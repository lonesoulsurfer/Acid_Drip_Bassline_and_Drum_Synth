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
 *   PTCH=global pitch  DRIV=drive/saturation  FILT=bipolar DJ filter
 *   CHNC=per-hit drop chance  HMNZ=velocity humanize  SWNG=16th swing
 *   ACNT=accent depth  FILL=procedural fill engine
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

// ── Acid synth — forward declarations ────────────────────────────────
// ── Acid synth — forward declarations ────────────────────────────────
// BM drum saves/loads are fully independent (BM EEPROM offset 256+).
// Only these cross-module variables are needed from drum mode:
extern volatile bool saveCommit;
// Note: acidPlaying is no longer referenced from this file. Drum and acid
// play state are fully independent — starting/stopping one never touches
// the other. See the PLAY/STOP chord handler below and its mirror in
// Acid_Drip_V4.ino.
bool bmAlarmStarted = false;
bool bmPlayChanged = false;  // signals V4 to sync acid play state
byte bmDrumPos  = 0;   // persistent drum pattern position (0-31)
// ── Fill engine ──────────────────────────────────────────────────────
// Procedural fills take over the last 4-8 steps before a boundary (length
// scales with frequency). Density accelerates into a kick+openhat landing.
byte     bmFillMode = 0;   // 0=off  1=every 1 bar  2=every 2 bars  3=every 4 bars
byte     bmFillType = 0;   // 0=HATS  1=CLAP  2=SNARE  3=KIT (tom descent -> snare)
uint16_t bmAbsStep  = 0;   // free-running step counter (fill phase), resets on play
bool     bmWasInFill = false;   // was the previous step inside a fill? (for landing)
// Each fill type's LEAD voice (HATS=hihat CLAP=clap SNARE=snare KIT=tom) has its
// own locked sound (bmFillPitch/Len/Vol — declared with the drum arrays below,
// since they need NUM_DRUMS). Applied during the fill, restored after.
uint8_t  bmFillTouched = 0;     // bitmask of voices given fill sound (need restore)
// Density acceleration: schedule one 32nd-note in-between hit, fired from
// bmUpdateControl. Same mechanism as the old double-time, but fill-only.
bool     bmFillDblPending = false;
uint32_t bmFillDblDueUs   = 0;
byte     bmFillDblDrum     = 0;
uint8_t  bmFillDblGain     = 0;   // final gain for the scheduled 32nd (vol already folded in)
uint32_t bmHumanizeJitUs = 0;     // max late jitter (us) at the current HMNZ setting
// Pot pickup for in-fill drum editing: on a target-drum switch all three
// pots lock, each releasing only once physically moved (movement-gated),
// so a returned-to drum keeps its own values until you touch a pot.
int8_t   bmFillEditDrum = -1;
bool     bmFillPotLock[3] = {true,true,true};   // 0=CUT/pitch 1=RES/len 2=DCY/vol
uint8_t  bmFillPotRef[3]  = {0,0,0};            // pot position captured at lock
#define NUM_DRUMS 8
// HMNZ micro-timing: each pattern hit can be nudged late independently, fired
// from bmUpdateControl (same micros() polling as swing/fill schedulers).
bool     bmHitPend[NUM_DRUMS]  = {};
uint32_t bmHitDueUs[NUM_DRUMS] = {};
uint8_t  bmHitGain[NUM_DRUMS]  = {};
uint32_t bmLastFlashDraw = 0;
bool bmLastFlashState[NUM_DRUMS] = {};
PWMAudio bmPWM(2);  // drum audio on GP2, DMA-driven
volatile bool bmTriggered[NUM_DRUMS] = {};   // written on core0, read in bmFillDrumBuffer() on core1
// Per-hit playback gain = (drum volume * velocity) >> 8, computed at
// trigger time so the audio loop stays at one multiply per voice.
// volatile: written on core 0 (trigger), read+decremented on core 1 (choke fade).
volatile uint8_t bmGain[NUM_DRUMS] = {};
volatile bool bmHatChoke = false;   // closed hat fired → fade out open hat
volatile uint8_t bmLastVel = 0;     // velocity of most recent hit (HMNZ diagnostic)
// Per-voice decay envelope: bmEnv is 8.8 fixed-point gain seeded with
// bmGain<<8 at trigger, multiplied by bmEnvM every sample on core 1.
// bmEnvM 65535 = hold (no decay); smaller = faster exponential decay.
volatile uint16_t bmEnv[NUM_DRUMS]  = {};
uint16_t          bmEnvM[NUM_DRUMS] = {65535,65535,65535,65535,65535,65535,65535,65535};


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
#define BM_DEF_DRUM_PITCH  128  // 128 = pot centre = natural speed
#define BM_DEF_DRUM_CROP   255

// Default mix balance (0-255). Samples are peak-normalized, so this
// table sets the musical balance — tune by ear. On the 16-bit bus a
// lone drum at 210 peaks at ~80% full scale.
//                                      KICK HAT SNARE RIM TOM BASS2 CLAP OPENHAT
const byte bmDefDrumVol[NUM_DRUMS] = { 210, 150, 200, 160, 170, 170, 240, 170 };

const float bmSampleRates[NUM_DRUMS] = {
  (float)sample0_SAMPLERATE, (float)sample1_SAMPLERATE,
  (float)sample2_SAMPLERATE, (float)sample3_SAMPLERATE,
  (float)sample4_SAMPLERATE, (float)sample5_SAMPLERATE,
  (float)sample6_SAMPLERATE, (float)sample7_SAMPLERATE
};
const int bmSampleCells[NUM_DRUMS] = {
  sample0_NUM_CELLS, sample1_NUM_CELLS, sample2_NUM_CELLS,
  sample3_NUM_CELLS, sample4_NUM_CELLS, sample5_NUM_CELLS,
  sample6_NUM_CELLS, sample7_NUM_CELLS
};

// Per-drum params — reset on pattern change, saved in slots
byte bmDrumPitch[NUM_DRUMS];
byte bmDrumLen[NUM_DRUMS];
byte bmDrumVol[NUM_DRUMS];

// Per-fill-voice params — each fill type's lead voice has its own locked sound,
// independent of the pattern. Applied during a fill, restored on the next step.
byte bmFillPitch[NUM_DRUMS];
byte bmFillLen[NUM_DRUMS];
byte bmFillVol[NUM_DRUMS];

// Global params
byte  bmStoredChance   = 128;   // CHANCE: 128=as-is, <128 thins hits, >128 adds ghost hits (live)
byte  bmStoredZoom     = 150;
byte  bmStoredRange    = 0;
byte  bmStoredHumanize = 30;    // velocity randomness 0=robotic..255=loose
byte  bmStoredPitch    = 160;   // global pitch multiplier (applied on top of per-drum)
byte  bmStoredDrive    = 0;     // drive: 0 = clean, 255 = ~4x pre-gain into the soft clip
byte  bmStoredFilter   = 128;   // bipolar DJ filter: 128=bypass, <128 lowpass, >128 highpass
// State-variable (TPT) filter: integrator states + Q16 coeffs, set by bmApplyFilter()
int32_t bmFiltIc1=0, bmFiltIc2=0;
int32_t bmFiltA1=65536, bmFiltA2=0, bmFiltA3=0, bmFiltK=32768;
byte    bmFiltMode=0;           // 0=bypass 1=lowpass 2=highpass
byte  bmStoredAccent   = 255;   // accent depth: 0=flat, 255=full grid
byte  bmStoredBeat     = 2;
float bmStoredTempo    = 120.0f;
byte  bmStoredSwing    = 0;     // 16th-note swing: 0=straight, delays odd steps up to ~60%
bool     bmSwingPending = false;   // an odd (swung) step is scheduled
uint32_t bmSwingDueUs   = 0;       // micros() deadline for the swung step

byte  bmParamBeat;
uint16_t bmDriveGainQ8 = 256;   // drive pre-gain, 8.8 fixed-point (256 = 1.0x)
byte     bmCrushBits   = 0;     // bit-reduction folded into the top of the drive knob
byte  bmParamHumanize;

// Default global func values
#define DEF_PITCH    128  // 128 = neutral (1.0x) — no pitch shift at default
#define DEF_DRIVE    0
#define DEF_CROP     255
#define DEF_DROP     128
#define DEF_ACCENT   255
#define DEF_HUMANIZE 30

// CHANCE drop weights per drum: how susceptible each voice is to a random
// drop (0=never .. 255=often). Anchors (kick, snare) stay near-solid so the
// backbone holds; hats, rim, toms thin out so the groove evolves bar to bar.
//                              KICK HIHAT SNARE RIM TOM BASS2 CLAP OPENHAT
const byte bmChanceWt[NUM_DRUMS] = { 12, 210,  30, 190, 160, 110, 120, 210 };
// GHOST add weights per drum: how readily each voice gets soft ghost hits added
// on empty steps. Hats and rim/snare fill in; kick/bass rarely ghost.
//                              KICK HIHAT SNARE RIM TOM BASS2 CLAP OPENHAT
const byte bmGhostWt[NUM_DRUMS]  = { 15, 230,  90, 200,  70,  40,  60,  80 };

// ── Clock ─────────────────────────────────────────────────────────────
bool     bmPlaying     = false;
byte     bmPulseNum    = 0;
uint16_t bmStepNum     = 0;

