/* SPDX-License-Identifier: GPL-3.0-only
 * After Felucca's tests/storage_test.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments). */
/* firmware/src/sy_store.c, Strata's flash, against a simulated NOR (erase: 0xFF; a program only clears bits; a page
 * write must not wrap) whose hooks refuse anything outside the firmware's allow-list (hal/fm1_flash_map.h FL_STORE_OK),
 * as storage_hw.c does:
 *   build/host/sy_store_test
 * an empty flash loads nothing; an object saved loads back, the copies alternating; a write torn after any byte (the
 * payload or the commit record) leaves the copy before; a part that ignores writes (protected) does not report SAVED;
 * Felucca's records in the same sectors (magic "FELU") read as none; user slots: a slot written keeps its pair's
 * others, an empty slot reads empty, every slot of all 128 round trips a packed patch; nothing is ever written outside
 * the allow-list. */
#define SY_ENGINE 1
#include "sy_host.h"
#include "../firmware/hal/fm1_flash_map.h"

static uint8_t nor[0x100000];
static int fail_bytes = -1;                               /* power lost after this many programmed bytes */
static int protect;                                       /* a write-protected part: erases and programs do nothing */
static uint32_t outside;                                  /* erases or programs refused: outside the allow-list */

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (!FL_STORE_OK(off, 4096u) || (off & 4095u)) {
        outside++;
        return -8;
    }
    if (!protect)
        memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (!FL_STORE_OK(off, n)) {
        outside++;
        return -8;
    }
    if ((off & 0xFFu) + n > 256u) {
        printf("store: a page write wraps at %#x\n", off);
        exit(1);
    }
    for (i = 0; i < n; i++) {
        if (fail_bytes == 0)
            return -9;
        if (fail_bytes > 0)
            fail_bytes--;
        if (!protect)
            nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/sy_store.c"

int main(void)
{
    static uint8_t a[SS_PAYLOAD_MAX], b[SS_PAYLOAD_MAX], got[SS_PAYLOAD_MAX];
    uint8_t pk[SP_PACK_MAX], back[SP_PACK_MAX];
    uint32_t i, s, cut;
    int n, ok;
    t_name = "store";
    memset(nor, 0xFF, sizeof nor);
    for (i = 0; i < sizeof a; i++) {
        a[i] = (uint8_t)(i * 7u + 1u);
        b[i] = (uint8_t)(i * 13u + 5u);
    }
    check("an empty flash loads nothing", ss_load(SS_SETTINGS, got, sizeof got) < 0 && ss_load(SS_EDIT, got, sizeof got) < 0);
    check("saved, loaded back", ss_save(SS_EDIT, a, 1000u) == 0 && ss_load(SS_EDIT, got, sizeof got) == 1000 &&
                                    !memcmp(got, a, 1000u));
    check("saved again: the other copy, the newest wins", ss_save(SS_EDIT, b, 1200u) == 0 &&
                                                              ss_load(SS_EDIT, got, sizeof got) == 1200 &&
                                                              !memcmp(got, b, 1200u) &&
                                                              ss_head(SS_EDIT, 0, &(ss_hdr_t){0}) == 0 &&
                                                              ss_head(SS_EDIT, 1, &(ss_hdr_t){0}) == 0);
    ok = 1;                                               /* a cut after any byte: the copy before stays */
    for (cut = 0; cut < 1000u + sizeof(ss_hdr_t); cut += 7u) {
        static uint8_t keep[sizeof nor];
        memcpy(keep, nor, sizeof nor);
        fail_bytes = (int)cut;
        ss_save(SS_EDIT, a, 1000u);
        fail_bytes = -1;
        n = ss_load(SS_EDIT, got, sizeof got);
        ok &= n == 1200 && !memcmp(got, b, 1200u);
        memcpy(nor, keep, sizeof nor);
    }
    check("a write torn after any byte (payload or record): the copy before loads", ok);
    protect = 1;
    check("a part that ignores writes: not reported saved, the old copy loads", ss_save(SS_EDIT, a, 1000u) == -7 &&
                                                                                    ss_load(SS_EDIT, got, sizeof got) == 1200 &&
                                                                                    !memcmp(got, b, 1200u));
    protect = 0;
    {                                                     /* Felucca's record in Strata's sector: none here */
        uint32_t off = ss_sector(SS_SETTINGS, 0);
        ss_hdr_t h = {0x554C4546u, SS_SETTINGS, 0, 9, 16, 0, {0xFFFFFFFFu, 0xFFFFFFFFu}, 0};
        memset(nor + off, 0xFF, 8192);
        h.crc = ss_crc32(nor + off + SS_PAYLOAD_OFF, 16);
        h.hcrc = ss_crc32(&h, sizeof h - 4u);
        memcpy(nor + off, &h, sizeof h);
        check("Felucca's record (FELU) in the same sector reads as none", ss_load(SS_SETTINGS, got, sizeof got) < 0);
    }
    {                                                     /* user slots */
        sy_patch_t p, q;
        uint32_t len;
        int all = 1, kept;
        sp_defaults(&p);
        check("an empty slot reads empty", ss_user_get(0, back, sizeof back) == 0 && ss_user_get(127, back, sizeof back) == 0);
        for (s = 0; s < SS_NUSER; s++) {                  /* every slot: a patch of its own */
            p.p[SP_A_POS] = (int16_t)(s * 7u);
            p.name[0] = (char)('A' + s % 26u);
            len = sp_pack(&p, pk);
            all &= ss_user_put(s, pk, len) == 0;
        }
        for (s = 0; s < SS_NUSER; s++) {
            n = ss_user_get(s, back, sizeof back);
            all &= n > 0 && sp_unpack(&q, back, (uint32_t)n) && q.p[SP_A_POS] == (int16_t)(s * 7u) &&
                   q.name[0] == (char)('A' + s % 26u);
        }
        check("all 128 slots: a packed patch each, round trip", all);
        sp_defaults(&p);                                  /* slot 7 again: its pair's others (5 .. 9) kept */
        p.p[SP_A_POS] = 999;
        len = sp_pack(&p, pk);
        kept = ss_user_put(7, pk, len) == 0;
        for (s = 5; s < 10u; s++) {
            n = ss_user_get(s, back, sizeof back);
            kept &= n > 0 && sp_unpack(&q, back, (uint32_t)n) && q.p[SP_A_POS] == (s == 7u ? 999 : (int16_t)(s * 7u));
        }
        check("a slot written: its pair's other slots kept", kept);
        check("a slot emptied reads empty", ss_user_put(8, pk, 0) == 0 && ss_user_get(8, back, sizeof back) == 0 &&
                                                ss_user_get(9, back, sizeof back) > 0);
    }
    check("every object's sectors inside the allow-list (no erase or program refused)", outside == 0);
    check("the hooks refuse outside the allow-list (the loader's area, the stock firmware's backup)",
          st_erase(0x00000u) == -8 && st_erase(0xE8000u) == -8 && st_prog(0xE0000u, a, 16) == -8 && outside == 3);
    return t_done();
}
