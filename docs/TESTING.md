# Strata testing

What is tested, how, and what the tests found. Status words as in [ARCHITECTURE.md](ARCHITECTURE.md):
**verified on host** (the real DSP sources built with the PC's compiler and checked offline), **verified in emulator**,
**needs hardware test**. Nothing here has run on an FM-1 yet: every figure about the device is an estimate until the
BENCH page runs on a unit.

## 1. Running the tests

From the repository root, on Linux or WSL2 (python3 with numpy, a C compiler):

| Command | What it does |
| --- | --- |
| `tests/sy_tests.sh` | every host test (§2); WAV demos of the test sounds into `build/sy_demo/`. `QUICK=1` checks aliasing on every fourth note only |
| `GOLDEN_UPDATE=1 build/host/sy_engine_test` | records new golden renders, for an intended change of sound only |
| `GOLDEN_RAW=DIR build/host/sy_engine_test`, then `tests/sy_golden_diff.py A B` | the golden renders as float before (A) and after (B) a change, and how far it moved them (dB against the sound) |
| `tools/costprobe/sy_cost.sh` | the target compiler's instructions in each kernel's inner loop (§4.1); needs the JieLi toolchain |
| `tools/costprobe/sy_dyn.sh` | the cost model: the whole engine's instructions a sample on the target in nine scenarios, and the CPU budget (§4.2) |

## 2. Verified on host

76 checks in five programs (`tests/sy_*.c`), all passing.

### 2.1 Wavetables (`sy_wt_test`, 8 checks)

The C generator (the one the FM-1 runs) against `tools/gen_wavetables.py`'s reference renders of all 54 factory tables:
within 3 LSB (0.25 LSB rms). Every mip level band-limited to its harmonics (under 1 LSB rms above them), no DC (mean
under 1 LSB), guard samples equal sample 0, every table's peak at 0.95 of full scale, a frame's levels agree on the
harmonics they share, and the level choice keeps the top harmonic at or under 21 kHz. A table builds in 0.6 ms on the
host (1.4 ms the slowest); on the device: **needs hardware test**.

### 2.2 Oscillator (`sy_osc_test`, 6 checks)

| What | Result |
| --- | --- |
| Pitch, A0..C8 | within 0.0003 cent of equal temperament at the codec's 44,117.6 Hz |
| Unison 1..16 voices, random phases | power within 0.92 dB of one voice; detune ratios symmetric about the note |
| Level hysteresis | a 5 Hz, ±30 cent vibrato across a level boundary: 0 level changes in 3 s |
| Aliasing, every table, C0..C8, positions 0, ½, 1 (inharmonic energy under the total) | smooth tables: worst −41.1 dB (CHAOS B), saws −48 dB; bright tables: worst −27.7 dB (BAND SWEEP), most −40 to −50 dB |
| Aliasing of the warps, 30 % on a saw and a vowel table, up to C5 | SYNC −42.5, BEND −50, PWM −34, ASYM −37, MIRROR −33 dB on the saw (worse at 100 % and above C5: §3) |

The aliasing checks are regression limits at today's figures (smooth −40, bright −27, warps −20 dBc), not the targets
of ARCHITECTURE.md §18 (−60 / −40 dBc): §3 says what is left.

### 2.3 Filter (`sy_filter_test`, 13 checks)

| What | Result |
| --- | --- |
| LP12 / HP12 at the cutoff, 100 Hz..10 kHz | −3 dB within 0.02 dB (the prewarp holds the tuning) |
| Slopes | LP12 two octaves up −24.5 dB, HP12 two down −24.1 dB; LP24 / HP24 −3.01 dB at the cutoff, −49.1 / −48.2 dB two octaves out |
| BP12, NOTCH, PEAK | BP 0.00 dB at the centre, NOTCH −131.6 dB, PEAK +12.04 dB |
| Resonance 100 % | LP12 +34.0 dB at the cutoff |
| LADDER | passband −0.18 dB, −50 dB two octaves up, −26 dB more the next octave |
| FORMANT (vowel A) | peaks at 600 Hz (+6 dB) and 1,040 Hz over the 3.5 kHz range (−17 dB) |
| VAR 100 % | left −12.3 dB, right −0.3 dB at the cutoff (the spread works both ways) |
| Stability: every type × 5 cutoffs × 3 resonances × 2 drives, the cutoff jumping 20 Hz ↔ 20 kHz every block | never NaN / Inf, largest output 4.9, no denormal state, every state exactly 0 after 40 s of silence (the self-oscillating ladder aside) |
| MIX 0 | the input, bit for bit |

### 2.4 Engine (`sy_engine_test`, 47 checks; `sy_smoke`, 2)

- **Parameters and presets**: 166 parameters; a patch packs into 387 bytes and back unchanged; an older patch (fewer
  values) loads with defaults; out-of-range values clamp; a foreign blob is refused.
- **Matrix**: offsets add, 100 % spans the range, a macro can drive a macro, global destinations take the newest voice's
  sources.
- **Voices**: POLY steals the oldest note that is not the lowest, VOICES limits, sustain, MONO (retrigger, back to the
  held note), LEGATO (pitch only), glide (98 % of an octave in 97 ms for 100 ms), the unison budget (two voices of 2 × 16
  stay within 48 instances), the CPU guard (unison down over 70 %, cheaper effects over 78 %, a voice shed after two
  halves over 85 %, all undone under 55 %).
- **Timing**: ATTACK and RELEASE 100 ms take 100.8 ms; LFO rates free and synced to the tempo.
- **Clicks** (largest sample-to-sample step against a steady sine's): 17 events (note on/off, steal, retrigger, MONO,
  table swap, patch change, cutoff, macro, bend and level jumps, unison 1 → 7, OSC B / SUB / filter on and off, filter
  type, warp change) all under 2×; worst 1.59× (unison 1 → 7).
- **Health**: 40 random patches × 3 s with random notes and knobs: every state finite, no denormal filter state.
- **DC**: the output's mean 1.2 × 10⁻⁵ over the random patches; TUBE drive at 100 % leaves 2.2 × 10⁻⁵ at the output; the
  pulse sub's, PWM's, ASYM's and SYNC's offsets are taken out (means under 2 % of the peak), the infrasonic thump of a
  note's attack and release −41 (ASYM 100 %) to −68 dBFS.
- **Effects**: the OTT's three bands sum back to the input within 3 × 10⁻⁸; a synced delay (1/4 at 120 BPM) lands at
  500.00 ms; every effect at its extremes on full-scale noise stays finite and under 1.0 (the limiter).
- **Determinism and golden renders**: eight test sounds (supersaw, pluck, pad, FM keys, sync lead, ladder bass, vowel pad,
  OTT growl) play the same twice in a run, bit for bit, and as recorded in `tests/sy_golden.txt`.

## 3. Known gaps (targets not met)

- **Aliasing of bright tables**: −27.7 dB worst (BAND SWEEP, TWIN BANDS, WIDE BAND), against a −40 dBc target. The
  linear interpolation's images of the top harmonics fold back; the bright tables are tapered at the top already. Next
  steps, in order of cost: level 1 at 8× oversampling (+34 KiB of RAM: 32 KiB of frames, 2 KiB of the generator's
  scratch), cubic interpolation for these tables only (+50 % for their oscillators).
- **Aliasing of warps** at high amounts and high notes: PWM 100 % reaches −6 dBc at C7, SYNC / MIRROR −15 dBc. The
  remedy is a polyBLEP / polyBLAMP correction at the warps' corners. Not done yet.
- **The oscillator kernel**: 35 listing instructions a sample against the ≤ 28 ARCHITECTURE.md hoped for.
- **The CPU budget**: see §4.3. As built, eight voices with unison and every effect do not fit on one core.

## 4. Cost on the target (estimate)

### 4.1 Kernels (`tools/costprobe/sy_cost.sh`)

The real `sy_*` sources compiled by the JieLi clang (`-O2 -mcpu=r3 -mfprev1`), the instructions in each inner loop
counted from the compiler's listing:

| Kernel | Listing instructions a sample |
| --- | --- |
| Oscillator, one unison voice (held gains / ramping) | plain 35 / 39, warp map 46 / 49, SYNC 62 / 65, FM 40 / 44 |
| Filter, a channel | SVF LP 19, BP 22, NOTCH 23, HP 24, PEAK 24 (24 dB: twice); LADDER 62 and two float divides |
| Sub | triangle 13, saw 25, sine 36, square / pulse / round 57–60 (both edge corrections counted) |
| Noise, a channel | white 21, brown 18, pink 41 |
| Mixing a source into its bus (stereo) | 14 |

Tuning in Phase 2 took the oscillator from 42 to 35 (7 stack reloads to 2) and the LP filter from 29 to 19, with the
sound unchanged bit for bit. The ladder keeps its two divides: Newton steps in their place cost as many instructions and
were up to −18 dB off exact division with the cutoff jumping. What a divide costs on the device is not known.

### 4.2 The whole engine: the cost model (`tools/costprobe/sy_dyn.sh`)

Inner loops are not all of the cost: envelopes, LFOs, the matrix, coefficients and each unison voice's set-up run once
a block, and the effects and the master section run on top. The cost model counts all of it: the engine plays each
scenario on the host with coverage on, and every instruction of the target's listing (compiled with `-g`, so each
carries its source line) is weighed by how often the host ran that line. Code inlined from a function called in several
places takes its call site's share; a loop's set-up code counts once per loop, not per pass. Checks against the
listing: 48 unison instances come to 48 × 35.6 instructions, the filters and the OTT's crossovers to within a few
instructions of their loops.

