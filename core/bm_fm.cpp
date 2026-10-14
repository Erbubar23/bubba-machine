/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — envoltorio de msfa (FM de 6 operadores) para el motor FM.
 *
 * Sin memoria dinámica ni constructores globales (el firmware no corre los de C++): los objetos
 * se construyen en memoria reservada la primera vez. Cada voz tiene su nota, su LFO (sincronizado
 * al tocar) y un buffer de 64 muestras, porque msfa calcula siempre de a 64 y el motor de Bubba
 * pide tramos más cortos cuando un paso cae dentro del bloque. */
#include <new>
#include <string.h>
#include "bm_config.h"
#include "bm_fm.h"
#include "synth.h"
#include "sin.h"
#include "exp2.h"
#include "freqlut.h"
#include "env.h"
#include "pitchenv.h"
#include "porta.h"
#include "lfo.h"
#include "controllers.h"
#include "dx7note.h"
#include "EngineMsfa.h"
#include "fm_op_kernel.h"

namespace {

struct Voz {
    Dx7Note note;
    Lfo lfo;
    int32_t buf[_N_];
    uint8_t pos;          /* próxima muestra de buf a entregar; _N_ = hay que calcular */
    uint8_t suelta;       /* ya se soltó la tecla */
    uint8_t alg;
    int8_t track;
    int32_t gain;         /* Q8: compensa cuántas portadoras suman */
};

/* nivel parejo entre algoritmos: una portadora suena a 1, seis a ~0.4 (≈ 1/raíz de n) */
int32_t gain_for(int alg)
{
    static const int16_t G[7] = {256, 256, 192, 154, 128, 115, 102};
    uint8_t c = FmCore::get_carrier_operators((uint8_t)(alg & 31));
    int n = 0;
    for (int i = 0; i < 6; i++) n += (c >> i) & 1;
    return G[n];
}

alignas(8) unsigned char mem_voz[BM_VOICES][sizeof(Voz)];
alignas(8) unsigned char mem_ctl[BM_TRACKS][sizeof(Controllers)];
alignas(8) unsigned char mem_core[sizeof(EngineMsfa)];
bool listo;

inline Voz &voz(int v) { return *reinterpret_cast<Voz *>(mem_voz[v]); }
inline Controllers &ctl(int t) { return *reinterpret_cast<Controllers *>(mem_ctl[t]); }

}  // namespace

#ifdef BM_NO_CXXLIB
/* Sin biblioteca de C++ (FM-1): lo que pide el compilador por las clases con funciones virtuales.
 * Nunca se llaman: no hay memoria dinámica y nada se destruye. */
extern "C" void __cxa_pure_virtual(void) { for (;;) { } }
void operator delete(void *) noexcept { }
void operator delete(void *, size_t) noexcept { }
#endif

