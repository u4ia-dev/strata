// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 brandonmbarone
// The simulated FM-1 below follows Felucca's web/test_web.mjs FakeFM1 (Copyright (C) 2026 Leo Kuroshita
// (@kurogedelic), Hügelton Instruments, GPL-3.0).
// STRATA's flasher in a real browser, before any FM-1 sees it: build/flasher (make_flasher.py) served on 127.0.0.1,
// headless Chrome with a throwaway profile over the DevTools protocol (a pipe, no packages). Before the page runs, a
// simulated FM-1 takes Web MIDI's place (the update protocol's device side: the handshake, step 1's reads, the reset
// into the update loader, step 2's reads, the reset into the new firmware; every byte served checked against the
// package) and a simulated console takes Web Serial's; the FM-1 can be switched off and on (fm.restart), and stay mute
// after that (cfg.mute: it takes in USB but sends nothing, as on 2026-10-16). Scenarios, each in a fresh page:
//   the package checked, the install gated by the four boxes; an install over the official firmware, Done only after
//   the restart; a resume (the FM-1 found in update mode); the install whose Strata is mute after the restart, never
//   Done; a device that is no FM-1; none at all; Cancel (nothing sent); backup mode (the soft
//   key, nothing written); the report and RUN over the console; an official file that is not V15 refused; a package
//   changed by one byte refused; no sideways scroll at a phone's width; the dark scheme.
// Screenshots: OUT/flasher_light.png (the page as it opens), flasher_done.png (after an install), flasher_phone_dark.png.
//   node web/strata_flash/flash_test.mjs [build/flasher] [OUT]   (after make_flasher.py; on Windows: Windows' node;
//                                                                  OUT: build/flasher_shots)
import { spawn } from "node:child_process";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const root = process.argv[2] || "build/flasher", out = process.argv[3] || "build/flasher_shots";
const CHROME = process.env.CHROME || ["C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe", "/usr/bin/google-chrome", "/usr/bin/chromium",
  "/usr/bin/chromium-browser", "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"].find((p) => fs.existsSync(p));
if (!CHROME) { console.log("flasher: no Chrome or Edge found (set CHROME)"); process.exit(1); }
if (!fs.existsSync(path.join(root, "index.html"))) { console.log(`flasher: no ${root}/index.html (make_flasher.py)`); process.exit(1); }
const TYPES = { ".html": "text/html", ".fwsc": "application/octet-stream", ".md": "text/plain" };

