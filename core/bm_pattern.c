/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — patrón, formato disperso, P-Locks TR, escalas y cuadrícula de acordes. */
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "bm_pattern.h"
#include "bm_params.h"

typedef char bm_assert_event[(sizeof(bm_event) == 8) ? 1 : -1];
typedef char bm_assert_track[(sizeof(bm_track) == 48) ? 1 : -1];
typedef char bm_assert_meta[(sizeof(bm_meta) == 64) ? 1 : -1];
typedef char bm_assert_notes[(sizeof(bm_tr) <= sizeof(bm_notes)) ? 1 : -1];
typedef char bm_assert_lane[(sizeof(bm_lane) == 8) ? 1 : -1];
typedef char bm_assert_prm[(AN_NPRM <= BM_PRM) ? 1 : -1];

const uint8_t bm_speed_tps[SPD_COUNT] = {12, 16, 24, 32, 48, 96};
const char *const bm_speed_names[SPD_COUNT] = {"X2", "X3/2", "X1", "X3/4", "X1/2", "X1/4"};
const char *const bm_engine_names[ENG_COUNT] = {"DRUMS", "SAMPLER", "ANALOG", "FM", "ACID", "STRING", "MODAL", "KIT", "MIDI"};

void bm_event_clear(bm_event *e)
{
    e->note = BM_EV_EMPTY; e->vel = BM_VEL_NORMAL; e->len = 1; e->prob = 100;
    e->lk[0] = e->lk[1] = e->lk[2] = e->lk[3] = BM_LOCK_NONE;
}

bool bm_event_empty(const bm_event *e) { return e->note == BM_EV_EMPTY; }

int bm_trk_len(const bm_pattern *p, int t, int lane)
{
    int len = bm_is_grid(p, t) ? bm_trk_trc(p, t)->lane[lane < 0 ? 0 : lane & 15].len : p->trk[t].length;
    return len >= 1 && len <= BM_STEPS ? len : 16;
}

void bm_track_defaults(bm_track *t, int track)
{
    memset(t, 0, sizeof *t);
    t->macro[0] = 96; t->macro[1] = 30; t->macro[2] = 60; t->macro[3] = 0;
    t->level = 100; t->pan = 64; t->send = 0; t->duck = (track >= 1) ? 60 : 0;
    t->attack = 0; t->sustain = 80; t->release = 40; t->env_amt = 40; t->glide = 30;
    t->interp = (track == 1) ? 1 : 0;
    t->transpose = 24;
    t->arp_mode = 0; t->arp_rate = 1; t->arp_oct = 1; t->arp_gate = 60;
    t->speed = SPD_X1; t->length = 16;
    t->accent = 100; t->midi_ch = (uint8_t)track; t->scl = 0xFF; t->scl_root = 9;
    t->engine = track == 0 ? ENG_DRUM : track == 1 ? ENG_BREAK : ENG_ANALOG;
    t->loop_b = BM_SRC_NONE; t->pitch = 24;
    if (track == 0) { t->macro[0] = t->macro[1] = t->macro[2] = t->macro[3] = 64; }   /* batería: 64 = el kit tal cual */
    if (track == 1) {                                          /* pads: FILTER abierto, PITCH 0, DECAY nada, START 0 */
        t->macro[0] = 127; t->macro[1] = 64; t->macro[2] = 127; t->macro[3] = 0;
        t->source = BM_SRC_NONE;
        t->gen[0] = t->gen[1] = t->gen[2] = t->gen[3] = 0;    /* BREAK apagado: suena la rejilla */
        t->rt[0] = 100; t->rt[1] = 20; t->rt[2] = 40; t->rt[3] = 10;
    }
    /* canales de notas: preset de ANALOG (bm_params.c), lo carga bm_pattern_init */
    if (track == 2) t->source = 1;
    if (track == 3) { t->source = 4; t->send = 30; }
    if (track == 4) { t->source = 7; t->send = 20; }
    if (track == 5) { t->source = 3; t->send = 40; }
}

void bm_tr_defaults(bm_tr *tr)
{
    memset(tr, 0, sizeof *tr);
    for (int l = 0; l < BM_LANES; l++) {
        bm_lane *ln = &tr->lane[l];
        ln->len = 16; ln->level = 100; ln->tune = 64; ln->decay = 64; ln->tone = 64; ln->color = 64;
    }
    for (int i = 0; i < BM_LANE_LOCKS; i++) tr->lk[i].lane = 0xFF;
}

