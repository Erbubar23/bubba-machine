/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pantallas (DISENO_3.0).
 *   y 0..23    estado (siempre igual; anillo de la pulsación de 2 s)
 *   y 24..199  contexto: en un canal, sus pestañas arriba y la página debajo
 *   y 200..239 leyenda de KNOB 1-4 en su orden físico y en su color
 * Todo el texto en pantalla va en ASCII (la fuente no tiene acentos). */
#include <stdio.h>
#include <string.h>
#include "bm_app_int.h"
#include "bm_rom.h"
#include "bm_util.h"
#include "bm_fm.h"
#include "bm_drums.h"
#include "bm_params.h"
#include <math.h>

#define PY 40   /* primera fila útil de una página de canal (debajo de las pestañas) */

static uint16_t dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }
static float bm_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ------------------------------------------------------------ estado */
static void draw_ring(bm_gfx *g, int cx, int cy, int p, uint16_t c)
{
    static const int8_t CS[24][2] = {{0,-7},{2,-7},{4,-6},{5,-5},{6,-4},{7,-2},{7,0},{7,2},{6,4},{5,5},{4,6},{2,7},
                                     {0,7},{-2,7},{-4,6},{-5,5},{-6,4},{-7,2},{-7,0},{-7,-2},{-6,-4},{-5,-5},{-4,-6},{-2,-7}};
    int n = p * 24 / 255;
    for (int i = 0; i < 24; i++) gfx_fill(g, cx + CS[i][0], cy + CS[i][1], 2, 2, i < n ? c : C_DGREY);
}

static void draw_status(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 0, BM_STATUS_H)) return;
    const bm_status *st = &a->ctx.st;
    char b[16];
    if (st->playing) for (int i = 0; i < 11; i++) gfx_vline(g, 5 + i, 6 + i / 2, 12 - (i / 2) * 2, C_GREEN);
    else gfx_fill(g, 5, 7, 10, 10, C_GREY);
    if (a->rec || st->smp_rec || a->song_rec || st->smp_armed) {
        if (!(st->smp_rec || a->song_rec || st->smp_armed) || ((a->now / 250) & 1)) gfx_disc(g, 23, 12, 4, st->smp_armed ? C_YELLOW : C_RED);
    }
    snprintf(b, sizeof b, "A%02d", st->pattern + 1);
    gfx_text(g, 32, 5, b, C_WHITE, 2);
    if (st->queued != 0xFF && st->queued != st->pattern) { snprintf(b, sizeof b, ">%02d", st->queued + 1); gfx_text(g, 70, 3, b, st->pchain_n > 1 ? C_CYAN : C_YELLOW, 1); }
    else if (st->scene_q != 0xFF) { snprintf(b, sizeof b, ">%c", 'A' + st->scene_q); gfx_text(g, 70, 3, b, C_CYAN, 1); }
    uint32_t bt = st->playing ? st->btick : 0;
    snprintf(b, sizeof b, "%u.%u", (unsigned)(bt / BM_BAR_TICKS % 99 + 1), (unsigned)((bt / BM_PPQN) % 4 + 1));
    gfx_text(g, 72, 13, b, C_GREY, 1);
    int t = a->track;
    bool ch = a->view == V_CH || a->layer;
    gfx_fill(g, 98, 4, 30, 16, ch ? bm_track_color(t) : C_DGREY);
    snprintf(b, sizeof b, "T%d", t + 1);
    gfx_text_c(g, 113, 5, b, ch ? C_BLACK : C_WHITE, 2);
    int bpm = a->pat.meta.bpm_x10;
    snprintf(b, sizeof b, "%d", (bpm + 5) / 10);
    gfx_text(g, 134, 5, b, C_WHITE, 2);
    gfx_text(g, 172, 13, "BPM", C_GREY, 1);
    if (a->set.clock) gfx_text(g, 172, 3, "EXT", st->ext_ok ? C_GREEN : C_YELLOW, 1);
    else if (st->scene != 0xFF) { snprintf(b, sizeof b, "%c", 'A' + st->scene); gfx_text(g, 172, 3, b, C_CYAN, 1); }
    if (st->fill && st->playing) gfx_text(g, 180, 3, "F", C_ORANGE, 1);
    if (a->pat.meta.transpose != 24) { snprintf(b, sizeof b, "%+d", a->pat.meta.transpose - 24); gfx_small(g, 186, 4, b, C_CYAN); }
    int hid = -1, p = bm_gesture_progress(&a->ges, a->now, &hid);
    if (p >= 0) draw_ring(g, 200, 12, p, C_YELLOW);
    else if (st->song_on) gfx_text(g, 194, 13, "S", C_YELLOW, 1);
    int bp = a->battery;
    uint16_t bc = bp > 30 ? C_GREEN : (bp > 15 ? C_YELLOW : C_RED);
    gfx_rect(g, 212, 7, 20, 10, C_GREY);
    gfx_fill(g, 232, 10, 2, 4, C_GREY);
    gfx_fill(g, 214, 9, bp * 16 / 100, 6, bc);
    gfx_hline(g, 0, 23, BM_LCD_W, C_DDGREY);
}

/* ------------------------------------------------------------ leyenda */
static void draw_legend(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, BM_LEGEND_Y, BM_LEGEND_H)) return;
    bm_legend L[4];
    bm_app_legend(a, L);
    gfx_hline(g, 0, BM_LEGEND_Y, BM_LCD_W, C_DDGREY);
    for (int i = 0; i < 4; i++) {
        int x = i * 60;
        uint16_t kc = bm_knob_color(i);
        bool hi = a->ov.on && a->ov.enc == i;
        if (hi) gfx_fill(g, x + 1, BM_LEGEND_Y + 1, 58, BM_LEGEND_H - 1, kc);
        if (i) gfx_vline(g, x, BM_LEGEND_Y + 2, BM_LEGEND_H - 4, C_DDGREY);
        if (!L[i].name[0]) continue;
        gfx_text(g, x + 4, BM_LEGEND_Y + 6, L[i].name, hi ? C_BLACK : kc, 1);
        gfx_text(g, x + 4, BM_LEGEND_Y + 17, L[i].val, hi ? C_BLACK : (L[i].locked ? kc : C_WHITE), 1);
        if (L[i].locked) gfx_fill(g, x + 52, BM_LEGEND_Y + 17, 4, 4, hi ? C_BLACK : kc);
        gfx_fill(g, x + 4, BM_LEGEND_Y + 30, 52, 4, hi ? dim(kc) : C_DGREY);
        gfx_fill(g, x + 4, BM_LEGEND_Y + 30, L[i].pct * 52 / 255, 4, hi ? C_BLACK : kc);
    }
}

/* ------------------------------------------------------------ helpers */
static void header(bm_gfx *g, const char *title, const char *sub, uint16_t col)
{
    gfx_text(g, 6, 30, title, col, 2);
    if (sub) gfx_text_r(g, 234, 34, sub, C_GREY, 1);
}

static void hint(bm_gfx *g, int y, const char *s) { if (gfx_strip_hit(g, y, 8)) gfx_text(g, 6, y, s, C_GREY, 1); }
static void hint_s(bm_gfx *g, int y, const char *s) { if (gfx_strip_hit(g, y, 6)) gfx_small(g, 4, y, s, C_GREY); }

static void row(bm_gfx *g, int y, bool sel, const char *l, const char *r)
{
    if (!gfx_strip_hit(g, y, 16)) return;
    if (sel) gfx_fill(g, 0, y, BM_LCD_W, 15, C_DGREY);
    gfx_text(g, 6, y + 4, l, sel ? C_WHITE : C_GREY, 1);
    if (r) gfx_text_r(g, 234, y + 4, r, sel ? C_CYAN : C_WHITE, 1);
}

static int src_name(const bm_app *a, int t, char *b, int cap)
{
    const bm_track *tr = &a->pat.trk[t];
    if (t == 1) {                                                /* el sampler: cuántos pads tiene el kit */
        int n = 0;
        for (int i = 0; i < BM_LANES; i++) n += bm_pad_ok(&a->pat.pad[i]);
        return n ? snprintf(b, (size_t)cap, TF("%d PADS"), n) : snprintf(b, (size_t)cap, "%s", T("NO KIT"));
    }
    if (tr->engine == ENG_MIDI) return snprintf(b, (size_t)cap, TF("CHANNEL %d"), tr->midi_ch + 1);
    return snprintf(b, (size_t)cap, "%s", bm_sound_name(a, t));
}

/* ¿el canal tiene golpe/nota en el paso absoluto s? */
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
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
    for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, s % len, k))) return 1;
    return 0;
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
        gfx_fill(g, x, y, 56, rowh - 3, bg);
        uint16_t fg = fl ? C_BLACK : (muted ? C_GREY : C_WHITE);
        gfx_text(g, x + 4, y + 4, bm_lane_name(a, t, i), fg, 1);
        char b[12]; snprintf(b, sizeof b, "%d %s", i + 1, T(bm_drm_short[bm_trk_trc(&a->pat, t)->lane[i].model % DRM_COUNT]));
        gfx_small(g, x + 4, y + rowh - 11, b, fg);
        if (muted) gfx_text(g, x + 40, y + rowh - 13, "M", C_RED, 1);
    }
}

/* ------------------------------------------------------------ pestañas del canal */
static void draw_tabs(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 24, 15)) return;
    int t = a->track;
    uint8_t tabs[8];
    int n = bm_app_tabs(a, t, tabs), in = 0;
    for (int i = 0; i < n; i++) if (tabs[i] == a->page) in = 1;
    int x = 4;
    for (int i = 0; i < n; i++) {
        const char *nm = bm_page_name(tabs[i]);
        bool cur = tabs[i] == a->page;
        int w = gfx_small_w(nm);
        gfx_small(g, x, 27, nm, cur ? C_WHITE : C_GREY);
        if (cur) gfx_fill(g, x, 34, w, 2, bm_track_color(t));
        x += w + 7;
    }
    if (!in) {                                     /* página fuera de las pestañas: ESCALA, GRABAR... */
        const char *nm = bm_page_name(a->page);
        int w = gfx_small_w(nm);
        gfx_fill(g, 236 - w - 4, 25, w + 4, 10, bm_track_color(t));
        gfx_small(g, 236 - w - 2, 27, nm, C_BLACK);
    }
    gfx_hline(g, 0, 37, BM_LCD_W, C_DDGREY);
}

/* ------------------------------------------------------------ HOME */
/* ------------------------------------------------------------ tira de teclas
 * Las luces de las teclas son blancas: dicen si una tecla está activa, no qué hace. La tira dibuja
 * las 11 negras y las 16 blancas con su nombre corto y el mismo estado que su luz:
 * relleno = encendida, borde claro = media, borde oscuro = tenue, sin borde = apagada. */
static void key_cell(bm_gfx *g, const bm_app *a, int x, int y, int w, int h, const bm_keyfn *f, int down, int big)
{
    uint8_t st = down ? LED_ON : f->led;
    if (st == LED_BLINK) st = ((a->now / 250u) & 1u) ? LED_ON : LED_MID;
    uint16_t bg = C_BLACK, fg = C_GREY, edge = 0;      /* apagada: el nombre igual se lee */
    switch (st) {
    case LED_ON: bg = C_WHITE; fg = C_BLACK; break;
    case LED_BREATH: bg = C_GREY; fg = C_BLACK; break;
    case LED_MID: edge = C_GREY; fg = C_WHITE; break;
    case LED_DIM: edge = C_DGREY; fg = C_GREY; break;
    default: break;
    }
    if (bg != C_BLACK) gfx_fill(g, x, y, w, h, bg);
    else if (edge) gfx_rect(g, x, y, w, h, edge);
    if (!f->label[0]) return;
    char l[6];
    int maxc = big ? (w - 2) / 6 : (w - 1) / 4;
    snprintf(l, sizeof l, "%.*s", maxc < 1 ? 1 : maxc > 5 ? 5 : maxc, T(f->label));   /* traducida y después recortada */
    if (big) gfx_text_c(g, x + w / 2, y + (h - 7) / 2, l, fg, 1);
    else gfx_small(g, x + (w - gfx_small_w(l)) / 2 + 1, y + (h - 5) / 2, l, fg);
}

static void draw_keystrip(bm_gfx *g, const bm_app *a, int y, int hb, int hw, int big)
{
    if (!gfx_strip_hit(g, y, hb + hw + 2)) return;
    bm_keyfn K[BM_KEYS];
    bm_app_keys(a, K);
    gfx_fill(g, 0, y, BM_LCD_W, hb + hw + 2, C_BLACK);
    int wy = y + hb + 2;
    for (int i = 0; i < BM_WHITES; i++) {
        int k = bm_white_key(i);
        key_cell(g, a, i * 15, wy, 14, hw, &K[k], a->keys_down[k], big);
    }
    for (int i = 0; i < BM_BLACKS; i++) {
        int k = bm_black_key(i), wi = bm_key_white_index(k - 1);
        key_cell(g, a, (wi + 1) * 15 - 7, y, 14, hb, &K[k], a->keys_down[k], 0);
    }
}


/* Solo las negras, cuando las blancas ya se ven en la pantalla (los pasos de SEQ, las corcheas de
 * CORTAR, los cortes del SLICER): más anchas y con su nombre entero, agrupadas como en el teclado
 * (2, 3, 2, 3 y 1). */
