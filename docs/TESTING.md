# Strata testing

What is tested, how, and what the tests found. Status words as in [ARCHITECTURE.md](ARCHITECTURE.md):
**verified on host** (the real DSP sources built with the PC's compiler and checked offline), **verified in emulator**,
**needs hardware test**. V0.5 ran on an FM-1 (2026-10-17): its BENCH figures are in [FLASHING.md](FLASHING.md) §8
(the core's rate ~180 instructions a µs on integer code, ~147 on float); V0.6 (the FX chain) has not yet.

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
| `tools/build.py --strata` | Strata for the FM-1: `build/strata.bin`, `strata.dis`, `strata.fwsc`, and the checks of §8.1 (the JieLi toolchain and the AC79 SDK, as Felucca's build) |
| `python3 web/strata_flash/make_flasher.py`, then `node web/strata_flash/flash_test.mjs` | the flasher into `build/flasher/` (only a package rebuilt byte for byte from the app and the loader just built), and its test in headless Chrome against a simulated FM-1 (§8.2; Windows' Node on Windows; screenshots into `build/flasher_shots/`) |
| `py tests/serve_backup_test.py` | the flasher server's backup step with a fake `pc_backup.py` and disk watcher, and its HTTP rules (§8.2; Windows' Python) |

## 2. Verified on host

135 checks in six programs (`tests/sy_*.c`), all passing (V0.66).

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

### 2.4 Engine (`sy_engine_test`, 94 checks; `sy_smoke`, 2)

- **Admission** (V0.66): with a voice measured at 1/8 of a block and the rest of the block at 1/4, eight notes at once
  get four voices (1/4 + 4 x 1/8 under 80 %) and four steal; with the cost not measured yet (a new sound), all eight.
  (The host has no clock: the test sets what the FM-1 would have measured; nothing else is refused here, the golden
  renders are unchanged.)
- **The fades** (V0.66): every module type (13) at full MIX and DEPTH under a 220 Hz sine, added, taken out, bypassed,
  back in and moved past DRIVE: the burst a change throws (the energy of the output's second difference over the 25
  blocks from it) against 25 steady blocks before and after. V0.6's switches (one block) and dip (2 + 2 blocks): the
  worst 188x (TAPE added), CHORUS and FLANGER added 75x, PHASER taken out 8.2x, DELAY 4.1x, REVERB moved 2.5x; V0.66
  (eight blocks, a dip of 8 + 8, the modulated lines filled before they are heard): at most 1.27x (PHASER back in),
  moved at most 0.94x. It found the lines' fault too: a CHORUS back in from bypassed read the sound of when it stopped
  (2,591x before the fill). The golden renders are unchanged (at the start and at a load the modules join as before).

- **Every smoothed value glides to a change** (V0.65, the memory audit): all 166 values turned to their other end while
  a sound plays, no patch load, each at its new value after 0.5 s. It fails on V0.6's code: 34 values short, the first
  the FX FILTER's CUTOFF (the gliding values' ids were kept in bytes, so every id from 256 up was another one; §5.4).

- **The FX chain** (V0.6): a module added under a sine joins at once, no dip, no click (the largest step 0.061 near
  it, steady 0.075); one taken out fades out over a block, then is gone (0.054, steady 0.054); two swapped dip the wet
  parts, then play in the new order (0.029, steady 0.039); a module bypassed is the input bit for bit once faded;
  DRIVE and DRIVE 2, each its own values, play in the chain's order bit for bit as called so; the slots as the audio
  side takes them (an instance once, none after the first empty slot). The new modules and the second instances (18)
  at random corners, six each, on full-scale noise for 2 s then silence for 4 s: finite, under 1, no float state
  subnormal; the FX filter's 21 types at four corners with the cutoff jumping 20 Hz ↔ 20 kHz: finite, under 1.
  Responses: the FX filter's LP 6 and HP 6 −3.02 and −3.00 dB at the cutoff, LO SHELF +18.00 dB, DJ open +0.00 dB,
  COMB + 12.9 dB at its frequency against −10.4 halfway; the PHASER (two still stages, half dry) −53 dB at CENTER,
  −0.06 dB at 60 Hz; the FLANGER (still at 0.3 ms, half dry) −51.7 dB at 1.67 kHz; RING MOD (1 kHz times 300 Hz) the
  sum and the difference 128 dB over the input; TREMOLO from 0.000 to 0.500 on 0.5 at full DEPTH, AUTOPAN's sides
  opposite; TAPE (still, clean) the input 4 ms later (correlation 1.0000); BITCRUSH at 1 bit three levels.
- **Parameters and presets**: 312 parameters; format 2 (V0.6): a patch with a chain of five packs into 550 bytes and
  back unchanged; the largest (eight modules of the most values, two 16-point shapes) into at most 710 (a slot holds
  766); a preset of format 1 (from before the chain) loads its effects that were ON as the chain in FX ORDER's order,
  its values and matrix kept; an older patch (fewer values, or no drawn shapes) loads with defaults; out-of-range values clamp;
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
  the optimisation pass, §5, and after the three effects' fixes, §5.2: supersaw, pluck, pad and sync lead). The chain
  (V0.6) left them unchanged: the six effects of before play as they did.
- **The CPU guard near a half's deadline** (V0.6): 88 % changes nothing; 95 % (the nested interrupts in) a budget step
  at once and the effects cheaper; 98 % a voice shed at once.

### 2.5 Store (`sy_store_test`, 12 checks)

`sy_store.c` (Strata's presets, settings and sound on the flash) against a simulated NOR that behaves as the part does
(an erase sets 0xFF, a program only clears bits, a page does not wrap) and refuses anything outside the firmware's
allow-list (`hal/fm1_flash_map.h`, the hooks as `storage_hw.c` has them): an empty flash loads nothing; saved and
loaded back; saved again to the other copy, the newest winning; a write torn after any byte of the payload or the
record leaves the copy before; a part that ignores writes is not reported saved (the read-back) and the old copy
loads; Felucca's record (`FELU`) in the same sector reads as none; all 64 user slots (V0.8; 128 before) round trip a packed patch, a slot
written keeps its pair's other slots, a slot emptied reads empty; every object's sectors inside the allow-list (no
erase or program refused), and the hooks refuse the loader's area and the stock firmware's backup.

### 2.6 USB MIDI out (`sy_midi_out_test`, 9 checks, V0.81)

`usb.c`'s out ring as the audio interrupt fills it: with room, every packet in and in order, nothing added (V0.8's
behaviour, packet for packet); full, a note's packet lost marks its channel (a controller's does not) and nothing more
goes in, so no note goes out that the recovery would cut; with room for 17 again, an All Notes Off on each channel
marked, in channel order, then the packet; the half's poll sends them with no packet after; while USB is not
configured nothing goes in and the mark waits; all 16 channels fit the room asked for. Felucca's own tests of the
ring (`scale_test`, `chord_test`, `v156_test`, `midi_control_test`, `midi_uart_test`) pass as before.

### 2.7 The drums (`sy_drum_test`, 28 checks, V0.85; V0.86 step 2)

The drum synth (`sy_drum.c`) through `sy_block`, as the audio interrupt runs it: every lane at velocity 127 heard,
under 0.8 (-2 dB), every sample finite, its voice ended within its DECAY; WT 1 and 2 play no hit while their table is
none or being built, and one once it is; DECAY as the time to -60 dB of KICK, HAT CL, HAT OP, CYM and WT 1 at 20, 50
and 80 (within 11 % of the setting at worst, 30 % allowed); TUNE: the KICK once swept at 48 Hz x 2^(TUNE / 12) at -12,
0, +12 (within 3 %); CLAP's four bursts 20 ms apart; a HAT CL ends a 2 s HAT OP within 60 ms; ACCENT 50 %: 1.5 x a
plain hit; VELOCITY 0: every velocity alike, 100: by its square; PAN hard left: nothing right; a muted track's hits
silent. The duck against the synth's held note rendered without it: down to 1 - AMOUNT in ATTACK's time, back in
RELEASE's for each SHAPE; at half RELEASE LINEAR, EXP and PUMP where their curves put them; the output the unducked one
x the gain sample for sample (over 0.1, 34,000 samples); at rest after, the gain exactly 1; a KICK with a full duck at
once the KICK alone, bit for bit (the drums after the duck). 40 random kits, every lane at random velocity, accent and
track: finite, every voice ended. Golden renders (each lane, and two bars of a beat over a held chord with the duck)
twice the same and as recorded in `tests/sy_drum_golden.txt`. The host's cost: all eight sounding ~0.2 % of real time
(the target's: BUDGET.md §15). And the engine's 94, its golden renders unchanged: the drums' block, nothing sounding,
touches nothing.

V0.86 step 2, the drum editor (10 checks more): SHAPE on a steady sound, ATTACK 90 and SUSTAIN 20: x 3.02 at the hit
(+9.6 dB) and x 0.437 on the body, within 2 %, the start's share falling between; DRIVE at 100 %: SOFT, HARD and FOLD
bounded by their out level (0.400 for 0.5 in), CRUSH on 2 bits (every sample a step of 1/4), MIX 50 % half the way;
EQ: LOW +12 +11.4 dB at 80 Hz and +0.1 at 6 kHz, MID +12 +12.0 at FREQ (1.27 kHz) and +0.5 at 80 Hz, HIGH -12 -10.5 dB
at 10 kHz and 0 at 80 Hz; COMP 80 % on a beat: its crest (peak over RMS) 5.81 -> 4.33, nothing over full scale; the
duck's SOURCE (ROW its row's hits, KICK the KICK's, BOTH both) and HOLD (100 ms: still at the bottom 80 ms after it
got there; without, on its way back); ACCENT's TONE 100: a soft HAT CL darker, an accented one brighter; the eight
factory kits: each loads whole, every lane accented finite, under 0.8 and ended (the loudest 0.697, MINIMAL's); 40
random kits with every effect (DRIVE, EQ, SHAPE, COMP, SOURCE, HOLD): finite, every voice ended; the host's cost with
every effect on, 0.4 % of real time (the target's: BUDGET.md §17). The golden renders unchanged: a drum with its
effects at rest and COMP off plays bit for bit as before.

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

(Phase 2's figures for the engine alone. The whole unit's, and the V0.65 audit: [BUDGET.md](BUDGET.md).)

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

### 5.4 Two more, found by the memory audit (V0.65)

[BUDGET.md](BUDGET.md) is the audit. Reading where every byte goes turned up two V0.6 bugs, both fixed:

- **34 values never glided after a load.** The list of smoothed values still on their way kept their ids in bytes;
  V0.6's chain took the parameters to 312, and an id from 256 up came back as another (`(uint8_t)i`): that value's
  flag stayed set and it never moved again. Every smoothed value of the FX FILTER, BITCRUSH, TREMOLO, RING MOD and TAPE
  (both instances) was hit: their knobs, MIDI and the undo changed the patch but not the sound until a load. The new
  check (§2.4) fails on the old code. The tests had set those values before a load, never while a sound played.
- **RING MOD's page name ran on.** `char name[8]` and "RING MOD": C keeps the 8 characters and drops the end without a
  warning, so the header read on into the knobs' ids. An emulator check (§6.3) asks every page's name to end in time.

### 5.5 Five more for V0.81 (the stress test, the sanitizers, the official firmware)

- **DELAY's page wrote past its handles.** `ui_p_delay` put MIX's handle as knob 4 on the DELAY page (no knob there)
  and cleared it after: the write went one past `pg.hx[4]`, into `pg.hy[0]`. Inside the page's own struct, so nothing
  else was hit and the page drew as it should; undefined all the same. Found by UBSan in the stress test (§6.6).
- **Two stale places in the looper.** A take's start (the track's scene now the empty clip) and a launch while stopped
  left the track's next event pointing into the clip it played before. No sound came of either (the walk skips a
  track while its take records, and PLAY and a take's end look the place up again), but an edit path that trusted it
  would have walked another clip's list. Both set it now; the stopped launch also lets the old clip's knob values go.
  Found by the stress test's invariant ("a track's next event not in its clip").
- **1 KB of the pool was in `.bss`.** `static float wg_a[N], wg_b[N] SY_POOL;` gives the section to `wg_b` alone. Found
  reading the symbols ([BUDGET.md](BUDGET.md) §14).
- **USB MIDI out could leave a note on.** A full out ring (a host reads the port only while an app has it open) dropped
  packets, a note-off among them, and the instrument it reached held that note. The official firmware's MIDI router
  recovers with All Notes Off on the channels it used; `usb.c` does the same now (§2.6).

### 5.6 Found building V0.85

- **A TYPE change lost the drums' details.** The first build turned a clip's hits into sixteenths when its track left
  DRUMS and back into plain hits when it came back: one turn of TYPE there and back cost every ACCENT, CHANCE and
  RATCHET. The emulator's power-cycle check found it (a ratchet gone after TYPE SYNTH and back). Now nothing is turned:
  a hit keeps its marker on any track (a synth or MIDI track plays it as a sixteenth, `lp_len`), a note keeps its
  length on a drum track (a plain hit), and a note's own length stays under the marker (16,383 ticks, ~42 quarters).
- **SELECT and EDIT twice on a section the family no longer shows** stepped that section's pages (OSC A's on a drum
  track, whose EDIT shows only its KIT): now they go to the family's first section. Reached in the emulator by a tap
  within 400 ms of boot (read as a second tap); on the FM-1 the boot alone takes ~770 ms.
- **The metal voices' cost**: their six squares' phases went through memory each sample (232 instructions a sample
  for three voices); counted in a pass of their own, 170, the same sound bit for bit (the drums' golden renders).

## 6. In WebAssembly and in the emulator (Phase 3)

The same sources compiled with Emscripten (6.0.12, `-O2 -ffp-contract=off`) and run under Node 18. The emulator's page
has also run in headless Chrome (§6.4); the workbench's page has not been opened in a browser.

### 6.1 The engine as WebAssembly (`tests/sy_wasm.sh`)

The six host programs (§2) built with `emcc` and run under Node: 133 checks (V0.65), all passing, the golden renders
included.
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

### 6.3 The app on the simulated FM-1 (`web/emu`, `strata_emu_test.mjs`, 171 checks)

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
| The SETTINGS | HOME held: a countdown from the hold time ("HOLD: SETTINGS IN 4"; the UI build: its screen, `web_test_cd` "SETTINGS 4"); let go at 1.5 s, nothing opens, the footer cleared; held 5 s: the SETTINGS; KNOB 1 changes the palette and the screen with it (26,085 pixels); the eighth, MONO: not a pixel outside the window that is not grey |
| OCT with EDIT held | the section's pages: OCT+ WAVE -> UNISON, OCT- back to WAVE, OCT- again round to OUTPUT; EDIT let go: no tap (the page stays); the keys' octave unchanged |
| OCT with ENV held | OCT+ steps the section shown (OSC A); ENV held on past the hold time: no ENV 2 layer; let go: no tap |
| OCT alone | the keys' octave +1 and back; the page stays |
| An LFO's POINTS page | LFO 1's first page; KNOB 1-4 the point, its place, its level, the curve |
| .. KNOB 1 | picks a point, round (+2: point 3; -3: point 5 of 5); the shape untouched (SINE) |
| .. KNOB 3 | lowers point 2 (1000 -> 990 for 5 slow detents): the sine's 5 points become the drawn shape (SHAPE DRAWN); 2,032 pixels change |
| .. KNOB 2, KNOB 4 | point 2 along the cycle (250 -> 253 slow; quickly no further than the next point, 500); the last point stays at the end; KNOB 4 on it curves the segment into it (+0.8 % for 4 slow detents) |
| .. LFO twice | a point left of the one picked, halfway (625 between 500 and 750), on the curve, picked; the page stays POINTS. The first point picked: the new one right of it (250, halfway to 500) |
| .. not | LFO twice from another family's page: no point (the LFO's next page); STAIRS shown (16 points): no 17th; the MOD layer: KNOB 3 changes nothing |
| .. LFO held | from the hold time a countdown ("HOLD: DELETE POINT 3 IN 4"; the UI build: "DELETE POINT 3 4"); let go at 2 s: nothing deleted, the footer cleared; held 5 s: point 3 goes (STAIRS drawn, 15 points), point 2 picked, the page stays; the first point picked: "THE END POINTS STAY"; a knob turned during the countdown stops it |
| Undo, redo | SAVE + OCT− twice takes back a level KNOB 3 set and the delete (SHAPE STAIRS, the drawn shape of 7 points); SAVE + OCT+ redoes the delete; SAVE let go: no page, no octave. On WT POS a gesture at a time (600 -> 606 in one, 607 a second later: undone 606, then 600; redone 606); a sound's load undone (PAD back to PLUCK) |
| SYNC FIXED | LFO 1 SHAPE: SYNC FIXED, KNOB 3 steps the note value (1/4 -> 1/8D), RATE untouched; FREE: KNOB 3 the rate in Hz, the note value kept. DELAY: FIXED by default, KNOB 3 steps its note (1/8D -> 1/8); FREE: the time in ms |
| GLOBAL | GLO tapped three times: the TEMPO page, 120 BPM 4/4; BPM 3 slow detents 123, held between 20 and 300; TS-BEAT 1-16; TS-NOTE 1, 2, 4, 8, 16; 300 BPM in eighths: 150 quarter notes a minute |
| FX | ALGORITHM through six effects (DRIVE .. EQ), round; the EQ's pages POINTS (POINT, FREQ, GAIN, Q) and GAINS (LOW, MID, HIGH, ON) |
| A knob turning | EQ POINTS, MID picked: KNOB 3 its gain, its cell lit, the curve before the turn kept (4,430 pixels); KNOB 4 its Q; LOW picked: LO Q; a second after the last detent both let go (2,813 pixels back) |
| .. the MOD layer there | KNOB 3 sets LFO 1's depth on MID's gain (the point picked) |
| The effects' order | FX held + ALGORITHM on DRIVE: DRIVE second ("DRIVE: 2 OF 6", FX ORDER 120); FX let go: no page step; ALGORITHM then follows the new order (after DRIVE: DELAY); two back: CHORUS first |
| Live only with sound | CHORUS on: its taps still in a second of silence (0 blits), moving with a note (7 in 0.5 s), still again after it (the UI build: OTT's levels, its chorus drawn still) |
| HOME | silence: nothing redrawn in a second (the flat capture drawn once); a note playing: the scope live (12 captures in 0.5 s, its rows sent 10 times; the UI build: the bloom); on another page no capture |
| The WAVE page's warp | WARP BEND+, KNOB 4 turned quickly (AMOUNT 55.1 %): the table's frames drawn warped (7,766 pixels of the display), the frame before the turn kept |
| SAVE once | a starter sound (PLUCK) saved at once as "PLUCK v2" in USER 1 ("SAVED: PLUCK v2 · USER 1"); the page stays; flash written, none refused. Once more: "PLUCK v3" in USER 2, USER 1 kept |
| SAVE held | a countdown ("HOLD: SAVE OVER USER 2 IN 3" at 2.3 s; the UI build: "SAVE OVER USER 2 3"); let go then: USER 2 untouched; held 5 s: over USER 2 ("SAVED OVER USER 2"), its stored WT POS the sound's |
| .. with OCT | SAVE held + OCT−: undo (KNOB 1's edit taken back); held on past 5 s: USER 2 not saved over |
| .. a starter sound | held and let go: "NOTHING TO SAVE OVER: TAP SAVE", nothing written |
| SAVE twice | the browser, nothing saved; KNOB 2 loads as it turns (USER 1, then USER 2 again from the flash, its WT POS as saved over) |
| .. SAVE there | NAME IT for the sound playing ("PLUCK v3" in its own USER 2); OCT− deletes, the keys type "WARM PAD", OCT+ the numbers ("WARM PAD2"); SAVE: "SAVED: WARM PAD · USER 2", the slot renamed, back in the browser |
| .. undo there | a load taken back (SAVE + OCT−): the sound before, as USER 2 again; the browser stays; SAVE twice or HOME: back |
| The allow-list | an erase at `0x00000` (the boot) or `0xE8000` (the stock firmware's backup) refused (−8) |
| A power cycle | the flash image into a new instance: USER 1 and USER 2 kept, the sound that was playing back, the palette |
| The matrix | SEQ: KNOB 1 picks slot 8 from slot 1 (round); KNOB 2 its source (LFO 1), KNOB 3 a destination, KNOB 4 an amount; the display changes with them |
| SCL | its page (its button lit); MAJOR, D, SNAP: G#3 plays G3, F#3 itself; WHITE: C4 plays D4, D4 E4, F3 G3, C#4 nothing; KNOB 4 the keys' velocity (127); SCL held + A3: the root A, no note; A major on the LEDs (the roots mid, C#4 glowing, C4 dark); all four back after a power cycle |
| The hold layers | GLO: PERFORM, GLO breathing, 8 detents bend halfway up, the wheel 5, a held note playing on; let go, the bend back to 0, the wheel kept. FX: MACROS, MACRO 1 up, A3 turns DELAY on (no note, its LED lit). LFO: MOMENTARY, KNOB 1's value back when let go. EDIT: JUMP, C#4 opens KEYS, A#3 GLOBAL, the knobs do nothing |
| PLAY | the transport on, LFO 1's phase from its start, the green LED lit, PLAY lit a quarter of each beat; off; ARP says it comes later |
| The SETTINGS | SCREEN, PANEL, MIDI; GLOW low reaches the LEDs' level; HOLD 500 ms; channel 2: channel 1's note not heard, channel 2's played; program change 3: the fourth starter sound; MIDI LEARN: KNOB 1 picks WT POS, CC 20 learned, OCT+ ends it, CC 20 sets it 0 to 1000; HELP's hint entering the effects; all back after a power cycle; CLEAR twice clears |
| Efficiency | every page (48), SAVE's and the browser's: nothing drawn in a second of silence. Every live graph live (the chorus and OTT on, LFO 1 on the EQ's MID) and a note held, each page's share of the SPI's time (12 MHz): CHORUS 24 %, HOME 21 %, EQ 12 %, OTT 9–10 %, an LFO 4–6 %, an envelope 3–5 %, the rest 0 (each live one 51 % before only changed rows were sent) |
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
written), HOME with a user preset, D dorian under SNAP and WHITE, the matrix, PERFORM, MACROS, JUMP, MOMENTARY, the
SETTINGS' PANEL and MIDI with a CC learned, LEARN's footer, a HELP hint). Bugs the tests found: the table's CC 0 means "no CC", so a CC 0 message (bank
select) set the first parameter without one (OSC A's ON); the lookup now skips CC 0. A script tapping ENV twice within
300 ms latched ENV's layer: right, by Felucca's double-tap rule, so the session taps slower. And undo kept a state's
values but not which preset it was: a load undone left the sound marked as the preset loaded, so PRESETS stepped from
the wrong place and SAVE held would have written over another preset's slot. Each state now carries its preset. The
browser also wrote user names in capitals ("SUPERSAW V2"): only the starter sounds' table names are raised now.
Adding SCL's section wrote past the sections' page memory (ui.spage), over the family's index: SCL's button went
dark and the family tables were read out of range; the array now has a place for every section (a check of SCL's
button LED fails on the old code).

**V0.6** (after the V0.5 hardware test): the checks follow the new panel (SELECT the section, ALGORITHM the page; GLO's
fourth page from VOICE is GLOBAL's TEMPO) and add: FX's chain view (SUPERSAW's chain: CHORUS, REVERB), FX twice opens
the module picked and twice again comes back, SELECT the chain's modules; a module added below the slot picked
("ADDED: PHASER · 2 OF 8"), a second of a type, a third refused, DELAY once; KNOB 3 moves the module picked, to the
top and no further; FX + OCT- takes it out; eight modules, a ninth refused; KNOB 4 the picked module's MIX; FX held:
F3 bypasses module 1 (its LED glowing), F3 again back; SAVE + OCT- undoes each step; a second EQ's pages set its own
values. EDIT + OCT- / OCT+: OSC A off, on, the page staying; SEQ: "SEQUENCER: IN A LATER VERSION", the matrix on
GLO; FX held, a key with no module: "NO MODULE THERE YET"; the browser: OCT- held, its countdown, let go at 1.5 s
nothing deleted, held 3 s USER 1 deleted (the sound playing on, from no slot), SAVE + OCT- straight after: back;
OCT- tapped there: the octave. Found by them: chain edits made within a second of each other were one undo step
(each add, take-out and bypass is a step of its own now).

**V0.65** (the memory audit): the display's graphs are called through a table now, and every page above still draws
as it did. Two checks more: BENCH's rows built at the widest figures a unit shows (the emulator draws no BENCH: no
clock of its own) fit the window (the widest, AUDIO, 20 px to spare; STACKS with its new SPARE note was 11 px over
until it took the AUDIO row's form, "2.2 / 6.6 KB"); and every page's name ends within its 8 characters (V0.6's RING
MOD pages ran on, §5.4).

**V0.66**: the emulator plays its keys as the FM-1 now does, from its "TIMER5" (`sy_keys_tick` each millisecond)
while they only play, from the main loop in the layers: every check above that plays a key passes either way. One
check more: eight notes at once over MIDI all sound (the app starts two a block). BENCH's rows fit with the SCREEN
row naming every page's section (7 px to spare).

V0.85, the drums (8 checks; 140 in all): T1 DRUMS, EDIT shows its KIT; white key 1 a KICK, white 9 the same accented
(1.5 x the peak at ACCENT 50 %), black 1 DUCK (the duck's state, the synth's gain under 1). The GRID: black keys pick
the row, white keys put a beat in (KICK four to the floor, SNARE on 2 and 4, HAT CL on the eighths, DUCK with the
kick), the empty clip a bar long and launched; the DUCK row's steps and its key lit; a step in and out again. PLAY, two
bars: KICK 8, SNARE 4, HAT CL 16, DUCK 8, the synth's gain down to 0.400 (AMOUNT 60 %). HIT's knobs (each turn a
turn of its own: the UI sums a millisecond's): the SNARE's step 5 RATCHET 4 hits, the KICK's step 9 CHANCE 50 %;
eight bars: SNARE 40, KICK 30 (24 and 6 of the 8 chances). A take on the drum track (REC, the keys from the CLIPS page:
GRID's take the keys): its hits KICK, SNARE and an accented HAT CL (36/16384, 38/16384, 42/16416), a bar. TYPE SYNTH
and back: the hits as they were, the pages following (GRID, NOTES, GRID). The kit's knobs: the SNARE played (stopped)
is the drum picked, its TUNE +5, LEVEL 80, the duck's AMOUNT 50. A power cycle: all of it back. Screens: 11 more
(the TRACK page with DRUMS, GRID and its VELOCITY turning, HIT on a ratchet, the KIT's DRUM, MORE, ACCENT and DUCK),
the check clean; the knob sweep 611 turns (583 before), none with findings.

V0.86 step 1 (19 checks more; 159 in all). The checks before were brought to V0.86's grammar where it changed what
they press (none dropped): the splash held 2 s, then HOME's SOUND, EDIT to OSC A's WAVE; PRESETS through its list (an
edited sound's first detent opens it alone, the next moves, the preset loads 0.4 s after); HOME held is the help map
over its countdown; the delay's SYNC on KNOB 1 and TIME on KNOB 2; GLO and ARP once in their menu go to its first
page, twice toggles (the ARP off, on); ENV twice ENV 1 <-> 2; the EQ's SHAPES; FX once on a module, the chain view
with it picked; SELECT in a menu of one module does nothing; NOTE+'s DELETE on KNOB 3; a drum take on MIXER (CLIPS'
keys launch). New: EDIT once its page left, in its menu its first, twice JUMP latched, a key opens SCALE and closes
it; LFO twice MOMENTARY latched on the page before (a knob there, LFO: back); GLO twice PERFORM, GLO closes it; SCL
and ARP twice on and off; SEQ once (CLIPS; the page left in LOOP and CLIP from another menu; CLIPS in its own), twice
TRACK's RECORD; ENV twice from ENV 1 and from ENV 2; REC twice: the second press nothing; HOME twice the browser;
HELP on: "AGAIN: ENV 2"; HOME held 5 s through the help map to the SETTINGS. TOOLS: COPY, PASTE into an empty clip at
once, into a full one asked then done; DOUBLE (4 bars, 8 notes), HALVE asked then done; COPY on to BAR 1, PASTE into a
clip with no bar 1 ("T1 C HAS NO BAR 1"), into one asked then done; CLEAR asked then done. SPEED x1 -> x2: two bars at
x2 pass twice in two bars of time; down to x1/2. TYPE: an empty track's at the turn's end, one with clips asks, the
asked detent sets it, an ask unanswered keeps it ("TYPE KEPT"). CLIPS' keys: white 1 launches T1 A (no note of its
own), black 2 scene B. The grid: a step held, CHANCE and RATCHET turned, kept at the release; held again with no turn,
out; a tap on an empty step, in. A power cycle: SPEED x2 and a pasted clip's length back. A centre detent: PAN from -1
through the middle holds four detents at 0, then moves; a new gesture from the middle does not hold. The battery's
bars at 520, 540, 570, 600 (0 .. 3), USB power 4. SCREEN OFF AFTER 1 MIN: on at 59 s, dark after (nothing drawn),
EDIT wakes it and does nothing else; SHOW BATTERY and SCREEN OFF AFTER back after a power cycle. Screens: 152 (21 more:
HOME, its MIX, PRESETS' list and its guard, the unsaved dot, the help map, JUMP latched, the EQ's SHAPES, TYPE
turning and asking, CLIP's SPEED, TOOLS asking, the drum grid with a step held, a drum track's HOME, KEY SCAN,
POWER), the check clean; the knob sweep 639 turns, none with findings (it found CLIPS' "SYNTH+MIDI" over SCENE's
handle: the knob's value is "SYN+MIDI" now).

V0.86's renders review and step 2 (7 checks more; 166 in all). PRESETS' list: untouched 3 s, it goes and nothing
loads; SAVE twice loads the one picked and closes it; SAVE + OCT+ names a new preset ("SUPERSAW v2", USER 3) and SAVE
saves it back to the page; SAVE + OCT- on it asks, again deletes ("DELETED USER 3"); on a starter sound, "A STARTER
SOUND STAYS". The drum editor: EDIT on a drum track opens DRUM's SOUND, PRESETS picks the drum (CLAP), SELECT goes to
KIT's ACCENT; DRUM's DRIVE, EQ (LOW held at its middle from 47) and SHAPE on the drum picked, the kit's dot after an
edit; KIT's SOURCE, HOLD, COMP's AMOUNT and MIX, ACCENT's TONE; HOME twice on a drum track opens the kits at the kit's
own, SAVE twice loads INDUSTRIAL (its name, its number, its KICK's DRIVE; the dot and the list gone); EDIT held 5 s on
DRUM: "DRUM: RESET", the drum's DRIVE at rest; a power cycle: INDUSTRIAL back with its SNARE's DRIVE and COMP. The
checks before on the new pages (V0.85's drum check on DRUM's SOUND, MIX, KIT's DUCK on KNOB 2). Screens: 159 (DRUM's
five pages, KIT's four, the kits' list, PRESETS' delete question; the drum shots before redone on the new pages), the
check clean; the knob sweep 681 turns (639 before), none with findings.

V0.87 step 1 (171 checks; the tests start on sound 0, `web_test_sound_set`, since a unit now boots on INIT): the
checks of removed controls rewritten for their replacements: the resets by a button + OCT− held 5 s (`octHold`: LFO,
EDIT, DRUM, SEQ's CLIP), HOME held 5 s DEBUG and twice the SETTINGS (LIGHTS MED kept through a power cycle), PRESETS'
list INIT first then A to Z and gone at 5 s, EDIT twice the wavetables' list (a table loaded) and on a drum track the
kits, ENV twice and LFO twice the assign window (its source, shut by its button), the SAVE window (SAVE AS NEW to NAME
IT, SAVE over USER 2, CANCEL, a knob shutting it, a starter's SAVE row closed), CORE 2 in DEBUG, the MATRIX in EDIT,
SELECT on HOME the track, SEQ + OCT+ a clip of a bar and PRESETS its length, SEQ + OCT− DELETE? then deleted, the
projects' list by SEQ twice through a power cycle, TYPE's question (no time out, turned again, SAVE yes, EDIT kept),
REC's tap after 300 ms and a second tap after a stop doing nothing. New: a new unit on INIT (its macros in slots
1-4, LIGHTS HIGH), TAKE (3 notes, a bar), tap tempo (150 BPM from 0.4 s taps, the transport stopped), the assign
window's page 2 (a slot, AMOUNT, UNIPOLAR's bit), WIPE (asked, SAVE: the palette and LIGHTS back). Screens: 173 (the
windows, the lists, the questions, the reset's countdown, a new clip, DEBUG, WIPE), the check clean (the windows'
"cannot" rows raised from MID to LABEL for it); the knob sweep 679 turns, none with findings; the stress test 24 x
3000, clean. The LEDs: `tests/sy_led_test.c` (host) traces the scan's line writes in quarters: every level steady
frame to frame (+-1 us), HIGH 90.7 / MED 39.7 / LOW 14.7 us lit a frame, the glow 3.4 / 2.4 / 1.7 us, OFF dark, the
breath 8 steps a period; how it looks **needs a hardware test** (FLASHING.md step 53).

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

### 6.5 The UI build: its screens and its drawing's time (`strata-ui`)

- **Every screen** (`web/emu/strata_ui_shots.mjs`, after `build_strata.sh`): 75 PNGs in `build/emu_strata/ui_shots`,
  each named after the design board it was built from, each reached as the panel reaches it (a page by its section, a
  knob turned for a turning shot, a button held for a layer or a countdown, SAVE tapped; `web_test_screen` for the
  screens only sy_main.c draws: SAFE START, power-on, USB BOOT's countdown), at 240 and 480 px. Looked at beside the
  boards ("Strata UI Firmware Renders"); found that way and fixed: a steep curve's stroke stretched over the whole
  screen (an instant attack, BITCRUSH's steps: the slope's widening now stops at 45 degrees and goes across beyond it),
  the flanger's comb aliased on the log axis (now linear, 0 .. 3 kHz), the sub an octave down drawn half a cycle.
- **The drawing's time** (`web/emu/strata_bench.c`, built natively): each frame's work timed in five sessions of the
  panel, the same file against V0.66's tree; the table in [UI.md](UI.md) §5. First run: HOME's bloom +7 % over
  V0.66's scope, a knob turning on FILTER +87 %, ENV with voices +125 %: each page was drawn three times, once a
  strip. Brought under V0.66's in every session by: a page's curves computed once a frame, the canvas cleared and the
  rows hashed two pixels at a time, a live frame redrawing only the rows its live parts take, a knob's turn only the
  body's rows, the stipple a store a dot, the bloom's segments without a pixel's bounds check.

**V0.67** (the UI made legible; [UI.md](UI.md)):

- **The screens' check** (`web_lint` in `strata_web.c`, under `LK_LINT`): the screen drawn twice into buffers of its
  own, with its text and without; a pixel the text changed fails if under it or beside it lies anything but the
  ground, the text's own ground or a blend of the two; two texts' ink boxes may not meet; a text may not reach within
  2 px of the screen's edges; no text in MID or LINE; no two knobs' handles on one place. The shots script runs it on
  each of its 80 screens (`lint.txt`, marked PNGs in `lint/`): its first run on V0.67's type found 24 screens with
  findings (a handle's ring on a title, chips past the edge, the MOD layer's codes on its keys' borders, the theme
  names crossing), all fixed; now clean. New checks in `strata_emu_test.mjs`: every one of the 149 pages clean in
  silence; FILTER's VALUES page: a quick turn of 20 detents moves the cutoff 20 (TONE's page: 600, accelerated); EDIT
  tapped on MORE steps to TONE (the VALUES pages skipped); the bloom: a bud in silence (open 0, swirl 0), 0.15 s into a
  note swirling and opening (0.65, 0.83), at 1.75 s open and unwound (0.90, 0.06), 4 s after the release a bud again
  (0.01) and nothing sent in the next second. The palette's check: round from STRATA to the twelfth, NEON.
- **V0.7, the chords** (`strata_emu_test.mjs`, seven checks; `web_test_chord`, `web_test_chd`, the USB MIDI sent
  kept by the emulator): with the chords off a key plays its note; FREE: MIN tapped, then E4: 64 67 71 "Em", let go
  nothing left; MIN and m7 together: 64 67 71 74 "Em7"; 9 alone keeps MIN; MAJ, VOICING +1, C5: 76 79 84 "C/E"; KEY
  in C major: D4 62 65 69 "Dm"; C4 then F4 with AUTO: 60 65 69 "F/C"; the in-key 7th on B4: "Bm7b5"; BASS and USB OUT:
  48 60 64 67, the chord's note-ons on channel 1 and the bass's on 2, and their offs; PERFORM: STRUM one note at 12 ms,
  three at 112 ms (30 ms apart); ARP up at 1/16 (125 ms): 60, 64, 67, 60, 64; PATTERN 1: a stab each quarter; HARP:
  five keys from C#4 to G5, one note each, rising, each of C major; MIDI IN as roots: D4 in, "D", its off ends it; the
  settings through a power cycle. The shots script adds the chord pages (`Chord*`, `Perf*`, `Bass`, `ChordMidi`).
- **V0.75, the ARP** (`strata_emu_test.mjs`, ten checks; `web_test_step`, `web_test_arp`, the USB MIDI sent with the
  time of each): MODE over C E G: UP 60 64 67 60 .., DOWN 67 64 60, UP-DOWN 60 64 67 64, DOWN-UP 67 64 60 64, UP & DOWN
  60 64 67 67 64 60, OUTSIDE IN 60 67 64, INSIDE OUT 64 67 60, PINKY 60 67 64 67, THUMB 60 64 60 67, two octaves up 60
  64 67 72 76 79, AS PLAYED (G C E pressed) 67 60 64, CHORD over two octaves 60 64 67 then 72 76 79; the steps (LENGTH
  4: PLAY, TIE, REST, three hits an octave up at 50 %): 60 at 0, 76 three times at 3, 3.33 and 3.67 steps at velocity
  50, 67 at 4, 72 at 7, 64 at 8, C4 held through the TIE until 1.5 steps; GATE 25 % and SWING 75 %: the steps at 0,
  1.5, 2, 3.5, 4, each a quarter of a step long, CHANCE 0 % on step 6 leaving it out and its note's place gone; SLIDE:
  C4 ends two blocks after E4 begins; LATCH: let go it plays on, D4 pressed and let go replaces it, LATCH off: silence;
  PLAY on: the steps on its beat to within a block, a key's first note at the next step; CLOCK IN MIDI: 30 clocks, a
  start, clocks every 1,152 samples (95.7 quarter notes a minute): the tempo 95.7, the arpeggio's notes on every sixth
  clock from the start, a stop ends the transport and PLAY's green LED; the ARP's settings and a step's through a power
  cycle; ARP held opens its pages without switching, a tap switches, the STEPS, RATCHET and GROOVE knobs; a chord held
  then ARP: arpeggiated at once (60 64 67 60 64 67), ARP again: the chord held, let go: silence. The shots add `Arp*`
  and `TempoClock`; the drawing's bench the ARP's pages while it plays (UI.md, BUDGET.md §10). Found on the way and
  fixed: a block comment left open on a line's end swallowed the next line's `chs.k = 0` (every arpeggio after the
  first began mid-pattern; the changed files scanned for the pattern since: no other); the first step played the
  chord's first key alone (MODE OFF waits 15 ms for the rest); the value flags fell on the rows of figures (the ARP's
  pages show figures and no flag); keys held when PERFORM switched were forgotten (now handed over and played on).
  **Needs a hardware test**: the timing by ear (RATCHET at 1/32, SWING, SLIDE with GLIDE), MIDI clock from a DAW over
  minutes (USB and TRS), the ARP LED's beat.
- **V0.75, the revision** (the user's six changes): the chords' checks rewritten for the knobs (FREE's TYPEs: MIN on E4
  64 67 71 "Em", MIN7, MAJ with VOICING +1 on C4 64 67 72 "C/E", MIN11 on D4 "Dm11", 13 on G3 "G13"; SPREAD 2 OCT 60 64 67
  72 76 79, OPEN 52 60 67 "C/E", WIDE 48 64 79; KEY's SHAPE 7TH on B4 "Bm7b5"; STRUM 30 ms: one note at 12 ms, three at
  112 ms, 30 ms apart; DOWN with TAPER 100 %: 67/100 64/75 60/50; HARP on every key; the settings through a power
  cycle). The buttons' grammar: a tap opens ENV, again its next page, on into ENV 2, twice the next section; HOME + OCT+
  the next sound, OCT− back, twice the presets; EDIT held 5 s resets OSC A (WT POS 400 -> 0) and SAVE + OCT− undoes it;
  SCL + OCT+ / OCT− the chords; GLO + OCT+ PERFORM latched, a GLO tap lets it go; FX + OCT− / OCT+ on CHORUS's page
  bypasses it and puts it back; SELECT on SETTINGS steps its pages; ENV + OCT+ latches ENV 2's MOD layer; LFO + OCT+
  adds a point (from EDIT too: its POINTS shown), LFO + OCT− deletes the picked one, LFO held 5 s resets LFO 1 (a knob
  turned stops the countdown; undone and redone); ARP: a tap its menu, again GROOVE, twice STEPS, + OCT+ / OCT− on and
  off from any page. `strata_knob_sweep.mjs` (new): every knob on every page turned once (546 turns) and the screen
  checked as it shows the turn: 75 findings before (the old flags over titles, figures, handles), none after (a value
  the page shows is lit in place; else it is in the bottom row). Found on the way: the top line's chord name under its
  section dots (moved left of them); `memmove` missing from the device's build (a loop).
- **V0.8, the looper** (132 checks, the sequencer's 13 replaced by the looper's 13; 127 screens and 583 knob turns,
  all clear): T1's clip from the launch pad's key 1 (the transport starting): C4 E4 G4 C5 a quarter apart, an eighth
  each, round again at 4, its voices tagged T1; the mix (VOLUME 50: 0.25 each side; PAN left: 0.25 / 0; MUTE: 0; T2
  SOLO: T1 0); T2 MIDI on channel 5 launched mid-bar: its first note at the bar, sent on its channel, no voice; STOP
  ALL at the bar; a first take on T3 B: REC stopped, the count-in (a click), the take from the bar, C4 at 0.5 for an
  eighth and E4 at 2.0 for a quarter, REC at 4.1: one bar, 60@48/48 64@192/96, playing at 4.50 and 6.00; overdub: D4
  at 9.0 sent once that pass, at 13.00 the next, UNDO PASS takes it out and puts it back; QUANTIZE 1/16: F4 at 16.3 on
  tick 24; the cutoff turned while recording: written, heard as turned, held by the next pass, let go when stopped;
  SONG A once, C twice: A C C C C A; a track's own chords (T5 off while T1 KEY), MIDI on T4's IN channel and on the
  selected track; the click on a beat, OFF none; NOTES (PITCH +2, START +1/16), EDIT's DELETE; SEQ + OCT+ the next
  track, SEQ held 5 s on CLIP the clip cleared; through a power cycle the clips, tracks, chords, SONG and settings;
  PROJECT 2 empty, 1 back. Found on the way: eight MIDI notes at once lost three in the chords' OFF mode (a note queued
  but not yet started was queued again with each new one: `ch_is_due`); a clip's note one tick late (the walk now
  includes the tick now); the project loader rejecting every project (its count); the transport's follower undoing a
  MIDI stop.
- **V0.8 BENCH, CORE 2** (132 checks; 122 screens and 573 knob turns, all clear): the page on the emulator's one core
  (STATE ONE CORE BUILD), TEST refused ("ONE CORE HERE"), ON ("ON AFTER A RESTART") and LOAD 16 -> 8 kept through a
  power cycle; two renders, the page as the emulator is and with a unit's figures after a test (example numbers, set by
  `web_test_c2`: nothing is measured here). The build checks core 1's waiting loop (in RAM, calling only its job) and
  that the voices' code is V0.8's (the listing: `voice_block` apart, `sy_block` in the interrupt).
- **V0.8, the sequencer** (130 checks; 120 screens and 570 knob turns, all clear): pattern 1 on PLAY, C4 E4 G4 C5 a
  step in four, at 0 4 8 12 16 steps, each half a step long (LENGTH 1/2), read from what USB OUT sends; SWING 75 %,
  NUDGE −25 %, CHANCE 0 % and TRANSPOSE +12 (72@0 74@1.5 76@1.75, step 4 left out, 72@4); DIRECTION BACKWARD (65 64
  62 60) and PING-PONG (60 62 64 65 64 62 60 62); live recording: REC's tap arms it ("REC ARMED"), C4 played at step
  2.1 goes into step 3 with NUDGE +11 % and LENGTH 6 quarter steps (held 0.2 s), E4 at 4.8 into step 6 with −20 %,
  not played again as the playhead came, played on the next pass; QUANTIZE: G4 near step 8 on it; REPLACE clears a
  pattern's four notes in an armed pass; by step: SEQ's NOTES, REC armed, C4 + E4 together into step 1 and step 2
  picked, then G4 into step 2; SEQ + OCT+ / OCT− the patterns, playing: "NEXT: PATTERN 5", switched at the pattern's
  end; CHAIN of pattern 1 twice then 2: 1 1 2 1 1 2; MOTION: a lane on the cutoff holds it at 1000 at step 1 (the
  engine follows 1000, the patch keeps its own), lets go at step 2, 0 at step 3, the patch's again when stopped; the
  cutoff's knob turned while recording takes a lane and writes 8 steps, the knob heard as it turns; PATTERNS: SEQ
  held, A3 picks pattern 3, F#3 copies, A#3 clears ("PATTERN 3 CLEARED: PASTE UNDOES"), G#3 pastes, held past 5 s with
  a key used: no reset; SEQ's taps (NOTES, FEEL, twice PATTERN) and SEQ held 5 s on SEQ (the steps cleared); the
  patterns, the pattern shown, CHAIN, RECORD and the chain's slots through a power cycle. The REC check now: a tap
  arms, again not, REC + OCT+ latches the MOD layer (REC twice no longer does); SEQ's: its menu. Found on the way: the
  patterns and their copy (14.3 KB) did not fit the spare SRAM's `.spare_bss` (7.9 KB free there: the "23,296 B
  left" counts the NOINIT area too) and went to the pool; the step pages' line under the title empty (the helper
  returned its end) and over the playhead's marker (its box 12 px, the columns 4 px lower). **Needs a hardware test**:
  the steps' timing by ear and on a DAW's clock over minutes, recording from the panel's keys and from MIDI (USB and
  TRS), a pattern's flash write after 3 s of quiet (the hush), the patterns through a power cycle on the unit.
- **The bloom over time** (`strata_bloom_study.mjs`): 17 frames of it (a note's swirl, a second note above, the
  release, a chord) on one sheet; read by eye.
- **The drawing's time**: six sessions now (FILTER's VALUES with a knob turning added); against V0.66 UI's tree on
  one machine, interleaved, 7 runs each: at or under it in each but HOME in silence (+5 µs in 2 s, nothing drawn);
  [UI.md](UI.md) §5.

### 6.6 The stress test (`web/emu/strata_fuzz.c`, V0.81)

The whole app (`strata_web.c`, natively) under AddressSanitizer and UBSan, driven at random: the buttons (taps,
holds, chords with OCT, a key while one is held), the keys, the knobs, MIDI in (notes on every channel, controllers,
start, continue, stop, the clock), the looper's own (launches, scenes, REC, UNDO PASS, notes and knob moves put in,
the tracks' settings in their ranges, the clips' lengths; V0.85: the grid's steps in and out and their hits' values,
the kit's values, hits of every row straight in), the pages, PLAY, MASTER, time; the audio rendered between.
After every step: the event pool (every event in one clip's list, the free list or UNDO's, none twice, the counts),
each clip (sorted, its count, inside its length; events in a clip of no length only while its take records), each
track's scenes and next event, the recording's lists, the chords' side in use the selected track's, the voices'
tracks, the parameters held; V0.85: the ratchets to come (their rows, tracks, counts), the kit in its ranges, the
duck's state and gain, each drum voice's track. 24 seeds of 3,000 steps (~15 minutes of play each) run clean on V0.81
and on V0.85. V0.86: TOOLS' paste (a clip or a bar, from any clip to any other), DOUBLE, HALVE, CLEAR's notes and
SPEED called straight (never on the clip recording, as the UI keeps it), and now and then SCREEN OFF AFTER 1 MIN with
a rest past it; every clip's SPEED checked in range. Its first run found three bugs in the new screens, fixed: the
EQ's SHAPES wrote "CUSTOM: AS POINTS SET IT" into 24 bytes and the latched MOMENTARY's banner "A KNOB · TAP LFO:
CLOSES" into 24 (each a byte past its buffer, on the stack), and the help map read a knob's name that can be none.
The 24 seeds run clean on V0.86 since. Step 2: now and then a factory kit loaded, or a drum's effect value, the duck's
SOURCE or HOLD, ACCENT's TONE or a COMP value set at random (out of range too, as a kit's sanity must catch); the
checks now take the duck's HOLD (state 3, its samples left in range) and COMP's level and gain (finite, the gain over
0 and at most its +9 dB). Its first run on step 2 stopped on the duck's state 3, HOLD, which the check did not yet
know: the check was out of date, not the code. The 24 seeds run clean on step 2.

    cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/strata_fuzz web/emu/strata_fuzz.c -lm
    for s in $(seq 1 24); do build/emu/strata_fuzz $s 3000 || echo "seed $s failed"; done

A failure prints its step; a third argument prints every step (`build/emu/strata_fuzz SEED STEPS v`).

## 7. Needs hardware test

- The instruction rate of this engine's float code, the cost of a float divide, the instruction cache's effect: the
  SETTINGS > BENCH (the hardware test build, §8.3) times two loops whose instructions the listing counts, and the
  whole engine's halves, with the 24 MHz timer.
- That the engine fits in RAM beside Felucca's platform, and the pool layout links.
- A table's build time (§2.1) and the gap it leaves when a table or a preset changes.
- The codec's output: levels, the −6 dBFS ceiling, DC at the jack.
- Everything the CPU guard does under a real load.
- The app on the device (after integration): the audio interrupt with its event rings, the load it reports, and the
  keys' latency while the screen redraws. The UI draws only what changed and sends only the rows that changed (the
  busiest live page 24 % of the SPI's time, §6.3), but the keys are read in the main loop, so a key can wait for a
  frame's drawing: the BENCH page times a frame; if it is long the keys move to the TIMER5 tick.
- The looper (V0.8): its timing by ear and against a DAW's clock, recording from the keys and MIDI (USB, TRS), the
  click's level, the projects' writes (objects at `0xCB000` and `0xB1000`, one a check) and their return at power-on.
- The second core (V0.8 BENCH): that it starts and answers on the unit, 30 s of jobs without a misread, its time for
  the oscillators' kernels against core 0's, how much core 0 slows while both run, its stack (FLASHING.md step 30).
- The flash on the device: an erase's and a page's IRQ-off time against the hush (the ring silent before any write);
  a SAVE's write and the background writes after 3 s of silence; that nothing outside the allow-list is ever touched
  (the RAM driver's second guard). Before any of it: a full flash dump (the brief's `docs/02-update-and-recovery.md`
  exists nowhere; [FLASHING.md](FLASHING.md), written for the hardware test, builds on the tools PHASE0.md §7 found).

## 8. The FM-1 build and its flasher (the hardware test build)

### 8.1 The build's checks (`tools/build.py --strata`)

Felucca's checks, unchanged, on Strata's unit: `_start` at the app's address and the entry stub; the RAM code (the flash
driver) with no calls into the flash; no reference into the chip's ROM; the image within the app slot (V0.65: 324,148
of 581,564 bytes; V0.6 407,008); `.data` + `.bss` with 4 KiB spare (65,152 of 73,728 bytes: 8.4 KiB) and the pool with
4 KiB (358,692 of 380,928 bytes: 21.7 KiB spare; Strata's `strata.ld` gives the pool 12 KiB of Felucca's main stack;
the margin was 8 KiB until integration had shown every platform buffer in it); every register access in `hal/`. That last one caught `SY_CBAR` (an empty barrier written in a
form the lint does not take): Felucca's own build had failed on it since Phase 2.

New for Strata, **the interrupt stack**. The audio's render and TIMER5, nested in it, share the system stack (7,936
bytes). From the listing: each function's pushes and stack adjustments make its frame; its depth is its frame plus its
deepest callee's (a call, or a tail call); calls through the oscillator kernels', the filter types' and the effects'
tables take every function their initializers name (read from the source, so a new kernel counts at once), and any
other call through a register fails the build. Now ~2.5 KiB (V0.6: the audio 2,228 bytes, TIMER5 196, the two
wrappers 56) of the 4 KiB allowed; the effects' table is the chain's (`FX_RUN`, a function a type). V0.65: a frame
too big for an immediate, made in a register (`rK = -N`, then `sp = rK`), is counted too (V0.6's 17 KB `ui_draw_disp`
frame was one: the count had seen 52 bytes of it), and an `sp` set from a register of no known value fails the build.