void bm_track_clear_events(bm_pattern *p, int track)
{
    if (bm_is_grid(p, track)) {
        bm_tr *tr = bm_trk_tr(p, track);
        memset(tr->st, 0, sizeof tr->st);
        for (int i = 0; i < BM_LANE_LOCKS; i++) tr->lk[i].lane = 0xFF;
        return;
    }
    for (int s = 0; s < BM_STEPS; s++)
        for (int e = 0; e < BM_EVENTS; e++)
            bm_event_clear(bm_ev(p, track, s, e));
}

static void sound_defaults(bm_track *t, int engine)
{
    if (engine == ENG_ACID) { t->macro[0] = 40; t->macro[1] = 96; t->macro[2] = 56; t->macro[3] = 90; t->source = 0; t->glide = 40; }
    else if (engine == ENG_FM) { t->macro[0] = 64; t->macro[1] = 64; t->macro[2] = 64; t->macro[3] = 64; t->source = 0; t->attack = 0; t->release = 64; }
}

bool bm_engine_ok(int track, int engine)
{
    if (track == 2) return engine == ENG_ANALOG || engine == ENG_ACID || engine == ENG_FM;
    if (bm_is_free(track)) return engine == ENG_ANALOG || engine == ENG_FM || engine == ENG_ACID || engine == ENG_KIT || engine == ENG_MIDI;
    return false;
}

static void notes_reset(bm_pattern *p, int track)
{
    bm_track_clear_events(p, track);
    p->loop[track - BM_TR_TRACKS].n = 0; p->loop[track - BM_TR_TRACKS].on = 0;
}

void bm_track_set_engine(bm_pattern *p, int track, int engine)
{
    if (!bm_engine_ok(track, engine)) return;
    bm_track *t = &p->trk[track];
    int was_tr = t->engine == ENG_KIT, was = t->engine;
    t->engine = (uint8_t)engine;
    if (engine == ENG_KIT) {
        bm_tr *tr = &p->nt[track - BM_TR_TRACKS].tr;
        bm_tr_defaults(tr);
        bm_kit_apply(tr, 0);
        t->macro[0] = t->macro[1] = t->macro[2] = t->macro[3] = 64; t->source = 0; t->duck = 0;
        return;
    }
    if (was_tr) notes_reset(p, track);                      /* de rejilla a notas: empiezan vacías */
    if (was == engine) return;
    if (engine == ENG_ANALOG) { t->source = track == 2 ? 1 : 0; bm_preset_apply(p, track, t->source); }
    else if (engine == ENG_MIDI) { t->macro[1] = 0; t->macro[2] = 0; t->macro[3] = 0; }   /* MIDI: programa y CC */
    else sound_defaults(t, engine);
}

void bm_pattern_init(bm_pattern *p)
{
    memset(p, 0, sizeof *p);
    p->meta.bpm_x10 = 1200; p->meta.swing = 50; p->meta.master = 4;
    p->meta.dly_time = 2; p->meta.dly_fb = 50; p->meta.dly_mix = 60;
    p->meta.sc_release = 50; p->meta.root = 9; p->meta.scale = 1;
    p->meta.chord_oct = 3; p->meta.fmt = BM_PAT_FMT; p->meta.transpose = 24;
    for (int t = 0; t < BM_TRACKS; t++) bm_track_defaults(&p->trk[t], t);
    for (int t = 0; t < BM_TR_TRACKS; t++) bm_tr_defaults(&p->tr[t]);
    for (int i = 0; i < BM_LANES; i++) bm_pad_clear(&p->pad[i]);
    for (int t = BM_TR_TRACKS; t < BM_TRACKS; t++) bm_track_clear_events(p, t);
    for (int i = 0; i < BM_SYN_TRACKS; i++) p->loop[i].bars = 2;
    bm_kit_apply(&p->tr[0], 0);
    for (int t = 2; t < BM_TRACKS; t++) bm_preset_apply(p, t, p->trk[t].source);
}

int bm_master_steps(const bm_meta *m) { return m->master * BM_PAGE_STEPS; }

