#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
# STRATA host tests: the synth's DSP (firmware/src/sy_*) compiled with the host compiler, no HAL, no hardware.
# Run from the repo root (Linux / WSL2; python3 with numpy):
#   tests/sy_tests.sh
# Everything here is "verified on host": it says nothing yet about the FM-1's timing (tools/costprobe and the
# BENCH page do) or its codec. Demos (WAV) go to build/sy_demo/.
#   WT        (tests/sy_wt_test.c) the on-device wavetable generator against tools/gen_wavetables.py's reference
#             renders; the format: band limits, DC, guards, peak, the levels against each other; wt_level.
#   OSC       (tests/sy_osc_test.c) the oscillator alone: every table's aliasing over C0..C8 (printed, held to
#             regression limits), the warps', pitch (0.05 cent), unison power and symmetry, level hysteresis, cost.
#             QUICK=1: every 4th note.
#   SMOKE     (tests/sy_smoke.c) the whole engine's first sound: a chord through the default patch.
#   FILTER    (tests/sy_filter_test.c) every type's response against its design (cutoff, slopes, resonance, notch,
#             peak, ladder, formant, stereo spread), stability at every corner with the cutoff jumping, no denormal
#             state, exact silence after the input stops, MIX 0 bit for bit, cost.
#   ENGINE    (tests/sy_engine_test.c) the parameter table and the preset format, the matrix, voices (POLY steals,
#             VOICES, MONO / LEGATO, glide, sustain, the unison budget, the CPU guard), envelope and LFO times,
#             clicks (17 events against the steady signal), health (40 random patches), DC (the blockers, every
#             source's mean), determinism and golden renders (tests/sy_golden.txt; GOLDEN_UPDATE=1 rewrites it, for
#             an intended change of sound only), effects (OTT sums back, the synced delay, extremes bounded), cost.
#             Demos (the test sounds as WAV) in build/sy_demo/.
set -e
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT" build/gen build/sy_demo
CC="${CC:-cc} -O2 -Wall -Wextra -Wdouble-promotion -Wno-unused-function -Wno-unused-parameter"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

python3 tools/gen_wavetables.py build/gen

$CC -Ibuild/gen -o "$OUT/sy_wt_test" tests/sy_wt_test.c -lm
run "wavetables: the C generator against the reference, the format, the level choice" "$OUT/sy_wt_test" build/gen/strata_wt_ref.bin

$CC -Ibuild/gen -o "$OUT/sy_osc_test" tests/sy_osc_test.c -lm
run "oscillator: aliasing of every table and warp, pitch, unison, level hysteresis, cost" "$OUT/sy_osc_test" ${QUICK:+quick}

$CC -Ibuild/gen -o "$OUT/sy_smoke" tests/sy_smoke.c -lm
run "engine: a chord through the default patch" "$OUT/sy_smoke"

$CC -Ibuild/gen -o "$OUT/sy_filter_test" tests/sy_filter_test.c -lm
run "filter: responses, stability at the corners, no denormals, silence, cost" "$OUT/sy_filter_test"

$CC -Ibuild/gen -o "$OUT/sy_engine_test" tests/sy_engine_test.c -lm
run "engine: parameters, matrix, voices, timing, clicks, health, DC, golden renders, effects, cost" \
    "$OUT/sy_engine_test" tests/sy_golden.txt build/sy_demo

[ $fail = 0 ] && echo "sy_tests: all passed" || echo "sy_tests: FAILED"
exit $fail
