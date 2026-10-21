// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 u4ia
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
const SEC = { OSCA: 0, LFO1: 7, LFO2: 8, VOICE: 9, MACRO: 10, MATRIX: 11, DRIVE: 12, CHORUS: 13, DELAY: 14, REVERB: 15,
              OTT: 16, EQ: 17, EQ2: 21, PHASER: 24, PHASER2: 25, CHAIN: 36, MENU: 37, GLOBAL: 38, SAVE: 39, BROWSE: 40,
              KEYS: 41, ARP: 42, CHORD: 43, CMID: 44, STEPS: 45, SEQ: 46, SPAT: 47, SCHN: 48 };   // SEC_*
const ID = { A_TABLE: 1, A_POS: 2, A_WARP: 3, A_WAMT: 4, L1_SHAPE: 78, L1_NOTE: 166, DL_NOTE: 168,   // sy_param.c SP_*
             EQ_ON: 169, EQ_LOW: 170, EQ_MID: 171, EQ_HIGH: 172, EQ_LFREQ: 173, EQ_MFREQ: 174, EQ_Q: 175, EQ_HFREQ: 176,
             FX_ORDER: 177, EQ_LQ: 178, EQ_HQ: 179, FXC1: 180, A_ON: 0, CH_ON: 135, CH_MIX: 140, OT_ON: 158,
             PH_ON: 236, EQ2_MID: 209, EQPT: 1009, EQF: 1010, EQG: 1011, EQQ: 1012,
             LPT: 1002, LX: 1003, LY: 1004, LC: 1005, BPM: 1006, TSB: 1007, TSN: 1008, CLOCK: 1071, DRAWN: 12 };  // sy_ui.c UI_*, LFO_DRAW
const SRC = { LFO1: 3, LFO2: 4 };                                                           // sy_param.c SRC_*
const pid = (x, sec, nm, k) => {               // the parameter at knob k of a page, found by its section and name
  const p = x.web_test_page();
  let id = -1;
  for (let q = 0; q < 200 && id < 0; q++) {
    x.web_test_go(q);
    if (x.web_test_page_section() === sec && cstr(x, x.web_test_page_name()) === nm) id = x.web_test_page_param(k);
  }
  x.web_test_go(p);
  return id;
};

