// SPDX-License-Identifier: GPL-3.0-only
// Every knob on every page turned, one at a time (V0.75): the screen as it shows the turn (the value's flag, or the
// value itself HOT where the page shows it) put through the screens' check (strata_web.c web_lint: text on a drawing,
// on text, at the edges, in MID or LINE; two handles on one place), then turned back. The pages SAVE's and the
// browser's own, and the knobs that act (BENCH's RUN and CLEAR, MIDI's LEARN and CLEAR), left out. A finding's screen
// as PNG (the check's marks), and the list.
//   node web/emu/strata_knob_sweep.mjs [build/emu_strata/strata.wasm] [build/emu_strata/knob_sweep]   (after build_strata.sh)
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

const wasm = process.argv[2] || "build/emu_strata/strata.wasm", out = process.argv[3] || "build/emu_strata/knob_sweep";
const { instance } = await WebAssembly.instantiate(readFileSync(wasm), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
ex.web_nor_erase();
ex.web_master(700);
ex.web_boot();
ex.web_test_set_get(0, 0);                      // (HELP's hints off)
const run = (s) => { for (let n = Math.max(1, Math.round(s * 44118 / 128)); n > 0; n--) ex.web_render(128); };
const cstr = (p) => { const m = new Uint8Array(ex.memory.buffer); let s = ""; while (m[p]) s += String.fromCharCode(m[p++]); return s; };
const K1 = 3;                                   // panel.c EN_K1
const SKIP_SEC = [39, 40];                      // SEC_SAVE, SEC_BROWSE
const SKIP_ID = [1032, 1033, 1035, 1036];      // sy_ui.c UI_LEARN, UI_MCLR, UI_BRUN, UI_BCLR (each an action)

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
function pngOf(scale) {                         // the check's screen, its findings marked (yellow: over a drawing; blue:
  const fb = new Uint16Array(ex.memory.buffer, ex.web_lint_screen(), 240 * 240), W = 240 * scale;   // text on text)
  const mk = new Uint8Array(ex.memory.buffer, ex.web_lint_px(), 240 * 240);
  const raw = Buffer.alloc((W * 3 + 1) * W);
  for (let y = 0; y < W; y++)
    for (let x = 0; x < W; x++) {
      const i = Math.floor(y / scale) * 240 + Math.floor(x / scale), v = fb[i], c = ((v & 0xff) << 8) | (v >> 8);
      const r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31, o = y * (W * 3 + 1) + 1 + x * 3;
      raw[o] = (r << 3) | (r >> 2);
      raw[o + 1] = (g << 2) | (g >> 4);
      raw[o + 2] = (b << 3) | (b >> 2);
      if (mk[i]) { raw[o] = 255; raw[o + 1] = mk[i] === 2 ? 0 : 255; raw[o + 2] = mk[i] === 2 ? 255 : 0; }
    }
  const hdr = Buffer.alloc(13);
  hdr.writeUInt32BE(W, 0);
  hdr.writeUInt32BE(W, 4);
  hdr[8] = 8;
  hdr[9] = 2;
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", hdr), chunk("IDAT", deflateSync(raw)),
                        chunk("IEND", Buffer.alloc(0))]);
}

rmSync(out, { recursive: true, force: true });
mkdirSync(out, { recursive: true });
run(0.6);
ex.web_keys(1 << 7);                            // (a note held: the live parts drawn as they are with sound)
run(0.3);
const found = [];
let turns = 0;
for (let p = 0, n = ex.web_test_go(0); p < n; p++) {
  ex.web_test_go(p);
  run(0.05);
  const sec = ex.web_test_page_section(), name = cstr(ex.web_test_page_name());
  if (SKIP_SEC.includes(sec))
    continue;
  for (let k = 0; k < 4; k++) {
    const id = ex.web_test_page_param(k);
    if (id < 0 || SKIP_ID.includes(id))
      continue;
    ex.web_enc(K1 + k, 1);
    run(0.04);
    turns++;
    const f = ex.web_lint();
    if (f) {
      const tag = `${String(p).padStart(3, "0")}-${sec}-${name.replace(/[^A-Z0-9]/g, "_")}-K${k + 1}`;
      found.push(`${tag}: ${f}\n${cstr(ex.web_lint_report())}`);
      writeFileSync(`${out}/${tag}.png`, pngOf(2));
    }
    ex.web_enc(K1 + k, -1);
    run(0.04);
  }
}
ex.web_keys(0);
writeFileSync(`${out}/sweep.txt`, found.join("\n"));
console.log(`sweep: ${turns} knob turns on every page, ${found.length} with findings (${out}/sweep.txt)`);
for (const f of found) console.log(f.split("\n")[0] + "  " + f.split("\n")[1]);
process.exit(found.length ? 1 : 0);
