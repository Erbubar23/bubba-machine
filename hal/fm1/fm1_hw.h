/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine en el FM-1: hechos de hardware y piezas del port sobre el SDK de JieLi.
 *
 * Hechos de la placa (pines, matriz, pantalla), medidos y publicados por Felucca y X0X
 * (hugelton/Felucca, charlesvestal/fm1-x0x). Aquí solo están los datos; el código es propio
 * y usa los drivers del SDK AC79 (Apache-2.0). Ver hal/fm1/README_PORT.md. */
#ifndef FM1_HW_H
#define FM1_HW_H

#include <stdint.h>

/* ---------------------------------------------------------------- versión */
#define FM1_IDENT        "FM-1_003"   /* lo que contesta al saludo del actualizador (modelo_versión) */
#define FM1_USB_PRODUCT  "FM-1 Bubba Machine"

/* ---------------------------------------------------------------- pantalla: ST7789V 240x240 */
/* SPI1 grupo B del SDK: PC9 CLK, PC10 DO. CS PC7, D/C PC8, backlight PA2 (activo en bajo;
 * esa línea también alimenta botones y teclas: queda siempre encendida). */
#define FM1_LCD_CS   IO_PORTC_07
#define FM1_LCD_DC   IO_PORTC_08
#define FM1_LCD_BL   IO_PORTA_02

/* ---------------------------------------------------------------- matriz de teclas */
/* 11 columnas x 6 filas con diodos. Columnas por 2x 74HC595 en cadena (16 bits, el primero
 * que entra es el bit 15): PA4 dato, PA3 reloj, PA1 latch. Columna p activa = bit p en 0
 * (las columnas 0 y 1 también ponen en 0 el bit 11 + p). Filas con pull-up, 0 = cerrada:
 * bit 0 PA0, bits 1..4 PA5..PA8, bit 5 PB7. */
#define FM1_SR_DATA  IO_PORTA_04
#define FM1_SR_CLK   IO_PORTA_03
#define FM1_SR_LATCH IO_PORTA_01
#define FM1_NCOL 11
#define FM1_NROW 6
/* LED de la tecla de la columna seleccionada en las filas 1..4: PA9, PA10, PH6, PH9 (alto = encendido) */

/* IDs: 0..13 botones, 14..40 teclas F3..G5. Botones medidos: */
enum {
    FM1_B_OCTDN = 0, FM1_B_OCTUP = 1,
};

/* ---------------------------------------------------------------- ADC (10 bits) */
#define FM1_ADC_MASTER AD_CH_PB06
#define FM1_ADC_BATT   AD_CH_PB01       /* PB1: solo analógico (8 s en bajo reinicia el chip) */

/* ---------------------------------------------------------------- flash (1 MiB) */
/* Lo que escribe el instalador es [0, 0x93000) y lo escribe el cargador oficial; el firmware
 * nunca toca eso. 0x97000-0xECFFF es la zona de datos (en el firmware de fábrica, toda era
 * la "VM" del SDK). Mapa de Bubba Machine:
 *   0x97000 patrones · 0xBF000 arranques · 0xC0000 tomas · 0xD9000 idioma · 0xDD000 ajustes globales
 *   0xE0000 cargador en espera · 0xE5000 VM del SDK (32 KiB, su tamaño por defecto)
 * Por encima de 0xED000 quedan BTIF, USR y la MAC del aparato: no se tocan nunca. */
#define FM1_W_LO        0x097000u      /* primer byte que el firmware puede escribir */
#define FM1_W_HI        0x0E5000u      /* primer byte que ya no (de ahí sigue la VM del SDK) */
#define FM1_OTA_AREA    0x0E0000u      /* cargador de actualización en espera (5 sectores) */
#define FM1_OTA_LEN     0x005000u
#define FM1_OTA_RES     (FM1_OTA_AREA + FM1_OTA_LEN - 0x100u)   /* copia del registro de actualización */
#define FM1_BOOT_SEC    0x0BF000u      /* registro de arranques (modo seguro) */
#define FM1_VM_LO       0x0E5000u      /* almacén de ajustes del SDK ("VM"), movido aquí por el */
#define FM1_VM_LEN      0x008000u      /* empaquetador para que no se mezcle con los datos */

