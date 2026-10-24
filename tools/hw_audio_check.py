#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Check the hardware test's recordings of the FM-1's USB audio (V0.87.1, test.html, serve.py: build/hwtest/<date-time>/).

  python tools/hw_audio_check.py RUN_DIR [--ref build/tproj] [--click-k 10]
  python tools/hw_audio_check.py --selftest

For each tpN.wav in RUN_DIR (16-bit PCM, at the rate the browser reported, 44.1 or 48 kHz) it prints a row:

  peak, rms    dBFS over the whole file
  clip         runs of 2 or more samples at full scale (and the samples in them)
  clicks/min   discontinuities: the second difference |x[n+1] - 2 x[n] + x[n-1]| beyond a robust local threshold,
               max(K * the median of the second difference in the surrounding 3 blocks of 1024 samples, 0.01 of full
               scale); events within 32 samples are one; those at the edge of a gap or repeat are not counted again.
               Whatever the content, a count means something only against the emulator's render (--ref).
  gaps         zero runs of 24 or more frames (both channels exactly 0) with sound on both sides (above -40 dBFS within
               10 ms): a dropout. Silence at the start, the end or between quiet parts is not a gap.
  repeats      runs of 4 or more identical, non-zero frames: a UAC underrun repeats the last frame
  dur / exp    the file's length, and what the run says it should be (the page's clock from the command to "none",
               plus the tail it records); drift is the file's frames against the page's wall clock at the stop (the audio
               clock against the computer's: a few hundred ppm is the browser's latency, percent is a lost or added rate)

