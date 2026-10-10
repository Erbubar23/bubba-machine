/* Bubba Machine — pantallas (SPEC_V3 §6.3, DISENO_0.2).
 *   y 0..23    estado (siempre igual; anillo de la pulsación de 2 s)
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
    if (a->rec || st->smp_rec || a->song_rec) {
        if (!(st->smp_rec || a->song_rec) || ((a->now / 250) & 1)) gfx_disc(g, 23, 12, 4, C_RED);
    }
    snprintf(b, sizeof b, "A%02d", st->pattern + 1);
    gfx_text(g, 32, 5, b, C_WHITE, 2);
    if (st->queued != 0xFF) { snprintf(b, sizeof b, ">%02d", st->queued + 1); gfx_text(g, 70, 3, b, C_YELLOW, 1); }
    else if (st->scene_q != 0xFF) { snprintf(b, sizeof b, ">%c", 'A' + st->scene_q); gfx_text(g, 70, 3, b, C_CYAN, 1); }
    uint32_t bt = st->playing ? st->btick : 0;
    snprintf(b, sizeof b, "%u.%u", (unsigned)(bt / BM_BAR_TICKS % 99 + 1), (unsigned)((bt / BM_PPQN) % 4 + 1));
    gfx_text(g, 72, 13, b, C_GREY, 1);
    int t = a->track;
    gfx_fill(g, 98, 4, 30, 16, bm_track_color(t));
    snprintf(b, sizeof b, "T%d", t + 1);
    gfx_text_c(g, 113, 5, b, C_BLACK, 2);
    int bpm = a->pat.meta.bpm_x10;
    snprintf(b, sizeof b, "%d", (bpm + 5) / 10);
    gfx_text(g, 134, 5, b, C_WHITE, 2);
    gfx_text(g, 172, 13, "BPM", C_GREY, 1);
    if (a->set.clock) gfx_text(g, 172, 3, "EXT", st->ext_ok ? C_GREEN : C_YELLOW, 1);
    else if (st->scene != 0xFF) { snprintf(b, sizeof b, "%c", 'A' + st->scene); gfx_text(g, 172, 3, b, C_CYAN, 1); }
    if (st->fill && st->playing) gfx_text(g, 180, 3, "F", C_ORANGE, 1);
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

static void hint(bm_gfx *g, int y, const char *s) { gfx_text(g, 6, y, s, C_GREY, 1); }

static int src_name(const bm_app *a, int t, char *b, int cap)
{
    const bm_track *tr = &a->pat.trk[t];
    if (t == 0) return snprintf(b, (size_t)cap, "%s", bm_rom_kit_names[tr->source % BM_KITS]);
    if (t == 1) return snprintf(b, (size_t)cap, "SLOT %c %s", 'A' + tr->source % 2, a->smp_valid[tr->source % 2] ? "" : "VACIO");
    return snprintf(b, (size_t)cap, "%s", bm_rom_wave_names[tr->source % BM_WAVES]);
}

/* ¿la pista tiene golpe/nota en el paso absoluto s? (TR: cualquier instrumento, con su largo) */
static int track_has(const bm_app *a, int t, int s)
{
    if (bm_is_tr(t)) {
        const bm_tr *tr = &a->pat.tr[t];
        for (int l = 0; l < BM_LANES; l++) {
            int len = tr->lane[l].len ? tr->lane[l].len : 16;
            if (tr->st[l][s % len] & ST_ON) return 1;
        }
        return 0;
    }
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
    for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(&a->pat.ev[t - 2][s % len][k])) return 1;
    return 0;
}

static void lane_pads(bm_gfx *g, const bm_app *a, int t, int y0, int mode)
{
    /* 4x4 instrumentos: mode 0 tocar, 1 mute, 2 borrar, 3 roll */
    for (int i = 0; i < 16; i++) {
        int x = (i % 4) * 59 + 2, y = y0 + (i / 4) * 32;
        if (!gfx_strip_hit(g, y, 30)) continue;
        bool muted = (a->pat.tr[t].mute >> i) & 1;
        bool act = (mode == 2 && ((a->erase_mask >> i) & 1)) || (mode == 3 && ((a->roll_mask >> i) & 1));
        bool sel = i == a->sel_lane[t];
        bool fl = a->now < a->lane_flash[t][i];
        uint16_t bg = act ? (mode == 2 ? C_RED : C_WHITE) : fl ? bm_track_color(t) : (sel && mode == 0 ? dim(bm_track_color(t)) : C_DDGREY);
        if (mode == 1) bg = muted ? C_DDGREY : bm_track_color(t);
        gfx_fill(g, x, y, 56, 28, bg);
        uint16_t fg = (act || fl || (mode == 1 && !muted)) ? C_BLACK : (muted ? C_GREY : C_WHITE);
        gfx_text(g, x + 4, y + 4, bm_lane_name(a, t, i), fg, 1);
        char b[6]; snprintf(b, sizeof b, "%d", i + 1);
        gfx_text(g, x + 4, y + 16, b, fg, 1);
        if (muted && mode != 1) gfx_text(g, x + 34, y + 16, "M", C_RED, 1);
    }
}

/* ------------------------------------------------------------ HOME */
static void draw_home(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
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
        uint32_t n = st->tn[t];
        int cur = st->playing ? (int)(n % 16) : -1;
        int base = st->playing ? (int)((n / 16) * 16) : 0;
        for (int s = 0; s < 16; s++) {
            uint16_t c = track_has(a, t, base + s) ? col : C_DGREY;
            if (s == cur) c = C_WHITE;
            gfx_fill(g, 96 + s * 6, y + 5, 5, 6, c);
        }
        gfx_fill(g, 96, y + 16, 95, 4, C_DGREY);
        gfx_fill(g, 96, y + 16, st->peak[t] * 95 / 255, 4, col);
        if (tr->speed != SPD_X1) gfx_small(g, 196, y + 4, bm_speed_names[tr->speed], C_GREY);
        if (tr->mute) gfx_text(g, 198, y + 14, "M", C_RED, 1);
        if (a->now < a->trig_until[t]) gfx_disc(g, 226, y + 13, 5, col);
        else gfx_disc(g, 226, y + 13, 2, C_DGREY);
    }
}

