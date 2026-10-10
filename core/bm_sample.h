/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — slots de sample en flash (DISENO_0.2 §5).
 * 2 slots de 64 KiB: cabecera de 128 B (largo, chops, CRC) + hasta 65408 muestras
 * de 8 bit a 16 kHz (≈ 4 s). Se reproducen directo desde la flash (XIP), sin RAM.
 * Igual que los patrones: la cabecera se escribe al final (commit). */
#ifndef BM_SAMPLE_H
#define BM_SAMPLE_H

#include "bm_store.h"
#include "bm_audio.h"

uint32_t bm_sample_addr(int slot);
/* Lee la cabecera y valida el CRC. map = lectura XIP. 0 si hay sample válido. */
int bm_sample_info(const bm_flash_ops *f, const uint8_t *(*map)(void *, uint32_t), void *mu, int slot, bm_smp_src *out);
int bm_sample_write(const bm_flash_ops *f, int slot, const int8_t *data, uint32_t len, const uint32_t *marks, int nmarks);
int bm_sample_erase(const bm_flash_ops *f, int slot);
/* Reescribe solo los chops (sector 0). scratch: 4 KiB de RAM prestada. */
int bm_sample_set_marks(const bm_flash_ops *f, int slot, const uint32_t *marks, int nmarks, uint8_t *scratch);

#endif
