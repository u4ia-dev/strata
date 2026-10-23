# The store and backups

The FM-1 has 1 MiB of flash. Strata writes only inside the user store's allow-list, the same ranges Felucca uses for
its own data, with a check at compile time and a second guard in the flash driver itself.

## What is kept where

| What | Where in the flash | Written |
| --- | --- | --- |
| Strata itself (the app area) | 0x4000 – 0x92FFF | by an install or update only (the update loader) |
| Your 64 presets | 0x97000 – 0xB0FFF | when you save or delete one |
| Projects 3 and 4 | 0xB1000 – 0xC0FFF | when stopped and quiet for 3 s, if changed |
| Projects 1 and 2 | 0xCB000 – 0xDAFFF | the same |
| Your drum library (64 drum presets, 16 kits) | from 0xDB000 | when you save or delete one |
| The update's staging | 0xE0000 – 0xE4FFF | during an install |
| The sound playing | 0xE5000 – 0xE6FFF | after 3 s without sound, if changed |
| The settings | 0xFC000 – 0xFDFFF | after 3 s without sound, if changed |

**Never written**: the boot area (0x0000 – 0x3FFF), the calibration and trim data, the Bluetooth pairing (0xE9000), the
unit's own key (0xFF000).

Each record is kept as two copies: a save goes to the other copy (erase, the data, a commit record last), is read back
before it counts, and the newest good copy loads, so a write cut off leaves the one before. Every write waits for the
audio to fade for a moment (an erase holds the processor for about 17 ms).

## Felucca's data

Felucca keeps its projects and samples in the same area (0x97000 to 0xDF000). Strata's records carry their own mark:
Felucca's data reads as empty to Strata, and Strata's as nothing to Felucca. But **saving anything in Strata can
overwrite Felucca's projects and samples** where a Strata record lands on them, and going back to Felucca or the
official firmware does not bring them back. Installing Strata does not erase the area by itself.

Presets that a build before V0.8 saved in slots 65 to 128 are no longer shown (Strata has 64 user slots since then) and
stay in the flash; projects 3 and 4 are not saved where they are ("PROJECT 3: OLD PRESETS THERE").

## Backups

A **full flash backup** is a raw 1 MiB image of the flash from address 0, read twice with both copies equal. It holds
everything: the firmware, your presets and projects, Felucca's data, the settings. It is the only way back to exactly
what you had. Make one before your first Strata, and whenever your presets and projects matter. How:
[Installing and going back](install.md#a-full-backup).

> **Tip:** keep two copies of your backup in different places, with the date in the folder's name. The flasher's
> backup step writes a receipt (`backup.json`) beside the two files, and counts a backup only while both files still
> match it.
