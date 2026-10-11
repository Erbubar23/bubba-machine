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

static const char *const TRACK_NAMES[3] = {"BATERIA", "SAMPLER", "BAJO"};
static const uint16_t TRACK_COLORS[BM_TRACKS] = {C_ORANGE, C_YELLOW, C_GREEN, C_BLUE, C_VIOLET, C_PINK};
static const uint16_t KNOB_COLORS[4] = {C_BLUE, C_GREEN, C_YELLOW, C_ORANGE};
const uint8_t bm_master_steps_tab[6] = {1, 2, 3, 4, 8, 0};
const uint8_t bm_roll_ticks[5] = {48, 24, 12, 32, 16};
const char *const bm_roll_names[5] = {"1/8", "1/16", "1/32", "1/8T", "1/16T"};
static const int16_t ENTER_MS[5] = {0, 250, 400, 800, -1};

const char *bm_track_name(const bm_app *a, int t)
{
    t %= BM_TRACKS;
    return t < BM_FREE0 ? TRACK_NAMES[t] : bm_engine_names[a->pat.trk[t].engine % ENG_COUNT];
}
uint16_t bm_track_color(int t) { return TRACK_COLORS[t % BM_TRACKS]; }
const char *bm_kit_name(int kit) { return kit % BM_KITS_ALL == BM_KIT_SYN ? "SINTE" : bm_rom_kit_names[kit % BM_KITS]; }

int bm_sound_count(const bm_app *a, int t)
{
    if (bm_is_tr(&a->pat, t)) return BM_KITS_ALL;
    if (a->pat.trk[t].engine == ENG_ACID) return 2;
    if (a->pat.trk[t].engine == ENG_VA) return BM_VA_TIMBRES;
    return BM_WAVES;
}

const char *bm_sound_name(const bm_app *a, int t)
{
    int s = a->pat.trk[t].source % bm_sound_count(a, t);
    if (bm_is_tr(&a->pat, t)) return bm_kit_name(s);
    if (a->pat.trk[t].engine == ENG_ACID) return bm_acid_names[s];
    if (a->pat.trk[t].engine == ENG_VA) return bm_va_names[s];
    return bm_rom_wave_names[s];
}
uint16_t bm_knob_color(int i) { return KNOB_COLORS[i & 3]; }
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
    if (bm_is_tr(&a->pat, t)) return bm_rom_kits[a->pat.trk[t].source % BM_KITS][lane & 15].name;
    k ^= 1;
    if (t == 1) snprintf(buf[k], sizeof buf[k], lane ? "SLICE %d" : "GEN", lane);
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
    for (int t = BM_FREE0; t < BM_TRACKS; t++) if (app_kind(a, t) == KIND_NOTE) return t;
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
    a->src_dirty = 1;
    return 0;
}

/* ------------------------------------------------------------ sampler: fuentes y dibujo */
/* Energía por columna en tres bandas: graves (< ~150 Hz), medios y agudos (> ~2 kHz), separadas con
 * dos filtros de un polo. Un master actual es un bloque si se dibujan los picos; en bandas se ve
 * dónde pega el bombo y dónde los hats. Dos pasadas: la primera busca el piso y el máximo de cada
 * banda; la segunda dibuja lo que sobresale del piso y estira cada banda a su máximo. */
