/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — voces y motores de sonido (vtable común, SPEC_V3 §2, §3, §5). */
#include <string.h>
#include "bm_audio.h"
#include "bm_rom.h"
#include "bm_util.h"
#include "bm_fm.h"
#include "bm_drums.h"
#include "bm_analog.h"
#include "bm_params.h"
#include <math.h>

/* Tiempos y constantes que se afinaron con los motores a 22 059 Hz: AT22 pasa n muestras de entonces
 * a muestras de BM_FS, y los coeficientes de un polo (Q15) se recalcularon para el mismo corte. */
#define AT22(n) ((uint32_t)(n) * BM_FS / 22059u)
#define ACID_ATK ((int32_t)(((uint64_t)1 << 26) * 22059u / BM_FS))     /* ataque de ~0.7 ms */

/* interpolación lineal en Q15 (cabe en 32 bits con muestras de 16) */
static inline int32_t lerp15(int32_t a, int32_t b, uint32_t frac16) { return a + (((b - a) * (int32_t)(frac16 >> 1)) >> 15); }

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


/* ------------------------------------------------------------ osciladores con poca alias (polyBLEP) */
static int32_t blep(uint32_t ph, uint32_t inc)            /* corrección Q15 cerca del salto */
{
    if (!inc) return 0;
    if (ph < inc) {
        int32_t x = (int32_t)(((uint64_t)ph << 15) / inc);
        return x + x - ((x * x) >> 15) - 32768;
    }
    if (ph > 0xFFFFFFFFu - inc) {
        int32_t x = -(int32_t)(((uint64_t)(0xFFFFFFFFu - ph) << 15) / inc);
        return ((x * x) >> 15) + x + x + 32768;
    }
    return 0;
}

static int32_t osc_saw(uint32_t ph, uint32_t inc) { return (int32_t)(ph >> 16) - 32768 - blep(ph, inc); }
static int32_t osc_pulse(uint32_t ph, uint32_t inc, uint32_t width) { return (osc_saw(ph, inc) - osc_saw(ph + width, inc)) / 2; }

static int32_t soft(int32_t x, int32_t knee)                /* saturación suave, sin divisiones */
{
    if (x > knee) x = knee + (x - knee) / 4;
    else if (x < -knee) x = -knee + (x + knee) / 4;
    if (x > 46000) x = 46000;
    if (x < -46000) x = -46000;
    return x;
}

/* ------------------------------------------------------------ ACID: monofónico estilo 303
 * Sierra o cuadrada, filtro escalera de 4 polos con resonancia y saturación, envolvente de filtro
 * corta, acento (abre más el filtro y suena más fuerte) y slide con LIGA. */
const char *const bm_acid_names[2] = {"SIERRA", "CUADRADA"};

static void acid_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int n = bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
    uint32_t inc = bm_note_inc[n];
    memcpy(v->lk, lk, 4);
    if (legato && v->active && v->track == e->track && v->gate && !v->fade) {
        v->inc_target = inc; v->note = note; v->vel = vel;   /* slide: sin redisparar las envolventes */
        return;
    }
    v->active = 1; v->track = e->track; v->note = note; v->vel = vel; v->gate = 1;
    v->stage = ENV_ATK; v->env = 0; v->fenv = 1 << 30; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->inc = v->inc_target = inc;
    if (!v->phase) v->phase = 0x40000000u;
    v->z[0] = v->z[1] = v->z[2] = v->z[3] = 0;
}

static void acid_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t legato = e->legato_next;
    e->legato_next = 0;
    for (int i = 0; i < BM_VOICES; i++) {                       /* mono: la misma voz */
        bm_voice *v = &c->voice[i];
        if (v->active && v->track == e->track && !v->fade) { acid_voice_start(e, v, note, vel, lk, legato && v->gate); return; }
    }
    bm_voice *v = bm_voice_get(c, e->track, note, vel, lk, 0);
    if (v) acid_voice_start(e, v, note, vel, lk, 0);
}

