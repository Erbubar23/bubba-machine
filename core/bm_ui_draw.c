/* Bubba Machine — pantallas (SPEC_V3 §6.3).
 *   y 0..23    estado (siempre igual)
 *   y 24..199  contexto (vista del modo; overlays solo aquí)
 *   y 200..239 leyenda de KNOB 1-4 en su orden físico y en su color
 * Todo el texto en pantalla va en ASCII (la fuente no tiene acentos). */
#include <stdio.h>
#include <string.h>
#include "bm_app.h"
#include "bm_rom.h"
#include "bm_util.h"

static uint16_t dim(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }

/* ------------------------------------------------------------ estado */
static void draw_status(bm_gfx *g, const bm_app *a)
{
    if (!gfx_strip_hit(g, 0, BM_STATUS_H)) return;
    const bm_status *st = &a->ctx.st;
    char b[16];
    if (st->playing) for (int i = 0; i < 11; i++) gfx_vline(g, 5 + i, 6 + i / 2, 12 - (i / 2) * 2, C_GREEN);
    else gfx_fill(g, 5, 7, 10, 10, C_GREY);
    if (a->rec || st->smp_rec) {
        if (!st->smp_rec || ((a->now / 250) & 1)) gfx_disc(g, 23, 12, 4, C_RED);
    }
    snprintf(b, sizeof b, "A%02d", st->pattern + 1);
    gfx_text(g, 32, 5, b, C_WHITE, 2);
    if (st->queued != 0xFF) { snprintf(b, sizeof b, ">%02d", st->queued + 1); gfx_text(g, 70, 3, b, C_YELLOW, 1); }
    int tick = st->playing ? st->tick : 0;
    snprintf(b, sizeof b, "%d.%d", tick / (BM_PPQN * 4) + 1, (tick / BM_PPQN) % 4 + 1);
    gfx_text(g, 72, 13, b, C_GREY, 1);
    int t = a->track;
    gfx_fill(g, 98, 4, 30, 16, bm_track_color(t));
    snprintf(b, sizeof b, "T%d", t + 1);
    gfx_text_c(g, 113, 5, b, C_BLACK, 2);
    int bpm = a->pat.meta.bpm_x10;
    snprintf(b, sizeof b, "%d", (bpm + 5) / 10);
    gfx_text(g, 136, 5, b, C_WHITE, 2);
    gfx_text(g, 175, 13, "BPM", C_GREY, 1);
    if (a->set.clock) gfx_text(g, 175, 3, "EXT", st->ext_ok ? C_GREEN : C_YELLOW, 1);
    if (st->song_on) gfx_text(g, 196, 13, "S", C_YELLOW, 1);
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

static int src_name(const bm_app *a, int t, char *b, int cap)
{
    const bm_track *tr = &a->pat.trk[t];
    if (t == 0) return snprintf(b, (size_t)cap, "%s", bm_rom_kit_names[tr->source % BM_KITS]);
    if (t == 1) return snprintf(b, (size_t)cap, "%s", a->ctx.smp_len ? "SAMPLE" : "VACIO");
    return snprintf(b, (size_t)cap, "%s", bm_rom_wave_names[tr->source % BM_WAVES]);
}

static int step_has(const bm_app *a, int t, int s, int sel_only, int *locked, int *weak, int *tie)
{
    int n = 0;
    *locked = 0; *weak = 0; *tie = 0;
    for (int k = 0; k < BM_EVENTS; k++) {
        const bm_event *e = &a->pat.ev[t][s][k];
        if (bm_event_empty(e)) continue;
        if (sel_only && t < 2 && e->note != a->drum_sel) continue;
        n++;
        for (int i = 0; i < 4; i++) if (e->lk[i] != BM_LOCK_NONE) *locked = 1;
        if (e->prob < 100) *weak = 1;
        if (e->len & BM_LEN_TIE) *tie = 1;
    }
    return n;
}

/* ------------------------------------------------------------ HOME */
static void draw_home(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int ppage = st->playing && st->step >= 0 ? st->step / BM_PAGE_STEPS : a->page;
    char b[16];
    for (int t = 0; t < BM_TRACKS; t++) {
        int y = 28 + t * 28;
        if (!gfx_strip_hit(g, y, 28)) continue;
        const bm_track *tr = &a->pat.trk[t];
        uint16_t col = tr->mute ? C_DGREY : bm_track_color(t);
        if (t == a->track) gfx_fill(g, 0, y, BM_LCD_W, 26, C_DDGREY);
        gfx_fill(g, 4, y + 4, 6, 18, col);
        snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
        gfx_text(g, 14, y + 4, b, tr->mute ? C_GREY : C_WHITE, 1);
        src_name(a, t, b, sizeof b);
        gfx_text(g, 14, y + 15, b, C_GREY, 1);
        for (int s = 0; s < 16; s++) {
            int ab = ppage * BM_PAGE_STEPS + s, lk, wk, ti;
            int n = step_has(a, t, ab, 0, &lk, &wk, &ti);
            uint16_t c = n ? col : C_DGREY;
            if (st->playing && ab == st->step) c = C_WHITE;
            gfx_fill(g, 96 + s * 6, y + 5, 5, 6, c);
        }
        gfx_fill(g, 96, y + 16, 95, 4, C_DGREY);
        gfx_fill(g, 96, y + 16, st->peak[t] * 95 / 255, 4, col);
        if (tr->mute) gfx_text(g, 198, y + 9, "M", C_RED, 1);
        if (a->now < a->trig_until[t]) gfx_disc(g, 226, y + 13, 5, col);
        else gfx_disc(g, 226, y + 13, 2, C_DGREY);
    }
}

/* ------------------------------------------------------------ SEQ */
static void draw_seq(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int t = a->track;
    uint16_t col = bm_track_color(t);
    char b[24], c2[24];
    if (gfx_strip_hit(g, 26, 14)) {
        snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
        gfx_text(g, 6, 29, b, col, 1);
        if (a->held_step >= 0 && a->held_long) snprintf(c2, sizeof c2, "PASO %02d", a->held_step + 1);
        else if (t == 0) snprintf(c2, sizeof c2, "%s", bm_rom_kits[a->pat.trk[0].source % BM_KITS][a->drum_sel].name);
        else if (t == 1) snprintf(c2, sizeof c2, "SLICE %d", a->drum_sel + 1);
        else bm_note_str(a->last_note[t], c2, sizeof c2);
        gfx_text(g, 80, 29, c2, C_WHITE, 1);
        snprintf(b, sizeof b, "P%d/%d", a->page + 1, a->pat.meta.length / 16);
        gfx_text_r(g, 234, 29, b, C_GREY, 1);
    }
    int len = a->pat.meta.length;
    for (int p = 0; p < 4; p++) {
        int y = 42 + p * 18;
        if (!gfx_strip_hit(g, y, 16)) continue;
        bool cur = p == a->page;
        for (int s = 0; s < 16; s++) {
            int ab = p * 16 + s, x = s * 15;
            if (ab >= len) { gfx_fill(g, x, y + 6, 14, 2, C_DDGREY); continue; }
            int lk, wk, ti;
            int n = step_has(a, t, ab, 1, &lk, &wk, &ti);
            int lko, wko, tio;
            int other = (t < 2) ? step_has(a, t, ab, 0, &lko, &wko, &tio) - n : 0;
            uint16_t c = cur ? col : dim(col);
            if (n) {
                if (wk) gfx_rect(g, x, y, 14, 15, c), gfx_rect(g, x + 1, y + 1, 12, 13, c);
                else gfx_fill(g, x, y, 14, 15, c);
                if (ti) gfx_fill(g, x + 14, y + 6, 1, 3, c);
                if (lk) gfx_fill(g, x + 1, y + 1, 3, 3, wk ? c : C_WHITE);
            } else {
                gfx_fill(g, x, y, 14, 15, cur ? C_DDGREY : C_BLACK);
                if (s % 4 == 0) gfx_fill(g, x + 6, y + 13, 2, 2, C_DGREY);
                if (other > 0) gfx_fill(g, x + 5, y + 5, 4, 4, C_GREY);
            }
            if (st->playing && ab == st->step) gfx_rect(g, x, y, 14, 15, C_WHITE);
            if (ab == a->held_step && a->held_long) { gfx_rect(g, x, y, 14, 15, C_WHITE); gfx_rect(g, x + 1, y + 1, 12, 13, C_BLACK); }
        }
        if (cur) gfx_fill(g, 0, y + 15, 240, 1, C_DGREY);
    }
    if (a->held_step >= 0 && a->held_long) {
        int s = a->held_step;
        const bm_event *e = NULL;
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *x = &a->pat.ev[t][s][k];
            if (!bm_event_empty(x) && (t >= 2 || x->note == a->drum_sel)) { e = x; break; }
        }
        if (!gfx_strip_hit(g, 116, 84)) return;
        gfx_fill(g, 0, 116, 240, 84, C_DDGREY);
        if (!e) { gfx_text_c(g, 120, 150, "PASO VACIO", C_GREY, 1); return; }
        if (t == 0) snprintf(b, sizeof b, "%s", bm_rom_kits[a->pat.trk[0].source % BM_KITS][e->note % 16].name);
        else if (t == 1) snprintf(b, sizeof b, "SL%d", e->note % 16 + 1);
        else bm_note_str(e->note, b, sizeof b);
        gfx_text(g, 6, 121, t < 2 ? "SONIDO" : "NOTA", C_GREY, 1);
        gfx_text(g, 6, 132, b, C_WHITE, 2);
        gfx_text(g, 90, 121, "PROB", C_GREY, 1);
        snprintf(b, sizeof b, "%d%%", e->prob);
        gfx_text(g, 90, 132, b, C_WHITE, 2);
        gfx_text(g, 164, 121, "LARGO", C_GREY, 1);
        if (e->len & BM_LEN_TIE) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "%d", e->len & BM_LEN_MASK);
        gfx_text(g, 164, 132, b, C_WHITE, 2);
        gfx_text(g, 6, 152, "SELECT  ALGORITHM  PRESETS", C_GREY, 1);
        for (int i = 0; i < 4; i++) {
            int x = i * 60 + 4;
            gfx_text(g, x, 166, bm_macro_name(t, i), bm_knob_color(i), 1);
            if (e->lk[i] != BM_LOCK_NONE) bm_fmt_macro(a, t, i, e->lk[i], b, sizeof b); else snprintf(b, sizeof b, "--");
            gfx_text(g, x, 178, b, e->lk[i] != BM_LOCK_NONE ? C_WHITE : C_GREY, 1);
        }
        return;
    }
    /* carriles de las 6 pistas, los 64 pasos */
    for (int tt = 0; tt < BM_TRACKS; tt++) {
        int y = 118 + tt * 13;
        if (!gfx_strip_hit(g, y, 12)) continue;
        uint16_t tc = a->pat.trk[tt].mute ? C_DGREY : bm_track_color(tt);
        if (tt == t) gfx_fill(g, 0, y - 1, 240, 12, C_DDGREY);
        gfx_fill(g, 2, y + 1, 4, 8, tc);
        for (int s = 0; s < len; s++) {
            int x = 14 + s * 3 + (s / 16) * 6;
            int lk, wk, ti;
            int n = step_has(a, tt, s, 0, &lk, &wk, &ti);
            if (st->playing && s == st->step) gfx_fill(g, x, y, 2, 10, C_WHITE);
            else if (n) gfx_fill(g, x, y + 1, 2, 8, tc);
            else if (s % 4 == 0) gfx_fill(g, x, y + 4, 2, 2, C_DGREY);
        }
    }
}

