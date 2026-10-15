/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pantallas (docs/DISENO_0.6.md §5). Cuatro franjas en todas:
 *   y 0..23     cabecera: la máquina, PLAY/REC, patrón, BPM, CPU (anillo de la pulsación de 2 s)
 *   y 24..135   visor: en un canal, sus pestañas arriba y la página debajo
 *   y 136..167  teclado: las 11 negras y las 16 blancas (= los 16 pasos) con lo que hace cada una
 *   y 168..239  perillas: K1-K4 con su dial, su valor y su nombre, en su orden físico
 * Todo el texto en pantalla va en ASCII (la fuente no tiene acentos). */
#include <stdio.h>
#include <string.h>
#include "bm_app_int.h"
#include "bm_rom.h"
#include "bm_util.h"
#include "bm_fm.h"
#include "bm_drums.h"
#include "bm_params.h"
#include "bm_snap.h"
#include "bm_wt.h"
#include <math.h>

#define PY   36    /* primera fila útil de una página de canal (debajo de las pestañas) */

/* La miga del dibujo (0.7, temporal): qué parte de la pantalla se estaba dibujando. Si el
 * procesador falla, hal/fm1/sdk/wl82_debug.c la guarda en la huella y la consola la muestra.
 * 0 = fuera del dibujo. */
volatile uint8_t bm_ui_paso;
#define PASO(n) (bm_ui_paso = (uint8_t)(n))
#define VB   135   /* última fila del visor */

static uint16_t dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }
static float bm_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ------------------------------------------------------------ cabecera */
static void draw_ring(bm_gfx *g, int cx, int cy, int p, uint16_t c)
{
    static const int8_t CS[24][2] = {{0,-7},{2,-7},{4,-6},{5,-5},{6,-4},{7,-2},{7,0},{7,2},{6,4},{5,5},{4,6},{2,7},
                                     {0,7},{-2,7},{-4,6},{-5,5},{-6,4},{-7,2},{-7,0},{-7,-2},{-6,-4},{-5,-5},{-4,-6},{-2,-7}};
    int n = p * 24 / 255;
    for (int i = 0; i < 24; i++) gfx_fill(g, cx + CS[i][0], cy + CS[i][1], 2, 2, i < n ? c : C_DGREY);
}

/* El nombre de lo que se toca: BX-1 en HOME, la máquina del canal adentro. */
static const char *machine_title(const bm_app *a, uint16_t *col)
{
    bool ch = a->view == V_CH || (a->layer && a->layer != L_HOME);
    *col = ch ? bm_track_color(a->track) : C_CYAN;
    if (ch) return bm_track_name(a, a->track);
    switch (a->view) {                              /* MIX, FX, TEMPO, ESCENAS y SONG son páginas de BX-1 */
    case V_SYSTEM: return "SYSTEM";
    case V_TIME: return "TIME";
    case V_SAVE: return "SAVE";
    default: return "BX-1";
    }
}

static void draw_status(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 0, BM_STATUS_H)) return;
    const bm_status *st = &a->ctx.st;
    char b[16];
    gfx_fill(g, 0, 0, BM_LCD_W, 23, C_BLACK);
    if (st->playing) for (int i = 0; i < 9; i++) gfx_vline(g, 4 + i, 7 + i / 2, 10 - (i / 2) * 2, C_GREEN);
    else gfx_fill(g, 4, 8, 8, 8, C_GREY);
    bool recording = a->rec || st->smp_rec || a->song_rec || st->smp_armed;
    if (recording && (!(st->smp_rec || a->song_rec || st->smp_armed) || ((a->now / 250) & 1))) gfx_disc(g, 18, 12, 3, st->smp_armed ? C_YELLOW : C_RED);
    uint16_t mc;
    const char *mt = machine_title(a, &mc);
    int tx = 25;
    if (a->view == V_CH) { gfx_text(g, 25, 9, "<", C_GREY, 1); tx = 32; }    /* HOME vuelve a BX-1 */
    gfx_text(g, tx, 5, mt, mc, 2);
    /* patrón, compás y lo que está en cola */
    int x = tx + gfx_text_w(mt, 2) + 6;
    if (x < 102) x = 102;
    snprintf(b, sizeof b, "A%02d", st->pattern + 1);
    gfx_text(g, x, 3, b, C_WHITE, 1);
    uint32_t bt = st->playing ? st->btick : 0;
    snprintf(b, sizeof b, "%u.%u", (unsigned)(bt / BM_BAR_TICKS % 99 + 1), (unsigned)((bt / BM_PPQN) % 4 + 1));
    gfx_small(g, x, 14, b, C_GREY);
    if (st->queued != 0xFF && st->queued != st->pattern) { snprintf(b, sizeof b, ">%02d", st->queued + 1); gfx_small(g, x + 20, 14, b, st->pchain_n > 1 ? C_CYAN : C_YELLOW); }
    else if (st->scene_q != 0xFF) { snprintf(b, sizeof b, ">%c", 'A' + st->scene_q); gfx_small(g, x + 20, 14, b, C_CYAN); }
    else if (st->scene != 0xFF) { snprintf(b, sizeof b, "%c", 'A' + st->scene); gfx_small(g, x + 20, 14, b, C_CYAN); }
    /* BPM */
    int bpm = a->pat.meta.bpm_x10;
    snprintf(b, sizeof b, "%d", bpm / 10);
    gfx_text_r(g, 186, 5, b, C_TEMPO, 2);
    if (bpm % 10) { snprintf(b, sizeof b, ".%d", bpm % 10); gfx_small(g, 187, 4, b, C_TEMPO); }
    gfx_small(g, 187, 14, a->set.clock ? (st->ext_ok ? "EXT" : "EXT?") : "BPM", a->set.clock && !st->ext_ok ? C_RED : C_GREY);
    /* REC: el largo de las notas que se graban · si no, la CPU */
    int hid = -1, p = bm_gesture_progress(&a->ges, a->now, &hid);
    if (p >= 0) draw_ring(g, 210, 12, p, C_YELLOW);
    else if (a->rec && app_kind(a, a->track) == KIND_NOTE) {
        if (a->rec_gate >= 100) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "G%d%%", a->rec_gate);
        gfx_small(g, 202, 4, b, C_GREEN);
        gfx_small(g, 202, 14, "REC", C_RED);
    } else if (a->cpu > 95) gfx_small(g, 202, 14, "CPU!", C_RED);   /* solo si el audio no alcanza */
    if (st->fill && st->playing) gfx_small(g, 202, 4, "FILL", C_ORANGE);
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

static void draw_legend(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_LEGEND_Y, BM_LEGEND_H)) return;
    bm_legend L[4];
    PASO(30);
    bm_app_legend(a, L);
    PASO(29);
    gfx_fill(g, 0, BM_LEGEND_Y, BM_LCD_W, BM_LEGEND_H, C_BLACK);
    gfx_hline(g, 0, BM_LEGEND_Y, BM_LCD_W, C_DDGREY);
    for (int i = 0; i < 4; i++) {
        int x = i * 60, cx = x + 30;
        uint16_t kc = bm_knob_color(i);
        bool hi = a->ov.on && a->ov.enc == i;
        if (i) gfx_vline(g, x, BM_LEGEND_Y + 4, 54, C_DDGREY);
        if (!L[i].name[0]) { draw_dial(g, cx, BM_LEGEND_Y + 19, 0, -1, C_DDGREY, false); continue; }
        if (hi) gfx_rect(g, x + 2, BM_LEGEND_Y + 2, 56, 58, kc);
        draw_dial(g, cx, BM_LEGEND_Y + 19, L[i].pct, -1, kc, hi);
        gfx_text_c(g, cx, BM_LEGEND_Y + 37, L[i].val, L[i].locked ? kc : C_WHITE, 1);
        if (L[i].locked) gfx_fill(g, x + 53, BM_LEGEND_Y + 38, 4, 4, kc);
        char nm[11]; snprintf(nm, sizeof nm, "%.9s", T(L[i].name));
        gfx_text_c(g, cx, BM_LEGEND_Y + 49, nm, kc, 1);
    }
}

/* ------------------------------------------------------------ helpers */
static void header(bm_gfx *g, const char *title, const char *sub, uint16_t col)
{
    gfx_text(g, 6, 28, title, col, 2);
    if (sub) gfx_text_r(g, 234, 32, sub, C_GREY, 1);
    gfx_hline(g, 4, 44, 232, C_DDGREY);
}

/* Las ayudas (0.7): se ven los primeros HINT_MS de cada página y después se van; la franja de teclas
 * y las perillas siguen diciendo qué hace cada cosa. Antes eran ~40 líneas fijas en pantalla. */
#define HINT_MS 4000u
static bool hints_on;
static void hint(bm_gfx *g, int y, const char *s) { if (hints_on && gfx_strip_hit(g, y, 8)) gfx_text(g, 6, y, s, C_GREY, 1); }
static void hint_s(bm_gfx *g, int y, const char *s) { if (hints_on && gfx_strip_hit(g, y, 6)) gfx_small(g, 4, y, s, C_GREY); }

static void hints_update(const bm_app *a)
{
    static uint32_t key = 0xFFFFFFFFu, t0;
    uint32_t k = (uint32_t)a->view | (uint32_t)a->page << 8 | (uint32_t)a->layer << 16 | (uint32_t)a->track << 24
               | (uint32_t)a->ed_page[a->track] << 28 | (uint32_t)a->fx_page << 30;
    if (k != key) { key = k; t0 = a->now; }
    hints_on = a->now - t0 < HINT_MS;
}

/* las páginas de BX-1 (HOME las recorre): la actual en cian, las otras en gris */
static void home_pages(bm_gfx *g, const bm_app *a, const char *sub, uint16_t subc)
{
    if (!gfx_strip_hit(g, 25, 10)) return;
    static const uint8_t V[5] = {V_HOME, V_FXG, V_GLO, V_SCENES, V_SONG};
    static const char *const N[5] = {"MIX", "FX", "TEMPO", "SCENES", "SONG"};
    int x = 4;
    for (int i = 0; i < 5; i++) {
        int w = gfx_small_w(N[i]);
        if (a->view == V[i]) { gfx_fill(g, x - 2, 25, w + 4, 9, C_CYAN); gfx_small(g, x, 27, N[i], C_BLACK); }
        else gfx_small(g, x, 27, N[i], C_GREY);
        x += w + 9;
    }
    if (sub) gfx_small(g, 236 - gfx_small_w(sub), 27, sub, subc);
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

static int src_name(const bm_app *a, int t, char *b, int cap)
{
    if (t == 1) {                                                /* el sampler: cuántos pads tiene el kit */
        int n = 0;
        for (int i = 0; i < BM_LANES; i++) n += bm_pad_ok(&a->pat.pad[i]);
        return n ? snprintf(b, (size_t)cap, TF("%d PADS"), n) : snprintf(b, (size_t)cap, "%s", T("NO KIT"));
    }
    return snprintf(b, (size_t)cap, "%s", bm_sound_name(a, t));
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
 * cada tecla con su nombre corto y el mismo estado que su luz: relleno = encendida, borde claro =
 * media, borde oscuro = tenue, sin borde = apagada. Las blancas son los 16 pasos. */
static void key_cell(bm_gfx *g, const bm_app *a, int x, int y, int w, int h, const bm_keyfn *f, int down, int big)
{
    uint8_t st = down ? LED_ON : f->led;
    if (st == LED_BLINK) st = ((a->now / 250u) & 1u) ? LED_ON : LED_MID;
    uint16_t bg = 0, fg = C_GREY, edge = 0;
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
    char l[6];
    int maxc = big ? (w - 2) / 6 : (w - 1) / 4;
    snprintf(l, sizeof l, "%.*s", maxc < 1 ? 1 : maxc > 5 ? 5 : maxc, T(f->label));   /* traducida y después recortada */
    if (big) gfx_text_c(g, x + w / 2, y + (h - 7) / 2, l, fg, 1);
    else gfx_small(g, x + (w - gfx_small_w(l)) / 2 + 1, y + (h - 5) / 2, l, fg);
}

static void draw_keys(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_KEYS_Y, BM_KEYS_H)) return;
    bm_keyfn K[BM_KEYS];
    PASO(31);
    bm_app_keys(a, K);
    PASO(28);
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
    /* SEQ de un canal de notas: las notas largas (TIE) como una barra continua sobre sus pasos */
    int t = a->track;
    if (a->view == V_CH && a->page == PG_SEQ && app_kind(a, t) == KIND_NOTE && !a->layer) {
        int p0 = a->step_page * 16, len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
        for (int s = p0 - 16; s < p0 + 16; s++) {
            if (s < 0 || s >= len) continue;
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = bm_evc(&a->pat, t, s, k);
                if (bm_event_empty(e)) continue;
                int l = (e->len & BM_LEN_MASK) + ((e->len & BM_LEN_TIE) ? 1 : 0);
                if (l < 2) continue;
                int x0 = (s - p0) * 15, x1 = (s - p0 + l) * 15 - 2;
                x0 = bm_clampi(x0, 0, 239); x1 = bm_clampi(x1, 0, 239);
                if (x1 > x0) gfx_fill(g, x0, BM_KEYS_Y + BM_KEYS_H - 2, x1 - x0, 2, C_GREEN);
            }
        }
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
        gfx_text(g, x + 3, y + 3, bm_lane_name(a, t, i), fg, 1);
        char b[12]; snprintf(b, sizeof b, "%d %s", i + 1, T(bm_drm_short[bm_trk_trc(&a->pat, t)->lane[i].model % DRM_COUNT]));
        gfx_small(g, x + 3, y + rowh - 9, b, fg);
        if (muted) gfx_text(g, x + 46, y + 3, "M", C_RED, 1);
    }
}

