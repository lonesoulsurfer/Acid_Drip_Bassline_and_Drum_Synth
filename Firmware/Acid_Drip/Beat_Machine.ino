/*
 * =====================================================================
 *  BEAT MACHINE 2  —  Acid Drip RP2040, standalone sketch
 *  Audio engine: DrumKid (Matt Bradshaw), ported by Marcus / lonesoulsurfer
 * =====================================================================
 *
 * SCREEN LAYOUT (320×240 landscape):
 *   y=0-11    Slot dots  (4 circles above pads 3-6)
 *   y=12-21   Step grid  (5 drum rows × 2px)
 *   y=24-43   Value strip
 *   y=46-83   Pad row 0  (beats 1-8, square pads)
 *   y=85-122  Pad row 1  (beats 9-16, square pads)
 *   y=125-140 FUNC label strip
 *   y=143-166 Pot bars
 *   y=170-239 Info strip
 *
 * DRUM EDIT (normal mode):
 *   Hold pad 1-5 (kick/hat/snare/rim/tom) while turning pots:
 *     CUT = pitch   RES = crop/length   DCY = volume
 *   Pot pickup prevents jumps on entry.
 *   Resets to defaults on pattern change.
 *   Saved/loaded with slots.
 *
 * FUNC MODE (pads 7+8):
 *   PTCH=global pitch  CRSH=bit crush  CROP=global crop
 *   DROP=mute combos   MPNT=midpoint   TMPO=tempo
 *   SWNG=swing         STEP=drum step division (fire every Nth acid step)
 *
 * SAVE/LOAD: hold pads 7+8, then tap/hold pads 3-6
 * RESET:     hold pads 1+2 for 1 second
 * =====================================================================
 */


// =====================================================================
// BEAT MACHINE 2 — Tab for Acid Drip V4
// Place this file in the same folder as Acid_Drip_V4.ino
// =====================================================================

bool bmMode = false;
bool bmAlarmStarted = false;
bool bmPlayChanged = false;  // signals V4 to sync acid play state
byte bmStepDiv  = 1;   // drum speed divider: 1=normal, 2=half time etc
byte bmStepOfst = 0;   // drum pattern offset: shift pattern N steps forward
byte bmDrumPos  = 0;   // persistent drum pattern position (0-15)
#define NUM_DRUMS 8
uint32_t bmLastFlashDraw = 0;
bool bmLastFlashState[NUM_DRUMS] = {};
PWMAudio bmPWM(2);  // drum audio on GP2, DMA-driven
bool bmTriggered[NUM_DRUMS] = {};


#include <Sample.h>
#include <mozzi_rand.h>
#include <PWMAudio.h>
#include "beats.h"
#include "sample0.h"
#include "sample1.h"
#include "sample2.h"
#include "sample3.h"
#include "sample4.h"
#include "sample5.h"
#include "sample6.h"
#include "sample7.h"

// ── Pins ─────────────────────────────────────────────────────────────
#define POT_CUT  26
#define POT_RES  28
#define POT_DECAY 27

#define BM_PAD_FUNC_A  6
#define BM_PAD_FUNC_B  7

// ── Display ───────────────────────────────────────────────────────────
#define SW  320
#define SH  240

#define BM_DOTS_Y   0
#define BM_DOTS_H  12
#define BM_GRID_Y  14
#define BM_GRID_H  16
#define BM_VS_Y    34    // value strip
#define BM_VS_H    20
#define BM_DS_Y    54    // drum strip — directly under value strip
#define BM_DS_H    16
#define BM_PAD_BW  38    // square pads
#define BM_PAD_BH  38
#define BM_PAD_SP   1
#define BM_PAD_SX   3
#define BM_PAD_R0  72    // pad row 0
#define BM_PAD_R1 111    // pad row 1
#define BM_LBL_Y  152    // func label strip
#define BM_LBL_H   16
#define BM_BAR_Y  170    // pot bars
#define BM_BAR_P    9
#define BM_BAR_BH   6
#define BM_IS_Y   200    // info strip


// ── Samples ───────────────────────────────────────────────────────────
Sample<sample0_NUM_CELLS, AUDIO_RATE> bms0(sample0_DATA);
Sample<sample1_NUM_CELLS, AUDIO_RATE> bms1(sample1_DATA);
Sample<sample2_NUM_CELLS, AUDIO_RATE> bms2(sample2_DATA);
Sample<sample3_NUM_CELLS, AUDIO_RATE> bms3(sample3_DATA);
Sample<sample4_NUM_CELLS, AUDIO_RATE> bms4(sample4_DATA);
Sample<sample5_NUM_CELLS, AUDIO_RATE> bms5(sample5_DATA);
Sample<sample6_NUM_CELLS, AUDIO_RATE> bms6(sample6_DATA);
Sample<sample7_NUM_CELLS, AUDIO_RATE> bms7(sample7_DATA);
// Looping disabled — samples play once per trigger, silence between hits
// Without this, samples loop continuously causing a constant hum

// ── Per-drum defaults ─────────────────────────────────────────────────
// pitch: 128 = natural speed (1.0x); 0=0.25x; 255=4.0x
// crop:  255 = full sample length
// vol:   200 = loud but not clipping
#define BM_DEF_DRUM_PITCH  128  // 128 = pot centre = natural speed
#define BM_DEF_DRUM_CROP   255
#define BM_DEF_DRUM_VOL    200

const float bmSampleRates[NUM_DRUMS] = {
  (float)sample0_SAMPLERATE, (float)sample1_SAMPLERATE,
  (float)sample2_SAMPLERATE, (float)sample3_SAMPLERATE,
  (float)sample4_SAMPLERATE
};
const int bmSampleCells[NUM_DRUMS] = {
  sample0_NUM_CELLS, sample1_NUM_CELLS, sample2_NUM_CELLS,
  sample3_NUM_CELLS, sample4_NUM_CELLS
};

// Per-drum params — reset on pattern change, saved in slots
byte bmDrumPitch[NUM_DRUMS];
byte bmDrumLen[NUM_DRUMS];
byte bmDrumVol[NUM_DRUMS];

// Global params
byte  bmStoredChance   = 0;
byte  bmStoredZoom     = 150;
byte  bmStoredRange    = 0;
byte  bmStoredMidpoint = 128;
byte  bmStoredPitch    = 160;   // global pitch multiplier (applied on top of per-drum)
byte  bmStoredCrush    = 255;
byte  bmStoredCrop     = 255;   // global crop (per-drum overrides)
byte  bmStoredDrop     = 128;
byte  bmStoredSwing    = 0;
byte  bmStoredBeat     = 2;
float bmStoredTempo    = 120.0f;

byte  bmParamChance, bmParamZoom, bmParamCrush, bmCrushComp, bmParamDrop, bmParamSwing, bmParamBeat;
int   bmParamRange, bmParamMidpoint;

// Default global func values
#define DEF_PITCH    128  // 128 = neutral (1.0x) — no pitch shift at default
#define DEF_CRUSH    255
#define DEF_CROP     255
#define DEF_DROP     128
#define DEF_SWING    0
#define DEF_MIDPOINT 128

const byte bmDropRef[NUM_DRUMS] = {
  0b00011110, 0b11111000, 0b00110000, 0b01111000, 0b00011100,
  0b00011110, 0b00110000, 0b11111000   // bass2, clap, openhat
};

// ── Clock ─────────────────────────────────────────────────────────────
bool     bmPlaying     = false;
byte     bmPulseNum    = 0;
uint16_t bmStepNum     = 0;

// ── Drum edit mode ────────────────────────────────────────────────────
// bmDrumEditPad = 0-7 while one of pads 1-8 is held
uint8_t  bmDrumEditPad    = 0;   // currently selected drum (0-7)
// pots always map directly to selected drum — no pickup needed

// ── UI ────────────────────────────────────────────────────────────────
bool     bmFuncMode  = false;
uint8_t  bmFuncSel   = 255;
bool     bmPotLocked = true;
uint8_t  bmPotPickup = 128;

#define BM_NUM_TAPS 4
float    bmTapTimes[BM_NUM_TAPS] = {};
byte     bmTapIdx = 0;

bool     bmPState[16]={}, bmPLast[16]={}, bmPChord[16]={}, bmPLong[16]={};
uint32_t bmPDeb[16]={}, bmPDown[16]={};