// ── Drum edit mode ────────────────────────────────────────────────────
// bmDrumEditPad = 0-7 while one of pads 1-8 is held
uint8_t  bmDrumEditPad    = 0;   // currently selected drum (0-7)
// Pot pickup: holding a pad does NOT change the drum — each pot only takes
// over once it is physically TURNED past where it sat when the pad was grabbed.
// So set values persist until you deliberately move a pot.
uint8_t  bmDrumPotRef[3]  = {0,0,0};
bool     bmDrumPotLock[3] = {true,true,true};
uint8_t  bmDrumEditPrev   = 255;  // drum edited last tick (255 = none held)

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

bool bmFullDirty=true, bmGridDirty=false, bmDotsDirty=false, bmVsDirty=false, bmVsValDirty=false,
     bmPadsDirty=false, bmBarsDirty=false, bmInfoDirty=false, bmDrumsDirty=false,
     bmDStripDirty=false;  // drum strip (name + mod indicators)
bool bmDotsDirtyDrum=false;  // lightweight: repaint only the 3 mod dots for bmDrumEditPad
bool bmBarsForce=false;   // force bmDrawBars to repaint everything (set on full redraw)

// ── Save / Load ───────────────────────────────────────────────────────
#define BM_NUM_SLOTS    4
#define BM_EEPROM_SIZE  512
#define BM_PATCH_VALID  0xC0
#define BM_SAVE_HOLD_MS 1000
#define BM_EEPROM_BASE  256
#define BM_SLOT_ADDR(s) (BM_EEPROM_BASE + (s)*(int)sizeof(BmPatch))

struct BmPatch {
  uint8_t valid;
  byte    beat, crush, filter, chance, accent, pitch;
  byte    fillMode, fillType, swing, humanize;
  uint8_t tempoByte;
  byte    dPitch[NUM_DRUMS], dLen[NUM_DRUMS], dVol[NUM_DRUMS];
};
// Acid_Drip_V4.ino uses the literal value 36 when clearing BM slots — keep in sync.
static_assert(sizeof(BmPatch) == 36, "BmPatch size changed — update clear-all in Acid_Drip_V4.ino");

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
// Clear-all confirmation message: bmClearAllMsg is consumed by bmDoDraw()
// (core 1) — never drawn synchronously from bmUpdateControl() (core 0),
// since only core 1 is allowed to touch the SPI/TFT bus. This timestamp
// then suppresses bmVsDirty-triggered redraws of the value strip until it
// expires, keeping the "DRUM SLOTS CLEARED" message visible briefly.
bool     bmClearAllMsg=false;
uint32_t bmClearAllFlashUntil=0;

// ── Names ─────────────────────────────────────────────────────────────
const char* bmBeatNames[16] = {
  "BASIC",      "HOUSE",      "TECHNO",     "ACID TRACK",
  "HI STATE",   "FUNKY",      "SWING",      "BREAKBEAT",
  "MINIMAL",    "LATIN",      "PACIFIC ST", "BLUE MONDAY",
  "DRUM N BASS","VOODOO RAY", "BUILD DOWN", "JACK BEAT",
};
const char* bmBeatShort[16] = {
  "BASC","HOUS","TECH","ACID",
  "HSOC","FUNK","SWNG","BKBT",
  "MINM","LATN","PCFC","BLMN",
  "DNBT","VOOD","GBLD","JACK",
};
const char*    bmDrumNames[NUM_DRUMS] = {"KICK","HIHAT","SNARE","RIM","TOM","BASS2","CLAP","OPENHAT"};
const uint16_t bmDrumCols[NUM_DRUMS] = {C_RED,C_GRN,C_YEL,C_CYN,0xFC60,C_ORG,C_MGR,C_BLU};
// FUNC slot 7 is now VOLS (drum volume presets) instead of ZOOM
const char*    bmFuncNames[8] = {"PTCH","DRIV","FILT","CHNC","HMNZ","SWNG","ACNT","FILL"};
const char*    bmFuncFullNames[8] = {"PITCH","DRIVE","FILTER","CHANCE","HUMANIZE","SWING","ACCENT","FILL"};

// ── Helpers ───────────────────────────────────────────────────────────
float bmByteToTempo(byte b){
  return (b<=192) ? 10.0f+b : 202.0f+12.66667f*(b-192.0f);
}
byte bmTempoToByte(float t){
  if(t<=202.0f) return (byte)constrain((int)(t-10.0f),0,192);
  return (byte)constrain((int)((t-202.0f)/12.66667f+192.0f),192,255);
}

// Per-drum pitch: exponential / equal-temperament, pot 128 = natural 1.0x.
// Every 64 pot units = one octave (each unit ~19 cents), so the knob sweeps
// in even musical steps. Endpoints unchanged: 0 → 0.25x (-2 oct), 255 → ~4x.
//   freq mult = 2^((pv - 128) / 64)
// Global FUNC pitch (bmStoredPitch) multiplies on top — and because
// multiplying frequencies adds intervals, it acts as a clean transpose.
float bmPitchMult(byte pv){
  return powf(2.0f, ((float)pv - 128.0f) / 64.0f);
}

// Decay multiplier from an effective length (shared by pattern + fill apply)
uint16_t bmEnvMFor(uint16_t effLen){
  if(effLen >= 250) return 65535;                  // no decay — natural sample
  float tau = 0.004f * expf((float)effLen * (6.215f/250.0f));
  float m   = 65536.0f * expf(-1.0f/(tau*16384.0f));
  return (uint16_t)min(65535.0f, m);
}

// Bipolar DJ filter coefficients (TPT state-variable). Knob 128 = bypass;
// below 128 sweeps a lowpass down (~6kHz → 80Hz), above 128 sweeps a
// highpass up (80Hz → ~4kHz). Fixed moderate resonance. Float here is fine —
// only runs when the knob moves; the audio path uses the integer coeffs.
void bmApplyFilter(){
  int p = bmStoredFilter;
  float fc;
  if(p < 122){      bmFiltMode=1; fc = 80.0f*powf(75.0f,(float)p/121.0f); }        // lowpass
  else if(p > 134){ bmFiltMode=2; fc = 80.0f*powf(50.0f,(float)(p-134)/121.0f); }  // highpass
  else {            bmFiltMode=0; fc = 6000.0f; }                                   // bypass
  float g = tanf(3.14159265f * fc / 16384.0f);
  float k = 0.5f;                       // resonance damping (lower = more squelch)
  float a1 = 1.0f/(1.0f + g*(g+k));
  bmFiltA1 = (int32_t)(a1*65536.0f);
  bmFiltA2 = (int32_t)(g*a1*65536.0f);
  bmFiltA3 = (int32_t)(g*g*a1*65536.0f);
  bmFiltK  = (int32_t)(k*65536.0f);
}
void bmVoiceSetFreq(byte d, float f){
  switch(d){
    case 0:bms0.setFreq(f);break; case 1:bms1.setFreq(f);break; case 2:bms2.setFreq(f);break;
    case 3:bms3.setFreq(f);break; case 4:bms4.setFreq(f);break; case 5:bms5.setFreq(f);break;
    case 6:bms6.setFreq(f);break; case 7:bms7.setFreq(f);break;
  }
}

void bmApplyDrum(byte d){
  float freq = bmPitchMult(bmStoredPitch) * bmPitchMult(bmDrumPitch[d])
               * bmSampleRates[d] / (float)bmSampleCells[d];
  // ── Decay envelope (replaces the old hard setEnd truncation) ──────
  // LEN sets the per-drum decay time. effLen 0 → ~4ms blip, 250+ → hold.
  // Exponential decay: bmEnvM is the per-sample 0.16 multiplier,
  // computed here at control rate (float ok), applied in the mixer.
  uint16_t effLen = bmDrumLen[d];
  if(effLen >= 250){
    bmEnvM[d] = 65535;                       // no decay — natural sample
  } else {
    // tau sweeps 4ms → 2s exponentially across the pot range
    float tau = 0.004f * expf((float)effLen * (6.215f/250.0f));
    float m   = 65536.0f * expf(-1.0f/(tau*16384.0f));
    bmEnvM[d] = (uint16_t)min(65535.0f, m);
  }
  switch(d){
    case 0: bms0.setFreq(freq); bms0.setEnd(bmSampleCells[0]); break;
    case 1: bms1.setFreq(freq); bms1.setEnd(bmSampleCells[1]); break;
    case 2: bms2.setFreq(freq); bms2.setEnd(bmSampleCells[2]); break;
    case 3: bms3.setFreq(freq); bms3.setEnd(bmSampleCells[3]); break;
    case 4: bms4.setFreq(freq); bms4.setEnd(bmSampleCells[4]); break;
    case 5: bms5.setFreq(freq); bms5.setEnd(bmSampleCells[5]); break;
    case 6: bms6.setFreq(freq); bms6.setEnd(bmSampleCells[6]); break;
    case 7: bms7.setFreq(freq); bms7.setEnd(bmSampleCells[7]); break;
  }
}

// Drive params only (gain + crush tip + makeup). No per-drum work — cheap
// enough to call on every pot tick without stalling the drum clock.
void bmApplyDrive(){
  // Exponential gain: gentle in the lower half, heavy in the upper half so the
  // top of the knob keeps biting instead of plateauing. 1x .. ~10x.
  bmDriveGainQ8 = (uint16_t)(256.0f * powf(10.0f, (float)bmStoredDrive/255.0f) + 0.5f);
  bmCrushBits   = (bmStoredDrive>=216) ? (byte)((bmStoredDrive-216)/5) : 0;  // 0..7 bits at the top
  // No makeup: the saturation cap sets the level. Drive gets louder/grittier as
  // you push it (overdrive character) instead of being held flat.
}