| Scenario | Instructions a sample | One core at 150 / 205 / 300 instructions a µs |
| --- | --- | --- |
| idle: no voice, no effect | 292 | 9 / 6 / 4 % |
| 1 voice: A × 1 | 480 | 14 / 10 / 7 % |
| 1 voice: A × 16 | 1,205 | 35 / 26 / 18 % |
| 8 voices: A × 1 | 1,794 | 53 / 39 / 26 % |
| 8 voices: A + B × 1, LP12 | 3,558 | 105 / 77 / 52 % |
| 8 voices: A + B × 3, LP24, every effect | 7,023 | 207 / 151 / 103 % |
| 8 voices: A + B × 16 (budget 48), LADDER, every effect | 7,371 | 217 / 159 / 108 % |
| 8 voices: A BEND+ / B SYNC × 1, LP12, SUB, pink NOISE | 5,346 | 157 / 115 / 79 % |
| 1 voice: A × 1, every effect | 2,033 | 60 / 44 / 30 % |

The parts: the engine idle 292 instructions a sample; a voice of A + B with LP12, 408; each unison instance more, 48;
every effect together, 1,552. The rate is the open question: 205 instructions a µs was measured on integer code
(ChoralRoot); this engine's float code has no device figure. Hence three columns.

### 4.3 What fits