static void compute_wave(uint8_t wave[3][232], const int8_t *d, uint32_t len)
{
    memset(wave, 0, 3 * 232);
    if (!d || !len) return;
    uint32_t mx[3] = {0, 0, 0}, mn[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    for (int pass = 0; pass < 2; pass++) {
        int32_t lo = 0, lp = 0;
        uint32_t i = 0;
        for (int x = 0; x < 232; x++) {
            uint32_t s1 = len * (uint32_t)(x + 1) / 232u, sum[3] = {0, 0, 0}, n = 0;
            for (; i < s1; i++, n++) {
                int32_t v = (int32_t)d[i] * 256;
                lo += ((v - lo) * 1875) >> 15;        /* ≈ 150 Hz a 16 kHz */
                lp += ((v - lp) * 17833) >> 15;       /* ≈ 2 kHz */
                int32_t b0 = lo, b1 = lp - lo, b2 = v - lp;
                sum[0] += (uint32_t)(b0 < 0 ? -b0 : b0);
                sum[1] += (uint32_t)(b1 < 0 ? -b1 : b1);
                sum[2] += (uint32_t)(b2 < 0 ? -b2 : b2);
            }
            for (int k = 0; k < 3; k++) {
                uint32_t m = n ? sum[k] / n : 0;
                if (pass == 0) { if (n && m > mx[k]) mx[k] = m; if (n && m < mn[k]) mn[k] = m; }
                else if (mx[k] > mn[k]) {
                    uint32_t r = (m > mn[k] ? m - mn[k] : 0) * 255u / (mx[k] - mn[k]);
                    wave[k][x] = (uint8_t)(r * r / 255u);    /* curva: los golpes resaltan sobre el relleno */
                }
            }
        }
    }
}

static void slice_span(const bm_smp_src *m, int i, uint32_t *s, uint32_t *e)
{
    if (!m->nmarks) { *s = m->len * (uint32_t)i / BM_SLICES; *e = m->len * (uint32_t)(i + 1) / BM_SLICES; }
    else { *s = m->marks[i]; *e = i + 1 < m->nmarks ? m->marks[i + 1] : m->len; }
}

/* Bombo y caja: el slice que más golpea en graves y el que más golpea en medios y agudos,
 * mirando solo el primer cuarto de cada slice (el ataque). ANCLAR los usa. */
static void find_ks(bm_smp_src *m)
{
    m->kslice = m->sslice = 0xFF;
    int n = m->nmarks ? m->nmarks : BM_SLICES;
    if (!m->data || !m->len || n < 2) return;
    uint32_t ks[BM_SLICES] = {0}, ss[BM_SLICES] = {0};
    for (int i = 0; i < n; i++) {
        uint32_t s, e;
        slice_span(m, i, &s, &e);
        uint32_t q = s + (e - s) / 4;
        int32_t lo = 0, lp = 0;
        uint64_t sk = 0, sn = 0;
        for (uint32_t j = s; j < q; j++) {
            int32_t v = (int32_t)m->data[j] * 256;
            lo += ((v - lo) * 1875) >> 15;
            lp += ((v - lp) * 17833) >> 15;
            int32_t b0 = lo, b12 = v - lo;
            sk += (uint32_t)(b0 < 0 ? -b0 : b0);
            sn += (uint32_t)(b12 < 0 ? -b12 : b12);
        }
        uint32_t w = q > s ? q - s : 1;
        ks[i] = (uint32_t)(sk / w); ss[i] = (uint32_t)(sn / w);
    }
    int k = 0;
    for (int i = 1; i < n; i++) if (ks[i] > ks[k]) k = i;
    int sbest = -1;
    for (int i = 0; i < n; i++) if (i != k && (sbest < 0 || ss[i] > ss[sbest])) sbest = i;
    m->kslice = (uint8_t)k; m->sslice = (uint8_t)sbest;
}

static void slice_energy(bm_app *a, const bm_smp_src *m)
{
    memset(a->slice_e, 0, sizeof a->slice_e);
    int n = m->nmarks ? m->nmarks : BM_SLICES;
    if (!m->len) return;
    for (int i = 0; i < n; i++) {
        uint32_t s, e;
        slice_span(m, i, &s, &e);
        int c0 = (int)((uint64_t)s * 232u / m->len), c1 = (int)((uint64_t)e * 232u / m->len);
        if (c1 <= c0) c1 = c0 + 1;
        for (int b = 0; b < 5; b++) {
            int x0 = c0 + (c1 - c0) * b / 5, x1 = c0 + (c1 - c0) * (b + 1) / 5;
            if (x1 <= x0) x1 = x0 + 1;
            int mx = 0;
            for (int x = x0; x < x1 && x < 232; x++) {
                int v = (a->wave[0][x] + a->wave[1][x] + a->wave[2][x]) / 3;
                if (v > mx) mx = v;
            }
            a->slice_e[i][b] = (uint8_t)mx;
        }
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
    if (src == BM_SRC_NONE) snprintf(buf, (size_t)cap, "NINGUNO");
    else if (src & BM_SRC_ROM) snprintf(buf, (size_t)cap, "%s", (src & 0x7F) < bm_rom_nloops ? bm_rom_loops[src & 0x7F].name : "FAB ?");
    else snprintf(buf, (size_t)cap, "TOMA %02d%s", src + 1, bm_app_src_ok(a, src) ? "" : " VACIA");
}

int bm_app_sources(const bm_app *a, uint8_t out[BM_TAKES + 8])
{
    int n = 0;
    for (int i = 0; i < bm_rom_nloops && i < 8; i++) out[n++] = (uint8_t)(BM_SRC_ROM | i);
    for (int i = 0; i < BM_TAKES; i++) if (a->take[i].valid) out[n++] = (uint8_t)i;
    return n;
}

float bm_app_brk_semis(const bm_app *a)
{
    const bm_smp_src *m = &a->ctx.smp[0];
    float s = (float)((int)a->pat.trk[1].pitch - 24);
    if (m->bpm_x10 && m->len) s += 12.0f * log2f((float)a->pat.meta.bpm_x10 / (float)m->bpm_x10);
    return s;
}

void app_send_sources(bm_app *a)
{
    const bm_track *tk = &a->pat.trk[1];
    uint8_t src[2] = {tk->source, tk->loop_b};
    for (int i = 0; i < 2; i++) {
        bm_smp_src *m = &a->smp_send[i];
        bm_app_src_info(a, src[i], m);
        find_ks(m);
        app_push_ptr(a, CMD_SMP_SRC, (uint8_t)i, m);
        a->sent_src[i] = src[i];
    }
    if (!a->ctx.st.smp_unsaved && !a->ctx.st.smp_rec) {
        compute_wave(a->wave, a->smp_send[0].data, a->smp_send[0].len);
        slice_energy(a, &a->smp_send[0]);
    }
    a->src_dirty = 0;
}

void app_preview_take(bm_app *a, uint8_t src)
{
    bm_smp_src *m = &a->smp_send[2];
    bm_app_src_info(a, src, m);
    app_push_ptr(a, CMD_SMP_SRC, 2, m);
}

void app_cut_wave(bm_app *a)
{
    const bm_take *t = &a->take[a->cut_take % BM_TAKES];
    compute_wave(a->wave_cut, t->valid ? t->data : NULL, t->valid ? t->len : 0);
}

void bm_app_takes_reload(bm_app *a)
{
    if (a->pf.flash) bm_takes_scan(a->pf.flash, a->pf.flash_map, a->pf.user, a->take);
    app_send_sources(a);
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
    if (n < 0) { app_toast(a, "PATRON LLENO: NO CABE"); return 0; }
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
            app_toast(a, "NO CABE EN TOMAS: DESCARTADA");
            return 0;
        }
        app_set_track(a, 1, TRK_OFF(source), id);
        a->pat.trk[1].source = (uint8_t)id;
        bm_app_src_info(a, (uint8_t)id, &a->smp_send[0]);
        find_ks(&a->smp_send[0]);
        app_push_ptr(a, CMD_SMP_SAVED, 0, &a->smp_send[0]);
        a->sent_src[0] = (uint8_t)id;
        compute_wave(a->wave, a->smp_send[0].data, a->smp_send[0].len);
        slice_energy(a, &a->smp_send[0]);
        a->cut_take = (uint8_t)id;
        snprintf(b, sizeof b, "TOMA %02d GUARDADA: LOOP A", id + 1);
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
            a->src_dirty = 1;
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
        if (app_playing(a)) { app_toast(a, "PARA (STOP) PARA GUARDAR"); a->njob = 0; return; }
        if (!a->pf.flash) { a->njob = 0; return; }
        if (a->cmdq.r != a->cmdq.w) return;
        if (!a->job_fading) { app_push(a, CMD_FADE, 1, 0, 0); a->job_fading = 1; return; }
        if (!a->ctx.st.faded) return;
    }
    int err = 0;
    for (int i = 0; i < a->njob; i++) err |= exec_job(a, a->job[i], a->job_arg[i]);
    a->njob = 0;
    if (a->job_fading) { app_push(a, CMD_FADE, 0, 0, 0); a->job_fading = 0; }
    if (err) app_toast(a, "ERROR DE FLASH");
}

