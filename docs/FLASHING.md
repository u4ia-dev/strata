# Flashing Strata for a hardware test

This is the procedure for putting Strata's **hardware test build** (`V0.87 UI`: V0.87's controls and windows on V0.86's interface and drum editor, on the STRATA UI of V0.67, SCL's chords, the ARP, the 8-track looper with V0.81's fixes,
its drum synth and sidechain, and CORE 2, the second core measured: [CONTROLS.md](CONTROLS.md) "Buttons", "The
looper", "The drums", "SETTINGS") on an FM-1, testing it, and going back. Strata is installed the way Felucca is: by
the FM-1's own USB-MIDI update path, from a web page in Chrome or Edge. Installing custom firmware is at your own
risk: read sections 2 and 7 before you start.

The first hardware test (`V0.4`, 2026-10-16) left the FM-1 unreachable by every updater; a Flipper Zero brought it
back. Section 9 says what happened and what changed. `V0.5` (2026-10-17) had the fix: it passed the flasher's restart
check and its report came back over USB, so bulk IN works after a power-on (**verified on hardware**, the V0.5
report). `V0.6` is the build after that test: the FX chain, seven new effects and the UI changes the test asked for
(section 1); it installed (2026-10-17), and its test waited for `V0.65`, the memory audit ([BUDGET.md](BUDGET.md)),
which ran on the FM-1 on 2026-10-18 (the test page's report); `V0.66` is the build after that report; V0.86 and V0.87 are
not yet run on an FM-1. Section 8 lists what was checked where.

| | |
| --- | --- |
| Package | `build/strata.fwsc`, identity `FM-1_900` (Felucca's development identity, kept); its size and SHA-256 on the flasher page |
| Writes | the app area `0x4000`–`0x92FFF` only, by Felucca's update loader (unchanged); never the boot head `0x0000`–`0x3FFF` |
| Strata itself writes | its 64 presets `0x97000`–`0xB0FFF` (V0.8; 128 before, to `0xCAFFF`), the looper's projects 1 and 2 `0xCB000`–`0xDAFFF` and 3 and 4 `0xB1000`–`0xC0FFF` (V0.8; 3 and 4 never where older presets are), the sound playing `0xE5000`–`0xE6FFF`, its settings `0xFC000`–`0xFDFFF` (and, during an update, the loader's staging `0xE0000`–`0xE4FFF`): Felucca's own storage ranges, nothing else |
| Shows itself on USB as | "Strata for FM-1" by u4ia, `1209:5354`; its update loader "Strata Update for FM-1", `1209:5355` (pid.codes; [RELEASE.md](RELEASE.md)). Both installers look for "FM-1" in the name (**needs hardware test**: step 58) |

## 1. What this build is

The Strata of Phase 4 on the FM-1: the wavetable engine, every page and layer of [CONTROLS.md](CONTROLS.md), SAVE
and the preset browser, the settings, on the FM-1's own flash; MIDI in over USB and the TRS jack; the FM-1 as a USB
audio input (what it plays, to the computer); MASTER; a USB serial console; and **SETTINGS > BENCH**, which measures
what this test is for. The platform is Felucca's, unchanged: the boot guard, the watchdog, the crash screen, the
update entry, the UBOOT entries.

**New in V0.6** (from the V0.5 test; [CONTROLS.md](CONTROLS.md)): the effects as a **chain** of up to eight modules
(add, take out, move, two of a type; the chain view on FX), seven new effects (FLANGER, PHASER, an FX FILTER of 21
types, BITCRUSH, TREMOLO / AUTOPAN, RING MOD, TAPE), SELECT picks the section and ALGORITHM the page, no ON knobs
(EDIT + OCT turns a source on or off, the FX-held keys bypass a module), the matrix on GLO, deleting a user preset in
the browser (and putting it back). Two fixes from V0.5's report: the keys and MIDI are read while a redraw waits for
the screen (the keys waited 91 ms), and the CPU guard steps at once when a half nears its deadline (5 halves were
late). Presets saved by V0.5 load, their effects that were on as the chain.

**New in V0.65** (the memory audit, [BUDGET.md](BUDGET.md); nothing on the panel changes): 83 KB less code (the UI,
the console and the presets' store built for size, the sound's code and the interrupts as before), the main stack 12
KiB (it used 17.9 KiB on V0.5: one 17 KiB frame, gone), the pool 12 KiB larger, RAM free 30 KiB (V0.6: 12). Two V0.6
bugs fixed: the FX FILTER's, BITCRUSH's, TREMOLO's, RING MOD's and TAPE's knobs changed nothing while a sound played
(only a load took them), and RING MOD's header showed odd characters after its name. And 44 KiB of SRAM Strata does
not use are watched, for later versions: BENCH's STACKS row ends in SPARE OK, or SPARE HIT if anything wrote there.

**New in V0.66** (from V0.65's report; nothing on the panel changes but BENCH's KEYS and SCREEN rows): big chords no
longer crackle (a note gets a free voice only while the time it needs fits: else it steals, as with every voice
taken; two notes start a block); the chain's changes fade over ~6 ms (a move dips ~12 ms), CHORUS, FLANGER, TAPE and
the FX FILTER switched in only once their lines hold the sound; the screen's SPI 5x faster (the clock other FM-1
firmwares run); the keys played from the scan's timer while they only play, so drawing and saving no longer hold them
up; the USB audio 1.45 ms deeper; VOWEL PAD's hiss down (its noise was 6.6 dB under the pad); BENCH counts late halves
after flash work apart and keeps an event log (the console's `bench`); the undo history and the FX lines in the spare
SRAM V0.65 found untouched.

**New in V0.85 DRUMS** (stage 2 of [LOOPER.md](LOOPER.md); [CONTROLS.md](CONTROLS.md) "The drums"): a track's TYPE
can be DRUMS: it plays the project's drum kit, eight drums of their own models (KICK, SNARE, CLAP, HAT CL, HAT OP, CYM,
and WT 1 and WT 2 on the oscillators' wavetables), each with TUNE, DECAY, TONE, its own, LEVEL and PAN on EDIT's KIT
pages. Its keys play the drums (white keys 9-16 accented, the first black key DUCK). Its clips are hits with ACCENT,
CHANCE and RATCHET, recorded as played or put in on SEQ > CLIP's GRID (the white keys the sixteen steps, the black keys
the rows, their LEDs showing them). The DUCK row's silent hits duck the synth (KIT > DUCK: AMOUNT, ATTACK, RELEASE,
SHAPE): the sidechain. The kit is kept in the project; projects V0.8 or V0.81 saved load with a new kit. Package
`build/strata.fwsc`, 610,090 bytes, sha256 `f52c25b848f4c572a3d1272f5d43d7957b0d1178120fa9622dff3dc487e71ded`. It has
V0.81's fixes (below).

**New in V0.81** (V0.8 LOOPER debugged and trimmed before its test; nothing on the panel changes): USB MIDI out no
longer leaves a note on: when its queue fills (a computer reads the port only while an app has it open) and a note is
lost, an All Notes Off goes out on that channel once there is room, as the official firmware does; the engine does
1-8 % less work for the same sound, bit for bit (the matrix and the parameters' scan); 1 KB of RAM back and the image
1.2 KB smaller; two of the looper's internal states that could go stale (a take's start, a launch while stopped: no
sound came of them) and a write past an array on DELAY's page, found by a new stress test. [BUDGET.md](BUDGET.md)
§14, [TESTING.md](TESTING.md) §5.5. Package `build/strata.fwsc`, 610,090 bytes, sha256
`c370d0a740074f419688529c212d9e4c514fc23918ca65965c175635bd792220`.

