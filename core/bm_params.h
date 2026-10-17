/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — parámetros de sonido: qué ajustes tiene cada motor, cómo se muestran y cómo se
 * traducen a valores físicos (Hz, segundos). Lo usan la pantalla (páginas del editor), los motores
 * (para calcular) y los presets/kits de fábrica.
 *
 * Cada canal guarda 4 macros (bm_track.macro, las perillas de SEQ y las que admiten P-Lock) y
 * BM_PRM parámetros más (bm_pattern.prm). Un parámetro dice en qué casilla vive: PS_M0..PS_M3 es
 * una macro, PS_TRK(campo) un campo de bm_track (ACCENT, GLIDE, ATTACK, TRANSPOSE...), si no es un
 * índice de prm[]. Todos valen 0..max (max <= 127).
 *
 * Las páginas de cada máquina siguen el panel de la de verdad, en su orden, de a 4 perillas. */
#ifndef BM_PARAMS_H
#define BM_PARAMS_H

#include <stddef.h>
#include "bm_pattern.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { PF_PCT, PF_BIP, PF_SEMI, PF_DSEMI, PF_CENT, PF_HZ, PF_TIME, PF_RATE, PF_LIST, PF_ONOFF, PF_INT, PF_DB,
       PF_TRSP,      /* transposición del canal: 24 = 0 semitonos */
       PF_OHZ,       /* corte del BB-303 (bm_cutoff_hz) */
       PF_DMS,       /* caída del BB-303 (bm_decay_ms) */
       PF_GLIDE,     /* slide del BB-303 en ms */
       PF_MOD,       /* B-MINI: cantidad de modulación con su destino (64 = nada, < OSC, > FILT) */
       PF_ALG,       /* BX-7: 0 = el algoritmo del patch, 1..32 */
       PF_OVR };     /* BX-7: 0 = lo del patch, 1..100 = 0..99 */
#define PS_M0 0x80u               /* casilla = macro 0..3 */
#define PS_T0 0x40u               /* casilla = byte de bm_track (offsetof) */
#define PS_TRK(f) ((uint8_t)(PS_T0 + offsetof(bm_track, f)))
static inline bool bm_ps_trk(unsigned slot) { return (slot & (PS_M0 | PS_T0)) == PS_T0; }

typedef struct {
    const char *name;             /* <= 8 letras */
    uint8_t slot, def, max, fmt;
    const char *const *list;      /* PF_LIST: un nombre por valor */
} bm_pdef;

typedef struct { const char *name; uint8_t n; bm_pdef p[4]; } bm_ppage;

/* ------------------------------------------------------------ BB-303 (TB-303): VCO, VCF y ENV
 * macros: 0 CUTOFF, 1 RESO, 2 ENV MOD, 3 DECAY · PRESETS: los sonidos de bm_ac_presets */
enum { AC_WAVE, AC_NPRM };
#define BM_AC_PAGES 2
extern const bm_ppage bm_ac_pages[BM_AC_PAGES];
typedef struct { const char *name; uint8_t cut, res, env, dec, acc, glide, wave; } bm_ac_preset;
#define BM_AC_PRESETS 8
extern const bm_ac_preset bm_ac_presets[BM_AC_PRESETS];

/* ------------------------------------------------------------ B-MINI (Minimoog): tres osciladores y escalera
 * macros: 0 CUTOFF, 1 EMPHASIS, 2 CONTOUR (cantidad de la envolvente), 3 F DECAY (64 = la de la página) */
enum { MN_W1, MN_R1, MN_L1, MN_W2, MN_R2, MN_T2, MN_L2, MN_W3, MN_R3, MN_T3, MN_L3, MN_NOISE,
       MN_DRIVE, MN_KEYT, MN_VELF, MN_FA, MN_FD, MN_FS, MN_FR, MN_AA, MN_AD, MN_AS, MN_AR,
       MN_LRATE, MN_LAMT, MN_LDEST, MN_GLIDE, MN_MODE, MN_VELA, MN_TUNE, MN_NPRM };
enum { MN_WAVE_TRI, MN_WAVE_SHARK, MN_WAVE_SAW, MN_WAVE_SQR, MN_WAVE_WIDE, MN_WAVE_NARROW, MN_WAVES };
enum { MN_D_PITCH, MN_D_CUT, MN_LDESTS };
enum { WM_POLY, WM_MONO, WM_LEGATO, WM_MODES };
#define BM_MN_PAGES 9
extern const bm_ppage bm_mn_pages[BM_MN_PAGES];

/* ------------------------------------------------------------ BX-7 (DX7): el patch y encima los ajustes
 * macros: 0 MOD LVL, 1 FEEDBACK, 2 DECAY (R2-3), 3 RELEASE (R4); 64 = el patch tal cual */