static bool acid_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    bool any = false;
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        if (v->inc != v->inc_target) {                          /* slide (por bloque) */
            uint32_t gs = (uint32_t)(t->glide + 8) * (BM_FS / 254u);
            int64_t gk = ((int64_t)65536 * frames) / (gs + frames);
            int64_t d = (int64_t)v->inc_target - (int64_t)v->inc;
            int64_t st = (d * gk) >> 16;
            if (st == 0) v->inc = v->inc_target; else v->inc = (uint32_t)((int64_t)v->inc + st);
        }
        int acc = v->vel >= 120;
        int dec = eff(v, t, 2);
        uint32_t c_f = bm_decay_coef[bm_clampi(acc ? dec * 2 / 3 : dec, 0, 127)];
        uint32_t c_amp = bm_decay_coef[bm_clampi(90 + dec / 4, 0, 127)];
        uint32_t c_rel = bm_decay_coef[14];
        int mod = eff(v, t, 3) * (acc ? 127 + t->accent : 127) / 127;
        int ci = eff(v, t, 0) + (int)((mod * (v->fenv >> 22)) / 340);
        ci = bm_clampi(ci, 0, 124);
        int32_t g = bm_onepole_a[ci];
        int32_t kq = eff(v, t, 1) * 990 / 127;                   /* realimentación Q8: hasta ~3.9 */
        int sq = t->source & 1;
        int32_t lvl = acc ? 127 : bm_clampi(v->vel + 8, 1, 127);
        for (uint16_t i = 0; i < frames && v->active; i++) {
            int32_t o = sq ? osc_pulse(v->phase, v->inc, 0x80000000u) : osc_saw(v->phase, v->inc);
            v->phase += v->inc;
            int32_t x = soft(o - (int32_t)(((int64_t)v->z[3] * kq) >> 8), 22000);
            v->z[0] += (int32_t)(((int64_t)(x - v->z[0]) * g) >> 15);
            v->z[1] += (int32_t)(((int64_t)(v->z[0] - v->z[1]) * g) >> 15);
            v->z[2] += (int32_t)(((int64_t)(v->z[1] - v->z[2]) * g) >> 15);
            v->z[3] += (int32_t)(((int64_t)(v->z[2] - v->z[3]) * g) >> 15);
            int32_t y = soft(v->z[3] + (int32_t)(((int64_t)v->z[3] * kq) >> 10), 26000);
            switch (v->stage) {
            case ENV_ATK: v->env += ACID_ATK; if (v->env >= (1 << 30)) { v->env = 1 << 30; v->stage = ENV_DEC; } break;
            case ENV_REL:
                v->env = (int32_t)(((int64_t)v->env * c_rel) >> 30);
                if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; }
                break;
            default: v->env = (int32_t)(((int64_t)v->env * c_amp) >> 30); if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; } break;
            }
            v->fenv = (int32_t)(((int64_t)v->fenv * c_f) >> 30);
            int32_t out = (int32_t)(((int64_t)y * (v->env >> 14)) >> 16);
            out = out * lvl >> 7;
            int fading = v->fade != 0;
            buf[i] += voice_fade_step(c, v, out);
            if (fading && v->fade == 0) break;
        }
    }
    return any;
}

static void acid_init(bm_engine *e) { common_init(e); }

/* ------------------------------------------------------------ batería modelada (T1 y canales KIT)
 * Cada instrumento (bm_lane) tiene su modelo (core/bm_drums.h) y su golpe: AFINA, DECAY, TONO y
 * COLOR. Las 4 macros del canal (TONO, AFINA, DECAY, COLOR; 64 = nada) los corren a todos y
 * admiten P-Lock por paso. El acento pega más fuerte (los modelos de Plaits cambian con él). */
#define DRUM_GAIN 30000.0f

static int drum_mod(int base, int macro) { return bm_clampi(base + macro - 64, 0, 127); }

static void drum_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)legato;
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int snd = note % BM_LANES;
    const bm_lane *ln = &bm_trk_tr(c->pat, e->track)->lane[snd];
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = e->track; v->note = (uint8_t)snd; v->sound = (uint8_t)snd; v->vel = vel;
    v->stage = ENV_DEC; v->gate = 0; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->choke = ln->choke & 3;
    v->gain = (uint8_t)bm_clampi(ln->level, 0, 127);
    v->env = 1 << 30;
    int model = ln->model % DRM_COUNT;
    float semis = (float)(drum_mod(ln->tune, eff(v, t, 1)) - 64) * 0.375f;
    float hz = (float)bm_drm_base_hz[model] * exp2f(semis / 12.0f);
    bm_drm_trigger((int)(v - c->voice), model, hz, drum_mod(ln->tone, eff(v, t, 0)) / 127.0f,
                   drum_mod(ln->decay, eff(v, t, 2)) / 127.0f, drum_mod(ln->color, eff(v, t, 3)) / 127.0f, vel / 127.0f);
}