/* ------------------------------------------------------------ demo de primer arranque */
static void ev_set(bm_pattern *p, int t, int s, int k, int note, int vel, int len, int prob)
{
    bm_event *e = bm_ev(p, t, s, k);
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = (uint8_t)prob;
}

static void demo_pattern(bm_pattern *p, bm_song *song)
{
    bm_pattern_init(p);
    p->meta.bpm_x10 = 1120; p->meta.swing = 56; p->meta.root = 9; p->meta.scale = 1;
    p->meta.master = 4; p->meta.fill_every = 4; p->meta.chord_set = 1; p->meta.strum = 12;
    bm_tr *d = &p->tr[0];
    for (int s = 0; s < 16; s += 4) d->st[0][s] = ST_ON;              /* bombo */
    d->st[0][14] = ST_ON | (1 << ST_COND_SH);                           /* bombo extra solo en el fill */
    d->st[4][4] = ST_ON; d->st[4][12] = ST_ON | ST_ACC;                 /* caja */
    for (int s = 2; s < 16; s += 4) d->st[2][s] = ST_ON;                /* hat cerrado */
    d->st[2][14] = ST_ON | (1 << ST_RAT_SH);                            /* con repetición x2 */
    d->st[3][14] = ST_ON | (2 << ST_COND_SH);                           /* hat abierto, no en el fill */
    d->st[5][12] = ST_ON | (1 << ST_COND_SH);                           /* clap solo en el fill */
    d->st[6][7] = ST_ON | ST_FLAM;                                      /* rim con flam */
    d->lane[12].len = 12;                                               /* shaker de 12 contra 16: polímetro */
    for (int s = 0; s < 12; s += 3) d->st[12][s] = ST_ON;
    d->lane[12].level = 70;
    bm_tr_lock_set(d, 12, 0, 1, 84);                                    /* P-Lock: bombo del paso 13 más agudo */
    /* sampler: el generador en corcheas, con el slice 1 fijo en el 1 y el 5 con acento en el 13 */
    bm_tr *b = &p->tr[1];
    for (int s = 0; s < 16; s += 2) b->st[BRK_GEN][s] = ST_ON;
    b->st[1][0] = ST_ON; b->st[5][12] = ST_ON | ST_ACC;
    p->trk[1].level = 40;
    p->trk[1].source = bm_rom_nloops ? BM_SRC_ROM : BM_SRC_NONE;
    p->trk[1].loop_b = bm_rom_nloops > 1 ? (uint8_t)(BM_SRC_ROM | 1) : BM_SRC_NONE;
    static const int prog[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}};   /* Am F C G */
    static const int bass[4] = {45, 41, 36, 43};
    for (int t = 2; t < BM_TRACKS; t++) p->trk[t].length = 64;
    p->trk[4].engine = ENG_MONO;                                        /* T5: una línea mono */
    for (int pg = 0; pg < 4; pg++) {
        int o = pg * 16, bn = bass[pg];
        ev_set(p, 2, o + 0, 0, bn, 110, 2, 100);
        ev_set(p, 2, o + 3, 0, bn, BM_VEL_GHOST, 1, 100);
        ev_set(p, 2, o + 6, 0, bn + 12, 100, 1 | BM_LEN_TIE, 100);
        ev_set(p, 2, o + 7, 0, bn + 10, 100, 1, 100);
        ev_set(p, 2, o + 10, 0, bn, 100, 1, 100);
        ev_set(p, 2, o + 14, 0, bn + 7, 90, 1, 70);
        for (int k = 0; k < 3; k++) {
            ev_set(p, 3, o + 0, k, prog[pg][k], 85, 3, 100);
            ev_set(p, 3, o + 8, k, prog[pg][k], 75, 2, 100);
        }
        ev_set(p, 3, o + 11, 0, prog[pg][2] + 12, 70, 1, 60);
        ev_set(p, 4, o + 4, 0, prog[pg][0] + 12, 70, 2, 100);
        ev_set(p, 4, o + 12, 0, prog[pg][1] + 12, 60, 2, BM_PROB_FILL);
        ev_set(p, 5, o + 0, 0, prog[pg][0] - 12, 60, 16, 100);
    }
    p->trk[0].level = 80; p->trk[2].level = 76; p->trk[3].level = 56; p->trk[4].level = 48; p->trk[5].level = 44;   /* con el break sonando, sin saturar */
    memset(song->scene, 0, sizeof song->scene);
    song->scene[0].valid = 1; song->scene[0].pattern = 0;                                  /* A: todo */
    song->scene[1].valid = 1; song->scene[1].pattern = 0; song->scene[1].trk_mute = 0x3C;  /* B: solo ritmo */
    song->scene[2].valid = 1; song->scene[2].pattern = 0; song->scene[2].trk_mute = 0x01;  /* C: sin batería */
}

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
        int ok = has_glob && s.ver == BM_FMT, old = has_glob && !ok;
        for (int i = 0; i < BM_PATTERNS && !has_glob; i++) {     /* sin ajustes: se mira el formato de los patrones */
            int len = 0;
            const uint8_t *p = store_ptr(a, i, &len);
            if (p && len > (int)offsetof(bm_meta, fmt) && p[offsetof(bm_meta, fmt)] != BM_FMT) old = 1;
        }
        if (old) {                                 /* datos de 0.2: la 0.3 no los lee, se empieza de cero */
            bm_store_format(&a->store, pf->flash);
            bm_takes_format(pf->flash);
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
        if (src && len > 0 && bm_pattern_unpack(&a->pat, src, len) < 0) bm_pattern_init(&a->pat);
        bm_takes_scan(pf->flash, pf->flash_map, pf->user, a->take);
    }
    if (fresh) { demo_pattern(&a->pat, &a->song); a->dirty = 1; a->song_dirty = 1; a->set_dirty = 1; a->t_edit = now; }
    a->set.last_pat %= BM_PATTERNS;
    a->ctx.cur_pat = a->set.last_pat;
    a->ctx.st.pattern = a->set.last_pat;
    bm_engines_sync(&a->ctx);
    bm_seq_set_bpm(&a->ctx, a->pat.meta.bpm_x10);
    apply_settings(a);
    app_send_sources(a);
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
static const uint8_t TABS_BRK[] = {PG_SEQ, PG_SOUND, PG_RETRIG, PG_TAKES, PG_MIX, PG_MORE};
static const uint8_t TABS_NOTE[] = {PG_SEQ, PG_SOUND, PG_ENV, PG_MIX, PG_ARP, PG_LOOP, PG_MORE};
static const uint8_t TABS_MIDI[] = {PG_SEQ, PG_MIDI, PG_ARP, PG_MORE};

