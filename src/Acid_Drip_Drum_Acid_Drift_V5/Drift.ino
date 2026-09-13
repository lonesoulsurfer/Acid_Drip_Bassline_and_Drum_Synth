// =====================================================================
// Drift.ino — the DRIFT (channel 2) synth voice.
// =====================================================================
// Moved here from BeatMachine2.ino. It was already a standalone function
// there, but it lived inside the drum machine's tab purely for historical
// reasons: DRIFT used to be synthesized inline in bmFillDrumBuffer() and
// share GP2 with the drums, then briefly moved to core 0 / GP15 to run
// alongside acid there — moved back to GP2 / core 1 since (see below), so
// this file now just holds the one shared render function, independent of
// either core's tab.
//
// DRIFT lives on GP2 / core 1 (BeatMachine2.ino's bmFillDrumBufferTo()),
// additively summed with drums there, for the whole session, always. It
// briefly rendered on GP15 / core 0 alongside acid instead, summed
// digitally in software — that made acid+DRIFT simultaneously audible, but
// there's a PHYSICAL mix pot on the PCB that blends GP15 and GP2 at a
// shared analog audio node, and that pot had nothing left to do once both
// engines were already combined in software on the same output before
// ever reaching it. Moving DRIFT back to GP2 (its original home) restores
// the pot as the real, continuously-variable acid/DRIFT balance control —
// see the DRIFT OUTPUT comment in the main sketch, near ch2SynthMode's
// declaration, for the full reasoning.
//
// CORE OWNERSHIP — the one rule that matters here.
// The echo and reverb state inside ch2RenderSample() is function-local
// statics. They are safe only because exactly ONE core ever calls this
// function in a session — true by construction again: this function is
// called from exactly one call site (bmFillDrumBufferTo(), core 1), for
// the entire session, every session. No handoff, no shared-state window,
// nothing to synchronise.
// Note the indices are all masked (& (CH2_ECHO_LEN - 1) and friends), so
// even if that ever changed again, the worst case would be a click from
// stale/mixed state, not an out-of-range write.
//
// Externs are declared explicitly below rather than leaning on Arduino's
// tab concatenation order for the VARIABLES. One ordering dependency does
// remain and can't be removed with an extern: the Ch2VerbBufs TYPE is
// defined in the main sketch. That is safe — Arduino always concatenates
// the main .ino first — but if the verb buffers ever move, the struct
// definition has to stay ahead of this tab.
// =====================================================================

extern uint8_t  ch2Sound;                     // 0-15, selected engine — see CH2_SND_NAMES
extern uint8_t  ch2Amount;                    // 0-255, live per-engine character knob (DCY pot)
extern volatile uint32_t ch2Phase;
extern volatile uint32_t ch2PhaseInc;
extern volatile uint16_t ch2Env;
extern volatile uint16_t ch2EnvM;
extern volatile uint32_t ch2DetuneRatioQ16;   // Q16 second-voice ratio — DTSQR + UNISN + SAW
extern volatile uint8_t  ch2ClickCount;       // CLICK: noise-burst samples remaining
extern volatile uint8_t  ch2NoiseIdx;         // CLICK + NOIZ: running index into noisetable[]
extern int16_t  ch2EchoBuf[];                 // ECHO delay line (CH2_ECHO_LEN samples, power of 2)
extern volatile uint16_t ch2EchoDelay;        // ECHO delay in samples, 0 = off
extern volatile uint8_t  ch2EchoFb;           // ECHO feedback, Q8
extern Ch2VerbBufs ch2VerbBufs;               // VERB comb/allpass delay lines (~4.9KB)
extern volatile uint8_t  ch2VerbFbQ8;         // VERB comb feedback (room size), Q8
extern volatile uint8_t  ch2VerbWetQ8;        // VERB wet mix, Q8; 0 = off
extern const uint8_t sinetable[256];
extern const uint8_t noisetable[64];
// ── New small persistent state for SND2's engines (8-15). Each is reset
// on trigger in triggerCh2Pulse() — see the reset block there. Same cost
// class as CLICK's ch2ClickCount/ch2NoiseIdx pair above; ARPG is the
// priciest (needs its own phase accumulator, since the shared ch2Phase
// always advances at the trigger-time rate for every engine uniformly).
extern volatile uint8_t  ch2SubDivFlip;       // SUBDIV: divide-by-2 flip-flop (1 bit, stored as a byte)
extern volatile int16_t  ch2CrushHold;        // CRUSH: currently-held sample value
extern volatile uint8_t  ch2CrushCtr;         // CRUSH: samples remaining before the next hold
extern volatile uint32_t ch2ArpgPhase;        // ARPG: its own phase accumulator (see case 12)
extern volatile uint8_t  ch2ArpgStep;         // ARPG: which of the 3 ratios is current (0-2)
extern volatile uint16_t ch2ArpgCtr;         // ARPG: samples remaining at the current step
extern volatile uint16_t ch2GongIdx;          // GONG: independent FM-index decay (own multiplier, not ch2EnvM)
extern volatile uint32_t ch2NoiseLfsr;        // NOIZ: xorshift32 noise state
extern volatile int16_t  ch2NoiseLp;          // NOIZ: one-pole lowpass state
extern volatile uint32_t ch2NoisePhase;       // NOIZ: pitched LFSR clock accumulator
extern volatile uint32_t ch2VowP1;            // VOWEL: formant-1 phase (absolute Hz)
extern volatile uint32_t ch2VowP2;            // VOWEL: formant-2 phase
extern volatile uint32_t ch2GatePhase;        // GATE: free-running chop phase

