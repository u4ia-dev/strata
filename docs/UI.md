# The STRATA UI (the `strata-ui` build, V0.66 UI)

An alternate V0.66 with the screen redrawn after the **Strata UI Overhaul** design (its gallery of 81 boards, approved
2026-10-17; the renders of this build beside each board: the "Strata UI Firmware Renders" page). The sound engine, the
panel's controls, the flash, the package and the update path are V0.66's; only what the screen shows changed.
[CONTROLS.md](CONTROLS.md) is still the reference for the panel; this file is the reference for the screen.

Status: verified in the emulator (90 checks, `web/emu/strata_emu_test.mjs`; every screen rendered,
`web/emu/strata_ui_shots.mjs`). **Needs a hardware test**: the drawing's time on the FM-1 (BENCH's SCREEN row), the
panel's look (the anti-aliasing, the PAPER theme), the live parts' smoothness.

## 1. The look

- **One screen a module.** A page is one 240 × 240 picture: a top line, a body that draws what the module does with
  the four knobs' numbered handles on what each moves, and a bottom row.
- **Top line.** The section's name in MID, 8 px tracked; its dot before it (filled: on; hollow: off, EDIT + OCT
  turns it on); at the right the family's module dots (EDIT 5, ENV 2, LFO 2, GLO 3), or a module's place in the FX
  chain as eight squares (filled: this one, outlined: in the chain, dashed: bypassed), or a word (HOME's tempo with
  its dot, KEYS' octave). An LFO's line names its rate and mode, the matrix the slots in use, the chain view its count
  and its CPU cost.
- **Bottom row.** The section's pages as zones (ALGORITHM steps them; the one shown inverted), or a toast (a HOT dot
  and a word for 1.5 s) in their place. HOME, the browser and NAME IT have no zones.
- **A knob turning.** Its handle goes HOT, its value shows in a flag beside it ("CUTOFF 998 Hz"), the shape before the
  turn stays dashed behind (the gesture's first state on the undo history), for a second after the last detent.
  Values show only while turning; the rest of the time the drawing says it.
- **Colour.** Five per theme: GROUND, INK (the drawing and the text), MID (labels, the lesser lines), LINE (tracks,
  the faint), HOT (the one accent: what is live: a knob turning, the voices, a countdown). SETTINGS > SCREEN: STRATA
  (warm white on black, vermilion), CLASSIC, ICE, VIOLET, PAPER (the light one), NIGHT, HI-CON, MONO. Dashed means
  before, off or bypassed.
- **Type.** Space Mono (SIL OFL 1.1), the design's letter-spacing set as each text is drawn: 7 px regular and bold
  (labels, chips), 8 px (the top line, the zones), 9 px bold (flags, the chain's names), 12 px bold (a module's
  word), 18 and 26 px bold (HOME's name by its length, the tempo, a countdown). The design's 10 px is drawn at 9, its
  14 px at 12.

## 2. The parts (`firmware/src/sy_gfx.c`)

The kit draws on gfx.c's canvas, which `lk_begin` places on the screen as a window (a strip of it, or a live part's
box); every shape takes the screen's coordinates and draws what of it falls inside (a shape wholly outside costs its
test). Anti-aliased, blended over the canvas in RGB565: boxes and outlines with fractional edges, discs, rings and
arcs (each row's span of the ring by a root, an arc's ends by cross products), segments, polylines (column by column
along the major axis, each segment half-open so a joint is drawn once), curves (a y a column, the stroke widened by
the slope up to 45 degrees and across the columns beyond it, solid or dashed along its length), the stipple, a
triangle, a toggle's capsule. No libm, no tables: square roots by Newton steps, angles by `sy_sin2pi` and rotations.
Text: `lk_text` places Space Mono on the baseline the design's CSS line box puts it (half the line height, plus
0.3795 of the size: Space Mono's (ascent − descent) / 2), with the letter-spacing in thousandths of an em; a face is
monospace, so a text's width is its length times one advance.

