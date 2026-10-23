# Looper basics

The looper records what you play into **clips** that loop. It has **eight tracks** (T1 to T8) and **eight scenes** (A
to H): each track can have a clip in each scene, so a scene is a row of clips that play together, like a section of a
song.

## The track you play

The keys play the **selected track**. On HOME, turn SELECT to pick it; on the looper's pages, turn PRESETS. Each track
has a **TYPE**:

- **SYNTH**: the sound playing. (Every synth track plays the one sound for now.)
- **SYNTH+MIDI**: the same, and its notes out over USB MIDI.
- **MIDI**: no sound, notes out over USB MIDI only, for a computer or another synth.
- **DRUMS**: the drum kit ([Drums basics](drums.md)).

Each track keeps its own SCALE, chords and ARP.

## The CLIPS page

Press <kbd>SEQ</kbd>. LOOP's CLIPS page shows the tracks as rows and the scenes as columns.

![CLIPS](../images/LoopClips87-2x.png)

- A clip playing is filled; a clip there but stopped is framed; an empty place is a dot.
- KNOB 1 sets the selected track's TYPE; KNOB 2 picks the SCENE.
- **On this page the keys launch**: white keys 1 to 8 launch this scene's clips on tracks 1 to 8 (again: stop), black
  keys 1 to 8 launch scenes A to H.

A launch waits for the next bar, so everything stays in time.

## Record a clip

1. Pick a track and a scene with an empty place.
2. Tap <kbd>REC</kbd>. Stopped, a bar of count-in clicks plays, then recording starts. (Running, it starts at the next
   bar.)
3. Play.
4. Tap <kbd>REC</kbd> again. The take ends, rounded to whole bars, and loops at once.

Tap <kbd>REC</kbd> on a clip that has notes to **overdub**: each pass adds to it until you tap REC again. Knob turns
are recorded too: turn CUTOFF while recording and the clip plays the move back.

> **Tip:** played something good without recording? Press <kbd>REC</kbd> twice: **TAKE** makes a clip on the selected
> track, in the scene picked, from what you just played.

## Clips by hand

- <kbd>SEQ</kbd> + <kbd>OCT+</kbd> makes an empty clip of one bar in the scene picked. Keep SEQ held and turn PRESETS
  to set its length.
- <kbd>SEQ</kbd> + <kbd>OCT−</kbd> asks DELETE? for the clip there; again deletes it.
- SELECT to **CLIP**: the clip as a piano roll (NOTES, NOTE+), its LENGTH and SPEED (CLIP), and TOOLS to copy, paste,
  double, halve and clear.

## The launch pad

Hold <kbd>SEQ</kbd> for the **launch pad**: white keys 1 to 8 launch the tracks' clips, white keys 9 to 16 the scenes,
black keys 1 to 8 mute tracks 1 to 8, and the last three are STOP ALL, SOLO and UNDO PASS (the last overdub taken
back).

![The launch pad, SEQ held](../images/LoopPad-2x.png)

## Mix, song, projects

- **MIXER** (ALGORITHM from CLIPS): each track's VOLUME, PAN, MUTE and SOLO. HOME's MIX pages set the volumes too.
- **SONG**: up to sixteen slots, each a scene and how many times it repeats. SONG ON plays them in order.
- **Projects**: four. Press <kbd>SEQ</kbd> twice for the list; PRESETS picks one, <kbd>SAVE</kbd> twice loads it (the
  one loaded is saved first). A project keeps the tracks, clips, scenes, SONG and the drum kit, and is saved by itself
  when the transport is stopped and nothing has sounded for three seconds.

<kbd>PLAY</kbd> stops everything; press it again and the clips that were on play from their start.

More: the Manual's [The looper](../manual/looper.md).

Next: [Drums basics](drums.md).
