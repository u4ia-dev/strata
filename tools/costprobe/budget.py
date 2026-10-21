#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
"""The CPU budget on the FM-1 from the target cost model (tools/costprobe/sy_dyn.sh writes DIR/dyn_N.json, one a
scenario of sy_profile.c): each scenario's share of one core, the parts (the engine idle, a voice, a unison
instance, the effects), and how many voices x unison fit.

Rates: listing instructions a microsecond on the device. 205 was measured on integer code (ChoralRoot, docs/CZ1.md);
this engine's float code has no device figure yet: the BENCH page will give it. So every figure here is an
estimate, shown at 150, 205 and 300 a microsecond.

    budget.py DIR
"""
import json
import os
import sys

FS = 44117.647
RATES = (150.0, 205.0, 300.0)
PLAN_LOAD = 0.75                       # the most we plan for (the guard sheds at 70 / 78 / 85 %)
SYSTEM = 0.08                          # audio ISR frame work, TIMER5 key scan, USB (ChoralRoot: 4-6 %)
NAMES = ["idle: no voice, no effect", "1 voice: A x 1", "1 voice: A x 16", "8 voices: A x 1",
         "8 voices: A + B x 1, LP12", "8 voices: A + B x 3, LP24, every effect",
         "8 voices: A + B x 16 (budget 48), LADDER, every effect",
         "8 voices: A BEND+ / B SYNC x 1, LP12, SUB + pink NOISE", "1 voice: A x 1, every effect"]


def load(d):
    out = {}
    for i in range(len(NAMES)):
        p = os.path.join(d, f"dyn_{i}.json")
        if os.path.exists(p):
            with open(p) as f:
                out[i] = json.load(f)["per_sample"]
    return out


def pct(n, rate):
    return n * FS / (rate * 1e6) * 100.0


def main(argv):
    d = argv[1] if len(argv) > 1 else "build/cost/prof"
    s = load(d)
    if len(s) < len(NAMES):
        print(f"budget: run tools/costprobe/sy_dyn.sh first ({len(s)} of {len(NAMES)} scenarios in {d})")
        return 1
    hdr = "  ".join(f"{r:.0f}/us" for r in RATES)
    print(f"\nbudget: the target listing's instructions a sample (estimate), and one core's share at {hdr}")
    for i, nm in enumerate(NAMES):
        print(f"  {s[i]:7.0f}  " + "  ".join(f"{pct(s[i], r):5.0f} %" for r in RATES) + f"   {nm}")
    idle = s[0]
    inst = (s[2] - s[1]) / 15.0                       # a unison instance more (A x 1 -> A x 16)
    voice = (s[3] - s[0]) / 8.0                       # a voice: A x 1, no filter
    voice_b_lp = (s[4] - s[0]) / 8.0                  # a voice: A + B x 1, LP12
    fx = s[8] - s[1]                                  # every effect
    print(f"\nbudget: parts (instructions a sample): the engine idle {idle:.0f}; a voice (A x 1, no filter) "
          f"{voice:.0f}; a voice (A + B x 1, LP12) {voice_b_lp:.0f}; a unison instance more {inst:.0f}; "
          f"every effect {fx:.0f}")
    for rate in RATES:
        room = (PLAN_LOAD - SYSTEM) * rate * 1e6 / FS   # instructions a sample we may spend
        print(f"\nbudget at {rate:.0f}/us: {room:.0f} instructions a sample at {PLAN_LOAD:.0%} less {SYSTEM:.0%} system")
        for fxon in (False, True):
            left = room - idle - (fx if fxon else 0.0)
            row = []
            for u in (1, 2, 3, 4, 7):
                per = voice_b_lp + 2.0 * (u - 1) * inst
                row.append(f"x{u}: {max(0.0, left / per):4.1f}")
            print(f"  voices of A + B, LP12, {'every effect' if fxon else 'no effect   '}: " + "   ".join(row))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