static void drum_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    int snd = note % BM_LANES;
    const bm_lane *ln = &bm_trk_tr(c->pat, e->track)->lane[snd];
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    int choke = ln->choke & 3;
    for (int i = 0; i < BM_VOICES; i++) {                       /* la misma línea o su grupo: se corta */
        bm_voice *v = &c->voice[i];
        if (v->active && v->track == e->track && (v->sound == snd || (choke && v->choke == choke))) bm_voice_fade(v);
    }
    int m = ln->model % DRM_COUNT;
    if ((m == DRM_BD_ANALOG || m == DRM_BD_SINTE) && e->track == 0 && c->sc_trig_at < 0) c->sc_trig_at = (int16_t)c->blk_off;
    bm_voice *v = bm_voice_get(c, e->track, (uint8_t)snd, vel, lk, 0);
    if (v) drum_voice_start(e, v, (uint8_t)snd, vel, lk, 0);
}

static void drum_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; /* golpes */ }

static bool drum_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    bool any = false;
    static float tmp[BM_BLOCK];
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        int alive = bm_drm_render(vi, tmp, frames);
        float g = DRUM_GAIN * (float)v->gain / 100.0f, pk = 0.0f;
        for (uint16_t i = 0; i < frames; i++) pk = fmaxf(pk, fabsf(tmp[i]));
        if (!v->fade) for (uint16_t i = 0; i < frames; i++) buf[i] += (int32_t)(tmp[i] * g);
        else
            for (uint16_t i = 0; i < frames; i++) {
                buf[i] += voice_fade_step(c, v, (int32_t)(tmp[i] * g));
                if (!v->fade) { alive = v->active; break; }       /* terminó el fade (o arrancó la voz pendiente) */
            }
        v->env = (int32_t)(fminf(pk, 1.0f) * (float)(1 << 30));   /* nivel real: lo que ya no se oye se roba sin fade */
        if (!alive && v->track == e->track && !v->pend) v->active = 0;
    }
    return any;
}

static void drum_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_drum = {drum_init, drum_note_on, drum_note_off, common_set_macro, drum_process, drum_voice_start};

/* ------------------------------------------------------------ T2: sampler de loops en slices
 * La nota lleva el slice (bits 0-2), la fuente (bits 3-4: 0 A, 1 B, 2 escucha), reversa (bit 5)
 * y media velocidad (bit 6). El loop sigue el tempo: suena a BPM / BPM del loop, más PITCH. */
static void slice_bounds(const bm_smp_src *m, int i, uint32_t *s, uint32_t *e)
{
    if (m->nmarks == 0) {
        *s = m->len * (uint32_t)i / BM_SLICES;
        *e = m->len * (uint32_t)(i + 1) / BM_SLICES;
    } else {
        *s = m->marks[i];
        *e = (i + 1 < m->nmarks) ? m->marks[i + 1] : m->len;
    }
}

static int32_t smp_at(const bm_smp_src *m, int32_t i)
{
    if (i < 0 || (uint32_t)i >= m->len) return 0;
    return (int32_t)m->data[i] << 8;
}

uint32_t bm_brk_rate(const bm_ctx *c, const bm_smp_src *m)
{
    uint32_t r = bm_semi_ratio[bm_clampi(c->pat->trk[1].pitch, 0, 48)];
    if (m->bpm_x10) r = (uint32_t)(((uint64_t)r * c->pat->meta.bpm_x10) / m->bpm_x10);
    return r;
}