/* ------------------------------------------------------------ pestañas del canal */
static void draw_tabs(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 24, 12)) return;
    int t = a->track;
    uint8_t tabs[8];
    int n = bm_app_tabs(a, t, tabs), in = 0;
    for (int i = 0; i < n; i++) if (tabs[i] == a->page) in = 1;
    /* 0.7: el nombre de la página actual y un punto por página (la actual, más larga). SELECT las recorre. */
    uint16_t tc = bm_track_color(t);
    gfx_text(g, 4, 25, bm_page_name(a->page), in ? tc : C_WHITE, 1);
    int x = 236 - (n - 1) * 6 - 12;
    for (int i = 0; i < n; i++) {
        bool cur = tabs[i] == a->page;
        gfx_fill(g, x, 28, cur ? 10 : 3, 3, cur ? tc : C_DGREY);
        x += cur ? 14 : 6;
    }
    gfx_hline(g, 0, 34, BM_LCD_W, C_DDGREY);
}

/* ------------------------------------------------------------ HOME: BX-1, el mezclador */
/* Una fila por canal: la máquina, su secuencia (pulsos para los golpes, bloques para las notas
 * largas), el efecto que más usa y su volumen. SELECT mueve el cursor; SEQ o EDIT entran. */
static void draw_home(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[16];
    home_pages(g, a, a->move ? (a->move < 0 ? "MOVE DARK" : "MOVE BRIGHT") : NULL, C_PINK);
    for (int t = 0; t < BM_TRACKS; t++) {
        int y = 33 + t * 16;
        if (!gfx_strip_hit(g, y, 16)) continue;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        bool sel = a->box_on ? t == a->box_track : t == a->track;
        if (sel) { gfx_fill(g, 0, y, BM_LCD_W, 15, C_DDGREY); gfx_fill(g, 0, y, 2, 15, C_CYAN); }
        snprintf(b, sizeof b, "%d", t + 1);
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

/* FADERS (HOME otra vez): los seis volúmenes y sus picos */
static void draw_mix(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    header(g, "FADERS", "K1-K4 = THE CURSOR TRACK", C_WHITE);
    for (int t = 0; t < BM_TRACKS; t++) {
        int x = 4 + t * 39, y = 48;
        if (!gfx_strip_hit(g, y, 86)) break;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        if (t == a->track) gfx_rect(g, x - 2, y - 2, 37, 88, C_CYAN);
        char b[8]; snprintf(b, sizeof b, "%.6s", bm_track_name(a, t));
        gfx_small(g, x, y, b, col);
        gfx_fill(g, x + 4, y + 8, 12, 60, C_DDGREY);
        int h = tr->level * 60 / 127;
        gfx_fill(g, x + 4, y + 68 - h, 12, h, col);
        gfx_fill(g, x + 20, y + 8, 6, 60, C_DDGREY);
        int pk = st->peak[t] * 60 / 255;
        gfx_fill(g, x + 20, y + 68 - pk, 6, pk, C_GREEN);
        char pn[8];
        if (tr->pan == 64) snprintf(pn, sizeof pn, "C"); else snprintf(pn, sizeof pn, "%c%d", tr->pan < 64 ? 'L' : 'R', tr->pan < 64 ? (64 - tr->pan) * 50 / 64 : (tr->pan - 64) * 50 / 63);
        gfx_small(g, x + 2, y + 72, pn, C_GREY);
        if (tr->mute) gfx_text(g, x + 22, y + 72, "M", C_RED, 1);
        else { snprintf(b, sizeof b, "R%d", tr->rsend * 9 / 127); gfx_small(g, x + 18, y + 72, b, C_GREY); }
    }
}

/* ------------------------------------------------------------ SEQ: rejilla TR */
/* Batería: la sección que tocan las negras 1-8 (instrumentos 1-8 o 9-16, la negra 9 cambia). */
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
            if (bm_is_pads(t)) {
                char nm[40];
                const bm_pad *pd = &a->pat.pad[sel];
                snprintf(b, sizeof b, TF("PAD %d"), sel + 1);
                int x = gfx_text(g, 4, PY, b, C_WHITE, 1) + 8;
                if (bm_pad_ok(pd)) { bm_app_src_name(a, pd->src, nm, sizeof nm); gfx_small(g, x, PY + 1, nm, C_GREY); }
                else gfx_small(g, x, PY + 1, "EMPTY", C_DGREY);
            } else gfx_text(g, 4, PY, bm_lane_name(a, t, sel), C_WHITE, 1);
            int len = tr->lane[sel].len;
            snprintf(b, sizeof b, "%s  P%d/%d L%d", base ? "9-16" : "1-8", a->step_page + 1, (len + 15) / 16, len);
            gfx_text_r(g, 236, PY, b, C_GREY, 1);
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

/* SEQ de un canal de notas: piano roll de la página con el largo de cada nota (las ligadas en
 * verde menta, como una sola nota larga), y los compases del canal. */
static void draw_seq_syn(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int t = a->track;
    const bm_track *trk = &a->pat.trk[t];
    uint16_t col = bm_track_color(t);
    char b[30], c2[12];
    int len = trk->length ? trk->length : 16;
    int ps = bm_app_track_step(a, t, -1);
    int spb = BM_BAR_TICKS / bm_speed_tps[trk->speed % SPD_COUNT];
    if (gfx_strip_hit(g, PY, 9)) {
        if (a->held_step >= 0 && a->held_long) snprintf(c2, sizeof c2, TF("STEP %02d"), a->held_step + 1);
        else bm_note_str(a->last_note[t], c2, sizeof c2);
        gfx_text(g, 4, PY, c2, C_WHITE, 1);
        if (a->rec_free == t + 1) {
            uint32_t el = a->rec_free_t0 == 0xFFFFFFFFu ? 0 : st->btick - a->rec_free_t0;
            snprintf(b, sizeof b, TF("FREE REC %u.%u"), (unsigned)(el / BM_BAR_TICKS + 1), (unsigned)(el / BM_PPQN % 4 + 1));
            gfx_fill(g, 52, PY - 1, gfx_text_w(b, 1) + 4, 9, C_RED);
            gfx_text(g, 54, PY, b, C_BLACK, 1);
        } else if (a->note_oct) { snprintf(b, sizeof b, TF("BLACKS %+d"), a->note_oct); gfx_small(g, 56, PY + 1, b, C_GREY); }
        if (len % spb == 0) snprintf(b, sizeof b, TF("%d BARS %s"), len / spb, bm_speed_names[trk->speed % SPD_COUNT]);
        else snprintf(b, sizeof b, "L%d %s", len, bm_speed_names[trk->speed % SPD_COUNT]);
        gfx_text_r(g, 236, PY, b, C_GREY, 1);
    }
    int p0 = a->step_page * 16, lo = 127, hi = 0;
    for (int s = 0; s < 16 && p0 + s < len; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, p0 + s, k);
            if (bm_event_empty(e)) continue;
            if (e->note < lo) lo = e->note;
            if (e->note > hi) hi = e->note;
        }
    if (lo > hi) { lo = a->last_note[t] - 6; hi = a->last_note[t] + 6; }
    while (hi - lo < 12) { hi++; if (hi - lo < 12) lo--; }
    if (hi - lo > 24) lo = hi - 24;
    int R = hi - lo + 1, ry = PY + 10, rh = 74, row = rh / R;
    if (row < 2) row = 2;
    rh = row * R;
    if (gfx_strip_hit(g, ry, rh + 2)) {
        for (int s = 0; s < 16; s++) {                           /* columnas = las 16 blancas */
            int ab = p0 + s, x = s * 15;
            gfx_fill(g, x, ry, 15, rh, ab == ps ? C_DDGREY : C_BLACK);
            if (ab >= len) { gfx_fill(g, x + 6, ry + rh / 2, 2, 2, C_DDGREY); continue; }
            if (s % 4 == 0) gfx_vline(g, x, ry, rh, s ? C_DGREY : C_GREY);
            else for (int y = ry; y < ry + rh; y += 4) gfx_fill(g, x, y, 1, 1, C_DDGREY);
            if (ab == ps) gfx_fill(g, x, ry, 14, 2, C_ORANGE);
        }
        for (int n = lo; n <= hi; n++)
            if (n % 12 == 0) gfx_hline(g, 0, ry + (hi - n) * row + row / 2, 240, C_DGREY);
        for (int s = -16; s < 16; s++) {
            int ab = p0 + s;
            if (ab < 0 || ab >= len) continue;
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = bm_evc(&a->pat, t, ab, k);
                if (bm_event_empty(e) || e->note < lo || e->note > hi) continue;
                int l = e->len & BM_LEN_MASK;
                if (l < 1) l = 1;
                bool tie = (e->len & BM_LEN_TIE) || l > 1;
                int x0 = s * 15 + e->micro * 15 / 24, x1 = (s + l) * 15 - 2;
                if (e->len & BM_LEN_TIE) x1 += 3;
                else if (e->gate) x1 = (s + l - 1) * 15 + 15 * e->gate / 100 - 1;
                if (x1 <= 0) continue;
                if (x0 < 0) x0 = 0;
                if (x1 > 239) x1 = 239;
                int y = ry + (hi - e->note) * row;
                uint16_t c = e->vel >= BM_VEL_ACC ? C_WHITE : tie ? C_GREEN : col;
                if (e->vel <= BM_VEL_GHOST) gfx_rect(g, x0, y, x1 - x0, row, c);
                else gfx_fill(g, x0, y, x1 - x0, row - (row > 3), c);
                if (s >= 0) gfx_fill(g, x0, y, 2, row, C_WHITE);
                bool wk = e->prob < 100 || e->prob == BM_PROB_FILL || e->prob == BM_PROB_NOFILL;
                if (wk && s >= 0) gfx_fill(g, x0 + 3, y, 2, row, C_BLACK);
            }
        }
        if (a->held_step >= 0 && a->held_long && a->held_step / 16 == a->step_page) gfx_rect(g, (a->held_step % 16) * 15 - 1, ry, 16, rh, C_YELLOW);
        if (a->rec && a->rec_step >= 0 && a->rec_step / 16 == a->step_page) gfx_rect(g, (a->rec_step % 16) * 15, ry, 15, rh, C_RED);
        snprintf(b, sizeof b, "%s", bm_note_names[hi % 12]);
        gfx_small(g, 236 - gfx_small_w(b), ry + 1, b, C_GREY);
        bm_note_str(lo, c2, sizeof c2);
        gfx_small(g, 236 - gfx_small_w(c2), ry + rh - 6, c2, C_GREY);
    }
    int oy = ry + rh + 3;
    if (a->held_step >= 0 && a->held_long) {
        if (!gfx_strip_hit(g, oy, VB - oy + 1)) return;
        const bm_event *e = NULL;
        for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, a->held_step, k))) { e = bm_evc(&a->pat, t, a->held_step, k); break; }
        gfx_fill(g, 0, oy, 240, VB - oy + 1, C_DDGREY);
        if (!e) { gfx_small(g, 4, oy + 3, "EMPTY STEP - HOLD + ANOTHER WHITE = LONG NOTE", C_GREY); return; }
        int nn = 0; char notes[24] = "";
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *x = bm_evc(&a->pat, t, a->held_step, k);
            if (bm_event_empty(x)) continue;
            char nb[6]; bm_note_str(x->note, nb, sizeof nb);
            size_t l = strlen(notes);
            snprintf(notes + l, sizeof notes - l, "%s%s", nn++ ? " " : "", nb);
        }
        gfx_small(g, 4, oy + 3, notes, C_WHITE);
        if (e->len & BM_LEN_TIE) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, TF("%d STEPS"), e->len & BM_LEN_MASK);
        gfx_small(g, 120, oy + 3, b, C_GREEN);
        if (e->prob == BM_PROB_FILL) snprintf(b, sizeof b, "FILL"); else if (e->prob == BM_PROB_NOFILL) snprintf(b, sizeof b, "NO FILL"); else snprintf(b, sizeof b, "%d%%", e->prob);
        gfx_small(g, 180, oy + 3, b, C_YELLOW);
        gfx_small(g, 4, oy + 11, "SELECT HIT - ALGORITHM PROB - PRESETS LENGTH", C_GREY);
        return;
    }
    if (gfx_strip_hit(g, oy, VB - oy + 1)) {                    /* los compases del canal y el seguidor */
        int bars = (len + spb - 1) / spb;
        if (bars < 1) bars = 1;
        int bw = 232 / bars;
        for (int k = 0; k < bars; k++) {
            int x = 4 + k * bw;
            bool cur = a->step_page * 16 / spb == k || (a->step_page * 16 + 15) / spb == k;
            gfx_rect(g, x, oy, bw - 2, 10, cur ? col : C_DGREY);
            snprintf(b, sizeof b, "%d", k + 1);
            gfx_small(g, x + 2, oy + 3, b, cur ? C_WHITE : C_GREY);
        }
        if (ps >= 0) { int px = 4 + ps * (bw * bars) / len; gfx_fill(g, px, oy + 11, 2, 4, C_ORANGE); }
    }
}

/* ------------------------------------------------------------ SONIDO */
/* BB-303: dos ciclos de la forma de onda, y el filtro como una curva. */
static void draw_synth(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const bm_track *tr = &a->pat.trk[t];
    gfx_rect(g, x0, y0, w, h, C_DDGREY);
    grid(g, x0 + 2, y0 + 2, w - 4, h - 4, 8);
    gfx_hline(g, x0 + 1, y0 + h / 2, w - 2, C_DDGREY);
    int sq = tr->source & 1;
    int py = -1, cw = (w - 4) / 2;
    for (int x = 0; x < w - 4; x++) {
        int ph = (x % cw) * 256 / cw, s = sq ? (ph < 128 ? 100 : -100) : 127 - ph;
        int y = y0 + h / 2 - s * (h / 2 - 6) / 128;
        if (py >= 0) gfx_line(g, x0 + 1 + x, py, x0 + 2 + x, y, C_CYAN);
        py = y;
    }
    int fx = x0 + 4 + tr->macro[0] * (w - 30) / 127, peak = tr->macro[1] * 14 / 127;
    for (int x = x0 + 4; x < x0 + w - 4; x++) {
        int y = x < fx - 12 ? y0 + 8 : x < fx ? y0 + 8 - peak * (x - fx + 12) / 12 : y0 + 8 + (x - fx) * 2 / 3;
        if (y < y0 + h - 2) gfx_fill(g, x, bm_clampi(y, y0 + 1, y0 + h - 2), 1, 2, C_ORANGE);
    }
}