int  fm1_flash_init(void);
void fm1_flash_plain_window(void);
void fm1_quiet(int on);
void fm1_miga(const char *c);                             /* paso actual, en una esquina de la pantalla */
void fm1_paso(const char *c);                             /* sub-paso (sin dibujar): dentro de la pantalla, la flash… */
void fm1_latido(void);                                    /* el bucle principal sigue vivo */
void fm1_perro(int on);                                   /* perro guardián del bucle (apagado en actualizaciones) */
int  fm1_cuelgue_previo(char out[5]);                     /* lo que anotó el perro antes de reiniciar, 0 si nada */
void fm1_boot_note(const char c[4]);                      /* anota 4 caracteres en el registro de este arranque */                                   /* pausa barrido y audio mientras se escribe la flash */                        /* la zona de datos se lee sin descifrar por XIP */
int  fm1_flash_read(uint32_t a, void *d, uint32_t n);
int  fm1_flash_erase(uint32_t a);                         /* sector de 4 KiB */
int  fm1_flash_prog(uint32_t a, const void *s, uint32_t n);
const uint8_t *fm1_flash_map(uint32_t a);                 /* lectura directa (XIP) */

/* ---------------------------------------------------------------- arranque */
enum { FM1_BOOT_NORMAL, FM1_BOOT_SAFE };
int  fm1_boot_begin(int keys_safe);       /* devuelve FM1_BOOT_*; puede no volver (modo del chip) */
void fm1_boot_ok(void);                   /* llamar cuando el firmware lleva un rato bien */
uint32_t fm1_boot_fails(void);
void fm1_boot_stage(int n);               /* etapa n (1..8) alcanzada en este arranque */
enum { FM1_RST_NONE = 0xFF, FM1_RST_POWER = 1, FM1_RST_WDT = 2, FM1_RST_SOFT = 3, FM1_RST_OTHER = 4 };
int  fm1_boot_past(int k, uint8_t *etapa, uint8_t *fin, uint8_t *ok);   /* k = 1: el arranque anterior */
void fm1_enter_uboot(void);               /* modo de actualización del chip (USB 4C4A:8057); no vuelve */

/* ---------------------------------------------------------------- pantalla */
void fm1_lcd_init(void);
void fm1_lcd_blit(void *u, int x, int y, int w, int h, const uint16_t *px);

/* ---------------------------------------------------------------- matriz */
void     fm1_keys_init(void);
void     fm1_keys_frame(uint8_t raw[FM1_NCOL], const uint8_t led[FM1_NCOL]);   /* un barrido completo */

/* barrido por interrupción (TIMER5) para el firmware completo; fm1_keys_frame es el del diagnóstico */
void fm1_scan_start(void);
void fm1_scan_stop(void);
int  fm1_scan_edge(int *down);                            /* un flanco (id 0..40) o -1 */
int  fm1_scan_encoder(int e);                             /* pasos de la perilla e desde la última vez */
int  fm1_scan_is_down(int id);
void fm1_scan_led(int id, uint8_t level);                 /* LED_OFF..LED_BREATH (core/bm_types.h) */

/* ---------------------------------------------------------------- audio: ALNK0 -> codec */
typedef void (*fm1_audio_render)(int32_t *lr, uint32_t frames);   /* estéreo, 24 bits abajo, con signo */
void fm1_audio_start(int32_t *dma, uint32_t frames_per_half, fm1_audio_render cb);   /* dma: 2 mitades */
void fm1_audio_start_mute(int32_t *dma, uint32_t frames_per_half, fm1_audio_render cb);   /* sin ALNK0 */
void fm1_audio_stop(void);
void fm1_audio_pause(int on);                             /* DMA e interrupción en pausa, sin rearmar el codec */
void fm1_scan_pause(int on);
uint32_t fm1_audio_halves(void);                          /* medios buffers calculados */
uint32_t fm1_audio_late(void);                            /* medios buffers que llegaron tarde (silencio) */
uint32_t fm1_scan_ticks(void);                            /* interrupciones del barrido (250 us cada una) */

