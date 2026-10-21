# Strata controls

Everything the panel does in Strata, the wavetable firmware for the M-VAVE FM-1. Status: V0.8, the sequencer (below),
on V0.75's ARP and one grammar for every menu button, V0.7's chords (reworked in V0.75: knobs only, STRUM), V0.67's
legible screens and V0.6's FX chain, **verified in emulator**; V0.5 ran on an FM-1 on 2026-10-17, V0.6 and V0.65
since, V0.7, V0.75 and V0.8 **need a hardware test**. How it is built:
[ARCHITECTURE.md](ARCHITECTURE.md) §14, §17. How it is tested: [TESTING.md](TESTING.md) §6.3.

## The panel

| Control | What it does |
| --- | --- |
| 27 keys, F3 to G5 | play notes; in some layers and screens they pick, toggle, jump or type (below) |
| OCT− / OCT+ | the keys' octave, −3 to +3 (the top line of SCL's page shows it) |
| KNOB 1–4 | the four values on the screen, left to right |
| SELECT | the next or previous section of the menu shown (OSC A, OSC B, SUB, NOISE, FILTER; an effect module; ..); in a menu of one section, its pages |
| ALGORITHM | the next or previous page of the section shown (everywhere, the FX chain view too) |
| PRESETS | the next or previous preset: the starter sounds, INIT, then the user's |
| MASTER | the output level |
| HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP | the menus: one grammar for all (Buttons, below) |
| SEQ | the sequencer's menu (V0.8, below); SEQ + OCT+ / OCT− the next pattern, the one before; held: PATTERNS |
| SAVE | save, the browser, undo and redo |
| REC | a tap: the sequencer armed to record, or not (V0.8); held: the MOD layer |
| PLAY | the transport: the sequencer plays (V0.8); ARP and PATTERN step on its beat (V0.75) |
| ARP | the ARP's menu; ARP + OCT+ the ARP on, ARP + OCT− off, from any page (V0.75, below) |

The screen (the STRATA UI: [UI.md](UI.md)) is one picture a module: a top line (the module's name and its dot, the
family's module dots or the chain's squares, a word at the right), the body (what the module does, with the four
knobs' numbered handles on what each moves; the knob turning: its handle vermilion, the value it sets vermilion
where the page shows it, the shape before the turn dashed), and a bottom row (the section's pages as zones,
ALGORITHM's; a word for a moment; V0.75: the knob turning's name and value, where the page does not show it, until a
second after its last detent: no flag over the page).

**VALUES** (V0.67): every module (OSC A and B, SUB, NOISE, FILTER, ENV 1 and 2, LFO 1 and 2, VOICE, each FX module)
has one more zone at the end of its pages, VALUES: its pages' knobs again, a page each in the same order ("VALUES
1/5" holds WAVE's four, "2/5" UNISON's, ..), each value large with its bar, the range its modulation takes it and the
source. A knob there moves the parameter's finest step a detent (0.1 % of its range; a list one entry) however fast it
turns: for an exact value. ALGORITHM and OCT− / OCT+ with the family button held reach the VALUES pages; a family
button's tap steps the drawings only.

## The pages

A menu button's tap shows its menu: the first tap the page shown last, the next taps its next page, on through its
sections (V0.75: EDIT and FX too); twice, its next section. SELECT picks the section, ALGORITHM steps its pages, in
every menu.

