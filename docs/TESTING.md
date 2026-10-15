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
| `tests/sy_wasm.sh` | the host tests built as WebAssembly and run under Node (§6.1); needs Emscripten (`~/emsdk`) and Node 18 |
| `web/strata/build.sh`, then `node web/strata/wb_test.mjs` | the workbench page into `build/strata/`, and its test (§6.2) |
| `web/emu/build_strata.sh`, then `node web/emu/strata_emu_test.mjs` | Strata on the simulated FM-1 into `build/emu_strata/`, and its test (§6.3); `node web/emu/strata_shot.mjs` writes the screen as PNG into `build/emu_strata/shots/` |
| `python3 -m http.server -d build/emu_strata 8000` | the emulator in a browser, at `http://localhost:8000` |
| `node web/emu/strata_browser_test.mjs` | the emulator's one-file page in headless Chrome or Edge under five content security policies (§6.4); on Windows with Windows' Node |

## 2. Verified on host

81 checks in five programs (`tests/sy_*.c`), all passing.

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

### 2.4 Engine (`sy_engine_test`, 52 checks; `sy_smoke`, 2)

- **Parameters and presets**: 166 parameters; a patch packs into 387 bytes and back unchanged; an older patch (fewer
  values) loads with defaults; out-of-range values clamp; a foreign blob is refused; the values read as stored
  (GLIDE, VEL AMP, NOISE STEREO, PREDELAY, the LFOs' DELAY, RISE, SMOOTH) take a change at once, without a patch load.
- **Values as text** (`sy_text.c`, what the knob cards and the workbench show): every list parameter names each of its
  values; every value of every parameter formats, in under 16 characters, with nothing unknown; spot values (the
  cutoff, times, levels, lists, destinations) read as expected.
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
  500.00 ms; every effect at its extremes on full-scale noise stays finite and under 1.0 (the limiter); the chorus on
  a 1 kHz sine is as clean after 10 minutes of running as at the start (−81 / −88 dB away from the tone; the old
  chorus fell to −20 dB, §5.1).
- **Determinism and golden renders**: eight test sounds (supersaw, pluck, pad, FM keys, sync lead, ladder bass, vowel pad,
  OTT growl) play the same twice in a run, bit for bit, and as recorded in `tests/sy_golden.txt` (recorded again after
  the optimisation pass, §5).

## 3. Known gaps (targets not met)

- **Aliasing of bright tables**: −27.7 dB worst (BAND SWEEP, TWIN BANDS, WIDE BAND), against a −40 dBc target. The
  linear interpolation's images of the top harmonics fold back; the bright tables are tapered at the top already. Next
  steps, in order of cost: level 1 at 8× oversampling (+34 KiB of RAM: 32 KiB of frames, 2 KiB of the generator's
  scratch), cubic interpolation for these tables only (+50 % for their oscillators).
- **Aliasing of warps** at high amounts and high notes: PWM 100 % reaches −6 dBc at C7, SYNC / MIRROR −15 dBc. The
  remedy is a polyBLEP / polyBLAMP correction at the warps' corners. Not done yet.
- **The oscillator kernel**: 35 listing instructions a sample while the position moves between frames, against the
  ≤ 28 ARCHITECTURE.md hoped for; 21 with the position on a frame (§4.1).
- **The CPU budget**: see §4.3. Eight voices of two oscillators and a filter fit one core at 205 instructions a µs
  with room to spare; with unison and every effect on, they do not, and the guard trims.

## 4. Cost on the target (estimate)

### 4.1 Kernels (`tools/costprobe/sy_cost.sh`)

The real `sy_*` sources compiled by the JieLi clang (`-O2 -mcpu=r3 -mfprev1`), the instructions in each inner loop
counted from the compiler's listing:

| Kernel | Listing instructions a sample |
| --- | --- |
| Oscillator, one unison voice, the position between frames (gains held / ramping) | plain 35 / 39, warp map 46 / 49, SYNC 62 / 65, FM 40 / 44 |
| The same, the position on a frame (either end, a keyframe, a table of one frame) | plain 21 / 23, warp map 31 / 33, SYNC 45 / 48, FM 24 / 28 |
| Filter, a channel | SVF LP 19, BP 22, NOTCH 23, HP 24, PEAK 24 (24 dB: twice); LADDER 62 and two float divides |
| Sub | triangle 13, saw 25, sine 36, square / pulse / round 57–60 (both edge corrections counted) |
| Noise, a channel | white 21, brown 18, pink 41 |

Tuning in Phase 2 took the oscillator from 42 to 35 (7 stack reloads to 2), added the one-frame kernels (what a
crossfade of exactly 0 or 1 gives, bit for bit, without reading the second frame) and took the LP filter from 29 to
19, the sound unchanged bit for bit. The ladder keeps its two divides: Newton steps in their place cost as many
instructions and were up to −18 dB off exact division with the cutoff jumping. What a divide costs on the device is
not known.

### 4.2 The whole engine: the cost model (`tools/costprobe/sy_dyn.sh`)

Inner loops are not all of the cost: envelopes, LFOs, the matrix, coefficients and each unison voice's set-up run once
a block, and the effects and the master section run on top. The cost model counts all of it: the engine plays each
scenario on the host with coverage on, and every instruction of the target's listing (compiled with `-g`, so each
carries its source line) is weighed by how often the host ran that line. Code inlined from a function called in several
places takes its call site's share; code outside a loop counts once a call at most, and inside one at most as often as
the loop goes round. The listing for the model is compiled with `-fno-unroll-loops`: an unrolled loop's copies would
each carry the whole loop's count (a first version overcounted the filter's state flush six times that way). Without
unrolling the loops cost a little more than in the firmware, so the model errs high. Checks against the listing: 48
unison instances come to 48 × 35.6 instructions, the filters and the OTT's crossovers to within a few instructions of
their loops.

