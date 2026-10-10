/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — utilidades: PRNG, CRC32 y tablas de DSP calculadas al arrancar. */
#ifndef BM_UTIL_H
#define BM_UTIL_H

#include "bm_types.h"

/* PRNG determinista (SPEC_V3 §4.1): nunca rand(). */
static inline uint32_t bm_xorshift32(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

uint32_t bm_crc32(uint32_t crc, const void *data, size_t n);

/* Tablas (Q indicado). Se llenan una vez en bm_tables_init(). */
extern uint32_t bm_note_inc[128];     /* incremento de fase 2^32/ciclo a BM_FS */
extern uint32_t bm_decay_coef[128];   /* multiplicador por muestra, Q30 (5 ms .. 4 s) */
extern uint32_t bm_attack_inc[128];   /* incremento lineal por muestra, Q30 (1 ms .. 1 s) */
extern int32_t  bm_svf_g[128];        /* tan(pi fc / fs), Q16 */
extern uint16_t bm_onepole_a[128];    /* 1 - e^(-2 pi fc / fs), Q15 */
extern uint16_t bm_pan_l[128], bm_pan_r[128]; /* Q15, potencia constante */
extern uint32_t bm_semi_ratio[49];    /* 2^((i-24)/12), Q16 */

void bm_tables_init(void);

/* Frecuencia del corte para un valor de macro 0..127 (Hz, para la UI). */
uint32_t bm_cutoff_hz(int v);
/* Tiempo de decay/attack en ms para la UI. */
uint32_t bm_decay_ms(int v);
uint32_t bm_attack_ms(int v);

#endif
