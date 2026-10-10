/* Bubba Machine — máquina de estados de gestos. */
#include <string.h>
#include "bm_gesture.h"

static void cfg(bm_gesture *g, int id, int layer, int early, int instant, int tap, int ms)
{
    bm_gbtn *b = &g->b[id];
    b->layer = (uint8_t)layer; b->early = (uint8_t)early; b->instant = (uint8_t)instant;
    b->tap = (uint8_t)tap; b->hold_ms = (uint16_t)ms;
}

void bm_gesture_init(bm_gesture *g, bm_gesture_cb cb, void *user)
{
    memset(g, 0, sizeof *g);
    g->cb = cb; g->user = user;
    for (int i = 0; i < BTN_COUNT; i++) cfg(g, i, 0, 0, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_PLAY, 0, 0, 1, 0, UI_HOLD_MS);
    cfg(g, BTN_REC, 0, 0, 1, 0, UI_HOLD_MS);
    cfg(g, BTN_OCT_DN, 0, 0, 1, 0, UI_HOLD_MS);
    cfg(g, BTN_OCT_UP, 0, 0, 1, 0, UI_HOLD_MS);
    cfg(g, BTN_GLO, 0, 0, 1, 1, UI_HOLD_MS);          /* modificador + TAP = página global */
    cfg(g, BTN_SEQ, 1, 1, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_HOME, 1, 1, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_FX, 1, 1, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_SCL, 1, 1, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_ARP, 1, 1, 0, 1, UI_HOLD_MS);
    cfg(g, BTN_SAVE, 1, 0, 0, 1, UI_SAVE_HOLD_MS);    /* hold largo: guardar; el combo no lo adelanta */
}

bool bm_gesture_is_down(const bm_gesture *g, int id) { return id >= 0 && id < BTN_COUNT && g->b[id].down; }

void bm_gesture_down(bm_gesture *g, int id, uint32_t now)
{
    if (id < 0 || id >= BTN_COUNT) return;
    bm_gbtn *b = &g->b[id];
    if (b->down) return;
    /* este botón cuenta como "otro control" para los que ya estaban abajo */
    for (int i = 0; i < BTN_COUNT; i++)
        if (i != id && g->b[i].down && !g->b[i].held && !g->b[i].instant) {
            g->b[i].combo = 1;
            if (g->b[i].layer && g->b[i].early) { g->b[i].held = 1; g->cb(g->user, i, G_HOLD); }
        }
    for (int i = 0; i < BTN_COUNT; i++)
        if (i != id && g->b[i].down && g->b[i].instant) g->b[i].combo = 1;
    b->down = 1; b->held = 0; b->combo = 0; b->t0 = now;
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
        if (b->tap && !b->combo && now - b->t0 < UI_HOLD_MS) g->cb(g->user, id, G_TAP);
        return;
    }
    if (b->held) { g->cb(g->user, id, G_RELEASE); return; }
    if (b->combo) return;
    if (b->layer && now - b->t0 >= b->hold_ms) {          /* el poll aún no corrió: fue un HOLD */
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
        if (b->down && b->layer && !b->held && !b->combo && now - b->t0 >= b->hold_ms) {
            b->held = 1;
            g->cb(g->user, i, G_HOLD);
        }
    }
}

void bm_gesture_other(bm_gesture *g, uint32_t now)
{
    (void)now;
    for (int i = 0; i < BTN_COUNT; i++) {
        bm_gbtn *b = &g->b[i];
        if (!b->down || b->held) continue;
        b->combo = 1;
        if (b->layer && b->early) { b->held = 1; g->cb(g->user, i, G_HOLD); }
    }
}
