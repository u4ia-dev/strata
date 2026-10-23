# Installing and going back

Read the [disclaimer and the data warning](../README.md#disclaimer) first: installing any firmware other than the
official one is at your own risk, and saving in Strata can overwrite Felucca's projects and samples.

## How an install works

Strata installs the way Felucca does: through the FM-1's own **USB-MIDI update path**, from a web page. The firmware
running now (the official one, Felucca or Strata) reads the package over MIDI and starts its **update loader**; the
loader writes the app area only (0x4000 – 0x92FFF); the FM-1 restarts into Strata. The boot area and your data are never
written. It takes about a minute.

You need a computer with **Chrome or Edge** (Web MIDI with SysEx), a **USB data cable** straight into it (no hub), the
FM-1's battery **charged**, and every other MIDI app **closed** (DAWs, M-UPGRADE, Felucca's editor): Windows lets one
app at a time open a MIDI port.

## A full backup

A full flash backup is 1 MiB, read twice with both copies equal: the only way back to exactly what you have now. Make
it **the same day as the install**, by a way that enters backup mode at power-on, which still works when a firmware's
own USB does not. The flasher recommends it before your first Strata (it is no longer required) and shows the last
backup's date.

**With the flasher's step 1**: backups are made on your own computer, not on the website. Run the flasher from the
Strata source with its server, `serve.py`, in an **administrator** PowerShell, and open it at `http://127.0.0.1:8000/`
(the server reads the backup-mode disk with the flipper-fm1-recovery tools; when it cannot, it says "Backups: OFF" and
why). Then:

1. Press **Back up now**, then put the FM-1 in backup mode by the **Flipper Zero route**: on the Flipper (unplugged from
   the computer), Apps > Tools > **FM1 Recovery**; the FM-1 switched off, a USB-C data cable from the Flipper to the
   FM-1; **OK** on the Flipper, switch the FM-1 on; at *Move FM1 cable to PC now*, move the cable to the computer
   without switching the FM-1 off.
2. **If Windows offers to format a disk: Cancel.** Never format the `WL82 UBOOT1.00` disk.
3. The page does the rest: it waits for the disk, reads the whole flash twice (read-only, the chip checked first),
   checks the image, writes `backup.json` beside the two files in a dated folder, and lets the FM-1 go.
4. Switch the FM-1 off and on to leave backup mode. Copy the backup folder somewhere else as well.

**Other ways into backup mode** work for the page too (it waits for the disk however it got there): the page's **Put
the FM-1 in backup mode** (the SysEx soft key), or <kbd>OCT−</kbd> and <kbd>OCT+</kbd> held 5 s on Felucca or Strata.
They are not enough on their own: from Strata, on one computer, the chip reached backup mode but Windows never read it
and it went back to Strata about 14 s later. Backup mode that no computer has read ends by itself after about 13 s.

