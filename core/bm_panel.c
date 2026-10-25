/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — las máquinas de panel por dentro: B-MINI (Minimoog, con los menús del POLY D de
 * Behringer), BUNO-6 (Juno-6) y BITTAL (Vital), 2026-10-22. Cada sección del panel en su botón:
 *   EDIT       el panel, una sección por página (con pestañas arriba)
 *   ENV        sus envolventes (una página cada una)
 *   LFO        su modulación (BITTAL: y los destinos de sus 4 macros)
 *   FX         DISTORTION (DRIVE, TONE, LEVEL); pág 2 CHORUS y REVERB (el DELAY, desde HOME)
 *   SCL        el ACCENT de cada paso; SCL otra vez: el GLIDE de cada paso (como la 303)
 *   SEQ, ARP y GLO como en las demás máquinas
 *   ALGORITHM  el corte del filtro, en cualquier página · PRESETS los sonidos
 * Un menú con páginas no se sale: su botón otra vez pasa a la siguiente, en círculo; se sale con otro
 * menú o HOME. El de una página (y SEQ), otra vez, vuelve al panel. */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_params.h"
#include "bm_lang.h"

#define VOLUME {"VOLUME", K_TRK, TRK(level), 0, 127, F_PCT, NULL}
static const bxp P_NONE[4] = {NONE, NONE, NONE, NONE};

/* ------------------------------------------------------------ B-MINI: el Minimoog, como el POLY D */
static const char *const MN_RANGES[5] = {"32'", "16'", "8'", "4'", "2'"};
const char *const bmini_r3_names[6] = {"LO", "32'", "16'", "8'", "4'", "2'"};
static const char *const MN_WAVES_N[MN_WAVES] = {"TRI", "SHARK", "SAW", "SQUARE", "WIDE", "NARROW"};
static const char *const MN_MODES[2] = {"MONO", "LEGATO"};
static const char *const MN_LWAVES[2] = {"TRI", "SQUARE"};
static const char *const MN_DESTS[MN_LDESTS] = {"OSC", "FILTER", "BOTH"};
static const bxp MN_EDIT[5][4] = {
    {{"CUTOFF", K_MAC, 0, 0, 127, F_PHZ, NULL}, {"EMPHASIS", K_MAC, 1, 0, 127, F_PCT, NULL},
     {"CONTOUR", K_MAC, 2, 0, 127, F_PCT, NULL}, {"KBD CTRL", K_PRM, MN_KEYT, 0, 127, F_PCT, NULL}},
    {{"RANGE", K_PRM, MN_R1, 0, 4, F_LIST, MN_RANGES}, {"WAVE", K_PRM, MN_W1, 0, MN_WAVES - 1, F_LIST, MN_WAVES_N},
     {"LEVEL", K_PRM, MN_L1, 0, 127, F_PCT, NULL}, {"TUNE", K_PRM, MN_TUNE, 0, 127, F_BIP, NULL}},
    {{"RANGE", K_PRM, MN_R2, 0, 4, F_LIST, MN_RANGES}, {"FREQ", K_PRM, MN_T2, 0, 127, F_BIP, NULL},
     {"WAVE", K_PRM, MN_W2, 0, MN_WAVES - 1, F_LIST, MN_WAVES_N}, {"LEVEL", K_PRM, MN_L2, 0, 127, F_PCT, NULL}},
    {{"RANGE", K_R3, MN_R3, 0, 5, F_LIST, bmini_r3_names}, {"FREQ", K_PRM, MN_T3, 0, 127, F_BIP, NULL},
     {"WAVE", K_PRM, MN_W3, 0, MN_WAVES - 1, F_LIST, MN_WAVES_N}, {"LEVEL", K_PRM, MN_L3, 0, 127, F_PCT, NULL}},
    {{"NOISE", K_PRM, MN_NOISE, 0, 127, F_PCT, NULL}, {"GLIDE", K_PRM, MN_GLIDE, 0, 127, F_TIME, NULL},
     {"MODE", K_PRM, MN_MODE, 0, 1, F_LIST, MN_MODES}, VOLUME}};
