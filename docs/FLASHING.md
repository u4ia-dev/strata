# Flashing Strata for a hardware test

This is the procedure for putting Strata's **hardware test build** (`V0.5 HARDWARE TEST`) on an FM-1, testing
it, and going back. Strata is installed the way Felucca is: by the FM-1's own USB-MIDI update path, from a web page
in Chrome or Edge. Installing custom firmware is at your own risk: read sections 2 and 7 before you start.

The first hardware test (`V0.4`, 2026-10-16) left the FM-1 unreachable by every updater; a Flipper Zero brought it
back. Section 9 says what happened and what changed; this build has the fix, and has not run on an FM-1 yet.
Section 8 lists what was checked where.

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
(the firmware, and Felucca's projects if you have Felucca), and it is the way back to exactly this state. Make it
**the same day as the install, by a route that enters backup mode at power-on**: that route still works when the
firmware's own USB does not, and making the backup proves it works on this computer.

**The flasher page makes it,** and installs nothing (Strata or V15) without one from the last 12 hours: start its
server in an administrator PowerShell (section 3, step 3), then its step 1.

1. **Back up now** on the page. Then backup mode by the **Flipper Zero route**
   ([merthsoft/flipper-fm1-recovery](https://github.com/merthsoft/flipper-fm1-recovery), used on 2026-10-17): its
   app `FM1 Recovery` (built for the Flipper's firmware: Momentum, API 86 here,
   `%USERPROFILE%\fm1-recovery\flipper\api86\fm1_boot_key.fap`, copied to `/ext/apps/Tools/`) sends the chip's USB
   key on the Flipper's own USB-C port while the FM-1 is switched on. The Flipper unplugged from the computer, the
   FM-1 charged and off, one USB-C data cable between them: **OK** in the app, switch the FM-1 on, wait for *Move FM1
   cable to PC now*, move the cable to the computer without switching the FM-1 off.

   **If Windows offers to format a disk: Cancel.** Never format the `WL82 UBOOT1.00` disk.
2. **The page does the rest** (its server, `serve.py`): it waits for the `WL82 UBOOT1.00` disk, runs
   flipper-fm1-recovery's `tools/pc_backup.py` on it (read-only: the chip key `0x980F` and flash `0x856014` checked,
   the whole flash read twice, two identical files required), checks the image's boot head (the flash header, its
   directory and the chip key, as `recovery_plan.py` checks a package), writes `backup.json` beside the two files in
   `%USERPROFILE%\fm1-recovery\backup-YYYYMMDD-HHMMSS`, and lets the FM-1 go. A USB device Windows could not read
   meanwhile is named on the page (on 2026-10-16 that was the FM-1). The install counts a backup only while both
   files still match its receipt.
3. **Leave backup mode:** switch the FM-1 off and on. Copy the backup folder somewhere else as well.

Other ways into backup mode work for the page's backup too (it waits for the disk, however it got there): the
page's **Put the FM-1 in backup mode** (the firmware's soft key `F0 22 24 35 7D F7`) and OCT− + OCT+ held 5 s on
Felucca or Strata. *Not enough on their own:* from Strata on 2026-10-16 the chip reached backup mode but Windows never
read it (*Unknown USB Device (Device Descriptor Request Failed)*, on a direct port and behind a hub), and it went back
to Strata about 14 s later; from the official firmware or Felucca on this computer: unverified. (Backup mode that no
computer has read ends by itself after about 13 s; once read, it stays until the FM-1 is switched off.)

**Backups made another way** are the same thing, a raw 1 MiB image of the flash from address 0, and any of these
tools can use any of them; the page counts only the ones it made. MvaveFM1Unbricker's `fm1_unbrick.py backup DIR`,
the FM-1 Transporter's `dump` (its computer talks to the XIAO, not to a disk, so the page cannot make that one), or
jl-uboot-tool by hand, as MvaveFM1Unbricker's UNBRICK-GUIDE.md does it. Find the device (an administrator
PowerShell):

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

## 3. Install