| Button | Section | Page: KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- |
| HOME | HOME | the sound (its category, number and name), its output live: MACRO 1 · 2 · 3 · 4 |
| EDIT | OSC A, OSC B | WAVE: WT POS · TABLE · WARP · AMOUNT; UNISON: UNISON · DETUNE · BLEND · WIDTH; SPREAD: UNI MODE · WT SPRD · PHASE · RAND; PITCH: OCT · SEMI · FINE · PH MODE; OUTPUT: LEVEL · PAN · ROUTE |
| | SUB | MAIN: SHAPE · OCT · LEVEL · WIDTH; OUTPUT: PAN · ROUTE |
| | NOISE | MAIN: TYPE · COLOR · STEREO · LEVEL; OUTPUT: PAN · ROUTE |
| | FILTER | TONE: TYPE · CUTOFF · RESO · DRIVE; MORE: KEYTRACK · VAR · MIX |
| ENV | ENV 1, ENV 2 | ADSR: ATTACK · DECAY · SUSTAIN · RELEASE; SHAPE: HOLD · A CURVE · D CURVE · R CURVE |
| LFO | LFO 1, LFO 2 | POINTS: POINT · TIME · LEVEL · CURVE; SHAPE: SHAPE · SYNC · RATE · MODE; TIME: DELAY · RISE · PHASE · SMOOTH |
| FX | FX CHAIN | CHAIN: ADD (the type) · SLOT · MOVE · MIX (below: the FX chain) |
| | the chain's modules | each its pages (below) |
| GLO | VOICE | VOICE: MODE · VOICES · GLIDE · GL MODE; PLAY: BEND UP · BEND DN · VEL AMP · VOLUME |
| | MATRIX | SLOTS: SLOT · SOURCE · DEST · AMOUNT |
| | GLOBAL | TEMPO: BPM · TS-BEAT · TS-NOTE · CLOCK IN (V0.75) |
| SCL | SCALE | SCALE: SCALE · ROOT · FIT · VELOCITY |
| | CHORD | CHORD: MODE · TYPE (KEY: SHAPE) · VOICING · SPREAD; STRUM: STRUM · DIRECTION · HUMAN · TAPER; BASS: BASS · OCTAVE · LEVEL (Chords, below) |
| | MIDI | MIDI: MIDI IN · USB OUT · CHORDS CH · BASS CH |
| ARP | ARP | STYLE: STYLE · RATE · MOVE · FEEL; GROOVE: GATE · SWING · LENGTH · LATCH (The ARP, below) |
| | STEPS | STEPS: STEP · KIND · LEVEL · OCTAVE; RATCHET: STEP · RATCHET · CHANCE · SLIDE |
| SEQ | SEQ | NOTES: STEP · NOTE · VELOCITY · LENGTH; FEEL: STEP · CHANCE · NUDGE · OCTAVE; MOTION: STEP · LANE · VALUE · PARAMETER (The sequencer, below) |
| | PATTERN | PATTERN: PATTERN · LENGTH · RATE · SWING; PLAY+REC: TRANSPOSE · DIRECTION · RECORD · QUANTIZE |
| | CHAIN | CHAIN: SLOT · PATTERN · REPEATS · CHAIN |
| HOME held 5 s | SETTINGS | SCREEN: COLOR · HELP · GLOW; PANEL: ACCEL · HOLD; MIDI: CHANNEL · LEARN · CLEAR · PROGRAM; BENCH: RUN · PEAKS; CORE 2: ON · TEST · LOAD |

