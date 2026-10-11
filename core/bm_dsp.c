/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — mezcla, sidechain, delay, punch-in FX, soft-clipper y salida
 * (SPEC_V3 §3.3, §4.3, §7). Cadena: pistas → delay → punch FX → clipper(DRIVE)
 * → MASTER → DAC. El pulso de Split Sync no pasa por clipper ni MASTER.
 *
 * 0.4: el bus es de coma flotante a 44.1 kHz (la FPU del AC791N es 1.5× más rápida que el entero
 * de 64 bits, docs/TRAZA_FM1.md). Se mezcla canal por canal con las ganancias calculadas una vez
 * por bloque; los canales en silencio no cuestan nada. 1.0 = 32768 en la escala de los motores. */
#include <math.h>
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"

enum {
    FX_STUT4, FX_STUT8, FX_STUT16, FX_STUT32, FX_REV8, FX_REV4, FX_TAPESTOP, FX_GATE,
    FX_LPF, FX_HPF, FX_BITCRUSH, FX_SRDOWN, FX_THROW, FX_FREEZE, FX_KILLLO, FX_KILLHI
};

#define K15 (1.0f / 32768.0f)
#define FX_MASK (BM_FXBUF_LEN - 1u)
#define DLY_MASK (BM_DELAY_LEN - 1u)

static inline int16_t to16(float x)                 /* 1.0 -> 32767, con saturación y redondeo */
{
    x *= 32767.0f;
    if (x >= 32767.0f) return 32767;
    if (x <= -32768.0f) return -32768;
    return (int16_t)(int32_t)(x + (x >= 0.0f ? 0.5f : -0.5f));
}

static inline float flush(float x) { return (x > -1e-20f && x < 1e-20f) ? 0.0f : x; }

/* SVF (Zavalishin/Simper, trapezoidal) en float: devuelve LP (hp=0) o HP (hp=1). */
static inline float svf_run(float *ic1, float *ic2, float x, float a1, float a2, float a3, float k, int hp)
{
    float v3 = x - *ic2;
    float v1 = a1 * *ic1 + a2 * v3;
    float v2 = *ic2 + a2 * *ic1 + a3 * v3;
    *ic1 = 2.0f * v1 - *ic1;
    *ic2 = 2.0f * v2 - *ic2;
    return hp ? x - k * v1 - v2 : v2;
}

/* Pasa-bajos de la grabación del sampler (biquad Butterworth a 7 kHz, antes de diezmar a 16 kHz). */
static float rb0, rb1, rb2, ra1, ra2;

static void rec_lp_init(void)
{
    if (rb0 != 0.0f) return;
    double w = 2.0 * 3.14159265358979 * 7000.0 / BM_FS, cw = cos(w), al = sin(w) / (2.0 * 0.7071);
    double a0 = 1.0 + al;
    rb0 = (float)((1.0 - cw) / 2.0 / a0); rb1 = (float)((1.0 - cw) / a0); rb2 = rb0;
    ra1 = (float)(-2.0 * cw / a0); ra2 = (float)((1.0 - al) / a0);
}

void bm_dsp_tick(bm_ctx *c)
{
    /* punch-in: cuantizado a 1/16 si está sonando */
    if (c->punch_req != c->punch_cur && (!c->playing || (c->btick % BM_TICKS_STEP) == 0)) {
        c->punch_cur = c->punch_req;
        c->punch_t = 0;
        c->punch_w0 = c->fx_w;
        c->punch_rp = (c->fx_w & (BM_FXBUF_LEN - 1)) << 8;
        if (c->arena_busy && c->punch_cur <= FX_TAPESTOP) c->punch_cur = 0xFF;   /* sin buffer mientras hay sample sin guardar */
        c->pf_ic1[0] = c->pf_ic1[1] = c->pf_ic2[0] = c->pf_ic2[1] = 0.0f;
        c->st.punch = c->punch_cur;
    }
    /* Split Sync: pulsos a ppqn configurable */
    if (c->sync_mode == 1 && c->playing) {
        uint8_t ppqn = c->sync_ppqn ? c->sync_ppqn : 2;
        if (c->btick % (BM_PPQN / ppqn) == 0) {
            c->sync_wait = (uint32_t)c->sync_lat * BM_FS / 1000u;
            c->sync_left = BM_FS * 5u / 1000u;
        }
    }
    /* MIDI clock de salida (24 PPQN) en modo interno */
    if (!c->ext_clock && c->midi_out && (c->st.free_ticks % 4u) == 0)
        c->midi_out(c->midi_user, 0xF8);
}

