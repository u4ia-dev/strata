/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 brandonmbarone */
/* STRATA engine (docs/ARCHITECTURE.md §3, §13): notes to voices, the block, the CPU guard, the tables.
 * Audio side (the ALNK0 interrupt): sy_note_on / sy_note_off / sy_cc / sy_bend / .. at a block's start (the panel's
 * keys and MIDI alike), then sy_block renders 32 frames: the parameters' smoothing, the matrix's global part, the
 * FREE LFOs, every voice, the volume, the effects (sy_fx.c), the master (DC block, clip) into Q15 for audio.c.
 * Main loop: sy_service builds a wavetable when an oscillator's TABLE changes (the oscillator fades out over a
 * block first and back in after), sy_patch_load changes the sound (the voices fade out first).
 * Voices: POLY takes a free voice, else steals (a voice in release, the quietest; then the oldest held note that is
 * neither the newest nor the lowest), the stolen voice fading out over 3 ms before it plays the new note. MONO and
 * LEGATO play one voice, the last note held (a stack); LEGATO does not restart the envelopes while notes are held.
 * Glide: GLIDE's time, always or (GL MODE LEGATO) only between held notes. The unison budget: the sounding
 * oscillator instances (voices x oscillators x unison) stay under sy_uni_budget; over it, the oldest voices lose a
 * unison voice a block (faded) before any voice is stolen. The CPU guard (sy_cpu) shrinks the budget, then the
 * effects' cost, then sheds voices, and undoes it in reverse when the load falls. */
#define SY_STACK 16u
#define SY_UNI_BUDGET 48u
static void sy_note_off(uint32_t note);
static sy_patch_t sy_next;                   /* a sound to change to (main loop -> audio side) */
static volatile uint8_t sy_next_st;          /* 1: asked for; 2: the voices are fading out */

static float sy_mix_l[SY_BLK], sy_mix_r[SY_BLK];
static float sy_vol;                         /* the volume at the end of the last block */
static float sy_dc[2][2];                    /* the master's DC blocker: last input, last output */
static uint8_t sy_stack[SY_STACK], sy_nstack;   /* MONO / LEGATO: notes held, the last on top */
static uint8_t sy_sustain;
static uint32_t sy_uni_budget = SY_UNI_BUDGET;
static uint8_t sy_wheel, sy_at, sy_expr = 127;
static lfo_t sy_glfo[2];
static struct {                              /* the CPU guard */
    uint32_t load_q8;                        /* the last half's load, smoothed (256 = 100 %) */
    uint8_t level;                           /* budget steps taken, 0..SY_GUARD_FX */
    uint8_t fx_lo;                           /* effects at low cost (OTT upward off, reverb half its lines) */
    uint8_t over;                            /* halves in a row over 85 % */
    uint16_t calm;                           /* halves in a row under 55 % */
    uint32_t shed;                           /* voices shed */
} sy_g;

/* ------------------------------------------------------------------------------------- the tables --- */
static uint8_t wt_build_o = 0xFF;            /* the oscillator whose table is being built */

/* main loop: an oscillator whose TABLE changed fades out, its table is built a frame a call, it fades in */
static void sy_service(void)
{
    uint32_t o;
    if (wt_build_o != 0xFF) {
        wt_table_t *t = &wt_tab[wt_build_o];
        if (!t->quiet)
            return;                          /* the audio has not let go of it yet */
        if (!wg_step())
            return;
        t->n = wg.n;
        t->id = wg.id;
        t->quiet = 0;
        t->busy = 0;
        wt_build_o = 0xFF;
        return;
    }
    for (o = 0; o < SY_NOSC; o++) {
        uint32_t id = (uint32_t)sy_patch.p[o ? SP_B_TABLE : SP_A_TABLE];
        if (id < WT_NFACT && (id != wt_tab[o].id || !wt_tab[o].n)) {
            wt_tab[o].quiet = 0;
            wt_tab[o].busy = 1;
            wg_begin(id, wt_pool[o * SY_NFRAME]);
            wt_build_o = (uint8_t)o;
            return;
        }
    }
}

/* audio side, each block: a busy table's oscillator fades to 0 over a block, then lets go (quiet); a ready one
 * fades back in */
static void sy_table_fades(void)
{
    uint32_t o;
    for (o = 0; o < SY_NOSC; o++) {
        wt_g[o][0] = wt_g[o][1];
        if (wt_tab[o].busy) {
            if (wt_g[o][0] <= 0.0f)
                wt_tab[o].quiet = 1;
            wt_g[o][1] = 0.0f;
        } else {
            wt_g[o][1] = wt_tab[o].n ? 1.0f : 0.0f;
        }
    }
}

