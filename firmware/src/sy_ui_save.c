/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA's SAVE and preset browser (the Strata Screen Design canvas, "Saving and presets", approved; ARCHITECTURE.md
 * §16), on sy_ui.c's pages and sy_ui_draw.c's regions:
 *   - SAVE tapped: its screen. A sound never saved is named first (its name to start from, the next free slot); one
 *     saved before is offered its next version, "NAME v2" (one above the highest a user slot holds), in the next
 *     free slot, the one before kept. The keys type at the cursor: F3 .. F#5 A .. Z, G5 a space; OCT+ the numbers
 *     and symbols (and back), OCT- deletes; KNOB 1 the cursor, KNOB 2 the character there, KNOB 3 the category,
 *     KNOB 4 the slot. SAVE writes it (the audio hushed around the flash work: sy_app.c), HOME goes back unsaved.
 *   - SAVE tapped twice: the sound written over its own slot at once (one never saved: named first).
 *   - SAVE held and let go (no OCT meanwhile: SAVE + OCT is undo, redo): the browser. The categories with their
 *     counts, the presets of the one picked (factory, then the user's), KNOB 1 the category, KNOB 2 the preset (it
 *     loads as it turns), KNOB 3 the bank (all, factory, user); HOME back, SAVE the SAVE screen for what is loaded.
 * Needs sy_ui.c, sy_ui_draw.c (WX WY WW WH, ui_text_c, ui_pill), sy_gfx.c, sy_store.c and sy_app.c's presets
 * (declared in sy_ui.c). */

static const char UI_ABC[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";   /* the keys F3 .. G5: letters, a space */
static const char UI_SYM[28] = "1234567890-._/#+&'!?(),:*@ ";   /* .. OCT+: numbers and symbols, a space */
static const char UI_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._/#+&'!?(),:*@";   /* KNOB 2's round */
#define UI_NCHARS (sizeof UI_CHARS - 1u)

static uint32_t ui_strlen(const char *s)
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}
static int ui_streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a name's base and version: "WARM PAD v3": "WARM PAD", 3; a name without " vN": itself, 1 */
static uint32_t ui_name_ver(const char *n, char *base)
{
    uint32_t len = ui_strlen(n), i = len, v = 0, m = 1, k;
    while (i > 0u && n[i - 1u] >= '0' && n[i - 1u] <= '9' && m <= 100000u) {
        v += (uint32_t)(n[i - 1u] - '0') * m;
        m *= 10u;
        i--;
    }
    if (i < len && i >= 2u && n[i - 1u] == 'v' && n[i - 2u] == ' ' && v) {
        for (k = 0; k < i - 2u; k++)
            base[k] = n[k];
        base[k] = 0;
        return v;
    }
    for (k = 0; k <= len; k++)
        base[k] = n[k];
    return 1u;
}

/* name's next version: "BASE vN", N one above the highest version of its base a user slot holds (the base cut to
 * fit 16 characters) */
static void ui_save_version(const char *name, char *out)
{
    char base[17], b2[17], suf[8];
    uint32_t s, top = ui_name_ver(name, base), v, n, k;
    const char *u;
    for (s = 0; s < SS_NUSER; s++)
        if ((u = sy_app_slot_name(s)) != 0 && (v = ui_name_ver(u, b2)) > top && ui_streq(b2, base))
            top = v;
    sp_fix(sp_put(suf, " v"), (int32_t)top + 1, 0u, 0);
    n = ui_strlen(base);
    if (n + ui_strlen(suf) > 16u)
        n = 16u - ui_strlen(suf);
    while (n && base[n - 1u] == ' ')
        n--;
    for (k = 0; k < n; k++)
        out[k] = base[k];
    sp_put(out + n, suf);
}

/* ---------------------------------------------------------------------------------------------- SAVE --- */
static void ui_save_open(void)
{
    int32_t own = sy_app_slot(), fr = sy_app_slot_free();
    uint32_t sec = ui_sec();
    if (sec != SEC_SAVE && sec != SEC_BROWSE)
        ui.sv_ret = ui.page;
    ui.sv_ver = own >= 0;
    if (ui.sv_ver)
        ui_save_version(sy_patch.name, ui.sv_name);
    else
        sp_put(ui.sv_name, sy_patch.name);
    ui.sv_len = (uint8_t)ui_strlen(ui.sv_name);
    ui.sv_cur = ui.sv_len;
    ui.sv_cat = sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER;
    ui.sv_sym = 0;
    ui.sv_key = 0xFF;
    ui.sv_slot = (uint8_t)(fr >= 0 ? fr : own >= 0 ? own : 0);
    ui_go(ui_first_page(SEC_SAVE));
}

