#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
# The Strata DSP as the FM-1 will run it: tools/costprobe/sy_probe.c (every sy_* source) compiled by the JieLi
# clang for the pi32v2 with the FPU (-mcpu=r3 -mfprev1), and the instructions in each kernel's inner loop counted
# from the compiler's own assembly (count_asm.py). tools/costprobe/budget.py turns them into voices.
#   tools/costprobe/sy_cost.sh [OPT]          (OPT: -O2 by default; JIELI_TOOLCHAIN as for build.sh; run from
#                                              the repo root after tools/gen_wavetables.py build/gen)
set -e
cd "$(dirname "$0")/../.."
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
OPT="${1:--O2}"
OUT=build/cost
mkdir -p "$OUT"
"$TC/pi32v2/bin/clang" -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off "$OPT" -fno-builtin -Wall \
    -Wno-unused-function -Ibuild/gen -S tools/costprobe/sy_probe.c -o "$OUT/sy.s"
"$TC/pi32v2/bin/clang" -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off "$OPT" -fno-builtin -Wall \
    -Wno-unused-function -Ibuild/gen -c tools/costprobe/sy_probe.c -o "$OUT/sy.o"
python3 tools/costprobe/count_asm.py "$OUT/sy.s" \
    osc_k_plain osc_k_plain_g osc_k_plain1 osc_k_plain1_g osc_k_map osc_k_map_g osc_k_map1 osc_k_map1_g \
    osc_k_sync osc_k_sync_g osc_k_sync1 osc_k_sync1_g osc_k_pm osc_k_pm_g osc_k_pm1 osc_k_pm1_g \
    flt_svf_lp flt_svf_bp flt_svf_hp flt_svf_notch flt_svf_peak flt_ladder voice_sub voice_noise probe_mix
