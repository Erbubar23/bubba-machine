/* Bubba Machine — secuenciador (DISENO_0.2 §3-§8).
 *
 * Reloj de 96 PPQN calculado desde el contador de muestras. Cada pista avanza
 * a su velocidad (ticks por paso) y con su largo; cada instrumento TR, con el
 * suyo. El "largo maestro" reinicia todas juntas y es donde cambian patrones,
 * escenas y canción. */
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"

static const uint8_t ARP_RATE[3] = {48, 24, 12};

uint32_t bm_ticks_to_samples(const bm_ctx *c, uint32_t ticks)
{
    return (uint32_t)(((uint64_t)c->spt_q16 * ticks) >> 16);
}

void bm_seq_set_bpm(bm_ctx *c, uint16_t bpm_x10)
{
    if (bpm_x10 < 400) bpm_x10 = 400;
    if (bpm_x10 > 2400) bpm_x10 = 2400;
    c->spt_q16 = (uint32_t)(((uint64_t)BM_FS * 600u * 65536u) / ((uint64_t)bpm_x10 * BM_PPQN));
}

int bm_track_tps(const bm_ctx *c, int t) { return bm_speed_tps[c->pat->trk[t].speed % SPD_COUNT]; }

static bool fill_now(const bm_ctx *c)
{
    if (c->fill) return true;
    int e = c->pat->meta.fill_every;
    if (!e) return false;
    return (c->btick / BM_BAR_TICKS) % (uint32_t)e == (uint32_t)(e - 1);
}

static void syn_gates_off(bm_ctx *c)
{
    for (int i = 0; i < BM_SYN_TRACKS; i++)
        for (int k = 0; k < BM_EVENTS; k++)
            if (c->gate[i][k].ticks) {
                bm_engine *e = &c->eng[i + BM_TR_TRACKS];
                e->vt->note_off(e, c->gate[i][k].note);
                c->gate[i][k].ticks = 0; c->gate[i][k].tie = 0;
            }
}

/* ------------------------------------------------------------ TR */
static void rq_push(bm_ctx *c, int t, int lane, int vel, int ticks)
{
    if (ticks < 1) ticks = 1;
    for (int i = 0; i < 16; i++)
        if (!c->rq[t][i].ticks) { c->rq[t][i].lane = (uint8_t)lane; c->rq[t][i].vel = (uint8_t)vel; c->rq[t][i].ticks = (uint16_t)ticks; return; }
}

static void rq_tick(bm_ctx *c)
{
    for (int t = 0; t < BM_TR_TRACKS; t++)
        for (int i = 0; i < 16; i++) {
            bm_retrig *r = &c->rq[t][i];
            if (r->ticks && --r->ticks == 0) {
                c->eng[t].vt->note_on(&c->eng[t], r->lane, r->vel);
                c->st.lane_trig[t] |= (uint16_t)(1u << r->lane);
            }
        }
}

static void tr_hit(bm_ctx *c, int t, int lane, int s, uint8_t b, int tps)
{
    bm_engine *e = &c->eng[t];
    const bm_tr *tr = &c->pat->tr[t];
    if (b & ST_LOCK)
        for (int i = 0; i < BM_LANE_LOCKS; i++)
            if (tr->lk[i].lane == lane && tr->lk[i].step == s && tr->lk[i].param < 4)
                e->vt->set_macro(e, (uint8_t)(BM_MACRO_LOCK | tr->lk[i].param), tr->lk[i].val);
    uint8_t vel = (b & ST_ACC) ? 127 : 96;
    e->vt->note_on(e, (uint8_t)lane, vel);
    int rat = ((b & ST_RAT_MASK) >> ST_RAT_SH) + 1;
    for (int k = 1; k < rat; k++) rq_push(c, t, lane, vel * 85 / 100, k * tps / rat);
    if (b & ST_FLAM) rq_push(c, t, lane, vel * 6 / 10, 2);
    c->st.lane_trig[t] |= (uint16_t)(1u << lane);
}

