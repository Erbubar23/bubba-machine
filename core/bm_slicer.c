/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — el Slicer (T2) por dentro, 2026-10-22: un sampler de pads como el KO II que ya
 * teníamos (cada pad, su inicio y su largo dentro de una toma; pueden encimarse). Sus botones:
 *   SEQ        la rejilla por pasos (V_CH, la de la batería); SEQ otra vez: LENGTH AND SPEED
 *   LFO, SCL   en la rejilla: ACCENTS y MUTE PADS
 *   EDIT       TRIM: la onda de la toma con el pad, START LENGTH ZOOM; negras AUTO, 8 y 16 cortan la
 *              toma del pad en los pads 1..n y escriben la secuencia que suena como ella; la negra 11
 *              (SAMPLE) graba lo que suena (K4: cuántos compases) · SOUND: PITCH LEVEL MODE REVERSE
 *   ENV        ATTACK RELEASE y el loop del modo LOOP (LOOP START / END)
 *   FX         como las de panel: DISTORTION; pág 2 CHORUS y REVERB (el DELAY, desde HOME)
 *   GLO        LENGTH (todos los pads), MIDI CH, MIDI OUT; pág 2 RESET
 *   blancas    los 16 pads (suena y se elige) · ALGORITHM el pad · PRESETS su toma (entera)
 * Un menú con páginas no se sale: su botón otra vez pasa a la siguiente; el de una página, otra vez,
 * vuelve a la rejilla. HOME sale a la consola. */
#include <stdio.h>
#include <string.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_params.h"
#include "bm_lang.h"

#define SL 1                                          /* el canal del Slicer (pista interna T2) */

const char *const slicer_mode_names[PAD_MODES] = {"ONESHOT", "KEY", "THRU", "LOOP"};
static const char *const DIRS[2] = {"FWD", "REV"};
static const char *const ZOOMS[4] = {"X1", "X2", "X4", "X8"};

/* lo que mueve cada perilla del Slicer (además de las de siempre: K_PRM, K_ON, K_TRK) */
enum { S_START = 1, S_LEN, S_ZOOM, S_BARS, S_PITCH, S_LEVEL, S_MODE, S_REV, S_ATK, S_REL, S_LOOPS, S_LOOPE, S_KLEN, S_RST };
#define SK(n, id, mx) {n, K_SLICE, id, 0, mx, F_INT, NULL}

static const bxp SL_EDIT[2][4] = {
    {SK("START", S_START, 255), SK("LENGTH", S_LEN, 255), SK("ZOOM", S_ZOOM, 3), SK("SAMPLE", S_BARS, 2)},
    {SK("PITCH", S_PITCH, 48), SK("LEVEL", S_LEVEL, 127), SK("MODE", S_MODE, PAD_MODES - 1), SK("REVERSE", S_REV, 1)}};
static const char *const SL_EDIT_N[2] = {"TRIM", "SOUND"};
static const bxp SL_ENV[4] = {SK("ATTACK", S_ATK, 127), SK("RELEASE", S_REL, 127), SK("LOOP IN", S_LOOPS, 254), SK("LOOP OUT", S_LOOPE, 255)};
static const bxp SL_FX[2][4] = {
    {{"DRIVE", K_PRM, PX_DS_G, 0, 127, F_PCT, NULL}, {"TONE", K_PRM, PX_DS_T, 0, 127, F_PCT, NULL},
     {"LEVEL", K_PRM, PX_DS_L, 0, 127, F_PCT, NULL}, {"ON", K_ON, PXF_DIST, 0, 1, F_ONOFF, NULL}},
    {{"CHORUS", K_PRM, PX_CHORUS, 0, 2, F_LIST, bm_px_chorus_names}, NONE,
     {"REVERB", K_TRK, TRK(rsend), 0, 127, F_PCT, NULL}, {"REV ON", K_ON, PXF_REVERB, 0, 1, F_ONOFF, NULL}}};
