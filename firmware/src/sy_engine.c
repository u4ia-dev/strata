/* SPDX-License-Identifier: GPL-3.0-only */
/* STRATA engine (docs/ARCHITECTURE.md §3, §13): notes to voices, the block, the CPU guard, the tables.
 * Audio side (the ALNK0 interrupt): sy_note_on / sy_note_off / sy_cc / sy_bend / .. at a block's start (the panel's
 * keys and MIDI alike), then sy_block renders 32 frames: the parameters' smoothing, the matrix's global part, the
 * FREE LFOs, every voice, the volume, the effects (sy_fx.c), the master (DC block, clip) into Q15 for audio.c, and
 * HOME's scope its samples when the screen asks.
 * Main loop: sy_service builds a wavetable when an oscillator's TABLE changes (the oscillator fades out over a
 * block first and back in after), sy_patch_load changes the sound (the voices fade out first).
 * Voices: POLY takes a free voice, else steals (a voice in release, the quietest; then the oldest held note that is
 * neither the newest nor the lowest), the stolen voice fading out over 3 ms before it plays the new note. MONO and
 * LEGATO play one voice, the last note held (a stack); LEGATO does not restart the envelopes while notes are held.
 * Glide: GLIDE's time, always or (GL MODE LEGATO) only between held notes. The unison budget: the sounding
 * oscillator instances (voices x oscillators x unison) stay under sy_uni_budget; over it, the oldest voices lose a
 * unison voice a block (faded) before any voice is stolen. The CPU guard (sy_cpu) shrinks the budget, then the
 * effects' cost, then sheds voices, and undoes it in reverse when the load falls. V0.87.1: the guard and the
 * admission go by the half's whole time, the interrupts nested in it too (what the deadline is), to a higher mark
 * (85 %: the V0.87 test's voices stopped at 60-69 % of the render alone, the guard stepping at 70 % of it), and a new
 * note the time does not hold takes unison voices from the others before it steals. */
#define SY_STACK 16u
#define SY_UNI_BUDGET 48u
static void sy_note_off(uint32_t note);
static sy_patch_t sy_next;                   /* a sound to change to (main loop -> audio side) */
static volatile uint8_t sy_next_st;          /* 1: asked for; 2: the voices are fading out */

static float sy_mix_l[SY_BLK], sy_mix_r[SY_BLK];
static float sy_vol;                         /* the volume at the end of the last block */
static float sy_dc[4][2];                    /* DC blockers, before and after the effects: last input, last output */
static uint8_t sy_stack[SY_STACK], sy_nstack;   /* MONO / LEGATO: notes held, the last on top */
static uint8_t sy_sustain;
static uint32_t sy_uni_budget = SY_UNI_BUDGET;
static uint8_t sy_wheel, sy_at, sy_expr = 127;
static lfo_t sy_glfo[2];
static volatile uint8_t sy_lfo_sync;         /* the transport started (PLAY): the FREE LFOs from their start, next block */
static struct {                              /* the CPU guard */
    uint32_t load_q8;                        /* the last half's render, smoothed (256 = 100 %: HOME, BENCH) */
    uint32_t tot_q8;                         /* V0.87.1: .. its whole time, the interrupts nested in it too (the guard's) */
    uint8_t level;                           /* budget steps taken, 0..SY_GUARD_FX */
    uint8_t fx_lo;                           /* effects at low cost (OTT upward off, reverb half its lines) */
    uint8_t over;                            /* halves in a row over SY_G_SHED */
    uint16_t calm;                           /* halves in a row under SY_G_CALM */
    uint32_t shed;                           /* voices shed */
} sy_g;
/* the guard's marks (V0.87.1), 256 the half's whole time: over SY_G_STEP (smoothed) the unison budget a step down a
 * half, over SY_G_FXLO the effects at low cost too, a half over SY_G_SHED twice in a row a voice shed; under SY_G_CALM
 * for a second a step back. V0.87's were 70, 78 and 85 % of the render alone and 55 % back: the nested interrupts
 * (TIMER5's keys and LEDs, the USB audio's packets) not counted, so a mark on the render was lower on the deadline by
 * however long they took */
