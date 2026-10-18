/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — núcleo WAVE (docs/DISENO_0.6.md §3.2): las máquinas sustractivas y de tablas.
 *   B-MINI   tres osciladores clásicos, filtro escalera de 4 polos, dos envolventes (T3)
 *   BITTAL   dos wavetables de un snapshot con unísono, filtro, dos envolventes, LFO y 4 macros (T5)
 *   BUNO-6   un DCO por voz (sierra, pulso, sub, ruido), pasa-altos, escalera, una envolvente (T6);
 *            su chorus está en la mezcla (core/bm_dsp.c)
 * Comparten los osciladores polyBLEP, el filtro, las envolventes y el LFO; cada máquina tiene su
 * camino de cálculo, así B-MINI no paga el unísono de BITTAL. Sin memoria dinámica. */
#ifndef BM_WAVE_H
#define BM_WAVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bm_wv_init(float fs);
/* eng = ENG_MINI, ENG_BITTAL o ENG_BUNO. legato 0: voz nueva · 1: sigue y solo cambia de nota (glide)
 * · 2: mono, redispara las envolventes desde la nota anterior. */
void  bm_wv_start(int v, int eng, const uint8_t *prm, const uint8_t mac[4], float note, float vel, int legato);
void  bm_wv_release(int v);
/* lane: 0 o 1, el búfer de trabajo del núcleo que calcula (con el audio en dos núcleos).
 * flags: WV_PARA (BUNO-6: the voice skips its VCF, bm_wv_bus filters the sum) and, for BITTAL, the
 * table readers (unison voices of both oscillators) this voice may use, 0 = all.
 * Devuelve 1 si sigue sonando, 0 si terminó, -1 si tenía NaN/inf: la voz volvió a cero y out es silencio. */
#define WV_PARA          0x100
#define WV_READERS_MASK  0x0FF
#define WV_READERS_ALL   8        /* BITTAL: readers shared by all the voices of the channel */
int   bm_wv_render(int v, int eng, const uint8_t *prm, const uint8_t mac[4], float *out, int n, int lane, int flags);
float bm_wv_level(int v);
/* BUNO-6, per channel t, on the sum of its voices (buf, engine scale): the HPF (Juno-60: one for the
 * whole instrument) and, with para, the VCF of the paraphonic fast path. any = 0: the channel is
 * silent (its state starts clean). Returns -1 if a state had NaN/inf (reset, buf silenced). */
int   bm_wv_bus(int t, const uint8_t *prm, const uint8_t mac[4], int32_t *buf, int n, int any, int para);
/* 1 if every BUNO-6 voice would get the same cutoff (ENV, LFO, KYBD and VELO to the VCF at 0, RES
 * low): then one VCF on the sum sounds the same (paraphonic fast path, to try by ear) */
int   bm_wv_para(const uint8_t *prm, const uint8_t mac[4]);
/* medición en el aparato: ticks de now() para reps bloques de 64 de una voz de cada máquina */
void  bm_wv_bench(const uint8_t *prm_mini, const uint8_t *prm_bt, const uint8_t *prm_bn, const uint8_t mac[4],
                  uint32_t (*now)(void), uint32_t out[3], int reps);

#ifdef __cplusplus
}
#endif

#endif
