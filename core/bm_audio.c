/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — contexto de audio: cola de comandos, MIDI de entrada y render. */
#include <string.h>
#include <stddef.h>
#include "bm_audio.h"
#include "bm_wave.h"
#include "bm_prof.h"
#include "bm_util.h"
#include "bm_fm.h"
#include "bm_drums.h"
#include "bm_params.h"

bool bm_cmd_push(bm_cmdq *q, const bm_cmd *c)
{
    uint16_t w = q->w, nw = (uint16_t)((w + 1) % BM_CMDQ_LEN);
    if (nw == q->r) return false;          /* llena: se descarta (la UI lo reintenta) */
    q->q[w] = *c;
    q->w = nw;
    return true;
}

bool bm_cmd_pop(bm_cmdq *q, bm_cmd *c)
{
    uint16_t r = q->r;
    if (r == q->w) return false;
    *c = q->q[r];
    q->r = (uint16_t)((r + 1) % BM_CMDQ_LEN);
    return true;
}

void bm_audio_init(bm_ctx *c, bm_pattern *pat, bm_song *song, bm_cmdq *q, bm_audio_mem *mem)
{
    memset(c, 0, sizeof *c);
    c->pat = pat; c->song = song; c->cmdq = q;
    c->mem = mem;
    memset(mem, 0, sizeof *mem);
    c->smp_base_inc = (uint32_t)(((uint64_t)BM_SMP_RATE << 16) / BM_FS);
    bm_fm_init((float)BM_FS);
    bm_drm_init((float)BM_FS);
    bm_wv_init((float)BM_FS);
    bm_dsp_reset();
    c->scene_q = -1;
    for (int t = 0; t < BM_TRACKS; t++) {
        c->eng[t].ctx = c;
        c->eng[t].track = (uint8_t)t;
        c->eng[t].vt = bm_engine_vt_for(pat->trk[t].engine);
        c->eng[t].vt->init(&c->eng[t]);
    }
    bm_seq_set_bpm(c, pat->meta.bpm_x10);
    c->queued = -1;
    c->sc_trig_at = -1;
    c->punch_req = c->punch_cur = 0xFF;
    c->master_target = 820; c->master_gain = 0;
    c->fade_gain = 1.0f;
    c->drive = 25;
    c->sync_ppqn = 2;
    c->roll_cfg.swing = 50; c->roll_cfg.ramp_len = BM_BAR_TICKS;
    c->st.queued = 0xFF; c->st.punch = 0xFF; c->st.step = -1;
    c->st.scene = 0xFF; c->st.scene_q = 0xFF;
    c->st.brk_slice = -1;
    c->st.smp_pos_src = -1;
    for (int i = 0; i < 16; i++) c->st.gen_sl[i] = 0xFF;
}

/* ------------------------------------------------------------ grabadora (arena de 48 KiB) */
static void smp_voices_off(bm_ctx *c, int src)
{
    for (int i = 0; i < BM_VOICES; i++)
        if (c->voice[i].active && c->voice[i].track == 1 && (src < 0 || c->voice[i].src == src)) { c->voice[i].active = 0; c->voice[i].pend = 0; }
}

void bm_smp_rec_start(bm_ctx *c)
{
    smp_voices_off(c, -1);
    c->smp_armed = 0; c->st.smp_armed = 0;
    c->smp_rec = 1; c->smp_rec_pos = 0; c->smp_dec_acc = 0; memset(c->smp_z, 0, sizeof c->smp_z);
    c->punch_cur = c->punch_req = 0xFF;
    memset(c->mem->arena, 0, sizeof c->mem->arena);
    c->arena_busy = 1;
    c->st.smp_rec = 1; c->st.smp_len = 0;
}

