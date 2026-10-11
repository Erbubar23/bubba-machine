/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — puntos de medición del cálculo de audio.
 *
 * Sin BM_PROF no hacen nada (PC, emulador). En el FM-1 (hal/fm1/fm1_bench.c) acumulan el tiempo
 * de cada sección en ticks del timer, y la consola los lee (tools/fm1/traza.py). */
#ifndef BM_PROF_H
#define BM_PROF_H

#include <stdint.h>

enum {
    BM_P_CMD,             /* comandos de la UI */
    BM_P_SEQ,             /* pasos del secuenciador y rasgueos */
    BM_P_T0,              /* canales 1..6 (BM_P_T0 + t) */
    BM_P_MIX = BM_P_T0 + 6,   /* mezcla, efectos y salida */
    BM_P_N
};

#ifdef BM_PROF
uint32_t bm_prof_now(void);
void bm_prof_add(int id, uint32_t t0);
#define BM_PROF_T0()        uint32_t bm_prof_t0_ = bm_prof_now()
#define BM_PROF_LAP(id)     do { bm_prof_add((id), bm_prof_t0_); bm_prof_t0_ = bm_prof_now(); } while (0)
#define BM_PROF_RESTART()   (bm_prof_t0_ = bm_prof_now())
#else
#define BM_PROF_T0()        do { } while (0)
#define BM_PROF_LAP(id)     do { } while (0)
#define BM_PROF_RESTART()   do { } while (0)
#endif

#endif
