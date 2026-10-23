# LEDs

Every LED in Strata — the keys' and the buttons' — keeps to one code, by what it means:

| State | Means | Examples |
| --- | --- | --- |
| **Off** | nothing there | an empty clip place, a key outside the scale |
| **Glowing** (dim) | there | a clip, a module in the chain, a scale's note, a drum, a family button not shown |
| **Mid** | in use | the scale's root, an accented drum key, the playhead's step on the GRID |
| **Lit** | active | a key held, a clip playing, the family shown, the transport's beat |
| **Blinking** (a quarter second) | waiting | a launched clip, a count-in, REC waiting for the bar |
| **Breathing** | recording, or a layer open | a layer's button while its layer is held or latched |

## The buttons

| LED | Lit | Glowing | Breathing / other |
| --- | --- | --- | --- |
| A menu button (HOME, EDIT, ENV, LFO, FX, GLO, SCL, ARP, SEQ) | the menu shown | the others | GLO, FX, LFO while their layer is open |
| <kbd>REC</kbd> | while the MOD layer is held; while the looper records (dark the last quarter of each beat) | otherwise | breathing while the MOD layer is latched; blinking while counting in or waiting for the bar |
| <kbd>PLAY</kbd> | the first quarter of each beat while the transport runs | | its green LED: lit while the transport runs |
| <kbd>ARP</kbd> | the ARP on (dark a moment on each beat while ARP or PATTERN plays) | the ARP off | |

## The keys

| Where | Lit | Glowing | Mid |
| --- | --- | --- | --- |
| Playing | a key held | the scale's notes (a scale other than CHROMATIC) | the scale's root |
| A drum track | a key held | its eight drums and DUCK | the accented keys (9–16) |
| MOD layer | the source picked | the sources in use on the page | |
| MACROS | a module that is in | a module bypassed | |
| CLIPS page | a clip playing; the scene picked | a clip there; a scene with clips | blinking: a clip launched or recording |
| LAUNCH PAD | a track's clip playing, the scene picked, a track not muted, SOLO on | a clip there, a scene with clips, STOP, SOLO, UNDO | blinking: launched or recording |
| GRID, HIT | the row's hits, the row picked | rows with hits there | the playhead's step |

## LIGHTS

SETTINGS > SCREEN > **LIGHTS** sets how bright every LED is: OFF, LOW, MED, HIGH (HIGH by default). Each level is
steady, without flicker: the LEDs are driven in quarters of the scan's tick (40 kHz). **Needs a hardware test**: how
the levels look on the panel.
