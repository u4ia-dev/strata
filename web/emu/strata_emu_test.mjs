// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// STRATA on the simulated FM-1 (build/emu_strata/strata.wasm, web/emu/strata_web.c) in Node, driven as the page and
// the worklet drive it: no imports; it boots and draws; a key plays and its release ends; the LEDs; knobs (a list one
// entry a detent; a value speed-sensitive: a slow turn 0.1 % a detent, a quick one more); the families (FX, ALGORITHM,
// SELECT); PRESETS; the MOD layer (REC held: a white key picks the source, a knob its depth, a matrix slot made; at 0
// it goes when the layer closes; a double tap latches it, a tap lets it go); the MENU (HOME held) and its palettes;
// a screen region drawn only when it changes; Web MIDI and its CCs; the same input twice gives the same sound.
//   node web/emu/strata_emu_test.mjs [build/emu_strata/strata.wasm]      (from the repo root, after build_strata.sh)
import { readFileSync } from "node:fs";

const bytes = readFileSync(process.argv[2] || "build/emu_strata/strata.wasm");
let checks = 0, failed = 0;
function check(what, ok) {
  checks++;
  if (!ok) failed++;
  console.log(`emu: ${what.padEnd(100)} ${ok ? "ok" : "FAIL"}`);
}
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, REC: 11 };   // panel.c B_*
const E = { SELECT: 0, ALGO: 1, PRESET: 2, K1: 3, K2: 4, K3: 5, K4: 6 };                    // panel.c EN_*
const SEC = { OSCA: 0, DRIVE: 12, CHORUS: 13, MENU: 17 };                                   // sy_param.c SEC_*, SEC_MENU
const SRC = { LFO1: 3, LFO2: 4 };                                                           // sy_param.c SRC_*

async function boot() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  ex.web_nor_erase();
  ex.web_master(700);
  ex.web_boot();
  return ex;
}
function play(ex, secs) {                       // renders secs of audio: its peak and an FNV hash of it
  let peak = 0, h = 2166136261;
  for (let n = Math.max(1, Math.round(secs * 44118 / 128)); n > 0; n--) {
    ex.web_render(128);
    const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), 128), r = new Float32Array(ex.memory.buffer, ex.web_out_r(), 128);
    for (let i = 0; i < 128; i++) {
      peak = Math.max(peak, Math.abs(l[i]), Math.abs(r[i]));
      h = Math.imul(h ^ (Math.round(l[i] * 8388608) | 0), 16777619) >>> 0;
    }
  }
  return { peak, h };
}
function cstr(ex, p) {
  const m = new Uint8Array(ex.memory.buffer);
  let e = p;
  while (m[e]) e++;
  return String.fromCharCode(...m.subarray(p, e));
}
const tap = (ex, b) => { ex.web_buttons(1 << b); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.35); };   // (no double tap)
const slots = (ex) => Array.from({ length: 8 }, (_, s) => [0, 1, 2].map((f) => ex.web_test_slot(s, f)));

const mod = await WebAssembly.compile(bytes);
check("no imports (the worklet instantiates it with none)", WebAssembly.Module.imports(mod).length === 0);

const ex = await boot();
const quiet = play(ex, 0.3);
const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240), colours = new Set(fb);
check(`boots and draws: ${ex.web_screen_draws()} blits, ${colours.size} colours on the screen, silent before a key`,
      ex.web_screen_draws() > 0 && colours.size > 30 && quiet.peak < 1e-6);
check(`the first page: OSC A ${cstr(ex, ex.web_test_page_name())} (section ${ex.web_test_page_section()}), sound "${cstr(ex, ex.web_test_patch_name())}"`,
      ex.web_test_page_section() === SEC.OSCA && cstr(ex, ex.web_test_page_name()) === "WAVE" &&
      cstr(ex, ex.web_test_patch_name()) === "SUPERSAW");
{
  const d0 = ex.web_screen_draws();
  play(ex, 0.2);
  check(`nothing changed, nothing drawn (${ex.web_screen_draws() - d0} blits in 0.2 s)`, ex.web_screen_draws() === d0);
}

