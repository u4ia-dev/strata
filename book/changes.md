# Changes

What each version changed, newest first.

## V0.87.1

The book matches Strata V0.87.1 at commit ae531d4.

**Panel.**
- <kbd>HOME</kbd> held for 5 s opens DEBUG only on the HOME page. On any other page the same hold shows the help map for
  as long as you hold it.
- WIPE moved from SETTINGS into DEBUG. Press <kbd>HOME</kbd> twice to confirm.
- A list or question is confirmed by pressing twice the button that opened it: <kbd>EDIT</kbd> for the wavetable, kit and
  drum-preset lists, <kbd>SEQ</kbd> for the projects list and a track's TYPE, <kbd>HOME</kbd> for WIPE. One press, or
  any other button, says no.
- CLIPS: <kbd>OCT−</kbd> and <kbd>OCT+</kbd> step through the scenes, KNOB 2 sets PLAYS, MUTE or SOLO, KNOB 4 chooses the
  track, and PRESETS opens the track's sound list again.
- Tap tempo is <kbd>GLO</kbd> held + F3, the lowest key (it does not sound). <kbd>GLO</kbd> + <kbd>PLAY</kbd> is PLAY's own
  again.

**Screens.** Strata starts with its logo. The POLARITY dial points up at rest. ARP STEPS shows each step's CHANCE as a
fader. TAPE moves with the sound. The drum grid's row names are shorter. KEY SCAN names KNOB 1's lines instead of flagging
them.

**Test projects.** DEBUG > TESTS has four built-in projects (VOICES, FX, DRUMS, LOOPER) that stress the voices, every
effect, the drums and the looper for 60 to 90 seconds. Loading one saves your open project first; nothing from a test is
saved, "TEST · NOT SAVED" shows while one is loaded, and the sound you had comes back when it ends.

**Sound engine.** Chords can use more voices before Strata limits them, and a new note first takes unison voices from the
others before it steals one. The effects' low-cost mode and HALL's extra lines fade in and out instead of clicking. The
MASTER knob ignores its own noise. PROJECT NEW keeps CLOCK IN.

**Backup mode.** Strata can enter backup mode in three ways. Unconfirmed on hardware.

## Book rewritten: condensed, one-page guide

The book now matches Strata V0.87 at commit 3f869d7. The Guide is one page of feature cards, the cheat sheet is laid out
wide, and the Manual is shorter, with steps first. SCL's unused MIDI page is gone from the firmware.

## V0.87

**Drums.** Any drum row can use any of seven models (ANALOG, PUNCH, FM, CHIP, NOISE, METAL, WAVE), on DRUM's new MODEL
page. 64 factory drum presets in eight categories (<kbd>EDIT</kbd> twice on DRUM), six factory kits (808, 909, LO-FI,
VIDEO GAME, TRAP, DRUM & BASS: <kbd>EDIT</kbd> twice on KIT or HOME), and your own library of 64 drum presets and 16
kits. EUCLID fills a row with a Euclidean rhythm.

**Interface.**
- <kbd>SAVE</kbd> opens the SAVE window (SAVE, SAVE AS NEW, CANCEL).
- <kbd>ENV</kbd> or <kbd>LFO</kbd> twice opens the assign window, with a POLARITY for each matrix slot.
- Resets are a menu button + <kbd>OCT−</kbd> held 5 s.
- <kbd>HOME</kbd> twice opens SETTINGS, which gained WIPE and LIGHTS; <kbd>HOME</kbd> held 5 s opens DEBUG.
- <kbd>EDIT</kbd> twice opens the wavetable list. The MATRIX moved to EDIT.
- Looper: tracks as rows on CLIPS, SONG on LOOP, the projects as a list (<kbd>SEQ</kbd> twice), new and deleted clips
  with <kbd>SEQ</kbd> + <kbd>OCT+</kbd> / <kbd>OCT−</kbd>, TAKE (<kbd>REC</kbd> twice), tap tempo (<kbd>GLO</kbd>
  held + <kbd>PLAY</kbd>).

## Earlier versions

| Version | What it added |
| --- | --- |
| V0.86 | The drum editor (DRUM, KIT); one screen layout; HOME's mixer pages; the presets list; looper SPEED and TOOLS; double presses; the help map |
| V0.85 | Drum tracks: eight drums, accents, chance and ratchet, the step grid, the DUCK sidechain |
| V0.8 | The 8-track looper: scenes, takes, overdubs, recorded knob moves, launch pad, mixer, SONG, projects |
| V0.75 | The arpeggiator with sixteen steps; one button grammar for every menu; strum |
| V0.7 | Chords: KEY and FREE modes, voicing, BASS |
| V0.67 | Larger, clearer screens; VALUES pages; four more themes |
| V0.66 | The STRATA screen design; smoother effect changes; faster screen and keys |
| V0.6 | The effects chain and seven new effects |
| V0.5 | The first Strata to run on an FM-1: the wavetable engine, presets, MIDI, USB audio |
