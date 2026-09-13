/*
 * =====================================================================
 *              Acid Drip - RP2040 Acid Bassline Synthesizer
 *              Synthesis engine ported from reference "Badass Bass"
 * =====================================================================
 *
 * BUTTON LAYOUT (2 rows of 8 pads):
 *
 *   ┌─────┬─────┬─────┬─────┬─────┬─────┬──────┬──────┐
 *   │  1  │  2  │  3  │  4  │  5  │  6  │  7   │  8   │
 *   │step │step │step │step │step │step │step  │step  │
 *   │ 1   │ 2   │ 3   │ 4   │ 5   │ 6   │ 7    │ 8    │
 *   ├─────┼─────┼─────┼─────┼─────┼─────┼──────┼──────┤
 *   │  9  │ 10  │ 11  │ 12  │ 13  │ 14  │  15  │  16  │
 *   │KEY  │RIFF │SOUND│WALK │ FX  │TEMPO│PLEN  │PAT>  │
 *   └─────┴─────┴─────┴─────┴─────┴─────┴──────┴──────┘
 *   (bottom row labels only visible in FUNC mode)
 *
 * PAD FUNCTIONS (normal mode):
 *   Short press         : Toggle step on/off
 *   Long press ×1       : Toggle accent
 *   Long press ×2       : Toggle glide
 *   Hold + CUT pot      : Set step note (scale-quantised)
 *   All 16 pads are full steps — no dedicated buttons
 *
 * CHORD: PLAY (pads 1+2 simultaneously):
 *   Start / Stop sequencer
 *   Long hold (1s+)     : Factory reset
 *
 * CHORD: FUNC (pads 7+8 simultaneously):
 *   Toggle FUNC mode on/off
 *   Exits FX assign sub-mode if active
 *
 * SAVE / LOAD (while holding pads 7+8):
 *   Tap pad 3/4/5/6     : Load patch from slot 1/2/3/4
 *   Hold pad 3/4/5/6    : Save patch to slot 1/2/3/4 (1 second hold)
 *   Slot status shown as dots top-right of screen (yellow=saved, grey=empty)
 *
 * FUNC MODE (pads 7+8 to enter/exit):
 *   Bottom row pads 9-16 select a function (labels shown on screen):
 *     Pad 9  = KEY    — top row pads 1-8 select root note (C D Eb F G Ab Bb B)
 *     Pad 10 = RIFF   — pads 1-8 select preset riff pattern (DFLT/SQNCE/FUNK/MINI/JUMP/RAVE/SYNC/DARK)
 *     Pad 11 = SOUND  — top row pads 1-8 select synth voice
 *     Pad 12 = WALK   — top row pads 1-8 select note walk mode
 *                        p1=OFF  p2=4TH    p3=OCTWAVE p4=5TH
 *                        p5=BOUNCE  p6=MIN3RD  p7=VAMP3  p8=RANDOM
 *     Pad 13 = FX     — enters FX ASSIGN sub-mode
 *     Pad 14 = TEMPO  — top row pads 1-8 select preset BPM (100/110/120/128/133/138/145/160)
 *                        tap TEMPO pad repeatedly to set BPM by tap-tempo
 *     Pad 15 = PLEN   — top row pads 1-8 set length 1-8
 *                        bottom row pads 9-16 set length 9-16
 *     Pad 16 = PAT>   — top row pads 1-8 select pattern play mode:
 *                        p1=FWD  p2=CW (fwd then rev)  p3=ALT (odd/even)  p4=REV
 *                        p5=SKIP2 (every 2nd)  p6=SKIP3 (every 3rd)  p7=PING  p8=RND
 *
 * FX ASSIGN SUB-MODE (FUNC → pad 13):
 *   Phase 1: press top-row pads 1-8 to select an effect:
 *     Pad 1=None  Pad 2=Oct Up  Pad 3=Retrigger  Pad 4=Stutter
 *     Pad 5=Maj Step  Pad 6=Min Step  Pad 7=Dom7 Step  Pad 8=Dim Step
 *   Phase 2: press any pad 1-16 to assign that effect to that step
 *   Tap selected FX pad again to deselect
 *   Long-hold pad 1 = clear all step effects
 *   Pads 7+8 chord = exit FX assign to main screen
 *
 * EASTER EGG: ACID WALKS (pads 11+12+13+14, hold 1s):
 *   Toggles a hidden bank of 8 patterns inspired by classic acid/house
 *   tracks, each with its own preset key/tempo/length/sound.
 *   Pads 1-8 select a pattern. FUNC mode, FX assign, and drum mode all
 *   continue to work normally while active. Hold the same chord again
 *   to exit back to the normal pattern.
 *
 * ACCENT EDIT (pads 15+16, hold 1s):
 *   Toggles a mode where CUT/RES/DCY tune the accent envelope itself
 *   instead of the live filter:
 *     CUT = accent peak cutoff boost   RES = accent peak resonance boost
 *     DCY = accent decay-time (CCW=short/snappy, CW=long/dramatic)
 *   Normal CUT/RES/DCY values freeze while tuning. Hold the chord again
 *   to exit — settings save to EEPROM automatically and persist across
 *   power cycles until changed again.
 *
 * WIRING:
 *   TFT   : SCK=GP6, SDA=GP7, RST=GP8, DC=GP10, CS=GP13
 *   Pots  : CUT=GP26, RES=GP28, DCY=GP27
 *   Sync  : IN=GP2 (hold pad 14 at power-on to boot into Sync In mode)
 *   Audio : GP15 (100R + 10nF RC filter to output jack)
 *
 *   Pads (top row, steps 1-8 / FUNC apply):
 *     GP14=pad1  GP12=pad2  GP11=pad3  GP16=pad4
 *     GP17=pad5  GP19=pad6  GP20=pad7  GP21=pad8
 *
 *   Pads (bottom row, steps 9-16 / FUNC select):
 *     GP0=pad9   GP1=pad10  GP3=pad11  GP4=pad12
 *     GP5=pad13  GP9=pad14  GP18=pad15 GP22=pad16
 *
 * =====================================================================
 */

// =====================================================================
// MOZZI CONFIG - must precede all includes
// =====================================================================
#include "MozziConfigValues.h"
#define MOZZI_AUDIO_MODE     MOZZI_OUTPUT_PWM
#define MOZZI_ANALOG_READ    MOZZI_ANALOG_READ_NONE
#define MOZZI_AUDIO_PIN_1    15
#define MOZZI_AUDIO_RATE     16384
#define MOZZI_CONTROL_RATE   256

#include <Mozzi.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <EEPROM.h>

// ── Beat Machine 2 — forward declarations ────────────────────────────
extern bool   bmMode;
extern bool   bmFullDirty;
extern void   bmInit();
extern void   bmUpdateControl();
extern void   bmDoDraw();
extern void   bmStartAudio();
extern void   bmClearPadState();
extern bool   bmPotLocked;
extern uint8_t bmPotPickup;
extern void     bmTriggerStep(uint8_t step);
extern byte     bmStepDiv;
extern byte     bmPulseNum;
extern uint16_t bmStepNum;
extern byte     bmDrumPos;
extern bool   bmPlaying;
extern bool   bmPlayChanged;
extern void   bmFillDrumBuffer();
extern void   bmDrawDrumFlash();
extern uint32_t bmLastFlashDraw;
extern float    bmStoredTempo;
extern bool     bmPChord[16];
extern bool     bmPState[16];
extern void   bmSavePatch(uint8_t slot);
extern void   bmLoadPatch(uint8_t slot);
extern bool   bmSlotHasData[4];
extern void   bmCheckSlots();
extern void   bmResetToBasic();

bool     bmModeArmed = false;
uint32_t bmModeArmMs = 0;
#define BM_SW_A  8   // pad 9 (index 8) — mode switch pad A
#define BM_SW_B  9   // pad 10 (index 9) — mode switch pad B
bool     bmAcidWasRunning = false;  // legacy — no longer used for logic (kept to avoid extern breakage)
// acidPlaying: true when the acid sequencer should fire notes.
// Independent of bmPlaying and seq.running.
// seq.running = acidPlaying || bmPlaying (the shared clock ticks whenever
// either module is active; neither module stopping kills the other's clock).
bool     acidPlaying      = false;

// Easter egg: hold pads 11+12+13+14 (indices 10-13) together for ~1s to
// toggle "Acid Walks" — a value strip of 8 classic-acid-inspired patterns.
#define WALK_PAD_A 10  // pad 11
#define WALK_PAD_B 11  // pad 12
#define WALK_PAD_C 12  // pad 13
#define WALK_PAD_D 13  // pad 14
#define WALK_HOLD_MS 1000
bool     walksMode    = false;  // true while the Acid Walks value strip is shown
bool     walksArmed   = false;  // chord held, waiting for WALK_HOLD_MS
uint32_t walksArmMs   = 0;
uint8_t  curWalk      = 0;      // currently-loaded Acid Walks pattern index

// ACCENT EDIT: hold pads 15+16 (indices 14-15) together for ~1s to toggle
// a mode where the CUT/RES/DECAY pots tune the accent envelope itself
// (peak cutoff boost, peak resonance boost, and decay-time multiplier)
// instead of the live filter. Values are saved to EEPROM on exit so the
// "locked in" accent sound persists across power cycles.
#define ACCENT_PAD_A 14  // pad 15
#define ACCENT_PAD_B 15  // pad 16
#define ACCENT_HOLD_MS 1000
bool     accentEditMode  = false;  // true while CUT/RES/DECAY pots tune the accent envelope
bool     accentEditArmed = false;  // chord held, waiting for ACCENT_HOLD_MS
uint32_t accentEditArmMs = 0;
int16_t  accentEnvCutoff = 90;   // accent peak cutoff boost (0-255) — replaces hardcoded 90
int16_t  accentEnvRes    = 90;   // accent peak resonance boost (0-400) — replaces hardcoded 90
int16_t  accentDecayDiv  = 5;    // accent envelope decay divisor (1-10) — replaces hardcoded 5



// =====================================================================
// PINS
// =====================================================================
#define TFT_CS    13
#define TFT_RST    8
#define TFT_DC    10
#define TFT_MOSI   7
#define TFT_SCK    6
#define POT_CUT   26
#define POT_RES   28
#define POT_DECAY 27
#define SYNC_IN    2

// 16 pads — top row (steps 1-8): applies FUNC values / FX assign phase 1+2
//           bottom row (steps 9-16): selects FUNC function in FUNC mode
// Chord: pads 1+2 (indices 0+1) = PLAY/STOP, pads 7+8 (indices 6+7) = FUNC toggle
const uint8_t PAD_PINS[16] = {
  14,12,11,16,17,19,20,21,   // pads 1-8  (steps 1-8,  top row) — pad1=GP14, pad3=GP11 (swapped)
   0, 1, 3, 4, 5, 9,18,22    // pads 9-16 (steps 9-16, bottom row)
};

#define PAD_PLAY_A  0   // pads 1+2 pressed together = PLAY/STOP
#define PAD_PLAY_B  1
#define PAD_FUNC_A  6   // pads 7+8 pressed together = FUNC toggle
#define PAD_FUNC_B  7

// =====================================================================
// DISPLAY
// =====================================================================
#define SW 320
#define SH 240

// This forces MADCTL's MY (Y-mirror) bit on top of setRotation(3)'s own
// MADCTL value. Reason unconfirmed — possibly needed for a specific panel
// variant with reversed row addressing, but that has NOT been verified.
// What IS confirmed: on a standard ILI9341 panel this fights with
// setRotation(3) and produces the wrong orientation (see repo issue #2).
// Comment this out if your screen looks wrong after setRotation(3) with
// it enabled.
#define PANEL_NEEDS_Y_MIRROR

// Colour palette (RGB565)
#define C_BG    0x0000
#define C_WHT   0xFFFF
#define C_RED   0xF800
#define C_GRN   0x07E0
#define C_YEL   0xFFE0
#define C_CYN   0x07FF
#define C_ORG   0xFC60
#define C_DGR   0x4208
#define C_MGR   0x8410
#define C_BLU   0x001F
#define C_MGN   0xF81F  // magenta — accent indicator

// Step-grid colors (303-style warm amber, replacing plain greys for step cells)
#define C_AMB   0xFC60  // orange — active note bar fill, cursor cell background (matches title 'I')
#define C_AMBBG 0x3922  // dark amber-brown — active step cell background (non-cursor)
#define C_AMBBR 0x59C2  // amber-brown — empty step cell border
#define C_MGND  0x3009  // dark magenta — accent step cell background

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

// =====================================================================
// NOTE FREQUENCY TABLE
// =====================================================================
// Placed in RAM (see updateAudio comment) — avoids XIP contention with core 1's SPI traffic
const uint16_t __not_in_flash("audiotables") noteFreq[60] = {
  274,291,308,326,346,366,388,411,436,461,489,518,
  549,581,616,652,691,732,776,822,871,923,978,1036,
  1097,1163,1232,1305,1383,1465,1552,1644,1742,1845,1955,2071,
  2195,2325,2463,2610,2765,2930,3104,3288,3484,3691,3910,4143,
  4389,4650,4927,5220,5530,5859,6207,6577,6968,7382,7821,8286,
};

// =====================================================================
// WAVETABLES
// =====================================================================
const uint8_t __not_in_flash("audiotables") sinetable[256] = {
  127,130,133,136,139,142,145,148,151,154,157,160,163,166,169,172,
  175,178,181,184,186,189,192,194,197,200,202,205,207,209,212,214,
  216,218,221,223,225,227,229,230,232,234,235,237,239,240,241,243,
  244,245,246,247,248,249,250,250,251,252,252,253,253,253,253,253,
  254,253,253,253,253,253,252,252,251,250,250,249,248,247,246,245,
  244,243,241,240,239,237,235,234,232,230,229,227,225,223,221,218,
  216,214,212,209,207,205,202,200,197,194,192,189,186,184,181,178,
  175,172,169,166,163,160,157,154,151,148,145,142,139,136,133,130,
  127,123,120,117,114,111,108,105,102,99,96,93,90,87,84,81,
  78,75,72,69,67,64,61,59,56,53,51,48,46,44,41,39,
  37,35,32,30,28,26,24,23,21,19,18,16,14,13,12,10,
  9,8,7,6,5,4,3,3,2,1,1,0,0,0,0,0,
  0,0,0,0,0,0,1,1,2,3,3,4,5,6,7,8,
  9,10,12,13,14,16,18,19,21,23,24,26,28,30,32,35,
  37,39,41,44,46,48,51,53,56,59,61,64,67,69,72,75,
  78,81,84,87,90,93,96,99,102,105,108,111,114,117,120,123
};

const uint8_t __not_in_flash("audiotables") noisetable[64] = {
  232,175,188,102,142,3,70,116,17,139,22,155,54,118,84,22,
  251,228,160,233,30,32,6,125,16,216,122,189,95,232,135,205,
  181,97,90,80,76,170,0,4,123,183,46,163,185,40,47,208,
  145,67,219,87,74,140,213,10,72,51,29,142,230,63,204,123
};

const uint8_t __not_in_flash("audiotables") compressortable[256] = {
  0,1,3,5,7,9,10,12,14,16,18,19,21,23,25,27,
  28,30,32,34,36,37,39,41,43,45,46,48,50,52,54,55,
  57,59,61,63,64,66,68,70,72,73,75,77,79,81,82,84,
  86,88,90,91,93,95,97,99,100,102,104,106,108,109,111,113,
  115,117,118,120,122,124,126,127,129,131,133,135,136,138,140,142,
  144,145,147,149,151,153,154,156,158,160,162,163,165,167,169,171,
  172,174,176,178,180,181,183,185,187,189,190,192,194,196,198,199,
  201,203,205,207,208,210,212,214,216,217,219,221,223,225,226,228,
  228,228,228,228,228,229,229,229,229,229,230,230,230,230,230,231,
  231,231,231,231,232,232,232,232,233,233,233,233,233,234,234,234,
  234,234,235,235,235,235,235,236,236,236,236,237,237,237,237,237,
  238,238,238,238,238,239,239,239,239,239,240,240,240,240,241,241,
  241,241,241,242,242,242,242,242,243,243,243,243,243,244,244,244,
  244,245,245,245,245,245,246,246,246,246,246,247,247,247,247,247,
  248,248,248,248,249,249,249,249,249,250,250,250,250,250,251,251,
  251,251,251,252,252,252,252,252,253,253,253,253,254,254,254,255
};

// Per-sound pot mapping ranges [CUT_LO, CUT_HI, RES_LO, RES_HI]
// Resonance smoothing curve — applied to positive gResonance values only.
// Maps linear 0-508 through a power-1.6 curve so the resonance peak builds
// gradually from zero instead of jumping abruptly at the midpoint of the pot.
// Negative gResonance (damping side) is left linear and unchanged.
// Usage: if (gResonance > 0) gResonance = resCurve[gResonance];
const uint16_t __not_in_flash("audiotables") resCurve[512] = {
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  2,  2,
    2,  2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  5,  5,  5,  5,  6,
    6,  6,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10, 10, 11, 11, 11,
   12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
   18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26,
   26, 27, 27, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 34, 34, 35,
   35, 36, 37, 37, 38, 38, 39, 40, 40, 41, 41, 42, 43, 43, 44, 45,
   45, 46, 47, 47, 48, 48, 49, 50, 50, 51, 52, 53, 53, 54, 55, 55,
   56, 57, 57, 58, 59, 60, 60, 61, 62, 62, 63, 64, 65, 65, 66, 67,
   68, 68, 69, 70, 71, 71, 72, 73, 74, 74, 75, 76, 77, 78, 78, 79,
   80, 81, 82, 82, 83, 84, 85, 86, 86, 87, 88, 89, 90, 91, 91, 92,
   93, 94, 95, 96, 97, 97, 98, 99,100,101,102,103,104,104,105,106,
  107,108,109,110,111,112,112,113,114,115,116,117,118,119,120,121,
  122,123,124,125,125,126,127,128,129,130,131,132,133,134,135,136,
  137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,
  153,154,155,156,157,158,159,160,161,162,163,164,165,167,168,169,
  170,171,172,173,174,175,176,177,178,179,180,182,183,184,185,186,
  187,188,189,190,191,193,194,195,196,197,198,199,200,201,203,204,
  205,206,207,208,209,211,212,213,214,215,216,218,219,220,221,222,
  223,225,226,227,228,229,230,232,233,234,235,236,238,239,240,241,
  243,244,245,246,247,249,250,251,252,254,255,256,257,258,260,261,
  262,263,265,266,267,268,270,271,272,274,275,276,277,279,280,281,
  282,284,285,286,288,289,290,291,293,294,295,297,298,299,301,302,
  303,305,306,307,309,310,311,313,314,315,317,318,319,321,322,323,
  325,326,327,329,330,331,333,334,336,337,338,340,341,342,344,345,
  347,348,349,351,352,354,355,356,358,359,361,362,363,365,366,368,
  369,370,372,373,375,376,378,379,380,382,383,385,386,388,389,391,
  392,393,395,396,398,399,401,402,404,405,407,408,410,411,412,414,
  415,417,418,420,421,423,424,426,427,429,430,432,433,435,436,438,
  439,441,442,444,446,447,449,450,452,453,455,456,458,459,461,462,
  464,465,467,469,470,472,473,475,476,478,480,481,483,484,486,487,
  489,491,492,494,495,497,498,500,502,503,505,506,508,508,508,508,
};

const int soundRange[11][4] = {
  {718,200,  70,1020},   // 0  SAW+LPF
  {670,260,  80,670},    // 1  SQR+LPF
  {670,0,    0,1020},    // 2  SINE+LPF
  {670,270,  80,1020},   // 3  NOISE+LPF
  {1020,250, 0,1020},    // 4  CSAW  — CUT reversed
  {1023,0,   0,1023},    // 5  CSQR  — CUT reversed
  {1020,600, 0,1020},    // 6  CSIN  — CUT reversed
  {1023,0,   1023,0},    // 7  NOISE+COMB
  {1023,0,   0,1023},    // 8  PWM
  {1023,0,   0,1023},    // 9  SUB SQUARE — RES=filter resonance(reversed), CUT=sub mix
  {0,1023,   0,1023},    // 10 WAVESHAPE
};