static void draw_blackstrip(bm_gfx *g, const bm_app *a, int y, int h)
{
    if (!gfx_strip_hit(g, y, h + 1)) return;
    bm_keyfn K[BM_KEYS];
    bm_app_keys(a, K);
    gfx_fill(g, 0, y, BM_LCD_W, h + 1, C_BLACK);
    static const uint8_t GRP[BM_BLACKS] = {0, 0, 1, 1, 1, 2, 2, 3, 3, 3, 4};
    int x = 3;
    for (int i = 0; i < BM_BLACKS; i++) {
        if (i && GRP[i] != GRP[i - 1]) x += 4;
        int k = bm_black_key(i);
        key_cell(g, a, x, y, 18, h, &K[k], a->keys_down[k], 0);
        x += 20;
    }
}

static void draw_home(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[16];
    for (int t = 0; t < BM_TRACKS; t++) {
        int y = 26 + t * 22;
        if (!gfx_strip_hit(g, y, 22)) continue;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        bool sel = a->box_on ? t == a->box_track : t == a->track;
        if (sel) gfx_fill(g, 0, y, BM_LCD_W, 21, C_DDGREY);
        if (bm_is_free(t)) gfx_rect(g, 4, y + 3, 6, 17, col); else gfx_fill(g, 4, y + 3, 6, 17, col);
        snprintf(b, sizeof b, "T%d %s", t + 1, T(bm_track_name(a, t)));
        gfx_text(g, 14, y + 3, b, tr->mute ? C_GREY : C_WHITE, 1);
        src_name(a, t, b, sizeof b);
        gfx_small(g, 14, y + 13, b, C_GREY);
        uint32_t n = st->tn[t];
        int cur = st->playing ? (int)(n % 16) : -1;
        int base = st->playing ? (int)((n / 16) * 16) : 0;
        for (int s = 0; s < 16; s++) {
            uint16_t c = track_has(a, t, base + s) ? col : C_DGREY;
            if (s == cur) c = C_WHITE;
            gfx_fill(g, 96 + s * 6, y + 4, 5, 6, c);
        }
        gfx_fill(g, 96, y + 14, 95, 3, C_DGREY);
        gfx_fill(g, 96, y + 14, st->peak[t] * 95 / 255, 3, col);
        if (tr->speed != SPD_X1) gfx_small(g, 196, y + 3, bm_speed_names[tr->speed], C_GREY);
        if (tr->mute) gfx_text(g, 198, y + 12, "M", C_RED, 1);
        if (a->now < a->trig_until[t]) gfx_disc(g, 226, y + 11, 5, col);
        else gfx_disc(g, 226, y + 11, 2, C_DGREY);
    }
    /* las teclas de HOME: blancas = los 16 patrones, negras = mutes, escenas y FILL */
    draw_keystrip(g, a, 161, 14, 22, 1);
}

static void draw_mix(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    header(g, "MIX", "ALGORITHM = TRACK", C_WHITE);
    for (int t = 0; t < BM_TRACKS; t++) {
        int x = 4 + t * 39, y = 52;
        if (!gfx_strip_hit(g, y, 140)) break;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        if (t == a->track) gfx_rect(g, x - 2, y - 2, 37, 136, C_WHITE);
        char b[8]; snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text(g, x + 6, y, b, col, 1);
        gfx_fill(g, x + 4, y + 12, 12, 90, C_DDGREY);
        int h = tr->level * 90 / 127;
        gfx_fill(g, x + 4, y + 102 - h, 12, h, col);
        gfx_fill(g, x + 20, y + 12, 6, 90, C_DDGREY);
        int pk = st->peak[t] * 90 / 255;
        gfx_fill(g, x + 20, y + 102 - pk, 6, pk, C_GREEN);
        char pn[6];
        if (tr->pan == 64) snprintf(pn, sizeof pn, "C"); else snprintf(pn, sizeof pn, "%c%d", tr->pan < 64 ? 'L' : 'R', tr->pan < 64 ? (64 - tr->pan) * 50 / 64 : (tr->pan - 64) * 50 / 63);
        gfx_small(g, x + 2, y + 108, pn, C_GREY);
        snprintf(b, sizeof b, "E%d", tr->send * 100 / 127);
        gfx_small(g, x + 2, y + 116, b, C_GREY);
        if (tr->mute) gfx_text(g, x + 10, y + 124, "M", C_RED, 1);
    }
}

/* ------------------------------------------------------------ SEQ: rejilla TR */
/* Batería: solo la sección que tocan las negras 1-8 (instrumentos 1-8 o 9-16, negra 9 cambia),
 * con filas del doble de alto. Cada línea con su cabezal (cada instrumento tiene su largo). */
static void draw_seq_tr(bm_gfx *g, const bm_app *a)
{
    int t = a->track, sel = a->sel_lane[t], base = (sel / 8) * 8;
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint16_t col = bm_track_color(t);
    char b[30];
    if (gfx_strip_hit(g, PY, 10)) {
        if (a->held_step >= 0 && a->held_long) {                 /* paso sostenido: cómo está */
            uint8_t v = tr->st[sel][a->held_step];
            int rat = (v & ST_FLAM) ? 4 : ((v & ST_RAT_MASK) >> ST_RAT_SH);
            static const char *const R[5] = {"X1", "X2", "X3", "X4", "FLAM"};
            static const char *const C[3] = {"ALWAYS", "FILL", "NO FILL"};
            snprintf(b, sizeof b, "%02d %s %s %s", a->held_step + 1, T((v & ST_ACC) ? "ACCENT" : "NORMAL"), R[rat], T(C[(v & ST_COND_MASK) >> ST_COND_SH]));
            gfx_fill(g, 0, PY - 1, 240, 10, C_DGREY);
            gfx_text(g, 4, PY, b, C_YELLOW, 1);
        } else {
            if (bm_is_pads(t)) {                                     /* el pad elegido y su toma */
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
        int l = base + i, y = PY + 12 + i * 16;
        if (!gfx_strip_hit(g, y, 16)) continue;
        bool muted = (tr->mute >> l) & 1;
        int len = tr->lane[l].len ? tr->lane[l].len : 16;
        int ps = bm_app_track_step(a, t, l);
        if (l == sel) gfx_fill(g, 0, y, 240, 15, C_DDGREY);
        char nm[5]; snprintf(nm, sizeof nm, "%s", T(bm_lane_name(a, t, l)));
        gfx_text(g, 1, y + 4, nm, a->now < a->lane_flash[t][l] ? col : (muted ? C_RED : (l == sel ? C_WHITE : C_GREY)), 1);
        for (int s2 = 0; s2 < 16; s2++) {
            int ab = a->step_page * 16 + s2, x = 27 + s2 * 13 + (s2 / 4);
            if (ab >= len) { gfx_fill(g, x + 5, y + 6, 2, 2, C_DDGREY); continue; }
            uint8_t v = tr->st[l][ab];
            uint16_t c = muted ? C_DGREY : (l == sel ? col : dim(col));
            if (v & ST_ON) {
                int cond = (v & ST_COND_MASK) >> ST_COND_SH;
                if (cond) gfx_rect(g, x, y + 1, 12, 13, cond == 1 ? C_ORANGE : C_GREY);
                else gfx_fill(g, x, y + 1, 12, 13, (v & ST_ACC) ? C_WHITE : c);
                if (cond && (v & ST_ACC)) gfx_fill(g, x + 2, y + 3, 8, 9, C_WHITE);
                int rat = (v & ST_RAT_MASK) >> ST_RAT_SH;
                for (int r = 0; r < rat; r++) gfx_fill(g, x + 2 + r * 3, y + 10, 2, 2, C_BLACK);
                if (v & ST_FLAM) gfx_fill(g, x + 9, y + 2, 2, 2, C_BLACK);
                if (v & ST_LOCK) gfx_fill(g, x + 1, y + 2, 2, 2, C_BLACK);
            } else {
                gfx_fill(g, x, y + 1, 12, 13, l == sel ? C_DGREY : C_BLACK);
                if (s2 % 4 == 0) gfx_fill(g, x + 5, y + 6, 2, 2, C_DGREY);
            }
            if (ab == ps) gfx_fill(g, x, y + 13, 12, 2, (v & ST_ON) && !muted ? C_WHITE : C_GREY);
            if (l == sel && ab == a->held_step && a->held_long) gfx_rect(g, x - 1, y, 14, 15, C_YELLOW);
        }
    }
}

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
            gfx_fill(g, 58, PY - 1, gfx_text_w(b, 1) + 4, 9, C_RED);
            gfx_text(g, 60, PY, b, C_BLACK, 1);
        } else if (a->note_oct) { snprintf(b, sizeof b, TF("BLACKS %+d"), a->note_oct); gfx_small(g, 62, PY + 1, b, C_GREY); }
        if (len % spb == 0) snprintf(b, sizeof b, TF("%d BARS %s"), len / spb, bm_speed_names[trk->speed % SPD_COUNT]);
        else snprintf(b, sizeof b, "L%d %s", len, bm_speed_names[trk->speed % SPD_COUNT]);
        gfx_text_r(g, 236, PY, b, C_GREY, 1);
    }
    /* rango de notas de la página (al menos una octava) */
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
    int R = hi - lo + 1, ry = PY + 11, rh = 100, row = rh / R;
    if (row < 3) row = 3;
    rh = row * R;
    if (gfx_strip_hit(g, ry, rh + 2)) {
        for (int s = 0; s < 16; s++) {                           /* columnas = las 16 blancas */
            int ab = p0 + s, x = s * 15;
            gfx_fill(g, x, ry, 15, rh, ab == ps ? C_DDGREY : C_BLACK);
            if (ab >= len) { gfx_fill(g, x + 6, ry + rh / 2, 2, 2, C_DDGREY); continue; }
            if (s % 4 == 0) gfx_vline(g, x, ry, rh, s ? C_DGREY : C_GREY);   /* tiempos */
            else for (int y = ry; y < ry + rh; y += 4) gfx_fill(g, x, y, 1, 1, C_DDGREY);
            if (ab == ps) gfx_fill(g, x, ry, 14, 2, C_WHITE);
        }
        for (int n = lo; n <= hi; n++)                           /* los do, de guía */
            if (n % 12 == 0) gfx_hline(g, 0, ry + (hi - n) * row + row / 2, 240, C_DGREY);
        for (int s = -16; s < 16; s++) {                         /* notas que empiezan antes y siguen sonando */
            int ab = p0 + s;
            if (ab < 0 || ab >= len) continue;
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = bm_evc(&a->pat, t, ab, k);
                if (bm_event_empty(e) || e->note < lo || e->note > hi) continue;
                int l = e->len & BM_LEN_MASK;
                if (l < 1) l = 1;
                int x0 = s * 15, x1 = (s + l) * 15 - 2;
                if (e->len & BM_LEN_TIE) x1 += 3;
                if (x1 <= 0) continue;
                if (x0 < 0) x0 = 0;
                if (x1 > 239) x1 = 239;
                int y = ry + (hi - e->note) * row;
                uint16_t c = e->vel >= BM_VEL_ACC ? C_WHITE : col;
                if (e->vel <= BM_VEL_GHOST) gfx_rect(g, x0, y, x1 - x0, row, col);
                else gfx_fill(g, x0, y, x1 - x0, row - (row > 3), c);
                if (s >= 0) gfx_fill(g, x0, y, 2, row, C_WHITE);   /* el ataque */
                bool wk = e->prob < 100 || e->prob == BM_PROB_FILL || e->prob == BM_PROB_NOFILL;
                if (wk && s >= 0) gfx_fill(g, x0 + 3, y, 2, row, C_BLACK);
            }
        }
        if (a->held_step >= 0 && a->held_long && a->held_step / 16 == a->step_page) gfx_rect(g, (a->held_step % 16) * 15 - 1, ry, 16, rh, C_YELLOW);
        snprintf(b, sizeof b, "%s", bm_note_names[hi % 12]);
        gfx_small(g, 236 - gfx_small_w(b), ry + 1, b, C_GREY);
        bm_note_str(lo, c2, sizeof c2);
        gfx_small(g, 236 - gfx_small_w(c2), ry + rh - 6, c2, C_GREY);
    }
    if (a->held_step >= 0 && a->held_long) {
        int y0 = ry + rh + 2;
        if (!gfx_strip_hit(g, y0, 200 - y0)) return;
        const bm_event *e = NULL;
        for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, a->held_step, k))) { e = bm_evc(&a->pat, t, a->held_step, k); break; }
        gfx_fill(g, 0, y0, 240, 200 - y0, C_DDGREY);
        if (!e) { gfx_text_c(g, 120, y0 + 14, "EMPTY STEP", C_GREY, 1); return; }
        int nn = 0; char notes[24] = "";
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *x = bm_evc(&a->pat, t, a->held_step, k);
            if (bm_event_empty(x)) continue;
            char nb[6]; bm_note_str(x->note, nb, sizeof nb);
            size_t l = strlen(notes);
            snprintf(notes + l, sizeof notes - l, "%s%s", nn++ ? " " : "", nb);
        }
        gfx_text(g, 4, y0 + 3, notes, C_WHITE, 1);
        if (e->len & BM_LEN_TIE) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, TF("%d STEPS"), e->len & BM_LEN_MASK);
        gfx_text_r(g, 236, y0 + 3, b, C_WHITE, 1);
        if (e->prob == BM_PROB_FILL) snprintf(b, sizeof b, "FILL"); else if (e->prob == BM_PROB_NOFILL) snprintf(b, sizeof b, "NO FILL"); else snprintf(b, sizeof b, "%d%%", e->prob);
        gfx_text(g, 4, y0 + 14, e->vel >= BM_VEL_ACC ? "ACCENT" : e->vel <= BM_VEL_GHOST ? "GHOST" : "NORMAL", C_YELLOW, 1);
        gfx_text(g, 90, y0 + 14, b, C_YELLOW, 1);
        gfx_small(g, 4, y0 + 26, "SELECT HIT - ALGORITHM PROB - PRESETS LENGTH", C_GREY);
        gfx_small(g, 4, y0 + 34, "BLACKS 1-8 = SCALE NOTES", C_GREY);
        return;
    }
    /* los compases del canal, y el seguidor de tempo punteado */
    int oy = ry + rh + 6;
    if (gfx_strip_hit(g, oy, 200 - oy)) {
        int bars = (len + spb - 1) / spb;
        if (bars < 1) bars = 1;
        int bw = 232 / bars;
        for (int k = 0; k < bars; k++) {
            int x = 4 + k * bw;
            bool cur = a->step_page * 16 / spb == k || (a->step_page * 16 + 15) / spb == k;
            gfx_fill(g, x, oy, bw - 2, 18, cur ? C_DDGREY : C_BLACK);
            gfx_rect(g, x, oy, bw - 2, 18, cur ? col : C_DGREY);
            snprintf(b, sizeof b, "%d", k + 1);
            gfx_small(g, x + 2, oy + 2, b, cur ? C_WHITE : C_GREY);
        }
        for (int s = 0; s < len; s++)
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = bm_evc(&a->pat, t, s, k);
                if (bm_event_empty(e)) continue;
                int x = 4 + s * (bw * bars) / len, y = oy + 15 - (e->note - lo) * 10 / (R > 1 ? R : 1);
                gfx_fill(g, x, bm_clampi(y, oy + 3, oy + 15), 2, 2, col);
            }
        int py = oy + 22;
        for (int x = 4; x < 4 + bw * bars; x += 4) gfx_fill(g, x, py, 2, 2, C_DGREY);   /* el seguidor: puntos */
        if (ps >= 0) {
            int px = 4 + ps * (bw * bars) / len;
            for (int x = 4; x <= px; x += 4) gfx_fill(g, x, py, 2, 2, col);
            gfx_fill(g, px, py - 2, 2, 6, C_WHITE);
        }
    }
}