uint16_t bmRawCut=0, bmRawRes=0, bmRawDcy=0;

bool     bmFired[NUM_DRUMS]={};
uint32_t bmFiredMs[NUM_DRUMS]={};
#define  BM_FLASH_MS 80

bool bmFullDirty=true, bmGridDirty=false, bmDotsDirty=false, bmVsDirty=false,
     bmPadsDirty=false, bmBarsDirty=false, bmInfoDirty=false, bmDrumsDirty=false,
     bmDStripDirty=false;  // drum strip (name + mod indicators)

// ── Save / Load ───────────────────────────────────────────────────────
#define BM_NUM_SLOTS    4
#define BM_EEPROM_SIZE  512
#define BM_PATCH_VALID  0xBE    // bumped from 0xBD — new struct, force re-validate
#define BM_SAVE_HOLD_MS 1000
#define BM_SLOT_ADDR(s) ((s)*(int)sizeof(BmPatch))

struct BmPatch {
  uint8_t valid;
  byte    beat, crush, crop, drop, swing, pitch;
  byte    chance, zoom, range, midpoint;
  uint8_t tempoByte;
  byte    dPitch[NUM_DRUMS], dLen[NUM_DRUMS], dVol[NUM_DRUMS];   // per-drum
};

bool     bmSlotHasData[BM_NUM_SLOTS]={};
int8_t   bmLastLoadedSlot=-1;
uint8_t  bmSaveSlotPending=255;
uint32_t bmSaveSlotDownMs=0;
uint8_t  bmSlotProgress=0;
bool     bmSlotProgressShow=false;
bool     bmSlotOverlay=false;
uint8_t  bmSlotOverlaySlot=0;
bool     bmSlotOverlaySave=false, bmSlotOverlayEmpty=false;
uint32_t bmSlotOverlayMs=0;

// ── Names ─────────────────────────────────────────────────────────────
const char* bmBeatNames[16] = {
  "BASIC",      "HOUSE",      "TECHNO",     "ACID TRACK",
  "HALF TIME",  "FUNKY",      "SWING",      "BREAKBEAT",
  "MINIMAL",    "LATIN",      "HIP HOP",    "BLUE MONDAY",
  "DRUM N BASS","TRIPLET",    "BUILD DOWN", "GHOST",
};
const char* bmBeatShort[16] = {
  "BASC","HOUS","TECH","ACID",
  "HTIM","FUNK","SWNG","BKBT",
  "MINM","LATN","HPHP","BLMN",
  "DNBT","TRPL","GBLD","GHST",
};
const char*    bmDrumNames[NUM_DRUMS] = {"KICK","HIHAT","SNARE","RIM","TOM","BASS2","CLAP","OPENHAT"};
const uint16_t bmDrumCols[NUM_DRUMS] = {C_RED,C_GRN,C_YEL,C_CYN,0xFC60,C_ORG,C_MGR,C_BLU};
// FUNC slot 7 is now VOLS (drum volume presets) instead of ZOOM
const char*    bmFuncNames[8] = {"PTCH","CRSH","CROP","DROP","MPNT","TMPO","SWNG","STEP"};
const char*    bmSwNames[3]   = {"STRT","SWNG","TRPL"};

// ── Helpers ───────────────────────────────────────────────────────────
float bmByteToTempo(byte b){
  return (b<=192) ? 10.0f+b : 202.0f+12.66667f*(b-192.0f);
}
byte bmTempoToByte(float t){
  if(t<=202.0f) return (byte)constrain((int)(t-10.0f),0,192);
  return (byte)constrain((int)((t-202.0f)/12.66667f+192.0f),192,255);
}

// Per-drum pitch: two-segment linear, pot 128 = natural speed 1.0x
//   0   → 0.25x (two octaves below)
//   128 → 1.0x  (natural speed)
//   255 → 4.0x  (two octaves above)
// Global FUNC pitch (bmStoredPitch) multiplies on top using the same formula.
float bmPitchMult(byte pv){
  if(pv<=128) return 0.25f + (float)pv*(0.75f/128.0f);
  else        return 1.0f  + (float)(pv-128)*(3.0f/127.0f);
}

void bmApplyDrum(byte d){
  float freq = bmPitchMult(bmStoredPitch) * bmPitchMult(bmDrumPitch[d])
               * bmSampleRates[d] / (float)bmSampleCells[d];
  // bmDrumLen sets per-drum length, bmStoredCrop scales it globally
  // bmStoredCrop 255=full length, 0=very short
  int rawEnd = map(bmDrumLen[d], 0, 255, 50, bmSampleCells[d]);
  int endCell = map(bmStoredCrop, 0, 255, 50, rawEnd);
  endCell = constrain(endCell, 50, bmSampleCells[d]);
  switch(d){
    case 0: bms0.setFreq(freq); bms0.setEnd(endCell); break;
    case 1: bms1.setFreq(freq); bms1.setEnd(endCell); break;
    case 2: bms2.setFreq(freq); bms2.setEnd(endCell); break;
    case 3: bms3.setFreq(freq); bms3.setEnd(endCell); break;
    case 4: bms4.setFreq(freq); bms4.setEnd(endCell); break;
    case 5: bms5.setFreq(freq); bms5.setEnd(endCell); break;
    case 6: bms6.setFreq(freq); bms6.setEnd(endCell); break;
    case 7: bms7.setFreq(freq); bms7.setEnd(endCell); break;
  }
}

void bmApplyParams(){
  bmParamChance   = bmStoredChance;
  bmParamZoom     = bmStoredZoom;
  bmParamRange    = 2*(int)bmStoredRange;
  bmParamMidpoint = 2*(int)bmStoredMidpoint-255;
  bmParamSwing    = bmStoredSwing/86;
  bmParamBeat     = bmStoredBeat;
  bmParamDrop     = map(bmStoredDrop,0,256,0,9);

  bmParamCrush = 7-(bmStoredCrush>>5);
  byte crushComp_ = bmParamCrush;
  if(bmParamCrush>=6) crushComp_--;
  if(bmParamCrush>=7) crushComp_--;
  bmCrushComp = crushComp_;

  for(byte d=0;d<NUM_DRUMS;d++) bmApplyDrum(d);
}

void bmResetFuncParams(){
  bmStoredPitch=DEF_PITCH; bmStoredCrush=DEF_CRUSH; bmStoredCrop=DEF_CROP;
  bmStoredDrop=DEF_DROP; bmStoredSwing=DEF_SWING; bmStoredMidpoint=DEF_MIDPOINT;
}

void bmResetDrumParams(){
  for(byte d=0;d<NUM_DRUMS;d++){
    bmDrumPitch[d]=BM_DEF_DRUM_PITCH;
    bmDrumLen[d]  =BM_DEF_DRUM_CROP;
    bmDrumVol[d]  =BM_DEF_DRUM_VOL;
  }
}

// ── Save / Load ───────────────────────────────────────────────────────
void bmSavePatch(uint8_t slot){
  if(slot>=BM_NUM_SLOTS) return;
  BmPatch p;
  p.valid=BM_PATCH_VALID; p.beat=bmStoredBeat;
  p.crush=bmStoredCrush; p.crop=bmStoredCrop; p.drop=bmStoredDrop;
  p.swing=bmStoredSwing; p.pitch=bmStoredPitch;
  p.chance=bmStoredChance; p.zoom=bmStoredZoom;
  p.range=bmStoredRange; p.midpoint=bmStoredMidpoint;
  p.tempoByte=bmTempoToByte(bmStoredTempo);
  for(byte d=0;d<NUM_DRUMS;d++){p.dPitch[d]=bmDrumPitch[d];p.dLen[d]=bmDrumLen[d];p.dVol[d]=bmDrumVol[d];}
  EEPROM.put(BM_SLOT_ADDR(slot),p); EEPROM.commit();
  bmSlotHasData[slot]=true;
}

