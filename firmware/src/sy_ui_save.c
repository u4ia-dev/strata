/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 u4ia */
/* STRATA's SAVE and preset browser (the STRATA UI design's SAVE AND PRESETS boards; ARCHITECTURE.md
 * §16), on sy_ui.c's pages, drawn with sy_gfx.c's kit:
 *   - SAVE tapped once (no second tap within UI_DTAP_MS): the sound saved at once as a new preset, its next version
 *     ("WARM PAD v2": one above the highest its base has in a user slot; a starter sound's "SUPERSAW v2"), in the
 *     first free slot, its category kept. Nothing opens: the keys keep playing.
 *   - SAVE held 5 s: from the hold time a countdown (a ring that empties), then the sound written over its own slot.
 *     Let go before, an OCT (SAVE + OCT: undo, redo), a knob or another button stop it; a sound from no user slot has
 *     none (let go: a toast says to tap).
 *   - SAVE tapped twice: the browser. The categories with their counts, the presets of the one picked (the starter
 *     sounds, then the user's), KNOB 1 the category, KNOB 2 the preset (it loads as it turns), KNOB 3 the bank (all,
 *     starter, user); SAVE twice or HOME: back; SAVE once: NAME IT for the sound playing.
 *   - NAME IT: the sound's name to edit. The keys type at the cursor: F3 .. F#5 A .. Z, G5 a space; OCT+ the numbers
 *     and symbols (and back), OCT- deletes; KNOB 1 the cursor, KNOB 2 the character there, KNOB 3 the category,
 *     KNOB 4 the slot (its own: a rename; a sound from no user slot: the first free one). SAVE writes it, HOME goes
 *     back to the browser unsaved.
 * Every write: the audio hushed around the flash work (sy_app.c).
 * Needs sy_ui.c, sy_ui_draw.c (ui_H, ui_hot, ui_ink, ui_boot_f), sy_gfx.c, sy_store.c and sy_app.c's presets
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

/* V0.87 (step 2): what NAME IT names (ui.sv_kind): 0 a sound (64 user slots, 16 characters), 1 a drum preset (64,
 * 11), 2 a kit (16, 12): a slot's name (0 empty), their count, a name's room, the first free slot */
static const char *ui_kind_name(uint32_t s)
{
    return ui.sv_kind == 1u ? dk_up_name(s) : ui.sv_kind == 2u ? dk_uk_name(s) : sy_app_slot_name(s);
}
static uint32_t ui_kind_n(void) { return ui.sv_kind == 1u ? DK_NUP : ui.sv_kind == 2u ? DK_NUK : SS_NUSER; }
static uint32_t ui_sv_max(void) { return ui.sv_kind == 1u ? 11u : ui.sv_kind == 2u ? 12u : 16u; }
static int32_t ui_kind_free(void)
{
    uint32_t s;
    for (s = 0; s < ui_kind_n(); s++)
        if (!ui_kind_name(s))
            return (int32_t)s;
    return -1;
}
/* name's next version: "BASE vN", N one above the highest version of its base a user slot holds (the base cut to
 * fit 16 characters; V0.87: the kind's slots, its room) */
static void ui_save_version(const char *name, char *out)
{
    char base[17], b2[17], suf[8];
    uint32_t s, top = ui_name_ver(name, base), v, n, k;
    const char *u;
    for (s = 0; s < ui_kind_n(); s++)
        if ((u = ui_kind_name(s)) != 0 && (v = ui_name_ver(u, b2)) > top && ui_streq(b2, base))
            top = v;
    sp_fix(sp_put(suf, " v"), (int32_t)top + 1, 0u, 0);
    n = ui_strlen(base);
    if (n + ui_strlen(suf) > ui_sv_max())
        n = ui_sv_max() - ui_strlen(suf);
    while (n && base[n - 1u] == ' ')
        n--;
    for (k = 0; k < n; k++)
        out[k] = base[k];
    sp_put(out + n, suf);
}

