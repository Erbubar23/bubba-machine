/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — internos de la aplicación, compartidos por bm_app.c (estado, guardado,
 * tick) y bm_input.c (botones, teclas, perillas, leyenda). */
#ifndef BM_APP_INT_H
#define BM_APP_INT_H

#include <stddef.h>
#include "bm_app.h"

#define TRK_OFF(f)  ((uint8_t)offsetof(bm_track, f))
#define META_OFF(f) ((uint8_t)offsetof(bm_meta, f))

/* comandos al audio */
void app_push(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z);
void app_push_i(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z, int32_t i);
void app_push_ptr(bm_app *a, uint8_t type, uint8_t x, const void *p);
void app_push_ev(bm_app *a, int t, int s, int k, const bm_event *e);
void app_set_track(bm_app *a, int t, uint8_t off, int v);
void app_set_meta(bm_app *a, uint8_t off, int v);
void app_edited(bm_app *a);

/* avisos */
void app_toast(bm_app *a, const char *msg);
void app_overlay(bm_app *a, int enc, const char *title, const char *val, const char *unit, int pct, int ghost, uint16_t col);

/* Orden de los canales en pantalla (1.0): batería, cuatro que eligen máquina y el Slicer al final.
 * El canal de pantalla n (0-5) es la pista interna BM_ORDER[n]. */
static const uint8_t BM_ORDER[BM_TRACKS] = {0, 2, 3, 5, 4, 1};
static inline int app_pos(int t) { for (int i = 0; i < BM_TRACKS; i++) if (BM_ORDER[i] == t) return i; return 0; }
static inline int app_fixed(int t) { return t == 0 || t == 1; }               /* la batería y el Slicer no eligen */

/* estado */
static inline int app_playing(const bm_app *a) { return a->ctx.st.playing; }
static inline int app_kind(const bm_app *a, int t) { return bm_kind(&a->pat, t); }
enum { BOX_LANE = 2 };
static inline int app_engine(const bm_app *a, int t) { return a->pat.trk[t].engine; }
int  app_scale(const bm_app *a, int t, int *root);   /* escala del canal (o la del proyecto) */
void app_set_view(bm_app *a, int v);
void app_enter(bm_app *a, int t, int page);           /* entra a un canal y a una página */

/* guardado y tomas */
void app_job(bm_app *a, int type, int arg);
void app_save_jobs(bm_app *a, int with_cur);
void app_send_takes(bm_app *a);                       /* las tomas guardadas al audio (para los pads) */
bool app_take_info(const bm_app *a, int src, bm_smp_src *m);   /* toma 0..7 o BM_TAKE_NEW */
/* Kit desde una toma (how: 0 en los golpes, 8 o 16 partes iguales): los pads 1..n son sus cortes.
 * También escribe la secuencia que suena como la toma (la de antes se borra). Devuelve n. */
int  app_kit_from_take(bm_app *a, int src, int how);
void app_pad_take(bm_app *a, int pad, int src);       /* la toma entera en un pad */
static inline bool app_kit_empty(const bm_app *a) { for (int i = 0; i < BM_LANES; i++) if (bm_pad_ok(&a->pat.pad[i])) return false; return true; }
void app_select_pattern(bm_app *a, int idx);
void *app_track_data(bm_app *a, int t);
size_t app_track_size(int t);
void app_init_sound(bm_app *a, int t);
int  app_set_machine(bm_app *a, int t, int eng);     /* T3-T6: otra máquina (las notas quedan); 0 = se cambió */
int  app_machine_track(const bm_app *a, int engine); /* el canal T3-T6 con esa máquina, o -1 */


/* entrada (bm_input.c) */
void app_sloop(bm_app *a, int on);                    /* SLOOP-REC en la consola: las teclas tocan el canal del cursor y graban */
void app_on_gesture(void *u, int id, bm_gkind k);
void app_input_tick(bm_app *a);
void app_leds(bm_app *a);
/* lo de la entrada que comparte la BX-7 */
int  app_lst(const bm_app *a, int d);                 /* un giro como pasos de lista (+-1 por detent) */
void app_play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel);   /* toca (y con REC graba) */
void app_rec_at(bm_app *a, int k, int t, const uint8_t *notes, int n, uint32_t rt);   /* graba en el tick rt */
void app_note_keys(const bm_app *a, bm_keyfn *K, int t);   /* el teclado cromático de un canal de notas */
void app_rec_set(bm_app *a, int on);
void app_enc_source(bm_app *a, int t, int d);         /* PRESETS: el sonido del canal */
void app_layer_legend(const bm_app *a, bm_legend *L, int k, const char *name, const char *val, int pct);
/* lo de la entrada que comparte el Slicer (core/bm_slicer.c) */
void app_pad_down(bm_app *a, int k, int pad);         /* una tecla toca (y con REC sonando graba) un pad */
void app_slicer_grid(bm_app *a, int layer);           /* a la rejilla del Slicer (V_CH), con esa capa abierta (o ninguna) */
void app_kit_len(bm_app *a, int t, int k, int d);     /* el largo de todos los instrumentos o pads del canal (1-64 pasos) */

/* la máquina de un canal de notas por dentro (V_MACH, bm_mach.c): la reparte según la máquina */
bool mach_has(int engine);                            /* tiene pantalla propia (las de notas) */
int  mach_view_page(const bm_app *a);               /* la página de 16 pasos que muestra el piano roll */
int  app_mach_shown(const bm_app *a);               /* GLO de la máquina: la que se ve en MACHINE (la elegida o la que suena) */
void app_mach_pick(bm_app *a, int d);               /* GLO de la máquina, MACHINE: las disponibles; quedarse 2 s la carga */
int  mach_len(const bm_app *a);                     /* el largo del canal del cursor, en pasos */
bool mach_step_mode(const bm_app *a);               /* REC parado en el SEQ de una máquina: paso a paso */
void mach_key_up(bm_app *a, int k);                 /* una tecla soltada (paso a paso: el cursor avanza) */
void mach_cursor(bm_app *a, int d);                 /* SEQ: SELECT mueve el cursor de paso */
void app_step_rec(bm_app *a);                       /* REC parado en el SEQ: grabar paso a paso */
int  mach_page_of(int engine, int btn);               /* la página BXP_* de ese botón en esa máquina, o -1 */
void mach_enter(bm_app *a, int t, int page);          /* entrar a la máquina del canal t en la página BXP_* */
bool mach_button(bm_app *a, int id, bm_gkind k);      /* true = lo usó */
void mach_key(bm_app *a, int k, int w, int b);
void mach_encoder(bm_app *a, int id, int d);
void mach_legend_view(const bm_app *a, bm_legend L[4]);
void mach_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
void mach_keys(const bm_app *a, bm_keyfn K[BM_KEYS]);
uint8_t mach_btn_led(const bm_app *a, int i);
const char *mach_title(const bm_app *a, char *buf, int cap);   /* "LFO 1/2", "EDIT OP3 2/6"... */
/* la BX-7 (bm_bx7.c) */
bool bx7_button(bm_app *a, int id, bm_gkind k);
void bx7_key(bm_app *a, int k, int w, int b);
void bx7_encoder(bm_app *a, int id, int d);
void bx7_legend(const bm_app *a, bm_legend L[4]);
void bx7_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
void bx7_keys(const bm_app *a, bm_keyfn K[BM_KEYS]);
uint8_t bx7_btn_led(const bm_app *a, int i);
int  bx7_page_of(int btn);
const char *bx7_title(const bm_app *a, char *buf, int cap);


#endif
