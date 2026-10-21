// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 u4ia
// STRATA's screen on the simulated FM-1 as PNG files, to look at without a browser (twice the panel's size). A short
// session, played as the panel would: the wave page (VOWELS, BEND+), REC held (LFO 1 picked by its white key, KNOB 1
// turned: its depth on WT POS), the unison page, an envelope with a key held, LFO 1's POINTS page (a point lowered,
// one added, a key held: the voice on its cycle), its SHAPE page with S&H (five cycles of sample levels), HOME with a
// chord (the scope), the wave page's AMOUNT turning, the filter's pages and each effect's, a knob turning on each,
// the keys' scale (SCL), the matrix (GLO), the FX chain and its new modules, the hold layers (PERFORM, MACROS, JUMP, MOMENTARY), the SETTINGS (SCREEN,
// PANEL, MIDI with a CC learned; LEARN's footer; a HELP hint) (HOME held 5 s), SAVE (once: a new version; held: the countdown), the browser (SAVE twice; all, PAD),
// NAME IT from it (a rename, the numbers, written), HOME with a user preset, and the wave page in each palette. It
// prints what the session set.
//   node web/emu/strata_shot.mjs [build/emu_strata/strata.wasm] [build/emu_strata/shots]   (after build_strata.sh)
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

const wasm = process.argv[2] || "build/emu_strata/strata.wasm", out = process.argv[3] || "build/emu_strata/shots";
const { instance } = await WebAssembly.instantiate(readFileSync(wasm), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_nor_erase();
ex.web_master(700);
ex.web_boot();
ex.web_test_set_get(0, 0);                      // (HELP's hints off: one shot below shows one)
const run = (s) => { for (let n = Math.max(1, Math.round(s * 44118 / 128)); n > 0; n--) ex.web_render(128); };
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, REC: 11, OCTDN: 12, OCTUP: 13 };   // panel.c B_*
const E = { SELECT: 0, ALGO: 1, PRESET: 2, K1: 3, K2: 4, K3: 5, K4: 6 };                    // panel.c EN_*
const tap = (b) => { ex.web_buttons(1 << b); run(0.05); ex.web_buttons(0); run(0.35); };   // (two within 300 ms latch)

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
function png(name, scale = 2) {                 // the frame buffer: RGB565, high byte first (the panel's order)
  const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240), W = 240 * scale;
  const raw = Buffer.alloc((W * 3 + 1) * W);
  for (let y = 0; y < W; y++) {
    for (let x = 0; x < W; x++) {
      const v = fb[Math.floor(y / scale) * 240 + Math.floor(x / scale)], c = ((v & 0xff) << 8) | (v >> 8);
      const r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31, o = y * (W * 3 + 1) + 1 + x * 3;
      raw[o] = (r << 3) | (r >> 2);
      raw[o + 1] = (g << 2) | (g >> 4);
      raw[o + 2] = (b << 3) | (b >> 2);
    }
  }
  const hdr = Buffer.alloc(13);
  hdr.writeUInt32BE(W, 0);
  hdr.writeUInt32BE(W, 4);
  hdr[8] = 8;                                   // 8 bits a channel, RGB
  hdr[9] = 2;
  writeFileSync(`${out}/${name}`, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", hdr),
                                                 chunk("IDAT", deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]));
  console.log(`shot: ${out}/${name}`);
}

