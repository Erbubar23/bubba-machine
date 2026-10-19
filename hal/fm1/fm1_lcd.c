/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Pantalla ST7789V de 240x240 por el SPI1 del SDK (dispositivo "spi1", grupo B).
 * Secuencia de arranque según la hoja de datos del ST7789V: reset por software, salir del
 * sueño, 16 bits por píxel, orientación por defecto, inversión (panel IPS), modo normal,
 * pantalla encendida. Sin desplazamiento de ventana: el panel empieza en (0, 0).
 *
 * Los envíos: con semáforo y sin bloqueo, como el driver de LCD del SDK
 * (apps/common/ui/lcd_driver/lcd_drive.c) y su ejemplo de SPI. Leído del driver (cpu.a, spi0.c):
 *  - IOCTL_SPI_SET_USE_SEM crea el semáforo y registra la interrupción del SPI (núcleo 1,
 *    prioridad 1). Un envío de >= 8 bytes (el umbral por defecto) espera dormido en el semáforo;
 *    los de un byte (comandos, ventana) siguen girando, que es más corto que un cambio de tarea.
 *  - IOCTL_SPI_NON_BLOCK: dev_write arranca el DMA y vuelve. El próximo dev_write espera solo al
 *    anterior; IOCTL_SPI_WRITE_NON_BLOCK_FLUSH espera sin mandar nada.
 *  - IOCTL_SPI_SEND_BYTE no espera a un envío sin bloqueo en curso y usa el modo de espera del
 *    último dev_write: por eso aquí todo va por dev_write (un byte también).
 * El CS y el DC son pines comunes: antes de tocarlos se espera el envío en curso (flush). Mientras
 * el DMA manda un tile desde un búfer, el núcleo ya prepara el siguiente en el otro (y dibuja la
 * franja que sigue). */
#include "app_config.h"
#include "system/includes.h"
#include "device/device.h"
#include "device/gpio.h"
#include "asm/spi.h"
#include "asm/includes.h"
#include "fm1_hw.h"

#define TXN (240 * 2 * 4)                /* bytes por envío: un tile de 60x8 (960 B) entra dos veces */

static void *spi;
static uint8_t tx[2][TXN] __attribute__((aligned(4)));   /* dos búferes, ya en orden de bytes del panel */
static int cur;                          /* el que se llena ahora; el otro puede estar saliendo */
static uint8_t enviando;                 /* hay un envío sin bloqueo en curso (con el CS abajo) */

/* espera el envío en curso y suelta el CS */
static void flush(void)
{
    if (!enviando)
        return;
    dev_ioctl(spi, IOCTL_SPI_WRITE_NON_BLOCK_FLUSH, 0);
    gpio_direction_output(FM1_LCD_CS, 1);
    enviando = 0;
}

static void cmd(uint8_t c)
{
    flush();
    gpio_direction_output(FM1_LCD_DC, 0);
    gpio_direction_output(FM1_LCD_CS, 0);
    dev_write(spi, &c, 1);               /* un byte: el driver lo manda y espera ahí mismo */
    gpio_direction_output(FM1_LCD_CS, 1);
}

/* datos cortos (los parámetros de un comando, hasta 4 bytes): de a uno, sin DMA, como antes */
static void data(const uint8_t *p, uint32_t n)
{
    flush();
    gpio_direction_output(FM1_LCD_DC, 1);
    gpio_direction_output(FM1_LCD_CS, 0);
    while (n--)
        dev_write(spi, (void *)p++, 1);
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
    int r, i, filas = (int)(TXN / (uint32_t)(w * 2));
    (void)u;
    if (!spi || w <= 0 || h <= 0)
        return;
    if (filas < 1)
        filas = 1;
    fm1_paso("S1");
    for (r = 0; r < h; r += filas) {
        int n = (h - r < filas ? h - r : filas) * w;
        const uint16_t *s = px + r * w;
        uint8_t *b = tx[cur];
        for (i = 0; i < n; i++) {        /* mientras tanto el DMA sigue con el envío anterior */
            b[2 * i] = (uint8_t)(s[i] >> 8);
            b[2 * i + 1] = (uint8_t)s[i];
        }
        if (r == 0) {                    /* la ventana: espera lo anterior y suelta el CS */
            ventana(x, y, x + w - 1, y + h - 1);
            fm1_paso("S2");
            gpio_direction_output(FM1_LCD_DC, 1);
            gpio_direction_output(FM1_LCD_CS, 0);
        }
        dev_write(spi, b, (uint32_t)n * 2u);   /* espera el trozo anterior, arranca este y vuelve */
        enviando = 1;
        cur ^= 1;
    }
    fm1_paso("S0");
}

/* SISTEMA > FAST SCREEN (a prueba): SPI a 30 MHz (divisor 1) en vez de 12 MHz (4). Medido: el
 * cuadro entero pasa de 85 a 39 ms (docs/TRAZA_FM1.md); falta confirmar a la vista que se ve bien. */
void fm1_lcd_speed(int fast)
{
    flush();                             /* no cambiar el reloj en medio de un envío */
    JL_SPI1->BAUD = fast ? 1 : 4;
}

void fm1_lcd_init(void)
{
    static const uint8_t madctl = 0x00, colmod = 0x55;
    int i;
    gpio_direction_output(FM1_LCD_BL, 0);          /* luz de fondo (y teclado) encendidos */
    gpio_direction_output(FM1_LCD_CS, 1);
    gpio_direction_output(FM1_LCD_DC, 1);
    if (!spi)                                      /* el modo seguro puede volver a llamar */
        spi = dev_open("spi1", NULL);
    if (!spi)
        return;
    flush();
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
    gpio_direction_output(FM1_LCD_DC, 1);
    gpio_direction_output(FM1_LCD_CS, 0);
    for (i = 0; i < 240 * 240 * 2; i += TXN)
        dev_write(spi, tx[0], TXN);
    dev_ioctl(spi, IOCTL_SPI_WRITE_NON_BLOCK_FLUSH, 0);
    gpio_direction_output(FM1_LCD_CS, 1);
    cmd(0x29);                                     /* DISPON */
    dev_ioctl(spi, IOCTL_SPI_SET_USE_SEM, 0);      /* esperar dormido (solo la primera vez crea el semáforo) */
    dev_ioctl(spi, IOCTL_SPI_NON_BLOCK, 1);
}