ex.web_keys(1 << 7);                            // key 7: C4 (F3 + 7)
const held = play(ex, 0.4), voices = ex.web_test_voices();
const keyLit = (ex.web_lit_keys() >> 7) & 1, editLit = (ex.web_lit_buttons() >> B.EDIT) & 1;
ex.web_keys(0);
const after = play(ex, 1.5);
check(`a key plays (peak ${held.peak.toFixed(3)}, ${voices} voice) and its release ends (${ex.web_test_voices()} voices after 1.5 s)`,
      held.peak > 0.02 && voices === 1 && ex.web_test_voices() === 0 && after.peak < held.peak);
check("the LEDs: the key held lit, the page's family button (EDIT) lit", keyLit === 1 && editLit === 1);

const [TABLE, POS] = [ex.web_test_page_param(0), ex.web_test_page_param(1)];
{                                               // KNOB 1, a list: one entry a detent
  const v0 = ex.web_test_value(TABLE), d0 = ex.web_screen_draws();
  ex.web_enc(E.K1, 3);
  play(ex, 0.03);
  check(`KNOB 1 turns the table one entry a detent (${v0} -> ${ex.web_test_value(TABLE)}), ${ex.web_screen_draws() - d0} regions redrawn`,
        ex.web_test_value(TABLE) === v0 + 3 && ex.web_screen_draws() - d0 <= 3);
}
{                                               // KNOB 2, a value: slow detents 0.1 % each, quick ones more
  const v0 = ex.web_test_value(POS);            // (SUPERSAW's WT POS is at its top: turned down)
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K2, -1); play(ex, 0.12); }
  const v1 = ex.web_test_value(POS);
  for (let i = 0; i < 6; i++) { ex.web_enc(E.K2, -1); play(ex, 0.004); }
  const v2 = ex.web_test_value(POS);
  check(`KNOB 2 speed-sensitive: 3 slow detents ${v1 - v0} (0.1 % each), 6 quick ones ${v2 - v1}`,
        v1 - v0 === -3 && v2 - v1 <= -150);
}
{
  tap(ex, B.FX);
  const s1 = ex.web_test_page_section();
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.03);
  const s2 = ex.web_test_page_section(), p2 = ex.web_test_page();
  ex.web_enc(E.SELECT, 1);
  play(ex, 0.03);
  check(`FX shows DRIVE, ALGORITHM the next effect, SELECT the next page (sections ${s1}, ${s2})`,
        s1 === SEC.DRIVE && s2 === SEC.CHORUS && ex.web_test_page() !== p2);
}
{
  ex.web_enc(E.PRESET, 1);
  play(ex, 0.3);
  check(`PRESETS loads the next sound ("${cstr(ex, ex.web_test_patch_name())}")`,
        ex.web_test_sound() === 1 && cstr(ex, ex.web_test_patch_name()) === "PLUCK");
}
{                                               // the MOD layer, on OSC A's WAVE page
  tap(ex, B.EDIT);
  const pos = ex.web_test_page_param(1);
  ex.web_buttons(1 << B.REC);
  play(ex, 0.6);
  const open = ex.web_test_mod();
  ex.web_keys(1 << 4);                          // A3, a white key: LFO 1
  play(ex, 0.03);
  ex.web_keys(0);
  play(ex, 0.03);
  const src = ex.web_test_mod_src(), quiet2 = ex.web_test_voices();
  for (let i = 0; i < 5; i++) { ex.web_enc(E.K2, 1); play(ex, 0.12); }
  const s = slots(ex), slot = s.findIndex(([a, d]) => a === SRC.LFO1 && d === pos);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`REC held: the MOD layer (${open}); A3 picks LFO 1 (${src}), plays nothing (${quiet2} voices)`,
        open === 1 && src === SRC.LFO1 && quiet2 === 0);
  check(`.. KNOB 2 sets LFO 1's depth on WT POS (slot ${slot + 1}: ${slot >= 0 ? s[slot][2] : "-"}); released, it closes (${ex.web_test_mod()})`,
        slot >= 0 && s[slot][2] === 10 && ex.web_test_mod() === 0 && ex.web_test_slot(slot, 2) === 10);
  ex.web_buttons(1 << B.REC);                   // a slot left at 0 goes when the layer closes
  play(ex, 0.6);
  ex.web_keys(1 << 6);                          // B3: LFO 2
  play(ex, 0.03);
  ex.web_keys(0);
  ex.web_enc(E.K2, 1);
  play(ex, 0.12);
  const made = slots(ex).findIndex(([a, d]) => a === SRC.LFO2 && d === pos);
  ex.web_enc(E.K2, -1);
  play(ex, 0.12);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`.. a slot it made (slot ${made + 1}) and left at 0 is freed when it closes`,
        made >= 0 && !slots(ex).some(([a, d]) => a === SRC.LFO2 && d === pos));
  tap(ex, B.REC);                               // (spaced: a single tap does nothing)
  const one = ex.web_test_mod();
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.08);
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.08);
  const latched = ex.web_test_mod();
  tap(ex, B.REC);
  check(`REC: a tap nothing (${one}), a double tap latches the layer (${latched}), a tap lets it go (${ex.web_test_mod()})`,
        one === 0 && latched === 2 && ex.web_test_mod() === 0);
}
{                                               // the MENU, its palettes
  ex.web_buttons(1 << B.HOME);
  play(ex, 0.6);
  ex.web_buttons(0);
  play(ex, 0.05);
  const sec = ex.web_test_page_section(), p0 = ex.web_test_palette(-1);
  const screen = () => new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).slice();
  const before = screen();
  ex.web_enc(E.K1, 1);
  play(ex, 0.05);
  const p1 = ex.web_test_palette(-1), moved = screen().filter((v, i) => v !== before[i]).length;
  ex.web_enc(E.K1, -1);
  play(ex, 0.05);
  check(`HOME held: the MENU (section ${sec}); KNOB 1 changes the palette (${p0} -> ${p1} -> ${ex.web_test_palette(-1)}), ${moved} pixels with it`,
        sec === SEC.MENU && p0 === 0 && p1 === 1 && ex.web_test_palette(-1) === 0 && moved > 2000);
  tap(ex, B.EDIT);
}
{
  ex.web_midi(0x90, 64, 100);
  const m = play(ex, 0.3), v = ex.web_test_voices();
  ex.web_midi(0x80, 64, 0);
  play(ex, 1.5);
  check(`Web MIDI plays (peak ${m.peak.toFixed(3)}, ${v} voice) and its note off ends it`,
        m.peak > 0.02 && v === 1 && ex.web_test_voices() === 0);
  const values = () => Array.from({ length: 512 }, (_, id) => ex.web_test_value(id));
  const changed = (a, b) => a.filter((x, id) => x !== b[id]).length;
  const cc = (n, val) => { ex.web_midi(0xB0, n, val); play(ex, 0.03); return values(); };
  const v0 = values(), v1 = cc(0, 0), v2 = cc(0, 127);  // bank select, both ends: no parameter's (the table's 0: none)
  const v3 = cc(74, 0);                                 // the cutoff's CC
  check(`MIDI CC: 0 (bank select) sets nothing (${changed(v0, v1)}, ${changed(v1, v2)} changed), 74 the cutoff (${changed(v2, v3)})`,
        changed(v0, v1) === 0 && changed(v1, v2) === 0 && changed(v2, v3) === 1);
}
{
  const a = await boot(), b = await boot();
  for (const x of [a, b]) {
    play(x, 0.1);
    x.web_keys(1 << 12);
    x.web_enc(E.K2, -4);
  }
  const ha = play(a, 0.5).h, hb = play(b, 0.5).h;
  check("the same input twice: the same sound, bit for bit", ha === hb);
}

console.log(`emu: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
