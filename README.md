# Strata

Firmware that makes the **M-VAVE FM-1** a complete pocket instrument: a wavetable synth deep enough to design sounds
from scratch, an 8-track looper and a drum machine for whole songs without a computer, on a panel built to stay quick
to play (the four knobs on each page's four values, one rule for every button, screens read at a glance). Free
software, GPL-3.0-only, built on [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita (Hügelton
Instruments).

**Install it in the browser:** <https://u4ia-dev.github.io/strata/> (Chrome or Edge on a computer).

## Disclaimer

Strata is independent, unofficial firmware. It is not affiliated with, endorsed by or supported by M-VAVE or the
owners of the FM-1, nor by Felucca's author or Hügelton Instruments. "M-VAVE" and "FM-1" are trademarks of their
respective owners; "Felucca" and "Hügelton Instruments" are names of Hügelton Instruments. They are used here only to
say what Strata runs on and where it comes from.

Strata is provided **as is, without any warranty**, under the GNU General Public License version 3 (sections 15 and
16: no warranty, no liability). Installing any firmware other than the official one can leave the FM-1 unusable until
it is recovered, and you install Strata at your own risk. Strata is a **pre-release**: its versions from V0.7 on are
being tested on the hardware now. Strata was created entirely with Claude, an AI model made by Anthropic.

## Before you install

**Your saved data.** Strata keeps its presets, projects and drum library in the FM-1's data area, in the same
flash where Felucca keeps its projects and samples (0x97000 to 0xDF000, inside the store's allow-list). Installing
Strata does not erase that area, but **saving anything in Strata can overwrite Felucca's projects and samples**, and
going back to Felucca or to the official firmware does not bring them back. Make a full flash backup before you
install (docs/FLASHING.md, section 2) and keep it: it is the only way back to exactly what you had. Strata never writes
the boot area, the calibration and trim data, Bluetooth pairing or the unit's own keys.

## What it is

- A wavetable engine (54 tables, two oscillators, sub and noise, filters, envelopes, drawn LFOs, a modulation matrix
  with polarity), a chain of up to eight effects, chords, an arpeggiator.
- An 8-track looper: synth, MIDI and drum tracks, clips in eight scenes, takes and overdubs, SONG, projects.
- A drum synth: seven models any row can use, 64 drum presets by category, six factory kits, your own presets and
  kits, Euclidean fills, a sidechain duck and a kit compressor.
- The STRATA screen UI with twelve themes and steady LEDs at four levels.

The panel, page by page: [docs/CONTROLS.md](docs/CONTROLS.md). The screens: [docs/UI.md](docs/UI.md). The looper:
[docs/LOOPER.md](docs/LOOPER.md). Installing, testing and going back: [docs/FLASHING.md](docs/FLASHING.md).

## Things to know

**The backup-mode message.** Like the official firmware, Strata answers the SysEx message
`F0 22 24 35 7D F7` (the "soft key") on its USB MIDI port by restarting into the chip's backup mode (UBOOT), where a
full flash backup can be read or written. Any program that can send SysEx to the FM-1 can send it. The FM-1 then
stops playing and waits; switching it off and on returns to Strata. Nothing is written by the message itself. The same
mode is reached by holding OCT− and OCT+ for 5 s (a countdown from 2 s).

**Developer features kept in the build.** The USB serial console (a CDC port: the device report, BENCH's
figures, the event log), the DEBUG menu (HOME held 5 s: BENCH, KEY SCAN, CORE 2) and CORE 2 (the second core, off by
default; ON starts it at the next power-on, TEST runs a 30 s load) stay in every Strata build, release builds too.
CORE 2's figures on the FM-1 are not measured yet (**needs a hardware test**).

## Going back

To the official V15: M-VAVE's updater, or the flasher's "Return to official V15" with M-VAVE's own file. To Felucca:
Felucca's installer. To exactly what you had: your flash backup, by the Flipper Zero route (docs/FLASHING.md, section 6).

## Licence and credits

GPL-3.0-only ([LICENSE](LICENSE)). What Strata changed in Felucca, what every copy must come with, and the parts
under other licences: [NOTICE.md](NOTICE.md), [LICENSING.md](LICENSING.md), [CREDITS.md](CREDITS.md),
[LICENSES/](LICENSES/). Felucca's own README: [docs/FELUCCA-README.md](docs/FELUCCA-README.md). Building:
[BUILDING.md](BUILDING.md).
