/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — contexto de audio: cola de comandos, MIDI de entrada y render. */
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"

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
    c->scene_q = -1;
    for (int t = 0; t < BM_TRACKS; t++) {
        c->eng[t].ctx = c;
        c->eng[t].track = (uint8_t)t;
        c->eng[t].vt = t == 0 ? &bm_eng_drum : (t == 1 ? &bm_eng_sampler : &bm_eng_wave);
        c->eng[t].vt->init(&c->eng[t]);
    }
    bm_seq_set_bpm(c, pat->meta.bpm_x10);
    c->queued = -1;
    c->sc_trig_at = -1;
    c->punch_req = c->punch_cur = 0xFF;
    c->master_target = 820; c->master_gain = 0;
    c->fade_gain = 32767;
    c->drive = 25;
    c->sync_ppqn = 2;
    c->st.queued = 0xFF; c->st.punch = 0xFF; c->st.step = -1;
    c->st.scene = 0xFF; c->st.scene_q = 0xFF;
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
    for (int t = 0; t < BM_TR_TRACKS; t++) c->roll[t].mask = 0;
    memset(c->strum, 0, sizeof c->strum);
}

static void apply(bm_ctx *c, const bm_cmd *k)
{
    switch (k->type) {
    case CMD_NOTE_ON: bm_live_note(c, k->a, k->b, k->c, 1); break;
    case CMD_NOTE_OFF: bm_live_note(c, k->a, k->b, 0, 0); break;
    case CMD_SET_EVENT:
        if (k->a >= BM_TR_TRACKS && k->a < BM_TRACKS && k->b < BM_STEPS && k->c < BM_EVENTS)
            c->pat->ev[k->a - BM_TR_TRACKS][k->b][k->c] = k->u.ev;
        break;
    case CMD_SET_TRSTEP:
        if (k->a < BM_TR_TRACKS && k->b < BM_LANES && k->c < BM_STEPS) {
            uint8_t v = (uint8_t)k->u.i;
            if (!(v & ST_ON)) { bm_tr_lock_clear_step(&c->pat->tr[k->a], k->c, k->b); v = 0; }
            else v = (uint8_t)((v & ~ST_LOCK) | (c->pat->tr[k->a].st[k->b][k->c] & ST_LOCK));
            c->pat->tr[k->a].st[k->b][k->c] = v;
        }
        break;
    case CMD_TR_LOCK:
        if (k->a < BM_TR_TRACKS && k->b < BM_LANES && k->c < BM_STEPS)
            bm_tr_lock_set(&c->pat->tr[k->a], k->c, k->b, (k->u.i >> 8) & 0xFF, (k->u.i & 0x10000) ? -1 : (k->u.i & 0xFF));
        break;
    case CMD_SET_LANE:
        if (k->a < BM_TR_TRACKS && k->b < BM_LANES && k->c < 4) ((uint8_t *)&c->pat->tr[k->a].lane[k->b])[k->c] = (uint8_t)k->u.i;
        break;
    case CMD_LANE_MUTE:
        if (k->a < BM_TR_TRACKS) c->pat->tr[k->a].mute = (uint16_t)k->u.i;
        break;
    case CMD_SET_TRACK:
        if (k->a < BM_TRACKS && k->b < sizeof(bm_track)) ((uint8_t *)&c->pat->trk[k->a])[k->b] = k->c;
        break;
    case CMD_SET_META:
        if (k->b >= 2 && k->b < sizeof(bm_meta)) ((uint8_t *)&c->pat->meta)[k->b] = k->c;
        break;
    case CMD_SET_BPM: c->pat->meta.bpm_x10 = (uint16_t)k->u.i; if (!c->ext_clock) bm_seq_set_bpm(c, (uint16_t)k->u.i); break;
    case CMD_TRACK_BULK:
        if (!k->u.ptr || k->a >= BM_TRACKS) break;
        if (bm_is_tr(k->a)) memcpy(&c->pat->tr[k->a], k->u.ptr, sizeof(bm_tr));
        else memcpy(c->pat->ev[k->a - BM_TR_TRACKS], k->u.ptr, sizeof c->pat->ev[0]);
        break;
    case CMD_TRACK_CLEAR:
        if (k->a < BM_TRACKS) bm_track_clear_events(c->pat, k->a);
        break;
    case CMD_PLAY:
        if (k->a == 0) bm_seq_stop(c); else bm_seq_start(c, k->a == 2);
        break;
    case CMD_PUNCH: c->punch_req = k->a; break;
    case CMD_FILL: c->fill = k->a; break;
    case CMD_QUEUE: c->queued = (k->a == 0xFF) ? -1 : k->a; c->st.queued = k->a; break;
    case CMD_LOAD:
        if (c->swap && c->swap(c->swap_user, c->cur_pat, k->a, c->pat) == 0) {
            c->cur_pat = k->a; c->st.pattern = k->a; bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
        }
        break;
    case CMD_SONG: c->song_on = k->a; c->st.song_on = k->a; c->song_pos = 0; c->song_rep = 0; break;
    case CMD_SCENE: bm_scene_cmd(c, k->a); break;
    case CMD_CHAIN: if (k->u.ptr) bm_chain_cmd(c, (const uint8_t *)k->u.ptr, k->a); break;
    case CMD_SMP_REC:
        if (k->a && !c->smp_rec) {
            for (int i = 0; i < BM_VOICES; i++) if (c->voice[i].active && c->voice[i].track == 1) c->voice[i].active = 0;
            c->smp_rec = 1; c->smp_rec_pos = 0; c->smp_dec_acc = 0; c->smp_lp = 0;
            c->smp.data = (const int8_t *)c->mem->arena; c->smp.len = 0; c->smp.nmarks = 0;
            c->punch_cur = c->punch_req = 0xFF;
            memset(c->mem->arena, 0, sizeof c->mem->arena);
            c->arena_busy = 1; c->st.smp_marks = 0;
        } else if (!k->a && c->smp_rec) {
            c->smp_rec = 0; c->smp.len = c->smp_rec_pos; c->st.smp_unsaved = c->smp_rec_pos > 0;
        }
        c->st.smp_rec = c->smp_rec; c->st.smp_len = c->smp_rec ? 0 : c->smp.len;
        break;
    case CMD_SMP_MARK:
        if (c->smp_rec && c->smp.nmarks < BM_SLICES) { c->smp.marks[c->smp.nmarks++] = c->smp_rec_pos; c->st.smp_marks = c->smp.nmarks; }
        break;
    case CMD_SMP_SRC:
    case CMD_SMP_SAVED:
        if (c->smp_rec) break;
        for (int i = 0; i < BM_VOICES; i++) if (c->voice[i].active && c->voice[i].track == 1) c->voice[i].active = 0;
        if (k->u.ptr) c->smp = *(const bm_smp_src *)k->u.ptr; else memset(&c->smp, 0, sizeof c->smp);
        c->st.smp_len = c->smp.len; c->st.smp_marks = c->smp.nmarks;
        if (k->type == CMD_SMP_SAVED || !c->st.smp_unsaved) {
            c->arena_busy = 0; c->st.smp_unsaved = 0;
            memset(c->mem->arena, 0, sizeof c->mem->arena);   /* vuelve a ser delay + punch */
            c->d_out = c->d_acc = 0;
        }
        break;
    case CMD_MASTER: c->master_target = (uint16_t)k->u.i; break;
    case CMD_DRIVE: c->drive = k->a; break;
    case CMD_SYNC: c->sync_mode = k->a; c->sync_ppqn = k->b; c->sync_lat = k->c; break;
    case CMD_CLOCK:
        c->ext_clock = k->a; c->ext_budget = 0; c->ext_seen = 0; c->st.ext_ok = 0;
        if (!k->a) bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
        break;
    case CMD_MIDI: midi_byte(c, k->a); break;
    case CMD_ROLL:
        if (k->a < BM_TR_TRACKS && k->b < BM_LANES) {
            if (k->c) c->roll[k->a].mask |= (uint16_t)(1u << k->b); else c->roll[k->a].mask &= (uint16_t)~(1u << k->b);
            if (k->u.i > 0) c->roll[k->a].rate = (uint8_t)k->u.i;
        }
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

void bm_audio_overload(bm_ctx *c)
{
    bm_voice *worst = NULL;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N; i++) {
        bm_voice *v = &c->voice[i];
        if (!v->active || v->fade) continue;
        if (!worst || v->env < worst->env) worst = v;
    }
    if (worst) bm_voice_fade(worst);
}

void bm_audio_render(bm_ctx *c, int32_t *out, uint32_t out_frames)
{
    bm_cmd k;
    int guard = 0;
    while (guard++ < BM_CMDQ_LEN && bm_cmd_pop(c->cmdq, &k)) apply(c, &k);

    uint32_t n_eng = out_frames / BM_OVERSAMPLE;
    bm_seq_strum_block(c, n_eng);
    while (n_eng) {
        uint32_t n = n_eng < BM_BLOCK ? n_eng : BM_BLOCK;
        for (int t = 0; t < BM_TRACKS; t++) memset(c->tbuf[t], 0, n * sizeof(int32_t));
        c->sc_trig_at = -1;
        uint32_t done = 0;
        while (done < n) {
            bool can_tick = !c->ext_clock || c->ext_budget > 0;
            if (can_tick && c->acc_q16 >= c->spt_q16) {
                c->acc_q16 -= c->spt_q16;
                if (c->ext_clock) c->ext_budget--;
                c->blk_off = (uint16_t)done;
                bm_seq_tick(c);
                continue;
            }
            uint32_t k2 = n - done;
            if (can_tick) {
                uint32_t until = (c->spt_q16 - c->acc_q16 + 65535u) >> 16;
                if (until < 1) until = 1;
                if (until < k2) k2 = until;
            }
            for (int t = 0; t < BM_TRACKS; t++)
                c->eng[t].vt->process(&c->eng[t], &c->tbuf[t][done], (uint16_t)k2);
            c->acc_q16 += k2 << 16;
            if (!can_tick && c->acc_q16 > c->spt_q16) c->acc_q16 = c->spt_q16;   /* externo: espera el próximo pulso */
            c->sample_clock += k2;
            done += k2;
        }
        bm_dsp_mix(c, out, n);
        out += n * 2u * BM_OVERSAMPLE;
        n_eng -= n;
    }
}