static void fire_tr(bm_ctx *c, int t, uint32_t n, int tps, bool fill)
{
    const bm_tr *tr = &c->pat->tr[t];
    if (c->pat->trk[t].mute) return;
    int fired = 0;
    for (int l = 0; l < BM_LANES; l++) {
        if (tr->mute & (1u << l)) continue;
        int len = tr->lane[l].len ? tr->lane[l].len : 16;
        int s = (int)(n % (uint32_t)len);
        uint8_t b = tr->st[l][s];
        if (!(b & ST_ON)) continue;
        int cond = (b & ST_COND_MASK) >> ST_COND_SH;
        if ((cond == 1 && !fill) || (cond == 2 && fill)) continue;
        tr_hit(c, t, l, s, b, tps);
        fired = 1;
    }
    if (fired) c->st.trig[t]++;
}

/* ------------------------------------------------------------ synths */
static void fire_syn(bm_ctx *c, int t, uint32_t n, int tps, bool fill)
{
    int i = t - BM_TR_TRACKS;
    const bm_track *tr = &c->pat->trk[t];
    bm_engine *e = &c->eng[t];
    int len = tr->length ? tr->length : 16;
    int s = (int)(n % (uint32_t)len);
    int fired = 0;
    for (int k = 0; k < BM_EVENTS; k++) {
        const bm_event *ev = &c->pat->ev[i][s][k];
        if (bm_event_empty(ev)) continue;
        if (ev->prob == BM_PROB_FILL) { if (!fill) continue; }
        else if (ev->prob == BM_PROB_NOFILL) { if (fill) continue; }
        else if (!c->fill && ev->prob < 100 && (bm_xorshift32(&c->rng) % 100u) >= ev->prob) continue;
        if (tr->mute) continue;
        for (int j = 0; j < 4; j++)
            if (ev->lk[j] != BM_LOCK_NONE) e->vt->set_macro(e, (uint8_t)(BM_MACRO_LOCK | j), ev->lk[j]);
        int legato = c->gate[i][k].ticks && c->gate[i][k].tie;
        uint8_t prev = c->gate[i][k].note;
        if (legato) e->vt->set_macro(e, BM_MACRO_LEGATO, 1);
        e->vt->note_on(e, ev->note, ev->vel);
        if (c->gate[i][k].ticks && prev != ev->note) e->vt->note_off(e, prev);
        int l = ev->len & BM_LEN_MASK;
        if (l < 1) l = 1;
        c->gate[i][k].note = ev->note;
        c->gate[i][k].tie = (ev->len & BM_LEN_TIE) ? 1 : 0;
        c->gate[i][k].ticks = (uint16_t)(c->gate[i][k].tie ? l * tps + tps / 4 : l * tps - 4);
        fired = 1;
    }
    if (fired) c->st.trig[t]++;
}

/* ------------------------------------------------------------ loopera */
static void loop_release(bm_ctx *c, int i)
{
    bm_looper *L = &c->lp[i];
    bm_engine *e = &c->eng[i + BM_TR_TRACKS];
    for (int k = 0; k < L->nheld; k++) e->vt->note_off(e, L->held[k]);
    L->nheld = 0;
}

static uint32_t loop_len(const bm_ctx *c, int i)
{
    int b = c->pat->loop[i].bars;
    return (uint32_t)(b ? b : 2) * BM_BAR_TICKS;
}

static void looper_tick(bm_ctx *c)
{
    for (int i = 0; i < BM_SYN_TRACKS; i++) {
        bm_looper *L = &c->lp[i];
        bm_loop *d = &c->pat->loop[i];
        bm_engine *e = &c->eng[i + BM_TR_TRACKS];
        if (L->mode == LOOP_ARMED && c->btick % BM_BAR_TICKS == 0) {
            L->mode = LOOP_REC; L->start = c->btick; d->n = 0; d->on = 1; c->st.edits++;
        }
        c->st.loop_mode[i] = L->mode;
        if (L->mode < LOOP_REC) continue;
        uint32_t len = loop_len(c, i);
        uint32_t pos = (c->btick - L->start) % len;
        c->st.loop_pos[i] = (uint16_t)pos;
        if (pos == 0 && c->btick != L->start) {
            loop_release(c, i);
            if (L->mode == LOOP_REC) L->mode = LOOP_PLAY;
        }
        if ((L->mode == LOOP_PLAY || L->mode == LOOP_DUB) && d->on && !c->pat->trk[i + BM_TR_TRACKS].mute)
            for (int k = 0; k < d->n; k++) {
                const bm_loopev *ev = &d->ev[k];
                if (ev->tick != pos) continue;
                if (ev->vel) {
                    e->vt->note_on(e, ev->note, ev->vel);
                    if (L->nheld < 8) L->held[L->nheld++] = ev->note;
                } else {
                    e->vt->note_off(e, ev->note);
                    for (int h = 0; h < L->nheld; h++)
                        if (L->held[h] == ev->note) { L->held[h] = L->held[--L->nheld]; break; }
                }
            }
    }
}

