// SPDX-License-Identifier: GPL-3.0-only
// STRATA's hardware test page (build/flasher/test.html, make_flasher.py) in a real browser, before any FM-1 runs it:
// served on 127.0.0.1, headless Chrome with a throwaway profile over the DevTools protocol (a pipe, as flash_test.mjs),
// the page opened with ?fast (its waits a tenth). Before the page runs, a simulated FM-1 takes Web MIDI's and Web
// Serial's place: its console answers status, bench, bench run, crash, inp and memr from a model of the patch (every
// parameter's stored value, sy_patch, and its value on the audio side, sp_n, which follows a change within a few ms,
// as sy_param.c's glide does, at the addresses the page's META names) and of the app's bytes (the fingerprint); notes,
// controllers (the parameters' own CCs, as sy_app.c maps them), the bank and program changes change it. It can keep
// the values from id 256 up where a load left them (cfg.bug: V0.6), run another build (cfg.other), take no program
// changes (cfg.noprogram), be switched off and on (fm.power), and its panel be used (fm.turn, fm.chain, fm.enc).
// Scenarios, each in a fresh page: as it opens; the automatic tests on a good FM-1 (every row, the load table, no
// SysEx, only the page's read-only commands, every note ended, the sound put back); PROGRAM off; another build; the
// fixed effects' knobs with and without V0.6's bug; the chain's seven steps; the encoders; the keys' wait; three
// restarts and one that crashed; the report; no sideways scroll at a phone's width; the dark scheme. V0.87.1 adds
// the test projects (the console answers `tproj N`, `tproj`, `tproj stop`; getUserMedia is stubbed with a generated
// tone from a device named "Strata for FM-1"; the page is served by the real serve.py with a temporary --hwtest folder,
// so the saved WAV and JSON, the token check and the refusals are the real ones), the three backup-mode buttons
// (`uboot yes a|b|c`: the console drops), the "draw" row's two buttons and the chord-MIDI row gone.
//   node web/strata_flash/test_test.mjs [build/flasher] [OUT]   (after make_flasher.py; on Windows: Windows' node;
//                                                                OUT: build/flasher_shots)
import { spawn } from "node:child_process";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { spawnSync } from "node:child_process";

const root = process.argv[2] || "build/flasher", out = process.argv[3] || "build/flasher_shots";
const CHROME = process.env.CHROME || ["C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe", "/usr/bin/google-chrome", "/usr/bin/chromium",
  "/usr/bin/chromium-browser", "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"].find((p) => fs.existsSync(p));
if (!CHROME) { console.log("testpage: no Chrome or Edge found (set CHROME)"); process.exit(1); }
const page = path.join(root, "test.html");
if (!fs.existsSync(page)) { console.log(`testpage: no ${page} (make_flasher.py)`); process.exit(1); }
const META = JSON.parse(fs.readFileSync(page, "utf8").match(/const META = (\{.*?\});\n/)[1]);

