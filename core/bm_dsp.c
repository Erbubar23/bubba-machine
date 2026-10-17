/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — mezcla, sidechain, delay, reverb, punch-in FX, soft-clipper y salida
 * (SPEC_V3 §3.3, §4.3, §7). Cadena: pistas (BUNO-6 con su chorus) → delay y reverb → punch FX
 * (o el BEAT FX de HOME) → filtro del master → clipper(DRIVE) → MASTER → DAC. El pulso de Split
 * Sync no pasa por clipper ni MASTER.
 *
 * 0.4: el bus es de coma flotante a 44.1 kHz (la FPU del AC791N es 1.5× más rápida que el entero
 * de 64 bits, docs/TRAZA_FM1.md). Se mezcla canal por canal con las ganancias calculadas una vez
 * por bloque; los canales en silencio no cuestan nada. 1.0 = 32768 en la escala de los motores. */
#include <math.h>
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"
#include "bm_params.h"

enum {
    FX_STUT4, FX_STUT8, FX_STUT16, FX_STUT32, FX_REV8, FX_REV4, FX_TAPESTOP, FX_GATE,
    FX_LPF, FX_HPF, FX_BITCRUSH, FX_SRDOWN, FX_THROW, FX_FREEZE, FX_KILLLO, FX_KILLHI
};

#define K15 (1.0f / 32768.0f)
#define FX_MASK (BM_FXBUF_LEN - 1u)

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

/* ------------------------------------------------------------ reverb (plato de Dattorro a Fs/2)
 * Cuatro difusores y un tanque de dos ramas, en int16 (33 KiB). Corre a la mitad de la frecuencia,
 * como el delay: las colas no necesitan más de 11 kHz y cuesta la mitad. Largos para 22 059 Hz. */
enum { RV_D1 = 105, RV_D2 = 79, RV_D3 = 281, RV_D4 = 205,
       RV_A1 = 498, RV_L1 = 3300, RV_B1 = 1334, RV_M1 = 2757,
       RV_A2 = 673, RV_L2 = 3125, RV_B2 = 1968, RV_M2 = 2344 };
static int16_t rv_buf[RV_D1 + RV_D2 + RV_D3 + RV_D4 + RV_A1 + RV_L1 + RV_B1 + RV_M1 + RV_A2 + RV_L2 + RV_B2 + RV_M2];
typedef struct { int16_t *b; uint16_t n, i; } rv_line;
static rv_line rl[12];
static float rv_lp1, rv_lp2, rv_bw, rv_fb1, rv_fb2, rv_acc, rv_prevL, rv_prevR, rv_curL, rv_curR;
static uint8_t rv_ph, rv_inited;

static void rv_init(void)
{
    static const uint16_t N[12] = {RV_D1, RV_D2, RV_D3, RV_D4, RV_A1, RV_L1, RV_B1, RV_M1, RV_A2, RV_L2, RV_B2, RV_M2};
    int16_t *b = rv_buf;
    for (int i = 0; i < 12; i++) { rl[i].b = b; rl[i].n = N[i]; rl[i].i = 0; b += N[i]; }
    memset(rv_buf, 0, sizeof rv_buf);
    rv_lp1 = rv_lp2 = rv_bw = rv_fb1 = rv_fb2 = rv_acc = 0.0f;
    rv_prevL = rv_prevR = rv_curL = rv_curR = 0.0f;
    rv_ph = 0;
    rv_inited = 1;
}

static inline float rv_rd(const rv_line *l) { return (float)l->b[l->i] * K15; }
static inline float rv_tap(const rv_line *l, int k) { int j = (int)l->i - k; if (j < 0) j += l->n; return (float)l->b[j] * K15; }
static inline void rv_wr(rv_line *l, float x) { l->b[l->i] = to16(x); if (++l->i >= l->n) l->i = 0; }
static inline float rv_ap(rv_line *l, float x, float g)   /* pasa-todo de Schroeder */
{
    float d = rv_rd(l), v = x + g * d;
    rv_wr(l, v);
    return d - g * v;
}

