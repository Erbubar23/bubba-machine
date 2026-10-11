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
static inline int app_midi(const bm_app *a, int t) { return a->pat.trk[t].engine == ENG_MIDI; }
static inline int app_engine(const bm_app *a, int t) { return a->pat.trk[t].engine; }
int  app_scale(const bm_app *a, int t, int *root);   /* escala del canal (o la del proyecto) */
void app_set_view(bm_app *a, int v);
void app_enter(bm_app *a, int t, int page);           /* entra a un canal y a una página */
void app_select_track(bm_app *a, int t, int keep_page);
int  app_page_ok(const bm_app *a, int t, int page);

/* guardado y tomas */
void app_job(bm_app *a, int type, int arg);
void app_save_jobs(bm_app *a, int with_cur);
void app_send_sources(bm_app *a);                     /* loops A y B del patrón al audio */
void app_preview_take(bm_app *a, uint8_t src);        /* fuente de escucha (TOMAS / CORTAR) */
void app_cut_wave(bm_app *a);                         /* dibujo de la toma que se corta */
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

/* entrada (bm_input.c) */
void app_on_gesture(void *u, int id, bm_gkind k);
void app_input_tick(bm_app *a);
void app_leds(bm_app *a);

extern const uint8_t bm_master_steps_tab[6];
extern const uint8_t bm_roll_ticks[5];

#endif
