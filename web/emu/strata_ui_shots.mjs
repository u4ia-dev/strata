// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 u4ia
// The STRATA UI (the strata-ui build) on the simulated FM-1, screen by screen, as PNG files named after the design's
// boards (its gallery: render/out/<Id>-black-1x.png), to set beside them: each screen reached as the panel reaches it
// (a page by its section, a knob turned for a turning shot, a button held for a layer or a countdown, SAVE tapped),
// the values set as the boards show them where the panel would take long. 240 x 240 (the panel's own pixels) and
// twice that. It prints each file and what the session set.
//   node web/emu/strata_ui_shots.mjs [build/emu_strata/strata.wasm] [build/emu_strata/ui_shots]   (after build_strata.sh)
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

const wasm = process.argv[2] || "build/emu_strata/strata.wasm", out = process.argv[3] || "build/emu_strata/ui_shots";
const { instance } = await WebAssembly.instantiate(readFileSync(wasm), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_nor_erase();
ex.web_master(700);
ex.web_boot();
ex.web_test_set_get(0, 0);                      // (HELP's hints off)
const run = (s) => { for (let n = Math.max(1, Math.round(s * 44118 / 128)); n > 0; n--) ex.web_render(128); };
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12,
            OCTUP: 13 };                                                                     // panel.c B_*
const E = { SELECT: 0, ALGO: 1, PRESET: 2, K1: 3, K2: 4, K3: 5, K4: 6 };                    // panel.c EN_*
const SEC = { OSCA: 0, OSCB: 1, SUB: 2, NOISE: 3, FILTER: 4, ENV1: 5, LFO1: 7, VOICE: 9, MACRO: 10, MATRIX: 11,
              CHAIN: 36, MENU: 37, GLOBAL: 38, KEYS: 41, ARP: 42, CHORD: 43, CMID: 44, STEPS: 45, SEQ: 46, LTRK: 47,
              LCLIP: 48, LSONG: 49, KIT: 50, LGRID: 51, DRUM: 52, DEBUG: 53 };   // SEC_*
const FXC1 = 180;                                                                            // sy_param.c SP_FXC1
const KEY = { F3: 0, G3: 2, A3: 4, C4: 7, E4: 11, G4: 14 };                                 // the keys, F3 = 0
const tap = (b) => { ex.web_buttons(1 << b); run(0.05); ex.web_buttons(0); run(0.35); };   // (two within 300 ms latch)
const rest = () => run(1.1);                    // (a knob's gesture over: its handle and flag gone)
const cstr = (p) => { const m = new Uint8Array(ex.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
function go(sec, name) {                        // the page by its section and name
  for (let p = 0, n = ex.web_test_go(0); p < n; p++) {
    ex.web_test_go(p);
    if (ex.web_test_page_section() === sec && cstr(ex.web_test_page_name()) === name) { run(0.05); return p; }
  }
  throw new Error(`no page ${sec} ${name}`);
}
function chain(insts) {                         // the FX chain: these instances in this order, each in
  for (let k = 0; k < 8; k++) ex.web_test_set(FXC1 + k, k < insts.length ? insts[k] + 1 : 0);
  for (const i of insts) ex.web_test_fx_on(i, 1);
  run(0.05);
}
const turn = (k, n) => { ex.web_enc(E.K1 + k, n); run(0.08); };
const twice = (b) => { for (let i = 0; i < 2; i++) { ex.web_buttons(1 << b); run(0.03); ex.web_buttons(0); run(0.08); } run(0.3); };   // (V0.87)

function crc32(buf) {
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    let c = (crc ^ buf[n]) & 0xff;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const len = Buffer.alloc(4), td = Buffer.concat([Buffer.from(type), data]), crc = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  crc.writeUInt32BE(crc32(td));
  return Buffer.concat([len, td, crc]);
}
function pngOf(scale, lint) {                   // the frame buffer: RGB565, high byte first (the panel's order); lint:
  const fb = new Uint16Array(ex.memory.buffer, lint ? ex.web_lint_screen() : ex.web_screen(), 240 * 240), W = 240 * scale;   // the check's, its findings
  const mk = lint ? new Uint8Array(ex.memory.buffer, ex.web_lint_px(), 240 * 240) : null;
  const raw = Buffer.alloc((W * 3 + 1) * W);
  for (let y = 0; y < W; y++)
    for (let x = 0; x < W; x++) {
      const v = fb[Math.floor(y / scale) * 240 + Math.floor(x / scale)], c = ((v & 0xff) << 8) | (v >> 8);
      const r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31, o = y * (W * 3 + 1) + 1 + x * 3;
      raw[o] = (r << 3) | (r >> 2);
      raw[o + 1] = (g << 2) | (g >> 4);
      raw[o + 2] = (b << 3) | (b >> 2);
      const m = mk ? mk[Math.floor(y / scale) * 240 + Math.floor(x / scale)] : 0;
      if (m) { raw[o] = 255; raw[o + 1] = m === 2 ? 0 : 255; raw[o + 2] = m === 2 ? 255 : 0; }
    }
  const hdr = Buffer.alloc(13);
  hdr.writeUInt32BE(W, 0);
  hdr.writeUInt32BE(W, 4);
  hdr[8] = 8;
  hdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", hdr), chunk("IDAT", deflateSync(raw)),
                        chunk("IEND", Buffer.alloc(0))]);
}
const shots = [], lints = [];
function shot(id) {
  run(0.05);
  writeFileSync(`${out}/${id}.png`, pngOf(1));
  writeFileSync(`${out}/${id}-2x.png`, pngOf(2));
  shots.push(id);
  console.log(`shot: ${out}/${id}.png  (${cstr(ex.web_test_toast()) || "no toast"})`);
  const n = ex.web_lint();                      // the screen's check (strata_web.c web_lint): text on a drawing, on text
  if (n) {
    const rep = cstr(ex.web_lint_report());
    lints.push(`${id}: ${n}\n${rep}`);
    console.log(`  LINT ${n}\n${rep.replace(/^/gm, "    ")}`);
    writeFileSync(`${out}/lint/${id}.png`, pngOf(3, true));
  }
}

