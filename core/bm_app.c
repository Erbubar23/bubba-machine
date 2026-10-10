/* Bubba Machine — aplicación: lógica de la interfaz (DISENO_0.2). */
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "bm_app.h"
#include "bm_util.h"
#include "bm_rom.h"
#include "bm_sample.h"

#define TRK_OFF(f)  ((uint8_t)offsetof(bm_track, f))
#define META_OFF(f) ((uint8_t)offsetof(bm_meta, f))

static const char *const TRACK_NAMES[BM_TRACKS] = {"DRUMS", "SAMPLER", "BASS", "SYN A", "SYN B", "SYN C"};
static const uint16_t TRACK_COLORS[BM_TRACKS] = {C_ORANGE, C_YELLOW, C_GREEN, C_BLUE, C_VIOLET, C_PINK};
static const uint16_t KNOB_COLORS[4] = {C_BLUE, C_GREEN, C_YELLOW, C_ORANGE};
static const char *const MAC_SYN[4] = {"CUTOFF", "RESO", "DECAY", "WAVE"};
static const char *const MAC_TR[4] = {"TONE", "TUNE", "DECAY", "START"};
static const char *const DLY_NAMES[4] = {"1/16", "1/8", "3/16", "1/4"};
static const char *const EXT_NAMES[5] = {"TRIADA", "+7", "+9", "SUS2", "SUS4"};
static const char *const ARP_MODES[5] = {"OFF", "UP", "DOWN", "UP-DN", "RAND"};
static const char *const ARP_RATES[3] = {"1/8", "1/16", "1/32"};
static const char *const VOICINGS[3] = {"CERRADO", "ABIERTO", "CONDUCIDO"};
static const char *const RAT_NAMES[5] = {"X1", "X2", "X3", "X4", "FLAM"};
static const char *const COND_NAMES[3] = {"SIEMPRE", "SOLO FILL", "SIN FILL"};
static const uint8_t MASTER_STEPS[6] = {1, 2, 3, 4, 8, 0};
static const uint8_t ROLL_TICKS[5] = {48, 24, 12, 32, 16};
const char *const bm_roll_names[5] = {"1/8", "1/16", "1/32", "1/8T", "1/16T"};
static const uint8_t CHOPS[4] = {0, 4, 8, 16};

const char *bm_track_name(int t) { return TRACK_NAMES[t % BM_TRACKS]; }
uint16_t bm_track_color(int t) { return TRACK_COLORS[t % BM_TRACKS]; }
uint16_t bm_knob_color(int i) { return KNOB_COLORS[i & 3]; }
const char *bm_macro_name(int t, int i) { return bm_is_tr(t) ? MAC_TR[i & 3] : MAC_SYN[i & 3]; }

/* ------------------------------------------------------------ comandos */
static void push(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.b = y; c.c = z;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_i(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z, int32_t i)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.b = y; c.c = z; c.u.i = i;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_ptr(bm_app *a, uint8_t type, uint8_t x, const void *p)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.u.ptr = p;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_ev(bm_app *a, int t, int s, int k, const bm_event *e)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = CMD_SET_EVENT; c.a = (uint8_t)t; c.b = (uint8_t)s; c.c = (uint8_t)k; c.u.ev = *e;
    bm_cmd_push(&a->cmdq, &c);
}

static void edited(bm_app *a) { a->dirty = 1; a->t_edit = a->now; }

static void push_trstep(bm_app *a, int t, int lane, int s, uint8_t v) { push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v); edited(a); }
static void push_trlock(bm_app *a, int t, int lane, int s, int param, int v)
{
    push_i(a, CMD_TR_LOCK, (uint8_t)t, (uint8_t)lane, (uint8_t)s, v < 0 ? (0x10000 | (param << 8)) : ((param << 8) | v));
    edited(a);
}
static void push_lane(bm_app *a, int t, int lane, int field, int v) { push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)lane, (uint8_t)field, v); edited(a); }

static void set_track(bm_app *a, int t, uint8_t off, int v) { push(a, CMD_SET_TRACK, (uint8_t)t, off, (uint8_t)v); edited(a); }
static void set_meta(bm_app *a, uint8_t off, int v) { push(a, CMD_SET_META, 0, off, (uint8_t)v); edited(a); }

static void toast(bm_app *a, const char *msg)
{
    snprintf(a->toast, sizeof a->toast, "%s", msg);
    a->toast_until = a->now + UI_TOAST_MS;
}

static void overlay(bm_app *a, int enc, const char *title, const char *val, const char *unit, int pct, int ghost, uint16_t col)
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

int bm_app_eff_view(const bm_app *a) { return a->layer ? a->layer : a->view; }
static int playing(const bm_app *a) { return a->ctx.st.playing; }
static int tr_of(const bm_app *a) { return bm_is_tr(a->track) ? a->track : 0; }

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

void bm_fmt_macro(const bm_app *a, int t, int i, int v, char *b, int cap)
{
    if (bm_is_tr(t)) {
        if (i == 1) snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 12) / 64);
        else fmt_pct(v, b, cap);
        return;
    }
    switch (i) {
    case 0: fmt_hz(bm_cutoff_hz(v), b, cap); break;
    case 1: fmt_pct(v, b, cap); break;
    case 2: fmt_ms(bm_decay_ms(v), b, cap); break;
    default: {
        int p = v * 3 * 256 / 127;
        int w = (a->pat.trk[t].source + (p >> 8)) % BM_WAVES;
        snprintf(b, (size_t)cap, "%s", bm_rom_wave_names[w]);
    }
    }
}

const char *bm_lane_name(const bm_app *a, int t, int lane)
{
    static char buf[2][8];
    static int k;
    if (t == 0) return bm_rom_kits[a->pat.trk[0].source % BM_KITS][lane & 15].name;
    k ^= 1;
    snprintf(buf[k], sizeof buf[k], "CHOP %d", (lane & 15) + 1);
    return buf[k];
}

int bm_app_track_step(const bm_app *a, int t, int lane)
{
    if (!playing(a)) return -1;
    uint32_t n = a->ctx.st.tn[t];
    int len = (lane >= 0 && bm_is_tr(t)) ? a->pat.tr[t].lane[lane].len : a->pat.trk[t].length;
    return (int)(n % (uint32_t)(len ? len : 16));
}

int bm_app_chord_track(const bm_app *a) { return a->track >= BM_TR_TRACKS ? a->track : 3; }

static int syn_step_events(const bm_app *a, int t, int s, int idx[4])
{
    int n = 0;
    for (int k = 0; k < BM_EVENTS; k++)
        if (!bm_event_empty(&a->pat.ev[t - BM_TR_TRACKS][s][k])) idx[n++] = k;
    return n;
}

static bool lock_mode(const bm_app *a) { return a->held_step >= 0 && a->held_long; }

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
    fmt_master(m->master, b, sizeof b); L4(L, 2, "MAESTRO", b, m->master ? m->master * 31 : 255);
    fmt_pct(a->set.drive, b, sizeof b); L4(L, 3, "DRIVE", b, a->set.drive * 2);
}