static void smp_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)legato;
    bm_ctx *c = e->ctx;
    int sl = note & BRK_SL_MASK, src = (note >> BRK_SRC_SH) & 3;
    if (src > 2) src = 0;
    const bm_smp_src *m = &c->smp[src];
    uint32_t s, en;
    slice_bounds(m, sl, &s, &en);
    if (en <= s) { v->active = 0; return; }
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = 1; v->note = (uint8_t)sl; v->vel = vel;
    v->stage = ENV_DEC; v->gate = 1; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->sound = (uint8_t)sl; v->src = (uint8_t)src; v->dir = (note & BRK_REV) ? 1 : 0; v->loopv = 0;
    v->gain = m->gain ? m->gain : 64;
    uint32_t ratio = bm_brk_rate(c, m);
    if (note & BRK_HALF) ratio >>= 1;
    v->step = (uint32_t)(((uint64_t)c->smp_base_inc * ratio) >> 16);
    v->start = s; v->end = en;
    v->pos = (en - s) * (uint32_t)c->brk_next_off;          /* Q16: dentro del slice */
    c->brk_next_off = 0;
    v->env = 1 << 30; v->lp = 0;
}

static void smp_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    int src = (note >> BRK_SRC_SH) & 3, sl = note & BRK_SL_MASK;
    if (src > 2) return;
    const bm_smp_src *m = &c->smp[src];
    if (m->len == 0 || !m->data || c->smp_rec) { c->brk_next_off = 0; return; }
    if (m->nmarks && sl >= m->nmarks) { c->brk_next_off = 0; return; }
    for (int i = 0; i < BM_VOICES; i++)                    /* break: un slice corta al anterior */
        if (c->voice[i].active && c->voice[i].track == 1) bm_voice_fade(&c->voice[i]);
    c->brk_last = note; c->brk_last_off = c->brk_next_off; c->st.brk_slice = (int8_t)sl;
    c->st.lane_trig[1] |= (uint16_t)(1u << sl);
    bm_voice *v = bm_voice_get(c, 1, note, vel, lk, 0);
    if (v) smp_voice_start(e, v, note, vel, lk, 0);
    c->brk_next_off = 0;
}

static void smp_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; }

/* Paso sostenido: el loop A suena entero desde pos y vuelve a empezar, hasta que se suelta. */
void bm_brk_hold(bm_ctx *c, uint32_t pos)
{
    const bm_smp_src *m = &c->smp[0];
    if (!m->len || !m->data || c->smp_rec) return;
    for (int i = 0; i < BM_VOICES; i++)
        if (c->voice[i].active && c->voice[i].track == 1) bm_voice_fade(&c->voice[i]);
    uint8_t lk[4] = {BM_LOCK_NONE, BM_LOCK_NONE, BM_LOCK_NONE, BM_LOCK_NONE};
    bm_voice *v = NULL;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N && !v; i++) if (!c->voice[i].active && !c->voice[i].pend) v = &c->voice[i];
    if (!v) v = bm_voice_get(c, 1, 0, 110, lk, 0);
    if (!v) return;
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = 1; v->note = 0; v->vel = 110; v->stage = ENV_DEC; v->gate = 1;
    v->fade = 0; v->pend = 0; v->age = ++c->age; v->sound = 0; v->src = 0; v->dir = 0; v->loopv = 1;
    v->gain = m->gain ? m->gain : 64;
    v->step = (uint32_t)(((uint64_t)c->smp_base_inc * bm_brk_rate(c, m)) >> 16);
    v->start = 0; v->end = m->len;
    v->pos = (pos < m->len ? pos : 0) << 16;
    v->env = 1 << 30; v->lp = 0;
}

