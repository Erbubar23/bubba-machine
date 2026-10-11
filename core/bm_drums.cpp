/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — batería modelada (core/bm_drums.h). Los modelos de Plaits se construyen con
 * placement new en el espacio de cada voz al golpe; no hay memoria dinámica ni constructores
 * globales. Plaits calcula a kSampleRate = PLAITS_SAMPLE_RATE (lo fija el build en BM_FS). */
#include <new>
#include <string.h>
#include "bm_drums.h"
#include "bm_config.h"

#include "plaits/dsp/drums/analog_bass_drum.h"
#include "plaits/dsp/drums/analog_snare_drum.h"
#include "plaits/dsp/drums/hi_hat.h"
#include "plaits/dsp/drums/synthetic_bass_drum.h"
#include "plaits/dsp/drums/synthetic_snare_drum.h"
#include "plaits/dsp/fx/overdrive.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/utils/random.h"

using namespace plaits;
using namespace stmlib;

extern "C" {
const char *const bm_drm_names[DRM_COUNT] = {"BOMBO ANALOG", "BOMBO SINTE", "CAJA ANALOG", "CAJA SINTE", "HAT ANALOG",
                                             "HAT METAL", "PALMAS", "TOM", "CENCERRO", "RESONADOR", "RUIDO", "FM"};
const char *const bm_drm_short[DRM_COUNT] = {"BD808", "BD909", "SD808", "SD909", "HH808", "METAL", "CLAP", "TOM",
                                             "COWBL", "RESON", "RUIDO", "FM"};
const char *const bm_drm_color[DRM_COUNT] = {"GOLPE", "GOLPE", "BORDONA", "BORDONA", "RUIDO", "RUIDO", "ESPACIO",
                                             "RUIDO", "MEZCLA", "RELACION", "ATAQUE", "RELACION"};
const uint16_t bm_drm_base_hz[DRM_COUNT] = {50, 55, 180, 200, 160, 220, 1100, 120, 540, 1700, 4000, 110};
}

static float fs_ = 44118.0f;

/* ------------------------------------------------------------ modelos propios */
static inline float noise() { return Random::GetFloat() * 2.0f - 1.0f; }
static inline float decay_coef(float seconds, float fs) { return 1.0f - 1.0f / (seconds * fs + 1.0f); }

/* Palmas 808: tres ráfagas cortas de ruido pasa-banda y una cola más larga. COLOR separa las
 * ráfagas (de palmas juntas a un aplauso abierto). */
struct Clap {
    Svf bp; float env, tail, kb, kt; uint32_t t, gap;
    void Init(float hz, float tone, float decay, float color, float accent) {
        bp.Init();
        bp.set_f_q<FREQUENCY_ACCURATE>(hz * (0.6f + 1.4f * tone) / fs_, 1.6f + tone);
        env = 0.0f; tail = 0.0f; t = 0;
        gap = (uint32_t)((0.006f + 0.012f * color) * fs_);
        kb = decay_coef(0.0035f, fs_);
        kt = decay_coef(0.05f + 0.6f * decay * decay, fs_);
        (void)accent;
    }
    void Render(float *out, int n) {
        for (int i = 0; i < n; i++, t++) {
            if (t < 3 * gap && t % gap == 0) env = 1.0f;
            if (t == 3 * gap) tail = 0.55f;
            env *= kb; tail *= kt;
            out[i] = bp.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(noise()) * (env + tail) * 1.6f;
        }
    }
};

/* Cencerro 808: dos cuadradas (540 y 800 Hz en el original) por un pasa-banda, con una caída
 * rápida al principio y después la cola. COLOR mezcla la segunda cuadrada. */
