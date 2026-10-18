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
#include <stddef.h>
#include "app_config.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "asm/adc_api.h"
#include "asm/includes.h"
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
/* Freno de voces: si el bloque anterior estuvo a punto de pasarse (95 %) o el promedio sostenido
 * pasa del 85 %, se apaga (con su fade de 2 ms) la voz del pool que menos suena, una cada 4 bloques
 * mientras siga así. Mejor perder una voz que cortar todo el audio (0.6.1 en el aparato: 105-123 %
 * con un patrón de seis máquinas). En 0.6.2 bastaba un bloque al 85 %: con el promedio en ~57 % los
 * picos de los pasos con muchas notas apagaban ~1 voz por segundo y se comían el pad de BUNO-6.
 * Va antes del render: el otro núcleo todavía no está tocando las voces. */
#define FRENO_PICO  95u
#define FRENO_PROM  85u
#define FRENO_CADA  4u
/* 0.7.1: el freno baja un tope de voces del pool (bm_audio_overload) y las notas nuevas roban dentro
 * de él. Antes cada nota nueva sonaba y el freno la apagaba al rato: con la carga alta todo el
 * tiempo se callaban las voces (en el aparato, 6 por segundo a los 9 minutos). Con el promedio por
 * debajo del 70 % y ningún pico durante ALIVIO_CADA bloques (~0.4 s), el tope sube de a una. */
#define ALIVIO_PROM 70u
#define ALIVIO_CADA 64u
/* 0.8.1: with TWO CORES the brake aims lower (~65 %): both cores flat out drew enough current
 * to make the FM-1 switch itself off while browsing. And when the UI loop is starved by the audio
 * (bm_app_tick slower than UI_LENTO_US three times within UI_VENTANA renders, ~1 s, while the audio
 * is above half of its time), the cap goes down one voice too; it only goes back up after a
 * UI_VENTANA with no slow UI. Fast UI samples (ticks that draw nothing) neither count nor reset
 * anything; a slow UI with little audio is the UI's own and does not count. */
#define FRENO2_PICO  80u
#define FRENO2_PROM  65u
#define ALIVIO2_PROM 50u
#define UI_LENTO_US  60000u
#define UI_VENTANA   172u                   /* renders of 5.8 ms: ~1 s */
#define UI_AUDIO_MIN 50u                    /* % of the budget: below this a slow UI is the UI's own */

static volatile uint8_t par_sesion;         /* lab 36: the core 1 spins through a whole render (off since 0.8.1) */
static volatile uint16_t lab_par;           /* lab 36: 0x100 | arg, se aplica al empezar un render */
static volatile uint32_t ui_us, ui_n;       /* the UI loop's last bm_app_tick and how many there were (ui_medida) */

/* the hook from the main loop: how long the UI took this time */
static void ui_medida(uint32_t us)
{
    ui_us = us;
    ui_n++;
}

static void render(int32_t *lr, uint32_t frames)
{
    static uint32_t n, freno_n, alivio_n, ses, ui_visto, ui_lento_n[3], ui_k;
    uint32_t budget = fm1_stat[FM1_STAT_BUDGET];
    int dos = app.ctx.par_on;
    uint32_t pico = dos ? FRENO2_PICO : FRENO_PICO, prom = dos ? FRENO2_PROM : FRENO_PROM;
    uint32_t alivio = dos ? ALIVIO2_PROM : ALIVIO_PROM;
    int ui_lento = 0;
    n++;
    if (ui_n != ui_visto) {                  /* a new UI measurement */
        ui_visto = ui_n;
        if (ui_us > UI_LENTO_US && budget && fm1_stat[FM1_STAT_RENDER_AVG] * 100u > budget * UI_AUDIO_MIN) {
            ui_lento_n[ui_k] = n | 1u;
            ui_k = (ui_k + 1u) % 3u;
            ui_lento = ui_lento_n[ui_k] && n - ui_lento_n[ui_k] < UI_VENTANA;   /* the third one in the window */
        }
    }
    int ui_bien = !ui_lento_n[(ui_k + 2u) % 3u] || n - ui_lento_n[(ui_k + 2u) % 3u] > UI_VENTANA;
    if (budget && n - freno_n >= FRENO_CADA &&
        (fm1_stat[FM1_STAT_RENDER_LAST] * 100u > budget * pico ||
         fm1_stat[FM1_STAT_RENDER_AVG] * 100u > budget * prom || ui_lento)) {
        if (bm_audio_overload(&app.ctx))
            fm1_stat[FM1_STAT_FRENO]++;
        if (ui_lento) {
            fm1_stat[FM1_STAT_FRENO_UI]++;
            ui_lento_n[0] = ui_lento_n[1] = ui_lento_n[2] = 0;
            ui_lento_n[(ui_k + 2u) % 3u] = n | 1u;      /* the window starts again from this one */
        }
        freno_n = n;
    } else if (budget && app.ctx.pool_cap && n - freno_n >= ALIVIO_CADA && n - alivio_n >= ALIVIO_CADA &&
               fm1_stat[FM1_STAT_RENDER_AVG] * 100u < budget * alivio && ui_bien) {
        bm_audio_relief(&app.ctx);
        alivio_n = n;
    }
    /* tracks left to a core 1 that stopped answering: back to this core once it let them go */
    if (app.ctx.par_quar && fm1_cpu1_idle())
        app.ctx.par_quar = 0;
    if (lab_par) {                           /* entre renders: ningún núcleo está calculando */
        uint8_t a = (uint8_t)lab_par;
        lab_par = 0;
        par_sesion = !(a & 0x80u);
        app.ctx.par_auto = (uint8_t)((a >> 6) & 1u);
        if (!app.ctx.par_auto) app.ctx.par_mask = a & 0x3Fu;
    }
    int sesion = app.ctx.par_on && par_sesion;
    if (sesion) {
        if (++ses == 0) ses = 1;
        fm1_cpu1_sesion(ses);
    }
    bm_app_audio(&app, lr, frames);
    if (sesion)
        fm1_cpu1_sesion(0);
    {                                        /* para la consola: voces de cada canal (4 bits) y el tope del freno */
        uint32_t v = 0;
        for (int i = 0; i < BM_VOICES; i++)
            if (app.ctx.voice[i].active && app.ctx.voice[i].track < BM_TRACKS && ((v >> (app.ctx.voice[i].track * 4)) & 15u) < 15u)
                v += 1u << (app.ctx.voice[i].track * 4);
        fm1_stat[FM1_STAT_VOCES] = v | (uint32_t)(app.ctx.pool_cap ? app.ctx.pool_cap : BM_V_POOL_N) << 24;
    }
    /* el codec toma los 24 bits de abajo de cada palabra, con signo (dato de Felucca: Q15 * 2^7,
     * audio.c). Antes se corrían 8 bits hacia arriba y en el aparato sonaba como una alarma. */
    for (uint32_t i = 0; i < frames * 2u; i++) {
        int32_t v = lr[i];
        lr[i] = v > 8388607 ? 8388607 : v < -8388608 ? -8388608 : v;
    }
}