void bmLoadPatch(uint8_t slot){
  if(slot>=BM_NUM_SLOTS||!bmSlotHasData[slot]) return;
  BmPatch p; EEPROM.get(BM_SLOT_ADDR(slot),p);
  if(p.valid!=BM_PATCH_VALID) return;
  bmStoredBeat=p.beat; bmStoredCrush=p.crush; bmStoredCrop=p.crop;
  bmStoredDrop=p.drop; bmStoredSwing=p.swing; bmStoredPitch=p.pitch;
  bmStoredChance=p.chance; bmStoredZoom=p.zoom;
  bmStoredRange=p.range; bmStoredMidpoint=p.midpoint;
  bmStoredTempo=bmByteToTempo(p.tempoByte);
  for(byte d=0;d<NUM_DRUMS;d++){bmDrumPitch[d]=p.dPitch[d];bmDrumLen[d]=p.dLen[d];bmDrumVol[d]=p.dVol[d];}
  bmApplyParams();
  bmLastLoadedSlot=(int8_t)slot;
  bmFullDirty=true;
}

void bmCheckSlots(){
  for(uint8_t s=0;s<BM_NUM_SLOTS;s++){
    uint8_t v; EEPROM.get(BM_SLOT_ADDR(s),v);
    bmSlotHasData[s]=(v==BM_PATCH_VALID);
  }
}

// ── Beat engine ───────────────────────────────────────────────────────
byte bmGetZoom(){
  byte zIdx=bmParamZoom/43, zVel=bmParamZoom%43, ret=0;
  switch(zIdx){
    case 0: if(bmStepNum%96==0) ret=42; else if(bmStepNum%48==0) ret=zVel; break;
    case 1: if(bmStepNum%48==0) ret=42; else if(bmStepNum%24==0) ret=zVel; break;
    case 2: if(bmStepNum%24==0) ret=42; else if(bmStepNum%12==0) ret=zVel; break;
    case 3: if(bmStepNum%12==0) ret=42;
            else if(bmParamSwing==0&&bmStepNum%6==0)  ret=zVel;
            else if(bmParamSwing==1&&bmStepNum%12==7) ret=zVel;
            else if(bmParamSwing==2&&bmStepNum%12==8) ret=zVel; break;
    case 4: if(bmParamSwing==0){if(bmStepNum%6==0) ret=42; else if(bmStepNum%3==0) ret=zVel;}
            else if(bmParamSwing==1){if(bmStepNum%12==0||bmStepNum%12==7) ret=42; else if(bmStepNum%12==5) ret=zVel;}
            else{if(bmStepNum%12==0||bmStepNum%12==8) ret=42; else if(bmStepNum%12==4) ret=zVel;} break;
    case 5: if(bmParamSwing==0){if(bmStepNum%3==0) ret=42; else ret=zVel;}
            else if(bmParamSwing==1){if(bmStepNum%12==0||bmStepNum%12==5||bmStepNum%12==7) ret=42; else ret=zVel;}
            else{if(bmStepNum%4==0) ret=42; ret=zVel;} break;
  }
  return ret;
}

void bmCalcNote(byte s){
  if(!bitRead(bmDropRef[s],bmParamDrop)) return;
  long vel=0;
  uint16_t eff=bmStepNum;
  if(bmParamSwing==1){if(eff%12==6)eff--;if(eff%12==7)eff--;}
  if(bmParamSwing==2){if(eff%12==6)eff--;if(eff%12==8)eff-=2;}
  eff=eff%192;
  if(eff%6==0){
    byte bb=pgm_read_byte(&beats[bmParamBeat][s][eff/48]);
    if(bitRead(bb,7-((eff/6)%8))) vel=255;
  }
  if((byte)rand(256)<bmParamChance){
    int lo=bmParamMidpoint-bmParamRange/2, hi=bmParamMidpoint+bmParamRange/2;
    if(lo>=hi) hi=lo+1;
    long rv=constrain(rand(lo,hi),-255,255);
    byte zm=bmGetZoom(); if(zm<42) rv=rv*6*zm/255;
    vel+=rv;
  }
  vel=constrain(vel,0,255);
  if(vel>8){
    bmTriggered[s]=true;
    switch(s){
      case 0:bms0.start();break; case 1:bms1.start();break;
      case 2:bms2.start();break; case 3:bms3.start();break;
      case 4:bms4.start();break; case 5:bms5.start();break;
      case 6:bms6.start();break; case 7:bms7.start();break;
    }
    bmFired[s]=true; bmFiredMs[s]=millis();
    // bmDrumsDirty NOT set — flash redrawn on 20ms timer in loop1
    // Setting dirty here causes SPI write on every hit = crackling
  }
}

// ── bmTriggerStep: called from acid advanceStep() ────────────────────
// Triggers drums on acid step position (0-15) — perfectly locked, zero drift
void bmTriggerStep(uint8_t step){
  if(!bmPlaying) return;
  // Step division: skip unless this acid step is a trigger point
  if(bmStepDiv > 1 && (step % bmStepDiv) != 0) return;
  // Drum position: persistent counter advances on every trigger
  // This ensures all 16 drum steps always play, just spread over more acid cycles
  uint8_t drumStep = (bmDrumPos + bmStepOfst) % 16;
  bmDrumPos = (bmDrumPos + 1) % 16;
  for(byte s=0;s<NUM_DRUMS;s++){
    if(!bitRead(bmDropRef[s],bmParamDrop)) continue;
    byte bb=pgm_read_byte(&beats[bmParamBeat][s][drumStep/8]);
    if(!bitRead(bb,7-(drumStep%8))) continue;
    bmTriggered[s]=true;
    switch(s){
      case 0:bms0.start();break; case 1:bms1.start();break;
      case 2:bms2.start();break; case 3:bms3.start();break;
      case 4:bms4.start();break;
    }
    bmFired[s]=true; bmFiredMs[s]=millis();
    bmDrumsDirty=true;  // only set when a drum actually fires
  }
  bmStepNum=(uint16_t)drumStep*6;
  bmGridDirty=true;
}

void bmDoPulse(){
  for(byte i=0;i<NUM_DRUMS;i++) bmCalcNote(i);
  if(++bmPulseNum==24) bmPulseNum=0;
  if(++bmStepNum>=96)  bmStepNum=0;
}

// ── FUNC presets ──────────────────────────────────────────────────────
void bmApplyFuncPreset(uint8_t slot){
  switch(bmFuncSel){
    case 0:{const byte p[]={64,96,128,150,160,180,210,240};bmStoredPitch=p[slot];break;}
    case 1:{const byte c[]={255,220,190,160,128,96,64,0};  bmStoredCrush=c[slot];break;}
    case 2:{const byte cr[]={255,200,160,120,80,50,25,0};  bmStoredCrop=cr[slot];break;}
    case 3:{const byte d[]={128,0,28,48,56,64,96,112};     bmStoredDrop=d[slot]; break;}
    case 4:{const byte m[]={0,32,64,96,128,160,192,255};   bmStoredMidpoint=m[slot];break;}
    case 5:{const float t[]={80,90,100,110,120,128,138,150};bmStoredTempo=t[slot];break;}
    case 6:{const byte sw[]={0,86,172,0,86,172,0,86};      bmStoredSwing=sw[slot];break;}
    case 7: break;  // STEP handled on bottom-row pad press (stays in FUNC)
  }
  bmApplyParams();
  bmFuncMode=false; bmFuncSel=255;
  bmPotLocked=true;
  bmFullDirty=true;
}

// ── Full reset ────────────────────────────────────────────────────────
void bmDoReset(){
  bmPlaying=false; bmPulseNum=0; bmStepNum=0; bmDrumPos=0;
  for(byte _i=0;_i<NUM_DRUMS;_i++) bmTriggered[_i]=false;
  bmStoredChance=0; bmStoredZoom=150; bmStoredRange=0;
  bmStoredTempo=120.0f; bmStoredBeat=2;
  bmResetFuncParams(); bmResetDrumParams();
  bmFuncMode=false; bmFuncSel=255; bmPotLocked=true;
  bmDrumEditPad=0;
  for(byte i=0;i<NUM_DRUMS;i++) bmFired[i]=false;
  for(byte i=0;i<16;i++){bmPState[i]=false;bmPLast[i]=false;bmPChord[i]=false;bmPLong[i]=false;}
  bmApplyParams();
  bmDStripDirty=true; bmFullDirty=true;
}

