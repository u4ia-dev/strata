#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
# After tools/size_fns.py (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0).
"""STRATA (build.py --strata): Felucca's drivers built as Felucca builds them, the rest at Strata's -O2. strata.c goes
to LLVM IR without the optimizer; every function that usb.c, midi_uart.c or ota.c defines gets 'optsize' (-Os, per
function), and 'noinline' where Strata's own code calls or names it (else -O2 inlines it there and optimises it as
Strata's); then build.py compiles the IR at -O2. Which file defines a function comes from a second IR of the same unit
built with -g (its DISubprogram's file), used for nothing else. Felucca and every other fork build usb.c at -Os; built
at -O2, Strata's V0.4 (2026-10-16) and OberVave (2026-10-13) stopped sending on the bulk IN endpoints after a chip
reset (docs/FLASHING.md, section 9). With STRATA_DRIVERS=0 the IR passes unchanged: that round trip is byte-identical
to compiling strata.c directly.
  strata_drivers.py IN.ll DEBUG.ll OUT.ll"""
import os
import re
import sys
from pathlib import Path

DRIVER_FILES = ("usb.c", "midi_uart.c", "ota.c")
DEFINE = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #(\d+)[^\n]*\{)$",
                    re.M)
BODY = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\((.*?)^\}", re.M | re.S)
REF = re.compile(r"@\"?([\w.]+)\"?")
GROUP = re.compile(r"^attributes #(\d+) = \{([^\n]*)\}$", re.M)
DBG_DEFINE = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\([^\n]*!dbg !(\d+)", re.M)
SUBPROGRAM = re.compile(r"^!(\d+) = distinct !DISubprogram\([^\n]*?file: !(\d+)", re.M)
DIFILE = re.compile(r"^!(\d+) = !DIFile\(filename: \"([^\"]*)\"", re.M)


def driver_functions(dbg):
    """the functions DEBUG.ll says are defined in DRIVER_FILES"""
    files = {n: Path(f).name for n, f in DIFILE.findall(dbg)}
    sub_file = {n: files.get(f, "") for n, f in SUBPROGRAM.findall(dbg)}
    return {name for name, sp in DBG_DEFINE.findall(dbg) if sub_file.get(sp) in DRIVER_FILES}


def main(src, dbg_src, dst):
    ir = Path(src).read_text()
    if os.environ.get("STRATA_DRIVERS") == "0":
        Path(dst).write_text(ir)
        print("drivers: STRATA_DRIVERS=0, every function at -O2 (V0.4's build)")
        return 0
    drivers = driver_functions(Path(dbg_src).read_text())
    body = {m.group(1): m.group(2) for m in BODY.finditer(ir)}
    groups = dict(GROUP.findall(ir))
    drivers &= body.keys()
    outside = {g for f, b in body.items() if f not in drivers for g in REF.findall(b) if g in drivers}
    if not {"usb_poll", "ep1_tx", "sie_wr", "sie_rd"} <= drivers:
        print("drivers: usb.c's functions not found in the IR (the debug IR's files?)")
        return 1
    count = [0, 0]

    def mark(m):
        name, attrs = m.group(2), groups.get(m.group(4), "")
        if name not in drivers:
            return m.group(0)
        add = [a for a in ("optsize",) if a not in attrs.split()]
        if name in outside and "alwaysinline" not in attrs.split() and "noinline" not in attrs.split():
            add.append("noinline")
            count[1] += 1
        count[0] += 1
        return m.group(1) + "".join(" " + a for a in add) + m.group(3)
    Path(dst).write_text(DEFINE.sub(mark, ir))
    print(f"drivers: {count[0]} functions of {', '.join(DRIVER_FILES)} at -Os, {count[1]} of them kept out of "
          f"Strata's code (noinline)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