void bm_app_legend(const bm_app *a, bm_legend L[4])
{
    memset(L, 0, sizeof(bm_legend) * 4);
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    int v = bm_app_eff_view(a);
    if (a->glo_down || v == V_GLO) { legend_global(a, L); return; }
    if (lock_mode(a) && v == V_SEQ) {
        for (int i = 0; i < 4; i++) {
            int val = tr->macro[i];
            if (bm_is_tr(t)) {
                int lv = bm_tr_lock_get(&a->pat.tr[t], a->held_step, a->sel_lane[t], i);
                if (lv >= 0) { val = lv; L[i].locked = 1; }
            } else {
                int idx[4];
                int n = syn_step_events(a, t, a->held_step, idx);
                if (n && a->pat.ev[t - 2][a->held_step][idx[0]].lk[i] != BM_LOCK_NONE) { val = a->pat.ev[t - 2][a->held_step][idx[0]].lk[i]; L[i].locked = 1; }
            }
            bm_fmt_macro(a, t, i, val, b, sizeof b);
            L4(L, i, bm_macro_name(t, i), b, val * 2);
        }
        return;
    }
    switch (v) {
    case V_HOME: case V_MUTE:
        snprintf(b, sizeof b, "%d", tr->level); L4(L, 0, "NIVEL", b, tr->level * 2);
        fmt_pan(tr->pan, b, sizeof b); L4(L, 1, "PAN", b, tr->pan * 2);
        fmt_pct(tr->send, b, sizeof b); L4(L, 2, "ENVIO", b, tr->send * 2);
        if (t >= 2) { fmt_pct(tr->duck, b, sizeof b); L4(L, 3, "DUCK", b, tr->duck * 2); } else L4(L, 3, "DUCK", t == 0 ? "FUENTE" : "-", 0);
        return;
    case V_INST: {
        const bm_lane *ln = &a->pat.tr[tr_of(a)].lane[a->sel_lane[tr_of(a)]];
        snprintf(b, sizeof b, "%d", ln->level); L4(L, 0, "NIVEL", b, ln->level * 2);
        snprintf(b, sizeof b, "%+d ST", ((ln->tune - 64) * 12) / 64); L4(L, 1, "AFINA", b, ln->tune * 2);
        snprintf(b, sizeof b, "%d%%", ln->decay * 100 / 127); L4(L, 2, "DECAY", b, ln->decay * 2);
        snprintf(b, sizeof b, "%d", ln->len); L4(L, 3, "LARGO", b, ln->len * 4);
        return;
    }
    case V_TIME: {
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        if (bm_is_tr(row)) {
            const bm_lane *ln = &a->pat.tr[row].lane[a->time_lane];
            snprintf(b, sizeof b, "%d", a->pat.tr[row].lane[0].len); L4(L, 0, "LARGO", b, a->pat.tr[row].lane[0].len * 4);
            L4(L, 1, "VELOCID", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
            L4(L, 2, "INSTRUM", bm_lane_name(a, row, a->time_lane), a->time_lane * 17);
            snprintf(b, sizeof b, "%d", ln->len); L4(L, 3, "SU LARGO", b, ln->len * 4);
        } else {
            snprintf(b, sizeof b, "%d", rt->length); L4(L, 0, "LARGO", b, rt->length * 4);
            L4(L, 1, "VELOCID", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
        }
        return;
    }
    case V_ENV:
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
    case V_FX: case V_PUNCH:
        L4(L, 0, "DLY TIME", DLY_NAMES[m->dly_time & 3], (m->dly_time & 3) * 85);
        fmt_pct(m->dly_fb, b, sizeof b); L4(L, 1, "FEEDBACK", b, m->dly_fb * 2);
        fmt_pct(m->dly_mix, b, sizeof b); L4(L, 2, "DLY MIX", b, m->dly_mix * 2);
        fmt_ms(bm_decay_ms(m->sc_release / 2), b, sizeof b); L4(L, 3, "SC REL", b, m->sc_release * 2);
        return;
    case V_SCL:
        L4(L, 0, "TONICA", bm_note_names[m->root % 12], m->root * 23);
        snprintf(b, sizeof b, "%.8s", bm_scale_names[m->scale % BM_SCALES]); L4(L, 1, "ESCALA", b, m->scale * 31);
        L4(L, 2, "ACORDE", EXT_NAMES[m->voicing % 5], m->voicing * 63);
        L4(L, 3, "LOCK", m->scale_lock ? "ON" : "OFF", m->scale_lock ? 255 : 0);
        return;
    case V_CHORDS: case V_CHORDLIVE:
        snprintf(b, sizeof b, "%.8s", bm_chord_set_names[m->chord_set % BM_CHORD_SETS]); L4(L, 0, "JUEGO", b, m->chord_set * 127);
        snprintf(b, sizeof b, "%.8s", VOICINGS[m->chord_voicing % 3]); L4(L, 1, "VOICING", b, m->chord_voicing * 127);
        snprintf(b, sizeof b, "%d", m->chord_oct); L4(L, 2, "OCTAVA", b, (m->chord_oct - 1) * 51);
        snprintf(b, sizeof b, "%dMS", m->strum); L4(L, 3, "STRUM", b, m->strum * 4);
        return;
    case V_ARP: case V_ARPLIVE:
        L4(L, 0, "MODO", ARP_MODES[tr->arp_mode % 5], tr->arp_mode * 63);
        L4(L, 1, "RATE", ARP_RATES[tr->arp_rate % 3], tr->arp_rate * 127);
        snprintf(b, sizeof b, "%d", tr->arp_oct); L4(L, 2, "OCTAVAS", b, tr->arp_oct * 85);
        snprintf(b, sizeof b, "%d%%", tr->arp_gate); L4(L, 3, "GATE", b, tr->arp_gate * 255 / 100);
        return;
    case V_ROLL:
        L4(L, 0, "RATE", bm_roll_names[a->roll_rate % 5], a->roll_rate * 63);
        return;
    case V_LOOP: {
        const bm_loop *lp = &a->pat.loop[t >= 2 ? t - 2 : 0];
        L4(L, 0, "LOOP", lp->on ? "ON" : "OFF", lp->on ? 255 : 0);
        snprintf(b, sizeof b, "%d", lp->bars); L4(L, 1, "COMPASES", b, lp->bars * 63);
        return;
    }
    case V_SAMPLER:
        L4(L, 0, "SLOT", a->pat.trk[1].source % 2 ? "B" : "A", (a->pat.trk[1].source % 2) * 255);
        if (a->chops_mode) snprintf(b, sizeof b, "%d", CHOPS[a->chops_mode]); else snprintf(b, sizeof b, "MANUAL");
        L4(L, 1, "CHOPS", b, a->chops_mode * 85);
        return;
    case V_SYSTEM:
        L4(L, 0, "VALOR", "K1", 0);
        return;
    case V_SAVE:
        snprintf(b, sizeof b, "A%02d", a->save_target + 1); L4(L, 0, "DESTINO", b, a->save_target * 17);
        return;
    case V_SONG:
        if (a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            if (e & 0x80) snprintf(b, sizeof b, "ESC %c", 'A' + (e & 3)); else snprintf(b, sizeof b, "A%02d", e + 1);
            L4(L, 0, "PASO", b, 0);
            snprintf(b, sizeof b, "x%d", a->song.rep[a->song_cur]); L4(L, 1, "VECES", b, 0);
        }
        L4(L, 3, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? 255 : 0);
        return;
    case V_LFO: case V_BOOT: case V_SCENES: case V_BANK: case V_LANEMUTE: case V_ERASE:
        return;
    default:
        for (int i = 0; i < 4; i++) {
            bm_fmt_macro(a, t, i, tr->macro[i], b, sizeof b);
            L4(L, i, bm_macro_name(t, i), b, tr->macro[i] * 2);
        }
    }
}

/* ------------------------------------------------------------ almacenamiento */
static const uint8_t *store_ptr(bm_app *a, int obj, int *len)
{
    if (!a->pf.flash_map || obj < 0 || obj >= BM_OBJ_COUNT || a->store.live[obj] < 0) return NULL;
    *len = a->store.live_len[obj];
    return a->pf.flash_map(a->pf.user, BM_STORE_LO + (uint32_t)a->store.live[obj] * BM_SLOT_SIZE + 16u);
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
        if (!(src && len > 0 && bm_pattern_unpack(p, src, len) > 0))
            for (int t = 0; t < BM_TRACKS; t++) bm_track_clear_events(p, t);   /* vacío: hereda sonidos */
        a->dirty = 0;
    }
    a->set.last_pat = (uint8_t)to;
    a->set_dirty = 1;
    return 0;
}

static void compute_wave(bm_app *a, const int8_t *d, uint32_t len)
{
    memset(a->wave, 0, sizeof a->wave);
    if (!d || !len) return;
    for (int x = 0; x < 232; x++) {
        uint32_t s0 = len * (uint32_t)x / 232u, s1 = len * (uint32_t)(x + 1) / 232u;
        int pk = 0;
        for (uint32_t i = s0; i < s1; i += 4) { int v = d[i] < 0 ? -d[i] : d[i]; if (v > pk) pk = v; }
        a->wave[x] = (uint8_t)pk;
    }
}

static void send_smp_src(bm_app *a)
{
    int slot = a->pat.trk[1].source % BM_SMP_SLOTS;
    if (a->smp_valid[slot]) a->smp_send = a->smp_info[slot]; else memset(&a->smp_send, 0, sizeof a->smp_send);
    compute_wave(a, a->smp_send.data, a->smp_send.len);
    push_ptr(a, CMD_SMP_SRC, 0, &a->smp_send);
}

void bm_app_samples_reload(bm_app *a)
{
    for (int s = 0; s < BM_SMP_SLOTS; s++)
        a->smp_valid[s] = a->pf.flash && bm_sample_info(a->pf.flash, a->pf.flash_map, a->pf.user, s, &a->smp_info[s]) == 0;
    send_smp_src(a);
}

static void job_push(bm_app *a, int type, int arg)
{
    for (int i = 0; i < a->njob; i++) if (a->job[i] == type && a->job_arg[i] == arg) return;
    if (a->njob < BM_JOBS) { a->job[a->njob] = (uint8_t)type; a->job_arg[a->njob] = (uint8_t)arg; a->njob++; }
}

static void save_jobs(bm_app *a, int with_cur)
{
    if (a->pend_obj >= 0) job_push(a, JOB_FLUSH_PEND, 0);
    if (with_cur) job_push(a, JOB_SAVE_CUR, 0);
}

static int exec_job(bm_app *a, int type, int arg)
{
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
        int n = bm_pattern_pack(&a->pat, a->pend, BM_PENDING_LEN);
        if (n < 0) { toast(a, "PATRON LLENO: NO CABE"); return 0; }
        int obj = type == JOB_SAVE_AS ? arg : a->ctx.st.pattern;
        if (bm_store_write(&a->store, (uint8_t)obj, a->pend, (uint16_t)n)) return -1;
        if (type == JOB_SAVE_AS) { a->ctx.cur_pat = (uint8_t)obj; a->ctx.st.pattern = (uint8_t)obj; }
        a->dirty = 0;
        return 0;
    }
    case JOB_SAVE_SONG: {
        uint8_t b[1 + 2 * BM_SONG_MAX + sizeof a->song.scene];
        b[0] = a->song.n;
        memcpy(b + 1, a->song.pat, BM_SONG_MAX);
        memcpy(b + 1 + BM_SONG_MAX, a->song.rep, BM_SONG_MAX);
        memcpy(b + 1 + 2 * BM_SONG_MAX, a->song.scene, sizeof a->song.scene);
        if (bm_store_write(&a->store, BM_OBJ_SONG, b, sizeof b)) return -1;
        a->song_dirty = 0;
        return 0;
    }
    case JOB_SAVE_GLOB:
        a->set.ver = 2;
        if (bm_store_write_glob(&a->store, (const uint8_t *)&a->set, sizeof a->set)) return -1;
        a->set_dirty = 0;
        return 0;
    case JOB_LOAD:
        push(a, CMD_LOAD, (uint8_t)arg, 0, 0);
        return 0;
    case JOB_SAVE_SAMPLE: {
        const bm_smp_src *s = &a->ctx.smp;
        if (s->len && bm_sample_write(a->pf.flash, arg, (const int8_t *)a->amem.arena, s->len, s->marks, s->nmarks)) return -1;
        a->smp_valid[arg] = bm_sample_info(a->pf.flash, a->pf.flash_map, a->pf.user, arg, &a->smp_info[arg]) == 0;
        if (a->smp_valid[arg]) a->smp_send = a->smp_info[arg]; else memset(&a->smp_send, 0, sizeof a->smp_send);
        compute_wave(a, a->smp_send.data, a->smp_send.len);
        push_ptr(a, CMD_SMP_SAVED, 0, &a->smp_send);
        return 0;
    }
    case JOB_SAMPLE_MARKS: {
        if (!a->smp_valid[arg] || a->pend_obj >= 0) return -1;
        uint32_t marks[BM_SLICES];
        int n = CHOPS[a->chops_mode];
        for (int i = 0; i < n; i++) marks[i] = a->smp_info[arg].len * (uint32_t)i / (uint32_t)n;
        if (bm_sample_set_marks(a->pf.flash, arg, marks, n, a->pend)) return -1;
        bm_app_samples_reload(a);
        return 0;
    }
    case JOB_SAMPLE_ERASE:
        if (bm_sample_erase(a->pf.flash, arg)) return -1;
        bm_app_samples_reload(a);
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
        if (playing(a)) { toast(a, "PARA (STOP) PARA GUARDAR"); a->njob = 0; return; }
        if (!a->pf.flash) { a->njob = 0; return; }
        if (a->cmdq.r != a->cmdq.w) return;
        if (!a->job_fading) { push(a, CMD_FADE, 1, 0, 0); a->job_fading = 1; return; }
        if (!a->ctx.st.faded) return;
    }
    int err = 0;
    for (int i = 0; i < a->njob; i++) err |= exec_job(a, a->job[i], a->job_arg[i]);
    a->njob = 0;
    if (a->job_fading) { push(a, CMD_FADE, 0, 0, 0); a->job_fading = 0; }
    if (err) toast(a, "ERROR DE FLASH");
}

