/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Firmware de diagnóstico de Bubba Machine: lo primero que se instala en el FM-1.
 *
 * No hace sonido. Sirve para comprobar, con el aparato en la mano, lo que el firmware completo
 * necesita: que arranca, la pantalla, cada botón, tecla y perilla (con su cruce de columna y
 * fila en la matriz), los LEDs, el MASTER, la batería y el USB-MIDI (las teclas mandan notas).
 * Tiene el mismo servicio de actualización que el firmware completo: desde el instalador se
 * puede pasar a la versión completa o volver al firmware de fábrica.
 *
 * Salidas de emergencia (fm1_boot.c): dos arranques fallidos = modo seguro; cuatro = modo de
 * actualización del chip; OCT- y OCT+ juntos 5 s = modo de actualización del chip. */
#include "app_config.h"
#include "system/includes.h"
#include "os/os_api.h"
#include "asm/adc_api.h"
#include "fm1_hw.h"
#include "bm_gfx.h"

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

/* Mapa de la matriz: ID en (fila, columna); -1 = nada (fila 0 y 5: perillas). Dato medido y
 * publicado por Felucca/X0X. */
static const int8_t KEYMAP[FM1_NROW][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
/* perilla i: contacto A en (columna, fila) y B en (columna, fila) */
static const uint8_t ENC[7][4] = {
    {0, 0, 1, 0}, {2, 0, 3, 0}, {8, 1, 9, 1}, {8, 0, 9, 0}, {6, 0, 7, 0}, {4, 0, 5, 0}, {0, 5, 1, 5},
};

static struct {
    uint8_t raw[FM1_NCOL], led[FM1_NCOL];
    uint8_t down[41];
    int last_id, last_col, last_row;
    uint8_t q[7];
    int16_t enc[7];
    uint16_t master, batt;
    int modo, upd_step, upd_code;
    uint32_t t0, combo_t0;
    uint8_t ok_marcado;
} d;

static bm_screen scr;

static void txt(bm_gfx *g, int x, int y, const char *s, uint16_t c) { gfx_text(g, x, y, s, c, 1); }

static void num(char *b, int v)
{
    char t[12];
    int n = 0, neg = v < 0;
    unsigned u = (unsigned)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
    } while (u && n < 11);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static char *cat(char *b, const char *s)
{
    while (*s)
        *b++ = *s++;
    *b = 0;
    return b;
}

static void dibujar(bm_gfx *g, void *u)
{
    char b[48], *p;
    int c, r, i;
    (void)u;
    gfx_text(g, 8, 6, "BUBBA MACHINE", C_YELLOW, 2);
    txt(g, 8, 24, "DIAGNOSTICO " BM_VERSION_STR " - " FM1_IDENT, C_GREY);
    p = cat(b, d.modo == FM1_BOOT_SAFE ? "MODO SEGURO  " : "ARRANQUE NORMAL  ");
    p = cat(p, "FALLIDOS ");
    num(p, (int)fm1_boot_fails());
    txt(g, 8, 38, b, d.modo == FM1_BOOT_SAFE ? C_ORANGE : C_GREEN);
    txt(g, 8, 50, fm1_midi_ready() ? "USB: MIDI LISTO" : "USB: SIN COMPUTADOR", fm1_midi_ready() ? C_GREEN : C_GREY);
    /* matriz: 11 columnas x 6 filas */
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 0; r < FM1_NROW; r++) {
            int x = 8 + c * 14, y = 66 + r * 12;
            uint16_t col = (d.raw[c] >> r) & 1 ? C_YELLOW : (KEYMAP[r][c] >= 0 ? C_DGREY : C_DDGREY);
            gfx_fill(g, x, y, 12, 10, col);
        }
    txt(g, 168, 66, "TECLA", C_GREY);
    if (d.last_id >= 0) {
        num(b, d.last_id);
        gfx_text(g, 168, 78, b, C_WHITE, 2);
        p = cat(b, "C");
        num(p, d.last_col);
        p = cat(p + (d.last_col > 9 ? 2 : 1), " F");
        num(p, d.last_row);
        txt(g, 168, 98, b, C_GREY);
    }
    /* perillas */
    for (i = 0; i < 7; i++) {
        int x = 8 + (i % 4) * 58, y = 142 + (i / 4) * 14;
        p = cat(b, "E");
        num(p, i);
        p = cat(p + 1, ":");
        num(p, d.enc[i]);
        txt(g, x, y, b, C_CYAN);
    }
    p = cat(b, "MASTER ");
    num(p, d.master);
    txt(g, 8, 174, b, C_WHITE);
    p = cat(b, "BATERIA ");
    num(p, d.batt);
    txt(g, 120, 174, b, C_WHITE);
    if (d.upd_step) {
        static const char *const PASO[] = {"", "LEYENDO PAQUETE", "REVISANDO ARRANQUE", "COPIANDO CARGADOR",
                                           "ESPERANDO CONFIRMACION", "REINICIANDO PARA ESCRIBIR"};
        if (d.upd_step == 9) {
            p = cat(b, "ACTUALIZACION RECHAZADA ");
            num(p, d.upd_code);
            txt(g, 8, 190, b, C_RED);
        } else {
            txt(g, 8, 190, PASO[d.upd_step < 6 ? d.upd_step : 0], C_YELLOW);
        }
    }
    if (d.combo_t0) {
        uint32_t t = (timer_get_ms() - d.combo_t0) / 1000u;
        p = cat(b, "MODO DEL CHIP EN ");
        num(p, t < 5 ? (int)(5 - t) : 0);
        gfx_fill(g, 0, 204, 240, 18, C_ORANGE);
        txt(g, 8, 209, b, C_BLACK);
    } else {
        txt(g, 8, 206, "OCT- + OCT+ 5 S: MODO DEL CHIP", C_DGREY);
    }
    txt(g, 8, 222, "LAS TECLAS MANDAN NOTAS POR USB", C_DGREY);
}