/* -------------------------------------------------------------------------------------- the notes --- */
static uint32_t sy_poly(void)
{
    int32_t p = sy_patch.p[SP_V_POLY];
    return sy_patch.p[SP_V_MODE] ? 1u : (uint32_t)(p < 1 ? 1 : p > (int32_t)SY_NVOICE ? (int32_t)SY_NVOICE : p);
}

static int sy_glide_on(int legato)
{
    return sy_patch.p[SP_V_GLIDE] > 0 && (sy_patch.p[SP_V_GLMODE] == 0 || legato);
}

/* the voice to steal: a released one (the quietest), else the oldest held that is neither the newest nor the
 * lowest note */
static sy_voice_t *sy_steal_pick(uint32_t np)
{
    sy_voice_t *best = 0;
    uint32_t i, lowest = 128, newest = 0;
    for (i = 0; i < np; i++)
        if (sy_v[i].on && !sy_v[i].steal) {
            if (!sy_v[i].gate && (!best || sy_v[i].amp < best->amp))
                best = &sy_v[i];
            if (sy_v[i].gate) {
                lowest = sy_v[i].note < lowest ? sy_v[i].note : lowest;
                newest = sy_v[i].age > newest ? sy_v[i].age : newest;
            }
        }
    if (best)
        return best;
    for (i = 0; i < np; i++) {
        sy_voice_t *v = &sy_v[i];
        if (v->on && !v->steal && v->age != newest && v->note != lowest && (!best || v->age < best->age))
            best = v;
    }
    if (!best)                                       /* (two voices: the older of them) */
        for (i = 0; i < np; i++)
            if (sy_v[i].on && !sy_v[i].steal && (!best || sy_v[i].age < best->age))
                best = &sy_v[i];
    return best;
}

static void sy_note_on(uint32_t note, uint32_t vel)
{
    uint32_t i, np = sy_poly();
    sy_voice_t *v = 0;
    if (note > 127u)
        return;
    if (!vel) {
        sy_note_off(note);
        return;
    }
    if (sy_patch.p[SP_V_MODE]) {                     /* MONO / LEGATO: voice 0, the stack */
        int held = sy_nstack > 0, legato = sy_patch.p[SP_V_MODE] == 2;
        for (i = 0; i < sy_nstack && sy_stack[i] != note; i++)
            ;
        if (i < sy_nstack) {                         /* (again: to the top) */
            for (; i + 1u < sy_nstack; i++)
                sy_stack[i] = sy_stack[i + 1u];
            sy_nstack--;
        }
        if (sy_nstack == SY_STACK) {
            for (i = 0; i + 1u < SY_STACK; i++)
                sy_stack[i] = sy_stack[i + 1u];
            sy_nstack--;
        }
        sy_stack[sy_nstack++] = (uint8_t)note;
        v = &sy_v[0];
        if (v->on && held && legato && v->gate) {    /* LEGATO: only the pitch moves */
            v->target = (float)note;
            if (!sy_glide_on(1))
                v->pitch = v->target;
            v->note = (uint8_t)note;
            v->key = 1;
            return;
        }
        voice_start(v, note, vel, v->on ? v->pitch : sy_last_pitch, sy_glide_on(held));
        return;
    }
    for (i = 0; i < np; i++)                         /* the same note: that voice again */
        if (sy_v[i].on && sy_v[i].note == note && !sy_v[i].steal) {
            v = &sy_v[i];
            break;
        }
    if (!v)
        for (i = 0; i < np; i++)
            if (!sy_v[i].on) {
                v = &sy_v[i];
                break;
            }
    if (!v) {
        v = sy_steal_pick(np);
        if (!v) {                                    /* every voice already stealing: the latest note waits */
            for (i = 0; i < np; i++)
                if (sy_v[i].steal) {
                    sy_v[i].next_note = (uint8_t)note;
                    sy_v[i].next_vel = (uint8_t)vel;
                    return;
                }
            return;
        }
        v->steal = VOICE_STEAL_BLKS;
        v->next_note = (uint8_t)note;
        v->next_vel = (uint8_t)vel;
        return;
    }
    {                                                /* GL MODE LEGATO: only while another note is held */
        uint32_t k, held = 0;
        for (k = 0; k < np; k++)
            held += sy_v[k].on && sy_v[k].gate && &sy_v[k] != v;
        voice_start(v, note, vel, sy_last_pitch, sy_glide_on(held > 0));
    }
}

