# Limits

| What | Limit |
| --- | --- |
| Voices | 8 |
| Unison | 16 copies per oscillator, within a shared budget |
| Wavetables | 54 factory tables |
| Matrix | 8 slots |
| Effects | 8 in the chain; each type twice, DELAY and REVERB once |
| LFO points | 16 |
| ARP steps | 16 |
| Presets | 64 of your own |
| Undo | the last 8 changes |
| MIDI LEARN | 32 mappings |
| Looper | 8 tracks × 8 scenes; clips of 1–16 bars; 16 SONG slots |
| A project | 2,048 notes, hits and moves |
| Projects | 4 |
| Drum library | 64 drum presets, 16 kits |

**The processor.** Eight voices with two oscillators and a filter fit comfortably. Big unison with every effect does
not, so a guard steps in by itself: first the oldest notes lose unison copies, then the costliest effects get lighter,
then a voice is released, each with a fade. Fewer unison copies, fewer effects (REVERB costs most) or fewer VOICES
avoid it. Unconfirmed on hardware: the guard and the drums' cost under a full load.

**Not yet**: a separate sound for each synth track (all synth tracks play the one sound loaded), sending knob CCs or
clock over MIDI, and putting the drums through the effects. MIDI out on the jack isn't possible: the FM-1's jack is an
input only.
