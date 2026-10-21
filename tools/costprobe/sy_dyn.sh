#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
# The target cost model: the JieLi clang listing with each instruction's source line (sy_probe.c, -O2 -g, and
# -fno-unroll-loops: an unrolled loop's copies would each be weighed by all of its passes), the
# engine on the host with coverage playing each scenario of tools/costprobe/sy_profile.c, and sy_dyncount.py
# weighing the one by the other; then budget.py: voices and unison against the audio's time. An estimate until
# the BENCH page times the engine on an FM-1.
#   tools/costprobe/sy_dyn.sh          (from the repo root after tools/gen_wavetables.py build/gen; JIELI_TOOLCHAIN
#                                       as for build.sh)
set -e
cd "$(dirname "$0")/../.."
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
OUT=build/cost
P=$OUT/prof
mkdir -p "$P"
"$TC/pi32v2/bin/clang" -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -O2 -fno-unroll-loops -g -fno-builtin \
    -Wall -Wno-unused-function -Ibuild/gen -S tools/costprobe/sy_probe.c -o "$OUT/sy_g.s"
cc -O0 --coverage -Wall -Wno-unused-function -Wno-unused-parameter -Ibuild/gen -c tools/costprobe/sy_profile.c \
    -o "$P/sy_profile.o"
cc --coverage -o "$P/sy_profile" "$P/sy_profile.o" -lm
n=$("$P/sy_profile" | wc -l)
s=0
while [ "$s" -lt "$n" ]; do
    rm -f "$P"/*.gcda
    line=$("$P/sy_profile" "$s")
    echo "== $s: $line"
    gcov -t -j "$P/sy_profile.gcda" > "$P/gcov_$s.json" 2>/dev/null
    python3 tools/costprobe/sy_dyncount.py "$OUT/sy_g.s" "$P/gcov_$s.json" "${line%% *}" --json "$P/dyn_$s.json"
    s=$((s + 1))
done
python3 tools/costprobe/budget.py "$P"
