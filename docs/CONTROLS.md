# Strata controls

Everything the panel does in Strata, the wavetable firmware for the M-VAVE FM-1. Status: **V0.87 steps 1 and 2** (the
changes below, "V0.87"; step 2 the drums: "The drums' models, presets and kits"), on V0.86 step 2, the drum editor (DRUM and KIT, each drum's DRIVE, EQ and SHAPE, COMP, eight factory kits), on step 1's interface (one layout on
every screen, the pages regrouped, HOME's three pages, PRESETS' list, SEQ in two groups with SPEED and TOOLS, the
buttons once and twice, the help map, SETTINGS > POWER), on V0.85's drums, V0.8's looper, V0.75's ARP,
V0.7's chords, V0.67's legible screens and V0.6's FX chain, **verified in emulator**; V0.5 ran on an FM-1 on
2026-10-17, V0.6 and V0.65 since, V0.7 to V0.87 **need a hardware test**. How it is built:
[ARCHITECTURE.md](ARCHITECTURE.md) §14, §17. How it is tested: [TESTING.md](TESTING.md) §6.3.

**V0.87 (step 1), in short** (the tables below say it in full):
- Resets: **a menu button + OCT− held 5 s** resets what it shows (EDIT, ENV, LFO, FX, GLO, SCL, ARP, SEQ); a countdown
  shows from 2 s ("LET GO OF OCT- TO STOP"); OCT− let go before 2 s is the button + OCT−'s own tap. A button held 5 s
  alone no longer resets; SAVE held 5 s no longer saves.
- HOME twice: the SETTINGS; HOME held 5 s: the **DEBUG** menu (BENCH, KEY SCAN, CORE 2). SETTINGS: SCREEN, PANEL, MIDI,
  POWER, **WIPE** (every setting back to the firmware's; the presets and projects stay; asked first).
- EDIT twice: the **wavetables' list** of the oscillator shown (SELECT the category, PRESETS the table, SAVE twice
  loads); on a drum track, the kits. JUMP is gone; the MATRIX is in EDIT, after FILTER; GLO is VOICE and GLOBAL.
- ENV twice / LFO twice: the **assign window** over the page shown before: KNOB 1 the source, KNOB 2 the parameter,
  ALGORITHM page 2: KNOB 1 POLARITY (BIPOLAR, UNIPOLAR), KNOB 2 AMOUNT; OCT− takes the source off; the button shuts it.
  ENV has no MOD layer and no combos any more.
- SAVE once: the **SAVE window**: SAVE (over its user slot; a starter sound: "STARTERS STAY"), SAVE AS NEW (NAME IT,
  its next version), CANCEL; OCT− / OCT+ pick, SAVE does it, anything else shuts it.
- PRESETS' list: INIT first, then A to Z; it goes after 5 s untouched. A new unit starts on **INIT**, its four macros
  in its first four slots.
- SEQ: once LOOP (CLIPS, MIXER, SONG) and CLIP; twice the **projects' list**; SEQ + OCT+ a new clip of a bar in the
  scene picked (PRESETS, SEQ still held: its length); SEQ + OCT− **DELETE?** asked, again: deleted. SELECT on HOME
  picks the track. CLIPS: the tracks as rows, the scenes as columns.
- REC twice: **TAKE** (what was played lately, stopped or running, as a clip on the selected track). GLO held +
  PLAY tapped: **tap tempo** (three taps or more).
- SETTINGS > SCREEN > **LIGHTS**: OFF, LOW, MED, HIGH (the LEDs; HIGH by default).

## The panel

| Control | What it does |
| --- | --- |
| 27 keys, F3 to G5 | play notes; in some layers and screens they pick, toggle, jump or type (below) |
| OCT− / OCT+ | the keys' octave, −3 to +3 (the top line of SCL's page shows it) |
| KNOB 1–4 | the four values on the screen, left to right |
| SELECT | the next or previous module of the menu shown (OSC A, OSC B, SUB, NOISE, FILTER; an effect module; ..), stopping at the first and the last; a menu of one module: nothing (V0.86) |
| ALGORITHM | the next or previous page of the module shown (everywhere, the FX chain view too), stopping at the first and the last |
| PRESETS | the presets as a list (the starter sounds, INIT, then the user's): PRESETS picks, SAVE twice loads the one picked (V0.86; SAVE, presets, undo, below); on SEQ's pages the track; on a drum track the drum |
| MASTER | the output level |
| HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP | the menus: one grammar for all (Buttons, below) |
| SEQ | the looper's menu (V0.8, below): once LOOP and CLIP, twice the projects' list (V0.87); SEQ + OCT+ a new clip, SEQ + OCT− its deletion (asked); held: the launch pad |
| SAVE | the SAVE window, the browser, undo and redo |
| REC | a tap: the looper records on the selected track, in the scene picked, or stops recording (V0.8; V0.87: it acts after 300 ms, waiting for a second); twice: TAKE (V0.87); held: the MOD layer |
| PLAY | the transport: the clips that are on play from their start (V0.8); ARP and PATTERN step on its beat (V0.75) |
| ARP | the ARP's menu; ARP + OCT+ the ARP on, ARP + OCT− off, from any page (V0.75, below) |

The screen (the STRATA UI: [UI.md](UI.md)) is one picture a module, laid out the same on every menu screen (V0.86):
a top line (the module's name and its dot; a word of status; at the right the menu's module dots, SELECT's: one dot
for a menu of one module, SEQ's both groups with the other dimmed, the FX chain's squares), the body (what the module
does, with the four knobs' numbered handles on what each moves; the knob turning: its handle vermilion, the value it
sets vermilion where the page shows it, the shape before the turn dashed), and a bottom row: the module's pages,
ALGORITHM's, on every screen even with one page; the page shown in full and inverted, the others by short names (UNI,
SPR, PIT ..) where they would not fit, a < or > where the row goes on; a word for a moment; the knob turning's name
and value, where the page does not show it, until a second after its last detent. A **P** in a square marks what
PRESETS turns on the page (the sound, the drum, the track). The layers and the dialogs keep their own look.

