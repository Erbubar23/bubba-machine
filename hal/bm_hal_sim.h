/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — HAL simulada compartida por el emulador web y las pruebas de PC.
 * Emula la flash NOR de 1 MiB (borrado a 0xFF, programación solo baja bits),
 * la pantalla (framebuffer 240x240) y los LEDs. */
#ifndef BM_HAL_SIM_H
#define BM_HAL_SIM_H

#include "../core/bm_app.h"

typedef struct {
    uint8_t flash[BM_FLASH_SIZE];
    uint16_t fb[BM_LCD_W * BM_LCD_H];
    uint8_t leds[BM_LEDS];
    uint32_t flash_writes;
    uint32_t spi_bytes;
    uint8_t midi_out[256];
    uint16_t midi_n;
} bm_sim;

void bm_sim_flash_erase_all(bm_sim *s);
void bm_sim_platform(bm_sim *s, bm_platform *pf);

#endif
