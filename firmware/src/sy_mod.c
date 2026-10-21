/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA modulation matrix (docs/ARCHITECTURE.md §11): eight slots of SOURCE, DESTINATION, AMOUNT (-100..100 %).
 * An amount of 100 % moves a destination across its whole range (the parameters' normalised values). Each block,
 * per voice, the slots add source x amount into the voice's offsets, one per modulated destination; destinations
 * that are not per voice (effects, macros, the volume) take the per-voice sources of the newest voice (the
 * effects are monophonic). Sources are the values at the end of the last block (no loops between the
 * envelopes, the LFOs and their own times: a block late, 0.7 ms). Macros are sources and destinations: slots
 * into a macro run first, from the macros as set. */
static const char *const SRC_NAMES[SRC_COUNT] = {"NONE", "ENV 1", "ENV 2", "LFO 1", "LFO 2", "VELOCITY", "NOTE",
                                                 "RANDOM", "ALTERNAT", "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4",
                                                 "MOD WHL", "BEND", "AFTERTCH", "EXPRESS", "FIXED"};

static struct {
    uint8_t n;                     /* destinations with a slot */
    uint16_t dst[NSLOT];           /* their parameter ids */
    uint8_t slot[NSLOT];           /* each slot's destination index (0xFF: unused) */
    uint8_t src[NSLOT];
    int16_t key[3 * NSLOT];        /* the slot parameters this was made from */
    uint8_t nz[2], z[2][NSLOT];    /* V0.81: each pass's offsets (0 the voices' and the effects', 1 the macros') */
    uint8_t na[2], as[2][NSLOT];   /* .. and its slots, in slot order (mod_offsets: the sums as before, bit for bit) */
} mm;
static uint8_t mod_idx[SP_COUNT];  /* a parameter's index in the offsets (0xFF: none) */
static float sy_src_g[SRC_COUNT];  /* global sources (macros, wheel, bend, aftertouch, expression, fixed) */
static float sy_goff[NSLOT];       /* the global offsets (effects, macros, the volume): sy_engine.c */

/* the destinations of the slots, rebuilt when a slot changes (a compare a block) */
static void mod_map(void)
{
    uint32_t s, i;
    for (s = 0; s < 3u * NSLOT && sy_patch.p[SP_M1_SRC + s] == mm.key[s]; s++)
        ;
    if (s == 3u * NSLOT)
        return;
    for (i = 0; i < SP_COUNT; i++)
        mod_idx[i] = 0xFF;
    mm.n = 0;
    for (s = 0; s < NSLOT; s++) {
        int32_t src = sy_patch.p[SP_M1_SRC + 3u * s], dst = sy_patch.p[SP_M1_DST + 3u * s];
        int32_t amt = sy_patch.p[SP_M1_AMT + 3u * s];
        mm.slot[s] = 0xFF;
        mm.src[s] = (uint8_t)src;
        if (src <= SRC_NONE || src >= SRC_COUNT || !amt || dst < 0 || dst >= SP_COUNT || !(SP[dst].flags & F_MOD))
            continue;
        if (mod_idx[dst] == 0xFF) {
            mod_idx[dst] = mm.n;
            mm.dst[mm.n++] = (uint16_t)dst;
        }
        mm.slot[s] = mod_idx[dst];
    }
    mm.nz[0] = mm.nz[1] = mm.na[0] = mm.na[1] = 0;
    for (i = 0; i < mm.n; i++) {
        uint32_t m = SP[mm.dst[i]].sec == SEC_MACRO;
        mm.z[m][mm.nz[m]++] = (uint8_t)i;
    }
    for (s = 0; s < NSLOT; s++)
        if (mm.slot[s] != 0xFF) {
            uint32_t m = SP[mm.dst[mm.slot[s]]].sec == SEC_MACRO;
            mm.as[m][mm.na[m]++] = (uint8_t)s;
        }
    for (s = 0; s < 3u * NSLOT; s++)
        mm.key[s] = sy_patch.p[SP_M1_SRC + s];
}

/* offsets for one voice (vs: its per-voice sources, indexed by SRC_*) or the global set (vs = 0: the newest
 * voice's sources in gv). macros: only the slots into macros (their pass), else all the others */
static void mod_offsets(float *off, const float *vs, int macros)
{
    uint32_t k, m = macros ? 1u : 0u;
    for (k = 0; k < mm.nz[m]; k++)
        off[mm.z[m][k]] = 0.0f;
    for (k = 0; k < mm.na[m]; k++) {
        uint32_t s = mm.as[m][k], src = mm.src[s];
        float v = src <= SRC_VOICE_LAST ? (vs ? vs[src] : 0.0f) : sy_src_g[src];
        off[mm.slot[s]] += v * (float)sy_patch.p[SP_M1_AMT + 3u * s] * 0.001f;
    }
}

/* a parameter's value for a voice: its smoothed normalised value plus the voice's offset, 0..1 */
static inline float mod_get(const float *off, uint32_t id)
{
    uint32_t d = mod_idx[id];
    return d == 0xFF ? sp_n[id] : sy_clampf(sp_n[id] + off[d], 0.0f, 1.0f);
}

/* a note value (NOTE_Q) moved by parameter id's offset (an LFO's RATE: faster; the delay's TIME: longer), in whole
 * values: 100 % across all of them */
static inline uint32_t mod_note(const float *off, uint32_t id, int32_t note, int32_t dir)
{
    uint32_t d = mod_idx[id];
    if (off && d != 0xFF)
        note += dir * sy_floori(off[d] * (float)(SY_NNOTE - 1u) + 0.5f);
    return (uint32_t)sy_clampi(note, 0, (int32_t)SY_NNOTE - 1);
}
