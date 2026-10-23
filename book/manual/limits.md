# Limits

## Counts

| What | At most |
| --- | --- |
| Voices | 8 (VOICE > VOICES sets fewer) |
| Unison copies an oscillator | 16 (within the unison budget, below) |
| Wavetables | 54 factory tables, built in RAM as they load; no tables of your own |
| Matrix slots | 8 |
| FX chain | 8 modules; each type twice, DELAY and REVERB once |
| LFO points | 16 an LFO |
| ARP steps | 16 |
| User presets | 64 (16-character names) |
| Undo | the last 8 states of the sound |
| Learned MIDI CCs | 32 |
| Looper | 8 tracks × 8 scenes; a clip 1 to 16 bars; SONG 16 slots, each up to 16 repeats |
| Events a project | 2,048 notes, hits and knob moves, shared by its clips |
| UNDO PASS | the last pass, up to 256 notes and moves |
| TAKE | the last 256 notes played, of the last 16 bars |
| Projects | 4 |
| Drum kit | 8 rows and DUCK, one kit a project |
| Drum library | 64 drum presets (11-character names), 16 kits (12 characters) |

## The processor

The FM-1 runs Strata on one core. Eight voices of two oscillators and a filter fit it with room to spare; unison and the
whole effects chain together do not, so a **CPU guard** watches each audio block's time and steps in:

1. Above 70 % load, the **unison budget** shrinks: the oldest notes lose unison copies (faded) before any note is
   taken. The budget is 48 oscillator copies sounding at once by default.
2. Above 78 %, the costly effects' quality steps down (OTT's upward compression off, the reverb's density halved).
3. Two blocks above 85 %, a **voice is let go**, faded.
4. Any single block over 90 %, 94 % or 96 % takes those steps at once.

Below 55 % for a second, the steps undo in reverse order. DEBUG > BENCH (CPU) and the console's `status` show the load,
the budget and the voices shed. The drums add about a fifth of a core at their heaviest (eight rows of the costliest
models all sounding); a usual beat sounds two or three rows at once. **Needs a hardware test**: the guard and the drums'
cost under a real load on the FM-1.

The FM-1's **second core** is off by default; DEBUG > CORE 2 measures it ([SETTINGS and DEBUG](settings.md#debug)).
Some FM-1s cannot run it.

## Memory

| | Used | Of |
| --- | --- | --- |
| The firmware (the app area) | 503,348 bytes | 581,564 bytes |
| RAM (`.data` + `.bss`) | 66,980 bytes | 73,728 bytes |

Besides these, a pool of 380,928 bytes holds the wavetables and the effects' delay lines (362,036 bytes in use). These
figures are V0.87's, from the build.

## Known gaps

- **Aliasing**: the brightest tables (BAND SWEEP, TWIN BANDS, WIDE BAND) alias at about −28 dB at worst; warps at high
  amounts on high notes alias more (PWM 100 % at C7 the most).
- **Not yet**: a sound for each synth track (every synth track plays the one sound loaded), inserts and sends for the
  tracks, voices on the second core (all planned for V0.9); factory presets beyond the eight starters; a knob's CC out
  and a clock out over MIDI. The drums do not go through the FX chain, and the kit's knobs are not recorded as moves.
- **Not possible**: MIDI out on the TRS jack (the FM-1's jack is an input only).

## What needs a hardware test

V0.5 ran on an FM-1 (2026-10-17), V0.6 and V0.65 since. Everything from V0.7 to V0.87 is verified in the emulator and
**needs a hardware test**, in particular: the screen's legibility and drawing time, the LEDs' levels, the looper's
timing, the projects' writes, the drums' cost, the battery's thresholds and the screen's sleep, and CORE 2.
