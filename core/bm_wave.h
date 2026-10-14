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
/* lane: 0 o 1, el búfer de trabajo del núcleo que calcula (con el audio en dos núcleos) */
int   bm_wv_render(int v, int eng, const uint8_t *prm, const uint8_t mac[4], float *out, int n, int lane);
float bm_wv_level(int v);
/* medición en el aparato: ticks de now() para reps bloques de 64 de una voz de cada máquina */
void  bm_wv_bench(const uint8_t *prm_mini, const uint8_t *prm_bt, const uint8_t *prm_bn, const uint8_t mac[4],
                  uint32_t (*now)(void), uint32_t out[3], int reps);

#ifdef __cplusplus
}
#endif

#endif
