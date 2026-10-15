# Phase 0: discovery and feasibility

Working name: **Strata** (a wavetable synth firmware for the M-VAVE FM-1, forked from Felucca 1.6.1).
This report is the Phase 0 checkpoint: what the hardware and the base give us, measured or
documented, and what a Serum-style instrument can afford on it. Nothing here ran on an FM-1 in
this project. Every figure is labelled with where it comes from:

- **[device, src]**: measured on a real FM-1 by someone else; the source is named
- **[repo]**: read from Felucca's source, linker script or build output (exact)
- **[host]**: measured here on the build machine (WSL2 Ubuntu 24.04)
- **[listing]**: counted in the pi32v2 disassembly from the JieLi compiler (exact count, estimated time)
- **unverified**: not confirmed yet; how to check it is given

## 1. Toolchain and base: verified here

| Item | Result |
| --- | --- |
| JieLi toolchain | `jieli-linux-toolchains-20260730.1`, clang 4.0.1 for pi32v2, in WSL2 `~/.jieli/toolchain` [host] |
| AC79 SDK files | `uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin` from `AC79NN_SDK_V1.2.1_2023-12-13` (gitee, sparse checkout); SHA-256 match `tools/build.py` [host] |
| Felucca 1.6.1 build | `./build.sh` passes all checks: image 511,468 B, `.data+.bss` 60,984 of 73,728 B, pool 328,896 of 368,640 B, package 610,090 B, identity `FM-1_900` [host] |
| Felucca host tests | 3,060 checks pass; the run stops at `tests/drumfuzz_test.c`, which misses `#include <stdint.h>` on Linux gcc 13 (macOS headers hide it). Upstream portability bug; one-line fix in the fork [host] |
| Regression suite | 98 golden renders unchanged, 0 health failures. Its CPU check uses macOS's `proc_pid_rusage`: on Linux it reports no counts, so host CPU is costed here from the target listing instead [host] |
| Browser emulator | emcc 6.0.12; `web/emu` builds (772 KB wasm) and its test passes; the native and wasm renders match bit for bit [host] |

Windows git converts line endings on this machine (`core.autocrlf=true`), which breaks the shell scripts under WSL. Every
clone here uses `-c core.autocrlf=false`; the build instructions will say so.

## 2. Hardware and base resources

### CPU