rmSync(out, { recursive: true, force: true });       // (no shot left from a build before)
mkdirSync(`${out}/lint`, { recursive: true });
run(0.6);
shot("Boot");                                   // (V0.86, P4: the splash held 2 s, then HOME)
run(1.6);
shot("Home");

// ---- EDIT: OSC A, OSC B, SUB, NOISE, FILTER
go(SEC.OSCA, "WAVE");
const [POS, TABLE, WARP, WAMT] = [0, 1, 2, 3].map((k) => ex.web_test_page_param(k));
ex.web_test_set(TABLE, 27);                     // VOWELS
ex.web_test_set(POS, 375);
ex.web_test_set(WARP, 2);                       // BEND+
ex.web_test_set(WAMT, 300);
run(0.6);                                       // (the table builds)
shot("Osc");
go(SEC.OSCA, "UNISON");
const UNI = ex.web_test_page_param(0);
ex.web_test_set(UNI, 7);
ex.web_test_set(ex.web_test_page_param(1), 350);
run(0.1);
shot("OscUni");
turn(1, 3);
shot("OscTurn");
rest();
go(SEC.OSCA, "SPREAD");
ex.web_test_set(ex.web_test_page_param(1), 500);
ex.web_test_set(ex.web_test_page_param(3), 400);
shot("OscSpread");
go(SEC.OSCA, "PITCH");
ex.web_test_set(ex.web_test_page_param(1), 7);
ex.web_test_set(ex.web_test_page_param(2), 6);
shot("OscPitch");
go(SEC.OSCA, "OUTPUT");
ex.web_test_set(ex.web_test_page_param(0), 800);
ex.web_test_set(ex.web_test_page_param(1), 200);
shot("OscOut");
go(SEC.OSCB, "WAVE");
shot("OscBOff");
const on = () => { ex.web_buttons(1 << B.EDIT); run(0.05); ex.web_buttons((1 << B.EDIT) | (1 << B.OCTUP)); run(0.05);
                   ex.web_buttons(1 << B.EDIT); run(0.05); ex.web_buttons(0); run(1.7); };   // EDIT + OCT+ (its toast gone)
go(SEC.SUB, "MAIN");
on();
ex.web_test_set(ex.web_test_page_param(2), 700);
shot("Sub");
go(SEC.SUB, "OUTPUT");
shot("SubOut");
go(SEC.NOISE, "MAIN");
on();
ex.web_test_set(ex.web_test_page_param(0), 1);  // PINK
ex.web_test_set(ex.web_test_page_param(1), -500);
shot("Noise");
go(SEC.NOISE, "OUTPUT");
shot("NoiseOut");
go(SEC.FILTER, "TONE");
ex.web_test_set(ex.web_test_page_param(0), 1);  // LP 24
ex.web_test_set(ex.web_test_page_param(1), 560);
ex.web_test_set(ex.web_test_page_param(2), 450);
run(0.1);
shot("Filter");
turn(1, 6);
shot("FilterTurn");
rest();
go(SEC.FILTER, "RESPONSE");
ex.web_test_set(ex.web_test_page_param(0), 500);
ex.web_test_set(ex.web_test_page_param(1), 300);
shot("FilterMore");
go(SEC.FILTER, "VALUES");                       // V0.67: VALUES (the first: TONE's knobs), a knob turning
shot("FilterValues");
turn(1, 4);
shot("FilterValuesTurn");
rest();
go(SEC.OSCA, "VALUES");
shot("OscValues");

// ---- ENV, LFO (a chord held: the voices on them)
go(SEC.ENV1, "ADSR");
ex.web_keys((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4));
run(0.25);
shot("Env");
ex.web_keys(0);
run(0.3);
go(SEC.ENV1, "SHAPE");
ex.web_test_set(ex.web_test_page_param(0), 200);
shot("EnvShape");
go(SEC.LFO1, "POINTS");
ex.web_keys(1 << KEY.C4);
turn(0, 1);
rest();
shot("Lfo");
ex.web_keys(0);
run(0.2);
go(SEC.LFO1, "SHAPE");
shot("LfoShape");
go(SEC.LFO1, "TIME");
ex.web_test_set(ex.web_test_page_param(0), 300);
ex.web_test_set(ex.web_test_page_param(1), 350);
ex.web_test_set(ex.web_test_page_param(3), 500);
shot("LfoTime");

