/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — ANALOG (core/bm_analog.h). Los osciladores calculan por bloque (como en Plaits);
 * el filtro y las envolventes, por muestra. El corte se mueve en semitonos y se interpola dentro del
 * bloque, así la envolvente de filtro barre sin escalones. Sin memoria dinámica: cada voz vive en
 * su espacio y se construye con placement new. */
#include <new>
#include <math.h>
#include <string.h>
#include "bm_analog.h"
#include "bm_config.h"
#include "bm_params.h"

#include "plaits/dsp/oscillator/sine_oscillator.h"
#include "plaits/dsp/oscillator/variable_shape_oscillator.h"
#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/units.h"

using namespace plaits;
using namespace stmlib;

static float fs_ = 44118.0f;

namespace {

enum { ST_OFF, ST_A, ST_D, ST_S, ST_R };

struct Adsr {
    float v; uint8_t st;
    void Gate(bool retrig) { if (retrig || st == ST_OFF || st == ST_R) st = ST_A; }
    void Release() { if (st != ST_OFF) st = ST_R; }
    /* coeficientes de un bloque: ka (ataque analógico, hacia 1.3), kd y kr (exponenciales) */
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

/* Oscilador propio con polyBLEP: el VariableShapeOscillator de Plaits costaba 431 ns por muestra
 * en el FM-1 (medido, lab 34); este calcula solo la forma pedida. El de Plaits queda para el
 * oscilador 2 con SYNC, que necesita su sincronía sin alias. */
inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Osc {
    float ph;
    inline float saw(float dt) { return 2.0f * ph - 1.0f - blep(ph, dt); }
    inline float sqr(float dt, float pw) {
        float p2 = ph + 1.0f - pw; if (p2 >= 1.0f) p2 -= 1.0f;
        return (ph < pw ? 1.0f : -1.0f) + blep(ph, dt) - blep(p2, dt);
    }
    inline float tri() { return 4.0f * fabsf(ph - 0.5f) - 1.0f; }
    inline void step(float dt) { ph += dt; if (ph >= 1.0f) ph -= 1.0f; }
    /* SIERRA hacia CUADRADA, CUADRADA con ancho de pulso, TRIANGULO hacia SIERRA */
    inline float wave(int w, float dt, float forma) {
        float y;
        switch (w) {
        case AN_WAVE_SQR: y = sqr(dt, 0.5f - 0.45f * forma); break;
        case AN_WAVE_TRI: y = forma > 0.0f ? tri() + forma * (saw(dt) - tri()) : tri(); break;
        default: y = forma > 0.0f ? saw(dt) + forma * (sqr(dt, 0.5f) - saw(dt)) : saw(dt); break;
        }
        step(dt);
        return y;
    }
};

/* saturación cúbica sin división (SoftClip de stmlib divide: 107 ns por muestra en el FM-1) */
inline float clip3(float x)
{
    x = x < -1.5f ? -1.5f : (x > 1.5f ? 1.5f : x);
    return x - 0.148148f * x * x * x;
}

struct Voice {
    VariableShapeOscillator o2;
    Osc a1, a2, aa, ab, as;
    Svf f1, f2;
    Adsr ea, ef;
    float note, target, vel;
    float lph, lsh, lprev;       /* LFO: fase, valor del azar, valor al final del bloque anterior */
    float cut_prev;              /* corte (semitonos) al final del bloque anterior */
    uint32_t rng;
    uint8_t first;
    uint8_t key[6];              /* envolventes: los ajustes con que se calcularon los coeficientes */
    float k[6];
};

alignas(8) unsigned char mem_[BM_VOICES][sizeof(Voice)];
inline Voice &V(int v) { return *reinterpret_cast<Voice *>(mem_[v]); }

inline float rnd(uint32_t &s) { s = s * 1664525u + 1013904223u; return (float)(int32_t)s * (1.0f / 2147483648.0f); }
inline float ka_of(float t) { return 1.0f - expf(-1.466f / (t * fs_ + 1.0f)); }
inline float kx_of(float t) { return expf(-4.6f / (t * fs_ + 1.0f)); }
inline float pc(int v) { return (float)v * (1.0f / 127.0f); }

inline float lfo_at(Voice &z, int shape) {
    float p = z.lph;
    switch (shape) {
    case AN_L_SIN: return Sine(p);
    case AN_L_TRI: return 4.0f * fabsf(p - 0.5f) - 1.0f;
    case AN_L_SAW: return 1.0f - 2.0f * p;
    case AN_L_SQR: return p < 0.5f ? 1.0f : -1.0f;
    default: return z.lsh;
    }
}

}  // namespace

extern "C" {

void bm_an_init(float fs)
{
    fs_ = fs;
    for (int v = 0; v < BM_VOICES; v++) {
        Voice *z = new (mem_[v]) Voice();
        z->ea.st = z->ef.st = ST_OFF; z->ea.v = z->ef.v = 0.0f;
    }
}

void bm_an_start(int v, const uint8_t *prm, const uint8_t mac[4], float note, float vel, int legato)
{
    if (v < 0 || v >= BM_VOICES) return;
    Voice &z = V(v);
    (void)mac;
    if (legato == 1 && z.ea.st != ST_OFF && z.ea.st != ST_R) {   /* misma voz: glide a la nota nueva */
        z.target = note;
        if (prm[AN_GLIDE] == 0) z.note = note;
        z.vel = vel;
        return;
    }
    bool sounding = legato == 2 && z.ea.st != ST_OFF;   /* mono: redispara desde la nota que sonaba */
    z.target = note;
    if (!sounding || prm[AN_GLIDE] == 0) z.note = note;
    z.vel = vel;
    if (!sounding) {
        z.o2.Init(); z.f1.Init(); z.f2.Init();
        z.a1.ph = 0.0f; z.a2.ph = 0.25f; z.aa.ph = 0.37f; z.ab.ph = 0.71f; z.as.ph = 0.0f;
        z.ea.v = z.ef.v = 0.0f;
        z.rng = 0x9E3779B9u * (uint32_t)(v + 1);
        z.first = 1;
    }
    z.lph = 0.0f; z.lsh = rnd(z.rng); z.lprev = 0.0f;
    z.ea.Gate(true); z.ef.Gate(true);
}

void bm_an_release(int v)
{
    if (v < 0 || v >= BM_VOICES) return;
    V(v).ea.Release(); V(v).ef.Release();
}

float bm_an_level(int v) { return v >= 0 && v < BM_VOICES ? V(v).ea.v : 0.0f; }

static int render(Voice &z, const uint8_t *prm, const uint8_t mac[4], float *out, int n);

int bm_an_render(int v, const uint8_t *prm, const uint8_t mac[4], float *out, int n)
{
    if (v < 0 || v >= BM_VOICES || n <= 0) return 0;
    return render(V(v), prm, mac, out, n);
}

static int render(Voice &z, const uint8_t *prm, const uint8_t mac[4], float *out, int n)
{
    if (z.ea.st == ST_OFF) { memset(out, 0, sizeof(float) * (size_t)n); return 0; }

    /* glide (por bloque, exponencial) */
    if (z.note != z.target) {
        float gt = prm[AN_GLIDE] ? bm_env_s(prm[AN_GLIDE]) : 0.0f;
        float k = gt > 0.0f ? 1.0f - expf(-(float)n / (gt * fs_ * 0.35f)) : 1.0f;
        z.note += (z.target - z.note) * k;
        if (fabsf(z.target - z.note) < 0.001f) z.note = z.target;
    }

    /* LFO: un valor por bloque */
    int ldest = prm[AN_LDEST] % AN_LDESTS;
    float lamt = pc(prm[AN_LAMT]), lfo = 0.0f;
    if (lamt > 0.0f) {
        z.lph += bm_lfo_hz(prm[AN_LRATE]) * (float)n / fs_;
        if (z.lph >= 1.0f) { z.lph -= (float)(int)z.lph; z.lsh = rnd(z.rng); }
        lfo = lfo_at(z, prm[AN_LSHAPE] % AN_LSHAPES);
    }

    float forma = pc(mac[3]);
    if (ldest == AN_D_SHAPE) { forma += 0.5f * lamt * lfo; CONSTRAIN(forma, 0.0f, 1.0f); }
    float pitch = z.note + (ldest == AN_D_PITCH ? lfo * lamt * lamt * 12.0f : 0.0f);
    float f1 = 440.0f * SemitonesToRatioSafe(pitch - 69.0f) / fs_;
    CONSTRAIN(f1, 0.00001f, 0.25f);
    float semi2 = (float)(((int)prm[AN_SEMI2] - 64) * 24 / 63) + (float)((int)prm[AN_FINE2] - 64) * (0.5f / 63.0f);
    float penv = (float)((int)prm[AN_PENV2] - 64) * (48.0f / 63.0f) * z.ef.v;
    float f2 = f1 * SemitonesToRatioSafe(semi2 + penv);
    CONSTRAIN(f2, 0.00001f, 0.25f);

    /* envolventes: los coeficientes se recalculan solo si cambió un ajuste (expf: 556 ns) */
    static const uint8_t KI[6] = {AN_AA, AN_AD, AN_AR, AN_FA, AN_FD, AN_FR};
    for (int i = 0; i < 6; i++) {
        uint8_t v = prm[KI[i]];
        if (z.key[i] != v || z.first) {
            z.key[i] = v;
            float t = bm_env_s(v);
            z.k[i] = (i % 3) == 0 ? ka_of(t) : kx_of(t);
        }
    }

    /* Por pasadas cortas: la FPU de este chip usa los registros de los enteros y un solo bucle con
     * todo se quedaba sin registros (lab 34). */
    static float b1[BM_BLOCK], b2[BM_BLOCK];        /* solo la tarea de audio: fuera de la pila */
    int w1 = prm[AN_W1] % AN_WAVES, w2 = prm[AN_W2] % 4;
    float mix2 = pc(prm[AN_MIX2]), ring = pc(prm[AN_RING]);
    bool need2 = mix2 > 0.0f || ring > 0.0f;
    float subl = pc(prm[AN_SUB]), noise = pc(prm[AN_NOISE]);
    float drive = pc(prm[AN_DRIVE]);
    float pre = 0.7f + 2.5f * drive * drive;

    /* 1. oscilador 1 */
    switch (w1) {
    case AN_WAVE_SIN: {
        float fold = 0.25f * (1.0f + 4.0f * forma);
        for (int i = 0; i < n; i++) {
            float sv = Sine(z.a1.ph);
            b1[i] = forma > 0.0f ? Sine(8.0f + fold * sv) : sv;
            z.a1.step(f1);
        }
        break;
    }
    case AN_WAVE_SUPER: {
        float det = 0.002f + 0.03f * forma, fa = f1 * (1.0f + det), fb = f1 * (1.0f - det * 0.93f);
        for (int i = 0; i < n; i++) {
            b1[i] = (z.a1.saw(f1) + 0.75f * (z.aa.saw(fa) + z.ab.saw(fb))) * 0.6f;
            z.a1.step(f1); z.aa.step(fa); z.ab.step(fb);
        }
        break;
    }
    case AN_WAVE_SQR: {
        float pw = 0.5f - 0.45f * forma;
        for (int i = 0; i < n; i++) { b1[i] = z.a1.sqr(f1, pw); z.a1.step(f1); }
        break;
    }
    case AN_WAVE_TRI:
        for (int i = 0; i < n; i++) { float t = z.a1.tri(); b1[i] = t + forma * (z.a1.saw(f1) - t); z.a1.step(f1); }
        break;
    default:
        if (forma > 0.0f) for (int i = 0; i < n; i++) { float sw = z.a1.saw(f1); b1[i] = sw + forma * (z.a1.sqr(f1, 0.5f) - sw); z.a1.step(f1); }
        else for (int i = 0; i < n; i++) { b1[i] = z.a1.saw(f1); z.a1.step(f1); }
        break;
    }

    /* 2. oscilador 2 (con SYNC, el de Plaits: sincronía sin alias), ring, sub y ruido */
    if (need2) {
        if (prm[AN_SYNC] && w2 != AN_WAVE_SIN) {
            float ws = w2 == AN_WAVE_SQR ? 1.0f : w2 == AN_WAVE_TRI ? 0.0f : 0.5f;
            z.o2.Render(f1, f2, 0.5f, ws, b2, (size_t)n);
        } else if (w2 == AN_WAVE_SIN) for (int i = 0; i < n; i++) { b2[i] = Sine(z.a2.ph); z.a2.step(f2); }
        else if (w2 == AN_WAVE_SQR) for (int i = 0; i < n; i++) { b2[i] = z.a2.sqr(f2, 0.5f); z.a2.step(f2); }
        else if (w2 == AN_WAVE_TRI) for (int i = 0; i < n; i++) { b2[i] = z.a2.tri(); z.a2.step(f2); }
        else for (int i = 0; i < n; i++) { b2[i] = z.a2.saw(f2); z.a2.step(f2); }
        if (ring > 0.0f) for (int i = 0; i < n; i++) b1[i] += mix2 * b2[i] + ring * b1[i] * b2[i];
        else for (int i = 0; i < n; i++) b1[i] += mix2 * b2[i];
    }
    if (subl > 0.0f) { float fs2 = f1 * 0.5f; for (int i = 0; i < n; i++) { b1[i] += subl * z.as.sqr(fs2, 0.5f); z.as.step(fs2); } }
    if (noise > 0.0f) for (int i = 0; i < n; i++) b1[i] += noise * rnd(z.rng);
    for (int i = 0; i < n; i++) b1[i] = clip3(b1[i] * pre);

    /* 3. filtro: corte en semitonos sobre 20 Hz, coeficientes cada 8 muestras */
    float cut = (float)mac[0] * (120.0f / 127.0f)
              + (float)((int)mac[2] - 64) * (84.0f / 63.0f) * z.ef.v
              + pc(prm[AN_KEYT]) * (z.note - 60.0f)
              + pc(prm[AN_VELF]) * z.vel * 30.0f
              + (ldest == AN_D_CUT ? lfo * lamt * 48.0f : 0.0f);
    if (z.first) { z.cut_prev = cut; z.first = 0; }
    float q = bm_reso_q(mac[1]), q2 = 0.5f + 0.02f * q;
    int ft = prm[AN_FTYPE] % AN_FILTERS;
    float c0 = z.cut_prev, dc = (cut - c0) / (float)n;
    z.cut_prev = cut;
    float kf = 20.0f / fs_;
    for (int i0 = 0; i0 < n; i0 += 8) {
        int i1 = i0 + 8 < n ? i0 + 8 : n;
        float f = SemitonesToRatioSafe(c0 + dc * (float)(i0 + 4)) * kf;
        CONSTRAIN(f, 0.0003f, 0.36f);
        z.f1.set_f_q<FREQUENCY_FAST>(f, q);
        switch (ft) {
        case AN_F_LP12: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_LOW_PASS>(b1[i]); break;
        case AN_F_BP: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(b1[i]) * 1.5f; break;
        case AN_F_HP: for (int i = i0; i < i1; i++) b1[i] = z.f1.Process<FILTER_MODE_HIGH_PASS>(b1[i]); break;
        default:
            z.f2.set_f_q<FREQUENCY_FAST>(f, q2);
            if (drive > 0.0f) for (int i = i0; i < i1; i++) b1[i] = z.f2.Process<FILTER_MODE_LOW_PASS>(clip3(z.f1.Process<FILTER_MODE_LOW_PASS>(b1[i])));
            else for (int i = i0; i < i1; i++) b1[i] = z.f2.Process<FILTER_MODE_LOW_PASS>(z.f1.Process<FILTER_MODE_LOW_PASS>(b1[i]));
            break;
        }
    }
    if (drive > 0.0f) { float d1 = 1.0f + drive; for (int i = 0; i < n; i++) b1[i] = clip3(b1[i] * d1); }

    /* 4. envolventes y nivel */
    float as = pc(prm[AN_AS]), fsus = pc(prm[AN_FS]);
    float velg = 1.0f - pc(prm[AN_VELA]) * (1.0f - z.vel);
    float trem = ldest == AN_D_AMP ? lamt * 0.5f : 0.0f;
    float g = velg * (1.0f - trem + trem * lfo) * 1.1f;
    float ka = z.k[0], kd = z.k[1], kr = z.k[2];
    for (int i = 0; i < n; i++) out[i] = b1[i] * z.ea.Next(ka, kd, as, kr) * g;
    ka = z.k[3]; kd = z.k[4]; kr = z.k[5];
    for (int i = 0; i < n; i++) z.ef.Next(ka, kd, fsus, kr);
    return z.ea.st != ST_OFF;
}

/* Medición en el aparato (hal/fm1/fm1_bench.c): ticks de now() para reps bloques de 64 de cada
 * pieza, con una voz aparte (no toca las del audio). */
void bm_an_bench(const uint8_t *prm, const uint8_t mac[4], uint32_t (*now)(void), uint32_t out[8], int reps)
{
    static float buf[BM_BLOCK];
    static unsigned char mem[sizeof(Voice)];
    Voice &z = *new (mem) Voice();
    volatile float sink = 0.0f;
    uint32_t t0;
    z.a1.ph = 0.0f;
    t0 = now();
    for (int r = 0; r < reps; r++) for (int i = 0; i < BM_BLOCK; i++) buf[i] = z.a1.wave(AN_WAVE_SAW, 0.01f, 0.0f);
    out[0] = now() - t0;
    z.f1.Init();
    t0 = now();
    for (int r = 0; r < reps; r++)
        for (int i = 0; i < BM_BLOCK; i++) { z.f1.set_f_q<FREQUENCY_FAST>(0.01f + (float)i * 1e-4f, 2.0f); buf[i] = z.f1.Process<FILTER_MODE_LOW_PASS>(buf[i]); }
    out[1] = now() - t0;
    t0 = now();
    for (int r = 0; r < reps; r++) for (int i = 0; i < BM_BLOCK; i++) buf[i] = clip3(buf[i] * 1.1f);
    out[2] = now() - t0;
    t0 = now();
    for (int r = 0; r < reps; r++) for (int i = 0; i < BM_BLOCK; i++) buf[i] = SemitonesToRatioSafe((float)i * 0.5f + buf[i]);
    out[3] = now() - t0;
    z.ea.st = z.ef.st = ST_OFF;
    {
        z.o2.Init(); z.f1.Init(); z.f2.Init(); z.a1.ph = z.a2.ph = z.aa.ph = z.ab.ph = z.as.ph = 0.0f;
        z.note = z.target = 48.0f; z.vel = 0.8f; z.ea.v = z.ef.v = 0.0f;
        z.rng = 1; z.first = 1; z.lph = 0.0f; z.lsh = 0.0f; z.lprev = 0.0f;
        z.ea.Gate(true); z.ef.Gate(true);
    }
    t0 = now();
    for (int r = 0; r < reps; r++) render(z, prm, mac, buf, BM_BLOCK);
    out[4] = now() - t0;
    t0 = now();
    for (int r = 0; r < reps; r++) for (int i = 0; i < BM_BLOCK; i++) sink = sink + buf[i];
    out[5] = now() - t0;
    t0 = now();
    for (int r = 0; r < reps; r++) for (int i = 0; i < BM_BLOCK; i++) buf[i] = 1.0f / (buf[i] + 2.0f);
    out[6] = now() - t0;
    t0 = now();
    for (int r = 0; r < reps; r++) sink = sink + expf(-1.0f / (sink + (float)r + 10.0f));
    out[7] = now() - t0;
}

}  // extern "C"

