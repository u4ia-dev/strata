#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
# STRATA's host tests built as WebAssembly (Emscripten) and run under Node: the DSP the browser runs (the workbench,
# the emulator) checked as the host's is, golden renders included. All passing says the WebAssembly engine plays the
# test sounds bit for bit as the host build ("verified in emulator" for the DSP).
#   tests/sy_wasm.sh            (from the repo root; emcc on the PATH, as after `. ~/emsdk/emsdk_env.sh`, or an
#                                emsdk in $EMSDK or ~/emsdk; node)
# QUICK=1: the oscillator's aliasing on every 4th note only (as tests/sy_tests.sh).
set -e
cd "$(dirname "$0")/.."
if ! command -v emcc >/dev/null 2>&1; then
    EMSDK="${EMSDK:-$HOME/emsdk}"
    PATH="$EMSDK:$EMSDK/upstream/emscripten:$PATH"
    export EMSDK PATH
fi
command -v emcc >/dev/null 2>&1 || { echo "sy_wasm: no emcc (Emscripten)"; exit 1; }
OUT=build/wasm
mkdir -p "$OUT" build/gen
EMCC="emcc -O2 -ffp-contract=off -Wall -Wno-unused-function -Wno-unused-parameter -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 \
    -sSTACK_SIZE=1048576 -sENVIRONMENT=node -sEXIT_RUNTIME=1"
fail=0
run() { echo "== $1"; shift; node "$@" || fail=1; }

[ -f build/gen/strata_wt.h ] || python3 tools/gen_wavetables.py build/gen

$EMCC -Ibuild/gen -o "$OUT/sy_wt_test.js" tests/sy_wt_test.c -lm
run "wavetables (wasm)" "$OUT/sy_wt_test.js" build/gen/strata_wt_ref.bin

$EMCC -Ibuild/gen -o "$OUT/sy_osc_test.js" tests/sy_osc_test.c -lm
run "oscillator (wasm)" "$OUT/sy_osc_test.js" ${QUICK:+quick}

$EMCC -Ibuild/gen -o "$OUT/sy_smoke.js" tests/sy_smoke.c -lm
run "engine: a chord (wasm)" "$OUT/sy_smoke.js"

$EMCC -Ibuild/gen -o "$OUT/sy_filter_test.js" tests/sy_filter_test.c -lm
run "filter (wasm)" "$OUT/sy_filter_test.js"

$EMCC -Ibuild/gen -o "$OUT/sy_store_test.js" tests/sy_store_test.c -lm
run "store (wasm)" "$OUT/sy_store_test.js"

$EMCC -Ibuild/gen -o "$OUT/sy_engine_test.js" tests/sy_engine_test.c -lm
run "engine, golden renders included (wasm)" "$OUT/sy_engine_test.js" tests/sy_golden.txt

[ $fail = 0 ] && echo "sy_wasm: all passed" || echo "sy_wasm: FAILED"
exit $fail