/* ------------------------------------------------------------------ audio en dos núcleos (a prueba)
 * SISTEMA > TWO CORES. El núcleo 1 calcula T4-T6 dentro de una interrupción (fm1_cpu1.c) mientras
 * este calcula T1-T3; el núcleo del audio hace el reparto (bm_audio_par_process) y ya probó en PC
 * que suena igual (test_dual_core). En 0.6.1 era una tarea con semáforos y el planificador del SDK
 * la turnaba con la del audio: más lento que un núcleo (docs/ARQUITECTURA_FM1.md §3). */
#define PAR_ESPERA_US 4000u                  /* un medio buffer dura 5.8 ms */
static int par_ok;

static void par_job(uint32_t arg)            /* en el núcleo 1: el tramo [done, done + n) */
{
    bm_audio_par_process(&app.ctx, arg >> 16, arg & 0xFFFFu);
}

static void par_go(void *u, uint32_t done, uint32_t n)
{
    (void)u;
    if (!fm1_cpu1_run(par_job, done << 16 | n))   /* not delivered: done here, and par_wait finds it done */
        par_job(done << 16 | n);
}

static volatile uint8_t par_fallo;          /* core 1 failed during this boot (the main loop writes it down) */

static int par_wait(void *u)
{
    (void)u;
    int r = fm1_cpu1_wait(PAR_ESPERA_US);
    if (r != 0) {
        /* el núcleo 1 no contestó: un núcleo desde ahora y hasta apagar. Its tracks stay silent
         * (bm_audio_render, par_quar) until it lets go of them; no waiting here (0.8.1: before, up
         * to 50 ms spinning). */
        par_ok = 0;
        app.ctx.par_on = 0;
        par_fallo = 1;
    }
    fm1_stat[FM1_STAT_PAR_CPU] = (uint32_t)fm1_cpu1_cpu();
    return r;
}

/* La primera vez que TWO CORES está prendido (no en cada arranque: si la prueba fallara mal, con
 * TWO CORES apagado el aparato arranca igual). Con el barrido andando: mide el tiempo con él. */
static void par_init(void)
{
    static int probado;
    if (probado) return;
    probado = 1;
    if (fm1_boot_c1_retired()) {             /* an earlier boot retired it and the FM-1 was not off since */
        fm1_cpu1_retirar();
        fm1_log("core 1 retired by an earlier boot: TWO CORES uses one core until power-off");
        return;
    }
    if (fm1_cpu1_init() != 0) return;
    app.ctx.par_go = par_go; app.ctx.par_wait = par_wait; app.ctx.par_user = NULL;
    app.ctx.par_mask = 0x38;                 /* para empezar T4-T6; después se ajusta solo (bm_audio_par_pick) */
    app.ctx.now = fm1_scan_now_ticks;
    app.ctx.par_auto = 1;
    par_ok = 1;
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


/* ------------------------------------------------------------------ migas de pan */
/* Dos caracteres en la esquina inferior derecha que dicen en qué paso está la tarea principal.
 * Si el aparato se cuelga, la pantalla se queda mostrando el último (sin USB ni consola, es lo
 * único que se puede leer). Solo desde la tarea principal: es la que usa la pantalla. */
static int migas_on;
static uint16_t min_sysvdd, min_vdc14, min_vbat, min_batt, now_sysvdd, now_vdc14, now_vbat, now_batt;
static int rastro_on;

extern volatile char fm1_miga_act[4];

void fm1_paso(const char *c)
{
    fm1_miga_act[2] = c[0];
    fm1_miga_act[3] = c[1];
}

void fm1_miga(const char *c)
{
    static uint16_t px[9 * 12];
    fm1_miga_act[0] = c[0];
    fm1_miga_act[1] = c[1];
    fm1_miga_act[2] = fm1_miga_act[3] = '-';
    if (rastro_on)                           /* the breadcrumb in RAM follows the step (fm1_boot.c) */
        fm1_rastro(c, min_sysvdd, min_vdc14, min_vbat, timer_get_ms());
    if (!migas_on)
        return;
    for (int i = 0; i < 9 * 12; i++)
        px[i] = 0x0000u;
    for (int k = 0; k < 2 && c[k]; k++) {
        const uint8_t *g = bm_font5x7[(uint8_t)c[k] - 32u];
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 5; x++)
                if ((g[y] >> (4 - x)) & 1u)
                    px[(y + 1) * 12 + k * 6 + x + 1] = C_YELLOW;
    }
    fm1_lcd_blit(NULL, 228, 231, 12, 9, px);
}

