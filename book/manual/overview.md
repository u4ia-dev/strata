# The Manual

The Manual describes everything Strata V0.87 does, in depth: every control and its mappings, every layer and window,
every module and page, every parameter, with tips. The [Guide](../guide/panel.md) is the short way in; this is the
reference.

## How it is arranged

1. **The panel**: [The panel and the screen](panel.md), [Buttons and mappings](buttons.md),
   [Layers and windows](layers.md).
2. **The sound**: [HOME](home.md), [Oscillators, sub and noise](oscillators.md), [Filter](filter.md),
   [Envelopes](envelopes.md), [LFOs](lfos.md), [Modulation](modulation.md), [Voice and global](voice.md),
   [The FX chain](fx.md).
3. **Playing**: [Scales and chords](chords.md), [The ARP](arp.md).
4. **The looper and the drums**: [The looper](looper.md), [The drums](drums.md).
5. **Keeping and connecting**: [Presets and saving](presets.md), [MIDI](midi.md), [USB](usb.md), [LEDs](leds.md),
   [SETTINGS and DEBUG](settings.md), [The store and backups](store.md).
6. **Installing and when things go wrong**: [Installing and going back](install.md),
   [Troubleshooting](troubleshooting.md), [Limits](limits.md).
7. **The [parameter reference](parameters.md)**: every parameter a sound stores, its range, its default, whether it
   can be modulated, and its MIDI CC. It is generated from the firmware's own table.

## Conventions

- Names in capitals are the screen's own: modules (OSC A, FILTER), pages (WAVE, TONE), parameters (WT POS, CUTOFF).
- Buttons are keys: <kbd>EDIT</kbd>, <kbd>OCT−</kbd>. "<kbd>FX</kbd> + <kbd>OCT+</kbd>" means hold FX and press OCT+.
  "Twice" means two presses within 300 ms. "Held" means held past the HOLD time (SETTINGS > PANEL, 400 ms by default).
- KNOB 1 to 4 are the four knobs under the screen, left to right. A page's knobs are written "KNOB 1 · 2 · 3 · 4".
- The keys are named by note, F3 (the lowest) to G5 (the highest), or counted: "white key 1" is F3, "black key 1" is
  F#3.
- A **menu** is what a button opens; a menu holds **modules** (SELECT steps them); a module holds **pages** (ALGORITHM
  steps them).

## Status

Strata is a pre-release. V0.5 ran on an FM-1 on 2026-10-17, V0.6 and V0.65 since; everything from V0.7 to V0.87 is
verified in the emulator and **needs a hardware test**. Where a feature depends on the hardware in a way the emulator
cannot show (the LEDs' look, the battery, the second core, the CPU's real headroom), its section says so.
