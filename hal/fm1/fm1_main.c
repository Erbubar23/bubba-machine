/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine en el FM-1: el núcleo (core/) sobre el SDK AC79 de JieLi.
 *
 * Qué corre dónde:
 *   - interrupción de ALNK0 (fm1_audio.c): el render del núcleo, 256 frames por medio buffer
 *     (5.8 ms), como pide SPEC_V3 §2;
 *   - interrupción de TIMER5 (fm1_scanhw.c): la matriz (antirrebote y perillas) y los LEDs;
 *   - tarea "bubba" (este archivo): botones, teclas y perillas hacia el núcleo, MIDI por USB,
 *     MASTER, la pantalla (bm_app_tick dibuja lo que cambió), el guardado y el servicio de
 *     actualización.
 *
 * Salidas de emergencia, las mismas del diagnóstico (fm1_boot.c): dos arranques fallidos =
 * modo seguro (sin sonido ni núcleo, solo pantalla y USB para reinstalar); cuatro = modo de
 * actualización del chip; OCT- y OCT+ juntos 5 s = modo del chip.
 *
 * Cada arranque anota hasta qué etapa llegó (E_*) y el modo seguro muestra las de los arranques
 * anteriores y cómo terminaron (apagado, perro guardián, reinicio por falla). Desde el modo
 * seguro, PLAY arranca Bubba Machine sin salida de audio (el núcleo corre igual, callado): así
 * se separa una falla del audio de una del resto. */
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "asm/adc_api.h"
#include "fm1_hw.h"
#include "bm_app.h"

const struct irq_info irq_info_table[] = {
    {-1, -1, -1},
};

const struct task_info task_info_table[] = {
    {"app_core", 15, 2048, 1024},
    {"sys_event", 29, 512, 0},
    {"systimer", 14, 256, 0},
    {"sys_timer", 9, 512, 128},
    {0, 0, 0, 0, 0},
};

#define HALF_FRAMES 256u                     /* múltiplo de BM_BLOCK * BM_OVERSAMPLE */
#define OK_MS       10000u                   /* tanto andando bien = el arranque fue bueno */
#define UBOOT_MS    5000u                    /* OCT- y OCT+ juntos: modo del chip */

enum { E_PANTALLA = 1, E_NUCLEO, E_TECLAS, E_AUDIO, E_SONANDO, E_1S, E_5S };
static const char *const ETAPA[9] = {"ANTES DE LA PANTALLA", "PANTALLA Y USB", "NUCLEO LISTO", "TECLAS",
                                     "AUDIO INICIADO", "AUDIO SONANDO", "1 S ANDANDO", "5 S ANDANDO", "5 S ANDANDO"};

static bm_app app;
static int32_t dma[2 * HALF_FRAMES * 2];
static bm_screen scr;                        /* pantallas propias: modo seguro y actualización */

/* perilla física (orden de la matriz) -> papel en el núcleo. SELECT, ALGORITHM y PRESETS por
 * su serigrafía; KNOB 1-4 en el orden que publicó Felucca (el diagnóstico lo confirma). */
static const uint8_t ENC_ROLE[7] = {ENC_SELECT, ENC_ALGO, ENC_K1, ENC_K2, ENC_K3, ENC_K4, ENC_PRESETS};

/* ------------------------------------------------------------------ audio */
static void render(int32_t *lr, uint32_t frames)
{
    bm_app_audio(&app, lr, frames);
    for (uint32_t i = 0; i < frames * 2u; i++)
        lr[i] = (int32_t)((uint32_t)lr[i] << 8);     /* 24 bits a la izquierda de la palabra */
}

/* ------------------------------------------------------------------ MIDI */
/* el núcleo manda bytes sueltos desde la interrupción de audio; la tarea los junta en mensajes */
#define MO_LEN 512u
static volatile uint8_t mo[MO_LEN];
static volatile uint16_t mo_w, mo_r;

static void midi_out(void *u, uint8_t b)
{
    uint16_t n = (uint16_t)((mo_w + 1u) % MO_LEN);
    (void)u;
    if (n != mo_r) {
        mo[mo_w] = b;
        mo_w = n;
    }
}

