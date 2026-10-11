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

struct Voice {
    VariableShapeOscillator o1, o2, oa, ob, sub;
    Svf f1, f2;
    Adsr ea, ef;
    float note, target, vel;
    float sph1, sph2;            /* fases de los osciladores SENO */
    float lph, lsh, lprev;       /* LFO: fase, valor del azar, valor al final del bloque anterior */
    float cut_prev;              /* corte (semitonos) al final del bloque anterior */
    uint32_t rng;
    uint8_t first;
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

/* una forma (0 triángulo, 0.5 sierra, 1 cuadrada) y ancho de pulso según la onda y FORMA */
inline void shape_of(int w, float forma, float *ws, float *pw) {
    *pw = 0.5f;
    switch (w) {
    case AN_WAVE_SQR: *ws = 1.0f; *pw = 0.5f - 0.45f * forma; break;
    case AN_WAVE_TRI: *ws = 0.5f * forma; break;
    default: *ws = 0.5f + 0.5f * forma; break;         /* SIERRA: hacia cuadrada */
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
        z.o1.Init(); z.o2.Init(); z.oa.Init(); z.ob.Init(); z.sub.Init();
        z.f1.Init(); z.f2.Init();
        z.sph1 = z.sph2 = 0.0f;
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

int bm_an_render(int v, const uint8_t *prm, const uint8_t mac[4], float *out, int n)
{
    if (v < 0 || v >= BM_VOICES || n <= 0) return 0;
    Voice &z = V(v);
    if (z.ea.st == ST_OFF) { memset(out, 0, sizeof(float) * (size_t)n); return 0; }

    /* glide (por bloque, exponencial) */
    if (z.note != z.target) {
        float gt = prm[AN_GLIDE] ? bm_env_s(prm[AN_GLIDE]) : 0.0f;
        float k = gt > 0.0f ? 1.0f - expf(-(float)n / (gt * fs_ * 0.35f)) : 1.0f;
        z.note += (z.target - z.note) * k;
        if (fabsf(z.target - z.note) < 0.001f) z.note = z.target;
    }

    /* LFO: un valor por bloque, interpolado */
    int ldest = prm[AN_LDEST] % AN_LDESTS;
    float lamt = pc(prm[AN_LAMT]);
    float lhz = bm_lfo_hz(prm[AN_LRATE]);
    z.lph += lhz * (float)n / fs_;
    if (z.lph >= 1.0f) { z.lph -= (float)(int)z.lph; z.lsh = rnd(z.rng); }
    float lfo = lamt > 0.0f ? lfo_at(z, prm[AN_LSHAPE] % AN_LSHAPES) : 0.0f;

    float forma = pc(mac[3]);
    if (ldest == AN_D_SHAPE) { forma += 0.5f * lamt * lfo; CONSTRAIN(forma, 0.0f, 1.0f); }
    float pitch = z.note + (ldest == AN_D_PITCH ? lfo * lamt * lamt * 12.0f : 0.0f);
    float f1 = 440.0f * SemitonesToRatioSafe(pitch - 69.0f) / fs_;
    float semi2 = (float)(((int)prm[AN_SEMI2] - 64) * 24 / 63) + (float)((int)prm[AN_FINE2] - 64) * (0.5f / 63.0f);
    float penv = (float)((int)prm[AN_PENV2] - 64) * (48.0f / 63.0f) * z.ef.v;
    float f2 = f1 * SemitonesToRatioSafe(semi2 + penv);

    static float b1[BM_BLOCK], b2[BM_BLOCK], b3[BM_BLOCK];   /* solo la tarea de audio: fuera de la pila */
    int w1 = prm[AN_W1] % AN_WAVES, w2 = prm[AN_W2] % 4;
    float ws, pw;
    if (w1 == AN_WAVE_SIN) {
        float k = 1.0f + 4.0f * forma;
        for (int i = 0; i < n; i++) {
            z.sph1 += f1; if (z.sph1 >= 1.0f) z.sph1 -= 1.0f;
            float s = Sine(z.sph1);
            b1[i] = forma > 0.0f ? Sine(8.0f + 0.25f * k * s) : s;
        }
    } else if (w1 == AN_WAVE_SUPER) {
        float d = 0.002f + 0.03f * forma;
        z.o1.Render(f1, 0.5f, 0.5f, b1, (size_t)n);
        z.oa.Render(f1 * (1.0f + d), 0.5f, 0.5f, b2, (size_t)n);
        z.ob.Render(f1 * (1.0f - d * 0.93f), 0.5f, 0.5f, b3, (size_t)n);
        for (int i = 0; i < n; i++) b1[i] = (b1[i] + 0.75f * (b2[i] + b3[i])) * 0.6f;
    } else {
        shape_of(w1, forma, &ws, &pw);
        z.o1.Render(f1, pw, ws, b1, (size_t)n);
    }
    float mix2 = pc(prm[AN_MIX2]), ring = pc(prm[AN_RING]);
    bool need2 = mix2 > 0.0f || ring > 0.0f;
    if (need2) {
        if (w2 == AN_WAVE_SIN) {
            for (int i = 0; i < n; i++) { z.sph2 += f2; if (z.sph2 >= 1.0f) z.sph2 -= 1.0f; b2[i] = Sine(z.sph2); }
        } else {
            shape_of(w2, 0.0f, &ws, &pw);
            if (prm[AN_SYNC]) z.o2.Render(f1, f2, pw, ws, b2, (size_t)n);
            else z.o2.Render(f2, pw, ws, b2, (size_t)n);
        }
    }
    float subl = pc(prm[AN_SUB]);
    if (subl > 0.0f) z.sub.Render(f1 * 0.5f, 0.5f, 1.0f, b3, (size_t)n);
    float noise = pc(prm[AN_NOISE]);
    float drive = pc(prm[AN_DRIVE]);
    float pre = 0.7f + 2.5f * drive * drive;

    /* filtro: corte en semitonos sobre 20 Hz */
    float cut = (float)mac[0] * (120.0f / 127.0f)
              + (float)((int)mac[2] - 64) * (84.0f / 63.0f) * z.ef.v
              + pc(prm[AN_KEYT]) * (z.note - 60.0f)
              + pc(prm[AN_VELF]) * z.vel * 30.0f
              + (ldest == AN_D_CUT ? lfo * lamt * 48.0f : 0.0f);
    if (z.first) { z.cut_prev = cut; z.first = 0; }
    float q = bm_reso_q(mac[1]);
    int ft = prm[AN_FTYPE] % AN_FILTERS;
    float c0 = z.cut_prev, dc = (cut - c0) / (float)n;
    z.cut_prev = cut;

    /* envolventes: coeficientes del bloque */
    float aka = ka_of(bm_env_s(prm[AN_AA])), akd = kx_of(bm_env_s(prm[AN_AD])), akr = kx_of(bm_env_s(prm[AN_AR]));
    float fka = ka_of(bm_env_s(prm[AN_FA])), fkd = kx_of(bm_env_s(prm[AN_FD])), fkr = kx_of(bm_env_s(prm[AN_FR]));
    float as = pc(prm[AN_AS]), fsus = pc(prm[AN_FS]);
    float velg = 1.0f - pc(prm[AN_VELA]) * (1.0f - z.vel);
    float trem = ldest == AN_D_AMP ? lamt * 0.5f : 0.0f;
    float g = velg * (1.0f - trem + trem * lfo) * 1.5f;
    float inv_fs = 1.0f / fs_;

    for (int i = 0; i < n; i++) {
        float x = b1[i];
        if (need2) x += mix2 * b2[i] + ring * b1[i] * b2[i];
        if (subl > 0.0f) x += subl * b3[i];
        if (noise > 0.0f) x += noise * rnd(z.rng);
        x = SoftClip(x * pre);
        float cs = c0 + dc * (float)(i + 1);
        float f = 20.0f * SemitonesToRatioSafe(cs) * inv_fs;
        CONSTRAIN(f, 0.0003f, 0.36f);
        float y;
        switch (ft) {
        case AN_F_LP12: z.f1.set_f_q<FREQUENCY_FAST>(f, q); y = z.f1.Process<FILTER_MODE_LOW_PASS>(x); break;
        case AN_F_BP: z.f1.set_f_q<FREQUENCY_FAST>(f, q); y = z.f1.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(x) * 1.5f; break;
        case AN_F_HP: z.f1.set_f_q<FREQUENCY_FAST>(f, q); y = z.f1.Process<FILTER_MODE_HIGH_PASS>(x); break;
        default:
            z.f1.set_f_q<FREQUENCY_FAST>(f, q);
            z.f2.set_f_q<FREQUENCY_FAST>(f, 0.5f + 0.02f * q);
            y = z.f2.Process<FILTER_MODE_LOW_PASS>(SoftClip(z.f1.Process<FILTER_MODE_LOW_PASS>(x)));
            break;
        }
        if (drive > 0.0f) y = SoftClip(y * (1.0f + drive));
        float ea = z.ea.Next(aka, akd, as, akr);
        z.ef.Next(fka, fkd, fsus, fkr);
        out[i] = y * ea * g;
    }
    return z.ea.st != ST_OFF;
}

}  // extern "C"