Some pages pick before they set: the LFO's POINTS (KNOB 1 the point), the EQ's POINTS (KNOB 1 LOW, MID or HIGH; KNOB
2–4 that point's frequency, gain and Q) and the MATRIX (KNOB 1 the slot, 1–8; KNOB 2–4 its source, destination and
amount; a destination steps only through what can be modulated, an effect's only while its module is in the chain).
With SYNC FIXED, an LFO's RATE, the delay's TIME and the flanger's, phaser's and tremolo's RATE are note values, one a
detent. There are no ON knobs: EDIT + OCT+ / OCT− turns the source or the filter shown on or off, and the FX-held keys
bypass a module (the dot before its name shows it).

## The FX chain

The effects are modules in a chain of up to eight, in the order they play, the limiter fixed after the last. FX shows
the **chain view**, laid out as the preset browser: the 13 types at the left, the chain at the right.

| Control | On the chain view |
| --- | --- |
| KNOB 1 or ALGORITHM | the type to add (each type's instances in the chain shown: 1/2, IN) |
| KNOB 2 or SELECT | a slot of the chain; past the last module, the place to add one (+ HERE) |
| KNOB 3 | MOVE: the module picked one place along the chain a detent |
| KNOB 4 | the module picked's MIX (OTT and TREMOLO: DEPTH; EQ: none) |
| FX + OCT+ | the type picked added below the slot picked (on + HERE: there), from its defaults: "ADDED: PHASER · 3 OF 8" |
| FX + OCT− | the module picked taken out (the matrix slots onto it with it) |
| FX twice | the module picked opened: its pages; SELECT steps the chain's modules, ALGORITHM a module's pages; FX twice there: back to the chain view, that module picked |

Each type plays twice at most (DRIVE 2, EQ 2, ..), DELAY and REVERB once: "PHASER: TWO AT MOST", "DELAY: ONE AT
MOST"; "THE CHAIN IS FULL: 8 MODULES". Undo (SAVE + OCT−) takes back each add, take-out, move and bypass. The top
line shows the chain's modules of 8 and their cost on one core (the cost model's estimate: REVERB ~13 %, the others 1–6 %).

| Module | Pages: KNOB 1 · 2 · 3 · 4 |
| --- | --- |
| DRIVE | DRIVE: TYPE · DRIVE · TONE · MIX |
| CHORUS | CHORUS: RATE · DEPTH · DELAY · MIX; MORE: FEEDBACK |
| DELAY | DELAY: MODE · SYNC · TIME · FEEDBACK; TONE: LP · HP · MIX |
| REVERB | REVERB: TYPE · SIZE · DECAY · MIX; TONE: DAMP · PREDELAY · WIDTH · LOW CUT |
| OTT | OTT: DEPTH · TIME · UPWARD · DOWNWARD; BANDS: LOW · MID · HIGH |
| EQ | POINTS: POINT · FREQ · GAIN · Q; GAINS: LOW · MID · HIGH |
| FLANGER | FLANGER: RATE · DEPTH · DELAY · MIX; MORE: FEEDBACK · SPREAD · SYNC |
| PHASER | PHASER: RATE · DEPTH · CENTER · MIX; MORE: FEEDBACK · STAGES · SPREAD · SYNC |
| FILTER | FILTER: TYPE · CUTOFF · RESO · MIX; MORE: DRIVE |
| BITCRUSH | CRUSH: BITS · RATE · MIX |
| TREMOLO | TREMOLO: MODE (TREMOLO, AUTOPAN) · SHAPE · RATE · DEPTH; MORE: SYNC |
| RING MOD | RING MOD: FREQ · SHAPE · SPREAD · MIX |
| TAPE | TAPE: WOW · FLUTTER · SATURATE · MIX; TONE: TONE |

The FX filter's types: LP 6, LP 12, LP 24, HP 6, HP 12, HP 24, BP 12, BP 24, NOTCH, PEAK, LADDER, DIRTY LP (the
resonance driven), FORMANT (CUTOFF walks the vowels), COMB + and COMB − (metallic; CUTOFF tunes them), LO SHELF, HI
SHELF and TILT (RESO a gain, −18 to +18 dB), DJ (one knob: CUTOFF under the middle a low pass, over it a high pass, open
at the middle), LO-FI (the rate held at CUTOFF, RESO the bits, then a low pass) and RING (times a carrier at CUTOFF,
RESO from sine to square). A new TYPE fades the module out and back in. Presets from before the chain load with
their effects that were on as the chain, in the order they played.

## Knobs

A knob is speed-sensitive: turned slowly a value moves 0.1 % of its range a detent, turned quickly up to 5 % (SETTINGS >
PANEL > ACCEL OFF: 1 % a detent at any speed). A list, a count or a short range moves one entry a detent. The knob
turning is lit; on the filter's and the effects' pages the part of the graph it moves is lit, and the shape before the
turn is dashed behind, until a second after the last detent. Its value (V0.75): where the page shows it (a chip, a
figure, a title), there, vermilion; else in the bottom row, its name and value. No flag covers the page.

## Buttons: taps, holds and chords

A hold is a button held for the HOLD time (SETTINGS > PANEL, 400 ms by default); a double tap is two taps within 300 ms.

**One grammar for the menu buttons** (V0.75: HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP; V0.8, SEQ: its taps, its hold
and its 5 s the same, its + OCT+ / + OCT− the patterns; SAVE keeps its own):

- **Tap**: open the menu where it was left; tap again: its next page, on through its sections.
- **Twice**: its next section (FX: the chain view <-> the module picked; HOME: the presets).
- **Held**: its layer while held (The layers, below).
- **Held 5 s**: what it shows back to its starting values: a countdown from 2 s, if nothing was turned or pressed
  meanwhile; a sound's section can be undone (SAVE + OCT−); SCL's and the ARP's settings cannot. HOME: the SETTINGS.
