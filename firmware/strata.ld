/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 brandonmbarone (Strata's changes) */
/* STRATA's memory map (tools/build.py --strata links with it): Felucca's app.ld, the same regions, sections and
 * symbols, with two changes (V0.65, the memory audit):
 *  - the main stack 12 KiB (Felucca's 24 KiB): Strata's deepest main-loop path is ~6.6 KiB from the listing (V0.6
 *    measured 17.9 KiB on the unit: one 17 KiB frame, every page's graph inlined into ui_draw_disp; V0.65 calls them
 *    through a table), build.py holds it to 8 KiB, BENCH reads it on the unit. The 12 KiB go to the POOL;
 *  - the two areas Strata leaves unused named, _spare0 (RAMTEXT after the RAM code, and 0x01C06000..0x01C08000,
 *    Felucca's room for RAM-run debug: ~29 KiB) and _spare1 (NOINIT after .noinit: ~15 KiB, where Felucca keeps a
 *    project across resets). Nothing is put there yet: the boot paints them and BENCH says whether anything else
 *    wrote there while Strata ran (sy_main.c), so that a later version can use them.
 * Felucca's notes: code + constants execute in place from XIP (app.bin offset 0 = 0x02000120). 0x01C00000..0x01C06000
 * holds .ram_text (code that must run while the flash is busy: the flash driver), then .c1_text (none in Strata).
 * RAM: .data/.bss (72 KiB). POOL: big zero-initialised buffers, section .pool (the same SRAM: the RAM / POOL boundary
 * only splits the budget; build.py reads both lengths from here). STACKS with 256-byte guard bands; NOINIT survives
 * resets and UBOOT entry. 0x01C7FD50.. is reserved: boot_info, boot-state mailbox, vector table. */
MEMORY
{
    XIP    (rx)  : ORIGIN = 0x02000120, LENGTH = 0x8DFBC
    RAMTEXT (rwx): ORIGIN = 0x01C00000, LENGTH = 0x6000
    RAM    (rwx) : ORIGIN = 0x01C08000, LENGTH = 72K
    POOL   (rw)  : ORIGIN = 0x01C1A000, LENGTH = 0x5D000
    NOINIT (rw)  : ORIGIN = 0x01C7C000, LENGTH = 0x3D50
}
SECTIONS
{
    .text : { KEEP(*(.text.crt0)) *(.text*) *(.rodata*) . = ALIGN(4); _etext = .; } > XIP
    .ram_text : { _rt_start = .; KEEP(*(.ram_text*)) . = ALIGN(4); _rt_end = .; } > RAMTEXT AT > XIP
    _rt_load = LOADADDR(.ram_text);
    .c1_text : { _c1_start = .; KEEP(*(.c1_text*)) . = ALIGN(4); _c1_end = .; } > RAMTEXT AT > XIP
    _c1_load = LOADADDR(.c1_text);
    .data : { . = ALIGN(4); _data_start = .; *(.data*) *(.sdata*) . = ALIGN(4); _data_end = .; } > RAM AT > XIP
    _data_load = LOADADDR(.data);
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    .pool (NOLOAD) : { _pool_start = .; *(.pool*) . = ALIGN(4); _pool_end = .; } > POOL
    .noinit (NOLOAD) : { *(.noinit*) . = ALIGN(4); _noinit_end = .; } > NOINIT
    _guard0 = 0x01C77000; _ustack_lo = 0x01C77100; _ustack_top = 0x01C7A000;
    _guard1 = 0x01C7A000; _sstack_lo = 0x01C7A100; _sstack_top = 0x01C7C000;
    _spare0_lo = ALIGN(_c1_end, 256); _spare0_hi = 0x01C08000;
    _spare1_lo = ALIGN(_noinit_end, 256); _spare1_hi = 0x01C7FD00;
    /DISCARD/ : { *(.comment) *(.note*) }
}
