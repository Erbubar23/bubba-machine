/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — la máquina de un canal de notas por dentro (V_MACH), 2026-10-22.
 *
 * Se entra desde HOME con el cursor en el canal y un botón de menú: cada máquina dice qué botones
 * tiene (mach_page_of) y a qué página llevan. Adentro la máquina toma el FM-1 entero; HOME sale.
 * Todas tienen su piano roll (SEQ) con REC tipo SLOOP y sus menús, como el aparato de verdad: la BX-7
 * (core/bm_bx7.c), el BB-303 (core/bm_b303.c) y las de panel, B-MINI, BUNO-6 y BITTAL (core/bm_panel.c). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "bm_mach.h"
#include "bm_util.h"
#include "bm_params.h"
#include "bm_fm.h"
#include "bm_lang.h"

static const char *const ARPM[ARP_MODES] = {"OFF", "UP", "DOWN", "INCL", "EXCL", "RANDOM", "ORDER", "REPEAT"};
static const char *const ARPR[3] = {"1/8", "1/16", "1/32"};

const bxp MACH_SEQ[4] = {
    NONE, {"SPEED", K_TRK, TRK(speed), 0, SPD_COUNT - 1, F_LIST, bm_speed_names},
    {"PAGE", K_VIEWX, 0, 0, 3, F_P1, NULL}, {"SCROLL", K_VIEWY, 0, 16, 112, F_BIP, NULL}};   /* K3/K4: recorrer el piano roll */
/* el GLO de cada máquina: lo primero, el largo; después cambiar de máquina (solo las que se pueden) */
const bxp MACH_GLO[2][4] = {
    {{"LENGTH", K_LEN, 0, 1, BM_STEPS, F_STEPS, NULL}, {"MACHINE", K_MACH, 0, 0, ENG_COUNT - 1, F_LIST, bm_engine_names},
     {"MIDI CH", K_TRK, TRK(midi_ch), 0, 15, F_CH, NULL}, {"MIDI OUT", K_TRK, TRK(midi_out), 0, 1, F_ONOFF, NULL}},
    {NONE, NONE, MACH_QUANT, {"RESET", K_RST, 0, 0, 1, F_INT, NULL}}};
const char *const MACH_QUANTS[4] = {"25%", "50%", "75%", "100%"};
const bxp MACH_ARP[2][4] = {
    {{"MODE", K_TRK, TRK(arp_mode), 0, ARP_MODES - 1, F_LIST, ARPM}, {"OCTAVE", K_TRK, TRK(arp_oct), 1, 4, F_INT, NULL},
     {"RATE", K_TRK, TRK(arp_rate), 0, 2, F_LIST, ARPR}, {"GATE", K_TRK, TRK(arp_gate), 20, 100, F_RAWPCT, NULL}},   /* FM-1: 20-100 % */
    {{"LATCH", K_LATCH, 0, 0, 1, F_ONOFF, NULL}, NONE, NONE, NONE}};

static int op_byte(const bm_app *a, int field) { return (5 - a->bx.edit % 6) * BM_FM_OP + field; }   /* OPn está en el dato 6-n */
static int eng_of(const bm_app *a) { return a->pat.trk[a->track].engine; }
static int latch_prm(const bm_app *a) { return eng_of(a) == ENG_FM ? BX_LATCH : bm_panel_eng(eng_of(a)) ? PX_LATCH : AC_LATCH; }

static int fx_on(const bm_app *a, int slot)
{
    const uint8_t *p = a->pat.prm[a->track];
    return eng_of(a) == ENG_ACID ? bm_acid_fx_on(p, slot) : bm_px_eng(eng_of(a)) ? bm_px_fx_on(p, eng_of(a), slot) : (p[BX_FXON] >> slot) & 1;
}

