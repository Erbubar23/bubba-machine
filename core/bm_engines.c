/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — voces y motores de sonido (vtable común, SPEC_V3 §2, §3, §5). */
#include <string.h>
#include "bm_audio.h"
#include "bm_rom.h"
#include "bm_util.h"

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

/* ------------------------------------------------------------ ANALOG: dos osciladores con detune, sub y ruido
 * Se dibuja en el motor de ondas (mismo filtro SVF y ADSR); solo cambia el oscilador. */
const char *const bm_va_names[BM_VA_TIMBRES] = {"SIERRA", "SIER+SUB", "CUADRADA", "PULSO", "SIER+CUA", "RUIDO"};

static int32_t va_osc(bm_voice *v, int timbre, uint32_t det)
{
    uint32_t i1 = v->inc, i2 = v->inc + (uint32_t)(((uint64_t)v->inc * det) >> 16);
    int32_t s;
    switch (timbre % BM_VA_TIMBRES) {
    case 1:  s = (osc_saw(v->phase, i1) + osc_saw(v->ph[0], i2)) * 3 / 8 + osc_pulse(v->ph[1], i1 >> 1, 0x80000000u) / 4; break;
    case 2:  s = (osc_pulse(v->phase, i1, 0x80000000u) + osc_pulse(v->ph[0], i2, 0x80000000u)) / 2; break;
    case 3:  s = (osc_pulse(v->phase, i1, 0x40000000u) + osc_pulse(v->ph[0], i2, 0x40000000u)) / 2; break;
    case 4:  s = (osc_saw(v->phase, i1) + osc_pulse(v->ph[0], i2, 0x80000000u)) / 2; break;
    case 5:  s = (osc_saw(v->phase, i1) + osc_saw(v->ph[0], i2)) * 3 / 8 + ((int32_t)(bm_xorshift32(&v->pos) >> 17) - 16384) / 2; break;
    default: s = (osc_saw(v->phase, i1) + osc_saw(v->ph[0], i2)) / 2; break;
    }
    v->ph[0] += i2;
    v->ph[1] += i1 >> 1;
    return s;
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

/* ------------------------------------------------------------ kit SINTE: batería calculada en vivo
 * Mismos 16 instrumentos y en el mismo orden que los kits de la ROM. */
enum { SD_KICK, SD_TOM, SD_SNARE, SD_HAT, SD_CLAP, SD_RIM, SD_BELL, SD_SHAKE, SD_SINE, SD_ZAP };
typedef struct { uint8_t model; uint16_t hz; uint8_t ratio4, ptau, decay, tone; } sdrum_def;
/* ratio4: subida de tono al golpe (en cuartos) · ptau y decay: índice de bm_decay_coef */
static const sdrum_def SDRUM[BM_KIT_SOUNDS] = {
    {SD_KICK, 48, 20, 31, 92, 0},   /* KICK  */
    {SD_KICK, 56, 14, 22, 80, 1},   /* KICK2: más corto y saturado */
    {SD_HAT, 0, 0, 0, 40, 0},       /* CL HH */
    {SD_HAT, 0, 0, 0, 76, 1},       /* OP HH */
    {SD_SNARE, 185, 6, 25, 64, 0},  /* SNARE */
    {SD_CLAP, 0, 0, 0, 66, 0},      /* CLAP  */
    {SD_RIM, 1650, 0, 0, 30, 0},    /* RIM   */
    {SD_TOM, 92, 7, 38, 80, 0},     /* TOM L */
    {SD_TOM, 132, 7, 38, 76, 0},    /* TOM M */
    {SD_TOM, 185, 7, 38, 72, 0},    /* TOM H */
    {SD_BELL, 540, 0, 0, 62, 0},    /* COWBL */
    {SD_HAT, 0, 0, 0, 102, 2},      /* CYMBL */
    {SD_SHAKE, 0, 0, 0, 50, 0},     /* SHAKE */
    {SD_SINE, 2450, 0, 0, 32, 0},   /* CLAVE */
    {SD_TOM, 262, 5, 30, 60, 1},    /* CONGA */
    {SD_ZAP, 70, 120, 26, 64, 0},   /* ZAP   */
};

static uint32_t hz_inc(uint32_t hz) { return (uint32_t)(((uint64_t)hz << 32) / BM_FS); }

static void sdrum_start(bm_ctx *c, int track, bm_voice *v, int snd, int vel, const uint8_t lk[4])
{
    const bm_track *t = &c->pat->trk[track];
    const bm_lane *ln = &bm_trk_tr(c->pat, track)->lane[snd];
    const sdrum_def *d = &SDRUM[snd];
    memcpy(v->lk, lk, 4);
    v->active = 1; v->track = (uint8_t)track; v->note = (uint8_t)snd; v->sound = (uint8_t)snd; v->src = 0xFF;
    v->stage = ENV_DEC; v->gate = 0; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->choke = (snd == 2 || snd == 3) ? 1 : 0;
    v->vel = (uint8_t)bm_clampi(vel * ln->level / 100, 1, 127);
    uint32_t ratio = bm_semi_ratio[bm_clampi(24 + ((eff(v, t, 1) - 64) * 12) / 64 + ((ln->tune - 64) * 12) / 64, 0, 48)];
    v->inc = (uint32_t)(((uint64_t)hz_inc(d->hz) * ratio) >> 16);
    v->inc_target = (uint32_t)(((uint64_t)v->inc * d->ratio4) >> 2);      /* subida de tono */
    v->ph[1] = (uint32_t)(((uint64_t)hz_inc(d->model == SD_BELL ? 800 : d->model == SD_SNARE ? 330 : 5400) * ratio) >> 16);
    int dec = eff(v, t, 2) * ln->decay / 127;
    v->step = bm_decay_coef[bm_clampi(d->decay * dec / 127, 1, 127)];   /* amplitud */
    v->end = bm_decay_coef[d->ptau ? d->ptau : 1];                        /* tono */
    v->phase = 0; v->ph[0] = 0x2A000000u; v->ph[2] = 0;
    v->env = 1 << 30; v->fenv = 1 << 30; v->lp = 0;
    v->z[0] = v->z[1] = 0; v->z[2] = 1 << 30; v->z[3] = 0;
    v->pos = 0x1234567u ^ (c->age * 2654435761u);
    v->start = 0;
}

static int32_t sdrum_sample(bm_voice *v)
{
    const sdrum_def *d = &SDRUM[v->sound];
    int32_t s = 0, n;
    uint32_t inc = v->inc + (uint32_t)(((uint64_t)v->inc_target * (uint32_t)(v->fenv >> 14)) >> 16);
    switch (d->model) {
    case SD_KICK: case SD_TOM: case SD_SINE: case SD_ZAP:
        s = bm_sine[v->phase >> 24];
        v->phase += inc;
        if (d->model == SD_KICK && v->start < AT22(60)) s += ((int32_t)(bm_xorshift32(&v->pos) >> 17) - 16384);   /* el click */
        if (d->tone == 1) s = (s * 3) / 2;                      /* más saturado */
        break;
    case SD_SNARE:
        n = (int32_t)(bm_xorshift32(&v->pos) >> 16) - 32768;
        v->z[0] += ((n - v->z[0]) * 4860) >> 15;                /* lo que queda arriba del ruido */
        s = (bm_sine[v->phase >> 24] + bm_sine[v->ph[0] >> 24]) * (v->z[2] >> 15) / 65536 + (n - v->z[0]) * 3 / 4;
        v->phase += inc; v->ph[0] += v->ph[1];
        v->z[2] = (int32_t)(((int64_t)v->z[2] * bm_decay_coef[44]) >> 30);   /* el cuerpo dura menos que el ruido */
        break;
    case SD_HAT: {
        n = (int32_t)(bm_xorshift32(&v->pos) >> 16) - 32768;
        int32_t m = ((v->phase & 0x80000000u) ? 9000 : -9000) + ((v->ph[0] & 0x80000000u) ? 9000 : -9000)
                  + ((v->ph[2] & 0x80000000u) ? 7000 : -7000);
        v->phase += v->ph[1]; v->ph[0] += v->ph[1] * 3 / 2 + 77777u * 22059u / BM_FS; v->ph[2] += v->ph[1] * 7 / 5 + 33333u * 22059u / BM_FS;
        int32_t x = n / 2 + m;
        v->z[0] += ((x - v->z[0]) * 13131) >> 15;               /* agudos: x menos su pasa-bajos */
        s = x - v->z[0];
        break;
    }
    case SD_CLAP: {
        n = (int32_t)(bm_xorshift32(&v->pos) >> 16) - 32768;
        if (v->start == 0 || v->start == AT22(230) || v->start == AT22(470)) v->z[2] = 1 << 30;   /* tres palmas y la cola */
        v->z[2] = (int32_t)(((int64_t)v->z[2] * bm_decay_coef[10]) >> 30);
        v->z[0] += ((n - v->z[0]) * 3151) >> 15;
        int32_t bp = n - v->z[0];
        int32_t burst = v->start < AT22(700) ? (v->z[2] >> 15) : 0, tail = v->start >= AT22(700) ? 32768 : 0;
        s = (int32_t)(((int64_t)bp * (burst + tail)) >> 15);
        break;
    }
    case SD_RIM:
        s = bm_sine[v->phase >> 24] + (v->start < AT22(40) ? ((int32_t)(bm_xorshift32(&v->pos) >> 17) - 16384) : 0);
        v->phase += inc;
        break;
    case SD_BELL:
        s = ((v->phase & 0x80000000u) ? 12000 : -12000) + ((v->ph[0] & 0x80000000u) ? 12000 : -12000);
        v->phase += inc; v->ph[0] += v->ph[1];
        v->z[0] += ((s - v->z[0]) * 2065) >> 15;
        s -= v->z[0];
        break;
    case SD_SHAKE:
        n = (int32_t)(bm_xorshift32(&v->pos) >> 16) - 32768;
        v->z[0] += ((n - v->z[0]) * 13984) >> 15;
        s = (n - v->z[0]) * (v->start < AT22(200) ? (int32_t)v->start : (int32_t)AT22(200)) / (int32_t)AT22(200);
        break;
    default: break;
    }
    v->start++;
    v->fenv = (int32_t)(((int64_t)v->fenv * v->end) >> 30);
    return s;
}

/* ------------------------------------------------------------ batería ROMpler (T1 y canales KIT) */
static int32_t drum_at(const bm_drum_sound *d, int32_t i)
{
    if (i < 0 || (uint32_t)i >= d->len) return 0;
    return bm_ulaw_dec[d->data[i]];
}

static void drum_voice_start(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato)
{
    (void)legato;
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int snd = note % BM_KIT_SOUNDS;
    if (t->source % BM_KITS_ALL == BM_KIT_SYN) { sdrum_start(c, e->track, v, snd, vel, lk); return; }
    const bm_drum_sound *d = &bm_rom_kits[t->source % BM_KITS][snd];
    memcpy(v->lk, lk, 4);
    v->src = 0;
    v->active = 1; v->track = e->track; v->note = (uint8_t)snd; v->vel = vel;
    v->stage = ENV_DEC; v->gate = 0; v->fade = 0; v->pend = 0; v->age = ++c->age;
    v->sound = (uint8_t)snd; v->choke = d->choke;
    const bm_lane *ln = &bm_trk_tr(c->pat, e->track)->lane[snd];
    v->vel = (uint8_t)bm_clampi(vel * ln->level / 100, 1, 127);
    v->step = (uint32_t)((uint64_t)bm_semi_ratio[bm_clampi(24 + ((eff(v, t, 1) - 64) * 12) / 64 + ((ln->tune - 64) * 12) / 64, 0, 48)]
                         * BM_ROM_RATE / BM_FS);                         /* la ROM está a 22 kHz */
    v->start = (uint32_t)((uint64_t)d->len * eff(v, t, 3) / 254u);
    v->end = d->len;
    v->pos = 0; v->env = 1 << 30; v->lp = 0;
}

static void drum_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    int snd = note % BM_KIT_SOUNDS;
    int syn = t->source % BM_KITS_ALL == BM_KIT_SYN;
    const bm_drum_sound *d = &bm_rom_kits[syn ? 0 : t->source % BM_KITS][snd];
    uint8_t lk[4];
    take_locks(e, lk);
    e->legato_next = 0;
    uint8_t choke = syn ? ((snd == 2 || snd == 3) ? 1 : 0) : d->choke;
    if (choke)
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && v->choke == choke) bm_voice_fade(v);
        }
    if ((syn ? snd < 2 : d->ducks) && e->track == 0 && c->sc_trig_at < 0) c->sc_trig_at = (int16_t)c->blk_off;
    bm_voice *v = bm_voice_get(c, e->track, (uint8_t)snd, vel, lk, 0);
    if (v) drum_voice_start(e, v, (uint8_t)snd, vel, lk, 0);
}