/* ------------------------------------------------------------ demo de primer arranque */
static void ev_set(bm_pattern *p, int t, int s, int k, int note, int vel, int len, int prob)
{
    bm_event *e = &p->ev[t - BM_TR_TRACKS][s][k];
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
    static const int prog[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}};   /* Am F C G */
    static const int bass[4] = {45, 41, 36, 43};
    for (int t = 2; t < BM_TRACKS; t++) p->trk[t].length = 64;
    for (int pg = 0; pg < 4; pg++) {
        int o = pg * 16, bn = bass[pg];
        ev_set(p, 2, o + 0, 0, bn, 110, 2, 100);
        ev_set(p, 2, o + 3, 0, bn, 90, 1, 100);
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
    p->trk[3].level = 70; p->trk[4].level = 60; p->trk[5].level = 55;
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

static void on_gesture(void *u, int id, bm_gkind k);

static void settings_defaults(bm_settings *s)
{
    memset(s, 0, sizeof *s);
    s->ver = 2; s->sync_ppqn = 2; s->drive = 25; s->led_dim = 1; s->autosave = 1;
}

static void apply_settings(bm_app *a)
{
    push(a, CMD_CLOCK, a->set.clock, 0, 0);
    push(a, CMD_SYNC, a->set.sync_mode, a->set.sync_ppqn, a->set.sync_lat);
    push(a, CMD_DRIVE, a->set.drive, 0, 0);
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
    bm_gesture_init(&a->ges, on_gesture, a);
    a->pend_obj = -1; a->undo_track = -1; a->redo_track = -1; a->held_step = -1; a->held_white = -1;
    a->pat_sel = -1; a->punch_key = -1; a->last_pad = -1; a->roll_rate = 1;
    static const uint8_t ln[BM_TRACKS] = {0, 0, 45, 57, 60, 64};
    memcpy(a->last_note, ln, sizeof ln);
    for (int k = 0; k < BM_KEYS; k++) a->key_rec_step[k] = -1;
    settings_defaults(&a->set);
    a->battery = 100;

    int fresh = 1;
    if (pf->flash && bm_store_mount(&a->store, pf->flash) == 0) {
        bm_settings s;
        if (bm_store_read_glob(&a->store, (uint8_t *)&s, sizeof s) == (int)sizeof s && s.ver == 2) a->set = s;
        uint8_t b[1 + 2 * BM_SONG_MAX + sizeof a->song.scene];
        if (bm_store_read(&a->store, BM_OBJ_SONG, b, sizeof b) == (int)sizeof b) {
            a->song.n = b[0] > BM_SONG_MAX ? BM_SONG_MAX : b[0];
            memcpy(a->song.pat, b + 1, BM_SONG_MAX);
            memcpy(a->song.rep, b + 1 + BM_SONG_MAX, BM_SONG_MAX);
            memcpy(a->song.scene, b + 1 + 2 * BM_SONG_MAX, sizeof a->song.scene);
        }
        for (int i = 0; i < BM_PATTERNS; i++) if (bm_store_has(&a->store, (uint8_t)i)) fresh = 0;
        int len = 0;
        const uint8_t *src = store_ptr(a, a->set.last_pat % BM_PATTERNS, &len);
        if (src && len > 0 && bm_pattern_unpack(&a->pat, src, len) < 0) bm_pattern_init(&a->pat);
    }
    if (fresh) { demo_pattern(&a->pat, &a->song); a->dirty = 1; a->song_dirty = 1; a->t_edit = now; }
    a->set.last_pat %= BM_PATTERNS;
    a->ctx.cur_pat = a->set.last_pat;
    a->ctx.st.pattern = a->set.last_pat;
    bm_seq_set_bpm(&a->ctx, a->pat.meta.bpm_x10);
    apply_settings(a);
    bm_app_samples_reload(a);
    a->view = V_BOOT;
    a->boot_t = now;
    a->scr.force = 1;
}

void bm_app_audio(bm_app *a, int32_t *out, uint32_t frames) { bm_audio_render(&a->ctx, out, frames); }
void bm_app_midi_in(bm_app *a, uint8_t b) { push(a, CMD_MIDI, b, 0, 0); }
void bm_app_battery(bm_app *a, uint8_t pct) { a->battery = pct > 100 ? 100 : pct; }

void bm_app_master(bm_app *a, uint16_t adc)
{
    a->master_adc = (uint16_t)((a->master_adc * 3u + adc) / 4u);
    int d = (int)a->master_adc - (int)a->master_sent;
    if (d > 4 || d < -4 || (adc == 0 && a->master_sent) || (adc >= 1023 && a->master_sent < 1023)) {
        a->master_sent = (adc == 0 || adc >= 1023) ? adc : a->master_adc;
        push_i(a, CMD_MASTER, 0, 0, 0, a->master_sent);
    }
}

/* ------------------------------------------------------------ acciones */
static void set_view(bm_app *a, int v)
{
    a->view = (uint8_t)v;
    a->ov.on = 0;
    a->clear_armed = 0; a->init_armed = 0; a->erase_slot_armed = 0;
}

static void *track_data(bm_app *a, int t) { return bm_is_tr(t) ? (void *)&a->pat.tr[t] : (void *)a->pat.ev[t - BM_TR_TRACKS]; }
static size_t track_size(int t) { return bm_is_tr(t) ? sizeof(bm_tr) : sizeof(bm_event) * BM_STEPS * BM_EVENTS; }

static void undo_snapshot(bm_app *a, int t)
{
    memcpy(&a->undo, track_data(a, t), track_size(t));
    a->undo_track = (int8_t)t;
    a->redo_track = -1;
}

static void do_undo(bm_app *a)
{
    if (a->undo_track < 0) { toast(a, "NADA QUE DESHACER"); return; }
    int t = a->undo_track;
    memcpy(&a->redo, track_data(a, t), track_size(t));
    a->redo_track = (int8_t)t;
    push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->undo);
    a->undo_track = -1;
    edited(a);
    toast(a, "DESHECHO");
}

static void do_redo(bm_app *a)
{
    if (a->redo_track < 0) { toast(a, "NADA QUE REHACER"); return; }
    int t = a->redo_track;
    memcpy(&a->undo, track_data(a, t), track_size(t));
    a->undo_track = (int8_t)t;
    push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->redo);
    a->redo_track = -1;
    edited(a);
    toast(a, "REHECHO");
}

static void clear_track(bm_app *a, int t)
{
    undo_snapshot(a, t);
    push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
    edited(a);
    char b[30];
    snprintf(b, sizeof b, "T%d BORRADA (EDIT+OCT- DESHACE)", t + 1);
    toast(a, b);
}

static void copy_track(bm_app *a)
{
    int t = a->track;
    memcpy(&a->clip, track_data(a, t), track_size(t));
    a->clip_type = 2; a->clip_track = (uint8_t)t;
    toast(a, "PISTA COPIADA");
}

static void paste_track(bm_app *a)
{
    int t = a->track;
    if (a->clip_type != 2 || bm_is_tr(t) != bm_is_tr(a->clip_track)) { toast(a, "NADA QUE PEGAR AQUI"); return; }
    undo_snapshot(a, t);
    push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, &a->clip);
    edited(a);
    toast(a, "PISTA PEGADA");
}

static void select_track(bm_app *a, int t)
{
    if (t < 0 || t >= BM_TRACKS) return;
    a->track = (uint8_t)t;
    if (a->view == V_LOOP && bm_is_tr(t)) a->view = V_SEQ;
    if (a->view == V_INST && !bm_is_tr(t)) a->view = V_EDIT;
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, TRACK_NAMES[t]);
    toast(a, b);
}

static void toggle_mute(bm_app *a, int t) { set_track(a, t, TRK_OFF(mute), a->pat.trk[t].mute ? 0 : 1); }

static void toggle_lane_mute(bm_app *a, int t, int lane)
{
    uint16_t m = (uint16_t)(a->pat.tr[t].mute ^ (1u << lane));
    push_i(a, CMD_LANE_MUTE, (uint8_t)t, 0, 0, m);
    edited(a);
}

static void solo(bm_app *a, int t)
{
    int only = 1;
    for (int i = 0; i < BM_TRACKS; i++) if ((i == t) == (a->pat.trk[i].mute != 0)) only = 0;
    for (int i = 0; i < BM_TRACKS; i++) set_track(a, i, TRK_OFF(mute), only ? 0 : (i != t));
    toast(a, only ? "SOLO OFF" : "SOLO");
}

static void select_pattern(bm_app *a, int idx)
{
    char b[30];
    idx = bm_clampi(idx, 0, BM_PATTERNS - 1);
    if (playing(a)) {
        if (idx == a->ctx.st.pattern) { push(a, CMD_QUEUE, 0xFF, 0, 0); return; }
        if (a->dirty && a->pend_obj >= 0 && a->pend_obj != a->ctx.st.pattern) { toast(a, "PARA (STOP) PARA GUARDAR"); return; }
        push(a, CMD_QUEUE, (uint8_t)idx, 0, 0);
        snprintf(b, sizeof b, "A%02d EN COLA", idx + 1);
    } else {
        if (idx == a->ctx.st.pattern) return;
        save_jobs(a, a->dirty);
        job_push(a, JOB_LOAD, idx);
        snprintf(b, sizeof b, "PATRON A%02d", idx + 1);
    }
    toast(a, b);
}

static void save_exec(bm_app *a)
{
    char b[30];
    switch (a->save_row) {
    case 0:
        save_jobs(a, 1);
        snprintf(b, sizeof b, "GUARDADO A%02d", a->ctx.st.pattern + 1);
        toast(a, playing(a) ? "PARA (STOP) PARA GUARDAR" : b);
        break;
    case 1:
        save_jobs(a, 0);
        job_push(a, JOB_SAVE_AS, a->save_target);
        snprintf(b, sizeof b, "GUARDADO COMO A%02d", a->save_target + 1);
        toast(a, playing(a) ? "PARA (STOP) PARA GUARDAR" : b);
        break;
    case 2:
        job_push(a, JOB_SAVE_SONG, 0);
        toast(a, "SONG Y ESCENAS GUARDADAS");
        break;
    case 3:
        if (a->clear_armed && a->now - a->clear_t < 3000) {
            for (int t = 0; t < BM_TRACKS; t++) push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
            edited(a); a->clear_armed = 0;
            toast(a, "PATRON BORRADO");
        } else { a->clear_armed = 1; a->clear_t = a->now; toast(a, "SAVE OTRA VEZ = BORRAR"); }
        break;
    default:
        snprintf(b, sizeof b, "%d EVENTOS", bm_pattern_count_events(&a->pat));
        toast(a, b);
    }
}

static void scene_store(bm_app *a, int s)
{
    bm_scene *sc = &a->song.scene[s];
    sc->valid = 1; sc->pattern = a->ctx.st.pattern; sc->trk_mute = 0;
    for (int t = 0; t < BM_TRACKS; t++) if (a->pat.trk[t].mute) sc->trk_mute |= (uint8_t)(1u << t);
    for (int t = 0; t < BM_TR_TRACKS; t++) sc->lane_mute[t] = a->pat.tr[t].mute;
    a->song_dirty = 1; a->t_edit = a->now;
    char b[24];
    snprintf(b, sizeof b, "ESCENA %c GUARDADA", 'A' + s);
    toast(a, b);
}

static void scene_launch(bm_app *a, int s)
{
    if (!a->song.scene[s].valid) { toast(a, "ESCENA VACIA: 5-8 GUARDA"); return; }
    if (a->launch_held & ~(1u << s)) {               /* otra sostenida: cadena */
        if (a->chain_n < 8) a->chain[a->chain_n++] = (uint8_t)s;
        push_ptr(a, CMD_CHAIN, a->chain_n, a->chain);
        toast(a, "CADENA");
    } else {
        a->chain_n = 0;
        a->chain[a->chain_n++] = (uint8_t)s;
        push(a, CMD_SCENE, (uint8_t)s, 0, 0);
    }
    a->launch_held |= (uint8_t)(1u << s);
}

/* ------------------------------------------------------------ gestos de botones */
static void close_layer(bm_app *a, int id)
{
    if (id == BTN_FX) { push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    if (id == BTN_ARP) {
        push(a, CMD_ARP, a->track, 0, 0);
        for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) push_i(a, CMD_ROLL, (uint8_t)tr_of(a), (uint8_t)l, 0, 0);
        a->roll_mask = 0;
    }
    if (id == BTN_EDIT) { a->erase_mask = 0; a->erase_syn = 0; }
    a->layer = 0;
}