#define SY_G_STEP 218u                       /* 85 % */
#define SY_G_FXLO 225u                       /* 88 % */
#define SY_G_SHED 236u                       /* 92 % */
#define SY_G_CALM 192u                       /* 75 % (a budget step is ~8 % of a core, tools/costprobe: no see-saw) */
#define SY_GUARD_FX 4u                       /* budget steps */
static const uint8_t SY_GUARD_BUDGET[SY_GUARD_FX + 1u] = {48, 40, 32, 24, 16};

/* admission (V0.66): a chord of eight VOWEL PADs took one half 3.85 ms of its 2.90 (BENCH on V0.65: 3 halves late)
 * before the guard, which only reads a half once it is over, shed five voices. So the engine times its blocks (the
 * platform's clock, SY_TICKS: none on the host and in the emulator, where nothing is refused) and a new voice takes a
 * free one only while the time it is measured to need fits: the voices' own (a voice's, measured, times one more than
 * sound now) and the rest of a block's (V0.87.1: the half's whole time less its voices', a block's share: the effects,
 * the drums, the events, the interrupts nested) under SY_ADM_PCT of a block. V0.87.1: a voice's time is its own
 * (envelopes, filter, amp, the matrix) and its unison voices': vt = c1 (K + u), u the voices' unison voices on average
 * (sy_instances), K SY_ADM_K. When the budget binds, a new voice adds its own and takes its unison voices from the
 * others (sy_budget, the oldest first); so a note that does not fit asks first whether it would with the budget a step
 * (or more) down, and if so the budget steps and the voice plays: the unison gives way before a note steals. Else it
 * steals, as POLY does with every voice taken. A sound's load forgets the voice's cost: its first two voices measure
 * it (the app starts at most two notes a block; V0.87.1: the first measure taken whole, not halved into the 0) */
#ifndef SY_TICKS
#define SY_TICKS() 0u                        /* the platform's clock (strata.c: TIMER4) */
#define SY_TICKS_BLK 0u                      /* .. its ticks in a block's time (0: no admission) */
#endif
#define SY_ADM_PCT 84u                       /* (V0.87.1; 80 % of the block's own time before) */
#define SY_ADM_K 5.0f                        /* a voice's own work in unison voices' (tools/costprobe, SUPERSAW: ~183 and
                                              * ~34 instructions a sample; a sound without its filter ~3, so 5 errs on
                                              * the side of trading less) */
static uint32_t sy_loads;                    /* sounds taken (sy_patch_swap: the event log) */
static struct {
    uint32_t vt, bt;                         /* a voice's time a block (smoothed), the rest of a block's */
    uint32_t refused;                        /* notes that stole for want of time (BENCH, the event log) */
    uint32_t traded;                         /* V0.87.1: notes that took a voice for unison given up (the budget) */
    uint32_t tvh;                            /* .. the voices' time this half so far */
} sy_adm;
static uint32_t sy_instances(void);
static uint32_t sy_uni_new(uint32_t *nosc)   /* a new voice's unison voices (the budget's cap aside), its oscillators */
{
    uint32_t o, n = 0;
    *nosc = 0;
    for (o = 0; o < SY_NOSC; o++)
        if (sy_patch.p[o ? SP_B_ON : SP_A_ON]) {
            int32_t u = sy_patch.p[o ? SP_B_UNI : SP_A_UNI];
            n += (uint32_t)(u < 1 ? 1 : u > (int32_t)SY_NUNI ? (int32_t)SY_NUNI : u);
            (*nosc)++;
        }
    return n;
}
static SY_NOINLINE int sy_admit(void)       /* (out of line: a note-on's, its float work kept in one place) */
{
    uint32_t i, n = 0, ni, un, fl, lv, cap = SY_TICKS_BLK / 100u * SY_ADM_PCT;
    float c1;
    if (!SY_TICKS_BLK || !sy_adm.vt)
        return 1;
    for (i = 0; i < SY_NVOICE; i++)
        n += sy_v[i].on != 0;
    if (!n)
        return 1;
    ni = sy_instances();
    un = sy_uni_new(&fl);
    c1 = (float)sy_adm.vt / ((float)ni / (float)n + SY_ADM_K);
    for (lv = sy_g.level;; lv++) {
        uint32_t nj = ni + un, b = SY_GUARD_BUDGET[lv];
        nj = nj > b ? b : nj;                    /* (what the budget leaves; each voice its oscillators at least) */
        nj = nj < (n + 1u) * fl ? (n + 1u) * fl : nj;
        if ((float)sy_adm.bt + c1 * ((float)(n + 1u) * SY_ADM_K + (float)nj) <= (float)cap) {
            if (lv != sy_g.level) {
                sy_g.level = (uint8_t)lv;
                sy_uni_budget = b;
                sy_g.calm = 0;
                sy_adm.traded++;
            }
            return 1;
        }
        if (lv >= SY_GUARD_FX || nj <= (n + 1u) * fl)
            return 0;                            /* (the budget at its floor, or no unison left to give) */
    }
}

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

