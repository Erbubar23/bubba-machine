/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — entrada (DISENO_3.0): cuatro gestos por botón, HOME en todas partes,
 * ALGORITHM que navega, teclas por página y capas, perillas y su leyenda. */
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include "bm_app_int.h"
#include "bm_util.h"
#include "bm_rom.h"
#include "bm_params.h"
#include "bm_drums.h"
#include "bm_snap.h"
#include "bm_fm.h"

static const char *const MAC_MINI[4] = {"CUTOFF", "EMPHASIS", "OSC TUNE", "CONTOUR"};
static const char *const MAC_BUNO[4] = {"FREQ", "RES", "ENV", "PWM"};
static const char *const MAC_ACID[4] = {"CUTOFF", "RESO", "DECAY", "ENV MOD"};
static const char *const MAC_FM[4] = {"BRIGHT", "FEEDBACK", "DECAY", "ALG"};   /* "ALG": no es la perilla ALGORITHM */
static const char *const MAC_TR[4] = {"TONE", "TUNE", "DECAY", "COLOR"};
static const char *const GEN_NAMES[4] = {"VARY", "ANCHOR", "REPEAT", "BREAK"};
static const char *const MAC_PAD[4] = {"FILTER", "PITCH", "DECAY", "START"};
static const char *const DLY_NAMES[4] = {"1/16", "1/8", "3/16", "1/4"};
static const char *const EXT_NAMES[5] = {"TRIAD", "+7", "+9", "SUS2", "SUS4"};
static const char *const ARP_MODES[5] = {"OFF", "UP", "DOWN", "UP-DN", "RAND"};
static const char *const ARP_RATES[3] = {"1/8", "1/16", "1/32"};
static const char *const VOICINGS[3] = {"CLOSED", "OPEN", "LED"};
static const char *const RAT_NAMES[5] = {"X1", "X2", "X3", "X4", "FLAM"};
static const char *const COND_NAMES[3] = {"ALWAYS", "FILL ONLY", "NO FILL"};
static const char *const SNAP_NAMES[3] = {"OFF", "1/16", "1/8"};
static const char *const PUNCH_NAMES[16] = {"S4", "S8", "S16", "S32", "R8", "R4", "TAPE", "GATE", "LPF", "HPF", "BIT", "SR", "DLY", "ECHO", "K-LO", "K-HI"};

enum { ROLE_NONE, ROLE_MUTE, ROLE_FILL, ROLE_PAT, ROLE_SCENE, ROLE_BRKFX, ROLE_LMUTE, ROLE_NOTE, ROLE_FILLK, ROLE_PAD, ROLE_PREVIEW, ROLE_CMOD };

const char *bm_macro_name(const bm_app *a, int t, int i)
{
    if (t == 1) return MAC_PAD[i & 3];
    if (bm_is_tr(&a->pat, t)) return MAC_TR[i & 3];
    if (a->pat.trk[t].engine == ENG_ACID) return MAC_ACID[i & 3];
    if (a->pat.trk[t].engine == ENG_FM) return MAC_FM[i & 3];
    if (a->pat.trk[t].engine == ENG_BUNO) return MAC_BUNO[i & 3];
    if (a->pat.trk[t].engine == ENG_BITTAL) {                    /* la macro dice a dónde va */
        int dst = a->pat.prm[t][BT_M1D + 2 * (i & 3)];
        return dst ? bm_btm_names[dst % BTM_TARGETS] : "MACRO";
    }
    return MAC_MINI[i & 3];
}

/* ------------------------------------------------------------ formato */
static void fmt_ms(uint32_t ms, char *b, int cap)
{
    if (ms >= 1000) snprintf(b, (size_t)cap, "%u.%uS", (unsigned)(ms / 1000), (unsigned)((ms % 1000) / 100));
    else snprintf(b, (size_t)cap, "%uMS", (unsigned)ms);
}

static void fmt_hz(uint32_t hz, char *b, int cap)
{
    if (hz >= 1000) snprintf(b, (size_t)cap, "%u.%02uK", (unsigned)(hz / 1000), (unsigned)((hz % 1000) / 10));
    else snprintf(b, (size_t)cap, "%uHZ", (unsigned)hz);
}

static void fmt_pct(int v, char *b, int cap) { snprintf(b, (size_t)cap, "%d%%", v * 100 / 127); }

static void fmt_pan(int v, char *b, int cap)
{
    if (v == 64) snprintf(b, (size_t)cap, "C");
    else if (v < 64) snprintf(b, (size_t)cap, "L%d", (64 - v) * 50 / 64);
    else snprintf(b, (size_t)cap, "R%d", (v - 64) * 50 / 63);
}

static void fmt_master(int m, char *b, int cap)
{
    if (!m) snprintf(b, (size_t)cap, "INF");
    else snprintf(b, (size_t)cap, "%d", m * 16);
}

static void fmt_semi(int v, char *b, int cap) { snprintf(b, (size_t)cap, "%+d ST", v - 24); }

void bm_fmt_macro(const bm_app *a, int t, int i, int v, char *b, int cap)
{
    if (t == 1) {                                                /* pads: FILTER PITCH DECAY START */
        if (i == 1) snprintf(b, (size_t)cap, "%+d ST", (v - 64) * 12 / 63);
        else if ((i == 0 || i == 2) && v >= 127) snprintf(b, (size_t)cap, "OFF");
        else fmt_pct(v, b, cap);
        return;
    }
    if (bm_is_tr(&a->pat, t)) {                                  /* batería: 64 = el kit tal cual */
        if (i == 1) snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 3) / 8);
        else snprintf(b, (size_t)cap, "%+d", v - 64);
        return;
    }
    if (bm_engine_paged(a->pat.trk[t].engine)) {                 /* la definición de la macro en sus páginas */
        int n;
        const bm_ppage *pg = bm_engine_pages(a->pat.trk[t].engine, &n);
        for (int p = 0; p < n; p++)
            for (int k = 0; k < pg[p].n; k++)
                if (pg[p].p[k].slot == (PS_M0 + (i & 3))) { bm_prm_fmt(&pg[p].p[k], v, b, cap); return; }
        fmt_pct(v, b, cap);
        return;
    }
    if (a->pat.trk[t].engine == ENG_FM) {
        const bm_track *tk = &a->pat.trk[t];
        if (i == 0) snprintf(b, (size_t)cap, "%+d", (v - 64) / 2);
        else if (i == 1) snprintf(b, (size_t)cap, "FB %d", bm_fm_fb_of(tk, v));
        else if (i == 2) snprintf(b, (size_t)cap, "%+d", (v - 64) / 3);
        else snprintf(b, (size_t)cap, v == 64 ? "ALG %d *" : "ALG %d", bm_fm_alg_of(tk, v));
        return;
    }
    switch (i) {
    case 0: fmt_hz(bm_cutoff_hz(v), b, cap); break;
    case 1: fmt_pct(v, b, cap); break;
    case 2: fmt_ms(bm_decay_ms(v), b, cap); break;
    default: fmt_pct(v, b, cap); break;
    }
}

static bool lock_mode(const bm_app *a) { return a->held_step >= 0 && a->held_long; }
static int tr_lane(const bm_app *a) { return a->sel_lane[a->track] & 15; }
static int seq_page(const bm_app *a) { return a->view == V_CH && a->page == PG_SEQ; }

/* Perillas: `d` llega acelerado (para 0..127 y el BPM); las listas, los largos y los pasos usan los
 * detents tal cual (lst): uno por detent, ninguno se pierde ni se salta. Fuera de un giro (o si el
 * signo no coincide) vale ±1. */
static int lst(const bm_app *a, int d)
{
    int r = a->enc_raw;
    if (!r || (r > 0) != (d > 0)) return d > 0 ? 1 : -1;
    return r;
}

static int wrap(int v, int n) { return n > 0 ? ((v % n) + n) % n : 0; }

/* ¿una nota larga que empezó antes sigue sonando en el paso s? */
static int note_covers(const bm_app *a, int t, int s)
{
    for (int b = 1; b < BM_STEPS && b <= s; b++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, s - b, k);
            if (!bm_event_empty(e) && (int)(e->len & BM_LEN_MASK) > b) return 1;
        }
    return 0;
}

static int syn_step_events(const bm_app *a, int t, int s, int idx[4])
{
    int n = 0;
    for (int k = 0; k < BM_EVENTS; k++)
        if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) idx[n++] = k;
    return n;
}

/* ------------------------------------------------------------ filas de MAS */
enum { ROW_ENGINE, ROW_ACC, ROW_LEN, ROW_SPEED, ROW_TRANSP, ROW_MIDIOUT, ROW_MIDICH, ROW_INTERP, ROW_INIT };

static int more_rows(const bm_app *a, int t, uint8_t out[10])
{
    int n = 0;
    if (t == 2) out[n++] = ROW_ENGINE;
    out[n++] = ROW_ACC; out[n++] = ROW_LEN; out[n++] = ROW_SPEED;
    if (app_kind(a, t) == KIND_NOTE) { out[n++] = ROW_TRANSP; out[n++] = ROW_MIDIOUT; }
    if (a->pat.trk[t].midi_out) out[n++] = ROW_MIDICH;
    if (t == 1) out[n++] = ROW_INTERP;
    out[n++] = ROW_INIT;
    return n;
}

static const char *const ROW_NAMES[] = {"MACHINE", "ACCENT", "LENGTH", "SPEED", "TRANSPOSE", "MIDI OUT", "MIDI CHANNEL", "INTERPOLATION", "INIT SOUND"};

void bm_app_more_row(const bm_app *a, int i, const char **name, char *val, int cap)
{
    uint8_t rows[10];
    int n = more_rows(a, a->track, rows), t = a->track;
    const bm_track *tk = &a->pat.trk[t];
    val[0] = 0;
    if (i < 0 || i >= n) { *name = ""; return; }
    *name = ROW_NAMES[rows[i]];
    switch (rows[i]) {
    case ROW_ENGINE: snprintf(val, (size_t)cap, "%s", bm_engine_names[tk->engine % ENG_COUNT]); break;
    case ROW_ACC: fmt_pct(tk->accent, val, cap); break;
    case ROW_LEN: snprintf(val, (size_t)cap, TF("%d STEPS"), bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? 0 : -1)); break;
    case ROW_SPEED: snprintf(val, (size_t)cap, "%s", bm_speed_names[tk->speed % SPD_COUNT]); break;
    case ROW_TRANSP: fmt_semi(tk->transpose, val, cap); break;
    case ROW_MIDICH: snprintf(val, (size_t)cap, "%d", tk->midi_ch + 1); break;
    case ROW_MIDIOUT: snprintf(val, (size_t)cap, "%s", tk->midi_out ? "ON" : "OFF"); break;
    case ROW_INTERP: snprintf(val, (size_t)cap, "%s", tk->interp ? "HERMITE" : "LINEAR"); break;
    default: snprintf(val, (size_t)cap, "TURN = YES"); break;
    }
}

int bm_app_more_count(const bm_app *a) { uint8_t r[10]; return more_rows(a, a->track, r); }

/* ------------------------------------------------------------ leyenda de las perillas */
static void L4(bm_legend *L, int i, const char *name, const char *val, int pct)
{
    snprintf(L[i].name, 10, "%s", name);
    snprintf(L[i].val, 10, "%s", val);
    L[i].pct = (uint8_t)bm_clampi(pct, 0, 255);
}

static void legend_global(const bm_app *a, bm_legend *L)
{
    const bm_meta *m = &a->pat.meta;
    char b[12];
    snprintf(b, sizeof b, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10); L4(L, 0, "BPM", b, (m->bpm_x10 - 400) * 255 / 2000);
    snprintf(b, sizeof b, "%d%%", m->swing); L4(L, 1, "SWING", b, (m->swing - 50) * 255 / 25);
    fmt_master(m->master, b, sizeof b); L4(L, 2, "MASTER", b, m->master ? m->master * 31 : 255);
    fmt_pct(a->set.drive, b, sizeof b); L4(L, 3, "DRIVE", b, a->set.drive * 2);
}

static void legend_mix(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    if (a->mix_page && a->view == V_CH) {
        if (t >= 1) { fmt_pct(tr->duck, b, sizeof b); L4(L, 0, "DUCK", b, tr->duck * 2); } else L4(L, 0, "DUCK", "SOURCE", 0);
        return;
    }
    snprintf(b, sizeof b, "%d", tr->level); L4(L, 0, "LEVEL", b, tr->level * 2);
    fmt_pan(tr->pan, b, sizeof b); L4(L, 1, "PAN", b, tr->pan * 2);
    fmt_pct(tr->send, b, sizeof b); L4(L, 2, "DELAY", b, tr->send * 2);
    fmt_pct(tr->rsend, b, sizeof b); L4(L, 3, "REVERB", b, tr->rsend * 2);
}

/* HOME (BX-1): el canal del cursor y el master */
static const char *const BEATFX_N[17] = {"OFF", "STUT 4", "STUT 8", "STUT 16", "STUT 32", "REV 8", "REV 4", "TAPE", "GATE",
                                         "LPF", "HPF", "BIT", "SR", "THROW", "FREEZE", "KILL LO", "KILL HI"};
static void legend_home(const bm_app *a, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[a->track];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    (void)m;
    int p = !tr->djf || tr->djf == 64 ? 0 : tr->djf < 64 ? -(int)tr->djf : (int)tr->djf - 64;
    snprintf(b, sizeof b, "%d", tr->level); L4(L, 0, "LEVEL", b, tr->level * 2);
    if (!p) snprintf(b, sizeof b, "OPEN"); else snprintf(b, sizeof b, "%s %d", p < 0 ? "LP" : "HP", p < 0 ? -p : p);
    L4(L, 1, "FILTER", b, (p + 64) * 2);
    fmt_pct(tr->send, b, sizeof b); L4(L, 2, "DELAY", b, tr->send * 2);
    fmt_pct(tr->rsend, b, sizeof b); L4(L, 3, "REVERB", b, tr->rsend * 2);
}

static void legend_fx(const bm_app *a, bm_legend *L)
{
    const bm_meta *m = &a->pat.meta;
    char b[12];
    if (a->fx_page == 2 && a->layer != L_PUNCH) {
        if (m->mfilt >= 60 && m->mfilt <= 68) snprintf(b, sizeof b, "OPEN");
        else if (m->mfilt < 60) snprintf(b, sizeof b, "LP %d", m->mfilt);
        else snprintf(b, sizeof b, "HP %d", m->mfilt - 64);
        L4(L, 0, "MST FILT", b, m->mfilt * 2);
        L4(L, 1, "BEAT FX", BEATFX_N[m->beatfx % 17], m->beatfx * 15);
        fmt_pct(a->set.drive, b, sizeof b); L4(L, 2, "DRIVE", b, a->set.drive * 2);
        return;
    }
    if (a->fx_page == 1 && a->layer != L_PUNCH) {
        fmt_pct(m->rv_size, b, sizeof b); L4(L, 0, "SIZE", b, m->rv_size * 2);
        fmt_pct(m->rv_damp, b, sizeof b); L4(L, 1, "DAMP", b, m->rv_damp * 2);
        fmt_pct(m->rv_mix, b, sizeof b); L4(L, 2, "REV MIX", b, m->rv_mix * 2);
        fmt_ms(bm_decay_ms(m->sc_release / 2), b, sizeof b); L4(L, 3, "SC REL", b, m->sc_release * 2);
        return;
    }
    L4(L, 0, "DLY TIME", DLY_NAMES[m->dly_time & 3], (m->dly_time & 3) * 85);
    fmt_pct(m->dly_fb, b, sizeof b); L4(L, 1, "FEEDBACK", b, m->dly_fb * 2);
    fmt_pct(m->dly_mix, b, sizeof b); L4(L, 2, "DLY MIX", b, m->dly_mix * 2);
    fmt_ms(bm_decay_ms(m->sc_release / 2), b, sizeof b); L4(L, 3, "SC REL", b, m->sc_release * 2);
}

static void legend_macros(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    for (int i = 0; i < 4; i++) { bm_fmt_macro(a, t, i, tr->macro[i], b, sizeof b); L4(L, i, bm_macro_name(a, t, i), b, tr->macro[i] * 2); }
}

static void legend_midi(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    snprintf(b, sizeof b, "%d", tr->midi_ch + 1); L4(L, 0, "CHANNEL", b, tr->midi_ch * 17);
    snprintf(b, sizeof b, "%d", tr->macro[1] + 1); L4(L, 1, "PROG", b, tr->macro[1] * 2);
    snprintf(b, sizeof b, "%d", tr->macro[2]); L4(L, 2, "CC1 MOD", b, tr->macro[2] * 2);
    snprintf(b, sizeof b, "%d", tr->macro[3]); L4(L, 3, "CC74", b, tr->macro[3] * 2);
}

static void legend_chords(const bm_app *a, bm_legend *L)
{
    const bm_meta *m = &a->pat.meta;
    char b[12];
    snprintf(b, sizeof b, "%.8s", bm_chord_set_names[m->chord_set % BM_CHORD_SETS]); L4(L, 0, "SET", b, m->chord_set * 127);
    snprintf(b, sizeof b, "%.8s", VOICINGS[m->chord_voicing % 3]); L4(L, 1, "VOICING", b, m->chord_voicing * 127);
    snprintf(b, sizeof b, "%d", m->chord_oct); L4(L, 2, "OCTAVE", b, (m->chord_oct - 1) * 51);
    snprintf(b, sizeof b, "%dMS", m->strum); L4(L, 3, "STRUM", b, m->strum * 4);
}

static void legend_arp(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    L4(L, 0, "MODE", ARP_MODES[tr->arp_mode % 5], tr->arp_mode * 63);
    L4(L, 1, "RATE", ARP_RATES[tr->arp_rate % 3], tr->arp_rate * 127);
    snprintf(b, sizeof b, "%d", tr->arp_oct); L4(L, 2, "OCTAVES", b, tr->arp_oct * 85);
    snprintf(b, sizeof b, "%d%%", tr->arp_gate); L4(L, 3, "GATE", b, tr->arp_gate * 255 / 100);
}

static void legend_len(const bm_app *a, bm_legend *L)
{
    int t = a->track;
    char b[12];
    int len = bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? tr_lane(a) : -1);
    snprintf(b, sizeof b, "%d", len); L4(L, 0, "LENGTH", b, len * 4);
    L4(L, 1, "SPEED", bm_speed_names[a->pat.trk[t].speed % SPD_COUNT], (SPD_COUNT - 1 - a->pat.trk[t].speed) * 51);
}

uint8_t bm_app_takes_sel(const bm_app *a)
{
    uint8_t list[BM_TAKES + 8];
    int n = bm_app_sources(a, list);
    if (!n) return BM_SRC_NONE;
    return list[a->takes_row % n];
}

/* ------------------------------------------------------------ editor de sonido por páginas
 * B-MINI, BITTAL y BUNO-6: los ajustes de la máquina (bm_params.h) en SONIDO. Batería: el instrumento elegido
 * (bm_lane_pages) en INSTR. K1-K4 = los 4 de la página; SELECT recorre las páginas y sigue a la
 * pestaña de al lado al llegar al borde; el botón de la página otra vez = la siguiente. */
const bm_ppage *bm_app_ed_pages(const bm_app *a, int t, int *n)
{
    if (bm_is_tr(&a->pat, t)) { *n = BM_LANE_PAGES; return bm_lane_pages; }
    return bm_engine_pages(a->pat.trk[t].engine, n);
}

int bm_app_ed_on(const bm_app *a)
{
    int n, t = a->track;
    if (a->view != V_CH || !bm_app_ed_pages(a, t, &n)) return 0;
    return bm_is_tr(&a->pat, t) ? a->page == PG_INST : a->page == PG_SOUND;
}

int bm_app_ed_value(const bm_app *a, int t, const bm_pdef *d)
{
    if (bm_is_tr(&a->pat, t)) return ((const uint8_t *)&bm_trk_trc(&a->pat, t)->lane[tr_lane(a)])[d->slot & 7];
    return bm_prm_get(&a->pat, t, d);
}

const char *bm_app_ed_name(const bm_app *a, int t, const bm_pdef *d)
{
    if (bm_is_tr(&a->pat, t) && d->slot == LN_COLOR)                 /* COLOR dice qué hace en ese modelo */
        return bm_drm_color[bm_trk_trc(&a->pat, t)->lane[tr_lane(a)].model % DRM_COUNT];
    return d->name;
}

static void ed_set(bm_app *a, int t, const bm_pdef *d, int v)
{
    if (bm_is_tr(&a->pat, t)) { app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)tr_lane(a), d->slot & 7, v); app_edited(a); return; }
    if (d->slot & PS_M0) { app_set_track(a, t, d->slot & 3, v); return; }
    app_push(a, CMD_SET_PRM, (uint8_t)t, d->slot, (uint8_t)v);
    app_edited(a);
}

static void ed_turn(bm_app *a, int t, int k, int d)
{
    int n;
    const bm_ppage *pg = bm_app_ed_pages(a, t, &n);
    if (!pg || !n) return;
    const bm_ppage *P = &pg[a->ed_page[t] % n];
    if (k >= P->n) return;
    const bm_pdef *def = &P->p[k];
    int cur = bm_app_ed_value(a, t, def);
    int step = def->max <= 16 ? lst(a, d) : d;                /* listas: de a una */
    int lo = bm_is_tr(&a->pat, t) && def->slot == LN_LEN ? 1 : 0;
    int nv = bm_clampi(cur + step, lo, def->max);
    if (nv == cur) return;
    ed_set(a, t, def, nv);
    char b[12], ti[22];
    bm_prm_fmt(def, nv, b, sizeof b);
    if (bm_is_tr(&a->pat, t)) snprintf(ti, sizeof ti, "%s %s", T(bm_lane_name(a, t, tr_lane(a))), T(bm_app_ed_name(a, t, def)));
    else snprintf(ti, sizeof ti, "%s %s", T(P->name), T(def->name));
    app_overlay(a, k, ti, b, "", nv * 255 / (def->max ? def->max : 1), def->fmt == PF_BIP || def->fmt == PF_SEMI || def->fmt == PF_DSEMI || def->fmt == PF_CENT ? 128 : -1, bm_knob_color(k));
}


static void legend_editor(const bm_app *a, int t, bm_legend *L)
{
    int n;
    const bm_ppage *pg = bm_app_ed_pages(a, t, &n);
    if (!pg || !n) return;
    const bm_ppage *P = &pg[a->ed_page[t] % n];
    char b[12];
    for (int k = 0; k < P->n; k++) {
        int v = bm_app_ed_value(a, t, &P->p[k]);
        bm_prm_fmt(&P->p[k], v, b, sizeof b);
        L4(L, k, bm_app_ed_name(a, t, &P->p[k]), b, v * 255 / (P->p[k].max ? P->p[k].max : 1));
    }
}

/* PADS: TRIM (START LENGTH ZOOM SNAP) · SOUND (PITCH LEVEL PLAY DIR) · ENV (ATTACK RELEASE CHOKE TEMPO) */
static void legend_pads(const bm_app *a, bm_legend *L)
{
    const bm_pad *p = &a->pat.pad[a->sel_lane[1] & 15];
    const bm_lane *ln = &a->pat.tr[1].lane[a->sel_lane[1] & 15];
    char b[12];
    bool ok = bm_pad_ok(p);
    switch (a->ed_page[1] % 4) {
    case 0: {
        unsigned st = (unsigned)((uint32_t)p->start * 1000u / BM_SMP_RATE), ln_ms = (unsigned)((uint32_t)p->len * 1000u / BM_SMP_RATE);
        snprintf(b, sizeof b, "%u.%03uS", st / 1000, st % 1000); L4(L, 0, "START", ok ? b : "-", ok && a->cut_wlen ? (int)((uint32_t)p->start * 255u / a->cut_wlen) : 0);
        snprintf(b, sizeof b, "%u MS", ln_ms); L4(L, 1, "LENGTH", ok ? b : "-", ok && a->cut_wlen ? (int)((uint32_t)p->len * 255u / a->cut_wlen) : 0);
        snprintf(b, sizeof b, "X%d", 1 << (a->cut_zoom & 3)); L4(L, 2, "ZOOM", b, a->cut_zoom * 85);
        L4(L, 3, "SNAP", SNAP_NAMES[a->cut_snap % 3], a->cut_snap * 127);
        break;
    }
    case 1:
        snprintf(b, sizeof b, "%+d ST", p->pitch - 24); L4(L, 0, "PITCH", b, p->pitch * 255 / 48);
        snprintf(b, sizeof b, "%d", ln->level); L4(L, 1, "LEVEL", b, ln->level * 2);
        L4(L, 2, "PLAY", bm_pad_mode_name(p->mode), p->mode * 85);
        L4(L, 3, "DIR", (p->flags & PAD_REV) ? "REV" : "FWD", (p->flags & PAD_REV) ? 255 : 0);
        break;
    case 3:
        snprintf(b, sizeof b, "%d%%", p->loop_s * 100 / 255); L4(L, 0, "LOOP IN", ok ? b : "-", p->loop_s);
        if (p->loop_e) snprintf(b, sizeof b, "%d%%", p->loop_e * 100 / 255); else snprintf(b, sizeof b, "WHOLE");
        L4(L, 1, "LOOP OUT", ok ? b : "-", p->loop_e);
        L4(L, 2, "PLAY", bm_pad_mode_name(p->mode), p->mode * 85);
        break;
    default:
        fmt_ms(bm_attack_ms(p->attack), b, sizeof b); L4(L, 0, "ATTACK", b, p->attack * 2);
        fmt_ms(bm_decay_ms(p->release), b, sizeof b); L4(L, 1, "RELEASE", b, p->release * 2);
        if (ln->choke) snprintf(b, sizeof b, "%d", ln->choke); else snprintf(b, sizeof b, "OFF");
        L4(L, 2, "CHOKE", b, ln->choke * 85);
        L4(L, 3, "TEMPO", (p->flags & PAD_TEMPO) ? "SYNC" : "OFF", (p->flags & PAD_TEMPO) ? 255 : 0);
        break;
    }
}

static void legend_view(const bm_app *a, bm_legend *L);

void bm_app_legend(const bm_app *a, bm_legend L[4])
{
    memset(L, 0, sizeof(bm_legend) * 4);
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    switch (a->layer) {
    case L_HOME: legend_global(a, L); return;
    case L_TONE:                                               /* K1 BPM y K2 SWING, como en HOME y en TEMPO */
        snprintf(b, sizeof b, "%d", (m->bpm_x10 + 5) / 10); L4(L, 0, "BPM", b, (m->bpm_x10 - 400) * 255 / 2000);
        snprintf(b, sizeof b, "%d%%", m->swing); L4(L, 1, "SWING", b, (m->swing - 50) * 255 / 25);
        fmt_semi(m->transpose, b, sizeof b); L4(L, 2, "TRANSP", b, m->transpose * 5);
        snprintf(b, sizeof b, "%+d", ((int)m->transpose - 24) / 12); L4(L, 3, "OCTAVE", b, m->transpose * 5);
        return;
    case L_LEN: legend_len(a, L); return;
    case L_ACC: fmt_pct(tr->accent, b, sizeof b); L4(L, 0, "ACCENT", b, tr->accent * 2); return;
    case L_REP: if (app_kind(a, t) == KIND_NOTE) { fmt_ms((uint32_t)tr->glide * 500 / 127, b, sizeof b); L4(L, 0, "GLIDE", b, tr->glide * 2); } return;
    case L_PUNCH: legend_fx(a, L); return;
    case L_CHORDS: legend_chords(a, L); return;
    case L_ARP: legend_arp(a, t, L); return;
    case L_ROLL: L4(L, 0, "RATE", bm_roll_names[a->roll_rate % 5], a->roll_rate * 63); return;
    case L_COPY: snprintf(b, sizeof b, "A%02d", a->ctx.st.pattern + 1); L4(L, 0, "FROM", b, 0); return;
    case L_ERASE: return;
    default: break;                                            /* MUTE y FILL: las perillas siguen con lo de SEQ */
    }
    legend_view(a, L);
    if (a->layer == L_FILL) {
        static const char *const FE[4] = {"OFF", "EVERY 2", "EVERY 4", "EVERY 8"};
        int fe = a->pat.meta.fill_every == 2 ? 1 : a->pat.meta.fill_every == 4 ? 2 : a->pat.meta.fill_every == 8 ? 3 : 0;
        L4(L, 0, "AUTO FILL", FE[fe], fe * 85);
        L[0].locked = 0;
    }
}