int bm_app_tabs(const bm_app *a, int t, uint8_t out[8])
{
    const uint8_t *s; int n;
    if (app_kind(a, t) == KIND_TR) { s = TABS_TR; n = (int)sizeof TABS_TR; }
    else if (app_kind(a, t) == KIND_BRK) { s = TABS_BRK; n = (int)sizeof TABS_BRK; }
    else if (app_midi(a, t)) { s = TABS_MIDI; n = (int)sizeof TABS_MIDI; }
    else { s = TABS_NOTE; n = (int)sizeof TABS_NOTE; }
    memcpy(out, s, (size_t)n);
    return n;
}

const char *bm_page_name(int page)
{
    static const char *const N[PG_COUNT] = {"SEQ", "SONIDO", "INSTR", "MEZCLA", "MAS", "RETRIG", "TOMAS", "ENV", "ARP", "LOOP",
                                            "MIDI", "ESCALA", "ACORDES", "LFO", "GRABAR", "CORTAR"};
    return page < PG_COUNT ? N[page] : "";
}

int app_page_ok(const bm_app *a, int t, int page)
{
    if (page == PG_SCL || page == PG_CHORDS || page == PG_LFO) return 1;
    if (page == PG_REC || page == PG_CUT) return t == 1;
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
    if (a->view == V_CH && a->page == PG_CUT && page != PG_CUT && a->take_dirty) app_job(a, JOB_TAKE_META, a->cut_take);
    a->page = (uint8_t)page;
    if (page != PG_REC && page != PG_CUT) a->ch_page[a->track] = (uint8_t)page;
    a->held_step = -1; a->held_white = -1;
    app_set_view(a, V_CH);
}