/* ------------------------------------------------------------ SONIDO */
/* ACID: dos ciclos de la forma de onda del oscilador, y el filtro como una curva. */
static void draw_synth(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const bm_track *tr = &a->pat.trk[t];
    gfx_rect(g, x0, y0, w, h, C_DGREY);
    gfx_hline(g, x0 + 1, y0 + h / 2, w - 2, C_DDGREY);
    int timbre = (tr->source & 1) * 2;
    int py = -1, cw = (w - 4) / 2;
    for (int x = 0; x < w - 4; x++) {
        int ph = (x % cw) * 256 / cw, s;
        int saw = 127 - ph, sq = ph < 128 ? 100 : -100, pul = ph < 64 ? 100 : -100;
        switch (timbre) {
        case 1: s = saw * 3 / 4 + ((x % (2 * cw)) < cw ? 30 : -30); break;
        case 2: s = sq; break;
        case 3: s = pul; break;
        case 4: s = (saw + sq) / 2; break;
        case 5: s = saw * 3 / 4 + (int)((x * 2654435761u) >> 27) - 16; break;
        default: s = saw; break;
        }
        int y = y0 + h / 2 - s * (h / 2 - 8) / 128;
        if (py >= 0) gfx_line(g, x0 + 1 + x, py, x0 + 2 + x, y, bm_track_color(t));
        py = y;
    }
    int fx = x0 + 4 + tr->macro[0] * (w - 30) / 127, peak = tr->macro[1] * 18 / 127;   /* el filtro */
    for (int x = x0 + 4; x < x0 + w - 4; x++) {
        int y = x < fx - 12 ? y0 + 10 : x < fx ? y0 + 10 - peak * (x - fx + 12) / 12 : y0 + 10 + (x - fx) * 2 / 3;
        if (y < y0 + h - 2) gfx_fill(g, x, bm_clampi(y, y0 + 1, y0 + h - 2), 1, 2, C_GREY);
    }
}

/* FM: los seis operadores como barras de nivel (con las macros ya aplicadas). Las portadoras, las
 * que se oyen, en el color del canal; los moduladores, que dan el timbre, en gris. Abajo, la
 * relación de frecuencia de cada uno. */
static void draw_fm(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    uint8_t p[BM_FM_PATCH];
    bm_fm_patch_of(&a->pat.trk[t], NULL, p);
    uint8_t car = bm_fm_carriers(p[BM_FM_ALG]);
    gfx_rect(g, x0, y0, w, h, C_DGREY);
    int cw = (w - 8) / 6, bh = h - 24;
    char b[12];
    for (int k = 0; k < 6; k++) {                         /* OP1 a la izquierda (el dato 5) */
        const uint8_t *o = p + (5 - k) * BM_FM_OP;
        int x = x0 + 4 + k * cw, lv = o[FMO_OL] * bh / 99;
        bool c = (car >> (5 - k)) & 1;
        gfx_fill(g, x + 3, y0 + 4, cw - 6, bh, C_DDGREY);
        if (lv > 0) gfx_fill(g, x + 3, y0 + 4 + bh - lv, cw - 6, lv, c ? bm_track_color(t) : C_GREY);
        snprintf(b, sizeof b, "%d", k + 1);
        gfx_text(g, x + cw / 2 - 3, y0 + 6, b, c ? C_WHITE : C_DGREY, 1);
        int fc = o[FMO_FC], ff = o[FMO_FF];
        if (o[FMO_MODE]) snprintf(b, sizeof b, "FIXED");
        else {
            int r100 = (fc ? fc * 100 : 50) * (100 + ff) / 100;      /* relación x100 */
            if (r100 % 100) snprintf(b, sizeof b, "%d.%02d", r100 / 100, r100 % 100);
            else snprintf(b, sizeof b, "%d", r100 / 100);
        }
        gfx_small(g, x + 2, y0 + h - 16, b, c ? C_WHITE : C_GREY);
    }
    snprintf(b, sizeof b, "ALG %d", p[BM_FM_ALG] + 1);
    gfx_small(g, x0 + 4, y0 + h - 8, b, C_YELLOW);
    snprintf(b, sizeof b, "FB %d", p[BM_FM_FB]);
    gfx_small(g, x0 + 60, y0 + h - 8, b, C_YELLOW);
    gfx_small(g, x0 + 110, y0 + h - 8, "IN COLOR: AUDIBLE", C_GREY);
}

/* Tres carriles, de abajo arriba: graves, medios, agudos. Separados para que el relleno
 * de graves y medios no tape el patrón de los hats. */
/* La onda espejada, como en un sampler: el pico en el tono oscuro y el RMS encima, a todo color.
 * Columnas [c0, c1) de wave en w-2 píxeles; las de [hl0, hl1) van con el color resaltado. */
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

/* ------------------------------------------------------------ sampler de pads (T2) */
/* El mapa de una toma (toda, en picos) con sus pads como rayas arriba; la ventana del zoom en
 * blanco y el cabezal en naranja. hl: el pad que se resalta (o -1). */
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
    for (int i = 0; i < BM_LANES; i++) {                         /* los pads de esta toma */
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

/* una casilla por blanca (pads): llena = suena, recuadro = el elegido, oscura = vacío */
static void pad_cells(bm_gfx *g, const bm_app *a, int cy)
{
    if (!gfx_strip_hit(g, cy, 19)) return;
    uint16_t col = bm_track_color(1);
    char b[4];
    int sel = a->sel_lane[1] & 15;
    bool holding = a->pad_key >= 0 && a->keys_down[a->pad_key];
    for (int i = 0; i < BM_LANES; i++) {
        int x = i * 15;
        bool ok = bm_pad_ok(&a->pat.pad[i]), play = a->now < a->lane_flash[1][i];
        if (play) gfx_fill(g, x, cy, 14, 18, col);
        else if (ok) gfx_fill(g, x, cy, 14, 18, C_DDGREY);
        else gfx_rect(g, x, cy, 14, 18, C_DDGREY);
        if (i == sel) gfx_rect(g, x, cy, 14, 18, holding ? C_ORANGE : C_CYAN);
        snprintf(b, sizeof b, "%d", i + 1);
        gfx_small(g, x + 7 - gfx_small_w(b) / 2, cy + 7, b, play ? C_BLACK : (i == sel ? C_WHITE : ok ? C_GREY : C_DGREY));
    }
}

/* PADS: el editor del pad elegido, como un KO II o un OP-1 field. Arriba el mapa de su toma (con
 * todos sus pads); en el medio la onda grande con el pad entre corchetes cian; abajo una casilla
 * por blanca. EDIT otra vez pasa TRIM > SOUND > ENV. */
static void draw_pads(bm_gfx *g, const bm_app *a)
{
    int sel = a->sel_lane[1] & 15, page = a->ed_page[1] % 3;
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
        static const char *const PG[3] = {N_("TRIM"), N_("SOUND"), N_("ENV")};
        int xr = 236;
        for (int i = 2; i >= 0; i--) {
            xr -= gfx_small_w(PG[i]);
            gfx_small(g, xr, PY + 1, PG[i], i == page ? C_YELLOW : C_DGREY);
            xr -= 6;
        }
    }
    if (!ok) {
        if (gfx_strip_hit(g, PY + 30, 40)) {
            gfx_text_c(g, 120, PY + 34, "EMPTY PAD", C_WHITE, 1);
            gfx_small(g, 120 - gfx_small_w("TAKES: N1 = KIT FROM A TAKE") / 2, PY + 50, "TAKES: N1 = KIT FROM A TAKE", C_GREY);
            gfx_small(g, 120 - gfx_small_w("N4 = THE TAKE ON THIS PAD") / 2, PY + 58, "N4 = THE TAKE ON THIS PAD", C_GREY);
        }
        pad_cells(g, a, PY + 108);
        hint_s(g, PY + 130, "WHITE = PLAY AND PICK A PAD - PRESETS = ITS TAKE");
        return;
    }
    take_map(g, a, p->src, len, PY + 11, 12, sel);
    /* la onda grande: la ventana del zoom */
    int wy = PY + 26, wh = 70;
    uint32_t v0 = a->cut_v0, v1 = a->cut_v1 > a->cut_v0 ? a->cut_v1 : len, span = v1 - v0;
#define VX(q) (5 + (int)(((int64_t)(q) - (int64_t)v0) * 230 / (int64_t)span))
    if (gfx_strip_hit(g, wy, wh + 1)) {
        uint32_t a0 = p->start, a1 = (uint32_t)p->start + p->len;
        int bx0 = bm_clampi(VX(a0), 4, 236), bx1 = bm_clampi(VX(a1), 4, 236);
        if (bx1 > bx0) gfx_fill(g, bx0, wy, bx1 - bx0, wh, C_DDGREY);           /* la caja del pad */
        gfx_hline(g, 4, wy + wh / 2, 232, C_DDGREY);
        int hl0 = (int)(((int64_t)a0 - v0) * 232 / (int64_t)span), hl1 = (int)(((int64_t)a1 - v0) * 232 / (int64_t)span);
        draw_wave(g, a->wave_cut, 4, wy, 232, wh, 0, 232, hl0, hl1, dim(col), C_WHITE);
        if (a->cut_snap) {                                                    /* la rejilla de SNAP */
            uint32_t bpm = a->take[p->src].bpm_x10 ? a->take[p->src].bpm_x10 : a->pat.meta.bpm_x10;
            uint32_t snap = (uint32_t)BM_SMP_RATE * (a->cut_snap == 1 ? 150u : 300u) / bpm;
            if (snap && span / snap < 116)
                for (uint32_t q = (v0 + snap - 1) / snap * snap; q < v1; q += snap) gfx_vline(g, VX(q), wy + wh - 4, 4, C_GREY);
        }
        for (int i = 0; i < BM_LANES; i++) {                                  /* dónde empiezan los otros pads */
            const bm_pad *o = &a->pat.pad[i];
            if (i == sel || !bm_pad_ok(o) || o->src != p->src || o->start < v0 || o->start >= v1) continue;
            int x = VX(o->start);
            gfx_vline(g, x, wy, wh, C_DGREY);
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_small(g, x + 2, wy + 2, b, C_GREY);
        }
        if (a0 >= v0 && a0 < v1) {                                           /* [ el inicio (K1 en TRIM) */
            int x = VX(a0);
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x, wy, 5, C_CYAN); gfx_hline(g, x, wy + wh - 1, 5, C_CYAN);
            gfx_fill(g, x, wy + 2, 9, 7, C_CYAN);
            snprintf(b, sizeof b, "%d", sel + 1);
            gfx_small(g, x + 2, wy + 3, b, C_BLACK);
        }
        if (a1 > v0 && a1 <= v1) {                                           /* ] el final (K2 en TRIM) */
            int x = VX(a1) - 1;
            gfx_vline(g, x, wy, wh, C_CYAN); gfx_hline(g, x - 4, wy, 5, C_CYAN); gfx_hline(g, x - 4, wy + wh - 1, 5, C_CYAN);
        }
        if (st->smp_pos_src == p->src && st->smp_pos >= v0 && st->smp_pos < v1) gfx_vline(g, VX(st->smp_pos), wy, wh, C_ORANGE);
    }
