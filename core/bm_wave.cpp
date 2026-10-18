/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — núcleo WAVE (core/bm_wave.h). Lo que se aprendió con ANALOG en el FM-1 (lab 34):
 * osciladores por bloque, en pasadas cortas (la FPU usa los registros de los enteros y un bucle con
 * todo junto se queda sin ellos), el filtro recalcula sus coeficientes cada 8 muestras con el corte
 * interpolado en semitonos, las envolventes guardan sus coeficientes mientras no cambie el ajuste
 * y la saturación es un polinomio sin divisiones. */
#include <new>
#include <math.h>
#include <string.h>
#include "bm_wave.h"
#include "bm_config.h"
#include "bm_params.h"
#include "bm_wt.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/units.h"

using namespace stmlib;

static float fs_ = 44118.0f;

namespace {

enum { ST_OFF, ST_A, ST_D, ST_S, ST_R };

struct Adsr {
    float v; uint8_t st;
    void Gate(bool retrig) { if (retrig || st == ST_OFF || st == ST_R) st = ST_A; }
    void Release() { if (st != ST_OFF) st = ST_R; }
    inline float Next(float ka, float kd, float s, float kr) {
        switch (st) {
        case ST_A: v += (1.3f - v) * ka; if (v >= 1.0f) { v = 1.0f; st = ST_D; } break;
        case ST_D: v = s + (v - s) * kd; if (v - s < 1.0e-4f) { v = s; st = ST_S; } break;
        case ST_S: v = s; break;
        case ST_R: v *= kr; if (v < 1.0e-4f) { v = 0.0f; st = ST_OFF; } break;
        default: v = 0.0f; break;
        }
        return v;
    }
};

inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Osc {
    float ph;
    inline float saw(float dt) const { return 2.0f * ph - 1.0f - blep(ph, dt); }
    inline float sqr(float dt, float pw) const {
        float p2 = ph + 1.0f - pw; if (p2 >= 1.0f) p2 -= 1.0f;
        return (ph < pw ? 1.0f : -1.0f) + blep(ph, dt) - blep(p2, dt);
    }
    inline float tri() const { return 4.0f * fabsf(ph - 0.5f) - 1.0f; }
    inline void step(float dt) { ph += dt; if (ph >= 1.0f) ph -= 1.0f; }
};

inline float ftz(float x) { return (x > -1e-15f && x < 1e-15f) ? 0.0f : x; }

inline float clip3(float x)
{
    x = x < -1.5f ? -1.5f : (x > 1.5f ? 1.5f : x);
    return x - 0.148148f * x * x * x;
}

/* Escalera de 4 polos (TPT de un polo en cascada, realimentación de la salida anterior y saturación
 * a la entrada). k hasta ~3.9: autooscila. La compensación de ganancia evita que la resonancia
 * vacíe los graves, como en las escaleras de verdad hasta cierto punto. */
struct Ladder {
    float s[4], y4;
    void Init() { s[0] = s[1] = s[2] = s[3] = 0.0f; y4 = 0.0f; }
    inline float Process(float x, float G, float k) {
        float u = clip3((x * (1.0f + 0.35f * k)) - k * y4);
        float v, y;
        v = (u - s[0]) * G; y = v + s[0]; s[0] = y + v; u = y;
        v = (u - s[1]) * G; y = v + s[1]; s[1] = y + v; u = y;
        v = (u - s[2]) * G; y = v + s[2]; s[2] = y + v; u = y;
        v = (u - s[3]) * G; y = v + s[3]; s[3] = y + v;
        y4 = y;
        return y;
    }
};

struct Voice {
    Osc o[3], sub;
    float wph[2][4];             /* BITTAL: fases del unísono de cada oscilador */
    Ladder lad;
    Svf f1, f2;
    float hp_z;                  /* BUNO-6: pasa-altos de un polo */
    Adsr ea, ef;
    float note, target, vel;
    float lph, lsh, ldel;        /* LFO: fase, valor del azar, rampa del retardo (BUNO-6) */
    float cut_prev, gate_g;
    uint32_t rng;
    uint8_t first, gate;
    uint8_t key[8];
    float k[8];
};

alignas(8) unsigned char mem_[BM_VOICES][sizeof(Voice)];
inline Voice &V(int v) { return *reinterpret_cast<Voice *>(mem_[v]); }

inline float rnd(uint32_t &s) { s = s * 1664525u + 1013904223u; return (float)(int32_t)s * (1.0f / 2147483648.0f); }
inline float ka_of(float t) { return 1.0f - expf(-1.466f / (t * fs_ + 1.0f)); }
inline float kx_of(float t) { return expf(-4.6f / (t * fs_ + 1.0f)); }
inline float pc(int v) { return (float)v * (1.0f / 127.0f); }
inline float bip(int v) { return (float)(v - 64) * (1.0f / 63.0f); }

/* los coeficientes de las envolventes: solo si cambió el ajuste (expf cuesta 556 ns en el FM-1) */
inline void env_coefs(Voice &z, const uint8_t idx[8], const uint8_t *prm, const float scale[8])
{
    for (int i = 0; i < 8; i++) {
        if (idx[i] == 0xFF) continue;
        uint8_t v = prm[idx[i]];
        uint8_t key = (uint8_t)(v ^ (uint8_t)(scale[i] * 16.0f));
        if (z.key[i] != key || z.first) {
            z.key[i] = key;
            float t = bm_env_s(v) * scale[i];
            z.k[i] = (i % 4) == 0 ? ka_of(t) : kx_of(t);
        }
    }
}

inline void glide(Voice &z, int gl, int n)
{
    if (z.note == z.target) return;
    float gt = gl ? bm_env_s(gl) : 0.0f;
    float k = gt > 0.0f ? 1.0f - expf(-(float)n / (gt * fs_ * 0.35f)) : 1.0f;
    z.note += (z.target - z.note) * k;
    if (fabsf(z.target - z.note) < 0.001f) z.note = z.target;
}

inline float hz_of(float note) { return 440.0f * SemitonesToRatioSafe(note - 69.0f); }

/* corte en semitonos sobre 20 Hz -> G de la escalera (cada 8 muestras) */
inline float ladder_g(float semis)
{
    float f = SemitonesToRatioSafe(semis) * 20.0f / fs_;
    CONSTRAIN(f, 0.0002f, 0.45f);
    float w = 3.14159265f * f;
    float g = w * (1.0f + w * w * (0.33333f + w * w * 0.13333f));   /* tan(w), suficiente hasta 0.45 */
    return g / (1.0f + g);
}

/* búfer de trabajo, uno por núcleo: con el audio en dos núcleos, T3 (B-MINI) calcula en uno y T5-T6
 * (BITTAL, BUNO-6) en el otro al mismo tiempo */
float SB[2][BM_BLOCK];

/* ------------------------------------------------------------ B-MINI */
float mini_osc(Osc &o, int w, float dt)
{
    float y;
    switch (w) {
    case MN_WAVE_TRI: y = o.tri(); break;
    case MN_WAVE_SHARK: { float t = o.tri(); y = t + 0.5f * (o.saw(dt) - t); break; }
    case MN_WAVE_SQR: y = o.sqr(dt, 0.5f); break;
    case MN_WAVE_WIDE: y = o.sqr(dt, 0.3f); break;
    case MN_WAVE_NARROW: y = o.sqr(dt, 0.12f); break;
    default: y = o.saw(dt); break;
    }
    o.step(dt);
    return y;
}

static const float RANGE[5] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};   /* 32' 16' 8' 4' 2' */

