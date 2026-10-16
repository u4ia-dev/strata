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

114 checks in six programs (`tests/sy_*.c`), all passing.

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

### 2.4 Engine (`sy_engine_test`, 73 checks; `sy_smoke`, 2)

- **Parameters and presets**: 180 parameters; a patch packs into at most 610 bytes (the LFOs' drawn shapes included)
  and back unchanged; an older patch (fewer values, or no drawn shapes) loads with defaults; out-of-range values clamp;
  a malformed drawn shape is not read (too many points) or made whole (places out of order, values out of range); a
  foreign blob is refused; the values read as stored (GLIDE, VEL AMP, NOISE STEREO, PREDELAY, the LFOs' DELAY, RISE,
  SMOOTH) take a change at once, without a patch load.
- **Values as text** (`sy_text.c`, what the knob cards and the workbench show): every list parameter names each of its
  values; every value of every parameter formats, in under 16 characters, with nothing unknown; spot values (the
  cutoff, times, levels, lists, destinations) read as expected.
- **Matrix**: offsets add, 100 % spans the range, a macro can drive a macro, global destinations take the newest voice's
  sources.
- **Voices**: POLY steals the oldest note that is not the lowest, VOICES limits, sustain, MONO (retrigger, back to the
  held note), LEGATO (pitch only), glide (98 % of an octave in 97 ms for 100 ms), the unison budget (two voices of 2 × 16
  stay within 48 instances), the CPU guard (unison down over 70 %, cheaper effects over 78 %, a voice shed after two
  halves over 85 %, all undone under 55 %).
- **Timing**: ATTACK and RELEASE 100 ms take 100.8 ms; LFO rates free and synced to the tempo (SYNC FIXED, 1/4 at
  120 BPM: 20 cycles in 10 s). RATE's modulation in FIXED steps the note value (1/4 + 25 %: 1/8T, 60 cycles in 10 s).
  The time signature: 1 BAR is 3 quarter notes in 3/4 and 6/8, 1.75 in 7/16; BPM counts TS-NOTE beats (120 in 6/8: a
  quarter note a second, a FIXED 1/4 LFO 10 cycles in 10 s).
- **LFO shapes as points** (the start of a drawing): every built-in shape's points are whole, and the table is read to
  its end; the points' curves lie on the shapes exactly (within 0.0004 of the ±1 range) but for SINE (within 0.053: no
  one curve follows a quarter sine closer) and BOUNCE (0.105, its first bounce twice as tall). A drawn pulse, high for
  30 % of its cycle, plays high for 30.1 % of 10 s; an edit of a drawn shape is not taken while under way and is taken
  at the next block when done; a patch load takes its drawn shapes.
- **Clicks** (largest sample-to-sample step against a steady sine's): 17 events (note on/off, steal, retrigger, MONO,
  table swap, patch change, cutoff, macro, bend and level jumps, unison 1 → 7, OSC B / SUB / filter on and off, filter
  type, warp change) all under 2×; worst 1.59× (unison 1 → 7). The hush for a SAVE's flash write (a 220 Hz sine):
  out 0.96× and back 1.00× the steady step; "hushed" after 16 blocks, the last 256 samples (both audio halves, the ring
  the DMA loops while an erase stops the interrupts) silent.
- **Health**: 40 random patches × 3 s with random notes and knobs: every state finite, no denormal filter state.
  REVERB's PLATE (four lines) with the stack filled with NaN first: silence in, silence out (§5.3).
- **DC**: the output's mean 1.2 × 10⁻⁵ over the random patches; TUBE drive at 100 % leaves 2.2 × 10⁻⁵ at the output; the
  pulse sub's, PWM's, ASYM's and SYNC's offsets are taken out (means under 2 % of the peak), the infrasonic thump of a
  note's attack and release −41 (ASYM 100 %) to −68 dBFS.