/* ------------------------------------------------------------ SEQ TR: matriz 16 x 16 */
static void draw_seq_tr(bm_gfx *g, const bm_app *a)
{
    int t = a->track, sel = a->sel_lane[t];
    const bm_tr *tr = &a->pat.tr[t];
    uint16_t col = bm_track_color(t);
    char b[24];
    if (gfx_strip_hit(g, 26, 14)) {
        snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
        gfx_text(g, 4, 29, b, col, 1);
        gfx_text(g, 66, 29, bm_lane_name(a, t, sel), C_WHITE, 1);
        int len = tr->lane[sel].len;
        snprintf(b, sizeof b, "P%d/%d L%d", a->page + 1, (len + 15) / 16, len);
        gfx_text_r(g, 236, 29, b, C_GREY, 1);
    }
    for (int l = 0; l < BM_LANES; l++) {
        int y = 41 + l * 9;
        if (!gfx_strip_hit(g, y, 9)) continue;
        bool muted = (tr->mute >> l) & 1;
        int len = tr->lane[l].len ? tr->lane[l].len : 16;
        int ps = bm_app_track_step(a, t, l);
        if (l == sel) gfx_fill(g, 0, y, 240, 9, C_DDGREY);
        char nm[5]; snprintf(nm, sizeof nm, "%s", bm_lane_name(a, t, l));
        gfx_small(g, 1, y + 2, nm, a->now < a->lane_flash[t][l] ? col : (muted ? C_DGREY : (l == sel ? C_WHITE : C_GREY)));
        for (int s = 0; s < 16; s++) {
            int ab = a->page * 16 + s, x = 20 + s * 13 + (s / 4);
            if (ab >= len) { gfx_fill(g, x + 5, y + 3, 2, 2, C_DDGREY); continue; }
            uint8_t v = tr->st[l][ab];
            uint16_t c = muted ? C_DGREY : (l == sel ? col : dim(col));
            if (v & ST_ON) {
                int cond = (v & ST_COND_MASK) >> ST_COND_SH;
                if (cond) gfx_rect(g, x, y, 12, 8, cond == 1 ? C_ORANGE : C_GREY);
                else gfx_fill(g, x, y, 12, 8, (v & ST_ACC) ? C_WHITE : c);
                if (cond && (v & ST_ACC)) gfx_fill(g, x + 2, y + 2, 8, 4, C_WHITE);
                int rat = (v & ST_RAT_MASK) >> ST_RAT_SH;
                for (int r = 0; r < rat; r++) gfx_fill(g, x + 2 + r * 3, y + 6, 2, 2, C_BLACK);
                if (v & ST_FLAM) gfx_fill(g, x + 9, y + 1, 2, 2, C_BLACK);
                if (v & ST_LOCK) gfx_fill(g, x + 1, y + 1, 2, 2, C_BLACK);
            } else {
                gfx_fill(g, x, y, 12, 8, l == sel ? C_DGREY : C_BLACK);
                if (s % 4 == 0) gfx_fill(g, x + 5, y + 3, 2, 2, C_DGREY);
            }
            if (ab == ps) gfx_fill(g, x, y + 7, 12, 2, (v & ST_ON) && !muted ? C_WHITE : C_GREY);   /* cabezal de esta línea */
            if (l == sel && ab == a->held_step && a->held_long) gfx_rect(g, x - 1, y - 1, 14, 10, C_YELLOW);
        }
    }
    if (a->held_step >= 0 && a->held_long && gfx_strip_hit(g, 188, 12)) {
        uint8_t v = tr->st[sel][a->held_step];
        int rat = (v & ST_FLAM) ? 4 : ((v & ST_RAT_MASK) >> ST_RAT_SH);
        static const char *const R[5] = {"X1", "X2", "X3", "X4", "FLAM"};
        static const char *const C[3] = {"SIEMPRE", "FILL", "SIN FILL"};
        snprintf(b, sizeof b, "%s %s %s", (v & ST_ACC) ? "ACENTO" : "NORMAL", R[rat], C[(v & ST_COND_MASK) >> ST_COND_SH]);
        gfx_fill(g, 0, 188, 240, 12, C_DGREY);
        gfx_text(g, 4, 190, b, C_WHITE, 1);
    }
}