static void ui_save_type(char c)                    /* a character in at the cursor */
{
    uint32_t i;
    if (ui.sv_len >= 16u) {
        ui_toast("16 CHARACTERS AT MOST");
        return;
    }
    for (i = ui.sv_len; i > ui.sv_cur; i--)
        ui.sv_name[i] = ui.sv_name[i - 1u];
    ui.sv_name[ui.sv_cur++] = c;
    ui.sv_name[++ui.sv_len] = 0;
    ui.dirty |= D_DISP | D_KNOBS;
}

static void ui_save_del(void)                       /* OCT-: the character before the cursor out */
{
    uint32_t i;
    if (!ui.sv_cur)
        return;
    for (i = ui.sv_cur - 1u; i < ui.sv_len; i++)
        ui.sv_name[i] = ui.sv_name[i + 1u];
    ui.sv_cur--;
    ui.sv_len--;
    ui.dirty |= D_DISP | D_KNOBS;
}

static void ui_save_write(void)                     /* SAVE on its screen: the name, trimmed, into the slot */
{
    while (ui.sv_len && ui.sv_name[ui.sv_len - 1u] == ' ')
        ui.sv_name[--ui.sv_len] = 0;
    if (ui.sv_cur > ui.sv_len)
        ui.sv_cur = ui.sv_len;
    if (!ui.sv_len) {
        ui_toast("NAME IT FIRST");
        return;
    }
    if (ui.saving || sy_app_save(ui.sv_slot, ui.sv_name, ui.sv_cat)) {
        ui_toast("BUSY: TRY AGAIN");
        return;
    }
    ui.saving = 1;
    ui.sv_over = 0;
    ui_toast("SAVING");
}

/* SAVE tapped twice: over its own slot at once (one never saved: its screen, to name it) */
static void ui_save_over(void)
{
    int32_t own = sy_app_slot();
    if (own < 0) {
        ui_save_open();
        ui_toast("NAME IT FIRST");
        return;
    }
    if (ui.saving || sy_app_save((uint32_t)own, sy_patch.name, sy_patch.cat)) {
        ui_toast("BUSY: TRY AGAIN");
        return;
    }
    ui.saving = 1;
    ui.sv_over = 1;
    ui.sv_slot = (uint8_t)own;
    ui_toast("SAVING");
}

/* sy_app.c: the SAVE asked for is done (rc 0) or failed */
static void sy_ui_saved(int rc, uint32_t s)
{
    char t[32], *e;
    ui.saving = 0;
    if (rc) {
        sp_fix(sp_put(t, "SAVE FAILED: "), rc, 0u, 0);
        ui_toast(t);
        return;
    }
    e = sp_put(t, ui.sv_over ? "SAVED OVER USER " : "SAVED: USER ");
    sp_fix(e, (int32_t)s + 1, 0u, 0);
    ui_toast(t);
    if (ui_sec() == SEC_SAVE)
        ui_go(ui.sv_ret);
    ui.dirty |= D_HEAD | D_DISP;
}

/* the keys on SAVE's screen: a character each (1: taken, no note) */
static int ui_save_key(uint32_t k)
{
    if (k >= 27u)
        return 1;
    ui_save_type((ui.sv_sym ? UI_SYM : UI_ABC)[k]);
    ui.sv_key = (uint8_t)k;
    ui.sv_key_t = fm1_ms;
    return 1;
}

/* -------------------------------------------------------------------------------------------- browser --- */
static void ui_browse_open(void)
{
    uint32_t sec = ui_sec();
    if (sec != SEC_SAVE && sec != SEC_BROWSE)
        ui.sv_ret = ui.page;
    ui.br_cat = 0;
    ui.br_bank = 0;
    ui_go(ui_first_page(SEC_BROWSE));
}

/* the browser's presets (its bank and category) and where the one playing is among them (n: not there) */
static uint32_t ui_browse_list(uint16_t *ids, uint32_t *at)
{
    uint32_t n = sy_app_list(ui.br_bank, ui.br_cat, ids), now = ui.br_want ? ui.br_want - 1u : sy_app_entry_now(), k;
    for (k = 0; k < n && ids[k] != now; k++)
        ;
    *at = k;
    return n;
}

