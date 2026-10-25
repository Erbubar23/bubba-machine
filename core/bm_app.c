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
    if (a->pat.trk[t].engine == ENG_ACID) return BM_AC_PRESETS;
    if (a->pat.trk[t].engine == ENG_FM) return BM_FM_PATCHES;   /* bancos de usuario apagados hasta tener todas las máquinas */
    int n = bm_snap_count(a->pat.trk[t].engine);
    return n ? n : 1;
}

const char *bm_sound_name(const bm_app *a, int t)
{
    int s = a->pat.trk[t].source % bm_sound_count(a, t);
    if (bm_is_tr(&a->pat, t)) return bm_kit_name(s);
    if (a->pat.trk[t].engine == ENG_ACID) return bm_ac_presets[s].name;
    if (a->pat.trk[t].engine == ENG_FM) return bm_fm_patch_name(s);
    return bm_snap_name(a->pat.trk[t].engine, s);
}
uint16_t bm_knob_color(int i) { return KNOB_COLORS[i & 3]; }
void bm_app_cpu(bm_app *a, int pct) { a->cpu = (uint8_t)bm_clampi(pct, 0, 255); }

int app_machine_track(const bm_app *a, int engine)
{
    for (int t = BM_TR_TRACKS; t < BM_TRACKS; t++) if (a->pat.trk[t].engine == engine) return t;
    return -1;
}

/* BITTAL (un canal a la vez): sus dos tablas, armadas en RAM desde el espectro del snapshot
 * (core/bm_wt.h). Se rearman cuando cambia el sonido o la tabla elegida con ALGORITHM; mientras,
 * ese oscilador calla. */