#undef VX
    /* lo que es el pad, en una línea */
    if (gfx_strip_hit(g, wy + wh + 2, 6)) {
        unsigned ms = (unsigned)((uint32_t)p->len * 1000u / BM_SMP_RATE);
        snprintf(b, sizeof b, "%s  %+d ST  %u MS%s%s", T(bm_pad_mode_name(p->mode)), p->pitch - 24, ms,
                 (p->flags & PAD_REV) ? "  REV" : "", (p->flags & PAD_TEMPO) ? "  SYNC" : "");
        gfx_small(g, 4, wy + wh + 2, b, C_WHITE);
        snprintf(b, sizeof b, "X%d", 1 << (a->cut_zoom & 3));
        gfx_small(g, 236 - gfx_small_w(b), wy + wh + 2, b, C_GREY);
    }
    pad_cells(g, a, PY + 108);
    bool holding = a->pad_key >= 0 && a->keys_down[a->pad_key];
    static const char *const HL[3] = {N_("K1 START - K2 END - EDIT = SOUND, ENV"), N_("K3 PLAY: ONESHOT, KEY, THRU OR LOOP"), N_("ATTACK, RELEASE, CHOKE GROUP, TEMPO SYNC")};
    hint_s(g, PY + 130, holding ? "HELD: KEY, THRU AND LOOP KEEP PLAYING" : HL[page]);
}

/* BREAK: la fila YOU (lo que programaste: el primer pad de cada paso) y la fila PLAYS (lo que
 * BREAK hace sonar este compás). Con las 4 perillas en 0 son iguales. */
static void draw_break(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    const bm_track *tk = &a->pat.trk[1];
    const bm_tr *tr = &a->pat.tr[1];
    uint16_t col = bm_track_color(1);
    char b[30];
    bool on = tk->gen[0] || tk->gen[2] || tk->gen[3];
    if (gfx_strip_hit(g, PY, 9)) {
        gfx_text(g, 4, PY, on ? "THE GRID, VARIED EVERY BAR" : "ALL AT 0: THE GRID AS IS", on ? C_WHITE : C_GREY, 1);
    }
    uint32_t blk = st->gen_blk;
    int cur = st->playing ? (int)(st->tn[1] % 16u) : -1;
    int ty = PY + 22, sy = PY + 58;
    if (gfx_strip_hit(g, ty - 9, 70)) {
        gfx_small(g, 2, ty - 8, "YOU", C_GREY);
        gfx_small(g, 2, sy - 8, "PLAYS", C_GREY);
        for (int k = 0; k < 16; k++) {
            int x = 1 + k * 15;
            int main = -1;
            for (int l = 0; l < BM_LANES && main < 0; l++) {
                int len = tr->lane[l].len ? tr->lane[l].len : 16;
                if (!((tr->mute >> l) & 1) && (tr->st[l][(blk * 16u + (uint32_t)k) % (uint32_t)len] & ST_ON)) main = l;
            }
            uint16_t bg = k == cur ? C_WHITE : main >= 0 ? C_DDGREY : C_BLACK;
            gfx_fill(g, x, ty, 13, 22, bg);
            if (main >= 0) { snprintf(b, sizeof b, "%d", main + 1); gfx_small(g, x + 7 - gfx_small_w(b) / 2, ty + 9, b, k == cur ? C_BLACK : C_GREY); }
            else gfx_fill(g, x + 5, ty + 10, 2, 2, C_DGREY);
            uint8_t pl = st->gen_sl[k], fx = st->gen_fx[k];
            bool changed = pl != 0xFF && pl != main;
            gfx_fill(g, x, sy, 13, 22, k == cur ? C_WHITE : pl != 0xFF ? (changed ? col : C_DDGREY) : C_BLACK);
            uint16_t fg = k == cur || changed ? C_BLACK : C_WHITE;
            if (pl != 0xFF) { snprintf(b, sizeof b, "%d", pl + 1); gfx_small(g, x + 7 - gfx_small_w(b) / 2, sy + 9, b, fg); }
            else if (fx & GEN_SKIP) gfx_text(g, x + 4, sy + 7, "-", C_RED, 1);
            int rt = fx & GEN_RT_MASK;
            if (rt) gfx_fill(g, x + 1, sy + 19, 11, 2, C_ORANGE);
            if (fx & GEN_REV) gfx_small(g, x + 1, sy + 1, "<", fg);
            if (fx & GEN_HALF) gfx_small(g, x + 9, sy + 1, "H", fg);
        }
    }
    hint_s(g, PY + 88, "VARY: OTHER PADS - ANCHOR: THE BEATS STAY");
    hint_s(g, PY + 96, "REPEAT: RETRIGS - BREAK: REVERSE, HALF, SILENCE");
    hint_s(g, PY + 104, "ORANGE = REPEAT - < = REVERSE - H = HALF SPEED");
    hint_s(g, PY + 118, "HOLD EDIT + BLACK 4 = CAPTURE INTO THE GRID");
}

/* ------------------------------------------------------------ editor de sonido por páginas
 * Arriba el preset (o el instrumento) y la página; en el medio un dibujo de lo que hace la página;
 * los 4 valores están en la leyenda de las perillas. */
static float ed_osc(int w, float forma, float p)          /* una forma de onda de ANALOG, p 0..1 */
{
    float saw = 1.0f - 2.0f * p;
    switch (w) {
    case AN_WAVE_SQR: return p < 0.5f - 0.45f * forma ? 1.0f : -1.0f;
    case AN_WAVE_TRI: { float tri = p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p; return tri * (1.0f - forma) + saw * forma; }
    case AN_WAVE_SIN: {
        float s = sinf(6.2831853f * p);
        return forma > 0.0f ? sinf(6.2831853f * 0.25f * (1.0f + 4.0f * forma) * s) : s;
    }
    case AN_WAVE_SUPER: return (saw + 0.75f * (1.0f - 2.0f * fmodf(p * 1.02f + 0.3f, 1.0f)) + 0.75f * (1.0f - 2.0f * fmodf(p * 0.98f + 0.6f, 1.0f))) * 0.4f;
    default: return saw * (1.0f - forma) + (p < 0.5f ? 1.0f : -1.0f) * forma;
    }
}

static void ed_wave(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h, int page)
{
    const uint8_t *q = a->pat.prm[t];
    float forma = a->pat.trk[t].macro[3] / 127.0f;
    int w1 = q[AN_W1] % AN_WAVES, w2 = q[AN_W2] % 4;
    float r2 = exp2f((float)(((int)q[AN_SEMI2] - 64) * 24 / 63) / 12.0f);
    float m2 = q[AN_MIX2] / 127.0f, ring = q[AN_RING] / 127.0f, sub = q[AN_SUB] / 127.0f;
    int py = -1, py2 = -1, mid = y0 + h / 2;
    uint16_t col = bm_track_color(t);
    gfx_hline(g, x0, mid, w, C_DDGREY);
    for (int x = 0; x < w; x++) {
        float p = (float)x * 2.0f / (float)w, ph = p - (float)(int)p;
        float s1 = ed_osc(w1, forma, ph);
        float p2 = q[AN_SYNC] ? ph * r2 : p * r2;
        float s2 = ed_osc(w2, 0.0f, p2 - (float)(int)p2);
        float y;
        if (page == AN_PG_OSC1) y = s1 + sub * (p < 1.0f ? 0.5f : -0.5f);
        else if (page == AN_PG_OSC2) y = s2;
        else y = (s1 + m2 * s2 + ring * s1 * s2 + sub * (p < 1.0f ? 0.5f : -0.5f)) / (1.0f + m2 + ring * 0.5f + sub * 0.5f);
        int yy = mid - (int)(bm_clampf(y, -1.2f, 1.2f) * (float)(h / 2 - 3));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, yy, col);
        py = yy;
        if (page == AN_PG_OSC2) {                                  /* la onda 1 de fondo */
            int y1 = mid - (int)(s1 * (float)(h / 2 - 3));
            if (py2 >= 0) gfx_line(g, x0 + x - 1, py2, x0 + x, y1, C_DGREY);
            py2 = y1;
        }
    }
    if (page == AN_PG_OSC1 && q[AN_NOISE]) gfx_small(g, x0 + w - 40, y0 + 2, "+ NOISE", C_GREY);
}

static float ed_filter_db(int type, float w, float qv)    /* respuesta del SVF, w = f / corte */
{
    float w2 = w * w;
    float d1 = sqrtf((1.0f - w2) * (1.0f - w2) + w2 / (qv * qv));
    float qb = 0.5f + 0.02f * qv, d2 = sqrtf((1.0f - w2) * (1.0f - w2) + w2 / (qb * qb));
    float m;
    switch (type) {
    case AN_F_LP12: m = 1.0f / d1; break;
    case AN_F_BP: m = (w / qv) / d1 * 1.5f; break;
    case AN_F_HP: m = w2 / d1; break;
    default: m = 1.0f / (d1 * d2); break;
    }
    return 20.0f * log10f(m > 1e-6f ? m : 1e-6f);
}

static void ed_filter(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const bm_track *tk = &a->pat.trk[t];
    const uint8_t *q = a->pat.prm[t];
    float fc = bm_cut_hz(tk->macro[0]), qv = bm_reso_q(tk->macro[1]);
    int type = q[AN_FTYPE] % AN_FILTERS;
    uint16_t col = bm_track_color(t);
    float env = (float)((int)tk->macro[2] - 64) * (84.0f / 63.0f);       /* semitonos que barre la envolvente */
    if (env != 0.0f) {                                                     /* hasta dónde llega la envolvente */
        float fe = fc * exp2f(env / 12.0f);
        int xa = x0 + (int)((log2f(bm_clampf(fc, 20.0f, 20000.0f) / 20.0f)) / 10.0f * (float)w);
        int xb = x0 + (int)((log2f(bm_clampf(fe, 20.0f, 20000.0f) / 20.0f)) / 10.0f * (float)w);
        gfx_fill(g, xa < xb ? xa : xb, y0 + h - 4, (xa < xb ? xb - xa : xa - xb) + 1, 3, C_YELLOW);
    }
    int y0db = y0 + h * 18 / 54;                                           /* 0 dB: arriba +18, abajo -36 */
    gfx_hline(g, x0, y0db, w, C_DDGREY);
    int py = -1;
    for (int x = 0; x < w; x++) {
        float f = 20.0f * exp2f((float)x * 10.0f / (float)w);
        float db = bm_clampf(ed_filter_db(type, f / fc, qv), -36.0f, 18.0f);
        int y = y0 + (int)((18.0f - db) / 54.0f * (float)(h - 6));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, y, col);
        py = y;
    }
    for (int k = 0; k < 4; k++) {                                          /* 100 Hz, 1 k, 10 k */
        static const float F[3] = {100.0f, 1000.0f, 10000.0f};
        static const char *const N[3] = {"100", "1K", "10K"};
        if (k == 3) break;
        int x = x0 + (int)(log2f(F[k] / 20.0f) / 10.0f * (float)w);
        gfx_vline(g, x, y0 + h - 8, 3, C_DGREY);
        gfx_small(g, x - 4, y0 + h - 4 - 8, N[k], C_DGREY);
    }
}

static void ed_adsr(bm_gfx *g, int x0, int y0, int w, int h, int A, int D, int S, int R, uint16_t col)
{
    /* tiempos en escala logarítmica, como las perillas */
    int a = 2 + A * (w / 4) / 127, d = 2 + D * (w / 4) / 127, r = 2 + R * (w / 4) / 127, s = w - a - d - r - 4;
    int yb = y0 + h - 2, sy = yb - S * (h - 4) / 127;
    int x1 = x0 + a, x2 = x1 + d, x3 = x2 + (s > 8 ? s : 8), x4 = x3 + r;
    for (int o = 0; o < 2; o++) {
        gfx_line(g, x0, yb + o, x1, y0 + 2 + o, bm_knob_color(0));
        gfx_line(g, x1, y0 + 2 + o, x2, sy + o, bm_knob_color(1));
        gfx_line(g, x2, sy + o, x3, sy + o, bm_knob_color(2));
        gfx_line(g, x3, sy + o, x4, yb + o, bm_knob_color(3));
    }
    (void)col;
}