**New in V0.8 LOOPER** (stage 1 of [LOOPER.md](LOOPER.md); [CONTROLS.md](CONTROLS.md) "The looper"): V0.8's
sequencer (never installed) rebuilt as an 8-track looper. SEQ opens LOOP (CLIPS, MIXER), TRACK (TRACK, RECORD,
CLICK), CLIP (NOTES, EDIT, CLIP) and SONG (SONG, PROJECT). Eight tracks, each SYNTH (the sound playing), SYNTH+MIDI or
MIDI (out over USB on its channel), each with its own SCALE, chords and ARP; eight scenes, a clip each track, one to
sixteen bars of notes and knob moves at the times they were played. REC on the selected track: an empty clip, a take
(stopped: a bar of count-in clicks) that REC ends rounded to whole bars, looping at once; a clip, overdub pass after
pass, UNDO PASS taking the last back. Launching a clip or a scene waits for the bar; SEQ held is the launch pad (white
keys 1-8 the tracks' clips, 9-16 the scenes; black keys MUTE, STOP ALL, SOLO, UNDO PASS); SEQ + OCT+ / OCT− the next
track. MIXER: VOLUME, PAN, MUTE, SOLO. SONG chains scenes with repeats. Four projects, kept in the flash (written when
stopped and quiet, only when changed). MIDI in plays the selected track, or a track whose MIDI IN is its channel.
**The user presets are 64 now** (they were 128): a preset an older build saved in slots 65 to 128 is no longer shown,
and stays in the flash (projects 3 and 4 are not saved over it; slots 65-104 are where they go). Package
`build/strata.fwsc`, 610,090 bytes, sha256 `5bbbe25573755844440b2c132ac8dd40ec6d37c7af2845041a9bac5303b4f4cc`. It also has
**CORE 2** (V0.8 BENCH, below).

**V0.8 BENCH** (stage 0 of the looper, [LOOPER.md](LOOPER.md); [CONTROLS.md](CONTROLS.md) "CORE 2"): the
second core, built in and **off by default**, measured on your FM-1 before the looper's V0.9 puts voices on it.
SETTINGS > CORE 2: ON starts core 1 at the next power-on (Felucca's way: X0X's mailbox with Melodee's and Jangada's
hardening, no supply raise); TEST runs 30 s while something plays: core 1 renders wavetable oscillators while core 0
plays, then core 0 the same work alone, and the page shows core 1's time against core 0's, how much core 0 slowed while
core 1 ran, core 0's longest wait, the jobs and any it would not run (BAD), and core 1's stack. A unit whose core 1
misreads or stops answering is held, plays on one core and turns CORE 2 off (some FM-1s do: fm1-x0x#10); never
after a boot that crashed, or with HOME held at power-on. The voices are built exactly as V0.8's (the test reads the
oscillators' own kernels with state of its own). Package `build/strata.fwsc`, 610,090 bytes, sha256
`1ece869f4da115fd79e766b4e3f062d1f5380e1f3b4baaab5b6a514e083286ed`.

**V0.8, the sequencer** (built 2026-10-21, never installed: the looper above replaced it): the SEQ button's menu (SEQ: NOTES, FEEL,
MOTION; PATTERN: PATTERN, PLAY+REC; CHAIN). Sixteen patterns of up to 64 steps, up to four notes a step, each step's
VELOCITY, LENGTH, CHANCE and NUDGE; a pattern's LENGTH, RATE, SWING, DIRECTION and TRANSPOSE; two MOTION lanes a
pattern (a parameter held at a step's value; the sound itself untouched); CHAIN of up to sixteen slots. PLAY plays the
pattern shown, on GLOBAL's tempo or MIDI's clock. REC's tap arms recording: live while PLAY runs (the keys and MIDI
in, a knob's turns into a lane), by step when stopped on SEQ's steps. SEQ + OCT+ / OCT− the next pattern, the one
before; SEQ held, PATTERNS (a white key picks one, F#3 COPY, G#3 PASTE, A#3 CLEAR). REC twice no longer latches the
MOD layer (REC + OCT+ does). The patterns are kept in the flash at `0xCB000`–`0xD2FFF` (four sector pairs of the user
store's free part, allow-listed as the presets are), written once 3 s pass without sound with the transport stopped.
Nothing plays until you write a pattern and press PLAY. Package `build/strata.fwsc`, 610,090 bytes, sha256
`571c920be293e0667831d2fa9367f5512669896236cdb580c65a328ca22144fa`.

**New in V0.75** (the ARP and one grammar for the menus; [CONTROLS.md](CONTROLS.md) "Buttons", "Chords", "The ARP"):
the ARP button opens the ARP's menu (STYLE, GROOVE; STEPS, RATCHET), ARP + OCT+ turns it on and ARP + OCT− off from
any page; ARP is a step arpeggiator (thirteen MODEs, 1-4 OCTAVES, dotted and triplet RATEs; sixteen steps, each PLAY,
TIE or REST with its LEVEL, OCTAVE, RATCHET, CHANCE and SLIDE; GATE, SWING and LATCH; on PLAY's beat; GLOBAL's CLOCK
IN MIDI). Every menu button: a tap its menu, again its next page, twice its next section, + OCT+ / OCT− on / off,
held 5 s what it shows reset; SELECT the section, ALGORITHM the page everywhere. The chords by knobs only (FREE: 23
TYPEs, KEY: 8 SHAPEs, SPREAD) with STRUM (gap, DIRECTION, HUMAN, TAPER); the ARP's styles ARP, PATTERN, HARP. A knob's
value: where the page shows it, vermilion there; else in the bottom row (no flag over the page). HOME's bloom plainer
(106 seeds). All off by default, as V0.7's. Package `build/strata.fwsc`, 610,090 bytes, sha256
`60b77f007564799466398cfbcda8b0cce1ae10512b262afebac06291ce990103`.

**New in V0.7** (the chords, after the Orchid's ideas; [CONTROLS.md](CONTROLS.md) "Chords"): SCL's pages CHORD
(KEY: a key plays its in-key chord; FREE: F3–C4 eight latching chord buttons, C#4–G5 the roots; VOICING with AUTO voice
leading), PERFORM on the ARP button (STRUM, STRUM 2 OCT, ARP, PATTERN, HARP, SLOP, from GLOBAL's tempo), BASS (the root
under the chord) and MIDI (MIDI notes in as roots; the chords out over USB MIDI, channels 1 and 2). All off by default:
the keys play as before until you turn them on. Package `build/strata.fwsc`, 610,090 bytes, sha256 `6d0fab73c47304e9fc6164c37ec028a95ba37847fd97925ab588743c9e9e4550`.

**New in V0.67** (from the UI build's install, "some of the text is too small"; [UI.md](UI.md)): nothing on the screen
under 9 px (the labels 7 → 9, the top line and the zones 8 → 10, the flags 9 → 12, a module's word 12 → 14), the
labels in their own brighter colour on every theme, larger handles, chips and zones, the how-to lines gone from the
screens, the MATRIX four slots at a time, the browser's presets at 12 px; **VALUES**, a zone at the end of every
module's pages: its knobs' values large, a knob there stepping the finest unit; four more themes (MOSS, DUSK, SAND,
NEON); HOME's bloom as a sunflower that opens with the sound and swirls at each note. RAM, the pool and the engine are
V0.66 UI's; the image 12.8 KB larger (the fonts). Package `build/strata.fwsc`, 610,090 bytes, sha256
`8219aeea6023bbd3b1794d24df6133ff348f9c0017b244a9a33d91900b46c3d9`.

**The UI build** (`strata-ui`, `V0.66 UI HARDWARE TEST`, installed 2026-10-18): V0.66 with the STRATA UI ([UI.md](UI.md)): one screen a
module, the knobs' numbered handles on what each moves, a value only while its knob turns, Space Mono, warm white on
black with one vermilion accent, HOME's bloom in place of the scope, the long holds as countdown screens. The engine,
the panel's controls, the presets and settings, the package, its identity and the flash it writes are V0.66's. Its
package: `build/strata.fwsc`, 610,090 bytes, sha256 `5638a66636d7d14e6e80270c62ade1467ba93c17abe685c8bcae3a028fdef2a8`.

Not in it yet: the factory presets (Phase 5; until then the eight starter sounds and INIT), a knob's CC out over MIDI,
a sound a track, inserts and sends and voices on the second core (V0.9), the drum synth and the sidechain (V0.85:
[LOOPER.md](LOOPER.md)).

## 2. Before anything: a full backup

> **Your saved data.** Strata keeps its presets, projects and drum library in the FM-1's data area, in the same
> flash where Felucca keeps its projects and samples (0x97000 to 0xDF000, inside the store's allow-list). Installing
> Strata does not erase that area, but **saving anything in Strata can overwrite Felucca's projects and samples**, and
> going back to Felucca or to the official firmware does not bring them back. Make a full flash backup before you
> install (docs/FLASHING.md, section 2) and keep it: it is the only way back to exactly what you had. Strata never writes
> the boot area, the calibration and trim data, Bluetooth pairing or the unit's own keys.

> **Disclaimer.** Strata is provided **as is, without any warranty**, under the GNU General Public License version 3 (sections 15 and
> 16: no warranty, no liability). Installing any firmware other than the official one can leave the FM-1 unusable until
> it is recovered, and you install Strata at your own risk. Strata is a **pre-release**: its versions from V0.7 on are
> being tested on the hardware now.

Make a full 1 MiB backup of the flash, read twice, both copies equal, and keep it. It holds what is on the FM-1 now
(the firmware, and Felucca's projects if you have Felucca), and it is the way back to exactly this state. Make it
**the same day as the install, by a route that enters backup mode at power-on**: that route still works when the
firmware's own USB does not, and making the backup proves it works on this computer.

**The flasher page makes it** (V0.87: recommended, no longer required; the page shows the last backup's date): start
its server in an administrator PowerShell (section 3, step 3), then its step 1 (the Flipper steps fold out).

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
5. Strata starts: the splash (two blooms over `STRATA` and `WAVETABLE · FM-1 · V0.87 UI`) for 2 s, then HOME.
6. **The restart check.** The page asks you to switch the FM-1 off, then on. It says **Done** only when Strata
   answers again after that (section 9: the first start after an install is the one start that hid the 2026-10-16
   fault). If it says *Strata did not answer after the restart*, stop: section 6, by the Flipper route.

To rebuild the flasher after a change (WSL): `tools/build.py --strata`, then
`python3 web/strata_flash/make_flasher.py` (it refuses a package that is not exactly the app and the loader just
built).

## 4. What to test

Note what you see and hear, take photos of anything odd, and read the report (section 5) at the end.

**The test page does most of it** (V0.65): with the flasher served (section 3: `serve.py`; an administrator
PowerShell is only needed for the backup step), open **<http://127.0.0.1:8000/test.html>** in Chrome or Edge, close the
flasher's tab first (one page at a time can open the FM-1's ports), and work down it. It connects to the FM-1's MIDI
port, its console and, if you want, its USB audio, checks that the FM-1 runs the very build the page was made for (a
fingerprint of the app's bytes read over the console), then:

- **runs by itself** (about two minutes): a clean start (no crash record, no safe start), the spare SRAM (item 18), a
  note on and off (item 8), CC 74 moving the cutoff and the sound following it (read from Strata's RAM), program
  changes, the load test on every starter sound with 1, 2, 4 and 8 notes (item 3: voices, CPU, the guard, late halves,
  the gaps in the USB audio), BENCH RUN against V0.5's figures (item 4), the stacks (item 17), the USB audio's counters;
- **guides**, and checks what it can: the five fixed effects' knobs, read from RAM while you turn them (item 16), the
  chain's adds, take-outs, moves, bypasses and undo, read from the patch (item 11), the encoders and keys (item 5), the
  keys' wait and a frame's drawing while you flick through pages (items 15, 19), three restarts (item 1);
- **asks** what only you can judge (the sound, the screen, SAVE and the browser), with a note each.

It plays notes, moves the cutoff and loads the starter sounds by program change (PROGRAM on: SETTINGS > MIDI, KNOB 4),
so SAVE your sound first; it puts a user preset back at the end. It sends no SysEx and only the console's read-only
commands (`status`, `bench`, `bench run`, `inp`, `crash`, `memr`), and writes nothing to the flash. Its report (copy, or
save as a file) is what to paste into the conversation. The list below is the whole test, for reference.

1. **Start-up.** Time from power on to the first page; the splash; whether it starts every time (try 3 power-ons).
2. **Sound.** Play every starter sound (PRESETS steps them). Listen for clicks, crackle, dropouts, aliasing (a
   high note's metallic whine), DC thumps, and how a note ends. Turn MASTER: the level follows, without zipper noise.
3. **Load.** HOME's header shows CPU % (the UI build: SETTINGS > BENCH's CPU row). On a few sounds (SUPERSAW's unison, a pad with every effect on), hold 1, 2,
   4 and 8 keys and note the CPU % and whether notes drop out (the guard sheds voices over 85 %).
4. **BENCH.** Hold HOME 5 s for the SETTINGS, turn ALGORITHM to the last page, BENCH. Its rows: AUDIO (a half's
   render: the last and the longest, and how many came late), CPU (load, the guard's steps, voices shed), SCREEN (a
   frame's drawing), KEYS (the longest the keys waited), TABLES (a wavetable build: its longest step and the whole),
   FLASH (the longest erase and page), STACKS, RUN. Turn **KNOB 1** for RUN (two loops timed, a moment's silence);
   **KNOB 2** clears the peaks. Do this after the load test, then after saving a few presets.
5. **The panel.** Every button, every key, every encoder (both directions, slow and fast), MASTER; the LEDs (the keys
   on SCL show the scale; PLAY's green LED blinks the tempo while it runs). The hold layers (GLO bend, FX macros,
   LFO momentary, EDIT jump), the MOD layer (REC).
6. **The screen.** Every page draws; the live pages (HOME's scope, the UI build's bloom, an envelope, an LFO) move
   while notes play and stop when they end; a turned knob's value follows without lag. **The UI build**, besides:
   each screen against the renders page ("Strata UI Firmware Renders": the emulator's pixels beside each design
   board); BENCH's SCREEN row after a minute on HOME with a chord held (the bloom: V0.66's scope for comparison) and
   after turning a knob on FILTER TONE quickly; the PAPER theme (SETTINGS > SCREEN) in daylight; the countdowns (HOME
   held, SAVE held on a user preset); anything that flickers, tears or lags.
7. **Saving.** SAVE once (V0.87: the SAVE window: SAVE AS NEW names a new version; on a user preset SAVE over it), SAVE twice (the browser), a rename in
   NAME IT; switch off and on: the sound playing comes back, the user presets are listed. Undo/redo (SAVE + OCT−/+).
8. **MIDI.** From a DAW over USB: notes, pitch bend, the mod wheel, CC 74 (the cutoff), a program change with
   PROGRAM on; MIDI LEARN. The TRS jack if you have a device for it.
9. **USB audio.** Record the FM-1 in the DAW (an audio input named after the USB name): clean, no gaps.
10. **Optional:** OCT− + OCT+ for 5 s: the countdown from 2 s, then UBOOT. Note whether Windows shows the
    `WL82 UBOOT1.00` disk or an unknown device; unread, the FM-1 returns to Strata by itself after ~13 s.

New in V0.6:

11. **The FX chain.** FX: the chain view. Add each type (KNOB 1 the type, KNOB 2 the slot, FX + OCT+), take one out
    (FX + OCT−), move one (KNOB 3), each while a note plays: listen for clicks (a move dips the effects for 3 ms; an
    add or a take-out should not be heard as more than the effect coming or going). FX twice opens a module, twice
    again back. FX held: F3 to F4 bypass the modules. Undo (SAVE + OCT−) after each.
12. **The new effects.** Each one's every knob, at its extremes too, on a sustained note and on chords: FLANGER,
    PHASER, the FX FILTER (its 21 types; COMB at a high RESO; DJ swept through its middle), BITCRUSH, TREMOLO and
    AUTOPAN, RING MOD, TAPE; two of a type (DRIVE and DRIVE 2). Note the CPU % with a full chain of eight (the header
    estimates it; HOME's header measures it).
13. **The panel's changes.** SELECT the section, ALGORITHM the page, in every family; EDIT + OCT− turns the section
    shown off, EDIT + OCT+ on; GLO's third section is the MATRIX (V0.8: SEQ opens the looper).
14. **Deleting a preset.** In the browser, on a user preset: OCT− held 3 s (a countdown), then SAVE + OCT− at once:
    it comes back. A preset saved by V0.5: loads, its effects as the chain.
15. **The fixes.** BENCH's KEYS row after flicking through pages while playing (under ~20 ms, against V0.5's 91 ms);
    AUDIO's late halves after the load test (item 3: none, or fewer than V0.5's 5).

New in V0.65:

16. **The new modules' knobs while a note plays.** FX FILTER, BITCRUSH, TREMOLO, RING MOD and TAPE (and a second of
    one): turn each knob with a note held; the sound follows at once (V0.6: nothing until a sound loaded). RING MOD's
    header: its name alone, nothing after it.
17. **The stacks.** After the whole test (every page, the DELAY's, FLANGER's and PHASER's graphs among them, a few
    saves and loads), BENCH's STACKS row: the main stack (the second figure) under ~8 KB of its 12; the first, the
    interrupts', as before (~2.2).
18. **The spare SRAM.** At the end, STACKS should still say **SPARE OK**. If it says SPARE HIT, note when it changed,
    and the console's `bench`: `spare_changed_bytes`. Either way nothing breaks: Strata keeps nothing there.
19. **The screen's speed.** BENCH's SCREEN row (the last and the longest frame) after turning knobs on a few live pages
    (HOME with a note held, an envelope, the EQ, the chain view): the UI's code is built for size now, so its drawing
    may be slower than V0.6's. Note whether turning a knob on a graph page feels slower.

New in V0.66:

20. **The screen.** The panel's SPI runs 5x faster: every page draws whole and clean, no stripes, wrong colours or
    torn rows (if anything looks wrong, a photo; the next build steps the clock back). Page changes should be quicker.
21. **Big chords.** VOWEL PAD and SUPERSAW, eight notes at once (the test page's load test does it): no crackle; the
    load table's late halves 0. Some notes may be stolen when the FM-1 has no time for them (the console's `bench`:
    `voices_refused`).
22. **The chain's changes, by ear.** CHORUS, FLANGER, TAPE, RING MOD and BITCRUSH at full MIX, a note held: add,
    take out, bypass (FX held + F3), back, move (KNOB 3): no click (a move ducks the effects for ~12 ms; CHORUS,
    FLANGER and TAPE come in ~20 ms after they are added, then fade in).
23. **The keys while it draws and saves.** Play quick notes while flicking through pages, and right after a SAVE:
    every note on time. BENCH's KEYS row counts only what the main loop held up (the layers); the console's
    `keys_from_timer5` is 1 on a plain page.
24. **VOWEL PAD.** Less hiss than V0.65's.
25. **The event log.** After the test, the flasher's device report (or `bench` in a terminal) ends with the event
    log: paste it with the report. With the USB audio recorded (a DAW, or the test page's audio), `uac_missed` and
    `uac_underruns` entries say when its gaps were, next to what else happened then.

New in V0.8 LOOPER (the test page's "Listen and look" has them as rows "take", "dub", "tracks", "project";
[CONTROLS.md](CONTROLS.md) "The looper"):

26. **A take.** SEQ (CLIPS: track 1, scene A, an empty grid). REC: a bar of count-in clicks (REC's LED blinking), then
    REC lit: play notes and chords for a bar or two, REC again: it loops at once, rounded to whole bars ("T1 A: 2
    BARS, LOOPING"), in time, no stuck notes. CLICK's LEVEL (TRACK > CLICK) by ear. PLAY stops it; PLAY again plays it
    from its start. With GLOBAL's CLOCK IN MIDI on a DAW's clock: starts with it and stays on its beat for a few
    minutes.
27. **Overdub and motion.** REC on the clip playing: add notes over a pass or two (what you add plays back from the
    next pass, never doubled), REC ends it. SEQ held, the last black key (UN): the last pass gone; again: back. REC
    again and turn FILTER > TONE's CUTOFF for a bar: the next pass moves it; stopped, the knob's own value. With
    SCL's chords on, or the ARP, the clip plays through them.
28. **Tracks.** SEQ + OCT+: track 2 ("T2 SYNTH"); record on it and on track 3 (MIDI from a keyboard over USB, then the
    TRS jack: no doubled or lost notes); SEQ held: white keys 1-3 launch their clips at the bar, 9-11 scenes A-C,
    black keys 1-3 MUTE; MIXER: VOLUME and PAN by ear. TRACK: one track MIDI, CHANNEL 2, to a DAW or a synth over
    USB: its notes there, none from the FM-1; MIDI IN on a channel: that channel plays it whichever track is
    selected.
29. **SONG and projects.** SONG: A twice, B, C; SONG ON, PLAY: the scenes in turn. Stop, wait 5 s in silence (the
    project's write: the device report's event log shows it), switch off and on: the clips, tracks, chords and SONG
    come back. PROJECT: 2, LOAD: empty; 1, LOAD: all there. If you had saved more than 64 presets with an older build:
    PROJECT 3 or 4 says "OLD PRESETS THERE" and is not saved (say so in the report).

CORE 2 (V0.8 BENCH; the test page's row "core2"):

30. **CORE 2.** HOME held 5 s (V0.87: DEBUG), ALGORITHM to CORE 2: STATE OFF. KNOB 1 right (ON, "ON AFTER A RESTART"),
    wait 5 s in silence (the settings' write), switch off and on. CORE 2 again: STATE **ANSWERING** (else NO ANSWER:
    OFF, which is an answer too: this unit runs on one core). Hold a chord, or PLAY a pattern, and turn KNOB 2 (TEST):
    30 s, listening for clicks; then DONE. Note CORE 1, CORE 0, SLOWED, WAIT, JOBS (and BAD) and STACK, or read the
    device report (its `c2_` lines). Then LOAD (KNOB 3) up to 32 and TEST again. To end: KNOB 1 left, 5 s, off and on:
    STATE OFF. If anything goes wrong: hold HOME while switching on (SAFE START: core 1 is not started), or let a
    second failed boot take the unit to backup mode as any build does (section 7).

New in V0.81:

31. **USB MIDI out, with nothing reading it.** TRACK: track 1 SYNTH+MIDI (or MIDI), CHANNEL 1. With the FM-1 on a
    computer and **no MIDI app open**, play the keys for a minute (well over 64 notes). Then open a MIDI monitor (or a
    DAW with a synth on channel 1) and play a note: older notes may come first, then **All Notes Off** (controller
    123) on channel 1, then yours; nothing left sounding. Whether the computer reads the port while no app has it
    open is **unverified** (Windows is said not to; macOS reads it): if nothing old arrives, the queue never filled and
    there was nothing to recover, which is fine too.
32. **The load.** HOME's CPU figure in silence and with a chord of SUPERSAW: a little under what V0.8 would show (the
    model says 1-8 % of the engine's work); note both.

New in V0.85 DRUMS (the test page's rows "drums", "grid", "duck"; [CONTROLS.md](CONTROLS.md) "The drums"):

33. **A drum track.** SEQ > CLIPS: track 1 (PRESETS picks it), TYPE DRUMS (V0.86: it changes when you stop turning). The white keys 1-8: KICK, SNARE, CLAP, HAT CL, HAT OP, CYM,
    WT 1, WT 2; 9-16 the same, louder (accented); the first black key DUCK (no sound). EDIT: KIT. DRUM: each drum's
    TUNE, DECAY and TONE by ear (stopped, the drum you play is the one picked); MORE: its own (PUNCH, SNAP, SPREAD,
    METAL, BEND), LEVEL, PAN. A HAT CL cuts a ringing HAT OP (DECAY long). No clicks at a hit's start or end.
34. **The grid.** SEQ > CLIP on the drum track: GRID. Black keys pick the row (1 KICK .. 9 DUCK), white keys put the
    sixteen steps in and out (stopped, each heard as it goes in); the keys' LEDs show the row's steps. PLAY: the beat
    in time over a few minutes, the playhead's LED walking; GRID's VELOCITY and ACCENT, HIT's CHANCE (a step that plays
    sometimes) and RATCHET (2-4 quick hits in its sixteenth) by ear.
35. **The duck.** Track 2 SYNTH with a clip of long chords (or hold a chord on it while the drums play); the drum
    track's DUCK row on the beats: the chord dips on each and comes back. KIT > DUCK: AMOUNT, ATTACK, RELEASE and SHAPE
    (PUMP: the pumping one) by ear; the drums themselves never dip.
36. **Recording and TYPE.** REC on the drum track's empty clip in another scene: play drums for a bar or two (accented
    ones too), REC: it loops as played. CLIPS' TYPE SYNTH and back to DRUMS (each asks one detent more: the track has
    clips): the beat as it was (its accents, chances,
    ratchets kept).
37. **The load with drums.** HOME's CPU figure with a full beat playing (all eight drums), then with SUPERSAW chords
    over it: note both (the model: the eight drums sounding at once 11-15 % of a core, a beat's usual two or three
    much less).

New in V0.86 UI ([CONTROLS.md](CONTROLS.md); the test page has no rows of its own for these yet: note them by hand):

38. **Power-on and the layout.** The splash for 2 s, then HOME's SOUND. On every menu screen the bottom row names the
    module's pages (the one shown in full and inverted, the others short: UNI, SPR ..), and the top right the menu's
    module dots (SEQ's four in two groups). Note any screen whose short names you cannot read at arm's length.
39. **The buttons once and twice.** From OSC A: ENV once (the page left in ENV), again (ENV 1's ADSR), twice (ENV 2);
    EDIT twice (V0.87): the wavetables' list; LFO twice on OSC A's WAVE (V0.87): the assign window; (V0.86's: MOMENTARY latched
    there, a knob moves, LFO puts it back; GLO twice: PERFORM latched, GLO closes it; SCL twice, ARP twice: on and
    off; SEQ once and twice; HOME twice: the browser. Whether 300 ms between the presses feels right, and whether a
    single press ever acts as a double (or the other way).
40. **PRESETS' list** (as the renders review changed it). Turn PRESETS: the list opens and follows, nothing loads;
    SAVE twice loads the one picked; left untouched for 3 s, the list goes. SAVE + OCT+ in the list: a new preset,
    named first; SAVE + OCT− on a user preset asks, again deletes it (a starter sound stays). The bottom row turns
    vermilion when loading would lose your edits. Whether 3 s is long enough to read the list.
41. **SEQ.** PRESETS picks the track on every SEQ page. CLIPS: TYPE turned on a track with clips asks for one more
    detent; on CLIPS the white keys 1-8 launch the scene's clips, the black keys 1-8 the scenes; their LEDs (lit
    playing, blinking launched or recording, glowing there). CLIP's SPEED x2 and x1/2 by ear, in time with the other
    tracks. TOOLS: COPY a clip, PASTE it into another scene (asked when something is there), DOUBLE, HALVE, CLEAR.
42. **The drum grid, a step held.** Hold a step's white key: the four knobs set its VELOCITY, ACCENT, CHANCE, RATCHET;
    let go and they are the page's again. A tap on a hit takes it out as before; a hold with a turn keeps it.
43. **Centre detents.** OSC A's OUTPUT PAN, a track's PAN, a drum's TUNE: turned through the middle, it stops there for
    three detents. Note whether that feels like a detent or like a stuck knob.
44. **HOME's other pages.** MIX 1-4, MIX 5-8: the eight tracks' VOLUME. On a drum track, HOME's grid of eight lit as the
    drums sound (in time with what you hear), PRESETS picking the drum, TUNE, DECAY, TONE and DUCK moving the whole kit.
45. **SETTINGS > POWER.** The battery's bars and its raw reading, on battery from full and over an hour (**unverified**:
    the stock thresholds 531, 561, 591 on this unit; note the raw figure at full, at the first bar lost, and when the
    FM-1 warns); USB power shows a bolt. SHOW BATTERY HOME, then EVERY SCREEN: the battery on the top lines. SCREEN OFF
    AFTER 1 MIN: after a minute untouched the panel goes dark while the sound goes on and the backlight stays lit;
    a button, a knob or a key wakes it, and that press does nothing else (**unverified** on this panel: Felucca's notes,
    found on its 1.1.5). If the panel ever stays dark, a power cycle brings it back with SCREEN OFF as you left it:
    set it OFF again (dark, a first press only wakes it; then HOME twice opens the SETTINGS).
46. **SETTINGS > KEY SCAN.** Press keys slowly and hard: does any dotted cell turn vermilion? (It would mean the keybed
    has a second contact, as velocity needs; the matrix's spare cells say probably not.)
47. **The help map.** HOME held on a few pages: what it says matches what the knobs and buttons do there; held on, the
    SETTINGS at 5 s.
48. **BENCH after a session.** The SCREEN row (the longest frame and its page: the help map and HOME's drum view are the
    new heavy ones), STACKS (the main stack's figure against the build's ~5.6 KB).

New in V0.86 step 2, the drum editor ([CONTROLS.md](CONTROLS.md) "The drums"; note them by hand too):

49. **DRUM.** On a drum track, EDIT: DRUM's SOUND for the drum PRESETS picks (stopped, the drum you play). By ear on a
    KICK and a SNARE: DRIVE's four TYPEs at DRIVE 50 and 100 (SOFT warm, HARD edged, FOLD hollow, CRUSH stepped), MIX
    back to 0; EQ's LOW, MID with FREQ swept, HIGH; SHAPE's ATTACK up (more click), SUSTAIN down (a shorter body).
    Note any setting that clicks, jumps in level or sounds broken. The graphs follow the knobs.
50. **KIT.** SELECT from DRUM: ACCENT's TONE (soft hits darker, accented brighter); DUCK's SOURCE KICK with a synth
    chord held under a beat (the synth ducks on each KICK, not on the DUCK row), BOTH; CURVE's HOLD (the duck stays
    down longer); COMP at AMOUNT 50 and 100 on a full beat (the beat denser, louder from 20 % on; any pumping you
    hear: note how fast).
51. **The kits.** EDIT twice on a drum track (V0.87): the eight kits; PRESETS picks, SAVE twice loads; each kit by ear over the
    same beat. Edit one, then EDIT twice: its dot, and the bottom row warns before you load another. Power-cycle: the
    kit as you left it.
52. **The load with the effects.** HOME's CPU figure with the beat playing on a kit with effects (INDUSTRIAL: DRIVE on six
    drums, COMP 60), then with SUPERSAW chords over it (the model: every effect on all eight drums at once 11-15 %
    of a core more; a beat's usual two or three ~5 % more).
53. **V0.87: the LEDs** (U9; **needs this hardware test**, verified on the host only: `tests/sy_led_test.c`). SETTINGS
    (HOME twice) > SCREEN > LIGHTS: HIGH, MED, LOW, OFF. At each, look at a lit button, a mid key (CLIPS: a clip there)
    and a glowing one, from the side too: steady, no flicker (V0.86's mid level and the breath's low end flickered);
    each level clearly dimmer than the one above; OFF dark. REC's count-in breathing: smooth, no steps you can see.
54. **V0.87: backup mode by hand** (W1; **needs this hardware test**). OCT− and OCT+ held 5 s: backup mode; leave it
    alone until it times out and restarts (about a minute): Strata starts normally, **no STRATA CRASH screen, no SAFE
    START** (V0.86 showed one). Photo either way.
55. **V0.87: the splash and the theme.** Pick a theme other than STRATA, power-cycle: the splash in that theme. Then
    SETTINGS > WIPE, SAVE: STRATA again, LIGHTS HIGH, your presets and projects still there.
56. **V0.87: the new controls.** EDIT twice (the wavetables' list: SELECT the category, SAVE twice loads); LFO twice on
    a WAVE page (the assign window: a source onto WT POS, ALGORITHM, AMOUNT and POLARITY, by ear); SAVE once (the SAVE
    window); a reset by EDIT + OCT− held 5 s, stopped once by letting go of OCT−; REC twice after playing a phrase
    (TAKE: it loops); GLO held + PLAY tapped in time (tap tempo); SEQ + OCT+ (a new clip, PRESETS its length), SEQ +
    OCT− twice (DELETE?, deleted). Any one that does not do what CONTROLS.md says: note it.
57. **V0.87 step 2: the drums by ear.** On a drum track: EDIT twice on KIT, load each of the six kits over the same
    beat (808, 909, LO-FI, VIDEO GAME, TRAP, DRUM & BASS): do they sound like their names, and unlike each other?
    DRUM > MODEL on the KICK row: each model, each TYPE, a hit at a time (anything that clicks, whistles, aliases
    badly or is far louder or quieter than the rest: note which). EDIT twice on DRUM: a few presets of each category.
    Save a drum preset and a kit of your own, power-cycle: both still there. EUCLID on a row: HITS, STEPS, ROTATE.
    BENCH's AUDIO row with VIDEO GAME and TRAP playing.
58. **The release build's USB identity** (all in one sitting with 53 to 57; **needs this hardware test**). After the
    install, the computer lists a MIDI port named **"Strata for FM-1"** (maker "u4ia"; Windows: Device Manager, Sound,
    video and game controllers), the flasher's Find, the test page and the console still connect, and the USB audio
    input still records. Send the soft key (the flasher's "Put the FM-1 in backup mode"), then switch the FM-1 off and
    on: Strata again. Optional: Felucca's installer still finds the FM-1 (the way back to Felucca).

## 5. The report

On the flasher page, step 3: **Read the device report** opens Strata's USB serial console (Chrome asks for the port:
the FM-1's) and reads `status` and `bench`; **Run BENCH's loops, then read** asks for RUN first. **Copy**, and paste
the text into the conversation with what you heard and saw, and photos of any crash screen. The same console works
in any serial terminal (115200 baud; type `help`). Its commands only read (`bench run` starts BENCH's loops);
`uboot yes` enters backup mode. The test page (section 4) makes a fuller report: everything it measured and what you
marked.

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
  page; nothing is uploaded), with the power box ticked (a backup recommended, section 2), **Return to
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

- **V0.86 step 2 (the drum editor, with the renders review's fixes)**: built and checked by the build: the image
  482,064 bytes (+17,584 on step 1), RAM 63,636 of 73,728 bytes (+816), the pool 362,036 bytes, the interrupt stack
  ~3.1 KB of 4, the main stack ~5.6 KB of 8, register access in `hal/` only; the package sha256
  `e56d69ba...4183c2c6e`, the flasher rebuilt (its page sha256 `99843dcb...00e19ffc`). **Verified on host**: the
  drums' 28 checks (10 new: SHAPE, DRIVE, EQ, COMP, the duck's SOURCE and HOLD, ACCENT's TONE, the factory kits, 40
  random kits with every effect, the cost) with their golden renders unchanged, the engine's 94, USB MIDI out's 9; the
  cost model (BUDGET.md §17: every effect on all eight drums at once +525 instructions a sample); the stress test's 24
  seeds x 3,000 steps under ASan and UBSan with kit loads and the effects' values in (its first run stopped on the
  duck's new HOLD state, which the check did not yet know; the check updated, clean). **Verified in the emulator**:
  166 checks (7 new: PRESETS' list by SAVE twice, its SAVE + OCT+ and SAVE + OCT−, DRUM's and KIT's pages, the kits'
  list, DRUM's reset, a power cycle with a kit), 159 screens checked, 681 knob turns, the flasher's 26 and the test
  page's 21, the emulator's page in a browser 6. **Needs a hardware test**: steps 38-52 (deferred to V0.9's test).
- **V0.86 UI**: built and checked by the build: the image 464,480 bytes (+26,356 on V0.85), RAM 62,820 of 73,728 bytes
  (+592), the pool 362,036 bytes, the interrupt stack ~2.7 KB of 4, the main stack ~5.6 KB of 8, register access in
  `hal/` only; the package sha256 `941a9d56...4c00d8d4`, the flasher rebuilt (its page sha256 `2d285d9d...ae03af86`).
  **Verified on host**: the engine's 94 checks and the drums' 18 with the golden renders unchanged (the effects' new
  parameters at their defaults move no sample), USB MIDI out's 9, the cost model (BUDGET.md §16: unchanged within 3
  instructions a sample), the stress test's 24 seeds x 3,000 steps under ASan and UBSan with TOOLS, SPEED and the
  screen's sleep in (it found two one-byte stack overflows in new screen text and a null name in the help map, all
  fixed). **Verified in the emulator**: 159 checks (19 new: the buttons once and twice, the help map, TOOLS, SPEED,
  TYPE's guard, CLIPS' keys, the grid's held step, a power cycle with SPEED, the centre detent, the battery's bars,
  the screen's sleep and wake, POWER's settings after a power cycle), 152 screens checked, 639 knob turns, the
  flasher's 26 and the test page's 21, the emulator's page in a browser 6. **Needs a hardware test**: steps 38-48.
  **Unverified**: the battery thresholds on this unit, the panel's sleep and wake (DISPOFF, SLPIN), whether the keybed
  has second contacts.

- **V0.85 DRUMS**: built and checked by the build: the image 438,124 bytes (+20,940 on V0.81), RAM 62,228 of 73,728
  bytes (+1,568: the drum voices, the kit, the ratchets), the pool 361,860 bytes, the interrupt stack ~2.7 KB of 4, the
  main stack ~4.8 KB, register access in `hal/` only; the flasher rebuilt (sha256 `f52c25b8...87e71ded`). **Verified
  on host**: the drums' 18 checks (TESTING.md §2.7: every lane's level and end, DECAY's time, TUNE, the bursts, the
  choke, ACCENT, VELOCITY, PAN, the duck's curves and that it takes the synth only, 40 random kits, golden renders),
  the engine's 94 with its golden renders unchanged (and as WebAssembly), USB MIDI out's 9, the stress test's 24 seeds
  x 3,000 steps with the drums in (ASan, UBSan), the cost model (BUDGET.md §15: the eight drums at once 11-15 % of a
  core). **Verified in the emulator**: 140 checks (the drums' 8: the pages and keys, the grid by its keys and LEDs, a
  beat's hits counted over two bars, the duck's depth, RATCHET and CHANCE from HIT's knobs over eight bars, a take, TYPE
  there and back, the kit's knobs, a power cycle), 138 screens checked, 611 knob turns, the flasher's 26 and the test
  page's 21 (three rows more for V0.85, one for V0.81). **Needs a hardware test**: the drums by ear, their timing over
  minutes, the duck by ear, the load (steps 33-37). **Unverified**: how the drums sound on the FM-1's output (levels,
  the click of a hit's start), the load a beat adds on a unit.
- **V0.81**: built and checked by the build: the image 417,184 bytes (−1,176), RAM 60,660 of 73,728 bytes (−1,024),
  the pool 361,788 bytes (+1,024: the generator's harmonics where they belong), the interrupt stack ~2.5 KB, the main
  stack ~4.8 KB, register access in `hal/` only; the flasher rebuilt (sha256 `c370d0a7...5bd792220`). **Verified on
  host**: the engine's 94 checks with its golden renders unchanged (the matrix and scan changes move no sample), USB
  MIDI out's 9 (§2.6 of TESTING.md) and Felucca's five ring tests, the stress test's 24 seeds × 3,000 steps under
  AddressSanitizer and UBSan (TESTING.md §6.6), the cost model (BUDGET.md §14). **Verified in the emulator**: 132
  checks, 127 screens and 583 knob turns, the flasher's 26 and the test page's 21. **Needs a hardware test**: steps
  31-32, and everything V0.8 LOOPER's steps 26-30 ask. **Unverified**: whether a computer reads USB MIDI while no app
  has it open (step 31); the core's margin at the boot loader's 360 MHz against the datasheet's 320 (PHASE0.md §2:
  every Felucca fork runs there).
- **V0.8 LOOPER**: built and checked by the build: the image 418,360 bytes, RAM 61,684 of 73,728 bytes (+8,048 on
  V0.8 BENCH: the chords' eight sides, the looper's state, the parameters held), the pool 360,764 bytes (the 2,048
  events in place of the patterns), the interrupt stack ~2.5 KB, the main stack ~4.8 KB, register access in `hal/`
  only; the presets' 13 pairs and the projects' objects asserted inside the allow-listed store and below the update's
  staging. **Verified on host**: the engine's 94 checks (and as WebAssembly), the store's (64 user slots). **Verified
  in the emulator**: 132 checks (the looper's 13: playing and launching from the pad, the mix, a MIDI track, STOP ALL,
  a take with its count-in, overdub and UNDO PASS, QUANTIZE and knob moves, SONG, chords a track and MIDI IN, the
  click, NOTES, the tracks' grammar, projects through a power cycle), 127 screens and 583 knob turns checked, the
  flasher's 26 and the test page's 21. **Needs a hardware test**: the timing by ear and against a DAW's clock,
  recording from the keys and MIDI (USB, TRS), the click's level, the projects' writes and their return, the launch
  pad's LEDs (steps 26-29). **Unverified**: whether an older build left presets in slots 65-128 on your unit (step
  29 shows it; nothing is written over them either way).

- **V0.8 BENCH**: built and checked by the build: the image 400,636 bytes, RAM 53,636 of 73,728 bytes (+4,644 on
  V0.8: core 1's stacks 4,352 bytes, its mailbox, the test's state), the pool 357,232 bytes (+512: the test's phases
  and buffer), the interrupt stack ~2.5 KB (the test's job in it), the main stack ~4.9 KB, the second core's waiting
  loop in RAM (50 instructions, calling only its job), register access in `hal/` only; the voices' code built as V0.8's
  (`voice_block` with `osc_block` inlined, `sy_block` in the audio interrupt: the listing). **Verified in the
  emulator**: 132 checks (CORE 2's two: the page on one core, TEST refused, ON and LOAD through a power cycle), 122
  screens (CORE 2's two), 573 knob turns, the flasher's 26, the test page's 21. **Needs a hardware test**: whether
  core 1 starts and answers on your unit, runs 30 s of jobs without a misread, its time against core 0's, how much
  core 0 slows while it runs, its stack, and no clicks while it tests (step 30). **Unverified**: all of the second
  core's figures; the emulator has one core and measures nothing.

- **V0.8**: built and checked by the build: the image 392,004 bytes, RAM 48,992 of 73,728 bytes (+448 on V0.75: the
  sequencer's settings and state), the pool 356,720 of 380,928 bytes (+14,348: the sixteen patterns and their copy),
  the interrupt stack ~2.5 KB, the main stack ~4.9 KB, register access in `hal/` only; the patterns' four flash objects
  asserted inside the allow-listed store and below the update's staging. **Verified on host**: the engine's 94 checks
  (and as WebAssembly). **Verified in the emulator**: 130 checks (the sequencer's thirteen: playback, its feel, live
  and step recording, REPLACE, the patterns, CHAIN, MOTION, PATTERNS, the grammar, a power cycle), 120 screens rendered
  and checked, every knob on every page turned and checked (570 turns), the emulator's page in a browser; the
  flasher's 26 and the test page's 21. **Needs a hardware test**: the steps' timing by ear and on a DAW's clock over
  minutes, recording from the keys and from MIDI (USB and TRS), the patterns' writes after 3 s of quiet and their
  return after a power cycle, the REC LED (the test page's "Listen and look": the V0.8 rows).

- **V0.75**: built and checked by the build: the image 371,420 bytes, RAM 48,544 of 73,728 bytes (+888 on V0.7: the
  arpeggiator's steps and state, STRUM's starts, MIDI's clock times), the pool and the spare SRAM as V0.7's, the
  interrupt stack ~2.5 KB, the main stack ~4.9 KB, register access in `hal/` only. **Verified on host**: the engine's 94
  checks (and as WebAssembly). **Verified in the emulator**: 117 checks (the ARP's, the chords' and the buttons'
  grammar among them), 109 screens rendered and checked, every knob on every page turned and checked (546 turns), the
  emulator's page in a browser; the flasher's 26 and the test page's 21. **Needs a hardware test**: the ARP and STRUM
  by ear (the timing, RATCHET, SWING, SLIDE with GLIDE, HUMAN), PLAY's beat, MIDI clock from a DAW (over USB and the TRS
  jack: the tempo, start and stop, staying on the beat over minutes), the buttons' grammar on the panel (the double
  taps, the 5 s holds), the ARP LED's beat (the test page's "Listen and look": the V0.75 rows).

- **V0.7**: built and checked by the build: the image 358,264 bytes, RAM 47,656 of 73,728 bytes (+528: the chords'
  state and Felucca's USB MIDI out queue, now used), the pool and the spare SRAM as V0.67's, the interrupt stack ~2.5 KB,
  the main stack ~4.6 KB, register access in `hal/` only. **Verified on host**: the engine's 94 checks (and as
  WebAssembly). **Verified in the emulator**: 100 checks (the chords' seven among them), 100 screens rendered and
  checked, the emulator's page in a browser. **Needs a hardware test**: the chords by ear (the strum's and the arpeggio's
  timing, AUTO's voicing), USB MIDI out into a computer (a DAW or a MIDI monitor: channels 1 and 2), MIDI in as roots,
  the chord buttons' LEDs (the test page's "Listen and look": the V0.7 rows).

- **V0.67**: built and checked by the build: the image 339,112 bytes, RAM 47,128 of 73,728 bytes, the pool 342,372 of
  380,928, the spare SRAM 21,900 bytes used and 23,296 left, the interrupt stack ~2.5 KB, the main stack ~4.6 KB (8 KiB
  allowed), register access in `hal/` only; the package rebuilt by the flasher's builder (sha256 `8219aeea...0b46c3d9`).
  **Verified on host**: the engine's 94 checks (`sy_tests.sh`, and as WebAssembly), the drawing's time against V0.66
  UI's ([UI.md](UI.md) §5). **Verified in the emulator**: 93 checks (`strata_emu_test.mjs`, the screens' check on all
  149 pages among them), 80 screens rendered and checked (`strata_ui_shots.mjs`), the flasher's 26 checks, the test
  page's 21, the emulator's page in a browser (6), the backup server's 13. **Needs a hardware test**: the legibility on
  the panel, VALUES' fine steps, the twelve themes, the bloom, BENCH's SCREEN row (the test page's "Listen and look":
  the four V0.67 rows).

- **The UI build** (`V0.66 UI`, branch `strata-ui`): built and checked by the build: the image 326,280 bytes (V0.66
  327,184; the fonts 39.0 KB, V0.66's 76.1), RAM 47,128 of 73,728 bytes, the pool 342,372 of 380,928 (the canvas 240
  × 80), the spare SRAM 21,900 bytes used (the curves' cache), 23,296 left; the interrupt stack ~2.5 KB, the main stack
  ~4.6 KB (8 KiB allowed); the package rebuilt by the flasher's builder (sha256 `5638a666...028fdef2a8`). **Verified in
  the emulator**: 90 checks (`strata_emu_test.mjs`), every screen rendered (`strata_ui_shots.mjs`), the drawing's time
  on the host against V0.66's (`strata_bench.c`, [UI.md](UI.md) §5). **Needs a hardware test**: everything in section
  4, item 6.

- **Built and checked by the build** (`tools/build.py --strata`, V0.65): the image (324,148 of 581,564 bytes; V0.6
  407,008), RAM (65,152 of 73,728 bytes: 8.4 KiB spare, over the 4 KiB margin), the pool (358,692 of 380,928 bytes:
  21.7 KiB spare, over its 4 KiB margin; `strata.ld` gives it 12 KiB of Felucca's main stack), RAM code with no calls
  into the flash, no register access outside `hal/`, the interrupt stack (~2.5 KiB of 7.9 KiB) and the main stack
  (~6.6 KB of 12, 8 KiB allowed), both from the listing, usb.c, midi_uart.c and ota.c still at `-Os` (57 functions),
  Strata's main-loop code at `-Os` (331 functions; the interrupts and the DSP the same instructions as without it),
  the package rebuilt byte for byte by the flasher's builder (sha256 `4ad8161a...5826e01b`). Felucca's own build:
  byte for byte as before.
- **Verified on hardware, V0.65's report (2026-10-18, the flasher's device report after 63 min of use):** it installed
  and runs; boot 769 ms; the main stack 4,968 of 12,032 bytes used (V0.5: 17.9 KiB: the stack fix holds), the
  interrupts' 2,192 of 7,936; the 45,056 spare bytes untouched (26 flash erases, little MIDI, no USB audio stream in
  that hour); BENCH RUN integer 3,892 µs (V0.5's, to the µs: ~180 instructions a µs), float 2,916 µs (V0.5 3,055: ~154,
  probably where the loop sits in the flash's cache); the flash as V0.5 (erase 17.4 ms, page 2.06 ms); 17 % CPU with no
  voice on SUPERSAW (its CHORUS and REVERB run for their tails: the cost model's ~5 + ~13 %). Read with care: 8 late
  halves while no half's render passed 2,582 of 2,901 µs: the interrupts were held off, not the render late; each of
  the 26 erases holds them 17 ms behind the hush, in silence, and the half after one counts as late (BENCH should leave
  those out: next version). The keys' longest wait 84 ms and a frame's 107 ms are the hour's peaks, SAVEs and the
  background saves (an erase and 15 pages, ~50 ms with the main loop held) and full redraws included: the test page's
  step D measures them with the peaks cleared. The test page itself has not run yet.
- **Verified on hardware, V0.5's report (2026-10-17):** the restart check (bulk IN after a power-on); the core's
  rate, ~180 instructions a µs on integer code and ~147 on float (BENCH RUN); a half's render 172 µs idle, 2,766 µs
  at the load test's peak (95 %, 5 late, 1 voice shed); the keys' longest wait 91 ms; the stacks (main 17.9 of 24.3
  KiB, interrupts 2.2 of 7.9 KiB); the flash (erase 17.4 ms, page 2.0 ms); boot 765 ms.
- **Verified on host and in the emulator:** the engine (133 checks, host and WebAssembly; V0.65: every smoothed value
  glides to a change while a sound plays, which V0.6 failed for 34; the chain's adds, take-outs
  and moves without a click, a module bypassed bit for bit, two of a type, every new module at its corners and the
  FX filter's 21 types with the cutoff jumping, the responses of the filter, phaser, flanger, ring mod, tremolo and
  tape; the golden renders unchanged: the six effects of before play as they did), the UI (89 checks; V0.65: BENCH's
  rows fit, every page's name ends in time; the chain view,
  its adds, take-outs, moves, limits, bypass, MIX and undo, a second EQ's pages; EDIT + OCT; the matrix on GLO; a
  preset deleted and put back), the workbench (5), the MASTER level path, BENCH's page. The keys read during a
  redraw's waits are the FM-1's alone (lcd.c's hook): **needs this hardware test**.
- **Verified in a browser against a simulated FM-1** (`web/strata_flash/flash_test.mjs`, 26 checks): the install and
  V15 on without a backup (V0.87: recommended only, the last one's date shown); Back up now (the token and the page's Origin sent; waiting,
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