// ---- FX: the chain view, then each module
chain([0, 1, 2, 3, 4]);
go(SEC.CHAIN, "CHAIN");
ex.web_enc(E.SELECT, 2);
run(0.1);
ex.web_buttons(1 << B.FX);                      // FX held: MACROS; C4 bypasses the fifth module (OTT)
run(0.7);
ex.web_keys(1 << KEY.C4);
run(0.05);
ex.web_keys(0);
run(0.05);
ex.web_buttons(0);
run(1.7);
shot("FxChain");
ex.web_enc(E.ALGO, 7);                          // the type to add: PHASER
run(0.05);
ex.web_enc(E.SELECT, 5);
run(0.05);
ex.web_buttons(1 << B.FX);
run(0.05);
ex.web_buttons((1 << B.FX) | (1 << B.OCTUP));
run(0.05);
ex.web_buttons(1 << B.FX);
run(0.05);
ex.web_buttons(0);
run(0.2);
shot("FxAdded");
run(1.6);
const MODS = [
  [12, ["DRIVE", "Drive"]], [13, ["CHORUS", "Chorus"], ["SPACE", "ChorusMore"]], [14, ["DELAY", "Delay"], ["TONE", "DelayTone"]],
  [15, ["REVERB", "Reverb"], ["TONE", "ReverbTone"]], [16, ["OTT", "Ott"], ["BANDS", "OttBands"]],
];
const MODS2 = [
  [17, ["POINTS", "Eq"], ["SHAPES", "EqShapes"]], [22, ["FLANGER", "Flanger"], ["TONE", "FlangerMore"]],
  [24, ["PHASER", "Phaser"], ["TONE", "PhaserMore"]], [26, ["FILTER", "FxFilter"], ["DYNAMICS", "FxFilterMore"]],
  [28, ["CRUSH", "Crush"]], [30, ["TREMOLO", "Tremolo"], ["SHAPE", "TremoloMore"]], [32, ["RING MOD", "Ring"]],
  [34, ["TAPE", "Tape"], ["TONE", "TapeTone"]],
];
chain([0, 1, 2, 3, 4]);
for (const [sec, ...pages] of MODS) for (const [nm, id] of pages) { go(sec, nm); shot(id); }
chain([5, 10, 12, 14, 16, 18, 20, 22]);
go(17, "SHAPES");                               // the EQ (V0.86): SMILE, a little up the range
turn(0, 7);
turn(1, 3);
rest();
for (const [sec, ...pages] of MODS2) for (const [nm, id] of pages) { go(sec, nm); shot(id); }
chain([0, 1, 2, 3]);

// ---- GLO, KEYS
go(SEC.VOICE, "VOICE");
ex.web_keys((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4));
run(0.2);
shot("Voice");
ex.web_keys(0);
run(0.3);
go(SEC.VOICE, "PLAY");
shot("VoicePlay");
{                                               // five slots, as the board: ENV 2 -> CUTOFF, LFO 1 -> WT POS, ..
  go(SEC.FILTER, "TONE");
  const CUT = ex.web_test_page_param(1);
  go(15, "REVERB");
  const RVMIX = ex.web_test_page_param(3);
  go(SEC.OSCA, "OUTPUT");
  const PAN = ex.web_test_page_param(1);
  go(SEC.MATRIX, "SLOTS");
  const base = ex.web_test_page_virt(1), set = (s, src, dst, amt) => { ex.web_test_set(base + 3 * s, src);
    ex.web_test_set(base + 3 * s + 1, dst); ex.web_test_set(base + 3 * s + 2, amt); };   // (SRC_*: sy_param.c)
  set(0, 2, CUT, 450); set(1, 3, POS, 300); set(2, 9, RVMIX, 600); set(3, 5, CUT, 200); set(4, 4, PAN, -350);
  run(0.05);
}
shot("Matrix");
go(SEC.GLOBAL, "TEMPO");
shot("Tempo");
ex.web_test_chd(21, 1);                         // (V0.75: CLOCK IN MIDI, no clock yet: the panel's tempo)
run(0.05);
shot("TempoClock");
ex.web_test_chd(21, 0);
run(0.05);
go(SEC.KEYS, "SCALE");
turn(0, 2);                                     // MINOR
rest();
shot("Keys");
turn(0, -2);                                    // (back to CHROMATIC for the chords)
rest();

// ---- V0.7, V0.75: SCL's chords (FREE: MIN7 on E4; KEY in C major; the ARP's styles; STRUM; BASS; MIDI)
go(SEC.CHORD, "CHORD");
shot("ChordOff");
ex.web_test_chd(0, 2);                          // FREE MIN7, root position
ex.web_test_chd(1, 11);
ex.web_test_chd(3, 4);
run(0.05);
const held = (k, s) => { ex.web_keys(1 << k); run(s); };
held(11, 0.15);                                 // E4
shot("Chord");
turn(1, 3);                                     // TYPE turning (MIN7 -> DIM7): the strip, the current HOT
shot("ChordType");
turn(2, -4);                                    // VOICING to AUTO, turning
shot("ChordTurn");
rest();
ex.web_keys(0);
run(0.2);
ex.web_test_chd(0, 1);                          // KEY in C major: A4 (A minor), SHAPE 7TH, SPREAD OPEN
go(SEC.KEYS, "SCALE");
turn(0, 1);
rest();
go(SEC.CHORD, "CHORD");
ex.web_test_chd(16, 1);
ex.web_test_chd(2, 1);
ex.web_test_chd(9, 1);                          // (the bass on)
run(0.05);
held(16, 0.15);
shot("ChordKey");
ex.web_keys(0);
run(0.3);
ex.web_test_chd(2, 0);
go(SEC.CHORD, "STRUM");                         // STRUM 40 ms, DOWN, HUMAN 30 %, TAPER 50 %
ex.web_test_chd(22, 10);
ex.web_test_chd(23, 1);
ex.web_test_chd(24, 30);
ex.web_test_chd(25, 50);
run(0.05);
held(16, 0.3);
shot("Strum");
turn(0, 2);                                     // STRUM turning
shot("StrumTurn");
rest();
ex.web_keys(0);
run(0.3);
for (const f of [22, 23, 24, 25]) ex.web_test_chd(f, 0);
go(SEC.ARP, "STYLE");                           // (V0.75: the ARP button's menu)
ex.web_test_chd(4, 1);                          // the ARP on
for (const [st, id, move, feel] of [[0, "PerfArp", 2, 2], [1, "PerfPattern", 4, 60], [2, "PerfHarp", 0, 0]]) {
  ex.web_test_chd(5, st);
  ex.web_test_chd(7, move);
  if (st !== 2) ex.web_test_chd(8, feel);
  run(0.05);
  shot(id);
}
ex.web_test_chd(5, 0);
ex.web_test_chd(4, 0);

