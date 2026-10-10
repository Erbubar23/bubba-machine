/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — punto de entrada en el FM-1 (ESQUELETO DE PORT).
 *
 * Estado: estructura completa del firmware en el equipo, con cada acceso a
 * registros marcado como TODO(HW-VERIFY). No se ha compilado con el toolchain
 * de JieLi ni probado en un FM-1. El orden de bring-up está en README_PORT.md.
 *
 * Diseño propio: aquí no hay código de otros firmwares. Los datos de
 * pines vienen de SPEC_V3 §1 (hechos de hardware). */
#include <string.h>
#include "../../core/bm_app.h"
#include "fm1_scan.h"

/* ------------------------------------------------------------ pines (SPEC_V3 §1) */
#define LCD_SPI_CLK   "PC9"     /* SPI1 */
#define LCD_SPI_MOSI  "PC10"
#define LCD_CS        "PC7"
#define LCD_DC        "PC8"
#define LCD_BL        "PA2"     /* activo en bajo; también habilita teclas y botones */
#define ADC_MASTER    4         /* PB6 */
#define ADC_BATT      3         /* PB1: solo entrada analógica (8 s en bajo resetea el chip) */
#define CODEC_CTRL    "PC0 PC1 PC2 PC6"
#define LED_LINES     "PH6 PH9 PA9 PA10"

/* ------------------------------------------------------------ memoria estática */
static bm_app app;                  /* ~81 KB: ver docs/MEMORIA.md */
static fm1_scan scan;
static fm1_scan_map scan_map;       /* TODO(HW-VERIFY): llenar con la calibración (README_PORT.md §3) */
#define HALF_FRAMES 128u            /* 2.9 ms a 44.1 kHz */
static int32_t dma_buf[2][HALF_FRAMES * 2];

/* ------------------------------------------------------------ drivers (TODO) */
static uint32_t millis(void) { return 0; /* TODO(HW-VERIFY): contador de TIMER4 (24 MHz) / 24000 */ }

static void lcd_blit(void *u, int x, int y, int w, int h, const uint16_t *px)
{
    (void)u; (void)x; (void)y; (void)w; (void)h; (void)px;
    /* TODO(HW-VERIFY): ST7789V por SPI1: CASET(x, x+w-1), RASET(y, y+h-1), RAMWR,
     * luego DMA de w*h*2 bytes desde px con D/C en alto. Los bytes van en big-endian
     * (RGB565 MSB primero): intercambiar bytes o configurar el SPI a 16 bits. */
}

static void led_set(void *u, int id, uint8_t level)
{
    (void)u; (void)id; (void)level;
    /* TODO(HW-VERIFY): guardar el nivel; la ISR de escaneo enciende el LED de cada
     * columna con un tiempo proporcional (apagado/tenue/encendido/parpadeo/respiración). */
}

static void midi_out(void *u, uint8_t b) { (void)u; (void)b; /* TODO(HW-VERIFY): cola hacia el endpoint USB-MIDI */ }

static int fl_read(uint32_t a, void *d, uint32_t n) { memcpy(d, (const void *)(uintptr_t)(0x02000000u + a), n); return 0; /* TODO(HW-VERIFY): base XIP */ }
static int fl_erase(uint32_t a) { (void)a; return -1; /* TODO(HW-VERIFY): rutina en .ram_text con IRQ apagadas; comando 0x20 */ }
static int fl_program(uint32_t a, const void *s, uint32_t n) { (void)a; (void)s; (void)n; return -1; /* TODO(HW-VERIFY): 0x02, página de 256 B */ }
static const bm_flash_ops FLASH = {fl_read, fl_erase, fl_program};
static const uint8_t *fl_map(void *u, uint32_t a) { (void)u; return (const uint8_t *)(uintptr_t)(0x02000000u + a); }

static uint16_t adc_read(int ch) { (void)ch; return 0; /* TODO(HW-VERIFY): conversión de 10 bits */ }

/* ------------------------------------------------------------ ISR */
void fm1_audio_isr(int half)                 /* TODO(HW-VERIFY): enlazar a la IRQ de ALNK0 */
{
    int32_t *b = dma_buf[half & 1];
    bm_app_audio(&app, b, HALF_FRAMES);
    for (unsigned i = 0; i < HALF_FRAMES * 2; i++) b[i] <<= 8;   /* 24 bit alineado a la izquierda */
}

void fm1_scan_isr(void)                      /* TODO(HW-VERIFY): TIMER5 a ~10 kHz */
{
    uint8_t rows = 0;                        /* TODO(HW-VERIFY): leer PA0, PA5..PA8, PB7 (activos en bajo) */
    uint8_t next = fm1_scan_column(&scan, rows);
    (void)next;                              /* TODO(HW-VERIFY): desplazar el 74HC595 a la columna next y prender su LED */
}

/* ------------------------------------------------------------ main */
int main(void)
{
    /* TODO(HW-VERIFY): relojes, caché XIP, ventana de datos sin cifrar, GPIO, SPI1, ALNK0, codec, USB. */
    fm1_scan_init(&scan, &scan_map);
    bm_platform pf = {lcd_blit, led_set, midi_out, fl_map, &FLASH, NULL};
    bm_app_init(&app, &pf, millis());
    /* TODO(HW-VERIFY): arrancar DMA de audio y TIMER5 recién ahora. */
    uint32_t next_ui = 0, next_adc = 0;
    for (;;) {
        uint32_t t = millis();
        int down, id;
        while ((id = fm1_scan_next_edge(&scan, &down)) >= 0) {
            if (id < BTN_COUNT) bm_app_button(&app, id, down, t);
            else bm_app_key(&app, id - BM_LED_KEY0, down, t);
        }
        if ((int32_t)(t - next_ui) >= 0) {
            next_ui = t + 1000u / UI_FPS;
            static const uint8_t ENC_ROLE[FM1_ENCS] = {ENC_SELECT, ENC_ALGO, ENC_K1, ENC_K2, ENC_K3, ENC_K4, ENC_PRESETS};
            for (int e = 0; e < FM1_ENCS; e++) {
                int16_t d = fm1_scan_take_encoder(&scan, e);
                if (d) bm_app_encoder(&app, ENC_ROLE[e], d, t);
            }
            bm_app_tick(&app, t);
        }
        if ((int32_t)(t - next_adc) >= 0) {
            next_adc = t + 10;
            bm_app_master(&app, adc_read(ADC_MASTER));
            static uint32_t nb;
            if (++nb % 500 == 0) bm_app_battery(&app, (uint8_t)(adc_read(ADC_BATT) / 10));  /* TODO(HW-VERIFY): curva real */
        }
    }
}