static void loop_record(bm_ctx *c, int i, uint8_t note, uint8_t vel)
{
    bm_looper *L = &c->lp[i];
    bm_loop *d = &c->pat->loop[i];
    if ((L->mode != LOOP_REC && L->mode != LOOP_DUB) || !c->playing) return;
    if (d->n >= BM_LOOP_EVENTS) return;
    uint32_t pos = (c->btick - L->start) % loop_len(c, i);
    d->ev[d->n].tick = (uint16_t)pos; d->ev[d->n].note = note; d->ev[d->n].vel = vel;
    d->n++;
    c->st.edits++;
}

void bm_loop_cmd(bm_ctx *c, int t, int action, int val)
{
    int i = t - BM_TR_TRACKS;
    if (i < 0 || i >= BM_SYN_TRACKS) return;
    bm_looper *L = &c->lp[i];
    bm_loop *d = &c->pat->loop[i];
    switch (action) {
    case 1:
        if (L->mode == LOOP_OFF) {
            if (d->n && d->on) { L->mode = LOOP_PLAY; L->start = c->btick - (c->btick % BM_BAR_TICKS); }
            else L->mode = LOOP_ARMED;
        } else if (L->mode == LOOP_ARMED) L->mode = LOOP_OFF;
        else if (L->mode == LOOP_REC) L->mode = LOOP_PLAY;
        else if (L->mode == LOOP_PLAY) L->mode = LOOP_DUB;
        else L->mode = LOOP_PLAY;
        break;
    case 2: loop_release(c, i); d->n = 0; d->on = 0; L->mode = LOOP_OFF; c->st.edits++; break;
    case 3: d->on = !d->on; if (!d->on) loop_release(c, i); c->st.edits++; break;
    case 4: if (val == 1 || val == 2 || val == 4) { d->bars = (uint8_t)val; c->st.edits++; } break;
    default: break;
    }
    c->st.loop_mode[i] = L->mode;
}

/* ------------------------------------------------------------ escenas, patrones y canción */
static void do_swap(bm_ctx *c, int to)
{
    if (to < 0 || to == c->cur_pat || !c->swap) return;
    syn_gates_off(c);
    if (c->swap(c->swap_user, c->cur_pat, to, c->pat) == 0) {
        c->cur_pat = (uint8_t)to;
        bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
        c->st.pattern = (uint8_t)to;
        for (int i = 0; i < BM_SYN_TRACKS; i++) {
            loop_release(c, i);
            const bm_loop *d = &c->pat->loop[i];
            c->lp[i].mode = (d->on && d->n) ? LOOP_PLAY : LOOP_OFF;
            c->lp[i].start = c->btick;
        }
    } else {
        c->st.swap_fail = 1;
    }
}

static void apply_scene(bm_ctx *c, int idx)
{
    if (!c->song || idx < 0 || idx >= BM_SCENES || !c->song->scene[idx].valid) return;
    const bm_scene *s = &c->song->scene[idx];
    do_swap(c, s->pattern);
    for (int t = 0; t < BM_TRACKS; t++) c->pat->trk[t].mute = (uint8_t)((s->trk_mute >> t) & 1);
    for (int t = 0; t < BM_TR_TRACKS; t++) c->pat->tr[t].mute = s->lane_mute[t];
    c->st.scene = (uint8_t)idx;
    c->st.edits++;
}