/* ------------------------------------------------------------ EDIT */
static void draw_wave(bm_gfx *g, const bm_app *a, int t, int x0, int y0, int w, int h)
{
    const bm_track *tr = &a->pat.trk[t];
    int p = tr->macro[3] * 3 * 256 / 127;
    int w0 = (tr->source + (p >> 8)) % BM_WAVES, w1 = (w0 + 1) % BM_WAVES, mf = p & 255;
    gfx_rect(g, x0, y0, w, h, C_DGREY);
    gfx_hline(g, x0 + 1, y0 + h / 2, w - 2, C_DDGREY);
    int py = -1;
    for (int x = 0; x < w - 4; x++) {
        int i = x * 256 / (w - 4);
        int s = bm_rom_waves[w0][2][i] + ((bm_rom_waves[w1][2][i] - bm_rom_waves[w0][2][i]) * mf >> 8);
        int y = y0 + h / 2 - s * (h / 2 - 4) / 32768;
        if (py >= 0) gfx_line(g, x0 + 1 + x, py, x0 + 2 + x, y, bm_track_color(t));
        py = y;
    }
}

static void draw_edit(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[30];
    if (t == 0) {
        header(g, "KIT", bm_rom_kit_names[tr->source % BM_KITS], bm_track_color(0));
        for (int i = 0; i < 16; i++) {
            int x = (i % 4) * 59 + 2, y = 52 + (i / 4) * 34;
            if (!gfx_strip_hit(g, y, 32)) continue;
            bool sel = i == a->drum_sel;
            gfx_fill(g, x, y, 56, 30, sel ? bm_track_color(0) : C_DDGREY);
            gfx_text(g, x + 4, y + 4, bm_rom_kits[tr->source % BM_KITS][i].name, sel ? C_BLACK : C_WHITE, 1);
            snprintf(b, sizeof b, "%d", i + 1);
            gfx_text(g, x + 4, y + 18, b, sel ? C_BLACK : C_GREY, 1);
        }
        return;
    }
    if (t == 1) {
        const bm_status *st = &a->ctx.st;
        header(g, "SAMPLER", "RESAMPLE", bm_track_color(1));
        gfx_rect(g, 6, 50, 58, 13, C_ORANGE);
        gfx_text(g, 10, 53, "VOLATIL", C_ORANGE, 1);
        uint32_t used = st->smp_rec ? a->ctx.smp_rec_pos : a->ctx.smp_len;
        uint32_t ms = used * 1000u / BM_FS, tot = BM_SAMPLER_LEN * 1000u / BM_FS;
        snprintf(b, sizeof b, "%u.%02u / %u.%02u S", (unsigned)(ms / 1000), (unsigned)(ms % 1000 / 10), (unsigned)(tot / 1000), (unsigned)(tot % 1000 / 10));
        gfx_text_r(g, 234, 53, b, C_WHITE, 1);
        gfx_text(g, 6, 70, st->smp_rec ? "GRABANDO" : (a->ctx.smp_len ? "LISTO" : "VACIO"), st->smp_rec ? C_RED : C_GREY, 2);
        int ty = 96, th = 56;
        if (gfx_strip_hit(g, ty, th + 12)) {
            gfx_rect(g, 4, ty, 232, th, C_DGREY);
            int fill = (int)(used * 230u / BM_SAMPLER_LEN);
            gfx_fill(g, 5, ty + 1, fill, th - 2, dim(bm_track_color(1)));
            uint32_t len = a->ctx.smp_len ? a->ctx.smp_len : used;
            int n = a->ctx.smp_marks;
            for (int i = 0; i < n; i++) {
                int x = 5 + (int)(a->ctx.smp_mark[i] * 230u / BM_SAMPLER_LEN);
                gfx_vline(g, x, ty + 1, th - 2, C_WHITE);
                snprintf(b, sizeof b, "%d", i + 1);
                gfx_small(g, x + 2, ty + 3, b, C_WHITE);
            }
            if (!n && len) for (int i = 1; i < 16; i++) gfx_vline(g, 5 + (int)(len * (uint32_t)i / 16u * 230u / BM_SAMPLER_LEN), ty + th - 8, 6, C_GREY);
            if (st->smp_rec) gfx_vline(g, 5 + fill, ty, th, C_RED);
        }
        gfx_text(g, 6, 160, "MANTEN REC: GRABA LA MEZCLA", C_GREY, 1);
        gfx_text(g, 6, 172, "BLANCAS AL GRABAR: CHOPS", C_GREY, 1);
        gfx_text(g, 6, 184, "SIN CHOPS: 16 PARTES IGUALES", C_GREY, 1);
        return;
    }
    snprintf(b, sizeof b, "%s", bm_rom_wave_names[tr->source % BM_WAVES]);
    header(g, "WAVETABLE", b, bm_track_color(t));
    if (gfx_strip_hit(g, 50, 92)) draw_wave(g, a, t, 4, 50, 232, 90);
    char c1[12], c2[12];
    bm_fmt_macro(a, t, 0, tr->macro[0], c1, sizeof c1);
    bm_fmt_macro(a, t, 3, tr->macro[3], c2, sizeof c2);
    snprintf(b, sizeof b, "SVF LP %s  RES %d%%", c1, tr->macro[1] * 100 / 127);
    gfx_text(g, 6, 150, b, C_WHITE, 1);
    snprintf(b, sizeof b, "ONDA: %s", c2);
    gfx_text(g, 6, 164, b, C_GREY, 1);
    gfx_text(g, 6, 178, t == 2 ? "MONO + GLIDE (TIE)" : "POLIFONICO (POOL 7 VOCES)", C_GREY, 1);
}

