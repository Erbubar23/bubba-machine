/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — wavetables de BITTAL (docs/DISENO_0.6.md §4.2).
 *
 * Un snapshot guarda cada tabla como espectro: hasta 8 frames, cada uno con hasta 256 armónicos
 * (magnitud en dB y fase, un byte cada una). Al cargar el sonido, el FM-1 arma en RAM cada frame en
 * 7 niveles, uno por octava, con una IFFT: el nivel 0 tiene 1024 muestras y 256 armónicos, y cada
 * nivel siguiente la mitad de muestras y de armónicos. Al tocar, se lee el nivel cuyo armónico más
 * alto queda por debajo de 19 kHz: la tabla no tiene aliasing en ninguna nota.
 *
 * Hay dos ranuras (los dos osciladores de T5). La UI las arma fuera del audio; mientras una se arma,
 * el audio la ve como no lista y ese oscilador calla. */
#ifndef BM_WT_H
#define BM_WT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BM_WT_FRAMES  8
#define BM_WT_N       1024        /* muestras del nivel 0 */
#define BM_WT_HARM    256         /* armónicos del nivel 0 */
#define BM_WT_LEVELS  7
#define BM_WT_TOT     2032        /* 1024 + 512 + ... + 16 */
#define BM_WT_SLOTS   2

/* Tabla en espectro (dentro de un snapshot): nf (1..8), nh (1..255, 0 = 256), luego nf * nh pares
 * (magnitud, fase). Magnitud 0 = nada; v > 0 = (v - 255) * 0.375 dB. Fase = v * 2 pi / 256. */
static inline int bm_wt_spec_nh(const uint8_t *t) { return t[1] ? t[1] : 256; }
static inline int bm_wt_spec_size(const uint8_t *t) { return 2 + (int)t[0] * bm_wt_spec_nh(t) * 2; }

extern const uint16_t bm_wt_off[BM_WT_LEVELS];

/* Arma la ranura desde un espectro (o la vacía si spec es NULL). Corre en la UI: ~10 ms. */
void bm_wt_build(int slot, const uint8_t *spec);
int  bm_wt_ready(int slot);
int  bm_wt_frames(int slot);
/* frame f (0..7, se recorta a los que hay) del nivel l: n muestras (potencia de 2) */
const int16_t *bm_wt_data(int slot, int f, int l, int *n);
/* nivel para una fundamental en Hz */
int  bm_wt_level_for(float hz);

#ifdef __cplusplus
}
#endif

#endif
