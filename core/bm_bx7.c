/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — la BX-7 por dentro: el FM-1 de fábrica adaptado a Bubba (2026-10-21).
 *
 * Se entra desde HOME con el cursor en su canal: SEQ, FX, ENV, LFO, EDIT o ARP llevan directo a esa
 * página (la tecla blanca del canal sigue entrando a tocar en PLAY/REC). Adentro, como en el FM-1:
 *   ALGORITHM  el algoritmo (1-32), con el dibujo · OCT sostenido + ALGORITHM: transposición ±12
 *   PRESETS    los sonidos de fábrica (los bancos de usuario, apagados hasta tener todas las máquinas)
 *   SELECT     la página del botón (o la casilla en FX) · K1-K4 lo que dice la pantalla
 *   OCT -/+    la octava (±3); los dos juntos: octava y transposición a 0
 *   FX ENV LFO EDIT GLO ARP SEQ: su página; el mismo botón otra vez vuelve a MAIN · HOME a la consola
 *   SCL        en FX toma la casilla elegida y SELECT la mueve en la cadena (otra vez SCL la suelta)
 *   REC        sonando: graba ya; parado: se arma y la primera nota arranca y graba (vacío: toma
 *              libre, el largo se cierra con REC); sostenido 2 s: borra las notas del canal
 *   EDIT       negras 1-6 OP1-OP6, 7 PIT (envolvente de afinación), 8 GLO (algoritmo, feedback...),
 *              9 MONO, 10 POLY; las blancas siguen tocando
 * Todo lo de la voz se escribe con CMD_FMV en pat->fmv; lo que va encima (BRIGHT, la página ENV) en
 * el canal; los efectos en prm (BX_*). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_params.h"
#include "bm_fm.h"
#include "bm_rom.h"
#include "bm_lang.h"

static const char *const WAVES[6] = {"TRI", "SAW DN", "SAW UP", "SQUARE", "SINE", "S&H"};
static const char *const CURVES[4] = {"-LIN", "-EXP", "+EXP", "+LIN"};
static const char *const MODES[2] = {"RATIO", "FIXED"};
static const char *const FTYPES[3] = {"LP", "BP", "HP"};

static const bxp P_MAIN[1][4] = {{
    {"FEEDBACK", K_FMV, BM_FM_FB, 0, 7, F_INT, NULL}, {"BRIGHT", K_MAC, 0, 0, 127, F_BIP, NULL},
    {"PORTA", K_TRK, TRK(glide), 0, 127, F_PCT, NULL}, {"KEY VEL", K_TRK, TRK(accent), 0, 127, F_PCT, NULL}}};
static const bxp P_ENV[1][4] = {{
    {"ATTACK", K_TRK, TRK(attack), 0, 127, F_PCT, NULL}, {"DECAY", K_MAC, 2, 0, 127, F_BIP, NULL},
    {"SUSTAIN", K_TRK, TRK(sustain), 0, 127, F_BIP, NULL}, {"RELEASE", K_MAC, 3, 0, 127, F_BIP, NULL}}};
static const bxp P_LFO[2][4] = {
    {{"WAVE", K_FMV, 142, 0, 5, F_LIST, WAVES}, {"SPEED", K_FMV, 137, 0, 99, F_INT, NULL},
     {"PMD", K_FMV, 139, 0, 99, F_INT, NULL}, {"AMD", K_FMV, 140, 0, 99, F_INT, NULL}},
    {{"DELAY", K_FMV, 138, 0, 99, F_INT, NULL}, {"P SENS", K_FMV, 143, 0, 7, F_INT, NULL},
     {"LFO SYNC", K_FMV, 141, 0, 1, F_ONOFF, NULL}, {"OSC SYNC", K_FMV, 136, 0, 1, F_ONOFF, NULL}}};