// ---- V0.75: the ARP (the keys C4 E4 G4 B4 latched; UP-DOWN over 2 octaves; twelve steps of every kind; GROOVE)
ex.web_test_chd(0, 0);                          // (MODE OFF: the keys themselves, no bass)
ex.web_test_chd(9, 0);
ex.web_test_chd(4, 1);
ex.web_test_chd(5, 0);
ex.web_test_chd(6, 9);                          // 1/16, UP-DOWN, 2 octaves
ex.web_test_chd(7, 2);
ex.web_test_chd(8, 2);
ex.web_test_chd(17, 60);                        // GATE 60 %, SWING 58 %, LATCH, LENGTH 12
ex.web_test_chd(18, 58);
ex.web_test_chd(19, 1);
ex.web_test_chd(20, 12);
for (const [i, kind, lvl, oct, rat, chn, sld] of [[0, 0, 100, 2, 1, 100, 0], [1, 0, 60, 2, 1, 100, 0], [2, 1, 100, 2, 1, 100, 0],
     [3, 0, 80, 3, 2, 100, 0], [4, 2, 100, 2, 1, 100, 0], [5, 0, 45, 2, 1, 50, 0], [6, 0, 100, 1, 1, 100, 1],
     [7, 0, 70, 2, 3, 100, 0], [8, 0, 90, 2, 1, 100, 0], [9, 1, 100, 2, 1, 100, 0], [10, 0, 55, 3, 1, 100, 0],
     [11, 0, 100, 2, 4, 75, 0]])
  [kind, lvl, oct, rat, chn, sld].forEach((v, f) => ex.web_test_step(i, f, v));
run(0.05);                                      // (the settings taken: MODE's change forgets what is held)
ex.web_keys((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4) | (1 << 18));   // (C4 E4 G4 B4, then let go: latched)
run(0.1);
ex.web_keys(0);
run(0.62);
shot("ArpPerform");
go(SEC.STEPS, "STEPS");
turn(0, 3);                                     // step 4 picked: 2 hits an octave up at 80 %
run(0.3);
shot("ArpSteps");
turn(2, -2);                                    // its LEVEL turning
shot("ArpStepsTurn");
rest();
go(SEC.STEPS, "RATCHET");
turn(0, 8);                                     // step 12: 4 hits, CHANCE 75 %
run(0.3);
shot("ArpRatchet");
rest();
go(SEC.ARP, "GROOVE");
run(0.3);
shot("ArpGroove");
turn(1, 6);                                     // SWING to 64 %, turning
shot("ArpGrooveTurn");
rest();
go(SEC.ARP, "STYLE");
ex.web_test_chd(7, 12);                         // CHORD over 3 octaves
ex.web_test_chd(8, 3);
run(0.3);
shot("ArpChord");
ex.web_test_chd(5, 1);                          // PATTERN 7 with GROOVE's swing and gate
ex.web_test_chd(7, 6);
run(0.3);
shot("ArpPattern");
ex.web_test_chd(19, 0);                         // (all back: LATCH off, the steps as they were, the ARP off, MODE KEY)
ex.web_test_chd(20, 16);
ex.web_test_chd(17, 50);
ex.web_test_chd(18, 50);
for (let i = 0; i < 16; i++) [0, 100, 2, 1, 100, 0].forEach((v, f) => ex.web_test_step(i, f, v));
ex.web_test_chd(5, 0);
ex.web_test_chd(4, 0);
ex.web_test_chd(0, 1);
ex.web_test_chd(9, 1);
run(0.4);
go(SEC.CHORD, "BASS");
held(14, 0.1);                                  // G4: G major (C major's), the bass under it
shot("Bass");
ex.web_keys(0);
run(0.3);
go(SEC.CMID, "MIDI");
ex.web_test_chd(12, 1);
ex.web_test_chd(13, 1);
run(0.05);
shot("ChordMidi");
for (const [f, v] of [[0, 0], [2, 0], [9, 0], [12, 0], [13, 0], [16, 0]]) ex.web_test_chd(f, v);   // (the chords off again)
go(SEC.KEYS, "SCALE");
turn(0, -1);
rest();

