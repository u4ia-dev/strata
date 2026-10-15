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

[ $fail = 0 ] && echo "sy_tests: all passed" || echo "sy_tests: FAILED"
exit $fail
