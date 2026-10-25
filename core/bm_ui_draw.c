/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pantallas (1.0, etapa 1): HOME (el mezclador y su PLAY/REC), su GLO, el secuenciador
 * de la batería, SAVE y SYSTEM. Franjas (2026-10-22: cada perilla se dibuja una sola vez):
 *   y 0..23     cabecera: la máquina, PLAY/REC, patrón, BPM, CPU (anillo de la pulsación de 2 s)
 *   y 24..135   visor: la pantalla de ahora (hasta 193 si la pantalla ya muestra K1-K4)
 *   y 136..193  perillas: K1-K4 con su dial, su valor y su nombre, en su orden físico (si el visor no las muestra)
 *   y 194..225  teclado: las 11 negras y las 16 blancas (= los 16 pasos) con lo que hace cada una
 *   y 226..239  SELECT, ALGORITHM y PRESETS: qué hacen aquí
 * Todo el texto en pantalla va en ASCII (la fuente no tiene acentos). */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "bm_app_int.h"
#include "bm_mach.h"
#include "bm_rom.h"
#include "bm_util.h"
#include "bm_drums.h"
#include "bm_params.h"
#include "bm_snap.h"
#include "bm_fm.h"
#include "bm_wt.h"
#include "bm_wave.h"
#include "bm_acid.h"

#define PY   36    /* primera fila útil de una pantalla de canal */

/* La miga del dibujo (0.7, temporal): qué parte de la pantalla se estaba dibujando. Si el
 * procesador falla, hal/fm1/sdk/wl82_debug.c la guarda en la huella y la consola la muestra.
 * 0 = fuera del dibujo. */
volatile uint8_t bm_ui_paso;
#define PASO(n) (bm_ui_paso = (uint8_t)(n))
#define VB   135   /* última fila del visor (con la franja de perillas) */
#define VT   193   /* última fila del visor sin la franja */

static uint16_t dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }
/* ------------------------------------------------------------ cabecera */
static void draw_ring(bm_gfx *g, int cx, int cy, int p, uint16_t c)
{
    static const int8_t CS[24][2] = {{0,-7},{2,-7},{4,-6},{5,-5},{6,-4},{7,-2},{7,0},{7,2},{6,4},{5,5},{4,6},{2,7},
                                     {0,7},{-2,7},{-4,6},{-5,5},{-6,4},{-7,2},{-7,0},{-7,-2},{-6,-4},{-5,-5},{-4,-6},{-2,-7}};
    int n = p * 24 / 255;
    for (int i = 0; i < 24; i++) gfx_fill(g, cx + CS[i][0], cy + CS[i][1], 2, 2, i < n ? c : C_DGREY);
}

static const char *machine_title(const bm_app *a, uint16_t *col)
{
    bool ch = a->view == V_CH || a->view == V_MACH || (a->sloop && a->view == V_HOME);
    *col = ch ? bm_track_color(a->track) : C_CYAN;
    if (ch) return bm_track_name(a, a->track);
    switch (a->view) {
    case V_SYSTEM: return "SYSTEM";
    case V_SAVE: return "SAVE";
    default: return "BX-1";
    }
}

/* "<": HOME vuelve al mezclador desde aquí: desde todas partes menos el mezclador (y su PLAY/REC, que también sale con HOME). */
static bool home_goes_back(const bm_app *a) { return a->view != V_HOME || a->sloop; }

/* adónde lleva mantener un botón 2 s (lo mismo que decide app_on_gesture) */
static const char *hold_target(const bm_app *a, int id)
{
    static char b[12];
    if (id == BTN_REC && (a->view == V_HOME || a->view == V_CH || a->view == V_MACH)) {   /* REC 2 s: borra el canal */
        snprintf(b, sizeof b, TF("CLEAR T%d"), app_pos(a->track) + 1);
        return b;
    }
    return id == BTN_HOME ? "SYSTEM" : id == BTN_SAVE ? "QUICK SAVE" : NULL;
}

/* el autoguardado no tapa la pantalla con un aviso: es una marca en la cabecera */
static bool toast_autosave(const bm_app *a) { return a->now < a->autosave_until; }

static void draw_status(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 0, BM_STATUS_H)) return;
    const bm_status *st = &a->ctx.st;
    char b[16];
    gfx_fill(g, 0, 0, BM_LCD_W, 23, C_BLACK);
    if (st->playing) for (int i = 0; i < 9; i++) gfx_vline(g, 4 + i, 7 + i / 2, 10 - (i / 2) * 2, C_GREEN);
    else gfx_fill(g, 4, 8, 8, 8, C_GREY);
    bool recording = a->rec;
    if (recording && ((a->now / 250) & 1)) gfx_disc(g, 18, 12, 3, C_RED);
    uint16_t mc;
    const char *mt = machine_title(a, &mc);
    int tx = 25;
    if (home_goes_back(a)) { gfx_text(g, 25, 9, "<", C_GREY, 1); tx = 32; }   /* HOME vuelve */
    gfx_text(g, tx, 5, mt, mc, 2);
    /* patrón, compás y lo que está en cola: "A01>A05" y cuatro pulsos que cuentan hasta el cambio */
    int x = tx + gfx_text_w(mt, 2) + 6;
    if (x < 102) x = 102;
    snprintf(b, sizeof b, "A%02d", st->pattern + 1);
    int qx = x + gfx_text(g, x, 3, b, C_WHITE, 1) + 1;
    bool queued = st->queued != 0xFF && st->queued != st->pattern;
    if (queued) {
        uint16_t qc = C_YELLOW;
        snprintf(b, sizeof b, ">A%02d", st->queued + 1);
        gfx_text(g, qx, 3, b, qc, 1);
        if (st->playing) {                                       /* pulsos que faltan para el cambio */
            uint32_t left = BM_BAR_TICKS - st->btick % BM_BAR_TICKS;    /* el patrón en cola entra al final del compás */
            int beats = (int)((left + BM_PPQN - 1) / BM_PPQN);
            for (int i = 0; i < 4; i++) gfx_fill(g, x + 20 + i * 5, 14, 4, 5, beats > 4 ? dim(qc) : (i < beats ? qc : C_DDGREY));
        }
    }
    uint32_t bt = st->playing ? st->btick : 0;
    snprintf(b, sizeof b, "%u.%u", (unsigned)(bt / BM_BAR_TICKS % 99 + 1), (unsigned)((bt / BM_PPQN) % 4 + 1));
    gfx_small(g, x, 14, b, C_GREY);
    if (!queued && st->scene_q != 0xFF) { snprintf(b, sizeof b, ">%c", 'A' + st->scene_q); gfx_small(g, x + 20, 14, b, C_CYAN); }
    else if (!queued && st->scene != 0xFF) { snprintf(b, sizeof b, "%c", 'A' + st->scene); gfx_small(g, x + 20, 14, b, C_CYAN); }
    /* los 6 canales: su color = suena, rojo = mute, blanco = golpe, borde = solo */
    for (int n = 0; n < BM_TRACKS; n++) {
        int t = BM_ORDER[n], cx = x + n * 7;
        bool mu = a->pat.trk[t].mute;
        gfx_fill(g, cx, 20, 5, 2, mu ? C_RED : (a->now < a->trig_until[t] ? C_WHITE : bm_track_color(t)));
        if (a->solo_on && a->solo_track == t) gfx_rect(g, cx - 1, 19, 7, 4, C_WHITE);
    }
    /* BPM */
    int bpm = a->pat.meta.bpm_x10;
    snprintf(b, sizeof b, "%d", bpm / 10);
    gfx_text_r(g, 186, 5, b, C_TEMPO, 2);
    if (bpm % 10) { snprintf(b, sizeof b, ".%d", bpm % 10); gfx_small(g, 187, 4, b, C_TEMPO); }
    gfx_small(g, 187, 14, a->set.clock ? (st->ext_ok ? "EXT" : "EXT?") : "BPM", a->set.clock && !st->ext_ok ? C_RED : C_GREY);
    /* la pulsación de 2 s: el anillo y adónde lleva (HOME: SYSTEM, SAVE: guardado rápido, una escena: borrarla) · si no, la CPU */
    int hid = -1, p = bm_gesture_progress(&a->ges, a->now, &hid);
    char kl[22];
    int kp = p < 0 ? bm_app_key_ring(a, kl, sizeof kl) : -1;
    const char *to = p >= 0 ? hold_target(a, hid) : kp >= 0 ? kl : NULL;
    if (p >= 0 && !to) p = -1;                                   /* un botón sin pulsación de 2 s: nada */
    if (p >= 0 || kp >= 0) {
        draw_ring(g, 210, 12, p >= 0 ? p : kp, C_YELLOW);
        int w = gfx_text_w(to, 1), x0 = 186 - gfx_text_w("888", 2);   /* tapa el BPM mientras dura */
        if (198 - w - 3 < x0) x0 = 198 - w - 3;
        gfx_fill(g, x0, 2, 201 - x0, 20, C_BLACK);
        gfx_text(g, 198 - w - 1, 9, to, C_YELLOW, 1);
    } else if (a->cpu > 95) gfx_small(g, 202, 14, "CPU!", C_RED);   /* solo si el audio no alcanza */
    if (toast_autosave(a)) gfx_disc(g, 223, 19, 2, C_GREEN);    /* autoguardado: una marca, sin tapar nada */
    /* batería, vertical */
    int bp = a->battery;
    uint16_t bc = bp > 30 ? C_GREEN : (bp > 15 ? C_YELLOW : C_RED);
    gfx_rect(g, 229, 5, 8, 15, C_DGREY);
    gfx_fill(g, 231, 3, 4, 2, C_DGREY);
    int bh = bp * 11 / 100;
    gfx_fill(g, 231, 18 - bh, 4, bh, bc);
    gfx_hline(g, 0, 23, BM_LCD_W, C_DDGREY);
}

/* ------------------------------------------------------------ perillas: cuatro diales */
/* 33 puntos de un arco de 270° (de abajo a la izquierda a abajo a la derecha), radio 13 */
static const int8_t ARC[33][2] = {
    {-9,9},{-10,8},{-11,6},{-12,5},{-13,3},{-13,1},{-13,-1},{-13,-3},{-12,-5},{-11,-6},{-10,-8},{-9,-9},{-7,-11},
    {-5,-12},{-3,-13},{-1,-13},{0,-13},{1,-13},{3,-13},{5,-12},{7,-11},{9,-9},{10,-8},{11,-6},{12,-5},{13,-3},
    {13,-1},{13,1},{13,3},{12,5},{11,6},{10,8},{9,9}};

static void draw_dial(bm_gfx *g, int cx, int cy, int pct, int ghost, uint16_t col, bool hi)
{
    int n = pct * 33 / 255;
    for (int i = 0; i < 33; i++) gfx_fill(g, cx + ARC[i][0] - 1, cy + ARC[i][1] - 1, 3, 3, i < n ? col : C_DDGREY);
    if (ghost >= 0) { int k = bm_clampi(ghost * 32 / 255, 0, 32); gfx_fill(g, cx + ARC[k][0] - 1, cy + ARC[k][1] - 1, 3, 3, C_GREY); }
    /* la aguja, como en un pixel-art: del centro al valor */
    int k = bm_clampi(n ? n - 1 : 0, 0, 32);
    gfx_line(g, cx, cy, cx + ARC[k][0] * 2 / 3, cy + ARC[k][1] * 2 / 3, hi ? C_WHITE : col);
    gfx_fill(g, cx - 1, cy - 1, 3, 3, hi ? C_WHITE : C_GREY);
}

static void sys_val(const bm_app *a, int i, char *v, int cap);

static void draw_legend(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_LEGEND_Y, BM_LEGEND_H)) return;
    static bm_legend L[4];                                       /* una vez por cuadro, no por franja */
    static uint32_t lf;
    if (lf != g->frame) {
        lf = g->frame;
        PASO(30);
        bm_app_legend(a, L);
        PASO(29);
        /* SYSTEM: K1 muestra el valor de la fila, no el nombre de otra perilla */
        if (a->view == V_SYSTEM && !a->layer && L[0].name[0]) sys_val(a, BM_SYS_ROWS[a->menu_row % BM_SYS_VIS], L[0].val, sizeof L[0].val);
    }
    gfx_fill(g, 0, BM_LEGEND_Y, BM_LCD_W, BM_LEGEND_H, C_BLACK);
    gfx_hline(g, 0, BM_LEGEND_Y, BM_LCD_W, C_DDGREY);
    for (int i = 0; i < 4; i++) {
        int x = i * 60, cx = x + 30;
        uint16_t kc = bm_knob_color(i);
        bool hi = a->ov.on && a->ov.enc == i;
        if (i) gfx_vline(g, x, BM_LEGEND_Y + 4, 50, C_DDGREY);
        if (!L[i].name[0]) { draw_dial(g, cx, BM_LEGEND_Y + 17, 0, -1, C_DDGREY, false); continue; }
        if (hi) gfx_rect(g, x + 2, BM_LEGEND_Y + 2, 56, 55, kc);
        draw_dial(g, cx, BM_LEGEND_Y + 17, L[i].pct, -1, kc, hi);
        gfx_text_c(g, cx, BM_LEGEND_Y + 33, L[i].val, L[i].locked ? kc : C_WHITE, 1);
        if (L[i].locked) gfx_fill(g, x + 53, BM_LEGEND_Y + 34, 4, 4, kc);
        char nm[11]; snprintf(nm, sizeof nm, "%.9s", T(L[i].name));
        gfx_text_c(g, cx, BM_LEGEND_Y + 45, nm, kc, 1);
    }
}

/* Las tres perillas grandes, en su orden del panel (SELECT, ALGORITHM, PRESETS): qué hacen aquí,
 * o "-" si aquí no hacen nada. La fila de abajo, con letra grande (2026-10-22: antes casi no se
 * leía): el nombre de la perilla en una etiqueta y lo que hace al lado. */
static void draw_bigknobs(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_FOOT_Y, BM_FOOT_H)) return;
    const char *v[3];
    static const char *const N[3] = {"SEL", "ALG", "PRE"};
    bm_app_bigknobs(a, &v[0], &v[1], &v[2]);
    gfx_fill(g, 0, BM_FOOT_Y, BM_LCD_W, BM_FOOT_H, C_BLACK);
    for (int i = 0; i < 3; i++) {
        int x = 1 + i * 80, y = BM_FOOT_Y + 3;
        bool dead = !v[i][0] || (v[i][0] == '-' && !v[i][1]);
        bool hi = a->ov.on && a->ov.knob == (i == 0 ? ENC_SELECT : i == 1 ? ENC_ALGO : ENC_PRESETS) + 1;
        int w = gfx_small_w(N[i]) + 4;
        gfx_fill(g, x, y - 1, w, 9, dead ? C_DDGREY : C_DGREY);
        gfx_small(g, x + 2, y + 1, N[i], dead ? C_DGREY : C_WHITE);
        char b[12];
        snprintf(b, sizeof b, "%.10s", dead ? "-" : T(v[i]));
        if (gfx_text_w(b, 1) > 78 - w - 3) b[(78 - w - 3) / 6] = 0;        /* que no pise a la siguiente */
        gfx_text(g, x + w + 3, y, b, hi ? a->ov.color : dead ? C_DGREY : C_WHITE, 1);
    }
}