int mach_get(const bm_app *a, const bxp *p)
{
    const bm_track *tr = &a->pat.trk[a->track];
    switch (p->kind) {
    case K_FMV: return a->pat.fmv[p->idx];
    case K_OP: return a->pat.fmv[op_byte(a, p->idx)];
    case K_TRK: return ((const uint8_t *)tr)[p->idx];
    case K_MAC: return tr->macro[p->idx & 3];
    case K_PRM: return a->pat.prm[a->track][p->idx];
    case K_LEN: return bm_clampi(tr->length ? tr->length : 16, 1, BM_STEPS);
    case K_ON: return fx_on(a, p->idx);
    case K_LATCH: return a->pat.prm[a->track][latch_prm(a)] != 0;
    case K_MACH: return app_mach_shown(a);
    case K_VIEWX: return mach_view_page(a);
    case K_VIEWY: return 64 + a->bx.vy;
    case K_QUANT: return 3 - (tr->qfree & 3);
    case K_PRMI: return p->max - a->pat.prm[a->track][p->idx];
    case K_R3: { int r = a->pat.prm[a->track][p->idx]; return r >= MN_R_LO ? 0 : r + 1; }
    default: return 0;
    }
}

void mach_fmt(const bxp *p, int v, char *b, int cap)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    switch (p->fmt) {
    case F_P1: snprintf(b, (size_t)cap, "%d", v + 1); break;
    case F_LIST: snprintf(b, (size_t)cap, "%s", p->list && v >= 0 && v <= p->max ? p->list[v] : "?"); break;
    case F_ONOFF: snprintf(b, (size_t)cap, "%s", v ? "ON" : "OFF"); break;
    case F_BIP: snprintf(b, (size_t)cap, v > 64 ? "+%d" : "%d", v - 64); break;      /* el snprintf del SDK no tiene %+d */
    case F_PCT: snprintf(b, (size_t)cap, "%d%%", (v * 100 + 63) / 127); break;
    case F_RAWPCT: snprintf(b, (size_t)cap, "%d%%", v); break;
    case F_TRSP: snprintf(b, (size_t)cap, v > 24 ? "+%d" : "%d", v - 24); break;
    case F_DT: snprintf(b, (size_t)cap, v > 7 ? "+%d" : "%d", v - 7); break;
    case F_HZ: { unsigned hz = (unsigned)(100.0f * powf(200.0f, (float)v / 127.0f)); if (hz < 1000) snprintf(b, (size_t)cap, "%u HZ", hz); else snprintf(b, (size_t)cap, "%u.%uK", hz / 1000, hz % 1000 / 100); break; }
    case F_NOTE: { int n = v + 21; snprintf(b, (size_t)cap, n / 12 - 2 < 0 ? "%s-%d" : "%s%d", NN[n % 12], n / 12 - 2 < 0 ? 2 - n / 12 : n / 12 - 2); break; }   /* DX7: 0 = A-1, 39 = C3 */
    case F_BARS: snprintf(b, (size_t)cap, v > 1 ? "%d BARS" : "%d BAR", v); break;
    case F_STEPS: snprintf(b, (size_t)cap, v > 1 ? "%d STEPS" : "%d STEP", v); break;
    case F_CH: snprintf(b, (size_t)cap, "%d", v + 1); break;
    case F_MIX:                                               /* B-MINI: a la izquierda el OSC 3, a la derecha el LFO */
        if (v <= 0) snprintf(b, (size_t)cap, "%s", "OSC 3");
        else if (v >= 127) snprintf(b, (size_t)cap, "%s", "LFO");
        else { int l = (v * 100 + 63) / 127; snprintf(b, (size_t)cap, "%d/%d", 100 - l, l); }
        break;
    case F_OHZ: case F_DMS: case F_GLIDE: case F_PHZ: case F_TIME: case F_RATE: case F_SEMI: case F_CENT: {   /* como en el editor */
        static const uint8_t PF[] = {PF_OHZ, PF_DMS, PF_GLIDE, PF_HZ, PF_TIME, PF_RATE, 0, PF_SEMI, PF_CENT};
        bm_pdef d = {p->name, 0, 0, p->max, PF[p->fmt - F_OHZ], NULL};
        bm_prm_fmt(&d, v, b, cap);
        break;
    }
    default: snprintf(b, (size_t)cap, "%d", v); break;
    }
}

int mach_pct(const bxp *p, int v) { return p->max > p->lo ? (v - p->lo) * 255 / (p->max - p->lo) : 0; }