/* ---------------------------------------------------------------------------------------------- SAVE --- */
/* SAVE tapped once: the sound as a new preset at once, its next version in the first free slot (sy_ui_saved says
 * where) */
static void ui_save_new(void)
{
    int32_t fr = sy_app_slot_free();
    if (fr < 0) {
        ui_toast("EVERY USER SLOT IN USE");
        return;
    }
    ui_save_version(sy_patch.name, ui.sv_name);
    ui.sv_len = (uint8_t)ui_strlen(ui.sv_name);
    ui.sv_cat = sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER;
    ui.sv_slot = (uint8_t)fr;
    if (ui.saving || sy_app_save((uint32_t)fr, ui.sv_name, ui.sv_cat)) {
        ui_toast("BUSY: TRY AGAIN");
        return;
    }
    ui.saving = 1;
    ui.sv_over = 0;
    ui_toast("SAVING");
}

/* SAVE held: the countdown to writing over its own slot (ui_cd_tick), when the sound has one */
static void ui_save_hold(void)
{
    if (sy_app_slot() >= 0)
        ui_cd_arm(B_SAVE);
}

/* the slot NAME IT writes to is the sound's own: a rename */
static int ui_save_renaming(void) { return !ui.sv_kind && sy_app_slot() >= 0 && (uint32_t)sy_app_slot() == ui.sv_slot; }

/* NAME IT (SAVE in the browser): the sound playing's name and category to edit, into its own slot (a rename) or,
 * a sound from no user slot, the first free one */
static void ui_save_open(void)
{
    int32_t own = sy_app_slot(), fr = sy_app_slot_free();
    ui.sv_kind = 0;
    sp_put(ui.sv_name, sy_patch.name);
    ui.sv_len = (uint8_t)ui_strlen(ui.sv_name);
    ui.sv_cur = ui.sv_len;
    ui.sv_cat = sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER;
    ui.sv_sym = 0;
    ui.sv_key = 0xFF;
    ui.sv_slot = (uint8_t)(own >= 0 ? own : fr >= 0 ? fr : 0);
    ui_go(ui_first_page(SEC_SAVE));
}

/* .. (V0.86, PRESETS' list: SAVE + OCT+) for a new preset: its next version's name, the first free slot */
static void ui_save_open_new(void)
{
    int32_t fr = sy_app_slot_free();
    if (fr < 0) {
        ui_toast("EVERY USER SLOT IN USE");
        return;
    }
    ui.sv_kind = 0;
    ui_save_version(sy_patch.name, ui.sv_name);
    ui.sv_len = (uint8_t)ui_strlen(ui.sv_name);
    ui.sv_cur = ui.sv_len;
    ui.sv_cat = sy_patch.cat < CAT_COUNT ? sy_patch.cat : CAT_OTHER;
    ui.sv_sym = 0;
    ui.sv_key = 0xFF;
    ui.sv_slot = (uint8_t)fr;
    ui.sv_list = 1;
    ui_go(ui_first_page(SEC_SAVE));
}