/* ------------------------------------------------------------ ENV */
static void draw_env(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    uint16_t col = bm_track_color(t);
    char b[24];
    snprintf(b, sizeof b, "T%d PAG %d/2", t + 1, a->env_page + 1);
    header(g, "ENV", b, col);
    int x0 = 8, y0 = 54, h = 96, yb = y0 + h;
    gfx_rect(g, 4, 50, 232, 104, C_DDGREY);
    if (t < 2) {
        int d = 20 + tr->macro[2] * 180 / 127;
        gfx_line(g, x0, y0, x0 + d, yb, col);
        gfx_line(g, x0, y0 + 1, x0 + d, yb + 1, col);
        gfx_text(g, 6, 162, "UN DISPARO: SOLO DECAY", C_GREY, 1);
        return;
    }
    int A = 4 + tr->attack * 50 / 127, D = 6 + tr->macro[2] * 60 / 127, S = 50, R = 6 + tr->release * 60 / 127;
    int sy = yb - tr->sustain * h / 127;
    int x1 = x0 + A, x2 = x1 + D, x3 = x2 + S, x4 = x3 + R;
    for (int o = 0; o < 2; o++) {
        gfx_line(g, x0, yb + o, x1, y0 + o, col);
        gfx_line(g, x1, y0 + o, x2, sy + o, col);
        gfx_line(g, x2, sy + o, x3, sy + o, col);
        gfx_line(g, x3, sy + o, x4, yb + o, col);
    }
    if (a->env_page == 1) {
        snprintf(b, sizeof b, "ENV > FILTRO %d%%", tr->env_amt * 100 / 127);
        gfx_text(g, 6, 162, b, C_WHITE, 1);
        snprintf(b, sizeof b, "GLIDE %dMS (CON TIE)", tr->glide * 500 / 127);
        gfx_text(g, 6, 176, b, C_GREY, 1);
    } else {
        gfx_text(g, 6, 162, "A      D      S      R", C_GREY, 1);
        gfx_text(g, 6, 176, "ENV OTRA VEZ = PAGINA 2", C_GREY, 1);
    }
}