// ── updateControl ─────────────────────────────────────────────────────
void bmUpdateControl(){
  uint32_t now=millis();

  // Check if any drum flash has expired — set dirty to redraw cleared state
  { uint32_t nowF=millis();
    for(byte _f=0;_f<NUM_DRUMS;_f++){
      if(bmFired[_f] && (nowF-bmFiredMs[_f])>=BM_FLASH_MS){
        bmFired[_f]=false;  // clear expired flash
        bmDrumsDirty=true;
      }
    }
  }
  // ── Drum clock — always runs regardless of mode ───────────────────
  // Fire mode switch when pads 9+10 held >= 1s in drum mode
  if(bmModeArmed && (now-bmModeArmMs)>=1000){
    bmMode=!bmMode;
    bmModeArmed=false;
    bmPChord[8]=false; bmPChord[9]=false;
    bmPState[8]=false; bmPState[9]=false;
    if(bmMode){
      bmStartAudio(); bmFullDirty=true;
      // Re-lock FUNC pot on drum mode entry — acid pot position must not
      // immediately overwrite FUNC params set before switching to acid
      bmPotLocked=true;
      bmPotPickup=(uint8_t)(analogRead(POT_CUT)>>2);
    } else{ bmClearPadState(); }
    rp2040.fifo.push_nb(bmMode?2u:3u);
  }
  if(bmModeArmed&&(digitalRead(PAD_PINS[8])!=LOW||digitalRead(PAD_PINS[9])!=LOW)){
    bmModeArmed=false;
  }
  // Drum triggers now come from acid advanceStep() via bmTriggerStep()
  // No independent clock needed — zero drift guaranteed

  // ── UI: pads and pots — only active in drum mode ─────────────────
  // When in acid mode, pads and pots belong exclusively to acid synth
  if(!bmMode) return;

  bmRawCut=analogRead(POT_CUT); bmRawRes=analogRead(POT_RES); bmRawDcy=analogRead(POT_DECAY);
  uint8_t bmPotCut=bmRawCut>>2, bmPotRes=bmRawRes>>2, bmPotDcy=bmRawDcy>>2;

  // ── Pots: always control the selected drum (bmDrumEditPad) ─────────────
  // Exception: in FUNC mode with a param selected, CUT adjusts that param.
  if(bmFuncMode&&bmFuncSel!=255){
    if(bmPotLocked){int d=(int)bmPotCut-(int)bmPotPickup;if(d<-6||d>6)bmPotLocked=false;}
    if(!bmPotLocked){
      bool ch=false;
      switch(bmFuncSel){
        case 0:if(bmPotCut!=bmStoredPitch)   {bmStoredPitch=bmPotCut;   bmApplyParams();ch=true;}break;
        case 1:if(bmPotCut!=bmStoredCrush)   {bmStoredCrush=bmPotCut;   bmApplyParams();ch=true;}break;
        case 2:if(bmPotCut!=bmStoredCrop)    {bmStoredCrop=bmPotCut;    bmApplyParams();ch=true;}break;
        case 3:if(bmPotCut!=bmStoredDrop)    {bmStoredDrop=bmPotCut;    bmApplyParams();ch=true;}break;
        case 4:if(bmPotCut!=bmStoredMidpoint){bmStoredMidpoint=bmPotCut;bmApplyParams();ch=true;}break;
        case 5:{float t=bmByteToTempo(bmPotCut);
                if(fabsf(t-bmStoredTempo)>0.5f){bmStoredTempo=t;bmApplyParams();bmVsDirty=true;ch=true;}break;}
        case 6:if(bmPotCut!=bmStoredSwing)   {bmStoredSwing=bmPotCut;   bmApplyParams();ch=true;}break;
        case 7: break;  // BEAT: no pot control, tiles only
      }
      if(ch){bmBarsDirty=true;bmVsDirty=true;bmInfoDirty=true;}
    }
  } else {
    // Normal mode: pots ONLY change drum sound while pad 1-8 is physically held
    // Release pad → pots lock immediately
    // This makes drum sounds persistent across pattern changes and mode switches
    byte d=bmDrumEditPad;
    bool padHeld=(d<NUM_DRUMS && bmPState[d]);  // is the selected drum's pad currently held?
    if(padHeld){
      bool ch=false;
      if(abs((int)bmPotCut-(int)bmDrumPitch[d])>2){bmDrumPitch[d]=bmPotCut;bmApplyDrum(d);ch=true;}
      if(abs((int)bmPotRes-(int)bmDrumLen[d])>2)  {bmDrumLen[d]=bmPotRes; bmApplyDrum(d);ch=true;}
      if(abs((int)bmPotDcy-(int)bmDrumVol[d])>2)  {bmDrumVol[d]=bmPotDcy; ch=true;}
      if(ch){bmBarsDirty=true;bmVsDirty=true;bmDStripDirty=true;}
    }
  }

  // ── Pad reads ─────────────────────────────────────────────────────────
  for(byte i=0;i<16;i++){
    bool r=(digitalRead(PAD_PINS[i])==LOW);
    if(r!=bmPLast[i]&&(now-bmPDeb[i])>20){
      bmPDeb[i]=now; bmPLast[i]=r;
      if(r){
        bmPDown[i]=now; bmPState[i]=true; bmPLong[i]=false;

        // Play/stop chord: mark on press, act on release
        if((i==0&&bmPState[1])||(i==1&&bmPState[0])){bmPChord[0]=bmPChord[1]=true;continue;}
        if(bmPChord[i]) continue;

        // FUNC toggle: pads 7+8
        if((i==BM_PAD_FUNC_A&&bmPState[BM_PAD_FUNC_B]&&(now-bmPDown[BM_PAD_FUNC_B])<200)||
           (i==BM_PAD_FUNC_B&&bmPState[BM_PAD_FUNC_A]&&(now-bmPDown[BM_PAD_FUNC_A])<200)){
          bmPChord[BM_PAD_FUNC_A]=bmPChord[BM_PAD_FUNC_B]=true;
          bmFuncMode=!bmFuncMode; bmFuncSel=255;
          bmPotLocked=true;
          bmFullDirty=true; continue;
        }
        // Pads 7+8 solo: skip if NOT in func mode and NOT in normal drum editing
        // Allow through if in FUNC with param selected (preset tiles)
        // Allow through if NOT in FUNC mode (drum edit pads 1-8)
        if((i==BM_PAD_FUNC_A||i==BM_PAD_FUNC_B) && bmFuncMode && bmFuncSel==255) continue;

        if(bmFuncMode){
          if(i>=8){
            uint8_t fn=i-8;
            if(fn==5&&bmFuncSel==5){  // tap tempo
              bmTapTimes[bmTapIdx]=(float)millis(); bmTapIdx=(bmTapIdx+1)%BM_NUM_TAPS;
              float total=0; byte valid=0;
              for(byte k=1;k<BM_NUM_TAPS;k++){
                byte ci=(bmTapIdx+BM_NUM_TAPS-k)%BM_NUM_TAPS,pi=(bmTapIdx+BM_NUM_TAPS-k-1)%BM_NUM_TAPS;
                if(bmTapTimes[pi]>0){float d=bmTapTimes[ci]-bmTapTimes[pi];
                  if(d>150&&d<3000){total+=d;valid++;}}
              }
              if(valid>=1){bmStoredTempo=constrain(60000.0f/(total/valid),40.0f,250.0f);
                           bmApplyParams();bmInfoDirty=true;}
              bmBarsDirty=true; continue;
            }
            if(fn==3){bmStoredDrop=(bmStoredDrop+28)%256;bmApplyParams();bmInfoDirty=true;}
            if(fn==6){bmStoredSwing=(bmStoredSwing+86)%258;bmApplyParams();bmInfoDirty=true;}
            bmFuncSel=fn; bmPotLocked=true; bmPotPickup=(uint8_t)(analogRead(POT_CUT)>>2);
            bmVsDirty=true; bmPadsDirty=true;
          } else {
            if(bmFuncSel==7){
              // Pads 1-4: set divider, pads 5-8: set offset
              const byte divs[4]={1,2,3,4};
              const byte ofsts[4]={2,4,6,8};
              if(i<4){
                bmStepDiv=divs[i]; bmStepOfst=0;
              } else {
                bmStepOfst=ofsts[i-4]; bmStepDiv=1;
              }
              bmVsDirty=true; bmBarsDirty=true;
            } else if(bmFuncSel!=255){
              bmApplyFuncPreset(i);
            }
          }
        } else {
          // Mode switch chord (pads 9+10) — arm on press, fire after 1s
          if((i==8 && bmPState[9]) || (i==9 && bmPState[8])){
            bmPChord[8]=true; bmPChord[9]=true;
            bmModeArmed=true; bmModeArmMs=millis();
            continue;
          }
          // Normal mode pads
          else if(i<8){
            // Pads 1-8: select drum, immediately show its values in the bar strip
            bmDrumEditPad=i;
            bmBarsDirty=true; bmVsDirty=true; bmDStripDirty=true; bmPadsDirty=true;
          } else if(i>=NUM_DRUMS && i<NUM_BEATS && i!=8 && i!=9){
            // Pads 9-16 (not drum edit pads 1-8): select beat pattern
            if(!bmFuncMode){
              bmStoredBeat=i; bmApplyParams();
              bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true;
            }
          }
          // Pads 8+9: always defer to release — may be start of mode switch gesture
        }
      } else {
        // Release
        if(bmPChord[i]&&(i==0||i==1)&&bmPChord[0]&&bmPChord[1]){
          uint32_t holdMs=now-max(bmPDown[0],bmPDown[1]);
          if(holdMs>=1000){ bmDoReset(); }
          else{ bmPlaying=!bmPlaying;
            if(bmPlaying){bmPulseNum=0;bmStepNum=0;bmDrumPos=0;}
            bmPlayChanged=true;
            bmVsDirty=true;bmInfoDirty=true;bmGridDirty=true;bmPadsDirty=true;}
        }
        // Pads 1-5 release: only select beat on short tap (< 200ms)
        // Long press = drum editing only — do not change the pattern
        if(i<NUM_DRUMS&&!bmPChord[i]){
          uint32_t holdMs=now-bmPDown[i];
          // Always redraw pad cell and bars on release so highlight clears
          bmBarsDirty=true; bmPadsDirty=true;
          // Only pads 0-4 select beat patterns on short tap
          // Pads 5-7 are drum edit only — never select patterns
          if(holdMs<200 && !bmFuncMode && i<5){
            bmStoredBeat=i; bmApplyParams();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true; bmDStripDirty=true;
          }
        }
        // Save/load release
        if((i==2||i==3||i==4||i==5)&&bmSaveSlotPending==(uint8_t)(i-2)){
          bmSlotProgressShow=false;
          if(!bmPLong[i]){
            uint8_t slot=i-2;
            if(bmSlotHasData[slot]){bmLoadPatch(slot);bmSlotOverlaySave=false;bmSlotOverlayEmpty=false;}
            else{bmSlotOverlaySave=false;bmSlotOverlayEmpty=true;}
            bmSlotOverlay=true;bmSlotOverlaySlot=i-2;bmSlotOverlayMs=now;
            bmInfoDirty=true;bmDotsDirty=true;
          }
          bmSaveSlotPending=255;
        }
        // Pads 8+9 released without chord — short tap = select pattern
        if((i==8||i==9) && !bmPChord[i]){
          uint32_t holdMs=now-bmPDown[i];
          if(holdMs<500 && i<NUM_BEATS && !bmFuncMode){
            bmStoredBeat=i; bmApplyParams();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true;
          }
        }
        bmPChord[i]=false; bmPState[i]=false;
      }
    }
  }

  // Long-press polls for save
  for(byte i=2;i<=5;i++){
    if(bmPState[i]&&bmPState[BM_PAD_FUNC_A]&&bmPState[BM_PAD_FUNC_B]){
      uint8_t slot=i-2; bmPChord[i]=true;
      if(!bmPLong[i]){
        if(bmSaveSlotPending!=slot){
          bmSaveSlotPending=slot;bmSaveSlotDownMs=bmPDown[i];
          bmSlotOverlaySlot=slot;bmSlotProgressShow=true;bmSlotOverlay=false;bmSlotProgress=254;
        }
        uint32_t held=now-bmSaveSlotDownMs;
        bmSlotProgress=(uint8_t)min((long)100,(long)held*100/BM_SAVE_HOLD_MS);
        if(held>=(uint32_t)BM_SAVE_HOLD_MS){
          bmPLong[i]=true;bmSaveSlotPending=255;bmSlotProgressShow=false;
          bmSavePatch(slot);
          bmSlotOverlay=true;bmSlotOverlaySave=true;bmSlotOverlayEmpty=false;
          bmSlotOverlaySlot=slot;bmSlotOverlayMs=now;
          bmInfoDirty=true;bmDotsDirty=true;
        }
      }
    }
  }
}