extern "C" {

void bm_fm_init(float fs)
{
    if (listo) return;
    Exp2::init();
    Sin::init();
    Freqlut::init(fs);
    Lfo::init(fs);
    PitchEnv::init(fs);
    Env::init_sr(fs);
    Porta::init_sr(fs);
    FmCore *core = new (mem_core) EngineMsfa();
    for (int t = 0; t < BM_TRACKS; t++) {
        Controllers *c = new (mem_ctl[t]) Controllers();
        c->values_[kControllerPitch] = 0x2000;
        c->values_[kControllerPitchRange] = 0;
        c->values_[kControllerPitchStep] = 0;
        c->values_[kControllerPortamentoGlissando] = 0;
        c->masterTune = 0;
        c->opSwitch = 0x3f;
        c->modwheel_cc = c->foot_cc = c->breath_cc = c->aftertouch_cc = 0;
        c->portamento_enable_cc = false;
        c->portamento_cc = 0;
        c->portamento_gliss_cc = false;
        c->core = core;
        c->refresh();
    }
    for (int v = 0; v < BM_VOICES; v++) {
        Voz *z = new (mem_voz[v]) Voz();
        z->pos = _N_;
        z->track = -1;
    }
    listo = true;
}

void bm_fm_bench(const uint8_t *patch, uint32_t (*now)(void), uint32_t out[5], int reps)
{
    static int32_t a[_N_], b[_N_];
    static int32_t fb[2];
    alignas(8) static unsigned char mem[sizeof(Dx7Note)];
    Dx7Note *n = new (mem) Dx7Note();
    n->init(patch, 60, 100, 60, -1, &ctl(0));
    for (int i = 0; i < _N_; i++) b[i] = i << 18;
    uint32_t t;
    t = now(); for (int r = 0; r < reps; r++) FmOpKernel::compute_pure(a, r << 20, 1 << 16, 1 << 23, 1 << 23, false); out[0] = now() - t;
    t = now(); for (int r = 0; r < reps; r++) FmOpKernel::compute(a, b, r << 20, 1 << 16, 1 << 23, 1 << 23, false); out[1] = now() - t;
    t = now(); for (int r = 0; r < reps; r++) FmOpKernel::compute_fb(a, r << 20, 1 << 16, 1 << 23, 1 << 23, fb, 3, false); out[2] = now() - t;
    t = now(); for (int r = 0; r < reps; r++) { memset(a, 0, sizeof a); n->compute(a, 1 << 23, 1 << 24, &ctl(0)); } out[3] = now() - t;
    t = now(); for (int r = 0; r < reps; r++) { a[r & 63] = r; } out[4] = now() - t;
}

namespace { struct AlgPeek : FmCore { static const FmAlgorithm &get(int i) { return algorithms[i]; } }; }

void bm_fm_alg_graph(int alg, uint8_t mods[6], int8_t fb[2])
{
    const FmAlgorithm &A = AlgPeek::get(alg & 31);
    uint8_t writers[3] = {0, 0, 0};                 /* quién escribió en cada bus desde la última lectura */
    fb[0] = fb[1] = -1;
    for (int i = 0; i < 6; i++) {
        int f = A.ops[i], in = (f >> 4) & 3, out = f & 3;
        mods[i] = in ? writers[in] : 0;
        if (f & 0x80) fb[0] = (int8_t)i;
        if (f & 0x40) fb[1] = (int8_t)i;
        if (out) { if (f & 4) writers[out] |= (uint8_t)(1u << i); else writers[out] = (uint8_t)(1u << i); }
    }
}

uint8_t bm_fm_carriers(int alg)
{
    return FmCore::get_carrier_operators((uint8_t)(alg & 31));
}

void bm_fm_start(int v, int t, const uint8_t *patch, int note, int vel, int src, int porta)
{
    Voz &z = voz(v);
    z.track = (int8_t)t;
    z.alg = patch[BM_FM_ALG] & 31;
    z.gain = gain_for(z.alg);
    z.note.init(patch, note, vel, src, porta, &ctl(t));
    z.lfo.reset(patch + 137);
    z.lfo.keydown();
    z.pos = _N_;
    z.suelta = 0;
}

void bm_fm_update(int v, const uint8_t *patch, int note, int vel, int porta)
{
    Voz &z = voz(v);
    if (z.track < 0) return;
    z.alg = patch[BM_FM_ALG] & 31;
    z.gain = gain_for(z.alg);
    z.note.update(patch, note, vel, porta, &ctl(z.track));
}

void bm_fm_release(int v)
{
    Voz &z = voz(v);
    if (z.track < 0 || z.suelta) return;
    z.note.keyup();
    z.suelta = 1;
}

int bm_fm_render(int v, int32_t *dst, int n)
{
    Voz &z = voz(v);
    if (z.track < 0) return 0;
    while (n > 0) {
        if (z.pos >= _N_) {
            if (z.suelta) {                      /* ¿todas las portadoras terminaron? (como Synth_Dexed) */
                VoiceStatus st;
                memset(&st, 0, sizeof st);
                z.note.peekVoiceStatus(st);
                uint8_t car = FmCore::get_carrier_operators(z.alg);
                int vivas = 0;
                for (int op = 0; op < 6; op++)
                    if ((car & (1 << op)) && !(st.amp[op] <= VOICE_SILENCE_LEVEL && st.ampStep[op] == 4)) vivas++;
                if (!vivas) { z.track = -1; return 0; }
            }
            memset(z.buf, 0, sizeof z.buf);
            z.note.compute(z.buf, z.lfo.getsample(), z.lfo.getdelay(), &ctl(z.track));
            z.pos = 0;
        }
        int k = _N_ - z.pos;
        if (k > n) k = n;
        const int32_t *s = z.buf + z.pos;
        for (int i = 0; i < k; i++) dst[i] += ((s[i] >> 10) * z.gain) >> 8;   /* Q24 de msfa -> escala de los motores */
        dst += k; n -= k; z.pos = (uint8_t)(z.pos + k);
    }
    return 1;
}

}  // extern "C"