// Ring buffer sizing: capacity stays deep (bmPWM.setBuffers below) so it
// can absorb one specific atomic, un-interspersable stall — the mode-switch
// screen wipes and long redraws. Screen wipes are now BANDED via
// bmFillScreenFed() (fills threaded between 24px strips), so no draw path
// needs a deep prefill any more. Normal calls keep only a SMALL backlog
// topped up: filling to capacity (the original behavior) meant every
// trigger — ch2 or drum — sat behind ~500ms of already-queued audio,
// which was ch2's step-timing bug AND the post-mode-switch drum stumble.
// bmFillDrumBufferMax() survives for exactly ONE case: EEPROM.commit(),
// whose flash-sector erase stalls BOTH cores and genuinely cannot have
// fills threaded through it. Everything else tops up to BM_BUF_TARGET.
// UNITS FIX: PWMAudio::availableForWrite() does NOT return samples. It
// returns _arb->available() * 4 — free 32-bit ring-buffer WORDS times 4
// (see arduino-pico PWMAudio.cpp). In mono mode each write(int16_t)
// duplicates the sample into both halves of ONE 32-bit word, so:
//     1 sample written  =  1 word consumed  =  availableForWrite() -4
// An empty 64x128 ring therefore reports 8192*4 = 32768, not 8192.
// The old constants treated availableForWrite() as sample counts, so
// "top up to 512" actually kept 8192-(7680/4) = 6272 samples (~383ms)
// queued — ~3 sixteenth-steps at 120 BPM, which was the entire cause of
// ch2 pulses sounding 3 steps after the pad they were programmed on.
// DRIFT (ch2) output trim. DRIFT is back on GP2, additively summed with
// drums in bmFillDrumBufferTo() (BeatMachine2.ino) — same output path
// drums use, so the relevant reference for THIS trim is drums, not acid.
// (It briefly fed updateAudio()/GP15 alongside acid instead, digitally
// mixed in software; moved back to GP2 so the PCB's physical mix pot has
// a real, separate DRIFT signal to blend against acid on GP15 — see the
// DRIFT OUTPUT comment in the main sketch, near ch2SynthMode's
// declaration.) DRIFT renders to ~full int16 (softclip ±32600); this
// scales it before summing with the drum bus. The value below (240/256,
// tuned against acid under the brief GP15-mixing arrangement) hasn't been
// re-validated against drums specifically since moving back — worth
// ear-checking DRIFT-vs-drums balance on the GP2 bus itself, independent
// of the mix pot, next time you're on hardware. Applied AFTER the
// softclip so echo/verb ratios and saturation stay put — only the jack
// level moves. TUNING KNOB vs drums: louder 200 ≈ -2.2dB, 224, 256 = full;
// quieter 128 ≈ -6dB, 96 ≈ -8.5dB.
#define CH2_OUT_LEVEL_Q8  240