void bm_smp_rec_stop(bm_ctx *c)
{
    c->smp_rec = 0; c->st.smp_rec = 0;
    uint32_t len = c->smp_rec_pos;
    if (!len) { c->arena_busy = c->st.smp_unsaved; return; }
    bm_smp_src *m = &c->smp[0];                         /* la toma nueva, sin guardar (se escucha en la grabadora) */
    smp_voices_off(c, BM_TAKE_NEW);
    memset(m, 0, sizeof *m);
    m->data = (const int8_t *)c->mem->arena; m->len = len; m->gain = 64;
    m->bpm_x10 = c->pat->meta.bpm_x10;
    uint32_t bar = (uint32_t)BM_SMP_RATE * 2400u / m->bpm_x10;     /* muestras por compás */
    uint32_t bars = (len + bar / 2) / bar;
    m->bars = (uint8_t)(bars < 1 ? 1 : bars > 8 ? 8 : bars);
    m->kslice = m->sslice = 0xFF;
    c->st.smp_unsaved = 1; c->st.smp_len = len;
    c->gen_blk = 0;
}

/* ------------------------------------------------------------ MIDI de entrada */
static uint8_t m_status, m_d1, m_have;

static void midi_byte(bm_ctx *c, uint8_t b)
{
    if (b >= 0xF8) {                        /* tiempo real */
        if (b == 0xF8 && c->ext_clock) {
            uint32_t now = c->sample_clock;
            if (c->ext_seen) {
                uint32_t iv = now - c->ext_last;
                if (iv > 4 && iv < BM_FS) {
                    uint32_t spt = (uint32_t)(((uint64_t)iv << 16) / 4u);
                    c->spt_q16 = (c->spt_q16 * 3u + spt) / 4u;   /* filtro de jitter */
                }
            }
            c->ext_last = now; c->ext_seen = 1;
            if (c->ext_budget < 12) c->ext_budget += 4;
            c->st.ext_ok = 1;
        } else if (b == 0xFA && c->ext_clock) {
            c->ext_budget = 0; bm_seq_start(c, 1);
        } else if (b == 0xFB && c->ext_clock) {
            c->playing = 1; c->st.playing = 1;
        } else if (b == 0xFC && c->ext_clock) {
            bm_seq_stop(c);
        }
        return;
    }
    if (b & 0x80) { m_status = b; m_have = 0; return; }
    if (!m_status) return;
    if (!m_have) { m_d1 = b; m_have = 1; if ((m_status & 0xE0) == 0xC0) m_have = 0; return; }
    m_have = 0;
    uint8_t ch = m_status & 0x0F, ty = m_status & 0xF0;
    if (ch >= BM_TRACKS) return;           /* canales 1..6 = pistas */
    if (ch == 1) m_d1 &= 7;                 /* sampler: la nota elige el slice */
    if (ty == 0x90 && b) bm_live_note(c, ch, m_d1, b, 1);
    else if (ty == 0x80 || (ty == 0x90 && !b)) bm_live_note(c, ch, m_d1, 0, 0);
}

/* ------------------------------------------------------------ comandos */
static void all_off(bm_ctx *c)
{
    for (int i = 0; i < BM_VOICES; i++) {
        bm_voice *v = &c->voice[i];
        if (v->active) { v->gate = 0; if (v->stage != ENV_OFF) v->stage = ENV_REL; bm_voice_fade(v); }
        v->pend = 0;
    }
    for (int t = 0; t < BM_TRACKS; t++) { c->arp[t].n = 0; c->arp[t].playing = 0; }
    for (int t = 0; t < BM_TR_TRACKS; t++) { c->roll[t].mask = 0; c->roll[t].tail_m = 0; }
    c->brk_live = 0;
    memset(c->strum, 0, sizeof c->strum);
}