static const bxp SL_GLO[2][4] = {
    {SK("LENGTH", S_KLEN, BM_STEPS), NONE, {"MIDI CH", K_TRK, TRK(midi_ch), 0, 15, F_CH, NULL}, {"MIDI OUT", K_TRK, TRK(midi_out), 0, 1, F_ONOFF, NULL}},
    {NONE, NONE, NONE, SK("RESET", S_RST, 1)}};
/* la rejilla (V_CH): LEVEL, LENGTH (todos los pads), PITCH y RELEASE del pad elegido */
static const bxp SL_GRID[4] = {SK("LEVEL", S_LEVEL, 127), SK("LENGTH", S_KLEN, BM_STEPS), SK("PITCH", S_PITCH, 48), SK("RELEASE", S_REL, 127)};

static int pad_sel(const bm_app *a) { return a->sel_lane[SL] & 15; }
static bm_pad *pad_of(bm_app *a) { return &a->pat.pad[pad_sel(a)]; }
static const bm_pad *cpad(const bm_app *a) { return &a->pat.pad[pad_sel(a)]; }

int slicer_page_of(int btn)
{
    switch (btn) {
    case BTN_EDIT: return BXP_MAIN;
    case BTN_ENV: return BXP_ENV;
    case BTN_FX: return BXP_FX;
    case BTN_GLO: return BXP_GLO;
    default: return -1;                                   /* SEQ, LFO y SCL: la rejilla */
    }
}

static int pages(int p) { return p == BXP_ENV ? 1 : 2; }
int slicer_sub(const bm_app *a) { return a->bx.sub[a->bx.page % BXP_N] % pages(a->bx.page); }

const bxp *slicer_knobs(const bm_app *a)
{
    int s = slicer_sub(a);
    switch (a->bx.page) {
    case BXP_ENV: return SL_ENV;
    case BXP_FX: return SL_FX[s];
    case BXP_GLO: return SL_GLO[s];
    default: return SL_EDIT[s];
    }
}

/* ------------------------------------------------------------ la toma del pad y la ventana de TRIM */
bool slicer_take(const bm_app *a, int pad, bm_smp_src *m)
{
    const bm_pad *p = &a->pat.pad[pad & 15];
    return bm_pad_ok(p) && bm_app_src_info(a, p->src, m) && m->len;
}

/* la ventana de la onda: la toma entera (X1) o, con zoom, alrededor del inicio del pad (a un tercio,
 * para ver más de lo que sigue) */
void slicer_view(const bm_app *a, uint32_t *v0, uint32_t *v1)
{
    bm_smp_src m;
    *v0 = *v1 = 0;
    if (!slicer_take(a, pad_sel(a), &m)) return;
    uint32_t span = m.len >> (a->bx.szoom & 3), at = cpad(a)->start;
    if (span < 232) span = m.len < 232 ? m.len : 232;
    uint32_t s = at > span / 3 ? at - span / 3 : 0;
    if (s + span > m.len) s = m.len - span;
    *v0 = s; *v1 = s + span;
}

/* compases que entran en una toma nueva (SAMPLE): 1, 2 o 4, los que caben en la RAM de grabación */
static uint32_t bar_samples(const bm_app *a) { return (uint32_t)BM_SMP_RATE * 2400u / (a->pat.meta.bpm_x10 ? a->pat.meta.bpm_x10 : 1200u); }
static int bars_of(int i) { return 1 << (i & 3); }
static int bars_max(const bm_app *a)                  /* el índice más alto que cabe (0 = 1 compás) */
{
    int i = 2;
    while (i > 0 && (uint32_t)bars_of(i) * bar_samples(a) > BM_SMP_REC_MAX) i--;
    return i;
}
static int bars_idx(const bm_app *a) { int i = a->bx.sbars > 2 ? 0 : a->bx.sbars, mx = bars_max(a); return i > mx ? mx : i; }