int bm_pattern_count_events(const bm_pattern *p)
{
    int n = 0;
    for (int t = 0; t < BM_TRACKS; t++) {
        if (bm_is_grid(p, t)) {
            const bm_tr *tr = bm_trk_trc(p, t);
            for (int l = 0; l < BM_LANES; l++)
                for (int s = 0; s < BM_STEPS; s++) if (tr->st[l][s] & ST_ON) n++;
        } else {
            for (int s = 0; s < BM_STEPS; s++)
                for (int e = 0; e < BM_EVENTS; e++) if (!bm_event_empty(bm_evc(p, t, s, e))) n++;
        }
    }
    return n;
}

void bm_pad_clear(bm_pad *p)
{
    memset(p, 0, sizeof *p);
    p->src = BM_SRC_NONE; p->pitch = 24; p->release = 20;
}

/* ------------------------------------------------------------ P-Locks TR */
int bm_tr_lock_get(const bm_tr *tr, int step, int lane, int param)
{
    for (int i = 0; i < BM_LANE_LOCKS; i++) {
        const bm_lock *k = &tr->lk[i];
        if (k->lane == lane && k->step == step && k->param == param) return k->val;
    }
    return -1;
}

bool bm_tr_lock_set(bm_tr *tr, int step, int lane, int param, int val)
{
    int free_i = -1;
    for (int i = 0; i < BM_LANE_LOCKS; i++) {
        bm_lock *k = &tr->lk[i];
        if (k->lane == lane && k->step == step && k->param == param) {
            if (val < 0) k->lane = 0xFF; else k->val = (uint8_t)val;
            goto flag;
        }
        if (k->lane == 0xFF && free_i < 0) free_i = i;
    }
    if (val < 0) goto flag;
    if (free_i < 0) return false;
    tr->lk[free_i].step = (uint8_t)step; tr->lk[free_i].lane = (uint8_t)lane;
    tr->lk[free_i].param = (uint8_t)param; tr->lk[free_i].val = (uint8_t)val;
flag: {
        int any = 0;
        for (int i = 0; i < BM_LANE_LOCKS; i++) if (tr->lk[i].lane == lane && tr->lk[i].step == step) any = 1;
        if (any) tr->st[lane][step] |= ST_LOCK; else tr->st[lane][step] &= (uint8_t)~ST_LOCK;
    }
    return true;
}

void bm_tr_lock_clear_step(bm_tr *tr, int step, int lane)
{
    for (int i = 0; i < BM_LANE_LOCKS; i++)
        if (tr->lk[i].lane == lane && tr->lk[i].step == step) tr->lk[i].lane = 0xFF;
    tr->st[lane][step] &= (uint8_t)~ST_LOCK;
}

/* ------------------------------------------------------------ formato disperso (versión BM_FMT)
 * meta 64 (bpm LE, fmt) | canales 6x48 | parámetros 6x32 (formato 4) |
 * por canal, según su tipo:
 *   rejilla (T1, T2, KIT): líneas 16x8 + mute 2 | n u16 + n x (pos u16 = lane<<6|paso, valor u8) |
 *                          m u8 + m x 4 (P-Locks)
 *   notas: n u16 + n x (pos u8 = slot<<6|paso, evento 8)
 * loop x4: on, bars, n + n x (tick u16, nota, vel)
 * pads de T2: 16 x 12 */
#define NEED(k) do { if (o + (k) > cap) return -1; } while (0)

static int pack_tr(const bm_tr *tr, uint8_t *out, int o, int cap)
{
    NEED((int)sizeof tr->lane + 2 + 2);
    memcpy(out + o, tr->lane, sizeof tr->lane); o += (int)sizeof tr->lane;
    out[o++] = (uint8_t)tr->mute; out[o++] = (uint8_t)(tr->mute >> 8);
    int at = o; o += 2;
    int n = 0;
    for (int l = 0; l < BM_LANES; l++)
        for (int s = 0; s < BM_STEPS; s++) {
            uint8_t v = tr->st[l][s];
            if (!v) continue;
            NEED(3);
            uint16_t pos = (uint16_t)(l << 6 | s);
            out[o++] = (uint8_t)pos; out[o++] = (uint8_t)(pos >> 8); out[o++] = v;
            n++;
        }
    out[at] = (uint8_t)n; out[at + 1] = (uint8_t)(n >> 8);
    int mat = o; NEED(1); o++;
    int m = 0;
    for (int i = 0; i < BM_LANE_LOCKS; i++) {
        if (tr->lk[i].lane == 0xFF) continue;
        NEED(4); memcpy(out + o, &tr->lk[i], 4); o += 4; m++;
    }
    out[mat] = (uint8_t)m;
    return o;
}