/* the SAVE screen's and the browser's knobs */
static void ui_save_item_turn(int32_t item, int32_t s)
{
    if (item == UI_SCUR) {
        ui.sv_cur = (uint8_t)sy_clampi((int32_t)ui.sv_cur + s, 0, ui.sv_len);
    } else if (item == UI_SCHR) {                   /* the character at the cursor (at the end: one more) */
        uint32_t c = 0;
        if (ui.sv_cur == ui.sv_len) {
            if (ui.sv_len >= 16u)
                return;
            ui.sv_name[ui.sv_len++] = ' ';
            ui.sv_name[ui.sv_len] = 0;
        }
        while (c < UI_NCHARS && UI_CHARS[c] != ui.sv_name[ui.sv_cur])
            c++;
        c = (uint32_t)(((int32_t)(c % UI_NCHARS) + s % (int32_t)UI_NCHARS + (int32_t)UI_NCHARS) % (int32_t)UI_NCHARS);
        ui.sv_name[ui.sv_cur] = UI_CHARS[c];
    } else if (item == UI_SCAT) {
        ui.sv_cat = (uint8_t)((ui.sv_cat + (uint32_t)(s % (int32_t)CAT_COUNT + (int32_t)CAT_COUNT)) % CAT_COUNT);
    } else if (item == UI_SSLOT) {
        ui.sv_slot = (uint8_t)((ui.sv_slot + (uint32_t)(s % (int32_t)SS_NUSER + (int32_t)SS_NUSER)) % SS_NUSER);
    } else if (item == UI_BCAT) {
        ui.br_cat = (uint8_t)((ui.br_cat + (uint32_t)(s % 9 + 9)) % 9u);
    } else if (item == UI_BBANK) {
        ui.br_bank = (uint8_t)((ui.br_bank + (uint32_t)(s % 3 + 3)) % 3u);
    } else if (item == UI_BPRE) {                   /* the preset: loaded as it turns (a load in flight: after it) */
        uint16_t ids[SY_NFACT + SS_NUSER];
        uint32_t at, n = ui_browse_list(ids, &at);
        if (!n)
            return;
        at = at == n ? (s > 0 ? 0u : n - 1u) : (uint32_t)sy_clampi((int32_t)at + s, 0, (int32_t)n - 1);
        ui.br_want = (uint16_t)(ids[at] + 1u);
    }
    ui.dirty |= D_DISP | D_KNOBS | D_HEAD;
}

static void ui_save_ms(void)                        /* every millisecond: a preset waiting to load; SAVE's tap */
{
    if (ui.br_want && !sy_next_st) {
        sy_app_entry_load(ui.br_want - 1u);
        ui.br_want = 0;
        ui.dirty |= D_DISP | D_KNOBS | D_HEAD;
    }
    if (ui.sv_tap_t && (int32_t)(fm1_ms - ui.sv_tap_t) >= 0) {   /* tapped once, no second: the screen */
        ui.sv_tap_t = 0;
        ui_save_open();
    }
    if (ui.sv_key != 0xFF && fm1_ms - ui.sv_key_t > 300u) {
        ui.sv_key = 0xFF;
        ui.dirty |= D_DISP;
    }
}

/* ------------------------------------------------------------------------------------------- drawing --- */
/* the knobs' cells: a pill each */
static void ui_save_cell(int32_t id, int act)
{
    static const char *const NAME[7] = {"CURSOR", "LETTER", "CATEGORY", "SLOT", "CATEGORY", "PRESET", "BANK"};
    static const char *const BANK[3] = {"ALL", "FACTORY", "USER"};
    char t[16];
    if (id == UI_SCUR) {
        sp_fix(t, (int32_t)ui.sv_cur + 1, 0u, 0);
    } else if (id == UI_SCHR) {                     /* (at the end: one more to add) */
        char c = ui.sv_cur < ui.sv_len ? ui.sv_name[ui.sv_cur] : 0;
        if (!c)
            sp_put(t, "ADD");
        else if (c == ' ')
            sp_put(t, "SPACE");
        else
            t[0] = c, t[1] = 0;
    } else if (id == UI_SCAT) {
        sp_put(t, CAT_NAMES[ui.sv_cat]);
    } else if (id == UI_SSLOT) {
        sp_fix(sp_put(t, "USER "), (int32_t)ui.sv_slot + 1, 0u, 0);
    } else if (id == UI_BCAT) {
        sp_put(t, ui.br_cat ? CAT_NAMES[ui.br_cat - 1u] : "ALL");
    } else if (id == UI_BBANK) {
        sp_put(t, BANK[ui.br_bank % 3u]);
    } else {
        uint16_t ids[SY_NFACT + SS_NUSER];
        uint32_t at, n = ui_browse_list(ids, &at);
        if (at < n)
            sp_fix(sp_put(sp_fix(t, (int32_t)at + 1, 0u, 0), "/"), (int32_t)n, 0u, 0);
        else
            sp_fix(sp_put(t, "-/"), (int32_t)n, 0u, 0);
    }
    ui_pill(t, 0, act);
    ui_text_c(0, 46, 60, &AF_XS, NAME[id - UI_SCUR], act ? sk.now : sk.label, sk.ground);
}