static bool smp_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[1];
    bool any = false;
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != 1) continue;
        any = true;
        const bm_smp_src *m = &c->smp[v->src % 3];
        if (!m->data) { v->active = 0; continue; }
        int32_t a = bm_onepole_a[eff(v, t, 0)];
        uint32_t coef = bm_decay_coef[eff(v, t, 2)];
        bool herm = t->interp != 0;
        uint32_t span = v->end - v->start;
        for (uint16_t i = 0; i < frames && v->active; i++) {
            uint32_t off = v->pos >> 16;
            if (off >= span && v->loopv) { v->pos -= span << 16; off = v->pos >> 16; }   /* vuelve al principio */
            if (off >= span) { v->active = 0; v->pend = 0; break; }
            int32_t idx = v->dir ? (int32_t)(v->end - 1 - off) : (int32_t)(v->start + off);
            int32_t s, dn = v->dir ? -1 : 1;                   /* dn: hacia dónde avanza la lectura */
            if (herm) s = hermite(smp_at(m, idx - dn), smp_at(m, idx), smp_at(m, idx + dn), smp_at(m, idx + 2 * dn), v->pos & 0xFFFF);
            else s = lerp15(smp_at(m, idx), smp_at(m, idx + dn), v->pos & 0xFFFF);
            v->lp += ((s - v->lp) * a) >> 15;
            if (!v->loopv) v->env = (int32_t)(((int64_t)v->env * coef) >> 30);
            int32_t out = (int32_t)(((int64_t)v->lp * (v->env >> 14)) >> 16);
            out = ((out * v->vel) >> 7) * v->gain >> 6;
            /* micro-fade de salida al final del slice para que no haga click */
            uint32_t left = span - off;
            if (left < AT22(64) && !v->loopv) out = out * (int32_t)left / (int32_t)AT22(64);
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

/* ------------------------------------------------------------ MIDI: notas hacia afuera */
static void midi_send(bm_ctx *c, uint8_t a, uint8_t b, uint8_t d)
{
    if (!c->midi_out) return;
    c->midi_out(c->midi_user, a); c->midi_out(c->midi_user, b); c->midi_out(c->midi_user, d);
}

static int midi_note(const bm_ctx *c, const bm_track *t, uint8_t note)
{
    return bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
}

static void midi_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    for (int i = 0; i < 4; i++) e->lock_next[i] = BM_LOCK_NONE;
    e->legato_next = 0;
    midi_send(c, (uint8_t)(0x90 | (t->midi_ch & 15)), (uint8_t)midi_note(c, t, note), vel ? vel : 1);
}

static void midi_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    midi_send(c, (uint8_t)(0x80 | (t->midi_ch & 15)), (uint8_t)midi_note(c, t, note), 0);
}

static bool midi_process(bm_engine *e, int32_t *buf, uint16_t frames) { (void)e; (void)buf; (void)frames; return false; }

static void midi_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)e; (void)note; (void)vel; (void)lk; (void)legato;
    v->active = 0;
}

static void midi_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_midi = {midi_init, midi_note_on, midi_note_off, common_set_macro, midi_process, midi_voice_start};

/* ------------------------------------------------------------ ANALOG (T3-T6): core/bm_analog.cpp
 * Las 4 macros (CORTE, RESO, ENVOLV, FORMA) admiten P-Lock por nota; el resto de los ajustes son
 * los del canal (bm_pattern.prm, páginas de SONIDO). POLI usa el pool; MONO y LEGATO, una voz con
 * glide (en T3 siempre una voz). LIGA en una nota = legato aunque el modo sea MONO. */
#define AN_GAIN 32768.0f

static float note_of(const bm_ctx *c, const bm_track *t, uint8_t note)
{
    return (float)bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
}

static void an_macros(const bm_voice *v, const bm_track *t, uint8_t m[4]) { for (int i = 0; i < 4; i++) m[i] = eff(v, t, i); }

static void an_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    memcpy(v->lk, lk, 4);
    if (!legato || !v->active || v->track != e->track) legato = 0;
    v->active = 1; v->track = e->track; v->note = note; v->vel = vel; v->gate = 1;
    v->stage = ENV_ATK; v->fade = 0; v->pend = 0; v->age = ++c->age; v->env = 1 << 30;
    uint8_t m[4];
    an_macros(v, t, m);
    bm_an_start((int)(v - c->voice), c->pat->prm[e->track], m, note_of(c, t, note), vel / 127.0f, legato);
}

static int an_mode(const bm_ctx *c, int t)
{
    int m = c->pat->prm[t][AN_MODE] % AN_MODES;
    return t == 2 && m == AN_M_POLY ? AN_M_MONO : m;
}

static void an_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t tie = e->legato_next;
    e->legato_next = 0;
    int mode = an_mode(c, e->track);
    if (mode != AN_M_POLY) {                                    /* una voz: la que suena, si hay */
        bm_voice *best = NULL;
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && !v->fade && (!best || v->gate > best->gate || v->age > best->age)) best = v;
        }
        if (best) {
            int leg = best->gate && (mode == AN_M_LEGATO || tie) ? 1 : 2;
            an_voice_start(e, best, note, vel, lk, (uint8_t)leg);
            return;
        }
    }
    bm_voice *v = bm_voice_get(c, e->track, note, vel, lk, 0);
    if (v) an_voice_start(e, v, note, vel, lk, 0);
}

