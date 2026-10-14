/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — aplicación: estado, guardado, tomas del sampler y tick (DISENO_3.0).
 * Los botones, las teclas y las perillas están en bm_input.c. */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "bm_app_int.h"
#include "bm_util.h"
#include "bm_rom.h"
#include "bm_params.h"
#include "bm_snap.h"
#include "bm_wt.h"

static const uint16_t TRACK_COLORS[BM_TRACKS] = {C_ORANGE, C_PINK, C_ACID, C_CYAN, C_VIOLET, C_GREEN};
static const uint16_t KNOB_COLORS[4] = {C_CYAN, C_ORANGE, C_GREEN, C_PINK};
const uint8_t bm_master_steps_tab[6] = {1, 2, 3, 4, 8, 0};
const uint8_t bm_roll_ticks[5] = {48, 24, 12, 32, 16};
const char *const bm_roll_names[5] = {"1/8", "1/16", "1/32", "1/8T", "1/16T"};
static const int16_t ENTER_MS[5] = {0, 250, 400, 800, -1};

const char *bm_track_name(const bm_app *a, int t)
{
    t %= BM_TRACKS;
    return bm_engine_names[a->pat.trk[t].engine % ENG_COUNT];
}
uint16_t bm_track_color(int t) { return TRACK_COLORS[t % BM_TRACKS]; }
const char *bm_kit_name(int kit) { return bm_kits[(kit % BM_KITS_N + BM_KITS_N) % BM_KITS_N].name; }

int bm_sound_count(const bm_app *a, int t)
{
    if (bm_is_tr(&a->pat, t)) return BM_KITS_N;
    if (a->pat.trk[t].engine == ENG_ACID) return 2;
    if (a->pat.trk[t].engine == ENG_FM) return bm_fm_count();
    int n = bm_snap_count(a->pat.trk[t].engine);
    return n ? n : 1;
}

const char *bm_sound_name(const bm_app *a, int t)
{
    int s = a->pat.trk[t].source % bm_sound_count(a, t);
    if (bm_is_tr(&a->pat, t)) return bm_kit_name(s);
    if (a->pat.trk[t].engine == ENG_ACID) return bm_acid_names[s];
    if (a->pat.trk[t].engine == ENG_FM) return bm_fm_patch_name(s);
    return bm_snap_name(a->pat.trk[t].engine, s);
}
uint16_t bm_knob_color(int i) { return KNOB_COLORS[i & 3]; }
void bm_app_cpu(bm_app *a, int pct) { a->cpu = (uint8_t)bm_clampi(pct, 0, 255); }

/* BITTAL (T5): sus dos tablas, armadas en RAM desde el espectro del snapshot (core/bm_wt.h). Se
 * rearman cuando cambia el sonido o la tabla elegida con ALGORITHM; mientras, ese oscilador calla. */
static void app_tables(bm_app *a)
{
    const int t = 4;
    if (a->pat.trk[t].engine != ENG_BITTAL) return;
    const uint8_t *q = a->pat.prm[t];
    int n = bm_snap_count(ENG_BITTAL);
    for (int o = 0; o < 2; o++) {
        int tab = q[o ? BT_TAB2 : BT_TAB1];
        int own = n ? a->pat.trk[t].source % n : 0;
        const uint8_t *s = bm_snap_get(ENG_BITTAL, tab ? tab - 1 : own);
        const uint8_t *spec = s ? bm_snap_table(s, tab ? 0 : o) : NULL;
        uint32_t sig = s ? (uint32_t)(s[8] | s[9] << 8) * 4u + (uint32_t)(tab ? 2 : o) + 1u : 0;
        if (sig == a->wt_sig[o]) continue;
        bm_wt_build(o, spec);
        a->wt_sig[o] = sig;
    }
}
int bm_app_layer(const bm_app *a) { return a->layer; }
int bm_app_enter_ms(const bm_app *a) { return ENTER_MS[a->set.enter % 5]; }

/* ------------------------------------------------------------ comandos */
void app_push(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.b = y; c.c = z;
    bm_cmd_push(&a->cmdq, &c);
}

void app_push_i(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z, int32_t i)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.b = y; c.c = z; c.u.i = i;
    bm_cmd_push(&a->cmdq, &c);
}

void app_push_ptr(bm_app *a, uint8_t type, uint8_t x, const void *p)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.u.ptr = p;
    bm_cmd_push(&a->cmdq, &c);
}

void app_push_ev(bm_app *a, int t, int s, int k, const bm_event *e)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = CMD_SET_EVENT; c.a = (uint8_t)t; c.b = (uint8_t)s; c.c = (uint8_t)k; c.u.ev = *e;
    bm_cmd_push(&a->cmdq, &c);
}

void app_edited(bm_app *a) { a->dirty = 1; a->t_edit = a->now; }
void app_set_track(bm_app *a, int t, uint8_t off, int v) { app_push(a, CMD_SET_TRACK, (uint8_t)t, off, (uint8_t)v); app_edited(a); }
void app_set_meta(bm_app *a, uint8_t off, int v) { app_push(a, CMD_SET_META, 0, off, (uint8_t)v); app_edited(a); }

void app_toast(bm_app *a, const char *msg)
{
    snprintf(a->toast, sizeof a->toast, "%s", msg);
    a->toast_until = a->now + UI_TOAST_MS;
}

void app_overlay(bm_app *a, int enc, const char *title, const char *val, const char *unit, int pct, int ghost, uint16_t col)
{
    bm_overlay *o = &a->ov;
    o->on = 1; o->enc = (uint8_t)enc; o->pct = (uint8_t)bm_clampi(pct, 0, 255);
    o->ghost = (uint8_t)(ghost < 0 ? 255 : bm_clampi(ghost, 0, 254));
    o->color = col;
    snprintf(o->title, sizeof o->title, "%s", title);
    snprintf(o->val, sizeof o->val, "%s", val);
    snprintf(o->unit, sizeof o->unit, "%s", unit);
    o->until = a->now + UI_OVERLAY_MS;
}

void app_ask(bm_app *a, int kind, int arg, int arg2, const char *l1, const char *l2)
{
    a->ask = (uint8_t)kind; a->ask_arg = (uint8_t)arg; a->ask_arg2 = (uint8_t)arg2;
    snprintf(a->ask_text[0], sizeof a->ask_text[0], "%s", l1);
    snprintf(a->ask_text[1], sizeof a->ask_text[1], "%s", l2 ? l2 : "");
    a->ov.on = 0;
}

int app_scale(const bm_app *a, int t, int *root)
{
    const bm_track *tk = &a->pat.trk[t % BM_TRACKS];
    if (tk->scl != 0xFF && tk->scl < BM_SCALES) { *root = tk->scl_root % 12; return tk->scl; }
    *root = a->pat.meta.root % 12;
    return a->pat.meta.scale % BM_SCALES;
}

const char *bm_lane_name(const bm_app *a, int t, int lane)
{
    static char buf[2][8];
    static int k;
    if (bm_is_tr(&a->pat, t)) return bm_lane_label(bm_trk_trc(&a->pat, t), a->pat.trk[t].source, lane);
    k ^= 1;
    if (t == 1) snprintf(buf[k], sizeof buf[k], "P%d", (lane & 15) + 1);     /* pads: P1..P16 */
    else snprintf(buf[k], sizeof buf[k], "%d", lane + 1);
    return buf[k];
}