static void ed_lfo(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const uint8_t *q = a->pat.prm[t];
    int shape = q[AN_LSHAPE] % AN_LSHAPES, amt = q[AN_LAMT];
    int mid = y0 + h / 2, py = -1;
    uint32_t rng = 12345;
    float sh = 0.0f;
    gfx_hline(g, x0, mid, w, C_DDGREY);
    for (int x = 0; x < w; x++) {
        float p = (float)x * 3.0f / (float)w, ph = p - (float)(int)p, y;
        switch (shape) {
        case AN_L_SIN: y = sinf(6.2831853f * ph); break;
        case AN_L_TRI: y = 4.0f * fabsf(ph - 0.5f) - 1.0f; break;
        case AN_L_SAW: y = 1.0f - 2.0f * ph; break;
        case AN_L_SQR: y = ph < 0.5f ? 1.0f : -1.0f; break;
        default:
            if (x == 0 || (int)((float)(x - 1) * 3.0f / (float)w * 2.0f) != (int)(p * 2.0f)) sh = (float)(int32_t)(rng = rng * 1664525u + 1013904223u) / 2147483648.0f;
            y = sh;
            break;
        }
        int yy = mid - (int)(y * (float)(h / 2 - 3) * (0.25f + 0.75f * (float)amt / 127.0f));
        if (py >= 0) gfx_line(g, x0 + x - 1, py, x0 + x, yy, amt ? bm_track_color(t) : C_DGREY);
        py = yy;
    }
}

static void draw_editor(bm_gfx *g, const bm_app *a)
{
    int t = a->track, n;
    const bm_ppage *pg = bm_app_ed_pages(a, t, &n);
    if (!pg || !n) return;
    int cur = a->ed_page[t] % n, tr = bm_is_tr(&a->pat, t);
    const bm_ppage *P = &pg[cur];
    uint16_t col = bm_track_color(t);
    char b[32];
    if (gfx_strip_hit(g, PY, 9)) {
        if (tr) {
            const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->sel_lane[t] & 15];
            snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->sel_lane[t] & 15)), T(bm_drm_names[ln->model % DRM_COUNT]));
        } else snprintf(b, sizeof b, "%s", bm_sound_name(a, t));
        gfx_text(g, 4, PY, b, C_WHITE, 1);
        gfx_small(g, 168, PY + 1, "PRESETS = ANOTHER", C_GREY);
    }
    if (gfx_strip_hit(g, PY + 12, 16)) {
        gfx_text(g, 4, PY + 12, P->name, col, 2);
        for (int i = 0; i < n; i++) {                                   /* una marca por página */
            int x = 236 - (n - i) * 9;
            if (i == cur) gfx_fill(g, x, PY + 16, 7, 7, col); else gfx_rect(g, x, PY + 16, 7, 7, C_DGREY);
        }
    }
    int x0 = 6, y0 = PY + 32, w = 228, h = 92;
    if (gfx_strip_hit(g, y0, h)) {
        gfx_rect(g, x0 - 2, y0 - 2, w + 4, h + 4, C_DDGREY);
        if (tr) {
            const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->sel_lane[t] & 15];
            /* golpe: caída según DECAY y el tono base del modelo, con AFINA */
            int dec = 6 + ln->decay * (w - 12) / 127, mid = y0 + h / 2;
            gfx_hline(g, x0, mid, w, C_DDGREY);
            int py = -1, pyn = -1;
            for (int x = 0; x < dec && x < w; x++) {
                float e = expf(-4.0f * (float)x / (float)dec);
                float hz = (float)bm_drm_base_hz[ln->model % DRM_COUNT] * exp2f((float)(ln->tune - 64) * 0.375f / 12.0f);
                float cyc = hz / 300.0f;
                float s = sinf((float)x * 0.12f * cyc) * e;
                int yy = mid - (int)(s * (float)(h / 2 - 4)), yn = mid + (int)(s * (float)(h / 2 - 4));
                if (py >= 0) { gfx_line(g, x0 + x - 1, py, x0 + x, yy, col); gfx_line(g, x0 + x - 1, pyn, x0 + x, yn, dim(col)); }
                py = yy; pyn = yn;
            }
            float hz = (float)bm_drm_base_hz[ln->model % DRM_COUNT] * exp2f((float)(ln->tune - 64) * 0.375f / 12.0f);
            snprintf(b, sizeof b, "%d HZ", (int)(hz + 0.5f));
            gfx_small(g, x0 + w - gfx_small_w(b) - 2, y0 + 2, b, C_GREY);
        } else if (cur == AN_PG_OSC1 || cur == AN_PG_OSC2 || cur == AN_PG_CROSS) ed_wave(g, a, t, x0, y0, w, h, cur);
        else if (cur == AN_PG_FILTER || cur == AN_PG_FILTER2) ed_filter(g, a, t, x0, y0, w, h);
        else if (cur == AN_PG_FILT || cur == AN_PG_AMP) {
            const uint8_t *q = a->pat.prm[t];
            int o = cur == AN_PG_FILT ? AN_FA : AN_AA;
            ed_adsr(g, x0, y0, w, h, q[o], q[o + 1], q[o + 2], q[o + 3], col);
        } else if (cur == AN_PG_LFO) ed_lfo(g, a, t, x0, y0, w, h);
        else {
            const uint8_t *q = a->pat.prm[t];
            static const char *const M[AN_MODES] = {"POLY: UP TO 7 VOICES", "MONO: ONE VOICE, RETRIGGERS", "LEGATO: ONE VOICE, TIED"};
            gfx_text_c(g, 120, y0 + 30, M[q[AN_MODE] % AN_MODES], C_WHITE, 1);
            if (t == 2) gfx_text_c(g, 120, y0 + 44, "(T3 IS ALWAYS ONE VOICE)", C_GREY, 1);
        }
    }
    hint_s(g, PY + 130, tr ? "SELECT = PAGE - BLACKS 1-8 + 9 = INSTRUMENT" : "SELECT = PAGE - EDIT = NEXT");
    hint_s(g, PY + 138, tr ? "EDIT = NEXT PAGE - WHITES = PLAY" : "ENV = ENVELOPES - LFO = LFO - KEYS = PLAY");
}

static void draw_sound(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[30];
    if (bm_is_tr(&a->pat, t)) {
        if (gfx_strip_hit(g, PY, 9)) {
            gfx_text(g, 4, PY, "KIT", C_GREY, 1); gfx_text(g, 28, PY, bm_sound_name(a, t), C_WHITE, 1);
            gfx_small(g, 80, PY + 1, "MODELED", C_YELLOW);
            gfx_small(g, 150, PY + 1, "PRESETS = ANOTHER KIT", C_GREY);
        }
        lane_pads(g, a, t, PY + 12, 37);
        return;
    }
    if (tr->engine == ENG_FM) {
        if (gfx_strip_hit(g, PY, 9)) {
            gfx_text(g, 4, PY, "FM", C_GREY, 1);
            gfx_text(g, 24, PY, bm_sound_name(a, t), C_WHITE, 1);
            gfx_small(g, 150, PY + 1, "PRESETS = PATCH", C_GREY);
        }
        if (gfx_strip_hit(g, PY + 12, 92)) draw_fm(g, a, t, 4, PY + 12, 232, 90);
        char c0[12], c1[12], c3[12];
        bm_fmt_macro(a, t, 0, tr->macro[0], c0, sizeof c0);
        bm_fmt_macro(a, t, 1, tr->macro[1], c1, sizeof c1);
        bm_fmt_macro(a, t, 3, tr->macro[3], c3, sizeof c3);
        snprintf(b, sizeof b, TF("BRIGHT %s  %s  %s"), c0, c1, c3);
        hint(g, 150, b);
        hint(g, 162, "* THE PATCH ALGORITHM");
        hint(g, 176, t == 2 ? "MONO + GLIDE (TIE)" : "POLYPHONIC (POOL OF 7 VOICES)");
        return;
    }
    if (tr->engine == ENG_ANALOG) { draw_editor(g, a); return; }
    if (gfx_strip_hit(g, PY, 9)) {
        gfx_text(g, 4, PY, bm_track_name(a, t), C_GREY, 1);
        gfx_text(g, 52, PY, bm_sound_name(a, t), C_WHITE, 1);
        gfx_small(g, 150, PY + 1, "PRESETS = TIMBRE", C_GREY);
    }
    if (gfx_strip_hit(g, PY + 12, 92)) draw_synth(g, a, t, 4, PY + 12, 232, 90);
    char c0[12], c1[12];
    bm_fmt_macro(a, t, 0, tr->macro[0], c0, sizeof c0);
    snprintf(b, sizeof b, TF("LP 4 POLE %s  RES %d%%"), c0, tr->macro[1] * 100 / 127);
    hint(g, 150, b);
    bm_fmt_macro(a, t, 3, tr->macro[3], c1, sizeof c1);
    snprintf(b, sizeof b, "%s %s", T(bm_macro_name(a, t, 3)), c1);
    hint(g, 162, b);
    hint(g, 176, "ACCENT OPENS THE FILTER - TIE = SLIDE");
}

static void draw_inst(bm_gfx *g, const bm_app *a)
{
    draw_editor(g, a);
}

static void draw_ch_mix(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    static const char *const NM[4] = {"LEVEL", "PAN", "SEND", "DUCK"};
    const int val[4] = {tr->level, tr->pan, tr->send, tr->duck};
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 58;
        if (!gfx_strip_hit(g, PY + 4, 100)) break;
        gfx_text(g, x, PY + 4, NM[i], bm_knob_color(i), 1);
        gfx_fill(g, x + 10, PY + 16, 20, 80, C_DDGREY);
        if (i == 1) { int c = x + 20, p = (val[1] - 64) * 10 / 64; gfx_fill(g, p < 0 ? c + p : c, PY + 16, p < 0 ? -p : (p ? p : 1), 80, bm_knob_color(1)); continue; }
        if (i == 3 && t == 0) { gfx_small(g, x + 4, PY + 50, "SOURCE", C_GREY); continue; }
        int h = val[i] * 80 / 127;
        gfx_fill(g, x + 10, PY + 96 - h, 20, h, bm_knob_color(i));
    }
    int pk = a->ctx.st.peak[t] * 228 / 255;
    if (gfx_strip_hit(g, PY + 104, 8)) { gfx_fill(g, 6, PY + 104, 228, 6, C_DDGREY); gfx_fill(g, 6, PY + 104, pk, 6, C_GREEN); }
    hint(g, 162, "SEND = TO THE DELAY - DUCK = T1 SIDECHAIN");
    hint(g, 174, "HOME + FX = DELAY AND SIDECHAIN");
}

static void draw_more(bm_gfx *g, const bm_app *a)
{
    int n = bm_app_more_count(a);
    char v[24];
    for (int i = 0; i < n; i++) {
        const char *nm;
        bm_app_more_row(a, i, &nm, v, sizeof v);
        row(g, PY + 2 + i * 17, i == a->more_row, nm, v);
    }
    hint(g, 172, "SELECT = ROW - ALGORITHM = CHANGE");
}