static const char *const MN_EDIT_N[5] = {"FILTER", "OSC 1", "OSC 2", "OSC 3", "OUTPUT"};
static const bxp MN_ENV[2][4] = {
    {{"ATTACK", K_PRM, MN_FA, 0, 127, F_TIME, NULL}, {"DECAY", K_PRM, MN_FD, 0, 127, F_TIME, NULL},
     {"SUSTAIN", K_PRM, MN_FS, 0, 127, F_PCT, NULL}, {"RELEASE", K_PRM, MN_FR, 0, 127, F_TIME, NULL}},
    {{"ATTACK", K_PRM, MN_AA, 0, 127, F_TIME, NULL}, {"DECAY", K_PRM, MN_AD, 0, 127, F_TIME, NULL},
     {"SUSTAIN", K_PRM, MN_AS, 0, 127, F_PCT, NULL}, {"RELEASE", K_PRM, MN_AR, 0, 127, F_TIME, NULL}}};
static const char *const MN_ENV_N[2] = {"FILTER", "LOUDNESS"};
static const bxp MN_LFO[2][4] = {
    {{"RATE", K_PRM, MN_LRATE, 0, 127, F_RATE, NULL}, {"WAVE", K_PRM, MN_LWAVE, 0, 1, F_LIST, MN_LWAVES},
     {"MOD MIX", K_PRMI, MN_MIX, 0, 127, F_MIX, NULL}, {"AMOUNT", K_PRM, MN_LAMT, 0, 127, F_PCT, NULL}},
    {{"DEST", K_PRM, MN_LDEST, 0, MN_LDESTS - 1, F_LIST, MN_DESTS}, NONE, NONE, NONE}};
static const char *const MN_LFO_N[2] = {"", "DEST"};

/* ------------------------------------------------------------ BUNO-6: el Juno-6 */
static const char *const BN_RANGES[3] = {"16'", "8'", "4'"};
static const char *const BN_PWM_N[BN_PWMS] = {"LFO", "MANUAL", "ENV"};
static const char *const BN_HPFS[4] = {"0", "1", "2", "3"};
static const char *const BN_POLS[2] = {"+", "-"};
static const char *const BN_VCAS[2] = {"ENV", "GATE"};
static const bxp BN_EDIT[5][4] = {
    {{"FREQ", K_MAC, 0, 0, 127, F_PHZ, NULL}, {"RES", K_MAC, 1, 0, 127, F_PCT, NULL},
     {"ENV", K_MAC, 2, 0, 127, F_PCT, NULL}, {"LFO", K_MAC, 3, 0, 127, F_PCT, NULL}},
    {{"RANGE", K_PRM, BN_RANGE, 0, 2, F_LIST, BN_RANGES}, {"PULSE", K_PRM, BN_PULSE, 0, 1, F_ONOFF, NULL},
     {"SAW", K_PRM, BN_SAW, 0, 1, F_ONOFF, NULL}, {"SUB", K_PRM, BN_SUB, 0, 127, F_PCT, NULL}},
    {{"LFO", K_PRM, BN_LFODCO, 0, 127, F_PCT, NULL}, {"PWM", K_PRM, BN_PWM, 0, 127, F_PCT, NULL},
     {"PWM MODE", K_PRM, BN_PWMSRC, 0, BN_PWMS - 1, F_LIST, BN_PWM_N}, {"NOISE", K_PRM, BN_NOISE, 0, 127, F_PCT, NULL}},
    {{"HPF", K_PRM, BN_HPF, 0, 3, F_LIST, BN_HPFS}, {"ENV +/-", K_PRM, BN_ENVPOL, 0, 1, F_LIST, BN_POLS},
     {"KYBD", K_PRM, BN_KEYT, 0, 127, F_PCT, NULL}, {"VELO", K_PRM, BN_VELF, 0, 127, F_PCT, NULL}},
    {{"ENV/GATE", K_PRM, BN_VCA, 0, 1, F_LIST, BN_VCAS}, {"VELO", K_PRM, BN_VELA, 0, 127, F_PCT, NULL}, NONE, VOLUME}};
