/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — voces y motores de sonido (vtable común, SPEC_V3 §2, §3, §5). */
#include <string.h>
#include "bm_audio.h"
#include "bm_rom.h"
#include "bm_util.h"
#include "bm_fm.h"
#include "bm_drums.h"
#include "bm_wave.h"
#include "bm_snap.h"
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

/* voces como máximo por máquina dentro del pool (BP-1 pads, BB-303, B-MINI, BX-7, BITTAL, BUNO-6) */
static const uint8_t VCAP[ENG_COUNT] = {BM_V_DRUM_N, 6, 1, 1, 6, 4, 6};

bm_voice *bm_voice_get(bm_ctx *c, int track, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    int lo, n;
    if (track == 0) { lo = BM_V_DRUM0; n = BM_V_DRUM_N; }
    else { lo = BM_V_POOL0; n = BM_V_POOL_N; }

    int mine = 0;
    if (lo == BM_V_POOL0)
        for (int i = 0; i < n; i++) { const bm_voice *v = &c->voice[lo + i]; mine += (v->active && v->track == track && !v->fade) || (v->pend && v->pend_track == track); }
    int full = lo == BM_V_POOL0 && mine >= VCAP[c->pat->trk[track % BM_TRACKS].engine % ENG_COUNT];   /* lleno: roba una suya */

    bm_voice *best = NULL;
    for (int i = 0; i < n; i++) {
        bm_voice *v = &c->voice[lo + i];
        if (full && !(v->active && v->track == track) && !(v->pend && v->pend_track == track)) continue;
        if (!v->active && !v->pend && !full) return v;
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
            /* audio en dos núcleos: si la voz pasa a un canal del otro núcleo, arranca después de
             * esperarlo (bm_audio_render), para que los dos no toquen la misma voz a la vez */
            if (c->par_on && (((c->par_mask >> v->track) ^ (c->par_mask >> v->pend_track)) & 1)) { c->par_defer = 1; return 0; }
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
const char *const bm_acid_names[2] = {"SAW", "SQUARE"};

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
        int dec = eff(v, t, 3);                                  /* macros: CUTOFF RESO ENV MOD DECAY */
        uint32_t c_f = bm_decay_coef[bm_clampi(acc ? dec * 2 / 3 : dec, 0, 127)];
        uint32_t c_amp = bm_decay_coef[bm_clampi(90 + dec / 4, 0, 127)];
        uint32_t c_rel = bm_decay_coef[14];
        int mod = eff(v, t, 2) * (acc ? 127 + t->accent : 127) / 127;
        int ci = eff(v, t, 0) + (int)((mod * (v->fenv >> 22)) / 340);
        ci = bm_clampi(ci, 0, 124);
        int32_t g = bm_onepole_a[ci];
        int32_t kq = eff(v, t, 1) * 990 / 127;                   /* realimentación Q8: hasta ~3.9 */
        int sq = c->pat->prm[e->track][AC_WAVE] & 1;
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

/* ------------------------------------------------------------ T2: sampler de pads
 * Cada pad (bm_pad) es un trozo de una toma: len muestras desde start. La nota es el pad (0..15)
 * más BRK_REV y BRK_HALF (negras en vivo y BREAK). Las 4 macros del canal admiten P-Lock por
 * paso: FILTER (pasa-bajos), PITCH (64 = 0, +-12 st), DECAY (127 = nada) y START (corre el inicio).
 * Lo dispara el secuenciador (cada modo con la compuerta de un paso) o una tecla sostenida
 * (bm_pad_key): ONESHOT suena entero, KEY mientras se sostiene, THRU sigue por el resto de la
 * toma y LOOP repite el pad. */
static int32_t smp_at(const bm_smp_src *m, int32_t i)
{
    if (i < 0 || (uint32_t)i >= m->len) return 0;
    return (int32_t)m->data[i] << 8;
}

const bm_smp_src *bm_smp_take(const bm_ctx *c, int src)
{
    if (src >= 0 && src < BM_TAKES) return &c->takes[src];
    if (src == BM_TAKE_NEW) return &c->smp[0];
    return NULL;
}

/* velocidad de lectura de un pad, Q16 (con el PITCH del canal y, si sigue el tempo, el BPM) */
static uint32_t pad_rate(const bm_ctx *c, const bm_pad *p, const bm_smp_src *m, int pitch_macro)
{
    int semis = (int)p->pitch - 24 + (pitch_macro - 64) * 12 / 63;
    uint32_t r = bm_semi_ratio[bm_clampi(semis + 24, 0, 48)];
    if ((p->flags & PAD_TEMPO) && m->bpm_x10) r = (uint32_t)(((uint64_t)r * c->pat->meta.bpm_x10) / m->bpm_x10);
    return r;
}

static void pad_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    int pi = note & 15, prev = (legato & 2) != 0, held = legato & 1;
    const bm_pad *p = prev ? &c->preview_pad : &c->pat->pad[pi];
    const bm_smp_src *m = bm_smp_take(c, p->src);
    const bm_track *t = &c->pat->trk[1];
    if (!m || !m->data || !p->len || p->start >= m->len) { v->active = 0; return; }
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = 1; v->note = (uint8_t)pi; v->sound = prev ? 0xFF : (uint8_t)pi; v->vel = vel;
    v->fade = 0; v->pend = 0; v->age = ++c->age;
    uint32_t s = p->start, len = p->len;
    if (len > m->len - s) len = m->len - s;
    uint32_t off = (uint32_t)((uint64_t)len * eff(v, t, 3) * 9u / (127u * 10u));   /* START */
    s += off; len -= off;
    v->src = p->src; v->start = s; v->end = s + len;
    v->ic2 = (int32_t)len;                                 /* el largo del pad (THRU vuelve a él al soltar) */
    v->dir = (uint8_t)(((p->flags & PAD_REV) != 0) ^ ((note & BRK_REV) != 0));
    v->gain = m->gain ? m->gain : 64;
    v->ph[0] = prev ? 100u : c->pat->tr[1].lane[pi].level;
    v->choke = prev ? 0 : (c->pat->tr[1].lane[pi].choke & 3);
    uint32_t ratio = pad_rate(c, p, m, eff(v, t, 1));
    if (note & BRK_HALF) ratio >>= 1;
    v->step = (uint32_t)(((uint64_t)c->smp_base_inc * ratio) >> 16);
    v->pos = 0; v->lp = 0;
    v->loopv = p->mode;
    v->gate = (uint8_t)held;
    v->ph[1] = 0; v->ph[2] = 0;                            /* LOOP: el tramo de sustain (relativo al inicio) */
    if (p->mode == PAD_LOOP && bm_pad_has_loop(p) && !off) {
        uint32_t ls = bm_pad_loop_start(p), le = bm_pad_loop_end(p);
        if (le > len) le = len;
        if (le > ls + 32u) { v->ph[1] = ls; v->ph[2] = le; }
    }
    /* el secuenciador abre la compuerta un paso (KEY y LOOP); la tecla, hasta que se suelta */
    v->phase = held || (p->mode != PAD_KEY && p->mode != PAD_LOOP) ? 0u : bm_ticks_to_samples(c, (uint32_t)bm_track_tps(c, 1));
    if (held && p->mode == PAD_THRU) { if (v->dir) v->start = 0; else v->end = m->len; }
    uint32_t att = bm_attack_ms(p->attack) * (BM_FS / 1000u);
    v->inc = att ? (1u << 30) / att : 0;
    v->env = att ? 0 : 1 << 30;
    v->stage = att ? ENV_ATK : ENV_SUS;
    v->inc_target = bm_decay_coef[bm_clampi(p->release, 0, 127)];
}

/* soltar: KEY y LOOP se apagan con su release; THRU termina el pad si todavía está en él */
static void pad_release(bm_voice *v)
{
    v->gate = 0; v->phase = 0;
    if (v->loopv == PAD_THRU) {
        uint32_t span = (uint32_t)v->ic2;
        if ((v->pos >> 16) < span) { if (v->dir) v->start = v->end - span; else v->end = v->start + span; }
        else v->stage = ENV_REL;
    } else if (v->loopv == PAD_KEY || v->loopv == PAD_LOOP) v->stage = ENV_REL;
}

static void pad_trigger(bm_engine *e, uint8_t note, uint8_t vel, uint8_t legato)
{
    bm_ctx *c = e->ctx;
    int pi = note & 15, prev = (legato & 2) != 0;
    const bm_pad *p = prev ? &c->preview_pad : &c->pat->pad[pi];
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    if (c->smp_rec || !p->len || (p->src >= BM_TAKES && p->src != BM_TAKE_NEW)) return;   /* un pad vacío no suena */
    int choke = prev ? 0 : (c->pat->tr[1].lane[pi].choke & 3);
    for (int i = 0; i < BM_VOICES; i++) {                       /* el mismo pad o su grupo: se corta */
        bm_voice *v = &c->voice[i];
        if (!v->active || v->track != 1) continue;
        if (v->sound == (prev ? 0xFF : pi) || (choke && v->choke == choke)) bm_voice_fade(v);
    }
    c->brk_last = note; c->st.brk_slice = (int8_t)pi;
    if (!prev) c->st.lane_trig[1] |= (uint16_t)(1u << pi);
    bm_voice *v = bm_voice_get(c, 1, note, vel, lk, legato);
    if (v) pad_voice_start(e, v, note, vel, lk, legato);
}

static void smp_note_on(bm_engine *e, uint8_t note, uint8_t vel) { pad_trigger(e, note, vel, 0); }

static void smp_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; }