int restrictValue(int val, int mn, int mx) {
  long temp = mx - mn;
  temp *= (val < 0 ? 0 : (val > 1023 ? 1023 : val));
  temp /= 1023;
  return (int)(temp + mn);
}

// =====================================================================
// SYNTHESIS STATE
// =====================================================================
volatile uint16_t gFreq      = 287;
volatile uint16_t gTarget    = 287;
volatile int32_t  gFreqFP    = 287 << 8;  // 8.8 fixed-point; gFreq = gFreqFP >> 8
volatile int32_t  gGlideStep = 0;
volatile bool     gGlide     = false;
volatile bool     gPorta     = false;     // portamento — reserved for future use
volatile uint8_t  gPortaSpeed= 4;

volatile int16_t  gCutoff           = 64;
volatile int16_t  gCutoffDisplay    = 64;   // 0-255 for bar display (raw pot, no envelope)
volatile int16_t  gResonance        = 0;
volatile int16_t  gResonanceDisplay = 512;  // 0-1023 for bar display (512 = mid)
volatile int16_t  gVolSub    = 500;
volatile int16_t  gEnvCutoff = 0;    // envelope filter boost — peaks on note-on, decays with DCY
volatile int16_t  gEnvRes    = 0;    // accent resonance envelope boost — peaks on accented note-on, decays with DCY
volatile bool     gAccentActive = false;  // true while the current note's accent envelope is decaying
volatile uint8_t  gSound     = 0;
volatile uint8_t  gEffect    = 0;
volatile int16_t  gDecaySpeed= 2;
int16_t  lastDecaySpeed = 2;  // frozen gDecaySpeed value while accentEditMode is active
volatile uint32_t gLastStepMs= 0;

static uint16_t cnt = 0;
volatile int16_t filtA = 0;
volatile int16_t filtB = 0;
static uint8_t   combBuf[256];
static uint8_t   combPtr = 0;

volatile uint32_t lfoPos   = 0;
volatile int      lfoOffset= 0;
volatile uint8_t  phaseSw  = 128;

volatile uint16_t vFreq[8];
volatile uint16_t vCnt[8];
volatile uint16_t subFreq = 144;  // Sub Square oscillator frequency = gFreq/2 (sound 9)
volatile uint16_t subMix  = 0;    // Sub Square mix balance 0-1023 (sound 9)
static uint16_t subCnt = 0;       // Sub Square phase accumulator
volatile uint8_t  numV  = 1;
volatile uint8_t  volStd= 128;
volatile uint8_t  volClp= 128;

const int8_t __not_in_flash("audiotables") arpeggio[4][4] = {
  {0, 4, 7, 12},
  {0, 3, 7, 12},
  {0, 4, 7, 11},
  {0, 3, 6,  9},
};

// =====================================================================
// SEQUENCER
// =====================================================================
#define NUM_STEPS 16

struct Step {
  uint8_t note;
  bool    active;
  bool    accent;
  bool    glide;
  uint8_t effect;
};

struct Sequencer {
  Step    steps[NUM_STEPS];
  uint8_t origNote[NUM_STEPS];  // working notes — rootNote transposed to current key
  uint8_t rootNote[NUM_STEPS];  // permanent store — always relative to key=C, never mutated
  uint8_t cur;
  uint8_t len;
  bool    running;
  uint16_t tempo;
  uint32_t interval;
  uint32_t lastUs;
  uint8_t  octave;
  uint8_t  sound;
  int8_t   trans;
  uint8_t  arpPos;
  uint8_t  key;
  uint8_t  scale;
  uint8_t  algo;
};

// =====================================================================
// PRESET PATTERNS  (8 baked-in acid riffs, absolute semitones key=C)
// =====================================================================
#define NUM_PRESETS 8
struct PresetPattern {
  uint8_t note[16];
  uint8_t flags[16];   // bit0=active, bit1=accent
  uint8_t glide[16];
  uint8_t effect[16];
};

