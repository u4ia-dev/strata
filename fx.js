// SPDX-License-Identifier: GPL-3.0-only
// The site's background (make_site.py puts it on the installer and the info page as fx.js): HOME's bloom from the
// panel's screen. A dense sunflower of dots on the golden angle inside the dial's ring of ticks, beside the page, with a
// faint field of the same spiral filling the rest; the inner dots turn faster, the screen's ink with a vermilion dot
// every few. The pointer pushes dots aside and they spring back; a click or a tap breathes the bloom out and in.
// Still for prefers-reduced-motion; paused while the tab is hidden and while the installer writes (<body data-busy>).
(() => {
  const cv = document.getElementById("fx");
  if (!cv || !cv.getContext) return;
  const ctx = cv.getContext("2d");
  const still = matchMedia("(prefers-reduced-motion: reduce)");
  const GOLD = Math.PI * (3 - Math.sqrt(5));
  const INK = "244,241,234", HOT = "255,77,46";
  let w = 0, h = 0, cx = 0, cy = 0, rr = 0, dim = 1, dots = [], raf = 0, last = 0, t = 0;
  let px = -1e9, py = -1e9, breath = 0, breathV = 0;

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
    if (document.body.hasAttribute("data-busy")) { setTimeout(kick, 500); return; }
    const dt = Math.min(0.05, last ? (now - last) / 1000 : 0.016);
    last = now; t += dt;
    breathV += (-breath * 40 - breathV * 7) * dt;   // the breath: a damped spring back to rest
    breath += breathV * dt;
    draw(true);
    raf = requestAnimationFrame(frame);
  }

  function draw(move) {
    ctx.clearRect(0, 0, w, h);
    const rad = Math.min(150, Math.max(80, w * 0.08)), rad2 = rad * rad;
    const ink = [[], [], [], []], hot = [];
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
  }

  function kick() {
    if (still.matches) { draw(false); return; }
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
  new MutationObserver(kick).observe(document.body, { attributes: true, attributeFilter: ["data-busy"] });
  build();
  kick();
})();
