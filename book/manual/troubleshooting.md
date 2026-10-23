# Troubleshooting

## Installing

| What happens | What to do |
| --- | --- |
| A write stopped (the cable came out, the browser closed) | Don't switch the FM-1 off. Replug it and press **Find the FM-1** again: it is still in update mode, and the page finishes the write. |
| The flasher finds nothing | Close every other app that uses MIDI (DAWs, M-UPGRADE, Felucca's editor), use a data cable straight into the computer, allow MIDI with SysEx in the browser, use Chrome or Edge. |
| "Strata did not answer after the restart" | Stop. Go back by the Flipper Zero route ([Installing and going back](install.md#going-back)). |
| Strata plays, but no updater finds it | The Flipper Zero route, or the FM-1 Transporter: both enter backup mode at power-on, whatever the firmware does. |
| Windows offers to format a disk in backup mode | **Cancel.** Never format the `WL82 UBOOT1.00` disk. |
| Windows reports a USB device it cannot recognise after OCT− + OCT+ | That way into backup mode does not work on this computer: use the Flipper Zero route. |
| The FM-1 does not appear on USB in any mode | The Flipper Zero route or the FM-1 Transporter: the boot area they rely on is never written by any of these tools. |

## Crashes and freezes

- **STRATA CRASH** on the screen: take a photo (it helps the next version). It restarts after 4 s. A second crash
  within 30 s of a start puts it in backup mode by itself (the boot guard), where your backup or another firmware can
  be written.
- **A frozen screen, no crash screen**: the watchdog restarts it within about 8 s; two such boots and the boot guard
  puts it in backup mode. Leave it on for two minutes before anything else.
- **A sound stops the unit at power-on**: hold <kbd>HOME</kbd> while switching on: Strata starts without bringing it
  back. After a crash it does that by itself and says **SAFE START**.

## No sound, or not the sound expected

| Check | Where |
| --- | --- |
| MASTER up; the sound's VOLUME not OFF | the panel; GLO > VOICE > PLAY |
| The selected track is not MIDI, muted, or silenced by another's SOLO | SEQ > LOOP > CLIPS (TYPE), MIXER |
| The track's VOLUME | HOME's MIX pages |
| OSC A on, its LEVEL up; ROUTE and the FILTER's CUTOFF not shutting it | EDIT |
| ENV 1's SUSTAIN or DECAY not making it silent | ENV |
| The keys do something else on this page | the CLIPS page launches, the GRID and HIT pages are steps, a layer is open: go to HOME |
| FIT WHITE: the black keys are silent | SCL > SCALE |
| The chords or the ARP are on (LATCH holds notes) | <kbd>SCL</kbd> + <kbd>OCT−</kbd>, <kbd>ARP</kbd> + <kbd>OCT−</kbd> |
| A DUCK row with AMOUNT high holds the synth down | a drum track's HOME, DUCK knob |
| A WAVE drum row is silent while its table builds | wait a moment after changing OSC A's or B's TABLE |
| MIDI notes in are ignored | SETTINGS > MIDI > CHANNEL (ANY, or the sender's) |

## The knobs and buttons

- **A knob does nothing audible**: a clip with recorded moves of that parameter is playing (the clip's moves win while
  it plays; stop the clip, or CLEAR the motion on TOOLS). Or a matrix slot holds the parameter at an end: look at
  VALUES, which shows the range the modulation takes it to.
- **PRESETS does not open the presets**: on the looper's pages it picks the track, on a drum track the drum.
- **A button does nothing but close a window**: with a window open (the assign window, the SAVE window, a question),
  the first press of another button closes it.
- **A reset did not happen**: a button + <kbd>OCT−</kbd> must be held a full 5 s; letting go of OCT− between 2 and 5 s
  stops it.
- **The screen is dark**: SETTINGS > POWER > SCREEN OFF AFTER; any control wakes it (and that touch does nothing else).
- **The LEDs are dark**: SETTINGS > SCREEN > LIGHTS.
- **Everything behaves strangely**: SETTINGS > WIPE puts every setting back (presets and projects stay).

## Saving and the looper

| Message | Means |
| --- | --- |
| EVERY USER SLOT IN USE | all 64 user presets are taken: delete one in the presets' list or the browser |
| BUSY: TRY AGAIN | a save is still being written: wait a moment |
| STARTERS STAY / A STARTER SOUND STAYS | a starter can't be written over or deleted: SAVE AS NEW |
| NOTHING TO SAVE OVER | the sound comes from no user slot: SAVE AS NEW |
| PROJECT 3: OLD PRESETS THERE | an older build's presets are where projects 3 and 4 go; they are not saved over |
| NOT SAVED: NOT LOADED | the project could not be saved, so the other was not loaded |
| FULL: 2048 NOTES AND MOVES | the project's events are used up: clear clips or motions |
| THAT SCENE HAS A CLIP | TAKE needs an empty place: pick another scene |
| NOTHING PLAYED TO TAKE | TAKE keeps only what the selected track played lately |
| RECORDING: REC ENDS IT | the tools don't work on the clip recording |
| THE TAKE SETS IT: REC ENDS IT | a take's length is set when REC ends it |
| THE MATRIX IS FULL: 8 SLOTS | free a slot on EDIT > MATRIX |
| MIDI'S CLOCK SETS THE TEMPO | tap tempo is off while CLOCK IN is MIDI |
| THE CHAIN IS FULL: 8 MODULES / TWO AT MOST / ONE AT MOST | the FX chain's limits |

## Crackles and dropouts

Too much at once for the processor: many notes with big UNISON, and every effect. The CPU guard steps in by itself:
first fewer unison copies on the oldest notes, then cheaper effects (OTT's upward compression off, the reverb lighter),
then a voice let go, faded. To avoid it: less UNISON, fewer modules (REVERB is the costliest), fewer VOICES. On the
FM-1, DEBUG > BENCH's AUDIO and CPU rows show the load. See [Limits](limits.md).

## Reporting a problem

On the flasher's step 3, **Read the device report** reads `status` and `bench` from Strata's serial console; copy it into
your report with what you did, what you heard and saw, and a photo of any crash screen.