/* ------------------------------------------------------------ SEQ synth */
static void draw_seq_syn(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int t = a->track;
    const bm_track *trk = &a->pat.trk[t];
    uint16_t col = bm_track_color(t);
    char b[24], c2[12];
    int len = trk->length ? trk->length : 16;
    int ps = bm_app_track_step(a, t, -1);
    if (gfx_strip_hit(g, 26, 14)) {
        snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
        gfx_text(g, 6, 29, b, col, 1);
        if (a->held_step >= 0 && a->held_long) snprintf(c2, sizeof c2, "PASO %02d", a->held_step + 1);
        else bm_note_str(a->last_note[t], c2, sizeof c2);
        gfx_text(g, 80, 29, c2, C_WHITE, 1);
        snprintf(b, sizeof b, "L%d %s", len, bm_speed_names[trk->speed % SPD_COUNT]);
        gfx_text_r(g, 234, 29, b, C_GREY, 1);
    }
    for (int p = 0; p < 4; p++) {
        int y = 42 + p * 18;
        if (!gfx_strip_hit(g, y, 16)) continue;
        bool cur = p == a->page;
        for (int s = 0; s < 16; s++) {
            int ab = p * 16 + s, x = s * 15;
            if (ab >= len) { gfx_fill(g, x, y + 6, 14, 2, C_DDGREY); continue; }
            int n = 0, lk = 0, wk = 0, ti = 0;
            for (int k = 0; k < BM_EVENTS; k++) {
                const bm_event *e = &a->pat.ev[t - 2][ab][k];
                if (bm_event_empty(e)) continue;
                n++;
                for (int i = 0; i < 4; i++) if (e->lk[i] != BM_LOCK_NONE) lk = 1;
                if (e->prob < 100 || e->prob == BM_PROB_FILL || e->prob == BM_PROB_NOFILL) wk = 1;
                if (e->len & BM_LEN_TIE) ti = 1;
            }
            uint16_t c = cur ? col : dim(col);
            if (n) {
                if (wk) { gfx_rect(g, x, y, 14, 15, c); gfx_rect(g, x + 1, y + 1, 12, 13, c); }
                else gfx_fill(g, x, y, 14, 15, c);
                if (n > 1) gfx_fill(g, x + 10, y + 11, 3, 3, C_BLACK);
                if (ti) gfx_fill(g, x + 14, y + 6, 1, 3, c);
                if (lk) gfx_fill(g, x + 1, y + 1, 3, 3, wk ? c : C_WHITE);
            } else {
                gfx_fill(g, x, y, 14, 15, cur ? C_DDGREY : C_BLACK);
                if (s % 4 == 0) gfx_fill(g, x + 6, y + 13, 2, 2, C_DGREY);
            }
            if (ab == ps) gfx_rect(g, x, y, 14, 15, C_WHITE);
            if (ab == a->held_step && a->held_long) { gfx_rect(g, x, y, 14, 15, C_YELLOW); }
        }
    }
    if (a->held_step >= 0 && a->held_long) {
        if (!gfx_strip_hit(g, 116, 84)) return;
        const bm_event *e = NULL;
        for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(&a->pat.ev[t - 2][a->held_step][k])) { e = &a->pat.ev[t - 2][a->held_step][k]; break; }
        gfx_fill(g, 0, 116, 240, 84, C_DDGREY);
        if (!e) { gfx_text_c(g, 120, 150, "PASO VACIO", C_GREY, 1); return; }
        bm_note_str(e->note, b, sizeof b);
        gfx_text(g, 6, 121, "NOTA", C_GREY, 1); gfx_text(g, 6, 132, b, C_WHITE, 2);
        if (e->prob == BM_PROB_FILL) snprintf(b, sizeof b, "FILL"); else if (e->prob == BM_PROB_NOFILL) snprintf(b, sizeof b, "!FILL"); else snprintf(b, sizeof b, "%d%%", e->prob);
        gfx_text(g, 90, 121, "PROB", C_GREY, 1); gfx_text(g, 90, 132, b, C_WHITE, 2);
        if (e->len & BM_LEN_TIE) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "%d", e->len & BM_LEN_MASK);
        gfx_text(g, 164, 121, "LARGO", C_GREY, 1); gfx_text(g, 164, 132, b, C_WHITE, 2);
        hint(g, 152, "SELECT  ALGORITHM  PRESETS");
        for (int i = 0; i < 4; i++) {
            int x = i * 60 + 4;
            gfx_text(g, x, 166, bm_macro_name(t, i), bm_knob_color(i), 1);
            if (e->lk[i] != BM_LOCK_NONE) bm_fmt_macro(a, t, i, e->lk[i], b, sizeof b); else snprintf(b, sizeof b, "--");
            gfx_text(g, x, 178, b, e->lk[i] != BM_LOCK_NONE ? C_WHITE : C_GREY, 1);
        }
        return;
    }
    for (int tt = 0; tt < BM_TRACKS; tt++) {                  /* las 6 pistas, cada una con su cabezal */
        int y = 118 + tt * 13;
        if (!gfx_strip_hit(g, y, 12)) continue;
        uint16_t tc = a->pat.trk[tt].mute ? C_DGREY : bm_track_color(tt);
        if (tt == t) gfx_fill(g, 0, y - 1, 240, 12, C_DDGREY);
        gfx_fill(g, 2, y + 1, 4, 8, tc);
        int tps_cur = st->playing ? (int)(st->tn[tt] % 64) : -1;
        for (int s = 0; s < 64; s++) {
            int x = 14 + s * 3 + (s / 16) * 6;
            if (s == tps_cur) gfx_fill(g, x, y, 2, 10, C_WHITE);
            else if (track_has(a, tt, s)) gfx_fill(g, x, y + 1, 2, 8, tc);
            else if (s % 4 == 0) gfx_fill(g, x, y + 4, 2, 2, C_DGREY);
        }
    }
}

