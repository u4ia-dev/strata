#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""The workbench as one HTML file: the page (web/strata/index.html) with the WebAssembly module (base64) and the
AudioWorklet's source (a string the page turns into a module) put in its two placeholders.

    make_page.py strata.wasm worklet.js index.html out.html
"""
import base64
import json
import sys


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
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(html)
    print(f"workbench: {out} ({len(html)} B, the module {len(b64) * 3 // 4} B)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