// Humanize params only (velocity spread + timing jitter). No per-drum work.
void bmApplyHumanize(){
  bmParamHumanize = bmStoredHumanize;
  bmHumanizeJitUs = (uint32_t)bmStoredHumanize * 86;          // max late jitter ~22ms at full
}

// Recompute only drum FREQUENCIES (global pitch changed). Skips the decay
// envelope — pitch doesn't affect it — and computes the global multiplier
// once, so a PTCH pot sweep stays cheap and never stalls the drum clock.
void bmReapplyPitch(){
  float gp = bmPitchMult(bmStoredPitch);
  for(byte d=0; d<NUM_DRUMS; d++)
    bmVoiceSetFreq(d, gp * bmPitchMult(bmDrumPitch[d]) * bmSampleRates[d] / (float)bmSampleCells[d]);
}

void bmApplyParams(){
  bmApplyHumanize();
  bmParamBeat = bmStoredBeat;
  bmApplyDrive();
  bmApplyFilter();
  for(byte d=0;d<NUM_DRUMS;d++) bmApplyDrum(d);
}
// Lightweight: only update beat pattern index. Does NOT reapply drive/filter/
// per-drum edits — those only change when the user deliberately adjusts them
// via FUNC mode or explicitly loads a slot.
void bmApplyBeat(){
  bmParamBeat = bmStoredBeat;
}

void bmResetFuncParams(){
  bmStoredPitch=DEF_PITCH; bmStoredDrive=DEF_DRIVE; bmStoredFilter=128;
  bmFiltIc1=bmFiltIc2=0;
  bmStoredChance=128; bmStoredAccent=DEF_ACCENT; bmStoredHumanize=DEF_HUMANIZE;
}

void bmResetDrumParams(){
  for(byte d=0;d<NUM_DRUMS;d++){
    bmDrumPitch[d]=128;                 // mid pitch
    bmDrumLen[d]  =191;                 // 3/4 length
    bmDrumVol[d]  =191;                 // 3/4 volume
    bmFillPitch[d]=BM_DEF_DRUM_PITCH;   // fill voices start matching the pattern
    bmFillLen[d]  =BM_DEF_DRUM_CROP;
    bmFillVol[d]  =bmDefDrumVol[d];
  }
}

// ── Save / Load ───────────────────────────────────────────────────────
void bmSavePatch(uint8_t slot){
  if(slot>=BM_NUM_SLOTS) return;
  BmPatch p;
  p.valid=BM_PATCH_VALID; p.beat=bmStoredBeat;
  p.crush=bmStoredDrive; p.filter=bmStoredFilter; p.chance=bmStoredChance;
  p.accent=bmStoredAccent; p.pitch=bmStoredPitch;
  p.fillMode=bmFillMode; p.fillType=bmFillType;
  p.swing=bmStoredSwing; p.humanize=bmStoredHumanize;
  p.tempoByte=bmTempoToByte(bmStoredTempo);
  for(byte d=0;d<NUM_DRUMS;d++){p.dPitch[d]=bmDrumPitch[d];p.dLen[d]=bmDrumLen[d];p.dVol[d]=bmDrumVol[d];}
  EEPROM.put(BM_SLOT_ADDR(slot),p);
  bmSlotHasData[slot]=true;
}

void bmLoadPatch(uint8_t slot){
  if(slot>=BM_NUM_SLOTS||!bmSlotHasData[slot]) return;
  BmPatch p; EEPROM.get(BM_SLOT_ADDR(slot),p);
  if(p.valid!=BM_PATCH_VALID) return;
  bmStoredBeat=p.beat; bmStoredDrive=p.crush;
  bmStoredFilter=p.filter; bmApplyFilter();
  bmStoredChance=p.chance;
  bmStoredAccent=p.accent; bmStoredPitch=p.pitch;
  bmFillMode=(p.fillMode<=3)?p.fillMode:0; bmFillType=(p.fillType<=3)?p.fillType:0;
  bmStoredSwing=p.swing; bmStoredHumanize=p.humanize;
  bmStoredTempo=constrain(bmByteToTempo(p.tempoByte),40.0f,250.0f);
  seq.tempo=(uint16_t)(bmStoredTempo+0.5f); seq.interval=bpm2us(seq.tempo);
  for(byte d=0;d<NUM_DRUMS;d++){bmDrumPitch[d]=p.dPitch[d];bmDrumLen[d]=p.dLen[d];bmDrumVol[d]=p.dVol[d];}
  bmApplyParams();
  bmLastLoadedSlot=(int8_t)slot;
  bmFullDirty=true;
}

// Reset the beat pattern to Basic (index 0) and clear the "slot loaded"
// indicator. Called every time drum mode is entered so drum mode never
// silently starts on whatever slot was last active — either from a
// previous bmLoadPatch() in an earlier drum-mode session, or from the
// acid patch loader's shared-slot auto-load (loadPatch() in
// Acid_Drip_V4.ino calls bmLoadPatch(slot) when the same slot number
// has drum data). A saved slot should only be live after the user
// explicitly picks it while in drum mode.
void bmResetToBasic(){
  bmStoredBeat = 0;
  bmLastLoadedSlot = -1;
  bmApplyBeat();
  bmPadsDirty = true; bmInfoDirty = true; bmGridDirty = true; bmDStripDirty = true;
}

void bmCheckSlots(){
  for(uint8_t s=0;s<BM_NUM_SLOTS;s++){
    uint8_t v; EEPROM.get(BM_SLOT_ADDR(s),v);
    bmSlotHasData[s]=(v==BM_PATCH_VALID);
  }
}

// ── Beat engine ──────────────────────────────────────────────────────
// Old DrumKid pulse engine (bmGetZoom / bmCalcNote / bmDoPulse) removed —
// drums are triggered exclusively by bmTriggerStep() from the acid clock.

// ── bmTriggerStep: called from acid advanceStep() ────────────────────
// Triggers drums on acid step position (0-15) — perfectly locked, zero drift
// ── bmFireFill: one fill step (fillPos 0..fillLen-1) ─────────────────
// Acid/house/techno fill idioms. Velocity ramps across the window; the
// final quarter schedules 32nd in-betweens so the fill accelerates in.
//   HATS  = closed-hat build opening to an open hat, kick holds the floor
//   CLAP  = clap build over the steady kick
//   SNARE = flat snare rush, no kick — the landing slams
//   KIT   = descending tom for ~70%, then snare into the landing (no kick)
void bmFireFill(uint8_t fillPos, uint8_t fillLen, uint8_t drumStep){
  uint8_t type = bmFillType;                       // 0=HATS 1=CLAP 2=SNARE 3=KIT
  float   p    = (fillLen>1) ? (float)fillPos/(float)(fillLen-1) : 0.0f;  // 0..1

  // Half-time kick stays under the HATS/CLAP builds (anchors the groove
  // while leaving space). SNARE and KIT drop it so the run-up is clean.
  if((type==0 || type==1) && (drumStep % 8 == 0)){
    bmGain[0]=bmDrumVol[0]; bmEnv[0]=(uint16_t)bmGain[0]<<8; bmTriggered[0]=true; bms0.start();
    bmFired[0]=true; bmFiredMs[0]=millis(); bmDrumsDirty=true;
  }

  // Pick the fill voice (and pitch bend) for this step
  byte d; bool bend=false;
  switch(type){
    case 0:  d=1; if(fillPos >= fillLen-2) d=7; break;   // HATS: closed, open on the tail
    case 1:  d=6;                                break;   // CLAP
    case 2:  d=2;                                break;   // SNARE rush (flat)
    default:                                              // KIT: descending toms with
      if(fillPos < (uint8_t)((fillLen*7)/10)){            //   rim ghosts, into a snare buzz
        if(fillPos & 1) d=3;                              //   odd  → rim ghost
        else          { d=4; bend=true; }                 //   even → descending tom
      } else d=2;                                         //   tail → snare (buzz via 32nds)
      break;
  }

  // Sparse opening (rest on odd steps in the first third → 8th feel) — only
  // the SNARE rush uses it. HATS/CLAP run continuous; KIT fills odds with rim.
  bool skip = (type==2) && (fillPos < fillLen/3) && (fillPos & 1);

  uint8_t vel = (uint8_t)(150 + (uint8_t)(105.0f*p));   // 150..255 across the window
  if(type==3 && d==3) vel = (uint8_t)(((uint16_t)vel*5)>>3);   // rim ghosts sit back (~0.6)

  // The type's LEAD voice plays its own locked sound; auxiliary voices (rim,
  // open hat, KIT's snare tail) follow the pattern.
  byte lead = (type==0)?1 : (type==1)?6 : (type==2)?2 : 4;
  uint8_t vol;
  if(d==lead){
    float ffreq = bmPitchMult(bmStoredPitch)*bmPitchMult(bmFillPitch[d])
                  * bmSampleRates[d] / (float)bmSampleCells[d];
    if(bend) ffreq *= (1.0f - 0.45f*p);                       // tom descends
    bmVoiceSetFreq(d, ffreq);
    bmEnvM[d] = bmEnvMFor(bmFillLen[d]);
    bmFillTouched |= (1<<d);                                  // restore after the fill
    vol = bmFillVol[d];
  } else {
    vol = bmDrumVol[d];
  }

  if(!skip){
    bmGain[d]=(uint8_t)(((uint16_t)vol*vel)>>8);
    bmEnv[d] =(uint16_t)bmGain[d]<<8;
    bmLastVel=vel;
    bmTriggered[d]=true;
    if(d==1) bmHatChoke=true; else if(d==7) bmHatChoke=false;   // hat choke
    switch(d){
      case 1:bms1.start();break; case 2:bms2.start();break; case 3:bms3.start();break;
      case 4:bms4.start();break; case 6:bms6.start();break; case 7:bms7.start();break;
    }
    bmFired[d]=true; bmFiredMs[d]=millis();
    bmDrumsDirty=true;
  }

  // Final quarter: schedule a softer 32nd in-between (skip clap — too long,
  // it would smear into mush).
  uint8_t lastZone = (fillLen/4 < 1) ? 1 : fillLen/4;
  if(type!=1 && fillPos >= fillLen - lastZone){
    bmFillDblDrum = d;
    bmFillDblGain = (uint8_t)(((uint16_t)vol * (uint8_t)(((uint16_t)vel*3)>>2))>>8);  // ~75% buzz, fill vol
    bmFillDblDueUs= micros() + seq.interval/2;
    bmFillDblPending = true;
  }
}

