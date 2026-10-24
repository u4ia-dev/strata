# Strata's flash and RAM (the V0.65 audit; V0.67's figures in §8, V0.8's in §11, V0.81's in §14, V0.85's in §15, V0.86's in §16 and §17, V0.87's in §18 and §19, V0.87.1's in §20)

Where Strata's flash and RAM go, what V0.65 freed and how, what the audit found on the way, and what is left. The
figures are from `tools/build.py --strata` and the listing (`build/strata.dis`, symbols from `objdump -t`) unless a
line says otherwise. Status words as in [ARCHITECTURE.md](ARCHITECTURE.md): **verified on host**, **verified in
emulator**, **needs hardware test**.

## 1. Where it stands

| | V0.6 | V0.65 | |
|---|---:|---:|---|
| App image (of the app slot's 581,564 B) | 407,008 B | 324,148 B | 257,416 B free (V0.6: 174,556) |
| `.data` + `.bss` (of 73,728 B) | 66,368 B | 65,152 B | 8,576 B free (V0.6: 7,360) |
| Pool (`.pool`) | 363,492 of 368,640 B | 358,692 of 380,928 B | 22,236 B free (V0.6: 5,148) |
| Main stack (reserved) | 24,320 B | 12,032 B | deepest path ~6.6 KB from the listing; 4,968 B used on the unit after an hour (V0.5: 17.9 KiB) |
| Interrupt stack (7,936 B reserved) | ~2.5 KB | ~2.5 KB | unchanged |
| **RAM free in all** | **12.2 KiB** | **30.1 KiB** | V0.66: **48.2 KiB** (27,168 B of `.bss`, 22,236 of the pool) |
| SRAM Strata leaves unused | (not named) | 44 KiB | painted at power-on; BENCH says whether anything wrote there |

The build keeps 4 KiB of `.bss` and 4 KiB of the pool spare (its checks fail below that); the free figures above
include those margins.

V0.66 (after V0.65's test found the spare SRAM untouched): the undo history (7,008 B) and the FX modules' short lines
(12,288 B) moved into it (`strata.ld` `.spare_bss`, cleared at boot): `.data` + `.bss` 46,560 B (65,152 in V0.65);
25,856 B of the spare SRAM left, still painted and watched; the image 327,184 B (the admission, the keys' timer path,
the event log: +3 KB).

## 2. What V0.65 changed

1. **The display's graphs through a table** (`sy_ui_draw.c` UI_GRAPH). Called from a switch, every page's graph was
   inlined into `ui_draw_disp`, and their local arrays did not share the stack: one frame of 17,336 bytes (V0.6's
   listing), under every frame the main loop draws. V0.5's BENCH measured the main stack at 17.9 KiB of 24.3. Called
   through the table, the deepest is one graph's own (the DELAY's, 4.3 KB).
2. **The power-on's code out of line** (`sy_app_boot`, SY_NOINLINE): its two patches and the settings' buffer
   (~2.6 KB) had stayed in `fm1_main`'s frame for as long as the unit ran.
3. **Strata's main-loop code built for size** (`tools/strata_drivers.py`, the size pass): the UI, the console, the
   presets' store, the app's and the boot's files (`sy_ui*.c`, `sy_text.c`, `sy_console.c`, `sy_store.c`, `sy_app.c`,
   `sy_main.c`, `sy_hw.c`) at `-Os`, as Felucca builds its own UI (`tools/size_fns.py`). What an interrupt can run
   stays at `-O2` (`size_fns.isr_reach`), and so do the DSP, the drawing's primitives (`sy_gfx.c`, `gfx.c`, `lcd.c`)
   and the wavetable generator; USB, TRS MIDI and the update entry stay at `-Os` as before. **79.6 KB** less code.
   With the pass on and off, the audio interrupt, `voice_block`, the FX filter, TIMER5 and `usb_poll` come out
   instruction for instruction the same (verified from the listings). `STRATA_SIZE=0` builds without it. The screen's
   drawing may be slower: **needs a hardware test** (BENCH's SCREEN row against V0.6's).