static const char *const BN_EDIT_N[5] = {"VCF", "DCO", "DCO MOD", "VCF MOD", "VCA"};
static const bxp BN_ENV[1][4] = {
    {{"ATTACK", K_PRM, BN_A, 0, 127, F_TIME, NULL}, {"DECAY", K_PRM, BN_D, 0, 127, F_TIME, NULL},
     {"SUSTAIN", K_PRM, BN_S, 0, 127, F_PCT, NULL}, {"RELEASE", K_PRM, BN_R, 0, 127, F_TIME, NULL}}};
static const char *const BN_ENV_N[1] = {""};
static const bxp BN_LFO[1][4] = {
    {{"RATE", K_PRM, BN_LRATE, 0, 127, F_RATE, NULL}, {"DELAY", K_PRM, BN_LDELAY, 0, 127, F_TIME, NULL}, NONE, NONE}};
static const char *const BN_LFO_N[1] = {""};

/* ------------------------------------------------------------ BITTAL: Vital */
static const char *const BT_UNIS[4] = {"1", "2", "3", "4"};
static const char *const BT_FILTERS_N[BT_FILTERS] = {"LP 24", "LP 12", "BP", "HP", "LADDER", "OFF"};
static const char *const BT_MODES[WM_MODES] = {"POLY", "MONO", "LEGATO"};
static const char *const BT_LSHAPES_N[BT_LSHAPES] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
static const char *const BT_LDESTS_N[BT_LDESTS] = {"PITCH", "CUTOFF", "POSITION", "LEVEL"};
#define BT_OSC(L, S, P, U) {{"LEVEL", K_PRM, L, 0, 127, F_PCT, NULL}, {"TRANSP", K_PRM, S, 0, 127, F_SEMI, NULL}, \
                            {"FRAME", K_PRM, P, 0, 127, F_PCT, NULL}, {"UNISON", K_PRM, U, 0, 3, F_LIST, BT_UNIS}}
#define BT_OSCP(D, F, T) {{"DETUNE", K_PRM, D, 0, 127, F_PCT, NULL}, {"FINE", K_PRM, F, 0, 127, F_CENT, NULL}, \
                          {"TABLE", K_PRM, T, 0, 63, F_INT, NULL}, NONE}
static const bxp BT_EDIT[7][4] = {
    BT_OSC(BT_L1, BT_S1, BT_P1, BT_U1), BT_OSCP(BT_D1, BT_F1, BT_TAB1),
    BT_OSC(BT_L2, BT_S2, BT_P2, BT_U2), BT_OSCP(BT_D2, BT_F2, BT_TAB2),
    {{"MODEL", K_PRM, BT_FT, 0, BT_FILTERS - 1, F_LIST, BT_FILTERS_N}, {"CUTOFF", K_PRM, BT_CUT, 0, 127, F_PHZ, NULL},
     {"RESO", K_PRM, BT_RES, 0, 127, F_PCT, NULL}, {"DRIVE", K_PRM, BT_DRIVE, 0, 127, F_PCT, NULL}},
    {{"ENV2 AMT", K_PRM, BT_FENV, 0, 127, F_BIP, NULL}, {"KEYTRK", K_PRM, BT_KEYT, 0, 127, F_PCT, NULL},
     {"VELO", K_PRM, BT_VELF, 0, 127, F_PCT, NULL}, {"ENV2 POS", K_PRM, BT_EPOS, 0, 127, F_PCT, NULL}},
    {{"MODE", K_PRM, BT_MODE, 0, WM_MODES - 1, F_LIST, BT_MODES}, {"GLIDE", K_PRM, BT_GLIDE, 0, 127, F_TIME, NULL},
     {"VELO", K_PRM, BT_VELA, 0, 127, F_PCT, NULL}, VOLUME}};