int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap)
{
    int o = 0;
    NEED(64 + (int)sizeof p->trk);
    memcpy(out, &p->meta, 64);
    out[0] = (uint8_t)(p->meta.bpm_x10 & 0xFF); out[1] = (uint8_t)(p->meta.bpm_x10 >> 8);
    out[offsetof(bm_meta, fmt)] = BM_PAT_FMT;
    o = 64;
    memcpy(out + o, p->trk, sizeof p->trk); o += (int)sizeof p->trk;
    NEED((int)sizeof p->prm);
    memcpy(out + o, p->prm, sizeof p->prm); o += (int)sizeof p->prm;
    for (int t = 0; t < BM_TRACKS; t++) {
        if (bm_is_grid(p, t)) { o = pack_tr(bm_trk_trc(p, t), out, o, cap); if (o < 0) return -1; continue; }
        int at = o; NEED(2); o += 2;
        int n = 0;
        for (int s = 0; s < BM_STEPS; s++)
            for (int e = 0; e < BM_EVENTS; e++) {
                const bm_event *ev = bm_evc(p, t, s, e);
                if (bm_event_empty(ev)) continue;
                NEED(9);
                out[o++] = (uint8_t)(s | (e << 6));
                memcpy(out + o, ev, 8); o += 8;
                n++;
            }
        out[at] = (uint8_t)n; out[at + 1] = (uint8_t)(n >> 8);
    }
    for (int t = 0; t < BM_SYN_TRACKS; t++) {
        const bm_loop *lp = &p->loop[t];
        NEED(3 + 4 * lp->n);
        out[o++] = lp->on; out[o++] = lp->bars; out[o++] = lp->n;
        for (int i = 0; i < lp->n; i++) {
            out[o++] = (uint8_t)lp->ev[i].tick; out[o++] = (uint8_t)(lp->ev[i].tick >> 8);
            out[o++] = lp->ev[i].note; out[o++] = lp->ev[i].vel;
        }
    }
    NEED((int)sizeof p->pad);
    memcpy(out + o, p->pad, sizeof p->pad); o += (int)sizeof p->pad;
    return o;
}

#define GET(k) do { if (o + (k) > len) return -1; } while (0)

static int unpack_tr(bm_tr *tr, const uint8_t *in, int o, int len)
{
    bm_tr_defaults(tr);
    GET((int)sizeof tr->lane + 4);
    memcpy(tr->lane, in + o, sizeof tr->lane); o += (int)sizeof tr->lane;
    tr->mute = (uint16_t)(in[o] | (in[o + 1] << 8)); o += 2;
    int n = in[o] | (in[o + 1] << 8); o += 2;
    for (int i = 0; i < n; i++) {
        GET(3);
        int pos = in[o] | (in[o + 1] << 8);
        tr->st[(pos >> 6) & 15][pos & 63] = in[o + 2];
        o += 3;
    }
    GET(1);
    int m = in[o++];
    for (int i = 0; i < m; i++) {
        GET(4);
        if (i < BM_LANE_LOCKS) memcpy(&tr->lk[i], in + o, 4);
        o += 4;
    }
    for (int l = 0; l < BM_LANES; l++) {
        bm_lane *ln = &tr->lane[l];
        if (ln->len < 1 || ln->len > BM_STEPS) ln->len = 16;
        if (ln->model >= 16) ln->model = 0;
        ln->choke &= 3;
    }
    return o;
}

