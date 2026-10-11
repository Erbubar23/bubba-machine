/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — parámetros de sonido, kits y presets de fábrica (core/bm_params.h). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "bm_params.h"
#include "bm_drums.h"

/* ------------------------------------------------------------ valores físicos */
float bm_cut_hz(int v) { return 20.0f * exp2f((float)v * (10.0f / 127.0f)); }
float bm_env_s(int v) { return 0.001f * powf(10.0f, (float)v * (4.0f / 127.0f)); }
float bm_lfo_hz(int v) { return 0.05f * exp2f((float)v * (9.0f / 127.0f)); }
float bm_reso_q(int v) { float x = (float)v / 127.0f; return 0.5f + 24.5f * x * x * x; }

/* ------------------------------------------------------------ ANALOG */
static const char *const WAVES1[AN_WAVES] = {"SIERRA", "CUADRADA", "TRIANG", "SENO", "3 SIERRAS"};
static const char *const WAVES2[4] = {"SIERRA", "CUADRADA", "TRIANG", "SENO"};
static const char *const FILTERS[AN_FILTERS] = {"LP 24", "LP 12", "BP", "HP"};
static const char *const LSHAPES[AN_LSHAPES] = {"SENO", "TRIANG", "SIERRA", "CUADRADA", "AZAR"};
static const char *const LDESTS[AN_LDESTS] = {"TONO", "CORTE", "FORMA", "NIVEL"};
static const char *const MODES[AN_MODES] = {"POLI", "MONO", "LEGATO"};

#define P(n, s, d, m, f, l) {n, s, d, m, f, l}
const bm_ppage bm_an_pages[BM_AN_PAGES] = {
    {"OSC 1", 4, {P("ONDA", AN_W1, AN_WAVE_SAW, AN_WAVES - 1, PF_LIST, WAVES1), P("FORMA", PS_M0 + 3, 0, 127, PF_PCT, 0),
                  P("SUB", AN_SUB, 0, 127, PF_PCT, 0), P("RUIDO", AN_NOISE, 0, 127, PF_PCT, 0)}},
    {"OSC 2", 4, {P("ONDA", AN_W2, AN_WAVE_SAW, 3, PF_LIST, WAVES2), P("AFINA", AN_SEMI2, 64, 127, PF_SEMI, 0),
                  P("FINO", AN_FINE2, 70, 127, PF_CENT, 0), P("NIVEL", AN_MIX2, 0, 127, PF_PCT, 0)}},
    {"CRUCE", 4, {P("SYNC", AN_SYNC, 0, 1, PF_ONOFF, 0), P("RING", AN_RING, 0, 127, PF_PCT, 0),
                  P("ENV>OSC2", AN_PENV2, 64, 127, PF_BIP, 0), P("GLIDE", AN_GLIDE, 0, 127, PF_TIME, 0)}},
    {"FILTRO", 4, {P("TIPO", AN_FTYPE, AN_F_LP24, AN_FILTERS - 1, PF_LIST, FILTERS), P("CORTE", PS_M0 + 0, 90, 127, PF_HZ, 0),
                   P("RESO", PS_M0 + 1, 20, 127, PF_PCT, 0), P("DRIVE", AN_DRIVE, 0, 127, PF_PCT, 0)}},
    {"FILTRO +", 3, {P("ENVOLV", PS_M0 + 2, 94, 127, PF_BIP, 0), P("TECLADO", AN_KEYT, 64, 127, PF_PCT, 0),
                     P("VELOCID", AN_VELF, 30, 127, PF_PCT, 0)}},
    {"ENV FILTRO", 4, {P("ATAQUE", AN_FA, 0, 127, PF_TIME, 0), P("DECAY", AN_FD, 60, 127, PF_TIME, 0),
                       P("SUSTAIN", AN_FS, 40, 127, PF_PCT, 0), P("SALIDA", AN_FR, 50, 127, PF_TIME, 0)}},
    {"ENV NIVEL", 4, {P("ATAQUE", AN_AA, 0, 127, PF_TIME, 0), P("DECAY", AN_AD, 60, 127, PF_TIME, 0),
                      P("SUSTAIN", AN_AS, 100, 127, PF_PCT, 0), P("SALIDA", AN_AR, 50, 127, PF_TIME, 0)}},
    {"LFO", 4, {P("FORMA", AN_LSHAPE, AN_L_TRI, AN_LSHAPES - 1, PF_LIST, LSHAPES), P("VELOC", AN_LRATE, 60, 127, PF_RATE, 0),
                P("DESTINO", AN_LDEST, AN_D_PITCH, AN_LDESTS - 1, PF_LIST, LDESTS), P("CANTIDAD", AN_LAMT, 0, 127, PF_PCT, 0)}},
    {"VOZ", 2, {P("MODO", AN_MODE, AN_M_POLY, AN_MODES - 1, PF_LIST, MODES), P("VEL>NIVEL", AN_VELA, 64, 127, PF_PCT, 0)}},
};