/* TAKES: las tomas guardadas (de dónde salen los sonidos de los pads), con la onda de la marcada. */
static void draw_takes(bm_gfx *g, const bm_app *a)
{
    uint8_t list[BM_TAKES + 8];
    int n = bm_app_sources(a, list);
    int first = a->takes_row > 3 ? a->takes_row - 3 : 0;
    char b[30];
    if (!n && gfx_strip_hit(g, PY + 20, 30)) {
        gfx_text_c(g, 120, PY + 24, "NO TAKES YET", C_WHITE, 1);
        gfx_text_c(g, 120, PY + 38, "REC = RECORD ONE", C_GREY, 1);
    }
    for (int i = 0; i < 4 && first + i < n; i++) {
        int y = PY + 1 + i * 14, e = first + i;
        if (!gfx_strip_hit(g, y, 14)) continue;
        uint8_t s = list[e];
        bool sel = e == a->takes_row;
        if (sel) gfx_fill(g, 0, y, 240, 13, C_DGREY);
        bm_app_src_name(a, s, b, sizeof b);
        gfx_text(g, 4, y + 3, b, sel ? C_WHITE : C_GREY, 1);
        if (s & BM_SRC_ROM) continue;
        unsigned ds = (unsigned)(a->take[s].len * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("%d BPM %d BAR %u.%uS"), (a->take[s].bpm_x10 + 5) / 10, a->take[s].bars, ds / 10, ds % 10);
        gfx_small(g, 232 - gfx_small_w(b), y + 4, b, C_GREY);
        int pads = 0;
        for (int k = 0; k < BM_LANES; k++) pads += bm_pad_ok(&a->pat.pad[k]) && a->pat.pad[k].src == s;
        if (pads) gfx_disc(g, 236, y + 6, 2, bm_track_color(1));         /* el kit usa esta toma */
        if ((a->take_dirty >> s) & 1) gfx_disc(g, 236, y + 6, 2, C_ORANGE);
    }
    uint8_t src = bm_app_takes_sel(a);
    if (src < BM_TAKES && a->take[src].valid && gfx_strip_hit(g, PY + 60, 40)) {   /* la toma marcada: 16 partes */
        int y = PY + 60, h = 38;
        gfx_fill(g, 4, y, 232, h, C_DDGREY);
        for (int x = 0; x < 232; x++) {
            int pk = a->wave_ov[x] * (h / 2 - 1) / 255;
            gfx_vline(g, 4 + x, y + h / 2 - pk, 2 * pk + 1, bm_track_color(1));
        }
        for (int k = 1; k < 16; k++) gfx_vline(g, 4 + k * 232 / 16, y + h - 4, 4, C_GREY);
        const bm_status *st = &a->ctx.st;
        if (st->smp_pos_src == 0 && st->smp_pos < a->take[src].len)        /* escucha (la voz de escucha usa su toma) */
            gfx_vline(g, 4 + (int)((uint64_t)st->smp_pos * 231u / a->take[src].len), y, h, C_ORANGE);
    }
    /* memoria de tomas */
    uint32_t total = (uint32_t)(BM_TAKE_HI - BM_TAKE_LO), freeb = bm_takes_free(a->take);
    if (gfx_strip_hit(g, PY + 102, 14)) {
        unsigned fs = (unsigned)(freeb * 10u / BM_SMP_RATE), ts = (unsigned)(total * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("FREE %u.%u S OF %u.%u S"), fs / 10, fs % 10, ts / 10, ts % 10);
        gfx_small(g, 4, PY + 102, b, C_GREY);
        gfx_fill(g, 4, PY + 110, 232, 3, C_DDGREY);
        gfx_fill(g, 4, PY + 110, (int)((total - freeb) * 232u / total), 3, C_GREY);
    }
    snprintf(b, sizeof b, TF("N4 = THE TAKE ON PAD %d"), (a->sel_lane[1] & 15) + 1);
    hint_s(g, PY + 118, "N1 HITS - N2 16 - N3 8: A KIT FROM THIS TAKE");
    hint_s(g, PY + 126, b);
    hint_s(g, PY + 134, "WHITE = LISTEN (HELD PLAYS ON) - ALGORITHM = PICK");
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
    if (gfx_strip_hit(g, PY, 14)) {
        int w = gfx_text_w(lbl, 1) + 8;
        gfx_fill(g, 4, PY, w, 12, lc);
        gfx_text(g, 8, PY + 2, lbl, C_BLACK, 1);
        gfx_text_r(g, 236, PY + 2, "SOURCE: MIX", C_GREY, 1);
    }
    uint32_t show = st->smp_unsaved && !st->smp_rec ? used : (room > used ? room - used : 0);
    unsigned ds = (unsigned)(show * 10u / BM_SMP_RATE);
    if (gfx_strip_hit(g, PY + 18, 40)) {
        snprintf(b, sizeof b, "%u.%u", ds / 10, ds % 10);
        int w = gfx_text(g, 8, PY + 18, b, big, 5);
        gfx_text(g, 16 + w, PY + 20, "S", C_GREY, 2);
        gfx_text(g, 16 + w, PY + 40, st->smp_unsaved && !st->smp_rec ? "RECORDED" : st->smp_rec ? "LEFT" : "AVAILABLE", C_GREY, 1);
    }
    if (gfx_strip_hit(g, PY + 56, 8)) {                        /* cuánto cabe en la biblioteca */
        unsigned fs = (unsigned)(bm_takes_free(a->take) * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("TAKES: FREE %u.%u S (%u KB)"), fs / 10, fs % 10, (unsigned)(bm_takes_free(a->take) / 1024u));
        gfx_small(g, 4, PY + 56, b, C_GREY);
        gfx_small(g, 236 - gfx_small_w("MAX 3.0 S PER TAKE"), PY + 56, "MAX 3.0 S PER TAKE", C_GREY);
    }
    /* barra: lo grabado, rayas por compás y el tope */
    int y = PY + 64;
    if (gfx_strip_hit(g, y, 22)) {
        gfx_fill(g, 4, y, 232, 14, C_DDGREY);
        uint32_t cap = BM_SMP_REC_MAX;
        int uw = (int)((uint64_t)used * 232u / cap);
        gfx_fill(g, 4, y, uw, 14, st->smp_rec ? C_RED : st->smp_unsaved ? C_GREEN : C_DGREY);
        uint32_t bar = (uint32_t)BM_SMP_RATE * 2400u / a->pat.meta.bpm_x10;
        for (uint32_t k = 1; k * bar < cap; k++) gfx_vline(g, 4 + (int)(k * bar * 232u / cap), y, 14, C_WHITE);
        int cx = 4 + (int)((uint64_t)room * 232u / cap);
        gfx_fill(g, cx - 1, y - 3, 2, 20, C_YELLOW);
        unsigned bs = (unsigned)(bar * 10u / BM_SMP_RATE);
        snprintf(b, sizeof b, TF("1 BAR = %u.%u S"), bs / 10, bs % 10);
        gfx_small(g, 4, y + 17, b, C_GREY);
        gfx_small(g, 236 - gfx_small_w(room < cap ? "LIMIT: MEMORY" : "LIMIT: RECORDING RAM"), y + 17, room < cap ? "LIMIT: MEMORY" : "LIMIT: RECORDING RAM", C_YELLOW);
    }
    if (st->smp_rec) { hint(g, 154, "REC = STOP"); hint(g, 166, "STOPS BY ITSELF AT THE LIMIT"); hint_s(g, 176, "DELAY AND PUNCH-IN PAUSED WHILE RECORDING"); }
    else if (st->smp_armed) { hint(g, 154, "STARTS ON THE BAR"); hint(g, 166, "REC AGAIN = CANCEL"); hint_s(g, 176, "KEYS ARE SILENT IN THE RECORDER"); }
    else if (st->smp_unsaved) { hint(g, 154, "IT SAVES WHEN YOU STOP"); hint(g, 166, "WHITES = LISTEN TO IT"); hint_s(g, 176, "HOME = DISCARD - REC = ANOTHER TAKE"); }
    else { hint(g, 154, "REC = ARM (K1: BAR OR NOW)"); hint(g, 166, "HOLD REC = RECORD WHILE HELD"); hint_s(g, 176, "SAVED TAKES BUILD THE KIT (TAKES)"); }
}

/* ------------------------------------------------------------ ENV, LFO, FX */
static void draw_env(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    uint16_t col = bm_track_color(t);
    char b[24];
    if (gfx_strip_hit(g, PY, 9)) { snprintf(b, sizeof b, TF("PAGE %d/2 (ENV AGAIN)"), a->env_page + 1); gfx_text(g, 4, PY, b, C_GREY, 1); }
    int x0 = 8, y0 = PY + 16, h = 90, yb = y0 + h;
    if (gfx_strip_hit(g, PY + 12, 100)) gfx_rect(g, 4, PY + 12, 232, 98, C_DDGREY);
    int A = 4 + tr->attack * 50 / 127, D = 6 + tr->macro[2] * 60 / 127, S = 50, R = 6 + tr->release * 60 / 127;
    int sy = yb - tr->sustain * h / 127;
    int x1 = x0 + A, x2 = x1 + D, x3 = x2 + S, x4 = x3 + R;
    for (int o = 0; o < 2; o++) {
        gfx_line(g, x0, yb + o, x1, y0 + o, bm_knob_color(0));
        gfx_line(g, x1, y0 + o, x2, sy + o, bm_knob_color(1));
        gfx_line(g, x2, sy + o, x3, sy + o, bm_knob_color(2));
        gfx_line(g, x3, sy + o, x4, yb + o, bm_knob_color(3));
    }
    (void)col;
    if (a->env_page == 1) {
        snprintf(b, sizeof b, TF("ENV > FILTER %d%%"), tr->env_amt * 100 / 127);
        hint(g, 160, b);
        snprintf(b, sizeof b, TF("GLIDE %dMS (WITH TIE)"), tr->glide * 500 / 127);
        hint(g, 172, b);
    } else hint(g, 166, "HOLD ENV = PAINT ACCENTS");
}

static void draw_lfo(bm_gfx *g, const bm_app *a)
{
    (void)a;
    gfx_text_c(g, 120, 80, "LFO", C_WHITE, 3);
    gfx_text_c(g, 120, 110, "IN A FUTURE VERSION", C_GREY, 1);
    hint(g, 150, "HOLD LFO = PAINT REPEATS");
    hint(g, 162, "(ON NOTES: TIES)");
}

static void draw_fx(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    static const char *const DT[4] = {"1/16", "1/8", "3/16", "1/4"};
    char b[24];
    header(g, "GLOBAL FX", "HOME + FX", C_WHITE);
    gfx_rect(g, 4, 52, 114, 70, C_BLUE);
    gfx_text(g, 10, 57, "DELAY", C_BLUE, 1);
    gfx_text(g, 10, 72, DT[m->dly_time & 3], C_WHITE, 2);
    snprintf(b, sizeof b, "FB %d%%", m->dly_fb * 100 / 127);
    gfx_text(g, 10, 94, b, C_GREEN, 1);
    snprintf(b, sizeof b, "MIX %d%%", m->dly_mix * 100 / 127);
    gfx_text(g, 10, 106, b, C_YELLOW, 1);
    gfx_rect(g, 122, 52, 114, 70, C_ORANGE);
    gfx_text(g, 128, 57, "SIDECHAIN", C_ORANGE, 1);
    gfx_text(g, 128, 69, "T1 > T2-T6", C_GREY, 1);
    for (int t = 1; t < BM_TRACKS; t++) {
        int x = 128 + (t - 1) * 21;
        gfx_fill(g, x, 82, 15, 32, C_DDGREY);
        int h = a->pat.trk[t].duck * 32 / 127;
        gfx_fill(g, x, 114 - h, 15, h, bm_track_color(t));
    }
    if (a->ctx.arena_busy) { gfx_fill(g, 4, 52, 114, 70, C_DDGREY); gfx_text(g, 10, 80, "PAUSED:", C_ORANGE, 1); gfx_text(g, 10, 92, "TAKE NOT", C_ORANGE, 1); gfx_text(g, 10, 104, "SAVED YET", C_ORANGE, 1); }
    snprintf(b, sizeof b, "DRIVE %d%%  (HOME + K4)", a->set.drive * 100 / 127);
    gfx_text(g, 6, 134, b, C_WHITE, 1);
    hint(g, 152, "HOLD FX = PUNCH-IN");
    hint(g, 164, "SEND AND DUCK: FX ON EACH TRACK");
}

