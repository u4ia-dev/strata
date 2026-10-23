# Strata: notices

Strata, firmware for the M-VAVE FM-1. Copyright (C) 2026 u4ia. Strata is free software under the GNU General Public
License, version 3 only (`GPL-3.0-only`, full text in [LICENSE](LICENSE)). It is a modified version of **Felucca**,
Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, also `GPL-3.0-only`:
<https://github.com/hugelton/Felucca>. Strata forked from Felucca 1.6.1 (commit `a53ecf428c29c780108fd70a31b71a32a6b5c313`, 2026-10-15) and keeps
Felucca's history, its copyright notices and its licences.

## Disclaimer

Strata is independent, unofficial firmware. It is not affiliated with, endorsed by or supported by M-VAVE or the
owners of the FM-1, nor by Felucca's author or Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
respective owners; "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. They are used here only to
say what Strata runs on and where it comes from.

Strata is provided **as is, without any warranty**, under the GNU General Public License version 3 (sections 15 and
16: no warranty, no liability). Installing any firmware other than the official one can leave the FM-1 unusable until
it is recovered, and you install Strata at your own risk. Strata is a **pre-release**: its versions from V0.7 on are
being tested on the hardware now.

## What Strata changed (GPL-3.0, section 5(a))

Strata's own files (the STRATA engine and UI `firmware/src/sy_*.c`, `firmware/src/strata.c`, its tests `tests/sy_*`,
the browser emulator's Strata build, the Strata flasher `web/strata_flash/`, its tools and docs) carry
`Copyright (C) 2026 u4ia`. These files of Felucca were modified for Strata from 2026-10-14 on (each code file says so
in its header):

- `CREDITS.md`
- `LICENSING.md`
- `README.md`
- `firmware/hal/fm1_flash.h`
- `firmware/hal/fm1_input.h`
- `firmware/hal/fm1_timer.h`
- `firmware/loader/loader.c`
- `firmware/src/gfx.c`
- `firmware/src/lcd.c`
- `firmware/src/usb.c`
- `tests/drumfuzz_test.c`
- `tools/build.py`
- `tools/gen_aa_font.py`
- `web/emu/index.html`

## What a copy of Strata must come with

1. **The complete corresponding source** (GPL-3.0, section 6): this repository, including the `firmware/engines`
   submodule (Felucca-Engines, <https://github.com/hugelton/Felucca-Engines>, at commit
   `c46e0086f69bdc26df1cd144dca9e683cc4531dd`). GitHub's automatic archives leave the submodule empty: a release
   offers `strata-X.Y-source.tar.gz` with its files in it (docs/RELEASE.md). The site links the source next to every
   package.
2. **The licence texts** with every package and on the site: [LICENSE](LICENSE), [LICENSING.md](LICENSING.md),
   [LICENSES/](LICENSES/) and this file.
3. **The notices as they are**: Felucca's and every third party's copyright lines stay in their files.
4. **The installation information** (GPL-3.0, section 6, the FM-1 being a user product): the update path and the
   package format are documented (docs/FLASHING.md, BUILDING.md, `tools/build.py`, `web/fm1pkg.js`); anyone can build
   and install a modified Strata the same way.

## In the firmware image and its package

| Part | Licence | Notes |
|---|---|---|
| Strata (`sy_*.c`, `strata.c`) | GPL-3.0-only | Copyright (C) 2026 u4ia |
| Felucca's platform code compiled into Strata (the HAL `firmware/hal/`, `libc.c`, `lcd.c`, `gfx.c`, `panel.c`, `usb.c`, `midi_uart.c`, `ota.c`, the update loader `firmware/loader/`) | GPL-3.0-only | Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments; modified as listed above |
| Space Mono, rasterised into the UI's text at build time (`tools/gen_aa_font.py`) | SIL OFL 1.1 | Copyright 2016 The Space Mono Project Authors; [LICENSES/OFL-SpaceMono.txt](LICENSES/OFL-SpaceMono.txt). The generated tables are not offered as a font |
| JieLi AC79 SDK files in every `.fwsc` package (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`, unmodified, read from the builder's SDK) | Apache-2.0 | Copyright JieLi Technology; [LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt); no NOTICE file. Not in this tree |
| Strata's wavetables, sounds, drum presets and kits | GPL-3.0-only | Made for Strata (formulas and parameter lists; no third-party audio) |

Felucca's own engines (Felucca-Engines, MIT), its ported DSP (DaisySP and Rings, MIT; msfa, Apache-2.0), its sample
sets (CC0), its fonts (Fukiai MIT, Inter Tight and DotGothic16 OFL) and the second-core and emulator credits (X0X,
Melodee, Jangada) are in the source tree as Felucca has them and are listed in [LICENSING.md](LICENSING.md) and
[CREDITS.md](CREDITS.md); Strata's firmware image does not compile Felucca's engines, its ported DSP or its samples.

## Vendor material, names, radio

- No M-VAVE material is in Strata or its packages. Returning to the official V15 uses a file the user downloads from
  M-VAVE; the page keeps only its size and SHA-256 and never uploads or redistributes it.
- Strata identifies itself over USB as "Strata for FM-1" by "u4ia" (pid.codes VID 0x1209, PIDs 0x5354 and 0x5355:
  docs/RELEASE.md), never as Felucca, M-VAVE or Hügelton.
- Strata never enables the FM-1's Bluetooth or Wi-Fi radio.