// Trigger one drum voice: seed gain/envelope, handle hat choke, start the
// sample. Called immediately, or deferred by the HMNZ timing scheduler.
void bmStartVoice(byte s, uint8_t gain){
  bmGain[s]=gain;
  bmEnv[s]=(uint16_t)gain<<8;
  if(s==1) bmHatChoke=true;        // closed hat chokes the open hat
  if(s==7) bmHatChoke=false;       // fresh open hat overrides choke
  bmTriggered[s]=true;
  switch(s){
    case 0:bms0.start();break; case 1:bms1.start();break;
    case 2:bms2.start();break; case 3:bms3.start();break;
    case 4:bms4.start();break; case 5:bms5.start();break;
    case 6:bms6.start();break; case 7:bms7.start();break;
  }
  bmFired[s]=true; bmFiredMs[s]=millis(); bmDrumsDirty=true;
}

// Fire a hit now, or — when HMNZ timing is up — nudge it late by a per-hit
// random amount. Shared by pattern hits and ghost hits so both humanize.
void bmFireHit(byte s, uint8_t gain){
  if(bmHumanizeJitUs){
    // If this drum still has a hit pending, fire it first so it isn't lost
    // when we overwrite the slot (prevents dropped hits = "off" timing).
    if(bmHitPend[s]){ bmHitPend[s]=false; bmStartVoice(s,bmHitGain[s]); }
    // Cap jitter to 1/3 of the step so it always lands inside its own step,
    // at any tempo — never bleeding into the next step.
    uint32_t maxJit = bmHumanizeJitUs;
    uint32_t cap = seq.interval/3;
    if(maxJit > cap) maxJit = cap;
    uint32_t jit=((uint32_t)rand(256)*maxJit)>>8;
    if(jit<800) bmStartVoice(s,gain);                                 // ~on-grid, fire now
    else { bmHitGain[s]=gain; bmHitDueUs[s]=micros()+jit; bmHitPend[s]=true; }
  } else bmStartVoice(s,gain);
}

void bmFireDrumStep(){
  uint8_t drumStep = bmDrumPos % 32;
  bmDrumPos = (bmDrumPos + 1) % 32;

  // Fill window: length scales with frequency so rare fills are bigger
  // events. 1 bar -> 4 steps (1 beat), 2 bar -> 6, 4 bar -> 8 (half bar).
  const uint16_t fillPeriod = (bmFillMode==1)?16 : (bmFillMode==2)?32 : (bmFillMode==3)?64 : 0;
  const uint8_t  fillLen    = (bmFillMode==3)?8 : (bmFillMode==2)?6 : 4;
  uint16_t posInPeriod = fillPeriod ? (bmAbsStep % fillPeriod) : 0;
  bool     inFill  = fillPeriod && (posInPeriod >= (uint16_t)(fillPeriod-fillLen));
  uint8_t  fillPos = inFill ? (uint8_t)(posInPeriod - (fillPeriod-fillLen)) : 0;
  bool     landing = (!inFill && bmWasInFill);   // first normal step after a fill = the "1"
  bmWasInFill = inFill;
  bmAbsStep++;

  // Restore any voices the fill re-tuned, on the first normal step after it
  if(!inFill && bmFillTouched){
    for(byte d=0; d<NUM_DRUMS; d++) if(bmFillTouched & (1<<d)) bmApplyDrum(d);
    bmFillTouched = 0;
  }

  if(inFill){
    bmFireFill(fillPos, fillLen, drumStep);
    bmStepNum=(uint16_t)drumStep*6; bmGridDirty=true;
    return;
  }
  // CHANCE split: below centre thins existing hits, above centre adds ghosts.
  uint8_t thinAmt  = (bmStoredChance<128) ? (uint8_t)(((uint16_t)(128-bmStoredChance)*255)/128) : 0;
  uint8_t ghostAmt = (bmStoredChance>128) ? (uint8_t)(((uint16_t)(bmStoredChance-128)*255)/127) : 0;
  for(byte s=0;s<NUM_DRUMS;s++){
    byte bb=pgm_read_byte(&beats[bmParamBeat][s][drumStep/8]);
    if(!bitRead(bb,7-(drumStep%8))){
      // GHOST: maybe add a soft hit on an empty WEAK step (off-16ths favored),
      // weighted per drum so hats/rim/snare fill in and the kick stays clean.
      if(ghostAmt){
        uint8_t pw=((drumStep&3)==0)?30:((drumStep&1)==0)?120:255;   // weak steps favored
        uint8_t gProb=((uint32_t)ghostAmt*bmGhostWt[s]*pw)>>16;
        if(gProb && (uint8_t)rand(256)<gProb){
          uint8_t gvel=60+(uint8_t)rand(40);                          // soft ghost
          bmFireHit(s,(uint8_t)(((uint16_t)bmDrumVol[s]*gvel)>>8));   // humanized like main hits
          bmLastVel=gvel;
        }
      }
      continue;
    }
    // THIN: ordered per-hit drop. Per-drum weight (anchors solid) times a metric
    // weight so the order of thinning is musical — weak in-between 16ths drop
    // first, 8th-offbeats next, downbeats protected — so the groove degrades and
    // fills back gracefully instead of scattershot.
    if(thinAmt){
      uint8_t pw = ((drumStep&3)==0) ? 40 : ((drumStep&1)==0) ? 130 : 255;  // strong..weak
      uint8_t effProb=((uint32_t)thinAmt*bmChanceWt[s]*pw)>>16;
      if(effProb && (uint8_t)rand(256)<effProb) continue;   // dropped this bar
    }
    // ── Velocity ──────────────────────────────────────────────────
    // Bipolar accent (ACNT). 128 = flat. Above 128 = ON-beat emphasis:
    // quarter notes punch, in-between 16ths soften (drives the pulse).
    // Below 128 = OFF-beat emphasis: the 8th-note "and"s punch, downbeats
    // soften (pushed house/garage feel). Magnitudes match at the extremes.
    int8_t acc = (int8_t)((int)bmStoredAccent - 128);   // -128..+127
    uint8_t vel;
    if(acc >= 0){
      uint16_t d=(uint16_t)acc;                                       // 0..127
      vel = ((drumStep & 3)==0) ? (uint8_t)(200+((110*d)>>8))         // quarter — loud
          : ((drumStep & 1)==0) ? (uint8_t)200                        // 8th offbeat — neutral
          :                       (uint8_t)(200-((70*d)>>8));         // 16th — soft
    } else {
      uint16_t d=(uint16_t)(-acc);                                    // 0..128
      vel = ((drumStep & 3)==0) ? (uint8_t)(200-((70*d)>>8))          // quarter — soft
          : ((drumStep & 1)==0) ? (uint8_t)(200+((110*d)>>8))         // 8th offbeat — loud
          :                       (uint8_t)200;                       // 16th — neutral
    }
    // HMNZ: bipolar random spread centred on the accent value, so the
    // average level stays constant across the whole knob — only the
    // hit-to-hit looseness grows. (A subtract-only version of this
    // sounded like a volume control: mean dropped ~8dB at full knob.)
    if(bmParamHumanize){
      int v=(int)vel + (int)rand((int)bmParamHumanize+1) - (int)(bmParamHumanize>>1);
      vel=(uint8_t)constrain(v,50,255);
    }
    uint8_t g=(uint8_t)(((uint16_t)bmDrumVol[s]*vel)>>8);
    bmLastVel=vel;   // live readout on the HMNZ value strip
    // HMNZ timing: nudge each hit late by a per-hit random amount (knob-scaled)
    // so the kit drifts off the grid independently — humanized micro-timing.
    bmFireHit(s,g);
  }
  // Landing resolve: a fill just ended → punch kick + open hat full on the
  // "1" so the fill arrives instead of just stopping.
  if(landing){
    bmGain[0]=bmDrumVol[0]; bmEnv[0]=(uint16_t)bmGain[0]<<8; bmTriggered[0]=true; bms0.start();
    bmFired[0]=true; bmFiredMs[0]=millis();
    bmGain[7]=bmDrumVol[7]; bmEnv[7]=(uint16_t)bmGain[7]<<8; bmTriggered[7]=true; bms7.start();
    bmHatChoke=false;   // let the open hat ring through the landing
    bmFired[7]=true; bmFiredMs[7]=millis();
    if(bmFillType==3){  // KIT lands with an added clap — the house slam
      bmGain[6]=bmDrumVol[6]; bmEnv[6]=(uint16_t)bmGain[6]<<8; bmTriggered[6]=true; bms6.start();
      bmFired[6]=true; bmFiredMs[6]=millis();
    }
    bmDrumsDirty=true;
  }
  bmStepNum=(uint16_t)drumStep*6;
  bmGridDirty=true;
}

