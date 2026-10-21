// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 u4ia
// STRATA's emulator page in a real browser: headless Chrome with a throwaway profile and its audio muted, driven over
// the DevTools protocol on a pipe (no packages). A local server serves build/emu_strata under content security
// policies like those a publishing host may set:
//   none   no policy, worklet.js beside the page
//   self   scripts from the page's origin and inline, no blob: (the worklet from worklet.js)
//   blob   scripts inline and blob: only (worklet.js refused: the page's fallback, the worklet's source as a Blob)
//   nowk   no policy, no worklet.js beside the page (404: the fallback)
//   wrap   strata_emulator_content.html in a skeleton like the host's, under the "self" policy
// Each: the panel builds; Switch on starts it (the screen draws: the worklet runs); a key on the computer keyboard
// lights its LED and its release puts it out; no sideways scroll. "wrap" also at a phone's width (the tall layout),
// with screenshots of both (browser_wide.png, browser_phone.png). The refusals the policies make are printed.
//   node web/emu/strata_browser_test.mjs [build/emu_strata] [build/emu_strata/shots]     (after build_strata.sh;
//   Chrome or Edge from $CHROME, else the usual places; on Windows run it with Windows' node)
import { spawn } from "node:child_process";
import http from "node:http";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const root = process.argv[2] || "build/emu_strata", out = process.argv[3] || "build/emu_strata/shots";
const CHROME = process.env.CHROME || ["C:/Program Files/Google/Chrome/Application/chrome.exe",
  "C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe", "/usr/bin/google-chrome", "/usr/bin/chromium",
  "/usr/bin/chromium-browser", "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"].find((p) => fs.existsSync(p));
if (!CHROME) { console.log("browser: no Chrome or Edge found (set CHROME)"); process.exit(1); }
const CSP = {
  none: null,
  nowk: null,
  self: "default-src 'self'; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; " +
        "font-src 'self' data:; img-src 'self' data: blob:; connect-src 'self'; worker-src 'self' blob:",
  blob: "default-src 'self'; script-src 'unsafe-inline' 'wasm-unsafe-eval' blob:; style-src 'unsafe-inline'; " +
        "font-src data:; img-src data: blob:; connect-src 'self'; worker-src blob:",
};
CSP.wrap = CSP.self;
const PAGE = { none: "strata_emulator.html", self: "strata_emulator.html", blob: "strata_emulator.html",
               nowk: "strata_emulator.html", wrap: "index.html" };
const SKELETON = '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" ' +
  'content="width=device-width, initial-scale=1, viewport-fit=cover"><style>:root{color-scheme:light;' +
  'padding-top:env(safe-area-inset-top,0px);padding-bottom:env(safe-area-inset-bottom,0px)}body{margin:0;' +
  'font:14px system-ui,sans-serif}img{max-width:100%}[hidden]{display:none!important}</style></head><body>';
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".wasm": "application/wasm", ".woff": "font/woff",
                ".txt": "text/plain" };

// ------------------------------------------------------------------ the server: /<policy>/<file>
const server = http.createServer((req, res) => {
  const [, variant, ...rest] = decodeURIComponent(req.url.split("?")[0]).split("/");
  let file = rest.join("/"), body;
  if (!(variant in CSP) || file.includes("..") || (variant === "nowk" && file === "worklet.js")) {
    res.writeHead(404);
    res.end();
    return;
  }
  if (variant === "wrap" && file === "index.html") {
    body = SKELETON + fs.readFileSync(path.join(root, "strata_emulator_content.html"), "utf8") + "</body></html>";
  } else {
    try { body = fs.readFileSync(path.join(root, file)); } catch { res.writeHead(404); res.end(); return; }
  }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(file)] || "application/octet-stream",
                       ...(CSP[variant] ? { "Content-Security-Policy": CSP[variant] } : {}) });
  res.end(body);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const base = `http://127.0.0.1:${server.address().port}`;