int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len)
{
    int o = 0;
    GET(64 + (int)sizeof p->trk);
    if (in[offsetof(bm_meta, fmt)] != BM_PAT_FMT) return -1;      /* hasta la v1, sin compatibilidad hacia atrás */
    bm_pattern_init(p);
    memcpy(&p->meta, in, 64);
    p->meta.bpm_x10 = (uint16_t)(in[0] | (in[1] << 8));
    p->meta.fmt = BM_PAT_FMT;
    o = 64;
    memcpy(p->trk, in + o, sizeof p->trk); o += (int)sizeof p->trk;
    GET((int)sizeof p->prm);
    memcpy(p->prm, in + o, sizeof p->prm); o += (int)sizeof p->prm;
    for (int t = 0; t < BM_TRACKS; t++) {                          /* los canales fijos no cambian de motor */
        uint8_t e = p->trk[t].engine;
        if (t < 2) p->trk[t].engine = (uint8_t)(t == 0 ? ENG_DRUM : ENG_BREAK);   /* T2: ENG_BREAK = el motor de pads */
        else if (!bm_engine_ok(t, e)) { p->trk[t].engine = ENG_ANALOG; bm_preset_apply(p, t, 0); }
    }
    for (int t = 0; t < BM_TRACKS; t++) {
        if (bm_is_grid(p, t)) {
            o = unpack_tr(bm_trk_tr(p, t), in, o, len);
            if (o < 0) return -1;
            continue;
        }
        bm_track_clear_events(p, t);
        GET(2);
        int n = in[o] | (in[o + 1] << 8); o += 2;
        for (int i = 0; i < n; i++) {
            GET(9);
            int s = in[o] & 0x3F, slot = in[o] >> 6;
            o++;
            memcpy(bm_ev(p, t, s, slot), in + o, 8);
            o += 8;
        }
    }
    for (int t = 0; t < BM_SYN_TRACKS; t++) {
        GET(3);
        bm_loop *lp = &p->loop[t];
        lp->on = in[o++]; lp->bars = in[o++]; lp->n = in[o++];
        if (lp->n > BM_LOOP_EVENTS) return -1;
        for (int i = 0; i < lp->n; i++) {
            GET(4);
            lp->ev[i].tick = (uint16_t)(in[o] | (in[o + 1] << 8));
            lp->ev[i].note = in[o + 2]; lp->ev[i].vel = in[o + 3];
            o += 4;
        }
        if (lp->bars != 1 && lp->bars != 2 && lp->bars != 4) lp->bars = 2;
    }
    GET((int)sizeof p->pad);
    memcpy(p->pad, in + o, sizeof p->pad); o += (int)sizeof p->pad;
    for (int i = 0; i < BM_LANES; i++) {
        bm_pad *d = &p->pad[i];
        if (d->src >= 8) bm_pad_clear(d);
        if (d->mode >= PAD_MODES) d->mode = PAD_ONESHOT;
        if (d->pitch > 48) d->pitch = 24;
    }
    if (p->meta.bpm_x10 < 400 || p->meta.bpm_x10 > 2400) p->meta.bpm_x10 = 1200;
    if (p->meta.swing < 50 || p->meta.swing > 75) p->meta.swing = 50;
    if (p->meta.master > 8) p->meta.master = 4;
    if (p->meta.scale >= BM_SCALES) p->meta.scale = 0;
    if (p->meta.transpose > 48) p->meta.transpose = 24;
    p->meta.root %= 12;
    for (int t = 0; t < BM_TRACKS; t++) {
        bm_track *tk = &p->trk[t];
        if (tk->speed >= SPD_COUNT) tk->speed = SPD_X1;
        if (tk->length < 1 || tk->length > BM_STEPS) tk->length = 16;
        if (tk->scl != 0xFF && tk->scl >= BM_SCALES) tk->scl = 0xFF;
        tk->scl_root %= 12; tk->midi_ch &= 15;
        if (tk->pitch > 48) tk->pitch = 24;
    }
    return o;
}

/* ------------------------------------------------------------ escalas */
const char *const bm_note_names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *const bm_scale_names[BM_SCALES] = {
    "MAJOR", "MINOR", "DORIAN", "PHRYG", "MIXO", "LYDIAN", "HARM MIN", "PENTA MA", "PENTA MI"};
static const int8_t SCALES[BM_SCALES][8] = {
    {0, 2, 4, 5, 7, 9, 11, -1}, {0, 2, 3, 5, 7, 8, 10, -1}, {0, 2, 3, 5, 7, 9, 10, -1},
    {0, 1, 3, 5, 7, 8, 10, -1}, {0, 2, 4, 5, 7, 9, 10, -1}, {0, 2, 4, 6, 7, 9, 11, -1},
    {0, 2, 3, 5, 7, 8, 11, -1}, {0, 2, 4, 7, 9, -1, -1, -1}, {0, 3, 5, 7, 10, -1, -1, -1}};

int bm_scale_len(int scale)
{
    int n = 0;
    while (n < 8 && SCALES[scale][n] >= 0) n++;
    return n;
}

