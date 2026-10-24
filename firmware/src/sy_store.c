/* SPDX-License-Identifier: GPL-3.0-only
 * After Felucca's src/storage.c (Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0). */
/* STRATA's flash (docs/ARCHITECTURE.md §16): the user presets, the settings and the sound playing (back at power-on),
 * each an object as Felucca keeps its own: an A/B sector pair. A save goes to the copy that is not the current one:
 * the sector erased, the payload programmed from offset 256, then the 32-byte commit record LAST (magic, type, copy,
 * sequence, length, the payload's CRC, the record's CRC); a load takes the valid copy with the newest sequence, so a
 * write torn anywhere leaves the copy before in charge, and a save is read back before it counts. Strata's records
 * carry their own magic ("STRA"): Felucca's objects in the same sectors read as none here, Strata's as none there.
 * Flash through three hooks: st_read, st_erase, st_prog (storage_hw.c on the FM-1, erasing and programming only inside
 * FL_STORE_OK, which the RAM driver checks again as FL_RANGE_OK; the emulator's, with the same check), and every
 * object's sectors are checked against those ranges at compile time. The main loop's only.
 *   user presets   64 slots (V0.8; 128 before), 5 to a sector pair (768 bytes each: a length, 0 empty, and a packed
 *                  patch), 13 pairs from 0x97000 (Felucca's projects and user samples: a Strata install takes them
 *                  over)
 *   settings       0xFC000 / 0xFD000 (Felucca's settings pair)
 *   the sound      0xE5000 / 0xE6000 (Felucca's autosave pair, above the update loader's staging)
 *   projects       V0.8, the looper's four (sy_loop.c): four objects each, a sector pair each; projects 1 and 2 from
 *                  0xCB000, just above where the 128 user presets ended (free since Strata took the store over), 3
 *                  and 4 from 0xB1000, where presets 65 .. 104 were: a project there is saved only while those
 *                  sectors hold no older Strata record (ss_foreign: presets an older build kept are never written
 *                  over)
 * Needs the hooks, FL_IN and the FL_* ranges (hal/fm1_flash.h; the emulator's copy), sy_param.c (SP_PACK_MAX). */
#define SS_MAGIC 0x41525453u                       /* "STRA" */
#define SS_SECTOR 4096u
#define SS_PAYLOAD_OFF 256u
#define SS_PAYLOAD_MAX (SS_SECTOR - SS_PAYLOAD_OFF)
#define SS_NUSER 64u                               /* user preset slots (V0.8: 128 before; the rest the projects') */
#define SS_SLOTS 5u                                /* .. to a sector pair */
#define SS_SLOT_BYTES 768u                         /* .. each: its length (2 bytes, little endian; 0: empty), the patch */
#define SS_NPAIRS ((SS_NUSER + SS_SLOTS - 1u) / SS_SLOTS)
#define SS_USER_BASE 0x97000u
#define SS_SET_BASE 0xFC000u
#define SS_EDIT_BASE 0xE5000u
#define SS_NPRJ 4u                                 /* V0.8: the looper's projects */
#define SS_PRJ_OBJS 4u                             /* .. their objects each (sy_loop.c LP_NOBJ) */
#define SS_PRJ_BYTES (SS_PRJ_OBJS * 2u * SS_SECTOR)
#define SS_PRJ_LO 0xCB000u                         /* .. 1 and 2 (where the 128 presets ended) */
#define SS_PRJ_HI (SS_USER_BASE + SS_NPAIRS * 2u * SS_SECTOR)   /* .. 3 and 4 (presets 65 .. 104 before) */
#define SS_DK_BASE (SS_PRJ_LO + 2u * SS_PRJ_BYTES)   /* V0.87 (D3): the user's drum presets, then kits (0xDB000) */
enum { SS_SETTINGS, SS_EDIT, SS_USER0, SS_PRJ0 = SS_USER0 + SS_NPAIRS, SS_DRUMS = SS_PRJ0 + SS_NPRJ * SS_PRJ_OBJS,
       SS_KITS, SS_NOBJ };
_Static_assert(SS_DK_BASE == 0xDB000u && FL_IN(SS_DK_BASE, 4u * SS_SECTOR, FL_DATA_LO, FL_DATA_HI) &&
               SS_DK_BASE + 4u * SS_SECTOR <= FL_OTA_LO, "the user's drums and kits: the main store, below the update's");
