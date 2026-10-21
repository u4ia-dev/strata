# The 8-track looper: decisions and plan

V0.8's sequencer, rebuilt as an 8-track looper, like a DAW. The answers below are the user's (2026-10-21); each
"default" is a choice made to fill a gap, open to change at the renders. What is not built yet is a plan, not a fact:
each stage gets its own section in CONTROLS.md, BUDGET.md and FLASHING.md when it is built.

## Decisions

| Area | Decision |
| --- | --- |
| Tracks | 8. Each picks what it plays: a Strata synth sound of its own, the drum kit, or MIDI only (out on a channel) |
| Recording | Loop recording of notes and knob moves (automation), overdubbing pass after pass |
| Loop length | The first take sets it: REC starts, the next REC tap ends it, rounded to whole bars. Each track keeps its own length (polymetric) |
| Tempo | Stays as set (GLOBAL's TEMPO, or MIDI's clock); takes are rounded to bars at it |
| Click | A click while recording (LEVEL; ON, OFF, REC ONLY) and one bar of count-in before a first take |
| Timing | Free: notes keep their played time (96 a beat), edited on a piano roll that can snap to a grid; QUANTIZE optional |
| Arrangement | Clips and scenes (a clip launcher), plus SONG: a chain of scenes with repeats |
| Keys | They play the selected track. SEQ held: a launch pad (below) |
| Chords and ARP | Per track: each synth or MIDI track its own SCALE, chords and ARP, applied as it plays |
| MIDI | Each track has a channel: notes in on it play and record that track; the panel keys (and notes on no track's channel) play the selected track. MIDI tracks send on their channel over **USB only** (the FM-1 has no TRS MIDI out, ARCHITECTURE.md §15; one question's option said "USB and TRS": that was wrong) |
| Mix and FX | Each track: VOLUME, PAN, MUTE, SOLO, a cheap insert of its own (FILTER, DRIVE or EQ) and a SEND into the shared FX rack |
| Drums | A new Strata drum synth, 8 lanes: KICK, SNARE, CLAP, HAT CL, HAT OP, CYM as dedicated models; WT 1, WT 2 a flexible voice on Strata's wavetables |
| A hit | Velocity and ACCENT; CHANCE and RATCHET (2..4 fast repeats) |
| Sidechain | Ghost steps only (silent hits on the drum track) duck every non-drum track by one shared AMOUNT, with ATTACK, RELEASE and SHAPE |
| Projects | 4 projects (8 sounds, the drum kit, clips, scenes, the song); the user preset slots go from 128 to 64 to make room |
| CPU | The second core, with the fallback to one core that Felucca's `fm1_cpu1.h` has |
| Wavetables | One shared bank in RAM: up to 32 frames (as now), split into tables every synth track's oscillators pick from; each track keeps its own filter, envelopes, LFOs, matrix, unison and insert |
| Delivery | Staged builds, each with renders and emulator tests; a bench build for the hardware first |

**Wavetables from flash: not possible** (found after the question was answered). The 54 factory tables are not
stored anywhere: Strata builds them from recipes into RAM (`sy_wtgen.c`). Stored ready to play, their 864 frames
would take 5,047,488 B (5,842 B a frame, its nine levels), and the FM-1's whole flash is 1 MiB, of which 189,560 B of
the app slot are free: about 32 frames, two tables. So the answer's fallback, the shared bank in RAM, is the design;
the bench build no longer measures flash reads.

## Defaults (not asked; change them at the renders)

- 8 scenes, A to H: each track one clip slot per scene, 64 clip slots a project. A launch waits for the next bar
  (LAUNCH: BAR, BEAT, NOW).
- The launch pad, SEQ held: white keys 1..8 launch the clip at the cursor's scene on tracks 1..8 (and select that
  track); white keys 9..16 launch scenes A..H; black keys 1..8 mute tracks 1..8; black keys 9..11 STOP ALL, SOLO (the
  selected track), UNDO PASS.
- UNDO PASS removes the last overdub pass of the selected track's clip; a second tap puts it back.
- A clip is at most 16 bars. Its notes and moves share the project's event memory (about 16 KB of RAM, roughly 2,000
  notes in all); the screen says how full it is.
- EDIT, ENV, LFO and FX follow the selected track: its sound, or the drum kit on the drum track. FX shows the rack,
  the selected track's insert and its SEND.
- The ghost steps are a ninth row on the drum track, DUCK, under the eight drums.

## Hardware limits this design keeps

- RAM: about 48 KiB free (V0.8), V0.8's patterns (13.5 KB of the pool) given back. The wavetables are 186,944 B
  of RAM and stay one bank.
- Flash: the projects go in the allow-listed user store (`0x97000`..`0xDFFFF`), with the compile-time checks and the
  flash driver's runtime guard as now; nothing is written outside it.
- CPU: eight voices with every effect fill one core; several sounds at once need the second core, which some FM-1s
  cannot run (fm1-x0x#10). On those the guard trims (unison, effect quality, voices) as it does now.

## Stages

0. **Bench build** (V0.8 BENCH): the second core on the user's FM-1, measured. BENCH gets CORE 2: core 1 renders
   wavetable oscillators while core 0 plays; the page shows whether it answered and stayed right (bad reads, give-ups),
   its time against core 0's for the same work, how much core 0 slowed while it ran, and its stack. No supply raise.
   Flashed by the user after a full backup, when they choose.
1. **V0.8, the looper**: 8 tracks (SYNTH: every synth track plays the current sound until V0.9; MIDI), clips and
   scenes, SONG, loop recording with the first take setting the length, the click and count-in, overdub and UNDO
   PASS, automation, the piano roll, per-track chords and ARP, per-track MIDI channels, VOLUME, PAN, MUTE, SOLO, the
   launch pad, 4 projects (presets to 64).
2. **V0.85, the drums**: the drum synth (6 models, 2 wavetable lanes), the drum track's grid view, ACCENT, CHANCE,
   RATCHET, the DUCK row and the sidechain.
3. **V0.9, the sounds**: a sound per synth track, the shared wavetable bank, inserts and sends, the second core
   (as the bench found it).
