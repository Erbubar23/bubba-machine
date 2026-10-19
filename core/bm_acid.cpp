/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23
 *
 * Portions adapted from Open303 (https://github.com/RobinSchmidt/Open303, commit 313bf0d, files
 * rosic_Open303.*, rosic_TeeBeeFilter.*, rosic_AnalogEnvelope.*, rosic_DecayEnvelope.*,
 * rosic_OnePoleFilter.*, rosic_BiquadFilter.*, rosic_LeakyIntegrator.*):
 *   Copyright (c) 2009 Robin Schmidt (www.rs-met.com)
 *   Released under the MIT license; the full notice is in third_party/open303/LICENSE.
 * What comes from there: the TB-303 filter (TeeBeeFilter in TB_303 mode, its coefficient fits and
 * the 150 Hz high-pass in its feedback), the measured CUTOFF / ENV MOD mapping, the accent circuit
 * (a 15 ms leaky integrator of the main envelope, added to the cutoff on accented notes, and the
 * main envelope added to the amp envelope), the fixed accent decay of 200 ms, the amp envelope
 * (instant attack, 1.23 s decay, 0.5 ms / 50 ms release) and its 200 Hz declicker, the pitch slew
 * of the slide, and the fixed filters around the VCF (44.5 Hz high-pass before, 24.2 Hz high-pass
 * and a 7.5 Hz notch after), with Open303's values. Changes: float, one sample per output sample
 * (no 4x oversampling: the cutoff is capped at 10 kHz), filter coefficients every 8 samples, a
 * polyBLEP saw/square instead of the mip-mapped tables, no allpass (phase only), a soft limit at
 * the output and the knob ranges of the TB-303 on our 0..127 macros. */
#include <math.h>
#include <string.h>
#include <new>
#include "bm_acid.h"
#include "bm_config.h"
#include "bm_vpool.h"
extern "C" {
#include "bm_util.h"
}

#include "stmlib/dsp/units.h"

using namespace stmlib;