/* the header's right: SAVE's slot, the browser's category and place */
static void ui_save_head(char *t)
{
    if (ui_sec() == SEC_SAVE) {
        sp_fix(sp_put(t, "USER "), (int32_t)ui.sv_slot + 1, 0u, 0);
    } else {
        uint16_t ids[SY_NFACT + SS_NUSER];
        uint32_t at, n = ui_browse_list(ids, &at);
        char *e = sp_put(sp_put(t, ui.br_cat ? CAT_NAMES[ui.br_cat - 1u] : "ALL"), " ");
        if (at < n)
            e = sp_fix(e, (int32_t)at + 1, 0u, 0);
        else
            e = sp_put(e, "-");
        sp_fix(sp_put(e, "/"), (int32_t)n, 0u, 0);
    }
}

/* the 27 keys, F3 .. G5, each with what it types (the one typed last lit a moment) */
static void ui_save_keys(int32_t y)
{
    static const uint8_t BLACK[12] = {0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0};   /* from F */
    const char *ch = ui.sv_sym ? UI_SYM : UI_ABC;
    uint32_t k, w = 0, pass;
    for (pass = 0; pass < 2u; pass++)               /* the white keys, then the black over them */
        for (k = 0, w = 0; k < 27u; k++) {
            int black = BLACK[k % 12u], on = k == ui.sv_key;
            char s[6];
            if (!black) {
                if (!pass) {
                    int32_t x = WX + 5 + (int32_t)(w * 27u) / 2;
                    uint16_t bg = on ? sk.now : sk.raised;
                    cv_rrect(x, y, 13, 52, 2, on ? sk.now : sk.rline2, sk.display);
                    cv_rrect(x + 1, y + 1, 11, 50, 1, bg, on ? sk.now : sk.rline2);
                    if (ch[k] == ' ')
                        sp_put(s, "SP");
                    else
                        s[0] = ch[k], s[1] = 0;
                    cv_text_on(x + (13 - text_w(&AF_XS, s)) / 2, y + 38, &AF_XS, s, on ? sk.ground : sk.text, bg);
                }
                w++;
            } else if (pass) {
                int32_t x = WX + 5 + (int32_t)(w * 27u) / 2 - 5;
                uint16_t bg = on ? sk.now : sk.ground;
                cv_rrect(x, y, 9, 30, 2, on ? sk.now : sk.rline, sk.raised);
                cv_rrect(x + 1, y + 1, 7, 28, 1, bg, on ? sk.now : sk.rline);
                s[0] = ch[k];
                s[1] = 0;
                cv_text_on(x + (9 - text_w(&AF_XS, s)) / 2, y + 17, &AF_XS, s, on ? sk.ground : sk.label, bg);
            }
        }
}

/* SAVE's screen: what it is (a new sound to name, the next version), the name with its cursor (a version's suffix in
 * SOUND), what happens to the slot, the keys' map */
static void ui_g_save(void)
{
    char t[40], *e;
    const char *occ = sy_app_slot_name(ui.sv_slot);
    int32_t x, i, cx;
    cv_text_on(WX + 4, WY + 3, &AF_XS, ui.sv_ver ? "SAVE THE NEXT VERSION" : "NAME THE SOUND", sk.dim, sk.display);
    sp_put(sp_fix(t, ui.sv_len, 0u, 0), "/16");
    cv_text_r(WX + WW - 4, WY + 3, &AF_XS, t, sk.faint, sk.display);
    cv_rrect(WX + 4, WY + 17, WW - 8, 24, 3, sk.rline2, sk.display);   /* the name, its cursor */
    cv_rrect(WX + 5, WY + 18, WW - 10, 22, 2, sk.raised, sk.rline2);
    {
        char base[17];
        uint32_t ver = ui_name_ver(ui.sv_name, base), bl = ver > 1u ? ui_strlen(base) : ui.sv_len;
        x = cv_text_on(WX + 10, WY + 21, &AF_M, base, sk.text, sk.raised);
        if (ver > 1u)
            x = cv_text_on(x, WY + 21, &AF_M, ui.sv_name + bl, sk.sound, sk.raised);
        for (cx = WX + 10, i = 0; i < (int32_t)ui.sv_cur; i++) {   /* (the cursor after its characters) */
            char c[2] = {ui.sv_name[i], 0};
            cx += text_w(&AF_M, c);
        }
        (void)x;
        cv_rect(cx, WY + 22, 2, 15, sk.now);
    }
    if (occ && !ui_streq(occ, ui.sv_name))          /* the slot: empty, or what it holds */
        e = sp_put(sp_put(t, "REPLACES "), occ);
    else if (ui.sv_ver && sy_app_slot() >= 0 && (uint32_t)sy_app_slot() != ui.sv_slot)
        e = sp_fix(sp_put(sp_put(sp_put(t, sy_patch.name), " STAYS IN USER "), ""), sy_app_slot() + 1, 0u, 0);
    else
        e = sp_put(t, occ ? "WRITES OVER ITSELF" : "AN EMPTY SLOT");
    (void)e;
    ui_text_c(WX + 4, WY + 43, WW - 8, &AF_XS, t, occ && !ui_streq(occ, ui.sv_name) ? sk.label : sk.faint, sk.display);
    ui_save_keys(WY + 54);
}