/* ------------------------------------------------------------ escala y acordes */
static void draw_keyboard(bm_gfx *g, const bm_app *a, int ky, int kh)
{
    int t = a->track, root, sc = 0;
    sc = app_scale(a, t, &root);
    int base_oct = (t == 2 ? 2 : 3) + a->oct;
    char b[12];
    if (!gfx_strip_hit(g, ky, kh + 2)) return;
    int n = bm_scale_len(sc);
    for (int i = 0; i < BM_WHITES; i++) {
        int x = i * 15, k = bm_white_key(i);
        uint16_t c = (i % n == 0) ? C_YELLOW : C_WHITE;
        if (a->keys_down[k]) c = bm_track_color(t);
        gfx_fill(g, x, ky, 14, kh, c);
        int note = bm_scale_note(root, sc, i, base_oct);
        snprintf(b, sizeof b, "%s", bm_note_names[note % 12]);
        gfx_small(g, x + 7 - gfx_small_w(b) / 2, ky + kh - 16, b, C_BLACK);
        snprintf(b, sizeof b, "%d", note / 12 - 1);
        gfx_small(g, x + 6, ky + kh - 8, b, C_DGREY);
    }
    for (int i = 0; i < BM_BLACKS; i++) {
        int k = bm_black_key(i);
        int wi = bm_key_white_index(k - 1);
        int x = wi * 15 + 9;
        uint16_t c = a->keys_down[k] ? bm_track_color(t) : C_DDGREY;
        gfx_fill(g, x, ky, 11, kh * 6 / 10, c);
        gfx_rect(g, x, ky, 11, kh * 6 / 10, C_BLACK);
        if (i < 7) {
            char nm[10];
            bm_chord_name(root, sc, i, 0, nm, sizeof nm);
            nm[3] = 0;
            gfx_small(g, x + 1, ky + kh * 6 / 10 - 9, nm, a->keys_down[k] ? C_BLACK : C_WHITE);
        } else {
            static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
            gfx_small(g, x + 2, ky + kh * 6 / 10 - 9, MODS[i - 7], a->keys_down[k] ? C_BLACK : C_YELLOW);
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
    int y = project ? 30 : PY;
    if (gfx_strip_hit(g, y, 26)) {
        gfx_text(g, 6, y, project ? "PROJECT SCALE" : (tr->scl == 0xFF ? "SCALE (FROM THE PROJECT)" : "OWN TRACK SCALE"), C_GREY, 1);
        snprintf(b, sizeof b, "%s %s", bm_note_names[root % 12], T(bm_scale_names[sc % BM_SCALES]));
        gfx_text(g, 6, y + 11, b, C_WHITE, 2);
        if (a->chord_name[0]) gfx_text_r(g, 234, y + 11, a->chord_name, C_YELLOW, 2);
    }
    if (app_kind(a, a->track) == KIND_NOTE || project) draw_keyboard(g, a, y + 32, 84);
    else gfx_text_c(g, 120, 100, "PICK A NOTE TRACK", C_GREY, 1);
    hint_s(g, 174, project ? "FOR TRACKS WITHOUT THEIR OWN SCALE" : "SCL AGAIN: CHORDS - HOME + SCL: PROJECT");
}

static void draw_chords(bm_gfx *g, const bm_app *a, int y0)
{
    const bm_meta *m = &a->pat.meta;
    char b[24];
    int t = bm_app_chord_track(a), root, sc = app_scale(a, t, &root);
    if (gfx_strip_hit(g, y0, 10)) {
        snprintf(b, sizeof b, "%s %.5s T%d", bm_note_names[root % 12], T(bm_scale_names[sc % BM_SCALES]), t + 1);
        gfx_text(g, 4, y0, b, C_WHITE, 1);
        if (a->last_pad >= 0) gfx_text_r(g, 236, y0, a->chord_name, C_WHITE, 1);
    }
    static const uint16_t FN[3] = {C_BLUE, C_GREEN, C_ORANGE};
    for (int p = 0; p < 16; p++) {
        int x = (p % 4) * 60, y = y0 + 11 + (p / 4) * 33;
        if (!gfx_strip_hit(g, y, 32)) continue;
        bm_pad_chord pc = bm_chord_pad(m->chord_set, p);
        uint16_t fc = FN[bm_chord_function(pc.degree)];
        int k = bm_white_key(p);
        bool on = a->keys_down[k];
        bool last = p == a->last_pad;
        gfx_fill(g, x + 1, y, 58, 31, on ? fc : (last ? dim(fc) : C_DDGREY));
        gfx_fill(g, x + 1, y, 3, 31, fc);
        char rn[10], nm[10];
        bm_roman(root, sc, pc.degree, pc.ext, rn, sizeof rn);
        bm_chord_name(root, sc, pc.degree, pc.ext, nm, sizeof nm);
        gfx_text(g, x + 8, y + 5, rn, on ? C_BLACK : C_WHITE, 1);
        gfx_text(g, x + 8, y + 18, nm, on ? C_BLACK : fc, 1);
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
        gfx_text_c(g, 120, 80, "ROLL", bm_track_color(t), 3);
        gfx_text_c(g, 120, 110, "HOLD ARP + WHITE", C_WHITE, 1);
        snprintf(b, sizeof b, TF("SPEED %s (K1)"), bm_roll_names[a->roll_rate % 5]);
        gfx_text_c(g, 120, 124, b, C_GREY, 1);
        return;
    }
    gfx_text(g, 6, PY + 6, M[tr->arp_mode % 5], C_WHITE, 3);
    snprintf(b, sizeof b, "%s  %d OCT  GATE %d%%", R[tr->arp_rate % 3], tr->arp_oct, tr->arp_gate);
    hint(g, PY + 36, b);
    const bm_arp *ar = &a->ctx.arp[t];
    for (int i = 0; i < 8; i++) {
        int x = 10 + i * 28, h = 10 + i * 6;
        bool has = i < ar->n;
        if (gfx_strip_hit(g, 160 - h, h)) gfx_fill(g, x, 160 - h, 22, h, has ? (ar->playing && ar->cur % 12 == ar->held[i] % 12 ? C_WHITE : bm_track_color(t)) : C_DDGREY);
    }
    hint(g, 172, tr->arp_mode ? "ALWAYS ON IN THIS TRACK" : "HOLD ARP = LIVE ARPEGGIO");
}

static void draw_loop(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_loop *lp = &a->pat.loop[t - 2];
    int mode = a->ctx.st.loop_mode[t - 2];
    static const char *const MN[5] = {"OFF", "ARMED", "RECORDING", "PLAY", "OVERDUB"};
    static const uint16_t MC[5] = {C_GREY, C_YELLOW, C_RED, C_GREEN, C_ORANGE};
    char b[30];
    gfx_text(g, 6, PY + 4, MN[mode % 5], MC[mode % 5], 3);
    snprintf(b, sizeof b, TF("%d BARS  %d/%d NOTES  %s"), lp->bars, lp->n, BM_LOOP_EVENTS, lp->on ? "ON" : "OFF");
    hint(g, PY + 32, b);
    int bx = 6, bw = 228, by = PY + 46, bh = 34;
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
    hint(g, 140, "REC: RECORD > PLAY > OVERDUB");
    hint(g, 152, "STARTS ON THE NEXT BAR");
    hint(g, 164, "REC 2 S: ERASE THE LOOP");
}

static void draw_midi(bm_gfx *g, const bm_app *a)
{
    const bm_track *tr = &a->pat.trk[a->track];
    char b[24];
    snprintf(b, sizeof b, TF("CHANNEL %d"), tr->midi_ch + 1);
    gfx_text(g, 6, PY + 6, b, C_WHITE, 3);
    snprintf(b, sizeof b, TF("PROGRAM %d"), tr->macro[1] + 1);
    hint(g, PY + 40, b);
    snprintf(b, sizeof b, "CC1 %d   CC74 %d", tr->macro[2], tr->macro[3]);
    hint(g, PY + 52, b);
    hint(g, 140, "NO INTERNAL SOUND:");
    hint(g, 152, "SENDS ITS NOTES OVER MIDI");
    hint(g, 164, "(USB AND TRS)");
}

/* ------------------------------------------------------------ globales */
static void draw_system(bm_gfx *g, const bm_app *a)
{
    const bm_settings *s = &a->set;
    char v[11][20];
    snprintf(v[0], 20, "%s", s->clock ? "EXTERNAL" : "INTERNAL");
    snprintf(v[1], 20, "%s", s->sync_mode ? "SPLIT L/R" : "STEREO");
    snprintf(v[2], 20, "%d PPQN", s->sync_ppqn);
    snprintf(v[3], 20, "%d MS", s->sync_lat);
    snprintf(v[4], 20, "%s", s->autosave ? "ON" : "OFF");
    snprintf(v[5], 20, "%s", s->led_dim == 0 ? "OFF" : (s->led_dim == 1 ? "DIM" : "MEDIUM"));
    int ms = bm_app_enter_ms(a);
    if (ms < 0) snprintf(v[6], 20, "OFF"); else snprintf(v[6], 20, "%d MS", ms);
    snprintf(v[7], 20, "%d/%d SLOTS", bm_store_used_slots(&a->store), (int)BM_STORE_SLOTS);
    int nt = 0; for (int i = 0; i < BM_TAKES; i++) nt += a->take[i].valid;
    unsigned fs = (unsigned)(bm_takes_free(a->take) * 10u / BM_SMP_RATE);
    snprintf(v[8], 20, TF("%d, FREE %u.%uS"), nt, fs / 10, fs % 10);
    snprintf(v[9], 20, "%s", bm_lang_name());
    snprintf(v[10], 20, "%s", BM_VERSION_STR);
    static const char *const L[11] = {"CLOCK", "SYNC OUT", "SYNC PPQN", "SYNC LATENCY", "AUTOSAVE", "IDLE LEDS", "AUTO ENTER", "PATTERNS", "TAKES", N_("LANGUAGE"), "VERSION"};
    for (int i = 0; i < 11; i++) row(g, 26 + i * 16, i == a->menu_row, T(L[i]), v[i]);
}

static void draw_glo(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    char v[7][16];
    snprintf(v[0], 16, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10);
    snprintf(v[1], 16, "%d%%", m->swing);
    if (m->master) snprintf(v[2], 16, TF("%d STEPS"), m->master * 16); else snprintf(v[2], 16, "INF");
    snprintf(v[3], 16, "%d%%", a->set.drive * 100 / 127);
    if (m->fill_every) snprintf(v[4], 16, TF("EVERY %d"), m->fill_every); else snprintf(v[4], 16, "OFF");
    snprintf(v[5], 16, "%+d ST", (int)m->transpose - 24);
    int ms = bm_app_enter_ms(a);
    if (ms < 0) snprintf(v[6], 16, "OFF"); else snprintf(v[6], 16, "%d MS", ms);
    static const char *const L[7] = {"BPM", "SWING", "MASTER LENGTH", "DRIVE", "AUTO FILL", "TRANSPOSE", "AUTO ENTER"};
    header(g, "GLOBAL", "GLO AGAIN = CLOSE", C_WHITE);
    for (int i = 0; i < 7; i++) row(g, 50 + i * 17, i == a->glo_row, L[i], v[i]);
    hint(g, 176, "SELECT ROW - ALGORITHM CHANGES");
    hint(g, 188, "HOLD GLO = TRANSPOSE, PLAY = TAP");
}

static void draw_save(bm_gfx *g, const bm_app *a)
{
    char r0[24], r1[24], r4[24];
    snprintf(r0, sizeof r0, "A%02d", a->ctx.st.pattern + 1);
    snprintf(r1, sizeof r1, "A%02d", a->save_target + 1);
    snprintf(r4, sizeof r4, "%d", bm_pattern_count_events(&a->pat));
    header(g, "SAVE", a->dirty ? "UNSAVED" : "SAVED", C_WHITE);
    row(g, 52, a->save_row == 0, "SAVE", r0);
    row(g, 70, a->save_row == 1, "SAVE AS", r1);
    row(g, 88, a->save_row == 2, "SAVE SONG AND SCENES", NULL);
    row(g, 106, a->save_row == 3, a->clear_armed ? "CLEAR? SAVE AGAIN" : "CLEAR PATTERN", NULL);
    row(g, 124, a->save_row == 4, "EVENTS", r4);
    hint(g, 146, "SELECT PICKS, SAVE RUNS");
    hint(g, 158, "HOLD SAVE = COPY PATTERN");
    hint(g, 170, "SAVE 2 S: QUICK SAVE");
    gfx_text(g, 6, 184, a->ctx.st.playing ? "ONLY WHEN STOPPED (CUTS AUDIO)" : "ONLY WHEN STOPPED", a->ctx.st.playing ? C_ORANGE : C_GREY, 1);
}

static void draw_song(bm_gfx *g, const bm_app *a)
{
    char b[24];
    header(g, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? C_YELLOW : C_WHITE);
    if (!a->song.n) {
        gfx_text_c(g, 120, 90, "EMPTY", C_GREY, 2);
        gfx_text_c(g, 120, 120, "WHITE = ADD PATTERN", C_GREY, 1);
        gfx_text_c(g, 120, 134, "OR HOME + REC: SONG REC", C_GREY, 1);
        return;
    }
    int first = a->song_cur > 6 ? a->song_cur - 6 : 0;
    for (int i = 0; i < 8 && first + i < a->song.n; i++) {
        int e = first + i;
        uint8_t p = a->song.pat[e];
        if (p & 0x80) snprintf(b, sizeof b, TF("%2d  SCN %c  x%d"), e + 1, 'A' + (p & 3), a->song.rep[e]);
        else snprintf(b, sizeof b, "%2d  A%02d    x%d", e + 1, p + 1, a->song.rep[e]);
        row(g, 50 + i * 16, e == a->song_cur, b, (a->ctx.st.song_on && a->ctx.st.playing && e == a->ctx.st.song_pos) ? ">" : NULL);
    }
    hint(g, 186, "WHITE ADDS - BLACK 9 DELETES");
}

static void draw_time(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    header(g, "TIME", "POLYMETER", C_WHITE);
    for (int t = 0; t < BM_TRACKS; t++) {
        int y = 50 + t * 24;
        if (!gfx_strip_hit(g, y, 22)) continue;
        const bm_track *tr = &a->pat.trk[t];
        bool sel = t == a->time_row;
        if (sel) gfx_fill(g, 0, y - 2, 240, 22, C_DDGREY);
        char b[16];
        snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text(g, 4, y + 4, b, bm_track_color(t), 1);
        int tps = bm_speed_tps[tr->speed % SPD_COUNT];
        int len = bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? (sel ? a->time_lane : 0) : -1);
        int w = len * tps * 48 / BM_BAR_TICKS;              /* 48 px por compás */
        if (w > 160) w = 160;
        if (w < len) w = len;
        int cur = st->playing ? (int)(st->tn[t] % (uint32_t)len) : -1;
        for (int s = 0; s < len; s++) {
            int x0 = 22 + s * w / len, x1 = 22 + (s + 1) * w / len;
            gfx_fill(g, x0, y + 2, x1 - x0 - (x1 - x0 > 2 ? 1 : 0), 12, s == cur ? C_WHITE : (s % 4 == 0 ? bm_track_color(t) : dim(bm_track_color(t))));
        }
        for (int bar = 1; bar * 48 < 160; bar++) gfx_vline(g, 22 + bar * 48, y, 16, C_GREY);
        snprintf(b, sizeof b, "%d %s", len, bm_speed_names[tr->speed % SPD_COUNT]);
        gfx_text_r(g, 236, y + 4, b, sel ? C_WHITE : C_GREY, 1);
    }
    if (bm_is_grid(&a->pat, a->time_row)) {
        char b[24];
        snprintf(b, sizeof b, "K3 INSTR: %s", T(bm_lane_name(a, a->time_row, a->time_lane)));
        hint(g, 190, b);
    } else hint(g, 190, "LINES = 1 BAR");
}

static void draw_scenes(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[24];
    header(g, "SCENES", a->song_rec ? "SONG REC" : "1-4 LAUNCH 5-8 STORE", a->song_rec ? C_RED : C_CYAN);
    for (int s = 0; s < BM_SCENES; s++) {
        int x = (s % 2) * 118 + 2, y = 50 + (s / 2) * 62;
        if (!gfx_strip_hit(g, y, 58)) continue;
        const bm_scene *sc = &a->song.scene[s];
        bool cur = st->scene == s, q = st->scene_q == s;
        gfx_fill(g, x, y, 114, 58, cur ? C_CYAN : C_DDGREY);
        if (q && ((a->now / 200) & 1)) { gfx_rect(g, x, y, 114, 58, C_YELLOW); gfx_rect(g, x + 1, y + 1, 112, 56, C_YELLOW); }
        snprintf(b, sizeof b, "%c", 'A' + s);
        gfx_text(g, x + 6, y + 6, b, cur ? C_BLACK : C_WHITE, 3);
        if (!sc->valid) { gfx_text(g, x + 34, y + 12, "EMPTY", C_GREY, 1); continue; }
        snprintf(b, sizeof b, "A%02d", sc->pattern + 1);
        gfx_text(g, x + 34, y + 8, b, cur ? C_BLACK : C_WHITE, 1);
        for (int t = 0; t < BM_TRACKS; t++)
            gfx_fill(g, x + 34 + t * 12, y + 24, 9, 9, ((sc->trk_mute >> t) & 1) ? C_DGREY : bm_track_color(t));
        if (cur && st->playing) {
            int bars = a->pat.meta.master ? a->pat.meta.master : 1;
            uint32_t total = (uint32_t)bars * BM_BAR_TICKS;
            gfx_fill(g, x + 6, y + 46, (int)(100u * (st->rtick % total) / total), 4, C_BLACK);
        }
    }
    if (a->chain_n > 1) {
        gfx_text(g, 6, 172, "CHAIN:", C_GREY, 1);
        for (int i = 0; i < a->chain_n; i++) {
            snprintf(b, sizeof b, "%c", 'A' + a->chain[i]);
            gfx_text(g, 52 + i * 12, 172, b, i == st->chain_pos ? C_YELLOW : C_WHITE, 1);
        }
    } else hint(g, 172, "HOLD ONE + OTHERS = CHAIN");
}

static void draw_copych(bm_gfx *g, const bm_app *a)
{
    char b[30];
    header(g, "COPY TRACK", "HOME + EDIT", C_WHITE);
    int s = a->copy_src, d = a->copy_dst % BM_TRACKS;
    if (gfx_strip_hit(g, 60, 60)) {
        gfx_fill(g, 10, 64, 90, 50, bm_track_color(s));
        snprintf(b, sizeof b, "T%d", s + 1);
        gfx_text_c(g, 55, 72, b, C_BLACK, 3);
        gfx_text_c(g, 55, 100, bm_track_name(a, s), C_BLACK, 1);
        gfx_text_c(g, 120, 84, ">", C_WHITE, 3);
        gfx_fill(g, 140, 64, 90, 50, bm_track_color(d));
        snprintf(b, sizeof b, "T%d", d + 1);
        gfx_text_c(g, 185, 72, b, C_BLACK, 3);
        gfx_text_c(g, 185, 100, bm_track_name(a, d), C_BLACK, 1);
    }
    hint(g, 130, "ALGORITHM PICKS THE TARGET");
    hint(g, 142, "OCT+ PASTES - HOME CANCELS");
    if (bm_is_free(d) && app_kind(a, s) != app_kind(a, d)) hint(g, 160, s == 0 ? "THE TARGET BECOMES A KIT" : "THE TARGET CHANGES ENGINE");
}

/* ------------------------------------------------------------ capas: el teclado con lo que hace cada tecla */
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
        if (a->layer == L_HOME || a->view == V_HOME) {
            char b[16]; snprintf(b, sizeof b, "A%02d", a->ctx.st.pattern + 1);
            gfx_text_r(g, 234, 32, b, C_BLACK, 1);
        } else {
            char b[16]; snprintf(b, sizeof b, "T%d", a->track + 1);
            gfx_text_r(g, 234, 32, b, C_BLACK, 1);
        }
    }
    int ky = 52, kh = 108, bh = 62;
    if (gfx_strip_hit(g, ky, kh)) {
        for (int i = 0; i < BM_WHITES; i++) {
            int k = bm_white_key(i), x = i * 15;
            const bm_keyfn *f = &K[k];
            uint16_t bg = a->keys_down[k] ? (f->color ? f->color : C_WHITE) : (f->led == LED_ON || f->led == LED_BLINK ? C_GREY : C_DGREY);
            if (f->led == LED_BLINK && ((a->now / 200) & 1)) bg = C_WHITE;
            gfx_fill(g, x, ky, 14, kh, bg);
            uint16_t dot = f->led == LED_OFF ? C_DDGREY : (f->color ? f->color : C_YELLOW);
            if (f->led == LED_DIM) dot = dim(dot);
            gfx_fill(g, x + 4, ky + bh + 4, 6, 4, dot);
            char l[6]; snprintf(l, sizeof l, "%.4s", f->label);
            int w = gfx_small_w(l);
            gfx_small(g, x + 7 - w / 2, ky + kh - ((i & 1) ? 18 : 9), l, a->keys_down[k] ? C_BLACK : C_WHITE);   /* alternadas: no se pisan */
        }
        for (int i = 0; i < BM_BLACKS; i++) {
            int k = bm_black_key(i), wi = bm_key_white_index(k - 1), x = wi * 15 + 8;
            const bm_keyfn *f = &K[k];
            uint16_t bg = a->keys_down[k] ? (f->color ? f->color : C_WHITE) : C_BLACK;
            gfx_fill(g, x, ky, 13, bh, bg);
            gfx_rect(g, x, ky, 13, bh, C_DGREY);
            uint16_t dot = f->led == LED_OFF ? C_DDGREY : (f->color ? f->color : C_YELLOW);
            if (f->led == LED_DIM) dot = dim(dot);
            if (f->label[0]) gfx_fill(g, x + 4, ky + 4, 5, 4, dot);
            char l[6]; snprintf(l, sizeof l, "%.3s", f->label);
            int w = gfx_small_w(l);
            gfx_small(g, x + 7 - w / 2, ky + bh - 9, l, a->keys_down[k] ? C_BLACK : (f->color ? f->color : C_WHITE));
        }
    }
    hint_s(g, 166, h1);
    hint_s(g, 175, h2);
    if (a->layer == L_HOME) hint_s(g, 186, "ALGORITHM = GO TO A TRACK - BUTTON = GLOBAL");
}

