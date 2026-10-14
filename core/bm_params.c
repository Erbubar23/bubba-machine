/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — parámetros de sonido, kits y presets de fábrica (core/bm_params.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bm_params.h"
#include "bm_drums.h"
#include "bm_snap.h"

/* ------------------------------------------------------------ valores físicos */
float bm_cut_hz(int v) { return 20.0f * exp2f((float)v * (10.0f / 127.0f)); }
float bm_env_s(int v) { return 0.001f * powf(10.0f, (float)v * (4.0f / 127.0f)); }
float bm_lfo_hz(int v) { return 0.05f * exp2f((float)v * (9.0f / 127.0f)); }
float bm_reso_q(int v) { float x = (float)v / 127.0f; return 0.5f + 24.5f * x * x * x; }

/* ------------------------------------------------------------ las máquinas del núcleo WAVE */
#define P(n, s, d, m, f, l) {n, s, d, m, f, l}
static const char *const MN_WAVES_N[MN_WAVES] = {"TRI", "SHARK", "SAW", "SQUARE", "WIDE", "NARROW"};
static const char *const RANGES[5] = {"32'", "16'", "8'", "4'", "2'"};
static const char *const MN_LDESTS_N[MN_LDESTS] = {"PITCH", "CUTOFF"};
static const char *const WMODES[WM_MODES] = {"POLY", "MONO", "LEGATO"};
static const char *const MONO2[2] = {"MONO", "LEGATO"};

const bm_ppage bm_mn_pages[BM_MN_PAGES] = {
    {"OSC 1", 3, {P("WAVE", MN_W1, MN_WAVE_SAW, MN_WAVES - 1, PF_LIST, MN_WAVES_N), P("RANGE", MN_R1, 1, 4, PF_LIST, RANGES),
                  P("LEVEL", MN_L1, 110, 127, PF_PCT, 0)}},
    {"OSC 2", 4, {P("WAVE", MN_W2, MN_WAVE_SAW, MN_WAVES - 1, PF_LIST, MN_WAVES_N), P("RANGE", MN_R2, 1, 4, PF_LIST, RANGES),
                  P("TUNE", MN_T2, 66, 127, PF_BIP, 0), P("LEVEL", MN_L2, 90, 127, PF_PCT, 0)}},
    {"OSC 3", 4, {P("WAVE", MN_W3, MN_WAVE_SQR, MN_WAVES - 1, PF_LIST, MN_WAVES_N), P("RANGE", MN_R3, 0, 4, PF_LIST, RANGES),
                  P("TUNE", MN_T3, 64, 127, PF_BIP, 0), P("LEVEL", MN_L3, 50, 127, PF_PCT, 0)}},
    {"FILTER", 4, {P("CUTOFF", PS_M0 + 0, 60, 127, PF_HZ, 0), P("EMPHASIS", PS_M0 + 1, 50, 127, PF_PCT, 0),
                   P("OSC TUNE", PS_M0 + 2, 64, 127, PF_BIP, 0), P("CONTOUR", PS_M0 + 3, 70, 127, PF_PCT, 0)}},
    {"FILTER +", 4, {P("DRIVE", MN_DRIVE, 30, 127, PF_PCT, 0), P("KEYTRACK", MN_KEYT, 40, 127, PF_PCT, 0),
                     P("VELOCITY", MN_VELF, 40, 127, PF_PCT, 0), P("NOISE", MN_NOISE, 0, 127, PF_PCT, 0)}},
    {"CONTOUR", 4, {P("ATTACK", MN_FA, 0, 127, PF_TIME, 0), P("DECAY", MN_FD, 55, 127, PF_TIME, 0),
                    P("SUSTAIN", MN_FS, 20, 127, PF_PCT, 0), P("RELEASE", MN_FR, 45, 127, PF_TIME, 0)}},
    {"LOUDNESS", 4, {P("ATTACK", MN_AA, 0, 127, PF_TIME, 0), P("DECAY", MN_AD, 70, 127, PF_TIME, 0),
                     P("SUSTAIN", MN_AS, 110, 127, PF_PCT, 0), P("RELEASE", MN_AR, 35, 127, PF_TIME, 0)}},
    {"MOD", 4, {P("LFO RATE", MN_LRATE, 70, 127, PF_RATE, 0), P("AMOUNT", MN_LAMT, 0, 127, PF_PCT, 0),
                P("TARGET", MN_LDEST, MN_D_CUT, MN_LDESTS - 1, PF_LIST, MN_LDESTS_N), P("GLIDE", MN_GLIDE, 35, 127, PF_TIME, 0)}},
    {"VOICE", 2, {P("MODE", MN_MODE, 0, 1, PF_LIST, MONO2), P("VEL>LEVEL", MN_VELA, 40, 127, PF_PCT, 0)}},
};

static const char *const PWMS[BN_PWMS] = {"LFO", "MANUAL", "ENV"};
static const char *const BN_RANGES[3] = {"16'", "8'", "4'"};
static const char *const HPFS[4] = {"0", "1", "2", "3"};
static const char *const POLS[2] = {"+", "-"};
static const char *const VCAS[2] = {"ENV", "GATE"};
const char *const bm_bn_chorus_names[BN_CHORUSES] = {"OFF", "I", "II", "I+II"};

const bm_ppage bm_bn_pages[BM_BN_PAGES] = {
    {"DCO", 4, {P("SAW", BN_SAW, 1, 1, PF_ONOFF, 0), P("PULSE", BN_PULSE, 0, 1, PF_ONOFF, 0),
                P("SUB", BN_SUB, 40, 127, PF_PCT, 0), P("NOISE", BN_NOISE, 0, 127, PF_PCT, 0)}},
    {"DCO +", 4, {P("PWM", PS_M0 + 3, 50, 127, PF_PCT, 0), P("PWM BY", BN_PWMSRC, BN_PWM_LFO, BN_PWMS - 1, PF_LIST, PWMS),
                  P("RANGE", BN_RANGE, 1, 2, PF_LIST, BN_RANGES), P("LFO>DCO", BN_LFODCO, 0, 127, PF_PCT, 0)}},
    {"VCF", 4, {P("FREQ", PS_M0 + 0, 72, 127, PF_HZ, 0), P("RES", PS_M0 + 1, 30, 127, PF_PCT, 0),
                P("ENV", PS_M0 + 2, 50, 127, PF_PCT, 0), P("HPF", BN_HPF, 1, 3, PF_LIST, HPFS)}},
    {"VCF +", 4, {P("ENV POL", BN_ENVPOL, 0, 1, PF_LIST, POLS), P("LFO>VCF", BN_LFOVCF, 0, 127, PF_PCT, 0),
                  P("KEYTRACK", BN_KEYT, 60, 127, PF_PCT, 0), P("VELOCITY", BN_VELF, 20, 127, PF_PCT, 0)}},
    {"ENV", 4, {P("ATTACK", BN_A, 10, 127, PF_TIME, 0), P("DECAY", BN_D, 70, 127, PF_TIME, 0),
                P("SUSTAIN", BN_S, 90, 127, PF_PCT, 0), P("RELEASE", BN_R, 60, 127, PF_TIME, 0)}},
    {"LFO VCA", 4, {P("LFO RATE", BN_LRATE, 60, 127, PF_RATE, 0), P("LFO DELAY", BN_LDELAY, 0, 127, PF_TIME, 0),
                    P("VCA", BN_VCA, 0, 1, PF_LIST, VCAS), P("CHORUS", BN_CHORUS, BN_CH_I, BN_CHORUSES - 1, PF_LIST, bm_bn_chorus_names)}},
};

static const char *const BT_FILTERS_N[BT_FILTERS] = {"LP 24", "LP 12", "BP", "HP", "LADDER", "OFF"};
static const char *const BT_LSHAPES_N[BT_LSHAPES] = {"SINE", "TRI", "SAW", "SQUARE", "RANDOM"};
static const char *const BT_LDESTS_N[BT_LDESTS] = {"PITCH", "CUTOFF", "POSITION", "LEVEL"};
static const char *const UNIS[4] = {"1", "2", "3", "4"};
const char *const bm_btm_names[BTM_TARGETS] = {"---", "CUTOFF", "RESO", "POS 1", "POS 2", "POSITION", "LEVEL 2", "DETUNE",
                                               "LFO", "ENV", "DRIVE", "RELEASE", "SPACE"};

const bm_ppage bm_bt_pages[BM_BT_PAGES] = {
    {"OSC 1", 4, {P("LEVEL", BT_L1, 100, 127, PF_PCT, 0), P("TUNE", BT_S1, 64, 127, PF_SEMI, 0),
                  P("POSITION", BT_P1, 0, 127, PF_PCT, 0), P("UNISON", BT_U1, 0, 3, PF_LIST, UNIS)}},
    {"OSC 1 +", 3, {P("DETUNE", BT_D1, 20, 127, PF_PCT, 0), P("FINE", BT_F1, 64, 127, PF_CENT, 0),
                    P("TABLE", BT_TAB1, 0, 63, PF_INT, 0)}},
    {"OSC 2", 4, {P("LEVEL", BT_L2, 0, 127, PF_PCT, 0), P("TUNE", BT_S2, 64, 127, PF_SEMI, 0),
                  P("POSITION", BT_P2, 0, 127, PF_PCT, 0), P("UNISON", BT_U2, 0, 3, PF_LIST, UNIS)}},
    {"OSC 2 +", 3, {P("DETUNE", BT_D2, 20, 127, PF_PCT, 0), P("FINE", BT_F2, 64, 127, PF_CENT, 0),
                    P("TABLE", BT_TAB2, 0, 63, PF_INT, 0)}},
    {"FILTER", 4, {P("TYPE", BT_FT, BT_F_LP24, BT_FILTERS - 1, PF_LIST, BT_FILTERS_N), P("CUTOFF", BT_CUT, 100, 127, PF_HZ, 0),
                   P("RESO", BT_RES, 15, 127, PF_PCT, 0), P("DRIVE", BT_DRIVE, 0, 127, PF_PCT, 0)}},
    {"FILTER +", 3, {P("ENV AMT", BT_FENV, 64, 127, PF_BIP, 0), P("KEYTRACK", BT_KEYT, 64, 127, PF_PCT, 0),
                     P("VELOCITY", BT_VELF, 20, 127, PF_PCT, 0)}},
    {"AMP ENV", 4, {P("ATTACK", BT_AA, 0, 127, PF_TIME, 0), P("DECAY", BT_AD, 70, 127, PF_TIME, 0),
                    P("SUSTAIN", BT_AS, 110, 127, PF_PCT, 0), P("RELEASE", BT_AR, 55, 127, PF_TIME, 0)}},
    {"MOD ENV", 4, {P("ATTACK", BT_MA, 0, 127, PF_TIME, 0), P("DECAY", BT_MD, 65, 127, PF_TIME, 0),
                    P("SUSTAIN", BT_MS, 40, 127, PF_PCT, 0), P("RELEASE", BT_MR, 55, 127, PF_TIME, 0)}},
    {"LFO", 4, {P("SHAPE", BT_LSHAPE, BT_L_TRI, BT_LSHAPES - 1, PF_LIST, BT_LSHAPES_N), P("RATE", BT_LRATE, 60, 127, PF_RATE, 0),
                P("TARGET", BT_LDEST, BT_D_POS, BT_LDESTS - 1, PF_LIST, BT_LDESTS_N), P("AMOUNT", BT_LAMT, 0, 127, PF_PCT, 0)}},
    {"MACRO 1 2", 4, {P("M1 TO", BT_M1D, BTM_CUT, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M1 AMT", BT_M1A, 110, 127, PF_BIP, 0),
                      P("M2 TO", BT_M2D, BTM_POS, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M2 AMT", BT_M2A, 120, 127, PF_BIP, 0)}},
    {"MACRO 3 4", 4, {P("M3 TO", BT_M3D, BTM_LFO, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M3 AMT", BT_M3A, 120, 127, PF_BIP, 0),
                      P("M4 TO", BT_M4D, BTM_SPACE, BTM_TARGETS - 1, PF_LIST, bm_btm_names), P("M4 AMT", BT_M4A, 120, 127, PF_BIP, 0)}},
    {"VOICE", 3, {P("MODE", BT_MODE, WM_POLY, WM_MODES - 1, PF_LIST, WMODES), P("GLIDE", BT_GLIDE, 0, 127, PF_TIME, 0),
                  P("VEL>LEVEL", BT_VELA, 50, 127, PF_PCT, 0)}},
};

/* ------------------------------------------------------------ batería */
static const char *const DRM_LIST[DRM_COUNT] = {"BD 808", "BD 909", "SD 808", "SD 909", "HAT 808", "METAL", "CLAP",
                                                "TOM", "COWBELL", "RESON", "NOISE", "FM"};
static const char *const CHOKES[4] = {"NO", "1", "2", "3"};
const bm_ppage bm_lane_pages[BM_LANE_PAGES] = {
    {"HIT", 4, {P("TUNE", LN_TUNE, 64, 127, PF_DSEMI, 0), P("DECAY", LN_DECAY, 64, 127, PF_PCT, 0),
                  P("TONE", LN_TONE, 64, 127, PF_PCT, 0), P("COLOR", LN_COLOR, 64, 127, PF_PCT, 0)}},
    {"INSTRUMENT", 4, {P("MODEL", LN_MODEL, 0, DRM_COUNT - 1, PF_LIST, DRM_LIST), P("LEVEL", LN_LEVEL, 100, 127, PF_INT, 0),
                        P("CHOKE", LN_CHOKE, 0, 3, PF_LIST, CHOKES), P("LENGTH", LN_LEN, 16, 64, PF_INT, 0)}},
};

/* AFINA: semitonos = (tune - 64) * 3 / 8 sobre el tono base del modelo (bm_drm_base_hz) */
#define L(n, m, tu, de, to, co, le, ch) {n, m, tu, de, to, co, le, ch}
const bm_kit bm_kits[BM_KITS_N] = {
    {"808", {
        L("KICK", DRM_BD_ANALOG, 64, 70, 40, 40, 110, 0), L("KICK2", DRM_BD_ANALOG, 72, 40, 70, 90, 100, 0),
        L("CL HH", DRM_HH_ANALOG, 64, 25, 100, 20, 80, 1), L("OP HH", DRM_HH_ANALOG, 64, 85, 100, 20, 75, 1),
        L("SNARE", DRM_SD_ANALOG, 64, 55, 70, 75, 95, 0), L("CLAP", DRM_CLAP, 64, 50, 60, 50, 95, 0),
        L("RIM", DRM_RESON, 8, 15, 100, 76, 85, 0), L("TOM L", DRM_TOM, 51, 70, 50, 30, 100, 0),
        L("TOM M", DRM_TOM, 68, 68, 50, 30, 100, 0), L("TOM H", DRM_TOM, 81, 66, 50, 30, 100, 0),
        L("COWBL", DRM_COWBELL, 64, 50, 50, 100, 85, 0), L("CYMBL", DRM_HH_METAL, 60, 110, 80, 40, 75, 0),
        L("SHAKE", DRM_NOISE, 64, 25, 100, 50, 80, 0), L("CLAVE", DRM_RESON, 82, 40, 0, 0, 127, 0),
        L("CONGA", DRM_TOM, 106, 45, 40, 10, 95, 0), L("ZAP", DRM_FM, 90, 35, 80, 20, 85, 0)}},
    {"909", {
        L("KICK", DRM_BD_SINTE, 64, 60, 50, 50, 110, 0), L("KICK2", DRM_BD_SINTE, 70, 35, 80, 110, 100, 0),
        L("CL HH", DRM_HH_METAL, 64, 20, 110, 60, 80, 1), L("OP HH", DRM_HH_METAL, 64, 80, 110, 60, 75, 1),
        L("SNARE", DRM_SD_SINTE, 64, 50, 60, 80, 95, 0), L("CLAP", DRM_CLAP, 64, 60, 70, 70, 95, 0),
        L("RIM", DRM_RESON, 12, 12, 110, 70, 85, 0), L("TOM L", DRM_TOM, 56, 62, 80, 50, 100, 0),
        L("TOM M", DRM_TOM, 72, 60, 80, 50, 100, 0), L("TOM H", DRM_TOM, 86, 58, 80, 50, 100, 0),
        L("RIDE", DRM_HH_METAL, 72, 100, 120, 30, 70, 0), L("CRASH", DRM_HH_METAL, 60, 120, 100, 90, 70, 0),
        L("SHAKE", DRM_NOISE, 64, 20, 110, 40, 75, 0), L("CLAVE", DRM_RESON, 82, 35, 10, 0, 127, 0),
        L("CONGA", DRM_TOM, 104, 40, 60, 20, 95, 0), L("ZAP", DRM_FM, 96, 30, 100, 30, 85, 0)}},
    {"SYNTH", {
        L("KICK", DRM_BD_SINTE, 56, 90, 30, 80, 110, 0), L("SUB", DRM_BD_ANALOG, 52, 110, 40, 10, 127, 0),
        L("HAT", DRM_HH_ANALOG, 70, 15, 110, 90, 80, 1), L("OHAT", DRM_HH_METAL, 66, 70, 100, 40, 75, 1),
        L("SNARE", DRM_SD_SINTE, 76, 40, 100, 100, 95, 0), L("CLAP", DRM_CLAP, 70, 70, 90, 110, 95, 0),
        L("BLIP", DRM_FM, 100, 10, 50, 80, 85, 0), L("ZAP", DRM_FM, 70, 30, 120, 40, 90, 0),
        L("METAL", DRM_FM, 80, 60, 100, 110, 80, 0), L("BLOCK", DRM_RESON, 40, 30, 30, 30, 90, 0),
        L("COWB", DRM_COWBELL, 70, 40, 70, 60, 85, 0), L("NOISE", DRM_NOISE, 64, 60, 40, 0, 80, 0),
        L("SHAK", DRM_NOISE, 64, 15, 110, 70, 80, 0), L("TOM", DRM_TOM, 60, 70, 60, 40, 100, 0),
        L("PERC", DRM_TOM, 100, 40, 90, 120, 90, 0), L("KICK3", DRM_BD_ANALOG, 80, 30, 80, 127, 100, 0)}},
};

void bm_kit_apply(bm_tr *tr, int kit)
{
    const bm_kit *k = &bm_kits[(kit % BM_KITS_N + BM_KITS_N) % BM_KITS_N];
    for (int l = 0; l < BM_LANES; l++) {
        const bm_kit_lane *s = &k->lane[l];
        bm_lane *d = &tr->lane[l];
        d->model = s->model; d->tune = s->tune; d->decay = s->decay; d->tone = s->tone;
        d->color = s->color; d->level = s->level; d->choke = s->choke;
    }
}

const char *bm_lane_label(const bm_tr *tr, int kit, int lane)
{
    const bm_kit_lane *s = &bm_kits[(kit % BM_KITS_N + BM_KITS_N) % BM_KITS_N].lane[lane & 15];
    int m = tr->lane[lane & 15].model;
    return m == s->model || m >= DRM_COUNT ? s->name : bm_drm_short[m];
}

/* ------------------------------------------------------------ comunes */
const bm_ppage *bm_engine_pages(int engine, int *n)
{
    switch (engine) {
    case ENG_MINI: *n = BM_MN_PAGES; return bm_mn_pages;
    case ENG_BUNO: *n = BM_BN_PAGES; return bm_bn_pages;
    case ENG_BITTAL: *n = BM_BT_PAGES; return bm_bt_pages;
    default: *n = 0; return NULL;
    }
}

int bm_engine_page_of(int engine, int what)
{
    static const int8_t PG[3][3] = {{6, 5, 7}, {6, 7, 8}, {4, 4, 5}};   /* B-MINI, BITTAL, BUNO-6 */
    int m = engine == ENG_MINI ? 0 : engine == ENG_BITTAL ? 1 : engine == ENG_BUNO ? 2 : -1;
    return m < 0 || what < 0 || what > 2 ? -1 : PG[m][what];
}

int bm_prm_get(const bm_pattern *p, int t, const bm_pdef *d)
{
    return d->slot & PS_M0 ? p->trk[t].macro[d->slot & 3] : p->prm[t][d->slot];
}

static void prm_set(bm_pattern *p, int t, const bm_pdef *d, int v)
{
    if (d->slot & PS_M0) p->trk[t].macro[d->slot & 3] = (uint8_t)v; else p->prm[t][d->slot] = (uint8_t)v;
}

void bm_prm_defaults(bm_pattern *p, int t)
{
    memset(p->prm[t], 0, BM_PRM);
    int n;
    const bm_ppage *pg = bm_engine_pages(p->trk[t].engine, &n);
    for (int i = 0; i < n; i++)
        for (int k = 0; k < pg[i].n; k++) prm_set(p, t, &pg[i].p[k], pg[i].p[k].def);
    if (p->trk[t].engine == ENG_BITTAL) {
        p->trk[t].macro[0] = 64; p->trk[t].macro[1] = 0; p->trk[t].macro[2] = 0; p->trk[t].macro[3] = 0;
    }
}

/* El preset es un snapshot del banco de la máquina (core/bm_snap.h): sus ajustes y sus macros. */
void bm_preset_apply(bm_pattern *p, int t, int preset)
{
    int eng = p->trk[t].engine;
    if (eng != ENG_MINI && eng != ENG_BITTAL && eng != ENG_BUNO) return;
    bm_prm_defaults(p, t);
    int n = bm_snap_count(eng);
    if (!n) return;
    const uint8_t *s = bm_snap_get(eng, (preset % n + n) % n);
    if (!s) return;
    memcpy(p->prm[t], s + 40, BM_PRM < BM_SNAP_PRM ? BM_PRM : BM_SNAP_PRM);
    memcpy(p->trk[t].macro, s + 36, 4);
    if (eng == ENG_BITTAL) { p->prm[t][BT_TAB1] = 0; p->prm[t][BT_TAB2] = 0; }
}

void bm_prm_fmt(const bm_pdef *d, int v, char *b, int cap)
{
    float x;
    switch (d->fmt) {
    case PF_PCT: snprintf(b, (size_t)cap, "%d%%", (v * 100 + 63) / 127); break;
    case PF_BIP: snprintf(b, (size_t)cap, "%+d", v - 64); break;
    case PF_SEMI: snprintf(b, (size_t)cap, "%+d ST", (v - 64) * 24 / 63); break;
    case PF_DSEMI: snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 3) / 8); break;
    case PF_CENT: snprintf(b, (size_t)cap, "%+d CT", (v - 64) * 50 / 63); break;
    case PF_HZ:
        x = bm_cut_hz(v);
        if (x < 1000.0f) snprintf(b, (size_t)cap, "%d HZ", (int)x); else snprintf(b, (size_t)cap, "%.1f KHZ", (double)(x / 1000.0f));
        break;
    case PF_TIME:
        x = bm_env_s(v);
        if (x < 1.0f) snprintf(b, (size_t)cap, "%d MS", (int)(x * 1000.0f + 0.5f)); else snprintf(b, (size_t)cap, "%.1f S", (double)x);
        break;
    case PF_RATE: x = bm_lfo_hz(v); snprintf(b, (size_t)cap, x < 10.0f ? "%.2f HZ" : "%.1f HZ", (double)x); break;
    case PF_LIST: snprintf(b, (size_t)cap, "%s", d->list && v <= d->max ? d->list[v] : "?"); break;
    case PF_ONOFF: snprintf(b, (size_t)cap, "%s", v ? "YES" : "NO"); break;
    default: snprintf(b, (size_t)cap, "%d", v); break;
    }
}
