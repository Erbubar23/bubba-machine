/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — lo que comparten las máquinas de los canales de notas por dentro (V_MACH): cómo se
 * describe una perilla de una página, cómo se lee, se escribe y se muestra, y lo que se toca igual en
 * todas (OCT, REC tipo SLOOP, la nota que arranca la grabación, la página SEQ). Cada máquina tiene sus
 * menús (core/bm_bx7.c, core/bm_b303.c y core/bm_panel.c: B-MINI, BUNO-6, BITTAL). */
#ifndef BM_MACH_H
#define BM_MACH_H

#include "bm_app_int.h"

/* qué mueve una perilla: un byte de la voz de la BX-7 (K_FMV) o de su operador (K_OP), un campo del
 * canal, una macro, un byte de prm, el largo en compases, el ON/OFF de una casilla de FX, volver al
 * preset, o el LATCH del arpegiador. K_PRMI: un byte de prm al revés (max - v); K_R3: el RANGE del OSC 3
 * del B-MINI, con LO primero (en prm va al final, MN_R_LO). */
enum { K_NONE, K_FMV, K_OP, K_TRK, K_MAC, K_PRM, K_LEN, K_ON, K_RST, K_LATCH, K_MACH, K_VIEWX, K_VIEWY, K_QUANT,
       K_PRMI, K_R3,
       K_SLICE };                        /* el Slicer: un campo del pad elegido o de su ventana (core/bm_slicer.c) */
enum { F_INT, F_P1, F_LIST, F_ONOFF, F_BIP, F_PCT, F_RAWPCT, F_TRSP, F_DT, F_HZ, F_NOTE, F_BARS, F_STEPS, F_CH,
       F_OHZ, F_DMS, F_GLIDE,       /* el corte, la caída y el slide del BB-303 */
       F_PHZ, F_TIME, F_RATE, F_MIX,     /* las de panel: el corte, un tiempo, la velocidad del LFO, MOD MIX (OSC 3 / LFO) */
       F_SEMI, F_CENT };                 /* BITTAL: TRANSP en semitonos, FINE en cents */
typedef struct { const char *name; uint8_t kind, idx, lo, max, fmt; const char *const *list; } bxp;

#define TRK(f) ((uint8_t)offsetof(bm_track, f))
#define NONE {"", K_NONE, 0, 0, 0, F_INT, NULL}

extern const bxp MACH_SEQ[4];                 /* SEQ: SPEED; K3/K4 recorren el piano roll */
extern const bxp MACH_GLO[2][4];              /* GLO: LENGTH, MACHINE, MIDI CH, MIDI OUT; pág 2 QUANT (K3), RESET */
extern const char *const MACH_QUANTS[4];
#define MACH_QUANT {"QUANT", K_QUANT, 0, 0, 3, F_LIST, MACH_QUANTS}   /* PLAY+REC: 25 50 75 100 % */
extern const bxp MACH_ARP[2][4];              /* ARP: MODE OCTAVE RATE GATE / LATCH */

int  mach_get(const bm_app *a, const bxp *p);
void mach_fmt(const bxp *p, int v, char *b, int cap);
int  mach_pct(const bxp *p, int v);
/* gira la perilla k: escribe el valor y devuelve el nuevo (-1 si no cambió; el título lo arma quien llama) */
int  mach_turn(bm_app *a, const bxp *p, int d);
void mach_legend(const bm_app *a, const bxp *P, bm_legend L[4]);
bool mach_oct(bm_app *a, int id, bm_gkind k);         /* OCT -/+ (los dos: todo a 0); true = lo usó */
void mach_rec(bm_app *a, bm_gkind k);                 /* REC tipo SLOOP; sostenido 2 s borra el canal */
/* toca la nota de la tecla k con esa fuerza; con REC armado la primera nota arranca y graba. Devuelve 1
 * si grabó la nota (para marcarle acento o slide). */