static void paso_actualizacion(int step, int code)
{
    d.upd_step = step;
    d.upd_code = code;
    bm_screen_render(&scr, dibujar, NULL, fm1_lcd_blit, NULL);
}

static void teclas(void)
{
    int c, r, i;
    fm1_keys_frame(d.raw, d.led);
    for (r = 0; r < FM1_NROW; r++)
        for (c = 0; c < FM1_NCOL; c++) {
            int id = KEYMAP[r][c], on = (d.raw[c] >> r) & 1;
            if (id < 0 || on == d.down[id])
                continue;
            d.down[id] = (uint8_t)on;
            if (on) {
                d.last_id = id;
                d.last_col = c;
                d.last_row = r;
            }
            if (id >= 14) {
                uint8_t m[3] = {(uint8_t)(on ? 0x90 : 0x80), (uint8_t)(53 + id - 14), (uint8_t)(on ? 100 : 0)};
                fm1_midi_send(m, 3);
            }
        }
    for (c = 0; c < FM1_NCOL; c++)
        d.led[c] = d.raw[c] & 0x1Eu;              /* el LED de lo que se aprieta */
    for (i = 0; i < 7; i++) {                     /* cuadratura: + = horario */
        uint8_t a = (d.raw[ENC[i][0]] >> ENC[i][1]) & 1, b = (d.raw[ENC[i][2]] >> ENC[i][3]) & 1;
        uint8_t s = (uint8_t)(a << 1 | b), idx = (uint8_t)(d.q[i] << 2 | s);
        if ((0x4182u >> idx) & 1u)
            d.enc[i]++;
        else if ((0x2814u >> idx) & 1u)
            d.enc[i]--;
        d.q[i] = s;
    }
    /* OCT- y OCT+ juntos 5 s: modo de actualización del chip */
    if (d.down[FM1_B_OCTDN] && d.down[FM1_B_OCTUP]) {
        if (!d.combo_t0)
            d.combo_t0 = timer_get_ms() | 1u;
        else if (timer_get_ms() - d.combo_t0 > 5000u)
            fm1_enter_uboot();
    } else {
        d.combo_t0 = 0;
    }
}

static void diag_task(void *p)
{
    uint32_t next_ui = 0, next_adc = 0;
    int safe_keys;
    (void)p;
    fm1_flash_init();
    fm1_keys_init();
    for (int i = 0; i < 3; i++)                   /* OCT- y OCT+ al prender = modo seguro */
        fm1_keys_frame(d.raw, NULL);
    safe_keys = ((d.raw[0] >> 4) & 1) && ((d.raw[1] >> 4) & 1);
    d.modo = fm1_boot_begin(safe_keys);
    fm1_update_boot_cleanup();
    d.last_id = -1;
    d.t0 = timer_get_ms();
    fm1_lcd_init();
    fm1_midi_init();
    adc_add_sample_ch(FM1_ADC_MASTER);
    adc_add_sample_ch(FM1_ADC_BATT);
    scr.force = 1;
    for (;;) {
        uint32_t t = timer_get_ms();
        teclas();
        fm1_update_poll(paso_actualizacion);
        if ((int32_t)(t - next_adc) >= 0) {
            next_adc = t + 50u;
            d.master = (uint16_t)adc_get_value(FM1_ADC_MASTER);
            d.batt = (uint16_t)adc_get_value(FM1_ADC_BATT);
        }
        if ((int32_t)(t - next_ui) >= 0) {
            next_ui = t + 100u;
            bm_screen_render(&scr, dibujar, NULL, fm1_lcd_blit, NULL);
        }
        if (!d.ok_marcado && t - d.t0 > 10000u) {  /* 10 s andando: este arranque fue bueno */
            fm1_boot_ok();
            d.ok_marcado = 1;
        }
        os_time_dly(1);
    }
}

void app_main(void)
{
    os_task_create(diag_task, NULL, 10, 2048, 0, "diag");
}