struct Cowbell {
    Svf bp; float ph1, ph2, f1, f2, env, k1, k2, mix;
    void Init(float hz, float tone, float decay, float color, float accent) {
        bp.Init();
        f1 = hz / fs_; f2 = hz * 1.48f / fs_;
        bp.set_f_q<FREQUENCY_ACCURATE>(hz * (1.2f + 3.0f * tone) / fs_, 2.2f);
        ph1 = ph2 = 0.0f; env = 1.0f; mix = 0.3f + 0.7f * color;
        k1 = decay_coef(0.012f, fs_);
        k2 = decay_coef(0.04f + 0.5f * decay * decay, fs_);
        (void)accent;
    }
    void Render(float *out, int n) {
        for (int i = 0; i < n; i++) {
            ph1 += f1; if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += f2; if (ph2 >= 1.0f) ph2 -= 1.0f;
            float s = (ph1 < 0.5f ? 0.5f : -0.5f) + mix * (ph2 < 0.5f ? 0.5f : -0.5f);
            env *= env > 0.35f ? k1 : k2;
            out[i] = bp.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(s) * env * 1.4f;
        }
    }
};

/* Dos resonadores golpeados: rim (con saturación), clave o bloque. COLOR = relación entre los
 * dos modos; TONO = cuánto suena el segundo y lo seco del golpe; DECAY = la Q. */
struct Reson {
    Svf r1, r2; float g2, drive; uint32_t t, plen;
    void Init(float hz, float tone, float decay, float color, float accent) {
        float q = 4.0f + 90.0f * decay * decay;
        float ratio = 1.5f + 3.0f * color;
        r1.Init(); r2.Init();
        r1.set_f_q<FREQUENCY_ACCURATE>(hz / fs_ > 0.45f ? 0.45f : hz / fs_, q);
        float f2 = hz * ratio / fs_;
        r2.set_f_q<FREQUENCY_ACCURATE>(f2 > 0.45f ? 0.45f : f2, q * 0.8f);
        g2 = 0.2f + 0.8f * tone; drive = 1.0f + 3.0f * tone * accent; t = 0;
        plen = (uint32_t)(0.5f * fs_ / hz) + 2u;           /* golpe de medio ciclo: misma energía en todo el rango */
    }
    void Render(float *out, int n) {
        for (int i = 0; i < n; i++, t++) {
            float x = t < plen ? 1.0f : (t < plen + (uint32_t)(0.0008f * fs_) ? 0.2f * noise() : 0.0f);
            float y = r1.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(x) + g2 * r2.Process<FILTER_MODE_BAND_PASS_NORMALIZED>(x);
            out[i] = SoftClip(y * drive * 2.5f);
        }
    }
};

/* Ruido filtrado: TONO barre de pasa-bajos (abajo) a pasa-altos (arriba); COLOR = ataque. */
struct Noise {
    Svf f; float env, katk, kdec; int hp; uint8_t rising;
    void Init(float hz, float tone, float decay, float color, float accent) {
        f.Init();
        hp = tone >= 0.5f;
        float x = hp ? (tone - 0.5f) * 2.0f : tone * 2.0f;
        float cut = hp ? 1500.0f * SemitonesToRatio(x * 36.0f) : 300.0f * SemitonesToRatio(x * 48.0f);
        cut *= hz / 4000.0f;
        f.set_f_q<FREQUENCY_ACCURATE>(cut / fs_ > 0.45f ? 0.45f : cut / fs_, 0.9f);
        env = 0.0f; rising = 1;
        katk = 1.0f / ((0.0005f + 0.04f * color * color) * fs_ + 1.0f);
        kdec = decay_coef(0.02f + 0.7f * decay * decay, fs_);
        (void)accent;
    }
    void Render(float *out, int n) {
        for (int i = 0; i < n; i++) {
            if (rising) { env += katk; if (env >= 1.0f) { env = 1.0f; rising = 0; } }
            else env *= kdec;
            float s = noise();
            out[i] = (hp ? f.Process<FILTER_MODE_HIGH_PASS>(s) : f.Process<FILTER_MODE_LOW_PASS>(s)) * env;
        }
    }
};