void app_select_track(bm_app *a, int t, int keep_page)
{
    if (t < 0 || t >= BM_TRACKS) return;
    a->track = (uint8_t)t;
    a->held_step = -1; a->held_white = -1;
    if (a->view == V_CH) {
        int pg = keep_page && app_page_ok(a, t, a->page) && a->page != PG_REC && a->page != PG_CUT ? a->page : a->ch_page[t];
        if (!app_page_ok(a, t, pg)) pg = PG_SEQ;
        a->page = (uint8_t)pg;
        a->ch_page[t] = (uint8_t)pg;
        int len = bm_trk_len(&a->pat, t, a->sel_lane[t]);
        if (a->step_page * 16 >= len) a->step_page = 0;
    }
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, bm_track_name(a, t));
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
    if (a->undo_track < 0) { app_toast(a, "NADA QUE DESHACER"); return; }
    int t = a->undo_track;
    memcpy(&a->redo, app_track_data(a, t), app_track_size(t));
    a->redo_track = (int8_t)t;
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->undo);
    a->undo_track = -1;
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, "DESHECHO EN T%d", t + 1);
    app_toast(a, b);
}

void app_do_redo(bm_app *a)
{
    if (a->redo_track < 0) { app_toast(a, "NADA QUE REHACER"); return; }
    int t = a->redo_track;
    memcpy(&a->undo, app_track_data(a, t), app_track_size(t));
    a->undo_track = (int8_t)t;
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->redo);
    a->redo_track = -1;
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, "REHECHO EN T%d", t + 1);
    app_toast(a, b);
}