static void apply(bm_ctx *c, const bm_cmd *k)
{
    switch (k->type) {
    case CMD_NOTE_ON: bm_live_note(c, k->a, k->b, k->c, 1); break;
    case CMD_NOTE_OFF: bm_live_note(c, k->a, k->b, 0, 0); break;
    case CMD_SET_EVENT:
        if (k->a >= BM_TR_TRACKS && k->a < BM_TRACKS && k->b < BM_STEPS && k->c < BM_EVENTS && !bm_is_grid(c->pat, k->a))
            *bm_ev(c->pat, k->a, k->b, k->c) = k->u.ev;
        break;
    case CMD_SET_TRSTEP:
        if (k->a < BM_TRACKS && bm_is_grid(c->pat, k->a) && k->b < BM_LANES && k->c < BM_STEPS) {
            bm_tr *tr = bm_trk_tr(c->pat, k->a);
            uint8_t v = (uint8_t)k->u.i;
            if (!(v & ST_ON)) { bm_tr_lock_clear_step(tr, k->c, k->b); v = 0; }
            else v = (uint8_t)((v & ~ST_LOCK) | (tr->st[k->b][k->c] & ST_LOCK));
            tr->st[k->b][k->c] = v;
        }
        break;
    case CMD_TR_LOCK:
        if (k->a < BM_TRACKS && bm_is_grid(c->pat, k->a) && k->b < BM_LANES && k->c < BM_STEPS)
            bm_tr_lock_set(bm_trk_tr(c->pat, k->a), k->c, k->b, (k->u.i >> 8) & 0xFF, (k->u.i & 0x10000) ? -1 : (k->u.i & 0xFF));
        break;
    case CMD_SET_LANE:
        if (k->a < BM_TRACKS && bm_is_grid(c->pat, k->a) && k->b < BM_LANES && k->c < sizeof(bm_lane)) ((uint8_t *)&bm_trk_tr(c->pat, k->a)->lane[k->b])[k->c] = (uint8_t)k->u.i;
        break;
    case CMD_SET_PRM:
        if (k->a < BM_TRACKS && k->b < BM_PRM) c->pat->prm[k->a][k->b] = k->c;
        break;
    case CMD_PRESET:
        if (k->a >= BM_TRACKS) break;
        if (bm_is_tr(c->pat, k->a)) bm_kit_apply(bm_trk_tr(c->pat, k->a), k->b);
        else bm_preset_apply(c->pat, k->a, k->b);
        c->pat->trk[k->a].source = k->b;
        break;
    case CMD_LANE_MUTE:
        if (k->a < BM_TRACKS && bm_is_grid(c->pat, k->a)) bm_trk_tr(c->pat, k->a)->mute = (uint16_t)k->u.i;
        break;
    case CMD_SET_TRACK:
        if (k->a < BM_TRACKS && k->b < sizeof(bm_track) && k->b != offsetof(bm_track, engine)) {
            ((uint8_t *)&c->pat->trk[k->a])[k->b] = k->c;
            if (k->b == offsetof(bm_track, midi_out)) bm_engines_sync(c);
        }
        break;
    case CMD_SET_META:
        if (k->b >= 2 && k->b < sizeof(bm_meta)) ((uint8_t *)&c->pat->meta)[k->b] = k->c;
        break;
    case CMD_SET_BPM: c->pat->meta.bpm_x10 = (uint16_t)k->u.i; if (!c->ext_clock) bm_seq_set_bpm(c, (uint16_t)k->u.i); break;
    case CMD_TRACK_BULK:
        if (!k->u.ptr || k->a >= BM_TRACKS) break;
        if (k->a < BM_TR_TRACKS) memcpy(&c->pat->tr[k->a], k->u.ptr, sizeof(bm_tr));
        else memcpy(&c->pat->nt[k->a - BM_TR_TRACKS], k->u.ptr, sizeof(bm_notes));
        break;
    case CMD_TRACK_CLEAR:
        if (k->a < BM_TRACKS) bm_track_clear_events(c->pat, k->a);
        break;
    case CMD_PLAY:
        if (k->a == 0) bm_seq_stop(c); else bm_seq_start(c, k->a == 2);
        break;
    case CMD_PUNCH: c->punch_req = k->a; break;
    case CMD_FILL: c->fill = k->a; break;
    case CMD_QUEUE: c->queued = (k->a == 0xFF) ? -1 : k->a; c->st.queued = k->a; c->pchain_n = 0; c->st.pchain_n = 0; break;
    case CMD_LOAD:
        if (c->swap && c->swap(c->swap_user, c->cur_pat, k->a, c->pat) == 0) {
            c->cur_pat = k->a; c->st.pattern = k->a; bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
            bm_engines_sync(c);
            c->gen_blk = 0;
        }
        break;
    case CMD_PCHAIN: if (k->u.ptr) bm_pchain_cmd(c, (const uint8_t *)k->u.ptr, k->a); break;
    case CMD_SONG: c->song_on = k->a; c->st.song_on = k->a; c->song_pos = 0; c->song_rep = 0; break;
    case CMD_SCENE: bm_scene_cmd(c, k->a); break;
    case CMD_CHAIN: if (k->u.ptr) bm_chain_cmd(c, (const uint8_t *)k->u.ptr, k->a); break;
    case CMD_SMP_REC:
        c->smp_cap = k->u.i > 0 && (uint32_t)k->u.i < BM_SMP_REC_MAX ? (uint32_t)k->u.i : BM_SMP_REC_MAX;
        c->st.smp_cap = c->smp_cap;
        if (k->a == 2 && !c->smp_rec) { c->smp_armed = 1; c->st.smp_armed = 1; }
        else if (k->a == 1 && !c->smp_rec) bm_smp_rec_start(c);
        else if (k->a == 0) {
            if (c->smp_rec) bm_smp_rec_stop(c);
            c->smp_armed = 0; c->st.smp_armed = 0;
        }
        break;
    case CMD_SMP_IMPORT:                                  /* una toma que llega por la consola: queda como recién grabada */
        if (k->a == 0) {
            if (c->smp_rec) bm_smp_rec_stop(c);
            smp_voices_off(c, -1);
            c->smp_armed = 0; c->st.smp_armed = 0;
            c->punch_cur = c->punch_req = 0xFF;
            memset(c->mem->arena, 0, sizeof c->mem->arena);
            c->arena_busy = 1; c->st.smp_unsaved = 0;
        } else if (c->arena_busy && !c->smp_rec && k->u.i > 0) {
            c->smp_rec_pos = (uint32_t)k->u.i < BM_SMP_REC_MAX ? (uint32_t)k->u.i : BM_SMP_REC_MAX;
            bm_smp_rec_stop(c);
            uint16_t bpm = (uint16_t)(k->b << 8 | k->c);
            bm_smp_src *m = &c->smp[0];
            if (bpm >= 400 && bpm <= 2400 && m->len) {
                m->bpm_x10 = bpm;
                uint32_t bar = (uint32_t)BM_SMP_RATE * 2400u / bpm, bars = (m->len + bar / 2) / bar;
                m->bars = (uint8_t)(bars < 1 ? 1 : bars > 8 ? 8 : bars);
            }
        }
        break;
    case CMD_SMP_TAKES:
        if (!k->u.ptr) break;
        for (int t = 0; t < BM_TAKES; t++) {                 /* una toma que cambió de lugar: se callan sus voces */
            const bm_smp_src *n = (const bm_smp_src *)k->u.ptr + t;
            if (n->data != c->takes[t].data || n->len != c->takes[t].len) smp_voices_off(c, t);
        }
        memcpy(c->takes, k->u.ptr, sizeof c->takes);
        break;
    case CMD_SMP_SAVED:
    case CMD_SMP_DISCARD:
        if (c->smp_rec) break;
        smp_voices_off(c, BM_TAKE_NEW);
        if (k->u.ptr) c->smp[0] = *(const bm_smp_src *)k->u.ptr; else memset(&c->smp[0], 0, sizeof c->smp[0]);
        c->arena_busy = 0; c->st.smp_unsaved = 0;
        memset(c->mem->arena, 0, sizeof c->mem->arena);   /* vuelve a ser delay + punch */
        c->d_acc = c->d_prev = c->d_cur = 0;
        break;
    case CMD_BRK_LIVE:
        c->brk_live = (uint8_t)(k->a & 7);
        break;
    case CMD_PAD_KEY:
        bm_pad_key(c, k->b, k->c ? k->c : 110, k->a);
        break;
    case CMD_SMP_PREVIEW:
        bm_smp_preview(c, k->b, (uint32_t)k->u.i & 0xFFFFu, (uint32_t)k->u.i >> 16, k->a);
        break;
    case CMD_ENGINE:
        if (k->a < BM_TRACKS && bm_engine_ok(k->a, k->b)) {
            for (int i = 0; i < BM_VOICES; i++) if (c->voice[i].track == k->a) { c->voice[i].active = 0; c->voice[i].pend = 0; }
            bm_track_set_engine(c->pat, k->a, k->b);
            bm_engines_sync(c);
            if (k->a >= BM_TR_TRACKS) c->lp[k->a - BM_TR_TRACKS].mode = LOOP_OFF;
            c->st.edits++;
        }
        break;
    case CMD_MIDI_SEND:
        if (c->midi_out) {
            c->midi_out(c->midi_user, k->a);
            if (k->b != 0xFF) c->midi_out(c->midi_user, k->b);
            if (k->c != 0xFF) c->midi_out(c->midi_user, k->c);
        }
        break;
    case CMD_MASTER: c->master_target = (uint16_t)k->u.i; break;
    case CMD_DRIVE: c->drive = k->a; break;
    case CMD_MOVE: c->move = (int8_t)k->a; break;
    case CMD_SYNC: c->sync_mode = k->a; c->sync_ppqn = k->b; c->sync_lat = k->c; break;
    case CMD_CLOCK:
        c->ext_clock = k->a; c->ext_budget = 0; c->ext_seen = 0; c->st.ext_ok = 0;
        if (!k->a) bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
        break;
    case CMD_MIDI: midi_byte(c, k->a); break;
    case CMD_ROLL:
        if (k->a < BM_TR_TRACKS && k->b < BM_LANES) {
            uint16_t bit = (uint16_t)(1u << k->b);
            if (k->c) {                                  /* empieza (o cambia de velocidad): la rampa, desde cero */
                if (!(c->roll[k->a].mask & bit)) c->roll[k->a].age[k->b] = 0;
                c->roll[k->a].mask |= bit; c->roll[k->a].tail_m &= (uint16_t)~bit;
            } else if (c->roll[k->a].mask & bit) {       /* soltar: sigue la cola */
                c->roll[k->a].mask &= (uint16_t)~bit;
                if (c->roll_cfg.tail) { c->roll[k->a].tail_m |= bit; c->roll[k->a].tail[k->b] = c->roll_cfg.tail; }
            }
            if (k->u.i > 0) c->roll[k->a].rate = (uint8_t)k->u.i;
        }
        break;
    case CMD_ROLL_CFG:
        c->roll_cfg.ramp = k->a; c->roll_cfg.swing = k->b; c->roll_cfg.tail = k->c;
        c->roll_cfg.ramp_len = (uint16_t)k->u.i;
        break;
    case CMD_REC: c->rec = k->a; break;
    case CMD_LOOP: bm_loop_cmd(c, k->a, k->b, k->c); break;
    case CMD_STRUM: {
        uint8_t n[4] = {(uint8_t)k->u.i, (uint8_t)(k->u.i >> 8), (uint8_t)(k->u.i >> 16), (uint8_t)(k->u.i >> 24)};
        bm_strum_cmd(c, k->a, n, k->b > 4 ? 4 : k->b, k->c);
        break;
    }
    case CMD_ARP:
        if (k->a < BM_TRACKS) {
            c->arp[k->a].on = k->b;
            if (!k->b && c->pat->trk[k->a].arp_mode == 0) {
                if (c->arp[k->a].playing) c->eng[k->a].vt->note_off(&c->eng[k->a], c->arp[k->a].cur);
                c->arp[k->a].n = 0; c->arp[k->a].playing = 0;
            }
        }
        break;
    case CMD_ALL_OFF: all_off(c); break;
    case CMD_FADE: c->fade_req = k->a; break;
    default: break;
    }
}

