# Shaping a sound

A Strata sound flows like this: the **sources** (OSC A, OSC B, SUB, NOISE) go through the **FILTER**, the **envelope**
ENV 1 shapes each note's level, and the **effects** colour the whole. ENV 2, the two LFOs and the macros move any of it
over time through the **modulation matrix**.

## The oscillators

<kbd>EDIT</kbd> opens OSC A. SELECT steps to OSC B, SUB, NOISE, FILTER and the MATRIX.

- **WAVE**: KNOB 1 WT POS moves through the table's frames; KNOB 2 picks the TABLE; KNOB 3 picks a WARP (SYNC, BEND,
  PWM, FM from the other oscillator ..) and KNOB 4 sets how much.
- **UNISON**: up to 16 stacked copies of the oscillator, detuned and spread in stereo, for a big sound.
- **PITCH**: OCT, SEMI and FINE tune the oscillator against the other.
- **OUTPUT**: its LEVEL, its PAN, and whether it goes through the filter.

To pick a table from a list, press <kbd>EDIT</kbd> twice: the oscillator's **tables** show by category (ANALOG,
ADDITIVE, VOCAL, SPECTRAL, SYNC, FM, FOLD, PHASE, DIGITAL). SELECT jumps a category, PRESETS picks a table,
<kbd>SAVE</kbd> twice loads it.

![The wavetables' list](../images/TablesList-2x.png)

OSC B, SUB and NOISE start off: show one and press <kbd>EDIT</kbd> + <kbd>OCT+</kbd> to turn it on.

## The filter

SELECT to FILTER and turn it on (<kbd>EDIT</kbd> + <kbd>OCT+</kbd>). KNOB 1 TYPE (low pass, high pass, band pass,
LADDER, FORMANT ..), KNOB 2 CUTOFF, KNOB 3 RESO, KNOB 4 DRIVE. The curve on the screen is the filter's response.

## The envelopes

<kbd>ENV</kbd> opens ENV 1, the level of every note: ATTACK, DECAY, SUSTAIN, RELEASE. ENV 2 (SELECT) does nothing
until you put it onto something: a classic is ENV 2 onto the filter's CUTOFF.

## The LFOs

<kbd>LFO</kbd> opens LFO 1. Its first page, POINTS, is the LFO's shape as points you can move: KNOB 1 picks a point,
KNOB 2 and 3 move it, KNOB 4 bends the line after it. <kbd>LFO</kbd> + <kbd>OCT+</kbd> adds a point,
<kbd>LFO</kbd> + <kbd>OCT−</kbd> deletes the one picked. SHAPE sets the shape, its RATE (or a note value of the tempo)
and how it starts with each note.

## Put a modulator onto a knob

This is the quickest way to make a sound move.

1. Go to the page with the knob you want to move, such as FILTER's TONE page (CUTOFF).
2. Press <kbd>LFO</kbd> twice (or <kbd>ENV</kbd> twice for an envelope or a macro). The **assign window** opens over
   that page.
3. KNOB 1 picks the source (LFO 1, LFO 2; or ENV 1, ENV 2, MACRO 1 to 4). KNOB 2 picks the parameter on the page.
4. Turn **ALGORITHM** to the window's second page: KNOB 2 sets the AMOUNT (either way), KNOB 1 the POLARITY
   (BIPOLAR: both ways round the knob; UNIPOLAR: one way).
5. Press the same button again to close it.

![The assign window](../images/AssignLfo-2x.png)

<kbd>OCT−</kbd> in the window takes the source off the parameter picked. Every assignment is a slot of the
**MATRIX** (EDIT, SELECT to the last module), where you can see and change all eight.

> **Tip:** to give INIT's macros a job, open a page, press <kbd>ENV</kbd> twice, pick MACRO 1 with KNOB 1 and the
> parameter with KNOB 2, then set its AMOUNT. HOME's KNOB 1 now moves it.

There is a faster way still: hold <kbd>REC</kbd> (the **MOD layer**), press a white key to pick a source, and turn a
knob: its depth goes onto that knob's parameter. The Manual's [Modulation](../manual/modulation.md) explains it.

More: the Manual's [Oscillators, sub and noise](../manual/oscillators.md), [Filter](../manual/filter.md),
[Envelopes](../manual/envelopes.md), [LFOs](../manual/lfos.md).

Next: [Effects](effects.md).
