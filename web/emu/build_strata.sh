#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
# STRATA on the simulated FM-1 in the browser: the app (web/emu/strata_web.c) compiled to WebAssembly with Emscripten,
# played by Felucca's page and worklet (web/emu/index.html, worklet.js), the page's name and its storage keys made
# Strata's (so it keeps nothing of a Felucca emulator's on the same site).
#   web/emu/build_strata.sh   ->  build/emu_strata/{index.html, worklet.js, strata.wasm, fonts/}, and the page with
#                                 the module and the font in it (make_page_strata.py): strata_emulator.html (to open
#                                 locally), strata_emulator_content.html (to publish, with worklet.js beside it)
# Needs build/gen: the wavetables (tools/gen_wavetables.py build/gen) and the UI's fonts, icons, keycaps and palettes
# (tools/gen_aa_font.py, gen_aa_icons.py, gen_aa_keycaps.py, gen_ui_palettes.py: made here when missing).
set -e
cd "$(dirname "$0")/../.."
if ! command -v emcc >/dev/null 2>&1; then
    EMSDK="${EMSDK:-$HOME/emsdk}"
    PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
    export EMSDK PATH
fi
command -v emcc >/dev/null 2>&1 || { echo "emu_strata: no emcc (Emscripten)"; exit 1; }
mkdir -p build/gen build/emu_strata/fonts
[ -f build/gen/strata_wt.h ] || python3 tools/gen_wavetables.py build/gen
[ build/gen/ui_fonts.h -nt tools/gen_aa_font.py ] || python3 tools/gen_aa_font.py build/gen/ui_fonts.h --preset inter-tight
[ -f build/gen/ui_icons.h ] || python3 tools/gen_aa_icons.py build/gen/ui_icons.h
[ -f build/gen/ui_keycaps.h ] || python3 tools/gen_aa_keycaps.py build/gen/ui_keycaps.h
[ -f build/gen/ui_palettes.h ] || python3 tools/gen_ui_palettes.py build/gen/ui_palettes.h
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Wno-unused-variable \
    -Wno-unused-but-set-variable -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 \
    -o build/emu_strata/strata.wasm web/emu/strata_web.c
sed -e 's|<title>Felucca</title>|<title>Strata Emulator</title>|' \
    -e 's|content="Felucca, the multi-engine synth firmware for the FM-1, running in your browser: the same code the instrument runs."|content="Strata, a wavetable synth firmware for the FM-1, running in your browser: the same code the instrument runs."|' \
    -e 's|  Felucca by Leo Kuroshita, Hügelton Instruments · |  Strata, a Felucca fork · Felucca by Leo Kuroshita, Hügelton Instruments · |' \
    -e 's|<nav><a href="../installer/">Install</a><a href="../editor/">Editor</a><a href="https://github.com/hugelton/Felucca">Source</a></nav>|<nav><a href="https://github.com/hugelton/Felucca">Felucca</a></nav>|' \
    -e 's|Felucca is not affiliated with or endorsed by them.|Strata and Felucca are not affiliated with or endorsed by them.|' \
    -e 's|"felucca-emu-flash"|"strata-emu-flash"|; s|"felucca-emu-knobs"|"strata-emu-knobs"|; s|"felucca-emu-knobmode"|"strata-emu-knobmode"|' \
    -e 's|fetch("felucca.wasm")|fetch("strata.wasm")|; s|felucca.wasm: |strata.wasm: |' \
    -e 's|console.warn("Felucca: |console.warn("Strata: |; s|console.warn(`Felucca: |console.warn(`Strata: |' \
    -e 's|@VERSION@|V0.65|g; s|@PREVIEW@| (hardware test build)|g' \
    web/emu/index.html > build/emu_strata/index.html
grep -q 'fetch("strata.wasm")' build/emu_strata/index.html || { echo "emu_strata: the page did not take the module's name"; exit 1; }
cp web/emu/worklet.js build/emu_strata/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu_strata/fonts/
echo "emu_strata: build/emu_strata ($(wc -c < build/emu_strata/strata.wasm | tr -d ' ') B wasm)"
python3 web/emu/make_page_strata.py build/emu_strata