static uint32_t fx_len(const bm_ctx *c, uint32_t ticks)
{
    uint32_t n = bm_ticks_to_samples(c, ticks);
    if (n >= BM_FXBUF_LEN) {     /* no cabe: se usa la mitad (1/4 -> 1/8 a tempos lentos) */
        while (n >= BM_FXBUF_LEN) n >>= 1;
    }
    return n ? n : 1;
}

void bm_dsp_mix(bm_ctx *c, int32_t *out, uint32_t n)
{
    static float L[BM_BLOCK], R[BM_BLOCK], S[BM_BLOCK], SC[BM_BLOCK];
    const bm_meta *m = &c->pat->meta;
    int pc = c->punch_cur == 0xFF ? -1 : c->punch_cur;
    int32_t peak[BM_TRACKS] = {0};
    if (n > BM_BLOCK) n = BM_BLOCK;

    /* ---- sidechain: la envolvente del bombo, solo si hace falta */
    int duck_any = 0;
    for (int t = 1; t < BM_TRACKS; t++) if (c->pat->trk[t].duck && (c->tact & (1u << t))) duck_any = 1;
    {
        float e = c->sc_env, k = (float)bm_decay_coef[m->sc_release / 2] * (1.0f / 1073741824.0f);
        int trig = c->sc_trig_at;
        if (duck_any || trig >= 0 || e > 0.0f) {
            for (uint32_t i = 0; i < n; i++) {
                if ((int)i == trig) e = 1.0f;
                SC[i] = e;
                e *= k;
            }
            c->sc_env = e < 1e-6f ? 0.0f : e;
        }
    }

    /* ---- canales: ganancias por bloque, un recorrido por canal que sonó */
    for (uint32_t i = 0; i < n; i++) L[i] = R[i] = S[i] = 0.0f;
    for (int t = 0; t < BM_TRACKS; t++) {
        if (!(c->tact & (1u << t))) continue;
        const bm_track *tr = &c->pat->trk[t];
        const int32_t *x = c->tbuf[t];
        float g = (float)tr->level * (1.0f / 400.0f) * K15;      /* nivel 100 = -12 dB de headroom en el bus */
        float gl = g * (float)bm_pan_l[tr->pan & 127] * K15;
        float gr = g * (float)bm_pan_r[tr->pan & 127] * K15;
        float gs = g * (float)tr->send * (1.0f / 128.0f);
        int32_t pk = 0;
        if (t >= 1 && tr->duck) {
            float dk = (float)tr->duck * (1.0f / 127.0f);
            for (uint32_t i = 0; i < n; i++) {
                int32_t xi = x[i], ax = xi < 0 ? -xi : xi;
                if (ax > pk) pk = ax;
                float v = (float)xi * (1.0f - SC[i] * dk);
                L[i] += v * gl; R[i] += v * gr; S[i] += v * gs;
            }
        } else {
            for (uint32_t i = 0; i < n; i++) {
                int32_t xi = x[i], ax = xi < 0 ? -xi : xi;
                if (ax > pk) pk = ax;
                float v = (float)xi;
                L[i] += v * gl; R[i] += v * gr; S[i] += v * gs;
            }
        }
        peak[t] = (int32_t)((int64_t)pk * tr->level / 400);
    }

    /* ---- delay a Fs/2 (escribe la suma de dos muestras, lee con interpolación lineal) y captura del punch */
    int16_t *fxb = c->mem->fx.fx;
    if (!(c->arena_busy || c->smp_rec)) {
        int16_t *dly = c->mem->fx.dly;
        static const uint8_t DT[4] = {24, 48, 72, 96};
        uint32_t dd = bm_ticks_to_samples(c, DT[m->dly_time & 3]) / 2u;
        if (dd > BM_DELAY_LEN - 2) dd = BM_DELAY_LEN - 2;
        if (dd < 1) dd = 1;
        float fb = (float)m->dly_fb * (0.9f / 127.0f);
        if (pc == FX_FREEZE) fb = 1.0f;
        if (pc == FX_THROW && fb < 0.75f) fb = 0.75f;
        float in_g = pc == FX_FREEZE ? 0.0f : 1.0f, thr = pc == FX_THROW ? 0.5f : 0.0f;
        float mix = (float)m->dly_mix * (1.0f / 128.0f);
        for (uint32_t i = 0; i < n; i++) {
            c->d_acc += in_g * (S[i] + thr * (L[i] + R[i]));
            float dv;
            if (c->fx_w & 1u) {
                uint32_t rd = (c->dly_w - dd) & DLY_MASK;
                c->d_prev = c->d_cur;
                c->d_cur = (float)dly[rd] * K15;
                dly[c->dly_w] = to16(c->d_acc * 0.5f + c->d_cur * fb);
                c->dly_w = (c->dly_w + 1u) & DLY_MASK;
                c->d_acc = 0.0f;
                dv = 0.5f * (c->d_prev + c->d_cur);
            } else dv = c->d_cur;
            L[i] += dv * mix; R[i] += dv * mix;
            fxb[c->fx_w & FX_MASK] = to16((L[i] + R[i]) * 0.5f);   /* captura para stutter/reverse/tape */
            c->fx_w++;
        }
    }

    /* ---- punch-in FX */
    if (pc >= 0) {
        float a1 = 0, a2 = 0, a3 = 0, kq = 1.4f;
        int fhp = 0, fon = 0;
        if (pc == FX_LPF || pc == FX_HPF || pc == FX_KILLLO || pc == FX_KILLHI) {
            uint32_t bar = bm_ticks_to_samples(c, 384);
            int prog = (int)((uint64_t)c->punch_t * 110u / (bar ? bar : 1));
            if (prog > 110) prog = 110;
            int ci = pc == FX_LPF ? 127 - prog : pc == FX_HPF ? prog : pc == FX_KILLLO ? 46 : 58;
            float g = (float)bm_svf_g[bm_clampi(ci, 0, 127)] * (1.0f / 65536.0f);
            a1 = 1.0f / (1.0f + g * (g + kq)); a2 = g * a1; a3 = g * a2;
            fhp = (pc == FX_HPF || pc == FX_KILLLO);
            fon = 1;
        }
        static const uint8_t TK[4] = {96, 48, 24, 12};
        uint32_t len = pc <= FX_STUT32 ? fx_len(c, TK[pc]) : (pc == FX_REV8 || pc == FX_REV4) ? fx_len(c, pc == FX_REV8 ? 48 : 96) : 1;
        uint32_t T = pc == FX_TAPESTOP ? bm_ticks_to_samples(c, 96) : 1;
        float gt = ((c->st.free_ticks % 24u) < 12u) ? 1.0f : 0.0f;
        for (uint32_t i = 0; i < n; i++) {
            uint32_t t = c->punch_t++;
            switch (pc) {
            case FX_STUT4: case FX_STUT8: case FX_STUT16: case FX_STUT32:
                L[i] = R[i] = (float)fxb[(c->punch_w0 + t % len) & FX_MASK] * K15;
                break;
            case FX_REV8: case FX_REV4:
                L[i] = R[i] = (float)fxb[(c->punch_w0 + BM_FXBUF_LEN * 4u - 1u - t % len) & FX_MASK] * K15;
                break;
            case FX_TAPESTOP: {
                int32_t rate = T ? 256 - (int32_t)((uint64_t)t * 256u / T) : 0;   /* Q8 */
                if (rate <= 0) { L[i] = R[i] = 0.0f; break; }
                c->punch_rp += (uint32_t)rate;
                uint32_t k = c->punch_rp >> 8;
                float f = (float)(c->punch_rp & 255u) * (1.0f / 256.0f);
                float s0 = (float)fxb[k & FX_MASK], s1 = (float)fxb[(k + 1u) & FX_MASK];
                L[i] = R[i] = (s0 + (s1 - s0) * f) * K15;
                break;
            }
            case FX_GATE:
                c->gate_g += (gt - c->gate_g) * (1.0f / 64.0f);
                L[i] *= c->gate_g; R[i] *= c->gate_g;
                break;
            case FX_BITCRUSH:                                 /* 5 bits, a propósito */
                L[i] = (float)(int32_t)(L[i] * 16.0f) * (1.0f / 16.0f);
                R[i] = (float)(int32_t)(R[i] * 16.0f) * (1.0f / 16.0f);
                break;
            case FX_SRDOWN:                                   /* ≈ 3.7 kHz, a propósito */
                if (t % 12u == 0) { c->sh_l = L[i]; c->sh_r = R[i]; }
                L[i] = c->sh_l; R[i] = c->sh_r;
                break;
            default: break;
            }
            if (fon) {
                L[i] = svf_run(&c->pf_ic1[0], &c->pf_ic2[0], L[i], a1, a2, a3, kq, fhp);
                R[i] = svf_run(&c->pf_ic1[1], &c->pf_ic2[1], R[i], a1, a2, a3, kq, fhp);
            }
        }
        for (int ch = 0; ch < 2; ch++) { c->pf_ic1[ch] = flush(c->pf_ic1[ch]); c->pf_ic2[ch] = flush(c->pf_ic2[ch]); }
    }

    /* ---- sampler: la mezcla (post-FX, pre-master), filtrada y diezmada a 16 kHz, 8 bit */
    if (c->smp_rec) {
        rec_lp_init();
        float *z = c->smp_z;
        for (uint32_t i = 0; i < n && c->smp_rec; i++) {
            float x = (L[i] + R[i]) * 0.5f;
            float y = rb0 * x + rb1 * z[0] + rb2 * z[1] - ra1 * z[2] - ra2 * z[3];
            z[1] = z[0]; z[0] = x; z[3] = z[2]; z[2] = y;
            c->smp_dec_acc += BM_SMP_RATE;
            if (c->smp_dec_acc >= BM_FS) {
                c->smp_dec_acc -= BM_FS;
                uint32_t cap = c->smp_cap && c->smp_cap < BM_SMP_REC_MAX ? c->smp_cap : BM_SMP_REC_MAX;
                if (c->smp_rec_pos < cap) {
                    float v = y * 128.0f;
                    int32_t q = (int32_t)(v + (v >= 0.0f ? 0.5f : -0.5f));
                    c->mem->arena[c->smp_rec_pos++] = (uint8_t)(int8_t)(q > 127 ? 127 : (q < -128 ? -128 : q));
                } else bm_smp_rec_stop(c);                    /* tope: se para sola */
            }
        }
        z[2] = flush(z[2]); z[3] = flush(z[3]);
    }

    /* ---- soft-clipper con DRIVE, MASTER (rampa por bloque) y fade de escritura de flash */
    uint32_t mt = (uint32_t)c->master_target * c->master_target >> 5;   /* curva cuadrática, Q15 */
    if (mt > 32767) mt = 32767;
    float mg = (float)c->master_gain * K15, dmg = ((float)mt * K15 - mg) / (float)(n ? n : 1);
    float drv = 1.0f + (float)c->drive * (7.0f / 127.0f);
    const float FADE = 1.0f / (0.005f * (float)BM_FS);                  /* 5 ms */
    const float CEIL = 29000.0f / 32768.0f * 8388352.0f;                /* techo ≈ -1 dBFS, a 24 bit */
    for (uint32_t i = 0; i < n; i++) {
        float y[2], in[2] = {L[i] * drv, R[i] * drv};
        for (int ch = 0; ch < 2; ch++) {
            float u = in[ch];
            if (u >= 1.0f) { y[ch] = 1.0f; c->clip_hold = 1; }
            else if (u <= -1.0f) { y[ch] = -1.0f; c->clip_hold = 1; }
            else y[ch] = u * (1.5f - 0.5f * u * u);
        }
        if (c->fade_req) { c->fade_gain -= FADE; if (c->fade_gain < 0.0f) c->fade_gain = 0.0f; }
        else { c->fade_gain += FADE; if (c->fade_gain > 1.0f) c->fade_gain = 1.0f; }
        float g = mg * c->fade_gain * CEIL;
        mg += dmg;
        int32_t oL = (int32_t)(y[0] * g), oR = (int32_t)(y[1] * g);
        if (c->sync_mode == 1) {                                 /* L = pulso, R = mezcla mono */
            oR = (oL + oR) / 2;
            int32_t p = 0;
            if (c->sync_wait) c->sync_wait--;
            else if (c->sync_left) { c->sync_left--; p = 7549747; }
            oL = p;
        }
        out[2 * i + 0] = oL; out[2 * i + 1] = oR;
    }
    c->master_gain = (uint16_t)mt;
    c->st.faded = (c->fade_req && c->fade_gain == 0.0f) ? 1 : 0;
    if (c->clip_hold) { c->st.clip = 1; c->clip_hold = 0; }
    for (int t = 0; t < BM_TRACKS; t++) {
        uint32_t p = (uint32_t)peak[t] >> 7;
        if (p > 255) p = 255;
        uint32_t old = c->peak_acc[t];
        c->peak_acc[t] = p > old ? p : (old * 15) / 16;
        c->st.peak[t] = (uint8_t)c->peak_acc[t];
    }
    if (c->smp_rec) c->st.smp_len = c->smp_rec_pos;
}