| Scenario | Before the pass | After | One core at 150 / 205 / 300 instructions a µs (after) |
| --- | --- | --- | --- |
| idle: no voice, no effect | 270 | 176 | 5 / 4 / 3 % |
| 1 voice: A × 1 | 458 | 342 | 10 / 7 / 5 % |
| 1 voice: A × 16 | 1,190 | 859 | 25 / 18 / 13 % |
| 8 voices: A × 1 | 1,783 | 1,525 | 45 / 33 / 22 % |
| 8 voices: A + B × 1, LP12 | 3,333 | 2,386 | 70 / 51 / 35 % |
| 8 voices: A + B × 3, LP24, every effect | 6,756 | 4,908 | 144 / 106 / 72 % |
| 8 voices: A + B × 16 (budget 48), LADDER, every effect | 7,100 | 5,227 | 154 / 112 / 77 % |
| 8 voices: A BEND+ / B SYNC × 1, LP12, SUB, pink NOISE | 5,138 | 4,155 | 122 / 89 / 61 % |
| 1 voice: A × 1, every effect | 1,950 | 1,436 | 42 / 31 / 21 % |

Listing instructions a sample. The parts after the pass: the engine idle 176; a voice of A + B with LP12, 276; each
unison instance more, 34 with the position on a frame (as in these scenarios; ~48 while it moves); every effect
together, 1,094 (reverb ~440, chorus ~165, delay ~100, the lines' int16 conversions ~90, OTT ~170, drive ~50). The
rate is the open question: 205 instructions a µs was measured on integer code (ChoralRoot); this engine's float code
has no device figure. Hence three columns.

### 4.3 What fits

At the planned 75 % of a core (less 8 % for the system):

| Voices of A + B with LP12 that fit | 150 / µs | 205 / µs | 300 / µs |
| --- | --- | --- | --- |
| no effects, no unison | 7.6 | 10.6 | 15.9 |
| no effects, 3 unison each | 5.1 | 7.1 | 10.6 |
| every effect, no unison | 3.6 | 6.7 | 11.9 |
| every effect, 3 unison each | 2.4 | 4.5 | 7.9 |

(The position on a frame; a moving one costs ~14 instructions more an instance.) So at 205 a µs: eight voices of two
oscillators and a filter fit; unison and the full rack together do not, and the guard (ARCHITECTURE.md §13) trims
unison, then the effects' quality, then voices. The unison budget's default (48 instances) is set from the BENCH
page's rate at integration; the second core (ARCHITECTURE.md §19) doubles the room.

### 4.4 RAM on the target

From the probe object (`build/cost/sy.o`), the engine without the UI, after the delay went to 750 ms a side:

| Section | Bytes | Largest |
| --- | --- | --- |
| `.pool` | 295,638 | wavetables 186,944 (32 frames × 5,842 B); delay lines 66,176; reverb 20,412; generator scratch 11,684; chorus 4,096 |
| `.bss` | ~20,300 | voices 14,208 (8 × 1,776) |
| `.text` | ~56,700 | code (with the probe's wrappers) |

Felucca's pool is 368,640 B and its screen canvas takes 59,520 B: with the engine, 13,482 B stay free, over the
8 KiB margin `tools/build.py` asks for (if Felucca's platform keeps no other pool buffers: integration confirms).

## 5. The optimisation pass (end of Phase 2)

Each change measured with the cost model, and the golden renders before and after it (`tests/sy_golden_diff.py`).

**Bit for bit** (the renders unchanged): the delay and reverb read their lines without floors or divisions; the
effects' wet/dry blends run inside their loops; the drive has a loop for each curve; the OTT's three bands are
written out; the limiter at rest skips a block under its knee; the master's volume, DC blockers and Q15 conversion
are two loops, not six; the smoother looks for changed values two at a time and only works on what changed or still
glides; the level choice is a binary search; the SVF kernels and the ladder are kept whole (one copy each); the filter
flushes only the states its type uses; the oscillator has its one-frame kernels; zeroing and copying go four words a
turn; the DIRECT bus is cleared and mixed only when a source takes it.

**Not bit for bit** (each measured against the sound, over the whole test phrase):

| Change | The renders moved |
| --- | --- |
| The chorus's read position (a bug fix, §5.1) | supersaw −76 dB, pad −72 dB |
| The chorus's LFO a rotating sine pair (set from the phase each block) and its DELAY / DEPTH ramped | −80 / −72 dB |
| The reverb's drift worked out at the block's ends and drawn straight between (0.27 Hz: under 10⁻⁶ samples off) | −81 / −73 dB |
| The oscillators added straight into an integer bus a route, their level, pan and fades in the unison gains (rounded to 1/32768); the filter in place when it is all wet | −71 to −90 dB, every sound |

Altogether, against the renders recorded before the pass: −71 dB (pad) to −90 dB (ladder bass), the largest sample
difference 2.4 × 10⁻⁴. The reverb's and the chorus's lines are int16: any difference, however small, turns some of their
roundings and goes round the reverb, so the sounds with reverb move at the level of its own rounding noise (−70 dB
of those sounds).

### 5.1 Two bugs found

- **The chorus lost precision the longer it ran.** Its read position was `(float)(count + N) − delay`, the count being
  every sample since power-on: a float has 24 bits, so the fraction that interpolates the line was a 256th of a sample
  after one second, an eighth after 24 s, and the whole position was off after six minutes. The short test renders
  never showed it. The position now comes from the write index (0..1023); a test runs the chorus for 10 minutes and
  checks it is as clean as at the start (the old one: −72 dB to −20 dB).
- **Values read as stored ignored a change until a patch load.** GLIDE (MIDI CC 5), VEL AMP, NOISE STEREO, PREDELAY
  and the LFOs' DELAY, RISE and SMOOTH are not smoothed, and the smoother only refreshed the smoothed ones: a knob or a
  CC changed nothing until the next preset. The smoother now refreshes every changed value (the smoothed ones glide);
  a test changes them while playing.

## 6. In WebAssembly and in the emulator (Phase 3)

The same sources compiled with Emscripten (6.0.12, `-O2 -ffp-contract=off`) and run under Node 18. The emulator's page
has also run in headless Chrome (§6.4); the workbench's page has not been opened in a browser.

### 6.1 The engine as WebAssembly (`tests/sy_wasm.sh`)

The five host programs (§2) built with `emcc` and run under Node: 81 checks, all passing, the golden renders included.
The WebAssembly engine plays the eight test sounds' phrases bit for bit as the host build does (neither fuses a
multiply and an add: the host's baseline x86-64 has no FMA, the WebAssembly is built with `-ffp-contract=off`). So the
workbench and the emulator sound as the host-verified engine does.

### 6.2 The workbench (`web/strata`, `wb_test.mjs`, 5 checks)

One HTML file: the DSP in an AudioWorklet, every control built from the parameter table and shown with `sp_text`, the
on-screen keys, the computer keyboard and Web MIDI, the test sounds and INIT, a patch as a copyable code, a scope. The
test drives its module's exports as the page does: no imports; every test sound plays the engine test's phrase bit for
bit as `tests/sy_golden.txt`; patch codes round trip (the test sounds, INIT) and a foreign one is refused; a load is
taken, a second one held while it fades (2 quanta), then played as sent; values as text.

### 6.3 The app on the simulated FM-1 (`web/emu`, `strata_emu_test.mjs`, 12 checks)

`sy_app.c` with the engine, Felucca's `panel.c` and `gfx.c`, compiled against a simulated FM-1 (`strata_web.c`, after
Felucca's): the clock, the audio halves as the DMA asks for them, the key matrix and its LEDs, the 240 × 240 screen,
an erased flash image. Not simulated: USB, TRS MIDI, the console, the battery, and time within an audio half (the clock
moves a millisecond at a time, so the screen shows `cpu --` there rather than a load). **Verified in emulator**, under
Node:

| Check | Result |
| --- | --- |
| The module | no imports (the worklet gives it none) |
| Boot | draws (79 colours), silent before a key; the first page OSC A, the sound SUPERSAW |
| A key | plays (peak 0.101, one voice); its release ends it (no voice after 1.5 s) |
| LEDs | the key held and the page's family button (EDIT) lit |
| KNOB 2 | turns the page's second parameter, the table (1 → 4 for three detents) |
| FX, twice | DRIVE, then CHORUS (a family button cycles its sections) |
| SELECT, PRESETS | the next page; the next sound (PLUCK) |
| Web MIDI | a note plays and its note-off ends it |
| MIDI CC | CC 0 (bank select) at 0 and 127 sets nothing; CC 74 sets the cutoff, and only it |
| Determinism | the same input twice: the same sound, bit for bit |

The screen was checked by eye from `strata_shot.mjs`'s PNGs (the header, the four knob cards with their values and
gauges, the footer). **A bug the test found**: the table's CC 0 means "no CC", so a CC 0 message (bank select) set
the first parameter without one (OSC A's ON); the lookup now skips CC 0, and the lookup before the fix fails the
check.

### 6.4 The emulator's page in a browser (`strata_browser_test.mjs`, 6 checks)

`build_strata.sh` also writes the page as one file (`make_page_strata.py`): the module as base64, the font as a data:
URI, the worklet's helpers declared in the page. The worklet loads from `worklet.js` beside the page or, where that
is refused or missing, from its source in the page as a Blob, so the page works under either kind of policy a
publishing host may set. Headless Chrome 155 (its own profile, audio muted), from a local server:

| Policy | Result |
| --- | --- |
| none; scripts from the page's origin only (no blob:); blob: only (`worklet.js` refused); no `worklet.js` (404) | each starts (the screen draws, so the worklet runs), the lowest key lights its LED and its release puts it out, no sideways scroll |
| the content alone, in a skeleton like the publishing host's, origin-only scripts | the same; at 400 px wide the tall layout, no sideways scroll |

The only errors are the refusals the policies make. Not covered: the sound itself (muted; the Node tests check it),
Web MIDI, and the publishing host's own policy, which is not known here.

## 7. Needs hardware test

- The instruction rate of this engine's float code, the cost of a float divide, the instruction cache's effect: the
  BENCH page (first integration build) times each kernel and the whole engine with the 24 MHz timer.
- That the engine fits in RAM beside Felucca's platform, and the pool layout links.
- A table's build time (§2.1) and the gap it leaves when a table or a preset changes.
- The codec's output: levels, the −6 dBFS ceiling, DC at the jack.
- Everything the CPU guard does under a real load.
- The app on the device (after integration): the audio interrupt with its event rings, the load it reports, and the
  keys' latency while the screen redraws. The skeleton redraws the whole screen on any change (~77 ms on the panel by
  Phase 0's figure) and reads the keys in the main loop, so a key could wait that long; the UI phase draws only what
  changed.