/* ------------------------------------------------------------ FX */
static void draw_fx(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    static const char *const DT[4] = {"1/16", "1/8", "3/16", "1/4"};
    char b[24];
    header(g, "FX", "GLOBALES", C_WHITE);
    gfx_rect(g, 4, 52, 114, 70, C_BLUE);
    gfx_text(g, 10, 57, "DELAY", C_BLUE, 1);
    gfx_text(g, 10, 72, DT[m->dly_time & 3], C_WHITE, 2);
    snprintf(b, sizeof b, "FB %d%%", m->dly_fb * 100 / 127);
    gfx_text(g, 10, 94, b, C_GREEN, 1);
    snprintf(b, sizeof b, "MIX %d%%", m->dly_mix * 100 / 127);
    gfx_text(g, 10, 106, b, C_YELLOW, 1);
    gfx_rect(g, 122, 52, 114, 70, C_ORANGE);
    gfx_text(g, 128, 57, "SIDECHAIN", C_ORANGE, 1);
    gfx_text(g, 128, 69, "T1 > T3-T6", C_GREY, 1);
    for (int t = 2; t < BM_TRACKS; t++) {
        int x = 128 + (t - 2) * 26;
        gfx_fill(g, x, 82, 18, 32, C_DDGREY);
        int h = a->pat.trk[t].duck * 32 / 127;
        gfx_fill(g, x, 114 - h, 18, h, bm_track_color(t));
    }
    snprintf(b, sizeof b, "DRIVE %d%%  (GLO+K4)", a->set.drive * 100 / 127);
    gfx_text(g, 6, 134, b, C_WHITE, 1);
    gfx_text(g, 6, 152, "MANTEN FX = PUNCH-IN", C_GREY, 1);
    gfx_text(g, 6, 164, "ENVIO POR PISTA: HOME K3", C_GREY, 1);
    gfx_text(g, 6, 176, "DUCK POR PISTA: HOME K4", C_GREY, 1);
}

