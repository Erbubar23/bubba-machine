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
    /* el codec toma los 24 bits de abajo de cada palabra, con signo (dato de Felucca: Q15 * 2^7,
     * audio.c). Antes se corrían 8 bits hacia arriba y en el aparato sonaba como una alarma. */
    for (uint32_t i = 0; i < frames * 2u; i++) {
        int32_t v = lr[i];
        lr[i] = v > 8388607 ? 8388607 : v < -8388608 ? -8388608 : v;
    }
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
    case 20: case 21: case 22: case 23: case 24: case 25: case 28: case 34:  /* traza y pruebas de la arquitectura (fm1_bench.c) */
        if (!fm1_scan_ticks()) {             /* el reloj de las mediciones es el barrido */
            fm1_scan_start();
            lab_scan = 1;
            os_time_dly(2);
        }
        {
            fm1_perro(0);                    /* una prueba larga no es un cuelgue */
            int r = fm1_bench(cmd, arg);
            fm1_perro(1);
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

/* ------------------------------------------------------------------ tarea principal */
static void bubba_task(void *p)
{
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
    fm1_boot_stage(E_PANTALLA);
    {
        char c[5];
        if (fm1_cuelgue_previo(c)) {
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
        modo_seguro();                       /* vuelve solo con PLAY (o el laboratorio) */
        sin_audio = !lab_bubba;
        modo = FM1_BOOT_NORMAL;
    }

    migas_on = 1;
    fm1_miga("A0");
    bm_platform pf = {fm1_lcd_blit, led, midi_out, mapa, &FLASH, NULL};
    fm1_flash_plain_window();                /* por si el SDK la volvió a poner desde su almacén */
    t0 = timer_get_ms();
    fm1_stat[FM1_STAT_VERSION] = 0x000300u | (uint32_t)sin_audio << 24;
    fm1_miga("A1");
    bm_app_init(&app, &pf, t0);
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
        }
        uint32_t u0 = fm1_scan_now_us();
        fm1_miga("L6");
        bm_app_tick(&app, t);
        fm1_miga("L7");
        uint32_t du = fm1_scan_now_us() - u0;
        fm1_stat[FM1_STAT_UI_LAST] = du;
        if (du > fm1_stat[FM1_STAT_UI_MAX])
            fm1_stat[FM1_STAT_UI_MAX] = du;
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
