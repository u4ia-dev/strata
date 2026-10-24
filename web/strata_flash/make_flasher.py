#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# After Felucca's web/make_site.py (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0).
"""Make STRATA's flasher: a folder to serve on this computer (Web MIDI's SysEx needs a secure context: localhost).

  build/flasher/index.html      web/strata_flash/index.html, with Felucca's fm1pkg.js and fm1ota.js (unchanged)
                                and the package's facts (version, identity, size, SHA-256) inlined
  build/flasher/strata.fwsc     the package, exactly build/strata.fwsc
  build/flasher/serve.py        a server for 127.0.0.1 only, and the backup step before any write (py serve.py in an
                                administrator PowerShell, then http://127.0.0.1:8000/)
  build/flasher/usb_check.html  which way the FM-1's USB data fails (MIDI each way, the console), nothing written
  build/flasher/test.html       the hardware test, run from the computer: MIDI in, the console's read-only commands
                                and its RAM reads (the addresses and the parameter table of this very app, from
                                build/strata.syms and build/strata.bin; a fingerprint of the app's bytes, so the page
                                knows the FM-1 runs it), the FM-1's USB audio; guided steps for the panel
  build/flasher/FLASHING.md     docs/FLASHING.md: the backup, the install, the test, the way back
  build/flasher/LICENSE, LICENSING.md, LICENSES/   (the package holds JieLi SDK files under Apache-2.0)

Checked before anything is written: the package's identity is FM-1_9XY; it holds Felucca's update loader
(FELUCCA-LOADER-1); and it is exactly what tools/fm1pkg_make.py makes from build/strata.bin and
build/loader/ota.bin (so the page ships the app the build checked), an app that is Strata's (its splash string).

  python3 web/strata_flash/make_flasher.py [PACKAGE] [OUT_DIR]     (after tools/build.py --strata; AC79_SDK as for
                                                                    the build)
"""
import hashlib
import json
import re
import shutil
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1]
sys.path.insert(0, str(SRC / "tools"))
import fm1pkg_make  # noqa: E402

BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def strata_version():
    m = re.search(r'#define STRATA_VERSION "([^"]+)"', (SRC / "firmware" / "src" / "strata.c").read_text())
    if not m:
        raise SystemExit("make_flasher: no STRATA_VERSION in firmware/src/strata.c")
    return m.group(1)


XIP = 0x02000120                                 # the app's first byte, as the core reads it (app.ld, strata.ld)
RAM = (0x01C00000, 0x01C80000)                   # what the console's memr reads (hal/fm1_sys.h fm1_mem_readable)


def symbols():
    """build/strata.syms (tools/build.py, objdump -t): name -> (address, size)"""
    syms = {}
    for ln in (SRC / "build" / "strata.syms").read_text().splitlines():
        p = ln.split()
        if len(p) >= 4 and re.fullmatch(r"[0-9a-f]{8}", p[0]) and re.fullmatch(r"[0-9a-f]{8}", p[-2]):
            syms[p[-1]] = (int(p[0], 16), int(p[-2], 16))
    return syms