/* BX-7: la topología del algoritmo como en una DX7: portadoras abajo, moduladores encima, la
 * realimentación como un lazo. Cada operador con su nivel y su relación de frecuencia. */
static void draw_fm(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    uint8_t p[BM_FM_PATCH];
    bm_fm_patch_of(&a->pat.trk[t], NULL, p);
    int alg = p[BM_FM_ALG] & 31;
    uint8_t car = bm_fm_carriers(alg), mods[6];
    int8_t fb[2];
    bm_fm_alg_graph(alg, mods, fb);
    if (!gfx_strip_hit(g, y0, h + 1)) return;
    gfx_rect(g, x0, y0, w, h, C_DDGREY);
    grid(g, x0 + 2, y0 + 2, w - 4, h - 4, 8);
    /* profundidad: las portadoras en 0; un modulador, uno más que su destino */
    int depth[6] = {0, 0, 0, 0, 0, 0}, maxd = 0;
    for (int it = 0; it < 6; it++)
        for (int i = 0; i < 6; i++)
            for (int j = 0; j < 6; j++)
                if (mods[j] & (1u << i)) if (depth[i] < depth[j] + 1) depth[i] = depth[j] + 1;
    for (int i = 0; i < 6; i++) if (depth[i] > maxd) maxd = depth[i];
    /* columnas: las portadoras de izquierda a derecha (OP1 primero), sus cadenas encima */
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
                int sib = 0;                                       /* hermanos ya puestos sobre el mismo destino */
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
    int bx[6], by[6];
    for (int i = 0; i < 6; i++) {
        bx[i] = x0 + 4 + (int)(cx[i] * cw + cw / 2.0f) - 9;
        by[i] = y0 + h - 18 - depth[i] * rh - 12;
    }
    uint16_t tc = bm_track_color(t);
    for (int j = 0; j < 6; j++)                                   /* conexiones */
        for (int i = 0; i < 6; i++)
            if (mods[j] & (1u << i)) gfx_line(g, bx[i] + 9, by[i] + 12, bx[j] + 9, by[j], C_GREY);
    for (int i = 0; i < 6; i++) if ((car >> i) & 1) gfx_vline(g, bx[i] + 9, by[i] + 12, y0 + h - 14 - by[i] - 12, C_GREY);
    gfx_hline(g, x0 + 4, y0 + h - 14, w - 8, C_DGREY);
    if (fb[0] >= 0 && fb[1] >= 0) {                               /* la realimentación */
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
        gfx_fill(g, bx[i], by[i] + 12 - lv * 12 / 16, 2, lv * 12 / 16, C_WHITE);        /* nivel */
        snprintf(b, sizeof b, "%d", 6 - i);
        gfx_text(g, bx[i] + 7, by[i] + 3, b, c ? C_BLACK : C_WHITE, 1);
    }
    snprintf(b, sizeof b, "ALG %d", alg + 1);
    gfx_text(g, x0 + 4, y0 + h - 10, b, C_ORANGE, 1);
    snprintf(b, sizeof b, "FB %d", p[BM_FM_FB]);
    gfx_small(g, x0 + 56, y0 + h - 9, b, C_ORANGE);
    for (int i = 0; i < 6; i++) {                                 /* relaciones de frecuencia, OP1..OP6 */
        const uint8_t *o = p + (5 - i) * BM_FM_OP;
        int fc = o[FMO_FC], ff = o[FMO_FF];
        if (o[FMO_MODE]) snprintf(b, sizeof b, "FX");
        else { int r100 = (fc ? fc * 100 : 50) * (100 + ff) / 100; if (r100 % 100) snprintf(b, sizeof b, "%d.%d", r100 / 100, r100 % 100 / 10); else snprintf(b, sizeof b, "%d", r100 / 100); }
        gfx_small(g, x0 + 86 + i * 24, y0 + h - 9, b, C_GREY);
    }
}

/* La onda espejada, como en un sampler: el pico en el tono oscuro y el RMS encima, a todo color. */
static void draw_wave(bm_gfx *g, const uint8_t wave[2][232], int x0, int y, int w, int h, int c0, int c1,
                      int hl0, int hl1, uint16_t col, uint16_t hcol)
{
    int mid = y + h / 2, half = h / 2 - 1;
    for (int x = 0; x < w - 2; x++) {
        int c = c0 + (c1 - c0) * x / (w - 2);
        if (c < 0 || c >= 232) continue;
        bool hi = c >= hl0 && c < hl1;
        uint16_t k = hi ? hcol : col;
        int p = wave[0][c] * half / 255, r = wave[1][c] * half / 255;
        if (p > 0) gfx_vline(g, x0 + 1 + x, mid - p, 2 * p + 1, hi ? C_GREY : dim(k));
        if (r > 0) gfx_vline(g, x0 + 1 + x, mid - r, 2 * r + 1, k);
        else gfx_vline(g, x0 + 1 + x, mid, 1, dim(k));
    }
}

/* ------------------------------------------------------------ BP-1: sampler de pads (T2) */
static void take_map(bm_gfx *g, const bm_app *a, int src, uint32_t len, int y, int h, int hl)
{
    if (!gfx_strip_hit(g, y, h + 1) || !len) return;
    uint16_t col = bm_track_color(1);
    const bm_pad *ph = hl >= 0 ? &a->pat.pad[hl] : NULL;
    int s0 = ph ? (int)((uint64_t)ph->start * 232u / len) : -1, s1 = ph ? (int)((uint64_t)(ph->start + ph->len) * 232u / len) : -1;
    gfx_fill(g, 4, y, 232, h, C_DDGREY);
    for (int x = 0; x < 232; x++) {
        int p = a->wave_ov[x] * (h / 2 - 1) / 255;
        gfx_vline(g, 4 + x, y + h / 2 - p, 2 * p + 1, x >= s0 && x < s1 ? col : C_GREY);
    }
    for (int i = 0; i < BM_LANES; i++) {
        const bm_pad *p = &a->pat.pad[i];
        if (!bm_pad_ok(p) || p->src != src) continue;
        int x0 = 4 + (int)((uint64_t)p->start * 231u / len), x1 = 4 + (int)((uint64_t)(p->start + p->len) * 231u / len);
        gfx_hline(g, x0, y, x1 - x0 > 1 ? x1 - x0 : 1, i == hl ? C_WHITE : dim(col));
        gfx_vline(g, x0, y, 3, i == hl ? C_WHITE : C_GREY);
    }
    if (a->cut_v1 > a->cut_v0 && a->cut_v1 - a->cut_v0 < len) {
        int v0 = (int)((uint64_t)a->cut_v0 * 232u / len), v1 = (int)((uint64_t)a->cut_v1 * 232u / len);
        gfx_rect(g, 4 + v0, y, v1 - v0 > 2 ? v1 - v0 : 2, h, C_WHITE);
    }
    const bm_status *st = &a->ctx.st;
    if (st->smp_pos_src == src && st->smp_pos < len) gfx_vline(g, 4 + (int)((uint64_t)st->smp_pos * 231u / len), y, h, C_ORANGE);
}

/* PADS: el editor del pad elegido, como un KO II o un OP-1 field. Arriba el mapa de su toma; en
 * el medio la onda grande con el pad entre corchetes cian y su loop (sustain) en verde menta. */
static void draw_pads(bm_gfx *g, const bm_app *a)
{
    PASO(12);
    int sel = a->sel_lane[1] & 15, page = a->ed_page[1] % 4;
    const bm_pad *p = &a->pat.pad[sel];
    const bm_status *st = &a->ctx.st;
    uint16_t col = bm_track_color(1);
    char b[40];
    bool ok = bm_pad_ok(p) && a->cut_wlen;
    uint32_t len = a->cut_wlen;
    if (gfx_strip_hit(g, PY, 9)) {
        snprintf(b, sizeof b, TF("PAD %d"), sel + 1);
        int x = gfx_text(g, 4, PY, b, C_WHITE, 1) + 10;
        if (ok) { bm_app_src_name(a, p->src, b, sizeof b); gfx_small(g, x, PY + 1, b, C_GREY); }
        static const char *const PG[4] = {N_("TRIM"), N_("SOUND"), N_("ENV"), N_("LOOP")};
        int xr = 236;
        for (int i = 3; i >= 0; i--) {
            xr -= gfx_small_w(PG[i]);
            gfx_small(g, xr, PY + 1, PG[i], i == page ? C_ORANGE : C_DGREY);
            xr -= 6;
        }
    }
    if (!ok) {
        if (gfx_strip_hit(g, PY + 24, 40)) {
            gfx_text_c(g, 120, PY + 28, "EMPTY PAD", C_WHITE, 1);
            gfx_small(g, 120 - gfx_small_w("TAKES: N1 = KIT FROM A TAKE") / 2, PY + 44, "TAKES: N1 = KIT FROM A TAKE", C_GREY);
            gfx_small(g, 120 - gfx_small_w("N4 = THE TAKE ON THIS PAD") / 2, PY + 52, "N4 = THE TAKE ON THIS PAD", C_GREY);
        }
        hint_s(g, VB - 6, "WHITE = PLAY AND PICK A PAD - PRESETS = ITS TAKE");
        return;
    }
    take_map(g, a, p->src, len, PY + 10, 9, sel);
    int wy = PY + 21, wh = 58;
    uint32_t v0 = a->cut_v0, v1 = a->cut_v1 > a->cut_v0 ? a->cut_v1 : len, span = v1 - v0;
#define VX(q) (5 + (int)(((int64_t)(q) - (int64_t)v0) * 230 / (int64_t)span))
    if (gfx_strip_hit(g, wy, wh + 1)) {
        uint32_t a0 = p->start, a1 = (uint32_t)p->start + p->len;
        int bx0 = bm_clampi(VX(a0), 4, 236), bx1 = bm_clampi(VX(a1), 4, 236);
        if (bx1 > bx0) gfx_fill(g, bx0, wy, bx1 - bx0, wh, C_DDGREY);
        gfx_hline(g, 4, wy + wh / 2, 232, C_DDGREY);
        int hl0 = (int)(((int64_t)a0 - v0) * 232 / (int64_t)span), hl1 = (int)(((int64_t)a1 - v0) * 232 / (int64_t)span);
        draw_wave(g, a->wave_cut, 4, wy, 232, wh, 0, 232, hl0, hl1, dim(col), C_CYAN);
        if (p->mode == PAD_LOOP && (bm_pad_has_loop(p) || page == 3)) {   /* el loop de sustain */
            uint32_t l0 = p->start + bm_pad_loop_start(p), l1 = p->start + bm_pad_loop_end(p);
            int lx0 = bm_clampi(VX(l0), 4, 236), lx1 = bm_clampi(VX(l1), 4, 236);
            gfx_fill(g, lx0, wy + wh - 3, lx1 - lx0 > 1 ? lx1 - lx0 : 1, 3, C_GREEN);
            gfx_small(g, lx0 + 2, wy + wh - 10, "LOOP", C_GREEN);
        }
        if (a->cut_snap) {
            uint32_t bpm = a->take[p->src].bpm_x10 ? a->take[p->src].bpm_x10 : a->pat.meta.bpm_x10;
            uint32_t snap = (uint32_t)BM_SMP_RATE * (a->cut_snap == 1 ? 150u : 300u) / bpm;
            if (snap && span / snap < 116)
                for (uint32_t q = (v0 + snap - 1) / snap * snap; q < v1; q += snap) gfx_vline(g, VX(q), wy + wh - 4, 4, C_GREY);
        }
        for (int i = 0; i < BM_LANES; i++) {
            const bm_pad *o = &a->pat.pad[i];
            if (i == sel || !bm_pad_ok(o) || o->src != p->src || o->start < v0 || o->start >= v1) continue;
            int x = VX(o->start);
            gfx_vline(g, x, wy, wh, C_DGREY);
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_small(g, x + 2, wy + 2, b, C_GREY);
        }
        if (a0 >= v0 && a0 < v1) {
            int x = VX(a0);
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x, wy, 5, C_CYAN); gfx_hline(g, x, wy + wh - 1, 5, C_CYAN);
            gfx_fill(g, x, wy + 2, 9, 7, C_CYAN);
            snprintf(b, sizeof b, "%d", sel + 1);
            gfx_small(g, x + 2, wy + 3, b, C_BLACK);
        }
        if (a1 > v0 && a1 <= v1) {
            int x = VX(a1) - 1;
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x - 4, wy, 5, C_CYAN); gfx_hline(g, x - 4, wy + wh - 1, 5, C_CYAN);
        }
        if (st->smp_pos_src == p->src && st->smp_pos >= v0 && st->smp_pos < v1) gfx_vline(g, VX(st->smp_pos), wy, wh, C_ORANGE);
    }