static const bxp P_GLO2[4] = {{"TRANSP", K_TRK, TRK(transpose), 12, 36, F_TRSP, NULL}, NONE, MACH_QUANT, {"RESET", K_RST, 0, 0, 1, F_INT, NULL}};   /* GLO pág 2 */
/* FX: una página por casilla, en el orden de las casillas del FM-1; K4 siempre ON/OFF */
static const bxp P_FX[BXF_N][4] = {
    {{"TYPE", K_PRM, BX_FLT_T, 0, 2, F_LIST, FTYPES}, {"CUTOFF", K_PRM, BX_FLT_C, 0, 127, F_HZ, NULL},
     {"RESO", K_PRM, BX_FLT_Q, 0, 127, F_PCT, NULL}, {"ON", K_ON, BXF_FILTER, 0, 1, F_ONOFF, NULL}},
    {{"SEND", K_TRK, TRK(rsend), 0, 127, F_PCT, NULL}, NONE, NONE, {"ON", K_ON, BXF_REVERB, 0, 1, F_ONOFF, NULL}},
    {{"SEND", K_TRK, TRK(send), 0, 127, F_PCT, NULL}, NONE, NONE, {"ON", K_ON, BXF_DELAY, 0, 1, F_ONOFF, NULL}},
    {{"GAIN", K_PRM, BX_DST_G, 0, 127, F_PCT, NULL}, {"TONE", K_PRM, BX_DST_T, 0, 127, F_PCT, NULL},
     {"LEVEL", K_PRM, BX_DST_L, 0, 127, F_PCT, NULL}, {"ON", K_ON, BXF_DIST, 0, 1, F_ONOFF, NULL}},
    {{"RATE", K_PRM, BX_CHO_F, 0, 127, F_PCT, NULL}, {"DEPTH", K_PRM, BX_CHO_D, 0, 127, F_PCT, NULL},
     {"MIX", K_PRM, BX_CHO_M, 0, 127, F_PCT, NULL}, {"ON", K_ON, BXF_CHORUS, 0, 1, F_ONOFF, NULL}},
    {{"RATE", K_PRM, BX_PHA_F, 0, 127, F_PCT, NULL}, {"DEPTH", K_PRM, BX_PHA_D, 0, 127, F_PCT, NULL},
     {"MIX", K_PRM, BX_PHA_M, 0, 127, F_PCT, NULL}, {"ON", K_ON, BXF_PHASER, 0, 1, F_ONOFF, NULL}}};
const char *const bx7_fx_names[BXF_N] = {"FILTER", "REVERB", "DELAY", "DIST", "CHORUS", "PHASER"};
/* EDIT: un operador en 6 páginas, como el FM-1 */
static const bxp P_OP[6][4] = {
    {{"RATE 1", K_OP, FMO_R1, 0, 99, F_INT, NULL}, {"RATE 2", K_OP, FMO_R2, 0, 99, F_INT, NULL},
     {"RATE 3", K_OP, FMO_R3, 0, 99, F_INT, NULL}, {"RATE 4", K_OP, FMO_R4, 0, 99, F_INT, NULL}},
    {{"LEVEL 1", K_OP, FMO_L1, 0, 99, F_INT, NULL}, {"LEVEL 2", K_OP, FMO_L2, 0, 99, F_INT, NULL},
     {"LEVEL 3", K_OP, FMO_L3, 0, 99, F_INT, NULL}, {"LEVEL 4", K_OP, FMO_L4, 0, 99, F_INT, NULL}},
    {{"DETUNE", K_OP, FMO_DT, 0, 14, F_DT, NULL}, {"COARSE", K_OP, FMO_FC, 0, 31, F_INT, NULL},
     {"FINE", K_OP, FMO_FF, 0, 99, F_INT, NULL}, {"MODE", K_OP, FMO_MODE, 0, 1, F_LIST, MODES}},
    {{"AMP MOD", K_OP, FMO_AMS, 0, 3, F_INT, NULL}, {"VEL SENS", K_OP, FMO_KVS, 0, 7, F_INT, NULL},
     {"OUT LVL", K_OP, FMO_OL, 0, 99, F_INT, NULL}, NONE},
    {{"L DEPTH", K_OP, FMO_LD, 0, 99, F_INT, NULL}, {"R DEPTH", K_OP, FMO_RD, 0, 99, F_INT, NULL},
     {"BREAK", K_OP, FMO_BP, 0, 99, F_NOTE, NULL}, NONE},
    {{"L CURVE", K_OP, FMO_LC, 0, 3, F_LIST, CURVES}, {"R CURVE", K_OP, FMO_RC, 0, 3, F_LIST, CURVES},
     {"RATE SCL", K_OP, FMO_RS, 0, 7, F_INT, NULL}, NONE}};   /* (el orden de las páginas del manual del FM-1) */