static uint8_t largo(uint8_t st)             /* bytes de un mensaje con este estado */
{
    if (st >= 0xF8u) return 1;
    if (st == 0xF1u || st == 0xF3u) return 2;
    if (st == 0xF2u) return 3;
    if (st >= 0xF0u) return 1;
    if ((st & 0xF0u) == 0xC0u || (st & 0xF0u) == 0xD0u) return 2;
    return 3;
}

static void midi_bombear(void)
{
    static uint8_t m[3], n, need, run;
    uint8_t s[3];
    while (mo_r != mo_w) {
        uint8_t b = mo[mo_r];
        mo_r = (uint16_t)((mo_r + 1u) % MO_LEN);
        if (b >= 0xF8u) {                    /* tiempo real: en cualquier momento */
            fm1_midi_send(&b, 1);
            continue;
        }
        if (b & 0x80u) {
            m[0] = b;
            n = 1;
            need = largo(b);
            run = b < 0xF0u ? b : 0;
        } else if (n == 0 && run) {          /* estado implícito */
            m[0] = run;
            m[1] = b;
            n = 2;
            need = largo(run);
        } else if (n && n < 3) {
            m[n++] = b;
        }
        if (n && n == need) {
            fm1_midi_send(m, n);
            fm1_stat[FM1_STAT_MIDI_OUT]++;
            n = 0;
        }
    }
    while (fm1_midi_get_short(s)) {          /* lo que llega del computador */
        fm1_stat[FM1_STAT_MIDI_IN]++;
        uint8_t k = largo(s[0]);
        for (uint8_t i = 0; i < k; i++)
            bm_app_midi_in(&app, s[i]);
    }
}

/* ------------------------------------------------------------------ plataforma */
static void led(void *u, int id, uint8_t level) { (void)u; fm1_scan_led(id, level); }
static const uint8_t *mapa(void *u, uint32_t a) { (void)u; return fm1_flash_map(a); }
static const bm_flash_ops FLASH = {fm1_flash_read, fm1_flash_erase, fm1_flash_prog};

/* ------------------------------------------------------------------ pantallas propias */
static int upd_step, upd_code, en_actualizacion;
static uint32_t combo_t0;

static void dibujar_aviso(bm_gfx *g, void *u)
{
    int seguro = u != NULL;
    gfx_text(g, 8, 8, "BUBBA MACHINE", C_YELLOW, 2);
    gfx_text(g, 8, 28, BM_VERSION_STR " - " FM1_IDENT, C_GREY, 1);
    if (seguro) {
        static const char *const FIN[] = {"-", "APAGADO", "PERRO GUARDIAN", "REINICIO POR FALLA", "OTRO"};
        char b[48];
        gfx_text(g, 8, 44, "MODO SEGURO", C_ORANGE, 2);
        gfx_text(g, 8, 64, "SIN SONIDO, USB ACTIVO. INSTALADOR WEB:", C_WHITE, 1);
        gfx_text(g, 8, 74, "REINSTALAR O VOLVER AL DE FABRICA.", C_WHITE, 1);
        gfx_text(g, 8, 90, "ARRANQUES ANTERIORES:", C_GREY, 1);
        for (int k = 1; k <= 3; k++) {
            uint8_t e, fin, ok;
            char *p = b;
            if (!fm1_boot_past(k, &e, &fin, &ok))
                break;
            *p++ = (char)('0' + k);
            *p++ = ' ';
            for (const char *q = ETAPA[e > 8 ? 8 : e]; *q;)
                *p++ = *q++;
            *p = 0;
            gfx_text(g, 8, 80 + k * 22, b, ok ? C_GREEN : C_YELLOW, 1);
            p = b;
            for (const char *q = "  FIN: "; *q;)
                *p++ = *q++;
            for (const char *q = ok ? "BIEN" : FIN[fin >= 1 && fin <= 4 ? fin : 0]; *q;)
                *p++ = *q++;
            *p = 0;
            gfx_text(g, 8, 90 + k * 22, b, C_GREY, 1);
        }
        gfx_text(g, 8, 160, fm1_midi_ready() ? "USB: MIDI LISTO" : fm1_midi_on() ? "USB: ESPERANDO AL COMPUTADOR" : "USB: APAGADO",
                 fm1_midi_ready() ? C_GREEN : C_GREY, 1);
        gfx_text(g, 8, 172, "PLAY: ARRANCAR SIN SALIDA DE AUDIO", C_CYAN, 1);
    }
    if (upd_step) {
        static const char *const PASO[] = {"", "LEYENDO PAQUETE", "REVISANDO ARRANQUE", "COPIANDO CARGADOR",
                                           "ESPERANDO CONFIRMACION", "REINICIANDO PARA ESCRIBIR"};
        gfx_fill(g, 0, 180, 240, 20, upd_step == 9 ? C_RED : C_YELLOW);
        gfx_text(g, 8, 186, upd_step == 9 ? "ACTUALIZACION RECHAZADA" : PASO[upd_step < 6 ? upd_step : 0], C_BLACK, 1);
        if (!seguro && upd_step != 9)
            gfx_text(g, 8, 52, "ACTUALIZANDO: NO DESCONECTES", C_YELLOW, 1);
    }
    if (combo_t0) {
        gfx_fill(g, 0, 204, 240, 18, C_ORANGE);
        gfx_text(g, 8, 209, "MODO DEL CHIP...", C_BLACK, 1);
    } else {
        gfx_text(g, 8, 210, "OCT- + OCT+ 5 S: MODO DEL CHIP", C_DGREY, 1);
    }
}

