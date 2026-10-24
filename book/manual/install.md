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
Felucca's projects and samples included. It's recommended, not required, and it's made with a backup tool on your
computer, not on this site.

1. Put the FM-1 in **backup mode** (Recovery, below): the Install page's **Put the FM-1 in backup mode** button, or
   <kbd>OCT−</kbd> + <kbd>OCT+</kbd> held for 5 seconds. If the computer doesn't see it, a Flipper Zero with
   [flipper-fm1-recovery](https://github.com/merthsoft/flipper-fm1-recovery) enters backup mode at power-on.
2. In backup mode the FM-1 appears as a USB disk named `WL82 UBOOT1.00`. If the computer offers to format it,
   cancel. Never format it.
3. Read the whole flash with a backup tool: flipper-fm1-recovery's, MvaveFM1Unbricker, the FM-1 Transporter, or
   jl-uboot-tool (`read 0 0x100000`). Read it twice and keep it only if both copies are identical.
4. Switch the FM-1 off and on to leave backup mode. Keep the backup somewhere safe.

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
- **Exactly what you had**: write your backup back in backup mode (Recovery, below), with the flipper-fm1-recovery
  restore tool or jl-uboot-tool. Write only from 0x4000 up, never the boot area.

## Recovery

If no updater can reach the FM-1, put it in **backup mode**, where the chip itself accepts a backup or firmware over
USB:

- **The SysEx message** `F0 22 24 35 7D F7` on the FM-1's USB MIDI port (as on the official firmware), or the Install
  page's **Put the FM-1 in backup mode**.
- **<kbd>OCT−</kbd> and <kbd>OCT+</kbd> held for 5 seconds** on Strata or Felucca (a countdown shows from 2 s).
- **A Flipper Zero** with flipper-fm1-recovery, which works at power-on whatever the firmware does. Use it if the
  other two don't make the FM-1 appear as a disk on the computer.

Switching the FM-1 off and on leaves backup mode. In backup mode, the flipper-fm1-recovery restore tool rewrites only
the firmware sectors that differ and checks them; it never writes the boot area.

- **A write stopped** (cable out, browser closed): don't switch the FM-1 off. Reconnect and press **Find the FM-1**
  again; the page finishes the write.
- **STRATA CRASH** on the screen: Strata restarts after 4 s. A second crash within 30 s of starting puts the FM-1 in
  backup mode by itself.
- **A frozen screen**: the watchdog restarts it within about 8 s; two such starts put it in backup mode.