/* ------------------------------------------------------------ teclado (SCL / KEYMAP) */
static void draw_keyboard(bm_gfx *g, const bm_app *a, int chords)
{
    const bm_meta *m = &a->pat.meta;
    int t = a->track;
    int base_oct = (t == 2 ? 2 : 3) + a->oct;
    char b[12];
    int ky = 66, kh = 90;
    if (!gfx_strip_hit(g, ky, kh + 2)) return;
    int n = bm_scale_len(m->scale);
    for (int i = 0; i < BM_WHITES; i++) {
        int x = i * 15, k = bm_white_key(i);
        uint16_t c = (i % n == 0) ? C_YELLOW : C_WHITE;
        if (a->keys_down[k]) c = bm_track_color(t);
        gfx_fill(g, x, ky, 14, kh, c);
        int note = chords ? bm_scale_note(m->root, m->scale, i, base_oct) : BM_KEY_BASE_NOTE + k + 12 * a->oct;
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
        gfx_fill(g, x, ky, 11, 52, c);
        gfx_rect(g, x, ky, 11, 52, C_BLACK);
        if (chords) {
            if (i < 7) {
                char nm[10];
                bm_chord_name(m->root, m->scale, i, 0, nm, sizeof nm);
                nm[3] = 0;
                gfx_small(g, x + 1, ky + 42, nm, a->keys_down[k] ? C_BLACK : C_WHITE);
            } else {
                static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
                gfx_small(g, x + 2, ky + 42, MODS[i - 7], a->keys_down[k] ? C_BLACK : C_YELLOW);
            }
        } else {
            int note = BM_KEY_BASE_NOTE + k + 12 * a->oct;
            snprintf(b, sizeof b, "%s", bm_note_names[note % 12]);
            gfx_small(g, x + 1, ky + 42, b, C_WHITE);
        }
    }
}

