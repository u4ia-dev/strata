#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""STRATA's emulator as one page: Felucca's page as build_strata.sh branded it (DIR/index.html) with what it fetched
put in, for a host that serves the page alone or with few files beside it:
  - the WebAssembly module (DIR/strata.wasm) as base64, in place of fetch("strata.wasm");
  - the worklet's helpers the page imports (DIR/worklet.js, its exports as plain declarations) in place of the import;
    its whole source as a string, made into a module (a Blob URL) when worklet.js cannot be loaded beside the page;
  - the knob labels' font (DIR/fonts/DotGothic16-subset.woff) as a data: URI, its licence (fonts/OFL.txt) in the footer;
  - the side gutters clear of a phone's safe areas (16 px at least).
Written twice: DIR/strata_emulator.html a document of its own (to open locally), DIR/strata_emulator_content.html the
content alone (to publish where the host wraps it in a document, with worklet.js beside it).

    make_page_strata.py DIR
"""
import base64
import html as htmllib
import json
import os
import re
import sys

HEAD = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n')
TITLE = '<title>Strata V0.67 Emulator</title>\n<meta name="claude:viewport-fit-sides" content="cover">\n'

LOAD_WORKLET = '''// the worklet: worklet.js beside the page, else its source (WORKLET_SRC) as a module of its own
async function loadWorklet(ac) {
  try { await ac.audioWorklet.addModule("worklet.js"); return; } catch {}
  const url = URL.createObjectURL(new Blob([WORKLET_SRC], { type: "text/javascript" }));
  try { await ac.audioWorklet.addModule(url); } finally { URL.revokeObjectURL(url); }
}
'''


def once(text, old, new):
    """text with old replaced by new, old found exactly once"""
    assert text.count(old) == 1, f"make_page_strata: {old[:60]!r} found {text.count(old)} times"
    return text.replace(old, new)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    d = argv[1]
    rd = lambda name, mode="r": open(os.path.join(d, name), mode, **({} if "b" in mode else {"encoding": "utf-8"}))
    with rd("index.html") as f:
        page = f.read()
    with rd("worklet.js") as f:
        worklet = f.read()
    with rd("strata.wasm", "rb") as f:
        wasm = base64.b64encode(f.read()).decode("ascii")
    with rd("fonts/DotGothic16-subset.woff", "rb") as f:
        font = base64.b64encode(f.read()).decode("ascii")
    with rd("fonts/OFL.txt") as f:
        ofl = f.read()

    assert "</script" not in worklet.lower(), "make_page_strata: the worklet's source would end the page's script"
    notice = page[page.index("<!--"):page.index("-->") + 3] + (
        "\n<!-- Changed for Strata, 2026-10-15 (Copyright (C) 2026 brandonmbarone, GPL-3.0-only): its name and storage"
        " keys (web/emu/build_strata.sh); the module, the worklet's helpers and the font put in, the gutters"
        " (web/emu/make_page_strata.py). -->")
    style = page[page.index("<style>"):page.index("</style>") + len("</style>")]
    body = page[page.index("<body>") + len("<body>"):page.index("</body>")].strip("\n")

    style = once(style, 'src: url("fonts/DotGothic16-subset.woff")', f'src: url("data:font/woff;base64,{font}")')
    style = once(style, "padding: 24px 16px 32px; }",
                 "padding-block: 24px 32px;\n       padding-left: max(16px, env(safe-area-inset-left, 0px));"
                 " padding-right: max(16px, env(safe-area-inset-right, 0px)); }")
    style = once(style, "main { padding: 8px 8px 24px; }", "main { padding-block: 8px 24px; }")
    style = once(style, "footer small { font-size: 11px; opacity: .8; }",
                 "footer small { font-size: 11px; opacity: .8; }\n"
                 "footer details { margin: 8px auto 0; max-width: 72ch; text-align: left; }\n"
                 "footer summary { cursor: pointer; text-align: center; }\n"
                 "footer pre { white-space: pre-wrap; font: 11px/1.5 ui-monospace, Menlo, monospace; }")

    body = once(body, '<a href="fonts/OFL.txt">SIL OFL 1.1</a>', '<a href="#ofl" id="ofllink">SIL OFL 1.1</a>')
    body = once(body, "</footer>", '  <details id="ofl"><summary>DotGothic16\'s licence (SIL OFL 1.1)</summary>'
                                   f"<pre>{htmllib.escape(ofl)}</pre></details>\n</footer>")
    # the helpers the page imports from worklet.js, declared in the page: its declarations, not the processor
    helpers = re.sub(r"^export ", "", worklet[:worklet.index('if (typeof registerProcessor === "function")')], flags=re.M)
    assert not re.search(r"\bexport\b", helpers), "make_page_strata: an export left in the worklet's helpers"
    body = once(body, 'import { midiKnob, KNOB_MODES } from "./worklet.js";\n',
                "// (worklet.js, its declarations: the page uses midiKnob and KNOB_MODES)\n" + helpers +
                "// ------------------------------------------------------------------ the module and the worklet\n"
                f'const WASM_B64 = "{wasm}";\nconst WORKLET_SRC = {json.dumps(worklet)};\n' + LOAD_WORKLET)
    body = once(body, '''    const [wasm] = await Promise.all([
      fetch("strata.wasm").then((r) => { if (!r.ok) throw new Error(`strata.wasm: ${r.status}`); return r.arrayBuffer(); }),
      ac.audioWorklet.addModule("worklet.js")]);''',
                '''    const wasm = Uint8Array.from(atob(WASM_B64), (c) => c.charCodeAt(0)).buffer;
    await loadWorklet(ac);''')
    body = once(body, "  } catch { /* no permission: the panel only */ }",
                '  } catch { document.getElementById("midiknobs").hidden = true; }   // no permission: the panel only')
    body = once(body, 'document.getElementById("go").addEventListener("click", switchOn);',
                'document.getElementById("go").addEventListener("click", switchOn);\n'
                'document.getElementById("ofllink").addEventListener("click", () => { document.getElementById("ofl").open = true; });')

    content = TITLE + notice + "\n" + style + "\n" + body + "\n"
    whole = HEAD + TITLE + notice + "\n" + style + "\n</head>\n<body>\n" + body + "\n</body>\n</html>\n"
    for name, text in (("strata_emulator.html", whole), ("strata_emulator_content.html", content)):
        with open(os.path.join(d, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"emu_strata: {d}/strata_emulator.html ({len(whole)} B), strata_emulator_content.html (the content alone)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