/* ------------------------------------------------------------ LOOP */
static void draw_loop(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    if (t < 2) { header(g, "LOOP", NULL, C_WHITE); gfx_text_c(g, 120, 100, "SOLO PISTAS SYNTH", C_GREY, 1); return; }
    const bm_loop *lp = &a->pat.loop[t - 2];
    int mode = a->ctx.st.loop_mode[t - 2];
    static const char *const MN[5] = {"OFF", "ARMADO", "GRABANDO", "PLAY", "OVERDUB"};
    static const uint16_t MC[5] = {C_GREY, C_YELLOW, C_RED, C_GREEN, C_ORANGE};
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
    header(g, "LOOP", b, bm_track_color(t));
    gfx_text(g, 6, 56, MN[mode % 5], MC[mode % 5], 3);
    snprintf(b, sizeof b, "%d COMP  %d/%d NOTAS  %s", lp->bars, lp->n, BM_LOOP_EVENTS, lp->on ? "ON" : "OFF");
    gfx_text(g, 6, 86, b, C_WHITE, 1);
    int bx = 6, bw = 228, by = 102, bh = 34;
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
    hint(g, 144, "REC: GRABA > PLAY > OVERDUB");
    hint(g, 156, "EMPIEZA EN EL SIGUIENTE COMPAS");
    hint(g, 168, "REC 2 S: BORRAR EL LOOP");
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

static void draw_sample_box(bm_gfx *g, const bm_app *a, int y, int h)
{
    if (!gfx_strip_hit(g, y, h + 10)) return;
    const bm_smp_src *s = &a->ctx.smp;
    gfx_rect(g, 4, y, 232, h, C_DGREY);
    uint32_t len = a->ctx.st.smp_rec ? a->ctx.smp_rec_pos : s->len;
    uint16_t col = bm_track_color(1);
    if (a->ctx.st.smp_rec) {
        int fill = (int)((uint64_t)len * 230u / BM_SMP_MAX);
        gfx_fill(g, 5, y + 1, fill, h - 2, dim(col));
        gfx_vline(g, 5 + fill, y, h, C_RED);
    } else if (len) {
        for (int x = 0; x < 230; x++) {
            int pk = a->wave[x * 232 / 230] * (h / 2 - 2) / 128;
            gfx_vline(g, 5 + x, y + h / 2 - pk, pk * 2 + 1, col);
        }
    }
    uint32_t L = a->ctx.st.smp_rec ? BM_SMP_MAX : (len ? len : 1);
    for (int i = 0; i < s->nmarks; i++) {
        int x = 5 + (int)((uint64_t)s->marks[i] * 229u / L);
        gfx_vline(g, x, y + 1, h - 2, C_WHITE);
        char b[4]; snprintf(b, sizeof b, "%d", i + 1);
        gfx_small(g, x + 2, y + 3, b, C_WHITE);
    }
    if (!s->nmarks && len && !a->ctx.st.smp_rec)
        for (int i = 1; i < 16; i++) gfx_vline(g, 5 + 229 * i / 16, y + h - 7, 5, C_GREY);
}

static void draw_edit(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[30];
    if (t == 0) {
        header(g, "KIT", bm_rom_kit_names[tr->source % BM_KITS], bm_track_color(0));
        lane_pads(g, a, 0, 52, 0);
        return;
    }
    if (t == 1) {
        snprintf(b, sizeof b, "SLOT %c", 'A' + tr->source % 2);
        header(g, "SAMPLE", b, bm_track_color(1));
        draw_sample_box(g, a, 52, 54);
        lane_pads(g, a, 1, 112, 0);
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
    hint(g, 178, t == 2 ? "MONO + GLIDE (TIE)" : "POLIFONICO (POOL 7 VOCES)");
}

static void draw_inst(bm_gfx *g, const bm_app *a)
{
    int t = bm_is_tr(a->track) ? a->track : 0, l = a->sel_lane[t];
    const bm_lane *ln = &a->pat.tr[t].lane[l];
    char b[24];
    snprintf(b, sizeof b, "INST %d", l + 1);
    header(g, bm_lane_name(a, t, l), b, bm_track_color(t));
    static const char *const NM[4] = {"NIVEL", "AFINA", "DECAY", "LARGO"};
    const int val[4] = {ln->level, ln->tune, ln->decay, ln->len * 2};
    for (int i = 0; i < 4; i++) {
        int x = 6 + i * 58;
        if (!gfx_strip_hit(g, 54, 70)) break;
        gfx_text(g, x, 54, NM[i], bm_knob_color(i), 1);
        gfx_fill(g, x + 10, 66, 20, 56, C_DDGREY);
        int h = val[i] * 56 / 128;
        gfx_fill(g, x + 10, 122 - h, 20, h, bm_knob_color(i));
    }
    int y = 134;
    if (gfx_strip_hit(g, y, 20)) {
        int ps = bm_app_track_step(a, t, l);
        for (int s = 0; s < 64; s++) {
            int x = 6 + s * 3 + (s / 16) * 4;
            uint16_t c = s >= ln->len ? C_BLACK : ((a->pat.tr[t].st[l][s] & ST_ON) ? bm_track_color(t) : C_DGREY);
            if (s == ps) c = C_WHITE;
            gfx_fill(g, x, y, 2, 14, c);
        }
    }
    hint(g, 160, "BLANCAS: TOCAR Y ELEGIR");
    hint(g, 172, "SELECT: INSTRUMENTO");
}

/* ------------------------------------------------------------ ENV, LFO, FX */
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
        hint(g, 162, "UN DISPARO: SOLO DECAY");
        return;
    }
    int A = 4 + tr->attack * 50 / 127, D = 6 + tr->macro[2] * 60 / 127, S = 50, R = 6 + tr->release * 60 / 127;
    int sy = yb - tr->sustain * h / 127;
    int x1 = x0 + A, x2 = x1 + D, x3 = x2 + S, x4 = x3 + R;
    for (int o = 0; o < 2; o++) {
        gfx_line(g, x0, yb + o, x1, y0 + o, bm_knob_color(0));
        gfx_line(g, x1, y0 + o, x2, sy + o, bm_knob_color(1));
        gfx_line(g, x2, sy + o, x3, sy + o, bm_knob_color(2));
        gfx_line(g, x3, sy + o, x4, yb + o, bm_knob_color(3));
    }
    if (a->env_page == 1) {
        snprintf(b, sizeof b, "ENV > FILTRO %d%%", tr->env_amt * 100 / 127);
        gfx_text(g, 6, 162, b, C_WHITE, 1);
        snprintf(b, sizeof b, "GLIDE %dMS (CON TIE)", tr->glide * 500 / 127);
        hint(g, 176, b);
    } else hint(g, 162, "ENV OTRA VEZ = PAGINA 2");
}

