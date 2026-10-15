#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 brandonmbarone
"""The target's instruction count for a run of the engine: the JieLi clang listing (-S -g: each instruction's
source line) weighed by how often the host ran that line (gcov, tools/costprobe/sy_profile.c). An estimate of the
work, not a measurement of the device's time (the BENCH page will be that):

  * an instruction counts its line's executions; one outside every loop of its target function at most once a call
    of that function (code the target hoisted, unrolled or set up before a loop);
  * code inlined from a function called in several places (the call sites read from the sources, a macro's use
    one): gcov counts its lines over every site, so each inlined copy takes its share through the nearest
    instruction before it of a function that calls it: that one's weight x (this line's count / the function's
    calls), or with none in sight an even share;
  * a function both inlined and kept whole (a kernel called directly and through a table): the whole copy
    keeps what the inlined ones do not account for;
  * calls the listing makes to code it does not show (memset, memcpy) are counted apart.

  sy_dyncount.py LISTING_G.s GCOV.json BLOCKS [--top N] [--json OUT]       (tools/costprobe/sy_dyn.sh runs it)
"""
import json
import os
import re
import sys

from count_asm import BRANCH, FUNC, LABEL

LOC = re.compile(r"^\s*\.loc\s+(\d+)\s+(\d+)")
FILE = re.compile(r'^\s*\.file\s+(\d+)\s+"([^"]*)"(?:\s+"([^"]*)")?')
IDENT_CALL = re.compile(r"\b([A-Za-z_][A-Za-z_0-9]*)\s*\(")
# target functions that are the probe's wrappers of an engine function (sy_probe.c): their calls are its calls
ALIAS = {"probe_engine": "sy_block", "probe_voice": "voice_block", "probe_fx": "fx_block"}
DEBUG = os.environ.get("DYN_DEBUG")                   # a source function: each of its weighed instructions


def parse_listing(path):
    """[(func, [items])] where an item is ("label", name) or ("insn", text, comment, file, line)"""
    files, funcs, cur, loc = {}, {}, None, (None, 0)
    with open(path) as f:
        for ln in f:
            ln = ln.rstrip("\n")
            m = FILE.match(ln)
            if m and m.group(3) is not None:
                files[int(m.group(1))] = os.path.basename(m.group(3))
                continue
            m = FUNC.match(ln)
            if m:
                cur = funcs.setdefault(m.group(1), [])
                loc = (None, 0)
                continue
            if cur is None:
                continue
            if ln.startswith(".Lfunc_end"):
                cur = None
                continue
            m = LOC.match(ln)
            if m:
                fl, line = files.get(int(m.group(1))), int(m.group(2))
                if line:                              # (line 0: compiler-made, kept on the line before)
                    loc = (fl, line)
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
            cur.append(("insn", text.strip(), comment.strip(), loc[0], loc[1]))
    return funcs


def loops_of(items):
    """for each item: the index of the back edge (a branch to an earlier label) closing the innermost loop it is
    in, or None"""
    pos, inner = {}, [None] * len(items)
    for i, it in enumerate(items):
        if it[0] == "label":
            pos[it[1]] = i
    spans = []
    for i, it in enumerate(items):
        if it[0] == "insn":
            m = BRANCH.search(it[1])
            if m and m.group(1) in pos and pos[m.group(1)] < i:
                spans.append((pos[m.group(1)], i))
    for a, b in sorted(spans, key=lambda x: -(x[1] - x[0])):     # widest first: the innermost writes last
        for k in range(a, b + 1):
            inner[k] = b
    return inner