int bm_audio_overload(bm_ctx *c)
{
    bm_voice *worst = NULL;
    int n = 0;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N; i++) {
        bm_voice *v = &c->voice[i];
        if (v->pend) n++;
        if (!v->active || v->fade) continue;
        n += !v->pend;
        /* la que menos se oye, y entre una cola y una nota sostenida, la cola */
        int tail = v->stage == ENV_REL, wt = worst && worst->stage == ENV_REL;
        if (!worst || tail > wt || (tail == wt && v->env < worst->env)) worst = v;
    }
    int cap = c->pool_cap ? c->pool_cap : BM_V_POOL_N;
    if (n < cap) cap = n;                       /* el tope arranca de lo que suena ahora */
    if (cap > BM_POOL_MIN) cap--;
    else cap = BM_POOL_MIN;                     /* la carga es de otro lado (batería, FX): no se baja más */
    c->pool_cap = (uint8_t)cap;
    if (n <= cap || !worst) return 0;
    bm_voice_fade(worst);
    return 1;
}

void bm_audio_relief(bm_ctx *c)
{
    if (c->pool_cap && ++c->pool_cap >= BM_V_POOL_N) c->pool_cap = 0;
}

/* ticks entre dos lecturas del reloj; leído desde el otro núcleo puede ir un pelo atrás: cero */
static inline uint32_t span(uint32_t a, uint32_t b) { uint32_t d = b - a; return d >= 0x80000000u ? 0 : d; }

