/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — voces y motores de sonido (vtable común, SPEC_V3 §2, §3, §5). */
#include <string.h>
#include "bm_audio.h"
#include "bm_rom.h"
#include "bm_util.h"

/* ------------------------------------------------------------ voces */
static int voice_prio(const bm_voice *v)
{
    if (!v->active) return 9;
    if (v->stage == ENV_REL || !v->gate) return 4;   /* colas: se roban primero */
    if (v->stage == ENV_SUS) return 3;                /* cuerpo de acordes */
    return 2;                                         /* ataques recientes */
}

bm_voice *bm_voice_get(bm_ctx *c, int track, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    int lo, n;
    if (track == 0) { lo = BM_V_DRUM0; n = BM_V_DRUM_N; }
    else if (track == 2) { lo = BM_V_BASS; n = 1; }
    else { lo = BM_V_POOL0; n = BM_V_POOL_N; }

    bm_voice *best = NULL;
    for (int i = 0; i < n; i++) {
        bm_voice *v = &c->voice[lo + i];
        if (!v->active && !v->pend) return v;
        if (!best) { best = v; continue; }
        int pv = voice_prio(v), pb = voice_prio(best);
        if (v->pend && !best->pend) continue;
        if ((best->pend && !v->pend) || pv > pb || (pv == pb && (v->env < best->env || (v->env == best->env && v->age < best->age))))
            best = v;
    }
    if (!best) return NULL;
    if (best->env < (1 << 20) && !best->pend) {      /* ya inaudible: se toma sin fade */
        best->active = 0;
        return best;
    }
    best->fade = BM_STEAL_FADE;                      /* micro-fade de 2 ms y luego arranca la nueva */
    best->pend = 1; best->pend_track = (uint8_t)track; best->pend_note = note;
    best->pend_vel = vel; best->pend_legato = legato;
    memcpy(best->pend_lk, lk, 4);
    return NULL;
}

void bm_voice_fade(bm_voice *v)
{
    if (v->active && !v->fade) { v->fade = BM_STEAL_FADE; v->pend = 0; }
}

/* Aplica el fade de robo/choke. Devuelve la muestra atenuada y apaga la voz al terminar. */
static int32_t voice_fade_step(bm_ctx *c, bm_voice *v, int32_t s)
{
    if (!v->fade) return s;
    s = s * v->fade / BM_STEAL_FADE;
    if (--v->fade == 0) {
        v->active = 0;
        if (v->pend) {
            v->pend = 0;
            bm_engine *e = &c->eng[v->pend_track];
            e->vt->voice_start(e, v, v->pend_note, v->pend_vel, v->pend_lk, v->pend_legato);
        }
    }
    return s;
}

static uint8_t eff(const bm_voice *v, const bm_track *t, int i)
{
    return v->lk[i] != BM_LOCK_NONE ? v->lk[i] : t->macro[i];
}

static void take_locks(bm_engine *e, uint8_t lk[4])
{
    for (int i = 0; i < 4; i++) { lk[i] = e->lock_next[i]; e->lock_next[i] = BM_LOCK_NONE; }
}

static void common_init(bm_engine *e)
{
    for (int i = 0; i < 4; i++) e->lock_next[i] = BM_LOCK_NONE;
    e->legato_next = 0;
}

static void common_set_macro(bm_engine *e, uint8_t id, uint8_t val)
{
    if (id & BM_MACRO_LOCK) e->lock_next[id & 3] = val;
    else if (id == BM_MACRO_LEGATO) e->legato_next = val;
    else if (id < 4) e->ctx->pat->trk[e->track].macro[id] = val;
}

static int32_t hermite(int32_t xm1, int32_t x0, int32_t x1, int32_t x2, uint32_t frac16)
{
    int64_t f = frac16;
    int64_t c1 = (x1 - xm1) / 2;
    int64_t c2 = xm1 - (5 * (int64_t)x0) / 2 + 2 * (int64_t)x1 - x2 / 2;
    int64_t c3 = (x2 - xm1) / 2 + (3 * ((int64_t)x0 - x1)) / 2;
    int64_t y = ((c3 * f) >> 16) + c2;
    y = ((y * f) >> 16) + c1;
    y = ((y * f) >> 16) + x0;
    return (int32_t)y;
}

/* ------------------------------------------------------------ T1: batería ROMpler */
static int32_t drum_at(const bm_drum_sound *d, int32_t i)
{
    if (i < 0 || (uint32_t)i >= d->len) return 0;
    return bm_ulaw_dec[d->data[i]];
}