/* ------------------------------------------------------------ leer y escribir */
int slicer_get(const bm_app *a, const bxp *p)
{
    if (p->kind != K_SLICE) return mach_get(a, p);
    const bm_pad *d = cpad(a);
    const bm_tr *tr = &a->pat.tr[SL];
    switch (p->idx) {
    case S_START: return d->start;
    case S_LEN: return d->len;
    case S_ZOOM: return a->bx.szoom & 3;
    case S_BARS: return bars_idx(a);
    case S_PITCH: return d->pitch;
    case S_LEVEL: return tr->lane[pad_sel(a)].level;
    case S_MODE: return d->mode % PAD_MODES;
    case S_REV: return (d->flags & PAD_REV) != 0;
    case S_ATK: return d->attack;
    case S_REL: return d->release;
    case S_LOOPS: return d->loop_s;
    case S_LOOPE: return d->loop_e;
    case S_KLEN: return tr->lane[pad_sel(a)].len ? tr->lane[pad_sel(a)].len : 16;
    default: return 0;
    }
}

/* ¿la perilla hace algo ahora? (el loop solo en modo LOOP; el pad vacío no tiene sonido que tocar) */
bool slicer_live(const bm_app *a, const bxp *p)
{
    if (p->kind == K_NONE) return false;
    if (p->kind != K_SLICE) return true;
    bool ok = bm_pad_ok(cpad(a));
    switch (p->idx) {
    case S_LOOPS: case S_LOOPE: return ok && cpad(a)->mode == PAD_LOOP;
    case S_START: case S_LEN: case S_PITCH: case S_MODE: case S_REV: case S_ATK: case S_REL: return ok;
    default: return true;
    }
}

static void ms(char *b, int cap, uint32_t samples)
{
    unsigned t = (unsigned)(samples * 1000u / BM_SMP_RATE);
    if (t >= 1000) snprintf(b, (size_t)cap, "%u.%03u S", t / 1000, t % 1000);
    else snprintf(b, (size_t)cap, "%u MS", t);
}

void slicer_fmt(const bm_app *a, const bxp *p, int v, char *b, int cap)
{
    if (p->kind != K_SLICE) { mach_fmt(p, v, b, cap); return; }
    switch (p->idx) {
    case S_START: case S_LEN: ms(b, cap, (uint32_t)v); break;
    case S_ZOOM: snprintf(b, (size_t)cap, "%s", ZOOMS[v & 3]); break;
    case S_BARS: snprintf(b, (size_t)cap, v ? "%d BARS" : "%d BAR", bars_of(v)); break;
    case S_PITCH: snprintf(b, (size_t)cap, v > 24 ? "+%d ST" : "%d ST", v - 24); break;   /* el snprintf del SDK no tiene %+d */
    case S_MODE: snprintf(b, (size_t)cap, "%s", slicer_mode_names[v % PAD_MODES]); break;
    case S_REV: snprintf(b, (size_t)cap, "%s", DIRS[v & 1]); break;
    case S_ATK: snprintf(b, (size_t)cap, "%u MS", (unsigned)bm_attack_ms(v)); break;
    case S_REL: case S_LEVEL: snprintf(b, (size_t)cap, "%d%%", (v * 100 + 63) / 127); break;
    case S_LOOPS: snprintf(b, (size_t)cap, "%d%%", v * 100 / 255); break;
    case S_LOOPE: if (v) snprintf(b, (size_t)cap, "%d%%", v * 100 / 255); else snprintf(b, (size_t)cap, "%s", "END"); break;
    case S_KLEN: snprintf(b, (size_t)cap, v > 1 ? "%d STEPS" : "%d STEP", v); break;
    case S_RST: snprintf(b, (size_t)cap, "%s", "TURN"); break;
    default: snprintf(b, (size_t)cap, "%d", v); break;
    }
}