**Other tools** make the same raw image (the page counts only its own): MvaveFM1Unbricker's `fm1_unbrick.py backup DIR`,
the FM-1 Transporter's `dump`, or jl-uboot-tool by hand. With jl-uboot-tool, in backup mode, find the disk whose model
is `WL82 UBOOT1.00 USB Device` (an administrator PowerShell: `Get-CimInstance Win32_DiskDrive | Format-Table Index,
Model, InterfaceType`; its index N gives `\\.\PhysicalDriveN`, never another line's), then:

```text
python jluboottool.py --device '\\.\PhysicalDriveN'
```

Quick info must say **Chip key: 0x980F** and **ID: 0x856014**; otherwise type `exit` and stop. Then:

```text
=>JL: read 0 0x100000 backup.bin
=>JL: read 0 0x100000 backup2.bin
=>JL: exit
```

The two files' SHA-256 (`Get-FileHash backup.bin, backup2.bin`) must be equal; else try another cable or port and read
again. The full procedure is in the source's docs/FLASHING.md, section 2.

## Install

1. Open the **[Strata flasher](../../flash/)** in Chrome or Edge. It checks its package (size, SHA-256, identity) and
   shows the SHA-256.
2. **1 / 3 Before you start**: the backup (recommended), and the three boxes: the battery charged and the cable straight
   in, the other MIDI apps closed, the data warning understood.
3. **2 / 3 Install**: **Find the FM-1**; allow MIDI with SysEx when the browser asks. The page says what it found (the
   official firmware, Felucca or Strata) before anything is written.
4. **Write Strata**: step 1 (the firmware running now reads the package and starts the update loader), the restart into
   the loader, step 2 (the loader writes the app area), the restart into Strata. **Do not unplug the FM-1 or touch its
   power switch** until the page says Done.
5. **The restart check**: the page asks you to switch the FM-1 off, then on. It says **Done** only when Strata answers
   again after that restart. If it says *Strata did not answer after the restart*, stop and go back by the Flipper Zero
   route (below).
6. **3 / 3 Test and report** (for hardware testers): **Read the device report** and **Run BENCH's loops, then read** open
   Strata's serial console (Chrome asks for the port: the FM-1's) and read `status` and `bench`; **Copy** puts the text
   on the clipboard.

Strata starts on its splash, then HOME. A unit new to Strata starts on INIT.

## Updating

Install the newer version the same way: the flasher finds the FM-1 running Strata. The update writes the app area only;
your presets, projects, drum library and settings stay. Read the [Changes](../changes.md) page for what is new; a
version that changes how something is kept says so there.

## Going back

- **To Felucca**: Felucca's own installer, [hugelton.github.io/Felucca](https://hugelton.github.io/Felucca/). It finds
  the FM-1 running Strata (Strata keeps Felucca's update entry, and its USB name has "FM-1" in it as the installers
  look for).
- **To the official V15**: on the flasher, **Going back**: pick M-VAVE's unmodified `FM-1.fwsc` (its SHA-256 is checked
  in the page; nothing is uploaded), tick the power box (a backup recommended), **Return to official V15**. M-VAVE's own
  updater works too. No M-VAVE file is part of Strata: you download it from M-VAVE.
- **To exactly what you had**: your backup, written back in backup mode. With jl-uboot-tool, cut the pieces out of the
  backup and write only those (every range 4 KiB aligned; never below 0x4000; leave the rest alone):

  ```powershell
  python -c "d=open('backup.bin','rb').read(); open('fw.bin','wb').write(d[0x4000:0x93000]); open('store.bin','wb').write(d[0x97000:0xE7000]); open('glob.bin','wb').write(d[0xFC000:0xFF000])"
  ```

  ```text
  =>JL: write 0x4000 fw.bin
  =>JL: write 0x97000 store.bin
  =>JL: write 0xFC000 glob.bin
  =>JL: read 0x4000 0x8F000 fw-check.bin
  =>JL: read 0x97000 0x50000 store-check.bin
  =>JL: read 0xFC000 0x3000 glob-check.bin
  =>JL: exit
  ```

  Each `-check.bin` must have the same SHA-256 as the file written; if not, write it again before switching off. Then
  switch off, wait 5 s, switch on. (`store.bin` and `glob.bin` bring back the data area and the settings as they were:
  leave them out to keep what Strata saved.)
- **When no updater reaches the FM-1** (Strata plays, but the flasher's Find, Felucca's installer and M-UPGRADE get no
  answer, or the restart check failed): the **Flipper Zero route**. Backup mode by the Flipper and a backup first, then
  flipper-fm1-recovery's `pc_restore.py` with a package (the official `FM-1.fwsc`, a Felucca or a Strata package): it
  rewrites only the app area's sectors that differ, reads each back and then the whole flash, and never writes the boot
  area or the data above. (Its upstream version refuses the official V15 package; the source's docs/FLASHING.md,
  section 6, says how that was solved.)

## If something goes wrong

See [Troubleshooting](troubleshooting.md#installing).
