# Strata for FM-1

**Strata** is free firmware for the **M-VAVE FM-1**. It turns the little keyboard into a wavetable synthesizer, an
8-track looper and a drum machine, with a screen interface of its own. Strata is by u4ia, a fork of
[Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton Instruments), and it is free software under
the GNU General Public License, version 3 only.

![HOME, the first screen](images/Home-2x.png)

## What it does

- **A wavetable synth**: two wavetable oscillators over 54 factory tables, a sub oscillator and noise, a filter of ten
  types, two envelopes, two LFOs you can draw, eight voices, and a modulation matrix of eight slots with polarity.
- **A chain of up to eight effects** from thirteen types (drive, chorus, delay, reverb, OTT, EQ, flanger, phaser, a
  filter, bitcrush, tremolo, ring mod, tape), in any order.
- **A performer's tools**: scales, chords on every key, strum, a bass note under the chord, an arpeggiator with
  sixteen programmable steps, PERFORM's bend and wheel, four macros, tap tempo.
- **An 8-track looper**: synth, MIDI and drum tracks, a clip per track in each of eight scenes, takes and overdubs,
  knob moves recorded, a piano roll, SONG, four projects.
- **A drum machine**: eight drum rows, seven drum models any row can use, 64 drum presets by category, six factory
  kits, your own drum presets and kits, a step grid, Euclidean fills, a sidechain duck and a kit compressor.
- **The STRATA screen**: one picture a module, numbered knob handles on what each knob moves, twelve colour themes,
  steady LEDs at four levels.

## Who it is for

Anyone with an FM-1 who wants more from it than the official firmware gives: sound design on a pocket synth, sketching
songs on the looper, playing chords and arpeggios from a small keyboard, or driving a computer or another synth over
USB MIDI. You need a computer with Chrome or Edge to install it, and nothing else.

## How this book is organised

- **[The Guide](guide/panel.md)** is short. It shows the panel, how to play and change a sound, save it, use the looper
  and the drums, and how to install Strata and go back. Read its first three pages and you are making music.
- **[The Manual](manual/overview.md)** is complete. It covers every button and its mappings, every layer and window,
  every module, page and parameter, with tips. Its last chapters are troubleshooting, the limits, and the
  [parameter reference](manual/parameters.md), a table of every parameter a sound stores.
- **[Changes](changes.md)** lists what each version brought, newest first.

The book matches **Strata V0.87** (steps 1 and 2). Names in capitals (OSC A, WAVE, ALGORITHM) are the screen's own
words; buttons are shown as keys, <kbd>EDIT</kbd>, <kbd>OCT−</kbd>; the four knobs under the screen are KNOB 1 to 4,
left to right.

## Disclaimer

Strata is independent, unofficial firmware. It is not affiliated with, endorsed by or supported by M-VAVE or the owners
of the FM-1, nor by Felucca's author or Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their respective
owners; "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. They are used here only to say what
Strata runs on and where it comes from.

Strata is provided **as is, without any warranty**, under the GNU General Public License version 3 (sections 15 and
16: no warranty, no liability). Installing any firmware other than the official one can leave the FM-1 unusable until
it is recovered, and you install Strata at your own risk. Strata is a **pre-release**: its versions from V0.7 on are
being tested on the hardware now. Where this book says a feature **needs a hardware test**, it works in the emulator
and has not yet been confirmed on an FM-1.

> **Your saved data.** Strata keeps its presets, projects and drum library in the FM-1's data area, in the same flash
> where Felucca keeps its projects and samples (0x97000 to 0xDF000). Installing Strata does not erase that area, but
> **saving anything in Strata can overwrite Felucca's projects and samples**, and going back to Felucca or to the
> official firmware does not bring them back. Make a full flash backup before you install and keep it: it is the only
> way back to exactly what you had. Strata never writes the boot area, the calibration and trim data, Bluetooth pairing
> or the unit's own keys.

## Where to install it

Strata installs from a web page, the **[Strata flasher](../flash/)**, in Chrome or Edge on a computer, with the FM-1 on
a USB data cable. The Guide's page [Installing, updating, going back](guide/install.md) walks through it; the Manual's
[Installing and going back](manual/install.md) has the details, the backup and every way back.

## Licence and credits

Strata, Copyright (C) 2026 u4ia, is a modified version of Felucca, Copyright (C) 2026 Leo Kuroshita (@kurogedelic),
Hügelton Instruments; both are GPL-3.0-only. The screen's typeface is Space Mono (SIL Open Font License 1.1). The
second core's start-up follows the FM-1 projects X0X, Melodee and Jangada (GPL-3.0). The complete source, the notices
(NOTICE.md, CREDITS.md) and the licence texts are published beside the flasher.