_Static_assert(SS_SLOTS * SS_SLOT_BYTES <= SS_PAYLOAD_MAX, "a pair's slots fit its payload");
_Static_assert(SP_PACK_MAX + 2u <= SS_SLOT_BYTES, "a packed patch fits a slot");
_Static_assert(FL_IN(SS_USER_BASE, SS_NPAIRS * 2u * SS_SECTOR, FL_DATA_LO, FL_DATA_HI), "user presets: the main store");
_Static_assert(FL_IN(SS_SET_BASE, 2u * SS_SECTOR, FL_GLOB_LO, FL_GLOB_HI), "settings: the globals");
_Static_assert(FL_IN(SS_EDIT_BASE, 2u * SS_SECTOR, FL_AUTO_LO, FL_AUTO_HI), "the sound playing: the autosave pair");
_Static_assert(SS_NPAIRS == 13u && SS_PRJ_HI == 0xB1000u, "the 64 presets: 13 pairs, 0x97000 .. 0xB0FFF");
_Static_assert(SS_USER_BASE + 26u * 2u * SS_SECTOR == SS_PRJ_LO, "projects 1, 2: where the 128 presets ended");
_Static_assert(FL_IN(SS_PRJ_LO, 2u * SS_PRJ_BYTES, FL_DATA_LO, FL_DATA_HI), "projects 1, 2: the main store");
_Static_assert(SS_PRJ_LO + 2u * SS_PRJ_BYTES <= FL_OTA_LO, "projects 1, 2: below the update's staging");
_Static_assert(FL_IN(SS_PRJ_HI, 2u * SS_PRJ_BYTES, FL_DATA_LO, FL_DATA_HI), "projects 3, 4: the main store");
_Static_assert(SS_PRJ_HI + 2u * SS_PRJ_BYTES <= SS_PRJ_LO, "projects 3, 4: under projects 1, 2");

typedef struct {
    uint32_t magic;
    uint16_t type, copy;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} ss_hdr_t;
_Static_assert(sizeof(ss_hdr_t) == 32u, "the commit record's layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint8_t ss_buf[SS_PAYLOAD_MAX] __attribute__((aligned(4)));   /* a payload (the driver programs from RAM) */

static uint32_t ss_crc32(const void *p, uint32_t n)   /* zlib's CRC-32, four bits a step */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return ~c;
}

static uint32_t ss_sector(uint32_t obj, uint32_t copy)   /* copy A (0) or B (1) of an object */
{
    if (obj == SS_SETTINGS)
        return SS_SET_BASE + copy * SS_SECTOR;
    if (obj == SS_EDIT)
        return SS_EDIT_BASE + copy * SS_SECTOR;
    if (obj == SS_DRUMS || obj == SS_KITS)         /* (V0.87: the user's drums, kits at 0xDB000, 0xDD000) */
        return SS_DK_BASE + (obj - SS_DRUMS) * 2u * SS_SECTOR + copy * SS_SECTOR;
    if (obj >= SS_PRJ0) {                          /* (a project's object: 1, 2 at 0xCB000; 3, 4 at 0xB1000) */
        uint32_t pj = (obj - SS_PRJ0) / SS_PRJ_OBJS, o = (obj - SS_PRJ0) % SS_PRJ_OBJS;
        return (pj < 2u ? SS_PRJ_LO + pj * SS_PRJ_BYTES : SS_PRJ_HI + (pj - 2u) * SS_PRJ_BYTES) + o * 2u * SS_SECTOR +
               copy * SS_SECTOR;
    }
    return SS_USER_BASE + (obj - SS_USER0) * 2u * SS_SECTOR + copy * SS_SECTOR;
}

