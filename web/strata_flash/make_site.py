#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make STRATA's site (GitHub Pages) from the flasher make_flasher.py made:
  index.html                  the installer: build/flasher's index.html without its INTERNAL parts (the test and
                              report), its LOCAL parts hidden (the local backup server's step) and its PUBLIC parts
                              shown (the site's pages along the top, the background); beside it the package
                              strata.fwsc and the notices and licences it links (README.md, NOTICE.md, LICENSE,
                              LICENSING.md, LICENSES/)
  info.html                   what Strata is, the data warning, the disclaimer, good to know, the licences
  book/                       the guide and the manual (book/, built by `npm run build` there into build/book)
  fx.js                       the background (web/strata_flash/site_fx.js: HOME's bloom as a field of dots)
  flash/index.html            the installer's old address, sent on to the new one
  .nojekyll                   (served as files)
The test page, the USB check, FLASHING.md and serve.py are the internal hardware test's and stay out.
  web/strata_flash/make_site.py REPO_URL [OUT_DIR]   (after make_flasher.py and the book; OUT_DIR: build/site)
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
sys.path.insert(0, str(HERE))
from make_flasher import brand  # noqa: E402  (the logo and the favicon into a page)
INTERNAL = ("serve.py", "__pycache__", "test.html", "usb_check.html", "FLASHING.md", "index.html")
BOOK_INSTALL = "book/manual/install.html"


def public_flasher(raw, repo):
    """the installer as the site serves it (see the docstring); its pointers to FLASHING.md, the internal test's
    document, turned to the manual's install page"""
    t = re.sub(r"[ ]*<!--INTERNAL-->.*?<!--/INTERNAL-->\n", "", raw, flags=re.S)
    t = re.sub(r"/\*INTERNAL\*/.*?/\*/INTERNAL\*/\n?", "", t, flags=re.S)
    t = re.sub(r"<!--LOCAL-->(.*?)<!--/LOCAL-->", r"<div hidden>\1</div>", t, flags=re.S)
    t = re.sub(r"<!--PUBLIC:(.*?):PUBLIC-->", r"\1", t, flags=re.S)
    t = t.replace("@REPO@", html.escape(repo, quote=True))
    swaps = [
        ("<title>Strata Flasher</title>", "<title>Strata for FM-1</title>"),
        ('<span class="n">1 / 3</span>', '<span class="n">1 / 2</span>'),
        ('<span class="n">2 / 3</span>', '<span class="n">2 / 2</span>'),
        ("Hardware test flasher for the M-VAVE FM-1 &middot; Chrome or Edge, on this computer only",
         "Installer for the M-VAVE FM-1 &middot; Chrome or Edge on a computer"),
        ('$("find").disabled ? "s3" : "s2"', '"s2"'),
        (" Now step 3.", ""),
        ("made the backup (FLASHING.md, section 6).",
         f'made the backup (<a href="{BOOK_INSTALL}#going-back">the manual</a>).'),
        ("Zero (FLASHING.md, section 6). Both",
         f'Zero (<a href="{BOOK_INSTALL}#recovery">the manual</a>). Both'),
        ('backup mode (FLASHING.md, " +\n    "section 6), and send the log.',
         'backup mode (the manual\'s " +\n    "Recovery), and keep the log.'),
        ("Use Chrome or Edge, and open the page from localhost (serve.py).", "Use Chrome or Edge on a computer."),
        ("run serve.py from Strata's source (FLASHING.md, section 2)", "use a backup tool (the manual: Back up first)"),
        ("open it from build\\\\flasher\\\\serve.py (FLASHING.md, section 3)", "use a backup tool (the manual: Back up first)"),
    ]
    for a, b in swaps:
        if t.count(a) != 1:
            raise SystemExit(f"site: the flasher's {a[:50]!r} is not there once: update make_site.py")
        t = t.replace(a, b)
    for left in ("INTERNAL-->", "INTERNAL*/", "LOCAL-->", "PUBLIC:", ":PUBLIC", "@REPO@", 'id="s3"', "FLASHING.md, section",
                 "<!--LOGO-->", "<!--FAVICON-->",
                 "test.html", "usb_check", "on this computer only"):
        if left in t:
            raise SystemExit(f"site: {left!r} left in the public flasher")
    return t


def nav(repo, here):
    r = html.escape(repo, quote=True)
    items = [("index.html", "Install"), ("book/index.html", "Guide &amp; manual"), ("info.html", "Info"), (r, "Source")]
    cur = ' aria-current="page"'
    return "".join(f'<a href="{u}"{cur if l == here else ""}>{l}</a>' for u, l in items)


def info_page(repo, version, sha):
    r = html.escape(repo, quote=True)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Strata for FM-1 · Info</title>
<!--FAVICON-->
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Space+Mono:wght@400;700&display=swap">
<style>
  :root {{ --bg: #0b0b0b; --card: #131312; --line: #2c2a27; --ink: #f4f1ea; --mute: #bab7b1; --hot: #ff4d2e;
          --hot-ink: #0b0b0b; color-scheme: dark; }}
  * {{ box-sizing: border-box; }}
  body {{ margin: 0; background: var(--bg); color: var(--ink); font: 14px/1.65 "Space Mono", ui-monospace, Consolas, monospace;
         padding: 28px max(16px, env(safe-area-inset-left, 0px)) 64px max(16px, env(safe-area-inset-right, 0px)); }}
  #fx {{ position: fixed; inset: 0; width: 100%; height: 100%; z-index: -1; pointer-events: none; }}
  main {{ max-width: 760px; margin: 0 auto; display: grid; gap: 18px; }}
  h1 {{ margin: 0; font-size: 32px; letter-spacing: .18em; display: flex; align-items: center; gap: 16px; }}
  h1 .logo {{ width: 64px; height: 64px; flex: none; }}
  h2 {{ margin: 0; font-size: 13px; letter-spacing: .14em; text-transform: uppercase; }}
  p {{ margin: 0; }} .mute {{ color: var(--mute); }}
  section {{ background: var(--card); border: 1px solid var(--line); padding: 16px 18px; display: grid; gap: 10px; }}
  section:hover {{ border-color: var(--mute); }}
  section.warn {{ border-left: 3px solid var(--hot); }}
  nav.site {{ display: flex; flex-wrap: wrap; gap: 8px; }}
  nav.site a {{ text-decoration: none; font-weight: 700; font-size: 12px; letter-spacing: .08em; text-transform: uppercase;
          padding: 9px 13px; border: 1px solid var(--line); color: var(--ink); background: var(--bg); }}
  nav.site a:hover, nav.site a:focus-visible {{ border-color: var(--hot); outline: none; }}
  nav.site a[aria-current="page"] {{ background: var(--hot); border-color: var(--hot); color: var(--hot-ink); }}
  a {{ color: var(--hot); }}
  code {{ font-size: 12.5px; }}
</style>
</head>
<body>
<canvas id="fx" aria-hidden="true"></canvas><script src="fx.js" defer></script>
<main>
  <nav class="site" aria-label="Site">{nav(repo, "Info")}</nav>
  <h1><!--LOGO-->STRATA</h1>
  <p class="mute">Strata turns the M-VAVE FM-1 into a complete pocket instrument: a deep wavetable synth, an 8-track
    looper and a drum machine. Free software, built on Felucca. Package {html.escape(version)}, SHA-256
    <code>{html.escape(sha[:16])}…</code></p>
  <section class="warn">
    <h2>Before you install</h2>
    <p><b>Saving anything in Strata can overwrite Felucca's projects and samples</b> (the same flash), and going back
      does not bring them back. Make a full flash backup first and keep it (<a href="{BOOK_INSTALL}#back-up-first">how</a>):
      it is the only way back to exactly what you had. Strata never writes the boot area, the calibration and trim data,
      Bluetooth pairing or the unit's own keys.</p>
    <p class="mute">Chrome or Edge on a computer, the FM-1 on a USB data cable straight into it. Backups are made with a
      backup tool on your computer, not on this site.</p>
  </section>
  <section class="warn">
    <h2>Disclaimer</h2>
    <p>Strata is independent, unofficial firmware: not affiliated with, endorsed by or supported by M-VAVE or the
      owners of the FM-1, nor by Felucca's author or Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
      respective owners; "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.</p>
    <p>Strata is provided <b>as is, without any warranty</b> (GPL-3.0, sections 15 and 16). Installing firmware other
      than the official one can leave the FM-1 unusable until it is recovered: you install it at your own risk. This is
      a <b>pre-release</b>, being tested on the hardware now.</p>
    <p>Strata was created entirely with Claude, an AI model made by Anthropic.</p>
  </section>
  <section>
    <h2>Good to know</h2>
    <p>Backup mode: OCT− and OCT+ held 5 s, or the SysEx message <code>F0 22 24 35 7D F7</code> on the FM-1's USB
      MIDI port (as on the official firmware). With V0.87 the computer may show an unknown USB device instead; the
      FM-1 Transporter and a Flipper Zero enter backup mode at power-on, whatever the firmware does. Switching the FM-1
      off and on returns to Strata.</p>
    <p>The USB serial console, the DEBUG menu (HOME held 5 s) and CORE 2 (off by default) are in every build.</p>
    <p>Going back: M-VAVE's updater or the installer's "Return to official V15" with M-VAVE's own file; Felucca's
      installer; or your backup, written back in backup mode (<a href="{BOOK_INSTALL}#going-back">how</a>).</p>
  </section>
  <section>
    <h2>Licences</h2>
    <p>Strata is a modified version of <a href="https://github.com/hugelton/Felucca">Felucca</a>, Copyright (C) 2026
      Leo Kuroshita (@kurogedelic), Hügelton Instruments, and is licensed GPL-3.0-only like it. The complete source,
      with the Felucca-Engines submodule: <a href="{r}">{r}</a>. The package also holds three JieLi AC79 SDK files
      (Apache-2.0). <a href="NOTICE.md">NOTICE.md</a>, <a href="LICENSING.md">LICENSING.md</a>,
      <a href="LICENSE">LICENSE</a>, <a href="LICENSES/">LICENSES/</a>.</p>
  </section>
</main>
</body>
</html>
"""


REDIRECT = """<!doctype html>
<meta charset="utf-8"><title>Strata for FM-1</title>
<meta http-equiv="refresh" content="0; url=../index.html">
<link rel="canonical" href="../index.html">
<p>The installer has moved to <a href="../index.html">the site's home page</a>.</p>
"""


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    repo = sys.argv[1]
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "build" / "site"
    if not (FLASHER / "index.html").exists() or not (FLASHER / "strata.fwsc").exists():
        raise SystemExit("site: run web/strata_flash/make_flasher.py first")
    if not (ROOT / "build" / "book" / "index.html").exists():
        raise SystemExit("site: build the book first (npm run build in book/)")
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(FLASHER, out, ignore=shutil.ignore_patterns(*INTERNAL))   # the package, the notices, the licences
    raw = (FLASHER / "index.html").read_text(encoding="utf-8")
    (out / "index.html").write_text(public_flasher(raw, repo), encoding="utf-8", newline="\n")
    shutil.copytree(ROOT / "build" / "book", out / "book")
    shutil.copy(HERE / "site_fx.js", out / "fx.js")
    items = "".join(f'<li><a href="{f.name}">{f.name}</a></li>' for f in sorted((out / "LICENSES").glob("*.txt")))
    (out / "LICENSES" / "index.html").write_text(   # (Pages lists no folder: an index of the texts)
        f'<!doctype html><meta charset="utf-8"><title>Licences</title><ul>{items}</ul>', encoding="utf-8")
    ver = re.search(r'"version":\s*"([^"]*)"', raw)  # (the facts make_flasher.py inlined)
    version = ver.group(1) if ver else "?"
    sha = hashlib.sha256((FLASHER / "strata.fwsc").read_bytes()).hexdigest()
    (out / "info.html").write_text(brand(info_page(repo, version, sha), ROOT), encoding="utf-8", newline="\n")
    shutil.copytree(ROOT / "assets" / "logo", out / "logo")   # (the logo's files, for anyone linking to them)
    (out / "flash").mkdir()
    (out / "flash" / "index.html").write_text(REDIRECT, encoding="utf-8", newline="\n")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    print(f"site: {out} (the installer at its root, {version}; source {repo})")


if __name__ == "__main__":
    main()