static void on_gesture(void *u, int id, bm_gkind k)
{
    bm_app *a = (bm_app *)u;
    if (a->view == V_BOOT) return;
    char b[30];
    int v = bm_app_eff_view(a);
    switch (id) {
    case BTN_PLAY:
        if (k == G_PRESS) push(a, CMD_PLAY, a->glo_down ? 2 : (playing(a) ? 0 : 1), 0, 0);
        return;
    case BTN_REC:
        if (k == G_PRESS) {
            a->rec_before = a->rec;
            if ((v == V_EDIT || v == V_SAMPLER) && a->track == 1) {
                if (a->ctx.st.smp_unsaved && playing(a)) { toast(a, "PARA PARA GUARDAR EL SAMPLE"); return; }
                push(a, CMD_SMP_REC, 1, 0, 0); a->smp_was_rec = 1;
            } else if (v == V_LOOP) push(a, CMD_LOOP, a->track, 1, 0);
            else if (v == V_SCENES) { a->song_rec = !a->song_rec; a->song_rec_loops = a->ctx.st.loops; toast(a, a->song_rec ? "SONG REC ON" : "SONG REC OFF"); }
            else {
                a->rec = !a->rec;
                push(a, CMD_REC, a->rec, 0, 0);
                if (a->rec) undo_snapshot(a, a->track);
                toast(a, a->rec ? "REC ON" : "REC OFF");
            }
        } else if (k == G_UP && a->smp_was_rec) {
            push(a, CMD_SMP_REC, 0, 0, 0);
            a->smp_was_rec = 0;
        } else if (k == G_LONG) {
            if (a->smp_was_rec) return;
            a->rec = a->rec_before; push(a, CMD_REC, a->rec, 0, 0);
            if (v == V_LOOP) { push(a, CMD_LOOP, a->track, 2, 0); toast(a, "LOOP BORRADO"); }
            else clear_track(a, a->track);
        }
        return;
    case BTN_OCT_DN: case BTN_OCT_UP:
        if (k != G_PRESS) return;
        {
            int d = id == BTN_OCT_UP ? 1 : -1;
            if (v == V_ERASE) { if (d < 0) do_undo(a); else do_redo(a); return; }
            if (a->glo_down) {
                int tp = bm_clampi(a->pat.trk[a->track].transpose + d, 0, 48);
                set_track(a, a->track, TRK_OFF(transpose), tp);
                snprintf(b, sizeof b, "TRANSPONER %+d", tp - 24);
            } else if (v == V_SEQ) {
                int len = bm_is_tr(a->track) ? a->pat.tr[a->track].lane[a->sel_lane[a->track]].len : a->pat.trk[a->track].length;
                int pages = (len + 15) / 16;
                a->page = (uint8_t)bm_clampi(a->page + d, 0, pages > 0 ? pages - 1 : 0);
                snprintf(b, sizeof b, "PAGINA %d/%d", a->page + 1, pages);
            } else {
                a->oct = (int8_t)bm_clampi(a->oct + d, -2, 2);
                snprintf(b, sizeof b, "OCTAVA %+d", a->oct);
            }
            toast(a, b);
        }
        return;
    case BTN_GLO:
        if (k == G_PRESS) a->glo_down = 1;
        else if (k == G_UP) a->glo_down = 0;
        else if (k == G_TAP) set_view(a, a->view == V_GLO ? V_HOME : V_GLO);
        return;
    case BTN_SAVE:
        if (k == G_TAP) { if (a->view == V_SAVE) save_exec(a); else set_view(a, V_SAVE); }
        else if (k == G_LONG) { save_jobs(a, 1); toast(a, playing(a) ? "PARA (STOP) PARA GUARDAR" : "GUARDADO RAPIDO"); }
        return;
    default: break;
    }
    if (k == G_TAP) {
        int nv = V_HOME;
        switch (id) {
        case BTN_HOME: nv = a->view == V_HOME ? V_SCENES : V_HOME; break;
        case BTN_SEQ:
            if (a->glo_down) nv = V_SONG;
            else if (a->view == V_SEQ && !bm_is_tr(a->track)) nv = V_LOOP;
            else nv = V_SEQ;
            break;
        case BTN_EDIT:
            if (a->init_armed && a->now - a->clear_t < 3000) {
                bm_track d; bm_track_defaults(&d, a->track);
                for (int i = 0; i < 16; i++) push(a, CMD_SET_TRACK, a->track, (uint8_t)i, ((const uint8_t *)&d)[i]);
                edited(a); a->init_armed = 0; toast(a, "SONIDO INICIAL"); return;
            }
            nv = (a->view == V_EDIT && bm_is_tr(a->track)) ? V_INST : V_EDIT;
            break;
        case BTN_ENV: if (a->view == V_ENV) a->env_page ^= 1; nv = V_ENV; break;
        case BTN_LFO: nv = V_LFO; break;
        case BTN_FX: nv = V_FX; break;
        case BTN_SCL: nv = a->view == V_SCL ? V_CHORDS : V_SCL; break;
        case BTN_ARP: nv = V_ARP; break;
        default: return;
        }
        set_view(a, nv);
    } else if (k == G_HOLD) {
        switch (id) {
        case BTN_SEQ: a->layer = V_BANK; break;
        case BTN_HOME: a->layer = V_MUTE; break;
        case BTN_EDIT: a->layer = V_ERASE; a->erase_mask = 0; a->erase_syn = 0; break;
        case BTN_FX: a->layer = V_PUNCH; break;
        case BTN_SCL: a->layer = V_CHORDLIVE; break;
        case BTN_ARP:
            if (bm_is_tr(a->track)) a->layer = V_ROLL;
            else { a->layer = V_ARPLIVE; push(a, CMD_ARP, a->track, 1, 0); }
            break;
        default: break;
        }
        a->ov.on = 0;
    } else if (k == G_RELEASE) {
        close_layer(a, id);
    } else if (k == G_LONG) {
        close_layer(a, id);
        if (id == BTN_HOME) set_view(a, V_SYSTEM);
        else if (id == BTN_SEQ) { a->time_row = a->track; set_view(a, V_TIME); }
        else if (id == BTN_EDIT) {
            if (a->track == 1) set_view(a, V_SAMPLER);
            else { a->init_armed = 1; a->clear_t = a->now; toast(a, "EDIT = SONIDO INICIAL"); }
        }
    }
}

void bm_app_button(bm_app *a, int id, int down, uint32_t now)
{
    a->now = now;
    if (down) bm_gesture_down(&a->ges, id, now);
    else bm_gesture_up(&a->ges, id, now);
}

/* ------------------------------------------------------------ notas y grabación en vivo */
static void rec_syn(bm_app *a, int k, int t, const uint8_t *notes, int n)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int len = a->pat.trk[t].length ? a->pat.trk[t].length : 16;
    uint32_t rt = a->ctx.st.rtick;
    int s = (int)(((rt + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)len);
    a->key_rec_step[k] = (int8_t)s;
    a->key_rec_tick[k] = rt;
    a->key_rec_slot[k] = -1;
    for (int i = 0; i < n; i++) {
        int slot = -1;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = &a->pat.ev[t - 2][s][e];
            if (!bm_event_empty(ev) && ev->note == notes[i]) { slot = e; break; }
            if (slot < 0 && bm_event_empty(ev)) slot = e;
        }
        if (slot < 0) continue;
        bm_event ev;
        bm_event_clear(&ev);
        ev.note = notes[i]; ev.vel = 100; ev.len = 1;
        push_ev(a, t, s, slot, &ev);
        if (a->key_rec_slot[k] < 0) a->key_rec_slot[k] = (int8_t)slot;
    }
    edited(a);
}