/* ------------------------------------------------------------ helpers */
static void header(bm_gfx *g, const char *title, const char *sub, uint16_t col)
{
    gfx_text(g, 6, 28, title, col, 2);
    if (sub) gfx_text_r(g, 234, 32, sub, C_GREY, 1);
    gfx_hline(g, 4, 44, 232, C_DDGREY);
}

static void row(bm_gfx *g, int y, bool sel, const char *l, const char *r)
{
    if (!gfx_strip_hit(g, y, 12)) return;
    if (sel) { gfx_fill(g, 0, y, BM_LCD_W, 11, C_DDGREY); gfx_fill(g, 0, y, 2, 11, C_CYAN); }
    gfx_text(g, 6, y + 2, l, sel ? C_WHITE : C_GREY, 1);
    if (r) gfx_text_r(g, 234, y + 2, r, sel ? C_CYAN : C_WHITE, 1);
}

/* la rejilla sutil del fondo (#161925) */
static void grid(bm_gfx *g, int x0, int y0, int w, int h, int step)
{
    for (int y = y0; y < y0 + h; y += step)
        if (gfx_strip_hit(g, y, 1)) for (int x = x0; x < x0 + w; x += step) gfx_fill(g, x, y, 1, 1, C_DDGREY);
}

/* ¿el canal tiene golpe/nota en el paso absoluto s? (0 nada, 1 golpe o comienzo, 2 nota que sigue) */
static int track_has(const bm_app *a, int t, int s)
{
    if (bm_is_grid(&a->pat, t)) {
        const bm_tr *tr = bm_trk_trc(&a->pat, t);
        for (int l = 0; l < BM_LANES; l++) {
            int len = tr->lane[l].len ? tr->lane[l].len : 16;
            if (tr->st[l][s % len] & ST_ON) return 1;
        }
        return 0;
    }
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16, r = 0;
    for (int back = 0; back < 64 && back < len; back++) {
        int ss = ((s - back) % len + len) % len;
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, ss, k);
            if (bm_event_empty(e)) continue;
            int l = (e->len & BM_LEN_MASK) + ((e->len & BM_LEN_TIE) ? 1 : 0);
            if (back == 0) return 1;
            if (back < l) r = 2;
        }
        if (r) return r;
    }
    return 0;
}

/* ------------------------------------------------------------ teclado (las 11 negras y las 16 blancas)
 * Las luces de las teclas son blancas: dicen si una tecla está activa, no qué hace. La franja dibuja
 * cada tecla con su nombre corto y el mismo estado que su luz: relleno = encendida, borde claro y
 * nombre blanco = media, borde oscuro y nombre gris = tenue, nombre gris oscuro = apagada. El color
 * de la función va como un subrayado. Las blancas son los 16 pasos. */
static void key_cell(bm_gfx *g, const bm_app *a, int x, int y, int w, int h, const bm_keyfn *f, int down, int big)
{
    uint8_t st = down ? LED_ON : f->led;
    if (st == LED_BLINK) st = ((a->now / 250u) & 1u) ? LED_ON : LED_MID;
    uint16_t bg = 0, fg = C_DGREY, edge = 0;               /* apagada: el nombre en gris oscuro */
    switch (st) {
    case LED_ON: bg = C_WHITE; fg = C_BLACK; break;
    case LED_BREATH: bg = C_GREY; fg = C_BLACK; break;
    case LED_MID: edge = C_GREY; fg = C_WHITE; break;
    case LED_DIM: edge = C_DGREY; fg = C_GREY; break;
    default: break;
    }
    if (bg) gfx_fill(g, x, y, w, h, bg);
    else if (edge) gfx_rect(g, x, y, w, h, edge);
    if (!f->label[0]) return;
    /* el color de la función (no el de la luz: las luces son blancas), como un subrayado */
    if (f->color && f->color != C_WHITE) gfx_fill(g, x + 2, y + h - 3, w - 4, 2, st == LED_OFF ? dim(f->color) : f->color);
    char l[6];
    int maxc = big ? (w - 2) / 6 : (w - 1) / 4;
    snprintf(l, sizeof l, "%.*s", maxc < 1 ? 1 : maxc > 5 ? 5 : maxc, T(f->label));   /* traducida y después recortada */
    if (big) gfx_text_c(g, x + w / 2, y + (h - 7) / 2, l, fg, 1);
    else gfx_small(g, x + (w - gfx_small_w(l)) / 2 + 1, y + (h - 5) / 2, l, fg);
}

static void draw_keys(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_KEYS_Y, BM_KEYS_H)) return;
    static bm_keyfn K[BM_KEYS];                                  /* una vez por cuadro, no por franja */
    static uint32_t kf;
    if (kf != g->frame) {
        kf = g->frame;
        PASO(31);
        bm_app_keys(a, K);
        PASO(28);
    }
    gfx_fill(g, 0, BM_KEYS_Y, BM_LCD_W, BM_KEYS_H, C_BLACK);
    gfx_hline(g, 0, BM_KEYS_Y, BM_LCD_W, C_DDGREY);
    int wy = BM_KEYS_Y + 15;
    for (int i = 0; i < BM_WHITES; i++) {
        int k = bm_white_key(i);
        key_cell(g, a, i * 15, wy, 14, 15, &K[k], a->keys_down[k], 0);
    }
    for (int i = 0; i < BM_BLACKS; i++) {
        int k = bm_black_key(i), wi = bm_key_white_index(k - 1);
        key_cell(g, a, (wi + 1) * 15 - 8, BM_KEYS_Y + 2, 16, 12, &K[k], a->keys_down[k], 0);
    }
}

static void lane_pads(bm_gfx *g, const bm_app *a, int t, int y0, int rowh)
{
    for (int i = 0; i < 16; i++) {
        int x = (i % 4) * 59 + 2, y = y0 + (i / 4) * rowh;
        if (!gfx_strip_hit(g, y, rowh - 2)) continue;
        bool muted = (bm_trk_trc(&a->pat, t)->mute >> i) & 1;
        bool sel = i == a->sel_lane[t];
        bool fl = a->now < a->lane_flash[t][i];
        uint16_t bg = fl ? bm_track_color(t) : (sel ? dim(bm_track_color(t)) : C_DDGREY);
        gfx_fill(g, x, y, 56, rowh - 2, bg);
        uint16_t fg = fl ? C_BLACK : (muted ? C_GREY : C_WHITE);
        char b[12];
        if (rowh < 20) {                                         /* compacta (capas): número y nombre en una línea */
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_small(g, x + 3, y + (rowh - 7) / 2 + 1, b, fl ? C_BLACK : C_GREY);
            snprintf(b, sizeof b, "%.5s", T(bm_lane_name(a, t, i)));
            gfx_text(g, x + 12, y + (rowh - 9) / 2, b, fg, 1);
        } else {
            gfx_text(g, x + 3, y + 3, bm_lane_name(a, t, i), fg, 1);
            snprintf(b, sizeof b, "%d %s", i + 1, T(bm_drm_short[bm_trk_trc(&a->pat, t)->lane[i].model % DRM_COUNT]));
            gfx_small(g, x + 3, y + rowh - 9, b, fg);
        }
        if (muted) gfx_text(g, x + 48, y + (rowh < 20 ? (rowh - 9) / 2 : 3), "M", C_RED, 1);
    }
}

/* ------------------------------------------------------------ HOME: BX-1, el mezclador */
static void draw_sloop(bm_gfx *g, const bm_app *a);

/* Una fila por canal: la máquina, su secuencia (pulsos para los golpes, bloques para las notas
 * largas), el filtro y su ganancia. SELECT (o la blanca de su canal) mueve el cursor; la blanca del canal
 * del cursor entra a PLAY/REC y SEQ abre el secuenciador de la batería. En PLAY/REC, el canal del cursor
 * en PIANO ROLL. */
static void draw_home(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[16];
    if (a->sloop) { draw_sloop(g, a); return; }
    if (gfx_strip_hit(g, 25, 10)) {
        gfx_fill(g, 2, 25, gfx_small_w("MIXER") + 4, 9, C_CYAN);
        gfx_small(g, 4, 27, "MIXER", C_BLACK);
    }
    for (int n = 0; n < BM_TRACKS; n++) {
        int t = BM_ORDER[n], y = 33 + n * 16;
        if (!gfx_strip_hit(g, y, 16)) continue;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        bool sel = t == a->track;
        if (sel) { gfx_fill(g, 0, y, BM_LCD_W, 15, C_DDGREY); gfx_fill(g, 0, y, 2, 15, C_CYAN); }
        snprintf(b, sizeof b, "%d", n + 1);
        gfx_text(g, 5, y + 4, b, sel ? C_CYAN : C_GREY, 1);
        gfx_text(g, 15, y + 4, bm_track_name(a, t), tr->mute ? C_GREY : col, 1);
        uint32_t n = st->tn[t];
        int cur = st->playing ? (int)(n % 16) : -1;
        int base = st->playing ? (int)((n / 16) * 16) : 0;
        gfx_rect(g, 90, y + 2, 84, 11, C_DDGREY);
        for (int s = 0; s < 16; s++) {
            int h = track_has(a, t, base + s), x = 92 + s * 5;
            if (h == 1) gfx_fill(g, x, y + 4, bm_is_grid(&a->pat, t) ? 3 : 5, 7, col);    /* golpe o comienzo */
            else if (h == 2) gfx_fill(g, x, y + 5, 5, 5, tr->mute ? C_DGREY : C_GREEN);   /* nota que sigue */
            else gfx_fill(g, x + 1, y + 7, 1, 1, C_DGREY);
            if (s == cur) gfx_fill(g, x, y + 11, 4, 1, C_WHITE);
        }
        if (tr->djf && tr->djf != 64) gfx_small(g, 180, y + 5, tr->djf < 64 ? "LP" : "HP", tr->mute ? C_DGREY : C_ORANGE);
        int vh = tr->level * 26 / 127, pk = st->peak[t] * 26 / 255;
        gfx_fill(g, 200, y + 4, 26, 3, C_DDGREY); gfx_fill(g, 200, y + 4, vh, 3, col);
        gfx_fill(g, 200, y + 9, 26, 2, C_DDGREY); gfx_fill(g, 200, y + 9, pk, 2, C_GREEN);
        if (tr->mute) gfx_text(g, 230, y + 4, "M", C_RED, 1);
        else if (a->now < a->trig_until[t]) gfx_disc(g, 233, y + 7, 3, col);
    }
}

/* ------------------------------------------------------------ SEQ: rejilla TR */
/* OCT -/+ cambian la página de sonidos de la batería: la página va como una tecla (borde y nombre)
 * para que se vea que la cambian ellos. Devuelve el ancho. */
static int page_tag(bm_gfx *g, const bm_app *a, int xr, int y, int h, uint16_t c)
{
    const char *s = (a->sel_lane[a->track] & 15) >= 8 ? "OCT 9-16" : "OCT 1-8";
    int w = gfx_small_w(s) + 6;
    gfx_rect(g, xr - w, y, w, h, c);
    gfx_small(g, xr - w + 3, y + (h - 5) / 2, s, c);
    return w;
}

/* Batería: 8 instrumentos por página de sonidos (1-8 o 9-16; OCT -/+ cambia) y los 16 pasos del compás
 * que ves (las negras 9 y 10 cambian el compás). */
static void draw_seq_tr(bm_gfx *g, const bm_app *a)
{
    int t = a->track, sel = a->sel_lane[t], base = (sel / 8) * 8;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint16_t col = bm_track_color(t);
    char b[30];
    if (gfx_strip_hit(g, PY, 9)) {
        if (a->held_step >= 0 && a->held_long) {                 /* paso sostenido: cómo está */
            uint8_t v = tr->st[sel][a->held_step];
            int rat = (v & ST_FLAM) ? 4 : ((v & ST_RAT_MASK) >> ST_RAT_SH);
            static const char *const R[5] = {"X1", "X2", "X3", "X4", "FLAM"};
            static const char *const C[3] = {"ALWAYS", "FILL", "NO FILL"};
            snprintf(b, sizeof b, "%02d %s %s %s", a->held_step + 1, T((v & ST_ACC) ? "ACCENT" : "NORMAL"), R[rat], T(C[(v & ST_COND_MASK) >> ST_COND_SH]));
            gfx_fill(g, 0, PY - 1, 240, 9, C_DGREY);
            gfx_text(g, 4, PY, b, C_YELLOW, 1);
        } else {
            gfx_text(g, 4, PY, bm_lane_name(a, t, sel), C_WHITE, 1);
            int len = tr->lane[sel].len;
            snprintf(b, sizeof b, TF("BAR %d/%d  %d STEPS"), a->step_page + 1, (len + 15) / 16, len);
            page_tag(g, a, 236 - gfx_text_r(g, 236, PY, b, C_GREY, 1) - 5, PY - 1, 9, C_WHITE);
        }
    }
    for (int i = 0; i < 8; i++) {
        int l = base + i, y = PY + 10 + i * 11;
        if (!gfx_strip_hit(g, y, 11)) continue;
        bool muted = (tr->mute >> l) & 1;
        int len = tr->lane[l].len ? tr->lane[l].len : 16;
        int ps = bm_app_track_step(a, t, l);
        if (l == sel) gfx_fill(g, 0, y, 240, 10, C_DDGREY);
        char nm[5]; snprintf(nm, sizeof nm, "%s", T(bm_lane_name(a, t, l)));
        gfx_small(g, 1, y + 3, nm, a->now < a->lane_flash[t][l] ? col : (muted ? C_RED : (l == sel ? C_WHITE : C_GREY)));
        for (int s2 = 0; s2 < 16; s2++) {
            int ab = a->step_page * 16 + s2, x = 24 + s2 * 13 + (s2 / 4);
            if (ab >= len) { gfx_fill(g, x + 5, y + 4, 2, 2, C_DDGREY); continue; }
            uint8_t v = tr->st[l][ab];
            uint16_t c = muted ? C_DGREY : (l == sel ? col : dim(col));
            if (v & ST_ON) {
                int cond = (v & ST_COND_MASK) >> ST_COND_SH;
                if (cond) gfx_rect(g, x, y + 1, 12, 9, cond == 1 ? C_ORANGE : C_GREY);
                else gfx_fill(g, x, y + 1, 12, 9, (v & ST_ACC) ? C_WHITE : c);
                if (cond && (v & ST_ACC)) gfx_fill(g, x + 2, y + 3, 8, 5, C_WHITE);
                int rat = (v & ST_RAT_MASK) >> ST_RAT_SH;
                for (int r = 0; r < rat; r++) gfx_fill(g, x + 2 + r * 3, y + 7, 2, 2, C_BLACK);
                if (v & ST_FLAM) gfx_fill(g, x + 9, y + 2, 2, 2, C_BLACK);
                if (v & ST_LOCK) gfx_fill(g, x + 1, y + 2, 2, 2, C_BLACK);
            } else {
                gfx_fill(g, x, y + 1, 12, 9, l == sel ? C_DGREY : C_BLACK);
                if (s2 % 4 == 0) gfx_fill(g, x + 5, y + 5, 2, 2, C_DGREY);
            }
            if (ab == ps) gfx_fill(g, x, y + 9, 12, 2, (v & ST_ON) && !muted ? C_ORANGE : C_GREY);
            if (l == sel && ab == a->held_step && a->held_long) gfx_rect(g, x - 1, y, 14, 11, C_YELLOW);
        }
    }
}

