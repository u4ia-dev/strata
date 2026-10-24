// SPDX-License-Identifier: GPL-3.0-only
// The site's background (make_site.py puts it on the installer and the info page as fx.js): HOME's bloom from the
// panel's screen. A dense sunflower of dots on the golden angle inside the dial's ring of ticks, beside the page, with a
// faint field of the same spiral filling the rest; the inner dots turn faster, the screen's ink with a vermilion dot
// every few. The pointer pushes dots aside and they spring back; a click or a tap breathes the bloom out and in.
// It follows the installer: while it writes (<body data-progress>, 0..1) the bloom fills from the centre out and the
// dial's ticks light round to the progress; Done breathes it out, an error lets it settle. Still for
// prefers-reduced-motion; paused while the tab is hidden; 30 frames a second while the installer writes.
(() => {
  const cv = document.getElementById("fx");
  if (!cv || !cv.getContext) return;
  const ctx = cv.getContext("2d");
  const still = matchMedia("(prefers-reduced-motion: reduce)");
  const GOLD = Math.PI * (3 - Math.sqrt(5));
  const INK = "244,241,234", HOT = "255,77,46";
  let w = 0, h = 0, cx = 0, cy = 0, rr = 0, dim = 1, dots = [], raf = 0, last = 0, t = 0;
  let px = -1e9, py = -1e9, breath = 0, breathV = 0;
  let prog = NaN, shown = 0, glow = 0, fxEnd = "", stillT = 0;            // the install: its progress, as drawn (eased), Done's glow

  function build() {
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    w = innerWidth; h = innerHeight;
    cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const wide = w >= 980;
    rr = wide ? Math.min(190, Math.max(130, (w - 760) * 0.16)) : Math.min(120, w * 0.26);   // the dial's radius
    cx = wide ? w - Math.max(rr + 60, (w - 760) * 0.26) : w - rr * 0.3;   // beside the page; on a phone in the
    cy = wide ? h * 0.42 : rr * 0.75;                                      // top corner, dimmer, behind less text
    dim = wide ? 1 : 0.5;
    dots = [];
    const core = wide ? 340 : 220;                 // the bloom: even density out to just inside the ring
    for (let i = 0; i < core; i++)
      dots.push({ a: i * GOLD, f: Math.sqrt((i + 0.5) / core), R: rr * 0.86, core: true, hot: i % 34 === 13 });
    const R = Math.hypot(w, h), field = Math.round(Math.min(520, Math.max(160, (w * h) / 2600)));
    for (let i = 0; i < field; i++) {               // the field: the same spiral, sparse, from the ring outwards
      const f = 0.12 + 0.88 * Math.sqrt((i + 0.5) / field);
      dots.push({ a: i * GOLD + 0.5, f, R, core: false, hot: i % 29 === 11 });
    }
    for (const d of dots) { d.ox = d.oy = d.vx = d.vy = 0; d.s = d.core ? 2.4 - d.f * 0.8 : 1.6; }
  }

  function frame(now) {
    raf = 0;
    if (document.hidden) return;
    const busy = document.body.hasAttribute("data-busy");
    if (busy && last && now - last < 32) { raf = requestAnimationFrame(frame); return; }   // (30 a second, writing)
    const dt = Math.min(0.05, last ? (now - last) / 1000 : 0.016);
    last = now; t += dt * (busy ? 1 + 2 * (isNaN(prog) ? 0 : prog) : 1);   // (it turns faster as it writes)
    shown += ((isNaN(prog) ? 0 : prog) - shown) * Math.min(1, dt * 4);
    glow = Math.max(0, glow - dt / 3);
    breathV += (-breath * 40 - breathV * 7) * dt;   // the breath: a damped spring back to rest
    breath += breathV * dt;
    draw(true);
    raf = requestAnimationFrame(frame);
  }

  function draw(move) {
    ctx.clearRect(0, 0, w, h);
    const rad = Math.min(150, Math.max(80, w * 0.08)), rad2 = rad * rad;
    const ink = [[], [], [], []], hot = [];
    const fill = isNaN(prog) && glow <= 0 ? -1 : glow > 0 ? 1 : shown;   // (the core's dots lit up to here, by radius)
    for (const d of dots) {
      const spin = move ? t * (d.core ? 0.06 + 0.16 * (1 - d.f) : 0.012 + 0.03 * (1 - d.f)) : 0;
      const r = d.f * d.R * (1 + breath * (d.core ? 0.45 : 0.2));
      const bx = cx + Math.cos(d.a + spin) * r, by = cy + Math.sin(d.a + spin) * r;
      if (move) {
        const dx = bx + d.ox - px, dy = by + d.oy - py, q = dx * dx + dy * dy;
        if (q < rad2 && q > 1) {                      // pushed aside by the pointer, harder the closer it is
          const dist = Math.sqrt(q), k = (1 - dist / rad) * 2.4;
          d.vx += (dx / dist) * k; d.vy += (dy / dist) * k;
        }
        d.vx = (d.vx - d.ox * 0.05) * 0.86; d.vy = (d.vy - d.oy * 0.05) * 0.86;   // and springs back
        d.ox += d.vx; d.oy += d.vy;
      }
      const x = bx + d.ox, y = by + d.oy;
      if (x < -4 || y < -4 || x > w + 4 || y > h + 4) continue;
      if (d.hot) { hot.push(x, y, d.s + 0.6); continue; }
      if (d.core && fill >= 0 && d.f * d.f <= fill) {            // (the install: lit, its leading edge vermilion)
        if (fill < 1 && d.f * d.f > fill - 0.06) { hot.push(x, y, d.s + 0.4); continue; }
        ink[3].push(x, y, d.s + 0.3); continue;
      }
      const lift = Math.min(1, Math.hypot(d.ox, d.oy) / 14);
      const b = d.core ? Math.min(3, 2 + Math.round((1 - d.f) + lift)) : Math.min(3, Math.round((1 - d.f) * 1.4 + lift * 2));
      ink[b].push(x, y, d.s);
    }
    const alpha = [0.17, 0.28, 0.47, 0.82];
    ink.forEach((b, i) => {
      ctx.fillStyle = `rgba(${INK},${alpha[i] * dim})`;
      for (let j = 0; j < b.length; j += 3) ctx.fillRect(b[j] - b[j + 2] / 2, b[j + 1] - b[j + 2] / 2, b[j + 2], b[j + 2]);
    });
    ctx.fillStyle = `rgba(${HOT},${dim})`;
    for (let j = 0; j < hot.length; j += 3) ctx.fillRect(hot[j] - hot[j + 2] / 2, hot[j + 1] - hot[j + 2] / 2, hot[j + 2], hot[j + 2]);
    const ring = rr * (1 + breath * 0.3), turn = move ? -t * 0.04 : 0;   // HOME's dial: 60 ticks, every fifth long
    ctx.strokeStyle = `rgba(${INK},${0.42 * dim})`; ctx.lineWidth = 1.5; ctx.beginPath();
    for (let i = 0; i < 60; i++) {
      const a = turn + (i / 60) * Math.PI * 2, l = i % 5 === 0 ? 11 : 5;
      ctx.moveTo(cx + Math.cos(a) * ring, cy + Math.sin(a) * ring);
      ctx.lineTo(cx + Math.cos(a) * (ring + l), cy + Math.sin(a) * (ring + l));
    }
    ctx.stroke();
    const lit = glow > 0 ? 60 : fill > 0 ? Math.round(fill * 60) : 0;   // (the install: the ticks lit round from 12)
    if (lit) {
      ctx.strokeStyle = `rgba(${HOT},${(glow > 0 ? Math.min(1, glow) : 0.95) * dim})`; ctx.lineWidth = 2.2; ctx.beginPath();
      for (let i = 0; i < lit; i++) {
        const a = -Math.PI / 2 + turn + (i / 60) * Math.PI * 2, l = i % 5 === 0 ? 13 : 7;
        ctx.moveTo(cx + Math.cos(a) * ring, cy + Math.sin(a) * ring);
        ctx.lineTo(cx + Math.cos(a) * (ring + l), cy + Math.sin(a) * (ring + l));
      }
      ctx.stroke();
    }
  }
  function follow() {                                        // <body data-progress, data-fx> from the installer
    const b = document.body.dataset, p = parseFloat(b.progress), end = b.fx || "";
    prog = isNaN(p) ? NaN : p;
    if (still.matches) shown = isNaN(prog) ? 0 : prog;
    if (end !== fxEnd) {
      fxEnd = end;
      if (end === "done") { glow = still.matches ? 0 : 3; breathV += 2.4; prog = NaN; shown = still.matches ? 0 : shown; }   // (still: no glow left lit)
      if (end === "err") { prog = NaN; shown = 0; }
    }
    kick();
  }

  function kick() {
    if (still.matches) {                         // (reduced motion: the progress still drawn, without moving, at most
      if (!stillT) stillT = setTimeout(() => { stillT = 0; draw(false); }, 100);   // ten times a second)
      return;
    }
    if (!raf && !document.hidden) { last = 0; raf = requestAnimationFrame(frame); }
  }

  addEventListener("resize", () => { build(); still.matches ? draw(false) : kick(); }, { passive: true });
  addEventListener("pointermove", (e) => { px = e.clientX; py = e.clientY; }, { passive: true });
  document.addEventListener("pointerleave", () => { px = py = -1e9; }, { passive: true });
  document.addEventListener("pointerdown", (e) => {
    if (still.matches || e.target.closest("button, a, input, label, select, textarea, summary, details")) return;
    breathV += 1.6;                                   // a breath: the bloom opens and settles
  }, { passive: true });
  document.addEventListener("visibilitychange", kick);
  if (still.addEventListener) still.addEventListener("change", kick);
  new MutationObserver(follow).observe(document.body, { attributes: true, attributeFilter: ["data-busy", "data-progress", "data-fx"] });
  build();
  kick();
})();
