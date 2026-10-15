#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""STRATA factory wavetables: their definitions, and everything made from them.

Every factory table is original: made from the formulas below and from keyframes designed here, with no
third-party wave data. The FM-1 has no room for stored frames (one 16-frame table is 66 KiB of mips), so it
builds each table itself when it is loaded (firmware/src/sy_wtgen.c), from what this script writes:

  build/gen/strata_wt.h        the recipes (family, frames, four parameter sweeps), names, categories and the
                               spectral keyframes, as C tables
  build/gen/strata_wt_ref.bin  every table rendered by this script in double precision (int16 frames with
                               their nine mip levels, exactly the device's layout): tests/sy_wt_test.c holds the
                               C generator to it
  --docs DIR                   DIR/wavetables.png (every table, frames stacked) and DIR/wav/NAME.wav (a slow
                               sweep through each table at C3), for docs/WAVETABLES.md

The math is the C generator's, written once more here; docs/ARCHITECTURE.md §6 has the format:
  a frame is a single cycle given by its harmonics 1..256 (cosine and sine amplitudes A_k, B_k, no DC);
  level L keeps harmonics 1..256 >> L and is rendered at LEV_N[L] samples (4x oversampled at levels 0 and 1, 8x at
  2 and 3, 16x at 4 and 5, 32x above: the images of linear interpolation fall with the oversampling, and the
  small levels, where few harmonics make each one loud, can afford more), plus a guard sample (a copy of sample
  0); a table is normalised so its largest sample over all frames and levels is 0.95 of
  full scale (each frame is first stored at its own full scale, then scaled down by its peak / the table's).
Families either give the harmonics directly (spectral) or render the cycle at 2048 points, each the mean of 4
sub-samples, and take them from its FFT (time domain).

  python3 tools/gen_wavetables.py [OUTDIR] [--docs DIR]      (OUTDIR default build/gen)
"""
import math
import struct
import sys
from pathlib import Path

import numpy as np

NH = 256                                  # harmonics at level 0
NLEV = 9
LEV_N = [1024, 512, 512, 256, 256, 128, 128, 64, 32]               # 4x 4x 8x 8x 16x 16x 32x 32x 32x
LEV_OFF = [sum(n + 1 for n in LEV_N[:lv]) for lv in range(NLEV)]
FRAME = sum(n + 1 for n in LEV_N)                                  # 2121 int16
NT = 2048                                 # time-domain render: points a cycle
SUB = 4                                   # sub-samples a point (box filter: less fold-back of sharp edges)
PEAK = 0.95 * 32767.0
SR = 44117.647                            # the FM-1's rate (docs WAVs)

# families (sy_wtgen.c WTF_*)
ANALOG, PWM, ADDITIVE, BANDS, FORMANT, CHAOS, SYNC, FM, FOLD, PD, DIGITAL, KEYS = range(12)
TAPER = 1                                 # flag: the top octave of every level tapered to -12 dB: the bright tables
                                          # (flat spectra up to the top alias most; tests/sy_osc_test.c classes them)

CATS = ["ANALOG", "ADDITIV", "VOCAL", "SPECTRL", "SYNC", "FM", "FOLD", "PHASE", "DIGITAL"]

# typical adult vowel formants (textbook values): (Hz, bandwidth Hz, gain dB) x 5, for A E I O U
VOWELS = [
    [(600, 60, 0), (1040, 70, -7), (2250, 110, -9), (2450, 120, -9), (2750, 130, -20)],
    [(400, 40, 0), (1620, 80, -12), (2400, 100, -9), (2800, 120, -12), (3100, 120, -18)],
    [(250, 60, 0), (1750, 90, -30), (2600, 100, -16), (3050, 120, -22), (3340, 120, -28)],
    [(400, 40, 0), (750, 80, -11), (2400, 100, -21), (2600, 120, -20), (2900, 120, -40)],
    [(350, 40, 0), (600, 80, -20), (2400, 100, -32), (2675, 120, -28), (2950, 120, -36)],
]
F_REF = 110.0                             # the pitch the vowel tables are voiced for (A2)
BW_WIDE = 2.5                             # formant bandwidths widened: harmonics 110 Hz apart still find them

K = np.arange(1, NH + 1, dtype=np.float64)


def hash_phase(k, seed):
    """a fixed pseudo-random phase, 0..1023 (1/1024 turns), the same in C (sy_wtgen.c wg_hash): an integer hash of
    k + 256 seed (a multiplicative hash alone would give phases linear in k: only a delay)"""
    h = ((k + 256 * seed) * 0x9E3779B1) & 0xFFFFFFFF
    h ^= h >> 15
    h = (h * 0x85EBCA77) & 0xFFFFFFFF
    h ^= h >> 13
    return h >> 22


HASH = [[hash_phase(k, s) / 1024.0 for k in range(1, NH + 1)] for s in range(4)]


def xorshift(s):
    s ^= (s << 13) & 0xFFFFFFFF
    s ^= s >> 17
    s ^= (s << 5) & 0xFFFFFFFF
    return s


# ------------------------------------------------------------------------------------------- the recipes ---
class T:
    """a factory table: family, frames, options a0 a1, four parameter sweeps (start, end) 0..255"""

    def __init__(self, name, cat, family, frames=16, a0=0, a1=0, p0=(0, 0), p1=(0, 0), p2=(0, 0), p3=(0, 0),
                 flags=0, keys=None):
        self.name, self.cat, self.family, self.frames = name, CATS.index(cat), family, frames
        self.a0, self.a1, self.p, self.flags, self.keys = a0, a1, [p0, p1, p2, p3], flags, keys
        self.key = 0


def fixed(v):
    return (v, v)


def sweep(a, b):
    return (a, b)


# keyframe designs (KEYS family): each a list of (A, B) spectra, quantised to bytes on output
def kf_formant(vowel, tilt, seed, jitter):
    rng = np.random.default_rng(seed)
    f = K * F_REF
    env = np.zeros(NH)
    for (fr, bw, db) in VOWELS[vowel]:
        env += 10 ** (db / 20) / (1 + ((f - fr) / (bw * BW_WIDE / 2)) ** 2)
    a = env * K ** -tilt
    ph = rng.random(NH) * jitter
    return a, ph


def kf_drawbars(reg):
    # 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1' on harmonics 1 2 3 4 5 6 8; a drawbar step is 3 dB
    a = np.zeros(NH)
    for h, d in zip((1, 2, 3, 4, 5, 6, 8), reg):
        if d:
            a[h - 1] = 10 ** (-(8 - d) * 3 / 20)
    return a, np.array(HASH[1])


def kf_bell(strike):
    # partials near a bell's hum, prime, tierce, quint, nominal (on harmonics 2 4 5 6 8) and the strike's
    # upper partials, fading as the bell rings
    a = np.zeros(NH)
    for h, g in ((2, 1.0), (4, 0.8), (5, 0.6), (6, 0.5), (8, 0.7), (10, 0.4), (12, 0.35), (15, 0.3)):
        a[h - 1] = g * (1.0 if h <= 8 else strike)
    for h in range(16, 64, 3):
        a[h - 1] = 0.25 * strike * (16 / h)
    return a, np.array(HASH[2])


def kf_strings(bright):
    f = K * 196.0
    body = 1 + 2.5 / (1 + ((f - 275) / 60) ** 2) + 2 / (1 + ((f - 460) / 90) ** 2) + 1.5 / (1 + ((f - 700) / 150) ** 2)
    body += 1.2 / (1 + ((f - 3000) / 900) ** 2)
    a = body / K * np.exp(-K / (8 + 120 * bright))
    return a, np.array(HASH[3])


def kf_glass(n):
    a = np.zeros(NH)
    for i in range(1, 12):
        h = i * i if n == 0 else i * (i + 1) // 2 if n == 1 else 1 + 3 * (i - 1) if n == 2 else 2 ** (i - 1)
        if h <= NH:
            a[h - 1] = 1 / i
    return a, np.array(HASH[0])


def kf_hollow(c):
    a = np.where(K % 2 == 1, 1 / K, 0.0) * (1 + 3 * np.exp(-((np.log2(K) - c) ** 2) / 0.5))
    return a, np.zeros(NH)


TABLES = [
    # ANALOG: the basic shapes, their morphs and pulse widths (basses, leads, supersaws)
    T("BASIC MORPH", "ANALOG", ANALOG, p0=sweep(0, 255), p1=fixed(255), p2=fixed(200)),
    T("SAW", "ANALOG", ANALOG, p0=fixed(128), p1=sweep(80, 255)),
    T("SQUARE", "ANALOG", ANALOG, p0=fixed(191), p1=sweep(80, 255)),
    T("TRI TO SAW", "ANALOG", ANALOG, p0=sweep(64, 128), p1=fixed(255)),
    T("SAW TO SQR", "ANALOG", ANALOG, p0=sweep(128, 191), p1=fixed(255)),
    T("SINE TO TRI", "ANALOG", ANALOG, p0=sweep(0, 64), p1=fixed(255)),
    T("PULSE WIDTH", "ANALOG", PWM, p0=sweep(0, 250), p1=fixed(255), flags=TAPER),
    T("SOFT PULSE", "ANALOG", PWM, p0=sweep(0, 220), p1=fixed(150)),
    T("DARK SAW", "ANALOG", ANALOG, p0=fixed(128), p1=sweep(0, 150)),
    # ADDITIVE: harmonic series with tilt, odd / even balance and comb notches (organs, keys, pads)
    T("ODD TILT", "ADDITIV", ADDITIVE, p0=sweep(255, 40), p1=fixed(0), flags=TAPER),
    T("ALL TILT", "ADDITIV", ADDITIVE, p0=sweep(255, 40), p1=fixed(128), flags=TAPER),
    T("ODD TO EVEN", "ADDITIV", ADDITIVE, p0=fixed(90), p1=sweep(0, 255)),
    T("COMB SWEEP", "ADDITIV", ADDITIVE, p0=fixed(60), p1=fixed(128), p2=fixed(255), p3=sweep(0, 255), flags=TAPER),
    T("COMB DEPTH", "ADDITIV", ADDITIVE, a1=1, p0=fixed(40), p1=fixed(128), p2=sweep(0, 255), p3=fixed(60), flags=TAPER),
    T("PHASED SAW", "ADDITIV", ADDITIVE, a1=2, p0=sweep(160, 40), p1=fixed(128), flags=TAPER),
    # SPECTRAL: bands of harmonics, seeded spectra and designed keyframes (pads, textures)
    T("BAND SWEEP", "SPECTRL", BANDS, a1=2, p0=sweep(0, 200), p1=fixed(40), p2=fixed(180), flags=TAPER),
    T("TWIN BANDS", "SPECTRL", BANDS, a1=2, a0=1, p0=sweep(40, 180), p1=fixed(30), p2=fixed(120), p3=fixed(160), flags=TAPER),
    T("WIDE BAND", "SPECTRL", BANDS, a1=2, a0=2, p0=sweep(40, 160), p1=fixed(160), p2=fixed(255), flags=TAPER),
    T("BAND SPREAD", "SPECTRL", BANDS, a1=2, a0=3, p0=fixed(130), p1=fixed(50), p2=fixed(200), p3=sweep(0, 255), flags=TAPER),
    T("CHAOS A", "SPECTRL", CHAOS, a0=1, p0=sweep(0, 255), p1=fixed(100), p2=fixed(200)),
    T("CHAOS B", "SPECTRL", CHAOS, a0=2, p0=sweep(0, 255), p1=fixed(40), p2=fixed(255)),
    T("DUST", "SPECTRL", CHAOS, a0=3, p0=sweep(0, 255), p1=fixed(30), p2=sweep(20, 90), flags=TAPER),
    T("DRAWBARS", "SPECTRL", KEYS, keys=[kf_drawbars(r) for r in
                                         ((8, 8, 8, 0, 0, 0, 0), (8, 6, 8, 4, 0, 0, 0), (8, 8, 8, 8, 6, 4, 4),
                                          (6, 8, 4, 8, 8, 6, 8))]),
    T("BELLS", "SPECTRL", KEYS, keys=[kf_bell(s) for s in (1.0, 0.6, 0.3, 0.0)]),
    T("GLASS", "SPECTRL", KEYS, keys=[kf_glass(n) for n in range(4)], flags=TAPER),
    T("STRINGS", "SPECTRL", KEYS, keys=[kf_strings(b) for b in (0.0, 0.3, 0.7, 1.0)]),
    T("HOLLOW", "SPECTRL", KEYS, keys=[kf_hollow(c) for c in (1.0, 2.5, 4.0, 5.5)], flags=TAPER),
    # VOCAL: formant envelopes over a buzz, A E I O U (talking basses, choirs)
    T("VOWELS", "VOCAL", FORMANT, p0=sweep(0, 255), p1=fixed(128), p2=fixed(160)),
    T("VOWEL SHIFT", "VOCAL", FORMANT, p0=fixed(0), p1=sweep(0, 255), p2=fixed(160)),
    T("DARK VOWELS", "VOCAL", FORMANT, p0=sweep(0, 255), p1=fixed(90), p2=fixed(40)),
    T("BRIGHT A-I", "VOCAL", FORMANT, p0=sweep(0, 128), p1=fixed(170), p2=fixed(230)),
    T("CHOIR", "VOCAL", KEYS, keys=[kf_formant(v, 1.2, 11 + v, 0.15) for v in (4, 3, 0, 1)]),
    # SYNC: hard sync, the slave's ratio swept 1 .. 16 (sync leads)
    T("SYNC SAW", "SYNC", SYNC, a0=0, p0=sweep(0, 255), flags=TAPER),
    T("SYNC SQUARE", "SYNC", SYNC, a0=1, p0=sweep(0, 255), flags=TAPER),
    T("SYNC SINE", "SYNC", SYNC, a0=2, p0=sweep(0, 255)),
    T("SYNC TRI", "SYNC", SYNC, a0=3, p0=sweep(0, 255)),
    # FM: one modulator, the index swept (keys, bells, basses)
    T("FM 1:1", "FM", FM, a0=1, a1=1, p0=sweep(0, 255)),
    T("FM 1:2", "FM", FM, a0=1, a1=2, p0=sweep(0, 255)),
    T("FM 1:3", "FM", FM, a0=1, a1=3, p0=sweep(0, 255), flags=TAPER),
    T("FM 2:7", "FM", FM, a0=2, a1=7, p0=sweep(0, 200), flags=TAPER),
    T("FM 1:5 BELL", "FM", FM, a0=1, a1=5, p0=sweep(0, 160), flags=TAPER),
    # FOLD: wavefolders driven harder across the table (aggressive basses and leads)
    T("FOLD SINE", "FOLD", FOLD, a0=0, a1=0, p0=sweep(0, 255)),
    T("FOLD TRI", "FOLD", FOLD, a0=1, a1=1, p0=sweep(0, 255), flags=TAPER),
    T("FOLD ASYM", "FOLD", FOLD, a0=0, a1=0, p0=fixed(90), p1=sweep(0, 255)),
    T("FOLD MIX", "FOLD", FOLD, a0=0, a1=1, p0=sweep(0, 200), p1=fixed(60), flags=TAPER),
    # PHASE: phase distortion, bent cosines and resonant windows (plucks, keys, basses)
    T("PD SAW", "PHASE", PD, a0=0, p0=sweep(0, 255)),
    T("PD SQUARE", "PHASE", PD, a0=1, p0=sweep(0, 255)),
    T("RESO SAW", "PHASE", PD, a0=2, p0=sweep(0, 255), flags=TAPER),
    T("RESO TRI", "PHASE", PD, a0=3, p0=sweep(0, 255), flags=TAPER),
    T("RESO TRAP", "PHASE", PD, a0=4, p0=sweep(0, 255), flags=TAPER),
    # DIGITAL: amplitude and time quantised (lo-fi, arps)
    T("CRUSH SINE", "DIGITAL", DIGITAL, a0=0, p0=sweep(0, 255), flags=TAPER),
    T("CRUSH SAW", "DIGITAL", DIGITAL, a0=1, p0=sweep(0, 230), flags=TAPER),
    T("STEPS", "DIGITAL", DIGITAL, a0=1, p1=sweep(1, 255), flags=TAPER),
    T("STAIR TRI", "DIGITAL", DIGITAL, a0=2, p0=fixed(150), p1=sweep(1, 255), flags=TAPER),
]


# --------------------------------------------------------------------------------------------- the math ---
def params(t, f):
    """the four parameters of frame f, 0..1"""
    u = f / (t.frames - 1) if t.frames > 1 else 0.0
    return [(a + (b - a) * u) / 255.0 for (a, b) in t.p]


def shape_spectrum(i, w):
    """the basic shapes' harmonics (A, B): 0 sine, 1 triangle, 2 saw (falling), 3 square, 4 pulse of width w"""
    A, B = np.zeros(NH), np.zeros(NH)
    odd = K % 2 == 1
    if i == 0:
        B[0] = 1.0
    elif i == 1:
        B = np.where(odd, 8 / (math.pi ** 2 * K ** 2) * np.where(((K - 1) / 2) % 2 == 0, 1.0, -1.0), 0.0)
    elif i == 2:
        B = 2 / (math.pi * K)
    elif i == 3:
        B = np.where(odd, 4 / (math.pi * K), 0.0)
    else:
        A = 2 / (math.pi * K) * np.sin(2 * math.pi * K * w)
        B = 2 / (math.pi * K) * (1 - np.cos(2 * math.pi * K * w))
    return A, B


def bright(v):
    kc = 2.0 ** (1 + 8 * v)
    return 1 / np.sqrt(1 + (K / kc) ** 4)


def phases(a, mode, seed):
    """amplitudes -> (A, B) with the phase mode: 0 sine, 1 alternating sign, 2 hashed"""
    if mode == 0:
        return np.zeros(NH), a
    if mode == 1:
        return np.zeros(NH), a * np.where(K % 2 == 1, 1.0, -1.0)
    ph = np.array(HASH[seed & 3]) if seed < 4 else np.array([hash_phase(k, seed) / 1024 for k in range(1, NH + 1)])
    return a * np.sin(2 * math.pi * ph), a * np.cos(2 * math.pi * ph)


def spectral(t, f):
    v = params(t, f)
    if t.family == ANALOG:
        s = 4 * v[0]
        i = min(int(math.floor(s)), 3)
        x = s - i
        A0, B0 = shape_spectrum(i, 0.5 - 0.48 * v[2])
        A1, B1 = shape_spectrum(i + 1, 0.5 - 0.48 * v[2])
        g = bright(v[1])
        return ((1 - x) * A0 + x * A1) * g, ((1 - x) * B0 + x * B1) * g
    if t.family == PWM:
        A, B = shape_spectrum(4, 0.5 - 0.48 * v[0])
        g = bright(v[1])
        return A * g, B * g
    if t.family == ADDITIVE:
        a = K ** -(0.5 + 2 * v[0])
        godd, geven = min(1.0, 2 * (1 - v[1])), min(1.0, 2 * v[1])
        a = a * np.where(K % 2 == 1, godd, geven)
        a[0] = 1.0
        a = a * (1 - v[2] * 0.5 * (1 + np.cos(2 * math.pi * K / (2 + 14 * v[3]))))
        return phases(a, t.a1, t.a0)
    if t.family == BANDS:
        c, sg, d = 8 * v[0], 0.15 + 1.35 * v[1], 2 * v[3]
        lk = np.log2(K)
        a = np.exp(-(lk - (c - d / 2)) ** 2 / (2 * sg * sg)) + np.exp(-(lk - (c + d / 2)) ** 2 / (2 * sg * sg))
        a[0] += v[2]
        return phases(a, t.a1, t.a0)
    if t.family == FORMANT:
        p = 4 * v[0]
        i = min(int(math.floor(p)), 3)
        x = p - i
        sh = 2.0 ** (2 * v[1] - 1)
        env = np.zeros(NH)
        for j in range(5):
            (f0, b0, d0), (f1, b1, d1) = VOWELS[i][j], VOWELS[i + 1][j]
            fr = f0 * (f1 / f0) ** x * sh
            bw = (b0 + (b1 - b0) * x) * sh * BW_WIDE
            g = 10 ** ((d0 + (d1 - d0) * x) / 20)
            env += g / (1 + ((K * F_REF - fr) / (bw / 2)) ** 2)
        return phases(env * K ** -(2 - v[2]), 0, 0)
    if t.family == CHAOS:
        u = 3 * v[0]
        j = min(int(math.floor(u)), 2)
        x = u - j
        dens = 0.1 + 0.9 * v[2]
        beta = 0.5 + 2 * v[1]
        mags, phs = [], []
        for kf in (j, j + 1):
            s = ((t.a0 + 1) << 16) ^ ((kf * 0x9E3779B9) & 0xFFFFFFFF) ^ 0x1234567
            m, p = np.zeros(NH), np.zeros(NH)
            for k in range(NH):
                s = xorshift(s)
                r1 = s
                s = xorshift(s)
                r2 = s
                s = xorshift(s)
                r3 = s
                if (r1 >> 8) / 16777216.0 < dens or k == 0:
                    m[k] = (k + 1) ** -beta * (0.25 + 0.75 * (r2 >> 8) / 16777216.0)
                p[k] = (r3 >> 22) / 1024.0
            mags.append(m)
            phs.append(p)
        a = (1 - x) * mags[0] + x * mags[1]
        d = phs[1] - phs[0]
        d -= np.floor(d + 0.5)                                     # the shorter way round
        ph = phs[0] + d * x
        return a * np.sin(2 * math.pi * ph), a * np.cos(2 * math.pi * ph)
    if t.family == KEYS:
        kb = t.keybytes
        nk = len(kb)
        u = (f / (t.frames - 1) if t.frames > 1 else 0.0) * (nk - 1)
        j = min(int(math.floor(u)), nk - 2)
        x = u - j
        m0, p0 = kb[j]
        m1, p1 = kb[j + 1]
        m = m0 + (m1.astype(np.float64) - m0) * x
        a = np.where(m > 0, 2.0 ** (-(255 - m) * 0.0830482), 0.0)
        d = (p1.astype(np.int64) - p0.astype(np.int64) + 128) % 256 - 128
        ph = (p0 + d * x) / 256.0
        return a * np.sin(2 * math.pi * ph), a * np.cos(2 * math.pi * ph)
    raise ValueError(t.family)


def tri_u(u):
    """the sine-phase triangle of a phase in turns: 0 at 0, +1 at 1/4, -1 at 3/4"""
    return 4 * np.abs(np.mod(u + 0.75, 1.0) - 0.5) - 1


def fold_tri(y):
    m = (y + 1) - 4 * np.floor((y + 1) / 4)
    return 1 - np.abs(m - 2)


def timedomain(t, f):
    """the cycle at NT points, each the mean of SUB sub-samples; integer phases where an edge depends on it"""
    v = params(t, f)
    s = np.arange(NT * SUB, dtype=np.int64)
    x = s / (NT * SUB)
    if t.family == SYNC:
        rq = int(2.0 ** (4 * v[0]) * 65536 + 0.5)
        ph = ((s << 19) * rq >> 16) & 0xFFFFFFFF
        xr = ph / 4294967296.0
        y = [1 - 2 * xr, np.where(ph < 0x80000000, 1.0, -1.0), np.sin(2 * math.pi * xr),
             4 * np.abs(((ph + 0xC0000000) & 0xFFFFFFFF) / 4294967296.0 - 0.5) - 1][t.a0]
    elif t.family == FM:
        c = ((t.a0 * s) % (NT * SUB)) / (NT * SUB)
        q = ((t.a1 * s) % (NT * SUB)) / (NT * SUB)
        y = np.sin(2 * math.pi * (c + (8 * v[0] / (2 * math.pi)) * np.sin(2 * math.pi * q)))
    elif t.family == FOLD:
        base = np.sin(2 * math.pi * x) if t.a0 == 0 else tri_u(x)
        z = (1 + 9 * v[0]) * base + v[1]
        y = np.sin(2 * math.pi * z / 4) if t.a1 == 0 else fold_tri(z)
    elif t.family == PD:
        if t.a0 == 0:
            d = 0.5 - 0.49 * v[0]
            phi = np.where(x < d, x / (2 * d), 0.5 + (x - d) / (2 * (1 - d)))
            y = -np.cos(2 * math.pi * phi)
        elif t.a0 == 1:
            w = 1 - 0.98 * v[0]
            h = np.minimum(1.0, np.mod(2 * x, 1.0) / w)
            y = -np.cos(2 * math.pi * (np.floor(2 * x) + h) / 2)
        else:
            r = 1 + 15 * v[0]
            win = [1 - x, 1 - np.abs(2 * x - 1), np.minimum(1.0, 2 * (1 - x))][t.a0 - 2]
            y = win * np.cos(2 * math.pi * r * x)
    elif t.family == DIGITAL:
        if v[1] > 0.5 / 255:
            st = 4 + int(60 * (1 - v[1]))
            xq = ((s * st) >> 13) / st
        else:
            xq = x
        base = [np.sin(2 * math.pi * xq), 1 - 2 * xq, tri_u(xq)][t.a0]
        half = 2.0 ** (1 + 7 * (1 - v[0])) / 2
        y = np.floor(base * half + 0.5) / half
    else:
        raise ValueError(t.family)
    pts = y.reshape(NT, SUB).mean(axis=1)
    X = np.fft.rfft(pts)
    return 2 * X.real[1:NH + 1] / NT, -2 * X.imag[1:NH + 1] / NT


def frame_levels(t, f):
    """one frame's nine levels, float, in the device's layout (guard samples included)"""
    A, B = (spectral if t.family in (ANALOG, PWM, ADDITIVE, BANDS, FORMANT, CHAOS, KEYS) else timedomain)(t, f)
    out = np.zeros(FRAME)
    for lv in range(NLEV):
        h, n = NH >> lv, LEV_N[lv]
        g = np.ones(h)
        if t.flags & TAPER and h >= 4:
            k = np.arange(1, h + 1)
            g = np.where(k > h // 2, 1 - 0.75 * ((k - h // 2) / (h // 2)) ** 2, 1.0)
        X = np.zeros(n // 2 + 1, dtype=np.complex128)
        X[1:h + 1] = (A[:h] - 1j * B[:h]) * g * (n / 2)
        y = np.fft.irfft(X, n)
        out[LEV_OFF[lv]:LEV_OFF[lv] + n] = y
        out[LEV_OFF[lv] + n] = y[0]
    return out


def rnd(v):
    return np.sign(v) * np.floor(np.abs(v) + 0.5)


def render(t):
    """the table as the device stores it: frames x FRAME int16"""
    fl = [frame_levels(t, f) for f in range(t.frames)]
    pk = [np.max(np.abs(x)) for x in fl]
    top = max(pk)
    q = []
    for x, p in zip(fl, pk):
        a = rnd(x * (PEAK / p)) if p > 0 else np.zeros(FRAME)
        q.append(rnd(a * (p / top)) if top > 0 else a)
    return np.array(q, dtype=np.int16)


# ----------------------------------------------------------------------------------------------- output ---
def keybytes(keys):
    """(A-amplitudes, phase turns) designs -> (magnitude, phase) bytes: 0.5 dB steps under the loudest, 1/256 turn"""
    top = max(np.max(a) for a, _ in keys)
    kb = []
    for a, ph in keys:
        db = 20 * np.log10(np.maximum(a / top, 1e-12))
        m = np.where(a > 0, np.clip(np.floor(255 + db * 2 + 0.5), 0, 255), 0).astype(np.uint8)
        p = (np.floor(np.mod(ph, 1.0) * 256 + 0.5).astype(np.int64) % 256).astype(np.uint8)
        kb.append((m, p))
    return kb


def c_array(name, ctype, vals, per=20):
    out = [f"static const {ctype} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(str(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return out


def main(argv):
    outdir = Path(argv[1]) if len(argv) > 1 and not argv[1].startswith("--") else Path("build/gen")
    docs = Path(argv[argv.index("--docs") + 1]) if "--docs" in argv else None
    outdir.mkdir(parents=True, exist_ok=True)
    keyblob = []
    for t in TABLES:
        assert len(t.name) <= 11 and 1 <= t.frames <= 16, t.name
        if t.family == KEYS:
            t.keybytes = keybytes(t.keys)
            t.key = len(keyblob)
            t.a0 = len(t.keys)
            for m, p in t.keybytes:
                keyblob += list(m) + list(p)
    assert len(TABLES) <= 64 and len(keyblob) < 65536
    L = ["/* generated by tools/gen_wavetables.py: the factory wavetables (docs/WAVETABLES.md). Do not edit. */",
         f"#define WT_NFACT {len(TABLES)}u", f"#define WT_NCAT {len(CATS)}u", ""]
    L.append("static const wt_recipe_t WT_RECIPES[WT_NFACT] = {   /* family frames a0 a1 {p0..p3} flags cat key */")
    for t in TABLES:
        ps = ", ".join(f"{{{a}, {b}}}" for a, b in t.p)
        L.append(f"    {{{t.family}, {t.frames}, {t.a0}, {t.a1}, {{{ps}}}, {t.flags}, {t.cat}, {t.key}}},"
                 f"   /* {t.name} */")
    L.append("};")
    L.append("static const char WT_NAMES[WT_NFACT][12] = {")
    L += [f'    "{t.name}",' for t in TABLES]
    L.append("};")
    L.append("static const char WT_CATNAMES[WT_NCAT][8] = {" + ", ".join(f'"{c}"' for c in CATS) + "};")
    L += c_array("WT_KEYDATA", "uint8_t", keyblob or [0])
    (outdir / "strata_wt.h").write_text("\n".join(L) + "\n")
    # the reference: "SYWT", count, frame size, then per table: frames (u16), frames x FRAME int16
    with open(outdir / "strata_wt_ref.bin", "wb") as fo:
        fo.write(struct.pack("<4sHH", b"SYWT", len(TABLES), FRAME))
        for t in TABLES:
            r = render(t)
            fo.write(struct.pack("<H", t.frames))
            fo.write(r.astype("<i2").tobytes())
            t.ref = r
    print(f"gen_wavetables: {len(TABLES)} tables, {len(keyblob)} keyframe bytes -> {outdir}")
    if docs:
        write_docs(docs)


def write_docs(d):
    from PIL import Image, ImageDraw
    d.mkdir(parents=True, exist_ok=True)
    (d / "wav").mkdir(exist_ok=True)
    cw, ch, cols = 240, 150, 6
    rows = (len(TABLES) + cols - 1) // cols
    img = Image.new("RGB", (cols * cw, rows * ch), (18, 18, 22))
    dr = ImageDraw.Draw(img)
    for i, t in enumerate(TABLES):
        x0, y0 = (i % cols) * cw, (i // cols) * ch
        lv0 = t.ref[:, :LEV_N[0]].astype(np.float64) / 32767.0
        for f in range(t.frames):
            dx, dy = f * 4, -f * 5 + 40
            col = (60 + 12 * f, 140 + 6 * f, 255 - 8 * f)
            pts = [(x0 + 10 + dx + j * 150 / LEV_N[0], y0 + 70 + dy - lv0[f, j] * 28) for j in range(0, LEV_N[0], 4)]
            dr.line(pts, fill=col, width=1)
        dr.text((x0 + 8, y0 + ch - 16), f"{i:2d} {t.name}", fill=(230, 230, 230))
        # a slow sweep through the table at C3, 2 s, from level 1 (128 harmonics: 16.7 kHz at C3), linear
        lv1 = t.ref[:, LEV_OFF[1]:LEV_OFF[1] + LEV_N[1] + 1].astype(np.float64) / 32767.0
        n, f0 = int(2 * SR), 130.81
        ph = np.cumsum(np.full(n, f0 / SR)) % 1.0
        pos = np.linspace(0, t.frames - 1, n)
        k0 = np.minimum(pos.astype(int), t.frames - 1)
        k1 = np.minimum(k0 + 1, t.frames - 1)
        xf = pos - k0
        j = ph * LEV_N[1]
        i0 = j.astype(int)
        fr = j - i0
        a = lv1[k0, i0] + (lv1[k0, i0 + 1] - lv1[k0, i0]) * fr
        b = lv1[k1, i0] + (lv1[k1, i0 + 1] - lv1[k1, i0]) * fr
        y = a + (b - a) * xf
        with open(d / "wav" / (t.name.replace(" ", "_").replace(":", "-") + ".wav"), "wb") as fo:
            data = (np.clip(y, -1, 1) * 30000).astype("<i2").tobytes()
            fo.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
                     struct.pack("<IHHIIHH", 16, 1, 1, 44118, 44118 * 2, 2, 16) + b"data" + struct.pack("<I", len(data)))
            fo.write(data)
    img.save(d / "wavetables.png")


if __name__ == "__main__":
    main(sys.argv)