4. **Strata's own linker script** (`firmware/strata.ld`, Felucca's `app.ld` otherwise unchanged): the main stack 12
   KiB (Felucca's 24 KiB), its 12 KiB given to the pool. `build.py` counts the main stack from the listing as it counts
   the interrupts' and fails the build over 8 KiB (the rest for what the count cannot follow: the flash's hooks and
   the console's commands, called through a register). BENCH reads its real use on the unit.
5. **The canvas 240 × 114** (`strata.c` CV_MAX; Felucca's `gfx.c` keeps 240 × 124 for Felucca): Strata's largest strip
   is the display's 240 × 114. 4,800 B of the pool.
6. **Where a smoothed value goes, worked out again** (`sy_param.c`): `sp_t`, the normalised target of each of the 312
   values, is gone; a gliding value's target is its stored value normalised each block (the same division on the same
   operands: the golden renders are unchanged, bit for bit). 1,248 B of `.bss`. The list of gliding values holds
   their ids in 16 bits (the bug in §3) and their flags in bits.
7. **The unused SRAM named and watched** (`strata.ld` `_spare0`, `_spare1`; `sy_main.c`): painted at power-on, counted
   every half second. BENCH's STACKS row ends in `SPARE OK` (nothing wrote there) or `SPARE HIT`; the console's
   `bench` prints `spare_bytes` and `spare_changed_bytes`.

Felucca's own build is byte for byte what it was (`build/felucca.bin`, 512,144 B, compared).

## 3. Found by the audit, fixed in V0.65

- **34 knobs that did nothing while a sound played.** The list of values still gliding (`sp_mv`) held their ids in
  bytes. Since V0.6 the parameters go to 311: an id from 256 up came back as another one, and that value never moved
  again until a sound loaded. Hit: every smoothed value of the FX FILTER, BITCRUSH, TREMOLO, RING MOD and TAPE, both
  instances (CUTOFF, RESO, DRIVE, MIX; RATE, MIX; RATE, DEPTH; FREQ, SHAPE, SPREAD, MIX; WOW, FLUTTER, SATURATE,
  TONE, MIX). On the unit, V0.6's test item 12 would have shown it. A new host check turns all 166 smoothed values
  while a sound plays and asks each to arrive: it fails on V0.6's code (34 values short) and passes now (**verified on
  host**, TESTING.md §2.4).
- **RING MOD's page name ran on.** A page's name was `char[8]`; "RING MOD" is 8 characters, so C dropped its end
  without a word and the header read on into the knobs' ids (garbage after the name, on both RING MOD pages). The field
  is 9 now; an emulator check asks every page's name to end within 8 (**verified in emulator**).
- **The build's stack count could miss a frame.** A frame too big for an immediate is made in a register (`rK = -N`,
  then `sp = rK`): V0.6's 17 KB frame was one, and the count saw 52 bytes of it. Such frames are counted now, and an
  `sp` set from a register the count cannot read fails the build (`tools/build.py` stack_funcs).

## 4. The SRAM map (V0.65, `firmware/strata.ld`)

| Range | Size | What |
|---|---:|---|
| `0x01C00000` .. `0x01C00B6C` | 2,924 B | `.ram_text`: the flash driver, run from RAM |
| `0x01C00C00` .. `0x01C08000` | 29,696 B | **spare** (`_spare0`): the rest of RAMTEXT, and Felucca's room for RAM-run debug |
| `0x01C08000` .. `0x01C1A000` | 73,728 B | `.data` + `.bss`: 65,152 used |
| `0x01C1A000` .. `0x01C77000` | 380,928 B | the pool: 358,692 used |
| `0x01C77000` .. `0x01C7A000` | 12,288 B | guard band (256 B) and the main stack |
| `0x01C7A000` .. `0x01C7C000` | 8,192 B | guard band (256 B) and the interrupts' stack |
| `0x01C7C000` .. `0x01C7C080` | 128 B | `.noinit`: the crash record, the boot guard, the panel's and settings' state |
| `0x01C7C100` .. `0x01C7FD00` | 15,360 B | **spare** (`_spare1`): the rest of NOINIT (Felucca keeps a project there) |
| `0x01C7FD50` .. | 688 B | reserved: boot info, the boot-state mailbox, the vectors (write-protected) |

