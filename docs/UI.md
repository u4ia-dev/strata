# The STRATA UI (V0.67)

Strata's screen since V0.67: the **Strata UI Overhaul** design (its gallery of 81 boards, approved 2026-10-17), first
built as the alternate "V0.66 UI" on the `strata-ui` branch, then made legible in V0.67 after its hardware test
("some of the text is too small"): nothing under 9 px, the labels brighter, larger handles, flags and zones, fewer
words on the screens, a **VALUES** zone on every module for exact values, four more themes, and a new animation on
HOME, the bloom as a sunflower. The sound engine, the panel's controls, the flash, the package and the update path
are V0.66's. [CONTROLS.md](CONTROLS.md) is the reference for the panel; this file is the reference for the screen.
V0.7 added SCL's chord screens, V0.75 the ARP's (STYLE, GROOVE, STEPS, RATCHET), CHORD's rework and STRUM, GLOBAL's
CLOCK IN (§4), the knob turning's value in place or in the bottom row in the flag's stead, and a plainer bloom. V0.8
added the sequencer's (NOTES, FEEL, MOTION, PATTERN, PLAY+REC, CHAIN) and the PATTERNS layer (§4).

Status: verified in the emulator (130 checks, `web/emu/strata_emu_test.mjs`; every screen rendered and checked for
overlaps, `web/emu/strata_ui_shots.mjs`; every knob on every page turned and checked, `web/emu/strata_knob_sweep.mjs`;
the bloom's motion, `web/emu/strata_bloom_study.mjs`). **Needs a hardware
test**: legibility at the panel's size and distance, the drawing's time on the FM-1 (BENCH's SCREEN row), the new
themes on the panel, the bloom's motion.

## 1. The look

- **One screen a module.** A page is one 240 × 240 picture: a top line, a body that draws what the module does with
  the four knobs' numbered handles on what each moves, and a bottom row.
- **Top line.** The section's name in LAB, 10 px bold; its dot before it (filled: on; hollow: off, EDIT + OCT turns
  it on); at the right the family's module dots (EDIT 5, ENV 2, LFO 2, GLO 3), or a module's place in the FX chain as
  eight squares (filled: this one, outlined: in the chain, dashed: bypassed), or a word (HOME's tempo with its dot,
  KEYS' octave). An LFO's line names its rate and mode, the matrix the slots in use, the chain view its count and its
  CPU cost.
- **Bottom row.** The section's pages as zones, 10 px bold (ALGORITHM steps them; the one shown inverted; where they
  do not fit, the others by three letters, then one), or a toast (a HOT dot and a word for 1.5 s) in their place.
  HOME, the browser and NAME IT have no zones. A module's VALUES pages show as one zone, "VALUES 2/5" ("VAL 2/5"
  among six).
- **A knob turning.** Its handle goes HOT, the shape before the turn stays dashed behind, for a second after the last
  detent. Its value (V0.75, in the flag's stead): where the page shows it (a chip, a figure, a title, a row's value),
  that goes HOT; else the bottom row shows its name and value ("CUTOFF 998 Hz", the value HOT) in the zones' place. No
  flag covers the page.
- **Handles.** A ring of 7 px around the knob's number (9 px bold); two never share a place (a handle that would sit
  on another's moves along its row, clear of it).
- **Colour.** Six per theme: GROUND, INK (the drawing and the values), LAB (the labels' text: INK over GROUND at
  ~76 %, V0.67), MID (the lesser lines; no text), LINE (tracks, the faint), HOT (the one accent: what is live: a knob
  turning, the voices, a countdown). Twelve themes, SETTINGS > SCREEN: STRATA (warm white on black, vermilion),
  CLASSIC, ICE, VIOLET, PAPER (light), NIGHT (dim), HI-CON, MONO, and V0.67's MOSS (sage on green-black,
  chartreuse), DUSK (blush on plum, rose), SAND (the second light one: ink on sand, deep teal) and NEON (mint on
  black, magenta). The labels' contrast on their ground: 6 to 14 : 1, most about 9 (NIGHT, dim by design, 4.9 : 1);
  V0.66 UI's labels in MID were 2 to 7 : 1, most about 4. Dashed means before, off or bypassed.
