# MIDI

Strata takes MIDI in over **USB** and the FM-1's **MIDI in jack** (TRS), and sends MIDI out over **USB** only: the FM-1
has no MIDI out jack (its jack is an input). A USB-MIDI host adapter can turn the USB out into DIN or TRS MIDI without a
computer.

## In

- **The channel**: SETTINGS > MIDI > CHANNEL, ANY (default) or 1 to 16. A looper track whose MIDI IN is a channel
  always takes that channel's notes, whatever CHANNEL says.
- **Notes** with velocity, to the selected track (or to the track whose RECORD > MIDI IN is the note's channel). They
  play through that track's chords and ARP as the keys do, and are recorded as the keys are. They are not fitted to
  SCL's scale. On a drum track they play the drums by their General MIDI numbers ([The drums](drums.md)).
- **Pitch bend** (the BEND source; VOICE's BEND UP / BEND DN set its range).
- **Channel aftertouch** (the AFTERTOUCH source).
- **CC 1** mod wheel (the MOD WHEEL source), **CC 11** expression (the EXPRESSION source), **CC 64** sustain (holds the
  releases), **CC 120** and **CC 123**: everything off.
- **CC 0**: the bank a program change loads from.
- **Program change**, with PROGRAM on (below).
- **Clock, start, continue, stop**, with GLO > GLOBAL > CLOCK IN MIDI (below).

### The controllers' default parameters

A controller sets its parameter across the parameter's whole range (0 to 127 mapped from its lowest to its highest
value).

| CC | Parameter | CC | Parameter |
| --- | --- | --- | --- |
| 5 | GLIDE | 21 | OSC A AMOUNT (warp) |
| 7 | VOLUME | 22 | OSC B AMOUNT |
| 16–19 | MACRO 1–4 | 70 | OSC A WT POS |
| 20 | OSC B WT POS | 71 | FILTER RESO |
| 74 | FILTER CUTOFF | 91, 93, 94 | REVERB, CHORUS, DELAY MIX |

The [parameter reference](parameters.md) lists each parameter's CC in its last column.

## MIDI LEARN

![SETTINGS > MIDI](../images/SetMidi-2x.png)

1. SETTINGS > MIDI > LEARN ON ("LEARN: TURN A KNOB"). A banner shows while it listens.
2. Turn a knob on any page: its parameter is picked (the bottom row names it).
3. Move a controller: that CC sets the parameter from then on, in place of what either had ("CC 74: FLT CUTOFF").
4. <kbd>OCT−</kbd> takes the picked parameter's CC away; <kbd>OCT+</kbd> (or LEARN OFF) ends LEARN.

Up to **32** learned CCs, kept with the settings. A learned CC comes before a parameter's default one. **CLEAR**
(turned twice within 2 s) forgets them all. CC 0, 32, 64 and 120 to 127 are never learned. The MIDI page lists the CCs
learned and the default CCs every sound answers.

## Program change

With SETTINGS > MIDI > **PROGRAM** on, a program change loads a preset from the bank CC 0 last chose:

| Bank (CC 0) | Program 0 | 1 .. 7 | 8 |
| --- | --- | --- | --- |
| 0 (the starters) | SUPERSAW | PLUCK, PAD, FM KEYS, SYNC LEAD, LADDER BASS, VOWEL PAD, OTT GROWL | INIT |
| anything else (yours) | USER 1 | USER 2 .. 8 | USER 9 (and so on to USER 64) |

A program pointing at an empty user slot loads nothing. PROGRAM is off by default.

## Clock

GLO > GLOBAL > **CLOCK IN**: INTERNAL (default) or **MIDI**. With MIDI:

- MIDI's clock (24 a quarter note) is the tempo: the BPM shown is it ("BPM: MIDI"), worked out over the last two beats'
  clocks.
- MIDI's **start** runs PLAY from the top (the first clock after it is the downbeat), **continue** runs it on, **stop**
  stops it.
- The clock gone for a second: the panel's BPM again.
- Tap tempo is refused ("MIDI'S CLOCK SETS THE TEMPO").

## Out (USB)

- Each looper track of TYPE **SYNTH+MIDI** or **MIDI** sends what it plays on its **CHANNEL** (TRACK > RECORD; tracks 1
  to 8 start on 1 to 8): the notes, the chords and the bass, the ARP's notes.
- When the computer is not reading (a port is read only while an app has it open) and a note is lost, an All Notes Off
  goes out on that channel once there is room, so no note is left on.
- Not yet: a knob's CC out, and Strata's clock out.

> **Tip:** a MIDI track is a sequencer track for your DAW or another synth: record into it from the FM-1's keys (with
> its chords and ARP), then route the FM-1's USB MIDI port to the instrument on the computer.