/* ------------------------------------------------------------ PIANO ROLL (0.9)
 * El loop entero del canal (1-4 compases) con las filas de alturas ajustadas a lo que hay (hasta 3
 * octavas); a la izquierda un teclado vertical que destella con lo que suena. En la batería y los pads,
 * una fila por instrumento con golpes. Las notas grabadas en esta pasada, en rojo mientras REC está
 * puesto. win: el corchete de las 16 blancas (SEQ). El reparto de filas se calcula al cambiar el
 * patrón (o al editarlo) y cada franja dibuja solo lo que la toca. */
static struct {
    uint32_t t_edit;
    uint8_t ok, trk, pat, eng, len, sel, lo, hi, n;
    uint8_t lane[BM_LANES];                       /* rejillas: los instrumentos que tienen fila */
} rl;

static void roll_layout(const bm_app *a, int t)
{
    bool grid = bm_is_grid(&a->pat, t);
    uint8_t len = (uint8_t)(grid ? 0 : bm_trk_len(&a->pat, t, -1)), sel = (uint8_t)(a->sel_lane[t] & 15);
    bool fresh = rl.ok && rl.t_edit == a->t_edit && rl.trk == t && rl.pat == a->ctx.st.pattern && rl.eng == a->pat.trk[t].engine
                 && rl.sel == sel && (grid || rl.len == len) && a->now - a->t_edit > 150;   /* recién editado: el audio quizá no lo aplicó */
    if (fresh) return;
    rl.ok = 1; rl.t_edit = a->t_edit; rl.trk = (uint8_t)t; rl.pat = a->ctx.st.pattern; rl.eng = a->pat.trk[t].engine; rl.sel = sel; rl.n = 0;
    if (grid) {
        const bm_tr *tr = bm_trk_trc(&a->pat, t);
        int maxl = 1;
        for (int l = 0; l < BM_LANES; l++) {
            int ll = tr->lane[l].len ? tr->lane[l].len : 16, any = l == sel;
            for (int s = 0; s < ll && !any; s++) any = (tr->st[l][s] & ST_ON) != 0;
            if (!any) continue;
            rl.lane[rl.n++] = (uint8_t)l;
            if (ll > maxl) maxl = ll;
        }
        rl.len = (uint8_t)maxl;
        return;
    }
    rl.len = len;
    int lo = 127, hi = 0;
    for (int s = 0; s < len; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, s, k);
            if (bm_event_empty(e)) continue;
            if (e->note < lo) lo = e->note;
            if (e->note > hi) hi = e->note;
        }
    if (lo > hi) { lo = a->last_note[t] - 6; hi = a->last_note[t] + 6; }
    while (hi - lo < 11) { hi++; if (hi - lo < 11) lo--; }       /* una octava como mínimo */
    if (hi - lo > 35) lo = hi - 35;                              /* tres como mucho */
    rl.lo = (uint8_t)bm_clampi(lo, 0, 127); rl.hi = (uint8_t)bm_clampi(hi, 0, 127);
}

/* ¿esta altura suena ahora en el canal (desde sus pasos o una tecla)? */
static bool roll_sounding(const bm_app *a, int t, int note)
{
    if (t >= BM_TR_TRACKS)
        for (int k = 0; k < BM_EVENTS; k++) if (a->ctx.gate[t - BM_TR_TRACKS][k].ticks && a->ctx.gate[t - BM_TR_TRACKS][k].note == note) return true;
    for (int k = 0; k < BM_KEYS; k++)
        if (a->keys_down[k] && a->key_trk[k] == t)
            for (int i = 0; i < a->key_nn[k]; i++) if (a->key_notes[k][i] == note) return true;
    return false;
}

/* page >= 0: solo esa página de 16 pasos (las máquinas: cada 16 pasos, una página); -1: el loop entero */
static void draw_seq_roll(bm_gfx *g, const bm_app *a, int t, int y0, int y1, bool win, int page)
{
    if (!gfx_strip_hit(g, y0 - 2, y1 - y0 + 2)) return;          /* el corchete va 2 filas arriba */
    static uint32_t lf;                                          /* el reparto, una vez por cuadro */
    if (lf != g->frame) { lf = g->frame; roll_layout(a, t); }
    bool grid = bm_is_grid(&a->pat, t);
    uint16_t col = bm_track_color(t);
    int lo = rl.lo, hi = rl.hi;                                  /* las máquinas: K4 corre las alturas */
    if (page >= 0 && !grid && a->bx.vy) {
        int sh = bm_clampi(a->bx.vy, -lo, 127 - hi);
        lo += sh; hi += sh;
    }
    int R = grid ? rl.n : hi - lo + 1, L = rl.len ? rl.len : 16, H = y1 - y0;
    if (R < 1) R = 1;
    int rh = bm_clampi(H / R, 2, 12), top = y0 + (H - R * rh) / 2, x0 = 15, W = 239 - x0;
    int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int ps = bm_app_track_step(a, t, grid ? (rl.n ? rl.lane[0] : 0) : -1);
    bool red = a->rec && a->rec_steps;
    int v0 = 0, VL = L;                                          /* los pasos que se ven */
    if (page >= 0) { v0 = page * 16; VL = 16; win = false; }
    int v1 = v0 + VL < L ? v0 + VL : L;
#define RX(s) (x0 + ((s) - v0) * W / VL)
    /* el fondo: los 16 pasos de las blancas en gris (con su corchete), compases y tiempos */
    if (win) {
        int p0 = a->step_page * 16, p1 = p0 + 16 > L ? L : p0 + 16;
        gfx_fill(g, RX(p0), top, RX(p1) - RX(p0), R * rh, C_DDGREY);
        gfx_hline(g, RX(p0), top - 2, RX(p1) - RX(p0) + 1, C_WHITE);
        gfx_hline(g, RX(p0), top + R * rh + 1, RX(p1) - RX(p0) + 1, C_WHITE);
        gfx_vline(g, RX(p0), top - 2, 2, C_WHITE); gfx_vline(g, RX(p1), top - 2, 2, C_WHITE);
    }
    if (v1 < v0 + VL) gfx_fill(g, RX(v1), top, RX(v0 + VL) - RX(v1), R * rh, C_DDGREY);   /* después del largo */
    for (int s = v0; s <= v1; s++) {
        uint16_t c = s % spb == 0 ? C_DGREY : s % 4 == 0 ? C_DDGREY : 0;
        if (c) gfx_vline(g, RX(s), top, R * rh, c);
    }
    if (ps >= v0 && ps < v1) gfx_vline(g, RX(ps), top, R * rh, C_ORANGE);   /* el cabezal */
    for (int r = 0; r < R; r++) {
        int y = top + r * rh;
        if (!gfx_strip_hit(g, y, rh)) continue;
        if (grid) {                                              /* el teclado: el nombre del instrumento, destella al sonar */
            int l = rl.lane[r];
            bool fl = a->now < a->lane_flash[t][l], mu = (bm_trk_trc(&a->pat, t)->mute >> l) & 1;
            if (fl) gfx_fill(g, 0, y, 14, rh - 1, col);
            if (rh >= 6) {
                char nm[5]; snprintf(nm, sizeof nm, "%.3s", T(bm_lane_name(a, t, l)));
                gfx_small(g, 1, y + (rh - 5) / 2, nm, fl ? C_BLACK : mu ? C_RED : l == rl.sel ? C_WHITE : C_GREY);
            }
            const bm_tr *tr = bm_trk_trc(&a->pat, t);
            int ll = tr->lane[l].len ? tr->lane[l].len : 16;
            for (int s = v0; s < v1; s++) {
                uint8_t v = tr->st[l][s % ll];
                if (!(v & ST_ON)) continue;
                bool mine = red && ((a->rec_steps >> (s % 64)) & 1) && ((a->rec_lanes >> l) & 1);
                uint16_t c = mine ? C_RED : mu ? C_DGREY : (v & ST_ACC) ? C_WHITE : s >= ll ? dim(col) : col;
                int xa = RX(s) + 1, xb = RX(s + 1) - 1;
                gfx_fill(g, xa, y + 1, xb > xa ? xb - xa : 1, rh > 3 ? rh - 2 : rh - 1, c);
            }
            continue;
        }
        int note = hi - r;                                       /* el teclado vertical */
        int pc = note % 12;
        bool blk = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10, on = roll_sounding(a, t, note);
        gfx_fill(g, 0, y, blk ? 9 : 14, rh - (rh > 2), on ? col : blk ? C_DGREY : C_GREY);
        if (pc == 0 && rh >= 5) gfx_hline(g, x0, y + rh - 1, W, C_DDGREY);   /* cada Do */
    }
    if (!grid) {                                                 /* las notas */
        for (int s = 0; s < L; s++)
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = bm_evc(&a->pat, t, s, k);
                if (bm_event_empty(e) || e->note > hi || e->note < lo) continue;
                int y = top + (hi - e->note) * rh;
                if (!gfx_strip_hit(g, y, rh)) continue;
                int l = e->len & BM_LEN_MASK;
                if (l < 1) l = 1;
                if (s >= v0 + VL || s + l <= v0) continue;       /* fuera de la página */
                int xa = RX(s) + e->micro * (W / L) / 24, xb = RX(s + l) - 1;
                if (e->len & BM_LEN_TIE) xb += 2;
                else if (e->gate) xb = RX(s + l - 1) + (RX(s + l) - RX(s + l - 1)) * e->gate / 100 - 1;
                if (xb > 239) xb = 239;
                if (xa < x0) xa = x0;
                bool mine = red && ((a->rec_steps >> s) & 1);
                uint16_t c = mine ? C_RED : e->vel >= BM_VEL_ACC ? C_WHITE : ((e->len & BM_LEN_TIE) || l > 1) ? C_GREEN : col;
                if (e->vel <= BM_VEL_GHOST) gfx_rect(g, xa, y, xb > xa ? xb - xa : 1, rh, c);
                else gfx_fill(g, xa, y, xb > xa ? xb - xa : 1, rh - (rh > 3), c);
            }
        if (page < 0 && a->held_step >= 0 && a->held_long && a->held_step < L) gfx_rect(g, RX(a->held_step) - 1, top, RX(a->held_step + 1) - RX(a->held_step) + 2, R * rh, C_YELLOW);
    }
    if (page >= 0 && mach_step_mode(a)) {                        /* paso a paso: el paso que se escribe */
        int cs = a->bx.step % L;
        if (cs >= v0 && cs < v1) {
            gfx_rect(g, RX(cs), top - 2, RX(cs + 1) - RX(cs) + 1, R * rh + 4, C_YELLOW);
            gfx_rect(g, RX(cs) + 1, top - 1, RX(cs + 1) - RX(cs) - 1, R * rh + 2, C_YELLOW);
        }
    }
#undef RX
}

/* V_MACH SEQ: la página de 16 pasos que se ve: la del cabezal sonando, la del cursor parado */
static int mach_roll_page(const bm_app *a, int t) { (void)t; return mach_view_page(a); }

/* PLAY/REC: el canal del cursor en PIANO ROLL; el teclado lo toca (y, con REC, lo sobregraba) */
static void draw_sloop(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    if (gfx_strip_hit(g, 25, 10)) {
        char b[32];
        gfx_fill(g, 0, 25, BM_LCD_W, 10, a->rec ? C_RED : C_DGREY);
        snprintf(b, sizeof b, TF("%s T%d %s"), a->rec ? "REC" : "PLAY", app_pos(t) + 1, bm_track_name(a, t));
        gfx_text(g, 4, 26, b, C_BLACK, 1);
        if (app_kind(a, t) == KIND_NOTE) snprintf(b, sizeof b, a->oct > 0 ? "OCT +%d" : "OCT %d", a->oct);   /* el snprintf del SDK no tiene %+d */
        else if (bm_is_tr(&a->pat, t))                       /* batería: OCT sostenidos = golpes suaves / fuertes */
            snprintf(b, sizeof b, "%s", T(bm_gesture_is_down(&a->ges, BTN_OCT_UP) ? "HARD" : bm_gesture_is_down(&a->ges, BTN_OCT_DN) ? "SOFT" : "OCT- SOFT  OCT+ HARD"));
        else b[0] = 0;
        gfx_text_r(g, 236, 26, b, C_BLACK, 1);
    }
    draw_seq_roll(g, a, t, 39, VB, false, -1);
}

/* GLO del mezclador > SELECT MACHINE: T1 y T2 fijos; en T3-T6, ALGORITHM elige (2 s en una la carga) */
static void mach_list(bm_gfx *g, const bm_app *a, int y0, int rh);

static void draw_glo(bm_gfx *g, const bm_app *a)
{
    char b[30];
    static const char *const PG[BM_GLO_PAGES] = {"SELECT MACHINE", "TEMPO AND SCALE", "MASTER FX"};
    if (gfx_strip_hit(g, 25, 10)) {
        gfx_fill(g, 2, 25, gfx_small_w("GLO") + 4, 9, C_CYAN);
        gfx_small(g, 4, 27, "GLO", C_BLACK);
        snprintf(b, sizeof b, "%s %d/%d", T(PG[a->glo_page % BM_GLO_PAGES]), a->glo_page % BM_GLO_PAGES + 1, BM_GLO_PAGES);
        gfx_small(g, 236 - gfx_small_w(b), 27, b, C_WHITE);
    }
    if (a->glo_page) {                                            /* TEMPO / MASTER FX: las cuatro perillas en lista */
        mach_list(g, a, 40, 38);
        return;
    }
    for (int n = 0; n < BM_TRACKS; n++) {
        int t = BM_ORDER[n], y = PY + 2 + n * 13;
        if (!gfx_strip_hit(g, y, 13)) continue;
        bool sel = t == a->track, fixed = app_fixed(t);
        uint16_t col = bm_track_color(t);
        if (sel) { gfx_fill(g, 0, y, BM_LCD_W, 12, C_DDGREY); gfx_fill(g, 0, y, 2, 12, C_CYAN); }
        snprintf(b, sizeof b, "T%d", n + 1);
        gfx_text(g, 6, y + 2, b, fixed ? C_GREY : col, 1);
        int e = sel && a->mach_trk == t ? a->mach_sel : a->pat.trk[t].engine;
        int other = fixed ? -1 : bm_machine_conflict(&a->pat, t, e);
        bool pending = sel && a->mach_trk == t && e != a->pat.trk[t].engine;
        snprintf(b, sizeof b, pending ? "< %s >" : "%s", bm_engine_names[e % ENG_COUNT]);
        gfx_text(g, 28, y + 2, b, fixed ? C_GREY : other >= 0 ? C_DGREY : sel ? C_WHITE : C_GREY, 1);
        if (fixed) gfx_small(g, 236 - gfx_small_w("FIXED"), y + 3, "FIXED", C_DGREY);
        else if (other >= 0) { snprintf(b, sizeof b, TF("ON T%d"), app_pos(other) + 1); gfx_small(g, 236 - gfx_small_w(b), y + 3, b, C_GREY); }
        else if (pending && a->mach_arm == 1) {
            uint32_t dt = a->now - a->mach_t0;
            unsigned left = dt >= BM_MACH_MS ? 0 : (unsigned)(BM_MACH_MS - dt + 99) / 100;
            snprintf(b, sizeof b, TF("LOAD IN %u.%u S"), left / 10, left % 10);
            gfx_small(g, 236 - gfx_small_w(b), y + 3, b, C_YELLOW);
            gfx_fill(g, 2, y + 11, (int)(dt >= BM_MACH_MS ? 236 : dt * 236u / BM_MACH_MS), 1, C_WHITE);
        } else if (!pending) gfx_small(g, 236 - gfx_small_w(bm_machine_origin(e)), y + 3, bm_machine_origin(e), C_DGREY);
    }
}


