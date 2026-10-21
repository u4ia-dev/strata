#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
"""How far a change moves the golden renders: the engine test's float renders (GOLDEN_RAW=DIR) before and after.

    GOLDEN_RAW=build/golden_a build/host/sy_engine_test        (before the change)
    GOLDEN_RAW=build/golden_b build/host/sy_engine_test        (after)
    tests/sy_golden_diff.py build/golden_a build/golden_b

For each test sound: the largest sample difference, and the difference's level against the sound's (dB). A rewrite
meant to change only rounding should sit near the float's own noise (under -100 dB); anything audible is a change
of sound and wants a reason before GOLDEN_UPDATE=1 records it."""
import os
import sys

import numpy as np


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    a_dir, b_dir = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(a_dir) if n.endswith(".f32"))
    worst = -999.0
    for n in names:
        a = np.fromfile(os.path.join(a_dir, n), dtype=np.float32).astype(np.float64)
        pb = os.path.join(b_dir, n)
        if not os.path.exists(pb):
            print(f"{n[:-4]:14s} missing in {b_dir}")
            continue
        b = np.fromfile(pb, dtype=np.float32).astype(np.float64)
        if a.size != b.size:
            print(f"{n[:-4]:14s} lengths differ ({a.size} / {b.size})")
            continue
        d = b - a
        sig = np.sqrt(np.mean(a * a)) + 1e-30
        rd = np.sqrt(np.mean(d * d))
        db = 20.0 * np.log10(rd / sig) if rd > 0 else -999.0
        worst = max(worst, db)
        print(f"{n[:-4]:14s} max |diff| {np.max(np.abs(d)):.3e}   diff rms {db:7.1f} dB against the sound"
              + ("   (bit for bit)" if rd == 0 else ""))
    print(f"worst: {worst:.1f} dB" if worst > -999.0 else "worst: identical")
    return 0


if __name__ == "__main__":
    sys.exit(main())
