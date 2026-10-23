# USB

On a computer, an FM-1 running Strata is one USB device with three functions: **MIDI**, **audio** and a **serial
console**. It names itself "Strata for FM-1" by "u4ia" (USB id 1209:5354; its update loader 1209:5355). The name has
"FM-1" in it so that FM-1 installers and the Strata flasher still find it. Strata never turns on the FM-1's Bluetooth
or Wi-Fi radio.

## MIDI

A USB MIDI port, in and out ([MIDI](midi.md)). Only one app at a time can open it on Windows: close a DAW before using
an installer, and the other way round.

## Audio

The FM-1 is a **USB audio input** on the computer: what it plays, to record in a DAW.

## The serial console

A USB serial port (CDC). Open it in any serial terminal (115200 baud) and type `help`. Its commands only read, except
`uboot yes`:

| Command | Shows |
| --- | --- |
| `status` | the device report: the version, the time since power-on, the CPU load and the guard's steps, the voices, the sound, USB and MIDI counts, the flash |
| `bench` | BENCH's figures (the same as DEBUG > BENCH, and CORE 2's) and the event log |
| `bench run` | runs BENCH's timed loops (a moment's silence), then `bench` again for the figures |
| `inp` | the panel's inputs as scanned |
| `crash` | the last crash's record |
| `params` | the sound's parameters |
| `memr ADDR [LEN]`, `flr OFF [LEN]` | memory and flash, read |
| `uboot yes` | restarts into backup mode, as the SysEx message does |

The flasher's step 3 (**Read the device report**, **Run BENCH's loops, then read**) reads `status` and `bench` over
this port from the browser, ready to copy into a test report. The console stays in every Strata build.

## The backup-mode message

Like the official firmware, Strata answers the SysEx message `F0 22 24 35 7D F7` (the "soft key") on its USB MIDI port
by restarting into the chip's **backup mode** (UBOOT), where a full flash backup can be read or written. Any program
that can send SysEx to the FM-1 can send it. The FM-1 then stops playing and waits; switching it off and on returns to
Strata. Nothing is written by the message itself. The same mode is reached by holding <kbd>OCT−</kbd> and
<kbd>OCT+</kbd> for 5 s, or by the console's `uboot yes`.

Backup mode that no computer has read ends by itself after about 13 s; once read, it stays until the FM-1 is switched
off. From Strata, on one computer, the chip reached backup mode but Windows never read it: the Flipper Zero route
([Installing and going back](install.md#a-full-backup)) is the way that works at power-on whatever the firmware does.

## Power

With a USB host powering the FM-1, SETTINGS > POWER shows a bolt in place of the battery's cells.
