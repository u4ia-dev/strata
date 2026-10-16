/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's values as text: a list parameter's names (sp_list), a section's name (SEC_NAMES, SEC_SHORT) and a value
 * as its unit shows it (sp_text), for the UI's knob cards and the browser workbench. At most SP_TEXT_N - 1
 * characters, Latin-1 (the degree sign is 0xB0, as the firmware's fonts have it). No printf: the firmware has none. */
#define SP_TEXT_N 16u

static const char *const SEC_NAMES[SEC_COUNT] = {"OSC A", "OSC B", "SUB", "NOISE", "FILTER", "ENV 1", "ENV 2",
                                                 "LFO 1", "LFO 2", "VOICE", "MACROS", "MATRIX", "DRIVE", "CHORUS",
                                                 "DELAY", "REVERB", "OTT"};
static const char *const SEC_SHORT[SEC_COUNT] = {"A", "B", "SUB", "NZ", "FLT", "E1", "E2", "L1", "L2", "VCE", "MAC",
                                                 "MTX", "DRV", "CHO", "DLY", "REV", "OTT"};
static const char *const PHMODE_NAMES[2] = {"RETRIG", "FREE"};
static const char *const UMODE_NAMES[2] = {"LINEAR", "SUPER"};
static const char *const ROUTE_NAMES[2] = {"FILTER", "DIRECT"};
static const char *const LFOM_NAMES[3] = {"FREE", "RETRIG", "ENV"};
static const char *const SYNCM_NAMES[2] = {"FREE", "FIXED"};
static const char *const VMODE_NAMES[3] = {"POLY", "MONO", "LEGATO"};
static const char *const GLMODE_NAMES[2] = {"ALWAYS", "LEGATO"};

/* a list parameter's names and their count (0: not a list) */
static const char *const *sp_list(uint32_t id, uint32_t *n)
{
    switch (id) {
    case SP_A_WARP: case SP_B_WARP: *n = WARP_COUNT; return WARP_NAMES;
    case SP_A_PHMODE: case SP_B_PHMODE: *n = 2u; return PHMODE_NAMES;
    case SP_A_UMODE: case SP_B_UMODE: *n = 2u; return UMODE_NAMES;
    case SP_A_ROUTE: case SP_B_ROUTE: case SP_SUB_ROUTE: case SP_NZ_ROUTE: *n = 2u; return ROUTE_NAMES;
    case SP_L1_SHAPE: case SP_L2_SHAPE: *n = LFO_NSHAPE; return LFO_NAMES;
    case SP_L1_MODE: case SP_L2_MODE: *n = 3u; return LFOM_NAMES;
    case SP_L1_SYNC: case SP_L2_SYNC: case SP_DL_SYNC: *n = 2u; return SYNCM_NAMES;
    case SP_SUB_SHAPE: *n = SUB_NSHAPE; return SUB_NAMES;
    case SP_NZ_TYPE: *n = 3u; return NZ_NAMES;
    case SP_F_TYPE: *n = FT_COUNT; return FT_NAMES;
    case SP_V_MODE: *n = 3u; return VMODE_NAMES;
    case SP_V_GLMODE: *n = 2u; return GLMODE_NAMES;
    case SP_DR_TYPE: *n = 4u; return DRV_NAMES;
    case SP_DL_MODE: *n = 2u; return DLY_NAMES;
    case SP_RV_TYPE: *n = 2u; return RV_NAMES;
    default: *n = 0; return 0;
    }
}

static char *sp_put(char *b, const char *s)
{
    while (*s)
        *b++ = *s++;
    *b = 0;
    return b;
}

/* v / 10^dec with dec decimals; sign: a '+' before a positive value */
static char *sp_fix(char *b, int32_t v, uint32_t dec, int sign)
{
    char t[12];
    uint32_t u = (uint32_t)(v < 0 ? -v : v), n = 0, k;
    if (v < 0)
        *b++ = '-';
    else if (sign && v > 0)
        *b++ = '+';
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u || n <= dec);
    for (k = n; k > 0; k--) {
        *b++ = t[k - 1u];
        if (k - 1u == dec && dec)
            *b++ = '.';
    }
    *b = 0;
    return b;
}