void bmTriggerStep(uint8_t step){
  (void)step;
  if(!bmPlaying) return;
  // Flush any still-pending swung step first so positions never overlap
  if(bmSwingPending){ bmSwingPending=false; bmFireDrumStep(); }
  // Swing: delay the odd 16th steps (the "e" and "a") by up to ~60% of the
  // step interval. bmDrumPos is the step about to play (before it advances).
  if(bmStoredSwing && (bmDrumPos & 1)){
    uint16_t swingPct = ((uint16_t)bmStoredSwing * 60) / 255;   // 0..60 %
    bmSwingDueUs = micros() + (((uint32_t)seq.interval * swingPct) / 100);
    bmSwingPending = true;
  } else {
    bmFireDrumStep();
  }
}

// ── Start drums cleanly ──────────────────────────────────────────────
// Resets sequencer position/fill-state and marks drums playing. Small
// named helper (rather than inlining) so the drum-mode START branch stays
// readable and there's one place that defines what "start drums" means.
void bmStartDrums(){
  bmPlaying=true;
  bmPulseNum=0;bmStepNum=0;bmDrumPos=0;bmAbsStep=0;bmFillTouched=0;bmWasInFill=false;bmFillDblPending=false;bmSwingPending=false;
  for(byte _h=0;_h<NUM_DRUMS;_h++)bmHitPend[_h]=false;
  bmPlayChanged=true;
  bmVsDirty=true;bmInfoDirty=true;bmGridDirty=true;bmPadsDirty=true;
}

