# Changes

What each version of Strata brought, newest first. **This book matches Strata V0.87 (steps 1 and 2), commit
`0f86b61`.** Every version from V0.7 on is verified in the emulator and needs a hardware test.

## V0.87

### Step 2: the drums' models, presets and kits

- **Any drum row, any model.** DRUM's new first page, MODEL: seven models with their types. ANALOG (a swept sine body:
  kicks, toms, congas, an 808 snare), PUNCH (the KICK, SNARE and CLAP of before), FM (DRUM, BELL), CHIP (SQUARE,
  TRIANGLE, NOISE: 8-bit sounds), NOISE (a band of noise), METAL (CLOSED, OPEN, CYMBAL) and WAVE (a one-shot on OSC A's
  or OSC B's table). SOUND's TONE and fourth knob take each model's names. A row keeps its key, GRID row and MIDI note
  whatever its model; projects from before play as they did.
- **64 factory drum presets** in eight categories (KICK, SNARE, CLAP, HAT, CYMBAL, TOM, PERC, FX): <kbd>EDIT</kbd> twice
  on DRUM opens the row's category, SELECT steps categories, <kbd>SAVE</kbd> twice copies a preset into the row.
- **Six factory kits**: 808, 909, LO-FI, VIDEO GAME, TRAP, DRUM & BASS, each built from the presets, with its own duck
  and COMP: <kbd>EDIT</kbd> twice on KIT or HOME, <kbd>SAVE</kbd> twice loads one.
- **Your library**: save a drum as a drum preset (64) or the kit as a kit (16) with the SAVE window, named with NAME
  IT; delete yours from the lists. Kept apart from the projects.
- **EUCLID**, a new page of the drum track's CLIP: HITS, STEPS, ROTATE and ROW fill a row with a Euclidean rhythm.

### Step 1: the interface

- **The SAVE window** (<kbd>SAVE</kbd> once): SAVE over your preset, SAVE AS NEW (named as its next version), CANCEL.
  SAVE held 5 s no longer saves.
- **The assign window** (<kbd>ENV</kbd> twice, <kbd>LFO</kbd> twice): an envelope, a macro or an LFO onto a knob of the
  page you were on, with AMOUNT and a new **POLARITY** (BIPOLAR, UNIPOLAR) for each matrix slot. ENV has no layer and
  no combos any more.
- **Resets** are now a menu button + <kbd>OCT−</kbd> held 5 s (a countdown from 2 s). A button held alone no longer
  resets.
- **HOME twice: the SETTINGS**; HOME held 5 s: the **DEBUG** menu (BENCH, KEY SCAN, CORE 2). SETTINGS gained **WIPE**
  (every setting back; presets and projects stay) and SCREEN > **LIGHTS** (the LEDs at OFF, LOW, MED, HIGH, steady at
  every level).
- **<kbd>EDIT</kbd> twice: the wavetables' list** by category. JUMP is gone. The MATRIX moved to EDIT, after FILTER;
  GLO is VOICE and GLOBAL.
- **The presets' list**: INIT first, then A to Z; it closes after 5 s untouched. A new unit starts on INIT, its four
  macros ready in its first four matrix slots.
- **The looper**: CLIPS shows the tracks as rows; SONG moved to LOOP; the projects are a list (<kbd>SEQ</kbd> twice);
  <kbd>SEQ</kbd> + <kbd>OCT+</kbd> makes a clip of a bar (PRESETS, SEQ held: its length), <kbd>SEQ</kbd> +
  <kbd>OCT−</kbd> deletes one (asked); changing the TYPE of a track with clips asks first; SELECT on HOME picks the
  track.
- **TAKE** (<kbd>REC</kbd> twice): what you just played becomes a clip. **Tap tempo**: <kbd>GLO</kbd> held,
  <kbd>PLAY</kbd> tapped.
- Strata names itself on USB as "Strata for FM-1" by u4ia, with its own USB id; the flasher's page in Strata's
  colours.

## V0.86

- **Step 2, the drum editor**: EDIT on a drum track opens DRUM (the drum PRESETS picks: SOUND, DRIVE, EQ, SHAPE, MIX,
  each drum's own drive, EQ and attack) and KIT (ACCENT with TONE, DUCK with SOURCE, CURVE with HOLD, COMP on the eight
  drums); factory kits.
- **Step 1, the interface**: one layout on every screen; the pages regrouped so a page's knobs belong together; HOME's
  three pages (SOUND, MIX 1-4, MIX 5-8) and a drum track's HOME; PRESETS as a list; SEQ's pages with SPEED and TOOLS
  (copy, paste, double, halve, clear); every menu button's second press; the help map (HOME held); SETTINGS > POWER
  (battery, screen off) and KEY SCAN; centre detents on bipolar knobs; the EQ's SHAPES; the FX filter's FOLLOW; the
  tape's HISS; the LEDs' rules.

## V0.85: the drums

A track's TYPE can be DRUMS: eight drums (KICK, SNARE, CLAP, HAT CL, HAT OP, CYM, WT 1, WT 2) played from the keys
(accented on the upper keys), hits with ACCENT, CHANCE and RATCHET, the GRID with the keys as steps, and the DUCK row:
the sidechain.

## V0.81

The looper's fixes before its test: USB MIDI out never leaves a note on; 1–8 % less work for the same sound; less
memory; two stale states and a write past an array fixed.

## V0.8: the looper

V0.8's sequencer (never installed) rebuilt as an 8-track looper: tracks of SYNTH, SYNTH+MIDI or MIDI, each with its
own scale, chords and ARP; eight scenes; takes, overdubs and UNDO PASS; knob moves recorded; the piano roll; the launch
pad; MIXER; SONG; four projects. The user presets went from 128 to 64 to make room. Also **CORE 2**, the second core
measured (off by default).

## V0.75: the ARP

The ARP button's menu: a step arpeggiator with thirteen modes, octaves, dotted and triplet rates, sixteen steps (PLAY,
TIE, REST, level, octave, ratchet, chance, slide), GATE, SWING and LATCH, on PLAY's beat or MIDI's clock; PATTERN and
HARP. One grammar for every menu button. Chords set by knobs only, with STRUM.

## V0.7: the chords

SCL's chords: KEY and FREE modes, voicing with AUTO voice leading, BASS under the chord.

## V0.67: the screen made legible

Nothing under 9 pixels, brighter labels, larger handles; VALUES on every module; four more themes (MOSS, DUSK, SAND,
NEON); HOME's bloom as a sunflower.

## V0.66 and the STRATA UI

The STRATA screen: one picture a module, numbered knob handles, Space Mono, warm white on black with one vermilion
accent. Also big chords without crackles, smoother effect changes, a faster screen, keys played from a timer.

## V0.65

The memory audit: much less code and more free memory, with the same sound.

## V0.6: the FX chain

The effects as a chain of up to eight modules (add, take out, move, two of a type), seven new effects (FLANGER, PHASER,
an FX FILTER of 21 types, BITCRUSH, TREMOLO, RING MOD, TAPE), SELECT for modules and ALGORITHM for pages, no ON knobs.

## V0.5

The first Strata to run well on an FM-1 (2026-10-17): the wavetable engine, the pages and layers, SAVE and the
browser, MIDI in, USB audio, the serial console and BENCH.