/* una muestra a Fs/2: entrada mono, salida estéreo */
static void rv_step(float x, float decay, float damp, float *oL, float *oR)
{
    rv_bw += 0.75f * (x - rv_bw);                                   /* ancho de banda de la entrada */
    float d = rv_ap(&rl[0], rv_bw, 0.75f);
    d = rv_ap(&rl[1], d, 0.75f);
    d = rv_ap(&rl[2], d, 0.625f);
    d = rv_ap(&rl[3], d, 0.625f);
    float a = rv_ap(&rl[4], d + rv_fb2 * decay, -0.7f);           /* rama 1 */
    float l1 = rv_rd(&rl[5]); rv_wr(&rl[5], a);
    rv_lp1 += (1.0f - damp) * (l1 - rv_lp1);
    float b1 = rv_ap(&rl[6], rv_lp1 * decay, 0.5f);
    float m1 = rv_rd(&rl[7]); rv_wr(&rl[7], b1);
    float a2 = rv_ap(&rl[8], d + m1 * decay, -0.7f);              /* rama 2 */
    float l2 = rv_rd(&rl[9]); rv_wr(&rl[9], a2);
    rv_lp2 += (1.0f - damp) * (l2 - rv_lp2);
    float b2 = rv_ap(&rl[10], rv_lp2 * decay, 0.5f);
    float m2 = rv_rd(&rl[11]); rv_wr(&rl[11], b2);
    rv_fb1 = m1; rv_fb2 = m2;
    *oL = 0.6f * (rv_tap(&rl[9], 197) + rv_tap(&rl[9], 2200) - rv_tap(&rl[10], 1400) + rv_tap(&rl[11], 1500) - rv_tap(&rl[5], 1500) - rv_tap(&rl[6], 140));
    *oR = 0.6f * (rv_tap(&rl[5], 260) + rv_tap(&rl[5], 2000) - rv_tap(&rl[6], 1240) + rv_tap(&rl[7], 1990) - rv_tap(&rl[9], 1580) - rv_tap(&rl[10], 280));
}

/* ------------------------------------------------------------ chorus de BUNO-6 (dos tomas de un BBD)
 * Mono de entrada, estéreo de salida: el mismo retardo modulado con el LFO en fase opuesta en cada
 * lado. Velocidades y retardos de los citados para el Juno-60 (a confirmar con el manual de servicio). */
#define CH_LEN 512u
static float ch_buf[CH_LEN], ch_lfo, ch_lpL, ch_lpR;
static uint32_t ch_w;      /* siempre < CH_LEN: como float, un contador libre pierde los decimales a los 6 min (2^24) */
static const float CH_RATE[BN_CHORUSES] = {0.0f, 0.513f, 0.863f, 9.75f};
static const float CH_MIN_MS[BN_CHORUSES] = {0.0f, 1.66f, 1.66f, 3.30f};
static const float CH_MAX_MS[BN_CHORUSES] = {0.0f, 5.35f, 5.35f, 3.70f};

static inline float ch_read(float dly)
{
    float rp = (float)ch_w - dly;
    if (rp < 0.0f) rp += (float)CH_LEN;
    uint32_t i0 = (uint32_t)rp;
    float f = rp - (float)i0;
    float a = ch_buf[i0 & (CH_LEN - 1u)], b = ch_buf[(i0 + 1u) & (CH_LEN - 1u)];
    return a + (b - a) * f;
}

/* la reverb y el chorus viven fuera del contexto: vuelven a cero con el audio */
void bm_dsp_reset(void)
{
    rv_init();
    memset(ch_buf, 0, sizeof ch_buf);
    ch_lfo = ch_lpL = ch_lpR = 0.0f; ch_w = 0;
}