static void drum_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)legato;
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[0];
    int snd = note % BM_KIT_SOUNDS;
    const bm_drum_sound *d = &bm_rom_kits[t->source % BM_KITS][snd];
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = 0; v->note = (uint8_t)snd; v->vel = vel;
    v->stage = ENV_DEC; v->gate = 0; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->sound = (uint8_t)snd; v->choke = d->choke;
    const bm_lane *ln = &c->pat->tr[0].lane[snd];
    v->vel = (uint8_t)bm_clampi(vel * ln->level / 100, 1, 127);
    v->step = bm_semi_ratio[bm_clampi(24 + ((eff(v, t, 1) - 64) * 12) / 64 + ((ln->tune - 64) * 12) / 64, 0, 48)];
    v->start = (uint32_t)((uint64_t)d->len * eff(v, t, 3) / 254u);
    v->end = d->len;
    v->pos = 0; v->env = 1 << 30; v->lp = 0;
}

static void drum_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[0];
    int snd = note % BM_KIT_SOUNDS;
    const bm_drum_sound *d = &bm_rom_kits[t->source % BM_KITS][snd];
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    if (d->choke)
        for (int i = 0; i < BM_V_DRUM_N; i++) {
            bm_voice *v = &c->voice[BM_V_DRUM0 + i];
            if (v->active && v->choke == d->choke) bm_voice_fade(v);
        }
    if (d->ducks && c->sc_trig_at < 0) c->sc_trig_at = (int16_t)c->blk_off;
    bm_voice *v = bm_voice_get(c, 0, (uint8_t)snd, vel, lk, 0);
    if (v) drum_voice_start(e, v, (uint8_t)snd, vel, lk, 0);
}

static void drum_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; /* one-shots */ }

static bool drum_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[0];
    bool any = false;
    for (int vi = 0; vi < BM_V_DRUM_N; vi++) {
        bm_voice *v = &c->voice[BM_V_DRUM0 + vi];
        if (!v->active || v->track != 0) continue;
        any = true;
        const bm_drum_sound *d = &bm_rom_kits[t->source % BM_KITS][v->sound];
        int32_t a = bm_onepole_a[eff(v, t, 0)];
        uint32_t coef = bm_decay_coef[eff(v, t, 2) * c->pat->tr[0].lane[v->sound].decay / 127];
        bool herm = t->interp != 0;
        for (uint16_t i = 0; i < frames && v->active; i++) {
            int32_t idx = (int32_t)(v->start + (v->pos >> 16));
            if ((uint32_t)idx >= v->end) { v->active = 0; v->pend = 0; break; }
            int32_t s = herm ? hermite(drum_at(d, idx - 1), drum_at(d, idx), drum_at(d, idx + 1), drum_at(d, idx + 2), v->pos & 0xFFFF)
                             : drum_at(d, idx);
            v->lp += ((s - v->lp) * a) >> 15;
            v->env = (int32_t)(((int64_t)v->env * coef) >> 30);
            int32_t out = (int32_t)(((int64_t)v->lp * (v->env >> 14)) >> 16);
            out = out * v->vel >> 7;
            int fading = v->fade != 0;
            buf[i] += voice_fade_step(c, v, out);
            if (fading && v->fade == 0) break;     /* la voz cambió de dueño o terminó */
            v->pos += v->step;
            if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; }
        }
    }
    return any;
}

static void drum_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_drum = {drum_init, drum_note_on, drum_note_off, common_set_macro, drum_process, drum_voice_start};

/* ------------------------------------------------------------ T2: sampler */
static void slice_bounds(const bm_ctx *c, int i, uint32_t *s, uint32_t *e)
{
    const bm_smp_src *m = &c->smp;
    if (m->nmarks == 0) {
        *s = m->len * (uint32_t)i / BM_SLICES;
        *e = m->len * (uint32_t)(i + 1) / BM_SLICES;
    } else {
        *s = m->marks[i];
        *e = (i + 1 < m->nmarks) ? m->marks[i + 1] : m->len;
    }
}

static int32_t smp_at(const bm_ctx *c, int32_t i)
{
    if (i < 0 || (uint32_t)i >= c->smp.len) return 0;
    return (int32_t)c->smp.data[i] << 8;
}

