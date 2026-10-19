// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// HOME's bloom on the simulated FM-1 over time, as one sheet: a note's swirl as it starts and unwinds, the bloom
// opening, a second note higher (the other way), the release closing it to a bud; a saw (the starter sound) and a
// single sine-like voice. Each frame the bloom's box at 3x, its time under it.
//   node web/emu/strata_bloom_study.mjs [build/emu_strata/strata.wasm] [build/emu_strata/bloom_study.png]
import { readFileSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

const wasm = process.argv[2] || "build/emu_strata/strata.wasm", out = process.argv[3] || "build/emu_strata/bloom_study.png";
const { instance } = await WebAssembly.instantiate(readFileSync(wasm), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_nor_erase();
ex.web_master(700);
ex.web_boot();
ex.web_test_set_get(0, 0);
const run = (s) => { for (let n = Math.max(1, Math.round(s * 44118 / 128)); n > 0; n--) ex.web_render(128); };
const cstr = (p) => { const m = new Uint8Array(ex.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
const KEY = { C4: 7, E4: 11, G4: 14, C5: 19 };
run(0.6);
for (let p = 0, n = ex.web_test_go(0); p < n; p++) {     // HOME
  ex.web_test_go(p);
  if (ex.web_test_page_section() === 10) break;
}
run(1.0);

const X0 = 110, Y0 = 60, W = 112, S = 3, CW = W * S, LAB = 14;
const frames = [];
function grab(label) {
  const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240), px = Buffer.alloc(CW * CW * 3);
  for (let y = 0; y < CW; y++)
    for (let x = 0; x < CW; x++) {
      const v = fb[(Y0 + Math.floor(y / S)) * 240 + X0 + Math.floor(x / S)], c = ((v & 0xff) << 8) | (v >> 8), o = (y * CW + x) * 3;
      px[o] = (((c >> 11) & 31) << 3) | (((c >> 11) & 31) >> 2);
      px[o + 1] = (((c >> 5) & 63) << 2) | (((c >> 5) & 63) >> 4);
      px[o + 2] = ((c & 31) << 3) | ((c & 31) >> 2);
    }
  frames.push({ px, label });
}
function play(mask, times, tag) {               // keys down, a frame at each time after (s)
  ex.web_keys(mask);
  let t = 0;
  for (const ts of times) { run(ts - t); t = ts; grab(`${tag} ${ts}s`); }
}
grab("silence");
play(1 << KEY.C4, [0.06, 0.25, 0.6, 1.5], "C4");
play((1 << KEY.C4) | (1 << KEY.G4), [0.06, 0.3, 1.2], "+G4");
play(0, [0.15, 0.5, 1.2, 2.5], "off");
play((1 << KEY.C4) | (1 << KEY.E4) | (1 << KEY.G4), [0.06, 0.4, 1.5], "chord");
play(0, [2.5], "after");
grab("still");

// the sheet: 4 a row, each its label
const cols = 4, rows = Math.ceil(frames.length / cols), SW = cols * (CW + 6), SH = rows * (CW + LAB + 6);
const raw = Buffer.alloc((SW * 3 + 1) * SH, 40);
for (let y = 0; y < SH; y++) raw[y * (SW * 3 + 1)] = 0;
frames.forEach((f, i) => {
  const ox = (i % cols) * (CW + 6), oy = Math.floor(i / cols) * (CW + LAB + 6) + LAB;
  for (let y = 0; y < CW; y++) f.px.copy(raw, (oy + y) * (SW * 3 + 1) + 1 + ox * 3, y * CW * 3, (y + 1) * CW * 3);
  console.log(`frame ${i}: ${f.label}`);
});
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
const hdr = Buffer.alloc(13);
hdr.writeUInt32BE(SW, 0);
hdr.writeUInt32BE(SH, 4);
hdr[8] = 8;
hdr[9] = 2;
writeFileSync(out, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", hdr), chunk("IDAT", deflateSync(raw)),
                                  chunk("IEND", Buffer.alloc(0))]));
console.log(`${out}: ${frames.length} frames (labels in order above; the screen's sound: ${cstr(ex.web_test_patch_name())})`);