int mach_turn(bm_app *a, const bxp *p, int d)
{
    int t = a->track;
    if (p->kind == K_NONE) return -1;
    if (p->kind == K_MACH) { app_mach_pick(a, d); return -1; }   /* la cuenta y el aviso: bm_input.c */
    if (p->kind == K_VIEWX) {                                 /* la página de 16 pasos que se ve */
        int np = (mach_len(a) + 15) / 16, v = bm_clampi(mach_view_page(a) + app_lst(a, d), 0, np - 1);
        a->bx.vman = 1; a->bx.vpage = (uint8_t)v;
        return v;
    }
    if (p->kind == K_VIEWY) {                                 /* las alturas que se ven, de a semitono */
        a->bx.vy = (int8_t)bm_clampi(a->bx.vy + app_lst(a, d), -48, 48);
        return 64 + a->bx.vy;
    }
    if (p->kind == K_RST) {                                   /* el sonido vuelve al del preset */
        app_push(a, CMD_PRESET, (uint8_t)t, a->pat.trk[t].source, 0);
        app_edited(a);
        app_toast(a, eng_of(a) == ENG_FM ? "VOICE RESET" : "SOUND RESET");
        return -1;
    }
    int cur_v = mach_get(a, p), step = p->max - p->lo <= 16 ? app_lst(a, d) : d;
    if (p->kind == K_ON || p->kind == K_LATCH) step = d > 0 ? 1 : -1;
    int v = bm_clampi(cur_v + step, p->lo, p->max);
    if (v == cur_v) return -1;
    switch (p->kind) {
    case K_FMV: app_push(a, CMD_FMV, p->idx, (uint8_t)v, 0); app_edited(a); break;
    case K_OP: app_push(a, CMD_FMV, (uint8_t)op_byte(a, p->idx), (uint8_t)v, 0); app_edited(a); break;
    case K_TRK:
        app_set_track(a, t, p->idx, v);
        if (p->idx == TRK(arp_mode) && !v) app_push(a, CMD_ARP, (uint8_t)t, 0, 0);   /* sin arpegiador: suelta lo que tenía */
        break;
    case K_MAC: app_set_track(a, t, (uint8_t)(offsetof(bm_track, macro) + (p->idx & 3)), v); break;
    case K_PRM: app_push(a, CMD_SET_PRM, (uint8_t)t, p->idx, (uint8_t)v); app_edited(a); break;
    case K_LATCH: app_push(a, CMD_SET_PRM, (uint8_t)t, (uint8_t)latch_prm(a), (uint8_t)v); app_edited(a); break;
    case K_LEN: app_set_track(a, t, TRK(length), v); break;
    case K_QUANT: app_set_track(a, t, TRK(qfree), 3 - v); break;
    case K_PRMI: app_push(a, CMD_SET_PRM, (uint8_t)t, p->idx, (uint8_t)(p->max - v)); app_edited(a); break;
    case K_R3: app_push(a, CMD_SET_PRM, (uint8_t)t, p->idx, (uint8_t)(v ? v - 1 : MN_R_LO)); app_edited(a); break;
    case K_ON: {
        uint8_t m;
        if (eng_of(a) == ENG_ACID) {
            m = bm_acid_fx_set(a->pat.prm[t][AC_FX], p->idx, v);
            app_push(a, CMD_SET_PRM, (uint8_t)t, AC_FX, m);
        } else if (bm_px_eng(eng_of(a))) {                  /* bit 0 DISTORTION prendida, bit 1 REVERB apagada */
            uint8_t bit = p->idx == PXF_DIST ? 1u : 2u;
            m = a->pat.prm[t][PX_FX];
            m = (uint8_t)((p->idx == PXF_DIST) == !!v ? m | bit : m & ~bit);
            app_push(a, CMD_SET_PRM, (uint8_t)t, PX_FX, m);
        } else {
            m = a->pat.prm[t][BX_FXON];
            m = (uint8_t)(v ? m | (1u << p->idx) : m & ~(1u << p->idx));
            app_push(a, CMD_SET_PRM, (uint8_t)t, BX_FXON, m);
        }
        app_edited(a);
        break;
    }
    default: break;
    }
    return v;
}