static void drum_note_off(bm_engine *e, uint8_t note) { (void)e; (void)note; /* one-shots */ }

static bool drum_process(bm_engine *e, int32_t *buf, uint16_t frames)
{
    bm_ctx *c = e->ctx;
    const bm_track *t = &c->pat->trk[e->track];
    const bm_tr *tr = bm_trk_tr(c->pat, e->track);
    bool any = false;
    for (int vi = 0; vi < BM_VOICES; vi++) {
        bm_voice *v = &c->voice[vi];
        if (!v->active || v->track != e->track) continue;
        any = true;
        int32_t a = bm_onepole_a[eff(v, t, 0)];
        if (v->src == 0xFF) {                                    /* kit SINTE */
            for (uint16_t i = 0; i < frames && v->active; i++) {
                int32_t s = sdrum_sample(v);
                v->lp += ((s - v->lp) * a) >> 15;
                v->env = (int32_t)(((int64_t)v->env * v->step) >> 30);
                int32_t out = (int32_t)(((int64_t)v->lp * (v->env >> 14)) >> 16);
                out = out * v->vel >> 7;
                int fading = v->fade != 0;
                buf[i] += voice_fade_step(c, v, out);
                if (fading && v->fade == 0) break;
                if (v->env < (1 << 14)) { v->active = 0; v->pend = 0; }
            }
            continue;
        }
        const bm_drum_sound *d = &bm_rom_kits[t->source % BM_KITS][v->sound];
        uint32_t coef = bm_decay_coef[eff(v, t, 2) * tr->lane[v->sound].decay / 127];
        bool herm = t->interp != 0;
        for (uint16_t i = 0; i < frames && v->active; i++) {
            int32_t idx = (int32_t)(v->start + (v->pos >> 16));
            if ((uint32_t)idx >= v->end) { v->active = 0; v->pend = 0; break; }
            int32_t s = herm ? hermite(drum_at(d, idx - 1), drum_at(d, idx), drum_at(d, idx + 1), drum_at(d, idx + 2), v->pos & 0xFFFF)
                             : lerp15(drum_at(d, idx), drum_at(d, idx + 1), v->pos & 0xFFFF);
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
    int sl = note & 7, src = (note >> BRK_SRC_SH) & 3;
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
    int src = (note >> BRK_SRC_SH) & 3, sl = note & 7;
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
    int n = bm_clampi(note + (int)t->transpose - 24 + (int)c->pat->meta.transpose - 24, 0, 127);
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
    v->ph[0] = 0x55555555u; v->ph[1] = 0; v->pos = 0xACE1u + c->age;
}

static void wave_note_on(bm_engine *e, uint8_t note, uint8_t vel)
{
    bm_ctx *c = e->ctx;
    uint8_t lk[4];
    take_locks(e, lk);
    uint8_t legato = e->legato_next;
    e->legato_next = 0;
    if (e->track == 2 || c->pat->trk[e->track].engine == ENG_MONO) {     /* mono: misma voz, con glide si liga */
        for (int i = 0; i < BM_VOICES; i++) {
            bm_voice *v = &c->voice[i];
            if (v->active && v->track == e->track && v->gate && !v->fade) {
                wave_voice_start(e, v, note, vel, lk, legato);
                return;
            }
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
        bool va = t->engine == ENG_VA;
        uint32_t det = (uint32_t)eff(v, t, 3) * 9;                 /* ANALOG: hasta ~1.7 % de detune */
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
            int32_t s0 = va ? va_osc(v, t->source, det) : T0[ix] + (int32_t)(((int64_t)(T0[(ix + 1) & 255] - T0[ix]) * fr) >> 16);
            if (mf && !va) {
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

static void acid_note_off(bm_engine *e, uint8_t note) { wave_note_off(e, note); }
const bm_engine_vt bm_eng_acid = {acid_init, acid_note_on, acid_note_off, common_set_macro, acid_process, acid_voice_start};

const bm_engine_vt *bm_engine_vt_for(int engine)
{
    switch (engine) {
    case ENG_ACID: return &bm_eng_acid;
    case ENG_DRUM: case ENG_KIT: return &bm_eng_drum;
    case ENG_BREAK: return &bm_eng_sampler;
    case ENG_MIDI: return &bm_eng_midi;
    default: return &bm_eng_wave;
    }
}
