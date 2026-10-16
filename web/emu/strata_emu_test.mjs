// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// STRATA on the simulated FM-1 (build/emu_strata/strata.wasm, web/emu/strata_web.c) in Node, driven as the page and
// the worklet drive it: no imports; it boots and draws; a key plays and its release ends; the LEDs; knobs (the WAVE
// page: KNOB 1 WT POS, KNOB 2 TABLE; a list one entry a detent; a value speed-sensitive: a slow turn 0.1 % a detent, a
// quick one more); the families (FX, ALGORITHM, SELECT); PRESETS; the MOD layer (REC held: a white key picks the
// source, a knob its depth, a matrix slot made; at 0 it goes when the layer closes; a double tap latches it, a tap
// lets it go); the MENU (HOME held) and its palettes; OCT- / OCT+ with a family button held (the section's pages,
// round; the octave stays; no tap, no layer after) and alone (the octave); an LFO's POINTS page (KNOB 1 picks a point,
// round; KNOB 2-4 its place, level, curve; the first edit draws from the shape; LFO twice adds a point halfway on the
// curve; 16 at most; nothing in the MOD layer; LFO held 5 s deletes the point picked, a countdown first, stopped by a
// release or a knob, the end points kept); SAVE + OCT- / OCT+ (undo, redo: a gesture at a time, a sound's load);
// SYNC FIXED (RATE and the delay's TIME step note values); GLO's GLOBAL page (BPM 20-300, TS-BEAT 1-16, TS-NOTE 1 2
// 4 8 16); a screen region drawn only when it changes; FX (six effects, the order, the EQ's pages), the chorus's
// taps and HOME's scope live only with sound; the warp drawn; SAVE (a name, a version, over), the browser, the
// flash's allow-list, a power cycle; every page drawing nothing in silence, a note held sending only the rows that
// change (the SPI's share); Web MIDI and its CCs; the same input twice, the same sound. SAVE: once a new version at
// once, held 5 s over its own slot (SAVE + OCT: undo, not that), twice the browser (SAVE there: NAME IT, a rename).
//   node web/emu/strata_emu_test.mjs [build/emu_strata/strata.wasm]      (from the repo root, after build_strata.sh)
import { readFileSync } from "node:fs";

const bytes = readFileSync(process.argv[2] || "build/emu_strata/strata.wasm");
let checks = 0, failed = 0;
function check(what, ok) {
  checks++;
  if (!ok) failed++;
  console.log(`emu: ${what.padEnd(100)} ${ok ? "ok" : "FAIL"}`);
}
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12,
            OCTUP: 13 };                                                                       // panel.c B_*
const E = { SELECT: 0, ALGO: 1, PRESET: 2, K1: 3, K2: 4, K3: 5, K4: 6 };                    // panel.c EN_*
const SEC = { OSCA: 0, LFO1: 7, LFO2: 8, MACRO: 10, MATRIX: 11, DRIVE: 12, CHORUS: 13, DELAY: 14, OTT: 16, EQ: 17,
              MENU: 18, GLOBAL: 19, SAVE: 20, BROWSE: 21 };                                   // SEC_*
const ID = { A_TABLE: 1, A_POS: 2, A_WARP: 3, A_WAMT: 4, L1_SHAPE: 78, L1_NOTE: 166, DL_NOTE: 168,   // sy_param.c SP_*
             EQ_ON: 169, EQ_LOW: 170, EQ_MID: 171, EQ_HIGH: 172, EQ_LFREQ: 173, EQ_MFREQ: 174, EQ_Q: 175, EQ_HFREQ: 176,
             FX_ORDER: 177, EQ_LQ: 178, EQ_HQ: 179, EQPT: 1009, EQF: 1010, EQG: 1011, EQQ: 1012,
             LPT: 1002, LX: 1003, LY: 1004, LC: 1005, BPM: 1006, TSB: 1007, TSN: 1008, DRAWN: 12 };  // sy_ui.c UI_*, LFO_DRAW
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

