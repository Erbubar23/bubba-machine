/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — gramática de gestos (DISENO_3.0: cuatro gestos por botón).
 *
 *  TAP      soltar antes de 300 ms (o botón sin capa) y sin combo -> al soltar
 *  HOLD     300 ms con el botón abajo -> al cumplirse (no al soltar): capa en vivo
 *  LONG     2 s sin tocar nada más -> al cumplirse: pantalla especial
 *  COMBO    otra tecla/encoder durante la pulsación: HOLD inmediato si hay capa,
 *           y cancela el TAP y el LONG
 *  PRESS/UP botones instantáneos (PLAY, OCT) -> al presionar/soltar
 */
#ifndef BM_GESTURE_H
#define BM_GESTURE_H

#include "bm_types.h"

typedef enum { G_PRESS, G_UP, G_TAP, G_HOLD, G_RELEASE, G_LONG } bm_gkind;

typedef void (*bm_gesture_cb)(void *user, int id, bm_gkind kind);

typedef struct {
    uint8_t down, held, combo, long_fired;
    uint8_t layer;      /* tiene capa de HOLD */
    uint8_t early;      /* el combo adelanta el HOLD */
    uint8_t instant;    /* manda PRESS/UP */
    uint8_t tap;        /* manda TAP si fue corto y sin combo */
    uint16_t hold_ms, long_ms;
    uint32_t t0;
} bm_gbtn;

typedef struct {
    bm_gbtn b[BTN_COUNT];
    bm_gesture_cb cb;
    void *user;
} bm_gesture;

void bm_gesture_init(bm_gesture *g, bm_gesture_cb cb, void *user);
void bm_gesture_down(bm_gesture *g, int id, uint32_t now);
void bm_gesture_up(bm_gesture *g, int id, uint32_t now);
void bm_gesture_poll(bm_gesture *g, uint32_t now);
void bm_gesture_other(bm_gesture *g, uint32_t now);
bool bm_gesture_is_down(const bm_gesture *g, int id);
/* Los 2 s de un botón (0 = sin LONG); solo se cambia con el botón arriba. */
void bm_gesture_set_long(bm_gesture *g, int id, uint16_t long_ms);
/* Progreso hacia los 2 s (0..255) del botón que se está sosteniendo, o -1. */
int  bm_gesture_progress(const bm_gesture *g, uint32_t now, int *id);

#endif
