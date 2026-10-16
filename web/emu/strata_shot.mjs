// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// STRATA's screen on the simulated FM-1 as PNG files, to look at without a browser (twice the panel's size). A short
// session, played as the panel would: the wave page (VOWELS, BEND+), REC held (LFO 1 picked by its white key, KNOB 1
// turned: its depth on WT POS), the unison page, an envelope with a key held, LFO 1's POINTS page (a point lowered,
// one added, a key held: the voice on its cycle), its SHAPE page with S&H (five cycles of sample levels), the MENU,
// and the wave page in each palette. It prints what the session set.
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
ex.web_enc(E.SELECT, 1);                        // its SHAPE page: S&H
run(0.05);
ex.web_enc(E.K1, 5 - ex.web_test_value(ex.web_test_page_param(0)));
run(0.1);
png("lfo_sh.png");
ex.web_enc(E.K1, 7);                            // (back to DRAWN)
run(0.1);
ex.web_buttons(1 << B.HOME);                    // HOME held: the MENU
run(0.6);
ex.web_buttons(0);
run(0.1);
png("menu.png");
tap(B.EDIT);                                    // back to OSC A
run(0.1);
for (let p = 0; p < 7; p++) {
  ex.web_test_palette(p);
  run(0.05);
  png(`pal${p}.png`);
}
ex.web_test_palette(0);