static void boundary(bm_ctx *c)
{
    if (c->scene_q >= 0) {
        apply_scene(c, c->scene_q);
        c->scene_q = -1;
        if (c->chain_n) {
            c->chain_pos = (uint8_t)((c->chain_pos + 1) % c->chain_n);
            c->scene_q = (int8_t)c->chain[c->chain_pos];
            c->st.chain_pos = c->chain_pos;
        }
        c->st.scene_q = c->scene_q < 0 ? 0xFF : (uint8_t)c->scene_q;
        return;
    }
    if (c->song_on && c->song && c->song->n) {
        if (++c->song_rep >= (c->song->rep[c->song_pos] ? c->song->rep[c->song_pos] : 1)) {
            c->song_rep = 0;
            c->song_pos = (uint8_t)((c->song_pos + 1) % c->song->n);
        }
        uint8_t e = c->song->pat[c->song_pos];
        if (e & 0x80) apply_scene(c, e & 3); else do_swap(c, e);
        c->st.song_pos = c->song_pos;
        return;
    }
    if (c->queued >= 0) {
        int to = c->queued;
        c->queued = -1;
        c->st.queued = 0xFF;
        do_swap(c, to);
    }
}

void bm_scene_cmd(bm_ctx *c, int idx)
{
    c->chain_n = 0; c->st.chain_n = 0;
    if (idx == 0xFF) { c->scene_q = -1; c->st.scene_q = 0xFF; return; }
    if (!c->playing) { apply_scene(c, idx); return; }
    c->scene_q = (int8_t)idx; c->st.scene_q = (uint8_t)idx;
}

void bm_chain_cmd(bm_ctx *c, const uint8_t *chain, int n)
{
    if (n < 1 || n > 8) return;
    memcpy(c->chain, chain, (size_t)n);
    c->chain_n = (uint8_t)n; c->chain_pos = 0;
    c->st.chain_n = (uint8_t)n; c->st.chain_pos = 0;
    if (!c->playing) { apply_scene(c, chain[0]); c->scene_q = n > 1 ? (int8_t)chain[1] : (int8_t)chain[0]; c->chain_pos = n > 1 ? 1 : 0; }
    else c->scene_q = (int8_t)chain[0];
    c->st.scene_q = (uint8_t)c->scene_q;
}

/* ------------------------------------------------------------ transporte */
void bm_seq_start(bm_ctx *c, int restart)
{
    if (c->playing && !restart) return;
    syn_gates_off(c);
    memset(c->rq, 0, sizeof c->rq);
    c->rtick = 0; c->btick = 0;
    c->rng = 0xB0BBAu ^ ((uint32_t)c->cur_pat * 7919u);   /* determinista desde PLAY */
    if (c->song_on && c->song && c->song->n) {
        c->song_pos = 0; c->song_rep = 0; c->st.song_pos = 0;
        uint8_t e = c->song->pat[0];
        if (e & 0x80) apply_scene(c, e & 3); else do_swap(c, e);
    }
    for (int i = 0; i < BM_SYN_TRACKS; i++) {
        loop_release(c, i);
        const bm_loop *d = &c->pat->loop[i];
        if (c->lp[i].mode != LOOP_ARMED) c->lp[i].mode = (d->on && d->n) ? LOOP_PLAY : LOOP_OFF;
        c->lp[i].start = 0;
    }
    c->playing = 1;
    c->acc_q16 = c->spt_q16;          /* el paso 1 suena en la próxima muestra */
    c->st.playing = 1;
}

void bm_seq_stop(bm_ctx *c)
{
    c->playing = 0;
    syn_gates_off(c);
    memset(c->rq, 0, sizeof c->rq);
    for (int i = 0; i < BM_SYN_TRACKS; i++) {
        loop_release(c, i);
        if (c->lp[i].mode == LOOP_REC || c->lp[i].mode == LOOP_DUB) c->lp[i].mode = LOOP_PLAY;
        if (c->lp[i].mode == LOOP_PLAY) c->lp[i].mode = LOOP_OFF;
        c->st.loop_mode[i] = c->lp[i].mode;
    }
    c->st.playing = 0;
    c->st.step = -1;
}

