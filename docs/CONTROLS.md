# Strata controls

Everything the panel does in Strata, the wavetable firmware for the M-VAVE FM-1. Status: the end of Phase 4 (the
user interface), **verified in emulator**; nothing has run on an FM-1 yet. How it is built: [ARCHITECTURE.md](ARCHITECTURE.md)
§17. How it is tested: [TESTING.md](TESTING.md) §6.3.

## The panel

| Control | What it does |
| --- | --- |
| 27 keys, F3 to G5 | play notes; in some layers and screens they pick, toggle, jump or type (below) |
| OCT− / OCT+ | the keys' octave, −3 to +3 (the header of SCL's page shows it) |
| KNOB 1–4 | the four values on the screen, left to right |
| SELECT | the next or previous page of the family shown |
| ALGORITHM | the next or previous section of the family shown (FX: the next effect in the chain) |
| PRESETS | the next or previous preset: the starter sounds, INIT, then the user's |
| MASTER | the output level |
| HOME, EDIT, ENV, LFO, SEQ, FX, GLO, SCL | the families of pages; held, their layers |
| SAVE | save, the browser, undo and redo |
| REC | held: the MOD layer |
| PLAY | the transport |
| ARP | the arpeggiator, in a later version |

The screen has five parts: the header (the section, the page and its number, and on the right what the page needs: the
oscillator's pitch, an envelope's or LFO's tabs, the effects that are on, the slots in use, the keys' octave), the
display (the graph), four knob cells (a dial or a list; the one turning is lit), and the footer (the modulation sources
in use on this page, or a word for a moment).

## The pages

A family button's tap shows its pages: the first tap the family's page shown last, the next taps step through its pages
(EDIT: through the pages of the section shown). ALGORITHM picks the section, SELECT steps the pages.

| Button | Section | Page: KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- |
| HOME | HOME | the sound (its category, number and name), its output live: MACRO 1 · 2 · 3 · 4 |
| EDIT | OSC A, OSC B | WAVE: WT POS · TABLE · WARP · AMOUNT; UNISON: UNISON · DETUNE · BLEND · WIDTH; SPREAD: UNI MODE · WT SPRD · PHASE · RAND; PITCH: OCT · SEMI · FINE · PH MODE; OUTPUT: LEVEL · PAN · ROUTE · ON |
| | SUB | MAIN: SHAPE · OCT · LEVEL · WIDTH; OUTPUT: PAN · ROUTE · ON |
| | NOISE | MAIN: TYPE · COLOR · STEREO · LEVEL; OUTPUT: PAN · ROUTE · ON |
| | FILTER | TONE: TYPE · CUTOFF · RESO · DRIVE; MORE: KEYTRACK · VAR · MIX · ON |
| ENV | ENV 1, ENV 2 | ADSR: ATTACK · DECAY · SUSTAIN · RELEASE; SHAPE: HOLD · A CURVE · D CURVE · R CURVE |
| LFO | LFO 1, LFO 2 | POINTS: POINT · TIME · LEVEL · CURVE; SHAPE: SHAPE · SYNC · RATE · MODE; TIME: DELAY · RISE · PHASE · SMOOTH |
| SEQ | MATRIX | SLOTS: SLOT · SOURCE · DEST · AMOUNT |
| FX | DRIVE | DRIVE: TYPE · DRIVE · TONE · MIX; POWER: ON |
| | CHORUS | CHORUS: RATE · DEPTH · DELAY · MIX; MORE: FEEDBACK · ON |
| | DELAY | DELAY: MODE · SYNC · TIME · FEEDBACK; TONE: LP · HP · MIX · ON |
| | REVERB | REVERB: TYPE · SIZE · DECAY · MIX; TONE: DAMP · PREDELAY · WIDTH · LOW CUT; POWER: ON |
| | OTT | OTT: DEPTH · TIME · UPWARD · DOWNWARD; BANDS: LOW · MID · HIGH · ON |
| | EQ | POINTS: POINT · FREQ · GAIN · Q; GAINS: LOW · MID · HIGH · ON |
| GLO | VOICE | VOICE: MODE · VOICES · GLIDE · GL MODE; PLAY: BEND UP · BEND DN · VEL AMP · VOLUME |
| | GLOBAL | TEMPO: BPM · TS-BEAT · TS-NOTE |
| SCL | KEYS | SCALE: SCALE · ROOT · FIT · VELOCITY |
| HOME held 5 s | SETTINGS | SCREEN: COLOR · HELP · GLOW; PANEL: ACCEL · HOLD; MIDI: CHANNEL · LEARN · CLEAR · PROGRAM; BENCH: RUN · PEAKS |