int bm_app_track_step(const bm_app *a, int t, int lane)
{
    if (!app_playing(a)) return -1;
    uint32_t n = a->ctx.st.tn[t];
    return (int)(n % (uint32_t)bm_trk_len(&a->pat, t, lane));
}

int bm_app_chord_track(const bm_app *a)
{
    if (app_kind(a, a->track) == KIND_NOTE) return a->track;
    for (int t = 3; t < BM_TRACKS; t++) if (app_kind(a, t) == KIND_NOTE) return t;
    return 2;
}

/* ------------------------------------------------------------ almacenamiento */
static const uint8_t *store_ptr(bm_app *a, int obj, int *len)
{
    if (!a->pf.flash_map || obj < 0 || obj >= BM_OBJ_COUNT || a->store.live[obj] < 0) return NULL;
    *len = a->store.live_len[obj];
    return a->pf.flash_map(a->pf.user, BM_STORE_LO + (uint32_t)a->store.live[obj] * BM_SLOT_SIZE + 16u);
}

static void empty_from(bm_pattern *p)
{
    for (int t = 0; t < BM_TRACKS; t++) bm_track_clear_events(p, t);    /* vacío: hereda sonidos */
    for (int i = 0; i < BM_SYN_TRACKS; i++) { p->loop[i].n = 0; p->loop[i].on = 0; }
}

/* Cambio de patrón: corre en la ISR de audio. Sin escrituras en flash. */
static int app_swap(void *u, int from, int to, bm_pattern *p)
{
    bm_app *a = (bm_app *)u;
    if (a->dirty) {
        if (a->pend_obj >= 0 && a->pend_obj != from) return -1;
        int n = bm_pattern_pack(p, a->pend, BM_PENDING_LEN);
        if (n < 0) return -1;
        a->pend_obj = (int16_t)from; a->pend_len = (uint16_t)n;
    }
    if (a->pend_obj == to) {
        bm_pattern_unpack(p, a->pend, a->pend_len);
        a->pend_obj = -1;
        a->dirty = 1;
    } else {
        int len = 0;
        const uint8_t *src = store_ptr(a, to, &len);
        if (!(src && len > 0 && bm_pattern_unpack(p, src, len) > 0)) empty_from(p);
        a->dirty = 0;
    }
    a->set.last_pat = (uint8_t)to;
    a->set_dirty = 1;
    return 0;
}

/* ------------------------------------------------------------ sampler: fuentes y dibujo */
/* La onda como en un sampler: por columna, el pico [0] y el RMS [1] de las muestras [s0, s1),
 * en 0..255 respecto de pk (el pico de la toma entera: el zoom no cambia la altura). */
static uint32_t isqrt32(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}

static uint32_t wave_peak(const int8_t *d, uint32_t len)
{
    uint32_t mx = 1;
    for (uint32_t i = 0; d && i < len; i++) { uint32_t u = (uint32_t)(d[i] < 0 ? -d[i] : d[i]); if (u > mx) mx = u; }
    return mx;
}

static void wave_cols(uint8_t wave[2][232], const int8_t *d, uint32_t s0, uint32_t s1, uint32_t pk)
{
    memset(wave, 0, 2 * 232);
    if (!d || s1 <= s0 || !pk) return;
    uint32_t span = s1 - s0;
    for (int x = 0; x < 232; x++) {
        uint32_t a = s0 + (uint32_t)((uint64_t)span * (uint32_t)x / 232u), b = s0 + (uint32_t)((uint64_t)span * (uint32_t)(x + 1) / 232u);
        if (b <= a) b = a + 1;
        if (a >= s1) break;
        if (b > s1) b = s1;
        uint32_t mx = 0, sq = 0;
        for (uint32_t i = a; i < b; i++) { uint32_t u = (uint32_t)(d[i] < 0 ? -d[i] : d[i]); if (u > mx) mx = u; sq += u * u; }
        uint32_t rms = isqrt32(sq / (b - a));
        mx = mx * 255u / pk; rms = rms * 255u / pk;
        wave[0][x] = (uint8_t)(mx > 255 ? 255 : mx);
        wave[1][x] = (uint8_t)(rms > 255 ? 255 : rms);
    }
}

bool bm_app_src_ok(const bm_app *a, uint8_t src)
{
    if (src & BM_SRC_ROM) return src != BM_SRC_NONE && (src & 0x7F) < bm_rom_nloops;
    return src < BM_TAKES && a->take[src].valid;
}

bool bm_app_src_info(const bm_app *a, uint8_t src, bm_smp_src *out)
{
    memset(out, 0, sizeof *out);
    out->kslice = out->sslice = 0xFF;
    if (!bm_app_src_ok(a, src)) return false;
    if (src & BM_SRC_ROM) {
        const bm_rom_loop *l = &bm_rom_loops[src & 0x7F];
        out->data = l->data; out->len = l->len; out->bpm_x10 = l->bpm_x10; out->bars = l->bars; out->gain = 64;
        return true;
    }
    const bm_take *t = &a->take[src];
    out->data = t->data; out->len = t->len; out->bpm_x10 = t->bpm_x10; out->bars = t->bars;
    out->gain = t->gain; out->nmarks = t->nmarks;
    memcpy(out->marks, t->marks, sizeof out->marks);
    return true;
}

void bm_app_src_name(const bm_app *a, uint8_t src, char *buf, int cap)
{
    if (src == BM_SRC_NONE) snprintf(buf, (size_t)cap, "NONE");
    else if (src & BM_SRC_ROM) snprintf(buf, (size_t)cap, "%s", (src & 0x7F) < bm_rom_nloops ? bm_rom_loops[src & 0x7F].name : "FAC ?");
    else snprintf(buf, (size_t)cap, TF("TAKE %02d%s"), src + 1, bm_app_src_ok(a, src) ? "" : T(" EMPTY"));
}

int bm_app_sources(const bm_app *a, uint8_t out[BM_TAKES + 8])
{
    int n = 0;
    for (int i = 0; i < bm_rom_nloops && i < 8; i++) out[n++] = (uint8_t)(BM_SRC_ROM | i);
    for (int i = 0; i < BM_TAKES; i++) if (a->take[i].valid) out[n++] = (uint8_t)i;
    return n;
}

void app_send_takes(bm_app *a)
{
    for (int i = 0; i < BM_TAKES; i++) bm_app_src_info(a, (uint8_t)i, &a->take_send[i]);
    app_push_ptr(a, CMD_SMP_TAKES, 0, a->take_send);
}

bool app_take_info(const bm_app *a, int src, bm_smp_src *m)
{
    if (src == BM_TAKE_NEW) {                                /* la recién grabada, todavía en la arena */
        *m = a->ctx.smp[0];
        return a->ctx.st.smp_unsaved && !a->ctx.st.smp_rec && m->len != 0;
    }
    return src >= 0 && src < BM_TAKES && bm_app_src_info(a, (uint8_t)src, m);
}

/* La ventana de la onda: la toma entera (zoom 0) o, con zoom, alrededor de la muestra at (a un
 * tercio, para ver más de lo que sigue). wave_ov guarda la toma entera para el mapa de arriba. */