static int modo, sin_audio;

static void paso_actualizacion(int step, int code)
{
    upd_step = step;
    upd_code = code;
    if (modo == FM1_BOOT_NORMAL && !en_actualizacion && step != 9) {
        en_actualizacion = 1;                /* empieza una sesión: silencio y la pantalla es suya */
        fm1_audio_stop();
        fm1_scan_stop();
        scr.force = 1;
    }
    bm_screen_render(&scr, dibujar_aviso, modo == FM1_BOOT_SAFE ? (void *)1 : NULL, fm1_lcd_blit, NULL);
    if (step == 9 && en_actualizacion) {     /* rechazada: nada cambió, se sigue tocando */
        os_time_dly(150);
        en_actualizacion = 0;
        upd_step = 0;
        fm1_scan_start();
        if (sin_audio)
            fm1_audio_start_mute(dma, HALF_FRAMES, render);
        else
            fm1_audio_start(dma, HALF_FRAMES, render);
        app.scr.force = 1;
    }
}

/* ------------------------------------------------------------------ modo seguro */
/* vuelve solo si se pulsa PLAY (arrancar sin salida de audio) */
static void modo_seguro(void)
{
    uint8_t raw[FM1_NCOL];
    uint32_t t0 = timer_get_ms(), next = 0;
    int ok = 0;
    fm1_keys_init();
    scr.force = 1;
    for (;;) {
        uint32_t t = timer_get_ms();
        fm1_keys_frame(raw, NULL);
        if ((raw[10] >> 2) & 1u) {           /* PLAY (12): fila 2, columna 10 */
            fm1_boot_ok();                   /* esta prueba no suma arranques fallidos */
            return;
        }
        if (((raw[0] >> 4) & 1u) && ((raw[1] >> 4) & 1u)) {      /* OCT- (0) y OCT+ (1): fila 4 */
            if (!combo_t0)
                combo_t0 = t | 1u;
            else if (t - combo_t0 > UBOOT_MS)
                fm1_enter_uboot();
        } else {
            combo_t0 = 0;
        }
        fm1_update_poll(paso_actualizacion);
        if ((int32_t)(t - next) >= 0) {
            next = t + 200u;
            bm_screen_render(&scr, dibujar_aviso, (void *)1, fm1_lcd_blit, NULL);
        }
        if (!ok && t - t0 > OK_MS) {         /* el modo seguro anda: el próximo arranque prueba normal */
            fm1_boot_ok();
            ok = 1;
        }
        os_time_dly(1);
    }
}

