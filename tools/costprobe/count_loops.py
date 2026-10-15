#!/usr/bin/env python3
"""Count the instructions in each loop of each function of a pi32v2 objdump listing.

The JieLi objdump does not decode the FPU (-mfprev1) instructions: each is 4 bytes that start
3f e5 / 3f f5 and print as '<unknown instruction>' plus a bogus 2-byte line. They are merged
here and counted as one instruction (column fpu).

  count_loops.py LISTING [func ...]
Prints, per function and loop (innermost first): instructions, of them FPU, loads/stores,
multiplies, and whether the loop body holds a call."""
import re
import sys

LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9.]*):\s*$")
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
TARGET = re.compile(r"goto -?\d+ <[^>]*: ([0-9a-f]+) >")


def functions(path):
    out, cur = {}, None
    with open(path) as f:
        lines = f.read().splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i]
        m = LABEL.match(ln.strip())
        if m:
            cur = out.setdefault(m.group(1), [])
            i += 1
            continue
        m = INSN.match(ln)
        if cur is not None and m:
            addr = int(m.group(1), 16)
            raw = m.group(2).split()
            text = m.group(3).strip()
            if raw[:2] in (["3f", "e5"], ["3f", "f5"]) and len(raw) == 2:
                # FPU: this line and the next are one 4-byte instruction
                nxt = INSN.match(lines[i + 1]) if i + 1 < len(lines) else None
                tail = nxt.group(2).split() if nxt else []
                cur.append((addr, "fpu " + " ".join(raw + tail), True))
                i += 2
                continue
            cur.append((addr, text, False))
        i += 1
    return out


def loops(insns):
    lo = insns[0][0]
    spans = {}
    for a, t, _ in insns:
        m = TARGET.search(t)
        if m and lo <= int(m.group(1), 16) < a:
            h = int(m.group(1), 16)
            spans[h] = max(spans.get(h, a), a)
    res = []
    for h, e in sorted(spans.items()):
        body = [(a, t, f) for a, t, f in insns if h <= a <= e]
        inner = [x for x in spans.items() if x != (h, e) and h <= x[0] and x[1] <= e]
        res.append({
            "head": h, "end": e, "n": len(body), "fpu": sum(1 for x in body if x[2]),
            "mem": sum(1 for x in body if re.search(r"\[", x[1]) and not x[2]),
            "mul": sum(1 for x in body if re.search(r"\*", x[1])),
            "call": sum(1 for x in body if x[1].startswith("call")),
            "nested": len(inner),
        })
    return res


def main():
    fns = functions(sys.argv[1])
    want = sys.argv[2:] or sorted(fns)
    for name in want:
        if name not in fns or not fns[name]:
            continue
        ls = loops(fns[name])
        print(f"{name}: {len(fns[name])} insns")
        for L in ls:
            print(f"   loop @{L['head']:x}-{L['end']:x}: {L['n']:3d} insns (fpu {L['fpu']}, mem {L['mem']}, "
                  f"mul {L['mul']}, calls {L['call']}, inner loops {L['nested']})")


if __name__ == "__main__":
    main()