/* ---------------------------------------------------------------- USB-MIDI */
void fm1_midi_init(void);
int  fm1_midi_ready(void);                                /* configurado por el host */
int  fm1_midi_on(void);                                   /* controlador encendido */
int  fm1_midi_send(const uint8_t *msg, uint32_t n);       /* mensajes MIDI completos (SysEx incluido); no espera */
uint32_t fm1_midi_lost(void);                             /* eventos descartados porque el computador no leía */
int  fm1_midi_get_sysex(uint8_t *buf, uint32_t max);      /* un SysEx completo F0..F7, o 0 */
int  fm1_midi_get_short(uint8_t msg[3]);                  /* un mensaje corto, o 0 */

/* ---------------------------------------------------------------- consola (fm1_consola.c) */
enum {
    FM1_STAT_VERSION, FM1_STAT_UPTIME, FM1_STAT_SYSCLK, FM1_STAT_LSBCLK,
    FM1_STAT_HALVES, FM1_STAT_LATE, FM1_STAT_RENDER_LAST, FM1_STAT_RENDER_MAX, FM1_STAT_RENDER_AVG, FM1_STAT_BUDGET,
    FM1_STAT_SCAN, FM1_STAT_SCANISR_MAX, FM1_STAT_UI_LAST, FM1_STAT_UI_MAX, FM1_STAT_USB, FM1_STAT_FAILS,
    FM1_STAT_PAST1, FM1_STAT_PAST2, FM1_STAT_PAST3, FM1_STAT_PAST4,
    FM1_STAT_AUDIO_CPU, FM1_STAT_MIDI_IN, FM1_STAT_MIDI_OUT, FM1_STAT_LOOP_MAX,
    FM1_STAT_HEAP_FREE, FM1_STAT_HEAP_MIN,          /* montón del SDK: libre según malloc y según la RAM */
    FM1_STAT_N
};
extern volatile uint32_t fm1_stat[FM1_STAT_N];      /* tiempos en microsegundos */
void fm1_consola(const uint8_t *sysex, uint32_t n);
void fm1_log(const char *s);                        /* registro de texto en RAM (consola: log) */
uint32_t fm1_log_read(uint32_t off, uint8_t *dst, uint32_t max);
int  fm1_lab(int cmd, int arg);                     /* pruebas por partes (fm1_main.c) */
/* toma que llega por la consola (tools/fm1/toma.py): sub 0 preparar · 1 datos (a = desde) · 2 lista
 * (a = muestras, b = bpm_x10). Devuelve 0 si se aceptó. Se guarda en la flash al parar, como una grabada. */
int  fm1_toma(int sub, uint32_t a, uint32_t b, const uint8_t *nib, uint32_t nn);
/* paquete de idioma (tools/fm1/idioma.py): sub 0 borrar la zona · 1 datos (off = desde) · 2 revisar y
 * activar · 3 quitar (vuelve el inglés). Solo con el transporte parado. Devuelve 0 si se aceptó. */
int  fm1_idioma(int sub, uint32_t off, const uint8_t *nib, uint32_t nn);
uint32_t fm1_scan_now_us(void);                     /* reloj de microsegundos (TIMER5) */
uint32_t fm1_scan_now_ticks(void);                  /* ticks del timer (~60 MHz), para tramos cortos */
uint32_t fm1_scan_ticks_hz(void);
int  fm1_bench(int cmd, int arg);
void fm1_heap_medir(void);                          /* memoria libre del montón -> fm1_stat (desde una tarea) */                   /* pruebas de la arquitectura (fm1_bench.c) */
void fm1_prof_reset(void);
int  fm1_prof_read(uint32_t *out);                  /* perfil del audio por sección (core/bm_prof.h) */

/* ---------------------------------------------------------------- actualización */
typedef void (*fm1_update_show)(int step, int code);
void fm1_update_poll(fm1_update_show show);               /* saludo y sesión de actualización */
void fm1_update_boot_cleanup(void);

#endif