const server = http.createServer((req, res) => {
  const file = decodeURIComponent(req.url.split("?")[0]).replace(/^\/+/, "") || "test.html";
  if (file.includes("..")) { res.writeHead(404); res.end(); return; }
  try {
    const body = fs.readFileSync(path.join(root, file));
    res.writeHead(200, { "Content-Type": file.endsWith(".html") ? "text/html" : "application/octet-stream" });
    res.end(body);
  } catch { res.writeHead(404); res.end(); }
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
let base = `http://127.0.0.1:${server.address().port}`;
const staticBase = base;
// serve.py itself, for the recordings (a temporary --hwtest folder)
const PY = ["python", "py", "python3"].find((c) => { try { return spawnSync(c, ["--version"]).status === 0; } catch { return false; } });
const hwdir = fs.mkdtempSync(path.join(os.tmpdir(), "strata-hwtest-"));
let serveProc = null, servePort = 0;
async function startServe() {
  if (!PY) throw new Error("no python for serve.py");
  const probe = http.createServer();
  await new Promise((r) => probe.listen(0, "127.0.0.1", r));
  servePort = probe.address().port;
  await new Promise((r) => probe.close(r));
  serveProc = spawn(PY, [path.join(root, "serve.py"), String(servePort), "--hwtest", hwdir, "--backups", path.join(hwdir, "bk")], { stdio: ["ignore", "ignore", "ignore"] });
  for (let t = 0; t < 100; t++) {
    const ok = await new Promise((r) => http.get({ host: "127.0.0.1", port: servePort, path: "/api/backup" }, (res) => { res.resume(); r(res.statusCode === 200); }).on("error", () => r(false)));
    if (ok) return;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error("serve.py did not start");
}
// a raw POST (node's http allows the Origin and token headers a test sets)
function rawPost(pth, body, headers = {}) {
  return new Promise((res, rej) => {
    const r = http.request({ host: "127.0.0.1", port: servePort, path: pth, method: "POST", headers: { Origin: `http://127.0.0.1:${servePort}`, ...headers } }, (resp) => {
      let d = ""; resp.on("data", (c) => (d += c)); resp.on("end", () => res({ code: resp.statusCode, body: d }));
    });
    r.on("error", rej);
    r.end(body);
  });
}

// ------------------------------------------------------------------ the simulated FM-1, in the page
const FAKE = (cfg) => `(() => {
  const cfg = ${JSON.stringify(cfg)}, M = ${JSON.stringify(META)};
  const P = M.params.map(([name, sec, lo, hi, flags, cc], id) => ({ id, name, sec, lo, hi, flags, cc }));
  const N = P.length, SOUNDS = ["SUPERSAW", "PLUCK", "PAD", "FM KEYS", "SYNC LEAD", "LADDER BASS", "VOWEL PAD", "OTT GROWL", "INIT"];
  const norm = (p, v) => Math.fround(Math.fround(v - p.lo) / Math.fround(p.hi - p.lo));
  const fm = window.__fm = { midi: [], cmds: [], sysex: null, on: true, sound: cfg.sound || "PAD", slot: cfg.slot ?? -1,
    p: new Int16Array(N), n: new Float32Array(N), changed: new Float64Array(N), loaded: 0, notes: new Map(), bank: 0,
    late: 0, shed: 0, run: 0, enc: [], keys: 0, crash: "", boots: 0, uac: 0, tp: { k: 0, t0: 0 }, uboot: [] };
  window.confirm = () => true;                                           // (the backup-mode buttons ask first)
  const TPN = ["VOICES", "FX", "DRUMS", "LOOPER"];
  const tone = { rate: 0 };
  Object.defineProperty(navigator.mediaDevices, "enumerateDevices", { value: async () => [
    { kind: "audioinput", label: "Microphone (Realtek)", deviceId: "d0" }, { kind: "audioinput", label: "Strata for FM-1 (1209:5354)", deviceId: "d1" },
    { kind: "audiooutput", label: "Speakers", deviceId: "o0" }], configurable: true });
  Object.defineProperty(navigator.mediaDevices, "getUserMedia", { value: async (c) => {   // (a generated tone, not the FM-1)
    fm.gum = fm.gum || []; fm.gum.push(JSON.stringify(c));
    const ctx = new AudioContext(), dst = ctx.createMediaStreamDestination(), osc = ctx.createOscillator(), g = ctx.createGain();
    osc.frequency.value = 440; g.gain.value = 0.5; osc.connect(g).connect(dst); osc.start(); await ctx.resume();
    tone.rate = ctx.sampleRate;
    return dst.stream;
  }, configurable: true });
  const cut = P.find((p) => p.name === "CUTOFF" && M.secs[p.sec] === "FILTER");
  function load() {                                                     // a sound: every value at once (sp_snap)
    for (const p of P) { fm.p[p.id] = p.lo + Math.trunc((p.hi - p.lo) * 0.3); fm.n[p.id] = norm(p, fm.p[p.id]); fm.changed[p.id] = 0; }
    for (const p of P) if (/^CHAIN [1-8]$/.test(p.name)) fm.p[p.id] = 0;
    fm.p[cut.id] = 700; fm.n[cut.id] = norm(cut, 700);
    fm.loaded = performance.now();
  }
  load();
  setInterval(() => {                                                   // the glide: a value follows its stored one
    const now = performance.now();
    for (const p of P) {
      if (!fm.changed[p.id] || now - fm.changed[p.id] < 8) continue;
      if (cfg.bug && p.id >= 256 && (p.flags & 4)) continue;           // (V0.6: an id from 256 up never moved again)
      fm.n[p.id] = norm(p, fm.p[p.id]); fm.changed[p.id] = 0;
    }
  }, 4);
  fm.set = (id, v) => { fm.p[id] = v; fm.changed[id] = performance.now(); };
  fm.turn = (id, d = 37) => { const p = P[id]; fm.set(id, Math.max(p.lo, Math.min(p.hi, fm.p[id] + d))); };
  const chainIds = P.filter((p) => /^CHAIN [1-8]$/.test(p.name)).map((p) => p.id);
  fm.chain = (mods) => { chainIds.forEach((id, k) => fm.set(id, k < mods.length ? mods[k] + 1 : 0)); };
  fm.onOf = (inst) => P.find((p) => p.sec === M.secs.indexOf("DRIVE") + inst && p.name === "ON").id;

  // MIDI: one port pair named as the FM-1's (usb.c: "Felucca")
  const access = { inputs: new Map(), outputs: new Map(), onstatechange: null };
  const outp = { id: "o1", name: "Felucca", type: "output", state: "connected", open: async () => {}, send: (d) => {
    if (!fm.on) throw new Error("InvalidStateError");
    const m = Array.from(d); fm.midi.push(m); rx(m);
  } };
  access.outputs.set("o1", outp);
  access.inputs.set("i1", { id: "i1", name: "Felucca", type: "input", state: "connected", onmidimessage: null, open: async () => {} });
  function rx([st, a, b]) {
    const t = st & 0xF0;
    if (st === 0xF0) return;
    if (t === 0x90 && b) fm.notes.set(a, 1);
    else if (t === 0x80 || (t === 0x90 && !b)) setTimeout(() => fm.notes.delete(a), 60);
    else if (t === 0xC0) {
      if (cfg.noprogram) return;
      const bk = fm.bank;                                               // (sy_app.c: the bank as the change arrives)
      setTimeout(() => { fm.sound = bk ? "USER " + a : SOUNDS[a] || "INIT"; fm.slot = bk ? a : -1; load(); }, 30);
    } else if (t === 0xB0) {
      if (a === 0) { fm.bank = b ? 1 : 0; return; }
      if (a === 123) { setTimeout(() => fm.notes.clear(), 60); return; }
      const p = P.find((q) => q.cc === a);
      if (p) fm.set(p.id, Math.max(p.lo, Math.min(p.hi, p.lo + Math.trunc((p.hi - p.lo) * b / 127))));
    }
  }
  Object.defineProperty(navigator, "requestMIDIAccess", { value: async (o) => { fm.sysex = !!(o && o.sysex); return access; }, configurable: true });

  // the console (sy_console.c): an echo, the reply, the prompt
  const hex = (v, n) => v.toString(16).toUpperCase().padStart(n, "0");
  function byte(a) {
    const sp = M.addr.sy_patch, sn = M.addr.sp_n;
    if (a >= sp && a < sp + 2 * N) { const v = fm.p[(a - sp) >> 1] & 0xFFFF; return (a - sp) & 1 ? v >> 8 : v & 0xFF; }
    if (a >= sn && a < sn + 4 * N) { const b = new Uint8Array(new Float32Array([fm.n[(a - sn) >> 2]]).buffer); return b[(a - sn) & 3]; }
    for (const [f, h] of M.fingerprint) if (a >= f && a < f + 16) return parseInt(h.substr(2 * (a - f), 2), 16) ^ (cfg.other ? 1 : 0);
    return 0;
  }
  function reply(line) {
    const kv = (o) => Object.entries(o).map(([k, v]) => k + " " + v + "\\r\\n").join("");
    if (line === "") return "";
    if (line === "status") return "strata " + (cfg.other ? "V0.6 HARDWARE TEST" : M.version) + " (" + M.product + ")\\r\\n" + kv({
      uptime_ms: Math.round(performance.now()), cpu_pct: 12 + 9 * fm.notes.size, half_us: 400, half_max_us: 900 + 100 * fm.notes.size,
      halves_late: fm.late, guard_level: 0, guard_fx_lo: 0, voices_shed: fm.shed, voices: fm.notes.size, sound: fm.sound,
      user_slot: fm.slot, master_q12: 3000, safe_start: 0, bootguard_failed: 0, usb_resets: 1, uac_alt: 1, uac_hz: 48000,
      uac_pkts: 1000, uac_rate_hz: 48000, uac_underruns: 0, uac_overruns: fm.uac, uac_missed: 0, uac_stalls: 0 });
    if (line === "bench") return kv({ boot_ms: 760, half_us: 400, half_max_us: 1500, frame_us: 4100, frame_max_us: 12900,
      wait_max_us: cfg.wait || 8200, stack_irq_used: 2300, stack_irq_size: 7936, stack_main_used: cfg.stack || 6900,
      stack_main_size: 12032, spare_bytes: 45056, spare_changed_bytes: cfg.spare || 0, run: fm.run, run_int_loops: 100000,
      run_int_us: fm.run === 2 ? 3895 : 0, run_flt_loops: 50000, run_flt_us: fm.run === 2 ? 3051 : 0 });
    let mm;
    if ((mm = line.match(/^tproj ([1-4])$/))) {
      if (cfg.tpbusy || fm.tp.k) return "tproj " + mm[1] + " busy\\r\\n";
      fm.tp.k = Number(mm[1]); fm.tp.t0 = performance.now(); fm.tp.log = (fm.tp.log || []).concat(fm.tp.k);
      setTimeout(() => { fm.tp.k = 0; }, cfg.tpms || 1500);
      return "tproj " + mm[1] + " " + TPN[mm[1] - 1] + " ok\\r\\n";
    }
    if (line === "tproj stop") { fm.tp.k = 0; return "tproj stop ok\\r\\n"; }
    if (line === "tproj") return fm.tp.k ? "tproj " + fm.tp.k + " " + TPN[fm.tp.k - 1] + " " + Math.round((performance.now() - fm.tp.t0) * 50) + "\\r\\n" : "tproj none\\r\\n";
    if ((mm = line.match(/^uboot yes ([abc])$/))) { fm.uboot.push(mm[1]); setTimeout(() => fm.power(false), 150); return "uboot way " + mm[1] + ": leaving\\r\\n"; }
    if (line === "bench run") { fm.run = 1; setTimeout(() => { fm.run = 2; }, 120); return "RUN asked: 'bench' again in a second\\r\\n"; }
    if (line === "crash") return fm.crash || "no crash record\\r\\n";
    if (line === "inp") {
      const e = fm.enc.length ? fm.enc : Array.from({ length: 7 }, () => [0, 0, 0]);
      const t = "window_ms 900\\r\\nenc moves/lost/detent state: " + e.map((x) => x.join("/")).join(" ") + "\\r\\nkey_presses " + fm.keys + "\\r\\n";
      fm.enc = []; fm.keys = 0;
      return t;
    }
    const m = line.match(/^memr 0x([0-9A-F]{8}) (\\d+)$/);
    if (m) {
      const a = parseInt(m[1], 16), n = Math.min(256, Number(m[2]));
      let t = "";
      for (let i = 0; i < n; i++) {
        if (i % 16 === 0) t += hex(a + i, 8) + ":";
        t += " " + hex(byte(a + i), 2);
        if (i % 16 === 15 || i + 1 === n) t += "\\r\\n";
      }
      return t;
    }
    return "? (help)\\r\\n";
  }
  let ctl = null, line = "";
  const port = { getInfo: () => ({ usbVendorId: 0x1209, usbProductId: 0x0001 }), readable: null, writable: null, opened: false,
    open: async () => {
      if (!fm.on) throw new Error("NetworkError: the device is gone");
      if (port.opened) throw new Error("InvalidStateError: already open");
      port.opened = true;
      port.readable = new ReadableStream({ start(c) { ctl = c; } });
      port.writable = new WritableStream({ write(chunk) {
        for (const ch of new TextDecoder().decode(chunk)) {
          if (ch !== "\\r") { line += ch; continue; }
          const l = line; line = "";
          fm.cmds.push(l);
          const r = reply(l), c = ctl;
          setTimeout(() => { if (fm.on && c === ctl) c.enqueue(new TextEncoder().encode(l + "\\r\\n" + r + "> ")); }, 2);
        }
      } });
    },
    close: async () => { port.opened = false; },
    setSignals: async () => { setTimeout(() => ctl && ctl.enqueue(new TextEncoder().encode("\\r\\nStrata " + M.version + " console - 'help'\\r\\n> ")), 5); },
  };
  fm.power = (on, crash) => {
    if (!on) { fm.on = false; outp.state = "disconnected"; try { ctl.close(); } catch (_) {} ctl = null; port.opened = false; return; }
    fm.boots++; fm.crash = crash || ""; fm.notes.clear(); fm.on = true; outp.state = "connected"; load();
    if (access.onstatechange) access.onstatechange({ port: outp });
  };
  Object.defineProperty(navigator, "serial", { value: { requestPort: async () => port, getPorts: async () => (fm.on ? [port] : []),
    addEventListener: () => {} }, configurable: true });
})();`;

// ------------------------------------------------------------------ Chrome over a pipe (JSON, each ended by a 0 byte)
const prof = fs.mkdtempSync(path.join(os.tmpdir(), "strata-testpage-"));
const chrome = spawn(CHROME, ["--headless=new", "--remote-debugging-pipe", `--user-data-dir=${prof}`, "--no-first-run",
  "--no-default-browser-check", "--window-size=1000,1000", "--autoplay-policy=no-user-gesture-required", "--disable-background-timer-throttling",
  "--disable-renderer-backgrounding", "--disable-backgrounding-occluded-windows", "about:blank"],
  { stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"] });   // (the pages' timers at full speed: each is a new tab)
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
async function open(cfg = {}, scheme = "light") {
  const { targetId } = await send("Target.createTarget", { url: "about:blank" });
  const { sessionId: sid } = await send("Target.attachToTarget", { targetId, flatten: true });
  for (const d of ["Runtime", "Page"]) await send(`${d}.enable`, {}, sid);
  await send("Page.addScriptToEvaluateOnNewDocument", { source: FAKE(cfg) }, sid);
  await send("Emulation.setEmulatedMedia", { features: [{ name: "prefers-color-scheme", value: scheme }] }, sid);
  await send("Page.navigate", { url: `${base}/test.html?fast` }, sid);
  for (let t = 0; t < 100 && !(await ev(sid, "document.readyState === 'complete'").catch(() => false)); t++) await sleep(50);
  return { sid, targetId };
}
const click = (sid, sel) => ev(sid, `document.querySelector(${JSON.stringify(sel)}).click()`);
const text = (sid, sel) => ev(sid, `document.querySelector(${JSON.stringify(sel)}).textContent`);
const fm = (sid, js) => ev(sid, `(() => { const fm = window.__fm; return ${js}; })()`);
async function until(sid, expr, ms) {
  const end = Date.now() + ms;
  while (Date.now() < end) { if (await ev(sid, expr)) return true; await sleep(50); }
  return false;
}
async function connect(sid) {
  await ev(sid, "document.getElementById('ready').click()");
  await click(sid, "#c-midi");
  await until(sid, "!/not connected/.test(document.getElementById('s-midi').textContent)", 2000);
  await click(sid, "#c-con");
  return until(sid, "/this very build|runs V0|runs something/.test(document.getElementById('s-build').textContent)", 5000);
}
const autoRows = (sid) => ev(sid, "[...document.querySelectorAll('#t-auto tbody tr')].map((tr) => [tr.cells[0].textContent, tr.cells[1].textContent, tr.cells[2].textContent])");
const step = (sid, k) => ev(sid, `document.querySelector('[data-status="${k}"]').textContent`);
async function guided(sid, k, act, ms = 8000) {
  await click(sid, `[data-start="${k}"]`);
  await sleep(150);
  await act();
  return until(sid, `/^(OK|FAIL|WARN|INFO|SKIP) /.test(document.querySelector('[data-status="${k}"]').textContent)`, ms);
}
// what the page sent: SysEx never; console commands only its own read-only ones; every note it started ended
async function hygiene(sid) {
  const midi = await fm(sid, "fm.midi"), cmds = await fm(sid, "fm.cmds"), sysex = await fm(sid, "fm.sysex");
  const ok = /^(|status|bench|bench run|inp|crash|memr 0x[0-9A-F]{8} \d{1,3})$/;
  const bad = cmds.filter((c) => !ok.test(c)), x = midi.filter((m) => m[0] === 0xF0 || (m[0] & 0x0F) !== 0);
  const open = new Map();
  for (const [st, a, b] of midi) {
    if ((st & 0xF0) === 0x90 && b) open.set(a, 1);
    else if ((st & 0xF0) === 0x80 || ((st & 0xF0) === 0x90 && !b)) open.delete(a);
    else if (st === 0xB0 && a === 123) open.clear();
  }
  return { sysex, bad, x, open: [...open.keys()], cmds: cmds.length, midi: midi.length };
}

let checks = 0, failed = 0;
const check = (what, ok) => { checks++; if (!ok) failed++; console.log(`testpage: ${what.padEnd(100)} ${ok ? "ok" : "FAIL"}`); };
try {
  {   // as it opens: the build's facts, nothing to press before the box
    const { sid, targetId } = await open();
    const facts = await text(sid, "#facts"), dis = await ev(sid, "['c-midi','c-con','auto'].every((id) => document.getElementById(id).disabled)");
    await ev(sid, "document.getElementById('ready').click()");
    const en = await ev(sid, "!document.getElementById('c-midi').disabled && document.getElementById('auto').disabled");
    check(`as it opens: "${facts.slice(0, 44)}...", Connect off until the box, the tests until connected`,
          facts.includes(META.version) && dis && en);
    await shot(sid, "testpage_light.png");
    check(`no exceptions as it opens${exceptions(sid).length ? ": " + exceptions(sid)[0] : ""}`, !exceptions(sid).length);
    await send("Target.closeTarget", { targetId });
  }
  {   // the automatic tests on a good FM-1, PROGRAM on, a user preset playing
    const { sid, targetId } = await open({ slot: 5, sound: "MY BASS" });
    const same = await connect(sid);
    check(`the console: the FM-1 runs this build (status's version, the fingerprint over memr)`,
          same && /this very build/.test(await text(sid, "#s-build")));
    await click(sid, "#auto");
    const done = await until(sid, "document.getElementById('s-auto').textContent === 'done'", 60000);
    const rows = await autoRows(sid), res = Object.fromEntries(rows.map((r) => [r[0], r[1]]));
    const bad = rows.filter((r) => !["OK", "INFO"].includes(r[1]));
    check(`the automatic run: ${rows.length} rows, every one OK or INFO${bad.length ? " (" + bad.map((r) => r[0] + ": " + r[1] + " " + r[2]).join("; ") + ")" : ""}`,
          done && rows.length === 9 && !bad.length);
    const load = await ev(sid, "document.querySelectorAll('#t-load tbody tr').length");
    const cc = rows.find((r) => /CC 74/.test(r[0]))?.[2] || "";
    check(`the load: 32 chords (8 starter sounds x 1, 2, 4, 8 notes); CC 74: "${cc.slice(0, 60)}..."`,
          load === 32 && /stored \d+, the sound there within \d+ ms/.test(cc));
    const h = await hygiene(sid);
    check(`it sent no SysEx (asked for none), ${h.cmds} console commands all read-only, ${h.midi} MIDI messages on channel 1, every note ended`,
          h.sysex === false && !h.bad.length && !h.x.length && !h.open.length);
    await until(sid, "window.__fm.slot === 5", 1000);
    const midi = await fm(sid, "fm.midi"), tail = midi.slice(-4).map((m) => m.join(" ")).join(" | ");
    check(`the sound put back at the end: bank 1, program 5 (user slot 5), bank 0 (${tail})`,
          /176 0 1 \| 192 5 \| 176 0 0/.test(tail) && await fm(sid, "fm.slot") === 5);
    const rep = await text(sid, "#report");
    check(`the report: the build, the device, every automatic row, the load table`,
          rep.includes(META.sha256) && /device: strata/.test(rep) && (rep.match(/^(OK|INFO) /gm) || []).length >= 9 && /SUPERSAW +8:/.test(rep));
    await shot(sid, "testpage_auto.png");
    check(`no exceptions in the run${exceptions(sid).length ? ": " + exceptions(sid)[0] : ""}`, !exceptions(sid).length);
    await send("Target.closeTarget", { targetId });
  }
  {   // PROGRAM off: the load on the sound playing alone; another build: the RAM reads off
    const a = await open({ noprogram: true });
    await connect(a.sid);
    await click(a.sid, "#auto");
    await until(a.sid, "document.getElementById('s-auto').textContent === 'done'", 60000);
    const r = await autoRows(a.sid), pc = r.find((x) => /Program/.test(x[0]));
    const n = await ev(a.sid, "document.querySelectorAll('#t-load tbody tr').length");
    check(`PROGRAM off: said (${pc[1]}: "${pc[2].slice(0, 40)}..."), the load on the sound playing only (${n} chords)`,
          pc[1] === "WARN" && /PROGRAM is off/.test(pc[2]) && n === 4);
    await send("Target.closeTarget", { targetId: a.targetId });
    const b = await open({ other: true });
    await connect(b.sid);
    const s = await text(b.sid, "#s-build");
    await click(b.sid, "#auto");
    await until(b.sid, "document.getElementById('s-auto').textContent === 'done'", 60000);
    const cc = (await autoRows(b.sid)).find((x) => /CC 74/.test(x[0]));
    const fx = await ev(b.sid, "document.querySelector('[data-start=\"fx\"]').disabled");
    check(`another build on the FM-1: said ("${s.slice(0, 40)}..."), the RAM checks skipped (CC 74: ${cc[1]})`,
          /checks that read its RAM are off/.test(s) && cc[1] === "SKIP");
    await send("Target.closeTarget", { targetId: b.targetId });
  }
  for (const bug of [false, true]) {   // the five fixed effects' knobs: followed, or (V0.6) stuck
    const { sid, targetId } = await open({ bug });
    await connect(sid);
    const ids = await ev(sid, "FIXED.map((t) => P.find((p) => SECS[p.sec] === t && SMOOTH(p) && p.id >= 256).id)");
    await guided(sid, "fx", async () => {
      for (const id of ids) { await fm(sid, `fm.turn(${id})`); await sleep(300); }
      await sleep(900);
      if (!bug) await click(sid, '[data-yn="ringhdr"][data-v="yes"]');
      await click(sid, '[data-done="fx"]');
    });
    const st = await step(sid, "fx"), t = await ev(sid, "[...document.querySelectorAll('#fx-tasks li')].map((li) => li.className)");
    check(bug ? `V0.6's bug: the knobs turned, the values stuck: FAIL, each module marked (${st.slice(0, 50)}...)`
              : `each of the five modules' knobs followed: OK (${st.slice(0, 50)}...)`,
          bug ? /^FAIL/.test(st) && t.every((c) => c === "bad") : /^OK/.test(st) && t.every((c) => c === "done") && /header: yes/.test(st));
    await send("Target.closeTarget", { targetId });
  }
  {   // the chain's seven steps, the encoders, the keys' wait, three restarts
    const { sid, targetId } = await open();
    await connect(sid);
    await guided(sid, "chain", async () => {
      const on = await fm(sid, `fm.onOf(${META.secs.indexOf("DRIVE 2") - META.secs.indexOf("DRIVE")})`);
      const D2 = META.secs.indexOf("DRIVE 2") - META.secs.indexOf("DRIVE"), PH = META.secs.indexOf("PHASER") - META.secs.indexOf("DRIVE");
      for (const [js, wait] of [["fm.chain([1, 3])", 600], ["fm.chain([1, 3, 0])", 600], ["fm.chain([1, 3, 0, " + D2 + "])", 600],
                               [`fm.chain([1, 3, 0, ${D2}, ${PH}])`, 600], [`fm.chain([1, 3, 0, ${D2}])`, 600],
                               [`fm.chain([3, 1, 0, ${D2}])`, 600], [`fm.set(fm.onOf(3), 1)`, 600], [`fm.set(fm.onOf(3), 0)`, 600],
                               [`fm.set(fm.onOf(3), 1)`, 600], [`fm.chain([1, 3, 0, ${D2}])`, 600]]) {
        await fm(sid, js); await sleep(wait);
      }
      void on;
      await click(sid, '[data-yn="clicks"][data-v="none"]');
      await click(sid, '[data-done="chain"]');
    }, 15000);
    const st = await step(sid, "chain");
    check(`the chain: add, take out, move, two of a type, bypass, back, undo, all seen (${st.slice(0, 60)}...)`, /^OK 7 of 7/.test(st));
    await guided(sid, "panel", async () => {
      await fm(sid, "(fm.enc = Array.from({ length: 7 }, () => [24, 0, 0]), fm.keys = 30)");
      await click(sid, '[data-done="panel"]');
    });
    const p = await step(sid, "panel");
    await guided(sid, "panel", async () => {
      await fm(sid, "(fm.enc = [[24, 0, 0], [24, 2, 0], [3, 0, 0], [24, 0, 0], [24, 0, 0], [24, 0, 0], [24, 0, 0]], fm.keys = 30)");
      await click(sid, '[data-done="panel"]');
    });
    const p2 = await step(sid, "panel");
    check(`the encoders: all moved, none lost: OK; one quiet and two transitions lost: WARN (${p2.slice(0, 50)}...)`,
          /^OK/.test(p) && /^WARN/.test(p2) && /few moves on 3/.test(p2) && /2 transitions lost/.test(p2));
    const before = (await fm(sid, "fm.cmds")).length;
    await guided(sid, "draw", async () => {}, 12000);
    const d = await step(sid, "draw"), during = (await fm(sid, "fm.cmds")).slice(before);
    check(`the keys' wait while it draws: OK at 8.2 ms, and only one console command, after (${d.slice(0, 50)}...)`,
          /^OK the keys waited at most 8.2 ms/.test(d) && during.length === 1 && during[0] === "bench");
    await click(sid, '[data-start="boot"]');
    for (let n = 0; n < 3; n++) {
      await until(sid, "/switch the FM-1 off/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 3000);
      await fm(sid, "fm.power(false)");
      await until(sid, "/switch it on/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 3000);
      await sleep(300);
      await fm(sid, "fm.power(true)");
    }
    await until(sid, "/^OK|^FAIL|^WARN/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 8000);
    const b = await step(sid, "boot");
    check(`three restarts: each one reconnected and read (${b.slice(0, 60)}...)`, /^OK/.test(b) && (b.match(/back after/g) || []).length === 3);
    await click(sid, '[data-start="boot"]');
    await until(sid, "/switch the FM-1 off/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 3000);
    await fm(sid, "fm.power(false)");
    await sleep(300);
    await fm(sid, "fm.power(true, 'count 1\\r\\nvec 0x00000001\\r\\npc 0x0201F0A0\\r\\n')");
    await until(sid, "/2 of 3/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 5000);
    await click(sid, '[data-fail="boot"]');
    await until(sid, "/^OK|^FAIL|^WARN/.test(document.querySelector('[data-status=\"boot\"]').textContent)", 8000);
    const b2 = await step(sid, "boot");
    check(`a restart with a crash record: FAIL, the record in the report (${b2.slice(0, 60)}...)`, /^FAIL/.test(b2) && /CRASH count 1/.test(b2));
    const h = await hygiene(sid);
    check(`the guided steps sent no SysEx, only read-only commands, ended every note`, h.sysex === false && !h.bad.length && !h.x.length && !h.open.length);
    check(`no exceptions in the guided steps${exceptions(sid).length ? ": " + exceptions(sid)[0] : ""}`, !exceptions(sid).length);
    await send("Target.closeTarget", { targetId });
  }
  {   // V0.87.1: the rows, the test projects through the real serve.py, backup mode
    await startServe();
    base = `http://127.0.0.1:${servePort}`;
    const wavInfo = (f) => {
      const b = fs.readFileSync(f), pcm = new Int16Array(b.buffer.slice(b.byteOffset + 44, b.byteOffset + b.length - ((b.length - 44) % 2)));
      let pk = 0;
      for (const v of pcm) pk = Math.max(pk, Math.abs(v));
      return { riff: b.toString("ascii", 0, 4) + b.toString("ascii", 8, 12), ch: b.readUInt16LE(22), rate: b.readUInt32LE(24), bits: b.readUInt16LE(34),
               frames: b.readUInt32LE(40) / 4, peak: pk / 32768, size: b.length };
    };
    const runs = () => fs.readdirSync(hwdir).filter((f) => /^\d{8}-\d{6}$/.test(f)).sort();
    {
      const { sid, targetId } = await open();
      const rows = await ev(sid, "[!document.querySelector('[data-look=\"chordmidi\"]'), !!document.querySelector('[data-look=\"wipe\"]'), !!document.querySelector('[data-look=\"confirm\"]'), document.querySelectorAll('[data-fail=\"draw\"]').length, /DEBUG/.test(document.getElementById('g-draw').textContent), !/SETTINGS \\(HOME held/.test(document.body.textContent)]");
      check(`the rows: the chord-MIDI row gone, WIPE and the confirmations in, the "draw" row with two fail buttons, DEBUG named, no old SETTINGS path`,
            rows[0] && rows[1] && rows[2] && rows[3] === 2 && rows[4] && rows[5]);
      await connect(sid);
      await guided(sid, "draw", async () => { await click(sid, '[data-fail="draw"][data-key="draw-steps"]'); }, 12000);
      const rep0 = await text(sid, "#report");
      check(`the "draw" row's own words: "I could not do the steps" is its own line, not "it did not work"`,
            /FAIL +draw-steps \(said\): the tester pressed 'I could not do the steps'/.test(rep0) && !/the tester: it did not work/.test(rep0));
      await send("Target.closeTarget", { targetId });
    }
    {   // TEST 2 on its own
      const { sid, targetId } = await open();
      await connect(sid);
      const before = (await fm(sid, "fm.cmds")).length;
      await click(sid, '[data-tp="2"]');
      const done = await until(sid, "document.querySelectorAll('#t-tp tbody tr').length === 1", 30000);
      const row = await ev(sid, "[...document.querySelectorAll('#t-tp tbody tr')].map((tr) => [tr.cells[0].textContent, tr.cells[1].textContent, tr.cells[2].textContent])[0] || []");
      const cmds = (await fm(sid, "fm.cmds")).slice(before), gum = await fm(sid, "fm.gum"), rn = runs();
      check(`TEST 2: sent status, bench, tproj 2, polled tproj to none, then status and bench; ${cmds.length} commands (${row[1]})`,
            done && cmds[0] === "status" && cmds[1] === "bench" && cmds[2] === "tproj 2" && cmds.filter((c) => c === "tproj").length >= 2 &&
            cmds.slice(-2).join() === "status,bench" && cmds.every((c) => /^(status|bench|tproj|tproj 2)$/.test(c)) && /^(OK|WARN)/.test(row[1]));
      const g = JSON.parse(gum[gum.length - 1]).audio;
      check(`it recorded the input named "Strata for FM-1" (exact id d1), echo cancellation, noise suppression, auto gain all off`,
            g.deviceId.exact === "d1" && g.echoCancellation === false && g.noiseSuppression === false && g.autoGainControl === false);
      const dir = rn.length ? path.join(hwdir, rn[rn.length - 1]) : "", files = dir ? fs.readdirSync(dir).sort() : [];
      check(`serve.py wrote build/hwtest/<date-time>/tp2.wav and tp2.json (${rn[rn.length - 1]}: ${files.join(", ")}), nothing else`,
            rn.length === 1 && files.join() === "tp2.json,tp2.wav");
      const w = wavInfo(path.join(dir, "tp2.wav")), j = JSON.parse(fs.readFileSync(path.join(dir, "tp2.json"), "utf8"));
      check(`the WAV: ${w.riff}, ${w.bits}-bit, ${w.ch} ch, ${w.rate} Hz = the JSON's rate, ${(w.frames / w.rate).toFixed(2)} s, peak ${w.peak.toFixed(3)} (the 0.5 tone)`,
            w.riff === "RIFFWAVE" && w.bits === 16 && w.ch === 2 && w.rate === j.rate && w.rate >= 22050 && w.frames / w.rate > 1.5 && Math.abs(w.peak - 0.5) < 0.02);
      check(`the JSON: test 2 FX, status and bench before and after, the console's lines, the deltas, timing (${Object.keys(j).length} fields)`,
            j.test === 2 && j.replied_name === "FX" && j.status_before.sound && j.status_after.uac_rate_hz && j.bench_before.half_us && j.bench_after.half_us &&
            j.console.length >= 6 && j.console[2].cmd === "tproj 2" && j.deltas.uac_overruns === 0 && j.tproj_ms_last > 0 && j.sent_at_frame >= 0 &&
            j.none_at_frame > j.sent_at_frame && j.device.includes("Strata for FM-1") && j.duration_s > 1.5 && j.version === META.version);
      check(`no exceptions in the recording${exceptions(sid).length ? ": " + exceptions(sid)[0] : ""}`, !exceptions(sid).length);
      await shot(sid, "testpage_tproj.png");
      await send("Target.closeTarget", { targetId });
    }
    {   // busy is an error shown; nothing is saved
      const n0 = runs().length, { sid, targetId } = await open({ tpbusy: true });
      await connect(sid);
      await click(sid, '[data-tp="1"]');
      await until(sid, "document.querySelectorAll('#t-tp tbody tr').length === 1", 15000);
      const row = await ev(sid, "document.querySelector('#t-tp tbody tr').textContent");
      check(`"tproj 1 busy" is an error shown on the page, and nothing was saved (${row.slice(0, 70)}...)`, /FAIL/.test(row) && /busy/.test(row) && runs().length === n0 && !(await fm(sid, "fm.tp.log")));
      await send("Target.closeTarget", { targetId });
    }
    {   // the four in one run folder
      const n0 = runs().length, { sid, targetId } = await open();
      await connect(sid);
      await click(sid, "#tp-all");
      const done = await until(sid, "document.getElementById('s-tp').textContent === 'done'", 90000);
      const rn = runs(), dir = path.join(hwdir, rn[rn.length - 1]), files = fs.readdirSync(dir).sort();
      const order = await fm(sid, "fm.tp.log");
      check(`Run all four: TEST 1, 2, 3, 4 in that order, one new folder with ${files.length} files (${files.join(" ")})`,
            done && rn.length === n0 + 1 && order.join() === "1,2,3,4" && files.join() === "tp1.json,tp1.wav,tp2.json,tp2.wav,tp3.json,tp3.wav,tp4.json,tp4.wav");
      const names = await ev(sid, "R.tp.map((t) => t.name).join()");
      check(`the report lists the four runs (${names})`, names === "VOICES,FX,DRUMS,LOOPER" && /== test projects/.test(await text(sid, "#report")));
      await send("Target.closeTarget", { targetId });
    }
    {   // serve.py's refusals, with the token the page was given
      const html = await new Promise((r) => http.get({ host: "127.0.0.1", port: servePort, path: "/test.html" }, (res) => { let d = ""; res.on("data", (c) => (d += c)); res.on("end", () => r(d)); }));
      const tok = (html.match(/const TOKEN = "([0-9a-f]+)"/) || [])[1] || "";
      const H = { "X-Strata-Token": tok };
      const run = JSON.parse((await rawPost("/api/hwtest/new", "", H)).body).run;
      const wav = Buffer.concat([Buffer.from("RIFF\0\0\0\0WAVE", "binary"), Buffer.alloc(100)]);
      const r1 = await rawPost("/api/hwtest/new", "", {}), r2 = await rawPost("/api/hwtest/new", "", { "X-Strata-Token": "nope" });
      const r3 = await rawPost("/api/hwtest/new", "", { ...H, Origin: "http://evil.example" });
      const names = ["tp5.wav", "tp0.json", "tp1.exe", "..%2ftp1.wav", "tp1.wav%00", "tp1.wav%0a", "x.wav", ".."].map((n) => rawPost(`/api/hwtest/${run}/${n}`, wav, H));
      const bad = (await Promise.all(names)).map((r) => r.code);
      const trav = [(await rawPost(`/api/hwtest/..%2f..%2fx/tp1.wav`, wav, H)).code, (await rawPost(`/api/hwtest/../tp1.wav`, wav, H)).code,
                    (await rawPost(`/api/hwtest/20990101-000000/tp1.wav`, wav, H)).code];
      const notwav = (await rawPost(`/api/hwtest/${run}/tp3.wav`, "hello", H)).code, notjson = (await rawPost(`/api/hwtest/${run}/tp3.json`, "[1]", H)).code;
      const big = await new Promise((res) => {
        const rq = http.request({ host: "127.0.0.1", port: servePort, path: `/api/hwtest/${run}/tp4.wav`, method: "POST",
          headers: { ...H, Origin: `http://127.0.0.1:${servePort}`, "Content-Length": String(70 * 1024 * 1024) } }, (resp) => { resp.resume(); res(resp.statusCode); });
        rq.on("error", () => res(0)); rq.write("x");
        setTimeout(() => { rq.destroy(); res(-1); }, 5000);
      });
      const good = (await rawPost(`/api/hwtest/${run}/tp1.wav`, wav, H)).code, goodj = (await rawPost(`/api/hwtest/${run}/tp1.json`, "{\"a\":1}", H)).code;
      const left = fs.readdirSync(path.join(hwdir, run)).sort().join();
      check(`serve.py: no token, a wrong token and another origin refused (${r1.code} ${r2.code} ${r3.code})`, r1.code === 403 && r2.code === 403 && r3.code === 403);
      check(`serve.py: other names refused (${bad.join(" ")}), traversal and an unknown folder refused (${trav.join(" ")})`,
            bad.every((c) => c === 404) && trav.every((c) => c === 404));
      check(`serve.py: a body that is no WAV or JSON refused (${notwav} ${notjson}), 70 MB refused before it is read (${big}), a good pair taken (${good} ${goodj}); the folder holds ${left}`,
            notwav === 400 && notjson === 400 && big === 413 && good === 200 && goodj === 200 && left === "tp1.json,tp1.wav");
    }
    {   // backup mode, way B: asked first, sent once, the console drops, the report says which way
      const { sid, targetId } = await open();
      await connect(sid);
      await ev(sid, "(document.getElementById('bk-before').value = 'Strata for FM-1 under Ports', 0)");
      const before = (await fm(sid, "fm.cmds")).filter((c) => /uboot/.test(c)).length;
      await click(sid, '[data-bk="b"]');
      await until(sid, "/console dropped/.test(document.getElementById('s-bk').textContent)", 8000);
      await ev(sid, "(document.getElementById('bk-after').value = 'Unknown USB device', document.getElementById('bk-after').dispatchEvent(new Event('input')), 0)");
      const cmds = (await fm(sid, "fm.cmds")).filter((c) => /uboot/.test(c)), rep = await text(sid, "#report");
      check(`backup mode way B: "uboot yes b" sent once (none before), the console dropped, the report says the way and what Windows showed`,
            before === 0 && cmds.join() === "uboot yes b" && /way B pressed at .*Windows before: Strata for FM-1 under Ports; the console dropped after .*Windows after: Unknown USB device/.test(rep));
      await send("Target.closeTarget", { targetId });
    }
    base = staticBase;
  }
  {   // a phone's width, the dark scheme
    const { sid, targetId } = await open({}, "dark");
    await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 844, deviceScaleFactor: 1, mobile: true }, sid);
    await sleep(200);
    const w = await ev(sid, "[document.documentElement.scrollWidth, innerWidth]"), bg = await ev(sid, "getComputedStyle(document.body).backgroundColor");
    check(`at 390 px no sideways scroll (${w[0]} <= ${w[1]}); dark ${bg}`, w[0] <= w[1] && bg === "rgb(19, 18, 16)");
    await shot(sid, "testpage_phone_dark.png");
    await send("Target.closeTarget", { targetId });
  }
} catch (e) {
  check(`the run: ${e.message}`, false);
} finally {
  console.log(`testpage: ${checks} checks, ${failed} failed (screenshots in ${out})`);
  chrome.kill();
  server.close();
  if (serveProc) serveProc.kill();
  try { fs.rmSync(hwdir, { recursive: true, force: true }); } catch (_) {}
  try { fs.rmSync(prof, { recursive: true, force: true }); } catch (_) {}
  process.exit(failed ? 1 : 0);
}