1. **The battery charged, the cable straight into the computer** (a data cable, no hub). Do not unplug it or touch
   the power switch while it writes: an update cut off by the FM-1 losing power can leave it needing the FM-1
   Transporter (MvaveFM1Unbricker's README reports exactly that).
2. **Close every other app that uses MIDI** (DAWs, M-UPGRADE, Felucca's editor): Windows lets one app at a time
   open a MIDI port.
3. **Serve the flasher** (it is already built in `build\flasher`; Web MIDI's SysEx needs a secure context, which a
   page on `127.0.0.1` is, and a page published on claude.ai is not allowed to use):

   ```powershell
   py %USERPROFILE%\fm1-dev\strata\build\flasher\serve.py
   ```

   in an **administrator** PowerShell (right-click Start, Terminal (Admin)): it reads the backup-mode disk for
   section 2, with flipper-fm1-recovery (`%USERPROFILE%\fm1-dev\fm1-flipper-recovery` with its `.local`;
   `--recovery DIR` for another). When it cannot, it prints `Backups: OFF` and why, and the page says so too. Open
   **<http://127.0.0.1:8000/>** in Chrome or Edge. The page checks the package (size, SHA-256, identity) and
   shows its SHA-256; it should match `Get-FileHash %USERPROFILE%\fm1-dev\strata\build\flasher\strata.fwsc`.
4. The backup (section 2) and the three boxes, then **Find the FM-1**; allow MIDI (with SysEx) when the browser asks. The page says
   what it found (for example `FM-1_015`, the official firmware, or a Felucca identity) before anything is
   written. **Write Strata**: step 1 (the firmware running now reads the package and starts the update loader),
   the restart into the loader, step 2 (the loader writes the app area), the restart into Strata. About a minute.
5. Strata starts: the splash (`STRATA`, `V0.5 HARDWARE TEST`), then OSC A's WAVE page.
6. **The restart check.** The page asks you to switch the FM-1 off, then on. It says **Done** only when Strata
   answers again after that (section 9: the first start after an install is the one start that hid the 2026-10-16
   fault). If it says *Strata did not answer after the restart*, stop: section 6, by the Flipper route.

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
10. **Optional:** OCT− + OCT+ for 5 s: the countdown from 2 s, then UBOOT. Note whether Windows shows the
    `WL82 UBOOT1.00` disk or an unknown device; unread, the FM-1 returns to Strata by itself after ~13 s.

## 5. The report

On the flasher page, step 3: **Read the device report** opens Strata's USB serial console (Chrome asks for the port:
the FM-1's) and reads `status` and `bench`; **Run BENCH's loops, then read** asks for RUN first. **Copy**, and paste
the text into the conversation with what you heard and saw, and photos of any crash screen. The same console works
in any serial terminal (115200 baud; type `help`). Its commands only read (`bench run` starts BENCH's loops);
`uboot yes` enters backup mode.

## 6. Going back

- **When no updater reaches the FM-1** (Strata plays, but the page's Find, Felucca's installer and M-UPGRADE get no
  answer; or the restart check failed): the Flipper Zero route, as on 2026-10-17. Backup mode by the Flipper and the
  backup (section 2), `pc_backup.py` left running; then flipper-fm1-recovery's `tools/pc_restore.py` with the
  package (the official `FM-1.fwsc`, a Felucca or a Strata package): first its offline plan, then, from a second
  administrator PowerShell, `--write --keeper-pid <pc_backup.py's PID>`. It rewrites only the sectors of
  `0x4000`–`0x92FFF` that differ, reads each back and then the whole flash, and never writes the boot head or the
  data above. *Upstream's `pc_restore.py` refuses the official V15 package* (`Invalid file extent`: its empty and
  resource entries); the fix used on 2026-10-17 is in `%USERPROFILE%\fm1-dev\fm1-flipper-recovery` (it takes the
  official package only by its SHA-256, and with `--require-v15` refuses a boot head that is not V15's).
- **To Felucca:** its installer, <https://hugelton.github.io/Felucca/>. It finds Strata's FM-1, because Strata keeps
  Felucca's update entry and USB name.
- **To the official V15:** on the flasher page, pick M-VAVE's unmodified `FM-1.fwsc` (its SHA-256 is checked in the
  page; nothing is uploaded), with a backup from the last 12 hours (section 2) and the power box ticked, **Return to
  official V15**. M-VAVE's own updater works
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
- **Strata runs but something is wrong:** Felucca's installer or the official V15 (section 6); if none of them
  finds the FM-1, the Flipper route (section 6, first item).
- **A sound stops the unit** after it was restored at power-on: hold **HOME** while switching on, and Strata starts
  without restoring it (after a crash it does that by itself, and says SAFE START).
- **The FM-1 does not appear on USB in any mode:** the Flipper route or the FM-1 Transporter: both enter backup mode
  at power-on, before any firmware runs, and the boot head they rely on is never written by any of these tools.

## 8. What was checked, and where

- **Built and checked by the build** (`tools/build.py --strata`): the image (362,796 of 581,564 bytes), RAM (48,288
  of 73,728 bytes), the pool (9.2 KiB spare, over the 8 KiB margin), RAM code with no calls into the flash, no
  register access outside `hal/`, the interrupt stack (~2.4 KiB of 7.9 KiB, from the listing), the package rebuilt
  byte for byte by the flasher's builder.
- **Verified on host and in the emulator:** the engine (114 checks, host and WebAssembly), the UI (78 checks), the
  MASTER level path, BENCH's page.
- **Verified in a browser against a simulated FM-1** (`web/strata_flash/flash_test.mjs`, 24 checks): the install and
  V15 off without a backup from the last 12 hours; Back up now (the token and the page's Origin sent; waiting,
  reading, verified; a device Windows could not read named), the server not an administrator, a backup 20 h old;
  the install over the official firmware and the resume in update mode, every byte served checked, each Done only
  after the simulated FM-1 was switched off and on and answered; a Strata mute after that restart never Done, the
  Flipper route named; the refusals (no FM-1, a device that is not one, a changed package, a file that is not V15);
  backup mode's confirmation; the report over a simulated console.
- **Verified on host** (`tests/serve_backup_test.py`, 13 tests, a fake `pc_backup.py` and disk watcher): the backup
  step verified with its receipt, the FM-1 let go; refused for two reads that differ, a backup that stops, another
  chip key, a boot head that does not check out; stopped while waiting with nothing read; no disk in time; the 12-hour
  rule and files changed after the receipt; why it is off; HTTP: other hosts, POSTs without the token or this
  page's Origin refused. The real disk watcher ran here once with no FM-1 (it ended on time, nothing found); with an
  FM-1 in backup mode it is **unverified** until the next backup.
- **Checked in the listing:** usb.c's `sie_wr` and `sie_rd` are out-of-line functions again, instruction for
  instruction Felucca's (only the `usb` struct's offsets differ); `V0.4` inlined them 12 times into the TIMER5
  interrupt. Against `V0.4`, function by function: 300 of 352 unchanged, every DSP function among them (`fx_eq` only
  gained two padding `nop`s); the changed ones are the drivers, the two that call them (`fm1_main`,
  `fm1_timer5_irq`) and the drivers that are calls again (`usb_poll`, `usb_retry`, `ota_service`, ...). The image is
  362,796 bytes; the interrupt stack ~2.5 KiB.
- **Needs this hardware test:** everything on the device, first of all **bulk IN after a power-on** (section 9):
  start-up, the audio's timing and sound, the panel, the screen, the flash's timings, USB MIDI and audio, the
  console, the interrupt and main stacks' real use, and the CPU rate (BENCH). **Unverified facts:** the cause in
  section 9; the soft key from the official firmware and Felucca on this computer; that the official firmware takes
  this package exactly as it takes Felucca's (the same format, loader and identity, so it should).

## 9. The 2026-10-16 lockout

**What happened** (`V0.4`, installed without a backup). The install said Done: Strata answered the handshake on its
first start. From the next start on, every start, the FM-1 received USB and sent nothing: notes played, the soft key
and the console's `uboot yes` arrived, but the handshake, the console's banner and every reply never came back
(EP0 worked: it enumerated; EP1 and EP3 bulk IN did not). With no reply, no updater could reach it. Its own way into
backup mode reached the chip's USB boot, which Windows could not read (`Device Descriptor Request Failed`), and the
FM-1 went back to Strata after ~14 s. On 2026-10-17 the Flipper Zero route entered backup mode at power-on, Windows
read it at once, the flash was backed up twice (identical; the boot head is V15's, the app area Strata's) and the
official V15 written back (143 sectors, verified).

**The likely cause** (unverified until this build runs). The first start after an install comes from the update
loader by a core reset, which leaves the USB controller as the loader set it; every later start is a chip reset.
Strata compiles Felucca's `usb.c` inside its own unit at `-O2`, the only Felucca-family firmware that does (Felucca and
every fork build it at `-Os`, `-O2` only for their DSP): `sie_wr` and `sie_rd`, the controller's register accesses
(a write to `CON1`, then a wait for `MC_ACK`), were inlined into the TIMER5 interrupt, the next access a few
instructions after the last where Felucca's calls leave a dozen. If the access right after an `INDEX` change still
reaches the old endpoint, `TXMAXP` (written first after `INDEX`) is never set for EP1 or EP3 while every receive
register (written later) is: bulk IN dead, OUT and EP0 fine, and the first start fine on the loader's settings. It
fits every observation; the mechanism is not proven. That `-O2` on `usb.c` is what breaks bulk IN is not this
project's guess alone: OberVave, another FM-1 firmware, bisected the same failure on 2026-10-13 (every packet on the
MIDI IN endpoint babbling, host-to-FM-1 fine, its first install fine) to exactly that, and went back to `-Os`
(its `tools/build.py` and `CLAUDE.md`). Which line `-O2` breaks is unknown there too.