/* ------------------------------------------------------------------ flash */
void fm1_quiet(int on)                       /* fm1_flash.c: antes y después de borrar o escribir */
{
    static int n;
    if (on) {
        if (n++ == 0) {
            fm1_audio_pause(1);
            fm1_scan_pause(1);
        }
    } else if (n && --n == 0) {
        fm1_scan_pause(0);
        fm1_audio_pause(0);
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
        static const char *const FIN[] = {"-", "APAGADO", "PERRO GUARDIAN", "REINICIO POR FALLA", "OTRO",
                                          "VDDIO LOW (BROWNOUT)", "LONG PRESS"};
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
            for (const char *q = ok ? "BIEN" : FIN[fin >= 1 && fin <= 6 ? fin : 0]; *q;)
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
        fm1_perro(0);
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
        fm1_perro(1);
    }
}


/* ------------------------------------------------------------------ laboratorio */
/* Pruebas por partes lanzadas desde la computadora (tools/fm1/consola.py lab …) mientras el
 * aparato está en modo seguro, con un registro de texto en RAM que la consola lee. Así cada
 * pieza (barrido, audio, flash, núcleo) se prueba sola y, si algo se cuelga, se sabe qué fue. */
static char logbuf[4096];
static volatile uint32_t log_n;
static int lab_scan, lab_audio, lab_bubba;

static char *pon_num(char *p, uint32_t v)
{
    char t[11];
    int n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *p++ = t[--n];
    return p;
}

void fm1_log(const char *s)
{
    char l[96], *p = l;
    uint32_t t = timer_get_ms();
    *p++ = '[';
    p = pon_num(p, t / 1000u);
    *p++ = '.';
    uint32_t ms = t % 1000u;
    *p++ = (char)('0' + ms / 100u); *p++ = (char)('0' + ms / 10u % 10u); *p++ = (char)('0' + ms % 10u);
    *p++ = ']'; *p++ = ' ';
    while (*s && p < l + sizeof l - 2) *p++ = *s++;
    *p++ = '\n';
    CPU_CRITICAL_ENTER();
    for (char *q = l; q < p && log_n < sizeof logbuf; q++)
        logbuf[log_n++] = *q;
    CPU_CRITICAL_EXIT();
}

static void log_num(const char *a, uint32_t v, const char *b)
{
    char l[80], *p = l;
    while (*a) *p++ = *a++;
    p = pon_num(p, v);
    while (*b) *p++ = *b++;
    *p = 0;
    fm1_log(l);
}

uint32_t fm1_log_read(uint32_t off, uint8_t *dst, uint32_t max)
{
    uint32_t n = 0;
    while (off + n < log_n && n < max) {
        dst[n] = (uint8_t)logbuf[off + n];
        n++;
    }
    return n;
}

static uint32_t fase;
static void tono(int32_t *lr, uint32_t frames)         /* triángulo de 440 Hz, -12 dB */
{
    for (uint32_t i = 0; i < frames; i++) {
        fase += 42838u * 1000u;                         /* 440 / 44118 * 2^32 */
        int32_t v = (int32_t)(fase >> 8) - (1 << 23);   /* diente de sierra de 24 bits */
        v = (v < 0 ? -v : v) * 2 - (1 << 23);           /* triángulo */
        v >>= 2;                                        /* -12 dB, en los 24 bits de abajo */
        lr[2 * i] = lr[2 * i + 1] = v;
    }
}

#define LAB_SECTOR 0x0E1000u                            /* en la zona del cargador en espera: libre fuera de una actualización */

static int prueba_flash(int veces)
{
    static uint8_t pat[256], back[256];
    int malas = 0;
    uint32_t peor = 0;
    for (int k = 0; k < veces; k++) {
        for (int i = 0; i < 256; i++)
            pat[i] = (uint8_t)(i * 7 + k);
        uint32_t t0 = fm1_scan_now_us();
        if (fm1_flash_erase(LAB_SECTOR) || fm1_flash_prog(LAB_SECTOR, pat, 256) ||
            fm1_flash_read(LAB_SECTOR, back, 256) || memcmp(pat, back, 256))
            malas++;
        uint32_t dt = fm1_scan_now_us() - t0;
        if (dt > peor)
            peor = dt;
        os_time_dly(1);
    }
    fm1_flash_erase(LAB_SECTOR);
    log_num("flash: escrituras malas ", (uint32_t)malas, "");
    log_num("flash: la mas lenta (us) ", peor, "");
    return malas;
}