static void draw_scl(bm_gfx *g, const bm_app *a, int live)
{
    const bm_meta *m = &a->pat.meta;
    char b[24];
    snprintf(b, sizeof b, "%s %s", bm_note_names[m->root % 12], bm_scale_names[m->scale % BM_SCALES]);
    gfx_text(g, 6, 30, live ? "EN VIVO" : "ESCALA", live ? C_YELLOW : C_GREY, 1);
    gfx_text(g, 6, 42, b, C_WHITE, 2);
    if (a->last_degree >= 0) gfx_text_r(g, 234, 42, a->chord_name, C_YELLOW, 2);
    if (a->track >= 2) draw_keyboard(g, a, 1);
    else gfx_text_c(g, 120, 100, "ELIGE UNA PISTA SYNTH (T3-T6)", C_GREY, 1);
    gfx_text(g, 6, 162, "BLANCAS: NOTAS DE LA ESCALA", C_GREY, 1);
    gfx_text(g, 6, 174, "NEGRAS 1-7: ACORDES", C_GREY, 1);
    gfx_text(g, 6, 186, "NEGRAS 8-11: +7 +9 SUS2 SUS4", C_GREY, 1);
}

/* ------------------------------------------------------------ ARP */
static void draw_arp(bm_gfx *g, const bm_app *a, int live)
{
    static const char *const M[5] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM"};
    static const char *const R[3] = {"1/8", "1/16", "1/32"};
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
    header(g, live ? "ARP VIVO" : "ARP", b, live ? C_YELLOW : bm_track_color(t));
    if (t < 2) { gfx_text_c(g, 120, 100, "SOLO PISTAS SYNTH (T3-T6)", C_GREY, 1); return; }
    gfx_text(g, 6, 56, M[tr->arp_mode % 5], C_WHITE, 3);
    snprintf(b, sizeof b, "%s  %d OCT  GATE %d%%", R[tr->arp_rate % 3], tr->arp_oct, tr->arp_gate);
    gfx_text(g, 6, 84, b, C_GREY, 1);
    const bm_arp *ar = &a->ctx.arp[t];
    for (int i = 0; i < 8; i++) {
        int x = 10 + i * 28, h = 10 + i * 6;
        bool has = i < ar->n;
        gfx_fill(g, x, 160 - h, 22, h, has ? (ar->playing && ar->cur % 12 == ar->held[i] % 12 ? C_WHITE : bm_track_color(t)) : C_DDGREY);
    }
    gfx_text(g, 6, 172, tr->arp_mode ? "SIEMPRE ACTIVO EN ESTA PISTA" : "MANTEN ARP = ARPEGIO EN VIVO", C_GREY, 1);
}

/* ------------------------------------------------------------ listas */
static void row(bm_gfx *g, int y, bool sel, const char *l, const char *r)
{
    if (!gfx_strip_hit(g, y, 16)) return;
    if (sel) gfx_fill(g, 0, y, BM_LCD_W, 15, C_DGREY);
    gfx_text(g, 6, y + 4, l, sel ? C_WHITE : C_GREY, 1);
    if (r) gfx_text_r(g, 234, y + 4, r, C_WHITE, 1);
}

static void draw_glo(bm_gfx *g, const bm_app *a)
{
    const bm_settings *s = &a->set;
    char v[10][16];
    snprintf(v[0], 16, "%s", s->clock ? "EXTERNO" : "INTERNO");
    snprintf(v[1], 16, "%s", s->sync_mode ? "SPLIT L/R" : "ESTEREO");
    snprintf(v[2], 16, "%d PPQN", s->sync_ppqn);
    snprintf(v[3], 16, "%d MS", s->sync_lat);
    snprintf(v[4], 16, "%d%%", s->drive * 100 / 127);
    snprintf(v[5], 16, "%s", s->autosave ? "ON" : "OFF");
    snprintf(v[6], 16, "%s", s->led_dim == 0 ? "OFF" : (s->led_dim == 1 ? "TENUE" : "MEDIO"));
    snprintf(v[7], 16, "RESAMPLE");
    snprintf(v[8], 16, "%d/%d SLOTS", bm_store_used_slots(&a->store), (int)BM_STORE_SLOTS);
    snprintf(v[9], 16, "%s", BM_VERSION_STR);
    static const char *const L[10] = {"RELOJ", "SALIDA SYNC", "SYNC PPQN", "SYNC LATENCIA", "DRIVE", "AUTOGUARDADO", "LED REPOSO", "FUENTE SAMPLER", "MEMORIA", "VERSION"};
    for (int i = 0; i < 10; i++) row(g, 26 + i * 17, i == a->menu_row, L[i], v[i]);
}