/* V0.87 (step 2): NAME IT for a drum preset (kind 1: the row picked, its next version, its category) or a kit (2) */
static void ui_save_open_dk(uint32_t kind)
{
    int32_t fr;
    ui.sv_kind = (uint8_t)kind;
    if ((fr = ui_kind_free()) < 0) {
        ui_toast(kind == 1u ? "EVERY DRUM SLOT IN USE" : "EVERY KIT SLOT IN USE");
        ui.sv_kind = 0;
        return;
    }
    ui_save_version(kind == 1u ? dk_rname(ui.krow) : dkk.name, ui.sv_name);
    ui.sv_len = (uint8_t)ui_strlen(ui.sv_name);
    ui.sv_cur = ui.sv_len;
    ui.sv_cat = (uint8_t)(kind == 1u ? dk_row_cat(ui.krow) : 0u);
    ui.sv_sym = 0;
    ui.sv_key = 0xFF;
    ui.sv_slot = (uint8_t)fr;
    ui.sv_list = 1;
    ui_go(ui_first_page(SEC_SAVE));
}
/* .. the row picked (kind 1) or the kit (2) as a record named so (a row of its place's own model: that model named) */
static void ui_dk_rec(uint32_t kind, void *rec, const char *name, uint32_t cat)
{
    uint32_t i, n = kind == 1u ? 11u : 12u;
    char *to;
    if (kind == 1u) {
        dk_pre_t *p = (dk_pre_t *)rec;
        uint32_t v, m = dk_model(ui.krow, &v);
        memset(p, 0, sizeof *p);
        p->cat = (uint8_t)(cat % DC_N);
        p->l = dkk.l[ui.krow % DK_N];
        p->l.model = (uint8_t)(m + 1u), p->l.var = (uint8_t)v;
        p->fx = dkk.fx[ui.krow % DK_N];
        to = p->name;
    } else {
        dk_kit_t *k = (dk_kit_t *)rec;
        *k = dkk;
        memset(k->name, 0, sizeof k->name);
        k->kit = 0xFF;
        to = k->name;
    }
    for (i = 0; i < n && name[i]; i++)
        to[i] = name[i];
}
/* the SAVE window's SAVE on a drum track: the row picked over its user drum preset, the kit over its user kit */
static void ui_dk_over(void)
{
    int32_t own = ui_sw_own();
    if (own < 0) {
        ui_toast("NOTHING TO SAVE OVER");
        return;
    }
    ui_dk_rec(ui.sw_kind, &dk_rec, ui.sw_kind == 1u ? dk_up[own].name : dk_uk[own], ui.sw_kind == 1u ? dk_up[own].cat : 0u);
    if (ui.saving || sy_dk_put(ui.sw_kind - 1u, (uint32_t)own, &dk_rec)) {
        ui_toast("BUSY: TRY AGAIN");
        return;
    }
    ui.saving = 1;
    ui.sv_over = 1;
    ui.sv_kind = ui.sw_kind;
    ui.sv_slot = (uint8_t)own;
    ui_toast("SAVING");
}
/* sy_app.c: a drum preset (kind 0) or a kit (1) written to the user's record i, or deleted there (del), or failed */
static void sy_ui_dk_saved(int rc, uint32_t kind, uint32_t i, int del)
{
    char t[sizeof ui.toast], u[16], *e;
    ui.saving = 0;
    ui.dirty = D_ALL;
    if (rc) {
        sp_fix(sp_put(t, "SAVE FAILED: "), rc, 0u, 0);
        ui_toast(t);
        return;
    }
    sp_fix(sp_put(u, kind ? "KIT " : "DRUM "), (int32_t)i + 1, 0u, 0);
    if (del) {
        sp_put(sp_put(t, "DELETED "), u);
        if (kind && dkk.kit == (0x80u | i))
            dkk.kit = 0xFF;
    } else {
        if (kind) {
            dkk.kit = (uint8_t)(0x80u | i);
            memset(dkk.name, 0, sizeof dkk.name);
            memcpy(dkk.name, dk_uk[i], sizeof dkk.name - 1u);
            ui.kedited = 0;
            lpx.seq++;
        } else {
            *dk_pid(ui.krow) = (uint8_t)(0x80u | i);
        }
        if (ui.sv_over) {
            sp_put(sp_put(t, "SAVED OVER "), u);
        } else {
            e = sp_put(sp_put(t, "SAVED: "), kind ? dk_uk[i] : dk_up[i].name);
            sp_put(sp_put(e, " \xB7 "), u);
        }
    }
    ui_toast(t);
    if (ui_sec() == SEC_SAVE)
        ui_go(ui.sv_ret);
    ui.sv_list = 0;
}

