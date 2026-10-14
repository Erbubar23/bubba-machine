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
void app_ask(bm_app *a, int kind, int arg, int arg2, const char *l1, const char *l2);

/* estado */
static inline int app_playing(const bm_app *a) { return a->ctx.st.playing; }
static inline int app_kind(const bm_app *a, int t) { return bm_kind(&a->pat, t); }
static inline int app_midi(const bm_app *a, int t) { (void)a; (void)t; return 0; }   /* 0.6: MIDI es MIDI OUT por canal */
enum { BOX_HOME, BOX_CH, BOX_LANE };
static inline int app_engine(const bm_app *a, int t) { return a->pat.trk[t].engine; }
int  app_scale(const bm_app *a, int t, int *root);   /* escala del canal (o la del proyecto) */
void app_set_view(bm_app *a, int v);
void app_enter(bm_app *a, int t, int page);           /* entra a un canal y a una página */
void app_select_track(bm_app *a, int t, int keep_page);
int  app_page_ok(const bm_app *a, int t, int page);

/* guardado y tomas */
void app_job(bm_app *a, int type, int arg);
void app_save_jobs(bm_app *a, int with_cur);
void app_send_takes(bm_app *a);                       /* las tomas guardadas al audio (para los pads) */
bool app_take_info(const bm_app *a, int src, bm_smp_src *m);   /* toma 0..7 o BM_TAKE_NEW */
void app_wave_view(bm_app *a, int src, uint32_t at, int zoom); /* dibujo de una toma (si cambió) */
/* Kit desde una toma (how: 0 en los golpes, 8 o 16 partes iguales): los pads 1..n son sus cortes.
 * Si la rejilla de T2 está vacía, también escribe la secuencia que suena como la toma. Devuelve n. */
int  app_kit_from_take(bm_app *a, int src, int how);
void app_pad_take(bm_app *a, int pad, int src);       /* la toma entera en un pad */
static inline bm_pad *app_pad(bm_app *a) { return &a->pat.pad[a->sel_lane[1] & 15]; }   /* el pad elegido */
static inline bool app_kit_empty(const bm_app *a) { for (int i = 0; i < BM_LANES; i++) if (bm_pad_ok(&a->pat.pad[i])) return false; return true; }
void app_select_pattern(bm_app *a, int idx);
void app_undo_snapshot(bm_app *a, int t);
void app_do_undo(bm_app *a);
void app_do_redo(bm_app *a);
void app_clear_track(bm_app *a, int t);
void app_copy_track(bm_app *a, int t);
void app_paste_track(bm_app *a, int t);
void *app_track_data(bm_app *a, int t);
size_t app_track_size(int t);
void app_init_sound(bm_app *a, int t);
void app_bass_machine(bm_app *a, int eng);          /* T3: BB-303 o B-MINI (las notas quedan) */

/* entrada (bm_input.c) */
void app_on_gesture(void *u, int id, bm_gkind k);
void app_input_tick(bm_app *a);
void app_leds(bm_app *a);

extern const uint8_t bm_master_steps_tab[6];
extern const uint8_t bm_roll_ticks[5];

#endif