/* the browser: the categories with their counts (the one picked raised), the presets of it, the one playing marked */
static void ui_g_browse(void)
{
    uint16_t ids[SY_NFACT + SS_NUSER], all[SY_NFACT + SS_NUSER];
    uint32_t at, n = ui_browse_list(ids, &at), c, k, first, na = sy_app_list(ui.br_bank, 0, all);
    char t[24];
    for (c = 0; c < 9u; c++) {                      /* ALL, then each category */
        uint32_t cnt = 0;
        int32_t y = WY + 3 + (int32_t)c * 11;
        int on = c == ui.br_cat;
        uint8_t cat;
        for (k = 0; k < na; k++)
            cnt += !c || (sy_app_entry(all[k], &cat) && cat + 1u == c);
        if (on)
            cv_rrect(WX + 3, y, 68, 11, 2, sk.raised, sk.display);
        cv_text_on(WX + 7, y + CAP_IN(XS, 11), &AF_XS, c ? CAT_NAMES[c - 1u] : "ALL",
                   on ? sk.text : cnt ? sk.text2 : sk.faint, on ? sk.raised : sk.display);
        sp_fix(t, (int32_t)cnt, 0u, 0);
        cv_text_r(WX + 67, y + CAP_IN(XS, 11), &AF_XS, t, on ? sk.label : sk.faint, on ? sk.raised : sk.display);
    }
    cv_rect(WX + 74, WY + 4, 1, 100, sk.dline);
    if (!n) {
        ui_text_c(WX + 78, WY + 40, 145, &AF_XS, "NONE HERE YET", sk.faint, sk.display);
        return;
    }
    first = at < n && at > 2u ? at - 2u : 0u;       /* five rows, the one playing in the middle when it can be */
    if (n > 5u && first > n - 5u)
        first = n - 5u;
    for (k = first; k < n && k < first + 5u; k++) {
        int32_t y = WY + 3 + (int32_t)(k - first) * 18;
        int on = k == at;
        uint8_t cat;
        const char *nm = sy_app_entry(ids[k], &cat);
        uint16_t bg = on ? sk.raised : sk.display;
        char up[17];
        uint32_t j;
        for (j = 0; nm && nm[j] && j < 16u; j++)    /* (the starter sounds' names are lowercase in their table) */
            up[j] = nm[j] >= 'a' && nm[j] <= 'z' ? (char)(nm[j] - 32) : nm[j];
        up[j] = 0;
        if (on) {
            cv_rrect(WX + 78, y, 145, 16, 3, sk.rline2, sk.display);
            cv_rrect(WX + 79, y + 1, 143, 14, 2, sk.raised, sk.rline2);
            cv_rect(WX + 83, y + 4, 2, 9, sk.now);
        }
        cv_text_on(WX + 89, y + CAP_IN(XS, 16), &AF_XS, nm ? up : "?", on ? sk.text : sk.text2, bg);
        if (ids[k] >= SY_USER)
            sp_fix(sp_put(t, "USER "), (int32_t)(ids[k] - SY_USER) + 1, 0u, 0);
        else
            sp_put(t, ids[k] + 1u < SY_NFACT ? "STARTER" : "");
        cv_text_r(WX + 218, y + CAP_IN(XS, 16), &AF_XS, t, on ? sk.label : sk.faint, bg);
    }
    cv_text_on(WX + 78, WY + 95, &AF_XS, "SAVE + OCT-: UNDO A LOAD", sk.faint, sk.display);
}