static void ui_save_type(char c)                    /* a character in at the cursor */
{
    uint32_t i;
    if (ui.sv_len >= ui_sv_max()) {
        ui_toast(ui.sv_kind == 1u ? "11 CHARACTERS AT MOST" : ui.sv_kind == 2u ? "12 CHARACTERS AT MOST" : "16 CHARACTERS AT MOST");
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

static void ui_save_write(void)                     /* SAVE on NAME IT: the name, trimmed, into the slot */
{
    while (ui.sv_len && ui.sv_name[ui.sv_len - 1u] == ' ')
        ui.sv_name[--ui.sv_len] = 0;
    if (ui.sv_cur > ui.sv_len)
        ui.sv_cur = ui.sv_len;
    if (!ui.sv_len) {
        ui_toast("NAME IT FIRST");
        return;
    }
    if (ui.sv_kind) {                               /* (V0.87: a drum preset, a kit) */
        ui_dk_rec(ui.sv_kind, &dk_rec, ui.sv_name, ui.sv_cat);
        if (ui.saving || sy_dk_put(ui.sv_kind - 1u, ui.sv_slot, &dk_rec)) {
            ui_toast("BUSY: TRY AGAIN");
            return;
        }
        ui.saving = 1;
        ui.sv_over = 0;
        ui_toast("SAVING");
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

/* SAVE held 5 s: over its own slot */
static void ui_save_over(void)
{
    int32_t own = sy_app_slot();
    if (own < 0) {
        ui_toast("NOTHING TO SAVE OVER: TAP SAVE");
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

/* sy_app.c: the SAVE asked for is done (rc 0: "SAVED: WARM PAD v2 · USER 3", the name cut to fit, or "SAVED OVER
 * USER 3"; NAME IT back to the browser) or failed */
static void sy_ui_saved(int rc, uint32_t s)
{
    char t[sizeof ui.toast], u[16], *e;
    uint32_t k, room;
    ui.saving = 0;
    if (rc) {
        sp_fix(sp_put(t, "SAVE FAILED: "), rc, 0u, 0);
        ui_toast(t);
        return;
    }
    if (ui.sv_over) {
        sp_fix(sp_put(t, "SAVED OVER USER "), (int32_t)s + 1, 0u, 0);
    } else {
        sp_fix(sp_put(u, " \xB7 USER "), (int32_t)s + 1, 0u, 0);
        e = sp_put(t, "SAVED: ");
        room = (uint32_t)(sizeof t - 1u) - (uint32_t)(e - t) - ui_strlen(u);
        for (k = 0; sy_patch.name[k] && k < room; k++)
            *e++ = sy_patch.name[k];
        sp_put(e, u);
    }
    ui_toast(t);
    ui.edited = 0;                                  /* (V0.86: saved, nothing unsaved: HOME's dot gone) */
    if (ui_sec() == SEC_SAVE)                       /* (V0.86: from PRESETS' list, back to the page) */
        ui_go(ui.sv_list ? ui.sv_ret : ui_first_page(SEC_BROWSE));
    ui.sv_list = 0;
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
    ui.sv_list = 0;
    ui.br_cat = 0;
    ui.br_bank = 0;
    ui_go(ui_first_page(SEC_BROWSE));
}

/* OCT- held 3 s in the browser: the user preset playing deleted (the sound plays on, from no slot); SAVE + OCT-
 * straight after puts it back (sy_app.c keeps its bytes until the next SAVE) */
static int sy_app_delete(uint32_t s);               /* sy_app.c */
static int sy_app_undelete(void);
static void ui_browse_del(void)
{
    int32_t s = sy_app_slot();
    if (s < 0)
        return;
    if (ui.saving || sy_app_delete((uint32_t)s)) {
        ui_toast("BUSY: TRY AGAIN");
        return;
    }
    ui.saving = 1;
    ui_toast("DELETING");
}
static void ui_browse_undel(void)
{
    if (ui.saving || sy_app_undelete()) {
        ui_toast(ui.saving ? "BUSY: TRY AGAIN" : "NO ROOM TO PUT IT BACK");
        return;
    }
    ui.saving = 1;
    ui.undel = 0;
    ui_toast("PUTTING IT BACK");
}
static void sy_ui_deleted(int rc, uint32_t s)
{
    char t[sizeof ui.toast];
    int lst = ui.del_list;                          /* (V0.86: asked from PRESETS' list: no undo there) */
    ui.saving = 0;
    ui.del_list = 0;
    if (rc) {
        sp_fix(sp_put(t, "DELETE FAILED: "), rc, 0u, 0);
    } else {
        sp_put(sp_fix(sp_put(t, "DELETED USER "), (int32_t)s + 1, 0u, 0), lst ? "" : " \xB7 SAVE+OCT- UNDO");
        ui.undel = !lst;
    }
    ui_toast(t);
    ui.dirty |= D_HEAD | D_DISP | D_KNOBS;
}
static void sy_ui_restored(int rc, uint32_t s)
{
    char t[sizeof ui.toast];
    ui.saving = 0;
    if (rc)
        sp_fix(sp_put(t, "PUT BACK FAILED: "), rc, 0u, 0);
    else
        sp_fix(sp_put(t, "BACK IN USER "), (int32_t)s + 1, 0u, 0);
    ui_toast(t);
    ui.dirty |= D_HEAD | D_DISP | D_KNOBS;
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
            if (ui.sv_len >= ui_sv_max())
                return;
            ui.sv_name[ui.sv_len++] = ' ';
            ui.sv_name[ui.sv_len] = 0;
        }
        while (c < UI_NCHARS && UI_CHARS[c] != ui.sv_name[ui.sv_cur])
            c++;
        c = (uint32_t)(((int32_t)(c % UI_NCHARS) + s % (int32_t)UI_NCHARS + (int32_t)UI_NCHARS) % (int32_t)UI_NCHARS);
        ui.sv_name[ui.sv_cur] = UI_CHARS[c];
    } else if (item == UI_SCAT) {                   /* (V0.87: a drum preset's categories; a kit has none) */
        uint32_t n = ui.sv_kind == 1u ? DC_N : ui.sv_kind == 2u ? 1u : CAT_COUNT;
        ui.sv_cat = (uint8_t)((ui.sv_cat + (uint32_t)(s % (int32_t)n + (int32_t)n)) % n);
    } else if (item == UI_SSLOT) {
        uint32_t n = ui_kind_n();
        ui.sv_slot = (uint8_t)((ui.sv_slot + (uint32_t)(s % (int32_t)n + (int32_t)n)) % n);
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
    if (ui.sv_tap_t && (int32_t)(fm1_ms - ui.sv_tap_t) >= 0 && !ui.br_want && !sy_next_st) {   /* tapped once, no */
        ui.sv_tap_t = 0;                            /* second (a load first): a new version; in the browser NAME IT */
        if (ui_sec() == SEC_BROWSE)
            ui_save_open();
        else
            ui_sw_open();                           /* (V0.87, U17: the SAVE window) */
    }
    if (ui.sv_key != 0xFF && fm1_ms - ui.sv_key_t > 300u) {
        ui.sv_key = 0xFF;
        ui.dirty |= D_DISP;
    }
}

/* ------------------------------------------------------------------------------------------- drawing --- */
/* the knobs' names and values, as their flags say them */
static void ui_save_flag(int32_t id, char *t)
{
    static const char *const NAME[7] = {"CURSOR ", "LETTER ", "CATEGORY ", "SLOT ", "CATEGORY ", "PRESET ", "BANK "};
    static const char *const BANK[3] = {"ALL", "STARTER", "USER"};   /* (as HOME and the list name them) */
    char *e = sp_put(t, NAME[id - UI_SCUR]);
    if (id == UI_SCUR) {
        sp_fix(e, (int32_t)ui.sv_cur + 1, 0u, 0);
    } else if (id == UI_SCHR) {                     /* (at the end: one more to add) */
        char c = ui.sv_cur < ui.sv_len ? ui.sv_name[ui.sv_cur] : 0;
        if (!c)
            sp_put(e, "ADD");
        else if (c == ' ')
            sp_put(e, "SPACE");
        else
            e[0] = c, e[1] = 0;
    } else if (id == UI_SCAT) {
        sp_put(e, ui.sv_kind == 1u ? DC_NAME[ui.sv_cat % DC_N] : ui.sv_kind == 2u ? "KIT" : CAT_NAMES[ui.sv_cat]);
    } else if (id == UI_SSLOT) {
        sp_fix(sp_put(e, ui.sv_kind == 1u ? "DRUM " : ui.sv_kind == 2u ? "KIT " : "USER "), (int32_t)ui.sv_slot + 1, 0u, 0);
    } else if (id == UI_BCAT) {
        sp_put(e, ui.br_cat ? CAT_NAMES[ui.br_cat - 1u] : "ALL");
    } else if (id == UI_BBANK) {
        sp_put(e, BANK[ui.br_bank % 3u]);
    } else {
        uint16_t ids[SY_NFACT + SS_NUSER];
        uint32_t at, n = ui_browse_list(ids, &at);
        if (at < n)
            sp_fix(sp_put(sp_fix(e, (int32_t)at + 1, 0u, 0), " OF "), (int32_t)n, 0u, 0);
        else
            sp_fix(sp_put(e, "- OF "), (int32_t)n, 0u, 0);
    }
}

/* the top line's word: NAME IT (a rename: RENAME), the browser and the presets in its category */
static void ui_save_label(char *t)
{
    if (ui_sec() == SEC_SAVE) {
        sp_put(t, ui_save_renaming() ? "RENAME" : "NAME IT");
    } else {
        uint16_t ids[SY_NFACT + SS_NUSER];
        uint32_t at;
        sp_fix(sp_put(t, "BROWSER \xB7 "), (int32_t)ui_browse_list(ids, &at), 0u, 0);
    }
}

/* NAME IT: the name in its sixteen cells (the cursor's inverted; at the end the place to add one, +), the category
 * and the slot it goes to (a rename, one taken, an empty one), letters or numbers, and the 27 keys with what each
 * types (the one typed last bold a moment) */
static void ui_p_save(void)
{
    const char *occ = ui_kind_name(ui.sv_slot), *ch = ui.sv_sym ? UI_SYM : UI_ABC;
    const char *cat = ui.sv_kind == 1u ? DC_NAME[ui.sv_cat % DC_N] : ui.sv_kind == 2u ? "KIT" : CAT_NAMES[ui.sv_cat % CAT_COUNT];
    uint32_t i, k;
    float cx = 24.0f + 12.0f * (float)ui.sv_cur, x, w;
    char t[32], f[32];
    lk_kb_t kb;
    for (i = 0; i < 16u; i++) {
        float bx = 24.0f + 12.0f * (float)i;
        char c[2] = {i < ui.sv_len ? ui.sv_name[i] : i == ui.sv_len ? '+' : 0, 0};
        int cur = i == ui.sv_cur;
        if (cur)
            lk_box(bx, 33.0f, bx + 11.0f, 55.0f, ui_ink(ui_hot(0) || ui_hot(1)));
        lk_seg(bx + 1.0f, 58.0f, bx + 10.0f, 58.0f, cur ? 3.0f : 1.0f, cur ? ui_ink(ui_hot(0)) : sk.line);
        if (c[0] && c[0] != ' ')
            lk_text_c(bx + 5.5f, 36.0f, T14B, c, cur ? sk.ground : c[0] == '+' && i == ui.sv_len ? sk.lab : sk.ink,
                      cur ? ui_ink(ui_hot(0) || ui_hot(1)) : sk.ground, 0, 16);
    }
    w = lk_tw(T10B, cat, 20) + 12.0f;               /* the category, the slot */
    lk_frame(26.5f, 80.5f, w - 1.0f, 17.0f, 1.0f, ui_ink(ui_hot(2)));
    lk_text(32.5f, 82.0f, T10B, cat, ui_ink(ui_hot(2)), sk.ground, 20, 13);
    x = 26.0f + w;
    {
        char *e = sp_put(sp_fix(sp_put(t, ui.sv_kind == 1u ? "DRUM " : ui.sv_kind == 2u ? "KIT " : "USER "),
                                (int32_t)ui.sv_slot + 1, 0u, 0), " \xB7 ");
        sp_put(e, ui_save_renaming() ? "RENAME" : occ ? (ui_streq(occ, ui.sv_name) ? "SAVES OVER" : "REPLACES") : "EMPTY");
        lk_fit(f, sizeof f, t, T10B, 20, 226.0f - (x + 26.0f));
        ui_H(3, x + 4.0f, 84.0f);
        w = lk_tw(T10B, f, 20) + 12.0f;
        lk_frame(x + 20.5f, 80.5f, w - 1.0f, 17.0f, 1.0f, ui_ink(ui_hot(3)));
        lk_text(x + 26.5f, 82.0f, T10B, f, ui_ink(ui_hot(3)), sk.ground, 20, 13);
    }
    x = lk_chip(26.0f, 104.0f, "ABC", !ui.sv_sym);
    lk_chip(x, 104.0f, "123", ui.sv_sym);
    lk_label(x + 42.0f, 107.0f, "OCT+", 20);
    lk_kb(&kb, 8.0f, 138.0f, 224.0f, 52.0f);
    for (k = 0; k < 27u; k++) {                     /* what each key types: a black key's over it, a white key's under */
        char s[3] = {ch[k], 0, 0};
        int on = k == ui.sv_key;
        float kx = lk_kb_cx(&kb, k);
        if (ch[k] == ' ')
            s[0] = 'S', s[1] = 'P';
        if (LK_BLACK[k % 12u])
            lk_text_c(kx, 124.0f, on ? T9B : T9, s, on ? sk.ink : sk.lab, sk.ground, 0, 0);
        else
            lk_text_c(kx, 193.0f, on ? T9B : T9, s, on ? sk.ink : sk.lab, sk.ground, 0, 0);
    }
    ui_H(0, cx + 0.5f, 63.0f);
    ui_H(1, cx + 0.5f, 18.0f);
    ui_H(2, 8.0f, 84.0f);
}

/* the browser: the categories with their counts (the one picked bold), the presets of it (the one playing outlined,
 * a small bloom at its end; ST a starter sound, U and its slot a user's), the bank's chips */
static void ui_p_browse(void)
{
    static const char *const BANK[3] = {"ALL", "STARTER", "USER"};
    uint16_t ids[SY_NFACT + SS_NUSER];
    uint32_t at, n = ui_browse_list(ids, &at), k, first;
    char t[24];
    float x, sy = 70.0f;
    x = 26.0f;
    for (k = 0; k < 3u; k++)
        x = lk_chip(x, 22.0f, BANK[k], k == ui.br_bank % 3u);
    x = lk_text(26.0f, 44.0f, T14B, ui.br_cat ? CAT_NAMES[ui.br_cat - 1u] : "ALL", ui_ink(ui_hot(0)), sk.ground, 0, 17);
    sp_put(sp_fix(t, (int32_t)n, 0u, 0), n == 1u ? " PRESET" : " PRESETS");
    lk_text(x + 8.0f, 47.0f, T10, t, sk.lab, sk.ground, 20, 0);
    if (!n) {
        lk_text(26.0f, 74.0f, T10, "NONE HERE YET", sk.lab, sk.ground, 20, 0);
    } else {
        first = at < n && at > 2u ? at - 2u : 0u;   /* five rows, the one playing third when it can be */
        if (n > 5u && first > n - 5u)
            first = n - 5u;
        for (k = first; k < n && k < first + 5u; k++) {
            float y = 70.0f + (float)((k - first) * 28u);
            int on = k == at;
            uint8_t cat;
            const char *nm = sy_app_entry(ids[k], &cat);
            char up[17];
            uint32_t j;
            for (j = 0; nm && nm[j] && j < 16u; j++)    /* (the starter sounds' names are lowercase in their table; */
                up[j] = ids[k] < SY_USER && nm[j] >= 'a' && nm[j] <= 'z' ? (char)(nm[j] - 32) : nm[j];   /* a user's v2 stays) */
            up[j] = 0;
            lk_text(30.0f, y, T12B, lk_fit(t, sizeof t, nm ? up : "?", T12B, 0, 150.0f), on ? sk.ink : sk.lab, sk.ground, 0, 15);
            if (ids[k] >= SY_USER)
                sp_fix(sp_put(t, "U"), (int32_t)(ids[k] - SY_USER) + 1, 0u, 0);
            else
                sp_put(t, ids[k] + 1u < SY_NFACT ? "ST" : "");
            lk_text_r(208.0f, y + 2.0f, T9, t, sk.lab, sk.ground, 0, 0);
            if (on)
                sy = y;
        }
        if (at < n) {                               /* the one playing: outlined, its small bloom */
            uint32_t i;
            float vx = 0.0f, vy = -1.0f, cs = sy_cos2pi(1.0f / 60.0f), sn = sy_sin2pi(1.0f / 60.0f), px = 0.0f, py = 0.0f;
            lk_frame(22.5f, sy - 5.5f, 210.0f, 25.0f, 1.0f, ui_ink(ui_hot(1)));
            for (i = 0; i <= 60u; i++) {
                float r = 5.0f + 2.0f * ui_boot_f((float)i / 60.0f, 1), bx = 221.0f + r * vx, by = sy + 7.0f + r * vy, nx;
                if (i)
                    lk_wu(px, py, bx, by, 1.0f, sk.ink);
                px = bx;
                py = by;
                nx = vx * cs - vy * sn;
                vy = vx * sn + vy * cs;
                vx = nx;
            }
        }
    }
    ui_H(0, 8.0f, 49.0f);
    ui_H(1, 8.0f, sy + 2.0f);
    ui_H(2, 8.0f, 24.0f);
}

/* V0.87 (U17): the SAVE window (SAVE once): SAVE over the sound's own slot (none: a starter, INIT, a sound not saved:
 * shown, not to be picked), SAVE AS NEW (NAME IT on its next version's name), CANCEL; OCT- and OCT+ move, SAVE does it */
static void ui_p_sw(void)
{
    static const char *const R[3] = {"SAVE", "SAVE AS NEW", "CANCEL"};
    char t[40], nm[24], r0[24];
    int32_t sl = ui_sw_own();
    int own = sl >= 0;
    uint32_t i, kind = ui.sw_kind;
    const char *what = kind == 1u ? dk_rname(ui.krow) : kind == 2u ? dkk.name : sy_patch.name;
    lk_frame(14.5f, 44.5f, 211.0f, 150.0f, 1.5f, sk.hot);
    sp_put(sp_put(t, "SAVE "), what);
    lk_text(26.0f, 56.0f, T14B, lk_fit(nm, sizeof nm, t, T14B, 0, 190.0f), sk.hot, sk.ground, 0, 17);
    ui.sv_kind = (uint8_t)kind;                     /* (V0.87: the next version among the kind's own) */
    ui_save_version(what, nm);
    if (own)
        sp_fix(sp_put(r0, kind == 1u ? "OVER DRUM " : kind == 2u ? "OVER KIT " : "OVER USER "), sl + 1, 0u, 0);
    else
        sp_put(r0, kind ? "FACTORY'S STAY" : "STARTERS STAY");
    for (i = 0; i < 3u; i++) {
        float y = 80.0f + 30.0f * (float)i;
        int on = i == ui.sw_pos;
        uint16_t bg = on ? sk.ink : sk.ground;
        const char *rt = i == 0u ? r0 : i == 1u ? nm : 0;
        if (on)
            lk_box(22.0f, y, 218.0f, y + 25.0f, sk.ink);
        lk_text(30.0f, y + 5.0f, T12B, R[i], on ? sk.ground : !own && !i ? sk.lab : sk.ink, bg, 0, 15);
        if (rt)
            lk_text_r(212.0f, y + 8.0f, T9, lk_fit(t, sizeof t, rt, T9, 20, 100.0f), on ? sk.ground : sk.lab, bg, 20, 0);
    }
    lk_text(26.0f, 176.0f, T9, "OCT- OCT+ PICK \xB7 SAVE: DO IT", sk.lab, sk.ground, 20, 0);
}