static void rec_tr(bm_app *a, int t, int lane)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int len = a->pat.tr[t].lane[lane].len ? a->pat.tr[t].lane[lane].len : 16;
    int s = (int)(((a->ctx.st.rtick + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)len);
    push_trstep(a, t, lane, s, (uint8_t)(a->pat.tr[t].st[lane][s] | ST_ON));
}

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel, int strum)
{
    if (strum && n > 1) {
        int32_t packed = 0;
        for (int i = 0; i < n && i < 4; i++) packed |= (int32_t)notes[i] << (8 * i);
        push_i(a, CMD_STRUM, (uint8_t)t, (uint8_t)n, (uint8_t)vel, packed);
    } else {
        for (int i = 0; i < n; i++) push(a, CMD_NOTE_ON, (uint8_t)t, notes[i], (uint8_t)vel);
    }
    memcpy(a->key_notes[k], notes, (size_t)n);
    a->key_nn[k] = (uint8_t)n;
    a->key_trk[k] = (uint8_t)t;
    if (n && !bm_is_tr(t)) a->last_note[t] = notes[0];
    a->key_rec_step[k] = -1;
    if (a->rec && playing(a) && a->view != V_SEQ) {
        if (bm_is_tr(t)) rec_tr(a, t, notes[0]);
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
    int t = bm_app_chord_track(a);
    bm_pad_chord pc = bm_chord_pad(m->chord_set, pad);
    int ext = chord_ext(a, pc.ext);
    uint8_t notes[4];
    int n = bm_chord_notes(m->root, m->scale, pc.degree, ext, m->chord_oct + a->oct, notes);
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
    bm_chord_name(m->root, m->scale, pc.degree, ext, a->chord_name, sizeof a->chord_name);
    bm_roman(m->root, m->scale, pc.degree, ext, a->chord_roman, sizeof a->chord_roman);
    a->last_pad = (int8_t)pad;
    play_notes(a, k, t, notes, n, 95, 1);
}

static void play_key_down(bm_app *a, int k, int w, int b, int v)
{
    int t = a->track;
    const bm_meta *m = &a->pat.meta;
    if (bm_is_tr(t)) {
        if (w >= 0) {
            uint8_t n = (uint8_t)w;
            a->sel_lane[t] = (uint8_t)w;
            play_notes(a, k, t, &n, 1, 110, 0);
        } else if (b < 6) select_track(a, b);
        return;
    }
    int base_oct = (t == 2 ? 2 : 3) + a->oct;
    if (v == V_SCL || m->scale_lock) {
        if (w >= 0) {
            uint8_t n = (uint8_t)bm_scale_note(m->root, m->scale, w, base_oct);
            play_notes(a, k, t, &n, 1, 105, 0);
        } else if (b < 7) {
            uint8_t notes[4];
            int ext = chord_ext(a, m->voicing % 5);
            int n = bm_chord_notes(m->root, m->scale, b, ext, base_oct, notes);
            bm_chord_name(m->root, m->scale, b, ext, a->chord_name, sizeof a->chord_name);
            bm_roman(m->root, m->scale, b, ext, a->chord_roman, sizeof a->chord_roman);
            play_notes(a, k, t, notes, n, 95, 1);
        } else a->chord_mod |= (uint8_t)(1u << (b - 7));
        return;
    }
    int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
    uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
    play_notes(a, k, t, &n, 1, 105, 0);
}

/* ------------------------------------------------------------ SEQ */
static void seq_step_down(bm_app *a, int w)
{
    int t = a->track;
    if (a->held_white >= 0) return;
    int s = a->page * BM_PAGE_STEPS + w;
    int len = bm_is_tr(t) ? a->pat.tr[t].lane[a->sel_lane[t]].len : a->pat.trk[t].length;
    if (s >= len) { toast(a, "FUERA DEL LARGO"); return; }
    undo_snapshot(a, t);
    a->held_white = (int8_t)w; a->held_step = (int8_t)s;
    a->held_long = 0; a->held_edit = 0; a->step_t0 = a->now;
    if (bm_is_tr(t)) {
        int lane = a->sel_lane[t];
        a->held_had_event = (a->pat.tr[t].st[lane][s] & ST_ON) != 0;
        if (!a->held_had_event) push_trstep(a, t, lane, s, ST_ON);
        return;
    }
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    a->held_had_event = n > 0;
    if (!n) {
        bm_event e; bm_event_clear(&e);
        e.note = a->last_note[t]; e.vel = 100;
        push_ev(a, t, s, 0, &e);
        edited(a);
    }
}

static void seq_step_up(bm_app *a, int w)
{
    if (w != a->held_white) return;
    int t = a->track, s = a->held_step;
    if (s >= 0 && a->held_had_event && !a->held_long && !a->held_edit) {
        if (bm_is_tr(t)) push_trstep(a, t, a->sel_lane[t], s, 0);
        else {
            bm_event e; bm_event_clear(&e);
            for (int k = 0; k < BM_EVENTS; k++) push_ev(a, t, s, k, &e);
            edited(a);
        }
    }
    a->held_white = -1; a->held_step = -1; a->held_long = 0; a->held_edit = 0;
}

static void seq_black_tr(bm_app *a, int k, int b)
{
    int t = a->track;
    if (b < 8) {
        int lane = (a->sel_lane[t] / 8) * 8 + b;
        a->sel_lane[t] = (uint8_t)lane;
        uint8_t n = (uint8_t)lane;
        play_notes(a, k, t, &n, 1, 100, 0);
    } else if (b == 8) {
        a->sel_lane[t] = (uint8_t)((a->sel_lane[t] + 8) % 16);
        toast(a, a->sel_lane[t] < 8 ? "INSTRUMENTOS 1-8" : "INSTRUMENTOS 9-16");
    } else if (b == 9) {
        push(a, CMD_FILL, 1, 0, 0); a->fill = 1;
    } else if (b == 10) {
        a->key_t0[k] = a->now;                 /* toque = mute; mantener = pantalla de mutes */
    }
}

static void seq_black_syn(bm_app *a, int b)
{
    if (b < 6) { select_track(a, b); return; }
    int t = a->track;
    if (b == 6) {
        if (lock_mode(a)) { memcpy(a->clip_step, a->pat.ev[t - 2][a->held_step], sizeof a->clip_step); a->clip_type = 1; toast(a, "PASO COPIADO"); }
        else copy_track(a);
    } else if (b == 7) {
        if (a->held_step >= 0 && a->clip_type == 1) {
            undo_snapshot(a, t);
            for (int e = 0; e < BM_EVENTS; e++) push_ev(a, t, a->held_step, e, &a->clip_step[e]);
            a->held_edit = 1; edited(a); toast(a, "PASO PEGADO");
        } else paste_track(a);
    } else if (b == 8) {
        if (a->held_step >= 0) {
            undo_snapshot(a, t);
            bm_event e; bm_event_clear(&e);
            for (int k = 0; k < BM_EVENTS; k++) push_ev(a, t, a->held_step, k, &e);
            a->held_edit = 1; edited(a); toast(a, "PASO BORRADO");
        } else clear_track(a, t);
    } else if (b == 9) { push(a, CMD_FILL, 1, 0, 0); a->fill = 1; }
    else if (b == 10) do_undo(a);
}

/* ------------------------------------------------------------ teclas */
static void song_append(bm_app *a, int p)
{
    if (a->song.n >= BM_SONG_MAX) { toast(a, "SONG LLENA"); return; }
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

static void key_release(bm_app *a, int k, int w, int b)
{
    if (a->key_nn[k]) {
        int t = a->key_trk[k];
        if (!bm_is_tr(t)) for (int i = 0; i < a->key_nn[k]; i++) push(a, CMD_NOTE_OFF, (uint8_t)t, a->key_notes[k][i], 0);
        if (a->key_rec_step[k] >= 0 && a->key_rec_slot[k] >= 0 && !bm_is_tr(t)) {
            int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            int steps = bm_clampi((int)((a->ctx.st.rtick - a->key_rec_tick[k] + (uint32_t)tps / 2) / (uint32_t)tps), 1, 16);
            int s = a->key_rec_step[k];
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = a->pat.ev[t - 2][s][e];
                for (int i = 0; i < a->key_nn[k]; i++)
                    if (!bm_event_empty(&ev) && ev.note == a->key_notes[k][i]) { ev.len = (uint8_t)steps; push_ev(a, t, s, e, &ev); }
            }
        }
        a->key_nn[k] = 0;
        a->key_rec_step[k] = -1;
    }
    if (a->punch_key == k) { push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    int v = bm_app_eff_view(a);
    if (b >= 0 && b < 4 && (v == V_CHORDS || v == V_CHORDLIVE)) a->chord_mod &= (uint8_t)~(1u << b);
    if (b >= 7 && b <= 10) a->chord_mod &= (uint8_t)~(1u << (b - 7));
    if (b == 9 && a->fill) { push(a, CMD_FILL, 0, 0, 0); a->fill = 0; }
    if (b == 10 && a->key_t0[k]) {
        if (a->layer == V_LANEMUTE) a->layer = 0;
        else if (bm_is_tr(a->track) && a->view == V_SEQ) toggle_lane_mute(a, a->track, a->sel_lane[a->track]);
        a->key_t0[k] = 0;
    }
    if (w >= 0) {
        if (a->roll_mask & (1u << w)) { a->roll_mask &= (uint16_t)~(1u << w); push_i(a, CMD_ROLL, (uint8_t)tr_of(a), (uint8_t)w, 0, 0); }
        a->erase_mask &= (uint16_t)~(1u << w);
        if (!a->erase_mask) a->erase_syn = 0;
        seq_step_up(a, w);
        if (w < 4) a->launch_held &= (uint8_t)~(1u << w);
    }
}

void bm_app_key(bm_app *a, int k, int down, uint32_t now)
{
    a->now = now;
    if (k < 0 || k >= BM_KEYS) return;
    if (down) {
        if (a->keys_down[k]) return;
        a->keys_down[k] = 1;
        bm_gesture_other(&a->ges, now);
    } else {
        if (!a->keys_down[k]) return;
        a->keys_down[k] = 0;
    }
    if (a->view == V_BOOT) return;
    int w = bm_key_white_index(k), b = bm_key_black_index(k);
    if (!down) { key_release(a, k, w, b); return; }
    if (a->glo_down && b >= 0 && b < 6) { toggle_mute(a, b); return; }
    int v = bm_app_eff_view(a);
    int t = a->track;
    switch (v) {
    case V_BANK: if (w >= 0) select_pattern(a, w); return;
    case V_MUTE:
        if (w >= 0 && w < 6) toggle_mute(a, w);
        else if (w >= 8 && w < 14) solo(a, w - 8);
        return;
    case V_LANEMUTE:
        if (w >= 0) toggle_lane_mute(a, tr_of(a), w);
        return;
    case V_PUNCH:
        if (w >= 0) { push(a, CMD_PUNCH, (uint8_t)w, 0, 0); a->punch_key = (int8_t)k; }
        return;
    case V_ERASE:
        if (b >= 0) {
            if (b == 0) copy_track(a); else if (b == 1) paste_track(a); else if (b == 2) clear_track(a, t);
            return;
        }
        if (bm_is_tr(t)) {
            if (playing(a)) { if (!a->erase_mask) undo_snapshot(a, t); a->erase_mask |= (uint16_t)(1u << w); }
            else {
                undo_snapshot(a, t);
                for (int s = 0; s < BM_STEPS; s++) if (a->pat.tr[t].st[w][s]) push_trstep(a, t, w, s, 0);
                char m[24]; snprintf(m, sizeof m, "%s BORRADO", bm_lane_name(a, t, w)); toast(a, m);
            }
        } else {
            if (playing(a)) { if (!a->erase_syn) undo_snapshot(a, t); a->erase_syn = 1; a->erase_mask |= (uint16_t)(1u << w); }
            else {
                int s = a->page * 16 + w;
                undo_snapshot(a, t);
                bm_event e; bm_event_clear(&e);
                for (int kk = 0; kk < BM_EVENTS; kk++) push_ev(a, t, s, kk, &e);
                edited(a);
            }
        }
        return;
    case V_ROLL:
        if (w >= 0) {
            a->roll_mask |= (uint16_t)(1u << w);
            a->sel_lane[tr_of(a)] = (uint8_t)w;
            push_i(a, CMD_ROLL, (uint8_t)tr_of(a), (uint8_t)w, 1, ROLL_TICKS[a->roll_rate % 5]);
        }
        return;
    case V_CHORDS: case V_CHORDLIVE:
        if (w >= 0) play_pad(a, k, w);
        else if (b < 4) a->chord_mod |= (uint8_t)(1u << b);
        return;
    case V_SCENES:
        if (w >= 0 && w < 4) scene_launch(a, w);
        else if (w >= 4 && w < 8) scene_store(a, w - 4);
        else if (b >= 0 && b < 6) select_track(a, b);
        return;
    case V_SONG:
        if (w >= 0) song_append(a, w);
        else if (b == 8) song_delete(a);
        return;
    case V_SEQ:
        if (w >= 0) seq_step_down(a, w);
        else if (bm_is_tr(t)) seq_black_tr(a, k, b);
        else seq_black_syn(a, b);
        return;
    case V_SAMPLER:
        if (b == 10) {
            int slot = a->pat.trk[1].source % 2;
            if (a->erase_slot_armed) { job_push(a, JOB_SAMPLE_ERASE, slot); a->erase_slot_armed = 0; toast(a, "SLOT BORRADO"); }
            else { a->erase_slot_armed = 1; toast(a, "NEGRA 11 OTRA VEZ = BORRAR"); }
            return;
        }
        break;
    default: break;
    }
    if ((v == V_EDIT || v == V_SAMPLER) && t == 1 && a->ctx.st.smp_rec) {
        if (w >= 0) push(a, CMD_SMP_MARK, 0, 0, 0);
        return;
    }
    play_key_down(a, k, w, b, v);
}

/* ------------------------------------------------------------ encoders */
static void enc_macro(bm_app *a, int i, int d)
{
    int t = a->track;
    int v = bm_clampi(a->pat.trk[t].macro[i] + d, 0, 127);
    set_track(a, t, (uint8_t)i, v);
    char b[12], ti[22];
    bm_fmt_macro(a, t, i, v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", t + 1, bm_macro_name(t, i));
    overlay(a, i, ti, b, "", v * 2, -1, bm_knob_color(i));
}

static void enc_step_tr(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step, lane = a->sel_lane[t];
    bm_tr *tr = &a->pat.tr[t];
    uint8_t bb = tr->st[lane][s];
    if (!(bb & ST_ON)) { toast(a, "PASO VACIO"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16], ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        int cur = bm_tr_lock_get(tr, s, lane, i);
        if (cur < 0) cur = a->pat.trk[t].macro[i];
        int v = bm_clampi(cur + d, 0, 127);
        push_trlock(a, t, lane, s, i, v);
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, bm_macro_name(t, i));
        overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
        return;
    }
    if (id == ENC_SELECT) {
        bb = d > 0 ? (uint8_t)(bb | ST_ACC) : (uint8_t)(bb & ~ST_ACC);
        snprintf(b, sizeof b, "%s", (bb & ST_ACC) ? "ACENTO" : "NORMAL");
        snprintf(ti, sizeof ti, "PASO %02d GOLPE", s + 1);
    } else if (id == ENC_ALGO) {
        int r = (bb & ST_FLAM) ? 4 : ((bb & ST_RAT_MASK) >> ST_RAT_SH);
        r = bm_clampi(r + (d > 0 ? 1 : -1), 0, 4);
        bb = (uint8_t)(bb & ~(ST_RAT_MASK | ST_FLAM));
        if (r == 4) bb |= ST_FLAM; else bb |= (uint8_t)(r << ST_RAT_SH);
        snprintf(b, sizeof b, "%s", RAT_NAMES[r]);
        snprintf(ti, sizeof ti, "PASO %02d REPETICION", s + 1);
    } else {
        int c = (bb & ST_COND_MASK) >> ST_COND_SH;
        c = bm_clampi(c + (d > 0 ? 1 : -1), 0, 2);
        bb = (uint8_t)((bb & ~ST_COND_MASK) | (c << ST_COND_SH));
        snprintf(b, sizeof b, "%s", COND_NAMES[c]);
        snprintf(ti, sizeof ti, "PASO %02d CONDICION", s + 1);
    }
    push_trstep(a, t, lane, s, bb);
    overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
}

static void enc_step_syn(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step;
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    if (!n) { toast(a, "PASO VACIO"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16] = "", ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        bm_event e0 = a->pat.ev[t - 2][s][idx[0]];
        int base = e0.lk[i] != BM_LOCK_NONE ? e0.lk[i] : a->pat.trk[t].macro[i];
        int v = bm_clampi(base + d, 0, 127);
        for (int j = 0; j < n; j++) { bm_event e = a->pat.ev[t - 2][s][idx[j]]; e.lk[i] = (uint8_t)v; push_ev(a, t, s, idx[j], &e); }
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, bm_macro_name(t, i));
        overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
        edited(a);
        return;
    }
    for (int j = 0; j < n; j++) {
        bm_event e = a->pat.ev[t - 2][s][idx[j]];
        if (id == ENC_SELECT) {
            e.note = (uint8_t)bm_clampi(e.note + d, 0, 127);
            if (j == 0) { a->last_note[t] = e.note; bm_note_str(e.note, b, sizeof b); }
        } else if (id == ENC_ALGO) {
            int p = e.prob == BM_PROB_FILL ? 101 : e.prob == BM_PROB_NOFILL ? 102 : e.prob;
            if (p <= 100) p = bm_clampi(p + d * 5, 0, 102); else p = bm_clampi(p + (d > 0 ? 1 : -1), 95, 102);
            e.prob = p == 101 ? BM_PROB_FILL : p == 102 ? BM_PROB_NOFILL : (uint8_t)bm_clampi(p, 0, 100);
            if (j == 0) { if (p == 101) snprintf(b, sizeof b, "SOLO FILL"); else if (p == 102) snprintf(b, sizeof b, "SIN FILL"); else snprintf(b, sizeof b, "%d%%", e.prob); }
        } else {
            int cur = (e.len & BM_LEN_TIE) ? 17 : (e.len & BM_LEN_MASK);
            cur = bm_clampi(cur + d, 1, 17);
            e.len = cur == 17 ? (uint8_t)(1 | BM_LEN_TIE) : (uint8_t)cur;
            if (j == 0) { if (cur == 17) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "%d PASOS", cur); }
        }
        push_ev(a, t, s, idx[j], &e);
    }
    const char *nm = id == ENC_SELECT ? "NOTA" : id == ENC_ALGO ? "PROB/FILL" : "LARGO";
    snprintf(ti, sizeof ti, "PASO %02d %s", s + 1, nm);
    overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
    edited(a);
}

