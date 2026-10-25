/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — el BB-303 por dentro: la TB-303 de Roland en el FM-1 (2026-10-22).
 *
 * El panel de la 303 en las perillas, en su orden:
 *   K1-K4      CUTOFF, RESO, ENV MOD, DECAY (la fila del medio del panel)
 *   ALGORITHM  ACCENT (la perilla de la derecha del panel) · OCT sostenido + ALGORITHM: TUNING
 *   EDIT otra vez: la página 2: TUNING, WAVE (SAW / SQUARE), SLIDE (el tiempo), VOLUME
 *   PRESETS    los sonidos de fábrica
 * Tocar ligado (apretar la siguiente antes de soltar) = SLIDE; con REC se graba.
 * Botones: EDIT el panel (MAIN), SEQ el piano roll, FX los efectos del TB-03 (página 1 OVERDRIVE;
 * FX otra vez, página 2 DELAY y REVERB, que son los envíos del canal), SCL el ACCENT de cada paso (SCL
 * otra vez: el SLIDE de cada paso; las 16 blancas son los 16 pasos de la página, SELECT cambia de
 * página), LFO el LFO (un añadido: la 303 no tiene), ARP el arpegiador, GLO el MIDI. Un menú con
 * páginas (EDIT, FX, SCL, ARP) no se sale: su botón otra vez pasa a la página siguiente, en círculo;
 * se sale con otro menú o HOME. El de una sola página, otra vez, vuelve al panel. ENV no hace nada. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_params.h"
#include "bm_lang.h"

static const char *const WAVES[2] = {"SAW", "SQUARE"};

static const bxp P_MAIN[2][4] = {
    {{"CUTOFF", K_MAC, 0, 0, 127, F_OHZ, NULL}, {"RESO", K_MAC, 1, 0, 127, F_PCT, NULL},
     {"ENV MOD", K_MAC, 2, 0, 127, F_PCT, NULL}, {"DECAY", K_MAC, 3, 0, 127, F_DMS, NULL}},
    {{"TUNING", K_TRK, TRK(transpose), 0, 48, F_TRSP, NULL}, {"WAVE", K_PRM, AC_WAVE, 0, 1, F_LIST, WAVES},
     {"SLIDE", K_TRK, TRK(glide), 0, 127, F_GLIDE, NULL}, {"VOLUME", K_TRK, TRK(level), 0, 127, F_PCT, NULL}}};
static const bxp P_ACCENT = {"ACCENT", K_TRK, TRK(accent), 0, 127, F_PCT, NULL};
static const bxp P_TUNING = {"TUNING", K_TRK, TRK(transpose), 0, 48, F_TRSP, NULL};   /* = el editor (bm_ac_pages) */
static const bxp P_FX[2][4] = {
    {{"DRIVE", K_PRM, AC_OD_G, 0, 127, F_PCT, NULL}, {"TONE", K_PRM, AC_OD_T, 0, 127, F_PCT, NULL},
     {"LEVEL", K_PRM, AC_OD_L, 0, 127, F_PCT, NULL}, {"ON", K_ON, ACF_OD, 0, 1, F_ONOFF, NULL}},
    {{"DELAY", K_TRK, TRK(send), 0, 127, F_PCT, NULL}, {"DLY ON", K_ON, ACF_DELAY, 0, 1, F_ONOFF, NULL},
     {"REVERB", K_TRK, TRK(rsend), 0, 127, F_PCT, NULL}, {"REV ON", K_ON, ACF_REVERB, 0, 1, F_ONOFF, NULL}}};
const char *const b303_fx_names[ACF_N] = {"OVERDRIVE", "DELAY", "REVERB"};
static const bxp P_LFO[4] = {
    {"RATE", K_PRM, AC_LFO_RATE, 0, AC_LFO_RATES - 1, F_LIST, bm_acid_lfo_rates},
    {"DEPTH", K_PRM, AC_LFO_DEPTH, 0, 127, F_PCT, NULL},
    {"WAVE", K_PRM, AC_LFO_WAVE, 0, ACW_N - 1, F_LIST, bm_acid_lfo_waves},
    {"DEST", K_PRM, AC_LFO_DEST, 0, ACL_DESTS - 1, F_LIST, bm_acid_lfo_dests}};