The biggest users: the wavetables 186,944 B (32 frames of 2,921 samples: each oscillator's 16, the worst case), the
delay 66,176 B (750 ms), the canvas 54,720 B, the reverb 23,662 B, the voices 14,208 B, the generator 14,760 B, the
FX modules' lines 12,288 B (`.bss`) and 8,192 B (the choruses), the undo history 7,008 B.

The spare areas are SRAM no Strata code, DMA or interrupt touches (the hardware's write limits cover only the two guard
bands and the reserved top; the ROM is not running while Strata does). That they stay untouched: a first **yes on
hardware** (2026-10-18: 0 of 45,056 bytes changed after 63 minutes, 26 flash erases among them, but little MIDI and no
USB audio stream); the test page's run (MIDI and the USB audio) is the next look. V0.65 paints them and BENCH watches
them. If they hold through the test, a later version can put buffers there
(the undo history, the FX lines, a groovebox's patterns) without taking from the pool.

## 5. What is left (not done in V0.65)

| What | Saves | Why not yet |
|---|---:|---|
| The 12 px regular face (`AF_S`, used in 7 places) set in the semibold or the 10 px face | ~16.7 KB flash | changes how those texts look: your call |
| Each glyph drawn at 2 sub-pixel places instead of 4 (S, M, XS, SB) | ~25 KB flash (an estimate) | text placed to the half pixel instead of the quarter: your call |
| XS's and SB's kerning pairs, the same 2,030 B twice | 2 KB flash | Felucca's font generator: a change to its output |
| The second instances' 76 rows of the parameter table, derived from the first's | 1.5 KB flash | every `SP[id]` read would go through a function |
| The undo history (8 patches, 7,008 B) into a spare area, or kept packed | 7 KB / ~1.3 KB of `.bss` | done in V0.66 (the spare area: V0.65's test found it untouched) |
| The HOME scope's copy (848 B) | 848 B | the UI would have to hold the capture until drawn |
| Felucca's USB buffers (the console's 2 KB, the audio ring's 2 KB) | — | Felucca's driver, unchanged by rule; V0.5's audio counters say not to shrink the ring |

## 6. For the groovebox

- **Flash:** 257,416 B free in the app slot, for code and built-in content (factory samples and loops compiled in, as
  Felucca's: at 4-bit ADPCM, 22 kHz mono, ~11 KB a second). User samples go in the store: 84 KiB free there
  (`0xCB000` .. `0xDFFFF`, ~7.8 s at 22 kHz ADPCM), unchanged by V0.65.
- **RAM:** 30.1 KiB free now; with the spare areas, if the hardware test shows them untouched, ~74 KiB. A sequencer's
  patterns and clips are small (a 16-step pattern of 8 tracks: well under 1 KB); a sampler's voices need little state
  and read their samples from the flash as Felucca's do. The constraint stays the CPU: the second core (§19 of
  ARCHITECTURE.md) is still off.

## 7. The UI build (`strata-ui`, V0.66 UI)

The STRATA UI ([UI.md](UI.md)) against V0.66, from the build (`tools/build.py --strata`):

| | V0.66 | V0.66 UI | |
| --- | --- | --- | --- |
| The app image | 327,184 B | 326,280 B | the fonts: Space Mono, six faces full ASCII and two sparse, 2 phases: 39.0 KB (Inter Tight's five: 76.1 KB) |
| RAM `.data` + `.bss` | 46,560 B | 47,128 B | the rows' hashes: the whole screen's 240 (the display's 114 before) |
| The pool | 358,692 B | 342,372 B | the canvas 240 × 80 (the screen in three strips), 240 × 114 before |
| The spare SRAM | 19,296 B | 21,900 B | a page's curves kept for its frame's other strips (3 × 217 floats) |

The drawing's time on the host, both builds alike (`web/emu/strata_bench.c`): at or under V0.66's in each of five
sessions of the panel (HOME in silence and with a chord, a knob turning on FILTER, ENV with a chord held, every page
shown once; the table in [UI.md](UI.md) §5). On the FM-1: BENCH's SCREEN row, **needs a hardware test**.

## 20. V0.87.1 (the hardware-test build; public in V0.875)

From the build (`tools/build.py --strata`, the log's "ok" lines), against V0.87 step 2 (§19):

| | V0.87 step 2 | V0.87.1 | |
|---|---:|---:|---|
| App image | 503,348 B | 519,048 B | +15,700; 62,516 B of the app slot's 581,564 B free |
| RAM .data + .bss | 66,980 B | 67,156 B | +176, of 73,728 |
| Pool | 362,036 B | 363,776 B | of 380,928 B; 17,152 B free with the 4 KiB margin |
| Spare SRAM | | 22,188 B used | 22,784 B left, watched on BENCH |
| Interrupt stack | ~3,068 B | ~3,064 B | of 4,096 allowed (audio 2,824 B, TIMER5 nested 184 B, wrappers 56 B) |
| Main stack | ~5,304 B | ~4,364 B | of 8,192 allowed (12,032 reserved): the path fm1_cstart > fm1_main > sy_app_ms > ui_buttons |

What it added: the four test projects' data and code (`sy_testproj.c`: compact tables built into the looper when one
loads, so no buffers of their own; their state is in the +176 of `.data` + `.bss`); the logo's drawing in place of the two blooms; DEBUG > TESTS and
the test mark; the HALL reverb's lines 5 to 8 ramping in (a 5-field struct on the stack in one out-of-line function);
the guard's and the admission's new arithmetic; the MASTER pot's dead band; backup mode's three ways; the console's
`tproj`, `uboot yes a|b|c` and three more `status` lines. `sy_app_ms` and the settings' save are out of line, as the
test projects' call had made the compiler take them into `fm1_main`, their flash buffers under every frame the loop
draws (the main stack +508 B). All figures **verified on host** (the build's counts); the unit's own use is on BENCH
(**needs hardware test**).

The guard and the admission (V0.87.1): they go by the half's whole time, the interrupts nested in it too, to higher
marks (steps at 85 %, effects at low cost at 88 %, a voice shed at 92 % twice in a row, back under 75 %; V0.87's were
70, 78, 85 and 55 % of the render alone), and a new note that does not fit first takes unison voices from the others
before it steals. Measured on the host (`web/emu/strata_clip.c`, `strata_guard.c`): **verified in emulator**; the CPU
on the FM-1 **needs hardware test** (BENCH's AUDIO row, `status`'s `load_total_pct` and `voices_traded`, test projects
1 and 3).

## 19. V0.87 step 2 (the drums' models, presets, kits)

| | V0.87 step 2 | |
|---|---:|---|
| App image | 503,348 B | +11,704 on step 1; 78,216 B of the app slot free |
| RAM .data + .bss | 66,980 B | +1,072 on step 1, of 73,728 |
| Pool, stacks | as step 1 | interrupt ~3,068 B, main ~5,304 B |

The flash: the models' code (ANALOG, FM, CHIP; NOISE is CLAP's filter without its bursts), the 64 factory presets
(32 B each, 2 KB), the six kits as preset numbers (~60 B each), the lists and NAME IT's kinds. The RAM: the user's
library's names (64 drum presets x 13 B, 16 kits x 13 B: 1,040 B) and one record buffer (182 B) the saves and loads
share; the voices' model state is now a union (one model a row at a time), so eight voices cost no more than
before. Nothing more in the pool. The cost (`tools/costprobe`, an estimate on the target's listing): the eight rows
on the new models at their heaviest (two ANALOG, two FM, three CHIP, one NOISE, all sounding, DECAY 90) 766
instructions a sample, against 671 for V0.85's eight rows: about 22 % of a core at 154/us, 3 points more; a usual
beat sounds two or three rows at once. **Needs a hardware test** (BENCH's AUDIO row with a kit playing).

## 18. V0.87 step 1 (the interface's changes, the LEDs)

| | V0.87 step 1 | |
|---|---:|---|
| App image | 491,644 B | +9,580 on V0.86 step 2; of the app slot's 581,564 B: 89,920 B free |
| RAM .data + .bss | 65,908 B | of 73,728 |
| Pool | 362,036 B | of 380,928 |
| Interrupt stack | ~3,056 B | of 4,096 allowed |
| Main stack | ~5,276 B | of 8,192 allowed |

What it added: the windows (assign, SAVE, the questions) and the lists by kind (presets, wavetables, kits, projects)
in the UI's own state (a few dozen bytes: no buffers); TAKE's ring of the last 256 notes (`lpk`, ~1.3 KB of .bss,
the one buffer of size); POLARITY a parameter (2 bytes a preset); the LED scan's quarters (TIMER5 at 40 kHz, three
short interrupts between ticks: a compare and at most one line write each; the tick's work unchanged, so the main
loop and the audio see the same load; **needs a hardware test** for how it looks). JUMP, the PROJECT page and the
held-5-s resets went. The size and RAM review after V0.87 (step 2 first) looks at what can go.

## 17. V0.86 step 2 (the drum editor)

From the build (2026-10-22, with the renders review's fixes): the image **482,064 B** (+17,584 on step 1: the eight
factory kits, 1,456 B; the drums' effects, their set-up at a hit (`sy_drum_hit` 5,022 B with it inlined) and their
blocks; DRUM's and KIT's new pages, ~6 KB (EQ's response 1,794 B and its curve 1,062 B, COMP's 1,396 B, DRIVE's 796
B); the kits' list; the review's PRESETS list by SAVE twice, SAVE + OCT+ and SAVE + OCT−); `.data` + `.bss`
**63,636 B** (+816: the eight drum voices' effect state, 8 x 100 B; COMP's and HOLD's state; the list's asks); the pool
362,036 B (unchanged); the interrupt stack ~3,056 B (+340: COMP's bus, two blocks of 128 B, in `sy_drum_block`) of
4,096 allowed; the main stack ~5,624 B (+4) of 8,192 allowed. Left: **99,500 B** of the app slot, **10,092 B** of
`.data` + `.bss`, **18,892 B** of the pool (4 KiB of each the build's margin); the spare SRAM 22,156 B used, 22,784 B
left. The flash: nothing new; the kit's step-2 fields were in step 1's stream (version 3, 182 B of kit).

The engine (`tools/costprobe/sy_dyn.sh`; `tests/sy_drum_golden.txt` and `tests/sy_golden.txt` unchanged: a drum with
its effects at rest and COMP off plays bit for bit as before):

| Scenario | Step 1 | Step 2 |
| --- | ---: | ---: |
| idle | 158 | 156 |
| the eight drums sounding at once | 676 | 678 |
| SUPERSAW, 8 notes, the drums, the duck | 4,834 | 4,834 |
| the eight drums at once, every effect on each (DRIVE SOFT, the three EQ bands, SHAPE) and COMP | | 1,203 (+525: 11-15 % of a core) |

The plan estimated ~240 a sample more with everything on; measured, the worst case (the new row: all eight drums
sounding at once, every effect on each) is +525. Its parts (the model's per-line counts): DRIVE ~20 a drum (SOFT; the
other TYPEs not counted), the three EQ bands ~29 a drum (LOW or HIGH alone ~10, MID alone ~18), SHAPE nothing a sample
(it is in the drum's own gain ramp, a block's two gains), COMP ~22 for the eight (a block's peak, its gain stepped
over the block). Two passes brought it from 1,323 to 1,203: each effect's coefficients held in registers through its
loop (a loop a DRIVE TYPE, a loop for the bands in use), and SHAPE into the gains (which put it after DRIVE and EQ;
before, it fed DRIVE). A beat seldom sounds more than three drums at once: with every effect on them, ~200 more. An
effect at its neutral setting costs nothing (set at each hit: DRIVE 0 or its MIX 0, a band at its middle, SHAPE's two
at theirs, COMP's AMOUNT 0). The drums count in the admission's block time as before (§15). **Needs hardware test**:
BENCH's figure for a beat with effects on.

## 16. V0.86 step 1 (the interface)

From the build (2026-10-22): the image **464,480 B** (+26,356 on V0.85: the pages regrouped and drawn by parameter,
HOME's MIX pages and its drum kit's view, PRESETS' list, the EQ's SHAPES and the effects' new parameters, SEQ's
TOOLS, SPEED and TYPE's guard, the buttons' second press, the help map, SETTINGS' POWER and KEY SCAN, the screen's
sleep, the centre detents); `.data` + `.bss` **62,820 B** (+592: the UI's new state, ~140 B; PRESETS' list and its
copy of the sound before, the tools' clipboard and asks; the clips' SPEEDs, 64 B; the battery's reading); the pool
362,036 B (+176); the interrupt stack ~2,716 B (+4) of 4,096 allowed; the main stack ~5,620 B (+816: the help map's
page, 2 x 96 B strings) of 8,192 allowed. Left: **117,084 B** of the app slot, **10,908 B** of `.data` + `.bss`,
**18,892 B** of the pool (4 KiB of each the build's margin). HOME's picture stays inside the V0.67 rule: the drum
kit's view reuses the bloom's code and buffers (the bloom smaller, placed by three variables), nothing new kept for
it. The flash: a project's stream is version 3 (the clips' SPEEDs, 64 B, and the kit's step-2 fields, 110 B, in the
four objects as before); versions 1 and 2 load (every clip at x1, the kit's new fields at their defaults). The
settings record is 4 B longer (SHOW BATTERY, SCREEN OFF AFTER, two spare): a record from before loads, these two
OFF.

The engine (`tools/costprobe/sy_dyn.sh`; `tests/sy_golden.txt` unchanged: the effects' new parameters at their
defaults leave every render bit for bit as before):

| Scenario | V0.85 | V0.86 |
| --- | ---: | ---: |
| idle | 156 | 158 |
| SUPERSAW, 8 notes | 4,301 | 4,304 |
| the eight drums sounding at once | 675 | 676 |
| SUPERSAW, 8 notes, the drums, the duck | 4,832 | 4,834 |

What the new parameters cost when used (from the code, not measured on the FM-1): the chorus's SPREAD and the
tremolo's a multiply each; the FX filter's FOLLOW a block's peak (128 compares) and a coefficient's update a block;
the tape's HISS a noise sample and a gate's level a sample; the EQ's PASS, NOTCH and BAND types the same biquad as a
PEAK (a point at 0 dB is skipped only when it is a gain type).

## 15. V0.85 (the drums)

From the build (2026-10-21): the image **438,124 B** (+20,940 on V0.81: `sy_drum.c`, the drum synth and the duck, ~5
KB; `sy_ui_drum.c`, the KIT's and the grid's pages, ~11 KB; the looper's hits, ratchets and grid, the stream's
version 2); `.data` + `.bss` **62,228 B** (+1,568: the eight drum voices, ~1 KB; the kit, 72 B; the ratchets to come,
8 x 12 B; the hits counted, 36 B; the UI's picks); the pool 361,860 B (+72: the project's head gathered, the kit in
it); the interrupt stack ~2,712 B (+176: a block of a drum, 128 B) of 4,096 allowed, the main stack ~4,804 B. Left:
**143,440 B** of the app slot, **11,500 B** of `.data` + `.bss`, **19,068 B** of the pool (4 KiB of each the build's
margin). The flash: a project's stream 72 B longer (the kit), inside its four objects as before.

The engine (`tools/costprobe/sy_dyn.sh`; the synth's renders bit for bit as before: `tests/sy_golden.txt` unchanged):

| Scenario | V0.81 | V0.85 |
| --- | ---: | ---: |
| idle (the drums' block, nothing sounding) | 153 | 156 |
| SUPERSAW, 8 notes | 4,300 | 4,301 |
| the eight drums sounding at once | | 675 (518 over idle: 11-15 % of a core) |
| SUPERSAW, 8 notes, the drums, the duck | | 4,832 |

The drums' own: the three metal voices ~170 (V0.85's first build 232: their six squares now counted in a pass of their
own, the phases in registers, the renders the same bit for bit), the mixing ~127 (each voice into both sides, its
track's gains ramped), WT 1 and 2 ~52, SNARE ~40, CLAP ~27, KICK ~19. A beat seldom sounds more than three at once (a
kick, a hat, a snare or clap: ~150-200). They count in the admission's block time like any fixed cost (sy_engine.c):
a heavy sound has its voices trimmed first.

## 14. V0.81 (V0.8 LOOPER's fixes, and less of everything)

From the build (2026-10-21): the image **417,184 B** (−1,176 on V0.8 LOOPER: the looper's recording state `lpr`
was 1,636 B of `.data` for two initial values, now set in `lp_boot`; +~460 B of code: USB MIDI out's recovery, the
matrix's lists); `.data` + `.bss` **60,660 B** (−1,024: `wg_a`, the wavetable generator's harmonics, was outside the
pool since V0.6, as `static float wg_a[N], wg_b[N] SY_POOL;` gives the section to the last name only); the pool
361,788 B (+1,024, the same array); the stacks as V0.8 LOOPER's (interrupts ~2,536 B, main ~4,788 B). Left: **164,380
B** of the app slot, **13,068 B** of `.data` + `.bss`, **19,140 B** of the pool (4 KiB of each the build's margin).

The engine (`tools/costprobe/sy_dyn.sh`, instructions a sample on the target's listing, the renders bit for bit as
V0.8's: `tests/sy_golden.txt` unchanged):

| Scenario | V0.8 | V0.81 | |
| --- | ---: | ---: | ---: |
| idle | 166 | 153 | −7.8 % |
| 1 voice, A × 1 | 313 | 295 | −5.8 % |
| 8 voices, A × 1 | 1,368 | 1,314 | −3.9 % |
| 8 voices, A + B × 1, LP12 | 2,222 | 2,166 | −2.5 % |
| 8 voices, A + B × 3, LP24, every effect | 4,961 | 4,904 | −1.1 % |
| SUPERSAW, 1 note | 1,368 | 1,350 | −1.3 % |
| SUPERSAW, 8 notes | 4,356 | 4,300 | −1.3 % |
| PLUCK, 8 notes | 1,996 | 1,939 | −2.9 % |
| PAD, 8 notes | 3,870 | 3,817 | −1.4 % |

Where: the matrix's offsets (`mod_offsets`, twice a block and once a voice) walked all eight slots and looked up each
destination's section; `mod_map` now keeps each pass's slots and offsets as lists when the slots change (the sums in
the same order). The parameters' change scan (`sp_block`, every block) tests four pairs a branch. The starter sounds are
scenarios now (`sy_profile.c`): the V0.65 test ran them, and V0.8's "every effect" scenarios ran no effect at all (the
chain was empty: `sp_chain_from_old` now builds it).

What the rest costs, and why it stays: in SUPERSAW's 8 notes the oscillators' kernel is 1,026 (~21 instructions an
instance a sample, already the V0.65 floor), the SVF 646 (20 a stage a channel a sample, no spills), the reverb 451 (a
third of a single note's cost; reading its six steady lines at offsets fixed a block would move the goldens), the
unison voices' setup ~300 an instance a block (the price of 32-sample blocks: a longer block halves it and moves
every golden and the latency). The idle 153 is the master's two DC blockers and Q15 out, the parameters' scan, the
matrix's map: ~4 % of a core.

The chip (the AC7911B8 datasheet, [PHASE0.md](PHASE0.md) §2): the core is rated DC–320 MHz and the boot loader leaves
it at 360 MHz, so there is no clock to gain; the measured ~180 integer instructions a µs is the 180 MHz bus's clock,
which suggests memory traffic, not instructions, sets the pace (**unverified**: BENCH times instruction loops, not
loads). The official firmware's 28 KB of cache RAM asks for 7 ways of a 6-way D-cache: not taken. Its SPI2 DMA panel
scan fits this board's pins (PA3, PA4) but would save ~0.5 % of the CPU while any LED glows (2 of the 16 bits) (the bit-banged shift is
also the glow's timer): not taken.

Not done, and why: a compact table of the parameters held by the looper's tracks (~640 B: needs a policy for a full
table, a change of behaviour); one list for UNDO's pass and the one before (512 B: needs proof they are never both in
use); the project's selected track and scene kept out of its autosave's comparison (they are saved on purpose, as the
sound's edits are; a write is one sector erase in two alternating sectors).

## 13. V0.8 (the looper)

From the build (2026-10-21): the image 418,360 B (+17,724 on V0.8 BENCH: `sy_loop.c`, its pages in `sy_ui_loop.c`, the
chords a track, the project's stream; V0.8's sequencer gone); `.data` + `.bss` 61,684 B (+8,048: the chords' eight
sides, ~4.2 KB; the looper's state, ~3.3 KB (each track's place and notes sounding, the recording's pass lists for
UNDO); the parameters held, 3 bytes each); the pool 360,764 B (+3,532: the 2,048 events, 16 KB, and the project's head
while it is read, 1.5 KB, in place of the patterns' 14.3 KB); the main stack ~4,788 B, the interrupt stack ~2,532 B.
The flash: the user presets 64 (13 pairs, `0x97000`–`0xB0FFF`), projects 1 and 2 at `0xCB000`–`0xDAFFF`, 3 and 4 at
`0xB1000`–`0xC0FFF` (four objects a project; a project of 2,048 events fills 3.6 of them), free: `0xC1000`–`0xCAFFF`
and `0xDB000`–`0xDFFFF` (60 KiB). Left: **163,204 B** of the app slot, **12,044 B** of `.data` + `.bss` (the build
keeps 4 KiB of it), **20,164 B** of the pool (4 KiB kept).

The drawing's time on the host (`web/emu/strata_bench.c`, three runs): LOOP's CLIPS with three clips playing
1,108–1,121 µs over 2 s, CLIP's NOTES with its clip playing 1,207–1,262 µs (the page drawn again 12 times a second
while it runs, at once when something changes: 2,015–2,300 µs at 25 a second before; V0.8's NOTES was 716–725, ARP's
STEPS 883–921); HOME 762–779 µs with a chord and 37–41 µs in silence, unchanged. The audio side: a block walks each playing
clip's ticks since the last (a few events), the eight chords' sides (most idle), the gains; a knob recording looks
through its clip once a block while it turns.

## 12. V0.8 BENCH (the second core, measured)

From the build (2026-10-21): the image 400,636 B (+8,632 on V0.8: `sy_c2.c`, CORE 2's page, the console's lines);
`.data` + `.bss` 53,636 B (+4,644: core 1's two stacks, 4,096 + 256 B, `hal/fm1_cpu1.S`; its mailbox, two cache
lines each way; the test's sums); the pool 357,232 B (+512: the test's 32 phases and increments and its block
buffer); `.c1_text` (core 1's waiting loop, 50 instructions) in RAMTEXT before `.spare_bss`, which is as V0.8's. The
interrupt stack ~2,500 B (the test's job runs in the audio interrupt), the main stack ~4,936 B. The voices' code as
V0.8 built it: the test reads the oscillators' kernels (`osc_run`, already called from several places) rather than
`osc_block`, whose second caller made the compiler stop inlining it into the voice (found on the way: the first try
moved `sy_block` out of the audio interrupt and `osc_block` out of the voice). Left: **180,928 B** of the app slot,
**20,092 B** of `.data` + `.bss`, **23,696 B** of the pool (4 KiB of each the build's margin).

What V0.9 needs from it ([LOOPER.md](LOOPER.md)): core 1's time for wavetable work against core 0's, how much core 0
slows while both run (the shared bus and caches), the wait, and whether the unit's core 1 answers at all. **Needs a
hardware test**: all of it (FLASHING.md step 30); the emulator has one core.

## 11. V0.8 (the sequencer)

From the build (`tools/build.py --strata`, 2026-10-21): the image 392,004 B (V0.75 371,420: +20,584: the sequencer in
`sy_seq.c`, its pages in `sy_ui_seq.c`, the buttons' and the settings' parts in `sy_ui.c`); `.data` + `.bss` 48,992 B
(+448: the sequencer's settings and its audio side's state: the notes sounding, the notes held while recording, the
steps recorded ahead of the playhead); the pool 356,720 B of 380,928 (+14,348: the sixteen patterns, 844 B each,
13,504 B, and PATTERNS' copy, 844 B; set at power-on, as the pool is not cleared); the spare SRAM as V0.75's; the main
stack ~4,924 B (+52), the interrupt stack ~2,520 B (as V0.75's) of 4,096 allowed. The flash: the patterns in four
sector pairs at `0xCB000`–`0xD2FFF`, of the user store's free 84 KiB: **52 KiB** left there (`0xD3000`–`0xDFFFF`).
Left: **189,560 B** of the app slot, **24,736 B** of `.data` + `.bss`, **24,208 B** of the pool (4 KiB of it the
build's margin).

Found on the way: the patterns first went to `.spare_bss` and did not link (RAMTEXT over by 6,404 B). The "23,296 B
left" of the spare SRAM counts both of its areas; `.spare_bss` itself (RAMTEXT, after the RAM code and the 21,900 B
in it) has ~7.9 KB free, the rest is NOINIT's area after `.noinit` (~15 KB), painted and watched but not linked into.

The drawing's time on the host (`web/emu/strata_bench.c`, three runs): SEQ's NOTES while a pattern plays 716–725 µs
over 2 s, 75,600 pixels sent (the playhead's marker and the steps' boxes, each step); HOME 744–755 µs with a chord
and 36–38 µs in silence (V0.75: 769–823 and 35–37: HOME is untouched); ARP's STYLE 652–656 µs and STEPS 848–879 µs
while the arpeggio plays (the bench now finds them: it looked for PERFORM, STYLE's old name, and for STEPS in the ARP's
section, and set HARP). The audio side: a block without a step checks sixteen notes' ends; a step is its notes' keys
into the chords' side, a few hundred instructions at most: under 0.1 % of a core.

## 10. V0.75 (the ARP)

From the build (`tools/build.py --strata`): the image 371,420 B (V0.7 358,264: +13,156: the step arpeggiator in
`sy_chord.c`, the chords' TYPEs and SHAPEs, SPREAD and STRUM, the pages in `sy_ui_chord.c`, the buttons' grammar,
MIDI's clock in `sy_app.c`); `.data` + `.bss` 48,544 B (+888: the chords' settings and state, the steps, GROOVE, the
transport, STRUM's starts put off (16 x 12 B); MIDI's clock, its last 49 times 196 B); the pool and the spare SRAM as
V0.7's; the main stack ~4,872 B (+248), the interrupt stack ~2,520 B (+44) of 4,096 allowed. The fonts are V0.7's (50.7
KB). Left: **210,144 B** of the app slot, **25,184 B** of `.data` + `.bss` (21.2 KB over the build's margin), the
pool's 38,556 B and the spare SRAM's 23,296 B.

The drawing's time on the host (`web/emu/strata_bench.c`, three runs): HOME in silence 35–37 µs over 2 s and with a
chord 769–823 µs (V0.67's 35 and 1,462: the bloom's 106 seeds now, a quarter of its 424; its state as before, 8 B); ARP's PERFORM page while the arpeggio plays 661–698 µs over 2 s
and STEPS 862–938 µs (ENV 1 with a chord held: 670–750), each sending only the rows where the step playing is marked
(22,080 pixels in 2 s). The audio side's cost is a step's few hundred instructions at most, a block's few tens between
steps: under 0.1 % of a core.

## 9. V0.7 (the chords)

From the build (`tools/build.py --strata`): the image 358,264 B (V0.67 339,112: +19,152, of it the chords' code ~16.4 KB,
`sy_chord.c` and its pages in `sy_ui_chord.c`, both built for size except what the audio interrupt runs, and the
chord names' letters in the large faces, 2.1 KB); `.data` + `.bss` 47,656 B (+528: the chords' state 242 B, Felucca's
USB MIDI out queue 256 B, now that something sends); the pool, the spare SRAM and the stacks as V0.67's. Left: 223,300 B
of the app slot, 26,072 B of `.data` + `.bss` (22.0 KB over the build's margin), the pool's 38,556 B and the spare
SRAM's 23,296 B.

## 8. V0.67 (the STRATA UI made legible) and the room left

From the build (`tools/build.py --strata`, 2026-10-18):

| | V0.66 | V0.66 UI | V0.67 | Left for what comes next |
| --- | ---: | ---: | ---: | --- |
| App image (of the app slot's 581,564 B) | 327,184 B | 326,280 B | 339,112 B | **242,452 B** (41.7 % of the slot) |
| `.data` + `.bss` (of 73,728 B) | 46,560 B | 47,128 B | 47,128 B | **26,600 B** (the build keeps 4 KiB of it spare: 22,504 usable) |
| Pool (of 380,928 B) | 358,692 B | 342,372 B | 342,372 B | **38,556 B** (4 KiB kept spare: 34,460 usable) |
| Spare SRAM (`.spare_bss`, of 45,196 B) | 19,296 B | 21,900 B | 21,900 B | **23,296 B** (watched on BENCH) |
| Main stack (12,032 B reserved) | | ~4,564 B | ~4,572 B | 8,192 B allowed by the build: ~3.6 KB of depth |
| Interrupt stack (7,936 B reserved) | ~2.5 KB | ~2.5 KB | ~2,496 B | 4,096 B allowed: ~1.6 KB |
| The user store's free part (`0xCB000`–`0xDFFFF`) | 84 KiB | 84 KiB | 84 KiB | **84 KiB** for user content (samples, patterns) |

What V0.67 spent, all in flash (RAM and the pool are V0.66 UI's to the byte): the fonts 39.0 → 48.6 KB (nothing
under 9 px), the 72 VALUES pages' table rows (1.4 KB) and their page (0.9 KB), the knobs' names and values split out
for VALUES (1.4 KB), the screens' larger layouts. The bloom: its state 8 B, as the trace's; HOME's code 50 B less.

**RAM free in all: 48.7 KiB** (26,600 B of `.data` + `.bss` and 23,296 of the spare SRAM; the pool's 38,556 besides:
86.4 KiB counting the pool). For a groovebox's patterns, an arpeggiator's and a chord memory's state, these are
plenty (a 16-step pattern of 8 tracks: well under 1 KB). What is short is the CPU (ARCHITECTURE.md §13: a chord of
eight two-oscillator voices with unison and the full rack does not fit one core at the expected rate; the second core
is still off) and the flash for built-in content (242 KB: ~22 s of 4-bit ADPCM at 22 kHz, or factory presets by the
thousand at ~0.6 KB each). [UI.md](UI.md) §5 has the drawing's time.