/* ------------------------------------------------------------ recuadro, pregunta, overlay, toast */
static void draw_box(bm_gfx *g, const bm_app *a)
{
    int t = a->box_mode == BOX_LANE ? a->track : a->box_track;
    if (!gfx_strip_hit(g, 70, 78)) return;
    gfx_fill(g, 40, 70, 160, 76, C_BLACK);
    gfx_rect(g, 40, 70, 160, 76, bm_track_color(t));
    gfx_rect(g, 41, 71, 158, 74, bm_track_color(t));
    char b[24];
    if (a->box_mode == BOX_LANE) {                          /* un instrumento de la batería */
        snprintf(b, sizeof b, "%d", a->box_lane + 1);
        gfx_text_c(g, 120, 80, b, bm_track_color(t), 4);
        const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[a->box_lane & 15];
        snprintf(b, sizeof b, "%s - %s", T(bm_lane_name(a, t, a->box_lane)), T(bm_drm_short[ln->model % DRM_COUNT]));
        gfx_text_c(g, 120, 114, b, C_WHITE, 1);
    } else {
        snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text_c(g, 120, 80, b, bm_track_color(t), 4);
        gfx_text_c(g, 120, 114, bm_track_name(a, t), C_WHITE, 1);
    }
    if (a->box_hold == 1) gfx_text_c(g, 120, 128, "RELEASE HOME TO ENTER", C_GREY, 1);
    else if (a->box_hold == 2) gfx_text_c(g, 120, 128, "AUTO ENTER: OFF", C_GREY, 1);
    else {
        int ms = bm_app_enter_ms(a);
        int left = (int)(a->box_until - a->now);
        int w = ms > 0 ? bm_clampi(left * 140 / ms, 0, 140) : 0;
        gfx_fill(g, 50, 132, 140, 4, C_DGREY);
        gfx_fill(g, 50, 132, 140 - w, 4, bm_track_color(t));
    }
}

static void draw_ask(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 62, 84)) return;
    gfx_fill(g, 10, 62, 220, 84, C_DDGREY);
    gfx_rect(g, 10, 62, 220, 84, C_YELLOW);
    gfx_rect(g, 11, 63, 218, 82, C_YELLOW);
    gfx_text_c(g, 120, 74, a->ask_text[0], C_WHITE, 1);
    gfx_text_c(g, 120, 88, a->ask_text[1], C_GREY, 1);
    gfx_fill(g, 30, 110, 80, 22, C_YELLOW);
    gfx_text_c(g, 70, 117, "OCT+ YES", C_BLACK, 1);
    gfx_rect(g, 130, 110, 80, 22, C_GREY);
    gfx_text_c(g, 170, 117, "HOME NO", C_WHITE, 1);
}

static void draw_overlay(bm_gfx *g, const bm_app *a)
{
    const bm_overlay *o = &a->ov;
    if (!gfx_strip_hit(g, 58, 92)) return;
    gfx_fill(g, 12, 58, 216, 92, C_DDGREY);
    gfx_rect(g, 12, 58, 216, 92, o->color);
    gfx_rect(g, 13, 59, 214, 90, o->color);
    gfx_text(g, 20, 66, o->title, C_GREY, 1);
    int w = gfx_text(g, 20, 84, o->val, C_WHITE, gfx_text_w(o->val, 3) > 196 ? 2 : 3);
    if (o->unit[0]) gfx_text(g, 26 + w, 94, o->unit, C_GREY, 2);
    gfx_fill(g, 20, 128, 200, 8, C_DGREY);
    gfx_fill(g, 20, 128, o->pct * 200 / 255, 8, o->color);
    if (o->ghost != 255) gfx_fill(g, 19 + o->ghost * 200 / 255, 124, 2, 16, C_WHITE);
}

static void draw_toast(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 182, 16)) return;
    gfx_fill(g, 0, 182, BM_LCD_W, 16, C_DGREY);
    gfx_text_c(g, 120, 186, a->toast, C_WHITE, 1);
}

/* ------------------------------------------------------------ entrada */
static void draw_channel(bm_gfx *g, const bm_app *a)
{
    draw_tabs(g, a);
    int t = a->track, kind = app_kind(a, t);
    switch (a->page) {
    case PG_SEQ:
        if (kind == KIND_TR) draw_seq_tr(g, a);
        else draw_seq_syn(g, a);
        break;
    case PG_SOUND: draw_sound(g, a); break;
    case PG_INST: draw_inst(g, a); break;
    case PG_MIX: draw_ch_mix(g, a); break;
    case PG_MORE: draw_more(g, a); break;
    case PG_BREAK: draw_break(g, a); break;
    case PG_TAKES: draw_takes(g, a); break;
    case PG_ENV: draw_env(g, a); break;
    case PG_ARP: draw_arp(g, a); break;
    case PG_LOOP: draw_loop(g, a); break;
    case PG_MIDI: draw_midi(g, a); break;
    case PG_SCL: draw_scl(g, a, 0); break;
    case PG_CHORDS: draw_chords(g, a, PY); break;
    case PG_LFO: draw_lfo(g, a); break;
    case PG_REC: draw_rec(g, a); break;
    case PG_PADS: draw_pads(g, a); break;
    default: break;
    }
    if (a->page == PG_SEQ || a->page == PG_PADS) draw_blackstrip(g, a, 185, 14);   /* las blancas ya están en pantalla */
    else draw_keystrip(g, a, 183, 6, 9, 0);
}

void bm_app_draw(bm_gfx *g, void *user)
{
    const bm_app *a = (const bm_app *)user;
    if (a->view == V_BOOT) {
        gfx_logo(g);
        gfx_text(g, 4, 231, "V" BM_VERSION_STR, C_DGREY, 1);
        return;
    }
    draw_status(g, a);
    gfx_clip(g, 0, BM_CTX_Y, BM_LCD_W, BM_CTX_H);
    if (a->layer && a->layer != L_CHORDS) draw_layer(g, a);
    else if (a->layer == L_CHORDS) { gfx_text(g, 4, 28, "LIVE CHORDS", C_GREEN, 1); draw_chords(g, a, 40); }
    else switch (a->view) {
    case V_HOME: draw_home(g, a); break;
    case V_MIX: draw_mix(g, a); draw_keystrip(g, a, 183, 6, 9, 0); break;
    case V_SCENES: draw_scenes(g, a); draw_keystrip(g, a, 183, 6, 9, 0); break;
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
    if (a->box_on) draw_box(g, a);
    if (a->ov.on) draw_overlay(g, a);
    if (a->ask) draw_ask(g, a);
    if (a->toast_until && !a->ask) draw_toast(g, a);
    gfx_noclip(g);
    draw_legend(g, a);
    if (a->now < a->clip_until) {
        gfx_fill(g, 0, 0, BM_LCD_W, 2, C_RED); gfx_fill(g, 0, BM_LCD_H - 2, BM_LCD_W, 2, C_RED);
        gfx_fill(g, 0, 0, 2, BM_LCD_H, C_RED); gfx_fill(g, BM_LCD_W - 2, 0, 2, BM_LCD_H, C_RED);
    }
}
