# Releasing Strata

What a public release needs, and where each stands (2026-10-23).

## The USB identity

Strata's USB ids are its own, not the pid.codes test id Felucca carries (0x1209 / 0x0001, "must be replaced before
any release"): **VID 0x1209, PID 0x5354** (the app, `firmware/src/strata.c`) and **PID 0x5355** (its update loader,
`firmware/loader/loader.c` with `STRATA_LOADER`, set by `tools/build.py --strata`). Its strings: manufacturer "u4ia",
product "Strata for FM-1" (the loader: "Strata Update for FM-1"); "FM-1" in the name keeps the installers' MIDI port
filter (`web/fm1ota.js`) finding it. Felucca's own build is unchanged.

The two PIDs are **requested, not yet granted**: pid.codes allocates them by a pull request to
<https://github.com/pidcodes/pidcodes.github.com> once the project's source is public with a licence file. The request
adds `org/u4ia/index.md` (layout: org, title: u4ia) and `1209/5354/index.md`, `1209/5355/index.md` (layout: pid,
title: "Strata for FM-1" / "Strata Update for FM-1", owner: u4ia, license: GPL-3.0-only, site: the Strata site,
source: the Strata repository), each with a short description. Firmware for existing hardware may be asked why it
needs a PID: the answer is that Strata presents its own USB MIDI, audio and serial functions and its own update
loader, and must not reuse M-VAVE's ids or a test id. If pid.codes assigns other numbers, change the two defines and
rebuild.

## The checklist

1. The hardware test passes (docs/FLASHING.md, section 4; the V0.87 steps 53 to 57).
2. The PIDs granted (above).
3. A source archive with the submodule: `git archive` of the tag plus `firmware/engines` (GitHub's own archives leave
   the submodule empty), attached to the release as `strata-X.Y-source.tar.gz`.
4. The site rebuilt from the tagged build: `web/strata_flash/make_site.py` (the flasher, the package, the licence
   texts, the notices, links to the source).
5. The release notes carry the disclaimer and the data warning (README.md).
6. The book (the Guide and the Manual, `book/`) brought up to the release by the strata-docs agent
   (`~/.claude/agents/strata-docs.md`: it reads the commits since the book's last update, refreshes the generated
   parameter reference `tools/gen_param_ref.sh`, updates the pages and `book/changes.md`), then built:
   `npm run build` in `book/` (HonKit, pinned; `npm install` once), before `make_site.py`.