static void sy_note_off(uint32_t note)
{
    uint32_t i;
    if (sy_patch.p[SP_V_MODE]) {
        sy_voice_t *v = &sy_v[0];
        for (i = 0; i < sy_nstack && sy_stack[i] != note; i++)
            ;
        if (i == sy_nstack)
            return;
        for (; i + 1u < sy_nstack; i++)
            sy_stack[i] = sy_stack[i + 1u];
        sy_nstack--;
        if (!v->on || v->note != note)
            return;
        if (sy_nstack) {                             /* back to the note still held: legato, no restart */
            v->target = (float)sy_stack[sy_nstack - 1u];
            v->note = sy_stack[sy_nstack - 1u];
            if (!sy_glide_on(1))
                v->pitch = v->target;
            return;
        }
        v->key = 0;
        if (!sy_sustain)
            voice_release(v);
        return;
    }
    for (i = 0; i < SY_NVOICE; i++) {
        sy_voice_t *v = &sy_v[i];
        if (v->steal && v->next_vel && v->next_note == note)
            v->next_vel = 0;                         /* released before it began: nothing to start */
        if (v->on && v->note == note && v->key && !v->steal) {
            v->key = 0;
            if (!sy_sustain)
                voice_release(v);
        }
    }
}

static void sy_sustain_set(int on)
{
    uint32_t i;
    sy_sustain = (uint8_t)(on != 0);
    if (!on)
        for (i = 0; i < SY_NVOICE; i++)
            if (sy_v[i].on && sy_v[i].gate && !sy_v[i].key)
                voice_release(&sy_v[i]);
}

static void sy_all_notes_off(void)
{
    uint32_t i;
    sy_nstack = 0;
    sy_sustain = 0;
    for (i = 0; i < SY_NVOICE; i++) {
        sy_v[i].key = 0;
        sy_v[i].next_vel = 0;
        if (sy_v[i].on)
            voice_release(&sy_v[i]);
    }
}

/* every voice out over a block (panic, a sound change) */
static void sy_all_sound_off(void)
{
    uint32_t i;
    sy_all_notes_off();
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on && !sy_v[i].steal)
            sy_v[i].steal = 1;
}

static void sy_bend(int32_t v14)                     /* 0..16383, 8192 centre */
{
    sy_bend_v = sy_clampf((float)(v14 - 8192) * (1.0f / 8192.0f), -1.0f, 1.0f);
}

/* MIDI CC: the controllers the engine takes itself (the CC map of parameters is the UI's: midi_learn.c) */
static void sy_cc(uint32_t cc, uint32_t val)
{
    switch (cc) {
    case 1: sy_wheel = (uint8_t)val; break;
    case 11: sy_expr = (uint8_t)val; break;
    case 64: sy_sustain_set(val >= 64u); break;
    case 120: sy_all_sound_off(); break;
    case 123: sy_all_notes_off(); break;
    }
}

/* --------------------------------------------------------------------------------- the unison budget --- */
static uint32_t sy_instances(void)
{
    uint32_t i, n = 0, o;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on)
            for (o = 0; o < SY_NOSC; o++)
                if (sy_patch.p[o ? SP_B_ON : SP_A_ON]) {
                    int32_t u = sy_patch.p[o ? SP_B_UNI : SP_A_UNI];
                    n += (uint32_t)(u < (int32_t)sy_v[i].uni_cap ? u : sy_v[i].uni_cap);
                }
    return n;
}

/* over the budget: the oldest voice still playing more than one unison voice gives one up (faded over the block) */
static void sy_budget(void)
{
    uint32_t n = sy_instances(), i;
    int32_t ua = sy_patch.p[SP_A_ON] ? sy_patch.p[SP_A_UNI] : 0, ub = sy_patch.p[SP_B_ON] ? sy_patch.p[SP_B_UNI] : 0;
    int32_t u = ua > ub ? ua : ub;
    while (n > sy_uni_budget && u > 1) {
        sy_voice_t *old = 0;
        for (i = 0; i < SY_NVOICE; i++) {
            sy_voice_t *v = &sy_v[i];
            int32_t eff = (int32_t)v->uni_cap < u ? (int32_t)v->uni_cap : u;
            if (v->on && !v->steal && eff > 1 && (!old || v->age < old->age))
                old = v;
        }
        if (!old)
            break;
        old->uni_cap = (uint8_t)(((int32_t)old->uni_cap < u ? (int32_t)old->uni_cap : u) - 1);
        n = sy_instances();
    }
}

