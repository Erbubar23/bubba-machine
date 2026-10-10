/* Bubba Machine — mezcla, sidechain, delay, punch-in FX, soft-clipper y salida
 * (SPEC_V3 §3.3, §4.3, §7). Cadena: pistas → delay → punch FX → clipper(DRIVE)
 * → MASTER → DAC. El pulso de Split Sync no pasa por clipper ni MASTER. */
#include <math.h>
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"

enum {
    FX_STUT4, FX_STUT8, FX_STUT16, FX_STUT32, FX_REV8, FX_REV4, FX_TAPESTOP, FX_GATE,
    FX_LPF, FX_HPF, FX_BITCRUSH, FX_SRDOWN, FX_THROW, FX_FREEZE, FX_KILLLO, FX_KILLHI
};

static uint8_t ulaw_enc_tab[1024];
static bool ulaw_ready;

static void ulaw_init(void)
{
    for (int i = 0; i < 1024; i++) {
        double x = (i * 32.0 + 16.0) / 32767.0;
        if (x > 1.0) x = 1.0;
        int q = (int)floor(127.0 * log1p(255.0 * x) / log1p(255.0) + 0.5);
        ulaw_enc_tab[i] = (uint8_t)(q > 127 ? 127 : q);
    }
    ulaw_ready = true;
}

extern const int16_t bm_ulaw_dec[256];

static inline uint8_t ulaw_enc(int32_t x)
{
    int neg = x < 0;
    int32_t m = neg ? -x : x;
    if (m > 32767) m = 32767;
    uint8_t q = ulaw_enc_tab[m >> 5];
    return neg ? (uint8_t)(-(int)q & 0xFF) : q;
}

static inline int16_t sat16(int32_t x) { return (int16_t)(x > 32767 ? 32767 : (x < -32768 ? -32768 : x)); }

static void svf_coefs(int ci, int64_t k, int64_t *a1, int64_t *a2, int64_t *a3)
{
    int64_t g = bm_svf_g[bm_clampi(ci, 0, 127)];
    int64_t den = 65536 + ((g * (g + k)) >> 16);
    *a1 = ((int64_t)1 << 32) / den;
    *a2 = (g * *a1) >> 16;
    *a3 = (g * *a2) >> 16;
}

/* SVF maestro de un canal: devuelve LP (hp=0) o HP (hp=1). */
static int32_t svf_run(int32_t *ic1, int32_t *ic2, int32_t in, int64_t a1, int64_t a2, int64_t a3, int64_t k, int hp)
{
    int64_t x = (int64_t)in << 2;
    int64_t v3 = x - *ic2;
    int64_t v1 = (a1 * *ic1 + a2 * v3) >> 16;
    int64_t v2 = *ic2 + ((a2 * *ic1 + a3 * v3) >> 16);
    *ic1 = (int32_t)(2 * v1 - *ic1);
    *ic2 = (int32_t)(2 * v2 - *ic2);
    int64_t y = hp ? (x - ((k * v1) >> 16) - v2) : v2;
    return (int32_t)(y >> 2);
}