int bm_scale_note(int root, int scale, int degree, int base_oct)
{
    int n = bm_scale_len(scale);
    int oct = degree / n, d = degree % n;
    if (d < 0) { d += n; oct--; }
    return bm_clampi(12 * (base_oct + 1) + root + SCALES[scale][d] + 12 * oct, 0, 127);
}

/* ext: 0 tríada, 1 +7, 2 +9, 3 SUS2, 4 SUS4 */
int bm_chord_notes(int root, int scale, int degree, int ext, int base_oct, uint8_t out[4])
{
    int k = 0;
    out[k++] = (uint8_t)bm_scale_note(root, scale, degree, base_oct);
    if (ext == 3) out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 1, base_oct);
    else if (ext == 4) out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 3, base_oct);
    else out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 2, base_oct);
    out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 4, base_oct);
    if (ext == 1) out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 6, base_oct);
    if (ext == 2) out[k++] = (uint8_t)bm_scale_note(root, scale, degree + 8, base_oct);
    return k;
}

static int quality(int root, int scale, int degree)   /* 0 mayor, 1 menor, 2 dism, 3 aum */
{
    uint8_t n[4];
    bm_chord_notes(root, scale, degree, 0, 3, n);
    int third = (n[1] - n[0] + 24) % 12, fifth = (n[2] - n[0] + 24) % 12;
    if (third == 3 && fifth == 6) return 2;
    if (third == 3) return 1;
    if (third == 4 && fifth == 8) return 3;
    return 0;
}

void bm_chord_name(int root, int scale, int degree, int ext, char *buf, int cap)
{
    uint8_t n[4];
    bm_chord_notes(root, scale, degree, 0, 3, n);
    int qq = quality(root, scale, degree);
    const char *q = qq == 2 ? "~" : qq == 1 ? "m" : qq == 3 ? "+" : "";
    const char *x = "";
    if (ext == 1) {
        uint8_t m[4]; bm_chord_notes(root, scale, degree, 1, 3, m);
        int sev = (m[3] - m[0] + 24) % 12;
        x = (sev == 11) ? "M7" : "7";
    } else if (ext == 2) x = "9";
    else if (ext == 3) { q = ""; x = "S2"; }
    else if (ext == 4) { q = ""; x = "S4"; }
    snprintf(buf, (size_t)cap, "%s%s%s", bm_note_names[n[0] % 12], q, x);
}

void bm_note_str(int note, char *buf, int cap)
{
    snprintf(buf, (size_t)cap, "%s%d", bm_note_names[note % 12], note / 12 - 1);
}

/* ------------------------------------------------------------ cuadrícula de acordes */
const char *const bm_chord_set_names[BM_CHORD_SETS] = {"DIATONIC", "PROGRESSIONS", "NEO SOUL"};

static const bm_pad_chord SETS[BM_CHORD_SETS][16] = {
    {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0},
     {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}},
    {{0, 0}, {4, 0}, {5, 0}, {3, 0}, {5, 0}, {3, 0}, {0, 0}, {4, 0},
     {1, 1}, {4, 1}, {0, 1}, {5, 1}, {0, 0}, {5, 0}, {3, 0}, {4, 0}},
    {{0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 2}, {5, 2}, {1, 2}, {0, 2},
     {3, 1}, {2, 1}, {5, 1}, {4, 4}, {1, 2}, {4, 1}, {0, 3}, {5, 1}},
};

bm_pad_chord bm_chord_pad(int set, int pad)
{
    return SETS[(set % BM_CHORD_SETS + BM_CHORD_SETS) % BM_CHORD_SETS][pad & 15];
}

int bm_chord_function(int degree)
{
    int d = degree % 7;
    if (d == 0 || d == 2 || d == 5) return 0;
    if (d == 1 || d == 3) return 1;
    return 2;
}

void bm_roman(int root, int scale, int degree, int ext, char *buf, int cap)
{
    static const char *const UP[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const LO[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    int d = degree % 7, q = quality(root, scale, degree);
    const char *x = ext == 1 ? "7" : ext == 2 ? "9" : ext == 3 ? "S2" : ext == 4 ? "S4" : "";
    snprintf(buf, (size_t)cap, "%s%s%s", q == 0 || q == 3 ? UP[d] : LO[d], q == 2 ? "~" : "", x);
}
