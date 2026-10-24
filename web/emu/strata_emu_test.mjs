// SPDX-License-Identifier: GPL-3.0-only
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
              KEYS: 41, ARP: 42, CHORD: 43, CMID: 44, STEPS: 45, SEQ: 46, LTRK: 47, LCLIP: 48, LSONG: 49, ENV1: 5,
              ENV2: 6, KIT: 50, DRUM: 52, DEBUG: 53 };   // SEC_*
const ID = { A_TABLE: 1, A_POS: 2, A_WARP: 3, A_WAMT: 4, L1_SHAPE: 78, L1_NOTE: 166, DL_NOTE: 168,   // sy_param.c SP_*
             EQ_ON: 169, EQ_LOW: 170, EQ_MID: 171, EQ_HIGH: 172, EQ_LFREQ: 173, EQ_MFREQ: 174, EQ_Q: 175, EQ_HFREQ: 176,
             FX_ORDER: 177, EQ_LQ: 178, EQ_HQ: 179, FXC1: 180, A_ON: 0, CH_ON: 135, CH_MIX: 140, OT_ON: 158,
             PH_ON: 236, EQ2_MID: 209, EQPT: 1009, EQF: 1010, EQG: 1011, EQQ: 1012,
             LPT: 1002, LX: 1003, LY: 1004, LC: 1005, BPM: 1006, TSB: 1007, TSN: 1008, CLOCK: 1067, DRAWN: 12,
             EQSHP: 1123, EQSF: 1124, EQSA: 1125 };  // sy_ui.c UI_*, LFO_DRAW
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
  ex.web_test_sound_set(0);                     // (V0.87: the boot plays INIT; the tests below start on sound 0)
  return ex;
}
const splash = (ex) => play(ex, 2.1);           // (V0.86, P4: the splash held 2 s, then HOME)
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
const saveTwice = (x) => {                      // SAVE twice (V0.86: PRESETS' list loads the one picked)
  for (let i = 0; i < 2; i++) { x.web_buttons(1 << B.SAVE); play(x, 0.03); x.web_buttons(0); play(x, 0.08); }
  play(x, 0.3);
};
const twiceB = (x, b) => {                      // a menu button tapped twice (V0.87: HOME the SETTINGS, EDIT the lists)
  for (let i = 0; i < 2; i++) { x.web_buttons(1 << b); play(x, 0.03); x.web_buttons(0); play(x, 0.08); }
  play(x, 0.3);
};
const octHold = (x, b, secs) => {               // V0.87 (U2): a menu button + OCT- held secs (5: what it shows reset)
  x.web_buttons(1 << b); play(x, 0.05); x.web_buttons((1 << b) | (1 << B.OCTDN)); play(x, secs);
  const r = [cstr(x, x.web_test_cd()), cstr(x, x.web_test_toast())];
  x.web_buttons(1 << b); play(x, 0.05); x.web_buttons(0); play(x, 0.4);
  return r;
};
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
{                                               // (V0.86, P4: the splash 2 s, then HOME's SOUND; EDIT: OSC A's WAVE)
  const s0 = ex.web_test_page_section();
  splash(ex);
  const s1 = ex.web_test_page_section(), n1 = cstr(ex, ex.web_test_page_name());
  tap(ex, B.EDIT);
  check(`the first page: HOME ${n1} (section ${s1}, ${s0} under the splash), sound "${cstr(ex, ex.web_test_patch_name())}"; EDIT: OSC A ${cstr(ex, ex.web_test_page_name())} (section ${ex.web_test_page_section()})`,
        s1 === SEC.MACRO && n1 === "SOUND" && ex.web_test_page_section() === SEC.OSCA &&
        cstr(ex, ex.web_test_page_name()) === "WAVE" && cstr(ex, ex.web_test_patch_name()) === "SUPERSAW");
}
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
  play(ex, 0.012);                              // (the turn's frame; the table built redraws once more after)
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
  tap(ex, B.FX);                                // (V0.86, C3: once in its menu, the chain view, the module picked)
  check(`FX: the chain view (section ${s1}: ${c}); SELECT the slot (${slot}); FX twice opens REVERB (${s2}); ALGORITHM its next page (${s3}), SELECT the module before (${s4}); FX once: back (${ex.web_test_page_section()}, CHORUS picked: slot ${ex.web_test_cslot()})`,
        s1 === SEC.CHAIN && c.join() === "2,4,0" && slot === 1 && s2 === SEC.REVERB && p3 !== p2 && s3 === SEC.REVERB &&
        s4 === SEC.CHORUS && ex.web_test_page_section() === SEC.CHAIN && ex.web_test_cslot() === 0);
}
{                                               // PRESETS' list (V0.86, the renders' note): PRESETS moves through it,
  ex.web_enc(E.PRESET, 1);                      // nothing loading; V0.87: 5 s untouched it goes, INIT first then A-Z
  play(ex, 0.3);
  const o0 = [ex.web_test_pl(0), ex.web_test_pl(1), cstr(ex, ex.web_test_patch_name())], at0 = ex.web_test_pl_id(o0[1]);
  play(ex, 3.0);
  const o3 = ex.web_test_pl(0);
  play(ex, 2.2);
  const o1 = [ex.web_test_pl(0), cstr(ex, ex.web_test_patch_name())];
  ex.web_enc(E.PRESET, 1);
  play(ex, 0.3);
  const want = ex.web_test_pl_id(ex.web_test_pl(1));
  saveTwice(ex);
  check(`PRESETS: the list opens, the next picked (${o0}, id ${at0}), nothing loaded; open at 3 s (${o3}), gone at 5 s (${o1}); again, SAVE twice: "${cstr(ex, ex.web_test_patch_name())}" loaded (${ex.web_test_sound()}), the list gone (${ex.web_test_pl(0)}); INIT first (${ex.web_test_pl_id(0)})`,
        o0[0] === 1 && at0 > 0 && o0[2] === "SUPERSAW" && o3 === 1 && o1.join() === "0,SUPERSAW" && ex.web_test_sound() === want &&
        ex.web_test_pl(0) === 0 && ex.web_test_pl_id(0) === ex.web_test_pl_id(0xFFFF));
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
  tap(ex, B.REC);                               // (V0.8: a tap records on the selected track: an empty clip, stopped: COUNT-IN; again: off)
  const armed = ex.web_test_lp(10, -1), armTxt = cstr(ex, ex.web_test_toast());
  tap(ex, B.REC);
  const one = ex.web_test_mod(), disarmed = ex.web_test_lp(10, -1);
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons((1 << B.REC) | (1 << B.OCTUP)); play(ex, 0.03);
  ex.web_buttons(1 << B.REC); play(ex, 0.03); ex.web_buttons(0); play(ex, 0.35);
  const latched = ex.web_test_mod();
  tap(ex, B.REC);
  check(`REC: a tap on an empty clip, stopped: the count-in (${armed}, "${armTxt}"), again off (${disarmed}; the layer ${one}); REC + OCT+ latches the layer (${latched}), a tap lets it go (${ex.web_test_mod()}; recording ${ex.web_test_lp(10, -1)})`,
        armed === 1 && armTxt === "T1 A: COUNT-IN" && disarmed === 0 && one === 0 && latched === 2 && ex.web_test_mod() === 0 &&
        ex.web_test_lp(10, -1) === 0 && ex.web_test_lp(15, -1) === 0);
}
{                                               // HOME held off HOME: the help map as long as held, never DEBUG (V0.87.1, item 1)
  const toast = () => cstr(ex, ex.web_test_toast()), page0 = ex.web_test_page(), sec0 = ex.web_test_page_section();
  ex.web_buttons(1 << B.HOME);
  play(ex, 0.5);
  const h1 = cstr(ex, ex.web_test_cd());
  play(ex, 5.5);
  const h2 = [cstr(ex, ex.web_test_cd()), ex.web_test_page_section()];
  ex.web_buttons(0);
  play(ex, 0.1);
  check(`HOME held off HOME (section ${sec0}): the help map, no countdown ("${h1}"), still at 6 s ("${h2[0]}", section ${h2[1]}); let go: closed ("${cstr(ex, ex.web_test_cd())}"), the page kept (${ex.web_test_page() === page0}), DEBUG never opened`,
        sec0 !== SEC.MACRO && h1 === "HELP: LET GO" && h2[0] === "HELP: LET GO" && h2[1] === sec0 && cstr(ex, ex.web_test_cd()) === "" &&
        ex.web_test_page() === page0 && toast() === "");
  tap(ex, B.HOME);                              // (HOME: the page HOME)
  check(`.. HOME tapped: HOME (section ${ex.web_test_page_section()})`, ex.web_test_page_section() === SEC.MACRO);
}
{                                               // the SETTINGS (HOME twice), DEBUG (HOME held 5 s on HOME), its palettes
  const toast = () => cstr(ex, ex.web_test_toast()), page0 = ex.web_test_page();
  ex.web_buttons(1 << B.HOME);
  play(ex, 1.5);
  const count = cstr(ex, ex.web_test_cd()), early = ex.web_test_page_section();   // (the countdown's screen)
  ex.web_buttons(0);
  play(ex, 0.1);
  const after = [ex.web_test_page(), toast()];
  check(`HOME held: the help map, its countdown ("${count}"); let go at 1.5 s, nothing opens (section ${early}, the page kept, the footer "${after[1]}")`,
        count === "HELP: DEBUG 4" && early !== SEC.MENU && after[0] === page0 && after[1] === "" && cstr(ex, ex.web_test_cd()) === "");
  ex.web_buttons(1 << B.HOME);                  // (V0.87, U4, U5: held 5 s the DEBUG menu, twice the SETTINGS)
  play(ex, 5.2);
  ex.web_buttons(0);
  play(ex, 0.05);
  const dbg = [ex.web_test_page_section(), cstr(ex, ex.web_test_page_name())];
  twiceB(ex, B.HOME);
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
  check(`HOME held 5 s: DEBUG (${dbg}); twice: the SETTINGS (section ${sec}); KNOB 1 changes the palette (${p0} -> ${p1}, ${moved} pixels with it); round to the twelfth, ${neon} (${p11}); the eighth, ${mono} (${p7}): ${grey} pixels not grey`,
        dbg.join() === `${SEC.DEBUG},BENCH` && sec === SEC.MENU && p0 === 0 && p1 === 1 && moved > 2000 && p11 === 11 && neon === "NEON" && p7 === 7 &&
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
  ex.web_buttons(1 << B.ENV);                   // V0.87 (U3): ENV + OCT+ / OCT-: nothing (no layer)
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
  check(`ENV + OCT+: no layer (${lat}; ENV let go: ${still}); ENV + OCT-: none (${ex.web_test_mod()}); the page stays (${p4}, ${name()}), no tap`,
        lat === 0 && still === 0 && ex.web_test_mod() === 0 && p4 === "WAVE" && name() === "WAVE" &&
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
  const [count] = octHold(ex, B.LFO, 2.5);      // (V0.87, U2: LFO + OCT- held, its countdown from 2 s; let go: kept)
  const kept = [ex.web_test_value(ID.L1_SHAPE), n()];
  const [, reset] = octHold(ex, B.LFO, 5.3);
  const r = [ex.web_test_value(ID.L1_SHAPE), n()];
  check(`.. the first point picked: "${ends}" (${n1}); LFO + OCT- held: "${count}", let go at 2.5 s: kept (SHAPE ${kept[0]}, ${kept[1]} points); held 5 s: "${reset}" (SHAPE ${r[0]}, ${r[1]} points)`,
        ends === "THE END POINTS STAY" && n1 === 15 && count.startsWith("RESET LFO 1") && kept[0] === ID.DRAWN &&
        kept[1] === 15 && reset === "LFO 1: RESET" && r[0] === 0 && name() === "POINTS");
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
  const dsec = ex.web_test_page_section(), [DSYNC, DTIME] = [ex.web_test_page_param(0), ex.web_test_page_param(1)];
  const t0 = ex.web_test_value(DTIME), d0 = ex.web_test_value(ID.DL_NOTE), ds0 = ex.web_test_value(DSYNC);
  ex.web_enc(E.K2, 1);                          // (V0.86: SYNC on KNOB 1, TIME on KNOB 2)
  play(ex, 0.03);
  const d1 = ex.web_test_value(ID.DL_NOTE);
  ex.web_enc(E.K1, -1);
  play(ex, 0.03);
  ex.web_enc(E.K2, 1);
  play(ex, 0.03);
  check(`DELAY (section ${dsec}): SYNC FIXED (${ds0}): KNOB 2 steps the note (1/8D ${d0} -> ${d1}); FREE: KNOB 2 the time in ms (${t0} -> ${ex.web_test_value(DTIME)})`,
        dsec === SEC.DELAY && ds0 === 1 && d0 === 10 && d1 === 11 && ex.web_test_value(DTIME) === t0 + 1 &&
        ex.web_test_value(ID.DL_NOTE) === 11);
}
{                                               // GLO: GLOBAL's tempo and time signature
  goPage(ex, SEC.GLOBAL, "TEMPO");              // (V0.86, C3: GLO once in its menu, its first page: VOICE's)
  tap(ex, B.GLO);
  const first = [ex.web_test_page_section(), name()];
  goPage(ex, SEC.GLOBAL, "TEMPO");
  play(ex, 0.03);
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
  check(`GLO once on GLOBAL: its first page (${first}); GLOBAL (section ${sec}) ${name()} (${items}): ${a[0]} BPM ${a[1]}/${a[2]}`,
        first.join() === [SEC.VOICE, "VOICE"].join() && sec === SEC.GLOBAL && name() === "TEMPO" && items.join() === [ID.BPM, ID.TSB, ID.TSN, ID.CLOCK].join() &&
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
  const toast = () => cstr(ex, ex.web_test_toast()), pos = ex.web_test_page_param(0);
  ex.web_test_set(pos, 500);                    // (V0.87: the sound the list loaded may sit at its top)
  play(ex, 1.2);
  const v0 = ex.web_test_value(pos);
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
  ex.web_enc(E.PRESET, -1);                     // (V0.86: the list, SAVE twice)
  play(ex, 0.1);
  saveTwice(ex);
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
        eq1[0] === "POINTS" && eq1.slice(1).join() === [ID.EQPT, ID.EQF, ID.EQG, ID.EQQ].join() && eq2[0] === "SHAPES" &&
        eq2.slice(1).join() === [ID.EQSHP, ID.EQSF, ID.EQSA, -1].join());
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
  splash(m);
  mtap(B.SEQ);
  const seq = [cstr(m, m.web_test_page_name()), m.web_test_page_section()];
  goPage(m, SEC.MATRIX, "SLOTS");               // (V0.87, U14: the MATRIX in EDIT, after the FILTER)
  play(m, 0.1);
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
  check(`SEQ: its menu (section ${seq[1]}, ${seq[0]}); EDIT's MATRIX (section ${sec}, ${pg}); KNOB 1 picks a slot, round (slot 1's source ${first}, slot 8's ${SRC8}); KNOB 2 its source (${s0} -> ${src}), KNOB 3 where it goes (${dst}), KNOB 4 how much (${amt}); ${changed(a, b)} pixels`,
        seq[0] === "CLIPS" && seq[1] === SEC.SEQ && sec === SEC.MATRIX && pg === "SLOTS" && SRC8 === first + 21 && DST8 === SRC8 + 1 && AMT8 === SRC8 + 2 &&
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
  const MAC1 = (() => { const p = h.web_test_page(); goPage(h, SEC.MACRO, "SOUND"); const v = h.web_test_page_virt(0); h.web_test_go(p); return v; })();
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
  for (let i = 0; i < 3; i++) { h.web_enc(E.K1, -1); play(h, 0.13); }   // (V0.86: EDIT once is WAVE: WT POS at its top)
  play(h, 0.1);
  const p1 = h.web_test_value(K1);
  letGo();
  check(`LFO held on OSC A's ${pg}: MOMENTARY (layer ${mom}); KNOB 1 moves its value ${p0} -> ${p1}; LFO let go: back to ${h.web_test_value(K1)}`,
        mom === 3 && K1 >= 0 && K1 < 1000 && p1 !== p0 && h.web_test_value(K1) === p0);
  goPage(h, SEC.OSCA, "WAVE");                  // V0.87 (U6): JUMP gone; EDIT twice: the tables of the oscillator shown
  play(h, 0.05);
  const t0 = h.web_test_value(ID.A_TABLE);
  twiceB(h, B.EDIT);
  const tl = [h.web_test_pl(0), h.web_test_pl(3), h.web_test_pl(4), h.web_test_lay()];
  h.web_enc(E.PRESET, 1); play(h, 0.1);
  twiceB(h, B.EDIT);                            // (V0.87.1: the opener twice loads)
  check(`EDIT twice on OSC A: its tables' list (open ${tl[0]}, kind ${tl[1]}, OSC ${tl[2]}, no layer ${tl[3]}); PRESETS, EDIT twice: OSC A's TABLE ${t0} -> ${h.web_test_value(ID.A_TABLE)}, the list gone (${h.web_test_pl(0)})`,
        tl.join() === "1,2,0,0" && h.web_test_value(ID.A_TABLE) !== t0 && h.web_test_pl(0) === 0);
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
  splash(g);
  twiceB(g, B.HOME);                            // HOME twice (V0.87, U4): the SETTINGS
  const pages = [gn()];
  g.web_enc(E.K3, -1);                          // SCREEN: KNOB 3 the LEDs' LIGHTS (V0.87, U9), HIGH -> MED
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
  check(`the SETTINGS' pages (${pages.join(", ")}); LIGHTS MED (${glow[0]}, the LEDs' level ${glow[1]}); HOLD ${hold} (500 ms); MIDI on channel ${ch}: channel 1's note not heard (${v1} voices), channel 2's played (${v2}); PROGRAM ${pc}: program change 3, the fourth starter sound (${snd[0]}, "${snd[1]}")`,
        pages.join() === "SCREEN,PANEL,MIDI" && glow[0] === 2 && glow[1] === 2 && hold === 2 && ch === 2 && v1 === 0 &&
        v2 === 1 && pc === 1 && snd[0] === 3);
  g.web_enc(E.K2, 1);                           // LEARN on; on a sound's page KNOB 1 picks its parameter, a CC is
  play(g, 0.13);                                // learned to it; OCT+ done; the CC then sets it
  const lw = gt();
  gb(B.EDIT);
  const K1 = g.web_test_page_virt(0), pg = gn();
  g.web_enc(E.K1, 1);
  play(g, 0.13);
  const pick = g.web_test_set_get(9, -1);
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
  check(`HELP: entering the effects, the footer's hint ("${hint}"); after a power cycle: HELP ${back[0]}, LIGHTS ${back[1]}, HOLD ${back[2]}, the channel ${back[3]}, PROGRAM ${back[4]}, CC 20 learned (${g2.web_test_learned(20)})`,
        hint === "AGAIN: THE MODULE" && back.join() === "0,2,2,2,1" && g2.web_test_learned(20) === K1);
  goPage(g, SEC.MENU, "MIDI");                  // CLEAR: turned once, a word; again within 2 s, every learned CC
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
  tap(ex, B.HOME);                              // (V0.86: once the page left, again in its menu its first: SOUND)
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
  const sw0 = [ex.web_test_win(1), ex.web_test_win(2), ex.web_test_page_section()];
  btn(B.SAVE);                                  // (SAVE AS NEW picked: NAME IT, the name's next version)
  play(ex, 0.4);
  const nm0 = [ex.web_test_page_section(), sname()];
  btn(B.SAVE);
  play(ex, 0.5);
  const s1 = [toast(), ex.web_test_page_section(), ex.web_test_user_slot(), slotName(0), name()];
  check(`SAVE once on a starter: the SAVE window (${sw0[0]}), SAVE AS NEW picked (${sw0[1]}), over the page (${sw0[2]}); SAVE: NAME IT (${nm0}); SAVE: "${s1[0]}", "${s1[4]}" in USER ${s1[2] + 1}, back on the page (${s1[1]}); ${ex.web_flash_writes_count() - w0} flash writes, ${ex.web_test_refused()} refused`,
        sw0.join() === `1,1,${SEC.OSCA}` && nm0[0] === SEC.SAVE && nm0[1] === `${name0} v2` && s1[0] === `SAVED: ${name0} v2 · USER 1` &&
        s1[1] === SEC.OSCA && s1[2] === 0 && s1[3] === `${name0} v2` && s1[4] === `${name0} v2` && ex.web_flash_writes_count() > w0 &&
        ex.web_test_refused() === 0);
  btn(B.SAVE);                                  // (on USER 1: SAVE over it picked first; OCT+ SAVE AS NEW)
  play(ex, 0.5);
  const sw1 = [ex.web_test_win(2)];
  btn(B.OCTUP);
  sw1.push(ex.web_test_win(2), ex.web_test_oct());
  btn(B.SAVE); play(ex, 0.4); btn(B.SAVE); play(ex, 0.5);
  check(`.. SAVE on USER 1: SAVE over it picked (row ${sw1[0] + 1}), OCT+ SAVE AS NEW (row ${sw1[1] + 1}, the octave kept ${sw1[2]}): "${name()}" in USER ${ex.web_test_user_slot() + 1} ("${toast()}"); USER 1 kept ("${slotName(0)}")`,
        sw1.join() === "0,1,0" && name() === `${name0} v3` && ex.web_test_user_slot() === 1 && slotName(0) === `${name0} v2` &&
        slotName(1) === `${name0} v3`);
  const POS = ID.A_POS, p0 = ex.web_test_slot_value(1, POS);    // WT POS changed first
  ex.web_test_set(POS, p0 > 500 ? p0 - 40 : p0 + 40);
  const p1 = ex.web_test_value(POS);
  btn(B.SAVE); play(ex, 0.5);
  ex.web_enc(E.K1, 1); play(ex, 0.1);           // (a knob: the window shut, the turn taken)
  const shut = [ex.web_test_win(1), ex.web_test_slot_value(1, POS), ex.web_test_value(POS)];
  btn(B.SAVE); play(ex, 0.5);
  btn(B.OCTUP); btn(B.OCTUP);                   // (CANCEL)
  const cpos = ex.web_test_win(2);
  btn(B.SAVE); play(ex, 0.3);
  const cancel = [ex.web_test_win(1), ex.web_test_slot_value(1, POS)];
  btn(B.SAVE); play(ex, 0.5);
  btn(B.SAVE); play(ex, 0.6);
  check(`.. the SAVE window: a knob shuts it (${shut[0]}, USER 2 ${shut[1]}, the sound ${shut[2]}); CANCEL (row ${cpos + 1}): nothing (${cancel}); SAVE: over USER 2 ("${toast()}"), its WT POS ${p0} -> ${ex.web_test_slot_value(1, POS)}`,
        shut[0] === 0 && shut[1] === p0 && shut[2] === p1 && cpos === 2 && cancel[0] === 0 && cancel[1] === p0 &&
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
  ex.web_enc(E.K2, ex.web_test_pl_user(0) - ex.web_test_pl_user(1));   // a load (USER 1, the list A-Z), undone there (SAVE held + OCT-), the browser staying
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
  ex.web_enc(E.K2, ex.web_test_pl_user(0) - ex.web_test_pl_user(1));   // (USER 1)
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
  ex.web_enc(E.PRESET, -99);                    // PRESETS (V0.86: the list, no round): down to the first starter,
  play(ex, 0.1);                                // SAVE twice
  saveTwice(ex);
  const st = [name(), ex.web_test_user_slot()];
  btn(B.SAVE); play(ex, 0.5);
  btn(B.OCTDN);
  const sst = [ex.web_test_win(1), ex.web_test_win(2)];
  btn(B.HOME); play(ex, 0.1);
  check(`SAVE once on a starter sound ("${st[0]}", user slot ${st[1]}): OCT- stays on SAVE AS NEW (${sst}), no SAVE over it; HOME shuts it (${ex.web_test_win(1)}), nothing saved (USER 3 "${slotName(2)}")`,
        st[1] === -1 && sst.join() === "1,1" && ex.web_test_win(1) === 0 && slotName(2) === "");
  {                                             // (V0.86: in PRESETS' list, SAVE + OCT+ saves the sound as a new
    const listChord = (b) => { ex.web_buttons(1 << B.SAVE); play(ex, 0.05); chord(B.SAVE, b); ex.web_buttons(0); play(ex, 0.4); };
    const pg0 = ex.web_test_page();             // preset, named first; SAVE + OCT- deletes the one picked, asked)
    ex.web_enc(E.PRESET, 1);
    play(ex, 0.1);
    listChord(B.OCTUP);
    const nm = [ex.web_test_page_section(), sname(), ex.web_test_save_slot()];
    tap(ex, B.SAVE);                            // (SAVE on NAME IT: written)
    play(ex, 0.6);
    const saved = [slotName(nm[2]), ex.web_test_page() === pg0, toast()];
    ex.web_enc(E.PRESET, 1);                    // (V0.87: the list A-Z: to the one just saved)
    play(ex, 0.1);
    const upos = ex.web_test_pl_user(nm[2]);
    ex.web_enc(E.PRESET, upos - ex.web_test_pl(1));
    play(ex, 0.1);
    listChord(B.OCTDN);
    const ask = [ex.web_test_pl(2), slotName(nm[2])];
    listChord(B.OCTDN);
    play(ex, 0.6);
    const gone = [slotName(nm[2]), toast()];
    ex.web_enc(E.PRESET, -99);
    play(ex, 0.1);
    listChord(B.OCTDN);
    const starter = [toast(), ex.web_test_pl(2)];
    play(ex, 3.2);
    check(`PRESETS' list: SAVE + OCT+ names a new preset (section ${nm[0]}, "${nm[1]}", USER ${nm[2] + 1}); SAVE: "${saved[0]}" saved, back to the page (${saved[1]}); SAVE + OCT- on it asks (${ask[0]}, kept "${ask[1]}"), again deletes ("${gone[0]}", "${gone[1]}"); on a starter: "${starter[0]}" (${starter[1]})`,
          nm[0] === SEC.SAVE && nm[1] === "INIT v2" && saved[0] === "INIT v2" && saved[1] === true &&
          ask[0] === upos + 1 && ask[1] === "INIT v2" && gone[0] === "" && gone[1] === `DELETED USER ${nm[2] + 1}` &&
          starter[0] === "A STARTER SOUND STAYS" && starter[1] === 0);
  }
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
  splash(z);
  const SECN = ["OSC A", "OSC B", "SUB", "NOISE", "FILTER", "ENV 1", "ENV 2", "LFO 1", "LFO 2", "VOICE", "HOME",
                "MATRIX", "DRIVE", "CHORUS", "DELAY", "REVERB", "OTT", "EQ", "DRIVE 2", "CHORUS 2", "OTT 2", "EQ 2",
                "FLANGER", "FLANGER 2", "PHASER", "PHASER 2", "FX FILTER", "FX FILTER 2", "BITCRUSH", "BITCRUSH 2",
                "TREMOLO", "TREMOLO 2", "RING MOD", "RING MOD 2", "TAPE", "TAPE 2", "CHAIN", "MENU", "GLOBAL", "SAVE",
                "BROWSE", "SCALE", "ARP", "CHORD", "(free)", "STEPS"];
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
  set(13, 1);                                   // USB OUT (V0.8: the track's): the chords and the bass on its channel, 1
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
        withBass === "48 60 64 67" && ons.length === 4 && offs.length === 4 && ons.includes("90:0:48") && ons.includes("90:0:60") &&
        offs.includes("80:0:48") && offs.includes("80:0:67"));
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
  const kept = [0, 1, 2, 5, 9, 22].map((f) => c2.web_test_chd(f, -1));
  check(`chords: after a power cycle MODE, TYPE, SPREAD, STYLE, BASS, STRUM ${kept.join(" ")}`,
        kept.join() === "2,13,2,0,1,9");
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
  twice(B.ARP);                                 // (V0.86, B11: twice the ARP off; again on)
  const p3 = [u.web_test_page_section(), name(), u.web_test_chd(4, -1)];
  twice(B.ARP);
  p3.push(u.web_test_chd(4, -1));
  u.web_enc(E.SELECT, 1); play(u, 0.1);         // (SELECT: STEPS)
  p3.push(u.web_test_page_section(), name());
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
  u.web_enc(E.SELECT, -1); play(u, 0.1);        // SELECT: the ARP section (STYLE, where it was left); GROOVE
  u.web_enc(E.ALGO, 1); play(u, 0.1);
  const pg = [u.web_test_page_section(), name()];
  u.web_enc(E.K1, 2); play(u, 0.05);
  u.web_enc(E.K2, 5); play(u, 0.05);
  u.web_enc(E.K3, -4); play(u, 0.05);
  u.web_enc(E.K4, 1); play(u, 0.05);
  const s3 = [17, 18, 20, 19].map((f) => u.web_test_chd(f, -1));
  combo(B.ARP, B.OCTDN);
  check(`ARP + OCT+ from HOME: on (${fromEdit[0]}, "${fromEdit[1]}", lit ${fromEdit[3]}; the page stays: section ${fromEdit[2]}); a tap: its menu (section ${opened[0]}, ${opened[1]}), again its first: ${p2}, twice: the ARP off, on (${p3}); STEPS: step ${s1[0] + 1} ${s1.slice(1)}; ${pr}: ${s2}; SELECT back, ALGORITHM: ${pg[1]} (section ${pg[0]}): ${s3}; ARP + OCT-: off (${u.web_test_chd(4, -1)}, ${name()})`,
        fromEdit[0] === 1 && fromEdit[1] === "ARP ON: ARP" && fromEdit[2] === SEC.MACRO && fromEdit[3] === 1 &&
        opened.join() === `${SEC.ARP},STYLE` && p2 === "STYLE" && p3.join() === `${SEC.ARP},STYLE,0,1,${SEC.STEPS},STEPS` &&
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
  tap(g, B.ENV);                                // ENV (V0.86): once its menu, again its first page; twice ENV 1 <-> 2
  const e = [[sec(), name()]];
  g.web_enc(E.ALGO, 1); play(g, 0.05);
  e.push([sec(), name()]);
  tap(g, B.ENV);
  e.push([sec(), name()]);
  tap(g, B.EDIT);                               // (V0.87, U15: twice, the assign window over the page before)
  twice(B.ENV);
  const aw = [g.web_test_win(0), sec(), g.web_test_win(4)];
  tap(g, B.ENV);
  check(`a tap opens ENV (${e[0]}); on SHAPE (${e[1]}) again its first page (${e[2]}); on OSC A twice: the assign window (${aw}); ENV: shut (${g.web_test_win(0)}, ${sec()})`,
        e[0].join() === "5,ADSR" && e[1].join() === "5,SHAPE" && e[2].join() === "5,ADSR" && aw.join() === `1,${SEC.OSCA},2` &&
        g.web_test_win(0) === 0 && sec() === SEC.OSCA);
  const n0 = cstr(g, g.web_test_patch_name());  // HOME + OCT+: the next sound, OCT-: back; twice: the presets
  combo(B.HOME, B.OCTUP);
  const n1 = cstr(g, g.web_test_patch_name());
  combo(B.HOME, B.OCTDN);
  play(g, 0.6);
  const n2 = cstr(g, g.web_test_patch_name());
  twice(B.HOME);
  const br = sec();
  tap(g, B.HOME);
  check(`HOME + OCT+: the next sound ("${n0}" -> "${n1}"), OCT-: back ("${n2}"); HOME twice: the SETTINGS (section ${br}); a tap there: back (${sec()})`,
        n1 !== n0 && n2 === n0 && br === SEC.MENU && sec() !== SEC.MENU);
  tap(g, B.EDIT);                               // EDIT held 5 s: the section shown reset; SAVE + OCT- undoes it
  const POS = ID.A_POS, set = 400;
  g.web_test_set(POS, set);
  play(g, 0.6);
  const [, rst] = octHold(g, B.EDIT, 5.3);      // (V0.87, U2: EDIT + OCT- held 5 s)
  const rs = [rst, g.web_test_value(POS), g.web_test_lay()];
  g.web_buttons(1 << B.SAVE);
  play(g, 0.05);
  g.web_buttons((1 << B.SAVE) | (1 << B.OCTDN)); play(g, 0.03); g.web_buttons(1 << B.SAVE); play(g, 0.03);
  g.web_buttons(0);
  play(g, 0.4);
  check(`EDIT + OCT- held 5 s: "${rs[0]}" (WT POS ${set} -> ${rs[1]}, INIT's; no layer: ${rs[2]}); SAVE + OCT-: back (${g.web_test_value(POS)})`,
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
  check(`SELECT on SETTINGS (one module: V0.86, L8): nothing (${name()})`, name() === "SCREEN");
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
    if (nm === "RESPONSE") more = q;            // (V0.86: MORE named for what it holds)
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

{                                               // V0.8, the looper (sy_loop.c): play, launch, the mix, a MIDI track, a take, overdub, UNDO, QUANTIZE, moves, SONG, chords a track, MIDI IN, the click, NOTES, projects
  const q = await boot(), qt = () => cstr(q, q.web_test_toast());
  const Q = 44117.647 * 60 / 120;               // (a quarter note at 120 BPM, in samples)
  const lp = (f, v = -1) => q.web_test_lp(f, v), tr = (t, f, v = -1) => q.web_test_lp_trk(t, f, v);
  const K = (n) => n - 53;
  const sent = (m0, on, ch = -1) => {           // what went out over USB since m0: [note, velocity, time, channel]
    const r = [];
    for (let i = m0; i < q.web_test_mout_n(); i++) {
      const p = q.web_test_mout(i);
      if (((p >> 8) & 0xF0) === (on ? 0x90 : 0x80) && (ch < 0 || ((p >> 8) & 15) === ch))
        r.push([(p >> 16) & 127, (p >> 24) & 127, q.web_test_mout_t(i), (p >> 8) & 15]);
    }
    return r;
  };
  const press = (b) => { q.web_buttons(1 << b); play(q, 0.03); q.web_buttons(0); play(q, 0.01); };
  const qtap = (b) => { press(b); play(q, 0.34); };
  const until = (qn) => { for (let i = 0; i < 20000 && q.web_test_arp(1) < qn; i++) play(q, 0.0015); };   // the transport at qn thousandths of a quarter
  const key = (n, on) => { q.web_keys(on ? 1 << K(n) : 0); play(q, 0.003); };
  const pad = (k) => {                          // SEQ held (the launch pad), key k pressed and let go, SEQ let go
    q.web_buttons(1 << B.SEQ); play(q, 0.7);
    q.web_keys(1 << k); play(q, 0.03); q.web_keys(0); play(q, 0.03);
    q.web_buttons(0); play(q, 0.1);
  };
  let tref = 0, qref = 0;
  const mark = () => { tref = q.web_test_arp(8); qref = q.web_test_arp(1) / 1000; };   // (the audio side's time and the transport's place now)
  const qof = (smp) => (qref + ((smp - tref) | 0) / Q).toFixed(2);
  const notesOf = (t, s) => { const r = []; for (let i = 0; i < 64; i++) { const n = q.web_test_lp_note(t, s, i, 0); if (n < 0) break; r.push(`${n}@${q.web_test_lp_note(t, s, i, 2)}/${q.web_test_lp_note(t, s, i, 3)}`); } return r.join(" "); };
  play(q, 0.2);
  // 1. a clip from the launch pad: T1 (SYNTH + MIDI out, channel 1), scene A: C4 E4 G4 C5 a beat apart, an eighth each
  tr(0, 3, 1);
  for (const [t, n] of [[0, 60], [96, 64], [192, 67], [288, 72]]) q.web_test_lp_put(0, 0, t, 48, n, 100);
  let m0 = q.web_test_mout_n();
  pad(0);                                       // (white key 1: T1's clip in scene A; stopped: the transport starts)
  const launched = [lp(15), tr(0, 10), qt()];
  let vt = -2;
  for (let i = 0; i < 40 && vt < 0; i++) { play(q, 0.01); for (let k = 0; k < 8; k++) if (q.web_test_voice_trk(k) >= 0) vt = q.web_test_voice_trk(k); }
  play(q, 2.2);
  const ons = sent(m0, 1, 0), offs = sent(m0, 0, 0), t0 = ons.length ? ons[0][2] : 0;
  const at = ons.map((x) => ((x[2] - t0) / Q).toFixed(2)), len = ons.slice(0, 4).map((x, i) => offs[i] ? ((offs[i][2] - x[2]) / Q).toFixed(2) : "-");
  check(`LOOPER: the pad's key 1 launches T1's clip, the transport starting (${launched.join(", ")}); its notes ${ons.map((x) => x[0]).join(" ")} at ${at.join(" ")} quarters, each ${len.join(" ")} long (an eighth), round again at 4; its voices T1's (${vt})`,
        launched[0] === 1 && launched[1] === 0 && launched[2] === "T1 PLAYS A" && ons.length >= 5 &&
        ons.slice(0, 5).map((x) => x[0]).join(" ") === "60 64 67 72 60" && at.slice(0, 5).join(" ") === "0.00 1.00 2.00 3.00 4.00" &&
        len.every((x) => Math.abs(+x - 0.5) < 0.02) && vt === 0);
  // 2. the mix: VOLUME, PAN, MUTE, SOLO as the track's gains
  tr(0, 4, 50);
  play(q, 0.01);
  const g1 = [tr(0, 14), tr(0, 15)];
  tr(0, 5, 1);
  play(q, 0.01);
  const g2 = [tr(0, 14), tr(0, 15)];
  tr(0, 5, 64); tr(0, 4, 100); tr(0, 6, 1);
  play(q, 0.01);
  const g3 = [tr(0, 14), tr(0, 15)];
  tr(0, 6, 0); tr(1, 7, 1);
  play(q, 0.01);
  const g4 = [tr(0, 14), tr(1, 14)];
  tr(1, 7, 0);
  check(`.. the mix: VOLUME 50: ${g1.join("/")}; PAN left: ${g2.join("/")}; MUTE: ${g3.join("/")}; T2 SOLO: T1 ${g4[0]}, T2 ${g4[1]}`,
        g1.join() === "250,250" && g2.join() === "250,0" && g3.join() === "0,0" && g4[0] === 0 && g4[1] === 1000);
  // 3. a MIDI track (T2, channel 5): launched mid-bar it starts at the next bar; its notes out on its channel, no voice
  tr(1, 0, 1); tr(1, 1, 4);
  q.web_test_lp_put(1, 0, 0, 96, 48, 90);
  q.web_test_lp_put(1, 0, 192, 96, 50, 90);
  until(5500);
  m0 = q.web_test_mout_n();
  q.web_test_lp_launch(1, 0);
  const wait = tr(1, 11);
  until(10100);
  const t2 = sent(m0, 1, 4), barAt = t2.length ? ((t2[0][2] - t0) / Q).toFixed(2) : "-";
  let t2v = 0;
  for (let i = 0; i < 8; i++) t2v += q.web_test_voice_trk(i) === 1;
  check(`.. T2 MIDI on channel 5: launched at 5.5 (waiting: ${wait}), its first note at ${barAt} quarters (the bar), ${t2.map((x) => x[0]).join(" ")} sent; voices of it ${t2v}`,
        wait === 0 && barAt === "8.00" && t2.map((x) => x[0]).slice(0, 2).join(" ") === "48 50" && t2v === 0);
  // 4. STOP ALL: at the next bar, every clip stops
  q.web_test_lp_req(3);
  play(q, 0.01);
  const stopping = [tr(0, 10), tr(0, 11)];
  until(12100);
  check(`.. STOP ALL: until the bar ${stopping.join("/")}; after it T1 ${tr(0, 10)}, T2 ${tr(1, 10)}`,
        stopping[0] === 0 && stopping[1] === 254 && tr(0, 10) === 255 && tr(1, 10) === 255);
  press(B.PLAY);                                // (stop)
  play(q, 0.2);
  // 5. a first take: T3 (SYNTH + MIDI out, channel 3), scene B: REC stopped: a bar of count-in (clicks), then C4 at 0.5 for an eighth,
  //    E4 at 2.0 for a quarter; REC at 4.1: one bar, it loops (the next pass plays them)
  lp(0, 2); lp(1, 1); tr(2, 3, 1); tr(2, 1, 2);
  press(B.REC);
  play(q, 0.33);                                // (V0.87: a single tap acts after the double tap's time)
  const cnt = [lp(10), lp(14) > 0, lp(15)];
  for (let i = 0; i < 2000 && lp(10) !== 3; i++) { play(q, 0.003); cnt[1] = cnt[1] || lp(14) > 0; }
  const take = [lp(10), lp(15), (q.web_test_arp(1) / 1000).toFixed(2)];
  until(500); key(60, 1); until(1000); key(60, 0);
  until(2000); key(64, 1); until(3000); key(64, 0);
  until(4100);
  mark();
  m0 = q.web_test_mout_n();
  qtap(B.REC);
  const ended = [lp(10), q.web_test_lp_clip(2, 1, 0, -1), notesOf(2, 1), tr(2, 10), qt()];
  until(7000);
  const pass2 = sent(m0, 1, 2).map((x) => `${x[0]}@${qof(x[2])}`);
  check(`.. a first take (T3, scene B): REC stopped, the count-in (${cnt.join(", ")}), then the take (${take.join(", ")}); REC at 4.1: ${ended[0]}, length ${ended[1]} (a bar), ${ended[2]}, T3 plays ${ended[3]} ("${ended[4]}"); the next pass: ${pass2.join(" ")}`,
        cnt[0] === 1 && cnt[1] && cnt[2] === 0 && take[0] === 3 && take[1] === 1 && ended[0] === 0 && ended[1] === 384 &&
        ended[2] === "60@48/48 64@192/96" && ended[3] === 1 && ended[4] === "T3 B: 1 BAR, LOOPING" &&
        pass2.join(" ") === "60@4.50 64@6.00");
  // 6. overdub: REC on the clip playing: D4 at 9.0 (its tick 96), played once as it was played, again the next pass; UNDO PASS takes it back, again puts it back
  until(8200);
  qtap(B.REC);
  const dub = lp(10);
  until(9000);
  m0 = q.web_test_mout_n();
  key(62, 1); until(9250); key(62, 0);
  until(11900);
  const live = sent(m0, 1, 2).filter((x) => x[0] === 62).length;
  qtap(B.REC);
  const dubbed = [lp(10), notesOf(2, 1)];
  mark();
  m0 = q.web_test_mout_n();
  until(14000);
  const again = sent(m0, 1, 2).filter((x) => x[0] === 62).map((x) => qof(x[2]));
  q.web_test_lp_req(2);
  play(q, 0.01);
  const undone = notesOf(2, 1);
  q.web_test_lp_req(2);
  play(q, 0.01);
  const redone = notesOf(2, 1);
  check(`.. overdub: REC on it (${dub}), D4 at 9.0: sent ${live} time(s) that pass; REC: ${dubbed[0]}, ${dubbed[1]}; the next pass D4 at ${again.join(" ")}; UNDO PASS: ${undone}; again: ${redone}`,
        dub === 4 && live === 1 && dubbed[0] === 0 && dubbed[1] === "60@48/48 62@96/24 64@192/96" && again.join(" ") === "13.00" &&
        undone === "60@48/48 64@192/96" && redone === dubbed[1]);
  // 7. QUANTIZE 1/16: a note at tick 29 lands on 24; knob moves while recording: written, heard while turning; played back the next pass; stopped, let go
  lp(2, 2);
  until(15200);
  qtap(B.REC);
  until(16300); key(65, 1); until(16500); key(65, 0);
  const CUT = pid(q, 4, "TONE", 1), cut0 = q.web_test_value(CUT);
  for (let g = 0; g < 200 && !(q.web_test_page_section() === 4 && cstr(q, q.web_test_page_name()) === "TONE"); g++) q.web_test_go(g);
  until(17000);
  let heard = 1;
  for (let i = 0; i < 12; i++) { q.web_enc(E.K2, -3); play(q, 0.04); if (q.web_test_ovr(CUT) !== -32768) heard = 0; }
  until(19500);
  qtap(B.REC);
  lp(2, 0);
  const qn = notesOf(2, 1), moved = q.web_test_lp_clip(2, 1, 3, -1), mv0 = q.web_test_lp_move(2, 1, CUT, 0, 1);
  q.web_test_set(CUT, cut0);
  until(21300);
  const ovr = q.web_test_ovr(CUT), fol = q.web_test_follow(CUT);
  press(B.PLAY);
  play(q, 0.05);
  check(`.. QUANTIZE 1/16: F4 at 16.3 into ${qn}; the cutoff turned while recording: ${moved} parameter(s) moved (first ${mv0}), heard while it turned (${heard}); the next pass holds it (${ovr}, followed ${fol}; the knob's ${cut0}); stopped: ${q.web_test_ovr(CUT)}`,
        qn.includes("65@24/") && moved === 1 && heard === 1 && ovr !== -32768 && ovr < cut0 && fol === ovr && q.web_test_ovr(CUT) === -32768);
  // 8. SONG: A once, C twice (T1's C two bars long), round again; PLAY from the top: T1 A, then C (bars 1 .. 4), A at 5
  lp(1, 0);
  q.web_test_lp_clip(0, 2, 0, 768);
  q.web_test_lp_put(0, 2, 0, 48, 55, 100);
  q.web_test_lp_song(0, 0, 0); q.web_test_lp_song(0, 1, 1);
  q.web_test_lp_song(1, 0, 2); q.web_test_lp_song(1, 1, 2);
  q.web_test_lp_song(2, 0, 255);
  lp(8, 1);
  press(B.PLAY);
  const song = [];
  for (const b of [1000, 5000, 9000, 13000, 17000, 21000]) { until(b); song.push(tr(0, 10)); }
  press(B.PLAY);
  lp(8, 0);
  play(q, 0.1);
  check(`.. SONG: A once, C twice: T1 at bars 1 .. 6 ${song.join(" ")}`, song.join(" ") === "0 2 2 2 2 0");
  // 9. chords a track: T1 KEY chords; T2's its own (off); MIDI IN: T4 IN channel 6
  lp(0, 0);
  q.web_test_chd(0, 1);
  lp(0, 4);
  const t5mode = q.web_test_chd(0, -1);
  m0 = q.web_test_mout_n();
  tr(4, 3, 1); tr(4, 1, 7);
  key(60, 1); play(q, 0.05);
  const one = sent(m0, 1, 7).length;
  key(60, 0); play(q, 0.05);
  lp(0, 0);
  tr(0, 3, 1); tr(0, 1, 0);
  m0 = q.web_test_mout_n();
  key(60, 1); play(q, 0.05);
  const three = sent(m0, 1, 0).length;
  key(60, 0); play(q, 0.05);
  q.web_test_chd(0, 0);
  tr(3, 2, 6); tr(3, 3, 1); tr(3, 1, 9);
  m0 = q.web_test_mout_n();
  q.web_midi(0x95, 70, 100); play(q, 0.05);
  const viaIn = sent(m0, 1, 9).map((x) => x[0]).join(" ");
  q.web_midi(0x85, 70, 0); play(q, 0.05);
  m0 = q.web_test_mout_n();
  q.web_midi(0x90, 72, 100); play(q, 0.05);
  const viaSel = sent(m0, 1, 0).map((x) => x[0]).join(" ");   // (T1's chords off again)
  q.web_midi(0x80, 72, 0); play(q, 0.05);
  check(`.. a track's own chords: T5's mode ${t5mode} while T1's is KEY; C4 on T5 sends ${one} note, on T1 ${three}; MIDI on channel 6 plays T4 (IN 6: ${viaIn}); on channel 1 the selected track, T1 (${viaSel})`,
        t5mode === 0 && one === 1 && three === 3 && viaIn === "70" && viaSel === "72");
  // 10. the click: ON, running: a click at each beat; OFF: none
  lp(4, 2);
  press(B.PLAY);
  until(1020);
  const ck1 = lp(14);
  lp(4, 0);
  until(2020);
  const ck0 = lp(14);
  press(B.PLAY);
  play(q, 0.1);
  check(`.. CLICK ON: sounding just after a beat (${ck1} samples left); OFF: ${ck0}`, ck1 > 0 && ck0 === 0);
  // 11. NOTES: T3's clip in B, its first note C4@48: PITCH +2, START one 1/16 later, EDIT's DELETE
  lp(0, 2); lp(1, 1);
  const n0 = [q.web_test_lp_note(2, 1, 0, 0), q.web_test_lp_note(2, 1, 0, 2), q.web_test_lp_note(2, 1, 0, 3)];
  const npg = goPage(q, SEC.LCLIP, "NOTES");
  play(q, 0.05);
  q.web_enc(E.K2, 2); play(q, 0.05);
  q.web_enc(E.K3, 1); play(q, 0.05);
  const edited = notesOf(2, 1);
  q.web_enc(E.K1, 10); play(q, 0.05);           // (the last note picked on NOTES; NOTE+'s DELETE: V0.86, C2)
  goPage(q, SEC.LCLIP, "NOTE+");
  play(q, 0.05);
  q.web_enc(E.K3, 1); play(q, 0.05);
  const deleted = [notesOf(2, 1), qt()];
  check(`.. NOTES (page ${npg}): PITCH +2, START +1/16: ${edited}; EDIT, the last note DELETEd: ${deleted[0]} ("${deleted[1]}")`,
        npg >= 0 && edited.includes(`${n0[0] + 2}@${n0[1] + 24}/${n0[2]}`) && deleted[1] === "NOTE DELETED" && !deleted[0].includes("64@192"));
  // 12. V0.87: SELECT on HOME the next track; SEQ + OCT+ a new clip of a bar (PRESETS, SEQ held: its length); SEQ + OCT-
  //     DELETE? asked, again: deleted; SEQ + OCT- held 5 s on CLIP: its settings reset
  goPage(q, SEC.MACRO, "SOUND"); play(q, 0.05);
  q.web_enc(E.SELECT, 1); play(q, 0.1);
  const nextT = [lp(0), qt()];
  lp(0, 6); lp(1, 3);
  q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons((1 << B.SEQ) | (1 << B.OCTUP)); play(q, 0.03);
  q.web_buttons(1 << B.SEQ); play(q, 0.03);
  const made = [q.web_test_lp_clip(6, 3, 0, -1), qt()];
  q.web_enc(E.PRESET, 1); play(q, 0.05);
  q.web_buttons(0); play(q, 0.35);
  const longer = [q.web_test_lp_clip(6, 3, 0, -1), qt()];
  const sdel = () => { q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons((1 << B.SEQ) | (1 << B.OCTDN)); play(q, 0.03);
                       q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons(0); play(q, 0.35); };
  sdel();
  const asked = [q.web_test_win(3), q.web_test_lp_clip(6, 3, 0, -1)];
  sdel();
  const gone = [q.web_test_win(3), q.web_test_lp_clip(6, 3, 0, -1), qt()];
  goPage(q, SEC.LCLIP, "CLIP");
  const [, rst] = octHold(q, B.SEQ, 5.3);
  check(`.. SELECT on HOME: track ${nextT[0] + 1} ("${nextT[1]}"); SEQ + OCT+: a new clip (${made[0]}, "${made[1]}"), PRESETS 2 bars (${longer[0]}, "${longer[1]}"); SEQ + OCT-: asked (${asked}), again: deleted (${gone[0]}, ${gone[1]}, "${gone[2]}"); SEQ + OCT- held 5 s on CLIP: "${rst}"`,
        nextT[0] === 3 && nextT[1] === "T4 SYNTH+MIDI" && made[0] === 384 && made[1] === "T7 D: NEW, 1 BAR" && longer[0] === 768 &&
        longer[1] === "T7 D: 2 BARS" && asked.join() === "2,768" && gone[0] === 0 && gone[1] === 0 && gone[2] === "T7 D DELETED" &&
        rst === "CLIP: RESET");
  // 13. the project: through a power cycle (the clips, the tracks, the chords, SONG, the settings); PROJECT 2 empty, 1 back
  lp(0, 0);
  q.web_test_chd(0, 2);
  lp(2, 3);
  play(q, 3.8);                                 // (3 s of quiet: the settings and the project written)
  const nor = new Uint8Array(q.memory.buffer, q.web_nor(), q.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const q2 = instance.exports;
  if (q2._initialize) q2._initialize();
  new Uint8Array(q2.memory.buffer, q2.web_nor(), q2.web_nor_size()).set(nor);
  q2.web_master(700);
  q2.web_boot();
  play(q2, 0.3);
  const n2s = notesOf(2, 1).replace(/\/\d+/g, "");
  const n2 = (t, s) => { const r = []; for (let i = 0; i < 64; i++) { const n = q2.web_test_lp_note(t, s, i, 0); if (n < 0) break; r.push(`${n}@${q2.web_test_lp_note(t, s, i, 2)}`); } return r.join(" "); };
  const back = [n2(0, 0), n2(2, 1), q2.web_test_lp_trk(1, 0, -1), q2.web_test_lp_trk(1, 1, -1), q2.web_test_lp_trk(3, 2, -1), q2.web_test_chd(0, -1),
                q2.web_test_lp_song(1, 0, -1), q2.web_test_lp(2, -1), q2.web_test_lp_clip(0, 2, 0, -1)];
  twiceB(q2, B.SEQ);                            // (V0.87, S2: SEQ twice, the projects' list; V0.87.1: SEQ twice loads)
  const pj = [q2.web_test_pl(0), q2.web_test_pl(3)];
  q2.web_enc(E.PRESET, 1); play(q2, 0.1);
  twiceB(q2, B.SEQ); play(q2, 0.5);
  const p2 = [q2.web_test_lp(7, -1), n2(0, 0), q2.web_test_lp_clip(0, 0, 0, -1)];
  twiceB(q2, B.SEQ);
  q2.web_enc(E.PRESET, -1); play(q2, 0.1);
  twiceB(q2, B.SEQ); play(q2, 0.5);
  const p1 = [q2.web_test_lp(7, -1), n2(0, 0)];
  check(`.. after a power cycle: T1 A ${back[0]}; T3 B ${back[1]}; T2 MIDI ${back[2]} channel ${back[3] + 1}; T4 IN ${back[4]}; T1's chords ${back[5]}; SONG's slot 2 ${back[6]}; QUANTIZE ${back[7]}; T1 C ${back[8]} ticks; PROJECTS' list (${pj}) 2: ${p2.join(", ")}; back to 1: ${p1.join(", ")}`,
        back[0] === "60@0 64@96 67@192 72@288" && back[1] === n2s && back[2] === 1 && back[3] === 4 && back[4] === 6 &&
        back[5] === 2 && back[6] === 2 && back[7] === 3 && back[8] === 768 && pj.join() === "1,3" && p2[0] === 1 && p2[1] === "" && p2[2] === 0 &&
        p1[0] === 0 && p1[1] === back[0]);
}

{                                               // V0.85, the drums (sy_drum.c, sy_loop.c, sy_ui_drum.c): a drum track's pages and keys, the grid by its keys and LEDs, a beat, RATCHET and CHANCE from HIT's knobs, the duck, a take, TYPE, the kit's knobs, a power cycle
  const d = await boot(), dt = () => cstr(d, d.web_test_toast());
  const lp = (f, v = -1) => d.web_test_lp(f, v), tr = (t, f, v = -1) => d.web_test_lp_trk(t, f, v);
  const kit = (l, f, v = -1) => d.web_test_kit(l, f, v), grid = (f, r, s, v = 0) => d.web_test_grid(f, r, s, v);
  const hits = (r) => kit(r, 24);
  const press = (b) => { d.web_buttons(1 << b); play(d, 0.03); d.web_buttons(0); play(d, 0.01); };
  const keyTap = (k) => { d.web_keys(1 << k); play(d, 0.02); d.web_keys(0); play(d, 0.03); };
  const W = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26], BK = [1, 3, 5, 8, 10, 13, 15, 17, 20];   // (white keys 1-16, black 1-9)
  play(d, 0.6);                                 // (a tap in the first 400 ms after boot would read as a second)
  // 1. T1 DRUMS: EDIT shows its KIT; white key 1 a KICK, white 9 the same accented (ACCENT 50 %: 1.5 x), black 1 DUCK
  tr(0, 0, 2);
  tap(d, B.SEQ);
  tap(d, B.EDIT);
  const kitPg = [d.web_test_page_section(), cstr(d, d.web_test_page_name())];
  const h0 = [hits(0), hits(8)];
  kit(0, 2, 0);                                 // (TONE 0: no click, the same shape at each gain)
  d.web_keys(1 << W[0]); const p1 = play(d, 0.25).peak; d.web_keys(0); play(d, 0.6);
  d.web_keys(1 << W[8]); const p2 = play(d, 0.25).peak; d.web_keys(0); play(d, 0.6);
  keyTap(BK[0]);
  const duck = [kit(0, 23), kit(0, 21)];
  check(`DRUMS: T1 DRUMS, EDIT shows its DRUM (V0.86: section ${kitPg[0]}, ${kitPg[1]}); white key 1 a KICK (peak ${p1.toFixed(3)}), white 9 accented (${(p2 / p1).toFixed(2)} x), black 1 DUCK (state ${duck[0]}, the synth's gain ${duck[1]}): KICKs ${hits(0) - h0[0]}, DUCKs ${hits(8) - h0[1]}`,
        tr(0, 0) === 2 && kitPg[0] === 52 && kitPg[1] === "MODEL" && p1 > 0.05 && Math.abs(p2 / p1 - 1.5) < 0.1 && duck[0] > 0 &&
        hits(0) - h0[0] === 2 && hits(8) - h0[1] === 1);
  // 2. the grid (SEQ's CLIP on a drum track): black keys the row, white keys the steps; an empty clip made a bar and
  //    launched; the LEDs show the row's steps and the row
  tap(d, B.SEQ);
  const gp = goPage(d, 51, "GRID");
  const put = (row, steps) => { keyTap(BK[row]); for (const s of steps) keyTap(W[s]); };
  put(0, [0, 4, 8, 12]);                        // (KICK four to the floor)
  put(1, [4, 12]);                              // (SNARE on 2 and 4)
  put(3, [0, 2, 4, 6, 8, 10, 12, 14]);          // (HAT CL on the eighths)
  put(8, [0, 4, 8, 12]);                        // (DUCK with the kick)
  play(d, 0.05);
  const lit = d.web_lit_keys(), want = [0, 4, 8, 12].reduce((m, s) => m | (1 << W[s]), 0) | (1 << BK[8]);
  const g2 = [grid(5, 0, 0), grid(5, 0, 1), grid(5, 1, 4), grid(5, 3, 14), d.web_test_lp_clip(0, 0, 0, -1), d.web_test_lp_clip(0, 0, 1, -1), tr(0, 10)];
  keyTap(W[1]); keyTap(W[1]);                   // (in and out again: step 2 of DUCK)
  check(`.. GRID (page ${gp}): the keys put a beat in (KICK 1: ${g2[0]}, 2: ${g2[1]}; SNARE 5: ${g2[2]}; HAT 15: ${g2[3]}), the clip ${g2[4]} ticks, ${g2[5]} hits, launched (${g2[6]}); LEDs: the DUCK row's steps and its key lit (${(lit & want) === want ? "yes" : lit.toString(2)}); a step in and out: ${grid(5, 8, 1)}`,
        gp >= 0 && g2[0] === 1 && g2[1] === 0 && g2[2] === 1 && g2[3] === 1 && g2[4] === 384 && g2[5] === 18 && g2[6] === 0 &&
        (lit & want) === want && grid(5, 8, 1) === 0);
  // 3. PLAY: two bars at 120 BPM (4 s): KICK 8, SNARE 4, HAT CL 16, DUCK 8; the synth's gain ducked to 1 - AMOUNT
  const c0 = [0, 1, 3, 8].map(hits);
  press(B.PLAY);
  let gmin = 1000;
  for (let i = 0; i < 4000 && d.web_test_arp(1) < 7950; i++) { play(d, 0.003); gmin = Math.min(gmin, kit(0, 21)); }
  const c1 = [0, 1, 3, 8].map((r, i) => hits(r) - c0[i]);
  check(`.. PLAY, two bars: KICK ${c1[0]}, SNARE ${c1[1]}, HAT CL ${c1[2]}, DUCK ${c1[3]}; the synth's gain down to ${gmin} (AMOUNT 60 %)`,
        c1.join() === "8,4,16,8" && Math.abs(gmin - 400) <= 2);
  // 4. HIT's knobs: STEP 5, ROW SNARE, RATCHET 4 hits; STEP 9, ROW KICK, CHANCE 50 %
  goPage(d, 51, "HIT");
  const turn = (k, n) => { d.web_enc(k, n); play(d, 0.3); };   // (a turn of its own: the UI sums a millisecond's)
  turn(E.K1, -20); turn(E.K1, 4);               // (step 5)
  turn(E.K2, -9); turn(E.K2, 1);                // (SNARE)
  turn(E.K4, 3);                                // (RATCHET 4 hits)
  turn(E.K1, 4); turn(E.K2, -1);                // (step 9, KICK)
  turn(E.K3, 4);                                // (CHANCE 4: 50 %)
  const attrs = [grid(4, 1, 4, 0), grid(3, 0, 8, 0)];
  for (let i = 0; i < 4000 && d.web_test_arp(1) % 4000 < 3850; i++) play(d, 0.003);   // (just before a bar)
  const c2 = [0, 1].map(hits);
  play(d, 16.0);                                // (eight bars)
  const c3 = [hits(1) - c2[1], hits(0) - c2[0]];   // (SNARE, KICK)
  check(`.. HIT: SNARE step 5 RATCHET ${attrs[0] + 1} hits, KICK step 9 CHANCE ${100 - attrs[1] * 100 / 8} %; eight bars: SNARE ${c3[0]} (5 a bar), KICK ${c3[1]} (3 a bar and some of 8)`,
        attrs[0] === 3 && attrs[1] === 4 && c3[0] === 40 && c3[1] > 24 && c3[1] < 32);
  // 5. a take on a drum track (scene B, T1; the transport running, no count-in): KICK, SNARE, an accented HAT CL go in
  //    as hits (GM's notes, no length: the hit's own); REC ends it, a bar
  goPage(d, 46, "MIXER");                       // (the keys play here: the grid's and CLIPS' pages take them)
  lp(1, 1); lp(3, 0);
  press(B.REC);
  for (let i = 0; i < 3000 && lp(10) !== 3; i++) play(d, 0.003);
  const takeSt = lp(10);
  keyTap(W[0]); play(d, 0.4); keyTap(W[1]); play(d, 0.4); keyTap(W[11]); play(d, 0.4);
  for (let i = 0; i < 3000 && d.web_test_arp(1) % 4000 < 3200; i++) play(d, 0.003);
  press(B.REC);
  play(d, 0.1);
  const tk = [0, 1, 2].map((i) => `${d.web_test_lp_note(0, 1, i, 0)}/${d.web_test_lp_note(0, 1, i, 3)}`);
  check(`.. a take on T1 B: recording (${takeSt}); its hits ${tk.join(" ")} (KICK, SNARE, HAT CL accented: 16384 + 32), a bar (${d.web_test_lp_clip(0, 1, 0, -1)})`,
        takeSt === 3 && tk.join(" ") === "36/16384 38/16384 42/16416" && d.web_test_lp_clip(0, 1, 0, -1) === 384);
  // 6. TYPE: SYNTH keeps the hits as they are (each played as a sixteenth), DRUMS again: nothing lost; the pages
  //    follow (GRID <-> NOTES)
  press(B.PLAY);
  play(d, 0.2);
  goPage(d, 51, "GRID");
  tr(0, 0, 0);
  const asSynth = [d.web_test_lp_note(0, 1, 0, 3), d.web_test_page_section(), cstr(d, d.web_test_page_name())];
  tr(0, 0, 2);
  const asDrums = [d.web_test_lp_note(0, 1, 0, 3), d.web_test_page_section(), cstr(d, d.web_test_page_name())];
  check(`.. TYPE SYNTH: a hit kept (${asSynth[0]}), the page ${asSynth[1]} ${asSynth[2]}; DRUMS again: ${asDrums[0]}, ${asDrums[1]} ${asDrums[2]}; the accented HAT ${d.web_test_lp_note(0, 1, 2, 3)}`,
        asSynth.join() === "16384,48,NOTES" && asDrums.join() === "16384,51,GRID" && d.web_test_lp_note(0, 1, 2, 3) === 16416);
  // 7. the kit's knobs (V0.86: DRUM's SOUND, TUNE on KNOB 1, the drum picked: the last played, stopped; MIX's LEVEL;
  //    KIT's DUCK, AMOUNT on KNOB 2)
  tap(d, B.EDIT);
  goPage(d, 52, "SOUND");
  keyTap(W[1]);                                 // (a SNARE played, stopped: the drum picked)
  play(d, 0.3);
  turn(E.K1, 5);                                // (TUNE +5)
  goPage(d, 52, "MIX");
  turn(E.K1, -20);                              // (LEVEL 100 -> 80: a turn after a pause, a step a detent)
  goPage(d, 50, "DUCK");
  turn(E.K2, -10);                              // (AMOUNT 60 -> 50)
  const kv = [kit(1, 0) - 24, kit(1, 4), kit(0, 12)];
  check(`.. the kit's knobs: the SNARE played is the drum picked; TUNE ${kv[0]}, LEVEL ${kv[1]}, the duck's AMOUNT ${kv[2]}`,
        kv[0] === 5 && kv[1] === 80 && kv[2] === 50);
  // 8. a power cycle: T1 DRUMS, its grid, its take, the kit come back
  play(d, 4.0);                                 // (stopped and quiet: the project written)
  const nor = new Uint8Array(d.memory.buffer, d.web_nor(), d.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const d2 = instance.exports;
  if (d2._initialize) d2._initialize();
  new Uint8Array(d2.memory.buffer, d2.web_nor(), d2.web_nor_size()).set(nor);
  d2.web_master(700);
  d2.web_boot();
  play(d2, 0.3);
  d2.web_test_lp(1, 0);
  const back = [d2.web_test_lp_trk(0, 0, -1), d2.web_test_grid(5, 0, 0, 0), d2.web_test_grid(5, 3, 14, 0), d2.web_test_grid(4, 1, 4, 0),
                d2.web_test_lp_note(0, 1, 2, 3), d2.web_test_kit(1, 0, -1) - 24, d2.web_test_kit(1, 4, -1), d2.web_test_kit(0, 12, -1)];
  check(`.. after a power cycle: T1 TYPE ${back[0]}; KICK 1 ${back[1]}, HAT 15 ${back[2]}, SNARE 5's RATCHET ${back[3] + 1}; the take's HAT ${back[4]}; the SNARE's TUNE ${back[5]}, LEVEL ${back[6]}; AMOUNT ${back[7]}`,
        back.join() === "2,1,1,3,16416,5,80,50");

  // 9. V0.86 step 2, the drum editor: DRUM (the drum PRESETS picks) and KIT; each page's knobs on the kit; a factory
  //    kit through HOME twice; the edits' dot; a power cycle
  {
    const e = d2, et = (k, n) => { e.web_enc(k, n); play(e, 0.3); }, en = () => cstr(e, e.web_test_page_name());
    const ek = (l, f, v = -1) => e.web_test_kit(l, f, v), es = () => e.web_test_page_section();
    const etap = (b) => { e.web_buttons(1 << b); play(e, 0.03); e.web_buttons(0); play(e, 0.35); };
    const etwice = (b) => { for (let i = 0; i < 2; i++) { e.web_buttons(1 << b); play(e, 0.03); e.web_buttons(0); play(e, 0.08); } play(e, 0.3); };
    play(e, 2.0);                               // (past the splash)
    e.web_test_lp(0, 0);
    play(e, 0.1);
    etap(B.EDIT);
    const p0 = [es(), en()];
    e.web_enc(E.PRESET, 2); play(e, 0.1);       // (PRESETS: the drum, KICK -> CLAP)
    const pk = ek(0, 49);
    e.web_enc(E.SELECT, 1); play(e, 0.1);       // (SELECT: KIT)
    const p1 = [es(), en()];
    check(`DRUM EDITOR: EDIT on a drum track: ${p0} (DRUM's MODEL, V0.87); PRESETS picks the drum (${pk}: CLAP); SELECT: ${p1} (KIT)`,
          p0.join() === "52,MODEL" && pk === 2 && p1[0] === 50);
    const ed0 = ek(0, 48);
    goPage(e, 52, "DRIVE"); play(e, 0.05);
    et(E.K1, 1); et(E.K2, 30); et(E.K3, -20);   // (CLAP: TYPE HARD, DRIVE up, MIX down)
    const dr = [ek(2, 30), ek(2, 31), ek(2, 32)];
    goPage(e, 52, "EQ"); play(e, 0.05);
    ek(2, 33, 47);
    et(E.K1, 1); et(E.K1, 1); et(E.K1, 1);      // (LOW: 47 -> 48 -> 49 -> 50, the middle: it stays there)
    et(E.K1, 1); et(E.K1, 1);
    const lo = ek(2, 33);
    goPage(e, 52, "SHAPE"); play(e, 0.05);
    et(E.K1, 10); et(E.K2, -10);
    const sh = [ek(2, 37), ek(2, 38)];
    check(`DRUM's pages on the drum picked: DRIVE TYPE, DRIVE, MIX ${dr}; EQ LOW from 47 holding at the middle: ${lo}; SHAPE ${sh}; the kit's dot after edits (${ed0} -> ${ek(0, 48)})`,
          dr[0] === 1 && dr[1] > 20 && dr[2] < 100 && lo === 50 && sh[0] > 50 && sh[1] < 50 && ed0 === 0 && ek(0, 48) === 1);
    goPage(e, 50, "DUCK"); play(e, 0.05);
    et(E.K1, 1);
    goPage(e, 50, "CURVE"); play(e, 0.05);
    et(E.K2, 20);
    goPage(e, 50, "COMP"); play(e, 0.05);
    et(E.K1, 40); et(E.K4, -30);
    goPage(e, 50, "ACCENT"); play(e, 0.05);
    et(E.K3, 25);
    const kt = [ek(0, 40), ek(0, 41), ek(0, 43), ek(0, 46), ek(0, 42)];
    check(`KIT's pages: the duck's SOURCE KICK (${kt[0]}), HOLD ${kt[1]}; COMP AMOUNT ${kt[2]}, MIX ${kt[3]}; ACCENT's TONE ${kt[4]}`,
          kt[0] === 1 && kt[1] > 0 && kt[2] > 20 && kt[3] < 100 && kt[4] > 0);
    etap(B.HOME);                               // (V0.87, U6: EDIT twice on a drum track: the kits)
    etwice(B.EDIT);
    const kl = [e.web_test_pl(0), e.web_test_pl(3), e.web_test_pl(1)];
    e.web_enc(E.PRESET, 2); play(e, 0.1);       // (808 -> LO-FI from where it opens; EDIT twice: loaded)
    const want = e.web_test_pl(1);
    twiceB(e, B.EDIT);
    const ld = [ek(0, 47), cstr(e, e.web_test_kit_name()), ek(0, 48), e.web_test_pl(0), ek(0, 30), ek(0, 31)];
    check(`EDIT twice on a drum track: the kits (open ${kl[0]}, kits ${kl[1]}, at ${kl[2]}); PRESETS on, EDIT twice: kit ${ld[0]} "${ld[1]}" (picked ${want}), the dot gone (${ld[2]}), the list gone (${ld[3]}); its KICK's DRIVE ${ld[4]} ${ld[5]}`,
          kl.join() === "1,1,0" && ld[0] === want && ld[1] === "LO-FI" && ld[2] === 0 && ld[3] === 0 && ld[4] === 3 && ld[5] === 50);
    etap(B.EDIT);                               // (EDIT held 5 s on DRUM: the drum picked as a kit starts)
    goPage(e, 52, "SOUND"); play(e, 0.05);
    const [, drs] = octHold(e, B.EDIT, 5.3);     // (V0.87, U2: EDIT + OCT- held 5 s)
    const rs = [drs, ek(ek(0, 49), 30), ek(ek(0, 49), 31)];
    check(`EDIT + OCT- held 5 s on DRUM: "${rs[0]}", the drum picked's DRIVE back to rest (${rs[1]}, ${rs[2]})`,
          rs[0] === "DRUM: RESET" && rs[2] === 0);
    play(e, 7.0);                               // (stopped, quiet: the project written, an object at a time) a power cycle
    const nor2 = new Uint8Array(e.memory.buffer, e.web_nor(), e.web_nor_size()).slice();
    const { instance } = await WebAssembly.instantiate(bytes, {});
    const e2 = instance.exports;
    if (e2._initialize) e2._initialize();
    new Uint8Array(e2.memory.buffer, e2.web_nor(), e2.web_nor_size()).set(nor2);
    e2.web_master(700);
    e2.web_boot();
    play(e2, 0.3);
    const b2 = [e2.web_test_kit(0, 47, -1), cstr(e2, e2.web_test_kit_name()), e2.web_test_kit(1, 30, -1), e2.web_test_kit(1, 31, -1),
                e2.web_test_kit(0, 43, -1)];                    // (-1: read only; a missing argument is a 0 written)
    check(`.. after a power cycle: the kit ${b2[0]} "${b2[1]}", its SNARE's DRIVE ${b2[2]} ${b2[3]}, COMP ${b2[4]}`,
          b2[0] === ld[0] && b2[1] === "LO-FI" && b2[2] === 3 && b2[3] === 70 && b2[4] === 30);
  }
}

{                                               // V0.8 BENCH: CORE 2 (sy_c2.c): the emulator has one core
  const k = await boot(), kt = () => cstr(k, k.web_test_toast());
  const pg = goPage(k, SEC.DEBUG, "CORE 2");     // (V0.87, U5: in DEBUG)
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

{                                               // V0.86: the menu buttons once and twice (B1-B17, C3, C4)
  const b = await boot(), name = () => cstr(b, b.web_test_page_name()), sec = () => b.web_test_page_section();
  const toast = () => cstr(b, b.web_test_toast());
  const btap = (x) => { b.web_buttons(1 << x); play(b, 0.03); b.web_buttons(0); play(b, 0.35); };
  const twice = (x) => { for (let i = 0; i < 2; i++) { b.web_buttons(1 << x); play(b, 0.03); b.web_buttons(0); play(b, 0.08); } play(b, 0.3); };
  splash(b);
  btap(B.EDIT);
  const w = [sec(), name()];
  b.web_enc(E.ALGO, 2); play(b, 0.05);
  btap(B.EDIT);                                 // (once in its menu: its first page)
  const w2 = [sec(), name()];
  twice(B.EDIT);                                // (V0.87: twice, the tables' list; untouched 5 s it goes)
  const j = [b.web_test_pl(0), b.web_test_pl(3), b.web_test_lay()];
  play(b, 5.2);
  const j2 = [b.web_test_pl(0), sec()];
  check(`EDIT once: OSC A ${w[1]} (${w[0]}); on a later page, once: its first (${w2}); twice: the tables' list (${j}); 5 s: gone (${j2})`,
        w.join() === `${SEC.OSCA},WAVE` && w2.join() === `${SEC.OSCA},WAVE` && j.join() === "1,2,0" && j2.join() === `0,${SEC.OSCA}`);
  btap(B.EDIT);
  const K1 = b.web_test_page_virt(0), v0 = b.web_test_value(K1);
  twice(B.LFO);                                 // (V0.87, U16: LFO twice, the assign window; KNOB 1 its source; LFO shuts it)
  const m = [b.web_test_win(0), b.web_test_win(4)];
  b.web_enc(E.K1, 1); play(b, 0.13);
  const v1 = b.web_test_value(K1), src1 = b.web_test_win(4);
  btap(B.LFO);
  const m2 = [b.web_test_win(0), b.web_test_value(K1)];
  twice(B.GLO);                                 // (GLO twice: PERFORM latched; GLO closes it)
  const p = b.web_test_lay();
  btap(B.GLO);
  check(`LFO twice on OSC A: the assign window (${m}), KNOB 1 its source (${src1}), the value kept (${v0} -> ${v1}); LFO: shut (${m2[0]}, ${m2[1]}); GLO twice: PERFORM latched (${p}), GLO: closed (${b.web_test_lay()})`,
        m.join() === `1,${SRC.LFO1}` && src1 === SRC.LFO2 && v1 === v0 && m2[0] === 0 && m2[1] === v0 && p === 1 && b.web_test_lay() === 0);
  twice(B.SCL);                                 // (SCL twice: the chords on; again off. ARP twice: on, again off)
  const c1 = b.web_test_chd(0, -1);
  twice(B.SCL);
  const c2 = b.web_test_chd(0, -1);
  twice(B.ARP);
  const a1 = b.web_test_chd(4, -1);
  twice(B.ARP);
  check(`SCL twice: the chords on (${c1}), again off (${c2}); ARP twice: on (${a1}), again off (${b.web_test_chd(4, -1)})`,
        c1 !== 0 && c2 === 0 && a1 === 1 && b.web_test_chd(4, -1) === 0);
  btap(B.SEQ);                                  // (SEQ once: LOOP and CLIP; from elsewhere the page left there; in its
  const q1 = [sec(), name()];                   // menu, CLIPS; twice: TRACK and SONG)
  b.web_enc(E.ALGO, 1); play(b, 0.05);
  const q1b = name();
  btap(B.ENV);
  btap(B.SEQ);
  const q3 = [sec(), name()];
  btap(B.SEQ);
  const q4 = [sec(), name()];
  twice(B.SEQ);
  const pjl = [b.web_test_pl(0), b.web_test_pl(3)];
  play(b, 5.2);
  check(`SEQ once: ${q1} (then ${q1b}); from ENV once: the page left in LOOP and CLIP (${q3}); once in its menu: ${q4}; twice: the projects' list (${pjl})`,
        q1.join() === `${SEC.SEQ},CLIPS` && q1b === "MIXER" && q3.join() === `${SEC.SEQ},MIXER` && q4.join() === `${SEC.SEQ},CLIPS` &&
        pjl.join() === "1,3");
  btap(B.EDIT);                                 // (ENV twice on OSC A: the assign window, ENV 2 first; EDIT shuts it)
  twice(B.ENV);
  const e2 = [b.web_test_win(0), b.web_test_win(4)];
  btap(B.EDIT);
  const e1 = [b.web_test_win(0), sec()];
  btap(B.REC);                                  // (REC twice after a tap that stopped it: nothing; V0.87's TAKE)
  const r1 = b.web_test_lp(10, -1);
  twice(B.REC);
  const r2 = b.web_test_lp(10, -1);
  btap(B.REC);
  check(`ENV twice on OSC A: the assign window (${e2}); EDIT: shut (${e1}); REC: the count-in (${r1}); REC twice: off, the second nothing (${r2}); REC: again (${b.web_test_lp(10, -1)})`,
        e2.join() === "1,2" && e1.join() === `0,${SEC.OSCA}` && r1 === 1 && r2 === 0 && b.web_test_lp(10, -1) === 1);
  btap(B.REC);
  twice(B.HOME);                                // (HOME twice: the SETTINGS; a tap: back)
  const h1 = sec();
  btap(B.HOME);
  b.web_test_set_get(0, 1);                     // (HELP on: a menu opened from elsewhere says what twice does)
  btap(B.EDIT);
  btap(B.ENV);
  const hint = toast();
  b.web_test_set_get(0, 0);
  check(`HOME twice: the SETTINGS (${h1}); a tap: back (${sec()}); HELP on: ENV opened from EDIT ("${hint}")`,
        h1 === SEC.MENU && sec() !== SEC.MENU && /^AGAIN: /.test(hint));
  btap(B.HOME);                                 // (V0.87.1: DEBUG from HOME only)
  b.web_buttons(1 << B.HOME);                   // (HOME held: the help map; held on: DEBUG at 5 s)
  play(b, 1.0);
  const hm = cstr(b, b.web_test_cd());
  play(b, 4.3);
  b.web_buttons(0);
  play(b, 0.1);
  check(`HOME held: the help map ("${hm}"); held 5 s: DEBUG (section ${sec()})`, hm.startsWith("HELP: DEBUG") && sec() === SEC.DEBUG);
}
{                                               // V0.86: SEQ's TOOLS, SPEED, TYPE's guard, CLIPS' keys, the grid held
  const q = await boot(), qt = () => cstr(q, q.web_test_toast()), lp = (f, v = -1) => q.web_test_lp(f, v);
  const nn = (t, s) => q.web_test_lp_clip(t, s, 2, -1), len = (t, s) => q.web_test_lp_clip(t, s, 0, -1);
  const turn = (e, s) => { q.web_enc(e, s); play(q, 0.13); };
  splash(q);
  for (const [tk, n] of [[0, 60], [96, 64], [384, 67], [480, 72]]) q.web_test_lp_put(0, 0, tk, 48, n, 100);
  q.web_test_lp_clip(0, 0, 0, 768);             // (T1 A: two bars, four notes, one a beat in each bar's first half)
  lp(0, 0); lp(1, 0);
  goPage(q, SEC.LCLIP, "TOOLS");
  play(q, 0.05);
  turn(E.K1, 1);                                // COPY: T1 A
  const cp = qt();
  lp(1, 1);                                     // (scene B: empty) PASTE: at once
  turn(E.K2, 1);
  const p1 = [qt(), nn(0, 1), len(0, 1)];
  turn(E.K2, 1);                                // (again: something goes, asked)
  const p2 = [qt(), nn(0, 1)];
  turn(E.K2, 1);
  const p3 = qt();
  check(`TOOLS: COPY ("${cp}"); PASTE into the empty T1 B at once ("${p1[0]}", ${p1[1]} notes, ${p1[2]} ticks); again asked ("${p2[0]}"), again: done ("${p3}")`,
        cp === "COPIED: T1 A" && p1[0] === "PASTED INTO T1 B" && p1[1] === 4 && p1[2] === 768 && p2[0] === "AGAIN: T1 B GOES" &&
        p2[1] === 4 && p3 === "PASTED INTO T1 B");
  turn(E.K3, 1);                                // DOUBLE: four bars, the notes again
  const d1 = [qt(), len(0, 1), nn(0, 1)];
  turn(E.K3, -1);                               // HALVE: notes go, asked; again: two bars
  const h1 = [qt(), len(0, 1)];
  turn(E.K3, -1);
  const h2 = [qt(), len(0, 1), nn(0, 1)];
  check(`DOUBLE ("${d1[0]}", ${d1[1]} ticks, ${d1[2]} notes); HALVE asked ("${h1[0]}", ${h1[1]}), again: "${h2[0]}" (${h2[1]} ticks, ${h2[2]} notes)`,
        d1[0] === "DOUBLED: 4 BARS" && d1[1] === 1536 && d1[2] === 8 && h1[0] === "AGAIN: ITS SECOND HALF GOES" && h1[1] === 1536 &&
        h2[0] === "HALVED: 2 BARS" && h2[1] === 768 && h2[2] === 4);
  lp(1, 0);                                     // COPY on: BAR 1 of T1 A; into T1 C (empty: no bar 1); into T1 B's bar 1
  turn(E.K1, 1);
  const cb = qt();
  lp(1, 2);
  turn(E.K2, 1);
  const nb = qt();
  lp(1, 1);
  turn(E.K2, 1);
  const ab = qt();
  turn(E.K2, 1);
  const pb = [qt(), nn(0, 1)];
  turn(E.K4, 1);                                // CLEAR: its notes, asked first
  const cl1 = [qt(), nn(0, 1)];
  turn(E.K4, 1);
  check(`COPY on: "${cb}"; PASTE into T1 C: "${nb}"; into T1 B: asked ("${ab}"), done ("${pb[0]}", ${pb[1]} notes); CLEAR asked ("${cl1[0]}", ${cl1[1]}), done ("${qt()}", ${nn(0, 1)})`,
        cb === "COPIED: BAR 1 OF T1 A" && nb === "T1 C HAS NO BAR 1" && ab === "AGAIN: BAR 1 OF T1 B GOES" &&
        pb[0] === "PASTED INTO BAR 1 OF T1 B" && pb[1] === 4 && cl1[0] === "AGAIN: ITS NOTES GO" && cl1[1] === 4 &&
        qt() === "NOTES CLEARED" && nn(0, 1) === 0 && len(0, 1) === 768);
  lp(1, 0);                                     // SPEED: CLIP's KNOB 2, x1/2 x1 x2
  goPage(q, SEC.LCLIP, "CLIP");
  play(q, 0.05);
  const sp0 = q.web_test_lp_clip(0, 0, 4, -1);
  turn(E.K2, 1);
  const sp2 = q.web_test_lp_clip(0, 0, 4, -1);
  q.web_test_lp_launch(0, 0);                   // (launched: the transport runs; two bars at x2: a pass a bar)
  q.web_buttons(1 << B.PLAY); play(q, 0.03); q.web_buttons(0);
  play(q, 4.2);
  const passes = q.web_test_lp_trk(0, 13, -1);
  q.web_buttons(1 << B.PLAY); play(q, 0.03); q.web_buttons(0); play(q, 0.3);
  turn(E.K2, -2);
  check(`SPEED: x1 (${sp0}) -> x2 (${sp2}); two bars at x2, two bars of time: ${passes} passes; down to x1/2 (${q.web_test_lp_clip(0, 0, 4, -1)})`,
        sp0 === 0 && sp2 === 2 && passes >= 2 && q.web_test_lp_clip(0, 0, 4, -1) === 1);
  goPage(q, SEC.SEQ, "CLIPS");                  // TYPE (C6): an empty track's at the turn's end; one with clips asks
  play(q, 0.05);
  lp(0, 1);
  turn(E.K1, 1);
  const ty0 = q.web_test_lp_trk(1, 3, -1);
  play(q, 0.6);
  const ty1 = q.web_test_lp_trk(1, 3, -1);
  lp(0, 0);
  turn(E.K1, 2);
  play(q, 0.6);
  const ask = [q.web_test_win(3), q.web_test_lp_trk(0, 0, -1)];
  turn(E.K1, -1);                               // (asked: turning again asks another; no time out)
  play(q, 3.0);
  const ask2 = [q.web_test_win(3), q.web_test_lp_trk(0, 0, -1)];
  twiceB(q, B.SEQ);                             // (V0.87.1: SEQ twice, the button that asked)
  const ty2 = [q.web_test_win(3), q.web_test_lp_trk(0, 0, -1), qt(), q.web_test_lp_trk(0, 3, -1)];
  turn(E.K1, 1);                                // (asked again; another button: kept)
  play(q, 0.6);
  tap(q, B.EDIT);
  const tkept = [qt(), q.web_test_lp_trk(0, 0, -1), q.web_test_win(3)];
  goPage(q, SEC.SEQ, "CLIPS");
  q.web_test_lp_trk(0, 3, 0);
  turn(E.K1, 1);                                // (asked again: SEQ once is a no: it waits out a second tap, then closes)
  play(q, 0.6);
  const w0 = q.web_test_win(3);
  q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons(0); play(q, 0.1);
  const w1 = q.web_test_win(3);
  play(q, 0.4);
  const once = [w0, w1, q.web_test_win(3), qt(), q.web_test_lp_trk(0, 3, -1)];
  turn(E.K1, 1);                                // (asked again: SAVE is another button: a no)
  play(q, 0.6);
  tap(q, B.SAVE);
  const sv = [qt(), q.web_test_win(3), q.web_test_lp_trk(0, 3, -1)];
  goPage(q, SEC.SEQ, "CLIPS");
  q.web_test_lp_trk(0, 3, 0);
  check(`TYPE on an empty T2: OUT ${ty0} while turning, ${ty1} after; T1 (clips) asked (window ${ask[0]}, type ${ask[1]}); turned again, still asked at 3 s (${ask2}); SEQ twice: yes (${ty2}); asked again, EDIT: "${tkept[0]}" (${tkept[1]}, ${tkept[2]}); SEQ once: waits, then no (${once}); SAVE: no (${sv})`,
        ty0 === 0 && ty1 === 1 && ask.join() === "1,0" && ask2.join() === "1,0" && ty2[0] === 0 && ty2[3] === 1 && ty2[2] === "T1 SYNTH+MIDI" &&
        tkept[0] === "TYPE KEPT" && tkept[1] === ty2[1] && tkept[2] === 0 &&
        once.join() === "1,1,0,TYPE KEPT,0" && sv.join() === "TYPE KEPT,0,0");
  q.web_test_lp_trk(0, 0, 0);
  const W = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26], BK = [1, 3, 5, 8, 10, 13, 15, 17, 20];
  const kn0 = q.web_test_key_note();
  q.web_keys(1 << W[0]); play(q, 0.05); q.web_keys(0); play(q, 0.2);   // CLIPS' keys: white 1 T1's clip, black 2 scene B
  const k1 = [qt(), q.web_test_key_note() === kn0 ? "none" : "one"];
  q.web_keys(1 << BK[1]); play(q, 0.05); q.web_keys(0); play(q, 0.2);
  const k2 = [qt(), lp(1)];
  q.web_buttons(1 << B.PLAY); play(q, 0.03); q.web_buttons(0); play(q, 0.3);
  check(`CLIPS' keys: white 1 launches T1 A ("${k1[0]}", the key's own note: ${k1[1]}); black 2 scene B ("${k2[0]}", picked ${k2[1]})`,
        k1[0].startsWith("T1 ") && k1[1] === "none" && k2[0].startsWith("SCENE B") && k2[1] === 1);
  q.web_test_lp_trk(7, 0, 2);                   // the grid, a step held (S7): T8 DRUMS
  lp(0, 7); lp(1, 0);
  goPage(q, 51, "GRID");
  play(q, 0.05);
  q.web_keys(1 << W[4]); play(q, 0.1);          // (step 5 put in, held: the knobs its hit's)
  const held = [cstr(q, q.web_test_page_name()), q.web_test_grid(5, 0, 4, 0)];
  turn(E.K3, 2);                                // (CHANCE)
  turn(E.K4, 1);                                // (RATCHET)
  q.web_keys(0); play(q, 0.1);
  const kept = [q.web_test_grid(5, 0, 4, 0), q.web_test_grid(3, 0, 4, 0), q.web_test_grid(4, 0, 4, 0)];
  q.web_keys(1 << W[4]); play(q, 0.1); q.web_keys(0); play(q, 0.1);   // (held again, no knob: out at the release)
  const out = q.web_test_grid(5, 0, 4, 0);
  q.web_keys(1 << W[4]); play(q, 0.1); q.web_keys(0); play(q, 0.1);   // (a tap on an empty step: in)
  check(`the grid: step 5 held (${held}); CHANCE, RATCHET turned: kept at the release (${kept}); held again, no turn: out (${out}); a tap: in (${q.web_test_grid(5, 0, 4, 0)})`,
        held[1] === 1 && kept.join() === "1,2,1" && out === 0 && q.web_test_grid(5, 0, 4, 0) === 1);
  play(q, 3.8);                                 // (3 s of quiet: the project written) a power cycle: SPEED kept
  q.web_test_lp_clip(0, 0, 4, 2);
  play(q, 3.8);
  const nor = new Uint8Array(q.memory.buffer, q.web_nor(), q.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const q2 = instance.exports;
  if (q2._initialize) q2._initialize();
  new Uint8Array(q2.memory.buffer, q2.web_nor(), q2.web_nor_size()).set(nor);
  q2.web_master(700);
  q2.web_boot();
  play(q2, 0.3);
  check(`.. after a power cycle: T1 A's SPEED x2 (${q2.web_test_lp_clip(0, 0, 4, -1)}), T1 B's length ${q2.web_test_lp_clip(0, 1, 0, -1)}`,
        q2.web_test_lp_clip(0, 0, 4, -1) === 2 && q2.web_test_lp_clip(0, 1, 0, -1) === 768);
}
{                                               // V0.86: centre detents (F26), SETTINGS > POWER (F31, F34)
  const p = await boot(), sec = () => p.web_test_page_section(), pt = () => cstr(p, p.web_test_toast());
  const turn = (e, s) => { p.web_enc(e, s); play(p, 0.13); };
  splash(p);
  goPage(p, SEC.OSCA, "OUTPUT");
  play(p, 0.05);
  const PAN = p.web_test_page_virt(1);
  p.web_test_set(PAN, -1);
  const pans = [];
  for (let i = 0; i < 5; i++) { turn(E.K2, 1); pans.push(p.web_test_value(PAN)); }
  p.web_test_set(PAN, 0);
  play(p, 0.8);
  turn(E.K2, 1);
  const fresh = p.web_test_value(PAN);          // (a new gesture from the middle: no hold)
  check(`a centre detent: PAN from -1, a detent at a time: ${pans} (through the middle: there, three held); from the middle, a new gesture: ${fresh}`,
        pans.slice(0, 4).join() === "0,0,0,0" && pans[4] > 0 && fresh > 0);
  const lv = [520, 540, 570, 600].map((r) => p.web_test_power(r, 0));
  const usb = p.web_test_power(-1, 1);
  p.web_test_power(575, 0);
  check(`the battery: 520 ${lv[0]}, 540 ${lv[1]}, 570 ${lv[2]}, 600 ${lv[3]} bars (the stock thresholds 531 / 561 / 591); USB power ${usb}`,
        lv.join() === "0,1,2,3" && usb === 4);
  goPage(p, SEC.MENU, "POWER");
  play(p, 0.05);
  turn(E.K1, 2);                                // (SHOW BATTERY: EVERY SCREEN)
  turn(E.K2, 1);                                // (SCREEN OFF AFTER: 1 MIN)
  const pg = p.web_test_page();
  play(p, 59.0);
  const on = [p.web_test_scr(), p.web_test_lcd_off()];
  play(p, 1.2);
  const off = [p.web_test_scr(), p.web_test_lcd_off()];
  const d0 = p.web_screen_draws();
  play(p, 1.0);
  const dark = p.web_screen_draws() - d0;
  p.web_buttons(1 << B.EDIT); play(p, 0.03); p.web_buttons(0); play(p, 0.5);   // (EDIT wakes it, and does nothing else)
  const woke = [p.web_test_scr(), p.web_test_lcd_off(), p.web_test_page() === pg];
  check(`SCREEN OFF AFTER 1 MIN: on at 59 s (${on}), dark after (${off}), nothing drawn (${dark}); EDIT wakes it (${woke}), the page kept`,
        on.join() === "0,0" && off.join() === "1,1" && dark === 0 && woke.join() === "0,0,true");
  play(p, 3.8);                                 // (the settings written) a power cycle: SHOW BATTERY, SCREEN OFF kept
  const nor = new Uint8Array(p.memory.buffer, p.web_nor(), p.web_nor_size()).slice();
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const p2 = instance.exports;
  if (p2._initialize) p2._initialize();
  new Uint8Array(p2.memory.buffer, p2.web_nor(), p2.web_nor_size()).set(nor);
  p2.web_master(700);
  p2.web_boot();
  splash(p2);
  goPage(p2, SEC.MENU, "POWER");
  check(`.. after a power cycle: SHOW BATTERY, SCREEN OFF AFTER kept (${p2.web_test_set_get(7, -1)}, ${p2.web_test_set_get(8, -1)})`,
        p2.web_test_set_get(7, -1) === 2 && p2.web_test_set_get(8, -1) === 1);
}

{                                               // V0.87: INIT at power-on; REC twice: TAKE; tap tempo; the assign window's slot
  const r = (await WebAssembly.instantiate(bytes, {})).instance.exports;   // and POLARITY; WIPE
  if (r._initialize) r._initialize();
  r.web_nor_erase(); r.web_master(700); r.web_boot(); play(r, 2.2);
  const init = [cstr(r, r.web_test_patch_name()), [0, 1, 2, 3].map((s) => r.web_test_slot(s, 0)).join(" ")];
  check(`power-on on a new unit: "${init[0]}", its first four slots the macros (${init[1]}); the LEDs HIGH (${r.web_dim_level()})`,
        init[0] === "INIT" && init[1] === "9 10 11 12" && r.web_dim_level() === 3);
  const v = await boot(), vt = () => cstr(v, v.web_test_toast());
  splash(v);
  for (const k of [0, 4, 7]) { v.web_keys(1 << k); play(v, 0.2); v.web_keys(0); play(v, 0.2); }
  twiceB(v, B.REC);
  const tk = [vt(), v.web_test_lp_clip(0, 0, 0, -1), v.web_test_lp_clip(0, 0, 2, -1), v.web_test_lp(10, -1)];
  twiceB(v, B.REC);
  check(`REC twice: TAKE ("${tk[0]}": length ${tk[1]}, ${tk[2]} notes, not recording ${tk[3]}); again, nothing new played: "${vt()}"`,
        tk[0] === "T1 A: TAKE OF 3 NOTES" && tk[1] > 0 && tk[2] === 3 && tk[3] === 0 && vt() === "NOTHING PLAYED TO TAKE");
  v.web_buttons(1 << B.GLO); play(v, 0.7);      // (GLO held, PERFORM: the lowest key, F3, taps the tempo, 0.4 s apart; V0.87.1)
  const kn = v.web_test_key_note(), nv0 = v.web_test_voices();
  let heardKey = 0;
  for (let i = 0; i < 4; i++) { v.web_keys(1); play(v, 0.03); heardKey |= v.web_test_voices() > nv0; v.web_keys(0); play(v, 0.37); }
  const tt = [vt(), v.web_test_tempo(0), v.web_test_run(), heardKey, v.web_test_key_note() === kn];
  v.web_buttons((1 << B.GLO) | (1 << B.PLAY)); play(v, 0.03); v.web_buttons(1 << B.GLO); play(v, 0.3);   // (GLO + PLAY: PLAY's own)
  const pl = [v.web_test_run(), vt()];
  v.web_buttons((1 << B.GLO) | (1 << B.PLAY)); play(v, 0.03); v.web_buttons(1 << B.GLO); play(v, 0.3);
  const pl2 = v.web_test_run();
  v.web_buttons(0); play(v, 0.3);
  check(`GLO held + F3 tapped 4 times, 0.4 s apart: "${tt[0]}" (${tt[1]} BPM), the transport stopped (${tt[2]}), F3 silent (${tt[3] ? "sounded" : "silent"}, key note kept ${tt[4]}); GLO + PLAY: PLAY's own (running ${pl[0]}, "${pl[1]}"), again stops (${pl2})`,
        tt[0] === "TAP: 150 BPM" && tt[1] === 150 && tt[2] === 0 && !tt[3] && tt[4] && pl[0] === 1 && pl[1] === "PLAY" && pl2 === 0);
  goPage(v, SEC.OSCA, "WAVE"); play(v, 0.05);
  twiceB(v, B.LFO);                             // (the assign window: ALGORITHM its page 2, a slot taken; KNOB 2 AMOUNT,
  v.web_enc(E.ALGO, 1); play(v, 0.05);          // KNOB 1 POLARITY: UNIPOLAR)
  const pg2 = v.web_test_win(0);
  for (let i = 0; i < 5; i++) { v.web_enc(E.K2, 1); play(v, 0.13); }
  v.web_enc(E.K1, 1); play(v, 0.13);
  const pol = v.web_test_win(6);
  tap(v, B.LFO);
  const sl = slots(v).findIndex(([a, d, m]) => a === SRC.LFO1 && d === ID.A_POS && m !== 0);
  check(`LFO twice on WAVE, page 2 (${pg2}): LFO 1 > WT POS in slot ${sl + 1} (AMOUNT ${sl >= 0 ? slots(v)[sl][2] : "-"}), UNIPOLAR (POLARITY bits ${pol}); shut, kept (${v.web_test_win(0)})`,
        pg2 === 2 && sl >= 0 && slots(v)[sl][2] > 0 && ((pol >> sl) & 1) === 1 && v.web_test_win(0) === 0);
  v.web_test_palette(3);
  const inSettings = goPage(v, SEC.MENU, "WIPE");   // (V0.87.1: WIPE is DEBUG's, HOME twice answers)
  v.web_test_palette(3);
  goPage(v, SEC.DEBUG, "WIPE"); play(v, 0.05);
  v.web_enc(E.K1, 1); play(v, 0.13);
  const wq = v.web_test_win(3);
  tap(v, B.SAVE);                               // (SAVE: another button: no)
  const w1 = [v.web_test_win(3), v.web_test_palette(-1), vt()];
  v.web_enc(E.K1, 1); play(v, 0.13);
  v.web_buttons(1 << B.HOME); play(v, 0.03); v.web_buttons(0); play(v, 0.1);   // (HOME once: waits for a second, then no)
  const w2a = v.web_test_win(3);
  play(v, 0.5);
  const w2 = [w2a, v.web_test_win(3), v.web_test_palette(-1), vt(), v.web_test_page_section()];
  v.web_enc(E.K1, 1); play(v, 0.13);
  v.web_buttons(1 << B.HOME); play(v, 0.03); v.web_buttons(0); play(v, 0.08);
  v.web_buttons(1 << B.HOME); play(v, 0.03); v.web_buttons(0); play(v, 0.3);
  check(`WIPE in DEBUG (not in SETTINGS: ${inSettings}): asked (${wq}); SAVE: no (${w1}); HOME once: waits (${w2[0]}), then no (${w2.slice(1)}); HOME twice: "${vt()}", the palette STRATA (${v.web_test_palette(-1)}), LIGHTS HIGH (${v.web_dim_level()}), still DEBUG (${v.web_test_page_section()})`,
        inSettings === -1 && wq === 3 && w1.join() === "0,3,KEPT" && w2[0] === 3 && w2[1] === 0 && w2[2] === 3 && w2[3] === "KEPT" &&
        w2[4] === SEC.DEBUG && /^WIPED/.test(vt()) && v.web_test_palette(-1) === 0 && v.web_dim_level() === 3 &&
        v.web_test_page_section() === SEC.DEBUG);
}
{                                               // V0.87 step 2: the drum models, presets and kits, the user's, EUCLID
  const d = await boot(), dt = () => cstr(d, d.web_test_toast()), dn = (r) => cstr(d, d.web_test_dk_name(r));
  const dbtn = (b) => { d.web_buttons(1 << b); play(d, 0.03); d.web_buttons(0); play(d, 0.05); };
  splash(d);
  d.web_test_lp_trk(0, 0, 2); d.web_test_lp(0, 0); play(d, 0.2);
  goPage(d, 52, "MODEL"); play(d, 0.05);
  const m0 = [d.web_test_dkm(0, 0), d.web_test_dkm(0, 1), dn(0)];
  const heard = [];
  for (let m = 0; m < 7; m++) {                 // (KNOB 1 the model: each heard on the KICK row)
    d.web_enc(E.K1, -10); play(d, 0.2); d.web_enc(E.K1, m); play(d, 0.2);
    d.web_test_drum_hit(0, 0, 120, 0);
    heard.push(`${d.web_test_dkm(0, 0)}:${play(d, 0.4).peak > 0.02 ? 1 : 0}`);
  }
  d.web_enc(E.K2, 5); play(d, 0.2);             // (KNOB 2 its TYPE: WAVE's TABLE B)
  const tb = [d.web_test_dkm(0, 0), d.web_test_dkm(0, 1), dn(0)];
  check(`DRUM > MODEL: the KICK row PUNCH KICK as a kit starts (${m0}); KNOB 1 each model, each heard (${heard.join(" ")}); KNOB 2 TYPE (${tb})`,
        m0.join() === "1,0,KICK" && heard.join(" ") === "0:1 1:1 2:1 3:1 4:1 5:1 6:1" && tb.join() === "6,1,KICK");
  twiceB(d, B.EDIT);                            // (EDIT twice on DRUM: the row's category's presets; PRESETS, SAVE twice)
  const dl = [d.web_test_pl(0), d.web_test_pl(3), d.web_test_pl(5)];
  d.web_enc(E.PRESET, 1); play(d, 0.1);
  twiceB(d, B.EDIT);
  const ld = [dn(0), d.web_test_dkm(0, 0), d.web_test_dkm(0, 1), d.web_test_dkm(0, 2), d.web_test_pl(0), dt()];
  twiceB(d, B.EDIT);
  d.web_enc(E.SELECT, 1); play(d, 0.1);
  const cat = [d.web_test_pl(5), d.web_test_pl(1)];
  play(d, 5.3);
  check(`EDIT twice on DRUM: the drum presets (${dl}: open, DRUMS, KICK); PRESETS, EDIT twice: "${ld[0]}" in the row (${ld.slice(1, 4)}, the list gone ${ld[4]}, "${ld[5]}"); SELECT: SNARE's (${cat})`,
        dl.join() === "1,4,0" && ld[0] === "909 KICK" && ld.slice(1, 5).join() === "1,0,2,0" && ld[5] === "KICK: 909 KICK" &&
        cat.join() === "1,0");
  dbtn(B.SAVE); play(d, 0.5);                   // (SAVE on DRUM: the drum; a factory's: SAVE AS NEW, NAME IT, written)
  const sw = [d.web_test_win(1), d.web_test_win(2)];
  dbtn(B.SAVE); play(d, 0.4);
  const nm = [d.web_test_page_section(), cstr(d, d.web_test_save_name())];
  dbtn(B.SAVE); play(d, 0.8);
  const sv = [dt(), d.web_test_dkm(0, 2), d.web_test_dkm(0, 3), dn(0), d.web_test_page_section()];
  goPage(d, 52, "SOUND"); d.web_enc(E.K1, 3); play(d, 0.3);
  dbtn(B.SAVE); play(d, 0.5);
  const sw2 = d.web_test_win(2);
  dbtn(B.SAVE); play(d, 0.8);
  check(`SAVE on DRUM: the window (${sw}), NAME IT (${nm}); SAVE: "${sv[0]}" (the row's preset ${sv[1]}, ${sv[2]} of the user's, "${sv[3]}", back ${sv[4]}); edited, SAVE over it (row ${sw2 + 1}): "${dt()}"`,
        sw.join() === "1,1" && nm[0] === SEC.SAVE && nm[1] === "909 KICK v2" && sv[0] === "SAVED: 909 KICK v2 · DRUM 1" &&
        sv[1] === 128 && sv[2] === 1 && sv[3] === "909 KICK v2" && sv[4] === 52 && sw2 === 0 && dt() === "SAVED OVER DRUM 1");
  goPage(d, 50, "ACCENT"); play(d, 0.1);        // (SAVE on KIT: the kit, as new, NAME IT)
  dbtn(B.SAVE); play(d, 0.5); dbtn(B.SAVE); play(d, 0.4);
  const kn = cstr(d, d.web_test_save_name());
  dbtn(B.SAVE); play(d, 0.8);
  const ks = [dt(), cstr(d, d.web_test_kit_name()), d.web_test_dkm(0, 4)];
  twiceB(d, B.EDIT);                            // (EDIT twice on KIT: the kits, the factory's six then the user's)
  const kl = [d.web_test_pl(0), d.web_test_pl(3), d.web_test_pl(1)];
  d.web_enc(E.PRESET, -10); play(d, 0.1);
  twiceB(d, B.EDIT);
  const k808 = [cstr(d, d.web_test_kit_name()), dn(0), d.web_test_dkm(0, 0), dn(3), d.web_test_dkm(3, 0)];
  twiceB(d, B.EDIT);
  d.web_enc(E.PRESET, 6); play(d, 0.1);
  twiceB(d, B.EDIT);
  check(`SAVE on KIT: "${kn}", "${ks[0]}" (the kit "${ks[1]}", ${ks[2]} kit of the user's); EDIT twice: the kits (${kl}); 808: "${k808[0]}", rows "${k808[1]}" (${k808[2]}), "${k808[3]}" (${k808[4]}); the user's back: "${cstr(d, d.web_test_kit_name())}"`,
        kn === "DRUM KIT v2" && ks[0] === "SAVED: DRUM KIT v2 · KIT 1" && ks[1] === "DRUM KIT v2" && ks[2] === 1 &&
        kl.join() === "1,1,6" && k808.join() === "808,808 KICK,0,808 HAT,5" && cstr(d, d.web_test_kit_name()) === "DRUM KIT v2");
  goPage(d, 51, "EUCLID"); play(d, 0.1);        // (EUCLID: the KICK row refilled, the others kept)
  d.web_test_grid(0, 1, 4, 100);
  d.web_enc(E.K1, 1); play(d, 0.2);
  const e1 = [d.web_test_dkm(0, 5), d.web_test_dkm(1, 5)];
  d.web_enc(E.K2, -8); play(d, 0.2);
  const e2 = [d.web_test_dkm(0, 5), d.web_test_dkm(1, 5)];
  check(`EUCLID: HITS 5 of 16 on the KICK row (${e1[0]} hits, the SNARE's ${e1[1]} kept); STEPS 8: twice a bar (${e2[0]}, ${e2[1]})`,
        e1.join() === "5,1" && e2.join() === "10,1");
  play(d, 3.8);                                 // (a power cycle: the user's drums and kits kept)
  const nor = new Uint8Array(d.memory.buffer, d.web_nor(), d.web_nor_size()).slice();
  const d2 = (await WebAssembly.instantiate(bytes, {})).instance.exports;
  if (d2._initialize) d2._initialize();
  new Uint8Array(d2.memory.buffer, d2.web_nor(), d2.web_nor_size()).set(nor);
  d2.web_master(700); d2.web_boot(); play(d2, 2.2);
  check(`.. after a power cycle: the user's drum presets ${d2.web_test_dkm(0, 3)}, kits ${d2.web_test_dkm(0, 4)}`,
        d2.web_test_dkm(0, 3) === 1 && d2.web_test_dkm(0, 4) === 1);
  goPage(d, 52, "SOUND");                        // (the drum presets' list: the user's deleted, asked first)
  twiceB(d, B.EDIT);
  d.web_enc(E.PRESET, 20); play(d, 0.1);
  const lch = (b) => { d.web_buttons(1 << B.SAVE); play(d, 0.05); d.web_buttons((1 << B.SAVE) | (1 << b)); play(d, 0.03);
                       d.web_buttons(1 << B.SAVE); play(d, 0.03); d.web_buttons(0); play(d, 0.4); };
  lch(B.OCTDN);
  const asked = [d.web_test_pl(2), d.web_test_dkm(0, 3)];
  lch(B.OCTDN); play(d, 0.8);
  check(`the drum presets' list: SAVE + OCT- on the user's asks (${asked}), again: "${dt()}" (${d.web_test_dkm(0, 3)} left)`,
        asked[0] === 9 && asked[1] === 1 && dt() === "DELETED DRUM 1" && d.web_test_dkm(0, 3) === 0);
}
{                                               // V0.87.1: a list is loaded from by its opener twice; once, or another button, leaves it
  const n = await boot();
  splash(n);
  goPage(n, SEC.OSCA, "WAVE"); play(n, 0.05);
  const t0 = n.web_test_value(ID.A_TABLE);
  twiceB(n, B.EDIT);
  n.web_enc(E.PRESET, 2); play(n, 0.1);
  const open0 = n.web_test_pl(0);
  saveTwice(n);                                 // (SAVE twice: no longer loads a table)
  const sv = [n.web_test_pl(0), n.web_test_value(ID.A_TABLE) === t0];
  twiceB(n, B.EDIT);
  n.web_enc(E.PRESET, 2); play(n, 0.1);
  n.web_buttons(1 << B.EDIT); play(n, 0.03); n.web_buttons(0); play(n, 0.1);
  const once0 = n.web_test_pl(0);
  play(n, 0.5);
  const once = [once0, n.web_test_pl(0), n.web_test_value(ID.A_TABLE) === t0];
  twiceB(n, B.EDIT);
  n.web_enc(E.PRESET, 2); play(n, 0.1);
  tap(n, B.ENV);
  const oth = [n.web_test_pl(0), n.web_test_value(ID.A_TABLE) === t0];
  check(`the tables' list: open (${open0}); SAVE twice leaves it, nothing loaded (${sv}); EDIT once: waits (${once[0]}), then gone (${once[1]}), nothing loaded (${once[2]}); another button: gone, nothing loaded (${oth})`,
        open0 === 1 && sv.join() === "0,true" && once.join() === "1,0,true" && oth.join() === "0,true");
  goPage(n, SEC.OSCA, "WAVE"); play(n, 0.05);
  twiceB(n, B.SEQ);                             // the projects' list: SEQ twice loads, SAVE twice does not
  n.web_enc(E.PRESET, 1); play(n, 0.1);
  const pj0 = [n.web_test_pl(0), n.web_test_pl(3), n.web_test_lp(7, -1)];
  saveTwice(n); play(n, 0.4);
  const pj1 = [n.web_test_pl(0), n.web_test_lp(7, -1)];
  twiceB(n, B.SEQ);
  n.web_enc(E.PRESET, 1); play(n, 0.1);
  twiceB(n, B.SEQ); play(n, 0.5);
  check(`the projects' list (${pj0}): SAVE twice leaves it, no project loaded (${pj1}); SEQ twice: project ${n.web_test_lp(7, -1) + 1} loaded`,
        pj0.join() === "1,3,0" && pj1.join() === "0,0" && n.web_test_lp(7, -1) === 1);
  n.web_test_lp_trk(0, 0, 2); n.web_test_lp(0, 0); play(n, 0.2);   // the drum presets' and the kits' lists on a drum track
  goPage(n, 52, "MODEL"); play(n, 0.05);
  const pre = n.web_test_dkm(0, 2);
  twiceB(n, B.EDIT);
  n.web_enc(E.PRESET, 1); play(n, 0.1);
  const drk = n.web_test_pl(3);
  saveTwice(n);
  const dr = [drk, n.web_test_pl(0), n.web_test_dkm(0, 2) === pre];
  goPage(n, 50, "ACCENT"); play(n, 0.05);
  twiceB(n, B.EDIT);
  n.web_enc(E.PRESET, 2); play(n, 0.1);
  const kit0 = [n.web_test_pl(0), n.web_test_pl(3)], kn0 = cstr(n, n.web_test_kit_name());
  saveTwice(n);
  const kt = [n.web_test_pl(0), cstr(n, n.web_test_kit_name()) === kn0];
  check(`the drum presets' list (kind ${dr[0]}): SAVE twice leaves it, the row's preset kept (${dr.slice(1)}); the kits' (${kit0}): SAVE twice leaves it, the kit kept (${kt})`,
        dr.join() === "4,0,true" && kit0.join() === "1,1" && kt.join() === "0,true");
}
{                                               // V0.87.1: DEBUG > TESTS, the four test projects (sy_testproj.c: stubs here)
  const t = await boot(), tt = () => cstr(t, t.web_test_toast()), sec = () => t.web_test_page_section();
  splash(t);
  tap(t, B.HOME);
  t.web_buttons(1 << B.HOME); play(t, 5.2); t.web_buttons(0); play(t, 0.1);   // (DEBUG: HOME held 5 s)
  goPage(t, SEC.DEBUG, "TESTS"); play(t, 0.05);
  const nm = () => t.web_test_tp(0, 0) + 1;
  const names = [nm()];
  t.web_enc(E.K1, 1); play(t, 0.1); names.push(nm());
  t.web_enc(E.K1, 5); play(t, 0.1); names.push(nm());
  t.web_enc(E.K1, -9); play(t, 0.1); names.push(nm());
  const q0 = t.web_test_win(3);
  play(t, 0.8);
  const asked = [q0, t.web_test_win(3), t.web_test_tp(0, 0)];
  t.web_enc(E.K1, 2); play(t, 0.1);                // (asked: turning picks another, still asked)
  const again = [t.web_test_win(3), t.web_test_tp(0, 0)];
  tap(t, B.EDIT);                                  // (another button: no)
  const no = [t.web_test_win(3), tt()];
  goPage(t, SEC.DEBUG, "TESTS"); play(t, 0.05);
  t.web_enc(E.K1, 1); play(t, 0.8);
  twiceB(t, B.HOME);                               // (HOME twice: load; the stub has none: NOT AVAILABLE)
  const na = [t.web_test_win(3), tt(), sec(), t.web_test_tp(2, 0)];
  play(t, 3.0);                                    // (the real load is asynchronous: it saves the owner's work, hushes, builds, plays)
  const up = [t.web_test_tp(2, 0), t.web_test_tp(4, 0) > 1500];
  t.web_enc(E.K2, 1); play(t, 0.3);                // (KNOB 2: stops it; it stays loaded, its time still)
  const stop = tt(), ms1 = t.web_test_tp(4, 0);
  play(t, 1.0);
  const down = [t.web_test_tp(2, 0), t.web_test_tp(4, 0) === ms1, ms1 > 1500];
  t.web_test_tp(1, 1); play(t, 2.2);              // (a test shown loaded, its time moving: the page shows it)
  const run = [t.web_test_tp(2, 0), t.web_test_tp(3, 0) > 1500];
  t.web_test_tp(1, -1);
  check(`DEBUG > TESTS: KNOB 1 picks TEST ${names.join(", ")} (stops at both ends); asked a moment after the last detent (${asked}), turning again keeps it (${again}); EDIT: no (${no}); HOME twice loads the real TEST 4: "${na[1]}" (question ${na[0]}, still DEBUG ${na[2] === SEC.DEBUG}, loaded ${na[3]}); 3 s on it plays (${up[0]}, its time over 1.5 s ${up[1]}); KNOB 2: "${stop}", still loaded ${down[0]}, its time stopped ${down[1]} at ${down[2]}; one shown loaded (${run})`,
        names.join() === "1,2,4,1" && asked.join() === "0,4,0" &&
        again.join() === "4,2" && no.join() === "0,KEPT" && na[0] === 0 && na[1] === "TEST 4: LOOPER" && na[2] === SEC.DEBUG &&
        na[3] === 4 && up[0] === 4 && up[1] && stop === "TEST STOPPED" && down[0] === 4 && down[1] && down[2] && run.join() === "2,true");
}
{                                               // V0.87.1: a test loaded, nothing saved: the top line's mark and HOME's line show only while sy_tp_active() is set
  const m = await boot();
  splash(m);
  const fb = () => new Uint16Array(m.memory.buffer, m.web_screen(), 240 * 240).slice();
  const band = (f, y0, y1) => f.slice(y0 * 240, y1 * 240);
  const diff = (a, b) => a.reduce((n, v, i) => n + (v !== b[i]), 0);
  const rows = [];
  for (const [name, set] of [["HOME", () => goPage(m, SEC.MACRO, "SOUND")], ["ENV 1", () => { for (let q = 0; q < 200; q++) { m.web_test_go(q); if (m.web_test_page_section() === SEC.ENV1) break; } }]]) {
    set(); play(m, 0.4);
    const a = fb();
    m.web_test_tp(1, 2); play(m, 0.4);
    const b = fb();
    m.web_test_tp(1, -1); play(m, 0.4);
    const c = fb();
    rows.push([name, diff(band(a, 0, 16), band(b, 0, 16)), diff(band(a, 0, 16), band(c, 0, 16)),
               name === "HOME" ? diff(band(a, 148, 166), band(b, 148, 166)) : 0, diff(a, c)]);
  }
  check(`a test loaded: the top line differs (${rows.map(r => r[0] + " " + r[1]).join(", ")} pixels), HOME's line under the rows differs (${rows[0][3]}); the test ended from sy_tp_active(): back as it was (${rows.map(r => r[2] + "/" + r[4]).join(", ")} pixels)`,
        rows.every(r => r[1] > 100 && r[2] === 0 && r[4] === 0) && rows[0][3] > 100);
}
{                                               // V0.87.1: CLIPS: the scenes on OCT-, OCT+, MUTE / SOLO on KNOB 2, the track on KNOB 4, PRESETS the sound's list
  const c = await boot();
  splash(c);
  goPage(c, SEC.SEQ, "CLIPS"); play(c, 0.05);
  const oct0 = c.web_test_oct(), sc0 = c.web_test_lp(1, -1);
  tap(c, B.OCTUP); tap(c, B.OCTUP);
  const up = [c.web_test_lp(1, -1), c.web_test_oct() === oct0];
  tap(c, B.OCTDN); tap(c, B.OCTDN); tap(c, B.OCTDN);
  const dn = c.web_test_lp(1, -1);
  const names = [0, 1, 2, 3].map((k) => cstr(c, c.web_test_knob_name(k)));
  c.web_enc(E.K2, 1); play(c, 0.15);
  const ms1 = [c.web_test_lp_trk(0, 6, -1), c.web_test_lp_trk(0, 7, -1)];
  c.web_enc(E.K2, 1); play(c, 0.15);
  const ms2 = [c.web_test_lp_trk(0, 6, -1), c.web_test_lp_trk(0, 7, -1)];
  c.web_enc(E.K2, -2); play(c, 0.15);
  const sel0 = c.web_test_lp(0, -1);
  c.web_enc(E.K4, 2); play(c, 0.15);
  const sel1 = c.web_test_lp(0, -1);
  c.web_enc(E.K4, -5); play(c, 0.15);
  const sel2 = c.web_test_lp(0, -1);
  c.web_enc(E.PRESET, 1); play(c, 0.15);
  const pl = [c.web_test_pl(0), c.web_test_pl(3), c.web_test_lp(0, -1)];
  check(`CLIPS: OCT+ twice: scene ${sc0} -> ${up[0]} (the octave kept ${up[1]}), OCT- three times: ${dn} (stops at A); knobs ${names.join(" | ")}; KNOB 2: MUTE (${ms1}), SOLO (${ms2}), back to plays; KNOB 4: track ${sel0 + 1} -> ${sel1 + 1} -> ${sel2 + 1}; PRESETS: the sounds' list (open ${pl[0]}, kind ${pl[1]}), the track kept (${pl[2] + 1})`,
        sc0 === 0 && up.join() === "2,true" && dn === 0 && names[2] === "" && names[3].length > 0 &&
        ms1.join() === "1,0" && ms2.join() === "0,1" && c.web_test_lp_trk(0, 6, -1) === 0 && c.web_test_lp_trk(0, 7, -1) === 0 &&
        sel0 === 0 && sel1 === 2 && sel2 === 0 && pl.join() === "1,0,0");
}
{                                               // V0.87.1: TAPE moves with the sound (frames apart differ), still in silence
  const m = await boot();
  splash(m);
  m.web_test_set(180, 23);                      // (SP_FXC1: TAPE, instance 22, first in the chain)
  m.web_test_fx_on(22, 1);
  goPage(m, 34, "TAPE"); play(m, 0.1);
  const fb = () => new Uint16Array(m.memory.buffer, m.web_screen(), 240 * 240).slice();
  const diff = (a, b) => a.reduce((n, v, i) => n + (v !== b[i]), 0);
  const quiet0 = fb(); play(m, 0.2); const quiet1 = fb();
  m.web_keys((1 << 7) | (1 << 11) | (1 << 14)); play(m, 0.4);
  const f0 = fb(); play(m, 0.12); const f1 = fb(); play(m, 0.3); const f2 = fb();
  const moved = [diff(f0, f1), diff(f1, f2)];
  m.web_keys(0); play(m, 4.0);
  const q1 = fb(); play(m, 0.2); const q2 = fb();
  check(`TAPE: silent, nothing moves (${diff(quiet0, quiet1)} pixels); with a chord, two frames 0.12 s and 0.3 s apart differ (${moved} pixels); after it dies away it is still again (${diff(q1, q2)})`,
        diff(quiet0, quiet1) === 0 && moved[0] > 200 && moved[1] > 200 && diff(q1, q2) === 0);
}
{                                               // V0.87.1: KEY SCAN names the encoders' lines: KNOB 1's are PA5 at columns 9 and 10 (counted from 1)
  const k = await boot();
  splash(k);
  const e = [k.web_test_ks_enc(1, 8), k.web_test_ks_enc(1, 9), k.web_test_ks_enc(0, 0), k.web_test_ks_enc(5, 1), k.web_test_ks_enc(1, 7), k.web_test_ks_enc(2, 8)];
  goPage(k, SEC.DEBUG, "KEY SCAN"); play(k, 0.1);
  check(`KEY SCAN: PA5:9 and PA5:10 are encoder ${e[0]} and ${e[1]} (KNOB 1's, matrix encoder 2); PA0:1 ${e[2]}, PB7:2 ${e[3]}; PA5:8 (a key) ${e[4]}, PA6:9 (a key) ${e[5]}`,
        e.join() === "2,2,0,6,-1,-1");
}
// V0.87.1, the pre-build review's UI findings (DECISIONS.md item 24): each of these failed before its fix
const loadedReal = (x) => x.web_test_tp(4, 0) >= 0;   // (the real test project, sy_testproj.c: its time, -1 none)
const toDebug = (x) => { x.web_buttons(1 << B.HOME); play(x, 5.2); x.web_buttons(0); play(x, 0.1); };
{                                               // 1. a question that opens by itself takes no press begun before it came
  const t = await boot(), tt = () => cstr(t, t.web_test_toast());
  splash(t); tap(t, B.HOME); toDebug(t);
  goPage(t, SEC.DEBUG, "TESTS"); play(t, 0.05);
  t.web_enc(E.K1, 1); play(t, 0.55);              // (the question comes at 0.6 s)
  const q0 = t.web_test_win(3);
  t.web_buttons(1 << B.HOME); play(t, 0.1); const qmid = t.web_test_win(3); t.web_buttons(0); play(t, 0.08);
  t.web_buttons(1 << B.HOME); play(t, 0.03); t.web_buttons(0); play(t, 0.3);   // (this HOME is a first tap, not a second)
  play(t, 3.0);
  const a = [q0, qmid, t.web_test_win(3), loadedReal(t), t.web_test_page_section() === SEC.DEBUG, tt()];
  const y = await boot();                         // (TYPE's question on CLIPS: SEQ pressed 0.45 s after KNOB 1, released after it came)
  splash(y);
  goPage(y, SEC.SEQ, "CLIPS"); play(y, 0.05);
  y.web_buttons(1 << B.SEQ); play(y, 0.03); y.web_buttons((1 << B.SEQ) | (1 << B.OCTUP)); play(y, 0.03); y.web_buttons(0); play(y, 0.4);   // (a clip on T1)
  y.web_enc(E.K1, 1); play(y, 0.45);
  y.web_buttons(1 << B.SEQ); play(y, 0.15); const ymid = y.web_test_win(3); y.web_buttons(0); play(y, 0.08);
  y.web_buttons(1 << B.SEQ); play(y, 0.03); y.web_buttons(0); play(y, 0.3);
  play(y, 1.0);
  const b = [ymid, y.web_test_win(3), y.web_test_lp_trk(0, 3, -1), y.web_test_lp(0, -1)];
  check(`a question opened by itself takes no earlier press: HOME down 0.55 s after KNOB 1, up after TESTS' question (${a[0]} -> ${a[1]}), then one tap: question ${a[2]}, no test loaded (${a[3]}), still DEBUG (${a[4]}); the same on TYPE (${b[0]}, question ${b[1]}, T1 OUT ${b[2]} kept)`,
        a[0] === 0 && a[1] !== 0 && a[2] === 0 && !a[3] && a[4] && b[0] !== 0 && b[1] === 0 && b[2] === 0);
}
{                                               // 2. a list opened and confirmed with its cursor unmoved loads nothing over what is there
  const n = await boot(); splash(n);
  n.web_test_lp_trk(0, 0, 2); n.web_test_lp(0, 0); play(n, 0.2);   // (T1 on drums)
  goPage(n, SEC.DRUM, "MODEL"); play(n, 0.05);
  const m0 = [n.web_test_dkm(0, 0), n.web_test_dkm(0, 1), n.web_test_dkm(0, 2)];
  n.web_enc(E.K2, 3); play(n, 0.6);
  const m1 = [n.web_test_dkm(0, 0), n.web_test_dkm(0, 1), n.web_test_dkm(0, 2)];
  twiceB(n, B.EDIT);
  const open = [n.web_test_pl(0), n.web_test_pl(3)];
  twiceB(n, B.EDIT);
  const m2 = [n.web_test_dkm(0, 0), n.web_test_dkm(0, 1), n.web_test_dkm(0, 2)];
  const dt = cstr(n, n.web_test_toast()), shut = n.web_test_pl(0) === 0;
  twiceB(n, B.EDIT);                              // (a cursor moved there and back: confirmed, loaded as ever)
  n.web_enc(E.PRESET, 1); play(n, 0.1); n.web_enc(E.PRESET, -1); play(n, 0.1);
  twiceB(n, B.EDIT);
  const m3 = [n.web_test_dkm(0, 0), n.web_test_dkm(0, 1), n.web_test_dkm(0, 2)];
  const k = await boot(); splash(k);
  k.web_test_lp_trk(0, 0, 2); k.web_test_lp(0, 0); play(k, 0.2);
  goPage(k, SEC.KIT, "ACCENT"); play(k, 0.05);
  const a0 = k.web_test_kit(0, 10, -1);
  k.web_enc(E.K1, 5); play(k, 0.6);
  const a1 = k.web_test_kit(0, 10, -1);
  twiceB(k, B.EDIT);
  const kopen = [k.web_test_pl(0), k.web_test_pl(3)];
  twiceB(k, B.EDIT);
  const a2 = k.web_test_kit(0, 10, -1), kt = cstr(k, k.web_test_toast());
  twiceB(k, B.EDIT);
  k.web_enc(E.PRESET, 1); play(k, 0.1); k.web_enc(E.PRESET, -1); play(k, 0.1);   // (moved away and back: it loads the kit again)
  twiceB(k, B.EDIT);
  const a3 = k.web_test_kit(0, 10, -1);
  check(`a list confirmed unmoved: the drum row edited ${m0} -> ${m1}, EDIT twice (list ${open}), EDIT twice: the row kept (${m2}, list ${shut ? "shut" : "open"}, "${dt}"); moved and back: loaded (${m3}); the kit's ACCENT ${a0} -> ${a1}, the kits' list (${kopen}), confirmed unmoved: kept (${a2}, "${kt}"); moved and back: the kit loaded again (${a3})`,
        m0.join() !== m1.join() && open.join() === "1,4" && m2.join() === m1.join() && shut && dt === "THE ROW IS KEPT" &&
        m3.join() !== m1.join() && a1 !== a0 && kopen.join() === "1,1" && a2 === a1 && kt === "IT IS THE ONE LOADED" && a3 === a0);
}
{                                               // 3. F3 taps the tempo only while GLO is held; PERFORM latched, it plays
  const t = await boot(); splash(t);
  const bpm0 = t.web_test_tempo(0);
  t.web_keys(1); play(t, 0.2); const v0 = t.web_test_voices(); t.web_keys(0); play(t, 0.5);
  twiceB(t, B.GLO);
  const lay = t.web_test_lay();
  t.web_keys(1); play(t, 0.2); const v1 = t.web_test_voices(), tst = cstr(t, t.web_test_toast()); t.web_keys(0); play(t, 0.4);
  t.web_buttons(1 << B.GLO); play(t, 0.03);       // (GLO held on the latched layer: F3 taps again, silent)
  t.web_keys(1); play(t, 0.2); const v2 = t.web_test_voices(); t.web_keys(0); play(t, 0.05); t.web_buttons(0); play(t, 0.3);
  check(`F3 with PERFORM latched (GLO twice, layer ${lay}): plays (${v1} voice, "${tst}", tempo ${bpm0} kept ${t.web_test_tempo(0) === bpm0}); plain F3 played (${v0}); with GLO held it taps, silent (${v2} voices)`,
        lay === 1 && v0 === 1 && v1 === 1 && !/^TAP/.test(tst) && v2 === 0);
}
{                                               // 4. WIPE and GLOBAL's reset end a loaded test first; 5. KNOB 2 stops a load under way
  const mk = async () => { const x = await boot(); splash(x); tap(x, B.HOME); toDebug(x); return x; };
  const loadTest = (x) => { goPage(x, SEC.DEBUG, "TESTS"); play(x, 0.05); x.web_enc(E.K1, 1); play(x, 0.8); };
  const w = await mk();
  loadTest(w);
  twiceB(w, B.HOME); play(w, 3.0);
  const on = [loadedReal(w), w.web_test_tp(2, 0)];
  goPage(w, SEC.DEBUG, "WIPE"); play(w, 0.05);
  w.web_enc(E.K1, 1); play(w, 0.13);
  twiceB(w, B.HOME);
  const wiped = [cstr(w, w.web_test_toast()), w.web_test_tp(2, 0), w.web_test_tempo(0)];
  const g = await mk();
  loadTest(g);
  twiceB(g, B.HOME); play(g, 3.0);
  goPage(g, SEC.GLOBAL, "TEMPO"); play(g, 0.05);
  g.web_enc(E.K1, -20); play(g, 0.3);
  const bp = g.web_test_tempo(0);
  const [, rst] = octHold(g, B.GLO, 5.3);
  const reset = [rst, g.web_test_tp(2, 0), g.web_test_tempo(0)];
  const s = await mk();
  loadTest(s);
  s.web_buttons(1 << B.HOME); play(s, 0.03); s.web_buttons(0); play(s, 0.08);
  s.web_buttons(1 << B.HOME); play(s, 0.03); s.web_buttons(0); play(s, 0.003);   // (the load begins: about 9 ms to build)
  const building = s.web_test_tp(2, 0) === 0;
  s.web_enc(E.K2, 1); play(s, 0.003);
  const stp = [cstr(s, s.web_test_toast()), loadedReal(s)];
  play(s, 3.0);
  const after = [loadedReal(s), s.web_test_tp(2, 0)];
  check(`WIPE ends a loaded test first (loaded ${on}; after: "${wiped[0]}", test ${wiped[1]}, tempo ${wiped[2]}); GLOBAL's reset (tempo ${bp}): test ${reset[1]}, tempo ${reset[2]}; KNOB 2 while it loads (not yet built ${building}): "${stp[0]}", and 3 s on nothing is loaded (${after})`,
        on[0] && on[1] === 2 && /^WIPED/.test(wiped[0]) && wiped[1] === 0 && wiped[2] === 120 &&
        bp !== 120 && reset[1] === 0 && reset[2] === 120 && building && stp[0] === "TEST STOPPED" && !after[0] && after[1] === 0);
}
{                                               // 6. CLIPS: OCT- with DELETE? open closes it as the scene steps
  const q = await boot(); splash(q);
  goPage(q, SEC.SEQ, "CLIPS"); play(q, 0.05);
  q.web_test_lp(1, 2);
  q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons((1 << B.SEQ) | (1 << B.OCTUP)); play(q, 0.03); q.web_buttons(0); play(q, 0.4);
  const sdel = () => { q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons((1 << B.SEQ) | (1 << B.OCTDN)); play(q, 0.03);
                       q.web_buttons(1 << B.SEQ); play(q, 0.03); q.web_buttons(0); play(q, 0.35); };
  const clip0 = q.web_test_lp_clip(0, 2, 0, -1);
  sdel();
  const asked = [q.web_test_win(3), q.web_test_lp(1, -1)];
  tap(q, B.OCTDN);
  const after = [q.web_test_win(3), q.web_test_lp(1, -1), q.web_test_lp_clip(0, 2, 0, -1)];
  check(`CLIPS: a clip made (${clip0}), SEQ + OCT-: DELETE? asked (${asked[0]}, scene ${asked[1]}); OCT- alone: the scene steps (${after[1]}) and the question is gone (${after[0]}), the clip kept (${after[2]})`,
        clip0 > 0 && asked[0] !== 0 && asked[1] === 2 && after[0] === 0 && after[1] === 1 && after[2] === clip0);
}
{                                               // 7. TAPE: once the sound has died away nothing is left on the screen that a full redraw does not draw
  const m = await boot(); splash(m);
  m.web_test_set(180, 23); m.web_test_fx_on(22, 1);
  const fb = () => new Uint16Array(m.memory.buffer, m.web_screen(), 240 * 240).slice();
  const diff = (a, b) => a.reduce((n, v, i) => n + (v !== b[i]), 0);
  const left = [];
  for (const gap of [0, 1, 2, 3, 4, 5, 6, 7]) {   // (the level settles at a different frame, an even or an odd one, each time)
    goPage(m, 34, "TAPE"); play(m, 0.1);
    m.web_keys((1 << 7) | (1 << 11) | (1 << 14)); play(m, 0.4 + gap * 0.013);
    m.web_keys(0); play(m, 4.0);
    const settled = fb();
    goPage(m, SEC.MACRO, "SOUND"); play(m, 0.2);
    goPage(m, 34, "TAPE"); play(m, 0.3);
    left.push(diff(settled, fb()));
  }
  check(`TAPE: settled after sound, the screen equals a full redraw of it (differing pixels ${left})`, left.every((d) => d === 0));
}
console.log(`emu: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