/* ------------------------------------------------------------ globales */
enum { SYS_ROWS = 11 };
static void sys_val(const bm_app *a, int i, char *v, int cap)
{
    const bm_settings *s = &a->set;
    size_t n = (size_t)cap;
    switch (i) {
    case 0: snprintf(v, n, "%s", s->clock ? "EXTERNAL" : "INTERNAL"); break;
    case 1: snprintf(v, n, "%s", s->sync_mode ? "SPLIT L/R" : "STEREO"); break;
    case 2: snprintf(v, n, "%d PPQN", s->sync_ppqn); break;
    case 3: snprintf(v, n, "%d MS", s->sync_lat); break;
    case 4: snprintf(v, n, "%s", s->autosave ? "ON" : "OFF"); break;
    case 5: snprintf(v, n, "%s", s->led_dim == 0 ? "OFF" : (s->led_dim == 1 ? "DIM" : "MEDIUM")); break;
    case 6: snprintf(v, n, "%s", s->fast_lcd ? "ON (TEST)" : "OFF"); break;
    case 7: snprintf(v, n, "%s", s->dual_core ? "ON (TEST)" : "OFF"); break;
    case 8: {
        int nt = 0; for (int k = 0; k < BM_TAKES; k++) nt += a->take[k].valid;
        unsigned fs = (unsigned)(bm_takes_free(a->take) * 10u / BM_SMP_RATE);
        snprintf(v, n, TF("%d+%d, FREE %u.%uS"), nt, bm_snaps_user_n(), fs / 10, fs % 10);
        break;
    }
    case 9: snprintf(v, n, "%s", bm_lang_name()); break;
    default: snprintf(v, n, "%s", BM_VERSION_STR); break;
    }
}

static void draw_system(bm_gfx *g, const bm_app *a)
{
    enum { NR = SYS_ROWS };
    char v[20];
    static const char *const L[NR] = {"CLOCK", "SYNC OUT", "SYNC PPQN", "SYNC LATENCY", "AUTOSAVE", "IDLE LEDS", "FAST SCREEN", "TWO CORES",
                                      "TAKES+SOUNDS", N_("LANGUAGE"), "VERSION"};
    for (int i = 0; i < BM_SYS_VIS; i++) {                       /* solo lo básico (2026-10-21) */
        int r = BM_SYS_ROWS[i];
        if (!gfx_strip_hit(g, 30 + i * 14, 14)) continue;
        sys_val(a, r, v, sizeof v);
        row(g, 30 + i * 14, i == a->menu_row, T(L[r]), v);
    }
}

static void draw_save(bm_gfx *g, const bm_app *a)
{
    char r0[24], r1[24], r4[24];
    snprintf(r0, sizeof r0, "A%02d", a->ctx.st.pattern + 1);
    snprintf(r1, sizeof r1, "A%02d", a->save_target + 1);
    snprintf(r4, sizeof r4, "%d", bm_pattern_count_events(&a->pat));
    header(g, "SAVE", a->dirty ? "UNSAVED" : "SAVED", C_WHITE);
    row(g, 50, a->save_row == 0, "SAVE", r0);
    row(g, 66, a->save_row == 1, "SAVE AS", r1);
    row(g, 82, a->save_row == 2, a->clear_armed ? "CLEAR? SAVE AGAIN" : "CLEAR PATTERN", NULL);
    (void)r4;
    gfx_small(g, 4, 122, a->ctx.st.playing ? "ONLY WHEN STOPPED (CUTS AUDIO)" : "ONLY WHEN STOPPED - SAVE 2 S: QUICK SAVE", a->ctx.st.playing ? C_ORANGE : C_GREY);
}

/* ------------------------------------------------------------ el sonido de la batería (ENV y EDIT) */
/* El instrumento elegido y su golpe dibujado (como en 0.9): la onda espejada que dura lo que DECAY y
 * oscila con TUNE sobre el tono del modelo. Se calcula solo cuando cambia (expf y sinf por x). */
#define HIT_W 228
static int16_t hit_y[HIT_W];
static uint8_t hit_key[4];
static int hit_hz;

static void hit_seg(bm_gfx *g, int xa, int ya, int xb, int yb, uint16_t c)
{
    if ((ya > yb ? ya : yb) < g->cy0 || (ya < yb ? ya : yb) >= g->cy1) return;
    gfx_line(g, xa, ya, xb, yb, c);
}

static void draw_hit(bm_gfx *g, const bm_app *a, int y0, int h)
{
    int t = a->track, lane = a->sel_lane[t] & 15;
    const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[lane];
    int model = ln->model % DRM_COUNT;
    uint16_t col = bm_track_color(t);
    char b[32];
    if (gfx_strip_hit(g, y0, 9)) {
        snprintf(b, sizeof b, "%d %s - %s", lane + 1, T(bm_lane_name(a, t, lane)), T(bm_drm_names[model]));
        gfx_text(g, 4, y0, b, C_WHITE, 1);
    }
    int x0 = 6, wy = y0 + 13, wh = h - 13, mid = wy + wh / 2;
    if (!gfx_strip_hit(g, wy - 2, wh + 4)) return;
    gfx_rect(g, x0 - 2, wy - 2, HIT_W + 4, wh + 4, C_DDGREY);
    gfx_hline(g, x0, mid, HIT_W, C_DDGREY);
    int dec = 6 + ln->decay * (HIT_W - 12) / 127, n = dec < HIT_W ? dec : HIT_W;
    const uint8_t key[4] = {(uint8_t)model, ln->tune, (uint8_t)dec, (uint8_t)wh};
    if (memcmp(key, hit_key, sizeof key)) {
        memcpy(hit_key, key, sizeof key);
        float hz = (float)bm_drm_base_hz[model] * exp2f((float)(ln->tune - 64) * 0.375f / 12.0f);
        for (int x = 0; x < n; x++)
            hit_y[x] = (int16_t)(sinf((float)x * 0.12f * hz / 300.0f) * expf(-4.0f * (float)x / (float)dec) * (float)(wh / 2 - 3));
        hit_hz = (int)(hz + 0.5f);
    }
    for (int x = 1; x < n; x++) {
        hit_seg(g, x0 + x - 1, mid - hit_y[x - 1], x0 + x, mid - hit_y[x], col);
        hit_seg(g, x0 + x - 1, mid + hit_y[x - 1], x0 + x, mid + hit_y[x], dim(col));
    }
    snprintf(b, sizeof b, "%d HZ", hit_hz);
    gfx_small(g, x0 + HIT_W - gfx_small_w(b) - 2, wy + 2, b, C_GREY);
}

/* ------------------------------------------------------------ capas: lo que hace cada tecla */
/* ACENTOS y REPETIR: los pasos del compás del instrumento elegido, cada uno encima de su blanca
 * (mismo ancho que la franja de teclas), para ver qué paso estás tocando. Con golpe: el color del
 * canal; con acento (o repetición): blanco (o amarillo) y su marca; el que suena, subrayado. */
static void draw_steps_ref(bm_gfx *g, const bm_app *a, int y0)
{
    int t = a->track, lane = a->sel_lane[t] & 15;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint16_t col = bm_track_color(t);
    int len = tr->lane[lane].len, rep = a->layer == L_REP;
    char b[24];
    if (gfx_strip_hit(g, y0, 9)) {
        snprintf(b, sizeof b, "%d %s", lane + 1, T(bm_lane_name(a, t, lane)));
        gfx_text(g, 4, y0, b, col, 1);
        snprintf(b, sizeof b, TF("BAR %d/%d"), a->step_page + 1, (len + 15) / 16);
        gfx_text_r(g, 236, y0, b, C_GREY, 1);
    }
    /* los otros instrumentos de la página en gris, una línea fina cada uno: para acentuar sabiendo
     * dónde caen el bombo o la caja (pedido del usuario 2026-10-21) */
    int base = (lane / 8) * 8, gy = y0 + 11;
    for (int i = 0; i < 8; i++) {
        int l = base + i, ry = gy + i * 3;
        if (l == lane || !gfx_strip_hit(g, ry, 2)) continue;
        int ll = tr->lane[l].len ? tr->lane[l].len : 16;
        for (int w = 0; w < 16; w++) {
            int s = a->step_page * 16 + w;
            if (s < ll && (tr->st[l][s] & ST_ON)) gfx_fill(g, w * 15 + 1, ry, 12, 2, C_DGREY);
        }
    }
    int y = gy + 26, h = 22;
    if (!gfx_strip_hit(g, y, h + 3)) return;
    int ps = bm_app_track_step(a, t, lane);
    for (int w = 0; w < 16; w++) {
        int s = a->step_page * 16 + w, x = w * 15;
        if (s >= len) { gfx_fill(g, x + 6, y + h / 2, 2, 2, C_DDGREY); continue; }
        uint8_t v = tr->st[lane][s];
        int rat = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
        bool on = (v & ST_ON) != 0, mark = on && (rep ? rat != 0 : (v & ST_ACC) != 0);
        if (on) gfx_fill(g, x, y, 14, h, mark ? (rep ? C_YELLOW : C_WHITE) : dim(col));
        else gfx_rect(g, x, y, 14, h, w % 4 == 0 ? C_DGREY : C_DDGREY);
        snprintf(b, sizeof b, "%d", s + 1);
        gfx_small(g, x + (14 - gfx_small_w(b)) / 2 + 1, y + 3, b, mark ? C_BLACK : on ? C_WHITE : C_DGREY);
        if (mark) {
            static const char *const R[5] = {"", "X2", "X3", "X4", "FL"};
            const char *m = rep ? R[rat] : "A";
            gfx_small(g, x + (14 - gfx_small_w(m)) / 2 + 1, y + 13, m, C_BLACK);
        }
        if (s == ps) gfx_fill(g, x, y + h + 1, 14, 2, C_ORANGE);
    }
}

/* LENGTH AND SPEED (Slicer): los 8 pads de la página, cada uno con su largo (una barra de 1 a 64
 * pasos, con los compases marcados); el elegido encendido */
static void lane_lengths(bm_gfx *g, const bm_app *a, int y0)
{
    int t = a->track, sel = a->sel_lane[t] & 15, base = (sel / 8) * 8;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint16_t col = bm_track_color(t);
    int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    char b[12];
    for (int i = 0; i < 8; i++) {
        int l = base + i, y = y0 + i * 11, len = tr->lane[l].len ? tr->lane[l].len : 16;
        if (!gfx_strip_hit(g, y, 10)) continue;
        bool s = l == sel;
        if (s) gfx_fill(g, 0, y, 240, 10, C_DDGREY);
        snprintf(b, sizeof b, "P%d", l + 1);
        gfx_small(g, 2, y + 3, b, s ? C_WHITE : C_GREY);
        gfx_fill(g, 20, y + 2, len * 3, 6, s ? col : dim(col));   /* 64 pasos = 192 px */
        for (int c = spb; c < BM_STEPS; c += spb) gfx_vline(g, 20 + c * 3, y + 1, 8, C_DGREY);
        snprintf(b, sizeof b, "%d", len);
        gfx_small(g, 236 - gfx_small_w(b), y + 3, b, s ? C_WHITE : C_GREY);
    }
}

/* La franja del teclado ya muestra cada tecla con su luz; el visor dice qué capa es y cómo se usa. */
static void draw_layer(bm_gfx *g, const bm_app *a)
{
    static char title[24];                                       /* el mapa de la capa: una vez por cuadro */
    static uint16_t col;
    static const char *h1, *h2;
    static uint32_t mf;
    if (mf != g->frame) {
        bm_keyfn K[BM_KEYS];                                     /* aquí solo hace falta el título y las ayudas */
        mf = g->frame;
        bm_app_keymap(a, K, title, sizeof title, &col, &h1, &h2);
    }
    if (gfx_strip_hit(g, 26, 20)) {
        gfx_fill(g, 0, 26, BM_LCD_W, 18, col);
        gfx_text(g, 6, 28, title, C_BLACK, 2);
        char b[24];
        snprintf(b, sizeof b, "T%d", app_pos(a->track) + 1);
        if (a->layer_latch) {                                    /* abierta con un toque: queda hasta cerrarla */
            gfx_small(g, 236 - gfx_small_w("LATCHED"), 28, "LATCHED", C_BLACK);
            gfx_small(g, 236 - gfx_small_w(b), 36, b, C_BLACK);
            page_tag(g, a, 236 - gfx_small_w(b) - 4, 34, 9, C_BLACK);
        } else page_tag(g, a, 234 - gfx_text_r(g, 234, 32, b, C_BLACK, 1) - 4, 31, 9, C_BLACK);
    }
    /* Lo que la capa toca, no una copia de la franja de teclas: en MUTE los instrumentos (mute en rojo,
     * destello al sonar); en ENV y EDIT el sonido elegido y su golpe; en ACENTOS y REPETIR los pasos
     * del compás encima de sus blancas. */
    if (a->layer == L_LEN) lane_lengths(g, a, 48);
    else if (a->layer == L_MUTE) lane_pads(g, a, a->track, 47, 14);
    else if (a->layer == L_HIT || a->layer == L_INST) draw_hit(g, a, 47, 58);
    else if (a->layer == L_ACC || a->layer == L_REP) draw_steps_ref(g, a, 48);
    /* cómo se usa (se va a los 4 s) y cómo se sale (queda) */
}

