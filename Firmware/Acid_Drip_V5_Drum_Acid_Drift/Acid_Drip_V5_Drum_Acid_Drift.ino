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
 *   Tap pad 3/4/5/6     : Load slot 1/2/3/4 — restores acid pattern, DRIFT,
 *                         and drum beat together, whichever of the three
 *                         actually have data saved in that slot
 *   Hold pad 3/4/5/6    : Save slot 1/2/3/4 (1 second hold) — captures
 *                         acid + DRIFT + drums together into that slot,
 *                         whatever each currently has dialled in. Same
 *                         gesture works from the drum screen too — either
 *                         screen saves/loads all three.
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
#include <math.h>   // expf/logf — ch2's decay-time-to-multiplier calc in triggerCh2Pulse()

// ── Beat Machine 2 — forward declarations ────────────────────────────
extern bool   bmMode;
extern bool   bmFullDirty;
extern void   bmInit();
extern void   bmUpdateControl();
extern void   bmDoDraw();
extern void   bmStartAudio();
int32_t       __not_in_flash_func(ch2RenderSample)();   // DRIFT render — defined in Drift.ino; RAM-resident, see there
extern bool   bmAlarmStarted;  // true once GP2 audio hardware is initialized —
                               // gates every bmFillDrumBuffer call (NOT debug)
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
extern void   bmFillDrumBufferMax();  // full pre-fill — ONLY for EEPROM.commit() (both cores
                                      // stall during flash erase, fills can't be threaded).
                                      // Never use before draws: see bmFillScreenFed()
extern void   bmPrintFed(const char* s);
extern void   bmFillScreenFed(uint16_t color);  // banded full-screen clear — ALWAYS use
                                                // instead of tft.fillScreen() once audio is
                                                // running; the atomic ~103ms clear both
                                                // underruns GP2 and XIP-stalls core 0/Mozzi  // print char-by-char, topping up GP2 between chars —
                                          // REQUIRED for any string over ~20 chars: one atomic
                                          // print() of a long string is 40-80ms of per-pixel GFX
                                          // SPI, which outspends the ring buffer in a single call
extern void   bmDrawDrumFlash();
extern uint32_t bmLastFlashDraw;
extern float    bmStoredTempo;
extern bool     bmPChord[16];
extern bool     bmPState[16];
extern bool     bmP11Deferred;
extern void   bmSavePatch(uint8_t slot);
extern void   bmLoadPatch(uint8_t slot);
extern bool   bmPatchWasPlaying(uint8_t slot);
extern void   bmStartDrums();
extern void   bmStopDrums();
extern bool   bmSlotHasData[4];
extern void   bmCheckSlots();
extern void   bmResetToBasic();

#define BM_SW_A  8   // pad 9 (index 8) — mode switch pad A
#define BM_SW_B  9   // pad 10 (index 9) — mode switch pad B
bool     bmAcidWasRunning = false;  // legacy — no longer used for logic (kept to avoid extern breakage)
// acidPlaying: true when the acid sequencer should fire notes AND render
// on GP15 (see updateAudio() — GP15 is acid-only; DRIFT lives on GP2).
// Independent of bmPlaying and of driftPlaying.
// seq.running = acidPlaying || driftPlaying || bmPlaying — the shared clock
// ticks whenever ANY module wants it, and stopping one never kills another's
// clock.
bool     acidPlaying      = false;

// driftPlaying: DRIFT's own play/stop state, exactly mirroring acidPlaying
// but for the DRIFT voice, which renders on GP2 (bmFillDrumBufferTo() in
// BeatMachine2.ino, summed with drums there) — see the DRIFT OUTPUT
// comment near ch2SynthMode's declaration. Acid and DRIFT can play
// SIMULTANEOUSLY, blended by the PCB's physical mix pot; pads 9+10+11 swap
// ch2SynthMode, which decides which one the pad/pot surface currently
// EDITS, not which one is audible. Each has its own independent play/stop
// state, which is why this can't just reuse acidPlaying: start acid, swap
// edit focus to DRIFT (9+10+11), start DRIFT too (pads 1+2) — both should
// now be audible together (mix pot permitting), and stopping one (pads 1+2
// again) must not touch the other's state.
bool     driftPlaying     = false;


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

// PATTERN CHAINING: quick double-tap of pads 12+13 (indices 11-12 — reuses
// two of the four WALK pads) toggles between:
//   - chain-build mode: pads 3-6 (slots 1-4) tap-append to a play order,
//     up to 16 positions, repeats allowed
//   - chain playback: on exit, the built chain auto-plays, advancing to
//     the next chain position after each slot's pattern completes one
//     full loop (tick-counted against seq.len, so it works under any
//     rrMode, not just plain FWD)
// Session-only — not saved to EEPROM. Tapping the gesture again at any
// time (whether currently building or currently chain-playing) drops
// whatever chain exists and starts a fresh build.
//
// This DOES collide with the WALK chord in practice, despite reusing only
// 2 of its 4 pads: a real "press all 4 together" gesture routinely lands
// 12+13 within each other's 200ms double-tap window before 11 and 14 also
// register, since WALK's own arm check only looks at whether all 4 are
// down AT THE INSTANT of a press — it has no way to tell "12+13 just
// landed, but this might still be the start of a 4-pad gesture" from "12+13
// was a deliberate standalone double-tap". CHAIN_HOLD_MS below exists
// solely to give WALK's other two pads a chance to prove it's the latter
// before CHAIN commits — see the arm/fire logic in updateControl() and the
// press-time chord check for the reasoning in full.
#define CHAIN_PAD_A WALK_PAD_B  // pad 12
#define CHAIN_PAD_B WALK_PAD_C  // pad 13
#define CHAIN_HOLD_MS 150       // short — long enough for a genuine 4-finger
                                 // WALK attempt to have landed pad 11 and/or
                                 // pad 14 too, short enough that a deliberate
                                 // double-tap toggle still feels immediate
bool     chainMode      = false;  // chain-builder UI/input active
bool     chainArmed     = false;  // 12+13 landed, waiting out CHAIN_HOLD_MS
uint32_t chainArmMs     = 0;
bool     chainActive    = false;  // a built chain is currently playing back
uint8_t  chain[16]      = {0};    // slot index (0-3) per chain position
uint8_t  chainLen       = 0;      // number of filled chain positions
uint8_t  chainPos       = 0;      // current playback position within the chain
uint16_t chainTickCount = 0;      // steps elapsed since the last chain advance

// ACCENT — was previously live-tunable (hold pads 15+16, tweak on CUT/RES/
// DECAY). Locked to a fixed value instead: the difference it made was
// small, and it freed pads 15+16 for MIX EDIT below.
#define ACCENT_CUTOFF_FIXED  40    // accent peak cutoff boost (0-255) — "low"
#define ACCENT_RES_FIXED    200    // accent peak resonance boost (0-400) — "mid"
#define ACCENT_DECAY_FIXED   10    // accent envelope decay divisor (1-20) — "mid"

// MIX EDIT: hold pads 15+16 (indices 14-15) together for ~1s to toggle a
// mode where the CUT/RES/DECAY pots trim the output level of each of the
// three synth engines independently — CUT=acid, RES=DRIFT, DECAY=drums.
// This is a DIGITAL trim on top of the physical summing pot (which only
// blends acid/GP15 against DRIFT+drums/GP2 as one combined pair) — it's
// what actually lets DRIFT be balanced against drums, which the pot alone
// can't do. Values save to EEPROM on exit, same pattern accent used to.
#define MIX_PAD_A 14  // pad 15
#define MIX_PAD_B 15  // pad 16
#define MIX_HOLD_MS 1000
bool     mixEditMode  = false;  // true while CUT/RES/DECAY pots trim engine levels
bool     mixEditArmed = false;  // chord held, waiting for MIX_HOLD_MS
uint32_t mixEditArmMs = 0;
// POT PICKUP. Without this, entering MIX EDIT snapped all three levels to
// wherever the physical pots happened to be sitting — so the saved mix you
// came in to inspect was destroyed by the act of opening the page, and you
// could not nudge one engine without the other two jumping too. Each pot
// now stays inert until it is MOVED past a threshold from its position at
// entry, so opening MIX EDIT is non-destructive and you adjust exactly the
// engine you touch. Same soft-takeover idea as the drum machine's
// bmFillPotLock/bmFillPotRef pair.
int      mixPotSnap[3] = {-1,-1,-1};   // raw pot at entry; -1 = not yet armed
bool     mixPotLive[3] = {false,false,false};
#define  MIX_PICKUP_DELTA 24           // raw ADC counts (~2.3% of travel)
// Same threshold reused by the per-engine pot ownership in updateControl().
#define  POT_PICKUP_DELTA MIX_PICKUP_DELTA
// MIX_UNITY: the raw 0-255 level value that means "unchanged, original
// volume". It is now the SAME for all three engines, so all three pots
// behave identically: unity at 2/3 travel, attenuation below, up to ~1.5x
// boost above.
//
// Acid used to be the exception — unity pinned at the TOP of its travel,
// attenuate-only — because its raw waveform rides close to full 0-255
// scale and any boost hard-clipped into buzzing. That asymmetry is why
// mixing acid felt worse than mixing drums or DRIFT: its pot had a
// different neutral position from the other two, and it could only ever
// be pulled DOWN, never pushed up against them. The fix is to give acid
// the soft-clip limiter that DRIFT and drums already have (see the acid
// gain block in updateAudio), which buys the same clean headroom they
// use — so the special case can go entirely.
#define MIX_UNITY       170   // all three engines: unity at 2/3 travel
uint8_t  mixAcidLevel  = MIX_UNITY;   // 0-255 raw pot position — persisted/displayed as-is
uint8_t  mixDriftLevel = MIX_UNITY;
uint8_t  mixDrumLevel  = MIX_UNITY;
// Derived Q8 gain constants actually applied at audio rate (256 = unity).
// Recomputed by recomputeMixGains() whenever a level above changes —
// keeps the division out of the audio-rate render paths.
uint16_t mixAcidGainQ8  = 256;
uint16_t mixDriftGainQ8 = 256;
uint16_t mixDrumGainQ8  = 256;
// SQUARE-LAW taper below unity, linear boost above it.
//
// These were linear in amplitude, which is a poor fader law because
// hearing is logarithmic: measured over acid's travel, the top HALF of
// the pot spanned only 6 dB — it felt like nothing was happening — while
// everything audible was crammed into the bottom fifth, where it dropped
// off a cliff. Squaring approximates a log taper for one multiply: half
// travel becomes -12 dB instead of -6, quarter travel -24 instead of -12.
// That spreads the useful range across the whole sweep, which is the
// single biggest reason these pots felt hard to set.
static uint16_t mixGainFor(uint8_t level, uint8_t unity) {
  if (level >= unity) {
    if (unity >= 255) return 256;                                  // no room to boost
    uint32_t over = ((uint32_t)(level - unity) * 128U) / (255U - unity);
    return (uint16_t)(256 + over);                                 // unity .. ~1.5x
  }
  uint32_t norm = ((uint32_t)level * 256U) / unity;                // 0..255 below unity
  return (uint16_t)((norm * norm) >> 8);                           // square law
}
void recomputeMixGains() {
  mixAcidGainQ8  = mixGainFor(mixAcidLevel,  MIX_UNITY);
  mixDriftGainQ8 = mixGainFor(mixDriftLevel, MIX_UNITY);
  mixDrumGainQ8  = mixGainFor(mixDrumLevel,  MIX_UNITY);
}



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

// SYNC IN pulse detection — interrupt-driven, not polled. DIN-sync-style
// clock pulses (24 PPQN, the TB-303/909 standard) are commonly only 1-2ms
// wide; polling at MOZZI_CONTROL_RATE (~3.9ms/cycle, see below) risks
// missing narrow pulses entirely, especially at higher tempos. An ISR
// catches every rising edge regardless of width or CPU timing; the
// control-rate code just checks and clears this flag once per cycle.
volatile bool gSyncPulseFlag = false;
void syncPulseISR() { gSyncPulseFlag = true; }

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
// DRIFT-EDIT step tints — subtle TEAL-shifted versions of the amber cell
// colours, shown ONLY while ch2EditMode is active (the "you are editing
// DRIFT" signal). Green raised, blue kept LOW: this is the key correction.
// The first version cooled toward blue, which collided with the azure
// trigger RING (0x04BF, blue-dominant) — a ringed step in edit was
// blue-on-blue and the ring vanished. Teal (green-dominant) is a clearly
// different hue from the ring's blue, so both read at once. Backgrounds/
// border only; rings, note bars and the bright playhead cell unchanged.
#define C_DR_BR  0x3AC5  // empty border  (teal, was C_AMBBR)
#define C_DR_BG  0x1A25  // inactive bg   (teal, was C_AMBBG)
#define C_DR_ACC 0x110C  // accent bg     (was C_MGND)
#define C_MGND  0x3009  // dark magenta — accent step cell background
#define C_CH2   0x04BF  // azure — channel-2 identity: trigger ring
                        // (drawStepCellEx) + ch2 FUNC value-strip tiles.
                        // Colour history, so nobody repeats it:
                        //  - C_ORG rejected: same 0xFC60 value as C_AMB —
                        //    an orange ring vanishes on active cells.
                        //  - violet 0xA81F rejected ON HARDWARE: it is
                        //    R21/G0/B31 vs accent magenta's R31/G0/B31 —
                        //    literally dim magenta, indistinguishable from
                        //    accent steps at arm's length.
                        // Azure (0,150,255) has ZERO red so it cannot muddle
                        // with the magenta family, sits between deep blue
                        // (0x001F, dim label borders) and cyan (0x07FF,
                        // small glide/FX dots) with clear daylight to both,
                        // and is near-complementary to amber — it pops
                        // hardest on exactly the cells that matter.

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
  // {RES min, RES max, CUT min, CUT max}. CUT min>max would REVERSE the
  // pot for that sound. Sound 7 was the only one written that way
  // (1023,0), so on NOISE+COMB alone the cutoff pot ran backwards —
  // clockwise closed the filter while every other sound opened it. Fixed
  // to 0,1023 to match the other ten. The "CUT reversed" notes on 4/5/6
  // were also wrong: those rows are 0->1020, i.e. not reversed at all.
  {718,200,  70,1020},   // 0  SAW+LPF
  {670,260,  80,670},    // 1  SQR+LPF
  {670,0,    0,1020},    // 2  SINE+LPF
  {670,270,  80,1020},   // 3  NOISE+LPF
  {1020,250, 0,1020},    // 4  CSAW  — RES reversed
  {1023,0,   0,1023},    // 5  CSQR  — RES reversed
  {1020,600, 0,1020},    // 6  CSIN  — RES reversed
  {1023,0,   0,1023},    // 7  NOISE+COMB — RES reversed (CUT was 1023,0: fixed)
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

// STEP-HIGHLIGHT TRACKING — a single shared source of truth for "which
// step cell is currently drawn as highlighted", used by drawMain()'s full
// redraw, updateMain()'s incremental chase, and loop1()'s inline chase.
// Previously these each kept their own independent static tracker, which
// desynced whenever a full redraw (e.g. an automatic chain-pattern switch)
// interrupted the normal per-tick chase: drawMain()'s 16-cell loop takes
// real time to run, and since drawStepCell() checks seq.cur live on every
// call, whichever cell coincided with seq.cur at ITS OWN draw moment got
// highlighted — but no other tracker knew about it, so it never got erased
// and stayed lit indefinitely. gDisplayCurOverride freezes seq.cur for the
// duration of drawMain()'s loop so exactly one cell is highlighted per
// pass, and gHighlightedStep is the single shared "last known highlighted
// cell" used everywhere so any tracker can correctly erase it later.
int16_t  gDisplayCurOverride = -1;   // >=0 while drawMain()'s loop is running
uint8_t  gHighlightedStep    = 255;  // 255 = none highlighted yet
bool     gSkipMainScreenClear = false;  // one-shot: skip drawMain()'s fillScreen() when
                                          // already on the main screen (no other screen's
                                          // remnants to wipe) — set before an in-place
                                          // refresh like a chain auto-advance to cut TFT time

volatile int16_t  gVolSub    = 500;
volatile int16_t  gEnvCutoff = 0;    // envelope filter boost — peaks on note-on, decays with DCY
volatile int16_t  gEnvRes    = 0;    // accent resonance envelope boost — peaks on accented note-on, decays with DCY
volatile bool     gAccentActive = false;  // true while the current note's accent envelope is decaying
volatile uint8_t  gSound     = 0;
volatile uint8_t  gEffect    = 0;
volatile int16_t  gDecaySpeed= 2;
int16_t  lastDecaySpeed = 2;  // frozen gDecaySpeed value while mixEditMode is active
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
#define EEPROM_SIZE 1064  // was 1024 — bumped again to reserve 40 fresh bytes at
                          // the tail (addresses 1024-1063) for the CUSTOM drum
                          // pattern's own persisted slot. See BM_CUSTOM_ADDR in
                          // BeatMachine2.ino — placed right at the old 1024
                          // boundary since everything below 1024 is already
                          // claimed (SLOT/ACCENT/CH2/CH2_SLOT/CH2_EUC/MIX). RP2040
                          // EEPROM emulation reserves a full flash sector
                          // regardless of the size requested here, so this is free.
#define PATCH_VALID 0xAC   // magic byte — slot has valid data
// Whether acid was playing when this slot was saved, packed into the spare
// top bit of p.rrMode (rrMode only ever uses values 0-7, bits 0-2 — see
// constrain() in loadPatch()). Deliberately NOT a new struct field: Patch
// contains a uint16_t (tempo), so appending a field grows sizeof(Patch) by
// 2 bytes (alignment padding), not 1 — which cascades forward through the
// macro-computed ACCENT_ADDR/CH2_ADDR chain and lands CH2_ADDR exactly on
// BM_EEPROM_BASE (256). Confirmed by hand: PATCH_SIZE 60->62 pushes
// CH2_ADDR from 248 to 256. See the BmPatch comment below for the same
// class of collision already hit once and reverted — not repeating it here.
#define PATCH_PLAYING_BIT 0x08

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

// Channel 2 tuning — own slot just past the accent settings, same reasoning:
// persists independently of pattern saves/loads and of accent tuning.
// Magic byte bumped 0xC2->0xC3: struct gained `sound`, so an old slot from
// before the multi-engine update is correctly treated as "no saved data"
// rather than being misread.
#define CH2_VALID   0xC3
#define CH2_ADDR    (ACCENT_ADDR + ACCENT_SETTINGS_SIZE)
struct Ch2Settings {
  uint8_t valid;
  int8_t  octave;       // -2..+2
  int16_t detuneCents;  // 0-50
  uint8_t wave;         // legacy — superseded by `sound`, kept so old field layout math stays obvious
  uint8_t sound;        // 0-7, selects ch2 synth engine — see CH2_SND_NAMES
  uint8_t amount;       // 0-255, generic live "character" knob (was the DCY-pot wave toggle)
};
#define CH2_SETTINGS_SIZE ((int)sizeof(Ch2Settings))

// CH2 settings v2 — EXTENDED blob, relocated to the tail of EEPROM.
// WHY RELOCATED: the original Ch2Settings at CH2_ADDR (248) ends at byte
// 256 — EXACTLY where BM_EEPROM_BASE starts the four drum patch slots.
// Zero headroom: growing the old struct in place would corrupt drum
// slot 1's valid byte. Drum slots span 256..399 (4 x 36), so this blob
// lives at 400 with 112 bytes of room. The legacy 248 blob is read ONCE
// as a migration source (see loadCh2Settings) and is otherwise dead
// space — do NOT reuse it for anything else.
#define CH2X_VALID  0xCC   // bumped 0xCB->0xCC: struct gained `playing` (play
                           // state on save). Confirmed by hand this doesn't
                           // change sizeof(Ch2SettingsX) — it lands in
                           // pre-existing tail alignment padding (still 78
                           // bytes) — but the byte there in OLD saves is
                           // whatever uninitialized padding happened to be
                           // in RAM at save time, not a safe false default,
                           // so old blobs must still be treated as invalid.
#define CH2X_ADDR   400
struct Ch2SettingsX {
  uint8_t  valid;        // CH2X_VALID
  int8_t   octave;       // -2..+2
  int16_t  detuneCents;  // 0-50
  uint8_t  sound;        // 0-7 engine (legacy `wave` dropped — superseded)
  uint8_t  amount;       // 0-255 per-engine character
  uint8_t  pitchMode;    // 0-7: ROOT,3RD,5TH,OCT,FOLW,ARP+,AR+-,WALK
  uint8_t  decaySel;     // 0-7 index into CH2_ENVM
  uint16_t steps;        // ch2Steps[] bitmap, bit i = step i — the pattern
                         // itself now survives power cycles
  uint8_t  echoSel;      // reserved (round 2) — 0 = OFF
  uint8_t  verbSel;      // reserved (round 2) — 0 = OFF
  uint8_t  autoRec;         // REC: bit L set = lane L has a pot-motion recording
  int8_t   stepNote[16];    // NOTE per-step pitch: chord-tone interval, 127 = none
  uint8_t  autoVal[3][16];  // REC: recorded raw>>2 per lane (OCT/AMT/DCY) per step
  uint8_t  playing;         // driftPlaying at save time — lands in what was
                             // trailing alignment padding, confirmed by hand
                             // (struct stays 78 bytes). See CH2X_VALID bump.
};
#define CH2X_SETTINGS_SIZE ((int)sizeof(Ch2SettingsX))
static_assert(sizeof(Ch2SettingsX) == 78, "Ch2SettingsX size changed — recheck 400+size <= 512");
static_assert(CH2X_ADDR + sizeof(Ch2SettingsX) <= 512, "Ch2SettingsX overruns EEPROM");

bool     slotHasData[4]  = {false,false,false,false};
int8_t   lastLoadedSlot  = -1;   // -1 = none loaded this session
volatile bool saveCommit = false;
uint8_t  saveSlotPending = 255;
uint32_t saveSlotDownMs  = 0;
#define  SAVE_HOLD_MS 1000

// Per-slot DRIFT (channel 2) storage — same Ch2SettingsX layout as the
// single always-on CH2X blob above, but one copy per numbered slot (0-3),
// living well past everything else now that EEPROM_SIZE is 1024. This is
// what makes save/load "combined": acid (SLOT_ADDR), drums (BM_SLOT_ADDR,
// in BeatMachine2.ino) and DRIFT (here) all get their own region indexed
// by the same slot number, and saveAllToSlot()/loadAllFromSlot() below
// touch all three together regardless of which screen you triggered the
// save/load gesture from.
#define CH2_SLOT_VALID   CH2X_VALID
#define CH2_SLOT_BASE    512
#define CH2_SLOT_ADDR(s) (CH2_SLOT_BASE + (s) * (int)sizeof(Ch2SettingsX))
static_assert(CH2_SLOT_BASE + 4 * (int)sizeof(Ch2SettingsX) <= EEPROM_SIZE, "ch2 slot region overruns EEPROM");
bool     ch2SlotHasData[4] = {false,false,false,false};

// PATT — user-savable custom pattern slots for the DRIFT PATT page
// (S1..S8 tiles). Originally this was EUC, a fixed floor-euclidean fill
// E(k,16), k = tile+1 — good for E5-E8 (5-8 pulses/16), rarely useful for
// E1-E4 (1-4 pulses is a pretty narrow creative space). The fixed fills are
// gone entirely now: all 8 slots start BLANK, long-holding a tile SAVES
// whatever is currently painted into ch2Steps as that slot's pattern, and
// a plain tap LOADS it back (blank if nothing's been saved there yet).
// Lives in the unused gap between the CH2_SLOT region (ends 824) and
// MIX_ADDR (900) — see the address map above.
#define CH2_EUC_VALID 0xE1
#define CH2_EUC_ADDR  824
struct Ch2EucSlots {
  uint8_t  valid;
  uint8_t  savedMask;   // bit i set = slot i holds a saved custom pattern
  uint16_t steps[8];    // custom pattern per slot (bit i = step i), valid
                         // only where savedMask's matching bit is set
};
#define CH2_EUC_SLOTS_SIZE ((int)sizeof(Ch2EucSlots))
static_assert(CH2_EUC_ADDR + sizeof(Ch2EucSlots) <= 900, "Ch2EucSlots overruns MIX_ADDR");
uint8_t  ch2EucSavedMask       = 0;        // RAM mirror of Ch2EucSlots.savedMask
uint16_t ch2EucCustom[8]       = {0};      // RAM mirror of Ch2EucSlots.steps[]

// MIX SETTINGS — per-engine output trim (acid/DRIFT/drums), tuned via
// MIX EDIT (hold pads 15+16), same slot-independent persistence pattern
// as accent settings used to be: one global value, not per-numbered-slot.
// Bumped 0xC7 -> 0xC8: acid unity moved from 255 to MIX_UNITY (170), so a
// saved acidLevel of 255 now means +1.5x boost rather than "normal". Old
// saves would load as a blaring acid channel; a new magic byte makes them
// fail validation and fall back to the (now symmetric) defaults instead.
#define MIX_VALID  0xC8
#define MIX_ADDR   900
struct MixSettings {
  uint8_t valid;
  uint8_t acidLevel;
  uint8_t driftLevel;
  uint8_t drumLevel;
};
#define MIX_SETTINGS_SIZE ((int)sizeof(MixSettings))
static_assert(MIX_ADDR + sizeof(MixSettings) <= EEPROM_SIZE, "MixSettings overruns EEPROM");

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
bool     syncMode=false;     // true = sync active this boot (either direction) —
                              // pad 14 held at power-on. GP2 is repurposed away
                              // from drum/DRIFT audio either way.
bool     syncOutMode=false;  // direction, valid only when syncMode is true:
                              // false = SYNC IN (GP2 listens for an external
                              // clock, unchanged from V4/V5). true = SYNC OUT
                              // (GP2 generates a DIN-sync-style clock from this
                              // device's own tempo) — pads 14+13 held together
                              // at power-on. See setup()'s BOOT MODE DETECT.
uint32_t syncOutPeriodUs = 0;   // current inter-pulse period being driven,
                                 // 0 = not yet set / not running — lets the
                                 // control-rate updater skip reprogramming the
                                 // PWM hardware unless the tempo has actually
                                 // moved, avoiding needless register writes.
bool     syncOutRunning = false; // whether GP2's PWM is currently emitting a
                                  // pulse train (mirrors seq.running while in
                                  // SYNC OUT mode) — see syncOutUpdate().
#define  SYNC_OUT_PPQN        24    // DIN sync standard (TB-303/808/909) — pulses
                                     // per quarter note. Matches the PPQN meaning
                                     // syncDiv already uses on the input side.
#define  SYNC_OUT_PULSE_US  2000    // fixed pulse width, regardless of tempo.
                                     // 2ms sits comfortably inside typical DIN
                                     // sync gear's expected pulse width (roughly
                                     // 1-5ms) and stays a safe fraction of the
                                     // inter-pulse period across the full 20-300
                                     // BPM range (about 1.6%-24% duty).