Some pages pick before they set: the LFO's POINTS (KNOB 1 the point), the EQ's POINTS (KNOB 1 LOW, MID or HIGH; KNOB
2–4 that point's frequency, gain and Q) and the MATRIX (KNOB 1 the slot, 1–8; KNOB 2–4 its source, destination and
amount; a destination steps only through what can be modulated). On the FX pages the six effects are in their chain
order. With SYNC FIXED, an LFO's RATE and the delay's TIME are note values, one a detent.

## Knobs

A knob is speed-sensitive: turned slowly a value moves 0.1 % of its range a detent, turned quickly up to 5 % (SETTINGS >
PANEL > ACCEL OFF: 1 % a detent at any speed). A list, a count or a short range moves one entry a detent. The knob
turning is lit; on the filter's and the effects' pages the part of the graph it moves is lit, its value in a flag, and
the shape before the turn is dashed behind, until a second after the last detent.

## Buttons: taps, holds and chords

A hold is a button held for the HOLD time (SETTINGS > PANEL, 400 ms by default); a double tap is two taps within 300 ms.

| Button | Tap | Twice | Held | Held 5 s | With another control |
| --- | --- | --- | --- | --- | --- |
| HOME | HOME | | | the SETTINGS (a countdown from the hold) | held at power-on: the last sound is not brought back (after a crash neither: SAFE START) |
| EDIT | its pages | | JUMP | | + OCT−/OCT+: the section's pages |
| ENV | its pages | the MOD layer for ENV 2, latched | the MOD layer for ENV 2 | | + OCT: the section's pages |
| LFO | its pages | on POINTS: a point added | MOMENTARY (on POINTS: deletes the point picked) | on POINTS: the point picked deleted (a countdown) | + OCT: the section's pages |
| SEQ | the MATRIX | | | | + OCT: the section's pages |
| FX | its pages | | MACROS | | + ALGORITHM: the effect shown moves along the chain; + OCT: the section's pages |
| GLO | its pages | | PERFORM | | + OCT: the section's pages |
| SCL | the KEYS page | | a word | | + a key: that note is the root |
| REC | | the MOD layer, latched (a tap lets it go) | the MOD layer | | + OCT: the section's pages |
| SAVE | a new version, saved at once | the browser (in it: back) | | saved over its own slot (a countdown from the hold) | + OCT−: undo; + OCT+: redo |
| PLAY | the transport on, off | | | | |
| ARP | "in a later version" | | | | |
| OCT− / OCT+ | the keys' octave | | | | with a family button or REC held: that section's pages; on NAME IT: OCT+ letters / symbols, OCT− deletes; while MIDI LEARN listens: OCT− takes the picked parameter's CC away, OCT+ ends LEARN; **both held 5 s: UBOOT**, the chip's USB boot (a countdown in the footer from 2 s; letting go stops it; [FLASHING.md](FLASHING.md)) |

## The layers