int slicer_pct(const bm_app *a, const bxp *p, int v)
{
    if (p->kind != K_SLICE) return mach_pct(p, v);
    bm_smp_src m;
    switch (p->idx) {
    case S_START: return slicer_take(a, pad_sel(a), &m) ? (int)((uint32_t)v * 255u / m.len) : 0;
    case S_LEN: return slicer_take(a, pad_sel(a), &m) ? (int)((uint32_t)v * 255u / m.len) : 0;
    case S_BARS: return v * 127;
    case S_KLEN: return (v - 1) * 255 / (BM_STEPS - 1);
    default: return p->max ? v * 255 / p->max : 0;
    }
}

/* Todos los pads vuelven al sonido de fábrica (los cortes quedan) y los efectos a los de fábrica */
static void sound_reset(bm_app *a)
{
    for (int i = 0; i < BM_LANES; i++) {
        bm_pad *d = &a->pat.pad[i];
        d->pitch = 24; d->mode = PAD_ONESHOT; d->flags &= (uint8_t)~PAD_REV;
        d->attack = 0; d->release = 20; d->loop_s = 0; d->loop_e = 0;   /* como un pad nuevo (bm_pad_clear) */
        app_push_i(a, CMD_SET_LANE, SL, (uint8_t)i, LN_LEVEL, 100);
    }
    bm_px_fx_defaults(a->pat.prm[SL]);
    app_edited(a);
    app_toast(a, "SOUND RESET");
}

/* K de TRIM: START mueve el inicio (el final queda), LENGTH el final. De a dos columnas de la ventana
 * por detent (con aceleración); ni más corto que 64 muestras ni fuera de la toma. */
static int trim(bm_app *a, int which, int d)
{
    bm_smp_src m;
    if (!slicer_take(a, pad_sel(a), &m)) return -1;
    bm_pad *p = pad_of(a);
    uint32_t v0, v1;
    slicer_view(a, &v0, &v1);
    int32_t px = (int32_t)((v1 - v0) / 116u);
    if (px < 8) px = 8;
    int32_t end = (int32_t)p->start + p->len;
    if (end > (int32_t)m.len) end = (int32_t)m.len;
    if (which == 0) {
        int32_t s = bm_clampi((int32_t)p->start + d * px, 0, end - 64);
        p->start = (uint16_t)s; p->len = (uint16_t)(end - s);
    } else {
        int32_t e = bm_clampi(end + d * px, (int32_t)p->start + 64, (int32_t)m.len);
        p->len = (uint16_t)(e - (int32_t)p->start);
    }
    app_edited(a);
    return which ? p->len : p->start;
}

int slicer_turn(bm_app *a, const bxp *p, int d)
{
    if (p->kind != K_SLICE) return mach_turn(a, p, d);
    if (!slicer_live(a, p)) return -1;                    /* sin función ahora: no hace nada */
    bm_pad *q = pad_of(a);
    int l = pad_sel(a), cur = slicer_get(a, p), v;
    int step = p->max <= 16 ? app_lst(a, d) : d;
    switch (p->idx) {
    case S_START: return trim(a, 0, d);
    case S_LEN: return trim(a, 1, d);
    case S_ZOOM: v = bm_clampi(cur + step, 0, 3); a->bx.szoom = (uint8_t)v; return v == cur ? -1 : v;
    case S_BARS: v = bm_clampi(cur + step, 0, bars_max(a)); a->bx.sbars = (uint8_t)v; return v == cur ? -1 : v;
    case S_RST: sound_reset(a); return -1;
    case S_KLEN: app_kit_len(a, SL, p == &SL_GRID[1] ? 1 : 0, d); return -1;   /* el aviso lo da ella (K2 en la rejilla, K1 en GLO) */
    default: break;
    }
    v = bm_clampi(cur + step, 0, p->max);
    if (v == cur) return -1;
    switch (p->idx) {
    case S_PITCH: q->pitch = (uint8_t)v; break;
    case S_LEVEL: app_push_i(a, CMD_SET_LANE, SL, (uint8_t)l, LN_LEVEL, v); break;
    case S_MODE: q->mode = (uint8_t)v; break;
    case S_REV: q->flags = (uint8_t)(v ? q->flags | PAD_REV : q->flags & ~PAD_REV); break;
    case S_ATK: q->attack = (uint8_t)v; break;
    case S_REL: q->release = (uint8_t)v; break;
    case S_LOOPS: q->loop_s = (uint8_t)v; if (q->loop_e && q->loop_e <= v) q->loop_e = (uint8_t)(v + 1); break;
    case S_LOOPE: if (v && v <= q->loop_s) v = q->loop_s + 1; q->loop_e = (uint8_t)v; break;
    default: return -1;
    }
    app_edited(a);
    return v;
}

