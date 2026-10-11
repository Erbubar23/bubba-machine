/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — parámetros de sonido: qué ajustes tiene cada motor, cómo se muestran y cómo se
 * traducen a valores físicos (Hz, segundos). Lo usan la pantalla (páginas del editor), los motores
 * (para calcular) y los presets/kits de fábrica.
 *
 * Cada canal guarda 4 macros (bm_track.macro, las perillas de SEQ y las que admiten P-Lock) y
 * BM_PRM parámetros más (bm_pattern.prm). Un parámetro dice en qué casilla vive: PS_M0..PS_M3 es
 * una macro, si no es un índice de prm[]. Todos valen 0..max (max <= 127). */
#ifndef BM_PARAMS_H
#define BM_PARAMS_H

#include "bm_pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { PF_PCT, PF_BIP, PF_SEMI, PF_DSEMI, PF_CENT, PF_HZ, PF_TIME, PF_RATE, PF_LIST, PF_ONOFF, PF_INT, PF_DB };
#define PS_M0 0x80u               /* casilla = macro 0..3 */

typedef struct {
    const char *name;             /* <= 9 letras */
    uint8_t slot, def, max, fmt;
    const char *const *list;      /* PF_LIST: un nombre por valor */
} bm_pdef;

typedef struct { const char *name; uint8_t n; bm_pdef p[4]; } bm_ppage;

/* ------------------------------------------------------------ ANALOG */
enum { AN_W1, AN_SUB, AN_NOISE, AN_W2, AN_SEMI2, AN_FINE2, AN_MIX2, AN_SYNC, AN_RING, AN_PENV2, AN_GLIDE,
       AN_FTYPE, AN_DRIVE, AN_KEYT, AN_VELF, AN_FA, AN_FD, AN_FS, AN_FR, AN_AA, AN_AD, AN_AS, AN_AR,
       AN_LSHAPE, AN_LRATE, AN_LDEST, AN_LAMT, AN_MODE, AN_VELA, AN_NPRM };
/* macros: 0 CORTE, 1 RESO, 2 ENV>FILTRO (64 = nada), 3 FORMA de la onda 1 */
enum { AN_WAVE_SAW, AN_WAVE_SQR, AN_WAVE_TRI, AN_WAVE_SIN, AN_WAVE_SUPER, AN_WAVES };
enum { AN_F_LP24, AN_F_LP12, AN_F_BP, AN_F_HP, AN_FILTERS };
enum { AN_L_SIN, AN_L_TRI, AN_L_SAW, AN_L_SQR, AN_L_SH, AN_LSHAPES };
enum { AN_D_PITCH, AN_D_CUT, AN_D_SHAPE, AN_D_AMP, AN_LDESTS };
enum { AN_M_POLY, AN_M_MONO, AN_M_LEGATO, AN_MODES };
#define BM_AN_PAGES 9
enum { AN_PG_OSC1, AN_PG_OSC2, AN_PG_CROSS, AN_PG_FILTER, AN_PG_FILTER2, AN_PG_FILT, AN_PG_AMP, AN_PG_LFO, AN_PG_VOICE };   /* páginas */
extern const bm_ppage bm_an_pages[BM_AN_PAGES];

/* ------------------------------------------------------------ batería: un instrumento (bm_lane) */
enum { LN_LEN, LN_LEVEL, LN_TUNE, LN_DECAY, LN_MODEL, LN_TONE, LN_COLOR, LN_CHOKE };   /* = campos de bm_lane */
#define BM_LANE_PAGES 2
extern const bm_ppage bm_lane_pages[BM_LANE_PAGES];   /* slot = campo LN_* */

typedef struct { char name[6]; uint8_t model, tune, decay, tone, color, level, choke; } bm_kit_lane;
typedef struct { const char *name; bm_kit_lane lane[BM_LANES]; } bm_kit;
#define BM_KITS_N 3
extern const bm_kit bm_kits[BM_KITS_N];
void bm_kit_apply(bm_tr *tr, int kit);                 /* carga el kit en las líneas (no toca largos ni pasos) */
const char *bm_lane_label(const bm_tr *tr, int kit, int lane);   /* nombre del kit, o del modelo si se cambió */

/* ------------------------------------------------------------ presets de los motores de notas */
typedef struct { const char *name; uint8_t macro[4]; const uint8_t *diff; } bm_preset;   /* diff: pares (parámetro, valor) sobre la base, hasta 0xFF */
extern const bm_preset bm_an_presets[];
extern const int bm_an_npresets;

/* páginas del editor del motor del canal (NULL si no tiene) */
const bm_ppage *bm_engine_pages(int engine, int *n);
/* valor actual de un parámetro de un canal y su escritura directa (sin cola de comandos) */
int  bm_prm_get(const bm_pattern *p, int t, const bm_pdef *d);
void bm_prm_defaults(bm_pattern *p, int t);            /* parámetros del motor del canal a fábrica */
void bm_preset_apply(bm_pattern *p, int t, int preset);
void bm_prm_fmt(const bm_pdef *d, int v, char *buf, int cap);

/* traducción a valores físicos (compartida por la pantalla y los motores) */
float bm_cut_hz(int v);          /* 0..127 -> 20 Hz .. 20 kHz */
float bm_env_s(int v);           /* 0..127 -> 1 ms .. 10 s */
float bm_lfo_hz(int v);          /* 0..127 -> 0.05 .. 25 Hz */
float bm_reso_q(int v);          /* 0..127 -> Q 0.5 .. 25 */

#ifdef __cplusplus
}
#endif

#endif