/* Tecla de un pad (on = 1) o soltarla (on = 0). */
void bm_pad_key(bm_ctx *c, int pad, uint8_t vel, int on)
{
    if (pad < 0 || pad > 15) return;
    if (!on) {
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == 1 && v->gate && v->sound == pad) pad_release(v);
        }
        return;
    }
    uint8_t note = (uint8_t)pad;
    if (c->brk_live & 1) note |= BRK_REV;
    if (c->brk_live & 2) note |= BRK_HALF;
    pad_trigger(&c->eng[1], note, vel, 1);
}

/* Escuchar un trozo de una toma (TAKES, grabadora): sostenido sigue, como un pad THRU. */
void bm_smp_preview(bm_ctx *c, int src, uint32_t start, uint32_t len, int on)
{
    if (!on) {
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == 1 && v->gate && v->sound == 0xFF) pad_release(v);
        }
        return;
    }
    bm_pad_clear(&c->preview_pad);
    c->preview_pad.src = (uint8_t)src; c->preview_pad.mode = PAD_THRU;
    c->preview_pad.start = (uint16_t)start; c->preview_pad.len = (uint16_t)len;
    pad_trigger(&c->eng[1], 0, 110, 1 | 2);
}

static bool smp_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[1];
    bool any = false;
    int8_t ps = -1;
    uint32_t pp = 0, page = 0;
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != 1) continue;
        any = true;
        const bm_smp_src *m = bm_smp_take(c, v->src);
        if (!m || !m->data) { v->active = 0; continue; }
        if (!v->fade && v->age >= page) {                       /* el cabezal: la voz más nueva */
            uint32_t off = v->pos >> 16, span = v->end - v->start;
            if (off >= span) off = span ? span - 1 : 0;
            ps = (int8_t)v->src; page = v->age;
            pp = v->dir ? v->end - 1 - off : v->start + off;
        }
        int32_t a = bm_onepole_a[eff(v, t, 0)];
        int dec = eff(v, t, 2);
        uint32_t coef = bm_decay_coef[dec];
        bool herm = t->interp != 0;
        for (uint16_t i = 0; i < frames && v->active; i++) {
            uint32_t span = v->end - v->start, off = v->pos >> 16;
            uint32_t lend = v->ph[2] ? v->ph[2] : span, lbeg = v->ph[2] ? v->ph[1] : 0;
            bool sus = v->loopv == PAD_LOOP && (v->gate || v->phase) && v->stage != ENV_REL;
            if (sus && off >= lend) { v->pos -= (lend - lbeg) << 16; off = v->pos >> 16; }   /* sostenido: el loop */
            else if (off >= span) { v->active = 0; v->pend = 0; break; }                     /* suelto: hasta el final */
            int32_t idx = v->dir ? (int32_t)(v->end - 1 - off) : (int32_t)(v->start + off);
            int32_t s, dn = v->dir ? -1 : 1;
            if (herm) s = hermite(smp_at(m, idx - dn), smp_at(m, idx), smp_at(m, idx + dn), smp_at(m, idx + 2 * dn), v->pos & 0xFFFF);
            else s = lerp15(smp_at(m, idx), smp_at(m, idx + dn), v->pos & 0xFFFF);
            v->lp += ((s - v->lp) * a) >> 15;
            switch (v->stage) {
            case ENV_ATK: v->env += (int32_t)v->inc; if (v->env >= (1 << 30)) { v->env = 1 << 30; v->stage = ENV_SUS; } break;
            case ENV_REL: v->env = (int32_t)(((int64_t)v->env * v->inc_target) >> 30); break;
            default: if (dec < 127) v->env = (int32_t)(((int64_t)v->env * coef) >> 30); break;
            }
            if (v->phase && --v->phase == 0) pad_release(v);    /* la compuerta del paso se cerró */
            int32_t out = (int32_t)(((int64_t)v->lp * (v->env >> 14)) >> 16);
            out = ((out * v->vel) >> 7) * v->gain >> 6;
            out = out * (int32_t)v->ph[0] / 100;
            uint32_t left = span - off;                          /* micro-fade al final para que no haga click */
            if (left < AT22(64) && v->loopv != PAD_LOOP) out = out * (int32_t)left / (int32_t)AT22(64);
            int fading = v->fade != 0;
            buf[i] += voice_fade_step(c, v, out);
            if (fading && v->fade == 0) break;     /* la voz cambió de dueño o terminó */
            v->pos += v->step;
            if (v->env < (1 << 14) && v->stage != ENV_ATK) { v->active = 0; v->pend = 0; }
        }
    }
    c->st.smp_pos = pp; c->st.smp_pos_src = ps;
    return any;
}