static const char *const OP_PAGES[6] = {"RATES", "LEVELS", "FREQ", "OUTPUT", "SCALING", "CURVES"};
static const bxp P_PIT[2][4] = {
    {{"RATE 1", K_FMV, 126, 0, 99, F_INT, NULL}, {"RATE 2", K_FMV, 127, 0, 99, F_INT, NULL},
     {"RATE 3", K_FMV, 128, 0, 99, F_INT, NULL}, {"RATE 4", K_FMV, 129, 0, 99, F_INT, NULL}},
    {{"LEVEL 1", K_FMV, 130, 0, 99, F_INT, NULL}, {"LEVEL 2", K_FMV, 131, 0, 99, F_INT, NULL},
     {"LEVEL 3", K_FMV, 132, 0, 99, F_INT, NULL}, {"LEVEL 4", K_FMV, 133, 0, 99, F_INT, NULL}}};
static const bxp P_EGLO[1][4] = {{
    {"ALGORITHM", K_FMV, BM_FM_ALG, 0, 31, F_P1, NULL}, {"FEEDBACK", K_FMV, BM_FM_FB, 0, 7, F_INT, NULL},
    {"OSC SYNC", K_FMV, 136, 0, 1, F_ONOFF, NULL}, {"TRANSP", K_FMV, BM_FM_TRSP, 0, 48, F_TRSP, NULL}}};

static const char *const PAGE_NAMES[BXP_N] = {"MAIN", "SEQ", "FX", "ENV", "LFO", "EDIT", "GLO", "ARP"};

int bx7_page_of(int btn)
{
    switch (btn) {
    case BTN_SEQ: return BXP_SEQ;
    case BTN_FX: return BXP_FX;
    case BTN_ENV: return BXP_ENV;
    case BTN_LFO: return BXP_LFO;
    case BTN_EDIT: return BXP_EDIT;
    case BTN_GLO: return BXP_GLO;
    case BTN_ARP: return BXP_ARP;
    default: return -1;
    }
}

static int page_btn(int p)
{
    static const int8_t B[BXP_N] = {-1, BTN_SEQ, BTN_FX, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_ARP};
    return p >= 0 && p < BXP_N ? B[p] : -1;
}

static int fx_at(const bm_app *a, int pos) { return a->pat.prm[a->track][BX_ORD0 + pos % BXF_N] % BXF_N; }

/* las 4 perillas de la página de ahora y cuántas páginas tiene (SELECT) */
static const bxp *cur(const bm_app *a, int *n)
{
    const bm_bx7ui *u = &a->bx;
    switch (u->page) {
    case BXP_SEQ: *n = 1; return MACH_SEQ;
    case BXP_ENV: *n = 1; return P_ENV[0];
    case BXP_LFO: *n = 2; return P_LFO[u->sub[BXP_LFO] % 2];
    case BXP_GLO: *n = 2; return u->sub[BXP_GLO] % 2 ? P_GLO2 : MACH_GLO[0];
    case BXP_ARP: *n = 2; return MACH_ARP[u->sub[BXP_ARP] % 2];
    case BXP_FX: *n = 1; return P_FX[fx_at(a, u->fxpos)];
    case BXP_EDIT:
        if (u->edit == BXE_PIT) { *n = 2; return P_PIT[u->esub[BXE_PIT] % 2]; }
        if (u->edit == BXE_GLO) { *n = 1; return P_EGLO[0]; }
        *n = 6; return P_OP[u->esub[u->edit % BXE_N] % 6];
    default: *n = 1; return P_MAIN[0];
    }
}

static int sub_of(const bm_app *a)
{
    const bm_bx7ui *u = &a->bx;
    if (u->page == BXP_EDIT) return u->esub[u->edit % BXE_N];
    return u->sub[u->page % BXP_N];
}