/* FM de 2 operadores con barrido de tono: COLOR = relación del modulador, TONO = índice. */
struct FmPerc {
    float pc, pm, f0, ratio, index, env, kenv, sweep, ksw;
    void Init(float hz, float tone, float decay, float color, float accent) {
        static const float R[8] = {0.5f, 1.0f, 1.41f, 2.0f, 2.76f, 3.5f, 5.19f, 7.0f};
        pc = pm = 0.0f; f0 = hz / fs_;
        ratio = R[(int)(color * 7.99f)];
        index = 6.0f * tone * tone * (0.5f + 0.5f * accent);
        env = 1.0f; kenv = decay_coef(0.02f + 0.8f * decay * decay, fs_);
        sweep = 3.0f; ksw = decay_coef(0.012f, fs_);
    }
    void Render(float *out, int n) {
        for (int i = 0; i < n; i++) {
            float f = f0 * (1.0f + sweep);
            if (f > 0.45f) f = 0.45f;
            pm += f * ratio; if (pm >= 1.0f) pm -= 1.0f;
            pc += f; if (pc >= 1.0f) pc -= 1.0f;
            float m = Sine(pm) * index * env;
            float p = pc + m; p -= (float)(int)p; if (p < 0.0f) p += 1.0f;
            out[i] = Sine(p) * env;
            env *= kenv; sweep *= ksw;
        }
    }
};

/* ------------------------------------------------------------ voces */
typedef HiHat<SquareNoise, SwingVCA, true, false> Hat808;
typedef HiHat<RingModNoise, LinearVCA, false, true> HatMetal;

template <size_t A, size_t B> struct MaxOf { static const size_t v = A > B ? A : B; };
static const size_t kModelBytes =
    MaxOf<MaxOf<MaxOf<sizeof(AnalogBassDrum), sizeof(SyntheticBassDrum)>::v, MaxOf<sizeof(AnalogSnareDrum), sizeof(SyntheticSnareDrum)>::v>::v,
          MaxOf<MaxOf<sizeof(Hat808), sizeof(HatMetal)>::v, MaxOf<MaxOf<sizeof(Clap), sizeof(Cowbell)>::v,
                                                                 MaxOf<sizeof(Reson), MaxOf<sizeof(Noise), sizeof(FmPerc)>::v>::v>::v>::v>::v;

struct DVoice {
    uint8_t model, trig;
    float f0, tone, decay, color, accent, gain;
    uint32_t t;
    uint8_t quiet;
    alignas(8) unsigned char odm[sizeof(Overdrive)];   /* sin constructores globales */
    alignas(8) unsigned char mem[kModelBytes];
};
static DVoice dv_[BM_VOICES];

/* Nivel de cada modelo, para que todos peguen parecido con los valores del kit (tests/test_core.c). */
static const float kGain[DRM_COUNT] = {2.0f, 1.0f, 1.0f, 1.0f, 1.6f, 1.1f, 1.8f, 1.5f, 1.0f, 1.0f, 1.0f, 1.0f};

void bm_drm_init(float fs)
{
    fs_ = fs;
    memset(dv_, 0, sizeof dv_);
}

void bm_drm_trigger(int v, int model, float hz, float tone, float decay, float color, float accent)
{
    if (v < 0 || v >= BM_VOICES) return;
    DVoice &d = dv_[v];
    d.model = (uint8_t)(model < 0 || model >= DRM_COUNT ? 0 : model);
    d.f0 = hz / fs_; d.tone = tone; d.decay = decay; d.color = color; d.accent = accent;
    d.gain = kGain[d.model];
    d.trig = 1; d.t = 0; d.quiet = 0;
    new (d.odm) Overdrive(); reinterpret_cast<Overdrive *>(d.odm)->Init();
    void *m = d.mem;
    switch (d.model) {
    case DRM_BD_ANALOG: new (m) AnalogBassDrum(); static_cast<AnalogBassDrum *>(m)->Init(); break;
    case DRM_BD_SINTE: new (m) SyntheticBassDrum(); static_cast<SyntheticBassDrum *>(m)->Init(); break;
    case DRM_SD_ANALOG: case DRM_TOM: new (m) AnalogSnareDrum(); static_cast<AnalogSnareDrum *>(m)->Init(); break;
    case DRM_SD_SINTE: new (m) SyntheticSnareDrum(); static_cast<SyntheticSnareDrum *>(m)->Init(); break;
    case DRM_HH_ANALOG: new (m) Hat808(); static_cast<Hat808 *>(m)->Init(); break;
    case DRM_HH_METAL: new (m) HatMetal(); static_cast<HatMetal *>(m)->Init(); break;
    case DRM_CLAP: new (m) Clap(); static_cast<Clap *>(m)->Init(hz, tone, decay, color, accent); break;
    case DRM_COWBELL: new (m) Cowbell(); static_cast<Cowbell *>(m)->Init(hz, tone, decay, color, accent); break;
    case DRM_RESON: new (m) Reson(); static_cast<Reson *>(m)->Init(hz, tone, decay, color, accent); break;
    case DRM_NOISE: new (m) Noise(); static_cast<Noise *>(m)->Init(hz, tone, decay, color, accent); break;
    case DRM_FM: new (m) FmPerc(); static_cast<FmPerc *>(m)->Init(hz, tone, decay, color, accent); break;
    }
}