/* ------------------------------------------------------------ arpegiador y roll */
static bool arp_active(const bm_ctx *c, int t)
{
    return c->arp[t].on || c->pat->trk[t].arp_mode != 0;
}

static void arp_tick(bm_ctx *c)
{
    for (int t = BM_TR_TRACKS; t < BM_TRACKS; t++) {
        bm_arp *a = &c->arp[t];
        const bm_track *tr = &c->pat->trk[t];
        bm_engine *e = &c->eng[t];
        if (a->playing && a->gate_left && --a->gate_left == 0) { e->vt->note_off(e, a->cur); a->playing = 0; }
        if (!arp_active(c, t) || a->n == 0) continue;
        uint8_t rate = ARP_RATE[tr->arp_rate % 3];
        if (c->st.free_ticks % rate) continue;
        if (a->playing) { e->vt->note_off(e, a->cur); a->playing = 0; }
        int oct = tr->arp_oct < 1 ? 1 : (tr->arp_oct > 3 ? 3 : tr->arp_oct);
        int total = a->n * oct;
        int mode = tr->arp_mode ? tr->arp_mode : 1;
        int i;
        if (mode == 4) i = (int)(bm_xorshift32(&c->rng) % (uint32_t)total);
        else if (mode == 2) { i = total - 1 - (a->idx % total); a->idx++; }
        else if (mode == 3 && total > 1) { int period = 2 * total - 2, p = a->idx % period; i = p < total ? p : period - p; a->idx++; }
        else { i = a->idx % total; a->idx++; }
        int note = a->held[i % a->n] + 12 * (i / a->n);
        a->cur = (uint8_t)(note > 127 ? 127 : note);
        e->vt->note_on(e, a->cur, 100);
        if (c->lp[t - BM_TR_TRACKS].mode == LOOP_REC || c->lp[t - BM_TR_TRACKS].mode == LOOP_DUB) loop_record(c, t - BM_TR_TRACKS, a->cur, 100);
        a->playing = 1;
        a->gate_left = (uint16_t)(rate * (tr->arp_gate ? tr->arp_gate : 50) / 100);
        if (!a->gate_left) a->gate_left = 1;
    }
}