enum { FX_ALG, FX_LSPD, FX_LDLY, FX_LPMD, FX_LAMD, FX_NPRM };
#define BM_FX_PAGES 4
extern const bm_ppage bm_fx_pages[BM_FX_PAGES];

/* ------------------------------------------------------------ BUNO-6 (Juno-6): un DCO por voz y chorus
 * macros: 0 FREQ, 1 RES, 2 ENV, 3 LFO (cuánto mueve el LFO al VCF) */
enum { BN_SAW, BN_PULSE, BN_RANGE, BN_SUB, BN_NOISE, BN_PWMSRC, BN_LFODCO, BN_HPF, BN_ENVPOL, BN_PWM,
       BN_KEYT, BN_VELF, BN_A, BN_D, BN_S, BN_R, BN_VCA, BN_LRATE, BN_LDELAY, BN_CHORUS, BN_VELA, BN_NPRM };
enum { BN_PWM_LFO, BN_PWM_MAN, BN_PWM_ENV, BN_PWMS };
enum { BN_CH_OFF, BN_CH_I, BN_CH_II, BN_CH_BOTH, BN_CHORUSES };
#define BM_BN_PAGES 8
extern const bm_ppage bm_bn_pages[BM_BN_PAGES];
extern const char *const bm_bn_chorus_names[BN_CHORUSES];

/* ------------------------------------------------------------ BITTAL (Vital): dos wavetables de un snapshot
 * Las 4 macros son las 4 macros del snapshot: cada una mueve un destino (BT_MxD) con su cantidad. */
enum { BT_L1, BT_S1, BT_F1, BT_P1, BT_U1, BT_D1, BT_TAB1,
       BT_L2, BT_S2, BT_F2, BT_P2, BT_U2, BT_D2, BT_TAB2,
       BT_FT, BT_CUT, BT_RES, BT_DRIVE, BT_FENV, BT_KEYT, BT_VELF,
       BT_AA, BT_AD, BT_AS, BT_AR, BT_MA, BT_MD, BT_MS, BT_MR,
       BT_LSHAPE, BT_LRATE, BT_LDEST, BT_LAMT,
       BT_M1D, BT_M1A, BT_M2D, BT_M2A, BT_M3D, BT_M3A, BT_M4D, BT_M4A,
       BT_MODE, BT_GLIDE, BT_VELA, BT_NPRM };
enum { BT_F_LP24, BT_F_LP12, BT_F_BP, BT_F_HP, BT_F_LADDER, BT_F_OFF, BT_FILTERS };
enum { BT_L_SIN, BT_L_TRI, BT_L_SAW, BT_L_SQR, BT_L_SH, BT_LSHAPES };
enum { BT_D_PITCH, BT_D_CUT, BT_D_POS, BT_D_AMP, BT_LDESTS };
enum { BTM_NONE, BTM_CUT, BTM_RES, BTM_POS1, BTM_POS2, BTM_POS, BTM_LVL2, BTM_DET, BTM_LFO, BTM_FENV, BTM_DRIVE,
       BTM_REL, BTM_SPACE, BTM_TARGETS };
#define BM_BT_PAGES 12
extern const bm_ppage bm_bt_pages[BM_BT_PAGES];
extern const char *const bm_btm_names[BTM_TARGETS];
/* cuánto mueve la macro i (0..127, 64 = nada) el destino dest en un canal BITTAL: -1..1 */
float bm_bt_macro(const uint8_t *prm, const uint8_t mac[4], int dest);

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

/* páginas del editor del motor del canal (NULL si no tiene) */
const bm_ppage *bm_engine_pages(int engine, int *n);
/* la página de una función en el editor de la máquina (-1 si no tiene): ENV la primera vez, ENV
 * otra vez, LFO */
enum { EPG_ENV1, EPG_ENV2, EPG_LFO };
int  bm_engine_page_of(int engine, int what);
static inline bool bm_engine_paged(int engine) { return engine >= ENG_ACID && engine < ENG_COUNT; }
/* la definición de la macro i de una máquina (nombre, formato y valor de fábrica), o NULL */
const bm_pdef *bm_engine_macro(int engine, int i);
/* las máquinas de T3-T6 y su "inspirada en" (solo para la pantalla) */
#define BM_MACHINES 5
extern const uint8_t bm_machines[BM_MACHINES];
const char *bm_machine_origin(int engine);
/* valor actual de un parámetro de un canal y su escritura directa (sin cola de comandos) */
int  bm_prm_get(const bm_pattern *p, int t, const bm_pdef *d);
/* Los bytes de prm[] que escribe un valor (PF_MOD escribe dos). Solo para casillas de prm[]. */
int  bm_prm_split(const bm_pdef *d, int v, uint8_t slot[2], uint8_t val[2]);
void bm_prm_defaults(bm_pattern *p, int t);            /* el sonido del canal a fábrica (la transposición queda) */
void bm_preset_apply(bm_pattern *p, int t, int preset);   /* el preset del banco de la máquina del canal */
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