// ---- the SETTINGS (HOME held 5 s: its countdown first)
go(SEC.MACRO, "SOUND");
ex.web_buttons(1 << B.HOME);
run(2.4);
shot("SettingsHold");                           // (V0.86, B17: the help map, the bar toward the SETTINGS)
ex.web_buttons(0);
run(0.1);
go(SEC.FILTER, "TONE");
ex.web_buttons(1 << B.HOME);
run(1.2);
shot("HelpMap");
ex.web_buttons(0);
run(0.1);
ex.web_test_power(575, 0);                      // (V0.86: the battery 3 of 4 on POWER)
for (const [sec, nm, id] of [[SEC.MENU, "SCREEN", "SetScreen"], [SEC.MENU, "PANEL", "SetPanel"], [SEC.MENU, "MIDI", "SetMidi"],
                             [SEC.DEBUG, "BENCH", "SetBench"], [SEC.DEBUG, "CORE 2", "SetCore2"], [SEC.DEBUG, "KEY SCAN", "SetKeyScan"],
                             [SEC.MENU, "POWER", "SetPower"], [SEC.MENU, "WIPE", "SetWipe"]]) {   // (V0.87: DEBUG, WIPE)
  go(sec, nm);
  shot(id);
}
ex.web_test_c2(4, 1);                           // CORE 2 as a unit shows it after a test (example figures: the
ex.web_test_c2(0, 1);                           // emulator has one core and measures nothing)
ex.web_test_c2(1, 1);
ex.web_test_c2(3, 412);
go(SEC.DEBUG, "BENCH");
go(SEC.DEBUG, "CORE 2");
shot("SetCore2Done");
ex.web_test_c2(4, 0);
ex.web_test_c2(0, 5);
ex.web_test_c2(1, 0);
go(SEC.MENU, "MIDI");
turn(1, 1);                                     // LEARN on
run(1.6);
go(SEC.OSCA, "WAVE");
turn(0, 1);                                     // the knob that picks
rest();
shot("MidiLearn");
ex.web_buttons(1 << B.OCTUP);                   // OCT+: LEARN ends
run(0.05);
ex.web_buttons(0);
run(1.7);

// ---- the layers
go(SEC.FILTER, "TONE");
ex.web_buttons(1 << B.REC);                     // REC held: MOD; F3 picks ENV 1, KNOB 2 its depth on CUTOFF
run(0.7);
ex.web_keys(1 << KEY.F3);
run(0.05);
ex.web_keys(0);
turn(1, 40);
run(0.2);
shot("ModLayer");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.GLO);
run(0.7);
turn(0, 3);
turn(1, 20);
shot("Perform");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.FX);
run(0.7);
shot("Macros");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.LFO);                     // (on FILTER TONE: LFO held is MOMENTARY)
run(0.7);
turn(1, 12);
turn(2, 10);
run(0.2);
shot("Momentary");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.EDIT);
run(0.7);
shot("Jump");
ex.web_buttons(0);
run(0.3);
for (let i = 0; i < 2; i++) { ex.web_buttons(1 << B.EDIT); run(0.03); ex.web_buttons(0); run(0.08); }   // (V0.86:
run(0.3);                                                                                               // twice:
shot("JumpLatched");                                                                                    // latched)
ex.web_buttons(1 << B.EDIT); run(0.03); ex.web_buttons(0); run(0.3);

// ---- SAVE, the browser, NAME IT, undo
go(SEC.FILTER, "TONE");
turn(1, 5);
rest();
ex.web_buttons(1 << B.SAVE);                    // SAVE + OCT-: undo
run(0.05);
ex.web_buttons((1 << B.SAVE) | (1 << B.OCTDN));
run(0.05);
ex.web_buttons(1 << B.SAVE);
run(0.05);
ex.web_buttons(0);
run(0.2);
shot("Undo");
go(SEC.FILTER, "VALUES");                       // (the cutoff modulated: its range, the source's word)
shot("FilterValuesMod");
go(SEC.LFO1, "VALUES");
shot("LfoValues");
run(1.6);
go(SEC.MACRO, "SOUND");
tap(B.SAVE);                                    // SAVE once: a new version, in the first free slot
run(0.6);
shot("Saved");
run(1.6);
ex.web_buttons(1 << B.SAVE);                    // SAVE held: over its own slot, the countdown
run(1.6);
shot("SaveHold");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.SAVE);                    // SAVE twice: the browser
run(0.05);
ex.web_buttons(0);
run(0.1);
ex.web_buttons(1 << B.SAVE);
run(0.05);
ex.web_buttons(0);
run(0.5);
shot("Browser");
ex.web_buttons(1 << B.OCTDN);                   // OCT- held: delete the user preset playing
run(1.7);
shot("BrowserDelete");
run(1.6);
ex.web_buttons(0);
run(0.5);
ex.web_buttons(1 << B.SAVE);                    // SAVE + OCT- straight after: back
run(0.05);
ex.web_buttons((1 << B.SAVE) | (1 << B.OCTDN));
run(0.05);
ex.web_buttons(1 << B.SAVE);
run(0.05);
ex.web_buttons(0);
run(0.5);
shot("Restored");
run(1.6);
tap(B.SAVE);                                    // SAVE once in the browser: NAME IT
run(0.4);
turn(0, -3);
rest();
shot("NameIt");
ex.web_buttons(1 << B.HOME);                    // HOME: back to the browser, HOME again: the page
run(0.05);
ex.web_buttons(0);
run(0.2);
ex.web_buttons(1 << B.HOME);
run(0.05);
ex.web_buttons(0);
run(0.3);

// ---- the system's
go(SEC.LFO1, "POINTS");
turn(0, 1);
rest();
ex.web_buttons(1 << B.LFO);                     // LFO held 5 s (V0.75): the LFO reset, a countdown from 2 s first
run(3.0);
shot("LfoReset");
ex.web_buttons(0);
run(0.3);
ex.web_test_screen(4);
run(0.1);
shot("Uboot");
ex.web_test_screen(0);
run(0.1);
ex.web_test_screen(1);
run(0.1);
shot("SafeStart");
tap(B.HOME);
go(SEC.MACRO, "SOUND");
ex.web_keys((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4));
run(0.6);
shot("Main");
ex.web_test_palette(4);                         // PAPER
run(0.2);
shot("Main-paper");
ex.web_keys(0);
run(0.3);
go(SEC.OSCA, "WAVE");
shot("Osc-paper");
go(SEC.FILTER, "TONE");
shot("Filter-paper");
for (const [i, nm] of [[8, "moss"], [9, "dusk"], [10, "sand"], [11, "neon"]]) {   // V0.67's four themes
  ex.web_test_palette(i);
  go(SEC.MACRO, "SOUND");
  ex.web_keys((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4));
  run(0.6);
  shot(`Main-${nm}`);
  ex.web_keys(0);
  run(0.3);
  go(SEC.FILTER, "TONE");
  shot(`Filter-${nm}`);
}
ex.web_test_palette(0);

