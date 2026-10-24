# The STRATA UI (V0.67)

Strata's screen since V0.67: the **Strata UI Overhaul** design (its gallery of 81 boards, approved 2026-10-17), first
built as the alternate "V0.66 UI" on the `strata-ui` branch, then made legible in V0.67 after its hardware test
("some of the text is too small"): nothing under 9 px, the labels brighter, larger handles, flags and zones, fewer
words on the screens, a **VALUES** zone on every module for exact values, four more themes, and a new animation on
HOME, the bloom as a sunflower. The sound engine, the panel's controls, the flash, the package and the update path
are V0.66's. [CONTROLS.md](CONTROLS.md) is the reference for the panel; this file is the reference for the screen.
V0.7 added SCL's chord screens, V0.75 the ARP's (STYLE, GROOVE, STEPS, RATCHET), CHORD's rework and STRUM, GLOBAL's
CLOCK IN (§4), the knob turning's value in place or in the bottom row in the flag's stead, and a plainer bloom. V0.8
added the looper's (CLIPS, MIXER, TRACK, RECORD, CLICK, NOTES, EDIT, CLIP, SONG, PROJECT) and the launch pad (§4),
in place of V0.8's sequencer pages (never installed). V0.86 (step 1 of its plan, the interface) laid every menu screen
out one way (§1), regrouped the pages, gave HOME three pages and a drum kit's view, PRESETS a list, SEQ two groups
with SPEED and TOOLS, the buttons a second press, HOME held a help map, SETTINGS POWER and KEY SCAN, and wrote the
LEDs' rules down (§1); step 2, the drum editor, added DRUM's and KIT's screens and the kits' list (§4). V0.87.1 (the build from the V0.87
hardware test, public in V0.875) changed the screens listed under the table in §4: "V0.87.1's screens".