**VALUES** (V0.67): every module (OSC A and B, SUB, NOISE, FILTER, ENV 1 and 2, LFO 1 and 2, VOICE, each FX module)
has one more zone at the end of its pages, VALUES: its pages' knobs again, a page each in the same order ("VALUES
1/5" holds WAVE's four, "2/5" UNISON's, ..), each value large with its bar, the range its modulation takes it and the
source. A knob there moves the parameter's finest step a detent (0.1 % of its range; a list one entry) however fast it
turns: for an exact value. VALUES is always a module's last page; ALGORITHM and OCT− / OCT+ with the family button
held reach it.

## The pages

A menu button once shows its menu where it was left, from another menu; in its own menu, its first page (V0.86, below:
Buttons). SELECT picks the module, ALGORITHM steps its pages, in every menu; both stop at the first and the last.

| Button | Module | Page: KNOB 1 · 2 · 3 · 4 |
| --- | --- | --- |
| HOME | HOME | SOUND: the sound (its category, number and name), its output live: MACRO 1 · 2 · 3 · 4 (a drum track: the kit, TUNE · DECAY · TONE · DUCK, below); MIX 1-4, MIX 5-8: the eight tracks' VOLUME, four a page |
| EDIT | OSC A, OSC B | WAVE: WT POS · TABLE · WARP · AMOUNT; UNISON: UNISON · UNI MODE · DETUNE · BLEND; SPREAD: WIDTH · WT SPRD · PHASE · RAND; PITCH: OCT · SEMI · FINE · PH MODE; OUTPUT: LEVEL · PAN · ROUTE |
| | SUB | MAIN: SHAPE · OCT · WIDTH; OUTPUT: LEVEL · PAN · ROUTE |
| | NOISE | MAIN: TYPE · COLOR · STEREO; OUTPUT: LEVEL · PAN · ROUTE |
| | FILTER | TONE: TYPE · CUTOFF · RESO · DRIVE; RESPONSE: KEYTRACK · VAR · MIX |
| ENV | ENV 1, ENV 2 | ADSR: ATTACK · DECAY · SUSTAIN · RELEASE; SHAPE: HOLD · A CURVE · D CURVE · R CURVE |
| LFO | LFO 1, LFO 2 | POINTS: POINT · TIME · LEVEL · CURVE; SHAPE: SHAPE · SYNC · RATE · MODE; TIME: DELAY · RISE · PHASE · SMOOTH |
| FX | FX CHAIN | CHAIN: ADD (the type) · SLOT · MOVE · MIX (below: the FX chain) |
| | the chain's modules | each its pages (below) |
| GLO | VOICE | VOICE: MODE · VOICES · GLIDE · GL MODE; PLAY: BEND UP · BEND DN · VEL AMP · VOLUME |
| | MATRIX | SLOTS: SLOT · SOURCE · DEST · AMOUNT |
| | GLOBAL | TEMPO: BPM · TS-BEAT · TS-NOTE · CLOCK IN (V0.75) |
| EDIT (a drum track) | DRUM | the drum PRESETS picks: SOUND: TUNE · DECAY · TONE · its own (PUNCH, SNAP, SPREAD, METAL, BEND); DRIVE: TYPE · DRIVE · MIX; EQ: LOW · MID · HIGH · FREQ; SHAPE: ATTACK · SUSTAIN; MIX: LEVEL · PAN (The drums, below) |
| | KIT | the whole kit: ACCENT: ACCENT · VELOCITY · TONE; DUCK: SOURCE · AMOUNT · ATTACK · RELEASE; CURVE: SHAPE · HOLD; COMP: AMOUNT · ATTACK · RELEASE · MIX |
| SCL | SCALE | SCALE: SCALE · ROOT · FIT · VELOCITY |
| | CHORD | CHORD: MODE · TYPE (KEY: SHAPE) · VOICING · SPREAD; STRUM: STRUM · DIRECTION · HUMAN · TAPER; BASS: BASS · OCTAVE · LEVEL (Chords, below; the selected track's) |
| ARP | ARP | STYLE: STYLE · RATE · MOVE · FEEL; GROOVE: GATE · SWING · LENGTH · LATCH (The ARP, below) |
| | STEPS | STEPS: STEP · KIND · LEVEL · OCTAVE; RATCHET: STEP · RATCHET · CHANCE · SLIDE |
| SEQ once | LOOP | CLIPS: TYPE · SCENE (the keys launch; V0.87: the tracks as rows); MIXER: VOLUME · PAN · MUTE/SOLO; SONG: SLOT · SCENE · REPEATS · SONG (V0.87) |
| | CLIP | NOTES: NOTE · PITCH · START · LENGTH; NOTE+: VELOCITY · GRID · DELETE; CLIP: LENGTH · SPEED · QUANTIZE · MOTION; TOOLS: COPY · PASTE · DOUBLE/HALVE · CLEAR (a drum track: GRID, HIT, CLIP, TOOLS) |
| SEQ (CLIP's ALGORITHM on) | TRACK | RECORD: QUANTIZE · CHANNEL · MIDI IN; CLICK: CLICK · LEVEL · COUNT-IN · LAUNCH |
| SEQ twice | the projects | a list as the presets' (PRESETS picks, SAVE twice loads; V0.87, S2) |
| HOME twice | SETTINGS | SCREEN: COLOR · HELP · LIGHTS; PANEL: ACCEL · HOLD; MIDI: CHANNEL · LEARN · CLEAR · PROGRAM; POWER: SHOW BATTERY · SCREEN OFF AFTER; WIPE |
| HOME held 5 s | DEBUG | BENCH: RUN · PEAKS; KEY SCAN; CORE 2: ON · TEST · LOAD |

V0.86 regrouped the pages so that a page's knobs belong together (G1-G8 in the plan): OSC's UNISON (UNISON, UNI MODE,
DETUNE, BLEND) and SPREAD (WIDTH, WT SPRD, PHASE, RAND), SUB's and NOISE's OUTPUT (LEVEL, PAN, ROUTE), the delay's
DELAY (SYNC, TIME, FEEDBACK, MIX) and TONE (MODE, LP, HP), the reverb's REVERB (TYPE, SIZE, PREDELAY, MIX) and TONE
(DECAY, DAMP, LOW CUT, WIDTH), the modulated effects' SYNC first. A page draws by parameter: its handles follow the
knobs wherever the table puts them.

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
| FX twice | the module picked opened: its pages; SELECT steps the chain's modules, ALGORITHM a module's pages; FX once there: back to the chain view, that module picked (V0.86) |

Each type plays twice at most (DRIVE 2, EQ 2, ..), DELAY and REVERB once: "PHASER: TWO AT MOST", "DELAY: ONE AT
MOST"; "THE CHAIN IS FULL: 8 MODULES". Undo (SAVE + OCT−) takes back each add, take-out, move and bypass. The top
line shows the chain's modules of 8 and their cost on one core (the cost model's estimate: REVERB ~13 %, the others 1–6 %).

| Module | Pages: KNOB 1 · 2 · 3 · 4 |
| --- | --- |
| DRIVE | DRIVE: TYPE · DRIVE · TONE · MIX |
| CHORUS | CHORUS: RATE · DEPTH · DELAY · MIX; SPACE: FEEDBACK · SPREAD (V0.86: the right side's taps from the left's, 0 .. 100 %) |
| DELAY | DELAY: SYNC · TIME · FEEDBACK · MIX; TONE: MODE · LP · HP |
| REVERB | REVERB: TYPE · SIZE · PREDELAY · MIX; TONE: DECAY · DAMP · LOW CUT · WIDTH |
| OTT | OTT: DEPTH · TIME · UPWARD · DOWNWARD; BANDS: LOW · MID · HIGH |
| EQ | POINTS: POINT · FREQ · GAIN · Q; SHAPES: SHAPE · FREQ · AMOUNT (below) |
| FLANGER | FLANGER: SYNC · RATE · DEPTH · MIX; TONE: DELAY · FEEDBACK · SPREAD |
| PHASER | PHASER: SYNC · RATE · DEPTH · MIX; TONE: CENTER · FEEDBACK · STAGES · SPREAD |
| FILTER | FILTER: TYPE · CUTOFF · RESO · MIX; DYNAMICS: DRIVE · FOLLOW (V0.86: the cutoff follows the input's level, −100 .. +100 %) |
| BITCRUSH | CRUSH: BITS · RATE · MIX |
| TREMOLO | TREMOLO: MODE (TREMOLO, AUTOPAN) · SYNC · RATE · DEPTH; SHAPE: SHAPE · SPREAD (V0.86: the right side's phase from the left's) |

No page is called MORE any more (V0.86, after the renders): each second page is named for what it holds.
| RING MOD | RING MOD: FREQ · SHAPE · SPREAD · MIX |
| TAPE | TAPE: WOW · FLUTTER · SATURATE · MIX; TONE: TONE · HISS (V0.86: a hiss under what plays, gated by it, −34 dB at most) |

**The EQ's SHAPES** (V0.86): SHAPE picks one of nine curves, each its three points' types, places and gains: FLAT, HIGH
PASS, LOW PASS, BAND PASS, NOTCH, TWO PEAKS, SMILE, TELEPHONE, AIR; FREQ moves it along (its places together), AMOUNT
scales its gains. A point set on POINTS makes it CUSTOM. Each point has a type now (VALUES' last knobs): LOW and HIGH
a SHELF, a PASS (12 dB an octave) or a PEAK; MID a PEAK, a NOTCH or a BAND.

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

**Centre detents** (V0.86, F26): on a knob with a middle (PAN, the tracks' and the drums' PAN, the drums' and the
kit's TUNE, the kit's TONE, every bipolar parameter: the EQ's gains, FOLLOW, the matrix's amounts ..), a turn through
the middle (or onto it) stops there and holds for three detents more the same way; a turn back leaves at once, and a
new turn from the middle (after 0.6 s) holds nothing.

## Buttons: taps, holds and chords

A hold is a button held for the HOLD time (SETTINGS > PANEL, 400 ms by default); a double tap is two taps within 300 ms.

**One grammar for the menu buttons** (V0.86, B1-B17: HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP and SEQ; SAVE keeps its
own):

- **Once**: from another menu, the page it was left on (SEQ: LOOP and CLIP, where they were left); in its own menu,
  its first page (FX: the chain view, the module shown picked). It acts at once.
- **Twice**: a second press within 300 ms adds its own action, on what the first opened (the table). ENV, LFO and FX
  twice count from what was shown before the first press (V0.87: ENV and LFO open the assign window over it).
- **Held**: its layer while held (The layers, below). HOME held: the help map.
- **+ OCT− held 5 s** (V0.87, U2): what it shows back to its starting values: a countdown from 2 s ("LET GO OF OCT- TO
  STOP"); a sound's section can be undone (SAVE + OCT−); SCL's and the ARP's settings cannot. HOME held 5 s: DEBUG.
- **+ OCT+ / + OCT−**: on / off (add / take out, next / back) for what the menu is about, from any page (the page stays,
  LFO's: its POINTS).
- **HELP** on (SETTINGS > SCREEN): a menu opened from another says what twice does there, for a moment ("AGAIN: ENV 2").

| Button | Once (another menu / its own) | Twice | Held | + OCT− held 5 s | + OCT+ | + OCT− |
| --- | --- | --- | --- | --- | --- | --- |
| HOME | the page left / SOUND | the SETTINGS; a tap: back | the help map; 5 s: DEBUG | | the next sound | the sound before |
| EDIT | the page left / OSC A > WAVE (a drum track: DRUM > SOUND) | the wavetables' list of the oscillator shown (a drum track: the kits) | | the section shown reset | the source or filter shown on | off |
| ENV | the page left / ENV 1 > ADSR | the assign window (an envelope or a macro onto a knob of the page before) | | the envelope shown reset | | |
| LFO | the page left / LFO 1 > POINTS | the assign window (an LFO onto a knob of the page before) | MOMENTARY | the LFO shown reset (its points too) | a point added (left of the one picked; halfway) | the point picked deleted (not the ends) |
| FX | the page left / the chain view | the chain view <-> the module picked | MACROS | the module shown reset (the chain view: the chain as INIT's) | the chain view: a module added; a module's page: it in | taken out; bypassed |
| GLO | the page left / VOICE > VOICE | PERFORM latched (GLO closes it) | PERFORM (PLAY taps the tempo) | VOICE or GLOBAL (120 BPM, 4/4, INTERNAL) reset | PERFORM latched (a GLO tap lets it go) | let go |
| SCL | the page left / SCALE | the chords on, off | a key is the scale's root | SCALE and the chords reset (the selected track's) | the chords on (MODE as it was: KEY at first) | off |
| ARP | the page left / ARP > STYLE | the ARP on, off | | its settings or its steps reset | the ARP on | off |
| SEQ | LOOP and CLIP where left / LOOP > CLIPS | the projects' list | the launch pad | the page's settings as they start (MIXER the track's, SONG, RECORD, CLICK, a clip's SPEED and GRID; the clips stay) | a new clip, a bar (PRESETS, SEQ held: its length) | DELETE? the clip; again: deleted |
| SAVE | the SAVE window | the browser (in it: back) | | | redo | undo |
| REC | after 300 ms: the looper records (an empty clip: a take; a clip: overdub), again: stops (a latched MOD layer: let go) | TAKE: what was played lately as a clip | the MOD layer | | the MOD layer latched | let go |
| PLAY | the transport on, off (the clips on from their start; the arpeggio on its beat); GLO held: tap tempo | | | | | |
| OCT− / OCT+ | the keys' octave (the browser: OCT−'s at its release) | | the browser: OCT− 3 s deletes the user preset playing | | | |

**The help map** (V0.86, B17): HOME held shows, for the screen you are on, what SELECT, ALGORITHM, PRESETS and the four
knobs do there, and every button's twice; held on, the bar along the bottom fills and DEBUG opens at 5 s (V0.87).
Let go before: nothing changes.

Also: HOME held at power-on: the last sound is not brought back (after a crash neither: SAFE START). On NAME IT,
OCT+ letters / symbols, OCT− deletes; while MIDI LEARN listens, OCT− takes the picked parameter's CC away and OCT+
ends LEARN. **OCT− and OCT+ both held 5 s: UBOOT**, the chip's USB boot (a countdown in the footer from 2 s; letting go
stops it; [FLASHING.md](FLASHING.md)).

**Power-on** (V0.86, P4): the splash for 2 s, then HOME's SOUND.

**The backup-mode message.** Like the official firmware, Strata answers the SysEx message
`F0 22 24 35 7D F7` (the "soft key") on its USB MIDI port by restarting into the chip's backup mode (UBOOT), where a
full flash backup can be read or written. Any program that can send SysEx to the FM-1 can send it. The FM-1 then
stops playing and waits; switching it off and on returns to Strata. Nothing is written by the message itself. The same
mode is reached by holding OCT− and OCT+ for 5 s (a countdown from 2 s).

**Developer features kept in the build.** The USB serial console (a CDC port: the device report, BENCH's
figures, the event log), the DEBUG menu (HOME held 5 s: BENCH, KEY SCAN, CORE 2) and CORE 2 (the second core, off by
default; ON starts it at the next power-on, TEST runs a 30 s load) stay in every Strata build, release builds too.
CORE 2's figures on the FM-1 are not measured yet (**needs a hardware test**).

## The layers

A layer lasts while its button is held (REC's MOD layer and GLO's PERFORM can be latched: the button + OCT+; a tap or
the button + OCT− lets it go; V0.8: a REC tap records; GLO twice latches PERFORM and a GLO tap closes it; V0.87: no
JUMP, no ENV layer, LFO twice is the assign window). A banner names it and says how it
ends ("TAP GLO: CLOSES" when latched), and its button breathes (a held MOD layer's is lit).

| Layer | Its button | The knobs | The keys | Let go |
| --- | --- | --- | --- | --- |
| MOD | REC | the source's depth on each knob's parameter (a matrix slot, made when needed; one at 0 goes when the layer closes) | a white key picks the source | it closes |
| PERFORM | GLO | 1 pitch bend (16 detents from the middle to an end), 2 mod wheel, 3 GLIDE, 4 VOLUME | play | the bend goes back to the middle; the wheel stays |
| MACROS | FX | MACRO 1–4 | F3 to F4 (the white keys): the chain's modules 1 to 8 bypassed or back in (lit when in, glowing when bypassed) | it closes |
| MOMENTARY | LFO | change the page's values for now (16 at most) | play | every value it changed goes back |
| LAUNCH PAD | SEQ (V0.8) | nothing | white keys 1-8 (F3 to F4): the tracks' clips in the scene picked (launch; the one playing: stop; an empty one: the track selected); 9-16 (G4 to G5): scenes A to H; black keys 1-8 (F#3 to A#4): MUTE tracks 1 to 8; C#5 STOP ALL, D#5 SOLO (the selected track), F#5 UNDO PASS | it closes (a key used: no reset at 5 s) |

The MOD layer's sources on the white keys: F3 ENV 1, G3 ENV 2, A3 LFO 1, B3 LFO 2, C4 to F4 MACRO 1 to 4, G4 VELOCITY,
A4 NOTE, B4 RANDOM, C5 ALTERNATE, D5 MOD WHEEL, E5 BEND, F5 AFTERTOUCH, G5 EXPRESSION.

## SAVE, presets, undo

- **SAVE once: the SAVE window** (V0.87, U17; the single tap waits 300 ms for a second one): SAVE (over its own user
  slot; on a starter sound "STARTERS STAY" and SAVE AS NEW picked), SAVE AS NEW (NAME IT with its next version: "WARM PAD
  v2", one above the highest of that name; SAVE there writes it in the first free slot of 128, its category kept: "SAVED:
  WARM PAD v2 · USER 3"), CANCEL. OCT− / OCT+ pick a row, SAVE does it; a knob or another button shuts it.
- **SAVE twice: the browser.** KNOB 1 the category (ALL, BASS, LEAD, PAD, PLUCK, KEYS, FX, SEQ, OTHER, with their
  counts), KNOB 2 the preset (loaded as it turns), KNOB 3 the bank (ALL, STARTER, USER). SAVE twice or HOME: back.
- **OCT− held 3 s in the browser: the user preset playing deleted** after a countdown ("HOLD: DELETE USER 3 IN 2";
  letting go stops it); the sound plays on, from no slot. **SAVE + OCT− straight after puts it back** ("BACK IN USER
  3"; its slot taken meanwhile: the first free one). The starter sounds stay. OCT− tapped there: the octave.
- **SAVE in the browser: NAME IT** for the sound playing. The keys type: F3 to F#5 A to Z, G5 a space; OCT+ the numbers
  and symbols, OCT− deletes. KNOB 1 the cursor, KNOB 2 the character there (at the end: ADD), KNOB 3 the category, KNOB
  4 the slot: its own (a rename), or for a starter sound the first free one (a slot in use says what it would replace).
  16 characters. SAVE writes it and goes back to the browser, HOME goes back without.
- **PRESETS' list** (V0.86, H4, changed after the renders): turning PRESETS opens a list of the presets (five rows
  round the one picked, the one playing marked) and moves through it; nothing loads while it turns. **SAVE twice**
  loads the one picked and closes the list (undo takes the load back). **3 s untouched**, the list goes and nothing
  changed; another button closes it too. **SAVE + OCT+** saves the sound playing as a new preset: NAME IT opens on its
  next version's name ("WARM PAD v2") in the first free slot; SAVE writes it and goes back to the page (HOME: back,
  unsaved). **SAVE + OCT−** deletes the preset picked: a question first ("DELETE?" over its name), a second SAVE + OCT− deletes
  it, anything else keeps it; the starter sounds stay. With edits not saved, the bottom row warns "SAVE TWICE: YOUR
  EDITS GO" (F28). SAVE + OCT− / OCT+ outside the list are undo and redo, as before. A sound edited since it was loaded
  or saved has a small vermilion dot after its name on HOME, in the list and in the browser (F25).
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

SCL's CHORD section makes the keys a chord instrument, after the Orchid's ideas (our own design and code). V0.8: each
track of the looper has its own SCALE, chords and ARP; the menus edit the selected track's.
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
- **MIDI** (V0.8: the looper's TRACK page): notes in always play through the track's chords; what they play goes out
  over USB on the track's CHANNEL with TYPE SYNTH+MIDI or MIDI, the bass with the chords.
- Kept with the project (V0.8; a track each), whatever sound is loaded. A change of MODE stops what plays (press again). The ARP on or off,
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

## The looper (V0.8)

The SEQ button's menu (V0.86, S1): **SEQ once: LOOP and CLIP** (playing and editing: LOOP's CLIPS, MIXER; CLIP's
NOTES, NOTE+, CLIP, TOOLS), **SEQ twice: TRACK and SONG** (setting up: TRACK's RECORD, CLICK; SONG's SONG, PROJECT).
The top right shows both groups' dots, the other group dimmed. Eight tracks, eight scenes (A to H): each track a clip
in each scene, a loop of notes and knob moves at the times they were played, one to sixteen bars long. The design and
its decisions: [LOOPER.md](LOOPER.md).

- **The track selected** plays the panel's keys and is what SCL, ARP and the chords' pages edit (each track has its own
  SCALE, chords and ARP). **PRESETS picks it on every SEQ page** (V0.86, C1: its name in the title, a P before it; a
  change of track says "T3 DRUMS" for a moment); SEQ + OCT+ / OCT− the next track, the one before; a white key 1-8 on
  the launch pad. The top right of the SEQ pages: the track, the scene picked, the bar and beat while running, REC or
  DUB.
- **A track's TYPE** (CLIPS' KNOB 1): SYNTH (the sound playing; V0.9: a sound a track), SYNTH+MIDI (the same, its notes
  out over USB MIDI too), MIDI (out over USB only, no sound: a DAW, another synth), DRUMS (below). It changes when the
  turning stops (0.5 s); a track that has clips asks for one detent more ("TURN ON: MIDI"; 2 s, else "TYPE KEPT": C6).
  RECORD's CHANNEL: its channel out (1-16; tracks 1 to 8 start on 1 to 8); MIDI IN: SELECTED (notes in play whichever
  track is selected) or a channel (notes on it play and record this track, selected or not).
- **CLIPS**: the grid, a column a track and a row a scene; the selected track's column lit. A clip playing is filled
  (its place a line along it), one launched blinks framed in vermilion until it starts, one recording is vermilion, an
  empty cell a dot; the scene picked bold, the selected cell framed. TYPE (KNOB 1), SCENE (KNOB 2). **The keys launch
  here** (V0.86, S5), SEQ not held: white keys 1-8 this scene's clips on tracks 1-8 (the one playing: it stops), black
  keys 1-8 scenes A-H; their LEDs show what plays (lit), what is launched or recording (blinking), what is there
  (glowing). The launch pad (SEQ held) stays as it was.
- **Launching** waits for the next bar (CLICK's LAUNCH: BAR, BEAT or NOW). Stopped, a launch starts the transport
  from the top. PLAY stops everything; PLAY again plays the clips that were on, from their start.
- **Recording** (REC, the selected track, the scene picked):
  - **A take** (an empty clip): stopped, a bar of COUNT-IN (its clicks) and the transport starts; running, it starts
    at the next bar. What you play goes in at its time (QUANTIZE: onto 1/32 .. 1/4), as long as you held it. REC again
    ends the take, rounded to whole bars (1 to 16; sixteen bars end it by themselves), and it loops at once. A take
    records at SPEED x1.
  - **Overdub** (a clip with notes): REC adds to it pass after pass (a clip not playing is launched first) until REC
    again. A note played just before its place is not played twice. **UNDO PASS** (the launch pad's last black key)
    takes the last pass back; again puts it back (until something new is recorded).
  - **Knob moves**: a knob turned while a synth track records (a parameter that can be modulated: CUTOFF, WT POS, an
    effect's MIX ..) writes its moves, replacing the ones it passes; while it turns, the knob is heard. When the clip
    plays the parameter follows them; stopped, or the clip stopped, the knob's own value again. The sound itself is
    untouched: SAVE and undo keep the knob's value.
  - The notes go in as played, before the track's chords and ARP: they play through them.
- **MIXER**: VOLUME (0-127, 100 as recorded), PAN, MUTE/SOLO (PLAYS, MUTE, SOLO) of the selected track; a strip each
  (PAN's dot over a fader; M, S under). A muted track starts no notes; SOLO mutes the others. HOME's MIX 1-4 and MIX
  5-8 set the eight VOLUMEs too.
- **NOTES, NOTE+** (V0.86, C2): the selected track's clip in the scene picked as a piano roll, a bar at a time round
  the note picked. NOTES: NOTE (in time order), PITCH, START (by GRID), LENGTH (by GRID); NOTE+: VELOCITY (a lane of
  them under the roll), GRID (FREE (a tick), 1/32 .. 1/4, 1/16 by default), DELETE (right: the note picked gone). The
  playhead marked over the bar playing.
- **CLIP**: LENGTH in bars (an empty clip: one made, of no notes, to fill by overdub); **SPEED** (V0.86, S8: x1/2, x1,
  x2; the clip playing goes on from where it is; a scene lasts as long as its longest clip at its speed); QUANTIZE
  (right: its notes onto GRID's, 1/16 with FREE); MOTION: a parameter it moves (its moves drawn as a line). SEQ + OCT−
  held 5 s on CLIP: its SPEED and GRID as they start (V0.87); SEQ + OCT−: the clip deleted, asked first.
- **TOOLS** (V0.86, S8), each a turn, as NEW is; one that would take something away asks for a second turn within 2 s
  ("AGAIN: T2 B GOES"):
  - COPY: a turn keeps the clip shown; turning on, a bar of it (BAR 1, BAR 2 ..).
  - PASTE (right): what was copied into the clip shown: a whole clip replaces it (its length and SPEED with it); a bar
    goes into the same bar here (that bar's notes and moves replaced; a clip without that bar: "T1 C HAS NO BAR 2").
    It takes what the copied clip holds when you paste.
  - DOUBLE / HALVE: right twice as long, its notes and moves again in the second half (up to the longest the time
    signature allows); left half as long, its second half gone (asked when notes go).
  - CLEAR: right its notes (a drum track: its hits), left the motion MOTION picks on CLIP; asked first.
  None works on the clip recording ("RECORDING: REC ENDS IT").
- **SONG**: sixteen slots, each a scene and its REPEATS; SONG ON: PLAY plays slot 1's scene its repeats (a scene as
  long as its longest clip), then slot 2's .., round again, from the first slot without a scene.
- **PROJECT**: four, each everything above (the tracks, their chords and ARP, the clips and their SPEED, SONG). The one
  loaded is kept when stopped and 3 s pass without sound; LOAD (right) saves it, stops the transport and loads the one
  picked (empty: a new one); NEW (right twice within 2 s): the one loaded cleared. Projects 1 and 2 live where the 128
  user presets used to end; 3 and 4 where presets 65 to 104 were (the user presets are 64 now): while an older build's
  presets are still there, those two are not saved ("PROJECT 3: OLD PRESETS THERE"). 2,048 notes and moves a project.
  A project saved by V0.8 to V0.85 loads with every clip at x1.
- **RECORD**: QUANTIZE (OFF, 1/32 .. 1/4), the selected track's CHANNEL out and MIDI IN.
- **CLICK**: OFF, REC (counting in and recording: the default), ON (always while running); LEVEL 0-10; the downbeat
  higher; COUNT-IN (OFF, 1 BAR) and LAUNCH (BAR, BEAT, NOW).
- **Kept**: QUANTIZE, COUNT-IN, LAUNCH, CLICK, its LEVEL and the project loaded with the settings; the rest in the
  project.

## The drums (V0.85; V0.86 the drum editor)

TRACK's TYPE has a fourth: **DRUMS**, a track that plays the project's drum kit (one kit a project; any track can be a
drum track, they share it). The design: [LOOPER.md](LOOPER.md) stage 2. Everything else of a track is as above (its
clips and scenes, VOLUME, PAN, MUTE, SOLO, recording, launching, SONG); a drum track has no chords, ARP or SCALE.

- **The kit**: eight drums, each its own model. KICK (a sine swept down; PUNCH: how high the sweep starts; TONE: drive
  and a click), SNARE (a body of two sines and noise; SNAP: the noise against the body; TONE: the noise's high-pass),
  CLAP (noise through a band-pass in four bursts; SPREAD: the bursts apart, 4 .. 20 ms; TONE: narrower), HAT CL, HAT
  OP and CYM (six square waves at a TR-808's frequencies and noise; METAL: the squares' share; TONE: the high-pass;
  a HAT CL ends a HAT OP ringing), WT 1 and WT 2 (one-shots on OSC A's and OSC B's wavetable; TONE: the FRAME; BEND:
  a fall from up to three octaves). Every drum: TUNE (±24 semitones), DECAY (its time to −60 dB, each model its own
  range), LEVEL, PAN.
- **Its keys** (a drum track selected): white keys 1-8 KICK, SNARE, CLAP, HAT CL, HAT OP, CYM, WT 1, WT 2; white keys
  9-16 the same, accented; the first black key DUCK. They glow so (the accented ones brighter). MIDI notes in: GM's
  drums: KICK 36 (35), SNARE 38 (37, 40), CLAP 39, HAT CL 42 (44), HAT OP 46, CYM 49 (51, 52, 53, 55, 57, 59), WT 1
  45 (41, 43, 47: the low toms), WT 2 50 (48); DUCK C1 (24); any other note none. A drum played is recorded as a hit
  (its first number), as notes are on a synth track.
- **HOME on a drum track** (V0.86, D1-D3): the kit's name, the eight drums as a 2 x 4 grid (each its name and its
  LEVEL, lit as it sounds), the output's bloom at the right; PRESETS picks the drum (framed: the one KIT opens on);
  the knobs move the whole kit: TUNE (±24 semitones on every drum), DECAY (every drum's shorter or longer, ±1 octave of
  time), TONE (every drum's TONE together) and the duck's AMOUNT.
- **DRUM** (EDIT on a drum track; V0.86 step 2, K1-K5): the drum PRESETS picks (wherever a drum track is selected;
  stopped, the drum you play is picked; the title says which, the eight names under it). SOUND: TUNE, DECAY, TONE and
  its own (PUNCH, SNAP, SPREAD, METAL or BEND); its fall drawn, KICK's sweep dashed. DRIVE: TYPE (SOFT, HARD, FOLD,
  CRUSH), DRIVE (in up to +24 dB, out held about level; CRUSH: 15 bits down to 2), MIX; its curve drawn. EQ: LOW (a
  shelf at 200 Hz), MID (a bell at FREQ), HIGH (a shelf at 2 kHz), each ±12 dB, FREQ 200 Hz .. 8 kHz; its response
  drawn. SHAPE: ATTACK (the hit's start, ±12 dB, back to the body over ~5 ms) and SUSTAIN (the body, ±12 dB); the
  fall drawn with and without. MIX: LEVEL and PAN (the eight as a mixer). Each drum: DRIVE, then EQ, then SHAPE, then
  its pan; each spends nothing at its neutral setting (DRIVE 0 or MIX 0, a band at its middle, SHAPE's at theirs).
  EDIT + OCT− held 5 s on DRUM: the drum picked as a kit starts, its effects off.
- **KIT** (EDIT on a drum track, after DRUM; K6, K9): ACCENT (ACCENT: an accented hit's lift, up to +6 dB; VELOCITY:
  how much a hit's velocity counts, 0 every hit as LEVEL; TONE: a soft hit darker, an accented one brighter; the gain
  by velocity drawn), DUCK (SOURCE: ROW (the DUCK row's hits), KICK (the KICK's), BOTH; AMOUNT, ATTACK 0 .. 50 ms,
  RELEASE 20 ms .. 1 s), CURVE (SHAPE: LINEAR, EXP (back quickly, then slowly), PUMP (held down, back late); HOLD: down
  for 0 .. 250 ms before the release; the synth's level drawn over time), COMP (on the eight drums together: AMOUNT
  (its threshold, 0 .. −30 dB, 4:1; above 20 % a make-up of up to +9 dB), ATTACK 0.1 .. 30 ms, RELEASE 20 ms .. 1 s,
  MIX (the pressed sound with the dry, in parallel); its curve drawn, level in against out). EDIT + OCT− held 5 s on KIT: the kit's own
  settings (these four pages) as a kit starts, its drums kept.
- **The drums' models, presets and kits** (V0.87 step 2; the plan's D1-D5):
  - **Any row any model.** DRUM > MODEL: KNOB 1 MODEL, KNOB 2 TYPE. ANALOG (a sine body swept down from DROP; SNAP
    adds its second body and noise: kicks, toms, congas, an 808 snare), PUNCH (KICK, SNARE, CLAP: V0.85's), FM (DRUM:
    a pitch drop and a click; BELL: none; TONE is RATIO, the model's own INDEX), CHIP (SQUARE and TRIANGLE swept down
    by SWEEP over SPEED; NOISE a 15-bit register at PITCH, LOOP from 50 its short loop; the level in 16 steps), NOISE
    (a band-pass at FREQ, WIDTH narrower), METAL (CLOSED, OPEN, CYMBAL: the six squares), WAVE (TABLE A, TABLE B: OSC
    A's or B's table, TONE the FRAME). SOUND's TONE and fourth knob take the model's names. A row keeps its place (its
    key, its GRID row, its GM note) whatever its model; a new model turns its name back to its place's. A project of
    before plays as it did (its rows on their own models, bit for bit).
  - **Drum presets by category** (KICK, SNARE, CLAP, HAT, CYMBAL, TOM, PERC, FX; eight factory presets each): EDIT
    twice on DRUM opens the row picked's category (its preset's, else its place's: the KICK row KICK, the WT rows PERC
    and TOM); SELECT the next category, PRESETS the preset (its model at the right), SAVE twice copies it into the row
    (its LEVEL and PAN kept): the row then shows its name. SAVE + OCT+ there: the row as a new preset, named first; SAVE
    + OCT− on one of yours: deleted, asked first.
  - **Kits**: EDIT twice on KIT (or from HOME): the six factory kits, 808, 909, LO-FI, VIDEO GAME, TRAP, DRUM & BASS
    (each row a copy of a preset), then yours (K1 to K16). PRESETS picks, SAVE twice loads the one picked as the
    project's kit (its drums, their effects, the duck and COMP; the tracks' hits stay); the list goes after 5 s
    untouched.
  - **Saving** (the SAVE window on a drum track): on DRUM it saves the row picked as a drum preset, on KIT, HOME and
    elsewhere the kit: SAVE over your own (its name kept: "SAVED OVER DRUM 3"), SAVE AS NEW (NAME IT: its next
    version, 11 characters for a drum, 12 for a kit; KNOB 3 a drum's category; KNOB 4 the slot). Yours are kept apart
    from projects: 64 drum presets and 16 kits (the store at 0xDB000, inside the allow-list); a kit is kept in its
    project as before.
  - **EUCLID** (CLIP's page on a drum track, after HIT): KNOB 1 HITS, KNOB 2 STEPS (1-16), KNOB 3 ROTATE, KNOB 4 the
    ROW: each turn refills the row's hits over the clip (an empty clip made a bar first), the pattern again every
    STEPS sixteenths; the other rows stay.
- **The kits** (EDIT twice on a drum track, V0.87; K8): PRESETS picks, SAVE twice loads the one picked as the
  project's kit; the list goes after 5 s untouched. The one the kit came from is marked, with a dot when it was edited since (HOME's name has the same dot);
  loading another then says first that the edits go: a kit loaded is not undone.
- **The sidechain**: the DUCK row's hits make no sound; each ducks the synth (every synth track, after its effects)
  by AMOUNT, over ATTACK, and lets it back over RELEASE along SHAPE. The drums are never ducked. MIDI tracks' notes go
  out untouched.
- **GRID, HIT** (SEQ's CLIP on a drum track, in place of NOTES and EDIT): the clip in sixteenths, sixteen steps at a
  time, a row a drum and DUCK. On these two pages **the white keys are the sixteen steps** (a key puts a hit of the
  row picked in, or takes it out; stopped, you hear it go in) and **the black keys pick the row** (1 KICK .. 9 DUCK);
  the keys' LEDs show the row's hits, the playhead's step and the row. An empty clip gets a bar at the first step put
  in, and plays when PLAY starts. KNOB 1 the step (the sixteen shown follow it), KNOB 2 the row; GRID: VELOCITY and
  ACCENT of the hit there; HIT: CHANCE (100 % .. 13 %: how often it plays) and RATCHET (1 .. 4 hits in its sixteenth).
  **A step held** (V0.86, S7): while a step's white key is held, the four knobs are its hit's VELOCITY, ACCENT, CHANCE
  and RATCHET (two by two under the grid, "STEP 5 HELD"); an empty step's key puts the hit in at once; a hit's key
  takes it out at the release, unless a knob turned while it was held. Let go, the knobs are STEP, ROW and the page's
  again.
  A hit's brightness is its velocity; accented, the cell's full height; under 100 %, hollow and filled as far as it
  plays; a ratchet, its hits as slivers; DUCK's, frames. CLIP's third and fourth pages are CLIP and TOOLS, as a synth
  track's.
- **TYPE** changes keep a clip as it is: on a synth or MIDI track a hit plays as a sixteenth, on a drum track a note as
  a plain hit of its drum (GM's numbers; others are silent). Back to DRUMS, nothing was lost.
- **Kept** in the project: the kit (its drums and their effects, the duck's and COMP's settings, its name and the
  factory kit it came from), the tracks' TYPE, the hits with their ACCENT, CHANCE and RATCHET. A project saved by V0.8
  or V0.81 loads with the kit as a project starts; one saved by V0.85 or step 1, with its effects off.

## LEDs

| LED | Lit | Glowing | Mid / breathing |
| --- | --- | --- | --- |
| A key | held; the MOD layer's source; MACROS: a module that is in; JUMP: the section shown; the launch pad: a track's clip playing, the scene picked, a track not muted, SOLO on; GRID and HIT (V0.85): the row's hits, the row picked; CLIPS (V0.86): a clip playing, the scene picked; blinking: a clip launched or recording | a scale's notes (a scale other than chromatic); the sources in use (MOD layer); a module bypassed (MACROS); a section's key (JUMP); the launch pad: a clip there, a scene with clips, STOP, SOLO, UNDO; a drum track: its eight drums and DUCK; GRID and HIT: rows with hits there; CLIPS: a clip there, a scene with clips | mid: the scale's root; a drum track's accented keys; GRID and HIT: the playhead's step |
| A family button | the family shown | the others | |
| REC | in its layer; the looper recording (dark the last quarter of each beat); counting in or waiting for the bar: blinking | otherwise | |
| A layer's button | REC or ENV while their MOD layer is held | | breathing: GLO, FX, LFO or EDIT while their layer is open, and REC or ENV while their MOD layer is latched |
| PLAY | a quarter of each beat while the transport runs | | its green LED: lit while it runs |
| ARP | the ARP on (dark a moment each beat while ARP or PATTERN plays) | the ARP off | |

SETTINGS > SCREEN > LIGHTS (V0.87, U9) sets how bright every LED is: OFF, LOW, MED, HIGH (each level steady every
frame: the scan's tick in quarters, 40 kHz). The rules every LED keeps (V0.86, F32; [UI.md](UI.md)
§6): off, nothing there; glowing, there; mid, in use; lit, active; blinking, waiting (a launched clip, a count-in);
breathing, recording or a latched layer.

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
- V0.8: each track's notes out over USB MIDI on its CHANNEL (TYPE MIDI, or SYNTH+MIDI), the chords and the bass with
  them; notes in on a track's MIDI IN channel play and record that track, the rest the selected track (The looper,
  below). V0.75: MIDI clock, start, continue and stop with GLOBAL's CLOCK IN MIDI (The ARP, above). Not yet: a knob's
  CC out.

## SETTINGS (HOME twice) and DEBUG (HOME held 5 s)

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| SCREEN | COLOR: STRATA (default), CLASSIC, ICE, VIOLET, PAPER, NIGHT, HI-CON, MONO, MOSS, DUSK, SAND, NEON (V0.67's four) | HELP: a section's hint at the bottom as it opens (on by default) | LIGHTS: the LEDs, OFF, LOW, MED, HIGH (V0.87) | |
| PANEL | ACCEL: the knobs' speed sensitivity | HOLD: 300, 400, 500 or 600 ms before a held button's layer | | |
| MIDI | CHANNEL: ANY or 1–16 | LEARN | CLEAR: the learned CCs | PROGRAM: program changes taken |
| DEBUG > BENCH | RUN: two loops timed, a moment's silence (on the FM-1) | PEAKS: CLEAR | | |
| DEBUG > KEY SCAN | the panel's matrix as scanned, live (read-only, V0.86 P3) | | | |
| DEBUG > CORE 2 | ON / OFF: the second core started at the next power-on (V0.8 BENCH; off by default) | TEST: 30 s, again to stop | LOAD: the test's unison voices, 4–32 (16) | |
| POWER | SHOW BATTERY: OFF, HOME, EVERY SCREEN (V0.86) | SCREEN OFF AFTER: OFF, 1, 5, 15 MIN | | |
| WIPE | WIPE: every setting back to the firmware's (theme, panel, MIDI and LEARN, tempo, scale and chords, ARP, looper, POWER, LIGHTS), asked first, SAVE answers; the presets and projects stay (V0.87, T1) | | | |

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

**POWER** (V0.86, F31, F34): the battery as four cells (the stock firmware's thresholds on the battery's ADC reading,
531, 561, 591; the lowest cell alone vermilion; a USB host powering it: a bolt) and its raw reading. SHOW BATTERY puts
the battery on HOME's top line or on every screen's. SCREEN OFF AFTER darkens the panel after that long with no
control touched; any control wakes it, and the press, turn or key that wakes it does nothing else. The backlight stays
lit: on the FM-1 its pin also enables the key and knob scan, so with it off nothing could wake the screen. The sound,
the looper, MIDI and USB go on; the LEDs stay as they are. Never dark under a countdown or a count-in. **Unverified on
the FM-1**: the thresholds against your battery, and the panel's sleep (Felucca's notes, from its 1.1.5).

**KEY SCAN** (V0.86, P3): the 11 x 6 matrix of keys, buttons and encoder pins as the scan reads it, live: a closed cell
filled, a key's or button's framed, an encoder's pin a ring, a cell of none dotted; a dotted cell that ever closes
while the page is open turns vermilion and is named ("SEEN CLOSED: PA5:9"): a second contact, such as velocity would
need. Read-only.

HOME leaves the SETTINGS. GLOBAL's TEMPO (BPM 20–300, TS-BEAT 1–16, TS-NOTE 1, 2, 4, 8, 16) and SCL's page are kept
with the settings too, not with a sound.
