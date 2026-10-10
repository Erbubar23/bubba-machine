/* Bubba Machine — secuenciador: reloj de 96 PPQN, swing, probabilidad, gates,
 * TIE/glide, cola de patrones, song mode y arpegiador (SPEC_V3 §4.3). */
#include <string.h>
#include "bm_audio.h"
#include "bm_util.h"

static const uint8_t ARP_RATE[3] = {48, 24, 12};   /* 1/8, 1/16, 1/32 */

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

static uint16_t step_tick(const bm_ctx *c, int s)
{
    int t = s * BM_TICKS_STEP;
    if (s & 1) t += (48 * c->pat->meta.swing) / 100 - 24;   /* swing en los pasos pares (1-based) */
    return (uint16_t)t;
}

static void gates_off(bm_ctx *c)
{
    for (int t = 0; t < BM_TRACKS; t++)
        for (int e = 0; e < BM_EVENTS; e++)
            if (c->gate[t][e].ticks) {
                c->eng[t].vt->note_off(&c->eng[t], c->gate[t][e].note);
                c->gate[t][e].ticks = 0; c->gate[t][e].tie = 0;
            }
}

static void fire_step(bm_ctx *c, int s)
{
    for (int t = 0; t < BM_TRACKS; t++) {
        const bm_track *tr = &c->pat->trk[t];
        bm_engine *e = &c->eng[t];
        int fired = 0;
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *ev = &c->pat->ev[t][s][k];
            if (bm_event_empty(ev)) continue;
            if (!c->fill && ev->prob < 100 && (bm_xorshift32(&c->rng) % 100u) >= ev->prob) continue;
            if (tr->mute) continue;
            for (int i = 0; i < 4; i++)
                if (ev->lk[i] != BM_LOCK_NONE) e->vt->set_macro(e, (uint8_t)(BM_MACRO_LOCK | i), ev->lk[i]);
            int legato = c->gate[t][k].ticks && c->gate[t][k].tie;
            uint8_t prev = c->gate[t][k].note;
            if (legato) e->vt->set_macro(e, BM_MACRO_LEGATO, 1);
            e->vt->note_on(e, ev->note, ev->vel);
            if (legato && prev != ev->note) e->vt->note_off(e, prev);
            else if (!legato && c->gate[t][k].ticks && prev != ev->note) e->vt->note_off(e, prev);
            int len = ev->len & BM_LEN_MASK;
            if (len < 1) len = 1;
            c->gate[t][k].note = ev->note;
            c->gate[t][k].tie = (ev->len & BM_LEN_TIE) ? 1 : 0;
            c->gate[t][k].ticks = (uint16_t)(c->gate[t][k].tie ? len * BM_TICKS_STEP + 6 : len * BM_TICKS_STEP - 4);
            fired = 1;
        }
        if (fired) c->st.trig[t]++;
    }
}

static void do_swap(bm_ctx *c, int to)
{
    if (to < 0 || to == c->cur_pat || !c->swap) return;
    gates_off(c);
    if (c->swap(c->swap_user, c->cur_pat, to, c->pat) == 0) {
        c->cur_pat = (uint8_t)to;
        bm_seq_set_bpm(c, c->pat->meta.bpm_x10);
        c->st.pattern = (uint8_t)to;
    } else {
        c->st.swap_fail = 1;
    }
}

static void pattern_end(bm_ctx *c)
{
    int to = -1;
    if (c->song_on && c->song && c->song->n) {
        if (++c->song_rep >= (c->song->rep[c->song_pos] ? c->song->rep[c->song_pos] : 1)) {
            c->song_rep = 0;
            c->song_pos = (uint8_t)((c->song_pos + 1) % c->song->n);
        }
        to = c->song->pat[c->song_pos];
        c->st.song_pos = c->song_pos;
    } else if (c->queued >= 0) {
        to = c->queued;
        c->queued = -1;
        c->st.queued = 0xFF;
    }
    do_swap(c, to);
}