static void legend_view(const bm_app *a, bm_legend *L)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    switch (a->view) {
    case V_HOME: legend_home(a, L); return;
    case V_SCENES: case V_GLO: legend_global(a, L); return;
    case V_MIX: legend_mix(a, t, L); return;
    case V_FXG: legend_fx(a, L); return;
    case V_SCLG:
        L4(L, 0, "ROOT", bm_note_names[m->root % 12], m->root * 23);
        snprintf(b, sizeof b, "%.8s", bm_scale_names[m->scale % BM_SCALES]); L4(L, 1, "SCALE", b, m->scale * 31);
        L4(L, 2, "CHORD", EXT_NAMES[m->voicing % 5], m->voicing * 63);
        L4(L, 3, "LOCK", m->scale_lock ? "ON" : "OFF", m->scale_lock ? 255 : 0);
        return;
    case V_TIME: {
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        if (bm_is_grid(&a->pat, row)) {
            const bm_tr *g = bm_trk_trc(&a->pat, row);
            snprintf(b, sizeof b, "%d", g->lane[0].len); L4(L, 0, "LENGTH", b, g->lane[0].len * 4);
            L4(L, 1, "SPEED", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
            if (row != 1) {
                L4(L, 2, "INSTRUM", bm_lane_name(a, row, a->time_lane), a->time_lane * 17);
                snprintf(b, sizeof b, "%d", g->lane[a->time_lane].len); L4(L, 3, "LANE LEN", b, g->lane[a->time_lane].len * 4);
            }
        } else {
            snprintf(b, sizeof b, "%d", rt->length); L4(L, 0, "LENGTH", b, rt->length * 4);
            L4(L, 1, "SPEED", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
        }
        return;
    }
    case V_SAVE: snprintf(b, sizeof b, "A%02d", a->save_target + 1); L4(L, 0, "TARGET", b, a->save_target * 17); return;
    case V_SYSTEM: L4(L, 0, "VALUE", "ALGO", 0); return;
    case V_SONG:
        if (a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            if (e & 0x80) snprintf(b, sizeof b, TF("SCN %c"), 'A' + (e & 3)); else snprintf(b, sizeof b, "A%02d", e + 1);
            L4(L, 0, "STEP", b, 0);
            snprintf(b, sizeof b, "x%d", a->song.rep[a->song_cur]); L4(L, 1, "TIMES", b, 0);
        }
        L4(L, 3, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? 255 : 0);
        return;
    case V_CH: break;
    default: return;
    }
    if (lock_mode(a) && a->page == PG_SEQ) {
        for (int i = 0; i < 4; i++) {
            int val = tr->macro[i];
            if (bm_is_grid(&a->pat, t)) {
                int lv = bm_tr_lock_get(bm_trk_trc(&a->pat, t), a->held_step, tr_lane(a), i);
                if (lv >= 0) { val = lv; L[i].locked = 1; }
            } else {
                int idx[4];
                int n = syn_step_events(a, t, a->held_step, idx);
                if (n && bm_evc(&a->pat, t, a->held_step, idx[0])->lk[i] != BM_LOCK_NONE) { val = bm_evc(&a->pat, t, a->held_step, idx[0])->lk[i]; L[i].locked = 1; }
            }
            bm_fmt_macro(a, t, i, val, b, sizeof b);
            L4(L, i, bm_macro_name(a, t, i), b, val * 2);
        }
        return;
    }
    int kind = app_kind(a, t);
    switch (a->page) {
    case PG_SEQ:
        if (app_midi(a, t)) legend_midi(a, t, L);
        else legend_macros(a, t, L);
        return;
    case PG_SOUND:
        if (kind == KIND_TR) legend_macros(a, t, L);
        else if (bm_engine_paged(tr->engine)) legend_editor(a, t, L);
        else {                                                 /* BX-7 y BB-303: K1-K3 como en SEQ, K4 = ACCENT */
            legend_macros(a, t, L);
            fmt_pct(tr->accent, b, sizeof b); L4(L, 3, "ACCENT", b, tr->accent * 2);
        }
        return;
    case PG_INST: legend_editor(a, t, L); return;
    case PG_MIX: legend_mix(a, t, L); return;
    case PG_MORE: {
        bm_legend tmp[4];
        memset(tmp, 0, sizeof tmp);
        legend_len(a, tmp);                                    /* K1 LENGTH, K2 SPEED como en la capa de SEQ y en TIME */
        L[0] = tmp[0]; L[1] = tmp[1];
        fmt_pct(tr->accent, b, sizeof b); L4(L, 2, "ACCENT", b, tr->accent * 2);
        if (kind == KIND_NOTE) { fmt_semi(tr->transpose, b, sizeof b); L4(L, 3, "TRANSP", b, tr->transpose * 5); }
        return;
    }
    case PG_BREAK:
        for (int i = 0; i < 4; i++) { fmt_pct(tr->gen[i], b, sizeof b); L4(L, i, GEN_NAMES[i], b, tr->gen[i] * 2); }
        return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (src != BM_SRC_NONE && !(src & BM_SRC_ROM)) {
            const bm_take *tk = &a->take[src];
            snprintf(b, sizeof b, "%d", (tk->bpm_x10 + 5) / 10); L4(L, 0, "BPM", b, (tk->bpm_x10 - 400) * 255 / 2000);
            snprintf(b, sizeof b, "%d", tk->bars); L4(L, 1, "BARS", b, tk->bars * 31);
            int db = ((int)tk->gain - 64) * 6 / 64;
            snprintf(b, sizeof b, "%+d DB", db); L4(L, 2, "GAIN", b, tk->gain * 2);
        } else if (src != BM_SRC_NONE) {
            const bm_rom_loop *l = &bm_rom_loops[src & 0x7F];
            snprintf(b, sizeof b, "%d", l->bpm_x10 / 10); L4(L, 0, "BPM", b, 0);
            snprintf(b, sizeof b, "%d", l->bars); L4(L, 1, "BARS", b, 0);
            L4(L, 2, "FACTORY", "FIXED", 0);
        }
        return;
    }
    case PG_ENV:
        if (a->env_page == 0) {
            fmt_ms(bm_attack_ms(tr->attack), b, sizeof b); L4(L, 0, "ATTACK", b, tr->attack * 2);
            fmt_ms(bm_decay_ms(tr->macro[2]), b, sizeof b); L4(L, 1, "DECAY", b, tr->macro[2] * 2);
            fmt_pct(tr->sustain, b, sizeof b); L4(L, 2, "SUSTAIN", b, tr->sustain * 2);
            fmt_ms(bm_decay_ms(tr->release), b, sizeof b); L4(L, 3, "RELEASE", b, tr->release * 2);
        } else {
            fmt_pct(tr->env_amt, b, sizeof b); L4(L, 0, "ENV>FILT", b, tr->env_amt * 2);
            fmt_ms((uint32_t)tr->glide * 500 / 127, b, sizeof b); L4(L, 1, "GLIDE", b, tr->glide * 2);
        }
        return;
    case PG_ARP: if (kind == KIND_NOTE) legend_arp(a, t, L); else L4(L, 0, "ROLL", bm_roll_names[a->roll_rate % 5], a->roll_rate * 63); return;
    case PG_LOOP: {
        const bm_loop *lp = &a->pat.loop[t >= 2 ? t - 2 : 0];
        L4(L, 0, "LOOP", lp->on ? "ON" : "OFF", lp->on ? 255 : 0);
        snprintf(b, sizeof b, "%d", lp->bars); L4(L, 1, "BARS", b, lp->bars * 63);
        return;
    }
    case PG_MIDI: legend_midi(a, t, L); return;
    case PG_SCL: {
        int root, sc = app_scale(a, t, &root);
        L4(L, 0, "ROOT", bm_note_names[root], root * 23);
        if (tr->scl == 0xFF) snprintf(b, sizeof b, "PROJECT"); else snprintf(b, sizeof b, "%.8s", bm_scale_names[sc]);
        L4(L, 1, "SCALE", b, tr->scl == 0xFF ? 0 : (sc + 1) * 25);
        L4(L, 2, "CHORD", EXT_NAMES[m->voicing % 5], m->voicing * 63);
        L4(L, 3, "LOCK", m->scale_lock ? "ON" : "OFF", m->scale_lock ? 255 : 0);
        return;
    }
    case PG_CHORDS: legend_chords(a, L); return;
    case PG_REC: L4(L, 0, "START", a->rec_now ? "NOW" : "BAR", a->rec_now ? 255 : 0); L4(L, 1, "SOURCE", "MIX", 0); return;
    case PG_PADS: legend_pads(a, L); return;
    default: return;
    }
}

/* ------------------------------------------------------------ las tres perillas grandes
 * El mismo reparto que enc_dispatch, en palabras cortas para la fila de abajo de la pantalla. */
void bm_app_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre)
{
    const char *S = "-", *A = "-", *P = "-";
    int t = a->track, kind = app_kind(a, t), e = a->pat.trk[t].engine;
    switch (a->layer) {
    case L_HOME: S = "TRACK"; A = "BPM .1"; P = "PATTERN"; goto out;
    case L_NONE: case L_LANEMUTE: case L_FILL: break;          /* MUTE y FILL: siguen con lo de SEQ */
    default: goto out;                                         /* las otras capas solo usan K1-K4 */
    }
    if (a->view == V_CH && a->page == PG_SEQ && a->rec && kind == KIND_NOTE && a->held_step < 0) { S = "STEP"; A = "GATE"; P = "MICRO"; goto out; }
    if (a->view == V_CH && a->page == PG_SEQ && a->held_step >= 0) {
        S = "ACCENT";
        if (bm_is_grid(&a->pat, t)) { A = "REPEAT"; P = "COND"; } else { A = "PROB"; P = "LENGTH"; }
        goto out;
    }
    switch (a->view) {
    case V_HOME: S = "TRACK"; A = "BPM"; P = "MOVE"; break;
    case V_SCENES: A = "BPM"; P = "MOVE"; break;
    case V_MIX: S = "TRACK"; A = "BPM"; P = "PATTERN"; break;
    case V_FXG: S = "PAGE"; A = "BPM"; P = "MOVE"; break;
    case V_GLO: S = "ROW"; A = "VALUE"; P = "MOVE"; break;
    case V_SONG: if (a->song.n) { S = "ROW"; A = "ENTRY"; } P = "MOVE"; break;
    case V_SYSTEM: S = "ROW"; A = "VALUE"; break;
    case V_SAVE: S = "ROW"; A = "TARGET"; P = "TARGET"; break;
    case V_TIME: S = "TRACK"; A = "TRACK"; break;
    case V_COPYCH: S = "TARGET"; A = "TARGET"; break;
    case V_CH:
        S = a->page == PG_MORE ? "ROW" : "PAGE";
        if (a->page == PG_MORE) A = "VALUE";
        else if (a->page == PG_TAKES) A = "TAKE";
        else if (a->page != PG_REC)
            switch (e) {
            case ENG_DRUM: A = "INSTR"; break;
            case ENG_BREAK: A = "PAD"; break;
            case ENG_ACID: case ENG_MINI: A = "MACHINE"; break;
            case ENG_FM: A = "ALG"; break;
            case ENG_BITTAL: A = "TABLE"; break;
            case ENG_BUNO: A = "CHORUS"; break;
            default: break;
            }
        P = bm_is_pads(t) ? "TAKE" : kind == KIND_TR ? "KIT" : e == ENG_ACID ? "WAVE" : e == ENG_FM ? "PATCH" : "SOUND";
        break;
    default: break;
    }
out:
    *sel = S; *alg = A; *pre = P;
}

/* ------------------------------------------------------------ mapa del teclado (capas, HOME)
 * Las luces son blancas y solo dicen el estado; la tira de teclas de la pantalla nombra cada tecla
 * con la misma intensidad. Una sola gramática en todas las pantallas:
 *   OFF    no hay nada: sin función, hueco vacío que no hace nada, fuera del largo
 *   DIM    disponible pero quieto o vacío, o con contenido pero callado (muteado, NO-FILL)
 *   MID    tiene contenido y va a sonar; una referencia (carril con pasos, patrón guardado, tónica)
 *   ON     elegido, sostenido, activo ahora, o el destello de un golpe (live)
 *   BLINK  solo el cabezal o lo que está en cola
 *   BREATH armado, fijo o especial (paso con p-lock, la salida de una capa fija) */
static bm_keyfn *KW(bm_keyfn *K, int w, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_white_key(w)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
    return k;
}

static bm_keyfn *KB(bm_keyfn *K, int b, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_black_key(b)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
    return k;
}

/* La tira muestra 3 letras: cada instrumento de los kits tiene su nombre corto fijo (KICK y KICK2,
 * o los tres TOM, no pueden leerse iguales). Si se le cambió el modelo, el del modelo. */
static const char KIT_SHORT[BM_KITS_N][BM_LANES][4] = {
    {"BD", "BD2", "CH", "OH", "SD", "CP", "RS", "LT", "MT", "HT", "CB", "CY", "SH", "CL", "CG", "ZAP"},
    {"BD", "BD2", "CH", "OH", "SD", "CP", "RS", "LT", "MT", "HT", "RD", "CR", "SH", "CL", "CG", "ZAP"},
    {"BD", "SUB", "CH", "OH", "SD", "CP", "BLP", "ZAP", "MTL", "BLK", "CB", "NZ", "SH", "TOM", "PRC", "BD3"},
};
static const char DRM_SHORT[DRM_COUNT][4] = {"BD", "BD", "SD", "SD", "HH", "MTL", "CP", "TOM", "CB", "RES", "NZ", "FM"};

static const char *lane_short(const bm_app *a, int t, int ln)
{
    ln &= 15;
    if (!bm_is_tr(&a->pat, t)) return bm_lane_name(a, t, ln);          /* pads P1..P16 */
    int kit = (a->pat.trk[t].source % BM_KITS_N + BM_KITS_N) % BM_KITS_N;
    int m = bm_trk_trc(&a->pat, t)->lane[ln].model;
    const char *s = (m == bm_kits[kit].lane[ln].model || m >= DRM_COUNT) ? KIT_SHORT[kit][ln] : DRM_SHORT[m];
    return s[0] ? s : bm_lane_name(a, t, ln);
}

/* el carril tiene algún golpe dentro de su largo */
static bool lane_has(const bm_tr *tr, int ln)
{
    int len = tr->lane[ln].len ? tr->lane[ln].len : 16;
    for (int s = 0; s < len && s < BM_STEPS; s++)
        if (tr->st[ln][s] & ST_ON) return true;
    return false;
}

/* un instrumento en un selector: el elegido ON, con pasos MID (muteado DIM), vacío OFF */
static uint8_t lane_led(const bm_tr *tr, int ln, int sel)
{
    if (ln == sel) return LED_ON;
    if (ln < 0 || ln >= BM_LANES || !lane_has(tr, ln)) return LED_OFF;
    return ((tr->mute >> ln) & 1) ? LED_DIM : LED_MID;
}

/* el instrumento acaba de sonar (los muteados no suenan, así que no destellan) */
static uint8_t lane_live(const bm_app *a, int t, int ln)
{
    ln &= 15;
    return (uint8_t)(a->now < a->lane_flash[t][ln] && !((bm_trk_trc(&a->pat, t)->mute >> ln) & 1));
}

/* Las negras de un canal de batería (SEQ y las demás páginas): 1-8 eligen y tocan un instrumento
 * del banco, 9 cambia de banco (HI/LO), 10 MUTE, 11 FILL. Las pantallas MUTE y FILL usan 1-9. */
static void tr_blacks(const bm_app *a, bm_keyfn *K, int t, bool all)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a), bank = (lane / 8) * 8;
    bool other = false;
    for (int i = 0; i < 8; i++) {
        int ln = bank + i;
        bm_keyfn *k = KB(K, i, lane_short(a, t, ln), lane_led(tr, ln, lane), bm_track_color(t));
        k->live = (uint8_t)(lane_has(tr, ln) && lane_live(a, t, ln));
        if (!other) other = lane_has(tr, (ln + 8) & 15);
    }
    KB(K, 8, lane < 8 ? "HI" : "LO", lane >= 8 ? LED_ON : (other ? LED_MID : LED_DIM), C_WHITE);
    if (!all) return;
    KB(K, 9, "MUTE", LED_DIM, C_RED);
    KB(K, 10, "FIL", a->fill ? LED_ON : LED_DIM, C_ORANGE);
}

/* Las negras de HOME (y de ESCENAS): 1-6 los canales (sonando MID y destellan con cada golpe,
 * muteados DIM), 7-10 las escenas A-D, 11 FILL. */
static void home_blacks(const bm_app *a, bm_keyfn *K)
{
    const bm_status *st = &a->ctx.st;
    char b[6];
    for (int t = 0; t < BM_TRACKS; t++) {
        bool mute = a->pat.trk[t].mute != 0;
        snprintf(b, sizeof b, "T%d", t + 1);
        KB(K, t, b, mute ? LED_DIM : LED_MID, bm_track_color(t))->live = (uint8_t)(!mute && a->now < a->trig_until[t]);
    }
    for (int s = 0; s < 4; s++) {
        snprintf(b, sizeof b, "%c", 'A' + s);
        KB(K, 6 + s, b, st->scene == s ? LED_ON : (st->scene_q == s ? LED_BLINK : (a->song.scene[s].valid ? LED_MID : LED_OFF)), C_CYAN);
    }
    KB(K, 10, "FIL", a->fill ? LED_ON : LED_DIM, C_ORANGE);
}

static void map_home(const bm_app *a, bm_keyfn *K)
{
    const bm_status *st = &a->ctx.st;
    char b[6];
    for (int w = 0; w < 16; w++) {
        bool has = bm_store_has(&a->store, (uint8_t)w) || w == st->pattern || a->pend_obj == w;
        uint8_t led = w == st->pattern ? LED_ON : (w == st->queued ? LED_BLINK : (has ? LED_MID : LED_DIM));
        snprintf(b, sizeof b, "%02d", w + 1);
        KW(K, w, b, led, w == st->pattern ? C_WHITE : w == st->queued ? C_CYAN : 0);
    }
    home_blacks(a, K);
}

static void map_steps(const bm_app *a, bm_keyfn *K, int mode)
{
    /* mode 0: acento · 1: repetir/ligar · 2: borrar · 3: solo en el fill
     * Un paso vacío está disponible (DIM) donde la capa lo crea: acento, y en batería repetir y fill. */
    int t = a->track, kind = app_kind(a, t);
    uint8_t empty = (mode == 0 || (kind != KIND_NOTE && (mode == 1 || mode == 3))) ? LED_DIM : LED_OFF;
    for (int w = 0; w < 16; w++) {
        int s = a->step_page * 16 + w;
        const char *l = "";
        uint8_t led = empty;
        if (kind == KIND_NOTE) {
            if (s >= a->pat.trk[t].length) { KW(K, w, "", LED_OFF, 0); continue; }
            int idx[4], n = syn_step_events(a, t, s, idx);
            if (n) {
                const bm_event *e = bm_evc(&a->pat, t, s, idx[0]);
                if (mode == 3) {
                    l = e->prob == BM_PROB_FILL ? "F" : e->prob == BM_PROB_NOFILL ? "-" : "";
                    led = e->prob == BM_PROB_FILL ? LED_ON : e->prob == BM_PROB_NOFILL ? LED_DIM : LED_MID;
                }
                else if (mode == 0) { l = e->vel >= BM_VEL_ACC ? "A" : e->vel <= BM_VEL_GHOST ? "G" : ""; led = e->vel >= BM_VEL_ACC ? LED_ON : LED_MID; }
                else if (mode == 1) { l = (e->len & BM_LEN_TIE) ? "TIE" : ""; led = (e->len & BM_LEN_TIE) ? LED_ON : LED_MID; }
                else { l = "X"; led = LED_MID; }
            }
        } else {
            const bm_tr *tr = bm_trk_trc(&a->pat, t);
            int lane = tr_lane(a);
            if (s >= tr->lane[lane].len) { KW(K, w, "", LED_OFF, 0); continue; }
            uint8_t v = tr->st[lane][s];
            int on = (v & ST_ON) != 0;
            if (on) {
                int rat = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
                int cond = (v & ST_COND_MASK) >> ST_COND_SH;
                if (mode == 3) { l = cond == 1 ? "F" : cond == 2 ? "-" : ""; led = cond == 1 ? LED_ON : cond == 2 ? LED_DIM : LED_MID; }
                else if (mode == 0) { l = (v & ST_ACC) ? "A" : ""; led = (v & ST_ACC) ? LED_ON : LED_MID; }
                else if (mode == 1) { l = RAT_NAMES[rat]; if (!rat) l = ""; led = rat ? LED_ON : LED_MID; }
                else { l = "X"; led = LED_MID; }
            }
        }
        if (s == bm_app_track_step(a, t, kind == KIND_NOTE ? -1 : tr_lane(a))) led = LED_BLINK;
        KW(K, w, l, led, mode == 0 ? C_WHITE : mode == 1 ? C_YELLOW : mode == 3 ? C_ORANGE : C_RED);
    }
}

/* Las blancas como los 16 instrumentos. act: los sostenidos (ON).
 * MUTE: con pasos y sonando MID y destella con cada golpe, muteado DIM, vacío OFF.
 * ERASE: con pasos MID (muteado o no), vacío OFF (no hay nada que borrar).
 * ROLL: todos suenan: vacío DIM, con pasos MID, y destellan. */
enum { LM_MUTE, LM_ERASE, LM_ROLL };

static void map_lanes(const bm_app *a, bm_keyfn *K, int t, uint16_t act, int mode)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int w = 0; w < 16; w++) {
        bool has = lane_has(tr, w), mute = (tr->mute >> w) & 1;
        uint8_t led, live = 0;
        if (mode == LM_MUTE) { led = has ? (mute ? LED_DIM : LED_MID) : LED_OFF; live = (uint8_t)(has && lane_live(a, t, w)); }
        else if (mode == LM_ERASE) led = has ? LED_MID : LED_OFF;
        else { led = has ? LED_MID : LED_DIM; live = lane_live(a, t, w); }
        if ((act >> w) & 1) led = LED_ON;
        KW(K, w, lane_short(a, t, w), led, bm_track_color(t))->live = live;
    }
}

static void map_chords(const bm_app *a, bm_keyfn *K)
{
    static const uint16_t FN[3] = {C_BLUE, C_GREEN, C_ORANGE};
    const bm_meta *m = &a->pat.meta;
    int root, sc = app_scale(a, bm_app_chord_track(a), &root);
    for (int p = 0; p < 16; p++) {
        bm_pad_chord pc = bm_chord_pad(m->chord_set, p);
        char rn[10];
        bm_roman(root, sc, pc.degree, pc.ext, rn, sizeof rn);
        rn[4] = 0;
        KW(K, p, rn, p == a->last_pad ? LED_ON : LED_DIM, FN[bm_chord_function(pc.degree)]);
    }
    static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
    for (int b = 0; b < 4; b++) KB(K, b, MODS[b], (a->chord_mod >> b) & 1 ? LED_ON : LED_DIM, C_YELLOW);
}

/* Las pistas del visor de cada capa: la línea 1 dice qué hacen teclas y perillas, la 2 empieza
 * siempre por cómo se sale. CONTROL = VERBO OBJETO, dos espacios entre pares, hasta 38 letras, los
 * controles con el nombre impreso en el equipo. */