// ------------------------------------------------------------------ the server: /ok/<file>, /bad/<file> (one byte off)
const server = http.createServer((req, res) => {
  const [, variant, ...rest] = decodeURIComponent(req.url.split("?")[0]).split("/");
  const file = rest.join("/") || "index.html";
  let body;
  if (!["ok", "bad"].includes(variant) || file.includes("..")) { res.writeHead(404); res.end(); return; }
  try { body = fs.readFileSync(path.join(root, file)); } catch { res.writeHead(404); res.end(); return; }
  if (variant === "bad" && file === "strata.fwsc") { body = Buffer.from(body); body[0x5000] ^= 1; }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(file)] || "application/octet-stream" });
  res.end(body);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}`;

// ------------------------------------------------------------------ the simulated FM-1 and console, in the page
const FAKE = (cfg) => `(() => {
  const cfg = ${JSON.stringify(cfg)};
  const eq = (a, b) => a.length === b.length && a.every((x, i) => x === b[i]);
  function pack7(data) { const out = []; let acc = 0, nb = 0; for (const b of data) { acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; } } if (nb) out.push(acc & 0x7F); return out; }
  function unpack7(s) { const out = []; let acc = 0, nb = 0; for (const b of s) { acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; } } return out; }
  const HS = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7], UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
  const SOFT = [0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7];
  const fm = window.__fm1 = { served: 0, bad: 0, upgrades: 0, softkeys: 0, identity: "", image: null, log: [],
                              mute: false, restarts: 0 };
  fm.restart = () => {                                                  // off (its ports gone), then on again
    const id = fm.identity, name = [...access.inputs.values()].map((p) => p.name)[0] || "Felucca";
    fm.restarts++;
    boot("", "");
    setTimeout(() => { boot(id, name); fm.mute = !!cfg.mute; }, 600);
  };
  fetch("strata.fwsc").then((r) => r.arrayBuffer()).then((b) => {      // the logical image: the 20 markers out
    const raw = new Uint8Array(b), img = new Uint8Array(raw.length - 20);
    for (let i = 0; i < 20; i++) img.set(raw.subarray(i * 48, i * 48 + 47), i * 47);
    img.set(raw.subarray(960), 940);
    fm.image = img;
  });
  const access = { inputs: new Map(), outputs: new Map(), onstatechange: null };
  let input, output, waiting = null, queue = [];
  function boot(identity, name) {
    fm.identity = identity; waiting = null; queue = [];
    for (const m of [access.inputs, access.outputs]) { for (const p of m.values()) p.state = "disconnected"; m.clear(); }
    if (!identity) return;
    const id = Math.random().toString(36).slice(2);
    input = { id: "i" + id, name, state: "connected", onmidimessage: null, open: async () => {} };
    output = { id: "o" + id, name, state: "connected", open: async () => {}, send: (d) => {
      if (output.state !== "connected") throw new Error("InvalidStateError");
      const data = Array.from(d);
      setTimeout(() => rx(data), 1);
    } };
    access.inputs.set(input.id, input);
    access.outputs.set(output.id, output);
  }
  function tx(bytes) { const i = input; setTimeout(() => { if (i.state === "connected" && i.onmidimessage) i.onmidimessage({ data: Uint8Array.from(bytes) }); }, 1); }
  function next() {
    const r = queue.shift();
    if (!r) return;
    waiting = r;
    const [addr, len] = r;
    const u = [0, 0x59, 0x30, 0, 0, 0, 0, addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF, len & 0xFF, len >> 8, 0];
    let s = 0;
    for (let i = 6; i < 14; i++) s += u[i];
    u.push(~s & 0xFF);
    tx([0xF0, ...pack7(u), 0xF7]);
  }
  function rx(d) {
    if (eq(d, HS)) {
      if (fm.mute) return;                                              // (in, but nothing back)
      const t = [...new TextEncoder().encode(fm.identity)];
      tx([0xF0, ...pack7([0, 0x59, 0x11, 0, 0, 0, ...t, ...new Array(28 - t.length).fill(0)]), 0xF7]);
    } else if (eq(d, SOFT)) {
      fm.softkeys++;
      setTimeout(() => boot("", ""), 50);                               // (gone from MIDI: a USB disk now)
    } else if (eq(d, UPGRADE)) {
      fm.upgrades++;
      const n = fm.image.length, flash = [];
      for (let a = 0x400; a < 0x400 + 0x93000; a += 512 * 32) flash.push([a, 512]);
      queue = fm.identity.startsWith("ota-")
        ? [[0, 64], [0x40, 160], ...flash, [n - 512, 512], [0xF0000000, 8]]   // the loader: header, flash.bin, ota.bin
        : [[0, 64], [0x40, 160], [0x400, 512], [0x4400, 512], [n - 512, 512], [0xE0000000, 8]];
      next();
    } else if (waiting) {
      const u = unpack7(d.slice(1, -1));
      const [addr, len] = waiting;
      const got = u.slice(14, 14 + (addr >= 0xE0000000 ? 8 : len));
      const want = addr >= 0xE0000000 ? [...new TextEncoder().encode("success"), 0] : Array.from(fm.image.subarray(addr, addr + len));
      if (!eq(got, want)) fm.bad++;
      waiting = null;
      fm.served++;
      if (addr === 0xE0000000) setTimeout(() => boot("ota-FM-1_900", "Felucca Update"), 300);
      else if (addr === 0xF0000000) setTimeout(() => boot(cfg.after || "FM-1_900", "Felucca"), 300);
      else next();
    }
  }
  if (cfg.mode === "stock") boot("FM-1_015", "FM-1");
  else if (cfg.mode === "loader") boot("ota-FM-1_900", "Felucca Update");
  else if (cfg.mode === "other") boot("XY-2_001", "FM-1");
  else boot("", "");
  Object.defineProperty(navigator, "requestMIDIAccess", { value: async () => access, configurable: true });

  const con = window.__con = { got: [] };                               // Strata's console (sy_console.c)
  const REPLY = { "": "", status: "strata V0.5 HARDWARE TEST (FM-1_900)\\r\\nuptime_ms 61234\\r\\ncpu_pct 31\\r\\nhalf_us 905\\r\\n",
                  bench: "boot_ms 712\\r\\nhalf_max_us 1830\\r\\nframe_max_us 9100\\r\\nwait_max_us 11200\\r\\n",
                  "bench run": "RUN asked: 'bench' again in a second\\r\\n" };
  const port = { open: async () => {}, close: async () => {}, setSignals: async () => {}, readable: null, writable: null };
  let ctl;
  port.readable = new ReadableStream({ start(c) { ctl = c; } });
  let line = "";
  port.writable = new WritableStream({ write(chunk) {
    for (const ch of new TextDecoder().decode(chunk)) {
      if (ch !== "\\r") { line += ch; continue; }
      con.got.push(line);
      const reply = line in REPLY ? REPLY[line] : "? (help)\\r\\n";
      line = "";
      setTimeout(() => ctl.enqueue(new TextEncoder().encode("\\r\\n" + reply + "> ")), 20);
    }
  } });
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port }, configurable: true });
})();`;

// ------------------------------------------------------------------ Chrome over a pipe (JSON, each ended by a 0 byte)
const prof = fs.mkdtempSync(path.join(os.tmpdir(), "strata-flasher-"));
const chrome = spawn(CHROME, ["--headless=new", "--remote-debugging-pipe", `--user-data-dir=${prof}`, "--no-first-run",
  "--no-default-browser-check", "--window-size=1000,1000", "about:blank"], { stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"] });
const toChrome = chrome.stdio[3], fromChrome = chrome.stdio[4];
let nextId = 1, acc = Buffer.alloc(0);
const pending = new Map(), events = [];
fromChrome.on("data", (d) => {
  acc = Buffer.concat([acc, d]);
  let z;
  while ((z = acc.indexOf(0)) >= 0) {
    const m = JSON.parse(acc.subarray(0, z).toString("utf8"));
    acc = acc.subarray(z + 1);
    if (m.id && pending.has(m.id)) {
      const p = pending.get(m.id);
      pending.delete(m.id);
      if (m.error) p.rej(new Error(`${p.method}: ${m.error.message}`));
      else p.res(m.result);
    } else if (m.method) events.push(m);
  }
});
function send(method, params = {}, sessionId) {
  const id = nextId++;
  toChrome.write(JSON.stringify({ id, method, params, ...(sessionId ? { sessionId } : {}) }) + "\0");
  return new Promise((res, rej) => pending.set(id, { res, rej, method }));
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
async function ev(sid, expression) {
  const r = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true, userGesture: true }, sid);
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
  return r.result.value;
}
async function shot(sid, name) {
  fs.mkdirSync(out, { recursive: true });
  const { data } = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true }, sid);
  fs.writeFileSync(path.join(out, name), Buffer.from(data, "base64"));
}
const exceptions = (sid) => events.filter((e) => e.sessionId === sid && e.method === "Runtime.exceptionThrown")
  .map((e) => e.params.exceptionDetails.exception?.description || e.params.exceptionDetails.text);

async function open(cfg, variant = "ok") {
  const { targetId } = await send("Target.createTarget", { url: "about:blank" });
  const { sessionId: sid } = await send("Target.attachToTarget", { targetId, flatten: true });
  for (const d of ["Runtime", "Page"]) await send(`${d}.enable`, {}, sid);
  await send("Page.addScriptToEvaluateOnNewDocument", { source: FAKE(cfg) }, sid);
  await send("Emulation.setEmulatedMedia", { features: [{ name: "prefers-color-scheme", value: "light" }] }, sid);
  await send("Page.navigate", { url: `${base}/${variant}/index.html` }, sid);
  for (let t = 0; t < 100; t++) {
    if (await ev(sid, "document.readyState === 'complete' && !/Loading/.test(document.getElementById('status').textContent) && !!window.__fm1.image")) break;
    await sleep(50);
  }
  return { sid, targetId };
}
const status = (sid) => ev(sid, "document.getElementById('status').textContent");
const shown = (sid, id) => ev(sid, `getComputedStyle(document.getElementById('${id}')).display !== 'none'`);   // (drawn, not the attribute)
const click = (sid, id) => ev(sid, `document.getElementById('${id}').click()`);
const tick = (sid) => ev(sid, "document.querySelectorAll('.pre').forEach((c) => { c.checked = true; c.dispatchEvent(new Event('change')); })");
async function until(sid, expr, ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) { if (await ev(sid, expr)) return true; await sleep(100); }
  return false;
}

let checks = 0, failed = 0;
const check = (what, ok) => { checks++; if (!ok) failed++; console.log(`flasher: ${what.padEnd(100)} ${ok ? "ok" : "FAIL"}`); };
try {
  {   // the package checked; Find waits for the four boxes
    const { sid, targetId } = await open({ mode: "stock" });
    const st = await status(sid), facts = await ev(sid, "document.getElementById('f-product').textContent + ' ' + document.getElementById('f-size').textContent");
    const before = await ev(sid, "document.getElementById('find').disabled");
    const hid = [];
    for (const id of ["confirm", "uboot-confirm", "stock-confirm", "bar", "report-text"]) if (await shown(sid, id)) hid.push(id);
    check(`as it opens, every confirmation, the bar and the report not drawn${hid.length ? " (drawn: " + hid.join(", ") + ")" : ""}`, !hid.length);
    await ev(sid, "document.getElementById('c-backup').click()");
    const one = await ev(sid, "document.getElementById('find').disabled");
    await tick(sid);
    const after = await ev(sid, "document.getElementById('find').disabled");
    check(`the package checked (${facts}); Find off, off with one box, on with four`,
          /Package checked/.test(st) && /FM-1_900/.test(facts) && /610,090/.test(facts) && before && one && !after);
    await shot(sid, "flasher_light.png");
    check(`no exceptions on load${exceptions(sid).map((x) => " | " + x.slice(0, 160)).join("")}`, exceptions(sid).length === 0);
    await send("Target.closeTarget", { targetId });
  }
  {   // an install over the official firmware: step 1, the loader, step 2, Strata back
    const { sid, targetId } = await open({ mode: "stock" });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "!document.getElementById('confirm').hidden", 8000);
    const found = await ev(sid, "document.getElementById('found').textContent");
    check("after Find: its confirmation drawn, Find itself off", await shown(sid, "confirm") && await ev(sid, "document.getElementById('find').disabled"));
    await click(sid, "write");
    const asked = await until(sid, "/switch the FM-1 off/.test(document.getElementById('status').textContent) || document.getElementById('status-card').dataset.kind === 'err'", 90000);
    await sleep(1500);
    const waiting = await status(sid);
    check(`written, then not Done before a restart: "${waiting.slice(0, 60)}..."`,
          asked && /switch the FM-1 off/.test(waiting) && !/^Done/.test(waiting));
    await ev(sid, "window.__fm1.restart()");
    const done = await until(sid, "/^Done/.test(document.getElementById('status').textContent) || document.getElementById('status-card').dataset.kind === 'err'", 30000);
    const st = await status(sid), fm = await ev(sid, "({ ...window.__fm1, image: null, restart: null })");
    check(`install over FM-1_015 ("${found.slice(0, 40)}..."): ${st.slice(0, 44)}; ${fm.served} reads, ${fm.bad} bad`,
          done && /^Done: Strata answers after a restart/.test(st) && /FM-1_015/.test(found) && /official/.test(found) &&
          fm.upgrades === 2 && fm.bad === 0 && fm.identity === "FM-1_900" && fm.restarts === 1);
    check("the bar at 100 % after the install", await ev(sid, "document.getElementById('bar').value") === 100);
    await shot(sid, "flasher_done.png");
    await send("Target.closeTarget", { targetId });
  }
  {   // the 2026-10-16 lockout: Strata written, then mute after the restart (in, nothing back): never Done
    const { sid, targetId } = await open({ mode: "stock", mute: true });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "!document.getElementById('confirm').hidden", 8000);
    await click(sid, "write");
    await until(sid, "/switch the FM-1 off/.test(document.getElementById('status').textContent)", 90000);
    await ev(sid, "window.__fm1.restart()");
    const end = await until(sid, "document.getElementById('status-card').dataset.kind === 'err' || /^Done/.test(document.getElementById('status').textContent)", 40000);
    const st = await status(sid);
    check(`mute after the restart: "${st.slice(0, 64)}..."`,
          end && !/^Done/.test(st) && /did not answer after the restart/.test(st) && /Flipper Zero route/.test(st));
    await send("Target.closeTarget", { targetId });
  }
  {   // the FM-1 already in update mode: the page finishes the write
    const { sid, targetId } = await open({ mode: "loader" });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "!document.getElementById('confirm').hidden", 8000);
    const found = await ev(sid, "document.getElementById('found').textContent");
    await click(sid, "write");
    await until(sid, "/switch the FM-1 off/.test(document.getElementById('status').textContent) || document.getElementById('status-card').dataset.kind === 'err'", 60000);
    await ev(sid, "window.__fm1.restart()");
    const done = await until(sid, "/^Done/.test(document.getElementById('status').textContent) || document.getElementById('status-card').dataset.kind === 'err'", 30000);
    const fm = await ev(sid, "({ ...window.__fm1, image: null, restart: null })");
    check(`resume in update mode: "${found.slice(0, 48)}...", ${fm.served} reads, ${fm.bad} bad, then the restart`,
          done && /^Done/.test(await status(sid)) && /update mode/.test(found) && fm.upgrades === 1 && fm.bad === 0 &&
          fm.identity === "FM-1_900" && fm.restarts === 1);
    await send("Target.closeTarget", { targetId });
  }
  {   // another device on a port named like the FM-1: refused before anything is written
    const { sid, targetId } = await open({ mode: "other" });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "document.getElementById('status-card').dataset.kind === 'err'", 8000);
    const st = await status(sid), fm = await ev(sid, "({ ...window.__fm1, image: null, restart: null })");
    check(`a device that is not an FM-1: "${st.slice(0, 60)}", nothing sent`,
          /not an FM-1/.test(st) && /XY-2_001/.test(st) && fm.upgrades === 0 && await ev(sid, "document.getElementById('confirm').hidden"));
    await send("Target.closeTarget", { targetId });
  }
  {   // no FM-1 at all
    const { sid, targetId } = await open({ mode: "none" });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "document.getElementById('status-card').dataset.kind === 'err'", 8000);
    check(`no FM-1: "${(await status(sid)).slice(0, 60)}"`, /No FM-1 answered/.test(await status(sid)));
    await send("Target.closeTarget", { targetId });
  }
  {   // Cancel after Find: nothing sent
    const { sid, targetId } = await open({ mode: "stock" });
    await tick(sid);
    await click(sid, "find");
    await until(sid, "!document.getElementById('confirm').hidden", 8000);
    await click(sid, "cancel");
    await sleep(2500);
    const fm = await ev(sid, "({ ...window.__fm1, image: null, restart: null })");
    check(`Cancel: "${(await status(sid)).slice(0, 40)}", no upgrade sent, Find on again`,
          fm.upgrades === 0 && /nothing was written/.test(await status(sid)) && !await ev(sid, "document.getElementById('find').disabled"));
    await send("Target.closeTarget", { targetId });
  }
  {   // backup mode: the soft key, after its own confirmation; nothing written
    const { sid, targetId } = await open({ mode: "stock" });
    const early = await shown(sid, "uboot-confirm");
    await click(sid, "uboot-go");                                   // (not drawn yet: must do nothing)
    await sleep(1500);
    const ignored = (await ev(sid, "window.__fm1.softkeys")) === 0;
    await click(sid, "uboot");
    const asked = await shown(sid, "uboot-confirm");
    const none = (await ev(sid, "window.__fm1.softkeys")) === 0;
    await click(sid, "uboot-go");
    await until(sid, "window.__fm1.softkeys > 0 && document.getElementById('status-card').dataset.kind === 'ok'", 8000);
    const fm = await ev(sid, "({ ...window.__fm1, image: null, restart: null })");
    check(`backup mode: confirmed first, the soft key once, no upgrade ("${(await status(sid)).slice(0, 44)}...")`,
          !early && ignored && asked && none && fm.softkeys === 1 && fm.upgrades === 0 && /UBOOT1\.00/.test(await status(sid)));
    await send("Target.closeTarget", { targetId });
  }
  {   // the report: status and bench; RUN: bench run, then status and bench
    const { sid, targetId } = await open({ mode: "stock" });
    await click(sid, "report");
    await until(sid, "!document.getElementById('report-text').hidden", 15000);
    const text = await ev(sid, "document.getElementById('report-text').textContent");
    const got1 = await ev(sid, "window.__con.got.slice()");
    check(`the report: ${got1.filter(Boolean).join(", ")}; ${text.split("\n").length} lines, Copy on`,
          /half_us 905/.test(text) && /wait_max_us 11200/.test(text) && /FM-1_900/.test(text) &&
          got1.filter(Boolean).join() === "status,bench" && !await ev(sid, "document.getElementById('copy').disabled"));
    await click(sid, "run");
    await until(sid, "window.__con.got.filter(Boolean).length >= 5 && document.getElementById('status-card').dataset.kind === 'ok'", 20000);
    const got2 = await ev(sid, "window.__con.got.filter(Boolean)");
    check(`RUN: ${got2.slice(2).join(", ")}`, got2.slice(2).join() === "bench run,status,bench");
    await send("Target.closeTarget", { targetId });
  }
  {   // an "official" file that is not V15: refused, its button stays off
    const { sid, targetId } = await open({ mode: "stock" });
    await tick(sid);
    await ev(sid, `(() => { const dt = new DataTransfer(); dt.items.add(new File([new Uint8Array(699956)], "FM-1.fwsc"));
      const i = document.getElementById('stock-file'); i.files = dt.files; i.dispatchEvent(new Event('change')); })()`);
    await until(sid, "/Cannot use/.test(document.getElementById('status').textContent)", 8000);
    check(`a fake FM-1.fwsc of V15's size: "${(await status(sid)).slice(0, 60)}..."`,
          /Cannot use this file/.test(await status(sid)) && await ev(sid, "document.getElementById('stock-go').disabled"));
    await send("Target.closeTarget", { targetId });
  }
  {   // a package one byte off: nothing offered
    const { sid, targetId } = await open({ mode: "stock" }, "bad");
    await tick(sid);
    check(`a package changed by one byte: "${(await status(sid)).slice(0, 56)}...", Find off`,
          /not the one it was made for/.test(await status(sid)) && await ev(sid, "document.getElementById('find').disabled"));
    await send("Target.closeTarget", { targetId });
  }
  {   // a phone's width; the dark scheme
    const { sid, targetId } = await open({ mode: "stock" });
    await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 1, mobile: true }, sid);
    await sleep(400);
    const [sw, iw] = await ev(sid, "[document.documentElement.scrollWidth, innerWidth]");
    const light = await ev(sid, "getComputedStyle(document.body).backgroundColor");
    await send("Emulation.setEmulatedMedia", { features: [{ name: "prefers-color-scheme", value: "dark" }] }, sid);
    await sleep(200);
    const dark = await ev(sid, "getComputedStyle(document.body).backgroundColor");
    await shot(sid, "flasher_phone_dark.png");
    check(`at 390 px no sideways scroll (${sw} <= ${iw}); light ${light}, dark ${dark}`, sw <= iw && light !== dark);
    await send("Target.closeTarget", { targetId });
  }
} catch (err) {
  failed++;
  console.log("flasher: ERROR " + (err.stack || err));
} finally {
  try { await Promise.race([send("Browser.close"), sleep(3000)]); } catch {}
  chrome.kill();
  server.close();
  await sleep(500);
  try { fs.rmSync(prof, { recursive: true, force: true }); } catch {}
}
console.log(`flasher: ${checks} checks, ${failed} failed`);
process.exit(failed ? 1 : 0);