- **Type.** Space Mono (SIL OFL 1.1), the letter-spacing set as each text is drawn. **Nothing under 9 px**: 9 px
  regular and bold (labels, captions, a row's figure, a handle's number), 10 px regular (a module's line under its
  word) and bold (the top line, the zones, chips, the toast), 12 px bold (flags, the MATRIX's and the browser's
  rows), 14 px bold (a module's word, NAME IT's letters), 18 px bold (a name on HOME, VALUES' figures) and 26 px bold
  (a short name, the tempo, a countdown). V0.66 UI's were 7, 8, 9 and 12 px (and 18, 26).
- **Words.** The screens say what things are, not how to use them: the hints that were on the chain view, MOD, JUMP,
  the browser, NAME IT, MIDI and PERFORM went (HELP's toasts and [CONTROLS.md](CONTROLS.md) keep them).

## 2. The parts (`firmware/src/sy_gfx.c`)

The kit draws on gfx.c's canvas, which `lk_begin` places on the screen as a window (a strip of it, or a live part's
box); every shape takes the screen's coordinates and draws what of it falls inside (a shape wholly outside costs its
test). Anti-aliased, blended over the canvas in RGB565: boxes and outlines with fractional edges, discs (V0.67: each
pixel by (ro² − d²) / 2ro, the edge's distance to first order, each row's span by a reciprocal root, clipped once:
no root a pixel), rings and arcs, segments, polylines, curves (a y a column, the stroke widened by the slope up to 45
degrees and across the columns beyond it, solid or dashed), the stipple, a triangle, a toggle's capsule. No libm, no
tables: roots by Newton steps or the halved exponent, angles by `sy_sin2pi` and rotations. Text: `lk_text` places
Space Mono on the baseline the design's CSS line box puts it, with the letter-spacing in thousandths of an em; a face
is monospace, so a text's width is its length times one advance.

Shared parts: a knob's handle, a value's flag, chips (16 px high), the zones, the toast, the top line, the chain's
squares, a layer's banner, the mix row every effect shares, a fader, the FM-1's 27 keys, the effects' glyphs (10 × 7,
a bit a pixel: 9 bytes each).

## 3. The frame (`firmware/src/sy_ui_draw.c` `sy_ui_frame`)

- **Strips.** The screen is drawn in three strips of 80 rows on a 240 × 80 canvas. Each strip's rows are hashed
  (FNV-1a, two pixels a step) and only the rows that changed are sent. A knob's turn redraws rows 16 .. 221 (its top
  line and its zones as they were; HOME, LFO and MATRIX: all of it).
- **Curves computed once.** The curves a page computes are kept for the frame's other strips (`ui_curve`: 3 × 217
  floats in the spare SRAM).
- **Live parts.** Voices on an envelope or an LFO, OTT's levels, the voices' pills: only the rows they take now and
  took before are redrawn, 14 times a second while they move. The bloom, an LFO's dial, the tape's reels, the
  metronome and a countdown's ring are the page drawn into their own box and sent alone.
- **Silence.** Nothing moves without sound: a page in silence sends no pixel.

### The bloom (HOME, V0.67)

The scope's capture (sy_engine.c `sy_scope`, 2 × 212 points, three periods of the newest voice's note, triggered),
kept in `ui_scope` as before, sown as a sunflower's seeds round (166, 116): seed j (L's samples the even seeds, R's
the odd; the oldest sample at the middle, the newest at the rim) at j golden angles and k·√j out, moved out or in by
its sample (2.5 px at the capture's peak, at most × 4 for a quiet one), larger and brighter (MID to INK) the higher
it is. A sine's periods show as rings, a saw's as a spiral's grain, a chord's as their interference; the newest
sample is the one HOT seed.

- **It opens with the sound**: the capture's level sets how far the seeds spread (a bud of 22 px at −42 dBFS and
  below, 44 px at −6 dBFS; open in ~70 ms, closing over ~450 ms after the release).
- **It swirls at each note**: a new note bends the golden angle (by 0.0042 of a turn a seed for each note's worth of
  swirl, at most 2.5), so the seeds wind into curved spiral arms, one way for a note above the last, the other way
  for one below; the swirl unwinds back into the sunflower's parastichies in about a second (τ 600 ms).
- **It turns** once in 48 s, a little faster while it swirls.
- **Cost.** 424 seeds (the old trace: 424 segments), each placed from the last by a rotation and its radius by a
  reciprocal root: no tables, no new buffers. Its box is a disc of radius 49.5 (each row sends only its part inside
  it), the dial's 48 ticks outside it, from 50, drawn with the page. Redrawn 20 times a second while a fresh capture
  comes or it still opens, closes or unwinds; in silence, a still bud and nothing sent.

