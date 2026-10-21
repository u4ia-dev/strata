#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
"""The workbench as one HTML file: the page (web/strata/index.html, its content: a title, styles, the markup and the
script) with the WebAssembly module (base64) and the AudioWorklet's source (a string the page turns into a module) in
its two placeholders. Written twice: OUT.html a document of its own (to open locally), OUT_content.html the content
alone (to publish where the host wraps it in a document).

    make_page.py strata.wasm worklet.js index.html OUT.html
"""
import base64
import json
import sys

HEAD = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n')


def main(argv):
    if len(argv) != 5:
        print(__doc__)
        return 2
    wasm, worklet, page, out = argv[1:]
    with open(wasm, "rb") as f:
        b64 = base64.b64encode(f.read()).decode("ascii")
    with open(worklet, encoding="utf-8") as f:
        src = json.dumps(f.read())
    with open(page, encoding="utf-8") as f:
        html = f.read()
    assert html.count("@WASM_B64@") == 2 and html.count("@WORKLET_SRC@") == 2   # (the comment names them once)
    html = html.replace('"@WASM_B64@"', '"' + b64 + '"').replace("const WORKLET_SRC = @WORKLET_SRC@;",
                                                                 "const WORKLET_SRC = " + src + ";")
    cut = html.index("<header>")                       # the head's part (title, styles) and the body's
    whole = HEAD + html[:cut] + "</head>\n<body>\n" + html[cut:] + "</body>\n</html>\n"
    content = out[:-5] + "_content.html" if out.endswith(".html") else out + "_content"
    for path, text in ((out, whole), (content, html)):
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    print(f"workbench: {out} ({len(whole)} B; the module {len(b64) * 3 // 4} B), {content} (the content alone)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