void app_wave_view(bm_app *a, int src, uint32_t at, int zoom)
{
    bm_smp_src m;
    if (!app_take_info(a, src, &m) || !m.len) {
        if (a->cut_wd) { memset(a->wave_cut, 0, sizeof a->wave_cut); memset(a->wave_ov, 0, sizeof a->wave_ov); }
        a->cut_wd = NULL; a->cut_wlen = 0; a->cut_v0 = a->cut_v1 = 0;
        return;
    }
    bool fresh = m.data != a->cut_wd || m.len != a->cut_wlen;
    if (fresh) {
        a->cut_pk = wave_peak(m.data, m.len);
        uint8_t w[2][232];
        wave_cols(w, m.data, 0, m.len, a->cut_pk);
        memcpy(a->wave_ov, w[0], sizeof a->wave_ov);
        a->cut_wd = m.data; a->cut_wlen = m.len;
    }
    uint32_t span = m.len >> (zoom & 3);
    if (span < 232) span = m.len < 232 ? m.len : 232;
    if (at > m.len) at = m.len;
    uint32_t v0 = at > span / 3 ? at - span / 3 : 0;
    if (v0 + span > m.len) v0 = m.len - span;
    if (!fresh && v0 == a->cut_v0 && v0 + span == a->cut_v1) return;
    a->cut_v0 = v0; a->cut_v1 = v0 + span;
    wave_cols(a->wave_cut, m.data, a->cut_v0, a->cut_v1, a->cut_pk);
}

/* El kit en el patrón: n pads desde los cortes marks[] de la toma src. Si seq, escribe en la
 * rejilla de T2 la secuencia que suena como la toma (cada pad en el paso donde empieza). */
static void kit_layout(bm_pattern *p, const bm_take *tk, int src, const uint32_t *marks, int n, bool seq)
{
    bm_tr *tr = &p->tr[1];
    for (int i = 0; i < BM_LANES; i++) {
        bm_pad *d = &p->pad[i];
        bm_pad_clear(d);
        if (i >= n) continue;
        uint32_t e = i + 1 < n ? marks[i + 1] : tk->len;
        d->src = (uint8_t)src; d->start = (uint16_t)marks[i]; d->len = (uint16_t)(e - marks[i]);
        d->flags = PAD_TEMPO;                                /* un loop cortado sigue el tempo */
        tr->lane[i].choke = 1;                               /* y sus cortes se cortan entre sí, como el loop */
        tr->lane[i].level = 100;
    }
    if (!seq) return;
    int steps = 16 * bm_clampi(tk->bars ? tk->bars : 1, 1, 4);
    for (int l = 0; l < BM_LANES; l++) tr->lane[l].len = (uint8_t)steps;
    for (int i = 0; i < n; i++) {
        int s = (int)(((uint64_t)marks[i] * (uint32_t)steps + tk->len / 2) / tk->len);
        if (s < steps && !(tr->st[i][s] & ST_ON)) tr->st[i][s] = ST_ON;
    }
}

static bool grid_empty(const bm_tr *tr)
{
    for (int l = 0; l < BM_LANES; l++)
        for (int s = 0; s < BM_STEPS; s++) if (tr->st[l][s] & ST_ON) return false;
    return true;
}

int app_kit_from_take(bm_app *a, int src, int how)
{
    if (src < 0 || src >= BM_TAKES || !a->take[src].valid) return 0;
    const bm_take *tk = &a->take[src];
    uint32_t m[BM_SLICES];
    int n;
    if (how == 0) {
        uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10;
        n = bm_slice_onsets(tk->data, tk->len, (uint32_t)BM_SMP_RATE * 150u / bpm, a->slice_sens, m);
    } else {
        n = how > BM_SLICES ? BM_SLICES : how;
        for (int i = 0; i < n; i++) m[i] = tk->len * (uint32_t)i / (uint32_t)n;
    }
    app_undo_snapshot(a, 1);
    kit_layout(&a->pat, tk, src, m, n, grid_empty(&a->pat.tr[1]));
    a->sel_lane[1] = 0;
    app_edited(a);
    return n;
}

void app_pad_take(bm_app *a, int pad, int src)
{
    if (src < 0 || src >= BM_TAKES || !a->take[src].valid) return;
    bm_pad *d = &a->pat.pad[pad & 15];
    bm_pad_clear(d);
    d->src = (uint8_t)src; d->len = (uint16_t)a->take[src].len;
    app_edited(a);
}

/* ------------------------------------------------------------ idioma */
const uint8_t *bm_app_lang_pack(const bm_app *a)
{
    if (!a->pf.flash_map) return NULL;
    const uint8_t *p = a->pf.flash_map(a->pf.user, BM_LANG_LO);
    return bm_lang_check(p, BM_LANG_HI - BM_LANG_LO, NULL) ? p : NULL;
}

int bm_app_lang_load(bm_app *a, int sub, uint32_t off, const uint8_t *d, uint32_t n)
{
    const bm_flash_ops *f = a->pf.flash;
    if (!f || app_playing(a) || a->njob) return -1;        /* escribir la flash corta el audio */
    if (sub == 0 || sub == 3) {
        bm_lang_use(NULL);                                  /* lo que se borra ya no se lee */
        for (uint32_t s = BM_LANG_LO; s < BM_LANG_HI; s += BM_SECTOR) if (f->erase(s)) return -1;
        if (sub == 3) { a->set.lang = 0; a->set_dirty = 1; a->t_edit = a->now; app_toast(a, "ENGLISH"); }
        return 0;
    }
    if (sub == 1) {
        if (!d || off > BM_LANG_HI - BM_LANG_LO || n > BM_LANG_HI - BM_LANG_LO - off) return -1;
        while (n) {                                         /* de a una página, sin cruzarla */
            uint32_t addr = BM_LANG_LO + off, k = BM_PAGE - (addr % BM_PAGE);
            if (k > n) k = n;
            if (f->program(addr, d, k)) return -1;
            off += k; d += k; n -= k;
        }
        return 0;
    }
    if (sub != 2) return -1;
    const uint8_t *p = bm_app_lang_pack(a);
    if (!p) { app_toast(a, "LANGUAGE PACK IS DAMAGED"); return -1; }
    a->set.lang = 1; a->set_dirty = 1; a->t_edit = a->now;
    bm_lang_use(p);
    app_toast(a, bm_lang_name());
    return 0;
}