New in V0.65, **the main stack** (`strata.ld`: 12,032 bytes, Felucca's 24,320): counted the same way from the power-on
(`fm1_cstart`), the display's graphs through their table (`UI_GRAPH`), held to 8 KiB (the rest for the flash's hooks
and the console's commands, called through a register, which the count does not follow). Now ~6.6 KB: `fm1_cstart` >
`sy_ui_frame` > `ui_draw_disp` > `ui_g_delay`. And **the size pass** (`tools/strata_drivers.py`): Strata's main-loop
files at `-Os` (331 functions), what an interrupt reaches kept at `-O2`. With the pass on and off the audio interrupt,
`voice_block`, `fx_ffilt`, TIMER5 and `usb_poll` are the same instructions (compared in the listings, addresses
aside); Felucca's own build is byte for byte what it was (512,144 bytes).

The package has Felucca's identity (`FM-1_900`), update loader and layout: 610,090 bytes. The first device build also
found three `strlen` calls in the effects' drawing (the host and Emscripten have one, the firmware not).

### 8.2 The flasher (`web/strata_flash`, `flash_test.mjs`, 26 checks; `tests/serve_backup_test.py`, 13 tests)

The page as `make_flasher.py` makes it (Felucca's `fm1ota.js` and `fm1pkg.js`, unchanged, inlined), in headless Chrome
against a simulated FM-1 on Web MIDI (after Felucca's `FakeFM1`: the handshake, step 1's reads, the reset into the
update loader, step 2's reads, the reset into the new firmware; every byte served compared with the package) and a
simulated console on Web Serial:

| What | Checked |
| --- | --- |
| as it opens | the package's size, SHA-256 and identity checked; no confirmation, bar, Stop or report drawn; Find off until a backup from the last 12 h and all three boxes; no exception |
| the backup step (serve.py's, simulated by the test's server) | none yet: Find and V15 off; Back up now sends the token and the page's Origin once; waiting (a device Windows could not read named), reading, verified, Find on, the status saying to switch the FM-1 off and on; Find with it still in backup mode says so; the server not an administrator, or a backup 20 h old: the reason shown, Find off |
| an install over the official firmware | Find names it (`FM-1_015`, official) and draws its confirmation; both steps, every byte right (47 reads), `FM-1_900` back, the bar full; not Done until the simulated FM-1 is switched off and on and answers |
| the 2026-10-16 lockout | Strata written, then mute after the restart: never Done, the Flipper route named |
| the FM-1 in update mode | Find says so; the write finished (41 reads, every byte right), then the restart check |
| refusals | a device that is no FM-1 (nothing sent); no FM-1; Cancel (nothing sent); an "official" file that is not V15; the package changed by one byte (Find stays off) |
| backup mode | a click on its button before its confirmation does nothing; after it, the soft key once and no update |
| the report | `status` and `bench` read and shown, Copy on; RUN: `bench run`, then `status` and `bench` |
| layout | no sideways scroll at 390 px; the dark scheme |

`tests/serve_backup_test.py` (Windows' Python, after `tools/build.py --strata`; it uses flipper-fm1-recovery's
`recovery_plan.py` beside the checkout) runs the server's backup job with a fake `pc_backup.py` and disk watcher:
verified with its receipt and the FM-1 let go; refused for reads that differ, a backup that stops, another chip key, a
boot head that does not check out; stopped while waiting, nothing read; no disk in time; the 12-hour rule, and files
changed after the receipt; why it is off; and HTTP: another Host, a POST without the token or this page's Origin.

**The hardware test page** (`web/strata_flash/test.html`, `test_test.mjs`, 21 checks). `make_flasher.py` gives the
page the facts of the app it ships: the RAM addresses of the patch's values (`sy_patch`) and of the audio side's
(`sp_n`) from `build/strata.syms` (written by `build.py`), the parameter table read out of `build/strata.bin`
(names, sections, ranges, flags, CCs: checked row by row) and three runs of its bytes as a fingerprint. In headless
Chrome against a simulated FM-1 (its console's `status`, `bench`, `crash`, `inp` and `memr` answered from a model of
the patch, whose values glide to a change within a few ms as `sy_param.c`'s do; its CCs mapped as `sy_app.c` maps
them; the bank as a program change arrives), with the page's waits a tenth (`?fast`): as it opens; the automatic run
on a good FM-1 (9 rows OK, 32 chords, the CC 74 value stored and followed, a user preset put back by bank and program);
no SysEx asked for or sent, every console command one of the read-only six, every note ended; PROGRAM off (said, the
load on the sound playing alone); another build on the FM-1 (said, the RAM checks skipped); the five fixed effects'
knobs followed (OK) and, with V0.6's bug simulated, stuck (FAIL, each module marked); the chain's seven steps seen;
the encoders (OK; WARN for a quiet one and lost transitions); the keys' wait (one console command, after the 25 s);
three restarts reconnected and read, and one with a crash record (FAIL); no sideways scroll at 390 px; the dark
scheme. Found by it: a backspace in the page's `?fast` pattern (a shell's heredoc), and the five effects' step ending
before its RING MOD question could be answered. The page's USB audio check (the gaps: runs of digital silence inside
a sounding note) has not run against a real stream: **unverified** until the hardware test.

Found by its screenshots: every confirmation was drawn from the start (a `display` rule beat the `hidden` attribute).
The buttons behind two of them did nothing until Find; the backup mode's would have worked at once. Fixed (a
`[hidden]` rule and the backup's own guard); the test now checks what is drawn, not the attribute.

### 8.3 What BENCH measures (SETTINGS > BENCH, the console's `bench`)

- **AUDIO**: a half's render in µs (TIMER5's nested time taken off), the last and the longest, and the halves the DMA
  had already left when their render ended (late). V0.66: those right after the interrupts were held (an erase, a
  page's program, RUN: the audio hushed) are counted apart (the console's `halves_late_held`).