void app_clear_track(bm_app *a, int t)
{
    app_undo_snapshot(a, t);
    app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
    app_edited(a);
    char b[30];
    snprintf(b, sizeof b, "T%d BORRADO (HOME+OCT- DESHACE)", t + 1);
    app_toast(a, b);
}

void app_copy_track(bm_app *a, int t)
{
    memcpy(&a->clip, app_track_data(a, t), app_track_size(t));
    a->clip_trk = a->pat.trk[t];
    a->clip_type = 2; a->clip_track = (uint8_t)t;
    app_toast(a, "CANAL COPIADO");
}

void app_paste_track(bm_app *a, int t)
{
    if (a->clip_type != 2 || app_kind(a, t) != bm_kind(&a->pat, a->clip_track) || (app_kind(a, t) == KIND_TR) != (a->clip_trk.engine == ENG_DRUM || a->clip_trk.engine == ENG_KIT)) {
        app_toast(a, "NADA QUE PEGAR AQUI"); return;
    }
    app_undo_snapshot(a, t);
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->clip);
    app_edited(a);
    app_toast(a, "CANAL PEGADO");
}

void app_init_sound(bm_app *a, int t)
{
    bm_track d; bm_track_defaults(&d, t);
    static const uint8_t F[] = {0, 1, 2, 3, TRK_OFF(attack), TRK_OFF(sustain), TRK_OFF(release), TRK_OFF(env_amt), TRK_OFF(glide), TRK_OFF(transpose)};
    for (unsigned i = 0; i < sizeof F; i++) app_push(a, CMD_SET_TRACK, (uint8_t)t, F[i], ((const uint8_t *)&d)[F[i]]);
    app_edited(a);
    app_toast(a, "SONIDO INICIAL");
}

