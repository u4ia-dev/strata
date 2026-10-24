// SPDX-License-Identifier: GPL-3.0-only
// After `honkit build`: HonKit links a folder's README as the folder ("../", "./"), which a web server opens but a
// page opened from disk cannot. Point those links at the folder's index.html, so the book works both ways.
import { readdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const out = process.argv[2] || "../build/book";
let pages = 0, links = 0;
const walk = (dir) => {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) { if (e.name !== "gitbook") walk(p); continue; }
    if (!e.name.endsWith(".html")) continue;
    const t = readFileSync(p, "utf8");
    const u = t.replace(/href="((?:\.\.\/)+|\.\/)"/g, (_, up) => { links++; return `href="${up === "./" ? "" : up}index.html"`; });
    if (u !== t) { writeFileSync(p, u); pages++; }
  }
};
walk(out);
console.log(`fix_links: ${links} folder links in ${pages} pages now end in index.html`);