static void draw_lfo(bm_gfx *g, const bm_app *a)
{
    (void)a;
    header(g, "LFO", NULL, C_WHITE);
    gfx_text_c(g, 120, 90, "RESERVADO", C_GREY, 2);
    gfx_text_c(g, 120, 120, "DECISION 5 DE LA SPEC", C_GREY, 1);
}

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
    if (a->ctx.arena_busy) { gfx_fill(g, 4, 52, 114, 70, C_DDGREY); gfx_text(g, 10, 80, "EN PAUSA:", C_ORANGE, 1); gfx_text(g, 10, 92, "SAMPLE SIN", C_ORANGE, 1); gfx_text(g, 10, 104, "GUARDAR", C_ORANGE, 1); }
    snprintf(b, sizeof b, "DRIVE %d%%  (GLO+K4)", a->set.drive * 100 / 127);
    gfx_text(g, 6, 134, b, C_WHITE, 1);
    hint(g, 152, "MANTEN FX = PUNCH-IN");
    hint(g, 164, "ENVIO POR PISTA: HOME K3");
    hint(g, 176, "DUCK POR PISTA: HOME K4");
}

/* ------------------------------------------------------------ escala y acordes */
static void draw_keyboard(bm_gfx *g, const bm_app *a)
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
        int note = bm_scale_note(m->root, m->scale, i, base_oct);
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
        if (i < 7) {
            char nm[10];
            bm_chord_name(m->root, m->scale, i, 0, nm, sizeof nm);
            nm[3] = 0;
            gfx_small(g, x + 1, ky + 42, nm, a->keys_down[k] ? C_BLACK : C_WHITE);
        } else {
            static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
            gfx_small(g, x + 2, ky + 42, MODS[i - 7], a->keys_down[k] ? C_BLACK : C_YELLOW);
        }
    }
}

static void draw_scl(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    char b[24];
    snprintf(b, sizeof b, "%s %s", bm_note_names[m->root % 12], bm_scale_names[m->scale % BM_SCALES]);
    gfx_text(g, 6, 30, "ESCALA", C_GREY, 1);
    gfx_text(g, 6, 42, b, C_WHITE, 2);
    if (a->chord_name[0]) gfx_text_r(g, 234, 42, a->chord_name, C_YELLOW, 2);
    if (a->track >= 2) draw_keyboard(g, a);
    else gfx_text_c(g, 120, 100, "ELIGE UNA PISTA SYNTH (T3-T6)", C_GREY, 1);
    hint(g, 162, "SCL OTRA VEZ: CUADRICULA");
    hint(g, 174, "NEGRAS 1-7: ACORDES");
    hint(g, 186, "NEGRAS 8-11: +7 +9 SUS2 SUS4");
}

static void draw_chords(bm_gfx *g, const bm_app *a, int live)
{
    const bm_meta *m = &a->pat.meta;
    char b[24];
    int t = bm_app_chord_track(a);
    if (gfx_strip_hit(g, 26, 22)) {
        gfx_text(g, 4, 28, live ? "EN VIVO" : bm_chord_set_names[m->chord_set % BM_CHORD_SETS], live ? C_YELLOW : C_GREY, 1);
        snprintf(b, sizeof b, "%s %.5s T%d", bm_note_names[m->root % 12], bm_scale_names[m->scale % BM_SCALES], t + 1);
        gfx_text(g, 4, 38, b, C_WHITE, 1);
        if (a->last_pad >= 0) { gfx_text_r(g, 236, 28, a->chord_name, C_WHITE, 2); }
    }
    static const uint16_t FN[3] = {C_BLUE, C_GREEN, C_ORANGE};
    for (int p = 0; p < 16; p++) {
        int x = (p % 4) * 60, y = 50 + (p / 4) * 33;
        if (!gfx_strip_hit(g, y, 32)) continue;
        bm_pad_chord pc = bm_chord_pad(m->chord_set, p);
        uint16_t fc = FN[bm_chord_function(pc.degree)];
        int k = bm_white_key(p);
        bool on = a->keys_down[k];
        bool last = p == a->last_pad;
        gfx_fill(g, x + 1, y, 58, 31, on ? fc : (last ? dim(fc) : C_DDGREY));
        gfx_fill(g, x + 1, y, 3, 31, fc);
        char rn[10], nm[10];
        bm_roman(m->root, m->scale, pc.degree, pc.ext, rn, sizeof rn);
        bm_chord_name(m->root, m->scale, pc.degree, pc.ext, nm, sizeof nm);
        gfx_text(g, x + 8, y + 5, rn, on ? C_BLACK : C_WHITE, 1);
        gfx_text(g, x + 8, y + 18, nm, on ? C_BLACK : fc, 1);
    }
    if (gfx_strip_hit(g, 184, 14)) {
        gfx_fill(g, 4, 188, 8, 6, C_BLUE); gfx_small(g, 14, 189, "REPOSO", C_GREY);
        gfx_fill(g, 64, 188, 8, 6, C_GREEN); gfx_small(g, 74, 189, "PREPARA", C_GREY);
        gfx_fill(g, 130, 188, 8, 6, C_ORANGE); gfx_small(g, 140, 189, "TENSION", C_GREY);
        gfx_small(g, 196, 189, "N1-4 MOD", C_GREY);
    }
}

