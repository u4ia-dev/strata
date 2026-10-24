# Installing and recovery

Strata installs over the FM-1's own USB-MIDI update path, from a web page, as Felucca does. The firmware running now
loads the package and starts the update loader, which writes the firmware area only and restarts into Strata. About a
minute.

> **Disclaimer.** Strata is unofficial and comes **without any warranty** (GPL-3.0). Installing firmware other than the
> official one can leave the FM-1 unusable until it is recovered; you install it at your own risk. Strata was created entirely with Claude, an AI model made by Anthropic.
>
> **Your data.** Saving anything in Strata can overwrite Felucca's projects and samples, which share the same flash.
> Going back does not bring them back. A full backup does.

## Back up first

A full backup is a copy of the FM-1's whole flash (1 MiB). It's the only way back to exactly what you have now,
Felucca's projects and samples included. It's recommended, not required. It's made in the chip's **backup mode**, with
a backup tool on a computer.

**1. Put the FM-1 in backup mode**, in any of these ways:

- **From Strata or Felucca**: hold <kbd>OCT−</kbd> and <kbd>OCT+</kbd> for 5 seconds (a countdown shows from 2 s).
  With Strata V0.87 the computer may show an *unknown USB device* instead; then switch the FM-1 off and on and use
  one of the last two ways. Strata V0.87.1 enters backup mode a different way (unconfirmed on hardware).
- **From the official firmware (V15)**: send the USB-MIDI message `F0 22 24 35 7D F7` with any program that sends
  SysEx.
- **The [FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter)**, a Seeed XIAO RP2040 wired to the FM-1's
  USB lines: at power-on, whatever the firmware does.
- **A Flipper Zero** with [flipper-fm1-recovery](https://github.com/merthsoft/flipper-fm1-recovery): at power-on,
  whatever the firmware does.

In backup mode the FM-1 appears as a USB disk named `WL82 UBOOT1.00`. If the computer offers to format it, cancel.
Never format it.

**2. Read the whole flash** with a backup tool, twice, and keep it only if both copies are identical:

- [MvaveFM1Unbricker](https://github.com/Quixotic7/MvaveFM1Unbricker): `fm1_unbrick.py backup` (Windows or Linux),
  which reads twice and compares by itself.
- [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool): `read 0 0x100000` (Windows or Linux).
- The FM-1 Transporter: `fm1t.py dump` (macOS, Linux or Windows), through its own board.
- flipper-fm1-recovery's `pc_backup.py` (Windows).

**3. Switch the FM-1 off and on** to leave backup mode. Keep the backup somewhere safe.

## Install

1. Open the site's **Install** page in **Chrome or Edge** on a computer. It checks the package and shows its SHA-256.
2. Connect the FM-1 with a USB data cable straight to the computer (no hub), with its battery charged. Close every
   other app that uses MIDI (DAWs, M-UPGRADE, Felucca's editor).
3. In **Before you start**, make your backup (recommended), then tick the three boxes: cable and battery, other MIDI
   apps closed, the data warning understood.
4. In **Install**, press **Find the FM-1** and allow MIDI with SysEx when the browser asks. The page says which
   firmware it found before anything is written.
5. Press **Write Strata**. Don't unplug the FM-1 or touch its power switch until the page says Done.
6. When asked, switch the FM-1 off and on. The page says **Done** once Strata answers after the restart.

To update, do the same with the new version: your presets, projects, drum library and settings stay.

## Going back

- **Official firmware (V15)**: on the Install page, under **Going back**, choose M-VAVE's own `FM-1.fwsc` (checked by
  its SHA-256, never uploaded) and press **Return to official V15**. M-VAVE's own updater also works.
- **Felucca**: use Felucca's installer at [hugelton.github.io/Felucca](https://hugelton.github.io/Felucca/); it finds
  the FM-1 running Strata.
- **Exactly what you had**: write your backup back in backup mode (Recovery, below), with the tool that made it
  (MvaveFM1Unbricker, jl-uboot-tool, the FM-1 Transporter or flipper-fm1-recovery). Write only from 0x4000 up, never
  the boot area.

## Recovery

If no updater can reach the FM-1, put it in **backup mode**, where the chip itself accepts a backup or firmware over
USB:

- **<kbd>OCT−</kbd> and <kbd>OCT+</kbd> held for 5 seconds** on Strata or Felucca, or **the SysEx message**
  `F0 22 24 35 7D F7` on the FM-1's USB MIDI port (as on the official firmware). With Strata V0.87 the computer may
  show an unknown USB device instead of the disk.
- **The FM-1 Transporter** or **a Flipper Zero** with flipper-fm1-recovery, which work at power-on whatever the
  firmware does. Use one of them if the FM-1 doesn't appear as a disk on the computer.

Switching the FM-1 off and on leaves backup mode. In backup mode, the restore tools (MvaveFM1Unbricker, the FM-1
Transporter, flipper-fm1-recovery) write only the firmware area and check it; none of them writes the boot area.

- **A write stopped** (cable out, browser closed): don't switch the FM-1 off. Reconnect and press **Find the FM-1**
  again; the page finishes the write.
- **STRATA CRASH** on the screen: Strata restarts after 4 s. A second crash within 30 s of starting puts the FM-1 in
  backup mode by itself.
- **A frozen screen**: the watchdog restarts it within about 8 s; two such starts put it in backup mode.
