/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — máquina de estados de gestos. */
#include <string.h>
#include "bm_gesture.h"

static void cfg(bm_gesture *g, int id, int layer, int early, int instant, int tap, int long_ms)
{
    bm_gbtn *b = &g->b[id];
    b->layer = (uint8_t)layer; b->early = (uint8_t)early; b->instant = (uint8_t)instant;
    b->tap = (uint8_t)tap; b->hold_ms = UI_HOLD_MS; b->long_ms = (uint16_t)long_ms;
}

void bm_gesture_init(bm_gesture *g, bm_gesture_cb cb, void *user)
{
    memset(g, 0, sizeof *g);
    g->cb = cb; g->user = user;
    /* 1.0: solo SCL, EDIT, ENV y LFO tienen capa (dentro del secuenciador de la batería; REPETIR va en
     * la negra 11). Los demás son de un toque, sin importar cuánto dure. */
    for (int i = 0; i < BTN_COUNT; i++) cfg(g, i, 0, 0, 0, 1, 0);
    cfg(g, BTN_SCL, 1, 1, 0, 1, 0);                   /* capa MUTE (silenciar sonidos) */
    cfg(g, BTN_EDIT, 1, 1, 0, 1, 0);                  /* capa INSTRUMENTO (modelo) */
    cfg(g, BTN_ENV, 1, 1, 0, 1, 0);                   /* capa SONIDO (golpe) */
    cfg(g, BTN_LFO, 1, 1, 0, 1, 0);                   /* capa ACENTOS */
    cfg(g, BTN_PLAY, 0, 0, 1, 0, 0);
    cfg(g, BTN_OCT_DN, 0, 0, 1, 0, 0);
    cfg(g, BTN_OCT_UP, 0, 0, 1, 0, 0);
    cfg(g, BTN_HOME, 0, 0, 0, 1, UI_LONG_MS);         /* 2 s: SISTEMA */
    cfg(g, BTN_SAVE, 0, 0, 0, 1, UI_LONG_MS);         /* 2 s: guardado rápido */
    cfg(g, BTN_REC, 0, 0, 0, 1, UI_LONG_MS);          /* 2 s en la BX-7: borra las notas del canal */
}

void bm_gesture_set_long(bm_gesture *g, int id, uint16_t long_ms)
{
    if (id >= 0 && id < BTN_COUNT && !g->b[id].down) g->b[id].long_ms = long_ms;
}

bool bm_gesture_is_down(const bm_gesture *g, int id) { return id >= 0 && id < BTN_COUNT && g->b[id].down; }

static void mark_combo(bm_gesture *g, int except)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        bm_gbtn *b = &g->b[i];
        if (i == except || !b->down) continue;
        b->combo = 1;
        if (!b->held && !b->instant && b->layer && b->early) { b->held = 1; g->cb(g->user, i, G_HOLD); }
    }
}

void bm_gesture_down(bm_gesture *g, int id, uint32_t now)
{
    if (id < 0 || id >= BTN_COUNT) return;
    bm_gbtn *b = &g->b[id];
    if (b->down) return;
    mark_combo(g, id);
    b->down = 1; b->held = 0; b->combo = 0; b->long_fired = 0; b->t0 = now;
    if (b->instant) g->cb(g->user, id, G_PRESS);
}

void bm_gesture_up(bm_gesture *g, int id, uint32_t now)
{
    if (id < 0 || id >= BTN_COUNT) return;
    bm_gbtn *b = &g->b[id];
    if (!b->down) return;
    b->down = 0;
    if (b->instant) {
        g->cb(g->user, id, G_UP);
        if (b->tap && !b->combo && !b->long_fired && now - b->t0 < UI_HOLD_MS) g->cb(g->user, id, G_TAP);
        return;
    }
    if (b->long_fired) return;
    if (b->held) { g->cb(g->user, id, G_RELEASE); return; }
    if (b->combo) return;
    if (b->layer && now - b->t0 >= b->hold_ms) {      /* el poll no alcanzó a correr */
        g->cb(g->user, id, G_HOLD);
        g->cb(g->user, id, G_RELEASE);
        return;
    }
    g->cb(g->user, id, G_TAP);
}

void bm_gesture_poll(bm_gesture *g, uint32_t now)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        bm_gbtn *b = &g->b[i];
        if (!b->down) continue;
        uint32_t el = now - b->t0;
        if (b->layer && !b->held && !b->combo && el >= b->hold_ms) { b->held = 1; g->cb(g->user, i, G_HOLD); }
        if (b->long_ms && !b->combo && !b->long_fired && el >= b->long_ms) { b->long_fired = 1; g->cb(g->user, i, G_LONG); }
    }
}

void bm_gesture_other(bm_gesture *g, uint32_t now)
{
    (void)now;
    mark_combo(g, -1);
}

int bm_gesture_progress(const bm_gesture *g, uint32_t now, int *id)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        const bm_gbtn *b = &g->b[i];
        if (!b->down || !b->long_ms || b->combo || b->long_fired) continue;
        uint32_t el = now - b->t0;
        if (el < UI_HOLD_MS) continue;
        *id = i;
        uint32_t p = (el - UI_HOLD_MS) * 255u / (b->long_ms - UI_HOLD_MS);
        return (int)(p > 255 ? 255 : p);
    }
    return -1;
}