Shared parts: a knob's handle, a value's flag, chips, the zones, the toast, the top line, the chain's squares, a
layer's banner, the mix row every effect shares, a fader, the FM-1's 27 keys, the effects' glyphs (10 × 7, a bit a
pixel: 9 bytes each, traced from the design's paths).

## 3. The frame (`firmware/src/sy_ui_draw.c` `sy_ui_frame`)

- **Strips.** The screen is drawn in three strips of 80 rows on a 240 × 80 canvas (V0.66: 240 × 114; 16,320 bytes of
  the pool less). Each strip's rows are hashed (FNV-1a, two pixels a step) and only the rows that changed are sent.
  A knob's turn redraws rows 16 .. 221 (its top line and its zones as they were; HOME, LFO and MATRIX: all of it).
- **Curves computed once.** The curves a page computes (the filter's and the effects' responses, an envelope, an
  LFO's cycle) are kept for the frame's other strips (`ui_curve`: 3 × 217 floats in the spare SRAM).
- **Live parts.** Voices on an envelope or an LFO, OTT's levels, the voices' pills: the page is placed in an empty
  window (nothing drawn) to find the rows they take now, then only those rows and the ones they took before are
  redrawn, 14 times a second while they move. The bloom, an LFO's dial, the tape's reels, the metronome and a
  countdown's ring are the page drawn into their own box and sent alone; their rows' hashes are forgotten, so the
  next strip sends them again.
- **The bloom** (HOME): the scope's capture (sy_engine.c `sy_scope`, 2 × 212 points, three periods of the newest
  voice's note, triggered) kept in `ui_scope` as V0.66 kept it, wrapped once around (150, 108): radius 36 + 17 px at
  the capture's peak (at most × 4 for a quiet one), the right side thin in MID, the left bold in INK, a HOT dot at its
  end; each point's angle from the last by a rotation. It turns once in 48 s and breathes (× 0.965 .. 1, 3.2 s) while
  sound plays, redrawn 20 times a second (V0.66's scope: 25). Its box is a disc of radius 54.3: each row sends (and
  clears) only its span inside the circle, so the dial's 48 ticks, from 55.5 out, are drawn with the page, never with
  the bloom.
- **Silence.** Nothing moves without sound: the bloom, the dial and the reels stop, the tempo's dot pulses only while
  PLAY runs or sound plays; a page in silence sends no pixel.

## 4. The screens

| Family | Screens (zones) |
|---|---|
| HOME | the name (26, 18 or 12 px by its length), its category and bank, TABLE, UNISON, FILTER; the bloom; MACRO 1-4 as dials |
| EDIT | OSC A, OSC B: WAVE (the table as strata, the frame playing bold, a small bloom of it, AMOUNT), UNISON (the voices fanned, DETUNE's bracket, WIDTH), SPREAD (each voice's frame, the phase clock), PITCH (the octave ladder, the semitone on an octave of keys, the cents gauge, PH MODE), OUTPUT (LEVEL, PAN's arc, ROUTE through the filter or around it) · SUB: MAIN (its shape under the oscillator, LEVEL, WIDTH), OUTPUT · NOISE: MAIN (two fields of grain, the colour's tilt, LEVEL), OUTPUT · FILTER: TONE (the response stippled under, the cutoff's post, the modulation's range, DRIVE), MORE (KEYTRACK on the keys, VAR's side dashed, MIX) |
| ENV | ENV 1, ENV 2: ADSR (the envelope hatched, the other one dotted, the voices riding it with trails), SHAPE (HOLD and the three curves) |
| LFO | LFO 1, LFO 2: POINTS (the drawn shape, the point picked and its moves, the segment KNOB 4 curves, the voices), SHAPE (six shapes, the cycle bracketed, SYNC's and MODE's chips), TIME (DELAY, RISE, the start, SMOOTH); a dial turning at the LFO's phase |
| FX | the chain view (the modules on a cable in their order, the type to add as a tag); each module: DRIVE, CHORUS (+ MORE), DELAY (+ TONE), REVERB (+ TONE), OTT (+ BANDS), EQ (POINTS, GAINS), FLANGER (+ MORE), PHASER (+ MORE), FILTER (+ MORE), BITCRUSH, TREMOLO (+ MORE), RING MOD, TAPE (+ TONE) |
| GLO | VOICE: VOICE (the pills, HOT playing; GLIDE's curve), PLAY (the bend ladder, VEL AMP's slope, VOLUME) · MATRIX: SLOTS (the eight, the one picked outlined; its source, place and amount at the bottom) · GLOBAL: TEMPO (the metronome swinging while PLAY runs, the bar's beats) |
| SCL | KEYS: SCALE (the scale on the 27 keys: dots in it, rings on the root, chevrons where SNAP moves a key; FIT's chips, VELOCITY) |
| SETTINGS | SCREEN (the eight themes, HELP, GLOW), PANEL (ACCEL, HOLD), MIDI (the channel, LEARN, the CCs learned, PROGRAM, the fixed CC map), BENCH (eight rows, the bar the last, the tick the peak) |
| SAVE | NAME IT (sixteen cells, the category and the slot, ABC / 123, the keys and what each types), the browser (the categories with counts, six presets, the one playing outlined with a small bloom, the bank's chips) |
| Layers | MOD (the page's four parameters with the source's depth, the keys and their sources), PERFORM, MACROS (the dials, the chain's modules on F3 .. F4), MOMENTARY (the page, what goes back dashed), JUMP (the keys and their sections) |
| System | the countdowns (SAVE held, HOME held, LFO held on POINTS, OCT− held in the browser, OCT− + OCT+: a HOT ring that empties), SAFE START, MIDI LEARN's banner, power-on |

Not in this build: the design's proposed boards (ARP, SEQ, SYNC, the HOME and MATRIX variants, KEYS' chords) and the
system sheet: their features are not in the firmware yet.

## 5. Budget against V0.66

| | V0.66 | V0.66 UI |
|---|---|---|
| The app image | 327,184 B | 326,280 B (the fonts 76.1 KB → 39.0 KB) |
| RAM `.data` + `.bss` | 46,560 B | 47,128 B (the row hashes: 240 rows, not 114) |
| The pool | 358,692 B | 342,372 B (the canvas 240 × 80) |
| The spare SRAM | 19,296 B | 21,900 B (the curves' cache) |
| Main stack (the build's count) | | ~4,564 B of 8,192 |

The drawing's time, measured on the host (`web/emu/strata_bench.c`: the emulator built natively, each frame's work
timed; both builds on one machine, medians of 5 to 7 runs; not the FM-1's milliseconds):

| 2 s of the panel | V0.66 µs | UI µs | pixels sent |
|---|---|---|---|
| HOME, silence | 36 | 33 | 0 → 0 |
| HOME, a chord (the scope → the bloom) | 1,833 | 1,802 | 351,600 → 308,727 |
| FILTER TONE, KNOB 2 turning | 3,735 | 3,584 | 1,147,440 → 1,256,880 |
| ENV 1 ADSR, a chord held | 1,429 | 718 | 24,720 → 30,240 |
| every page shown once | 7,658 | 7,125 | 4,550,400 → 4,550,400 |

## 6. Building and checking

- `./build.sh --strata` on this branch builds the UI (its fonts: `gen_aa_font.py --preset space-mono`).
- `web/emu/build_strata.sh`, then `node web/emu/strata_emu_test.mjs` (90 checks) and
  `node web/emu/strata_ui_shots.mjs` (every screen as PNG, named after the design's boards, in
  `build/emu_strata/ui_shots`).
- `cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_bench web/emu/strata_bench.c -lm`, then
  `build/emu/strata_bench`: the UI's drawing time in five sessions of the panel.
