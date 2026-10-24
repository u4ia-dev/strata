#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# After tools/size_fns.py (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0).
"""STRATA (build.py --strata): Felucca's drivers built as Felucca builds them, the rest at Strata's -O2. strata.c goes
to LLVM IR without the optimizer; every function that usb.c, midi_uart.c or ota.c defines gets 'optsize' (-Os, per
function), and 'noinline' where Strata's own code calls or names it (else -O2 inlines it there and optimises it as
Strata's); then build.py compiles the IR at -O2. Which file defines a function comes from a second IR of the same unit
built with -g (its DISubprogram's file), used for nothing else. Felucca and every other fork build usb.c at -Os; built
at -O2, Strata's V0.4 (2026-10-16) and OberVave (2026-10-13) stopped sending on the bulk IN endpoints after a chip
reset (docs/FLASHING.md, section 9). With STRATA_DRIVERS=0 the IR passes unchanged: that round trip is byte-identical
to compiling strata.c directly.
V0.65: Strata's main-loop files (SIZE_FILES: the UI, the console, the presets' store, the app's and the boot's) are
built for size too ('optsize'), as Felucca builds its own UI (size_fns.py), except what an interrupt can run
(size_fns.isr_reach: kept at -O2). The drawing's primitives (sy_gfx.c, gfx.c, lcd.c), the wavetables' generator and
the DSP stay at -O2. STRATA_SIZE=0: the drivers alone (V0.6's build).
  strata_drivers.py IN.ll DEBUG.ll OUT.ll"""
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from size_fns import isr_reach  # noqa: E402  (Felucca's: the interrupts' roots are the same two)

DRIVER_FILES = ("usb.c", "midi_uart.c", "ota.c")
SIZE_FILES = ("sy_ui.c", "sy_ui_draw.c", "sy_ui_fx.c", "sy_ui_save.c", "sy_text.c", "sy_console.c", "sy_store.c",
              "sy_app.c", "sy_main.c", "sy_hw.c", "sy_chord.c", "sy_ui_chord.c",   # (V0.7: the chords'; V0.8, the
              "sy_loop.c", "sy_ui_loop.c", "sy_ui_drum.c")   # sequencer's; their interrupt side stays -O2: isr_reach)
DEFINE = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #(\d+)[^\n]*\{)$",
                    re.M)
BODY = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\((.*?)^\}", re.M | re.S)
REF = re.compile(r"@\"?([\w.]+)\"?")
GROUP = re.compile(r"^attributes #(\d+) = \{([^\n]*)\}$", re.M)
DBG_DEFINE = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\([^\n]*!dbg !(\d+)", re.M)
SUBPROGRAM = re.compile(r"^!(\d+) = distinct !DISubprogram\([^\n]*?file: !(\d+)", re.M)
DIFILE = re.compile(r"^!(\d+) = !DIFile\(filename: \"([^\"]*)\"", re.M)


def functions_of(dbg, names):
    """the functions DEBUG.ll says are defined in the files names"""
    files = {n: Path(f).name for n, f in DIFILE.findall(dbg)}
    sub_file = {n: files.get(f, "") for n, f in SUBPROGRAM.findall(dbg)}
    return {name for name, sp in DBG_DEFINE.findall(dbg) if sub_file.get(sp) in names}


def main(src, dst_dbg, dst):
    ir = Path(src).read_text()
    if os.environ.get("STRATA_DRIVERS") == "0":
        Path(dst).write_text(ir)
        print("drivers: STRATA_DRIVERS=0, every function at -O2 (V0.4's build)")
        return 0
    dbg = Path(dst_dbg).read_text()
    drivers = functions_of(dbg, DRIVER_FILES)
    body = {m.group(1): m.group(2) for m in BODY.finditer(ir)}
    groups = dict(GROUP.findall(ir))
    drivers &= body.keys()
    outside = {g for f, b in body.items() if f not in drivers for g in REF.findall(b) if g in drivers}
    if not {"usb_poll", "ep1_tx", "sie_wr", "sie_rd"} <= drivers:
        print("drivers: usb.c's functions not found in the IR (the debug IR's files?)")
        return 1
    size, isr = set(), set()
    if os.environ.get("STRATA_SIZE") != "0":
        size = functions_of(dbg, SIZE_FILES) & body.keys()
        isr = size & isr_reach(ir)
        size -= isr | drivers
        if not {"sy_ui_ms", "sy_ui_frame", "con_exec"} <= size:
            print("size: the UI's functions not found in the IR (the debug IR's files?)")
            return 1
    count = [0, 0, 0]

    def mark(m):
        name, attrs = m.group(2), groups.get(m.group(4), "").split()
        if name not in drivers and name not in size:
            return m.group(0)
        add = [] if "optsize" in attrs or "minsize" in attrs else ["optsize"]
        if name in drivers:
            if name in outside and "alwaysinline" not in attrs and "noinline" not in attrs:
                add.append("noinline")
                count[1] += 1
            count[0] += 1
        else:
            count[2] += 1
        return m.group(1) + "".join(" " + a for a in add) + m.group(3)
    Path(dst).write_text(DEFINE.sub(mark, ir))
    print(f"drivers: {count[0]} functions of {', '.join(DRIVER_FILES)} at -Os, {count[1]} of them kept out of "
          f"Strata's code (noinline)")
    print(f"size: {count[2]} main-loop functions of Strata's at -Os ({len(isr)} of those files' an interrupt reaches "
          f"kept at -O2)" if size else "size: STRATA_SIZE=0, Strata's main loop at -O2 (V0.6's build)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