static void smp_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_sampler = {smp_init, smp_note_on, smp_note_off, common_set_macro, smp_process, pad_voice_start};

/* ------------------------------------------------------------ MIDI OUT: el canal suena y además manda
 * sus notas por MIDI (MAS > MIDI OUT). Envuelve al motor de su máquina. */
static void midi_send(bm_ctx *c, uint8_t a, uint8_t b, uint8_t d)
{
    if (!c->midi_out) return;
    c->midi_out(c->midi_user, a); c->midi_out(c->midi_user, b); c->midi_out(c->midi_user, d);
}

static int midi_note(const bm_ctx *c, const bm_track *t, uint8_t note)
{
    return bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
}

static const bm_engine_vt *inner(const bm_engine *e) { return bm_engine_vt_for(e->ctx->pat->trk[e->track].engine); }

static void mo_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    midi_send(c, (uint8_t)(0x90 | (t->midi_ch & 15)), (uint8_t)midi_note(c, t, note), vel ? vel : 1);
    inner(e)->note_on(e, note, vel);
}

static void mo_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    midi_send(c, (uint8_t)(0x80 | (t->midi_ch & 15)), (uint8_t)midi_note(c, t, note), 0);
    inner(e)->note_off(e, note);
}

static void mo_init(bm_engine *e) { inner(e)->init(e); }
static void mo_set_macro(bm_engine *e, uint8_t id, uint8_t val) { inner(e)->set_macro(e, id, val); }
static bool mo_process(bm_engine *e, int32_t *buf, uint16_t frames) { return inner(e)->process(e, buf, frames); }
static void mo_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    inner(e)->voice_start(e, v, note, vel, lk, legato);
}

