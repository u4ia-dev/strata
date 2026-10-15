#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Phase 0 cost probe: compile tools/costprobe/kernels.c for the target (JieLi clang, pi32v2,
# the FPU flags the FM-1 firmwares that use floats ship with) and count each kernel's loop.
#   tools/costprobe/run.sh            (JIELI_TOOLCHAIN as for build.sh)
set -e
cd "$(dirname "$0")"
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
OUT="${OUT:-/tmp/costprobe}"
mkdir -p "$OUT"
"$TC/pi32v2/bin/clang" -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -O2 -fno-builtin -Wall \
    -c kernels.c -o "$OUT/kernels.o"
"$TC/common/bin/objdump" -d "$OUT/kernels.o" > "$OUT/kernels.dis"
python3 count_loops.py "$OUT/kernels.dis"
