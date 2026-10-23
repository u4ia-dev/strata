#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
"""Make STRATA's site (GitHub Pages) from the flasher make_flasher.py made:
  index.html                  the landing page: what Strata is, the disclaimer, the data warning, the links
  flash/                      build/flasher as it is (the flasher, the package, the test page, the USB check, FLASHING.md,
                              README.md, NOTICE.md, LICENSE, LICENSING.md, LICENSES/: the licences beside the package)
  LICENSE, NOTICE.md, LICENSING.md, LICENSES/   (again at the top)
  .nojekyll                   (served as files)
serve.py stays out (the backup step runs on the user's own computer, from the source).
  web/strata_flash/make_site.py REPO_URL [OUT_DIR]   (after make_flasher.py; OUT_DIR: build/site)
"""
import hashlib
import html
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
FLASHER = ROOT / "build" / "flasher"


def page(repo, version, sha):
    r = html.escape(repo, quote=True)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Strata for FM-1</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap">
<style>
  :root {{ --bg: #0b0b0b; --card: #131312; --line: #2c2a27; --ink: #f4f1ea; --mute: #bab7b1; --hot: #ff4d2e;
          --hot-ink: #0b0b0b; color-scheme: dark; }}
  @media (prefers-color-scheme: light) {{ :root {{ --bg: #f4f1ea; --card: #fbfaf6; --line: #d6d1c6; --ink: #0b0b0b;
          --mute: #55524c; --hot: #d93a1c; --hot-ink: #fff; color-scheme: light; }} }}
  * {{ box-sizing: border-box; }}
  body {{ margin: 0; background: var(--bg); color: var(--ink); font: 14px/1.65 "Space Mono", ui-monospace, Consolas, monospace;
         padding: 40px max(16px, env(safe-area-inset-left, 0px)) 64px max(16px, env(safe-area-inset-right, 0px)); }}
  main {{ max-width: 760px; margin: 0 auto; display: grid; gap: 18px; }}
  h1 {{ margin: 0; font-size: 32px; letter-spacing: .18em; }}
  h2 {{ margin: 0; font-size: 13px; letter-spacing: .14em; text-transform: uppercase; }}
  p {{ margin: 0; }} .mute {{ color: var(--mute); }}
  section {{ background: var(--card); border: 1px solid var(--line); padding: 16px 18px; display: grid; gap: 10px; }}
  section:hover {{ border-color: var(--mute); }}
  section.warn {{ border-left: 3px solid var(--hot); }}
  .row {{ display: flex; flex-wrap: wrap; gap: 10px; }}
  a.btn {{ text-decoration: none; font-weight: 700; font-size: 12px; letter-spacing: .08em; text-transform: uppercase;
          padding: 10px 14px; border: 1px solid var(--line); color: var(--ink); }}
  a.btn:hover, a.btn:focus-visible {{ border-color: var(--hot); outline: none; }}
  a.btn.go {{ background: var(--hot); border-color: var(--hot); color: var(--hot-ink); }}
  a {{ color: var(--hot); }}
  code {{ font-size: 12.5px; }}
</style>
</head>
<body>
<main>
  <h1>STRATA</h1>
  <p class="mute">Wavetable synth, 8-track looper and drum machine firmware for the M-VAVE FM-1. By u4ia, a fork of
    Felucca. Free software, GPL-3.0-only. Package {html.escape(version)}, SHA-256 <code>{html.escape(sha[:16])}…</code></p>
  <div class="row">
    <a class="btn go" href="flash/">Install</a>
    <a class="btn" href="flash/test.html">Test</a>
    <a class="btn" href="flash/FLASHING.md">How to (FLASHING.md)</a>
    <a class="btn" href="{r}">Source</a>
  </div>
  <section class="warn">
    <h2>Before you install</h2>
    <p><b>Saving anything in Strata can overwrite Felucca's projects and samples</b> (the same flash), and going back
      does not bring them back. Make a full flash backup first and keep it (FLASHING.md, section 2): it is the only way
      back to exactly what you had. Strata never writes the boot area, the calibration and trim data, Bluetooth pairing
      or the unit's own keys.</p>
    <p class="mute">Chrome or Edge on a computer, the FM-1 on a USB data cable straight into it. Backups are made on your
      computer (the source's <code>serve.py</code>, or the Flipper Zero route), not on this site.</p>
  </section>
  <section class="warn">
    <h2>Disclaimer</h2>
    <p>Strata is independent, unofficial firmware: not affiliated with, endorsed by or supported by M-VAVE or the
      owners of the FM-1, nor by Felucca's author or Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
      respective owners; "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.</p>
    <p>Strata is provided <b>as is, without any warranty</b> (GPL-3.0, sections 15 and 16). Installing firmware other
      than the official one can leave the FM-1 unusable until it is recovered: you install it at your own risk. This is
      a <b>pre-release</b>, being tested on the hardware now.</p>
  </section>
  <section>
    <h2>Good to know</h2>
    <p>The SysEx message <code>F0 22 24 35 7D F7</code> on the FM-1's USB MIDI port (as on the official firmware)
      restarts it into backup mode; any program that can send SysEx can send it. Switching the FM-1 off and on returns
      to Strata. OCT− and OCT+ held 5 s do the same.</p>
    <p>The USB serial console, the DEBUG menu (HOME held 5 s) and CORE 2 (off by default) are in every build.</p>
    <p>Going back: M-VAVE's updater or the flasher's "Return to official V15" with M-VAVE's own file; Felucca's
      installer; or your backup by the Flipper Zero route.</p>
  </section>
  <section>
    <h2>Licences</h2>
    <p>Strata, Copyright (C) 2026 u4ia, is a modified version of <a href="https://github.com/hugelton/Felucca">Felucca</a>,
      Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: both GPL-3.0-only. The complete source,
      with the Felucca-Engines submodule: <a href="{r}">{r}</a>. The package also holds three JieLi AC79 SDK files
      (Apache-2.0). <a href="NOTICE.md">NOTICE.md</a>, <a href="LICENSING.md">LICENSING.md</a>,
      <a href="LICENSE">LICENSE</a>, <a href="LICENSES/">LICENSES/</a>.</p>
  </section>
</main>
</body>
</html>
"""


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    repo = sys.argv[1]
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "build" / "site"
    if not (FLASHER / "index.html").exists() or not (FLASHER / "strata.fwsc").exists():
        raise SystemExit("site: run web/strata_flash/make_flasher.py first")
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(FLASHER, out / "flash", ignore=shutil.ignore_patterns("serve.py", "__pycache__"))
    for doc in ("LICENSE", "LICENSING.md", "NOTICE.md", "README.md"):
        shutil.copy(ROOT / doc, out / doc)
    shutil.copytree(ROOT / "LICENSES", out / "LICENSES")
    for lic in (out / "LICENSES", out / "flash" / "LICENSES"):   # (Pages lists no folder: an index of the texts)
        items = "".join(f'<li><a href="{f.name}">{f.name}</a></li>' for f in sorted(lic.glob("*.txt")))
        (lic / "index.html").write_text(f'<!doctype html><meta charset="utf-8"><title>Licences</title><ul>{items}</ul>',
                                        encoding="utf-8")
    raw = (FLASHER / "index.html").read_text(encoding="utf-8")
    ver = re.search(r'"version":\s*"([^"]*)"', raw)  # (the facts make_flasher.py inlined)
    meta = {"version": ver.group(1) if ver else "?",
            "sha256": hashlib.sha256((FLASHER / "strata.fwsc").read_bytes()).hexdigest()}
    (out / "index.html").write_text(page(repo, str(meta["version"]), str(meta["sha256"])), encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"site: {out} (the flasher in flash/, {meta['version']}; source {repo})")


if __name__ == "__main__":
    main()