async function boot() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports;
  if (ex._initialize) ex._initialize();
  ex.web_nor_erase();
  ex.web_master(700);
  ex.web_boot();
  ex.web_test_set_get(0, 0);                    // (HELP's hints off: the tests read the footer; one test turns them on)
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
const fxTwice = (x) => {                        // FX tapped twice: the chain view <-> the module picked
  for (let i = 0; i < 2; i++) { x.web_buttons(1 << B.FX); play(x, 0.03); x.web_buttons(0); play(x, 0.08); }
  play(x, 0.3);
};
const goPage = (x, sec, nm) => {                // the page of a section, by its name, shown at once
  for (let q = 0; q < x.web_test_go(0) * 0 + 200; q++) {
    x.web_test_go(q);
    if (x.web_test_page_section() === sec && cstr(x, x.web_test_page_name()) === nm) return q;
  }
  return -1;
};

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
{                                               // FX: the chain view; FX twice opens the module picked
  tap(ex, B.FX);                                // (SUPERSAW: CHORUS, REVERB)
  const s1 = ex.web_test_page_section(), c = [0, 1, 2].map((k) => ex.web_test_chain(k));
  ex.web_enc(E.SELECT, 1);                      // SELECT: the next slot
  play(ex, 0.03);
  const slot = ex.web_test_cslot();
  fxTwice(ex);
  const s2 = ex.web_test_page_section(), p2 = ex.web_test_page();
  ex.web_enc(E.ALGO, 1);                        // ALGORITHM: the module's next page
  play(ex, 0.03);
  const p3 = ex.web_test_page(), s3 = ex.web_test_page_section();
  ex.web_enc(E.SELECT, -1);                     // SELECT: the module before in the chain
  play(ex, 0.03);
  const s4 = ex.web_test_page_section();
  fxTwice(ex);
  check(`FX: the chain view (section ${s1}: ${c}); SELECT the slot (${slot}); FX twice opens REVERB (${s2}); ALGORITHM its next page (${s3}), SELECT the module before (${s4}); FX twice: back (${ex.web_test_page_section()}, CHORUS picked: slot ${ex.web_test_cslot()})`,
        s1 === SEC.CHAIN && c.join() === "2,4,0" && slot === 1 && s2 === SEC.REVERB && p3 !== p2 && s3 === SEC.REVERB &&
        s4 === SEC.CHORUS && ex.web_test_page_section() === SEC.CHAIN && ex.web_test_cslot() === 0);
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
  tap(ex, B.REC);                               // (V0.8: a tap arms the sequencer, again not; REC + OCT+ latches the layer)
  const armed = ex.web_test_sq(5, -1), armTxt = cstr(ex, ex.web_test_toast());
  tap(ex, B.REC);
  const one = ex.web_test_mod(), disarmed = ex.web_test_sq(5, -1);
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons((1 << B.REC) | (1 << B.OCTUP)); play(ex, 0.03);
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.35);
  const latched = ex.web_test_mod();
  tap(ex, B.REC);
  check(`REC: a tap arms the sequencer (${armed}, "${armTxt}"), again not (${disarmed}; the layer ${one}); REC + OCT+ latches the layer (${latched}), a tap lets it go (${ex.web_test_mod()}; armed ${ex.web_test_sq(5, -1)})`,
        armed === 1 && armTxt === "REC ARMED" && disarmed === 0 && one === 0 && latched === 2 && ex.web_test_mod() === 0 &&
        ex.web_test_sq(5, -1) === 0);
}
{                                               // the SETTINGS (HOME held 5 s), its palettes
  const toast = () => cstr(ex, ex.web_test_toast()), page0 = ex.web_test_page();
  ex.web_buttons(1 << B.HOME);
  play(ex, 1.5);
  const count = cstr(ex, ex.web_test_cd()), early = ex.web_test_page_section();   // (the countdown's screen)
  ex.web_buttons(0);
  play(ex, 0.1);
  const after = [ex.web_test_page(), toast()];
  check(`HOME held: a countdown ("${count}"); let go at 1.5 s, nothing opens (section ${early}, the page kept, the footer "${after[1]}")`,
        count === "SETTINGS 4" && early !== SEC.MENU && after[0] === page0 && after[1] === "" && cstr(ex, ex.web_test_cd()) === "");
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
  ex.web_enc(E.K1, -2);                         // (round: the last, V0.67's NEON)
  play(ex, 0.05);
  const p11 = ex.web_test_palette(-1), neon = cstr(ex, ex.web_test_palette_name());
  ex.web_enc(E.K1, -4);
  play(ex, 0.05);
  const p7 = ex.web_test_palette(-1), mono = cstr(ex, ex.web_test_palette_name());
  const grey = screen().filter((v, i) => {                 // (outside the swatches: they show the others)
    const s = ((v & 0xFF) << 8) | (v >> 8), r = s >> 11, g = (s >> 5) & 63, b = s & 31, y = Math.floor(i / 240);
    return (y < 18 || y >= 162) && (Math.abs(r * 2 - g) > 2 || Math.abs(b * 2 - g) > 2);
  }).length;
  ex.web_enc(E.K1, 5);
  play(ex, 0.05);
  check(`HOME held 5 s: the SETTINGS (section ${sec}); KNOB 1 changes the palette (${p0} -> ${p1}, ${moved} pixels with it); round to the twelfth, ${neon} (${p11}); the eighth, ${mono} (${p7}): ${grey} pixels not grey`,
        sec === SEC.MENU && p0 === 0 && p1 === 1 && moved > 2000 && p11 === 11 && neon === "NEON" && p7 === 7 &&
        mono === "MONO" && grey === 0 && ex.web_test_palette(-1) === 0);
  tap(ex, B.EDIT);
}
const name = () => cstr(ex, ex.web_test_page_name());
const chord = (held, b) => { ex.web_buttons((1 << held) | (1 << b)); play(ex, 0.03); ex.web_buttons(1 << held); play(ex, 0.03); };
{                                               // EDIT + OCT-/OCT+: the section shown off and on; ENV + OCT: pages
  const oct0 = ex.web_test_oct(), p0 = name(), toast = () => cstr(ex, ex.web_test_toast());
  ex.web_buttons(1 << B.EDIT);
  play(ex, 0.05);
  chord(B.EDIT, B.OCTDN);
  const off = [ex.web_test_value(ID.A_ON), toast(), name()];
  chord(B.EDIT, B.OCTUP);
  const on = [ex.web_test_value(ID.A_ON), toast(), name()];
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`EDIT held: OCT- OSC A off (${off[0]}, "${off[1]}"), OCT+ on again (${on[0]}, "${on[1]}"); the page stays (${p0}, ${off[2]}, ${on[2]}); EDIT let go: no tap (${name()}); the octave stays (${ex.web_test_oct()})`,
        p0 === "WAVE" && off[0] === 0 && off[1] === "OSC A: OFF" && on[0] === 1 && on[1] === "OSC A: ON" &&
        off[2] === "WAVE" && on[2] === "WAVE" && name() === "WAVE" && ex.web_test_page_section() === SEC.OSCA &&
        ex.web_test_oct() === oct0);
  ex.web_buttons(1 << B.ENV);                   // V0.75: ENV + OCT+: ENV 2's MOD layer latched; ENV + OCT-: let go
  play(ex, 0.05);
  chord(B.ENV, B.OCTUP);
  const lat = ex.web_test_mod();
  ex.web_buttons(0);
  play(ex, 0.4);
  const still = ex.web_test_mod(), p4 = name();
  ex.web_buttons(1 << B.ENV);
  play(ex, 0.05);
  chord(B.ENV, B.OCTDN);
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`ENV + OCT+: ENV 2's MOD layer latched (${lat}; ENV let go: ${still}); ENV + OCT-: let go (${ex.web_test_mod()}); the page stays (${p4}, ${name()}), no tap`,
        lat === 2 && still === 2 && ex.web_test_mod() === 0 && p4 === "WAVE" && name() === "WAVE" &&
        ex.web_test_page_section() === SEC.OSCA);
  ex.web_buttons(1 << B.OCTUP); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.03);
  const o1 = ex.web_test_oct();
  ex.web_buttons(1 << B.OCTDN); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.03);
  check(`OCT+ alone: the keys' octave (${oct0} -> ${o1} -> ${ex.web_test_oct()}), the page stays (${name()})`,
        o1 === oct0 + 1 && ex.web_test_oct() === oct0 && name() === "WAVE");
}
{                                               // an LFO's POINTS page
  const twice = () => { ex.web_buttons(1 << B.LFO); play(ex, 0.03); chord(B.LFO, B.OCTUP); ex.web_buttons(0); play(ex, 0.3); };   // (V0.75: LFO + OCT+)
  const pt = (i) => [1, 2, 3].map((f) => ex.web_test_draw(0, i, f));
  tap(ex, B.LFO);
  const sec = ex.web_test_page_section(), pg = name(), items = [0, 1, 2, 3].map((k) => ex.web_test_page_param(k));
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.03);
  const SHAPE = ex.web_test_page_param(0), shape0 = ex.web_test_value(SHAPE);
  ex.web_enc(E.ALGO, -1);
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
  twice();
  const n1 = ex.web_test_draw(0, 0, 0), nw = ex.web_test_lpt(0), [xn, yn] = pt(nw);
  check(`LFO + OCT+ on POINTS: a point (${n0} -> ${n1}) left of the one picked, at ${xn} (halfway ${xb} .. ${pt(nw + 1)[0]}), picked (${nw + 1}); the page stays (${name()})`,
        n1 === n0 + 1 && nw === 3 && xn === Math.floor((xb + pt(nw + 1)[0]) / 2) && name() === "POINTS" &&
        yn <= Math.max(pt(nw - 1)[1], pt(nw + 1)[1]) && yn >= Math.min(pt(nw - 1)[1], pt(nw + 1)[1]));
  ex.web_enc(E.K1, -nw);                        // the first point picked: the new one right of it
  play(ex, 0.03);
  twice();
  check(`.. the first point picked: the new one right of it (point 2 at ${pt(1)[0]}, halfway 0 .. ${xa}), ${ex.web_test_draw(0, 0, 0)} points`,
        ex.web_test_draw(0, 0, 0) === n1 + 1 && ex.web_test_lpt(0) === 1 && pt(1)[0] === Math.floor(xa / 2));
  tap(ex, B.EDIT);                              // LFO + OCT+ from another menu: its POINTS shown, a point added
  const n2 = ex.web_test_draw(0, 0, 0), e2 = ex.web_test_page_section();
  twice();
  const n3 = ex.web_test_draw(0, 0, 0);
  check(`.. LFO + OCT+ from EDIT (section ${e2}): the LFO's POINTS (${name()}), a point added (${n2} -> ${n3})`,
        e2 === SEC.OSCA && n3 === n2 + 1 && name() === "POINTS" && ex.web_test_page_section() === SEC.LFO1);
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.03);
  ex.web_enc(E.K1, 8 - ex.web_test_value(SHAPE));   // SHAPE STAIRS: 16 points, no 17th
  play(ex, 0.03);
  ex.web_enc(E.ALGO, -1);
  play(ex, 0.03);
  twice();
  check(`STAIRS shown (16 points): LFO + OCT+ adds none (SHAPE ${ex.web_test_value(SHAPE)}, drawn ${ex.web_test_draw(0, 0, 0)} points)`,
        ex.web_test_value(SHAPE) === 8 && ex.web_test_draw(0, 0, 0) === n3 && name() === "POINTS");
  ex.web_buttons(1 << B.REC);                   // the MOD layer: the points stay
  play(ex, 0.6);
  ex.web_enc(E.K3, 5);
  play(ex, 0.12);
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`the MOD layer on POINTS: KNOB 3 changes nothing (SHAPE ${ex.web_test_value(SHAPE)}, ${ex.web_test_draw(0, 0, 0)} points)`,
        ex.web_test_value(SHAPE) === 8 && ex.web_test_draw(0, 0, 0) === n3);
  tap(ex, B.EDIT);
}
{                                               // V0.75: LFO + OCT-: the point picked goes; LFO held 5 s: the LFO reset
  const n = () => ex.web_test_draw(0, 0, 0), toast = () => cstr(ex, ex.web_test_toast());
  const del = () => { ex.web_buttons(1 << B.LFO); play(ex, 0.03); chord(B.LFO, B.OCTDN); const t = toast(); ex.web_buttons(0); play(ex, 0.4); return t; };
  tap(ex, B.LFO);                               // (POINTS: STAIRS shown, its 16 points)
  ex.web_enc(E.K1, 2 - ex.web_test_lpt(0));     // point 3
  play(ex, 0.03);
  const gone = del();
  check(`LFO + OCT-: point 3 goes ("${gone}"): STAIRS drawn (SHAPE ${ex.web_test_value(ID.L1_SHAPE)}), ${n()} points, point ${ex.web_test_lpt(0) + 1} picked; the page stays (${name()})`,
        gone === "POINT 3 DELETED" && ex.web_test_value(ID.L1_SHAPE) === ID.DRAWN && n() === 15 && ex.web_test_lpt(0) === 1 &&
        ex.web_test_draw(0, 2, 1) === 250 && name() === "POINTS");
  ex.web_enc(E.K1, -1);                         // the first point: it stays
  play(ex, 0.03);
  const ends = del(), n1 = n();
  ex.web_buttons(1 << B.LFO);                   // held: MOMENTARY, then the reset's countdown from 2 s
  play(ex, 2.5);
  const count = cstr(ex, ex.web_test_cd());
  ex.web_buttons(0);
  play(ex, 0.1);
  const kept = [ex.web_test_value(ID.L1_SHAPE), n()];
  ex.web_buttons(1 << B.LFO);                   // a knob turned during the countdown stops it
  play(ex, 2.2);
  ex.web_enc(E.K3, 1);
  play(ex, 3.2);
  ex.web_buttons(0);
  play(ex, 0.1);
  const kept2 = ex.web_test_value(ID.L1_SHAPE);
  ex.web_buttons(1 << B.LFO);
  play(ex, 5.3);
  const reset = toast();
  ex.web_buttons(0);
  play(ex, 0.4);
  const r = [ex.web_test_value(ID.L1_SHAPE), n()];
  check(`.. the first point picked: "${ends}" (${n1}); held: "${count}", let go at 2.5 s: kept (SHAPE ${kept[0]}, ${kept[1]} points); a knob turned stops it (${kept2}); held 5 s: "${reset}" (SHAPE ${r[0]}, ${r[1]} points)`,
        ends === "THE END POINTS STAY" && n1 === 15 && count.startsWith("RESET LFO 1") && kept[0] === ID.DRAWN &&
        kept[1] === 15 && kept2 === ID.DRAWN && reset === "LFO 1: RESET" && r[0] === 0 && name() === "POINTS");
  ex.web_buttons(1 << B.SAVE);                  // SAVE + OCT-: the reset undone, SAVE + OCT+: again
  play(ex, 0.05);
  chord(B.SAVE, B.OCTDN);
  const undone = [toast(), ex.web_test_value(ID.L1_SHAPE), n()];
  chord(B.SAVE, B.OCTUP);
  const redone = [toast(), ex.web_test_value(ID.L1_SHAPE)];
  chord(B.SAVE, B.OCTDN);
  ex.web_buttons(0);
  play(ex, 0.4);
  check(`SAVE + OCT-: the reset undone ("${undone[0]}": SHAPE ${undone[1]}, ${undone[2]} points); SAVE + OCT+: again ("${redone[0]}": SHAPE ${redone[1]}); SAVE let go: no page, no octave`,
        undone[0] === "UNDO" && undone[1] === ID.DRAWN && undone[2] === 15 && redone[0] === "REDO" && redone[1] === 0 &&
        name() === "POINTS" && ex.web_test_oct() === 0);
}
{                                               // SYNC FREE / FIXED: RATE in Hz or a note value
  ex.web_enc(E.ALGO, 1);                      // LFO 1 SHAPE
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
  goPage(ex, SEC.DELAY, "DELAY");               // the delay (FIXED 1/8D by default)
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
  goPage(ex, SEC.VOICE, "VOICE");               // GLO from VOICE: three taps, PLAY, the MATRIX's SLOTS, GLOBAL's TEMPO
  for (let i = 0; i < 3; i++) tap(ex, B.GLO);
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
  check(`GLO, from VOICE three taps: GLOBAL (section ${sec}) ${name()} (${items}): ${a[0]} BPM ${a[1]}/${a[2]}`,
        sec === SEC.GLOBAL && name() === "TEMPO" && items.join() === [ID.BPM, ID.TSB, ID.TSN, ID.CLOCK].join() &&
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
{                                               // the FX chain: add, take out, move, the limits, bypass, MIX, undo
  const c = await boot(), ct = () => cstr(c, c.web_test_toast()), ch = () => [0, 1, 2, 3, 4, 5, 6, 7].map((k) => c.web_test_chain(k));
  const ctap = (b) => { c.web_buttons(1 << b); play(c, 0.03); c.web_buttons(0); play(c, 0.35); };
  const fxoct = (b) => {                        // FX + OCT- / OCT+, FX let go after (no tap)
    c.web_buttons(1 << B.FX); play(c, 0.05);
    c.web_buttons((1 << B.FX) | (1 << b)); play(c, 0.03);
    c.web_buttons(1 << B.FX); play(c, 0.03);
    c.web_buttons(0); play(c, 0.4);
  };
  const pick = (k) => { c.web_enc(E.K2, k - c.web_test_cslot()); play(c, 0.05); };
  const type = (t) => { c.web_enc(E.K1, t - c.web_test_ctype()); play(c, 0.05); };
  play(c, 0.2);
  ctap(B.FX);                                   // (SUPERSAW: CHORUS, REVERB)
  type(7);                                      // KNOB 1: PHASER
  pick(0);                                      // KNOB 2: slot 1, CHORUS
  fxoct(B.OCTUP);
  const add = [ch().slice(0, 4).join(), ct(), c.web_test_cslot(), c.web_test_value(ID.PH_ON), c.web_test_page_section()];
  check(`chain: KNOB 1 a type (PHASER), FX + OCT+ adds it below the slot picked (${add[0]}, "${add[1]}", slot ${add[2] + 1}, its ON ${add[3]}); the view stays (${add[4]}); no tap after (FX let go)`,
        add[0] === "2,13,4,0" && add[1] === "ADDED: PHASER · 2 OF 8" && add[2] === 1 && add[3] === 1 && add[4] === SEC.CHAIN);
  fxoct(B.OCTUP);                               // a second: PHASER 2, below the first; a third: refused
  const two = ch()[2];
  fxoct(B.OCTUP);
  const lim = [ct(), ch().slice(0, 5).join()];
  type(2);                                      // DELAY: one; a second refused
  fxoct(B.OCTUP);
  fxoct(B.OCTUP);
  const one = [ct(), ch().filter((v) => v === 3).length];
  check(`.. a second PHASER (PHASER 2: ${two}); a third refused ("${lim[0]}", ${lim[1]}); DELAY once, a second refused ("${one[0]}", ${one[1]} in it)`,
        two === 14 && lim[0] === "PHASER: TWO AT MOST" && lim[1] === "2,13,14,4,0" && one[0] === "DELAY: ONE AT MOST" && one[1] === 1);
  pick(2);                                      // KNOB 3: PHASER 2 moved one up, then past the top
  c.web_enc(E.K3, -1);
  play(c, 0.05);
  const mv = [ch().slice(0, 5).join(), c.web_test_cslot()];
  c.web_enc(E.K3, -3);
  play(c, 0.05);
  c.web_enc(E.K3, -1);
  play(c, 0.05);
  const top = [ch().slice(0, 5).join(), ct()];
  check(`.. KNOB 3 moves the module picked (PHASER 2 up: ${mv[0]}, picked ${mv[1] + 1}); to the top and no further (${top[0]}, "${top[1]}")`,
        mv[0] === "2,14,13,3,4" && mv[1] === 1 && top[0] === "14,2,13,3,4" && top[1] === "FIRST IN THE CHAIN");
  fxoct(B.OCTDN);                               // FX + OCT-: the module picked out
  const out = [ch().slice(0, 5).join(), ct()];
  check(`.. FX + OCT- takes the module picked out (${out[0]}, "${out[1]}")`, out[0] === "2,13,3,4,0" && out[1] === "TAKEN OUT: PHASER 2");
  for (const t of [0, 4, 5, 6]) { type(t); pick(c.web_test_chain(7) ? 7 : 8); fxoct(B.OCTUP); }   // DRIVE, OTT, EQ, FLANGER
  const full = [ch().filter((v) => v).length, ct()];
  type(9);
  fxoct(B.OCTUP);
  check(`.. eight modules (${full[0]}, "${full[1]}"): a ninth refused ("${ct()}")`, full[0] === 8 && ct() === "THE CHAIN IS FULL: 8 MODULES");
  pick(0);                                      // KNOB 4: the slot picked's MIX (CHORUS's)
  const m0 = c.web_test_value(ID.CH_MIX);
  for (let i = 0; i < 3; i++) { c.web_enc(E.K4, 1); play(c, 0.12); }
  const m1 = c.web_test_value(ID.CH_MIX);
  c.web_buttons(1 << B.FX);                     // FX held: F3 bypasses module 1 (CHORUS), F3 again takes it back
  play(c, 0.6);
  c.web_keys(1 << 0); play(c, 0.05); c.web_keys(0); play(c, 0.05);
  const byp = [c.web_test_value(ID.CH_ON), ct(), (c.web_lit_keys() >> 0) & 1, (c.web_dim_keys() >> 0) & 1, c.web_test_voices()];
  c.web_keys(1 << 0); play(c, 0.05); c.web_keys(0); play(c, 0.05);
  const back = [c.web_test_value(ID.CH_ON), ct()];
  c.web_buttons(0);
  play(c, 0.3);
  check(`.. KNOB 4 the slot picked's MIX (CHORUS ${m0} -> ${m1}); FX held: F3 bypasses module 1 (ON ${byp[0]}, "${byp[1]}", its LED glowing ${byp[3]}, lit ${byp[2]}, no note ${byp[4]}), F3 again: back (${back[0]}, "${back[1]}")`,
        m1 === m0 + 3 && byp[0] === 0 && byp[1] === "CHORUS BYPASSED" && byp[2] === 0 && byp[3] === 1 && byp[4] === 0 &&
        back[0] === 1 && back[1] === "CHORUS IN");
  const before = ch().join();                   // SAVE + OCT-: undo, a step at a time (the bypass twice, then the MIX)
  c.web_buttons(1 << B.SAVE);
  play(c, 0.05);
  for (let i = 0; i < 4; i++) { c.web_buttons((1 << B.SAVE) | (1 << B.OCTDN)); play(c, 0.03); c.web_buttons(1 << B.SAVE); play(c, 0.03); }
  c.web_buttons(0);
  play(c, 0.4);
  check(`.. SAVE + OCT- undoes the chain's edits (${before} -> ${ch().join()})`, ch().join() !== before && ch().filter((v) => v).length === 7);
  type(5);                                      // a second EQ (its pages: its own values)
  pick(c.web_test_chain(7) ? 7 : ch().filter((v) => v).length);
  fxoct(B.OCTUP);
  const at = c.web_test_cslot();
  fxTwice(c);
  const eq2 = [c.web_test_page_section(), cstr(c, c.web_test_page_name()), ...[0, 1, 2, 3].map((k) => c.web_test_page_param(k))];
  c.web_enc(E.K1, 1);                           // POINTS: MID picked, KNOB 3 its gain: EQ 2's
  play(c, 0.12);
  for (let i = 0; i < 3; i++) { c.web_enc(E.K3, 1); play(c, 0.12); }
  check(`.. a second EQ (slot ${at + 1}), FX twice: its pages (section ${eq2[0]}, ${eq2[1]}: ${eq2.slice(2)}); KNOB 3 on MID: EQ 2's gain (${c.web_test_value(ID.EQ2_MID)}), the first's kept (${c.web_test_value(ID.EQ_MID)})`,
        eq2[0] === SEC.EQ2 && eq2[1] === "POINTS" && eq2.slice(2).join() === [ID.EQPT, ID.EQF, ID.EQG, ID.EQQ].join() &&
        c.web_test_value(ID.EQ2_MID) === 3 && c.web_test_value(ID.EQ_MID) === 0);
}
{                                               // an EQ module's pages; a knob turning lit, the curve before it kept
  ex.web_test_set(ID.FXC1 + 2, 6);              // (EQ the chain's third: CHORUS, REVERB, EQ)
  goPage(ex, SEC.EQ, "POINTS");
  play(ex, 0.03);
  const pg = () => [name(), ...[0, 1, 2, 3].map((k) => ex.web_test_page_param(k))];
  const eq1 = pg();
  ex.web_enc(E.ALGO, 1);
  play(ex, 0.03);
  const eq2 = pg();
  ex.web_enc(E.ALGO, -1);
  play(ex, 0.03);
  check(`EQ: its pages ${eq1[0]} (${eq1.slice(1)}), ${eq2[0]} (${eq2.slice(1)}; no ON: the FX-held keys bypass it)`,
        eq1[0] === "POINTS" && eq1.slice(1).join() === [ID.EQPT, ID.EQF, ID.EQG, ID.EQQ].join() && eq2[0] === "GAINS" &&
        eq2.slice(1).join() === [ID.EQ_LOW, ID.EQ_MID, ID.EQ_HIGH, -1].join());
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
  for (const [id, v] of [[ID.EQ_MID, 0], [ID.EQ_Q, 400], [ID.EQ_LQ, 300], [106 + 3 * sl, 0], [107 + 3 * sl, 0], [108 + 3 * sl, 0], [ID.FXC1 + 2, 0]])
    ex.web_test_set(id, v);
  play(ex, 0.1);
  tap(ex, B.EDIT);
}
{                                               // GLO's MATRIX, a slot at a time (a fresh unit); SEQ: its menu (V0.8)
  const m = await boot(), mscr = () => new Uint16Array(m.memory.buffer, m.web_screen(), 240 * 240).slice();
  const mtap = (b) => { m.web_buttons(1 << b); play(m, 0.03); m.web_buttons(0); play(m, 0.35); };
  play(m, 0.2);
  mtap(B.SEQ);
  const seq = [cstr(m, m.web_test_page_name()), m.web_test_page_section()];
  for (let i = 0; i < 3; i++) mtap(B.GLO);      // (VOICE, PLAY, then the MATRIX's SLOTS)
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
  check(`SEQ: its menu (section ${seq[1]}, ${seq[0]}); GLO: the matrix (section ${sec}, ${pg}); KNOB 1 picks a slot, round (slot 1's source ${first}, slot 8's ${SRC8}); KNOB 2 its source (${s0} -> ${src}), KNOB 3 where it goes (${dst}), KNOB 4 how much (${amt}); ${changed(a, b)} pixels`,
        seq[0] === "NOTES" && seq[1] === SEC.SEQ && sec === SEC.MATRIX && pg === "SLOTS" && SRC8 === first + 21 && DST8 === SRC8 + 1 && AMT8 === SRC8 + 2 &&
        s0 === 0 && src === SRC.LFO1 && dst > 0 && amt > 0 && changed(a, b) > 300);
}
{                                               // SCL: the keys' scale, its root, how they fit it, their velocity
  const u = await boot(), ut = () => cstr(u, u.web_test_toast());
  const utap = (b) => { u.web_buttons(1 << b); play(u, 0.03); u.web_buttons(0); play(u, 0.35); };
  const ukey = (k) => {                         // a key pressed and let go: the note it started, the voices then
    u.web_keys(1 << k);
    play(u, 0.05);
    const r = [u.web_test_key_note(), u.web_test_voices()];
    u.web_keys(0);
    play(u, 0.3);
    return r;
  };
  play(u, 0.2);
  const plain = ukey(3);                        // (G#3 as it is: 56)
  utap(B.SCL);
  const sec = u.web_test_page_section(), pg = cstr(u, u.web_test_page_name()), sclLit = (u.web_lit_buttons() >> B.SCL) & 1;
  u.web_enc(E.K1, 1);                           // KNOB 1: MAJOR; KNOB 2: D; KNOB 3: SNAP
  play(u, 0.12);
  u.web_enc(E.K2, 2);
  play(u, 0.12);
  u.web_enc(E.K3, 1);
  play(u, 0.12);
  const snap = ukey(3), own = ukey(1);          // G#3: off D major, down to G3; F#3: on it, as it is
  check(`SCL: its page (section ${u.web_test_page_section()}, ${pg}), SCL lit (${sclLit}); MAJOR, D, SNAP (${u.web_test_scl(0)}, ${u.web_test_scl(1)}, ${u.web_test_scl(2)}): G#3 plays ${plain[0]} before, ${snap[0]} now; F#3 ${own[0]}`,
        sec === SEC.KEYS && pg === "SCALE" && sclLit === 1 && u.web_test_scl(0) === 1 && u.web_test_scl(1) === 2 && u.web_test_scl(2) === 1 &&
        plain[0] === 56 && snap[0] === 55 && own[0] === 54);
  u.web_enc(E.K3, 1);                           // WHITE: the white keys walk the scale from the root at C4
  play(u, 0.12);
  const c4 = ukey(7), d4 = ukey(9), f3 = ukey(0), cs4 = ukey(8);
  check(`.. WHITE: C4 plays ${c4[0]} (the root, D4), D4 ${d4[0]} (E4), F3 ${f3[0]} (four degrees under the root: G3), C#4 nothing (${cs4[1]} voices)`,
        c4[0] === 62 && d4[0] === 64 && f3[0] === 55 && cs4[1] === 0);
  for (let i = 0; i < 10; i++) { u.web_enc(E.K4, 5); play(u, 0.004); }   // KNOB 4: the velocity, up to 127
  play(u, 0.1);
  ukey(7);
  const vel = u.web_test_key_vel();
  u.web_buttons(1 << B.SCL);                    // SCL held: a word, then a key is the root (no note)
  play(u, 0.6);
  const word = ut();
  u.web_keys(1 << 4);                           // A3
  play(u, 0.05);
  const quiet = u.web_test_voices();
  u.web_keys(0);
  play(u, 0.05);
  const rt = [u.web_test_scl(1), ut()];
  u.web_buttons(0);
  play(u, 0.3);
  check(`.. KNOB 4: the keys' velocity ${u.web_test_scl(3)} (a key's: ${vel}); SCL held ("${word}") + A3: the root ${rt[0]} ("${rt[1]}"), no note (${quiet} voices); the page stays (${u.web_test_page_section()})`,
        u.web_test_scl(3) === 127 && vel === 127 && word === "SCL HELD: A KEY IS THE ROOT" && rt[0] === 9 &&
        rt[1] === "ROOT A" && quiet === 0 && u.web_test_page_section() === SEC.KEYS);
  u.web_enc(E.K3, -2);                          // FIT OFF: A major on the keys' LEDs
  play(u, 0.2);
  const mid = u.web_mid_keys(), dim = u.web_dim_keys(), bit = (m, k) => (m >>> k) & 1;
  check(`.. the LEDs, A major: A3 and A4 at the mid level (${bit(mid, 4)}, ${bit(mid, 16)}), C#4 glowing (${bit(dim, 8)}), C4 dark (${bit(dim, 7)}${bit(mid, 7)})`,
        bit(mid, 4) && bit(mid, 16) && bit(dim, 8) && !bit(dim, 7) && !bit(mid, 7) && !bit(dim, 4));
  play(u, 3.8);                                 // (3 s of quiet: the settings written) a power cycle: they come back
  const nor = new Uint8Array(u.memory.buffer, u.web_nor(), u.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const u2 = instance.exports;
  if (u2._initialize) u2._initialize();
  new Uint8Array(u2.memory.buffer, u2.web_nor(), u2.web_nor_size()).set(nor);
  u2.web_master(700);
  u2.web_boot();
  play(u2, 0.3);
  check(`.. after a power cycle: the scale ${u2.web_test_scl(0)}, the root ${u2.web_test_scl(1)}, the fit ${u2.web_test_scl(2)}, the velocity ${u2.web_test_scl(3)}`,
        u2.web_test_scl(0) === 1 && u2.web_test_scl(1) === 9 && u2.web_test_scl(2) === 0 && u2.web_test_scl(3) === 127);
}
{                                               // the hold layers: GLO PERFORM, FX MACROS, LFO MOMENTARY, EDIT JUMP
  const h = await boot(), ht = () => cstr(h, h.web_test_toast()), hn = () => cstr(h, h.web_test_page_name());
  const hold = (b) => { h.web_buttons(1 << b); play(h, 0.6); };
  const letGo = () => { h.web_buttons(0); play(h, 0.2); };
  const breathes = (b) => (h.web_breath_buttons() >> b) & 1;
  play(h, 0.2);
  hold(B.GLO);                                  // GLO: PERFORM, the keys playing on
  const perf = [h.web_test_lay(), breathes(B.GLO)];
  h.web_keys(1 << 7);
  play(h, 0.05);
  for (let i = 0; i < 8; i++) { h.web_enc(E.K1, 1); play(h, 0.02); }   // KNOB 1: the bend, halfway up
  for (let i = 0; i < 5; i++) { h.web_enc(E.K2, 1); play(h, 0.13); }   // KNOB 2: the mod wheel, slowly
  play(h, 0.05);
  const under = [h.web_test_bend(), h.web_test_wheel(), h.web_test_voices()];
  letGo();
  h.web_keys(0);
  play(h, 0.2);
  check(`GLO held: PERFORM (layer ${perf[0]}, GLO breathing ${perf[1]}); KNOB 1 bends (${under[0]} / 1000), KNOB 2 the mod wheel (${under[1]}), the note playing on (${under[2]}); let go: the bend back (${h.web_test_bend()}), the wheel kept (${h.web_test_wheel()}), the layer shut (${h.web_test_lay()})`,
        perf[0] === 1 && perf[1] === 1 && under[0] === 500 && under[1] === 5 && under[2] === 1 && h.web_test_bend() === 0 &&
        h.web_test_wheel() === 5 && h.web_test_lay() === 0);
  const MAC1 = pid(h, SEC.MACRO, "HOME", 0);
  hold(B.FX);                                   // FX: MACROS; KNOB 1 MACRO 1; G3 bypasses module 2 (REVERB), A3 none
  const mac = h.web_test_lay(), m0 = h.web_test_value(MAC1), rv = h.web_test_value(149);
  for (let i = 0; i < 4; i++) { h.web_enc(E.K1, 1); play(h, 0.13); }
  h.web_keys(1 << 2);
  play(h, 0.05);
  const fx = [h.web_test_value(MAC1), h.web_test_value(149), ht(), h.web_test_voices(), (h.web_dim_keys() >> 2) & 1];
  h.web_keys(0);
  play(h, 0.05);
  h.web_keys(1 << 4);
  play(h, 0.05);
  const none = ht();
  h.web_keys(0);
  letGo();
  check(`FX held: MACROS (layer ${mac}); KNOB 1 MACRO 1 (${m0} -> ${fx[0]}); G3 bypasses REVERB (${rv} -> ${fx[1]}, "${fx[2]}", no note: ${fx[3]} voices, its LED glowing ${fx[4]}); A3: "${none}"; let go (${h.web_test_lay()})`,
        mac === 2 && fx[0] > m0 && rv === 1 && fx[1] === 0 && fx[2] === "REVERB BYPASSED" && fx[3] === 0 && fx[4] === 1 &&
        none === "NO MODULE THERE YET" && h.web_test_lay() === 0);
  h.web_test_set(149, 1);
  tap(h, B.EDIT);                               // LFO on one of OSC A's pages: MOMENTARY
  const K1 = h.web_test_page_virt(0), p0 = h.web_test_value(K1), pg = hn();
  hold(B.LFO);
  const mom = h.web_test_lay();
  for (let i = 0; i < 3; i++) { h.web_enc(E.K1, 1); play(h, 0.13); }
  play(h, 0.1);
  const p1 = h.web_test_value(K1);
  letGo();
  check(`LFO held on OSC A's ${pg}: MOMENTARY (layer ${mom}); KNOB 1 moves its value ${p0} -> ${p1}; LFO let go: back to ${h.web_test_value(K1)}`,
        mom === 3 && K1 >= 0 && K1 < 1000 && p1 !== p0 && h.web_test_value(K1) === p0);
  hold(B.EDIT);                                 // EDIT: JUMP; C#4 opens KEYS, A#3 GLOBAL; the knobs do nothing
  const jmp = [h.web_test_lay(), (h.web_dim_keys() >> 8) & 1, (h.web_lit_keys() >> 0) & 1];
  h.web_keys(1 << 8);
  play(h, 0.05);
  h.web_keys(0);
  play(h, 0.05);
  const s1 = h.web_test_page_section();
  h.web_keys(1 << 5);
  play(h, 0.05);
  h.web_keys(0);
  play(h, 0.05);
  const s2 = h.web_test_page_section(), bpm = h.web_test_tempo(0);
  h.web_enc(E.K1, 3);
  play(h, 0.13);
  const bpm2 = h.web_test_tempo(0);
  letGo();
  check(`EDIT held: JUMP (layer ${jmp[0]}; C#4 glowing ${jmp[1]}, F3 lit: OSC A shown ${jmp[2]}); C#4: section ${s1} (KEYS), A#3: ${s2} (GLOBAL); KNOB 1 under it: BPM ${bpm} -> ${bpm2}; let go: on GLOBAL (${h.web_test_page_section()})`,
        jmp[0] === 4 && jmp[1] === 1 && jmp[2] === 1 && s1 === SEC.KEYS && s2 === SEC.GLOBAL && bpm2 === bpm &&
        h.web_test_page_section() === SEC.GLOBAL && h.web_test_lay() === 0);
}
{                                               // PLAY: the transport (the FREE LFOs from the beat), its LEDs; ARP
  const r = await boot(), rt = () => cstr(r, r.web_test_toast());
  const rtap = (b) => { r.web_buttons(1 << b); play(r, 0.03); r.web_buttons(0); };
  play(r, 0.77);                                // (the LFOs running)
  const ph0 = r.web_test_lfo_phase(0);
  rtap(B.PLAY);
  play(r, 0.01);
  const ph1 = r.web_test_lfo_phase(0), run = r.web_test_run(), word = rt();
  play(r, 0.04);                                // (a frame: the LEDs)
  const green = (r.web_lit_buttons() >> 14) & 1;
  let on = 0;
  for (let i = 0; i < 100; i++) { play(r, 0.01); on += (r.web_lit_buttons() >> B.PLAY) & 1; }
  rtap(B.PLAY);
  play(r, 0.1);
  const off = [r.web_test_run(), (r.web_lit_buttons() >> 14) & 1, rt()];
  const rcombo = (h, b) => { r.web_buttons(1 << h); play(r, 0.03); r.web_buttons((1 << h) | (1 << b)); play(r, 0.03); r.web_buttons(0); };
  rcombo(B.ARP, B.OCTUP);                       // (V0.75: ARP + OCT+ the ARP on, lit; ARP + OCT- off)
  play(r, 0.05);
  const arp = [rt(), (r.web_lit_buttons() >> B.ARP) & 1, r.web_test_chd(4, -1)];
  rcombo(B.ARP, B.OCTDN);
  play(r, 0.05);
  const arp2 = [rt(), (r.web_lit_buttons() >> B.ARP) & 1, r.web_test_chd(4, -1)];
  check(`PLAY: the transport on (${run}, "${word}"), LFO 1 from its start (phase ${ph0} -> ${ph1} of 65536), the green LED (${green}), PLAY lit a quarter of each beat (${on} of 100 at 120 BPM); again, off (${off[0]}, green ${off[1]}, "${off[2]}"); ARP: "${arp[0]}" (lit ${arp[1]}, PERFORM ${arp[2]}), again "${arp2[0]}" (${arp2[1]}, ${arp2[2]})`,
        run === 1 && word === "PLAY" && ph0 > 8000 && ph1 < 4000 && green === 1 && on >= 18 && on <= 32 &&
        off[0] === 0 && off[1] === 0 && off[2] === "STOP" && arp[0] === "ARP ON: ARP" && arp[1] === 1 && arp[2] === 1 &&
        arp2[0] === "ARP OFF" && arp2[1] === 0 && arp2[2] === 0);
}
{                                               // the SETTINGS: SCREEN, PANEL, MIDI (its channel, program changes,
  const g = await boot(), gt = () => cstr(g, g.web_test_toast()), gn = () => cstr(g, g.web_test_page_name());   // LEARN)
  const gb = (b) => { g.web_buttons(1 << b); play(g, 0.03); g.web_buttons(0); play(g, 0.05); };
  play(g, 0.2);
  g.web_buttons(1 << B.HOME);                   // HOME held 5 s: the SETTINGS
  play(g, 5.2);
  g.web_buttons(0);
  play(g, 0.1);
  const pages = [gn()];
  g.web_enc(E.K3, 1);                           // SCREEN: KNOB 3 the LEDs' glow, low
  play(g, 0.13);
  const glow = [g.web_test_set_get(1, -1), g.web_dim_level()];
  g.web_enc(E.ALGO, 1);                       // PANEL: KNOB 2 the hold time, 500 ms
  play(g, 0.1);
  pages.push(gn());
  g.web_enc(E.K2, 1);
  play(g, 0.13);
  const hold = g.web_test_set_get(2, -1);
  g.web_enc(E.ALGO, 1);                       // MIDI: KNOB 1 the channel (2), KNOB 4 PROGRAM on
  play(g, 0.1);
  pages.push(gn());
  g.web_enc(E.K1, 2);
  play(g, 0.13);
  g.web_enc(E.K4, 1);
  play(g, 0.13);
  const ch = g.web_test_set_get(3, -1), pc = g.web_test_set_get(4, -1);
  g.web_midi(0x90, 60, 100);                    // channel 1: not heard; channel 2: played
  play(g, 0.1);
  const v1 = g.web_test_voices();
  g.web_midi(0x80, 60, 0);
  g.web_midi(0x91, 60, 100);
  play(g, 0.1);
  const v2 = g.web_test_voices();
  g.web_midi(0x81, 60, 0);
  play(g, 0.3);
  g.web_midi(0xC1, 3, 0);                       // a program change, 3: the fourth starter sound
  play(g, 0.3);
  const snd = [g.web_test_sound(), cstr(g, g.web_test_patch_name())];
  check(`the SETTINGS' pages (${pages.join(", ")}); GLOW low (${glow[0]}, the LEDs' level ${glow[1]}); HOLD ${hold} (500 ms); MIDI on channel ${ch}: channel 1's note not heard (${v1} voices), channel 2's played (${v2}); PROGRAM ${pc}: program change 3, the fourth starter sound (${snd[0]}, "${snd[1]}")`,
        pages.join() === "SCREEN,PANEL,MIDI" && glow[0] === 1 && glow[1] === 1 && hold === 2 && ch === 2 && v1 === 0 &&
        v2 === 1 && pc === 1 && snd[0] === 3);
  g.web_enc(E.K2, 1);                           // LEARN on; on a sound's page KNOB 1 picks its parameter, a CC is
  play(g, 0.13);                                // learned to it; OCT+ done; the CC then sets it
  const lw = gt();
  gb(B.EDIT);
  const K1 = g.web_test_page_virt(0), pg = gn();
  g.web_enc(E.K1, 1);
  play(g, 0.13);
  const pick = g.web_test_set_get(7, -1);
  g.web_midi(0xB1, 20, 64);
  play(g, 0.05);
  const lt = gt(), learned = g.web_test_learned(20);
  gb(B.OCTUP);
  const off = [g.web_test_set_get(5, -1), gt()];
  g.web_midi(0xB1, 20, 0);
  play(g, 0.05);
  const lo = g.web_test_value(K1);
  g.web_midi(0xB1, 20, 127);
  play(g, 0.05);
  const hi = g.web_test_value(K1);
  check(`MIDI LEARN on ("${lw}"); KNOB 1 on ${pg} picks its parameter (${pick}); CC 20 learned to it ("${lt}"); OCT+ ends it (${off[0]}, "${off[1]}"); CC 20 then sets it (0: ${lo}, 127: ${hi})`,
        lw === "LEARN: TURN A KNOB" && K1 >= 0 && K1 < 1000 && pick === K1 && learned === K1 && lt.startsWith("CC 20: ") &&
        off[0] === 0 && off[1] === "LEARN OFF" && hi > lo);
  g.web_test_set_get(0, 1);                     // HELP on: entering the effects, their hint
  tap(g, B.FX);
  const hint = gt();
  g.web_test_set_get(0, 0);
  play(g, 3.8);                                 // (3 s of quiet: the settings written) a power cycle
  const nor = new Uint8Array(g.memory.buffer, g.web_nor(), g.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const g2 = instance.exports;
  if (g2._initialize) g2._initialize();
  new Uint8Array(g2.memory.buffer, g2.web_nor(), g2.web_nor_size()).set(nor);
  g2.web_master(700);
  g2.web_boot();
  play(g2, 0.3);
  const back = [0, 1, 2, 3, 4].map((f) => g2.web_test_set_get(f, -1));
  check(`HELP: entering the effects, the footer's hint ("${hint}"); after a power cycle: HELP ${back[0]}, GLOW ${back[1]}, HOLD ${back[2]}, the channel ${back[3]}, PROGRAM ${back[4]}, CC 20 learned (${g2.web_test_learned(20)})`,
        hint === "FX+OCT+ ADDS \u00b7 FX TWICE OPENS" && back.join() === "0,1,2,2,1" && g2.web_test_learned(20) === K1);
  g.web_buttons(1 << B.HOME);                   // CLEAR: turned once, a word; again within 2 s, every learned CC
  play(g, 5.2);
  g.web_buttons(0);
  play(g, 0.1);
  g.web_enc(E.ALGO, 2);
  play(g, 0.1);
  g.web_enc(E.K3, 1);
  play(g, 0.13);
  const c1 = [gt(), g.web_test_set_get(6, -1)];
  g.web_enc(E.K3, 1);
  play(g, 0.13);
  check(`.. CLEAR on ${gn()}: once "${c1[0]}" (${c1[1]} learned), again "${gt()}" (${g.web_test_set_get(6, -1)})`,
        c1[0] === "TURN AGAIN: ALL CLEARED" && c1[1] === 1 && gt() === "THE LEARNED CCS CLEARED" &&
        g.web_test_set_get(6, -1) === 0);
}
{                                               // OTT's bands' levels: still in silence, moving with a note (the
  const was = [0, 1, 2, 3, 4, 5, 6, 7].map((k) => ex.web_test_chain(k)), won = ex.web_test_value(ID.OT_ON);   // STRATA
  ex.web_test_set(ID.FXC1, 5);                  // UI's chorus draws its shape, still: OTT is the module's live one;
  ex.web_test_set(ID.FXC1 + 1, 0);              // OTT in the chain, alone, on; its page)
  ex.web_test_set(ID.OT_ON, 1);
  goPage(ex, SEC.OTT, "OTT");
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
  check(`OTT on: in silence its levels still (${still} blits in 1 s); a note: moving (${moving} in 0.5 s); after it, still again (${ex.web_screen_draws() - d1})`,
        still === 0 && moving >= 4 && ex.web_screen_draws() - d1 === 0);
  was.forEach((v, k) => ex.web_test_set(ID.FXC1 + k, v));   // (the chain as it was)
  ex.web_test_set(ID.OT_ON, won);
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
  const cd = cstr(ex, ex.web_test_cd());
  ex.web_buttons(0);
  play(ex, 0.4);
  const early = [ex.web_test_slot_value(1, POS), toast()];
  hold(5.3);
  ex.web_buttons(0);
  play(ex, 0.3);
  check(`SAVE held: a countdown ("${cd}"); let go at 2.3 s, USER 2 untouched (WT POS ${early[0]}, the footer "${early[1]}"); held 5 s, over USER 2 ("${toast()}"): its WT POS ${p0} -> ${ex.web_test_slot_value(1, POS)}`,
        cd === "SAVE OVER USER 2 3" && early[0] === p0 && early[1] === "" && toast() === "SAVED OVER USER 2" &&
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
  btn(B.SAVE);                                  // the browser: OCT- held 3 s deletes the user preset playing
  btn(B.SAVE);
  play(ex, 0.4);
  ex.web_enc(E.K2, -1);                         // (USER 1)
  play(ex, 0.3);
  const del0 = [name(), ex.web_test_user_slot(), slotName(0), ex.web_test_oct()];
  ex.web_buttons(1 << B.OCTDN);
  play(ex, 1.5);
  const dcd = cstr(ex, ex.web_test_cd());
  ex.web_buttons(0);
  play(ex, 0.3);
  const kept = [slotName(0), ex.web_test_oct(), toast()];
  ex.web_buttons(1 << B.OCTDN);
  play(ex, 3.3);
  ex.web_buttons(0);
  play(ex, 0.4);
  const del1 = [toast(), slotName(0), ex.web_test_user_slot(), name(), ex.web_test_oct()];
  check(`the browser: OCT- held, a countdown ("${dcd}"); let go at 1.5 s, nothing deleted ("${kept[0]}"), no octave (${kept[1]}); held 3 s: USER 1 deleted ("${del1[0]}", "${del1[1]}"), the sound plays on ("${del1[3]}", from no slot: ${del1[2]})`,
        del0[1] === 0 && dcd === "DELETE USER 1 2" && kept[0] === del0[2] && kept[1] === del0[3] && kept[2] === "" &&
        del1[0] === "DELETED USER 1 · SAVE+OCT- UNDO" && del1[1] === "" && del1[2] === -1 && del1[3] === del0[0] &&
        del1[4] === del0[3]);
  hold(0.7);                                    // SAVE + OCT- straight after: put back
  chord(B.SAVE, B.OCTDN);
  ex.web_buttons(0);
  play(ex, 0.5);
  const put = [toast(), slotName(0)];
  btn(B.OCTDN);                                 // OCT-'s tap there: the octave, at its release
  const oc = ex.web_test_oct();
  btn(B.OCTUP);
  check(`.. SAVE + OCT- straight after: back ("${put[0]}", USER 1 "${put[1]}"); OCT- tapped in the browser: the octave (${del0[3]} -> ${oc} -> ${ex.web_test_oct()})`,
        put[0] === "BACK IN USER 1" && put[1] === del0[2] && oc === del0[3] - 1 && ex.web_test_oct() === del0[3]);
  btn(B.HOME);
  play(ex, 0.1);
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
                "MATRIX", "DRIVE", "CHORUS", "DELAY", "REVERB", "OTT", "EQ", "DRIVE 2", "CHORUS 2", "OTT 2", "EQ 2",
                "FLANGER", "FLANGER 2", "PHASER", "PHASER 2", "FX FILTER", "FX FILTER 2", "BITCRUSH", "BITCRUSH 2",
                "TREMOLO", "TREMOLO 2", "RING MOD", "RING MOD 2", "TAPE", "TAPE 2", "CHAIN", "MENU", "GLOBAL", "SAVE",
                "BROWSE", "SCALE", "ARP", "CHORD", "MIDI", "STEPS"];
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
  param(SEC.MATRIX, "SLOTS", 0);
  const M1 = z.web_test_page_virt(1);           // (slot 1 picked: its SOURCE)
  for (const [k, i] of [[0, 2], [1, 5], [2, 6], [3, 0]])   // (every live graph live: CHORUS, OTT and EQ the chain,
    z.web_test_set(ID.FXC1 + k, i);             // in; LFO 1 on the EQ's MID)
  z.web_test_set(ID.CH_ON, 1);
  z.web_test_set(ID.OT_ON, 1);
  z.web_test_set(ID.EQ_ON, 1);
  z.web_test_set(M1, SRC.LFO1);
  z.web_test_set(M1 + 1, ID.EQ_MID);
  z.web_test_set(M1 + 2, 500);
  const busy = [], rows = [], lint = [];
  let worst = 0, wname = "", pages = 0;
  for (let q = 0; q < n; q++) {
    z.web_test_go(q);
    if (z.web_test_page() !== q) continue;      // (SAVE's, the browser's: below)
    pages++;
    hush();
    const quiet = idleDraws();
    if (quiet) busy.push(`${here()} ${quiet}`);
    const lf = z.web_lint();                    // (V0.67: no text on a drawing, on text, past the edges, in MID)
    if (lf) lint.push(`${here()}: ${cstr(z, z.web_lint_report()).trim().replace(/\n/g, "; ")}`);
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
  check(`every page (${pages}): its text clear of the drawing, of other text and of the edges, none in MID, no two knobs' handles on one place (${lint.length ? lint.join(" | ") : "all clear"})`,
        pages >= 40 && lint.length === 0);
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
{
  const c = await boot();                               // (V0.66: at most two note-ons a block; none lost)
  play(c, 0.1);
  for (const n of [48, 52, 55, 59, 62, 65, 69, 72]) c.web_midi(0x90, n, 90);
  play(c, 0.05);
  const v = c.web_test_voices();
  for (const n of [48, 52, 55, 59, 62, 65, 69, 72]) c.web_midi(0x80, n, 0);
  check(`eight notes at once over MIDI: all eight sound (${v}), started two a block`, v === 8);
}
{
  const ROW = ["AUDIO", "CPU", "SCREEN", "KEYS", "TABLES", "FLASH", "STACKS", "RUN"];
  const over = Array.from({ length: 16 }, (_, i) => ex.web_test_bench_fit(i));   // (the emulator draws no BENCH)
  const worst = over.indexOf(Math.max(...over));
  check(`BENCH's rows at their widest figures fit the window (the widest, ${ROW[worst % 8]}: ${over[worst] > 0 ?
        over[worst] + " px over" : -over[worst] + " px to spare"})`, over[worst] <= 0);
  check(`every page's name ends within its 8 characters (V0.6: RING MOD's ran on into its knobs' ids)`,
        ex.web_test_page_names_bad() === 0);
}

{                                               // V0.7, V0.75: SCL's chords (sy_chord.c): FREE's TYPE, KEY's SHAPE, SPREAD, STRUM
  const c = await boot();
  let mask = 0;
  const notes = () => [...Array(8).keys()].map((i) => c.web_test_voice_note(i)).filter((n) => n >= 0).sort((a, b) => a - b);
  const press = (k) => { mask |= 1 << k; c.web_keys(mask); play(c, 0.04); };
  const release = (k) => { mask &= ~(1 << k); c.web_keys(mask); play(c, 0.06); };
  const nm = () => cstr(c, c.web_test_chord_name());
  const set = (f, v) => { c.web_test_chd(f, v); play(c, 0.01); };
  const K = (n) => n - 53;                      // a key by its note (F3 the first)
  const chordOf = (n) => { press(K(n)); const r = [notes().join(" "), nm()]; release(K(n)); return r; };
  const sent = (m0, on) => {
    const r = [];
    for (let i = m0; i < c.web_test_mout_n(); i++) {
      const p = c.web_test_mout(i);
      if (((p >> 8) & 0xF0) === (on ? 0x90 : 0x80)) r.push([(p >> 16) & 127, (p >> 24) & 127, c.web_test_mout_t(i)]);
    }
    return r;
  };
  play(c, 0.3);
  const plain = (press(K(64)), notes());        // (the chords off: a key, its note, as before)
  release(K(64));
  set(0, 2);                                    // FREE: every key TYPE's chord, root position
  set(3, 4);
  set(1, 1);                                    // MIN on E4
  const em = chordOf(64), gone = notes().length;
  set(1, 11);                                   // MIN7
  const em7 = chordOf(64);
  set(1, 0);                                    // MAJ, VOICING +1, on C4 (no key is a button)
  set(3, 5);
  const inv = chordOf(60);
  set(3, 4);
  set(1, 21);                                   // MIN11 on D4, 13 on G3
  const m11 = chordOf(62);
  set(1, 22);
  const g13 = chordOf(55);
  check(`chords, FREE: off, a key plays ${plain}; MIN on E4: ${em[0]} "${em[1]}", let go ${gone} left; MIN7: ${em7[0]} "${em7[1]}"; MAJ, VOICING +1, C4: ${inv[0]} "${inv[1]}"; MIN11 on D4: ${m11[0]} "${m11[1]}"; 13 on G3: ${g13[0]} "${g13[1]}"`,
        plain.join() === "64" && em[0] === "64 67 71" && em[1] === "Em" && gone === 0 && em7[0] === "64 67 71 74" &&
        em7[1] === "Em7" && inv[0] === "64 67 72" && inv[1] === "C/E" && m11[0] === "62 65 69 72 76 79" &&
        m11[1] === "Dm11" && g13[0] === "55 59 62 65 69 76" && g13[1] === "G13");
  set(1, 0);                                    // SPREAD: 2 OCT, OPEN, WIDE on C4's C major
  set(2, 3);
  const dbl = chordOf(60);
  set(2, 1);
  const open = chordOf(60);
  set(2, 2);
  const wide = chordOf(60);
  set(2, 0);
  check(`chords, SPREAD on C major: 2 OCT ${dbl[0]}; OPEN ${open[0]} "${open[1]}"; WIDE ${wide[0]}`,
        dbl[0] === "60 64 67 72 76 79" && open[0] === "52 60 67" && open[1] === "C/E" && wide[0] === "48 64 79");
  set(0, 1);                                    // KEY in C major (SCALE: MAJOR), root position
  set(3, 4);
  goPage(c, SEC.KEYS, "SCALE");
  c.web_enc(E.K1, 1);
  play(c, 0.1);
  const dm = chordOf(62);                       // D4: D minor in C major
  set(3, 0);                                    // AUTO from here: C4 then F4 keeps F near C (F/C)
  chordOf(60);
  const fauto = chordOf(65);
  set(16, 1);                                   // SHAPE 7TH: B4 a half-diminished seventh
  set(3, 4);
  const b7 = chordOf(71);
  set(16, 0);
  check(`chords, KEY in C major: D4 ${dm[0]} "${dm[1]}"; C4 then F4 with AUTO: ${fauto[0]} "${fauto[1]}"; SHAPE 7TH on B4: ${b7[0]} "${b7[1]}"`,
        dm[0] === "62 65 69" && dm[1] === "Dm" && fauto[0] === "60 65 69" && fauto[1] === "F/C" && b7[0] === "71 74 77 81" &&
        b7[1] === "Bm7b5");
  set(9, 1);                                    // BASS: the root an octave under
  let m0 = c.web_test_mout_n();
  set(13, 1);                                   // USB OUT: the chords on channel 1, the bass on 2
  press(K(60));
  const withBass = notes().join(" ");
  release(K(60));
  const out = [];
  for (let i = m0; i < c.web_test_mout_n(); i++) {
    const p = c.web_test_mout(i);
    out.push(`${(p >> 8) & 0xF0 ? ((p >> 8) & 0xF0).toString(16) : "?"}:${(p >> 8) & 15}:${(p >> 16) & 127}`);
  }
  const ons = out.filter((s) => s.startsWith("90")), offs = out.filter((s) => s.startsWith("80"));
  check(`chords, BASS and USB OUT: C4 with its bass ${withBass}; sent ${out.join(" ")}`,
        withBass === "48 60 64 67" && ons.length === 4 && offs.length === 4 && ons.includes("90:1:48") && ons.includes("90:0:60") &&
        offs.includes("80:1:48") && offs.includes("80:0:67"));
  set(9, 0);
  set(22, 9);                                   // STRUM 30 ms, UP: C4's C major, a note each 30 ms
  m0 = c.web_test_mout_n();
  c.web_keys(mask |= 1 << K(60));
  play(c, 0.012);
  const s1 = notes().length;
  play(c, 0.1);
  const s3 = notes().join(" ");
  release(K(60));
  const up = sent(m0, 1);
  set(23, 1);                                   // DOWN, TAPER 100 %: the last at half the level
  set(25, 100);
  m0 = c.web_test_mout_n();
  press(K(60));
  play(c, 0.1);
  release(K(60));
  const dn = sent(m0, 1);
  const gaps = up.slice(1).map((x, i) => Math.round((x[2] - up[i][2]) / 44.1176));
  check(`chords, STRUM 30 ms: ${s1} note at 12 ms, ${s3} at 112 ms (${up.map((x) => x[0]).join(" ")}, ${gaps.join(" ")} ms apart); DOWN with TAPER 100 %: ${dn.map((x) => `${x[0]}/${x[1]}`).join(" ")}`,
        s1 === 1 && s3 === "60 64 67" && up.map((x) => x[0]).join() === "60,64,67" && gaps.every((g) => Math.abs(g - 30) <= 1) &&
        dn.map((x) => x[0]).join() === "67,64,60" && dn[0][1] === 100 && dn[2][1] === 50);
  set(22, 0);
  set(23, 0);
  set(25, 0);
  set(13, 0);
  set(4, 1);                                    // the ARP: ARP up at 1/16 (125 ms a step, half of it sounding)
  set(5, 0);
  c.web_keys(mask |= 1 << K(60));
  const seqn = [];
  for (let i = 0; i < 5; i++) { play(c, 0.03); seqn.push(notes().join(" ") || "-"); play(c, 0.095); }
  release(K(60));
  set(5, 1);                                    // PATTERN 1: a stab each quarter (500 ms)
  c.web_keys(mask |= 1 << K(60));
  const stab = [];
  for (let i = 0; i < 4; i++) { play(c, 0.05); stab.push(notes().length); play(c, 0.2); }
  release(K(60));
  check(`chords, the ARP: ARP ${seqn.join(", ")}; PATTERN 1, every 250 ms: ${stab.join(" ")}`,
        seqn.join() === "60,64,67,60,64" && stab.join() === "3,0,3,0");
  set(5, 2);                                    // HARP: every key a string over four octaves (FREE MAJ on SCALE's C)
  set(0, 2);
  const hn = [];
  for (const k of [0, 8, 12, 16, 20, 26]) { press(k); hn.push(notes().join(" ")); release(k); }
  set(4, 0);
  set(0, 0);
  check(`chords, HARP on C major (SCALE's root), F3 .. G5 rising: ${hn.join(" | ")}`,
        hn.every((x) => x.split(" ").length === 1) && hn.map(Number).every((x, i, a) => !i || x > a[i - 1]) &&
        hn.map(Number).every((x) => [0, 4, 7].includes(x % 12)));
  set(0, 2);                                    // MIDI IN as roots: a note makes a chord, its off ends it
  set(12, 1);
  c.web_midi(0x90, 62, 90);
  play(c, 0.05);
  const mi = [notes().join(" "), nm()];
  c.web_midi(0x80, 62, 0);
  play(c, 0.1);
  const mo = notes().length;
  check(`chords, MIDI IN as roots (FREE, MAJ): D4 in, ${mi[0]} "${mi[1]}"; its off, ${mo} left`,
        mi[0] === "62 66 69" && mi[1] === "D" && mo === 0);
  c.web_test_chd(5, 0);                         // the settings kept: FREE, ARP, the bass, MIDI in, STRUM, SPREAD
  c.web_test_chd(9, 1);
  c.web_test_chd(22, 9);
  c.web_test_chd(2, 2);
  c.web_test_chd(1, 13);
  play(c, 8.0);                                 // (3 s of quiet after the reverb's tail: the settings written)
  const nor = new Uint8Array(c.memory.buffer, c.web_nor(), c.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const c2 = instance.exports;
  if (c2._initialize) c2._initialize();
  new Uint8Array(c2.memory.buffer, c2.web_nor(), c2.web_nor_size()).set(nor);
  c2.web_master(700);
  c2.web_boot();
  play(c2, 0.3);
  const kept = [0, 1, 2, 5, 9, 12, 22].map((f) => c2.web_test_chd(f, -1));
  check(`chords: after a power cycle MODE, TYPE, SPREAD, STYLE, BASS, MIDI IN, STRUM ${kept.join(" ")}`,
        kept.join() === "2,13,2,0,1,1,9");
}
{                                               // V0.75: the ARP (sy_chord.c: MODE, the steps, GROOVE, LATCH, the beat)
  const c = await boot();
  let mask = 0;
  const FS = 44117.647, SMP = FS * 60 / 120 / 4;   // (a 1/16 at 120 BPM: 5514.7 samples)
  const set = (f, v) => { c.web_test_chd(f, v); play(c, 0.01); };
  const st = (i, f, v) => c.web_test_step(i, f, v);
  const K = (n) => n - 53;
  const sent = (m0, on) => {                    // what went out over USB since m0: [note, velocity, time] (ons or offs)
    const r = [];
    for (let i = m0; i < c.web_test_mout_n(); i++) {
      const p = c.web_test_mout(i);
      if (((p >> 8) & 0xF0) === (on ? 0x90 : 0x80)) r.push([(p >> 16) & 127, (p >> 24) & 127, c.web_test_mout_t(i)]);
    }
    return r;
  };
  const hold = (notes) => { for (const n of notes) { mask |= 1 << K(n); c.web_keys(mask); play(c, 0.004); } };
  const letgo = () => { mask = 0; c.web_keys(0); play(c, 0.4); };
  const steps0 = () => { for (let i = 0; i < 16; i++) { st(i, 0, 0); st(i, 1, 100); st(i, 2, 2); st(i, 3, 1); st(i, 4, 100); st(i, 5, 0); } };
  play(c, 0.3);
  set(13, 1);                                   // USB OUT: the arpeggio read from what it sends
  set(4, 1);                                    // the ARP: ARP, 1/16 at 120 BPM, half a step long
  set(5, 0);
  const run = (mode, oct, notes, secs) => {     // MODE over the notes held: the first note-ons
    set(7, mode);
    set(8, oct);
    const m0 = c.web_test_mout_n();
    hold(notes);
    play(c, secs);
    letgo();
    return sent(m0, 1).map((x) => x[0]);
  };
  const CEG = [60, 64, 67], m = {};
  for (const [nm, mode, oct] of [["UP", 0, 1], ["DOWN", 1, 1], ["UP-DOWN", 2, 1], ["DOWN-UP", 3, 1], ["UP & DOWN", 4, 1],
                                 ["OUTSIDE IN", 8, 1], ["INSIDE OUT", 9, 1], ["PINKY", 10, 1], ["THUMB", 11, 1],
                                 ["UP 2 OCT", 0, 2]])
    m[nm] = run(mode, oct, CEG, 0.95).slice(0, 8).join(" ");
  m["AS PLAYED"] = run(5, 1, [67, 60, 64], 0.6).slice(0, 4).join(" ");
  m["CHORD 2 OCT"] = run(12, 2, CEG, 0.3).slice(0, 6).join(" ");
  const want = { "UP": "60 64 67 60 64 67 60 64", "DOWN": "67 64 60 67 64 60 67 64", "UP-DOWN": "60 64 67 64 60 64 67 64",
                 "DOWN-UP": "67 64 60 64 67 64 60 64", "UP & DOWN": "60 64 67 67 64 60 60 64",
                 "OUTSIDE IN": "60 67 64 60 67 64 60 67", "INSIDE OUT": "64 67 60 64 67 60 64 67",
                 "PINKY": "60 67 64 67 60 67 64 67", "THUMB": "60 64 60 67 60 64 60 67", "UP 2 OCT": "60 64 67 72 76 79 60 64",
                 "AS PLAYED": "67 60 64 67", "CHORD 2 OCT": "60 64 67 72 76 79" };
  const bad = Object.keys(want).filter((k) => m[k] !== want[k]);
  check(`ARP: MODE over C E G: ${Object.keys(want).map((k) => `${k} ${m[k]}`).join(", ")}`, bad.length === 0);

  set(7, 0);                                    // the steps: LENGTH 4: PLAY, TIE, REST, PLAY (3 hits, an octave up, 50 %)
  set(8, 1);
  set(20, 4);
  st(1, 0, 1);
  st(2, 0, 2);
  st(3, 3, 3); st(3, 2, 3); st(3, 1, 50);
  let m0 = c.web_test_mout_n();
  hold(CEG);
  play(c, 1.05);
  letgo();
  const on1 = sent(m0, 1), off1 = sent(m0, 0), t0 = on1.length ? on1[0][2] : 0;
  const at = (x) => Math.round((x[2] - t0) / SMP * 100) / 100;   // a time in steps from the first note
  const seq1 = on1.map((x) => `${x[0]}@${at(x)}/${x[1]}`).join(" ");
  const off60 = off1.find((x) => x[0] === 60), r76 = on1.filter((x) => x[0] === 76).map(at);
  check(`.. the steps (PLAY, TIE, REST, 3 hits +1 OCT at 50 %): ${seq1}; C4 held through the TIE, off at ${off60 ? at(off60) : "-"} steps`,
        on1.map((x) => x[0]).join() === "60,76,76,76,67,72,72,72,64" && r76.length === 3 && Math.abs(r76[0] - 3) < 0.02 &&
        Math.abs(r76[1] - 3.33) < 0.02 && Math.abs(r76[2] - 3.67) < 0.02 && on1[1][1] === 50 && on1[0][1] === 100 &&
        Math.abs(at(on1[4]) - 4) < 0.02 && off60 && Math.abs(at(off60) - 1.5) < 0.02);

  steps0();                                     // GATE 25 %, SWING 75 %: the odd steps half a step late
  set(20, 16);
  set(17, 25);
  set(18, 75);
  st(5, 4, 0);                                  // (step 6: CHANCE 0 %, a gap: its note passed over)
  m0 = c.web_test_mout_n();
  hold(CEG);
  play(c, 0.95);
  letgo();
  const on2 = sent(m0, 1), off2 = sent(m0, 0), t2 = on2[0][2];
  const tt = on2.map((x) => Math.round((x[2] - t2) / SMP * 100) / 100);
  const len2 = on2.slice(0, 4).map((x) => { const o = off2.find((y) => y[0] === x[0] && y[2] > x[2]); return o ? Math.round((o[2] - x[2]) / SMP * 100) / 100 : -1; });
  check(`.. GATE 25 %, SWING 75 %: steps at ${tt.join(" ")} (step 6 left out by CHANCE 0 %: ${on2.map((x) => x[0]).join(" ")}), each ${len2.join(" ")} of a step`,
        tt.slice(0, 5).every((x, i) => Math.abs(x - [0, 1.5, 2, 3.5, 4][i]) < 0.02) && Math.abs(tt[5] - 6) < 0.02 &&
        on2.map((x) => x[0]).slice(0, 7).join() === "60,64,67,60,64,60,64" && len2.every((x) => Math.abs(x - 0.25) < 0.02));
  st(5, 4, 100);
  set(18, 50);
  set(17, 50);
  st(0, 5, 1);                                  // SLIDE on step 1: C4 on until E4 has started
  m0 = c.web_test_mout_n();
  hold(CEG);
  play(c, 0.2);
  letgo();
  const on3 = sent(m0, 1), off3 = sent(m0, 0);
  const sl = [on3.find((x) => x[0] === 64), off3.find((x) => x[0] === 60)];
  st(0, 5, 0);
  check(`.. SLIDE: C4 off ${sl[0] && sl[1] ? sl[1][2] - sl[0][2] : "-"} samples after E4 began (legato)`,
        sl[0] && sl[1] && sl[1][2] > sl[0][2] && sl[1][2] - sl[0][2] <= 96);

  set(19, 1);                                   // LATCH: let go, it plays on; the next press after, what is held anew
  m0 = c.web_test_mout_n();
  hold(CEG);
  mask = 0; c.web_keys(0);
  play(c, 0.6);
  const latched = sent(m0, 1).length;
  hold([62]);
  mask = 0; c.web_keys(0);
  play(c, 0.15);
  m0 = c.web_test_mout_n();
  play(c, 0.5);
  const anew = [...new Set(sent(m0, 1).map((x) => x[0]))].join(" ");
  set(19, 0);                                   // LATCH off: what was let go goes
  play(c, 0.2);
  m0 = c.web_test_mout_n();
  play(c, 0.4);
  const after = sent(m0, 1).length;
  check(`.. LATCH: C E G let go, ${latched} notes in 0.6 s; D4 pressed and let go: ${anew} alone; LATCH off: ${after} notes after`,
        latched >= 4 && anew === "62" && after === 0);

  tap(c, B.PLAY);                               // PLAY: the steps on its beat (a key's first note at the next step)
  play(c, 0.3);
  const tNow = c.web_test_arp(8), q0 = c.web_test_arp(1) / 1000, tRun = tNow - q0 * FS * 60 / 120;
  m0 = c.web_test_mout_n();
  const tPress = c.web_test_arp(8);
  hold([60]);
  play(c, 0.6);
  letgo();
  const on4 = sent(m0, 1), g4 = on4.map((x) => (x[2] - tRun) / SMP);
  const wait4 = on4.length ? (on4[0][2] - tPress) / SMP : -1;
  check(`.. PLAY on: the steps on its beat (${g4.map((x) => x.toFixed(2)).join(" ")} steps from its start), the first ${wait4.toFixed(2)} of a step after the key`,
        on4.length >= 4 && g4.every((x) => Math.abs(x - Math.round(x)) < 0.02) && wait4 > 0 && wait4 <= 1.01 &&
        c.web_test_arp(0) === 1);
  tap(c, B.PLAY);

  set(21, 1);                                   // CLOCK IN MIDI: 24 clocks, a start, clocks: the tempo, the steps on them
  const P = 9 * 128;                            // (a clock every 9 renders: 1152 samples, 95.7 quarter notes a minute)
  const clk = (n) => { for (let i = 0; i < n; i++) { c.web_midi(0xF8, 0, 0); play(c, 9 * 128 / 44118 + 1e-6); } };
  clk(30);
  c.web_midi(0xFA, 0, 0);
  play(c, 0.003);
  const tStart = c.web_test_arp(8);
  clk(26);
  m0 = c.web_test_mout_n();
  mask = (1 << K(60)) | (1 << K(64));           // (both at once: no audio between the clocks)
  c.web_keys(mask);
  clk(40);
  letgo();
  const tempo = c.web_test_arp(5) / 100, follow = c.web_test_arp(6), on5 = sent(m0, 1);
  const p5 = on5.map((x) => (x[2] - tStart) / P);
  c.web_midi(0xFC, 0, 0);
  play(c, 0.05);
  const stopped = c.web_test_arp(0), green = (c.web_lit_buttons() >> 14) & 1;
  check(`.. CLOCK IN MIDI: the tempo ${tempo} (95.7 sent, following: ${follow}); the steps on clocks ${p5.map((x) => x.toFixed(2)).join(" ")} after the start (6 a step); stop: running ${stopped}, PLAY's green ${green}`,
        Math.abs(tempo - 95.7) < 1.0 && follow === 1 && on5.length >= 5 &&
        p5.every((x) => Math.abs(x - Math.round(x)) < 0.05 && Math.round(x) % 6 === 0) && stopped === 0 && green === 0);
  set(21, 0);

  set(17, 75);                                  // the settings kept: GROOVE's, CLOCK IN, a step's, through a power cycle
  set(18, 60);
  set(20, 5);
  set(19, 1);
  set(21, 1);
  st(2, 0, 1); st(2, 1, 40); st(2, 2, 4); st(2, 3, 2); st(2, 4, 55); st(2, 5, 1);
  set(4, 0);
  play(c, 8.0);
  const nor = new Uint8Array(c.memory.buffer, c.web_nor(), c.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const c2 = instance.exports;
  if (c2._initialize) c2._initialize();
  new Uint8Array(c2.memory.buffer, c2.web_nor(), c2.web_nor_size()).set(nor);
  c2.web_master(700);
  c2.web_boot();
  play(c2, 0.3);
  const kept = [17, 18, 20, 19, 21].map((f) => c2.web_test_chd(f, -1)), kept2 = [0, 1, 2, 3, 4, 5].map((f) => c2.web_test_step(2, f, -1));
  check(`.. after a power cycle: GATE, SWING, LENGTH, LATCH, CLOCK IN ${kept.join(" ")}; step 3 ${kept2.join(" ")}`,
        kept.join() === "75,60,5,1,1" && kept2.join() === "1,40,4,2,55,1");
}
{                                               // V0.75: the ARP button: a tap its menu (again: its next page, twice its
  const u = await boot(), name = () => cstr(u, u.web_test_page_name()), toast = () => cstr(u, u.web_test_toast());
  const combo = (h, b) => { u.web_buttons(1 << h); play(u, 0.03); u.web_buttons((1 << h) | (1 << b)); play(u, 0.03); u.web_buttons(0); play(u, 0.3); };
  const twice = (b) => { for (let i = 0; i < 2; i++) { u.web_buttons(1 << b); play(u, 0.03); u.web_buttons(0); play(u, 0.08); } play(u, 0.3); };
  play(u, 0.3);                                 // next section); ARP + OCT+ on, ARP + OCT- off, from any page
  combo(B.ARP, B.OCTUP);
  const fromEdit = [u.web_test_chd(4, -1), toast(), u.web_test_page_section(), (u.web_lit_buttons() >> B.ARP) & 1];
  tap(u, B.ARP);
  const opened = [u.web_test_page_section(), name()];
  tap(u, B.ARP);
  const p2 = name();
  twice(B.ARP);
  const p3 = [u.web_test_page_section(), name()];
  u.web_enc(E.K1, 3); play(u, 0.05);           // STEPS: KNOB 1 the step, 2 its kind, 3 its level, 4 its octave
  u.web_enc(E.K2, 1); play(u, 0.05);
  u.web_enc(E.K3, -2); play(u, 0.05);
  u.web_enc(E.K4, 1); play(u, 0.05);
  const s1 = [u.web_test_arp(7), ...[0, 1, 2].map((f) => u.web_test_step(3, f, -1))];
  u.web_enc(E.ALGO, 1); play(u, 0.1);           // RATCHET: 2 its hits, 3 its chance, 4 its slide
  const pr = name();
  u.web_enc(E.K2, 2); play(u, 0.05);
  u.web_enc(E.K3, -4); play(u, 0.05);
  u.web_enc(E.K4, 1); play(u, 0.05);
  const s2 = [3, 4, 5].map((f) => u.web_test_step(3, f, -1));
  u.web_enc(E.SELECT, 1); play(u, 0.1);         // SELECT: the ARP section (GROOVE, where it was left)
  const pg = [u.web_test_page_section(), name()];
  u.web_enc(E.K1, 2); play(u, 0.05);
  u.web_enc(E.K2, 5); play(u, 0.05);
  u.web_enc(E.K3, -4); play(u, 0.05);
  u.web_enc(E.K4, 1); play(u, 0.05);
  const s3 = [17, 18, 20, 19].map((f) => u.web_test_chd(f, -1));
  combo(B.ARP, B.OCTDN);
  check(`ARP + OCT+ from EDIT: on (${fromEdit[0]}, "${fromEdit[1]}", lit ${fromEdit[3]}; the page stays: section ${fromEdit[2]}); a tap: its menu (section ${opened[0]}, ${opened[1]}), again: ${p2}, twice: ${p3[1]} (section ${p3[0]}); STEPS: step ${s1[0] + 1} ${s1.slice(1)}; ${pr}: ${s2}; SELECT: ${pg[1]} (section ${pg[0]}): ${s3}; ARP + OCT-: off (${u.web_test_chd(4, -1)}, ${name()})`,
        fromEdit[0] === 1 && fromEdit[1] === "ARP ON: ARP" && fromEdit[2] === 0 && fromEdit[3] === 1 &&
        opened.join() === `${SEC.ARP},STYLE` && p2 === "GROOVE" && p3.join() === `${SEC.STEPS},STEPS` &&
        s1.join() === "3,1,90,3" && pr === "RATCHET" && s2.join() === "3,80,1" && pg.join() === `${SEC.ARP},GROOVE` &&
        s3.join() === "60,55,12,1" && u.web_test_chd(4, -1) === 0 && name() === "GROOVE");
  const h = await boot();                       // a chord held, then ARP + OCT+: it plays the chord held; + OCT-: held
  const hcombo = (b) => { h.web_buttons(1 << B.ARP); play(h, 0.03); h.web_buttons((1 << B.ARP) | (1 << b)); play(h, 0.03); h.web_buttons(0); play(h, 0.3); };
  const notesH = () => [...Array(8).keys()].map((i) => h.web_test_voice_note(i)).filter((n) => n >= 0).sort((a, b) => a - b);
  play(h, 0.3);
  h.web_test_chd(5, 0);
  h.web_test_chd(13, 1);
  play(h, 0.02);
  h.web_keys((1 << 7) | (1 << 11) | (1 << 14));
  play(h, 0.3);
  const before = notesH().join(" "), m0 = h.web_test_mout_n();
  hcombo(B.OCTUP);
  play(h, 0.6);
  const outs = [];
  for (let i = m0; i < h.web_test_mout_n(); i++) {
    const p = h.web_test_mout(i);
    if (((p >> 8) & 0xF0) === 0x90) outs.push((p >> 16) & 127);
  }
  hcombo(B.OCTDN);
  play(h, 0.1);
  const heldOn = notesH().join(" ");
  h.web_keys(0);
  play(h, 1.5);
  check(`a chord held (${before}), then ARP + OCT+: it arpeggiates (${outs.slice(0, 6).join(" ")}); ARP + OCT-: ${heldOn} held; let go: ${notesH().length} voices`,
        before === "60 64 67" && outs.slice(0, 6).join() === "60,64,67,60,64,67" && heldOn === "60 64 67" &&
        notesH().length === 0);
}
{                                               // V0.75: one grammar for the menu buttons
  const g = await boot(), name = () => cstr(g, g.web_test_page_name()), sec = () => g.web_test_page_section();
  const toast = () => cstr(g, g.web_test_toast());
  const combo = (h, b) => { g.web_buttons(1 << h); play(g, 0.03); g.web_buttons((1 << h) | (1 << b)); play(g, 0.03); g.web_buttons(0); play(g, 0.3); };
  const twice = (b) => { for (let i = 0; i < 2; i++) { g.web_buttons(1 << b); play(g, 0.03); g.web_buttons(0); play(g, 0.08); } play(g, 0.3); };
  play(g, 0.3);
  tap(g, B.ENV);                                // ENV: a tap its menu, again the next page, through ENV 1 to ENV 2
  const e = [[sec(), name()]];
  tap(g, B.ENV);
  e.push([sec(), name()]);
  tap(g, B.ENV);
  e.push([sec(), name()]);
  twice(B.ENV);                                 // twice: the next section (ENV 2 -> ENV 1)
  e.push([sec(), name()]);
  check(`a tap opens ENV (${e[0]}), again its next page (${e[1]}), on into ENV 2 (${e[2]}); twice: the next section (${e[3]})`,
        e[0].join() === "5,ADSR" && e[1].join() === "5,SHAPE" && e[2].join() === "6,ADSR" && e[3][0] === 5);
  const n0 = cstr(g, g.web_test_patch_name());  // HOME + OCT+: the next sound, OCT-: back; twice: the presets
  combo(B.HOME, B.OCTUP);
  const n1 = cstr(g, g.web_test_patch_name());
  combo(B.HOME, B.OCTDN);
  play(g, 0.6);
  const n2 = cstr(g, g.web_test_patch_name());
  twice(B.HOME);
  const br = sec();
  tap(g, B.HOME);
  check(`HOME + OCT+: the next sound ("${n0}" -> "${n1}"), OCT-: back ("${n2}"); HOME twice: the presets (section ${br}); a tap there: back (${sec()})`,
        n1 !== n0 && n2 === n0 && br === SEC.BROWSE && sec() !== SEC.BROWSE);
  tap(g, B.EDIT);                               // EDIT held 5 s: the section shown reset; SAVE + OCT- undoes it
  const POS = ID.A_POS, set = 400;
  g.web_test_set(POS, set);
  play(g, 0.6);
  g.web_buttons(1 << B.EDIT);
  play(g, 5.3);
  const rs = [toast(), g.web_test_value(POS), g.web_test_lay()];
  g.web_buttons(0);
  play(g, 0.4);
  g.web_buttons(1 << B.SAVE);
  play(g, 0.05);
  g.web_buttons((1 << B.SAVE) | (1 << B.OCTDN)); play(g, 0.03); g.web_buttons(1 << B.SAVE); play(g, 0.03);
  g.web_buttons(0);
  play(g, 0.4);
  check(`EDIT held 5 s: "${rs[0]}" (WT POS ${set} -> ${rs[1]}, INIT's; JUMP closed: ${rs[2]}); SAVE + OCT-: back (${g.web_test_value(POS)})`,
        rs[0] === "OSC A: RESET" && rs[1] === 0 && rs[2] === 0 && g.web_test_value(POS) === set);
  combo(B.SCL, B.OCTUP);                        // SCL + OCT+: the chords on (KEY), OCT-: off
  const ch = [g.web_test_chd(0, -1), toast()];
  combo(B.SCL, B.OCTDN);
  combo(B.GLO, B.OCTUP);                        // GLO + OCT+: PERFORM's layer latched; a GLO tap lets it go
  const lay = g.web_test_lay();
  tap(g, B.GLO);
  const lay2 = g.web_test_lay();
  check(`SCL + OCT+: the chords on (${ch[0]}, "${ch[1]}"), OCT-: off (${g.web_test_chd(0, -1)}); GLO + OCT+: PERFORM's layer latched (${lay}), a GLO tap: let go (${lay2})`,
        ch[0] === 1 && ch[1] === "CHORDS KEY" && g.web_test_chd(0, -1) === 0 && lay === 1 && lay2 === 0);
  const ON = ID.CH_ON;                          // FX + OCT- on CHORUS's page: bypassed; + OCT+: in
  g.web_test_fx_on(1, 1);
  g.web_test_set(ID.FXC1, 2);
  play(g, 0.05);
  goPage(g, SEC.CHORUS, "CHORUS");
  play(g, 0.05);
  combo(B.FX, B.OCTDN);
  const byp = [g.web_test_value(ON), toast()];
  combo(B.FX, B.OCTUP);
  check(`FX + OCT- on CHORUS: bypassed (${byp[0]}, "${byp[1]}"), FX + OCT+: in (${g.web_test_value(ON)}, "${toast()}")`,
        byp[0] === 0 && byp[1] === "CHORUS BYPASSED" && g.web_test_value(ON) === 1 && toast() === "CHORUS IN");
  goPage(g, SEC.MENU, "SCREEN");                // SELECT in a menu of one section: its pages
  g.web_enc(E.SELECT, 1);
  play(g, 0.1);
  check(`SELECT on SETTINGS (one section): its next page (${name()})`, name() === "PANEL");
}
{                                               // V0.67: VALUES, the family's taps, the bloom
  const v = await boot(), FILTER = 4;
  play(v, 0.6);
  const cut = pid(v, FILTER, "TONE", 1);
  let vp = -1, tone = -1, more = -1;
  for (let q = 0; q < 200; q++) {
    v.web_test_go(q);
    if (v.web_test_page_section() !== FILTER) continue;
    const nm = cstr(v, v.web_test_page_name());
    if (nm === "VALUES" && v.web_test_page_param(1) === cut && vp < 0) vp = q;
    if (nm === "TONE") tone = q;
    if (nm === "MORE") more = q;
  }
  const quick = (page) => {                     // KNOB 2: twenty detents 5 ms apart (a quick turn)
    v.web_test_go(page);
    play(v, 0.05);
    v.web_test_set(cut, 400);
    play(v, 0.3);
    for (let i = 0; i < 20; i++) { v.web_enc(E.K2, 1); play(v, 0.005); }
    play(v, 0.05);
    return v.web_test_value(cut) - 400;
  };
  const dv = quick(vp), dt = quick(tone);
  v.web_test_go(more);
  play(v, 0.05);
  v.web_buttons(1 << B.EDIT);
  play(v, 0.05);
  v.web_buttons(0);
  play(v, 0.4);
  const after = cstr(v, v.web_test_page_name());
  check(`VALUES: FILTER's (page ${vp}, TONE's knobs): a quick turn of 20 detents moves the cutoff 20 (${dv}), on TONE ${dt}; EDIT tapped on MORE: ${after} (V0.75: the next page through the sections, VALUES skipped)`,
        vp >= 0 && dv === 20 && dt > 40 && after === "WAVE" && v.web_test_page_section() === 0);
  for (let q = 0; q < 200; q++) {               // HOME's bloom: a note swirls it and it opens; let go, it closes
    v.web_test_go(q);
    if (v.web_test_page_section() === SEC.MACRO) break;
  }
  play(v, 1.5);
  const b0 = [v.web_test_bloom(0), v.web_test_bloom(1)];
  v.web_keys(1 << 7);
  play(v, 0.15);
  const b1 = [v.web_test_bloom(0), v.web_test_bloom(1)];
  play(v, 1.6);
  const b2 = [v.web_test_bloom(0), v.web_test_bloom(1)];
  v.web_keys(0);
  play(v, 4.0);
  const b3 = [v.web_test_bloom(0), v.web_test_bloom(1)], d0 = v.web_screen_draws();
  play(v, 1.0);
  const still = v.web_screen_draws() - d0;
  check(`the bloom (open, swirl in thousandths): silence ${b0}; a note, 0.15 s: ${b1} (swirling, opening); 1.75 s: ${b2} (open, unwound); let go, 4 s: ${b3} (a bud), then ${still} blits in 1 s`,
        b0[0] === 0 && b0[1] === 0 && b1[1] > 400 && b1[0] > 100 && b2[0] > 400 && Math.abs(b2[1]) < 150 && b3[0] < 20 &&
        Math.abs(b3[1]) < 20 && still === 0);
}

{                                               // V0.8, the sequencer: playback, feel, record live and by step, patterns, chain, motion, PATTERNS, a power cycle
  const q = await boot(), qt = () => cstr(q, q.web_test_toast());
  const SMP = 44117.647 * 60 / 120 / 4;         // (a 1/16 step at 120 BPM, in samples)
  const sq = (f, v = -1) => q.web_test_sq(f, v), qs = (p, s, f, v = -1) => q.web_test_sq_step(p, s, f, v);
  const qp = (p, f, v = -1) => q.web_test_sq_pat(p, f, v), K = (n) => n - 53;
  const sent = (m0, on) => {                    // what went out over USB since m0: [note, velocity, time]
    const r = [];
    for (let i = m0; i < q.web_test_mout_n(); i++) {
      const p = q.web_test_mout(i);
      if (((p >> 8) & 0xF0) === (on ? 0x90 : 0x80)) r.push([(p >> 16) & 127, (p >> 24) & 127, q.web_test_mout_t(i)]);
    }
    return r;
  };
  const press = (b) => { q.web_buttons(1 << b); play(q, 0.03); q.web_buttons(0); play(q, 0.01); };
  const qtap = (b) => { press(b); play(q, 0.34); };
  const qchord = (held, b) => { q.web_buttons(1 << held); play(q, 0.03); q.web_buttons((1 << held) | (1 << b)); play(q, 0.03);
                                q.web_buttons(1 << held); play(q, 0.03); q.web_buttons(0); play(q, 0.35); };
  const until = (qn) => { for (let i = 0; i < 4000 && q.web_test_arp(1) < qn; i++) play(q, 0.0015); };   // the transport at qn thousandths of a quarter
  const clear = (p) => { for (let s = 0; s < 64; s++) { for (let f = 0; f < 4; f++) qs(p, s, f, 255); qs(p, s, 8, 2); qs(p, s, 9, 100); qs(p, s, 10, 50); } };
  const notes = (p, s) => [0, 1, 2, 3].map((f) => qs(p, s, f)).filter((n) => n !== 255).join(" ");
  play(q, 0.2);
  q.web_test_chd(13, 1);                        // USB OUT: what the sequencer plays read from what it sends

  for (const [s, n] of [[0, 60], [4, 64], [8, 67], [12, 72]]) qs(0, s, 0, n);   // pattern 1: C4 E4 G4 C5, a step in four
  let m0 = q.web_test_mout_n();
  press(B.PLAY);
  play(q, 2.15);
  press(B.PLAY);
  let on = sent(m0, 1), off = sent(m0, 0), t0 = on.length ? on[0][2] : 0;
  const at = (x) => Math.round((x[2] - t0) / SMP * 100) / 100;
  const lens = on.slice(0, 4).map((x) => { const o = off.find((y) => y[0] === x[0] && y[2] > x[2]); return o ? Math.round((o[2] - x[2]) / SMP * 100) / 100 : -1; });
  check(`SEQ: pattern 1 on PLAY (C4 E4 G4 C5 a step in four): ${on.map((x) => `${x[0]}@${at(x)}`).join(" ")}; each ${lens.join(" ")} of a step (LENGTH 1/2); stopped: running ${sq(10)}`,
        on.map((x) => x[0]).slice(0, 5).join() === "60,64,67,72,60" && [0, 4, 8, 12, 16].every((v, i) => on[i] && Math.abs(at(on[i]) - v) < 0.03) &&
        lens.every((x) => Math.abs(x - 0.5) < 0.03) && sq(10) === 0);

  clear(0);                                     // the feel: SWING 75 %, NUDGE -25 % on step 3, CHANCE 0 % on step 4, TRANSPOSE +12, LENGTH 4
  for (const [s, n] of [[0, 60], [1, 62], [2, 64], [3, 65]]) qs(0, s, 0, n);
  qs(0, 2, 10, 25);
  qs(0, 3, 9, 0);
  qp(0, 0, 4); qp(0, 2, 75); qp(0, 4, 36);
  m0 = q.web_test_mout_n();
  press(B.PLAY);
  play(q, 0.95);
  press(B.PLAY);
  on = sent(m0, 1); t0 = on.length ? on[0][2] : 0;
  check(`.. SWING 75 %, NUDGE -25 % (step 3), CHANCE 0 % (step 4), TRANSPOSE +12: ${on.map((x) => `${x[0]}@${at(x)}`).join(" ")}`,
        on.map((x) => x[0]).slice(0, 6).join() === "72,74,76,72,74,76" &&
        [0, 1.5, 1.75, 4, 5.5, 5.75].every((v, i) => on[i] && Math.abs(at(on[i]) - v) < 0.03));
  qs(0, 3, 9, 100); qs(0, 2, 10, 50); qp(0, 2, 50); qp(0, 4, 24);
  const dir = (d) => { qp(0, 3, d); const m = q.web_test_mout_n(); press(B.PLAY); play(q, 1.0); press(B.PLAY); return sent(m, 1).map((x) => x[0]).slice(0, 8).join(" "); };
  const back = dir(1), ping = dir(2);
  qp(0, 3, 0);
  check(`.. DIRECTION: BACKWARD ${back}; PING-PONG ${ping}`, back === "65 64 62 60 65 64 62 60" && ping === "60 62 64 65 64 62 60 62");

  sq(0, 1);                                     // recording, live: pattern 2, REC armed, PLAY; C4 just after step 3, E4 just before step 6
  qtap(B.REC);
  const armTxt = qt(), armed = sq(5);
  m0 = q.web_test_mout_n();
  press(B.PLAY);
  until(525);                                   // (step 2.1)
  q.web_keys(1 << K(60));
  play(q, 0.2);
  q.web_keys(0);
  until(1200);                                  // (step 4.8)
  q.web_keys(1 << K(64));
  play(q, 0.06);
  const fresh = sent(m0, 1).filter((x) => x[0] === 64).length;
  q.web_keys(0);
  play(q, 0.1);
  sq(4, 1);                                     // QUANTIZE: G4 near step 8, on it
  until(1980);
  q.web_keys(1 << K(67));
  play(q, 0.05);
  q.web_keys(0);
  until(5400);                                  // (the next pass: the notes recorded play)
  const again = sent(m0, 1).filter((x) => x[0] === 64).length;
  press(B.PLAY);
  sq(4, 0);
  const r2 = [notes(1, 2), qs(1, 2, 10) - 50, qs(1, 2, 8)], r5 = [notes(1, 5), qs(1, 5, 10) - 50], r8 = [notes(1, 8), qs(1, 8, 10) - 50];
  check(`.. REC ("${armTxt}", armed ${armed}), live: C4 at step 2.1 into step 3 (${r2[0]}, NUDGE ${r2[1]} %, LENGTH ${r2[2]} quarter steps: held 0.2 s); E4 at 4.8 into step 6 (${r5[0]}, ${r5[1]} %; not played again as it came: ${fresh}, the next pass: ${again}); QUANTIZE: G4 into step 9 (${r8[0]}, ${r8[1]} %)`,
        armTxt === "REC ARMED" && armed === 1 && r2[0] === "60" && Math.abs(r2[1] - 10) <= 3 && Math.abs(r2[2] - 6) <= 1 &&
        r5[0] === "64" && Math.abs(r5[1] + 20) <= 3 && fresh === 0 && again >= 1 && r8[0] === "67" && r8[1] === 0);

  clear(2);                                     // REPLACE: a pass with REC armed clears what the playhead passes
  sq(0, 2);
  for (const s of [0, 3, 7, 11]) qs(2, s, 0, 48 + s);
  sq(3, 1);
  press(B.PLAY);
  play(q, 2.1);
  press(B.PLAY);
  const left = qp(2, 7);
  sq(3, 0);
  check(`.. REPLACE: pattern 3's four notes after a pass armed: ${left ? "some left" : "none left"}`, left === 0);

  qtap(B.SEQ);                                  // by step: stopped, armed, SEQ's menu shown: the keys fill the step picked
  const ssec = q.web_test_page_section(), spg = cstr(q, q.web_test_page_name()), srec = sq(18);
  sq(0, 3); sq(6, 0);
  q.web_keys((1 << K(60)) | (1 << K(64)));
  play(q, 0.05);
  q.web_keys(0);
  play(q, 0.05);
  const c1 = sq(6);
  q.web_keys(1 << K(67));
  play(q, 0.05);
  q.web_keys(0);
  play(q, 0.05);
  const st0 = notes(3, 0), st1 = notes(3, 1), c2 = sq(6);
  check(`.. by step (SEQ: section ${ssec}, ${spg}; step recording ${srec}): C4 + E4 into step 1 (${st0}), then step ${c1 + 1} picked; G4 into step 2 (${st1}), then step ${c2 + 1}`,
        ssec === SEC.SEQ && spg === "NOTES" && srec === 1 && st0 === "60 64" && c1 === 1 && st1 === "67" && c2 === 2);
  qtap(B.REC);

  qchord(B.SEQ, B.OCTUP);                       // SEQ + OCT+ / OCT-: the next pattern, the one before; playing, at the end of the one playing
  const nx = [sq(0), qt()];
  qchord(B.SEQ, B.OCTDN);
  const pv = sq(0);
  press(B.PLAY);
  play(q, 0.1);
  qchord(B.SEQ, B.OCTUP);
  const queued = [sq(0), sq(1), qt()];
  play(q, 1.6);
  const switched = [sq(0), sq(1)];
  press(B.PLAY);
  check(`.. SEQ + OCT+: pattern ${nx[0] + 1} ("${nx[1]}"); OCT-: ${pv + 1}; playing, OCT+: still ${queued[0] + 1}, ${queued[1] + 1} next ("${queued[2]}"); at its end: ${switched[0] + 1} (next ${switched[1]})`,
        nx[0] === 4 && nx[1] === "PATTERN 5" && pv === 3 && queued[0] === 3 && queued[1] === 4 && queued[2] === "NEXT: PATTERN 5" &&
        switched[0] === 4 && switched[1] === 255);

  for (const p of [0, 1]) { clear(p); qp(p, 0, 4); qs(p, 0, 0, 60 + p); }   // CHAIN: pattern 1 twice, pattern 2, round again
  q.web_test_sq_chain(0, 0, 0); q.web_test_sq_chain(0, 1, 2); q.web_test_sq_chain(1, 0, 1); q.web_test_sq_chain(1, 1, 1);
  q.web_test_sq_chain(2, 0, 255);
  sq(2, 1);
  m0 = q.web_test_mout_n();
  press(B.PLAY);
  play(q, 3.05);
  press(B.PLAY);
  const chainSeq = sent(m0, 1).map((x) => x[0] - 59).join(" ");
  sq(2, 0);
  check(`.. CHAIN (1 twice, 2): the patterns played ${chainSeq}`, chainSeq.startsWith("1 1 2 1 1 2"));

  const CUT = pid(q, 4, "TONE", 1), cut0 = q.web_test_value(CUT);   // MOTION: a lane holds the cutoff at steps 1 and 3, the patch untouched
  clear(4);
  qp(4, 0, 4); qp(4, 5, CUT);
  qs(4, 0, 11, 254); qs(4, 2, 11, 0);
  sq(0, 4);
  press(B.PLAY);
  until(60);
  const mo0 = [sq(14), sq(15), q.web_test_follow(CUT), q.web_test_value(CUT)];
  until(310);
  const mo1 = [sq(14), q.web_test_follow(CUT)];
  until(560);
  const mo2 = [sq(14), sq(15), q.web_test_follow(CUT)];
  press(B.PLAY);
  play(q, 0.05);
  const mo3 = [sq(14), q.web_test_follow(CUT), q.web_test_value(CUT)];
  check(`.. MOTION: step 1 holds the cutoff (${mo0[0] === CUT ? "it" : mo0[0]}) at ${mo0[1]} (followed ${mo0[2]}, the patch ${mo0[3]}); step 2 lets go (${mo1[0]}, ${mo1[1]}); step 3 at ${mo2[1]} (${mo2[2]}); stopped: ${mo3[0]}, ${mo3[1]}, the patch ${mo3[2]}`,
        mo0[0] === CUT && mo0[1] === 1000 && mo0[2] === 1000 && mo0[3] === cut0 && mo1[0] === 65535 && mo1[1] === cut0 &&
        mo2[0] === CUT && mo2[1] === 0 && mo2[2] === 0 && mo3[0] === 65535 && mo3[1] === cut0 && mo3[2] === cut0);
  clear(5);                                     // .. recorded: armed and playing, the cutoff's knob turned: a lane for it, its values at the steps it passes
  qp(5, 0, 8);
  sq(0, 5);
  sq(5, 1);
  for (let g = 0; g < 200 && !(q.web_test_page_section() === 4 && cstr(q, q.web_test_page_name()) === "TONE"); g++) q.web_test_go(g);
  press(B.PLAY);
  until(100);
  let held = 1;
  for (let i = 0; i < 12; i++) { q.web_enc(E.K2, -3); play(q, 0.04); if (sq(14) !== 65535) held = 0; }
  until(2000);
  press(B.PLAY);
  sq(5, 0);
  const lane = qp(5, 5), vals = Array.from({ length: 8 }, (_, s) => qs(5, s, 11)).filter((x) => x !== 255).length;
  q.web_test_set(CUT, cut0);
  check(`.. MOTION recorded: the cutoff's knob turned while playing armed: lane 1 the cutoff (${lane === CUT ? "it" : lane}), ${vals} steps with values; the knob heard while it turned (${held})`,
        lane === CUT && vals >= 3 && held === 1);

  qtap(B.SEQ);                                  // PATTERNS (SEQ held): a white key picks, COPY, CLEAR, PASTE (the black ones at the left)
  clear(2); qs(2, 0, 0, 62);
  q.web_buttons(1 << B.SEQ);
  play(q, 0.8);
  const lay = q.web_test_lay();
  q.web_keys(1 << 4); play(q, 0.05); q.web_keys(0); play(q, 0.05);   // (A3: pattern 3)
  const picked = sq(0);
  q.web_keys(1 << 1); play(q, 0.05); q.web_keys(0); play(q, 0.05);   // (F#3: COPY)
  const copied = sq(20);
  q.web_keys(1 << 5); play(q, 0.05); q.web_keys(0); play(q, 0.05);   // (A#3: CLEAR)
  const cleared = qp(2, 7), clrTxt = qt();
  q.web_keys(1 << 3); play(q, 0.05); q.web_keys(0); play(q, 0.05);   // (G#3: PASTE)
  const pasted = notes(2, 0);
  play(q, 5.0);                                 // (held on past 5 s: a key used, no reset)
  q.web_buttons(0);
  play(q, 0.3);
  check(`.. PATTERNS: SEQ held (layer ${lay}); A3: pattern ${picked + 1}; F#3 copies (${copied}); A#3 clears (holds ${cleared}: "${clrTxt}"); G#3 pastes (${pasted}); let go: closed (${q.web_test_lay()}), nothing reset (${notes(2, 0)})`,
        lay === 5 && picked === 2 && copied === 1 && cleared === 0 && clrTxt === "PATTERN 3 CLEARED: PASTE UNDOES" && pasted === "62" &&
        q.web_test_lay() === 0 && notes(2, 0) === "62");

  const g0 = [q.web_test_page_section(), cstr(q, q.web_test_page_name())];   // the grammar: a tap the next page, twice the next section; held 5 s: the steps cleared
  qtap(B.SEQ);
  const g1 = cstr(q, q.web_test_page_name());
  press(B.SEQ); press(B.SEQ); play(q, 0.35);
  const g2 = [q.web_test_page_section(), cstr(q, q.web_test_page_name())];
  for (let g = 0; g < 200 && q.web_test_page_section() !== SEC.SEQ; g++) q.web_test_go(g);
  q.web_buttons(1 << B.SEQ);
  play(q, 5.3);
  q.web_buttons(0);
  play(q, 0.1);
  const rs = [qt(), qp(2, 7)];
  check(`.. SEQ's taps: ${g0[1]} (section ${g0[0]}), then ${g1}; twice: section ${g2[0]} (${g2[1]}); held 5 s on SEQ: "${rs[0]}" (pattern 3 holds ${rs[1]})`,
        g0[0] === SEC.SEQ && g1 !== g0[1] && g2[0] === SEC.SPAT && rs[0] === "PATTERN 3 CLEARED: PASTE UNDOES" && rs[1] === 0);

  qs(6, 9, 0, 70); qs(6, 9, 4, 90); qs(6, 9, 8, 7); qp(6, 0, 24); qp(6, 1, 5);   // a power cycle: the patterns and the sequencer's settings kept
  sq(0, 6); sq(2, 1); sq(3, 1); q.web_test_sq_chain(3, 0, 6); q.web_test_sq_chain(3, 1, 4);
  play(q, 6.0);
  const nor = new Uint8Array(q.memory.buffer, q.web_nor(), q.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const q2 = instance.exports;
  if (q2._initialize) q2._initialize();
  new Uint8Array(q2.memory.buffer, q2.web_nor(), q2.web_nor_size()).set(nor);
  q2.web_master(700);
  q2.web_boot();
  play(q2, 0.3);
  const kp = [q2.web_test_sq_step(6, 9, 0, -1), q2.web_test_sq_step(6, 9, 4, -1), q2.web_test_sq_step(6, 9, 8, -1),
              q2.web_test_sq_pat(6, 0, -1), q2.web_test_sq_pat(6, 1, -1), q2.web_test_sq_step(1, 0, 0, -1)];
  const ks = [q2.web_test_sq(0, -1), q2.web_test_sq(2, -1), q2.web_test_sq(3, -1), q2.web_test_sq_chain(3, 0, -1), q2.web_test_sq_chain(3, 1, -1)];
  check(`.. after a power cycle: pattern 7's step 10 ${kp.slice(0, 3).join(" ")}, LENGTH ${kp[3]}, RATE ${kp[4]}; pattern 2's step 1 ${kp[5]}; the pattern shown ${ks[0] + 1}, CHAIN ${ks[1]}, RECORD ${ks[2]}, slot 4 ${ks[3] + 1} x${ks[4]}`,
        kp.join() === "70,90,7,24,5,61" && ks.join() === "6,1,1,6,4");
}

{                                               // V0.8 BENCH: CORE 2 (sy_c2.c): the emulator has one core
  const k = await boot(), kt = () => cstr(k, k.web_test_toast());
  const pg = goPage(k, SEC.MENU, "CORE 2");
  play(k, 0.1);
  const st = k.web_test_c2(0, -1), load = k.web_test_c2(2, -1);
  k.web_enc(E.K2, 1);                           // TEST: refused, one core here
  play(k, 0.1);
  const t1 = kt();
  k.web_enc(E.K1, 1);                           // ON (at the next power-on)
  play(k, 0.1);
  const t2 = kt();
  k.web_enc(E.K3, -2);                          // LOAD 16 -> 8
  play(k, 0.1);
  const l2 = k.web_test_c2(2, -1);
  check(`CORE 2: the page (${pg}); one core here (state ${st}); LOAD ${load}; TEST refused ("${t1}"); ON ("${t2}"); LOAD turned down twice: ${l2}`,
        pg >= 0 && st === 5 && load === 16 && t1 === "ONE CORE HERE" && t2 === "ON AFTER A RESTART" && l2 === 8 &&
        k.web_test_c2(1, -1) === 1);
  play(k, 3.8);                                 // (3 s of quiet: the settings written) a power cycle
  const nor = new Uint8Array(k.memory.buffer, k.web_nor(), k.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const k2 = instance.exports;
  if (k2._initialize) k2._initialize();
  new Uint8Array(k2.memory.buffer, k2.web_nor(), k2.web_nor_size()).set(nor);
  k2.web_master(700);
  k2.web_boot();
  play(k2, 0.3);
  check(`.. after a power cycle: ON ${k2.web_test_c2(1, -1)}, LOAD ${k2.web_test_c2(2, -1)}; still one core (state ${k2.web_test_c2(0, -1)})`,
        k2.web_test_c2(1, -1) === 1 && k2.web_test_c2(2, -1) === 8 && k2.web_test_c2(0, -1) === 5);
}

console.log(`emu: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