static void an_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend && v->pend_track == e->track && v->pend_note == note) v->pend = 0;
        if (v->active && v->track == e->track && v->note == note && v->gate) {
            v->gate = 0; v->stage = ENV_REL;
            if (c->pat->trk[e->track].engine == ENG_ANALOG) bm_an_release(i);
        }
    }
}

static bool an_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    const uint8_t *prm = c->pat->prm[e->track];
    bool any = false;
    static float tmp[BM_BLOCK];
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        uint8_t m[4];
        an_macros(v, t, m);
        int alive = bm_an_render(vi, prm, m, tmp, frames);
        if (!v->fade) for (uint16_t i = 0; i < frames; i++) buf[i] += (int32_t)(tmp[i] * AN_GAIN);
        else
            for (uint16_t i = 0; i < frames; i++) {
                buf[i] += voice_fade_step(c, v, (int32_t)(tmp[i] * AN_GAIN));
                if (!v->fade) { alive = v->active; break; }
            }
        if (v->active && v->track == e->track) {
            v->env = (int32_t)(fminf(bm_an_level(vi), 1.0f) * (float)(1 << 30));
            if (!alive && !v->pend) v->active = 0;
        }
    }
    return any;
}

static void an_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_analog = {an_init, an_note_on, an_note_off, common_set_macro, an_process, an_voice_start};
static void acid_note_off(bm_engine *e, uint8_t note) { an_note_off(e, note); }
const bm_engine_vt bm_eng_acid = {acid_init, acid_note_on, acid_note_off, common_set_macro, acid_process, acid_voice_start};

/* ------------------------------------------------------------ FM: 6 operadores (msfa, core/bm_fm.cpp)
 * El patch de fábrica (PRESETS) y encima las macros, con 64 = el patch tal cual:
 *   BRILLO   nivel de los moduladores (±32)        REALIM  realimentación (±4)
 *   DECAY    tiempos de caída y salida de todos     ALGORITMO  fuera del centro, otro de los 32
 * ATAQUE y SALIDA del canal alargan las envolventes. Mono con glide en T3 o con LIGA. */
static uint8_t fm_m(const bm_voice *v, const bm_track *t, int i) { return v ? eff(v, t, i) : t->macro[i]; }

int bm_fm_alg_of(const bm_track *t, int m4)
{
    const uint8_t *p = bm_fm_rom[t->source % BM_FM_PATCHES];
    return (m4 >= 56 && m4 <= 72) ? p[BM_FM_ALG] + 1 : m4 * 32 / 128 + 1;
}

int bm_fm_fb_of(const bm_track *t, int m2)
{
    return bm_clampi(bm_fm_rom[t->source % BM_FM_PATCHES][BM_FM_FB] + (m2 - 64) / 16, 0, 7);
}

void bm_fm_patch_of(const bm_track *t, const bm_voice *v, uint8_t *p)
{
    memcpy(p, bm_fm_rom[t->source % BM_FM_PATCHES], BM_FM_PATCH);
    int m1 = fm_m(v, t, 0), m2 = fm_m(v, t, 1), m3 = fm_m(v, t, 2), m4 = fm_m(v, t, 3);
    p[BM_FM_ALG] = (uint8_t)(bm_fm_alg_of(t, m4) - 1);
    p[BM_FM_FB] = (uint8_t)bm_fm_fb_of(t, m2);
    uint8_t car = bm_fm_carriers(p[BM_FM_ALG]);
    int dec = (m3 - 64) / 3, atk = t->attack * 70 / 127, rel = (t->release - 64) / 2;
    for (int op = 0; op < 6; op++) {
        uint8_t *o = p + op * BM_FM_OP;
        if (!(car & (1u << op))) o[FMO_OL] = (uint8_t)bm_clampi(o[FMO_OL] + (m1 - 64) / 2, 0, 99);
        o[FMO_R2] = (uint8_t)bm_clampi(o[FMO_R2] - dec, 1, 99);
        o[FMO_R3] = (uint8_t)bm_clampi(o[FMO_R3] - dec, 1, 99);
        o[FMO_R4] = (uint8_t)bm_clampi(o[FMO_R4] - dec - rel, 1, 99);
        if (atk) o[FMO_R1] = (uint8_t)bm_clampi(o[FMO_R1] < 99 - atk ? o[FMO_R1] : 99 - atk, 1, 99);
    }
}