/* ------------------------------------------------------------ ARP y ROLL */
static void draw_arp(bm_gfx *g, const bm_app *a, int live)
{
    static const char *const M[5] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM"};
    static const char *const R[3] = {"1/8", "1/16", "1/32"};
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(t));
    header(g, live ? "ARP VIVO" : "ARP", b, live ? C_YELLOW : bm_track_color(t));
    if (t < 2) { gfx_text_c(g, 100, 90, "EN BATERIA Y SAMPLER:", C_GREY, 1); gfx_text_c(g, 120, 104, "MANTEN ARP = ROLL", C_WHITE, 1); return; }
    gfx_text(g, 6, 56, M[tr->arp_mode % 5], C_WHITE, 3);
    snprintf(b, sizeof b, "%s  %d OCT  GATE %d%%", R[tr->arp_rate % 3], tr->arp_oct, tr->arp_gate);
    gfx_text(g, 6, 84, b, C_GREY, 1);
    const bm_arp *ar = &a->ctx.arp[t];
    for (int i = 0; i < 8; i++) {
        int x = 10 + i * 28, h = 10 + i * 6;
        bool has = i < ar->n;
        gfx_fill(g, x, 160 - h, 22, h, has ? (ar->playing && ar->cur % 12 == ar->held[i] % 12 ? C_WHITE : bm_track_color(t)) : C_DDGREY);
    }
    hint(g, 172, tr->arp_mode ? "SIEMPRE ACTIVO EN ESTA PISTA" : "MANTEN ARP = ARPEGIO EN VIVO");
}

static void draw_roll(bm_gfx *g, const bm_app *a)
{
    int t = bm_is_tr(a->track) ? a->track : 0;
    header(g, "ROLL", bm_roll_names[a->roll_rate % 5], C_YELLOW);
    lane_pads(g, a, t, 52, 3);
    hint(g, 186, a->rec ? "REC: SE GRABA COMO REPETICION" : "K1: VELOCIDAD DEL ROLL");
}

/* ------------------------------------------------------------ listas */
static void row(bm_gfx *g, int y, bool sel, const char *l, const char *r)
{
    if (!gfx_strip_hit(g, y, 16)) return;
    if (sel) gfx_fill(g, 0, y, BM_LCD_W, 15, C_DGREY);
    gfx_text(g, 6, y + 4, l, sel ? C_WHITE : C_GREY, 1);
    if (r) gfx_text_r(g, 234, y + 4, r, C_WHITE, 1);
}

static void draw_system(bm_gfx *g, const bm_app *a)
{
    const bm_settings *s = &a->set;
    char v[10][18];
    snprintf(v[0], 18, "%s", s->clock ? "EXTERNO" : "INTERNO");
    snprintf(v[1], 18, "%s", s->sync_mode ? "SPLIT L/R" : "ESTEREO");
    snprintf(v[2], 18, "%d PPQN", s->sync_ppqn);
    snprintf(v[3], 18, "%d MS", s->sync_lat);
    snprintf(v[4], 18, "%s", s->autosave ? "ON" : "OFF");
    snprintf(v[5], 18, "%s", s->led_dim == 0 ? "OFF" : (s->led_dim == 1 ? "TENUE" : "MEDIO"));
    snprintf(v[6], 18, "RESAMPLE");
    snprintf(v[7], 18, "%d/%d SLOTS", bm_store_used_slots(&a->store), (int)BM_STORE_SLOTS);
    unsigned la = a->smp_valid[0] ? (unsigned)(a->smp_info[0].len * 10u / BM_SMP_RATE) : 0, lb = a->smp_valid[1] ? (unsigned)(a->smp_info[1].len * 10u / BM_SMP_RATE) : 0;
    snprintf(v[8], 18, "A %u.%uS B %u.%uS", la / 10, la % 10, lb / 10, lb % 10);
    snprintf(v[9], 18, "%s", BM_VERSION_STR);
    static const char *const L[10] = {"RELOJ", "SALIDA SYNC", "SYNC PPQN", "SYNC LATENCIA", "AUTOGUARDADO", "LED REPOSO", "FUENTE SAMPLER", "PATRONES", "SAMPLES", "VERSION"};
    for (int i = 0; i < 10; i++) row(g, 26 + i * 17, i == a->menu_row, L[i], v[i]);
}

static void draw_glo(bm_gfx *g, const bm_app *a)
{
    const bm_meta *m = &a->pat.meta;
    char b[16];
    header(g, "GLOBAL", NULL, C_WHITE);
    const char *nm[4] = {"BPM", "SWING", "MAESTRO", "DRIVE"};
    char v[4][12];
    snprintf(v[0], 12, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10);
    snprintf(v[1], 12, "%d%%", m->swing);
    if (m->master) snprintf(v[2], 12, "%d", m->master * 16); else snprintf(v[2], 12, "INF");
    snprintf(v[3], 12, "%d%%", a->set.drive * 100 / 127);
    for (int i = 0; i < 4; i++) {
        int x = (i % 2) * 118 + 4, y = 52 + (i / 2) * 52;
        if (!gfx_strip_hit(g, y, 48)) continue;
        gfx_rect(g, x, y, 114, 48, bm_knob_color(i));
        gfx_text(g, x + 6, y + 6, nm[i], bm_knob_color(i), 1);
        gfx_text(g, x + 6, y + 20, v[i], C_WHITE, 2);
    }
    if (m->fill_every) snprintf(b, sizeof b, "CADA %d COMP", m->fill_every); else snprintf(b, sizeof b, "OFF");
    gfx_text(g, 6, 162, "FILL AUTO (SELECT):", C_GREY, 1);
    gfx_text(g, 126, 162, b, C_ORANGE, 1);
    hint(g, 176, "GLO SOSTENIDO + KNOB = IGUAL");
}