#undef VX
    PASO(14);
    if (gfx_strip_hit(g, wy + wh + 2, 6)) {
        unsigned ms = (unsigned)((uint32_t)p->len * 1000u / BM_SMP_RATE);
        snprintf(b, sizeof b, "%s  %+d ST  %u MS%s%s", T(bm_pad_mode_name(p->mode)), p->pitch - 24, ms,
                 (p->flags & PAD_REV) ? "  REV" : "", (p->flags & PAD_TEMPO) ? "  SYNC" : "");
        gfx_small(g, 4, wy + wh + 2, b, C_WHITE);
        snprintf(b, sizeof b, "X%d", 1 << (a->cut_zoom & 3));
        gfx_small(g, 236 - gfx_small_w(b), wy + wh + 2, b, C_GREY);
    }
    PASO(15);
    bool holding = a->pad_key >= 0 && a->keys_down[a->pad_key];
    static const char *const HL[4] = {N_("K1 START - K2 END - EDIT = SOUND, ENV, LOOP"), N_("K3 PLAY: ONESHOT, KEY, THRU OR LOOP"), N_("ATTACK, RELEASE, CHOKE GROUP, TEMPO SYNC"),
                                      N_("HELD: THE LOOP REPEATS - RELEASED: TO THE END")};
    hint_s(g, VB - 6, holding ? "HELD: KEY, THRU AND LOOP KEEP PLAYING" : HL[page]);
}

/* BREAK: la fila YOU (lo que programaste) y la fila PLAYS (lo que BREAK hace sonar este compás). */
static void draw_break(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    const bm_track *tk = &a->pat.trk[1];
    const bm_tr *tr = &a->pat.tr[1];
    uint16_t col = bm_track_color(1);
    char b[30];
    bool on = tk->gen[0] || tk->gen[2] || tk->gen[3];
    if (gfx_strip_hit(g, PY, 9)) gfx_text(g, 4, PY, on ? "THE GRID, VARIED EVERY BAR" : "ALL AT 0: THE GRID AS IS", on ? C_WHITE : C_GREY, 1);
    uint32_t blk = st->gen_blk;
    int cur = st->playing ? (int)(st->tn[1] % 16u) : -1;
    int ty = PY + 18, sy = PY + 46;
    if (gfx_strip_hit(g, ty - 8, 58)) {
        gfx_small(g, 2, ty - 7, "YOU", C_GREY);
        gfx_small(g, 2, sy - 7, "PLAYS", C_GREY);
        for (int k = 0; k < 16; k++) {
            int x = 1 + k * 15, main = -1;
            for (int l = 0; l < BM_LANES && main < 0; l++) {
                int len = tr->lane[l].len ? tr->lane[l].len : 16;
                if (!((tr->mute >> l) & 1) && (tr->st[l][(blk * 16u + (uint32_t)k) % (uint32_t)len] & ST_ON)) main = l;
            }
            uint16_t bg = k == cur ? C_WHITE : main >= 0 ? C_DDGREY : C_BLACK;
            gfx_fill(g, x, ty, 13, 18, bg);
            if (main >= 0) { snprintf(b, sizeof b, "%d", main + 1); gfx_small(g, x + 7 - gfx_small_w(b) / 2, ty + 7, b, k == cur ? C_BLACK : C_GREY); }
            else gfx_fill(g, x + 5, ty + 8, 2, 2, C_DGREY);
            uint8_t pl = st->gen_sl[k], fx = st->gen_fx[k];
            bool changed = pl != 0xFF && pl != main;
            gfx_fill(g, x, sy, 13, 18, k == cur ? C_WHITE : pl != 0xFF ? (changed ? col : C_DDGREY) : C_BLACK);
            uint16_t fg = k == cur || changed ? C_BLACK : C_WHITE;
            if (pl != 0xFF) { snprintf(b, sizeof b, "%d", pl + 1); gfx_small(g, x + 7 - gfx_small_w(b) / 2, sy + 7, b, fg); }
            else if (fx & GEN_SKIP) gfx_text(g, x + 4, sy + 5, "-", C_RED, 1);
            if (fx & GEN_RT_MASK) gfx_fill(g, x + 1, sy + 15, 11, 2, C_ORANGE);
            if (fx & GEN_REV) gfx_small(g, x + 1, sy + 1, "<", fg);
            if (fx & GEN_HALF) gfx_small(g, x + 9, sy + 1, "H", fg);
        }
    }
    hint_s(g, PY + 70, "VARY: OTHER PADS - ANCHOR: THE BEATS STAY");
    hint_s(g, PY + 78, "REPEAT: RETRIGS - BREAK: REVERSE, HALF, SILENCE");
    hint_s(g, PY + 88, "HOLD EDIT + BLACK 4 = CAPTURE INTO THE GRID");
}

/* ------------------------------------------------------------ editor de sonido por páginas */
static float mini_wave(int w, float p)             /* las formas de B-MINI */
{
    float saw = 1.0f - 2.0f * p, tri = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
    switch (w) {
    case MN_WAVE_TRI: return tri;
    case MN_WAVE_SHARK: return 0.5f * (tri + saw);
    case MN_WAVE_SQR: return p < 0.5f ? 1.0f : -1.0f;
    case MN_WAVE_WIDE: return p < 0.3f ? 1.0f : -1.0f;
    case MN_WAVE_NARROW: return p < 0.12f ? 1.0f : -1.0f;
    default: return saw;
    }
}

/* B-MINI: los tres osciladores sumados (el de la página resaltado) · BUNO-6: el DCO con su PWM */
static void ed_wave(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h, int page)
{
    const uint8_t *q = a->pat.prm[t];
    int eng = a->pat.trk[t].engine, py = -1, py2 = -1, mid = y0 + h / 2;
    uint16_t col = C_CYAN;
    gfx_hline(g, x0, mid, w, C_DDGREY);
    static const float RG[5] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
    for (int x = 0; x < w; x++) {
        float p = (float)x * 2.0f / (float)w, y, y2 = 0.0f;
        if (eng == ENG_BUNO) {
            float ph = p - (float)(int)p, pw = 0.5f - 0.46f * (float)a->pat.trk[t].macro[3] / 127.0f * 0.5f;
            y = (q[BN_SAW] ? 1.0f - 2.0f * ph : 0.0f) + (q[BN_PULSE] ? (ph < pw ? 1.0f : -1.0f) : 0.0f);
            float ps = p * 0.5f; ps -= (float)(int)ps;
            y += (float)q[BN_SUB] / 127.0f * (ps < 0.5f ? 1.0f : -1.0f);
            y *= 0.6f;
        } else {
            static const uint8_t W[3] = {MN_W1, MN_W2, MN_W3}, R[3] = {MN_R1, MN_R2, MN_R3}, L[3] = {MN_L1, MN_L2, MN_L3};
            y = 0.0f;
            int sel = page >= 0 && page <= 2 ? page : -1;
            for (int k = 0; k < 3; k++) {
                float pk = p * RG[q[R[k]] % 5] / RG[q[R[0]] % 5]; pk -= (float)(int)pk;
                float v = mini_wave(q[W[k]] % MN_WAVES, pk) * (float)q[L[k]] / 127.0f;
                y += v * 0.45f;
                if (k == sel) y2 = v;
            }
            if (sel < 0) y2 = 0.0f;
        }
        int yy = mid - (int)(bm_clampf(y, -1.2f, 1.2f) * (float)(h / 2 - 3));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, yy, col);
        py = yy;
        if (eng == ENG_MINI && page <= 2) {
            int y1 = mid - (int)(y2 * (float)(h / 2 - 3));
            if (py2 >= 0) gfx_line(g, x0 + x - 1, py2, x0 + x, y1, C_ORANGE);
            py2 = y1;
        }
    }
}

/* BITTAL: la tabla en 2.5D, los frames uno detrás del otro; el de la posición actual en cian */
static void ed_table(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h, int osc)
{
    int slot = osc & 1;
    if (!bm_wt_ready(slot)) { gfx_text_c(g, x0 + w / 2, y0 + h / 2 - 3, "NO TABLE", C_GREY, 1); return; }
    const uint8_t *q = a->pat.prm[t];
    int nf = bm_wt_frames(slot);
    float pos = (float)q[slot ? BT_P2 : BT_P1] / 127.0f + bm_bt_macro(q, a->pat.trk[t].macro, slot ? BTM_POS2 : BTM_POS1) + bm_bt_macro(q, a->pat.trk[t].macro, BTM_POS);
    pos = bm_clampf(pos, 0.0f, 1.0f);
    int curf = (int)(pos * (float)(nf - 1) + 0.5f);
    int dx = 7, dy = 5, ww = w - 8 - dx * (nf - 1), hh = h - 6 - dy * (nf - 1);
    if (hh < 16) hh = 16;
    for (int f = nf - 1; f >= 0; f--) {
        int n;
        const int16_t *d = bm_wt_data(slot, f, 3, &n);              /* 128 muestras para dibujar */
        int ox = x0 + 4 + f * dx, oy = y0 + 3 + (nf - 1 - f) * dy, mid = oy + hh / 2, pyy = -1;
        uint16_t c = f == curf ? C_CYAN : (f < curf ? C_DGREY : dim(C_VIOLET));
        for (int x = 0; x < ww; x++) {
            int yy = mid - (int)d[x * n / ww] * (hh / 2 - 1) / 32768;
            if (pyy >= 0) gfx_line(g, ox + x - 1, pyy, ox + x, yy, c);
            pyy = yy;
        }
    }
    char b[24];
    snprintf(b, sizeof b, "WT FRAME %d/%d", curf + 1, nf);
    gfx_small(g, x0 + 4, y0 + h - 6, b, C_WHITE);
    snprintf(b, sizeof b, "UNISON %d", (q[slot ? BT_U2 : BT_U1] & 3) + 1);
    gfx_small(g, x0 + w - gfx_small_w(b) - 4, y0 + h - 6, b, C_GREY);
}

static float ed_filter_db(int type, float w, float qv)    /* respuesta del filtro, w = f / corte */
{
    float w2 = w * w;
    float d1 = sqrtf((1.0f - w2) * (1.0f - w2) + w2 / (qv * qv));
    float qb = 0.5f + 0.02f * qv, d2 = sqrtf((1.0f - w2) * (1.0f - w2) + w2 / (qb * qb));
    float m;
    switch (type) {
    case 1: m = 1.0f / d1; break;                          /* LP 12 */
    case 2: m = (w / qv) / d1 * 1.5f; break;               /* BP */
    case 3: m = w2 / d1; break;                            /* HP */
    default: m = 1.0f / (d1 * d2); break;                  /* LP 24 y escalera */
    }
    return 20.0f * log10f(m > 1e-6f ? m : 1e-6f);
}

static void ed_filter(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const bm_track *tk = &a->pat.trk[t];
    const uint8_t *q = a->pat.prm[t];
    int eng = tk->engine, type = 0, cut, res;
    float env;
    if (eng == ENG_BITTAL) { type = q[BT_FT] % BT_FILTERS; cut = q[BT_CUT]; res = q[BT_RES]; env = (float)((int)q[BT_FENV] - 64) / 63.0f * 72.0f; if (type == BT_F_LADDER) type = 0; }
    else { cut = tk->macro[0]; res = tk->macro[1]; env = (float)(eng == ENG_MINI ? tk->macro[3] : tk->macro[2]) / 127.0f * 72.0f; }
    if (eng == ENG_BITTAL && q[BT_FT] == BT_F_OFF) { gfx_text_c(g, x0 + w / 2, y0 + h / 2, "FILTER OFF", C_GREY, 1); return; }
    float fc = bm_cut_hz(cut), qv = bm_reso_q(res);
    if (env != 0.0f) {
        float fe = fc * exp2f(env / 12.0f);
        int xa = x0 + (int)((log2f(bm_clampf(fc, 20.0f, 20000.0f) / 20.0f)) / 10.0f * (float)w);
        int xb = x0 + (int)((log2f(bm_clampf(fe, 20.0f, 20000.0f) / 20.0f)) / 10.0f * (float)w);
        gfx_fill(g, xa < xb ? xa : xb, y0 + h - 4, (xa < xb ? xb - xa : xa - xb) + 1, 3, C_GREEN);
    }
    int y0db = y0 + h * 18 / 54;
    gfx_hline(g, x0, y0db, w, C_DDGREY);
    int py = -1;
    for (int x = 0; x < w; x++) {
        float f = 20.0f * exp2f((float)x * 10.0f / (float)w);
        float db = bm_clampf(ed_filter_db(type, f / fc, qv), -36.0f, 18.0f);
        int y = y0 + (int)((18.0f - db) / 54.0f * (float)(h - 6));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, y, C_ORANGE);
        py = y;
    }
    static const float F[3] = {100.0f, 1000.0f, 10000.0f};
    static const char *const N[3] = {"100", "1K", "10K"};
    for (int k = 0; k < 3; k++) {
        int x = x0 + (int)(log2f(F[k] / 20.0f) / 10.0f * (float)w);
        gfx_vline(g, x, y0 + h - 8, 3, C_DGREY);
        gfx_small(g, x - 4, y0 + h - 12, N[k], C_DGREY);
    }
    if (eng == ENG_BUNO && q[BN_HPF]) {                          /* el pasa-altos de BUNO-6 */
        static const float HZ[4] = {0.0f, 120.0f, 250.0f, 520.0f};
        int x = x0 + (int)(log2f(HZ[q[BN_HPF] & 3] / 20.0f) / 10.0f * (float)w);
        gfx_fill(g, x0, y0 + 2, x - x0, 3, C_PINK);
        gfx_small(g, x0 + 2, y0 + 6, "HPF", C_PINK);
    }
}

static void ed_adsr(bm_gfx *g, int x0, int y0, int w, int h, int A, int D, int S, int R)
{
    int a = 2 + A * (w / 4) / 127, d = 2 + D * (w / 4) / 127, r = 2 + R * (w / 4) / 127, s = w - a - d - r - 4;
    int yb = y0 + h - 2, sy = yb - S * (h - 4) / 127;
    int x1 = x0 + a, x2 = x1 + d, x3 = x2 + (s > 8 ? s : 8), x4 = x3 + r;
    for (int o = 0; o < 2; o++) {
        gfx_line(g, x0, yb + o, x1, y0 + 2 + o, bm_knob_color(0));
        gfx_line(g, x1, y0 + 2 + o, x2, sy + o, bm_knob_color(1));
        gfx_line(g, x2, sy + o, x3, sy + o, bm_knob_color(2));
        gfx_line(g, x3, sy + o, x4, yb + o, bm_knob_color(3));
    }
}

