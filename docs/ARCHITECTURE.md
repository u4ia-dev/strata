# Strata architecture

Strata is a wavetable synthesizer firmware for the M-VAVE FM-1, organised the way Xfer Serum 2 is (wavetable
oscillators with warps and unison, a filter, envelopes, LFOs, macros, a modulation matrix, an effects rack) and cut to
what the FM-1 can run. It is a fork of Felucca 1.6.1 (GPL-3.0-only): Felucca's platform (HAL, USB, MIDI, storage, the
update path, the display and input layers, the menu, MIDI learn, the emulator and the test harness) stays. Felucca's
instrument (four tracks, engines, sequencer, song, projects) is replaced by one polyphonic synth.

The numbers behind every choice are in [PHASE0.md](PHASE0.md). Status words used below: **verified on host**,
**verified in emulator**, **needs hardware test**. Nothing in this document has run on an FM-1 yet.

**After Phase 2** (the DSP core and its host harness): the engine described in §4–§14 is written (`firmware/src/sy_*`)
and **verified on host** (81 checks, [TESTING.md](TESTING.md)). Where the build differs from the plan below, the text
says so. Two findings change the plan: the whole engine costs well over what Phase 0 assumed (after an optimisation
pass, eight voices of two oscillators and a filter fit one core at 205 instructions a µs, unison with every effect does
not: §13, TESTING.md §4), and the delay went to 750 ms a side so that the pool fits beside the screen's canvas (§20).

**After Phase 3** (the emulator and WebAssembly): the engine built as WebAssembly passes the same 81 checks under Node
and plays the test sounds bit for bit as the host build does; a browser workbench plays the DSP with every parameter
(`web/strata`); and the app skeleton `sy_app.c` runs on Felucca's simulated FM-1 (`web/emu`), **verified in emulator**
(12 checks): the audio interrupt with its event rings, MIDI in, the parameters four to a page, the family buttons, the
starter sounds, the LEDs and a placeholder screen (TESTING.md §6). Nothing is built for the FM-1 yet: that is
integration (Phase 6).

Contents: 1 constraints, 2 source layout, 3 real-time model, 4 signal flow, 5 number formats, 6 wavetables,
7 oscillators, 8 sub and noise, 9 filter, 10 envelopes and LFOs, 11 modulation, 12 parameters, 13 voices and the
CPU guard, 14 effects, 15 MIDI, 16 presets and storage, 17 user interface, 18 testing, 19 the second core, 20 risks.

## 1. Constraints (from Phase 0)

| | |
| --- | --- |
| CPU | one pi32v2 core at 360 MHz for audio (the second is a v1 option), ~205 listing instructions/µs on integer code; single-precision FPU on the general registers (`-mcpu=r3 -mfprev1`), timing on the device unknown |
| Audio | 44,117.6 Hz; 128-frame halves (2,901 µs) rendered in the ALNK0 interrupt in four blocks of 32 samples |
| Budget | plan for ≤ 75 % of a half at the worst case (Felucca sheds voices at 85 %); ~8 % goes to the system |
| RAM | 72 KiB `.data/.bss` + 360 KiB pool (+ 24 KiB RAM code) |
| Flash | app slot 581,564 B; user store `0x97000–0xDFFFF`; settings `0xFC000–0xFEFFF` |
| Panel | 27 keys (no velocity), 14 buttons, 7 relative encoders (no push), MASTER pot, an LED under every key and button, 240 × 240 screen (~77 ms for a full redraw) |
| MIDI | USB in/out, TRS in only (the FM-1 has no MIDI out jack: §15) |

## 2. Source layout