/* ------------------------------------------------------------ recuadro, pregunta, overlay, toast */
/* ALGORITHM en INSTR (batería): el instrumento al que vas */
static void draw_box(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    if (!gfx_strip_hit(g, 50, 70)) return;
    gfx_fill(g, 40, 50, 160, 68, C_BLACK);
    gfx_rect(g, 40, 50, 160, 68, bm_track_color(t));
    gfx_rect(g, 41, 51, 158, 66, bm_track_color(t));
    char b[24];
    snprintf(b, sizeof b, "%d", a->box_lane + 1);
    gfx_text_c(g, 120, 58, b, bm_track_color(t), 4);
    const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->box_lane & 15];
    if (bm_is_pads(t)) {                                         /* un pad del Slicer: su toma */
        char tk[16];
        if (bm_pad_ok(&a->pat.pad[a->box_lane & 15])) bm_app_src_name(a, a->pat.pad[a->box_lane & 15].src, tk, sizeof tk);
        else snprintf(tk, sizeof tk, "%s", T("EMPTY"));
        snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->box_lane)), tk);
    } else snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->box_lane)), T(bm_drm_short[ln->model % DRM_COUNT]));
    gfx_text_c(g, 120, 92, b, C_WHITE, 1);
    int left = (int)(a->box_until - a->now);
    int w = bm_clampi(left * 140 / 400, 0, 140);
    gfx_fill(g, 50, 108, 140, 3, C_DGREY);
    gfx_fill(g, 50, 108, 140 - w, 3, bm_track_color(t));
}

/* K1-K4: una línea con el nombre y el valor en la fila de arriba (pestañas o título), sin tapar la
 * rejilla ni el paso sostenido; el dial de abajo ya marca cuál giró. */
static void draw_overlay_line(bm_gfx *g, const bm_app *a)
{
    const bm_overlay *o = &a->ov;
    bool tall = a->layer || a->view == V_SAVE;
    int h = tall ? 21 : 11;                                      /* también tapa el título grande de esas pantallas */
    if (!gfx_strip_hit(g, 24, h)) return;
    gfx_fill(g, 0, 24, BM_LCD_W, h, C_BLACK);
    int y = 24 + (h - 7) / 2;
    gfx_text(g, 4, y, o->title, o->color, 1);
    int x = 236;
    if (o->unit[0]) x -= gfx_text_r(g, x, y, o->unit, C_GREY, 1) + 4;
    gfx_text_r(g, x, y, o->val, C_WHITE, 1);
    gfx_fill(g, 0, 24 + h - 1, BM_LCD_W, 1, C_DDGREY);
    gfx_fill(g, 0, 24 + h - 1, o->pct * BM_LCD_W / 255, 1, o->color);
}

static void draw_overlay(bm_gfx *g, const bm_app *a)
{
    const bm_overlay *o = &a->ov;
    if (o->knob >= ENC_K1 + 1 && o->knob <= ENC_K4 + 1 && !o->beat) { draw_overlay_line(g, a); return; }
    if (!gfx_strip_hit(g, 46, 84)) return;
    gfx_fill(g, 12, 46, 216, 84, C_BLACK);
    gfx_rect(g, 12, 46, 216, 84, o->color);
    gfx_rect(g, 13, 47, 214, 82, o->color);
    grid(g, 16, 50, 208, 76, 6);
    gfx_text(g, 20, 52, o->title, C_GREY, 1);
    if (o->knob) {                                               /* la perilla grande que se giró, en la esquina */
        static const char *const KN[ENC_COUNT] = {"SELECT", "ALGORITHM", "K1", "K2", "K3", "K4", "PRESETS"};
        gfx_small(g, 222 - gfx_small_w(KN[(o->knob - 1) % ENC_COUNT]), 53, KN[(o->knob - 1) % ENC_COUNT], o->color);
    }
    int w = gfx_text(g, 20, 68, o->val, C_WHITE, gfx_text_w(o->val, 3) > 196 ? 2 : 3);
    if (o->unit[0]) gfx_text(g, 26 + w, 78, o->unit, C_GREY, 2);
    if (o->beat) {                                               /* el beat, en el color del tempo */
        const bm_status *st = &a->ctx.st;
        int cur = st->playing ? (int)((st->btick / BM_PPQN) % 4) : -1;
        for (int i = 0; i < 4; i++) gfx_fill(g, 20 + i * 51, 108, 47, 12, i == cur ? o->color : dim(dim(o->color)));
        return;
    }
    gfx_fill(g, 20, 110, 200, 8, C_DGREY);
    gfx_fill(g, 20, 110, o->pct * 200 / 255, 8, o->color);
    if (o->ghost != 255) gfx_fill(g, 19 + o->ghost * 200 / 255, 106, 2, 16, C_WHITE);
}

static void draw_toast(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 118, 16)) return;
    gfx_fill(g, 0, 118, BM_LCD_W, 16, C_DGREY);
    gfx_hline(g, 0, 118, BM_LCD_W, C_CYAN);
    gfx_text_c(g, 120, 122, a->toast, C_WHITE, 1);
}

/* ------------------------------------------------------------ BX-7 (V_MACH): el FM-1 de fábrica, con nuestra cara */
extern const char *const bx7_fx_names[BXF_N];

/* La topología del algoritmo como en una DX7: portadoras abajo, moduladores encima, la realimentación
 * como un lazo; cada operador con su nivel (la barra blanca) y abajo las relaciones de frecuencia. */
static void draw_fm(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h, int sel)
{
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    static struct { uint32_t frame; int t, alg, bx[6], by[6]; uint8_t p[BM_FM_PATCH], car, mods[6]; int8_t fb[2]; } L;
    if (L.frame != g->frame || L.t != t) {
        L.frame = g->frame; L.t = t;
        uint8_t *p = L.p, *mods = L.mods;
        bm_fm_patch_of(&a->pat, t, NULL, p);
        int alg = L.alg = p[BM_FM_ALG] & 31;
        uint8_t car = L.car = bm_fm_carriers(alg);
        bm_fm_alg_graph(alg, mods, L.fb);
        int depth[6] = {0, 0, 0, 0, 0, 0}, maxd = 0;
        for (int it = 0; it < 6; it++)
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 6; j++)
                    if (mods[j] & (1u << i)) if (depth[i] < depth[j] + 1) depth[i] = depth[j] + 1;
        for (int i = 0; i < 6; i++) if (depth[i] > maxd) maxd = depth[i];
        float cx[6]; int placed[6] = {0, 0, 0, 0, 0, 0}, col = 0;
        for (int pass = 0; pass < 2; pass++)
            for (int op = 1; op <= 6; op++) {
                int i = 6 - op;
                if (placed[i]) continue;
                bool carrier = (car >> i) & 1;
                if (pass == 0 && !carrier) continue;
                int tgt = -1;
                for (int j = 0; j < 6; j++) if (mods[j] & (1u << i)) { tgt = j; break; }
                if (tgt >= 0 && placed[tgt]) {
                    int sib = 0;
                    for (int k = 0; k < 6; k++) if (k != i && placed[k] && (mods[tgt] & (1u << k))) sib++;
                    cx[i] = cx[tgt] + (float)sib;
                    if (sib) { cx[i] = (float)col; col++; }
                } else { cx[i] = (float)col; col++; }
                placed[i] = 1;
            }
        int ncol = col > 0 ? col : 1;
        float cw = (float)(w - 8) / (float)ncol;
        int rh = (h - 18) / (maxd + 1);
        if (rh > 22) rh = 22;
        for (int i = 0; i < 6; i++) {
            L.bx[i] = x0 + 4 + (int)(cx[i] * cw + cw / 2.0f) - 9;
            L.by[i] = y0 + h - 18 - depth[i] * rh - 12;
        }
    }
    const uint8_t *p = L.p, *mods = L.mods, car = L.car;
    const int8_t *fb = L.fb;
    const int *bx = L.bx, *by = L.by, alg = L.alg;
    gfx_rect(g, x0, y0, w, h, C_DDGREY);
    grid(g, x0 + 2, y0 + 2, w - 4, h - 4, 8);
    uint16_t tc = bm_track_color(t);
    for (int j = 0; j < 6; j++)
        for (int i = 0; i < 6; i++)
            if (mods[j] & (1u << i)) gfx_line(g, bx[i] + 9, by[i] + 12, bx[j] + 9, by[j], C_GREY);
    for (int i = 0; i < 6; i++) if ((car >> i) & 1) gfx_vline(g, bx[i] + 9, by[i] + 12, y0 + h - 14 - by[i] - 12, C_GREY);
    gfx_hline(g, x0 + 4, y0 + h - 14, w - 8, C_DGREY);
    if (fb[0] >= 0 && fb[1] >= 0) {
        int a0 = fb[0], a1 = fb[1];
        int xr = bx[a0] + 20;
        gfx_hline(g, bx[a0] + 18, by[a0] + 6, 4, C_ORANGE);
        gfx_vline(g, xr, by[a1] - 3, by[a0] + 6 - (by[a1] - 3) + 1, C_ORANGE);
        gfx_hline(g, bx[a1] + 9, by[a1] - 3, xr - bx[a1] - 9, C_ORANGE);
        gfx_vline(g, bx[a1] + 9, by[a1] - 3, 3, C_ORANGE);
    }
    char b[12];
    for (int i = 0; i < 6; i++) {
        const uint8_t *o = p + i * BM_FM_OP;
        bool c = (car >> i) & 1;
        int lv = o[FMO_OL] * 16 / 99;
        gfx_fill(g, bx[i], by[i], 18, 12, c ? tc : C_DGREY);
        gfx_fill(g, bx[i], by[i] + 12 - lv * 12 / 16, 2, lv * 12 / 16, C_WHITE);
        if (sel == 5 - i) gfx_rect(g, bx[i] - 2, by[i] - 2, 22, 16, C_WHITE);      /* el operador que se edita */
        snprintf(b, sizeof b, "%d", 6 - i);
        gfx_text(g, bx[i] + 7, by[i] + 3, b, c ? C_BLACK : C_WHITE, 1);
    }
    snprintf(b, sizeof b, "ALG %d", alg + 1);
    gfx_text(g, x0 + 4, y0 + h - 10, b, C_ORANGE, 1);
    snprintf(b, sizeof b, "FB %d", p[BM_FM_FB]);
    gfx_small(g, x0 + 56, y0 + h - 9, b, C_ORANGE);
    for (int i = 0; i < 6; i++) {
        const uint8_t *o = p + (5 - i) * BM_FM_OP;
        int fc = o[FMO_FC], ff = o[FMO_FF];
        if (o[FMO_MODE]) snprintf(b, sizeof b, "FX");
        else { int r100 = (fc ? fc * 100 : 50) * (100 + ff) / 100; if (r100 % 100) snprintf(b, sizeof b, "%d.%d", r100 / 100, r100 % 100 / 10); else snprintf(b, sizeof b, "%d", r100 / 100); }
        gfx_small(g, x0 + 86 + i * 24, y0 + h - 9, b, i == sel ? C_WHITE : C_GREY);
    }
}

/* una envolvente de 4 tramos (tiempos 0..1 de ancho relativo, niveles 0..1) en un recuadro */
static void env_shape(bm_gfx *g, int x0, int y0, int w, int h, const float tm[4], const float lv[4], float start, uint16_t c)
{
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    gfx_rect(g, x0, y0, w, h, C_DDGREY);
    float sum = tm[0] + tm[1] + tm[2] + tm[3];
    if (sum <= 0.0f) sum = 1.0f;
    int x = x0 + 2, y = y0 + h - 2 - (int)(start * (float)(h - 4));
    for (int i = 0; i < 4; i++) {
        int nx = x + (int)(tm[i] / sum * (float)(w - 4)), ny = y0 + h - 2 - (int)(lv[i] * (float)(h - 4));
        gfx_line(g, x, y, nx, ny, c);
        x = nx; y = ny;
    }
}

/* Las cuatro perillas como una lista (GLO, ARP, LFO...): una fila por perilla, en su orden, con su
 * dial en su color, el nombre y el valor grande. Es la única vez que se dibujan (sin la franja). */
static void mach_list(bm_gfx *g, const bm_app *a, int y0, int rh)
{
    static bm_legend L[4];                                       /* una vez por cuadro, no por franja */
    static uint32_t lf;
    if (lf != g->frame) { lf = g->frame; bm_app_legend(a, L); }
    for (int k = 0; k < 4; k++) {
        int y = y0 + k * rh, h = rh - 3, cy = y + h / 2;
        if (!L[k].name[0] || !gfx_strip_hit(g, y, h)) continue;
        uint16_t kc = bm_knob_color(k);
        bool hi = a->ov.on && a->ov.enc == k;
        gfx_fill(g, 2, y, 236, h, C_DDGREY);
        if (hi) gfx_rect(g, 2, y, 236, h, kc);
        draw_dial(g, 19, cy, L[k].pct, -1, kc, hi);
        gfx_text(g, 40, cy - 3, T(L[k].name), kc, 1);
        int s = gfx_text_w(L[k].val, 2) > 236 - 110 ? 1 : 2;
        gfx_text_r(g, 233, cy - (s == 2 ? 7 : 3), L[k].val, C_WHITE, s);
    }
}

/* ------------------------------------------------------------ BB-303: el panel de la TB-303 */
/* una perilla del panel: de 7:00 a 5:00, como las de la 303; el aro con el color de la perilla del
 * FM-1 que la mueve (o gris si ninguna la mueve en esta página) */
static void dial(bm_gfx *g, int cx, int cy, int r, int v, int max, uint16_t ring, const char *name)
{
    if (!gfx_strip_hit(g, cy - r, 2 * r + 22)) return;
    gfx_disc(g, cx, cy, r, ring);
    gfx_disc(g, cx, cy, r - 2, C_DGREY);
    float ang = (-135.0f + 270.0f * (float)v / (float)(max > 0 ? max : 1)) * 0.0174533f;
    gfx_line(g, cx, cy, cx + (int)(sinf(ang) * (float)(r - 2)), cy - (int)(cosf(ang) * (float)(r - 2)), ring == C_DDGREY ? C_GREY : C_WHITE);
    uint16_t nc = ring == C_DDGREY ? C_DGREY : C_WHITE;           /* el nombre, grande (se lee); dos palabras, dos renglones */
    const char *sp = strchr(name, ' ');
    if (!sp) { gfx_text(g, cx - gfx_text_w(name, 1) / 2, cy + r + 4, name, nc, 1); return; }
    char w1[12];
    snprintf(w1, sizeof w1, "%.*s", (int)(sp - name), name);
    gfx_text(g, cx - gfx_text_w(w1, 1) / 2, cy + r + 4, w1, nc, 1);
    gfx_text(g, cx - gfx_text_w(sp + 1, 1) / 2, cy + r + 13, sp + 1, nc, 1);
}

/* ------------------------------------------------------------ el dibujo de lo que se mueve
 * (2026-10-22) Debajo del panel, en el lugar que dejó la franja de perillas: la curva del filtro, la
 * forma de la onda o el contorno de la sección que se edita. */
