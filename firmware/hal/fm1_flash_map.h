/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The FM-1 SPI NOR's map: the ranges the firmware may erase and program (the allow-list), and the check of a range
 * against them. fm1_flash.h's RAM driver checks every erase and program against FL_RANGE_OK; storage_hw.c checks
 * FL_STORE_OK first. Plain constants and macros, no code: the host tests and the browser emulator (Strata's) use the
 * same ranges as the firmware. (Moved here unchanged from fm1_flash.h.) */
#pragma once
#include <stdint.h>

#define FL_DATA_LO      0x00097000u                /* Felucca main store */
#define FL_DATA_HI      0x000E0000u
#define FL_GLOB_LO      0x000FC000u                /* Felucca superblock / globals */
#define FL_GLOB_HI      0x000FF000u
#define FL_OTA_LO       0x000E0000u                /* M-UPGRADE loader staging, ota.c */
#define FL_OTA_HI       0x000E5000u
#define FL_AUTO_LO      0x000E5000u                /* 1.2: the autosave (storage.c OBJ_AUTOSAVE), A/B, just above */
#define FL_AUTO_HI      0x000E7000u                /* the OTA staging */
#define FL_VBANK_LO     0x000E7000u                /* 1.4.1: the FM6 voice bank (fm6_vbank.c), the sector above */
#define FL_VBANK_HI     0x000E8000u                /* (below the stock OTA's VM backup, 0xE8000: never written) */
/* [off, off + n) inside [lo, hi), without wrapping: off + n can overflow, and
 * the 1 MiB part ignores the high address bits, so a wrapped range lands low. */
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
/* Felucca's own store (projects, user samples; settings; the autosave and the FM6 voice bank: one range) */
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI) || \
                             FL_IN(off, n, FL_AUTO_LO, FL_VBANK_HI))