static void ed_lfo(bm_gfx *g, int x0, int y0, int w, int h, int shape, int amt, uint16_t col)
{
    int mid = y0 + h / 2, py = -1;
    uint32_t rng = 12345;
    float sh = 0.0f;
    gfx_hline(g, x0, mid, w, C_DDGREY);
    for (int x = 0; x < w; x++) {
        float p = (float)x * 3.0f / (float)w, ph = p - (float)(int)p, y;
        switch (shape) {
        case 0: y = sinf(6.2831853f * ph); break;
        case 1: y = 4.0f * fabsf(ph - 0.5f) - 1.0f; break;
        case 2: y = 1.0f - 2.0f * ph; break;
        case 3: y = ph < 0.5f ? 1.0f : -1.0f; break;
        default:
            if (x == 0 || (int)((float)(x - 1) * 3.0f / (float)w * 2.0f) != (int)(p * 2.0f)) sh = (float)(int32_t)(rng = rng * 1664525u + 1013904223u) / 2147483648.0f;
            y = sh;
            break;
        }
        int yy = mid - (int)(y * (float)(h / 2 - 3) * (0.25f + 0.75f * (float)amt / 127.0f));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, yy, amt ? col : C_DGREY);
        py = yy;
    }
}

/* BITTAL: las 4 macros con su destino y lo que mueven ahora */
static void ed_macros(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const uint8_t *q = a->pat.prm[t];
    char b[24];
    (void)w; (void)h;
    for (int i = 0; i < 4; i++) {
        int y = y0 + 4 + i * 15;
        if (!gfx_strip_hit(g, y, 14)) continue;
        int dst = q[BT_M1D + 2 * i] % BTM_TARGETS, amt = (int)q[BT_M1A + 2 * i] - 64, v = a->pat.trk[t].macro[i];
        snprintf(b, sizeof b, "M%d", i + 1);
        gfx_text(g, x0 + 4, y + 3, b, bm_knob_color(i), 1);
        gfx_text(g, x0 + 24, y + 3, bm_btm_names[dst], dst ? C_WHITE : C_DGREY, 1);
        snprintf(b, sizeof b, "%+d", amt * 100 / 63);
        gfx_small(g, x0 + 100, y + 4, b, C_GREY);
        gfx_fill(g, x0 + 130, y + 4, 90, 5, C_DDGREY);
        gfx_fill(g, x0 + 130, y + 4, v * 90 / 127, 5, bm_knob_color(i));
    }
}

static void draw_editor(bm_gfx *g, const bm_app *a)
{
    int t = a->track, n;
    const bm_ppage *pg = bm_app_ed_pages(a, t, &n);
    if (!pg || !n) return;
    int cur = a->ed_page[t] % n, tr = bm_is_tr(&a->pat, t), eng = a->pat.trk[t].engine;
    const bm_ppage *P = &pg[cur];
    uint16_t col = bm_track_color(t);
    char b[32];
    if (gfx_strip_hit(g, PY, 9)) {
        if (tr) {
            const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->sel_lane[t] & 15];
            snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->sel_lane[t] & 15)), T(bm_drm_names[ln->model % DRM_COUNT]));
        } else snprintf(b, sizeof b, "%s", bm_sound_name(a, t));
        gfx_text(g, 4, PY, b, C_WHITE, 1);
        if (!tr && bm_engine_paged(eng)) {                          /* la huella del snapshot: #A4F9 */
            const uint8_t *s = bm_snap_get(eng, a->pat.trk[t].source);
            if (s) { snprintf(b, sizeof b, "#%04X", (unsigned)(s[8] | s[9] << 8)); gfx_small(g, 236 - gfx_small_w(b), PY + 1, b, C_DGREY); }
        }
    }
    if (gfx_strip_hit(g, PY + 10, 9)) {
        gfx_text(g, 4, PY + 10, P->name, col, 1);
        for (int i = 0; i < n; i++) {
            int x = 236 - (n - i) * 7;
            if (i == cur) gfx_fill(g, x, PY + 11, 5, 5, col); else gfx_rect(g, x, PY + 11, 5, 5, C_DGREY);
        }
    }
    int x0 = 6, y0 = PY + 21, w = 228, h = VB - y0 - 1;
    if (!gfx_strip_hit(g, y0 - 2, h + 4)) return;
    gfx_rect(g, x0 - 2, y0 - 2, w + 4, h + 4, C_DDGREY);
    const uint8_t *q = a->pat.prm[t];
    if (tr) {
        const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->sel_lane[t] & 15];
        int dec = 6 + ln->decay * (w - 12) / 127, mid = y0 + h / 2, py = -1, pyn = -1;
        gfx_hline(g, x0, mid, w, C_DDGREY);
        float hz = (float)bm_drm_base_hz[ln->model % DRM_COUNT] * exp2f((float)(ln->tune - 64) * 0.375f / 12.0f);
        for (int x = 0; x < dec && x < w; x++) {
            float e = expf(-4.0f * (float)x / (float)dec);
            float s = sinf((float)x * 0.12f * hz / 300.0f) * e;
            int yy = mid - (int)(s * (float)(h / 2 - 4)), yn = mid + (int)(s * (float)(h / 2 - 4));
            if (py >= 0) { gfx_line(g, x0 + x - 1, py, x0 + x, yy, col); gfx_line(g, x0 + x - 1, pyn, x0 + x, yn, dim(col)); }
            py = yy; pyn = yn;
        }
        snprintf(b, sizeof b, "%d HZ", (int)(hz + 0.5f));
        gfx_small(g, x0 + w - gfx_small_w(b) - 2, y0 + 2, b, C_GREY);
        return;
    }
    int amp = bm_engine_page_of(eng, EPG_AMP), fen = bm_engine_page_of(eng, EPG_FENV), lfo = bm_engine_page_of(eng, EPG_LFO);
    if (eng == ENG_BITTAL) {
        if (cur <= 3) ed_table(g, a, t, x0, y0, w, h, cur >= 2);
        else if (cur == 4 || cur == 5) ed_filter(g, a, t, x0, y0, w, h);
        else if (cur == amp) ed_adsr(g, x0, y0, w, h, q[BT_AA], q[BT_AD], q[BT_AS], q[BT_AR]);
        else if (cur == fen) ed_adsr(g, x0, y0, w, h, q[BT_MA], q[BT_MD], q[BT_MS], q[BT_MR]);
        else if (cur == lfo) ed_lfo(g, x0, y0, w, h, q[BT_LSHAPE] % BT_LSHAPES, q[BT_LAMT], col);
        else if (cur == 9 || cur == 10) ed_macros(g, a, t, x0, y0, w, h);
        else {
            static const char *const M[WM_MODES] = {"POLY: UP TO 4 VOICES", "MONO: ONE VOICE, RETRIGGERS", "LEGATO: ONE VOICE, TIED"};
            gfx_text_c(g, 120, y0 + h / 2 - 4, M[q[BT_MODE] % WM_MODES], C_WHITE, 1);
        }
    } else if (eng == ENG_MINI) {
        if (cur <= 2) ed_wave(g, a, t, x0, y0, w, h, cur);
        else if (cur == 3 || cur == 4) ed_filter(g, a, t, x0, y0, w, h);
        else if (cur == fen) ed_adsr(g, x0, y0, w, h, q[MN_FA], q[MN_FD], q[MN_FS], q[MN_FR]);
        else if (cur == amp) ed_adsr(g, x0, y0, w, h, q[MN_AA], q[MN_AD], q[MN_AS], q[MN_AR]);
        else if (cur == lfo) ed_lfo(g, x0, y0, w, h, 1, q[MN_LAMT], col);
        else gfx_text_c(g, 120, y0 + h / 2 - 4, q[MN_MODE] ? "LEGATO: TIED NOTES GLIDE" : "MONO: EVERY NOTE RETRIGGERS", C_WHITE, 1);
    } else {                                                        /* BUNO-6 */
        if (cur <= 1) ed_wave(g, a, t, x0, y0, w, h, cur);
        else if (cur <= 3) ed_filter(g, a, t, x0, y0, w, h);
        else if (cur == amp) ed_adsr(g, x0, y0, w, h, q[BN_A], q[BN_D], q[BN_S], q[BN_R]);
        else {
            ed_lfo(g, x0, y0, w, h / 2, 1, 90, col);
            int ch = q[BN_CHORUS] % BN_CHORUSES;
            snprintf(b, sizeof b, "CHORUS %s", bm_bn_chorus_names[ch]);
            gfx_text(g, x0 + 6, y0 + h / 2 + 6, b, ch ? C_PINK : C_GREY, 2);
            if (hints_on) gfx_small(g, x0 + 6, y0 + h - 8, "ALGORITHM = CHORUS - VCA ENV OR GATE", C_GREY);
        }
    }
}

static void draw_sound(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[30];
    if (bm_is_tr(&a->pat, t)) {
        if (gfx_strip_hit(g, PY, 9)) {
            gfx_text(g, 4, PY, "KIT", C_GREY, 1); gfx_text(g, 28, PY, bm_sound_name(a, t), C_WHITE, 1);
            gfx_small(g, 80, PY + 1, "MODELED", C_ORANGE);
            if (hints_on) gfx_small(g, 150, PY + 1, "PRESETS = ANOTHER KIT", C_GREY);
        }
        lane_pads(g, a, t, PY + 10, 22);
        return;
    }
    if (tr->engine == ENG_FM) {
        if (gfx_strip_hit(g, PY, 9)) {
            gfx_text(g, 4, PY, bm_sound_name(a, t), C_WHITE, 1);
            if (hints_on) gfx_small(g, 120, PY + 1, "ALGORITHM = ALG - PRESETS = PATCH", C_GREY);
        }
        draw_fm(g, a, t, 4, PY + 10, 232, VB - PY - 10);
        return;
    }
    if (bm_engine_paged(tr->engine)) { draw_editor(g, a); return; }
    if (gfx_strip_hit(g, PY, 9)) {                                /* BB-303 */
        gfx_text(g, 4, PY, bm_sound_name(a, t), C_WHITE, 1);
        if (hints_on) gfx_small(g, 110, PY + 1, "ALGORITHM = B-MINI - PRESETS = WAVE", C_GREY);
    }
    draw_synth(g, a, t, 4, PY + 10, 232, 70);
    char c0[12];
    bm_fmt_macro(a, t, 0, tr->macro[0], c0, sizeof c0);
    snprintf(b, sizeof b, TF("LP 4 POLE %s  RES %d%%"), c0, tr->macro[1] * 100 / 127);
    hint_s(g, PY + 84, b);
    hint_s(g, PY + 92, "ACCENT OPENS THE FILTER - TIE = SLIDE");
}

static void draw_inst(bm_gfx *g, const bm_app *a) { draw_editor(g, a); }

static void draw_ch_mix(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    static const char *const NM[4] = {"LEVEL", "PAN", "DELAY", "REVERB"};
    const int val[4] = {tr->level, tr->pan, tr->send, tr->rsend};
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 58;
        if (!gfx_strip_hit(g, PY + 2, 76)) break;
        gfx_text(g, x, PY + 2, NM[i], bm_knob_color(i), 1);
        gfx_fill(g, x + 10, PY + 13, 20, 60, C_DDGREY);
        if (i == 1) { int c = x + 20, p = (val[1] - 64) * 10 / 64; gfx_fill(g, p < 0 ? c + p : c, PY + 13, p < 0 ? -p : (p ? p : 1), 60, bm_knob_color(1)); continue; }
        int h = val[i] * 60 / 127;
        gfx_fill(g, x + 10, PY + 73 - h, 20, h, bm_knob_color(i));
    }
    int pk = a->ctx.st.peak[t] * 228 / 255;
    if (gfx_strip_hit(g, PY + 78, 6)) { gfx_fill(g, 6, PY + 78, 228, 4, C_DDGREY); gfx_fill(g, 6, PY + 78, pk, 4, C_GREEN); }
    char b[30];
    if (t) snprintf(b, sizeof b, TF("DUCK %d%% (FX AGAIN: K1)"), tr->duck * 100 / 127); else snprintf(b, sizeof b, "T1 IS THE SIDECHAIN SOURCE");
    hint_s(g, PY + 86, a->mix_page && t ? "K1 = DUCK (T1 SIDECHAIN)" : b);
    hint_s(g, PY + 94, "HOME + FX = DELAY, REVERB AND SIDECHAIN");
}

static void draw_more(bm_gfx *g, const bm_app *a)
{
    int n = bm_app_more_count(a);
    char v[24];
    int first = a->more_row > 6 ? a->more_row - 6 : 0;
    for (int i = 0; i < 7 && first + i < n; i++) {
        const char *nm;
        bm_app_more_row(a, first + i, &nm, v, sizeof v);
        row(g, PY + 1 + i * 12, first + i == a->more_row, nm, v);
    }
    hint_s(g, VB - 6, "SELECT = ROW - ALGORITHM = CHANGE");
}