void bm_app_keymap(const bm_app *a, bm_keyfn K[BM_KEYS], char *title, int cap, uint16_t *color, const char **h1, const char **h2)
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    int t = a->track, kind = app_kind(a, t);
    const bm_meta *m = &a->pat.meta;
    char b[8];
    *h1 = ""; *h2 = "";
    *color = C_WHITE;
    title[0] = 0;
    int L = a->layer;
    if (!L && (a->view == V_HOME || a->view == V_MIX)) L = L_HOME;
    switch (L) {
    case L_HOME:
        snprintf(title, (size_t)cap, "HOME");
        *h1 = "WHITE = CUE PATTERN  BLACK 1-6 = MUTE";
        *h2 = "EXIT: RELEASE HOME  HOLD BLACK = SOLO";
        map_home(a, K);
        return;
    case L_LEN: {
        snprintf(title, (size_t)cap, "LENGTH AND SPEED");
        *color = bm_track_color(t);
        *h1 = "WHITE = SET LENGTH  OCT-/OCT+ = PAGE";
        *h2 = "EXIT: RELEASE SEQ  BLACK 1-6 = SPEED";
        int len = bm_trk_len(&a->pat, t, kind == KIND_NOTE ? -1 : tr_lane(a));
        for (int w = 0; w < 16; w++) {
            int v = a->len_page * 16 + w + 1;
            snprintf(b, sizeof b, "%d", v);
            KW(K, w, b, v == len ? LED_ON : (v < len ? LED_MID : LED_DIM), bm_track_color(t));
        }
        static const char *const SP[SPD_COUNT] = {"X2", "3/2", "X1", "3/4", "1/2", "1/4"};
        for (int s = 0; s < SPD_COUNT; s++) KB(K, s, SP[s], a->pat.trk[t].speed == s ? LED_ON : LED_DIM, C_WHITE);
        int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
        for (int c = 1; c <= 4; c++) { snprintf(b, sizeof b, "%dBR", c); KB(K, 5 + c, b, len == c * spb ? LED_ON : LED_DIM, C_YELLOW); }
        return;
    }
    case L_ERASE:
        snprintf(title, (size_t)cap, "ERASE");
        *color = C_RED;
        *h1 = app_playing(a) ? "HOLD WHITE = ERASE LIVE  OCT+ = REDO"
            : kind == KIND_TR ? "WHITE = ERASE INSTRUMENT  OCT+ = REDO" : "WHITE = ERASE STEP  OCT+ = REDO";
        *h2 = "EXIT: RELEASE EDIT  OCT- = UNDO";
        if (kind == KIND_TR) map_lanes(a, K, t, a->erase_mask, LM_ERASE);
        else map_steps(a, K, 2);
        KB(K, 0, "CPY", LED_DIM, C_WHITE); KB(K, 1, "PST", LED_DIM, C_WHITE); KB(K, 2, "CLR", LED_DIM, C_RED);
        if (t == 1) KB(K, 3, "CAPT", LED_DIM, C_YELLOW);
        return;
    case L_ACC:
        snprintf(title, (size_t)cap, "ACCENT");
        *h1 = kind == KIND_NOTE ? "WHITE = CYCLE ACCENT AND GHOST" : "WHITE = TOGGLE ACCENT";
        *h2 = "EXIT: RELEASE ENV  OCT+ = NEXT PAGE";
        map_steps(a, K, 0);
        return;
    case L_REP:
        snprintf(title, (size_t)cap, kind == KIND_NOTE ? "TIE" : "REPEAT");
        *color = C_YELLOW;
        *h1 = kind == KIND_NOTE ? "WHITE = TIE TO NEXT NOTE  K1 = GLIDE" : "WHITE = CYCLE X1-X4 AND FLAM";
        *h2 = "EXIT: RELEASE LFO  OCT+ = NEXT PAGE";
        map_steps(a, K, 1);
        return;
    case L_PUNCH:
        snprintf(title, (size_t)cap, "PUNCH-IN");
        *color = C_RED;
        *h1 = "WHITE = PLAY EFFECT  BLACK 1-6 = MUTE";
        *h2 = "EXIT: RELEASE FX";
        for (int w = 0; w < 16; w++) KW(K, w, PUNCH_NAMES[w], a->ctx.st.punch == w ? LED_ON : (a->ctx.arena_busy && w <= 6 ? LED_OFF : LED_DIM), C_YELLOW);
        for (int i = 0; i < BM_TRACKS; i++) {
            bool mute = a->pat.trk[i].mute != 0;
            snprintf(b, sizeof b, "T%d", i + 1);
            KB(K, i, b, mute ? LED_DIM : LED_MID, bm_track_color(i))->live = (uint8_t)(!mute && a->now < a->trig_until[i]);
        }
        return;
    case L_CHORDS:
        snprintf(title, (size_t)cap, "CHORDS");
        *color = C_GREEN;
        *h1 = "WHITE = PLAY CHORD  BLACK 1-4 = EXTEND";
        *h2 = "EXIT: RELEASE SCL";
        map_chords(a, K);
        return;
    case L_ARP: case L_ROLL:
        if (L == L_ROLL) {
            snprintf(title, (size_t)cap, "ROLL %s", bm_roll_names[a->roll_rate % 5]);
            *color = bm_track_color(t);
            *h1 = "HOLD WHITE = ROLL  K1 = ROLL SPEED";
            *h2 = a->rec ? "EXIT: RELEASE ARP  REC IS RECORDING" : "EXIT: RELEASE ARP";
            map_lanes(a, K, t, a->roll_mask, LM_ROLL);
        } else {
            snprintf(title, (size_t)cap, "ARPEGGIO");
            *color = bm_track_color(t);
            *h1 = "HOLD KEYS = ARPEGGIATE IN TEMPO";
            *h2 = "EXIT: RELEASE ARP";
            for (int k = 0; k < BM_KEYS; k++) {
                int note = BM_KEY_BASE_NOTE + k + 12 * a->oct;
                snprintf(K[k].label, sizeof K[k].label, "%s", bm_note_names[note % 12]);
                K[k].led = LED_DIM; K[k].color = bm_track_color(t);
            }
        }
        return;
    case L_COPY:
        snprintf(title, (size_t)cap, "COPY PATTERN");
        *color = C_YELLOW;
        *h1 = "WHITE = PASTE  BLACK+WHITE = ONE TRACK";
        *h2 = "EXIT: RELEASE SAVE";
        for (int w = 0; w < 16; w++) {
            snprintf(b, sizeof b, "%02d", w + 1);
            KW(K, w, b, w == a->ctx.st.pattern ? LED_ON : (bm_store_has(&a->store, (uint8_t)w) ? LED_MID : LED_DIM), C_YELLOW);
        }
        for (int i = 0; i < BM_TRACKS; i++) {
            snprintf(b, sizeof b, "T%d", i + 1);
            KB(K, i, b, LED_DIM, bm_track_color(i));                    /* sostenida: ON (app_leds) */
        }
        return;
    case L_TONE:
        snprintf(title, (size_t)cap, "TRANSPOSE");
        *color = C_CYAN;
        *h1 = "KEYS = TRANSPOSE PROJECT  K2 = OCTAVE";
        *h2 = "EXIT: RELEASE GLO  PLAY = TAP TEMPO";
        for (int k = 0; k < BM_KEYS; k++) {
            int off = k - 7;
            snprintf(K[k].label, sizeof K[k].label, off > 0 ? "+%d" : "%d", off);   /* el snprintf del SDK no tiene %+d */
            K[k].led = (24 + off == m->transpose) ? LED_ON : (off == 0 ? LED_MID : LED_DIM);
            K[k].color = C_CYAN;
        }
        return;
    case L_LANEMUTE: case L_FILL: {
        bool fill = L == L_FILL;
        snprintf(title, (size_t)cap, fill ? "FILL" : "MUTE INSTRUMENTS");
        *color = fill ? C_ORANGE : bm_track_color(t);
        if (fill) {
            *h1 = kind == KIND_TR ? "WHITE = MAKE HIT FILL-ONLY" : "WHITE = MAKE NOTE FILL-ONLY";
            *h2 = a->layer_latch ? "EXIT: BLACK 11 OR HOME  K1 = AUTO FILL" : "EXIT: RELEASE BLACK 11";
            map_steps(a, K, 3);
        } else {
            *h1 = "WHITE = TOGGLE MUTE  BLACK 1-8 = PLAY";
            *h2 = a->layer_latch ? "EXIT: BLACK 10 OR HOME" : "EXIT: RELEASE BLACK 10";
            map_lanes(a, K, t, 0, LM_MUTE);
        }
        if (kind == KIND_TR) tr_blacks(a, K, t, false);              /* las negras siguen eligiendo instrumento */
        KB(K, fill ? 10 : 9, "X", LED_BREATH, fill ? C_ORANGE : C_RED);   /* sostenida: ON (app_leds) */
        return;
    }
    default: break;
    }
    if (a->view == V_CH && a->page == PG_CHORDS) map_chords(a, K);
}

/* ------------------------------------------------------------ notas y grabación en vivo */
static void rec_syn(bm_app *a, int k, int t, const uint8_t *notes, int n)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
    uint32_t rt = a->ctx.st.rtick;
    uint32_t q = (rt + (uint32_t)tps / 2) / (uint32_t)tps;
    int s = (int)(q % (uint32_t)len);
    int micro = bm_clampi((int)(rt - q * (uint32_t)tps), -11, 11);   /* lo que se adelantó o se atrasó */
    if (micro > -3 && micro < 3) micro = 0;                    /* lo que está casi en el paso, en el paso */
    a->key_rec_step[k] = (int8_t)s;
    a->key_rec_tick[k] = rt;
    a->key_rec_slot[k] = -1;
    for (int i = 0; i < n; i++) {
        int slot = -1;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = bm_evc(&a->pat, t, s, e);
            if (!bm_event_empty(ev) && ev->note == notes[i]) { slot = e; break; }
            if (slot < 0 && bm_event_empty(ev)) slot = e;
        }
        if (slot < 0) continue;
        bm_event ev;
        bm_event_clear(&ev);
        ev.note = notes[i]; ev.len = 1; ev.micro = (int8_t)micro;
        app_push_ev(a, t, s, slot, &ev);
        if (a->key_rec_slot[k] < 0) a->key_rec_slot[k] = (int8_t)slot;
    }
    app_edited(a);
}

static int quant_step(const bm_app *a, int t, int len)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    return (int)(((a->ctx.st.rtick + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)(len ? len : 16));
}

static void rec_tr(bm_app *a, int t, int lane)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int s = quant_step(a, t, tr->lane[lane].len);
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (uint8_t)(tr->st[lane][s] | ST_ON));
    app_edited(a);
}

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel, int strum)
{
    if (strum && n > 1) {
        int32_t packed = 0;
        for (int i = 0; i < n && i < 4; i++) packed |= (int32_t)notes[i] << (8 * i);
        app_push_i(a, CMD_STRUM, (uint8_t)t, (uint8_t)n, (uint8_t)vel, packed);
    } else {
        for (int i = 0; i < n; i++) app_push(a, CMD_NOTE_ON, (uint8_t)t, notes[i], (uint8_t)vel);
    }
    memcpy(a->key_notes[k], notes, (size_t)n);
    a->key_nn[k] = (uint8_t)n;
    a->key_trk[k] = (uint8_t)t;
    if (n && app_kind(a, t) == KIND_NOTE) a->last_note[t] = notes[0];
    a->key_rec_step[k] = -1;
    if (a->rec && app_playing(a) && !seq_page(a)) {
        if (bm_is_grid(&a->pat, t)) rec_tr(a, t, notes[0] & 15);
        else if (a->ctx.st.loop_mode[t - 2] == LOOP_OFF) rec_syn(a, k, t, notes, n);
    }
}

static int chord_ext(const bm_app *a, int base)
{
    if (a->chord_mod & 1) return 1;
    if (a->chord_mod & 2) return 2;
    if (a->chord_mod & 4) return 3;
    if (a->chord_mod & 8) return 4;
    return base;
}

static void play_pad(bm_app *a, int k, int pad)
{
    const bm_meta *m = &a->pat.meta;
    int t = bm_app_chord_track(a), root, sc = app_scale(a, t, &root);
    bm_pad_chord pc = bm_chord_pad(m->chord_set, pad);
    int ext = chord_ext(a, pc.ext);
    uint8_t notes[4];
    int n = bm_chord_notes(root, sc, pc.degree, ext, m->chord_oct + a->oct, notes);
    if (m->chord_voicing == 1 && n >= 3) notes[1] = (uint8_t)bm_clampi(notes[1] + 12, 0, 127);
    if (m->chord_voicing == 2 && a->prev_chord_n) {           /* conducción de voces */
        int avg = 0;
        for (int i = 0; i < a->prev_chord_n; i++) avg += a->prev_chord[i];
        avg /= a->prev_chord_n;
        for (int i = 0; i < n; i++) {
            while (notes[i] - avg > 6 && notes[i] >= 12) notes[i] -= 12;
            while (avg - notes[i] > 6 && notes[i] <= 115) notes[i] += 12;
        }
    }
    memcpy(a->prev_chord, notes, (size_t)n);
    a->prev_chord_n = (uint8_t)n;
    bm_chord_name(root, sc, pc.degree, ext, a->chord_name, sizeof a->chord_name);
    bm_roman(root, sc, pc.degree, ext, a->chord_roman, sizeof a->chord_roman);
    a->last_pad = (int8_t)pad;
    play_notes(a, k, t, notes, n, 95, 1);
}

static int base_oct(const bm_app *a, int t) { return (t == 2 ? 2 : 3) + a->oct; }

/* Tocar el instrumento del canal con una tecla (páginas que no son SEQ). */
static void play_key_down(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    const bm_meta *m = &a->pat.meta;
    if (kind == KIND_TR) {
        if (w >= 0) {
            uint8_t n = (uint8_t)w;
            a->sel_lane[t] = (uint8_t)w;
            play_notes(a, k, t, &n, 1, 110, 0);
        }
        return;
    }
    int root, sc = app_scale(a, t, &root);
    if ((a->view == V_CH && a->page == PG_SCL) || m->scale_lock) {
        if (w >= 0) {
            uint8_t n = (uint8_t)bm_scale_note(root, sc, w, base_oct(a, t));
            play_notes(a, k, t, &n, 1, 105, 0);
        } else if (b < 7) {
            uint8_t notes[4];
            int ext = chord_ext(a, m->voicing % 5);
            int n = bm_chord_notes(root, sc, b, ext, base_oct(a, t), notes);
            bm_chord_name(root, sc, b, ext, a->chord_name, sizeof a->chord_name);
            bm_roman(root, sc, b, ext, a->chord_roman, sizeof a->chord_roman);
            play_notes(a, k, t, notes, n, 95, 1);
        } else { a->chord_mod |= (uint8_t)(1u << (b - 7)); a->key_role[k] = ROLE_CMOD; }
        return;
    }
    int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
    uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
    play_notes(a, k, t, &n, 1, 105, 0);
}

/* ------------------------------------------------------------ SEQ: pasos */
static void seq_step_down(bm_app *a, int w)
{
    int t = a->track, kind = app_kind(a, t);
    if (a->held_white >= 0) {                                  /* notas: blanca sostenida + otra = una nota larga */
        int s2 = a->step_page * BM_PAGE_STEPS + w, s = a->held_step;
        if (kind != KIND_NOTE || s < 0 || s2 <= s) return;
        int l = s2 - s + 1;
        for (int e = 0; e < BM_EVENTS; e++) {
            bm_event ev = *bm_evc(&a->pat, t, s, e);
            if (bm_event_empty(&ev)) continue;
            ev.len = (uint8_t)l; ev.gate = 0;
            app_push_ev(a, t, s, e, &ev);
        }
        a->held_edit = 1;
        app_edited(a);
        char b[24]; snprintf(b, sizeof b, TF("LONG NOTE: %d STEPS"), l);
        app_toast(a, b);
        return;
    }
    int s = a->step_page * BM_PAGE_STEPS + w;
    int len = bm_trk_len(&a->pat, t, kind == KIND_NOTE ? -1 : tr_lane(a));
    if (s >= len) { app_toast(a, "OUTSIDE THE LENGTH"); return; }
    app_undo_snapshot(a, t);
    a->held_white = (int8_t)w; a->held_step = (int8_t)s;
    a->held_long = 0; a->held_edit = 0; a->held_notes = 0; a->step_t0 = a->now;
    if (kind == KIND_TR) {
        int lane = tr_lane(a);
        a->held_had_event = (bm_trk_trc(&a->pat, t)->st[lane][s] & ST_ON) != 0;
        if (!a->held_had_event) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, ST_ON); app_edited(a); }
        return;
    }
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    a->held_had_event = n > 0;
    if (!n) {
        bm_event e; bm_event_clear(&e);
        e.note = a->last_note[t];
        app_push_ev(a, t, s, 0, &e);
        app_edited(a);
    }
}

static void seq_step_up(bm_app *a, int w)
{
    if (w != a->held_white) return;
    int t = a->track, s = a->held_step;
    if (s >= 0 && a->held_had_event && !a->held_long && !a->held_edit) {
        if (bm_is_grid(&a->pat, t)) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)tr_lane(a), (uint8_t)s, 0); app_edited(a); }
        else {
            bm_event e; bm_event_clear(&e);
            for (int k = 0; k < BM_EVENTS; k++) app_push_ev(a, t, s, k, &e);
            app_edited(a);
        }
    }
    a->held_white = -1; a->held_step = -1; a->held_long = 0; a->held_edit = 0;
}

/* nota en el paso sostenido (negras 1-8 = grados de la escala) */
static void step_note(bm_app *a, int note)
{
    int t = a->track, s = a->held_step;
    int idx[4], n = syn_step_events(a, t, s, idx);
    a->held_edit = 1;
    for (int j = 0; j < n; j++)
        if (bm_evc(&a->pat, t, s, idx[j])->note == note && (a->held_had_event || a->held_notes)) {   /* ya estaba: se quita */
            bm_event e; bm_event_clear(&e);
            app_push_ev(a, t, s, idx[j], &e); app_edited(a);
            return;
        }
    int slot = -1;
    if (!a->held_had_event && !a->held_notes) slot = n ? idx[0] : 0;   /* el primero reemplaza la nota de relleno */
    else for (int e = 0; e < BM_EVENTS && slot < 0; e++) if (bm_event_empty(bm_evc(&a->pat, t, s, e))) slot = e;
    if (slot < 0) { app_toast(a, "4 NOTES PER STEP"); return; }
    bm_event e;
    if (n) e = *bm_evc(&a->pat, t, s, idx[0]); else bm_event_clear(&e);
    e.note = (uint8_t)note;
    app_push_ev(a, t, s, slot, &e);
    a->held_notes++;
    a->last_note[t] = (uint8_t)note;
    app_edited(a);
}

/* ------------------------------------------------------------ negras 10 y 11: pantallas de MUTE y FILL
 * Un toque las deja abiertas (se cierran con la misma tecla o con HOME); mantener la tecla = solo
 * mientras la sostienes, como antes. En FILL el fill suena mientras la pantalla está abierta. */
static void enc_fill_every(bm_app *a, int d);

static void key_screen_open(bm_app *a, int k, int L)
{
    a->layer = (uint8_t)L;
    a->layer_latch = 1;
    a->ov.on = 0;
    a->key_role[k] = L == L_FILL ? ROLE_FILLK : ROLE_LMUTE;
    a->key_t0[k] = a->now;
    if (L == L_FILL) { app_push(a, CMD_FILL, 1, 0, 0); a->fill = 1; }
}

static void key_screen_close(bm_app *a)
{
    if (a->layer == L_FILL) { app_push(a, CMD_FILL, 0, 0, 0); a->fill = 0; }
    if (a->layer == L_FILL || a->layer == L_LANEMUTE) a->layer = L_NONE;
    a->layer_latch = 0;
}

/* FILL: la blanca deja ese golpe (o esa nota) solo para el fill, u otra vez como siempre */
static void paint_fill(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w, kind = app_kind(a, t);
    app_undo_snapshot(a, t);
    if (kind == KIND_NOTE) {
        if (s >= a->pat.trk[t].length) return;
        int idx[4], n = syn_step_events(a, t, s, idx);
        if (!n) { app_toast(a, "STEP HAS NO NOTES: WRITE THEM IN SEQ"); return; }
        int fill = bm_evc(&a->pat, t, s, idx[0])->prob != BM_PROB_FILL;
        for (int j = 0; j < n; j++) {
            bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
            e.prob = fill ? BM_PROB_FILL : 100;
            app_push_ev(a, t, s, idx[j], &e);
        }
        app_toast(a, fill ? "ONLY IN THE FILL" : "ALWAYS PLAYS");
        app_edited(a);
        return;
    }
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = tr->st[lane][s];
    int cond = (v & ST_COND_MASK) >> ST_COND_SH;
    if (!(v & ST_ON)) v = (uint8_t)(ST_ON | (1u << ST_COND_SH));               /* vacío: golpe solo en el fill */
    else if (cond == 1) v = 0;                                                  /* ya era del fill: se borra */
    else v = (uint8_t)((v & ~ST_COND_MASK) | (1u << ST_COND_SH));              /* golpe normal: pasa al fill */
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v);
    app_edited(a);
}

/* ------------------------------------------------------------ capas: pasos pintados */
static void paint_accent(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w, kind = app_kind(a, t);
    app_undo_snapshot(a, t);
    if (kind == KIND_NOTE) {
        if (s >= a->pat.trk[t].length) return;
        int idx[4], n = syn_step_events(a, t, s, idx);
        if (!n) { bm_event e; bm_event_clear(&e); e.note = a->last_note[t]; e.vel = BM_VEL_ACC; app_push_ev(a, t, s, 0, &e); app_edited(a); return; }
        for (int j = 0; j < n; j++) {
            bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
            e.vel = e.vel >= BM_VEL_ACC ? BM_VEL_GHOST : e.vel <= BM_VEL_GHOST ? BM_VEL_NORMAL : BM_VEL_ACC;
            app_push_ev(a, t, s, idx[j], &e);
        }
        app_edited(a);
        return;
    }
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = tr->st[lane][s];
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (v & ST_ON) ? (v ^ ST_ACC) : (ST_ON | ST_ACC));
    app_edited(a);
}

static void paint_repeat(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w, kind = app_kind(a, t);
    app_undo_snapshot(a, t);
    if (kind == KIND_NOTE) {
        if (s >= a->pat.trk[t].length) return;
        int idx[4], n = syn_step_events(a, t, s, idx);
        for (int j = 0; j < n; j++) {
            bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
            e.len = (e.len & BM_LEN_TIE) ? (uint8_t)(e.len & BM_LEN_MASK) : (uint8_t)(e.len | BM_LEN_TIE);
            app_push_ev(a, t, s, idx[j], &e);
        }
        if (n) app_edited(a);
        return;
    }
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = tr->st[lane][s];
    int ln = lane;
    if (!(v & ST_ON)) v = ST_ON;
    int r = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
    r = (r + 1) % 5;
    v = (uint8_t)(v & ~(ST_RAT_MASK | ST_FLAM));
    if (r == 4) v |= ST_FLAM; else v |= (uint8_t)(r << ST_RAT_SH);
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)ln, (uint8_t)s, v);
    app_edited(a);
}

static void set_len(bm_app *a, int t, int len)
{
    len = bm_clampi(len, 1, BM_STEPS);
    if (bm_is_grid(&a->pat, t)) { for (int l = 0; l < BM_LANES; l++) app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)l, 0, len); app_edited(a); }
    else app_set_track(a, t, TRK_OFF(length), len);
    char b[24]; snprintf(b, sizeof b, TF("T%d LENGTH %d"), t + 1, len);
    app_toast(a, b);
}

/* El sampler: CAPT escribe en la rejilla lo que BREAK está haciendo en este compás (el pad que
 * cambió, las repeticiones y los silencios). La reversa y la media velocidad no se guardan. */
static void capture(bm_app *a)
{
    const bm_tr *tr = &a->pat.tr[1];
    const bm_ctx *c = &a->ctx;
    app_undo_snapshot(a, 1);
    uint32_t blk = c->st.gen_blk;
    int n = 0;
    for (int k = 0; k < 16; k++) {
        uint8_t sub = c->gen_sl[k], fx = c->gen_fx[k];
        if (sub == 0xFF && !(fx & (GEN_RT_MASK | GEN_SKIP))) continue;
        uint32_t at = blk * 16u + (uint32_t)k;
        uint8_t keep = 0;
        int first = -1;
        for (int l = 0; l < BM_LANES; l++) {
            int len = tr->lane[l].len ? tr->lane[l].len : 16, s = (int)(at % (uint32_t)len);
            if (!(tr->st[l][s] & ST_ON)) continue;
            if (first < 0) { first = l; keep = tr->st[l][s]; }
            if (sub != 0xFF || (fx & GEN_SKIP)) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0);
        }
        if (first < 0 || (fx & GEN_SKIP)) { n += first >= 0; continue; }
        int l = sub != 0xFF ? sub : first;
        int len = tr->lane[l].len ? tr->lane[l].len : 16, s = (int)(at % (uint32_t)len);
        uint8_t v = (uint8_t)((keep & ~(ST_RAT_MASK | ST_FLAM | ST_LOCK)) | ST_ON);
        int rt = fx & GEN_RT_MASK;
        if (rt >= 1 && rt <= 3) v |= (uint8_t)(rt << ST_RAT_SH);
        app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, v);
        n++;
    }
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, TF("CAPTURED: %d STEPS"), n);
    app_toast(a, b);
}

/* ------------------------------------------------------------ PADS (T2) */
static const char *const PAD_MODE_N[PAD_MODES] = {"ONESHOT", "KEY", "THRU", "LOOP"};
const char *bm_pad_mode_name(int mode) { return PAD_MODE_N[(unsigned)mode % PAD_MODES]; }

static uint32_t snap_len(const bm_app *a, const bm_take *tk)
{
    uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10;
    uint32_t eighth = (uint32_t)BM_SMP_RATE * 300u / bpm;        /* muestras por corchea */
    return a->cut_snap == 1 ? eighth / 2 : a->cut_snap == 2 ? eighth : 0;
}

static uint32_t snap_to(const bm_app *a, const bm_take *tk, uint32_t pos)
{
    uint32_t step = snap_len(a, tk);
    return step ? ((pos + step / 2) / step) * step : pos;
}

/* la ganancia, el BPM o los compases de una toma cambiaron */
static void take_changed(bm_app *a, int id)
{
    a->take_dirty |= (uint8_t)(1u << id);
    a->t_edit = a->now;
    app_send_takes(a);
}

static const bm_take *pad_take(const bm_app *a, const bm_pad *p)
{
    return p->src < BM_TAKES && a->take[p->src].valid ? &a->take[p->src] : NULL;
}

/* Una tecla toca un pad: sostenida, según su modo (ONESHOT, KEY, THRU, LOOP). Con REC sonando,
 * el golpe queda en la línea del pad. */
static void pad_down(bm_app *a, int k, int pad)
{
    a->key_role[k] = ROLE_PAD;
    a->pad_key = (int8_t)k;
    a->sel_lane[1] = (uint8_t)pad;
    app_push(a, CMD_PAD_KEY, 1, (uint8_t)pad, 110);
    if (a->rec && app_playing(a)) rec_tr(a, 1, pad);
}

/* Escuchar un trozo de una toma (TAKES): sostenido sigue por la toma. */
static void preview_down(bm_app *a, int k, int src, uint32_t start, uint32_t len)
{
    a->key_role[k] = ROLE_PREVIEW;
    a->pad_key = (int8_t)k;
    app_push_i(a, CMD_SMP_PREVIEW, 1, (uint8_t)src, 0, (int32_t)(start | len << 16));
}

static void pad_toast(bm_app *a, const char *fmt)
{
    char b[30]; snprintf(b, sizeof b, fmt, a->sel_lane[1] % 16 + 1);
    app_toast(a, b);
}

/* N1-N4 de PADS: copiar, pegar, vaciar, y el inicio al golpe más cercano */
static void pad_copy(bm_app *a)
{
    int l = a->sel_lane[1] & 15;
    a->pad_clip = a->pat.pad[l];
    a->pad_clip_level = a->pat.tr[1].lane[l].level; a->pad_clip_choke = a->pat.tr[1].lane[l].choke;
    a->pad_clip_on = 1;
    pad_toast(a, TF("PAD %d COPIED"));
}

static void pad_paste(bm_app *a)
{
    if (!a->pad_clip_on) { app_toast(a, "NOTHING TO PASTE HERE"); return; }
    int l = a->sel_lane[1] & 15;
    a->pat.pad[l] = a->pad_clip;
    app_push_i(a, CMD_SET_LANE, 1, (uint8_t)l, 1, a->pad_clip_level);
    app_push_i(a, CMD_SET_LANE, 1, (uint8_t)l, 7, a->pad_clip_choke);
    app_edited(a);
    pad_toast(a, TF("PASTED ON PAD %d"));
}

static void pad_clear(bm_app *a)
{
    bm_pad_clear(app_pad(a));
    app_edited(a);
    pad_toast(a, TF("PAD %d EMPTY"));
}

static void pad_hit(bm_app *a)
{
    bm_pad *p = app_pad(a);
    const bm_take *tk = pad_take(a, p);
    if (!tk) { app_toast(a, "EMPTY PAD: TAKES > PAD"); return; }
    uint32_t m[BM_SLICES], bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10;
    int n = bm_slice_onsets(tk->data, tk->len, (uint32_t)BM_SMP_RATE * 75u / bpm, 127, m);
    uint32_t best = p->start, d = 0xFFFFFFFFu, end = (uint32_t)p->start + p->len;
    for (int i = 0; i < n; i++) {
        uint32_t x = m[i] > p->start ? m[i] - p->start : p->start - m[i];
        if (x < d && m[i] + 64 < end) { d = x; best = m[i]; }
    }
    p->start = (uint16_t)best; p->len = (uint16_t)(end - best);
    app_edited(a);
    app_toast(a, "START ON THE HIT");
}

/* K de la página TRIM: mueve el inicio (el final queda) o el largo. Sin SNAP, dos columnas de la
 * ventana por paso; con SNAP, de rejilla en rejilla. */
static void pad_trim(bm_app *a, int which, int d)
{
    bm_pad *p = app_pad(a);
    const bm_take *tk = pad_take(a, p);
    if (!tk) { app_toast(a, "EMPTY PAD: TAKES > PAD"); return; }
    uint32_t step = snap_len(a, tk), span = a->cut_v1 > a->cut_v0 ? a->cut_v1 - a->cut_v0 : tk->len;
    int32_t px = (int32_t)(span / 116u);
    if (px < 8) px = 8;
    int32_t end = (int32_t)p->start + p->len;
    if (which == 0) {
        int32_t s = (int32_t)p->start;
        s = step ? (int32_t)snap_to(a, tk, (uint32_t)s) + lst(a, d) * (int32_t)step : s + d * px;
        s = bm_clampi(s, 0, end - 64);
        p->start = (uint16_t)s; p->len = (uint16_t)(end - s);
    } else {
        int32_t e = step ? (int32_t)snap_to(a, tk, (uint32_t)end) + lst(a, d) * (int32_t)step : end + d * px;
        e = bm_clampi(e, (int32_t)p->start + 64, (int32_t)tk->len);
        p->len = (uint16_t)(e - (int32_t)p->start);
    }
    app_edited(a);
}