Status: verified in the emulator (166 checks, `web/emu/strata_emu_test.mjs`; every screen rendered and checked for
overlaps, `web/emu/strata_ui_shots.mjs`; every knob on every page turned and checked, `web/emu/strata_knob_sweep.mjs`;
the bloom's motion, `web/emu/strata_bloom_study.mjs`). **Needs a hardware
test**: legibility at the panel's size and distance, the drawing's time on the FM-1 (BENCH's SCREEN row), the new
themes on the panel, the bloom's motion.

## 1. The look

- **One screen a module.** A page is one 240 × 240 picture: a top line, a body that draws what the module does with
  the four knobs' numbered handles on what each moves, and a bottom row.
- **One layout on every menu screen** (V0.86, L1-L8): HOME, the modules, SEQ's pages, the SETTINGS, the browser and
  NAME IT alike; the layers (MOD, PERFORM, MACROS, MOMENTARY, the launch pad, the help map; V0.87: JUMP gone) and the dialogs (the
  countdowns, SAFE START, PRESETS' list) keep their own look.
- **Top line.** The module's name in LAB, 10 px bold; its dot before it (filled: on; hollow: off, EDIT + OCT turns
  it on); a word of status left of the dots (HOME's tempo with its dot, SCL's octave or chord, SEQ's track, scene and
  bar); at the right the menu's module dots, SELECT's (EDIT 5, ENV 2, LFO 2, GLO 3; one for a menu of one module;
  SEQ's four in two groups, LOOP and CLIP, TRACK and SONG, the group not shown dimmed), or a module's place in the FX
  chain as eight squares (filled: this one, outlined: in the chain, dashed: bypassed). An LFO's line names its rate
  and mode, the matrix the slots in use, the chain view its count and its CPU cost. SETTINGS > POWER > SHOW BATTERY:
  the battery after the name (16 x 8: its cells, the lowest alone HOT; USB power a bolt), where it fits.
- **Bottom row.** The module's pages as zones, 10 px bold, on every screen even with one page (ALGORITHM steps them;
  SELECT and ALGORITHM stop at the ends): the one shown in full and inverted, the others whole where all fit, else by
  their short names (UNI, SPR, PIT, OUT, VAL; three letters by default), and past twelve or so a window round the one
  shown with a < or > where the row goes on (in LAB). A module's VALUES pages show as one zone, always the last,
  "VALUES 2/5" ("VAL 2/5" short). A toast (a HOT dot and a word for 1.5 s) takes the row's place.
- **A P in a square** (V0.86): what PRESETS turns on the page shown, beside it as a knob's handle is beside its value:
  the sound (HOME), the drum (a drum track's HOME), the track (every SEQ page's title).
- **Unsaved** (F25): a small HOT dot after the sound's or kit's name on HOME, in PRESETS' list and in the browser, while
  it is edited since it was loaded or saved. Nowhere else.
- **A knob turning.** Its handle goes HOT, the shape before the turn stays dashed behind, for a second after the last
  detent. Its value (V0.75, in the flag's stead): where the page shows it (a chip, a figure, a title, a row's value),
  that goes HOT; else the bottom row shows its name and value ("CUTOFF 998 Hz", the value HOT) in the zones' place. No
  flag covers the page.
- **Handles.** A ring of 7 px around the knob's number (9 px bold); two never share a place (a handle that would sit
  on another's moves along its row, clear of it).
- **Colour.** Six per theme: GROUND, INK (the drawing and the values), LAB (the labels' text: INK over GROUND at
  ~76 %, V0.67), MID (the lesser lines; no text), LINE (tracks, the faint), HOT (the one accent: what is live: a knob
  turning, the voices, a countdown). Twelve themes, SETTINGS > SCREEN: STRATA (warm white on black, vermilion; V0.875: a black ground, labels 13.9 : 1),
  CLASSIC, ICE, VIOLET, PAPER (light), AMBER (V0.875: amber phosphor on black; NIGHT's place), HI-CON, MONO, and V0.67's MOSS (sage on green-black,
  chartreuse), DUSK (blush on plum, rose), SAND (the second light one: ink on sand, deep teal) and NEON (mint on
  black, magenta). The labels' contrast on their ground: 6 to 14 : 1, most about 9;
  V0.66 UI's labels in MID were 2 to 7 : 1, most about 4. Dashed means before, off or bypassed.
- **Type.** Space Mono (SIL OFL 1.1), the letter-spacing set as each text is drawn. **Nothing under 9 px**: 9 px
  regular and bold (labels, captions, a row's figure, a handle's number), 10 px regular (a module's line under its
  word) and bold (the top line, the zones, chips, the toast), 12 px bold (flags, the MATRIX's and the browser's
  rows), 14 px bold (a module's word, NAME IT's letters), 18 px bold (a name on HOME, VALUES' figures) and 26 px bold
  (a short name, the tempo, a countdown). V0.66 UI's were 7, 8, 9 and 12 px (and 18, 26).
- **Words.** The screens say what things are, not how to use them: the hints that were on the chain view, MOD, JUMP,
  the browser, NAME IT, MIDI and PERFORM went (HELP's toasts, the help map and [CONTROLS.md](CONTROLS.md) keep them).
- **LEDs** (V0.86, F32: kept everywhere): **off**, nothing there; **glowing** (the dim level), there (a clip, a module
  in the chain, a scale's note, a drum); **mid**, in use (the root, an accented drum, the playhead's step); **lit**,
  active (a key held, a clip playing, the family shown, the transport's beat); **blinking** (a quarter second), waiting
  (a launched clip, a count-in, REC waiting for the bar); **breathing**, recording or a latched layer (a layer's
  button). A new light takes one of these, by what it means.

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
| HOME | SOUND: the name (26, 18 or 14 px by its length; the unsaved dot), its category and bank, TABLE, UNISON, FILTER (a label over each value); the bloom; MACRO 1-4 as dials. On a drum track (V0.86): the kit's name, "P T8 DRUMS · KICK", the eight drums as a 2 x 4 grid of 28 x 38 cells (each its four-letter name and its LEVEL as a bar, lit by its envelope as it sounds, the one PRESETS picked framed), the bloom smaller at the right; TUNE, DECAY, TONE, DUCK as dials. MIX 1-4, MIX 5-8 (V0.86): MIXER, the eight tracks as faders (number, type, VOLUME), the page's four with handles, the others in LAB and LINE |
| EDIT | OSC A, OSC B: WAVE, UNISON, SPREAD (WT SPREAD's value under the mode), PITCH (FREE and RETRIG named under their boxes), OUTPUT, VALUES · SUB: MAIN (OCT under the shape's name), OUTPUT, VALUES · NOISE: MAIN, OUTPUT, VALUES · FILTER: TONE, MORE, VALUES · MATRIX (V0.87, after FILTER): SLOTS (four slots at a time, the four of the one picked: source → destination at 10 px, the amount's bar and figure; the picked one's source and destination at 12 px with its dial) |
| EDIT (a drum track) | V0.86 step 2. DRUM: the drum PRESETS picks as the title at 14 px with its P, the eight names under it in two rows (the picked one inverted); SOUND (its fall from the hit to -60 dB, KICK's sweep dashed), DRIVE (its curve, level in against out, the straight line dashed), EQ (its response 20 Hz .. 20 kHz, ±12 dB, a handle on the curve a band, FREQ's arrows over MID's; the values in a row under it), SHAPE (the fall with SHAPE, without it dashed; 12 dB of room over 0 dB), MIX (the eight as a mixer: LEVEL a bar, PAN a dot over it), VALUES; the knobs' values in a row under the drawing. KIT: ACCENT (the gain by velocity, plain and accented; TONE's line under it), DUCK (SOURCE's chip; the synth's level over time from a hit, "BACK IN 118 MS"), CURVE (SHAPE's name as the title, the same drawing with HOLD), COMP ("4:1 OVER -15 DB · +3.4 DB" or "OFF: THE DRUMS AS THEY ARE"; its curve in against out, the threshold dashed with a marker), VALUES |
| ENV | ENV 1, ENV 2: ADSR, SHAPE, VALUES |
| LFO | LFO 1, LFO 2: POINTS, SHAPE, TIME, VALUES; a dial turning at the LFO's phase |
| FX | the chain view (eight boxes with 9 px bold names, the module picked named at the bottom, the type to add as a 12 px tag); each module its pages and VALUES |
| GLO | VOICE: VOICE, PLAY, VALUES · GLOBAL: TEMPO (V0.75: CLOCK IN's chips, INT and MIDI; "BPM: MIDI" and MIDI's tempo while it follows the clock) |
| SCL | SCALE (the scale on the 27 keys, FIT's chips, VELOCITY's bar and figure) · CHORD: CHORD (V0.75: the chord named at 26 px, a row a knob: MODE's chips, TYPE's (KEY: SHAPE's) strip from the one before the current with its place in the list, VOICING's AUTO chip and ruler, SPREAD's chips; the notes and the bass on three octaves of keys), STRUM (V0.75: the chord as it strums, a row a note from its start, as bright as its level; DIRECTION, HUMAN, TAPER in a row), BASS (on, the octave, the chord and the bass on four octaves, an arrow from the root, LEVEL); the top line names the chord playing |
| ARP | ARP (V0.75): STYLE (the style at 14 px, ARP's state, a bar of sixteen steps of what it plays: a row a note, its time across, ties long, ratchets split, the level as brightness, a CHANCE under 100 % hollow, SWING's steps late, LENGTH's end dotted, the step playing marked under it; HARP: the note each key plays) · STEPS: STEPS and RATCHET (the step picked as the title; sixteen columns: OCTAVE a tick over the 0 line, LEVEL a bar, RATCHET's hits side by side, TIE carried across, REST a stub, SLIDE a slope, the steps past LENGTH dim, the one picked framed, the one playing marked over it; RATCHET's: each step's CHANCE a meter under it), GROOVE (two beats of notes where SWING puts them, an arrow from each late one's straight place, as long as GATE makes them; LENGTH's sixteen cells; LATCH's chip). Their values are figures in a row under the drawing (KNOB 1's the title), HOT while they turn |
| SEQ | The looper (V0.8; V0.86: the title the track with its P (PRESETS'), KNOB 1 no longer the track; CLIPS: TYPE and SCENE, "THE KEYS / LAUNCH" in the third column, the selected track's column lit in LINE, a launched clip blinking; TYPE turned shows the type it goes to, with a ? while it asks; RECORD: QUANTIZE's chips, CHANNEL and MIDI IN; CLICK: four rows, CLICK, LEVEL with the bar's beats beside it, COUNT-IN, LAUNCH; NOTE+ (was EDIT): VELOCITY on KNOB 1; CLIP: LENGTH, SPEED (x1/2, x1, x2), QUANTIZE, MOTION; TOOLS: four rows, each what its knob does and what it would do now, an ask HOT. A drum track's GRID with a step held: "STEP 6 HELD", its column lit, the four knobs two by two under the grid, its rows 11 px). CLIPS: the selected track as the title (KNOB 1) and what runs as a chip (PLAYING, STOPPED, COUNT-IN, REC: NEXT BAR, REC, OVERDUB); the tracks' numbers (the selected one boxed, a muted one underlined) over an 8 × 8 grid of 25 × 13 cells, the scenes' letters at the left (the one picked bold): a clip playing filled with a line along its foot for its place (stopped but on: MID), a clip there framed, an empty cell a dot, one launched framed HOT, one recording filled HOT, one stopping at the bar a notch; the selected track's cell in the scene picked framed. MIXER: a strip a track (PAN's dot on a short line, a fader of VOLUME with a tick at 100, M and S under; the selected one framed). TRACK, RECORD, CLICK: words and chips (TYPE, QUANTIZE, COUNT-IN, LAUNCH, CLICK; CHANNEL and MIDI IN as 12 px values; LEVEL ten cells; the bar's beats as rings, the one now HOT). NOTES, EDIT: "NOTE 5 OF 12" as the title, the track, scene and bar under it, a piano roll a bar at a time round the note picked (the range its notes span, an octave at least, the Cs dotted and named at the left; each note a bar as bright as its velocity; the one picked framed, HOT while it moves; the playhead marked over the bar playing; EDIT: a lane of velocities under it). CLIP: LENGTH as the title and sixteen cells, the clip's notes at a glance, the parameter picked's moves as a step line. SONG: the sixteen slots in two rows (a scene's letter and its repeats each, END after the last, the slot picked framed, the one playing marked), SONG's chip. PROJECT: four boxes (the one loaded filled; EMPTY, KEPT or PRESETS THERE), the events in use. Their values in a row under the drawing or in place, HOT while they turn; the top line: the track, the scene, the bar and beat while running, REC or DUB ("T3 A 2.2 REC") |
| SETTINGS | SCREEN (the twelve themes, HELP, LIGHTS: OFF to HIGH, a sun whose rays grow with it), PANEL, MIDI (the channel grid, LEARN, the CCs learned, PROGRAM, the CCs every sound answers as a table), BENCH (eight rows, wider figures), KEY SCAN (V0.86: the 11 x 6 matrix live, a cell a key's, an encoder pin's or none's; one of none seen closed HOT), CORE 2, POWER (V0.86: the battery 26 x 13 with four cells, "BATTERY 3 OF 4" or "USB POWER", the raw reading; SHOW BATTERY, SCREEN OFF AFTER at 14 px; what wakes it); DEBUG (V0.875: the four corners, OCT- + F3 + G5 + GLO: BENCH, KEY SCAN, CORE 2, TESTS, WIPE (V0.87, asked as a window; V0.87.1: DEBUG's)) |
| SAVE | NAME IT (sixteen cells, the category and the slot, ABC / 123, each key's letter over or under it), the browser (the bank's chips, the category at 14 px with its count, five presets at 12 px, the one playing outlined with a small bloom) |
| PRESETS' list | V0.86: PRESETS and "2 OF 10" on the top line; five rows of 31 px round the one picked (its number, its name at 12 px, the one playing a dot at the right, the unsaved dot after a name), the one picked inverted; under them SAVE + OCT+'s and SAVE + OCT−'s words; the bottom row "SAVE TWICE: LOAD IT" (HOT "SAVE TWICE: YOUR EDITS GO" when it would leave edits); SAVE + OCT−'s question in the rows' place, framed HOT ("DELETE?", the name, how to answer). The kits' list (V0.86 step 2; V0.87: EDIT twice from KIT or HOME on a drum track): KITS and "1 OF 6", the six factory kits as the presets' rows (the one the project's kit came from a dot at the right, its edits' dot after the name), what a kit holds in two lines under them, "SAVE TWICE: LOAD IT" (HOT "SAVE TWICE: THE KIT'S EDITS GO") |
| Windows (V0.87) | over the page, its top and bottom lines kept: the assign window (ASSIGN AN LFO / AN ENVELOPE: SOURCE, the page's parameters with a source's amount dotted, those it cannot move "NO"; page 2 POLARITY and AMOUNT as a dial, V0.87.1: pointing up at rest), the SAVE window (three rows, the one picked inverted), the questions (DELETE?, T1: DRUMS?, WIPE?, V0.87.1: TEST?: what the opener, pressed twice, does, what once or anything else does), the lists (presets, wavetables with their categories, kits, projects: a row each, the one picked inverted, the one playing dotted) |
| Layers | MOD (the page's four parameters with the source's depth, each white key's source under it), PERFORM, MACROS, MOMENTARY, (V0.86's JUMP: the keys' sections over and under them, the section shown named at 14 px; a latched layer's banner says its button closes it), the help map (V0.86: HOME held; "HELP" banner; the screen's module and page; SELECT, ALGORITHM, PRESETS and the knobs on it; the buttons' twice in two columns; a bar filling toward the SETTINGS at 5 s), LAUNCH PAD (V0.8: the keys, 1 to 8 and A to H under the white ones, 1 to 8 over the black ones (a muted track's HOT), ST, SO, UN over the last three; a track's clip in the scene picked a disc while it plays, a HOT ring launched, a ring there, HOT recording; the scene picked a disc; the selected track and scene named at 14 px) |
| System | the countdowns, SAFE START, MIDI LEARN's banner, power-on (V0.86: the splash held 2 s, then HOME; V0.87.1: the splash is the logo), the screen dark (SCREEN OFF AFTER: nothing drawn; the page redrawn whole as it wakes) |

**V0.87.1's screens** (the lead's decisions from the V0.87 hardware test; **verified in emulator**, legibility on the
panel **needs a hardware test**):

- **The splash** is the logo (`tools/gen_logo.py`'s spec, in a 100-unit square at 1.4 px a unit, centred at (120, 86)): 89
  dots on the golden angle at radius 36.12 sqrt((i + 0.5) / 89), discs of 7.2 down to 3.96 units, the ink fading outward
  to half; the dots i % 8 == 3 (past the third) in the accent, a spiral arm; the dial's ring of 60 ticks from twelve
  o'clock, every fifth long, at 46.5 units. The theme's own colours from the first frame.
- **The assign window's page 2** (POLARITY, AMOUNT): the dial points up at rest, the knob's place; the arc is the swing
  the source makes from it (BIPOLAR both ways of the top by the AMOUNT, UNIPOLAR one way by its sign; the dial's ends
  are +-100 %); a tick marks the top.
- **ARP STEPS**: each step's CHANCE a fader's cap on a rail in its column (up at 100 %, down at 0; a ground-coloured gap
  round the cap keeps it clear of the level bar); RATCHET's page keeps its hollow bars.
- **TAPE**: the reels turn from one turn in 5 s at rest to one in 1.2 s at full level, the tape's ripples travel (WOW's
  at 0.6 Hz, FLUTTER's at 8 Hz), the heads glow with the level and a dot rides SATURATE's curve (the output's peak,
  smoothed; quick up, slower down); the frames go on a moment after the sound.
- **The drum GRID**'s row names are short (KICK, SNARE, CLAP, HAT C, HAT O, CYM, WT 1, WT 2, DUCK) and stop short of
  the first step (x 48).
- **CLIPS**: three handles in the bottom columns, TYPE (KNOB 1), PLAYS / MUTE / SOLO (KNOB 2) and TRACK, "T3" (KNOB 4),
  and "KEYS: LAUNCH . OCT-, OCT+: SCENE" under them; no P tag on its title (PRESETS is the sound's again); the selected
  cell and the scene picked in ink, not vermilion.
- **DEBUG > TESTS**: four rows (a number, the name at 12 px bold, at the right what it stresses, or "RUNNING 12 S" /
  "ENDED 70 S" once loaded), the one picked inverted; below, "KNOB 1 PICKS . HOME TWICE LOADS IT", "KNOB 2: STOPS THE ONE
  LOADED", "NOTHING IS SAVED WHILE ONE IS LOADED", "A PROJECT LOADED, NEW, OR A RESTART ENDS IT. STOPPING IT DOES NOT". The
  question ("TEST 2: FX?") is a window: "HOME TWICE: LOAD IT AND START IT", "ONCE, OR ANOTHER BUTTON: NO", "NOTHING FROM
  IT IS EVER SAVED", "LOADING A PROJECT ENDS IT".
- **TEST . NOT SAVED** in vermilion on every page's top line while a test is loaded (the longest of "TEST . NOT SAVED",
  "NOT SAVED", "TEST" that fits between the label and what is at the right; the battery keeps clear of it); HOME shows
  "TEST 2 . FX" under the sound's rows.
- **Confirming**: the lists' bottom rows say "EDIT TWICE: LOAD IT" (tables, kits, drum presets), "SEQ TWICE: LOAD IT"
  (projects), "SAVE TWICE: LOAD IT" (sounds); the questions: TYPE "SEQ TWICE: CHANGE IT", WIPE "HOME TWICE: EVERY SETTING
  BACK". The help map adds "A LIST OR QUESTION: ITS OPENER x2"; off HOME it has no countdown bar and its banner reads
  "LET GO TO CLOSE" (V0.875: on every page; its last line says "DEBUG: OCT- + F3 + G5 + GLO HELD"). F3's LED glows on PERFORM's layer (the tap-tempo key), and the
  layer's screen says "THE LOWEST KEY, F3: TAP TEMPO"; V0.875: under it four dots, one a tap (the last flashes in the accent, taps over 2 s apart start again), "n OF 3", then the BPM in the accent once three taps set it.
- **KEY SCAN**: the cells of an encoder's lines are rings, not flagged; under the matrix a line names them ("PA5:9, PA5:10:
  KNOB 1"), then "DOTTED CELL + A KEY: A 2ND CONTACT?" and the seen-closed word.

**VALUES** (V0.67): every module's last zone (OSC A and B, SUB, NOISE, FILTER, ENV 1 and 2, LFO 1 and 2, VOICE, each
FX module): its pages' knobs again, a page each in the same order ("VALUES 1/5" is WAVE's knobs, "2/5" UNISON's, ..),
each knob a row: its name (10 px bold), its value large (18 px bold, HOT while it turns), its place on its range (a
bipolar one's bar from the middle), the range its modulation takes it, and the source's word ("ENV 2 +45.0%"). A knob
on VALUES steps the parameter's finest unit (0.1 % of its range) a detent at any speed; lists still step one entry.
ALGORITHM reaches VALUES, always a module's last page.

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
- `web/emu/build_strata.sh`, then `node web/emu/strata_emu_test.mjs` (159 checks) and
  `node web/emu/strata_ui_shots.mjs` (every screen as PNG, named after the design's boards, in
  `build/emu_strata/ui_shots`; each checked as it is taken).
- **The screens' check** (`web_lint`, `web/emu/strata_web.c`): the screen drawn twice into buffers of its own, with
  its text and without; every pixel the text changed is checked against what lies under it and its neighbours (only
  the ground, the text's own ground or a blend of the two may be there), the texts' ink boxes against each other, a
  text past the screen's edges, a text in MID or LINE, and two knobs' handles on one place. The emulator's tests run
  it on all 180 pages; the shots script on its 152 screens (findings in `lint.txt`, marked PNGs in `lint/`);
  `node web/emu/strata_knob_sweep.mjs` on every knob of every page as it turns (639 turns).
- `node web/emu/strata_bloom_study.mjs`: the bloom over time (a note's swirl, a second note, the release, a chord) as
  one sheet, `build/emu_strata/bloom_study.png`.
- `cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_bench web/emu/strata_bench.c -lm`, then
  `build/emu/strata_bench`: the UI's drawing time in six sessions of the panel.