static const char *const BT_EDIT_N[7] = {"OSC 1", "OSC 1 +", "OSC 2", "OSC 2 +", "FILTER", "FILTER +", "VOICE"};
static const bxp BT_ENV[2][4] = {
    {{"ATTACK", K_PRM, BT_AA, 0, 127, F_TIME, NULL}, {"DECAY", K_PRM, BT_AD, 0, 127, F_TIME, NULL},
     {"SUSTAIN", K_PRM, BT_AS, 0, 127, F_PCT, NULL}, {"RELEASE", K_PRM, BT_AR, 0, 127, F_TIME, NULL}},
    {{"ATTACK", K_PRM, BT_MA, 0, 127, F_TIME, NULL}, {"DECAY", K_PRM, BT_MD, 0, 127, F_TIME, NULL},
     {"SUSTAIN", K_PRM, BT_MS, 0, 127, F_PCT, NULL}, {"RELEASE", K_PRM, BT_MR, 0, 127, F_TIME, NULL}}};
static const char *const BT_ENV_N[2] = {"ENV 1", "ENV 2"};
#define BT_MAC(n1, d1, a1, n2, d2, a2) {{n1 " TO", K_PRM, d1, 0, BTM_TARGETS - 1, F_LIST, bm_btm_names}, \
                                        {n1 " AMT", K_PRM, a1, 0, 127, F_BIP, NULL}, \
                                        {n2 " TO", K_PRM, d2, 0, BTM_TARGETS - 1, F_LIST, bm_btm_names}, \
                                        {n2 " AMT", K_PRM, a2, 0, 127, F_BIP, NULL}}
static const bxp BT_LFO[3][4] = {
    {{"SHAPE", K_PRM, BT_LSHAPE, 0, BT_LSHAPES - 1, F_LIST, BT_LSHAPES_N}, {"RATE", K_PRM, BT_LRATE, 0, 127, F_RATE, NULL},
     {"DEST", K_PRM, BT_LDEST, 0, BT_LDESTS - 1, F_LIST, BT_LDESTS_N}, {"AMOUNT", K_PRM, BT_LAMT, 0, 127, F_PCT, NULL}},
    BT_MAC("M1", BT_M1D, BT_M1A, "M2", BT_M2D, BT_M2A),
    BT_MAC("M3", BT_M3D, BT_M3A, "M4", BT_M4D, BT_M4A)};
static const char *const BT_LFO_N[3] = {"", "MACRO 1-2", "MACRO 3-4"};

/* ------------------------------------------------------------ los efectos, iguales en las tres */
static const bxp PX_FX1[4] = {{"DRIVE", K_PRM, PX_DS_G, 0, 127, F_PCT, NULL}, {"TONE", K_PRM, PX_DS_T, 0, 127, F_PCT, NULL},
                              {"LEVEL", K_PRM, PX_DS_L, 0, 127, F_PCT, NULL}, {"ON", K_ON, PXF_DIST, 0, 1, F_ONOFF, NULL}};
#define PX_FX2(...) {{__VA_ARGS__}, NONE, {"REVERB", K_TRK, TRK(rsend), 0, 127, F_PCT, NULL}, {"REV ON", K_ON, PXF_REVERB, 0, 1, F_ONOFF, NULL}}
static const bxp PX_FX2_STD[4] = PX_FX2("CHORUS", K_PRM, PX_CHORUS, 0, 2, F_LIST, bm_px_chorus_names);
static const bxp PX_FX2_BN[4] = PX_FX2("CHORUS", K_PRM, BN_CHORUS, 0, BN_CHORUSES - 1, F_LIST, bm_bn_chorus_names);   /* el del Juno, con I+II */
const char *const panel_fx_names[PXF_N] = {"DISTORTION", "CHORUS", "REVERB"};

typedef struct {
    const char *name;                              /* para el recuadro: "B-MINI CUTOFF" */
    int edit_n; const bxp (*edit)[4]; const char *const *edit_names;
    int env_n; const bxp (*env)[4]; const char *const *env_names;
    int lfo_n; const bxp (*lfo)[4]; const char *const *lfo_names;
    const bxp *fx2;
    bxp algo;                                      /* ALGORITHM: el corte */
} pmach;