void bm_seq_start(bm_ctx *c, int restart)
{
    if (c->playing && !restart) return;
    gates_off(c);
    c->tick = 0; c->next_step = 0;
    c->rng = 0xB0BBAu ^ ((uint32_t)c->cur_pat * 7919u);   /* determinista desde PLAY */
    if (c->song_on && c->song && c->song->n) {
        c->song_pos = 0; c->song_rep = 0; c->st.song_pos = 0;
        do_swap(c, c->song->pat[0]);
    }
    c->playing = 1;
    c->acc_q16 = c->spt_q16;          /* el paso 1 suena en la próxima muestra */
    c->st.playing = 1;
}

void bm_seq_stop(bm_ctx *c)
{
    c->playing = 0;
    gates_off(c);
    c->st.playing = 0;
    c->st.step = -1;
}

/* ------------------------------------------------------------ arpegiador */
static bool arp_active(const bm_ctx *c, int t)
{
    return c->arp[t].on || c->pat->trk[t].arp_mode != 0;
}

static void arp_tick(bm_ctx *c)
{
    for (int t = 2; t < BM_TRACKS; t++) {
        bm_arp *a = &c->arp[t];
        const bm_track *tr = &c->pat->trk[t];
        bm_engine *e = &c->eng[t];
        if (a->playing && a->gate_left && --a->gate_left == 0) {
            e->vt->note_off(e, a->cur); a->playing = 0;
        }
        if (!arp_active(c, t) || a->n == 0) continue;
        uint8_t rate = ARP_RATE[tr->arp_rate % 3];
        if (c->st.free_ticks % rate) continue;
        if (a->playing) { e->vt->note_off(e, a->cur); a->playing = 0; }
        int oct = tr->arp_oct < 1 ? 1 : (tr->arp_oct > 3 ? 3 : tr->arp_oct);
        int total = a->n * oct;
        int mode = tr->arp_mode ? tr->arp_mode : 1;
        int i;
        if (mode == 4) {
            i = (int)(bm_xorshift32(&c->rng) % (uint32_t)total);
        } else if (mode == 2) {
            i = total - 1 - (a->idx % total); a->idx++;
        } else if (mode == 3 && total > 1) {
            int period = 2 * total - 2, p = a->idx % period;
            i = p < total ? p : period - p; a->idx++;
        } else {
            i = a->idx % total; a->idx++;
        }
        int note = a->held[i % a->n] + 12 * (i / a->n);
        if (note > 127) note = 127;
        a->cur = (uint8_t)note;
        e->vt->note_on(e, a->cur, 100);
        a->playing = 1;
        a->gate_left = (uint16_t)(rate * (tr->arp_gate ? tr->arp_gate : 50) / 100);
        if (!a->gate_left) a->gate_left = 1;
    }
}

void bm_live_note(bm_ctx *c, int t, uint8_t note, uint8_t vel, int on)
{
    if (t < 0 || t >= BM_TRACKS) return;
    bm_engine *e = &c->eng[t];
    if (t >= 2 && arp_active(c, t)) {
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
                if (a->held[i] == note) {
                    memmove(&a->held[i], &a->held[i + 1], (size_t)(a->n - i - 1));
                    a->n--;
                    break;
                }
            if (a->n == 0 && a->playing) { e->vt->note_off(e, a->cur); a->playing = 0; }
        }
        return;
    }
    if (on) e->vt->note_on(e, note, vel);
    else e->vt->note_off(e, note);
}

void bm_seq_tick(bm_ctx *c)
{
    c->st.free_ticks++;
    arp_tick(c);
    bm_dsp_tick(c);
    if (!c->playing) return;
    for (int t = 0; t < BM_TRACKS; t++)
        for (int k = 0; k < BM_EVENTS; k++)
            if (c->gate[t][k].ticks && --c->gate[t][k].ticks == 0) {
                c->eng[t].vt->note_off(&c->eng[t], c->gate[t][k].note);
                c->gate[t][k].tie = 0;
            }
    int len = c->pat->meta.length;
    while (c->next_step < len && step_tick(c, c->next_step) == c->tick) {
        fire_step(c, c->next_step);
        c->st.step = (int8_t)c->next_step;
        c->next_step++;
    }
    c->tick++;
    if (c->tick >= len * BM_TICKS_STEP) {
        c->tick = 0; c->next_step = 0;
        c->st.loops++;
        pattern_end(c);
    }
    c->st.tick = c->tick;
}