const bm_engine_vt bm_eng_midi_out = {mo_init, mo_note_on, mo_note_off, mo_set_macro, mo_process, mo_voice_start};

/* ------------------------------------------------------------ núcleo WAVE: B-MINI, BITTAL, BUNO-6 (core/bm_wave.cpp)
 * Las 4 macros admiten P-Lock por nota; el resto de los ajustes son los del canal (bm_pattern.prm).
 * B-MINI: una voz (T3) con glide. BITTAL: POLI (hasta 4 voces), MONO o LEGATO. BUNO-6: 6 voces.
 * LIGA en una nota = legato aunque el modo sea MONO. */
#define WV_GAIN 32768.0f

static float note_of(const bm_ctx *c, const bm_track *t, uint8_t note)
{
    return (float)bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
}

static void wv_macros(const bm_voice *v, const bm_track *t, uint8_t m[4]) { for (int i = 0; i < 4; i++) m[i] = eff(v, t, i); }

static void wv_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    memcpy(v->lk, lk, 4);
    if (!legato || !v->active || v->track != e->track) legato = 0;
    v->active = 1; v->track = e->track; v->note = note; v->vel = vel; v->gate = 1;
    v->stage = ENV_ATK; v->fade = 0; v->pend = 0; v->age = ++c->age; v->env = 1 << 30;
    uint8_t m[4];
    wv_macros(v, t, m);
    bm_wv_start((int)(v - c->voice), t->engine, c->pat->prm[e->track], m, note_of(c, t, note), vel / 127.0f, legato);
}

