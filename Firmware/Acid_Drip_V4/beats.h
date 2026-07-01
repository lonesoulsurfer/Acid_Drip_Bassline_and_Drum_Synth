// Beat patterns for Beat Machine 2 — tuned for acid/techno/house
// Drum order: kick, hat, snare, rim, tom, bass2, clap, openhat
// Each row = 4 bytes = 32 steps (2 bars of 16th notes)
// Bit 7 of byte 0 = step 1, bit 6 = step 2 ... bit 0 of byte 3 = step 32

#define NUM_BEATS 16

const byte beats[NUM_BEATS][8][4] PROGMEM = {

  { // 1. BASIC — simple 4/4, clean foundation for any acid line
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: 1 3 5 7 (every beat)
    {0b00100010,0b00100010,0b00100010,0b00100010,},  // hat: offbeats (2 4 6 8)
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 3 7 (2 and 4)
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 2. HOUSE — four-on-floor kick, open hat offbeat, snare 2+4
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: every beat
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: every 8th
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00100010,0b00100010,0b00100010,0b00100010,},  // openhat: offbeats
  },

  { // 3. TECHNO — driving kick, closed hat 16ths, snare 2+4
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: every beat
    {0b11111111,0b11111111,0b11111111,0b11111111,},  // hat: every 16th
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 4. ACIDTRK — syncopated kick for acid tracks, hat 8ths, snare 2+4
    {0b10000010,0b00100000,0b10000010,0b00100000,},  // kick: 1 7 11 (funk pocket)
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: every 8th
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000010,0b00000000,0b00000010,0b00000000,},  // rim: ghost on 8+24
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00100000,0b00100000,0b00100000,0b00100000,},  // openhat: offbeat
  },

  { // 5. HSOC — hardcore/acid-house hybrid groove, four-on-the-floor kick
    // with a busier breakbeat-flavored snare cluster and driving 16th hats
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: every beat (acid house foundation)
    {0b11111111,0b11111111,0b11111111,0b11111111,},  // hat: driving 16ths (hardcore energy)
    {0b00001010,0b00010000,0b00001010,0b00010001,},  // snare: syncopated breakbeat-style cluster
    {0b00000000,0b00000001,0b00000000,0b00000010,},  // rim: ghost pickups before the bar turns over
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00100010,0b00000000,0b00100010,0b00000000,},  // openhat: offbeat lift in bar 1
  },

  { // 6. FUNKY — deep funk pocket, syncopated kick + snare
    {0b10000010,0b00100000,0b10000010,0b00100000,},  // kick: syncopated
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // hat: quarter offbeats
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000001,0b00000001,0b00000001,0b00000001,},  // rim: ghost 16th before beat
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b10000000,0b00000000,0b10000000,},  // clap: on 3
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 7. SWING — swung hat pattern, great with acid bassline
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: every beat
    {0b10100010,0b10100010,0b10100010,0b10100010,},  // hat: swung feel
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00100000,0b00100000,0b00100000,0b00100000,},  // openhat: swung
  },

  { // 8. BREAKBT — classic Amen-style breakbeat
    {0b10100000,0b00110000,0b10100000,0b00110000,},  // kick: syncopated
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: every 8th
    {0b00001001,0b01001001,0b00001001,0b01001001,},  // snare: breakbeat pattern
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 9. MINML — minimal techno, sparse kick, hi-hat carries groove
    {0b10000000,0b00001000,0b10000000,0b00001000,},  // kick: 1 5 (sparse)
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: every 8th
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000010,0b00000010,0b00000010,0b00000010,},  // rim: 8th+1 ghost
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 10. LATIN — latin-influenced, rim carries clave feel
    {0b10000010,0b00100000,0b10000010,0b00100000,},  // kick: syncopated
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // hat: quarter notes
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b10001010,0b00101000,0b10001010,0b00101000,},  // rim: clave pattern
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 11. PACIFIC — 808 State "Pacific State"-style: 909 four-on-the-floor
    // kick with 8th-note hats and offbeat opens, but the centerpiece is an
    // "unusual" clap pattern that lands on odd/asymmetric positions
    // (5, 12, 21, 29, 31) rather than the expected straight 2&4 — the
    // production approach was to keep things sophisticated by landing
    // hits on odd-numbered steps and varying the pattern between bars
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: 909 four-on-the-floor
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: 8ths
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // snare: unused (clap is the feature)
    {0b00000001,0b00000001,0b00000001,0b00000100,},  // rim: ghost fills for the swing feel
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00001000,0b00010000,0b00001000,0b00001010,},  // clap: "unusual" asymmetric pattern
    {0b00100010,0b00100010,0b00100010,0b00100010,},  // openhat: offbeat lift
  },

  { // 12. BLUMND — Blue Monday inspired, works with synth bass
    {0b10001000,0b11111111,0b10001000,0b10001000,},  // kick: bar 2 fill
    {0b10111011,0b00111011,0b00111011,0b00111011,},  // hat: syncopated
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 13. DNBTEC — drum and bass inspired, fast feel with gaps
    {0b10100000,0b00100000,0b10100000,0b00100000,},  // kick: dnb kick placement
    {0b11111111,0b11111111,0b11111111,0b11111111,},  // hat: 16ths
    {0b00001000,0b10001000,0b00001000,0b10001000,},  // snare: 2+4 + extra
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 14. VOODOO — A Guy Called Gerald "Voodoo Ray"-style: classic acid
    // house 808 groove — four-on-the-floor kick, shuffling 8th-note hats
    // with offbeat opens, backbeat clap, and toms/rim trading off in a
    // syncopated, melodic-feeling interplay (the toms-and-claves-as-
    // melody approach that defines the original's percussion arrangement)
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: four-on-the-floor 808
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: shuffling 8ths
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: layered with clap for a thicker backbeat
    {0b00000100,0b00000001,0b00000100,0b00000001,},  // rim/clave: syncopated counter-rhythm
    {0b01000000,0b01000100,0b00000001,0b00000100,},  // tom: melodic, varying placement
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // clap: on 2+4 (backbeat)
    {0b00100010,0b00100010,0b00100010,0b00100010,},  // openhat: classic acid-house lift
  },

  { // 15. GBLDWN — build-down groove, tom drives, sparse kick
    {0b10000000,0b10000000,0b10000000,0b10000000,},  // kick: beat 1 only
    {0b10101010,0b10101010,0b10101010,0b10101010,},  // hat: every 8th
    {0b00001000,0b00001000,0b00001000,0b00001000,},  // snare: 2+4
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b10001010,0b00101000,0b10001010,0b00101000,},  // tom: driving rhythm
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // clap: unused
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // openhat: unused
  },

  { // 16. JACK — classic jackin' house groove: four-on-the-floor kick, the
    // signature "jack" clap pattern (hits 2,5,9,13 in bar1, 3,5,9,11 in
    // bar2) driving the groove forward, kick-reinforcing snare on beats
    // 1-3 of each bar, sparse ghost rim 16ths, and offbeat open hats
    {0b10001000,0b10001000,0b10001000,0b10001000,},  // kick: four-on-the-floor
    {0b11111111,0b11111111,0b11111111,0b11111111,},  // hat: driving 16ths
    {0b10001000,0b10000000,0b10001000,0b10000000,},  // snare: reinforces kick on beats 1-3
    {0b00000100,0b00000100,0b00000100,0b00000100,},  // rim: sparse ghost 16ths
    {0b00000000,0b00000000,0b00000000,0b00000000,},
    {0b00000000,0b00000000,0b00000000,0b00000000,},  // bass2: unused
    {0b01001000,0b10001000,0b00101000,0b10100000,},  // clap: "the jack" — 2,5,9,13/3,5,9,11
    {0b00100010,0b00100010,0b00100010,0b00100010,},  // openhat: offbeat lift
  },

};