#define PIC_X 6
#define PIC_W 228
enum { FC_LP4, FC_LP2, FC_BP, FC_HP, FC_FLAT };

/* La respuesta del filtro de 20 Hz a 20 kHz (eje logarítmico): el corte en Hz y RESO de 0 a 127. La curva se
 * calcula solo cuando cambia (powf por columna). */
static void filter_curve(bm_gfx *g, int y0, int h, float fc, int res, int type, uint16_t col)
{
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    static int16_t ys[PIC_W];
    static int key[5] = {-1, -1, -1, -1, -1};
    int k[5] = {(int)fc, res, type, y0, h};
    if (memcmp(k, key, sizeof k)) {
        memcpy(key, k, sizeof k);
        float q = 0.6f + (float)res / 127.0f * 7.0f;
        for (int x = 0; x < PIC_W; x++) {
            float r = 20.0f * powf(1000.0f, (float)x / (float)(PIC_W - 1)) / fc;
            float d = sqrtf((1.0f - r * r) * (1.0f - r * r) + (r / q) * (r / q));
            float m = type == FC_HP ? r * r / d : type == FC_BP ? (r / q) / d : type == FC_FLAT ? 1.0f : 1.0f / d;
            if (type == FC_LP4) m /= sqrtf((1.0f - r * r) * (1.0f - r * r) + 2.0f * r * r);   /* y dos polos más */
            float db = 20.0f * log10f(m > 1e-6f ? m : 1e-6f);
            if (db > 18.0f) db = 18.0f;
            if (db < -42.0f) db = -42.0f;
            ys[x] = (int16_t)(y0 + 2 + (int)((18.0f - db) / 60.0f * (float)(h - 4)));
        }
    }
    int base = y0 + h - 2;
    gfx_rect(g, PIC_X - 2, y0, PIC_W + 4, h, C_DDGREY);
    for (int f = 100; f <= 10000; f *= 10) gfx_vline(g, PIC_X + (int)(log10f((float)f / 20.0f) / 3.0f * (float)(PIC_W - 1)), y0 + 1, h - 2, C_DDGREY);
    gfx_hline(g, PIC_X, y0 + 2 + (h - 4) * 18 / 60, PIC_W, C_DDGREY);   /* 0 dB */
    uint16_t fill = dim(dim(col));
    for (int x = 0; x < PIC_W; x++) {
        int y = ys[x];
        if (y < base) gfx_vline(g, PIC_X + x, y, base - y, fill);
        if (x) hit_seg(g, PIC_X + x - 1, ys[x - 1], PIC_X + x, y, col);
    }
}

/* Dos ciclos de una forma de onda (fn devuelve -1..1 para una fase 0..2: dos ciclos, para que el
 * sub de una octava abajo haga uno) */
typedef float (*wave_fn)(const bm_app *a, float ph2);
static void osc_shape(bm_gfx *g, const bm_app *a, int y0, int h, wave_fn fn, uint16_t col)
{
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    int mid = y0 + h / 2, amp = h / 2 - 4, py = mid;
    gfx_rect(g, PIC_X - 2, y0, PIC_W + 4, h, C_DDGREY);
    gfx_hline(g, PIC_X, mid, PIC_W, C_DDGREY);
    for (int x = 0; x < PIC_W; x++) {
        float v = fn(a, (float)x * 2.0f / (float)PIC_W);
        int y = mid - (int)((v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v) * (float)amp);
        if (x) hit_seg(g, PIC_X + x - 1, py, PIC_X + x, y, col);
        py = y;
    }
}

static float frac1(float x) { return x - (float)(int)x; }

/* SCL del BB-303 y del B-MINI: el roll de la página de 16 pasos y debajo el ACCENT y el SLIDE (o GLIDE)
 * de cada paso; la fila que se edita, encendida */
static void draw_scl(bm_gfx *g, const bm_app *a, const char *row2)
{
    int t = a->track;
    const bm_bx7ui *u = &a->bx;
    bool sl = u->sub[BXP_SCL] % 2 != 0;
    int L = mach_len(a);
    draw_seq_roll(g, a, t, 39, 82, false, u->blk);
    for (int row = 0; row < 2; row++) {
        int y = 90 + row * 15;
        if (!gfx_strip_hit(g, y, 13)) continue;
        bool act = row == (sl ? 1 : 0);
        uint16_t rc = row ? C_GREEN : C_ORANGE;
        gfx_small(g, 2, y + 3, row ? row2 : "ACC", act ? rc : C_DGREY);
        for (int w = 0; w < 16; w++) {
            int s = u->blk * 16 + w, x = 20 + w * 14;
            if (s >= L) continue;
            bool has = false, on = false;
            for (int e = 0; e < BM_EVENTS; e++) {
                const bm_event *ev = bm_evc(&a->pat, t, s, e);
                if (bm_event_empty(ev)) continue;
                has = true;
                on |= row ? (ev->len & BM_LEN_TIE) != 0 : ev->vel >= BM_VEL_ACC;
            }
            if (on) gfx_fill(g, x, y, 12, 12, act ? rc : dim(rc));
            else gfx_rect(g, x, y, 12, 12, has ? (act ? C_GREY : C_DGREY) : C_DDGREY);
        }
    }
}

static float wv_303(const bm_app *a, float ph2)                 /* SAW o SQR, como el interruptor */
{
    float p = frac1(ph2);
    return (a->pat.prm[a->track][AC_WAVE] & 1) ? (p < 0.5f ? 1.0f : -1.0f) : 1.0f - 2.0f * p;
}

static void draw_b303(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_bx7ui *u = &a->bx;
    const bm_track *tr = &a->pat.trk[t];
    const uint8_t *p = a->pat.prm[t];
    uint16_t col = bm_track_color(t);
    char b[16];
    switch (u->page) {
    case BXP_SEQ:                                                 /* el piano roll: ACCENT en blanco, SLIDE en verde */
        draw_seq_roll(g, a, t, 39, VB - 10, false, mach_roll_page(a, t));
        return;
    case BXP_SCL:                                                 /* ACCENT o SLIDE de cada paso: las 16 blancas */
        draw_scl(g, a, "SLD");
        return;
    case BXP_LFO: {                                               /* las cuatro perillas y la forma de un ciclo */
        mach_list(g, a, 37, 30);
        if (gfx_strip_hit(g, 158, 35)) {
            int w = p[AC_LFO_WAVE] % ACW_N, px = -1, py = 0;
            uint16_t c = p[AC_LFO_DEPTH] ? col : C_DGREY;
            gfx_rect(g, 8, 158, 224, 34, C_DDGREY);
            for (int x = 0; x < 222; x++) {
                int y = 189 - (int)(bm_acid_lfo_shape(w, (float)x / 222.0f, 0) * 28.0f);
                if (px >= 0) gfx_line(g, 8 + px, py, 9 + x, y, c);
                px = x + 1; py = y;
            }
        }
        return;
    }
    case BXP_FX: {                                                /* las tres casillas del TB-03, en su orden */
        int pg = u->sub[BXP_FX] % 2;
        for (int i = 0; i < ACF_N; i++) {
            int x = 4 + i * 80, y = 44;
            if (!gfx_strip_hit(g, y, 46)) continue;
            bool on = bm_acid_fx_on(p, i), sel = pg ? i != ACF_OD : i == ACF_OD;
            if (on) gfx_fill(g, x, y, 72, 44, dim(col));
            gfx_rect(g, x, y, 72, 44, sel ? C_WHITE : C_DGREY);
            if (sel) gfx_rect(g, x + 1, y + 1, 70, 42, C_WHITE);
            gfx_small(g, x + 36 - gfx_small_w(T(b303_fx_names[i])) / 2, y + 14, T(b303_fx_names[i]), on ? C_WHITE : C_GREY);
            gfx_small(g, x + 36 - gfx_small_w(on ? "ON" : "OFF") / 2, y + 28, on ? "ON" : "OFF", on ? C_ORANGE : C_DGREY);
            if (i < ACF_N - 1) { gfx_hline(g, x + 73, y + 22, 6, C_GREY); gfx_line(g, x + 76, y + 19, x + 78, y + 22, C_GREY); gfx_line(g, x + 76, y + 25, x + 78, y + 22, C_GREY); }
        }
        return;
    }
    case BXP_GLO: case BXP_ARP:
        mach_list(g, a, 40, 38);
        return;
    default: break;
    }
    /* el panel: la fila de perillas de la 303 y debajo la forma de onda, el slide y el volumen */
    int pg = u->sub[BXP_MAIN] % 2;
    uint16_t k1 = bm_knob_color(0), k2 = bm_knob_color(1), k3 = bm_knob_color(2), k4 = bm_knob_color(3), off = C_DDGREY;
    static const char *const NM[6] = {"TUNING", "CUT OFF", "RESO", "ENV MOD", "DECAY", "ACCENT"};
    int val[6] = {tr->transpose, tr->macro[0], tr->macro[1], tr->macro[2], tr->macro[3], tr->accent};
    int mx[6] = {48, 127, 127, 127, 127, 127};
    uint16_t ring[6] = {pg ? k1 : off, pg ? off : k1, pg ? off : k2, pg ? off : k3, pg ? off : k4, C_ORANGE};
    for (int i = 0; i < 6; i++) dial(g, 20 + i * 40, 54, 14, bm_clampi(val[i], 0, mx[i]), mx[i], ring[i], NM[i]);
    if (gfx_strip_hit(g, 92, 30)) {
        uint16_t wc = pg ? k2 : C_DGREY;
        int w = p[AC_WAVE] & 1;
        gfx_text(g, 6, 94, "WAVEFORM", pg ? C_WHITE : C_DGREY, 1);
        for (int i = 0; i < 2; i++) {                             /* el interruptor: SAW / SQR */
            int x = 6 + i * 34;
            if (i == w) gfx_fill(g, x, 104, 32, 15, wc); else gfx_rect(g, x, 104, 32, 15, C_DGREY);
            gfx_text(g, x + 16 - gfx_text_w(i ? "SQR" : "SAW", 1) / 2, 108, i ? "SQR" : "SAW", i == w ? C_BLACK : C_DGREY, 1);
        }
        static const bxp GL = {"SLIDE", K_TRK, 0, 0, 127, F_GLIDE, NULL};
        mach_fmt(&GL, tr->glide, b, sizeof b);
        gfx_text(g, 92, 94, "SLIDE", pg ? C_WHITE : C_DGREY, 1);
        gfx_text(g, 92, 105, b, pg ? k3 : C_DGREY, 2);
    }
    dial(g, 210, 105, 12, tr->level, 127, pg ? k4 : off, "VOLUME");
    /* abajo, lo que mueve la página: el filtro (CUT OFF y RESO) o la onda (SAW / SQR) */
    if (pg) osc_shape(g, a, 134, 58, wv_303, col);
    else filter_curve(g, 134, 58, bm_acid_cut_hz(tr->macro[0]), tr->macro[1], FC_LP4, col);
}

/* ------------------------------------------------------------ las de panel: B-MINI, BUNO-6, BITTAL */
/* un contorno (ATTACK, DECAY, SUSTAIN, RELEASE) desde sus cuatro bytes de prm */
static void env_contour(bm_gfx *g, const uint8_t *q, int y0, int h, uint16_t c)
{
    float sus = (float)q[2] / 127.0f;
    float tm[4] = {0.05f + (float)q[0] / 127.0f, 0.1f + (float)q[1] / 127.0f, 0.5f, 0.1f + (float)q[3] / 127.0f};
    float lv[4] = {1.0f, sus, sus, 0.0f};
    env_shape(g, 4, y0, 232, h, tm, lv, 0.0f, c);
}

/* lo que sale del LFO (y de la modulación) en el recuadro de abajo, x 0..1 */
static float panel_lfo_at(int eng, const uint8_t *p, float x)
{
    float ph = x * 2.0f, f = ph - (float)(int)ph;
    float tri = 4.0f * fabsf(f - 0.5f) - 1.0f, sqr = f < 0.5f ? 1.0f : -1.0f;
    if (eng == ENG_BUNO) {                                         /* el Juno: triángulo, entra con su DELAY */
        float d = (float)p[BN_LDELAY] / 127.0f, r = d > 0.0f ? x / d : 1.0f;
        return tri * (r < 1.0f ? r : 1.0f);
    }
    if (eng == ENG_BITTAL) {
        switch (p[BT_LSHAPE] % BT_LSHAPES) {
        case BT_L_SIN: return sinf(ph * 6.2831853f);
        case BT_L_SAW: return 2.0f * f - 1.0f;
        case BT_L_SQR: return sqr;
        case BT_L_SH: { uint32_t r = (uint32_t)(ph * 4.0f) * 2654435761u; return (float)((r >> 24) & 255) / 127.5f - 1.0f; }
        default: return tri;
        }
    }
    float s3 = (float)p[MN_MIX] / 127.0f, o3p = ph * 4.0f - (float)(int)(ph * 4.0f), o3;   /* B-MINI: MOD MIX, OSC 3 y LFO */
    switch (p[MN_W3] % MN_WAVES) {
    case MN_WAVE_TRI: o3 = 4.0f * fabsf(o3p - 0.5f) - 1.0f; break;
    case MN_WAVE_SQR: o3 = o3p < 0.5f ? 1.0f : -1.0f; break;
    case MN_WAVE_WIDE: o3 = o3p < 0.3f ? 1.0f : -1.0f; break;
    case MN_WAVE_NARROW: o3 = o3p < 0.12f ? 1.0f : -1.0f; break;
    default: o3 = 2.0f * o3p - 1.0f; break;
    }
    return (1.0f - s3) * (p[MN_LWAVE] ? sqr : tri) + s3 * o3;
}

/* BITTAL: la onda de cada tabla en su posición, la que está sonando (con el LFO, las macros y ENV 2)
 * o, callado, la del FRAME */
static void bt_waves(bm_gfx *g, const bm_app *a, int y0, int h, uint16_t col)
{
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    static uint32_t seen, still;
    uint32_t sq = bm_wv_btseq;
    if (sq != seen) { seen = sq; still = 0; } else if (still < 100) still++;
    const uint8_t *p = a->pat.prm[a->track];
    for (int o = 0; o < 2; o++) {
        int x0 = 4 + o * 118, w = 114, mid = y0 + h / 2;
        bool on = p[o ? BT_L2 : BT_L1] > 0;
        gfx_rect(g, x0, y0, w, h, C_DDGREY);
        gfx_small(g, x0 + 3, y0 + 2, o ? "OSC 2" : "OSC 1", on ? C_GREY : C_DGREY);
        if (!bm_wt_ready(o)) continue;
        float pos = still < 8 ? bm_wv_btpos[o] : (float)p[o ? BT_P2 : BT_P1] / 127.0f;
        int nf = bm_wt_frames(o), n = 0, nb = 0;
        float fp = pos * (float)(nf - 1);
        int fa = (int)fp, fb = fa + 1 < nf ? fa + 1 : fa;
        float ff = fp - (float)fa;
        const int16_t *ta = bm_wt_data(o, fa, 0, &n), *tb = bm_wt_data(o, fb, 0, &nb);
        if (!ta || !tb || n <= 0 || nb != n) continue;
        int py = mid;
        for (int x = 0; x < w - 4; x++) {
            int i = x * n / (w - 4);
            float v = ((float)ta[i] + ff * (float)(tb[i] - ta[i])) * (1.0f / 32768.0f);
            int y = mid - (int)(v * (float)(h / 2 - 3));
            if (x) gfx_line(g, x0 + 1 + x, py, x0 + 2 + x, y, on ? col : C_DGREY);
            py = y;
        }
    }
}