def load_gcov(path):
    """line counts {(file, line): count}, the line's function {(file, line): name}, calls {name: n}, and each
    function's extent {name: (file, first, last)}"""
    with open(path) as f:                             # (gcov -t -j: one JSON document a line, one data file)
        data = json.loads(f.read().strip().splitlines()[0])
    counts, owner, calls, extent = {}, {}, {}, {}
    for fe in data["files"]:
        base = os.path.basename(fe["file"])
        for fn in fe["functions"]:
            calls[fn["name"]] = calls.get(fn["name"], 0) + fn["execution_count"]
            extent[fn["name"]] = (base, fn["start_line"], fn["end_line"])
        for le in fe["lines"]:
            k = (base, le["line_number"])
            counts[k] = counts.get(k, 0) + le["count"]
            if le.get("function_name"):
                owner[k] = le["function_name"]
    return counts, owner, calls, extent


DEFINE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z_0-9]*)")
IDENT = re.compile(r"\b([A-Za-z_][A-Za-z_0-9]*)\b")


def call_graph(extent, src_dirs):
    """{callee: {callers}} and {callee: call sites} from the sources: the known functions each function calls,
    directly or through the macros it uses (SY_FLUSH calls sy_absf, NZ_LOOP sy_rand ...), a macro's use a site"""
    names, lines_of, macros = set(extent), {}, {}
    for d in src_dirs:
        for fn in sorted(os.listdir(d)):
            if not (fn.startswith("sy_") and fn.endswith((".c", ".h"))):
                continue
            with open(os.path.join(d, fn), encoding="utf-8") as f:
                lines_of.setdefault(fn, f.read().split("\n"))
            ls, i = lines_of[fn], 0
            while i < len(ls):                        # macro bodies, continued lines and all
                m = DEFINE.match(ls[i])
                if m:
                    body = ls[i]
                    while body.endswith("\\") and i + 1 < len(ls):
                        i += 1
                        body = body[:-1] + " " + ls[i]
                    macros[m.group(1)] = set(IDENT.findall(body[m.end():]))
                i += 1

    def expand(idents, seen):
        out = set()
        for x in idents:
            if x in names:
                out.add(x)
            elif x in macros and x not in seen:
                out |= expand(macros[x], seen | {x})
        return out

    graph, sites = {n: set() for n in names}, {n: 0 for n in names}
    for n, (base, a, b) in extent.items():
        text = "\n".join(lines_of.get(base, [])[a - 1:b])
        uses = [m.group(1) for m in IDENT_CALL.finditer(text)] + [x for x in IDENT.findall(text) if x in macros]
        for u in uses:                                # (a macro both ways: counted once, as a call)
            for f in expand([u], set()) & names - {n}:
                graph[n].add(f)
                sites[f] += 1
    callers = {n: set() for n in names}
    for c, cs in graph.items():
        for x in cs:
            callers[x].add(c)
    return callers, sites


def weigh_function(tf, items, counts, owner, calls, callers, sites, edge_w):
    """each instruction's weight (None: no line); edge_w: the back edges' weights from a first pass (each
    instruction at most its innermost loop's: a loop's set-up code carries the loop's line)"""
    own = ALIAS.get(tf, tf)
    tcalls = calls.get(own)
    inner = loops_of(items)
    ws, recent = [None] * len(items), []              # recent: (owner, weight) so far, newest last
    for i, it in enumerate(items):
        if it[0] != "insn":
            continue
        k = (it[3], it[4])
        src = owner.get(k)
        if src is None:
            continue
        w = cnt = float(counts.get(k, 0))
        if src != own and calls.get(src) and sites.get(src, 0) > 1:
            ctx = None                                # inlined from several places: this copy's share through its
            for (co, cw) in reversed(recent[-400:]):  # caller's instruction (whose weight is its own share) ..
                if co != src and co in callers.get(src, ()):
                    ctx = cw
                    break
            w = min(cnt, ctx * cnt / calls[src]) if ctx is not None else cnt / sites[src]   # .. or an even share
        if tcalls == 0:
            w = 0.0                                   # (a function the run never called)
        elif inner[i] is None:
            if tcalls is not None:
                w = min(w, float(tcalls))             # outside every loop: once a call at most
        elif edge_w is not None and edge_w.get(inner[i]) is not None and inner[i] != i:
            w = min(w, edge_w[inner[i]])
        ws[i] = w
        recent.append((src, w))
    return ws


