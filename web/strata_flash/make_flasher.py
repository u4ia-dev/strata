#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
# After Felucca's web/make_site.py (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0).
"""Make STRATA's flasher: a folder to serve on this computer (Web MIDI's SysEx needs a secure context: localhost).

  build/flasher/index.html      web/strata_flash/index.html, with Felucca's fm1pkg.js and fm1ota.js (unchanged)
                                and the package's facts (version, identity, size, SHA-256) inlined
  build/flasher/strata.fwsc     the package, exactly build/strata.fwsc
  build/flasher/serve.py        a server for 127.0.0.1 only (python serve.py, then http://127.0.0.1:8000/)
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
    if (SRC / "docs" / "FLASHING.md").exists():
        shutil.copy(SRC / "docs" / "FLASHING.md", out / "FLASHING.md")
    for f in sorted((SRC / "LICENSES").glob("*.txt")):
        shutil.copy(f, out / "LICENSES" / f.name)
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(SRC / doc, out / doc)
    print(f"flasher: {out}: index.html ({len(html)} B), strata.fwsc ({len(raw)} B, {product}, sha256 "
          f"{meta['sha256'][:16]}...), {meta['version']}")
    print(f"         serve it: python3 {out / 'serve.py'}, then http://127.0.0.1:8000/ in Chrome or Edge")


if __name__ == "__main__":
    main(*sys.argv[1:3])