int bm_app_sound_load(bm_app *a, int sub, uint32_t off, const uint8_t *d, uint32_t n)
{
    const bm_flash_ops *f = a->pf.flash;
    if (!f || app_playing(a) || a->njob) return -1;        /* escribir la flash corta el audio */
    switch (sub) {
    case 0: a->snap_len = 0; return 0;
    case 1:
        if (!d || off > BM_SNAP_MAX || n > BM_SNAP_MAX - off) return -1;
        memcpy(a->snap_buf + off, d, n);
        if (off + n > a->snap_len) a->snap_len = (uint16_t)(off + n);
        return 0;
    case 3: {                                               /* off = máquina << 8 | su número en PRESETS */
        int eng = (int)(off >> 8), idx = (int)(off & 0xFF);
        const uint8_t *p = eng == ENG_FM ? bm_snap_get(ENG_FM, idx) : bm_snap_get(eng, idx);
        int u = p ? bm_snaps_user_of(p) : -1;
        int r = u < 0 ? -1 : bm_snaps_erase(f, u);
        app_toast(a, r ? "NOT A USER SOUND" : "SOUND DELETED");
        return r;
    }
    case 2: break;
    default: return -1;
    }
    if (!bm_snap_ok(a->snap_buf, a->snap_len)) { app_toast(a, "THAT IS NOT A BUBBA SOUND"); return -1; }
    uint8_t used[BM_TAKE_SECTORS];
    bm_takes_used(a->take, used);
    int i = bm_snaps_write(f, a->pf.flash_map, a->pf.user, a->snap_buf, a->snap_len, used);
    if (i < 0) { app_toast(a, "NO ROOM: DELETE A TAKE OR A SOUND"); return -1; }
    char b[30];
    if (a->snap_buf[4] == ENG_FM) snprintf(b, sizeof b, TF("BX-7 BANK: %d VOICES"), 32);
    else { char nm[13]; memcpy(nm, a->snap_buf + 12, 12); nm[12] = 0; snprintf(b, sizeof b, TF("SOUND SAVED: %s"), nm); }
    app_toast(a, b);
    return 0;
}

void bm_app_takes_reload(bm_app *a)
{
    if (a->pf.flash) bm_takes_scan(a->pf.flash, a->pf.flash_map, a->pf.user, a->take);
    app_send_takes(a);
}

uint32_t bm_app_rec_room(const bm_app *a)
{
    uint32_t r = bm_takes_room(a->take);
    return r > BM_SMP_REC_MAX ? BM_SMP_REC_MAX : r;
}

/* ------------------------------------------------------------ trabajos de flash (solo en STOP) */
void app_job(bm_app *a, int type, int arg)
{
    for (int i = 0; i < a->njob; i++) if (a->job[i] == type && a->job_arg[i] == arg) return;
    if (a->njob < BM_JOBS) { a->job[a->njob] = (uint8_t)type; a->job_arg[a->njob] = (uint8_t)arg; a->njob++; }
}

void app_save_jobs(bm_app *a, int with_cur)
{
    if (a->pend_obj >= 0) app_job(a, JOB_FLUSH_PEND, 0);
    if (with_cur) app_job(a, JOB_SAVE_CUR, 0);
}

static int write_pattern(bm_app *a, int obj)
{
    int n = bm_pattern_pack(&a->pat, a->pend, BM_PENDING_LEN);
    if (n < 0) { app_toast(a, "PATTERN FULL: NO ROOM"); return 0; }
    return bm_store_write(&a->store, (uint8_t)obj, a->pend, (uint16_t)n);
}

static int load_into(bm_app *a, int obj)
{
    int len = 0;
    const uint8_t *src = store_ptr(a, obj, &len);
    if (src && len > 0 && bm_pattern_unpack(&a->pat, src, len) > 0) return 1;
    empty_from(&a->pat);
    return 0;
}

/* Copia un canal (o el patrón entero) a otro patrón. Corre parado y con el audio en fade:
 * guarda el actual, abre el destino, pega, lo guarda y vuelve a abrir el actual. */
static int copy_to(bm_app *a, int dst, int trk)
{
    int cur = a->ctx.st.pattern;
    if (a->pend_obj >= 0) return -1;
    if (write_pattern(a, cur)) return -1;
    a->dirty = 0;
    if (trk >= 0) {                                    /* el buffer de rehacer sirve de bolsa */
        memcpy(&a->redo, app_track_data(a, trk), app_track_size(trk));
        a->redo_track = -1;
        bm_track tk = a->pat.trk[trk];
        bm_loop lp;
        memset(&lp, 0, sizeof lp);
        if (trk >= BM_TR_TRACKS) lp = a->pat.loop[trk - BM_TR_TRACKS];
        load_into(a, dst);
        memcpy(app_track_data(a, trk), &a->redo, app_track_size(trk));
        a->pat.trk[trk] = tk;
        if (trk >= BM_TR_TRACKS) a->pat.loop[trk - BM_TR_TRACKS] = lp;
    }
    int err = write_pattern(a, dst);
    load_into(a, cur);
    bm_engines_sync(&a->ctx);
    return err;
}

static int exec_job(bm_app *a, int type, int arg)
{
    char b[30];
    switch (type) {
    case JOB_FLUSH_PEND:
        if (a->pend_obj >= 0) {
            if (bm_store_write(&a->store, (uint8_t)a->pend_obj, a->pend, a->pend_len)) return -1;
            a->pend_obj = -1;
        }
        return 0;
    case JOB_SAVE_CUR:
    case JOB_SAVE_AS: {
        if (a->pend_obj >= 0) return -1;
        int obj = type == JOB_SAVE_AS ? arg : a->ctx.st.pattern;
        if (write_pattern(a, obj)) return -1;
        if (type == JOB_SAVE_AS) { a->ctx.cur_pat = (uint8_t)obj; a->ctx.st.pattern = (uint8_t)obj; a->set.last_pat = (uint8_t)obj; a->set_dirty = 1; }
        a->dirty = 0;
        return 0;
    }
    case JOB_COPY_PAT: return copy_to(a, arg, -1);
    case JOB_COPY_TRK: return copy_to(a, arg & 15, arg >> 4);
    case JOB_SAVE_SONG: {
        uint8_t buf[1 + 2 * BM_SONG_MAX + sizeof a->song.scene];
        buf[0] = a->song.n;
        memcpy(buf + 1, a->song.pat, BM_SONG_MAX);
        memcpy(buf + 1 + BM_SONG_MAX, a->song.rep, BM_SONG_MAX);
        memcpy(buf + 1 + 2 * BM_SONG_MAX, a->song.scene, sizeof a->song.scene);
        if (bm_store_write(&a->store, BM_OBJ_SONG, buf, sizeof buf)) return -1;
        a->song_dirty = 0;
        return 0;
    }
    case JOB_SAVE_GLOB:
        a->set.ver = BM_FMT;
        if (bm_store_write_glob(&a->store, (const uint8_t *)&a->set, sizeof a->set)) return -1;
        a->set_dirty = 0;
        return 0;
    case JOB_LOAD:
        app_push(a, CMD_LOAD, (uint8_t)arg, 0, 0);
        return 0;
    case JOB_SAVE_TAKE: {
        const bm_smp_src *m = &a->ctx.smp[0];
        if (!a->ctx.st.smp_unsaved || !m->len) return 0;
        int id = bm_takes_write(a->pf.flash, a->pf.flash_map, a->pf.user, a->take, m->data, m->len, m->bpm_x10, m->bars);
        if (id < 0) {
            app_push_ptr(a, CMD_SMP_DISCARD, 0, &a->smp_send[0]);
            app_toast(a, "NO ROOM IN TAKES: DISCARDED");
            return 0;
        }
        bm_app_src_info(a, (uint8_t)id, &a->smp_send[0]);
        app_push_ptr(a, CMD_SMP_SAVED, 0, &a->smp_send[0]);
        app_send_takes(a);
        uint8_t list[BM_TAKES + 8];
        int nl = bm_app_sources(a, list);
        for (int i = 0; i < nl; i++) if (list[i] == id) a->takes_row = (uint8_t)i;
        if (app_kit_empty(a)) {                               /* el kit estaba vacío: la toma lo arma */
            app_kit_from_take(a, id, 0);
            snprintf(b, sizeof b, TF("TAKE %02d: KIT READY"), id + 1);
        } else snprintf(b, sizeof b, TF("TAKE %02d SAVED: TAKES > KIT"), id + 1);
        app_toast(a, b);
        return 0;
    }
    case JOB_TAKE_META:
        if (arg < BM_TAKES && a->take[arg].valid) {
            if (a->pend_obj >= 0) return -1;
            if (bm_takes_update(a->pf.flash, &a->take[arg], a->pend)) return -1;
        }
        a->take_dirty &= (uint8_t)~(1u << arg);
        return 0;
    case JOB_TAKE_ERASE:
        if (arg < BM_TAKES && a->take[arg].valid) {
            if (bm_takes_erase(a->pf.flash, &a->take[arg])) return -1;
            for (int i = 0; i < BM_LANES; i++) if (a->pat.pad[i].src == arg) bm_pad_clear(&a->pat.pad[i]);   /* sus pads quedan vacíos */
            app_send_takes(a);
            app_edited(a);
        }
        return 0;
    }
    return 0;
}