| | | |
| --- | --- | --- |
| SoC | JieLi AC791N (WL82), two pi32v2 cores | FM-1-RE `docs/architecture.md` [repo] |
| Clock | **360 MHz** (PLL 540 MHz × 2/3; HSB 180, LSB 60 MHz). Felucca never programs the PLL; this is what the boot loader leaves. The stock firmware itself switches to 240 MHz | [device, X0X `docs/plans/2026-10-04-dual-core-and-clock.md`, read from the PLL registers on a unit]; ChoralRoot `docs/DUALCORE.md` (Melodee) agrees |
| Throughput | **~205 listing instructions per µs** on integer code (1.76 cycles each on average) | [device, ChoralRoot `docs/CZ1.md`: listing counts against a measured render time] |
| Branches | a one-instruction branch loop takes 7 cycles | [device, X0X, same doc]. The compiler also emits zero-overhead `rep N rX { }` hardware loops [listing] |
| Issue | pairs of instructions issue together (`a # b` in the listing) | [listing] |
| FPU | single precision, works on the general registers: `-mcpu=r3 -mfprev1`. One-instruction divide, no square root. Doubles are software | [listing: a float multiply is one 4-byte instruction with these flags, a libcall without]; shipping on units in X0X, FoMni, Polly, AMB-1 (`tools/build.py` of each). FPU timing on the device: **unverified** |
| Integer DSP | 32×32→64 multiply and multiply-accumulate (`r1_r0 += r3 * r2`), hardware divide, `clz`, `smax`, bit-field extract, post-increment loads | [listing] |
| I-cache | 32 KB, shared by both cores; code runs from XIP flash through it | X0X notes; **unverified** |
| Second core | Felucca 1.6.1 has it as a beta (MENU > SYSTEM > 2ND CORE, off by default). A heavy 4-track project: one core 65–74 % (slowest half 3,285 µs, late halves), two cores 35–51 % | [device, Felucca PR #247]. X0X: 43/83/115 % → 39/54/68 % [device]. ChoralRoot raises the core rails before starting it (Melodee found misreads at the boot loader's voltage) |

### Memory

512 KiB SRAM in all [repo, `firmware/app.ld`]:

| Region | Size | Use in Felucca |
| --- | --- | --- |
| RAMTEXT `0x01C00000` | 24 KiB | code that runs while the flash is busy (flash driver, ~3 KiB) and core 1's waiting loop; room for hot DSP loops |
| RAM (`.data`/`.bss`) | 72 KiB | ISR-side state; Felucca uses 61 KiB (sequencer/song structures 20 KiB of it) |
| POOL (`.pool`) | 360 KiB | big buffers; Felucca: delay line 128 KiB, LCD canvas 58 KiB (240 × 124 px), engine memory 50 KiB, slicer 32 KiB, reverb 10 KiB, chorus 4 KiB |
| Stacks | 24 + 8 KiB | user and supervisor stacks with 256 B guard bands |
| NOINIT, boot area | 16 KiB | crash/diagnostic records, boot info, mailbox, vectors |

### Flash (1 MiB SPI NOR, XIP at `0x02000000`)

| Range | What | Who writes it |
| --- | --- | --- |
| `0x00000–0x03FFF` | flash header, SPL (`uboot.boot`), `isd_config` (chip key) | never at runtime; the update loader never either |
| `0x04000–0x92FFF` | app area: `app.bin` slot **581,564 B** (code + constants), `cfg_tool.bin`, `cfg` | the update loader only (package, M-UPGRADE path) |
| `0x93000–0x96FFF` | VM region head | nothing (loader may clear an update record here) |
| `0x97000–0xDFFFF` | **292 KiB user store**: Felucca projects, user sample slots (240 KiB), user presets | the app (allow-list `FL_DATA`) |
| `0xE0000–0xE4FFF` | OTA staging for the update loader | the app (`FL_OTA`) |
| `0xE5000–0xE7FFF` | autosave A/B, FM6 voice bank | the app |
| `0xE8000`, `0xE9000` (BTIF), `0xEA000–0xFBFFF` (USR) | stock firmware's VM backup, Bluetooth pairing, stock user patches | **never** |
| `0xFC000–0xFEFFF` | settings A/B, one more A/B copy | the app (`FL_GLOB`) |
| `0xFF000–0xFFFFF` | `key_mac` (unit-unique) | **never** |

The runtime guard is `FL_RANGE_OK` in `firmware/hal/fm1_flash.h`, checked inside the RAM-resident erase/program routines.
The update loader has its own window (`firmware/loader/loader.c`) and writes only `[0x4000, 0x93000)`. Both stay unchanged
in the fork, with compile-time asserts added for every new object Strata stores.

### Audio

| | |
| --- | --- |
| Output | I2S (ALNK0) to an external codec, 24-bit words; the Q15 mix is shifted to a −6 dBFS ceiling (`OUT_SHIFT`) [repo] |
| Sample rate | **44,117.6 Hz** (24 MHz / 544). Felucca computes pitch with 44,100: +0.7 cent sharp [repo] |
| Buffers | two halves of **128 frames (2,901 µs)**; rendered in the ALNK0 interrupt (priority 3) in blocks of **32 samples** (control rate 1.38 kHz) [repo] |
| Overload | two halves in a row over 85 %: a voice is faded out (`audio.c shed_voice`) [repo] |
| USB audio | UAC1 input to the computer (the master out), 44.1 or 48 kHz [repo] |

### Panel, display, MIDI

| | |
| --- | --- |
| Keys | **27** (F3–G5), switches in a diode matrix: **no velocity, no aftertouch** (Felucca plays a set velocity) [repo] |
| Buttons | **14**: HOME, ENV, LFO, FX, SCL, EDIT, GLO, SAVE, ARP, SEQ, REC, PLAY, OCT−, OCT+ [repo] |
| Encoders | **7**, detented, relative, no push: SELECT, ALGORITHM, PRESETS, KNOB 1–4 [repo] |
| Pot | MASTER volume (ADC) [repo] |
| Wheels, pads, faders | **none** on the panel (FM-1-RE finds wheel-calibration code in the stock firmware, inherited from other models; Felucca reads no wheel) |
| LEDs | one under every key and button (41), three levels (lit, glow, breathing/mid) [repo] |
| Display | 240 × 240 RGB565, SPI at 12 MHz: a 240 × 40 strip takes 12.8 ms, the full screen ~77 ms, so animated graphics must redraw only what changes [repo, ChoralRoot `docs/EDITOR.md`] |
| MIDI | USB-MIDI in/out (class compliant), TRS MIDI **in** (UART1 RX on PH8, verified on hardware by Felucca). TRS MIDI **out: none** (resolved after Phase 0, see the update at the end) |
| Other | battery, speaker and headphones (codec), USB-C |


### Felucca's existing load

Read off units by others [device]: Felucca, heavy 4-track project 65–74 % one core / 35–51 % two cores (PR #247); X0X
factory loop 43 %, all parts 83 % (one core); ChoralRoot CZ-1, 8 voices 1,699–1,985 µs a half before its rewrite. Felucca's
INFO page shows the same meter (`song.cpu_q8`: render time over the half). Reading it on your unit is optional and not
needed for the decision below.

## 3. Budget

### CPU

Per-sample costs counted in the target listing (`tools/costprobe/run.sh`, JieLi clang `-O2 -mcpu=r3 -mfprev1`;
compiled C, not hand-tuned), converted at 205 instructions/µs for integer kernels and an assumed **150/µs for FPU-heavy
ones** (no device figure for the FPU yet) by `tools/costprobe/budget.py`:

| Kernel | Instructions / sample | µs per half (128 samples) |
| --- | --- | --- |
| Wavetable oscillator instance (int16 mips, 2-frame crossfade, linear interpolation, stereo out) | 37 | 23.1 |
| + a phase warp on it (estimate) | +10 | +6.2 |
| SVF multimode, float, per channel | 21 | 17.9 |
| 4-pole ladder, float, per channel | 28 | 23.9 |
| Amp envelope + pan + mix, stereo | 13 | 11.1 |
| Drive (soft clip), stereo | 22 | 18.8 |
| Chorus (2 modulated taps a side) | 48 | 41.0 |
| Delay (stereo/ping-pong, int16 lines, feedback filter) | 76 | 64.9 |
| Reverb (4-line FDN, damping) | 176 | 150 |
| OTT (3 bands, up/down, per-sample split) | ~170 | ~145 |

A voice is two oscillators × unison + sub + noise + a stereo filter + amp + control-rate work (envelopes, LFOs, matrix,
pitch per unison voice): **92 µs fixed + 23 µs per oscillator instance** a half. With the planned ceiling of 75 % of a
half, minus 8 % for the system (key scan, USB, the ISR's own work), and the whole FX rack on (445 µs, 15 %):

| Unison per oscillator (2 osc) | Voices (no warp) | Voices (warp on both) |
| --- | --- | --- |
| 1 | 10.9 | 10.0 |
| 2 | 8.1 | 7.2 |
| 3 | 6.5 | 5.6 |
| 4 | 5.4 | 4.6 |
| 7 | 3.6 | 3.0 |
| 16 (mono lead) | 1.8 | 1.5 |

What this means:

- **8 voices × 2 osc × 2 unison**, or **6 × 2 × 3**, fits on one core with every effect on. A mono or legato lead with
  2 osc × 16 unison and warps costs ~36 % of a half: big supersaws are affordable as long as they are not 8-note chords.
- 8 voices × 2 osc × 3 unison (a typical poly pad) needs ~20 % more: tuning the oscillator loop for this core (the compiler
  spills loop invariants; `rep` hardware loops avoid the branch cost) and/or the second core. I expect to get there but
  will not promise it before the device benchmark.
- Unison is therefore **budget-scaled**: a patch asks for N unison voices; when the notes sounding need more oscillator
  instances than the budget allows, the oldest voices step down in unison count with a short crossfade (no click), before
  any voice is stolen. The CPU guard then sheds in order: unison on older voices, the FX quality (OTT, reverb density),
  then whole voices (faded), as Felucca's shed does.
- Control-rate modulation every 32 samples with per-sample linear ramps on gains, cutoff and position (no zipper noise).

The **device BENCH page** (first integration build) times each kernel with the 24 MHz timer on the unit and reports the
clock; it replaces every assumed rate above. That needs a device install, so it waits for your go-ahead.

### RAM (plan)

| Block | KiB | Notes |
| --- | --- | --- |
| Wavetables, both oscillators | 144 | int16, octave mips with 4× oversampling: a frame is 1,024 samples at the bottom (up to 256 harmonics) down to 32 at the top, 4.06 KiB a frame; **16 keyframes per oscillator** with smooth crossfaded position (a shared pool: one table may take more frames when the other takes fewer) |
| LCD canvas | 58 | Felucca's 240 × 124 canvas, kept |
| Delay | 80 | stereo int16 at half rate (filtered): up to 0.93 s a side (1/4 note down to 65 BPM) |
| Reverb | 24 | FDN or plate, int16 lines |
| Chorus | 4 | |
| Generator scratch | 16 | building a table at load (FFT), freed for nothing else while idle |
| Storage, voice/mix buffers | 14 | |
| Pool total | **340 of 360** | 20 KiB spare (build.py demands 8) |
| `.data/.bss` | ~45 of 72 | USB/MIDI rings, UI, 8 voices with up to 16 unison phases each, the patch and its undo copy |

The alternative is 512-sample frames (up to 128 harmonics, 2.06 KiB a frame, 32 keyframes per oscillator). It loses the
top octave of notes below F#2 (an A1 bass would stop at 7 kHz). Basses are your first priority, so I recommend the
1,024-sample option. The offline harness will measure the aliasing of both before this is final.

### Flash (plan)

| | KiB |
| --- | --- |
| App slot | 568 |
| Kept platform (HAL, USB, MIDI, storage, update entry, LCD/fonts/icons, UI framework) | ~230 (estimate) |
| Strata engine, FX, modulation | ~50 |
| Strata UI pages and help | ~60 |
| Factory presets (64–128) | ~30 |
| Factory wavetables: on-device generator + recipes, plus stored spectral keyframes | ~70 |
| **Spare** | **~120** |

Factory wavetables cannot be stored as raw frames: one 16-frame table at 1,024 samples is 32 KiB. Most of the factory set is
made by an **on-device generator** from recipes of a few bytes each (analog morphs, PWM, sync, FM ratio/index sweeps,
additive tilt/odd-even, formant/vowel, wavefold, phase distortion, digital/quantised). It renders each keyframe and band-limits
it with an FFT into the mips at preset load (~10–20 ms, under a short fade). A few tables that are not formula-like are
stored as compact spectra. `tools/gen_wavetables.py` documents and previews every recipe from the same definitions.

The user store (`0x97000–0xDFFFF`, 292 KiB, Felucca's own allow-listed range) holds 128 user preset slots in A/B
sectors, the autosave and, later, imported user wavetables. Nothing new is written anywhere else.

## 4. What Serum has that does not fit, and the stand-in

| Serum 2 | Cost here | Strata |
| --- | --- | --- |
| 3 wavetable oscillators | +40–50 % per voice | 2 wavetable + sub + noise (as your MVP list) |
| 256 frames × 2,048 samples (2 MiB a table) | 4 × all of RAM | ≤16 keyframes per oscillator (1,024-sample mips), continuous crossfaded position |
| 16 unison on every note | 8 × 2 × 16 = 256 instances | 16 in mono/legato; poly unison scaled to the budget |
| ~80 filter models, oversampled circuit models | many times the voice budget | SVF LP/HP/BP/notch/peak 12/24, a ladder-style LP with drive, comb ±, formant (vowel); a second filter in v1 |
| Multisample, sample, granular, spectral oscillators | sample memory and CPU | stretch: Felucca's SAMPLE/GRAIN engines and user sample slots (granular time-stretch) |
| 64 matrix slots, 10 LFOs, 4 envelopes, 8 macros | UI more than CPU | 8 slots, 2 LFOs, 2 envelopes, 4 macros (MVP); 16 / 4 / 4 / 8 in v1 |
| Convolution, Bode, oversampled distortion, Nitrous reverbs | CPU/RAM | algorithmic plate/hall reverb, waveshapers without oversampling; Bode/flanger/phaser/hyper in v1 as CPU allows |
| Clip sequencer, wavetable editor, formula parser | | none on the device; a web editor page is stretch |

## 5. Your additions

| Wish | Plan |
| --- | --- |
| Basses, leads/supersaws, pads, plucks/keys/arps | the 1,024-sample mips favour basses; budget-scaled unison for leads; pads get smooth WT position and slow LFOs; plucks get fast curved envelopes |
| Pitch bending | MIDI bend (range per patch, up/down separately) and a **panel bend**: a knob layer that springs back to centre when you stop turning; glide with time, curve, always/legato, scaled |
| Delay and reverb | in the MVP rack: tempo-synced stereo/ping-pong delay with feedback filter; plate and hall reverb |
| OTT (multiband up/down compression) | in the MVP rack: three bands, depth, time, up/down amounts, band gains; ~5 % CPU |
| Time stretching | stretch tier: a granular sample oscillator (Felucca's GRAIN engine and sample slots) whose position moves at any rate independent of pitch. A cheaper relative ships earlier: wavetables made from audio slices, scanned by an envelope or LFO |

## 6. Proposed scope: please confirm

**MVP** (everything in your MVP list, plus pitch bend and OTT):

- OSC A and B: wavetable, ~48 factory tables, WT position, **6 warps** (Sync, Bend ±, PWM, Asym ±, Mirror, FM from B),
  octave/semitone/fine, phase, random phase, phase memory, level, pan, unison 1–16 (detune, blend, width, linear/super
  spread, random phase)
- SUB (sine, rounded rect, triangle, saw, square, pulse; octave), NOISE (white, pink, brown; colour, stereo)
- One filter per voice, stereo: SVF modes, ladder-style LP with drive, comb, formant; cutoff, resonance, drive,
  key tracking, variant knob, mix; per-source routing
- ENV 1 (amp) and ENV 2: attack, hold, decay, sustain, release, curves
- LFO 1 and 2: shapes, Hz or tempo sync, free/retrigger/one-shot, delay/rise, mono/poly
- 4 macros; 8-slot matrix; sources: envelopes, LFOs, macros, velocity, note, mod wheel, bend, aftertouch (MIDI), note
  random, note alternate
- Mono, legato, poly (8), glide, voice stealing with fades, budget-scaled unison, CPU guard
- FX rack: drive → chorus → delay → reverb → OTT → limiter, each with mix and bypass, tempo sync
- MIDI: USB and TRS in, USB out, clock in, CC map, program change, MIDI learn (Felucca's)
- 64+ factory presets with macros, 128 user slots, categories, save/undo, restore on boot
- UI: Felucca's conventions, Serum-style hold-source-turn-target modulation, graphics for waveform, filter, envelope, LFO
- Host harness (aliasing sweeps, clicks, NaN/denormal, filter stability, determinism, cost), emulator, CONTROLS.md
  and the other docs

**v1**, after the MVP is verified on your unit, one feature at a time while CPU allows: second filter
(serial/parallel), ENV 3–4, LFO 3–4 with drawn shapes, 16 slots with aux source and curves, more warps (remap curves,
flip, quantise, odd/even, FM/PD/AM/RM from other sources), unison stacks (12, 12+7, centre −12/−24), flanger, phaser,
hyper/dimension, EQ, arpeggiator, preset morphing, 8 macros, the second core as an option.

**Stretch**: granular/sample oscillator with time stretch, wavetable import from a web editor page, MPE-lite.

## 7. Notes for the next phases

- Package identity stays Felucca's (`FM-1_900`, release `FM-1_9XY`), as do the update loader, the package format and
  the flash map, so M-UPGRADE and **Return to official V15** keep working. Firmware name and version string only change on
  screen and in file names.
- Felucca's 4-track sequencer, song mode and engines are replaced, as X0X and ChoralRoot do; its platform (HAL, USB/MIDI,
  storage, update path, LCD and UI framework, menu, MIDI learn, help, emulator, test harness) is kept.
- `docs/02-update-and-recovery.md`, named in the brief, exists in none of Felucca, FM-1-RE (either branch), FM-1-transporter
  or the other repositories; MvaveFM1Unbricker and mvave-fm1-open-firmware could not be found. FLASHING.md will be built on
  FM-1 Transporter (full 1 MiB dump in ~3 s, sector-verified writes), kagaimiq's jl-uboot-tool (yours) and Felucca's
  recovery notes. If you have that document, please share it.

## Update after the checkpoint

- **TRS MIDI out does not exist on the FM-1.** Its only MIDI jack is an input: PCB photos show the receive
  optocoupler (`PC1`) at the jack's pads and no TX pin is known (ip2k/mvave-fm1-open-firmware `docs/01-hardware.md`);
  CTL-1, a MIDI controller firmware, states "the FM-1 has no MIDI out jack" (charlesvestal/fm1-ctl `docs/MANUAL.md`);
  Felucca's HAL has RX only. MIDI out is USB; a USB-MIDI host adapter gives DIN/TRS out without a computer.
- **Recovery tools found** (via aroum/fm1-custom-fw): Quixotic7/MvaveFM1Unbricker (jl-uboot-tool backup/restore that
  refuses to touch `< 0x4000` and verifies SHA-256), ip2k/mvave-fm1-open-firmware (`docs/07-recovery-and-risk.md`,
  the USB_KEY dongle), merthsoft/flipper-fm1-recovery. FLASHING.md will build on them.
- **The stock VM store** (`0x94000–0xE8FFF`: the stock firmware's settings, a copy of the Bluetooth MAC, wheel
  calibration it inherited) contains Felucca's user store. Felucca-family firmwares overwrite that part; the stock
  firmware rebuilds its settings when it returns. Only a full flash backup restores the original contents.

## Update after Phase 2

- **TRS MIDI out is dropped** (your decision at the start of Phase 2). MIDI out is USB.
- **RAM, measured** on the target build of the engine (TESTING.md §4.4), replacing the plan's table in §3: the pool
  holds the wavetables at 182.6 KiB (32 frames of 5,842 B: the upper levels are oversampled more than the planned 4 ×,
  against aliasing), the delay at 64.6 KiB (750 ms a side: your decision, so that the pool fits), the reverb at
  19.9 KiB, the generator's scratch at 11.4 KiB and the chorus at 4 KiB: 295,638 B in all. With Felucca's screen
  canvas (59,520 B), 13,482 B of the 360 KiB pool stay free, over its 8 KiB margin. `.bss` holds ~20 KiB, the eight
  voices 14.2 KiB of it.
- **CPU, estimated** for the whole engine by the target cost model (TESTING.md §4): well over this report's §3
  figures, mostly control-rate work and effects the Phase 0 estimate left out or underrated. An optimisation pass
  took about 27 % off: the effects ~1,100 listing instructions a sample (~520 estimated in Phase 0), a voice of two
  oscillators and a filter ~276. At 205 instructions a µs, one core holds about 10 such voices without effects and
  about 6–7 with every effect (4–5 at 3 unison each). The rate of this float code on the device is still unmeasured
  (the BENCH page).