static void draw_save(bm_gfx *g, const bm_app *a)
{
    char r0[24], r1[24], r4[24];
    snprintf(r0, sizeof r0, "A%02d", a->ctx.st.pattern + 1);
    snprintf(r1, sizeof r1, "A%02d", a->save_target + 1);
    snprintf(r4, sizeof r4, "%d/%d", bm_pattern_count_events(&a->pat), BM_PAT_MAX_EVENTS);
    header(g, "SAVE", a->dirty ? "SIN GUARDAR" : "GUARDADO", C_WHITE);
    row(g, 52, a->save_row == 0, "GUARDAR", r0);
    row(g, 70, a->save_row == 1, "GUARDAR COMO", r1);
    row(g, 88, a->save_row == 2, "GUARDAR SONG", NULL);
    row(g, 106, a->save_row == 3, a->clear_armed ? "BORRAR? SAVE OTRA VEZ" : "BORRAR PATRON", NULL);
    row(g, 124, a->save_row == 4, "EVENTOS", r4);
    gfx_text(g, 6, 150, "SELECT ELIGE, SAVE EJECUTA", C_GREY, 1);
    gfx_text(g, 6, 162, "MANTEN SAVE 1 S: RAPIDO", C_GREY, 1);
    gfx_text(g, 6, 174, a->ctx.st.playing ? "SOLO EN STOP (CORTA EL AUDIO)" : "SOLO EN STOP", a->ctx.st.playing ? C_ORANGE : C_GREY, 1);
}

static void draw_song(bm_gfx *g, const bm_app *a)
{
    char b[24];
    header(g, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? C_YELLOW : C_WHITE);
    if (!a->song.n) {
        gfx_text_c(g, 120, 90, "VACIA", C_GREY, 2);
        gfx_text_c(g, 120, 120, "BLANCA = ANADIR PATRON", C_GREY, 1);
        return;
    }
    int first = a->song_cur > 6 ? a->song_cur - 6 : 0;
    for (int i = 0; i < 8 && first + i < a->song.n; i++) {
        int e = first + i;
        snprintf(b, sizeof b, "%2d  A%02d  x%d", e + 1, a->song.pat[e] + 1, a->song.rep[e]);
        row(g, 50 + i * 16, e == a->song_cur, b, (a->ctx.st.song_on && a->ctx.st.playing && e == a->ctx.st.song_pos) ? ">" : NULL);
    }
    gfx_text(g, 6, 186, "BLANCA AGREGA - NEGRA 9 BORRA", C_GREY, 1);
}

/* ------------------------------------------------------------ capas */
static void draw_bank(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[8];
    header(g, "PATRONES", "CAMBIA AL FINAL", C_WHITE);
    for (int i = 0; i < BM_PATTERNS; i++) {
        int x = (i % 8) * 30, y = 62 + (i / 8) * 54;
        if (!gfx_strip_hit(g, y, 48)) continue;
        bool cur = i == st->pattern, q = i == st->queued;
        bool has = bm_store_has(&a->store, (uint8_t)i) || cur || a->pend_obj == i;
        uint16_t bg = cur ? C_WHITE : (has ? C_DGREY : C_BLACK);
        gfx_fill(g, x + 1, y, 28, 46, bg);
        if (!has) gfx_rect(g, x + 1, y, 28, 46, C_DDGREY);
        if (q) { gfx_rect(g, x + 1, y, 28, 46, C_YELLOW); gfx_rect(g, x + 2, y + 1, 26, 44, C_YELLOW); }
        snprintf(b, sizeof b, "%02d", i + 1);
        gfx_text_c(g, x + 15, y + 8, b, cur ? C_BLACK : C_WHITE, 1);
        if ((cur && a->dirty) || a->pend_obj == i) gfx_disc(g, x + 15, y + 32, 3, C_ORANGE);
    }
    gfx_text(g, 6, 180, "PUNTO NARANJA: SIN GUARDAR", C_GREY, 1);
}