void slicer_legend_of(const bm_app *a, const bxp *P, bm_legend L[4])
{
    char b[12];
    for (int k = 0; k < 4; k++) {
        if (!slicer_live(a, &P[k])) continue;             /* sin función: nada (ni el nombre) */
        int v = slicer_get(a, &P[k]);
        slicer_fmt(a, &P[k], v, b, sizeof b);
        app_layer_legend(a, L, k, P[k].name, b, slicer_pct(a, &P[k], v));
    }
}

void slicer_legend(const bm_app *a, bm_legend L[4]) { slicer_legend_of(a, slicer_knobs(a), L); }

/* el giro: en TRIM y SOUND las perillas están dibujadas (sin el recuadro grande); en las demás, la
 * línea de arriba dice qué se movió */
static void turned(bm_app *a, int k, const bxp *p, int v)
{
    if (v < 0) return;
    if (a->view == V_MACH && a->bx.page == BXP_MAIN) { a->ov.on = 0; return; }
    char b[12], ti[22];
    slicer_fmt(a, p, v, b, sizeof b);
    snprintf(ti, sizeof ti, "P%d %s", pad_sel(a) + 1, T(p->name));
    app_overlay(a, k, ti, b, "", slicer_pct(a, p, v), p->idx == S_PITCH && p->kind == K_SLICE ? 128 : -1, bm_knob_color(k));
}

/* la rejilla (V_CH): K1-K4 del pad elegido */
void slicer_grid_knob(bm_app *a, int k, int d) { if (k >= 0 && k < 4) turned(a, k, &SL_GRID[k], slicer_turn(a, &SL_GRID[k], d)); }
void slicer_grid_legend(const bm_app *a, bm_legend L[4]) { slicer_legend_of(a, SL_GRID, L); }

/* ------------------------------------------------------------ cortar y grabar */
static void cut(bm_app *a, int how)
{
    const bm_pad *p = cpad(a);
    if (!bm_pad_ok(p) || p->src >= BM_TAKES) return;      /* pad vacío: no hay qué cortar */
    int n = app_kit_from_take(a, p->src, how);
    char b[30];
    snprintf(b, sizeof b, TF("%d PADS + SEQUENCE"), n);
    app_toast(a, b);
}

/* SAMPLE: graba lo que suena (la mezcla) desde el próximo compás, los compases de K4; parado,
 * empieza con PLAY. Otra vez: para. La toma se guarda al parar y queda en el pad elegido. */
static void sample(bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    if (st->smp_rec || st->smp_armed) { app_push_i(a, CMD_SMP_REC, 0, 0, 0, 0); app_toast(a, "SAMPLE STOPPED"); return; }
    if (st->smp_unsaved) { app_toast(a, "STOP: THE LAST SAMPLE IS SAVED FIRST"); return; }
    uint32_t cap = (uint32_t)bars_of(bars_idx(a)) * bar_samples(a);
    if (cap > BM_SMP_REC_MAX) cap = BM_SMP_REC_MAX;
    if (cap > bm_app_rec_room(a)) { app_toast(a, "NO ROOM: DELETE A TAKE"); return; }
    app_push_i(a, CMD_SMP_REC, 2, 0, 0, (int32_t)cap);
    app_toast(a, app_playing(a) ? "SAMPLE: NEXT BAR" : "SAMPLE ARMED: PRESS PLAY");
}