static void smp_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)legato;
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[1];
    int sl = note % BM_SLICES;
    uint32_t s, en;
    slice_bounds(c, sl, &s, &en);
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = 1; v->note = (uint8_t)sl; v->vel = vel;
    v->stage = ENV_DEC; v->gate = 1; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->sound = (uint8_t)sl;
    const bm_lane *ln = &c->pat->tr[1].lane[sl];
    v->vel = (uint8_t)bm_clampi(vel * ln->level / 100, 1, 127);
    uint32_t ratio = bm_semi_ratio[bm_clampi(24 + ((eff(v, t, 1) - 64) * 12) / 64 + ((ln->tune - 64) * 12) / 64, 0, 48)];
    v->step = (uint32_t)(((uint64_t)c->smp_base_inc * ratio) >> 16);
    v->start = s + (uint32_t)((uint64_t)(en - s) * eff(v, t, 3) / 254u);
    v->end = en; v->pos = 0; v->env = 1 << 30; v->lp = 0;
}

static void smp_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    int sl = note % BM_SLICES;
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    if (c->smp.len == 0 || !c->smp.data || c->smp_rec) return;
    if (c->smp.nmarks && sl >= c->smp.nmarks) return;
    bm_voice *v = bm_voice_get(c, 1, (uint8_t)sl, vel, lk, 0);
    if (v) smp_voice_start(e, v, (uint8_t)sl, vel, lk, 0);
}

static void smp_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; }

static bool smp_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[1];
    bool any = false;
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != 1) continue;
        any = true;
        int32_t a = bm_onepole_a[eff(v, t, 0)];
        uint32_t coef = bm_decay_coef[eff(v, t, 2) * c->pat->tr[1].lane[v->sound].decay / 127];
        bool herm = t->interp != 0;
        if (!c->smp.data) { v->active = 0; continue; }
        for (uint16_t i = 0; i < frames && v->active; i++) {
            int32_t idx = (int32_t)(v->start + (v->pos >> 16));
            if ((uint32_t)idx >= v->end) { v->active = 0; v->pend = 0; break; }
            int32_t s = herm ? hermite(smp_at(c, idx - 1), smp_at(c, idx), smp_at(c, idx + 1), smp_at(c, idx + 2), v->pos & 0xFFFF)
                             : smp_at(c, idx);
            v->lp += ((s - v->lp) * a) >> 15;
            v->env = (int32_t)(((int64_t)v->env * coef) >> 30);
            int32_t out = (int32_t)(((int64_t)v->lp * (v->env >> 14)) >> 16);
            out = out * v->vel >> 7;
            /* micro-fade de salida al final del slice para que no haga click */
            uint32_t left = v->end - (uint32_t)idx;
            if (left < 64) out = out * (int32_t)left / 64;
            int fading = v->fade != 0;
            buf[i] += voice_fade_step(c, v, out);
            if (fading && v->fade == 0) break;     /* la voz cambió de dueño o terminó */
            v->pos += v->step;
            if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; }
        }
    }
    return any;
}

static void smp_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_sampler = {smp_init, smp_note_on, smp_note_off, common_set_macro, smp_process, smp_voice_start};

/* ------------------------------------------------------------ T3-T6: wavetable + SVF */
static const uint8_t MAXH[BM_WAVE_LEVELS] = {127, 64, 32, 16, 8, 4, 2, 1};

static int wave_level(uint32_t inc)
{
    uint32_t maxh = inc ? (uint32_t)(0x80000000u / inc) : 127u;
    for (int l = 0; l < BM_WAVE_LEVELS; l++) if (MAXH[l] <= maxh) return l;
    return BM_WAVE_LEVELS - 1;
}

static void wave_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int n = bm_clampi(note + (int)t->transpose - 24, 0, 127);
    uint32_t inc = bm_note_inc[n];
    memcpy(v->lk, lk, 4);
    if (legato && v->active && v->track == e->track && v->gate && !v->fade) {
        v->inc_target = inc; v->note = note; v->vel = vel;   /* glide sin redisparar */
        return;
    }
    v->active = 1; v->track = e->track; v->note = note; v->vel = vel; v->gate = 1;
    v->stage = ENV_ATK; v->env = 0; v->fenv = 1 << 30; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->inc = v->inc_target = inc;
    v->phase = 0; v->ic1 = v->ic2 = 0;
}

static void wave_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t legato = e->legato_next;
    e->legato_next = 0;
    if (e->track == 2) {
        bm_voice *v = &c->voice[BM_V_BASS];
        if (v->active && v->track == 2 && v->gate && !v->fade) {
            if (legato) { wave_voice_start(e, v, note, vel, lk, 1); return; }
            /* mono: redisparo en la misma voz */
            wave_voice_start(e, v, note, vel, lk, 0);
            return;
        }
    }
    bm_voice *v = bm_voice_get(c, e->track, note, vel, lk, 0);
    if (v) wave_voice_start(e, v, note, vel, lk, 0);
}

