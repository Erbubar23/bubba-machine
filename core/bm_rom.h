/* Bubba Machine — ROM de sonidos (generada por tools/gen_assets.py). */
#ifndef BM_ROM_H
#define BM_ROM_H

#include "bm_types.h"

#define BM_KITS         2
#define BM_KIT_SOUNDS   16
#define BM_WAVES        16
#define BM_WAVE_LEVELS  8
#define BM_WAVE_LEN     256

typedef struct {
    const uint8_t *data;   /* mu-law 8 bit a BM_FS */
    uint32_t len;
    uint8_t choke;         /* grupo de choke (0 = ninguno) */
    uint8_t ducks;         /* dispara el sidechain */
    char name[7];
} bm_drum_sound;

extern const bm_drum_sound bm_rom_kits[BM_KITS][BM_KIT_SOUNDS];
extern const char bm_rom_kit_names[BM_KITS][8];
extern const int16_t bm_ulaw_dec[256];
extern const int16_t bm_rom_waves[BM_WAVES][BM_WAVE_LEVELS][BM_WAVE_LEN];
extern const char bm_rom_wave_names[BM_WAVES][8];

#endif