static int wv_mode(const bm_ctx *c, int t)
{
    const bm_track *tr = &c->pat->trk[t];
    if (tr->engine == ENG_MINI) return c->pat->prm[t][MN_MODE] ? WM_LEGATO : WM_MONO;
    if (tr->engine == ENG_BITTAL) return c->pat->prm[t][BT_MODE] % WM_MODES;
    return WM_POLY;
}

static void wv_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t tie = e->legato_next;
    e->legato_next = 0;
    int mode = wv_mode(c, e->track);
    if (mode != WM_POLY) {                                      /* una voz: la que suena, si hay */
        bm_voice *best = NULL;
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && !v->fade && (!best || v->gate > best->gate || v->age > best->age)) best = v;
        }
        if (best) {
            int leg = best->gate && (mode == WM_LEGATO || tie) ? 1 : 2;
            wv_voice_start(e, best, note, vel, lk, (uint8_t)leg);
            return;
        }
    }
    bm_voice *v = bm_voice_get(c, e->track, note, vel, lk, 0);
    if (v) wv_voice_start(e, v, note, vel, lk, 0);
}

static void wv_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend && v->pend_track == e->track && v->pend_note == note) v->pend = 0;
        if (v->active && v->track == e->track && v->note == note && v->gate) {
            v->gate = 0; v->stage = ENV_REL;
            bm_wv_release(i);
        }
    }
}

static bool wv_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    const uint8_t *prm = c->pat->prm[e->track];
    bool any = false;
    static float tmp2[2][BM_BLOCK];
    int lane = c->par_on ? (c->par_mask >> e->track) & 1 : 0;
    float *tmp = tmp2[lane];
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        uint8_t m[4];
        wv_macros(v, t, m);
        int alive = bm_wv_render(vi, t->engine, prm, m, tmp, frames, lane);
        if (!v->fade) for (uint16_t i = 0; i < frames; i++) buf[i] += (int32_t)(tmp[i] * WV_GAIN);
        else
            for (uint16_t i = 0; i < frames; i++) {
                buf[i] += voice_fade_step(c, v, (int32_t)(tmp[i] * WV_GAIN));
                if (!v->fade) { alive = v->active; break; }
            }
        if (v->active && v->track == e->track) {
            v->env = (int32_t)(fminf(bm_wv_level(vi), 1.0f) * (float)(1 << 30));
            if (!alive && !v->pend) v->active = 0;
        }
    }
    return any;
}

static void wv_init(bm_engine *e) { common_init(e); }

const bm_engine_vt bm_eng_wave = {wv_init, wv_note_on, wv_note_off, common_set_macro, wv_process, wv_voice_start};
static void acid_note_off(bm_engine *e, uint8_t note)
{
    bm_ctx *c = e->ctx;
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend && v->pend_track == e->track && v->pend_note == note) v->pend = 0;
        if (v->active && v->track == e->track && v->note == note && v->gate) { v->gate = 0; v->stage = ENV_REL; }
    }
}
const bm_engine_vt bm_eng_acid = {acid_init, acid_note_on, acid_note_off, common_set_macro, acid_process, acid_voice_start};

/* ------------------------------------------------------------ FM: 6 operadores (msfa, core/bm_fm.cpp)
 * El patch de fábrica (PRESETS) y encima las macros, con 64 = el patch tal cual:
 *   MOD LVL  nivel de los moduladores (±32)        FEEDBACK  realimentación (±4)
 *   DECAY    tiempos de caída (R2-R3) de todos     RELEASE   la salida (R4) de todos
 * Encima también los ajustes del canal: el algoritmo (FX_ALG, 0 = el del patch), el LFO (FX_L*, 0 =
 * el del patch) y R1 ATK (attack del canal). Polifónico; una nota con LIGA sigue con glide (PORTA). */