// ── Full reset ────────────────────────────────────────────────────────
void bmDoReset(){
  bmPlaying=false; bmPulseNum=0; bmStepNum=0; bmDrumPos=0; bmAbsStep=0;
  bmFillMode=0; bmFillType=0; bmFillTouched=0;
  bmWasInFill=false; bmFillDblPending=false;
  for(byte _h=0;_h<NUM_DRUMS;_h++) bmHitPend[_h]=false;
  bmFillEditDrum=-1;
  for(byte _i=0;_i<NUM_DRUMS;_i++) bmTriggered[_i]=false;
  bmStoredChance=128; bmStoredZoom=150; bmStoredRange=0;
  bmStoredSwing=0; bmSwingPending=false;
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

  // Swing: fire the delayed odd step when its offset elapses
  if(bmSwingPending){
    if(!bmPlaying) bmSwingPending=false;
    else if((int32_t)(micros()-bmSwingDueUs) >= 0){ bmSwingPending=false; bmFireDrumStep(); }
  }

  // Fill density acceleration: fire the scheduled 32nd-note in-between hit
  if(bmFillDblPending){
    if(!bmPlaying) bmFillDblPending=false;
    else if((int32_t)(micros()-bmFillDblDueUs) >= 0){
      bmFillDblPending=false;
      byte d=bmFillDblDrum;
      bmGain[d]=bmFillDblGain;
      bmEnv[d] =(uint16_t)bmGain[d]<<8;
      bmTriggered[d]=true;
      switch(d){ case 0:bms0.start();break; case 1:bms1.start();break; case 2:bms2.start();break;
                 case 4:bms4.start();break; case 6:bms6.start();break; case 7:bms7.start();break; }
      bmFired[d]=true; bmFiredMs[d]=millis(); bmDrumsDirty=true;
    }
  }

  // HMNZ timing: fire each late-nudged pattern hit when its offset elapses
  for(byte d=0; d<NUM_DRUMS; d++){
    if(bmHitPend[d]){
      if(!bmPlaying) bmHitPend[d]=false;
      else if((int32_t)(micros()-bmHitDueUs[d]) >= 0){ bmHitPend[d]=false; bmStartVoice(d,bmHitGain[d]); }
    }
  }

  // HMNZ diagnostic: refresh value strip per hit so V: readout is live
  { static uint8_t lastShownVel=255;
    if(bmFuncMode && bmFuncSel==4 && bmLastVel!=lastShownVel){
      lastShownVel=bmLastVel; bmVsDirty=true;
    }
  }
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
  // Mode switch (pads 9+10 held >=1s) is handled exclusively by
  // doModeSwitch() in Acid_Drip_V4.ino's updateControl() — having two
  // independent checks of the same bmModeArmed/bmModeArmMs flags risked
  // inconsistent state (bmStoredTempo not synced, double bmMode toggles
  // under edge-case timing). Only the disarm-on-release check stays here
  // as a safety net in case this file's updateControl ever runs first.
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
  if(bmFuncMode && bmFuncSel==7){
    // FILL slot: the three pots edit this fill type's LEAD voice sound —
    // its own locked pitch/len/vol, independent of the pattern drum. Movement-
    // gated. HATS=hihat CLAP=clap SNARE=snare KIT=tom.
    byte d = (bmFillType==0)?1 : (bmFillType==1)?6 : (bmFillType==2)?2 : 4;   // hat/clap/snare/tom
    if((int8_t)d != bmFillEditDrum){     // target switched → lock all pots
      bmFillEditDrum = (int8_t)d;
      bmFillPotLock[0]=bmFillPotLock[1]=bmFillPotLock[2]=true;
      bmFillPotRef[0]=bmPotCut; bmFillPotRef[1]=bmPotRes; bmFillPotRef[2]=bmPotDcy;
    }
    bool ch=false;
    // CUT → fill pitch
    if(bmFillPotLock[0]){ if(abs((int)bmPotCut-(int)bmFillPotRef[0])>6) bmFillPotLock[0]=false; }
    if(!bmFillPotLock[0] && abs((int)bmPotCut-(int)bmFillPitch[d])>2){bmFillPitch[d]=bmPotCut; ch=true;}
    // RES → fill length
    if(bmFillPotLock[1]){ if(abs((int)bmPotRes-(int)bmFillPotRef[1])>6) bmFillPotLock[1]=false; }
    if(!bmFillPotLock[1] && abs((int)bmPotRes-(int)bmFillLen[d])>2){bmFillLen[d]=bmPotRes; ch=true;}
    // DCY → fill volume
    if(bmFillPotLock[2]){ if(abs((int)bmPotDcy-(int)bmFillPotRef[2])>6) bmFillPotLock[2]=false; }
    if(!bmFillPotLock[2] && abs((int)bmPotDcy-(int)bmFillVol[d])>2){bmFillVol[d]=bmPotDcy; ch=true;}
    if(ch){bmBarsDirty=true;bmVsDirty=true;bmDStripDirty=true;}
  } else if(bmFuncMode&&bmFuncSel!=255){
    if(bmPotLocked){int d=(int)bmPotCut-(int)bmPotPickup;if(d<-6||d>6){bmPotLocked=false;bmVsDirty=true;}}
    if(!bmPotLocked){
      // Rate-limit param tracking to ~60Hz. ADC noise dithers the low bit, so a
      // bare != re-runs the apply (filter uses tanf/powf) and flags redraws on
      // nearly every loop even when the pot is still — that floods core 0 and
      // delays the drum sequencer enough to drop a step. The pot doesn't need
      // sub-ms response; capping the work keeps func-mode timing identical to
      // out-of-func while preserving full range (exact 128 centre detent, etc).
      static uint32_t bmFuncPotMs=0;
      uint32_t nowP=millis();
      if((uint32_t)(nowP-bmFuncPotMs) >= 15){
        bmFuncPotMs=nowP;
        bool ch=false;
        switch(bmFuncSel){
          case 0:if(bmPotCut!=bmStoredPitch)   {bmStoredPitch=bmPotCut;   bmReapplyPitch();ch=true;}break;
          case 1:if(bmPotCut!=bmStoredDrive)   {bmStoredDrive=bmPotCut;   bmApplyDrive();  ch=true;}break;
          case 2:if(bmPotCut!=bmStoredFilter)  {bmStoredFilter=bmPotCut;  bmApplyFilter(); ch=true;}break;
          case 3:if(bmPotCut!=bmStoredChance)  {bmStoredChance=bmPotCut;  ch=true;}break;
          case 4:if(bmPotCut!=bmStoredHumanize){bmStoredHumanize=bmPotCut;bmApplyHumanize();ch=true;}break;
          case 5:if(bmPotCut!=bmStoredSwing){bmStoredSwing=bmPotCut;ch=true;}break;
          case 6:if(bmPotCut!=bmStoredAccent)  {bmStoredAccent=bmPotCut;  ch=true;}break;
          case 7: break;  // BEAT: no pot control, tiles only
        }
        if(ch){bmBarsDirty=true;bmVsValDirty=true;}
      }
    }
  } else {
    // Normal mode: a held pad selects a drum, but its values only change when a
    // pot is actually TURNED — never just by holding the pad. On grabbing a pad
    // the three pots lock at their current positions; each unlocks only once
    // moved past a threshold, then tracks. Released or idle pots leave the drum
    // untouched, so set values persist across pattern changes and mode switches.
    byte d=bmDrumEditPad;
    bool padHeld=(d<NUM_DRUMS && bmPState[d]);
    if(padHeld){
      if(d!=bmDrumEditPrev){                       // just grabbed this drum → lock pots here
        bmDrumPotRef[0]=bmPotCut; bmDrumPotRef[1]=bmPotRes; bmDrumPotRef[2]=bmPotDcy;
        bmDrumPotLock[0]=bmDrumPotLock[1]=bmDrumPotLock[2]=true;
        bmDrumEditPrev=d;
      }
      // Rate-limit drum param application to ~60Hz so powf/expf in bmApplyDrum
      // cannot block the acid step sequencer. Full ADC range is still reachable —
      // only the apply (not the read) is throttled, identical to func-mode pots.
      static uint32_t bmDrumPotMs=0;
      uint32_t nowD=millis();
      bool ch=false;
      if(bmDrumPotLock[0]){ if(abs((int)bmPotCut-(int)bmDrumPotRef[0])>6) bmDrumPotLock[0]=false; }
      if(!bmDrumPotLock[0] && bmPotCut!=bmDrumPitch[d]){ bmDrumPitch[d]=bmPotCut; ch=true; }
      if(bmDrumPotLock[1]){ if(abs((int)bmPotRes-(int)bmDrumPotRef[1])>6) bmDrumPotLock[1]=false; }
      if(!bmDrumPotLock[1] && bmPotRes!=bmDrumLen[d]){ bmDrumLen[d]=bmPotRes; ch=true; }
      if(bmDrumPotLock[2]){ if(abs((int)bmPotDcy-(int)bmDrumPotRef[2])>6) bmDrumPotLock[2]=false; }
      if(!bmDrumPotLock[2] && bmPotDcy!=bmDrumVol[d]){ bmDrumVol[d]=bmPotDcy; bmGain[d]=bmPotDcy; bmEnv[d]=(uint16_t)bmPotDcy<<8; ch=true; }
      if(ch){
        bmBarsDirty=true;
        // Use bmVsValDirty (not bmVsDirty) so only the P/L/V numbers repaint,
        // not the full strip — avoids the flickering drum name / BPM / play state.
        bmVsValDirty=true;
        // bmDStripDirty triggers a full 320px clear + 8-tile redraw — too expensive
        // during pot movement and starves bmGridDirty. Instead use bmDotsDirtyDrum
        // which redraws only the 3 mod dots for the active drum (9 tiny fillRects).
        bmDotsDirtyDrum=true;
        // Apply the heavy float work (powf/expf) at most every 15ms so it cannot
        // stall the acid clock between steps. Values are already written above;
        // the audio path reads bmEnvM[d] next sample — a 15ms lag is inaudible.
        if((uint32_t)(nowD-bmDrumPotMs) >= 15){
          bmDrumPotMs=nowD;
          bmApplyDrum(d);
        }
      }
    } else {
      bmDrumEditPrev=255;                          // released → next grab re-locks
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
          bmFillEditDrum=-1;
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
            if(fn==3){bmStoredChance=(bmStoredChance+32)%256;bmInfoDirty=true;bmVsDirty=true;}
            bmFuncSel=fn; bmPotLocked=true; bmPotPickup=(uint8_t)(analogRead(POT_CUT)>>2);
            bmFillEditDrum=-1;   // force fill pots to re-lock on (re-)entry
            bmVsDirty=true; bmPadsDirty=true; bmBarsDirty=true; bmDStripDirty=true; bmInfoDirty=true;
          } else {
            if(bmFuncSel==7){
              // Pads 1-4: fill frequency  OFF / 1 BAR / 2 BAR / 4 BAR
              // Pads 5-8: fill type       HATS / CLAP / SNARE / KIT
              // Pads 7+8 are also the FUNC-exit chord, so SNARE/KIT act on
              // release (below) — a press here would beat the chord.
              if(i<4) bmFillMode = i;                              // freq
              else if(i!=BM_PAD_FUNC_A && i!=BM_PAD_FUNC_B) bmFillType = i-4;  // HATS/CLAP
              bmVsDirty=true; bmBarsDirty=true; bmDStripDirty=true; bmInfoDirty=true;
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
              bmStoredBeat=i; bmApplyBeat();
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
          else{
            if(!bmPlaying){
              // START (drum mode): starts drums only.
              bmStartDrums();
            } else {
              // STOP (drum mode): stops drums only. Acid is untouched and
              // keeps driving the shared clock if it's still running — see
              // updateControl()'s bmPlayChanged handler in Acid_Drip_V4.ino,
              // which only kills seq.running once acid is also stopped.
              bmPlaying=false;
              bmPlayChanged=true;
              bmVsDirty=true;bmInfoDirty=true;bmGridDirty=true;bmPadsDirty=true;
            }
          }
        }
        // Pads 1-5 release: only select beat on short tap (< 200ms)
        // Long press = drum editing only — do not change the pattern
        if(i<NUM_DRUMS&&!bmPChord[i]){
          uint32_t holdMs=now-bmPDown[i];
          // Always redraw pad cell and bars on release so highlight clears
          bmBarsDirty=true; bmPadsDirty=true;
          // Short tap selects beat pattern for pads 0-7, long hold = drum edit
          if(holdMs<200 && !bmFuncMode && i<NUM_DRUMS){
            bmStoredBeat=i; bmApplyBeat();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true; bmDStripDirty=true;
          }
        }
        // Save/load release
        if((i==2||i==3||i==4||i==5)&&bmSaveSlotPending==(uint8_t)(i-2)){
          bmSlotProgressShow=false;
          if(!bmPLong[i]){
            uint8_t slot=i-2;
            // Drum mode load: drums only — never touches acid slots.
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
            bmStoredBeat=i; bmApplyBeat();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true;
          }
        }
        // FILL slot: SNARE/KIT tiles (pads 7/8) act on release so the
        // FUNC-exit chord takes priority. Apply only on a short solo tap
        // with no chord — if the chord fired, bmPChord[i] is set and func
        // has already toggled, so this is skipped.
        if((i==BM_PAD_FUNC_A||i==BM_PAD_FUNC_B) && bmFuncMode && bmFuncSel==7 && !bmPChord[i]){
          if((now-bmPDown[i])<400){
            bmFillType = i-4;
            bmVsDirty=true; bmBarsDirty=true; bmDStripDirty=true; bmInfoDirty=true;
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
          // Drum mode save: drums only — never touches acid slots.
          bmSavePatch(slot);
          // Defer the actual flash write to core 1 (see loop1()'s
          // saveCommit check) — calling EEPROM.commit() directly here
          // runs it inside the audio-critical core-0 control path.
          saveCommit = true;
          bmSlotOverlay=true;bmSlotOverlaySave=true;bmSlotOverlayEmpty=false;
          bmSlotOverlaySlot=slot;bmSlotOverlayMs=now;
          bmInfoDirty=true;bmDotsDirty=true;
        }
      }
    }
  }

  // Clear all saved DRUM slots: hold pads 3+4+5+6 (indices 2-5) simultaneously
  // for 1 second, WITHOUT the FUNC pads (7+8) held — that combo is reserved
  // for save-to-slot above. Only wipes drum patches; acid patches saved in the
  // same slot numbers are untouched. Mirrors the equivalent acid-mode clear-all
  // in Acid_Drip_V4.ino, which only wipes acid slots.
  if(bmPState[2]&&bmPState[3]&&bmPState[4]&&bmPState[5]&&
     !(bmPState[BM_PAD_FUNC_A]&&bmPState[BM_PAD_FUNC_B])&&
     !bmPLong[2]&&(now-bmPDown[2])>1000){
    bmPLong[2]=true;
    uint8_t invalid=0x00;
    for(uint8_t s=0;s<BM_NUM_SLOTS;s++){
      EEPROM.put(BM_SLOT_ADDR(s),invalid);
      bmSlotHasData[s]=false;
    }
    // Defer the actual flash write to core 1 (see loop1()'s saveCommit
    // check) — this function runs on core 0, inside the audio-critical
    // Mozzi control path, and calling EEPROM.commit() directly from here
    // was corrupting/crashing the display (white screen). Same reason the
    // confirmation message below is a dirty flag, not a direct tft.* call.
    saveCommit = true;
    bmDotsDirty=true;
    bmClearAllMsg=true;
    bmClearAllFlashUntil = now + 800;  // suppress bmVsDirty redraw briefly
  }
}

// ── updateAudio ───────────────────────────────────────────────────────
// bmDrumVol[d] is a continuous mix scalar applied every sample.
// 0=silent, 255=full. No velocity gating.
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
  // Patterns are 32 steps (2 bars); grid shows 16 — display the bar
  // containing the playhead (bar 0 when stopped).
  byte cur16=bmPlaying?(bmStepNum/6)%16:255;
  byte bar  =bmPlaying?(byte)((bmStepNum/96)%2):0;
  for(byte t=0;t<NUM_DRUMS;t++){
    for(byte s=0;s<16;s++){
      byte bb=pgm_read_byte(&beats[bmParamBeat][t][bar*2+s/8]);
      bool on=bitRead(bb,7-(s%8));
      bool fl=bmFired[t]&&(millis()-bmFiredMs[t]<BM_FLASH_MS);
      bool hd=(s==cur16);
      uint16_t col=hd?C_WHT:(fl&&on?C_WHT:(on?bmDrumCols[t]:0x0861));
      tft.fillRect(4+s*19,BM_GRID_Y+t*2,18,2,col);
    }
  }
}