static void draw_save(bm_gfx *g, const bm_app *a)
{
    char r0[24], r1[24], r4[24];
    snprintf(r0, sizeof r0, "A%02d", a->ctx.st.pattern + 1);
    snprintf(r1, sizeof r1, "A%02d", a->save_target + 1);
    snprintf(r4, sizeof r4, "%d", bm_pattern_count_events(&a->pat));
    header(g, "SAVE", a->dirty ? "SIN GUARDAR" : "GUARDADO", C_WHITE);
    row(g, 52, a->save_row == 0, "GUARDAR", r0);
    row(g, 70, a->save_row == 1, "GUARDAR COMO", r1);
    row(g, 88, a->save_row == 2, "GUARDAR SONG Y ESCENAS", NULL);
    row(g, 106, a->save_row == 3, a->clear_armed ? "BORRAR? SAVE OTRA VEZ" : "BORRAR PATRON", NULL);
    row(g, 124, a->save_row == 4, "EVENTOS", r4);
    hint(g, 150, "SELECT ELIGE, SAVE EJECUTA");
    hint(g, 162, "MANTEN SAVE 2 S: RAPIDO");
    gfx_text(g, 6, 174, a->ctx.st.playing ? "SOLO EN STOP (CORTA EL AUDIO)" : "SOLO EN STOP", a->ctx.st.playing ? C_ORANGE : C_GREY, 1);
}

static void draw_song(bm_gfx *g, const bm_app *a)
{
    char b[24];
    header(g, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? C_YELLOW : C_WHITE);
    if (!a->song.n) {
        gfx_text_c(g, 120, 90, "VACIA", C_GREY, 2);
        gfx_text_c(g, 120, 120, "BLANCA = ANADIR PATRON", C_GREY, 1);
        gfx_text_c(g, 120, 134, "O GRABA ESCENAS (HOME HOME)", C_GREY, 1);
        return;
    }
    int first = a->song_cur > 6 ? a->song_cur - 6 : 0;
    for (int i = 0; i < 8 && first + i < a->song.n; i++) {
        int e = first + i;
        uint8_t p = a->song.pat[e];
        if (p & 0x80) snprintf(b, sizeof b, "%2d  ESC %c  x%d", e + 1, 'A' + (p & 3), a->song.rep[e]);
        else snprintf(b, sizeof b, "%2d  A%02d    x%d", e + 1, p + 1, a->song.rep[e]);
        row(g, 50 + i * 16, e == a->song_cur, b, (a->ctx.st.song_on && a->ctx.st.playing && e == a->ctx.st.song_pos) ? ">" : NULL);
    }
    hint(g, 186, "BLANCA AGREGA - NEGRA 9 BORRA");
}

/* ------------------------------------------------------------ TIEMPO (SEQ 2 s) */
static void draw_time(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    header(g, "TIEMPO", "POLIMETRO", C_WHITE);
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
        int len = bm_is_tr(t) ? a->pat.tr[t].lane[sel ? a->time_lane : 0].len : tr->length;
        if (len < 1) len = 1;
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
    if (bm_is_tr(a->time_row)) {
        char b[24];
        snprintf(b, sizeof b, "K3 INSTR: %s", bm_lane_name(a, a->time_row, a->time_lane));
        hint(g, 190, b);
    } else hint(g, 190, "LINEAS = 1 COMPAS");
}

/* ------------------------------------------------------------ SAMPLER (EDIT 2 s en T2) */
static void draw_sampler(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int slot = a->pat.trk[1].source % 2;
    char b[30];
    if (gfx_strip_hit(g, 26, 22)) {
        gfx_text(g, 6, 30, "SAMPLER", bm_track_color(1), 2);
        for (int s = 0; s < 2; s++) {
            int x = 168 + s * 34;
            gfx_fill(g, x, 28, 30, 16, s == slot ? bm_track_color(1) : C_DDGREY);
            snprintf(b, sizeof b, "%c", 'A' + s);
            gfx_text_c(g, x + 15, 32, b, s == slot ? C_BLACK : (a->smp_valid[s] ? C_WHITE : C_GREY), 1);
        }
    }
    const char *state = st->smp_rec ? "GRABANDO" : st->smp_unsaved ? "SIN GUARDAR" : a->smp_valid[slot] ? "LISTO" : "VACIO";
    gfx_text(g, 6, 52, state, st->smp_rec ? C_RED : st->smp_unsaved ? C_ORANGE : C_WHITE, 2);
    uint32_t len = st->smp_rec ? a->ctx.smp_rec_pos : a->ctx.smp.len;
    unsigned ms = (unsigned)(len * 1000u / BM_SMP_RATE), mx = (unsigned)(BM_SMP_MAX * 1000u / BM_SMP_RATE);
    snprintf(b, sizeof b, "%u.%02u / %u.%02u S", ms / 1000, ms % 1000 / 10, mx / 1000, mx % 1000 / 10);
    gfx_text_r(g, 234, 56, b, C_WHITE, 1);
    draw_sample_box(g, a, 74, 62);
    snprintf(b, sizeof b, "%d CHOPS  16 KHZ 8 BIT", a->ctx.smp.nmarks ? a->ctx.smp.nmarks : 16);
    gfx_text(g, 6, 142, b, C_GREY, 1);
    hint(g, 156, "MANTEN REC: GRABA (MAX 3 S)");
    hint(g, 168, "BLANCAS AL GRABAR: CHOPS");
    hint(g, 180, "NEGRA 11 X2: BORRAR SLOT");
}