/* orden desde la consola: devuelve 0 si se hizo */
static bm_track lab_trk;                     /* el canal antes de lab 26 */
static int lab_trk_t = -1;
static int lab_set;                          /* lab 29: canal*64 + byte de bm_track */

/* one scratch for the console packets (language and sounds): they come one at a time, from the main loop */
static uint8_t consola_d[256];

int fm1_idioma(int sub, uint32_t off, const uint8_t *nib, uint32_t nn)
{
    uint8_t *d = consola_d;
    uint32_t n = nn / 2;
    if (n > sizeof consola_d)
        return -1;
    for (uint32_t i = 0; i < n; i++)
        d[i] = (uint8_t)((nib[2 * i] & 15u) << 4 | (nib[2 * i + 1] & 15u));
    int r = bm_app_lang_load(&app, sub, off, d, n);
    if (sub != 1) log_num(r ? "consola: idioma rechazado, paso " : "consola: idioma, paso ", (uint32_t)sub, "");
    return r;
}

int fm1_sonido(int sub, uint32_t off, const uint8_t *nib, uint32_t nn)
{
    uint8_t *d = consola_d;
    uint32_t n = nn / 2;
    if (n > sizeof consola_d)
        return -1;
    for (uint32_t i = 0; i < n; i++)
        d[i] = (uint8_t)((nib[2 * i] & 15u) << 4 | (nib[2 * i + 1] & 15u));
    int r = bm_app_sound_load(&app, sub, off, d, n);
    if (sub != 1) log_num(r ? "consola: sonido rechazado, paso " : "consola: sonido, paso ", (uint32_t)sub, "");
    return r;
}

/* consola 23: sub 0 = resumen (montón libre y mínimo + bm_app_report), 1 = toma idx, 2 = sonido del
 * usuario idx, 3 = borrar la toma idx (en STOP, como N11 en TOMAS). Devuelve el largo o -1. */
int fm1_informe(int sub, int idx, uint8_t *out, int cap)
{
    if (sub == 0) {
        if (cap < 8 + BM_REPORT_LEN) return -1;
        uint32_t h = fm1_stat[FM1_STAT_HEAP_FREE], m = fm1_stat[FM1_STAT_HEAP_MIN];
        for (int i = 0; i < 4; i++) { out[i] = (uint8_t)(h >> (8 * i)); out[4 + i] = (uint8_t)(m >> (8 * i)); }
        int n = bm_app_report(&app, 0, 0, out + 8, cap - 8);
        return n < 0 ? -1 : 8 + n;
    }
    if (sub == 1 || sub == 2) return bm_app_report(&app, sub, idx, out, cap);
    if (sub == 3) {
        if (idx < 0 || idx >= BM_TAKES || !app.take[idx].valid) return -1;
        bm_app_take_delete(&app, idx);
        log_num("consola: borrar la toma ", (uint32_t)idx + 1u, "");
        return 0;
    }
    return -1;
}

int fm1_toma(int sub, uint32_t a, uint32_t b, const uint8_t *nib, uint32_t nn)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    if (sub == 1) {                                       /* datos: a la arena, que el audio ya soltó */
        uint32_t n = nn / 2;
        if (a > BM_SMP_REC_MAX || n > BM_SMP_REC_MAX - a)
            return -1;
        uint8_t *d = app.amem.arena + a;
        for (uint32_t i = 0; i < n; i++)
            d[i] = (uint8_t)((nib[2 * i] & 15u) << 4 | (nib[2 * i + 1] & 15u));
        return 0;
    }
    c.type = CMD_SMP_IMPORT;
    c.a = (uint8_t)(sub == 2);
    if (sub == 2) { c.u.i = (int32_t)a; c.b = (uint8_t)(b >> 8); c.c = (uint8_t)b; }
    else if (sub != 0) return -1;
    if (sub == 0) fm1_log("consola: llega una toma");
    else log_num("consola: toma lista, muestras ", a, "");
    return bm_cmd_push(&app.cmdq, &c) ? 0 : -1;
}