/* girar una perilla de la página */
static void set(bm_app *a, int k, const bxp *p, int d)
{
    int v = mach_turn(a, p, d);
    if (v < 0) return;
    char b[12], ti[22];
    mach_fmt(p, v, b, sizeof b);
    if (a->bx.page == BXP_EDIT && a->bx.edit <= BXE_OP6) snprintf(ti, sizeof ti, "OP%d %s", a->bx.edit + 1, T(p->name));
    else if (a->bx.page == BXP_FX) snprintf(ti, sizeof ti, "%s %s", T(bx7_fx_names[fx_at(a, a->bx.fxpos)]), T(p->name));
    else snprintf(ti, sizeof ti, "BX-7 %s", T(p->name));
    app_overlay(a, k, ti, b, "", mach_pct(p, v), -1, bm_knob_color(k));
}

/* ------------------------------------------------------------ entrar, botones, teclas, perillas */
bool bx7_button(bm_app *a, int id, bm_gkind k)
{
    switch (id) {
    case BTN_SCL:
        if ((k == G_TAP || k == G_HOLD) && a->bx.page == BXP_FX) {
            int f = fx_at(a, a->bx.fxpos);
            if (!a->bx.grab && (f == BXF_REVERB || f == BXF_DELAY)) { app_toast(a, "SEND: ALWAYS AFTER THE FX"); return true; }
            a->bx.grab = (uint8_t)!a->bx.grab;
            app_toast(a, a->bx.grab ? "SELECT MOVES IT" : "PLACED");
        }
        return true;
    default: break;
    }
    int p = bx7_page_of(id);
    if (p < 0) return true;                                   /* lo demás no hace nada en la máquina */
    if (k != G_TAP && k != G_HOLD) return true;
    bm_bx7ui *u = &a->bx;
    int n;
    if (u->page != p) {                                       /* otro menú: entra en su página 1 */
        u->page = (uint8_t)p;
        if (p == BXP_EDIT) u->esub[u->edit % BXE_N] = 0; else u->sub[p] = 0;
    } else if (cur(a, &n), n > 1) {                           /* con páginas: la siguiente, en círculo (no sale) */
        if (p == BXP_EDIT) u->esub[u->edit % BXE_N] = (uint8_t)((u->esub[u->edit % BXE_N] + 1) % n);
        else u->sub[p] = (uint8_t)((u->sub[p] + 1) % n);
    } else u->page = BXP_MAIN;                                /* de una página: otra vez, MAIN */
    a->bx.grab = 0;
    a->ov.on = 0;
    return true;
}

void bx7_key(bm_app *a, int k, int w, int b)
{
    int t = a->track;
    if (a->bx.page == BXP_EDIT && b >= 0) {                   /* EDIT: las negras eligen qué editar */
        if (b <= BXE_GLO) { a->bx.edit = (uint8_t)b; a->ov.on = 0; return; }
        if (b == 8 || b == 9) {
            app_push(a, CMD_SET_PRM, (uint8_t)t, BX_MONO, (uint8_t)(b == 8));
            app_edited(a);
            app_toast(a, b == 8 ? "MONO" : "POLY");
        }
        return;
    }
    (void)w;
    mach_play(a, k, 105);
}