void mach_legend(const bm_app *a, const bxp *P, bm_legend L[4])
{
    char b[12];
    for (int k = 0; k < 4; k++) {
        if (P[k].kind == K_NONE) continue;
        int v = mach_get(a, &P[k]);
        if (P[k].kind == K_RST) snprintf(b, sizeof b, "%s", "TURN");
        else mach_fmt(&P[k], v, b, sizeof b);
        app_layer_legend(a, L, k, P[k].name, b, mach_pct(&P[k], v));
    }
}

bool mach_oct(bm_app *a, int id, bm_gkind k)
{
    char b[30];
    if (id != BTN_OCT_DN && id != BTN_OCT_UP) return false;
    int other = id == BTN_OCT_UP ? BTN_OCT_DN : BTN_OCT_UP;
    if (k == G_PRESS) {
        a->oct_used = 0;
        if (bm_gesture_is_down(&a->ges, other)) {             /* los dos: todo a cero, como el FM-1 */
            a->oct = 0;
            a->oct_used = 1;
            app_set_track(a, a->track, TRK(transpose), 24);
            app_toast(a, "OCTAVE 0  TRANSPOSE 0");
        }
        return true;
    }
    if (k != G_UP || a->oct_used) return true;                /* al soltar; sostenido con ALGORITHM era transposición */
    a->oct_used = 1;
    a->oct = (int8_t)bm_clampi(a->oct + (id == BTN_OCT_UP ? 1 : -1), -3, 3);
    snprintf(b, sizeof b, a->oct > 0 ? TF("OCTAVE +%d") : TF("OCTAVE %d"), a->oct);
    app_toast(a, b);
    return true;
}

void mach_rec(bm_app *a, bm_gkind k)
{
    int t = a->track;
    char b[30];
    if (k == G_LONG) {                                        /* 2 s: borra las notas del canal */
        if (a->rec) app_rec_set(a, 0);
        a->bx.armed = 0;
        app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
        app_edited(a);
        snprintf(b, sizeof b, TF("T%d CLEARED"), app_pos(t) + 1);
        app_toast(a, b);
        return;
    }
    if (k != G_TAP) return;
    if (a->rec) { app_rec_set(a, 0); a->bx.armed = 0; return; }
    if (a->bx.armed) { a->bx.armed = 0; app_toast(a, "REC OFF"); return; }
    if (a->ctx.st.playing) app_rec_set(a, 1);
    else if (a->bx.page == BXP_SEQ) {                         /* parado en el SEQ: paso a paso, desde el paso del cursor */
        a->bx.chord = 0;
        if (a->bx.step >= mach_len(a)) a->bx.step = 0;
        app_step_rec(a);
    } else { a->bx.armed = 1; app_toast(a, "REC ARMED: PLAY A NOTE"); }
}

int mach_len(const bm_app *a)
{
    int L = a->pat.trk[a->track].length;
    return L > 0 && L <= BM_STEPS ? L : 16;
}

/* la página de 16 pasos que muestra el piano roll: la que se eligió con K3, si no la del cabezal
 * (sonando) o la del cursor (parado) */
int mach_view_page(const bm_app *a)
{
    int L = mach_len(a), np = (L + 15) / 16;
    if (a->bx.vman) return a->bx.vpage < np ? a->bx.vpage : np - 1;
    int s = a->ctx.st.playing ? bm_app_track_step(a, a->track, -1) : a->bx.step;
    if (s < 0) s = 0;
    return (s % L) / 16;
}

bool mach_step_mode(const bm_app *a) { return a->rec && !a->ctx.st.playing && a->view == V_MACH && a->bx.page == BXP_SEQ; }

/* Paso a paso: la primera tecla reescribe el paso del cursor (lo de antes se borra), las que se suman
 * mientras está apretada hacen un acorde; al soltarlas todas, el cursor pasa al siguiente. Cada nota,
 * un paso de largo (los largos se graban sonando). */