Felucca builds the app as one compilation unit (`firmware/src/felucca.c` includes every `.c`), so the compiler sees
everything and nothing is exported. Strata keeps that: `firmware/src/strata.c` is the unit, `tools/build.py` compiles it.
The DSP files carry the prefix `sy_` (Felucca's `storage.c` already uses `st_`); the table below gives the plan's names,
and Phase 2 built: `sy_core.h` (sizes, rate, math), `sy_wt.c`, `sy_wtgen.c`, `sy_param.c` (the parameter table and
the preset format), `sy_osc.c`, `sy_env.c`, `sy_lfo.c`, `sy_mod.c`, `sy_filter.c`, `sy_voice.c` (sources, routing, a
voice's block), `sy_fx.c` and `sy_engine.c` (notes, allocation, the unison budget, the CPU guard, the block, patch
changes, table builds); tests in `tests/sy_*`. Phase 3 added `sy_text.c` (a value as its unit shows it, for the knob
cards and the workbench), `sy_sounds.h` (the eight starter sounds, until Phase 5's factory presets) and `sy_app.c`
(the app around the engine on Felucca's HAL, `panel.c` and `gfx.c`: the audio interrupt, the event rings, MIDI in, the
pages, the LEDs, a placeholder screen); `strata.c`, the unit that includes Felucca's platform and `sy_app.c`, comes
with integration. In the browser: `web/strata` (the workbench: the DSP alone) and `web/emu/strata_web.c` (the app on
the simulated FM-1). Phase 4 adds the UI: `sy_gfx.c` (the palettes by colour role, anti-aliased arcs, discs,
segments and curves on `gfx.c`'s canvas), `sy_ui.c` (pages, families, gestures, the knobs, the MOD layer) and
`sy_ui_draw.c` (the five screen regions, the graphs, the LEDs); `sy_app.c` keeps the audio, the events, MIDI in, the
sounds and the keys.

| Kept from Felucca (unchanged where possible, so upstream fixes merge) | Replaced or new in Strata |
| --- | --- |
| `firmware/hal/*` (all of it), `firmware/loader/*` (the update loader, byte for byte), `crt0.S`, `app.ld` (region sizes may move between RAM and POOL) | `strata.c` (the unit), `st_core.h` (types, sizes) |
| `libc.c`, `lcd.c`, `gfx.c`, the generated fonts, icons, keycaps and palettes | `st_param.c` (the parameter table), `st_patch.c` (patch, undo, preset format) |
| `usb.c` (USB-MIDI, UAC audio input, CDC console, SysEx frames), `midi_uart.c` (TRS in) | `st_wt.c` (wavetable pool, mips), `st_wtgen.c` (generator, FFT, recipes), `st_osc.c` (oscillator kernels, warps, unison) |
| `fm1_flash.h` driver, `storage.c` (A/B sector objects), `storage_hw.c` | `st_voice.c` (sub, noise, filter, amp, a voice's render), `st_env.c`, `st_lfo.c`, `st_mod.c` (matrix, macros) |
| `ota.c`, `ota_hw.c` (the M-UPGRADE entry), `console.c` (trimmed to Strata's state), `master.c` (MASTER pot) | `st_alloc.c` (voice allocation, mono/legato, glide, the unison budget and CPU guard) |
| `main.c` (boot, guards, the boot-loop guard, UBOOT entries, the main loop), `audio.c` (the ISR, the shed) | `st_fx.c` (drive, chorus, delay, reverb, OTT, limiter), `st_midi.c` (notes, CC map, PC, clock, learn) |
| the menu framework (`ui_menu.c`), help hints (`ui_help.c`), MIDI learn (`midi_learn.c`), the input gestures of `ui_input.c` / `ui_layer.c` (tap, hold, double-tap lock, SAVE held = undo, LFO held = MOMENTARY) | `st_store.c` (user presets, autosave, settings on `storage.c`), `st_ui*.c` (pages, knob cards, graphs, layers) |
| `tests/hostsim.c` (the simulated HAL), the storage, OTA, loader, USB and MIDI tests, `web/emu` | `tests/st_*` (the DSP harness and unit tests), `tools/gen_wavetables.py`, `tools/gen_presets.py` |

Removed: the engines and the Felucca-Engines submodule, the sequencer, song, projects, motion, slicer, chord keys,
the performance FX layer, the SAMPLE/GRAIN sample sets (the stretch tier may bring GRAIN back), the web editor protocol
(Strata's own editor is stretch). Copyright headers of moved code stay as they are; new files carry Strata's.

The DSP files (`st_wt`, `st_wtgen`, `st_osc`, `st_voice`, `st_env`, `st_lfo`, `st_mod`, `st_alloc`, `st_fx`, `st_param`,
`st_patch`) use no HAL and no globals of the UI. The same sources build four ways: the FM-1 image (JieLi clang), the
host harness (gcc, `tests/`), the browser emulator (emcc, `web/emu`) and the workbench (emcc, `web/strata`).

## 3. Real-time model

```
TIMER5 IRQ (prio 4, 10 kHz)   key/encoder/LED scan, ms clock, USB and TRS polls (2 kHz)
ALNK0 IRQ  (prio 3, 345 Hz)   one half = 4 blocks of 32 samples:
                               per block: panel keys + MIDI in -> events -> voices
                                          control rate: envelopes, LFOs, matrix, parameters
                                          audio rate: voices -> mix -> FX -> master -> Q15 stereo
                               end of half: load meter, CPU guard
main loop  (~60 frames/s)     UI input and drawing, wavetable builds, storage, USB SysEx, console
```

- **Events** reach the voices only at block starts (0.73 ms granularity), as in Felucca. Panel keys and MIDI share one
  path: two rings with one producer each (the panel's from the main loop, MIDI's), which the audio interrupt drains
  before each block (`sy_app.c`).
- **Main loop to ISR.** The UI never touches voice state. It writes the patch's parameter values (16-bit stores, atomic on
  this core) and posts commands (note panic, patch swap, table swap) into a small single-producer ring the ISR drains at
  a block start. A parameter written mid-block is picked up at the next block and smoothed (§12).
- **ISR to main loop.** Meters, the scope, voice states for the LEDs and the graphs: plain reads of ISR-written words.
- **Wavetable builds** run in the main loop (§6.5), never in the ISR. The ISR sees a table only through its descriptor;
  a table being rebuilt is marked busy, and an oscillator reading a busy table fades to silence first.
- **Flash writes** stop interrupts for up to ~45 ms (a sector erase). Strata writes only when nothing sounds (autosave,
  settings) or on an explicit SAVE, and fades the output to silence first, so the stalled DMA repeats silence.

## 4. Signal flow

```
                 per voice (8), stereo                                                      global, stereo
 OSC A ─ unison 1..16 (phase, detune, pan, pos) ─┐
 OSC B ─ unison 1..16 ───────────────────────────┤─(route: FILTER / DIRECT)─┐
 SUB ────────────────────────────────────────────┤                          │
 NOISE ──────────────────────────────────────────┘                          │
          │ FILTER (stereo, per voice) ─── AMP (ENV 1 × velocity) ── PAN ───┤
          └─ DIRECT sources skip the filter, still through AMP ─────────────┘
                                                                            ▼
   voice sum ─► DRIVE ─► CHORUS ─► DELAY ─► REVERB ─► OTT ─► LIMITER ─► MASTER (pot) ─► Q15 out
                                                                                        ├─► DAC (24-bit, −6 dBFS ceiling)
                                                                                        └─► USB audio input
```

Each oscillator, SUB and NOISE has a FILTER/DIRECT switch (Serum's per-source routing; Serum's mixer buses are left out).
The FX rack's order is fixed in the MVP; each module has an on switch and a mix.

## 5. Number formats

| Stage | Format | Why |
| --- | --- | --- |
| Wavetable data | int16 | half the RAM of float; the integer pipeline has the fast 32 × 32 multiply |
| Oscillator phase | uint32 (wraps once per cycle) | exact, cheap; the top bits index the mip, the next 15 bits interpolate |
| Oscillator inner loop | integer, Q15 gains, accumulates into int32 | the most repeated kernel (voices × 2 × unison): integer arithmetic has a measured cost on the device |
| Voice bus after the oscillators, filter, envelopes, FX | float (−1..1 nominal) | filters and feedback FX stay stable and predictable at any resonance; no scaling bugs; the FPU is shipping in four FM-1 firmwares |
| Mix to `audio.c` | int32 Q15 per channel, interleaved | Felucca's interface (`OUT_SHIFT` makes the 24-bit word with a −6 dBFS ceiling) |

If the device benchmark shows the FPU much slower than ~150 instructions/µs, the filter and the delay/reverb lines
move to fixed point (the kernels are isolated, the harness compares the two bit-exactly enough to keep the goldens).

**Denormals and DC.** The pi32v2 FPU's denormal behaviour is unknown, and x86 builds of the harness slow down on them.
Every recursive float state (filter integrators, one-poles, delay/reverb feedback, envelope followers) is flushed to
exact zero below 1e-15 at the block end (a compare per state per block, not per sample). Wavetables have no DC (bin 0 is
zeroed at build time); 5 Hz DC blockers sit before and after the effects (the drive's asymmetric curves make DC);
sources with an offset take it out themselves (the pulse sub, PWM's hold, the BEND / ASYM / MIRROR / SYNC warps'
mean); the harness fails on any NaN, Inf or denormal in any state.

## 6. Wavetables

### 6.1 Format

A **frame** is one single-cycle wave, stored as nine band-limited **mip levels**:

| Level | Samples | Harmonics kept | Oversampling | Used for fundamentals up to ≈ |
| --- | --- | --- | --- | --- |
| 0 | 1,024 | 256 | 4 × | 82 Hz (E2); lower notes keep 256 harmonics |
| 1 | 512 | 128 | 4 × | 164 Hz |
| 2 | 512 | 64 | 8 × | 328 Hz |
| 3 | 256 | 32 | 8 × | 656 Hz |
| 4 | 256 | 16 | 16 × | 1.31 kHz |
| 5 | 128 | 8 | 16 × | 2.63 kHz |
| 6 | 128 | 4 | 32 × | 5.25 kHz |
| 7 | 64 | 2 | 32 × | 10.5 kHz |
| 8 | 32 | 1 (sine) | 32 × | above |

(Phase 2: the plan's 4 × throughout left the interpolation's images too strong in the upper levels, so they are
oversampled more; level 1 at 8 × would cost another 34 KiB, TESTING.md §3.) Each level carries one guard sample (a copy
of sample 0, so the interpolation never masks) and is int16 normalised over the whole table (§6.5). A frame is 2,921
samples, 5,842 B. A **table** is up to 16 frames (keyframes); the oscillator's position moves continuously between
adjacent frames (crossfade, §7.2), which is Serum's smooth interpolation, so 16 keyframes give a continuous sweep.

The pool holds 32 frames (186,944 B, 182.6 KiB), 16 for each of OSC A and OSC B. Frame k + 1 always lies a frame's
length after frame k, which the oscillator's inner loop relies on (§7.2).

### 6.2 Level selection

Per voice and oscillator, once per block, from the highest fundamental the block will play:
`f_eff = f0 × (largest unison detune ratio) × (warp's harmonic stretch)`, the level is the smallest one whose top harmonic
stays at or below 21 kHz. Brightness then steps by at most the top octave when a note crosses a level boundary
(~10–20 kHz content). Built (Phase 2): a level changes only once the note has moved a semitone past the boundary
(hysteresis: a vibrato across it does not flap), and the change is a crossfade of the two levels over one block, at
double cost for that block only.

### 6.3 Interpolation and aliasing

Linear interpolation inside a frame and a 4× oversampled table leave interpolation images of the top octave of
harmonics at about −19 dB under those harmonics (−34 dB for the octave below), which fold back when the note is high in
its level's range. For tables whose energy is low up there (saws, squares, vocal, most morphs) this lands below −60 dBc;
for flat, noisy spectra it does not. The harness sweeps every factory table over the keyboard and measures the
inharmonic energy (§18); a table that fails gets its top octave of harmonics tapered at build time. Cubic interpolation
(+50 % per instance) is the fallback if tapering is not enough, applied per table, not globally.

Measured (Phase 2, TESTING.md §2.2): smooth tables −41 dBc at worst (saws −48), bright tables −27.7 dBc at worst with
their tops tapered; the warps alias much more at high amounts above C5 (PWM 100 %: −6 dBc at C7). The −60 / −40 dBc
targets are not met; TESTING.md §3 has the remedies.

### 6.4 Factory tables

Stored frames do not fit (one 16-frame table is 66 KiB of mips, 32 KiB at level 0 alone), so the factory set is
**described**, and built at load time by an on-device generator:

| Kind | What is stored | Families |
| --- | --- | --- |
| Recipe | family id, frame count, ~6 parameters with their sweep across the frames (≈ 16 B) | basic analog morph (sine→tri→saw→square→pulse), PWM, hard sync (saw/square/sine, ratio sweep), FM (ratio, index sweep), additive (tilt, odd/even, comb notches), formant/vowel (A-E-I-O-U paths, formant shift), wavefold (sine/triangle folds), phase distortion (CZ-style windows), digital (quantise, bit-reduce, step), harmonic bands, chaos (seeded spectra) |
| Spectral keyframes | 2–8 keyframes of up to 256 harmonics (log-magnitude byte + phase byte each), interpolated into 16 frames by magnitude/phase morph or crossfade | the tables that are not formulas: designed vocal and choir shapes, resynthesised textures |

`tools/gen_wavetables.py` holds the definitions. It writes the C table of recipes and keyframes (`build/gen`), renders
every table to PNG and WAV for documentation (`docs/WAVETABLES.md`), and writes reference frames the C generator is
tested against (§18). Every table is original: generated from formulas and hand-placed keyframes, no third-party data.
Built: 54 factory tables, 12 KiB of keyframe data.

### 6.5 Building a table

The generator renders each keyframe at 2,048 points (recipes in the time domain, spectral keyframes by inverse FFT),
takes a real FFT, keeps harmonics 1..256 (DC removed), and for each level zeroes the harmonics above its limit and
inverse-transforms into its slot. The whole table is then scaled so that its largest sample over all frames and levels
(band-limiting overshoots) is 0.95 × full scale, so levels and frames match in loudness. 2,048-point float FFT scratch: 16
KiB of pool. Estimated 1 ms a frame on the device (**needs hardware test**): ~16 ms for a full table.

Swap protocol (main loop): mark the oscillator's table busy, wait for the ISR to report it silent (its oscillators fade
over one block), build, publish the descriptor, clear busy (the oscillators fade back in over one block). Changing
OSC A's table while holding notes therefore gives a short gap of the table's build time (~20 ms) instead of a click. A
preset change fades every voice out (10 ms), builds what changed, and starts clean (§16).

## 7. Oscillators

### 7.1 Parameters (each of OSC A and OSC B)

TABLE, POSITION, WARP MODE, WARP AMOUNT, OCTAVE (−4..+4), SEMI (±12), FINE (±100 cents), PHASE, RANDOM PHASE, PHASE
MEMORY (all voices reset / free-running per voice / continuous), LEVEL, PAN, UNISON (1..16), DETUNE, BLEND, WIDTH,
UNISON MODE (linear, super), UNISON POSITION SPREAD, ROUTE (filter / direct), ON.

### 7.2 Kernel

Per unison voice per sample: advance the phase; take the level's two adjacent samples in frame `k` and in frame `k + 1`;
mix the frames by the position's fraction, interpolate by the phase's fraction; accumulate into the voice's L and R with
the unison voice's Q15 pan gains. Increments, frame pointers, crossfade and gains are set per block; the increment and
the crossfade ramp linearly across the block, so pitch and position modulation are smooth. Each warp mode has its own
loop (no per-sample switch). Phase 0 cost: 37 listing instructions per instance as compiled C; the target after tuning
for this core was ≤ 28. Built (Phase 2): the kernels are integer, write left and right interleaved through one
pointer, read frame k + 1 at a fixed offset from frame k, and come in four forms: gains held (the usual block) or
ramping, two frames crossfaded or one (the position on a frame: either end, a keyframe, a table of one frame, which a
crossfade of exactly 0 or 1 gives bit for bit): 35 instructions a sample plain (21 on a frame), 46 through a warp map
(31), 62 for SYNC (45), 40 for FM (24) (TESTING.md §4.1). An oscillator adds into its route's integer bus with the
source's level, pan and fades in its unison voices' gains; the bus becomes float once.

### 7.3 Warp modes

MVP (6), all phase-domain or a single extra lookup:

| Mode | Operation on phase `p` (0..1) or output | Aliasing control |
| --- | --- | --- |
| SYNC | read at `frac(p × r)`, r = 1..16 from AMOUNT; a raised-cosine window over the cycle fades the reset (Serum's soft-to-hard sync is the window's width) | mip level chosen for `f0 × r` |
| BEND ± | `p' = p^g` mirrored about the centre, g from AMOUNT (pinch in / out) | level for `f0 × (max slope)` |
| PWM | `p' = p / w` while `p < w`, then hold the last sample (0 = silence for square-like tables) | level for `f0 / w` |
| ASYM ± | `p' = p^g` over the whole cycle | level for `f0 × (max slope)` |
| MIRROR | first half plays the wave at double speed, second half mirrored; AMOUNT moves the fold point | level for `2 × f0` |
| FM (from the other oscillator) | phase offset `p + depth × B(t)`, B being the other oscillator's centre unison voice (one modulator per oscillator: the per-unison-voice modulator of the plan costs a second oscillator per instance) | depth held under 21 kHz as the others; level for `f0 × (1 + depth × fB / f0)` |

v1 adds: FLIP, REMAP (curve), QUANTISE, ODD/EVEN, FM from SUB/NOISE, AM, RM, thru-zero FM, PD from other oscillators.

Built (Phase 2): the warps of the phase (BEND, ASYM, MIRROR, PWM, SYNC) go through a 65-point map of the phase,
rebuilt when the warp or its amount changes; each warp's AMOUNT is capped so its steepest slope keeps the top harmonic
under 21 kHz at the note playing.

### 7.4 Unison

N voices per oscillator (1..16), detune spread across `[−DETUNE, +DETUNE]` cents: **linear** (even) or **super**
(denser near the centre, a supersaw curve). BLEND sets the side voices' gain against the centre voice (one with an odd
N, two with an even N), with equal-power compensation so the loudness stays the same as N changes. WIDTH spreads the
voices' pans (alternating left/right from the centre out). POSITION SPREAD offsets each voice's wavetable position.
RANDOM PHASE draws each voice's start phase per note (no "laser zap" unless asked for).

Detune ratios depend only on the patch, so they are computed when DETUNE, N or the mode change, not per voice. A
voice multiplies its centre increment by them each block. Per-voice detune modulation recomputes them per voice and
block (exp2 approximation).

## 8. Sub and noise

**SUB**: sine (table), triangle, saw, square, pulse (polyBLEP; width from a parameter), rounded rectangle (a square
softened by a one-pole); OCTAVE −3..0 relative to OSC A's pitch (Serum's sub follows the note), LEVEL, PAN, ROUTE.
**NOISE**: white (xorshift), pink (three-pole filter), brown (leaky integrator); COLOUR (a tilt filter), STEREO
(decorrelated left/right), LEVEL, PAN, ROUTE. Samples as noise sources (Serum's noise oscillator) are stretch.

## 9. Filter

One filter per voice, stereo (the unison spread is stereo), between the routed sources and the amp.

| Type | Topology | Why |
| --- | --- | --- |
| LP12, HP12, BP12, NOTCH, PEAK | trapezoidal (TPT) state-variable filter, Simper's form | zero-delay feedback: tuning stays correct up to Nyquist, stable under any modulation speed, never blows up as resonance rises; one structure gives every response |
| LP24, HP24, BP24 | two TPT SVFs in series, resonance on the second | the 24 dB slopes at twice the cost |
| LADDER | four one-pole TPT stages in a loop with a soft saturator (`x − 4x³/27`, flat beyond ±1.5) in the feedback, resonance compensated for passband loss | the "MG" character for basses and leads; the saturator bounds the loop at any resonance |
| FORMANT | three parallel TPT band-passes tuned to vowel formants; CUTOFF morphs through the vowels A E I O U, VAR shifts them | vocal and talking basses |
| COMB ± (v1) | short feedback delay per voice | needs 16 KiB of delay lines at 8 voices; comes with the second filter in v1 if RAM allows |

Controls: CUTOFF (20 Hz–20 kHz, exponential), RESONANCE (to self-oscillation on the SVF types, bounded on all),
DRIVE (input gain into a soft clip, level-compensated), KEY TRACK (0–100 %, about C3), VAR (built: the stereo spread
of the cutoff, left down and right up by up to an octave each; on FORMANT, the formants' shift), MIX (dry/wet), and
per-source routing (§4).

Coefficients come from the cutoff once per block (a `tan` approximation, prewarped) and step linearly across the
block. The SVF's three coefficients are each bounded, so a ramp between any two filters stays stable even with the
cutoff jumping 20 Hz ↔ 20 kHz in a block (tested); each response is a loop of its own. The cutoff is clamped below
0.47 fs after modulation. The harness drives every type with impulses, noise and square waves at every
cutoff/resonance corner, sweeping cutoff at audio-rate depth, and fails on any growth (§18).

## 10. Envelopes and LFOs

**ENV 1** (amp) and **ENV 2** (MVP; ENV 3–4 in v1): ATTACK, HOLD, DECAY, SUSTAIN, RELEASE (1 ms – 30 s, exponential
knob), with ATTACK, DECAY and RELEASE CURVE (−100 % concave … +100 % convex, as Serum's curve handles), and a
LEGATO-INVERT flag. Evaluated per voice per block: the segment's linear position is shaped by the curve and the
amp envelope's value ramps linearly across the block. Envelopes retrigger from their current level, never from zero, so
voice reuse and retriggers do not click.

**LFO 1, LFO 2** (MVP; 3–4 in v1): SHAPE (sine, triangle, saw up, saw down, square, sample & hold, smooth random,
step 4, stairs, pulse, bounce, trapezoid, and DRAWN), RATE (0.01–40 Hz, or synced 8 bars … 1/32 with dotted and
triplet), MODE (free: one phase for all voices, aligned to the MIDI clock when it runs; retrigger: per voice, from
PHASE at the note; envelope: one cycle, then hold), DELAY and RISE (fade-in), PHASE, SMOOTH, and POLY / MONO.
Evaluated per block, linear between blocks.

**Drawn shapes** (Phase 4, at the user's request): each LFO's shape as up to 16 points over a cycle (the first at its
start, the last at its end, two at one place a step), with a curve between each two: the envelopes' `u / (u + (1 − u)
2^(4c))`, so a curve split in two at a new point stays the same shape (`k` becomes `k / d` and `d`, `d = u + k (1 −
u)`). The patch keeps both LFOs' points (`sp_draw_t`); every built-in shape has its points (`SP_SHAPE_PTS`), where
a drawing starts: exact but for SINE and BOUNCE, whose quarter sines no one curve follows closer than 2.6 % of the
range (5.3 % on BOUNCE's first bounce). The audio side takes the points as floats at a block's start when they changed
(`lfo_draw_sync`), never half written: the main loop writes them between `sy_draw_edit` and `sy_draw_done`, a sequence
count odd while an edit is under way, and a compiler barrier each side; a patch load takes its own. Edited on the
LFO's POINTS page (§17).

## 11. Modulation

### 11.1 Sources

Per voice: ENV 1, ENV 2, LFO 1, LFO 2 (POLY), VELOCITY, NOTE (key position, ±1 over the keyboard), NOTE-ON RANDOM,
NOTE-ON ALTERNATE (0/1), voice index. Global: MACRO 1–4, MOD WHEEL (CC1), PITCH BEND, CHANNEL AFTERTOUCH (MIDI only:
the keys have none), EXPRESSION (CC11), LFOs in MONO mode, a FIXED value. Unipolar sources span 0..1, bipolar −1..1.

### 11.2 Matrix

Eight slots (sixteen in v1): SOURCE, DESTINATION, AMOUNT (−100..+100 %); v1 adds AUX SOURCE (multiplies), CURVE and
OUTPUT scaling, as Serum's matrix row. A destination is any parameter flagged modulatable (oscillator position, warp,
level, pan, pitch, detune, blend; sub and noise levels; filter cutoff, resonance, drive, var, mix; envelope times and
sustain; LFO rates; FX amounts and mixes; macros themselves). An amount of 100 % moves the destination across its
whole range. The four macros are global sources that can also be destinations.

Evaluation, once per block: per voice, the slots whose destination is per-voice add `source × amount × range` into
that voice's offsets; slots with a global destination (effects, macros) take their per-voice sources from the newest
voice (Serum's "the effects are monophonic"). Destinations are then clamped to their ranges. Cost: ~20 instructions per
slot per voice per block, under 1 % of the CPU at 8 voices.

### 11.3 Assignment on the panel

As Serum's drag of a source onto a knob: **hold the source, turn the knob, the turn sets the amount**. The knob's card
then shows the modulation range as an arc in the source's colour. Details in §17.

## 12. Parameters

One table (`st_param.c`, an X-macro) defines every patch parameter: a stable numeric id (append-only), a short name, a
section, its range and default as a 16-bit integer, the display unit and curve (linear, exponential, note values, list),
whether it is modulatable, per voice or global, its smoothing (none, block, or per-sample ramp), and its MIDI CC (if any).
The UI draws knob cards from the table, the matrix offers its destinations from it, storage writes presets in its id
order, MIDI maps CCs through it, and the help hints are keyed by id.

Changes from the knobs or from MIDI are written as targets; the ISR moves the working value towards the target with a
one-pole smoother per block (time constant per parameter, 5–20 ms), and gains, pans, cutoff and position additionally
ramp linearly inside the block. A knob turned fast or a CC jump therefore never steps the audio (§18 tests it).

## 13. Voices and the CPU guard

- **8 voices**, each with up to 2 × 16 unison phases. POLY (1..8) limits a patch.
- **Modes**: POLY, MONO (last note priority, retrigger), LEGATO (no retrigger while a note is held; envelopes marked
  LEGATO-INVERT retrigger anyway).
- **Glide**: TIME, CURVE (exponential/linear), ALWAYS or LEGATO only, SCALED (shorter for small intervals, as Serum).
- **Pitch bend**: RANGE UP and DOWN (0..24 semitones) per patch; the panel bend is a knob that springs back (§17).
- **Sustain** (CC64) holds releases.
- **Stealing** (a new note with every voice in use): first a voice in release, the quietest; then the oldest held note
  that is not the newest or the lowest; never the note just played. The stolen voice fades out over 4 blocks (2.9 ms)
  before it is reused, so steals do not click (the harness measures the step, §18). Built (Phase 2): every switch
  fades the same way (a source on or off, a warp, shape, type or route change, the filter on, off or its type, a table
  swap, a patch change): out over 4 blocks, the change made while silent, back in over 4.
- **Unison budget**: the total of sounding oscillator instances (voices × oscillators × unison) is limited (default
  48 on one core; set by the guard). When notes would exceed it, the oldest voices step down in unison count (their
  outer voices fade out over a block) before any voice is stolen. New notes get the patch's full unison while the budget
  allows.
- **CPU guard** (end of every half, from the measured render time, smoothed): above 70 % the unison budget shrinks by
  one oldest-voice step per half; above 78 % the costly FX quality steps down (OTT upward compression off, reverb
  density halved); two halves above 85 % shed a voice, as Felucca's `shed_voice` does, faded. Below 55 % for a second,
  the steps undo in reverse order. The load, the budget and the shed count are on the INFO page and the console.
- **Cost** (Phase 2, after its optimisation pass, TESTING.md §4): the target cost model puts a voice of A + B with
  LP12 at ~276 listing instructions a sample, each unison instance at ~34 (the position on a frame; ~48 moving), every
  effect together at ~1,100 and the engine's overhead at ~180. At the 205 instructions a µs measured on integer code,
  one core holds about 10 such voices without effects and about 6–7 with every effect, 7 and 4–5 at 3 unison each:
  eight voices fit; unison with the whole rack does not, and the guard trims. The budget's default (48 instances) is set
  from the BENCH page's real rate at integration; the second core (§19) doubles the room.

## 14. Effects

Global, stereo, float, fixed order, each with ON and MIX:

| Module | Algorithm | Notes |
| --- | --- | --- |
| DRIVE | soft clip, tube-like asymmetric, hard clip, sine fold; pre-gain, tone (tilt), level compensation | no oversampling (cost) |
| CHORUS | two modulated taps per side on one 4 KiB int16 line (23 ms a side), delay 2–15 ms, depth up to 3 ms, linear interpolation | RATE (Hz/sync), DEPTH, DELAY, FEEDBACK, tone |
| DELAY | stereo or ping-pong, two int16 lines stored at half rate (filtered), up to 750 ms a side (so the pool fits, §20) | TIME (10–750 ms or synced, dotted/triplet; a synced time longer than the line stops at it), FEEDBACK, LP/HP in the loop, OFFSET L/R |
| REVERB | PLATE and HALL: a 4- or 8-line feedback delay network at half rate (lines of 39–79 ms) with damping and slow modulation, pre-delay up to 50 ms | SIZE, DECAY, DAMP, PRE-DELAY, WIDTH, LOW CUT |
| OTT | three bands split by complementary SVF crossovers (they sum back exactly), per band an envelope follower and up- and downward compression | DEPTH, TIME, UPWARD, DOWNWARD, band gains L/M/H |
| LIMITER | look-ahead-free peak limiter with a soft knee (above 0.75) before the master | catches resonant peaks and OTT gain |

Half-rate processing of the delay and reverb halves their memory and cost; their high end is filtered below 10 kHz
anyway. All delay times and LFO rates sync to the internal tempo or the MIDI clock.

## 15. MIDI

- **In**: USB-MIDI and TRS (Felucca's drivers), one channel (or omni), notes, velocity, pitch bend, channel
  aftertouch, CC1 mod wheel, CC11 expression, CC64 sustain, CC120/123 panic, program change and bank select (bank 0
  factory, bank 1 user), clock (tempo for synced rates), start/stop (resets synced LFOs and delays), and a default CC
  map for the macros and the main parameters (CONTROLS.md lists it). Felucca's MIDI LEARN assigns any CC to the
  parameter of any knob.
- **Out**: USB-MIDI: panel keys (with the panel velocity), and optionally the knobs' CCs. **TRS MIDI out is not
  possible on a stock FM-1**: its only MIDI jack is an input (an optocoupler at its pads; no TX pin is known on the
  board; confirmed by PCB photos and by three firmware projects). Options: a USB-MIDI host adapter turns the FM-1's USB
  MIDI out into DIN/TRS without a computer; a hardware modification (a free GPIO with UART TX, two resistors and a jack)
  would need a pin found on the board first, and is out of scope until one is. Decided after Phase 1: TRS MIDI out is
  dropped; MIDI out is USB only.

## 16. Presets and storage

- **Patch**: the parameter values in id order with a count (new parameters take their defaults when older presets
  load), the matrix slots, the two table references (factory id or user table), the macro names, the preset name (16
  characters) and category (bass, lead, pad, pluck, keys, FX, sequence/arp, other), then the LFOs' drawn shapes (`'D'`,
  each LFO's count of points and its points; a patch without them takes the default's). At most 582 bytes.
- **Factory presets** (64 at first, up to 128) are constants in the image, written by `tools/gen_presets.py` from
  readable definitions.
- **User presets**: 128 slots in the user store as A/B sector objects (`storage.c`: commit record last, newest valid copy
  wins, a torn write leaves the old copy). Six patches per object, 22 objects, 176 KiB of `0x97000–0xDFFFF`.
- **Edit buffer and restore on boot**: the sound being played is autosaved (A/B, `0xE5000/0xE6000`, as Felucca's
  autosave) once nothing has sounded for 3 s after a change, and restored at power-on unless the last boot crashed.
- **Undo**: a ring of 8 patch snapshots in RAM; a snapshot is taken when an edit gesture starts (a knob untouched for
  1 s before) and on every load. SAVE held steps back.
- **Settings** (MIDI channel, panel velocity, palette, LED mode, HOLD time, help on/off …) in Felucca's settings pair
  `0xFC000/0xFD000`.
- **Flash safety**: only `storage.c` writes, through `FL_RANGE_OK` (the runtime guard in the RAM-resident driver, kept).
  Every object's sectors are compile-time constants checked against the allow-list with `_Static_assert`. The update
  loader and the head are untouched. Felucca's flash map, package format and identity (`FM-1_900`, release `FM-1_9XY`)
  stay, so M-UPGRADE and **Return to official V15** keep working.

## 17. User interface (draft; CONTROLS.md is the reference once the UI phase is done)

**Phase 4, at the look stop** (verified in emulator; the design: the Strata Screen Design canvas, approved with its
seven palettes and STRATA's amber as the default). The screen is five regions, each drawn only when it changes and sent
on its own (one canvas strip at a time): a header (the section, its power light, the page and its number, at the right
the oscillator's OCT SEM FIN, an envelope's or LFO's tabs, the effects that are on; in the MOD layer a banner with
the source and the depth just set), the display window and its graph, four knob cells (a dial: the value's arc, the
modulation's halos, the pointer; or a list's pill), and a footer of the mod sources' chips (lit: moving something on
this page) or a word for a moment. Colour is a role (sound, now, value, modulation and its negative and other
sources, ...), each palette thirteen base colours and the rest derived; MENU > COLOR picks one. Graphs so far: the
table in 3D (its frames as the oscillator stores them, the playing one on top, the range modulation sweeps), one
frame in 2D (warped by the oscillator's own warp function, the unwarped one behind) with the unison's voices, an
envelope (the other one behind, each voice on it, a time ruler), the palettes, and a list of the section's values for
the pages whose graph comes later. Knobs are speed-sensitive (a slow turn 0.1 % a detent, a quick one up to 5 %;
MENU > ACCEL OFF: 1 %); a list, a count or a short range moves one value a detent. The MOD layer: REC held (double
tap: latched); a white key picks the source (the keys light: the pick, the sources in use), a knob sets its depth on
that knob's parameter (a matrix slot made when needed, freed at 0 when the layer closes); ENV held: the same for
ENV 2, the keys still playing. HOME held: the MENU. Two new font faces (10 px caps, 12 px semibold) and Z, _ and ±
cost about 33 KB of flash.

**After the look stop** (the user's changes, verified in emulator): OCT− / OCT+ with a family button (or REC) held
step the pages of the section shown, round (OSC A's WAVE, UNISON, SPREAD, PITCH, OUTPUT); the octave stays and the
held button's release and hold do nothing. The WAVE page's knobs are WT POS, TABLE, WARP, AMOUNT. Each LFO has three
pages, each with its graph (a cycle of the shape as the audio side plays it: a random shape as five cycles of sample
levels; where each voice is in its cycle, live while notes sound; the shape, the rate, the mode): POINTS (KNOB 1
picks a point, round; KNOB 2 moves it along the cycle between its neighbours, the first and last staying at the
ends; KNOB 3 up and down; KNOB 4 the curve of the segment from it, the last point's into it; the graph marks the
points, the one picked and that segment), SHAPE (SHAPE, SYNC, RATE, MODE) and TIME (DELAY, RISE, PHASE, SMOOTH). A
shape other than DRAWN shows its own points; the first edit draws from them ("DRAWN FROM SINE"). LFO tapped twice on
the POINTS page adds a point left of the one picked (the first point picked: right of it), halfway to the point
before, on the curve, and picks it (16 points at most; none where two points share a place).

Still to come in Phase 4: the LFO, FX, GLO and SCL layers, the filter, matrix, rack and HOME graphs, presets
(browser, user slots, save, undo, restore), MIDI learn, help, the rest of the MENU, and CONTROLS.md. The draft below
is the plan those follow.

**Screen layout** (Felucca's): a header with the page and the preset; four knob cards for KNOB 1–4, each with its value,
a ring, and the modulation range as a coloured arc; the graph area (the oscillator's frame with the position's neighbour
ghosted, the filter's response curve with the cutoff and resonance, the envelope's shape with a moving dot per voice, the
LFO's shape with its phase, the matrix as a list); a footer with key hints. Everything is drawn lazily: only what
changed, in strips, so the 12 MHz display keeps up.

**Buttons** (tap acts on release and cycles a family's pages; hold opens the family's quick layer; double tap locks it):

| Button | Tap: pages | Hold |
| --- | --- | --- |
| HOME | HOME (preset, macros on KNOB 1–4), MACROS | the MENU |
| EDIT | the sound sources; ALGORITHM picks OSC A, OSC B, SUB, NOISE, FILTER; taps cycle that section's pages | quick jump: a key per section |
| ENV | ENV 1, ENV 1 +, ENV 2, ENV 2 + | **assign ENV 2**: turn a knob to set its amount |
| LFO | LFO 1 POINTS, SHAPE, TIME; LFO 2 the same (built); twice on POINTS: a point (built) | **MOMENTARY** (Felucca): a knob's change lasts while LFO is held |
| SEQ | MATRIX (KNOB 1 slot, 2 source, 3 destination, 4 amount) | — |
| REC | — | **MOD layer**: the white keys are the sources (lit: in use on this page), pick one, turn a knob: its amount |
| FX | DRIVE, CHORUS, DELAY, DELAY +, REVERB, REVERB +, OTT, OTT + | **macros**: KNOB 1–4 = MACRO 1–4, keys toggle the modules |
| GLO | VOICE (mode, poly, glide, bend range), GLOBAL (tune, tempo, panel velocity) | **performance**: KNOB 1 pitch bend (springs back), 2 mod wheel, 3 glide, 4 volume |
| SCL | KEYS (scale and root shown on the LEDs, octave, panel velocity) | the root by key, as Felucca |
| SAVE | SAVE (a user slot), PRESETS (browse by category) | UNDO |
| ARP | (v1: the arpeggiator) | — |
| PLAY | starts and stops the internal clock (synced LFOs and delays; the arp in v1) | — |
| OCT− / OCT+ | octave; with a family button or REC held, the section's pages (built); in dialogs back / confirm | both 5 s: UBOOT (Felucca) |

PRESETS turns through presets from every page that is not a list. SELECT steps the pages (or, in the MENU, the tempo or
mod wheel, as Felucca's SELECT option).

**LEDs**: the keys show the scale (root lit, scale notes glowing), the notes sounding, and in the MOD layer the sources
in use; buttons show the open page's family, a locked layer breathes.

**Help**: every page has a one-line hint shown for ~2 s on entry (Felucca's HELP setting).

## 18. Testing

The host harness (`tests/sy_*`, gcc, no HAL) renders audio offline through the real DSP sources. What it checks and
what it found: [TESTING.md](TESTING.md). The plan:

| Test | Pass condition |
| --- | --- |
| Aliasing | every factory table swept C0–C8 with and without warps; inharmonic energy (FFT, harmonics masked) under the threshold for its class (saw-like −60 dBc, bright −40 dBc) |
| Clicks | note on/off, voice steal, retrigger, table swap, preset change, fast knob turns and CC jumps: the largest sample-to-sample step stays under 2× the steady signal's |
| Numerical health | no NaN, Inf, denormal state; no DC above −60 dBFS at the output |
| Filter stability | every type at cutoff/resonance/drive corners, audio-rate cutoff modulation, impulses and full-scale squares: bounded output, decays after input stops |
| Determinism | the same events give the same output hash run to run and between the host, the emulator and (for the shared parts) a cc build |
| Golden renders | every factory preset plays a fixed phrase; its hash and levels are recorded; a change must be intended (`GOLDEN_UPDATE=1`) |
| Cost | per-kernel listing counts from the target build (`tools/costprobe/sy_cost.sh`), and the whole engine's on the target by the cost model (`sy_dyn.sh`: the listing weighed by the host's line counts) |
| Units | parameter table, matrix, allocation and stealing, glide, storage round trip and migration, flash allow-list |

The emulator (`web/emu`: Felucca's page and worklet, with Strata's glue `strata_web.c`) runs the app on a simulated
FM-1 in a browser; `strata_emu_test.mjs` drives it under Node, `strata_shot.mjs` writes its screen as PNG and
`strata_browser_test.mjs` runs the page in headless Chrome. The engine
also builds as WebAssembly for the host tests (`tests/sy_wasm.sh`) and for the workbench (`web/strata`, `wb_test.mjs`).
Felucca's platform tests (storage, OTA, loader, USB, MIDI parser, input) keep running.

**On the device** (needs your go-ahead, after a full backup): a BENCH page times each kernel with TIMER4 and reports
the clock and instructions/µs, replacing the Phase 0 assumptions in this document.

## 19. The second core (v1)

Voices are independent until the mix, so core 1 renders half of them into its own bus while core 0 renders the rest and
the FX, using Felucca's mailbox (`hal/fm1_cpu1.h`): hold before any reset, a timeout that falls back to one core, off
by default. The unison budget doubles when core 1 answers. ChoralRoot raises the core supply before starting core 1;
whether Strata needs that is decided by the device test.

## 20. Risks and open items

| Risk | Mitigation |
| --- | --- |
| CPU: the engine costs well over Phase 0's estimate; after the optimisation pass eight voices fit, unison with every effect does not (§13, TESTING.md §4) | the guard trims; the budget's default and the second core decided with the BENCH page's real rate |
| FPU slower than assumed | kernels isolated; fixed-point filter/FX fallback; decided by the BENCH page |
| I-cache misses (code runs from flash; 32 KB cache shared) | hot loops are small; if the BENCH page shows UI-dependent load, the oscillator and filter loops move to `.ram_text` (24 KiB) |
| Aliasing of bright tables with linear interpolation | per-table taper; cubic interpolation for the few that need it |
| Brightness steps at mip boundaries on glides | measured; crossfade the levels in boundary blocks |
| Table build gap (~20 ms) on table/preset change | faded; documented; preset changes are silent between the old sound's fade and the new one |
| RAM: the engine's pool was 214 B over Felucca's beside the screen's canvas, before the 8 KiB margin | the delay went to 750 ms a side: 295,638 B, 13,482 B free (TESTING.md §4.4); the generator's scratch (11.4 KiB) could move to `.bss` if more is needed |
| TRS MIDI out | not possible on stock hardware; dropped (§15) |