static void jobs_run(bm_app *a)
{
    if (!a->njob) return;
    int flash = 0;
    for (int i = 0; i < a->njob; i++) if (a->job[i] != JOB_LOAD) flash = 1;
    if (flash) {
        if (app_playing(a)) { app_toast(a, "STOP TO SAVE"); a->njob = 0; return; }
        if (!a->pf.flash) { a->njob = 0; return; }
        if (a->cmdq.r != a->cmdq.w) return;
        if (!a->job_fading) { app_push(a, CMD_FADE, 1, 0, 0); a->job_fading = 1; return; }
        if (!a->ctx.st.faded) return;
    }
    int err = 0;
    for (int i = 0; i < a->njob; i++) err |= exec_job(a, a->job[i], a->job_arg[i]);
    a->njob = 0;
    if (a->job_fading) { app_push(a, CMD_FADE, 0, 0, 0); a->job_fading = 0; }
    if (err) app_toast(a, "FLASH ERROR");
}

/* ------------------------------------------------------------ demo de primer arranque */
static void ev_set(bm_pattern *p, int t, int s, int k, int note, int vel, int len, int prob)
{
    bm_event *e = bm_ev(p, t, s, k);
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = (uint8_t)prob;
}

/* Patrón de pruebas: acid techno a 160 BPM en La menor. BR-8 con el kit 909, una línea de BB-303
 * con acentos y slides, BP-1 cortando la primera toma (si hay), acordes de BX-7, un pluck de BITTAL
 * y un pad de BUNO-6 de cuatro compases (una nota larga). */
static void tr_on(bm_tr *d, int lane, const char *pat, uint8_t extra)
{
    for (int s = 0; pat[s] && s < BM_STEPS; s++)
        if (pat[s] == 'x' || pat[s] == 'X') d->st[lane][s] = (uint8_t)(ST_ON | (pat[s] == 'X' ? ST_ACC : 0) | extra);
}

static void demo_pattern(bm_pattern *p, bm_song *song, const bm_take *tk, uint8_t take)
{
    bm_pattern_init(p);
    p->meta.bpm_x10 = 1600; p->meta.swing = 50; p->meta.root = 9; p->meta.scale = 1;
    p->meta.master = 4; p->meta.fill_every = 4; p->meta.chord_set = 0; p->meta.strum = 0;
    p->meta.dly_time = 2; p->meta.dly_fb = 45; p->meta.dly_mix = 40;
    /* T1: 909 */
    bm_tr *d = &p->tr[0];
    bm_kit_apply(d, 1);
    p->trk[0].source = 1;
    tr_on(d, 0, "x...x...x...x...", 0);                                 /* bombo */
    d->st[0][14] = ST_ON | (1 << ST_COND_SH);                           /* y uno más en el fill */
    tr_on(d, 2, ".x.x.x.x.x.x.x.x", 0);                                 /* hat cerrado */
    tr_on(d, 3, "..x...x...x.....", 0);                                 /* hat abierto a contratiempo */
    d->st[2][14] = ST_ON | ST_ACC | (1 << ST_RAT_SH);                   /* y un redoble x2 */
    tr_on(d, 5, "....x.......x...", 0);                                 /* palmas */
    tr_on(d, 6, "...x.......x..x.", 0);                                 /* rim */
    d->lane[6].level = 70;
    tr_on(d, 12, "xxxxxxxxxxxx", 0);                                    /* shaker de 12 contra 16 */
    d->lane[12].len = 12; d->lane[12].level = 45;
    d->st[15][15] = ST_ON | (1 << ST_COND_SH);                          /* zap en el fill */
    d->lane[1].level = 0;
    /* T2: el kit de pads sale de la primera toma (en los golpes) y la rejilla la toca en orden */
    p->trk[1].level = 70;
    if (tk) {
        uint32_t m[BM_SLICES];
        uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : 1600;
        int n = bm_slice_onsets(tk->data, tk->len, (uint32_t)BM_SMP_RATE * 150u / bpm, 64, m);
        kit_layout(p, tk, take, m, n, true);
    }
    /* T3: ACID, la línea 303 (acento = vel 127, LIGA = slide) */
    bm_track_set_engine(p, 2, ENG_ACID);
    bm_track *t3 = &p->trk[2];
    t3->macro[0] = 28; t3->macro[1] = 108; t3->macro[2] = 58; t3->macro[3] = 105; t3->glide = 45; t3->accent = 115;
    t3->length = 16; t3->send = 25;
    static const struct { int8_t s, n; uint8_t acc, slide; } L[] = {
        {0, 45, 1, 0}, {1, 45, 0, 0}, {2, 57, 1, 1}, {3, 45, 0, 0}, {4, 48, 0, 0}, {5, 45, 0, 1}, {6, 52, 1, 0},
        {8, 45, 0, 0}, {9, 57, 1, 0}, {10, 55, 0, 1}, {11, 52, 0, 0}, {12, 45, 1, 0}, {13, 48, 0, 1}, {14, 50, 0, 0}, {15, 45, 0, 0}};
    for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++)
        ev_set(p, 2, L[i].s, 0, L[i].n, L[i].acc ? BM_VEL_ACC : BM_VEL_NORMAL, 1 | (L[i].slide ? BM_LEN_TIE : 0), 100);
    /* T4: BX-7, acordes de E.PIANO a contratiempo, con delay */
    p->trk[3].source = 0; p->trk[3].length = 32; p->trk[3].send = 45; p->trk[3].rsend = 30;
    for (int k = 0; k < 3; k++) { static const int C[3] = {57, 60, 64}; ev_set(p, 3, 10, k, C[k], 90, 1, 100); ev_set(p, 3, 26, k, C[k] - 2 + (k == 1), 80, 1, 100); }
    /* T5: BITTAL, un pluck que a veces no está */
    p->trk[4].source = 13; bm_preset_apply(p, 4, 13);
    p->trk[4].length = 32; p->trk[4].send = 35; p->trk[4].rsend = 60;
    { static const int8_t S[6] = {2, 6, 14, 18, 22, 30}, N[6] = {69, 72, 76, 74, 72, 76};
      for (int i = 0; i < 6; i++) ev_set(p, 4, S[i], 0, N[i], 85, 1, i % 3 == 2 ? 60 : 100); }
    /* T6: BUNO-6, un pad de La menor que dura los cuatro compases */
    p->trk[5].source = 2; bm_preset_apply(p, 5, 2);
    p->trk[5].length = 64; p->trk[5].send = 0; p->trk[5].rsend = 70;
    for (int k = 0; k < 3; k++) { static const int C[3] = {57, 60, 64}; ev_set(p, 5, 0, k, C[k], 75, 64, 100); }
    p->trk[0].level = 84; p->trk[2].level = 66; p->trk[3].level = 32; p->trk[4].level = 36; p->trk[5].level = 26;
    p->meta.rv_mix = 70;
    memset(song->scene, 0, sizeof song->scene);
    song->scene[0].valid = 1; song->scene[0].pattern = 0;                                  /* A: todo */
    song->scene[1].valid = 1; song->scene[1].pattern = 0; song->scene[1].trk_mute = 0x38;  /* B: batería, voces y acid */
    song->scene[2].valid = 1; song->scene[2].pattern = 0; song->scene[2].trk_mute = 0x01;  /* C: sin batería */
    song->scene[3].valid = 1; song->scene[3].pattern = 0; song->scene[3].trk_mute = 0x3A;  /* D: batería y acid */
}