static uint32_t fm_sig(const bm_voice *v, const bm_track *t)
{
    return (uint32_t)fm_m(v, t, 0) | (uint32_t)fm_m(v, t, 1) << 7 | (uint32_t)fm_m(v, t, 2) << 14 | (uint32_t)fm_m(v, t, 3) << 21
         | (uint32_t)((t->source ^ t->attack ^ (t->release << 1)) & 15) << 28;
}

static int fm_note(const bm_ctx *c, const bm_track *t, uint8_t note)
{
    return bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
}

static void fm_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int vi = (int)(v - c->voice);
    uint8_t p[BM_FM_PATCH];
    int glide = legato && v->active && v->track == e->track && v->gate && !v->fade;
    int from = glide ? fm_note(c, t, v->note) : fm_note(c, t, note);
    memcpy(v->lk, lk, 4);
    bm_fm_patch_of(t, v, p);
    v->ic1 = (int32_t)fm_sig(v, t);
    if (glide) {                                          /* ligado: cambia de nota sin redisparar */
        v->note = note; v->vel = vel;
        bm_fm_update(vi, p, fm_note(c, t, note), vel, t->glide);
        return;
    }
    v->active = 1; v->track = e->track; v->note = note; v->vel = vel; v->gate = 1;
    v->stage = ENV_SUS; v->env = 1 << 30; v->fade = 0; v->pend = 0; v->age = ++c->age;
    bm_fm_start(vi, e->track, p, fm_note(c, t, note), vel, from, -1);
}

static void fm_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t legato = e->legato_next;
    e->legato_next = 0;
    if (e->track == 2) {                                  /* T3: mono, con glide si liga */
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && v->gate && !v->fade) { fm_voice_start(e, v, note, vel, lk, legato); return; }
        }
    }
    bm_voice *v = bm_voice_get(c, e->track, note, vel, lk, 0);
    if (v) fm_voice_start(e, v, note, vel, lk, 0);
}

static void fm_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend && v->pend_track == e->track && v->pend_note == note) v->pend = 0;
        if (v->active && v->track == e->track && v->note == note && v->gate) {
            v->gate = 0; v->stage = ENV_REL;
            bm_fm_release(i);
        }
    }
}

static bool fm_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    bool any = false;
    int32_t tmp[BM_BLOCK];
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        uint32_t sig = fm_sig(v, t);
        if ((int32_t)sig != v->ic1) {                     /* se movió una perilla: el patch cambia en vivo */
            uint8_t p[BM_FM_PATCH];
            bm_fm_patch_of(t, v, p);
            bm_fm_update(vi, p, fm_note(c, t, v->note), v->vel, -1);
            v->ic1 = (int32_t)sig;
        }
        if (v->stage == ENV_REL) v->env -= v->env >> 7;    /* para elegir a quién robar */
        int vive;
        if (!v->fade) vive = bm_fm_render(vi, buf, frames);   /* lo normal: directo al canal */
        else {                                            /* robo o choke: con el micro-fade */
            memset(tmp, 0, frames * sizeof tmp[0]);
            vive = bm_fm_render(vi, tmp, frames);
            for (uint16_t i = 0; i < frames; i++) {
                buf[i] += voice_fade_step(c, v, tmp[i]);
                if (v->fade == 0) break;                  /* la voz cambió de dueño o terminó */
            }
        }
        if (!vive && v->track == e->track && !v->fade && v->stage == ENV_REL) { v->active = 0; v->pend = 0; }
    }
    return any;
}

static void fm_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_fm = {fm_init, fm_note_on, fm_note_off, common_set_macro, fm_process, fm_voice_start};

const bm_engine_vt *bm_engine_vt_for(int engine)
{
    switch (engine) {
    case ENG_FM: return &bm_eng_fm;
    case ENG_ACID: return &bm_eng_acid;
    case ENG_DRUM: case ENG_KIT: return &bm_eng_drum;
    case ENG_BREAK: return &bm_eng_sampler;
    case ENG_MIDI: return &bm_eng_midi;
    case ENG_ANALOG: return &bm_eng_analog;
    default: return &bm_eng_analog;
    }
}