// ── updateAudio ───────────────────────────────────────────────────────
// bmDrumVol[d] is a continuous mix scalar applied every sample.
// 0=silent, 200=default, 255=full. No velocity gating.
// atten=7 with 5 drums at full vol: max sum = 5*(255*127>>7) = ~635 → constrain.
// ── Drum audio — DMA-driven PWMAudio on GP2 ──────────────────────────
// Callback fires from DMA interrupt when buffer needs filling.
// Same infrastructure as Mozzi's GP15 — zero timer jitter.
// Called at AUDIO_RATE (16384Hz) by PWMAudio DMA engine.
// bmDrumCallback removed — buffer filled from loop1() on core 1 (bmFillDrumBuffer)

// =====================================================================
// DISPLAY
// =====================================================================

void bmDrawSlotDots(){
  tft.fillRect(0,BM_DOTS_Y,SW,BM_DOTS_H,C_BG);
  tft.setTextSize(1); tft.setTextColor(C_DGR);
  tft.setCursor(4,BM_DOTS_Y+2); tft.print("SLOTS:");
  const int DOT_R=4, DOT_CY=BM_DOTS_Y+BM_DOTS_H/2;
  for(uint8_t s=0;s<4;s++){
    int col=s+2;
    int cx=BM_PAD_SX+col*(BM_PAD_BW+BM_PAD_SP)+BM_PAD_BW/2;
    if(!bmSlotHasData[s]){
      tft.drawCircle(cx,DOT_CY,DOT_R,C_DGR);
    } else if((int8_t)s==bmLastLoadedSlot){
      tft.fillCircle(cx,DOT_CY,DOT_R,C_CYN);
      tft.drawCircle(cx,DOT_CY,DOT_R,C_WHT);
    } else {
      tft.fillCircle(cx,DOT_CY,DOT_R,C_YEL);
      tft.drawCircle(cx,DOT_CY,DOT_R,C_ORG);
    }
    tft.setTextColor(C_DGR); tft.setTextSize(1);
    tft.setCursor(cx-2,BM_DOTS_Y+BM_DOTS_H-8); tft.print(s+1);
  }
}

void bmDrawGrid(){
  byte cur16=bmPlaying?(bmStepNum/6)%16:255;
  for(byte t=0;t<NUM_DRUMS;t++){
    for(byte s=0;s<16;s++){
      byte bb=pgm_read_byte(&beats[bmParamBeat][t][s/8]);
      bool on=bitRead(bb,7-(s%8));
      bool fl=bmFired[t]&&(millis()-bmFiredMs[t]<BM_FLASH_MS);
      bool hd=(s==cur16);
      uint16_t col=hd?C_WHT:(fl&&on?C_WHT:(on?bmDrumCols[t]:0x0861));
      tft.fillRect(4+s*19,BM_GRID_Y+t*2,18,2,col);
    }
  }
}