void bm_dsp_tick(bm_ctx *c)
{
    if (!ulaw_ready) ulaw_init();
    /* punch-in: cuantizado a 1/16 si está sonando */
    if (c->punch_req != c->punch_cur && (!c->playing || (c->tick % BM_TICKS_STEP) == 0)) {
        c->punch_cur = c->punch_req;
        c->punch_t = 0;
        c->punch_w0 = c->fx_w;
        c->punch_rp = (c->fx_w % BM_FXBUF_LEN) << 8;
        c->pf_ic1[0] = c->pf_ic1[1] = c->pf_ic2[0] = c->pf_ic2[1] = 0;
        c->st.punch = c->punch_cur;
    }
    /* Split Sync: pulsos a ppqn configurable */
    if (c->sync_mode == 1 && c->playing) {
        uint8_t ppqn = c->sync_ppqn ? c->sync_ppqn : 2;
        if (c->tick % (BM_PPQN / ppqn) == 0) {
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
    const bm_meta *m = &c->pat->meta;
    uint32_t sc_coef = bm_decay_coef[m->sc_release / 2] >> 15;     /* Q15 */
    static const uint8_t DT[4] = {24, 48, 72, 96};
    uint32_t dd = bm_ticks_to_samples(c, DT[m->dly_time & 3]) / 2u;
    if (dd > BM_DELAY_LEN - 2) dd = BM_DELAY_LEN - 2;
    if (dd < 1) dd = 1;
    int32_t fb = m->dly_fb * 115 / 127;                 /* Q7, máx ~0.9 */
    int pc = c->punch_cur == 0xFF ? -1 : c->punch_cur;
    if (pc == FX_FREEZE) fb = 128;
    if (pc == FX_THROW && fb < 96) fb = 96;
    int32_t peak[BM_TRACKS] = {0};

    /* master: rampa por bloque hacia el pot */
    uint32_t mt = (uint32_t)c->master_target * c->master_target >> 5;   /* curva cuadrática, Q15 */
    if (mt > 32767) mt = 32767;
    int32_t mg0 = c->master_gain, mg1 = (int32_t)mt;
    int32_t drive_q8 = 256 + c->drive * 1792 / 127;

    /* coeficientes del SVF maestro para los FX de filtro */
    int64_t a1 = 0, a2 = 0, a3 = 0, kq = 91750;   /* k ≈ 1.4 */
    int fhp = 0, fon = 0;
    if (pc == FX_LPF || pc == FX_HPF || pc == FX_KILLLO || pc == FX_KILLHI) {
        uint32_t bar = bm_ticks_to_samples(c, 384);
        int prog = (int)((uint64_t)c->punch_t * 110u / (bar ? bar : 1));
        if (prog > 110) prog = 110;
        int ci = pc == FX_LPF ? 127 - prog : pc == FX_HPF ? prog : pc == FX_KILLLO ? 46 : 58;
        fhp = (pc == FX_HPF || pc == FX_KILLLO);
        svf_coefs(ci, kq, &a1, &a2, &a3);
        fon = 1;
    }

    for (uint32_t i = 0; i < n; i++) {
        if (c->sc_trig_at >= 0 && (uint32_t)c->sc_trig_at == i) c->sc_env = 32767;
        int32_t sc = c->sc_env;
        c->sc_env = (int32_t)(((int64_t)sc * sc_coef) >> 15);

        int32_t L = 0, R = 0, send = 0;
        for (int t = 0; t < BM_TRACKS; t++) {
            const bm_track *tr = &c->pat->trk[t];
            int32_t x = c->tbuf[t][i];
            x = x * tr->level / 400;            /* nivel 100 = -12 dB de headroom en el bus */
            if (t >= 2 && tr->duck) x = (int32_t)(((int64_t)x * (32767 - sc * tr->duck / 127)) >> 15);
            int32_t ax = x < 0 ? -x : x;
            if (ax > peak[t]) peak[t] = ax;
            L += (x * bm_pan_l[tr->pan & 127]) >> 15;
            R += (x * bm_pan_r[tr->pan & 127]) >> 15;
            send += (x * tr->send) >> 7;
        }

        /* delay a Fs/2: escribe en muestras pares, mantiene en impares */
        c->d_acc += (pc == FX_FREEZE) ? 0 : send + ((pc == FX_THROW) ? (L + R) / 2 : 0);
        if ((c->sample_clock + i) & 1) {
            uint32_t rd = (c->dly_w - dd) & (BM_DELAY_LEN - 1);
            c->d_out = c->dly[rd];
            c->dly[c->dly_w] = sat16(c->d_acc / 2 + ((c->d_out * fb) >> 7));
            c->dly_w = (c->dly_w + 1) & (BM_DELAY_LEN - 1);
            c->d_acc = 0;
        }
        L += (c->d_out * m->dly_mix) >> 7;
        R += (c->d_out * m->dly_mix) >> 7;

        /* captura para stutter/reverse/tape */
        c->fxb[c->fx_w % BM_FXBUF_LEN] = ulaw_enc((L + R) / 2);
        c->fx_w++;

        if (pc >= 0) {
            uint32_t t = c->punch_t++;
            int32_t s = 0;
            switch (pc) {
            case FX_STUT4: case FX_STUT8: case FX_STUT16: case FX_STUT32: {
                static const uint8_t TK[4] = {96, 48, 24, 12};
                uint32_t len = fx_len(c, TK[pc]);
                s = bm_ulaw_dec[c->fxb[(c->punch_w0 + t % len) % BM_FXBUF_LEN]];
                L = R = s; break;
            }
            case FX_REV8: case FX_REV4: {
                uint32_t len = fx_len(c, pc == FX_REV8 ? 48 : 96);
                s = bm_ulaw_dec[c->fxb[(c->punch_w0 + BM_FXBUF_LEN * 4u - 1u - t % len) % BM_FXBUF_LEN]];
                L = R = s; break;
            }
            case FX_TAPESTOP: {
                uint32_t T = bm_ticks_to_samples(c, 96);
                int32_t rate = T ? 256 - (int32_t)((uint64_t)t * 256u / T) : 0;   /* Q8 */
                if (rate <= 0) { L = R = 0; break; }
                c->punch_rp += (uint32_t)rate;
                s = bm_ulaw_dec[c->fxb[(c->punch_rp >> 8) % BM_FXBUF_LEN]];
                L = R = s; break;
            }
            case FX_GATE: {
                int32_t target = ((c->st.free_ticks % 24u) < 12u) ? 32767 : 0;
                c->gate_g += (target - c->gate_g) >> 5;
                L = (int32_t)(((int64_t)L * c->gate_g) >> 15);
                R = (int32_t)(((int64_t)R * c->gate_g) >> 15);
                break;
            }
            case FX_BITCRUSH: L = (L >> 11) << 11; R = (R >> 11) << 11; break;
            case FX_SRDOWN:
                if (t % 6u == 0) { c->sh_l = L; c->sh_r = R; }
                L = c->sh_l; R = c->sh_r; break;
            default: break;
            }
            if (fon) {
                L = svf_run(&c->pf_ic1[0], &c->pf_ic2[0], L, a1, a2, a3, kq, fhp);
                R = svf_run(&c->pf_ic1[1], &c->pf_ic2[1], R, a1, a2, a3, kq, fhp);
            }
        }

        /* sampler: resample de la mezcla (post-FX, pre-master) */
        if (c->smp_rec) {
            if (c->smp_rec_pos < BM_SAMPLER_LEN) {
                int32_t v = (L + R) / 2 >> 8;
                c->smp[c->smp_rec_pos++] = (int8_t)(v > 127 ? 127 : (v < -128 ? -128 : v));
            } else {
                c->smp_rec = 0; c->smp_len = BM_SAMPLER_LEN; c->st.smp_rec = 0;
            }
        }

        /* soft-clipper con DRIVE */
        int32_t y[2];
        int32_t in[2] = {L, R};
        for (int ch = 0; ch < 2; ch++) {
            int64_t u = ((int64_t)in[ch] * drive_q8) >> 8;
            if (u >= 32768) { y[ch] = 32767; c->clip_hold = 1; }
            else if (u <= -32768) { y[ch] = -32767; c->clip_hold = 1; }
            else y[ch] = (int32_t)((3 * u) / 2 - (((u * u) >> 15) * u >> 15) / 2);
        }

        /* MASTER + fade de escritura de flash */
        int32_t mg = mg0 + (int32_t)(((int64_t)(mg1 - mg0) * (int32_t)i) / (int32_t)n);
        if (c->fade_req) { c->fade_gain -= 300; if (c->fade_gain < 0) c->fade_gain = 0; }
        else { c->fade_gain += 300; if (c->fade_gain > 32767) c->fade_gain = 32767; }
        int32_t g = (int32_t)(((int64_t)mg * c->fade_gain) >> 15);
        g = (g * 29000) >> 15;                                   /* techo ≈ -1 dBFS */
        int32_t oL = (int32_t)(((int64_t)y[0] * g) >> 15) << 8;
        int32_t oR = (int32_t)(((int64_t)y[1] * g) >> 15) << 8;

        if (c->sync_mode == 1) {                                 /* L = pulso, R = mezcla mono */
            oR = (oL + oR) / 2;
            int32_t p = 0;
            if (c->sync_wait) c->sync_wait--;
            else if (c->sync_left) { c->sync_left--; p = 7549747; }
            oL = p;
        }
        /* x2 a 44.1 kHz con interpolación lineal (el escalón ZOH dejaba -22 dB de imágenes) */
        out[4 * i + 0] = (c->prev_l + oL) / 2; out[4 * i + 1] = (c->prev_r + oR) / 2;
        out[4 * i + 2] = oL; out[4 * i + 3] = oR;
        c->prev_l = oL; c->prev_r = oR;
    }
    c->master_gain = (uint16_t)mg1;
    c->st.faded = (c->fade_req && c->fade_gain == 0) ? 1 : 0;
    if (c->clip_hold) { c->st.clip = 1; c->clip_hold = 0; }
    for (int t = 0; t < BM_TRACKS; t++) {
        uint32_t p = (uint32_t)peak[t] >> 7;
        if (p > 255) p = 255;
        uint32_t old = c->peak_acc[t];
        c->peak_acc[t] = p > old ? p : (old * 7) / 8;
        c->st.peak[t] = (uint8_t)c->peak_acc[t];
    }
    if (c->smp_rec) { c->st.smp_len = c->smp_rec_pos; }
}