const [POS, TABLE] = [ex.web_test_page_param(0), ex.web_test_page_param(1)];
check(`the WAVE page: KNOB 1 WT POS (${POS}), KNOB 2 TABLE (${TABLE}), KNOB 3 and 4 WARP and AMOUNT`,
      POS === ID.A_POS && TABLE === ID.A_TABLE && ex.web_test_page_param(2) === 3 && ex.web_test_page_param(3) === 4);
{                                               // KNOB 2, a list: one entry a detent
  const v0 = ex.web_test_value(TABLE), d0 = ex.web_screen_draws();
  ex.web_enc(E.K2, 3);
  play(ex, 0.03);
  check(`KNOB 2 turns the table one entry a detent (${v0} -> ${ex.web_test_value(TABLE)}), ${ex.web_screen_draws() - d0} regions redrawn`,
        ex.web_test_value(TABLE) === v0 + 3 && ex.web_screen_draws() - d0 <= 3);
}
{                                               // KNOB 1, a value: slow detents 0.1 % each, quick ones more
  const v0 = ex.web_test_value(POS);            // (SUPERSAW's WT POS is at its top: turned down)
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K1, -1); play(ex, 0.12); }
  const v1 = ex.web_test_value(POS);
  for (let i = 0; i < 6; i++) { ex.web_enc(E.K1, -1); play(ex, 0.004); }
  const v2 = ex.web_test_value(POS);
  check(`KNOB 1 (WT POS) speed-sensitive: 3 slow detents ${v1 - v0} (0.1 % each), 6 quick ones ${v2 - v1}`,
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
  const pos = ex.web_test_page_param(0);
  ex.web_buttons(1 << B.REC);
  play(ex, 0.6);
  const open = ex.web_test_mod();
  ex.web_keys(1 << 4);                          // A3, a white key: LFO 1
  play(ex, 0.03);
  ex.web_keys(0);
  play(ex, 0.03);
  const src = ex.web_test_mod_src(), quiet2 = ex.web_test_voices();
  for (let i = 0; i < 5; i++) { ex.web_enc(E.K1, 1); play(ex, 0.12); }
  const s = slots(ex), slot = s.findIndex(([a, d]) => a === SRC.LFO1 && d === pos);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`REC held: the MOD layer (${open}); A3 picks LFO 1 (${src}), plays nothing (${quiet2} voices)`,
        open === 1 && src === SRC.LFO1 && quiet2 === 0);
  check(`.. KNOB 1 sets LFO 1's depth on WT POS (slot ${slot + 1}: ${slot >= 0 ? s[slot][2] : "-"}); released, it closes (${ex.web_test_mod()})`,
        slot >= 0 && s[slot][2] === 10 && ex.web_test_mod() === 0 && ex.web_test_slot(slot, 2) === 10);
  ex.web_buttons(1 << B.REC);                   // a slot left at 0 goes when the layer closes
  play(ex, 0.6);
  ex.web_keys(1 << 6);                          // B3: LFO 2
  play(ex, 0.03);
  ex.web_keys(0);
  ex.web_enc(E.K1, 1);
  play(ex, 0.12);
  const made = slots(ex).findIndex(([a, d]) => a === SRC.LFO2 && d === pos);
  ex.web_enc(E.K1, -1);
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
{                                               // the SETTINGS (HOME held 5 s), its palettes
  const toast = () => cstr(ex, ex.web_test_toast()), page0 = ex.web_test_page();
  ex.web_buttons(1 << B.HOME);
  play(ex, 1.5);
  const count = toast(), early = ex.web_test_page_section();
  ex.web_buttons(0);
  play(ex, 0.1);
  const after = [ex.web_test_page(), toast()];
  check(`HOME held: a countdown ("${count}"); let go at 1.5 s, nothing opens (section ${early}, the page kept, the footer "${after[1]}")`,
        count === "HOLD: SETTINGS IN 4" && early !== SEC.MENU && after[0] === page0 && after[1] === "");
  ex.web_buttons(1 << B.HOME);
  play(ex, 5.2);
  ex.web_buttons(0);
  play(ex, 0.05);
  const sec = ex.web_test_page_section(), p0 = ex.web_test_palette(-1);
  const screen = () => new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).slice();
  const before = screen();
  ex.web_enc(E.K1, 1);
  play(ex, 0.05);
  const p1 = ex.web_test_palette(-1), moved = screen().filter((v, i) => v !== before[i]).length;
  ex.web_enc(E.K1, -2);
  play(ex, 0.05);
  const p7 = ex.web_test_palette(-1), mono = cstr(ex, ex.web_test_palette_name());
  const grey = screen().filter((v, i) => {                 // (outside the window: its swatches show the others)
    const s = ((v & 0xFF) << 8) | (v >> 8), r = s >> 11, g = (s >> 5) & 63, b = s & 31, y = Math.floor(i / 240);
    return (y < 22 || y >= 136) && (Math.abs(r * 2 - g) > 2 || Math.abs(b * 2 - g) > 2);
  }).length;
  ex.web_enc(E.K1, 1);
  play(ex, 0.05);
  check(`HOME held 5 s: the SETTINGS (section ${sec}); KNOB 1 changes the palette (${p0} -> ${p1}, ${moved} pixels with it); the eighth, ${mono} (${p7}): ${grey} pixels not grey`,
        sec === SEC.MENU && p0 === 0 && p1 === 1 && moved > 2000 && p7 === 7 && mono === "MONO" && grey === 0 &&
        ex.web_test_palette(-1) === 0);
  tap(ex, B.EDIT);
}
const name = () => cstr(ex, ex.web_test_page_name());
const chord = (held, b) => { ex.web_buttons((1 << held) | (1 << b)); play(ex, 0.03); ex.web_buttons(1 << held); play(ex, 0.03); };
{                                               // OCT- / OCT+ with a family button held: the section's pages
  const oct0 = ex.web_test_oct(), p0 = name();
  ex.web_buttons(1 << B.EDIT);
  play(ex, 0.05);
  chord(B.EDIT, B.OCTUP);
  const p1 = name();
  chord(B.EDIT, B.OCTDN);
  const p2 = name();
  chord(B.EDIT, B.OCTDN);
  const p3 = name(), s3 = ex.web_test_page_section();
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`EDIT held: OCT+ ${p0} -> ${p1}, OCT- -> ${p2}, OCT- round -> ${p3} (OSC A); EDIT let go: no tap (${name()}); the octave stays (${ex.web_test_oct()})`,
        p0 === "WAVE" && p1 === "UNISON" && p2 === "WAVE" && p3 === "OUTPUT" && s3 === SEC.OSCA && name() === "OUTPUT" &&
        ex.web_test_oct() === oct0);
  ex.web_buttons(1 << B.ENV);                   // ENV held past its hold time: no ENV 2 layer after a chord
  play(ex, 0.05);
  chord(B.ENV, B.OCTUP);
  play(ex, 0.6);
  const layer = ex.web_test_mod(), p4 = name();
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`ENV held: OCT+ steps the section shown (OUTPUT -> ${p4}); held on, no MOD layer (${layer}); let go, no tap (${name()})`,
        p4 === "WAVE" && layer === 0 && name() === "WAVE" && ex.web_test_page_section() === SEC.OSCA);
  ex.web_buttons(1 << B.OCTUP); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.03);
  const o1 = ex.web_test_oct();
  ex.web_buttons(1 << B.OCTDN); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.03);
  check(`OCT+ alone: the keys' octave (${oct0} -> ${o1} -> ${ex.web_test_oct()}), the page stays (${name()})`,
        o1 === oct0 + 1 && ex.web_test_oct() === oct0 && name() === "WAVE");
}
{                                               // an LFO's POINTS page
  const twice = (b) => { for (let i = 0; i < 2; i++) { ex.web_buttons(1 << b); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.08); } play(ex, 0.3); };
  const pt = (i) => [1, 2, 3].map((f) => ex.web_test_draw(0, i, f));
  tap(ex, B.LFO);
  const sec = ex.web_test_page_section(), pg = name(), items = [0, 1, 2, 3].map((k) => ex.web_test_page_param(k));
  ex.web_enc(E.SELECT, 1);
  play(ex, 0.03);
  const SHAPE = ex.web_test_page_param(0), shape0 = ex.web_test_value(SHAPE);
  ex.web_enc(E.SELECT, -1);
  play(ex, 0.03);
  check(`LFO: LFO 1 ${pg} first (section ${sec}), KNOB 1-4 the point, its place, level, curve (${items}); SHAPE ${shape0}`,
        sec === SEC.LFO1 && pg === "POINTS" && items.join() === [ID.LPT, ID.LX, ID.LY, ID.LC].join() && shape0 === 0);
  ex.web_enc(E.K1, 2);
  play(ex, 0.03);
  const l1 = ex.web_test_lpt(0);
  ex.web_enc(E.K1, -3);
  play(ex, 0.03);
  const l2 = ex.web_test_lpt(0);
  ex.web_enc(E.K1, -3);
  play(ex, 0.03);
  check(`KNOB 1 picks a point, round: +2 -> ${l1 + 1}, -3 -> ${l2 + 1} of 5; the shape untouched (SHAPE ${ex.web_test_value(SHAPE)})`,
        l1 === 2 && l2 === 4 && ex.web_test_lpt(0) === 1 && ex.web_test_value(SHAPE) === 0);
  const before = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).slice();
  for (let i = 0; i < 5; i++) { ex.web_enc(E.K3, -1); play(ex, 0.12); }
  const moved = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).filter((v, i) => v !== before[i]).length;
  check(`KNOB 3 lowers point 2 (y ${pt(1)[1]}): the sine's points drawn (SHAPE ${ex.web_test_value(SHAPE)}, ${ex.web_test_draw(0, 0, 0)} points); ${moved} pixels`,
        ex.web_test_value(SHAPE) === ID.DRAWN && ex.web_test_draw(0, 0, 0) === 5 && pt(1)[1] === 990 && pt(1)[0] === 250 && moved > 100);
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K2, 1); play(ex, 0.12); }
  const x1 = pt(1)[0];
  for (let i = 0; i < 30; i++) { ex.web_enc(E.K2, 1); play(ex, 0.004); }
  const x2 = pt(1)[0];
  ex.web_enc(E.K1, 3);                          // the last point: its place stays at the end
  play(ex, 0.03);
  ex.web_enc(E.K2, -5);
  play(ex, 0.03);
  const c0 = ex.web_test_draw(0, 3, 3);
  for (let i = 0; i < 4; i++) { ex.web_enc(E.K4, 1); play(ex, 0.12); }
  check(`KNOB 2 moves it (250 -> ${x1}), quickly up to the next point (${x2}); the last stays at the end (${pt(4)[0]}); KNOB 4 there curves the segment into it (${c0} -> ${ex.web_test_draw(0, 3, 3)})`,
        x1 === 253 && x2 === 500 && pt(4)[0] === 1000 && ex.web_test_draw(0, 3, 3) === c0 + 8);
  ex.web_enc(E.K1, -1);                         // point 4 (at 750, point 3 at 500)
  play(ex, 0.03);
  const [xa, xb] = [pt(1)[0], pt(2)[0]], n0 = ex.web_test_draw(0, 0, 0);
  twice(B.LFO);
  const n1 = ex.web_test_draw(0, 0, 0), nw = ex.web_test_lpt(0), [xn, yn] = pt(nw);
  check(`LFO twice on POINTS: a point (${n0} -> ${n1}) left of the one picked, at ${xn} (halfway ${xb} .. ${pt(nw + 1)[0]}), picked (${nw + 1}); the page stays (${name()})`,
        n1 === n0 + 1 && nw === 3 && xn === Math.floor((xb + pt(nw + 1)[0]) / 2) && name() === "POINTS" &&
        yn <= Math.max(pt(nw - 1)[1], pt(nw + 1)[1]) && yn >= Math.min(pt(nw - 1)[1], pt(nw + 1)[1]));
  ex.web_enc(E.K1, -nw);                        // the first point picked: the new one right of it
  play(ex, 0.03);
  twice(B.LFO);
  check(`.. the first point picked: the new one right of it (point 2 at ${pt(1)[0]}, halfway 0 .. ${xa}), ${ex.web_test_draw(0, 0, 0)} points`,
        ex.web_test_draw(0, 0, 0) === n1 + 1 && ex.web_test_lpt(0) === 1 && pt(1)[0] === Math.floor(xa / 2));
  tap(ex, B.EDIT);                              // twice from another family: to the LFO, no point
  const n2 = ex.web_test_draw(0, 0, 0);
  twice(B.LFO);
  check(`.. LFO twice from another page: no point (${n2} -> ${ex.web_test_draw(0, 0, 0)}), the LFO's next page (${name()})`,
        ex.web_test_draw(0, 0, 0) === n2 && name() === "SHAPE");
  ex.web_enc(E.K1, 8 - ex.web_test_value(SHAPE));   // SHAPE STAIRS: 16 points, no 17th
  play(ex, 0.03);
  ex.web_enc(E.SELECT, -1);
  play(ex, 0.03);
  twice(B.LFO);
  check(`STAIRS shown (16 points): LFO twice adds none (SHAPE ${ex.web_test_value(SHAPE)}, drawn ${ex.web_test_draw(0, 0, 0)} points)`,
        ex.web_test_value(SHAPE) === 8 && ex.web_test_draw(0, 0, 0) === n2 && name() === "POINTS");
  ex.web_buttons(1 << B.REC);                   // the MOD layer: the points stay
  play(ex, 0.6);
  ex.web_enc(E.K3, 5);
  play(ex, 0.12);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`the MOD layer on POINTS: KNOB 3 changes nothing (SHAPE ${ex.web_test_value(SHAPE)}, ${ex.web_test_draw(0, 0, 0)} points)`,
        ex.web_test_value(SHAPE) === 8 && ex.web_test_draw(0, 0, 0) === n2);
  tap(ex, B.EDIT);
}
{                                               // LFO held 5 s on POINTS: the point picked goes
  const n = () => ex.web_test_draw(0, 0, 0), toast = () => cstr(ex, ex.web_test_toast());
  tap(ex, B.LFO);                               // (POINTS: STAIRS shown, its 16 points)
  ex.web_enc(E.K1, 2 - ex.web_test_lpt(0));     // point 3
  play(ex, 0.03);
  const shape0 = ex.web_test_value(ID.L1_SHAPE), n0 = n();
  ex.web_buttons(1 << B.LFO);
  play(ex, 1.5);
  const count = toast();
  play(ex, 0.5);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`LFO held on POINTS: a countdown ("${count}"); let go at 2 s, nothing deleted (SHAPE ${ex.web_test_value(ID.L1_SHAPE)}, the footer "${toast()}")`,
        count === "HOLD: DELETE POINT 3 IN 4" && ex.web_test_value(ID.L1_SHAPE) === shape0 && n() === n0 && toast() === "");
  ex.web_buttons(1 << B.LFO);
  play(ex, 5.3);
  const gone = toast();
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`.. held 5 s: point 3 goes ("${gone}"): STAIRS drawn (SHAPE ${ex.web_test_value(ID.L1_SHAPE)}), ${n()} points, point ${ex.web_test_lpt(0) + 1} picked; the page stays (${name()})`,
        gone === "POINT 3 DELETED" && ex.web_test_value(ID.L1_SHAPE) === ID.DRAWN && n() === 15 && ex.web_test_lpt(0) === 1 &&
        ex.web_test_draw(0, 2, 1) === 250 && name() === "POINTS");
  ex.web_enc(E.K1, -1);                         // the first point: it stays
  play(ex, 0.03);
  ex.web_buttons(1 << B.LFO);
  play(ex, 1.0);
  const ends = toast();
  play(ex, 4.5);
  ex.web_buttons(0);
  play(ex, 0.1);
  const n1 = n();
  ex.web_enc(E.K1, 4);                          // a knob turned during the countdown stops it
  play(ex, 0.03);
  ex.web_buttons(1 << B.LFO);
  play(ex, 2.0);
  ex.web_enc(E.K3, 1);
  play(ex, 3.5);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`.. the first point picked: "${ends}", none deleted (${n1}); a knob turned during the countdown stops it (${n()} points)`,
        ends === "THE END POINTS STAY" && n1 === 15 && n() === 15);
  ex.web_buttons(1 << B.SAVE);                  // SAVE + OCT-: undo (the level KNOB 3 set, then the delete)
  play(ex, 0.05);
  chord(B.SAVE, B.OCTDN);
  chord(B.SAVE, B.OCTDN);
  const undone = [toast(), ex.web_test_value(ID.L1_SHAPE), n()];
  chord(B.SAVE, B.OCTUP);
  const redone = [toast(), ex.web_test_value(ID.L1_SHAPE), n()];
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`SAVE + OCT- twice: the delete undone ("${undone[0]}": SHAPE ${undone[1]} STAIRS, the drawn shape of ${undone[2]}); SAVE + OCT+: redone ("${redone[0]}": SHAPE ${redone[1]}, ${redone[2]} points); SAVE let go: no page, no octave`,
        undone[0] === "UNDO" && undone[1] === 8 && undone[2] === 7 && redone[0] === "REDO" && redone[1] === ID.DRAWN &&
        redone[2] === 15 && name() === "POINTS" && ex.web_test_oct() === 0);
}
{                                               // SYNC FREE / FIXED: RATE in Hz or a note value
  ex.web_enc(E.SELECT, 1);                      // LFO 1 SHAPE
  play(ex, 0.03);
  const [SYNC, RATE] = [ex.web_test_page_param(1), ex.web_test_page_param(2)];
  const r0 = ex.web_test_value(RATE), q0 = ex.web_test_value(ID.L1_NOTE), s0 = ex.web_test_value(SYNC);
  ex.web_enc(E.K2, 1);
  play(ex, 0.03);
  ex.web_enc(E.K3, 2);
  play(ex, 0.03);
  const fixed = [ex.web_test_value(SYNC), ex.web_test_value(ID.L1_NOTE), ex.web_test_value(RATE)];
  ex.web_enc(E.K2, -1);
  play(ex, 0.03);
  ex.web_enc(E.K3, 2);
  play(ex, 0.03);
  check(`LFO 1 SHAPE: SYNC ${s0} -> FIXED (${fixed[0]}): KNOB 3 steps the note value (1/4: ${q0} -> ${fixed[1]}, RATE stays ${fixed[2]}); FREE: KNOB 3 the rate in Hz (${r0} -> ${ex.web_test_value(RATE)}), the note kept`,
        s0 === 0 && fixed[0] === 1 && q0 === 8 && fixed[1] === 10 && fixed[2] === r0 && ex.web_test_value(RATE) === r0 + 2 &&
        ex.web_test_value(ID.L1_NOTE) === 10);
  tap(ex, B.FX);                                // the delay (FIXED 1/8D by default)
  ex.web_enc(E.ALGO, 2 - (ex.web_test_page_section() - SEC.DRIVE));
  play(ex, 0.03);
  const dsec = ex.web_test_page_section(), [DSYNC, DTIME] = [ex.web_test_page_param(1), ex.web_test_page_param(2)];
  const t0 = ex.web_test_value(DTIME), d0 = ex.web_test_value(ID.DL_NOTE), ds0 = ex.web_test_value(DSYNC);
  ex.web_enc(E.K3, 1);
  play(ex, 0.03);
  const d1 = ex.web_test_value(ID.DL_NOTE);
  ex.web_enc(E.K2, -1);
  play(ex, 0.03);
  ex.web_enc(E.K3, 1);
  play(ex, 0.03);
  check(`DELAY (section ${dsec}): SYNC FIXED (${ds0}): KNOB 3 steps the note (1/8D ${d0} -> ${d1}); FREE: KNOB 3 the time in ms (${t0} -> ${ex.web_test_value(DTIME)})`,
        dsec === SEC.DELAY && ds0 === 1 && d0 === 10 && d1 === 11 && ex.web_test_value(DTIME) === t0 + 1 &&
        ex.web_test_value(ID.DL_NOTE) === 11);
}
{                                               // GLO: GLOBAL's tempo and time signature
  tap(ex, B.GLO);
  tap(ex, B.GLO);
  tap(ex, B.GLO);
  const sec = ex.web_test_page_section(), items = [0, 1, 2, 3].map((k) => ex.web_test_page_param(k));
  const t = () => [0, 1, 2, 3].map((f) => ex.web_test_tempo(f));
  const a = t();
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K1, 1); play(ex, 0.12); }
  const b = t();
  for (let i = 0; i < 40; i++) { ex.web_enc(E.K1, -1); play(ex, 0.004); }
  const lo = t()[0];
  for (let i = 0; i < 40; i++) { ex.web_enc(E.K1, 1); play(ex, 0.004); }
  const hi = t()[0];
  ex.web_enc(E.K2, 3); play(ex, 0.03);
  const tsb = t()[1];
  ex.web_enc(E.K2, -20); play(ex, 0.03);
  const tsb1 = t()[1];
  ex.web_enc(E.K2, 30); play(ex, 0.03);
  const tsb16 = t()[1];
  ex.web_enc(E.K3, 1); play(ex, 0.03);
  const n8 = t();
  ex.web_enc(E.K3, 9); play(ex, 0.03);
  const n16 = t()[2];
  ex.web_enc(E.K3, -9); play(ex, 0.03);
  const n1 = t()[2];
  check(`GLO three taps: GLOBAL (section ${sec}) ${name()} (${items}): ${a[0]} BPM ${a[1]}/${a[2]}`,
        sec === SEC.GLOBAL && name() === "TEMPO" && items.join() === [ID.BPM, ID.TSB, ID.TSN, -1].join() &&
        a[0] === 120 && a[1] === 4 && a[2] === 4 && a[3] === 12000);
  check(`.. BPM: 3 slow detents ${a[0]} -> ${b[0]}, held between 20 and 300 (${lo}, ${hi}); TS-BEAT 1-16 (${tsb}, ${tsb1}, ${tsb16}); TS-NOTE 1 2 4 8 16 (${n8[2]}, ${n16}, ${n1}); 300 BPM in eighths: ${n8[3] / 100} quarter notes a minute`,
        b[0] === 123 && lo === 20 && hi === 300 && tsb === 7 && tsb1 === 1 && tsb16 === 16 && n8[2] === 8 &&
        n8[3] === 15000 && n16 === 16 && n1 === 1);
  ex.web_enc(E.K2, -12); play(ex, 0.03);
  ex.web_enc(E.K3, 2); play(ex, 0.03);
  check(`.. back to 4/4 (${t()[1]}/${t()[2]}): 300 BPM, ${t()[3] / 100} quarter notes a minute`, t().join() === [300, 4, 4, 30000].join());
  tap(ex, B.EDIT);
}
{                                               // undo, redo: a knob's gesture, a sound's load
  const toast = () => cstr(ex, ex.web_test_toast()), pos = ex.web_test_page_param(0), v0 = ex.web_test_value(pos);
  const undo = (b) => { ex.web_buttons(1 << B.SAVE); play(ex, 0.05); chord(B.SAVE, b); ex.web_buttons(0); play(ex, 0.4); };
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K1, 1); play(ex, 0.12); }
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K1, 1); play(ex, 0.12); }
  const v1 = ex.web_test_value(pos);
  play(ex, 1.2);
  ex.web_enc(E.K1, 1);
  play(ex, 0.12);
  const v2 = ex.web_test_value(pos);
  undo(B.OCTDN);
  const u1 = ex.web_test_value(pos);
  undo(B.OCTDN);
  const u2 = [ex.web_test_value(pos), toast()];
  undo(B.OCTUP);
  check(`SAVE + OCT-: a gesture at a time (WT POS ${v0} -> ${v1}, a second later ${v2}; undone ${u1}, ${u2[0]}, "${u2[1]}"); SAVE + OCT+ redoes (${ex.web_test_value(pos)})`,
        v1 === v0 + 6 && v2 === v1 + 1 && u1 === v1 && u2[0] === v0 && u2[1] === "UNDO" && ex.web_test_value(pos) === v1);
  const s0 = cstr(ex, ex.web_test_patch_name());
  ex.web_enc(E.PRESET, 1);
  play(ex, 0.3);
  const s1 = cstr(ex, ex.web_test_patch_name());
  undo(B.OCTDN);
  check(`.. a sound's load undone ("${s0}" -> "${s1}" -> "${cstr(ex, ex.web_test_patch_name())}", WT POS ${ex.web_test_value(pos)})`,
        s1 !== s0 && cstr(ex, ex.web_test_patch_name()) === s0 && ex.web_test_value(pos) === v1);
}
const scr = () => new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240).slice();
const changed = (a, b, y0 = 0, y1 = 240) => a.filter((v, i) => v !== b[i] && i >= y0 * 240 && i < y1 * 240).length;
{                                               // FX: six effects, the EQ's pages; a knob turning lit, the curve before it kept
  tap(ex, B.FX);                                // (FX shows the effect it showed last: DRIVE first)
  ex.web_enc(E.ALGO, SEC.DRIVE - ex.web_test_page_section());
  play(ex, 0.03);
  const secs = [];
  for (let i = 0; i < 6; i++) { secs.push(ex.web_test_page_section()); ex.web_enc(E.ALGO, 1); play(ex, 0.03); }
  const round = ex.web_test_page_section();
  ex.web_enc(E.ALGO, -1);
  play(ex, 0.03);
  const pg = () => [name(), ...[0, 1, 2, 3].map((k) => ex.web_test_page_param(k))];
  const eq1 = pg();
  ex.web_enc(E.SELECT, 1);
  play(ex, 0.03);
  const eq2 = pg();
  ex.web_enc(E.SELECT, -1);
  play(ex, 0.03);
  check(`FX: ALGORITHM through six effects (${secs}), round (${round}); the EQ's pages ${eq1[0]} (${eq1.slice(1)}), ${eq2[0]} (${eq2.slice(1)})`,
        secs.join() === [12, 13, 14, 15, 16, 17].join() && round === SEC.DRIVE && eq1[0] === "POINTS" &&
        eq1.slice(1).join() === [ID.EQPT, ID.EQF, ID.EQG, ID.EQQ].join() && eq2[0] === "GAINS" &&
        eq2.slice(1).join() === [ID.EQ_LOW, ID.EQ_MID, ID.EQ_HIGH, ID.EQ_ON].join());
  ex.web_enc(E.K1, 1);                          // POINTS: MID picked, its GAIN (KNOB 3) turned
  play(ex, 0.12);
  const before = scr();
  for (let i = 0; i < 3; i++) { ex.web_enc(E.K3, 1); play(ex, 0.12); }
  const act = ex.web_test_act(), ghost = ex.web_test_ghost(), mid = ex.web_test_value(ID.EQ_MID), lit = scr();
  ex.web_enc(E.K4, 2);                          // .. its Q, and the shelves' (LOW: LO Q)
  play(ex, 0.12);
  const q = ex.web_test_value(ID.EQ_Q);
  ex.web_enc(E.K1, -1);
  play(ex, 0.03);
  ex.web_enc(E.K4, 1);
  play(ex, 0.12);
  const lq = ex.web_test_value(ID.EQ_LQ);
  play(ex, 1.2);
  const act2 = ex.web_test_act(), ghost2 = ex.web_test_ghost(), rest = scr();
  check(`EQ POINTS: MID picked, KNOB 3 its GAIN (${mid}), its cell lit (knob ${act + 1}), the curve before kept (${ghost}), ${changed(before, lit)} pixels; KNOB 4 its Q (${q}); LOW's: LO Q (${lq}); a second on, let go (${act2 === 255 ? "none" : act2}, ${ghost2}), ${changed(lit, rest)} pixels back`,
        mid === 3 && act === 2 && ghost === 1 && changed(before, lit) > 200 && q === 402 && lq === 301 &&
        act2 === 255 && ghost2 === 0 && changed(lit, rest) > 200);
  ex.web_enc(E.K1, 1);                          // the MOD layer on POINTS: KNOB 3 sets LFO 1's depth on MID's GAIN
  play(ex, 0.03);
  ex.web_buttons(1 << B.REC);
  play(ex, 0.6);
  ex.web_keys(1 << 4);                          // A3: LFO 1
  play(ex, 0.03);
  ex.web_keys(0);
  for (let i = 0; i < 5; i++) { ex.web_enc(E.K3, 1); play(ex, 0.12); }
  const sl = slots(ex).findIndex(([a, d]) => a === SRC.LFO1 && d === ID.EQ_MID);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`.. the MOD layer on POINTS: KNOB 3 sets LFO 1's depth on MID's GAIN (slot ${sl + 1}: ${sl >= 0 ? ex.web_test_slot(sl, 2) : "-"})`,
        sl >= 0 && ex.web_test_slot(sl, 2) === 10);
  for (const [id, v] of [[ID.EQ_MID, 0], [ID.EQ_Q, 400], [ID.EQ_LQ, 300], [106 + 3 * sl, 0], [107 + 3 * sl, 0], [108 + 3 * sl, 0]])
    ex.web_test_set(id, v);
  ex.web_enc(E.ALGO, SEC.DRIVE - ex.web_test_page_section());   // FX held + ALGORITHM: DRIVE along the rack
  play(ex, 0.03);
  const p0 = ex.web_test_page();
  ex.web_buttons(1 << B.FX);
  play(ex, 0.05);
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.05);
  const moved = ex.web_test_value(ID.FX_ORDER), toast = cstr(ex, ex.web_test_toast());
  ex.web_buttons(0);
  play(ex, 0.4);
  const stay = ex.web_test_page();
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.03);
  const next = ex.web_test_page_section();
  ex.web_enc(E.ALGO, -2);
  play(ex, 0.03);
  const first = ex.web_test_page_section();
  check(`FX held + ALGORITHM on DRIVE: DRIVE second (FX ORDER ${moved}, "${toast}"); FX let go, no page step; ALGORITHM after DRIVE: DELAY (${next}); two back, CHORUS first (${first})`,
        moved === 120 && toast === "DRIVE: 2 OF 6" && stay === p0 && next === SEC.DELAY && first === SEC.CHORUS);
  ex.web_test_set(ID.FX_ORDER, 0);
  play(ex, 0.1);
  tap(ex, B.EDIT);
}
{                                               // SEQ: the matrix, a slot at a time (a fresh unit)
  const m = await boot(), mscr = () => new Uint16Array(m.memory.buffer, m.web_screen(), 240 * 240).slice();
  const mtap = (b) => { m.web_buttons(1 << b); play(m, 0.03); m.web_buttons(0); play(m, 0.35); };
  play(m, 0.2);
  mtap(B.SEQ);
  const sec = m.web_test_page_section(), pg = cstr(m, m.web_test_page_name()), first = m.web_test_page_virt(1);
  m.web_enc(E.K1, -1);                          // KNOB 1: the slot before slot 1, round: slot 8
  play(m, 0.12);
  const SRC8 = m.web_test_page_virt(1), DST8 = m.web_test_page_virt(2), AMT8 = m.web_test_page_virt(3);
  const s0 = m.web_test_value(SRC8), a = mscr();
  m.web_enc(E.K2, 3);                           // KNOB 2: its source, three on from NONE: LFO 1
  play(m, 0.12);
  m.web_enc(E.K3, 2);                           // KNOB 3: where it goes, the second parameter that can be modulated
  play(m, 0.12);
  for (let i = 0; i < 6; i++) { m.web_enc(E.K4, 1); play(m, 0.004); }   // KNOB 4: how much, quickly
  play(m, 0.1);
  const b = mscr(), src = m.web_test_value(SRC8), dst = m.web_test_value(DST8), amt = m.web_test_value(AMT8);
  check(`SEQ: the matrix (section ${sec}, ${pg}); KNOB 1 picks a slot, round (slot 1's source ${first}, slot 8's ${SRC8}); KNOB 2 its source (${s0} -> ${src}), KNOB 3 where it goes (${dst}), KNOB 4 how much (${amt}); ${changed(a, b, 22, 136)} pixels`,
        sec === SEC.MATRIX && pg === "SLOTS" && SRC8 === first + 21 && DST8 === SRC8 + 1 && AMT8 === SRC8 + 2 &&
        s0 === 0 && src === SRC.LFO1 && dst > 0 && amt > 0 && changed(a, b, 22, 136) > 300);
}
{                                               // the chorus's taps: still in silence, moving with a note
  tap(ex, B.FX);
  ex.web_enc(E.ALGO, SEC.CHORUS - ex.web_test_page_section());
  ex.web_test_set(135, 1);                      // CHORUS ON
  play(ex, 1.6);
  const d0 = ex.web_screen_draws();
  play(ex, 1.0);
  const still = ex.web_screen_draws() - d0;
  ex.web_keys(1 << 7);
  play(ex, 0.5);
  const moving = ex.web_screen_draws() - d0 - still;
  ex.web_keys(0);
  play(ex, 3.0);
  const d1 = ex.web_screen_draws();
  play(ex, 1.0);
  check(`CHORUS on: in silence its taps still (${still} blits in 1 s); a note: moving (${moving} in 0.5 s); after it, still again (${ex.web_screen_draws() - d1})`,
        still === 0 && moving >= 4 && ex.web_screen_draws() - d1 === 0);
  ex.web_test_set(135, 0);
  tap(ex, B.EDIT);
}
{                                               // HOME: the sound and its output, live
  tap(ex, B.HOME);
  play(ex, 1.5);                                // (a word in the footer from before: gone)
  const dq = ex.web_screen_draws();
  play(ex, 1.0);
  const quiet = ex.web_screen_draws() - dq;     // (silence: the scope's flat capture, drawn once)
  const sec = ex.web_test_page_section(), s0 = ex.web_test_scope(), d0 = ex.web_screen_draws();
  ex.web_keys(1 << 7);
  play(ex, 0.5);
  const s1 = ex.web_test_scope(), d1 = ex.web_screen_draws();
  ex.web_keys(0);
  play(ex, 1.5);
  tap(ex, B.EDIT);
  const s2 = ex.web_test_scope();
  play(ex, 0.5);
  check(`HOME (section ${sec}): silence, nothing redrawn (${quiet} blits in 1 s); a note playing, the scope live (${s1 - s0} captures in 0.5 s, ${d1 - d0} blits); another page, none (${ex.web_test_scope() - s2})`,
        sec === SEC.MACRO && quiet === 0 && s1 - s0 >= 8 && d1 - d0 >= 5 && ex.web_test_scope() - s2 <= 1);
}
{                                               // OSC A's WAVE page: the frames drawn through the warp
  const n0 = name();
  ex.web_test_set(ID.A_WARP, 2);                // BEND+
  ex.web_test_set(ID.A_WAMT, 0);
  play(ex, 0.1);
  const a = scr();
  for (let i = 0; i < 12; i++) { ex.web_enc(E.K4, 1); play(ex, 0.004); }
  play(ex, 0.05);
  const b = scr(), amt = ex.web_test_value(ID.A_WAMT), ghost = ex.web_test_ghost();
  check(`${n0}: WARP BEND+, KNOB 4 (AMOUNT ${amt}) draws the table's frames warped (${changed(a, b, 22, 136)} pixels of the display), the frame before the turn kept (${ghost})`,
        n0 === "WAVE" && amt > 100 && changed(a, b, 22, 136) > 1500 && ghost === 1);
  ex.web_test_set(ID.A_WARP, 0);
  ex.web_test_set(ID.A_WAMT, 0);
  play(ex, 1.2);
}
{                                               // SAVE: once, a new preset at once; held 5 s, over its own slot;
  const toast = () => cstr(ex, ex.web_test_toast()), sname = () => cstr(ex, ex.web_test_save_name());   // twice, the
  const key = (k) => { ex.web_keys(1 << k); play(ex, 0.03); ex.web_keys(0); play(ex, 0.03); };          // browser
  const btn = (b, t = 0.03) => { ex.web_buttons(1 << b); play(ex, t); ex.web_buttons(0); play(ex, 0.03); };
  const name = () => cstr(ex, ex.web_test_patch_name()), slotName = (s) => cstr(ex, ex.web_test_slot_name(s));
  const hold = (secs) => { ex.web_buttons(1 << B.SAVE); play(ex, secs); };
  tap(ex, B.EDIT);
  const w0 = ex.web_flash_writes_count(), name0 = name();        // (a starter sound)
  btn(B.SAVE);
  play(ex, 0.5);
  const s1 = [toast(), ex.web_test_page_section(), ex.web_test_user_slot(), slotName(0), name()];
  check(`SAVE once: saved at once as a new preset ("${s1[0]}"): "${name0}" is "${s1[4]}" in USER ${s1[2] + 1}; the page stays (section ${s1[1]}); ${ex.web_flash_writes_count() - w0} flash writes, ${ex.web_test_refused()} refused`,
        s1[0] === `SAVED: ${name0} v2 · USER 1` && s1[1] === SEC.OSCA && s1[2] === 0 && s1[3] === `${name0} v2` &&
        s1[4] === `${name0} v2` && ex.web_flash_writes_count() > w0 && ex.web_test_refused() === 0);
  btn(B.SAVE);
  play(ex, 0.5);
  check(`.. once more: its next version, "${name()}" in USER ${ex.web_test_user_slot() + 1} ("${toast()}"); USER 1 kept ("${slotName(0)}")`,
        name() === `${name0} v3` && ex.web_test_user_slot() === 1 && slotName(0) === `${name0} v2` &&
        slotName(1) === `${name0} v3`);
  const POS = ID.A_POS, p0 = ex.web_test_slot_value(1, POS);    // held: WT POS changed first
  ex.web_test_set(POS, p0 > 500 ? p0 - 40 : p0 + 40);
  const p1 = ex.web_test_value(POS);
  hold(2.3);
  const cd = toast();
  ex.web_buttons(0);
  play(ex, 0.4);
  const early = [ex.web_test_slot_value(1, POS), toast()];
  hold(5.3);
  ex.web_buttons(0);
  play(ex, 0.3);
  check(`SAVE held: a countdown ("${cd}"); let go at 2.3 s, USER 2 untouched (WT POS ${early[0]}, the footer "${early[1]}"); held 5 s, over USER 2 ("${toast()}"): its WT POS ${p0} -> ${ex.web_test_slot_value(1, POS)}`,
        cd === "HOLD: SAVE OVER USER 2 IN 3" && early[0] === p0 && early[1] === "" && toast() === "SAVED OVER USER 2" &&
        ex.web_test_slot_value(1, POS) === p1);
  ex.web_test_set(POS, p1 > 500 ? p1 - 20 : p1 + 20);   // (a change USER 2 does not have)
  const p3 = ex.web_test_value(POS);
  play(ex, 1.2);
  const K1P = ex.web_test_page_param(0), k0 = ex.web_test_value(K1P);
  ex.web_enc(E.K1, 3);                          // KNOB 1: an edit to undo
  play(ex, 1.2);
  const k1 = ex.web_test_value(K1P);
  hold(0.7);
  chord(B.SAVE, B.OCTDN);                       // SAVE + OCT-: undo; the countdown stops
  play(ex, 5.0);
  ex.web_buttons(0);
  play(ex, 0.3);
  check(`.. SAVE held + OCT-: undo (KNOB 1 ${k0} -> ${k1} -> ${ex.web_test_value(K1P)}); held on past 5 s, USER 2 not saved over (its WT POS ${ex.web_test_slot_value(1, POS)}, the sound's ${p3})`,
        k1 !== k0 && ex.web_test_value(K1P) === k0 && p3 !== p1 && ex.web_test_slot_value(1, POS) === p1);
  btn(B.SAVE);                                  // twice: the browser, nothing saved
  btn(B.SAVE);
  play(ex, 0.5);
  const bsec = ex.web_test_page_section(), none = slotName(2);
  ex.web_enc(E.K2, -1);                         // KNOB 2: the preset before (USER 1), loaded; then USER 2 again
  play(ex, 0.3);
  const before = name(), us1 = ex.web_test_user_slot();
  ex.web_enc(E.K2, 1);
  play(ex, 0.3);
  check(`SAVE twice: the browser (section ${bsec}), nothing saved (USER 3 "${none}"); KNOB 2 loads as it turns ("${before}", USER ${us1 + 1}), then "${name()}" from USER ${ex.web_test_user_slot() + 1} (WT POS ${ex.web_test_value(POS)})`,
        bsec === SEC.BROWSE && none === "" && before === `${name0} v2` && us1 === 0 && name() === `${name0} v3` &&
        ex.web_test_user_slot() === 1 && ex.web_test_value(POS) === p1);
  btn(B.SAVE);                                  // SAVE there: NAME IT for the sound playing, in its own slot
  play(ex, 0.5);
  const nsec = ex.web_test_page_section(), n0 = sname(), ns = ex.web_test_save_slot();
  for (let i = 0; i < 16; i++) btn(B.OCTDN);    // OCT-: deleted, then typed: WARM PAD
  for (const k of [22, 0, 17, 12, 26, 15, 0, 3]) key(k);
  const typed = sname();
  btn(B.OCTUP);                                 // OCT+: the numbers; F#3 is 2; OCT- deletes it; OCT+ back
  key(1);
  const two = sname();
  btn(B.OCTDN);
  btn(B.OCTUP);
  ex.web_enc(E.K3, 1);                          // KNOB 3: the category
  play(ex, 0.12);
  btn(B.SAVE);
  play(ex, 0.4);
  const saved = [toast(), ex.web_test_page_section(), slotName(1), name(), ex.web_test_user_slot()];
  check(`.. SAVE there: NAME IT (section ${nsec}), "${n0}" in its own USER ${ns + 1}; OCT- deletes, the keys type "${typed}", OCT+ numbers ("${two}")`,
        nsec === SEC.SAVE && n0 === `${name0} v3` && ns === 1 && typed === "WARM PAD" && two === "WARM PAD2");
  check(`.. SAVE writes it ("${saved[0]}"): USER 2 renamed ("${saved[2]}"), the sound "${saved[3]}"; back in the browser (section ${saved[1]})`,
        saved[0] === "SAVED: WARM PAD · USER 2" && saved[1] === SEC.BROWSE && saved[2] === "WARM PAD" &&
        saved[3] === "WARM PAD" && saved[4] === 1);
  ex.web_enc(E.K2, -1);                         // a load, undone there (SAVE held + OCT-), the browser staying
  play(ex, 0.3);
  const ld = name();
  hold(0.7);
  chord(B.SAVE, B.OCTDN);
  ex.web_buttons(0);
  play(ex, 0.2);
  const und = [toast(), name(), ex.web_test_page_section(), ex.web_test_user_slot()];
  btn(B.SAVE);                                  // twice in the browser: back
  btn(B.SAVE);
  play(ex, 0.3);
  const back1 = ex.web_test_page_section();
  btn(B.SAVE);
  btn(B.SAVE);
  play(ex, 0.3);
  btn(B.HOME);                                  // HOME in it: back
  play(ex, 0.1);
  check(`.. in the browser: SAVE held + OCT- undoes a load ("${ld}" -> "${und[1]}" of USER ${und[3] + 1} again, "${und[0]}", section ${und[2]}); SAVE twice: back (section ${back1}); HOME: back (${ex.web_test_page_section()})`,
        ld === `${name0} v2` && und[0] === "UNDO" && und[1] === "WARM PAD" && und[3] === 1 && und[2] === SEC.BROWSE &&
        back1 === SEC.OSCA && ex.web_test_page_section() === SEC.OSCA);
  ex.web_enc(E.PRESET, 1);                      // PRESETS: from the last user preset round to the first starter
  play(ex, 0.4);
  const st = [name(), ex.web_test_user_slot()];
  hold(1.0);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`SAVE held on a starter sound ("${st[0]}", user slot ${st[1]}): nothing to save over ("${toast()}"), nothing saved (USER 3 "${slotName(2)}")`,
        st[1] === -1 && toast() === "NOTHING TO SAVE OVER: TAP SAVE" && slotName(2) === "");
  const ra = ex.web_test_erase(0), rb = ex.web_test_erase(0xE8000);
  check(`the flash's allow-list: an erase at 0x00000 (the boot) or 0xE8000 (the stock firmware's backup) refused (${ra}, ${rb}; ${ex.web_test_refused()} refused)`,
        ra === -8 && rb === -8 && ex.web_test_refused() === 2);
}
{                                               // power off and on: the presets, the settings, the sound playing come back
  ex.web_test_palette(3);
  play(ex, 3.8);                                // (3 s of quiet: the settings and the sound written)
  const nor = new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const b2 = instance.exports;
  if (b2._initialize) b2._initialize();
  new Uint8Array(b2.memory.buffer, b2.web_nor(), b2.web_nor_size()).set(nor);
  b2.web_master(700);
  b2.web_boot();
  play(b2, 0.3);
  check(`power off and on: USER 1 "${cstr(b2, b2.web_test_slot_name(0))}", USER 2 "${cstr(b2, b2.web_test_slot_name(1))}" kept; the sound playing back ("${cstr(b2, b2.web_test_patch_name())}"); the palette ${b2.web_test_palette(-1)}`,
        cstr(b2, b2.web_test_slot_name(0)) === cstr(ex, ex.web_test_slot_name(0)) && cstr(b2, b2.web_test_slot_name(1)) === "WARM PAD" &&
        cstr(b2, b2.web_test_patch_name()) === cstr(ex, ex.web_test_patch_name()) && b2.web_test_palette(-1) === 3);
  ex.web_test_palette(0);
}
{                                               // every page: in silence nothing drawn; a note held, the pixels sent
  const z = await boot(), n = z.web_test_go(0), SPI = 12e6 / 8 / 2;   // (the FM-1's SPI1: 12 MHz, 2 bytes a pixel)
  const SECN = ["OSC A", "OSC B", "SUB", "NOISE", "FILTER", "ENV 1", "ENV 2", "LFO 1", "LFO 2", "VOICE", "HOME",
                "MATRIX", "DRIVE", "CHORUS", "DELAY", "REVERB", "OTT", "EQ", "MENU", "GLOBAL", "SAVE", "BROWSE"];
  const here = () => `${SECN[z.web_test_page_section()]} ${cstr(z, z.web_test_page_name())}`;
  const hush = () => { for (let k = 0; k < 40 && play(z, 0.25).peak > 1e-5; k++); play(z, 0.3); };
  const idleDraws = () => { play(z, 1.2); const d0 = z.web_screen_draws(); play(z, 1.0); return z.web_screen_draws() - d0; };
  const param = (sec, nm, k) => {               // a page's k-th parameter, found by its section and name
    for (let q = 0; q < n; q++) {
      z.web_test_go(q);
      if (z.web_test_page_section() === sec && cstr(z, z.web_test_page_name()) === nm) return z.web_test_page_param(k);
    }
    return -1;
  };
  const CH_ON = param(SEC.CHORUS, "MORE", 1), OT_ON = param(SEC.OTT, "BANDS", 3);
  param(SEC.MATRIX, "SLOTS", 0);
  const M1 = z.web_test_page_virt(1);           // (slot 1 picked: its SOURCE)
  z.web_test_set(CH_ON, 1);                     // (every live graph live: the chorus and OTT on, LFO 1 on the EQ's MID)
  z.web_test_set(OT_ON, 1);
  z.web_test_set(ID.EQ_ON, 1);
  z.web_test_set(M1, SRC.LFO1);
  z.web_test_set(M1 + 1, ID.EQ_MID);
  z.web_test_set(M1 + 2, 500);
  const busy = [], rows = [];
  let worst = 0, wname = "", pages = 0;
  for (let q = 0; q < n; q++) {
    z.web_test_go(q);
    if (z.web_test_page() !== q) continue;      // (SAVE's, the browser's: below)
    pages++;
    hush();
    const quiet = idleDraws();
    if (quiet) busy.push(`${here()} ${quiet}`);
    const p0 = z.web_screen_px();
    z.web_keys(1 << 7);
    play(z, 1.0);
    const px = z.web_screen_px() - p0;
    z.web_keys(0);
    rows.push(`${here()} ${Math.round(100 * px / SPI)} %`);
    if (px > worst) worst = px, wname = here();
  }
  z.web_test_go(0);
  hush();
  const zb = (b) => { z.web_buttons(1 << b); play(z, 0.03); z.web_buttons(0); play(z, 0.03); };
  zb(B.SAVE);                                   // SAVE twice: the browser; once there: NAME IT
  zb(B.SAVE);
  const qb = idleDraws(), sb = z.web_test_page_section();
  zb(B.SAVE);
  const qs = idleDraws(), ss = z.web_test_page_section();
  zb(B.HOME);
  if (qs) busy.push(`SAVE ${qs}`);
  if (qb) busy.push(`BROWSE ${qb}`);
  console.log(`emu: a note held, the SPI's time a second each page takes: ${rows.join(", ")}`);
  check(`every page (${pages}, SAVE's and the browser's): in silence nothing drawn (${busy.length ? busy.join(", ") : "none drew"})`,
        pages >= 40 && ss === SEC.SAVE && sb === SEC.BROWSE && busy.length === 0);
  check(`.. a note held: only the rows that change sent; the busiest, ${wname}, ${Math.round(100 * worst / SPI)} % of the SPI's time (12 MHz; under 30 %)`,
        worst / SPI < 0.3);
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
    x.web_enc(E.K1, -4);
  }
  const ha = play(a, 0.5).h, hb = play(b, 0.5).h;
  check("the same input twice: the same sound, bit for bit", ha === hb);
}

console.log(`emu: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