int render_mini(Voice &z, const uint8_t *prm, const uint8_t mac[4], float *out, int n, float *b1)
{
    glide(z, prm[MN_GLIDE], n);
    float lamt = pc(prm[MN_LAMT]), lfo = 0.0f;
    if (lamt > 0.0f) { z.lph += bm_lfo_hz(prm[MN_LRATE]) * (float)n / fs_; if (z.lph >= 1.0f) z.lph -= (float)(int)z.lph; lfo = 4.0f * fabsf(z.lph - 0.5f) - 1.0f; }
    int ld = prm[MN_LDEST] % MN_LDESTS;
    float pitch = z.note + (ld == MN_D_PITCH ? lfo * lamt * lamt * 2.0f : 0.0f);
    float f0 = hz_of(pitch) / fs_;
    float tune = bip(prm[MN_TUNE]) * 1.0f;                          /* TUNE: ±1 semitono a 2 y 3 */
    float fr[3];
    fr[0] = f0 * RANGE[prm[MN_R1] % 5];
    fr[1] = f0 * RANGE[prm[MN_R2] % 5] * SemitonesToRatioSafe(bip(prm[MN_T2]) * 7.0f + tune);
    fr[2] = f0 * RANGE[prm[MN_R3] % 5] * SemitonesToRatioSafe(bip(prm[MN_T3]) * 7.0f - tune * 0.5f);
    for (int i = 0; i < 3; i++) CONSTRAIN(fr[i], 0.00001f, 0.25f);

    /* las dos envolventes; la macro F DECAY (64 = nada) corre la caída de la de filtro */
    static const uint8_t KI[8] = {0, 1, 0xFF, 3, 4, 5, 0xFF, 7};
    static const float SC[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    int fd = prm[MN_FD] + (int)mac[3] - 64;
    const uint8_t ev[8] = {prm[MN_FA], (uint8_t)(fd < 0 ? 0 : fd > 127 ? 127 : fd), 0, prm[MN_FR], prm[MN_AA], prm[MN_AD], 0, prm[MN_AR]};
    env_coefs(z, KI, ev, SC);

    const uint8_t W[3] = {(uint8_t)(prm[MN_W1] % MN_WAVES), (uint8_t)(prm[MN_W2] % MN_WAVES), (uint8_t)(prm[MN_W3] % MN_WAVES)};
    const float Lv[3] = {pc(prm[MN_L1]), pc(prm[MN_L2]), pc(prm[MN_L3])};
    for (int i = 0; i < n; i++) b1[i] = 0.0f;
    for (int k = 0; k < 3; k++) {
        if (Lv[k] <= 0.0f) continue;
        for (int i = 0; i < n; i++) b1[i] += Lv[k] * mini_osc(z.o[k], W[k], fr[k]);
    }
    float noise = pc(prm[MN_NOISE]);
    if (noise > 0.0f) for (int i = 0; i < n; i++) b1[i] += noise * rnd(z.rng);
    float drive = pc(prm[MN_DRIVE]), pre = 0.55f + 2.5f * drive * drive;
    for (int i = 0; i < n; i++) b1[i] = clip3(b1[i] * pre);

    /* filtro: CUTOFF, EMPHASIS y CONTOUR (la cantidad de la envolvente de filtro) */
    float cut = (float)mac[0] * (120.0f / 127.0f) + pc(mac[2]) * 72.0f * z.ef.v
              + pc(prm[MN_KEYT]) * (z.note - 60.0f) + pc(prm[MN_VELF]) * z.vel * 24.0f
              + (ld == MN_D_CUT ? lfo * lamt * 36.0f : 0.0f);
    if (z.first) { z.cut_prev = cut; z.first = 0; }
    float kres = pc(mac[1]) * 3.95f;
    float c0 = z.cut_prev, dc = (cut - c0) / (float)n;
    z.cut_prev = cut;
    for (int i0 = 0; i0 < n; i0 += 8) {
        int i1 = i0 + 8 < n ? i0 + 8 : n;
        float G = ladder_g(c0 + dc * (float)(i0 + 4));
        for (int i = i0; i < i1; i++) b1[i] = z.lad.Process(b1[i], G, kres);
    }
    float fsus = pc(prm[MN_FS]), as = pc(prm[MN_AS]);
    float velg = 1.0f - pc(prm[MN_VELA]) * (1.0f - z.vel);
    float ka = z.k[4], kd = z.k[5], kr = z.k[7];
    for (int i = 0; i < n; i++) out[i] = b1[i] * z.ea.Next(ka, kd, as, kr) * velg * 1.2f;
    ka = z.k[0]; kd = z.k[1]; kr = z.k[3];
    for (int i = 0; i < n; i++) z.ef.Next(ka, kd, fsus, kr);
    return z.ea.st != ST_OFF;
}

/* ------------------------------------------------------------ BUNO-6 */
static const float HPF_HZ[4] = {0.0f, 120.0f, 250.0f, 520.0f};   /* posiciones 0-3 (a medir contra un Juno) */

int render_buno(Voice &z, const uint8_t *prm, const uint8_t mac[4], float *out, int n, float *b1)
{
    /* LFO triangular con retardo: la rampa sube después de la nota */
    z.lph += bm_lfo_hz(prm[BN_LRATE]) * (float)n / fs_;
    if (z.lph >= 1.0f) z.lph -= (float)(int)z.lph;
    float dl = bm_env_s(prm[BN_LDELAY]);
    z.ldel += (float)n / (fs_ * (dl > 0.002f ? dl : 0.002f));
    if (z.ldel > 1.0f) z.ldel = 1.0f;
    float lfo = (4.0f * fabsf(z.lph - 0.5f) - 1.0f) * (prm[BN_LDELAY] ? z.ldel : 1.0f);

    static const uint8_t KI[8] = {BN_A, BN_D, 0xFF, BN_R, 0xFF, 0xFF, 0xFF, 0xFF};
    static const float SC[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    env_coefs(z, KI, prm, SC);

    static const float RNG[3] = {0.5f, 1.0f, 2.0f};                  /* 16' 8' 4' */
    float pitch = z.note + lfo * pc(prm[BN_LFODCO]) * pc(prm[BN_LFODCO]) * 3.0f;
    float f0 = hz_of(pitch) / fs_ * RNG[prm[BN_RANGE] % 3];
    CONSTRAIN(f0, 0.00001f, 0.25f);
    float pwm = pc(prm[BN_PWM]), mod;
    switch (prm[BN_PWMSRC] % BN_PWMS) {
    case BN_PWM_MAN: mod = 1.0f; break;
    case BN_PWM_ENV: mod = z.ef.v; break;
    default: mod = 0.5f + 0.5f * lfo; break;
    }
    float pw = 0.5f - 0.46f * pwm * mod;
    bool saw = prm[BN_SAW] != 0, pul = prm[BN_PULSE] != 0;
    float subl = pc(prm[BN_SUB]), noise = pc(prm[BN_NOISE]);
    for (int i = 0; i < n; i++) {
        float y = 0.0f;
        if (saw) y += z.o[0].saw(f0);
        if (pul) y += z.o[0].sqr(f0, pw);
        b1[i] = y;
        z.o[0].step(f0);
    }
    if (subl > 0.0f) { float fs2 = f0 * 0.5f; for (int i = 0; i < n; i++) { b1[i] += subl * z.sub.sqr(fs2, 0.5f); z.sub.step(fs2); } }
    if (noise > 0.0f) for (int i = 0; i < n; i++) b1[i] += noise * rnd(z.rng);
    for (int i = 0; i < n; i++) b1[i] *= 0.55f;

    int hpf = prm[BN_HPF] & 3;
    if (hpf) {
        float a = expf(-6.2831853f * HPF_HZ[hpf] / fs_);
        for (int i = 0; i < n; i++) { float x = b1[i]; z.hp_z = x + a * (z.hp_z - x); b1[i] = x - z.hp_z; }
        /* (un polo: hp = x - lp) */
    }
    /* VCF: FREQ, RES, ENV (con su polaridad), LFO y teclado */
    float env = pc(mac[2]) * 84.0f * z.ef.v * (prm[BN_ENVPOL] ? -1.0f : 1.0f);
    float cut = (float)mac[0] * (120.0f / 127.0f) + env + pc(mac[3]) * lfo * 30.0f   /* macro LFO: el LFO al VCF */
              + pc(prm[BN_KEYT]) * (z.note - 60.0f) + pc(prm[BN_VELF]) * z.vel * 20.0f;
    if (z.first) { z.cut_prev = cut; z.first = 0; }
    float kres = pc(mac[1]) * 3.9f;
    float c0 = z.cut_prev, dc = (cut - c0) / (float)n;
    z.cut_prev = cut;
    for (int i0 = 0; i0 < n; i0 += 8) {
        int i1 = i0 + 8 < n ? i0 + 8 : n;
        float G = ladder_g(c0 + dc * (float)(i0 + 4));
        for (int i = i0; i < i1; i++) b1[i] = z.lad.Process(b1[i], G, kres);
    }
    /* VCA: con la envolvente o en GATE (con 2 ms de suavizado) */
    float s = pc(prm[BN_S]);
    float velg = 1.0f - pc(prm[BN_VELA]) * (1.0f - z.vel);
    float ka = z.k[0], kd = z.k[1], kr = z.k[3];
    if (prm[BN_VCA]) {
        float tgt = z.gate ? 1.0f : 0.0f, kg = 1.0f / (0.002f * fs_);
        for (int i = 0; i < n; i++) {
            z.ef.Next(ka, kd, s, kr);
            z.gate_g += (tgt - z.gate_g) * kg;
            out[i] = b1[i] * z.gate_g * velg * 1.3f;
        }
        z.ea.v = z.gate_g > z.ef.v ? z.gate_g : z.ef.v;
        z.ea.st = (z.gate || z.gate_g > 1e-4f || z.ef.st != ST_OFF) ? ST_S : ST_OFF;
        if (!z.gate && z.gate_g <= 1e-4f) z.ea.st = ST_OFF;
    } else {
        for (int i = 0; i < n; i++) out[i] = b1[i] * z.ef.Next(ka, kd, s, kr) * velg * 1.3f;
        z.ea.v = z.ef.v; z.ea.st = z.ef.st;
    }
    return z.ea.st != ST_OFF;
}

/* ------------------------------------------------------------ BITTAL */
inline float lfo_at(Voice &z, int shape)
{
    float p = z.lph;
    switch (shape) {
    case BT_L_SIN: return sinf(6.2831853f * p);
    case BT_L_TRI: return 4.0f * fabsf(p - 0.5f) - 1.0f;
    case BT_L_SAW: return 1.0f - 2.0f * p;
    case BT_L_SQR: return p < 0.5f ? 1.0f : -1.0f;
    default: return z.lsh;
    }
}

/* un oscilador de tabla con su unísono, sumado en dst */
void wt_osc(Voice &z, int o, int slot, float f0, float pos, int uni, float det, float lvl, float *dst, int n)
{
    if (!bm_wt_ready(slot) || lvl <= 0.0f) return;
    int nf = bm_wt_frames(slot);
    CONSTRAIN(pos, 0.0f, 1.0f);
    float fp = pos * (float)(nf - 1);
    int fa = (int)fp, fb = fa + 1 < nf ? fa + 1 : fa;
    float ff = fp - (float)fa;
    float hz = f0 * fs_ * (1.0f + det);
    int lvlx = bm_wt_level_for(hz), N;
    const int16_t *ta = bm_wt_data(slot, fa, lvlx, &N);
    const int16_t *tb = bm_wt_data(slot, fb, lvlx, &N);
    float fN = (float)N;
    uint32_t mask = (uint32_t)N - 1u;
    float g = lvl * (1.0f / 32768.0f) / sqrtf((float)uni);
    for (int u = 0; u < uni; u++) {
        float sp = uni > 1 ? ((float)u / (float)(uni - 1)) * 2.0f - 1.0f : 0.0f;
        float dt = f0 * (1.0f + det * sp);
        CONSTRAIN(dt, 0.00001f, 0.45f);
        float ph = z.wph[o][u];
        for (int i = 0; i < n; i++) {
            float x = ph * fN;
            uint32_t i0 = (uint32_t)x;
            float fr = x - (float)i0;
            uint32_t j0 = i0 & mask, j1 = (i0 + 1u) & mask;
            float a = (float)ta[j0] + fr * (float)(ta[j1] - ta[j0]);
            float b = (float)tb[j0] + fr * (float)(tb[j1] - tb[j0]);
            dst[i] += (a + ff * (b - a)) * g;
            ph += dt; if (ph >= 1.0f) ph -= 1.0f;
        }
        z.wph[o][u] = ph;
    }
}

int render_bittal(Voice &z, const uint8_t *prm, const uint8_t mac[4], float *out, int n, float *b1)
{
    glide(z, prm[BT_GLIDE], n);
    float lamt = pc(prm[BT_LAMT]) + 0.5f * bm_bt_macro(prm, mac, BTM_LFO), lfo = 0.0f;
    CONSTRAIN(lamt, 0.0f, 1.0f);
    if (lamt > 0.0f) {
        z.lph += bm_lfo_hz(prm[BT_LRATE]) * (float)n / fs_;
        if (z.lph >= 1.0f) { z.lph -= (float)(int)z.lph; z.lsh = rnd(z.rng); }
        lfo = lfo_at(z, prm[BT_LSHAPE] % BT_LSHAPES);
    }
    int ld = prm[BT_LDEST] % BT_LDESTS;
    float rel = 1.0f + 3.0f * bm_bt_macro(prm, mac, BTM_REL);
    if (rel < 0.15f) rel = 0.15f;
    static const uint8_t KI[8] = {BT_MA, BT_MD, 0xFF, BT_MR, BT_AA, BT_AD, 0xFF, BT_AR};
    float SC[8] = {1, 1, 1, rel, 1, 1, 1, rel};
    env_coefs(z, KI, prm, SC);

    float pitch = z.note + (ld == BT_D_PITCH ? lfo * lamt * lamt * 12.0f : 0.0f);
    float f0 = hz_of(pitch) / fs_;
    float dets = bm_bt_macro(prm, mac, BTM_DET);
    float pall = bm_bt_macro(prm, mac, BTM_POS) + (ld == BT_D_POS ? lfo * lamt * 0.5f : 0.0f);
    for (int i = 0; i < n; i++) b1[i] = 0.0f;
    for (int o = 0; o < 2; o++) {
        const int b = o ? BT_L2 : BT_L1;                             /* L, S, F, P, U, D en orden */
        float lvl = pc(prm[b + 0]) + (o ? bm_bt_macro(prm, mac, BTM_LVL2) : 0.0f);
        if (lvl <= 0.0f) continue;
        float semi = bip(prm[b + 1]) * 24.0f + bip(prm[b + 2]) * 0.5f;
        float pos = pc(prm[b + 3]) + pall + bm_bt_macro(prm, mac, o ? BTM_POS2 : BTM_POS1);
        int uni = 1 + (prm[b + 4] & 3);
        float det = (pc(prm[b + 5]) + 0.5f * dets) * 0.02f;
        if (det < 0.0f) det = 0.0f;
        wt_osc(z, o, o, f0 * SemitonesToRatioSafe(semi), pos, uni, det, lvl, b1, n);
    }
    float drive = pc(prm[BT_DRIVE]) + bm_bt_macro(prm, mac, BTM_DRIVE);
    CONSTRAIN(drive, 0.0f, 1.0f);
    float pre = 0.8f + 2.5f * drive * drive;
    for (int i = 0; i < n; i++) b1[i] = clip3(b1[i] * pre);

    int ft = prm[BT_FT] % BT_FILTERS;
    if (ft != BT_F_OFF) {
        float fenv = bip(prm[BT_FENV]) + bm_bt_macro(prm, mac, BTM_FENV);
        float cut = (float)prm[BT_CUT] * (120.0f / 127.0f) + bm_bt_macro(prm, mac, BTM_CUT) * 60.0f
                  + fenv * 72.0f * z.ef.v + pc(prm[BT_KEYT]) * (z.note - 60.0f) + pc(prm[BT_VELF]) * z.vel * 24.0f
                  + (ld == BT_D_CUT ? lfo * lamt * 48.0f : 0.0f);
        if (z.first) { z.cut_prev = cut; z.first = 0; }
        float res = pc(prm[BT_RES]) + bm_bt_macro(prm, mac, BTM_RES);
        CONSTRAIN(res, 0.0f, 1.0f);
        float c0 = z.cut_prev, dc = (cut - c0) / (float)n;
        z.cut_prev = cut;
        float q = 0.5f + 24.5f * res * res * res, q2 = 0.5f + 0.02f * q, kres = res * 3.9f;
        for (int i0 = 0; i0 < n; i0 += 8) {
            int i1 = i0 + 8 < n ? i0 + 8 : n;
            float cs = c0 + dc * (float)(i0 + 4);
            if (ft == BT_F_LADDER) {
                float G = ladder_g(cs);
                for (int i = i0; i < i1; i++) b1[i] = z.lad.Process(b1[i], G, kres);
                continue;
            }
            float f = SemitonesToRatioSafe(cs) * 20.0f / fs_;
            CONSTRAIN(f, 0.0003f, 0.36f);
            z.f1.set_f_q<FREQUENCY_FAST>(f, q);
            switch (ft) {
            case BT_F_LP12: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_LOW_PASS>(b1[i]); break;
            case BT_F_BP: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(b1[i]) * 1.5f; break;
            case BT_F_HP: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_HIGH_PASS>(b1[i]); break;
            default:
                z.f2.set_f_q<FREQUENCY_FAST>(f, q2);
                for (int i = i0; i < i1; i++) b1[i] = z.f2.Process<FILTER_MODE_LOW_PASS>(z.f1.Process<FILTER_MODE_LOW_PASS>(b1[i]));
                break;
            }
        }
    } else z.first = 0;

    float as = pc(prm[BT_AS]), ms = pc(prm[BT_MS]);
    float velg = 1.0f - pc(prm[BT_VELA]) * (1.0f - z.vel);
    float trem = ld == BT_D_AMP ? lamt * 0.5f : 0.0f;
    float g = velg * (1.0f - trem + trem * lfo) * 1.1f;
    float ka = z.k[4], kd = z.k[5], kr = z.k[7];
    for (int i = 0; i < n; i++) out[i] = b1[i] * z.ea.Next(ka, kd, as, kr) * g;
    ka = z.k[0]; kd = z.k[1]; kr = z.k[3];
    for (int i = 0; i < n; i++) z.ef.Next(ka, kd, ms, kr);
    return z.ea.st != ST_OFF;
}

int render(Voice &z, int eng, const uint8_t *prm, const uint8_t mac[4], float *out, int n, int lane)
{
    if (z.ea.st == ST_OFF && z.ef.st == ST_OFF) { memset(out, 0, sizeof(float) * (size_t)n); return 0; }
    float *b1 = SB[lane & 1];
    switch (eng) {
    case ENG_MINI: return render_mini(z, prm, mac, out, n, b1);
    case ENG_BUNO: return render_buno(z, prm, mac, out, n, b1);
    default: return render_bittal(z, prm, mac, out, n, b1);
    }
}

}  // namespace

extern "C" {

float bm_bt_macro(const uint8_t *prm, const uint8_t mac[4], int dest)
{
    float s = 0.0f;
    for (int i = 0; i < 4; i++)
        if (prm[BT_M1D + 2 * i] == dest) s += pc(mac[i]) * bip(prm[BT_M1A + 2 * i]);
    return s;
}

void bm_wv_init(float fs)
{
    fs_ = fs;
    memset(mem_, 0, sizeof mem_);                       /* sin restos de un arranque anterior */
    memset(SB, 0, sizeof SB);
    for (int v = 0; v < BM_VOICES; v++) {
        Voice *z = new (mem_[v]) Voice();
        z->ea.st = z->ef.st = ST_OFF; z->ea.v = z->ef.v = 0.0f;
    }
}

void bm_wv_start(int v, int eng, const uint8_t *prm, const uint8_t mac[4], float note, float vel, int legato)
{
    if (v < 0 || v >= BM_VOICES) return;
    Voice &z = V(v);
    (void)mac;
    int gl = eng == ENG_MINI ? prm[MN_GLIDE] : eng == ENG_BITTAL ? prm[BT_GLIDE] : 0;
    bool alive = z.ea.st != ST_OFF && z.ea.st != ST_R;
    if (legato == 1 && alive) { z.target = note; if (!gl) z.note = note; z.vel = vel; z.gate = 1; return; }
    bool sounding = legato == 2 && z.ea.st != ST_OFF;
    z.target = note;
    if (!sounding || !gl) z.note = note;
    z.vel = vel;
    if (!sounding) {
        z.f1.Init(); z.f2.Init(); z.lad.Init(); z.hp_z = 0.0f;
        z.o[0].ph = 0.0f; z.o[1].ph = 0.31f; z.o[2].ph = 0.67f; z.sub.ph = 0.0f;
        z.rng = 0x9E3779B9u * (uint32_t)(v + 1);
        for (int o = 0; o < 2; o++) for (int u = 0; u < 4; u++) z.wph[o][u] = rnd(z.rng) * 0.5f + 0.5f;
        z.ea.v = z.ef.v = 0.0f; z.gate_g = 0.0f;
        z.first = 1;
    }
    z.gate = 1;
    z.lph = eng == ENG_BUNO ? z.lph : 0.0f; z.lsh = rnd(z.rng); z.ldel = 0.0f;
    z.ea.Gate(true); z.ef.Gate(true);
}

void bm_wv_release(int v)
{
    if (v < 0 || v >= BM_VOICES) return;
    V(v).ea.Release(); V(v).ef.Release(); V(v).gate = 0;
}

float bm_wv_level(int v) { return v >= 0 && v < BM_VOICES ? V(v).ea.v : 0.0f; }

int bm_wv_render(int v, int eng, const uint8_t *prm, const uint8_t mac[4], float *out, int n, int lane)
{
    if (v < 0 || v >= BM_VOICES || n <= 0) return 0;
    Voice &z = V(v);
    int alive = render(z, eng, prm, mac, out, n, lane);
    /* Un NaN en un filtro o una envolvente se queda: la última muestra y los estados, una vez por
     * bloque. Si algo se rompió, la voz vuelve a cero, el bloque sale en silencio y se avisa (-1). */
    float s = z.ea.v + z.ef.v + z.lad.y4 + z.lad.s[0] + z.lad.s[1] + z.lad.s[2] + z.lad.s[3] + z.hp_z + z.gate_g
            + z.note + z.cut_prev + z.lph;
    if (!(s > -1e9f && s < 1e9f) || !(out[n - 1] > -1e4f && out[n - 1] < 1e4f)) {
        memset(out, 0, sizeof(float) * (size_t)n);
        z.f1.Init(); z.f2.Init(); z.lad.Init(); z.hp_z = 0.0f; z.gate_g = 0.0f;
        z.ea.v = z.ef.v = 0.0f; z.ea.st = z.ef.st = ST_OFF;
        z.note = z.target = 60.0f; z.cut_prev = 0.0f; z.lph = 0.0f; z.first = 1;
        return -1;
    }
    /* colas: lo que se apaga solo no baja a los subnormales (lentos), una vez por bloque */
    for (int i = 0; i < 4; i++) z.lad.s[i] = ftz(z.lad.s[i]);
    z.lad.y4 = ftz(z.lad.y4); z.hp_z = ftz(z.hp_z); z.gate_g = ftz(z.gate_g);
    return alive;
}

void bm_wv_bench(const uint8_t *pm, const uint8_t *pb, const uint8_t *pn, const uint8_t mac[4],
                 uint32_t (*now)(void), uint32_t out[3], int reps)
{
    static float buf[BM_BLOCK];
    static unsigned char mem[sizeof(Voice)];
    const uint8_t *P[3] = {pm, pb, pn};
    const int E[3] = {ENG_MINI, ENG_BITTAL, ENG_BUNO};
    for (int m = 0; m < 3; m++) {
        Voice &z = *new (mem) Voice();
        z.ea.st = z.ef.st = ST_OFF;
        z.note = z.target = 48.0f; z.vel = 0.8f; z.rng = 1; z.first = 1; z.gate = 1;
        z.ea.Gate(true); z.ef.Gate(true);
        uint32_t t0 = now();
        for (int r = 0; r < reps; r++) render(z, E[m], P[m], mac, buf, BM_BLOCK, 0);
        out[m] = now() - t0;
    }
}

}  // extern "C"