- **CPU**: the guard's smoothed load, its budget steps and the voices it shed.
- **SCREEN**: a frame's drawing (`sy_app_frame`), the last and the longest, in ms; V0.66: and the section of the
  longest (the console's `frame_max_page`).
- **KEYS**: the longest time between two of the main loop's millisecond passes: the keys, MIDI and the knobs wait at
  most that long. V0.66: while the keys only play, TIMER5 plays them (10 kHz) and this counts nothing; it counts while
  the main loop has them (a layer, a screen that types), drawing and, apart, over flash work ("11.2 · SAVES 52.1 MS").
- **The event log** (V0.66, the console's `bench`, the last 32: `ev MS WHAT ARG`): late halves (their render, µs),
  late halves after held interrupts, the USB audio's underruns, missed packets and stalls, the flash's operations,
  sounds loaded, voices shed, notes refused a free voice, TIMER5 held over 0.6 ms, the keys left waiting over 20 ms
  (drawing or saving) and frames over 50 ms.
- **TABLES**: a wavetable build's longest step (a millisecond pass while it builds) and the whole build.
- **FLASH**: the longest interrupts-off window of an erase and of a page's program (TIMER4 around the RAM driver's
  call), and the erases so far.
- **STACKS**: both stacks painted at power-on (below where the boot runs, 512 bytes clear), their unpainted depth:
  the interrupts' / the main loop's, in KB (V0.65; "IRQ 2.2 · MAIN 17.9 KB" before). Then **SPARE OK** or **SPARE HIT**:
  the 44 KiB of SRAM Strata leaves unused (BUDGET.md §4), painted at power-on, every word checked each half second;
  HIT means something wrote there (the console's `bench`: `spare_bytes`, `spare_changed_bytes`).
- **RUN**: two loops (100,000 integer and 50,000 float iterations) timed with the interrupts off behind the hush;
  their instructions are counted in `build/strata.dis` (`sy_run_int`, `sy_run_flt`), so their times give the core's
  rate.