static void pad_sound(bm_app *a, int k, int d)
{
    bm_pad *p = app_pad(a);
    int l = a->sel_lane[1] & 15;
    const bm_lane *ln = &a->pat.tr[1].lane[l];
    char b[16];
    if (!bm_pad_ok(p)) { app_toast(a, "EMPTY PAD: TAKES > PAD"); return; }
    if (a->ed_page[1] == 1) {                                    /* SOUND: PITCH LEVEL PLAY DIR */
        if (k == 0) { p->pitch = (uint8_t)bm_clampi(p->pitch + lst(a, d), 0, 48); snprintf(b, sizeof b, "%+d", p->pitch - 24); app_overlay(a, 0, "PITCH", b, "ST", p->pitch * 255 / 48, 128, bm_knob_color(0)); }
        else if (k == 1) { int v = bm_clampi(ln->level + d, 0, 127); app_push_i(a, CMD_SET_LANE, 1, (uint8_t)l, 1, v); snprintf(b, sizeof b, "%d", v); app_overlay(a, 1, "LEVEL", b, "", v * 2, -1, bm_knob_color(1)); }
        else if (k == 2) { p->mode = (uint8_t)wrap(p->mode + lst(a, d), PAD_MODES); app_overlay(a, 2, "PLAY", PAD_MODE_N[p->mode], "", p->mode * 85, -1, bm_knob_color(2)); }
        else { p->flags = (uint8_t)(d > 0 ? p->flags | PAD_REV : p->flags & ~PAD_REV); app_overlay(a, 3, "DIR", (p->flags & PAD_REV) ? "REV" : "FWD", "", (p->flags & PAD_REV) ? 255 : 0, -1, bm_knob_color(3)); }
    } else if (a->ed_page[1] == 3) {                             /* LOOP: dónde empieza y termina el sustain */
        if (k == 0) { p->loop_s = (uint8_t)bm_clampi(p->loop_s + d, 0, 254); if (p->loop_e && p->loop_e <= p->loop_s) p->loop_e = (uint8_t)(p->loop_s + 1); snprintf(b, sizeof b, "%d", p->loop_s * 100 / 255); app_overlay(a, 0, "LOOP IN", b, "%", p->loop_s, -1, bm_knob_color(0)); }
        else if (k == 1) { int v = bm_clampi(p->loop_e + d, 0, 255); if (v && v <= p->loop_s) v = p->loop_s + 1; p->loop_e = (uint8_t)v; if (v) snprintf(b, sizeof b, "%d", v * 100 / 255); else snprintf(b, sizeof b, "WHOLE"); app_overlay(a, 1, "LOOP OUT", b, v ? "%" : "", v, -1, bm_knob_color(1)); }
        else if (k == 2) { p->mode = (uint8_t)wrap(p->mode + lst(a, d), PAD_MODES); app_overlay(a, 2, "PLAY", PAD_MODE_N[p->mode], "", p->mode * 85, -1, bm_knob_color(2)); }
        if (k <= 1 && p->mode != PAD_LOOP) { p->mode = PAD_LOOP; app_toast(a, "PLAY = LOOP: HELD REPEATS THE LOOP"); }
    } else {                                                     /* ENV: ATTACK RELEASE CHOKE TEMPO */
        if (k == 0) { p->attack = (uint8_t)bm_clampi(p->attack + d, 0, 127); }
        else if (k == 1) { p->release = (uint8_t)bm_clampi(p->release + d, 0, 127); }
        else if (k == 2) { int v = bm_clampi(ln->choke + lst(a, d), 0, 3); app_push_i(a, CMD_SET_LANE, 1, (uint8_t)l, 7, v); }
        else p->flags = (uint8_t)(d > 0 ? p->flags | PAD_TEMPO : p->flags & ~PAD_TEMPO);   /* derecha SYNC */
    }
    app_edited(a);
}

/* Un kit desde la toma marcada en TAKES (pregunta si el kit ya tiene pads). */
static void kit_ask(bm_app *a, int src, int how)
{
    if (src >= BM_TAKES || !a->take[src].valid) { app_toast(a, "NO TAKE"); return; }
    if (app_kit_empty(a)) {
        int n = app_kit_from_take(a, src, how);
        char b[30]; snprintf(b, sizeof b, TF("KIT: %d PADS"), n);
        app_toast(a, b);
        return;
    }
    char l1[30]; snprintf(l1, sizeof l1, TF("NEW KIT FROM TAKE %02d?"), src + 1);
    app_ask(a, ASK_KIT, src, how, l1, "THE PADS ARE REPLACED");
}

/* ------------------------------------------------------------ teclas */
static void song_append(bm_app *a, int p)
{
    if (a->song.n >= BM_SONG_MAX) { app_toast(a, "SONG FULL"); return; }
    a->song.pat[a->song.n] = (uint8_t)p; a->song.rep[a->song.n] = 1;
    a->song_cur = a->song.n++;
    a->song_dirty = 1; a->t_edit = a->now;
}

static void song_delete(bm_app *a)
{
    if (!a->song.n) return;
    for (int i = a->song_cur; i + 1 < a->song.n; i++) { a->song.pat[i] = a->song.pat[i + 1]; a->song.rep[i] = a->song.rep[i + 1]; }
    a->song.n--;
    if (a->song_cur >= a->song.n && a->song_cur) a->song_cur--;
    a->song_dirty = 1; a->t_edit = a->now;
}

/* Grabación libre (como una loopera): en un canal de notas vacío, REC no tiene largo fijo.
 * Al parar REC, el canal queda con los compases completos que tocaste (1-4) y cada nota
 * con lo que la sostuviste. */
static int notes_empty(const bm_app *a, int t)
{
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) return 0;
    return 1;
}

static void free_rec_start(bm_app *a)
{
    int t = a->track;
    if (app_kind(a, t) != KIND_NOTE || !notes_empty(a, t)) return;
    a->rec_free = (uint8_t)(t + 1);
    a->rec_free_t0 = app_playing(a) ? a->ctx.st.btick : 0xFFFFFFFFu;
    app_set_track(a, t, TRK_OFF(length), BM_STEPS);
    app_toast(a, "FREE REC: REC AGAIN CLOSES");
}

static void free_rec_close(bm_app *a)
{
    int t = a->rec_free - 1;
    a->rec_free = 0;
    int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int bars = 1;
    if (a->rec_free_t0 != 0xFFFFFFFFu) {
        uint32_t el = a->ctx.st.btick - a->rec_free_t0;
        bars = bm_clampi((int)((el + BM_BAR_TICKS / 2) / BM_BAR_TICKS), 1, 4);
    }
    int len = bm_clampi(bars * spb, 1, BM_STEPS);
    /* lo grabado se dobla sobre el largo nuevo, en su mismo lugar del compás */
    bm_notes *nb = &a->redo.nt;
    a->redo_track = -1;
    for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) bm_event_clear(&nb->ev[s][k]);
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, s, k);
            if (bm_event_empty(e)) continue;
            int d = s % len;
            for (int j = 0; j < BM_EVENTS; j++)
                if (bm_event_empty(&nb->ev[d][j])) { nb->ev[d][j] = *e; break; }
                else if (nb->ev[d][j].note == e->note) break;
        }
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, nb);
    app_set_track(a, t, TRK_OFF(length), len);
    char b[30]; snprintf(b, sizeof b, bars > 1 ? TF("T%d: %d BARS") : TF("T%d: %d BAR"), t + 1, bars);
    app_toast(a, b);
}

static void toggle_mute(bm_app *a, int t)
{
    bool muted = !a->pat.trk[t].mute;                         /* el patrón cambia cuando el audio lo aplica */
    app_set_track(a, t, TRK_OFF(mute), muted ? 1 : 0);
    char b[24]; snprintf(b, sizeof b, "%s %s", bm_track_name(a, t), T(muted ? "MUTED" : "ON"));
    app_toast(a, b);
}

static void toggle_lane_mute(bm_app *a, int t, int lane)
{
    uint16_t m = (uint16_t)(bm_trk_trc(&a->pat, t)->mute ^ (1u << lane));
    app_push_i(a, CMD_LANE_MUTE, (uint8_t)t, 0, 0, m);
    app_edited(a);
    char b[24]; snprintf(b, sizeof b, "%s %s", T(bm_lane_name(a, t, lane)), T((m >> lane) & 1 ? "MUTE" : "PLAYS"));
    app_toast(a, b);
}

static void scene_store(bm_app *a, int s)
{
    bm_scene *sc = &a->song.scene[s];
    sc->valid = 1; sc->pattern = a->ctx.st.pattern; sc->trk_mute = 0;
    for (int t = 0; t < BM_TRACKS; t++) if (a->pat.trk[t].mute) sc->trk_mute |= (uint8_t)(1u << t);
    for (int t = 0; t < BM_TR_TRACKS; t++) sc->lane_mute[t] = a->pat.tr[t].mute;
    a->song_dirty = 1; a->t_edit = a->now;
    char b[24]; snprintf(b, sizeof b, TF("SCENE %c STORED"), 'A' + s);
    app_toast(a, b);
}

static void scene_launch(bm_app *a, int s)
{
    if (!a->song.scene[s].valid) { app_toast(a, "EMPTY SCENE (HOME > SCENES)"); return; }
    if (a->launch_held & ~(1u << s)) {               /* otra sostenida: cadena */
        if (a->chain_n < 8) a->chain[a->chain_n++] = (uint8_t)s;
        app_push_ptr(a, CMD_CHAIN, a->chain_n, a->chain);
        app_toast(a, "SCENE CHAIN");
    } else {
        a->chain_n = 0;
        a->chain[a->chain_n++] = (uint8_t)s;
        app_push(a, CMD_SCENE, (uint8_t)s, 0, 0);
        char b[16]; snprintf(b, sizeof b, TF("SCENE %c"), 'A' + s);
        app_toast(a, b);
    }
    a->launch_held |= (uint8_t)(1u << s);
}

/* Gramática de HOME (pantalla, MEZCLA y capa): blancas patrones, negras mutes, escenas y FILL. */
static void home_key(bm_app *a, int k, int w, int b)
{
    if (w >= 0) {
        a->key_role[k] = ROLE_PAT;
        if (a->pat_held & ~(1u << w)) {                     /* otra blanca sostenida: cadena */
            if (a->pchain_n < 8) a->pchain[a->pchain_n++] = (uint8_t)w;
            app_push_ptr(a, CMD_PCHAIN, a->pchain_n, a->pchain);
            char t[24]; snprintf(t, sizeof t, TF("CHAIN OF %d PATTERNS"), a->pchain_n);
            app_toast(a, t);
        } else {
            a->pchain_n = 0;
            a->pchain[a->pchain_n++] = (uint8_t)w;
            app_select_pattern(a, w);
        }
        a->pat_held |= (uint16_t)(1u << w);
        return;
    }
    if (b < 6) { a->key_role[k] = ROLE_MUTE; a->key_t0[k] = a->now; return; }
    if (b < 10) { a->key_role[k] = ROLE_SCENE; scene_launch(a, b - 6); return; }
    a->key_role[k] = ROLE_FILL;
    app_push(a, CMD_FILL, 1, 0, 0); a->fill = 1;
}

static void key_release(bm_app *a, int k, int w, int b)
{
    if (a->key_nn[k]) {
        int t = a->key_trk[k];
        if (app_kind(a, t) == KIND_NOTE) for (int i = 0; i < a->key_nn[k]; i++) app_push(a, CMD_NOTE_OFF, (uint8_t)t, a->key_notes[k][i], 0);
        if (a->key_rec_step[k] >= 0 && a->key_rec_slot[k] >= 0 && app_kind(a, t) == KIND_NOTE) {
            int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            int s = a->key_rec_step[k];
            uint32_t held = a->ctx.st.rtick - a->key_rec_tick[k];     /* lo que la sostuviste, en ticks */
            int steps = (int)(held / (uint32_t)tps), frac = (int)(held % (uint32_t)tps), gate = 0;
            if (frac * 8 > tps) { steps++; gate = frac * 100 / tps; if (gate >= 90) gate = 0; }
            if (steps < 1) { steps = 1; gate = bm_clampi(frac * 100 / tps, 10, 99); if (gate >= 90) gate = 0; }
            if (!a->rec_free) {                                    /* el compás corta la nota (auto-gate clip) */
                int spb = BM_BAR_TICKS / tps, end = (s / spb + 1) * spb;
                if (s + steps > end) { steps = end - s; gate = 0; }
            }
            steps = bm_clampi(steps, 1, BM_STEPS);
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = *bm_evc(&a->pat, t, s, e);
                for (int i = 0; i < a->key_nn[k]; i++)
                    if (!bm_event_empty(&ev) && ev.note == a->key_notes[k][i]) { ev.len = (uint8_t)steps; ev.gate = (uint8_t)gate; app_push_ev(a, t, s, e, &ev); }
            }
        }
        a->key_nn[k] = 0;
        a->key_rec_step[k] = -1;
    }
    if (a->punch_key == k) { app_push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    switch (a->key_role[k]) {
    case ROLE_MUTE:
        if (a->solo_key == k && a->solo_on) {                 /* fin del solo: vuelven los mutes de antes */
            for (int t = 0; t < BM_TRACKS; t++) app_set_track(a, t, TRK_OFF(mute), (a->solo_saved >> t) & 1);
            a->solo_on = 0; a->solo_key = -1;
        } else if (b >= 0 && b < 6) toggle_mute(a, b);
        if (a->solo_key == k) a->solo_key = -1;
        break;
    case ROLE_FILL: app_push(a, CMD_FILL, 0, 0, 0); a->fill = 0; break;
    case ROLE_SCENE: if (b >= 6 && b < 10) a->launch_held &= (uint8_t)~(1u << (b - 6)); break;
    case ROLE_BRKFX:
        if (b >= 8) { a->brk_live &= (uint8_t)~(1u << (b - 8)); app_push(a, CMD_BRK_LIVE, a->brk_live, 0, 0); }
        break;
    case ROLE_PAD:                                          /* solo la última tecla de pad lo suelta */
        if (a->pad_key == k) { app_push(a, CMD_PAD_KEY, 0, (uint8_t)(a->sel_lane[1] & 15), 0); a->pad_key = -1; }
        break;
    case ROLE_PREVIEW:
        if (a->pad_key == k) { app_push(a, CMD_SMP_PREVIEW, 0, 0, 0); a->pad_key = -1; }
        break;
    case ROLE_CMOD:                                         /* solo el modificador suelta su +7/+9/SUS: un acorde no */
        if (b >= 0 && b < 4) a->chord_mod &= (uint8_t)~(1u << b);
        else if (b >= 7 && b <= 10) a->chord_mod &= (uint8_t)~(1u << (b - 7));
        break;
    case ROLE_LMUTE: case ROLE_FILLK:                       /* mantenida: era solo mientras la sostenías */
        if (a->layer_latch && a->now - a->key_t0[k] >= UI_HOLD_MS) key_screen_close(a);
        break;
    default: break;
    }
    a->key_role[k] = ROLE_NONE;
    a->key_t0[k] = 0;
    if (w >= 0) {
        if (a->roll_mask & (1u << w)) { a->roll_mask &= (uint16_t)~(1u << w); app_push_i(a, CMD_ROLL, a->track, (uint8_t)w, 0, 0); }
        a->erase_mask &= (uint16_t)~(1u << w);
        if (!a->erase_mask) a->erase_syn = 0;
        a->pat_held &= (uint16_t)~(1u << w);
        seq_step_up(a, w);
        if (w < 4) a->launch_held &= (uint8_t)~(1u << w);
    }
}

static void layer_key(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    switch (a->layer) {
    case L_HOME: home_key(a, k, w, b); return;
    case L_LEN:
        if (w >= 0) set_len(a, t, a->len_page * 16 + w + 1);
        else if (b >= 6 && b <= 9) {                                 /* compases completos */
            int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            set_len(a, t, (b - 5) * spb);
        } else if (b < SPD_COUNT) {
            app_set_track(a, t, TRK_OFF(speed), b);
            char s[24]; snprintf(s, sizeof s, TF("T%d SPEED %s"), t + 1, bm_speed_names[b]);
            app_toast(a, s);
        }
        return;
    case L_ACC: if (w >= 0) paint_accent(a, w); return;
    case L_REP: if (w >= 0) paint_repeat(a, w); return;
    case L_LANEMUTE: case L_FILL:
        if ((a->layer == L_LANEMUTE && b == 9) || (a->layer == L_FILL && b == 10)) { key_screen_close(a); return; }   /* la misma tecla: salir */
        if (w >= 0) {
            if (a->layer == L_FILL) paint_fill(a, w);
            else if (kind == KIND_TR) toggle_lane_mute(a, t, w);
            return;
        }
        if (kind == KIND_TR && b >= 0 && b < 8) {                            /* elegir (y escuchar) un instrumento */
            int lane = (tr_lane(a) / 8) * 8 + b;
            a->sel_lane[t] = (uint8_t)lane;
            if (bm_is_pads(t)) { pad_down(a, k, lane); return; }
            uint8_t n = (uint8_t)lane;
            play_notes(a, k, t, &n, 1, 100, 0);
        } else if (kind == KIND_TR && b == 8) a->sel_lane[t] = (uint8_t)((tr_lane(a) + 8) % 16);
        return;
    case L_PUNCH:
        if (w >= 0) { app_push(a, CMD_PUNCH, (uint8_t)w, 0, 0); a->punch_key = (int8_t)k; }
        else if (b < 6) toggle_mute(a, b);
        return;
    case L_ERASE:
        if (b >= 0) {
            if (b == 0) app_copy_track(a, t);
            else if (b == 1) app_paste_track(a, t);
            else if (b == 2) app_clear_track(a, t);
            else if (b == 3 && t == 1) capture(a);
            return;
        }
        if (kind == KIND_TR) {
            if (app_playing(a)) { if (!a->erase_mask) app_undo_snapshot(a, t); a->erase_mask |= (uint16_t)(1u << w); }
            else {
                const bm_tr *tr = bm_trk_trc(&a->pat, t);
                app_undo_snapshot(a, t);
                for (int s = 0; s < BM_STEPS; s++) if (tr->st[w][s]) app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)w, (uint8_t)s, 0);
                app_edited(a);
                char m[24]; snprintf(m, sizeof m, TF("%s ERASED"), T(bm_lane_name(a, t, w))); app_toast(a, m);
            }
        } else {
            if (app_playing(a)) { if (!a->erase_syn) app_undo_snapshot(a, t); a->erase_syn = 1; a->erase_mask |= (uint16_t)(1u << w); }
            else {
                int s = a->step_page * 16 + w;
                app_undo_snapshot(a, t);
                bm_event e; bm_event_clear(&e); for (int kk = 0; kk < BM_EVENTS; kk++) app_push_ev(a, t, s, kk, &e);
                app_edited(a);
            }
        }
        return;
    case L_ROLL:
        if (w >= 0 && (kind == KIND_TR || w < BM_SLICES)) {
            a->roll_mask |= (uint16_t)(1u << w);
            if (kind == KIND_TR) a->sel_lane[t] = (uint8_t)w;
            app_push_i(a, CMD_ROLL, (uint8_t)t, (uint8_t)w, 1, bm_roll_ticks[a->roll_rate % 5]);
        }
        return;
    case L_CHORDS:
        if (w >= 0) play_pad(a, k, w);
        else if (b < 4) { a->chord_mod |= (uint8_t)(1u << b); a->key_role[k] = ROLE_CMOD; }
        return;
    case L_ARP: {
        int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
        uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
        play_notes(a, k, t, &n, 1, 105, 0);
        return;
    }
    case L_COPY:
        if (w >= 0) {
            if (app_playing(a)) { app_toast(a, "STOP TO COPY"); return; }
            if (w == a->ctx.st.pattern) { app_toast(a, "IT IS THE CURRENT PATTERN"); return; }
            int trk = -1;
            for (int i = 0; i < BM_TRACKS; i++) if (a->keys_down[bm_black_key(i)]) trk = i;
            char l1[30], l2[30];
            if (trk >= 0) snprintf(l1, sizeof l1, TF("COPY T%d TO A%02d?"), trk + 1, w + 1);
            else snprintf(l1, sizeof l1, TF("COPY A%02d TO A%02d?"), a->ctx.st.pattern + 1, w + 1);
            snprintf(l2, sizeof l2, TF("A%02d ALREADY HAS DATA"), w + 1);
            if (bm_store_has(&a->store, (uint8_t)w)) app_ask(a, trk >= 0 ? ASK_COPY_TRK : ASK_COPY_PAT, w, trk, l1, l2);
            else if (trk >= 0) { app_job(a, JOB_COPY_TRK, w | (trk << 4)); app_toast(a, "TRACK COPIED"); }
            else { app_job(a, JOB_COPY_PAT, w); app_toast(a, "PATTERN COPIED"); }
        }
        return;
    case L_TONE: {
        int tp = bm_clampi(24 + k - 7, 0, 48);
        app_set_meta(a, META_OFF(transpose), tp);
        char s[24]; snprintf(s, sizeof s, TF("TRANSPOSE %+d"), tp - 24);
        app_toast(a, s);
        return;
    }
    default: return;
    }
}

static void seq_keys(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    if (w >= 0) { seq_step_down(a, w); return; }
    if (kind == KIND_TR) {
        if (b < 8) {
            int lane = (tr_lane(a) / 8) * 8 + b;
            a->sel_lane[t] = (uint8_t)lane;
            if (bm_is_pads(t)) { pad_down(a, k, lane); return; }
            uint8_t n = (uint8_t)lane;
            play_notes(a, k, t, &n, 1, 100, 0);
        } else if (b == 8) {
            a->sel_lane[t] = (uint8_t)((tr_lane(a) + 8) % 16);
            app_toast(a, bm_is_pads(t) ? (tr_lane(a) < 8 ? "PADS 1-8" : "PADS 9-16") : tr_lane(a) < 8 ? "INSTRUMENTS 1-8" : "INSTRUMENTS 9-16");
        } else if (b == 9) key_screen_open(a, k, L_LANEMUTE);
        else key_screen_open(a, k, L_FILL);
        return;
    }
    if (b < 8) {                                                         /* notas: grados de la escala */
        int root, sc = app_scale(a, t, &root);
        int note = bm_scale_note(root, sc, b, base_oct(a, t) + a->note_oct);
        if (a->held_step >= 0) { step_note(a, note); return; }
        uint8_t n = (uint8_t)note;
        play_notes(a, k, t, &n, 1, 105, 0);
        if (a->rec && app_playing(a) && a->ctx.st.loop_mode[t - 2] == LOOP_OFF) rec_syn(a, k, t, &n, 1);
    } else if (b == 8 || b == 9) {
        a->note_oct = (int8_t)bm_clampi(a->note_oct + (b == 9 ? 1 : -1), -2, 2);
        char s[24]; snprintf(s, sizeof s, TF("NOTES OCTAVE %+d"), a->note_oct);
        app_toast(a, s);
    } else key_screen_open(a, k, L_FILL);
}

static void page_keys(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    switch (a->page) {
    case PG_SEQ: seq_keys(a, k, w, b); return;
    case PG_CHORDS:
        if (w >= 0) play_pad(a, k, w);
        else if (b < 4) { a->chord_mod |= (uint8_t)(1u << b); a->key_role[k] = ROLE_CMOD; }
        return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (src >= BM_TAKES || !a->take[src].valid) { if (w >= 0 || b < 4) app_toast(a, "NO TAKE"); return; }
        const bm_take *tk = &a->take[src];
        if (w >= 0) {                                            /* la toma en 16 partes: sostenida sigue */
            preview_down(a, k, src, tk->len * (uint32_t)w / 16u, tk->len / 16u);
            return;
        }
        if (b == 0) kit_ask(a, src, 0);                          /* KIT en los golpes */
        else if (b == 1 || b == 2) kit_ask(a, src, b == 1 ? 16 : 8);
        else if (b == 3) {                                       /* la toma entera en el pad elegido */
            app_pad_take(a, a->sel_lane[1] & 15, src);
            char m[30]; snprintf(m, sizeof m, TF("TAKE %02d > PAD %d"), src + 1, (a->sel_lane[1] & 15) + 1);
            app_toast(a, m);
        } else if (b == 10) {
            char l1[30]; snprintf(l1, sizeof l1, TF("DELETE TAKE %02d?"), src + 1);
            app_ask(a, ASK_TAKE_DEL, src, 0, l1, "CANNOT BE UNDONE");
        }
        return;
    }
    case PG_REC:                                             /* grabando, las teclas no suenan; después, escuchar la toma nueva */
        if (w >= 0 && !a->ctx.st.smp_rec && a->ctx.st.smp_unsaved) {
            uint32_t len = a->ctx.smp[0].len;
            preview_down(a, k, BM_TAKE_NEW, len * (uint32_t)w / 16u, len / 16u);
        }
        return;
    case PG_PADS:
        if (w >= 0) { pad_down(a, k, w); return; }
        if (b == 0) pad_copy(a);
        else if (b == 1) pad_paste(a);
        else if (b == 2) pad_clear(a);
        else if (b == 3) pad_hit(a);
        else if (b >= 8) {                                       /* REV, HALF, STUTTER mientras se sostienen */
            a->key_role[k] = ROLE_BRKFX;
            a->brk_live |= (uint8_t)(1u << (b - 8));
            app_push(a, CMD_BRK_LIVE, a->brk_live, 0, 0);
        }
        return;
    default: break;
    }
    if (kind == KIND_TR && b >= 0) { seq_keys(a, k, -1, b); return; }
    play_key_down(a, k, w, b);
}

void bm_app_key(bm_app *a, int k, int down, uint32_t now)
{
    a->now = now;
    if (k < 0 || k >= BM_KEYS) return;
    if (down) {
        if (a->keys_down[k]) return;
        a->keys_down[k] = 1;
        bm_gesture_other(&a->ges, now);
        if (bm_gesture_is_down(&a->ges, BTN_HOME)) a->home_used = 1;
    } else {
        if (!a->keys_down[k]) return;
        a->keys_down[k] = 0;
    }
    if (a->view == V_BOOT) return;
    int w = bm_key_white_index(k), b = bm_key_black_index(k);
    if (!down) { key_release(a, k, w, b); return; }
    if (a->layer) { layer_key(a, k, w, b); return; }
    switch (a->view) {
    case V_HOME: case V_MIX: home_key(a, k, w, b); return;
    case V_SCENES:
        if (w >= 0 && w < 4) scene_launch(a, w);
        else if (w >= 4 && w < 8) scene_store(a, w - 4);
        else if (b >= 0) home_key(a, k, -1, b);
        return;
    case V_SONG:
        if (w >= 0) song_append(a, w);
        else if (b == 8) song_delete(a);
        return;
    case V_CH: page_keys(a, k, w, b); return;
    default: play_key_down(a, k, w, b); return;
    }
}

/* ------------------------------------------------------------ botones */
static int layer_of(const bm_app *a, int id)
{
    switch (id) {
    case BTN_HOME: return L_HOME;
    case BTN_SEQ: return L_LEN;
    case BTN_EDIT: return L_ERASE;
    case BTN_ENV: return L_ACC;
    case BTN_LFO: return L_REP;
    case BTN_FX: return L_PUNCH;
    case BTN_SCL: return L_CHORDS;
    case BTN_ARP: return bm_is_grid(&a->pat, a->track) ? L_ROLL : L_ARP;
    case BTN_SAVE: return L_COPY;
    case BTN_GLO: return L_TONE;
    default: return L_NONE;
    }
}