- **+ OCT+ / + OCT−**: on / off (add / take out, next / back) for what the menu is about, from any page (the page stays,
  LFO's: its POINTS).

| Button | Tap | Twice | Held | Held 5 s | + OCT+ | + OCT− |
| --- | --- | --- | --- | --- | --- | --- |
| HOME | HOME | the presets (a tap: back) | | the SETTINGS (a countdown from the hold) | the next sound | the sound before |
| EDIT | its pages | the next section | JUMP | the section shown reset | the source or filter shown on | off |
| ENV | its pages | ENV 1 <-> ENV 2 | the MOD layer for ENV 2 | the envelope shown reset | that MOD layer latched | let go |
| LFO | its pages | LFO 1 <-> LFO 2 | MOMENTARY | the LFO shown reset (its points too) | a point added (left of the one picked; halfway) | the point picked deleted (not the ends) |
| FX | its pages (the chain view: none) | the chain view <-> the module picked | MACROS | the module shown reset (the chain view: the chain as INIT's) | the chain view: a module added; a module's page: it in | taken out; bypassed |
| GLO | its pages | the next section | PERFORM | VOICE, the MATRIX (cleared) or GLOBAL (120 BPM, 4/4, INTERNAL) reset | PERFORM latched (a GLO tap lets it go) | let go |
| SCL | its pages | the next section | a key is the scale's root | SCALE, the chords or their MIDI reset | the chords on (MODE as it was: KEY at first) | off |
| ARP | its pages | ARP <-> STEPS | | its settings or its steps reset | the ARP on | off |
| SAVE | a new version, saved at once | the browser (in it: back) | | saved over its own slot (a countdown from the hold) | redo | undo |
| SEQ | its pages | the next section | PATTERNS | the section shown reset (SEQ: the pattern's steps cleared, PASTE puts them back; PATTERN: its settings; CHAIN: cleared) | the next pattern (playing: at the end of this one) | the pattern before |
| REC | the sequencer armed, or not (a latched MOD layer: let go) | | the MOD layer | | the MOD layer latched | let go |
| PLAY | the transport on, off (the sequencer from its start; the arpeggio on its beat) | | | | | |
| OCT− / OCT+ | the keys' octave (the browser: OCT−'s at its release) | | the browser: OCT− 3 s deletes the user preset playing | | | |

Also: HOME held at power-on: the last sound is not brought back (after a crash neither: SAFE START). On NAME IT,
OCT+ letters / symbols, OCT− deletes; while MIDI LEARN listens, OCT− takes the picked parameter's CC away and OCT+
ends LEARN. **OCT− and OCT+ both held 5 s: UBOOT**, the chip's USB boot (a countdown in the footer from 2 s; letting go
stops it; [FLASHING.md](FLASHING.md)).

## The layers

A layer lasts while its button is held (REC's and ENV's MOD layer and GLO's PERFORM can be latched: the button + OCT+;
a tap or the button + OCT− lets it go; V0.8: REC twice no longer latches, a REC tap arms the sequencer). A banner
names it and says how it ends, and its button breathes (a held MOD layer's is lit).

| Layer | Its button | The knobs | The keys | Let go |
| --- | --- | --- | --- | --- |
| MOD | REC (ENV: for ENV 2) | the source's depth on each knob's parameter (a matrix slot, made when needed; one at 0 goes when the layer closes) | a white key picks the source (REC's layer; ENV's keys play) | it closes |
| PERFORM | GLO | 1 pitch bend (16 detents from the middle to an end), 2 mod wheel, 3 GLIDE, 4 VOLUME | play | the bend goes back to the middle; the wheel stays |
| MACROS | FX | MACRO 1–4 | F3 to F4 (the white keys): the chain's modules 1 to 8 bypassed or back in (lit when in, glowing when bypassed) | it closes |
| MOMENTARY | LFO | change the page's values for now (16 at most) | play | every value it changed goes back |
| PATTERNS | SEQ (V0.8) | nothing | a white key picks pattern 1 to 16 (playing: next, at the end of the one playing); F#3 COPY, G#3 PASTE, A#3 CLEAR (the pattern shown; CLEAR copies it first, so PASTE undoes it) | it closes (a key used: no reset at 5 s) |
| JUMP | EDIT | nothing | a key opens its section: the white keys OSC A, OSC B, SUB, NOISE, FILTER, ENV 1, ENV 2, LFO 1, LFO 2, MATRIX, then the chain's modules 1 to 6; F#3 HOME, G#3 VOICE, A#3 GLOBAL, C#4 SCALE, D#4 SETTINGS, A#4 the FX chain view | the page it opened |

The MOD layer's sources on the white keys: F3 ENV 1, G3 ENV 2, A3 LFO 1, B3 LFO 2, C4 to F4 MACRO 1 to 4, G4 VELOCITY,
A4 NOTE, B4 RANDOM, C5 ALTERNATE, D5 MOD WHEEL, E5 BEND, F5 AFTERTOUCH, G5 EXPRESSION.

## SAVE, presets, undo

- **SAVE once**: the sound is saved at once as a new preset, its next version: "WARM PAD v2" (one above the highest
  version of that name in a user slot; a starter sound's first is "SUPERSAW v2"), in the first free slot of 128, its
  category kept. A word at the bottom says where: "SAVED: WARM PAD v2 · USER 3". The single tap waits 300 ms for a
  second one.
- **SAVE held 5 s**: the sound is saved over its own slot after a countdown (a ring that empties: "SAVE OVER USER 3");
  letting go, OCT, a knob or another button stop it. A sound that is not a user preset has no slot to save over.
- **SAVE twice: the browser.** KNOB 1 the category (ALL, BASS, LEAD, PAD, PLUCK, KEYS, FX, SEQ, OTHER, with their
  counts), KNOB 2 the preset (loaded as it turns), KNOB 3 the bank (ALL, STARTER, USER). SAVE twice or HOME: back.
- **OCT− held 3 s in the browser: the user preset playing deleted** after a countdown ("HOLD: DELETE USER 3 IN 2";
  letting go stops it); the sound plays on, from no slot. **SAVE + OCT− straight after puts it back** ("BACK IN USER
  3"; its slot taken meanwhile: the first free one). The starter sounds stay. OCT− tapped there: the octave.
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
- What else the keys do: in REC's MOD layer they pick sources, in MACROS they bypass modules, in JUMP they open sections,
  on NAME IT they type, with SCL held they set the root; with the chords on they play chords (below).

## Chords (V0.7; V0.75: knobs only, STRUM)

SCL's CHORD and MIDI sections make the keys a chord instrument, after the Orchid's ideas (our own design and code).
Only the knobs set them: no key is a button. With MODE OFF, the ARP off and BASS off the keys play as before. SCL +
OCT+ turns the chords on (MODE as it was, KEY at first), SCL + OCT− off, from any page.

- **CHORD**: MODE · TYPE · VOICING · SPREAD. A row a knob on the page, the current choice lit, vermilion while its knob
  turns.
  - **MODE**: OFF, KEY, FREE.
  - **KEY**: every key plays its in-key chord: the scale and root SCALE sets, stacked from the key (a key off the scale
    moves down onto it). KNOB 2 is its SHAPE: TRIAD, 7TH, 9TH, SUS2, SUS4, 6TH, ADD9, POWER (the root, its fifth, the
    octave).
  - **FREE**: every key plays TYPE's chord on it. TYPE: MAJ, MIN, SUS2, SUS4, DIM, AUG, POWER, 6, MIN6, 7, MAJ7, MIN7,
    MIN7b5, DIM7, 7SUS4, ADD9, MIN ADD9, 9, MAJ9, MIN9, 6/9, MIN11, 13 (the page shows a strip of them from the one
    before the current, its place in the list at the right). FREE's TYPE and KEY's SHAPE are kept apart.
  - **VOICING**: AUTO (each chord in the inversion nearest the one before: smooth voice leading), or −3 to +6
    (inversions up, the top notes down).
  - **SPREAD**: CLOSE (as built), OPEN (the second note from the top an octave down), WIDE (the lowest an octave down,
    the highest one up), 2 OCT (the chord again an octave up, 8 notes at most).
  - The chord playing is named on SCL's pages ("Cm7/G": the note after the slash is the lowest), its notes and the bass
    shown on keys. One chord at a time: the last root pressed plays; let go of it, the one held before plays again.
- **STRUM**: STRUM · DIRECTION · HUMAN · TAPER: how a chord's notes start (the chords played, PATTERN's stabs).
  - **STRUM**: the gap between them, OFF (together) or 2 to 200 ms.
  - **DIRECTION**: UP (the lowest first), DOWN, ALTERNATE (up, then down, chord by chord), RANDOM.
  - **HUMAN**: 0 to 100 %: each note up to 12 ms late and its level up to 25 % either way, at random: a hand's.
  - **TAPER**: 0 to 100 %: the later notes softer, the last at half the level at 100 %.
  - The page draws the chord as it strums: a row a note, each from its start, as bright as its level.
- **BASS**: the chord's root one or two octaves under its lowest note, on the same sound (one of the 8 voices), at LEVEL
  (% of the chord's velocity).
- **MIDI**: MIDI IN AS SENT (default) or ROOTS (incoming notes play chords, as root keys; with HARP: its chord's
  root); USB OUT OFF (default) or ON: what the chords play goes out over USB MIDI, the chords on CHORDS CH (1), the bass
  on BASS CH (2).
- Kept with the settings, whatever sound is loaded. A change of MODE stops what plays (press again). The ARP on or off,
  or its style changed, plays what is held the new way at once (a chord held, then ARP + OCT+: arpeggiated; ARP +
  OCT−: held as a chord); to HARP, press again.

## The ARP (V0.75)

The ARP button's menu: tap ARP for it (STYLE, GROOVE; twice: STEPS, RATCHET). ARP + OCT+ turns it on, ARP + OCT− off,
from any page; hold a chord and ARP + OCT+: it arpeggiates what is held at once. It plays the chord CHORD makes (KEY,
FREE) or, with MODE OFF, the keys held (and MIDI notes with MIDI IN ROOTS); BASS goes on under it; USB OUT sends what
it plays.

- **STYLE**: STYLE · RATE · MOVE · FEEL, each style with its own three.
  - **ARP** (the default): a step arpeggiator. RATE · MODE · OCTAVES.
  - **PATTERN**: the chord as stabs on one of 16 rhythms of 16 steps (MOVE), STRUMmed as a chord is; FEEL the ACCENT,
    how much softer the stabs off the beat are (20 %); the stabs as long as GROOVE's GATE, swung by its SWING.
  - **HARP**: every key a string over four octaves: each plays the chord's note at or under its place, so a finger run
    along the keys plays the chord rising. The chord is CHORD's on SCALE's root (KNOB 2 changes it; a MIDI note in as
    a root, with MIDI IN ROOTS, moves it there); with MODE OFF the strings are the scale's notes.
  - **RATE**: a step, 5 to 120 ms or a note value of the tempo: 1/32, 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4,
    1/4D, 1/2, 1 BAR (1/16 by default).
  - **MODE**, over the notes low to high and across OCTAVES (C E G, one octave): UP (C E G), DOWN (G E C), UP-DOWN
    (C E G E), DOWN-UP (G E C E), UP & DOWN (the ends twice: C E G G E C), AS PLAYED (the order they were pressed),
    RANDOM, WALK (a step up or down at random), OUTSIDE IN (C G E), INSIDE OUT (E G C), PINKY (the top note between the
    others: C G E G), THUMB (the bottom one between: C E C G), CHORD (all of them each step, an octave up each time
    through OCTAVES).
  - **OCTAVES**: 1 to 4.
  - The page draws sixteen steps of what it will play: a row a note, ties long, ratchets split, the level as
    brightness, a CHANCE under 100 % hollow, SWING's steps late, LENGTH's end dotted; the step playing marked under it.
- **STEPS** and **RATCHET** (the STEPS section): KNOB 1 picks a step (1 to 16, its number the title); the other knobs
  set it. Sixteen steps, LENGTH of them play, round.
  - **KIND**: PLAY (a note), TIE (the note before held on through this step, no new note), REST (silence).
  - **LEVEL**: 5 to 100 % of the key's velocity. **OCTAVE**: −2 to +2 octaves on the step's note.
  - **RATCHET**: 1 to 4 hits in the step. **CHANCE**: 0 to 100 %, the step plays (a step left out keeps its note's
    place: the order goes on). **SLIDE**: the note held on into the next one (with VOICE MONO or LEGATO and GLIDE: a
    glide).
  - Only PLAY steps take the next note of MODE's order.
  - The screen: a column a step: OCTAVE a tick over the 0 line, LEVEL a bar (RATCHET's hits side by side, a CHANCE
    under 100 % hollow), a TIE the note before carried across, a REST a stub, SLIDE a slope into the next step; the
    steps past LENGTH dim; the step picked framed; the step playing marked over it. RATCHET's page: each step's CHANCE a
    meter under it.
- **GROOVE** (ARP and PATTERN): GATE · SWING · LENGTH · LATCH.
  - **GATE**: a note's length, 5 to 200 % of a step (50 %); over 100 % the notes overlap (legato).
  - **SWING**: 50 % (straight) to 75 %: the odd steps late, by half a step at 75 %.
  - **LENGTH**: ARP's steps, 1 to 16.
  - **LATCH**: what the keys held keeps playing after they are let go; the next press after all were let go starts
    anew (keys pressed while others are held join). LATCH off: what was let go stops. Every style but HARP; with the
    ARP off it holds the chord.
- **The beat**: PLAY on, ARP and PATTERN step on its beat, their pattern from the bar's start; a key's first note waits
  for the next step. PLAY off: they start at the key. With MODE OFF the first step waits 15 ms, so a chord's notes
  are all in it whichever key came first (AS PLAYED: at once).
- **CLOCK IN** (GLO > GLOBAL > TEMPO, KNOB 4): INTERNAL (default), or MIDI: MIDI's clock is the tempo (the BPM shown
  is it, "BPM: MIDI"; KNOB 1 says so), MIDI's start, continue and stop run PLAY (the first clock after a start is the
  downbeat). The clock gone a second: the panel's BPM again.
- The ARP LED: lit while the ARP is on (dark a moment on each beat while ARP or PATTERN plays), glowing while off.
- Kept with the settings, whatever sound is loaded.

## The sequencer (V0.8)

The SEQ button's menu: SEQ (its steps: NOTES, FEEL, MOTION), PATTERN (PATTERN, PLAY+REC), CHAIN. Sixteen patterns of
up to 64 steps, each step up to four notes. PLAY plays the pattern shown from its start, on the transport's beat (MIDI's
clock with CLOCK IN MIDI); PLAY again stops it. Its notes play as the keys' do: SCALE's chords, the ARP, STRUM, BASS and
USB OUT act on them (a step's note is a root with CHORD on; with the ARP on, the ARP plays what the steps hold).

- **The steps** (SEQ): KNOB 1 picks a step (its number the title); the pages show sixteen at a time, the ones holding
  the step picked (four boxes at the top right: which sixteen, the step playing under them).
  - **NOTES**: NOTE moves the step's notes together (an empty step: one, as the step before's; turned below C-1:
    the step emptied); VELOCITY (all its notes); LENGTH, 1/4 step to 64 steps (past the next step: held on, legato).
    The page is a piano roll: each note a bar from where NUDGE puts it, as long as LENGTH, as bright as its velocity;
    the Cs named at the left; LENGTH's end dashed.
  - **FEEL**: CHANCE, 0 to 100 % (the step plays, or not); NUDGE, −50 to +50 % of a step (early, late); OCTAVE, the
    step's notes an octave at a time. NUDGE's ticks round each step's middle, CHANCE's meters under them.
  - **MOTION**: two lanes a pattern, each a parameter of the sound (one that can be modulated: CUTOFF, WT POS, an
    effect's MIX ..). LANE picks one; PARAMETER its parameter; VALUE its value at the step picked (turned below the
    lowest: none there). At a step with a value the parameter is there; at one without, the knob's own. The sound
    itself is untouched: its knob, SAVE and undo keep its own value.
- **PATTERN**: PATTERN (1 to 16: picked while one plays, it waits for that one's end, "P1>3" at the top right);
  LENGTH, 1 to 64 steps; RATE, a step's note value (1/32 .. 1 BAR, 1/16 by default); SWING, 50 % (straight) to 75 %:
  the odd steps late, by half a step at 75 %. The page: the sixteen (the one shown filled, the one next red, those
  holding notes framed), LENGTH's cells (a step with notes bright), a bar of steps at RATE with SWING's.
- **PLAY+REC**: TRANSPOSE, −24 to +24 semitones; DIRECTION, FWD, BACK, PING (there and back, the ends once), RANDOM;
  RECORD, OVERDUB (notes added) or REPLACE (each step cleared as the playhead first passes it, while armed); QUANTIZE,
  OFF (NUDGE keeps where a note came) or ON (notes on the steps).
- **CHAIN**: up to sixteen slots, each a pattern (SLOT picks one, PATTERN sets it; END: the chain ends before it) and
  its REPEATS (1 to 16); CHAIN on: PLAY plays slot 1's pattern, its repeats, then slot 2's .., round again. A pattern
  picked while the chain plays comes next, once; the chain then goes on.
- **Recording** (REC arms it: its LED lit, "REC" at the top right):
  - **Live**: PLAY with REC armed: what the keys play (as SCALE fits them; MIDI notes in too) goes into the step
    nearest, as long as it was held; a note just before a step goes into it and is not played again as the playhead
    comes. A knob turned meanwhile (a parameter that can be modulated) takes a MOTION lane (the first free; both in
    use: "MOTION: BOTH LANES IN USE") and writes it at each step it passes while it turns; it is heard as turned.
  - **By step**: stopped, REC armed, SEQ's steps shown ("REC: THE KEYS FILL STEP 5"): the keys pressed together are
    the step picked's notes (what it held before goes), and the next step is picked when all are let go. KNOB 1 skips
    steps (a rest).
- **The patterns' grammar**: SEQ + OCT+ / OCT− the next pattern, the one before (playing: at the end of the one
  playing); SEQ held: PATTERNS (The layers, above); SEQ held 5 s on SEQ: the pattern's steps cleared (PASTE in
  PATTERNS puts them back).