/* x rounded to dec decimals, then a unit */
static char *sp_real(char *b, float x, uint32_t dec, const char *unit)
{
    static const float P10[4] = {1.0f, 10.0f, 100.0f, 1000.0f};
    b = sp_fix(b, (int32_t)(x * P10[dec] + (x < 0.0f ? -0.5f : 0.5f)), dec, 0);
    return sp_put(b, unit);
}

/* a time in seconds: ms under a second, s above */
static char *sp_secs(char *b, float s)
{
    if (s < 0.00995f)
        return sp_real(b, s * 1000.0f, 1u, " ms");
    if (s < 0.9995f)
        return sp_real(b, s * 1000.0f, 0u, " ms");
    return sp_real(b, s, s < 9.995f ? 2u : 1u, " s");
}

/* a frequency: Hz to 1 kHz, kHz above */
static char *sp_freq(char *b, float f, uint32_t lowdec)
{
    if (f < 9.995f)
        return sp_real(b, f, lowdec, " Hz");
    if (f < 99.95f)
        return sp_real(b, f, 1u, " Hz");
    if (f < 999.5f)
        return sp_real(b, f, 0u, " Hz");
    return sp_real(b, f * 0.001f, f < 9995.0f ? 2u : 1u, " kHz");
}

/* parameter id's value v as text into b (SP_TEXT_N bytes) */
static void sp_text(uint32_t id, int32_t v, char *b)
{
    const sp_desc_t *d = &SP[id];
    float n = sp_norm_of(id, v);
    const char *const *names;
    uint32_t cnt;
    b[0] = 0;
    switch (d->unit) {
    case U_LIST:
        names = sp_list(id, &cnt);
        sp_put(b, names && v >= 0 && (uint32_t)v < cnt ? names[v] : "?");
        break;
    case U_ONOFF: sp_put(b, v ? "ON" : "OFF"); break;
    case U_PCT: sp_put(sp_fix(b, v, 1u, 0), "%"); break;
    case U_BIPCT: sp_put(sp_fix(b, v, 1u, 1), "%"); break;
    case U_CENT: sp_put(sp_fix(b, v, 0u, 1), " ct"); break;
    case U_SEMI: sp_put(sp_fix(b, v, 0u, d->lo < 0), " st"); break;
    case U_OCT: sp_fix(b, v, 0u, 1); break;
    case U_DEG: sp_put(sp_fix(b, v, 0u, 0), "\xB0"); break;
    case U_HZ: sp_freq(b, sp_cut_hz(n), 1u); break;
    case U_TIME0:
        if (v == d->lo) {
            sp_put(b, "OFF");
            break;
        }
        /* fall through */
    case U_TIME: sp_secs(b, sp_time_s(n)); break;
    case U_LFOHZ: sp_freq(b, sp_lfo_hz(n), 2u); break;
    case U_DB:
        if (v <= 0)
            sp_put(b, "OFF");
        else
            sp_put(sp_fix(b, v - 600, 1u, 1), " dB");
        break;
    case U_DBX: sp_put(sp_fix(b, v, 1u, 1), " dB"); break;
    case U_TABLE: sp_put(b, v >= 0 && v < (int32_t)WT_NFACT ? WT_NAMES[v] : "?"); break;
    case U_SRC: sp_put(b, v >= 0 && v < (int32_t)SRC_COUNT ? SRC_NAMES[v] : "?"); break;
    case U_DST:
        if (v < 0 || v >= (int32_t)SP_COUNT || !(SP[v].flags & F_MOD))
            sp_put(b, "NONE");
        else
            sp_put(sp_put(sp_put(b, SEC_SHORT[SP[v].sec]), " "), SP[v].name);
        break;
    case U_SYNC: sp_put(b, v >= 0 && v < (int32_t)SY_NNOTE ? NOTE_NAMES[v] : "?"); break;
    case U_MS: sp_real(b, sp_dly_s(n) * 1000.0f, 0u, " ms"); break;
    default: sp_fix(b, v, 0u, 0); break;                       /* U_INT */
    }
}
