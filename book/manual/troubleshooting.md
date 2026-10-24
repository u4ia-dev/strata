# Troubleshooting

## No sound, or the wrong sound

| Check | Where |
| --- | --- |
| MASTER up, and the sound's VOLUME not OFF | the panel; <kbd>GLO</kbd> > VOICE > PLAY |
| The selected track isn't MIDI, muted, or silenced by another track's SOLO | <kbd>SEQ</kbd> > CLIPS (TYPE) and MIXER |
| The keys aren't doing something else on this page | CLIPS launches clips, GRID and HIT set steps, a layer may be open: press <kbd>HOME</kbd> |
| FIT is not WHITE (which silences the black keys) | <kbd>SCL</kbd> > SCALE |
| Chords or a latched ARP aren't on | <kbd>SCL</kbd> + <kbd>OCT−</kbd>, <kbd>ARP</kbd> + <kbd>OCT−</kbd> |
| A DUCK row isn't holding the synth down | a drum track's HOME, DUCK knob |
| MIDI arrives on Strata's channel | SETTINGS > MIDI > CHANNEL |
| A sound stops the FM-1 at power-on | Hold <kbd>HOME</kbd> while switching on |

## Controls

- **A knob does nothing**: a playing clip may have recorded moves of that parameter, which take over while it plays.
  Stop the clip, or clear the motion on CLIP > TOOLS.
- **PRESETS doesn't open the presets**: on looper pages it chooses the track, on a drum track the drum.
- **A button only closed a window**: with a window open, the first press of another button closes it.
- **A reset didn't happen**: hold the button and <kbd>OCT−</kbd> for the full 5 seconds.
- **The screen or LEDs are dark**: SETTINGS > POWER > SCREEN OFF AFTER (any control wakes it), SCREEN > LIGHTS.
- **Crackles**: too much unison and too many effects at once; Strata trims them by itself. Use fewer unison copies or
  effects.
- **Everything feels wrong**: DEBUG > WIPE (hold <kbd>HOME</kbd> 5 s on HOME) resets the settings; presets and projects stay.

## Messages

| Message | Means |
| --- | --- |
| EVERY USER SLOT IN USE | All 64 preset slots are taken; delete one |
| STARTERS STAY | Starter sounds can't be overwritten or deleted; save as new |
| THE MATRIX IS FULL: 8 SLOTS | Free a slot on <kbd>EDIT</kbd> > MATRIX |
| THE CHAIN IS FULL: 8 MODULES | Remove an effect first |
| FULL: 2048 NOTES AND MOVES | The project is full; clear some clips or motions |
| THAT SCENE HAS A CLIP | TAKE needs an empty place; choose another scene |
| NOTHING PLAYED TO TAKE | TAKE only keeps what the selected track played recently |
| PROJECT 3: OLD PRESETS THERE | Presets from an old Strata version occupy that project's space; it isn't saved |
| MIDI'S CLOCK SETS THE TEMPO | Tap tempo is off while CLOCK IN is MIDI |
| SAFE START | Strata restarted after a crash without the last sound; INIT is playing |

## Installing

See [Installing and recovery](install.md#recovery) for a stopped write, a crash screen, or an FM-1 that no updater
can find.
