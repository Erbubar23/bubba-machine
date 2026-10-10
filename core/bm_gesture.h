/* Bubba Machine — gramática de gestos de los botones (SPEC_V3 §6.2).
 *
 *  TAP     soltar antes del umbral (o botón sin capa) y sin combo -> al soltar
 *  HOLD    umbral vencido con el botón abajo -> al vencer (no al soltar)
 *  COMBO   otra tecla/encoder antes del umbral -> HOLD inmediato si el botón
 *          tiene capa; si no, solo cancela el TAP
 *  PRESS/UP botones instantáneos (PLAY, REC, OCT, GLO) -> al presionar/soltar
 */
#ifndef BM_GESTURE_H
#define BM_GESTURE_H

#include "bm_types.h"

typedef enum { G_PRESS, G_UP, G_TAP, G_HOLD, G_RELEASE } bm_gkind;

typedef void (*bm_gesture_cb)(void *user, int id, bm_gkind kind);

typedef struct {
    uint8_t down, held, combo;
    uint8_t layer;      /* tiene capa de HOLD */
    uint8_t early;      /* el combo adelanta el HOLD */
    uint8_t instant;    /* manda PRESS/UP */
    uint8_t tap;        /* además manda TAP si fue corto y sin combo */
    uint16_t hold_ms;
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
void bm_gesture_other(bm_gesture *g, uint32_t now);   /* se usó otro control */
bool bm_gesture_is_down(const bm_gesture *g, int id);

#endif