static void open_layer(bm_app *a, int id)
{
    if (a->layer) return;                                    /* nunca dos capas a la vez */
    int L = layer_of(a, id);
    if (!L) return;
    if (L == L_ARP && app_kind(a, a->track) != KIND_NOTE) return;
    a->layer = (uint8_t)L;
    a->ov.on = 0;
    if (L == L_ARP) app_push(a, CMD_ARP, a->track, 1, 0);
    if (L == L_ERASE) { a->erase_mask = 0; a->erase_syn = 0; }
    if (L == L_LEN) {
        int len = bm_trk_len(&a->pat, a->track, app_kind(a, a->track) == KIND_NOTE ? -1 : 0);
        a->len_page = (uint8_t)((len - 1) / 16);
    }
}

static void close_layer(bm_app *a, int id)
{
    int L = layer_of(a, id);
    if (L == L_ROLL && a->layer == L_ARP) L = L_ARP;
    if (L == L_ARP && a->layer == L_ROLL) L = L_ROLL;
    if (!L || a->layer != L) return;
    if (L == L_PUNCH) { app_push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    if (L == L_ARP) app_push(a, CMD_ARP, a->track, 0, 0);
    if (L == L_ROLL) {
        for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) app_push_i(a, CMD_ROLL, a->track, (uint8_t)l, 0, 0);
        a->roll_mask = 0;
    }
    if (L == L_ERASE) { a->erase_mask = 0; a->erase_syn = 0; }
    a->layer = L_NONE;
}

/* Toque de un botón: su página en el canal donde estás. Otra vez: la siguiente. */
static void button_page(bm_app *a, int id)
{
    if (id == BTN_FX && a->view == V_FXG) { a->fx_page = (uint8_t)((a->fx_page + 1) % 3); return; }   /* DELAY > REVERB > MASTER */
    int t = a->track, kind = app_kind(a, t), midi = app_midi(a, t);
    int cur = a->view == V_CH ? a->page : -1, pg = PG_SEQ;
    switch (id) {
    case BTN_SEQ:
        if (cur == PG_SEQ) pg = bm_is_pads(t) ? PG_BREAK : (kind == KIND_NOTE && !midi) ? PG_LOOP : PG_SEQ;
        else pg = PG_SEQ;
        break;
    case BTN_EDIT:
        if (bm_is_pads(t)) {                                     /* PADS: TRIM > SOUND > ENV */
            if (cur == PG_PADS) a->ed_page[t] = (uint8_t)((a->ed_page[t] + 1) % 4);
            pg = PG_PADS;
        } else if (midi) pg = PG_MIDI;
        else if (kind == KIND_TR) {                              /* SONIDO > INSTR (2 páginas) > SONIDO */
            if (cur == PG_SOUND) { pg = PG_INST; a->ed_page[t] = 0; }
            else if (cur == PG_INST && a->ed_page[t] + 1 < BM_LANE_PAGES) { pg = PG_INST; a->ed_page[t]++; }
            else pg = PG_SOUND;
        } else if (bm_engine_paged(app_engine(a, t))) {          /* SONIDO: la página siguiente */
            int np; bm_engine_pages(app_engine(a, t), &np);
            if (cur == PG_SOUND) a->ed_page[t] = (uint8_t)((a->ed_page[t] + 1) % np);
            pg = PG_SOUND;
        } else pg = PG_SOUND;
        break;
    case BTN_ENV:
        if (bm_is_pads(t)) { a->ed_page[t] = 2; pg = PG_PADS; }
        else if (kind == KIND_TR) { pg = PG_INST; if (cur != PG_INST) a->ed_page[t] = 0; }
        else if (bm_is_pads(t)) { a->ed_page[t] = 2; pg = PG_PADS; }
        else if (midi) pg = PG_MIDI;
        else if (bm_engine_paged(app_engine(a, t))) {             /* ENV NIVEL, otra vez ENV FILTRO */
            int amp = bm_engine_page_of(app_engine(a, t), EPG_AMP), fen = bm_engine_page_of(app_engine(a, t), EPG_FENV);
            a->ed_page[t] = (uint8_t)(cur == PG_SOUND && a->ed_page[t] == amp ? fen : amp);
            pg = PG_SOUND;
        } else { if (cur == PG_ENV) a->env_page ^= 1; else a->env_page = 0; pg = PG_ENV; }
        break;
    case BTN_LFO:
        if (kind == KIND_NOTE && !midi && bm_engine_paged(app_engine(a, t))) { a->ed_page[t] = (uint8_t)bm_engine_page_of(app_engine(a, t), EPG_LFO); pg = PG_SOUND; }
        else pg = PG_LFO;
        break;
    case BTN_FX:
        if (cur == PG_MIX) a->mix_page ^= 1; else a->mix_page = 0;
        pg = PG_MIX;
        break;
    case BTN_SCL: pg = cur == PG_SCL ? PG_CHORDS : PG_SCL; break;
    case BTN_ARP: pg = PG_ARP; break;
    default: return;
    }
    app_enter(a, t, pg);
    a->nav_btn = 0;                                              /* su luz: este botón, mientras sigas aquí */
    if (a->view == V_CH && a->page == pg) {
        a->nav_btn = (uint8_t)(id + 1); a->nav_track = a->track; a->nav_page = a->page; a->nav_sub = a->ed_page[a->track];
    }
}

static void home_combo(bm_app *a, int id)
{
    a->home_used = 1;
    switch (id) {
    case BTN_OCT_DN: app_do_undo(a); break;
    case BTN_OCT_UP: app_do_redo(a); break;
    case BTN_PLAY: app_push(a, CMD_PLAY, app_playing(a) ? 2 : 1, 0, 0); app_toast(a, "FROM STEP 1"); break;
    case BTN_REC: a->song_rec = !a->song_rec; a->song_rec_loops = a->ctx.st.loops; app_toast(a, a->song_rec ? "SONG REC ON" : "SONG REC OFF"); break;
    case BTN_SEQ: app_set_view(a, V_SONG); break;
    case BTN_FX: if (a->view == V_FXG) a->fx_page = (uint8_t)((a->fx_page + 1) % 3); else { a->fx_page = 0; app_set_view(a, V_FXG); } break;
    case BTN_SCL: app_set_view(a, V_SCLG); break;
    case BTN_SAVE: a->save_row = 1; a->save_target = (uint8_t)((a->ctx.st.pattern + 1) % BM_PATTERNS); app_set_view(a, V_SAVE); break;
    case BTN_EDIT: {
        a->copy_src = a->track;
        int k = app_kind(a, a->track);
        a->copy_dst = 0xFF;
        for (int i = 1; i < BM_TRACKS && a->copy_dst == 0xFF; i++) {
            int d = (a->track + i) % BM_TRACKS;
            if (d == 1 || bm_is_pads(a->track)) continue;
            if (app_kind(a, d) == k) a->copy_dst = (uint8_t)d;
        }
        if (a->copy_dst == 0xFF) { app_toast(a, "THE SAMPLER IS NOT COPIED"); break; }
        app_set_view(a, V_COPYCH);
        break;
    }
    default: app_toast(a, "HOME + THIS BUTTON: NOTHING"); break;
    }
}

static void copy_channel(bm_app *a)
{
    int s = a->copy_src, d = a->copy_dst;
    int ks = app_kind(a, s);
    app_undo_snapshot(a, d);
    (void)ks;
    memcpy(&a->clip, app_track_data(a, s), app_track_size(s));
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)d, &a->clip);
    const uint8_t *src = (const uint8_t *)&a->pat.trk[s];
    for (unsigned off = 0; off < sizeof(bm_track); off++)
        if (off != TRK_OFF(engine) && off != TRK_OFF(mute) && off != TRK_OFF(midi_ch)) app_push(a, CMD_SET_TRACK, (uint8_t)d, (uint8_t)off, src[off]);
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, TF("T%d COPIED TO T%d"), s + 1, d + 1);
    app_toast(a, b);
    app_enter(a, d, PG_SEQ);
}

static void ask_yes(bm_app *a)
{
    int k = a->ask, x = a->ask_arg, y = a->ask_arg2;
    a->ask = ASK_NONE;
    char b[30];
    switch (k) {
    case ASK_CLEAR: app_clear_track(a, x); break;
    case ASK_ENGINE:
        a->eng_pending = 0xFF;
        app_undo_snapshot(a, x);
        app_push(a, CMD_ENGINE, (uint8_t)x, (uint8_t)y, 0);
        app_edited(a);
        snprintf(b, sizeof b, TF("T%d IS NOW %s"), x + 1, T(bm_engine_names[y]));
        app_toast(a, b);
        break;
    case ASK_COPY_PAT: app_job(a, JOB_COPY_PAT, x); app_toast(a, "PATTERN COPIED"); break;
    case ASK_COPY_TRK: app_job(a, JOB_COPY_TRK, x | (y << 4)); app_toast(a, "TRACK COPIED"); break;
    case ASK_TAKE_DEL:
        app_job(a, JOB_TAKE_ERASE, x);
        snprintf(b, sizeof b, TF("TAKE %02d DELETED"), x + 1);
        app_toast(a, b);
        break;
    case ASK_KIT: { int n = app_kit_from_take(a, x, y); snprintf(b, sizeof b, TF("KIT: %d PADS"), n); app_toast(a, b); break; }
    case ASK_INIT: app_init_sound(a, x); break;
    case ASK_DISCARD:
        if (a->ctx.st.smp_unsaved && !a->ctx.st.smp_rec) { app_push_ptr(a, CMD_SMP_DISCARD, 0, &a->smp_send[0]); app_toast(a, "TAKE DISCARDED"); }
        break;
    default: break;
    }
}

/* HOME (o GLO otra vez) en una pantalla abierta desde otra: vuelve a ella (al canal, con su página).
 * Devuelve 0 si se llegó con HOME (entonces HOME recorre sus páginas). */
static int view_back(bm_app *a)
{
    int pv = a->prev_view;
    if (pv == V_BOOT || pv == a->view) { a->prev_view = V_BOOT; return 0; }
    if (pv == V_CH) app_enter(a, a->track, a->prev_page);
    else { app_set_view(a, pv); a->prev_view = V_BOOT; }
    return 1;
}

static void tap_tempo(bm_app *a)
{
    if (a->tap_n && a->now - a->tap_t[(a->tap_n - 1) & 3] > 2000) a->tap_n = 0;
    a->tap_t[a->tap_n & 3] = a->now;
    a->tap_n++;
    if (a->tap_n < 2) { app_toast(a, "TAP"); return; }
    int n = a->tap_n > 4 ? 4 : a->tap_n;
    uint32_t first = a->tap_t[(a->tap_n - n) & 3], last = a->tap_t[(a->tap_n - 1) & 3];
    uint32_t iv = (last - first) / (uint32_t)(n - 1);
    if (!iv) return;
    int bpm = bm_clampi((int)(600000u / iv), 400, 2400);
    app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
    char b[16]; snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
    app_overlay(a, -1, "TAP TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, C_CYAN);
}

void app_on_gesture(void *u, int id, bm_gkind k)
{
    bm_app *a = (bm_app *)u;
    if (a->view == V_BOOT) return;
    uint16_t bit = (uint16_t)(1u << id);
    if (id != BTN_HOME && (a->home_mod & bit)) {             /* HOME + botón = su versión global */
        if ((k == G_TAP || k == G_HOLD || k == G_PRESS) && !(a->home_done & bit)) { a->home_done |= bit; home_combo(a, id); }
        return;
    }
    if (a->ask) {                                            /* pregunta: OCT+ sí, HOME no, otro botón la cierra */
        if (id == BTN_OCT_UP && k == G_PRESS) { ask_yes(a); return; }
        if (k == G_PRESS || k == G_TAP || k == G_HOLD) {
            int was = a->ask;
            a->ask = ASK_NONE;
            if (was == ASK_DISCARD && id == BTN_HOME && k == G_TAP) {   /* la toma queda y HOME sale, como siempre */
                app_set_view(a, V_HOME);
                app_toast(a, "TAKE KEPT (NOT SAVED)");
                return;
            }
            if (id == BTN_HOME || id == BTN_OCT_DN) { app_toast(a, "CANCELED"); return; }
        }
    }
    /* pantalla abierta con un toque (MUTE, FILL): HOME la cierra; otro botón la cierra y hace lo suyo.
     * PLAY y OCT -/+ (páginas de pasos) no la cierran. */
    if (a->layer_latch && id != BTN_PLAY && id != BTN_OCT_DN && id != BTN_OCT_UP) {
        if (id == BTN_HOME) {
            if (k == G_TAP) { key_screen_close(a); return; }
            if (k == G_HOLD) key_screen_close(a);            /* y se abre la capa de HOME */
        }
        else if (k == G_PRESS || k == G_TAP || k == G_HOLD) key_screen_close(a);
    }
    char b[30];
    switch (id) {
    case BTN_PLAY:
        if (k != G_PRESS) return;
        if (a->layer == L_TONE) { tap_tempo(a); return; }
        app_push(a, CMD_PLAY, app_playing(a) ? 0 : 1, 0, 0);
        /* con el reloj externo, PLAY espera los pulsos MIDI: sin ellos no avanza nada y parecía
         * colgado (dos veces en el aparato, con CLOCK cambiado sin querer en SYSTEM) */
        if (!app_playing(a) && a->set.clock && !a->ctx.st.ext_ok) app_toast(a, "EXT CLOCK: WAITING FOR MIDI");
        return;
    case BTN_OCT_DN: case BTN_OCT_UP: {
        if (k != G_PRESS) return;
        int d = id == BTN_OCT_UP ? 1 : -1;
        if (a->layer == L_ERASE) { if (d < 0) app_do_undo(a); else app_do_redo(a); return; }
        if (a->layer == L_LEN) {
            a->len_page = (uint8_t)bm_clampi(a->len_page + d, 0, 3);
            snprintf(b, sizeof b, TF("LENGTHS %d-%d"), a->len_page * 16 + 1, a->len_page * 16 + 16); app_toast(a, b);
            return;
        }
        if (a->view == V_COPYCH) { if (d > 0) copy_channel(a); else app_set_view(a, V_CH); return; }
        bool steps = (a->view == V_CH && a->page == PG_SEQ) || a->layer == L_ACC || a->layer == L_REP || a->layer == L_ERASE || a->layer == L_FILL;
        if (steps) {
            int t = a->track, len = bm_trk_len(&a->pat, t, app_kind(a, t) == KIND_NOTE ? -1 : tr_lane(a));
            int pages = (len + 15) / 16;
            a->step_page = (uint8_t)bm_clampi(a->step_page + d, 0, pages > 0 ? pages - 1 : 0);
            snprintf(b, sizeof b, TF("PAGE %d/%d"), a->step_page + 1, pages);
        } else {
            a->oct = (int8_t)bm_clampi(a->oct + d, -2, 2);
            snprintf(b, sizeof b, TF("OCTAVE %+d"), a->oct);
        }
        app_toast(a, b);
        return;
    }
    case BTN_REC:
        if (k == G_TAP) {
            if (a->view == V_CH && a->track == 1 && a->page != PG_PADS) {   /* en PADS, REC graba lo que tocas */
                if (a->page != PG_REC) { app_enter(a, 1, PG_REC); }
                if (a->ctx.st.smp_rec || a->ctx.st.smp_armed) { app_push(a, CMD_SMP_REC, 0, 0, 0); return; }
                uint32_t room = bm_app_rec_room(a);
                if (!room) { app_toast(a, "TAKES FULL: DELETE ONE"); return; }
                app_push_i(a, CMD_SMP_REC, a->rec_now ? 1 : 2, 0, 0, (int32_t)room);
                app_toast(a, a->rec_now ? "RECORDING" : app_playing(a) ? "STARTS ON THE BAR" : "ARMED: PLAY TO RECORD");
            } else if (a->view == V_CH && a->page == PG_LOOP) app_push(a, CMD_LOOP, a->track, 1, 0);
            else if (a->view == V_SCENES) { a->song_rec = !a->song_rec; a->song_rec_loops = a->ctx.st.loops; app_toast(a, a->song_rec ? "SONG REC ON" : "SONG REC OFF"); }
            else {
                a->rec = !a->rec;
                app_push(a, CMD_REC, a->rec, 0, 0);
                if (a->rec) app_undo_snapshot(a, a->track);
                app_toast(a, a->rec ? "REC ON" : "REC OFF");
                if (a->rec) free_rec_start(a);
                else if (a->rec_free) free_rec_close(a);
            }
        } else if (k == G_HOLD) {                           /* grabar solo mientras lo sostienes */
            if (a->view == V_CH && a->page == PG_REC) {     /* en la GRABADORA: la mezcla, ya */
                uint32_t room = bm_app_rec_room(a);
                if (!room) { app_toast(a, "TAKES FULL: DELETE ONE"); return; }
                app_push_i(a, CMD_SMP_REC, 1, 0, 0, (int32_t)room);
                a->rec_hold = 2;
            } else {                                        /* notas, golpes o slices fijos */
                a->rec_before = a->rec; a->rec = 1; a->rec_hold = 1;
                app_push(a, CMD_REC, 1, 0, 0);
                app_undo_snapshot(a, a->track);
            }
        } else if (k == G_RELEASE || k == G_LONG) {
            if (a->rec_hold == 2) app_push(a, CMD_SMP_REC, 0, 0, 0);
            else if (a->rec_hold == 1) { a->rec = a->rec_before; app_push(a, CMD_REC, a->rec, 0, 0); }
            a->rec_hold = 0;
            /* 2 s solo existen en LOOP (bm_app_button): borran el loop. La pista no se borra con
             * REC: eso es EDIT sostenido + CLR. */
            if (k == G_LONG && a->view == V_CH && a->page == PG_LOOP) { app_push(a, CMD_LOOP, a->track, 2, 0); app_toast(a, "LOOP ERASED"); }
        }
        return;
    case BTN_GLO:
        if (k == G_TAP) {
            if (a->view == V_GLO) { if (!view_back(a)) app_set_view(a, V_HOME); }
            else app_set_view(a, V_GLO);
        } else if (k == G_HOLD) open_layer(a, id);
        else if (k == G_RELEASE) close_layer(a, id);
        return;
    case BTN_SAVE:
        if (k == G_TAP) {
            if (a->view == V_SAVE) {
                switch (a->save_row) {
                case 0:
                    app_save_jobs(a, 1);
                    snprintf(b, sizeof b, TF("SAVED A%02d"), a->ctx.st.pattern + 1);
                    app_toast(a, app_playing(a) ? "STOP TO SAVE" : b);
                    break;
                case 1:
                    app_save_jobs(a, 0);
                    app_job(a, JOB_SAVE_AS, a->save_target);
                    snprintf(b, sizeof b, TF("SAVED AS A%02d"), a->save_target + 1);
                    app_toast(a, app_playing(a) ? "STOP TO SAVE" : b);
                    break;
                case 2: app_job(a, JOB_SAVE_SONG, 0); app_toast(a, "SONG AND SCENES SAVED"); break;
                case 3:
                    if (a->clear_armed && a->now - a->clear_t < 3000) {
                        for (int t = 0; t < BM_TRACKS; t++) app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
                        app_edited(a); a->clear_armed = 0;
                        app_toast(a, "PATTERN CLEARED");
                    } else { a->clear_armed = 1; a->clear_t = a->now; app_toast(a, "SAVE AGAIN = CLEAR"); }
                    break;
                default:
                    snprintf(b, sizeof b, TF("%d EVENTS"), bm_pattern_count_events(&a->pat));
                    app_toast(a, b);
                }
            } else app_set_view(a, V_SAVE);
        } else if (k == G_HOLD) open_layer(a, id);
        else if (k == G_RELEASE) close_layer(a, id);
        else if (k == G_LONG) {
            close_layer(a, id);
            app_save_jobs(a, 1);
            app_toast(a, app_playing(a) ? "STOP TO SAVE" : "QUICK SAVE");
        }
        return;
    case BTN_HOME:
        if (k == G_TAP) {
            if (a->view == V_CH && a->page == PG_REC && a->ctx.st.smp_unsaved && !a->ctx.st.smp_rec) {   /* la toma sin guardar: preguntar */
                app_ask(a, ASK_DISCARD, 0, 0, "DISCARD THE NEW TAKE?", "IT IS NOT SAVED YET");
                return;
            }
            if (a->view == V_COPYCH) { app_set_view(a, V_CH); app_toast(a, "CANCELED"); return; }
            if (view_back(a)) return;                        /* una pantalla abierta desde otra: vuelve */
            int nv = V_HOME;                                 /* las páginas de BX-1: MIX > FX > TEMPO > ESCENAS > SONG */
            if (a->view == V_HOME) { nv = V_FXG; a->fx_page = 0; }
            else if (a->view == V_FXG) nv = V_GLO;
            else if (a->view == V_GLO) nv = V_SCENES;
            else if (a->view == V_SCENES) nv = V_SONG;
            app_set_view(a, nv);
            a->prev_view = V_BOOT;                           /* llegaste con HOME: HOME sigue recorriendo */
        } else if (k == G_HOLD) { a->home_used = 0; open_layer(a, id); }
        else if (k == G_RELEASE) {
            close_layer(a, id);
            if (a->pat_sel >= 0) { app_select_pattern(a, a->pat_sel); a->pat_sel = -1; }   /* el patrón que quedó en PRESETS */
            if (a->box_on && a->box_hold == 1) { a->box_on = 0; app_enter(a, a->box_track, a->ch_page[a->box_track]); }
        } else if (k == G_LONG) { close_layer(a, id); app_set_view(a, V_SYSTEM); }
        return;
    default: break;
    }
    if (k == G_TAP) button_page(a, id);
    else if (k == G_HOLD) open_layer(a, id);
    else if (k == G_RELEASE) close_layer(a, id);
    else if (k == G_LONG) {
        close_layer(a, id);
        if (id == BTN_SEQ) { a->time_row = a->track; app_set_view(a, V_TIME); }
        else if (id == BTN_EDIT) {
            if (a->track == 1) app_enter(a, 1, PG_TAKES);
            else { snprintf(b, sizeof b, TF("INIT SOUND ON T%d?"), a->track + 1); app_ask(a, ASK_INIT, a->track, 0, b, "THE SOUND SETTINGS ARE LOST"); }
        }
    }
}

void bm_app_button(bm_app *a, int id, int down, uint32_t now)
{
    a->now = now;
    if (id < 0 || id >= BTN_COUNT) return;
    uint16_t bit = (uint16_t)(1u << id);
    if (down && id != BTN_HOME && bm_gesture_is_down(&a->ges, BTN_HOME)) { a->home_mod |= bit; a->home_done &= (uint16_t)~bit; a->home_used = 1; }
    if (down && id == BTN_REC)                       /* REC 2 s: solo en LOOP (borra el loop) */
        bm_gesture_set_long(&a->ges, BTN_REC, (uint16_t)(a->view == V_CH && a->page == PG_LOOP && !a->layer ? UI_LONG_MS : 0));
    if (down) bm_gesture_down(&a->ges, id, now);
    else bm_gesture_up(&a->ges, id, now);
    if (!down) { a->home_mod &= (uint16_t)~bit; a->home_done &= (uint16_t)~bit; }
}

/* ------------------------------------------------------------ perillas */
static void enc_macro(bm_app *a, int t, int i, int d)
{
    int v = bm_clampi(a->pat.trk[t].macro[i] + d, 0, 127);
    app_set_track(a, t, (uint8_t)i, v);
    if (a->rec && app_playing(a) && app_kind(a, t) == KIND_NOTE)       /* REC: la nota sostenida graba la perilla */
        for (int k = 0; k < BM_KEYS; k++) {
            if (a->key_rec_step[k] < 0 || a->key_trk[k] != t) continue;
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = *bm_evc(&a->pat, t, a->key_rec_step[k], e);
                if (bm_event_empty(&ev)) continue;
                ev.lk[i] = (uint8_t)v;
                app_push_ev(a, t, a->key_rec_step[k], e, &ev);
            }
            a->knob_rec = 1;
        }
    char b[12], ti[22];
    bm_fmt_macro(a, t, i, v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", t + 1, T(bm_macro_name(a, t, i)));
    app_overlay(a, i, ti, b, "", v * 2, -1, bm_knob_color(i));
}

static void enc_track_u8(bm_app *a, int t, int k, uint8_t off, int cur, int d, int lo, int hi, const char *name, int pct_mul)
{
    int v = bm_clampi(cur + d, lo, hi);
    app_set_track(a, t, off, v);
    char b[12], ti[22];
    fmt_pct(v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", t + 1, T(name));
    app_overlay(a, k, ti, b, "", v * pct_mul, -1, k >= 0 ? bm_knob_color(k) : C_WHITE);
}

static void enc_step_grid(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step, lane = tr_lane(a);
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint8_t bb;
    bb = tr->st[lane][s];
    if (!(bb & ST_ON)) { app_toast(a, "EMPTY STEP"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16], ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        int cur = bm_tr_lock_get(tr, s, lane, i);
        if (cur < 0) cur = a->pat.trk[t].macro[i];
        int v = bm_clampi(cur + d, 0, 127);
        app_push_i(a, CMD_TR_LOCK, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (i << 8) | v);
        app_edited(a);
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, T(bm_macro_name(a, t, i)));
        app_overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
        return;
    }
    if (id == ENC_SELECT) {
        bb = d > 0 ? (uint8_t)(bb | ST_ACC) : (uint8_t)(bb & ~ST_ACC);
        snprintf(b, sizeof b, "%s", (bb & ST_ACC) ? "ACCENT" : "NORMAL");
        snprintf(ti, sizeof ti, TF("STEP %02d HIT"), s + 1);
    } else if (id == ENC_ALGO) {
        int r = (bb & ST_FLAM) ? 4 : ((bb & ST_RAT_MASK) >> ST_RAT_SH);
        r = bm_clampi(r + lst(a, d), 0, 4);
        bb = (uint8_t)(bb & ~(ST_RAT_MASK | ST_FLAM));
        if (r == 4) bb |= ST_FLAM; else bb |= (uint8_t)(r << ST_RAT_SH);
        snprintf(b, sizeof b, "%s", RAT_NAMES[r]);
        snprintf(ti, sizeof ti, TF("STEP %02d REPEAT"), s + 1);
    } else {
        int c = (bb & ST_COND_MASK) >> ST_COND_SH;
        c = bm_clampi(c + lst(a, d), 0, 2);
        bb = (uint8_t)((bb & ~ST_COND_MASK) | (c << ST_COND_SH));
        snprintf(b, sizeof b, "%s", COND_NAMES[c]);
        snprintf(ti, sizeof ti, TF("STEP %02d CONDITION"), s + 1);
    }
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, bb);
    app_edited(a);
    app_overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
}