- **Kept**: the patterns in the flash (four sector pairs at 0xCB000, free space of the main store, written while
  stopped once 3 s pass without sound); the pattern shown, CHAIN, RECORD, QUANTIZE and the chain's slots with the
  settings.

## LEDs

| LED | Lit | Glowing | Mid / breathing |
| --- | --- | --- | --- |
| A key | held; the MOD layer's source; MACROS: a module that is in; JUMP: the section shown; PATTERNS: the pattern shown | a scale's notes (a scale other than chromatic); the sources in use (MOD layer); a module bypassed (MACROS); a section's key (JUMP); PATTERNS: the one next, those holding notes, COPY PASTE CLEAR | mid: the scale's root |
| A family button | the family shown | the others | |
| REC | in its layer; the sequencer armed (recording: dark the last quarter of each beat) | otherwise | |
| A layer's button | REC or ENV while their MOD layer is held | | breathing: GLO, FX, LFO or EDIT while their layer is open, and REC or ENV while their MOD layer is latched |
| PLAY | a quarter of each beat while the transport runs | | its green LED: lit while it runs |
| ARP | the ARP on (dark a moment each beat while ARP or PATTERN plays) | the ARP off | |

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

- **MIDI LEARN** (SETTINGS > MIDI > LEARN ON): turn a knob on any page (its parameter is picked; the bottom row names it),
  move a controller: that CC sets the parameter from then on, in place of what either had. OCT− takes the picked
  parameter's CC away, OCT+ (or LEARN OFF) ends it. 32 learned CCs at most, kept with the settings; a learned CC comes
  before a parameter's own. CLEAR (turned twice within 2 s) forgets them all. CC 0, 32, 64 and 120–127 are never
  learned.