const PresetPattern PRESETS[NUM_PRESETS] = {
// P0: DFLT — original default riff
{ {24,24,24,27,24,36,31,29,24,31,29,31,36,24,36,39}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P1: SQNCE — tight 8-step motif looped twice, accent-glide pairs on push notes
{ {24,24,31,24,29,27,24,29,24,24,31,24,29,27,24,29}, {3,0,1,0,3,1,0,1,3,0,1,0,3,1,0,1}, {0,1,0,0,0,1,1,0,0,1,0,0,0,1,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P2: FUNK — sparse, punchy, lots of rests
{ {24,24,24,27,24,24,31,24,29,24,24,27,24,31,29,24}, {3,0,0,1,0,1,3,0,3,0,0,1,0,1,1,3}, {0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P3: MINI — hypnotic minimal loop with one accent shift
{ {24,24,27,24,24,27,24,29,24,24,27,24,29,31,29,27}, {1,1,1,1,1,1,1,3,1,1,1,1,1,3,1,1}, {0,0,1,0,0,1,0,0,0,0,1,0,0,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P4: JUMP — octave leaps, big energy
{ {24,36,24,36,27,39,27,31,24,36,29,36,24,34,36,24}, {3,1,1,1,3,1,1,1,3,1,1,1,3,1,1,3}, {0,0,0,1,0,0,0,0,0,0,1,0,0,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P5: RAVE — pumping 16th notes, heavy accents, octave hits, all active
{ {24,36,24,27,36,24,34,36,24,36,27,36,29,36,34,39}, {3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,3}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P6: SYNC — syncopated off-beat accents and rests
{ {24,24,24,29,27,24,31,29,24,27,24,29,31,31,34,36}, {0,3,0,1,3,0,1,1,0,3,0,1,3,0,1,3}, {0,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// P7: DARK — sparse minor, chromatic Ab passing note at step 13, rests give space to brood
{ {24,24,27,24,29,27,24,29,24,27,24,27,32,31,29,27}, {3,0,1,0,3,1,0,1,3,1,0,1,3,1,1,3}, {0,1,0,0,0,1,1,0,0,0,0,1,0,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
};

uint8_t curPreset = 0;

// =====================================================================
// EASTER EGG — "ACID WALKS" (8 patterns inspired by classic acid/303 tracks)
// Hold pads 11+12+13+14 together for ~1s to activate. Same PresetPattern
// layout as PRESETS above (absolute semitones, key=C). These are original
// interpretations in the spirit of each track, not transcriptions.
// =====================================================================
#define NUM_WALKS 8
const PresetPattern WALK_PATTERNS[NUM_WALKS] = {
// W0: DAFNK — Daft Punk "Da Funk"-style: the real 303 riff is in G minor,
// using only G (root), A#/Bb (m3), C (4th), D#/Eb (m6), and F (m7) — D and
// A are omitted. The famous opening is a single rest on step 1 (the "only
// muted note... creates a very effective rest... every time it loops"),
// with an octave turnaround to G3 on the last step before looping back.
{ {31,31,34,31,29,31,34,36, 31,31,27,31,34,29,31,43}, {0,1,1,1,3,1,1,1,3,1,1,1,3,1,1,3}, {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2} },
// W1: ACIDTR — Phuture "Acid Tracks"-style: 8-step near-static hypnotic
// core (the magic in the original comes from live filter/resonance tweaks
// on a near-static loop, not the notes), steps 8-15 a subtle variation
{ {24,24,24,24,27,24,24,24, 24,24,24,24,29,24,24,27}, {1,1,1,3,1,1,1,1,1,1,1,3,1,1,1,1}, {0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0}, {0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0} },
// W2: ILUVU — Donna Summer "I Feel Love"-style: the real riff isn't a
// plain octave alternation — it's a "doubles" cell (root, octave, then
// the 5th and b7th a step below the root) that gives it harmonic
// movement and a percolating, rolling feel. C3 anchors every off-beat
// (the delay-doubled note), while the low note cycles C2-C2-G1-Bb1.
{ {24,36,24,36,19,36,22,36, 24,36,24,36,19,36,22,36}, {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// W3: CNFUSE — New Order "Confusion"-style: this is a documented
// transcription of the Pump Panel Reconstruction's 303 line (the version
// used in Blade) — G2-dominant with a drop to G1, then a section in
// A#1/A#2 (Bb), square wave, two slides and accents on steps 5/8/10/15.
{ {31,31,31,31,31,19,31,31,34,31,22,22,22,34,31,22}, {1,1,1,1,3,1,1,3,1,3,1,1,1,1,3,1}, {1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0}, {0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0} },
// W4: HISTAT — Josh Wink "Higher State of Consciousness"-style: aggressive,
// heavy accents and octave jumps with occasional glide "squeals" rather
// than glide on every other step (clearer articulation, less mushy)
{ {24,24,36,24,27,24,36,29,24,24,36,24,31,24,36,34}, {3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,3}, {0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,1}, {0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0} },
// W5: ACPRNC — Hardfloor "Acperience 1"-style: documented key is G major
// (was C minor). G2 anchors the riff with excursions to A2 (2nd), B2
// (3rd), D3 (5th up) and F3 (maj 7th colour tone), selective slides for
// movement, and a descending diatonic run (F3-D3-B2-A2) in the back half.
{ {31,31,31,33,35,31,31,38,31,31,33,31,41,38,35,33}, {3,1,1,1,3,1,1,1,3,1,1,1,3,1,1,1}, {0,0,1,0,0,0,0,1,0,0,1,0,0,1,1,0}, {0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0} },
// W6: SPASTK — Plastikman "Spastik"-style: tight rolling 8-step pulse with
// stutter-rests and a sudden octave-jump accent — denser than the original
// 2-hit version while keeping the minimal, glitchy character
{ {24,24,24,24,24,24,36,24, 24,24,24,24,24,24,29,24}, {1,1,0,1,1,0,3,1,1,1,0,1,1,0,3,1}, {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,3,0,0,0,0,0,0,0,3,0} },
// W7: PACIFC — 808 State "Pacific State"-style: the real bassline is a
// "snaking 16th-note" SH-101 line in D minor with octave jumps giving it
// "syncopated funk" (the original was C-major arpeggiated, wrong key and
// too smooth). D2 is the anchor, with octave jumps to D3 and movement
// through the D minor triad (D-F-A) plus the G passing tone.
{ {26,26,38,29,26,33,29,26,26,38,31,29,26,33,31,29}, {3,1,1,3,1,1,1,3,3,1,1,3,1,1,1,3}, {0,1,0,0,1,0,1,0,0,1,0,0,1,0,1,0}, {0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0} },
};
const char* VSTRIP_WALKS[NUM_WALKS] = {"DAFNK","ACIDTR","ILUVU","CNFUSE","HISTAT","ACPRNC","SPASTK","PACIFC"};
// Full names shown in the CUT/RES label area when a walk is active
const char* WALK_FULLNAMES[NUM_WALKS] = {"DA FUNK","ACID TRACKS","I FEEL LOVE","CONFUSION","HIGHER STATE","ACPERIENCE 1","SPASTIK","PACIFIC STATE"};
// Tempos matching each track (BPM)
const uint16_t WALK_TEMPOS[NUM_WALKS] = {111, 120, 125, 134, 130, 125, 126, 125};
// Pattern lengths (steps) — shorter, tighter loops for tracks built on
// hypnotic repeating cells (Da Funk, Acid Tracks, I Feel Love, Spastik),
// full 16 for tracks with a longer evolving phrase (Confusion, Higher
// State, Acperience 1, Pacific State)
const uint8_t  WALK_LENS[NUM_WALKS]   = {16, 8, 8, 16, 16, 16, 8, 16};
// Sound engine per track — chosen for tonal character:
//   0=SAW+LPF (classic acid), 1=SQR+LPF (hollow/punchy), 2=SINE+LPF (smooth),
//   4=CSAW+COMB (metallic/ringing resonance)
const uint8_t  WALK_SOUNDS[NUM_WALKS] = {1, 0, 1, 1, 0, 0, 1, 1};

// Default pattern: C2 C2 C2 Eb2 C2 C3 G2 F2 C2 G2 F2 G2 C3 C2 C3 Eb3
// (key=0, scale=CHROM, octave=1 — absolute semitones)
Sequencer seq = {
  {
    {24,true,false,false,0},{24,true,false,false,0},
    {24,true,false,false,0},{27,true,false,false,0},
    {24,true,false,false,0},{36,true,false,false,0},
    {31,true,false,false,0},{29,true,false,false,0},
    {24,true,false,false,0},{31,true,false,false,0},
    {29,true,false,false,0},{31,true,false,false,0},
    {36,true,false,false,0},{24,true,false,false,0},
    {36,true,false,false,0},{39,true,false,false,0},
  },
  {24,24,24,27,24,36,31,29,24,31,29,31,36,24,36,39},  // origNote working copy
  {24,24,24,27,24,36,31,29,24,31,29,31,36,24,36,39},  // rootNote permanent (key=C)
  0,16,false,120,0,0,1,0,0,0,0,0,0
};

// =====================================================================
// PATCH SAVE / LOAD  (EEPROM emulated in RP2040 flash)
// =====================================================================
#define NUM_SLOTS   4
#define EEPROM_SIZE 512
#define PATCH_VALID 0xAC   // magic byte — slot has valid data

struct Patch {
  uint8_t valid;
  uint8_t note[NUM_STEPS];
  uint8_t flags[NUM_STEPS];  // bit0=active, bit1=accent, bit2=glide
  uint8_t effect[NUM_STEPS];
  uint8_t key, scale, sound, octave, len; int8_t trans; uint8_t algo;
  uint16_t tempo;
  uint8_t kwMode, rrMode;  // rrMode (PAT> playback order) — reuses the byte formerly named swingAmt
};
#define PATCH_SIZE   ((int)sizeof(Patch))
#define SLOT_ADDR(s) ((s) * PATCH_SIZE)

// Accent envelope settings — stored in their own slot, just past the 4
// pattern slots, so they persist independently of pattern saves/loads.
#define ACCENT_VALID   0xAE   // magic byte — accent settings slot has valid data
#define ACCENT_ADDR    (NUM_SLOTS * PATCH_SIZE)
struct AccentSettings {
  uint8_t valid;
  int16_t envCutoff;  // 0-255
  int16_t envRes;     // 0-400
  int16_t decayDiv;   // 1-10
};
#define ACCENT_SETTINGS_SIZE ((int)sizeof(AccentSettings))

bool     slotHasData[4]  = {false,false,false,false};
int8_t   lastLoadedSlot  = -1;   // -1 = none loaded this session
volatile bool saveCommit = false;
uint8_t  saveSlotPending = 255;
uint32_t saveSlotDownMs  = 0;
#define  SAVE_HOLD_MS 1000

// =====================================================================
// UI / MENU
// =====================================================================
enum FuncSel {
  FUNC_NONE=-1,
  FUNC_KEY=0, FUNC_PAT, FUNC_SOUND, FUNC_WALK, FUNC_FX, FUNC_TEMPO, FUNC_PLEN, FUNC_PATMODE
};

const char* SNAMES[] = {
  "SAW+LPF","SQUARE+LPF","SINE+LPF","NOISE+LPF",
  "SAW+COMB","SQUARE+COMB","SINE+COMB","NOISE+COMB",
  "PULSE LFO","MULTI SQUARE","WAVESHAPE"
};

// Indices 8-11 are legacy entries unreachable from the UI
const char* FXNAMES[] = {
  "None","Oct Up","Retrigger","Stutter",
  "Maj Step","Min Step","Dom7 Step","Dim Step",
  "Compressor","Overdrive","Sine Modulate","Bit Crush"
};

const char* NNAMES[]    = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* SCNAMES[]   = {"CHROMATIC","MAJOR","MINOR","PENTATONIC","BLUES"};
const char* FUNCNAMES[] = {"KEY","RIFF","SOUND","WALK","FX","TEMPO","PLEN","PAT>"};

// =====================================================================
// KEY WALK
// =====================================================================
// kwMode: 0=off
//   1=4TH     root(2 bars) -> +4th(2 bars) -> repeat
//   2=OCTWAVE call & response: root(2 bars) -> +12 accented(2 bars) -> root glided(2 bars) -> repeat
//   3=5TH     root(2 bars) -> +5th(2 bars) -> repeat
//   4=BOUNCE  root(2 bars) -> +2nd(2 bars) -> repeat
//   5=MIN3RD  root(2 bars) -> +b3(2 bars) -> repeat
//   6=VAMP3   root(2 bars) -> +5th(2 bars) -> +4th(2 bars) -> repeat (6-bar cycle)
//   7=RND1    random musical destination each 2 bars
uint8_t  kwMode      = 0;
uint8_t  kwStepCount = 0;
uint8_t  kwEventCount= 0;
int8_t   kwRoot      = 0;
int8_t   kwOctTrans  = 0;   // OCTWAVE: current trans offset (0 or +12)
bool     kwForceAccent = false;  // OCTWAVE: force accent on the octave-hit step
bool     kwForceGlide  = false;  // OCTWAVE: force glide on the return-to-root step

const char* KWNAMES[] = {
  "OFF","4TH","OCTWAVE","5TH",
  "BOUNCE","MIN3RD","VAMP3","RANDOM"
};

// Value strip tile labels (max 5 chars)
const char* VSTRIP_KEY[8] = {"C","D","Eb","F","G","Ab","Bb","B"};
const char* VSTRIP_PAT[8] = {"DFLT","SQNCE","FUNK","MINI","JUMP","RAVE","SYNC","DARK"};
const char* VSTRIP_OCT[8] = {"CHROM","MAJ","MIN","PENT","BLUES","OCT0","OCT1","OCT2"};
const char* VSTRIP_SND[8] = {"SAW","SQR","SINE","PWM","CSAW","CSQR","CSIN","SUBSQ"};
const char* VSTRIP_WLK[8] = {"OFF","4TH","OCTWV","5TH","BNCE","MIN3","VAMP3","RND"};
const char* VSTRIP_FX[8]  = {"NONE","OCT+","RTRG","STUT","MSTP","mSTP","D7ST","DMST"};
const char* VSTRIP_PLN[8] = {"1","2","3","4","6","8","12","16"};
const char* VSTRIP_PMD[8] = {"FWD","CW","ALT","REV","SKIP2","SKIP3","PING","RND"};

const uint16_t TEMPO_PRESETS[8] = {100,110,120,128,133,138,145,160};
const char*    VSTRIP_BPM[8]    = {"100","110","120","128","133","138","145","160"};

// =====================================================================
// SCALES
// =====================================================================
const uint8_t SCALE_CHROMATIC[] = {0,1,2,3,4,5,6,7,8,9,10,11};
const uint8_t SCALE_MAJOR[]     = {0,2,4,5,7,9,11};
const uint8_t SCALE_MINOR[]     = {0,2,3,5,7,8,10};
const uint8_t SCALE_PENTA[]     = {0,2,4,7,9};
const uint8_t SCALE_BLUES[]     = {0,3,5,6,7,10};

const uint8_t* SCALES[]    = {SCALE_CHROMATIC,SCALE_MAJOR,SCALE_MINOR,SCALE_PENTA,SCALE_BLUES};
const uint8_t  SCALE_LENS[]= {12,7,7,5,6};
#define SCALE_OCTS 3

uint8_t scalePosCount() { return SCALE_LENS[seq.scale] * SCALE_OCTS; }

// scaleNote(stepIdx): maps origNote through current scale using chromatic degree-index mapping.
// origNote is never mutated — switching back to CHROM restores the exact original pitch.
uint8_t scaleNote(uint8_t stepIdx) {
  uint8_t raw = seq.origNote[constrain((int)stepIdx, 0, NUM_STEPS-1)];
  if (seq.scale == 0) return constrain((int)raw, 0, 59);

  int base = (int)seq.key + (int)seq.octave * 12;
  int rel  = (int)raw - base;
  int reg  = rel / 12;
  int pc   = rel % 12;
  if (pc < 0) { pc += 12; reg--; }
  reg = constrain(reg, 0, 2);

  uint8_t to_len = SCALE_LENS[seq.scale];
  uint8_t mapped = (uint8_t)(((uint16_t)pc * to_len + 6) / 12);
  if (mapped >= to_len) mapped = to_len - 1;

  int result = (int)seq.key + (int)SCALES[seq.scale][mapped]
               + reg * 12 + (int)seq.octave * 12;
  return (uint8_t)constrain(result, 0, 59);
}

// Convert scale position index to absolute semitone
uint8_t scalePosToAbs(uint8_t pos) {
  uint8_t len = SCALE_LENS[seq.scale];
  uint8_t oct = pos / len;
  uint8_t deg = pos % len;
  int n = (int)seq.key + (int)SCALES[seq.scale][deg] + (int)oct*12 + (int)seq.octave*12;
  return (uint8_t)constrain(n, 0, 59);
}

// Snap cache: rawNote (0-59) → nearest scale position index (for CUT-pot note editor)
uint8_t snapCache[60];
bool    snapCacheDirty = true;

void rebuildSnapCache() {
  uint8_t total = scalePosCount();
  for (uint8_t n = 0; n < 60; n++) {
    uint8_t best = 0; int bestDist = 9999;
    for (uint8_t p = 0; p < total; p++) {
      int d = abs((int)scalePosToAbs(p) - (int)n);
      if (d < bestDist) { bestDist = d; best = p; }
    }
    snapCache[n] = best;
  }
  snapCacheDirty = false;
}

uint8_t snapToScale(uint8_t rawNote) {
  if (snapCacheDirty) rebuildSnapCache();
  return snapCache[constrain((int)rawNote, 0, 59)];
}

// Snap absolute semitone to nearest note in current scale
uint8_t snapAbsToScale(uint8_t absNote) {
  uint8_t total = scalePosCount();
  uint8_t bestNote = absNote; int bestDist = 9999;
  for (uint8_t p = 0; p < total; p++) {
    uint8_t candidate = scalePosToAbs(p);
    int d = abs((int)candidate - (int)absNote);
    if (d < bestDist) { bestDist = d; bestNote = candidate; }
  }
  return bestNote;
}

// =====================================================================
// UI STATE
// =====================================================================
struct UI {
  uint8_t  editStep;
  bool     dirty;
  bool     fullDirty;
  bool     editDirty;
  bool     funcDirty;
  bool     cellsDirty;
  bool     barDirty;
  bool     valDirty;
  bool     infoDirty;
  uint8_t  cellIdx;
  uint32_t lastMs;
  // Save/load overlay
  bool     slotOverlay;       // true = show save/load banner
  uint8_t  slotOverlaySlot;   // slot number 0-3
  bool     slotOverlaySave;   // true=save, false=load
  bool     slotOverlayEmpty;  // true=load of empty slot
  bool     slotOverlayCleared;// true=show "ACID SLOTS CLEARED" banner (core1-rendered)
  uint32_t slotOverlayMs;     // millis() when overlay was triggered
  uint8_t  slotProgress;      // 0-100, fill % during hold-to-save
  bool     slotProgressShow;  // true = show progress bar (holding)
};
UI ui = {0,true,true,false,false,false,false,false,false,0,0,
         false,0,false,false,false,0,0,false};

void loadPreset(uint8_t idx) {
  if (idx >= NUM_PRESETS) return;
  curPreset = idx;
  const PresetPattern& p = PRESETS[idx];
  seq.key = 0;  // presets stored in key=C; reset key so rootNote == origNote
  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    seq.steps[i].note   = p.note[i];
    seq.rootNote[i]     = p.note[i];   // permanent C-relative store
    seq.origNote[i]     = p.note[i];   // working copy (same as root since key=C)
    seq.steps[i].active = p.flags[i] & 1;
    seq.steps[i].accent = (p.flags[i] >> 1) & 1;
    seq.steps[i].glide  = p.glide[i];
    seq.steps[i].effect = p.effect[i];
  }
  seq.len = 16;
  ui.dirty=true; ui.fullDirty=true; ui.cellsDirty=true; ui.cellIdx=0; ui.infoDirty=true;
}

// EASTER EGG: load one of the 8 Acid Walks patterns (same layout as loadPreset)
void loadWalk(uint8_t idx) {
  if (idx >= NUM_WALKS) return;
  curWalk = idx;
  const PresetPattern& p = WALK_PATTERNS[idx];
  seq.key = 0;
  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    seq.steps[i].note   = p.note[i];
    seq.rootNote[i]     = p.note[i];
    seq.origNote[i]     = p.note[i];
    seq.steps[i].active = p.flags[i] & 1;
    seq.steps[i].accent = (p.flags[i] >> 1) & 1;
    seq.steps[i].glide  = p.glide[i];
    seq.steps[i].effect = p.effect[i];
  }
  seq.len = WALK_LENS[idx];
  if (seq.cur >= seq.len) seq.cur = 0;
  seq.tempo    = WALK_TEMPOS[idx];
  seq.interval = bpm2us(seq.tempo);
  seq.sound    = WALK_SOUNDS[idx];
  noInterrupts();
  gSound     = seq.sound;
  filtA = filtB = 0;
  gEnvCutoff = 0;
  gEnvRes    = 0;
  interrupts();
  ui.dirty=true; ui.fullDirty=true; ui.cellsDirty=true; ui.cellIdx=0; ui.infoDirty=true; ui.valDirty=true; ui.barDirty=true;
}

// =====================================================================
// BUTTON STATE
// =====================================================================
#define DB        20
#define LG       500    // long-press threshold (ms)
#define RPT_DELAY 300
#define RPT_RATE   80

bool     pState[16]={false}, pLast[16]={false}, pLong[16]={false};
uint32_t pDeb[16]={0}, pDown[16]={0};
uint8_t  pCycle[16]={0};
bool     pNoteEdit[16]={false};
uint8_t  pLastPotStep[16]={255};
bool     pChord[16]={false};

bool     funcMode = false;
FuncSel  funcSel  = FUNC_NONE;

uint32_t tapLastMs = 0;
uint8_t  tapCount  = 0;
uint32_t tapSumMs  = 0;
#define  TAP_TIMEOUT 2000

bool    fxAssignMode  = false;
bool    fxAssignHasFx = false;
bool    fxAssignFresh = false;
uint8_t fxAssignFx    = 0;

// PATMODE: 0=fwd 1=fwd1-8 2=fwd9-16 3=rev 4=rev1-8 5=rev9-16 6=pingpong 7=random
uint8_t rrMode    = 0;
bool    rrPingFwd = true;

bool     syncIn=false, syncOk=false;
bool     syncMode=false;  // true = boot with pad 14 held = sync-in mode, GP2 is input
uint32_t syncMs=0;
#define  SYNCTOUT 2000

// Sync pulse divider — pulses per quarter note from the source.
// Set to 1 for 1 PPQN sources (1 pulse per beat — most common on simple gear).
// Set to 2 for 2 PPQN (1 pulse per 8th note — Badass Bass original).
// Set to 24 for DIN sync standard (Roland TR-808/909/TB-303).
uint8_t  syncDiv     = 1;
uint8_t  syncPulse   = 0;

// =====================================================================
// HELPERS
// =====================================================================
uint32_t    bpm2us(uint16_t b)  { return 60000000UL / b / 4; }
const char* nName(uint8_t i)    { return NNAMES[i % 12]; }
uint8_t     nOct(uint8_t i)     { return i / 12; }

// =====================================================================
// FUNCTION BUTTON MAPS
// =====================================================================
const uint8_t KEY_MAP[8]    = {0,2,3,5,7,8,10,11};  // C D Eb F G Ab Bb B
const uint8_t SOUND_MAP[8]  = {0,1,2,8,4,5,6,9};    // pad4=PWM(8), pad8=Sub Square(9)
const uint8_t PLEN_MAP[8]   = {1,2,3,4,6,8,12,16};
const uint8_t FX_CYCLE[]    = {0,1,2,3,4,5,6,7};
#define FX_CYCLE_LEN (sizeof(FX_CYCLE)/sizeof(FX_CYCLE[0]))
const uint8_t FX_PAD_MAP[8] = {0,1,2,3,4,5,6,7};    // None OctUp Retrig Stutter MajStep MinStep Dom7Step DimStep

// =====================================================================
// NOTE TRIGGER
// =====================================================================
void triggerNote(uint8_t freqIdx, bool accent, bool gl) {
  uint16_t f = noteFreq[constrain((int)freqIdx, 0, 59)] / 2;

  if ((gl || gPorta) && seq.running && f != gFreq) {
    int32_t ticks;
    if (gl) {
      ticks = (int32_t)((uint64_t)seq.interval * MOZZI_CONTROL_RATE / 1000000UL);
      if (ticks < 2) ticks = 2;
    } else {
      // portamento: speed 1-8 → ticks 32,24,16,12,8,6,4,2
      const uint8_t portaTicks[8] = {32,24,16,12,8,6,4,2};
      ticks = portaTicks[gPortaSpeed - 1];
    }
    gTarget    = f;
    int32_t diff = ((int32_t)f - (int32_t)gFreq) << 8;
    gGlideStep = diff / ticks;
    if (gGlideStep == 0) gGlideStep = (diff > 0) ? 1 : -1;
    gGlide     = true;
  } else {
    gFreq   = f;
    gFreqFP = (int32_t)f << 8;
    gGlide  = false;
    cnt     = 0;
  }

  gVolSub    = accent ? -60 : 30;  // accent = boosted above full volume, normal = slight attenuation
  gEnvCutoff = accent ? accentEnvCutoff : 30;  // moderate brightness lift — avoid pushing cutoff to its extreme
  gEnvRes    = accent ? accentEnvRes    : 0;   // gentle resonance nudge — avoid pushing into self-oscillation
  gAccentActive = accent;  // accented notes get a slower filter-envelope decay (303-style "wah")
  gLastStepMs= millis();
}

// =====================================================================
// KEY WALK
// =====================================================================
void applyKeyWalk() {
  kwEventCount++;
  switch (kwMode) {
    case 1:  // 4TH
      seq.trans = (kwEventCount % 2 == 1) ? (kwRoot + 5) % 12 : kwRoot;
      break;
    case 2:  // OCTWAVE -- call & response: root(2 bars) -> +12 accented(2 bars) -> root glided(2 bars) -> repeat
      {
        uint8_t phase = kwEventCount % 3;
        kwForceAccent = false;
        kwForceGlide  = false;
        if (phase == 1) {
          // Octave hit — accent for emphasis
          kwOctTrans   = 12;
          kwForceAccent = true;
        } else if (phase == 2) {
          // Return to root — glide the comedown
          kwOctTrans   = 0;
          kwForceGlide = true;
        } else {
          kwOctTrans = 0;
        }
        seq.trans = (int8_t)constrain((int)kwRoot + (int)kwOctTrans, -24, 24);
      }
      break;
    case 3:  // 5TH
      seq.trans = (kwEventCount % 2 == 1) ? (kwRoot + 7) % 12 : kwRoot;
      break;
    case 4:  // BOUNCE
      seq.trans = (kwEventCount % 2 == 1) ? (kwRoot + 2) % 12 : kwRoot;
      break;
    case 5:  // MIN3RD
      seq.trans = (kwEventCount % 2 == 1) ? (kwRoot + 3) % 12 : kwRoot;
      break;
    case 6:  // VAMP3 — root → 5th → 4th → repeat (6-bar cycle, 2 bars each)
      { uint8_t phase = kwEventCount % 3;
        if      (phase == 1) seq.trans = (kwRoot + 7) % 12;
        else if (phase == 2) seq.trans = (kwRoot + 5) % 12;
        else                 seq.trans = kwRoot;
      }
      break;
    case 7:  // RANDOM
      if (kwEventCount % 2 == 1) {
        const uint8_t dests[5] = {2,5,7,10,3};
        seq.trans = (kwRoot + dests[random(5)]) % 12;
      } else {
        seq.trans = kwRoot;
      }
      break;
    default: break;
  }
  ui.dirty = true;
}

// =====================================================================
// STEP ADVANCE
// =====================================================================
uint8_t nextPatStep() {
  uint8_t len  = seq.len;
  uint8_t last = len - 1;

  switch (rrMode) {
    case 0: // FWD — forward through all steps
      return (seq.cur >= last) ? 0 : seq.cur + 1;

    case 1: // CW — bounce across pads 1-8 (indices 0-7) only, regardless of seq.len
      { const uint8_t CW_MAX = 7;  // pad 8 = index 7
        if (rrPingFwd) {
          if (seq.cur >= CW_MAX) { rrPingFwd = false; return CW_MAX - 1; }
          return seq.cur + 1;
        } else {
          if (seq.cur == 0) { rrPingFwd = true; return 1; }
          return seq.cur - 1;
        }
      }

    case 2: // ALT — odd steps first (0,2,4...) then even steps (1,3,5...)
      { uint8_t half = (len + 1) / 2;
        if (seq.cur % 2 == 0) {
          // currently on an odd-index (0,2,4...) step
          uint8_t next = seq.cur + 2;
          if (next >= len) return 1;   // switch to even-index steps
          return next;
        } else {
          // currently on an even-index (1,3,5...) step
          uint8_t next = seq.cur + 2;
          if (next >= len) return 0;   // back to start of odd-index steps
          return next;
        }
      }

    case 3: // REV — reverse through all steps
      return (seq.cur == 0) ? last : seq.cur - 1;

    case 4: // SKIP2 — every other step: 0,2,4,6...
      { uint8_t next = seq.cur + 2;
        return (next >= len) ? 0 : next;
      }

    case 5: // SKIP3 — every third step: 0,3,6,9...
      { uint8_t next = seq.cur + 3;
        return (next >= len) ? (next - len) : next;  // wrap within len
      }

    case 6: // PING — forward then reverse (classic ping-pong)
      if (rrPingFwd) {
        if (seq.cur >= last) { rrPingFwd = false; return last > 0 ? last - 1 : 0; }
        return seq.cur + 1;
      } else {
        if (seq.cur == 0) { rrPingFwd = true; return 1; }
        return seq.cur - 1;
      }

    case 7: // RND — random from active steps within length
      { uint8_t actv[16]; uint8_t n = 0;
        for (uint8_t s = 0; s < len; s++) if (seq.steps[s].active) actv[n++] = s;
        if (n == 0) return seq.cur;
        uint8_t pick = actv[random(n)];
        if (pick == seq.cur && n > 1) pick = actv[random(n)];
        return pick;
      }

    default: return (seq.cur >= last) ? 0 : seq.cur + 1;
  }
}

void advanceStep() {
  seq.cur = nextPatStep();
  // Drum triggers always fire when the clock ticks — bmTriggerStep() gates
  // internally on bmPlaying so silent when drums are stopped.
  bmTriggerStep(seq.cur);

  if (kwMode > 0) {
    kwStepCount++;
    if (kwStepCount >= 32) {
      kwStepCount = 0;
      applyKeyWalk();
    }
  }

  // Acid note logic only fires when acid is independently playing.
  // seq.running means the shared clock is ticking (either acid or drums
  // active); acidPlaying means acid should actually produce notes.
  if (!acidPlaying) return;

  Step& s = seq.steps[seq.cur];
  gEffect     = s.effect;
  gLastStepMs = millis();

  uint8_t baseNote = constrain((int)scaleNote(seq.cur) + seq.trans, 0, 59);
  if (s.effect == 1) baseNote = constrain((int)baseNote + 12, 0, 59);  // Oct Up
  // effect 3 = Stutter: handled in updateControl sub-step block, no pitch change here

  uint8_t ni = baseNote;
  if (s.effect == 4) {
    // Maj Step — deterministic chord tone per step position, major arpeggio {0,4,7,12}
    ni = constrain((int)baseNote + arpeggio[0][seq.cur % 4], 0, 59);
  } else if (s.effect == 5) {
    // Min Step — deterministic chord tone per step position, minor arpeggio {0,3,7,12}
    ni = constrain((int)baseNote + arpeggio[1][seq.cur % 4], 0, 59);
  } else if (s.effect == 6) {
    // Dom7 Step — deterministic chord tone, dominant 7th arpeggio {0,4,7,11}
    ni = constrain((int)baseNote + arpeggio[2][seq.cur % 4], 0, 59);
  } else if (s.effect == 7) {
    // Dim Step — deterministic chord tone, diminished arpeggio {0,3,6,9}
    ni = constrain((int)baseNote + arpeggio[3][seq.cur % 4], 0, 59);
  } else {
    seq.arpPos = 0;
  }

  if (s.active) {
    bool acc = s.accent || kwForceAccent;
    bool gl  = s.glide  || kwForceGlide;
    triggerNote(ni, acc, gl);
  } else gVolSub = 500;
  kwForceAccent = false;
  kwForceGlide  = false;

  ui.dirty = true;
  ui.barDirty = true;
}

// =====================================================================
// PAD HANDLERS
// =====================================================================
void doPadPress(uint8_t p) {
  if (p < NUM_STEPS) { ui.editStep = p; ui.dirty = true; }
}

void doPadRelease(uint8_t p) {
  if (p < NUM_STEPS) {
    if (!pNoteEdit[p] && !pLong[p]) {
      seq.steps[p].active = !seq.steps[p].active;
      ui.editStep = p; ui.dirty = true; ui.editDirty = true;
      // Note: pCycle[p] is intentionally left untouched here. A short press
      // only resets it when it was already 0 (no-op), so a mistimed short
      // press in the middle of an accent/glide long-press sequence (just
      // under the LG threshold) can't silently wipe cycle progress.
    }
  }
  pNoteEdit[p] = false; pLastPotStep[p] = 255;
}

void doPadLong(uint8_t p) {
  if (p >= NUM_STEPS) return;
  Step& s = seq.steps[p];
  ui.editStep = p;
  // Long-press alternates between accent and glide only. FX assignment
  // is exclusively done via the FUNC+FX sub-mode (doFXAssign) — never
  // reachable from a plain long-press on a step pad.
  if (pCycle[p] % 2 == 0) s.accent = !s.accent;
  else                    s.glide  = !s.glide;
  pCycle[p]++;
  ui.dirty = true; ui.editDirty = true;
}

// =====================================================================
// PATCH SAVE / LOAD
// =====================================================================
void savePatch(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  Patch p;
  p.valid = PATCH_VALID;
  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    p.note[i]   = seq.steps[i].note;
    p.flags[i]  = (seq.steps[i].active ? 1 : 0)
                | (seq.steps[i].accent ? 2 : 0)
                | (seq.steps[i].glide  ? 4 : 0);
    p.effect[i] = seq.steps[i].effect;
  }
  p.key    = seq.key;    p.scale   = seq.scale;
  p.sound  = seq.sound;  p.octave  = seq.octave;
  p.len    = seq.len;    p.trans   = seq.trans;
  p.algo   = seq.algo;   p.tempo   = seq.tempo;
  p.kwMode = kwMode;     p.rrMode  = rrMode;
  EEPROM.put(SLOT_ADDR(slot), p);
  // Acid save writes the acid slot ONLY. Drum patterns live in their own
  // EEPROM region and are saved exclusively from drum mode. Acid and drum
  // slots are fully independent even when they share a slot number.
  saveCommit        = true;
  slotHasData[slot] = true;
}

void loadPatch(uint8_t slot) {
  if (slot >= NUM_SLOTS || !slotHasData[slot]) return;
  Patch p;
  EEPROM.get(SLOT_ADDR(slot), p);
  if (p.valid != PATCH_VALID) return;

  // Reset key to C on load — patches store absolute notes, rootNote = origNote at key=C
  // Key can be changed after loading via FUNC→KEY
  seq.key = 0;
  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    seq.steps[i].note   = p.note[i];
    seq.rootNote[i]     = p.note[i];   // permanent C-relative store
    seq.origNote[i]     = p.note[i];   // working copy
    seq.steps[i].active = p.flags[i] & 1;
    seq.steps[i].accent = p.flags[i] & 2;
    seq.steps[i].glide  = p.flags[i] & 4;
    seq.steps[i].effect = p.effect[i];
  }
  seq.key      = p.key;    seq.scale    = p.scale;
  seq.sound    = p.sound;  seq.octave   = p.octave;
  seq.len      = p.len;    seq.trans    = p.trans;
  seq.algo     = p.algo;   seq.tempo    = p.tempo;
  seq.interval = bpm2us(seq.tempo);
  kwMode       = p.kwMode;
  rrMode       = constrain(p.rrMode, (uint8_t)0, (uint8_t)7);
  rrPingFwd    = true;  // reset ping-pong direction so playback starts forward
  kwStepCount  = 0; kwEventCount = 0; kwRoot = seq.trans; kwOctTrans = 0; kwForceAccent = false; kwForceGlide = false;

  noInterrupts();
  gSound     = seq.sound;
  filtA = filtB = 0;
  gEnvCutoff = 0;
  gEnvRes    = 0;
  interrupts();

  snapCacheDirty = true;
  lastLoadedSlot = (int8_t)slot;
  // Acid load restores the acid slot ONLY. Drum patterns are loaded
  // exclusively from drum mode — loading an acid slot never activates a
  // drum pattern and never overrides the tempo with a drum patch's tempo.
  ui.dirty=true; ui.fullDirty=true; ui.infoDirty=true;
}

void checkSlots() {
  for (uint8_t s = 0; s < NUM_SLOTS; s++) {
    uint8_t v;
    EEPROM.get(SLOT_ADDR(s), v);
    slotHasData[s] = (v == PATCH_VALID);
  }
  bmCheckSlots();
}

// =====================================================================
// ACCENT SETTINGS SAVE / LOAD — separate EEPROM slot, persists across
// pattern saves/loads and power cycles
// =====================================================================
void saveAccentSettings() {
  AccentSettings a;
  a.valid     = ACCENT_VALID;
  a.envCutoff = accentEnvCutoff;
  a.envRes    = accentEnvRes;
  a.decayDiv  = accentDecayDiv;
  EEPROM.put(ACCENT_ADDR, a);
  saveCommit = true;
}

void loadAccentSettings() {
  AccentSettings a;
  EEPROM.get(ACCENT_ADDR, a);
  if (a.valid != ACCENT_VALID) return;  // no saved settings — keep defaults
  accentEnvCutoff = constrain(a.envCutoff, (int16_t)0,   (int16_t)255);
  accentEnvRes    = constrain(a.envRes,    (int16_t)0,   (int16_t)400);
  accentDecayDiv  = constrain(a.decayDiv,  (int16_t)1,   (int16_t)20);
}

// =====================================================================
// FUNC MODE — bottom row selects function
// =====================================================================
void doFuncSelect(uint8_t padIdx) {
  if (padIdx < 8 || padIdx > 15) return;
  FuncSel newSel = (FuncSel)(padIdx - 8);

  // Repeated TEMPO pad press = tap tempo
  if (newSel == FUNC_TEMPO && funcSel == FUNC_TEMPO) {
    uint32_t nowT = millis();
    uint32_t gap  = nowT - tapLastMs;
    if (tapLastMs == 0 || gap > TAP_TIMEOUT) {
      tapCount = 1; tapSumMs = 0;
    } else {
      tapSumMs += gap; tapCount++;
      uint16_t newTempo = (uint16_t)constrain(
          (long)60000 * (tapCount-1) / (long)tapSumMs, 40, 300);
      seq.tempo    = newTempo;
      seq.interval = bpm2us(seq.tempo);
    }
    tapLastMs = nowT;
    ui.dirty=true; ui.barDirty=true; ui.valDirty=true; ui.infoDirty=true;
    return;
  }

  fxAssignMode  = false;
  fxAssignHasFx = false;
  funcSel = newSel;

  if (funcSel == FUNC_FX) {
    fxAssignMode  = true;
    fxAssignFresh = true;
    funcMode      = false;
  }
  if (funcSel == FUNC_TEMPO) {
    tapLastMs = 0; tapCount = 0; tapSumMs = 0;
  }

  if (fxAssignMode) { ui.dirty=true; ui.fullDirty=true; }
  else              { ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true; }
}

// FUNC MODE — top row applies value for selected function
void doFuncApply(uint8_t slot) {
  if (funcSel == FUNC_NONE) return;

  switch (funcSel) {
    case FUNC_KEY: {
      int newKey = KEY_MAP[slot] % 12;
      int delta  = newKey - 0;  // rootNote is always stored relative to key=C (0)
      // delta from C to newKey — use shortest path around the octave
      if (delta >  6) delta -= 12;
      if (delta < -6) delta += 12;
      seq.key = (uint8_t)newKey;
      // Recalculate origNote from rootNote + key delta (no accumulation possible)
      for (uint8_t i = 0; i < NUM_STEPS; i++) {
        seq.origNote[i] = (uint8_t)constrain((int)seq.rootNote[i] + delta, 0, 59);
        seq.steps[i].note = seq.origNote[i];
      }
      snapCacheDirty = true;
      ui.dirty=true; ui.cellsDirty=true; ui.cellIdx=0;
      break;
    }
    case FUNC_PAT:
      loadPreset(slot);
      break;
    case FUNC_SOUND:
      seq.sound = SOUND_MAP[constrain(slot, 0, 7)];
      noInterrupts();
      gSound = seq.sound;
      filtA = filtB = 0;
      gEnvCutoff = 0;
      gEnvRes    = 0;
      interrupts();
      ui.dirty = true;
      break;
    case FUNC_WALK:
      kwMode       = constrain(slot, 0, 7);
      kwStepCount  = 0;
      kwEventCount = 0;
      kwRoot       = seq.trans;
      kwOctTrans   = 0;
      kwForceAccent = false;
      kwForceGlide  = false;
      if (kwMode == 0) { seq.trans = 0; kwRoot = 0; }
      ui.dirty = true;
      break;
    case FUNC_PATMODE:
      rrMode    = constrain(slot, 0, 7);
      rrPingFwd = true;
      break;
    case FUNC_TEMPO:
      seq.tempo    = TEMPO_PRESETS[constrain(slot, 0, 7)];
      seq.interval = bpm2us(seq.tempo);
      tapCount = 0; tapLastMs = 0; tapSumMs = 0;
      ui.dirty=true; ui.barDirty=true;
      break;
    default: break;
  }
  ui.valDirty = true;
  ui.infoDirty= true;
}

void doFXAssign(uint8_t padIdx) {
  if (!fxAssignHasFx) {
    if (padIdx < 8) {
      fxAssignFx    = FX_PAD_MAP[padIdx];
      fxAssignHasFx = true;
      ui.dirty=true; ui.fullDirty=true;
    }
    return;
  }

  // Tap the currently selected FX button again to deselect
  if (padIdx < 8 && FX_PAD_MAP[padIdx] == fxAssignFx) {
    fxAssignHasFx = false;
    ui.dirty=true; ui.fullDirty=true;
    return;
  }

  // Assign or toggle FX on tapped step
  if (padIdx < NUM_STEPS) {
    seq.steps[padIdx].effect = (seq.steps[padIdx].effect == fxAssignFx) ? 0 : fxAssignFx;
    ui.dirty=true; ui.fullDirty=true;
  }
}

// =====================================================================
// DISPLAY — STEP CELL
// =====================================================================
void drawStepCell(uint8_t i) {
  const int bW=36, bH=58, bSp=3, sX=4;
  const int rY[2] = {38, 98};
  const int BAR_TOP=6, BAR_H=44, IND_Y_OFF=50;

  Step& s   = seq.steps[i];
  int row   = i / 8, col = i % 8;
  int bx    = sX + col*(bW+bSp);
  int by    = rY[row];
  bool isActive  = (i == seq.cur && acidPlaying);
  bool noteEditing = pNoteEdit[i];

  // Erase — bottom row extends 22px to cover the label strip below
  tft.fillRect(bx-1, by-1, bW+3, (row==1) ? bH+22 : bH+4, C_BG);

  // Cell background
  if (s.active) tft.fillRoundRect(bx, by, bW, bH, 3, isActive ? C_AMB : (s.accent ? C_MGND : C_AMBBG));
  else          tft.drawRoundRect(bx, by, bW, bH, 3, C_AMBBR);
  if (s.accent && s.active && !isActive) tft.drawRoundRect(bx, by, bW, bH, 3, C_MGN);

  if (noteEditing) tft.drawRoundRect(bx-1, by-1, bW+2, bH+2, 4, C_YEL);

  // Note bar
  if (s.active) {
    int noteBarH = constrain((BAR_H * constrain((int)s.note, 0, 59)) / 59, 3, BAR_H);
    int barTop   = by + BAR_TOP + (BAR_H - noteBarH);

    uint16_t barColor;
    if (noteEditing)    barColor = C_YEL;
    else if (isActive)  barColor = s.accent ? C_RED : C_GRN;
    else if (s.accent)  barColor = C_MGN;
    else                barColor = C_AMB;

    tft.fillRect(bx+2, barTop, bW-4, noteBarH, barColor);

    if (!noteEditing) {
      uint8_t cn = scaleNote(i);
      tft.setTextSize(1);
      int nw = (strlen(nName(cn)) + 1) * 6;
      int tx = bx + (bW - nw) / 2;
      int ty;
      if (noteBarH >= 12) {
        tft.setTextColor(C_BG);
        ty = barTop + (noteBarH - 7) / 2;
      } else {
        tft.setTextColor(barColor);
        ty = barTop - 8;
      }
      ty = constrain(ty, by+BAR_TOP, by+IND_Y_OFF-7);
      tft.setCursor(tx, ty);
      tft.print(nName(cn)); tft.print(nOct(cn));
    }
  }

  // Step number
  tft.setTextSize(1);
  tft.setTextColor(s.active ? C_WHT : C_AMBBR);
  tft.setCursor(bx+2, by+1);
  tft.print(i+1);

  // Indicator dots
  int indY = by + IND_Y_OFF + 3;
  if (s.glide  && s.active)  tft.fillRect(bx+bW-6, indY, 6, 5,       C_CYN);
  if (s.effect > 0)           tft.fillCircle(bx+5,   indY+2, 3,       C_CYN);
  if (s.accent && s.active)   tft.fillCircle(bx+bW/2,indY+2, 3,       C_MGN);

  // Active step indicator line
  if (isActive) tft.fillRect(bx, by+bH, bW, 2, C_GRN);

  // FUNC label strip under bottom-row pads (row 1 only)
  if (row == 1) {
    const int LBL_Y = by + bH + 2;
    const int LBL_H = 18;
    const char* fn = FUNCNAMES[i - 8];

    bool isSelected = funcMode && (funcSel != FUNC_NONE) && ((int)(i-8) == (int)funcSel);
    bool funcActive = funcMode && !isSelected;

    uint16_t lblBg  = isSelected ? C_BG  : (funcActive ? C_RED  : 0x000F);
    uint16_t lblBdr = isSelected ? C_RED : (funcActive ? C_RED  : C_BLU);
    uint16_t lblTxt = isSelected ? C_RED : (funcActive ? C_WHT  : C_CYN);

    tft.fillRect(bx, LBL_Y, bW, LBL_H, lblBg);
    tft.drawRect(bx, LBL_Y, bW, LBL_H, lblBdr);
    tft.setTextColor(lblTxt); tft.setTextSize(1);
    int fw = strlen(fn) * 6;
    tft.setCursor(bx + (bW - fw) / 2, LBL_Y + 6);
    tft.print(fn);

    if (isSelected) tft.drawRoundRect(bx-1, by-1, bW+2, bH+2, 4, C_RED);
  }
}

// =====================================================================
// DISPLAY — VALUE STRIP
// =====================================================================
void drawValStrip() {
  const int VS_Y=12, VS_H=20, VS_W=40;
  tft.fillRect(0, VS_Y, 320, VS_H, C_BG);

  // EASTER EGG: Acid Walks pattern strip — shown when active, unless the
  // user has entered FUNC mode (FUNC's own value strip takes priority so
  // KEY/SOUND/etc selection remains visible and usable as normal)
  if (walksMode && !funcMode) {
    tft.setTextSize(1);
    for (uint8_t t = 0; t < NUM_WALKS; t++) {
      int tx = t * VS_W;
      bool isCur = (t == curWalk);
      tft.fillRect(tx, VS_Y, VS_W, VS_H, isCur ? C_AMB : C_AMBBG);
      tft.drawRect(tx, VS_Y, VS_W, VS_H, isCur ? C_WHT : C_AMBBR);
      tft.setTextColor(isCur ? C_BG : C_AMB);
      int fw = strlen(VSTRIP_WALKS[t]) * 6;
      tft.setCursor(tx + (VS_W - fw) / 2, VS_Y + (VS_H-7) / 2);
      tft.print(VSTRIP_WALKS[t]);
    }
    return;
  }

  if (!funcMode || funcSel == FUNC_NONE) return;

  tft.setTextSize(1);

  // TEMPO: 8 BPM tiles
  if (funcSel == FUNC_TEMPO) {
    int activeT = -1;
    for (int t = 0; t < 8; t++) {
      if (TEMPO_PRESETS[t] == seq.tempo) { activeT = t; break; }
    }
    for (uint8_t t = 0; t < 8; t++) {
      int tx = t * VS_W;
      bool isCur = (t == (uint8_t)activeT);
      tft.fillRect(tx, VS_Y, VS_W, VS_H, isCur ? C_CYN  : 0x0861);
      tft.drawRect(tx, VS_Y, VS_W, VS_H, isCur ? C_WHT  : C_DGR);
      tft.setTextColor(isCur ? C_BG : C_MGR);
      int fw = strlen(VSTRIP_BPM[t]) * 6;
      tft.setCursor(tx + (VS_W - fw) / 2, VS_Y + (VS_H-7) / 2);
      tft.print(VSTRIP_BPM[t]);
    }
    return;
  }

  // FX: full-width info banner
  if (funcSel == FUNC_FX) {
    tft.fillRect(0, VS_Y, 320, VS_H, 0x000F);
    tft.drawRect(0, VS_Y, 320, VS_H, C_YEL);
    tft.setTextColor(C_YEL);
    tft.setCursor(4, VS_Y+6);
    tft.print("FX ASSIGN — use pads 1-8 to select effect, then any pad to assign");
    return;
  }

  // PLEN: 16 individual step tiles
  if (funcSel == FUNC_PLEN) {
    const int TW = 320 / 16;
    for (uint8_t t = 0; t < 16; t++) {
      int tx    = t * TW;
      bool active  = (t < seq.len);
      bool isLast  = (t == seq.len - 1);
      tft.fillRect(tx, VS_Y, TW, VS_H, isLast ? C_CYN  : (active ? 0x0841 : C_BG));
      tft.drawRect(tx, VS_Y, TW, VS_H, isLast ? C_WHT  : (active ? C_CYN  : C_DGR));
      tft.setTextColor(isLast ? C_BG : (active ? C_CYN : C_DGR));
      tft.setCursor(tx + (TW-6)/2, VS_Y + (VS_H-7)/2);
      tft.print(t+1);
    }
    return;
  }

  // All other functions: 8 equal tiles
  const char** labels = nullptr;
  int activeIdx = -1;

  switch (funcSel) {
    case FUNC_KEY:
      labels = VSTRIP_KEY;
      for (int s=0; s<8; s++) if (KEY_MAP[s]%12 == seq.key) { activeIdx=s; break; }
      break;
    case FUNC_PAT:
      labels = VSTRIP_PAT;
      activeIdx = (int)curPreset;
      break;
    case FUNC_SOUND:
      labels = VSTRIP_SND;
      for (int s=0; s<8; s++) if (SOUND_MAP[s] == seq.sound) { activeIdx=s; break; }
      break;
    case FUNC_WALK:
      labels = VSTRIP_WLK;
      activeIdx = kwMode;
      break;
    case FUNC_PATMODE:
      labels = VSTRIP_PMD;
      activeIdx = (int)rrMode;
      break;
    default: break;
  }

  if (!labels) return;

  for (uint8_t t = 0; t < 8; t++) {
    int tx = t * VS_W;
    bool isCur = (t == (uint8_t)activeIdx);
    tft.fillRect(tx, VS_Y, VS_W, VS_H, isCur ? C_CYN  : 0x0861);
    tft.drawRect(tx, VS_Y, VS_W, VS_H, isCur ? C_WHT  : C_DGR);
    tft.setTextColor(isCur ? C_BG : C_MGR);
    int fw = strlen(labels[t]) * 6;
    tft.setCursor(tx + (VS_W - fw) / 2, VS_Y + (VS_H - 7) / 2);
    tft.print(labels[t]);
  }
}

// =====================================================================
// DISPLAY — POT BARS
// =====================================================================
void drawBars() {
  const int bY=180, bP=10, bBarH=6;

  static int      prevCw      = -1;
  static int      prevResW    = -1;
  static int      prevBpmW    = -1;
  static int      prevDcyw    = -1;
  static uint16_t prevTempo   = 0;
  static bool     prevTempMode= false;
  static bool     prevAccentEdit = false;

  // Bar borders are drawn ONCE in drawMain() via drawBarBorders().
  // All fills are inset by 1px (x=37, y+1, w=178, h=bBarH-2) so border pixels
  // are never touched.
  //
  // Each bar is drawn in a single pass to avoid flicker:
  //   1. Draw the coloured fill from the left edge up to the current value.
  //   2. Erase only the remainder to the right (BG fill from cw to FW).
  // This way no full-black flash appears between erase and fill.
  const int FX = 37;        // inset x
  const int FW = 178;       // inset width
  const int FH = bBarH - 2; // inset height

  // Helper lambda — draw bar fill in one clean pass, no flash
  // col=fill colour, val=filled width (0..FW), fy=top-left y of interior
  auto drawBarFill = [&](int fy, int val, uint16_t col) {
    if (val > 0)   tft.fillRect(FX,        fy, val,      FH, col);
    if (val < FW)  tft.fillRect(FX + val,  fy, FW - val, FH, C_BG);
  };

  // CUT
  int cw = constrain((gCutoffDisplay * FW) / 255, 0, FW);
  if (cw != prevCw || accentEditMode != prevAccentEdit) {
    if (accentEditMode) {
      int acw = constrain((accentEnvCutoff * FW) / 255, 0, FW);
      drawBarFill(bY + 1, acw, C_AMB);
      tft.fillRect(224, bY, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY); tft.print("ACUT");
    } else {
      drawBarFill(bY + 1, cw, C_RED);
    }
    prevCw = cw;
  }

  // EASTER EGG: show the active Acid Walks track name to the right of the
  // CUT bar (this area is otherwise unused on this row).
  if (!accentEditMode) {
    static bool prevWalksMode = false;
    static uint8_t prevWalkShown = 255;
    if (walksMode != prevWalksMode || (walksMode && curWalk != prevWalkShown)) {
      tft.fillRect(224, bY, 96, bBarH+1, C_BG);
      if (walksMode) {
        tft.setTextColor(C_AMB);
        tft.setCursor(224, bY);
        tft.print(WALK_FULLNAMES[curWalk]);
      }
      prevWalksMode = walksMode; prevWalkShown = curWalk;
    }
  }

  // RES / BPM / ARES
  bool tempoMode = (funcMode && funcSel == FUNC_TEMPO);
  if (accentEditMode) {
    int arw = constrain((accentEnvRes * FW) / 400, 0, FW);
    if (arw != prevResW || accentEditMode != prevAccentEdit) {
      drawBarFill(bY + bP + 1, arw, C_AMB);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY+bP); tft.print("ARES");
      prevResW = arw;
    }
  } else if (tempoMode) {
    int bw = constrain(((int)(seq.tempo - 40) * FW) / 260, 0, FW);
    bool flash = (tapLastMs > 0 && (millis() - tapLastMs) < 200);
    if (bw != prevBpmW || tempoMode != prevTempMode || seq.tempo != prevTempo || flash || accentEditMode != prevAccentEdit) {
      drawBarFill(bY + bP + 1, bw, flash ? C_WHT : C_CYN);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      tft.setTextColor(C_CYN); tft.setCursor(4,   bY+bP); tft.print("BPM");
      tft.setTextColor(C_CYN); tft.setCursor(224, bY+bP); tft.print(seq.tempo);
      if (tempoMode != prevTempMode) tft.drawRect(36, bY+bP, 180, bBarH, C_CYN);
      prevBpmW = bw; prevTempo = seq.tempo; prevTempMode = tempoMode;
    }
  } else {
    int rw = (constrain((int)gResonanceDisplay, 0, 1023) * FW) / 1023;
    if (rw != prevResW || tempoMode != prevTempMode || accentEditMode != prevAccentEdit) {
      drawBarFill(bY + bP + 1, rw, C_YEL);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      tft.setTextColor(C_WHT); tft.setCursor(4, bY+bP); tft.print("RES");
      if (tempoMode != prevTempMode) tft.drawRect(36, bY+bP, 180, bBarH, C_DGR);
      prevResW = rw; prevTempMode = tempoMode;
    }
  }

  // DCY / ADCY
  if (accentEditMode) {
    // accentDecayDiv ranges 20 (short, CCW) .. 1 (long, CW) — invert for the
    // bar so CW (longer, more dramatic decay) shows more fill
    int adw = constrain(((20 - accentDecayDiv) * FW) / 19, 0, FW);
    if (adw != prevDcyw || accentEditMode != prevAccentEdit) {
      drawBarFill(bY + bP*2 + 1, adw, C_AMB);
      tft.fillRect(224, bY+bP*2, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP*2, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY+bP*2); tft.print("ADCY");
      prevDcyw = adw;
    }
    prevAccentEdit = true;
    return;  // skip the normal WALK-label row below — ACCENT EDIT owns this row
  } else {
    int dcyw = constrain((int)((gDecaySpeed - 3) * FW) / 1023, 0, FW);
    if (dcyw != prevDcyw || accentEditMode != prevAccentEdit) {
      drawBarFill(bY + bP*2 + 1, dcyw, C_MGR);
      prevDcyw = dcyw;
    }
  }
  prevAccentEdit = accentEditMode;

  // WALK label (right of DCY bar)
  tft.fillRect(224, bY+bP*2, 96, bBarH+1, C_BG);
  tft.setTextColor(kwMode > 0 ? C_CYN : C_DGR);
  tft.setCursor(224, bY+bP*2); tft.print(KWNAMES[kwMode]);
}

// =====================================================================
// DISPLAY — INFO STRIP
// =====================================================================
void drawInfoStrip() {
  const int IS_Y = 213;
  tft.drawFastHLine(0, IS_Y, SW, C_DGR);
  tft.fillRect(0, IS_Y+1, SW, SH-(IS_Y+1), C_BG);

  const int ty = IS_Y + 10;

  if (accentEditMode) {
    tft.setTextSize(1);
    tft.setTextColor(C_AMB);
    tft.setCursor(4, ty);
    tft.print("ACCENT EDIT - CUT/RES/DCY tune accent (15+16=exit)");
    return;
  }

  // Layout (320px total):
  //  x=4   BPM:nnn      alloc 48px  → x=52
  //  x=52  Key Scl Oct  alloc 66px  → x=118
  //  x=118 Sound        alloc 36px  → x=154
  //  x=154 nSTEP        alloc 42px  → x=196
  //  x=196 Walk mode    alloc 48px  → x=244
  //  x=244 Pat mode     alloc 36px  → x=280
  //  x=280 [PLAY/STOP]  alloc 36px  → x=316
  //  x=316 S (sync)                 → x=322

  tft.setTextSize(1);

  tft.setTextColor(C_CYN);
  tft.setCursor(4, ty); tft.print("BPM:"); tft.print(seq.tempo);

  tft.setTextColor(C_WHT);
  tft.setCursor(52, ty);
  tft.print(NNAMES[seq.key]); tft.print(" ");
  const char* scShort[] = {"CHR","MAJ","MIN","PNT","BLU"};
  tft.print(scShort[constrain((int)seq.scale, 0, 4)]);
  tft.print(" O"); tft.print(seq.octave);

  tft.setTextColor(C_MGR);
  tft.setCursor(118, ty);
  { int sslot = 0;
    for (int s=0; s<8; s++) if (SOUND_MAP[s] == seq.sound) { sslot=s; break; }
    tft.print(VSTRIP_SND[sslot]);
  }

  tft.setTextColor(C_GRN);
  tft.setCursor(154, ty);
  tft.print(seq.len); tft.print("ST");

  tft.setTextColor(kwMode > 0 ? C_CYN : C_DGR);
  tft.setCursor(196, ty);
  tft.print(kwMode > 0 ? KWNAMES[kwMode] : "WALK");

  tft.setTextColor(rrMode > 0 ? C_CYN : C_DGR);
  tft.setCursor(244, ty);
  tft.print(VSTRIP_PMD[rrMode]);

  tft.setTextColor(acidPlaying ? C_GRN : C_YEL);
  tft.setCursor(280, ty);
  tft.print(acidPlaying ? "[PLAY]" : "[STOP]");

  if (syncOk) {
    tft.setTextColor(C_CYN);
    tft.setCursor(316, ty);
    tft.print("S");
  }
}

// =====================================================================
// DISPLAY — MAIN SCREEN (full redraw)
// =====================================================================
// Shared bar drawing constants
#define BAR_Y    180
#define BAR_P     10
#define BAR_BARH   6

void drawBarLabels() {
  tft.setTextSize(1);
  tft.setTextColor(C_WHT); tft.setCursor(4, BAR_Y);         tft.print("CUT");
  tft.setTextColor(C_WHT); tft.setCursor(4, BAR_Y+BAR_P);   tft.print("RES");
  tft.setTextColor(C_WHT); tft.setCursor(4, BAR_Y+BAR_P*2); tft.print("DCY");
}

// Draw static bar outlines — called once on full redraw only.
// drawBars() never redraws these to avoid the flickering white-border effect.
void drawBarBorders() {
  tft.drawRect(36, BAR_Y,         180, BAR_BARH, C_DGR);  // CUT
  tft.drawRect(36, BAR_Y+BAR_P,   180, BAR_BARH, C_DGR);  // RES (C_DGR default; CYN in BPM mode)
  tft.drawRect(36, BAR_Y+BAR_P*2, 180, BAR_BARH, C_DGR);  // DCY
}

// Slot occupancy dots — drawn inside the value strip, centred above pads 3-6.
// Called once on full redraw and after any slot state change.
// Empty = hollow grey, saved = filled yellow, lastLoaded = filled black outline cyan.
void drawSlotDots() {
  const int bW=36, bSp=3, sX=4;
  const int DOT_R = 4, DOT_Y = 5;

  for (uint8_t s = 0; s < 4; s++) {
    uint8_t col = (s + 2) % 8;
    int cx      = sX + col * (bW + bSp) + bW / 2;

    tft.fillRect(cx-DOT_R-1, 0, (DOT_R+1)*2+2, 12, C_BG);

    if (!slotHasData[s]) {
      tft.drawCircle(cx, DOT_Y, DOT_R, C_DGR);          // hollow grey = empty
    } else if ((int8_t)s == lastLoadedSlot) {
      tft.fillCircle(cx, DOT_Y, DOT_R, C_CYN);          // cyan = currently loaded
      tft.drawCircle(cx, DOT_Y, DOT_R, C_WHT);
    } else {
      tft.fillCircle(cx, DOT_Y, DOT_R, C_YEL);          // yellow = saved, not loaded
      tft.drawCircle(cx, DOT_Y, DOT_R, C_ORG);
    }
  }
}

// Save progress — only redraws the value strip when pct changes.
// Smooth orange fill, no flicker.
void drawSaveProgress(uint8_t pct) {
  static int8_t prevPct = -1;
  if (pct == prevPct) return;   // nothing changed — skip the SPI write
  prevPct = pct;

  const int VS_Y=12, VS_H=20;
  const int FX=1, FW=SW-2, FH=VS_H-2;

  // Draw border once (same colour, so cheap to repeat)
  tft.drawRect(0, VS_Y, SW, VS_H, C_ORG);

  // Fill interior: orange portion then black remainder — two rects, no full clear
  int filled = FW * pct / 100;
  if (filled > 0)        tft.fillRect(FX,          VS_Y+1, filled,      FH, 0x6200);
  if (filled < FW)       tft.fillRect(FX + filled,  VS_Y+1, FW - filled, FH, C_BG);

  // "SAVING" centred — single draw, colour flips at halfway
  const char* label = "SAVING";
  tft.setTextSize(2);
  int lw = strlen(label) * 12;
  uint16_t tcol = (filled > (SW/2 - lw/2)) ? C_WHT : C_DGR;
  tft.setTextColor(tcol);
  tft.setCursor((SW - lw) / 2, VS_Y + (VS_H - 14) / 2);
  tft.print(label);

  // Slot number — left side
  tft.setTextSize(1); tft.setTextColor(C_ORG);
  tft.setCursor(4, VS_Y + 7);
  tft.print("SLT "); tft.print(ui.slotOverlaySlot + 1);
}

// Confirmation banner in value strip after save/load completes
void drawSlotOverlay() {
  if (!ui.slotOverlay) return;

  bool expired = (millis() - ui.slotOverlayMs) > 1400;
  if (expired) {
    ui.slotOverlay        = false;
    ui.slotOverlayCleared = false;
    ui.slotProgressShow   = false;
    ui.valDirty           = true;
    drawSlotDots();   // refresh dots in case a save just populated a slot
    return;
  }

  const int VS_Y=12, VS_H=20;

  if (ui.slotOverlayCleared) {
    tft.fillRect(0, VS_Y, SW, VS_H, C_BG);
    tft.drawRect(0, VS_Y, SW, VS_H, C_DGR);
    tft.setTextSize(1); tft.setTextColor(C_DGR);
    tft.setCursor(4, VS_Y+7); tft.print("ACID SLOTS CLEARED");
  } else if (ui.slotOverlayEmpty) {
    tft.fillRect(0, VS_Y, SW, VS_H, C_BG);
    tft.drawRect(0, VS_Y, SW, VS_H, C_DGR);
    tft.setTextSize(1); tft.setTextColor(C_DGR);
    tft.setCursor(4, VS_Y+7);
    tft.print("SLOT "); tft.print(ui.slotOverlaySlot+1);
    tft.print("  —  EMPTY");
  } else if (ui.slotOverlaySave) {
    tft.fillRect(0, VS_Y, SW, VS_H, 0x0300);
    tft.drawRect(0, VS_Y, SW, VS_H, C_GRN);
    tft.setTextSize(2); tft.setTextColor(C_GRN);
    const char* label = "SAVED";
    int lw = strlen(label) * 12;
    tft.setCursor((SW-lw)/2, VS_Y + (VS_H-14)/2);
    tft.print(label);
    tft.setTextSize(1); tft.setTextColor(C_GRN);
    tft.setCursor(4, VS_Y+7); tft.print("SLT "); tft.print(ui.slotOverlaySlot+1);
  } else {
    tft.fillRect(0, VS_Y, SW, VS_H, 0x0008);
    tft.drawRect(0, VS_Y, SW, VS_H, C_CYN);
    tft.setTextSize(2); tft.setTextColor(C_CYN);
    const char* label = "LOADED";
    int lw = strlen(label) * 12;
    tft.setCursor((SW-lw)/2, VS_Y + (VS_H-14)/2);
    tft.print(label);
    tft.setTextSize(1); tft.setTextColor(C_CYN);
    tft.setCursor(4, VS_Y+7); tft.print("SLT "); tft.print(ui.slotOverlaySlot+1);
  }
}

void drawMain() {
  tft.startWrite();
  // Full screen clear — ensures FX assign remnants (bars, info, footer) are wiped
  tft.fillScreen(C_BG);
  tft.fillRect(0, 96, SW, 2, C_BG);
  bmFillDrumBuffer();  // top up GP2 DMA buffer — drawMain() is the longest blocking call

  drawValStrip();
  drawSlotDots();
  bmFillDrumBuffer();
  for (uint8_t i = 0; i < 16; i++) {
    drawStepCell(i);
    if ((i & 3) == 3) bmFillDrumBuffer();  // every 4 cells — keep buffer fed during this loop
  }

  // Bar labels and static borders (drawn once here; drawBars() only repaints fills)
  drawBarLabels();
  drawBarBorders();
  bmFillDrumBuffer();

  // Bar fills — inset by 1px so drawBarBorders() outlines are never touched.
  // Single-pass: colour fill then erase remainder (no full-black flash).
  const int BFX = 37, BFW = 178, BFH = BAR_BARH - 2;

  auto drawBarFillM = [&](int fy, int val, uint16_t col) {
    if (val > 0)  tft.fillRect(BFX,       fy, val,       BFH, col);
    if (val < BFW) tft.fillRect(BFX + val, fy, BFW - val, BFH, C_BG);
  };

  bool tempoMode = (funcMode && funcSel == FUNC_TEMPO);
  if (tempoMode) {
    tft.setTextColor(C_CYN); tft.setCursor(4, BAR_Y+BAR_P); tft.print("BPM");
    int bw = constrain(((int)(seq.tempo - 40) * BFW) / 260, 0, BFW);
    drawBarFillM(BAR_Y + BAR_P + 1, bw, C_CYN);
    tft.drawRect(36, BAR_Y+BAR_P, 180, BAR_BARH, C_CYN);
    tft.setTextColor(C_CYN); tft.setCursor(224, BAR_Y+BAR_P); tft.print(seq.tempo);
  } else {
    int rw = (constrain((int)gResonanceDisplay, 0, 1023) * BFW) / 1023;
    drawBarFillM(BAR_Y + BAR_P + 1, rw, C_YEL);
  }

  int cw = constrain((gCutoffDisplay * BFW) / 255, 0, BFW);
  drawBarFillM(BAR_Y + 1, cw, C_RED);

  int dcyw = constrain((int)((gDecaySpeed - 3) * BFW) / 1023, 0, BFW);
  drawBarFillM(BAR_Y + BAR_P*2 + 1, dcyw, C_MGR);

  tft.fillRect(224, BAR_Y+BAR_P*2, 96, BAR_BARH+1, C_BG);
  tft.setTextColor(kwMode > 0 ? C_CYN : C_DGR);
  tft.setCursor(224, BAR_Y+BAR_P*2); tft.print(KWNAMES[kwMode]);
  bmFillDrumBuffer();

  drawInfoStrip();
  bmFillDrumBuffer();
  tft.endWrite();
}

void updateMain() {
  static uint8_t prevCur = 255;
  if (prevCur != 255 && prevCur != seq.cur) drawStepCell(prevCur);
  drawStepCell(seq.cur);
  prevCur = seq.cur;
  drawBars();
  drawInfoStrip();
}

// =====================================================================
// DISPLAY — FX ASSIGN SCREEN
// =====================================================================
void drawFXInfo() {
  // Info strip only — called when infoDirty without full redraw
  const char* scShort[] = {"CHR","MAJ","MIN","PNT","BLU"};
  const int iY = 196;
  tft.fillRect(0, iY, SW, 14, C_BG);
  tft.drawFastHLine(0, iY, SW, C_DGR);
  tft.setTextSize(1);
  tft.setTextColor(C_CYN);  tft.setCursor(4,   iY+4); tft.print("BPM:"); tft.print(seq.tempo);
  tft.setTextColor(C_WHT);  tft.setCursor(52,  iY+4);
  tft.print(NNAMES[seq.key]); tft.print(" "); tft.print(scShort[constrain((int)seq.scale,0,4)]);
  tft.print(" O"); tft.print(seq.octave);
  tft.setTextColor(C_MGR);  tft.setCursor(118, iY+4);
  { int sslot=0; for(int s=0;s<8;s++) if(SOUND_MAP[s]==seq.sound){sslot=s;break;}
    tft.print(VSTRIP_SND[sslot]); }
  tft.setTextColor(C_GRN);  tft.setCursor(154, iY+4); tft.print(seq.len); tft.print("ST");
  tft.setTextColor(acidPlaying ? C_GRN : C_YEL);
  tft.setCursor(280, iY+4); tft.print(acidPlaying ? "[PLAY]" : "[STOP]");
}

void drawFXBars() {
  // Pot bars only — called when barDirty without full redraw
  const int bY=163, bP=9, bBarH=5;
  const int FX2=37, FW2=178, FH2=bBarH-2;
  int cw   = constrain((gCutoffDisplay * FW2) / 255, 0, FW2);
  int rw   = (constrain((int)gResonanceDisplay, 0, 1023) * FW2) / 1023;
  int dcyw = constrain((int)((gDecaySpeed - 3) * FW2) / 1023, 0, FW2);
  if (cw > 0)   tft.fillRect(FX2, bY+1,      cw,   FH2, C_RED);
  if (cw < FW2) tft.fillRect(FX2+cw, bY+1,   FW2-cw, FH2, C_BG);
  if (rw > 0)   tft.fillRect(FX2, bY+bP+1,   rw,   FH2, C_YEL);
  if (rw < FW2) tft.fillRect(FX2+rw, bY+bP+1, FW2-rw, FH2, C_BG);
  if (dcyw > 0)   tft.fillRect(FX2, bY+bP*2+1, dcyw, FH2, C_MGR);
  if (dcyw < FW2) tft.fillRect(FX2+dcyw, bY+bP*2+1, FW2-dcyw, FH2, C_BG);
}

void drawFXAssign() {
  if (fxAssignFresh) {
    tft.fillScreen(C_BG);   // full clear — wipes all remnants from previous screen
    // Don't draw slot dots in FX mode — full screen is used for FX assignment UI
    fxAssignFresh = false;
  }

  tft.setTextColor(C_WHT); tft.setTextSize(2);
  tft.setCursor(4, 4); tft.print("FX ASSIGN");

  tft.fillRect(168, 0, SW-168, 22, C_BLU);
  tft.setTextColor(C_CYN); tft.setTextSize(1);
  tft.setCursor(170, 7);
  if (fxAssignHasFx) {
    tft.print("assigning: ");
    char nm[12]; strncpy(nm, FXNAMES[fxAssignFx], 11); nm[11]=0;
    tft.setTextColor(C_YEL); tft.print(nm);
  } else {
    tft.print("pick an FX to assign");
  }

  // 8 FX buttons in 2 rows of 4
  const int eW=76, eH=32, eSp=2, eX=2, eY=26;
  for (uint8_t i = 0; i < 8; i++) {
    int bx = eX + (i%4) * (eW+eSp);
    int by = eY + (i/4) * (eH+eSp);
    uint8_t fx  = FX_PAD_MAP[i];
    bool sel = (fxAssignHasFx && fxAssignFx == fx);
    tft.drawRoundRect(bx-1, by-1, eW+2, eH+2, 4, C_BG);
    tft.fillRoundRect(bx, by, eW, eH, 3, sel ? C_YEL : C_DGR);
    if (sel) tft.drawRoundRect(bx-1, by-1, eW+2, eH+2, 4, C_ORG);
    tft.setTextColor(sel ? C_BG : C_WHT); tft.setTextSize(1);
    tft.setCursor(bx+3, by+4); tft.print(i+1);
    char nm[12]; strncpy(nm, FXNAMES[fx], 11); nm[11]=0;
    tft.setCursor(bx+3, by+16); tft.print(nm);
  }

  // Step grid
  const int sY = 100;
  tft.fillRect(0, sY, SW, 60, C_BG);   // clear step grid area only (not bar area below)
  tft.setTextSize(1);
  tft.setTextColor(fxAssignHasFx ? C_CYN : C_DGR);
  tft.setCursor(4, sY);
  tft.print(fxAssignHasFx ? "TAP ANY STEP TO ASSIGN:" : "STEP EFFECTS (select FX above to assign):");

  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    int bx = 4  + (i%8) * 39;
    int by = sY + 10 + (i/8) * 28;
    uint8_t fx  = seq.steps[i].effect;
    bool match  = (fxAssignHasFx && fx == fxAssignFx);
    uint16_t fc = match ? C_ORG : (fx > 0 ? C_YEL : C_DGR);
    if (match) tft.fillRoundRect(bx, by, 36, 24, 2, C_ORG);
    else       tft.drawRoundRect(bx, by, 36, 24, 2, fc);
    tft.setTextColor(match ? C_BG : fc); tft.setTextSize(1);
    tft.setCursor(bx+3, by+4); tft.print(i+1);
    if (fx > 0) {
      char sh[5]; strncpy(sh, FXNAMES[fx], 4); sh[4]=0;
      tft.setCursor(bx+3, by+14); tft.print(sh);
    }
  }

  // Pot bars — same geometry as main screen, shifted to y=163
  {
    const int bY=163, bP=9, bBarH=5;
    const int FX2=37, FW2=178, FH2=bBarH-2;
    tft.setTextSize(1);
    tft.setTextColor(C_WHT); tft.setCursor(4, bY);       tft.print("CUT");
    tft.setTextColor(C_WHT); tft.setCursor(4, bY+bP);    tft.print("RES");
    tft.setTextColor(C_WHT); tft.setCursor(4, bY+bP*2);  tft.print("DCY");
    tft.drawRect(36, bY,       178, bBarH, C_DGR);
    tft.drawRect(36, bY+bP,    178, bBarH, C_DGR);
    tft.drawRect(36, bY+bP*2,  178, bBarH, C_DGR);
    int cw   = constrain((gCutoffDisplay * FW2) / 255, 0, FW2);
    int rw   = (constrain((int)gResonanceDisplay, 0, 1023) * FW2) / 1023;
    int dcyw = constrain((int)((gDecaySpeed - 3) * FW2) / 1023, 0, FW2);
    if (cw > 0)   tft.fillRect(FX2, bY+1,      cw,   FH2, C_RED);
    if (cw < FW2) tft.fillRect(FX2+cw, bY+1,   FW2-cw, FH2, C_BG);
    if (rw > 0)   tft.fillRect(FX2, bY+bP+1,   rw,   FH2, C_YEL);
    if (rw < FW2) tft.fillRect(FX2+rw, bY+bP+1, FW2-rw, FH2, C_BG);
    if (dcyw > 0)   tft.fillRect(FX2, bY+bP*2+1, dcyw, FH2, C_MGR);
    if (dcyw < FW2) tft.fillRect(FX2+dcyw, bY+bP*2+1, FW2-dcyw, FH2, C_BG);
  }

  // Info strip — BPM, key, sound, steps, play state
  {
    const int iY = 196;
    tft.fillRect(0, iY, SW, 14, C_BG);
    tft.drawFastHLine(0, iY, SW, C_DGR);
    tft.setTextSize(1);
    tft.setTextColor(C_CYN);  tft.setCursor(4,   iY+4); tft.print("BPM:"); tft.print(seq.tempo);
    tft.setTextColor(C_WHT);  tft.setCursor(52,  iY+4);
    const char* scShort[] = {"CHR","MAJ","MIN","PNT","BLU"};
    tft.print(NNAMES[seq.key]); tft.print(" "); tft.print(scShort[constrain((int)seq.scale,0,4)]);
    tft.print(" O"); tft.print(seq.octave);
    tft.setTextColor(C_MGR);  tft.setCursor(118, iY+4);
    { int sslot=0; for(int s=0;s<8;s++) if(SOUND_MAP[s]==seq.sound){sslot=s;break;}
      tft.print(VSTRIP_SND[sslot]); }
    tft.setTextColor(C_GRN);  tft.setCursor(154, iY+4); tft.print(seq.len); tft.print("ST");
    tft.setTextColor(acidPlaying ? C_GRN : C_YEL);
    tft.setCursor(280, iY+4); tft.print(acidPlaying ? "[PLAY]" : "[STOP]");
  }

  // Footer — two explicit lines (manual split, no reliance on GFX auto-wrap;
  // the old single-line strings were 65/55 chars, wider than the 320px panel
  // can hold at ~52 chars/line, so the wrapped remainder used to render
  // partially off the bottom of the screen)
  const int footY = 210, footH = SH - footY;   // reclaims the dead gap below the info strip
  tft.fillRect(0, footY, SW, footH, C_DGR);
  tft.setTextColor(C_WHT); tft.setTextSize(1); tft.setTextWrap(false);
  tft.setCursor(4, footY+4);
  tft.print(fxAssignHasFx
    ? "tap pads to assign   tap FX again to deselect"
    : "tap an FX above to select it");
  tft.setCursor(4, footY+16);
  tft.print(fxAssignHasFx
    ? "hold pad1 = clear all"
    : "hold pad1 = clear all   p7+p8 = exit");
}

// =====================================================================
// DISPLAY — DISPATCH
// =====================================================================
// =====================================================================
// DISPLAY — SAVE / LOAD OVERLAY
// =====================================================================

void doDraw() {
  if (fxAssignMode) {
    if (ui.fullDirty) { ui.fullDirty=false; drawFXAssign(); return; }
    // Only redraw pot bars when pots move — not the whole screen
    if (ui.barDirty) { ui.barDirty=false; drawFXBars(); return; }
    if (ui.infoDirty) { ui.infoDirty=false; drawFXInfo(); return; }
    return;
  }
  if (ui.fullDirty) { ui.fullDirty=false; drawMain(); return; }
  if (ui.cellsDirty) {
    if (ui.valDirty) { ui.valDirty=false; drawValStrip(); return; }
    uint32_t now = millis();
    if (now - ui.lastMs > 20) {
      ui.lastMs = now;
      drawStepCell(ui.cellIdx);
      if (ui.cellIdx+1 < NUM_STEPS) drawStepCell(ui.cellIdx+1);
      ui.cellIdx += 2;
      if (ui.cellIdx >= NUM_STEPS) { ui.cellsDirty=false; ui.cellIdx=0; }
    }
    return;
  }
  if (ui.editDirty) { ui.editDirty=false; drawStepCell(ui.editStep); return; }
  if (ui.funcDirty) {
    if (ui.valDirty)  { ui.valDirty=false;  drawValStrip();  return; }
    if (ui.infoDirty) { ui.infoDirty=false; drawInfoStrip(); return; }
    uint32_t now = millis();
    if (now - ui.lastMs > 30) {
      ui.lastMs = now;
      uint8_t base = 8 + ui.cellIdx;
      drawStepCell(base);
      if (base+1 < 16) drawStepCell(base+1);
      ui.cellIdx += 2;
      if (ui.cellIdx >= 8) { ui.funcDirty=false; ui.cellIdx=0; }
    }
    return;
  }
  if (ui.barDirty)  { ui.barDirty=false;  drawBars();      return; }
  if (ui.valDirty)  { ui.valDirty=false;  drawValStrip();  return; }
  if (ui.infoDirty) { ui.infoDirty=false; drawInfoStrip(); return; }
  updateMain();
}

void doModeSwitch() {
  bmModeArmed = false;
  bmMode = !bmMode;
  if (bmMode) {
    // Entering drum mode — acid sequencer KEEPS RUNNING if it was playing.
    // updateAudio() returns silence in bmMode so GP15 is muted,
    // but advanceStep() still ticks → bmTriggerStep() drives drums perfectly.
    // If acid was stopped, seq.running stays false → drums silent until started.
    bmAcidWasRunning = acidPlaying;  // keep legacy flag in sync for updateAudio()
    if (!syncMode) bmStartAudio();
    bmFullDirty = true;
    bmStoredTempo = (float)seq.tempo;   // display tracks the shared clock
    bmPotLocked = true;
    // Land on Basic ONLY when drums aren't already playing. If a pattern is
    // actively running (bmPlaying), preserve it on entry so hopping synth->
    // drums->synth doesn't wipe a live pattern and its function tweaks.
    // When drums are silent we still reset to a clean Basic slate. Saved
    // slots only take effect when explicitly selected while in drum mode.
    if (!bmPlaying) {
      bmResetToBasic();
    }
    bmPotPickup = (uint8_t)(analogRead(POT_CUT) >> 2);
    // BM_SW_A/B (pads 9+10) are very likely still physically held during
    // this transition. Leave bmPState/bmPLast untouched (still true,
    // matching the held GPIO) and force bmPChord true so the eventual
    // release is absorbed by the harmless "bmPChord[i]=false" path below
    // instead of the "released without chord -> select beat pattern"
    // branch, which would otherwise change bmStoredBeat unexpectedly.
    bmPChord[BM_SW_A]=true; bmPChord[BM_SW_B]=true;
  } else {
    // Leaving drum mode — acid was never stopped so nothing to restart.
    // Just restore pad state and redraw.
    bmClearPadState();
    ui.fullDirty = true; ui.dirty = true;
    // Skip BM_SW_A/B (pads 9+10) — they are very likely still physically
    // held during this transition. Resetting pState/pLast to false here
    // would desync them from the GPIO, causing a spurious doPadPress()
    // on the next tick and a doPadRelease() step-toggle when the user
    // eventually lifts the pads.
    for (byte _i=0; _i<16; _i++) {
      if (_i == BM_SW_A || _i == BM_SW_B) continue;
      pState[_i]=false; pLast[_i]=false;
    }
  }
  // BM_SW_A/B (pads 9+10): keep pChord TRUE rather than clearing it.
  // pState/pLast for these two pads are intentionally left untouched (still
  // true, matching the physically-held pads). When the user releases the
  // pads, pLast!=r triggers the release branch; pChord[i] being true routes
  // it into the harmless "pChord[i]=false" no-op instead of doPadRelease(),
  // which would otherwise toggle seq.steps[8/9].active and force an
  // unwanted extra ui.editDirty redraw right after the mode-switch redraw
  // (the brief flash-then-redraw effect).
  pChord[BM_SW_A] = true; pChord[BM_SW_B] = true;
  // Note: seq.lastUs is deliberately left untouched here. The original
  // self-correcting catch-up logic in the sequencer-advance loop
  // (seq.lastUs += seq.interval, with a snap-forward only if more than
  // one full interval behind) handles brief blocking windows without
  // drift. An earlier attempt to resync seq.lastUs = micros() here
  // discarded that phase reference and could itself delay or skip the
  // next step — now that the mode-switch redraw is deferred to the next
  // loop1() pass and the FIFO branch only does a single fillScreen(),
  // the blocking window is short enough that no special-casing is needed.
  rp2040.fifo.push_nb(bmMode ? 2u : 3u);
}

// =====================================================================
// MOZZI updateControl() — 256Hz
// =====================================================================
void updateControl() {
  // ── Beat Machine: drum clock always runs, mode switch on pads 1+2+3 ─
  // Sync acid play state when drum play toggled from drum mode
  if (bmPlayChanged) {
    bmPlayChanged = false;
    if (bmPlaying) {
      // Drums started — ensure the shared clock is ticking.
      // Only reset the clock phase if it wasn't already running
      // (i.e. acid was also stopped). If acid is running the clock
      // is already ticking at the right phase — leave it alone.
      if (!seq.running) {
        seq.running = true;
        seq.cur = seq.len - 1;
        seq.lastUs = micros();
        syncPulse = 0;
        bmStepNum = 0; bmPulseNum = 0;
      }
    } else {
      // Drums stopped — only stop the shared clock if acid is also not playing.
      // If acid is still playing, the clock must keep ticking for acid steps.
      if (!acidPlaying) {
        seq.running = false;
      }
    }
    ui.dirty = true; ui.barDirty = true; ui.infoDirty = true;
  }
  // Drums driven directly from acid advanceStep() — no tempo sync needed
  // Fire mode switch when pads 9+10 held >= 1s
  if (bmModeArmed && (millis()-bmModeArmMs) >= 1000) {
    doModeSwitch();
    // Note: pChord/pState for BM_SW_A/B are deliberately left as set by
    // doModeSwitch() (pChord=true, pState/pLast unchanged) — see comments
    // there for why resetting them here caused a spurious step-toggle and
    // extra redraw on release.
  }
  // Disarm if either pad released
  if (bmModeArmed && (digitalRead(PAD_PINS[BM_SW_A])!=LOW || digitalRead(PAD_PINS[BM_SW_B])!=LOW)) {
    bmModeArmed=false;
  }

  // EASTER EGG: fire Acid Walks toggle when pads 11+12+13+14 held >= 1s
  if (walksArmed && (millis()-walksArmMs) >= WALK_HOLD_MS) {
    walksArmed = false;
    walksMode  = !walksMode;
    ui.dirty=true; ui.fullDirty=true; ui.cellsDirty=true; ui.cellIdx=0; ui.infoDirty=true; ui.barDirty=true;
  }
  // Disarm if any of the four pads released before the hold completes
  if (walksArmed && (digitalRead(PAD_PINS[WALK_PAD_A])!=LOW || digitalRead(PAD_PINS[WALK_PAD_B])!=LOW ||
                     digitalRead(PAD_PINS[WALK_PAD_C])!=LOW || digitalRead(PAD_PINS[WALK_PAD_D])!=LOW)) {
    walksArmed=false;
  }

  // ACCENT EDIT: fire toggle when pads 15+16 held >= ACCENT_HOLD_MS.
  // On exit (accentEditMode was true), save the tuned values to EEPROM
  // so they're "locked in" until the next edit session.
  if (accentEditArmed && (millis()-accentEditArmMs) >= ACCENT_HOLD_MS) {
    accentEditArmed = false;
    if (accentEditMode) saveAccentSettings();  // leaving edit mode — persist
    accentEditMode = !accentEditMode;
    ui.dirty=true; ui.fullDirty=true; ui.barDirty=true; ui.infoDirty=true;
  }
  // Disarm if either pad released before the hold completes
  if (accentEditArmed && (digitalRead(PAD_PINS[ACCENT_PAD_A])!=LOW || digitalRead(PAD_PINS[ACCENT_PAD_B])!=LOW)) {
    accentEditArmed=false;
  }
  bmUpdateControl();   // always tick drum clock regardless of mode

  // Glide and envelope always run regardless of mode — keeps acid sounding right
  // when playing simultaneously with drums
  {
    uint32_t now_ae = millis();
    if (gGlide) {
      gFreqFP += gGlideStep;
      int32_t newFreq = gFreqFP >> 8;
      if (gGlideStep > 0) {
        if (newFreq >= (int32_t)gTarget) { gFreqFP = (int32_t)gTarget << 8; gGlide = false; }
      } else {
        if (newFreq <= (int32_t)gTarget) { gFreqFP = (int32_t)gTarget << 8; gGlide = false; }
      }
      gFreq = (uint16_t)constrain(gFreqFP >> 8, 1, 65535);
    }
    static uint32_t lastFallMs = 0;
    static int32_t  fallFrac   = 0;
    static int32_t  envFrac    = 0;
    if (now_ae - lastFallMs >= 5) {
      uint32_t elapsed = now_ae - lastFallMs;
      lastFallMs = now_ae;
      int fallSpeed = gDecaySpeed;
      if (gGlide || seq.steps[seq.cur].glide) fallSpeed *= 2;
      if (fallSpeed < 1024) {
        fallFrac += (int32_t)elapsed * 1024;
        int32_t steps = fallFrac / fallSpeed;
        fallFrac -= steps * fallSpeed;
        gVolSub  += (int16_t)steps;
        // Accented notes get a slower filter-envelope decay — the cutoff
        // sweeps down over a longer "wah" rather than snapping back at the
        // same rate as a normal hit, matching the 303's accent behaviour
        // (accent raises both the envelope peak and its decay time).
        int envDiv  = gAccentActive ? accentDecayDiv : 3;
        int envSpeed = max(fallSpeed / envDiv, 1);
        envFrac += (int32_t)elapsed * 1024;
        int32_t envSteps = envFrac / envSpeed;
        envFrac -= envSteps * envSpeed;
        gEnvCutoff -= (int16_t)envSteps;
        gEnvRes    -= (int16_t)envSteps;
        if (gEnvCutoff <= 0) { gEnvCutoff = 0; gAccentActive = false; }
        if (gEnvRes    <= 0)   gEnvRes    = 0;
      }
      gVolSub    = constrain((int16_t)gVolSub,    (int16_t)-128, (int16_t)500);
      gEnvCutoff = constrain((int16_t)gEnvCutoff, (int16_t)0,    (int16_t)255);
      gEnvRes    = constrain((int16_t)gEnvRes,    (int16_t)0,    (int16_t)400);
    }
  }

  // Sequencer advance — self-correcting timing (no drift accumulation)
  if (seq.running) {
    uint32_t us = micros();
    if (us - seq.lastUs >= seq.interval) {
      seq.lastUs += seq.interval;  // advance by exact interval, not captured time
      // Catch-up: if we fell more than one interval behind, snap forward
      // (prevents cascade of rapid fires after a long delay)
      if (us - seq.lastUs >= seq.interval) seq.lastUs = us - (seq.interval >> 1);
      advanceStep();
    }
  }

  // If in BM mode hand off all further control to drum machine
  if (bmMode) { return; }

  uint32_t now = millis();

  // --- POTS ---
  int rawCut    = analogRead(POT_CUT);
  int rawRes    = analogRead(POT_RES);
  int rawResOrig= rawRes;         // un-reversed for sounds 8+9
  rawRes        = 1023 - rawRes;  // reverse pot direction
  int rawDecay  = analogRead(POT_DECAY);
  // Share with BeatMachine2 — prevents double ADC reads on same pins

  static int lastRawCut=0, lastRawRes=0, lastRawDecay=0;
  if (abs(rawCut-lastRawCut)>4 || abs(rawRes-lastRawRes)>4 || abs(rawDecay-lastRawDecay)>4) {
    lastRawCut=rawCut; lastRawRes=rawRes; lastRawDecay=rawDecay;
    ui.barDirty = true;
  }

  gDecaySpeed = rawDecay + 3;

  // Display bars always reflect raw physical pot position — full travel, all sounds.
  // Audio engine uses soundRange-mapped values below; display is completely independent.
  gCutoffDisplay   = rawCut >> 2;          // 0-1023 → 0-255
  gResonanceDisplay= rawResOrig;           // 0-1023, un-reversed physical pot position

  if (accentEditMode) {
    // ACCENT EDIT: CUT/RES/DECAY pots tune the accent envelope itself.
    // Normal gCutoff/gResonance/gDecaySpeed are frozen at their last
    // values so the live bassline keeps its current tone while tuning.
    gDecaySpeed = lastDecaySpeed;

    accentEnvCutoff = rawCut >> 2;                          // 0-1023 -> 0-255
    accentEnvRes    = (int16_t)((rawResOrig * 400L) / 1023); // 0-1023 -> 0-400
    accentDecayDiv  = 20 - (int16_t)((rawDecay * 19L) / 1023); // 0-1023 -> 20..1 (CW = longer decay)
  } else {
  lastDecaySpeed = gDecaySpeed;

  uint8_t si    = constrain(gSound, 0, 10);
  int mappedRes = restrictValue(rawRes, soundRange[si][0], soundRange[si][1]);
  int mappedCut = restrictValue(rawCut, soundRange[si][2], soundRange[si][3]);

  // Envelope volume decay contribution (pre-calc)
  int16_t volSub4  = (gVolSub > 400) ? 100 : (gVolSub >> 2);

  switch (gSound) {
    case 0: case 1: case 2: case 3:
      gCutoff   = max(0, mappedCut/4 - volSub4);
      gResonance= mappedRes - 512;
      break;
    case 4:
      gCutoff   = max(0, mappedCut/4 - volSub4) >> 4;
      gResonance= mappedRes - 512;
      break;
    case 5: case 6: case 7:
      gCutoff   = max(0, mappedCut - volSub4) >> 4;
      gResonance= mappedRes - 512;
      break;
    case 8:
      if (mappedRes < 10) {
        if (lfoOffset > 0 && lfoOffset > (128 - (mappedCut>>3))) {
          phaseSw = lfoOffset; lfoOffset--;
        } else {
          phaseSw = 128 - (mappedCut>>3); lfoPos = 0x10000; lfoOffset = 0;
        }
      } else {
        lfoPos += mappedRes - 10;
        int lfoVal = restrictValue((sinetable[(lfoPos>>10)&255]<<2)+8, 128, 128-(mappedCut>>3));
        phaseSw = (uint8_t)lfoVal; lfoOffset = phaseSw;
      }
      gCutoff   = max(0, (rawCut>>2) - volSub4);
      gResonance= rawResOrig - 512;
      break;
    case 9:
      // Sub Square — main square layered with a sub-octave square wave.
      // mappedCut (0-1023) -> mix balance: 0 = all carrier, max = all sub-octave.
      // mappedRes (0-1023, reversed) -> filter resonance (shared LPF).
      subFreq = (uint16_t)max(1, gFreq >> 1);  // one octave below carrier
      subMix  = (uint16_t)constrain(mappedCut, 0, 1023);
      gCutoff   = max(0, (rawCut>>2) - volSub4);
      gResonance= rawResOrig - 512;
      break;
    case 10:
      gCutoff   = max(0, mappedCut - volSub4) >> 1;
      gResonance= mappedRes;
      break;
  }

  // Resonance smoothing — apply power curve to positive gResonance only.
  // The damping region (gResonance < 0) stays linear; the resonance peak region
  // builds gradually from zero instead of jumping abruptly at the pot midpoint.
  if (gResonance > 0 && gResonance < 512) {
    gResonance = (int16_t)resCurve[gResonance];
  }
  // Note: accent's filter character (extra cutoff brightness + resonance
  // "squelch") comes entirely from the gEnvCutoff/gEnvRes envelopes set in
  // triggerNote() and applied in updateAudio() via effCut/effRes. There is
  // deliberately no separate static accent boost here — a single decaying
  // envelope reads as a 303-style sweep rather than a step that's simply
  // "on" for its whole duration.
  }  // end !accentEditMode

  // ── PAD READS ──────────────────────────────────────────────────────
  // Re-use rawCut (already read) for note-edit pot quantisation
  uint8_t scaleTotal = scalePosCount();
  uint8_t potPos     = (uint8_t)((rawCut * (long)scaleTotal) / 1024);
  if (potPos >= scaleTotal) potPos = scaleTotal - 1;
  uint8_t noteEditPotStep = scalePosToAbs(potPos);

  for (uint8_t i = 0; i < 16; i++) {
    bool r = (digitalRead(PAD_PINS[i]) == LOW);
    if (r != pLast[i] && (now - pDeb[i]) > DB) {
      pDeb[i]=now; pLast[i]=r; pState[i]=r;

      if (r) {
        // ── PAD PRESSED ────────────────────────────────────────────
        pDown[i]=now; pLong[i]=false; pNoteEdit[i]=false; pChord[i]=false;
        pLastPotStep[i]=noteEditPotStep;

        // PLAY chord (pads 1+2)
        if ((i==PAD_PLAY_A && pState[PAD_PLAY_B]) ||
            (i==PAD_PLAY_B && pState[PAD_PLAY_A])) {
          pChord[PAD_PLAY_A]=true; pChord[PAD_PLAY_B]=true;
        }
        // MODE SWITCH chord (pads 9+10) — arm on press, fire after 1s hold
        // Uses same pattern as pads 1+2: set chord flag, arm timer, continue
        else if ((i==BM_SW_A && pState[BM_SW_B]) ||
                 (i==BM_SW_B && pState[BM_SW_A])) {
          pChord[BM_SW_A]=true; pChord[BM_SW_B]=true;
          bmModeArmed=true; bmModeArmMs=now;
          continue;  // skip all other pad actions
        }
        // EASTER EGG: ACID WALKS chord (pads 11+12+13+14) — arm on the 4th
        // pad press if the other three are already held, fire after 1s hold
        else if ((i==WALK_PAD_A || i==WALK_PAD_B || i==WALK_PAD_C || i==WALK_PAD_D) &&
                 pState[WALK_PAD_A] && pState[WALK_PAD_B] &&
                 pState[WALK_PAD_C] && pState[WALK_PAD_D]) {
          pChord[WALK_PAD_A]=true; pChord[WALK_PAD_B]=true;
          pChord[WALK_PAD_C]=true; pChord[WALK_PAD_D]=true;
          walksArmed=true; walksArmMs=now;
          continue;  // skip all other pad actions
        }
        // ACCENT EDIT chord (pads 15+16) — arm on 2nd pad press, fire after
        // ACCENT_HOLD_MS to toggle the CUT/RES/DECAY pots into accent-tuning mode
        else if ((i==ACCENT_PAD_A && pState[ACCENT_PAD_B]) ||
                 (i==ACCENT_PAD_B && pState[ACCENT_PAD_A])) {
          pChord[ACCENT_PAD_A]=true; pChord[ACCENT_PAD_B]=true;
          accentEditArmed=true; accentEditArmMs=now;
          continue;  // skip all other pad actions
        }
        // FUNC chord (pads 7+8)
        else if ((i==PAD_FUNC_A && pState[PAD_FUNC_B] && (now-pDown[PAD_FUNC_B])<200) ||
                 (i==PAD_FUNC_B && pState[PAD_FUNC_A] && (now-pDown[PAD_FUNC_A])<200)) {
          pChord[PAD_FUNC_A]=true; pChord[PAD_FUNC_B]=true;
          if (fxAssignMode) {
            fxAssignMode=false; fxAssignHasFx=false; funcSel=FUNC_NONE;
            ui.dirty=true; ui.fullDirty=true;
          } else if (funcMode) {
            funcMode=false; funcSel=FUNC_NONE;
            gSound = seq.sound;
            ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true; ui.infoDirty=true;
          } else {
            funcMode=true; funcSel=FUNC_NONE;
            ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true;
          }
        }
        // FX assign sub-mode
        else if (fxAssignMode) {
          if (i != PAD_FUNC_A && i != PAD_FUNC_B) doFXAssign(i);
        }
        // EASTER EGG: Acid Walks pattern select — pads 1-8 (indices 0-7)
        // load one of the 8 patterns while the value strip is shown.
        // Only applies when FUNC mode isn't active, so FUNC menu
        // selection (KEY/PAT/SOUND/etc via pads 1-16) still works normally.
        // Pads 7+8 (indices 6,7 = PAD_FUNC_A/B) are excluded here so the
        // FUNC chord can still form on the second press — otherwise the
        // first press (pad 7) would fire loadWalk(6)=SPASTK before pad 8
        // is pressed and the chord is recognised.
        else if (walksMode && !funcMode) {
          if (i < 8 && i != PAD_FUNC_A && i != PAD_FUNC_B) loadWalk(i);
        }
        // FUNC mode input
        else if (funcMode) {
          if (i == PAD_FUNC_A || i == PAD_FUNC_B) {
            // deferred to release — chord handler above
          } else if (i == PAD_PLAY_A || i == PAD_PLAY_B) {
            // deferred to release — chord handler handles both pads together
          } else if (i >= 8) {
            if (funcSel == FUNC_PLEN) {
              // Bottom-row pads 9-16 (indices 8-15) set length 9-16
              seq.len = (i - 8) + 9;
              if (seq.cur >= seq.len) seq.cur = 0;
              ui.dirty=true; ui.valDirty=true; ui.infoDirty=true;
            } else {
              doFuncSelect(i);
            }
          } else if (funcSel == FUNC_PLEN) {
            // Top-row pads 1-8 (indices 0-7) also set length 1-8
            if (!((i==PAD_PLAY_A && pState[PAD_PLAY_B]) || (i==PAD_PLAY_B && pState[PAD_PLAY_A]))) {
              seq.len = i + 1;
              if (seq.cur >= seq.len) seq.cur = 0;
              ui.dirty=true; ui.valDirty=true; ui.infoDirty=true;
            }
          } else {
            // Only apply if the other PLAY pad isn't also held (would be a PLAY chord)
            if (!((i==PAD_PLAY_A && pState[PAD_PLAY_B]) || (i==PAD_PLAY_B && pState[PAD_PLAY_A]))) {
              doFuncApply(i);
            }
          }
        }
        // Normal step input
        else {
          doPadPress(i);
        }

      } else {
        // ── PAD RELEASED ───────────────────────────────────────────
        if (pChord[i]) {
          if ((i==PAD_PLAY_A || i==PAD_PLAY_B) && pChord[PAD_PLAY_A] && pChord[PAD_PLAY_B]) {
            uint32_t holdMs = now - max(pDown[PAD_PLAY_A], pDown[PAD_PLAY_B]);
            if (holdMs >= (uint32_t)LG) {
              // Long hold — FACTORY RESET (sequencer keeps running)
              const uint8_t defNote[16] = {24,24,24,27,24,36,31,29,24,31,29,31,36,24,36,39};
              for (uint8_t s=0; s<NUM_STEPS; s++) {
                seq.steps[s].note   = defNote[s];
                seq.rootNote[s]     = defNote[s];
                seq.origNote[s]     = defNote[s];
                seq.steps[s].active = true;
                seq.steps[s].accent = false;
                seq.steps[s].glide  = false;
                seq.steps[s].effect = 0;
              }
              seq.len      = 16;   seq.tempo    = 120;
              seq.interval = bpm2us(120);
              seq.key      = 0;    seq.scale    = 0;
              seq.sound    = 0;    seq.octave   = 1;
              seq.trans    = 0;    seq.algo     = 0;
              gSound       = 0;
              kwMode=0; kwStepCount=0; kwEventCount=0; kwRoot=0; kwOctTrans=0; kwForceAccent=false; kwForceGlide=false;
              noInterrupts();
              filtA=0; filtB=0; gEnvCutoff=0;
              interrupts();
              gPorta=false; gPortaSpeed=4;
              rrMode=0; rrPingFwd=true;
              funcMode=false; funcSel=FUNC_NONE;
              fxAssignMode=false; fxAssignHasFx=false;
              ui.dirty=true; ui.fullDirty=true; ui.infoDirty=true;
            } else {
              // Short press — toggle acid PLAY/STOP.
              // Fully independent of drums: starting or stopping acid never
              // starts or stops drums (mirrored logic in BeatMachine2.ino).
              if (!acidPlaying) {
                acidPlaying = true;
                // Ensure the shared clock is ticking.
                if (!seq.running) {
                  seq.running = true;
                  seq.cur = seq.len - 1;
                  seq.lastUs = micros();
                  syncPulse = 0;
                }
              } else {
                acidPlaying = false;
                // Stopping acid only — drums are untouched and keep
                // driving the shared clock if they're still running.
                if (!bmPlaying) {
                  seq.running = false;
                }
              }
              bmAcidWasRunning = acidPlaying;
              ui.dirty=true; ui.barDirty=true; ui.infoDirty=true;
            }
          }
          pChord[i] = false;
        }
        else if (fxAssignMode) {
          if (i == PAD_FUNC_A || i == PAD_FUNC_B) doFXAssign(i);
          pCycle[i]=0; pNoteEdit[i]=false; pLastPotStep[i]=255;
        }
        else if (funcMode) {
          if (i == PAD_FUNC_A || i == PAD_FUNC_B ||
              i == PAD_PLAY_A || i == PAD_PLAY_B) {
            if (funcSel == FUNC_PLEN) {
              seq.len = i + 1;
              if (seq.cur >= seq.len) seq.cur = 0;
              ui.dirty=true; ui.valDirty=true; ui.infoDirty=true;
            } else if (funcSel != FUNC_NONE) {
              doFuncApply(i);
            }
          }
          pCycle[i]=0; pNoteEdit[i]=false; pLastPotStep[i]=255;
        }
        else if (walksMode && !funcMode && (i == PAD_FUNC_A || i == PAD_FUNC_B)) {
          // Pads 7/8 release without a FUNC chord having formed — treat as
          // a normal walks pattern-select tap (pad 7=SPASTK, pad 8=PACIFC)
          loadWalk(i);
        }
        else {
          doPadRelease(i);
        }
      }
    }

    // ── LONG-PRESS POLLS ─────────────────────────────────────────────

    // Save/load: pads 3-6 (indices 2-5) while BOTH FUNC pads (7+8) are physically held.
    // Save = hold pad 3-6 for SAVE_HOLD_MS. Load = short tap.
    // saveSlotPending is latched on first press so releasing a FUNC pad mid-hold
    // doesn't abort a save already in progress.
    if ((i==2||i==3||i==4||i==5) && pState[i] &&
        pState[PAD_FUNC_A] && pState[PAD_FUNC_B]) {
      uint8_t slot = i - 2;
      pChord[i] = true;
      if (!pLong[i]) {
        if (saveSlotPending != slot) {
          saveSlotPending           = slot;
          saveSlotDownMs            = pDown[i];
          ui.slotOverlaySlot        = slot;
          ui.slotProgressShow       = true;
          ui.slotOverlay            = false;
          ui.slotProgress           = 254;  // force prevPct mismatch so first draw fires
        }
        // Update progress bar
        uint32_t held = now - saveSlotDownMs;
        ui.slotProgress = (uint8_t)min((long)100, (long)held * 100 / SAVE_HOLD_MS);

        if (held >= (uint32_t)SAVE_HOLD_MS) {
          pLong[i]                  = true;
          saveSlotPending           = 255;
          ui.slotProgressShow       = false;
          savePatch(slot);
          ui.slotOverlay            = true;
          ui.slotOverlaySave        = true;
          ui.slotOverlayEmpty       = false;
          ui.slotOverlaySlot        = slot;
          ui.slotOverlayMs          = now;
          ui.infoDirty              = true;
        }
      }
    }
    if ((i==2||i==3||i==4||i==5) && !pState[i] &&
        saveSlotPending == (uint8_t)(i-2)) {
      ui.slotProgressShow = false;
      if (!pLong[i]) {
        uint8_t slot = i - 2;
        if (slotHasData[slot]) {
          loadPatch(slot);
          ui.slotOverlay      = true;
          ui.slotOverlaySave  = false;
          ui.slotOverlayEmpty = false;
          ui.slotOverlaySlot  = slot;
          ui.slotOverlayMs    = now;
        } else {
          ui.slotOverlay      = true;
          ui.slotOverlaySave  = false;
          ui.slotOverlayEmpty = true;
          ui.slotOverlaySlot  = slot;
          ui.slotOverlayMs    = now;
        }
        ui.infoDirty = true;
      }
      saveSlotPending = 255;
    }

    // Clear all saved ACID slots: hold pads 3+4+5+6 (indices 2-5) simultaneously
    // for 1 second, while in acid mode. Only wipes acid patches — drum patches
    // saved in the same slot numbers are untouched. (Drum mode has its own
    // equivalent clear-all in BeatMachine2.ino that only wipes drum slots.)
    if (i==2 && pState[2] && pState[3] && pState[4] && pState[5] &&
        !pLong[2] && (now - pDown[2]) > 1000) {
      pLong[2] = true;
      uint8_t invalid = 0x00;
      for (uint8_t s = 0; s < NUM_SLOTS; s++) {
        EEPROM.put(SLOT_ADDR(s), invalid);   // RAM-backed buffer only — cheap on core0
        slotHasData[s] = false;
      }
      saveCommit    = true;   // actual flash write deferred to core1 (loop1)
      lastLoadedSlot= -1;
      // Flash "ACID SLOTS CLEARED" via the same overlay path save/load use —
      // drawSlotDots()/tft.* must never run on core0 (Mozzi's control path);
      // loop1() on core1 renders this the next time it services slotOverlay.
      ui.slotOverlay        = true;
      ui.slotOverlayCleared = true;
      ui.slotOverlayMs      = now;
      ui.valDirty = true;
    }

    // FX mode: long-press pad 1 = clear all step effects
    if (fxAssignMode && i==0 && pState[i] && !pLong[i] && (now-pDown[i])>LG) {
      pLong[i] = true;
      for (uint8_t s=0; s<NUM_STEPS; s++) seq.steps[s].effect = 0;
      fxAssignHasFx = false;
      ui.dirty=true; ui.fullDirty=true;
    }

    // Normal long-press → accent/glide cycle
    // Skip if the CUT pot has moved at all since the press began — any pot
    // movement signals intent to edit the note, not toggle accent/glide,
    // even if it hasn't reached the 2-step threshold that formally engages
    // pNoteEdit yet.
    // Also skip while pads 3+4+5+6 are all simultaneously held — that's the
    // clear-all-slots combo, which needs pLong[2] to stay false until its own
    // 1-second threshold fires below. Without this exclusion, this 500ms
    // handler claims pad 3 first and permanently blocks clear-all from ever
    // triggering.
    bool clearAllChordHeld = pState[2] && pState[3] && pState[4] && pState[5];
    if (!pChord[i] && !funcMode && !fxAssignMode && !clearAllChordHeld &&
        pState[i] && !pLong[i] && !pNoteEdit[i] && (now-pDown[i])>LG &&
        noteEditPotStep == pLastPotStep[i]) {
      pLong[i] = true;
      doPadLong(i);
    }

    // Note-edit via CUT pot (normal mode only)
    if (!pChord[i] && !funcMode && !fxAssignMode &&
        pState[i] && (now-pDown[i])>200 && i<NUM_STEPS) {
      if (!pNoteEdit[i] && abs((int)noteEditPotStep - (int)pLastPotStep[i]) >= 2)
        pNoteEdit[i] = true;
      if (pNoteEdit[i] && noteEditPotStep != pLastPotStep[i]) {
        seq.steps[i].note  = noteEditPotStep;
        seq.origNote[i]    = noteEditPotStep;
        // Store key=C equivalent in rootNote so key changes work correctly after editing
        int keyDelta = (int)seq.key; if (keyDelta > 6) keyDelta -= 12;
        seq.rootNote[i]    = (uint8_t)constrain((int)noteEditPotStep - keyDelta, 0, 59);
        pLastPotStep[i]    = noteEditPotStep;
        ui.editStep = i;
        ui.dirty=true; ui.editDirty=true;
      }
    }
  }

  // Sub-step position (used by retrigger effect)
  static uint8_t lastMicron = 0;
  uint8_t micron = 0;
  if (seq.running && seq.interval > 0) {
    uint32_t elapsed = micros() - seq.lastUs;
    micron = (uint8_t)constrain((int32_t)(elapsed * 64 / seq.interval), 0, 63);
  }
  if (seq.running && seq.steps[seq.cur].active && gEffect == 2) {
    if (micron >= 32 && lastMicron < 32) {
      uint8_t rni = constrain((int)scaleNote(seq.cur) + seq.trans, 0, 59);
      triggerNote(rni, seq.steps[seq.cur].accent, false);
    }
  }
  // Stutter: re-triggers at 1/3 and 2/3 of the step — creates flam/stutter feel
  if (seq.running && seq.steps[seq.cur].active && gEffect == 3) {
    if ((micron >= 21 && lastMicron < 21) || (micron >= 42 && lastMicron < 42)) {
      uint8_t rni = constrain((int)scaleNote(seq.cur) + seq.trans, 0, 59);
      triggerNote(rni, false, false);
    }
  }
  lastMicron = micron;

  // Sync — enabled only in sync mode (pad 14 held at boot)
  bool sn = false, sr = false;
  syncOk = syncMode;  // sync active only when boot-selected

  // Sync clock tracking:
  // Measure the interval between rising edges, derive BPM, then let the
  // internal clock run steps at that tempo. This works correctly whether
  // the source sends 1 PPQN (1 pulse/quarter), 2 PPQN, 24 PPQN, etc —
  // as long as syncDiv is set to match the source's PPQN.
  //
  // syncDiv = pulses per quarter note from the source:
  //   1  = 1 PPQN  (Volca, simple clock outputs — 1 pulse per beat)
  //   2  = 2 PPQN  (some Korg gear — 1 pulse per 8th note)
  //   24 = 24 PPQN (DIN sync standard — Roland TR-808/909/TB-303)
  //
  // The measured pulse interval × syncDiv = one quarter note duration,
  // which is used to update seq.interval (= one 16th note = quarter/4).
  static uint32_t lastSyncEdgeMs = 0;
  if (syncOk && sr) {
    syncPulse++;
    if (syncPulse >= syncDiv) {
      syncPulse = 0;
      // One quarter note has elapsed — measure and update tempo
      if (lastSyncEdgeMs > 0) {
        uint32_t quarterMs = now - lastSyncEdgeMs;
        if (quarterMs > 50 && quarterMs < 3000) {  // sanity: 20-1200 BPM
          // One 16th note = quarter / 4, converted to microseconds
          seq.interval = (uint32_t)(quarterMs * 250UL);  // ms*1000/4 = ms*250
          uint16_t newTempo = (uint16_t)constrain(60000UL / quarterMs, 20, 300);
          if (newTempo != seq.tempo) {
            seq.tempo   = newTempo;
            ui.barDirty = true;
            ui.infoDirty= true;
          }
        }
      }
      lastSyncEdgeMs = now;
    }
  }

}

// =====================================================================
// MOZZI updateAudio() — 16384Hz
// =====================================================================
// Placed in RAM (not flash) — on RP2040, core 1's heavy SPI/XIP traffic during
// drawMain() can otherwise contend with core 0's flash (XIP) fetches for this
// audio-critical function, causing core 0 to occasionally miss its sample
// deadline -> an audible click/pop in the GP15 output during long redraws.
// This affects the combined mix (both GP2 drums and GP15 acid share one
// output jack), so it can sound like a "drum jump" even though the drum
// trigger timing itself (measured via Serial) is perfect.
AudioOutput __not_in_flash_func(updateAudio)() {
  if (!seq.running) return MonoOutput::from8Bit(128);
  // Produce acid audio only when acid is independently playing —
  // drums running alone should not trigger the acid oscillator.
  if (!acidPlaying) return MonoOutput::from8Bit(128);

  cnt += gFreq;
  int16_t o = 0;
  uint8_t rawOut = 128;

  int16_t effCut = constrain((int16_t)gCutoff + gEnvCutoff, 0, 255);
  int16_t effRes = constrain((int16_t)gResonance + gEnvRes, -512, 511);

  switch (gSound) {
    case 0:  // SAW + LPF
      o = 127 - (cnt >> 8);
      goto lpf;
    case 1:  // SQR + LPF
      o = (cnt < 32768) ? 100 : -100;
      goto lpf;
    case 2:  // SINE + LPF
      o = (int16_t)sinetable[cnt >> 8] - 128;
      goto lpf;
    case 3:  // NOISE + LPF
      o = (int16_t)noisetable[(cnt >> 10) & 63] - 128;
      lpf: {
        int16_t dist = o - filtA;
        filtB += dist * effCut / 256;
        filtA += filtB + dist * effRes / 256;
        filtA = constrain(filtA, -128, 127);
        filtB = constrain(filtB, -128, 127);
        int16_t _o = filtA - gVolSub + 128;
        rawOut = (uint8_t)constrain(_o, 0, 255);
      } break;

    case 4:  // CSAW + COMB
      o = 127 - (cnt >> 8);
      goto comb;
    case 5:  // CSQR + COMB
      o = (cnt < 32768) ? 100 : -100;
      goto comb;
    case 6:  // CSIN + COMB
      o = (int16_t)sinetable[cnt >> 8] - 128;
      goto comb;
    case 7:  // NOISE + COMB
      o = (int16_t)noisetable[(cnt >> 10) & 63] - 128;
      comb: {
        filtA = o + (int16_t)combBuf[(combPtr - (uint8_t)gCutoff) & 255] * (effRes - 512) / 512;
        int16_t outC = constrain(filtA - gVolSub + 128, 0, 255);
        combBuf[combPtr++] = (uint8_t)outC;
        rawOut = (uint8_t)outC;
      } break;

    case 8:  // PWM + LPF — pulse wave (variable duty cycle via phaseSw), filtered like SQR
      o = ((cnt >> 8) > phaseSw) ? 100 : -100;
      goto lpf;
    case 9: {  // Sub Square + LPF — main square layered with sub-octave square
      subCnt += subFreq;
      int16_t carrier = (cnt    < 32768) ? 100 : -100;
      int16_t sub     = (subCnt < 32768) ? 100 : -100;
      // Crossfade: mix=0 -> all carrier, mix=1023 -> all sub
      o = (int16_t)(((int32_t)carrier * (1023 - (int32_t)subMix) + (int32_t)sub * subMix) >> 10);
      goto lpf;
    }
    case 10: {  // Waveshape
      uint8_t sineIdx = (uint8_t)(((uint32_t)(cnt>>9) * (uint16_t)constrain(gResonance, 0, 1023)) >> 8);
      int16_t s = (int16_t)sinetable[sineIdx] - 128;
      filtA = (gCutoff < 256)
              ? ((s >= 0) ? s + gCutoff : s - gCutoff)
              : ((s >= 0) ? 127 - s + (511 - gCutoff) : -s - (511 - gCutoff));
      filtA = constrain(filtA, -128, 127);
      rawOut = (uint8_t)constrain(filtA - gVolSub + 128, 0, 255);
      break;
    }
    default: rawOut = 128; break;
  }

  // Post-processing effects (slots 6/7 are now pitch-modifier steps, handled at note trigger)
  switch (gEffect) {
    case 8:   // Legacy compressor (unreachable from UI)
      rawOut = compressortable[rawOut];
      break;
    case 9:   // Legacy overdrive (unreachable from UI)
      // overdrivetable removed — fall through to no-op
      break;
    case 10:  // Legacy sine modulate (unreachable from UI)
      rawOut = sinetable[rawOut];
      break;
    case 11:  // Legacy bit crush (unreachable from UI)
      rawOut &= 0xC0;
      break;
  }

  return MonoOutput::from8Bit(rawOut);
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  for (uint8_t i = 0; i < 16; i++) {
    pinMode(PAD_PINS[i], INPUT_PULLUP);
    pLast[i] = LOW;
  }

  // ── BOOT MODE DETECT ────────────────────────────────────────────────
  // Hold pad 14 (GP4) at power-on to select SYNC IN mode.
  // In sync mode: GP2 is configured as a digital input for sync clock.
  // In drum mode: GP2 is claimed by PWMAudio DMA for drum audio output.
  // The SPDT switch on the PCB physically routes GP2 to either the
  // audio node (drums) or leaves it open (sync). Hold pad 14 at boot
  // to match the switch being in SYNC position.
  delay(50);  // allow pullup to settle
  syncMode = (digitalRead(PAD_PINS[13]) == LOW);

  if (syncMode) {
    // Sync mode: configure GP2 as digital input for sync clock
    pinMode(SYNC_IN, INPUT);
    // Do NOT call bmStartAudio() — GP2 stays as input
  }
  // Drum mode: GP2 claimed by PWMAudio DMA handles GPIO setup in bmStartAudio()

  SPI.setTX(TFT_MOSI);
  SPI.setSCK(TFT_SCK);
  SPI.begin();
  tft.begin(24000000);
  tft.setRotation(3);
#ifdef PANEL_NEEDS_Y_MIRROR
  tft.sendCommand(ILI9341_MADCTL, (uint8_t[]){0x80}, 1);
#endif
  tft.fillScreen(C_BG);

  // ── ANIMATED SPLASH ─────────────────────────────────────────────────
  {
    tft.fillScreen(C_BG);

    const uint16_t acidPal[6] = {
      0x07E0,   // acid green
      0xFFE0,   // yellow
      0xFC60,   // orange
      0x07FF,   // cyan
      0xF81F,   // magenta
      0xAFE5    // lime
    };
    const uint16_t letterCol[9] = {
      acidPal[0],  // A — green
      acidPal[1],  // C — yellow
      acidPal[2],  // I — orange
      acidPal[3],  // D — cyan
      0xFFFF,      // . — white
      acidPal[4],  // D — magenta
      acidPal[5],  // R — lime
      acidPal[2],  // I — orange
      acidPal[1],  // P — yellow
    };

    const int SW2 = 7;    // stroke width
    const int LH  = 52;   // letter height
    const int BL  = 148;  // baseline Y

    auto drawA = [&](int x, uint16_t col) {
      int ax = x + 17, ay = BL - LH;
      tft.fillTriangle(ax, ay, x,       BL, x+SW2,    BL, col);
      tft.fillTriangle(ax, ay, ax+SW2,  ay, x+SW2,    BL, col);
      tft.fillTriangle(ax, ay, x+34,    BL, x+34-SW2, BL, col);
      tft.fillTriangle(ax, ay, ax+SW2,  ay, x+34-SW2, BL, col);
    };
    auto drawC = [&](int x, uint16_t col) {
      tft.fillRect(x, BL-LH,  26, SW2, col);
      tft.fillRect(x, BL-SW2, 26, SW2, col);
      tft.fillRect(x, BL-LH, SW2, LH,  col);
    };
    auto drawI = [&](int x, uint16_t col) {
      tft.fillRect(x, BL-LH, SW2+1, LH, col);
    };
    auto drawD = [&](int x, uint16_t col) {
      tft.fillRect(x,        BL-LH,     SW2, LH,      col);
      tft.fillRect(x,        BL-LH,      26, SW2,     col);
      tft.fillRect(x,        BL-SW2,     26, SW2,     col);
      tft.fillRect(x+22,     BL-LH+7,  SW2,  LH-14,  col);
      tft.fillRect(x+22+SW2, BL-LH+15, SW2-2,LH-30,  col);
    };
    auto drawDot = [&](int x, int y, uint16_t col) {
      tft.fillCircle(x, y, SW2, col);
    };
    auto drawR = [&](int x, uint16_t col) {
      tft.fillRect(x,    BL-LH,          SW2, LH,        col);
      tft.fillRect(x,    BL-LH,           24, SW2,       col);
      tft.fillRect(x,    BL-LH/2-SW2/2,   24, SW2,       col);
      tft.fillRect(x+22, BL-LH+SW2,      SW2, LH/2-SW2, col);
      tft.fillTriangle(x+SW2,    BL-LH/2+SW2, x+30,     BL, x+30+SW2, BL,       col);
      tft.fillTriangle(x+SW2,    BL-LH/2+SW2, x+SW2*2,  BL-LH/2+SW2, x+30+SW2, BL, col);
    };
    auto drawP = [&](int x, int tail_ext, uint16_t lcol, uint16_t dcol) {
      tft.fillRect(x,    BL-LH,          SW2, LH,        lcol);
      tft.fillRect(x,    BL-LH,           24, SW2,       lcol);
      tft.fillRect(x,    BL-LH/2-SW2/2,   24, SW2,       lcol);
      tft.fillRect(x+22, BL-LH+SW2,      SW2, LH/2-SW2, lcol);
      if (tail_ext > 0) {
        int ptDripX = x + SW2/2;
        tft.fillRect(ptDripX-1, BL, 3, tail_ext, dcol);
        if (tail_ext > 8) {
          int br = min((tail_ext-8)/3+2, 6);
          tft.fillCircle(ptDripX, BL+tail_ext, br, dcol);
        }
      }
    };

    // Layout (total fits 320px)
    const int GAP=9, DGAP=18;
    int xA  = 0,           xC  = xA +36+GAP,  xI1 = xC +28+GAP,
        xD1 = xI1+10+GAP,  xDt = xD1+32+DGAP, xD2 = xDt+8 +DGAP,
        xR  = xD2+32+GAP,  xI2 = xR +38+GAP,  xP  = xI2+10+GAP;
    int dtx = xP + SW2/2;

    // Phase 1: reveal letters
    uint32_t t0 = millis();
    const int LD = 100;
    auto waitUntil = [](uint32_t target) { while (millis() < target) {} };

    drawA(xA,  letterCol[0]); waitUntil(t0 + LD*1);
    drawC(xC,  letterCol[1]); waitUntil(t0 + LD*2);
    drawI(xI1, letterCol[2]); waitUntil(t0 + LD*3);
    drawD(xD1, letterCol[3]); waitUntil(t0 + LD*4);
    drawDot(xDt, BL-LH/2, letterCol[4]); waitUntil(t0 + LD*5);
    drawD(xD2, letterCol[5]); waitUntil(t0 + LD*6);
    drawR(xR,  letterCol[6]); waitUntil(t0 + LD*7);
    drawI(xI2, letterCol[7]); waitUntil(t0 + LD*8);

    // Phase 2: P appears, drip grows
    waitUntil(t0 + LD*9);
    drawP(xP, 0, letterCol[8], letterCol[8]);
    delay(250);

    // Stage A: tail grows
    for (int ext = 1; ext <= 32; ext++) {
      tft.fillRect(dtx-8, BL-1, 18, 52, C_BG);
      drawP(xP, ext, letterCol[8], letterCol[8]);
      delay(ext < 12 ? 35 : 22);
    }

    // Stage B: bulb stretches and detaches
    {
      int bulbY = BL+32, bulbR = 6, prevBulbY = bulbY;
      for (int s = 0; s < 12; s++) {
        tft.fillRect(dtx-8, BL-1, 18, prevBulbY-BL+bulbR+4, C_BG);
        tft.fillRect(xP, BL-SW2, SW2, SW2, letterCol[8]);
        int stemW = (s < 4) ? 3 : (s < 8) ? 2 : 1;
        tft.fillRect(dtx-stemW/2, BL, stemW, bulbY-BL, letterCol[8]);
        tft.fillCircle(dtx, bulbY, bulbR, letterCol[8]);
        prevBulbY = bulbY;
        bulbY += 1 + s/4;
        delay(28);
      }
      // Snap — erase stem
      tft.fillRect(dtx-2, BL, 4, prevBulbY-BL-bulbR, C_BG);
      tft.fillRect(xP, BL-SW2, SW2, SW2, letterCol[8]);
      delay(30);

      // Stage C: free fall
      float dripY = (float)prevBulbY;
      float vel   = (float)(1 + 12/4);
      const float grav = 1.18f;
      const int dripR  = bulbR;
      int prevDY = (int)dripY;

      while ((int)dripY < SH - dripR - 1) {
        int dy = (int)dripY;
        tft.fillRect(dtx-dripR-1, prevDY-15, dripR*2+3, dripR*2+20, C_BG);
        int tailLen = constrain((int)(vel*2.2f), 6, 20);
        tft.fillCircle(dtx, dy, dripR, letterCol[8]);
        tft.fillTriangle(dtx-dripR+2, dy-2, dtx+dripR-2, dy-2, dtx, dy-tailLen, letterCol[8]);
        prevDY = dy;
        vel   *= grav;
        dripY += vel;
        delay(13);
      }

      // Phase 4: impact splat
      tft.fillRect(dtx-dripR-2, prevDY-18, dripR*2+5, dripR*2+24, C_BG);
      int impY = SH - 3;

      for (int r2 = 4; r2 <= 18; r2 += 3) { tft.drawCircle(dtx, impY, r2, letterCol[8]); delay(20); }
      delay(30);
      tft.fillRect(dtx-22, impY-5, 44, 10, C_BG);

      for (int i = 0; i < 5; i++)
        tft.drawFastHLine(dtx-8-i*3, impY-1+i, (8+i*3)*2+1, acidPal[i]);

      // Particles
      const int NUM_P = 32;
      struct Particle { int16_t x,y,vx,vy; uint16_t col; uint8_t life,size; };
      Particle parts[NUM_P];

      const int8_t DX[32] = { 8, 7, 6, 5, 4, 2, 0,-2,-4,-5,-6,-7,-8,-7,-6,-5,
                               4, 3, 2, 1, 0,-1,-2,-3, 8, 6, 3,-3,-6,-8, 5,-5};
      const int8_t DY[32] = { 0,-3,-5,-6,-7,-7,-8,-7,-7,-6,-5,-3, 0, 3, 5, 6,
                              -7,-8,-7,-6,-8,-6,-7,-8,-4,-6,-8,-8,-6,-4,-5,-5};

      for (int i = 0; i < NUM_P; i++) {
        int spd = 5 + (i%5)*4;
        parts[i] = {(int16_t)(dtx*8),(int16_t)(impY*8),
                    (int16_t)(DX[i]*spd),(int16_t)(DY[i]*spd),
                    acidPal[i%6], (uint8_t)(22+(i%8)*3), (uint8_t)((i%3==0)?4:3)};
      }

      const int SAFE_Y = BL + 12;
      for (int frame = 0; frame < 45; frame++) {
        for (int i = 0; i < NUM_P; i++) {
          if (!parts[i].life) continue;
          int px=parts[i].x/8, py=parts[i].y/8;
          if (py>=SAFE_Y && px>=0 && px<SW) tft.fillCircle(px, py, parts[i].size+1, C_BG);
          parts[i].x  += parts[i].vx;
          parts[i].y  += parts[i].vy;
          parts[i].vy += 10;
          parts[i].life--;
          if (parts[i].y/8 >= SH-2 && parts[i].vy>0) parts[i].vy = -(parts[i].vy*3)/8;
          int nx=parts[i].x/8, ny=parts[i].y/8;
          if (nx>=0 && nx<SW && ny>=SAFE_Y && ny<SH && parts[i].life>0) {
            uint8_t sz = (parts[i].life>10) ? parts[i].size : parts[i].size-1;
            if (sz > 0) tft.fillCircle(nx, ny, sz, parts[i].col);
          }
        }
        delay(28);
      }
      for (int i = 0; i < NUM_P; i++) {
        int px=parts[i].x/8, py=parts[i].y/8;
        if (py>=SAFE_Y && px>=0 && px<SW) tft.fillCircle(px, py, parts[i].size+1, C_BG);
      }

      // Redraw all letters
      drawA(xA,  letterCol[0]); drawC(xC,  letterCol[1]);
      drawI(xI1, letterCol[2]); drawD(xD1, letterCol[3]);
      drawDot(xDt, BL-LH/2, letterCol[4]);
      drawD(xD2, letterCol[5]); drawR(xR, letterCol[6]);
      drawI(xI2, letterCol[7]); drawP(xP, 0, letterCol[8], 0);
    }

    // Phase 5: hold then wipe
    delay(700);
    for (int y = 0; y < SH; y += 6) { tft.fillRect(0, y, SW, 6, C_BG); delay(3); }
  }
  // ── END SPLASH ────────────────────────────────────────────────────────

  // Confirm SYNC IN mode was selected (pad 14 held at boot) before moving on
  if (syncMode) {
    tft.fillScreen(C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_CYN);
    tft.setCursor(40, 100);
    tft.print("SYNC IN ACTIVE");
    tft.setTextSize(1);
    tft.setTextColor(C_DGR);
    tft.setCursor(60, 130);
    tft.print("external clock on GP2");
    delay(1200);
    tft.fillScreen(C_BG);
  }

  memset(combBuf, 0, sizeof(combBuf));
  for (uint8_t i = 0; i < 8; i++) { vFreq[i]=287; vCnt[i]=0; }
  for (uint8_t i = 0; i < NUM_STEPS; i++) seq.steps[i].active = true;

  seq.interval = bpm2us(seq.tempo);
  seq.lastUs   = micros();
  gSound       = seq.sound;

  EEPROM.begin(EEPROM_SIZE);
  checkSlots();
  loadAccentSettings();

  drawMain();
  ui.dirty     = true;
  ui.fullDirty = false;
  ui.lastMs    = millis();

  bmInit();
  startMozzi(MOZZI_CONTROL_RATE);
  rp2040.fifo.push(1);  // signal core 1 that setup is complete
}

// =====================================================================
// LOOP — core 0
// =====================================================================
void loop() {
  audioHook();
}

// =====================================================================
// CORE 1 — Display handler
// All SPI/TFT calls happen here, never in core 0.
// =====================================================================
void setup1() {
  rp2040.fifo.pop();  // wait for core 0 setup to complete
}

void loop1() {
  // Fill drum buffer first — always top priority, must not starve
  bmFillDrumBuffer();

  // Handle mode switch signal from core 0. Drain the FIFO so any queued
  // duplicate/stale messages don't cause repeated fillScreen() calls that
  // leave the display blank for multiple loop1() passes — keep only the
  // most recent message.
  uint32_t fifoMsg;
  bool gotSwitch = false;
  uint32_t lastMsg = 0;
  while (rp2040.fifo.pop_nb(&fifoMsg)) {
    if (fifoMsg == 2 || fifoMsg == 3) { gotSwitch = true; lastMsg = fifoMsg; }
  }
  if (gotSwitch) {
    tft.fillScreen(0x0000);
    bmFillDrumBuffer();  // refill after screen clear (slow)
    if (lastMsg == 2) {
      bmFullDirty = true;
      bmDoDraw();
    } else {
      ui.fullDirty = false;  // consume the flag; drawMain() below does the work
      drawMain();
    }
    bmFillDrumBuffer();  // refill after draw
    return;
  }

  if (bmMode) {
    bmDoDraw();
    bmFillDrumBuffer();
  }
  if (saveCommit) {
    saveCommit = false;
    EEPROM.commit();
  }
  if (bmMode) return;

  // Save progress bar — only redraws when pct changes (drawSaveProgress is self-throttled)
  if (ui.slotProgressShow) {
    drawSaveProgress(ui.slotProgress);
  }
  // Save/load confirmation banner
  else if (ui.slotOverlay) {
    drawSlotOverlay();
  }
  else if (ui.fullDirty || ui.editDirty || ui.funcDirty ||
      ui.cellsDirty || ui.barDirty || ui.valDirty || ui.infoDirty) {
    ui.dirty = false;
    doDraw();
  } else {
    // Cursor tracking during playback
    uint32_t now1 = millis();
    if (seq.running && !fxAssignMode) {
      uint32_t msSinceStep = now1 - gLastStepMs;
      if (msSinceStep > 40 && msSinceStep < 80 && (now1 - ui.lastMs) > 60) {
        ui.lastMs = now1;
        static uint8_t prevCurDisp = 255;
        if (prevCurDisp != seq.cur) {
          if (prevCurDisp != 255) drawStepCell(prevCurDisp);
          drawStepCell(seq.cur);
          prevCurDisp = seq.cur;
        }
      }
    }
  }
  delay(4);
}