static void app_tables(bm_app *a)
{
    const int t = app_machine_track(a, ENG_BITTAL);
    if (t < 0) return;
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
    o->beat = 0;
    o->knob = a->enc_cur;                  /* la perilla que gira (0 si no vino de una perilla) */
    snprintf(o->title, sizeof o->title, "%s", title);
    snprintf(o->val, sizeof o->val, "%s", val);
    snprintf(o->unit, sizeof o->unit, "%s", unit);
    o->until = a->now + UI_OVERLAY_MS;
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
    /* 2026-10-22 (pedido del usuario): al cortar, la secuencia se escribe siempre (la de antes se va:
     * sus pads ya no son los mismos) */
    bm_tr *tr = &a->pat.tr[1];
    memset(tr->st, 0, sizeof tr->st);
    for (int i = 0; i < BM_LANE_LOCKS; i++) tr->lk[i].lane = 0xFF;
    kit_layout(&a->pat, tk, src, m, n, true);
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
    case 4: {                                               /* off = su índice entre los del usuario (consola) */
        int r = bm_snaps_erase(f, (int)off);
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

static void put16(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static void put32le(uint8_t *b, uint32_t v) { put16(b, v); put16(b + 2, v >> 16); }

int bm_app_report(const bm_app *a, int what, int idx, uint8_t *o, int cap)
{
    if (what == 0) {
        if (cap < BM_REPORT_LEN) return -1;
        memset(o, 0, BM_REPORT_LEN);
        uint8_t used[BM_TAKE_SECTORS];
        bm_takes_used(a->take, used);
        bm_snaps_mark(used, (int)BM_TAKE_SECTORS);
        uint32_t fs = 0, ss = 0;
        for (int s = 0; s < (int)BM_TAKE_SECTORS; s++) fs += !used[s];
        int nt = 0;
        for (int i = 0; i < BM_TAKES; i++) nt += a->take[i].valid;
        for (int i = 0; i < bm_snaps_user_n(); i++) { int k = 0; bm_snaps_user_p(i, &k); ss += (uint32_t)k; }
        const uint8_t *lp = bm_app_lang_pack(a);
        char ln[12] = {0};
        uint32_t lsz = lp ? bm_lang_check(lp, BM_LANG_HI - BM_LANG_LO, ln) : 0;
        o[0] = 1;                                   /* versión del informe */
        o[1] = (uint8_t)app_playing(a);
        o[2] = a->cpu;
        o[3] = a->set.dual_core;
        o[4] = (uint8_t)nt; o[5] = (uint8_t)bm_snaps_user_n(); o[6] = BM_SNAPS_USER; o[7] = (uint8_t)(lp != NULL);
        put32le(o + 8, bm_takes_len(a->take));
        put32le(o + 12, BM_TAKES_MAX_LEN);
        put32le(o + 16, bm_takes_free(a->take));
        put32le(o + 20, bm_app_rec_room(a));
        put32le(o + 24, BM_TAKE_HI - BM_TAKE_LO);
        put32le(o + 28, fs * BM_SECTOR);
        put32le(o + 32, ss * BM_SECTOR);
        put16(o + 36, BM_SMP_RATE);
        put32le(o + 38, BM_SMP_REC_MAX);
        memcpy(o + 42, ln, 12);
        put16(o + 54, lsz);
        put16(o + 56, BM_LANG_HI - BM_LANG_LO);
        strncpy((char *)o + 58, BM_VERSION_STR, 14);
        return BM_REPORT_LEN;
    }
    if (what == 1) {                                /* toma: id, largo, BPM, compases, ganancia, pads que la usan */
        if (idx < 0 || idx >= BM_TAKES || cap < 12) return -1;
        const bm_take *t = &a->take[idx];
        memset(o, 0, 12);
        o[0] = t->valid; o[1] = (uint8_t)idx;
        put32le(o + 2, t->valid ? t->len : 0);
        put16(o + 6, t->bpm_x10);
        o[8] = t->bars; o[9] = t->gain;
        for (int i = 0; i < BM_LANES; i++) o[10] += t->valid && a->pat.pad[i].src == idx && a->pat.pad[i].len;
        o[11] = t->nmarks;
        return 12;
    }
    if (what == 2) {                                /* sonido del usuario: máquina, tamaño, sectores, nombre, autor, licencia, huella */
        int ns = 0;
        const uint8_t *p = bm_snaps_user_p(idx, &ns);
        if (!p || cap < 32) return -1;
        o[0] = p[4]; o[1] = p[6]; o[2] = (uint8_t)ns; o[3] = 0;
        o[4] = p[10]; o[5] = p[11];                /* tamaño (como en la cabecera) */
        o[6] = p[8]; o[7] = p[9];                  /* huella */
        memcpy(o + 8, p + 12, 12);
        memcpy(o + 20, p + 24, 12);
        return 32;
    }
    return -1;
}

void bm_app_take_delete(bm_app *a, int id)
{
    if (id >= 0 && id < BM_TAKES) a->ext_take_del = (uint8_t)(id + 1);
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
    case JOB_SAVE_SONG: {
        uint8_t buf[2 + 2 * BM_SONG_MAX + sizeof a->song.scene];
        buf[0] = BM_SONG_VER; buf[1] = a->song.n;
        memcpy(buf + 2, a->song.pat, BM_SONG_MAX);
        memcpy(buf + 2 + BM_SONG_MAX, a->song.rep, BM_SONG_MAX);
        memcpy(buf + 2 + 2 * BM_SONG_MAX, a->song.scene, sizeof a->song.scene);
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
        } else {                                              /* si no, queda entera en el pad elegido (EDIT > TRIM la corta) */
            app_pad_take(a, a->sel_lane[1] & 15, id);
            snprintf(b, sizeof b, TF("TAKE %02d ON PAD %d"), id + 1, (a->sel_lane[1] & 15) + 1);
        }
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
        if (app_playing(a)) {                                   /* sonando: espera a que pares (la flash pausa el audio) */
            int n = 0;
            for (int i = 0; i < a->njob; i++) {
                if (a->job[i] == JOB_LOAD) { exec_job(a, JOB_LOAD, a->job_arg[i]); continue; }
                a->job[n] = a->job[i]; a->job_arg[n] = a->job_arg[i]; n++;
            }
            a->njob = n;
            return;
        }
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

/* ------------------------------------------------------------ patrones de fábrica (primer arranque) */
static void ev_set(bm_pattern *p, int t, int s, int k, int note, int vel, int len, int prob)
{
    bm_event *e = bm_ev(p, t, s, k);
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = (uint8_t)prob;
}

/* Patrones de fábrica (2026-10-24, pedido del usuario): 1 DA FUNK, 2 BLADE, 3 ENJOY THE SILENCE.
 * Los canales como vienen (BR-8, Slicer vacío, BB-303, B-MINI, BITTAL, BUNO-6). Tempo y tono de cada
 * tema según Wikipedia, Hooktheory y bases de DJ; las líneas:
 *  - DA FUNK (Daft Punk, 111 BPM, Sol menor): no hay hoja publicada. La 303 usa las notas que da Attack
 *    Magazine (G A# C D# F, el primer 1/16 en silencio); el bajo es un Sol grave con cada bombo y el
 *    lead va en cuartas (Reverb Machine). El orden de las notas es nuestro, a oído.
 *  - BLADE (New Order, "Confusion", Pump Panel Reconstruction, 136 BPM, Sol menor): la 303 de la hoja
 *    de djjondent.blogspot.com (G y A#), con los acentos que propone un usuario en forums.adafruit.com.
 *  - ENJOY THE SILENCE (Depeche Mode, 113 BPM): la batería de drum-patterns.com y el estribillo de
 *    Hooktheory (Fm Ab Cm Eb, un compás cada uno; voces y bajo nuestros).
 * La escena B de todos los patrones es todo muteado (scenes_default). */
static void tr_on(bm_tr *d, int lane, const char *pat, uint8_t extra)
{
    for (int s = 0; pat[s] && s < BM_STEPS; s++)
        if (pat[s] == 'x' || pat[s] == 'X') d->st[lane][s] = (uint8_t)(ST_ON | (pat[s] == 'X' ? ST_ACC : 0) | extra);
}

typedef struct { int8_t s, n; uint8_t acc, slide; } line303;   /* acento = vel 127, slide = LIGA hasta la siguiente */
static void put303(bm_pattern *p, int t, const line303 *L, int n)
{
    for (int i = 0; i < n; i++)
        ev_set(p, t, L[i].s, 0, L[i].n, L[i].acc ? BM_VEL_ACC : BM_VEL_NORMAL, 1 | (L[i].slide ? BM_LEN_TIE : 0), 100);
}

static void od_on(bm_pattern *p, int t, int gain)
{
    p->prm[t][AC_FX] = bm_acid_fx_set(p->prm[t][AC_FX], ACF_OD, 1);
    p->prm[t][AC_OD_G] = (uint8_t)gain;
}

static void snd(bm_pattern *p, int t, int preset) { p->trk[t].source = (uint8_t)preset; bm_preset_apply(p, t, preset); }

static void scene4(bm_song *song, int pat, uint8_t a, uint8_t c, uint8_t d)   /* A, C y D (la B es la de siempre) */
{
    bm_scene *s = song->scene[pat];
    s[0].flags = s[2].flags = s[3].flags = SCN_STORED;
    s[0].trk_mute = a; s[2].trk_mute = c; s[3].trk_mute = d;
}

static void factory_da_funk(bm_pattern *p, bm_song *song)
{
    p->meta.bpm_x10 = 1110; p->meta.swing = 54; p->meta.root = 7; p->meta.scale = 1;
    p->meta.dly_time = 2; p->meta.dly_fb = 40; p->meta.dly_mix = 35; p->meta.rv_mix = 45;
    bm_tr *d = &p->tr[0];                                                /* T1: 909, cuatro en el piso */
    bm_kit_apply(d, 1); p->trk[0].source = 1;
    tr_on(d, 0, "x...x...x...x...", 0);
    tr_on(d, 5, "....x.......x...", 0);
    tr_on(d, 2, "xxXxxxXxxxXxxxXx", 0); d->lane[2].level = 55;
    /* T3: BB-303 ROBOT FUNK con OVERDRIVE (en el tema entra a la mitad: escena D) */
    snd(p, 2, 8); od_on(p, 2, 100);
    static const line303 F[] = {
        {1, 43, 1, 0}, {2, 55, 0, 1}, {3, 43, 0, 0}, {4, 46, 0, 0}, {5, 43, 1, 0}, {6, 53, 0, 0}, {7, 43, 0, 0},
        {8, 48, 0, 1}, {9, 51, 0, 0}, {10, 43, 1, 0}, {11, 41, 0, 0}, {12, 43, 0, 0}, {13, 58, 1, 1}, {14, 55, 0, 0}, {15, 53, 0, 0}};
    put303(p, 2, F, (int)(sizeof F / sizeof F[0]));
    p->trk[2].length = 16; p->trk[2].send = 30;
    /* T4: B-MINI WIZARD BASS, el Sol grave con cada bombo */
    snd(p, 3, 0); p->trk[3].length = 16;
    for (int s = 0; s < 16; s += 4) ev_set(p, 3, s, 0, 31, 100, 1, 100);
    /* T5: BITTAL CHORD STAB, la "insistencia": un Sol agudo en 1/16 con delay */
    snd(p, 4, 8); p->trk[4].length = 16; p->trk[4].send = 55;
    for (int s = 0; s < 16; s++) ev_set(p, 4, s, 0, 79, s % 4 == 2 ? 95 : 70, 1, 100);
    /* T6: BUNO-6 POP RIFF con DISTORTION, el lead en cuartas (dos compases) */
    snd(p, 5, 10); p->prm[5][PX_FX] |= 1u; p->prm[5][PX_DS_G] = 90;
    p->trk[5].length = 32; p->trk[5].send = 20; p->trk[5].rsend = 30;
    static const int8_t M[][3] = {
        {0, 62, 2}, {2, 62, 1}, {3, 60, 1}, {4, 62, 2}, {6, 65, 2}, {8, 62, 2}, {10, 60, 1}, {11, 58, 1}, {12, 60, 2}, {14, 58, 1}, {15, 55, 1},
        {16, 55, 2}, {18, 58, 1}, {19, 60, 1}, {20, 62, 2}, {22, 70, 2}, {24, 67, 2}, {26, 65, 2}, {28, 62, 2}, {30, 60, 1}, {31, 58, 1}};
    for (unsigned i = 0; i < sizeof M / sizeof M[0]; i++) {
        ev_set(p, 5, M[i][0], 0, M[i][1], 95, M[i][2], 100);
        ev_set(p, 5, M[i][0], 1, M[i][1] + 5, 85, M[i][2], 100);
    }
    p->trk[0].level = 84; p->trk[2].level = 60; p->trk[3].level = 70; p->trk[4].level = 28; p->trk[5].level = 45;
    scene4(song, 0, 0x00, 0x04, 0x30);          /* A todo · C sin la 303 (el principio) · D la 303 con batería y bajo */
}

static void factory_blade(bm_pattern *p, bm_song *song)
{
    p->meta.bpm_x10 = 1360; p->meta.swing = 50; p->meta.root = 7; p->meta.scale = 1;
    p->meta.dly_time = 2; p->meta.dly_fb = 35; p->meta.dly_mix = 30; p->meta.rv_mix = 50;
    bm_tr *d = &p->tr[0];
    bm_kit_apply(d, 1); p->trk[0].source = 1;
    tr_on(d, 0, "x...x...x...x...", 0);
    tr_on(d, 3, "..x...x...x...x.", 0); d->lane[3].level = 65;
    tr_on(d, 2, "xx.xxx.xxx.xxx.x", 0); d->lane[2].level = 50;
    tr_on(d, 5, "....x.......x...", 0);
    /* T3: BB-303 PHILLY 95, cuadrada, corte alto y OVERDRIVE fuerte; Sol (G3) y La# (A#2, A#3) */
    snd(p, 2, 7); od_on(p, 2, 115);
    p->prm[2][AC_WAVE] = 1; p->trk[2].macro[0] = 95;
    static const line303 B[] = {
        {0, 55, 1, 0}, {3, 55, 1, 0}, {5, 46, 1, 0}, {6, 46, 1, 0}, {8, 46, 1, 0}, {10, 58, 1, 0}, {12, 58, 1, 0}, {13, 58, 1, 0}, {15, 58, 0, 0}};
    put303(p, 2, B, (int)(sizeof B / sizeof B[0]));
    bm_ev(p, 2, 0, 0)->len = 3; bm_ev(p, 2, 10, 0)->len = 2;          /* las notas sostenidas de la hoja */
    p->trk[2].length = 16; p->trk[2].send = 25;
    /* T5: BITTAL SIREN, una nota larga cada cuatro compases, baja */
    snd(p, 4, 2); p->trk[4].length = 64; p->trk[4].rsend = 60;
    ev_set(p, 4, 0, 0, 67, 80, 32, 100);
    /* T6: BUNO-6 DARK RAVE, el acorde de Sol menor cada dos compases */
    snd(p, 5, 14); p->trk[5].length = 32; p->trk[5].rsend = 45;
    for (int k = 0; k < 3; k++) { static const int C[3] = {55, 58, 62}; ev_set(p, 5, 0, k, C[k], 100, 2, 100); ev_set(p, 5, 22, k, C[k], 90, 1, 100); }
    p->trk[0].level = 88; p->trk[2].level = 62; p->trk[4].level = 22; p->trk[5].level = 34;
    scene4(song, 1, 0x00, 0x38, 0x01);          /* A todo · C batería y 303 · D sin batería */
}

static void factory_enjoy(bm_pattern *p, bm_song *song)
{
    p->meta.bpm_x10 = 1130; p->meta.swing = 50; p->meta.root = 5; p->meta.scale = 1;
    p->meta.dly_time = 1; p->meta.dly_fb = 30; p->meta.dly_mix = 25; p->meta.rv_mix = 55;
    bm_tr *d = &p->tr[0];                                                /* T1: 909, la rejilla de dos compases */
    bm_kit_apply(d, 1); p->trk[0].source = 1;
    tr_on(d, 0, "X...X...x...X...", 0);
    tr_on(d, 2, "X...X..X.x..X...", 0);
    tr_on(d, 3, "..x...x...x...x.", 0); d->lane[3].level = 50;
    tr_on(d, 4, "....X..X....X.......X.......X...", 0); d->lane[4].len = 32;
    tr_on(d, 5, "....X..X....X.......X.......X...", 0); d->lane[5].len = 32; d->lane[5].level = 60;
    /* el estribillo: Fm Ab Cm Eb, un compás cada uno */
    static const int8_t R[4] = {41, 44, 36, 39}, C[4][3] = {{53, 56, 60}, {56, 60, 63}, {55, 60, 63}, {55, 58, 63}};
    snd(p, 3, 14); p->trk[3].length = 64;                               /* T4: B-MINI HIGHWAY BASS en corcheas */
    for (int b = 0; b < 4; b++) for (int s = 0; s < 16; s += 2) ev_set(p, 3, b * 16 + s, 0, R[b], s % 4 ? 85 : 100, 1, 100);
    snd(p, 5, 3); p->trk[5].length = 64; p->trk[5].rsend = 55;          /* T6: BUNO-6 PWM STRINGS */
    for (int b = 0; b < 4; b++) for (int k = 0; k < 3; k++) ev_set(p, 5, b * 16, k, C[b][k], 80, 16, 100);
    p->trk[0].level = 84; p->trk[3].level = 72; p->trk[5].level = 36;
    scene4(song, 2, 0x00, 0x20, 0x01);          /* A todo · C batería y bajo · D sin batería */
}

#define BM_FACTORY_PATS 3
static void factory_pattern(bm_pattern *p, bm_song *song, int i)
{
    bm_pattern_init(p);
    p->meta.master = 4; p->meta.fill_every = 4;
    if (i == 0) factory_da_funk(p, song);
    else if (i == 1) factory_blade(p, song);
    else factory_enjoy(p, song);
}

/* La escena B de fábrica en todos los patrones: los seis canales muteados, la batería también (pedido del
 * usuario, 2026-10-24). Queda hasta que se guarde otra encima. */
static void scenes_default(bm_song *song)
{
    memset(song->scene, 0, sizeof song->scene);
    for (int p = 0; p < BM_PATTERNS; p++) { song->scene[p][1].flags = SCN_STORED; song->scene[p][1].trk_mute = (uint8_t)((1u << BM_TRACKS) - 1u); }
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
    s->ver = BM_FMT; s->factory = BM_FACTORY_VER; s->sync_ppqn = 2; s->drive = 25; s->led_dim = 1; s->autosave = 1; s->enter = 2;
    /* dual_core apagado: en 0.6.1 venía encendido y en el aparato iba más lento que un núcleo (123 %
     * contra ~105 %). Desde 0.6.2 usa el núcleo 1 por interrupción (hal/fm1/fm1_cpu1.c), a prueba. */
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
    a->pend_obj = -1; a->held_step = -1; a->held_white = -1;
    a->slice_sens = 64; a->pad_key = -1;
    a->solo_key = -1; a->solo_track = -1;
    a->mach_trk = BM_TR_TRACKS;
    static const uint8_t ln[BM_TRACKS] = {0, 0, 45, 57, 60, 64};
    memcpy(a->last_note, ln, sizeof ln);
    for (int k = 0; k < BM_KEYS; k++) a->key_rec_step[k] = -1;
    settings_defaults(&a->set);
    a->battery = 100;

    scenes_default(&a->song);
    int fresh = 1, mounted = 0;
    if (pf->flash && bm_store_mount(&a->store, pf->flash) == 0) {
        mounted = 1;
        bm_settings s;
        int has_glob = bm_store_read_glob(&a->store, (uint8_t *)&s, sizeof s) == (int)sizeof s;
        int ok = has_glob && s.ver == BM_FMT, old = has_glob && !ok, oldpat = 0;
        for (int i = 0; i < BM_PATTERNS; i++) {    /* patrones de otro formato: hasta la v1 no se convierten */
            int len = 0;
            const uint8_t *p = store_ptr(a, i, &len);
            if (p && len > (int)offsetof(bm_meta, fmt) && p[offsetof(bm_meta, fmt)] != BM_PAT_FMT && p[offsetof(bm_meta, fmt)] != 9) oldpat = 1;   /* el 9 se lee */
        }
        if (old) {                                 /* datos de 0.2: se empieza de cero */
            bm_store_format(&a->store, pf->flash);
            bm_takes_format(pf->flash);
        } else if (oldpat) {                       /* patrones de 0.3: se borran (canción y ajustes se rehacen); las tomas se quedan */
            bm_store_format(&a->store, pf->flash);
            ok = 0;
        }
        if (ok) a->set = s;
        if (ok && a->set.factory != BM_FACTORY_VER) {   /* patrones de fábrica nuevos: los guardados se borran (tomas, sonidos y ajustes se quedan) */
            bm_store_format(&a->store, pf->flash);
            a->set.factory = BM_FACTORY_VER;
        }
        uint8_t b[2 + 2 * BM_SONG_MAX + sizeof a->song.scene];   /* otra versión (0.8: escenas con patrón): se ignora */
        if (ok && bm_store_read(&a->store, BM_OBJ_SONG, b, sizeof b) == (int)sizeof b && b[0] == BM_SONG_VER) {
            a->song.n = b[1] > BM_SONG_MAX ? BM_SONG_MAX : b[1];
            memcpy(a->song.pat, b + 2, BM_SONG_MAX);
            memcpy(a->song.rep, b + 2 + BM_SONG_MAX, BM_SONG_MAX);
            memcpy(a->song.scene, b + 2 + 2 * BM_SONG_MAX, sizeof a->song.scene);
            for (int i = 0; i < a->song.n; i++) if (bm_song_pat(a->song.pat[i]) >= BM_PATTERNS) a->song.pat[i] = 0;
        }
        for (int i = 0; i < BM_PATTERNS; i++) if (bm_store_has(&a->store, (uint8_t)i)) fresh = 0;
        int len = 0;
        const uint8_t *src = store_ptr(a, a->set.last_pat % BM_PATTERNS, &len);
        bm_snaps_scan(pf->flash, pf->flash_map, pf->user);     /* los sonidos del usuario, antes que todo lo demás */
        if (src && len > 0 && bm_pattern_unpack(&a->pat, src, len) < 0) bm_pattern_init(&a->pat);
        bm_takes_scan(pf->flash, pf->flash_map, pf->user, a->take);
    }
    bm_lang_use(a->set.lang ? bm_app_lang_pack(a) : NULL);
    if (fresh) {                                   /* los de fábrica: el 1 queda cargado (se guarda solo), el 2 y el 3 se escriben ya */
        if (mounted) for (int i = 1; i < BM_FACTORY_PATS; i++) { factory_pattern(&a->pat, &a->song, i); write_pattern(a, i); }
        factory_pattern(&a->pat, &a->song, 0);
        a->set.last_pat = 0;
        a->dirty = 1; a->song_dirty = 1; a->set_dirty = 1; a->t_edit = now;
    }
    a->set.last_pat %= BM_PATTERNS;
    a->ctx.cur_pat = a->set.last_pat;
    a->ctx.st.pattern = a->set.last_pat;
    bm_engines_sync(&a->ctx);
    bm_seq_set_bpm(&a->ctx, a->pat.meta.bpm_x10);
    apply_settings(a);
    app_send_takes(a);
    a->view = V_BOOT;
    a->boot_t = now;
    a->scr.force = 1;
    a->scr.budget = BM_SCR_BUDGET;
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
/* HOME es la consola: salir de ella apaga PLAY/REC y cierra las capas. */
void app_set_view(bm_app *a, int v)
{
    if (v != V_HOME && a->sloop) app_sloop(a, 0);
    a->layer = L_NONE; a->layer_latch = 0;
    a->view = (uint8_t)v;
    a->ov.on = 0;
    a->clear_armed = 0;
}

/* La rejilla por pasos de la batería y del Slicer (V_CH). */
void app_enter(bm_app *a, int t, int page)
{
    (void)page;
    if (!bm_is_grid(&a->pat, t)) return;
    a->track = (uint8_t)t;
    a->held_step = -1; a->held_white = -1;
    a->mach_arm = 0;
    int len = bm_trk_len(&a->pat, t, a->sel_lane[t] & 15);
    if (a->step_page * BM_PAGE_STEPS >= len) a->step_page = 0;
    app_set_view(a, V_CH);
}

/* ------------------------------------------------------------ datos de un canal */
void *app_track_data(bm_app *a, int t) { return t < BM_TR_TRACKS ? (void *)&a->pat.tr[t] : (void *)&a->pat.nt[t - BM_TR_TRACKS]; }
size_t app_track_size(int t) { return t < BM_TR_TRACKS ? sizeof(bm_tr) : sizeof(bm_notes); }

void app_init_sound(bm_app *a, int t)
{
    if (app_kind(a, t) == KIND_NOTE) app_push(a, CMD_PRESET, (uint8_t)t, 0xFF, 0);   /* los ajustes de fábrica de la máquina */
    else {
        bm_track d; bm_track_defaults(&d, t);
        static const uint8_t F[] = {0, 1, 2, 3, TRK_OFF(attack), TRK_OFF(sustain), TRK_OFF(release), TRK_OFF(env_amt), TRK_OFF(glide), TRK_OFF(transpose)};
        for (unsigned i = 0; i < sizeof F; i++) app_push(a, CMD_SET_TRACK, (uint8_t)t, F[i], ((const uint8_t *)&d)[F[i]]);
    }
    app_edited(a);
    app_toast(a, "INIT SOUND");
}

/* T3-T6: otra máquina. Las notas quedan; el sonido pasa a su primer preset. 0 = se cambió. */
int app_set_machine(bm_app *a, int t, int eng)
{
    if (!bm_engine_ok(t, eng) || t < BM_TR_TRACKS || a->pat.trk[t].engine == eng) return -1;
    if (bm_machine_conflict(&a->pat, t, eng) >= 0) return -1;
    app_push(a, CMD_ENGINE, (uint8_t)t, (uint8_t)eng, 0);
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, "T%d = %s", app_pos(t) + 1, bm_engine_names[eng]);
    app_toast(a, b);
    return 0;
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
    if (st->smp_unsaved && !st->smp_rec && !app_playing(a) && !a->njob && a->pf.flash) {   /* toma nueva: al parar */
        app_job(a, JOB_SAVE_TAKE, 0);
        app_toast(a, "SAVING THE TAKE");
    }
    if (a->take_dirty && !app_playing(a) && !a->njob && a->pf.flash && now - a->t_edit > 1500)
        for (int i = 0; i < BM_TAKES; i++) if (a->take_dirty & (1u << i)) app_job(a, JOB_TAKE_META, i);
    if (a->set.autosave && !app_playing(a) && !a->njob && (a->dirty || a->pend_obj >= 0 || a->song_dirty || a->set_dirty)
        && now - a->t_edit > UI_AUTOSAVE_MS && a->pf.flash) {
        app_save_jobs(a, a->dirty);
        if (a->song_dirty) app_job(a, JOB_SAVE_SONG, 0);
        if (a->set_dirty) app_job(a, JOB_SAVE_GLOB, 0);
        a->autosave_until = now + UI_TOAST_MS;
    }
    if (a->ext_take_del && !app_playing(a)) {      /* pedido de la consola: como N11 en TOMAS */
        int id = a->ext_take_del - 1;
        a->ext_take_del = 0;
        if (id < BM_TAKES && a->take[id].valid) app_job(a, JOB_TAKE_ERASE, id);
    }
    jobs_run(a);
    app_leds(a);
    /* 25 cuadros por segundo como mucho, y un redibujo entero repartido en varios ticks (bm_gfx.h) */
    if (a->pf.blit && bm_screen_due(&a->scr, now)) bm_screen_render(&a->scr, bm_app_draw, a, a->pf.blit, a->pf.user);
}