static const pmach PM[3] = {
    {"B-MINI", 5, MN_EDIT, MN_EDIT_N, 2, MN_ENV, MN_ENV_N, 2, MN_LFO, MN_LFO_N, PX_FX2_STD,
     {"CUTOFF", K_MAC, 0, 0, 127, F_PHZ, NULL}},
    {"BUNO-6", 5, BN_EDIT, BN_EDIT_N, 1, BN_ENV, BN_ENV_N, 1, BN_LFO, BN_LFO_N, PX_FX2_BN,
     {"FREQ", K_MAC, 0, 0, 127, F_PHZ, NULL}},
    {"BITTAL", 7, BT_EDIT, BT_EDIT_N, 2, BT_ENV, BT_ENV_N, 3, BT_LFO, BT_LFO_N, PX_FX2_STD,
     {"CUTOFF", K_PRM, BT_CUT, 0, 127, F_PHZ, NULL}}};

static const pmach *pm_of(const bm_app *a)
{
    int e = a->pat.trk[a->track].engine;
    return &PM[e == ENG_BUNO ? 1 : e == ENG_BITTAL ? 2 : 0];
}

int panel_page_of(int btn)
{
    switch (btn) {
    case BTN_EDIT: return BXP_MAIN;
    case BTN_SEQ: return BXP_SEQ;
    case BTN_FX: return BXP_FX;
    case BTN_ENV: return BXP_ENV;
    case BTN_LFO: return BXP_LFO;
    case BTN_SCL: return BXP_SCL;
    case BTN_ARP: return BXP_ARP;
    case BTN_GLO: return BXP_GLO;
    default: return -1;
    }
}

static int pages(const pmach *m, int p)
{
    switch (p) {
    case BXP_MAIN: return m->edit_n;
    case BXP_ENV: return m->env_n;
    case BXP_LFO: return m->lfo_n;
    case BXP_SEQ: return 1;
    default: return 2;
    }
}

int panel_sub(const bm_app *a) { return a->bx.sub[a->bx.page % BXP_N] % pages(pm_of(a), a->bx.page); }

static const bxp *cur(const bm_app *a, int *n)
{
    const pmach *m = pm_of(a);
    *n = pages(m, a->bx.page);
    int s = panel_sub(a);
    switch (a->bx.page) {
    case BXP_SEQ: return MACH_SEQ;
    case BXP_ENV: return m->env[s];
    case BXP_LFO: return m->lfo[s];
    case BXP_FX: return s ? m->fx2 : PX_FX1;
    case BXP_SCL: return P_NONE;
    case BXP_GLO: return MACH_GLO[s];
    case BXP_ARP: return MACH_ARP[s];
    default: return m->edit[s];
    }
}

const bxp *panel_knobs(const bm_app *a) { int n; return cur(a, &n); }

/* el panel está a la vista: se ve girar la perilla, sin el recuadro grande encima (ALGORITHM se ve
 * solo si está en la página, como el CUTOFF del B-MINI en FILTER) */
static bool on_panel(const bm_app *a, int k, const bxp *p)
{
    if (a->bx.page != BXP_MAIN) return false;
    if (k >= 0) return true;
    const bxp *P = pm_of(a)->edit[panel_sub(a)];
    for (int i = 0; i < 4; i++) if (P[i].kind == p->kind && P[i].idx == p->idx) return true;
    return false;
}

static void turned(bm_app *a, int k, const bxp *p, int v)
{
    if (v < 0) return;
    if (on_panel(a, k, p)) { a->ov.on = 0; return; }
    char b[12], ti[22];
    mach_fmt(p, v, b, sizeof b);
    const char *pre = a->bx.page == BXP_FX ? "FX" : a->bx.page == BXP_LFO ? "LFO" : a->bx.page == BXP_ENV ? "ENV" : pm_of(a)->name;
    snprintf(ti, sizeof ti, "%s %s", pre, T(p->name));
    app_overlay(a, k, ti, b, "", mach_pct(p, v), p->fmt == F_BIP || p->fmt == F_SEMI || p->fmt == F_CENT ? 128 : -1,
                k >= 0 ? bm_knob_color(k) : C_ORANGE);
}