#define  SYNC_OUT_PWM_RANGE 4095    // analogWrite() resolution for GP2 while in
                                     // SYNC OUT mode — high enough that a 2ms
                                     // pulse is still represented precisely even
                                     // at the longest (lowest-BPM) periods.

// ── DRIFT OUTPUT ──────────────────────────────────────────────────────
// Settled behavior: ACID lives on GP15 (this file's updateAudio()), alone,
// always — same as the very first single-voice design. DRIFT lives on GP2
// instead (BeatMachine2.ino's bmFillDrumBufferTo()), additively summed with
// drums there. GP15 and GP2 meet at a shared analog audio node on the PCB,
// blended by a PHYSICAL mix pot — that pot, not any ratio in software, is
// the real-time acid/DRIFT balance control, and it's continuously
// variable rather than a fixed constant. Pads 9+10+11 swap ch2SynthMode,
// which decides which one the pad/pot SURFACE currently edits — it has no
// bearing on which is audible or on the mix balance; that's the physical
// pot's job, not this flag's.
//
// (An earlier attempt summed DRIFT digitally into THIS function instead,
// putting both engines on GP15 together at a fixed software ratio. That
// technically made them simultaneously audible, but left the physical mix
// pot with nothing to blend — it can only act on GP15 vs GP2, and once
// DRIFT was already summed into GP15 in software, there was no longer a
// separate DRIFT signal on GP2 for the pot to bring in or take away.)
//
// Drums stay additively summed with DRIFT on GP2 (not a separate output),
// so drums are present whenever the pot includes any GP2 in the blend,
// regardless of whether DRIFT itself is currently playing.

// CHANNEL 2 "DRIFT" (second synth voice) — RUNTIME-selected: hold pads
// 9+10+11 for 1s (the layer chord; 9+10 alone is drums). The old
// pad-11-at-boot selection is gone.
// ch2SynthMode means "DRIFT is the current EDIT FOCUS" — which screen is
// showing, which pads/pots are wired to which engine's parameters. It does
// NOT mean "DRIFT is audible" — that's acid's/DRIFT's own play flags
// (below) plus the physical mix pot, entirely independent of which one
// ch2SynthMode currently points at. Toggling it swaps which of
// acidPlaying/driftPlaying the pads-1+2 chord controls, but leaves the
// OTHER one's play state (and audibility) completely untouched.
volatile bool ch2SynthMode = false;  // RUNTIME edit-focus selection
                                     // (volatile: core 0 flips it
                                     // mid-session, core 1's fill loop
                                     // reads it too). No longer a boot-time
                                     // flag — selected by the 9+10(+11)
                                     // layer chord.

// seqIsRunningForDisplay(): "is the shared step position walking" from
// the perspective of the playhead cursor and DRIFT's live-play gestures
// (KBRD transpose, quantized note-record) — a UI-display question, about
// whichever engine's screen is currently showing (ch2SynthMode), NOT about
// audio mixing (that's the physical mix pot's job, independent of this
// flag). NOT the same question as seq.running, which also has to account
// for drums (bmPlaying) and BOTH engines regardless of which is on screen.
// Whichever of acid/DRIFT is currently the edit focus (ch2SynthMode) is
// the one whose own play flag answers this — the other one's flag may
// still be true (and audible, if the pot includes its output) but isn't
// what's walking the cursor on the screen you're looking at right now.
bool seqIsRunningForDisplay() {
  return ch2SynthMode ? driftPlaying : acidPlaying;
}

bool     ch2EditMode    = false;  // viewing/editing channel 2: pads toggle ch2Steps[],
                                    // CUT/RES/DCY live-control octave/detune/amount
#define LAYER_PAD_C 10            // pad 11 — the DRIFT selector in the layer chord
bool     p11Deferred    = false;  // pad 11 pressed while 9/10 held — action
                                  // deferred to release (chord may be forming)
bool     layerArmed     = false;  // pads 9+10 landed; waiting out LAYER_HOLD_MS
                                  // before firing, so pad 11 has time to land too
uint32_t layerArmMs = 0;
#define  LAYER_HOLD_MS 500        // short hold, NOT an accidental-trigger guard —
                                  // it exists purely to give pad 11 a window to
                                  // arrive after 9/10 without the chord already
                                  // having fired as DRUMS

// SECOND-LAYER chord (pads 9+10, optionally +11). Arms the instant 9+10
// complete, then fires after LAYER_HOLD_MS. WHICH engine is decided AT
// FIRE TIME by whether pad 11 is also down: 9+10 = DRUMS, 9+10+11 =
// DRIFT (channel 2) — EXCEPT from inside drums itself, where 9+10+11 is
// a deliberate no-op (see the bmMode check below). Called from both this
// file's and BeatMachine2.ino's pad scans (one translation unit) so
// either mode can trigger it identically.
void fireLayerChord() {
  bool wantDrift = (digitalRead(PAD_PINS[LAYER_PAD_C]) == LOW);
  if (wantDrift && bmMode) {
    // 9+10+11 pressed WHILE IN DRUMS — no-op, deliberately. This chord's
    // job while in drums is just the 9+10 exit-to-acid gesture; adding
    // pad 11 into that mix was jumping straight to DRIFT, which made it
    // too easy to land in the wrong place with an imprecise 3-pad press.
    // Pad 11 was chord fuel here too: eat its press (bmPChord[10]) so its
    // eventual release doesn't fall through to the deferred beat-select
    // replay (see BeatMachine2.ino's pad-scan release handler) and select
    // a pattern nobody asked for — the "PACIFIC ST turns on" symptom this
    // fixes. pChord[LAYER_PAD_C] (the ACID-side array) stays untouched;
    // it's irrelevant here since acid's own pad scan doesn't run while
    // bmMode is true.
    bmPChord[10] = true; bmP11Deferred = false;
    return;
  }
  if (wantDrift) {
    // 9+10+11 — toggle DRIFT on/off, i.e. swap which of acid/DRIFT is the
    // CURRENT EDIT FOCUS (ch2SynthMode) — which one the pad/pot surface
    // controls. This does NOT swap which is audible: acid and DRIFT can
    // both be playing and both be in the mix (blended by the PCB's
    // physical mix pot) regardless of ch2SynthMode — see the DRIFT
    // OUTPUT comment near ch2SynthMode's declaration.
    // Each engine's own play/stop state (acidPlaying / driftPlaying) is
    // untouched by this — swapping edit focus is a different action from
    // starting/stopping one (pads 1+2).
    // Pad 11 was chord fuel: eat its release and clear any pre-chord
    // residue (a note-edit it may have opened before 9/10 landed).
    pChord[LAYER_PAD_C]=true; p11Deferred=false;
    pCycle[LAYER_PAD_C]=0; pNoteEdit[LAYER_PAD_C]=false; pLastPotStep[LAYER_PAD_C]=255;
    if (!ch2SynthMode) {
      // Turning DRIFT the EDIT FOCUS — GP2 hardware is already running
      // from boot (see setup()), so there's nothing to start here; this
      // only ever changes which engine the pads/pots control, never
      // audio routing.
      ch2SynthMode = true;
      ch2EditMode  = true;
    } else {
      // DRIFT -> ACID: persist DRIFT, drop back to the acid voice/screen.
      if (ch2EditMode) saveCh2Settings();
      ch2SynthMode = false;
      ch2EditMode  = false;
    }
    ui.dirty=true; ui.fullDirty=true; ui.barDirty=true; ui.infoDirty=true;
  } else {
    // 9+10 — DRUMS. doModeSwitch() just toggles bmMode; entering or
    // leaving is never refused (drums additively share GP2 with DRIFT,
    // never displacing it, and don't touch acid on GP15 at all).
    doModeSwitch();   // sets its own dirty flags / FIFO redraw
  }
}
// Sound-engine selection lives on the CH2 FUNC page (SND, slot 1) while
// ch2EditMode is on — ch1's FUNC_SOUND is ch1's in every mode (the old
// ch2SynthMode hijack of FUNC+11 is gone). CH2 EDIT itself (this
// hold-chord) stays a plain STEP-only toggle for ch2Steps[], and yields
// to funcMode so the two don't fight over the same pads (see the
// ch2EditMode press/release branches).
bool     ch2Steps[NUM_STEPS]  = {false}; // on/off per step — the rhythmic pulse layer
// NOTE: per-step pitch for DRIFT, recorded live. Value is a CHORD-TONE
// INTERVAL INDEX (0..CH2_KB_KEYS-1 — ROOT/3RD/5TH/OCT/2ND/4TH/7TH/-OCT,
// see ch2KbIntervalSemis), so a recorded step tracks key AND chord
// changes, not a fixed pitch. Sentinel 127 = "no recorded note, follow
// the PITCH mode" (the default for every step, so existing patterns and
// ARP/WALK are unchanged). Tapping a NOTE key plays it immediately AND,
// if the sequencer is running, records it onto the step under the
// playhead — so playing a few notes builds a loop that repeats on its
// own from then on.
#define CH2_STEP_NONE 127
int8_t   ch2StepNote[NUM_STEPS] = { CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,
                             CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,
                             CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,
                             CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE,CH2_STEP_NONE };
// DRIFT is hard-locked to exactly 16 steps by TWO things: the 16 physical
// pads map 1:1 to steps (see the ch2Steps toggle in the pad handlers), and
// ch2Steps is persisted as a uint16_t bitmap (Ch2SettingsX.steps). Bumping
// NUM_STEPS above 16 would silently drop high steps on save and leave them
// unreachable from the pads; below 16 would leave pads pointing past the
// arrays. Widening DRIFT needs a pad remap AND a wider save format — until
// then this assert turns "someday" memory/data corruption into a build error.
static_assert(NUM_STEPS == 16, "DRIFT step count is locked to 16: 16 pads map 1:1 to steps and ch2Steps saves as a uint16_t bitmap.");
#define CH2_KB_KEYS   8           // top-row pads = 8 chord-tone intervals
int16_t  ch2KbNoteOverride = -1;  // >=0: a live NOTE press, exact note for the
                                  // next triggerCh2Pulse; -1 = normal (core0 only)
int8_t   ch2Octave      = -1;     // -2..+2 octaves relative to channel 1's root key, via CUT pot (live)
uint16_t ch2EnvMLive = 0;         // live per-sample decay multiplier from pot 3
                                  // (0 = not yet set; triggerCh2Pulse falls back)
uint16_t ch2DcyDisplay = 512;     // raw DCY pot (0-1023) for the DRIFT bar fill
uint16_t ch2OctDisplay = 512;     // raw OCT pot (0-1023) — bar tracks the VALUE
uint16_t ch2AmtDisplay = 512;     // raw AMT pot (0-1023)   driving the sound, so
                                  // recorded automation animates the bar and a
                                  // turned-but-recorded pot does NOT move it
int16_t  ch2DetuneCents = 18;     // 0-30 cents, LIVE on RES pot for DTSQR/UNISN only — was the
                                    // this; UNISN uses it to space its second voice off the first
uint8_t  ch2Wave         = 0;     // legacy 0/1 sub-sine/detuned-square toggle — superseded by ch2Sound,
                                    // kept only so old boards mid-flash-update don't reference a removed symbol
uint8_t  ch2Sound        = 0;     // 0-7 — selected synth engine, see CH2_SND_NAMES, chosen on the SOUND page
uint8_t  ch2Amount       = 128;   // 0-255, generic live "character" knob via DCY pot (live) — meaning is
                                    // per-engine: FM index, fold drive, ring wet/dry, PWM duty, click length...
#define  CH2_NUM_SOUNDS 16
const char* CH2_SND_NAMES[CH2_NUM_SOUNDS] = {
  "SUBSIN", "DTSQR", "FMBEL", "RINGM", "CLICK", "FOLDR", "PWM", "UNISN",
  "SAW", "SUBDIV", "NOIZ", "VOWEL", "ARPG", "GONG", "CRUSH", "GATE"
};
// CH2_SND_NAMES index reference:
//   0 SUBSIN — sub sine; ch2Amount blends in an octave-up sine (0 = pure
//              sub, max = 50/50 sub+shimmer — the sub stays the foundation)
//   1 DTSQR  — TWO detuned squares (root + ch2DetuneCents via ratio, same
//              scheme as UNISN) summed — real beating/width at any pitch
//   2 FMBEL  — 2-op FM bell/cowbell, ch2Amount = mod index (brightness)
//   3 RINGM  — ring-mod clang (inharmonic carrier x modulator), ch2Amount = wet/dry
//   4 CLICK  — sub sine + short noise-burst attack transient, ch2Amount = burst length/level
//   5 FOLDR  — wavefolded sine, ch2Amount = fold drive
//   6 PWM    — square with ENVELOPE-SWEPT duty: ch2Amount sets the base
//              width, each pulse opens ~25% wider on attack and narrows
//              into the tail — movement per hit, not a static hollow
//   7 UNISN  — two detuned sines summed (thicker sub/pad), uses ch2DetuneCents for spread
//  ── SND2 (pad 2) ──
//   8 SAW    — naive ramp; ch2Amount blends in a second detuned ramp
//              (supersaw-lite width), same trick as DTSQR/UNISN
//   9 SUBDIV — TRUE sub-octave square (divide-by-2 flip-flop) as the
//              FOUNDATION; ch2Amount stacks the root square on top
//              (octaver pedal), 0 = pure sub — never a bare sine
//  10 NOIZ   — xorshift32 noise through a one-pole colour filter;
//              ch2Amount opens it dark->bright and the note's own pitch
//              adds to the cutoff, so it plays from the keyboard
//  11 VOWEL  — fixed-ratio formant stack (root + 2 partials), ch2Amount
//              crossfades "ah"-ish -> "ee"-ish ratios
//  12 ARPG   — internal chiptune arpeggio (root/+7st/+12st cycling on its
//              own phase accumulator), ch2Amount sets the cycle speed
//  13 GONG   — inharmonic FM; ch2Amount sets the RATIO (3.6:1 bell ->
//              7.1:1 sheet metal). The index decays a quarter as fast as
//              the amplitude and never reaches zero, so the metal partial
//              genuinely outlasts the hit at every DCY setting
//  14 CRUSH  — sample-and-hold decimation of the sine (time-domain lo-fi,
//              vs FOLDR's amplitude-domain fold), ch2Amount sets hold length
//  15 GATE   — amplitude chopped by a fast internal (non-harmonic) gate,
//              ch2Amount sets the chop rate — stutter/tremolo texture

// ── CH2 FUNC PAGE ────────────────────────────────────────────────────
// In ch2SynthMode + ch2EditMode, FUNC gets channel 2's OWN 8 slots —
// mirroring how drum mode owns its FUNC set. ch1's FUNC row (KEY/RIFF/
// SOUND/...) is untouched and fully reachable whenever ch2EditMode is
// off; the old FUNC_SOUND hijack (SOUND controlling ch2Sound whenever
// ch2SynthMode was on, leaving ch1's engine unreachable all session)
// is removed — ch2 engine select now lives here as slot 0 (SND1).
// funcSel deliberately stays FUNC_NONE while ch2FuncSel is driving, so
// none of the funcSel==FUNC_PLEN / FUNC_TEMPO special cases at the pad
// call sites can misfire on ch2 slot numbers.
// SND1/SND2 split ch2Sound's 16 engines across two adjacent pads rather
// than a bank-toggle on one — tap either pad, tiles for that half light
// up directly, nothing to flip through first. NOTE (manual note entry)
// is gone — PITCH modes and PATT already cover melodic generation,
// so it had the weakest claim on the freed slot; FOLW (really the whole
// PITCH-mode page, not just literal follow) moved here from slot 1.
const char* CH2FUNCNAMES[8] = {"SND1","SND2","FOLW","ECHO","VERB","PATT","EVOL","REC"};
// DRIFT pot-3 (DCY) label per engine — its "amount" means something
// different on each: order matches ch2Sound 0-15, see CH2_SND_NAMES's
// index reference above. Kept <=4 chars for the bar-label column.
const char* CH2_AMT_LABELS[CH2_NUM_SOUNDS] = {
  "SHMR","DTUN","IDX","MIX","CLK","FOLD","DUTY","DTUN",
  "WDTH","ROOT","TONE","VOWL","SPD","METL","LOFI","CHOP"
};
// PITCH modes. First five all FOLLOW ch1's last-played note (see the
// follow branch in triggerCh2Pulse) — F+8/F-8/F+5/F+3 add a harmony
// interval on top of the follow, FOLW is the plain mirror. 5-7 are the
// key-relative arpeggio/walk modes (NOT follow-based).
//   F+8 = follow + octave up    F-8 = follow + octave down
//   F+5 = follow + fifth        F+3 = follow + third
const char* CH2_VS_PITCH[8] = {"F+8","F-8","F+5","F+3","FOLW","ARP+","AR+-","WALK"};
// KBRD key labels — chord-tone intervals, see ch2KbIntervalSemis(). Kept
// as its own table (rather than reusing CH2_VS_PITCH) since KBRD has no
// FOLW/ARP+/AR+-/WALK slots — it's 8 playable/recordable intervals.
const char* CH2_KB_NAMES[8]  = {"ROOT","3RD","5TH","OCT","2ND","4TH","7TH","-OCT"};
const char* CH2_VS_DCY[8]   = {"25","60","120","250","400","700","1.2S","2S"};
// Per-sample envelope multipliers for {25,60,120,250,400,700,1200,2000}ms
// decay-to--60dB at 16384Hz: envM = 65536*exp(ln(0.001)/(t*16384)).
// Precomputed — the old code ran expf() on every trigger for no reason.
const uint16_t CH2_ENVM[8]  = {64440,65077,65306,65426,65467,65497,65513,65522};
uint8_t ch2FuncSel   = 255;  // 255 = none; 0-7 = CH2FUNCNAMES index
uint8_t ch2PitchMode = 0;    // 0-7, see CH2_VS_PITCH
uint8_t ch2DecaySel  = 4;    // default 400ms — matches the old fixed feel
uint8_t ch2ArpPos    = 0;    // ARP+/AR+- cycle position, advances per trigger
// Base phase the arp cycle restarts from at the top of each pattern cycle.
// Two jobs. (1) Repeatability: ch2ArpPos free-ran, so unless the hit count
// per bar happened to be a multiple of the cycle length the tone landing on
// a given step drifted every bar and never lined up twice. (2) It's the ONLY
// way the auto-walk can affect ARP+ at all — ARP+ generates pitch from a
// counter and never reads ch2StepNote[], so rotating the pattern is
// inaudible to it. Advancing this each time the walk fires makes the walk
// rotate the TONE ASSIGNMENT instead. Indexing the arp by seq.cur cannot
// work: the cycle is 4 long and every beat-quantized rotation is a multiple
// of 4, so the tone at each step would be unchanged by every walk amount.
// Kept modulo 12 = lcm(4,6) so a wrap is seamless for both ARP+ and AR+-.
uint8_t ch2ArpPhase  = 0;
int8_t  ch2WalkDeg   = 0;    // WALK: current scale-degree offset

// ── CH2 ECHO ─────────────────────────────────────────────────────────
// Tempo-synced delay on the ch2 voice only, rendered on core 1 inside
// bmFillDrumBufferTo() (see the echo block there). Division presets, not
// milliseconds — free-time echo fights a step sequencer's groove.
// 16KB buffer = 500ms ceiling: a 1/4-note delay is exact down to 120 BPM
// and clamps to ~500ms below that (still musical, just no longer a true
// quarter). Power-of-two length so core 1 wraps with a mask, not a div.
// If the linker ever reports RAM overflow, this buffer is the first
// thing to shrink — drop to 4096/int16 and accept the 250ms ceiling.
#define CH2_ECHO_LEN 16384           // samples; MUST stay a power of two.
                                     // Doubled from 8192 when the preset set
                                     // gained 4D and 1/2 divisions: a half-
                                     // note at 120 BPM is a full second, and
                                     // a tile that silently clamps to half
                                     // its labeled time is a lie. 32KB —
                                     // the largest RAM object in the build;
                                     // still the first thing to shrink if
                                     // the linker reports overflow.
int16_t  ch2EchoBuf[CH2_ECHO_LEN];   // 16KB BSS — zeroed at boot
volatile uint16_t ch2EchoDelay = 0;  // samples, 0 = echo off; core0 writes, core1 reads
volatile uint8_t  ch2EchoFb    = 0;  // feedback, Q8 (x/256)
uint8_t  ch2EchoSel = 0;             // 0-7, see CH2_VS_ECHO
// 16TH and 8TH removed after hardware testing: at typical tempos those
// delays (125/250ms) land UNDER the pulse's own decay tail (default
// 400ms) — the repeat is masked by the still-ringing source. Not a bug,
// physics: echoes shorter than the source's decay hide inside it.
// Replaced with LONGER divisions (4D, 1/2) that ring into clear air.
// 8FB kept: an 8th WITH feedback repeats past the tail, so it was
// always audible.
const char* CH2_VS_ECHO[8] = {"OFF","8D","1/4","4D","1/2","8FB","4FB","2FB"};

// ── CH2 VERB ─────────────────────────────────────────────────────────
// Lo-fi Schroeder reverb on the ch2 voice only, rendered on core 1 after
// the echo (repeats get reverberated) and before bmSoftClip. 4 parallel
// feedback combs with one-pole damping in each loop → 2 series allpasses.
// Comb lengths are PRIMES scaled from Schroeder's classic 30-41ms to
// 16384Hz — mutually prime so the resonant modes interleave instead of
// stacking into one metallic pitch. At this rate/width it is knowingly
// grainy and dark — dusty-spring territory, not a hall; that's the
// instrument's aesthetic, accepted up front.
#define CH2_VERB_C0 487
#define CH2_VERB_C1 541
#define CH2_VERB_C2 641
#define CH2_VERB_C3 677
#define CH2_VERB_A0 83
#define CH2_VERB_A1 29
#define CH2_VERB_DAMP 112   // Q8 one-pole damping in each comb loop (~0.44)
struct Ch2VerbBufs {        // one struct so BeatMachine2 externs ONE symbol
  int16_t c0[CH2_VERB_C0], c1[CH2_VERB_C1], c2[CH2_VERB_C2], c3[CH2_VERB_C3];
  int16_t a0[CH2_VERB_A0], a1[CH2_VERB_A1];
};
Ch2VerbBufs ch2VerbBufs;                  // ~4.9KB BSS — zeroed at boot
volatile uint8_t ch2VerbFbQ8  = 0;        // comb feedback (room size), Q8
volatile uint8_t ch2VerbWetQ8 = 0;        // wet mix, Q8; 0 = verb off entirely
uint8_t ch2VerbSel = 0;                   // 0-7, see CH2_VS_VERB
const char* CH2_VS_VERB[8] = {"OFF","SML","MED","BIG","HALL","MEDW","BIGW","WASH"};

// ── CH2 EUC / HARM / STUT — sequencer-side, no DSP ───────────────────
// EUC: per-slot user pattern loader, WRITTEN INTO ch2Steps — destructive
// on purpose, it's a groove preset loader. All 8 slots start blank; hold a
// tile to save the current ch2Steps into it, tap to load it back (see
// ch2LoadEucSlot/Ch2EucSlots above). ch2EucSel is display-only (highlights
// the last slot loaded/saved this session); the PATTERN is what persists,
// via ch2EucCustom[]/Ch2EucSlots in EEPROM.
uint8_t ch2EucSel  = 255;  // 255 = no highlight
const char* CH2_VS_EUC[8]  = {"S1","S2","S3","S4","S5","S6","S7","S8"};
// EUC hold-to-save gesture, one tile at a time — heldMs/ch2GestPad/
// ch2GestArmed (declared just below) are shared across every ch2 FUNC page.
#define CH2_EUC_SAVE_HOLD_MS 600
#define CH2_EUC_FLASH_MS     300
int8_t   ch2EucFlashTile = -1;     // tile to pulse white after a save, -1 = none
uint32_t ch2EucFlashMs   = 0;
// ── CH2 LIVE PLAYABILITY ─────────────────────────────────────────────
// A HELD key on the KBRD page transposes the running DRIFT line — you
// perform, live, without touching the recorded pattern. ch2GestSince/
// ch2GestPad/ch2GestArmed time every top-row hold and are shared across
// every ch2 FUNC page (KBRD transpose, EUC/PATT's hold-to-save, formerly
// HARM's bar-count programming) — one hold-timer, many pages reading it.
int16_t  ch2XposeSemis   = 0;    // live transpose applied to DRIFT playback
int8_t   ch2XposeKey     = -1;   // top-row pad driving it, -1 = none
uint32_t ch2GestSince    = 0;    // when the currently-held top-row pad went down
int8_t   ch2GestPad      = -1;   // which top-row pad is being timed
bool     ch2GestArmed    = false;// this hold already fired its one-shot action
// EVOLVE v2: replaces the v1 shadow-array mutator entirely (swap/nudge/
// duplicate/remove — see git history). v1 mutated a persistent shadow copy
// of the pattern one small step at a time, independently per mutation
// event; it never converged on anything predictable, and there was no way
// to get "this specific thing happens, then reverts, then a different
// specific thing happens" out of it — every mode was the same generic
// algorithm at a different dial setting.
//
// v2 is a ROOT/EXCURSION cadence instead: the pattern plays exactly as
// recorded for a fixed number of bars (root), then a mode-specific effect
// applies for a fixed number of bars (excursion), then back to root, on
// repeat. Each of the 7 presets is now a genuinely distinct, named effect
// (octave shifts, an extra step, a stutter, or combinations), not one
// algorithm scaled by intensity.
//
// Implemented as a LIVE OVERLAY, same principle as the old HARM offset and
// ch2XposeSemis: nothing in ch2Steps/ch2StepNote is ever touched. This
// replaces v1's shadow-copy approach (ch2EvoSteps/ch2EvoStepNote/
// ch2EvoAdded, all gone) with something structurally simpler — an
// excursion's parameters are computed fresh when it STARTS and just stop
// being read when it ENDS, so there's no persistent divergent state to
// resync or accidentally leave stale. That simplicity is deliberate: v1
// shipped two real bugs from having too much state to keep synchronized.
uint8_t ch2EvolSel = 0;    // 0 = OFF, 1-7 = mode preset, see CH2_VS_EVOL
const char* CH2_VS_EVOL[8] = {"OFF","OCTL","ADD1","DEEP","STUT","HVY","WILD","MAX"};

// What an excursion actually DOES. Two independent dimensions, either or
// both active per mode:
//  - Octave overlay (ch2EvoOctOffset, semitones) — added to every pitch
//    path in triggerCh2Pulse, same guard ch2XposeSemis uses (skipped for a
//    live NOTE-key press — you pressed that note, EVOLVE doesn't get to
//    override your hands).
//  - Extra step (ch2EvoExtraStep/ch2EvoExtraNote) — one specific step,
//    chosen fresh each excursion, force-triggers for the excursion's
//    duration with a chosen note, whether or not that step was originally
//    on. STUT reuses the exact same mechanism but always targets the step
//    immediately after an already-active one, so it reads as a ratchet
//    rather than a fill appearing from nowhere.
#define CH2_EVO_KIND_OCT_ALT   1   // octave shift, alternates/varies each excursion
#define CH2_EVO_KIND_ADD_STEP  2   // one extra step appears, reverts after
#define CH2_EVO_KIND_OCT_DEEP  3   // octave shift, always DOWN, amount varies
#define CH2_EVO_KIND_STUTTER   4   // duplicate an active step onto the next one
#define CH2_EVO_KIND_ADD_OCT   5   // extra step AND octave shift together (HVY/MAX)
#define CH2_EVO_KIND_RANDOM    6   // WILD — fresh random pick each excursion among
                                    // oct-up / oct-down / extra-step
