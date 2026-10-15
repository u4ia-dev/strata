// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// STRATA's screen on the simulated FM-1 as PNG files, to look at without a browser (twice the panel's size): after
// boot (OSC A, the first sound), and on the FX family's first page with a key held and two knobs turned.
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
const run = (s) => { for (let n = Math.round(s * 44118 / 128); n > 0; n--) ex.web_render(128); };

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
ex.web_keys(1 << 7);                            // key 7 (C4) held
ex.web_buttons(1 << 0);                         // FX (panel.c B_FX): DRIVE
run(0.05);
ex.web_buttons(0);
ex.web_enc(3, 20);                              // KNOB 1: DRIVE's ON (already on)
ex.web_enc(4, 1);                               // KNOB 2: its TYPE, one step
run(0.6);
png("fx.png");
