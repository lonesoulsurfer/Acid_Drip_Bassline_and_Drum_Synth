# Acid Drip - Bassline Synth & Drum Machine

Youtube Video (click the image) - [![Acid Drip Demo](https://img.youtube.com/vi/anotb2mvv04/maxresdefault.jpg)](https://youtu.be/anotb2mvv04) 


An RP2040-based acid bassline synthesizer and drum machine built on the Mozzi audio library. Two instruments in one device, a 16-step acid sequencer and a 16-pattern drum groove box running simultaneously on dual cores with a 320x240 ILI9341 TFT display and 16 Cherry MX pads.

NOTE: the images of the synth are V1. V2 is what I have provided in this repo and looks a little different (extra pot for mix added)

NOTE: V5 firmware adds pattern chaining -- link the 4 save slots into a longer playback sequence, in any order, with repeats. See Pattern Chaining under the Acid Synthesizer section below.

![Platform](https://img.shields.io/badge/platform-RP2040-blue) ![IDE](https://img.shields.io/badge/IDE-Arduino-teal) ![Audio](https://img.shields.io/badge/audio-Mozzi-green)


---

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | RP2040 (Raspberry Pi Pico or equivalent) |
| Display | ILI9341 320x240 TFT |
| Pads | 16x Cherry MX switches (2 rows of 8) |
| Pots | 3x analog (CUT, RES, DECAY) |
| Audio out (acid) | GP15 -> 100R + 10nF RC filter -> 3.5mm jack |
| Audio out (drums) | GP2 -> 470R resistor -> 3.5mm jack |
| Sync in | GP2 (hardware SPDT switch selects drum audio OR sync input) |


### Wiring

```
TFT   : SCK=GP6  SDA=GP7  RST=GP8  DC=GP10  CS=GP13
Pots  : CUT=GP26  RES=GP28  DCY=GP27
Sync  : IN=GP2
Audio : Acid=GP15   Drums=GP2

Top row pads (steps 1-8):
  GP14=pad1  GP12=pad2  GP11=pad3  GP16=pad4
  GP17=pad5  GP19=pad6  GP20=pad7  GP21=pad8

Bottom row pads (steps 9-16 / FUNC select):
  GP0=pad9   GP1=pad10  GP3=pad11  GP4=pad12
  GP5=pad13  GP9=pad14  GP18=pad15 GP22=pad16
```

---

## Dependencies

Install via Arduino Library Manager:

- **Mozzi** (audio synthesis engine)
- **Adafruit GFX**
- **Adafruit ILI9341**

Board package: **Raspberry Pi Pico/RP2040** (Earle Philhower core)

---

## Files

| File | Description |
|------|-------------|
| `Acid_Drip_V4.ino` | Main sketch -- acid synth engine, sequencer, UI |
| `BeatMachine2.ino` | Drum machine tab -- DrumKid engine, 8 instruments |
| `beats.h` | 16 preset drum patterns (techno, house, hip-hop, etc.) |
| `sample0-7.h` | Drum samples stored as int8 arrays in flash (PROGMEM) |

---

## Acid Synthesizer

### Pad Layout

```
+---------+---------+---------+---------+---------+---------+---------+---------+
|    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |
|  step   |  step   |  step   |  step   |  step   |  step   |  step   |  step   |
|    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |
+---------+---------+---------+---------+---------+---------+---------+---------+
|    9    |   10    |   11    |   12    |   13    |   14    |   15    |   16    |
|   KEY   |  RIFF   |  SOUND  |  WALK   |   FX    |  TEMPO  |  PLEN   |  PAT>  |
+---------+---------+---------+---------+---------+---------+---------+---------+
  (bottom row labels show only in FUNC mode)
```

### Step Editing

| Action | Result |
|--------|--------|
| Short press a pad | Toggle step on/off |
| Long press x1 | Toggle accent |
| Long press x2 | Toggle glide |
| Hold pad + turn CUT pot | Set step note (scale-quantised) |

### Chords

| Chord | Action |
|-------|--------|
| Pads 1+2 | Play / Stop |
| Pads 1+2 (hold 2s) | Factory reset |
| Pads 7+8 | Enter / exit FUNC mode |
| Pads 9+10 | Switch between acid synth and drum machine |
| Pads 11+12+13+14 (hold 1s) | Toggle Acid Walks easter egg |
| Pads 12+13 | Enter / exit chain-build mode |
| Pads 15+16 (hold 1s) | Toggle Accent Edit mode |

### Save / Load (hold pads 7+8)

- **Tap** pads 3/4/5/6 -> load from slot 1/2/3/4
- **Hold** pads 3/4/5/6 for 1 second -> save to slot 1/2/3/4
- Slot status shown top-right (yellow dot = saved, grey = empty)

### Pattern Chaining

Link the 4 save slots into a longer playback sequence, up to 16 positions, in any order, with repeats.

**Building a chain:**

1. Tap pads 12+13 together to enter chain-build mode. The screen switches to a dedicated chain-builder view.
2. Tap pads 3/4/5/6 to append that slot to the chain. Tapping the same pad more than once repeats that slot -- for example tapping 3, 3, 4, 5, 6, 6 builds a chain that plays slot 1 twice, then slot 2, slot 3, and slot 4 twice.
3. Tap pads 12+13 again to exit and start playback. The chain begins automatically, even if nothing was playing yet.

Each chain position plays through one full loop of that slot's own saved pattern length before advancing to the next position -- a slot saved at 16 steps plays 16 steps, a slot saved at 8 steps plays 8, and so on. When the chain reaches the end it loops back to the first position.

Pads 7+8 (FUNC) cancel an in-progress build without starting playback. Tapping 12+13 again at any other time, including while a chain is already playing, drops the current chain and starts a fresh build.

A small "CH" indicator appears next to the slot status dots whenever a chain is actively playing.

Chains are session-only and are not saved to EEPROM -- rebuilding one takes a few seconds if needed again after a power cycle. Chaining currently works in the acid synth only, not drum mode.

### FUNC Mode (enter with pads 7+8)

| Pad | Function | Options |
|-----|----------|---------|
| 9 | KEY | Root note: C D Eb F G Ab Bb B |
| 10 | RIFF | Load preset riff pattern: DFLT / SQNCE / FUNK / MINI / JUMP / RAVE / SYNC / DARK |
| 11 | SOUND | Synth voice: SAW / SQR / SINE / PWM / CSAW / CSQR / CSIN / SUBSQ |
| 12 | WALK | Note walk: OFF / 4TH / OCTWAVE / 5TH / BOUNCE / MIN3RD / VAMP3 / RANDOM |
| 13 | FX | Enter FX assign sub-mode |
| 14 | TEMPO | Preset BPM: 100/110/120/128/133/138/145/160 -- or tap repeatedly for tap tempo |
| 15 | PLEN | Pattern length: 1 / 2 / 3 / 4 / 6 / 8 / 12 / 16 |
| 16 | PAT> | Playback order: FWD / CW / ALT / REV / SKIP2 / SKIP3 / PING / RND |

> Note: there is currently no live scale-type or octave selector -- `seq.scale`/`seq.octave` are only set when loading a saved patch slot.

### FX Assign (FUNC -> pad 13)

1. Press a top-row pad to select an effect:
   `None / Oct Up / Retrigger / Stutter / Maj Step / Min Step / Dom7 Step / Dim Step`
2. Press any pad 1-16 to assign that effect to that step.
3. Tap the selected FX pad again to deselect.
4. Long-hold pad 1 to clear all step effects.
5. Pads 7+8 to exit back to main screen.

### Accent Edit

Hold **pads 15+16** for 1 second to enter Accent Edit mode. The three pots are repurposed to tune the accent envelope itself rather than the live filter:

| Pot | Function |
|-----|----------|
| CUT | Accent peak cutoff boost (brightness added on accented steps) |
| RES | Accent peak resonance boost |
| DCY | Accent envelope decay-time multiplier |

Normal CUT/RES/DCY values freeze at their last setting while tuning, so the bassline's base tone doesn't change -- only accented steps reflect the new values, live, as you turn the pots. Hold pads 15+16 again to exit; the tuned values are saved to EEPROM automatically and persist across power cycles until changed again.

### Acid Walks (Easter Egg)

Hold **pads 11+12+13+14** for 1 second to toggle a hidden mode with 8 preset patterns inspired by classic acid/house tracks, each pre-loaded with its own key, tempo, length and sound. Select a pattern with pads 1-8. FUNC mode, FX assignment and drum mode all continue to work normally while active. Hold the same 4-pad chord again to exit.

---

## Beat Machine

Switch between acid and drum mode by pressing **pads 9+10 simultaneously**. The sequencer state is preserved when switching.

### Drum Instruments (8 tracks)

| Pad | Instrument |
|-----|-----------|
| 1 | Kick |
| 2 | Hi-hat (closed) |
| 3 | Snare |
| 4 | Rim |
| 5 | Tom |
| 6 | Bass 2 |
| 7 | Clap |
| 8 | Open hat |

### Pattern Selection

Short-tap any pad (1-16) to load that beat pattern. The current pattern name and number are shown on the display. Pads 1-5 double as drum edit pads (see below) -- a short tap selects the pattern, a long hold enters drum editing.

### Per-Drum Editing

Hold any of pads 1-5 (kick/hat/snare/rim/tom) and turn the pots:

- **CUT** -> pitch (exponential, pot centre = natural speed, each 64 units = one octave)
- **RES** -> decay length (exponential envelope from a 4ms blip to full sample tail)
- **DCY** -> volume

Pot pickup prevents value jumps when grabbing a control -- each pot only responds once it has been physically moved past its position at the moment the pad was grabbed. Per-drum settings reset on pattern change and are saved with slots.

> Bass 2, Clap, and Open Hat (pads 6-8) cannot be edited via hold+pot. Pattern selection is their only pad function.

### Beat Machine FUNC Mode (pads 7+8)

| Label | Function | Detail |
|-------|----------|--------|
| PTCH | Global pitch | Transposes all drums together; stacks on top of per-drum pitch |
| DRIV | Drive / saturation | Exponential pre-gain into a hard-knee saturator; top of the knob also adds bit crush |
| FILT | Bipolar DJ filter | Centre = bypass; below centre = lowpass sweep; above centre = highpass sweep |
| CHNC | Chance | Centre = as-is; below = thins existing hits (anchors held solid); above = adds ghost hits on empty steps |
| HMNZ | Humanize | Adds random velocity spread and per-hit micro-timing jitter; centre = robotic |
| SWNG | Swing | Delays odd 16th steps up to 60% of the step interval for a swung feel |
| ACNT | Accent depth | Centre = flat; above centre = on-beat emphasis; below centre = off-beat (pushed) emphasis |
| FILL | Procedural fill engine | See Fill Engine below |

All FUNC parameters except FILT and CHNC are saved with slots.

### Fill Engine (FUNC -> FILL)

The fill engine takes over the last few steps before a pattern boundary with a procedurally generated build, then lands on beat 1 with a kick and open hat.

**Pads 1-4** set the fill frequency:

| Pad | Label | Behaviour |
|-----|-------|-----------|
| 1 | OFF | No fills |
| 2 | 1 BAR | Fill every 16 steps (4 steps long) |
| 3 | 2 BAR | Fill every 32 steps (6 steps long) |
| 4 | 4 BAR | Fill every 64 steps (8 steps long, half a bar) |

**Pads 5-8** set the fill type:

| Pad | Label | Behaviour |
|-----|-------|-----------|
| 5 | HATS | Closed-hat build opening to an open hat on the tail; kick holds the floor |
| 6 | CLAP | Clap build over a steady half-time kick |
| 7 | SNARE | Flat snare rush with no kick; the landing slams |
| 8 | KIT | Descending tom for the first 70% of the window, then snare into the landing; KIT also adds a clap on the "1" |

> Pads 7 and 8 are also the FUNC-exit chord. SNARE and KIT selections register on release so the exit gesture takes priority.

**Editing the fill sound:** while in the FILL slot, the three pots control the lead voice's own pitch, decay length, and volume independently of the pattern's drum settings. These are saved with slots.

Density accelerates into the landing: the final quarter of the fill window schedules additional 32nd-note in-between hits so the build feels like it rushes into the "1".

### Preset Patterns

16 patterns selectable from the pad grid:

| # | Short name | Full name |
|---|-----------|-----------|
| 1 | BASC | BASIC |
| 2 | HOUS | HOUSE |
| 3 | TECH | TECHNO |
| 4 | ACID | ACID TRACK |
| 5 | HSOC | HI STATE |
| 6 | FUNK | FUNKY |
| 7 | SWNG | SWING |
| 8 | BKBT | BREAKBEAT |
| 9 | MINM | MINIMAL |
| 10 | LATN | LATIN |
| 11 | PCFC | PACIFIC ST |
| 12 | BLMN | BLUE MONDAY |
| 13 | DNBT | DRUM N BASS |
| 14 | VOOD | VOODOO RAY |
| 15 | GBLD | BUILD DOWN |
| 16 | JACK | JACK BEAT |

### Save / Load (drum mode)

Same gesture as the acid synth: hold pads 7+8, then tap pads 3-6 to load from slot 1-4 or hold pads 3-6 for one second to save. Per-drum pitch/length/volume, global FUNC parameters, tempo, swing, fill settings, and the selected pattern are all stored.

---

## Sync In / Drum Mode (GP2)

GP2 is shared between drum audio output and external sync clock input. A physical **SPDT switch on the PCB** routes the pin to either function.

**Boot detection selects the mode:**

- **Drum mode (default):** GP2 is claimed by PWMAudio DMA and outputs drum audio. Switch in the DRUMS position.
- **Sync In mode:** Hold **pad 14** at power-on. GP2 is configured as a digital input and listens for an external clock pulse. Switch in the SYNC position.

A cyan "SYNC IN ACTIVE" message is shown briefly after boot when sync mode is detected.

> The switch position and held pad must match. If the switch is in SYNC position but the device boots into drum mode, GP2 will try to drive audio into the sync signal line.

---

## Architecture Notes

The firmware runs on both RP2040 cores:

- **Core 0** -- Mozzi audio engine (acid synthesis + drum mixing), pad scanning, sequencer logic
- **Core 1** -- TFT display rendering, drum sample buffer filling (via `bmFillDrumBuffer`)

The two cores communicate via the RP2040 inter-core FIFO. All SPI/TFT calls happen exclusively on core 1.

The drum clock is driven directly by the acid sequencer's `advanceStep()` call rather than an independent timer. This guarantees zero drift between the two engines at all tempos and pattern lengths.

The drum audio path on GP2 uses a separate `PWMAudio` DMA instance at 488kHz carrier (versus Mozzi's 48kHz on GP15). This prevents carrier-frequency beating between the two audio outputs that would otherwise cause the acid synth to sound quieter when drums are playing.

---

## Credits

- Acid synthesis engine adapted from *Badass Bass*  https://www.youtube.com/watch?v=MvDILHYImc4
- Drum engine adapted from **DrumKid** by Matt Bradshaw  https://www.youtube.com/watch?v=509iZGjnVhM
- Built with the [Mozzi](https://sensorium.github.io/Mozzi/) audio library
- Hardware design and firmware by Marcus ([lonesoulsurfer](https://github.com/lonesoulsurfer))