def weigh(funcs, counts, owner, calls, callers, sites):
    """a function both inlined somewhere and kept whole (a kernel called directly and through a table): the host
    counts its calls together, so the whole copy keeps only what the inlined ones do not account for"""
    per_src, per_tgt, libcalls = {}, {}, {}
    tot = slots = 0.0
    unmapped = 0
    weights = {}
    for tf, items in funcs.items():
        if tf.startswith("probe_") and tf not in ALIAS:
            continue
        first = weigh_function(tf, items, counts, owner, calls, callers, sites, None)
        edges = {i: first[i] for i in set(x for x in loops_of(items) if x is not None)}
        weights[tf] = weigh_function(tf, items, counts, owner, calls, callers, sites, edges)
    inl, whole = {}, {}
    for tf, ws in weights.items():
        own = ALIAS.get(tf, tf)
        for i, it in enumerate(funcs[tf]):
            if it[0] == "insn" and ws[i]:
                src = owner[(it[3], it[4])]
                if src == own:
                    whole[src] = whole.get(src, 0.0) + ws[i]
                else:
                    inl[src] = inl.get(src, 0.0) + ws[i]
    for tf, ws in weights.items():
        own = ALIAS.get(tf, tf)
        scale = max(0.0, 1.0 - inl[own] / whole[own]) if inl.get(own) and whole.get(own) else 1.0
        for i, it in enumerate(funcs[tf]):
            if it[0] != "insn":
                continue
            if ws[i] is None:
                unmapped += 1
                continue
            w, text, src = ws[i] * scale, it[1], owner[(it[3], it[4])]
            if DEBUG and src == DEBUG and w > 0:
                print(f"    {tf:14s} {it[3]}:{it[4]:<5d} w {w:10.0f}  {text}")
            tot += w
            slots += 0.0 if text.endswith("#") else w
            per_src[src] = per_src.get(src, 0.0) + w
            per_tgt[tf] = per_tgt.get(tf, 0.0) + w
            m = re.match(r"call\s+([A-Za-z_][A-Za-z_0-9]*)$", text)
            if m and (m.group(1) in ("memset", "memcpy", "memcmp") or m.group(1).startswith("__")):
                libcalls[m.group(1)] = libcalls.get(m.group(1), 0.0) + w
    return tot, slots, per_src, per_tgt, libcalls, unmapped


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    listing, gcov, blocks = argv[1], argv[2], int(argv[3])
    top = int(argv[argv.index("--top") + 1]) if "--top" in argv else 18
    out = argv[argv.index("--json") + 1] if "--json" in argv else None
    here = os.path.dirname(os.path.abspath(__file__))
    srcs = [os.path.join(here, "..", "..", "firmware", "src"), os.path.join(here, "..", "..", "tests")]
    funcs = parse_listing(listing)
    counts, owner, calls, extent = load_gcov(gcov)
    callers, sites = call_graph(extent, srcs)
    tot, slots, per_src, per_tgt, libcalls, unmapped = weigh(funcs, counts, owner, calls, callers, sites)
    n = blocks * 32.0
    print(f"  {tot / n:8.1f} instructions a sample ({slots / n:.1f} issue slots), {tot / blocks:.0f} a block")
    for name, w in sorted(per_src.items(), key=lambda x: -x[1])[:top]:
        print(f"      {w / n:8.1f}  {name}")
    for name, w in sorted(libcalls.items(), key=lambda x: -x[1]):
        print(f"      calls to {name}: {w / blocks:.2f} a block (their bodies not counted)")
    if out:
        with open(out, "w") as f:
            json.dump({"per_sample": tot / n, "slots_per_sample": slots / n, "unmapped_insns": unmapped,
                       "by_source": {k: v / n for k, v in per_src.items()},
                       "by_target": {k: v / n for k, v in per_tgt.items()},
                       "libcalls_per_block": {k: v / blocks for k, v in libcalls.items()}}, f, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