/* TAKES: las tomas guardadas, con la onda de la marcada. */
static void draw_takes(bm_gfx *g, const bm_app *a)
{
    uint8_t list[BM_TAKES + 8];
    int n = bm_app_sources(a, list);
    int first = a->takes_row > 2 ? a->takes_row - 2 : 0;
    char b[30];
    if (!n && gfx_strip_hit(g, PY + 14, 30)) {
        gfx_text_c(g, 120, PY + 18, "NO TAKES YET", C_WHITE, 1);
        gfx_text_c(g, 120, PY + 30, "REC = RECORD ONE", C_GREY, 1);
    }
    for (int i = 0; i < 3 && first + i < n; i++) {
        int y = PY + i * 12, e = first + i;
        if (!gfx_strip_hit(g, y, 12)) continue;
        uint8_t s = list[e];
        bool sel = e == a->takes_row;
        if (sel) { gfx_fill(g, 0, y, 240, 11, C_DDGREY); gfx_fill(g, 0, y, 2, 11, C_CYAN); }
        bm_app_src_name(a, s, b, sizeof b);
        gfx_text(g, 4, y + 2, b, sel ? C_WHITE : C_GREY, 1);
        if (s & BM_SRC_ROM) continue;
        unsigned ds = (unsigned)(a->take[s].len * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("%d BPM %d BAR %u.%uS"), (a->take[s].bpm_x10 + 5) / 10, a->take[s].bars, ds / 10, ds % 10);
        gfx_small(g, 230 - gfx_small_w(b), y + 3, b, C_GREY);
        int pads = 0;
        for (int k = 0; k < BM_LANES; k++) pads += bm_pad_ok(&a->pat.pad[k]) && a->pat.pad[k].src == s;
        if (pads) gfx_disc(g, 235, y + 5, 2, bm_track_color(1));
        if ((a->take_dirty >> s) & 1) gfx_disc(g, 235, y + 5, 2, C_ORANGE);
    }
    uint8_t src = bm_app_takes_sel(a);
    if (src < BM_TAKES && a->take[src].valid && gfx_strip_hit(g, PY + 38, 28)) {
        int y = PY + 38, h = 26;
        gfx_fill(g, 4, y, 232, h, C_DDGREY);
        for (int x = 0; x < 232; x++) {
            int pk = a->wave_ov[x] * (h / 2 - 1) / 255;
            gfx_vline(g, 4 + x, y + h / 2 - pk, 2 * pk + 1, bm_track_color(1));
        }
        for (int k = 1; k < 16; k++) gfx_vline(g, 4 + k * 232 / 16, y + h - 3, 3, C_GREY);
        const bm_status *st = &a->ctx.st;
        if (st->smp_pos_src == 0 && st->smp_pos < a->take[src].len)
            gfx_vline(g, 4 + (int)((uint64_t)st->smp_pos * 231u / a->take[src].len), y, h, C_ORANGE);
    }
    /* las tomas: hasta 6 s en total, en la zona que comparten con los sonidos */
    uint32_t total = BM_TAKES_MAX_LEN, freeb = bm_takes_free(a->take);
    if (gfx_strip_hit(g, PY + 67, 12)) {
        unsigned fs = (unsigned)(freeb * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("FREE %u.%u S - %d SOUNDS ON TOP"), fs / 10, fs % 10, bm_snaps_user_n());
        gfx_small(g, 4, PY + 67, b, C_GREY);
        uint32_t used = total > freeb ? total - freeb : 0;
        gfx_fill(g, 4, PY + 75, 232, 3, C_DDGREY);
        gfx_fill(g, 4, PY + 75, (int)(used * 232u / total), 3, C_GREY);
    }
    snprintf(b, sizeof b, TF("N4 = THE TAKE ON PAD %d"), (a->sel_lane[1] & 15) + 1);
    hint_s(g, PY + 81, "N1 HITS - N2 16 - N3 8: A KIT FROM THIS TAKE");
    hint_s(g, PY + 89, b);
}

static void draw_rec(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[30];
    uint32_t room = st->smp_rec || st->smp_armed ? (st->smp_cap ? st->smp_cap : BM_SMP_REC_MAX) : bm_app_rec_room(a);
    uint32_t used = st->smp_rec ? st->smp_len : (st->smp_unsaved ? st->smp_len : 0);
    const char *lbl; uint16_t lc, big;
    if (st->smp_rec) { lbl = "REC"; lc = C_RED; big = C_RED; }
    else if (st->smp_armed) { lbl = "ARMED"; lc = C_YELLOW; big = C_WHITE; }
    else if (st->smp_unsaved) { lbl = "UNSAVED"; lc = C_GREEN; big = C_GREEN; }
    else { lbl = "READY"; lc = C_DGREY; big = C_WHITE; }
    if (gfx_strip_hit(g, PY, 12)) {
        int w = gfx_text_w(lbl, 1) + 8;
        gfx_fill(g, 4, PY, w, 11, lc);
        gfx_text(g, 8, PY + 2, lbl, C_BLACK, 1);
        gfx_text_r(g, 236, PY + 2, "SOURCE: MIX", C_GREY, 1);
    }
    uint32_t show = st->smp_unsaved && !st->smp_rec ? used : (room > used ? room - used : 0);
    unsigned ds = (unsigned)(show * 10u / BM_SMP_RATE);
    if (gfx_strip_hit(g, PY + 14, 30)) {
        snprintf(b, sizeof b, "%u.%u", ds / 10, ds % 10);
        int w = gfx_text(g, 8, PY + 14, b, big, 4);
        gfx_text(g, 16 + w, PY + 16, "S", C_GREY, 2);
        gfx_text(g, 16 + w, PY + 32, st->smp_unsaved && !st->smp_rec ? "RECORDED" : st->smp_rec ? "LEFT" : "AVAILABLE", C_GREY, 1);
    }
    if (gfx_strip_hit(g, PY + 46, 6)) {
        unsigned fs = (unsigned)(bm_takes_free(a->take) * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("TAKES: FREE %u.%u OF 6 S"), fs / 10, fs % 10);
        gfx_small(g, 4, PY + 46, b, C_GREY);
        gfx_small(g, 236 - gfx_small_w("MAX 3.0 S PER TAKE"), PY + 46, "MAX 3.0 S PER TAKE", C_GREY);
    }
    int y = PY + 54;
    if (gfx_strip_hit(g, y, 20)) {
        gfx_fill(g, 4, y, 232, 11, C_DDGREY);
        uint32_t cap = BM_SMP_REC_MAX;
        int uw = (int)((uint64_t)used * 232u / cap);
        gfx_fill(g, 4, y, uw, 11, st->smp_rec ? C_RED : st->smp_unsaved ? C_GREEN : C_DGREY);
        uint32_t bar = (uint32_t)BM_SMP_RATE * 2400u / a->pat.meta.bpm_x10;
        for (uint32_t k = 1; k * bar < cap; k++) gfx_vline(g, 4 + (int)(k * bar * 232u / cap), y, 11, C_WHITE);
        int cx = 4 + (int)((uint64_t)room * 232u / cap);
        gfx_fill(g, cx - 1, y - 2, 2, 15, C_YELLOW);
        unsigned bs = (unsigned)(bar * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("1 BAR = %u.%u S"), bs / 10, bs % 10);
        gfx_small(g, 4, y + 13, b, C_GREY);
        gfx_small(g, 236 - gfx_small_w(room < cap ? "LIMIT: MEMORY" : "LIMIT: RECORDING RAM"), y + 13, room < cap ? "LIMIT: MEMORY" : "LIMIT: RECORDING RAM", C_YELLOW);
    }
    if (st->smp_rec) { hint_s(g, PY + 78, "REC = STOP - STOPS BY ITSELF AT THE LIMIT"); hint_s(g, PY + 86, "DELAY AND PUNCH-IN PAUSED WHILE RECORDING"); }
    else if (st->smp_armed) { hint_s(g, PY + 78, "STARTS ON THE BAR - REC AGAIN = CANCEL"); hint_s(g, PY + 86, "KEYS ARE SILENT IN THE RECORDER"); }
    else if (st->smp_unsaved) { hint_s(g, PY + 78, "IT SAVES WHEN YOU STOP - WHITES = LISTEN"); hint_s(g, PY + 86, "HOME = DISCARD - REC = ANOTHER TAKE"); }
    else { hint_s(g, PY + 78, "REC = ARM (K1: BAR OR NOW)"); hint_s(g, PY + 86, "HOLD REC = RECORD WHILE HELD"); }
}

/* ------------------------------------------------------------ ENV, LFO, FX */
static void draw_env(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[24];
    if (gfx_strip_hit(g, PY, 9)) { snprintf(b, sizeof b, TF("PAGE %d/2 (ENV AGAIN)"), a->env_page + 1); gfx_text(g, 4, PY, b, C_GREY, 1); }
    if (gfx_strip_hit(g, PY + 10, 72)) {
        gfx_rect(g, 4, PY + 10, 232, 70, C_DDGREY);
        ed_adsr(g, 8, PY + 14, 224, 62, tr->attack, tr->macro[2], tr->sustain, tr->release);
    }
    if (a->env_page == 1) {
        snprintf(b, sizeof b, TF("ENV > FILTER %d%%"), tr->env_amt * 100 / 127);
        hint_s(g, PY + 84, b);
        snprintf(b, sizeof b, TF("GLIDE %dMS (WITH TIE)"), tr->glide * 500 / 127);
        hint_s(g, PY + 92, b);
    } else hint_s(g, PY + 86, "HOLD ENV = PAINT ACCENTS");
}

static void draw_lfo(bm_gfx *g, const bm_app *a)
{
    (void)a;
    gfx_text_c(g, 120, 56, "LFO", C_WHITE, 3);
    gfx_text_c(g, 120, 84, "B-MINI, BITTAL AND BUNO-6 HAVE IT IN SOUND", C_GREY, 1);
    hint_s(g, 104, "HOLD LFO = PAINT REPEATS (ON NOTES: TIES)");
}

static void draw_fx(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    static const char *const DT[4] = {"1/16", "1/8", "3/16", "1/4"};
    char b[24];
    static const char *const FP[3] = {"DELAY", "REVERB", "MASTER"};
    home_pages(g, a, FP[a->fx_page % 3], C_WHITE);
    if (a->fx_page == 2) {                                       /* MASTER: filtro, BEAT FX y drive */
        static const char *const BF[17] = {"OFF", "STUT4", "STUT8", "STUT16", "STUT32", "REV8", "REV4", "TAPE", "GATE",
                                           "LPF", "HPF", "BIT", "SR", "THROW", "FREEZE", "KILL LO", "KILL HI"};
        char f[16];
        if (m->mfilt >= 60 && m->mfilt <= 68) snprintf(f, sizeof f, "OPEN");
        else if (m->mfilt < 60) snprintf(f, sizeof f, "LP %d", m->mfilt);
        else snprintf(f, sizeof f, "HP %d", m->mfilt - 64);
        gfx_rect(g, 4, 48, 112, 56, C_CYAN);
        gfx_text(g, 8, 52, "FILTER", C_CYAN, 1);
        gfx_text(g, 8, 66, f, C_WHITE, 2);
        gfx_rect(g, 122, 48, 114, 56, C_ORANGE);
        gfx_text(g, 126, 52, "BEAT FX", C_ORANGE, 1);
        gfx_text(g, 126, 66, BF[m->beatfx % 17], m->beatfx ? C_PINK : C_GREY, 2);
        snprintf(b, sizeof b, "DRIVE %d%%", a->set.drive * 100 / 127);
        gfx_small(g, 6, 110, b, C_GREEN);
        hint_s(g, 120, "FX AGAIN = DELAY > REVERB > MASTER");
        return;
    }
    gfx_rect(g, 4, 48, 74, 56, a->fx_page ? C_DGREY : C_CYAN);
    gfx_text(g, 8, 52, "DELAY", C_CYAN, 1);
    gfx_text(g, 8, 64, DT[m->dly_time & 3], C_WHITE, 2);
    snprintf(b, sizeof b, "FB %d%%", m->dly_fb * 100 / 127); gfx_small(g, 8, 84, b, C_ORANGE);
    snprintf(b, sizeof b, "MIX %d%%", m->dly_mix * 100 / 127); gfx_small(g, 8, 92, b, C_GREEN);
    gfx_rect(g, 82, 48, 74, 56, a->fx_page ? C_PINK : C_DGREY);
    gfx_text(g, 86, 52, "REVERB", C_PINK, 1);
    snprintf(b, sizeof b, "%d%%", m->rv_size * 100 / 127); gfx_text(g, 86, 64, b, C_WHITE, 2);
    snprintf(b, sizeof b, "DAMP %d%%", m->rv_damp * 100 / 127); gfx_small(g, 86, 84, b, C_ORANGE);
    snprintf(b, sizeof b, "MIX %d%%", m->rv_mix * 100 / 127); gfx_small(g, 86, 92, b, C_GREEN);
    gfx_rect(g, 160, 48, 76, 56, C_DGREY);
    gfx_text(g, 164, 52, "DUCK", C_ORANGE, 1);
    for (int t = 1; t < BM_TRACKS; t++) {
        int x = 164 + (t - 1) * 14;
        gfx_fill(g, x, 66, 10, 30, C_DDGREY);
        int h = a->pat.trk[t].duck * 30 / 127;
        gfx_fill(g, x, 96 - h, 10, h, bm_track_color(t));
    }
    if (a->ctx.arena_busy) { gfx_fill(g, 4, 48, 74, 56, C_DDGREY); gfx_small(g, 8, 70, "PAUSED: TAKE", C_ORANGE); gfx_small(g, 8, 78, "NOT SAVED YET", C_ORANGE); }
    hint_s(g, 112, "FX AGAIN = DELAY > REVERB > MASTER");
    hint_s(g, 120, "HOLD FX = PUNCH-IN");
}