/* Presets: la base es la primera definición de cada parámetro y encima van los cambios. */
#define END 0xFF
static const uint8_t D_SAW[] = {END};
static const uint8_t D_BASS[] = {AN_MODE, AN_M_MONO, AN_SUB, 70, AN_FD, 52, AN_FS, 10, AN_AD, 70, AN_AS, 90, AN_AR, 30, AN_GLIDE, 40, AN_KEYT, 40, END};
static const uint8_t D_SUB[] = {AN_MODE, AN_M_MONO, AN_W1, AN_WAVE_TRI, AN_SUB, 110, AN_FD, 40, AN_FS, 30, AN_AR, 25, AN_GLIDE, 30, END};
static const uint8_t D_PLUCK[] = {AN_W1, AN_WAVE_SQR, AN_FD, 45, AN_FS, 0, AN_FR, 45, AN_AD, 62, AN_AS, 0, AN_AR, 55, AN_VELF, 70, END};
static const uint8_t D_PAD[] = {AN_W2, AN_WAVE_SAW, AN_MIX2, 110, AN_FINE2, 78, AN_FA, 85, AN_FD, 95, AN_FS, 70, AN_FR, 90,
                                AN_AA, 85, AN_AS, 120, AN_AR, 92, AN_LSHAPE, AN_L_TRI, AN_LRATE, 40, AN_LDEST, AN_D_CUT, AN_LAMT, 25, END};
static const uint8_t D_STRINGS[] = {AN_W1, AN_WAVE_SUPER, AN_FTYPE, AN_F_LP12, AN_FA, 70, AN_FS, 90, AN_AA, 72, AN_AS, 120, AN_AR, 85, AN_KEYT, 90, END};
static const uint8_t D_SYNC[] = {AN_MODE, AN_M_LEGATO, AN_W2, AN_WAVE_SAW, AN_MIX2, 127, AN_SEMI2, 76, AN_SYNC, 1, AN_PENV2, 100,
                                 AN_FD, 70, AN_FS, 60, AN_GLIDE, 35, END};
static const uint8_t D_LEAD[] = {AN_MODE, AN_M_LEGATO, AN_W2, AN_WAVE_SQR, AN_MIX2, 90, AN_FINE2, 74, AN_GLIDE, 45, AN_FS, 70,
                                 AN_LSHAPE, AN_L_SIN, AN_LRATE, 75, AN_LAMT, 8, AN_DRIVE, 40, END};
static const uint8_t D_BRASS[] = {AN_W2, AN_WAVE_SAW, AN_MIX2, 100, AN_FINE2, 76, AN_FA, 50, AN_FD, 75, AN_FS, 70, AN_AA, 30, AN_AS, 110, END};
static const uint8_t D_ORGAN[] = {AN_W1, AN_WAVE_SQR, AN_W2, AN_WAVE_SIN, AN_SEMI2, 96, AN_MIX2, 100, AN_SUB, 60, AN_FTYPE, AN_F_LP12,
                                  AN_FS, 127, AN_AD, 0, AN_AS, 127, AN_AR, 20, AN_VELA, 0, END};
static const uint8_t D_BELL[] = {AN_W1, AN_WAVE_SIN, AN_W2, AN_WAVE_SIN, AN_SEMI2, 101, AN_FINE2, 72, AN_MIX2, 40, AN_RING, 110,
                                 AN_FTYPE, AN_F_LP12, AN_FS, 127, AN_AD, 90, AN_AS, 0, AN_AR, 90, END};
static const uint8_t D_PWM[] = {AN_W1, AN_WAVE_SQR, AN_LSHAPE, AN_L_TRI, AN_LRATE, 50, AN_LDEST, AN_D_SHAPE, AN_LAMT, 80,
                                AN_FA, 20, AN_AA, 20, AN_AS, 110, AN_AR, 70, END};
static const uint8_t D_STAB[] = {AN_W2, AN_WAVE_SAW, AN_MIX2, 110, AN_SEMI2, 83, AN_FD, 50, AN_FS, 0, AN_AD, 66, AN_AS, 0, AN_AR, 50, END};
static const uint8_t D_SUPER[] = {AN_MODE, AN_M_LEGATO, AN_W1, AN_WAVE_SUPER, AN_SUB, 40, AN_GLIDE, 30, AN_FS, 80, AN_DRIVE, 30, END};
static const uint8_t D_NOISE[] = {AN_W1, AN_WAVE_SIN, AN_NOISE, 127, AN_FTYPE, AN_F_BP, AN_FA, 80, AN_FD, 95, AN_FS, 0, AN_FR, 90,
                                  AN_AA, 60, AN_AS, 127, AN_AR, 95, AN_KEYT, 127, END};
static const uint8_t D_ACIDISH[] = {AN_MODE, AN_M_LEGATO, AN_FTYPE, AN_F_LP24, AN_FD, 48, AN_FS, 0, AN_AS, 110, AN_AR, 25,
                                    AN_DRIVE, 70, AN_GLIDE, 30, AN_VELF, 90, END};