void bmDrawValStrip(){
  bool inFunc=bmFuncMode&&bmFuncSel!=255;
  tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,inFunc?0x1000:C_BG);
  tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,inFunc?C_ORG:C_DGR);
  tft.setTextSize(1);
  if(!inFunc){
    if(bmFuncMode){
      tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,0x1000); tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_ORG);
      tft.setTextColor(C_ORG); tft.setCursor(4,BM_VS_Y+6);
      tft.print("FUNC — press pad 9-16"); return;
    }

    // Normal: drum name | PCH/LEN/VOL values | pattern name
    byte d=bmDrumEditPad;
    uint16_t dc=(bmPState[d])?bmDrumCols[d]:C_DGR;
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,0x0008);
    tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,dc);
    // All info on one centred line (BM_VS_H=20, text=8px, centre at +6)
    const int cy = BM_VS_Y+6;
    tft.setTextSize(1);
    // Drum name — left
    tft.setTextColor(dc);
    tft.setCursor(4,cy); tft.print(bmDrumNames[d]);
    // P / L / V values
    tft.setTextColor(C_YEL); tft.setCursor(76,cy);
    tft.print("P:"); tft.print(bmDrumPitch[d]);
    tft.setTextColor(C_GRN); tft.setCursor(116,cy);
    tft.print("L:"); tft.print(bmDrumLen[d]);
    tft.setTextColor(C_CYN); tft.setCursor(156,cy);
    tft.print("V:"); tft.print(bmDrumVol[d]);
    // Step div/offset
    if(bmStepDiv>1||bmStepOfst>0){
      tft.setTextColor(C_MGR); tft.setCursor(196,cy);
      if(bmStepOfst>0){tft.print("+");tft.print(bmStepOfst);tft.print("st");}
      else{tft.print("\xf7");tft.print(bmStepDiv);}
    }
    // BPM — right
    tft.setTextColor(C_WHT);
    tft.setCursor(220,cy); tft.print((int)bmStoredTempo); tft.print("BPM");
    // [PLAY]/[STOP] — far right
    tft.setTextColor(bmPlaying?C_GRN:C_YEL);
    tft.setCursor(262,cy); tft.print(bmPlaying?"[PLAY]":"[STOP]");
    return;
  }
  tft.setTextColor(C_ORG); tft.setCursor(4,BM_VS_Y+6); tft.print(bmFuncNames[bmFuncSel]); tft.print(":");
  tft.setTextColor(C_WHT); tft.setCursor(36,BM_VS_Y+6);
  switch(bmFuncSel){
    case 0:tft.print(bmStoredPitch);break; case 1:tft.print(bmStoredCrush);break;
    case 2:tft.print(bmStoredCrop);break;  case 3:tft.print(bmParamDrop);break;
    case 4:tft.print(bmParamMidpoint);break;
    case 5:tft.print((int)bmStoredTempo);tft.print("BPM");break;
    case 6:tft.print(bmSwNames[bmParamSwing]);break;
    case 7:{
        if(bmStepOfst>0){tft.print("OFST:+");tft.print(bmStepOfst);}
        else{tft.print(bmStepDiv==1?"normal":"div x");if(bmStepDiv>1)tft.print(bmStepDiv);}
      }break;
  }
  if(bmFuncSel!=7){
    tft.setTextColor(bmPotLocked?C_RED:C_GRN);
    tft.setCursor(120,BM_VS_Y+6); tft.print(bmPotLocked?"[turn CUT]":"[CUT live]");
  }
}

void bmDrawSaveProgress(){
  static uint8_t prevPct=255;
  if(bmSlotProgress==prevPct) return; prevPct=bmSlotProgress;
  const int FX=1,BM_FW=SW-2,FH=BM_VS_H-2;
  tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_ORG);
  int filled=BM_FW*bmSlotProgress/100;
  if(filled>0)  tft.fillRect(FX,       BM_VS_Y+1,filled,  FH,0x6200);
  if(filled<BM_FW) tft.fillRect(FX+filled,BM_VS_Y+1,BM_FW-filled,FH,C_BG);
  tft.setTextSize(1); tft.setTextColor(C_ORG);
  tft.setCursor(4,BM_VS_Y+6); tft.print("SAVING SLT"); tft.print(bmSlotOverlaySlot+1);
}

void bmDrawSlotOverlay(){
  if(!bmSlotOverlay) return;
  if((millis()-bmSlotOverlayMs)>1400){bmSlotOverlay=false;bmVsDirty=true;bmDotsDirty=true;return;}
  tft.setTextSize(1);
  if(bmSlotOverlayEmpty){
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,C_BG); tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_DGR);
    tft.setTextColor(C_DGR); tft.setCursor(4,BM_VS_Y+6);
    tft.print("SLOT "); tft.print(bmSlotOverlaySlot+1); tft.print("  —  EMPTY");
  } else if(bmSlotOverlaySave){
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,0x0300); tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_GRN);
    tft.setTextSize(2); tft.setTextColor(C_GRN);
    tft.setCursor((SW-60)/2,BM_VS_Y+3); tft.print("SAVED");
    tft.setTextSize(1); tft.setTextColor(C_GRN);
    tft.setCursor(4,BM_VS_Y+6); tft.print("SLT"); tft.print(bmSlotOverlaySlot+1);
  } else {
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,0x0008); tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_CYN);
    tft.setTextSize(2); tft.setTextColor(C_CYN);
    tft.setCursor((SW-72)/2,BM_VS_Y+3); tft.print("LOADED");
    tft.setTextSize(1); tft.setTextColor(C_CYN);
    tft.setCursor(4,BM_VS_Y+6); tft.print("SLT"); tft.print(bmSlotOverlaySlot+1);
  }
}

void bmDrawPadCell(byte i){
  int row=i/8, col=i%8;
  int bx=BM_PAD_SX+col*(BM_PAD_BW+BM_PAD_SP);
  int by=(row==0)?BM_PAD_R0:BM_PAD_R1;
  bool isSel=(!bmFuncMode&&i==bmParamBeat), isPlay=isSel&&bmPlaying;
  bool isDrumEdit=(i<NUM_DRUMS && bmPState[i]);  // only while pad physically held
  tft.fillRect(bx-1,by-1,BM_PAD_BW+3,BM_PAD_BH+3,C_BG);
  uint16_t fill=isPlay?0x0421:(isSel?0x0841:(isDrumEdit?0x0008:C_BG));
  tft.fillRoundRect(bx,by,BM_PAD_BW,BM_PAD_BH,3,fill);
  uint16_t bord=isPlay?C_WHT:(isSel?C_CYN:(isDrumEdit?bmDrumCols[i]:C_DGR));
  tft.drawRoundRect(bx,by,BM_PAD_BW,BM_PAD_BH,3,bord);
  if(isSel||isDrumEdit) tft.drawRoundRect(bx-1,by-1,BM_PAD_BW+2,BM_PAD_BH+2,4,bord);
  tft.setTextSize(1);
  // Always show beat short name — drum identity is shown in the drum strip above
  const char* nm=bmBeatShort[i];
  tft.setTextColor(isSel?C_GRN:(isDrumEdit?bmDrumCols[i]:0x2945));
  int fw=strlen(nm)*6;
  tft.setCursor(bx+(BM_PAD_BW-fw)/2,by+8); tft.print(nm);
  char nb[4]; itoa(i+1,nb,10);
  tft.setTextColor(isSel?C_CYN:(isDrumEdit?bmDrumCols[i]:0x2104));
  fw=strlen(nb)*6;
  tft.setCursor(bx+(BM_PAD_BW-fw)/2,by+24); tft.print(nb);
}

void bmDrawFuncLabels(){
  const int TW=SW/8;
  tft.fillRect(0,BM_LBL_Y,SW,BM_LBL_H,C_BG);
  tft.setTextSize(1);
  for(byte t=0;t<8;t++){
    int tx=t*TW;
    bool isSel=bmFuncMode&&(bmFuncSel==t);
    bool funcActv=bmFuncMode&&!isSel;
    uint16_t lblBg =isSel?C_BG :(funcActv?C_RED :0x000F);
    uint16_t lblBdr=isSel?C_RED:(funcActv?C_RED :C_BLU);
    uint16_t lblTxt=isSel?C_RED:(funcActv?C_WHT :C_CYN);
    tft.fillRect(tx,BM_LBL_Y,TW,BM_LBL_H,lblBg);
    tft.drawRect(tx,BM_LBL_Y,TW,BM_LBL_H,lblBdr);
    const char* fn=bmFuncNames[t]; int fw=strlen(fn)*6;
    tft.setTextColor(lblTxt);
    tft.setCursor(tx+(TW-fw)/2,BM_LBL_Y+4); tft.print(fn);
  }
}

void bmDrawBarFill(int fy,int val,int maxW,uint16_t col){
  if(val>0)    tft.fillRect(37,    fy,val,    BM_BAR_BH-2,col);
  if(val<maxW) tft.fillRect(37+val,fy,maxW-val,BM_BAR_BH-2,C_BG);
}
// bmDrawBarLabels unused in normal mode (labels drawn per-drum in bmDrawBars)
void bmDrawBarLabels(){}
void bmDrawBarBorders(){
  tft.drawRect(36,BM_BAR_Y,        176,BM_BAR_BH,C_DGR);
  tft.drawRect(36,BM_BAR_Y+BM_BAR_P,  176,BM_BAR_BH,C_DGR);
  tft.drawRect(36,BM_BAR_Y+BM_BAR_P*2,176,BM_BAR_BH,C_DGR);
}