// Draws only the FUNC value field (clears just that region) so the label,
// border and [CUT live] indicator don't flicker when the value updates.
void bmDrawFuncValue(){
  tft.fillRect(34,BM_VS_Y+1,84,BM_VS_H-2,0x1000);
  tft.setTextSize(1);
  tft.setTextColor(C_WHT); tft.setCursor(36,BM_VS_Y+6);
  switch(bmFuncSel){
    case 0:{int st=((int)bmStoredPitch-128)*12/64;
            if(st>0)tft.print("+"); tft.print(st); tft.print("st");}break;
    case 1:tft.print(((uint16_t)bmStoredDrive*100)/255);tft.print("%");
           if(bmCrushBits){tft.setTextColor(C_MGR);tft.print(" +CRU");} break;
    case 2:{ if(bmFiltMode==1){tft.print("LP ");tft.print((121-(int)bmStoredFilter)*100/121);tft.print("%");}
             else if(bmFiltMode==2){tft.print("HP ");tft.print(((int)bmStoredFilter-134)*100/121);tft.print("%");}
             else tft.print("BYPASS"); } break;
    case 3:{ int c=(int)bmStoredChance-128;
             if(c>2){tft.print("GHOST ");tft.print(c*100/127);tft.print("%");}
             else if(c<-2){tft.print("THIN ");tft.print((-c)*100/128);tft.print("%");}
             else tft.print("AS-IS"); }break;
    case 4:tft.print(bmStoredHumanize);
           tft.setTextColor(C_GRN); tft.print("  V:"); tft.print(bmLastVel); break;
    case 5:tft.print(((uint16_t)bmStoredSwing*60)/255);tft.print("% SWING");break;
    case 6:{ int a=(int)bmStoredAccent-128;
             if(a>2){tft.print("ON ");tft.print(a*100/127);tft.print("%");}
             else if(a<-2){tft.print("OFF ");tft.print((-a)*100/128);tft.print("%");}
             else tft.print("FLAT"); }break;
    case 7:{
        const char* fm[4]={"OFF","1 BAR","2 BAR","4 BAR"};
        const char* ft[4]={"HATS","CLAP","SNARE","KIT"};
        tft.print(fm[bmFillMode]);
        if(bmFillMode){ tft.setTextColor(C_CYN); tft.print("  "); tft.print(ft[bmFillType]); }
      }break;
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
      tft.print("FUNC: press pad 9-16"); return;
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
    // Fill indicator
    if(bmFillMode){
      const char* fmS[4]={"","F1","F2","F4"};
      tft.setTextColor(C_MGR); tft.setCursor(196,cy);
      tft.print(fmS[bmFillMode]);
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
  bmDrawFuncValue();
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

// Clear-all confirmation — called only from bmDoDraw() (core 1). Do not
// call this directly from bmUpdateControl() (core 0); see bmClearAllMsg.
void bmDrawClearAllMsg(){
  tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,C_BG);
  tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_DGR);
  tft.setTextSize(1); tft.setTextColor(C_DGR);
  tft.setCursor(4,BM_VS_Y+6); tft.print("DRUM SLOTS CLEARED");
}

void bmDrawSlotOverlay(){
  if(!bmSlotOverlay) return;
  if((millis()-bmSlotOverlayMs)>1400){bmSlotOverlay=false;bmVsDirty=true;bmDotsDirty=true;return;}
  tft.setTextSize(1);
  if(bmSlotOverlayEmpty){
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,C_BG); tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,C_DGR);
    tft.setTextColor(C_DGR); tft.setCursor(4,BM_VS_Y+6);
    tft.print("SLOT "); tft.print(bmSlotOverlaySlot+1); tft.print("  -  EMPTY");
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
    if(bmFuncSel==7){
      // FILL: show the fill voice's own (locked) PCH/LEN/VOL
      byte d = (bmFillType==0)?1 : (bmFillType==1)?6 : (bmFillType==2)?2 : 4;   // hat/clap/snare/tom
      uint16_t dc=bmDrumCols[d];
      tft.fillRect(0,BM_BAR_Y,36,BM_BAR_BH*3+BM_BAR_P*2+2,C_BG);
      tft.setTextColor(dc);
      tft.setCursor(2,BM_BAR_Y+1);            tft.print("PCH");
      tft.setCursor(2,BM_BAR_Y+BM_BAR_P+1);   tft.print("LEN");
      tft.setCursor(2,BM_BAR_Y+BM_BAR_P*2+1); tft.print("VOL");
      bmDrawBarBorders();
      tft.fillRect(214,BM_BAR_Y,SW-214,BM_BAR_BH*3+BM_BAR_P*2+2,C_BG);
      int pw=constrain((int)(bmFillPitch[d]*BM_FW)/255,0,BM_FW);
      int lw=constrain((int)(bmFillLen[d]  *BM_FW)/255,0,BM_FW);
      int vw=constrain((int)(bmFillVol[d]  *BM_FW)/255,0,BM_FW);
      bmDrawBarFill(BM_BAR_Y+1,           pw,BM_FW,C_YEL);
      bmDrawBarFill(BM_BAR_Y+BM_BAR_P+1,  lw,BM_FW,C_GRN);
      bmDrawBarFill(BM_BAR_Y+BM_BAR_P*2+1,vw,BM_FW,C_CYN);
      tft.setTextColor(C_YEL); tft.setCursor(216,BM_BAR_Y+1);            tft.print(bmFillPitch[d]);
      tft.setTextColor(C_GRN); tft.setCursor(216,BM_BAR_Y+BM_BAR_P+1);   tft.print(bmFillLen[d]);
      tft.setTextColor(C_CYN); tft.setCursor(216,BM_BAR_Y+BM_BAR_P*2+1); tft.print(bmFillVol[d]);
    } else {
    int cw=constrain((int)(bmRawCut*BM_FW)/1023,0,BM_FW);
    bmDrawBarFill(BM_BAR_Y+1,cw,BM_FW,bmFuncSel==7?C_DGR:(bmPotLocked?C_DGR:C_YEL));
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P+1,0,BM_FW,C_BG);
    bmDrawBarFill(BM_BAR_Y+BM_BAR_P*2+1,0,BM_FW,C_BG);
    // Clear the area right of the bar (used to show a redundant LCK/OK label)
    tft.fillRect(212,BM_BAR_Y,SW-212,BM_BAR_BH*3+BM_BAR_P*2,C_BG);
    }
    return;
  }

  // Normal: PCH/LEN/VOL for the selected drum
  // Static vars — labels/borders drawn once on drum change, fills only when value changes
  static byte prevDrum=255, prevPch=255, prevLen=255, prevVol=255;
  static bool bordersDrawn=false;
  if(bmBarsForce){ prevDrum=255; prevPch=255; prevLen=255; prevVol=255; bordersDrawn=false; bmBarsForce=false; }
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
  // ── FILL selected: tiles column-aligned with the pads below, so
  //    tile N = pad N. Pads 1-4 = frequency, pads 5-8 = type. ──
  if(bmFuncMode && bmFuncSel==7){
    const char* freqLbl[4]={"OFF","1BR","2BR","4BR"};
    const char* typeLbl[4]={"HATS","CLAP","SNAR","KIT"};
    for(byte t=0;t<8;t++){
      int bx=BM_PAD_SX+t*(BM_PAD_BW+BM_PAD_SP);
      bool sel = (t<4) ? (bmFillMode==t) : (bmFillMode && bmFillType==(t-4));
      uint16_t bg=sel?C_ORG:(t<4?0x000F:0x0800);
      uint16_t bd=sel?C_YEL:(t<4?C_BLU:C_GRN);
      uint16_t tx=sel?C_BG :(t<4?C_CYN:C_GRN);
      tft.fillRect(bx,BM_DS_Y,TW,BM_DS_H,bg);
      tft.drawRect(bx,BM_DS_Y,TW,BM_DS_H,bd);
      const char* lbl = (t<4)?freqLbl[t]:typeLbl[t-4];
      int fw=strlen(lbl)*6;
      tft.setTextColor(tx);
      tft.setCursor(bx+(TW-fw)/2, BM_DS_Y+4); tft.print(lbl);
    }
    return;
  }
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
    // Mod dots: yellow=pitch  green=crop  cyan=vol  (dim if at reset baseline)
    bool pitchMod=(bmDrumPitch[d]!=128);
    bool cropMod =(bmDrumLen[d] !=191);
    bool volMod  =(bmDrumVol[d]  !=191);
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
  extern bool syncMode;
  // Single row: beat name (size 1) + flags on same line, freeing vertical space
  tft.fillRect(0,BM_IS_Y+14,SW,SH-BM_IS_Y-14,C_BG);
  const int ty=BM_IS_Y+20;
  // Beat name — or, when in a FUNC slot, the full function name instead
  tft.setTextSize(2);
  if(bmFuncMode && bmFuncSel!=255){
    tft.setTextColor(C_ORG);
    tft.setCursor(4,ty); tft.print(bmFuncFullNames[bmFuncSel]);
  } else {
    tft.setTextColor(bmPlaying?C_GRN:C_CYN);
    tft.setCursor(4,ty); tft.print(bmBeatNames[bmParamBeat]);
  }
  // Flags right-aligned on same row
  tft.setTextSize(1);
  int fx=180;  // flags start x
  if(bmLastLoadedSlot>=0){tft.setTextColor(C_CYN);tft.setCursor(fx,ty+4);tft.print("S");tft.print(bmLastLoadedSlot+1);}
  // Sync-in mode indicator — boot-time selection, GP2 is a clock input
  if(syncMode){tft.setTextColor(C_CYN);tft.setCursor(290,ty+4);tft.print("SYNC");}
}

void bmDrawInfoStrip(){
  tft.drawFastHLine(0,BM_IS_Y,SW,C_DGR);
  bmDrawDrumFlash(); bmDrawInfoBody();
}

