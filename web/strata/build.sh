#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# STRATA's workbench: the synth's DSP as WebAssembly (web/strata/strata_web.c) and the page, as one HTML file:
#   web/strata/build.sh  ->  build/strata/strata_workbench.html (and strata.wasm, for web/strata/wb_test.mjs)
# Open the page from a local web server (python3 -m http.server in build/strata) or, in Chrome / Edge, as a file.
# Needs emcc on the PATH (or an emsdk in $EMSDK or ~/emsdk) and python3.
set -e
cd "$(dirname "$0")/../.."
if ! command -v emcc >/dev/null 2>&1; then
    EMSDK="${EMSDK:-$HOME/emsdk}"
    PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
    export EMSDK PATH
fi
command -v emcc >/dev/null 2>&1 || { echo "workbench: no emcc (Emscripten)"; exit 1; }
[ -f build/gen/strata_wt.h ] || python3 tools/gen_wavetables.py build/gen
OUT=build/strata
mkdir -p "$OUT"
emcc -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -Ibuild/gen \
    --no-entry -sSTANDALONE_WASM -sFILESYSTEM=0 -sINITIAL_MEMORY=16777216 -sSTACK_SIZE=262144 \
    -o "$OUT/strata.wasm" web/strata/strata_web.c
python3 web/strata/make_page.py "$OUT/strata.wasm" web/strata/worklet.js web/strata/index.html \
    "$OUT/strata_workbench.html"
