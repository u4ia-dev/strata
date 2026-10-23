# Installing, updating, going back

Strata installs the way Felucca does: through the FM-1's own USB-MIDI update path, from a web page. Read the
[disclaimer and the data warning](../README.md#disclaimer) first: you install Strata at your own risk.

## You need

- A computer with **Chrome or Edge** (they can send MIDI system-exclusive messages from a web page).
- A **USB data cable** straight from the computer to the FM-1, no hub.
- The FM-1's **battery charged**.
- Every other app that uses MIDI **closed** (DAWs, M-UPGRADE, Felucca's editor): only one app at a time can open the
  FM-1's MIDI port.

## Back up first

A full flash backup is the only way back to exactly what you have now, including Felucca's projects and samples,
which saving in Strata can overwrite. The flasher recommends one before your first Strata. Backups are made on your
own computer, not on the website: with the Strata source's `serve.py` and the Flipper Zero route, or with another
FM-1 backup tool. The Manual's [Installing and going back](../manual/install.md#a-full-backup) explains each way.

## Install

1. Open the **[Strata flasher](../../flash/)** in Chrome or Edge. It checks its package and shows its SHA-256.
2. **1 / 3 Before you start**: make your backup (recommended), then tick the three boxes (the cable and battery, the
   other MIDI apps closed, the data warning understood).
3. **2 / 3 Install**: press **Find the FM-1** and allow MIDI with SysEx when the browser asks. The page says what
   firmware it found before anything is written.
4. Press **Write Strata**. The firmware running now reads the package and starts the update loader; the loader writes
   the app area; the FM-1 restarts into Strata. About a minute. **Do not unplug the FM-1 or switch it off** until the
   page says Done.
5. When the page asks, switch the FM-1 off and on. The page says **Done** only when Strata answers again after that
   restart.

Strata starts on its splash, then HOME.

## Update

To install a newer Strata, do the same again: the flasher finds the FM-1 running Strata and writes the new version.
Your presets, projects, drum library and settings stay where they are.

## Go back

- **To Felucca**: its own installer, [hugelton.github.io/Felucca](https://hugelton.github.io/Felucca/). It finds the
  FM-1 running Strata.
- **To the official firmware (V15)**: on the flasher, in **Going back**, pick M-VAVE's own `FM-1.fwsc` (checked by its
  SHA-256, never uploaded) and press **Return to official V15**. M-VAVE's own updater works too.
- **To exactly what you had**: write your backup back by the Flipper Zero route.

## If something goes wrong

- **A write stopped** (the cable came out, the browser closed): don't switch the FM-1 off. Replug it and press **Find
  the FM-1** again; it is still waiting in update mode and the page finishes the write.
- **STRATA CRASH** on the screen: take a photo. It restarts by itself; a second crash within 30 seconds of a start
  puts it in backup mode, where your backup or another firmware can be written.
- **Strata plays but no updater finds it**: use the Flipper Zero route, which enters backup mode at power-on whatever
  the firmware does.

More: the Manual's [Installing and going back](../manual/install.md) and [Troubleshooting](../manual/troubleshooting.md).

Next: [The cheat sheet](cheatsheet.md).