def test_meta(app, version, product, sha):
    """test.html's facts about this very app: the console's RAM reads go to these addresses, the parameters are read
    from its table (sy_param.c sp_desc_t: name[9], sec, unit, flags, cc, a pad byte, lo, hi, def), the sections'
    names from its SEC_NAMES; three runs of its bytes are the fingerprint the page reads back over memr"""
    syms = symbols()
    if (SRC / "build" / "strata.syms").stat().st_mtime < (SRC / "build" / "strata.bin").stat().st_mtime - 5:
        raise SystemExit("make_flasher: build/strata.syms is older than build/strata.bin: run tools/build.py --strata")

    def at(a, n):
        off = a - XIP
        if off < 0 or off + n > len(app):
            raise SystemExit(f"make_flasher: {a:#x}+{n} is not in build/strata.bin (strata.syms of another build?)")
        return app[off:off + n]

    def cstr(a):
        b = at(a, 1)
        s = bytearray()
        while b[0] and len(s) < 32:
            s += b
            a += 1
            b = at(a, 1)
        return s.decode("latin-1")
    need = ("SP", "SEC_NAMES", "sp_n", "sy_patch", "fm1_alnk0_irq")
    if any(n not in syms for n in need):
        raise SystemExit(f"make_flasher: build/strata.syms lacks one of {need}")
    sp_a, sp_len = syms["SP"]
    if sp_len % 20:
        raise SystemExit("make_flasher: SP is not a table of 20-byte rows (sy_param.c sp_desc_t changed?)")
    params = []
    for i in range(sp_len // 20):
        r = at(sp_a + 20 * i, 20)
        name = r[:9].split(b"\0")[0].decode("latin-1")
        lo, hi, de = struct.unpack_from("<hhh", r, 14)
        if not re.fullmatch(r"[A-Z0-9 .+/%-]{1,8}", name) or not lo <= de <= hi or r[9] > 64:
            raise SystemExit(f"make_flasher: parameter {i} reads {name!r} {lo}..{hi} ({de}): not this app's table")
        params.append([name, r[9], lo, hi, r[11], r[12]])
    names_a, names_len = syms["SEC_NAMES"]
    secs = [cstr(struct.unpack_from("<I", at(names_a, names_len), 4 * k)[0]) for k in range(names_len // 4)]
    for ram in ("sp_n", "sy_patch"):
        a, n = syms[ram]
        if not RAM[0] <= a < a + n <= RAM[1]:
            raise SystemExit(f"make_flasher: {ram} at {a:#x} is not where memr reads")
    if syms["sp_n"][1] != 4 * len(params) or syms["sy_patch"][1] < 2 * len(params):
        raise SystemExit("make_flasher: sp_n / sy_patch are not the parameter table's size")
    code = syms["voice_block" if "voice_block" in syms else "fm1_alnk0_irq"][0]   # (code: the voices' when the
    prints = [(a, at(a, 16).hex()) for a in (sp_a, names_a, code)]                   # compiler kept them apart)
    return {"version": version, "product": product, "sha256": sha, "params": params, "secs": secs,
            "addr": {"sp_n": syms["sp_n"][0], "sy_patch": syms["sy_patch"][0]},
            "fingerprint": [[a, h] for a, h in prints],
            "flags": {"mod": 1, "voice": 2, "smooth": 4},
            # RUN's two loops (sy_main.c sy_run_int, sy_run_flt): their instructions a pass, as counted in the listing
            # (V0.5's, and V0.65's the same instructions), and V0.5's times on the unit (its report, 2026-10-17)
            "run": {"int_loops": 100000, "int_insns": 7, "flt_loops": 50000, "flt_insns": 9,
                    "v05_int_us": 3892, "v05_flt_us": 3055},
            "v05": {"wait_max_us": 91203, "halves_late": 5, "stack_main": 18330}}


def main(pkg="build/strata.fwsc", out="build/flasher"):
    pkg, out = SRC / pkg, SRC / out
    raw = pkg.read_bytes()
    app = (SRC / "build" / "strata.bin").read_bytes()
    ota = (SRC / "build" / "loader" / "ota.bin").read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit(f"make_flasher: {pkg}: identity {product!r} is not a Felucca-family package (FM-1_9XY)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"make_flasher: {pkg}: no Felucca update loader in it")
    if b"STRATA\0" not in app or strata_version().encode() not in app:
        raise SystemExit("make_flasher: build/strata.bin is not Strata's app (no splash or version string)")
    if fm1pkg_make.ufw(fm1pkg_make.flash_image(app, fm1pkg_make.KEY), ota, product) != raw:
        raise SystemExit(f"make_flasher: {pkg} is not build/strata.bin + build/loader/ota.bin: run tools/build.py "
                         "--strata again")
    html = (HERE / "index.html").read_text(encoding="utf-8")
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"make_flasher: index.html must hold {mark} once")
    lib = "\n".join(strip_module((SRC / "web" / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js"))
    meta = {"version": strata_version(), "product": product, "pkg": "strata.fwsc", "size": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest()}
    html = html.replace("/*LIB*/", lib).replace("/*META*/", json.dumps(meta))
    if out.exists():
        shutil.rmtree(out)
    (out / "LICENSES").mkdir(parents=True)
    (out / "index.html").write_text(html, encoding="utf-8")
    (out / "strata.fwsc").write_bytes(raw)
    shutil.copy(HERE / "serve.py", out / "serve.py")
    shutil.copy(HERE / "usb_check.html", out / "usb_check.html")
    test = (HERE / "test.html").read_text(encoding="utf-8")
    if test.count("/*META*/") != 1:
        raise SystemExit("make_flasher: test.html must hold /*META*/ once")
    tmeta = test_meta(app, meta["version"], product, meta["sha256"])
    (out / "test.html").write_text(test.replace("/*META*/", json.dumps(tmeta, separators=(",", ":"))), encoding="utf-8")
    if (SRC / "docs" / "FLASHING.md").exists():
        shutil.copy(SRC / "docs" / "FLASHING.md", out / "FLASHING.md")
    for f in sorted((SRC / "LICENSES").glob("*.txt")):
        shutil.copy(f, out / "LICENSES" / f.name)
    for doc in ("LICENSE", "LICENSING.md", "NOTICE.md", "README.md"):   # (V0.87: the notices, the disclaimer)
        shutil.copy(SRC / doc, out / doc)
    print(f"flasher: {out}: index.html ({len(html)} B), strata.fwsc ({len(raw)} B, {product}, sha256 "
          f"{meta['sha256'][:16]}...), {meta['version']}")
    print(f"         serve it (Windows, an administrator PowerShell, for the backup step): py {out / 'serve.py'}, "
          "then http://127.0.0.1:8000/ in Chrome or Edge")


if __name__ == "__main__":
    main(*sys.argv[1:3])