static void enc_step_syn(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step;
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    if (!n) { app_toast(a, "EMPTY STEP"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16] = "", ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        const bm_event *e0 = bm_evc(&a->pat, t, s, idx[0]);
        int base = e0->lk[i] != BM_LOCK_NONE ? e0->lk[i] : a->pat.trk[t].macro[i];
        int v = bm_clampi(base + d, 0, 127);
        for (int j = 0; j < n; j++) { bm_event e = *bm_evc(&a->pat, t, s, idx[j]); e.lk[i] = (uint8_t)v; app_push_ev(a, t, s, idx[j], &e); }
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, T(bm_macro_name(a, t, i)));
        app_overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
        app_edited(a);
        return;
    }
    const char *nm = "";
    for (int j = 0; j < n; j++) {
        bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
        if (id == ENC_SELECT) {                                        /* acento: normal > acento > fantasma */
            int lv = e.vel >= BM_VEL_ACC ? 2 : e.vel <= BM_VEL_GHOST ? 0 : 1;
            lv = bm_clampi(lv + lst(a, d), 0, 2);
            e.vel = lv == 2 ? BM_VEL_ACC : lv == 0 ? BM_VEL_GHOST : BM_VEL_NORMAL;
            if (j == 0) snprintf(b, sizeof b, "%s", lv == 2 ? "ACCENT" : lv == 0 ? "GHOST" : "NORMAL");
            nm = "HIT";
        } else if (id == ENC_ALGO) {
            int p = e.prob == BM_PROB_FILL ? 101 : e.prob == BM_PROB_NOFILL ? 102 : e.prob;
            if (p <= 100) p = bm_clampi(p + d * 5, 0, 102); else p = bm_clampi(p + lst(a, d), 95, 102);
            e.prob = p == 101 ? BM_PROB_FILL : p == 102 ? BM_PROB_NOFILL : (uint8_t)bm_clampi(p, 0, 100);
            if (j == 0) { if (p == 101) snprintf(b, sizeof b, "FILL ONLY"); else if (p == 102) snprintf(b, sizeof b, "NO FILL"); else snprintf(b, sizeof b, "%d%%", e.prob); }
            nm = "PROB/FILL";
        } else {
            int cur = (e.len & BM_LEN_TIE) ? 17 : (e.len & BM_LEN_MASK);
            cur = bm_clampi(cur + d, 1, 17);
            e.len = cur == 17 ? (uint8_t)(1 | BM_LEN_TIE) : (uint8_t)cur;
            if (j == 0) { if (cur == 17) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, TF("%d STEPS"), cur); }
            nm = "LENGTH";
        }
        app_push_ev(a, t, s, idx[j], &e);
    }
    snprintf(ti, sizeof ti, TF("STEP %02d %s"), s + 1, T(nm));
    app_overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
    app_edited(a);
}

static bool enc_global(bm_app *a, int k, int d)
{
    bm_meta *m = &a->pat.meta;
    char b[16];
    switch (k) {
    case 0: {
        int bpm = bm_clampi(m->bpm_x10 + d * 10, 400, 2400);
        app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
        snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
        app_overlay(a, -1, "TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, C_TEMPO);
        a->ov.beat = 1;
        return true;
    }
    case 1: {
        int sw = bm_clampi(m->swing + lst(a, d), 50, 75);
        app_set_meta(a, META_OFF(swing), sw);
        snprintf(b, sizeof b, "%d", sw);
        app_overlay(a, 1, "SWING  50=STRAIGHT", b, "%", (sw - 50) * 255 / 25, -1, bm_knob_color(1));
        return true;
    }
    case 2: {
        int i = 0;
        while (i < 6 && bm_master_steps_tab[i] != m->master) i++;
        i = bm_clampi((i >= 6 ? 3 : i) + lst(a, d), 0, 5);
        app_set_meta(a, META_OFF(master), bm_master_steps_tab[i]);
        fmt_master(bm_master_steps_tab[i], b, sizeof b);
        app_overlay(a, 2, "MASTER LENGTH", b, bm_master_steps_tab[i] ? "STEPS" : "", i * 51, -1, bm_knob_color(2));
        return true;
    }
    case 3:
        a->set.drive = (uint8_t)bm_clampi(a->set.drive + d, 0, 127);
        app_push(a, CMD_DRIVE, a->set.drive, 0, 0);
        a->set_dirty = 1; a->t_edit = a->now;
        fmt_pct(a->set.drive, b, sizeof b);
        app_overlay(a, 3, "DRIVE (SOFT-CLIP)", b, "", a->set.drive * 2, -1, bm_knob_color(3));
        return true;
    default: return false;
    }
}

static void enc_fill_every(bm_app *a, int d)
{
    static const uint8_t FE[4] = {0, 2, 4, 8};
    bm_meta *m = &a->pat.meta;
    char b[16];
    int i = 0;
    while (i < 4 && FE[i] != m->fill_every) i++;
    i = bm_clampi((i >= 4 ? 0 : i) + lst(a, d), 0, 3);
    app_set_meta(a, META_OFF(fill_every), FE[i]);
    if (FE[i]) snprintf(b, sizeof b, TF("EVERY %d"), FE[i]); else snprintf(b, sizeof b, "OFF");
    app_overlay(a, -1, "AUTOMATIC FILL", b, FE[i] ? "BARS" : "", i * 85, -1, C_WHITE);
}

static void enc_transpose(bm_app *a, int d)
{
    int tp = bm_clampi(a->pat.meta.transpose + d, 0, 48);
    app_set_meta(a, META_OFF(transpose), tp);
    char b[12]; snprintf(b, sizeof b, "%+d", tp - 24);
    app_overlay(a, -1, "PROJECT TRANSPOSE", b, "ST", tp * 5, 120, C_CYAN);
}

/* REC expresivo en el paso marcado (docs/DISENO_0.6.md §8): el largo del último paso y el corrimiento */
/* en pantalla: 100 = TIE (liga con la siguiente), 95 = el paso entero (gate 0), 10..90 = el porcentaje */
static int ev_gate_ui(const bm_event *e) { return (e->len & BM_LEN_TIE) ? 100 : e->gate ? e->gate : 95; }

static void enc_rec(bm_app *a, int id, int d)
{
    int t = a->track, len = bm_trk_len(&a->pat, t, -1);
    char b[16];
    if (a->rec_step < 0 || a->rec_step >= len) a->rec_step = (int8_t)bm_clampi(a->step_page * 16, 0, len - 1);
    if (id == ENC_SELECT) {
        a->rec_step = (int8_t)bm_clampi(a->rec_step + lst(a, d), 0, len - 1);
        a->step_page = (uint8_t)(a->rec_step / 16);
        snprintf(b, sizeof b, "%d", a->rec_step + 1);
        app_overlay(a, -1, "REC STEP", b, "", a->rec_step * 255 / (len > 1 ? len - 1 : 1), -1, C_RED);
        return;
    }
    int s = a->rec_step, n = 0, shown = 0;
    for (int e = 0; e < BM_EVENTS; e++) {
        bm_event ev = *bm_evc(&a->pat, t, s, e);
        if (bm_event_empty(&ev)) continue;
        if (id == ENC_ALGO) {
            int g = bm_clampi(ev_gate_ui(&ev) + 5 * lst(a, d), 10, 100);
            ev.len = (uint8_t)((ev.len & BM_LEN_MASK) | (g >= 100 ? BM_LEN_TIE : 0));
            ev.gate = (uint8_t)(g >= 95 ? 0 : g);
            shown = g;
        } else {
            ev.micro = (int8_t)bm_clampi(ev.micro + lst(a, d), -11, 11);
            shown = ev.micro;
        }
        app_push_ev(a, t, s, e, &ev);
        n++;
    }
    if (!n) { app_toast(a, "EMPTY STEP: PLAY A NOTE THERE FIRST"); return; }
    app_edited(a);
    if (id == ENC_ALGO) {
        a->rec_gate = (uint8_t)shown;
        if (shown >= 100) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "%d", shown);
        app_overlay(a, -1, "GATE LENGTH", b, shown >= 100 ? "" : "%", shown * 255 / 100, -1, C_GREEN);
    } else {
        a->rec_micro = (int8_t)shown;
        snprintf(b, sizeof b, "%+d", shown);
        app_overlay(a, -1, "MICRO TIMING", b, "TICKS", (shown + 11) * 255 / 22, 128, C_ORANGE);
    }
}

static void enc_bpm_fine(bm_app *a, int d)
{
    int bpm = bm_clampi(a->pat.meta.bpm_x10 + lst(a, d), 400, 2400);
    app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
    char b[12]; snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
    app_overlay(a, -1, "TEMPO (FINE)", b, "BPM", (bpm - 400) * 255 / 2000, -1, C_TEMPO);
    a->ov.beat = 1;
}

/* Controles bipolares (MOVE, filtro DJ, PAN): el centro retiene 2 detents antes de salir, y un giro
 * que lo cruza se para en él (como la zona OPEN del filtro del master). */
static int center_stop(bm_app *a, int cur, int d, int c, int lo, int hi)
{
    int nv = bm_clampi(cur + d, lo, hi);
    if (a->stick_id != a->enc_cur || a->enc_new) { a->stick = 0; a->stick_id = a->enc_cur; }
    if (cur != c) {
        a->stick = 0;
        return (cur < c && nv > c) || (cur > c && nv < c) ? c : nv;
    }
    int r = lst(a, d);
    if (a->stick && (a->stick > 0) != (r > 0)) a->stick = 0;
    a->stick = (int8_t)bm_clampi(a->stick + r, -20, 20);
    if (a->stick >= -2 && a->stick <= 2) return c;
    int out = a->stick > 0 ? a->stick - 2 : a->stick + 2;
    a->stick = 0;
    return bm_clampi(c + out, lo, hi);
}

/* filtro DJ: posición -63..63 (0 abierto) <-> djf (0 abierto, 1-63 pasa-bajos, 65-127 pasa-altos) */
static int djf_pos(uint8_t djf) { return !djf || djf == 64 ? 0 : djf < 64 ? -(int)djf : (int)djf - 64; }
static uint8_t djf_of(int pos) { return (uint8_t)(pos < 0 ? -pos : pos > 0 ? 64 + pos : 0); }
static void fmt_djf(uint8_t djf, char *b, int cap)
{
    int p = djf_pos(djf);
    if (!p) snprintf(b, (size_t)cap, "OPEN");
    else snprintf(b, (size_t)cap, "%s %d", p < 0 ? "LP" : "HP", p < 0 ? -p : p);
}

/* HOME (0.7), K1-K4: el canal del cursor como una tira de mezclador: nivel, filtro DJ, delay, reverb */
static void enc_home(bm_app *a, int k, int d)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[16], ti[22];
    switch (k) {
    case 0:
        snprintf(ti, sizeof ti, TF("T%d LEVEL"), t + 1);
        enc_track_u8(a, t, 0, TRK_OFF(level), tr->level, d, 0, 127, ti, 2);
        return;
    case 1: {
        int p = center_stop(a, djf_pos(tr->djf), d, 0, -63, 63);
        app_set_track(a, t, TRK_OFF(djf), djf_of(p));
        a->pat.trk[t].djf = djf_of(p);
        fmt_djf(djf_of(p), b, sizeof b);
        snprintf(ti, sizeof ti, TF("T%d FILTER"), t + 1);
        app_overlay(a, 1, ti, b, "", (p + 64) * 2, 128, bm_knob_color(1));
        return;
    }
    case 2:
        snprintf(ti, sizeof ti, TF("T%d DELAY"), t + 1);
        enc_track_u8(a, t, 2, TRK_OFF(send), tr->send, d, 0, 127, ti, 2);
        return;
    case 3:
        snprintf(ti, sizeof ti, TF("T%d REVERB"), t + 1);
        enc_track_u8(a, t, 3, TRK_OFF(rsend), tr->rsend, d, 0, 127, ti, 2);
        return;
    default: return;
    }
}

/* HOME, PRESETS = MOVE (0.7): una perilla que mueve toda la mezcla. A la izquierda cierra el filtro
 * del master y manda la mezcla al eco; a la derecha abre el pasa-altos y la lava en la reverb. En
 * vivo: no se guarda. Los patrones se eligen con las blancas. */
static void enc_move(bm_app *a, int d)
{
    a->move = (int8_t)center_stop(a, a->move, d, 0, -63, 63);
    app_push(a, CMD_MOVE, (uint8_t)a->move, 0, 0);
    char b[8];
    snprintf(b, sizeof b, "%+d", a->move);
    app_overlay(a, -1, a->move < 0 ? "MOVE  DARK + ECHO" : a->move > 0 ? "MOVE  BRIGHT + WASH" : "MOVE  OFF",
                a->move ? b : "0", "", (a->move + 64) * 2, 128, C_PINK);
}

static void box_turn_in(bm_app *a, int d, int lane);

/* ALGORITHM dentro de un canal: el modo de su máquina (docs/DISENO_0.6.md §5.3) */
static void enc_mode(bm_app *a, int d)
{
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    char b[20];
    switch (tr->engine) {
    case ENG_DRUM: box_turn_in(a, d, 1); return;                         /* el instrumento */
    case ENG_BREAK: {                                                     /* el pad */
        int p = bm_clampi((a->sel_lane[1] & 15) + lst(a, d), 0, BM_LANES - 1);
        a->sel_lane[1] = (uint8_t)p;
        app_wave_view(a, a->pat.pad[p].src, a->pat.pad[p].start, a->cut_zoom);
        snprintf(b, sizeof b, TF("PAD %d"), p + 1);
        app_toast(a, b);
        return;
    }
    case ENG_ACID: case ENG_MINI: app_bass_machine(a, d > 0 ? ENG_MINI : ENG_ACID); return;
    case ENG_FM: {                                                        /* el algoritmo, 1..32 */
        int alg = bm_clampi(bm_fm_alg_of(tr, tr->macro[3]) + lst(a, d), 1, 32);
        app_set_track(a, t, 3, (alg - 1) * 4 + 1);
        snprintf(b, sizeof b, "%d", alg);
        app_overlay(a, -1, "BX-7 ALGORITHM", b, "/ 32", (alg - 1) * 8, -1, bm_track_color(t));
        return;
    }
    case ENG_BITTAL: {                                                    /* la tabla del oscilador 1 */
        int n = bm_snap_count(ENG_BITTAL);
        int v = bm_clampi(a->pat.prm[t][BT_TAB1] + lst(a, d), 0, n);
        app_push(a, CMD_SET_PRM, (uint8_t)t, BT_TAB1, (uint8_t)v);
        a->pat.prm[t][BT_TAB1] = (uint8_t)v;
        app_edited(a);
        app_overlay(a, -1, "OSC 1 TABLE", v ? bm_snap_name(ENG_BITTAL, v - 1) : "OWN", "", v * 255 / (n ? n : 1), -1, bm_track_color(t));
        return;
    }
    case ENG_BUNO: {                                                      /* el chorus */
        int v = wrap(a->pat.prm[t][BN_CHORUS] + lst(a, d), BN_CHORUSES);
        app_push(a, CMD_SET_PRM, (uint8_t)t, BN_CHORUS, (uint8_t)v);
        a->pat.prm[t][BN_CHORUS] = (uint8_t)v;
        app_edited(a);
        app_overlay(a, -1, "BUNO-6 CHORUS", bm_bn_chorus_names[v], "", v * 85, -1, bm_track_color(t));
        return;
    }
    default: return;
    }
}

static void enc_pattern(bm_app *a, int d)
{
    int base = a->pat_sel >= 0 ? a->pat_sel : a->ctx.st.pattern;
    a->pat_sel = (int8_t)bm_clampi(base + lst(a, d), 0, BM_PATTERNS - 1);
    a->pat_sel_t = a->now;
    char b[8]; snprintf(b, sizeof b, "A%02d", a->pat_sel + 1);
    app_overlay(a, -1, app_playing(a) ? "PATTERN (QUEUED)" : "PATTERN", b, "", a->pat_sel * 17, -1, C_WHITE);
}

/* ALGORITHM en HOME: un recuadro dice a qué canal vas; al rato (o al soltar HOME) entras. */
static void box_commit(bm_app *a)
{
    a->box_on = 0;
    if (a->box_mode == BOX_LANE) {                            /* el instrumento elegido, y se escucha */
        int t = a->track;
        a->sel_lane[t] = a->box_lane;
        app_push(a, CMD_NOTE_ON, (uint8_t)t, a->box_lane, 100);
        char b[24]; snprintf(b, sizeof b, "%d %s", a->box_lane + 1, T(bm_lane_name(a, t, a->box_lane)));
        app_toast(a, b);
    } else if (a->box_mode == BOX_CH) app_select_track(a, a->box_track, 1);
    else app_enter(a, a->box_track, a->ch_page[a->box_track]);
}

/* Dentro de un canal ALGORITHM también pasa por el recuadro: se ve a dónde vas y se entra al
 * soltar (el tiempo de ENTRADA de GLO). En INSTR recorre los 16 instrumentos. */
static void box_turn_in(bm_app *a, int d, int lane)
{
    if (!a->box_on || a->box_mode != (lane ? BOX_LANE : BOX_CH)) {
        a->box_track = a->track; a->box_lane = (uint8_t)tr_lane(a);
        a->box_mode = (uint8_t)(lane ? BOX_LANE : BOX_CH);
    }
    if (lane) a->box_lane = (uint8_t)bm_clampi(a->box_lane + lst(a, d), 0, BM_LANES - 1);
    else a->box_track = (uint8_t)bm_clampi(a->box_track + lst(a, d), 0, BM_TRACKS - 1);
    int ms = bm_app_enter_ms(a);
    a->box_on = 1; a->box_hold = 0;
    if (ms <= 0) { box_commit(a); return; }                  /* sin espera (u OFF): directo */
    a->box_until = a->now + (uint32_t)ms;
}

static void box_turn(bm_app *a, int d, int hold)
{
    if (!a->box_on || a->box_mode != BOX_HOME) { a->box_track = a->track; a->box_mode = BOX_HOME; }
    a->box_track = (uint8_t)bm_clampi(a->box_track + lst(a, d), 0, BM_TRACKS - 1);
    int ms = bm_app_enter_ms(a);
    a->box_on = 1;
    if (hold) { a->box_hold = 1; return; }
    if (ms == 0) { a->box_on = 0; app_enter(a, a->box_track, a->ch_page[a->box_track]); return; }
    if (ms < 0) { a->box_hold = 2; a->track = a->box_track; a->box_until = a->now + 700; return; }
    a->box_hold = 0;
    a->box_until = a->now + (uint32_t)ms;
}

static void enc_source(bm_app *a, int t, int d)
{
    char b[16];
    int kind = app_kind(a, t);
    if (app_midi(a, t)) {
        int v = bm_clampi(a->pat.trk[t].macro[1] + lst(a, d), 0, 127);
        app_set_track(a, t, 1, v);
        app_push(a, CMD_MIDI_SEND, (uint8_t)(0xC0 | (a->pat.trk[t].midi_ch & 15)), (uint8_t)v, 0xFF);
        snprintf(b, sizeof b, "%d", v + 1);
        app_overlay(a, -1, "MIDI PROGRAM", b, "", v * 2, -1, bm_track_color(t));
        return;
    }
    if (bm_is_pads(t)) {                                         /* la toma entera en el pad elegido */
        uint8_t list[BM_TAKES + 8];
        int n = bm_app_sources(a, list), i = 0, pad = a->sel_lane[1] & 15;
        if (!n) { app_toast(a, "NO TAKE"); return; }
        while (i < n && list[i] != a->pat.pad[pad].src) i++;
        i = i >= n ? 0 : wrap(i + lst(a, d), n);
        app_pad_take(a, pad, list[i]);
        char ti[16]; snprintf(ti, sizeof ti, TF("PAD %d"), pad + 1);
        bm_app_src_name(a, list[i], b, sizeof b);
        app_overlay(a, -1, ti, b, "", i * 255 / (n > 1 ? n - 1 : 1), -1, bm_track_color(1));
        return;
    }
    int lim = bm_sound_count(a, t), e = a->pat.trk[t].engine;
    int v = wrap(a->pat.trk[t].source % lim + lst(a, d), lim);
    if (kind == KIND_TR && a->enc_new) app_undo_snapshot(a, t);   /* el kit de antes: HOME + OCT- (una vez por giro) */
    if (kind == KIND_TR || bm_engine_paged(e)) { app_push(a, CMD_PRESET, (uint8_t)t, (uint8_t)v, 0); app_edited(a); }
    else app_set_track(a, t, TRK_OFF(source), v);
    a->pat.trk[t].source = (uint8_t)v;
    app_overlay(a, -1, kind == KIND_TR ? "KIT" : e == ENG_ACID ? "BB-303 WAVE" : e == ENG_FM ? "BX-7 PATCH" : "SOUND",
                bm_sound_name(a, t), "", v * 255 / (lim > 1 ? lim - 1 : 1), -1, bm_track_color(t));
}

static void more_change(bm_app *a, int d)
{
    uint8_t rows[10];
    int n = more_rows(a, a->track, rows), t = a->track;
    if (a->more_row >= n) a->more_row = (uint8_t)(n - 1);
    const bm_track *tk = &a->pat.trk[t];
    char b[30];
    switch (rows[a->more_row]) {
    case ROW_ENGINE:                                            /* a la derecha B-MINI, a la izquierda BB-303 */
        if ((d > 0) != (tk->engine == ENG_MINI)) app_bass_machine(a, d > 0 ? ENG_MINI : ENG_ACID);
        return;
    case ROW_MIDIOUT:                                           /* a la derecha ON, a la izquierda OFF */
        if ((d > 0) == (tk->midi_out != 0)) return;
        app_set_track(a, t, TRK_OFF(midi_out), d > 0);
        app_toast(a, d > 0 ? "MIDI OUT ON: NOTES ALSO GO OUT" : "MIDI OUT OFF");
        return;
    case ROW_ACC: enc_track_u8(a, t, -1, TRK_OFF(accent), tk->accent, d, 0, 127, "ACCENT", 2); return;
    case ROW_LEN: set_len(a, t, bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? 0 : -1) + lst(a, d)); return;
    case ROW_SPEED: {
        int v = bm_clampi(tk->speed + -lst(a, d), 0, SPD_COUNT - 1);
        app_set_track(a, t, TRK_OFF(speed), v);
        snprintf(b, sizeof b, TF("T%d SPEED"), t + 1);
        app_overlay(a, -1, b, bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, C_WHITE);
        return;
    }
    case ROW_TRANSP: {
        int v = bm_clampi(tk->transpose + lst(a, d), 0, 48);
        app_set_track(a, t, TRK_OFF(transpose), v);
        char vb[12]; fmt_semi(v, vb, sizeof vb);
        snprintf(b, sizeof b, TF("T%d TRANSPOSE"), t + 1);
        app_overlay(a, -1, b, vb, "", v * 5, 120, C_WHITE);
        return;
    }
    case ROW_MIDICH: {
        int v = bm_clampi(tk->midi_ch + lst(a, d), 0, 15);
        app_set_track(a, t, TRK_OFF(midi_ch), v);
        char vb[8]; snprintf(vb, sizeof vb, "%d", v + 1);
        app_overlay(a, -1, "MIDI CHANNEL", vb, "", v * 17, -1, C_WHITE);
        return;
    }
    case ROW_INTERP: if ((d > 0) != (tk->interp != 0)) app_set_track(a, t, TRK_OFF(interp), d > 0); return;   /* derecha HERMITE */
    default:
        snprintf(b, sizeof b, TF("INIT SOUND ON T%d?"), t + 1);
        app_ask(a, ASK_INIT, t, 0, b, "THE SOUND SETTINGS ARE LOST");
        return;
    }
}

/* Las sub-páginas de la página del canal (las que recorre SELECT antes de cambiar de pestaña): las
 * páginas del editor (SOUND, INSTR), las de PADS, ENV y su ENV FILTER, MIX y su DUCK, y las filas de MORE. */
static uint8_t *sub_page(bm_app *a, int *n)
{
    int t = a->track;
    *n = 0;
    if (a->view != V_CH) return NULL;
    switch (a->page) {
    case PG_SOUND: case PG_INST:
        if (!bm_app_ed_on(a) || !bm_app_ed_pages(a, t, n)) { *n = 0; return NULL; }
        return &a->ed_page[t];
    case PG_PADS: *n = 4; return &a->ed_page[1];
    case PG_ENV: *n = 2; return &a->env_page;
    case PG_MIX: *n = 2; return &a->mix_page;
    case PG_MORE: *n = bm_app_more_count(a); return &a->more_row;
    default: return NULL;
    }
}

static void tabs_turn(bm_app *a, int d)
{
    uint8_t tabs[8];
    int n = bm_app_tabs(a, a->track, tabs), i = 0;
    while (i < n && tabs[i] != a->page) i++;
    if (i >= n) i = d > 0 ? 0 : n - 1;
    else i = (i + (d > 0 ? 1 : n - 1)) % n;
    app_enter(a, a->track, tabs[i]);
    int sn;
    uint8_t *sp = sub_page(a, &sn);                 /* a la derecha entra por la primera, a la izquierda por la última */
    if (sp && sn > 0) *sp = (uint8_t)(d > 0 || a->page == PG_MORE ? 0 : sn - 1);   /* MORE, siempre arriba */
}

/* SELECT en un canal: de a un detent, la sub-página siguiente y, en el borde, la pestaña de al lado */
static void select_turn(bm_app *a, int d)
{
    int r = lst(a, d), s = r > 0 ? 1 : -1;
    for (int i = 0; i < (r > 0 ? r : -r); i++) {
        int n;
        uint8_t *sp = sub_page(a, &n);
        if (sp && n > 0 && *sp % n + s >= 0 && *sp % n + s < n) *sp = (uint8_t)(*sp % n + s);
        else tabs_turn(a, s);
    }
    if (a->page == PG_MORE) { a->ask = ASK_NONE; a->eng_pending = 0xFF; }
}

static void system_change(bm_app *a, int d)
{
    bm_settings *s = &a->set;
    /* los interruptores: a la derecha = encendido (o el siguiente), a la izquierda = apagado; nunca
     * dan la vuelta, así que girar de más no los cambia otra vez */
    int on = d > 0;
    switch (a->menu_row) {
    case 0:                                                     /* derecha EXTERNAL, izquierda INTERNAL */
        if (on == s->clock) return;
        s->clock = (uint8_t)on; app_push(a, CMD_CLOCK, s->clock, 0, 0);
        app_toast(a, s->clock ? "CLOCK: EXTERNAL (MIDI)" : "CLOCK: INTERNAL");
        break;
    case 1: if (on == s->sync_mode) return; s->sync_mode = (uint8_t)on; break;
    case 2: {
        static const uint8_t PQ[3] = {1, 2, 4};
        int i = s->sync_ppqn == 4 ? 2 : s->sync_ppqn == 2 ? 1 : 0;
        s->sync_ppqn = PQ[bm_clampi(i + lst(a, d), 0, 2)];
        break;
    }
    case 3: s->sync_lat = (uint8_t)bm_clampi(s->sync_lat + lst(a, d), 0, 20); break;
    case 4: if (on == s->autosave) return; s->autosave = (uint8_t)on; break;
    case 5: s->led_dim = (uint8_t)bm_clampi(s->led_dim + lst(a, d), 0, 2); break;
    case 6:
        if (on == s->fast_lcd) return;
        s->fast_lcd = (uint8_t)on;
        app_toast(a, s->fast_lcd ? "FAST SCREEN ON (TEST)" : "FAST SCREEN OFF");
        break;
    case 7:
        if (on == s->dual_core) return;
        s->dual_core = (uint8_t)on;
        app_toast(a, s->dual_core ? "TWO CORES ON (TEST)" : "TWO CORES OFF");
        break;
    case 9:                                                     /* idioma: derecha el paquete cargado, izquierda inglés */
        if (on && !bm_app_lang_pack(a)) { app_toast(a, T("NO LANGUAGE PACK: LOAD ONE BY USB")); return; }
        if (on == s->lang) return;
        s->lang = (uint8_t)on;
        bm_lang_use(s->lang ? bm_app_lang_pack(a) : NULL);
        break;
    default: return;
    }
    app_push(a, CMD_SYNC, s->sync_mode, s->sync_ppqn, s->sync_lat);
    a->set_dirty = 1; a->t_edit = a->now;
}