A layer lasts while its button is held (the MOD layer can be latched by a double tap). A banner names it, the footer
says how it ends, and its button breathes (a held MOD layer's is lit).

| Layer | Its button | The knobs | The keys | Let go |
| --- | --- | --- | --- | --- |
| MOD | REC (ENV: for ENV 2) | the source's depth on each knob's parameter (a matrix slot, made when needed; one at 0 goes when the layer closes) | a white key picks the source (REC's layer; ENV's keys play) | it closes |
| PERFORM | GLO | 1 pitch bend (16 detents from the middle to an end), 2 mod wheel, 3 GLIDE, 4 VOLUME | play | the bend goes back to the middle; the wheel stays |
| MACROS | FX | MACRO 1–4 | F3 DRIVE, G3 CHORUS, A3 DELAY, B3 REVERB, C4 OTT, D4 EQ: on and off (lit when on) | it closes |
| MOMENTARY | LFO (not on the POINTS page) | change the page's values for now (16 at most) | play | every value it changed goes back |
| JUMP | EDIT | nothing | a key opens its section: the white keys OSC A, OSC B, SUB, NOISE, FILTER, ENV 1, ENV 2, LFO 1, LFO 2, MATRIX, DRIVE, CHORUS, DELAY, REVERB, OTT, EQ; F#3 HOME, G#3 VOICE, A#3 GLOBAL, C#4 KEYS, D#4 SETTINGS | the page it opened |

The MOD layer's sources on the white keys: F3 ENV 1, G3 ENV 2, A3 LFO 1, B3 LFO 2, C4 to F4 MACRO 1 to 4, G4 VELOCITY,
A4 NOTE, B4 RANDOM, C5 ALTERNATE, D5 MOD WHEEL, E5 BEND, F5 AFTERTOUCH, G5 EXPRESSION.

## SAVE, presets, undo

- **SAVE once**: the sound is saved at once as a new preset, its next version: "WARM PAD v2" (one above the highest
  version of that name in a user slot; a starter sound's first is "SUPERSAW v2"), in the first free slot of 128, its
  category kept. The footer says where: "SAVED: WARM PAD v2 · USER 3". The single tap waits 300 ms for a second one.
- **SAVE held 5 s**: the sound is saved over its own slot after a countdown ("HOLD: SAVE OVER USER 3 IN 4"); letting go,
  OCT, a knob or another button stop it. A sound that is not a user preset has no slot to save over.
- **SAVE twice: the browser.** KNOB 1 the category (ALL, BASS, LEAD, PAD, PLUCK, KEYS, FX, SEQ, OTHER, with their
  counts), KNOB 2 the preset (loaded as it turns), KNOB 3 the bank (ALL, STARTER, USER). SAVE twice or HOME: back.
- **SAVE in the browser: NAME IT** for the sound playing. The keys type: F3 to F#5 A to Z, G5 a space; OCT+ the numbers
  and symbols, OCT− deletes. KNOB 1 the cursor, KNOB 2 the character there (at the end: ADD), KNOB 3 the category, KNOB
  4 the slot: its own (a rename), or for a starter sound the first free one (a slot in use says what it would replace).
  16 characters. SAVE writes it and goes back to the browser, HOME goes back without.
- **Undo, redo**: SAVE held + OCT− steps back, + OCT+ forward again, through the last 8 states: each knob gesture (a turn
  after a second without one), each MOD-layer edit, LFO point, effect switch and preset load. An undone load makes the
  sound the preset it was again.
- The sound playing, the user presets and the settings are kept: the sound and the settings once nothing has sounded for
  3 s, a preset when it is saved. At power-on they come back (HOME held at power-on: the sound does not).

## The keys

- No velocity on the FM-1's keys: they play at SCL's VELOCITY (100 by default). MIDI notes keep their own.
- **SCL**: SCALE (chromatic, major, minor, dorian, mixolydian, the pentatonics, harmonic and melodic minor, phrygian,
  lydian, locrian, blues, whole tone, the two diminished), ROOT, FIT: OFF (each key its own note), SNAP (a key off the
  scale plays the scale's note below it), WHITE (the white keys walk the scale from the root at C4; the black keys are
  silent). SCL held + a key: that key's note is the root. MIDI notes are not fitted.
- What else the keys do: in REC's MOD layer they pick sources, in MACROS they switch effects, in JUMP they open sections,
  on NAME IT they type, with SCL held they set the root.

## LEDs

| LED | Lit | Glowing | Mid / breathing |
| --- | --- | --- | --- |
| A key | held; the MOD layer's source; MACROS: an effect that is on; JUMP: the section shown | a scale's notes (a scale other than chromatic); the sources in use (MOD layer); an effect that is off (MACROS); a section's key (JUMP) | mid: the scale's root |
| A family button | the family shown | the others | |
| REC | in its layer | otherwise | |
| A layer's button | REC or ENV while their MOD layer is held | | breathing: GLO, FX, LFO or EDIT while their layer is open, and REC or ENV while their MOD layer is latched |
| PLAY | a quarter of each beat while the transport runs | | its green LED: lit while it runs |

SETTINGS > SCREEN > GLOW sets how bright the glow is (HIGH or LOW).

## MIDI

- One channel or any (SETTINGS > MIDI > CHANNEL). Notes with velocity, pitch bend, channel aftertouch (the AT source),
  CC 1 the mod wheel, 11 expression, 64 sustain, 120 and 123 everything off.
- Program change (SETTINGS > MIDI > PROGRAM ON): a preset from the bank CC 0 last chose: 0 the starter sounds (0 SUPERSAW
  … 7, 8 INIT), anything else the user slots (program 0 USER 1).
- The controllers' default parameters:

| CC | Parameter | CC | Parameter |
| --- | --- | --- | --- |
| 5 | GLIDE | 21 | OSC A AMOUNT (warp) |
| 7 | VOLUME | 22 | OSC B AMOUNT |
| 16–19 | MACRO 1–4 | 70 | OSC A WT POS |
| 20 | OSC B WT POS | 71 | FILTER RESO |
| 74 | FILTER CUTOFF | 91, 93, 94 | REVERB, CHORUS, DELAY MIX |

- **MIDI LEARN** (SETTINGS > MIDI > LEARN ON): turn a knob on any page (its parameter is picked; the footer names it),
  move a controller: that CC sets the parameter from then on, in place of what either had. OCT− takes the picked
  parameter's CC away, OCT+ (or LEARN OFF) ends it. 32 learned CCs at most, kept with the settings; a learned CC comes
  before a parameter's own. CLEAR (turned twice within 2 s) forgets them all. CC 0, 32, 64 and 120–127 are never
  learned.
- Not yet: MIDI clock, start and stop, and MIDI out (they come with integration).

## SETTINGS (HOME held 5 s)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| SCREEN | COLOR: STRATA (default), CLASSIC, ICE, VIOLET, PAPER, NIGHT, HI-CON, MONO | HELP: a section's hint in the footer as it opens (on by default) | GLOW: the LEDs' glow, HIGH or LOW | |
| PANEL | ACCEL: the knobs' speed sensitivity | HOLD: 300, 400, 500 or 600 ms before a held button's layer | | |
| MIDI | CHANNEL: ANY or 1–16 | LEARN | CLEAR: the learned CCs | PROGRAM: program changes taken |
| BENCH | RUN: two loops timed, a moment's silence (on the FM-1) | PEAKS: CLEAR | | |

BENCH shows what the FM-1 measures, a row each: AUDIO, CPU, SCREEN, KEYS, TABLES, FLASH, STACKS, RUN (what each
means: [TESTING.md](TESTING.md) §8.3; how to use it in the hardware test: [FLASHING.md](FLASHING.md) §4). The
emulator says it has no clock of its own.

HOME leaves the SETTINGS. GLOBAL's TEMPO (BPM 20–300, TS-BEAT 1–16, TS-NOTE 1, 2, 4, 8, 16) and SCL's page are kept
with the settings too, not with a sound.