void bx7_encoder(bm_app *a, int id, int d)
{
    int t = a->track, n;
    bm_bx7ui *u = &a->bx;
    char b[16];
    switch (id) {
    case ENC_SELECT:
        if (u->page == BXP_FX) {
            int np = (u->fxpos + app_lst(a, d) + BXF_N * 4) % BXF_N;
            if (u->grab) {                                    /* SCL tomada: la casilla se mueve en la cadena */
                uint8_t x = a->pat.prm[t][BX_ORD0 + u->fxpos], y = a->pat.prm[t][BX_ORD0 + np];
                app_push(a, CMD_SET_PRM, (uint8_t)t, (uint8_t)(BX_ORD0 + u->fxpos), y);
                app_push(a, CMD_SET_PRM, (uint8_t)t, (uint8_t)(BX_ORD0 + np), x);
                app_edited(a);
            }
            u->fxpos = (uint8_t)np;
            a->ov.on = 0;
            return;
        }
        return;                                               /* las páginas, con su botón */
    case ENC_ALGO:
        if (bm_gesture_is_down(&a->ges, BTN_OCT_DN) || bm_gesture_is_down(&a->ges, BTN_OCT_UP)) {   /* OCT + ALGORITHM: transposición */
            a->oct_used = 1;
            int v = bm_clampi(a->pat.trk[t].transpose + app_lst(a, d), 12, 36);
            app_set_track(a, t, TRK(transpose), v);
            snprintf(b, sizeof b, v > 24 ? "+%d ST" : "%d ST", v - 24);
            app_overlay(a, -1, "TRANSPOSE", b, "", (v - 12) * 255 / 24, 128, C_ORANGE);
            return;
        }
        {
            int v = (a->pat.fmv[BM_FM_ALG] % 32 + app_lst(a, d) + 64) % 32;
            app_push(a, CMD_FMV, BM_FM_ALG, (uint8_t)v, 0);
            app_edited(a);
            snprintf(b, sizeof b, "%d", v + 1);
            if (u->page == BXP_MAIN || (u->page == BXP_EDIT && u->edit != BXE_PIT))
                a->ov.on = 0;                                /* el dibujo está a la vista: se ve cambiar */
            else
                app_overlay(a, -1, "ALGORITHM", b, "", v * 255 / 31, -1, C_ORANGE);
        }
        return;
    case ENC_PRESETS: app_enc_source(a, t, d); return;
    default: break;
    }
    int k = id - ENC_K1;
    if (k < 0 || k > 3) return;
    const bxp *P = cur(a, &n);
    set(a, k, &P[k], d);
}

void bx7_legend(const bm_app *a, bm_legend L[4])
{
    int n;
    mach_legend(a, cur(a, &n), L);
}

void bx7_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    int n;
    cur(a, &n);
    *sel = a->bx.page == BXP_FX ? "SLOT" : a->bx.page == BXP_SEQ ? "STEP" : "-";
    *alg = "ALG";
    *pre = "PRESET";
}

void bx7_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    app_note_keys(a, K, a->track);
    if (a->bx.page != BXP_EDIT) return;
    static const char *const EK[10] = {"OP1", "OP2", "OP3", "OP4", "OP5", "OP6", "PIT", "GLO", "MONO", "POLY"};
    int mono = a->pat.prm[a->track][BX_MONO] != 0;
    for (int i = 0; i < BM_BLACKS; i++) {
        bm_keyfn *f = &K[bm_black_key(i)];
        memset(f, 0, sizeof *f);
        if (i >= 10) continue;
        snprintf(f->label, sizeof f->label, "%s", EK[i]);
        f->led = i < 8 ? (i == a->bx.edit ? LED_ON : LED_DIM) : ((i == 8) == mono ? LED_ON : LED_DIM);
        f->color = C_ORANGE;
    }
}

uint8_t bx7_btn_led(const bm_app *a, int i)
{
    if (i == BTN_SCL) return a->bx.page != BXP_FX ? LED_OFF : a->bx.grab ? LED_BREATH : LED_DIM;
    int p = bx7_page_of(i);
    if (p < 0) return LED_OFF;
    return p == a->bx.page ? LED_ON : LED_DIM;
}

const char *bx7_title(const bm_app *a, char *buf, int cap)
{
    int n;
    cur(a, &n);
    const bm_bx7ui *u = &a->bx;
    if (u->page == BXP_EDIT) {
        if (u->edit <= BXE_OP6) snprintf(buf, (size_t)cap, "OP%d %s", u->edit + 1, T(OP_PAGES[u->esub[u->edit] % 6]));
        else snprintf(buf, (size_t)cap, "%s", u->edit == BXE_PIT ? (u->esub[BXE_PIT] % 2 ? "PITCH LEVELS" : "PITCH RATES") : "VOICE");
    } else if (u->page == BXP_FX) snprintf(buf, (size_t)cap, "FX %s", T(bx7_fx_names[fx_at(a, u->fxpos)]));
    else if (n > 1) snprintf(buf, (size_t)cap, "%s %d/%d", T(PAGE_NAMES[u->page % BXP_N]), sub_of(a) % n + 1, n);
    else snprintf(buf, (size_t)cap, "%s", T(PAGE_NAMES[u->page % BXP_N]));
    (void)page_btn;
    return buf;
}