int fm1_lab(int cmd, int arg)
{
    switch (cmd) {
    case 1:
        fm1_log("lab: barrido (TIMER5) en marcha");
        fm1_scan_start();
        lab_scan = 1;
        return 0;
    case 2:
        fm1_log("lab: audio ALNK0 en silencio");
        fm1_audio_stop();
        fm1_audio_start(dma, HALF_FRAMES, NULL);
        lab_audio = 1;
        return 0;
    case 3:
        fm1_log("lab: audio ALNK0 con tono de 440 Hz");
        fm1_audio_stop();
        fm1_audio_start(dma, HALF_FRAMES, tono);
        lab_audio = 1;
        return 0;
    case 4:
        log_num("lab: prueba de flash, veces ", (uint32_t)arg, "");
        return prueba_flash(arg ? arg : 10);
    case 5:
        fm1_log("lab: audio detenido");
        fm1_audio_stop();
        lab_audio = 0;
        return 0;
    case 6:
        fm1_log("lab: arrancar Bubba completo con audio");
        lab_bubba = 1;
        return 0;
    case 20: case 21: case 22: case 23: case 24: case 25: case 28: case 34: case 35:  /* traza y pruebas de la arquitectura (fm1_bench.c) */
        if ((cmd == 28 || cmd == 34) && modo != FM1_BOOT_NORMAL) {
            /* los motores usan sus tablas (msfa, bancos de sonidos), que arma bm_app_init: en el modo
             * seguro no existen y la prueba del FM reiniciaba el chip (0.6.1, en el aparato) */
            fm1_log("prueba: los motores necesitan Bubba andando (PLAY o lab 6)");
            return -1;
        }
        if (!fm1_scan_ticks()) {             /* el reloj de las mediciones es el barrido */
            fm1_scan_start();
            lab_scan = 1;
            os_time_dly(2);
        }
        {
            /* una prueba larga no es un cuelgue; al terminar, el perro vuelve a como estaba: en el modo
             * seguro está apagado (nadie late) y prenderlo reiniciaba el chip a los 3 s (0.6.2) */
            int era = fm1_perro(0);
            int r = fm1_bench(cmd, arg);
            fm1_perro(era);
            return r;
        }
    case 7:                                  /* pulsar un botón (0..13) como si fuera la mano */
        if (modo != FM1_BOOT_NORMAL || arg >= BTN_COUNT)
            return -1;
        log_num("lab: boton ", (uint32_t)arg, " pulsado");
        bm_app_button(&app, arg, 1, timer_get_ms());
        os_time_dly(10);
        bm_app_button(&app, arg, 0, timer_get_ms());
        return 0;
    case 26: {                               /* canal (bits 4-6) al motor (bits 0-3), para medir motores; */
        if (modo != FM1_BOOT_NORMAL)         /* antes guarda el canal como estaba (lab 32 lo vuelve) */
            return -1;
        bm_cmd c;
        memset(&c, 0, sizeof c);
        c.type = CMD_ENGINE; c.a = (uint8_t)((arg >> 4) & 7); c.b = (uint8_t)(arg & 15);
        if (c.a >= BM_TRACKS)
            return -1;
        if (lab_trk_t < 0) {
            lab_trk = app.pat.trk[c.a];
            lab_trk_t = c.a;
        }
        log_num("lab: motor ", (uint32_t)arg, "");
        return bm_cmd_push(&app.cmdq, &c) ? 0 : -1;
    }
    case 32: {                               /* vuelve el canal que cambió lab 26 a como estaba */
        if (modo != FM1_BOOT_NORMAL || lab_trk_t < 0)
            return -1;
        bm_cmd c;
        memset(&c, 0, sizeof c);
        c.type = CMD_ENGINE; c.a = (uint8_t)lab_trk_t; c.b = lab_trk.engine;
        bm_cmd_push(&app.cmdq, &c);
        for (unsigned i = 0; i < sizeof lab_trk; i++) {
            if (i == offsetof(bm_track, engine))
                continue;
            c.type = CMD_SET_TRACK; c.b = (uint8_t)i; c.c = ((const uint8_t *)&lab_trk)[i];
            while (!bm_cmd_push(&app.cmdq, &c))
                os_time_dly(1);
        }
        log_num("lab: canal ", (uint32_t)lab_trk_t + 1, " como estaba");
        lab_trk_t = -1;
        return 0;
    }
    case 36:                                 /* reparto a mano: bits 0-5 canales al núcleo 1, bit 6 automático, */
        if (modo != FM1_BOOT_NORMAL)         /* bit 7 sin sesión (el núcleo 1 sale después de cada trabajo) */
            return -1;
        log_num("lab: reparto ", (uint32_t)arg, "");
        lab_par = (uint16_t)(0x100u | (arg & 0xFFu));
        return 0;
    case 29: lab_set = arg; return 0;        /* 29 canal*64 + byte de bm_track, luego 31 valor */
    case 31: {
        int t = lab_set >> 6, off = lab_set & 63;
        if (modo != FM1_BOOT_NORMAL || t >= BM_TRACKS || off >= (int)sizeof(bm_track))
            return -1;
        bm_cmd c;
        memset(&c, 0, sizeof c);
        if (off == offsetof(bm_track, engine)) { c.type = CMD_ENGINE; c.a = (uint8_t)t; c.b = (uint8_t)arg; }
        else { c.type = CMD_SET_TRACK; c.a = (uint8_t)t; c.b = (uint8_t)off; c.c = (uint8_t)arg; }
        return bm_cmd_push(&app.cmdq, &c) ? 0 : -1;
    }
    case 33: {                               /* el canal arg al registro: motor, fuente y macros */
        if (arg >= BM_TRACKS)
            return -1;
        const bm_track *k = &app.pat.trk[arg];
        log_num("lab: canal ", (uint32_t)arg + 1, "");
        log_num("  motor ", k->engine, "");
        log_num("  fuente ", k->source, "");
        for (int i = 0; i < 4; i++) log_num("  macro ", k->macro[i], "");
        log_num("  mute ", k->mute, "");
        log_num("  nivel ", k->level, "");
        log_num("  largo ", k->length, "");
        if (bm_kind(&app.pat, arg) == KIND_NOTE) {
            uint32_t ne = 0;
            for (int s = 0; s < BM_STEPS; s++)
                for (int e = 0; e < BM_EVENTS; e++) if (!bm_event_empty(bm_evc(&app.pat, arg, s, e))) ne++;
            log_num("  notas ", ne, "");
        }
        for (int i = 0; i < BM_VOICES; i++)
            if (app.ctx.voice[i].active && app.ctx.voice[i].track == arg) log_num("  voz activa ", (uint32_t)i, "");
        return 0;
    }
    case 27: {                               /* fuente (patch, kit, banco) del canal (bits 5-7) = bits 0-4 */
        if (modo != FM1_BOOT_NORMAL)
            return -1;
        bm_cmd c;
        memset(&c, 0, sizeof c);
        c.type = CMD_SET_TRACK; c.a = (uint8_t)((arg >> 5) & 7); c.b = (uint8_t)offsetof(bm_track, source); c.c = (uint8_t)(arg & 31);
        return bm_cmd_push(&app.cmdq, &c) ? 0 : -1;
    }
    }
    return -1;
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
    fm1_log("modo seguro");
    for (;;) {
        uint32_t t = timer_get_ms();
        if (lab_scan) {                      /* el barrido por interrupción ya lee la matriz */
            memset(raw, 0, sizeof raw);
            if (fm1_scan_is_down(BTN_PLAY)) raw[10] |= 4u;
            if (fm1_scan_is_down(BTN_OCT_DN)) raw[0] |= 16u;
            if (fm1_scan_is_down(BTN_OCT_UP)) raw[1] |= 16u;
        } else {
            fm1_keys_frame(raw, NULL);
        }
        if (((raw[10] >> 2) & 1u) || lab_bubba) {   /* PLAY (12): fila 2, columna 10 */
            fm1_boot_ok();                   /* esta prueba no suma arranques fallidos */
            fm1_audio_stop();
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

/* ------------------------------------------------------------------ rails and battery */
/* Every 10 ms from the main loop (the SDK's ADC scan reads them every few ms): the lowest SYSVDD,
 * VDC14 and VBAT seen since boot (after the first second: the ADC starts at nothing) go to the
 * breadcrumb in RAM and to the console. adc_get_voltage gives mV against the chip's band gap;
 * the VBAT channel is VBAT / 4. PB1 (the battery divider of the board, no curve yet) goes raw. */
static uint16_t baja(uint16_t m, uint16_t v) { return v && (!m || v < m) ? v : m; }

static void rieles(uint32_t ms)
{
    now_sysvdd = (uint16_t)adc_get_voltage(AD_CH_SYSVDD);
    now_vdc14 = (uint16_t)adc_get_voltage(AD_CH_VDC14);
    now_vbat = (uint16_t)(adc_get_voltage(AD_CH_VBAT) * 4u);
    now_batt = (uint16_t)adc_get_value(FM1_ADC_BATT);
    if (ms < 1000u)
        return;
    if (now_sysvdd > 500u) min_sysvdd = baja(min_sysvdd, now_sysvdd);
    if (now_vdc14 > 500u) min_vdc14 = baja(min_vdc14, now_vdc14);
    if (now_vbat > 1000u) min_vbat = baja(min_vbat, now_vbat);
    if (now_batt > 1u) min_batt = baja(min_batt, now_batt);
    fm1_stat[FM1_STAT_RAIL_MIN] = min_sysvdd | (uint32_t)min_vdc14 << 16;
    fm1_stat[FM1_STAT_RAIL_NOW] = now_sysvdd | (uint32_t)now_vdc14 << 16;
    fm1_stat[FM1_STAT_VBAT] = min_vbat | (uint32_t)now_vbat << 16;
    fm1_stat[FM1_STAT_BATT_ADC] = min_batt | (uint32_t)now_batt << 16;
}

/* the reset sources, the power setup and the previous run's breadcrumb: console and log */
static void informe_arranque(void)
{
    static const char H[] = "0123456789ABCDEF";
    uint8_t rst[5], fl = 0, prev = 0, r[8];
    char l[96], *p;
    if (fm1_boot_raw(0, rst, &fl)) {
        fm1_stat[FM1_STAT_RST_RAW] = rst[0] | (uint32_t)rst[1] << 8 | (uint32_t)rst[2] << 16 | (uint32_t)rst[4] << 24;
        (void)fm1_boot_raw(1, r, &prev);
        fm1_stat[FM1_STAT_RST_PMU] = rst[3] | (uint32_t)prev << 8 | (uint32_t)fl << 16;
        p = l;
        for (const char *q = "reset: P3_RST_SRC "; *q;) *p++ = *q++;
        *p++ = H[rst[0] >> 4]; *p++ = H[rst[0] & 15];
        for (const char *q = rst[0] & 2u ? " VDDIO LOW (brownout)" : rst[0] & 4u ? " WDT" : rst[0] & 16u ? " LONG PRESS" :
                             rst[0] & 1u ? " POWER ON" : " (soft or other)"; *q;) *p++ = *q++;
        for (const char *q = rst[4] & 0x80u ? ", LVD pending" : ""; *q;) *p++ = *q++;
        *p = 0;
        fm1_log(l);
    }
    fm1_stat[FM1_STAT_PWR0] = fm1_power_regs[0][0] | (uint32_t)fm1_power_regs[0][1] << 8 |
                              (uint32_t)fm1_power_regs[0][2] << 16 | (uint32_t)fm1_power_regs[0][3] << 24;
    fm1_stat[FM1_STAT_PWR1] = fm1_power_regs[1][0] | (uint32_t)fm1_power_regs[1][1] << 8 |
                              (uint32_t)fm1_power_regs[1][2] << 16 | (uint32_t)fm1_power_regs[1][3] << 24;
    if (fm1_stat[FM1_STAT_PWR1]) {
        fm1_boot_flag(FM1_BF_POWER);
        fm1_log("power: stock rails set (DCDC13, VDC14 1.60 V, SYSVDD 1.38 V)");
    }
    if (fm1_rastro_previo(r)) {
        fm1_stat[FM1_STAT_RASTRO0] = r[0] | (uint32_t)r[1] << 8 | (uint32_t)r[2] << 16 | (uint32_t)r[3] << 24;
        fm1_stat[FM1_STAT_RASTRO1] = r[4] | (uint32_t)r[5] << 8 | (uint32_t)r[6] << 16 | (uint32_t)r[7] << 24;
        p = l;
        for (const char *q = "breadcrumb of the last run: step "; *q;) *p++ = *q++;
        *p++ = (char)r[2]; *p++ = (char)r[3];
        *p = 0;
        fm1_log(l);
    }
}

/* ------------------------------------------------------------------ tarea principal */
static void bubba_task(void *p)
{
    int cuelgue = 0, volvio_seguro = 0, lcd_fast = -1, anotado = 0;
    uint8_t raw[FM1_NCOL];
    uint32_t t0, next_adc = 0, heap_t = 0;
    int ok = 0, id, down, seen = 0;
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
    adc_add_sample_ch(AD_CH_SYSVDD);         /* the rails, for the breadcrumb and the console */
    adc_add_sample_ch(AD_CH_VDC14);
    fm1_boot_stage(E_PANTALLA);
    informe_arranque();
    {
        uint32_t pc; uint8_t ce, db;
        if (fm1_excepcion_previa(&pc, &ce, &db)) {   /* antes que el perro: comparten los 8 bytes */
            cuelgue = 1;
            char l[96], *p = l;
            static const char H[] = "0123456789ABCDEF";
            static const char *const TK[8] = {"?", "bubba", "audio", "app_core", "sys_event", "timer", "-", "otra"};
            uint32_t eb = (ce >> 2) & 31u, dbb = db & 31u;
            const char *s = "falla: nucleo ";
            while (*s) *p++ = *s++;
            *p++ = (char)('0' + (ce & 1u));
            s = (ce & 0x80u) ? " RETI 0x" : " PC 0x"; while (*s) *p++ = *s++;
            for (int i = 28; i >= 0; i -= 4) *p++ = H[(pc >> i) & 15u];
            s = " MIGA "; while (*s) *p++ = *s++;
            *p++ = (char)('0' + eb / 10u); *p++ = (char)('0' + eb % 10u);
            s = " DBG "; while (*s) *p++ = *s++;
            *p++ = (char)('0' + dbb / 10u); *p++ = (char)('0' + dbb % 10u);
            s = " tarea "; while (*s) *p++ = *s++;
            s = TK[db >> 5]; while (*s) *p++ = *s++;
            if (ce & 2u) { s = " (reparto)"; while (*s) *p++ = *s++; }
            *p = 0;
            fm1_log(l);
        }
    }
    {
        char c[5];
        if (fm1_cuelgue_previo(c)) {
            cuelgue = 1;
            char l[40] = "reinicio por cuelgue en ";
            for (int i = 0; i < 4; i++) l[24 + i] = c[i];
            l[28] = 0;
            fm1_log(l);
            fm1_boot_note(c);
            fm1_stat[FM1_STAT_LOOP_MAX] = (uint32_t)(uint8_t)c[0] | (uint32_t)(uint8_t)c[1] << 8 |
                                          (uint32_t)(uint8_t)c[2] << 16 | (uint32_t)(uint8_t)c[3] << 24;
        }
    }
    if (modo == FM1_BOOT_SAFE) {
        volvio_seguro = 1;
        modo_seguro();                       /* vuelve solo con PLAY (o el laboratorio) */
        sin_audio = !lab_bubba;
        modo = FM1_BOOT_NORMAL;
    }

    migas_on = 1;
    rastro_on = 1;
    fm1_miga("A0");
    bm_platform pf = {fm1_lcd_blit, led, midi_out, mapa, &FLASH, NULL};
    fm1_flash_plain_window();                /* por si el SDK la volvió a poner desde su almacén */
    t0 = timer_get_ms();
    fm1_stat[FM1_STAT_VERSION] = 0x000300u | (uint32_t)sin_audio << 24;
    fm1_miga("A1");
    bm_app_init(&app, &pf, t0);
    if (cuelgue || volvio_seguro) {          /* algo falló: las pruebas (pantalla rápida, dos núcleos) se apagan */
        if (app.set.fast_lcd || app.set.dual_core) fm1_log("pruebas apagadas despues de un fallo");
        app.set.fast_lcd = 0; app.set.dual_core = 0; app.set_dirty = 1;
    }
    {                                        /* the last boot with TWO CORES did not reach 5 s: off */
        uint8_t e, fin, ok, rst[5], fl;
        if (app.set.dual_core && fm1_boot_past(1, &e, &fin, &ok) && fm1_boot_raw(1, rst, &fl) &&
            (fl & FM1_BF_DUAL) && e < E_5S) {
            fm1_log("TWO CORES off: the last boot with it did not reach 5 s");
            app.set.dual_core = 0; app.set_dirty = 1;
            fm1_stat[FM1_STAT_C1] |= 4u;
        }
    }
    if (fm1_boot_c1_retired())
        fm1_stat[FM1_STAT_C1] |= 2u;
    fm1_miga("A2");
    fm1_boot_stage(E_NUCLEO);
    fm1_miga("A3");
    bm_app_master(&app, (uint16_t)adc_get_value(FM1_ADC_MASTER));
    bm_app_battery(&app, 100);               /* sin curva medida todavía: ver README_PORT.md */
    if (!lab_scan)
        fm1_scan_start();
    fm1_miga("A4");
    fm1_boot_stage(E_TECLAS);
    fm1_miga("A5");
    fm1_log("nucleo listo, barrido en marcha");
    if (sin_audio)
        fm1_audio_start_mute(dma, HALF_FRAMES, render);
    else
        fm1_audio_start(dma, HALF_FRAMES, render);
    fm1_miga("A6");
    fm1_perro(1);                            /* desde aquí, un bucle quieto 3 s reinicia y lo anota */
    fm1_log(sin_audio ? "audio sin salida" : "audio ALNK0 en marcha");
    fm1_boot_stage(E_AUDIO);
    fm1_miga("A7");
    fm1_log("etapa AUDIO anotada");

    for (;;) {
        uint32_t t = timer_get_ms();
        fm1_miga("L1");
        while ((id = fm1_scan_edge(&down)) >= 0) {
            if (id < BTN_COUNT)
                bm_app_button(&app, id, down, t);
            else
                bm_app_key(&app, id - BM_LED_KEY0, down, t);
        }
        for (int e = 0; e < 7; e++) {
            int d = -fm1_scan_encoder(e);    /* el sentido de la cuadratura es el contrario (probado en el aparato) */
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
        fm1_miga("L3");
        midi_bombear();
        fm1_miga("L4");
        fm1_update_poll(paso_actualizacion);
        fm1_miga("L5");
        if ((int32_t)(t - next_adc) >= 0) {
            next_adc = t + 10u;
            bm_app_master(&app, (uint16_t)adc_get_value(FM1_ADC_MASTER));
            rieles(t - t0);
        }
        if (lcd_fast != app.set.fast_lcd) { lcd_fast = app.set.fast_lcd; fm1_lcd_speed(lcd_fast); }
        if (app.set.dual_core && !sin_audio)
            par_init();
        app.ctx.par_on = (uint8_t)(par_ok && app.set.dual_core && !sin_audio);
        if (app.ctx.par_on && !(anotado & 1)) {  /* the boot record: TWO CORES ran in this boot */
            anotado |= 1;
            fm1_boot_flag(FM1_BF_DUAL);
        }
        if (par_fallo && !(anotado & 2)) {       /* core 1 retired: kept until the FM-1 is off */
            anotado |= 2;
            fm1_boot_flag(FM1_BF_C1_RETIRED);
            fm1_stat[FM1_STAT_C1] |= 1u;
        }
        fm1_stat[FM1_STAT_C1] = (fm1_stat[FM1_STAT_C1] & ~8u) | (app.ctx.par_quar ? 8u : 0u);
        if (par_ok) {                        /* para la consola: el reparto y la carga estimada de cada núcleo */
            uint32_t mhz = fm1_scan_ticks_hz() / 1000000u, l0 = app.ctx.cost_over, l1 = 0, m = app.ctx.par_on ? app.ctx.par_mask : 0;
            for (int t = 0; t < BM_TRACKS; t++) { if ((m >> t) & 1u) l1 += app.ctx.cost[t]; else l0 += app.ctx.cost[t]; }
            fm1_stat[FM1_STAT_PAR_MASK] = m;
            fm1_stat[FM1_STAT_PAR_MISMO] = fm1_cpu1_mismo();
            fm1_stat[FM1_STAT_CARGA0] = mhz ? l0 / mhz : 0;
            fm1_stat[FM1_STAT_CARGA1] = mhz ? l1 / mhz : 0;
        }
        if (fm1_stat[FM1_STAT_BUDGET])
            bm_app_cpu(&app, (int)(fm1_stat[FM1_STAT_RENDER_AVG] * 100u / fm1_stat[FM1_STAT_BUDGET]));
        uint32_t u0 = fm1_scan_now_us();
        fm1_miga("L6");
        bm_app_tick(&app, t);
        fm1_miga("L7");
        uint32_t du = fm1_scan_now_us() - u0;
        fm1_stat[FM1_STAT_UI_LAST] = du;
        if (du > fm1_stat[FM1_STAT_UI_MAX])
            fm1_stat[FM1_STAT_UI_MAX] = du;
        if (du < 1000000u)
            ui_medida(du);
        {                                    /* the whole loop, from one turn to the next */
            static uint32_t lt;
            uint32_t lu = fm1_scan_now_us(), d = lu - lt;
            if (lt && d < 10000000u) {
                fm1_stat[FM1_STAT_LOOP_US] = d;
                if (d > fm1_stat[FM1_STAT_LOOP_US_MAX]) fm1_stat[FM1_STAT_LOOP_US_MAX] = d;
            }
            lt = lu;
        }
        if (fm1_audio_halves() > 20u && !(seen & 1)) {
            fm1_log("audio: 20 bloques calculados");
            fm1_boot_stage(E_SONANDO);
            fm1_log("etapa SONANDO anotada");
            seen |= 1;
        }
        if (t - t0 > 1000u)
            fm1_boot_stage(E_1S);
        if (t - t0 > 5000u)
            fm1_boot_stage(E_5S);
        if (t - heap_t > 5000u) {            /* memoria libre, para la consola */
            heap_t = t;
            fm1_heap_medir();
        }
        fm1_miga("L8");
        fm1_latido();
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