/* macros: CORTE, RESO, ENVOLV (64 = nada), FORMA */
const bm_preset bm_an_presets[] = {
    {"SIERRA", {90, 20, 94, 0}, D_SAW},
    {"BAJO", {45, 40, 100, 0}, D_BASS},
    {"SUB BAJO", {60, 10, 80, 0}, D_SUB},
    {"PLUCK", {50, 35, 110, 40}, D_PLUCK},
    {"PAD", {75, 25, 80, 0}, D_PAD},
    {"CUERDAS", {85, 10, 74, 60}, D_STRINGS},
    {"SYNC", {100, 15, 80, 0}, D_SYNC},
    {"SOLISTA", {88, 30, 86, 30}, D_LEAD},
    {"METALES", {60, 20, 104, 0}, D_BRASS},
    {"ORGANO", {110, 0, 64, 50}, D_ORGAN},
    {"CAMPANA", {120, 0, 64, 0}, D_BELL},
    {"PWM", {95, 20, 74, 40}, D_PWM},
    {"STAB", {58, 45, 108, 0}, D_STAB},
    {"SUPER", {96, 15, 80, 70}, D_SUPER},
    {"VIENTO", {70, 90, 96, 0}, D_NOISE},
    {"CHIRRIDO", {35, 95, 112, 20}, D_ACIDISH},
};
const int bm_an_npresets = (int)(sizeof bm_an_presets / sizeof bm_an_presets[0]);

/* ------------------------------------------------------------ batería */
static const char *const DRM_LIST[DRM_COUNT] = {"BD 808", "BD 909", "SD 808", "SD 909", "HAT 808", "METAL", "PALMAS",
                                                "TOM", "CENCERRO", "RESON", "RUIDO", "FM"};
static const char *const CHOKES[4] = {"NO", "1", "2", "3"};
const bm_ppage bm_lane_pages[BM_LANE_PAGES] = {
    {"GOLPE", 4, {P("AFINA", LN_TUNE, 64, 127, PF_DSEMI, 0), P("DECAY", LN_DECAY, 64, 127, PF_PCT, 0),
                  P("TONO", LN_TONE, 64, 127, PF_PCT, 0), P("COLOR", LN_COLOR, 64, 127, PF_PCT, 0)}},
    {"INSTRUMENTO", 4, {P("MODELO", LN_MODEL, 0, DRM_COUNT - 1, PF_LIST, DRM_LIST), P("NIVEL", LN_LEVEL, 100, 127, PF_INT, 0),
                        P("CORTA A", LN_CHOKE, 0, 3, PF_LIST, CHOKES), P("LARGO", LN_LEN, 16, 64, PF_INT, 0)}},
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
    {"SINTE", {
        L("KICK", DRM_BD_SINTE, 56, 90, 30, 80, 110, 0), L("SUB", DRM_BD_ANALOG, 52, 110, 40, 10, 127, 0),
        L("HAT", DRM_HH_ANALOG, 70, 15, 110, 90, 80, 1), L("OHAT", DRM_HH_METAL, 66, 70, 100, 40, 75, 1),
        L("SNARE", DRM_SD_SINTE, 76, 40, 100, 100, 95, 0), L("CLAP", DRM_CLAP, 70, 70, 90, 110, 95, 0),
        L("BLIP", DRM_FM, 100, 10, 50, 80, 85, 0), L("ZAP", DRM_FM, 70, 30, 120, 40, 90, 0),
        L("METAL", DRM_FM, 80, 60, 100, 110, 80, 0), L("BLOQ", DRM_RESON, 40, 30, 30, 30, 90, 0),
        L("COWB", DRM_COWBELL, 70, 40, 70, 60, 85, 0), L("RUIDO", DRM_NOISE, 64, 60, 40, 0, 80, 0),
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
    if (engine == ENG_ANALOG) { *n = BM_AN_PAGES; return bm_an_pages; }
    *n = 0;
    return NULL;
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
    if (p->trk[t].engine == ENG_ANALOG && t == 2) p->prm[t][AN_MODE] = AN_M_MONO;
}

void bm_preset_apply(bm_pattern *p, int t, int preset)
{
    if (p->trk[t].engine != ENG_ANALOG) return;
    const bm_preset *s = &bm_an_presets[(preset % bm_an_npresets + bm_an_npresets) % bm_an_npresets];
    bm_prm_defaults(p, t);
    memcpy(p->trk[t].macro, s->macro, 4);
    for (const uint8_t *d = s->diff; *d != END; d += 2) p->prm[t][d[0]] = d[1];
    if (t == 2 && p->prm[t][AN_MODE] == AN_M_POLY) p->prm[t][AN_MODE] = AN_M_MONO;
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
    case PF_ONOFF: snprintf(b, (size_t)cap, "%s", v ? "SI" : "NO"); break;
    default: snprintf(b, (size_t)cap, "%d", v); break;
    }
}