static uint8_t fm_m(const bm_voice *v, const bm_track *t, int i) { return v ? eff(v, t, i) : t->macro[i]; }

int bm_fm_alg_of(const bm_track *t, const uint8_t *prm)
{
    int a = prm[FX_ALG];
    return a >= 1 && a <= 32 ? a : bm_fm_patch(t->source)[BM_FM_ALG] % 32 + 1;
}

int bm_fm_fb_of(const bm_track *t, int m2)
{
    return bm_clampi(bm_fm_patch(t->source)[BM_FM_FB] + (m2 - 64) / 16, 0, 7);
}

void bm_fm_patch_of(const bm_track *t, const uint8_t *prm, const bm_voice *v, uint8_t *p)
{
    memcpy(p, bm_fm_patch(t->source), BM_FM_PATCH);
    int m1 = fm_m(v, t, 0), m2 = fm_m(v, t, 1), m3 = fm_m(v, t, 2), m4 = fm_m(v, t, 3);
    p[BM_FM_ALG] = (uint8_t)(bm_fm_alg_of(t, prm) - 1);
    p[BM_FM_FB] = (uint8_t)bm_fm_fb_of(t, m2);
    for (int i = 0; i < 4; i++) if (prm[FX_LSPD + i]) p[137 + i] = (uint8_t)bm_clampi(prm[FX_LSPD + i] - 1, 0, 99);   /* LFS LFD LPMD LAMD */
    if (prm[FX_LPMD] > 1 && !(p[143] & 7)) p[143] = 3;           /* PMD sin sensibilidad en el patch: una media */
    uint8_t car = bm_fm_carriers(p[BM_FM_ALG]);
    int dec = (m3 - 64) / 3, atk = t->attack * 70 / 127, rel = (m4 - 64) / 2;
    for (int op = 0; op < 6; op++) {
        uint8_t *o = p + op * BM_FM_OP;
        if (!(car & (1u << op))) o[FMO_OL] = (uint8_t)bm_clampi(o[FMO_OL] + (m1 - 64) / 2, 0, 99);
        else if (prm[FX_LAMD] > 1 && !o[FMO_AMS]) o[FMO_AMS] = 2;  /* AMD: las portadoras lo sienten */
        o[FMO_R2] = (uint8_t)bm_clampi(o[FMO_R2] - dec, 1, 99);
        o[FMO_R3] = (uint8_t)bm_clampi(o[FMO_R3] - dec, 1, 99);
        o[FMO_R4] = (uint8_t)bm_clampi(o[FMO_R4] - dec - rel, 1, 99);
        if (atk) o[FMO_R1] = (uint8_t)bm_clampi(o[FMO_R1] < 99 - atk ? o[FMO_R1] : 99 - atk, 1, 99);
    }
}

static uint32_t fm_sig(const bm_voice *v, const bm_track *t, const uint8_t *prm)
{
    uint32_t h = (uint32_t)(t->source ^ t->attack) + 31u * prm[FX_ALG];
    for (int i = 0; i < 4; i++) h = h * 31u + prm[FX_LSPD + i];
    return (uint32_t)fm_m(v, t, 0) | (uint32_t)fm_m(v, t, 1) << 7 | (uint32_t)fm_m(v, t, 2) << 14 | (uint32_t)fm_m(v, t, 3) << 21
         | (h & 15u) << 28;
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
    bm_fm_patch_of(t, c->pat->prm[e->track], v, p);
    v->ic1 = (int32_t)fm_sig(v, t, c->pat->prm[e->track]);
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
    if (legato) {                                         /* LIGA: la última voz sigue con glide (PORTA) */
        bm_voice *last = NULL;
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && v->gate && !v->fade && (!last || v->age > last->age)) last = v;
        }
        if (last) { fm_voice_start(e, last, note, vel, lk, legato); return; }
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
        uint32_t sig = fm_sig(v, t, c->pat->prm[e->track]);
        if ((int32_t)sig != v->ic1) {                     /* se movió una perilla: el patch cambia en vivo */
            uint8_t p[BM_FM_PATCH];
            bm_fm_patch_of(t, c->pat->prm[e->track], v, p);
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
    case ENG_DRUM: return &bm_eng_drum;
    case ENG_BREAK: return &bm_eng_sampler;
    default: return &bm_eng_wave;
    }
}