static void roll_tick(bm_ctx *c)
{
    for (int t = 0; t < BM_TR_TRACKS; t++) {
        uint16_t m = c->roll[t].mask;
        uint8_t rate = c->roll[t].rate;
        if (!m || !rate || c->st.free_ticks % rate) continue;
        int tps = bm_track_tps(c, t);
        for (int l = 0; l < BM_LANES; l++) {
            if (!(m & (1u << l))) continue;
            c->eng[t].vt->note_on(&c->eng[t], (uint8_t)l, 100);
            c->st.lane_trig[t] |= (uint16_t)(1u << l);
            if (c->rec && c->playing) {               /* se graba como pasos con repetición */
                bm_tr *tr = &c->pat->tr[t];
                int len = tr->lane[l].len ? tr->lane[l].len : 16;
                int s = (int)(((c->rtick + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)len);
                uint8_t b = (uint8_t)(tr->st[l][s] | ST_ON);
                if (rate < tps) {
                    int rat = tps / rate; if (rat > 4) rat = 4;
                    int cur = ((b & ST_RAT_MASK) >> ST_RAT_SH) + 1;
                    if (rat > cur) b = (uint8_t)((b & ~ST_RAT_MASK) | ((rat - 1) << ST_RAT_SH));
                }
                tr->st[l][s] = b;
                c->st.edits++;
            }
        }
    }
}

/* ------------------------------------------------------------ strum */
void bm_seq_strum_block(bm_ctx *c, uint32_t frames)
{
    for (int i = 0; i < 8; i++) {
        bm_strum *s = &c->strum[i];
        if (!s->vel) continue;
        if (s->wait > frames) { s->wait = (uint16_t)(s->wait - frames); continue; }
        uint8_t v = s->vel; s->vel = 0;
        bm_live_note(c, s->track, s->note, v, 1);
    }
}

void bm_strum_cmd(bm_ctx *c, int t, const uint8_t *notes, int n, uint8_t vel)
{
    uint32_t gap = (uint32_t)c->pat->meta.strum * BM_FS / 1000u;
    for (int k = 0; k < n; k++) {
        if (k == 0 || !gap) { bm_live_note(c, t, notes[k], vel, 1); continue; }
        for (int i = 0; i < 8; i++)
            if (!c->strum[i].vel) { c->strum[i].track = (uint8_t)t; c->strum[i].note = notes[k]; c->strum[i].vel = vel; c->strum[i].wait = (uint16_t)(gap * (uint32_t)k); break; }
    }
}

/* ------------------------------------------------------------ notas en vivo */
void bm_live_note(bm_ctx *c, int t, uint8_t note, uint8_t vel, int on)
{
    if (t < 0 || t >= BM_TRACKS) return;
    bm_engine *e = &c->eng[t];
    if (!on)
        for (int i = 0; i < 8; i++)
            if (c->strum[i].vel && c->strum[i].track == t && c->strum[i].note == note) c->strum[i].vel = 0;
    if (t >= BM_TR_TRACKS && arp_active(c, t)) {
        bm_arp *a = &c->arp[t];
        if (on) {
            for (int i = 0; i < a->n; i++) if (a->held[i] == note) return;
            if (a->n >= 8) return;
            int i = a->n++;
            while (i > 0 && a->held[i - 1] > note) { a->held[i] = a->held[i - 1]; i--; }
            a->held[i] = note;
            if (a->n == 1) a->idx = 0;
        } else {
            for (int i = 0; i < a->n; i++)
                if (a->held[i] == note) { memmove(&a->held[i], &a->held[i + 1], (size_t)(a->n - i - 1)); a->n--; break; }
            if (a->n == 0 && a->playing) { e->vt->note_off(e, a->cur); a->playing = 0; }
        }
        return;
    }
    if (t >= BM_TR_TRACKS) loop_record(c, t - BM_TR_TRACKS, note, on ? vel : 0);
    if (on) { e->vt->note_on(e, note, vel); if (t < BM_TR_TRACKS) c->st.lane_trig[t] |= (uint16_t)(1u << (note & 15)); }
    else e->vt->note_off(e, note);
}

/* ------------------------------------------------------------ tick */
void bm_seq_tick(bm_ctx *c)
{
    c->st.free_ticks++;
    arp_tick(c);
    roll_tick(c);
    bm_dsp_tick(c);
    if (!c->playing) return;
    for (int i = 0; i < BM_SYN_TRACKS; i++)
        for (int k = 0; k < BM_EVENTS; k++)
            if (c->gate[i][k].ticks && --c->gate[i][k].ticks == 0) {
                bm_engine *e = &c->eng[i + BM_TR_TRACKS];
                e->vt->note_off(e, c->gate[i][k].note);
                c->gate[i][k].tie = 0;
            }
    rq_tick(c);
    bool fill = fill_now(c);
    c->st.fill = fill;
    for (int t = 0; t < BM_TRACKS; t++) {
        int tps = bm_track_tps(c, t);
        uint32_t n = c->rtick / (uint32_t)tps;
        uint32_t r = c->rtick - n * (uint32_t)tps;
        uint32_t sw = (uint32_t)((2 * tps * c->pat->meta.swing) / 100 - tps);
        c->st.tn[t] = (uint16_t)n;
        if (r != ((n & 1) ? sw : 0)) continue;
        if (bm_is_tr(t)) fire_tr(c, t, n, tps, fill);
        else fire_syn(c, t, n, tps, fill);
        if (t == 0) c->st.step = (int8_t)(n % 64);
    }
    looper_tick(c);
    c->rtick++; c->btick++;
    c->st.btick = c->btick;
    int master = bm_master_steps(&c->pat->meta) * BM_TICKS_STEP;
    if (master && c->rtick >= (uint32_t)master) {
        c->rtick = 0;
        c->st.loops++;
        boundary(c);
    } else if (!master && c->btick % BM_BAR_TICKS == 0) {
        c->st.loops++;
        boundary(c);
    }
    c->st.rtick = c->rtick;
}