/* FX de las de panel y del Slicer: DISTORTION > CHORUS > REVERB, la página encendida (s 0: DISTORTION) */
static void px_fx_boxes(bm_gfx *g, const bm_app *a, int s)
{
    int t = a->track, eng = a->pat.trk[t].engine;
    const uint8_t *p = a->pat.prm[t];
    uint16_t col = bm_track_color(t);
    for (int i = 0; i < PXF_N; i++) {
        int x = 4 + i * 80, y = 44;
        if (!gfx_strip_hit(g, y, 46)) continue;
        bool on = bm_px_fx_on(p, eng, i), sel = s ? i != PXF_DIST : i == PXF_DIST;
        int ch = bm_px_chorus(p, eng);
        const char *st = i == PXF_CHORUS ? (eng == ENG_BUNO ? bm_bn_chorus_names[ch] : bm_px_chorus_names[ch]) : on ? "ON" : "OFF";
        if (on) gfx_fill(g, x, y, 72, 44, dim(col));
        gfx_rect(g, x, y, 72, 44, sel ? C_WHITE : C_DGREY);
        if (sel) gfx_rect(g, x + 1, y + 1, 70, 42, C_WHITE);
        gfx_small(g, x + 36 - gfx_small_w(T(panel_fx_names[i])) / 2, y + 14, T(panel_fx_names[i]), on ? C_WHITE : C_GREY);
        gfx_small(g, x + 36 - gfx_small_w(st) / 2, y + 28, st, on ? C_ORANGE : C_DGREY);
        if (i < PXF_N - 1) { gfx_hline(g, x + 73, y + 22, 6, C_GREY); gfx_line(g, x + 76, y + 19, x + 78, y + 22, C_GREY); gfx_line(g, x + 76, y + 25, x + 78, y + 22, C_GREY); }
    }
}

/* las formas del B-MINI (la del oscilador de la sección) */
static float wv_mini(const bm_app *a, float ph2)
{
    const uint8_t *q = a->pat.prm[a->track];
    int s = panel_sub(a), w = q[s == 1 ? MN_W1 : s == 2 ? MN_W2 : MN_W3] % MN_WAVES;
    float p = frac1(ph2), saw = 1.0f - 2.0f * p, tri = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
    switch (w) {
    case MN_WAVE_TRI: return tri;
    case MN_WAVE_SHARK: return 0.5f * (tri + saw);
    case MN_WAVE_SQR: return p < 0.5f ? 1.0f : -1.0f;
    case MN_WAVE_WIDE: return p < 0.3f ? 1.0f : -1.0f;
    case MN_WAVE_NARROW: return p < 0.12f ? 1.0f : -1.0f;
    default: return saw;
    }
}

/* el DCO del Juno: el pulso (con su ancho), el diente de sierra y el sub de una octava abajo, sumados */
static float wv_buno(const bm_app *a, float ph2)
{
    const uint8_t *q = a->pat.prm[a->track];
    float p = frac1(ph2), v = 0.0f, n = 0.0f;
    if (q[BN_PULSE]) { v += p < 0.5f - (float)q[BN_PWM] / 127.0f * 0.45f ? 1.0f : -1.0f; n += 1.0f; }
    if (q[BN_SAW]) { v += 1.0f - 2.0f * p; n += 1.0f; }
    if (q[BN_SUB]) { float s = (float)q[BN_SUB] / 127.0f; v += (ph2 < 1.0f ? s : -s); n += s; }
    return n > 0.0f ? v / n : 0.0f;
}

static float wv_blfo(const bm_app *a, float ph2) { return panel_lfo_at(ENG_BUNO, a->pat.prm[a->track], ph2 * 0.5f); }

/* debajo del panel, lo que mueve la sección: el filtro, la onda del oscilador o el contorno */
static void panel_picture(bm_gfx *g, const bm_app *a, int y0, int h)
{
    int t = a->track, eng = a->pat.trk[t].engine, s = panel_sub(a);
    const uint8_t *p = a->pat.prm[t];
    const bm_track *tr = &a->pat.trk[t];
    uint16_t col = bm_track_color(t);
    switch (eng) {
    case ENG_MINI:
        if (s == 0) filter_curve(g, y0, h, bm_cut_hz(tr->macro[0]), tr->macro[1], FC_LP4, col);
        else if (s <= 3) osc_shape(g, a, y0, h, wv_mini, p[s == 1 ? MN_L1 : s == 2 ? MN_L2 : MN_L3] ? col : C_DGREY);
        else env_contour(g, p + MN_AA, y0, h, col);
        return;
    case ENG_BUNO:
        if (s == 0 || s == 3) filter_curve(g, y0, h, bm_cut_hz(tr->macro[0]), tr->macro[1], FC_LP4, col);
        else if (s == 1) osc_shape(g, a, y0, h, wv_buno, col);
        else if (s == 2) osc_shape(g, a, y0, h, wv_blfo, p[BN_LFODCO] ? col : C_DGREY);
        else env_contour(g, p + BN_A, y0, h, col);
        return;
    default: {                                                    /* BITTAL */
        static const uint8_t FT[BT_FILTERS] = {FC_LP4, FC_LP2, FC_BP, FC_HP, FC_LP4, FC_FLAT};
        if (s <= 3) bt_waves(g, a, y0, h, col);
        else if (s <= 5) filter_curve(g, y0, h, bm_cut_hz(p[BT_CUT]), p[BT_RES], FT[p[BT_FT] % BT_FILTERS], col);
        else env_contour(g, p + BT_AA, y0, h, col);
        return;
    }
    }
}

static void draw_panel(bm_gfx *g, const bm_app *a)
{
    int t = a->track, eng = a->pat.trk[t].engine;
    const bm_bx7ui *u = &a->bx;
    const uint8_t *p = a->pat.prm[t];
    uint16_t col = bm_track_color(t);
    int s = panel_sub(a);
    const bxp *P = panel_knobs(a);
    char b[16];
    switch (u->page) {
    case BXP_SEQ:                                                 /* el piano roll: ACCENT en blanco, GLIDE en verde */
        draw_seq_roll(g, a, t, 39, VB - 10, false, mach_roll_page(a, t));
        return;
    case BXP_SCL:
        draw_scl(g, a, "GLD");
        return;
    case BXP_ENV:                                                 /* el contorno de la página, grande */
        env_contour(g, p + P[0].idx, 40, 92, col);
        return;
    case BXP_LFO:                                                 /* las perillas y lo que sale */
        mach_list(g, a, 37, 30);
        if ((eng != ENG_BITTAL || s == 0) && gfx_strip_hit(g, 158, 35)) {
            int amt = eng == ENG_BUNO ? 127 : p[eng == ENG_BITTAL ? BT_LAMT : MN_LAMT], py = 0;
            uint16_t c = amt ? col : C_DGREY;
            gfx_rect(g, 8, 158, 224, 34, C_DDGREY);
            for (int x = 0; x < 222; x++) {
                int y = 175 - (int)(panel_lfo_at(eng, p, (float)x / 222.0f) * 14.0f);
                if (x) gfx_line(g, 8 + x, py, 9 + x, y, c);
                py = y;
            }
        }
        return;
    case BXP_FX:                                                  /* las tres casillas, en el orden de la cadena */
        px_fx_boxes(g, a, s);
        return;
    case BXP_GLO: case BXP_ARP:
        mach_list(g, a, 40, 38);
        return;
    default: break;
    }
    /* el panel: sus secciones (la de la página, encendida), sus cuatro perillas (las únicas: sin la
     * franja de abajo) y debajo el dibujo de la sección: el filtro, la onda o el contorno */
    int ns = panel_edit_n(a), tw = 236 / ns;
    int cy = 70, r = 18, vy = 114;
    if (gfx_strip_hit(g, 38, 11))
        for (int i = 0; i < ns; i++) {
            int x = 2 + i * tw;
            const char *nm = T(panel_edit_name(a, i));
            char sq[12];
            if (gfx_small_w(nm) > tw - 4) {                      /* pestaña angosta: sin espacios ("OSC1+") */
                int j = 0;
                for (const char *c = nm; *c && j < (int)sizeof sq - 1; c++) if (*c != ' ') sq[j++] = *c;
                sq[j] = 0;
                nm = sq;
            }
            if (i == s) gfx_fill(g, x, 38, tw - 2, 10, col); else gfx_rect(g, x, 38, tw - 2, 10, C_DDGREY);
            gfx_small(g, x + (tw - 2) / 2 - gfx_small_w(nm) / 2, 40, nm, i == s ? C_BLACK : C_GREY);
        }
    for (int k = 0; k < 4; k++) {
        if (P[k].kind == K_NONE) continue;
        int v = mach_get(a, &P[k]), cx = 30 + k * 60;
        dial(g, cx, cy, r, v - P[k].lo, P[k].max - P[k].lo, bm_knob_color(k), T(P[k].name));
        if (gfx_strip_hit(g, vy - 1, 9)) {
            mach_fmt(&P[k], v, b, sizeof b);
            gfx_text_c(g, cx, vy, b, C_ORANGE, 1);
        }
    }
    panel_picture(g, a, 128, 64);
}

static void draw_bx7(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_bx7ui *u = &a->bx;
    uint16_t col = bm_track_color(t);
    char b[30];
    switch (u->page) {
    case BXP_SEQ:                                                 /* el piano: el canal entero, como SLOOP */
        draw_seq_roll(g, a, t, 39, VB, false, mach_roll_page(a, t));
        return;
    case BXP_ENV: {
        const bm_track *tr = &a->pat.trk[t];
        float tm[4] = {0.05f + (float)tr->attack / 127.0f, 0.25f + (float)(tr->macro[2]) / 127.0f * 0.6f, 0.6f,
                       0.15f + (float)(tr->macro[3]) / 127.0f * 0.6f};
        float sus = 0.25f + 0.6f * (float)tr->sustain / 127.0f;
        float lv[4] = {1.0f, sus, sus, 0.0f};
        env_shape(g, 4, 40, 232, 88, tm, lv, 0.0f, col);
        return;
    }
    case BXP_LFO: {
        if (!gfx_strip_hit(g, 40, 89)) return;
        int wave = a->pat.fmv[142] % 6, sp = a->pat.fmv[137];
        gfx_rect(g, 4, 40, 232, 88, C_DDGREY);
        gfx_hline(g, 6, 84, 228, C_DDGREY);
        int cyc = 1 + sp / 25, py = 84;
        uint32_t r = 12345u;
        for (int x = 0; x < 228; x++) {
            float ph = (float)(x * cyc % 228) / 228.0f, v;
            switch (wave) {
            case 0: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
            case 1: v = 1.0f - 2.0f * ph; break;
            case 2: v = 2.0f * ph - 1.0f; break;
            case 3: v = ph < 0.5f ? 1.0f : -1.0f; break;
            case 4: v = sinf(ph * 6.2831853f); break;
            default: if (x * cyc % 228 < cyc) r = r * 1103515245u + 12345u; v = (float)((r >> 16) & 255) / 127.5f - 1.0f; break;
            }
            int y = 84 - (int)(v * 38.0f);
            if (x) gfx_line(g, 5 + x, py, 6 + x, y, col);
            py = y;
        }
        return;
    }
    case BXP_FX: {                                                /* las seis casillas en el orden de la cadena */
        for (int i = 0; i < BXF_N; i++) {
            int x = 4 + (i % 3) * 78, y = 40 + (i / 3) * 44;
            if (!gfx_strip_hit(g, y, 40)) continue;
            int f = a->pat.prm[t][BX_ORD0 + i] % BXF_N;
            bool on = (a->pat.prm[t][BX_FXON] >> f) & 1, sel = i == u->fxpos;
            if (on) gfx_fill(g, x, y, 74, 38, dim(col));
            gfx_rect(g, x, y, 74, 38, sel ? (u->grab ? C_YELLOW : C_WHITE) : C_DGREY);
            if (sel) gfx_rect(g, x + 1, y + 1, 72, 36, u->grab ? C_YELLOW : C_WHITE);
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_small(g, x + 4, y + 4, b, C_GREY);
            gfx_text_c(g, x + 37, y + 14, T(bx7_fx_names[f]), on ? C_WHITE : C_GREY, 1);
            gfx_small(g, x + 37 - gfx_small_w(on ? "ON" : "OFF") / 2, y + 28, on ? "ON" : "OFF", on ? C_ORANGE : C_DGREY);
        }
        return;
    }
    case BXP_EDIT:
        if (u->edit == BXE_PIT) {                                 /* el pitch EG: 4 tiempos y 4 niveles */
            const uint8_t *v = a->pat.fmv + 126;
            float tm[4], lv[4];
            for (int i = 0; i < 4; i++) { tm[i] = 0.1f + (float)(99 - v[i]) / 99.0f; lv[i] = (float)v[4 + i] / 99.0f; }
            env_shape(g, 4, 40, 232, 80, tm, lv, (float)v[7] / 99.0f, C_ORANGE);
        } else draw_fm(g, a, t, 2, 38, 236, 84, u->edit <= BXE_OP6 ? u->edit : -1);
        return;
    case BXP_GLO: case BXP_ARP:
        mach_list(g, a, 40, 38);
        return;
    default:                                                      /* MAIN: el algoritmo */
        draw_fm(g, a, t, 2, 38, 236, 94, -1);
        return;
    }
}

/* ------------------------------------------------------------ el Slicer (V_MACH): TRIM, SOUND, ENV, FX, GLO */
/* La onda de una toma en columnas (pico y RMS, 0..255) de la muestra v0 a la v1, como en un sampler:
 * se calcula solo cuando cambia lo que se ve (la toma se lee directo de la flash) */
#define WV_W 232
typedef struct { const int8_t *d; uint32_t v0, v1; uint8_t rev, pk[WV_W], rms[WV_W]; } wave_cols;

