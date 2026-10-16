# Flashing Strata for a hardware test

This is the procedure for putting Strata's **hardware test build** (`V0.4 HARDWARE TEST`) on an FM-1, testing
it, and going back. Strata is installed the way Felucca is: by the FM-1's own USB-MIDI update path, from a web page
in Chrome or Edge, with no extra hardware. Installing custom firmware is at your own risk: read sections 2 and 7
before you start.

Nothing in this build has run on an FM-1 yet. Section 8 lists what was checked where.

| | |
| --- | --- |
| Package | `build/strata.fwsc`, identity `FM-1_900` (Felucca's development identity, kept), 610,090 bytes |
| Writes | the app area `0x4000`–`0x92FFF` only, by Felucca's update loader (unchanged); never the boot head `0x0000`–`0x3FFF` |
| Strata itself writes | its presets `0x97000`–`0xCAFFF`, the sound playing `0xE5000`–`0xE6FFF`, its settings `0xFC000`–`0xFDFFF` (and, during an update, the loader's staging `0xE0000`–`0xE4FFF`): Felucca's own storage ranges, nothing else |
| Shows itself on USB as | "Felucca" (Felucca's descriptors, unchanged: both installers find the FM-1 by that name) |

## 1. What this build is

The Strata of Phase 4 on the FM-1: the wavetable engine, every page and layer of [CONTROLS.md](CONTROLS.md), SAVE
and the preset browser, the settings, on the FM-1's own flash; MIDI in over USB and the TRS jack; the FM-1 as a USB
audio input (what it plays, to the computer); MASTER; a USB serial console; and **SETTINGS > BENCH**, which measures
what this test is for. The platform is Felucca's, unchanged: the boot guard, the watchdog, the crash screen, the
update entry, the UBOOT entries.

Not in it yet: the factory presets (Phase 5; until then the eight starter sounds and INIT), MIDI clock, start and
stop, MIDI out, ARP, the second core.

## 2. Before anything: a full backup

Make a full 1 MiB backup of the flash, read twice, both copies equal, and keep it. It holds what is on the FM-1 now
(the firmware, and Felucca's projects if you have Felucca), and it is the way back to exactly this state.

1. **Backup mode.** The FM-1 has to be in the chip's own USB boot (a USB disk named `WL82 UBOOT1.00`). Either:
   - the flasher page's **Put the FM-1 in backup mode** button (section 3 starts the page). It sends the FM-1's
     soft key (`F0 22 24 35 7D F7`) to whatever firmware runs: the official one, Felucca or Strata. *Unverified on
     the official firmware by this project; Felucca's sources document it as the stock soft key.*
   - on Felucca or Strata: hold **OCT−** and **OCT+** together for 5 seconds.
   - or the way you usually do it.

   **If Windows offers to format a disk: Cancel.** Never format the `WL82 UBOOT1.00` disk.
2. **Read it twice** with jl-uboot-tool, as MvaveFM1Unbricker's UNBRICK-GUIDE.md does it. Find the device (an
   administrator PowerShell):

   ```powershell
   Get-CimInstance Win32_DiskDrive | Format-Table Index, Model, InterfaceType
   ```

   The line `WL82 UBOOT1.00 USB Device` gives the index N: the device is `\\.\PhysicalDriveN` (never use any other
   line's number). In jl-uboot-tool's folder:

   ```text
   python jluboottool.py --device '\\.\PhysicalDriveN'
   ```

   Quick info must say **Chip key: 0x980F** and **ID: 0x856014**; otherwise type `exit` and stop. Then:

   ```text
   =>JL: read 0 0x100000 backup.bin
   =>JL: read 0 0x100000 backup2.bin
   =>JL: exit
   ```

   ```powershell
   Get-FileHash backup.bin, backup2.bin
   ```

   The two hashes must be equal (else try another cable or port and read again). Keep `backup.bin` somewhere safe.
   MvaveFM1Unbricker's `python fm1_unbrick.py backup DIR` does the same steps for you.
3. **Leave backup mode:** switch the FM-1 off and on.

## 3. Install

1. **The battery charged, the cable straight into the computer** (a data cable, no hub). Do not unplug it or touch
   the power switch while it writes: an update cut off by the FM-1 losing power can leave it needing the FM-1
   Transporter (MvaveFM1Unbricker's README reports exactly that).
2. **Close every other app that uses MIDI** (DAWs, M-UPGRADE, Felucca's editor): Windows lets one app at a time
   open a MIDI port.
3. **Serve the flasher** (it is already built in `build\flasher`; Web MIDI's SysEx needs a secure context, which a
   page on `127.0.0.1` is, and a page published on claude.ai is not allowed to use):

   ```powershell
   python %USERPROFILE%\fm1-dev\strata\build\flasher\serve.py
   ```

   Open **<http://127.0.0.1:8000/>** in Chrome or Edge. The page checks the package (size, SHA-256, identity) and
   shows its SHA-256; it should match `Get-FileHash %USERPROFILE%\fm1-dev\strata\build\flasher\strata.fwsc`.
4. Tick the four boxes, press **Find the FM-1**, and allow MIDI (with SysEx) when the browser asks. The page says
   what it found (for example `FM-1_015`, the official firmware, or a Felucca identity) before anything is
   written. **Write Strata**: step 1 (the firmware running now reads the package and starts the update loader),
   the restart into the loader, step 2 (the loader writes the app area), the restart into Strata. About a minute.
   The page says **Done** when the FM-1 reports `FM-1_900` again.
5. Strata starts: the splash (`STRATA`, `V0.4 HARDWARE TEST`), then OSC A's WAVE page.

To rebuild the flasher after a change (WSL): `tools/build.py --strata`, then
`python3 web/strata_flash/make_flasher.py` (it refuses a package that is not exactly the app and the loader just
built).

## 4. What to test

Note what you see and hear, take photos of anything odd, and read the report (section 5) at the end.

1. **Start-up.** Time from power on to the first page; the splash; whether it starts every time (try 3 power-ons).
2. **Sound.** Play every starter sound (PRESETS steps them). Listen for clicks, crackle, dropouts, aliasing (a
   high note's metallic whine), DC thumps, and how a note ends. Turn MASTER: the level follows, without zipper noise.
3. **Load.** HOME's header shows CPU %. On a few sounds (SUPERSAW's unison, a pad with every effect on), hold 1, 2,
   4 and 8 keys and note the CPU % and whether notes drop out (the guard sheds voices over 85 %).
4. **BENCH.** Hold HOME 5 s for the SETTINGS, turn SELECT to the last page, BENCH. Its rows: AUDIO (a half's
   render: the last and the longest, and how many came late), CPU (load, the guard's steps, voices shed), SCREEN (a
   frame's drawing), KEYS (the longest the keys waited), TABLES (a wavetable build: its longest step and the whole),
   FLASH (the longest erase and page), STACKS, RUN. Turn **KNOB 1** for RUN (two loops timed, a moment's silence);
   **KNOB 2** clears the peaks. Do this after the load test, then after saving a few presets.
5. **The panel.** Every button, every key, every encoder (both directions, slow and fast), MASTER; the LEDs (the keys
   on SCL show the scale; PLAY's green LED blinks the tempo while it runs). The hold layers (GLO bend, FX macros,
   LFO momentary, EDIT jump), the MOD layer (REC).
6. **The screen.** Every page draws; the live pages (HOME's scope, an envelope, an LFO) move while notes play and
   stop when they end; a turned knob's value follows without lag.
7. **Saving.** SAVE once (a new version), SAVE twice (the browser), SAVE held 5 s (over its own slot), a rename in
   NAME IT; switch off and on: the sound playing comes back, the user presets are listed. Undo/redo (SAVE + OCT−/+).
8. **MIDI.** From a DAW over USB: notes, pitch bend, the mod wheel, CC 74 (the cutoff), a program change with
   PROGRAM on; MIDI LEARN. The TRS jack if you have a device for it.
9. **USB audio.** Record the FM-1 in the DAW (an audio input named after the USB name): clean, no gaps.
10. **Optional:** OCT− + OCT+ for 5 s: the countdown from 2 s, then UBOOT; switch off and on to return to Strata.

## 5. The report

On the flasher page, step 3: **Read the device report** opens Strata's USB serial console (Chrome asks for the port:
the FM-1's) and reads `status` and `bench`; **Run BENCH's loops, then read** asks for RUN first. **Copy**, and paste
the text into the conversation with what you heard and saw, and photos of any crash screen. The same console works
in any serial terminal (115200 baud; type `help`). Its commands only read (`bench run` starts BENCH's loops);
`uboot yes` enters backup mode.

## 6. Going back

- **To Felucca:** its installer, <https://hugelton.github.io/Felucca/>. It finds Strata's FM-1, because Strata keeps
  Felucca's update entry and USB name.
- **To the official V15:** on the flasher page, pick M-VAVE's unmodified `FM-1.fwsc` (its SHA-256 is checked in the
  page; nothing is uploaded), tick the backup and power boxes, **Return to official V15**. M-VAVE's own updater works
  too.
- **To exactly what you had,** from your backup, with jl-uboot-tool in backup mode (section 2). Every range below is
  4 KiB aligned (jl-uboot-tool erases whole blocks); never write below `0x4000`, and leave the rest of the flash
  (Bluetooth pairing at `0xE9000`, the unit's key at `0xFF000`, the stock areas) alone. Cut the pieces out of the
  backup:

  ```powershell
  python -c "d=open('backup.bin','rb').read(); open('fw.bin','wb').write(d[0x4000:0x93000]); open('store.bin','wb').write(d[0x97000:0xE7000]); open('glob.bin','wb').write(d[0xFC000:0xFF000])"
  ```

  Then write the firmware region, and, if you want Felucca's projects and settings back as they were, the two
  storage ranges:

  ```text
  =>JL: write 0x4000 fw.bin
  =>JL: write 0x97000 store.bin
  =>JL: write 0xFC000 glob.bin
  =>JL: read 0x4000 0x8F000 fw-check.bin
  =>JL: read 0x97000 0x50000 store-check.bin
  =>JL: read 0xFC000 0x3000 glob-check.bin
  =>JL: exit
  ```

  Each `-check.bin` must have the same SHA-256 (`Get-FileHash`) as the file written; if not, write it again before
  switching off. Then switch off, wait 5 s, switch on.

## 7. If something goes wrong

- **A write stops** (the page reports it, the cable came out): do not switch the FM-1 off. Replug, press **Find the
  FM-1** again: it is still in update mode, and the page finishes the write.
- **STRATA CRASH** on the screen: a photo, please. It restarts after 4 s; a second crash within 30 s of a start
  puts it in backup mode by itself (the boot guard), where your backup or the official firmware can be written.
- **A frozen screen, no crash screen:** the watchdog restarts it within ~8 s; two such boots and the boot guard puts
  it in backup mode. Leave it on for two minutes before anything else.
- **Strata runs but something is wrong:** Felucca's installer or the official V15 (section 6), or backup mode by
  OCT− + OCT+ for 5 s.
- **A sound stops the unit** after it was restored at power-on: hold **HOME** while switching on, and Strata starts
  without restoring it (after a crash it does that by itself, and says SAFE START).
- **The FM-1 does not appear on USB in any mode:** MvaveFM1Unbricker's README, "The FM-1 does not show up", then the
  FM-1 Transporter (its TRANSPORTER-GUIDE.md).

## 8. What was checked, and where

- **Built and checked by the build** (`tools/build.py --strata`): the image (370,944 of 581,564 bytes), RAM (48,288
  of 73,728 bytes), the pool (9.2 KiB spare, over the 8 KiB margin), RAM code with no calls into the flash, no
  register access outside `hal/`, the interrupt stack (~2.4 KiB of 7.9 KiB, from the listing), the package rebuilt
  byte for byte by the flasher's builder.
- **Verified on host and in the emulator:** the engine (114 checks, host and WebAssembly), the UI (78 checks), the
  MASTER level path, BENCH's page.
- **Verified in a browser against a simulated FM-1** (`web/strata_flash/flash_test.mjs`, 16 checks): the install over
  the official firmware and the resume in update mode, every byte served checked; the refusals (no FM-1, a device
  that is not one, a changed package, a file that is not V15); backup mode's confirmation; the report over a
  simulated console.
- **Needs this hardware test:** everything on the device: start-up, the audio's timing and sound, the panel, the
  screen, the flash's timings, USB MIDI and audio, the console, the interrupt and main stacks' real use, and the CPU
  rate (BENCH). **Unverified facts:** the official firmware's soft key (Felucca documents it); that the official
  firmware takes this package exactly as it takes Felucca's (the same format, loader and identity, so it should).