static void step_write(bm_app *a, uint8_t note, int vel)
{
    bm_bx7ui *u = &a->bx;
    int t = a->track, s = u->step % mach_len(a);
    bm_event ev;
    if (!u->chord) {
        bm_event_clear(&ev);
        for (int e = 0; e < BM_EVENTS; e++) app_push_ev(a, t, s, e, &ev);
        u->chord = 1; u->nslot = 0;
    }
    if (u->nslot >= BM_EVENTS) return;
    bm_event_clear(&ev);
    ev.note = note; ev.len = 1;
    ev.vel = (uint8_t)bm_clampi(vel, 1, BM_VEL_ACC - 1);      /* el ACCENT del BB-303 se pone con SCL */
    app_push_ev(a, t, s, u->nslot++, &ev);
    a->rec_steps |= 1ull << s;
    app_edited(a);
}

void mach_key_up(bm_app *a, int k)
{
    bm_bx7ui *u = &a->bx;
    if (!u->chord) return;
    for (int k2 = 0; k2 < BM_KEYS; k2++) if (k2 != k && a->keys_down[k2]) return;   /* el acorde sigue apretado */
    u->chord = 0;
    u->step = (uint8_t)((u->step + 1) % mach_len(a));
}

void mach_cursor(bm_app *a, int d)
{
    int L = mach_len(a);
    a->bx.chord = 0; a->bx.vman = 0;                          /* el roll vuelve a seguir al cursor */
    a->bx.step = (uint8_t)(((a->bx.step % L) + d % L + L) % L);
}

int mach_play(bm_app *a, int k, int vel)
{
    int t = a->track;
    uint8_t n = (uint8_t)bm_clampi(BM_KEY_BASE_NOTE + k + 12 * a->oct, 0, 127);
    if (mach_step_mode(a)) {                                  /* REC parado en el SEQ: escribe en el paso del cursor */
        app_play_notes(a, k, t, &n, 1, vel);
        step_write(a, n, vel);
        return 1;
    }
    if (a->bx.armed && !a->ctx.st.playing) {                  /* armado: la primera nota arranca y graba */
        a->bx.armed = 0;
        app_push(a, CMD_PLAY, 1, 0, 0);
        app_rec_set(a, 1);
        app_play_notes(a, k, t, &n, 1, vel);
        if (a->pat.trk[t].arp_mode) return 0;
        app_rec_at(a, k, t, &n, 1, 0);
        return 1;
    }
    app_play_notes(a, k, t, &n, 1, vel);
    return a->key_rec_step[k] >= 0;
}

uint8_t mach_common_led(const bm_app *a, int i)
{
    if (i == BTN_HOME) return LED_DIM;
    if (i == BTN_REC) return a->rec ? LED_ON : a->bx.armed ? LED_BREATH : LED_DIM;
    if (i == BTN_OCT_DN) return a->oct < 0 ? LED_ON : LED_DIM;
    if (i == BTN_OCT_UP) return a->oct > 0 ? LED_ON : LED_DIM;
    return 0xFF;
}

void mach_head(bm_app *a) { a->bx.grab = 0; a->bx.armed = 0; a->bx.chord = 0; }

/* ------------------------------------------------------------ el reparto según la máquina del canal */
bool mach_has(int engine) { return engine == ENG_FM || engine == ENG_ACID || bm_panel_eng(engine) || engine == ENG_BREAK; }

int mach_page_of(int engine, int btn)
{
    switch (engine) {
    case ENG_FM: return bx7_page_of(btn);
    case ENG_ACID: return b303_page_of(btn);
    case ENG_BREAK: return slicer_page_of(btn);
    default: return bm_panel_eng(engine) ? panel_page_of(btn) : -1;
    }
}

void mach_enter(bm_app *a, int t, int page)
{
    app_sloop(a, 0);
    a->track = (uint8_t)t;
    a->bx.page = (uint8_t)(page >= 0 && page < BXP_N ? page : BXP_MAIN);
    a->bx.vman = 0; a->bx.vy = 0;
    mach_head(a);
    app_set_view(a, V_MACH);
}

