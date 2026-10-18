# Strata's flash and RAM (the V0.65 audit)

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
| **RAM free in all** | **12.2 KiB** | **30.1 KiB** | |
| SRAM Strata leaves unused | (not named) | 44 KiB | painted at power-on; BENCH says whether anything wrote there |

The build keeps 4 KiB of `.bss` and 4 KiB of the pool spare (its checks fail below that); the free figures above
include those margins.

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
| The undo history (8 patches, 7,008 B) into a spare area, or kept packed | 7 KB / ~1.3 KB of `.bss` | the spare areas first need the hardware's word; packed, the ghost of a turn needs it unpacked |
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