struct Ch2EvoPreset { uint8_t rootBars; uint8_t excBars; uint8_t kind; };
const Ch2EvoPreset CH2_EVO_PRESET[8] = {
  {1, 1, 0},                              // 0 OFF — unused, guarded out before lookup
  {3, 1, CH2_EVO_KIND_OCT_ALT},            // 1 OCTL — 3 bars root, 1 bar octave lift, repeat
  {3, 1, CH2_EVO_KIND_ADD_STEP},           // 2 ADD1 — one extra step, mostly root
  {3, 1, CH2_EVO_KIND_OCT_DEEP},           // 3 DEEP — occasional low octave dip
  {2, 1, CH2_EVO_KIND_STUTTER},            // 4 STUT — a ratchet every 3rd bar
  {2, 1, CH2_EVO_KIND_ADD_OCT},            // 5 HVY  — extra step + octave, every 3rd bar
  {1, 1, CH2_EVO_KIND_RANDOM},             // 6 WILD — random pick, every other bar
  {1, 1, CH2_EVO_KIND_ADD_OCT},            // 7 MAX  — extra step + octave, every other bar
};

bool     ch2EvoInExcursion = false;   // which segment of the cadence we're in
uint16_t ch2EvoStepsInSeg  = 0;       // steps elapsed in the current segment,
                                       // vs (rootBars or excBars)*16 — same
                                       // unconditional-count-then-act-at-lap-
                                       // end shape HARM's auto-walk used.
int8_t   ch2EvoOctOffset   = 0;       // live semitone offset, 0 = root/no effect
int8_t   ch2EvoExtraStep   = -1;      // step index force-triggered this excursion, -1 = none
int8_t   ch2EvoExtraNote   = CH2_STEP_NONE;  // the note that forced step plays
int8_t   ch2EvoLastOctDir  = 0;       // previous excursion's octave choice, so
                                       // "a slightly different note next time"
                                       // doesn't just repeat the same one twice
int8_t   ch2EvoLastExtraStep = -1;    // previous excursion's extra-step choice,
                                       // same reasoning — vary it, don't repeat

// Which source triggerCh2Pulse/the trigger gate should read. No shadow copy
// in v2 — ch2Steps/ch2StepNote are read directly; the only overlay is one
// possible FORCED extra step during an excursion, computed fresh each time
// (see ch2EvoStartExcursion below), never stored.
inline bool ch2EvoStepOn(uint8_t i) {
  if (ch2Steps[i]) return true;
  return ch2EvolSel && ch2EvoInExcursion && ch2EvoExtraStep == (int8_t)i;
}
inline int8_t ch2EvoNoteAt(uint8_t i) {
  if (ch2Steps[i]) return ch2StepNote[i];   // recorded note always wins
  if (ch2EvolSel && ch2EvoInExcursion && ch2EvoExtraStep == (int8_t)i) return ch2EvoExtraNote;
  return CH2_STEP_NONE;
}

// Called once, the instant an excursion BEGINS (root -> excursion). Picks
// this excursion's octave offset and/or extra step based on the mode's
// kind, biased away from repeating whatever the previous excursion did —
// "changes again to a slightly different note" rather than the same
// change every time. Nothing here persists past the excursion; there's
// nothing to undo when it ends, only overlay reads to stop happening.
void ch2EvoStartExcursion() {
  ch2EvoOctOffset = 0; ch2EvoExtraStep = -1; ch2EvoExtraNote = CH2_STEP_NONE;
  uint8_t kind = CH2_EVO_PRESET[ch2EvolSel].kind;

  bool wantOct = (kind == CH2_EVO_KIND_OCT_ALT || kind == CH2_EVO_KIND_OCT_DEEP ||
                  kind == CH2_EVO_KIND_ADD_OCT);
  bool wantAdd = (kind == CH2_EVO_KIND_ADD_STEP || kind == CH2_EVO_KIND_ADD_OCT);
  bool wantStut = (kind == CH2_EVO_KIND_STUTTER);

  if (kind == CH2_EVO_KIND_RANDOM) {   // WILD — fresh 3-way pick every time
    uint8_t r = (uint8_t)random(3);
    wantOct = (r != 2); wantAdd = (r == 2);
  }

  if (wantOct) {
    int8_t dir;
    if (kind == CH2_EVO_KIND_OCT_DEEP) {
      // always down, but vary the AMOUNT (1 or 2 octaves) so it's not the
      // exact same dip every time
      dir = (ch2EvoLastOctDir == -1) ? -2 : -1;
    } else {
      // alternate up/down, one octave each way — avoid repeating last time
      dir = (ch2EvoLastOctDir >= 0) ? -1 : 1;
      if (kind == CH2_EVO_KIND_RANDOM) dir = random(2) ? 1 : -1;  // WILD: no memory bias
    }
    ch2EvoOctOffset  = (int8_t)(dir * 12);
    ch2EvoLastOctDir = dir;
  }

  if (wantAdd) {
    uint8_t cand[NUM_STEPS], n = 0;
    for (uint8_t i = 0; i < NUM_STEPS; i++)
      if (!ch2Steps[i] && (int8_t)i != ch2EvoLastExtraStep) cand[n++] = i;
    if (n == 0)   // everything's on, or the only free slot was last time's — allow a repeat
      for (uint8_t i = 0; i < NUM_STEPS; i++) if (!ch2Steps[i]) cand[n++] = i;
    if (n > 0) {
      uint8_t pick = cand[random(n)];
      ch2EvoExtraStep    = (int8_t)pick;
      ch2EvoLastExtraStep = (int8_t)pick;
      // borrow the note from the nearest active step so the extra hit is
      // harmonically related to what's actually playing, not an arbitrary
      // interval — falls back to ROOT if the pattern has no active steps at all
      int8_t note = CH2_STEP_NONE;
      for (uint8_t d = 1; d < NUM_STEPS && note == CH2_STEP_NONE; d++) {
        uint8_t a = (pick + d) % NUM_STEPS, b = (pick + NUM_STEPS - d) % NUM_STEPS;
        if (ch2Steps[a] && ch2StepNote[a] != CH2_STEP_NONE) note = ch2StepNote[a];
        else if (ch2Steps[b] && ch2StepNote[b] != CH2_STEP_NONE) note = ch2StepNote[b];
      }
      ch2EvoExtraNote = (note == CH2_STEP_NONE) ? 0 : note;   // 0 = ROOT
    }
  }

  if (wantStut) {
    uint8_t cand[NUM_STEPS], n = 0;
    for (uint8_t i = 0; i < NUM_STEPS; i++) {
      uint8_t nxt = (i + 1) % NUM_STEPS;
      if (ch2Steps[i] && ch2StepNote[i] != CH2_STEP_NONE && !ch2Steps[nxt]) cand[n++] = i;
    }
    if (n > 0) {
      uint8_t src = cand[random(n)];
      ch2EvoExtraStep = (int8_t)((src + 1) % NUM_STEPS);
      ch2EvoExtraNote = ch2StepNote[src];
    }
  }
  ui.dirty = true;   // no per-cell marker currently renders EVOLVE's excursion
                      // state (ch2EvoExtraStep/ch2EvoInExcursion are read-only
                      // audio-timing flags — see ch2EvoStepOn/ch2EvoNoteAt, not
                      // drawn anywhere in drawStepCellEx), so there's nothing
                      // on screen that actually needs repainting here. This
                      // used to also set cellsDirty/cellIdx=0, which swept a
                      // full 16-cell redraw across whichever screen was up —
                      // acid's grid or DRIFT's — every time EVOLVE flipped
                      // segments (periodic, tied to the pattern loop). That's
                      // the visible "steps redrawing after every loop" you'd
                      // see with EVOLVE on, for no actual visual change.
}

// Called once, the instant an excursion ENDS (excursion -> root). Nothing
// to restore — the overlay reads (ch2EvoStepOn/ch2EvoNoteAt/the octave add
// in triggerCh2Pulse) all already gate on ch2EvoInExcursion, so clearing
// the flag is the entire revert. This is the payoff of not having a
// shadow copy: reverting can't desync because there's nothing to desync.
void ch2EvoEndExcursion() {
  ch2EvoOctOffset = 0; ch2EvoExtraStep = -1; ch2EvoExtraNote = CH2_STEP_NONE;
  ui.dirty = true;   // see ch2EvoStartExcursion() above — same reasoning,
                      // no cellsDirty sweep needed, nothing visual to revert.
}
// Force back to the ROOT segment and zero the cadence counter. Called
// everywhere the real pattern changes under EVOLVE's feet (record, clear,
// PATT load, patch load, manual grid edit) or EVOLVE turns on from OFF —
// same call sites v1's ch2EvoResync() used, same reasoning (don't let
// state computed against the OLD pattern keep being read against a new
// one), just a much smaller thing to reset now that there's no shadow copy.
void ch2EvoResetCadence() {
  if (ch2EvoInExcursion) ch2EvoEndExcursion();
  ch2EvoInExcursion = false;
  ch2EvoStepsInSeg  = 0;
}


// Hold threshold. Must clear a normal record-tap on the KBRD page
// (~50-100ms) so a quick note entry doesn't blip the line.
#define CH2_XPOSE_HOLD_MS   180
// NOTE page press-flash. Declared up here rather than beside ch2NotePlay()
// because ch2PollLiveGestures() (below, but earlier in the file) now sets
// them too — a global has to be declared before its first use in the TU.
uint32_t gCh2NotePressMs  = 0;   // millis() of the last NOTE key press
uint8_t  gCh2NotePressIdx = 0;   // which key, for the press-flash above

// ── CH2 REC (pot-motion recorder) — TOGGLE model. Short-tap the REC pad
// (16) to start/stop recording; long-press to clear. While recording,
// any pot you move arms its lane and its motion is captured per step.
bool ch2Recording = false;

// ── POT-MOTION RECORDER ──────────────────────────────────────────────
// Three lanes matching the DRIFT pots: 0=OCT(cut), 1=AMT(res), 2=DCY.
// Hold pad 16 (REC) and move a pot: that lane arms on movement and its
// value is written to the current playhead step each control tick, so one
// loop of the pattern captures the motion. Release REC and recorded lanes
// play back per step (advanceStep) while un-recorded pots stay live. Re-tap
// the REC pad to clear all lanes. Stored as raw pot >> 2 (0-255/step).
uint8_t  ch2AutoVal[3][NUM_STEPS] = {{0}};  // recorded value per lane per step
uint8_t  ch2AutoRec   = 0;           // bit L set = lane L has a recording
uint8_t  ch2RecActive = 0;           // bit L set = actively recording now (transient)
int      ch2RecSnap[3] = {-1,-1,-1}; // raw pot at REC-arm, for move detection
bool     ch2PrevRecArmed = false;
uint32_t gCh2RecClrFlashMs = 0;      // clear-all ack flash timer

// Apply a raw pot value (0-1023) to a DRIFT lane's parameter. Shared by
// live reads, recording, and playback so all three map identically.
void ch2ApplyPotRaw(uint8_t lane, int raw) {
  raw = constrain(raw, 0, 1023);
  switch (lane) {
    case 0: ch2Octave = (int8_t)((raw * 5L) / 1024) - 2; ch2OctDisplay = (uint16_t)raw; break;
    case 1:
      if (ch2Sound == 1 || ch2Sound == 7) ch2DetuneCents = (int16_t)((raw * 30L) / 1023);
      else                                ch2Amount      = (uint8_t)(raw >> 2);
      ch2AmtDisplay = (uint16_t)raw;
      break;
    case 2: {                                                     // log decay 25ms..2s
      float t = 0.025f * powf(80.0f, (float)raw / 1023.0f);
      ch2EnvMLive   = (uint16_t)(65536.0f * expf(logf(0.001f) / (t * (float)AUDIO_RATE)));
      ch2DcyDisplay = (uint16_t)raw;
    } break;
  }
}

// Per-step automation playback: for each recorded lane not being actively
// recorded, apply its stored value for the current step. Runs whenever
// DRIFT plays (edit mode or under the acid screen).
void ch2AutoPlayback() {
  if (!ch2AutoRec) return;
  for (uint8_t L = 0; L < 3; L++)
    if ((ch2AutoRec & (1 << L)) && !(ch2RecActive & (1 << L)))
      ch2ApplyPotRaw(L, (int)ch2AutoVal[L][seq.cur] << 2);
}

// Last note ch1 actually SOUNDED (0-59 noteFreq index), captured inside
// triggerNote() so it includes ratchets, stutters, and effect-transposed
// pitches (Oct Up, chord-step arps). -1 until the first audible note.
// FOLW pitch mode reads this instead of the current step's STORED note,
// so ch2 harmonizes with what you hear — a silent ch1 step no longer
// yanks ch2 to a pitch that was never played.
// Last note index ch1 ACTUALLY fired (captured in triggerNote(), the one
// choke point every ch1 voice pass goes through — steps, riffs, walks).
// -1 until the first note of the session; FOLW falls back to the current
// step's stored note only in that window. int16_t on purpose: a uint8_t
// with a 255 sentinel would make the >=0 check always-true and pin FOLW
// to a garbage top note before ch1 ever plays.
int16_t gLastCh1Note = -1;

// Publish verb params for the current preset. No tempo dependency, so
// unlike echo this only needs calling from apply + load — not per trigger.
void ch2ApplyVerb() {
  // WET retuned upward alongside the structural sum>>2 -> sum>>1 fix in
  // the DSP (see BeatMachine2): the original scaling left the wet path
  // ~30dB below dry — inaudible under a playing pattern. Now lands
  // roughly -15dB (SML) to -6dB (WASH): present without swamping.
  //                     OFF SML MED BIG HALL MEDW BIGW WASH
  static const uint8_t FB[8]  = {0,154,184,205,220, 184, 205, 225};
  static const uint8_t WET[8] = {0, 96,120,140,140, 190, 190, 240};
  uint8_t s = ch2VerbSel & 7;
  ch2VerbFbQ8  = FB[s];
  ch2VerbWetQ8 = WET[s];
}
volatile uint32_t ch2Phase    = 0;      // oscillator phase accumulator
volatile uint32_t ch2PhaseInc = 0;      // phase increment per sample, set on trigger
volatile uint16_t ch2Env      = 0;      // 8.8 fixed-point envelope level — 0 = silent
volatile uint16_t ch2EnvM     = 65490;  // per-sample decay multiplier (~400ms to near-silent at 16384Hz)
volatile uint32_t ch2DetuneRatioQ16 = 65536;  // Q16 second-voice ratio (1 + cents/1731), set every
                                              // trigger — shared by BOTH dual-osc engines (DTSQR, UNISN)
volatile uint8_t  ch2ClickCount = 0;     // CLICK: samples of noise-burst attack remaining
volatile uint8_t  ch2NoiseIdx   = 0;     // CLICK + NOIZ: running index into noisetable[] for the burst
#define  CH2_CLICK_LEN_MAX 24            // CLICK: longest burst at ch2Amount==255, ~1.5ms @16384Hz
// ── SND2 (engines 8-15) new persistent state — see Drift.ino's engine
// switch for how each is used, and triggerCh2Pulse() for where they're
// reset. Same cost class as ch2ClickCount/ch2NoiseIdx above.
volatile uint8_t  ch2SubDivFlip = 0;     // SUBDIV: divide-by-2 flip-flop
volatile int16_t  ch2CrushHold  = 0;     // CRUSH: currently-held sample value
volatile uint8_t  ch2CrushCtr   = 0;     // CRUSH: samples remaining before the next hold
volatile uint32_t ch2ArpgPhase  = 0;     // ARPG: its own phase accumulator
volatile uint8_t  ch2ArpgStep   = 0;     // ARPG: which of the 3 ratios is current (0-2)
volatile uint16_t ch2ArpgCtr    = 0;     // ARPG: samples remaining at the current step (needs >255)
volatile uint16_t ch2GongIdx    = 0;     // GONG: independent FM-index decay
volatile uint32_t ch2NoiseLfsr  = 0x1234567;  // NOIZ: xorshift32 noise state (never 0)
volatile int16_t  ch2NoiseLp    = 0;     // NOIZ: one-pole lowpass state (colour filter)
volatile uint32_t ch2NoisePhase = 0;     // NOIZ: clock accumulator — sets noise pitch from the note
volatile uint32_t ch2VowP1      = 0;     // VOWEL: formant-1 phase (absolute Hz, not a ratio)
volatile uint32_t ch2VowP2      = 0;     // VOWEL: formant-2 phase
volatile uint32_t ch2GatePhase  = 0;     // GATE: chop phase, free-running and pitch-independent
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
  gLastCh1Note = freqIdx;  // FOLW source — see declaration
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
  gEnvCutoff = accent ? ACCENT_CUTOFF_FIXED : 30;  // moderate brightness lift — avoid pushing cutoff to its extreme
  gEnvRes    = accent ? ACCENT_RES_FIXED    : 0;   // gentle resonance nudge — avoid pushing into self-oscillation
  gAccentActive = accent;  // accented notes get a slower filter-envelope decay (303-style "wah")
  gLastStepMs= millis();
}

// CHANNEL 2 — a rhythmic pulse layer, not a melodic voice. Fires from
// advanceStep() whenever ch2Steps[seq.cur] is on, independent of whatever
// channel 1 is doing on that same step (channel 2 has no notes of its own
// to trigger — see triggerCh2Pulse() below). Pitch always tracks channel
// 1's current root key, so changing key never leaves channel 2 clashing.
// Publish echo params for the current preset + CURRENT tempo. Called from
// ch2FuncApply (immediate response to a tile press) and from every
// triggerCh2Pulse (so tempo changes — FUNC TEMPO, tap tempo — retune the
// delay time without any extra plumbing at the tempo call sites; between
// triggers the tail simply rings at the old time, which is fine).
// One integer divide per trigger; core 1 reads the two volatiles.
void ch2ApplyEcho() {
  // preset:            OFF  8D 1/4  4D 1/2  8FB 4FB 2FB
  static const uint8_t M[8]  = {0,  3,  4,  6,  8,  2,  4,  8};  // x 16th
  static const uint8_t FB[8] = {0, 76, 76, 76, 76,168,168,168};  // Q8
  uint8_t s = ch2EchoSel & 7;
  if (M[s] == 0) { ch2EchoDelay = 0; ch2EchoFb = 0; return; }
  // samples per 16th at AUDIO_RATE 16384: (60/tempo/4)*16384 = 245760/tempo
  uint32_t d = ((uint32_t)M[s] * 245760UL) / (uint32_t)seq.tempo;
  ch2EchoDelay = (uint16_t)constrain(d, (uint32_t)1, (uint32_t)(CH2_ECHO_LEN - 1));
  ch2EchoFb    = FB[s];
}

// Nearest in-scale semitone to a target interval — so "3RD"/"5TH" (and the
// ARP/WALK cycles built on them) are always consonant in the CURRENT scale:
// major 3rd in MAJ, minor 3rd in MIN, the P4 blues gives you instead of a
// 3rd, etc. Chromatic (scale 0) passes the target through exactly.
static int8_t ch2NearestScaleSemi(int8_t target) {
  if (seq.scale == 0) return target;
  const uint8_t* sc = SCALES[seq.scale];
  uint8_t L = SCALE_LENS[seq.scale];
  int8_t best = 0; int bestD = 127;
  for (uint8_t k = 0; k < L; k++) {
    int d = abs((int)sc[k] - (int)target);
    if (d < bestD) { bestD = d; best = (int8_t)sc[k]; }
  }
  return best;
}

// Semitone offset for a KBRD key (0..CH2_KB_KEYS-1). Same relative-interval
// idea as PITCH mode (ROOT/3RD/5TH/OCT via ch2NearestScaleSemi so they stay
// consonant in the current scale), rounded out with 2ND/4TH/7TH/-OCT. This
// replaces the old "8 chromatic scale degrees" mapping — a KBRD key is now
// a CHORD TONE, so a recorded step follows key/scale changes instead of
// locking to a fixed degree.
static int16_t ch2KbIntervalSemis(uint8_t idx) {
  switch (idx) {
    default:
    case 0: return 0;                        // ROOT
    case 1: return ch2NearestScaleSemi(4);    // 3RD
    case 2: return ch2NearestScaleSemi(7);    // 5TH
    case 3: return 12;                        // OCT
    case 4: return ch2NearestScaleSemi(2);    // 2ND
    case 5: return ch2NearestScaleSemi(5);    // 4TH
    case 6: return ch2NearestScaleSemi(11);   // 7TH
    case 7: return -12;                       // -OCT
  }
}

// Semitone offset for the current PITCH mode. Advances ARP/WALK state per
// TRIGGER (not per step) and wraps — the agreed behavior: no bar reset.
// Core-0 only (called from advanceStep's trigger path), so the state
// needs no cross-core care.
static int16_t ch2IntervalSemis() {
  int8_t third = ch2NearestScaleSemi(4);
  int8_t fifth = ch2NearestScaleSemi(7);
  // NOTE: modes 0-4 are now the FOLLOW family, handled in triggerCh2Pulse
  // before this is ever called — so cases 0-3 below are dead. Kept only
  // because the ARP cycles (5/6) reuse `third`/`fifth` directly. This
  // function now effectively only serves modes 5-7.
  switch (ch2PitchMode) {
    default:
    case 0: return 0;              // (dead) was ROOT
    case 1: return third;          // (dead) was 3RD
    case 2: return fifth;          // (dead) was 5TH
    case 3: return 12;             // (dead) was OCT
    case 5: {                      // ARP+ : R,3,5,O ascending cycle
      const int8_t cyc[4] = {0, third, fifth, 12};
      int16_t s = cyc[ch2ArpPos & 3];
      ch2ArpPos = (ch2ArpPos + 1) & 3;
      return s;
    }
    case 6: {                      // AR+- : R,3,5,O,5,3 up-down bounce
      const int8_t cyc[6] = {0, third, fifth, 12, fifth, third};
      int16_t s = cyc[ch2ArpPos % 6];
      ch2ArpPos = (ch2ArpPos + 1) % 6;
      return s;
    }
    case 7: {                      // WALK: drunken scale-degree walk
      uint8_t L = SCALE_LENS[seq.scale];
      int8_t maxDeg = (int8_t)(L * 2 - 1);     // two octaves of range
      int8_t r = (int8_t)(random(3)) - 1;      // -1 / 0 / +1
      ch2WalkDeg = constrain((int8_t)(ch2WalkDeg + r), (int8_t)0, maxDeg);
      return (int16_t)SCALES[seq.scale][ch2WalkDeg % L] + 12 * (ch2WalkDeg / L);
    }
  }
}

// Map a KBRD key (interval index 0..CH2_KB_KEYS-1, see ch2KbIntervalSemis)
// to an absolute note index in ch1's current key/scale — so the keyboard
// is always in tune and follows key AND CHORD (root/3rd/5th/etc) changes,
// not just scale changes.
// Deliberately does NOT apply HARM here (that was the first version's bug —
// this function is only reached by the RECORDED-note and live-NOTE-press
// paths, not FOLLOW or ARP/WALK, so an offset applied here was inaudible on
// any pattern using those). HARM itself is gone now, replaced by EVOLVE.
// EVOLVE's octave overlay is applied once in triggerCh2Pulse instead,
// AFTER this function returns and after the FOLLOW/ARP paths compute their
// own pitch — same single-call-site principle HARM used, but unlike HARM
// (and unlike EVOLVE v1's shadow-mutation, which only ever touched the
// recorded-note path) it now genuinely applies to every pitch path
// equally, since it's an offset added at the end rather than something
// baked into stored per-step data.
static uint8_t ch2KeyDegreeToNote(uint8_t deg) {
  int base = (int)seq.key + (int)seq.octave * 12 + (int)ch2Octave * 12;
  int note = base + (int)ch2KbIntervalSemis(deg);
  return (uint8_t)constrain(note, 0, 59);
}

// Full reset for DRIFT (channel 2) — same "hold pads 1+2 for ~1s" gesture
// as drums (bmDoReset) and the acid sequencer (inline FACTORY RESET
// below), but scoped to ch2 only: channel 1's pattern, drums, and the
// shared clock are all untouched — DRIFT just goes back to a blank
// pattern at its default sound/pitch/fx, same as re-entering fresh.
void ch2DoReset() {
  for (uint8_t i = 0; i < NUM_STEPS; i++) { ch2Steps[i] = false; ch2StepNote[i] = CH2_STEP_NONE; }
  ch2Octave      = -1;
  ch2DetuneCents = 18;
  ch2Sound       = 0;
  ch2Amount      = 128;
  ch2PitchMode   = 0;
  ch2DecaySel    = 4;
  ch2EchoSel     = 0;
  ch2VerbSel     = 0;
  ch2AutoRec = 0; ch2RecActive = 0;
  for (uint8_t L=0;L<3;L++) for (uint8_t i=0;i<NUM_STEPS;i++) ch2AutoVal[L][i] = 0;
  ch2ApplyEcho();               // republish so a live echo tail doesn't linger stale
  funcMode   = false;
  ch2FuncSel = 255;
  ch2EvolSel = 0; ch2EvoResetCadence();     // a reset stops EVOLVE and returns it to root
  ch2XposeSemis = 0; ch2XposeKey = -1;      // and drops any live transpose
  ui.dirty = true; ui.fullDirty = true; ui.infoDirty = true;
}

