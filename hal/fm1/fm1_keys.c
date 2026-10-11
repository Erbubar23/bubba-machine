/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Barrido de la matriz de teclas del FM-1 con la API gpio del SDK (ver fm1_hw.h).
 * Un barrido completo recorre las 11 columnas: selecciona la columna en los 74HC595, espera a
 * que se asiente, lee las 6 filas y deja prendidos un momento los LEDs de esa columna. */
#include "app_config.h"
#include "system/includes.h"
#include "device/gpio.h"
#include "asm/includes.h"
#include "fm1_hw.h"

static const uint16_t ROW_IO[FM1_NROW] = {IO_PORTA_00, IO_PORTA_05, IO_PORTA_06, IO_PORTA_07, IO_PORTA_08, IO_PORTB_07};
static const uint16_t LED_IO[4] = {IO_PORTA_09, IO_PORTA_10, IO_PORTH_06, IO_PORTH_09};   /* filas 1..4 */

static void esperar_us(uint32_t us)
{
    volatile uint32_t n = us * 40u;     /* aproximado; solo para que las líneas se asienten */
    while (n--)
        ;
}

static void columnas(uint16_t w)
{
    int i;
    for (i = 0; i < 16; i++) {
        gpio_direction_output(FM1_SR_DATA, (w & (0x8000u >> i)) ? 1 : 0);
        gpio_direction_output(FM1_SR_CLK, 1);
        gpio_direction_output(FM1_SR_CLK, 0);
    }
    gpio_direction_output(FM1_SR_LATCH, 1);
    gpio_direction_output(FM1_SR_LATCH, 0);
}

static void leds(uint8_t rows)
{
    int r;
    for (r = 0; r < 4; r++)
        gpio_direction_output(LED_IO[r], (rows >> (r + 1)) & 1);
}

void fm1_keys_init(void)
{
    int i;
    for (i = 0; i < FM1_NROW; i++) {
        gpio_set_die(ROW_IO[i], 1);
        gpio_set_pull_down(ROW_IO[i], 0);
        gpio_set_pull_up(ROW_IO[i], 1);
        gpio_direction_input(ROW_IO[i]);
    }
    for (i = 0; i < 4; i++)
        gpio_direction_output(LED_IO[i], 0);
    gpio_direction_output(FM1_SR_DATA, 0);
    gpio_direction_output(FM1_SR_CLK, 0);
    gpio_direction_output(FM1_SR_LATCH, 0);
    columnas(0xFFFFu);
}

void fm1_keys_frame(uint8_t raw[FM1_NCOL], const uint8_t led[FM1_NCOL])
{
    int p, r;
    for (p = 0; p < FM1_NCOL; p++) {
        uint8_t v = 0;
        leds(0);
        columnas((uint16_t)(0xFFFFu ^ (1u << p) ^ (p < 2 ? 1u << (11 + p) : 0u)));
        esperar_us(10);
        for (r = 0; r < FM1_NROW; r++)
            if (!gpio_read(ROW_IO[r]))
                v |= (uint8_t)(1u << r);
        raw[p] = v;
        if (led && led[p]) {
            leds(led[p]);
            esperar_us(40);
        }
    }
    leds(0);
    columnas(0xFFFFu);
}
