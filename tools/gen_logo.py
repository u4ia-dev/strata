# SPDX-License-Identifier: GPL-3.0-only
"""Strata's logo: the bloom (HOME's sunflower of dots on the golden angle) inside the dial's ring, the screen's ink
with one spiral arm in vermilion (the dots 21 apart lie on one arm). Writes assets/logo/*.svg; the same spec draws
the FM-1's splash (sy_ui_draw.c).
  python3 tools/gen_logo.py [OUT_DIR]   (default assets/logo)"""
import math, pathlib, sys

GOLD = math.pi * (3 - math.sqrt(5))
INK, HOT, BG = "#f4f1ea", "#ff4d2e", "#0b0b0b"


def bloom(n, arm, ring, r_dots=0.86, s_in=None, s_out=None, ink=INK, hot=HOT, shape="dot", step=13, fade=0.5):
    """n dots in a disc of radius r_dots (of 50), dot size from s_in (centre) to s_out (edge); the arm's dots
    (index % step == arm, None: none) in vermilion, the others fading outward to 1 - fade; ring: the 60 ticks (every fifth long) at radius 50 * 0.93"""
    R = 50 * r_dots * (0.84 if ring else 1.0)
    s_in = s_in or 7.2 * math.sqrt(89 / n)
    s_out = s_out or s_in * 0.55
    out = []
    for i in range(n):
        f = math.sqrt((i + 0.5) / n)
        a = i * GOLD
        x, y = 50 + math.cos(a) * f * R, 50 + math.sin(a) * f * R
        s = s_in + (s_out - s_in) * f
        h = arm is not None and i % step == arm and i > 2
        c, o = (hot, 1.0) if h else (ink, 1.0 - fade * f)
        op = f' opacity="{o:.2f}"' if o < 0.995 else ""
        if shape == "sq":
            out.append(f'<rect x="{x - s / 2:.2f}" y="{y - s / 2:.2f}" width="{s:.2f}" height="{s:.2f}" fill="{c}"{op}/>')
        else:
            out.append(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{s / 2:.2f}" fill="{c}"{op}/>')
    if ring:
        rr = 50 * 0.93
        d = []
        for i in range(60):
            a = -math.pi / 2 + i / 60 * 2 * math.pi
            l = 6.0 if i % 5 == 0 else 2.6
            d.append(f"M{50 + math.cos(a) * (rr - l):.2f} {50 + math.sin(a) * (rr - l):.2f}"
                     f"L{50 + math.cos(a) * rr:.2f} {50 + math.sin(a) * rr:.2f}")
        out.append(f'<path d="{"".join(d)}" stroke="{ink}" stroke-width="1.4" stroke-linecap="butt" fill="none" opacity=".7"/>')
    return "".join(out)


def svg(body, bg=None, title="Strata"):
    b = f'<rect width="100" height="100" rx="18" fill="{bg}"/>' if bg else ""
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" role="img" aria-label="{title}">'
            f"<title>{title}</title>{b}{body}</svg>\n")


# the logo's spec (the FM-1's splash draws the same: sy_ui_draw.c): 89 dots, the arm every 8th from the 3rd (one
# spiral arm, from the centre sweeping out round the left), the ring's 60 ticks; the mark without the ring for mid sizes,
# 34 bigger dots for small ones (the same arm: a dot's angle is its index's)
VARIANTS = {   # file: (bloom's arguments, a ground tile)
    "logo": (dict(n=89, arm=3, ring=True, step=8), None),
    "logo-mark": (dict(n=55, arm=3, ring=False, step=8, s_in=10.5), None),
    "logo-small": (dict(n=34, arm=3, ring=False, step=8, s_in=14.0, fade=0.3), None),
    "logo-tile": (dict(n=89, arm=3, ring=True, step=8), BG),
    "favicon": (dict(n=34, arm=3, ring=False, step=8, s_in=14.0, fade=0.3), BG),
}

if __name__ == "__main__":
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "assets/logo")
    out.mkdir(parents=True, exist_ok=True)
    for k, (a, bg) in VARIANTS.items():
        (out / f"{k}.svg").write_text(svg(bloom(**a), bg), encoding="utf-8", newline="\n")
    print("logo:", ", ".join(VARIANTS))