With --ref DIR it reads DIR/tpN.wav (what web/emu/strata_tproj.c renders, at 44,118 Hz) and prints, for the same
statistics, the recording against the render: level and peak difference, the click and gap rates, the spectrum in octave
bands, and the length of the sound (first to last audible sample). Statistics only, never samples: the two never line
up to the sample (a different rate, the start's lead, the USB clock).

The rate: 44.1 or 48 kHz is the USB's; the device's own is 44,118 Hz, so a recording's length is the render's within
0.04 % at most if the FM-1 rate-matches the USB, and 0.04 % longer if it labels its frames 44,100 (BENCH's
`uac_rate_hz` and `uac_hz` in the JSON say which). Every figure here is per second, so neither rate changes it.
Stdlib plus numpy."""
import argparse
import json
import math
import sys
import tempfile
import wave
from pathlib import Path

import numpy as np

CLICK_K, CLICK_FLOOR, CLICK_BLOCK, CLICK_MERGE = 10.0, 0.01, 1024, 32
GAP_MIN, GAP_CTX_N, GAP_CTX_LEVEL = 24, 480, 0.01
REP_MIN, REP_LEVEL = 4, 0.003
CLIP_LEVEL = 32767.0 / 32768.0 - 1e-6
BANDS = [(0, 100), (100, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]


def read_wav(path):
    """(samples float32 [n, ch] in -1..1, rate)"""
    with wave.open(str(path), "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width != 2:
        raise ValueError(f"{path}: {8 * width}-bit (16-bit PCM only)")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    return x[: (len(x) // ch) * ch].reshape(-1, ch), rate


def write_wav(path, x, rate):
    q = np.clip(np.round(x * 32768.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(q.shape[1])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(q.tobytes())


def db(v):
    return 20.0 * math.log10(v) if v > 1e-9 else -180.0


def runs(mask):
    """(start, end) of each run of True in a boolean array, end exclusive"""
    if not mask.size:
        return np.zeros((0, 2), dtype=np.int64)
    d = np.diff(np.concatenate(([0], mask.astype(np.int8), [0])))
    return np.stack((np.flatnonzero(d == 1), np.flatnonzero(d == -1)), axis=1)


def clipped(x):
    r = runs((np.abs(x) >= CLIP_LEVEL).any(axis=1))
    long = [(a, b) for a, b in r if b - a >= 2]
    return len(long), int(sum(b - a for a, b in long))


def gaps(x):
    """dropouts: [(start, length)] zero runs with sound either side"""
    out = []
    mag = np.abs(x).max(axis=1)
    for a, b in runs(~x.any(axis=1)):
        if b - a < GAP_MIN:
            continue
        before, after = mag[max(0, a - GAP_CTX_N):a], mag[b:b + GAP_CTX_N]
        if before.size and after.size and before.max() > GAP_CTX_LEVEL and after.max() > GAP_CTX_LEVEL:
            out.append((int(a), int(b - a)))
    return out


def repeats(x):
    """[(start, repeated frames)]: identical, audible frames in a row"""
    same = (x[1:] == x[:-1]).all(axis=1) & (np.abs(x[1:]).max(axis=1) > REP_LEVEL) & (np.abs(x[1:]).max(axis=1) < CLIP_LEVEL)   # (a clipped top is flat: it is clipping)
    return [(int(a), int(b - a)) for a, b in runs(same) if b - a >= REP_MIN]


def clicks(x, k=CLICK_K, skip=()):
    """sample indices of discontinuities (one per cluster), not those within 3 samples of a (start, length) in skip"""
    found = []
    for c in range(x.shape[1]):
        a = np.abs(x[2:, c] - 2.0 * x[1:-1, c] + x[:-2, c])
        nb = len(a) // CLICK_BLOCK + 1
        pad = np.zeros(nb * CLICK_BLOCK, dtype=np.float32)
        pad[: len(a)] = a
        med = np.median(pad.reshape(nb, CLICK_BLOCK), axis=1)
        loc = np.maximum(np.maximum(np.roll(med, 1), med), np.roll(med, -1))
        loc[0], loc[-1] = max(med[0], med[min(1, nb - 1)]), max(med[-1], med[max(nb - 2, 0)])
        thr = np.maximum(k * np.repeat(loc, CLICK_BLOCK)[: len(a)], CLICK_FLOOR)
        found.extend((np.flatnonzero(a > thr) + 1).tolist())
    found.sort()
    out = []
    for i in found:
        if not out or i - out[-1] > CLICK_MERGE:
            out.append(i)
    for s, n in skip:
        out = [i for i in out if not (s - 3 <= i <= s + n + 3)]
    return out


def active_span(x, rate):
    """seconds from the first to the last 10 ms block above -60 dBFS rms"""
    m = x.mean(axis=1) if x.shape[1] > 1 else x[:, 0]
    n = max(1, rate // 100)
    nb = len(m) // n
    if nb == 0:
        return 0.0
    r = np.sqrt((m[: nb * n].reshape(nb, n) ** 2).mean(axis=1))
    on = np.flatnonzero(r > 1e-3)
    return 0.0 if not on.size else (on[-1] - on[0] + 1) * n / rate


def bands(x, rate):
    """mean power per octave band, dB, from 4096-point hann frames of the mono mix"""
    m = x.mean(axis=1) if x.shape[1] > 1 else x[:, 0]
    n = 4096
    if len(m) < n:
        return [-180.0] * len(BANDS), 0.0
    win = np.hanning(n).astype(np.float32)
    idx = np.arange(0, len(m) - n, n // 2)
    if len(idx) > 4000:
        idx = idx[:: len(idx) // 4000 + 1]
    p = np.zeros(n // 2 + 1)
    for i in idx:
        p += np.abs(np.fft.rfft(m[i:i + n] * win)) ** 2
    p /= len(idx)
    f = np.fft.rfftfreq(n, 1.0 / rate)
    out = [10.0 * math.log10(p[(f >= lo) & (f < hi)].sum() + 1e-18) for lo, hi in BANDS]
    return out, float((f * p).sum() / max(p.sum(), 1e-18))


def stats(x, rate, k=CLICK_K):
    dur = len(x) / rate
    g, r = gaps(x), repeats(x)
    skip = [(s, n) for s, n in g] + [(s, n) for s, n in r]
    c = clicks(x, k, skip)
    nclip, sclip = clipped(x)
    b, cen = bands(x, rate)
    return {"dur": dur, "peak": db(float(np.abs(x).max()) if x.size else 0.0),
            "rms": db(float(np.sqrt((x.astype(np.float64) ** 2).mean())) if x.size else 0.0),
            "clip_runs": nclip, "clip_samples": sclip, "clicks": c, "cpm": len(c) / dur * 60.0 if dur else 0.0,
            "gaps": g, "repeats": r, "span": active_span(x, rate), "bands": b, "centroid": cen}


def expected(fig):
    """what the run says the file's length should be, or None: the page's clock from the command to 'none', plus
    the 1.5 s it records after (and the 0.3 s it waits to flush)"""
    try:
        return (fig["none_at_frame"] - fig["sent_at_frame"]) / fig["rate"] + 1.5 + 0.3
    except (KeyError, TypeError, ZeroDivisionError):
        return None


def drift_ppm(fig):
    try:
        return (fig["frames"] / fig["wall_frames"] - 1.0) * 1e6
    except (KeyError, TypeError, ZeroDivisionError):
        return None


def where(items, rate, n=3):
    return ", ".join(f"{s / rate:.2f}s" + (f" x{ln}" if ln else "") for s, ln in items[:n]) + (" ..." if len(items) > n else "")


def check_file(wav, ref_dir, k, out):
    n = wav.stem
    try:
        x, rate = read_wav(wav)
    except (OSError, ValueError, wave.Error, EOFError) as e:
        out.append(f"{n}: cannot read ({e})")
        return None
    fig = {}
    jf = wav.with_suffix(".json")
    if jf.is_file():
        try:
            fig = json.loads(jf.read_text(encoding="utf-8"))
        except ValueError:
            fig = {}
    s = stats(x, rate, k)
    s["name"], s["rate"], s["exp"], s["drift"], s["fig"] = n, rate, expected(fig), drift_ppm(fig), fig
    ref = None
    if ref_dir:
        rw = Path(ref_dir) / wav.name
        if not rw.is_file():                                  # (the renders' own names: tproj1_voices.wav ..)
            for alt in sorted(Path(ref_dir).glob("tproj" + n[2:] + "_*.wav")):
                rw = alt
                break
        if rw.is_file():
            rx, rrate = read_wav(rw)
            ref = stats(rx, rrate, k)
            ref["rate"] = rrate
        else:
            out.append(f"{n}: no render {rw}")
    s["ref"] = ref
    return s


def table(rows):
    head = ["test", "rate", "dur s", "exp s", "peak", "rms", "clip runs", "clicks/min", "gaps (max)", "repeats (max)", "drift ppm"]
    lines = []
    for s in rows:
        g, r = s["gaps"], s["repeats"]
        lines.append([s["name"], str(s["rate"]), f"{s['dur']:.1f}", "-" if s["exp"] is None else f"{s['exp']:.1f}",
                      f"{s['peak']:.1f}", f"{s['rms']:.1f}", f"{s['clip_runs']} ({s['clip_samples']})",
                      f"{s['cpm']:.1f} ({len(s['clicks'])})", f"{len(g)} ({max((n for _, n in g), default=0)})",
                      f"{len(r)} ({max((n for _, n in r), default=0)})", "-" if s["drift"] is None else f"{s['drift']:+.0f}"])
    w = [max(len(head[i]), *(len(r[i]) for r in lines)) for i in range(len(head))] if lines else [len(h) for h in head]
    fmt = lambda row: "  ".join(c.rjust(w[i]) if i else c.ljust(w[i]) for i, c in enumerate(row))
    return [fmt(head), fmt(["-" * x for x in w])] + [fmt(r) for r in lines]


def notes(s):
    out = []
    rate = s["rate"]
    if s["clip_runs"]:
        out.append(f"  {s['name']}: CLIPPED: {s['clip_runs']} runs at full scale ({s['clip_samples']} samples)")
    if s["gaps"]:
        out.append(f"  {s['name']}: GAPS at {where(s['gaps'], rate)} (frames of zero in the middle of sound)")
    if s["repeats"]:
        out.append(f"  {s['name']}: REPEATED FRAMES at {where(s['repeats'], rate)} (a UAC underrun repeats the last frame)")
    if s["clicks"]:
        out.append(f"  {s['name']}: clicks at {where([(i, 0) for i in s['clicks']], rate, 6)}")
    if s["exp"] is not None and abs(s["dur"] - s["exp"]) > max(1.0, 0.01 * s["exp"]):
        out.append(f"  {s['name']}: LENGTH {s['dur']:.1f} s, the run says {s['exp']:.1f} s")
    if s["drift"] is not None and abs(s["drift"]) > 5000:
        out.append(f"  {s['name']}: DRIFT {s['drift']:+.0f} ppm of the wall clock (more than latency explains)")
    fig = s["fig"]
    if fig:
        d = fig.get("deltas", {})
        busy = {k: v for k, v in d.items() if v}
        if busy:
            out.append(f"  {s['name']}: the FM-1 counted {', '.join(f'{k} +{v}' for k, v in busy.items())} during the run")
        if fig.get("timed_out") or fig.get("stopped_by_tester"):
            out.append(f"  {s['name']}: the run was {'cut at the time limit' if fig.get('timed_out') else 'stopped by the tester'}")
    return out


def compare(s):
    """the recording against the emulator's render: statistics, the same measures"""
    r = s["ref"]
    if r is None:
        return []
    lines = [f"{s['name']} against its render ({r['rate']} Hz, {r['dur']:.1f} s):",
             f"    {'':<14}{'recording':>11}{'render':>11}{'difference':>12}",
             f"    {'peak dBFS':<14}{s['peak']:>11.1f}{r['peak']:>11.1f}{s['peak'] - r['peak']:>+12.1f}",
             f"    {'rms dBFS':<14}{s['rms']:>11.1f}{r['rms']:>11.1f}{s['rms'] - r['rms']:>+12.1f}",
             f"    {'clicks/min':<14}{s['cpm']:>11.1f}{r['cpm']:>11.1f}{s['cpm'] - r['cpm']:>+12.1f}",
             f"    {'gaps':<14}{len(s['gaps']):>11}{len(r['gaps']):>11}{len(s['gaps']) - len(r['gaps']):>+12}",
             f"    {'repeats':<14}{len(s['repeats']):>11}{len(r['repeats']):>11}{len(s['repeats']) - len(r['repeats']):>+12}",
             f"    {'clip runs':<14}{s['clip_runs']:>11}{r['clip_runs']:>11}{s['clip_runs'] - r['clip_runs']:>+12}",
             f"    {'sound s':<14}{s['span']:>11.1f}{r['span']:>11.1f}{s['span'] - r['span']:>+12.1f}",
             f"    {'centroid Hz':<14}{s['centroid']:>11.0f}{r['centroid']:>11.0f}{s['centroid'] - r['centroid']:>+12.0f}"]
    # (a band's level only against the whole: the recording's gain can differ, so each band is read after the rms)
    off = s["rms"] - r["rms"]
    bd = [(f"{lo}-{hi}", (a - b) - 0.0) for (lo, hi), a, b in zip(BANDS, s["bands"], r["bands"])]
    lines.append("    bands dB rec-render (power): " + " ".join(f"{n}:{v:+.1f}" for n, v in bd))
    v = []
    if abs(off) > 3:
        v.append(f"LEVEL differs by {off:+.1f} dB rms (MASTER, the USB's gain, or the project)")
    if s["cpm"] > 2 * r["cpm"] + 1:
        v.append(f"MORE CLICKS than the render: {s['cpm']:.1f} against {r['cpm']:.1f} a minute")
    if len(s["gaps"]) > len(r["gaps"]) or len(s["repeats"]) > len(r["repeats"]):
        v.append("GAPS or REPEATS the render does not have")
    if r["span"] and abs(s["span"] / r["span"] - 1.0) > 0.01:
        v.append(f"the sound is {s['span']:.1f} s long, the render's {r['span']:.1f} s")
    if s["peak"] > r["peak"] + 3:
        v.append(f"peak {s['peak'] - r['peak']:+.1f} dB over the render's")
    if any(abs(d) > 6 for _, d in bd if True) and abs(off) <= 3:
        v.append("a band differs by more than 6 dB (the spectrum is not the render's)")
    lines.append("    " + ("; ".join(v) if v else "matches the render in level, clicks, gaps and length"))
    return lines


def run(run_dir, ref_dir, k):
    d = Path(run_dir)
    wavs = sorted(d.glob("tp[1-4].wav"))
    out = [f"{d}: {len(wavs)} recording(s)" + (f", renders from {ref_dir}" if ref_dir else "")]
    rows = [r for r in (check_file(w, ref_dir, k, out) for w in wavs) if r]
    if rows:
        out += table(rows)
        for s in rows:
            out += notes(s)
        for s in rows:
            out += compare(s)
    return out, rows


# --------------------------------------------------------------------------------------------------- self-test
def synth(rate, secs, seed):
    """music-like: three low tones (a chord), a bass, and a decaying noise hit every half second, kept under 0.7"""
    rng = np.random.default_rng(seed)
    t = np.arange(int(rate * secs)) / rate
    m = 0.12 * np.sin(2 * np.pi * 110 * t) + 0.08 * np.sin(2 * np.pi * 220.5 * t) + 0.08 * np.sin(2 * np.pi * 330.7 * t + 1) \
        + 0.06 * np.sin(2 * np.pi * 523.3 * t) * (0.6 + 0.4 * np.sin(2 * np.pi * 0.7 * t))
    hit = np.zeros_like(t)
    n = int(rate * 0.25)
    env = np.exp(-np.arange(n) / (rate * 0.05))
    for i in range(0, len(t) - n, rate // 2):
        nz = rng.standard_normal(n)
        nz = np.convolve(nz, np.ones(8) / 8, mode="same")                       # a dull hit, not hiss
        hit[i:i + n] += 0.25 * env * nz
    x = (m + hit)[:, None] * np.array([[1.0, 0.9]])
    ramp = np.linspace(0.0, 1.0, rate // 20)[:, None]
    x[: rate // 2] = 0                                                          # the lead, silence, then a fade in
    x[rate // 2: rate // 2 + len(ramp)] *= ramp
    x[-rate // 2:] = 0
    x[-rate // 2 - len(ramp): -rate // 2] *= ramp[::-1]
    return x.astype(np.float32)


def selftest():
    ok = True
    print("hw_audio_check self-test: synthetic recordings with known faults, a clean one, and a render")
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        run_dir, ref_dir = td / "run", td / "ref"
        run_dir.mkdir()
        ref_dir.mkdir()
        rate = 48000
        base = synth(rate, 20.0, 1)
        write_wav(ref_dir / "tp1.wav", synth(44118, 20.0, 1), 44118)
        write_wav(ref_dir / "tp2.wav", synth(44118, 20.0, 1), 44118)
        write_wav(ref_dir / "tp3.wav", synth(44118, 20.0, 1), 44118)
        write_wav(run_dir / "tp1.wav", base, rate)                              # clean
        # tp2: 6 clicks (a step of 0.3 held for 40 samples), 3 gaps, 2 repeats
        bad = base.copy()
        click_at = [int(rate * s) for s in (2.1, 4.3, 6.7, 9.2, 12.4, 15.5)]
        for i in click_at:                                                      # a step that dies away: one discontinuity
            bad[i:i + 400] += (0.3 * np.exp(-np.arange(400) / 60.0))[:, None].astype(np.float32)
        gap_at = [(int(rate * 3.3), 200), (int(rate * 8.1), 60), (int(rate * 13.9), 900)]
        for a, n in gap_at:
            bad[a:a + n] = 0
        rep_at = [(int(rate * 5.55), 150), (int(rate * 16.8), 400)]
        for a, n in rep_at:
            bad[a:a + n] = bad[a - 1]
        write_wav(run_dir / "tp2.wav", bad, rate)
        (run_dir / "tp2.json").write_text(json.dumps({"rate": rate, "frames": len(bad), "wall_frames": len(bad) + 40,
                                                      "sent_at_frame": 0, "none_at_frame": int(rate * 18.0),
                                                      "deltas": {"uac_underruns": 3, "uac_overruns": 0}}), encoding="utf-8")
        # tp3: clipping, 44.1 kHz: the middle 2 s at 4x gain; and 2 % too short (a lost stretch)
        loud = np.clip(synth(44100, 20.0, 1) * 1.0, -1, 1)
        loud[44100 * 8:44100 * 10] = np.clip(loud[44100 * 8:44100 * 10] * 6.0, -1.0, 1.0)
        loud = loud[: int(len(loud) * 0.9)]
        write_wav(run_dir / "tp3.wav", loud, 44100)
        out, rows = run(run_dir, ref_dir, CLICK_K)
        print("\n".join(out))
        by = {r["name"]: r for r in rows}
        checks = [
            ("clean file: no clip, no click, no gap, no repeat",
             not by["tp1"]["clicks"] and not by["tp1"]["gaps"] and not by["tp1"]["repeats"] and by["tp1"]["clip_runs"] == 0),
            ("clean file against its render: no verdict",
             "matches the render" in "\n".join(compare(by["tp1"]))),
            ("tp2: the 6 injected clicks found, within 3 samples (and no others)",
             len(by["tp2"]["clicks"]) == 6 and all(min(abs(c - a) for a in click_at) <= 3 for c in by["tp2"]["clicks"])),
            ("tp2: the 3 gaps found, with their lengths",
             [n for _, n in by["tp2"]["gaps"]] == [200, 60, 900]),
            ("tp2: the 2 repeats found, with their lengths",
             [n for _, n in by["tp2"]["repeats"]] == [149, 399] or [n for _, n in by["tp2"]["repeats"]] == [150, 400]),
            ("tp2: the JSON's counters and drift read", abs(by["tp2"]["drift"] + 40 / len(bad) * 1e6) < 5 and by["tp2"]["exp"] is not None),
            ("tp3: clipped runs found at 44.1 kHz", by["tp3"]["clip_runs"] >= 100),
            ("tp3: shorter than its render: said",
             "the sound is" in "\n".join(compare(by["tp3"]))),
            ("tp3: louder peak against the render: said", "peak" in "\n".join(compare(by["tp3"]))),
        ]
        for what, good in checks:
            print(f"selftest: {what:<75} {'ok' if good else 'FAIL'}")
            ok &= bool(good)
    print(f"selftest: {'all passed' if ok else 'FAILED'}")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description="the hardware test's USB audio recordings: levels, clicks, gaps, repeats, drift")
    ap.add_argument("run_dir", nargs="?", type=Path)
    ap.add_argument("--ref", type=Path, help="the emulator's renders (build/tproj/tpN.wav, web/emu/strata_tproj.c)")
    ap.add_argument("--click-k", type=float, default=CLICK_K, help="a click is this many times the local median second difference")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.run_dir:
        ap.error("RUN_DIR (build/hwtest/<date-time>) or --selftest")
    if not a.run_dir.is_dir():
        print(f"no folder {a.run_dir}")
        return 1
    out, rows = run(a.run_dir, a.ref, a.click_k)
    print("\n".join(out))
    return 0 if rows else 1


if __name__ == "__main__":
    sys.exit(main())
