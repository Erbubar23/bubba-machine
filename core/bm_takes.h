/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — biblioteca de tomas del sampler (DISENO_3.0, Grabadora).
 *
 * 128 KiB de flash (32 sectores) para tomas de largo libre: hasta BM_TAKES tomas de
 * hasta 3 s (la RAM de grabación), 8.1 s en total. Cada toma ocupa sectores seguidos:
 * cabecera de 128 B al inicio del primero y el audio detrás (8 bit a 16 kHz). Se
 * reproducen directo desde la flash (XIP). La cabecera se escribe al final (commit). */
#ifndef BM_TAKES_H
#define BM_TAKES_H

#include "bm_store.h"

#define BM_TAKE_SECTORS  ((BM_TAKE_HI - BM_TAKE_LO) / BM_SECTOR)

typedef struct {
    uint8_t valid, id;          /* id 0..BM_TAKES-1: TOMA 01..08 */
    uint8_t nmarks, gain;       /* cortes (0 = 8 iguales) · ganancia, 64 = 0 dB */
    uint16_t bpm_x10;           /* tempo al grabar */
    uint8_t bars;
    uint8_t sector, nsect;      /* dónde vive */
    uint32_t len;               /* muestras */
    uint32_t marks[BM_SLICES];
    const int8_t *data;         /* XIP */
} bm_take;

typedef const uint8_t *(*bm_map_fn)(void *user, uint32_t addr);

/* Lee la biblioteca. Devuelve cuántas tomas válidas hay (tk[id].valid). */
int  bm_takes_scan(const bm_flash_ops *f, bm_map_fn map, void *mu, bm_take tk[BM_TAKES]);
/* Muestras que caben en la toma más grande que se puede grabar ahora (hueco contiguo). */
uint32_t bm_takes_room(const bm_take tk[BM_TAKES]);
/* Muestras libres en total (para la barra de memoria). */
uint32_t bm_takes_free(const bm_take tk[BM_TAKES]);
/* Guarda una toma nueva. Devuelve su id, o -1 si no cabe o falla la flash. */
int  bm_takes_write(const bm_flash_ops *f, bm_map_fn map, void *mu, bm_take tk[BM_TAKES],
                    const int8_t *data, uint32_t len, uint16_t bpm_x10, uint8_t bars);
/* Reescribe la cabecera (cortes, BPM, compases, ganancia). scratch: 4 KiB prestados. */
int  bm_takes_update(const bm_flash_ops *f, bm_take *t, uint8_t *scratch);
int  bm_takes_erase(const bm_flash_ops *f, bm_take *t);
/* Borra toda la zona (al pasar de 0.2 a 0.3). */
int  bm_takes_format(const bm_flash_ops *f);

#endif