/* ------------------------------------------------------------------------------------ the CPU guard --- */
#define SY_GUARD_FX 4u                       /* budget steps */
static const uint8_t SY_GUARD_BUDGET[SY_GUARD_FX + 1u] = {48, 40, 32, 24, 16};

/* end of a half (audio.c): load_q8 the render's share of the half, 256 = 100 %. Over 70 % (smoothed) the budget
 * steps down, one step a half; over 78 % the effects get cheaper too; two halves over 85 % shed a voice (faded;
 * returns 1). Under 55 % for a second, one step back (the effects first). */
static int sy_cpu(uint32_t load_q8)
{
    int shed = 0;
    sy_g.load_q8 = (sy_g.load_q8 * 3u + load_q8) / 4u;
    if (load_q8 > 218u) {
        if (++sy_g.over >= 2u) {
            sy_voice_t *v = sy_steal_pick(SY_NVOICE);
            if (v && !v->steal) {
                v->steal = VOICE_STEAL_BLKS;
                v->next_vel = 0;
                sy_g.shed++;
            }
            sy_g.over = 0;
            shed = 1;
        }
    } else {
        sy_g.over = 0;
    }
    if (sy_g.load_q8 > 179u) {
        if (sy_g.level < SY_GUARD_FX)
            sy_g.level++;
        if (sy_g.load_q8 > 200u)
            sy_g.fx_lo = 1;
        sy_g.calm = 0;
    } else if (sy_g.load_q8 < 141u && (sy_g.level || sy_g.fx_lo)) {
        if (++sy_g.calm >= 345u) {
            if (sy_g.fx_lo)
                sy_g.fx_lo = 0;
            else
                sy_g.level--;
            sy_g.calm = 0;
        }
    } else {
        sy_g.calm = 0;
    }
    sy_uni_budget = SY_GUARD_BUDGET[sy_g.level];
    fx_lo = sy_g.fx_lo;
    return shed;
}

/* ------------------------------------------------------------------------------------- the block --- */
/* 32 frames of the synth: interleaved Q15 (int32) for audio.c */
static void sy_patch_swap(void);
static void sy_block(int32_t *out)
{
    uint32_t i, k;
    float vs[SRC_VOICE_LAST + 1u], vol0 = sy_vol, vol1, dv;
    sy_voice_t *newest = 0;
    if (sy_next_st)
        sy_patch_swap();
    sp_block();
    mod_map();
    sy_table_fades();
    sy_budget();
    /* global sources, the macros (and slots into them), the global destinations */
    for (k = 0; k < 4u; k++)
        sy_src_g[SRC_MAC1 + k] = sp_n[SP_MAC1 + k];
    sy_src_g[SRC_WHEEL] = (float)sy_wheel * (1.0f / 127.0f);
    sy_src_g[SRC_BEND] = sy_bend_v;
    sy_src_g[SRC_AT] = (float)sy_at * (1.0f / 127.0f);
    sy_src_g[SRC_EXPR] = (float)sy_expr * (1.0f / 127.0f);
    sy_src_g[SRC_FIXED] = 1.0f;
    for (i = 0; i < SY_NVOICE; i++)
        if (sy_v[i].on && (!newest || sy_v[i].age > newest->age))
            newest = &sy_v[i];
    for (k = 0; k <= SRC_VOICE_LAST; k++)
        vs[k] = newest ? newest->src[k] : 0.0f;
    mod_offsets(sy_goff, vs, 1);
    for (k = 0; k < 4u; k++)
        sy_src_g[SRC_MAC1 + k] = mod_get(sy_goff, SP_MAC1 + k);
    mod_offsets(sy_goff, vs, 0);
    for (k = 0; k < 2u; k++) {                       /* the FREE LFOs */
        lfo_par_t lp;
        voice_lfo_par(0, k, &lp);
        sy_glfo_v[k] = lfo_block(&sy_glfo[k], &lp);
    }
    for (i = 0; i < SY_BLK; i++)
        sy_mix_l[i] = sy_mix_r[i] = 0.0f;
    for (i = 0; i < SY_NVOICE; i++) {
        sy_voice_t *v = &sy_v[i];
        if (!v->on)
            continue;
        voice_block(v, sy_mix_l, sy_mix_r);
        if (!v->on && v->next_vel) {                 /* a steal's fade is over: the new note */
            uint32_t n = v->next_note, vel = v->next_vel;
            v->next_vel = 0;
            voice_start(v, n, vel, sy_last_pitch, sy_glide_on(0));
        }
    }
    vol1 = sp_db_gain((int32_t)(sp_lin(SP_V_VOL, mod_get(sy_goff, SP_V_VOL)) + 0.5f));
    dv = (vol1 - vol0) * (1.0f / (float)SY_BLK);
    for (i = 0; i < SY_BLK; i++, vol0 += dv) {
        sy_mix_l[i] *= vol0;
        sy_mix_r[i] *= vol0;
    }
    sy_vol = vol1;
    fx_block(sy_mix_l, sy_mix_r);
    for (i = 0; i < SY_BLK; i++) {                   /* the master: 5 Hz DC blocker, then Q15 */
        float l = sy_mix_l[i], r = sy_mix_r[i];
        float yl = l - sy_dc[0][0] + 0.99929f * sy_dc[0][1], yr = r - sy_dc[1][0] + 0.99929f * sy_dc[1][1];
        sy_dc[0][0] = l;
        sy_dc[0][1] = yl;
        sy_dc[1][0] = r;
        sy_dc[1][1] = yr;
        out[2u * i] = (int32_t)(sy_clampf(yl, -1.0f, 1.0f) * 32767.0f);
        out[2u * i + 1u] = (int32_t)(sy_clampf(yr, -1.0f, 1.0f) * 32767.0f);
    }
    for (k = 0; k < 2u; k++) {
        SY_FLUSH(sy_dc[k][0]);
        SY_FLUSH(sy_dc[k][1]);
    }
}