/* 0.4: sin loops de fábrica. El sampler toca tomas: las que se graban o se suben con
 * tools/fm1/toma.py. (El código de las fuentes de ROM queda sin fuentes.) */
const bm_rom_loop bm_rom_loops[1] = {{0, 0u, 0u, 0u, ""}};
const int bm_rom_nloops = 0;

/* ------------------------------------------------------------ init */
static void midi_out_cb(void *u, uint8_t b)
{
    bm_app *a = (bm_app *)u;
    if (a->pf.midi_out) a->pf.midi_out(a->pf.user, b);
}

static void settings_defaults(bm_settings *s)
{
    memset(s, 0, sizeof *s);
    s->ver = BM_FMT; s->sync_ppqn = 2; s->drive = 25; s->led_dim = 1; s->autosave = 1; s->enter = 2;
}

static void apply_settings(bm_app *a)
{
    app_push(a, CMD_CLOCK, a->set.clock, 0, 0);
    app_push(a, CMD_SYNC, a->set.sync_mode, a->set.sync_ppqn, a->set.sync_lat);
    app_push(a, CMD_DRIVE, a->set.drive, 0, 0);
}

void bm_app_init(bm_app *a, const bm_platform *pf, uint32_t now)
{
    memset(a, 0, sizeof *a);
    a->pf = *pf;
    a->now = now;
    bm_tables_init();
    bm_pattern_init(&a->pat);
    bm_audio_init(&a->ctx, &a->pat, &a->song, &a->cmdq, &a->amem);
    a->ctx.swap = app_swap; a->ctx.swap_user = a;
    a->ctx.midi_out = midi_out_cb; a->ctx.midi_user = a;
    bm_gesture_init(&a->ges, app_on_gesture, a);
    a->pend_obj = -1; a->undo_track = -1; a->redo_track = -1; a->held_step = -1; a->held_white = -1;
    a->slice_sens = 64; a->pad_key = -1; a->rec_gate = 100; a->tie_from = -1; a->rec_step = -1;
    a->pat_sel = -1; a->punch_key = -1; a->last_pad = -1; a->roll_rate = 1; a->solo_key = -1; a->solo_track = -1;
    static const uint8_t ln[BM_TRACKS] = {0, 0, 45, 57, 60, 64};
    memcpy(a->last_note, ln, sizeof ln);
    for (int k = 0; k < BM_KEYS; k++) a->key_rec_step[k] = -1;
    settings_defaults(&a->set);
    a->battery = 100;

    int fresh = 1;
    if (pf->flash && bm_store_mount(&a->store, pf->flash) == 0) {
        bm_settings s;
        int has_glob = bm_store_read_glob(&a->store, (uint8_t *)&s, sizeof s) == (int)sizeof s;
        int ok = has_glob && s.ver == BM_FMT, old = has_glob && !ok, oldpat = 0;
        for (int i = 0; i < BM_PATTERNS; i++) {    /* patrones de otro formato: hasta la v1 no se convierten */
            int len = 0;
            const uint8_t *p = store_ptr(a, i, &len);
            if (p && len > (int)offsetof(bm_meta, fmt) && p[offsetof(bm_meta, fmt)] != BM_PAT_FMT) oldpat = 1;
        }
        if (old) {                                 /* datos de 0.2: se empieza de cero */
            bm_store_format(&a->store, pf->flash);
            bm_takes_format(pf->flash);
        } else if (oldpat) {                       /* patrones de 0.3: se borran (canción y ajustes se rehacen); las tomas se quedan */
            bm_store_format(&a->store, pf->flash);
            ok = 0;
        }
        if (ok) a->set = s;
        uint8_t b[1 + 2 * BM_SONG_MAX + sizeof a->song.scene];
        if (ok && bm_store_read(&a->store, BM_OBJ_SONG, b, sizeof b) == (int)sizeof b) {
            a->song.n = b[0] > BM_SONG_MAX ? BM_SONG_MAX : b[0];
            memcpy(a->song.pat, b + 1, BM_SONG_MAX);
            memcpy(a->song.rep, b + 1 + BM_SONG_MAX, BM_SONG_MAX);
            memcpy(a->song.scene, b + 1 + 2 * BM_SONG_MAX, sizeof a->song.scene);
        }
        for (int i = 0; i < BM_PATTERNS; i++) if (bm_store_has(&a->store, (uint8_t)i)) fresh = 0;
        int len = 0;
        const uint8_t *src = store_ptr(a, a->set.last_pat % BM_PATTERNS, &len);
        bm_snaps_scan(pf->flash, pf->flash_map, pf->user);     /* los sonidos del usuario, antes que todo lo demás */
        if (src && len > 0 && bm_pattern_unpack(&a->pat, src, len) < 0) bm_pattern_init(&a->pat);
        bm_takes_scan(pf->flash, pf->flash_map, pf->user, a->take);
    }
    bm_lang_use(a->set.lang ? bm_app_lang_pack(a) : NULL);
    uint8_t first = BM_SRC_NONE;
    for (int i = BM_TAKES - 1; i >= 0; i--) if (a->take[i].valid) first = (uint8_t)i;
    if (fresh) { demo_pattern(&a->pat, &a->song, first < BM_TAKES ? &a->take[first] : NULL, first); a->dirty = 1; a->song_dirty = 1; a->set_dirty = 1; a->t_edit = now; }
    a->set.last_pat %= BM_PATTERNS;
    a->ctx.cur_pat = a->set.last_pat;
    a->ctx.st.pattern = a->set.last_pat;
    bm_engines_sync(&a->ctx);
    bm_seq_set_bpm(&a->ctx, a->pat.meta.bpm_x10);
    apply_settings(a);
    app_send_takes(a);
    a->view = V_BOOT;
    a->page = PG_SEQ;
    a->boot_t = now;
    a->scr.force = 1;
}