| The bloom against V0.66 UI's trace | V0.66 UI | V0.67 |
| --- | --- | --- |
| Its state in RAM | 8 B (the turn's, the breath's) + 4 (a timer) | 8 B (the turn, how open and the swirl in Q12, the last note's age and key, a flag) + the turn's 4 |
| The capture | 2 × 212 points (`ui_scope`, 848 B) | the same buffer |
| HOME's code (the bloom, the page, the frame) | 12,932 B | 12,882 B |
| Its drawing, 2 s of a chord (host) | 1,788 µs | 1,462 µs |
| Pixels sent, 2 s of a chord | 308,727 | 256,851 |

## 4. The screens

| Family | Screens (zones) |
|---|---|
| HOME | the name (26, 18 or 14 px by its length), its category and bank, TABLE, UNISON, FILTER (a label over each value); the bloom; MACRO 1-4 as dials |
| EDIT | OSC A, OSC B: WAVE, UNISON, SPREAD (WT SPREAD's value under the mode), PITCH (FREE and RETRIG named under their boxes), OUTPUT, VALUES · SUB: MAIN (OCT under the shape's name), OUTPUT, VALUES · NOISE: MAIN, OUTPUT, VALUES · FILTER: TONE, MORE, VALUES |
| ENV | ENV 1, ENV 2: ADSR, SHAPE, VALUES |
| LFO | LFO 1, LFO 2: POINTS, SHAPE, TIME, VALUES; a dial turning at the LFO's phase |
| FX | the chain view (eight boxes with 9 px bold names, the module picked named at the bottom, the type to add as a 12 px tag); each module its pages and VALUES |
| GLO | VOICE: VOICE, PLAY, VALUES · MATRIX: SLOTS (four slots at a time, the four of the one picked: source → destination at 10 px, the amount's bar and figure; the picked one's source and destination at 12 px with its dial) · GLOBAL: TEMPO (V0.75: CLOCK IN's chips, INT and MIDI; "BPM: MIDI" and MIDI's tempo while it follows the clock) |
| SCL | SCALE (the scale on the 27 keys, FIT's chips, VELOCITY's bar and figure) · CHORD: CHORD (V0.75: the chord named at 26 px, a row a knob: MODE's chips, TYPE's (KEY: SHAPE's) strip from the one before the current with its place in the list, VOICING's AUTO chip and ruler, SPREAD's chips; the notes and the bass on three octaves of keys), STRUM (V0.75: the chord as it strums, a row a note from its start, as bright as its level; DIRECTION, HUMAN, TAPER in a row), BASS (on, the octave, the chord and the bass on four octaves, an arrow from the root, LEVEL) · MIDI (the keys and MIDI in into the chords, into the sound and out over USB, the channels); the top line names the chord playing |
| ARP | ARP (V0.75): STYLE (the style at 14 px, ARP's state, a bar of sixteen steps of what it plays: a row a note, its time across, ties long, ratchets split, the level as brightness, a CHANCE under 100 % hollow, SWING's steps late, LENGTH's end dotted, the step playing marked under it; HARP: the note each key plays) · STEPS: STEPS and RATCHET (the step picked as the title; sixteen columns: OCTAVE a tick over the 0 line, LEVEL a bar, RATCHET's hits side by side, TIE carried across, REST a stub, SLIDE a slope, the steps past LENGTH dim, the one picked framed, the one playing marked over it; RATCHET's: each step's CHANCE a meter under it), GROOVE (two beats of notes where SWING puts them, an arrow from each late one's straight place, as long as GATE makes them; LENGTH's sixteen cells; LATCH's chip). Their values are figures in a row under the drawing (KNOB 1's the title), HOT while they turn |
| SEQ | SEQ (V0.8): NOTES, FEEL and MOTION share a frame: the step picked as the title (HOT while KNOB 1 turns; while step recording, framed HOT with "REC: THE KEYS FILL STEP 5" under the title), a line under it (the pattern's steps and RATE; MOTION: the lane and its parameter), four boxes at the top right (which sixteen steps are shown, the one playing marked under them), sixteen columns across 34 .. 226 with the beats ticked under, LENGTH's end dashed, the step playing marked over its column. NOTES: a piano roll (each note a bar from where NUDGE puts it, as long as LENGTH, as bright as its velocity; the range the notes span, an octave at least; the Cs dotted and named at the left). FEEL: NUDGE's ticks round each step's middle over CHANCE's meters (an arrow and a % at the left). MOTION: the lane's values as bars, a stub where none · PATTERN: PATTERN (the title; the sixteen as a 4 × 4 grid, the one shown filled, the one next HOT, those holding notes framed; a chip PLAYING, STOPPED or NEXT UP; LENGTH's 64 cells, a step with notes bright; a bar of steps at RATE, SWING moving the odd ones), PLAY+REC (TRANSPOSE as the title over a ruler of four octaves; DIRECTION's, RECORD's and QUANTIZE's chips) · CHAIN (the sixteen slots in two rows, each its pattern and its repeats, END after the last, the slot picked framed, the one playing marked; what the chain plays in words; CHAIN's chip). Their values are figures in a row under the drawing or in place, HOT while they turn; the top line: the pattern, the one next, REC ("P1>3 REC") |
| SETTINGS | SCREEN (the twelve themes, HELP, GLOW), PANEL, MIDI (the channel grid, LEARN, the CCs learned, PROGRAM, the CCs every sound answers as a table), BENCH (eight rows, wider figures) |
| SAVE | NAME IT (sixteen cells, the category and the slot, ABC / 123, each key's letter over or under it), the browser (the bank's chips, the category at 14 px with its count, five presets at 12 px, the one playing outlined with a small bloom) |
| Layers | MOD (the page's four parameters with the source's depth, each white key's source under it), PERFORM, MACROS, MOMENTARY, JUMP (the keys' sections over and under them, the section shown named at 14 px), PATTERNS (V0.8: the keys, 1 to 16 under the white ones, the pattern shown a disc, the one next a HOT ring, those holding notes a ring; CP, PA, CL over F#3, G#3, A#3; the pattern shown named at 14 px) |
| System | the countdowns, SAFE START, MIDI LEARN's banner, power-on |

**VALUES** (V0.67): every module's last zone (OSC A and B, SUB, NOISE, FILTER, ENV 1 and 2, LFO 1 and 2, VOICE, each
FX module): its pages' knobs again, a page each in the same order ("VALUES 1/5" is WAVE's knobs, "2/5" UNISON's, ..),
each knob a row: its name (10 px bold), its value large (18 px bold, HOT while it turns), its place on its range (a
bipolar one's bar from the middle), the range its modulation takes it, and the source's word ("ENV 2 +45.0%"). A knob
on VALUES steps the parameter's finest unit (0.1 % of its range) a detent at any speed; lists still step one entry. A
family button's tap steps the drawings only; ALGORITHM and OCT with a family button reach VALUES.

## 5. Budget against V0.66 UI

From the build (`tools/build.py --strata`):

| | V0.66 UI | V0.67 | |
|---|---|---|---|
| The app image | 326,280 B | 339,112 B | the fonts 39.0 → 48.6 KB (nothing under 9 px), VALUES (72 pages: 1.4 KB of table, its page 0.9 KB), the knobs' names and values split out (1.4 KB) |
| RAM `.data` + `.bss` | 47,128 B | 47,128 B | the same (the bloom's state 8 B as before; the labels' colour 2 B) |
| The pool | 342,372 B | 342,372 B | the same |
| The spare SRAM | 21,900 B | 21,900 B | the same |
| Main stack (the build's count) | ~4,564 B | ~4,572 B | of 8,192 allowed (12,032 reserved) |

The drawing's time, measured on the host (`web/emu/strata_bench.c`: the emulator built natively, each frame's work
timed; both builds on one machine, interleaved, medians of 7 runs; not the FM-1's milliseconds):

| 2 s of the panel | V0.66 UI µs | V0.67 µs | pixels sent |
|---|---|---|---|
| HOME, silence | 30 | 35 | 0 → 0 |
| HOME, a chord (the bloom) | 1,788 | 1,462 | 308,727 → 256,851 |
| FILTER TONE, KNOB 2 turning | 3,711 | 3,656 | 1,256,880 → 1,240,320 |
| ENV 1 ADSR, a chord held | 737 | 686 | 30,240 → 30,720 |
| every page shown once (108 pages → 201) | 71.2 a page | 59.0 a page | 42,133 → 42,699 a page |
| FILTER VALUES, KNOB 2 turning (new) | — | 2,096 | 328,320 |

HOME in silence: 5 µs more over 2 s (0.04 µs a frame; nothing drawn or sent in either; V0.66 UI's runs spread from
27 to 74 µs).

## 6. Building and checking

- `./build.sh --strata` builds the UI (its fonts: `gen_aa_font.py --preset space-mono`).
- `web/emu/build_strata.sh`, then `node web/emu/strata_emu_test.mjs` (130 checks) and
  `node web/emu/strata_ui_shots.mjs` (every screen as PNG, named after the design's boards, in
  `build/emu_strata/ui_shots`; each checked as it is taken).
- **The screens' check** (`web_lint`, `web/emu/strata_web.c`): the screen drawn twice into buffers of its own, with
  its text and without; every pixel the text changed is checked against what lies under it and its neighbours (only
  the ground, the text's own ground or a blend of the two may be there), the texts' ink boxes against each other, a
  text past the screen's edges, a text in MID or LINE, and two knobs' handles on one place. The emulator's tests run
  it on all 163 pages; the shots script on its 120 screens (findings in `lint.txt`, marked PNGs in `lint/`);
  `node web/emu/strata_knob_sweep.mjs` on every knob of every page as it turns (570 turns).
- `node web/emu/strata_bloom_study.mjs`: the bloom over time (a note's swirl, a second note, the release, a chord) as
  one sheet, `build/emu_strata/bloom_study.png`.
- `cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_bench web/emu/strata_bench.c -lm`, then
  `build/emu/strata_bench`: the UI's drawing time in six sessions of the panel.
