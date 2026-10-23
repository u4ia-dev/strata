# Strata for FM-1

**Strata** is free firmware for the **M-VAVE FM-1**: a wavetable synthesizer, an 8-track looper and a drum machine,
with a screen interface of its own. It is by u4ia, a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo
Kuroshita (Hügelton Instruments), under the GNU General Public License, version 3 only.

![HOME](images/Home-2x.png)

- **Synth**: two wavetable oscillators on 54 tables, sub, noise, a filter, envelopes, drawable LFOs, eight voices.
- **Effects**: a chain of up to eight from thirteen types.
- **Playing**: scales, chords on every key, strum, an arpeggiator with sixteen steps, macros, tap tempo.
- **Looper**: synth, MIDI and drum tracks, clips in eight scenes, overdubs, recorded knob moves, four projects.
- **Drums**: seven drum models, 64 presets, six kits, a step grid, Euclidean fills, a sidechain duck.

## This book

- **[Guide](guide/guide.md)**: get started and see every feature on one page.
- **[Cheat sheet](guide/cheatsheet.md)**: every button and knob.
- **Manual**: each feature in depth, starting with [The panel](manual/panel.md).

It matches Strata V0.87. Buttons look like <kbd>EDIT</kbd>; the knobs under the screen are KNOB 1 to 4.

## Install

Open the site's **[Install](../flash/)** page in Chrome or Edge on a computer, with the FM-1 on a USB data cable.
Steps and recovery: [Installing and recovery](manual/install.md).

> **Back up first.** Strata keeps its presets, projects and drum library in the flash area where Felucca keeps its
> projects and samples. **Saving anything in Strata can overwrite them**, and going back to Felucca or the official
> firmware does not restore them. A full flash backup is the only way back to exactly what you had
> ([how](manual/install.md#back-up-first)). Strata never writes the boot area, calibration, Bluetooth pairing or the
> unit's keys.

## Disclaimer

Strata is independent, unofficial firmware, not affiliated with or endorsed by M-VAVE or the FM-1's owners, nor by
Felucca's author or Hügelton Instruments; their names are used only to say what Strata runs on and where it comes from.
It is provided **as is, without any warranty** (GPL-3.0, sections 15 and 16). Installing firmware other than the
official one can leave the FM-1 unusable until it is recovered; you install it at your own risk. Strata is a
pre-release.

Strata, Copyright (C) 2026 u4ia, is a modified version of Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic),
Hügelton Instruments. The screen uses Space Mono (SIL OFL 1.1). Source, notices and licences are on the site.