At the planned 75 % of a core (less 8 % for the system) and 205 instructions a µs: about 7 voices of A + B with LP12
and no effects, about 3 with every effect on. At 300 a µs, about 10 and 6. The 8-voice, 48-instance design
does not fit one core as built. Where the cost is:

| Part | Instructions a sample | Notes |
| --- | --- | --- |
| Effects | ~1,550 | chorus ~520 (four sines and four floor conversions a sample), reverb ~420, OTT ~260, the lines' int16 conversions ~85, wet/dry blends ~75, delay ~75, drive ~35 |
| Per-voice control and mixing | ~260 a voice | ~45 parameter reads with clamps a block, coefficient and gain work, buffer clearing, the source mixes |
| Engine overhead | ~290 | the smoother walking all 166 parameters every block (~78), the master section (DC blockers, limiter, clamps, the Q15 conversion: ~160) |
| Oscillators and filters | as §4.1 | already lean |

These are the targets of the optimisation pass and the decisions in the Phase 2 checkpoint (cheaper effects, cheaper
control, the second core, fewer voices or a smaller unison budget). The guard (ARCHITECTURE.md §13) keeps the audio
running whatever the patch: it trims unison, then the effects' quality, then voices.

### 4.4 RAM on the target

From the probe object (`build/cost/sy.o`), the engine without the UI:

| Section | Bytes | Largest |
| --- | --- | --- |
| `.pool` | 309,334 | wavetables 186,944 (32 frames × 5,842 B); delay lines 79,872; reverb 20,412; generator scratch 11,684; chorus 4,096 |
| `.bss` | ~19,000 | voices 14,208 (8 × 1,776) |
| `.text` | ~52,000 | code (with the probe's wrappers) |

Felucca's pool is 368,640 B and its screen canvas takes 59,520 B of it: the two together are over by about 200 B,
before the 8 KiB margin `tools/build.py` asks for. Integration has to free at least 8.2 KiB of pool (more if
Felucca's platform keeps other pool buffers): shortening the delay
(905 → 750 ms a side frees 13.4 KiB) or moving the generator scratch (11.4 KiB) to `.bss` are the candidates
(ARCHITECTURE.md §20).

## 5. Needs hardware test

- The instruction rate of this engine's float code, the cost of a float divide, the instruction cache's effect: the
  BENCH page (first integration build) times each kernel and the whole engine with the 24 MHz timer.
- That the engine fits in RAM beside Felucca's platform, and the pool layout links.
- A table's build time (§2.1) and the gap it leaves when a table or a preset changes.
- The codec's output: levels, the −6 dBFS ceiling, DC at the jack.
- Everything the CPU guard does under a real load.
