#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Phase 0 CPU budget for the FM-1: voices x oscillators x unison against one audio half.

Inputs are per-sample instruction counts from the target listing (tools/costprobe/run.sh,
JieLi clang -O2, pi32v2 -mcpu=r3 -mfprev1) and two conversion rates:
  INT_RATE   205 instructions / us: measured on an FM-1 by ChoralRoot (docs/CZ1.md, listing count
             of integer code over the unit's measured time)
  FLOAT_RATE 150 instructions / us: an assumption for FPU-heavy code (no device figure yet;
             the on-device BENCH page will replace it)
Everything here is an estimate until the BENCH page runs on a unit."""

HALF_US = 128 / 44117.6 * 1e6          # one I2S half: 128 frames = 2901 us
PLAN_LOAD = 0.75                       # worst case we plan for (Felucca sheds at 85 %)
SYSTEM = 0.08                          # audio ISR frame work, TIMER5 key scan, USB (ChoralRoot: 4-6 %)
INT_RATE, FLOAT_RATE = 205.0, 150.0

# per-sample listing instructions (as compiled today) and which rate applies
OSC = (37, INT_RATE)                   # one wavetable oscillator instance (one unison voice), stereo out
WARP = (10, INT_RATE)                  # phase-domain warp on that instance (estimate)
VOICE = {                              # per voice, once
    "sub": (20, INT_RATE),
    "noise": (12, INT_RATE),
    "filter svf stereo": (42, FLOAT_RATE),
    "amp + mix stereo": (13, FLOAT_RATE),
    "control rate (env, lfo, matrix, pitch)": (40, INT_RATE),
}
FX = {
    "drive": (22, FLOAT_RATE),
    "chorus": (48, FLOAT_RATE),
    "delay (ping-pong)": (76, FLOAT_RATE),
    "reverb (FDN 4)": (176, FLOAT_RATE),
    "OTT 3-band": (170, FLOAT_RATE),
    "master (dc, limiter)": (30, FLOAT_RATE),
}


def us_per_half(items):
    return sum(n / rate for n, rate in items) * 128


def main():
    budget = HALF_US * (PLAN_LOAD - SYSTEM)
    fx = us_per_half(FX.values())
    voice_fixed = us_per_half(VOICE.values())
    osc = us_per_half([OSC])
    osc_w = us_per_half([OSC, WARP])
    print(f"half {HALF_US:.0f} us; planned {PLAN_LOAD:.0%} - system {SYSTEM:.0%} = {budget:.0f} us for voices + FX")
    print(f"FX rack, all on: {fx:.0f} us ({fx / HALF_US:.1%}); per voice fixed {voice_fixed:.1f} us; "
          f"per osc instance {osc:.1f} us ({osc_w:.1f} with a warp)")
    for name, items in (("all FX", fx), ("FX without reverb/OTT", fx - us_per_half([FX['reverb (FDN 4)'], FX['OTT 3-band']]))):
        left = budget - items
        print(f"\n{name}: {left:.0f} us for voices")
        print("  unison/osc  2 osc: voices   (with warp on both)")
        for u in (1, 2, 3, 4, 5, 6, 7, 8, 16):
            per = voice_fixed + 2 * u * osc
            per_w = voice_fixed + 2 * u * osc_w
            print(f"  {u:9d}  {left / per:12.1f}   ({left / per_w:.1f})   {per:6.1f} us a voice")
    # one voice of everything: what a single mono lead costs at 16 unison
    lead = voice_fixed + 2 * 16 * osc_w
    print(f"\nmono lead, 2 osc x 16 unison + warps: {lead:.0f} us ({lead / HALF_US:.1%} of a half)")


if __name__ == "__main__":
    main()