bool panel_button(bm_app *a, int id, bm_gkind k)
{
    int p = panel_page_of(id);
    if (p < 0) return true;                                   /* lo demás no hace nada */
    if (k != G_TAP && k != G_HOLD) return true;
    bm_bx7ui *u = &a->bx;
    int n = pages(pm_of(a), p);
    if (u->page != p) {                                       /* otro menú: entra en su página 1 */
        u->page = (uint8_t)p;
        u->sub[p] = 0;
        if (p == BXP_SCL) u->blk = (uint8_t)(u->step % mach_len(a) / 16);
    } else if (n > 1) u->sub[p] = (uint8_t)((u->sub[p] + 1) % n);   /* la siguiente, en círculo */
    else u->page = BXP_MAIN;                                  /* de una página: otra vez, el panel */
    a->ov.on = 0;
    return true;
}

void panel_key(bm_app *a, int k, int w, int b)
{
    (void)b;
    if (a->bx.page == BXP_SCL) { if (w >= 0 && w < 16) mach_scl_toggle(a, w); return; }   /* las negras: nada */
    mach_play(a, k, 105);
}

void panel_encoder(bm_app *a, int id, int d)
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
    case ENC_ALGO: {                                          /* el corte, en cualquier página */
        const bxp *p = &pm_of(a)->algo;
        turned(a, -1, p, mach_turn(a, p, d));
        return;
    }
    case ENC_PRESETS: app_enc_source(a, a->track, d); return;
    default: break;
    }
    int k = id - ENC_K1;
    if (k < 0 || k > 3) return;
    const bxp *P = cur(a, &n);
    turned(a, k, &P[k], mach_turn(a, &P[k], d));
}

void panel_legend(const bm_app *a, bm_legend L[4])
{
    int n;
    mach_legend(a, cur(a, &n), L);
}

void panel_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    *sel = a->bx.page == BXP_SCL ? "STEPS" : a->bx.page == BXP_SEQ ? "STEP" : "-";
    *alg = pm_of(a)->algo.name;
    *pre = "PRESET";
}

uint8_t panel_btn_led(const bm_app *a, int i)
{
    int p = panel_page_of(i);
    if (p < 0) return LED_OFF;
    return p == a->bx.page ? LED_ON : LED_DIM;
}

const char *panel_title(const bm_app *a, char *buf, int cap)
{
    const pmach *m = pm_of(a);
    const bm_bx7ui *u = &a->bx;
    int n = pages(m, u->page), s = panel_sub(a);
    switch (u->page) {
    case BXP_MAIN: snprintf(buf, (size_t)cap, "%s %d/%d", T(m->edit_names[s]), s + 1, n); break;
    case BXP_ENV:
        if (n > 1) snprintf(buf, (size_t)cap, "ENV %d/%d %s", s + 1, n, T(m->env_names[s]));
        else snprintf(buf, (size_t)cap, "%s", "ENV");
        break;
    case BXP_LFO:
        if (n > 1) snprintf(buf, (size_t)cap, m->lfo_names[s][0] ? "LFO %d/%d %s" : "LFO %d/%d", s + 1, n, T(m->lfo_names[s]));
        else snprintf(buf, (size_t)cap, "%s", "LFO");
        break;
    case BXP_FX: snprintf(buf, (size_t)cap, "FX %d/2 %s", s + 1, T(s ? "CHORUS REVERB" : "DISTORTION")); break;
    case BXP_SCL: snprintf(buf, (size_t)cap, "%s %d-%d", T(s ? "GLIDE" : "ACCENT"), u->blk * 16 + 1, u->blk * 16 + 16); break;
    case BXP_GLO: snprintf(buf, (size_t)cap, "GLO %d/2", s + 1); break;
    case BXP_ARP: snprintf(buf, (size_t)cap, "ARP %d/2", s + 1); break;
    default: snprintf(buf, (size_t)cap, "%s", "SEQ"); break;
    }
    return buf;
}

/* el panel: cuántas secciones tiene EDIT y cómo se llaman */
int panel_edit_n(const bm_app *a) { return pm_of(a)->edit_n; }
const char *panel_edit_name(const bm_app *a, int i) { return pm_of(a)->edit_names[i % pm_of(a)->edit_n]; }