static const bxp P_NONE[4] = {NONE, NONE, NONE, NONE};
static const char *const PAGE_NAMES[BXP_N] = {"PANEL", "SEQ", "FX", "", "LFO", "", "GLO", "ARP", "ACCENT"};

int b303_page_of(int btn)
{
    switch (btn) {
    case BTN_EDIT: return BXP_MAIN;
    case BTN_SEQ: return BXP_SEQ;
    case BTN_FX: return BXP_FX;
    case BTN_GLO: return BXP_GLO;
    case BTN_ARP: return BXP_ARP;
    case BTN_LFO: return BXP_LFO;
    case BTN_SCL: return BXP_SCL;
    default: return -1;
    }
}

static const bxp *cur(const bm_app *a, int *n)
{
    const bm_bx7ui *u = &a->bx;
    switch (u->page) {
    case BXP_SEQ: *n = 1; return MACH_SEQ;
    case BXP_FX: *n = 2; return P_FX[u->sub[BXP_FX] % 2];
    case BXP_LFO: *n = 1; return P_LFO;
    case BXP_SCL: *n = 1; return P_NONE;
    case BXP_GLO: *n = 2; return MACH_GLO[u->sub[BXP_GLO] % 2];
    case BXP_ARP: *n = 2; return MACH_ARP[u->sub[BXP_ARP] % 2];
    default: *n = 2; return P_MAIN[u->sub[BXP_MAIN] % 2];
    }
}

/* el panel está a la vista: se ve girar la perilla, sin el recuadro grande encima */
static bool panel(const bm_app *a) { return a->bx.page == BXP_MAIN; }

static void turned(bm_app *a, int k, const bxp *p, int v)
{
    if (v < 0) return;
    if (panel(a)) { a->ov.on = 0; return; }
    char b[12], ti[22];
    mach_fmt(p, v, b, sizeof b);
    snprintf(ti, sizeof ti, "%s %s", a->bx.page == BXP_FX ? "FX" : a->bx.page == BXP_LFO ? "LFO" : "BB-303", T(p->name));
    app_overlay(a, k, ti, b, "", mach_pct(p, v), p->fmt == F_TRSP ? 128 : -1, k >= 0 ? bm_knob_color(k) : C_ORANGE);
}

bool b303_button(bm_app *a, int id, bm_gkind k)
{
    int p = b303_page_of(id);
    if (p < 0) return true;                                   /* lo demás no hace nada en la 303 */
    if (k != G_TAP && k != G_HOLD) return true;
    bm_bx7ui *u = &a->bx;
    int n = p == BXP_MAIN || p == BXP_FX || p == BXP_SCL || p == BXP_ARP || p == BXP_GLO ? 2 : 1;   /* sus páginas */
    if (u->page != p) {                                       /* otro menú: entra en su página 1 */
        u->page = (uint8_t)p;
        u->sub[p] = 0;
        if (p == BXP_SCL) u->blk = (uint8_t)(u->step % mach_len(a) / 16);
    } else if (n > 1) u->sub[p] = (uint8_t)((u->sub[p] + 1) % n);   /* con páginas: la siguiente, en círculo */
    else u->page = BXP_MAIN;                                  /* de una página: otra vez, el panel */
    a->ov.on = 0;
    return true;
}

/* SCL: la blanca w prende o apaga el ACCENT (o el SLIDE) del paso w de la página; un paso sin nota no.
 * La usa también el B-MINI (ACCENT y GLIDE). */
void mach_scl_toggle(bm_app *a, int w)
{
    int t = a->track, s = a->bx.blk * 16 + w;
    bool slide = a->bx.sub[BXP_SCL] % 2 != 0;
    if (s >= mach_len(a)) return;
    bool has = false, on = false;
    for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&a->pat, t, s, e);
        if (bm_event_empty(ev)) continue;
        has = true;
        on |= slide ? (ev->len & BM_LEN_TIE) != 0 : ev->vel >= BM_VEL_ACC;
    }
    char b[24];
    if (!has) { snprintf(b, sizeof b, TF("STEP %d: NO NOTE"), s + 1); app_toast(a, b); return; }
    for (int e = 0; e < BM_EVENTS; e++) {
        bm_event ev = *bm_evc(&a->pat, t, s, e);
        if (bm_event_empty(&ev)) continue;
        if (slide) ev.len = (uint8_t)(on ? ev.len & ~BM_LEN_TIE : ev.len | BM_LEN_TIE);
        else ev.vel = (uint8_t)(on ? 100 : BM_VEL_ACC);
        app_push_ev(a, t, s, e, &ev);
    }
    app_edited(a);
}