bool mach_button(bm_app *a, int id, bm_gkind k)
{
    switch (id) {
    case BTN_PLAY: case BTN_SAVE: return false;
    case BTN_HOME:                                            /* sale a la consola: la grabación termina */
        if (k == G_TAP) { if (a->rec) app_rec_set(a, 0); mach_head(a); }
        return false;
    default: break;
    }
    if (eng_of(a) == ENG_BREAK) return slicer_button(a, id, k);   /* el Slicer: REC graba pads, OCT nada */
    if (id == BTN_REC) { mach_rec(a, k); return true; }
    if (mach_oct(a, id, k)) return true;
    switch (eng_of(a)) {
    case ENG_FM: return bx7_button(a, id, k);
    case ENG_ACID: return b303_button(a, id, k);
    default: return panel_button(a, id, k);
    }
}

void mach_key(bm_app *a, int k, int w, int b)
{
    switch (eng_of(a)) {
    case ENG_FM: bx7_key(a, k, w, b); return;
    case ENG_ACID: b303_key(a, k, w, b); return;
    case ENG_BREAK: slicer_key(a, k, w, b); return;
    default: panel_key(a, k, w, b); return;
    }
}

void mach_encoder(bm_app *a, int id, int d)
{
    if (eng_of(a) == ENG_BREAK) { slicer_encoder(a, id, d); return; }
    if (a->bx.page == BXP_SEQ && id == ENC_SELECT) { mach_cursor(a, app_lst(a, d)); return; }   /* SEQ: SELECT mueve el cursor de paso */
    switch (eng_of(a)) {
    case ENG_FM: bx7_encoder(a, id, d); return;
    case ENG_ACID: b303_encoder(a, id, d); return;
    default: panel_encoder(a, id, d); return;
    }
}

void mach_legend_view(const bm_app *a, bm_legend L[4])
{
    switch (eng_of(a)) {
    case ENG_FM: bx7_legend(a, L); return;
    case ENG_ACID: b303_legend(a, L); return;
    case ENG_BREAK: slicer_legend(a, L); return;
    default: panel_legend(a, L); return;
    }
}

void mach_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    switch (eng_of(a)) {
    case ENG_FM: bx7_bigknobs(a, sel, alg, pre); return;
    case ENG_ACID: b303_bigknobs(a, sel, alg, pre); return;
    case ENG_BREAK: slicer_bigknobs(a, sel, alg, pre); return;
    default: panel_bigknobs(a, sel, alg, pre); return;
    }
}

void mach_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    if (eng_of(a) == ENG_FM) { bx7_keys(a, K); return; }
    if (eng_of(a) == ENG_BREAK) { slicer_keys(a, K); return; }
    if (a->bx.page == BXP_SCL) { b303_keys(a, K); return; }   /* BB-303 y las de panel: los pasos */
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    app_note_keys(a, K, a->track);
}

uint8_t mach_btn_led(const bm_app *a, int i)
{
    if (eng_of(a) == ENG_BREAK) return slicer_btn_led(a, i);
    uint8_t c = mach_common_led(a, i);
    if (c != 0xFF) return c;
    switch (eng_of(a)) {
    case ENG_FM: return bx7_btn_led(a, i);
    case ENG_ACID: return b303_btn_led(a, i);
    default: return panel_btn_led(a, i);
    }
}

const char *mach_title(const bm_app *a, char *buf, int cap)
{
    switch (eng_of(a)) {
    case ENG_FM: return bx7_title(a, buf, cap);
    case ENG_ACID: return b303_title(a, buf, cap);
    case ENG_BREAK: return slicer_title(a, buf, cap);
    default: return panel_title(a, buf, cap);
    }
}

/* ¿la pantalla de la máquina ya muestra K1-K4 (su panel, o la lista de GLO, ARP y LFO)? Entonces no va
 * la franja de perillas de abajo: cada perilla se dibuja una sola vez. */
bool mach_knobs_shown(const bm_app *a)
{
    int p = a->bx.page;
    switch (eng_of(a)) {
    case ENG_FM: return p == BXP_GLO || p == BXP_ARP;
    case ENG_BREAK: return slicer_knobs_shown(a);
    default: return p == BXP_MAIN || p == BXP_LFO || p == BXP_GLO || p == BXP_ARP;   /* BB-303 y las de panel */
    }
}