void bm_app_audio(bm_app *a, int32_t *out, uint32_t frames) { bm_audio_render(&a->ctx, out, frames); }
void bm_app_midi_in(bm_app *a, uint8_t b) { app_push(a, CMD_MIDI, b, 0, 0); }
void bm_app_battery(bm_app *a, uint8_t pct) { a->battery = pct > 100 ? 100 : pct; }

void bm_app_master(bm_app *a, uint16_t adc)
{
    a->master_adc = (uint16_t)((a->master_adc * 3u + adc) / 4u);
    int d = (int)a->master_adc - (int)a->master_sent;
    if (d > 4 || d < -4 || (adc == 0 && a->master_sent) || (adc >= 1023 && a->master_sent < 1023)) {
        a->master_sent = (adc == 0 || adc >= 1023) ? adc : a->master_adc;
        app_push_i(a, CMD_MASTER, 0, 0, 0, a->master_sent);
    }
}

/* ------------------------------------------------------------ navegación */
static const uint8_t TABS_TR[] = {PG_SEQ, PG_SOUND, PG_INST, PG_MIX, PG_MORE};
static const uint8_t TABS_PADS[] = {PG_SEQ, PG_PADS, PG_BREAK, PG_TAKES, PG_MIX, PG_MORE};
static const uint8_t TABS_NOTE[] = {PG_SEQ, PG_SOUND, PG_ENV, PG_MIX, PG_ARP, PG_LOOP, PG_MORE};
static const uint8_t TABS_WV[] = {PG_SEQ, PG_SOUND, PG_MIX, PG_ARP, PG_LOOP, PG_MORE};   /* ENV y LFO están en SONIDO */

int bm_app_tabs(const bm_app *a, int t, uint8_t out[8])
{
    const uint8_t *s; int n;
    if (bm_is_pads(t)) { s = TABS_PADS; n = (int)sizeof TABS_PADS; }
    else if (app_kind(a, t) == KIND_TR) { s = TABS_TR; n = (int)sizeof TABS_TR; }
    else if (bm_engine_paged(a->pat.trk[t].engine)) { s = TABS_WV; n = (int)sizeof TABS_WV; }
    else { s = TABS_NOTE; n = (int)sizeof TABS_NOTE; }
    memcpy(out, s, (size_t)n);
    return n;
}

const char *bm_page_name(int page)
{
    static const char *const N[PG_COUNT] = {"SEQ", "SOUND", "INSTR", "MIX", "MORE", "BREAK", "TAKES", "ENV", "ARP", "LOOP",
                                            "MIDI", "SCALE", "CHORDS", "LFO", "RECORD", "PADS"};
    return page < PG_COUNT ? N[page] : "";
}

int app_page_ok(const bm_app *a, int t, int page)
{
    if (page == PG_SCL || page == PG_CHORDS || page == PG_LFO) return 1;
    if (page == PG_REC) return t == 1;
    uint8_t tabs[8];
    int n = bm_app_tabs(a, t, tabs);
    for (int i = 0; i < n; i++) if (tabs[i] == page) return 1;
    return 0;
}

void app_set_view(bm_app *a, int v)
{
    if (v == V_GLO && a->view != V_GLO) { a->prev_view = a->view; a->prev_page = a->page; }
    a->view = (uint8_t)v;
    a->ov.on = 0;
    a->clear_armed = 0;
}

void app_enter(bm_app *a, int t, int page)
{
    a->track = (uint8_t)(t % BM_TRACKS);
    if (!app_page_ok(a, a->track, page)) page = PG_SEQ;
    if (a->view == V_CH && a->page == PG_PADS && page != PG_PADS && a->take_dirty)   /* la ganancia de la toma se guarda al salir */
        for (int i = 0; i < BM_TAKES; i++) if (a->take_dirty & (1u << i)) app_job(a, JOB_TAKE_META, i);
    a->page = (uint8_t)page;
    if (page != PG_REC) a->ch_page[a->track] = (uint8_t)page;
    a->held_step = -1; a->held_white = -1;
    app_set_view(a, V_CH);
}

void app_select_track(bm_app *a, int t, int keep_page)
{
    if (t < 0 || t >= BM_TRACKS) return;
    a->track = (uint8_t)t;
    a->held_step = -1; a->held_white = -1;
    if (a->view == V_CH) {
        int pg = keep_page && app_page_ok(a, t, a->page) && a->page != PG_REC ? a->page : a->ch_page[t];
        if (!app_page_ok(a, t, pg)) pg = PG_SEQ;
        a->page = (uint8_t)pg;
        a->ch_page[t] = (uint8_t)pg;
        int len = bm_trk_len(&a->pat, t, a->sel_lane[t]);
        if (a->step_page * 16 >= len) a->step_page = 0;
    }
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, T(bm_track_name(a, t)));
    app_toast(a, b);
}

/* ------------------------------------------------------------ undo, portapapeles */
void *app_track_data(bm_app *a, int t) { return t < BM_TR_TRACKS ? (void *)&a->pat.tr[t] : (void *)&a->pat.nt[t - BM_TR_TRACKS]; }
size_t app_track_size(int t) { return t < BM_TR_TRACKS ? sizeof(bm_tr) : sizeof(bm_notes); }

void app_undo_snapshot(bm_app *a, int t)
{
    memcpy(&a->undo, app_track_data(a, t), app_track_size(t));
    a->undo_track = (int8_t)t;
    a->redo_track = -1;
}

void app_do_undo(bm_app *a)
{
    if (a->undo_track < 0) { app_toast(a, "NOTHING TO UNDO"); return; }
    int t = a->undo_track;
    memcpy(&a->redo, app_track_data(a, t), app_track_size(t));
    a->redo_track = (int8_t)t;
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->undo);
    a->undo_track = -1;
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, TF("UNDONE ON T%d"), t + 1);
    app_toast(a, b);
}

void app_do_redo(bm_app *a)
{
    if (a->redo_track < 0) { app_toast(a, "NOTHING TO REDO"); return; }
    int t = a->redo_track;
    memcpy(&a->undo, app_track_data(a, t), app_track_size(t));
    a->undo_track = (int8_t)t;
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->redo);
    a->redo_track = -1;
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, TF("REDONE ON T%d"), t + 1);
    app_toast(a, b);
}

void app_clear_track(bm_app *a, int t)
{
    app_undo_snapshot(a, t);
    app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
    app_edited(a);
    char b[30];
    snprintf(b, sizeof b, TF("T%d ERASED (HOME+OCT- UNDOES)"), t + 1);
    app_toast(a, b);
}

void app_copy_track(bm_app *a, int t)
{
    memcpy(&a->clip, app_track_data(a, t), app_track_size(t));
    a->clip_trk = a->pat.trk[t];
    a->clip_type = 2; a->clip_track = (uint8_t)t;
    app_toast(a, "TRACK COPIED");
}