- V0.7: the chords out over USB MIDI (Chords, above). V0.75: MIDI clock, start, continue and stop with GLOBAL's CLOCK
  IN MIDI (The ARP, above). Not yet: the rest of MIDI out (a knob's CC, the keys without the chords).

## SETTINGS (HOME held 5 s)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| SCREEN | COLOR: STRATA (default), CLASSIC, ICE, VIOLET, PAPER, NIGHT, HI-CON, MONO, MOSS, DUSK, SAND, NEON (V0.67's four) | HELP: a section's hint at the bottom as it opens (on by default) | GLOW: the LEDs' glow, HIGH or LOW | |
| PANEL | ACCEL: the knobs' speed sensitivity | HOLD: 300, 400, 500 or 600 ms before a held button's layer | | |
| MIDI | CHANNEL: ANY or 1–16 | LEARN | CLEAR: the learned CCs | PROGRAM: program changes taken |
| BENCH | RUN: two loops timed, a moment's silence (on the FM-1) | PEAKS: CLEAR | | |
| CORE 2 | ON / OFF: the second core started at the next power-on (V0.8 BENCH; off by default) | TEST: 30 s, again to stop | LOAD: the test's unison voices, 4–32 (16) | |

BENCH shows what the FM-1 measures, a row each: AUDIO, CPU, SCREEN (V0.66: and the section of the longest frame),
KEYS (V0.66: what the main loop held the keys up, drawing · over saves; on a page where the keys only play the scan's
timer plays them), TABLES, FLASH, STACKS, RUN (what each means: [TESTING.md](TESTING.md) §8.3; how to use it in the
hardware test: [FLASHING.md](FLASHING.md) §4). The emulator says it has no clock of its own.

**CORE 2** (V0.8 BENCH: the FM-1's second core measured before the looper puts voices on it, [LOOPER.md](LOOPER.md)).
ON takes effect at the next power-on (the settings are written after 3 s of quiet: wait 5 s, then switch off and on);
not after a boot that crashed, nor with HOME held at power-on. Its rows: STATE (OFF, ANSWERING, NO ANSWER: OFF,
MISREAD: OFF, STOPPED: OFF, SAFE START: NOT ON; a unit whose core 1 misreads or stops is held, plays on one core and
turns ON off), TEST (READY, the seconds left, DONE), CORE 1 (its time for LOAD's oscillators a half, the tick its
longest), CORE 0 (the same work on core 0), SLOWED (core 0's own render while core 1 ran, against alone), WAIT (core
0's longest wait for core 1), JOBS (done, and BAD: any it would not run), STACK (core 1's, of 4,096 bytes). TEST runs
while something plays: hold a chord or PLAY a pattern. The console's `bench` has the same figures (`c2_` lines). The
emulator has one core: its page says so and TEST is refused.

HOME leaves the SETTINGS. GLOBAL's TEMPO (BPM 20–300, TS-BEAT 1–16, TS-NOTE 1, 2, 4, 8, 16) and SCL's page are kept
with the settings too, not with a sound.