static void system_change(bm_app *a, int d)
{
    bm_settings *s = &a->set;
    switch (a->menu_row) {
    case 0: s->clock = !s->clock; push(a, CMD_CLOCK, s->clock, 0, 0); break;
    case 1: s->sync_mode = !s->sync_mode; break;
    case 2: s->sync_ppqn = s->sync_ppqn == 1 ? (d > 0 ? 2 : 4) : s->sync_ppqn == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 1 : 2); break;
    case 3: s->sync_lat = (uint8_t)bm_clampi(s->sync_lat + d, 0, 20); break;
    case 4: s->autosave = !s->autosave; break;
    case 5: s->led_dim = (uint8_t)bm_clampi(s->led_dim + d, 0, 2); break;
    default: return;
    }
    push(a, CMD_SYNC, s->sync_mode, s->sync_ppqn, s->sync_lat);
    a->set_dirty = 1; a->t_edit = a->now;
}

static bool enc_global(bm_app *a, int id, int d)
{
    bm_meta *m = &a->pat.meta;
    int t = a->track;
    char b[16];
    switch (id) {
    case ENC_K1: {
        int bpm = bm_clampi(m->bpm_x10 + d * 10, 400, 2400);
        push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); edited(a);
        snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
        overlay(a, 0, "TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, bm_knob_color(0));
        return true;
    }
    case ENC_K2: {
        int sw = bm_clampi(m->swing + (d > 0 ? 1 : -1), 50, 75);
        set_meta(a, META_OFF(swing), sw);
        snprintf(b, sizeof b, "%d", sw);
        overlay(a, 1, "SWING  50=RECTO", b, "%", (sw - 50) * 255 / 25, -1, bm_knob_color(1));
        return true;
    }
    case ENC_K3: {
        int i = 0;
        while (i < 6 && MASTER_STEPS[i] != m->master) i++;
        i = bm_clampi((i >= 6 ? 3 : i) + (d > 0 ? 1 : -1), 0, 5);
        set_meta(a, META_OFF(master), MASTER_STEPS[i]);
        fmt_master(MASTER_STEPS[i], b, sizeof b);
        overlay(a, 2, "LARGO MAESTRO", b, MASTER_STEPS[i] ? "PASOS" : "", i * 51, -1, bm_knob_color(2));
        return true;
    }
    case ENC_K4:
        a->set.drive = (uint8_t)bm_clampi(a->set.drive + d, 0, 127);
        push(a, CMD_DRIVE, a->set.drive, 0, 0);
        a->set_dirty = 1; a->t_edit = a->now;
        fmt_pct(a->set.drive, b, sizeof b);
        overlay(a, 3, "DRIVE (SOFT-CLIP)", b, "", a->set.drive * 2, -1, bm_knob_color(3));
        return true;
    case ENC_SELECT: {
        static const uint8_t FE[4] = {0, 2, 4, 8};
        int i = 0;
        while (i < 4 && FE[i] != m->fill_every) i++;
        i = bm_clampi((i >= 4 ? 0 : i) + (d > 0 ? 1 : -1), 0, 3);
        set_meta(a, META_OFF(fill_every), FE[i]);
        if (FE[i]) snprintf(b, sizeof b, "CADA %d", FE[i]); else snprintf(b, sizeof b, "OFF");
        overlay(a, -1, "FILL AUTOMATICO", b, FE[i] ? "COMP" : "", i * 85, -1, C_WHITE);
        return true;
    }
    case ENC_ALGO: {
        int lim = t == 0 ? BM_KITS : t == 1 ? BM_SMP_SLOTS : BM_WAVES;
        if (t == 1 && a->ctx.st.smp_unsaved) { toast(a, "PRIMERO SE GUARDA EL SAMPLE"); return true; }
        int v = (a->pat.trk[t].source + (d > 0 ? 1 : lim - 1)) % lim;
        set_track(a, t, TRK_OFF(source), v);
        a->pat.trk[t].source = (uint8_t)v;       /* para mandar ya la fuente correcta */
        if (t == 1) send_smp_src(a);
        const char *nm = t == 0 ? bm_rom_kit_names[v] : t == 1 ? (v ? "SLOT B" : "SLOT A") : bm_rom_wave_names[v];
        overlay(a, -1, t == 0 ? "KIT" : t == 1 ? "SAMPLE" : "BANCO DE ONDAS", nm, "", v * 255 / (lim > 1 ? lim - 1 : 1), -1, bm_track_color(t));
        return true;
    }
    default: return false;
    }
}