/* el canal t calcula [done, done + n); midiendo, suma lo que tardó (el otro núcleo también) */
static bool track_process(bm_ctx *c, int t, uint32_t done, uint32_t n, int mide)
{
    uint32_t t0 = mide ? c->now() : 0;
    bool r = c->eng[t].vt->process(&c->eng[t], &c->tbuf[t][done], (uint16_t)n);
    if (mide) c->cost_acc[t] += span(t0, c->now());
    return r;
}

static int midiendo(const bm_ctx *c) { return c->now && c->par_auto && (c->par_n % BM_PAR_MIDE) == 0; }

void bm_audio_par_process(bm_ctx *c, uint32_t done, uint32_t n)
{
    uint8_t act = 0;
    int mide = midiendo(c);
    for (int t = 0; t < BM_TRACKS; t++)
        if (((c->par_mask >> t) & 1) && track_process(c, t, done, n, mide)) act |= (uint8_t)(1u << t);
    c->par_tact = act;
}

uint8_t bm_audio_par_pick(const uint32_t cost[BM_TRACKS], uint32_t over, uint8_t cur)
{
    uint32_t best = 0xFFFFFFFFu, now_load = 0xFFFFFFFFu;
    uint8_t bm = cur;
    for (unsigned m = 0; m < (1u << BM_TRACKS); m++) {
        uint32_t l0 = over, l1 = 0;
        for (int t = 0; t < BM_TRACKS; t++) { if ((m >> t) & 1) l1 += cost[t]; else l0 += cost[t]; }
        uint32_t mx = l0 > l1 ? l0 : l1;
        if (m == cur) now_load = mx;
        if (mx < best) { best = mx; bm = (uint8_t)m; }
    }
    /* cambiar solo si mejora de verdad: un canal que va y viene entre núcleos no gana nada */
    return (uint64_t)best * 10u < (uint64_t)now_load * 8u ? bm : cur;
}

