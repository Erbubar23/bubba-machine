/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — datos de fábrica: patches FM (tools/gen_fm.py). Los loops de fábrica del sampler se
 * fueron en 0.4 (bm_rom_nloops = 0): el sampler toca tomas.
 * 0.4: los kits grabados y las wavetables se fueron; la batería y los synths se calculan en vivo. */
#ifndef BM_ROM_H
#define BM_ROM_H

#include "bm_types.h"

/* Loops de fábrica del sampler (generados por tools/gen_loops.py): 8 bit a BM_SMP_RATE,
 * de solo lectura. T2 los ve como fuentes después de los slots de flash. */
typedef struct {
    const int8_t *data;
    uint32_t len;
    uint16_t bpm_x10;      /* tempo al que se grabó */
    uint8_t bars;
    char name[12];
} bm_rom_loop;

extern const bm_rom_loop bm_rom_loops[];

/* Patches de fábrica del motor FM (tools/gen_fm.py): voz de la DX7 sin empaquetar, 156 bytes. */
#define BM_FM_PATCHES   20
extern const uint8_t bm_fm_rom[BM_FM_PATCHES][156];
extern const char bm_fm_names[BM_FM_PATCHES][9];
extern const int bm_rom_nloops;

#endif
