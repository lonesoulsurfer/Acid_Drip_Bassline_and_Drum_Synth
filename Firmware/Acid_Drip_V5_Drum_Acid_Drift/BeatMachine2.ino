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
// Combined save/load — defined in Acid_Drip_V5.ino. The save/load gesture
// here in drum mode calls these instead of bmSavePatch()/bmLoadPatch()
// directly, so a save from the drum screen also captures acid + DRIFT
// (and vice versa) into the same slot. See the COMBINED SAVE / LOAD
// comment in Acid_Drip_V5.ino for the reasoning.
extern void saveAllToSlot(uint8_t slot);
extern void loadAllFromSlot(uint8_t slot);
extern bool comboSlotHasData(uint8_t slot);
// MIX EDIT trim/boost — Q8 gain (256=unity, up to ~384=1.5x boost), set via
// the CUT/RES/DECAY pots in Acid_Drip_V5.ino's mixEditMode and recomputed
// there by recomputeMixGains(). Applied here to DRIFT and the drum bus
// right where they're mixed together (see bmFillDrumBufferTo).
extern bool     mixEditMode;   // MIX EDIT borrows CUT/RES/DECAY — suspend drum pot edits
extern uint16_t mixDriftGainQ8;
extern uint16_t mixDrumGainQ8;
// DRIFT (second synth voice) lives on THIS output — GP2 — additively
// summed with drums below (bmFillDrumBufferTo()), blended against acid on
// GP15 at the PCB's shared audio node by the physical mix pot. It briefly
// rendered on GP15 instead, mixed digitally with acid in software; moved
// back so the physical pot has a real, separate DRIFT signal to blend —
// see the DRIFT OUTPUT comment near ch2SynthMode's declaration in the
// main sketch for the full reasoning.
extern uint8_t  ch2Wave;
extern uint8_t  ch2Sound;                     // 0-7, selected engine — see CH2_SND_NAMES
extern uint8_t  ch2Amount;                    // 0-255, live per-engine character knob (DCY pot)
extern volatile uint32_t ch2Phase;
extern volatile uint32_t ch2PhaseInc;
extern volatile uint16_t ch2Env;
extern volatile uint16_t ch2EnvM;
extern volatile uint32_t ch2DetuneRatioQ16;   // Q16 second-voice ratio — DTSQR + UNISN
extern volatile uint8_t  ch2ClickCount;       // CLICK: noise-burst samples remaining
extern volatile uint8_t  ch2NoiseIdx;         // CLICK: running index into noisetable[]
extern int16_t  ch2EchoBuf[];                 // ECHO delay line (CH2_ECHO_LEN samples, power of 2)
extern volatile uint16_t ch2EchoDelay;        // ECHO delay in samples, 0 = off
extern volatile uint8_t  ch2EchoFb;           // ECHO feedback, Q8
extern Ch2VerbBufs ch2VerbBufs;               // VERB comb/allpass delay lines (~4.9KB)
extern volatile uint8_t  ch2VerbFbQ8;         // VERB comb feedback (room size), Q8
extern volatile uint8_t  ch2VerbWetQ8;        // VERB wet mix, Q8; 0 = off
extern const uint8_t sinetable[256];
extern const uint8_t noisetable[64];
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

// ── CUSTOM pattern editing ────────────────────────────────────────────
// Pattern index 15 ("JACK BEAT") is repurposed as CUSTOM: a RAM-backed,
// user-editable pattern instead of a PROGMEM preset. Every read of a
// pattern byte (trigger step + grid draw) goes through bmBeatByte() below,
// which branches to this buffer for BM_CUSTOM_IDX and to beats[] PROGMEM
// otherwise — nothing else in the playback/draw path needs to know which
// backing store it's reading from.
// Only steps 1-15 (pad 16 is voice-cycle/exit) are addressable in v1, and
// each edit mirrors into both bar copies (bytes 0-1 and 2-3) so a CUSTOM
// pattern is really a 16-step loop repeated twice per the existing 32-step
// structure — no changes needed to the playback engine's step math.
#define BM_CUSTOM_IDX 15
byte     bmCustomBeat[NUM_DRUMS][4] = {};   // CUSTOM pattern, blank until edited
bool     bmPatternEditMode = false;
uint8_t  bmEditVoice        = 0;            // which drum row CUSTOM edit currently targets

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
uint32_t bmSwingNominalUs = 0;     // true on-grid micros() time the pending swung step was armed at
// How many microseconds late the CURRENT bmFireDrumStep() call is, relative to
// its own nominal (on-grid) step time — 0 for a straight step, up to ~60% of
// the interval for a swung one. Set immediately before every bmFireDrumStep()
// call; bmFireHit() reads it to size the HMNZ jitter budget so swing + HMNZ
// can never together push a hit past the next step's nominal boundary (see
// bmFireHit()'s cap calculation).
uint32_t bmStepLatencyUs = 0;

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
bool     bmP11Deferred = false;  // pad 11 (index 10) pressed while 9/10 held —
                                  // beat-select deferred to release; see the
                                  // pad-scan press/release handlers below and
                                  // fireLayerChord() in Acid_Drip_V5.ino.
// Last pad-16 hold tier shown on the live legend (see bmP16Tier()) — 255
// means "not currently held", forcing a repaint the moment it is. Lets
// bmUpdateControl() only mark the label strip dirty when the tier that
// would fire on release actually CHANGES, instead of every control pass.
uint8_t  bmP16TierShown=255;

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

// CUSTOM pattern (bmCustomBeat) auto-persists on its own, separate from the
// 4 BmPatch slots above — one shared buffer, saved automatically whenever
// you exit pattern-edit mode (long-hold pad 16), not tied to a save-slot
// choice. Lives at the tail of EEPROM_SIZE (Acid_Drip_V5.ino), which was
// bumped from 1024 to 1064 to make room — see the comment there. Placed at
// the OLD boundary deliberately: everything below 1024 is already claimed
// by other structs in that tab, so this is the only address guaranteed
// free without auditing every other region's exact size.
#define BM_CUSTOM_ADDR 1024
struct BmCustomPersist { uint8_t valid; byte custom[NUM_DRUMS][4]; };
static_assert(sizeof(BmCustomPersist) <= 40, "BmCustomPersist grew past its reserved 40 bytes at BM_CUSTOM_ADDR — bump EEPROM_SIZE in Acid_Drip_V5.ino too");