void bm_dsp_tick(bm_ctx *c)
{
    /* punch-in: cuantizado a 1/16 si está sonando. Sin capa de PUNCH, el BEAT FX de HOME. */
    uint8_t want = c->punch_req;
    if (want == 0xFF && c->pat->meta.beatfx) want = (uint8_t)(c->pat->meta.beatfx - 1);
    if (want != c->punch_cur && (!c->playing || (c->btick % BM_TICKS_STEP) == 0)) {
        c->punch_cur = want;
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

/* Filtro DJ de un canal (K2 de HOME): djf 0 o 64 abierto, 1-63 cierra un pasa-bajos (63 = lo más
 * cerrado), 65-127 abre un pasa-altos. El mismo SVF que el master; el estado vuelve a cero abierto. */
static void dj_filter(bm_ctx *c, int t, uint8_t djf, int32_t *x, uint32_t n)
{
    if (!djf || djf == 64) { c->dj_ic1[t] = c->dj_ic2[t] = 0.0f; return; }
    int hp = djf > 64;
    int ci = hp ? 12 + (djf - 64) * 100 / 63 : 127 - djf * 112 / 63;
    float gq = (float)bm_svf_g[bm_clampi(ci, 0, 127)] * (1.0f / 65536.0f), kq = 1.1f;
    float a1 = 1.0f / (1.0f + gq * (gq + kq)), a2 = gq * a1, a3 = gq * a2;
    for (uint32_t i = 0; i < n; i++) x[i] = (int32_t)svf_run(&c->dj_ic1[t], &c->dj_ic2[t], (float)x[i], a1, a2, a3, kq, hp);
    c->dj_ic1[t] = flush(c->dj_ic1[t]); c->dj_ic2[t] = flush(c->dj_ic2[t]);
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
    static float RV[BM_BLOCK];
    for (uint32_t i = 0; i < n; i++) L[i] = R[i] = S[i] = RV[i] = 0.0f;
    int rv_any = 0;
    for (int t = 0; t < BM_TRACKS; t++) {
        if (!(c->tact & (1u << t))) continue;
        const bm_track *tr = &c->pat->trk[t];
        int32_t *x = c->tbuf[t];
        dj_filter(c, t, tr->djf, x, n);
        float g = (float)tr->level * (1.0f / 400.0f) * K15;      /* nivel 100 = -12 dB de headroom en el bus */
        float gl = g * (float)bm_pan_l[tr->pan & 127] * K15;
        float gr = g * (float)bm_pan_r[tr->pan & 127] * K15;
        float gs = g * (float)tr->send * (1.0f / 128.0f);
        float rs = (float)tr->rsend * (1.0f / 128.0f);
        if (tr->engine == ENG_BITTAL) rs += bm_bt_macro(c->pat->prm[t], tr->macro, BTM_SPACE) * 0.8f;   /* macro SPACE */
        float grv = g * (rs < 0.0f ? 0.0f : rs);
        if (grv > 0.0f) {
            rv_any = 1;
            for (uint32_t i = 0; i < n; i++) RV[i] += (float)x[i] * grv;
        }
        int32_t pk = 0;
        int chm = tr->engine == ENG_BUNO ? c->pat->prm[t][BN_CHORUS] % BN_CHORUSES : 0;
        if (chm) {                                               /* BUNO-6: chorus estéreo */
            float rate = CH_RATE[chm] / (float)BM_FS;
            float mn = CH_MIN_MS[chm] * 0.001f * (float)BM_FS, mx = CH_MAX_MS[chm] * 0.001f * (float)BM_FS;
            float mid = 0.5f * (mn + mx), dep = 0.5f * (mx - mn);
            const float kl = 0.55f;                                 /* el BBD filtra los agudos */
            for (uint32_t i = 0; i < n; i++) {
                int32_t xi = x[i], ax = xi < 0 ? -xi : xi;
                if (ax > pk) pk = ax;
                float v = (float)xi;
                ch_buf[ch_w] = v;
                ch_lfo += rate; if (ch_lfo >= 1.0f) ch_lfo -= 1.0f;
                float tri = 4.0f * fabsf(ch_lfo - 0.5f) - 1.0f;
                float wl = ch_read(mid + dep * tri), wr = ch_read(mid - dep * tri);
                ch_lpL += kl * (wl - ch_lpL); ch_lpR += kl * (wr - ch_lpR);
                ch_w = (ch_w + 1u) & (CH_LEN - 1u);
                L[i] += (v + ch_lpL) * gl * 0.75f; R[i] += (v + ch_lpR) * gr * 0.75f; S[i] += v * gs;
            }
            peak[t] = (int32_t)((int64_t)pk * tr->level / 400);
            continue;
        }
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

    /* ---- MOVE a la derecha: la mezcla entera también entra a la reverb (se lava) */
    float mv = (float)c->move * (1.0f / 63.0f);
    if (mv > 0.0f) {
        float k = mv * 0.35f;
        for (uint32_t i = 0; i < n; i++) RV[i] += (L[i] + R[i]) * k;
        rv_any = 1;
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
        if (mv < 0.0f) {                                         /* MOVE a la izquierda: la mezcla al eco, más repeticiones */
            thr += -mv * 0.5f;
            fb += -mv * 0.35f; if (fb > 0.9f) fb = 0.9f;
            if (mix < -mv * 0.6f) mix = -mv * 0.6f;
        }
        for (uint32_t i = 0; i < n; i++) {
            c->d_acc += in_g * (S[i] + thr * (L[i] + R[i]));
            float dv;
            if (c->fx_w & 1u) {
                uint32_t rd = c->dly_w >= dd ? c->dly_w - dd : c->dly_w + BM_DELAY_LEN - dd;   /* el búfer no es potencia de 2 */
                c->d_prev = c->d_cur;
                c->d_cur = (float)dly[rd] * K15;
                dly[c->dly_w] = to16(c->d_acc * 0.5f + c->d_cur * fb);
                if (++c->dly_w >= BM_DELAY_LEN) c->dly_w = 0;
                c->d_acc = 0.0f;
                dv = 0.5f * (c->d_prev + c->d_cur);
            } else dv = c->d_cur;
            L[i] += dv * mix; R[i] += dv * mix;
            fxb[c->fx_w & FX_MASK] = to16((L[i] + R[i]) * 0.5f);   /* captura para stutter/reverse/tape */
            c->fx_w++;
        }
    }

    /* ---- reverb (a Fs/2: suma de dos muestras, sale con interpolación lineal) */
    {
        if (!rv_inited) rv_init();
        const bm_meta *mm = &c->pat->meta;
        float mix = (float)mm->rv_mix * (1.0f / 127.0f);
        if (mv > 0.0f && mix < mv * 0.7f) mix = mv * 0.7f;
        if (rv_any || rv_fb1 != 0.0f || rv_fb2 != 0.0f || rv_curL != 0.0f) {
            float decay = 0.45f + 0.5f * (float)mm->rv_size * (1.0f / 127.0f);
            float damp = 0.05f + 0.85f * (float)mm->rv_damp * (1.0f / 127.0f);
            for (uint32_t i = 0; i < n; i++) {
                rv_acc += RV[i];
                if (rv_ph) {
                    rv_prevL = rv_curL; rv_prevR = rv_curR;
                    rv_step(rv_acc * 0.5f, decay, damp, &rv_curL, &rv_curR);
                    rv_acc = 0.0f;
                    L[i] += 0.5f * (rv_prevL + rv_curL) * mix; R[i] += 0.5f * (rv_prevR + rv_curR) * mix;
                } else { L[i] += rv_curL * mix; R[i] += rv_curR * mix; }
                rv_ph ^= 1u;
            }
            rv_fb1 = flush(rv_fb1); rv_fb2 = flush(rv_fb2); rv_lp1 = flush(rv_lp1); rv_lp2 = flush(rv_lp2);
            if (!rv_any && fabsf(rv_curL) < 1e-6f && fabsf(rv_curR) < 1e-6f) rv_curL = rv_curR = rv_fb1 = rv_fb2 = 0.0f;
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

    /* ---- filtro del master (K3 de HOME): 64 abierto, a la izquierda pasa-bajos, a la derecha pasa-altos */
    {
        int mf = c->pat->meta.mfilt;
        if (c->move < 0) mf = bm_clampi(60 + c->move, 4, 59);        /* MOVE: cierra el pasa-bajos */
        else if (c->move > 0) mf = bm_clampi(68 + c->move * 3 / 4, 69, 127);   /* abre el pasa-altos */
        if (mf < 60 || mf > 68) {
            int hp = mf > 68;
            int ci = hp ? (mf - 68) * 70 / 59 + 20 : 127 - (60 - mf) * 85 / 60;
            float gq = (float)bm_svf_g[bm_clampi(ci, 0, 127)] * (1.0f / 65536.0f), kq = 1.2f;
            float a1 = 1.0f / (1.0f + gq * (gq + kq)), a2 = gq * a1, a3 = gq * a2;
            for (uint32_t i = 0; i < n; i++) {
                L[i] = svf_run(&c->mf_ic1[0], &c->mf_ic2[0], L[i], a1, a2, a3, kq, hp);
                R[i] = svf_run(&c->mf_ic1[1], &c->mf_ic2[1], R[i], a1, a2, a3, kq, hp);
            }
            for (int ch = 0; ch < 2; ch++) { c->mf_ic1[ch] = flush(c->mf_ic1[ch]); c->mf_ic2[ch] = flush(c->mf_ic2[ch]); }
        } else c->mf_ic1[0] = c->mf_ic1[1] = c->mf_ic2[0] = c->mf_ic2[1] = 0.0f;
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