/* ------------------------------------------------------------ escenas */
static void draw_scenes(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[24];
    header(g, "ESCENAS", a->song_rec ? "SONG REC" : "1-4 LANZA 5-8 GUARDA", a->song_rec ? C_RED : C_CYAN);
    for (int s = 0; s < BM_SCENES; s++) {
        int x = (s % 2) * 118 + 2, y = 50 + (s / 2) * 62;
        if (!gfx_strip_hit(g, y, 58)) continue;
        const bm_scene *sc = &a->song.scene[s];
        bool cur = st->scene == s, q = st->scene_q == s;
        gfx_fill(g, x, y, 114, 58, cur ? C_CYAN : C_DDGREY);
        if (q && ((a->now / 200) & 1)) { gfx_rect(g, x, y, 114, 58, C_YELLOW); gfx_rect(g, x + 1, y + 1, 112, 56, C_YELLOW); }
        snprintf(b, sizeof b, "%c", 'A' + s);
        gfx_text(g, x + 6, y + 6, b, cur ? C_BLACK : C_WHITE, 3);
        if (!sc->valid) { gfx_text(g, x + 34, y + 12, "VACIA", C_GREY, 1); continue; }
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
        int x = 6;
        gfx_text(g, x, 178, "CADENA:", C_GREY, 1);
        for (int i = 0; i < a->chain_n; i++) {
            snprintf(b, sizeof b, "%c", 'A' + a->chain[i]);
            gfx_text(g, 52 + i * 12, 178, b, i == st->chain_pos ? C_YELLOW : C_WHITE, 1);
        }
    } else hint(g, 178, "MANTEN UNA + OTRAS = CADENA");
}

/* ------------------------------------------------------------ capas */
static void draw_bank(bm_gfx *g, const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    char b[8];
    header(g, "PATRONES", "CAMBIA AL CICLO", C_WHITE);
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
    hint(g, 180, "PUNTO NARANJA: SIN GUARDAR");
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

static void draw_lanemute(bm_gfx *g, const bm_app *a)
{
    int t = bm_is_tr(a->track) ? a->track : 0;
    header(g, "MUTE INSTR", bm_track_name(t), bm_track_color(t));
    lane_pads(g, a, t, 52, 1);
}

static void draw_erase(bm_gfx *g, const bm_app *a)
{
    int t = a->track;
    header(g, "BORRAR", a->ctx.st.playing ? "AL PASAR" : "TODO", C_RED);
    if (bm_is_tr(t)) lane_pads(g, a, t, 52, 2);
    else { hint(g, 60, "SONANDO: MANTEN UNA BLANCA"); hint(g, 72, "Y SE BORRA LO QUE PASA"); hint(g, 90, "PARADO: BLANCA = ESE PASO"); }
    hint(g, 182, "N1 COPIA N2 PEGA N3 LIMPIA");
    gfx_text(g, 6, 192, "OCT- DESHACER  OCT+ REHACER", C_YELLOW, 1);
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
        bool off = a->ctx.arena_busy && i <= 6;
        gfx_fill(g, x + 1, y, 28, 50, on ? C_WHITE : C_DDGREY);
        gfx_small(g, x + 15 - gfx_small_w(N1[i]) / 2, y + 14, N1[i], on ? C_BLACK : (off ? C_DGREY : C_WHITE));
        gfx_small(g, x + 15 - gfx_small_w(N2[i]) / 2, y + 26, N2[i], on ? C_BLACK : (off ? C_DGREY : C_YELLOW));
    }
    hint(g, 180, a->ctx.arena_busy ? "1-7 EN PAUSA: SAMPLE SIN GUARDAR" : "SUELTA LA TECLA = SECO");
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
    case V_SCENES: draw_scenes(g, a); break;
    case V_SEQ: if (bm_is_tr(a->track)) draw_seq_tr(g, a); else draw_seq_syn(g, a); break;
    case V_LOOP: draw_loop(g, a); break;
    case V_EDIT: draw_edit(g, a); break;
    case V_INST: draw_inst(g, a); break;
    case V_ENV: draw_env(g, a); break;
    case V_LFO: draw_lfo(g, a); break;
    case V_FX: draw_fx(g, a); break;
    case V_SCL: draw_scl(g, a); break;
    case V_CHORDS: draw_chords(g, a, 0); break;
    case V_CHORDLIVE: draw_chords(g, a, 1); break;
    case V_ARP: draw_arp(g, a, 0); break;
    case V_ARPLIVE: draw_arp(g, a, 1); break;
    case V_ROLL: draw_roll(g, a); break;
    case V_SAVE: draw_save(g, a); break;
    case V_GLO: draw_glo(g, a); break;
    case V_SYSTEM: draw_system(g, a); break;
    case V_SONG: draw_song(g, a); break;
    case V_TIME: draw_time(g, a); break;
    case V_SAMPLER: draw_sampler(g, a); break;
    case V_BANK: draw_bank(g, a); break;
    case V_MUTE: draw_mute(g, a); break;
    case V_LANEMUTE: draw_lanemute(g, a); break;
    case V_ERASE: draw_erase(g, a); break;
    case V_PUNCH: draw_punch(g, a); break;
    default: break;
    }
    if (a->ov.on) draw_overlay(g, a);
    if (a->toast_until) draw_toast(g, a);
    gfx_noclip(g);
    draw_legend(g, a);
    if (a->now < a->clip_until) {
        gfx_fill(g, 0, 0, BM_LCD_W, 2, C_RED); gfx_fill(g, 0, BM_LCD_H - 2, BM_LCD_W, 2, C_RED);
        gfx_fill(g, 0, 0, 2, BM_LCD_H, C_RED); gfx_fill(g, BM_LCD_W - 2, 0, 2, BM_LCD_H, C_RED);
    }
}