namespace {

float fs_ = 44118.0f;
int ready;                                    /* the coefficients below are computed */

/* the fixed filters (Open303's "tweakables"), coefficients from bm_acid_init */
struct OnePoleHp { float b0, a1; };           /* rosic OnePoleFilter HIGHPASS: y = b0 (x - x1) + a1 y1 */
OnePoleHp hp_pre, hp_post, hp_fb;
float n_b0, n_b1, n_a1, n_a2;                 /* notch (BANDREJECT), b2 = b0 */
float d_b0, d_a1, d_a2;                       /* declicker LP12 at 200 Hz, b1 = 2 b0, b2 = b0 */
float k_dec, k_rc2, k_rel_n, k_rel_a;         /* amp decay 1230 ms, accent RC 15 ms, releases 0.5 / 50 ms */
float c_acc;                                  /* main envelope of an accented note: 200 ms */

const float CUT_LO = 313.8152786f, CUT_HI = 2394.411987f;   /* Open303's measured cutoff range */
const float CUT_MAX = 10000.0f;               /* 1x: the filter tracks and stays stable up to here */
const float GAIN = 0.55f;                     /* to the engine scale (about the old BB-303 level in the demo) */

struct Acid {
    float ph, freq, ftgt;          /* oscillator: phase 0..1, cycles per sample now and the target (slide) */
    float menv, rc2, amp;          /* main envelope (MEG), accent RC, amp envelope */
    float dz1, dz2;                /* declicker (transposed direct form II) */
    float y1, y2, y3, y4;          /* TB-303 filter */
    float fbx, fby;                /* the high-pass in its feedback */
    float h1x, h1y, h2x, h2y;      /* high-pass before and after the filter */
    float nz1, nz2;                /* notch */
    float b0, k, g;                /* filter coefficients (every 8 samples) */
    float acc, lvl;                /* accent gain of the note (0 = none), level for quiet notes */
    float mc, kslew, scaler, offset, cut_hz, rskew;
    uint8_t key[4];                /* the knobs the cached coefficients came from */
    uint8_t gate, accent, fresh;
};
static_assert(sizeof(Acid) <= BM_VPOOL_BYTES && alignof(Acid) <= 8, "BB-303 voice does not fit the shared pool (bm_vpool.h)");

inline Acid &A(int v) { return *reinterpret_cast<Acid *>(bm_vpool(v)); }
inline bool mine(int v) { return bm_vpool_is(v, VP_ACID) != 0; }

inline float one_minus_exp(float samples) { return 1.0f - expf(-1.0f / samples); }
inline float ftz(float x) { return (x > -1e-15f && x < 1e-15f) ? 0.0f : x; }

OnePoleHp make_hp(float hz)
{
    float x = expf(-6.2831853f * hz / fs_);
    OnePoleHp h = {0.5f * (1.0f + x), x};
    return h;
}

inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

float cut_hz_of(int v) { return CUT_LO * powf(CUT_HI / CUT_LO, (float)v * (1.0f / 127.0f)); }
float decay_ms_of(int v) { return 200.0f * powf(10.0f, (float)v * (1.0f / 127.0f)); }
float slide_ms_of(int v) { return (float)(v + 8) * (1000.0f / 254.0f); }

/* Open303::calculateEnvModScalerAndOffset (the measured mapping) */
void env_mapping(Acid &z, int cut, int env)
{
    float e = (float)env * (1.0f / 127.0f), c = (float)cut * (1.0f / 127.0f);
    float sLo = 3.773996325f * e + 0.736965594f, sHi = 4.194548788f * e + 0.864344901f;
    z.scaler = (1.0f - c) * sLo + c * sHi;
    z.offset = 0.048292931f * c + 0.294391201f;
    z.cut_hz = cut_hz_of(cut);
}

/* TeeBeeFilter::calculateCoefficientsApprox4, TB_303 branch */
inline void filter_coefs(Acid &z, float hz)
{
    if (hz < 200.0f) hz = 200.0f;
    if (hz > CUT_MAX) hz = CUT_MAX;
    float fx = hz * (0.70710678f / fs_);
    z.b0 = (0.00045522346f + 6.1922189f * fx) / (1.0f + 12.358354f * fx + 4.4156345f * (fx * fx));
    float k = fx * (fx * (fx * (fx * (fx * (fx + 7198.6997f) - 5837.7917f) - 476.47308f) + 614.95611f) + 213.87126f) + 16.998792f;
    float g = k * (1.0f / 17.0f);
    g = (g - 1.0f) * z.rskew + 1.0f;
    z.g = g * (1.0f + z.rskew);
    z.k = k * z.rskew;
}

/* the cached coefficients: exp() only when a knob moved (it is slow on the FM-1) */
void coefs(Acid &z, const bm_acid_par *p)
{
    if (z.fresh || z.key[0] != p->cut || z.key[1] != p->env) { z.key[0] = p->cut; z.key[1] = p->env; env_mapping(z, p->cut, p->env); }
    if (z.fresh || z.key[2] != p->res) {
        z.key[2] = p->res;
        float r = (float)p->res * (1.0f / 127.0f);
        z.rskew = (1.0f - expf(-3.0f * r)) * (1.0f / (1.0f - 0.049787068f));   /* TeeBeeFilter::setResonance */
    }
    uint8_t dk = (uint8_t)(z.accent ? 0xFF : p->dec);
    if (z.fresh || z.key[3] != dk) {
        z.key[3] = dk;
        z.mc = z.accent ? c_acc : expf(-1.0f / (0.001f * decay_ms_of(p->dec) * fs_));
    }
    z.kslew = one_minus_exp(0.0002f * slide_ms_of(p->slide) * fs_);     /* Open303: tau = 0.2 x slide time */
    z.fresh = 0;
}

inline float soft_limit(float x)          /* linear up to 0.8, then bends towards ~1.4 */
{
    float a = x < 0.0f ? -x : x;
    if (a <= 0.8f) return x;
    float y = 0.8f + (a - 0.8f) / (1.0f + 1.6f * (a - 0.8f));
    return x < 0.0f ? -y : y;
}

void reset_state(Acid &z)
{
    z.y1 = z.y2 = z.y3 = z.y4 = 0.0f; z.fbx = z.fby = 0.0f;
    z.h1x = z.h1y = z.h2x = z.h2y = 0.0f; z.nz1 = z.nz2 = 0.0f;
    z.dz1 = z.dz2 = 0.0f; z.amp = 0.0f; z.menv = 0.0f; z.rc2 = 0.0f;
}

}  // namespace