static void wave_fill(wave_cols *w, const bm_smp_src *m, uint32_t v0, uint32_t v1, bool rev)
{
    if (v1 > m->len) v1 = m->len;
    if (w->d == m->data && w->v0 == v0 && w->v1 == v1 && w->rev == rev) return;
    w->d = m->data; w->v0 = v0; w->v1 = v1; w->rev = rev;
    uint32_t span = v1 > v0 ? v1 - v0 : 1, top = 16;
    static uint8_t pk[WV_W]; static uint32_t sq[WV_W];
    for (int c = 0; c < WV_W; c++) {
        uint32_t a0 = v0 + (uint32_t)((uint64_t)span * (uint32_t)c / WV_W), a1 = v0 + (uint32_t)((uint64_t)span * (uint32_t)(c + 1) / WV_W);
        if (a1 <= a0) a1 = a0 + 1;
        uint32_t p = 0, s = 0, n = 0;
        for (uint32_t i = a0; i < a1 && i < v1; i += (a1 - a0) > 64 ? (a1 - a0) / 64 : 1, n++) {   /* hasta 64 muestras por columna */
            int x = m->data[i], ax = x < 0 ? -x : x;
            if ((uint32_t)ax > p) p = (uint32_t)ax;
            s += (uint32_t)(x * x);
        }
        pk[c] = (uint8_t)(p > 127 ? 127 : p);
        sq[c] = n ? s / n : 0;
        if (p > top) top = p;
    }
    for (int c = 0; c < WV_W; c++) {
        int o = rev ? WV_W - 1 - c : c, r = 0;
        while ((uint32_t)((r + 1) * (r + 1)) <= sq[c]) r++;                   /* raíz entera */
        w->pk[o] = (uint8_t)(pk[c] * 255u / top);
        w->rms[o] = (uint8_t)bm_clampi(r * 255 / (int)top, 0, 255);
    }
}

/* la onda espejada: el pico en el tono oscuro y el RMS encima; las columnas [hl0, hl1) en hcol */
static void wave_draw(bm_gfx *g, const wave_cols *w, int y, int h, int hl0, int hl1, uint16_t col, uint16_t hcol)
{
    if (!gfx_strip_hit(g, y, h)) return;
    int mid = y + h / 2, half = h / 2 - 1;
    for (int c = 0; c < WV_W; c++) {
        bool hi = c >= hl0 && c < hl1;
        uint16_t k = hi ? hcol : col;
        int p = w->pk[c] * half / 255, r = w->rms[c] * half / 255;
        if (p > 0) gfx_vline(g, 4 + c, mid - p, 2 * p + 1, dim(k));
        if (r > 0) gfx_vline(g, 4 + c, mid - r, 2 * r + 1, k);
        else gfx_vline(g, 4 + c, mid, 1, dim(k));
    }
}

static wave_cols w_ov, w_main;

/* una fila de perillas dibujadas (las únicas de la pantalla): dial, nombre y valor en su color */
static void slicer_knob_row(bm_gfx *g, const bm_app *a, const bxp *P, int cy, int r)
{
    char b[12];
    for (int k = 0; k < 4; k++) {
        if (!slicer_live(a, &P[k])) continue;             /* sin función ahora: nada */
        int v = slicer_get(a, &P[k]), cx = 30 + k * 60;
        dial(g, cx, cy, r, slicer_pct(a, &P[k], v), 255, bm_knob_color(k), T(P[k].name));
        int vy = cy + r + (strchr(P[k].name, ' ') ? 23 : 14);
        if (gfx_strip_hit(g, vy - 1, 9)) {
            slicer_fmt(a, &P[k], v, b, sizeof b);
            gfx_text_c(g, cx, vy, b, C_ORANGE, 1);
        }
    }
}

/* TRIM: arriba la toma entera (los pads, la ventana); en el medio la ventana grande con el pad entre
 * corchetes; abajo las cuatro perillas */
static void slicer_trim(bm_gfx *g, const bm_app *a)
{
    int sel = a->sel_lane[1] & 15;
    const bm_pad *p = &a->pat.pad[sel];
    const bm_status *st = &a->ctx.st;
    uint16_t col = bm_track_color(1);
    bm_smp_src m;
    char b[8];
    if (!slicer_take(a, sel, &m)) {
        if (gfx_strip_hit(g, 80, 30)) {
            snprintf(b, sizeof b, "P%d", sel + 1);
            gfx_text_c(g, 120, 80, b, col, 2);
            gfx_text_c(g, 120, 100, T("EMPTY"), C_GREY, 1);
        }
        slicer_knob_row(g, a, slicer_knobs(a), 152, 10);
        return;
    }
    uint32_t v0, v1;
    slicer_view(a, &v0, &v1);
    uint32_t span = v1 > v0 ? v1 - v0 : 1, a0 = p->start, a1 = (uint32_t)p->start + p->len;
    if (gfx_strip_hit(g, 38, 10)) {                             /* la toma entera */
        wave_fill(&w_ov, &m, 0, m.len, false);
        int s0 = (int)((uint64_t)a0 * WV_W / m.len), s1 = (int)((uint64_t)a1 * WV_W / m.len);
        wave_draw(g, &w_ov, 38, 10, s0, s1 > s0 ? s1 : s0 + 1, C_DGREY, col);
        int x0 = 4 + (int)((uint64_t)v0 * WV_W / m.len), x1 = 4 + (int)((uint64_t)v1 * WV_W / m.len);
        if (v1 - v0 < m.len) gfx_rect(g, x0, 38, x1 - x0 > 2 ? x1 - x0 : 2, 10, C_WHITE);   /* la ventana de abajo */
    }
    int wy = 52, wh = 84;
#define VX(q) (4 + (int)(((int64_t)(q) - (int64_t)v0) * WV_W / (int64_t)span))
    if (gfx_strip_hit(g, wy, wh + 1)) {
        wave_fill(&w_main, &m, v0, v1, false);
        int bx0 = bm_clampi(VX(a0), 4, 236), bx1 = bm_clampi(VX(a1), 4, 236);
        if (bx1 > bx0) gfx_fill(g, bx0, wy, bx1 - bx0, wh, C_DDGREY);
        gfx_hline(g, 4, wy + wh / 2, WV_W, C_DDGREY);
        wave_draw(g, &w_main, wy, wh, bx0 - 4, bx1 - 4, C_DGREY, col);
        for (int i = 0; i < BM_LANES; i++) {                    /* los otros pads de esta toma: dónde empiezan */
            const bm_pad *o = &a->pat.pad[i];
            if (i == sel || !bm_pad_ok(o) || o->src != p->src || o->start < v0 || o->start >= v1) continue;
            int x = VX(o->start);
            gfx_vline(g, x, wy, wh, C_GREY);
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_small(g, x + 2, wy + wh - 7, b, C_GREY);
        }
        if (p->mode == PAD_LOOP && bm_pad_has_loop(p)) {        /* el loop de sustain */
            int lx0 = bm_clampi(VX(a0 + bm_pad_loop_start(p)), 4, 236), lx1 = bm_clampi(VX(a0 + bm_pad_loop_end(p)), 4, 236);
            gfx_fill(g, lx0, wy + wh - 3, lx1 - lx0 > 1 ? lx1 - lx0 : 1, 3, C_GREEN);
        }
        if (a0 >= v0 && a0 < v1) {                              /* el pad: corchetes y su número */
            int x = VX(a0);
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x, wy, 5, C_CYAN); gfx_hline(g, x, wy + wh - 1, 5, C_CYAN);
            gfx_fill(g, x, wy + 2, 13, 9, C_CYAN);
            snprintf(b, sizeof b, "%d", sel + 1);
            gfx_text(g, x + 1, wy + 3, b, C_BLACK, 1);
        }
        if (a1 > v0 && a1 <= v1) {
            int x = VX(a1) - 1;
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x - 4, wy, 5, C_CYAN); gfx_hline(g, x - 4, wy + wh - 1, 5, C_CYAN);
        }
        if (st->smp_pos_src == p->src && st->smp_pos >= v0 && st->smp_pos < v1) gfx_vline(g, VX(st->smp_pos), wy, wh, C_ORANGE);   /* lo que suena */
    }
#undef VX
    slicer_knob_row(g, a, slicer_knobs(a), 152, 10);
}

/* el pad entero (su tramo de la toma), al derecho o al revés como suena */
static void slicer_pad_wave(bm_gfx *g, const bm_app *a, int y, int h, uint16_t c)
{
    int sel = a->sel_lane[1] & 15;
    const bm_pad *p = &a->pat.pad[sel];
    bm_smp_src m;
    if (!gfx_strip_hit(g, y, h + 1) || !slicer_take(a, sel, &m)) return;
    gfx_rect(g, 2, y, 236, h, C_DDGREY);
    wave_fill(&w_main, &m, p->start, (uint32_t)p->start + p->len, (p->flags & PAD_REV) != 0);
    wave_draw(g, &w_main, y + 1, h - 2, 0, WV_W, C_DGREY, c);
}

static void draw_slicer(bm_gfx *g, const bm_app *a)
{
    uint16_t col = bm_track_color(1);
    switch (a->bx.page) {
    case BXP_ENV: {                                               /* la envolvente sobre el pad, y su loop */
        int sel = a->sel_lane[1] & 15, y = 40, h = 88;
        const bm_pad *p = &a->pat.pad[sel];
        slicer_pad_wave(g, a, y, h, dim(col));
        if (!bm_pad_ok(p) || !gfx_strip_hit(g, y, h + 1)) return;
        uint32_t len = p->len ? p->len : 1;
        int ax = 4 + (int)bm_clampi((int)((uint64_t)bm_attack_ms(p->attack) * BM_SMP_RATE / 1000u * WV_W / len), 0, WV_W);
        int top = y + 6, bot = y + h - 3, end = 4 + WV_W - 1;
        int rx = end - bm_clampi(p->release * 40 / 127, 2, 40);   /* el final se apaga con RELEASE */
        gfx_line(g, 4, bot, ax, top, col);
        gfx_line(g, ax, top, rx, top, col);
        gfx_line(g, rx, top, end, bot, col);
        if (p->mode == PAD_LOOP) {
            uint32_t ls = bm_pad_loop_start(p), le = bm_pad_loop_end(p);
            int lx0 = 4 + (int)((uint64_t)ls * WV_W / len), lx1 = 4 + (int)((uint64_t)le * WV_W / len);
            gfx_fill(g, lx0, bot - 2, lx1 - lx0 > 1 ? lx1 - lx0 : 1, 3, C_GREEN);
            gfx_vline(g, lx0, y + 1, h - 2, C_GREEN); gfx_vline(g, lx1 - 1, y + 1, h - 2, C_GREEN);
        }
        return;
    }
    case BXP_FX: px_fx_boxes(g, a, slicer_sub(a)); return;
    case BXP_GLO: mach_list(g, a, 40, 38); return;
    default: break;
    }
    if (slicer_sub(a) == 0) { slicer_trim(g, a); return; }
    slicer_knob_row(g, a, slicer_knobs(a), 66, 18);              /* SOUND: las cuatro perillas y el pad como suena */
    slicer_pad_wave(g, a, 128, 64, col);
}

/* V_MACH: la máquina del canal; arriba su página y lo que pasa (REC, armado, ACCENT o el sonido) */
static void draw_mach(bm_gfx *g, const bm_app *a)
{
    int t = a->track, eng = a->pat.trk[t].engine;
    uint16_t col = bm_track_color(t);
    char b[30], ti[24];
    if (gfx_strip_hit(g, 25, 11)) {
        mach_title(a, ti, sizeof ti);
        int w = gfx_small_w(ti) + 6;
        gfx_fill(g, 2, 25, w, 9, col);
        gfx_small(g, 5, 27, ti, C_BLACK);
        int L = mach_len(a);
        uint16_t bc = a->rec || a->bx.armed ? C_RED : C_WHITE;
        if (eng == ENG_BREAK) {                                  /* el Slicer: SAMPLE, REC o el pad y su toma */
            slicer_status(a, b, sizeof b, &bc);
            if (a->rec && bc == C_WHITE) { snprintf(b, sizeof b, "%s", T("REC")); bc = C_RED; }
        }
        else if (mach_step_mode(a)) snprintf(b, sizeof b, TF("STEP REC %d/%d"), a->bx.step % L + 1, L);
        else if (a->rec) snprintf(b, sizeof b, "%s", T("REC"));
        else if (a->bx.armed) snprintf(b, sizeof b, "%s", T("ARMED"));
        else if (a->bx.page == BXP_SEQ && L > 16) snprintf(b, sizeof b, TF("PAGE %d/%d"), mach_roll_page(a, t) + 1, (L + 15) / 16);
        else snprintf(b, sizeof b, "%s", bm_sound_name(a, t));
        gfx_small(g, 236 - gfx_small_w(b), 27, b, bc);
    }
    if (eng == ENG_FM) draw_bx7(g, a);
    else if (eng == ENG_ACID) draw_b303(g, a);
    else if (eng == ENG_BREAK) draw_slicer(g, a);
    else draw_panel(g, a);                                       /* B-MINI, BUNO-6, BITTAL */
}

/* ------------------------------------------------------------ entrada */
/* ¿la pantalla ya muestra K1-K4 (un panel con sus perillas o la lista de las cuatro)? Entonces no va
 * la franja de abajo (cada perilla, una sola vez) y el visor llega hasta el teclado. */
static bool knobs_in_view(const bm_app *a)
{
    if (a->layer || a->box_on) return false;
    switch (a->view) {
    case V_GLO: return a->glo_page != 0;
    case V_MACH: return mach_knobs_shown(a);
    default: return false;
    }
}

static void draw_channel(bm_gfx *g, const bm_app *a)
{
    PASO(8);
    draw_seq_tr(g, a);
}

void bm_app_draw(bm_gfx *g, void *user)
{
    const bm_app *a = (const bm_app *)user;
    if (a->view == V_BOOT) {
        gfx_logo(g);
        gfx_text(g, 4, 231, "V" BM_VERSION_STR, C_DGREY, 1);
        return;
    }
    PASO(1);
    draw_status(g, a);
    bool tall = knobs_in_view(a);
    gfx_clip(g, 0, BM_CTX_Y, BM_LCD_W, tall ? BM_CTX_H_TALL : BM_CTX_H);
    PASO(2);
    if (a->layer) draw_layer(g, a);
    else switch (a->view) {
    case V_HOME: PASO(3); draw_home(g, a); break;
    case V_CH: draw_channel(g, a); break;
    case V_GLO: draw_glo(g, a); break;
    case V_SYSTEM: draw_system(g, a); break;
    case V_SAVE: draw_save(g, a); break;
    case V_MACH: draw_mach(g, a); break;
    default: break;
    }
    PASO(24);
    if (a->box_on) draw_box(g, a);
    PASO(25);
    if (a->ov.on) draw_overlay(g, a);
    PASO(27);
    if (a->toast_until) draw_toast(g, a);
    gfx_noclip(g);
    PASO(28);
    draw_keys(g, a);
    PASO(29);
    if (!tall) draw_legend(g, a);
    draw_bigknobs(g, a);
    PASO(0);
    if (a->now < a->clip_until) {
        gfx_fill(g, 0, 0, BM_LCD_W, 2, C_RED); gfx_fill(g, 0, BM_LCD_H - 2, BM_LCD_W, 2, C_RED);
        gfx_fill(g, 0, 0, 2, BM_LCD_H, C_RED); gfx_fill(g, BM_LCD_W - 2, 0, 2, BM_LCD_H, C_RED);
    }
}