- **Effects**: the OTT's three bands sum back to the input within 3 × 10⁻⁸; a synced delay (1/4 at 120 BPM) lands at
  500.00 ms, and TIME's modulation steps its note value (−25 %: 1/8T, 166.85 ms); every effect at its extremes on
  full-scale noise (the EQ's bands at +18 dB) stays finite and under 1.0 (the limiter); the chorus on a 1 kHz sine is
  as clean after 10 minutes of running as at the start (−66 / −64 dB away from the tone at its default 0.50 Hz; −81 /
  −88 dB when it ran at a quarter of that, §5.2; the old chorus fell to −20 dB, §5.1). FX ORDER: each of the 720
  codes is an order of the six and comes back the same; the modules play in the patch's order (OTT, EQ, REVERB, DRIVE,
  DELAY, CHORUS: the same, bit for bit, as calling them so); a new order mid-note dips the wet parts without a click
  (the largest step under 2× the steady output's).
- **EQ** (the shelves at their own Q, 0.707 by default; the peak at MID Q): on and flat, the input bit for bit; each
  band's gain measured on a sine against its design: the peak's centre
  ±12.00 dB, the low shelf's far side +11.97 dB (30 Hz), the high shelf's −11.99 dB (16 kHz), halfway at FREQ (+6.00,
  −6.00 dB), flat away from the bands (0.00; the peak 0.16 dB a decade off, as a Q of 1 has it); against the curve the
  screen draws (`fx_eq_db`), within 0.05 dB everywhere (0.00 measured); every frequency jumping 20 Hz ↔ 20 kHz each
  block with the bands at +18 dB and the peak at Q 8: finite, largest output 3.5 on noise at 0.1; its states exactly 0
  after 10 s of silence.
- **Determinism and golden renders**: eight test sounds (supersaw, pluck, pad, FM keys, sync lead, ladder bass, vowel pad,
  OTT growl) play the same twice in a run, bit for bit, and as recorded in `tests/sy_golden.txt` (recorded again after
  the optimisation pass, §5, and after the three effects' fixes, §5.2: supersaw, pluck, pad and sync lead).

### 2.5 Store (`sy_store_test`, 12 checks)

`sy_store.c` (Strata's presets, settings and sound on the flash) against a simulated NOR that behaves as the part does
(an erase sets 0xFF, a program only clears bits, a page does not wrap) and refuses anything outside the firmware's
allow-list (`hal/fm1_flash_map.h`, the hooks as `storage_hw.c` has them): an empty flash loads nothing; saved and
loaded back; saved again to the other copy, the newest winning; a write torn after any byte of the payload or the
record leaves the copy before; a part that ignores writes is not reported saved (the read-back) and the old copy
loads; Felucca's record (`FELU`) in the same sector reads as none; all 128 user slots round trip a packed patch, a slot
written keeps its pair's other slots, a slot emptied reads empty; every object's sectors inside the allow-list (no
erase or program refused), and the hooks refuse the loader's area and the stock firmware's backup.

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
| EQ, a band a channel | steady 21, its coefficients ramping 32 (a band at 0 dB: none) |

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

| Scenario | Before the pass | After | Now, with the EQ | One core at 150 / 205 / 300 instructions a µs (now) |
| --- | --- | --- | --- | --- |
| idle: no voice, no effect | 270 | 176 | 182 | 5 / 4 / 3 % |
| 1 voice: A × 1 | 458 | 342 | 352 | 10 / 8 / 5 % |
| 1 voice: A × 16 | 1,190 | 859 | 866 | 25 / 19 / 13 % |
| 8 voices: A × 1 | 1,783 | 1,525 | 1,563 | 46 / 34 / 23 % |
| 8 voices: A + B × 1, LP12 | 3,333 | 2,386 | 2,423 | 71 / 52 / 36 % |
| 8 voices: A + B × 3, LP24, every effect | 6,756 | 4,908 | 5,116 | 150 / 110 / 75 % |
| 8 voices: A + B × 16 (budget 48), LADDER, every effect | 7,100 | 5,227 | 5,436 | 160 / 117 / 80 % |
| 8 voices: A BEND+ / B SYNC × 1, LP12, SUB, pink NOISE | 5,138 | 4,155 | 4,191 | 123 / 90 / 62 % |
| 1 voice: A × 1, every effect | 1,950 | 1,436 | 1,626 | 48 / 35 / 24 % |

Listing instructions a sample. The parts now: the engine idle 182; a voice of A + B with LP12, 280; each unison
instance more, 34 with the position on a frame (as in these scenarios; ~48 while it moves); every effect together,
1,275 (reverb ~440, chorus ~165, delay ~100, the lines' int16 conversions ~90, OTT ~170, drive ~50, the EQ's three
bands steady ~180). "Every effect" has the EQ's three bands on (±3 dB); a band at 0 dB costs nothing. The rate is the
open question: 205 instructions a µs was measured on integer code (ChoralRoot); this engine's float code has no device
figure. Hence three columns.

### 4.3 What fits

At the planned 75 % of a core (less 8 % for the system):

| Voices of A + B with LP12 that fit | 150 / µs | 205 / µs | 300 / µs |
| --- | --- | --- | --- |
| no effects, no unison | 7.5 | 10.5 | 15.6 |
| no effects, 3 unison each | 5.0 | 7.0 | 10.5 |
| every effect, no unison | 2.9 | 5.9 | 11.1 |
| every effect, 3 unison each | 2.0 | 4.0 | 7.4 |

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
8 KiB margin `tools/build.py` asks for (if Felucca's platform keeps no other pool buffers: integration confirms). The
UI's undo history (8 patches of 610 B: 4,880 B), HOME's scope (its capture and the UI's copy: 1.7 KB), the display's
row hashes (456 B), the store's buffer (3,840 B), the user slots' index (2,432 B) and a SAVE's packed patch (~630 B)
are in `.bss`, not the pool: about 14 KB, where Felucca's engines, sequencer and song freed far more `.bss`, but
integration confirms the room.

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

### 5.2 Three more, found drawing the effects (Phase 4)

Drawing each effect from its parameters (the FX pages, `sy_ui_fx.c`) meant working out what each knob does to the
sound, and three did not do what they show. All three were in the engine's first commit; each is fixed and the golden
renders of the sounds that use them re-recorded:

- **The chorus ran at a quarter of its RATE**: its LFO's increment had a stray × 0.25, so RATE 0.12 Hz swept at
  0.03 Hz. It runs at RATE now; RATE's default is 0.50 Hz (it was 0.12 shown, 0.03 played). SUPERSAW and PAD.
- **The delay's HP cut at a tenth of its frequency** (56 Hz shown, 5.6 Hz cut) and **the reverb's LOW CUT at a
  fortieth** (80 Hz shown, 2 Hz cut: none). Both one-poles now sit where their knobs say, as the delay's LP did.
  SUPERSAW and PAD (the reverb), PLUCK and SYNC LEAD (the delay).

### 5.3 One more, found by the effects' order (Phase 4)

- **REVERB's PLATE read four lines it never wrote.** The network mixes eight lines for HALL and four for PLATE, but its
  output sum always read all eight, `y[4..7]` included, which PLATE leaves unset: whatever the stack held there went
  into the output. The fixed order hid it (the stack happened to hold small finite values); the health test caught it
  once the order changed what ran before the reverb. PLATE now sums its own four lines; HALL is unchanged bit for bit.
  A test fills the stack with NaN before PLATE runs on silence and asks for silence out (it fails on the old code).

## 6. In WebAssembly and in the emulator (Phase 3)

The same sources compiled with Emscripten (6.0.12, `-O2 -ffp-contract=off`) and run under Node 18. The emulator's page
has also run in headless Chrome (§6.4); the workbench's page has not been opened in a browser.

### 6.1 The engine as WebAssembly (`tests/sy_wasm.sh`)

The six host programs (§2) built with `emcc` and run under Node: 114 checks, all passing, the golden renders included.
The WebAssembly engine plays the eight test sounds' phrases bit for bit as the host build does (neither fuses a
multiply and an add: the host's baseline x86-64 has no FMA, the WebAssembly is built with `-ffp-contract=off`). So the
workbench and the emulator sound as the host-verified engine does.

### 6.2 The workbench (`web/strata`, `wb_test.mjs`, 5 checks)

One HTML file: the DSP in an AudioWorklet, every control built from the parameter table and shown with `sp_text`, the
on-screen keys, the computer keyboard and Web MIDI, the test sounds and INIT, a patch as a copyable code, a scope. The
test drives its module's exports as the page does: no imports; every test sound plays the engine test's phrase bit for
bit as `tests/sy_golden.txt`; patch codes round trip (the test sounds, INIT, an LFO's drawn shape) and a foreign one
is refused; a load is taken, a second one held while it fades (2 quanta), then played as sent; values as text. The
workbench draws no LFO shape: its codes keep the drawn shapes they bring, and SHAPE DRAWN plays them.

### 6.3 The app on the simulated FM-1 (`web/emu`, `strata_emu_test.mjs`, 63 checks)

`sy_app.c` and the UI (`sy_ui.c`, `sy_ui_draw.c`, `sy_ui_fx.c`, `sy_ui_save.c`, `sy_gfx.c`) with the engine, `sy_store.c`,
Felucca's `panel.c` and `gfx.c`, compiled against a simulated FM-1 (`strata_web.c`, after Felucca's): the clock, the
audio halves as the DMA asks for them, the key matrix and its LEDs, the 240 x 240 screen (the pixels each blit sends
counted), a flash image behind the firmware's allow-list (the page keeps it across reloads). Not simulated: USB, TRS
MIDI, the console, the battery, time within an audio half (no CPU load to show), the panel's speed (a region is drawn
and sent at once) and the flash's (a write takes no time). **Verified in emulator**, under Node:

| Check | Result |
| --- | --- |
| The module | no imports (the worklet gives it none) |
| Boot | draws (260 colours), silent before a key; the first page OSC A WAVE, the sound SUPERSAW |
| Idle | nothing changed, nothing drawn (0 blits in 0.2 s) |
| A key | plays (peak 0.102, one voice); its release ends it |
| LEDs | the key held and the page's family button (EDIT) lit |
| The WAVE page | KNOB 1 WT POS, KNOB 2 TABLE, KNOB 3 WARP, KNOB 4 AMOUNT |
| KNOB 2 (a list) | the table one entry a detent (1 -> 4 for three); 3 regions redrawn, not the screen |
| KNOB 1 (a value) | speed-sensitive: 3 slow detents move WT POS 0.1 % each, 6 quick ones 25.1 % |
| FX, ALGORITHM, SELECT | DRIVE; the next effect (CHORUS); the next page |
| PRESETS | the next sound (PLUCK) |
| The MOD layer | REC held opens it; a white key (A3) picks LFO 1 and plays nothing; KNOB 1 sets LFO 1's depth on WT POS in a matrix slot (5 slow detents: 1.0 %); released, it closes, the slot stays |
| .. its leftovers | a slot it made and turned back to 0 is freed when it closes |
| REC's gestures | a tap does nothing, a double tap latches the layer, a tap then lets it go |
| The SETTINGS | HOME held: a countdown from the hold time ("HOLD: SETTINGS IN 4"); let go at 1.5 s, nothing opens, the footer cleared; held 5 s: the SETTINGS; KNOB 1 changes the palette and the screen with it (26,085 pixels); the eighth, MONO: not a pixel outside the window that is not grey |
| OCT with EDIT held | the section's pages: OCT+ WAVE -> UNISON, OCT- back to WAVE, OCT- again round to OUTPUT; EDIT let go: no tap (the page stays); the keys' octave unchanged |
| OCT with ENV held | OCT+ steps the section shown (OSC A); ENV held on past the hold time: no ENV 2 layer; let go: no tap |
| OCT alone | the keys' octave +1 and back; the page stays |
| An LFO's POINTS page | LFO 1's first page; KNOB 1-4 the point, its place, its level, the curve |
| .. KNOB 1 | picks a point, round (+2: point 3; -3: point 5 of 5); the shape untouched (SINE) |
| .. KNOB 3 | lowers point 2 (1000 -> 990 for 5 slow detents): the sine's 5 points become the drawn shape (SHAPE DRAWN); 2,032 pixels change |
| .. KNOB 2, KNOB 4 | point 2 along the cycle (250 -> 253 slow; quickly no further than the next point, 500); the last point stays at the end; KNOB 4 on it curves the segment into it (+0.8 % for 4 slow detents) |
| .. LFO twice | a point left of the one picked, halfway (625 between 500 and 750), on the curve, picked; the page stays POINTS. The first point picked: the new one right of it (250, halfway to 500) |
| .. not | LFO twice from another family's page: no point (the LFO's next page); STAIRS shown (16 points): no 17th; the MOD layer: KNOB 3 changes nothing |
| .. LFO held | from the hold time a countdown ("HOLD: DELETE POINT 3 IN 4"); let go at 2 s: nothing deleted, the footer cleared; held 5 s: point 3 goes (STAIRS drawn, 15 points), point 2 picked, the page stays; the first point picked: "THE END POINTS STAY"; a knob turned during the countdown stops it |
| Undo, redo | SAVE + OCT− twice takes back a level KNOB 3 set and the delete (SHAPE STAIRS, the drawn shape of 7 points); SAVE + OCT+ redoes the delete; SAVE let go: no page, no octave. On WT POS a gesture at a time (600 -> 606 in one, 607 a second later: undone 606, then 600; redone 606); a sound's load undone (PAD back to PLUCK) |
| SYNC FIXED | LFO 1 SHAPE: SYNC FIXED, KNOB 3 steps the note value (1/4 -> 1/8D), RATE untouched; FREE: KNOB 3 the rate in Hz, the note value kept. DELAY: FIXED by default, KNOB 3 steps its note (1/8D -> 1/8); FREE: the time in ms |
| GLOBAL | GLO tapped three times: the TEMPO page, 120 BPM 4/4; BPM 3 slow detents 123, held between 20 and 300; TS-BEAT 1-16; TS-NOTE 1, 2, 4, 8, 16; 300 BPM in eighths: 150 quarter notes a minute |
| FX | ALGORITHM through six effects (DRIVE .. EQ), round; the EQ's pages POINTS (POINT, FREQ, GAIN, Q) and GAINS (LOW, MID, HIGH, ON) |
| A knob turning | EQ POINTS, MID picked: KNOB 3 its gain, its cell lit, the curve before the turn kept (4,430 pixels); KNOB 4 its Q; LOW picked: LO Q; a second after the last detent both let go (2,813 pixels back) |
| .. the MOD layer there | KNOB 3 sets LFO 1's depth on MID's gain (the point picked) |
| The effects' order | FX held + ALGORITHM on DRIVE: DRIVE second ("DRIVE: 2 OF 6", FX ORDER 120); FX let go: no page step; ALGORITHM then follows the new order (after DRIVE: DELAY); two back: CHORUS first |
| Live only with sound | CHORUS on: its taps still in a second of silence (0 blits), moving with a note (7 in 0.5 s), still again after it |
| HOME | silence: nothing redrawn in a second (the flat capture drawn once); a note playing: the scope live (12 captures in 0.5 s, its rows sent 10 times); on another page no capture |
| The WAVE page's warp | WARP BEND+, KNOB 4 turned quickly (AMOUNT 55.1 %): the table's frames drawn warped (7,766 pixels of the display), the frame before the turn kept |
| SAVE once | a starter sound (PLUCK) saved at once as "PLUCK v2" in USER 1 ("SAVED: PLUCK v2 · USER 1"); the page stays; flash written, none refused. Once more: "PLUCK v3" in USER 2, USER 1 kept |
| SAVE held | a countdown ("HOLD: SAVE OVER USER 2 IN 3" at 2.3 s); let go then: USER 2 untouched; held 5 s: over USER 2 ("SAVED OVER USER 2"), its stored WT POS the sound's |
| .. with OCT | SAVE held + OCT−: undo (KNOB 1's edit taken back); held on past 5 s: USER 2 not saved over |
| .. a starter sound | held and let go: "NOTHING TO SAVE OVER: TAP SAVE", nothing written |
| SAVE twice | the browser, nothing saved; KNOB 2 loads as it turns (USER 1, then USER 2 again from the flash, its WT POS as saved over) |
| .. SAVE there | NAME IT for the sound playing ("PLUCK v3" in its own USER 2); OCT− deletes, the keys type "WARM PAD", OCT+ the numbers ("WARM PAD2"); SAVE: "SAVED: WARM PAD · USER 2", the slot renamed, back in the browser |
| .. undo there | a load taken back (SAVE + OCT−): the sound before, as USER 2 again; the browser stays; SAVE twice or HOME: back |
| The allow-list | an erase at `0x00000` (the boot) or `0xE8000` (the stock firmware's backup) refused (−8) |
| A power cycle | the flash image into a new instance: USER 1 and USER 2 kept, the sound that was playing back, the palette |
| Efficiency | every page (45), SAVE's and the browser's: nothing drawn in a second of silence. Every live graph live (the chorus and OTT on, LFO 1 on the EQ's MID) and a note held, each page's share of the SPI's time (12 MHz): CHORUS 24 %, HOME 21 %, EQ 12 %, OTT 9–10 %, an LFO 4–6 %, an envelope 3–5 %, the rest 0 (each live one 51 % before only changed rows were sent) |
| Web MIDI | a note plays and its note-off ends it |
| MIDI CC | CC 0 (bank select) at 0 and 127 sets nothing; CC 74 sets the cutoff, and only it |
| Determinism | the same input twice: the same sound, bit for bit |

The screens were checked by eye from `strata_shot.mjs`'s PNGs (a session: the wave page with VOWELS and BEND+, REC
held with LFO 1 on WT POS, the unison page, ENV 1 and ENV 2 with a key held, LFO 1's POINTS page (the sine's points;
then drawn: a point lowered, one added and curved, a key held: the voice on its cycle), its SHAPE page with S&H's five
sample cycles and with SYNC FIXED (RATE 1/8), the delete's countdown, GLOBAL's TEMPO at 132 BPM in 6/8, HOME with a
chord (the scope), the WAVE page's AMOUNT turning, the filter's TONE (CUTOFF turning, ENV 2 on it) and MORE (KEYTRACK
turning), each effect with a knob turning (DRIVE, CHORUS's DEPTH, DELAY's FEEDBACK, its TONE's LP, REVERB's DECAY, OTT's
DEPTH with a chord, the EQ's MID and MID FREQ, its POINTS with LFO 1 on MID), the SETTINGS, each palette, SAVE (once:
the footer's "SAVED"; held: the countdown), the browser (all, a category), NAME IT from it (a rename, the symbols,
written), HOME with a user preset). Bugs the tests found: the table's CC 0 means "no CC", so a CC 0 message (bank
select) set the first parameter without one (OSC A's ON); the lookup now skips CC 0. A script tapping ENV twice within
300 ms latched ENV's layer: right, by Felucca's double-tap rule, so the session taps slower. And undo kept a state's
values but not which preset it was: a load undone left the sound marked as the preset loaded, so PRESETS stepped from
the wrong place and SAVE held would have written over another preset's slot. Each state now carries its preset. The
browser also wrote user names in capitals ("SUPERSAW V2"): only the starter sounds' table names are raised now.

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
  keys' latency while the screen redraws. The UI draws only what changed and sends only the rows that changed (the
  busiest live page 24 % of the SPI's time, §6.3), but the keys are read in the main loop, so a key can wait for a
  frame's drawing: the BENCH page times a frame; if it is long the keys move to the TIMER5 tick.
- The flash on the device: an erase's and a page's IRQ-off time against the hush (the ring silent before any write);
  a SAVE's write and the background writes after 3 s of silence; that nothing outside the allow-list is ever touched
  (the RAM driver's second guard). Before any of it: a full flash dump (the brief's `docs/02-update-and-recovery.md`
  exists nowhere; FLASHING.md, Phase 7, builds on the tools PHASE0.md §7 found).