/* ------------------------------------------------------------ escala y acordes */
static void draw_keyboard(bm_gfx *g, const bm_app *a, int ky, int kh)
{
    int t = a->track, root, sc = app_scale(a, t, &root);
    int base_oct = (t == 2 ? 2 : 3) + a->oct;
    char b[12];
    if (!gfx_strip_hit(g, ky, kh + 2)) return;
    int n = bm_scale_len(sc);
    for (int i = 0; i < BM_WHITES; i++) {
        int x = i * 15, k = bm_white_key(i);
        uint16_t c = (i % n == 0) ? C_ORANGE : C_WHITE;
        if (a->keys_down[k]) c = bm_track_color(t);
        gfx_fill(g, x, ky, 14, kh, c);
        int note = bm_scale_note(root, sc, i, base_oct);
        snprintf(b, sizeof b, "%s", bm_note_names[note % 12]);
        gfx_small(g, x + 7 - gfx_small_w(b) / 2, ky + kh - 14, b, C_BLACK);
        snprintf(b, sizeof b, "%d", note / 12 - 1);
        gfx_small(g, x + 6, ky + kh - 7, b, C_DGREY);
    }
    for (int i = 0; i < BM_BLACKS; i++) {
        int k = bm_black_key(i), wi = bm_key_white_index(k - 1), x = wi * 15 + 9;
        uint16_t c = a->keys_down[k] ? bm_track_color(t) : C_DDGREY;
        gfx_fill(g, x, ky, 11, kh * 6 / 10, c);
        gfx_rect(g, x, ky, 11, kh * 6 / 10, C_BLACK);
        if (i < 7) {
            char nm[10];
            bm_chord_name(root, sc, i, 0, nm, sizeof nm);
            nm[3] = 0;
            gfx_small(g, x + 1, ky + kh * 6 / 10 - 8, nm, a->keys_down[k] ? C_BLACK : C_WHITE);
        } else {
            static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
            gfx_small(g, x + 2, ky + kh * 6 / 10 - 8, MODS[i - 7], a->keys_down[k] ? C_BLACK : C_YELLOW);
        }
    }
}

static void draw_scl(bm_gfx *g, const bm_app *a, int project)
{
    const bm_meta *m = &a->pat.meta;
    const bm_track *tr = &a->pat.trk[a->track];
    char b[30];
    int root = m->root, sc = m->scale;
    if (!project && tr->scl != 0xFF) { root = tr->scl_root; sc = tr->scl; }
    int y = project ? 28 : PY;
    if (gfx_strip_hit(g, y, 26)) {
        gfx_text(g, 6, y, project ? "PROJECT SCALE" : (tr->scl == 0xFF ? "SCALE (FROM THE PROJECT)" : "OWN TRACK SCALE"), C_GREY, 1);
        snprintf(b, sizeof b, "%s %s", bm_note_names[root % 12], T(bm_scale_names[sc % BM_SCALES]));
        gfx_text(g, 6, y + 10, b, C_WHITE, 2);
        if (a->chord_name[0]) gfx_text_r(g, 234, y + 10, a->chord_name, C_ORANGE, 2);
    }
    if (app_kind(a, a->track) == KIND_NOTE || project) draw_keyboard(g, a, y + 28, VB - y - 36);
    else gfx_text_c(g, 120, 90, "PICK A NOTE TRACK", C_GREY, 1);
    hint_s(g, VB - 6, project ? "FOR TRACKS WITHOUT THEIR OWN SCALE" : "SCL AGAIN: CHORDS - HOME + SCL: PROJECT");
}

static void draw_chords(bm_gfx *g, const bm_app *a, int y0)
{
    const bm_meta *m = &a->pat.meta;
    char b[24];
    int t = bm_app_chord_track(a), root, sc = app_scale(a, t, &root);
    if (gfx_strip_hit(g, y0, 9)) {
        snprintf(b, sizeof b, "%s %.5s T%d", bm_note_names[root % 12], T(bm_scale_names[sc % BM_SCALES]), t + 1);
        gfx_text(g, 4, y0, b, C_WHITE, 1);
        if (a->last_pad >= 0) gfx_text_r(g, 236, y0, a->chord_name, C_WHITE, 1);
    }
    static const uint16_t FN[3] = {C_CYAN, C_GREEN, C_ORANGE};
    int rh = (VB - y0 - 10) / 4;
    for (int p = 0; p < 16; p++) {
        int x = (p % 4) * 60, y = y0 + 10 + (p / 4) * rh;
        if (!gfx_strip_hit(g, y, rh)) continue;
        bm_pad_chord pc = bm_chord_pad(m->chord_set, p);
        uint16_t fc = FN[bm_chord_function(pc.degree)];
        int k = bm_white_key(p);
        bool on = a->keys_down[k], last = p == a->last_pad;
        gfx_fill(g, x + 1, y, 58, rh - 1, on ? fc : (last ? dim(fc) : C_DDGREY));
        gfx_fill(g, x + 1, y, 3, rh - 1, fc);
        char rn[10], nm[10];
        bm_roman(root, sc, pc.degree, pc.ext, rn, sizeof rn);
        bm_chord_name(root, sc, pc.degree, pc.ext, nm, sizeof nm);
        gfx_text(g, x + 8, y + 2, rn, on ? C_BLACK : C_WHITE, 1);
        gfx_small(g, x + 8, y + rh - 8, nm, on ? C_BLACK : fc);
    }
}

/* ------------------------------------------------------------ ARP y LOOP */
static void draw_arp(bm_gfx *g, const bm_app *a)
{
    static const char *const M[5] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM"};
    static const char *const R[3] = {"1/8", "1/16", "1/32"};
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[24];
    if (bm_is_grid(&a->pat, t)) {
        gfx_text_c(g, 120, 56, "ROLL", bm_track_color(t), 3);
        gfx_text_c(g, 120, 86, "HOLD ARP + WHITE", C_WHITE, 1);
        snprintf(b, sizeof b, TF("SPEED %s (K1)"), bm_roll_names[a->roll_rate % 5]);
        gfx_text_c(g, 120, 98, b, C_GREY, 1);
        return;
    }
    gfx_text(g, 6, PY + 4, M[tr->arp_mode % 5], C_WHITE, 3);
    snprintf(b, sizeof b, "%s  %d OCT  GATE %d%%", R[tr->arp_rate % 3], tr->arp_oct, tr->arp_gate);
    hint_s(g, PY + 30, b);
    const bm_arp *ar = &a->ctx.arp[t];
    for (int i = 0; i < 8; i++) {
        int x = 10 + i * 28, h = 8 + i * 5;
        if (gfx_strip_hit(g, 122 - h, h)) gfx_fill(g, x, 122 - h, 22, h, i < ar->n ? (ar->playing && ar->cur % 12 == ar->held[i] % 12 ? C_WHITE : bm_track_color(t)) : C_DDGREY);
    }
    hint_s(g, VB - 6, tr->arp_mode ? "ALWAYS ON IN THIS TRACK" : "HOLD ARP = LIVE ARPEGGIO");
}

static void draw_loop(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_loop *lp = &a->pat.loop[t - 2];
    int mode = a->ctx.st.loop_mode[t - 2];
    static const char *const MN[5] = {"OFF", "ARMED", "RECORDING", "PLAY", "OVERDUB"};
    static const uint16_t MC[5] = {C_GREY, C_YELLOW, C_RED, C_GREEN, C_ORANGE};
    char b[30];
    gfx_text(g, 6, PY + 2, MN[mode % 5], MC[mode % 5], 2);
    snprintf(b, sizeof b, TF("%d BARS  %d/%d NOTES  %s"), lp->bars, lp->n, BM_LOOP_EVENTS, lp->on ? "ON" : "OFF");
    hint_s(g, PY + 22, b);
    int bx = 6, bw = 228, by = PY + 32, bh = 34;
    if (gfx_strip_hit(g, by, bh)) {
        gfx_rect(g, bx, by, bw, bh, C_DGREY);
        uint32_t L = (uint32_t)(lp->bars ? lp->bars : 2) * BM_BAR_TICKS;
        for (int i = 1; i < lp->bars * 4; i++) gfx_vline(g, bx + (int)(bw * (uint32_t)i / (uint32_t)(lp->bars * 4)), by + 1, i % 4 ? 4 : bh - 2, C_DDGREY);
        for (int i = 0; i < lp->n; i++) {
            if (!lp->ev[i].vel) continue;
            int x = bx + (int)((uint32_t)bw * lp->ev[i].tick / L);
            int y = by + bh - 4 - (lp->ev[i].note % 24);
            gfx_fill(g, x, y, 3, 3, bm_track_color(t));
        }
        if (mode >= LOOP_REC) gfx_vline(g, bx + (int)((uint32_t)bw * a->ctx.st.loop_pos[t - 2] / L), by, bh, C_WHITE);
    }
    hint_s(g, PY + 72, "REC: RECORD > PLAY > OVERDUB - STARTS ON THE NEXT BAR");
    hint_s(g, PY + 80, "REC 2 S: ERASE THE LOOP");
}

/* ------------------------------------------------------------ globales */
static void draw_system(bm_gfx *g, const bm_app *a)
{
    const bm_settings *s = &a->set;
    enum { NR = 11 };
    char v[NR][20];
    snprintf(v[0], 20, "%s", s->clock ? "EXTERNAL" : "INTERNAL");
    snprintf(v[1], 20, "%s", s->sync_mode ? "SPLIT L/R" : "STEREO");
    snprintf(v[2], 20, "%d PPQN", s->sync_ppqn);
    snprintf(v[3], 20, "%d MS", s->sync_lat);
    snprintf(v[4], 20, "%s", s->autosave ? "ON" : "OFF");
    snprintf(v[5], 20, "%s", s->led_dim == 0 ? "OFF" : (s->led_dim == 1 ? "DIM" : "MEDIUM"));
    snprintf(v[6], 20, "%s", s->fast_lcd ? "ON (TEST)" : "OFF");
    snprintf(v[7], 20, "%s", s->dual_core ? "ON (TEST)" : "OFF");
    int nt = 0; for (int i = 0; i < BM_TAKES; i++) nt += a->take[i].valid;
    unsigned fs = (unsigned)(bm_takes_free(a->take) * 10u / BM_SMP_RATE);
    snprintf(v[8], 20, TF("%d+%d, FREE %u.%uS"), nt, bm_snaps_user_n(), fs / 10, fs % 10);
    snprintf(v[9], 20, "%s", bm_lang_name());
    snprintf(v[10], 20, "%s", BM_VERSION_STR);
    static const char *const L[NR] = {"CLOCK", "SYNC OUT", "SYNC PPQN", "SYNC LATENCY", "AUTOSAVE", "IDLE LEDS", "FAST SCREEN", "TWO CORES",
                                      "TAKES+SOUNDS", N_("LANGUAGE"), "VERSION"};
    int first = a->menu_row > 7 ? a->menu_row - 7 : 0;
    for (int i = 0; i < 8 && first + i < NR; i++) row(g, 26 + i * 12, first + i == a->menu_row, T(L[first + i]), v[first + i]);
    if (a->menu_row == 6 || a->menu_row == 7) hint_s(g, 124, "TESTING: SAFE MODE TURNS IT OFF AFTER A CRASH");
}

static void draw_glo(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    char v[6][16];
    snprintf(v[0], 16, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10);
    snprintf(v[1], 16, "%d%%", m->swing);
    if (m->master) snprintf(v[2], 16, TF("%d STEPS"), m->master * 16); else snprintf(v[2], 16, "INF");
    snprintf(v[3], 16, "%d%%", a->set.drive * 100 / 127);
    if (m->fill_every) snprintf(v[4], 16, TF("EVERY %d"), m->fill_every); else snprintf(v[4], 16, "OFF");
    snprintf(v[5], 16, "%+d ST", (int)m->transpose - 24);
    static const char *const L[6] = {"BPM", "SWING", "MASTER LENGTH", "DRIVE", "AUTO FILL", "TRANSPOSE"};
    home_pages(g, a, NULL, C_WHITE);
    for (int i = 0; i < 6; i++) row(g, 48 + i * 12, i == a->glo_row, L[i], v[i]);
    hint_s(g, 124, "HOLD GLO = TRANSPOSE, PLAY = TAP");
}

static void draw_save(bm_gfx *g, const bm_app *a)
{
    char r0[24], r1[24], r4[24];
    snprintf(r0, sizeof r0, "A%02d", a->ctx.st.pattern + 1);
    snprintf(r1, sizeof r1, "A%02d", a->save_target + 1);
    snprintf(r4, sizeof r4, "%d", bm_pattern_count_events(&a->pat));
    header(g, "SAVE", a->dirty ? "UNSAVED" : "SAVED", C_WHITE);
    row(g, 48, a->save_row == 0, "SAVE", r0);
    row(g, 60, a->save_row == 1, "SAVE AS", r1);
    row(g, 72, a->save_row == 2, "SAVE SONG AND SCENES", NULL);
    row(g, 84, a->save_row == 3, a->clear_armed ? "CLEAR? SAVE AGAIN" : "CLEAR PATTERN", NULL);
    row(g, 96, a->save_row == 4, "EVENTS", r4);
    hint_s(g, 112, "SELECT PICKS, SAVE RUNS - HOLD SAVE = COPY PATTERN");
    gfx_small(g, 4, 122, a->ctx.st.playing ? "ONLY WHEN STOPPED (CUTS AUDIO)" : "ONLY WHEN STOPPED - SAVE 2 S: QUICK SAVE", a->ctx.st.playing ? C_ORANGE : C_GREY);
}

static void draw_song(bm_gfx *g, const bm_app *a)
{
    char b[24];
    home_pages(g, a, a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? C_ORANGE : C_GREY);
    if (!a->song.n) {
        gfx_text_c(g, 120, 66, "EMPTY", C_GREY, 2);
        gfx_text_c(g, 120, 92, "WHITE = ADD PATTERN", C_GREY, 1);
        gfx_text_c(g, 120, 104, "OR HOME + REC: SONG REC", C_GREY, 1);
        return;
    }
    int first = a->song_cur > 5 ? a->song_cur - 5 : 0;
    for (int i = 0; i < 6 && first + i < a->song.n; i++) {
        int e = first + i;
        uint8_t p = a->song.pat[e];
        if (p & 0x80) snprintf(b, sizeof b, TF("%2d  SCN %c  x%d"), e + 1, 'A' + (p & 3), a->song.rep[e]);
        else snprintf(b, sizeof b, "%2d  A%02d    x%d", e + 1, p + 1, a->song.rep[e]);
        row(g, 48 + i * 12, e == a->song_cur, b, (a->ctx.st.song_on && a->ctx.st.playing && e == a->ctx.st.song_pos) ? ">" : NULL);
    }
    hint_s(g, 124, "WHITE ADDS - BLACK 9 DELETES");
}