**What changed.**

- `tools/build.py --strata` builds Felucca's drivers (`usb.c`, `midi_uart.c`, `ota.c`) at `-Os` inside Strata's `-O2`
  unit: the unit goes to LLVM IR, `tools/strata_drivers.py` marks those 57 functions `optsize` and keeps the 10
  that Strata's code calls from being inlined into it, and the IR is compiled at `-O2`. Unmarked, that round trip
  gives the same `strata.bin` byte for byte as before; marked, 300 of V0.4's 352 functions are unchanged and the
  synth's are all among them (section 8).
- `firmware/src/strata.c`: `sie_wr` and `sie_rd` declared `noinline, minsize` before `usb.c` is included, so they are
  Felucca's functions whatever the inliner does; `usb.c` itself stays Felucca's.
- The flasher's Done waits for a restart that is a real power-off (section 3, step 6).
- Backups by a route that enters backup mode at power-on, the same day (section 2); the way back when no updater
  answers (section 6).

**The rules from here.** Before any Strata build goes on the FM-1: a fresh double backup by the Flipper route (it
also proves the route on the day; the flasher page makes it and will not write without it, section 2). After it: the restart check, then the first report (section 5) after another
power-on. Felucca's drivers are not built at `-O2` again; a change to how they are compiled is checked in the
listing against Felucca's and counts as a change that needs this hardware test.