static void draw_mute(bm_gfx *g, const bm_app *a)
{
    header(g, "MUTE", "9-14 = SOLO", C_WHITE);
    for (int t = 0; t < BM_TRACKS; t++) {
        int x = (t % 3) * 80, y = 56 + (t / 3) * 64;
        if (!gfx_strip_hit(g, y, 58)) continue;
        bool m = a->pat.trk[t].mute;
        gfx_fill(g, x + 2, y, 76, 58, m ? C_DDGREY : bm_track_color(t));
        char b[12];
        snprintf(b, sizeof b, "T%d", t + 1);
        gfx_text(g, x + 8, y + 8, b, m ? C_GREY : C_BLACK, 2);
        gfx_text(g, x + 8, y + 30, bm_track_name(t), m ? C_GREY : C_BLACK, 1);
        if (m) gfx_text(g, x + 8, y + 42, "MUTE", C_RED, 1);
    }
}

static void draw_punch(bm_gfx *g, const bm_app *a)
{
    static const char *const N1[16] = {"STU", "STU", "STU", "STU", "REV", "REV", "TAPE", "GATE", "LPF", "HPF", "BIT", "SR", "DLY", "ECHO", "KILL", "KILL"};
    static const char *const N2[16] = {"1/4", "1/8", "1/16", "1/32", "1/8", "1/4", "STOP", "1/16", "SWP", "SWP", "CRSH", "DOWN", "THRW", "FRZ", "LOW", "HIGH"};
    header(g, "PUNCH-IN", "CUANTIZA A 1/16", C_YELLOW);
    for (int i = 0; i < 16; i++) {
        int x = (i % 8) * 30, y = 58 + (i / 8) * 56;
        if (!gfx_strip_hit(g, y, 50)) continue;
        bool on = a->ctx.st.punch == i;
        gfx_fill(g, x + 1, y, 28, 50, on ? C_WHITE : C_DDGREY);
        gfx_small(g, x + 15 - gfx_small_w(N1[i]) / 2, y + 14, N1[i], on ? C_BLACK : C_WHITE);
        gfx_small(g, x + 15 - gfx_small_w(N2[i]) / 2, y + 26, N2[i], on ? C_BLACK : C_YELLOW);
    }
    gfx_text(g, 6, 180, "SUELTA LA TECLA = SECO", C_GREY, 1);
}

static void draw_lfo(bm_gfx *g, const bm_app *a)
{
    (void)a;
    header(g, "LFO", NULL, C_WHITE);
    gfx_text_c(g, 120, 90, "RESERVADO", C_GREY, 2);
    gfx_text_c(g, 120, 120, "DECISION 5 DE LA SPEC", C_GREY, 1);
}

/* ------------------------------------------------------------ overlay y toast */
static void draw_overlay(bm_gfx *g, const bm_app *a)
{
    const bm_overlay *o = &a->ov;
    if (!gfx_strip_hit(g, 58, 92)) return;
    gfx_fill(g, 12, 58, 216, 92, C_DDGREY);
    gfx_rect(g, 12, 58, 216, 92, o->color);
    gfx_rect(g, 13, 59, 214, 90, o->color);
    gfx_text(g, 20, 66, o->title, C_GREY, 1);
    int w = gfx_text(g, 20, 84, o->val, C_WHITE, 3);
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
    switch (bm_app_eff_view(a)) {
    case V_HOME: draw_home(g, a); break;
    case V_SEQ: draw_seq(g, a); break;
    case V_EDIT: draw_edit(g, a); break;
    case V_ENV: draw_env(g, a); break;
    case V_LFO: draw_lfo(g, a); break;
    case V_FX: draw_fx(g, a); break;
    case V_SCL: draw_scl(g, a, 0); break;
    case V_KEYMAP: draw_scl(g, a, 1); break;
    case V_ARP: draw_arp(g, a, 0); break;
    case V_ARPLIVE: draw_arp(g, a, 1); break;
    case V_SAVE: draw_save(g, a); break;
    case V_GLO: draw_glo(g, a); break;
    case V_SONG: draw_song(g, a); break;
    case V_BANK: draw_bank(g, a); break;
    case V_MUTE: draw_mute(g, a); break;
    case V_PUNCH: draw_punch(g, a); break;
    default: break;
    }
    if (a->ov.on) draw_overlay(g, a);
    if (a->toast_until) draw_toast(g, a);
    gfx_noclip(g);
    draw_legend(g, a);
    if (a->now < a->clip_until) {                         /* borde rojo: el bus saturó */
        gfx_fill(g, 0, 0, BM_LCD_W, 2, C_RED); gfx_fill(g, 0, BM_LCD_H - 2, BM_LCD_W, 2, C_RED);
        gfx_fill(g, 0, 0, 2, BM_LCD_H, C_RED); gfx_fill(g, BM_LCD_W - 2, 0, 2, BM_LCD_H, C_RED);
    }
}