// ---- V0.86: HOME's MIX pages, PRESETS' list (and its guard on an edited sound)
go(SEC.MACRO, "MIX 1-4");
run(0.1);
shot("HomeMix");
go(SEC.MACRO, "SOUND");
run(0.1);
ex.web_enc(2, 1);                               // (PRESETS: the list, one on)
run(0.15);
shot("PresetsList");
run(2.5);
go(SEC.FILTER, "TONE");
turn(1, 3);
rest();
go(SEC.MACRO, "SOUND");
run(0.1);
shot("HomeEdited");                             // (the unsaved dot)
ex.web_enc(2, 1);                               // (edited: the first detent opens it alone)
run(0.15);
shot("PresetsGuard");                           // (V0.86: SAVE twice would leave the edits: said so)
ex.web_enc(2, 99);                              // (the last user preset; SAVE + OCT-: asked first)
run(0.1);
ex.web_buttons(1 << B.SAVE); run(0.05); ex.web_buttons((1 << B.SAVE) | (1 << B.OCTDN)); run(0.03);
ex.web_buttons(1 << B.SAVE); run(0.03); ex.web_buttons(0); run(0.1);
shot("PresetsDelete");
run(3.2);

// ---- V0.8: the looper (a project: T1 chords, T2 a MIDI bass line, T3 an arpeggio with the cutoff moving, clips in
// other scenes; SONG A A B C)
{
  const lp = (f, v = -1) => ex.web_test_lp(f, v), tr = (t, f, v = -1) => ex.web_test_lp_trk(t, f, v);
  const clip = (t, s, bars) => ex.web_test_lp_clip(t, s, 0, bars * 384);
  const put = (t, s, tick, len, notes, vel = 100) => notes.forEach((n, i) => ex.web_test_lp_put(t, s, tick, len, n, vel - 6 * i));
  ex.web_test_chd(0, 0);                        // (the chords off, no bass: the notes as they are)
  ex.web_test_chd(9, 0);
  clip(0, 0, 2);                                // T1 A: four chords, half a bar each
  [[0, [60, 64, 67]], [192, [57, 60, 64]], [384, [53, 57, 60]], [576, [55, 59, 62]]].forEach(([t, ns]) => put(0, 0, t, 180, ns, 96));
  clip(0, 1, 1);
  put(0, 1, 0, 360, [60, 63, 67], 90);
  tr(1, 0, 1); tr(1, 1, 1);                     // T2 MIDI, channel 2: a bass line
  clip(1, 0, 2);
  [[0, 36], [96, 36], [192, 33], [288, 33], [384, 29], [480, 29], [576, 31], [672, 43]].forEach(([t, n], i) => put(1, 0, t, 72, [n], 110 - 4 * i));
  clip(1, 2, 1);
  put(1, 2, 0, 90, [36], 100);
  put(1, 2, 192, 90, [39], 90);
  clip(2, 0, 1);                                // T3 A: an arpeggio in sixteenths, the cutoff moving under it
  for (let i = 0; i < 16; i++) put(2, 0, 24 * i, 18, [[72, 76, 79, 84][i % 4] - (i >= 8 ? 2 : 0)], 70 + (i % 4 === 0 ? 30 : 0));
  go(SEC.FILTER, "TONE");
  const cut = ex.web_test_page_param(1);
  for (let i = 0; i < 12; i++) ex.web_test_lp_mput(2, 0, 32 * i, cut, Math.round(300 + 600 * Math.sin(i / 11 * Math.PI)));
  clip(2, 1, 1);
  put(2, 1, 0, 48, [79], 90); put(2, 1, 96, 48, [77], 80); put(2, 1, 192, 96, [76], 90);
  clip(3, 2, 2);
  put(3, 2, 0, 700, [48, 55], 80);
  clip(4, 1, 1); put(4, 1, 48, 24, [84], 60);
  clip(5, 3, 1); put(5, 3, 0, 96, [67], 80);
  clip(6, 0, 1); put(6, 0, 0, 24, [91], 50); put(6, 0, 192, 24, [91], 50);
  clip(7, 4, 1); put(7, 4, 0, 384, [43], 70);
  tr(1, 6, 0); tr(4, 4, 70); tr(5, 4, 85); tr(5, 5, 30); tr(6, 6, 1); tr(2, 4, 90); tr(2, 5, 90); tr(7, 4, 110);
  [[0, 0, 2], [1, 1, 1], [2, 2, 2], [3, 255, 1]].forEach(([i, sc, r]) => { ex.web_test_lp_song(i, 0, sc); ex.web_test_lp_song(i, 1, r); });
  lp(0, 0); lp(1, 0);
  go(SEC.SEQ, "CLIPS");
  run(0.1);
  shot("LoopClips");
  for (const t of [0, 1, 2, 6]) ex.web_test_lp_launch(t, 0);   // scene A's clips, from the top
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(2.6);                                     // (playing: bar 2; T3's B launched, T7 stopping, both at the bar)
  ex.web_test_lp_launch(2, 1);
  ex.web_test_lp_launch(6, 254);
  lp(0, 2); lp(1, 0);
  run(0.1);
  shot("LoopClipsPlay");
  lp(0, 4); lp(1, 2);                           // REC on T5's empty clip in C: from the next bar, the take
  ex.web_test_lp_req(1);
  run(1.6);
  shot("LoopClipsRec");
  ex.web_keys(1 << 7); run(0.2); ex.web_keys(0); run(0.4);
  ex.web_test_lp_req(1);                        // (REC again: the take ends, a bar)
  run(0.3);
  lp(0, 1);
  go(SEC.SEQ, "MIXER");
  run(0.1);
  shot("LoopMixer");
  turn(0, -3);                                  // T2's VOLUME turning (V0.86: KNOB 1)
  shot("LoopMixerTurn");
  rest();
  go(SEC.SEQ, "CLIPS");                         // (V0.86, C6: TYPE turned, waiting for the turning to stop)
  turn(0, 1);
  shot("LoopType");
  run(0.6);
  shot("LoopTypeAsk");
  run(2.2);
  lp(2, 2);
  go(SEC.LTRK, "RECORD");
  run(0.1);
  shot("LoopRecord");
  lp(2, 0);
  go(SEC.LTRK, "CLICK");
  run(0.25);
  shot("LoopClick");
  lp(0, 0); lp(1, 0);
  go(SEC.LCLIP, "NOTES");
  turn(0, 4);                                   // the fifth note picked (the second chord's lowest)
  rest();
  shot("LoopNotes");
  turn(1, 2);                                   // PITCH turning
  shot("LoopNotesTurn");
  turn(1, -2);
  rest();
  go(SEC.LCLIP, "NOTE+");
  run(0.1);
  shot("LoopEdit");
  lp(0, 2); lp(1, 0);
  go(SEC.LCLIP, "CLIP");
  run(0.1);
  shot("LoopClip");
  turn(1, 1);                                   // (V0.86, S8: SPEED x2)
  shot("LoopClipSpeed");
  turn(1, -1);
  rest();
  lp(0, 0); lp(1, 0);
  go(SEC.LCLIP, "TOOLS");
  run(0.1);
  turn(0, 1);                                   // COPY: T1 A
  rest();
  lp(1, 1);
  run(0.1);
  turn(1, 1);                                   // PASTE into T1 B: asked
  shot("LoopTools");
  run(2.2);
  lp(0, 2); lp(1, 0);
  lp(8, 1);
  go(SEC.SEQ, "SONG");                          // (V0.87, U21: SONG in LOOP)
  run(0.1);
  shot("LoopSong");
  lp(8, 0);
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(3.8);                                     // (stopped: 3 s of quiet, the project kept)
  for (let i = 0; i < 2; i++) { ex.web_buttons(1 << 9); run(0.03); ex.web_buttons(0); run(0.08); }   // (V0.87, S2: SEQ
  run(0.3);                                     // twice, the projects' list)
  shot("LoopProjects");
  run(5.2);
  lp(0, 2); lp(1, 0);
  for (const t of [0, 1, 2]) ex.web_test_lp_launch(t, 0);
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(0.6);
  ex.web_buttons(1 << 9);                       // the launch pad: SEQ held
  run(0.8);
  shot("LoopPad");
  ex.web_buttons(0);
  run(0.2);
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(0.3);
  go(SEC.MACRO, "SOUND");                       // (V0.87: SELECT on HOME, the next track, its word)
  ex.web_enc(E.SELECT, 1); run(0.2);
  shot("LoopNextTrack");
  rest();
}
{                                               // ---- V0.85, the drums: T8 DRUMS, a beat on its grid, the kit's pages
  const lp = (f, v = -1) => ex.web_test_lp(f, v), tr = (t, f, v = -1) => ex.web_test_lp_trk(t, f, v);
  const g = (row, steps, vel = 100) => steps.forEach((st, i) => ex.web_test_grid(0, row, st, Array.isArray(vel) ? vel[i] : vel));
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0); run(0.2);   // (stopped)
  if (lp(15)) { ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0); run(0.2); }
  lp(0, 7); lp(1, 0);
  tr(7, 0, 2);
  go(SEC.SEQ, "CLIPS");
  run(0.1);
  shot("DrumTrack");
  g(0, [0, 4, 8, 10, 12], [120, 100, 110, 70, 100]);   // KICK
  g(1, [4, 12], [110, 120]);                    // SNARE
  ex.web_test_grid(2, 1, 12, 1);                // (its second accented)
  g(2, [12, 15], [90, 60]);                     // CLAP
  g(3, [0, 2, 4, 6, 8, 10, 12], [90, 60, 85, 55, 90, 60, 85]);   // HAT CL
  ex.web_test_grid(3, 3, 6, 2);                 // (one at 75 %)
  g(4, [14], 80);                               // HAT OP
  g(5, [0], 70);                                // CYM
  g(6, [3, 11], [90, 80]);                      // WT 1
  g(7, [7], 85);                                // WT 2
  ex.web_test_grid(4, 7, 7, 2);                 // (a ratchet of 3)
  g(8, [0, 4, 8, 12]);                          // DUCK
  ex.web_test_lp_launch(7, 0);
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(1.37);
  go(SEC.LGRID, "GRID");
  turn(1, 1);                                   // (the row: SNARE, its step 5)
  turn(0, 4);
  rest();
  shot("DrumGrid");
  turn(2, -8);                                  // VELOCITY turning
  shot("DrumGridTurn");
  rest();
  ex.web_keys(1 << 9);                          // (V0.86, S7: step 6 held, CHANCE turning)
  run(0.1);
  turn(2, 2);
  shot("DrumGridHeld");
  ex.web_keys(0);
  run(0.1);
  rest();
  go(SEC.LGRID, "HIT");
  turn(1, 6);                                   // (WT 2's ratcheted step)
  turn(0, 3);
  rest();
  shot("DrumHit");
  ex.web_buttons(1 << B.PLAY); run(0.03); ex.web_buttons(0);
  run(0.3);
  go(52, "SOUND");                              // (V0.86 step 2: DRUM, the drum PRESETS picks; KIT, the kit's)
  run(0.1);
  shot("DrumKit");
  ex.web_enc(2, 5);                             // (PRESETS: CYM, a long fall)
  rest();
  shot("DrumKitCym");
  turn(1, 4);                                   // DECAY turning
  shot("DrumKitTurn");
  rest();
  ex.web_enc(2, -5);                            // (back to the KICK)
  run(0.1);
  go(52, "DRIVE");
  turn(1, 25);                                  // (DRIVE up: SOFT, +6 dB)
  rest();
  shot("DrumDrive");
  go(52, "EQ");
  turn(0, 15); turn(1, -10); turn(2, 8);        // (LOW up, MID down, HIGH up)
  rest();
  shot("DrumEq");
  go(52, "SHAPE");
  turn(0, 20); turn(1, -15);                    // (ATTACK up, SUSTAIN down)
  rest();
  shot("DrumShape");
  go(52, "MIX");
  run(0.1);
  shot("DrumMore");
  go(SEC.KIT, "ACCENT");
  turn(2, 40);                                  // (TONE: soft hits darker)
  rest();
  shot("DrumAccent");
  go(SEC.KIT, "DUCK");
  run(0.1);
  shot("DrumDuck");
  turn(0, 1);                                   // SOURCE turning: KICK
  shot("DrumDuckTurn");
  rest();
  go(SEC.KIT, "CURVE");
  turn(1, 16);                                  // (HOLD 40 MS)
  rest();
  shot("DrumCurve");
  go(SEC.KIT, "COMP");
  turn(0, 50); turn(3, -20);                    // (AMOUNT 50 %, MIX 80 %)
  rest();
  shot("DrumComp");
  go(SEC.KIT, "COMP");                          // (V0.87, U6: EDIT twice on a drum track: the kits)
  run(0.1);
  twice(B.EDIT);
  ex.web_enc(2, 1);
  run(0.1);
  shot("DrumKits");
  run(5.2);
  go(SEC.MACRO, "SOUND");                       // (V0.86, D1-D3: HOME on a drum track, its drums lit as they sound)
  for (const l of [0, 3, 1]) ex.web_test_drum_hit(l, 7, 110, 0);
  run(0.12);
  shot("DrumHome");
}
{                                               // V0.87: the windows, the lists, the questions, the reset's countdown
  ex.web_test_lp(0, 0);
  ex.web_test_lp(1, 0);
  go(SEC.OSCA, "WAVE");
  run(0.1);
  twice(B.LFO);
  shot("AssignLfo");
  ex.web_enc(E.ALGO, 1); run(0.1);
  turn(1, 6); rest();
  shot("AssignLfo2");
  tap(B.LFO);
  go(SEC.FILTER, "TONE");
  run(0.1);
  twice(B.ENV);
  shot("AssignEnv");
  tap(B.ENV);
  go(SEC.OSCA, "WAVE");
  tap(B.SAVE); run(0.4);
  shot("SaveWindow");
  tap(B.HOME);
  go(SEC.OSCA, "WAVE");
  twice(B.EDIT);
  shot("TablesList");
  ex.web_enc(E.SELECT, 1); run(0.1);
  shot("TablesCategory");
  run(5.2);
  ex.web_enc(E.PRESET, 1); run(0.1);
  shot("PresetsList");
  run(5.2);
  ex.web_buttons(1 << B.EDIT); run(0.05); ex.web_buttons((1 << B.EDIT) | (1 << B.OCTDN)); run(3.0);
  shot("ResetCountdown");
  ex.web_buttons(1 << B.EDIT); run(0.05); ex.web_buttons(0); run(0.4);
  ex.web_test_lp_put(0, 0, 0, 48, 60, 100);
  go(SEC.SEQ, "CLIPS");
  run(0.1);
  shot("LoopClips87");
  ex.web_buttons(1 << B.SEQ); run(0.03); ex.web_buttons((1 << B.SEQ) | (1 << B.OCTDN)); run(0.03);
  ex.web_buttons(1 << B.SEQ); run(0.03); ex.web_buttons(0); run(0.35);
  shot("AskDelete");
  tap(B.HOME);
  go(SEC.SEQ, "CLIPS");
  turn(0, 3); run(0.6);
  shot("AskType");
  tap(B.EDIT);
  go(SEC.MENU, "WIPE");
  turn(0, 1); run(0.1);
  shot("AskWipe");
  tap(B.HOME);
  go(SEC.SEQ, "CLIPS");
  ex.web_test_lp(1, 1);                         // (scene B: empty)
  ex.web_buttons(1 << B.SEQ); run(0.03); ex.web_buttons((1 << B.SEQ) | (1 << B.OCTUP)); run(0.03);
  ex.web_buttons(1 << B.SEQ); run(0.03);
  ex.web_enc(E.PRESET, 1); run(0.1);
  shot("NewClip");
  ex.web_buttons(0); run(0.4);
}
console.log(`shots ${shots.length}: ${out}; the check: ${lints.length ? lints.length + " with findings" : "clean"}`);
writeFileSync(`${out}/lint.txt`, lints.join("\n"));
writeFileSync(`${out}/shots.json`, JSON.stringify(shots));