void app_select_pattern(bm_app *a, int idx)
{
    char b[30];
    idx = bm_clampi(idx, 0, BM_PATTERNS - 1);
    if (app_playing(a)) {
        if (idx == a->ctx.st.pattern) { app_push(a, CMD_QUEUE, 0xFF, 0, 0); return; }
        if (a->dirty && a->pend_obj >= 0 && a->pend_obj != a->ctx.st.pattern) { app_toast(a, "PARA (STOP) PARA GUARDAR"); return; }
        app_push(a, CMD_QUEUE, (uint8_t)idx, 0, 0);
        snprintf(b, sizeof b, "A%02d EN COLA", idx + 1);
    } else {
        if (idx == a->ctx.st.pattern) return;
        app_save_jobs(a, a->dirty);
        app_job(a, JOB_LOAD, idx);
        snprintf(b, sizeof b, "PATRON A%02d", idx + 1);
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
    if (st->swap_fail) { st->swap_fail = 0; app_toast(a, "SIN CAMBIO: PARA Y GUARDA"); }
    if (a->layer == L_ERASE && app_playing(a) && a->erase_mask) {      /* BORRAR sonando */
        int t = a->track;
        if (t == 1) {                                                   /* sampler: lo que pasa por la fila TU */
            const bm_tr *tr = bm_trk_trc(&a->pat, 1);
            int s = bm_app_track_step(a, 1, BRK_GEN);
            for (int l = 0; s >= 0 && l <= BM_SLICES; l++)
                if (tr->st[l][s]) { app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0); app_edited(a); }
        } else if (bm_is_grid(&a->pat, t)) {
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
    if (a->wave_was_rec && !st->smp_rec && st->smp_unsaved) {          /* toma nueva: se dibuja ya */
        compute_wave(a->wave, (const int8_t *)a->amem.arena, st->smp_len);
        slice_energy(a, &a->ctx.smp[0]);
    }
    a->wave_was_rec = st->smp_rec;
    if (a->src_dirty || a->pat.trk[1].source != a->sent_src[0] || a->pat.trk[1].loop_b != a->sent_src[1]) app_send_sources(a);
    if (st->smp_unsaved && !st->smp_rec && !app_playing(a) && !a->njob && a->pf.flash) {   /* toma nueva: al parar */
        app_job(a, JOB_SAVE_TAKE, 0);
        app_toast(a, "GUARDANDO LA TOMA");
    }
    if (a->take_dirty && !app_playing(a) && !a->njob && a->pf.flash && now - a->t_edit > 1500
        && !(a->view == V_CH && a->page == PG_CUT))
        for (int i = 0; i < BM_TAKES; i++) if (a->take_dirty & (1u << i)) app_job(a, JOB_TAKE_META, i);
    if (a->set.autosave && !app_playing(a) && !a->njob && (a->dirty || a->pend_obj >= 0 || a->song_dirty || a->set_dirty)
        && now - a->t_edit > UI_AUTOSAVE_MS && a->pf.flash) {
        app_save_jobs(a, a->dirty);
        if (a->song_dirty) app_job(a, JOB_SAVE_SONG, 0);
        if (a->set_dirty) app_job(a, JOB_SAVE_GLOB, 0);
        app_toast(a, "AUTOGUARDADO");
    }
    jobs_run(a);
    app_leds(a);
    if (a->pf.blit) bm_screen_render(&a->scr, bm_app_draw, a, a->pf.blit, a->pf.user);
}
