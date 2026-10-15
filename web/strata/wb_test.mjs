// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// The workbench's module (build/strata/strata.wasm, web/strata/strata_web.c) through the exports the page and the
// worklet use: no imports (the worklet instantiates it with none); every test sound plays the engine test's phrase
// to the hash recorded in tests/sy_golden.txt (bit for bit the host's DSP); a patch's code round trip; a patch load
// faded and swapped as on the FM-1; values as text.
//   node web/strata/wb_test.mjs [build/strata/strata.wasm]        (from the repo root, after web/strata/build.sh)
import { readFileSync } from "node:fs";

const path = process.argv[2] || "build/strata/strata.wasm";
const bytes = readFileSync(path);
let checks = 0, failed = 0;
function check(what, ok) {
  checks++;
  if (!ok) failed++;
  console.log(`wb: ${what.padEnd(96)} ${ok ? "ok" : "FAIL"}`);
}

async function instance() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  return ex;
}
const latin1 = new TextDecoder("windows-1252");
function cstr(ex, p) {
  const m = new Uint8Array(ex.memory.buffer);
  let e = p;
  while (m[e]) e++;
  return latin1.decode(m.subarray(p, e));
}

const mod = await WebAssembly.compile(bytes);
check(`no imports (${WebAssembly.Module.imports(mod).map((i) => i.module + "." + i.name).join(", ") || "none"})`,
      WebAssembly.Module.imports(mod).length === 0);

// ---- the test sounds' phrase, as tests/sy_engine_test.c play() and hash_out()
const golden = new Map();
for (const ln of readFileSync("tests/sy_golden.txt", "utf8").split("\n")) {
  const sp = ln.lastIndexOf(" ");
  if (sp > 0) golden.set(ln.slice(0, sp), ln.slice(sp + 1).trim());
}
const ex = await instance();
ex.sw_init();
const LINE = [48, 55, 60, 63, 67, 72, 70, 67], MASK = (1n << 64n) - 1n, PRIME = 1099511628211n;
let allSame = true;
for (let k = 0; k < ex.sw_sound_count(); k++) {
  const name = cstr(ex, ex.sw_sound_name(k));
  let h = 1469598103934665603n;
  const run = (secs) => {
    const nb = ex.sw_test_blocks(secs);
    for (let b = 0; b < nb; b++) {
      ex.sw_render(32);
      const l = new Float32Array(ex.memory.buffer, ex.sw_out_l(), 32), r = new Float32Array(ex.memory.buffer, ex.sw_out_r(), 32);
      for (let i = 0; i < 32; i++) {
        h = ((h ^ BigInt(Math.trunc(l[i] * 32768) >>> 0)) * PRIME) & MASK;
        h = ((h ^ BigInt(Math.trunc(r[i] * 32768) >>> 0)) * PRIME) & MASK;
      }
    }
  };
  ex.sw_test_sound(k);
  ex.sw_note(48, 100); ex.sw_note(55, 90); ex.sw_note(63, 80);
  run(0.8);
  ex.sw_note(48, 0); ex.sw_note(55, 0); ex.sw_note(63, 0);
  for (let i = 0; i < 8; i++) {
    ex.sw_note(LINE[i], 70 + 7 * i);
    run(0.15);
    ex.sw_note(LINE[i], 0);
    run(0.05);
  }
  ex.sw_test_bend(12000);
  ex.sw_note(60, 100);
  run(0.3);
  ex.sw_note(60, 0);
  ex.sw_test_bend(8192);
  run(1.0);
  const got = h.toString(16).padStart(16, "0"), want = golden.get(name);
  console.log(`wb:   ${name.padEnd(12)} ${got}${got === want ? "" : "  <- tests/sy_golden.txt has " + want}`);
  allSame &&= got === want;
}
check("every test sound plays the engine test's phrase bit for bit as recorded (tests/sy_golden.txt)", allSame);

// ---- a patch's code round trip, every test sound and INIT
{
  const m = await instance();
  let same = true;
  for (let k = 0; k <= m.sw_sound_count(); k++) {
    m.sw_sound(k);
    const a = Array.from(new Int16Array(m.memory.buffer, m.sw_values(), m.sw_param_count())), name = cstr(m, m.sw_patch_name());
    const n = m.sw_pack();
    new Int16Array(m.memory.buffer, m.sw_values(), m.sw_param_count()).fill(0);
    same &&= m.sw_unpack(n) === 1 && name === cstr(m, m.sw_patch_name()) &&
             a.every((v, i) => v === new Int16Array(m.memory.buffer, m.sw_values(), m.sw_param_count())[i]);
  }
  new Uint8Array(m.memory.buffer, m.sw_code_buf(), 4).set([88, 88, 88, 88]);
  check("a patch's code: packed and unpacked the same (the test sounds, INIT); a foreign one refused",
        same && m.sw_unpack(40) === 0);
}

// ---- a load as the page does it: faded, then the values playing are the ones sent
{
  const m = await instance();
  m.sw_init();
  m.sw_note(60, 100);
  for (let b = 0; b < 8; b++) m.sw_render(128);
  m.sw_sound(0);
  const want = Array.from(new Int16Array(m.memory.buffer, m.sw_values(), m.sw_param_count()));
  const loaded = m.sw_load(), busy = m.sw_load();
  let quanta = 0;
  while (m.sw_loading() && quanta < 100) { m.sw_render(128); quanta++; }
  for (let b = 0; b < 100; b++) m.sw_render(128);                 // (the tables built a step a quantum)
  m.sw_current();
  const now = Array.from(new Int16Array(m.memory.buffer, m.sw_values(), m.sw_param_count()));
  check(`a patch load: taken, a second one held while it fades (${quanta} quanta), then playing as sent`,
        loaded === 1 && busy === 0 && quanta > 0 && quanta < 20 && want.every((v, i) => v === now[i]));
}

// ---- values as text through the export
{
  const m = await instance();
  const id = (name, sec) => {
    for (let i = 0; i < m.sw_param_count(); i++)
      if (cstr(m, m.sw_param_name(i)) === name && cstr(m, m.sw_section_name(m.sw_param_section(i))) === sec) return i;
    return -1;
  };
  const cut = id("CUTOFF", "FILTER"), pos = id("WT POS", "OSC A"), deg = id("PHASE", "OSC A");
  check("values as text: the cutoff, a percentage, the degree sign (Latin-1)",
        cstr(m, m.sw_text(cut, 1000)) === "20.0 kHz" && cstr(m, m.sw_text(pos, 750)) === "75.0%" &&
        cstr(m, m.sw_text(deg, 90)) === "90°");
}

console.log(`wb: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