mkdirSync(out, { recursive: true });
run(0.6);
png("boot.png");
const [POS, TABLE, WARP, WAMT] = [0, 1, 2, 3].map((k) => ex.web_test_page_param(k));
ex.web_test_set(TABLE, 27);                     // VOWELS
ex.web_test_set(POS, 375);
ex.web_test_set(WARP, 2);                       // BEND+
ex.web_test_set(WAMT, 300);
run(0.4);                                       // (the table builds)
png("wave.png");
ex.web_buttons(1 << B.REC);                     // REC held: the MOD layer
run(0.6);
ex.web_keys(1 << 4);                            // A3, a white key: LFO 1
run(0.05);
ex.web_keys(0);
for (let i = 0; i < 4; i++) { ex.web_enc(E.K1, 1); run(0.004); }   // a quick turn of KNOB 1
run(0.1);
png("mod.png");
console.log(`mod layer ${ex.web_test_mod()}, source ${ex.web_test_mod_src()}, slot 1: src ${ex.web_test_slot(0, 0)} dst ${ex.web_test_slot(0, 1)} amt ${ex.web_test_slot(0, 2)}`);
ex.web_buttons(0);
run(0.1);
png("wave_mod.png");
tap(B.EDIT);                                    // OSC A UNISON
run(0.1);
png("unison.png");
tap(B.ENV);                                     // ENV 1
ex.web_keys(1 << 7);
run(0.3);
png("env.png");
ex.web_keys(0);
run(0.2);
tap(B.ENV);
tap(B.ENV);                                     // ENV 2 (its default decays to nothing)
ex.web_keys(1 << 7);
run(0.15);
png("env2.png");
ex.web_keys(0);
run(0.2);
tap(B.LFO);                                     // LFO 1 POINTS: the sine's points, point 2 picked
ex.web_enc(E.K1, 1);
run(0.1);
png("lfo_points.png");
for (let i = 0; i < 12; i++) { ex.web_enc(E.K3, -1); run(0.004); }   // lowered: drawn from the sine
run(0.1);
ex.web_enc(E.K1, 1);                            // point 3, then LFO twice: a point left of it
run(0.05);
for (let i = 0; i < 2; i++) { ex.web_buttons(1 << B.LFO); run(0.03); ex.web_buttons(0); run(0.08); }
for (let i = 0; i < 3; i++) { ex.web_enc(E.K4, 1); run(0.004); }    // its curve
ex.web_keys(1 << 7);
run(0.35);
png("lfo_drawn.png");
ex.web_keys(0);
console.log(`LFO 1: ${ex.web_test_draw(0, 0, 0)} points, point ${ex.web_test_lpt(0) + 1} picked`);
run(0.2);
ex.web_enc(E.ALGO, 1);                        // its SHAPE page: S&H
run(0.05);
ex.web_enc(E.K1, 5 - ex.web_test_value(ex.web_test_page_param(0)));
run(0.1);
png("lfo_sh.png");
ex.web_enc(E.K1, 7);                            // (back to DRAWN)
run(0.1);
ex.web_enc(E.K2, 1);                            // SYNC FIXED: RATE a note value (1/8)
run(0.05);
ex.web_enc(E.K3, 3);
ex.web_keys(1 << 7);
run(0.3);
png("lfo_fixed.png");
ex.web_keys(0);
run(0.2);
ex.web_enc(E.ALGO, -1);                       // POINTS: LFO held, the delete's countdown
run(0.05);
ex.web_buttons(1 << B.LFO);
run(1.5);
png("lfo_delete.png");
ex.web_buttons(0);
run(0.1);
const cstr = (q) => { const m = new Uint8Array(ex.memory.buffer); let e = q; while (m[e]) e++; return String.fromCharCode(...m.subarray(q, e)); };
const go = (sec, nm) => {                       // a section's page by its name, at once
  for (let q = 0; q < 200; q++) { ex.web_test_go(q); if (ex.web_test_page_section() === sec && cstr(ex.web_test_page_name()) === nm) return; }
};
go(38, "TEMPO");                                // GLOBAL's TEMPO, 6/8 at 132
for (let i = 0; i < 12; i++) { ex.web_enc(E.K1, 1); run(0.06); }
ex.web_enc(E.K2, 2);
ex.web_enc(E.K3, 1);
run(0.1);
png("tempo.png");
console.log(`GLOBAL: ${ex.web_test_tempo(0)} BPM ${ex.web_test_tempo(1)}/${ex.web_test_tempo(2)}`);
const sect = () => ex.web_test_page_section();
const to = (sec) => { ex.web_enc(E.SELECT, sec - sect()); run(0.05); };   // SELECT: a section of the family
const turn = (k, n, dt = 0.12) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(E.K1 + k, n > 0 ? 1 : -1); run(dt); } };
const set = (id, v) => ex.web_test_set(id, v);
tap(B.HOME);                                    // HOME: a chord playing, the scope live
ex.web_keys((1 << 7) | (1 << 11) | (1 << 14));
run(0.6);
png("home.png");
ex.web_keys(0);
run(0.5);
tap(B.EDIT);                                    // OSC A WAVE: AMOUNT turning (BEND+ 30 % -> more)
ex.web_enc(E.ALGO, -1);
run(0.05);
turn(3, 4);
png("wave_warp.png");
run(1.2);
tap(B.EDIT);
to(4);                                          // FILTER TONE: LP 24, ENV 2 on the cutoff, CUTOFF turning
const F = ex.web_test_page_param(1);            // (SP_F_CUT; ON just before it in the table)
set(F - 2, 1);
set(F - 1, 1);
set(F + 1, 320);
set(106, 2); set(107, F); set(108, 250);        // slot 1: ENV 2 -> CUTOFF +25 %
run(0.1);
turn(1, 4);
png("filter_tone.png");
run(1.2);
ex.web_enc(E.ALGO, 1);                        // MORE: KEYTRACK turning
run(0.05);
turn(0, 6, 0.004);
png("filter_more.png");
run(1.2);
set(106, 0); set(107, 0); set(108, 0);
for (let k = 0; k < 8; k++) set(180 + k, k < 6 ? k + 1 : 0);   // the chain: the six of before, in their order
go(12, "DRIVE");                                // DRIVE: TUBE, DRIVE turning
set(130, 1); set(131, 1);
run(0.05);
turn(1, 4);
png("drive.png");
run(1.2);
go(13, "CHORUS");                                         // CHORUS: DEPTH turning, its taps moving
set(135, 1);
run(0.3);
turn(1, 4);
png("chorus.png");
run(1.2);
go(14, "DELAY");                                         // DELAY: PING-PONG 1/8D, FEEDBACK turning
set(141, 1);
run(0.05);
turn(3, 4);
png("delay.png");
run(1.2);
ex.web_enc(E.ALGO, 1);                        // its TONE page: LP turning
run(0.05);
turn(0, -5, 0.004);
png("delay_tone.png");
run(1.2);
go(15, "REVERB");                                         // REVERB: HALL, DECAY turning
set(149, 1);
run(0.05);
turn(2, 4);
png("reverb.png");
run(1.2);
go(16, "OTT");                                         // OTT: a chord through it, DEPTH turning
set(158, 1);
ex.web_keys((1 << 2) | (1 << 9) | (1 << 14));
run(0.8);
turn(0, -6, 0.004);
png("ott.png");
ex.web_keys(0);
run(1.2);
go(17, "POINTS");                                         // EQ POINTS: MID picked, its GAIN turning; LFO 1 on MID's gain
set(169, 1); set(170, 60); set(171, -40); set(172, 30);
set(106, 3); set(107, 171); set(108, 250);      // slot 1: LFO 1 -> MID, +-25 %
ex.web_enc(E.K1, 1);
run(0.1);
turn(2, -3);
png("eq.png");
run(1.2);
ex.web_keys(1 << 7);                            // a note: the curve and MID's point where LFO 1 has them
run(0.45);
png("eq_live.png");
ex.web_keys(0);
run(1.5);
ex.web_enc(E.ALGO, 1);                        // GAINS: LOW turning
run(0.05);
turn(0, 4);
png("eq_gains.png");
run(1.2);
set(106, 0); set(107, 0); set(108, 0);
for (const id of [130, 135, 141, 149, 158, 169]) set(id, 0);
for (let k = 0; k < 8; k++) set(180 + k, [2, 15, 13, 23, 4][k] || 0);   // the chain: CHORUS, FILTER, PHASER, TAPE, REVERB
for (const id of [135, 149, 256, 236, 300]) set(id, 1);
tap(B.FX);                                      // FX: the chain view (TAPE picked to add)
ex.web_enc(E.K1, 12 - ex.web_test_ctype());
ex.web_enc(E.K2, 3 - ex.web_test_cslot());
run(0.3);
png("fx_chain.png");
for (const [sec, nm, f] of [[22, "FLANGER", "flanger.png"], [24, "PHASER", "phaser.png"], [26, "FILTER", "fx_filter.png"],
                            [28, "CRUSH", "bitcrush.png"], [30, "TREMOLO", "tremolo.png"], [32, "RING MOD", "ring.png"],
                            [34, "TAPE", "tape.png"]]) {
  go(sec, nm);
  run(0.3);
  png(f);
}
for (const id of [135, 149, 256, 236, 300]) set(id, 0);
for (let k = 0; k < 8; k++) set(180 + k, 0);
tap(1);                                         // SCL: D dorian, SNAP (a key off the scale plays the note below)
ex.web_enc(E.K1, 3);
run(0.12);
ex.web_enc(E.K2, 2);
run(0.12);
ex.web_enc(E.K3, 1);
run(0.12);
png("keys.png");
ex.web_enc(E.K3, 1);                            // WHITE: the white keys walk the scale
run(0.12);
png("keys_white.png");
ex.web_enc(E.K1, -3);                           // (back: chromatic, C, OFF)
run(0.12);
ex.web_enc(E.K2, -2);
run(0.12);
ex.web_enc(E.K3, -2);
run(0.12);
go(11, "SLOTS");                                // GLO's MATRIX; slot 2 made: ENV 2, negative, onto OSC B's FINE
ex.web_enc(E.K1, 1);
run(0.12);
ex.web_enc(E.K2, 2);
run(0.12);
for (let i = 0; i < 14; i++) { ex.web_enc(E.K3, 1); run(0.13); }
for (let i = 0; i < 5; i++) { ex.web_enc(E.K4, -1); run(0.004); }
run(0.1);
png("matrix.png");
ex.web_buttons(1 << B.GLO);                     // the hold layers: GLO, PERFORM (a bend up, the mod wheel)
run(0.6);
for (let i = 0; i < 6; i++) { ex.web_enc(E.K1, 1); run(0.02); }
for (let i = 0; i < 6; i++) { ex.web_enc(E.K2, 1); run(0.02); }
run(0.1);
png("perform.png");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.FX);                      // FX, MACROS
run(0.6);
png("macros.png");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.EDIT);                    // EDIT, JUMP
run(0.6);
png("jump.png");
ex.web_buttons(0);
run(0.3);
tap(B.EDIT);                                    // LFO on an OSC A page, MOMENTARY (KNOB 1 turned)
ex.web_buttons(1 << B.LFO);
run(0.6);
ex.web_enc(E.K1, 2);
run(0.13);
png("momentary.png");
ex.web_buttons(0);
run(0.3);
ex.web_buttons(1 << B.HOME);                    // HOME held 5 s: the SETTINGS
run(5.2);
ex.web_buttons(0);
run(0.1);
png("settings.png");
ex.web_enc(E.ALGO, 1);                        // PANEL; MIDI with LEARN on, a knob picked, CC 74 learned to it
run(0.1);
png("settings_panel.png");
ex.web_enc(E.ALGO, 1);
run(0.1);
ex.web_enc(E.K2, 1);
run(0.13);
tap(B.EDIT);
ex.web_enc(E.K1, 1);
run(0.13);
png("learn.png");
ex.web_midi(0xB0, 74, 64);
run(0.05);
ex.web_buttons(1 << B.HOME);
run(5.2);
ex.web_buttons(0);
run(0.1);
ex.web_enc(E.ALGO, 2);
run(0.1);
png("settings_midi.png");
ex.web_enc(E.K2, -1);                           // (LEARN off)
run(0.13);
ex.web_test_set_get(0, 1);                      // HELP on: a section's hint as it opens
tap(B.FX);
png("help.png");
ex.web_test_set_get(0, 0);
run(2.1);
const btn = (b, t = 0.03) => { ex.web_buttons(1 << b); run(t); ex.web_buttons(0); run(0.03); };
const key = (k) => { ex.web_keys(1 << k); run(0.03); ex.web_keys(0); run(0.03); };
tap(B.EDIT);                                    // SAVE once: a new preset at once (its next version, USER 1)
btn(B.SAVE);
run(0.5);
png("save_new.png");
ex.web_buttons(1 << B.SAVE);                    // held: the countdown to saving over USER 1 (let go: nothing)
run(2.0);
png("save_hold.png");
ex.web_buttons(0);
run(0.4);
btn(B.SAVE);                                    // twice: the browser
btn(B.SAVE);
run(0.3);
png("browser.png");
btn(B.SAVE);                                    // SAVE there: NAME IT for the sound playing (WARM PAD), the numbers
run(0.5);
for (let i = 0; i < 16; i++) btn(B.OCTDN);
for (const k of [22, 0, 17, 12, 26, 15, 0]) key(k);
ex.web_keys(1 << 3);                            // (D held: its key lit)
run(0.05);
png("save_name.png");
ex.web_keys(0);
run(0.05);
btn(B.OCTUP);
ex.web_keys(1 << 1);                            // the numbers: F#3 is 2
run(0.05);
png("save_symbols.png");
ex.web_keys(0);
run(0.05);
btn(B.OCTDN);
btn(B.OCTUP);
btn(B.SAVE);                                    // written: USER 1 renamed, back in the browser
run(0.4);
png("save_done.png");
ex.web_enc(E.K1, 3);                            // PAD
run(0.1);
png("browser_pad.png");
btn(B.HOME);
run(0.1);
tap(B.HOME);                                    // HOME: the sound now USER 1
run(0.2);
png("home_user.png");
tap(B.EDIT);                                    // back to OSC A (its WAVE page)
to(0);
run(0.1);
for (let p = 0; p < 8; p++) {
  ex.web_test_palette(p);
  run(0.05);
  png(`pal${p}.png`);
}
ex.web_test_palette(0);