void b303_key(bm_app *a, int k, int w, int b)
{
    (void)b;
    if (a->bx.page == BXP_SCL) { if (w >= 0 && w < 16) mach_scl_toggle(a, w); return; }   /* las negras: nada */
    mach_play(a, k, 100);
}

/* SCL: las 16 blancas son los pasos de la página; ON = con ACCENT (o SLIDE), DIM = con nota */
void b303_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    int t = a->track, L = mach_len(a);
    bool slide = a->bx.sub[BXP_SCL] % 2 != 0;
    for (int w = 0; w < 16; w++) {
        int s = a->bx.blk * 16 + w;
        bm_keyfn *f = &K[bm_white_key(w)];
        if (s >= L) continue;
        bool has = false, on = false;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = bm_evc(&a->pat, t, s, e);
            if (bm_event_empty(ev)) continue;
            has = true;
            on |= slide ? (ev->len & BM_LEN_TIE) != 0 : ev->vel >= BM_VEL_ACC;
        }
        snprintf(f->label, sizeof f->label, "%d", s + 1);
        f->led = on ? LED_ON : has ? LED_DIM : LED_OFF;
        f->color = slide ? C_GREEN : C_ORANGE;
        f->live = (uint8_t)(a->ctx.st.playing && bm_app_track_step(a, t, -1) == s);
    }
}

void b303_encoder(bm_app *a, int id, int d)
{
    bm_bx7ui *u = &a->bx;
    int n;
    switch (id) {
    case ENC_SELECT:
        if (u->page == BXP_SCL) {                             /* la página de 16 pasos */
            int nb = (mach_len(a) + 15) / 16;
            u->blk = (uint8_t)((u->blk + app_lst(a, d) + nb * 8) % nb);
        }
        return;                                               /* las páginas, con su botón */
    case ENC_ALGO:
        if (bm_gesture_is_down(&a->ges, BTN_OCT_DN) || bm_gesture_is_down(&a->ges, BTN_OCT_UP)) {   /* OCT + ALGORITHM: TUNING */
            a->oct_used = 1;
            turned(a, -1, &P_TUNING, mach_turn(a, &P_TUNING, app_lst(a, d)));
            return;
        }
        turned(a, -1, &P_ACCENT, mach_turn(a, &P_ACCENT, d));
        return;
    case ENC_PRESETS: app_enc_source(a, a->track, d); return;
    default: break;
    }
    int k = id - ENC_K1;
    if (k < 0 || k > 3) return;
    const bxp *P = cur(a, &n);
    turned(a, k, &P[k], mach_turn(a, &P[k], d));
}

void b303_legend(const bm_app *a, bm_legend L[4])
{
    int n;
    mach_legend(a, cur(a, &n), L);
}

void b303_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    int n;
    cur(a, &n);
    *sel = a->bx.page == BXP_SCL ? "STEPS" : a->bx.page == BXP_SEQ ? "STEP" : "-";
    *alg = "ACCENT";
    *pre = "PRESET";
}

uint8_t b303_btn_led(const bm_app *a, int i)
{
    int p = b303_page_of(i);
    if (p < 0) return LED_OFF;
    return p == a->bx.page ? LED_ON : LED_DIM;
}

const char *b303_title(const bm_app *a, char *buf, int cap)
{
    int n;
    cur(a, &n);
    const bm_bx7ui *u = &a->bx;
    if (u->page == BXP_FX) snprintf(buf, (size_t)cap, "FX %d/2 %s", u->sub[BXP_FX] % 2 + 1, T(u->sub[BXP_FX] % 2 ? "DELAY REVERB" : "OVERDRIVE"));
    else if (u->page == BXP_SCL) snprintf(buf, (size_t)cap, "%s %d-%d", T(u->sub[BXP_SCL] % 2 ? "SLIDE" : "ACCENT"), u->blk * 16 + 1, u->blk * 16 + 16);
    else if (n > 1) snprintf(buf, (size_t)cap, "%s %d/%d", T(PAGE_NAMES[u->page % BXP_N]), u->sub[u->page % BXP_N] % n + 1, n);
    else snprintf(buf, (size_t)cap, "%s", T(PAGE_NAMES[u->page % BXP_N]));
    return buf;
}