int bm_drm_render(int v, float *out, int n)
{
    if (v < 0 || v >= BM_VOICES || n <= 0) return 0;
    DVoice &d = dv_[v];
    bool tr = d.trig != 0;
    d.trig = 0;
    void *m = d.mem;
    static float t1[BM_BLOCK], t2[BM_BLOCK];           /* solo la tarea de audio llama aquí: fuera de la pila */
    switch (d.model) {
    case DRM_BD_ANALOG: {
        float fm = d.color * 2.0f;
        static_cast<AnalogBassDrum *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay,
                                                 fm < 1.0f ? fm : 1.0f, fm > 1.0f ? fm - 1.0f : 0.0f, out, (size_t)n);
        float drive = d.color > 0.5f ? (d.color - 0.5f) * 2.0f : 0.0f;
        if (drive > 0.0f) reinterpret_cast<Overdrive *>(d.odm)->Process(0.5f + 0.5f * drive, out, (size_t)n);
        break;
    }
    case DRM_BD_SINTE:
        static_cast<SyntheticBassDrum *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay,
                                                    d.color > 0.5f ? (d.color - 0.5f) * 2.0f : 0.0f,
                                                    d.color * 2.0f < 1.0f ? d.color * 2.0f : 1.0f, 0.3f, out, (size_t)n);
        break;
    case DRM_SD_ANALOG:
        static_cast<AnalogSnareDrum *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay, d.color, out, (size_t)n);
        break;
    case DRM_TOM:
        static_cast<AnalogSnareDrum *>(m)->Render(false, tr, d.accent, d.f0, d.tone * 0.6f, d.decay, d.color * 0.35f, out, (size_t)n);
        break;
    case DRM_SD_SINTE:
        static_cast<SyntheticSnareDrum *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay, d.color, out, (size_t)n);
        break;
    case DRM_HH_ANALOG:
        static_cast<Hat808 *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay, d.color, t1, t2, out, (size_t)n);
        break;
    case DRM_HH_METAL:
        static_cast<HatMetal *>(m)->Render(false, tr, d.accent, d.f0, d.tone, d.decay, d.color, t1, t2, out, (size_t)n);
        break;
    case DRM_CLAP: static_cast<Clap *>(m)->Render(out, n); break;
    case DRM_COWBELL: static_cast<Cowbell *>(m)->Render(out, n); break;
    case DRM_RESON: static_cast<Reson *>(m)->Render(out, n); break;
    case DRM_NOISE: static_cast<Noise *>(m)->Render(out, n); break;
    case DRM_FM: static_cast<FmPerc *>(m)->Render(out, n); break;
    }
    /* nivel por el acento (los modelos de Plaits ya pegan más fuerte con él) y fin del golpe:
     * pasados 20 ms, tres bloques seguidos por debajo de -90 dB */
    float g = d.gain * (0.45f + 0.55f * d.accent), pk = 0.0f;
    for (int i = 0; i < n; i++) {
        out[i] *= g;
        float a = out[i] < 0.0f ? -out[i] : out[i];
        if (a > pk) pk = a;
    }
    d.t += (uint32_t)n;
    if (d.t > (uint32_t)(0.02f * fs_) && pk < 3.0e-5f) { if (++d.quiet >= 3) return 0; } else d.quiet = 0;
    return d.t < (uint32_t)(8.0f * fs_);
}