// ------------------------------------------------------------------ Chrome over a pipe (JSON, each ended by a 0 byte)
fs.mkdirSync(out, { recursive: true });
const prof = fs.mkdtempSync(path.join(os.tmpdir(), "strata-chrome-"));
const chrome = spawn(CHROME, ["--headless=new", "--remote-debugging-pipe", `--user-data-dir=${prof}`, "--no-first-run",
  "--no-default-browser-check", "--mute-audio", "--autoplay-policy=no-user-gesture-required", "--window-size=1100,1000",
  "about:blank"], { stdio: ["ignore", "ignore", "ignore", "pipe", "pipe"] });
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
async function ev(sid, expression) {                // (as a user gesture: Switch on may start the audio)
  const r = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true, userGesture: true }, sid);
  if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
  return r.result.value;
}
async function shot(sid, name) {
  const { data } = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true }, sid);
  fs.writeFileSync(path.join(out, name), Buffer.from(data, "base64"));
}
const messages = (sid) => events.filter((e) => e.sessionId === sid).map((e) => {
  const p = e.params;
  if (e.method === "Runtime.exceptionThrown") return "EXCEPTION " + (p.exceptionDetails.exception?.description || p.exceptionDetails.text);
  if (e.method === "Runtime.consoleAPICalled" && ["error", "warning"].includes(p.type))
    return p.type.toUpperCase() + " " + p.args.map((a) => a.value ?? a.description).join(" ");
  if (e.method === "Log.entryAdded" && ["error", "warning"].includes(p.entry.level) && !/favicon\.ico/.test(p.entry.url || ""))
    return p.entry.level.toUpperCase() + " " + p.entry.text;
  return null;
}).filter(Boolean);

let checks = 0, failed = 0;
const check = (what, ok) => { checks++; if (!ok) failed++; console.log(`browser: ${what.padEnd(100)} ${ok ? "ok" : "FAIL"}`); };
const LIT = "document.querySelectorAll('#panel g.lit').length";
try {
  for (const variant of Object.keys(PAGE)) {
    const { targetId } = await send("Target.createTarget", { url: "about:blank" });
    const { sessionId: sid } = await send("Target.attachToTarget", { targetId, flatten: true });
    for (const d of ["Runtime", "Log", "Page"]) await send(`${d}.enable`, {}, sid);
    await send("Page.navigate", { url: `${base}/${variant}/${PAGE[variant]}` }, sid);
    for (let t = 0; t < 100; t++) {
      if (await ev(sid, "document.readyState === 'complete' && document.querySelectorAll('#panel g').length > 50")) break;
      await sleep(50);
    }
    const title = await ev(sid, "document.title");
    await ev(sid, "document.getElementById('go').click()");
    for (let t = 0; t < 200; t++) {
      if (await ev(sid, "document.getElementById('start').hidden || !document.getElementById('status').hidden")) break;
      await sleep(50);
    }
    const status = await ev(sid, "document.getElementById('status').hidden ? '' : document.getElementById('status').textContent");
    await sleep(1500);
    const colours = await ev(sid, `(() => { const d = document.getElementById('screen').getContext('2d').getImageData(0, 0, 240, 240).data;
      const s = new Set(); for (let i = 0; i < d.length; i += 4) s.add(d[i] << 16 | d[i + 1] << 8 | d[i + 2]); return s.size; })()`);
    const lit0 = await ev(sid, LIT);
    await ev(sid, "dispatchEvent(new KeyboardEvent('keydown', { code: 'KeyZ', key: 'z' }))");   // Z: the lowest key
    await sleep(600);
    const lit1 = await ev(sid, LIT);
    await ev(sid, "dispatchEvent(new KeyboardEvent('keyup', { code: 'KeyZ', key: 'z' }))");
    await sleep(600);
    const lit2 = await ev(sid, LIT);
    const fits = await ev(sid, "document.documentElement.scrollWidth <= innerWidth");
    check(`[${variant}] "${title}": ${status || "on"}, the screen ${colours} colours, a key's LED: ${lit0} -> ${lit1} -> ${lit2} lit`,
          !status && colours > 8 && lit1 === lit0 + 1 && lit2 === lit0 && fits);
    if (variant === "wrap") {
      await shot(sid, "browser_wide.png");
      await send("Emulation.setDeviceMetricsOverride", { width: 400, height: 860, deviceScaleFactor: 1, mobile: true }, sid);
      await sleep(800);
      const [sw, iw, tall] = await ev(sid, "[document.documentElement.scrollWidth, innerWidth, matchMedia('(orientation: portrait) and (max-width: 700px)').matches]");
      check(`[wrap] at 400 px: the tall layout, no sideways scroll (${sw} <= ${iw})`, tall && sw <= iw);
      await shot(sid, "browser_phone.png");
    }
    for (const m of messages(sid)) console.log(`         ${m.slice(0, 200)}`);
    await send("Target.closeTarget", { targetId });
  }
} catch (err) {
  failed++;
  console.log("browser: ERROR " + (err.stack || err));
} finally {
  try { await Promise.race([send("Browser.close"), sleep(3000)]); } catch {}
  chrome.kill();
  server.close();
  await sleep(500);
  try { fs.rmSync(prof, { recursive: true, force: true }); } catch {}
}
console.log(`browser: ${checks} checks, ${failed} failed (screenshots in ${out})`);
process.exit(failed ? 1 : 0);