void bmDrawBars(){
  const int BM_FW=174;
  tft.setTextSize(1);

  if(bmFuncMode&&bmFuncSel!=255){
    tft.fillRect(0,BM_BAR_Y,36,BM_BAR_BH*3+BM_BAR_P*2,C_BG);
    tft.setTextColor(C_ORG); tft.setCursor(4,BM_BAR_Y);      tft.print(bmFuncNames[bmFuncSel]);
    tft.setTextColor(C_WHT); tft.setCursor(4,BM_BAR_Y+BM_BAR_P);
    switch(bmFuncSel){
      case 0:tft.print(bmStoredPitch);break; case 1:tft.print(bmStoredCrush);break;
      case 2:tft.print(bmStoredCrop);break;  case 3:tft.print(bmParamDrop);break;
      case 4:tft.print(bmParamMidpoint);break;
      case 5:tft.print((int)bmStoredTempo);break;
      case 6:tft.print(bmSwNames[bmParamSwing]);break;
      case 7:{
        if(bmStepOfst>0){
          tft.print("OFST: +"); tft.print(bmStepOfst);
        } else {
          tft.print(bmStepDiv==1?"STEP: normal":"STEP: ÷");
          if(bmStepDiv>1) tft.print(bmStepDiv);
        }
        break;
      }
    }
    if(bmFuncSel==7){
      // Show 8 step options as mini tiles
      // First 4 pads = dividers, last 4 = offsets
      const byte divs[4]={1,2,3,4};
      const byte ofsts[4]={2,4,6,8};
      tft.fillRect(36,BM_BAR_Y,SW-36,BM_BAR_BH*3+BM_BAR_P*2,C_BG);
      int tw=(SW-40)/8;
      for(byte t=0;t<8;t++){
        bool sel;
        if(t<4) sel=(divs[t]==bmStepDiv && bmStepOfst==0);
        else    sel=(ofsts[t-4]==bmStepOfst && bmStepDiv==1);
        int tx=38+t*tw;
        uint16_t bg=sel?C_ORG:(t<4?0x000F:0x0800);
        uint16_t bd=sel?C_YEL:(t<4?C_BLU:C_GRN);
        tft.fillRect(tx,BM_BAR_Y,tw-2,BM_BAR_BH*3+BM_BAR_P*2-1,bg);
        tft.drawRect(tx,BM_BAR_Y,tw-2,BM_BAR_BH*3+BM_BAR_P*2-1,bd);
        tft.setTextColor(sel?C_BG:(t<4?C_CYN:C_GRN));
        tft.setCursor(tx+2,BM_BAR_Y+BM_BAR_P-4);
        if(t<4){ tft.print("÷"); tft.print(divs[t]); }
        else   { tft.print("+"); tft.print(ofsts[t-4]); }
      }
    } else {
    int cw=constrain((int)(bmRawCut*BM_FW)/1023,0,BM_FW);
    bmDrawBarFill(BM_BAR_Y+1,cw,BM_FW,bmFuncSel==7?C_DGR:(bmPotLocked?C_DGR:C_YEL));
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P+1,0,BM_FW,C_BG);
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P*2+1,0,BM_FW,C_BG);
    }
    if(bmFuncSel!=7){tft.setTextColor(bmPotLocked?C_RED:C_GRN);tft.setCursor(216,BM_BAR_Y);tft.print(bmPotLocked?"LCK":"OK");}
    return;
  }

  // Normal: PCH/LEN/VOL for the selected drum
  // Static vars — labels/borders drawn once on drum change, fills only when value changes
  static byte prevDrum=255, prevPch=255, prevLen=255, prevVol=255;
  static bool bordersDrawn=false;
  byte d=bmDrumEditPad;
  if(d!=prevDrum || !bordersDrawn){
    prevDrum=d; prevPch=255; prevLen=255; prevVol=255; bordersDrawn=true;
    // Clear label column and redraw — no full-bar clear, preserves borders
    tft.fillRect(0,BM_BAR_Y,34,BM_BAR_BH*3+BM_BAR_P*2+2,C_BG);
    tft.setTextSize(1);
    tft.setTextColor(bmDrumCols[d]);
    tft.setCursor(2,BM_BAR_Y+1);       tft.print("PCH");
    tft.setCursor(2,BM_BAR_Y+BM_BAR_P+1); tft.print("LEN");
    tft.setCursor(2,BM_BAR_Y+BM_BAR_P*2+1);tft.print("VOL");
    bmDrawBarBorders();
    // Clear numeric value column
    tft.fillRect(214,BM_BAR_Y,SW-214,BM_BAR_BH*3+BM_BAR_P*2+2,C_BG);
  }
  // Only repaint fills and values that changed — no labels touched
  int pw=constrain((int)(bmDrumPitch[d]*BM_FW)/255,0,BM_FW);
  int lw=constrain((int)(bmDrumLen[d]*BM_FW)/255,0,BM_FW);
  int vw=constrain((int)(bmDrumVol[d]*BM_FW)/255,0,BM_FW);
  if(bmDrumPitch[d]!=prevPch){
    prevPch=bmDrumPitch[d];
    bmDrawBarFill(BM_BAR_Y+1,pw,BM_FW,C_YEL);
    tft.fillRect(214,BM_BAR_Y,SW-214,BM_BAR_BH,C_BG);
    tft.setTextColor(C_YEL); tft.setCursor(216,BM_BAR_Y+1); tft.print(bmDrumPitch[d]);
  }
  if(bmDrumLen[d]!=prevLen){
    prevLen=bmDrumLen[d];
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P+1,lw,BM_FW,C_GRN);
    tft.fillRect(214,BM_BAR_Y+BM_BAR_P,SW-214,BM_BAR_BH,C_BG);
    tft.setTextColor(C_GRN); tft.setCursor(216,BM_BAR_Y+BM_BAR_P+1); tft.print(bmDrumLen[d]);
  }
  if(bmDrumVol[d]!=prevVol){
    prevVol=bmDrumVol[d];
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P*2+1,vw,BM_FW,C_CYN);
    tft.fillRect(214,BM_BAR_Y+BM_BAR_P*2,SW-214,BM_BAR_BH,C_BG);
    tft.setTextColor(C_CYN); tft.setCursor(216,BM_BAR_Y+BM_BAR_P*2+1); tft.print(bmDrumVol[d]);
  }
}

// ── Drum strip: y=BM_DS_Y — directly under value strip ──────────────────
// Matches Acid303 func label colour scheme:
//   Normal (not selected): dark blue bg, blue border, cyan text
//   Selected drum:         red bg, red border, white text
//   (mirrors how func labels go all-red when FUNC is active)
void bmDrawDrumStrip(){
  tft.fillRect(0,BM_DS_Y,SW,BM_DS_H,C_BG);
  const int TW=BM_PAD_BW;   // same width as pad cells
  tft.setTextSize(1);
  for(byte d=0;d<NUM_DRUMS;d++){
    int bx=BM_PAD_SX+d*(BM_PAD_BW+BM_PAD_SP);   // same x pitch as pad columns
    bool isSel=(!bmFuncMode&&bmDrumEditPad==d);
    uint16_t lblBg =isSel?C_RED :0x000F;
    uint16_t lblBdr=isSel?C_RED :C_BLU;
    uint16_t lblTxt=isSel?C_WHT :C_CYN;
    tft.fillRect(bx,BM_DS_Y,TW,BM_DS_H,lblBg);
    tft.drawRect(bx,BM_DS_Y,TW,BM_DS_H,lblBdr);
    // Drum name centred
    const char* dn=bmDrumNames[d];
    int fw=strlen(dn)*6;
    tft.setTextColor(lblTxt);
    tft.setCursor(bx+(TW-fw)/2, BM_DS_Y+4); tft.print(dn);
    // Mod dots: yellow=pitch  green=crop  cyan=vol  (dim if default)
    bool pitchMod=(bmDrumPitch[d]!=BM_DEF_DRUM_PITCH);
    bool cropMod =(bmDrumLen[d] !=BM_DEF_DRUM_CROP);
    bool volMod  =(bmDrumVol[d]  !=BM_DEF_DRUM_VOL);
    int dotY=BM_DS_Y+BM_DS_H-4, dotX=bx+TW/2-8;
    tft.fillRect(dotX,   dotY,3,3,pitchMod?(isSel?C_YEL:C_YEL):0x0821);
    tft.fillRect(dotX+5, dotY,3,3,cropMod ?(isSel?C_GRN:C_GRN):0x0821);
    tft.fillRect(dotX+10,dotY,3,3,volMod  ?(isSel?C_CYN:C_CYN):0x0821);
  }
}