// Poll-based live gestures for the ch2 top row. This reads the PHYSICAL pad
// state rather than hooking press/release because the top row dispatches
// unevenly — pads 3-6 apply on PRESS while 1/2/7/8 defer to RELEASE (they
// double as the PLAY/FUNC chord pads). A poll sees all eight identically and
// can measure hold duration, which neither dispatch path can. Called once per
// updateControl() pass, after the pad scan.
void ch2PollLiveGestures() {
  // Keep the flash animation moving. Pure elapsed-time state — nothing here
  // blocks — but SOMETHING has to keep asking for a repaint while a pulse is
  // mid-flight, or the tile would just sit at whichever colour happened to
  // be on screen when the last unrelated redraw fired. (HARM used to have a
  // second, longer commit-flash pair here too — EVOLVE is a plain tap-select
  // page like ECHO/VERB, nothing to commit, so just EUC's remains.)
  if (ch2EucFlashTile >= 0) {
    if (millis() - ch2EucFlashMs >= CH2_EUC_FLASH_MS) ch2EucFlashTile = -1;
    ui.valDirty = true;
  }

  // Any exit from the ch2 FUNC pages drops the transpose — otherwise a
  // held key at the moment you leave would leave DRIFT stuck off-key.
  if (!ch2EditMode || !funcMode) {
    if (ch2XposeSemis || ch2XposeKey >= 0) {
      gCh2NotePressMs = 0;   // same reason as the release branch below
      ui.dirty = true; ui.valDirty = true;
    }
    ch2XposeSemis = 0; ch2XposeKey = -1; ch2GestPad = -1; ch2GestArmed = false;
    return;
  }

  int8_t held = -1;
  for (uint8_t i = 0; i < CH2_KB_KEYS; i++) if (pState[i]) { held = (int8_t)i; break; }

  if (held < 0) {                       // nothing held — release the gesture
    // EUC tap = LOAD. Deferred to release for the same reason HARM's old
    // manual-select used to be: the top row dispatches unevenly (pads 3-6
    // on press, 1/2/7/8 on release), so only release can tell a genuine
    // tap from the start of a hold. !ch2GestArmed means the hold-to-save
    // branch below did NOT fire for this press, so it's a genuine tap.
    // (EVOLVE, unlike HARM, doesn't need a release-deferred branch here at
    // all — it's a plain tap-select page like ECHO/VERB, dispatched
    // through the ordinary ch2FuncApply path, no hold gesture of its own.)
    if (ch2FuncSel == 5 && ch2GestPad >= 0 && !ch2GestArmed) {
      ch2LoadEucSlot((uint8_t)ch2GestPad);
      ui.dirty = true; ui.valDirty = true; ui.barDirty = true;
    }
    if (ch2XposeSemis || ch2XposeKey >= 0) {
      // A transpose hold just ended. Expire the press-flash as well, or the
      // value strip keeps showing the key you were HOLDING for whatever is
      // left of its 1s window instead of reverting to the step's recorded
      // note. gCh2NotePressMs was stamped back on the PRESS, so a hold of
      // ~180ms-1s got released with the flash still live — which is exactly
      // why this looked intermittent: hold past 1s and the flash had already
      // expired on its own, so those releases reverted correctly. Only
      // cleared when a transpose actually engaged; a quick record-tap never
      // reaches the 180ms threshold and keeps its full "just picked" flash.
      gCh2NotePressMs = 0;
      ui.dirty = true; ui.valDirty = true;
    }
    ch2XposeSemis = 0; ch2XposeKey = -1; ch2GestPad = -1; ch2GestArmed = false;
    return;
  }

  uint32_t now = millis();
  if (held != ch2GestPad) {             // new pad — restart the hold timer
    ch2GestPad = held; ch2GestSince = now; ch2GestArmed = false;
    ch2XposeSemis = 0; ch2XposeKey = -1;
    // NOTE's press-flash (formerly slot 2) lived here — gone with the page.
    // PITCH (now slot 2) is a plain tap-select page, no gesture polling needed.
  }
  // PAD_PLAY_A/PAD_PLAY_B (pads 1+2) double as tiles S1/S2 on this same
  // gesture tracker. A two-finger PLAY/STOP release lands asynchronously —
  // the first pad up drops out of `held`, and the loop above picks up the
  // still-down second pad as if it were a brand-new press, arming a tap
  // that fires the instant it releases too. pChord[] stays true for
  // whichever PLAY pad hasn't released yet for exactly this window, so use
  // it to latch suppression instead: once any part of a PLAY chord touches
  // this press, ch2GestArmed stays true for the rest of it, blocking the
  // tap-load/select AND the hold-save below for PATT specifically — the
  // only page below still driven by this poll's tap/hold logic. (EVOLVE,
  // unlike HARM before it, doesn't need this: it dispatches through the
  // plain ch2FuncApply tap path, same as ECHO/VERB, so pads 1/2 landing
  // async here doesn't affect it at all.) A genuine solo tap or hold on
  // S1/S2 (PAD_PLAY_B never joins) never sees pChord[held] true, so it's
  // unaffected.
  if (held >= 0 && pChord[held]) ch2GestArmed = true;
  uint32_t heldMs = now - ch2GestSince;

  // EUC page: long-hold a tile to SAVE whatever pattern is currently
  // painted into ch2Steps as that slot's custom pattern (overwriting
  // whatever was there before — blank or a previous save). ch2GestArmed
  // suppresses the tap-load handled in the release branch above, so a
  // hold never also fires a load on release.
  if (ch2FuncSel == 5 && !ch2GestArmed && heldMs >= CH2_EUC_SAVE_HOLD_MS) {
    ch2GestArmed = true;
    uint16_t bits = 0;
    for (uint8_t i = 0; i < NUM_STEPS; i++) if (ch2Steps[i]) bits |= (uint16_t)1 << i;
    ch2EucCustom[held]  = bits;
    ch2EucSavedMask    |= (uint8_t)(1 << held);
    ch2EucSel           = held;
    ch2EucFlashTile     = (int8_t)held;
    ch2EucFlashMs       = now;
    saveCh2EucSlots();
    ui.dirty = true; ui.valDirty = true; ui.barDirty = true;
  }
}

// Play DRIFT immediately at a NOTE key (live audition) and, if the
// sequencer is running, RECORD it onto the step NEAREST the playhead —
// play a few notes while it loops and the pattern remembers them. Also
// flashes the pressed key in the value strip for ~1s (gCh2NotePressMs)
// so you can SEE what you just picked, independent of playhead tracking.
// (gCh2NotePressMs / gCh2NotePressIdx are declared up with the ch2 live
// gesture state, since ch2PollLiveGestures() also writes them.)
// Nearest-step quantize: a press near the END of the current step's
// window (>60% elapsed) almost always MEANT the step about to start —
// human reaction time to what you hear lands late, not early. Pushing
// it forward in that window makes "hit exactly when you want" far more
// forgiving without adding any perceptible lag when you nail it. Shared
// by NOTE recording and REC automation recording.
uint8_t ch2QuantizedTargetStep() {
  uint8_t target = seq.cur;
  if (seq.interval > 0) {
    uint32_t elapsed = micros() - seq.lastUs;
    if (elapsed * 10 > (uint32_t)seq.interval * 6) target = (seq.cur + 1) % seq.len;
  }
  return target;
}

void ch2NotePlay(uint8_t idx) {
  ch2KbNoteOverride = (int16_t)ch2KeyDegreeToNote(idx);  // triggerCh2Pulse reads this once
  triggerCh2Pulse();
  ch2KbNoteOverride = -1;
  gCh2NotePressMs  = millis();
  gCh2NotePressIdx = idx;
  ui.dirty = true; ui.valDirty = true;   // redraw the value strip NOW — this is
                                          // the actual fix: without valDirty the
                                          // press-flash below never got drawn
  if (seqIsRunningForDisplay()) {
    uint8_t target = ch2QuantizedTargetStep();
    // Clobber policy: overwriting the step you're AIMING at (seq.cur) is
    // intended overdub — the last note you played on it wins. But the
    // forward quantize (target = cur+1) exists only to catch a late reaction
    // onto an UPCOMING step; it must not silently overwrite a note already
    // recorded there. If it bumped us forward onto an occupied step, fall
    // back to seq.cur rather than steal a note we weren't aiming at.
    if (target != seq.cur && ch2StepNote[target] != CH2_STEP_NONE)
      target = seq.cur;
    ch2Steps[target]    = true;
    ch2StepNote[target] = (int8_t)idx;
    if (ch2EvolSel) ch2EvoResetCadence();   // recording snaps EVOLVE back to root —
                                             // a stale extra-step index could point
                                             // at a step whose content just changed
    ui.dirty = true; ui.editDirty = true; ui.editStep = target;
  }
}