void bm_app_encoder(bm_app *a, int id, int d, uint32_t now)
{
    a->now = now;
    if (!d || a->view == V_BOOT) return;
    bm_gesture_other(&a->ges, now);
    if (d >= 3 || d <= -3) d *= 4;
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    bm_meta *m = &a->pat.meta;
    char b[16];
    int v = bm_app_eff_view(a);

    if (a->glo_down || v == V_GLO) { if (enc_global(a, id, d)) return; }

    if (a->held_step >= 0 && v == V_SEQ) {
        if (bm_is_tr(t)) enc_step_tr(a, id, d); else enc_step_syn(a, id, d);
        return;
    }
    if (id == ENC_ALGO && v != V_TIME) { select_track(a, bm_clampi(t + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1)); return; }
    if (id == ENC_PRESETS) {
        int base = a->pat_sel >= 0 ? a->pat_sel : a->ctx.st.pattern;
        a->pat_sel = (int8_t)bm_clampi(base + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
        a->pat_sel_t = now;
        snprintf(b, sizeof b, "A%02d", a->pat_sel + 1);
        overlay(a, -1, playing(a) ? "PATRON (SE ENCOLA)" : "PATRON", b, "", a->pat_sel * 17, -1, C_WHITE);
        return;
    }
    int k = id - ENC_K1;
    switch (v) {
    case V_HOME: case V_MUTE:
        if (k < 0 || k > 3) break;
        {
            static const uint8_t OFFS[4] = {TRK_OFF(level), TRK_OFF(pan), TRK_OFF(send), TRK_OFF(duck)};
            const uint8_t cur[4] = {tr->level, tr->pan, tr->send, tr->duck};
            if (k == 3 && t < 2) { overlay(a, 3, "DUCK", t == 0 ? "FUENTE" : "-", "", 0, -1, bm_knob_color(3)); return; }
            int nv = bm_clampi(cur[k] + d, 0, 127);
            set_track(a, t, OFFS[k], nv);
            if (k == 1) fmt_pan(nv, b, sizeof b); else if (k == 0) snprintf(b, sizeof b, "%d", nv); else fmt_pct(nv, b, sizeof b);
            static const char *const NM[4] = {"NIVEL", "PAN", "ENVIO DELAY", "DUCK (SIDECHAIN)"};
            char ti[22]; snprintf(ti, sizeof ti, "T%d %s", t + 1, NM[k]);
            overlay(a, k, ti, b, "", nv * 2, k == 1 ? 128 : -1, bm_knob_color(k));
        }
        return;
    case V_INST: {
        int tt = tr_of(a), lane = a->sel_lane[tt];
        if (id == ENC_SELECT) { a->sel_lane[tt] = (uint8_t)bm_clampi(lane + (d > 0 ? 1 : -1), 0, 15); return; }
        if (k < 0 || k > 3) break;
        const bm_lane *ln = &a->pat.tr[tt].lane[lane];
        const uint8_t cur[4] = {ln->level, ln->tune, ln->decay, ln->len};
        static const uint8_t FIELD[4] = {1, 2, 3, 0};
        int nv = k == 3 ? bm_clampi(cur[k] + (d > 0 ? 1 : -1), 1, 64) : bm_clampi(cur[k] + d, 0, 127);
        push_lane(a, tt, lane, FIELD[k], nv);
        static const char *const NM[4] = {"NIVEL", "AFINACION", "DECAY", "LARGO"};
        if (k == 1) snprintf(b, sizeof b, "%+d ST", ((nv - 64) * 12) / 64); else if (k == 2) snprintf(b, sizeof b, "%d%%", nv * 100 / 127); else snprintf(b, sizeof b, "%d", nv);
        char ti[22]; snprintf(ti, sizeof ti, "%s %s", bm_lane_name(a, tt, lane), NM[k]);
        overlay(a, k, ti, b, k == 3 ? "PASOS" : "", k == 3 ? nv * 4 : nv * 2, -1, bm_knob_color(k));
        return;
    }
    case V_TIME: {
        if (id == ENC_SELECT || id == ENC_ALGO) { a->time_row = (uint8_t)bm_clampi(a->time_row + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1); return; }
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        char ti[22];
        if (k == 0) {
            int cur = bm_is_tr(row) ? a->pat.tr[row].lane[0].len : rt->length;
            int nv = bm_clampi(cur + (d > 0 ? 1 : -1), 1, 64);
            if (bm_is_tr(row)) for (int l = 0; l < BM_LANES; l++) push_lane(a, row, l, 0, nv);
            else set_track(a, row, TRK_OFF(length), nv);
            snprintf(b, sizeof b, "%d", nv); snprintf(ti, sizeof ti, "T%d LARGO", row + 1);
            overlay(a, 0, ti, b, "PASOS", nv * 4, -1, bm_knob_color(0));
        } else if (k == 1) {
            int nv = bm_clampi(rt->speed + (d > 0 ? -1 : 1), 0, SPD_COUNT - 1);   /* a la derecha = más rápido */
            set_track(a, row, TRK_OFF(speed), nv);
            snprintf(ti, sizeof ti, "T%d VELOCIDAD", row + 1);
            overlay(a, 1, ti, bm_speed_names[nv], "", (SPD_COUNT - 1 - nv) * 51, -1, bm_knob_color(1));
        } else if (k == 2 && bm_is_tr(row)) {
            a->time_lane = (uint8_t)bm_clampi(a->time_lane + (d > 0 ? 1 : -1), 0, 15);
        } else if (k == 3 && bm_is_tr(row)) {
            const bm_lane *ln = &a->pat.tr[row].lane[a->time_lane];
            int nv = bm_clampi(ln->len + (d > 0 ? 1 : -1), 1, 64);
            push_lane(a, row, a->time_lane, 0, nv);
            snprintf(b, sizeof b, "%d", nv); snprintf(ti, sizeof ti, "%s LARGO", bm_lane_name(a, row, a->time_lane));
            overlay(a, 3, ti, b, "PASOS", nv * 4, -1, bm_knob_color(3));
        }
        return;
    }
    case V_ENV:
        if (k < 0 || k > 3) break;
        {
            char ti[22];
            if (a->env_page == 0) {
                static const uint8_t OFFS[4] = {TRK_OFF(attack), 2, TRK_OFF(sustain), TRK_OFF(release)};
                const uint8_t cur[4] = {tr->attack, tr->macro[2], tr->sustain, tr->release};
                int nv = bm_clampi(cur[k] + d, 0, 127);
                set_track(a, t, OFFS[k], nv);
                if (k == 0) fmt_ms(bm_attack_ms(nv), b, sizeof b); else if (k == 2) fmt_pct(nv, b, sizeof b); else fmt_ms(bm_decay_ms(nv), b, sizeof b);
                static const char *const NM[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
                snprintf(ti, sizeof ti, "T%d %s", t + 1, NM[k]);
                overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
            } else if (k < 2) {
                const uint8_t cur = k == 0 ? tr->env_amt : tr->glide;
                int nv = bm_clampi(cur + d, 0, 127);
                set_track(a, t, k == 0 ? TRK_OFF(env_amt) : TRK_OFF(glide), nv);
                if (k == 0) fmt_pct(nv, b, sizeof b); else fmt_ms((uint32_t)nv * 500 / 127, b, sizeof b);
                snprintf(ti, sizeof ti, "T%d %s", t + 1, k == 0 ? "ENV > FILTRO" : "GLIDE");
                overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
            }
        }
        return;
    case V_FX: case V_PUNCH:
        if (k < 0 || k > 3) break;
        if (k == 0) {
            int nv = bm_clampi(m->dly_time + (d > 0 ? 1 : -1), 0, 3);
            set_meta(a, META_OFF(dly_time), nv);
            overlay(a, 0, "DELAY TIEMPO", DLY_NAMES[nv], "", nv * 85, -1, bm_knob_color(0));
        } else {
            static const uint8_t OFFS[4] = {0, META_OFF(dly_fb), META_OFF(dly_mix), META_OFF(sc_release)};
            const uint8_t cur[4] = {0, m->dly_fb, m->dly_mix, m->sc_release};
            int nv = bm_clampi(cur[k] + d, 0, 127);
            set_meta(a, OFFS[k], nv);
            if (k == 3) fmt_ms(bm_decay_ms(nv / 2), b, sizeof b); else fmt_pct(nv, b, sizeof b);
            static const char *const NM[4] = {"", "DELAY FEEDBACK", "DELAY MIX", "SIDECHAIN RELEASE"};
            overlay(a, k, NM[k], b, "", nv * 2, -1, bm_knob_color(k));
        }
        return;
    case V_SCL:
        if (k < 0 || k > 3) break;
        {
            const char *val = "";
            if (k == 0) { int nv = (m->root + (d > 0 ? 1 : 11)) % 12; set_meta(a, META_OFF(root), nv); val = bm_note_names[nv]; }
            if (k == 1) { int nv = (m->scale + (d > 0 ? 1 : BM_SCALES - 1)) % BM_SCALES; set_meta(a, META_OFF(scale), nv); val = bm_scale_names[nv]; }
            if (k == 2) { int nv = (m->voicing + (d > 0 ? 1 : 4)) % 5; set_meta(a, META_OFF(voicing), nv); val = EXT_NAMES[nv]; }
            if (k == 3) { int nv = !m->scale_lock; set_meta(a, META_OFF(scale_lock), nv); val = nv ? "ON" : "OFF"; }
            static const char *const NM[4] = {"TONICA", "ESCALA", "ACORDE BASE", "TECLADO EN ESCALA"};
            overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        }
        return;
    case V_CHORDS: case V_CHORDLIVE:
        if (k < 0 || k > 3) break;
        {
            const char *val = "";
            if (k == 0) { int nv = (m->chord_set + (d > 0 ? 1 : BM_CHORD_SETS - 1)) % BM_CHORD_SETS; set_meta(a, META_OFF(chord_set), nv); val = bm_chord_set_names[nv]; }
            if (k == 1) { int nv = (m->chord_voicing + (d > 0 ? 1 : 2)) % 3; set_meta(a, META_OFF(chord_voicing), nv); val = VOICINGS[nv]; a->prev_chord_n = 0; }
            if (k == 2) { int nv = bm_clampi(m->chord_oct + (d > 0 ? 1 : -1), 1, 5); set_meta(a, META_OFF(chord_oct), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
            if (k == 3) { int nv = bm_clampi(m->strum + (d > 0 ? 2 : -2), 0, 60); set_meta(a, META_OFF(strum), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
            static const char *const NM[4] = {"JUEGO DE ACORDES", "VOICING", "OCTAVA", "STRUM (MS)"};
            overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        }
        return;
    case V_ARP: case V_ARPLIVE:
        if (k < 0 || k > 3) break;
        if (t < 2) { toast(a, "ARP: SOLO PISTAS SYNTH"); return; }
        {
            int nv;
            if (k == 0) { nv = (tr->arp_mode + (d > 0 ? 1 : 4)) % 5; set_track(a, t, TRK_OFF(arp_mode), nv); overlay(a, 0, "ARP MODO", ARP_MODES[nv], "", nv * 63, -1, bm_knob_color(0)); }
            if (k == 1) { nv = bm_clampi(tr->arp_rate + (d > 0 ? 1 : -1), 0, 2); set_track(a, t, TRK_OFF(arp_rate), nv); overlay(a, 1, "ARP RATE", ARP_RATES[nv], "", nv * 127, -1, bm_knob_color(1)); }
            if (k == 2) { nv = bm_clampi(tr->arp_oct + (d > 0 ? 1 : -1), 1, 3); set_track(a, t, TRK_OFF(arp_oct), nv); snprintf(b, sizeof b, "%d", nv); overlay(a, 2, "ARP OCTAVAS", b, "", nv * 85, -1, bm_knob_color(2)); }
            if (k == 3) { nv = bm_clampi(tr->arp_gate + d, 10, 100); set_track(a, t, TRK_OFF(arp_gate), nv); snprintf(b, sizeof b, "%d", nv); overlay(a, 3, "ARP GATE", b, "%", nv * 255 / 100, -1, bm_knob_color(3)); }
        }
        return;
    case V_ROLL:
        if (k == 0) {
            a->roll_rate = (uint8_t)bm_clampi(a->roll_rate + (d > 0 ? 1 : -1), 0, 4);
            overlay(a, 0, "ROLL", bm_roll_names[a->roll_rate], "", a->roll_rate * 63, -1, bm_knob_color(0));
            for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) push_i(a, CMD_ROLL, (uint8_t)tr_of(a), (uint8_t)l, 1, ROLL_TICKS[a->roll_rate]);
        }
        return;
    case V_LOOP:
        if (k == 0) { push(a, CMD_LOOP, (uint8_t)t, 3, 0); edited(a); }
        if (k == 1) {
            const bm_loop *lp = &a->pat.loop[t - 2];
            int nb = lp->bars == 1 ? (d > 0 ? 2 : 1) : lp->bars == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 4 : 2);
            push(a, CMD_LOOP, (uint8_t)t, 4, (uint8_t)nb); edited(a);
            snprintf(b, sizeof b, "%d", nb);
            overlay(a, 1, "LOOP COMPASES", b, "", nb * 63, -1, bm_knob_color(1));
        }
        return;
    case V_SAMPLER:
        if (k == 0) {
            if (a->ctx.st.smp_unsaved) { toast(a, "PRIMERO SE GUARDA EL SAMPLE"); return; }
            int v2 = !(a->pat.trk[1].source % 2);
            set_track(a, 1, TRK_OFF(source), v2);
            a->pat.trk[1].source = (uint8_t)v2;
            send_smp_src(a);
        } else if (k == 1) {
            int slot = a->pat.trk[1].source % 2;
            if (!a->smp_valid[slot]) { toast(a, "SLOT VACIO"); return; }
            a->chops_mode = (uint8_t)bm_clampi(a->chops_mode + (d > 0 ? 1 : -1), 0, 3);
            if (a->chops_mode) { if (a->pend_obj >= 0) job_push(a, JOB_FLUSH_PEND, 0); job_push(a, JOB_SAMPLE_MARKS, slot); }
            if (a->chops_mode) snprintf(b, sizeof b, "%d PARTES", CHOPS[a->chops_mode]); else snprintf(b, sizeof b, "MANUAL");
            overlay(a, 1, "CHOPS", b, "", a->chops_mode * 85, -1, bm_knob_color(1));
        }
        return;
    case V_SYSTEM:
        if (id == ENC_SELECT) a->menu_row = (uint8_t)bm_clampi(a->menu_row + (d > 0 ? 1 : -1), 0, 9);
        else if (id == ENC_K1) system_change(a, d > 0 ? 1 : -1);
        return;
    case V_SAVE:
        if (id == ENC_SELECT) a->save_row = (uint8_t)bm_clampi(a->save_row + (d > 0 ? 1 : -1), 0, 4);
        else if (id == ENC_K1) a->save_target = (uint8_t)bm_clampi(a->save_target + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
        a->clear_armed = 0;
        return;
    case V_SONG:
        if (id == ENC_SELECT && a->song.n) a->song_cur = (uint8_t)bm_clampi(a->song_cur + (d > 0 ? 1 : -1), 0, a->song.n - 1);
        else if (id == ENC_K1 && a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            int code = (e & 0x80) ? 16 + (e & 3) : e;               /* 0..15 patrones, 16..19 escenas */
            code = bm_clampi(code + (d > 0 ? 1 : -1), 0, 19);
            a->song.pat[a->song_cur] = (uint8_t)(code >= 16 ? (0x80 | (code - 16)) : code);
            a->song_dirty = 1; a->t_edit = now;
        }
        else if (id == ENC_K2 && a->song.n) { a->song.rep[a->song_cur] = (uint8_t)bm_clampi(a->song.rep[a->song_cur] + (d > 0 ? 1 : -1), 1, 16); a->song_dirty = 1; a->t_edit = now; }
        else if (id == ENC_K4) { push(a, CMD_SONG, a->ctx.st.song_on ? 0 : 1, 0, 0); toast(a, a->ctx.st.song_on ? "SONG OFF" : "SONG ON"); }
        return;
    case V_LFO:
        toast(a, "LFO RESERVADO");
        return;
    default: break;
    }
    if (id == ENC_SELECT) {
        if (bm_is_tr(t) && (v == V_SEQ || v == V_EDIT)) {
            a->sel_lane[t] = (uint8_t)bm_clampi(a->sel_lane[t] + (d > 0 ? 1 : -1), 0, 15);
            overlay(a, -1, "INSTRUMENTO", bm_lane_name(a, t, a->sel_lane[t]), "", a->sel_lane[t] * 17, -1, bm_track_color(t));
        }
        return;
    }
    if (k >= 0 && k <= 3) enc_macro(a, k, d);
}

/* ------------------------------------------------------------ LEDs */
static uint8_t led_last[BM_LEDS];

static void led(bm_app *a, int id, uint8_t lvl)
{
    if (lvl == LED_DIM && a->set.led_dim == 0) lvl = LED_OFF;
    if (led_last[id] == lvl + 1) return;
    led_last[id] = (uint8_t)(lvl + 1);
    if (a->pf.led) a->pf.led(a->pf.user, id, lvl);
}

static void leds_update(bm_app *a)
{
    int v = bm_app_eff_view(a);
    const bm_status *st = &a->ctx.st;
    if (a->view == V_BOOT) {
        int lit = (int)((a->now - a->boot_t) / 40u) % (BM_KEYS + 6);
        for (int k = 0; k < BM_KEYS; k++) led(a, BM_LED_KEY0 + k, k == lit ? LED_ON : LED_OFF);
        for (int i = 0; i < BTN_COUNT; i++) led(a, i, LED_DIM);
        return;
    }
    static const int VB[V_COUNT] = {
        [V_HOME] = BTN_HOME, [V_SCENES] = BTN_HOME, [V_SEQ] = BTN_SEQ, [V_LOOP] = BTN_SEQ, [V_EDIT] = BTN_EDIT,
        [V_INST] = BTN_EDIT, [V_ENV] = BTN_ENV, [V_LFO] = BTN_LFO, [V_FX] = BTN_FX, [V_SCL] = BTN_SCL,
        [V_CHORDS] = BTN_SCL, [V_ARP] = BTN_ARP, [V_SAVE] = BTN_SAVE, [V_GLO] = BTN_GLO, [V_SONG] = BTN_SEQ,
        [V_SYSTEM] = BTN_HOME, [V_TIME] = BTN_SEQ, [V_SAMPLER] = BTN_EDIT, [V_BANK] = BTN_SEQ, [V_MUTE] = BTN_HOME,
        [V_ERASE] = BTN_EDIT, [V_PUNCH] = BTN_FX, [V_CHORDLIVE] = BTN_SCL, [V_ARPLIVE] = BTN_ARP, [V_ROLL] = BTN_ARP,
        [V_LANEMUTE] = BTN_SEQ};
    for (int i = 0; i < BTN_COUNT; i++) {
        uint8_t l = LED_DIM;
        if (i == VB[v] && VB[v]) l = a->layer ? LED_BREATH : LED_ON;
        if (i == BTN_PLAY) l = st->playing ? ((st->btick % BM_PPQN) < 24 ? LED_ON : LED_DIM) : LED_DIM;
        if (i == BTN_REC) l = (st->smp_rec || a->song_rec) ? LED_BLINK : (a->rec ? LED_ON : LED_DIM);
        if (i == BTN_GLO && a->glo_down) l = LED_ON;
        if ((i == BTN_OCT_DN && a->oct < 0) || (i == BTN_OCT_UP && a->oct > 0)) l = LED_ON;
        led(a, i, l);
    }
    int t = a->track;
    for (int k = 0; k < BM_KEYS; k++) {
        int w = bm_key_white_index(k), b = bm_key_black_index(k);
        uint8_t l = LED_OFF;
        if (a->keys_down[k]) l = LED_ON;
        else if (v == V_SEQ && bm_is_tr(t)) {
            int lane = a->sel_lane[t];
            if (w >= 0) {
                int s = a->page * 16 + w, len = a->pat.tr[t].lane[lane].len;
                if (s < len) {
                    uint8_t bb = a->pat.tr[t].st[lane][s];
                    l = (bb & ST_ON) ? ((bb & ST_LOCK) ? LED_BREATH : LED_ON) : LED_DIM;
                    if (s == bm_app_track_step(a, t, lane)) l = LED_BLINK;
                }
            } else if (b < 8) {
                int ln = (lane / 8) * 8 + b;
                l = ln == lane ? LED_ON : (((a->pat.tr[t].mute >> ln) & 1) ? LED_OFF : LED_DIM);
            } else l = (b == 8 && lane >= 8) ? LED_ON : LED_DIM;
        } else if (v == V_SEQ) {
            if (w >= 0) {
                int s = a->page * 16 + w;
                if (s < a->pat.trk[t].length) {
                    int idx[4];
                    l = syn_step_events(a, t, s, idx) ? LED_ON : LED_DIM;
                    if (s == bm_app_track_step(a, t, -1)) l = LED_BLINK;
                }
            } else l = b < 6 ? (b == t ? LED_ON : (a->pat.trk[b].mute ? LED_OFF : LED_DIM)) : LED_DIM;
        } else if (v == V_BANK) {
            if (w >= 0) l = w == st->pattern ? LED_ON : (w == st->queued ? LED_BLINK : (bm_store_has(&a->store, (uint8_t)w) ? LED_DIM : LED_OFF));
        } else if (v == V_MUTE) {
            if (w >= 0 && w < 6) l = a->pat.trk[w].mute ? LED_OFF : LED_ON;
            else if (w >= 8 && w < 14) l = LED_DIM;
        } else if (v == V_LANEMUTE || v == V_ERASE || v == V_ROLL) {
            if (w >= 0) l = ((a->pat.tr[tr_of(a)].mute >> w) & 1) ? LED_OFF : LED_DIM;
            if (w >= 0 && ((a->erase_mask | a->roll_mask) & (1u << w))) l = LED_ON;
        } else if (v == V_PUNCH) {
            if (w >= 0) l = (st->punch == w) ? LED_ON : LED_DIM;
        } else if (v == V_CHORDS || v == V_CHORDLIVE) {
            if (w >= 0) l = w == a->last_pad ? LED_ON : LED_DIM;
            else if (b < 4) l = LED_DIM;
        } else if (v == V_SCENES) {
            if (w >= 0 && w < 4) l = w == st->scene ? LED_ON : (w == st->scene_q ? LED_BLINK : (a->song.scene[w].valid ? LED_DIM : LED_OFF));
            else if (w >= 4 && w < 8) l = LED_DIM;
        } else if (bm_is_tr(t) && w >= 0) {
            l = w == a->sel_lane[t] ? LED_ON : LED_DIM;
        } else if (t >= 2 && (v == V_SCL || a->pat.meta.scale_lock)) {
            if (w >= 0) l = (w % bm_scale_len(a->pat.meta.scale)) == 0 ? LED_ON : LED_DIM;
            else if (b < 7) l = LED_DIM;
        }
        led(a, BM_LED_KEY0 + k, l);
    }
}

/* ------------------------------------------------------------ tick */
void bm_app_tick(bm_app *a, uint32_t now)
{
    a->now = now;
    bm_status *st = &a->ctx.st;
    if (a->view == V_BOOT && now - a->boot_t >= UI_BOOT_MS) { a->view = V_HOME; a->scr.force = 1; }
    bm_gesture_poll(&a->ges, now);
    if (a->held_white >= 0 && !a->held_long && now - a->step_t0 >= UI_HOLD_MS) a->held_long = 1;
    for (int k = 0; k < BM_KEYS; k++)                         /* negra 11 sostenida en SEQ TR: mutes */
        if (a->key_t0[k] && a->keys_down[k] && now - a->key_t0[k] >= UI_HOLD_MS && !a->layer) a->layer = V_LANEMUTE;
    if (a->pat_sel >= 0 && now - a->pat_sel_t > 500) { select_pattern(a, a->pat_sel); a->pat_sel = -1; }
    if (a->ov.on && (int32_t)(now - a->ov.until) > 0) a->ov.on = 0;
    if (a->toast_until && (int32_t)(now - a->toast_until) > 0) a->toast_until = 0;
    for (int t = 0; t < BM_TRACKS; t++)
        if (st->trig[t] != a->last_trig[t]) { a->last_trig[t] = st->trig[t]; a->trig_until[t] = now + 90; }
    for (int t = 0; t < BM_TR_TRACKS; t++) {
        uint16_t m = st->lane_trig[t];
        if (m) { st->lane_trig[t] = 0; for (int l = 0; l < BM_LANES; l++) if (m & (1u << l)) a->lane_flash[t][l] = now + 90; }
    }
    if (st->edits != a->edits_seen) { a->edits_seen = st->edits; edited(a); }
    if (st->clip) { st->clip = 0; a->clip_until = now + 200; }
    if (st->swap_fail) { st->swap_fail = 0; toast(a, "SIN CAMBIO: PARA Y GUARDA"); }
    if (a->layer == V_ERASE && playing(a) && a->erase_mask) {      /* BORRAR sonando */
        int t = a->track;
        if (bm_is_tr(t)) {
            for (int l = 0; l < BM_LANES; l++)
                if (a->erase_mask & (1u << l)) {
                    int s = bm_app_track_step(a, t, l);
                    if (s >= 0 && a->pat.tr[t].st[l][s]) push_trstep(a, t, l, s, 0);
                }
        } else if (a->erase_syn) {
            int s = bm_app_track_step(a, t, -1), idx[4];
            if (s >= 0 && syn_step_events(a, t, s, idx)) {
                bm_event e; bm_event_clear(&e);
                for (int k = 0; k < BM_EVENTS; k++) push_ev(a, t, s, k, &e);
                edited(a);
            }
        }
    }
    if (a->song_rec && playing(a) && st->loops != a->song_rec_loops && st->scene != 0xFF) {   /* SONG REC */
        a->song_rec_loops = st->loops;
        uint8_t e = (uint8_t)(0x80 | st->scene);
        if (a->song.n && a->song.pat[a->song.n - 1] == e && a->song.rep[a->song.n - 1] < 16) a->song.rep[a->song.n - 1]++;
        else if (a->song.n < BM_SONG_MAX) { a->song.pat[a->song.n] = e; a->song.rep[a->song.n] = 1; a->song.n++; }
        a->song_dirty = 1; a->t_edit = now;
    }
    if (a->wave_was_rec && !st->smp_rec) compute_wave(a, (const int8_t *)a->amem.arena, a->ctx.smp.len);
    a->wave_was_rec = st->smp_rec;
    if (st->smp_unsaved && !st->smp_rec && !playing(a) && !a->njob && a->pf.flash) {   /* sample nuevo: al parar */
        job_push(a, JOB_SAVE_SAMPLE, a->pat.trk[1].source % BM_SMP_SLOTS);
        toast(a, (a->pat.trk[1].source % 2) ? "GUARDANDO SAMPLE B" : "GUARDANDO SAMPLE A");
    }
    if (a->set.autosave && !playing(a) && !a->njob && (a->dirty || a->pend_obj >= 0 || a->song_dirty || a->set_dirty)
        && now - a->t_edit > UI_AUTOSAVE_MS && a->pf.flash) {
        save_jobs(a, a->dirty);
        if (a->song_dirty) job_push(a, JOB_SAVE_SONG, 0);
        if (a->set_dirty) job_push(a, JOB_SAVE_GLOB, 0);
        toast(a, "AUTOGUARDADO");
    }
    jobs_run(a);
    leds_update(a);
    if (a->pf.blit) bm_screen_render(&a->scr, bm_app_draw, a, a->pf.blit, a->pf.user);
}