static void glo_change(bm_app *a, int d)
{
    switch (a->glo_row) {
    case 0: case 1: case 2: case 3: enc_global(a, a->glo_row, d); break;
    case 4: enc_fill_every(a, d); break;
    default: enc_transpose(a, d); break;
    }
}

static void enc_channel(bm_app *a, int id, int d)
{
    int t = a->track, kind = app_kind(a, t), k = id - ENC_K1;
    const bm_track *tr = &a->pat.trk[t];
    bm_meta *m = &a->pat.meta;
    char b[16], ti[22];
    if (id == ENC_PRESETS) { enc_source(a, t, d); return; }
    if (id == ENC_SELECT) { select_turn(a, d); return; }
    if (id == ENC_ALGO) {
        if (a->page == PG_MORE) { more_change(a, d); return; }
        if (a->page == PG_TAKES) {
            uint8_t list[BM_TAKES + 8];
            int n = bm_app_sources(a, list);
            if (!n) return;
            a->takes_row = (uint8_t)bm_clampi(a->takes_row + lst(a, d), 0, n - 1);
            return;
        }
        if (a->page == PG_REC) { app_toast(a, "THE SAMPLER STAYS HERE"); return; }
        enc_mode(a, d);
        return;
    }
    if (k < 0 || k > 3) return;
    switch (a->page) {
    case PG_SEQ:
        if (app_midi(a, t)) goto midi;
        enc_macro(a, t, k, d);
        return;
    case PG_SOUND:
        if (kind == KIND_TR) { enc_macro(a, t, k, d); return; }
        if (bm_engine_paged(tr->engine)) { ed_turn(a, t, k, d); return; }
        if (k == 3) { enc_track_u8(a, t, 3, TRK_OFF(accent), tr->accent, d, 0, 127, "ACCENT", 2); return; }
        enc_macro(a, t, k, d);                                 /* K1-K3 como en SEQ */
        return;
    case PG_INST: ed_turn(a, t, k, d); return;
    case PG_MIX: goto mix;
    case PG_MORE:
        if (k == 0) set_len(a, t, bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? tr_lane(a) : -1) + lst(a, d));
        else if (k == 1) { int v = bm_clampi(tr->speed + -lst(a, d), 0, SPD_COUNT - 1); app_set_track(a, t, TRK_OFF(speed), v); app_overlay(a, 1, "SPEED", bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, bm_knob_color(1)); }
        else if (k == 2) enc_track_u8(a, t, 2, TRK_OFF(accent), tr->accent, d, 0, 127, "ACCENT", 2);
        else if (kind == KIND_NOTE) { int v = bm_clampi(tr->transpose + lst(a, d), 0, 48); app_set_track(a, t, TRK_OFF(transpose), v); fmt_semi(v, b, sizeof b); app_overlay(a, 3, "TRANSPOSE", b, "", v * 5, 120, bm_knob_color(3)); }
        return;
    case PG_BREAK: enc_track_u8(a, 1, k, (uint8_t)(TRK_OFF(gen) + k), tr->gen[k], d, 0, 127, GEN_NAMES[k], 2); return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (src == BM_SRC_NONE || (src & BM_SRC_ROM)) { app_toast(a, "FACTORY IS READ ONLY"); return; }
        bm_take *tk = &a->take[src];
        if (k == 0) { tk->bpm_x10 = (uint16_t)bm_clampi(tk->bpm_x10 + d * 10, 400, 2400); snprintf(b, sizeof b, "%d", tk->bpm_x10 / 10); app_overlay(a, 0, "TAKE BPM", b, "BPM", (tk->bpm_x10 - 400) * 255 / 2000, -1, bm_knob_color(0)); }
        else if (k == 1) { tk->bars = (uint8_t)bm_clampi(tk->bars + lst(a, d), 1, 8); snprintf(b, sizeof b, "%d", tk->bars); app_overlay(a, 1, "BARS", b, "", tk->bars * 31, -1, bm_knob_color(1)); }
        else if (k == 2) { tk->gain = (uint8_t)bm_clampi(tk->gain + d, 8, 127); snprintf(b, sizeof b, "%+d", ((int)tk->gain - 64) * 6 / 64); app_overlay(a, 2, "GAIN", b, "DB", tk->gain * 2, 128, bm_knob_color(2)); }
        else return;
        take_changed(a, src);
        return;
    }
    case PG_ENV:
        if (a->env_page == 0) {
            static const uint8_t OFFS[4] = {TRK_OFF(attack), 2, TRK_OFF(sustain), TRK_OFF(release)};
            const uint8_t cur[4] = {tr->attack, tr->macro[2], tr->sustain, tr->release};
            int nv = bm_clampi(cur[k] + d, 0, 127);
            app_set_track(a, t, OFFS[k], nv);
            if (k == 0) fmt_ms(bm_attack_ms(nv), b, sizeof b); else if (k == 2) fmt_pct(nv, b, sizeof b); else fmt_ms(bm_decay_ms(nv), b, sizeof b);
            static const char *const NM[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
            snprintf(ti, sizeof ti, "T%d %s", t + 1, T(NM[k]));
            app_overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
        } else if (k < 2) {
            const uint8_t cur = k == 0 ? tr->env_amt : tr->glide;
            int nv = bm_clampi(cur + d, 0, 127);
            app_set_track(a, t, k == 0 ? TRK_OFF(env_amt) : TRK_OFF(glide), nv);
            if (k == 0) fmt_pct(nv, b, sizeof b); else fmt_ms((uint32_t)nv * 500 / 127, b, sizeof b);
            snprintf(ti, sizeof ti, "T%d %s", t + 1, T(k == 0 ? "ENV > FILTER" : "GLIDE"));
            app_overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
        }
        return;
    case PG_ARP:
        if (kind != KIND_NOTE) {
            if (k == 0) { a->roll_rate = (uint8_t)bm_clampi(a->roll_rate + lst(a, d), 0, 4); app_overlay(a, 0, "ROLL", bm_roll_names[a->roll_rate], "", a->roll_rate * 63, -1, bm_knob_color(0)); }
            return;
        }
        goto arp;
    case PG_LOOP:
        if (k == 0 && (d > 0) != (a->pat.loop[t - 2].on != 0)) { app_push(a, CMD_LOOP, (uint8_t)t, 3, 0); app_edited(a); }   /* derecha ON */
        if (k == 1) {
            const bm_loop *lp = &a->pat.loop[t - 2];
            int nb = lp->bars == 1 ? (d > 0 ? 2 : 1) : lp->bars == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 4 : 2);
            app_push(a, CMD_LOOP, (uint8_t)t, 4, (uint8_t)nb); app_edited(a);
            snprintf(b, sizeof b, "%d", nb);
            app_overlay(a, 1, "LOOP BARS", b, "", nb * 63, -1, bm_knob_color(1));
        }
        return;
    case PG_MIDI: goto midi;
    case PG_SCL: {
        const char *val = "";
        if (k == 0) {
            if (tr->scl == 0xFF) { int nv = wrap(m->root + lst(a, d), 12); app_set_meta(a, META_OFF(root), nv); val = bm_note_names[nv]; }
            else { int nv = wrap(tr->scl_root + lst(a, d), 12); app_set_track(a, t, TRK_OFF(scl_root), nv); val = bm_note_names[nv]; }
        } else if (k == 1) {
            int cur = tr->scl == 0xFF ? -1 : tr->scl;
            int nv = bm_clampi(cur + lst(a, d), -1, BM_SCALES - 1);
            if (cur == -1 && nv == 0) app_set_track(a, t, TRK_OFF(scl_root), m->root);
            app_set_track(a, t, TRK_OFF(scl), nv < 0 ? 0xFF : nv);
            val = nv < 0 ? "PROJECT" : bm_scale_names[nv];
        } else if (k == 2) { int nv = wrap(m->voicing + lst(a, d), 5); app_set_meta(a, META_OFF(voicing), nv); val = EXT_NAMES[nv]; }
        else { int nv = d > 0; if (nv != m->scale_lock) app_set_meta(a, META_OFF(scale_lock), nv); val = nv ? "ON" : "OFF"; }
        static const char *const NM[4] = {"ROOT", "TRACK SCALE", "BASE CHORD", "KEYS IN SCALE"};
        app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        return;
    }
    case PG_CHORDS: goto chords;
    case PG_REC: if (k == 0 && (d > 0) != (a->rec_now != 0)) { a->rec_now = d > 0; app_toast(a, a->rec_now ? "STARTS WHEN YOU PRESS REC" : "STARTS ON THE BAR"); } return;
    case PG_PADS:
        if (a->ed_page[1] == 0) {                                /* TRIM: START LENGTH ZOOM SNAP */
            if (k <= 1) pad_trim(a, k, d);
            else if (k == 2) a->cut_zoom = (uint8_t)bm_clampi(a->cut_zoom + lst(a, d), 0, 3);
            else a->cut_snap = (uint8_t)bm_clampi(a->cut_snap + lst(a, d), 0, 2);
        } else pad_sound(a, k, d);
        return;
    case PG_LFO: app_toast(a, "LFO: IN A FUTURE VERSION"); return;
    default: return;
    }
mix:
    {
        if (a->mix_page && a->view == V_CH) {                   /* FX otra vez: DUCK */
            if (k) return;
            if (t == 0) { app_overlay(a, 0, "DUCK", "T1 IS THE SOURCE", "", 0, -1, bm_knob_color(0)); return; }
            enc_track_u8(a, t, 0, TRK_OFF(duck), tr->duck, d, 0, 127, "DUCK (SIDECHAIN)", 2);
            return;
        }
        static const uint8_t OFFS[4] = {TRK_OFF(level), TRK_OFF(pan), TRK_OFF(send), TRK_OFF(rsend)};
        const uint8_t cur[4] = {tr->level, tr->pan, tr->send, tr->rsend};
        int nv = k == 1 ? center_stop(a, cur[k], d, 64, 0, 127) : bm_clampi(cur[k] + d, 0, 127);   /* PAN: el centro retiene */
        app_set_track(a, t, OFFS[k], nv);
        if (k == 1) fmt_pan(nv, b, sizeof b); else if (k == 0) snprintf(b, sizeof b, "%d", nv); else fmt_pct(nv, b, sizeof b);
        static const char *const NM[4] = {"LEVEL", "PAN", "DELAY SEND", "REVERB SEND"};
        snprintf(ti, sizeof ti, "T%d %s", t + 1, T(NM[k]));
        app_overlay(a, k, ti, b, "", nv * 2, k == 1 ? 128 : -1, bm_knob_color(k));
        return;
    }
midi:
    {
        static const uint8_t CC[4] = {0, 0, 1, 74};
        if (k == 0) { int v = bm_clampi(tr->midi_ch + lst(a, d), 0, 15); app_set_track(a, t, TRK_OFF(midi_ch), v); snprintf(b, sizeof b, "%d", v + 1); app_overlay(a, 0, "MIDI CHANNEL", b, "", v * 17, -1, bm_knob_color(0)); return; }
        if (k == 1) { enc_source(a, t, d); return; }
        int v = bm_clampi(tr->macro[k] + d, 0, 127);
        app_set_track(a, t, (uint8_t)k, v);
        app_push(a, CMD_MIDI_SEND, (uint8_t)(0xB0 | (tr->midi_ch & 15)), CC[k], (uint8_t)v);
        snprintf(b, sizeof b, "%d", v); snprintf(ti, sizeof ti, "CC %d", CC[k]);
        app_overlay(a, k, ti, b, "", v * 2, -1, bm_knob_color(k));
        return;
    }
arp:
    {
        int nv;
        if (k == 0) { nv = wrap(tr->arp_mode + lst(a, d), 5); app_set_track(a, t, TRK_OFF(arp_mode), nv); app_overlay(a, 0, "ARP MODE", ARP_MODES[nv], "", nv * 63, -1, bm_knob_color(0)); }
        if (k == 1) { nv = bm_clampi(tr->arp_rate + lst(a, d), 0, 2); app_set_track(a, t, TRK_OFF(arp_rate), nv); app_overlay(a, 1, "ARP RATE", ARP_RATES[nv], "", nv * 127, -1, bm_knob_color(1)); }
        if (k == 2) { nv = bm_clampi(tr->arp_oct + lst(a, d), 1, 3); app_set_track(a, t, TRK_OFF(arp_oct), nv); snprintf(b, sizeof b, "%d", nv); app_overlay(a, 2, "ARP OCTAVES", b, "", nv * 85, -1, bm_knob_color(2)); }
        if (k == 3) { nv = bm_clampi(tr->arp_gate + d, 10, 100); app_set_track(a, t, TRK_OFF(arp_gate), nv); snprintf(b, sizeof b, "%d", nv); app_overlay(a, 3, "ARP GATE", b, "%", nv * 255 / 100, -1, bm_knob_color(3)); }
        return;
    }