extern "C" {

float bm_acid_cut_hz(int v) { return cut_hz_of(v < 0 ? 0 : v > 127 ? 127 : v); }
float bm_acid_decay_ms(int v) { return decay_ms_of(v < 0 ? 0 : v > 127 ? 127 : v); }
float bm_acid_slide_ms(int v) { return slide_ms_of(v < 0 ? 0 : v > 127 ? 127 : v); }

void bm_acid_init(float fs)
{
    fs_ = fs;
    hp_pre = make_hp(44.486f); hp_post = make_hp(24.167f); hp_fb = make_hp(150.0f);
    {   /* notch: 7.5164 Hz, 4.7 octaves wide (RBJ band-reject) */
        float w = 6.2831853f * 7.5164f / fs_, s = sinf(w), c = cosf(w);
        float alpha = s * sinhf(0.5f * 0.69314718f * 4.7f * w / s), sc = 1.0f / (1.0f + alpha);
        n_b0 = sc; n_b1 = -2.0f * c * sc; n_a1 = 2.0f * c * sc; n_a2 = (alpha - 1.0f) * sc;
    }
    {   /* declicker: LP12 at 200 Hz, Q 0.707 (RBJ) */
        float w = 6.2831853f * 200.0f / fs_, s = sinf(w), c = cosf(w);
        float alpha = s / (2.0f * 0.70710678f), sc = 1.0f / (1.0f + alpha);
        d_a1 = 2.0f * c * sc; d_a2 = (alpha - 1.0f) * sc; d_b0 = 0.5f * (1.0f - c) * sc;
    }
    k_dec = one_minus_exp(1.230f * fs_);
    k_rc2 = one_minus_exp(0.015f * fs_);
    k_rel_n = one_minus_exp(0.0005f * fs_);
    k_rel_a = one_minus_exp(0.050f * fs_);
    c_acc = expf(-1.0f / (0.200f * fs_));
    ready = 1;
}

void bm_acid_start(int v, int note, int accent, int legato, float lvl, const bm_acid_par *p)
{
    if (!bm_vpool_ok(v)) return;
    if (!ready) bm_acid_init((float)BM_FS);
    if (!bm_vpool_take(v, VP_ACID) || legato == 2) {     /* a new voice: from scratch (the same every time) */
        Acid *n = new (bm_vpool(v)) Acid();
        memset(n, 0, sizeof *n);
        legato = 0;
    }
    Acid &z = A(v);
    if (note < 0) note = 0;
    if (note > 127) note = 127;
    float f = (float)bm_note_inc[note] * (1.0f / 4294967296.0f);
    bool idle = z.amp < 1e-4f && !z.gate;
    z.accent = (uint8_t)(accent != 0);
    z.acc = accent ? (float)p->acc * (1.0f / 127.0f) : 0.0f;
    z.lvl = lvl;
    z.fresh = 1;                          /* the decay (accented or not) and the rest: again */
    coefs(z, p);
    z.ftgt = f;
    if (legato && z.gate) return;         /* slide: same envelopes, the pitch glides */
    if (idle) { reset_state(z); z.ph = 0.0f; }   /* Open303: reset only when silent (no clicks) */
    z.freq = f;
    z.menv = 1.0f;                        /* DecayEnvelope::trigger */
    z.amp = 1.0f;                         /* attack 0: at the peak at once (the declicker smooths it) */
    z.gate = 1;
}

void bm_acid_release(int v)
{
    if (!mine(v)) return;
    A(v).gate = 0;
}

float bm_acid_level(int v) { return mine(v) ? A(v).amp : 0.0f; }

int bm_acid_render(int v, const bm_acid_par *p, float *out, int n)
{
    if (n <= 0) return 0;
    if (!mine(v)) { memset(out, 0, sizeof(float) * (size_t)n); return 0; }
    Acid &z = A(v);
    coefs(z, p);
    const bool sq = (p->wave & 1) != 0;
    const float krel = z.accent ? k_rel_a : k_rel_n, kslew = z.kslew;
    const float ampenv = 0.45f + 4.0f * z.acc;        /* the MEG into the VCA (more with an accent) */
    float gout = GAIN * z.lvl, dlast = 0.0f;
    for (int i0 = 0; i0 < n; i0 += 8) {
        int i1 = i0 + 8 < n ? i0 + 8 : n;
        /* the cutoff: nominal x 2^(ENV MOD part + accent part), Open303::getSample */
        float oct = z.scaler * (z.menv - z.offset) + z.acc * z.rc2;
        filter_coefs(z, z.cut_hz * SemitonesToRatioSafe(12.0f * oct));
        const float b0 = z.b0, b2 = 2.0f * z.b0, k = z.k, g2 = 2.0f * z.g;
        for (int i = i0; i < i1; i++) {
            /* pitch slew (the slide) and the oscillator, polyBLEP */
            z.freq += kslew * (z.ftgt - z.freq);
            float dt = z.freq;
            float o = 2.0f * z.ph - 1.0f - blep(z.ph, dt);
            if (sq) {
                float p2 = z.ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                o = (z.ph < 0.5f ? 1.0f : -1.0f) + blep(z.ph, dt) - blep(p2, dt);
            }
            z.ph += dt; if (z.ph >= 1.0f) z.ph -= 1.0f;
            float x = -o;
            /* high-pass before the filter */
            z.h1y = hp_pre.b0 * (x - z.h1x) + hp_pre.a1 * z.h1y; z.h1x = x; x = z.h1y;
            /* TeeBeeFilter, TB_303 mode, with the high-pass in its feedback */
            float fb = k * z.y4;
            z.fby = hp_fb.b0 * (fb - z.fbx) + hp_fb.a1 * z.fby; z.fbx = fb;
            float y0 = x - z.fby;
            z.y1 += b2 * (y0 - z.y1 + z.y2);
            z.y2 += b0 * (z.y1 - 2.0f * z.y2 + z.y3);
            z.y3 += b0 * (z.y2 - 2.0f * z.y3 + z.y4);
            z.y4 += b0 * (z.y3 - 2.0f * z.y4);
            float y = g2 * z.y4;
            /* high-pass and notch after it */
            z.h2y = hp_post.b0 * (y - z.h2x) + hp_post.a1 * z.h2y; z.h2x = y; y = z.h2y;
            float nn = n_b0 * y + z.nz1;
            z.nz1 = n_b1 * y + n_a1 * nn + z.nz2;
            z.nz2 = n_b0 * y + n_a2 * nn;
            y = nn;
            /* envelopes: MEG, accent RC, amp (decays while held, fast release), declicker */
            float me = z.menv;
            z.rc2 += k_rc2 * ((z.acc > 0.0f ? me : 0.0f) - z.rc2);
            float a;
            if (z.gate) { z.amp -= k_dec * z.amp; a = z.amp + ampenv * me; }
            else { z.amp -= krel * z.amp; a = z.amp; }
            z.menv = me * z.mc;
            float d = d_b0 * a + z.dz1;
            z.dz1 = 2.0f * d_b0 * a + d_a1 * d + z.dz2;
            z.dz2 = d_b0 * a + d_a2 * d;
            dlast = d;
            out[i] = soft_limit(y * d * gout);
        }
    }
    /* NaN/inf in a state stays forever: once per block, back to zero and say so */
    float s = z.y1 + z.y2 + z.y3 + z.y4 + z.fby + z.h1y + z.h2y + z.nz1 + z.nz2 + z.dz1 + z.dz2 + z.amp + z.menv + z.rc2 + z.freq;
    if (!(s > -1e9f && s < 1e9f) || !(out[n - 1] > -1e4f && out[n - 1] < 1e4f)) {
        memset(out, 0, sizeof(float) * (size_t)n);
        reset_state(z);
        z.gate = 0; z.freq = z.ftgt = 0.001f;
        return -1;
    }
    z.y1 = ftz(z.y1); z.y2 = ftz(z.y2); z.y3 = ftz(z.y3); z.y4 = ftz(z.y4); z.fby = ftz(z.fby); z.fbx = ftz(z.fbx);
    z.h1y = ftz(z.h1y); z.h2y = ftz(z.h2y); z.nz1 = ftz(z.nz1); z.nz2 = ftz(z.nz2);
    z.dz1 = ftz(z.dz1); z.dz2 = ftz(z.dz2); z.menv = ftz(z.menv); z.rc2 = ftz(z.rc2);
    /* the end: released (or held until the amp decayed) and the declicked level is down at -80 dB */
    float lvl = dlast < 0.0f ? -dlast : dlast;
    if (z.amp < 1e-4f && lvl < 1e-4f && (!z.gate || z.menv < 1e-4f)) { reset_state(z); z.gate = 0; return 0; }
    return 1;
}

}  // extern "C"