struct BmPatch {
  uint8_t valid;
  byte    beat, crush, filter, chance, accent, pitch;
  byte    fillMode, fillType, swing, humanize;
  uint8_t tempoByte;
  byte    dPitch[NUM_DRUMS], dLen[NUM_DRUMS], dVol[NUM_DRUMS];
};
// Acid_Drip_V4.ino uses the literal value 36 when clearing BM slots — keep in sync.
// CUSTOM pattern data (bmCustomBeat) is intentionally NOT stored here — see
// BM_CUSTOM_ADDR below. It was briefly added as a `custom[NUM_DRUMS][4]` field
// (grew this struct to 68 bytes), which pushed BM_SLOT_ADDR(3)'s end from 400
// to 528 — directly overrunning CH2X_ADDR (400, DRIFT channel-2 extended
// settings) in the acid tab. Reverted. Do not grow this struct without
// re-checking the full EEPROM address map in Acid_Drip_V5.ino first — the
// region right after it (400+) is claimed by another struct, not free space.
static_assert(sizeof(BmPatch) == 36, "BmPatch size changed — update clear-all in Acid_Drip_V4.ino, and re-check the EEPROM map in Acid_Drip_V5.ino before growing this");

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
  "DRUM N BASS","VOODOO RAY", "BUILD DOWN", "CUSTOM",
};
const char* bmBeatShort[16] = {
  "BASC","HOUS","TECH","ACID",
  "HSOC","FUNK","SWNG","BKBT",
  "MINM","LATN","PCFC","BLMN",
  "DNBT","VOOD","GBLD","CUST",
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

// Single point every pattern-byte read goes through. Branches to the RAM
// CUSTOM buffer for BM_CUSTOM_IDX, PROGMEM beats[] otherwise. See the
// CUSTOM pattern editing comment block above (near NUM_DRUMS) for context.
inline byte bmBeatByte(byte pattern, byte voice, byte idx){
  if(pattern==BM_CUSTOM_IDX) return bmCustomBeat[voice][idx];
  return pgm_read_byte(&beats[pattern][voice][idx]);
}

// Toggles one step of the CUSTOM pattern for the given voice, mirrors it
// into both bar copies, and auditions on ON. Shared by pad 16's short-tap
// (step 16) and the pads-1-15 step-toggle block below.
inline void bmToggleCustomStep(byte voice, byte step){
  byte byteIdx=step/8, bitPos=(byte)(7-(step%8));
  bmCustomBeat[voice][byteIdx]   ^= (byte)(1<<bitPos);
  bmCustomBeat[voice][byteIdx+2] ^= (byte)(1<<bitPos);   // mirror into bar 2
  if(bitRead(bmCustomBeat[voice][byteIdx],bitPos)){
    bmStartVoice(voice,bmDrumVol[voice]);                // audition on ON
  }
}

// Which pad-16 hold tier holdMs currently sits in, given whether pattern
// edit mode is already active. Single source of truth for both the
// release handler (which ACTS on this) and bmDrawFuncLabels() (which
// PREVIEWS it live while the pad is still held) — keep the thresholds
// here in sync with the release handler's comment block if they change.
//   0 = TAP  (STEP if editing, ENTER if not)
//   1 = VOICE   (only reachable while already editing)
//   2 = EXIT+SAVE (only reachable while already editing)
//   3 = CLEAR
// Each tier now gets a fixed 500ms dwell, and the whole 4-tier, 2000ms
// sequence REPEATS for as long as the pad stays down — so CLEAR lights
// up at 1.5s same as any other tier, then it cycles back to TAP/STEP at
// 2s and keeps going around for as long as you hold, rather than
// latching on CLEAR forever past the first pass. That also gives VOICE
// its own full 500ms window to release into instead of a narrow gap.
inline uint8_t bmP16Tier(uint32_t holdMs, bool editing){
  uint8_t slot = (uint8_t)((holdMs % 2000UL) / 500UL);  // 0..3, repeating
  if(!editing && (slot==1 || slot==2)) return 0;  // VOICE/EXIT+SAVE only apply once already editing
  return slot;
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
  bmLoadCustom();
}

// CUSTOM pattern auto-persistence — separate from the 4 patch slots above.
// bmSaveCustom() is called on exiting pattern-edit mode (see the pad-16
// release handler); bmLoadCustom() is called once at boot via bmCheckSlots
// (itself called from bmInit()). EEPROM.commit() is deferred to loop1() via
// the shared saveCommit flag, same as the patch-slot save — writing here
// only queues the flash write, it doesn't block the audio path.
void bmSaveCustom(){
  BmCustomPersist c;
  c.valid=BM_PATCH_VALID;
  for(byte d=0;d<NUM_DRUMS;d++) for(byte b=0;b<4;b++) c.custom[d][b]=bmCustomBeat[d][b];
  EEPROM.put(BM_CUSTOM_ADDR,c);
  saveCommit=true;
}
void bmLoadCustom(){
  BmCustomPersist c; EEPROM.get(BM_CUSTOM_ADDR,c);
  if(c.valid!=BM_PATCH_VALID) return;   // never saved yet — leave bmCustomBeat blank
  for(byte d=0;d<NUM_DRUMS;d++) for(byte b=0;b<4;b++) bmCustomBeat[d][b]=c.custom[d][b];
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
    // Cap jitter to 1/3 of the step so it lands inside its own step, at any
    // tempo — but a swung step has ALREADY spent part of that step's
    // interval (bmStepLatencyUs, up to ~60%) before we even get here. Size
    // the remaining budget off what's left of the interval (minus a small
    // safety margin) rather than a flat 1/3, so swing + HMNZ can't stack
    // past the next step's nominal time and force the flush-collision in
    // bmTriggerStep() (two steps' hits landing on top of each other).
    uint32_t interval = seq.interval;
    uint32_t margin    = interval/10;                 // keep clear of the next step
    uint32_t spent     = bmStepLatencyUs + margin;
    uint32_t budget    = (spent < interval) ? (interval - spent) : 0;
    uint32_t cap = interval/3;
    if(cap > budget) cap = budget;
    uint32_t maxJit = bmHumanizeJitUs;
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
    byte bb=bmBeatByte(bmParamBeat,s,drumStep/8);
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
  // Flush any still-pending swung step first so positions never overlap.
  // It's firing late (relative to its own nominal time), not on the fresh
  // grid we're currently at — tell bmFireHit() how late via bmStepLatencyUs
  // so HMNZ jitter doesn't stack an additional cap's worth on top.
  if(bmSwingPending){
    bmSwingPending=false;
    bmStepLatencyUs = (uint32_t)(micros() - bmSwingNominalUs);
    bmFireDrumStep();
  }
  // Swing: delay the odd 16th steps (the "e" and "a") by up to ~60% of the
  // step interval. bmDrumPos is the step about to play (before it advances).
  if(bmStoredSwing && (bmDrumPos & 1)){
    uint16_t swingPct = ((uint16_t)bmStoredSwing * 60) / 255;   // 0..60 %
    bmSwingNominalUs = micros();                                // this step's true grid time
    bmSwingDueUs = bmSwingNominalUs + (((uint32_t)seq.interval * swingPct) / 100);
    bmSwingPending = true;
  } else {
    bmStepLatencyUs = 0;   // straight step, firing right on its own grid time
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
  bmP11Deferred = false;
  bmApplyParams();
  bmDStripDirty=true; bmFullDirty=true;
}

// ── updateControl ─────────────────────────────────────────────────────
void bmUpdateControl(){
  uint32_t now=millis();

  // Swing: fire the delayed odd step when its offset elapses
  if(bmSwingPending){
    if(!bmPlaying) bmSwingPending=false;
    else if((int32_t)(micros()-bmSwingDueUs) >= 0){
      bmSwingPending=false;
      bmStepLatencyUs = (uint32_t)(micros() - bmSwingNominalUs);
      bmFireDrumStep();
    }
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
  // The layer chord (pads 9+10, +11 for DRIFT) arms here or in the acid
  // file's own pad scan and FIRES exclusively in Acid_Drip_V5.ino's
  // updateControl() after LAYER_HOLD_MS — one fire handler, one decision
  // point (engine chosen at fire time by pad 11), no duplicated state
  // machines. This file only ARMS it from drum mode's own pad scan and
  // keeps this disarm-on-release safety net in case this updateControl
  // runs first in a scan cycle. layerArmed/layerArmMs are acid-file
  // globals, visible here because the .ino files are one translation unit.
  if(layerArmed&&(digitalRead(PAD_PINS[8])!=LOW||digitalRead(PAD_PINS[9])!=LOW)){
    layerArmed=false;
  }
  // Drum triggers now come from acid advanceStep() via bmTriggerStep()
  // No independent clock needed — zero drift guaranteed


  // ── UI: pads and pots — only active in drum mode ─────────────────
  // When in acid mode, pads and pots belong exclusively to acid synth.
  // Also suspended during MIX EDIT: that mode borrows the same three
  // pots to set per-engine levels, and it is now reachable from the
  // drum screen. Without this guard a single pot turn would set a mix
  // level AND scramble the selected drum's pitch/length/volume at once.
  if(!bmMode || mixEditMode){
    // Clear the "which drum am I editing" trackers on the way out. The
    // pots re-lock on every fresh grab (d != bmDrumEditPrev below), but
    // leaving drum mode with a pad still held strands that value — and
    // re-grabbing the same pad later would then skip the lock and let it
    // adopt whatever positions acid or DRIFT had left the pots in.
    // Clearing here means the next grab always re-locks, matching the
    // per-engine pot ownership the acid/DRIFT side now has.
    bmDrumEditPrev = 255;
    bmFillEditDrum = -1;
    return;
  }

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
  } else if(bmPatternEditMode){
    // CUSTOM edit: pots continuously control the currently selected edit
    // voice (bmEditVoice) — no pad hold needed, since every pad here is a
    // step toggle instead. Re-locks (pot pickup) whenever the target voice
    // changes via a voice-cycle tap on pad 16. Same lock/track mechanism
    // and shared bmDrumPot* state as the normal held-pad drum edit below —
    // safe to share since the two modes are mutually exclusive.
    byte d=bmEditVoice;
    if(d!=bmDrumEditPrev){
      bmDrumPotRef[0]=bmPotCut; bmDrumPotRef[1]=bmPotRes; bmDrumPotRef[2]=bmPotDcy;
      bmDrumPotLock[0]=bmDrumPotLock[1]=bmDrumPotLock[2]=true;
      bmDrumEditPrev=d;
    }
    static uint32_t bmEditPotMs=0;
    uint32_t nowE=millis();
    bool ch=false;
    if(bmDrumPotLock[0]){ if(abs((int)bmPotCut-(int)bmDrumPotRef[0])>6) bmDrumPotLock[0]=false; }
    if(!bmDrumPotLock[0] && abs((int)bmPotCut-(int)bmDrumPitch[d])>2){ bmDrumPitch[d]=bmPotCut; ch=true; }
    if(bmDrumPotLock[1]){ if(abs((int)bmPotRes-(int)bmDrumPotRef[1])>6) bmDrumPotLock[1]=false; }
    if(!bmDrumPotLock[1] && abs((int)bmPotRes-(int)bmDrumLen[d])>2){ bmDrumLen[d]=bmPotRes; ch=true; }
    if(bmDrumPotLock[2]){ if(abs((int)bmPotDcy-(int)bmDrumPotRef[2])>6) bmDrumPotLock[2]=false; }
    if(!bmDrumPotLock[2] && abs((int)bmPotDcy-(int)bmDrumVol[d])>2){ bmDrumVol[d]=bmPotDcy; bmGain[d]=bmPotDcy; bmEnv[d]=(uint16_t)bmPotDcy<<8; ch=true; }
    if(ch){
      bmBarsDirty=true; bmVsValDirty=true; bmDotsDirtyDrum=true;
      if((uint32_t)(nowE-bmEditPotMs) >= 15){ bmEditPotMs=nowE; bmApplyDrum(d); }
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
        if (i == 10) bmP11Deferred = false;  // fresh press — stale defer state dies here
                                              // (pad 11 — the DRIFT selector, LAYER_PAD_C
                                              // in the acid file; literal here to avoid
                                              // depending on cross-file #define ordering)

        // Play/stop chord: mark on press, act on release
        if((i==0&&bmPState[1])||(i==1&&bmPState[0])){bmPChord[0]=bmPChord[1]=true;continue;}
        if(bmPChord[i]) continue;

        // FUNC toggle: pads 7+8. Suppressed during pattern edit, where
        // these two pads double as step 7/8 toggles — see the CUSTOM
        // pattern editing block above.
        if(!bmPatternEditMode &&
           ((i==BM_PAD_FUNC_A&&bmPState[BM_PAD_FUNC_B]&&(now-bmPDown[BM_PAD_FUNC_B])<200)||
           (i==BM_PAD_FUNC_B&&bmPState[BM_PAD_FUNC_A]&&(now-bmPDown[BM_PAD_FUNC_A])<200))){
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

        if(bmPatternEditMode){
          // CUSTOM pattern editing claims all 16 pads as step toggles /
          // voice-cycle / exit. All of that fires on RELEASE (short-tap
          // vs long-hold), so press does nothing here — see the release
          // handler below.
        } else if(bmFuncMode){
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
          // Layer chord (pads 9+10, optionally +11) — arm on press, fire
          // after LAYER_HOLD_MS in the acid-side unified handler. From
          // drum mode: 9+10 exits to acid; 9+10+11 is a deliberate no-op
          // (see fireLayerChord()'s bmMode check) — too easy to land pad
          // 11 by accident on an imprecise 3-pad press and get bounced
          // into DRIFT instead of just leaving drums.
          if((i==8 && bmPState[9]) || (i==9 && bmPState[8])){
            bmPChord[8]=true; bmPChord[9]=true;
            layerArmed=true; layerArmMs=millis();
            continue;
          }
          // Pad 11 pressed while 9 or 10 is already held: a layer chord
          // may be forming, so DEFER — same idiom as pads 9/10 themselves
          // (which always defer; see the "always defer to release" note
          // below), applied to pad 11 too. Without this, pad 11's own
          // immediate beat-select fired the instant it landed — regardless
          // of whether 9/10 were mid-chord — which is exactly the "PACIFIC
          // ST turns on for free" symptom this is fixing. Resolution: if
          // the chord fires with 11 down, fireLayerChord() eats it
          // (bmPChord[10]); if it aborts, the release handler
          // replays the deferred select. Solo pad-11 presses (9/10 not
          // involved) never reach this branch — normal beat-select keeps
          // its immediate-feedback snap.
          else if (i == 10 && (bmPState[8] || bmPState[9])) {
            bmP11Deferred = true;
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
          // RESET suppressed during pattern edit — pads 1/2 double as step
          // 1/2 toggles there, and a stray >=1s co-hold shouldn't wipe the
          // device. Falls through to the ordinary start/stop toggle instead.
          if(holdMs>=1000 && !bmPatternEditMode){ bmDoReset(); }
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
        // Suppressed during pattern edit — see the step-toggle block below.
        if(i<NUM_DRUMS&&!bmPChord[i]&&!bmPatternEditMode){
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
            // Combined load: brings back acid + DRIFT + drums together,
            // whichever of the three actually have data in this slot.
            if(comboSlotHasData(slot)){loadAllFromSlot(slot);bmSlotOverlaySave=false;bmSlotOverlayEmpty=false;}
            else{bmSlotOverlaySave=false;bmSlotOverlayEmpty=true;}
            bmSlotOverlay=true;bmSlotOverlaySlot=i-2;bmSlotOverlayMs=now;
            bmInfoDirty=true;bmDotsDirty=true;
          }
          bmSaveSlotPending=255;
        }
        // Pads 8+9 released without chord — short tap = select pattern
        // Suppressed during pattern edit — see the step-toggle block below.
        if((i==8||i==9) && !bmPChord[i] && !bmPatternEditMode){
          uint32_t holdMs=now-bmPDown[i];
          if(holdMs<500 && i<NUM_BEATS && !bmFuncMode){
            bmStoredBeat=i; bmApplyBeat();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true;
          }
        }
        // Pad 11 released, its press having been DEFERRED (a layer chord
        // was forming when it landed — see the press handler above): if
        // the chord actually fired with pad 11 down, fireLayerChord()
        // already consumed it via bmPChord[10] and this is a no-op. If
        // the chord never fired (9/10 released early, or the 500ms hold
        // never elapsed before 11 came back up), replay the beat-select
        // pad 11's press would have done, so a genuine tap through a
        // near-miss chord attempt still works.
        if(i==10 && bmP11Deferred && !bmPChord[i] && !bmPatternEditMode){
          bmP11Deferred = false;
          if(!bmFuncMode){
            bmStoredBeat=10; bmApplyBeat();
            bmPadsDirty=true; bmInfoDirty=true; bmGridDirty=true;
          }
        }

        // ── CUSTOM pattern edit: pad 16 is step 16 + voice-cycle + entry/exit + clear ──
        // Duration tiers on the same pad, same idiom as RESET vs start/stop
        // elsewhere in this file. Tiers now come from bmP16Tier(), which
        // gives each one a 500ms window and repeats the whole 4-tier
        // sequence for as long as the pad is held — so release just reads
        // off whichever tier is current, same value bmDrawFuncLabels() was
        // already previewing live. Keep bmP16Tier() in sync with this if
        // the tiers themselves ever change.
        //   tier 0, not editing: ENTER edit mode.
        //   tier 0, editing:     toggle step 16 — same as every other step
        //     pad, so all 16 pads behave uniformly on a quick tap.
        //   tier 1 (~0.5-1s), editing: cycle edit voice.
        //   tier 2 (~1-1.5s), editing: EXIT edit mode. Auto-persists the
        //     pattern (bmSaveCustom) — no separate save gesture needed.
        //   tier 3 (~1.5-2s), either state: wipe the whole CUSTOM pattern
        //     and (re-)enter a blank editor. Keep holding past 2s and the
        //     cycle repeats — CLEAR comes back around every 2s rather than
        //     latching once you overshoot it.
        if(i==BM_CUSTOM_IDX && !bmPChord[i]){
          uint32_t holdMs=now-bmPDown[i];
          uint8_t tier=bmP16Tier(holdMs, bmPatternEditMode);
          if(tier==3){
            for(byte d=0;d<NUM_DRUMS;d++) for(byte b=0;b<4;b++) bmCustomBeat[d][b]=0;
            bmPatternEditMode=true; bmEditVoice=0; bmDrumEditPad=0;
            bmStoredBeat=BM_CUSTOM_IDX; bmApplyBeat();
            bmFullDirty=true;
          } else if(bmPatternEditMode && tier==2){
            bmPatternEditMode=false;
            bmSaveCustom();
            bmFullDirty=true;
          } else if(bmPatternEditMode && tier==1){
            bmEditVoice=(bmEditVoice+1)%NUM_DRUMS; bmDrumEditPad=bmEditVoice;
            bmVsDirty=true; bmPadsDirty=true; bmDStripDirty=true; bmGridDirty=true;
          } else if(bmPatternEditMode){
            bmToggleCustomStep(bmEditVoice,BM_CUSTOM_IDX);
            bmGridDirty=true; bmBarsDirty=true; bmPadsDirty=true;
          } else {
            bmPatternEditMode=true; bmEditVoice=0; bmDrumEditPad=0;
            bmStoredBeat=BM_CUSTOM_IDX; bmApplyBeat();
            bmFullDirty=true;
          }
        }

        // ── CUSTOM pattern edit: pads 1-15 (i=0-14) toggle a step ────────
        // Step 16 (pad 16) is handled above, sharing the same toggle logic
        // via bmToggleCustomStep. Short tap only, matching the short-tap
        // convention used elsewhere in this file, so a deliberate long-hold
        // chord elsewhere can't double-fire a toggle.
        if(bmPatternEditMode && i<BM_CUSTOM_IDX && !bmPChord[i]){
          uint32_t holdMs=now-bmPDown[i];
          if(holdMs<400){
            bmToggleCustomStep(bmEditVoice,i);
            bmGridDirty=true;
          }
          bmBarsDirty=true; bmPadsDirty=true;
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

  // Pad-16 hold-tier live preview: while pad 16 is physically held (and
  // we're not in FUNC mode, where it means something else — see the press
  // handler above), keep the function-legend strip's highlight in sync
  // with whichever tier the CURRENT hold duration would fire on release.
  // Only marks dirty on an actual tier change (bmP16Tier() only returns 4
  // distinct values), so this is a no-op the rest of the time — cheap
  // enough to poll unconditionally every control pass.
  if(bmPState[BM_CUSTOM_IDX] && !bmFuncMode){
    uint8_t tier=bmP16Tier(now-bmPDown[BM_CUSTOM_IDX], bmPatternEditMode);
    if(tier!=bmP16TierShown){ bmP16TierShown=tier; bmPadsDirty=true; }
  } else if(bmP16TierShown!=255){
    bmP16TierShown=255; bmPadsDirty=true;   // released (or FUNC took over) — repaint back to normal
  }

  // Long-press polls for save. Suppressed during pattern edit: this fires
  // off raw simultaneous pad-hold state (pads 3-6 + 7+8), all of which are
  // step-toggle pads while editing — don't want a save silently firing
  // because a few step pads happened to be held together for a second.
  for(byte i=2;i<=5;i++){
    if(!bmPatternEditMode && bmPState[i]&&bmPState[BM_PAD_FUNC_A]&&bmPState[BM_PAD_FUNC_B]){
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
          // Combined save: captures acid + DRIFT + drums together into
          // this slot, whatever each currently has dialled in — not just
          // the drum pattern. saveAllToSlot() sets saveCommit itself; the
          // actual flash write is still deferred to core 1 (see loop1()'s
          // saveCommit check) — calling EEPROM.commit() directly here
          // runs it inside the audio-critical core-0 control path.
          saveAllToSlot(slot);
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
  if(!bmPatternEditMode &&
     bmPState[2]&&bmPState[3]&&bmPState[4]&&bmPState[5]&&
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
  // This runs EVERY step tick (bmGridDirty set in bmFireDrumStep) and is
  // 8x16 = 128 individual fillRect SPI transactions — the single biggest
  // per-tick draw in drum mode. Top up the ring per drum row so the DMA
  // never starves mid-grid; each call is a near-free no-op once topped.
  byte cur16=bmPlaying?(bmStepNum/6)%16:255;
  byte bar  =bmPlaying?(byte)((bmStepNum/96)%2):0;
  for(byte t=0;t<NUM_DRUMS;t++){
    bmFillDrumBuffer();
    for(byte s=0;s<16;s++){
      byte bb=bmBeatByte(bmParamBeat,t,bar*2+s/8);
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
  bmFillDrumBuffer();   // text-heavy strip, redrawn on every FUNC/value change

  if(bmPatternEditMode){
    byte d=bmEditVoice;
    uint16_t dc=bmDrumCols[d];
    tft.fillRect(0,BM_VS_Y,SW,BM_VS_H,0x0008);
    tft.drawRect(0,BM_VS_Y,SW,BM_VS_H,dc);
    const int cy=BM_VS_Y+6;
    tft.setTextSize(1);
    tft.setTextColor(C_MGR); tft.setCursor(4,cy); tft.print("EDIT:");
    tft.setTextColor(dc); tft.setCursor(40,cy); tft.print(bmDrumNames[d]);
    tft.setTextColor(C_YEL); tft.setCursor(112,cy);
    tft.print("P:"); tft.print(bmDrumPitch[d]);
    tft.setTextColor(C_GRN); tft.setCursor(152,cy);
    tft.print("L:"); tft.print(bmDrumLen[d]);
    tft.setTextColor(C_CYN); tft.setCursor(192,cy);
    tft.print("V:"); tft.print(bmDrumVol[d]);
    tft.setTextColor(C_DGR); tft.setCursor(232,cy); tft.print("P16 HOLD:VOICE");
    return;
  }

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
    bmFillDrumBuffer();   // mid-strip top-up — ~20 more chars of text below
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
  bmFillDrumBuffer();   // 16 of these run back-to-back on bmPadsDirty — keep GP2 fed per cell

  int row=i/8, col=i%8;
  int bx=BM_PAD_SX+col*(BM_PAD_BW+BM_PAD_SP);
  int by=(row==0)?BM_PAD_R0:BM_PAD_R1;

  if(bmPatternEditMode){
    tft.fillRect(bx-1,by-1,BM_PAD_BW+3,BM_PAD_BH+3,C_BG);
    uint16_t col16=bmDrumCols[bmEditVoice];
    // All 16 pads render uniformly: on/off for the current edit voice, plus
    // a playhead outline on whichever step is about to/just fired. Pad 16's
    // other duties (voice-cycle, entry/exit, clear) are duration-gated on
    // release — see bmUpdateControl() — and don't change how it's drawn.
    byte byteIdx=i/8, bitPos=(byte)(7-(i%8));
    bool on=bitRead(bmCustomBeat[bmEditVoice][byteIdx],bitPos);
    bool isPlayhead=bmPlaying && ((bmDrumPos%16)==i);
    uint16_t fill=on?col16:C_BG;
    tft.fillRoundRect(bx,by,BM_PAD_BW,BM_PAD_BH,3,fill);
    uint16_t bord=isPlayhead?C_WHT:(on?col16:C_DGR);
    tft.drawRoundRect(bx,by,BM_PAD_BW,BM_PAD_BH,3,bord);
    if(isPlayhead) tft.drawRoundRect(bx-1,by-1,BM_PAD_BW+2,BM_PAD_BH+2,4,bord);
    tft.setTextSize(1); tft.setTextColor(on?C_BG:0x2104);
    char nb[4]; itoa(i+1,nb,10);
    int fw=strlen(nb)*6;
    tft.setCursor(bx+(BM_PAD_BW-fw)/2,by+16); tft.print(nb);
    return;
  }

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
  bmFillDrumBuffer();   // 8 label cells with text, runs right after the 16-pad loop

  // pad16Held: physically down right now, and not doing FUNC-select duty
  // (pad 16 doubles as the FUNC-slot-8 selector — see the press handler —
  // so this legend has no business appearing there). Showing the legend
  // the instant pad 16 is pressed, even before bmPatternEditMode flips on,
  // is what lets you see "TAP=ENTER" light up before you've committed to
  // anything — that's the live preview this screen exists for.
  bool pad16Held = bmPState[BM_CUSTOM_IDX] && !bmFuncMode;
  if(bmPatternEditMode || pad16Held){
    // FUNC name tiles are meaningless here — CUSTOM edit (or pad 16 being
    // held on the way in) reuses this strip (full 320px width, otherwise
    // idle) as a live instructions legend. Baseline WHITE; whichever tier
    // the CURRENT hold duration would fire on release lights up in that
    // segment's own colour, so you can see what letting go does before you
    // commit to it. tier==255 (not currently held, i.e. already editing
    // and just sitting there between taps) leaves everything white.
    uint8_t tier = bmPState[BM_CUSTOM_IDX]
                 ? bmP16Tier(millis()-bmPDown[BM_CUSTOM_IDX], bmPatternEditMode)
                 : 255;
    tft.fillRect(0,BM_LBL_Y,SW,BM_LBL_H,C_BG);
    tft.setTextSize(1);
    int x=4;
    auto seg=[&](const char* s, uint16_t activeCol, uint8_t myTier){
      tft.setTextColor(tier==myTier ? activeCol : C_WHT);
      tft.setCursor(x,BM_LBL_Y+4); tft.print(s);
      x+=strlen(s)*6+8;
    };
    if(bmPatternEditMode){
      seg("TAP=STEP", C_CYN, 0);
      seg(".5s=VOICE", C_YEL, 1);
      seg("1s=EXIT+SAVE", C_GRN, 2);
      seg("1.5s=CLEAR", C_RED, 3);
    } else {
      // Not editing yet — VOICE and EXIT+SAVE don't apply until you're
      // actually in, so there's nothing useful to show for them; every
      // release in tiers 0-2 does the same thing (ENTER), so that segment
      // stays lit across all of them rather than pretending there are
      // intermediate tiers that aren't really there.
      seg("TAP=ENTER", C_CYN, 0);
      seg("1.5s=CLEAR", C_RED, 3);
    }
    return;
  }

  const int TW=SW/8;
  tft.fillRect(0,BM_LBL_Y,SW,BM_LBL_H,C_BG);
  tft.setTextSize(1);
  for(byte t=0;t<8;t++){
    bmFillDrumBuffer();   // one label (rects+text) per iteration
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
  bmFillDrumBuffer();   // text-heavy strip

  tft.fillRect(0,BM_DS_Y,SW,BM_DS_H,C_BG);
  const int TW=BM_PAD_BW;   // same width as pad cells
  tft.setTextSize(1);
  // ── FILL selected: tiles column-aligned with the pads below, so
  //    tile N = pad N. Pads 1-4 = frequency, pads 5-8 = type. ──
  if(bmFuncMode && bmFuncSel==7){
    const char* freqLbl[4]={"OFF","1BR","2BR","4BR"};
    const char* typeLbl[4]={"HATS","CLAP","SNAR","KIT"};
    for(byte t=0;t<8;t++){
      bmFillDrumBuffer();   // one tile (rects+text) per iteration — keep GP2 fed
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
    bmFillDrumBuffer();   // one tile (rects+name+dots) per iteration — keep GP2 fed
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
  // Redraw ONLY names whose lit-state changed since the last pass.
  // This runs on bmDrumsDirty — i.e. on EVERY drum hit and again when
  // each flash expires. The old version overdrew all 8 names (~32 chars
  // of per-pixel GFX text ≈ 30-40ms of SPI) every time, which single-
  // handedly exceeded the ring buffer's ~31ms budget in one atomic call
  // — the drum-mode crackle. bmLastFlashState[] existed for exactly this
  // comparison; now it's actually used. Typically 1-2 names change per
  // event → ~2-8ms, and GP2 is topped up before each name drawn.
  uint32_t now = millis();
  const int ty=BM_IS_Y+6;
  tft.setTextSize(1);
  for(byte t=0;t<NUM_DRUMS;t++){
    bool lit = bmFired[t] && (now - bmFiredMs[t] < BM_FLASH_MS);
    if (lit == bmLastFlashState[t]) continue;   // unchanged — pixels already correct
    bmLastFlashState[t] = lit;
    bmFillDrumBuffer();
    tft.setTextColor(lit ? bmDrumCols[t] : C_DGR, C_BG);
    tft.setCursor(4+t*38, ty); tft.print(bmDrumNames[t]);
  }
}

void bmDrawDrumFlashFull(){
  // Full redraw on screen switch — fillRect once then overdraw ALL names
  // unconditionally (unlike bmDrawDrumFlash's changed-only fast path) and
  // re-sync bmLastFlashState[]. GP2 topped up per name: 8 names of text
  // is ~30-40ms of SPI, more than the ring's budget in one atomic run.
  uint32_t now = millis();
  const int ty=BM_IS_Y+6;
  tft.fillRect(0,BM_IS_Y,SW,14,C_BG);
  tft.setTextSize(1);
  for(byte t=0;t<NUM_DRUMS;t++){
    bmFillDrumBuffer();
    bool lit = bmFired[t] && (now - bmFiredMs[t] < BM_FLASH_MS);
    bmLastFlashState[t] = lit;
    tft.setTextColor(lit ? bmDrumCols[t] : C_DGR, C_BG);
    tft.setCursor(4+t*38, ty); tft.print(bmDrumNames[t]);
  }
}

// Print a string one character at a time, topping up GP2 between chars.
// For SIZE-2 text a single print() of a 10-char name is ~2000 per-pixel
// GFX writes (~50ms) in ONE atomic call — no fill placed between whole
// draw calls can survive that. Per-char, the largest atomic chunk is one
// size-2 glyph (~192 px ≈ a few ms), safely inside the ring's budget.
void bmPrintFed(const char* s){
  while(*s){ tft.print(*s++); bmFillDrumBuffer(); }
}

// Full-screen clear in horizontal bands, topping up GP2 between bands.
// tft.fillScreen() is one atomic ~103ms SPI blast — more than double the
// ring buffer's budget, so it guaranteed an underrun crackle on every
// full redraw. Worse, that sustained SPI/XIP storm from core 1 is exactly
// the mechanism (documented at updateAudio's __not_in_flash_func) that
// stalls core 0's flash fetches and makes Mozzi miss sample deadlines —
// the acid-side hiccup on mode switch. Banding fixes both at once: GP2
// stays fed, and the inter-band gaps let core 0's XIP fetches through.
// This REPLACES the old bmFillDrumBufferMax()-before-fillScreen idiom,
// which caused its own bug: pre-queueing ~500ms of audio meant every
// drum/ch2 hit for the next half-second played behind the backlog —
// the "drums pause/stumble after mode switch" symptom.
void bmFillScreenFed(uint16_t color){
  const int BAND = 24;   // 320x24x2B ≈ 15KB ≈ ~10ms per band
  for (int y = 0; y < SH; y += BAND) {
    tft.fillRect(0, y, SW, (y + BAND <= SH) ? BAND : SH - y, color);
    bmFillDrumBuffer();
  }
}

void bmDrawInfoBody(){
  bmFillDrumBuffer();   // text-heavy strip

  extern bool syncMode;
  // Single row: beat name (size 1) + flags on same line, freeing vertical space
  tft.fillRect(0,BM_IS_Y+14,SW,SH-BM_IS_Y-14,C_BG);
  const int ty=BM_IS_Y+20;
  // Beat name — or, when in a FUNC slot, the full function name instead
  tft.setTextSize(2);
  if(bmFuncMode && bmFuncSel!=255){
    tft.setTextColor(C_ORG);
    tft.setCursor(4,ty); bmPrintFed(bmFuncFullNames[bmFuncSel]);
  } else {
    tft.setTextColor(bmPlaying?C_GRN:C_CYN);
    tft.setCursor(4,ty); bmPrintFed(bmBeatNames[bmParamBeat]);
  }
  // Flags right-aligned on same row
  tft.setTextSize(1);
  int fx=180;  // flags start x
  if(bmLastLoadedSlot>=0){tft.setTextColor(C_CYN);tft.setCursor(fx,ty+4);tft.print("S");tft.print(bmLastLoadedSlot+1);}
  // Sync-in mode indicator — boot-time selection, GP2 is a clock input
  if(syncMode){tft.setTextColor(C_CYN);tft.setCursor(290,ty+4);tft.print("SYNC");}
}

void bmDrawInfoStrip(){
  // Full-redraw path (called from bmDrawMain after fillScreen): the name
  // strip pixels are GONE, so the changed-only bmDrawDrumFlash() would
  // skip names whose lit-state matches the stale bmLastFlashState[] and
  // leave them invisible. Must use the unconditional Full variant, which
  // repaints all 8 names and re-syncs the state array.
  tft.drawFastHLine(0,BM_IS_Y,SW,C_DGR);
  bmDrawDrumFlashFull(); bmDrawInfoBody();
}

void bmDrawMain(){
  bmBarsForce=true;   // re-entry: force PCH/LEN/VOL bars + values to repaint
  tft.startWrite();
  bmFillScreenFed(C_BG);   // banded — see bmFillScreenFed()
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
  // Top up GP2 before dispatching ANY draw item — every branch below
  // blocks the only core that can feed the ring buffer. Individual heavy
  // draws (grid, pad loop, strips) also fill internally; this entry call
  // guarantees they start from a full ~31ms budget.
  bmFillDrumBuffer();
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


#define BM_BUF_WORDS        8192   // 64 x 128 — must match bmPWM.setBuffers() below
#define BM_AVAIL_PER_SAMPLE    4   // availableForWrite() units per mono sample
#define BM_BUF_CAPACITY  (BM_BUF_WORDS * BM_AVAIL_PER_SAMPLE)  // 32768 — empty-buffer reading
// 768 samples ≈ 47ms. Raised from 512 (31ms) as engineering margin: the
// worst remaining atomic draw is one size-2 glyph (~3-5ms) so 512 would
// probably hold, but 47ms is still musically tight (~3/8 of a 16th at
// 120 BPM, mostly masked by Mozzi's own output-buffer latency on GP15)
// and buys headroom against SPI-cost estimates being off. If drums/ch2
// ever feel late against the grid, drop back to 512 and retest.
#define BM_BUF_TARGET    (768 * BM_AVAIL_PER_SAMPLE)


static void bmFillDrumBufferTo(uint32_t targetQueued){
  if(!bmAlarmStarted) return;

  while(bmPWM.availableForWrite() > (int32_t)(BM_BUF_CAPACITY - targetQueued)){
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
      mix = bmSoftClip(mix);   // peak safety for the drum bus alone
      if (mixDrumGainQ8 != 256) mix = (mix * (int32_t)mixDrumGainQ8) >> 8;  // MIX EDIT trim/boost
    }

    // DRIFT (second synth voice) lives on THIS output — GP2, additively
    // summed with drums, blended against acid/GP15 at the PCB's shared
    // audio node by the physical mix pot (see the DRIFT OUTPUT comment
    // near ch2SynthMode's declaration in the main sketch). Rendered
    // UNCONDITIONALLY (not gated on driftPlaying) so an echo/reverb tail
    // already in flight keeps decaying naturally after DRIFT is stopped,
    // rather than cutting off hard — new notes are separately gated by
    // driftPlaying in advanceStep()'s ch2WantsToFire. ch2RenderSample()
    // lives in Drift.ino and returns a finished sample (softclipped +
    // CH2_OUT_LEVEL-trimmed internally), so it deliberately does NOT go
    // through the drum bus's drive saturator / resonant filter above —
    // those are drum-specific effects, and routing DRIFT through them
    // would smear its own already-processed sound in ways the drum knobs
    // were never meant to touch.
    int32_t drift = ch2RenderSample();
    if (mixDriftGainQ8 != 256) drift = (drift * (int32_t)mixDriftGainQ8) >> 8;  // MIX EDIT trim/boost
    mix += drift;

    // Final safety clip on the COMBINED bus — drums were already
    // peak-limited alone just above; this catches the case where a DRIFT
    // peak coincides with an already-loud drum hit and pushes the sum past
    // full scale. Same curve as the per-voice clip, applied a second time
    // to the sum.
    mix = bmSoftClip(mix);

    // Full 16-bit scale — compensates for higher GP2 resistor (22K)
    bmPWM.write((int16_t)mix, false);
  }
}

// Normal top-up — keeps only BM_BUF_TARGET (~31ms) of backlog queued, so
// ch2/drum triggers reach the speaker promptly. Called from everywhere
// bmFillDrumBuffer() was already called (loop1(), drawMain(), etc.) —
// same name, same signature, nothing else needed to change at those call
// sites.
void bmFillDrumBuffer(){ bmFillDrumBufferTo(BM_BUF_TARGET); }

// Full pre-fill — tops the buffer all the way to capacity (~500ms).
// ONLY for operations that stall the CPU itself so fills CANNOT be
// threaded through them — currently just EEPROM.commit() in loop1(),
// whose flash-sector erase freezes both cores (multicore lockout).
// Do NOT use this before screen draws: use bmFillScreenFed() /
// interleaved bmFillDrumBuffer() instead. A capacity prefill queues
// ~500ms of audio ahead, so every hit triggered while it drains plays
// late — that was the post-mode-switch drum stumble.
void bmFillDrumBufferMax(){ bmFillDrumBufferTo(BM_BUF_CAPACITY); }

void bmStartAudio(){
  if(!bmAlarmStarted){
    bmAlarmStarted=true;
    // High PWM carrier (488kHz vs GP15's 48kHz) eliminates carrier
    // beating that was causing acid to sound quiet when drums play
    bmPWM.setBuffers(64, 128);  // same 8192-sample CAPACITY, sliced into much smaller
                                // chunks (128 vs 1024) — shrinks whatever's actively
                                // "in flight" with DMA at any moment, which sits outside
                                // availableForWrite()'s accounting entirely. Only actually
                                // queued this deep right before EEPROM.commit() (see
                                // bmFillDrumBufferMax()); normal ops top up to BM_BUF_TARGET.
    // Carrier sets resolution (levels = sysclk/carrier), so 488281 makes GP2
    // an 8-bit output. That is NOT the DRIFT crackle: the acid+drift build
    // runs DRIFT on this same output at this same carrier and sounds clean,
    // so 8 bits is demonstrably enough for this voice. Leave it here — the
    // frequency was chosen to keep the carrier far from GP15's 48kHz, which
    // fixed acid going quiet under drums, and that fix is worth keeping.
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
  bmP11Deferred = false;
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
  // Alarm started eagerly at boot now (setup(), non-sync only) — DRIFT
  // needs GP2 live from the first sample too, not just drums, since the
  // layer chord can reach DRIFT without ever entering drum mode first.
}