chords:
    {
        const char *val = "";
        if (k == 0) { int nv = wrap(m->chord_set + lst(a, d), BM_CHORD_SETS); app_set_meta(a, META_OFF(chord_set), nv); val = bm_chord_set_names[nv]; }
        if (k == 1) { int nv = wrap(m->chord_voicing + lst(a, d), 3); app_set_meta(a, META_OFF(chord_voicing), nv); val = VOICINGS[nv]; a->prev_chord_n = 0; }
        if (k == 2) { int nv = bm_clampi(m->chord_oct + lst(a, d), 1, 5); app_set_meta(a, META_OFF(chord_oct), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
        if (k == 3) { int nv = bm_clampi(m->strum + 2 * lst(a, d), 0, 60); app_set_meta(a, META_OFF(strum), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
        static const char *const NM[4] = {"CHORD SET", "VOICING", "OCTAVE", "STRUM (MS)"};
        app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
    }
}

static void enc_dispatch(bm_app *a, int id, int d, uint32_t now);

/* Aceleración por el tiempo entre giros de cada perilla (no por cuántos detents juntó el bucle, que
 * depende de la pantalla): rápido = x2 o x4 en los valores continuos. Las listas usan lst(). */
#define ENC_FAST_MS  18u     /* menos de esto por detent: x4 */
#define ENC_MED_MS   35u     /* menos de esto: x2 */
#define ENC_GESTURE_MS 800u  /* quieta más que esto: el giro empieza un gesto nuevo */

void bm_app_encoder(bm_app *a, int id, int d, uint32_t now)
{
    a->now = now;
    if (!d || a->view == V_BOOT || id < 0 || id >= ENC_COUNT) return;
    bm_gesture_other(&a->ges, now);
    if (bm_gesture_is_down(&a->ges, BTN_HOME)) a->home_used = 1;
    d = bm_clampi(d, -100, 100);
    uint32_t dt = now - a->enc_t[id];
    a->enc_t[id] = now;
    uint32_t per = dt / (uint32_t)(d < 0 ? -d : d);
    a->enc_new = dt > ENC_GESTURE_MS;
    a->enc_raw = (int8_t)d;
    a->enc_cur = (uint8_t)(id + 1);
    int mul = per < ENC_FAST_MS ? 4 : per < ENC_MED_MS ? 2 : 1;
    enc_dispatch(a, id, d * mul, now);
    a->enc_raw = 0;
    a->enc_cur = 0;
}

static void enc_dispatch(bm_app *a, int id, int d, uint32_t now)
{
    int k = id - ENC_K1;
    bm_meta *m = &a->pat.meta;
    char b[16];

    /* capas primero: las perillas siguen a la capa */
    switch (a->layer) {
    case L_HOME:
        if (id == ENC_SELECT) box_turn(a, d, 1);              /* HOME + SELECT: a otro canal desde cualquier lado */
        else if (id == ENC_ALGO) enc_bpm_fine(a, d);          /* HOME + ALGORITHM: décimas de BPM */
        else if (id == ENC_PRESETS) enc_pattern(a, d);        /* se elige al soltar HOME */
        else if (k >= 0 && k <= 3) enc_global(a, k, d);
        return;
    case L_TONE:                                              /* GLO sostenido: K1 BPM, K2 SWING como en todas partes */
        if (k == 0 || k == 1) enc_global(a, k, d);
        else if (k == 2) enc_transpose(a, lst(a, d));
        else if (k == 3) enc_transpose(a, 12 * lst(a, d));
        return;
    case L_LEN:
        if (k == 0) set_len(a, a->track, bm_trk_len(&a->pat, a->track, app_kind(a, a->track) == KIND_NOTE ? -1 : 0) + lst(a, d));
        else if (k == 1) { int v = bm_clampi(a->pat.trk[a->track].speed + -lst(a, d), 0, SPD_COUNT - 1); app_set_track(a, a->track, TRK_OFF(speed), v); app_overlay(a, 1, "SPEED", bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, bm_knob_color(1)); }
        return;
    case L_ACC: if (k == 0) enc_track_u8(a, a->track, 0, TRK_OFF(accent), a->pat.trk[a->track].accent, d, 0, 127, "ACCENT", 2); return;
    case L_REP: if (k == 0 && app_kind(a, a->track) == KIND_NOTE) enc_track_u8(a, a->track, 0, TRK_OFF(glide), a->pat.trk[a->track].glide, d, 0, 127, "GLIDE", 2); return;
    case L_ROLL:
        if (k == 0) {
            a->roll_rate = (uint8_t)bm_clampi(a->roll_rate + lst(a, d), 0, 4);
            app_overlay(a, 0, "ROLL", bm_roll_names[a->roll_rate], "", a->roll_rate * 63, -1, bm_knob_color(0));
            for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) app_push_i(a, CMD_ROLL, a->track, (uint8_t)l, 1, bm_roll_ticks[a->roll_rate]);
        }
        return;
    case L_FILL: case L_LANEMUTE:                               /* pantallas de MUTE y FILL: las perillas siguen con lo de SEQ */
        if (a->layer == L_FILL && k == 0) { enc_fill_every(a, d); return; }   /* K1 = FILL AUTOMATICO */
        if (id == ENC_SELECT) key_screen_close(a);              /* cambiar de página cierra la pantalla */
        break;
    case L_PUNCH: case L_CHORDS: case L_ARP: case L_ERASE: case L_COPY:
        if (a->layer == L_PUNCH && k >= 0 && k <= 3) break;
        if ((a->layer == L_CHORDS || a->layer == L_ARP) && k >= 0 && k <= 3) break;
        return;
    default: break;
    }
    if (a->layer == L_PUNCH) goto fxg;
    if (a->layer == L_CHORDS) { uint8_t pg = a->page; a->page = PG_CHORDS; enc_channel(a, id, d); a->page = pg; return; }
    if (a->layer == L_ARP) { uint8_t pg = a->page; a->page = PG_ARP; enc_channel(a, id, d); a->page = pg; return; }

    /* REC en un canal de notas: SELECT = el paso, ALGORITHM = su gate (10..100 %, 100 = TIE), PRESETS = su micro-timing */
    if (a->rec && a->view == V_CH && a->page == PG_SEQ && app_kind(a, a->track) == KIND_NOTE && a->held_step < 0
        && (id == ENC_SELECT || id == ENC_ALGO || id == ENC_PRESETS)) { enc_rec(a, id, d); return; }
    /* paso sostenido > lista > canal > HOME. Con un paso sostenido PRESETS es la condición (golpes) o
     * el largo (notas), nunca el kit: cambiarlo ahí reemplazaba los 16 instrumentos sin querer. */
    if (a->view == V_CH && a->page == PG_SEQ && a->held_step >= 0) {
        if (bm_is_grid(&a->pat, a->track)) enc_step_grid(a, id, d); else enc_step_syn(a, id, d);
        return;
    }
    /* las páginas de la consola (BX-1): PRESETS = MOVE en todas */
    if (id == ENC_PRESETS && (a->view == V_HOME || a->view == V_SCENES || a->view == V_FXG || a->view == V_GLO || a->view == V_SONG)) {
        enc_move(a, d);
        return;
    }
    switch (a->view) {
    case V_HOME: case V_SCENES:
        if (id == ENC_ALGO) enc_global(a, 0, d);               /* ALGORITHM = BPM */
        else if (id == ENC_SELECT) {                           /* el cursor de canal (ESCENAS no tiene cursor) */
            if (a->view == V_HOME) a->track = (uint8_t)bm_clampi(a->track + lst(a, d), 0, BM_TRACKS - 1);
        }
        else if (a->view == V_HOME) enc_home(a, k, d);
        else enc_global(a, k, d);
        return;
    case V_MIX:
        if (id == ENC_SELECT) { a->track = (uint8_t)bm_clampi(a->track + lst(a, d), 0, BM_TRACKS - 1); return; }
        if (id == ENC_ALGO) { enc_global(a, 0, d); return; }
        if (id == ENC_PRESETS) { enc_pattern(a, d); return; }
        { uint8_t v = a->view, pg = a->page; a->view = V_CH; a->page = PG_MIX; enc_channel(a, id, d); a->view = v; a->page = pg; }
        return;
    case V_GLO:
        if (id == ENC_SELECT) a->glo_row = (uint8_t)bm_clampi(a->glo_row + lst(a, d), 0, 5);
        else if (id == ENC_ALGO) glo_change(a, d);
        else if (k >= 0 && k <= 3) enc_global(a, k, d);
        return;
    case V_SYSTEM:
        if (id == ENC_SELECT) a->menu_row = (uint8_t)bm_clampi(a->menu_row + lst(a, d), 0, 10);
        else if (id == ENC_ALGO || id == ENC_K1) system_change(a, d);
        return;
    case V_SAVE:                                               /* PRESETS = el patrón de destino, como ALGORITHM */
        if (id == ENC_SELECT) a->save_row = (uint8_t)bm_clampi(a->save_row + lst(a, d), 0, 4);
        else if (id == ENC_ALGO || id == ENC_K1 || id == ENC_PRESETS) a->save_target = (uint8_t)bm_clampi(a->save_target + lst(a, d), 0, BM_PATTERNS - 1);
        a->clear_armed = 0;
        return;
    case V_SONG:
        if (id == ENC_SELECT && a->song.n) a->song_cur = (uint8_t)bm_clampi(a->song_cur + lst(a, d), 0, a->song.n - 1);
        else if ((id == ENC_K1 || id == ENC_ALGO) && a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            int code = (e & 0x80) ? 16 + (e & 3) : e;               /* 0..15 patrones, 16..19 escenas */
            code = bm_clampi(code + lst(a, d), 0, 19);
            a->song.pat[a->song_cur] = (uint8_t)(code >= 16 ? (0x80 | (code - 16)) : code);
            a->song_dirty = 1; a->t_edit = now;
        }
        else if (id == ENC_K2 && a->song.n) { a->song.rep[a->song_cur] = (uint8_t)bm_clampi(a->song.rep[a->song_cur] + lst(a, d), 1, 16); a->song_dirty = 1; a->t_edit = now; }
        else if (id == ENC_K4) {                                /* a la derecha ON, a la izquierda OFF */
            int v = d > 0;
            if (v != (a->ctx.st.song_on != 0)) { app_push(a, CMD_SONG, (uint8_t)v, 0, 0); app_toast(a, v ? "SONG ON" : "SONG OFF"); }
        }
        return;
    case V_TIME: {
        if (id == ENC_SELECT || id == ENC_ALGO) { a->time_row = (uint8_t)bm_clampi(a->time_row + lst(a, d), 0, BM_TRACKS - 1); return; }
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        char ti[22];
        if (k == 0) set_len(a, row, bm_trk_len(&a->pat, row, bm_is_grid(&a->pat, row) ? 0 : -1) + lst(a, d));
        else if (k == 1) {
            int nv = bm_clampi(rt->speed + -lst(a, d), 0, SPD_COUNT - 1);   /* a la derecha = más rápido */
            app_set_track(a, row, TRK_OFF(speed), nv);
            snprintf(ti, sizeof ti, TF("T%d SPEED"), row + 1);
            app_overlay(a, 1, ti, bm_speed_names[nv], "", (SPD_COUNT - 1 - nv) * 51, -1, bm_knob_color(1));
        } else if (k == 2 && bm_is_grid(&a->pat, row) && row != 1) {   /* T2: cada pad tiene su largo en la capa LENGTH */
            a->time_lane = (uint8_t)bm_clampi(a->time_lane + lst(a, d), 0, 15);
        } else if (k == 3 && bm_is_grid(&a->pat, row) && row != 1) {
            const bm_lane *ln = &bm_trk_trc(&a->pat, row)->lane[a->time_lane];
            int nv = bm_clampi(ln->len + lst(a, d), 1, 64);
            app_push_i(a, CMD_SET_LANE, (uint8_t)row, a->time_lane, 0, nv); app_edited(a);
            snprintf(b, sizeof b, "%d", nv); snprintf(ti, sizeof ti, TF("%s LENGTH"), T(bm_lane_name(a, row, a->time_lane)));
            app_overlay(a, 3, ti, b, "STEPS", nv * 4, -1, bm_knob_color(3));
        }
        return;
    }
    case V_FXG:                                                /* SELECT = DELAY > REVERB > MASTER, ALGORITHM = BPM */
        if (id == ENC_SELECT) { a->fx_page = (uint8_t)bm_clampi(a->fx_page + lst(a, d), 0, 2); return; }
        if (id == ENC_ALGO) { enc_global(a, 0, d); return; }
        goto fxg;
    case V_SCLG:
        if (k < 0 || k > 3) return;
        {
            const char *val = "";
            if (k == 0) { int nv = wrap(m->root + lst(a, d), 12); app_set_meta(a, META_OFF(root), nv); val = bm_note_names[nv]; }
            if (k == 1) { int nv = wrap(m->scale + lst(a, d), BM_SCALES); app_set_meta(a, META_OFF(scale), nv); val = bm_scale_names[nv]; }
            if (k == 2) { int nv = wrap(m->voicing + lst(a, d), 5); app_set_meta(a, META_OFF(voicing), nv); val = EXT_NAMES[nv]; }
            if (k == 3) { int nv = d > 0; if (nv != m->scale_lock) app_set_meta(a, META_OFF(scale_lock), nv); val = nv ? "ON" : "OFF"; }
            static const char *const NM[4] = {"PROJECT ROOT", "PROJECT SCALE", "BASE CHORD", "KEYS IN SCALE"};
            app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        }
        return;
    case V_COPYCH:
        if (id == ENC_ALGO || id == ENC_SELECT) {
            int ks = app_kind(a, a->copy_src), dst = a->copy_dst;
            for (int i = 0; i < BM_TRACKS; i++) {
                dst = (dst + (d > 0 ? 1 : BM_TRACKS - 1)) % BM_TRACKS;
                if (dst == a->copy_src || dst == 1) continue;
                if (app_kind(a, dst) == ks) break;
            }
            a->copy_dst = (uint8_t)dst;
        }
        return;
    case V_CH: enc_channel(a, id, d); return;
    default: return;
    }
fxg:
    if (k < 0 || k > 3) return;
    if (a->fx_page == 2 && a->layer != L_PUNCH) {               /* MASTER: filtro, BEAT FX, drive */
        if (k == 0) {
            int nv = bm_clampi(m->mfilt + d, 0, 127);
            app_set_meta(a, META_OFF(mfilt), nv);
            if (nv >= 60 && nv <= 68) snprintf(b, sizeof b, "OPEN"); else if (nv < 60) snprintf(b, sizeof b, "LP %d", nv); else snprintf(b, sizeof b, "HP %d", nv - 64);
            app_overlay(a, 0, "MASTER FILTER", b, "", nv * 2, 128, bm_knob_color(0));
        } else if (k == 1) {
            int nv = bm_clampi(m->beatfx + lst(a, d), 0, 16);
            app_set_meta(a, META_OFF(beatfx), nv);
            app_overlay(a, 1, "BEAT FX", BEATFX_N[nv], "", nv * 15, -1, bm_knob_color(1));
        } else if (k == 2) enc_global(a, 3, d);                 /* DRIVE */
        return;
    }
    if (a->fx_page == 1 && a->layer != L_PUNCH && k < 3) {      /* REVERB: tamaño, amortiguación, mezcla */
        static const uint8_t RO[3] = {META_OFF(rv_size), META_OFF(rv_damp), META_OFF(rv_mix)};
        const uint8_t cur[3] = {m->rv_size, m->rv_damp, m->rv_mix};
        int nv = bm_clampi(cur[k] + d, 0, 127);
        app_set_meta(a, RO[k], nv);
        fmt_pct(nv, b, sizeof b);
        static const char *const NM[3] = {"REVERB SIZE", "REVERB DAMP", "REVERB MIX"};
        app_overlay(a, k, NM[k], b, "", nv * 2, -1, bm_knob_color(k));
        return;
    }
    if (k == 0) {
        int nv = bm_clampi(m->dly_time + lst(a, d), 0, 3);
        app_set_meta(a, META_OFF(dly_time), nv);
        app_overlay(a, 0, "DELAY TIME", DLY_NAMES[nv], "", nv * 85, -1, bm_knob_color(0));
    } else {
        static const uint8_t OFFS[4] = {0, META_OFF(dly_fb), META_OFF(dly_mix), META_OFF(sc_release)};
        const uint8_t cur[4] = {0, m->dly_fb, m->dly_mix, m->sc_release};
        int nv = bm_clampi(cur[k] + d, 0, 127);
        app_set_meta(a, OFFS[k], nv);
        if (k == 3) fmt_ms(bm_decay_ms(nv / 2), b, sizeof b); else fmt_pct(nv, b, sizeof b);
        static const char *const NM[4] = {"", "DELAY FEEDBACK", "DELAY MIX", "SIDECHAIN RELEASE"};
        app_overlay(a, k, NM[k], b, "", nv * 2, -1, bm_knob_color(k));
    }
}

/* ------------------------------------------------------------ tick de la entrada */
void app_input_tick(bm_app *a)
{
    uint32_t now = a->now;
    if (a->held_white >= 0 && !a->held_long && now - a->step_t0 >= UI_HOLD_MS) a->held_long = 1;
    if (a->rec_free && a->rec_free_t0 == 0xFFFFFFFFu && app_playing(a)) a->rec_free_t0 = a->ctx.st.btick;   /* empieza con PLAY */
    if (a->fill && a->layer != L_FILL) {                         /* el fill suena solo con su pantalla abierta o su tecla sostenida */
        int held = 0;
        for (int k = 0; k < BM_KEYS; k++) if (a->keys_down[k] && (a->key_role[k] == ROLE_FILL || a->key_role[k] == ROLE_FILLK)) held = 1;
        if (!held) { app_push(a, CMD_FILL, 0, 0, 0); a->fill = 0; }
    }
    for (int k = 0; k < BM_KEYS; k++) {
        if (!a->keys_down[k] || !a->key_t0[k] || now - a->key_t0[k] < UI_HOLD_MS) continue;
        if (a->key_role[k] == ROLE_MUTE && !a->solo_on) {           /* negra sostenida en HOME: solo */
            int t = bm_key_black_index(k);
            a->solo_saved = 0;
            for (int i = 0; i < BM_TRACKS; i++) if (a->pat.trk[i].mute) a->solo_saved |= (uint8_t)(1u << i);
            for (int i = 0; i < BM_TRACKS; i++) app_set_track(a, i, TRK_OFF(mute), i != t);
            a->solo_on = 1; a->solo_key = (int8_t)k; a->solo_track = (int8_t)t;
            char b[16]; snprintf(b, sizeof b, "SOLO T%d", t + 1);
            app_toast(a, b);
        }
    }
    if (a->box_on && a->box_hold != 1 && (int32_t)(now - a->box_until) >= 0) {
        if (a->box_hold == 0) box_commit(a); else a->box_on = 0;
    }
}

/* ------------------------------------------------------------ LEDs */
static uint8_t led_last[BM_LEDS];

static void led(bm_app *a, int id, uint8_t lvl)
{
    if (id < BM_LED_KEY0 && lvl == LED_DIM) {                /* SISTEMA > LED: solo los botones en reposo */
        if (a->set.led_dim == 0) lvl = LED_OFF;
        else if (a->set.led_dim == 2) lvl = LED_MID;
    }
    if (led_last[id] == lvl + 1) return;
    led_last[id] = (uint8_t)(lvl + 1);
    if (a->pf.led) a->pf.led(a->pf.user, id, lvl);
}

/* El botón de la página del canal: el que la abrió (button_page) mientras sigas en ella; si no,
 * el que la abre según la página (y en las de un editor por páginas, la envolvente o el LFO). */
static int page_button(const bm_app *a)
{
    int t = a->track;
    if (a->nav_btn && a->nav_track == t && a->nav_page == a->page && a->nav_sub == a->ed_page[t]) return a->nav_btn - 1;
    switch (a->page) {
    case PG_SEQ: case PG_LOOP: case PG_BREAK: return BTN_SEQ;
    case PG_SOUND:
        if (app_kind(a, t) == KIND_NOTE && bm_engine_paged(app_engine(a, t))) {
            int e = app_engine(a, t), p = a->ed_page[t];
            if (p == bm_engine_page_of(e, EPG_AMP) || p == bm_engine_page_of(e, EPG_FENV)) return BTN_ENV;
            if (p == bm_engine_page_of(e, EPG_LFO)) return BTN_LFO;
        }
        return BTN_EDIT;
    case PG_PADS: return a->ed_page[t] == 2 ? BTN_ENV : BTN_EDIT;
    case PG_INST: case PG_TAKES: case PG_MIDI: return BTN_EDIT;
    case PG_ENV: return BTN_ENV;
    case PG_MIX: return BTN_FX;
    case PG_SCL: case PG_CHORDS: return BTN_SCL;
    case PG_ARP: return BTN_ARP;
    case PG_LFO: return BTN_LFO;
    default: return -1;
    }
}

/* El botón sostenido de cada capa (respira). MUTE y FILL se abren con una tecla: ninguno. */
static int layer_button(int L)
{
    switch (L) {
    case L_HOME: return BTN_HOME;
    case L_LEN: return BTN_SEQ;
    case L_ERASE: return BTN_EDIT;
    case L_ACC: return BTN_ENV;
    case L_REP: return BTN_LFO;
    case L_PUNCH: return BTN_FX;
    case L_CHORDS: return BTN_SCL;
    case L_ARP: case L_ROLL: return BTN_ARP;
    case L_COPY: return BTN_SAVE;
    case L_TONE: return BTN_GLO;
    default: return -1;                                      /* L_NONE, L_LANEMUTE, L_FILL */
    }
}

/* Las páginas de la consola (BX-1) que recorre HOME: MIX > FX > TEMPO > ESCENAS > SONG. TEMPO (GLO)
 * es de la consola si se llegó desde ella; desde un canal es la lista de GLO. */
static bool console_view(int v) { return v == V_HOME || v == V_MIX || v == V_FXG || v == V_SCENES || v == V_SONG; }
static bool console_page(const bm_app *a) { return console_view(a->view) || (a->view == V_GLO && console_view(a->prev_view)); }

/* El botón de "dónde estás" en cada pantalla. En la consola es HOME. */
static int view_button(const bm_app *a)
{
    if (console_page(a)) return BTN_HOME;
    switch (a->view) {
    case V_SYSTEM: return BTN_HOME;
    case V_TIME: return BTN_SEQ;
    case V_GLO: return BTN_GLO;
    case V_SAVE: return BTN_SAVE;
    case V_SCLG: return BTN_SCL;
    case V_CH: return page_button(a);
    default: return -1;
    }
}

/* En la consola, el otro botón que abre la misma página (MID): FX, GLO, SEQ (canción). En HOME y
 * en los faders, SEQ y EDIT entran al canal del cursor. */
static int alias_button(const bm_app *a, int i)
{
    switch (a->view) {
    case V_FXG: return i == BTN_FX;
    case V_GLO: return i == BTN_GLO && console_page(a);
    case V_SONG: return i == BTN_SEQ;
    case V_HOME: case V_MIX: return i == BTN_SEQ || i == BTN_EDIT;
    default: return 0;
    }
}

/* Páginas de pasos del canal (OCT -/+ en SEQ y en ACENTO, REPETIR y FILL). */
static int step_pages(const bm_app *a)
{
    int t = a->track, len = bm_trk_len(&a->pat, t, app_kind(a, t) == KIND_NOTE ? -1 : tr_lane(a));
    int pages = (len + 15) / 16;
    return pages > 0 ? pages : 1;
}

static uint8_t page_arrow(int up, int cur, int last) { return (up ? cur < last : cur > 0) ? LED_MID : LED_OFF; }

/* HOME + EDIT: hay otro canal del mismo tipo para copiar (home_combo) */
static bool copy_target(const bm_app *a)
{
    if (bm_is_pads(a->track)) return false;
    int k = app_kind(a, a->track);
    for (int i = 1; i < BM_TRACKS; i++) {
        int d = (a->track + i) % BM_TRACKS;
        if (d != 1 && app_kind(a, d) == k) return true;
    }
    return false;
}

/* Hay algo sin guardar (el patrón, la canción, una toma o su ganancia). Los ajustes no: SAVE no
 * los escribe, se guardan solos. */
static bool unsaved(const bm_app *a)
{
    return a->dirty || a->song_dirty || a->pend_obj >= 0 || a->take_dirty || a->ctx.st.smp_unsaved;
}

static bool writing(const bm_app *a)
{
    for (int i = 0; i < a->njob; i++) if (a->job[i] != JOB_LOAD) return true;
    return a->job_fading;
}

/* Un patrón o una escena en cola (no las cadenas, que siempre tienen el siguiente en cola). */
static bool queued(const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    return (st->queued != 0xFF && st->queued != st->pattern && st->pchain_n <= 1) || (st->scene_q != 0xFF && !st->chain_n);
}

static uint8_t play_led(const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    if (st->playing) return (st->btick % BM_BAR_TICKS) < 24 ? LED_ON : (st->btick % BM_PPQN) < 24 ? LED_MID : LED_DIM;
    return a->set.clock && !st->ext_ok ? LED_BREATH : LED_DIM;      /* reloj externo sin señal: esperando */
}

static uint8_t rec_led(const bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    int t = a->track, lm = LOOP_OFF;
    if (t >= BM_TR_TRACKS && t < BM_TRACKS && app_kind(a, t) == KIND_NOTE) lm = st->loop_mode[t - BM_TR_TRACKS];
    bool smp = a->view != V_CH || t == 1;                   /* la grabadora: no dentro de otra máquina */
    if ((smp && st->smp_rec) || a->rec_hold || lm == LOOP_REC) return LED_BLINK;   /* grabando */
    if ((smp && st->smp_armed) || lm == LOOP_ARMED) return LED_BREATH;              /* armado */
    if (a->rec || lm == LOOP_DUB) return LED_ON;                            /* REC de notas, sobregrabación */
    return LED_DIM;
}

/* Las teclas tocan notas con la octava de OCT -/+ (fuera de SEQ, donde OCT pasa páginas). */
static bool oct_plays(const bm_app *a)
{
    switch (a->view) {
    case V_HOME: case V_MIX: case V_SCENES: case V_SONG: case V_COPYCH: case V_BOOT: return false;
    case V_CH:
        if (a->page == PG_CHORDS) return true;
        if (a->page == PG_SEQ || a->page == PG_TAKES || a->page == PG_REC || a->page == PG_PADS) return false;
        break;
    default: break;
    }
    return app_kind(a, a->track) == KIND_NOTE;
}

/* En la pantalla (sin pregunta, sin HOME ni capa): el toque o el sostenido hacen algo. Los botones
 * de página entran al canal desde cualquier pantalla y todos tienen capa; OCT solo si las teclas
 * tocan notas. */
static bool btn_usable(const bm_app *a, int id)
{
    if (id == BTN_OCT_DN || id == BTN_OCT_UP) return oct_plays(a);
    return true;
}

/* La luz de un botón. Blancas: dicen el estado, nunca la función (eso lo dice la pantalla).
 * OFF no hace nada aquí · DIM disponible · MID significado especial ahora · ON dónde estás o un
 * estado que queda · BLINK te necesita ya · BREATH armado o pendiente.
 * Precedencia: pregunta > HOME sostenido > capa sostenida > pantalla. */
static uint8_t btn_led(const bm_app *a, int i)
{
    if (a->ask || a->view == V_COPYCH)                      /* OCT+ sí · HOME no · OCT- también cancela */
        return i == BTN_OCT_UP ? LED_BLINK : i == BTN_HOME ? LED_ON : i == BTN_OCT_DN ? LED_MID : LED_OFF;
    if (a->layer == L_HOME) {                               /* HOME sostenido: los que tienen HOME + */
        switch (i) {
        case BTN_HOME: return LED_ON;
        case BTN_OCT_DN: return a->undo_track >= 0 ? LED_MID : LED_OFF;
        case BTN_OCT_UP: return a->redo_track >= 0 ? LED_MID : LED_OFF;
        case BTN_EDIT: return copy_target(a) ? LED_MID : LED_OFF;
        case BTN_REC: return a->song_rec ? LED_ON : LED_MID;
        case BTN_SEQ: case BTN_FX: case BTN_SCL: case BTN_SAVE: case BTN_PLAY: return LED_MID;
        default: return LED_OFF;
        }
    }
    if (a->layer) {                                         /* capa sostenida, o MUTE / FILL abiertas */
        int L = a->layer;
        if (i == layer_button(L)) return LED_BREATH;
        switch (i) {
        case BTN_OCT_DN: case BTN_OCT_UP: {
            int up = i == BTN_OCT_UP;
            if (L == L_ERASE) return (up ? a->redo_track : a->undo_track) >= 0 ? LED_MID : LED_OFF;
            if (L == L_LEN) return page_arrow(up, a->len_page, 3);
            if (L == L_ACC || L == L_REP || L == L_FILL) return page_arrow(up, a->step_page, step_pages(a) - 1);
            if (L == L_ARP || L == L_CHORDS) return (up ? a->oct > 0 : a->oct < 0) ? LED_ON : LED_MID;
            return LED_OFF;
        }
        case BTN_PLAY: return L == L_TONE ? LED_MID : play_led(a);         /* TONE: tap tempo */
        case BTN_REC: { uint8_t r = rec_led(a); return r == LED_DIM ? LED_OFF : r; }
        case BTN_HOME: return a->layer_latch ? LED_MID : LED_OFF;           /* MUTE / FILL: salir */
        default: return LED_OFF;
        }
    }
    switch (i) {
    case BTN_PLAY: return play_led(a);
    case BTN_REC: return rec_led(a);
    case BTN_OCT_DN: case BTN_OCT_UP: {
        int up = i == BTN_OCT_UP;
        if (a->view == V_CH && a->page == PG_SEQ) return page_arrow(up, a->step_page, step_pages(a) - 1);
        if (up ? a->oct > 0 : a->oct < 0) return LED_ON;                    /* octava corrida */
        break;
    }
    case BTN_HOME:                                          /* dentro de un canal: salir a la consola */
        if (queued(a) && a->view != V_HOME && a->view != V_MIX && a->view != V_SCENES) return LED_BLINK;   /* patrón o escena en cola */
        break;
    case BTN_SAVE:
        if (a->view == V_SAVE && a->clear_armed && a->now - a->clear_t < 3000) return LED_BLINK;   /* SAVE otra vez = borrar */
        if (writing(a)) return LED_ON;
        if (a->view != V_SAVE && a->view != V_CH && unsaved(a)) return LED_BREATH;   /* global: no dentro de la máquina */
        break;
    case BTN_SEQ:
        if (a->song_rec && a->view != V_CH) return LED_BLINK;               /* SONG REC (de la consola) */
        break;
    default: break;
    }
    if (i == view_button(a)) return LED_ON;
    if (alias_button(a, i)) return LED_MID;
    return btn_usable(a, i) ? LED_DIM : LED_OFF;
}

/* Qué hace cada tecla ahora y cómo está su luz. Las luces del FM-1 son blancas: solo dicen el estado
 * (la gramática está arriba, en el mapa del teclado), así que la pantalla muestra estos mismos nombres
 * (tira de teclas) y la luz y el dibujo salen siempre de aquí. Dentro de un canal las teclas son la
 * superficie de esa máquina; en HOME, la de la consola. */
static void KL(bm_keyfn *K, int k, const char *l) { snprintf(K[k].label, sizeof K[k].label, "%s", l); }

/* las notas que el canal t acaba de tocar desde sus pasos (para que destellen sus teclas) */
static int live_notes(const bm_app *a, int t, uint8_t out[4])
{
    if (!(a->now < a->trig_until[t])) return 0;
    int s = bm_app_track_step(a, t, -1), idx[4];
    if (s < 0) return 0;
    int n = syn_step_events(a, t, s, idx);
    for (int i = 0; i < n; i++) out[i] = bm_evc(&a->pat, t, s, idx[i])->note;
    return n;
}

static uint8_t note_in(int note, const uint8_t *nn, int n)
{
    for (int i = 0; i < n; i++) if (nn[i] == note) return 1;
    return 0;
}

/* El teclado de un canal de notas fuera de SEQ: cromático (la escala DIM, la tónica MID, fuera de la
 * escala OFF) o, con LOCK o en SCALE, la escala en las blancas y los acordes en las negras. */
static void note_keys(const bm_app *a, bm_keyfn *K, int t)
{
    int root, sc = app_scale(a, t, &root), sl = bm_scale_len(sc);
    uint8_t nn[4];
    int nl = live_notes(a, t, nn);
    uint16_t col = bm_track_color(t);
    if (a->pat.meta.scale_lock || (a->view == V_CH && a->page == PG_SCL)) {
        static const char *const R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
        static const char *const X[4] = {"+7", "+9", "S2", "S4"};
        for (int w = 0; w < 16; w++) {
            int note = bm_scale_note(root, sc, w, base_oct(a, t));
            KW(K, w, bm_note_names[note % 12], (w % sl) == 0 ? LED_MID : LED_DIM, col)->live = note_in(note, nn, nl);
        }
        for (int b = 0; b < 7; b++) KB(K, b, R[b], LED_DIM, col);
        for (int b = 7; b < 11; b++) KB(K, b, X[b - 7], (a->chord_mod >> (b - 7)) & 1 ? LED_ON : LED_DIM, C_YELLOW);
        return;
    }
    uint16_t pcs = 0;
    for (int d = 0; d < sl; d++) pcs |= (uint16_t)(1u << (bm_scale_note(root, sc, d, 0) % 12));
    for (int k = 0; k < BM_KEYS; k++) {
        int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0), pc = (note + 120) % 12;
        KL(K, k, bm_note_names[pc]);
        K[k].led = pc == root ? LED_MID : ((pcs >> pc) & 1) ? LED_DIM : LED_OFF;
        K[k].color = col;
        K[k].live = note_in(note, nn, nl);
    }
}

/* Las blancas de un canal de batería fuera de SEQ: los 16 instrumentos, como un selector */
static void lane_whites(const bm_app *a, bm_keyfn *K, int t)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int w = 0; w < 16; w++)
        KW(K, w, lane_short(a, t, w), lane_led(tr, w, tr_lane(a)), bm_track_color(t))->live = (uint8_t)(lane_has(tr, w) && lane_live(a, t, w));
}

static void keys_fill(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    const bm_status *st = &a->ctx.st;
    int t = a->track, kind = app_kind(a, t);
    char title[24], b8[8]; uint16_t col; const char *h1, *h2;
    bool mapped = a->layer || a->view == V_HOME || a->view == V_MIX || (a->view == V_CH && a->page == PG_CHORDS);
    if (mapped) { bm_app_keymap(a, K, title, sizeof title, &col, &h1, &h2); return; }
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    if (a->view == V_SCENES) {                                          /* consola: escenas y canales */
        static const char *const SL[8] = {"A", "B", "C", "D", "ST A", "ST B", "ST C", "ST D"};
        for (int w = 0; w < 4; w++)
            KW(K, w, SL[w], w == st->scene ? LED_ON : (w == st->scene_q ? LED_BLINK : (a->song.scene[w].valid ? LED_MID : LED_OFF)), C_CYAN);
        for (int w = 4; w < 8; w++) KW(K, w, SL[w], a->song.scene[w - 4].valid ? LED_MID : LED_DIM, C_CYAN);
        home_blacks(a, K);
        return;
    }
    if (a->view == V_SONG) {                                            /* blancas = agregar ese patrón */
        int cur = a->song.n && a->song_cur < a->song.n && a->song.pat[a->song_cur] < 0x80 ? a->song.pat[a->song_cur] : -1;
        for (int w = 0; w < 16; w++) {
            bool has = bm_store_has(&a->store, (uint8_t)w) || w == st->pattern;
            snprintf(b8, sizeof b8, "%02d", w + 1);
            KW(K, w, b8, w == cur ? LED_ON : (has ? LED_MID : LED_DIM), C_WHITE);
        }
        KB(K, 8, "DEL", a->song.n ? LED_DIM : LED_OFF, C_RED);
        return;
    }
    if (a->view != V_CH) {                                             /* las demás pantallas tocan el canal */
        if (kind == KIND_NOTE) note_keys(a, K, t); else lane_whites(a, K, t);
        return;
    }
    switch (a->page) {
    case PG_SEQ:
        for (int w = 0; w < 16; w++) {
            int s = a->step_page * 16 + w;
            uint8_t l = LED_OFF;
            snprintf(b8, sizeof b8, "%d", s + 1);
            if (kind == KIND_NOTE) {
                int idx[4];
                if (s < a->pat.trk[t].length) {
                    l = syn_step_events(a, t, s, idx) ? LED_ON : note_covers(a, t, s) ? LED_MID : LED_OFF;
                    if (s == bm_app_track_step(a, t, -1)) l = LED_BLINK;
                }
            } else {
                const bm_tr *tr = bm_trk_trc(&a->pat, t);
                int lane = tr_lane(a);
                if (s < tr->lane[lane].len) {
                    uint8_t bb = tr->st[lane][s];
                    l = (bb & ST_ON) ? ((bb & ST_LOCK) ? LED_BREATH : LED_ON) : LED_OFF;
                    if (s == bm_app_track_step(a, t, lane)) l = LED_BLINK;
                }
            }
            KW(K, w, b8, l, 0);
        }
        if (kind != KIND_NOTE) { tr_blacks(a, K, t, true); return; }
        {
            int root, sc = app_scale(a, t, &root), idx[4], n = 0;
            if (a->held_step >= 0) n = syn_step_events(a, t, a->held_step, idx);
            for (int b = 0; b < 8; b++) {                               /* grados: ON los que tiene el paso sostenido */
                int note = bm_scale_note(root, sc, b, base_oct(a, t) + a->note_oct);
                uint8_t l = LED_DIM;
                for (int i = 0; i < n; i++) if (bm_evc(&a->pat, t, a->held_step, idx[i])->note == note) l = LED_ON;
                KB(K, b, bm_note_names[note % 12], l, bm_track_color(t));
            }
            KB(K, 8, "O-", a->note_oct < 0 ? LED_ON : LED_DIM, C_WHITE);
            KB(K, 9, "O+", a->note_oct > 0 ? LED_ON : LED_DIM, C_WHITE);
            KB(K, 10, "FIL", a->fill ? LED_ON : LED_DIM, C_ORANGE);
        }
        return;
    case PG_PADS:
        for (int w = 0; w < 16; w++) {
            snprintf(b8, sizeof b8, "%d", w + 1);
            KW(K, w, b8, w == (a->sel_lane[1] & 15) ? LED_ON : (bm_pad_ok(&a->pat.pad[w]) ? LED_MID : LED_OFF), bm_track_color(1))->live =
                (uint8_t)(a->now < a->lane_flash[1][w]);
        }
        {
            static const char *const CB[4] = {"CPY", "PST", "CLR", "HIT"};
            static const char *const FX[3] = {"REV", "HLF", "STU"};
            for (int b = 0; b < 4; b++) KB(K, b, CB[b], LED_DIM, C_WHITE);
            for (int b = 8; b < 11; b++) KB(K, b, FX[b - 8], (a->brk_live >> (b - 8)) & 1 ? LED_ON : LED_DIM, C_YELLOW);
        }
        return;
    case PG_TAKES: {                                                    /* sin toma marcada no hacen nada */
        uint8_t src = bm_app_takes_sel(a);
        uint8_t l = src < BM_TAKES && a->take[src].valid ? LED_DIM : LED_OFF;
        static const char *const TB[4] = {"HIT", "16", "8", "PAD"};
        for (int w = 0; w < 16; w++) { snprintf(b8, sizeof b8, "%d", w + 1); KW(K, w, b8, l, C_WHITE); }
        for (int b = 0; b < 4; b++) KB(K, b, TB[b], l, C_WHITE);
        KB(K, 10, "DEL", l, C_RED);
        return;
    }
    case PG_REC:                                                        /* después de grabar: escuchar la toma */
        if (st->smp_unsaved && !st->smp_rec)
            for (int w = 0; w < 16; w++) { snprintf(b8, sizeof b8, "%d", w + 1); KW(K, w, b8, LED_DIM, C_RED); }
        return;
    default:
        if (kind == KIND_NOTE) note_keys(a, K, t);
        else { lane_whites(a, K, t); tr_blacks(a, K, t, true); }
        return;
    }
}

void bm_app_keys(const bm_app *a, bm_keyfn K[BM_KEYS])
{
    keys_fill(a, K);
    if (!a->box_on) return;
    /* el recuadro de SELECT/ALGORITHM abierto: parpadea la tecla del canal o instrumento al que vas */
    int k = -1, t = a->track;
    bool home = a->layer == L_HOME || (!a->layer && (a->view == V_HOME || a->view == V_MIX || a->view == V_SCENES));
    if (a->box_mode == BOX_HOME && home) k = bm_black_key(a->box_track % BM_TRACKS);
    else if (a->box_mode == BOX_LANE && !a->layer && a->view == V_CH && app_kind(a, t) == KIND_TR) {
        int ln = a->box_lane & 15;
        if (a->page == PG_SEQ) { if (ln / 8 == tr_lane(a) / 8) k = bm_black_key(ln % 8); }
        else if (a->page != PG_PADS && a->page != PG_TAKES && a->page != PG_REC) k = bm_white_key(ln);
    }
    if (k >= 0 && K[k].label[0]) { K[k].led = LED_BLINK; K[k].live = 0; }
}

void app_leds(bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    if (a->view == V_BOOT) {
        int lit = (int)((a->now - a->boot_t) / 40u) % (BM_KEYS + 6);
        for (int k = 0; k < BM_KEYS; k++) led(a, BM_LED_KEY0 + k, k == lit ? LED_ON : LED_OFF);
        for (int i = 0; i < BTN_COUNT; i++) led(a, i, LED_DIM);
        return;
    }
    (void)st;
    for (int i = 0; i < BTN_COUNT; i++) led(a, i, btn_led(a, i));
    bm_keyfn K[BM_KEYS];
    bm_app_keys(a, K);
    for (int k = 0; k < BM_KEYS; k++)                         /* sostenida (si hace algo) o un golpe que suena: ON */
        led(a, BM_LED_KEY0 + k, (a->keys_down[k] && K[k].label[0]) || K[k].live ? LED_ON : K[k].led);
}