// ── CH2 (DRIFT) sample render ──────────────────────────────────────
// Extracted from bmFillDrumBufferTo() as a standalone function. Returns
// one sample (post echo/verb/softclip/DRIFT output-trim). Reads the ch2*
// globals; the echo/verb position statics inside belong to whichever
// SINGLE core calls this function — see the CORE OWNERSHIP note at the
// top of this file. Today, and for the whole session every session,
// that's core 1 (bmFillDrumBufferTo(), the only call site). If DRIFT is
// ever routed back to GP15/core 0 as it briefly was in the past, this
// comment and the one at the top of the file both need updating BEFORE
// any second call site is added — the statics are not safe to touch
// from two cores at once.
// __not_in_flash_func: this is audio-critical, and on its current GP2
// route it runs on core 1 — the same core that owns all TFT/SPI. A
// flash-resident render competes with drawing traffic for XIP, misses the
// GP2 deadline and the ring buffer underruns. updateAudio() carries this
// same attribute for exactly this reason; see the note there.
int32_t __not_in_flash_func(ch2RenderSample)(){
  int32_t mix = 0;
      // Second synth voice — 8 selectable engines (ch2Sound), all built on
      // the same single phase-accumulator + decay-envelope skeleton so
      // adding an engine never costs more than a table lookup or two.
      // Mutually exclusive with drums (both need GP2), so this branch
      // replaces the drum mix entirely rather than adding to it.
      if(ch2Env){
        int16_t s16 = 0;
        switch(ch2Sound){
          case 0: {  // SUBSIN — sub sine + ch2Amount-blended octave-up sine.
                     // Blend caps at 50/50 (>>9, not >>8) on purpose: the
                     // engine is called SUB — the octave adds shimmer on
                     // top of the foundation, it never replaces it.
            int16_t v1 = (int16_t)sinetable[ch2Phase >> 24] - 128;
            int16_t v2 = (int16_t)sinetable[(uint32_t)(ch2Phase << 1) >> 24] - 128;
            s16 = (int16_t)(v1 + (((int32_t)(v2 - v1) * ch2Amount) >> 9));
            break;
          }
          case 1: {  // DTSQR — TWO detuned squares (root + ratio'd second
                     // voice, same ch2DetuneRatioQ16 scheme as UNISN),
                     // summed and halved. The old single-oscillator
                     // version just played slightly sharp — beating only
                     // existed against ch1 on the same note. This one
                     // beats against ITSELF, so the width survives any
                     // PITCH interval. RES pot (detune) sets the spread.
            // +0x40000000 = 90-degree head-start on osc 2. Without it,
            // phase2 = ch2Phase*ratio starts at 0 too (ch2Phase resets to
            // 0 each trigger), so both squares are IDENTICAL at note onset
            // and only drift apart over the note — at musical detune on a
            // short pulse, they barely diverge before the envelope kills
            // them, which is why DTUN sounded like it did nothing. The
            // offset makes the two voices immediately distinct (instant
            // thickening) with the detune beating layered on top.
            uint32_t phase2 = (uint32_t)(((uint64_t)ch2Phase * ch2DetuneRatioQ16) >> 16) + 0x40000000UL;
            int16_t v1 = (ch2Phase < 0x80000000UL) ? 127 : -128;
            int16_t v2 = (phase2   < 0x80000000UL) ? 127 : -128;
            s16 = (int16_t)((v1 + v2) >> 1);
            break;
          }
          case 2: {  // FMBEL — 2-op FM bell: sine carrier, sine modulator at a
                     // fixed 3.5:1 ratio, index scaled by ch2Amount and by
                     // the envelope itself so it brightens on attack and
                     // mellows into the tail — classic FM-bell behaviour.
            uint32_t modPhase = (uint32_t)(((uint64_t)ch2Phase * 7ULL) >> 1);  // x3.5
            int16_t  modS     = (int16_t)sinetable[modPhase >> 24] - 128;
            uint8_t  idxShift = 8 + (ch2Amount >> 5);  // amount 0..255 -> shift 8..15
            uint32_t carrierPhase = ch2Phase + (((int32_t)modS * (ch2Env >> 8)) << idxShift);
            s16 = (int16_t)sinetable[carrierPhase >> 24] - 128;
            break;
          }
          case 3: {  // RINGM — ring mod: sine carrier x square at an
                     // inharmonic ~2.8:1 ratio for a clangy/cowbell-ish
                     // metallic tone. ch2Amount is wet/dry, plain sine (0)
                     // up to full clang (255).
            uint32_t ringPhase = (uint32_t)(((uint64_t)ch2Phase * 179ULL) >> 6);  // x~2.797
            int16_t  carrier   = (int16_t)sinetable[ch2Phase >> 24] - 128;
            int16_t  ring      = (ringPhase < 0x80000000UL) ? carrier : (int16_t)(-carrier);
            s16 = (int16_t)(carrier + (((int32_t)(ring - carrier) * ch2Amount) >> 8));
            break;
          }
          case 4: {  // CLICK — sub sine plus a short noise-burst attack
                     // transient (ch2ClickCount samples, length set at
                     // trigger from ch2Amount).
            s16 = (int16_t)sinetable[ch2Phase >> 24] - 128;
            if(ch2ClickCount){
              int16_t n = (int16_t)noisetable[ch2NoiseIdx & 63] - 128;
              s16 = (int16_t)constrain((int32_t)s16 + (n >> 1), -128, 127);
              ch2NoiseIdx++;
              ch2ClickCount--;
            }
            break;
          }
          case 5: {  // FOLDR — wavefolded sine: drive it past full-scale,
                     // then fold back in, adding odd harmonics/growl.
                     // ch2Amount sets the drive amount (1x..8x).
            int32_t base  = (int16_t)sinetable[ch2Phase >> 24] - 128;
            int32_t drive = 1 + (ch2Amount >> 5);  // 1..8
            int32_t d     = base * drive;
            while(d >  127) d =  254 - d;
            // Reflection around -128 is -256 - d. The previous -258 - d
            // had a FIXED POINT at d = -129 (maps to itself, still below
            // the exit threshold) — an infinite loop that hard-locked
            // core 1 (audio + display dead, no watchdog) whenever
            // base*drive hit exactly -129, e.g. sine value -43 at
            // drive 3 (ch2Amount 64-95). Reachable every cycle at that
            // pot position.
            while(d < -128) d = -256 - d;
            s16 = (int16_t)d;
            break;
          }
          case 6: {  // PWM — envelope-swept duty. ch2Amount sets the base
                     // width (~12%..~62%); the envelope adds up to ~25%
                     // on top, so every pulse opens wide at the attack
                     // and narrows as it decays — the movement that makes
                     // PWM PWM, per hit, no LFO state needed. Sum can
                     // reach ~87% duty at max pot + full attack; duty
                     // above 50% mirrors below, so extremes just thin
                     // out symmetrically. No uint32 overflow: max thresh
                     // 0x9F800000 + 0x3FFF0000 < 2^32.
            uint32_t thresh = 0x20000000UL + ((uint32_t)ch2Amount << 23)
                            + ((uint32_t)(ch2Env >> 2) << 16);
            s16 = (ch2Phase < thresh) ? 127 : -128;
            break;
          }
          case 7: {  // UNISN — two sines, root + ch2DetuneCents-spaced,
                     // summed and halved for a thicker sub/pad layer.
            // +0x40000000 = 90-degree head-start on osc 2 — see DTSQR
            // (case 1) for the full why. Same shared-phase-0 defeat, same
            // fix: immediate two-voice thickening instead of waiting for
            // a beat that a short pulse never completes.
            uint32_t phase2 = (uint32_t)(((uint64_t)ch2Phase * ch2DetuneRatioQ16) >> 16) + 0x40000000UL;
            int16_t  v1 = (int16_t)sinetable[ch2Phase >> 24] - 128;
            int16_t  v2 = (int16_t)sinetable[phase2 >> 24] - 128;
            s16 = (int16_t)((v1 + v2) >> 1);
            break;
          }
          // ── SND2 (engines 8-15) — see CH2_SND_NAMES for the full index
          // reference. Same skeleton as 0-7 above: phase accumulator +
          // shared decay envelope, ch2Amount as the one live character
          // knob. New persistent state, where an engine needed it, is
          // declared at the top of this file and reset in
          // triggerCh2Pulse() — see that reset block for the full list.
          case 8: {  // SAW — two ramps summed. ch2Amount now sets the DETUNE
                     // SPREAD directly (0 to ~50 cents) at a fixed 50/50
                     // blend, which is what the WDTH label always promised.
                     //
                     // It previously crossfaded between two saws whose
                     // spread came from ch2DetuneRatioQ16 — derived from
                     // ch2DetuneCents, which the RES pot only writes while
                     // engines 1 or 7 (DTSQR/UNISN) are selected. On SAW the
                     // RES pot drives ch2Amount instead, so the detune was
                     // frozen at whatever was last dialled on a DIFFERENT
                     // engine (18 cents from boot). Invisible hidden state
                     // deciding how the engine sounds.
                     //
                     // The phase offset scales with amount too: at 0 both
                     // ramps are phase-identical so you get a genuinely pure
                     // single saw, and at max they sit a quarter cycle apart
                     // for instant thickness. DTSQR needs its fixed 90-degree
                     // offset because a short note ends before slow beating
                     // develops; here the offset arrives WITH the detune, so
                     // both effects grow together off one knob.
            uint32_t detR   = 65536UL + (((uint32_t)ch2Amount * 1893UL) >> 8);
            uint32_t offset = (uint32_t)ch2Amount << 22;      // 0 .. ~1/4 cycle
            uint32_t phase2 = (uint32_t)(((uint64_t)ch2Phase * detR) >> 16) + offset;
            int16_t v1 = (int16_t)(ch2Phase >> 24) - 128;
            int16_t v2 = (int16_t)(phase2   >> 24) - 128;
            s16 = (int16_t)((v1 + v2) >> 1);
            break;
          }
          case 9: {  // SUBDIV — TRUE sub-octave square, one octave below
                     // the root: a divide-by-2 flip-flop (ch2SubDivFlip)
                     // toggled each time ch2Phase wraps. NOT the same as
                     // reading a lower phase bit — a bit off a free-running
                     // counter is always a MULTIPLE of the fundamental,
                     // never a sub-multiple, so a true divide-by-2 needs
                     // this one bit of persistent state.
                     //
                     // The sub is the FOUNDATION and is always present;
                     // ch2Amount adds the root square on top (octaver-pedal
                     // stack), it does NOT crossfade to it. The previous
                     // version blended a sine against the sub, which meant
                     // amount=0 was a bare sine — measurably IDENTICAL to
                     // SUBSIN at its own zero (spectral correlation 1.00),
                     // and still 0.98 at half travel. Two engines sharing
                     // most of a knob's travel is why this didn't feel
                     // distinct. Now amount=0 is a pure sub square, which
                     // nothing else here produces.
                     //
                     // Levels: sub 92 + root max (72*255)>>9 = 35 → peak
                     // 127, so the stack fills the sample range without
                     // clipping into bmSoftClip on its own.
            if (ch2Phase < ch2PhaseInc) ch2SubDivFlip ^= 1;
            int16_t sub  = ch2SubDivFlip ? 92 : -92;
            int16_t root = (ch2Phase < 0x80000000UL) ? 72 : -72;
            s16 = (int16_t)(sub + (((int32_t)root * ch2Amount) >> 9));
            break;
          }
          case 10: {  // NOIZ — enveloped noise, no tonal component.
                      //
                      // The old version stepped through noisetable[64] and
                      // masked with &63, which is a 64-SAMPLE LOOP: measured
                      // autocorrelation at lag 64 was 0.98, i.e. it was a
                      // periodic ~256Hz buzz, not noise at all. It also
                      // never read ch2Phase, so every note played back
                      // identically — spectral centroid was 3814Hz whether
                      // the note was 55Hz or 880Hz. Both are fixed here.
                      //
                      // xorshift32 gives a 2^32-1 period (never repeats
                      // audibly) for three shifts and three XORs — cheaper
                      // than a table read plus the index arithmetic it
                      // replaces. The one-pole lowpass is the character
                      // knob: ch2Amount opens it from dark rumble to open
                      // hiss. k saturates at 8 because >>3 of a full-scale
                      // delta is unity gain; past that it would overshoot.
                      // PITCH comes from the LFSR CLOCK RATE, not from the
                      // filter. Holding each random value for a note-derived
                      // number of samples is how the SID and NES noise
                      // channels work, and it's the only way noise reads as
                      // pitched: white noise filtered a bit brighter still
                      // sounds like the same white noise.
                      //
                      // The first attempt nudged the filter cutoff by
                      // ch2PhaseInc>>25 instead. That was far too weak —
                      // over a 55-220Hz octave it moved one step of an
                      // eight-step filter, and since k is capped at 8 the
                      // cap swallowed it completely at high ch2Amount.
                      // Measured centroid across 55-880Hz: x1.00 spread at
                      // amount 255, i.e. the OCT pot did literally nothing.
                      // Clocking gives x2.1-x3.5 instead.
                      //
                      // 16x the note rate keeps the clock inside the audio
                      // band across the useful range (55Hz note -> 880Hz
                      // clock, 440Hz -> 7kHz). Above a 1024Hz note the <<4
                      // would overflow 32 bits and wrap to a SMALL value,
                      // making high notes suddenly dark — the guard clocks
                      // every sample there instead, which is the correct
                      // limit anyway (full-rate white noise).
                      //
                      // ch2Amount stays on the colour filter, so the two
                      // controls now own one axis each: OCT sets the noise
                      // pitch, RES sets its tone.
            bool clk;
            if (ch2PhaseInc >= (1UL << 28)) {
              clk = true;                                  // already at/above max clock
            } else {
              uint32_t prev = ch2NoisePhase;
              ch2NoisePhase += (ch2PhaseInc << 4);         // 16x the note frequency
              clk = (ch2NoisePhase < prev);                // wrapped -> new random value
            }
            if (clk) {
              uint32_t r = ch2NoiseLfsr;
              r ^= r << 13;  r ^= r >> 17;  r ^= r << 5;
              ch2NoiseLfsr = r;
            }
            int16_t raw = (int16_t)((ch2NoiseLfsr >> 8) & 0xFF) - 128;
            uint8_t k = (uint8_t)(1 + (ch2Amount >> 5));   // 1..8, 8 = unity (no overshoot)
            ch2NoiseLp = (int16_t)(ch2NoiseLp +
                          ((((int32_t)raw - ch2NoiseLp) * k) >> 3));
            s16 = ch2NoiseLp;
            break;
          }
          case 11: {  // VOWEL — true formant synthesis. ch2Amount crossfades
                      // "ah" (F1 700Hz, F2 1150Hz) to "ee" (F1 300, F2 2300).
                      //
                      // The formants are FIXED IN HERTZ, which is the whole
                      // point and what the previous version got wrong: it
                      // placed the partials at fixed RATIOS of the note, so
                      // they tracked pitch exactly — measured 231/363 Hz at
                      // a 110Hz note but 925/1452 Hz at 440Hz. Formants that
                      // move with the note aren't formants; the vowel
                      // identity never changed and it read as an organ.
                      // Now the vowel stays put as you play up the keyboard,
                      // which is exactly what makes speech sound like speech.
                      //
                      // Fixed-frequency oscillators would normally beat
                      // inharmonically against the note. They don't here
                      // because both formant phases RESET every fundamental
                      // period — so the output is strictly periodic at the
                      // note pitch (all energy on its harmonics) with the
                      // spectral peak parked at the formant frequency. That
                      // is classic FOF/VOSIM synthesis. The per-period ramp
                      // is the resonance decay; without it each reset would
                      // be a hard discontinuity.
                      //
                      // Cheaper than before: two 32-bit adds replace two
                      // 64-bit multiplies. 262144 = 2^32 / AUDIO_RATE, so
                      // "Hz * 262144" is the phase increment for that Hz.
            if (ch2Phase < ch2PhaseInc) { ch2VowP1 = 0; ch2VowP2 = 0; }
            uint16_t f1 = (uint16_t)(700  - (((uint32_t)400  * ch2Amount) >> 8));
            uint16_t f2 = (uint16_t)(1150 + (((uint32_t)1150 * ch2Amount) >> 8));
            ch2VowP1 += (uint32_t)f1 * 262144UL;
            ch2VowP2 += (uint32_t)f2 * 262144UL;
            uint8_t ramp = (uint8_t)(255 - (ch2Phase >> 24));   // resonance decay per period
            int16_t v1   = (int16_t)sinetable[ch2VowP1 >> 24] - 128;
            int16_t v2   = (int16_t)sinetable[ch2VowP2 >> 24] - 128;
            int16_t root = (int16_t)sinetable[ch2Phase >> 24] - 128;
            s16 = (int16_t)((root >> 3)
                          + (((int32_t)v1 * ramp) >> 9)
                          + (((int32_t)v2 * ramp) >> 10));
            break;
          }
          case 12: {  // ARPG — internal chiptune arpeggio: cycles the
                      // EFFECTIVE pitch through {root, +~7 semi, +12 semi}
                      // (Q8 ratios 256/384/512) every N samples, ch2Amount
                      // sets N (arpeggio speed). Uses its OWN phase
                      // accumulator (ch2ArpgPhase) rather than the shared
                      // ch2Phase — the shared one always advances at the
                      // trigger-time rate for every engine uniformly (see
                      // the ch2Phase += ch2PhaseInc line below this
                      // switch), so an engine that wants a DIFFERENT
                      // effective rate mid-note has to keep its own. One
                      // voice reads as a chord/riff instead of a static
                      // pitch per hit — nothing else here has internal
                      // rhythmic/melodic motion. Priciest of the 8 new
                      // engines: ~9 bytes of new state (ch2ArpgPhase +
                      // ch2ArpgStep + ch2ArpgCtr) vs 0-3 for the others.
            static const uint16_t ARPG_RATIO_Q8[3] = {256, 384, 512};
            // Step length 128..2040 samples = 7.8..125 ms. The old
            // "1 + (ch2Amount >> 3)" gave 1..32 samples — 0.06 to 1.95 ms
            // per step, so a full 3-note cycle repeated at 170-5461 Hz.
            // That is AUDIO RATE: the ear heard a buzzy timbre, not an
            // arpeggio, at every knob position. A chiptune arp steps at
            // roughly 1/60 s (16.7 ms), which now sits near the fast end.
            // Higher amount = faster, so the SPD label still reads right.
            // The counter is uint16_t because 2040 does not fit in a byte.
            if (ch2ArpgCtr == 0) {
              ch2ArpgStep = (uint8_t)((ch2ArpgStep + 1) % 3);
              ch2ArpgCtr  = (uint16_t)(128 + ((((uint32_t)(255 - ch2Amount)) * 1920UL) >> 8));
            } else {
              ch2ArpgCtr--;
            }
            uint32_t effInc = (uint32_t)(((uint64_t)ch2PhaseInc * ARPG_RATIO_Q8[ch2ArpgStep]) >> 8);
            ch2ArpgPhase += effInc;
            s16 = (int16_t)sinetable[ch2ArpgPhase >> 24] - 128;
            break;
          }
          case 13: {  // GONG — FM at an inharmonic ~5.4:1 ratio (vs
                      // FMBEL's consonant 3.5:1) for a metal-plate/gong
                      // character. Index decays on its OWN slower
                      // multiplier (ch2GongIdx) instead of riding the
                      // shared ch2Env, so the metallic partial audibly
                      // outlasts the amplitude decay — genuine "shimmer
                      // after the hit," which FMBEL doesn't have (its
                      // index decays together with amplitude, same
                      // envelope for both). The slow multiplier is fixed,
                      // not user-tunable, to avoid piling another knob
                      // onto a single engine.
            // GONG — FM at an inharmonic ratio for a metal-plate character.
            // Two changes from the first version, both measured:
            //
            // 1. The index decayed on a FIXED multiplier (65450), which
            //    reaches zero in ~320ms no matter what DCY is set to. So
            //    the "shimmer outlasts the hit" claim only held for short
            //    decays; on anything longer the note collapsed to a bare
            //    sine — 4 partials above 5% of peak, late in the note.
            //    The multiplier is now derived FROM ch2EnvM, so the index
            //    always decays a quarter as fast as the amplitude and the
            //    claim holds at every decay setting. Measured: 9 partials
            //    still present late in the note.
            // 2. A floor stops the index reaching zero at all, so a long
            //    tail keeps a metallic edge instead of turning into a sine
            //    — the specific thing that made this feel like SUBSIN.
            //
            // ch2Amount now sets the RATIO (3.6:1 clangy bell -> 7.1:1
            // sheet metal) rather than the index, because the index is
            // envelope-driven above and no longer needs a knob. That also
            // gives GONG a character axis FMBEL doesn't have — FMBEL is a
            // fixed consonant 3.5:1 with a brightness knob.
            uint16_t ratioQ5  = (uint16_t)(115 + (((uint32_t)112 * ch2Amount) >> 8));
            uint32_t modPhase = (uint32_t)(((uint64_t)ch2Phase * ratioQ5) >> 5);
            int16_t  modS     = (int16_t)sinetable[modPhase >> 24] - 128;
            uint32_t carrierPhase = ch2Phase + (((int32_t)modS * (ch2GongIdx >> 8)) << 13);
            s16 = (int16_t)sinetable[carrierPhase >> 24] - 128;
            uint16_t gm = (uint16_t)(65536 - ((65536 - ch2EnvM) >> 2));  // 4x slower than amplitude
            ch2GongIdx = (uint16_t)(((uint32_t)ch2GongIdx * gm) >> 16);
            if (ch2GongIdx < 0x8000) ch2GongIdx = 0x8000;                // never a pure sine
            break;
          }
          case 14: {  // CRUSH — lo-fi on BOTH axes at once: sample-rate
                      // decimation (hold) and bit-depth reduction (mask).
                      // ch2Amount drives them together, 8192Hz/7-bit up to
                      // ~500Hz/4-bit.
                      //
                      // The old version held "1 + (ch2Amount >> 5)" samples,
                      // so at amount 0 it held exactly 1 — no decimation at
                      // all, i.e. a clean sine, measured at correlation 1.00
                      // with SUBSIN. Same dead-bottom-of-the-knob problem
                      // SUBDIV had. The minimum is now 2, so every position
                      // is audibly crushed and the knob controls how much,
                      // not whether. Adding bit-depth alongside rate is what
                      // separates this from a plain downsample: the two
                      // artefacts sound different and stacking them is the
                      // recognisable "crushed" character.
            if (ch2CrushCtr == 0) {
              int16_t v  = (int16_t)sinetable[ch2Phase >> 24] - 128;
              uint8_t sh = (uint8_t)(1 + (ch2Amount >> 6));   // discard 1-4 low bits
              ch2CrushHold = (int16_t)((v >> sh) << sh);
              ch2CrushCtr  = (uint8_t)(2 + (ch2Amount >> 3)); // hold 2-33 samples
            } else {
              ch2CrushCtr--;
            }
            s16 = ch2CrushHold;
            break;
          }
          case 15: {  // GATE — rhythmic amplitude chop at 4-40 Hz, on its
                      // OWN free-running phase so the chop rate is fixed in
                      // Hz and does not follow the note.
                      //
                      // The old version gated on "ch2Phase * gateMult" with
                      // gateMult an INTEGER 3-10. Its comment claimed the
                      // gate was "not harmonic with the fundamental on
                      // purpose" — but an integer multiple of the phase is
                      // by definition strictly harmonic, so the comment
                      // stated the opposite of what the code did. Worse, at
                      // a 110Hz note that put the gate at 330-1100 Hz: audio
                      // rate, so it waveshaped the tone into a hard-sync-ish
                      // timbre instead of chopping it, and the chop "rate"
                      // changed every time you played a different note.
                      // A stutter has to live at 4-40 Hz and stay put, which
                      // needs its own accumulator — the shared ch2Phase
                      // always runs at the note frequency.
                      //
                      // 262144 = 2^32 / AUDIO_RATE, so "Hz * 262144" is the
                      // phase increment for that many Hz.
            uint16_t rateHz = (uint16_t)(4 + (((uint32_t)36 * ch2Amount) >> 8));
            ch2GatePhase += (uint32_t)rateHz * 262144UL;
            int16_t base = (int16_t)sinetable[ch2Phase >> 24] - 128;
            s16 = (ch2GatePhase & 0x80000000UL) ? base : 0;
            break;
          }
          default: s16 = (int16_t)sinetable[ch2Phase >> 24] - 128; break;
        }
        int8_t s = (int8_t)constrain((int32_t)s16, -128, 127);
        mix = (int32_t)(ch2Env >> 8) * s;
        ch2Env = (uint16_t)(((uint32_t)ch2Env * ch2EnvM) >> 16);
        ch2Phase += ch2PhaseInc;
      }
      // ── CH2 ECHO ─ tempo-synced delay, 50% wet ────────────────────
      // OUTSIDE the ch2Env gate on purpose: the tail must keep ringing
      // after the envelope dies (mix is simply 0 then, so the delay line
      // plays out pure repeats). Sits BEFORE bmSoftClip so repeats
      // saturate together with the dry signal — combined with the int16
      // clamp on the feedback write below, high-feedback presets self-
      // limit instead of overflowing. Length is a power of two → wrap is
      // a mask, and the whole block is one read + one MAC + one write
      // per sample. ch2EchoDelay/ch2EchoFb are core-0-published volatiles
      // (16/8-bit aligned loads — atomic on ARM); pos belongs to the calling core.
      {
        uint16_t d = ch2EchoDelay;             // one volatile snapshot per sample
        if (d) {
          static uint16_t echoPos = 0;
          uint16_t rd  = (uint16_t)(echoPos - d) & (CH2_ECHO_LEN - 1);
          int32_t  wet = ch2EchoBuf[rd];
          int32_t  wr  = mix + ((wet * (int32_t)ch2EchoFb) >> 8);
          ch2EchoBuf[echoPos] = (int16_t)constrain(wr, (int32_t)-32768, (int32_t)32767);
          echoPos = (echoPos + 1) & (CH2_ECHO_LEN - 1);
          mix += wet >> 1;                     // 50% wet
        }
      }
      // ── CH2 VERB ─ lo-fi Schroeder: 4 damped combs -> 2 allpasses ──
      // After the echo (repeats get reverberated), before bmSoftClip
      // (tail saturates with everything else). Outside the ch2Env gate
      // for the same reason as echo: the tail must ring after the pulse
      // dies. Input is attenuated >>2 and every delay-line write is
      // int16-saturated, so even HALL/WASH feedback settles into soft
      // grit instead of wrapping. Allpasses use the proper lattice form
      // (v = x + g*d; y = d - g*v, g = 1/2) — the naive "y = d - x/2,
      // store x + d/2" variant is NOT unit-magnitude and colors the tail.
      // Positions/damping states are statics owned by the calling core; buffers are
      // core-0-cleared only on the explicit OFF->ON gesture (benign race,
      // same as echo). Cost: ~4 combs + 2 APs ≈ 30 integer ops/sample.
      {
        uint8_t vWet = ch2VerbWetQ8;           // one volatile snapshot
        if (vWet) {
          static uint16_t cp0=0, cp1=0, cp2=0, cp3=0, vap0=0, vap1=0;
          static int16_t  lp0=0, lp1=0, lp2=0, lp3=0;
          uint8_t vFb = ch2VerbFbQ8;
          int32_t inr = mix >> 2;
          int32_t sum = 0;
          #define CH2_COMB(N, LEN) { \
            int16_t y = ch2VerbBufs.c##N[cp##N]; \
            lp##N = (int16_t)(((int32_t)lp##N * CH2_VERB_DAMP \
                             + (int32_t)y * (256 - CH2_VERB_DAMP)) >> 8); \
            int32_t w = inr + (((int32_t)lp##N * vFb) >> 8); \
            ch2VerbBufs.c##N[cp##N] = (int16_t)constrain(w, (int32_t)-32768, (int32_t)32767); \
            if (++cp##N >= LEN) cp##N = 0; \
            sum += y; }
          CH2_COMB(0, CH2_VERB_C0)  CH2_COMB(1, CH2_VERB_C1)
          CH2_COMB(2, CH2_VERB_C2)  CH2_COMB(3, CH2_VERB_C3)
          #undef CH2_COMB
          // >>1, NOT >>2: the four combs' first reflections arrive at
          // DIFFERENT times (30-41ms apart), so the instantaneous sum is
          // ~one comb's worth, not four — halving is real headroom, but
          // quartering was 6dB of wet level thrown away for stacking that
          // never happens. Combined with the >>2 input attenuation, the
          // old scaling put the wet path ~30dB under the dry: technically
          // present, perceptually absent. Rare coherent peaks now clip
          // into the int16 allpass writes / softclip — grit, not wrap.
          int32_t x = sum >> 1;
          { int16_t d = ch2VerbBufs.a0[vap0];
            int32_t v = x + (d >> 1);
            int32_t y = (int32_t)d - (v >> 1);
            ch2VerbBufs.a0[vap0] = (int16_t)constrain(v, (int32_t)-32768, (int32_t)32767);
            if (++vap0 >= CH2_VERB_A0) vap0 = 0;
            x = y; }
          { int16_t d = ch2VerbBufs.a1[vap1];
            int32_t v = x + (d >> 1);
            int32_t y = (int32_t)d - (v >> 1);
            ch2VerbBufs.a1[vap1] = (int16_t)constrain(v, (int32_t)-32768, (int32_t)32767);
            if (++vap1 >= CH2_VERB_A1) vap1 = 0;
            x = y; }
          mix += ((int32_t)x * vWet) >> 8;
        }
      }
      mix = bmSoftClip(mix);
      // DRIFT output trim — see CH2_OUT_LEVEL_Q8. Must stay AFTER the
      // softclip: trimming before it would also change how hard the
      // clipper is driven, altering the sound instead of just the level.
      mix = (mix * (int32_t)CH2_OUT_LEVEL_Q8) >> 8;
  return mix;
}