void bmDrawMain(){
  bmBarsForce=true;   // re-entry: force PCH/LEN/VOL bars + values to repaint
  tft.startWrite();
  tft.fillScreen(C_BG);
  bmFillDrumBuffer();
  bmDrawSlotDots(); bmDrawGrid(); bmDrawValStrip();
  bmFillDrumBuffer();
  for(byte i=0;i<16;i++) {
    bmDrawPadCell(i);
    if ((i & 3) == 3) bmFillDrumBuffer();
  }
  bmDrawFuncLabels();
  bmFillDrumBuffer();
  bmDrawBarLabels(); bmDrawBarBorders(); bmDrawBars();
  bmDrawDrumStrip();
  bmFillDrumBuffer();
  bmDrawInfoStrip();
  bmFillDrumBuffer();
  tft.endWrite();
}

void bmDoDraw(){
  if(bmFullDirty)       {bmFullDirty=false;  bmDrawMain();     return;}
  if(bmSlotProgressShow){bmDrawSaveProgress();               return;}
  if(bmSlotOverlay)     {bmDrawSlotOverlay();                return;}
  if(bmClearAllMsg)     {bmClearAllMsg=false; bmDrawClearAllMsg(); return;}
  // Suppress value-strip redraws briefly after clear-all so the confirmation
  // message stays visible instead of being immediately overwritten by the
  // next pot/edit-triggered bmVsDirty.
  if(bmClearAllFlashUntil && (long)(millis()-bmClearAllFlashUntil)<0){
    bmVsDirty=false; bmVsValDirty=false;
  } else {
    bmClearAllFlashUntil=0;
  }
  if(bmBarsDirty)       {bmBarsDirty=false;  bmDrawBars();      return;}  // first — pot feedback must feel instant
  if(bmVsDirty)         {bmVsDirty=false; bmVsValDirty=false; bmDrawValStrip();  return;}
  if(bmVsValDirty)      {bmVsValDirty=false;
                          if(bmFuncMode&&bmFuncSel!=255){ bmDrawFuncValue(); return; }
                          // Normal mode: only repaint the P/L/V number fields (not the whole strip)
                          // to prevent the drum name / BPM / play state from flickering.
                          if(!bmFuncMode){
                            byte d=bmDrumEditPad;
                            const int cy=BM_VS_Y+6;
                            // Clear and repaint only the three value fields
                            tft.fillRect(76,cy,140,8,0x0008);   // covers P:xxx L:xxx V:xxx
                            tft.setTextSize(1);
                            tft.setTextColor(C_YEL); tft.setCursor(76,cy);
                            tft.print("P:"); tft.print(bmDrumPitch[d]);
                            tft.setTextColor(C_GRN); tft.setCursor(116,cy);
                            tft.print("L:"); tft.print(bmDrumLen[d]);
                            tft.setTextColor(C_CYN); tft.setCursor(156,cy);
                            tft.print("V:"); tft.print(bmDrumVol[d]);
                          }
                          return;}
  // Lightweight mod-dot update: repaint only the 3 mod dots for the active drum.
  // Much cheaper than bmDStripDirty (which clears+redraws all 8 tiles).
  if(bmDotsDirtyDrum)   {bmDotsDirtyDrum=false;
                          byte d=bmDrumEditPad;
                          bool pitchMod=(bmDrumPitch[d]!=128);
                          bool cropMod =(bmDrumLen[d]  !=191);
                          bool volMod  =(bmDrumVol[d]  !=191);
                          int bx=BM_PAD_SX+d*(BM_PAD_BW+BM_PAD_SP);
                          int dotY=BM_DS_Y+BM_DS_H-4, dotX=bx+BM_PAD_BW/2-8;
                          tft.fillRect(dotX,   dotY,3,3,pitchMod?C_YEL:0x0821);
                          tft.fillRect(dotX+5, dotY,3,3,cropMod ?C_GRN:0x0821);
                          tft.fillRect(dotX+10,dotY,3,3,volMod  ?C_CYN:0x0821);
                          return;}
  if(bmGridDirty)       {bmGridDirty=false;  bmDrawGrid();      return;}
  if(bmPadsDirty)       {bmPadsDirty=false;  for(byte i=0;i<16;i++) bmDrawPadCell(i); bmDrawFuncLabels(); return;}
  if(bmDotsDirty)       {bmDotsDirty=false;  bmDrawSlotDots();  return;}
  if(bmDStripDirty)     {bmDStripDirty=false; bmDotsDirtyDrum=false; bmDrawDrumStrip(); return;}
  if(bmInfoDirty)       {bmInfoDirty=false;  bmDrawInfoBody(); return;}
  if(bmDrumsDirty)      {bmDrumsDirty=false; bmDrawDrumFlash();    return;}
}

// Called from Acid_Drip_V4 when drum mode is first activated
// ── bmFillDrumBuffer: called from loop1() on core 1 ─────────────────
// Fills GP2 PWMAudio buffer with drum samples.
// Running on core 1 means zero conflict with Mozzi audio ISR on core 0.
// ── Soft clipper: linear to 75% FS, gentle 4:1 knee above ────────────
// Replaces the old hard constrain. Only transient peak collisions
// (e.g. kick+snare on the backbeat) touch the knee — steady content
// passes through untouched. Integer-only, ~3 ops, safe in the DMA path.
static inline int32_t bmSoftClip(int32_t x){
  if(x >  24576){ x =  24576 + ((x - 24576) >> 2); if(x >  32600) x =  32600; }
  else if(x < -24576){ x = -24576 + ((x + 24576) >> 2); if(x < -32600) x = -32600; }
  return x;
}

// Drive saturator: a deliberately HARD knee (much lower + steeper than the
// peak limiter) so it generates audible harmonics — grit, not just level.
// Knee 8000, 2:1 above, bounded near the clean ceiling so drive doesn't just
// get louder. Pre-gain pushes the signal into this curve.
static inline int32_t bmDriveSat(int32_t x){
  const int32_t K=8000, C=24000;
  if(x >  K){ x =  K + ((x - K) >> 1); if(x >  C) x =  C; }
  else if(x < -K){ x = -K + ((x + K) >> 1); if(x < -C) x = -C; }
  return x;
}

void bmFillDrumBuffer(){
  if(!bmAlarmStarted) return;
  while(bmPWM.availableForWrite()){
    int32_t mix = 0;
    if(bmPlaying){
      // Each voice: (envelope gain 8.8 → 8-bit) * sample(±127), then
      // the envelope decays by its per-sample multiplier. env==0 = done.
      #define BM_VOICE(N) if(bmTriggered[N] && bmEnv[N]){ \
        mix += (int32_t)(bmEnv[N]>>8)*(int8_t)bms##N.next(); \
        bmEnv[N]=(uint16_t)(((uint32_t)bmEnv[N]*bmEnvM[N])>>16); }
      BM_VOICE(0) BM_VOICE(1) BM_VOICE(2) BM_VOICE(3)
      BM_VOICE(4) BM_VOICE(5) BM_VOICE(6)
      if(bmTriggered[7] && bmEnv[7]){
        mix += (int32_t)(bmEnv[7]>>8)*(int8_t)bms7.next();
        // Hat choke: closed hat forces a ~6ms decay regardless of LEN
        uint16_t m7 = bmHatChoke ? (uint16_t)64900 : bmEnvM[7];
        bmEnv[7]=(uint16_t)(((uint32_t)bmEnv[7]*m7)>>16);
      }
      #undef BM_VOICE
      // Drive: pre-gain pushes the bus into a hard saturation curve (audible
      // grit, not just level), then the top of the knob folds in bit crush.
      if(bmStoredDrive){
        mix = ((int32_t)mix * bmDriveGainQ8) >> 8;          // pre-gain
        mix = bmDriveSat(mix);                              // hard saturate
        if(bmCrushBits) mix = (mix >> bmCrushBits) << bmCrushBits;  // crush tip
      }
      // Bipolar DJ filter (TPT state-variable). Runs every sample so the
      // integrator states stay coherent; the knob selects which output to use
      // (dry at center, lowpass below, highpass above). Resonance via bmFiltK.
      {
        int32_t v3 = mix - bmFiltIc2;
        int32_t v1 = (int32_t)(((int64_t)bmFiltA1*bmFiltIc1 + (int64_t)bmFiltA2*v3) >> 16);
        int32_t v2 = bmFiltIc2 + (int32_t)(((int64_t)bmFiltA2*bmFiltIc1 + (int64_t)bmFiltA3*v3) >> 16);
        bmFiltIc1 = 2*v1 - bmFiltIc1;
        bmFiltIc2 = 2*v2 - bmFiltIc2;
        if(bmFiltIc1 >  (1<<22)) bmFiltIc1= (1<<22); else if(bmFiltIc1 < -(1<<22)) bmFiltIc1=-(1<<22);
        if(bmFiltIc2 >  (1<<22)) bmFiltIc2= (1<<22); else if(bmFiltIc2 < -(1<<22)) bmFiltIc2=-(1<<22);
        if(bmFiltMode==1)      mix = v2;                                              // lowpass
        else if(bmFiltMode==2) mix = mix - (int32_t)(((int64_t)bmFiltK*v1)>>16) - v2; // highpass
      }
      mix = bmSoftClip(mix);   // final peak safety
    }
    // Full 16-bit scale — compensates for higher GP2 resistor (22K)
    bmPWM.write((int16_t)mix, false);
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