int  mach_play(bm_app *a, int k, int vel);
uint8_t mach_common_led(const bm_app *a, int i);      /* HOME, REC y OCT: 0xFF si no es uno de esos */
void mach_head(bm_app *a);                            /* al entrar: sin grabación armada ni casilla tomada */
bool mach_knobs_shown(const bm_app *a);               /* la pantalla ya muestra K1-K4: sin la franja de perillas */

/* el BB-303 (core/bm_b303.c) */
int  b303_page_of(int btn);
bool b303_button(bm_app *a, int id, bm_gkind k);
void b303_key(bm_app *a, int k, int w, int b);
void b303_encoder(bm_app *a, int id, int d);
void b303_legend(const bm_app *a, bm_legend L[4]);
void b303_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
uint8_t b303_btn_led(const bm_app *a, int i);
const char *b303_title(const bm_app *a, char *buf, int cap);
void b303_keys(const bm_app *a, bm_keyfn K[BM_KEYS]);  /* SCL: las blancas son los pasos */
extern const char *const b303_fx_names[ACF_N];
void mach_scl_toggle(bm_app *a, int w);               /* SCL: el ACCENT (o SLIDE / GLIDE) del paso w de la página */

/* las de panel: B-MINI, BUNO-6, BITTAL (core/bm_panel.c) */
int  panel_page_of(int btn);
bool panel_button(bm_app *a, int id, bm_gkind k);
void panel_key(bm_app *a, int k, int w, int b);
void panel_encoder(bm_app *a, int id, int d);
void panel_legend(const bm_app *a, bm_legend L[4]);
void panel_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
uint8_t panel_btn_led(const bm_app *a, int i);
const char *panel_title(const bm_app *a, char *buf, int cap);
int  panel_sub(const bm_app *a);                      /* la página del menú que se ve */
const bxp *panel_knobs(const bm_app *a);              /* sus cuatro perillas */
int  panel_edit_n(const bm_app *a);                   /* las secciones de EDIT y sus nombres */
const char *panel_edit_name(const bm_app *a, int i);
extern const char *const panel_fx_names[PXF_N];
extern const char *const bmini_r3_names[6];

/* el Slicer (core/bm_slicer.c): EDIT (TRIM, SOUND), ENV, FX y GLO; SEQ, LFO y SCL son su rejilla (V_CH) */
int  slicer_page_of(int btn);
bool slicer_button(bm_app *a, int id, bm_gkind k);
void slicer_key(bm_app *a, int k, int w, int b);
void slicer_encoder(bm_app *a, int id, int d);
void slicer_legend(const bm_app *a, bm_legend L[4]);
void slicer_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
uint8_t slicer_btn_led(const bm_app *a, int i);
const char *slicer_title(const bm_app *a, char *buf, int cap);
void slicer_keys(const bm_app *a, bm_keyfn K[BM_KEYS]);
void slicer_status(const bm_app *a, char *b, int cap, uint16_t *col);   /* la cabecera: SAMPLE o el pad y su toma */
bool slicer_knobs_shown(const bm_app *a);
int  slicer_sub(const bm_app *a);
const bxp *slicer_knobs(const bm_app *a);
int  slicer_get(const bm_app *a, const bxp *p);
void slicer_fmt(const bm_app *a, const bxp *p, int v, char *b, int cap);
int  slicer_pct(const bm_app *a, const bxp *p, int v);
int  slicer_turn(bm_app *a, const bxp *p, int d);
bool slicer_live(const bm_app *a, const bxp *p);      /* ¿la perilla hace algo ahora? */
bool slicer_take(const bm_app *a, int pad, bm_smp_src *m);   /* la toma del pad (si tiene) */
void slicer_view(const bm_app *a, uint32_t *v0, uint32_t *v1);   /* TRIM: la ventana de la onda, en muestras */
void slicer_grid_knob(bm_app *a, int k, int d);      /* la rejilla: LEVEL, LENGTH, PITCH y RELEASE */
void slicer_grid_legend(const bm_app *a, bm_legend L[4]);
extern const char *const slicer_mode_names[PAD_MODES];

#endif