/* audio side, each block: a busy table's oscillator fades to 0 over 4 blocks, then lets go (quiet); a ready one
 * fades back in */
static void sy_table_fades(void)
{
    uint32_t o;
    for (o = 0; o < SY_NOSC; o++) {
        float g = wt_g[o][1];
        wt_g[o][0] = g;
        if (wt_tab[o].busy) {
            if (g <= 0.0f)
                wt_tab[o].quiet = 1;
            wt_g[o][1] = sy_maxf(0.0f, g - VOICE_FADE);
        } else {
            wt_g[o][1] = wt_tab[o].n ? sy_minf(1.0f, g + VOICE_FADE) : 0.0f;
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
            v->trk = (uint8_t)(sy_ntrk % (SY_NOTRK + 1u));
            return;
        }
        voice_start(v, note, vel, v->on ? v->pitch : sy_last_pitch, sy_glide_on(held));
        return;
    }
    for (i = 0; i < np; i++)                         /* the same note (of the same track): that voice again */
        if (sy_v[i].on && sy_v[i].note == note && sy_v[i].trk == sy_ntrk && !sy_v[i].steal) {
            v = &sy_v[i];
            break;
        }
    if (!v)
        for (i = 0; i < np; i++)
            if (!sy_v[i].on) {
                if (sy_admit())
                    v = &sy_v[i];
                else
                    sy_adm.refused++;                /* (a free voice, but no time for it: steal) */
                break;
            }
    if (!v) {
        v = sy_steal_pick(np);
        if (!v) {                                    /* every voice already stealing: the latest note waits */
            for (i = 0; i < np; i++)
                if (sy_v[i].steal) {
                    sy_v[i].next_note = (uint8_t)note;
                    sy_v[i].next_vel = (uint8_t)vel;
                    sy_v[i].next_trk = sy_ntrk;
                    return;
                }
            return;
        }
        v->steal = VOICE_STEAL_BLKS;
        v->next_note = (uint8_t)note;
        v->next_vel = (uint8_t)vel;
        v->next_trk = sy_ntrk;
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
        if (v->steal && v->next_vel && v->next_note == note && v->next_trk == sy_ntrk)
            v->next_vel = 0;                         /* released before it began: nothing to start */
        if (v->on && v->note == note && v->trk == sy_ntrk && v->key && !v->steal) {
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
            sy_v[i].steal = VOICE_STEAL_BLKS;
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

/* the guard's three steps (V0.87.1: one place each, web/emu/strata_guard.c forces them): a voice shed (faded over
 * VOICE_STEAL_BLKS; 1 if one was), the unison budget a step down, the effects at low cost (or back) */
static int sy_g_shed(void)
{
    sy_voice_t *v = sy_steal_pick(SY_NVOICE);
    if (!v || v->steal)
        return 0;
    v->steal = VOICE_STEAL_BLKS;
    v->next_vel = 0;
    sy_g.shed++;
    return 1;
}
static void sy_g_step(void)
{
    if (sy_g.level < SY_GUARD_FX)
        sy_g.level++;
    sy_uni_budget = SY_GUARD_BUDGET[sy_g.level];
}
static void sy_g_fx(int lo)
{
    sy_g.fx_lo = (uint8_t)(lo != 0);
    fx_lo = sy_g.fx_lo;
}

/* end of a half (sy_app.c): load_q8 the render's share of the half (HOME's), tot_q8 its whole time's, the interrupts
 * nested in it too (256 = 100 %). V0.87.1: the guard on the whole, smoothed: over SY_G_STEP the budget steps down, one
 * step a half; over SY_G_FXLO the effects get cheaper too; two halves over SY_G_SHED shed a voice (faded; returns 1).
 * Under SY_G_CALM for a second, one step back (the effects first) */
static int sy_cpu(uint32_t load_q8, uint32_t tot_q8)
{
    int shed = 0;
    sy_g.load_q8 = (sy_g.load_q8 * 3u + load_q8) / 4u;
    sy_g.tot_q8 = (sy_g.tot_q8 * 3u + tot_q8) / 4u;
    if (tot_q8 > SY_G_SHED) {
        if (++sy_g.over >= 2u) {
            shed = sy_g_shed();
            sy_g.over = 0;
        }
    } else {
        sy_g.over = 0;
    }
    if (sy_g.tot_q8 > SY_G_STEP) {
        sy_g_step();
        if (sy_g.tot_q8 > SY_G_FXLO)
            sy_g_fx(1);
        sy_g.calm = 0;
    } else if (sy_g.tot_q8 < SY_G_CALM && (sy_g.level || sy_g.fx_lo)) {
        if (++sy_g.calm >= 345u) {
            if (sy_g.fx_lo)
                sy_g_fx(0);
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

/* V0.87.1: the admission's rest of a block from the half's whole time (TIMER4 ticks): less its voices', a block's
 * share, smoothed (it was the rest of sy_block alone: the events, the chords, the looper's click, the USB tap and the
 * nested interrupts left out) */
static void sy_adm_half(uint32_t tot_ticks)
{
    uint32_t r = tot_ticks > sy_adm.tvh ? tot_ticks - sy_adm.tvh : 0u;
    if (SY_TICKS_BLK)
        sy_adm.bt = (sy_adm.bt * 3u + r / 4u) / 4u;  /* (a half four blocks: sy_app.c's SY_HALF) */
    sy_adm.tvh = 0;
    sy_v_new = 0;
}

/* end of a half, again: tot_q8 the render and the interrupts nested in it together (256 = the half). Near the deadline
 * the guard steps at once, not a few smoothed halves later: over 90 % a budget step (and over 94 % the effects at low
 * cost), over 96 % a voice shed now (BENCH on V0.5: 95 % at its peak, 5 halves late, 1 voice shed). V0.87.1: not in a
 * half that started voices from silence: a start's first block costs 1.4 to 2.4 times a steady one (two voices, the
 * starter sounds, on the host), a peak that is gone a half later; a load that stays sheds by sy_cpu's two halves.
 * The way back is sy_cpu's, unchanged. Returns 1 when a voice was shed */
static int sy_cpu_now(uint32_t tot_q8)
{
    int shed = 0;
    if (tot_q8 <= 230u)
        return 0;
    sy_g_step();
    if (tot_q8 > 240u)
        sy_g_fx(1);
    sy_g.calm = 0;
    if (tot_q8 > 246u && !sy_v_new)
        shed = sy_g_shed();
    return shed;
}

/* ------------------------------------------------------------------------------------- the scope --- */
/* HOME's scope (sy_ui_draw.c): the main loop arms it with a step (samples a point); the audio side waits for the
 * output (left plus right) to rise through 0 from under -1 % (or SY_SCOPE_WAIT samples: free running), then takes
 * every step-th sample of each side, SY_SCOPE_N of them, and hands them back (ready). The main loop reads them only
 * when ready, the audio side writes them only when armed: no sample is read while it is written */
#define SY_SCOPE_N 212u
#define SY_SCOPE_WAIT 8192u
static volatile uint16_t sy_out_pk;          /* the last block's output peak, Q15 (every 4th frame) */
#define SY_HUSH_BLKS 8u                      /* the hush's fade, and its silent blocks before it counts: two I2S */
                                             /* halves (sy_app.c), the whole ring the DMA loops with the IRQs off */
static volatile uint8_t sy_hush_req, sy_hushed;   /* the audio out for flash work: asked (main loop); out (audio) */
static float sy_hush_g = 1.0f;               /* .. the output's gain at the end of the last block */
static uint8_t sy_hush_n;                    /* .. silent blocks since it reached 0 */
static struct {
    volatile uint8_t st;                     /* 0 idle, 1 armed (the audio side's), 2 ready (the main loop's) */
    uint8_t step, low;                       /* samples a point (1..16); the sum has been under -1 % */
    uint16_t n, k, wait;                     /* points taken; samples to the next; samples waited */
    uint32_t frames;                         /* captures handed back (the tests) */
    int16_t v[2][SY_SCOPE_N];                /* left, right (Q15) */
} sy_scope;

static void sy_scope_arm(uint32_t step)      /* main loop */
{
    sy_scope.step = (uint8_t)sy_clampi((int32_t)step, 1, 16);
    sy_scope.n = sy_scope.k = sy_scope.wait = 0;
    sy_scope.low = 0;
    SY_CBAR();                               /* (all of it before the audio side may look) */
    sy_scope.st = 1;
}

static void sy_scope_block(const int32_t *out)   /* audio side, armed: a block of the output */
{
    uint32_t i;
    for (i = 0; i < SY_BLK; i++) {
        int32_t l = out[2u * i], r = out[2u * i + 1u];
        if (sy_scope.wait < SY_SCOPE_WAIT) {     /* the trigger: up through 0 */
            sy_scope.wait++;
            if (l + r < -655)
                sy_scope.low = 1;
            else if (sy_scope.low && l + r >= 0)
                sy_scope.wait = SY_SCOPE_WAIT;
            if (sy_scope.wait < SY_SCOPE_WAIT)
                continue;
        }
        if (sy_scope.k) {
            sy_scope.k--;
            continue;
        }
        sy_scope.v[0][sy_scope.n] = (int16_t)l;
        sy_scope.v[1][sy_scope.n] = (int16_t)r;
        sy_scope.k = (uint16_t)(sy_scope.step - 1u);
        if (++sy_scope.n == SY_SCOPE_N) {
            sy_scope.frames++;
            sy_scope.st = 2;
            return;
        }
    }
}

/* ------------------------------------------------------------------------------------- the block --- */
/* the 5 Hz DC blockers (y = x - x1 + R y1, run inside sy_block's loops); a state, flushed, back after a block */
#define SY_DC_R 0.99929f
static inline void sy_dc_store(float *st, float x1, float y1)
{
    SY_FLUSH(x1);
    SY_FLUSH(y1);
    st[0] = x1;
    st[1] = y1;
}

/* 32 frames of the synth: interleaved Q15 (int32) for audio.c */
static void sy_patch_swap(void);
static void sy_block(int32_t *out)
{
    uint32_t i, k, tv, nv = 0;
    float vs[SRC_VOICE_LAST + 1u], vol0 = sy_vol, vol1, dv;
    sy_voice_t *newest = 0;
    if (sy_next_st)
        sy_patch_swap();
    sp_block();
    lfo_draw_sync();
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
    if (sy_lfo_sync) {                               /* (PLAY: on the beat from here) */
        sy_glfo[0].ph = sy_glfo[1].ph = 0;
        fx_lfo_reset();                              /* (the flangers', phasers', tremolos' too) */
        sy_lfo_sync = 0;
    }
    for (k = 0; k < 2u; k++) {                       /* the FREE LFOs */
        lfo_par_t lp;
        voice_lfo_par(0, k, &lp);
        sy_glfo_v[k] = lfo_block(&sy_glfo[k], &lp);
    }
    for (i = 0; i < SY_BLK; i++)
        sy_mix_l[i] = sy_mix_r[i] = 0.0f;
    tv = SY_TICKS();
    for (i = 0; i < SY_NVOICE; i++) {
        sy_voice_t *v = &sy_v[i];
        if (!v->on)
            continue;
        nv++;
        voice_block(v, sy_mix_l, sy_mix_r);
        if (!v->on && v->next_vel) {                 /* a steal's fade is over: the new note */
            uint32_t n = v->next_note, vel = v->next_vel, was = sy_ntrk;
            v->next_vel = 0;
            sy_ntrk = v->next_trk;
            voice_start(v, n, vel, sy_last_pitch, sy_glide_on(0));
            sy_ntrk = (uint8_t)was;
        }
    }
    tv = SY_TICKS() - tv;
    vol1 = sp_db_gain((int32_t)(sp_lin(SP_V_VOL, mod_get(sy_goff, SP_V_VOL)) + 0.5f));
    dv = (vol1 - vol0) * (1.0f / (float)SY_BLK);
    {                                                /* the volume, ramped, and no DC into the effects (warps, */
        float a0 = sy_dc[0][0], b0 = sy_dc[0][1], a1 = sy_dc[1][0], b1 = sy_dc[1][1];   /* sync, FM make some) */
        for (i = 0; i < SY_BLK; i++, vol0 += dv) {
            float xl = sy_mix_l[i] * vol0, xr = sy_mix_r[i] * vol0;
            float yl = xl - a0 + SY_DC_R * b0, yr = xr - a1 + SY_DC_R * b1;
            a0 = xl, b0 = yl, a1 = xr, b1 = yr;
            sy_mix_l[i] = yl;
            sy_mix_r[i] = yr;
        }
        sy_dc_store(sy_dc[0], a0, b0);
        sy_dc_store(sy_dc[1], a1, b1);
    }
    sy_vol = vol1;
    fx_block(sy_mix_l, sy_mix_r);
    sy_drum_block(sy_mix_l, sy_mix_r);               /* (V0.85: the duck on the synth, the drums added: sy_drum.c) */
    {                                                /* the master: no DC out of the effects (TUBE makes some), */
        float a0 = sy_dc[2][0], b0 = sy_dc[2][1], a1 = sy_dc[3][0], b1 = sy_dc[3][1];   /* to Q15 */
        for (i = 0; i < SY_BLK; i++) {
            float yl = sy_mix_l[i] - a0 + SY_DC_R * b0, yr = sy_mix_r[i] - a1 + SY_DC_R * b1;
            a0 = sy_mix_l[i], b0 = yl, a1 = sy_mix_r[i], b1 = yr;
            out[2u * i] = (int32_t)(sy_clampf(yl, -1.0f, 1.0f) * 32767.0f);
            out[2u * i + 1u] = (int32_t)(sy_clampf(yr, -1.0f, 1.0f) * 32767.0f);
        }
        sy_dc_store(sy_dc[2], a0, b0);
        sy_dc_store(sy_dc[3], a1, b1);
    }
    {                                                /* hushed for flash work (sy_app.c): out over 8 blocks (5.8 ms), */
        const float st = 1.0f / (float)SY_HUSH_BLKS;   /* held, back over 8 (as it plays: nothing to add otherwise). */
        float g0 = sy_hush_g, g1 = sy_hush_req ? sy_maxf(0.0f, g0 - st) : sy_minf(1.0f, g0 + st);   /* Hushed once */
        if (g0 < 1.0f || g1 < 1.0f) {               /* 8 blocks more are silent: an erase then loops only silence */
            float g = g0, dg = (g1 - g0) * (1.0f / (float)SY_BLK);
            for (i = 0; i < SY_BLK; i++, g += dg) {
                out[2u * i] = (int32_t)((float)out[2u * i] * g);
                out[2u * i + 1u] = (int32_t)((float)out[2u * i + 1u] * g);
            }
        }
        sy_hush_g = g1;
        sy_hush_n = (uint8_t)(g0 > 0.0f || g1 > 0.0f ? 0u : sy_hush_n < SY_HUSH_BLKS ? sy_hush_n + 1u : SY_HUSH_BLKS);
        sy_hushed = (uint8_t)(sy_hush_req && sy_hush_n >= SY_HUSH_BLKS);
    }
    if (sy_scope.st == 1u)
        sy_scope_block(out);
    {                                                /* the block's level, every 4th frame (the screen: sounding) */
        int32_t pk = 0;
        for (i = 0; i < 2u * SY_BLK; i += 8u) {
            int32_t a = out[i] < 0 ? -out[i] : out[i], c = out[i + 1u] < 0 ? -out[i + 1u] : out[i + 1u];
            pk = a > pk ? a : pk;
            pk = c > pk ? c : pk;
        }
        sy_out_pk = (uint16_t)pk;
    }
    if (SY_TICKS_BLK) {                              /* (admission: a voice's time, quick to follow; the rest */
        if (nv)                                      /* from the half's whole, sy_adm_half) */
            sy_adm.vt = sy_adm.vt ? (sy_adm.vt + tv / nv) / 2u : tv / nv;
        sy_adm.tvh += tv;
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
    sy_adm.vt = 0;                           /* (a voice's cost: the new sound's, measured anew) */
    sy_loads++;
    fx.settle = 1;                           /* (its modules join switched in: the voices are out, as at the start) */
    lfo_dr_seq = 1u;                         /* (its drawn shapes: taken at this block's start) */
    for (i = 0; i < SY_NOSC; i++)            /* a table the new sound does not share: nothing reads it until */
        if ((uint32_t)sy_patch.p[i ? SP_B_TABLE : SP_A_TABLE] != wt_tab[i].id)   /* sy_service rebuilds it */
            wt_tab[i].busy = 1;
    sy_next_st = 0;
}

/* main loop: GLOBAL's tempo and time signature. BPM counts beats of TS-NOTE (1 a whole note .. 16 a sixteenth); a
 * bar is TS-BEAT beats. The audio side reads quarter notes a minute and a bar in quarter notes (a float store each:
 * the next block takes them) */
static void sy_set_tempo(uint32_t bpm, uint32_t beat, uint32_t note)
{
    sy_tempo = (float)bpm * 4.0f / (float)note;
    sy_bar_q = (float)beat * 4.0f / (float)note;
    sy_beat_q = 4.0f / (float)note;
}

static void sy_init(void)
{
    uint32_t i;
    wg_init();
    sp_defaults(&sy_patch);
    sp_snap();
    sy_drum_init();                                  /* (V0.85: the kit's defaults, the drums silent) */
    for (i = 0; i < SY_NVOICE; i++)
        voice_init(&sy_v[i], i);
    sy_last_pitch = 60.0f;                           /* (everything from scratch: the same events give the */
    sy_age = 0;                                      /* same sound, run after run) */
    sy_rseed = 0x2545F491u;
    sy_alt = 0;
    sy_bend_v = 0.0f;
    sy_wheel = sy_at = 0;
    sy_expr = 127;
    sy_sustain = 0;
    sy_nstack = 0;
    sy_uni_budget = SY_UNI_BUDGET;
    memset(&sy_g, 0, sizeof sy_g);
    memset(&sy_adm, 0, sizeof sy_adm);
    sy_v_new = 0;
    memset(sy_dc, 0, sizeof sy_dc);
    memset(sy_glfo, 0, sizeof sy_glfo);
    sy_lfo_sync = 0;
    memset(&sy_scope, 0, sizeof sy_scope);
    sy_hush_req = sy_hushed = sy_hush_n = 0;
    sy_hush_g = 1.0f;
    sy_glfo_v[0] = sy_glfo_v[1] = 0.0f;
    lfo_dr_seq = 1u;                                 /* (the drawn shapes: taken at the first block) */
    sy_next_st = 0;
    wt_build_o = 0xFF;
    fx_lo = 0;
    sy_tempo = 120.0f;
    sy_bar_q = 4.0f;
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
