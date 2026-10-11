/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — ROM de sonidos (generada por tools/gen_assets.py). */
#ifndef BM_ROM_H
#define BM_ROM_H

#include "bm_types.h"

#define BM_KITS         2        /* kits de la ROM ("acústicos": sonidos grabados) */
#define BM_KIT_SYN      BM_KITS  /* el kit siguiente es SINTE: batería calculada en vivo */
#define BM_KITS_ALL     (BM_KITS + 1)
#define BM_KIT_SOUNDS   16
#define BM_WAVES        16
#define BM_WAVE_LEVELS  8
#define BM_WAVE_LEN     256
#define BM_WAVE_HARM    127      /* armónicos guardados por onda */

typedef struct {
    const uint8_t *data;   /* mu-law 8 bit a BM_ROM_RATE */
    uint32_t len;
    uint8_t choke;         /* grupo de choke (0 = ninguno) */
    uint8_t ducks;         /* dispara el sidechain */
    char name[7];
} bm_drum_sound;

extern const bm_drum_sound bm_rom_kits[BM_KITS][BM_KIT_SOUNDS];
extern const char bm_rom_kit_names[BM_KITS][8];
extern const int16_t bm_ulaw_dec[256];
extern const int16_t bm_wave_spec[BM_WAVES][BM_WAVE_HARM][2];                /* [h][0] coseno, [1] seno (Q15) */
extern int16_t bm_rom_waves[BM_WAVES][BM_WAVE_LEVELS][BM_WAVE_LEN];          /* en RAM: bm_waves_init() */
void bm_waves_init(void);
extern const char bm_rom_wave_names[BM_WAVES][8];

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
#define BM_FM_PATCHES   16
extern const uint8_t bm_fm_rom[BM_FM_PATCHES][156];
extern const char bm_fm_names[BM_FM_PATCHES][9];
extern const int bm_rom_nloops;

#endif
