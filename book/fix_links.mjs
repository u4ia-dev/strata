// SPDX-License-Identifier: GPL-3.0-only
// After `honkit build`: HonKit links a folder's README as the folder ("../", "./"), which a web server opens but a
// page opened from disk cannot. Point those links at the folder's index.html, so the book works both ways. And
// Strata's logo (assets/logo, tools/gen_logo.py): the favicon instead of HonKit's, the logo atop the contents.
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

const LOGO_DIR = join(fileURLToPath(new URL(".", import.meta.url)), "..", "assets", "logo");
const svgUri = (f) => "data:image/svg+xml," + encodeURIComponent(readFileSync(join(LOGO_DIR, f), "utf8").trim());
const FAV = svgUri("favicon.svg"), LOGO = svgUri("logo.svg");

const out = process.argv[2] || "../build/book";
let pages = 0, links = 0;
const walk = (dir) => {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) { if (e.name !== "gitbook") walk(p); continue; }
    if (!e.name.endsWith(".html")) continue;
    const t = readFileSync(p, "utf8");
    const up = "../".repeat(relative(out, dir).split(sep).filter(Boolean).length);
    const u = t.replace(/href="((?:\.\.\/)+|\.\/)"/g, (_, up) => { links++; return `href="${up === "./" ? "" : up}index.html"`; })
      .replace(/<link rel="shortcut icon"[^>]*>/, `<link rel="icon" type="image/svg+xml" href="${FAV}">`)
      .replace(/<link rel="apple-touch-icon-precomposed"[^>]*>/, "")
      .replace('<ul class="summary">', `<a class="st-brand" href="${up}index.html"><img src="${LOGO}" alt="" width="40" height="40"><span>STRATA</span></a><ul class="summary">`);
    if (u !== t) { writeFileSync(p, u); pages++; }
  }
};
walk(out);
console.log(`fix_links: ${links} folder links in ${pages} pages now end in index.html`);