static void wave_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend && v->pend_track == e->track && v->pend_note == note) v->pend = 0;
        if (v->active && v->track == e->track && v->note == note && v->gate) { v->gate = 0; v->stage = ENV_REL; }
    }
}

static bool wave_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    bool any = false;
    int32_t sus = (int32_t)((int64_t)t->sustain * (1 << 30) / 127);
    uint32_t c_rel = bm_decay_coef[t->release];
    uint32_t a_inc = bm_attack_inc[t->attack];
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        /* glide (por bloque) */
        if (v->inc != v->inc_target) {
            uint32_t gs = (uint32_t)t->glide * (BM_FS / 254u);
            int64_t gk = ((int64_t)65536 * frames) / (gs + frames);
            int64_t d = (int64_t)v->inc_target - (int64_t)v->inc;
            int64_t step = (d * gk) >> 16;
            if (step == 0) v->inc = v->inc_target; else v->inc = (uint32_t)((int64_t)v->inc + step);
        }
        int lvl = wave_level(v->inc);
        int p = eff(v, t, 3) * 3 * 256 / 127;
        int w0 = (t->source + (p >> 8)) % BM_WAVES, w1 = (w0 + 1) % BM_WAVES, mf = p & 255;
        const int16_t *T0 = bm_rom_waves[w0][lvl], *T1 = bm_rom_waves[w1][lvl];
        uint32_t c_dec = bm_decay_coef[eff(v, t, 2)];
        int ci = eff(v, t, 0) + (int)(((int32_t)t->env_amt * (v->fenv >> 22)) / 256);
        ci = bm_clampi(ci, 0, 127);
        int64_t g = bm_svf_g[ci];
        int64_t k = 131072 - ((int64_t)eff(v, t, 1) * 127795) / 127;
        int64_t den = 65536 + ((g * (g + k)) >> 16);
        int64_t a1 = ((int64_t)1 << 32) / den;
        int64_t a2 = (g * a1) >> 16;
        int64_t a3 = (g * a2) >> 16;
        for (uint16_t i = 0; i < frames && v->active; i++) {
            uint32_t ix = v->phase >> 24, fr = (v->phase >> 8) & 0xFFFF;
            int32_t s0 = T0[ix] + (int32_t)(((int64_t)(T0[(ix + 1) & 255] - T0[ix]) * fr) >> 16);
            if (mf) {
                int32_t s1 = T1[ix] + (int32_t)(((int64_t)(T1[(ix + 1) & 255] - T1[ix]) * fr) >> 16);
                s0 += ((s1 - s0) * mf) >> 8;
            }
            v->phase += v->inc;
            int64_t x = (int64_t)s0 << 2;
            int64_t v3 = x - v->ic2;
            int64_t v1 = (a1 * v->ic1 + a2 * v3) >> 16;
            int64_t v2 = v->ic2 + ((a2 * v->ic1 + a3 * v3) >> 16);
            v->ic1 = (int32_t)(2 * v1 - v->ic1);
            v->ic2 = (int32_t)(2 * v2 - v->ic2);
            int32_t y = (int32_t)(v2 >> 2);
            switch (v->stage) {
            case ENV_ATK:
                v->env += (int32_t)a_inc;
                if (v->env >= (1 << 30)) { v->env = 1 << 30; v->stage = ENV_DEC; }
                break;
            case ENV_DEC:
                v->env = sus + (int32_t)(((int64_t)(v->env - sus) * c_dec) >> 30);
                if (v->env - sus < (1 << 16)) { v->env = sus; v->stage = ENV_SUS; }
                break;
            case ENV_SUS:
                v->env = sus;
                break;
            case ENV_REL:
                v->env = (int32_t)(((int64_t)v->env * c_rel) >> 30);
                if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; }
                break;
            default: break;
            }
            v->fenv = (int32_t)(((int64_t)v->fenv * c_dec) >> 30);
            int32_t out = (int32_t)(((int64_t)y * (v->env >> 14)) >> 16);
            out = out * v->vel >> 7;
            int fading = v->fade != 0;
            buf[i] += voice_fade_step(c, v, out);
            if (fading && v->fade == 0) break;
        }
        if (v->active && v->track == e->track && v->stage == ENV_SUS && sus < (1 << 14)) v->active = 0;
    }
    return any;
}

static void wave_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_wave = {wave_init, wave_note_on, wave_note_off, common_set_macro, wave_process, wave_voice_start};