void triggerCh2Pulse() {
  int16_t idx2;
  if (ch2KbNoteOverride >= 0) {
    // Live NOTE press — exact note, bypasses PITCH mode entirely.
    idx2 = ch2KbNoteOverride;
  } else if (ch2EvoNoteAt(seq.cur) != CH2_STEP_NONE) {
    // This step has a RECORDED note, OR is EVOLVE's forced extra step for
    // the current excursion (see ch2EvoNoteAt) — either way it overrides
    // PITCH mode. No shadow copy in v2: ch2EvoNoteAt reads ch2StepNote
    // directly and only substitutes ch2EvoExtraNote for a step that's
    // genuinely off in the real pattern, so a recorded note is always
    // exactly what you recorded.
    idx2 = (int16_t)ch2KeyDegreeToNote((uint8_t)ch2EvoNoteAt(seq.cur));
  } else if (ch2PitchMode <= 4) {
    // FOLLOW family (modes 0-4): mirror the note ch1 last actually PLAYED
    // (captured in triggerNote(), so it tracks ratchets and transposed
    // pitches), then add a harmony interval. Holding through ch1's silent
    // steps sustains the audible harmony instead of jumping to a stored-
    // but-silent pitch. Falls back to the current step's stored note only
    // before the first ch1 note of the session.
    //   0 F+8 = +octave   1 F-8 = -octave   2 F+5 = fifth
    //   3 F+3 = third      4 FOLW = plain mirror
    int16_t src = (gLastCh1Note >= 0)
                ? gLastCh1Note
                : (int16_t)constrain((int)scaleNote(seq.cur) + seq.trans, 0, 59);
    int16_t off;
    switch (ch2PitchMode) {
      case 0:  off =  12; break;                       // F+8 octave up
      case 1:  off = -12; break;                       // F-8 octave down
      case 2:  off = ch2NearestScaleSemi(7); break;    // F+5 fifth (scale-aware)
      case 3:  off = ch2NearestScaleSemi(4); break;    // F+3 third (scale-aware)
      default: off =   0; break;                       // FOLW plain
    }
    idx2 = src + off + (int16_t)ch2Octave * 12;
  } else {
    // Modes 5-7: key-relative arpeggio / walk (not follow-based).
    idx2 = (int16_t)seq.key + (int16_t)seq.octave * 12 + (int16_t)ch2Octave * 12
         + ch2IntervalSemis();
  }
  // EVOLVE's octave overlay applies here, universally, same as HARM's
  // offset once did — unlike v1's shadow-mutation (which only ever
  // touched the recorded-note path above), ch2EvoOctOffset is a plain
  // semitone add at the end, so it reaches FOLLOW-family and ARP/WALK too,
  // not just recorded notes. Live transpose from a held KBRD key.
  // Deliberately NOT applied to the ch2KbNoteOverride branch: that IS the
  // note you're physically pressing, so shifting it too would double-count
  // the same gesture and the audition wouldn't match the key you hit.
  // Every other path — recorded step note, FOLLOW, arp/walk — is the
  // running loop, and that's what should move.
  if (ch2KbNoteOverride < 0) idx2 += ch2XposeSemis + (int16_t)ch2EvoOctOffset;
  idx2 = constrain(idx2, (int16_t)0, (int16_t)59);
  uint16_t f2 = noteFreq[idx2] / 2;

  // NOTE: DTSQR used to bake its cents-detune into f2 here (one mistuned
  // oscillator). It is now a true TWO-oscillator engine like UNISN —
  // osc 1 at pitch, osc 2 derived live via ch2DetuneRatioQ16 — so the
  // trigger-side offset is gone. Both dual-voice engines (DTSQR, UNISN)
  // leave f2/ch2PhaseInc at true pitch; anything else would put osc 1
  // sharp of ch1 and osc 2 sharp of THAT.

  ch2PhaseInc = (uint32_t)(((uint64_t)f2 << 32) / AUDIO_RATE);
  ch2Phase    = 0;       // restart phase on trigger for a clean attack transient

  // UNISN's second voice, computed as phase2 = ch2Phase * ratio each sample
  // (see bmFillDrumBuffer) rather than a separate accumulator — exact and
  // one less piece of cross-core state to keep in sync.
  // Detune is LIVE on the RES pot again, but ONLY while DTSQR/UNISN is the
  // selected engine (see the pot read). For the other six engines RES
  // drives AMOUNT instead and ch2DetuneCents just holds its last value —
  // harmless, since only these two engines read the ratio.
  ch2DetuneRatioQ16 = (uint32_t)(65536 + ((uint32_t)65536 * (uint32_t)ch2DetuneCents) / 1731);

  // CLICK's noise-burst attack — length scales with ch2Amount so the DCY
  // pot controls "how much click" live. Zero for every other engine, so
  // this is a no-op there.
  ch2ClickCount = (ch2Sound == 4) ? (uint8_t)(1 + ((uint32_t)ch2Amount * CH2_CLICK_LEN_MAX) / 255) : 0;
  ch2NoiseIdx   = (uint8_t)(millis() & 0x3F);  // cheap re-seed so repeated hits don't sound identical

  // SND2 engines' own state — reset unconditionally regardless of which
  // engine is selected, same as ch2ClickCount effectively is above; cheap,
  // and harmless for the 7 engines that don't read them.
  ch2SubDivFlip = 0;                  // SUBDIV starts each note on the low half
  ch2CrushCtr   = 0;                  // CRUSH recomputes immediately on a fresh trigger
  // ARPG: step starts at 2, NOT 0. The counter also starts at 0, so the very
  // first sample of a note takes the "advance" branch — from step 0 that
  // landed on step 1, i.e. every note began on the FIFTH instead of the root.
  // Starting at 2 means the first advance wraps to 0 and the note opens on
  // the root, which is what an arpeggio is supposed to do.
  ch2ArpgPhase  = 0; ch2ArpgStep = 2; ch2ArpgCtr = 0;
  ch2GongIdx    = 0xFFFF;             // GONG's independent index decay starts at full
  ch2VowP1      = 0; ch2VowP2 = 0;    // VOWEL formants restart with the note
  ch2GatePhase  = 0;                  // GATE opens each note un-chopped
  // NOIZ: re-seed per hit so repeated notes aren't bit-identical, but never
  // to 0 — an xorshift register seeded 0 is a fixed point and outputs
  // silence forever. OR in a constant so the millis() term can't zero it.
  ch2NoiseLfsr  = (uint32_t)millis() | 0x9E3779B9UL;
  ch2NoiseLp    = 0;                  // start the colour filter from silence
  ch2NoisePhase = 0;                  // NOIZ clock starts fresh with the note

  ch2ApplyEcho();    // retune echo to the current tempo — see ch2ApplyEcho()

  ch2Env  = 0xFFFF;  // full level on trigger; decays each sample in bmFillDrumBuffer()
  // Decay time now comes from the CH2 FUNC DCY page (8 fixed presets,
  // 25ms..2s — reaching click and drone extremes the old 80-500ms pot
  // sweep never could). BEHAVIOR CHANGE, deliberate: ch2Amount (DCY pot)
  // no longer alters pulse length — it previously double-dutied as BOTH
  // decay time and per-engine character, so e.g. raising FM brightness
  // also lengthened the pulse. Amount is now pure engine character.
  // Table is precomputed (see CH2_ENVM) — no per-trigger expf().
  // Decay is now a LIVE pot (pot 3), computed in updateControl into
  // ch2EnvMLive — no longer the DCY FUNC preset table. That FUNC slot is
  // freed (next feature). Fallback to the mid table entry if the pot
  // path hasn't run yet (first trigger at boot before any control pass).
  ch2EnvM = ch2EnvMLive ? ch2EnvMLive : CH2_ENVM[4];

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

    case 2: // ALT — even-index steps first (0,2,4...) then odd-index (1,3,5...)
      { if (len <= 1) return 0;   // degenerate: one step, nothing to alternate —
                                    // old code could return index 1 here, out of
                                    // range for a length-1 pattern
        uint8_t next = seq.cur + 2;
        if (seq.cur % 2 == 0) {
          // currently on an even-index (0,2,4...) step
          if (next >= len) return 1;   // switch to odd-index steps
          return next;
        } else {
          // currently on an odd-index (1,3,5...) step
          if (next >= len) return 0;   // back to start of even-index steps
          return next;
        }
      }

    case 3: // REV — reverse through all steps
      return (seq.cur == 0) ? last : seq.cur - 1;

    case 4: // SKIP2 — every other step: 0,2,4,6...
      { uint8_t next = seq.cur + 2;
        return (next >= len) ? 0 : next;
      }

    case 5: // SKIP3 — every third step: 0,3,6,9... A single subtraction only
             // wraps correctly when len>2; for len==1 or len==2 the old
             // "next - len" version undershoots and seq.cur climbs straight
             // out of the valid 0..len-1 range (and eventually out of the
             // whole 16-element seq.steps[] array — real OOB read). True
             // modulo, computed in a wider type so cur+3 can't itself
             // overflow uint8_t before the mod, wraps correctly for any len.
      return (uint8_t)(((uint16_t)seq.cur + 3) % len);

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

  // CHANNEL 2: fires on its own step pattern, independent of whatever
  // channel 1 is doing on this same step (it always follows channel 1's
  // current step position/length/order exactly — no note of its own,
  // just rhythm — see triggerCh2Pulse()'s comment for why).
  // NOTE page: only poke the value strip while the "CLEARED" flash is
  // actually up, so it can time itself out. Repainting EVERY step (the
  // old behavior) redrew all 8 tiles several times a second — that was
  // the visible flashing. The key labels are static and don't need a
  // per-step refresh; a live key press already sets valDirty itself.
  // REC page: refresh while armed (banner tracks arm state + lit lanes) and
  // during its clear-flash. Cheap single-banner redraw, not the tile loop.
  if (ch2EditMode && funcMode && ch2FuncSel == 7 &&
      (ch2Recording || (gCh2RecClrFlashMs && (millis() - gCh2RecClrFlashMs) < 800))) {
    ui.valDirty = true;
  }
  // REC automation — replay whatever was recorded onto THIS step, before
  // the normal ch2 hit, regardless of which function page happens to be
  //
  // A play flag is REQUIRED here, not just "DRIFT exists". advanceStep()
  // ticks whenever seq.running is true, so with no gate at all DRIFT would
  // fire on any tick — drum- or acid-driven — whether or not it was ever
  // told to play. This matches how drums already work: bmTriggerStep()
  // gates internally on bmPlaying. Each engine checks its own play flag.
  // Gating the whole block (not just the trigger) also stops recorded pot
  // motion replaying and stops AUTO-WALK silently mutating the pattern
  // while you're stopped.
  //
  // DRIFT wants to fire exactly when its own play flag says go
  // (driftPlaying) — symmetric with acid's own gate (acidPlaying, used
  // further down in this function). ch2SynthMode is NOT part of this gate:
  // acid and DRIFT can play simultaneously now (see updateAudio()), so
  // ch2SynthMode means only "which one the pads/pots currently edit", not
  // "which one is audible" — requiring it here would silence DRIFT's notes
  // the moment you switched editing focus back to acid's screen, even
  // while it kept playing in the mix.
  bool ch2WantsToFire = driftPlaying;
  if (ch2WantsToFire) {
    // Restart the arp cycle from the current phase at the top of every
    // pattern cycle, so ARP+/AR+- repeat identically bar to bar instead of
    // drifting with the hit count.
    if (seq.cur == 0) ch2ArpPos = ch2ArpPhase;
    ch2AutoPlayback();                 // apply recorded pot motion for this step
    if (ch2EvoStepOn(seq.cur)) triggerCh2Pulse();

    // EVOLVE SCHEDULER — once the current segment (root or excursion) has
    // run its assigned number of bars (in real 16-step bars, same fixed
    // unit HARM's auto-walk used, independent of seq.len for the same
    // reason: a length-3 pattern laps 5.3x per 16-step bar, so counting
    // LAPS instead would make "N bars" mean something different at every
    // pattern length), flip to the other segment. ch2EvoStepsInSeg counts
    // every step UNCONDITIONALLY, but the flip itself still only ever
    // happens at seq.cur==len-1 — never mid-loop — so root/excursion
    // transitions always land on a bar boundary, same "changes land on the
    // LAST step of the pattern" timing HARM used, for the same reason: a
    // few steps late on an odd pattern length beats an audible stumble.
    if (ch2EvolSel != 0) {
      ch2EvoStepsInSeg++;
      if (seq.len && seq.cur == (uint8_t)(seq.len - 1)) {
        const Ch2EvoPreset& p = CH2_EVO_PRESET[ch2EvolSel];
        uint16_t needed = (uint16_t)(ch2EvoInExcursion ? p.excBars : p.rootBars) * 16;
        if (ch2EvoStepsInSeg >= needed) {
          ch2EvoStepsInSeg = 0;
          ch2EvoInExcursion = !ch2EvoInExcursion;
          if (ch2EvoInExcursion) ch2EvoStartExcursion(); else ch2EvoEndExcursion();
        }
      }
    }
  }


  // PATTERN CHAINING: count ticks against the *current* seq.len rather than
  // watching for seq.cur to wrap back to 0 — that wrap point isn't reliable
  // under every rrMode (RND can land on 0 at any time, PINGPONG wraps at
  // both ends), so a plain elapsed-step counter is correct regardless of
  // which play algorithm is active. Re-reads seq.len each time, so it
  // self-corrects to whichever length the newly-loaded slot uses.
  if (chainActive && chainLen > 0) {
    chainTickCount++;
    if (chainTickCount >= seq.len) {
      chainTickCount = 0;
      chainPos = (chainPos + 1 >= chainLen) ? 0 : chainPos + 1;
      gSkipMainScreenClear = true;  // already on the main screen — skip the redundant wipe
      loadPatch(chain[chainPos]);
      // Also reload DRIFT's pattern/settings for this unified slot —
      // chain[] holds indices into the same 4 unified save slots
      // loadAllFromSlot() draws from (each one holds acid + DRIFT + drums
      // together), but loadPatch() only ever touched the acid side. That
      // meant chaining built from the DRIFT screen advanced the visible
      // chain position and the acid pattern underneath just fine, but
      // DRIFT itself never budged — same sound the whole way through,
      // because nothing was ever telling it to change. loadCh2Patch()
      // already no-ops safely if this particular slot never had DRIFT
      // data saved into it (see its own ch2SlotHasData guard), so this is
      // safe to call unconditionally here regardless of which screen the
      // chain was built from. Drums deliberately NOT included — chaining
      // has never touched the drum pattern, and unlike DRIFT (which was
      // reported as silently stuck), that's not something anyone's
      // flagged as broken.
      loadCh2Patch(chain[chainPos]);
      // Force playback to the new pattern's own step 0, rather than
      // wherever seq.cur happened to be under the OLD pattern. Without
      // this, loadPatch() swaps every step's note/active/effect data but
      // leaves seq.cur untouched, so the newly-loaded pattern picks up
      // mid-way through itself instead of announcing itself from the top
      // — easy to mistake for "nothing changed", especially if that
      // carried-over position lands on a quiet or similar-sounding step.
      // Set directly to 0 (not len-1) since the note-firing code just
      // below in this same advanceStep() call reads seq.steps[seq.cur]
      // for THIS tick — we want that read to land on the new pattern's
      // first step immediately, not schedule it for next tick.
      seq.cur = 0;
    }
  }

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
  p.kwMode = kwMode;
  p.rrMode = rrMode | (acidPlaying ? PATCH_PLAYING_BIT : 0);
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
  rrMode       = constrain((uint8_t)(p.rrMode & 0x07), (uint8_t)0, (uint8_t)7);
  // Note: p.rrMode's top bit (PATCH_PLAYING_BIT) carries whether acid was
  // playing when this was saved. Deliberately NOT applied here — loadPatch()
  // is also called mid-playback by pattern chaining (loadPatch(chain[...]))
  // where forcing acidPlaying would be wrong. Only loadAllFromSlot() (the
  // actual user-facing slot-load gesture) applies it — see patchWasPlaying().
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

// Was acid playing when this slot was saved? Reads PATCH_PLAYING_BIT out of
// the saved rrMode byte. Used only by loadAllFromSlot() — see the note in
// loadPatch() on why this isn't applied there directly.
bool patchWasPlaying(uint8_t slot) {
  if (slot >= NUM_SLOTS || !slotHasData[slot]) return false;
  Patch p;
  EEPROM.get(SLOT_ADDR(slot), p);
  if (p.valid != PATCH_VALID) return false;
  return (p.rrMode & PATCH_PLAYING_BIT) != 0;
}

void checkSlots() {
  for (uint8_t s = 0; s < NUM_SLOTS; s++) {
    uint8_t v;
    EEPROM.get(SLOT_ADDR(s), v);
    slotHasData[s] = (v == PATCH_VALID);
    uint8_t cv;
    EEPROM.get(CH2_SLOT_ADDR(s), cv);
    ch2SlotHasData[s] = (cv == CH2_SLOT_VALID);
  }
  bmCheckSlots();
}

// =====================================================================
// DRIFT (CHANNEL 2) PER-SLOT SAVE / LOAD
// Mirrors saveCh2Settings()/loadCh2Settings() below, but reads/writes the
// per-slot region above instead of the single always-on CH2X blob. Kept
// separate from that blob on purpose: the always-on blob is DRIFT's
// "last edited" state, restored every power-on regardless of slots;
// these are DRIFT's saved state per numbered slot, only touched by the
// save/load gesture.
// =====================================================================
void saveCh2Patch(uint8_t slot) {
  if (slot >= NUM_SLOTS) return;
  Ch2SettingsX c;
  c.valid       = CH2_SLOT_VALID;
  c.octave      = ch2Octave;
  c.detuneCents = ch2DetuneCents;
  c.sound       = ch2Sound;
  c.amount      = ch2Amount;
  c.pitchMode   = ch2PitchMode;
  c.decaySel    = ch2DecaySel;
  c.steps       = 0;
  for (uint8_t i = 0; i < NUM_STEPS; i++) if (ch2Steps[i]) c.steps |= (uint16_t)1 << i;
  c.echoSel = ch2EchoSel;
  c.verbSel = ch2VerbSel;
  c.autoRec = ch2AutoRec;
  for (uint8_t L=0;L<3;L++) for (uint8_t i=0;i<NUM_STEPS;i++) c.autoVal[L][i] = ch2AutoVal[L][i];
  for (uint8_t i = 0; i < NUM_STEPS; i++) c.stepNote[i] = ch2StepNote[i];
  c.playing = driftPlaying ? 1 : 0;
  EEPROM.put(CH2_SLOT_ADDR(slot), c);
  ch2SlotHasData[slot] = true;
  saveCommit = true;
}

// Was DRIFT playing when this slot was saved? Used only by loadAllFromSlot()
// to force play state — deliberately NOT applied inside loadCh2Patch()
// itself, which (unlike loadPatch()) currently has no other call sites, but
// keeping the same separation-of-concerns as the acid/drum equivalents so
// a future caller doesn't inherit a surprise side effect.
bool ch2PatchWasPlaying(uint8_t slot) {
  if (slot >= NUM_SLOTS || !ch2SlotHasData[slot]) return false;
  Ch2SettingsX c;
  EEPROM.get(CH2_SLOT_ADDR(slot), c);
  if (c.valid != CH2_SLOT_VALID) return false;
  return c.playing != 0;
}

void loadCh2Patch(uint8_t slot) {
  if (slot >= NUM_SLOTS || !ch2SlotHasData[slot]) return;
  Ch2SettingsX c;
  EEPROM.get(CH2_SLOT_ADDR(slot), c);
  if (c.valid != CH2_SLOT_VALID) return;
  ch2Octave      = constrain(c.octave,      (int8_t)-2,  (int8_t)2);
  ch2DetuneCents = constrain(c.detuneCents, (int16_t)0,  (int16_t)30);
  ch2Sound       = (c.sound >= CH2_NUM_SOUNDS) ? 0 : c.sound;
  ch2Amount      = c.amount;
  ch2PitchMode   = c.pitchMode & 7;
  ch2DecaySel    = c.decaySel  & 7;
  ch2EchoSel     = c.echoSel   & 7;
  ch2VerbSel     = c.verbSel   & 7;
  ch2ApplyVerb();
  ch2ApplyEcho();
  ch2AutoRec = c.autoRec;
  for (uint8_t L=0;L<3;L++) for (uint8_t i=0;i<NUM_STEPS;i++) ch2AutoVal[L][i] = c.autoVal[L][i];
  for (uint8_t i = 0; i < NUM_STEPS; i++) {
    ch2Steps[i]    = (c.steps >> i) & 1;
    ch2StepNote[i] = c.stepNote[i];
  }
  ch2EvoResetCadence();   // loading a patch replaces the pattern out from
                           // under EVOLVE — always reset, not just when
                           // it's on, so a stale extra-step from the
                           // PREVIOUS patch can't leak in if EVOLVE gets
                           // turned on later this session.
}

// =====================================================================
// COMBINED SAVE / LOAD — one gesture, either screen, all three engines.
// Previously save/load only touched whichever engine's screen you were
// on (acid screen -> acid pattern only, drum screen -> drum beat only),
// and DRIFT wasn't part of the slot system at all. Now a single slot
// save captures acid + drums + DRIFT together, whatever each currently
// has dialled in, regardless of which screen triggered it — and load
// brings back whichever of the three actually have data in that slot.
// Called from both the acid-mode gesture handler below and the
// drum-mode one in BeatMachine2.ino.
// =====================================================================
void saveAllToSlot(uint8_t slot) {
  savePatch(slot);      // acid pattern + sound
  saveCh2Patch(slot);   // DRIFT pattern + settings
  bmSavePatch(slot);    // drum beat + kit (BeatMachine2.ino)
  saveCommit = true;    // actual flash write deferred to core1 (loop1)
}

void loadAllFromSlot(uint8_t slot) {
  if (slotHasData[slot])    loadPatch(slot);
  if (ch2SlotHasData[slot]) loadCh2Patch(slot);
  if (bmSlotHasData[slot])  bmLoadPatch(slot);

  // Force each engine's play/stop state to exactly what it was when this
  // slot was saved — a saved-stopped engine goes silent even if it's
  // currently playing, and vice versa (confirmed this is what's wanted,
  // not a softer "only turn things on" version). An engine with no data
  // in this slot is left exactly as it currently is, same as the pattern
  // load above only touching engines that actually have data here.
  bool wantAcid  = slotHasData[slot]    ? patchWasPlaying(slot)    : acidPlaying;
  bool wantDrift = ch2SlotHasData[slot] ? ch2PatchWasPlaying(slot) : driftPlaying;
  bool wantDrums = bmSlotHasData[slot]  ? bmPatchWasPlaying(slot)  : bmPlaying;

  // Shared clock: wanted if ANY of the three ends up playing. Only reset
  // phase/position when actually starting it from fully stopped — mirrors
  // the exact reset done by the acid/DRIFT/drum START handlers elsewhere.
  bool clockWanted = wantAcid || wantDrift || wantDrums;
  if (clockWanted && !seq.running) {
    seq.running = true;
    seq.cur = seq.len - 1;
    seq.lastUs = micros();
    syncPulse = 0;
    if (chainActive) chainTickCount = 0;
  } else if (!clockWanted) {
    seq.running = false;
  }

  acidPlaying  = wantAcid;
  driftPlaying = wantDrift;

  // Drums: only call the start/stop helpers on an actual transition, so an
  // engine that's ALREADY in the right state doesn't get its position
  // reset for no reason (bmStartDrums() zeroes bmDrumPos etc.).
  if (wantDrums && !bmPlaying)      bmStartDrums();
  else if (!wantDrums && bmPlaying) bmStopDrums();

  ui.dirty = true; ui.fullDirty = true; ui.infoDirty = true; ui.barDirty = true;
}

// True if slot has ANY saved data — acid, DRIFT, or drums — used by both
// gesture handlers to decide "load" vs "empty slot" on a short tap.
bool comboSlotHasData(uint8_t slot) {
  if (slot >= NUM_SLOTS) return false;
  return slotHasData[slot] || ch2SlotHasData[slot] || bmSlotHasData[slot];
}

// =====================================================================
// MIX SETTINGS SAVE / LOAD — separate EEPROM slot, persists across
// pattern saves/loads and power cycles. Replaces the old accent-settings
// persistence (accent is now fixed, see ACCENT_*_FIXED above).
// =====================================================================
void saveMixSettings() {
  MixSettings m;
  m.valid      = MIX_VALID;
  m.acidLevel  = mixAcidLevel;
  m.driftLevel = mixDriftLevel;
  m.drumLevel  = mixDrumLevel;
  EEPROM.put(MIX_ADDR, m);
  saveCommit = true;
}

void loadMixSettings() {
  MixSettings m;
  EEPROM.get(MIX_ADDR, m);
  if (m.valid != MIX_VALID) return;  // no saved settings — keep defaults (unity)
  mixAcidLevel  = m.acidLevel;
  mixDriftLevel = m.driftLevel;
  mixDrumLevel  = m.drumLevel;
  recomputeMixGains();
}

// =====================================================================
// CHANNEL 2 SETTINGS SAVE / LOAD — separate EEPROM slot, same pattern as
// accent settings above
// =====================================================================
void saveCh2Settings() {
  // v2 blob at the EEPROM tail — see Ch2SettingsX for why it moved.
  // Now also persists the ch2 STEP PATTERN, so rings/pattern survive a
  // power cycle. Saved on ch2 edit-mode exit, same trigger as before.
  Ch2SettingsX c;
  c.valid       = CH2X_VALID;
  c.octave      = ch2Octave;
  c.detuneCents = ch2DetuneCents;
  c.sound       = ch2Sound;
  c.amount      = ch2Amount;
  c.pitchMode   = ch2PitchMode;
  c.decaySel    = ch2DecaySel;
  c.steps       = 0;
  for (uint8_t i = 0; i < NUM_STEPS; i++) if (ch2Steps[i]) c.steps |= (uint16_t)1 << i;
  c.echoSel = ch2EchoSel;
  c.verbSel = ch2VerbSel;
  c.autoRec = ch2AutoRec;
  for (uint8_t L=0;L<3;L++) for (uint8_t i=0;i<NUM_STEPS;i++) c.autoVal[L][i] = ch2AutoVal[L][i];
  for (uint8_t i = 0; i < NUM_STEPS; i++) c.stepNote[i] = ch2StepNote[i];
  c.playing = 0;   // unused by this always-on blob (that's saveCh2Patch's job) —
                     // zeroed explicitly rather than left as uninitialized stack data
  EEPROM.put(CH2X_ADDR, c);
  saveCommit = true;
}

void loadCh2Settings() {
  Ch2SettingsX c;
  EEPROM.get(CH2X_ADDR, c);
  if (c.valid == CH2X_VALID) {
    ch2Octave      = constrain(c.octave,      (int8_t)-2,  (int8_t)2);
    ch2DetuneCents = constrain(c.detuneCents, (int16_t)0,  (int16_t)30);
    ch2Sound       = (c.sound >= CH2_NUM_SOUNDS) ? 0 : c.sound;
    ch2Amount      = c.amount;
    ch2PitchMode   = c.pitchMode & 7;
    ch2DecaySel    = c.decaySel  & 7;
    ch2EchoSel     = c.echoSel   & 7;
    ch2VerbSel     = c.verbSel   & 7;
    ch2ApplyVerb();   // no tempo dependency — safe to publish at load
    // Publish now so a pattern that starts playing before any FUNC touch
    // already has its echo. seq.tempo may still be the boot default here
    // (patch not loaded yet) — the per-trigger ch2ApplyEcho() self-corrects
    // on the first pulse, so a momentarily mis-timed tail is the worst case.
    ch2ApplyEcho();
    ch2AutoRec = c.autoRec;
    for (uint8_t L=0;L<3;L++) for (uint8_t i=0;i<NUM_STEPS;i++) ch2AutoVal[L][i] = c.autoVal[L][i];
    for (uint8_t i = 0; i < NUM_STEPS; i++) {
      ch2Steps[i]       = (c.steps >> i) & 1;
      ch2StepNote[i]    = c.stepNote[i];      // per-step NOTE pitch (127 = none)
    }
    ch2EvoResetCadence();   // same reasoning as the other patch-load site — always,
                             // not just when EVOLVE is on (see that comment)
    return;
  }
  // MIGRATION — one-shot: no v2 blob yet, fall back to the legacy 0xC3
  // blob at 248 for the fields it had; new fields keep their defaults.
  // First ch2 edit-mode exit writes a v2 blob and this path never runs
  // again. The legacy region is then dead space — do not reuse.
  Ch2Settings o;
  EEPROM.get(CH2_ADDR, o);
  if (o.valid != CH2_VALID) return;  // nothing saved anywhere — defaults
  ch2Octave      = constrain(o.octave,      (int8_t)-2,  (int8_t)2);
  ch2DetuneCents = constrain(o.detuneCents, (int16_t)0,  (int16_t)30);
  ch2Sound       = (o.sound >= CH2_NUM_SOUNDS) ? 0 : o.sound;
  ch2Amount      = o.amount;
}

// EUC PATTERN SLOTS SAVE / LOAD — see Ch2EucSlots. save fires immediately
// from the hold gesture (ch2PollLiveGestures); load fires once at boot.
void saveCh2EucSlots() {
  Ch2EucSlots e;
  e.valid     = CH2_EUC_VALID;
  e.savedMask = ch2EucSavedMask;
  for (uint8_t s = 0; s < 8; s++) e.steps[s] = ch2EucCustom[s];
  EEPROM.put(CH2_EUC_ADDR, e);
  saveCommit = true;   // actual flash write deferred to core1 (loop1)
}

void loadCh2EucSlots() {
  Ch2EucSlots e;
  EEPROM.get(CH2_EUC_ADDR, e);
  if (e.valid != CH2_EUC_VALID) return;  // nothing saved yet — ch2EucSavedMask
                                          // stays 0, so every slot loads blank
  ch2EucSavedMask = e.savedMask;
  for (uint8_t s = 0; s < 8; s++) ch2EucCustom[s] = e.steps[s];
}

// =====================================================================
// FUNC MODE — bottom row selects function
// =====================================================================
void doFuncSelect(uint8_t padIdx) {
  if (padIdx < 8 || padIdx > 15) return;

  // CH2 EDIT owns the FUNC row: select from CH2FUNCNAMES via ch2FuncSel.
  // funcSel stays FUNC_NONE the whole time (see CH2 FUNC PAGE comment at
  // the declarations) so no ch1 special case can misfire — including the
  // tap-tempo repeat check just below, which must not see ch2 presses.
  if (ch2EditMode) {
    uint8_t newFunc = padIdx - 8;
    // No "re-tap NOTE = clear the melody" secondary action any more. It was
    // hidden, destructive and easy to fire by accident — you go to press
    // NOTE, you're already on NOTE, and the melody is gone with no warning.
    // Nothing is lost by dropping it: exiting FUNC and tapping a pad off
    // does exactly the same thing per step (the ch2 pad toggle clears
    // ch2StepNote on OFF), and that route is the obvious one to try rather
    // than a gesture you have to be told about.
    ch2FuncSel = newFunc;
    // barDirty too: the x=224 column carries PATT's gesture legend, which
    // must appear on entering that page and be wiped on leaving it.
    ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true; ui.barDirty=true;
    return;
  }

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

// EUC — load slot's pattern into ch2Steps (destructive, "groove preset
// loader"). Loads the saved custom pattern if this slot has one; otherwise
// the slot is BLANK (all steps off) — there's no more built-in euclidean
// fill to fall back to, so an unsaved slot just clears the grid.
// Skips the grid redraw entirely when the pattern doesn't actually change
// (e.g. tapping across several still-unsaved/blank slots in a row, or
// re-tapping the slot that's already loaded) — cellsDirty repaints all 16
// step cells in sequence, which reads as the grid "flashing" if it fires
// on every tap regardless of whether anything's different. ECHO/VERB/etc
// never touch cellsDirty at all, which is why only PATT had this problem.
void ch2LoadEucSlot(uint8_t slot) {
  uint16_t bits = (ch2EucSavedMask & (uint8_t)(1 << slot)) ? ch2EucCustom[slot] : 0;
  uint16_t cur  = 0;
  for (uint8_t i = 0; i < NUM_STEPS; i++) if (ch2Steps[i]) cur |= (uint16_t)1 << i;
  if (bits != cur) {
    for (uint8_t i = 0; i < NUM_STEPS; i++) ch2Steps[i] = (bits >> i) & 1;
    ui.cellsDirty = true; ui.cellIdx = 0;  // whole grid changed — rings everywhere
    ch2EvoResetCadence();   // rhythm changed under EVOLVE — always reset, same
                             // "don't let stale state leak into a later ON"
                             // reasoning as the patch-load sites
  }
  ch2EucSel = slot;
}

// CH2 FUNC — top row applies value for the selected ch2 function.
// All 8 slots live: SND1 / SND2 / FOLW / ECHO / VERB / PATT / EVOL / REC.
void ch2FuncApply(uint8_t slot) {
  if (ch2FuncSel > 7 || slot > 7) return;
  switch (ch2FuncSel) {
    case 0:  // SND1 — engine select, lower half (0-7)
      ch2Sound = (uint8_t)constrain((int)slot, 0, 7);
      // Pot-2's DRIFT label is engine-specific (CH2_AMT_LABELS). Request a
      // bar repaint via the dirty flag — NEVER draw here: ch2FuncApply()
      // runs on core 0 (pad handler) and ALL TFT/SPI must happen on core 1
      // (see loop1). A direct drawBarLabels() here raced core 1 on the SPI
      // bus and white-screened the panel. drawBars()'s RES-row refresh is
      // keyed on prevCh2Snd, so it repaints the amount label on core 1.
      ui.dirty = true; ui.barDirty = true;
      break;
    case 1:  // SND2 — engine select, upper half (8-15). Split across two
             // adjacent pads rather than a bank-toggle on one SND page —
             // tap either pad, its 8 tiles light up directly.
      ch2Sound = (uint8_t)constrain((int)slot + 8, 8, CH2_NUM_SOUNDS - 1);
      ui.dirty = true; ui.barDirty = true;
      break;
    case 2:  // PITCH (moved here from slot 1; NOTE, previously slot 2,
             // is gone — see the CH2 FUNC PAGE comment up top for why).
             // Reset motion state so ARP/WALK start predictably.
      ch2PitchMode = slot;
      ch2ArpPos = 0; ch2WalkDeg = 0; ch2ArpPhase = 0;
      break;
    case 3: {  // ECHO
      // Clear the delay line when enabling from OFF — otherwise a stale
      // tail from whenever echo was last on replays out of nowhere. The
      // memset races core 1's per-sample reads, but the loser only reads
      // zeros-in-progress for a few samples: inaudible, and only on the
      // explicit user gesture of turning echo on.
      bool wasOff = (ch2EchoSel == 0);
      ch2EchoSel = slot;
      if (wasOff && slot != 0) memset(ch2EchoBuf, 0, sizeof(ch2EchoBuf));
      ch2ApplyEcho();
      break;
    }
    case 4: {  // VERB — same OFF->ON clear rationale as ECHO above
      bool wasOff = (ch2VerbSel == 0);
      ch2VerbSel = slot;
      if (wasOff && slot != 0) memset(&ch2VerbBufs, 0, sizeof(ch2VerbBufs));
      ch2ApplyVerb();
      break;
    }
    case 5:  // EUC — now driven entirely from ch2PollLiveGestures(): a tap
             // LOADS the slot (custom saved pattern if there is one, else
             // blank) on release, a long hold SAVES ch2Steps into it.
             // Deliberately does nothing here: pads 3-6 dispatch on press,
             // 1/2/7/8 on release, so this path alone can't yet tell a tap
             // from the start of a hold — unlike EVOLVE (case 6) below,
             // which has no hold gesture of its own and dispatches here
             // normally.
      break;
    case 6: {  // EVOLVE — OFF + 7 named modes, plain tap-select same shape
               // as ECHO/VERB above. ANY change (OFF->ON, or switching from
               // one mode to another) snaps back to root and zeroes the bar
               // counter — otherwise switching e.g. STUT->WILD mid-excursion
               // would apply WILD's octave overlay while still counting
               // bars against STUT's cadence, which is nonsense.
      if (slot != ch2EvolSel) ch2EvoResetCadence();
      ch2EvolSel = slot;
      break;
    }
    case 7:  // REC — pot-motion recorder (Part 2); no value-tile action.
      break;
    default: return;
  }
  ui.dirty = true; ui.valDirty = true; ui.infoDirty = true;
}

// FUNC MODE — top row applies value for selected function
void doFuncApply(uint8_t slot) {
  // CH2 EDIT routes here from the same pad call sites; must branch before
  // the FUNC_NONE guard below, since funcSel is deliberately FUNC_NONE
  // for the whole ch2 FUNC session.
  if (ch2EditMode) {
    // NOTE (formerly slot 2) is gone, and its keyboard-not-tiles dispatch
    // went with it — PITCH (now slot 2) is a plain tile-apply page like
    // ECHO/VERB/EUC, so it needs nothing special here.
    ch2FuncApply(slot);
    return;
  }

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
      // ch2's engine select used to hijack this slot whenever ch2SynthMode
      // was on — leaving ch1's engine UNREACHABLE for the whole session.
      // It now lives on the CH2 FUNC page (slot 1, via ch2FuncApply), so
      // SOUND is ch1's again in every mode.
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
// drawStepCellEx: withLabel=false is the playhead-chase variant — skips the
// row-1 FUNC label repaint (fill + border + centred text, the single most
// expensive part of a bottom-row cell) because the label never changes when
// only the cursor moves. This halves bottom-row chase cost, which combined
// with the bmFillDrumBuffer() top-up below is what stops GP2 underrunning
// (heard as crackle mixed into the acid output) now that the drum/ch2 ring
// runs at a real ~31ms backlog instead of the accidental ~383ms.
// All original call sites go through the drawStepCell() wrapper unchanged.
void drawStepCellEx(uint8_t i, bool withLabel) {
  // Top up GP2 before the longest per-tick blocking draw. Called here (not
  // at call sites) so every path — chase, cellsDirty, funcDirty, editDirty,
  // drawMain's loop — is covered by one change. Cheap no-op when topped.
  bmFillDrumBuffer();

  const int bW=36, bH=58, bSp=3, sX=4;
  const int rY[2] = {38, 98};
  const int BAR_TOP=6, BAR_H=44, IND_Y_OFF=50;

  Step& s   = seq.steps[i];
  int row   = i / 8, col = i % 8;
  int bx    = sX + col*(bW+bSp);
  int by    = rY[row];
  uint8_t curForDisplay = (gDisplayCurOverride >= 0) ? (uint8_t)gDisplayCurOverride : seq.cur;
  bool isActive  = (i == curForDisplay && seqIsRunningForDisplay());
  bool noteEditing = pNoteEdit[i];

  // Erase — bottom row extends 22px to cover the label strip below, but
  // ONLY when the label is being repainted this call. The chase variant
  // must stop at bH+3, not bH+4: the label strip starts at by+bH+2
  // (LBL_Y), and a bH+4 erase from by-1 reaches exactly that row — it
  // would shave the label's top border by 1px on every playhead pass.
  // bH+3 still covers the active-step indicator line (by+bH..by+bH+1)
  // and the selection/edit rings (bottom edge by+bH+1).
  int eraseH = (row==1) ? (withLabel ? bH+22 : bH+3) : bH+4;
  tft.fillRect(bx-1, by-1, bW+3, eraseH, C_BG);

  // Cell background. In DRIFT EDIT the non-playhead backgrounds/border use
  // the cooled azure tints (isActive/playhead cell stays bright C_AMB so
  // it still reads as the cursor). This whole-grid wash is the DRIFT-edit
  // indicator; it vanishes the moment you leave edit even if DRIFT keeps
  // playing under the acid screen.
  uint16_t cBg  = ch2EditMode ? C_DR_BG  : C_AMBBG;
  uint16_t cAcc = ch2EditMode ? C_DR_ACC : C_MGND;
  uint16_t cBr  = ch2EditMode ? C_DR_BR  : C_AMBBR;
  if (s.active) tft.fillRoundRect(bx, by, bW, bH, 3, isActive ? C_AMB : (s.accent ? cAcc : cBg));
  else          tft.drawRoundRect(bx, by, bW, bH, 3, cBr);
  if (s.accent && s.active && !isActive) tft.drawRoundRect(bx, by, bW, bH, 3, C_MGN);

  if (noteEditing) tft.drawRoundRect(bx-1, by-1, bW+2, bH+2, 4, C_YEL);

  // CH2 trigger ring — violet outer border on any step with a channel-2
  // pulse programmed. Reads ch2Steps[] LIVE, so it renders in CH2 EDIT and
  // persists automatically when dropping back to normal ch1 editing —
  // through playhead passes, FUNC toggles, and full redraws — with no
  // extra state to maintain. Precedence: the yellow note-edit ring wins
  // while a note is being held (transient editing state trumps a static
  // marker); on row 1 the red FUNC-selected ring is drawn later in the
  // label section and overwrites this one naturally. Geometry is the
  // shared outer-ring slot, already inside every erase variant's bounds,
  // so toggling a step OFF erases the ring with no special casing.
  if (ch2SynthMode && ch2Steps[i] && !noteEditing)
    tft.drawRoundRect(bx-1, by-1, bW+2, bH+2, 4, C_CH2);

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

  // FUNC label strip under bottom-row pads (row 1 only). Skipped in the
  // chase variant — the erase above didn't touch the strip, so the label
  // on screen is still valid. The isSelected red outline is the one thing
  // that DOES live inside the bH+4 erase zone (it rings the cell itself),
  // so it must be repainted in both variants or a selected FUNC cell would
  // lose its ring every time the playhead passes through it.
  if (row == 1) {
    bool isSelected = ch2EditMode
      ? (funcMode && ch2FuncSel == (uint8_t)(i - 8))
      : (funcMode && (funcSel != FUNC_NONE) && ((int)(i-8) == (int)funcSel));
    if (withLabel) {
      const int LBL_Y = by + bH + 2;
      const int LBL_H = 18;
      // CH2 EDIT swaps the label set — ch2's own FUNC page, ch1's row
      // untouched everywhere else. ch2EditMode entry/exit sets fullDirty,
      // so the swap repaints without any extra plumbing.
      const char* fn = ch2EditMode ? CH2FUNCNAMES[i - 8] : FUNCNAMES[i - 8];

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
    }
    if (isSelected) tft.drawRoundRect(bx-1, by-1, bW+2, bH+2, 4, C_RED);
  }
}

// Original entry point — full repaint including the row-1 label strip.
// Every pre-existing call site uses this unchanged; only the playback
// cursor-chase paths call drawStepCellEx(i, false) directly.
void drawStepCell(uint8_t i) { drawStepCellEx(i, true); }

// =====================================================================
// DISPLAY — VALUE STRIP
// =====================================================================
void drawValStrip() {
  bmFillDrumBuffer();  // text-heavy strip, redrawn on every FUNC selection and pot tick

  const int VS_Y=12, VS_H=20, VS_W=40;
  tft.fillRect(0, VS_Y, 320, VS_H, C_BG);

  // EASTER EGG: Acid Walks pattern strip — shown when active, unless the
  // user has entered FUNC mode (FUNC's own value strip takes priority so
  // KEY/SOUND/etc selection remains visible and usable as normal)
  if (walksMode && !funcMode) {
    tft.setTextSize(1);
    for (uint8_t t = 0; t < NUM_WALKS; t++) {
      bmFillDrumBuffer();  // one tile per iteration — keep GP2 fed
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

  // ── CH2 FUNC value strips ─────────────────────────────────────────
  // Sits before the funcSel guard below: funcSel stays FUNC_NONE for the
  // whole ch2 FUNC session (see CH2 FUNC PAGE comment), so without this
  // branch the strip would draw blank.
  if (ch2EditMode && funcMode) {
    if (ch2FuncSel > 7) return;   // no function selected yet — blank, ch1 parity
    tft.setTextSize(1);
    // Every ch2FuncSel case falls through to the shared label strip below —
    // activeIdx lights whichever tile applies for that page.
    const char** labels = nullptr;
    int activeIdx = -1;
    switch (ch2FuncSel) {
      case 0: labels = CH2_SND_NAMES;    // SND1 — engines 0-7
              activeIdx = (ch2Sound < 8) ? ch2Sound : -1;
              break;
      case 1: labels = &CH2_SND_NAMES[8];  // SND2 — engines 8-15, same array
              // sliced 8 entries in (contiguous, so this just works as a
              // const char** — no duplicate name table to keep in sync).
              activeIdx = (ch2Sound >= 8) ? (ch2Sound - 8) : -1;
              break;
      case 2: labels = CH2_VS_PITCH;  activeIdx = ch2PitchMode; break;  // moved from slot 1
      case 3: labels = CH2_VS_ECHO;   activeIdx = ch2EchoSel;   break;
      case 4: labels = CH2_VS_VERB;   activeIdx = ch2VerbSel;   break;
      case 5: labels = CH2_VS_EUC;    activeIdx = (ch2EucSel == 255) ? -1 : ch2EucSel; break;
      case 6: labels = CH2_VS_EVOL;   activeIdx = ch2EvolSel;   break;
      case 7: {  // REC — pot-motion recorder status banner
        bool rec = ch2Recording;
        bool clr = gCh2RecClrFlashMs && (millis() - gCh2RecClrFlashMs) < 700;
        tft.fillRect(0, VS_Y, 320, VS_H, rec ? 0x6000 : 0x0861);
        tft.drawRect(0, VS_Y, 320, VS_H, rec ? C_RED : C_CH2);
        tft.setTextColor(C_WHT); tft.setCursor(4, VS_Y + (VS_H - 7) / 2);
        if (clr) { tft.print("REC CLEARED - lanes wiped"); return; }
        tft.print(rec ? "* RECORDING - move pots (tap=stop)  "
                      : "REC  tap=start  hold=clear   ");
        if (ch2AutoRec & 1) tft.print("OCT ");
        if (ch2AutoRec & 2) tft.print("AMT ");
        if (ch2AutoRec & 4) tft.print("DCY ");
        return;
      }
      default: return;  // unreachable — all 8 slots live
    }
    for (uint8_t t = 0; t < 8; t++) {
      bmFillDrumBuffer();  // one tile per iteration — keep GP2 fed
      int tx = t * VS_W;
      bool isCur = (t == (uint8_t)activeIdx);
      // Violet accents — matches the ch2 trigger ring.
      uint16_t fill = isCur ? C_CH2 : 0x0861;
      uint16_t edge = isCur ? C_WHT : C_DGR;
      uint16_t txt  = isCur ? C_WHT : C_MGR;
      // EUC saved-custom tiles get a green edge (same "you chose this"
      // green HARM's lock state used to use) so it's visually obvious the
      // slot no longer plays the stock euclidean fill. A brief white pulse
      // marks the instant a save actually lands.
      if (ch2FuncSel == 5 && (ch2EucSavedMask & (1 << t))) {
        edge = isCur ? C_WHT : C_GRN;
      }
      if (t == (uint8_t)ch2EucFlashTile) {
        fill = C_WHT; edge = C_WHT; txt = C_BG;
      }
      tft.fillRect(tx, VS_Y, VS_W, VS_H, fill);
      tft.drawRect(tx, VS_Y, VS_W, VS_H, edge);
      tft.setTextColor(txt);
      int fw = strlen(labels[t]) * 6;
      tft.setCursor(tx + (VS_W - fw) / 2, VS_Y + (VS_H - 7) / 2);
      tft.print(labels[t]);
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
      bmFillDrumBuffer();  // one tile per iteration — keep GP2 fed
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
    bmPrintFed("FX ASSIGN — use pads 1-8 to select effect, then any pad to assign");
    return;
  }

  // PLEN: 16 individual step tiles
  if (funcSel == FUNC_PLEN) {
    const int TW = 320 / 16;
    for (uint8_t t = 0; t < 16; t++) {
      bmFillDrumBuffer();  // one tile per iteration — keep GP2 fed
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
      // ch2 engine labels moved to the CH2 FUNC page (branch above) —
      // SOUND is ch1's again in every mode.
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
    bmFillDrumBuffer();  // one tile per iteration — keep GP2 fed
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
// DCY bar width: gDecaySpeed is deliberately FROZEN while in CH2 EDIT (see
// updateControl()'s ch2EditMode branch) so twisting the knob there doesn't
// also warp ch1's live bassline decay. But the pot IS live in that mode —
// it's driving ch2Amount instead — so the on-screen bar needs to read
// whichever one is actually moving, or it looks broken (pot works, bar
// doesn't) even though the DCY-pot-fed variable behind it is correct.
static inline int dcyBarWidth(int W) {
  if (ch2EditMode) return constrain(((int)ch2Amount * W) / 255, 0, W);
  return constrain((int)((gDecaySpeed - 3) * W) / 1023, 0, W);
}

// The strip right of the DCY bar (x=224..320). Shared by drawBars() and the
// full-redraw path, which previously each printed the KEY WALK label with
// their own copy of the same three lines — and both leaked that acid-only
// label onto the DRIFT screen. One helper now owns the region so the two
// paths can't drift apart again.
//
// Acid keeps the KEY WALK label here. DRIFT shows nothing in this region
// now — HARM used to mirror its auto-walk status here so it stayed visible
// after navigating away from the HARM page, but EVOLVE has no equivalent
// running state worth surfacing outside its own value-strip tile (which
// already shows the active intensity preset whenever that page is open).
// The fillRect is unconditional — switching engines must actively wipe the
// other engine's text, not merely decline to redraw it.
// Set when something else has painted over the x=224 tag region (full
// redraw, ACCENT EDIT), so the next drawDcyRowTag() repaints even though
// its content hasn't changed. A global rather than a defaulted parameter
// because .ino auto-prototyping and default arguments don't mix well.
bool gDcyTagForce = true;

void drawDcyRowTag(int y, int barH, int rowPitch) {
  // Repaint ONLY when the content changes. drawBars() runs on every barDirty
  // pass and pot movement sets that constantly, so an unconditional
  // fillRect+print here flashes on every call. That went unnoticed for the
  // KEY WALK label — dim grey on black barely registers — but the bright
  // green DRIFT walk tag made it obvious. Same fix drawBars() already uses
  // for the bar fills themselves (prevDcyw / prevCh2Edit).
  static bool    lastCh2    = false;
  static uint8_t lastKw     = 0;
  static const char* lastTop = nullptr;
  // Gesture legends show ONLY on pages whose HOLD does something you can't
  // guess from looking — contextual help that disappears when it stops being
  // relevant, rather than permanent text you stop seeing after a week. The
  // rows above the DCY row are free in DRIFT (acid uses them for the Acid
  // Walks name and the tempo readout), but BOTH get cleared whenever their
  // bar repaints, so this helper has to own them or a pot move wipes them.
  //
  // KBRD/NOTE's "transpose" hint lived here — removed along with the page.
  // HARM's "hold=count / tap=select" hint went with HARM — EVOLVE has no
  // hold gesture at all, it's a plain tap-select page like ECHO/VERB/PITCH.
  const char* hintTop = nullptr;
  const char* hintBot = nullptr;
  if (ch2EditMode && funcMode) {
    if (ch2FuncSel == 5) { hintTop = "hold=save";  hintBot = "tap=load";   }
  }
  if (!gDcyTagForce && ch2EditMode == lastCh2 &&
      hintTop == lastTop && kwMode == lastKw) return;
  gDcyTagForce = false;
  lastCh2 = ch2EditMode;
  lastKw = kwMode;               lastTop = hintTop;

  // Top up GP2 before the SPI below. Every other draw path in this file does
  // this — drawStepCellEx, drawValStrip, drawInfoStrip, drawMain, loop1 —
  // because an uninterrupted burst of TFT writes starves the drum/DRIFT ring
  // buffer. This helper was added later and never got it: up to 14 tft calls
  // (three fillRects plus the legend text) with no top-up in between. Cheap
  // no-op when the buffer is already full.
  bmFillDrumBuffer();

  // Clear the two rows above whenever a legend appears OR disappears — on the
  // way out this wipes it, on the way in it clears whatever was there before.
  tft.fillRect(224, y - rowPitch * 2, 96, barH + 1, C_BG);
  tft.fillRect(224, y - rowPitch,     96, barH + 1, C_BG);
  if (hintTop) {
    tft.setTextColor(C_MGR);              // grey — secondary to the green status
    tft.setCursor(224, y - rowPitch * 2); tft.print(hintTop);
    tft.setCursor(224, y - rowPitch);     tft.print(hintBot);
  }

  tft.fillRect(224, y, 96, barH + 1, C_BG);
  if (ch2EditMode) return;   // no DCY-row status text needed — EVOLVE's tile
                              // highlight in the value strip already shows
                              // its state, same as ECHO/VERB/EUC never
                              // needed one either. HARM's SEQ/BARS readout
                              // went with HARM.
  // KEY WALK status. kwMode 0 is the default, so printing "OFF" was just
  // noise on the acid screen — blank it and let the empty space mean off,
  // matching the DRIFT walk tag above. When a walk IS running this is the
  // only on-screen indication of WHICH one (4TH / OCTWAVE / VAMP3 / ...),
  // so the label itself earns its place — don't remove it outright.
  if (kwMode == 0) return;                 // region already cleared above
  tft.setTextColor(C_CYN);
  tft.setCursor(224, y);
  tft.print(KWNAMES[kwMode]);
}

void drawBars() {
  // Top up GP2 before the SPI burst below. drawBars() does ~23 tft writes
  // with no feed points, which was survivable when barDirty only fired on a
  // pot move. V5 sets it far more often — an EVOLVE mutation, ch2 FUNC
  // page switches and FUNC enter/exit all mark it — so the same unfed burst
  // now happens routinely while GP2 may be carrying DRIFT rather than cheap
  // drum samples. drawDcyRowTag() at the end feeds again, splitting the run.
  bmFillDrumBuffer();
  const int bY=180, bP=10, bBarH=6;

  static int      prevCw      = -1;
  static int      prevResW    = -1;
  static int      prevBpmW    = -1;
  static int      prevDcyw    = -1;
  static uint16_t prevTempo   = 0;
  static bool     prevTempMode= false;
  static bool     prevMixEdit = false;
  static bool     prevCh2Edit    = false;  // tracks DRIFT-edit transitions so
                                           // the RES/DTUN label repaints once
                                           // on entering/leaving DRIFT
  static uint8_t  prevCh2Snd     = 255;    // engine change -> DTUN/-- label refresh

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

  // CUT (OCT in DRIFT). The DRIFT fill uses C_CH2 so all three DRIFT pot
  // bars share the azure identity — previously this bar stayed C_RED
  // while RES/DCY went azure, leaving the first bar an odd one out.
  // ch2EditMode is in the refresh test so it repaints on entering/leaving
  // DRIFT, not only when the pot moves.
  // The width TESTED for change must be the width actually DRAWN. This used
  // to compute cw from the DRIFT/acid source, test only that, then draw a
  // separate mix bar from mixAcidLevel inside the if. During MIX EDIT the
  // tested value never moved — the DRIFT pot handler is skipped while
  // mixing, so ch2OctDisplay is frozen — so the ACID bar never repainted as
  // you turned the pot. The gain WAS changing; you just couldn't see it.
  // The DRIFT and DRUM mix bars below already tested their own mix level;
  // only this one was inverted.
  int cw = mixEditMode ? constrain((mixAcidLevel * FW) / 255, 0, FW)
         : ch2EditMode ? constrain((int)((ch2OctDisplay * FW) / 1023), 0, FW)
                       : constrain((gCutoffDisplay * FW) / 255, 0, FW);
  if (cw != prevCw || mixEditMode != prevMixEdit || ch2EditMode != prevCh2Edit) {
    if (mixEditMode) {
      drawBarFill(bY + 1, cw, C_AMB);
      tft.fillRect(224, bY, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY); tft.print("ACID");
    } else {
      drawBarFill(bY + 1, cw, ch2EditMode ? C_CH2 : C_RED);
    }
    prevCw = cw;
  }

  // EASTER EGG: show the active Acid Walks track name to the right of the
  // CUT bar (this area is otherwise unused on this row).
  if (!mixEditMode) {
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

  // RES / BPM / MIX(DRIFT)
  bool tempoMode = (funcMode && funcSel == FUNC_TEMPO);
  if (mixEditMode) {
    int dmw = constrain((mixDriftLevel * FW) / 255, 0, FW);
    if (dmw != prevResW || mixEditMode != prevMixEdit) {
      drawBarFill(bY + bP + 1, dmw, C_AMB);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY+bP); tft.print("DRFT");
      prevResW = dmw;
    }
  } else if (tempoMode) {
    int bw = constrain(((int)(seq.tempo - 40) * FW) / 260, 0, FW);
    bool flash = (tapLastMs > 0 && (millis() - tapLastMs) < 200);
    if (bw != prevBpmW || tempoMode != prevTempMode || seq.tempo != prevTempo || flash || mixEditMode != prevMixEdit) {
      drawBarFill(bY + bP + 1, bw, flash ? C_WHT : C_CYN);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      tft.setTextColor(C_CYN); tft.setCursor(4,   bY+bP); tft.print("BPM");
      tft.setTextColor(C_CYN); tft.setCursor(224, bY+bP); tft.print(seq.tempo);
      if (tempoMode != prevTempMode) tft.drawRect(36, bY+bP, 180, bBarH, C_CYN);
      prevBpmW = bw; prevTempo = seq.tempo; prevTempMode = tempoMode;
    }
  } else {
    int rw = ch2EditMode ? (constrain((int)ch2AmtDisplay, 0, 1023) * FW) / 1023
                         : (constrain((int)gResonanceDisplay, 0, 1023) * FW) / 1023;
    if (rw != prevResW || tempoMode != prevTempMode || mixEditMode != prevMixEdit
        || ch2EditMode != prevCh2Edit || ch2Sound != prevCh2Snd) {
      drawBarFill(bY + bP + 1, rw, ch2EditMode ? C_CH2 : C_YEL);
      tft.fillRect(224, bY+bP, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP, 36, bBarH+1, C_BG);
      // DRIFT relabels this pot DTUN (detune); azure to match the mode.
      // This live refresh was the culprit: it repainted "RES" on every
      // pot move, stomping the DTUN that drawBarLabels() had written.
      if (ch2EditMode) { tft.setTextColor(C_CH2); tft.setCursor(4, bY+bP);
                         tft.print(CH2_AMT_LABELS[ch2Sound & 15]); }
      else             { tft.setTextColor(C_WHT); tft.setCursor(4, bY+bP); tft.print("RES");  }
      if (tempoMode != prevTempMode) tft.drawRect(36, bY+bP, 180, bBarH, C_DGR);
      prevResW = rw; prevTempMode = tempoMode;
    }
  }

  // DCY / MIX(DRUMS)
  if (mixEditMode) {
    int rmw = constrain((mixDrumLevel * FW) / 255, 0, FW);
    if (rmw != prevDcyw || mixEditMode != prevMixEdit) {
      drawBarFill(bY + bP*2 + 1, rmw, C_AMB);
      tft.fillRect(224, bY+bP*2, 96, bBarH+1, C_BG);
      tft.fillRect(0, bY+bP*2, 36, bBarH+1, C_BG);
      tft.setTextColor(C_AMB); tft.setCursor(4, bY+bP*2); tft.print("DRUM");
      prevDcyw = rmw;
    }
    prevMixEdit = true;
    gDcyTagForce = true;  // MIX EDIT paints over x=224; repaint on exit
    return;  // skip the normal WALK-label row below — MIX EDIT owns this row
  } else {
    int dcyw = ch2EditMode ? constrain((int)((ch2DcyDisplay * FW) / 1023), 0, FW)
                           : dcyBarWidth(FW);
    if (dcyw != prevDcyw || mixEditMode != prevMixEdit || ch2EditMode != prevCh2Edit) {
      drawBarFill(bY + bP*2 + 1, dcyw, ch2EditMode ? C_CH2 : C_MGR);
      prevDcyw = dcyw;
    }
  }
  prevMixEdit    = mixEditMode;
  prevCh2Edit    = ch2EditMode;
  prevCh2Snd     = ch2Sound;

  // Right of the DCY bar: KEY WALK on acid, PATT's gesture legend on DRIFT
  // (see drawDcyRowTag — HARM's auto-walk status used to live here too).
  drawDcyRowTag(bY + bP*2, bBarH, bP);
}

// =====================================================================
// DISPLAY — INFO STRIP
// =====================================================================
void drawInfoStrip() {
  bmFillDrumBuffer();  // text-heavy strip

  const int IS_Y = 213;
  tft.drawFastHLine(0, IS_Y, SW, C_DGR);
  tft.fillRect(0, IS_Y+1, SW, SH-(IS_Y+1), C_BG);

  const int ty = IS_Y + 10;

  if (mixEditMode) {
    tft.setTextSize(1);
    tft.setTextColor(C_AMB);
    tft.setCursor(4, ty);
    bmPrintFed("MIX EDIT - CUT=ACID RES=DRFT DCY=DRUM (15+16=exit)");
    return;
  }

  // DRIFT edit banner — the channel finally gets its name on screen.
  // Same takeover idiom as ACCENT EDIT above; azure = DRIFT identity.
  if (ch2EditMode) {
    tft.setTextSize(1);
    tft.setTextColor(C_CH2);
    tft.setCursor(4, ty);
    bmPrintFed("DRIFT EDIT - FUNC:NOTE = play top row; 9+10+11=exit, 9+10=drums");
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

  bmFillDrumBuffer();  // mid-strip top-up — text above ≈ 15 chars, more below
  tft.setTextColor(C_MGR);
  tft.setCursor(118, ty);
  { int sslot = 0;
    for (int s=0; s<8; s++) if (SOUND_MAP[s] == seq.sound) { sslot=s; break; }
    tft.print(VSTRIP_SND[sslot]);
  }

  tft.setTextColor(C_GRN);
  tft.setCursor(154, ty);
  tft.print(seq.len); tft.print("ST");

  bmFillDrumBuffer();  // mid-strip top-up
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
    // Only one character of width is allocated here (see the x=316/x=322
    // layout comment above), so direction is shown by colour rather than
    // by a second letter: cyan for SYNC IN (unchanged), magenta for SYNC
    // OUT.
    tft.setTextColor(syncOutMode ? C_MGN : C_CYN);
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
  // Clear the label column (x 0..35) for all three rows BEFORE printing.
  // Without this, calling drawBarLabels() on an engine change painted the
  // new pot-2 amount label directly over the old one — and since the
  // labels differ in width (SHMR/IDX/MIX/FOLD/DUTY/DTUN), the tail of a
  // longer previous label survived under a shorter new one, giving two
  // overlapping unreadable texts. This is the pot-2 double-text bug.
  tft.fillRect(0, BAR_Y,          36, 8, C_BG);
  tft.fillRect(0, BAR_Y+BAR_P,    36, 8, C_BG);
  tft.fillRect(0, BAR_Y+BAR_P*2,  36, 8, C_BG);
  if (ch2EditMode) {
    // DRIFT: pots mean octave / per-engine amount (or detune) / decay.
    tft.setTextColor(C_CH2); tft.setCursor(4, BAR_Y);         tft.print("OCT");
    tft.setTextColor(C_CH2); tft.setCursor(4, BAR_Y+BAR_P);   tft.print(CH2_AMT_LABELS[ch2Sound & 15]);
    tft.setTextColor(C_CH2); tft.setCursor(4, BAR_Y+BAR_P*2); tft.print("DCY");
    return;
  }
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

  // Chain-active indicator — sits right after the pad-6/slot-4 dot (the
  // last of the four above). Only meaningful during playback; the
  // chain-build screen is a full takeover so it never needs this.
  {
    uint8_t lastCol = (3 + 2) % 8;
    int afterDotX = sX + lastCol * (bW + bSp) + bW + 2;
    tft.fillRect(afterDotX, 0, 26, 12, C_BG);
    if (chainActive) {
      tft.setTextColor(C_ORG); tft.setTextSize(1);
      tft.setCursor(afterDotX, 3); tft.print("CH");
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

// Skipping the fillScreen() below when already on the main screen (no other
// screen's remnants to wipe, e.g. an automatic chain-pattern switch) cuts
// real TFT time — bmFillDrumBuffer() has to synthesize enough samples to
// cover however much real time drawMain() takes, so less TFT time means
// less audio catch-up work too. Confirmed with timing instrumentation
// during development: this alone cut a ~303ms redraw (with drums playing)
// down to ~183ms by eliminating a ~103ms full-screen clear that was pure
// waste in the in-place-refresh case.
void drawMain() {
  tft.startWrite();
  if (!gSkipMainScreenClear) {
    // Full screen clear — ensures FX assign remnants (bars, info, footer) are wiped.
    // Banded (see bmFillScreenFed): the old atomic fillScreen here was the
    // "sometimes" crackle on FUNC/FX transitions — anything setting fullDirty
    // (FX-assign exit, chain abort, patch load) paid ~103ms in one call.
    bmFillScreenFed(C_BG);
    tft.fillRect(0, 96, SW, 2, C_BG);
  }
  gSkipMainScreenClear = false;  // one-shot — always clear again unless explicitly requested
  bmFillDrumBuffer();  // top up GP2 DMA buffer — drawMain() is the longest blocking call

  drawValStrip();
  drawSlotDots();
  bmFillDrumBuffer();
  // Freeze seq.cur for this loop — it takes real time to run and seq.cur
  // keeps advancing on core0 throughout, so without freezing, whichever
  // cell happens to coincide with the live value at ITS OWN draw moment
  // ends up highlighted (see comment at gDisplayCurOverride's declaration).
  gDisplayCurOverride = seq.cur;
  for (uint8_t i = 0; i < 16; i++) {
    drawStepCell(i);
    if ((i & 3) == 3) bmFillDrumBuffer();  // every 4 cells — keep buffer fed during this loop
  }
  gHighlightedStep = (uint8_t)gDisplayCurOverride;  // record what this pass actually drew
  gDisplayCurOverride = -1;                          // release — chase logic uses live seq.cur again

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

  int dcyw = dcyBarWidth(BFW);
  drawBarFillM(BAR_Y + BAR_P*2 + 1, dcyw, C_MGR);

  gDcyTagForce = true;   // full redraw wiped the region — repaint regardless
  drawDcyRowTag(BAR_Y + BAR_P*2, BAR_BARH, BAR_P);
  bmFillDrumBuffer();

  drawInfoStrip();
  bmFillDrumBuffer();
  tft.endWrite();
}

// Moving-playhead redraw (un-light the old step cell, light the current one).
// Pulled out of updateMain() so doDraw() can also call it BEFORE its barDirty
// early-return: turning a DRIFT pot sets barDirty every control tick, which
// otherwise returns at drawBars() on every pass and freezes the on-screen
// step. Cheap — redraws at most two cells. Grid-screen only (caller guards on
// !funcMode) so it never paints over the FUNC value tiles.
void chasePlayhead() {
  // Called every doDraw pass (see doDraw / updateMain), so it MUST be a no-op
  // unless the playhead actually moved or play/stop toggled — otherwise it
  // erase+redraws the same cell forever (the "flashing when stopped" flicker).
  // seq.cur is snapshotted ONCE and pinned via gDisplayCurOverride so the
  // highlight can't land on the wrong step if core 0 advances seq.cur partway
  // through the redraw — that divergence was leaving cells lit-but-never-
  // cleared (the orange trail). lastPlaying forces one redraw on stop so the
  // final cell drops its cursor tint (the stop handler doesn't repaint cells).
  static bool lastPlaying = false;
  uint8_t cur    = seq.cur;
  bool    playing = seqIsRunningForDisplay();
  if (gHighlightedStep == cur && playing == lastPlaying) return;
  // With nothing running there is no cursor on screen at all —
  // drawStepCellEx only tints a cell when seqIsRunningForDisplay() is
  // true. But seq.cur keeps advancing regardless (it's what clocks
  // drums on GP15, and either voice on GP2), so without this guard every
  // step repainted two cells that render identically to their unlit
  // state: no highlight moves, you just see the repaint flashing along
  // the row. Track cur so a later start doesn't erase a stale cell, and
  // bail before drawing. The playing->stopped transition still falls
  // through (lastPlaying differs) so the final lit cell drops its tint
  // exactly once.
  if (!playing && !lastPlaying) { gHighlightedStep = cur; return; }
  gDisplayCurOverride = cur;
  if (gHighlightedStep != 255 && gHighlightedStep != cur) drawStepCellEx(gHighlightedStep, false);
  drawStepCellEx(cur, false);
  gDisplayCurOverride = -1;
  gHighlightedStep = cur;
  lastPlaying = playing;
}

void updateMain() {
  chasePlayhead();
  drawBars();
  bmFillDrumBuffer();  // drawInfoStrip below is text-heavy — top up first
  drawInfoStrip();
}

// =====================================================================
// DISPLAY — CHAIN BUILD SCREEN
// =====================================================================
void drawChainMode() {
  bmFillScreenFed(C_BG);   // banded full clear — same idiom as drawFXAssign

  tft.setTextColor(C_WHT); tft.setTextSize(2);
  tft.setCursor(4, 4); tft.print("CHAIN BUILD");

  tft.fillRect(200, 0, SW-200, 22, C_BLU);
  tft.setTextColor(C_CYN); tft.setTextSize(1);
  tft.setCursor(202, 7);
  tft.print(chainLen); tft.print("/16");

  // 4 slot buttons — same visual language as the FX-assign buttons, but
  // mapped to pads 3-6 / slots 1-4. Empty slots are dimmed since tapping
  // an empty slot still appends it to the chain (it'll just load nothing
  // useful) — this is a visual warning, not a block.
  const int eW=74, eH=40, eSp=4, eX=4, eY=28;
  for (uint8_t s = 0; s < 4; s++) {
    int bx = eX + s * (eW+eSp);
    bool has = slotHasData[s];
    tft.drawRoundRect(bx-1, eY-1, eW+2, eH+2, 4, C_BG);
    tft.fillRoundRect(bx, eY, eW, eH, 3, has ? C_DGR : C_BG);
    tft.drawRoundRect(bx, eY, eW, eH, 3, has ? C_YEL : C_DGR);
    tft.setTextColor(has ? C_YEL : C_DGR); tft.setTextSize(2);
    tft.setCursor(bx+eW/2-6, eY+eH/2-8); tft.print(s+1);
  }

  // Growing sequence — 16-cell grid, 8 per row, same layout math as the
  // FX-assign step grid. The most recently appended entry is highlighted
  // so a tap has visible confirmation even though the physical pad LED
  // can't be controlled from here (it's wired to the switch itself).
  const int sY = 84;
  tft.setTextSize(1);
  tft.setTextColor(C_CYN);
  tft.setCursor(4, sY);
  tft.print(chainLen == 0 ? "tap slots above to build the chain:" : "sequence:");

  for (uint8_t i = 0; i < 16; i++) {
    int bx = 4  + (i%8) * 39;
    int by = sY + 14 + (i/8) * 28;
    bool filled  = (i < chainLen);
    bool newest  = (filled && i == chainLen-1);
    uint16_t fc = filled ? (newest ? C_ORG : C_YEL) : C_DGR;
    if (newest) tft.fillRoundRect(bx, by, 36, 24, 2, C_ORG);
    else        tft.drawRoundRect(bx, by, 36, 24, 2, fc);
    tft.setTextColor(newest ? C_BG : fc); tft.setTextSize(1);
    tft.setCursor(bx+3, by+4); tft.print(i+1);
    tft.setCursor(bx+3, by+14);
    if (filled) tft.print(chain[i]+1);
    else        tft.print("-");
  }

  // Footer — two lines, same layout as the FX-assign footer fix
  const int footY = 210, footH = SH - footY;
  tft.fillRect(0, footY, SW, footH, C_DGR);
  tft.setTextColor(C_WHT); tft.setTextSize(1); tft.setTextWrap(false);
  tft.setCursor(4, footY+4);
  bmPrintFed("tap pads 3-6 to add to the chain");
  tft.setCursor(4, footY+16);
  bmPrintFed("12+13 = play chain   FUNC = cancel");
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
  int dcyw = dcyBarWidth(FW2);
  if (cw > 0)   tft.fillRect(FX2, bY+1,      cw,   FH2, C_RED);
  if (cw < FW2) tft.fillRect(FX2+cw, bY+1,   FW2-cw, FH2, C_BG);
  if (rw > 0)   tft.fillRect(FX2, bY+bP+1,   rw,   FH2, C_YEL);
  if (rw < FW2) tft.fillRect(FX2+rw, bY+bP+1, FW2-rw, FH2, C_BG);
  if (dcyw > 0)   tft.fillRect(FX2, bY+bP*2+1, dcyw, FH2, C_MGR);
  if (dcyw < FW2) tft.fillRect(FX2+dcyw, bY+bP*2+1, FW2-dcyw, FH2, C_BG);
}

void drawFXAssign() {
  if (fxAssignFresh) {
    bmFillScreenFed(C_BG);   // banded full clear — wipes all remnants from previous screen
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
    int dcyw = dcyBarWidth(FW2);
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
  if (chainMode) {
    if (ui.fullDirty) { ui.fullDirty=false; drawChainMode(); return; }
    return;
  }
  if (ui.fullDirty) { ui.fullDirty=false; drawMain(); return; }
  // Chase the playhead BEFORE any partial-redraw early-return can starve it.
  // A DRIFT pot turn sets a redraw flag every control tick — whichever flag
  // it is (bar/cells/func/edit), servicing it must not freeze the moving
  // step. chasePlayhead() is a no-op unless the step actually moved, so this
  // is cheap. Grid-screen only (!funcMode) so it never paints over FUNC tiles.
  if (!funcMode) chasePlayhead();
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
  // Entering/leaving drum mode never touches GP15 (acid) at all, and
  // doesn't displace DRIFT either — drums and DRIFT are additively summed
  // together on GP2 (see bmFillDrumBufferTo() in BeatMachine2.ino), not
  // mutually exclusive. There is nothing to displace and nothing to
  // refuse: this always succeeds.
  bmMode = !bmMode;
  if (bmMode) {
    // Entering drum mode — acid (GP15) and DRIFT (GP2, alongside drums)
    // KEEP PLAYING, unmuted, exactly as they were; bmMode only affects the
    // UI/pad focus, not what's audible. advanceStep() ticks regardless of
    // bmMode -> bmTriggerStep() drives drums perfectly in parallel. If
    // nothing was playing anywhere, seq.running stays false -> drums
    // silent until started (pads 1+2, handled entirely in
    // BeatMachine2.ino's bmMode-gated block).
    bmAcidWasRunning = acidPlaying;  // keep legacy flag in sync
    if (!syncMode && !bmAlarmStarted) bmStartAudio();  // no-op safety net; setup() already started it
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

// SYNC OUT: drives GP2 as a hardware-PWM pulse train derived from this
// device's own tempo, when syncOutMode is active (see the BOOT MODE
// DETECT block in setup()). No-op otherwise. Called every
// updateControl() tick (256Hz); only touches the PWM hardware when the
// driven period has actually changed, or when starting/stopping, to
// keep this to a handful of register writes per tempo change rather
// than per tick.
//
// Design notes:
// - SYNC_OUT_PPQN (24) matches DIN sync standard, spread evenly across
//   each quarter note using seq.interval (the current 16th-note duration
//   in microseconds, already tracked for SYNC IN / internal timing) —
//   one quarter is 4x that, and PPQN pulses are spaced quarterUs/PPQN
//   apart.
// - Pulses only run while seq.running is true. DIN sync has no separate
//   start/stop message; the convention this follows is that the clock
//   line simply goes idle (held low) when the source transport stops,
//   rather than free-running through a stopped pattern — most receiving
//   gear treats "pulses present" as "running".
// - Pulse WIDTH is held constant (SYNC_OUT_PULSE_US) across tempo, not a
//   fixed duty percentage — the duty value fed to analogWrite() is
//   recomputed from the new period every time the period changes, so
//   the pulse looks the same width to receiving gear at 20 BPM as it
//   does at 300 BPM.
void syncOutUpdate() {
  if (!syncOutMode) return;

  if (!seq.running) {
    if (syncOutRunning) {
      analogWrite(SYNC_IN, 0);     // pulses off, line idle low
      syncOutRunning  = false;
      syncOutPeriodUs = 0;         // force a fresh reprogram on the next start
    }
    return;
  }

  uint32_t quarterUs = seq.interval * 4UL;
  uint32_t periodUs  = quarterUs / SYNC_OUT_PPQN;
  if (periodUs < 100) periodUs = 100;  // sanity floor — guards the divide
                                        // below rather than asking the PWM
                                        // hardware for an implausible rate

  if (periodUs != syncOutPeriodUs) {
    syncOutPeriodUs = periodUs;
    analogWriteFreq(1000000UL / periodUs);
    uint32_t duty = (uint32_t)(((uint64_t)SYNC_OUT_PULSE_US * SYNC_OUT_PWM_RANGE) / periodUs);
    if (duty >= SYNC_OUT_PWM_RANGE) duty = SYNC_OUT_PWM_RANGE - 1;  // never fully-on
    analogWrite(SYNC_IN, duty);
  }
  syncOutRunning = true;
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
        if (chainActive) chainTickCount = 0;  // resync chain to the restarted loop
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
  // SECOND-LAYER chord fire — pads 9+10 held >= LAYER_HOLD_MS. The engine
  // is decided in fireLayerChord() at fire time, by whether pad 11 is
  // also physically down. pChord/pState for BM_SW_A/B are deliberately
  // left as set (see doModeSwitch comments — resetting them here caused
  // spurious step-toggles on release).
  if (layerArmed && (millis()-layerArmMs) >= LAYER_HOLD_MS) {
    layerArmed = false;
    fireLayerChord();
  }
  // Disarm if either anchor pad released before the hold completes
  if (layerArmed && (digitalRead(PAD_PINS[BM_SW_A])!=LOW || digitalRead(PAD_PINS[BM_SW_B])!=LOW)) {
    layerArmed=false;
  }

  // PATTERN CHAINING chord fire — pads 12+13 held >= CHAIN_HOLD_MS. The
  // hold (and the abort check right after) exist to let a genuine WALK
  // attempt (11+12+13+14) prove itself before this commits — see
  // CHAIN_HOLD_MS's declaration. Blocked while FUNC or FX-assign mode is
  // active so modes can't stack; tapping the gesture again at any other
  // time (building OR a chain already playing back) drops the current
  // chain and starts a fresh build, same as before this was made a hold.
  if (chainArmed && (millis()-chainArmMs) >= CHAIN_HOLD_MS) {
    chainArmed = false;
    if (!funcMode && !fxAssignMode) {
      if (chainMode) {
        // Exit build → commit and play, if anything was actually built
        chainMode = false;
        if (chainLen > 0) {
          chainActive = true; chainPos = 0; chainTickCount = 0;
          loadPatch(chain[0]);
          loadCh2Patch(chain[0]);  // see the mid-playback swap's identical
                                    // call for why — DRIFT needs to reload
                                    // here too, not just on later advances,
                                    // or the very first chain segment
                                    // starts out already stuck on whatever
                                    // DRIFT happened to be playing before
          // Always start the chain from the first slot's own step 0 —
          // previously this only happened when acid wasn't already
          // running (see the seq.cur line that used to live inside the
          // block below); the far more common case of "already playing
          // when you exit chain-build" left seq.cur wherever it was
          // before you ever opened the chain menu, so the chain's first
          // segment picked up mid-pattern instead of announcing itself
          // from the top. Set to len-1, not 0 directly: this runs outside
          // advanceStep(), so the NEXT tick's own nextPatStep() call is
          // what actually lands on step 0 — same convention the
          // !acidPlaying branch below already used for a fresh start.
          seq.cur = seq.len - 1;
          if (!acidPlaying) {
            acidPlaying = true;
            if (!seq.running) {
              seq.running = true;
              seq.lastUs = micros();
              syncPulse = 0;
            }
          }
        }
      } else {
        // Enter build mode fresh — discards any chain currently playing
        chainMode = true; chainActive = false;
        chainLen = 0; chainPos = 0; chainTickCount = 0;
      }
      ui.dirty=true; ui.fullDirty=true;
    }
  }
  // Abort if pad 11 or pad 14 comes down before the hold completes — this
  // is what actually resolves the WALK/CHAIN collision: it means the 4-pad
  // gesture is still forming, so let it keep forming instead of locking in
  // CHAIN out from under it. WALK's own arm check (below) will pick this
  // gesture up normally once all four pads are down, exactly as if CHAIN's
  // 12+13 double-tap had never been part of it.
  //
  // Deliberately NOT also aborting if pad 12/13 release early: unlike
  // layerArmed/mixEditArmed/walksArmed, this was never a HOLD gesture —
  // it's a quick double-TAP, and taps routinely release well within
  // CHAIN_HOLD_MS, often before the second pad even lands. Requiring both
  // to stay down for the full window broke almost every real double-tap
  // (this was tried and caused a full regression — chain became nearly
  // impossible to trigger — so it's called out here to stop it coming
  // back).
  if (chainArmed && (digitalRead(PAD_PINS[WALK_PAD_A])==LOW || digitalRead(PAD_PINS[WALK_PAD_D])==LOW)) {
    chainArmed=false;
  }

  // EASTER EGG: fire Acid Walks toggle when pads 11+12+13+14 held >= 1s
  if (walksArmed && (millis()-walksArmMs) >= WALK_HOLD_MS) {
    walksArmed = false;
    walksMode  = !walksMode;
    // Safety net: if CHAIN's 12+13 double-tap had already committed
    // (chainMode true) in the brief window before CHAIN_HOLD_MS's abort
    // check could catch it — or from any other stale state — force it
    // closed here too. doDraw() checks chainMode BEFORE it ever looks at
    // ui.fullDirty for drawMain(), so leaving chainMode true would mean
    // walksMode toggles on successfully but the screen keeps showing the
    // chain-build UI regardless — the exact "only get the chaining
    // feature coming up" symptom this whole fix is for.
    chainMode = false; chainArmed = false;
    ui.dirty=true; ui.fullDirty=true; ui.cellsDirty=true; ui.cellIdx=0; ui.infoDirty=true; ui.barDirty=true;
  }
  // Disarm if any of the four pads released before the hold completes
  if (walksArmed && (digitalRead(PAD_PINS[WALK_PAD_A])!=LOW || digitalRead(PAD_PINS[WALK_PAD_B])!=LOW ||
                     digitalRead(PAD_PINS[WALK_PAD_C])!=LOW || digitalRead(PAD_PINS[WALK_PAD_D])!=LOW)) {
    walksArmed=false;
  }

  // MIX EDIT: fire toggle when pads 15+16 held >= MIX_HOLD_MS.
  // On exit (mixEditMode was true), save the tuned levels to EEPROM
  // so they're "locked in" until the next edit session.
  if (mixEditArmed && (millis()-mixEditArmMs) >= MIX_HOLD_MS) {
    mixEditArmed = false;
    if (mixEditMode) saveMixSettings();  // leaving edit mode — persist
    mixEditMode = !mixEditMode;
    if (mixEditMode) {
      // Entering: snapshot the pots so none of them takes control until
      // it is actually moved. Keeps the saved mix intact on entry.
      mixPotSnap[0] = analogRead(POT_CUT);
      mixPotSnap[1] = analogRead(POT_RES);
      mixPotSnap[2] = analogRead(POT_DECAY);
      mixPotLive[0] = mixPotLive[1] = mixPotLive[2] = false;
    }
    ui.dirty=true; ui.fullDirty=true; ui.barDirty=true; ui.infoDirty=true;
  }
  // Disarm if either pad released before the hold completes
  if (mixEditArmed && (digitalRead(PAD_PINS[MIX_PAD_A])!=LOW || digitalRead(PAD_PINS[MIX_PAD_B])!=LOW)) {
    mixEditArmed=false;
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
        int envDiv  = gAccentActive ? ACCENT_DECAY_FIXED : 3;
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

  // MIX EDIT owns the pots in EVERY mode — hoisted ABOVE the bmMode return
  // below. It used to sit further down, after that return, so on the DRUM
  // screen you could open MIX EDIT and see its banner but none of the three
  // pots did anything: the function had already bailed. Mixing is exactly
  // the task you want available while listening to whichever engine is
  // playing, so it must not depend on which screen you happen to be on.
  // bmUpdateControl() suspends its own pad/pot handling while this is
  // active (see its guard) so the drum params don't move at the same time.
  if (mixEditMode) {
    const int mRaw[3] = { analogRead(POT_CUT), analogRead(POT_RES), analogRead(POT_DECAY) };
    uint8_t* const mLvl[3] = { &mixAcidLevel, &mixDriftLevel, &mixDrumLevel };
    bool changed = false;
    for (uint8_t L = 0; L < 3; L++) {
      // Soft takeover: a pot is inert until moved past the threshold from
      // where it sat on entry, then it stays live for the rest of the session.
      if (!mixPotLive[L]) {
        if (mixPotSnap[L] < 0 || abs(mRaw[L] - mixPotSnap[L]) > MIX_PICKUP_DELTA) mixPotLive[L] = true;
        else continue;
      }
      uint8_t v = (uint8_t)(mRaw[L] >> 2);        // 0-1023 -> 0-255, CW increases on all three
      if (*mLvl[L] != v) { *mLvl[L] = v; changed = true; }
    }
    if (changed) { recomputeMixGains(); ui.barDirty = true; }
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

  // ── PER-ENGINE POT OWNERSHIP ────────────────────────────────────────
  // Three physical pots are shared by acid, DRIFT and MIX EDIT, and each
  // engine used to read them LIVE every pass. So the moment you switched
  // engines, the new one adopted whatever positions the pots were left in
  // by the old one — set up a DRIFT sound, jump to acid, and acid's
  // cutoff/res/decay snapped to the DRIFT knob positions, and vice versa.
  // Nothing was remembered because nothing was ever stored: the pot WAS
  // the value.
  //
  // Each engine now keeps its own held values, and on entering an engine
  // all three pots go inert until physically MOVED past a threshold — at
  // which point that one pot (only) takes over and stays live for as long
  // as you remain in that engine. Leave and the value freezes exactly
  // where you left it. Same soft-takeover MIX EDIT and the drum machine's
  // fill editor already use; this just makes it the rule everywhere.
  //
  // Substituting into rawCut/rawRes/rawDecay here rather than at each use
  // site means every downstream consumer — acid's filter maths, DRIFT's
  // ch2ApplyPotRaw lanes, the bar displays, the change-detect below —
  // sees the held value with no further changes needed.
  {
    const uint8_t ctx = mixEditMode ? 2 : (ch2EditMode ? 1 : 0);   // 0 acid, 1 DRIFT, 2 mix
    static uint8_t potCtx = 255;
    static int     potSnap[3] = {-1,-1,-1};
    static bool    potLive[3] = {false,false,false};
    // Held values per context. Seeded from the physical pots the first
    // time each engine is entered, so a fresh boot behaves as before
    // rather than starting from an arbitrary midpoint.
    static int     ctxHeld[3][3];
    static bool    ctxSeeded[3] = {false,false,false};
    const int liveRaw[3] = { rawCut, rawResOrig, rawDecay };

    if (ctx != potCtx) {
      potCtx = ctx;
      for (uint8_t L = 0; L < 3; L++) { potSnap[L] = liveRaw[L]; potLive[L] = false; }
      if (!ctxSeeded[ctx]) {
        for (uint8_t L = 0; L < 3; L++) ctxHeld[ctx][L] = liveRaw[L];
        ctxSeeded[ctx] = true;
      }
    }
    for (uint8_t L = 0; L < 3; L++) {
      if (!potLive[L] && abs(liveRaw[L] - potSnap[L]) > POT_PICKUP_DELTA) potLive[L] = true;
      if (potLive[L]) ctxHeld[ctx][L] = liveRaw[L];
    }
    rawCut     = ctxHeld[ctx][0];
    rawResOrig = ctxHeld[ctx][1];
    rawRes     = 1023 - rawResOrig;   // re-derive the reversed copy from the held value
    rawDecay   = ctxHeld[ctx][2];
  }

  static int lastRawCut=0, lastRawRes=0, lastRawDecay=0;
  if (abs(rawCut-lastRawCut)>4 || abs(rawRes-lastRawRes)>4 || abs(rawDecay-lastRawDecay)>4) {
    lastRawCut=rawCut; lastRawRes=rawRes; lastRawDecay=rawDecay;
    ui.barDirty = true;
  }

  gDecaySpeed = rawDecay + 3;

  // Display bars: in ACID they track the raw physical pots. In DRIFT they
  // must track the VALUE driving the sound (ch2*Display, set by
  // ch2ApplyPotRaw for live pots AND recorded-automation playback) — so a
  // recorded lane's bar animates with playback and a turned-but-recorded
  // pot does NOT move it. Every bar-draw path reads these two globals, so
  // fixing them here fixes drawMain() and drawBars() together.
  if (ch2EditMode) {
    gCutoffDisplay    = (int16_t)(ch2OctDisplay >> 2);   // 0-1023 -> 0-255
    gResonanceDisplay = (int16_t)ch2AmtDisplay;          // 0-1023
    // Physical pots don't move during playback, so nudge the bars to
    // repaint whenever the automation changes a display value.
    static uint16_t lastO=9999,lastA=9999,lastD=9999;
    if (ch2OctDisplay!=lastO || ch2AmtDisplay!=lastA || ch2DcyDisplay!=lastD) {
      lastO=ch2OctDisplay; lastA=ch2AmtDisplay; lastD=ch2DcyDisplay; ui.barDirty=true;
    }
  } else {
    gCutoffDisplay   = rawCut >> 2;          // 0-1023 → 0-255
    gResonanceDisplay= rawResOrig;           // 0-1023, un-reversed physical pot position
  }

  if (mixEditMode) {
    // Levels are handled in the hoisted block above (so they also work on
    // the drum screen). All that's left here is freezing channel 1's tone:
    // gCutoff/gResonance/gDecaySpeed hold their last values so the live
    // bassline keeps its sound while the same pots are borrowed for mixing.
    gDecaySpeed = lastDecaySpeed;
  } else if (ch2EditMode) {
    // CH2 EDIT: CUT/RES/DCY pots live-control the pulse layer's octave,
    // detune, and per-engine "amount" while its screen/pads are active
    // (both the STEP and SOUND pages — engine choice is pad-driven, these
    // three stay live throughout so you can dial in tone while auditioning
    // engines on the SOUND page). Normal gDecaySpeed is frozen at its last
    // value, same reasoning as Accent Edit — editing channel 2 shouldn't
    // change channel 1's live tone.
    gDecaySpeed = lastDecaySpeed;

    // POT MAP (revised): CUT=octave, RES=per-engine AMOUNT (was on DCY
    // pot), DCY=live decay time. Detune removed as a control.
    // DRIFT pots routed through the pot-motion recorder. Lane 0=OCT(cut),
    // 1=AMT(res), 2=DCY. While REC (pad 16) is held, a lane arms when its
    // pot moves and captures to the current step; recorded lanes otherwise
    // play back (advanceStep) and are skipped here; un-recorded pots are
    // live. Lane 2's expf is guarded to pot movement to save control-rate
    // cost, same as before.
    int rawv[3] = { (int)rawCut, (int)rawResOrig, (int)rawDecay };
    bool armed = ch2Recording;
    if (armed && !ch2PrevRecArmed) { for (uint8_t L=0;L<3;L++) ch2RecSnap[L]=rawv[L]; ch2RecActive=0; }
    if (!armed && ch2PrevRecArmed) ch2RecActive = 0;   // stopped: end capture
    ch2PrevRecArmed = armed;
    static int prevRawDcy = -1;
    for (uint8_t L = 0; L < 3; L++) {
      bool active = ch2RecActive & (1 << L);
      // Arm a lane the first time its pot moves past threshold while
      // recording. On arming, baseline-fill every step with the current
      // value so un-swept steps don't jump — your sweep then overwrites
      // steps as the playhead passes them.
      if (armed && !active && ch2RecSnap[L] >= 0 && abs(rawv[L]-ch2RecSnap[L]) > 16) {
        ch2RecActive |= (1 << L); ch2AutoRec |= (1 << L); active = true;
        for (uint8_t i = 0; i < NUM_STEPS; i++) ch2AutoVal[L][i] = (uint8_t)(rawv[L] >> 2);
      }
      if (active) {                                     // recording this lane
        ch2AutoVal[L][seq.cur] = (uint8_t)(rawv[L] >> 2);
        if (L != 2 || rawv[2] != prevRawDcy) ch2ApplyPotRaw(L, rawv[L]);  // hear it live
      } else if (ch2AutoRec & (1 << L)) {
        // recorded lane, not recording now → advanceStep playback owns it
      } else {
        if (L != 2 || rawv[2] != prevRawDcy) ch2ApplyPotRaw(L, rawv[L]);  // normal live
      }
    }
    prevRawDcy = rawv[2];
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
  }  // end !mixEditMode

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
        if (i == LAYER_PAD_C) p11Deferred = false;  // fresh press — stale defer state dies here
        pLastPotStep[i]=noteEditPotStep;

        // PLAY chord (pads 1+2)
        if ((i==PAD_PLAY_A && pState[PAD_PLAY_B]) ||
            (i==PAD_PLAY_B && pState[PAD_PLAY_A])) {
          pChord[PAD_PLAY_A]=true; pChord[PAD_PLAY_B]=true;
        }
        // SECOND-LAYER chord (pads 9+10, optionally +11) — arm on press,
        // fire after LAYER_HOLD_MS. That short hold exists purely so pad
        // 11 has time to land after 9/10 before the engine gets decided —
        // without it, a DRIFT attempt (9+10+11) where 11 lands a beat
        // late reads as plain DRUMS (9+10) instead.
        else if ((i==BM_SW_A && pState[BM_SW_B]) ||
                 (i==BM_SW_B && pState[BM_SW_A])) {
          pChord[BM_SW_A]=true; pChord[BM_SW_B]=true;
          layerArmed=true; layerArmMs=now;
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
        // Pad 11 pressed while 9 or 10 is held: a layer chord may be
        // forming, so DEFER — no action on press. Resolution: if the
        // chord fires with 11 down, the fire handler eats it (pChord);
        // if the gesture aborts, the release chain replays the press in
        // the contexts where losing it matters (DRIFT step toggle, FUNC
        // select) and drops it elsewhere. Deliberately placed AFTER the
        // walks detector: a walks gesture starting on pad 11 (with
        // 12+13+14 already down) must still arm. Solo pad-11 presses
        // never reach this branch — step editing keeps its snap.
        else if (i == LAYER_PAD_C && (pState[BM_SW_A] || pState[BM_SW_B])) {
          p11Deferred = true;
          continue;
        }
        // PATTERN CHAINING chord (pads 12+13) — quick double-tap within
        // 200ms, same detection style as the FUNC chord, but arms rather
        // than firing immediately: see CHAIN_HOLD_MS's declaration for why
        // (it exists to let a real WALK attempt prove itself before this
        // commits). The actual toggle happens in updateControl() once
        // CHAIN_HOLD_MS elapses with pad 11 and pad 14 still not both down.
        else if ((i==CHAIN_PAD_A && pState[CHAIN_PAD_B] && (now-pDown[CHAIN_PAD_B])<200) ||
                 (i==CHAIN_PAD_B && pState[CHAIN_PAD_A] && (now-pDown[CHAIN_PAD_A])<200)) {
          pChord[CHAIN_PAD_A]=true; pChord[CHAIN_PAD_B]=true;
          chainArmed=true; chainArmMs=now;
          continue;  // skip all other pad actions
        }
        // MIX EDIT chord (pads 15+16) — arm on 2nd pad press, fire after
        // MIX_HOLD_MS to toggle the CUT/RES/DECAY pots into per-engine mix-trim mode
        else if ((i==MIX_PAD_A && pState[MIX_PAD_B]) ||
                 (i==MIX_PAD_B && pState[MIX_PAD_A])) {
          pChord[MIX_PAD_A]=true; pChord[MIX_PAD_B]=true;
          mixEditArmed=true; mixEditArmMs=now;
          continue;  // skip all other pad actions
        }
        // FUNC chord (pads 7+8)
        else if ((i==PAD_FUNC_A && pState[PAD_FUNC_B] && (now-pDown[PAD_FUNC_B])<200) ||
                 (i==PAD_FUNC_B && pState[PAD_FUNC_A] && (now-pDown[PAD_FUNC_A])<200)) {
          pChord[PAD_FUNC_A]=true; pChord[PAD_FUNC_B]=true;
          if (fxAssignMode) {
            fxAssignMode=false; fxAssignHasFx=false; funcSel=FUNC_NONE;
            ui.dirty=true; ui.fullDirty=true;
          } else if (chainMode) {
            // Abort an in-progress chain build — discard whatever was tapped
            // in so far rather than leaving a half-built chain lying around.
            // Also clears chainArmed: if the user had just re-tapped 12+13
            // (to exit/restart the build) and then hit this FUNC-abort
            // chord before that hold elapsed, leaving chainArmed ticking
            // would silently re-enter chain mode CHAIN_HOLD_MS later, right
            // out from under the abort they just did.
            chainMode=false; chainArmed=false; chainLen=0; chainPos=0; chainTickCount=0;
            ui.dirty=true; ui.fullDirty=true;
          } else if (funcMode) {
            funcMode=false; funcSel=FUNC_NONE;
            gSound = seq.sound;
            // barDirty: leaving FUNC must wipe PATT's gesture legend at x=224.
            ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true; ui.infoDirty=true; ui.barDirty=true;
          } else {
            funcMode=true; funcSel=FUNC_NONE;
            ui.dirty=true; ui.funcDirty=true; ui.cellIdx=0; ui.valDirty=true; ui.barDirty=true;
          }
        }
        // FX assign sub-mode
        else if (fxAssignMode) {
          if (i != PAD_FUNC_A && i != PAD_FUNC_B) doFXAssign(i);
        }
        // CHANNEL 2 EDIT sub-mode — exclusive takeover of all pads while
        // editing the pulse layer's pattern, same idiom as FX-assign and
        // chain-build above. BM_SW_A/B (9/10) are excluded — that's the
        // entry/exit gesture, handled by its own chord check, not here.
        // Yields to funcMode (same idiom as walksMode below) so the CH2
        // FUNC page (ch2FuncSel via doFuncSelect/ch2FuncApply) gets first
        // claim on every pad; pads only toggle ch2Steps[] once FUNC mode
        // is off again. ALSO yields to chainMode: without this, opening
        // chain-build from the DRIFT screen was unusable — this branch
        // sits earlier than the chainMode branch below, so pads 3-6 were
        // always toggling DRIFT steps instead of ever reaching the
        // chain's own append logic. The chain-build screen is a full
        // takeover (see the chainMode branch's own comment), so DRIFT
        // step-editing has no business running underneath it either way.
        else if (ch2EditMode && !funcMode && !chainMode) {
          // BM_SW_A/B (pads 9/10) are the CH2 EDIT entry/exit chord, so
          // they can't toggle on PRESS — the chord only reveals itself on
          // the second pad. But excluding them entirely (the old behavior)
          // meant STEPS 9 AND 10 COULD NEVER CARRY A CH2 TRIGGER. They now
          // toggle on RELEASE instead (see the release-chain ch2 branch):
          // if a chord formed, pChord[] eats the release before it gets
          // there; a genuine single tap toggles. Same defer-to-release
          // idiom as the PLAY/FUNC chord pads in ch1's FUNC mode.
          // PAD_FUNC_A/B (pads 7/8) are the FUNC chord and need the same
          // treatment: toggling on press meant a solo landing of pad 7
          // (before pad 8 caught up) turned its step on immediately, and
          // only got undone if the partner arrived within the chord's
          // 200ms window — miss that window and the stray toggle stuck.
          // Deferring to release matches how ch1/acid's own step pads
          // work (toggle on doPadRelease, never on press), so holding a
          // FUNC pad alone never lights a step here either.
          // MIX_PAD_A/B (pads 15/16) are the same story for the MIX EDIT
          // chord: the first of the two to land has no partner down yet,
          // so it fell through to this same toggle-on-press branch and
          // stuck ON until mixEditArmed fired and swallowed the *release*
          // — the press-time toggle itself was never undone.
          if (i != BM_SW_A && i != BM_SW_B && i != PAD_FUNC_A && i != PAD_FUNC_B &&
              i != MIX_PAD_A && i != MIX_PAD_B) {
            ch2Steps[i] = !ch2Steps[i];
            if (!ch2Steps[i]) ch2StepNote[i] = CH2_STEP_NONE;  // clear recorded pitch on OFF
            if (ch2EvolSel) ch2EvoResetCadence();   // hand-editing the grid resets EVOLVE
            // Single-cell redraw — only this step's ring changed. The old
            // ui.fullDirty here forced a full ~183ms screen rebuild per
            // pad press, which made toggling feel laggy and paid a whole
            // banded wipe for a one-cell change.
            ui.dirty=true; ui.editDirty=true; ui.editStep=i;
          }
        }
        // PATTERN CHAINING build sub-mode — exclusive takeover of all pads
        // while building, same idiom as FX-assign above. Pads 3-6 (slots
        // 1-4) append to the chain; every other pad is ignored so nothing
        // falls through to normal step-editing while a chain is mid-build.
        // CHAIN_PAD_A/B (12/13) are excluded — that gesture is handled by
        // its own chord check above, not here.
        else if (chainMode) {
          if (i != CHAIN_PAD_A && i != CHAIN_PAD_B) {
            if (i >= 2 && i <= 5 && chainLen < 16) {
              chain[chainLen++] = i - 2;
              ui.dirty=true; ui.fullDirty=true;
            }
          }
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
            } else if (ch2EditMode && i == 15) {
              // Pad 16 (REC) in DRIFT — deferred to release, which selects
              // the REC page / toggles recording / clears (see release
              // path). Nothing to do on press.
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
              if (ch2EditMode) {
                // In DRIFT edit — this hold resets CHANNEL 2 only (pattern,
                // sound, and fx), same idea as the acid/drum resets below
                // but scoped so it doesn't touch channel 1 or drums.
                ch2DoReset();
              } else {
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
              }
            } else if (ch2SynthMode) {
              // Short press, DRIFT is the CURRENT EDIT FOCUS — toggle
              // DRIFT's OWN play/stop, not acid's. Acid (GP15) and DRIFT
              // (GP2) can play SIMULTANEOUSLY, blended by the PCB's
              // physical mix pot — ch2SynthMode only decides which one
              // pads 1+2 (and the rest of the pad/pot surface) currently
              // controls, not which one is audible. To layer both: start
              // one, swap focus (9+10+11), press 1+2 again to start the
              // other — it joins the mix (pot permitting) without touching
              // the first one's play state.
              if (!driftPlaying) {
                driftPlaying = true;
                if (!seq.running) {
                  seq.running = true;
                  seq.cur = seq.len - 1;
                  seq.lastUs = micros();
                  syncPulse = 0;
                  if (chainActive) chainTickCount = 0;  // resync chain to the restarted loop
                }
              } else {
                driftPlaying = false;
                // Stopping DRIFT only — ACID's own play state (acidPlaying)
                // is untouched, and if it's playing it stays audible; the
                // shared clock keeps ticking if acid or drums still want it.
                if (!acidPlaying && !bmPlaying) {
                  seq.running = false;
                }
              }
              ui.dirty=true; ui.barDirty=true; ui.infoDirty=true;
            } else {
              // Short press — toggle acid PLAY/STOP. Only reached when
              // ch2SynthMode is false (acid is the current edit focus) —
              // see the branch above for DRIFT's equivalent. Acid and
              // DRIFT can be simultaneously playing and audible; this only
              // ever touches acid's own flag.
              // Fully independent of drums: starting or stopping acid never
              // starts or stops drums (mirrored logic in BeatMachine2.ino),
              // and leaves DRIFT's own play state untouched too.
              if (!acidPlaying) {
                acidPlaying = true;
                // Ensure the shared clock is ticking.
                if (!seq.running) {
                  seq.running = true;
                  seq.cur = seq.len - 1;
                  seq.lastUs = micros();
                  syncPulse = 0;
                  if (chainActive) chainTickCount = 0;  // resync chain to the restarted loop
                }
              } else {
                acidPlaying = false;
                // Stopping acid only — drums and DRIFT's own play state
                // are untouched and keep driving the shared clock (and, for
                // DRIFT, keep sounding) if either still wants it.
                if (!bmPlaying && !driftPlaying) {
                  seq.running = false;
                }
                // DRIFT's FOLLOW pitch modes (F+8/F-8/F+5/F+3/FOLW, see
                // triggerCh2Pulse) mirror gLastCh1Note — the last note ch1
                // actually fired. That's set exclusively in triggerNote(),
                // which stops getting called the instant acid is stopped
                // (advanceStep() gates the whole note-firing block on
                // acidPlaying). Left alone, gLastCh1Note stays pinned at
                // whatever ch1's last note was, so every DRIFT trigger from
                // here on reuses that one frozen pitch — monotone, no note
                // movement, exactly the reported symptom, and only when
                // ch1 had been playing and got stopped (DRIFT alone, never
                // having had a source note, correctly falls back to its
                // own per-step computed pitch — see the >=0 check in
                // triggerCh2Pulse). Resetting it to -1 here restores that
                // same per-step fallback the moment acid goes silent.
                gLastCh1Note = -1;
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
        else if (ch2EditMode && !funcMode) {
          // Release during CH2 edit: pads 1-6 and 11-14 toggled on press
          // already — just clear tracking state for those. Pads 7/8
          // (PAD_FUNC_A/B), 9/10 (BM_SW_A/B), and 15/16 (MIX_PAD_A/B) are
          // the exception: they're all chord pads, deferred from press,
          // so a release that reaches THIS branch (i.e. pChord[] didn't
          // consume it — no chord formed) is a genuine single tap and
          // toggles the step here. This is what makes steps 7, 8, 9, 10,
          // 15, and 16 programmable at all despite also doubling as
          // chord pads.
          if (i == BM_SW_A || i == BM_SW_B || i == PAD_FUNC_A || i == PAD_FUNC_B ||
              i == MIX_PAD_A || i == MIX_PAD_B || (i == LAYER_PAD_C && p11Deferred)) {
            // Chord pads: normal defer-to-release toggle. Pad 11: only if
            // its press was DEFERRED (chord was forming) and no chord
            // fired — replay the toggle it would have done on press.
            p11Deferred = false;
            ch2Steps[i] = !ch2Steps[i];
            if (!ch2Steps[i]) ch2StepNote[i] = CH2_STEP_NONE;  // clear recorded pitch on OFF
            if (ch2EvolSel) ch2EvoResetCadence();   // hand-editing the grid resets EVOLVE
            ui.dirty=true; ui.editDirty=true; ui.editStep=i;
          }
          pCycle[i]=0; pNoteEdit[i]=false; pLastPotStep[i]=255;
        }
        else if (chainMode) {
          // Release during chain-build: the append itself already happened
          // on press (see the CHAIN_PAD-adjacent press-chain branch above).
          // Just clear press-tracking state so a later exit from chainMode
          // doesn't leave stale long-press/note-edit state behind for pads
          // 3-6 (or any other pad) that were tapped while building.
          pCycle[i]=0; pNoteEdit[i]=false; pLastPotStep[i]=255;
        }
        else if (funcMode) {
          if (i == LAYER_PAD_C && p11Deferred) {
            // Deferred pad-11 press, chord aborted: replay the FUNC-row
            // select it would have done on press (bottom-row index 10).
            p11Deferred = false;
            doFuncSelect(i);
          }
          else if (ch2EditMode && i == 15) {
            // Pad 16 (REC) release, deferred from press. TOGGLE model:
            //   not on REC page  → short tap selects it
            //   on REC page      → short tap = start/stop recording
            //                      long press = clear all lanes
            bool longPress = (millis() - pDown[i]) >= LG;
            if (ch2FuncSel != 7) {
              if (!longPress) doFuncSelect(i);          // select REC page
            } else if (longPress) {
              bool had = ch2AutoRec != 0;               // clear
              ch2AutoRec = 0; ch2RecActive = 0; ch2Recording = false;
              gCh2RecClrFlashMs = had ? millis() : 0;
              ui.dirty = true; ui.valDirty = true;
            } else {
              ch2Recording = !ch2Recording;             // start/stop
              if (!ch2Recording) ch2RecActive = 0;
              ui.dirty = true; ui.valDirty = true;
            }
          }
          else if (i == PAD_FUNC_A || i == PAD_FUNC_B ||
              i == PAD_PLAY_A || i == PAD_PLAY_B) {
            if (funcSel == FUNC_PLEN) {
              seq.len = i + 1;
              if (seq.cur >= seq.len) seq.cur = 0;
              ui.dirty=true; ui.valDirty=true; ui.infoDirty=true;
            } else if (funcSel != FUNC_NONE || (ch2EditMode && ch2FuncSel <= 7)) {
              // The ch2 clause is required: funcSel deliberately stays
              // FUNC_NONE for the whole ch2 FUNC session (so ch1's PLEN/
              // TEMPO special cases can't misfire), but that also silently
              // gated THIS release-path apply — pads 1/2/7/8 are the PLAY/
              // FUNC chord pads, deferred from press to release, so they
              // were dead for ch2 values while pads 3-6 worked from the
              // press path. doFuncApply() routes to ch2FuncApply() itself.
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
          saveAllToSlot(slot);
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
        if (comboSlotHasData(slot)) {
          loadAllFromSlot(slot);
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
    // p11Deferred check: a pad-11 press held as part of a forming layer
    // chord must NOT fire its long-press (accent/glide toggle) at the LG
    // threshold — the defer branch only suppressed the PRESS action, not
    // this. Without it, holding 9+10+11 toggled accent on step 11 at
    // ~500ms, before the 1s chord fired. pChord alone doesn't cover it:
    // the defer sets p11Deferred, not pChord[11].
    //
    // p11Deferred ALONE isn't enough either, because it's only set when
    // pad 11 goes down while 9 or 10 is ALREADY held. Land on 11 first —
    // by even a few milliseconds, which is a coin toss with three fingers
    // arriving together — and it stays false, so accent fired anyway. That
    // was the intermittent "sometimes it accents before it switches".
    // Testing the LIVE pad state as well makes the suppression independent
    // of the order your fingers touch down, which is what the chord's own
    // fire-time decision already is. Holding 11 genuinely alone still
    // accents: this only suppresses while 9 or 10 is actually down.
    bool layerChordForming = (i == LAYER_PAD_C) &&
                             (p11Deferred || pState[BM_SW_A] || pState[BM_SW_B]);
    if (!pChord[i] && !layerChordForming &&
        !funcMode && !fxAssignMode && !chainMode && !ch2EditMode && !clearAllChordHeld &&
        pState[i] && !pLong[i] && !pNoteEdit[i] && (now-pDown[i])>LG &&
        noteEditPotStep == pLastPotStep[i]) {
      pLong[i] = true;
      doPadLong(i);
    }

    // Note-edit via CUT pot (normal mode only)
    if (!pChord[i] && !layerChordForming &&
        !funcMode && !fxAssignMode && !chainMode && !ch2EditMode &&
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

  // CH2 live gestures — must run after the pad scan so pState[] is current
  // this pass, and unconditionally (not inside the pad loop) so releasing
  // the last held key still drops the transpose.
  ch2PollLiveGestures();

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
  bool sr = false;
  syncOk = syncMode;  // sync active only when boot-selected
  if (syncOk && !syncOutMode && gSyncPulseFlag) {
    gSyncPulseFlag = false;  // consume — ISR may set it again immediately after
    sr = true;
  }

  // Sync clock tracking (SYNC IN only — gSyncPulseFlag is never set in
  // SYNC OUT mode since the ISR is never attached there, but the explicit
  // !syncOutMode guard documents that this whole block is direction-scoped):
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
  if (syncOk && !syncOutMode && sr) {
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

  // SYNC OUT: drive GP2's pulse train from this device's own tempo. See
  // syncOutUpdate() for the reasoning; a no-op whenever syncOutMode is false.
  syncOutUpdate();

}

// =====================================================================
// MOZZI updateAudio() — 16384Hz
// =====================================================================
// Placed in RAM (not flash) — on RP2040, core 1's heavy SPI/XIP traffic during
// drawMain() can otherwise contend with core 0's flash (XIP) fetches for this
// audio-critical function, causing core 0 to occasionally miss its sample
// deadline -> an audible click/pop in the GP15 output during long redraws.
// This affects the combined mix (both GP2 drums+DRIFT and GP15 acid share
// one output jack via the PCB's mix pot), so it can sound like a "drum
// jump" even though the drum trigger timing itself (measured via Serial)
// is perfect.

// Runs the acid synth engine for one sample and returns rawOut (uint8_t,
// 0-255, 128-centered) — exactly what the original single-voice acid path
// computed before handing it to MonoOutput::from8Bit(). Factored out on
// general principle (updateAudio() reads more clearly as "gate, then
// render"); there's currently only the one call site since DRIFT no
// longer shares this output.
static inline uint8_t computeAcidSample() {
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

  return rawOut;
}

AudioOutput __not_in_flash_func(updateAudio)() {
  // GP15 is ACID ONLY. DRIFT lives on GP2 now (bmFillDrumBufferTo() in
  // BeatMachine2.ino, summed with drums there) — see the DRIFT OUTPUT
  // comment near ch2SynthMode's declaration for why: there's a physical
  // mix pot on the PCB that blends GP15 and GP2 at the shared audio node
  // in hardware, and that's the actual intended acid/DRIFT balance
  // control — continuously variable, not a fixed software ratio. Digitally
  // summing DRIFT into this function (an earlier attempt) put both engines
  // on the same output and left the pot with nothing left to blend.
  if (!seq.running) return MonoOutput::from8Bit(128);
  if (!acidPlaying) return MonoOutput::from8Bit(128);
  uint8_t s = computeAcidSample();
  if (mixAcidGainQ8 != 256) {   // 256 = unity, skip the work on the common case
    int32_t dev = (int32_t)s - 128;                             // deviation from silence
    int32_t d   = (dev * (int32_t)mixAcidGainQ8) >> 8;          // MIX EDIT trim/boost
    // Soft-clip, mirroring bmSoftClip's shape in this 8-bit domain: above
    // 3/4 of full scale the slope drops to 1/4 instead of hard-limiting.
    // This is what lets acid be BOOSTED at all — the reason it was
    // previously attenuate-only was that gain above 1.0x had nowhere to go
    // but hard clipping, which buzzes. A soft knee gives it the same clean
    // headroom DRIFT and drums get, so all three pots can behave the same.
    if      (d >  96) { d =  96 + ((d -  96) >> 2); if (d >  127) d =  127; }
    else if (d < -96) { d = -96 + ((d + 96) >> 2);  if (d < -128) d = -128; }
    s = (uint8_t)constrain(128 + d, (int32_t)0, (int32_t)255);
  }
  return MonoOutput::from8Bit(s);
}

// =====================================================================
// SETUP
// =====================================================================
void setup() {
  Serial.begin(115200);  // kept for field debugging — USB CDC init is free
                         // and a serial port is invaluable when a build
                         // misbehaves; no runtime prints remain in normal
                         // operation
  for (uint8_t i = 0; i < 16; i++) {
    pinMode(PAD_PINS[i], INPUT_PULLUP);
    pLast[i] = LOW;
  }

  SPI.setTX(TFT_MOSI);
  SPI.setSCK(TFT_SCK);
  SPI.begin();
  tft.begin(24000000);
  tft.setRotation(3);
#ifdef PANEL_NEEDS_Y_MIRROR
  tft.sendCommand(ILI9341_MADCTL, (uint8_t[]){0x80}, 1);
#endif
  tft.fillScreen(C_BG);
  // TFT init moved up here (was originally after the boot mode detect
  // block below) so the SYNC MODE menu, when pad 14 is held, has a
  // screen to draw to. Nothing between here and where it used to sit
  // depended on TFT being uninitialized, so this is a pure reorder.

  // ── BOOT MODE DETECT ────────────────────────────────────────────────
  // Hold pad 14 (GP9) at power-on to bring up a SYNC MODE menu: press
  // pad 1 for SYNC IN, pad 2 for SYNC OUT. Direction is a firmware-only
  // choice, same jack, same physical SPDT switch position — hold pad 14
  // to match the switch being in SYNC position (either direction), leave
  // it alone for normal drum mode with the switch in DRUMS position.
  // In sync mode (either direction): GP2 is taken away from drum/DRIFT
  // audio for the whole session — a GPIO can't be a PWM audio output and
  // a clock line at the same time. In drum mode: GP2 is claimed by
  // PWMAudio DMA for drum (and DRIFT) audio output.
  delay(50);  // allow pullup to settle
  bool bootP14Held = (digitalRead(PAD_PINS[13]) == LOW);

  if (bootP14Held) {
    // SYNC MODE menu — block here until pad 1 (IN) or pad 2 (OUT) is
    // pressed. Runs before Mozzi's audio is started and before the main
    // loop, so a plain polling wait is safe; nothing else needs the CPU
    // yet, and this is the same "block in setup() for a boot-time pad
    // choice" idiom the old (now-removed) DRIFT routing menu used.
    tft.fillScreen(C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_CYN);
    tft.setCursor(52, 70);
    tft.print("SYNC MODE");
    tft.setTextSize(1);
    tft.setTextColor(C_WHT);
    tft.setCursor(40, 110);
    tft.print("PAD 1 = SYNC IN");
    tft.setCursor(40, 130);
    tft.print("PAD 2 = SYNC OUT");

    while (true) {
      if (digitalRead(PAD_PINS[PAD_PLAY_A]) == LOW) { syncMode = true; syncOutMode = false; break; }
      if (digitalRead(PAD_PINS[PAD_PLAY_B]) == LOW) { syncMode = true; syncOutMode = true;  break; }
      delay(10);
    }
    // Debounce: wait for whichever pad was pressed to release, so the same
    // press doesn't also register as a step-1/step-2 toggle once the main
    // pad scan starts up.
    while (digitalRead(PAD_PINS[PAD_PLAY_A]) == LOW || digitalRead(PAD_PINS[PAD_PLAY_B]) == LOW) delay(10);

    // Confirm the choice before moving on
    tft.fillScreen(C_BG);
    tft.setTextSize(2);
    tft.setTextColor(C_CYN);
    tft.setCursor(40, 100);
    tft.print(syncOutMode ? "SYNC OUT ACTIVE" : "SYNC IN ACTIVE");
    tft.setTextSize(1);
    tft.setTextColor(C_DGR);
    tft.setCursor(60, 130);
    tft.print(syncOutMode ? "clock output on GP2" : "external clock on GP2");
    delay(1200);
    tft.fillScreen(C_BG);
  } else {
    syncMode    = false;
    syncOutMode = false;
  }

  // NOTE: the pad-11-at-boot selection of channel 2 is GONE. DRIFT is
  // enabled at runtime by the layer chord (hold 9+10+11 for 1s; see the
  // layer-chord fire handler). ch2SynthMode always boots false, and DRIFT
  // lives on GP2 (see the DRIFT OUTPUT comment near ch2SynthMode's
  // declaration) — same output drums use, additively summed there.
  // GP2/PWMAudio is started EAGERLY here (non-sync) rather than lazily on
  // first entry to drum mode: DRIFT can now be reached first, via the
  // layer chord, without ever touching drums, and needs GP2 live from the
  // first sample just as much as drums do. doModeSwitch()'s own
  // `if (!syncMode && !bmAlarmStarted) bmStartAudio();` stays as a
  // no-op safety net for whichever path the code takes.
  if (!syncMode) bmStartAudio();

  if (syncMode && !syncOutMode) {
    // SYNC IN: configure GP2 as digital input for external sync clock.
    // INPUT_PULLDOWN assumes an active-high pulse source (idle low, pulses
    // high) — the common case for DIN-sync/trigger clock outputs. If your
    // sync source is open-drain or active-low instead, this needs to be
    // INPUT_PULLUP with the interrupt mode changed to FALLING below.
    pinMode(SYNC_IN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(SYNC_IN), syncPulseISR, RISING);
    // Do NOT call bmStartAudio() — GP2 stays as input for the whole
    // session. This now makes BOTH drums AND DRIFT unavailable on this
    // boot, not just drums — DRIFT lives on GP2 too, and a GPIO pin can't
    // be a PWM audio output and a digital clock input at once. Acid alone
    // is what sync mode gets; that's a hardware constraint, not a software
    // choice.
  } else if (syncOutMode) {
    // SYNC OUT: GP2 becomes a hardware-PWM-driven pulse output instead —
    // free-running once programmed, near-zero CPU cost, no ISR jitter.
    // analogWriteFreq()/analogWriteRange() set the shape (frequency and
    // resolution); syncOutUpdate() (called every updateControl() tick)
    // reprograms the frequency and starts/stops the train to track tempo
    // and play state. Nothing is emitted until playback actually starts —
    // see syncOutUpdate() below. Same GP2-is-exclusively-sync constraint
    // as SYNC IN: drums and DRIFT are unavailable this session, and
    // bmStartAudio() is likewise never called.
    pinMode(SYNC_IN, OUTPUT);
    digitalWrite(SYNC_IN, LOW);
    analogWriteRange(SYNC_OUT_PWM_RANGE);
  }

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

  // ── DRIFT ROUTING MENU — REMOVED ────────────────────────────────────
  // Used to block here for a pad-1/pad-9 choice between "DRUMS + ACID/DRIFT"
  // (DRIFT and acid mutually exclusive on GP15, drums always on GP2) and
  // "ACID + DRIFT" (DRIFT stacks with acid by displacing drums onto GP2 —
  // no drums that session). Both were solving the same underlying want
  // (acid+DRIFT together) by making it an either/or choice. It isn't one:
  // acid stays on GP15 alone, DRIFT lives on GP2 additively summed with
  // drums, and GP15/GP2 blend together at a shared analog audio node via
  // the PCB's physical mix pot — every combination (acid alone, DRIFT
  // alone, both together, either or both alongside drums) is simultaneously
  // available, with the pot doing real-time balance work no boot-time
  // choice ever could. Nothing to choose at power-on.
  //
  // SYNC MODE's pad-1/pad-2 menu (see the BOOT MODE DETECT block, up near
  // the top of setup()) is a different situation from either of the above:
  // GP2 really can only be one thing per boot — drum/DRIFT audio, a sync
  // input, or a sync output — so unlike DRIFT routing, that one genuinely
  // needs a choice, and its confirmation screen already ran up there,
  // right after the choice was made, rather than down here.

  memset(combBuf, 0, sizeof(combBuf));
  for (uint8_t i = 0; i < 8; i++) { vFreq[i]=287; vCnt[i]=0; }
  for (uint8_t i = 0; i < NUM_STEPS; i++) seq.steps[i].active = true;

  seq.interval = bpm2us(seq.tempo);
  seq.lastUs   = micros();
  gSound       = seq.sound;

  EEPROM.begin(EEPROM_SIZE);
  checkSlots();
  loadMixSettings();
  loadCh2Settings();
  loadCh2EucSlots();

  drawMain();
  ui.dirty     = true;
  ui.fullDirty = false;
  ui.lastMs    = millis();

  bmInit();
  startMozzi(MOZZI_CONTROL_RATE);
  rp2040.fifo.push(1);  // signal core 1 that setup is complete

  // SAFETY NET: core 1 does ALL display drawing AND all DRIFT/drum
  // synthesis (bmFillDrumBuffer, called from inside the draw loop) — see
  // "CORE 1 — Display handler" below. If anything on core 1 ever hangs
  // (loop1() stops returning), channel-1 acid audio on core 0 keeps
  // playing completely independently via audioHook() below, while the
  // screen freezes/blanks — sound with no picture. Both loop() and
  // loop1() feed this every pass, so either core stalling for >2s
  // triggers a full reset instead of needing a power cycle. 2s is well
  // clear of every known legitimate stall here (EEPROM.commit(),
  // mode-switch fillScreen()), all of which finish in tens of ms.
  rp2040.wdt_begin(2000);
}

// =====================================================================
// LOOP — core 0
// =====================================================================
void loop() {
  rp2040.wdt_reset();
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
  rp2040.wdt_reset();
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
    // No deep prefill any more — bmFillScreenFed() keeps GP2 topped up
    // band-by-band, so the wipe can't underrun. The old bmFillDrumBufferMax()
    // here was actively harmful: it queued ~500ms of audio ahead, so every
    // drum/ch2 hit triggered during the switch played up to half a second
    // late while the backlog drained — the post-switch stumble/pause.
    bmFillScreenFed(0x0000);
    bmFillDrumBuffer();
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
    // EEPROM.commit() erases+programs a flash sector, which on RP2040
    // stalls BOTH cores (multicore lockout) for tens of ms — DMA keeps
    // draining GP2 but nothing can refill it. Same class of atomic,
    // un-interspersable stall as the mode-switch fillScreen(): pre-fill
    // to full capacity (~500ms) so the ring rides through the freeze.
    bmFillDrumBufferMax();
    EEPROM.commit();
    bmFillDrumBuffer();
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
    // Cursor tracking during playback.
    //
    // This used to be a SECOND, bespoke copy of the playhead chase, gated by
    // `msSinceStep > 40 && msSinceStep < 80 && (now1 - ui.lastMs) > 60`.
    // Three independent ways to silently drop a step:
    //   1. The 40..80ms WINDOW. If the loop didn't happen to come back inside
    //      it — busy topping up drums, or mid-SPI-burst — that step was never
    //      drawn at all. Heavier drum load = more misses = skipped pads.
    //   2. ui.lastMs is SHARED with the cellsDirty (20ms) and funcDirty (30ms)
    //      batchers. Any grid repaint or pot turn stole the timestamp and
    //      blocked the cursor for a further 60ms.
    //   3. It lacked chasePlayhead's gDisplayCurOverride pin, so if core 0
    //      advanced seq.cur between the erase and the redraw it cleared one
    //      cell and lit a different one, leaving the old cell stuck lit.
    //
    // The jitter came from the ASYMMETRY: whenever a redraw flag was pending
    // the other path (doDraw -> chasePlayhead) ran with none of these gates,
    // so cursor timing visibly changed depending on whether something else
    // happened to be dirty. Both paths now run the same chase, which is a
    // no-op unless the step actually moved, so it's cheap to call every pass.
    //
    // NOT guarded on !funcMode: drawStepCellEx() IS func-aware — its row==1
    // branch draws the FUNC tile labels and selection state, which is how
    // the funcDirty batcher paints them in the first place. Guarding here
    // just froze the playhead whenever FUNC was open.
    if (seq.running && !fxAssignMode) chasePlayhead();
  }
  delay(4);
}