void app_paste_track(bm_app *a, int t)
{
    if (a->clip_type != 2 || app_kind(a, t) != bm_kind(&a->pat, a->clip_track) || (app_kind(a, t) == KIND_TR) != (a->clip_trk.engine == ENG_DRUM)) {
        app_toast(a, "NOTHING TO PASTE HERE"); return;
    }
    app_undo_snapshot(a, t);
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->clip);
    app_edited(a);
    app_toast(a, "TRACK PASTED");
}

void app_init_sound(bm_app *a, int t)
{
    bm_track d; bm_track_defaults(&d, t);
    static const uint8_t F[] = {0, 1, 2, 3, TRK_OFF(attack), TRK_OFF(sustain), TRK_OFF(release), TRK_OFF(env_amt), TRK_OFF(glide), TRK_OFF(transpose)};
    for (unsigned i = 0; i < sizeof F; i++) app_push(a, CMD_SET_TRACK, (uint8_t)t, F[i], ((const uint8_t *)&d)[F[i]]);
    app_edited(a);
    app_toast(a, "INIT SOUND");
}

void app_bass_machine(bm_app *a, int eng)
{
    if (!bm_engine_ok(2, eng) || a->pat.trk[2].engine == eng) return;
    app_push(a, CMD_ENGINE, 2, (uint8_t)eng, 0);
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, TF("T3 IS NOW %s"), bm_engine_names[eng]);
    app_toast(a, b);
}

void app_select_pattern(bm_app *a, int idx)
{
    char b[30];
    idx = bm_clampi(idx, 0, BM_PATTERNS - 1);
    if (app_playing(a)) {
        if (idx == a->ctx.st.pattern) { app_push(a, CMD_QUEUE, 0xFF, 0, 0); return; }
        if (a->dirty && a->pend_obj >= 0 && a->pend_obj != a->ctx.st.pattern) { app_toast(a, "STOP TO SAVE"); return; }
        app_push(a, CMD_QUEUE, (uint8_t)idx, 0, 0);
        snprintf(b, sizeof b, TF("A%02d QUEUED"), idx + 1);
    } else {
        if (idx == a->ctx.st.pattern) return;
        app_save_jobs(a, a->dirty);
        app_job(a, JOB_LOAD, idx);
        snprintf(b, sizeof b, TF("PATTERN A%02d"), idx + 1);
    }
    app_toast(a, b);
}

/* ------------------------------------------------------------ tick */
void bm_app_tick(bm_app *a, uint32_t now)
{
    a->now = now;
    bm_status *st = &a->ctx.st;
    if (a->view == V_BOOT && now - a->boot_t >= UI_BOOT_MS) { a->view = V_HOME; a->scr.force = 1; }
    bm_gesture_poll(&a->ges, now);
    app_input_tick(a);
    app_tables(a);
    if (a->pat_sel >= 0 && now - a->pat_sel_t > 500) { app_select_pattern(a, a->pat_sel); a->pat_sel = -1; }
    if (a->ov.on && (int32_t)(now - a->ov.until) > 0) a->ov.on = 0;
    if (a->toast_until && (int32_t)(now - a->toast_until) > 0) a->toast_until = 0;
    for (int t = 0; t < BM_TRACKS; t++)
        if (st->trig[t] != a->last_trig[t]) { a->last_trig[t] = st->trig[t]; a->trig_until[t] = now + 90; }
    for (int t = 0; t < BM_TRACKS; t++) {
        uint16_t m = st->lane_trig[t];
        if (m) { st->lane_trig[t] = 0; for (int l = 0; l < BM_LANES; l++) if (m & (1u << l)) a->lane_flash[t][l] = now + 90; }
    }
    if (st->edits != a->edits_seen) { a->edits_seen = st->edits; app_edited(a); }
    if (st->clip) { st->clip = 0; a->clip_until = now + 200; }
    if (st->swap_fail) { st->swap_fail = 0; app_toast(a, "NO CHANGE: STOP AND SAVE"); }
    if (a->layer == L_ERASE && app_playing(a) && a->erase_mask) {      /* BORRAR sonando */
        int t = a->track;
        if (bm_is_grid(&a->pat, t)) {
            const bm_tr *tr = bm_trk_trc(&a->pat, t);
            for (int l = 0; l < BM_LANES; l++)
                if (a->erase_mask & (1u << l)) {
                    int s = bm_app_track_step(a, t, l);
                    if (s >= 0 && tr->st[l][s]) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)l, (uint8_t)s, 0); app_edited(a); }
                }
        } else if (a->erase_syn) {
            int s = bm_app_track_step(a, t, -1), any = 0;
            for (int k = 0; s >= 0 && k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) any = 1;
            if (any) {
                bm_event e; bm_event_clear(&e);
                for (int k = 0; k < BM_EVENTS; k++) app_push_ev(a, t, s, k, &e);
                app_edited(a);
            }
        }
    }
    if (a->song_rec && app_playing(a) && st->loops != a->song_rec_loops && st->scene != 0xFF) {   /* SONG REC */
        a->song_rec_loops = st->loops;
        uint8_t e = (uint8_t)(0x80 | st->scene);
        if (a->song.n && a->song.pat[a->song.n - 1] == e && a->song.rep[a->song.n - 1] < 16) a->song.rep[a->song.n - 1]++;
        else if (a->song.n < BM_SONG_MAX) { a->song.pat[a->song.n] = e; a->song.rep[a->song.n] = 1; a->song.n++; }
        a->song_dirty = 1; a->t_edit = now;
    }
    if (st->smp_unsaved && !st->smp_rec && !app_playing(a) && !a->njob && a->pf.flash) {   /* toma nueva: al parar */
        app_job(a, JOB_SAVE_TAKE, 0);
        app_toast(a, "SAVING THE TAKE");
    }
    if (a->take_dirty && !app_playing(a) && !a->njob && a->pf.flash && now - a->t_edit > 1500
        && !(a->view == V_CH && a->page == PG_PADS))
        for (int i = 0; i < BM_TAKES; i++) if (a->take_dirty & (1u << i)) app_job(a, JOB_TAKE_META, i);
    if (a->set.autosave && !app_playing(a) && !a->njob && (a->dirty || a->pend_obj >= 0 || a->song_dirty || a->set_dirty)
        && now - a->t_edit > UI_AUTOSAVE_MS && a->pf.flash) {
        app_save_jobs(a, a->dirty);
        if (a->song_dirty) app_job(a, JOB_SAVE_SONG, 0);
        if (a->set_dirty) app_job(a, JOB_SAVE_GLOB, 0);
        app_toast(a, "AUTOSAVED");
    }
    jobs_run(a);
    if (a->view == V_CH && a->track == 1) {                 /* el dibujo de la onda que se ve */
        const bm_pad *pd = app_pad(a);
        if (a->page == PG_PADS) app_wave_view(a, bm_pad_ok(pd) ? pd->src : -1, pd->start, a->cut_zoom);
        else if (a->page == PG_TAKES) app_wave_view(a, bm_app_takes_sel(a), 0, 0);
        else if (a->page == PG_REC) app_wave_view(a, BM_TAKE_NEW, 0, 0);
    }
    app_leds(a);
    if (a->pf.blit) bm_screen_render(&a->scr, bm_app_draw, a, a->pf.blit, a->pf.user);
}
