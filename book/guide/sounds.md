# Sounds and presets

A **sound** is everything the synth plays with: the oscillators, the filter, the envelopes, the LFOs, the matrix, the
effects and the macros. A **preset** is a sound kept with a name. Strata has three kinds:

- **INIT**, a plain starting point (OSC A on a basic table, everything else off).
- **Eight starter sounds**, which you can play and edit but never write over.
- **Your presets**, up to 64, each with a name of 16 characters and a category (BASS, LEAD, PAD, PLUCK, KEYS, FX,
  SEQ, OTHER).

## Load a preset

1. Turn **PRESETS** on almost any page. The presets' list opens on the sound playing.
2. Turn PRESETS to the one you want.
3. Press <kbd>SAVE</kbd> twice. It loads.

![The presets' list](../images/PresetsList-2x.png)

The list shows five presets round the one picked; a dot at the right marks the one playing. A small vermilion dot
after a name means that sound has edits you have not saved. When loading would lose them, the bottom line warns
**SAVE TWICE: YOUR EDITS GO**. If that was a mistake, <kbd>SAVE</kbd> + <kbd>OCT−</kbd> (undo) brings your edited
sound back.

## Save your sound

Press <kbd>SAVE</kbd> once. The **SAVE window** opens over the page:

![The SAVE window](../images/SaveWindow-2x.png)

1. Press <kbd>OCT−</kbd> or <kbd>OCT+</kbd> to pick a row:
   - **SAVE** writes over the sound's own preset (a starter sound can't be written over: the row says STARTERS STAY).
   - **SAVE AS NEW** keeps it as a new preset, named as the next version: "WARM PAD" becomes "WARM PAD v2".
   - **CANCEL** closes the window.
2. Press <kbd>SAVE</kbd> to do it. Any other button or knob closes the window and does nothing.

SAVE AS NEW opens **NAME IT**, where you can change the name before it is written.

## Name it

![NAME IT, here naming a drum preset](../images/DrumNameIt-2x.png)

- The keys type: F3 to F#5 are A to Z, G5 is a space. <kbd>OCT+</kbd> switches to numbers and symbols; <kbd>OCT−</kbd>
  deletes.
- KNOB 1 moves the cursor, KNOB 2 changes the letter under it, KNOB 3 sets the category, KNOB 4 the slot.
- <kbd>SAVE</kbd> writes it. <kbd>HOME</kbd> leaves without saving.

## Other ways in

- **From the list**: with the presets' list open, <kbd>SAVE</kbd> + <kbd>OCT+</kbd> saves the sound playing as a new
  preset (NAME IT first), and <kbd>SAVE</kbd> + <kbd>OCT−</kbd> deletes the preset picked: it asks DELETE? first, and
  a second SAVE + OCT− confirms.
- **The browser**: <kbd>SAVE</kbd> twice (with no list open) opens the preset browser, sorted by category and bank.
  KNOB 2 loads presets as it turns. <kbd>SAVE</kbd> there renames the sound playing.

## What is kept

Strata keeps the sound playing, your presets and the settings in the FM-1's flash. The sound and the settings are
written once nothing has sounded for three seconds; a preset when you save it. At power-on they come back.

More: the Manual's [Presets and saving](../manual/presets.md).

Next: [Shaping a sound](shaping.md).