void bmDrawDrumFlash(){
  // Overdraw all names — no fillRect, no flash
  uint32_t now = millis();
  const int ty=BM_IS_Y+6;
  tft.setTextSize(1);
  for(byte t=0;t<NUM_DRUMS;t++){
    bool lit = bmFired[t] && (now - bmFiredMs[t] < BM_FLASH_MS);
    bmLastFlashState[t] = lit;
    tft.setTextColor(lit ? bmDrumCols[t] : C_DGR, C_BG);
    tft.setCursor(4+t*38, ty); tft.print(bmDrumNames[t]);
  }
}

void bmDrawDrumFlashFull(){
  // Full redraw on screen switch — fillRect once then overdraw text
  uint32_t now = millis();
  const int ty=BM_IS_Y+6;
  tft.fillRect(0,BM_IS_Y,SW,14,C_BG);
  tft.setTextSize(1);
  for(byte t=0;t<NUM_DRUMS;t++){
    bool lit = bmFired[t] && (now - bmFiredMs[t] < BM_FLASH_MS);
    bmLastFlashState[t] = lit;
    tft.setTextColor(lit ? bmDrumCols[t] : C_DGR, C_BG);
    tft.setCursor(4+t*38, ty); tft.print(bmDrumNames[t]);
  }
}

void bmDrawInfoBody(){
  // Single row: beat name (size 1) + flags on same line, freeing vertical space
  tft.fillRect(0,BM_IS_Y+14,SW,SH-BM_IS_Y-14,C_BG);
  const int ty=BM_IS_Y+20;
  // Beat name
  tft.setTextSize(2);
  tft.setTextColor(bmPlaying?C_GRN:C_CYN);
  tft.setCursor(4,ty); tft.print(bmBeatNames[bmParamBeat]);
  // Flags right-aligned on same row
  tft.setTextSize(1);
  int fx=180;  // flags start x
  if(bmLastLoadedSlot>=0){tft.setTextColor(C_CYN);tft.setCursor(fx,ty+4);tft.print("S");tft.print(bmLastLoadedSlot+1);}
}

void bmDrawInfoStrip(){
  tft.drawFastHLine(0,BM_IS_Y,SW,C_DGR);
  bmDrawDrumFlash(); bmDrawInfoBody();
}

void bmDrawMain(){
  tft.fillScreen(C_BG);
  bmDrawSlotDots(); bmDrawGrid(); bmDrawValStrip();
  for(byte i=0;i<16;i++) bmDrawPadCell(i);
  bmDrawFuncLabels();
  bmDrawBarLabels(); bmDrawBarBorders(); bmDrawBars();
  bmDrawDrumStrip();
  bmDrawInfoStrip();
}

void bmDoDraw(){
  if(bmFullDirty)       {bmFullDirty=false;  bmDrawMain();     return;}
  if(bmSlotProgressShow){bmDrawSaveProgress();               return;}
  if(bmSlotOverlay)     {bmDrawSlotOverlay();                return;}
  if(bmBarsDirty)       {bmBarsDirty=false;  bmDrawBars();      return;}  // first — pot feedback must feel instant
  if(bmVsDirty)         {bmVsDirty=false;    bmDrawValStrip();  return;}
  if(bmGridDirty)       {bmGridDirty=false;  bmDrawGrid();      return;}
  if(bmPadsDirty)       {bmPadsDirty=false;  for(byte i=0;i<16;i++) bmDrawPadCell(i); bmDrawFuncLabels(); return;}
  if(bmDotsDirty)       {bmDotsDirty=false;  bmDrawSlotDots();  return;}
  if(bmDStripDirty)     {bmDStripDirty=false;bmDrawDrumStrip();    return;}
  if(bmInfoDirty)       {bmInfoDirty=false;  bmDrawInfoBody(); return;}
  if(bmDrumsDirty)      {bmDrumsDirty=false; bmDrawDrumFlash();    return;}
}

// Called from Acid_Drip_V4 when drum mode is first activated
// ── bmFillDrumBuffer: called from loop1() on core 1 ─────────────────
// Fills GP2 PWMAudio buffer with drum samples.
// Running on core 1 means zero conflict with Mozzi audio ISR on core 0.
void bmFillDrumBuffer(){
  if(!bmAlarmStarted) return;
  while(bmPWM.availableForWrite()){
    int asig = 0;
    if(bmPlaying){
      const byte atten=7;
      if(bmTriggered[0]) asig+=(((int)bmDrumVol[0]*(int8_t)bms0.next())>>atten);
      if(bmTriggered[1]) asig+=(((int)bmDrumVol[1]*(int8_t)bms1.next())>>atten);
      if(bmTriggered[2]) asig+=(((int)bmDrumVol[2]*(int8_t)bms2.next())>>atten);
      if(bmTriggered[3]) asig+=(((int)bmDrumVol[3]*(int8_t)bms3.next())>>atten);
      if(bmTriggered[4]) asig+=(((int)bmDrumVol[4]*(int8_t)bms4.next())>>atten);
      if(bmTriggered[5]) asig+=(((int)bmDrumVol[5]*(int8_t)bms5.next())>>atten);
      if(bmTriggered[6]) asig+=(((int)bmDrumVol[6]*(int8_t)bms6.next())>>atten);
      if(bmTriggered[7]) asig+=(((int)bmDrumVol[7]*(int8_t)bms7.next())>>atten);
      if(bmParamCrush>0) asig=(asig>>bmParamCrush)<<bmCrushComp;
      asig=constrain(asig,-128,127);
    }
    // Full 16-bit scale — compensates for higher GP2 resistor (22K)
    // Constrain avoids int overflow: asig is already -128..127
    bmPWM.write((int16_t)(asig << 8), false);
  }
}

void bmStartAudio(){
  if(!bmAlarmStarted){
    bmAlarmStarted=true;
    // Large buffers: 8 x 256 = 2048 samples = 125ms headroom
    // Prevents underrun clicks even during long SPI display writes
    // High PWM carrier (488kHz vs GP15's 48kHz) eliminates carrier
    // beating that was causing acid to sound quiet when drums play
    bmPWM.setBuffers(8, 1024);  // 8192 samples = 500ms — survives full acid screen redraw
    bmPWM.setPWMFrequency(488281);  // 125MHz/256 — far from GP15's 48kHz
    bmPWM.setFrequency(AUDIO_RATE);
    bmPWM.begin(AUDIO_RATE);
  }
}


// ── bmResyncClock: call when returning to acid mode ──────────────────
// Resets bmNextPulseMs to now so drum clock doesn't race to catch up
// after a tempo change during drum mode.
// bmResyncClock removed — drums driven by acid steps directly

// ── bmClearPadState: called when leaving drum mode ───────────────────
// Ensures no stale pad presses carry over to acid mode
void bmClearPadState(){
  for(byte i=0;i<16;i++){
    bmPState[i]=false; bmPLast[i]=false;
    bmPChord[i]=false; bmPLong[i]=false;
  }
}
// ── bmInit: called once from Acid_Drip_V4 setup() ───────────────────
void bmInit(){
  // Disable looping — drums play once per hit, return 0 when finished
  bms0.setLoopingOff(); bms1.setLoopingOff(); bms2.setLoopingOff();
  bms3.setLoopingOff(); bms4.setLoopingOff();
  bms5.setLoopingOff(); bms6.setLoopingOff(); bms7.setLoopingOff();
  bmDrumEditPad=0;
  for(byte i=0;i<NUM_DRUMS;i++) bmFired[i]=false;
  bmResetDrumParams();
  bmResetFuncParams();
  bmStoredBeat=0;
  bmApplyParams();
  bmCheckSlots();
  bmFullDirty=true;
  // Alarm started on first entry to drum mode, not at boot
  // This ensures no drum output until user explicitly enters drum machine
}