/* ------------------------------------------------------------ botones, teclas y perillas */
bool slicer_button(bm_app *a, int id, bm_gkind k)
{
    if (id == BTN_REC) {                                  /* REC: graba los pads que tocas; 2 s borra la secuencia */
        if (k == G_LONG) mach_rec(a, k);
        else if (k == G_TAP) app_rec_set(a, !a->rec);
        return true;
    }
    if (k != G_TAP && k != G_HOLD) return true;
    if (id == BTN_SEQ || id == BTN_LFO || id == BTN_SCL) {   /* la rejilla, o su capa */
        app_slicer_grid(a, id == BTN_LFO ? L_ACC : id == BTN_SCL ? L_MUTE : L_NONE);
        return true;
    }
    int p = slicer_page_of(id);
    if (p < 0) return true;                               /* lo demás (OCT, ARP...) no hace nada */
    bm_bx7ui *u = &a->bx;
    if (u->page != p) { u->page = (uint8_t)p; u->sub[p] = 0; }
    else if (pages(p) > 1) u->sub[p] = (uint8_t)((u->sub[p] + 1) % pages(p));   /* la siguiente, en círculo */
    else { app_slicer_grid(a, L_NONE); return true; }    /* de una página: otra vez, la rejilla */
    a->ov.on = 0;
    return true;
}

void slicer_key(bm_app *a, int k, int w, int b)
{
    if (w >= 0) { app_pad_down(a, k, w); return; }        /* blancas: los pads */
    if (a->bx.page != BXP_MAIN || slicer_sub(a) != 0) return;   /* las negras, solo en TRIM */
    switch (b) {
    case 0: cut(a, 0); return;                            /* AUTO: en los golpes */
    case 1: cut(a, 8); return;
    case 2: cut(a, 16); return;
    case 10: sample(a); return;
    default: return;
    }
}

void slicer_encoder(bm_app *a, int id, int d)
{
    char b[24];
    switch (id) {
    case ENC_SELECT: return;                              /* las páginas, con su botón */
    case ENC_ALGO: {                                      /* el pad */
        int v = bm_clampi(pad_sel(a) + app_lst(a, d), 0, BM_LANES - 1);
        if (v == pad_sel(a)) return;
        a->sel_lane[SL] = (uint8_t)v;
        if (bm_pad_ok(cpad(a))) bm_app_src_name(a, cpad(a)->src, b, sizeof b); else snprintf(b, sizeof b, "%s", T("EMPTY"));
        char ti[12]; snprintf(ti, sizeof ti, TF("PAD %d"), v + 1);
        if (a->bx.page == BXP_MAIN) a->ov.on = 0;         /* TRIM y SOUND ya lo muestran */
        else app_overlay(a, -1, ti, b, "", v * 17, -1, bm_track_color(SL));
        return;
    }
    case ENC_PRESETS: app_enc_source(a, SL, d); return;   /* la toma del pad, entera */
    default: break;
    }
    int k = id - ENC_K1;
    if (k < 0 || k > 3) return;
    const bxp *P = slicer_knobs(a);
    turned(a, k, &P[k], slicer_turn(a, &P[k], d));
}

void slicer_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    (void)a;
    *sel = "-"; *alg = "PAD"; *pre = "TAKE";
}