/* fin de un render medido: costos que suben de golpe y bajan 1/16 por medición; cada
 * BM_PAR_ELIGE renders, el reparto (antes de calcular nada: los dos núcleos están quietos) */
static void par_balance(bm_ctx *c)
{
    for (int t = 0; t < BM_TRACKS; t++) {
        uint32_t a = c->cost_acc[t], k = c->cost[t] - (c->cost[t] >> 4);
        c->cost[t] = a > k ? a : k;
        c->cost_acc[t] = 0;
    }
    uint32_t k = c->cost_over - (c->cost_over >> 4);
    c->cost_over = c->over_acc > k ? c->over_acc : k;
    c->over_acc = 0;
}

void bm_audio_render(bm_ctx *c, int32_t *out, uint32_t out_frames)
{
    bm_cmd k;
    int guard = 0;
    c->par_n++;
    if (c->par_auto && c->par_on && (c->par_n % BM_PAR_ELIGE) == 0)
        c->par_mask = bm_audio_par_pick(c->cost, c->cost_over, c->par_mask);
    int mide = midiendo(c);
    uint32_t o0 = mide ? c->now() : 0;
    BM_PROF_T0();
    while (guard++ < BM_CMDQ_LEN && bm_cmd_pop(c->cmdq, &k)) apply(c, &k);
    BM_PROF_LAP(BM_P_CMD);

    uint32_t n_eng = out_frames / BM_OVERSAMPLE;
    bm_seq_strum_block(c, n_eng);
    BM_PROF_LAP(BM_P_SEQ);
    if (mide) c->over_acc += span(o0, c->now());
    while (n_eng) {
        uint32_t n = n_eng < BM_BLOCK ? n_eng : BM_BLOCK;
        for (int t = 0; t < BM_TRACKS; t++) if (c->tact & (1u << t)) memset(c->tbuf[t], 0, sizeof c->tbuf[t]);   /* solo los que sonaron */
        c->tact = 0;
        c->sc_trig_at = -1;
        uint32_t done = 0;
        while (done < n) {
            bool can_tick = !c->ext_clock || c->ext_budget > 0;
            if (can_tick && c->acc_q16 >= c->spt_q16) {
                c->acc_q16 -= c->spt_q16;
                if (c->ext_clock) c->ext_budget--;
                c->blk_off = (uint16_t)done;
                BM_PROF_RESTART();
                uint32_t s0 = mide ? c->now() : 0;
                bm_seq_tick(c);
                if (mide) c->over_acc += span(s0, c->now());
                BM_PROF_LAP(BM_P_SEQ);
                continue;
            }
            uint32_t k2 = n - done;
            if (can_tick) {
                uint32_t until = (c->spt_q16 - c->acc_q16 + 65535u) >> 16;
                if (until < 1) until = 1;
                if (until < k2) k2 = until;
            }
            BM_PROF_RESTART();
            int par = c->par_on && c->par_mask && c->par_go && c->par_wait;
            if (par) c->par_go(c->par_user, done, k2);            /* los canales de par_mask, en el otro núcleo */
            for (int t = 0; t < BM_TRACKS; t++) {
                if (par && ((c->par_mask >> t) & 1)) continue;
                if (track_process(c, t, done, k2, mide)) c->tact |= (uint8_t)(1u << t);
                BM_PROF_LAP(BM_P_T0 + t);
            }
            if (par) {
                c->par_wait(c->par_user);
                c->tact |= c->par_tact;
                if (c->par_defer) {                                  /* voces que cambiaron de núcleo */
                    c->par_defer = 0;
                    for (int i = 0; i < BM_VOICES; i++) {
                        bm_voice *v = &c->voice[i];
                        if (!v->pend || v->active) continue;
                        v->pend = 0;
                        bm_engine *e = &c->eng[v->pend_track];
                        e->vt->voice_start(e, v, v->pend_note, v->pend_vel, v->pend_lk, v->pend_legato);
                    }
                }
            }
            c->acc_q16 += k2 << 16;
            if (!can_tick && c->acc_q16 > c->spt_q16) c->acc_q16 = c->spt_q16;   /* externo: espera el próximo pulso */
            c->sample_clock += k2;
            done += k2;
        }
        BM_PROF_RESTART();
        uint32_t m0 = mide ? c->now() : 0;
        bm_dsp_mix(c, out, n);
        if (mide) c->over_acc += span(m0, c->now());
        BM_PROF_LAP(BM_P_MIX);
        out += n * 2u * BM_OVERSAMPLE;
        n_eng -= n;
    }
    if (mide) par_balance(c);
}
