/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Pantalla ST7789V de 240x240 por el SPI1 del SDK (dispositivo "spi1", grupo B).
 * Secuencia de arranque según la hoja de datos del ST7789V: reset por software, salir del
 * sueño, 16 bits por píxel, orientación por defecto, inversión (panel IPS), modo normal,
 * pantalla encendida. Sin desplazamiento de ventana: el panel empieza en (0, 0). */
#include "app_config.h"
#include "system/includes.h"
#include "device/device.h"
#include "device/gpio.h"
#include "asm/spi.h"
#include "asm/includes.h"
#include "fm1_hw.h"

static void *spi;
static uint8_t tx[240 * 2 * 8];          /* 8 filas por envío, ya en orden de bytes del panel */

static void cmd(uint8_t c)
{
    gpio_direction_output(FM1_LCD_DC, 0);
    gpio_direction_output(FM1_LCD_CS, 0);
    dev_ioctl(spi, IOCTL_SPI_SEND_BYTE, c);
    gpio_direction_output(FM1_LCD_CS, 1);
}

static void data(const uint8_t *p, uint32_t n)
{
    gpio_direction_output(FM1_LCD_DC, 1);
    gpio_direction_output(FM1_LCD_CS, 0);
    if (n <= 4) {
        while (n--)
            dev_ioctl(spi, IOCTL_SPI_SEND_BYTE, *p++);
    } else {
        dev_write(spi, (void *)p, n);
    }
    gpio_direction_output(FM1_LCD_CS, 1);
}

static void ventana(int x0, int y0, int x1, int y1)
{
    uint8_t b[4];
    b[0] = (uint8_t)(x0 >> 8); b[1] = (uint8_t)x0; b[2] = (uint8_t)(x1 >> 8); b[3] = (uint8_t)x1;
    cmd(0x2A); data(b, 4);
    b[0] = (uint8_t)(y0 >> 8); b[1] = (uint8_t)y0; b[2] = (uint8_t)(y1 >> 8); b[3] = (uint8_t)y1;
    cmd(0x2B); data(b, 4);
    cmd(0x2C);
}

void fm1_lcd_blit(void *u, int x, int y, int w, int h, const uint16_t *px)
{
    int r, i, filas = (int)(sizeof tx / (uint32_t)(w * 2));
    (void)u;
    if (!spi || w <= 0 || h <= 0)
        return;
    if (filas < 1)
        filas = 1;
    fm1_paso("S1");
    ventana(x, y, x + w - 1, y + h - 1);
    fm1_paso("S2");
    for (r = 0; r < h; r += filas) {
        int n = (h - r < filas ? h - r : filas) * w;
        const uint16_t *s = px + r * w;
        for (i = 0; i < n; i++) {
            tx[2 * i] = (uint8_t)(s[i] >> 8);
            tx[2 * i + 1] = (uint8_t)s[i];
        }
        data(tx, (uint32_t)n * 2u);
    }
    fm1_paso("S0");
}

/* SISTEMA > FAST SCREEN (a prueba): SPI a 30 MHz (divisor 1) en vez de 12 MHz (4). Medido: el
 * cuadro entero pasa de 85 a 39 ms (docs/TRAZA_FM1.md); falta confirmar a la vista que se ve bien. */
void fm1_lcd_speed(int fast)
{
    JL_SPI1->BAUD = fast ? 1 : 4;
}

void fm1_lcd_init(void)
{
    static const uint8_t madctl = 0x00, colmod = 0x55;
    int i;
    gpio_direction_output(FM1_LCD_BL, 0);          /* luz de fondo (y teclado) encendidos */
    gpio_direction_output(FM1_LCD_CS, 1);
    gpio_direction_output(FM1_LCD_DC, 1);
    spi = dev_open("spi1", NULL);
    if (!spi)
        return;
    cmd(0x01);                                     /* SWRESET */
    os_time_dly(15);
    cmd(0x11);                                     /* SLPOUT */
    os_time_dly(12);
    cmd(0x3A); data(&colmod, 1);                   /* 16 bits */
    cmd(0x36); data(&madctl, 1);
    cmd(0x21);                                     /* INVON */
    cmd(0x13);                                     /* NORON */
    memset(tx, 0, sizeof tx);
    ventana(0, 0, 239, 239);
    for (i = 0; i < 240; i += 8)
        data(tx, sizeof tx);
    cmd(0x29);                                     /* DISPON */
}