uint8_t slicer_btn_led(const bm_app *a, int i)
{
    switch (i) {
    case BTN_HOME: return LED_DIM;
    case BTN_REC: return a->rec ? LED_ON : LED_DIM;
    case BTN_SEQ: case BTN_LFO: case BTN_SCL: return LED_DIM;
    default: break;
    }
    int p = slicer_page_of(i);
    if (p < 0) return LED_OFF;
    return p == a->bx.page ? LED_ON : LED_DIM;
}

const char *slicer_title(const bm_app *a, char *buf, int cap)
{
    int s = slicer_sub(a);
    switch (a->bx.page) {
    case BXP_ENV: snprintf(buf, (size_t)cap, "%s", "ENV"); break;
    case BXP_FX: snprintf(buf, (size_t)cap, "FX %d/2 %s", s + 1, T(s ? "CHORUS REVERB" : "DISTORTION")); break;
    case BXP_GLO: snprintf(buf, (size_t)cap, "GLO %d/2", s + 1); break;
    default: snprintf(buf, (size_t)cap, "%s %d/2", T(SL_EDIT_N[s]), s + 1); break;
    }
    return buf;
}

/* las blancas son los 16 pads: el elegido ON, con toma MID (destella al sonar), vacío OFF. En TRIM, las
 * negras AUTO, 8 y 16 (con un pad que tenga toma) y SAMPLE (armado respira, grabando ON). */
void slicer_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    uint16_t col = bm_track_color(SL);
    char b[6];
    for (int w = 0; w < 16; w++) {
        bm_keyfn *f = &K[bm_white_key(w)];
        bool ok = bm_pad_ok(&a->pat.pad[w]);
        snprintf(b, sizeof b, "P%d", w + 1);
        snprintf(f->label, sizeof f->label, "%s", b);
        f->led = w == pad_sel(a) ? LED_ON : ok ? LED_MID : LED_OFF;
        f->color = col;
        f->live = (uint8_t)(ok && a->now < a->lane_flash[SL][w]);
    }
    if (a->bx.page != BXP_MAIN || slicer_sub(a) != 0) return;
    bool ok = bm_pad_ok(cpad(a)) && cpad(a)->src < BM_TAKES;
    static const char *const CUT[3] = {"AUTO", "8", "16"};
    for (int i = 0; i < 3; i++) {
        bm_keyfn *f = &K[bm_black_key(i)];
        snprintf(f->label, sizeof f->label, "%s", CUT[i]);
        f->led = ok ? LED_MID : LED_OFF; f->color = C_CYAN;
    }
    const bm_status *st = &a->ctx.st;
    bm_keyfn *f = &K[bm_black_key(10)];
    snprintf(f->label, sizeof f->label, "%s", "SMP");
    f->led = st->smp_rec ? LED_ON : st->smp_armed ? LED_BREATH : LED_DIM; f->color = C_RED;
}

/* la cabecera: lo que graba SAMPLE, o el pad elegido y su toma */
void slicer_status(const bm_app *a, char *b, int cap, uint16_t *col)
{
    const bm_status *st = &a->ctx.st;
    *col = C_WHITE;
    if (st->smp_rec) {
        unsigned t = (unsigned)(st->smp_len * 10u / BM_SMP_RATE);
        snprintf(b, (size_t)cap, TF("SAMPLING %u.%u S"), t / 10, t % 10);
        *col = C_RED;
        return;
    }
    if (st->smp_armed) { snprintf(b, (size_t)cap, "%s", T(app_playing(a) ? "SAMPLE: NEXT BAR" : "SAMPLE ARMED")); *col = C_RED; return; }
    char tk[16];
    if (bm_pad_ok(cpad(a))) bm_app_src_name(a, cpad(a)->src, tk, sizeof tk); else snprintf(tk, sizeof tk, "%s", T("EMPTY"));
    snprintf(b, (size_t)cap, "P%d %s", pad_sel(a) + 1, tk);
}

bool slicer_knobs_shown(const bm_app *a) { return a->bx.page == BXP_MAIN || a->bx.page == BXP_GLO; }