/* ------------------------------------------------------------------ tarea principal */
static void bubba_task(void *p)
{
    uint8_t raw[FM1_NCOL];
    uint32_t t0, next_adc = 0;
    int ok = 0, id, down;
    (void)p;

    fm1_flash_init();
    fm1_keys_init();
    for (int i = 0; i < 3; i++)              /* OCT- y OCT+ al prender = modo seguro */
        fm1_keys_frame(raw, NULL);
    modo = fm1_boot_begin(((raw[0] >> 4) & 1u) && ((raw[1] >> 4) & 1u));
    fm1_update_boot_cleanup();
    fm1_lcd_init();
    fm1_midi_init();
    adc_add_sample_ch(FM1_ADC_MASTER);
    adc_add_sample_ch(FM1_ADC_BATT);
    fm1_boot_stage(E_PANTALLA);
    if (modo == FM1_BOOT_SAFE) {
        modo_seguro();                       /* vuelve solo con PLAY */
        sin_audio = 1;
        modo = FM1_BOOT_NORMAL;
    }

    bm_platform pf = {fm1_lcd_blit, led, midi_out, mapa, &FLASH, NULL};
    t0 = timer_get_ms();
    fm1_stat[FM1_STAT_VERSION] = 0x000300u | (uint32_t)sin_audio << 24;
    bm_app_init(&app, &pf, t0);
    fm1_boot_stage(E_NUCLEO);
    bm_app_master(&app, (uint16_t)adc_get_value(FM1_ADC_MASTER));
    bm_app_battery(&app, 100);               /* sin curva medida todavía: ver README_PORT.md */
    fm1_scan_start();
    fm1_boot_stage(E_TECLAS);
    if (sin_audio)
        fm1_audio_start_mute(dma, HALF_FRAMES, render);
    else
        fm1_audio_start(dma, HALF_FRAMES, render);
    fm1_boot_stage(E_AUDIO);

    for (;;) {
        uint32_t t = timer_get_ms();
        while ((id = fm1_scan_edge(&down)) >= 0) {
            if (id < BTN_COUNT)
                bm_app_button(&app, id, down, t);
            else
                bm_app_key(&app, id - BM_LED_KEY0, down, t);
        }
        for (int e = 0; e < 7; e++) {
            int d = fm1_scan_encoder(e);
            if (d)
                bm_app_encoder(&app, ENC_ROLE[e], d, t);
        }
        if (fm1_scan_is_down(BTN_OCT_DN) && fm1_scan_is_down(BTN_OCT_UP)) {
            if (!combo_t0)
                combo_t0 = t | 1u;
            else if (t - combo_t0 > UBOOT_MS) {
                fm1_audio_stop();
                fm1_scan_stop();
                fm1_enter_uboot();
            }
        } else {
            combo_t0 = 0;
        }
        midi_bombear();
        fm1_update_poll(paso_actualizacion);
        if ((int32_t)(t - next_adc) >= 0) {
            next_adc = t + 10u;
            bm_app_master(&app, (uint16_t)adc_get_value(FM1_ADC_MASTER));
        }
        uint32_t u0 = fm1_scan_now_us();
        bm_app_tick(&app, t);
        uint32_t du = fm1_scan_now_us() - u0;
        fm1_stat[FM1_STAT_UI_LAST] = du;
        if (du > fm1_stat[FM1_STAT_UI_MAX])
            fm1_stat[FM1_STAT_UI_MAX] = du;
        if (fm1_audio_halves() > 20u)
            fm1_boot_stage(E_SONANDO);
        if (t - t0 > 1000u)
            fm1_boot_stage(E_1S);
        if (t - t0 > 5000u)
            fm1_boot_stage(E_5S);
        if (!ok && t - t0 > OK_MS) {         /* 10 s andando: este arranque fue bueno */
            fm1_boot_ok();
            ok = 1;
        }
        os_time_dly(1);
    }
}

void app_main(void)
{
    os_task_create(bubba_task, NULL, 10, 4096, 0, "bubba");
}