/* ------------------------------------------------------------------------------------ the sound --- */

/* main loop: changes the sound. The voices fade out over a block, then the audio side takes the new patch whole
 * (sy_patch_swap); its tables are built after (sy_service), each oscillator fading in when its table is ready */
static void sy_patch_load(const sy_patch_t *pt)
{
    while (sy_next_st)
        ;                                    /* (one in flight: the audio side is a block or two from done) */
    sy_next = *pt;
    sy_next_st = 1;
}

/* audio side, at a block's start */
static void sy_patch_swap(void)
{
    uint32_t i, on = 0;
    if (sy_next_st == 1) {
        sy_all_sound_off();
        sy_next_st = 2;
        return;
    }
    for (i = 0; i < SY_NVOICE; i++)
        on |= sy_v[i].on;
    if (sy_next_st != 2 || on)
        return;
    sy_patch = sy_next;
    sp_snap();
    for (i = 0; i < SY_NOSC; i++)            /* a table the new sound does not share: nothing reads it until */
        if ((uint32_t)sy_patch.p[i ? SP_B_TABLE : SP_A_TABLE] != wt_tab[i].id)   /* sy_service rebuilds it */
            wt_tab[i].busy = 1;
    sy_next_st = 0;
}

static void sy_init(void)
{
    uint32_t i;
    wg_init();
    sp_defaults(&sy_patch);
    sp_snap();
    for (i = 0; i < SY_NVOICE; i++)
        voice_init(&sy_v[i], i);
    for (i = 0; i < SY_NOSC; i++) {
        memset(&wt_tab[i], 0, sizeof wt_tab[i]);
        wt_tab[i].f = wt_pool[i * SY_NFRAME];
        wt_tab[i].id = 0xFF;
        wt_g[i][0] = wt_g[i][1] = 0.0f;
    }
    sy_glfo[0].seed = 0xA5A5A5u;
    sy_glfo[1].seed = 0x5A5A5Au;
    memset(mm.key, 0x7F, sizeof mm.key);             /* the matrix maps on the first block */
    sy_vol = sp_db_gain(sy_patch.p[SP_V_VOL]);
    fx_init();
}

/* builds the tables the patch names now (start-up: no audio running yet) */
static void sy_tables_now(void)
{
    uint32_t o;
    for (o = 0; o < SY_NOSC; o++) {
        uint32_t id = (uint32_t)sy_patch.p[o ? SP_B_TABLE : SP_A_TABLE];
        wg_begin(id < WT_NFACT ? id : 0u, wt_pool[o * SY_NFRAME]);
        while (!wg_step())
            ;
        wt_tab[o].n = wg.n;
        wt_tab[o].id = wg.id;
        wt_tab[o].busy = wt_tab[o].quiet = 0;
        wt_g[o][1] = 1.0f;
    }
}