static int ss_head(uint32_t obj, uint32_t copy, ss_hdr_t *h)   /* the commit record valid: 0 */
{
    if (obj >= SS_NOBJ || copy > 1u || st_read(ss_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != SS_MAGIC || h->type != obj || h->copy != copy || h->len > SS_PAYLOAD_MAX ||
        h->hcrc != ss_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int ss_body(uint32_t obj, uint32_t copy, const ss_hdr_t *h)   /* the payload into ss_buf, its CRC good: 0 */
{
    if (st_read(ss_sector(obj, copy) + SS_PAYLOAD_OFF, ss_buf, h->len) || ss_crc32(ss_buf, h->len) != h->crc)
        return -1;
    return 0;
}

/* the current copy (its payload left in ss_buf, *h its record): the newest valid sequence (A on a tie, the
 * sequence wrapping), -1 when neither is valid */
static int ss_current(uint32_t obj, ss_hdr_t *h)
{
    ss_hdr_t a, b;
    int va = ss_head(obj, 0, &a) == 0, vb = ss_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (ss_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && ss_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && ss_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* an object's sectors hold a valid Strata record of another object (an older build's: presets 65 .. 104 where projects
 * 3 and 4 go): 1, never written over */
static int ss_foreign(uint32_t obj)
{
    uint32_t copy;
    for (copy = 0; copy < 2u; copy++) {
        ss_hdr_t h;
        if (obj >= SS_NOBJ || st_read(ss_sector(obj, copy), &h, sizeof h))
            return 1;
        if (h.magic == SS_MAGIC && h.hcrc == ss_crc32(&h, sizeof h - 4u) && h.type != obj)
            return 1;
    }
    return 0;
}

/* an object into dst (max bytes): its length, -1 none (or too long) */
static int ss_load(uint32_t obj, void *dst, uint32_t max)
{
    ss_hdr_t h;
    uint32_t i;
    if (ss_current(obj, &h) < 0 || h.len > max)
        return -1;
    for (i = 0; i < h.len; i++)
        ((uint8_t *)dst)[i] = ss_buf[i];
    return (int)h.len;
}

/* an object saved (src may be ss_buf itself): 0, else the driver's error, -7 when the copy read back is not it */
static int ss_save(uint32_t obj, const void *src, uint32_t len)
{
    ss_hdr_t h, chk;
    uint32_t base, off, to, seq;
    int cur, rc;
    if (obj >= SS_NOBJ || len > SS_PAYLOAD_MAX)
        return -1;
    if (src != ss_buf)
        for (off = 0; off < len; off++)
            ss_buf[off] = ((const uint8_t *)src)[off];
    {                                               /* the current copy's sequence (its payload not needed) */
        ss_hdr_t a, b;
        int va = ss_head(obj, 0, &a) == 0, vb = ss_head(obj, 1, &b) == 0;
        cur = vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u)) ? 1 : va ? 0 : -1;
        seq = cur < 0 ? 0u : cur ? b.seq : a.seq;
    }
    to = cur == 0 ? 1u : 0u;                        /* the other copy */
    base = ss_sector(obj, to);
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u)
        if ((rc = st_prog(base + SS_PAYLOAD_OFF + off, ss_buf + off, len - off > 256u ? 256u : len - off)) != 0)
            return rc;
    h.magic = SS_MAGIC;
    h.type = (uint16_t)obj;
    h.copy = (uint16_t)to;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = ss_crc32(ss_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = ss_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)    /* the commit record, last */
        return rc;
    if (ss_head(obj, to, &chk) || memcmp(&chk, &h, sizeof h) || ss_body(obj, to, &chk))   /* (a part that did not */
        return -7;                                  /* take it must not report SAVED) */
    return 0;
}

/* --------------------------------------------------------------------------------------- user slots --- */
/* user slot s's packed patch into dst (max bytes): its length; 0 empty (its pair never saved, or no copy of it
 * readable: nothing to give back either way) */
static int ss_user_get(uint32_t s, uint8_t *dst, uint32_t max)
{
    uint32_t at = (s % SS_SLOTS) * SS_SLOT_BYTES, len, i;
    int n;
    if (s >= SS_NUSER)
        return 0;
    n = ss_load(SS_USER0 + s / SS_SLOTS, ss_buf, SS_PAYLOAD_MAX);   /* (in place: ss_buf onto itself) */
    if (n < 0 || (uint32_t)n < at + 2u)
        return 0;
    len = (uint32_t)ss_buf[at] | (uint32_t)ss_buf[at + 1u] << 8;
    if (!len || len > SS_SLOT_BYTES - 2u || len > max || at + 2u + len > (uint32_t)n)
        return 0;
    for (i = 0; i < len; i++)
        dst[i] = ss_buf[at + 2u + i];
    return (int)len;
}

/* user slot s set to a packed patch (len 0: emptied), its pair's other slots kept: 0 */
static int ss_user_put(uint32_t s, const uint8_t *src, uint32_t len)
{
    uint32_t obj = SS_USER0 + s / SS_SLOTS, at = (s % SS_SLOTS) * SS_SLOT_BYTES, i;
    int n;
    if (s >= SS_NUSER || len > SS_SLOT_BYTES - 2u)
        return -1;
    n = ss_load(obj, ss_buf, SS_PAYLOAD_MAX);
    for (i = n < 0 ? 0u : (uint32_t)n; i < SS_SLOTS * SS_SLOT_BYTES; i++)
        ss_buf[i] = 0;                              /* (a pair never saved, or saved shorter: empty slots) */
    ss_buf[at] = (uint8_t)len;
    ss_buf[at + 1u] = (uint8_t)(len >> 8);
    for (i = 0; i < len; i++)
        ss_buf[at + 2u + i] = src[i];
    return ss_save(obj, ss_buf, SS_SLOTS * SS_SLOT_BYTES);
}

/* ---------------------------------------------------------------------------------------- records --- */
/* V0.87 (D3): object obj as n records of sz bytes: record i into dst (0: read; never saved, or none readable: -1) */
static int ss_rec_get(uint32_t obj, uint32_t i, uint32_t sz, uint32_t n, void *dst)
{
    int got;
    if (i >= n || (got = ss_load(obj, ss_buf, SS_PAYLOAD_MAX)) < 0 || (uint32_t)got < (i + 1u) * sz)
        return -1;
    memcpy(dst, ss_buf + i * sz, sz);
    return 0;
}
/* .. record i set to src's sz bytes (0: emptied, zeros), the others kept: 0 */
static int ss_rec_put(uint32_t obj, uint32_t i, uint32_t sz, uint32_t n, const void *src)
{
    int got;
    uint32_t k;
    if (i >= n || n * sz > SS_PAYLOAD_MAX)
        return -1;
    got = ss_load(obj, ss_buf, SS_PAYLOAD_MAX);
    for (k = got < 0 ? 0u : (uint32_t)got; k < n * sz; k++)
        ss_buf[k] = 0;
    for (k = 0; k < sz; k++)
        ss_buf[i * sz + k] = src ? ((const uint8_t *)src)[k] : 0u;
    return ss_save(obj, ss_buf, n * sz);
}