static void draw_time(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    header(g, "TIME", "POLYMETER", C_WHITE);
    for (int t = 0; t < BM_TRACKS; t++) {
        int y = 47 + t * 14;
        if (!gfx_strip_hit(g, y, 14)) continue;
        const bm_track *tr = &a->pat.trk[t];
        bool sel = t == a->time_row;
        if (sel) gfx_fill(g, 0, y - 1, 240, 14, C_DDGREY);
        char b[16];
        snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text(g, 4, y + 3, b, bm_track_color(t), 1);
        int tps = bm_speed_tps[tr->speed % SPD_COUNT];
        int len = bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? (sel ? a->time_lane : 0) : -1);
        int w = len * tps * 48 / BM_BAR_TICKS;
        if (w > 160) w = 160;
        if (w < len) w = len;
        int cur = st->playing ? (int)(st->tn[t] % (uint32_t)len) : -1;
        for (int s = 0; s < len; s++) {
            int x0 = 22 + s * w / len, x1 = 22 + (s + 1) * w / len;
            gfx_fill(g, x0, y + 2, x1 - x0 - (x1 - x0 > 2 ? 1 : 0), 9, s == cur ? C_WHITE : (s % 4 == 0 ? bm_track_color(t) : dim(bm_track_color(t))));
        }
        for (int bar = 1; bar * 48 < 160; bar++) gfx_vline(g, 22 + bar * 48, y, 12, C_GREY);
        snprintf(b, sizeof b, "%d %s", len, bm_speed_names[tr->speed % SPD_COUNT]);
        gfx_text_r(g, 236, y + 3, b, sel ? C_WHITE : C_GREY, 1);
    }
    if (bm_is_grid(&a->pat, a->time_row)) {
        char b[24];
        snprintf(b, sizeof b, "K3 INSTR: %s", T(bm_lane_name(a, a->time_row, a->time_lane)));
        hint_s(g, 130, b);
    } else hint_s(g, 130, "LINES = 1 BAR");
}

static void draw_scenes(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[24];
    home_pages(g, a, a->song_rec ? "SONG REC" : NULL, C_RED);
    for (int s = 0; s < BM_SCENES; s++) {
        int x = (s % 2) * 118 + 2, y = 48 + (s / 2) * 40;
        if (!gfx_strip_hit(g, y, 38)) continue;
        const bm_scene *sc = &a->song.scene[s];
        bool cur = st->scene == s, q = st->scene_q == s;
        gfx_fill(g, x, y, 114, 37, cur ? C_CYAN : C_DDGREY);
        if (q && ((a->now / 200) & 1)) { gfx_rect(g, x, y, 114, 37, C_YELLOW); gfx_rect(g, x + 1, y + 1, 112, 35, C_YELLOW); }
        snprintf(b, sizeof b, "%c", 'A' + s);
        gfx_text(g, x + 6, y + 6, b, cur ? C_BLACK : C_WHITE, 3);
        if (!sc->valid) { gfx_text(g, x + 34, y + 12, "EMPTY", C_GREY, 1); continue; }
        snprintf(b, sizeof b, "A%02d", sc->pattern + 1);
        gfx_text(g, x + 34, y + 5, b, cur ? C_BLACK : C_WHITE, 1);
        for (int t = 0; t < BM_TRACKS; t++)
            gfx_fill(g, x + 34 + t * 12, y + 17, 9, 8, ((sc->trk_mute >> t) & 1) ? C_DGREY : bm_track_color(t));
        if (cur && st->playing) {
            int bars = a->pat.meta.master ? a->pat.meta.master : 1;
            uint32_t total = (uint32_t)bars * BM_BAR_TICKS;
            gfx_fill(g, x + 6, y + 31, (int)(100u * (st->rtick % total) / total), 3, C_BLACK);
        }
    }
    if (a->chain_n > 1) {
        gfx_small(g, 6, 130, "CHAIN:", C_GREY);
        for (int i = 0; i < a->chain_n; i++) {
            snprintf(b, sizeof b, "%c", 'A' + a->chain[i]);
            gfx_small(g, 36 + i * 8, 130, b, i == st->chain_pos ? C_YELLOW : C_WHITE);
        }
    } else hint_s(g, 130, "HOLD ONE + OTHERS = CHAIN");
}

static void draw_copych(bm_gfx *g, const bm_app *a)
{
    char b[30];
    header(g, "COPY TRACK", "HOME + EDIT", C_WHITE);
    int s = a->copy_src, d = a->copy_dst % BM_TRACKS;
    if (gfx_strip_hit(g, 52, 52)) {
        gfx_fill(g, 10, 54, 90, 46, bm_track_color(s));
        snprintf(b, sizeof b, "T%d", s + 1);
        gfx_text_c(g, 55, 60, b, C_BLACK, 3);
        gfx_text_c(g, 55, 88, bm_track_name(a, s), C_BLACK, 1);
        gfx_text_c(g, 120, 70, ">", C_WHITE, 3);
        gfx_fill(g, 140, 54, 90, 46, bm_track_color(d));
        snprintf(b, sizeof b, "T%d", d + 1);
        gfx_text_c(g, 185, 60, b, C_BLACK, 3);
        gfx_text_c(g, 185, 88, bm_track_name(a, d), C_BLACK, 1);
    }
    hint_s(g, 110, "ALGORITHM PICKS THE TARGET - OCT+ PASTES - HOME CANCELS");
    hint_s(g, 120, "NOTES TO NOTES, HITS TO HITS: THE MACHINES DO NOT CHANGE");
}

/* ------------------------------------------------------------ capas: lo que hace cada tecla */
/* La franja del teclado ya muestra cada tecla con su luz; el visor dice qué capa es y cómo se usa. */
static void draw_layer(bm_gfx *g, const bm_app *a)
{
    bm_keyfn K[BM_KEYS];
    char title[24];
    uint16_t col;
    const char *h1, *h2;
    bm_app_keymap(a, K, title, sizeof title, &col, &h1, &h2);
    if (gfx_strip_hit(g, 26, 20)) {
        gfx_fill(g, 0, 26, BM_LCD_W, 18, col);
        gfx_text(g, 6, 28, title, C_BLACK, 2);
        char b[16];
        if (a->layer == L_HOME || a->view == V_HOME) snprintf(b, sizeof b, "A%02d", a->ctx.st.pattern + 1);
        else snprintf(b, sizeof b, "T%d", a->track + 1);
        gfx_text_r(g, 234, 32, b, C_BLACK, 1);
    }
    /* las teclas activas, grandes, para leerlas de lejos */
    int n = 0;
    for (int k = 0; k < BM_KEYS && n < 6; k++) {
        if (!K[k].label[0] || (K[k].led != LED_ON && K[k].led != LED_BLINK && !a->keys_down[k])) continue;
        int x = 4 + (n % 3) * 78, y = 52 + (n / 3) * 22;
        if (gfx_strip_hit(g, y, 20)) {
            bool dn = a->keys_down[k];
            gfx_fill(g, x, y, 74, 19, dn ? col : C_DDGREY);
            char l[8]; snprintf(l, sizeof l, "%.5s", T(K[k].label));
            gfx_text(g, x + 4, y + 6, l, dn ? C_BLACK : C_WHITE, 1);
            gfx_small(g, x + 60, y + 7, bm_key_is_black(k) ? "B" : "W", C_GREY);
        }
        n++;
    }
    if (!n && gfx_strip_hit(g, 60, 10)) gfx_text_c(g, 120, 62, "THE KEYS BELOW SAY WHAT THEY DO", C_GREY, 1);
    hint_s(g, 104, h1);
    hint_s(g, 113, h2);
    if (a->layer == L_HOME) hint_s(g, 124, "SELECT = GO TO A TRACK - ALGORITHM = BPM .1");
}

/* ------------------------------------------------------------ recuadro, pregunta, overlay, toast */
static void draw_box(bm_gfx *g, const bm_app *a)
{
    int t = a->box_mode == BOX_LANE ? a->track : a->box_track;
    if (!gfx_strip_hit(g, 50, 70)) return;
    gfx_fill(g, 40, 50, 160, 68, C_BLACK);
    gfx_rect(g, 40, 50, 160, 68, bm_track_color(t));
    gfx_rect(g, 41, 51, 158, 66, bm_track_color(t));
    char b[24];
    if (a->box_mode == BOX_LANE) {
        snprintf(b, sizeof b, "%d", a->box_lane + 1);
        gfx_text_c(g, 120, 58, b, bm_track_color(t), 4);
        const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->box_lane & 15];
        snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->box_lane)), T(bm_drm_short[ln->model % DRM_COUNT]));
        gfx_text_c(g, 120, 92, b, C_WHITE, 1);
    } else {
        snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text_c(g, 120, 58, b, bm_track_color(t), 4);
        gfx_text_c(g, 120, 92, bm_track_name(a, t), C_WHITE, 1);
    }
    if (a->box_hold == 1) gfx_text_c(g, 120, 106, "RELEASE HOME TO ENTER", C_GREY, 1);
    else {
        int left = (int)(a->box_until - a->now);
        int w = bm_clampi(left * 140 / 400, 0, 140);
        gfx_fill(g, 50, 108, 140, 3, C_DGREY);
        gfx_fill(g, 50, 108, 140 - w, 3, bm_track_color(t));
    }
}

static void draw_ask(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 46, 80)) return;
    gfx_fill(g, 10, 46, 220, 78, C_DDGREY);
    gfx_rect(g, 10, 46, 220, 78, C_YELLOW);
    gfx_rect(g, 11, 47, 218, 76, C_YELLOW);
    gfx_text_c(g, 120, 56, a->ask_text[0], C_WHITE, 1);
    gfx_text_c(g, 120, 70, a->ask_text[1], C_GREY, 1);
    gfx_fill(g, 30, 92, 80, 22, C_YELLOW);
    gfx_text_c(g, 70, 99, "OCT+ YES", C_BLACK, 1);
    gfx_rect(g, 130, 92, 80, 22, C_GREY);
    gfx_text_c(g, 170, 99, "HOME NO", C_WHITE, 1);
}

static void draw_overlay(bm_gfx *g, const bm_app *a)
{
    const bm_overlay *o = &a->ov;
    if (!gfx_strip_hit(g, 46, 84)) return;
    gfx_fill(g, 12, 46, 216, 84, C_BLACK);
    gfx_rect(g, 12, 46, 216, 84, o->color);
    gfx_rect(g, 13, 47, 214, 82, o->color);
    grid(g, 16, 50, 208, 76, 6);
    gfx_text(g, 20, 52, o->title, C_GREY, 1);
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

/* ------------------------------------------------------------ entrada */
static void draw_channel(bm_gfx *g, const bm_app *a)
{
    PASO(8);
    draw_tabs(g, a);
    int t = a->track, kind = app_kind(a, t);
    PASO(9);
    switch (a->page) {
    case PG_SEQ:
        if (kind == KIND_TR) draw_seq_tr(g, a);
        else draw_seq_syn(g, a);
        break;
    case PG_SOUND: PASO(11); draw_sound(g, a); break;
    case PG_INST: draw_inst(g, a); break;
    case PG_MIX: PASO(18); draw_ch_mix(g, a); break;
    case PG_MORE: PASO(19); draw_more(g, a); break;
    case PG_BREAK: PASO(16); draw_break(g, a); break;
    case PG_TAKES: PASO(17); draw_takes(g, a); break;
    case PG_ENV: draw_env(g, a); break;
    case PG_ARP: draw_arp(g, a); break;
    case PG_LOOP: draw_loop(g, a); break;
    case PG_SCL: draw_scl(g, a, 0); break;
    case PG_CHORDS: draw_chords(g, a, PY); break;
    case PG_LFO: draw_lfo(g, a); break;
    case PG_REC: draw_rec(g, a); break;
    case PG_PADS: draw_pads(g, a); break;
    default: break;
    }
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
    hints_update(a);
    draw_status(g, a);
    gfx_clip(g, 0, BM_CTX_Y, BM_LCD_W, BM_CTX_H);
    PASO(2);
    if (a->layer && a->layer != L_CHORDS) draw_layer(g, a);
    else if (a->layer == L_CHORDS) { gfx_text(g, 4, 26, "LIVE CHORDS", C_GREEN, 1); draw_chords(g, a, 36); }
    else switch (a->view) {
    case V_HOME: PASO(3); draw_home(g, a); break;
    case V_MIX: draw_mix(g, a); break;
    case V_SCENES: draw_scenes(g, a); break;
    case V_SONG: draw_song(g, a); break;
    case V_CH: draw_channel(g, a); break;
    case V_GLO: draw_glo(g, a); break;
    case V_SYSTEM: draw_system(g, a); break;
    case V_TIME: draw_time(g, a); break;
    case V_SAVE: draw_save(g, a); break;
    case V_FXG: draw_fx(g, a); break;
    case V_SCLG: draw_scl(g, a, 1); break;
    case V_COPYCH: draw_copych(g, a); break;
    default: break;
    }
    PASO(24);
    if (a->box_on) draw_box(g, a);
    PASO(25);
    if (a->ov.on) draw_overlay(g, a);
    PASO(26);
    if (a->ask) draw_ask(g, a);
    PASO(27);
    if (a->toast_until && !a->ask) draw_toast(g, a);
    gfx_noclip(g);
    PASO(28);
    draw_keys(g, a);
    PASO(29);
    draw_legend(g, a);
    PASO(0);
    if (a->now < a->clip_until) {
        gfx_fill(g, 0, 0, BM_LCD_W, 2, C_RED); gfx_fill(g, 0, BM_LCD_H - 2, BM_LCD_W, 2, C_RED);
        gfx_fill(g, 0, 0, 2, BM_LCD_H, C_RED); gfx_fill(g, BM_LCD_W - 2, 0, 2, BM_LCD_H, C_RED);
    }
}
