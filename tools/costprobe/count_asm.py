#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 u4ia
"""Count the instructions in each loop of each function of a JieLi clang assembly listing (-S, pi32v2).

The compiler's own listing is used rather than objdump's: objdump does not decode the FPU (-mfprev1)
instructions and misreads their second halves, while the assembly prints them as '(f)' operations on the
general registers. Per loop (innermost first): instructions; issue slots (an instruction ending in '#' pairs
with the next one: dual issue); loads and stores; spill reloads (values the loop keeps on the stack because it
runs out of registers); FPU operations; multiplies; calls.

  count_asm.py LISTING.s [func ...]           (sy_cost.sh runs it)
  count_asm.py LISTING.s --loops func         one function's loops only, the largest first
"""
import re
import sys

FUNC = re.compile(r"^([A-Za-z_][A-Za-z_0-9]*):")
LABEL = re.compile(r"^(\.L[A-Za-z_0-9]+):")
BRANCH = re.compile(r"goto (\.L[A-Za-z_0-9]+)")


def parse(path):
    funcs, cur = {}, None
    with open(path) as f:
        for ln in f:
            ln = ln.rstrip("\n")
            m = FUNC.match(ln)
            if m:
                cur = funcs.setdefault(m.group(1), [])
                continue
            if cur is None:
                continue
            if ln.startswith(".Lfunc_end"):
                cur = None
                continue
            m = LABEL.match(ln)
            if m:
                cur.append(("label", m.group(1)))
                continue
            if not ln.startswith("\t"):
                continue
            body = ln.strip()
            if not body or body.startswith(".") or body.startswith(";"):
                continue
            text, _, comment = body.partition(";")
            cur.append(("insn", text.strip(), comment.strip()))
    return funcs


def loops(items):
    pos, out = {}, []
    for i, it in enumerate(items):
        if it[0] == "label":
            pos[it[1]] = i
    for i, it in enumerate(items):
        if it[0] != "insn":
            continue
        m = BRANCH.search(it[1])
        if m and m.group(1) in pos and pos[m.group(1)] < i:
            body = [x for x in items[pos[m.group(1)]:i + 1] if x[0] == "insn"]
            n = len(body)
            pairs = sum(1 for x in body if x[1].endswith("#"))
            out.append({
                "label": m.group(1), "n": n, "slots": n - pairs,
                "mem": sum(1 for x in body if "[" in x[1]),
                "reload": sum(1 for x in body if "Reload" in x[2]),
                "fpu": sum(1 for x in body if "(f)" in x[1]),
                "mul": sum(1 for x in body if re.search(r"\*", x[1])),
                "call": sum(1 for x in body if x[1].startswith("call")),
            })
    return out


def main(argv):
    funcs = parse(argv[1])
    want = [a for a in argv[2:] if not a.startswith("--")] or sorted(funcs)
    for name in want:
        if name not in funcs:
            print(f"{name}: not in the listing")
            continue
        ls = loops(funcs[name])
        ninsn = sum(1 for x in funcs[name] if x[0] == "insn")
        print(f"{name}: {ninsn} instructions")
        for L in sorted(ls, key=lambda x: -x["n"]) if "--loops" in argv else ls:
            print(f"   loop {L['label']:10s} {L['n']:4d} insns, {L['slots']:4d} issue slots (fpu {L['fpu']}, mem "
                  f"{L['mem']}, reloads {L['reload']}, mul {L['mul']}, calls {L['call']})")


if __name__ == "__main__":
    main(sys.argv)
