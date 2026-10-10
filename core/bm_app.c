/* Bubba Machine — aplicación: lógica de la interfaz (SPEC_V3 §6). */
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "bm_app.h"
#include "bm_util.h"
#include "bm_rom.h"

#define TRK_OFF(f)  ((uint8_t)offsetof(bm_track, f))
#define META_OFF(f) ((uint8_t)offsetof(bm_meta, f))

static const char *const TRACK_NAMES[BM_TRACKS] = {"DRUMS", "SAMPLER", "BASS", "SYN A", "SYN B", "SYN C"};
static const uint16_t TRACK_COLORS[BM_TRACKS] = {C_ORANGE, C_YELLOW, C_GREEN, C_BLUE, C_VIOLET, C_PINK};
static const uint16_t KNOB_COLORS[4] = {C_BLUE, C_GREEN, C_YELLOW, C_ORANGE};
static const char *const MAC_SYN[4] = {"CUTOFF", "RESO", "DECAY", "WAVE"};
static const char *const MAC_SMP[4] = {"TONE", "TUNE", "DECAY", "START"};
static const char *const DLY_NAMES[4] = {"1/16", "1/8", "3/16", "1/4"};
static const char *const EXT_NAMES[5] = {"TRIADA", "+7", "+9", "SUS2", "SUS4"};
static const char *const ARP_MODES[5] = {"OFF", "UP", "DOWN", "UP-DN", "RAND"};
static const char *const ARP_RATES[3] = {"1/8", "1/16", "1/32"};

const char *bm_track_name(int t) { return TRACK_NAMES[t % BM_TRACKS]; }
uint16_t bm_track_color(int t) { return TRACK_COLORS[t % BM_TRACKS]; }
uint16_t bm_knob_color(int i) { return KNOB_COLORS[i & 3]; }
const char *bm_macro_name(int t, int i) { return t < 2 ? MAC_SMP[i & 3] : MAC_SYN[i & 3]; }

/* ------------------------------------------------------------ comandos */
static void push(bm_app *a, uint8_t type, uint8_t x, uint8_t y, uint8_t z)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.a = x; c.b = y; c.c = z;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_i(bm_app *a, uint8_t type, int32_t i)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = type; c.u.i = i;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_ev(bm_app *a, int t, int s, int k, const bm_event *e)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = CMD_SET_EVENT; c.a = (uint8_t)t; c.b = (uint8_t)s; c.c = (uint8_t)k; c.u.ev = *e;
    bm_cmd_push(&a->cmdq, &c);
}

static void push_bulk(bm_app *a, int t, const void *p)
{
    bm_cmd c;
    memset(&c, 0, sizeof c);
    c.type = CMD_TRACK_BULK; c.a = (uint8_t)t; c.u.ptr = p;
    bm_cmd_push(&a->cmdq, &c);
}

static void edited(bm_app *a) { a->dirty = 1; a->t_edit = a->now; }

static void set_track(bm_app *a, int t, uint8_t off, int v)
{
    push(a, CMD_SET_TRACK, (uint8_t)t, off, (uint8_t)v);
    edited(a);
}

static void set_meta(bm_app *a, uint8_t off, int v)
{
    push(a, CMD_SET_META, 0, off, (uint8_t)v);
    edited(a);
}

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

static int pages(const bm_app *a) { int p = a->pat.meta.length / BM_PAGE_STEPS; return p < 1 ? 1 : p; }

static int playing(const bm_app *a) { return a->ctx.st.playing; }

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

void bm_fmt_macro(const bm_app *a, int t, int i, int v, char *b, int cap)
{
    if (t < 2) {
        switch (i) {
        case 1: snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 12) / 64); break;
        case 2: fmt_ms(bm_decay_ms(v), b, cap); break;
        default: fmt_pct(v, b, cap); break;
        }
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

int bm_app_flash_events(const bm_app *a) { return bm_pattern_count_events(&a->pat); }

/* ------------------------------------------------------------ paso mantenido */
static int step_events(const bm_app *a, int s, int idx[4])
{
    int n = 0, t = a->track;
    for (int k = 0; k < BM_EVENTS; k++) {
        const bm_event *e = &a->pat.ev[t][s][k];
        if (bm_event_empty(e)) continue;
        if (t < 2 && e->note != a->drum_sel) continue;
        idx[n++] = k;
    }
    return n;
}

static bool lock_mode(const bm_app *a) { return a->held_step >= 0 && a->held_long; }

void bm_app_legend(const bm_app *a, bm_legend L[4])
{
    memset(L, 0, sizeof(bm_legend) * 4);
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    if (a->glo_down) {
        snprintf(L[0].name, 10, "BPM"); snprintf(L[0].val, 10, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10); L[0].pct = (uint8_t)((m->bpm_x10 - 400) * 255 / 2000);
        snprintf(L[1].name, 10, "SWING"); snprintf(L[1].val, 10, "%d%%", m->swing); L[1].pct = (uint8_t)((m->swing - 50) * 255 / 25);
        snprintf(L[2].name, 10, "LARGO"); snprintf(L[2].val, 10, "%d", m->length); L[2].pct = (uint8_t)(m->length * 255 / 64);
        snprintf(L[3].name, 10, "DRIVE"); fmt_pct(a->set.drive, L[3].val, 10); L[3].pct = (uint8_t)(a->set.drive * 2);
        return;
    }
    if (lock_mode(a)) {
        int idx[4];
        int n = step_events(a, a->held_step, idx);
        for (int i = 0; i < 4; i++) {
            int v = tr->macro[i];
            if (n && a->pat.ev[t][a->held_step][idx[0]].lk[i] != BM_LOCK_NONE) { v = a->pat.ev[t][a->held_step][idx[0]].lk[i]; L[i].locked = 1; }
            snprintf(L[i].name, 10, "%s", bm_macro_name(t, i));
            bm_fmt_macro(a, t, i, v, L[i].val, 10);
            L[i].pct = (uint8_t)(v * 2);
        }
        return;
    }
    switch (bm_app_eff_view(a)) {
    case V_HOME:
        snprintf(L[0].name, 10, "NIVEL"); snprintf(L[0].val, 10, "%d", tr->level); L[0].pct = (uint8_t)(tr->level * 2);
        snprintf(L[1].name, 10, "PAN"); fmt_pan(tr->pan, L[1].val, 10); L[1].pct = (uint8_t)(tr->pan * 2);
        snprintf(L[2].name, 10, "ENVIO"); fmt_pct(tr->send, L[2].val, 10); L[2].pct = (uint8_t)(tr->send * 2);
        snprintf(L[3].name, 10, "DUCK");
        if (t >= 2) { fmt_pct(tr->duck, L[3].val, 10); L[3].pct = (uint8_t)(tr->duck * 2); } else snprintf(L[3].val, 10, t == 0 ? "FUENTE" : "-");
        return;
    case V_ENV:
        if (a->env_page == 0) {
            snprintf(L[0].name, 10, "ATTACK"); fmt_ms(bm_attack_ms(tr->attack), L[0].val, 10); L[0].pct = (uint8_t)(tr->attack * 2);
            snprintf(L[1].name, 10, "DECAY"); fmt_ms(bm_decay_ms(tr->macro[2]), L[1].val, 10); L[1].pct = (uint8_t)(tr->macro[2] * 2);
            snprintf(L[2].name, 10, "SUSTAIN"); fmt_pct(tr->sustain, L[2].val, 10); L[2].pct = (uint8_t)(tr->sustain * 2);
            snprintf(L[3].name, 10, "RELEASE"); fmt_ms(bm_decay_ms(tr->release), L[3].val, 10); L[3].pct = (uint8_t)(tr->release * 2);
        } else {
            snprintf(L[0].name, 10, "ENV>FILT"); fmt_pct(tr->env_amt, L[0].val, 10); L[0].pct = (uint8_t)(tr->env_amt * 2);
            snprintf(L[1].name, 10, "GLIDE"); fmt_ms((uint32_t)tr->glide * 500 / 127, L[1].val, 10); L[1].pct = (uint8_t)(tr->glide * 2);
        }
        return;
    case V_FX:
        snprintf(L[0].name, 10, "DLY TIME"); snprintf(L[0].val, 10, "%s", DLY_NAMES[m->dly_time & 3]); L[0].pct = (uint8_t)((m->dly_time & 3) * 85);
        snprintf(L[1].name, 10, "FEEDBACK"); fmt_pct(m->dly_fb, L[1].val, 10); L[1].pct = (uint8_t)(m->dly_fb * 2);
        snprintf(L[2].name, 10, "DLY MIX"); fmt_pct(m->dly_mix, L[2].val, 10); L[2].pct = (uint8_t)(m->dly_mix * 2);
        snprintf(L[3].name, 10, "SC REL"); fmt_ms(bm_decay_ms(m->sc_release / 2), L[3].val, 10); L[3].pct = (uint8_t)(m->sc_release * 2);
        return;
    case V_SCL: case V_KEYMAP:
        snprintf(L[0].name, 10, "TONICA"); snprintf(L[0].val, 10, "%s", bm_note_names[m->root % 12]); L[0].pct = (uint8_t)(m->root * 23);
        snprintf(L[1].name, 10, "ESCALA"); snprintf(L[1].val, 10, "%.8s", bm_scale_names[m->scale % BM_SCALES]); L[1].pct = (uint8_t)(m->scale * 31);
        snprintf(L[2].name, 10, "ACORDE"); snprintf(L[2].val, 10, "%s", EXT_NAMES[m->voicing % 5]); L[2].pct = (uint8_t)(m->voicing * 63);
        snprintf(L[3].name, 10, "LOCK"); snprintf(L[3].val, 10, "%s", m->scale_lock ? "ON" : "OFF"); L[3].pct = m->scale_lock ? 255 : 0;
        return;
    case V_ARP: case V_ARPLIVE:
        snprintf(L[0].name, 10, "MODO"); snprintf(L[0].val, 10, "%s", ARP_MODES[tr->arp_mode % 5]); L[0].pct = (uint8_t)(tr->arp_mode * 63);
        snprintf(L[1].name, 10, "RATE"); snprintf(L[1].val, 10, "%s", ARP_RATES[tr->arp_rate % 3]); L[1].pct = (uint8_t)(tr->arp_rate * 127);
        snprintf(L[2].name, 10, "OCTAVAS"); snprintf(L[2].val, 10, "%d", tr->arp_oct); L[2].pct = (uint8_t)(tr->arp_oct * 85);
        snprintf(L[3].name, 10, "GATE"); snprintf(L[3].val, 10, "%d%%", tr->arp_gate); L[3].pct = (uint8_t)(tr->arp_gate * 255 / 100);
        return;
    case V_GLO:
        snprintf(L[0].name, 10, "VALOR"); snprintf(L[0].val, 10, "K1");
        return;
    case V_SAVE:
        snprintf(L[0].name, 10, "DESTINO"); snprintf(L[0].val, 10, "A%02d", a->save_target + 1); L[0].pct = (uint8_t)(a->save_target * 17);
        return;
    case V_SONG:
        if (a->song.n) {
            snprintf(L[0].name, 10, "PATRON"); snprintf(L[0].val, 10, "A%02d", a->song.pat[a->song_cur] + 1);
            snprintf(L[1].name, 10, "VECES"); snprintf(L[1].val, 10, "x%d", a->song.rep[a->song_cur]);
        }
        snprintf(L[3].name, 10, "SONG"); snprintf(L[3].val, 10, "%s", a->ctx.st.song_on ? "ON" : "OFF"); L[3].pct = a->ctx.st.song_on ? 255 : 0;
        return;
    case V_LFO: case V_BOOT:
        return;
    default:
        for (int i = 0; i < 4; i++) {
            snprintf(L[i].name, 10, "%s", bm_macro_name(t, i));
            bm_fmt_macro(a, t, i, tr->macro[i], b, sizeof b);
            snprintf(L[i].val, 10, "%s", b);
            L[i].pct = (uint8_t)(tr->macro[i] * 2);
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

/* Cambio de patrón: corre en la ISR de audio (ver bm_audio.h). Sin escrituras en flash. */
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
        if (src && len > 0 && bm_pattern_unpack(p, src, len) > 0) {
            a->dirty = 0;
        } else {
            for (int t = 0; t < BM_TRACKS; t++) bm_track_clear_events(p, t);   /* vacío: hereda sonidos */
            a->dirty = 0;
        }
    }
    a->set.last_pat = (uint8_t)to;
    a->set_dirty = 1;
    return 0;
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
        if (n < 0) return -1;
        int obj = type == JOB_SAVE_AS ? arg : a->ctx.st.pattern;
        if (bm_store_write(&a->store, (uint8_t)obj, a->pend, (uint16_t)n)) return -1;
        if (type == JOB_SAVE_AS) { a->ctx.cur_pat = (uint8_t)obj; a->ctx.st.pattern = (uint8_t)obj; }
        a->dirty = 0;
        return 0;
    }
    case JOB_SAVE_SONG: {
        uint8_t b[1 + 2 * BM_SONG_MAX];
        b[0] = a->song.n;
        memcpy(b + 1, a->song.pat, BM_SONG_MAX);
        memcpy(b + 1 + BM_SONG_MAX, a->song.rep, BM_SONG_MAX);
        if (bm_store_write(&a->store, BM_OBJ_SONG, b, sizeof b)) return -1;
        a->song_dirty = 0;
        return 0;
    }
    case JOB_SAVE_GLOB:
        a->set.ver = 1;
        if (bm_store_write_glob(&a->store, (const uint8_t *)&a->set, sizeof a->set)) return -1;
        a->set_dirty = 0;
        return 0;
    case JOB_LOAD:
        push(a, CMD_LOAD, (uint8_t)arg, 0, 0);
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
        if (a->cmdq.r != a->cmdq.w) return;                 /* que el audio aplique todo antes */
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
    bm_event *e = &p->ev[t][s][k];
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = (uint8_t)prob;
}

static void demo_pattern(bm_pattern *p)
{
    bm_pattern_init(p);
    p->meta.bpm_x10 = 1120; p->meta.swing = 56; p->meta.root = 9; p->meta.scale = 1;
    static const int prog[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 52, 55}, {55, 59, 62}};   /* Am F C G */
    static const int bass[4] = {45, 41, 36, 43};
    for (int pg = 0; pg < 4; pg++) {
        int o = pg * 16;
        for (int s = 0; s < 16; s += 4) ev_set(p, 0, o + s, 0, 0, 120, 1, 100);
        ev_set(p, 0, o + 4, 1, 4, 110, 1, 100);
        ev_set(p, 0, o + 12, 1, 4, 110, 1, 100);
        if (pg == 3) ev_set(p, 0, o + 12, 2, 5, 100, 1, 100);
        for (int s = 2; s < 16; s += 4) ev_set(p, 0, o + s, 2, 2, 90, 1, 100);
        ev_set(p, 0, o + 14, 3, 3, 80, 1, 100);
        ev_set(p, 0, o + 7, 2, 2, 60, 1, 50);
        ev_set(p, 0, o + 15, 1, 2, 50, 1, 35);
        int bn = bass[pg];
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
        ev_set(p, 4, o + 12, 0, prog[pg][1] + 12, 60, 2, 80);
        ev_set(p, 5, o + 0, 0, prog[pg][0] - 12, 60, 16, 100);
    }
    for (int pg = 0; pg < 4; pg++) p->ev[0][pg * 16 + 12][0].lk[1] = 84;   /* P-Lock de ejemplo: bombo del paso 13 más agudo */
    p->trk[3].level = 70; p->trk[4].level = 60; p->trk[5].level = 55;
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
    s->ver = 1; s->clock = 0; s->sync_mode = 0; s->sync_ppqn = 2; s->sync_lat = 0;
    s->drive = 25; s->led_dim = 1; s->autosave = 1; s->last_pat = 0;
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
    a->pend_obj = -1; a->undo_track = -1; a->held_step = -1; a->held_white = -1;
    a->pat_sel = -1; a->punch_key = -1; a->last_degree = -1;
    static const uint8_t ln[BM_TRACKS] = {0, 0, 45, 57, 60, 64};
    memcpy(a->last_note, ln, sizeof ln);
    for (int k = 0; k < BM_KEYS; k++) a->key_rec_step[k] = -1;
    settings_defaults(&a->set);
    a->battery = 100;

    int fresh = 1;
    if (pf->flash && bm_store_mount(&a->store, pf->flash) == 0) {
        bm_settings s;
        if (bm_store_read_glob(&a->store, (uint8_t *)&s, sizeof s) == (int)sizeof s && s.ver == 1) a->set = s;
        uint8_t b[1 + 2 * BM_SONG_MAX];
        if (bm_store_read(&a->store, BM_OBJ_SONG, b, sizeof b) == (int)sizeof b) {
            a->song.n = b[0] > BM_SONG_MAX ? BM_SONG_MAX : b[0];
            memcpy(a->song.pat, b + 1, BM_SONG_MAX);
            memcpy(a->song.rep, b + 1 + BM_SONG_MAX, BM_SONG_MAX);
        }
        for (int i = 0; i < BM_PATTERNS; i++) if (bm_store_has(&a->store, (uint8_t)i)) fresh = 0;
        int len = 0;
        const uint8_t *src = store_ptr(a, a->set.last_pat % BM_PATTERNS, &len);
        if (src && len > 0) bm_pattern_unpack(&a->pat, src, len);
    }
    if (fresh) { demo_pattern(&a->pat); a->dirty = 1; a->t_edit = now; }
    a->set.last_pat %= BM_PATTERNS;
    a->ctx.cur_pat = a->set.last_pat;
    a->ctx.st.pattern = a->set.last_pat;
    bm_seq_set_bpm(&a->ctx, a->pat.meta.bpm_x10);
    apply_settings(a);
    a->view = V_BOOT;
    a->boot_t = now;
    a->scr.force = 1;
}

void bm_app_audio(bm_app *a, int32_t *out, uint32_t frames) { bm_audio_render(&a->ctx, out, frames); }

void bm_app_midi_in(bm_app *a, uint8_t b) { push(a, CMD_MIDI, b, 0, 0); }

void bm_app_battery(bm_app *a, uint8_t pct) { a->battery = pct > 100 ? 100 : pct; }

void bm_app_master(bm_app *a, uint16_t adc)
{
    a->master_adc = (uint16_t)((a->master_adc * 3u + adc) / 4u);      /* media móvil */
    int d = (int)a->master_adc - (int)a->master_sent;
    if (d > 4 || d < -4 || (adc == 0 && a->master_sent) || (adc >= 1023 && a->master_sent < 1023)) {
        a->master_sent = (adc == 0 || adc >= 1023) ? adc : a->master_adc;   /* histéresis ±4 */
        push_i(a, CMD_MASTER, a->master_sent);
    }
}

/* ------------------------------------------------------------ vistas */
static void set_view(bm_app *a, int v)
{
    a->view = (uint8_t)v;
    a->ov.on = 0;
    a->clear_armed = 0;
}

static void undo_snapshot(bm_app *a, int t)
{
    memcpy(a->undo_ev, a->pat.ev[t], sizeof a->undo_ev);
    a->undo_track = (int8_t)t;
}

static void select_track(bm_app *a, int t)
{
    if (t < 0 || t >= BM_TRACKS) return;
    a->track = (uint8_t)t;
    char b[24];
    snprintf(b, sizeof b, "T%d %s", t + 1, TRACK_NAMES[t]);
    toast(a, b);
}

static void toggle_mute(bm_app *a, int t)
{
    set_track(a, t, TRK_OFF(mute), a->pat.trk[t].mute ? 0 : 1);
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
        toast(a, b);
    } else {
        if (idx == a->ctx.st.pattern) return;
        save_jobs(a, a->dirty);
        job_push(a, JOB_LOAD, idx);
        snprintf(b, sizeof b, "PATRON A%02d", idx + 1);
        toast(a, b);
    }
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
        toast(a, "SONG GUARDADA");
        break;
    case 3:
        if (a->clear_armed && a->now - a->clear_t < 3000) {
            for (int t = 0; t < BM_TRACKS; t++) push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
            edited(a);
            a->clear_armed = 0;
            toast(a, "PATRON BORRADO");
        } else {
            a->clear_armed = 1; a->clear_t = a->now;
            toast(a, "SAVE OTRA VEZ = BORRAR");
        }
        break;
    default:
        snprintf(b, sizeof b, "%d/%d EVENTOS", bm_pattern_count_events(&a->pat), BM_PAT_MAX_EVENTS);
        toast(a, b);
        break;
    }
}

static void on_gesture(void *u, int id, bm_gkind k)
{
    bm_app *a = (bm_app *)u;
    if (a->view == V_BOOT) return;
    char b[30];
    switch (id) {
    case BTN_PLAY:
        if (k == G_PRESS) push(a, CMD_PLAY, a->glo_down ? 2 : (playing(a) ? 0 : 1), 0, 0);
        return;
    case BTN_REC:
        if (k == G_PRESS) {
            if (a->view == V_EDIT && a->track == 1) { push(a, CMD_SMP_REC, 1, 0, 0); a->smp_was_rec = 1; }
            else {
                a->rec = !a->rec;
                if (a->rec) undo_snapshot(a, a->track);
                toast(a, a->rec ? "REC ON" : "REC OFF");
            }
        } else if (k == G_UP && a->smp_was_rec) {
            push(a, CMD_SMP_REC, 0, 0, 0);
            a->smp_was_rec = 0;
        }
        return;
    case BTN_OCT_DN: case BTN_OCT_UP:
        if (k != G_PRESS) return;
        {
            int d = id == BTN_OCT_UP ? 1 : -1;
            if (a->glo_down) {
                int tp = bm_clampi(a->pat.trk[a->track].transpose + d, 0, 48);
                set_track(a, a->track, TRK_OFF(transpose), tp);
                snprintf(b, sizeof b, "TRANSPONER %+d", tp - 24);
                toast(a, b);
            } else if (bm_app_eff_view(a) == V_SEQ) {
                a->page = (uint8_t)bm_clampi(a->page + d, 0, pages(a) - 1);
                snprintf(b, sizeof b, "PAGINA %d/%d", a->page + 1, pages(a));
                toast(a, b);
            } else {
                a->oct = (int8_t)bm_clampi(a->oct + d, -2, 2);
                snprintf(b, sizeof b, "OCTAVA %+d", a->oct);
                toast(a, b);
            }
        }
        return;
    case BTN_GLO:
        if (k == G_PRESS) a->glo_down = 1;
        else if (k == G_UP) a->glo_down = 0;
        else if (k == G_TAP) set_view(a, a->view == V_GLO ? V_HOME : V_GLO);
        return;
    case BTN_SAVE:
        if (k == G_TAP) { if (a->view == V_SAVE) save_exec(a); else set_view(a, V_SAVE); }
        else if (k == G_HOLD) {
            save_jobs(a, 1);
            toast(a, playing(a) ? "PARA (STOP) PARA GUARDAR" : "GUARDADO RAPIDO");
        }
        return;
    default: break;
    }
    if (k == G_TAP) {
        int v = V_HOME;
        switch (id) {
        case BTN_HOME: v = V_HOME; break;
        case BTN_SEQ: v = a->glo_down ? V_SONG : V_SEQ; break;
        case BTN_EDIT: v = V_EDIT; break;
        case BTN_ENV: if (a->view == V_ENV) a->env_page ^= 1; v = V_ENV; break;
        case BTN_LFO: v = V_LFO; break;
        case BTN_FX: v = V_FX; break;
        case BTN_SCL: v = V_SCL; break;
        case BTN_ARP: v = V_ARP; break;
        default: return;
        }
        set_view(a, v);
    } else if (k == G_HOLD) {
        switch (id) {
        case BTN_SEQ: a->layer = V_BANK; break;
        case BTN_HOME: a->layer = V_MUTE; break;
        case BTN_FX: a->layer = V_PUNCH; break;
        case BTN_SCL: a->layer = V_KEYMAP; break;
        case BTN_ARP:
            a->layer = V_ARPLIVE;
            if (a->track >= 2) push(a, CMD_ARP, a->track, 1, 0); else toast(a, "ARP: SOLO PISTAS SYNTH");
            break;
        default: break;
        }
        a->ov.on = 0;
    } else if (k == G_RELEASE) {
        if (id == BTN_FX) { push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
        if (id == BTN_ARP) push(a, CMD_ARP, a->track, 0, 0);
        a->layer = 0;
    }
}

void bm_app_button(bm_app *a, int id, int down, uint32_t now)
{
    a->now = now;
    if (down) bm_gesture_down(&a->ges, id, now);
    else bm_gesture_up(&a->ges, id, now);
}

/* ------------------------------------------------------------ teclas */
static void rec_note(bm_app *a, int k, int t, const uint8_t *notes, int n)
{
    int len = a->pat.meta.length;
    int tick = a->ctx.st.tick;
    int s = ((tick + BM_TICKS_STEP / 2) / BM_TICKS_STEP) % len;
    a->key_rec_step[k] = (int8_t)s;
    a->key_rec_tick[k] = (uint16_t)tick;
    a->key_rec_slot[k] = -1;
    for (int i = 0; i < n; i++) {
        int slot = -1;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = &a->pat.ev[t][s][e];
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

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel)
{
    for (int i = 0; i < n; i++) push(a, CMD_NOTE_ON, (uint8_t)t, notes[i], (uint8_t)vel);
    memcpy(a->key_notes[k], notes, (size_t)n);
    a->key_nn[k] = (uint8_t)n;
    a->key_trk[k] = (uint8_t)t;
    if (n) a->last_note[t] = notes[0];
    a->key_rec_step[k] = -1;
    if (a->rec && playing(a) && a->view != V_SEQ) rec_note(a, k, t, notes, n);
}

static int chord_ext(const bm_app *a)
{
    if (a->chord_mod & 1) return 1;
    if (a->chord_mod & 2) return 2;
    if (a->chord_mod & 4) return 3;
    if (a->chord_mod & 8) return 4;
    return a->pat.meta.voicing % 5;
}

static void play_key_down(bm_app *a, int k, int w, int b, int v)
{
    int t = a->track;
    const bm_meta *m = &a->pat.meta;
    if (t < 2) {
        if (w >= 0) {
            uint8_t n = (uint8_t)w;
            if (t == 0 || v != V_SEQ) a->drum_sel = (uint8_t)w;
            play_notes(a, k, t, &n, 1, 110);
        } else if (b < 6) select_track(a, b);
        return;
    }
    int base_oct = (t == 2 ? 2 : 3) + a->oct;
    bool scale = v == V_SCL || v == V_KEYMAP || m->scale_lock;
    if (scale) {
        if (w >= 0) {
            uint8_t n = (uint8_t)bm_scale_note(m->root, m->scale, w, base_oct);
            play_notes(a, k, t, &n, 1, 105);
        } else if (b < 7) {
            uint8_t notes[4];
            int ext = chord_ext(a);
            int n = bm_chord_notes(m->root, m->scale, b, ext, base_oct, notes);
            bm_chord_name(m->root, m->scale, b, ext, a->chord_name, sizeof a->chord_name);
            a->last_degree = (int8_t)b;
            play_notes(a, k, t, notes, n, 95);
        } else {
            a->chord_mod |= (uint8_t)(1u << (b - 7));
        }
        return;
    }
    int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
    uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
    play_notes(a, k, t, &n, 1, 105);
}

static void seq_key_down(bm_app *a, int w, int b)
{
    int t = a->track;
    char msg[30];
    if (b >= 0) {
        if (b < 6) { select_track(a, b); return; }
        if (b == 6) {                                   /* COPY */
            if (lock_mode(a)) { memcpy(a->clip_step, a->pat.ev[t][a->held_step], sizeof a->clip_step); a->clip_type = 1; toast(a, "PASO COPIADO"); }
            else { memcpy(a->clip_ev, a->pat.ev[t], sizeof a->clip_ev); a->clip_trk = a->pat.trk[t]; a->clip_type = 2; toast(a, "PISTA COPIADA"); }
        } else if (b == 7) {                            /* PASTE */
            undo_snapshot(a, t);
            if (a->held_step >= 0 && a->clip_type == 1) {
                for (int e = 0; e < BM_EVENTS; e++) push_ev(a, t, a->held_step, e, &a->clip_step[e]);
                a->held_edit = 1; toast(a, "PASO PEGADO");
            } else if (a->clip_type == 2) {
                push_bulk(a, t, a->clip_ev);
                for (int i = 0; i < 4; i++) push(a, CMD_SET_TRACK, (uint8_t)t, (uint8_t)i, a->clip_trk.macro[i]);
                toast(a, "PISTA PEGADA");
            } else { toast(a, "PORTAPAPELES VACIO"); return; }
            edited(a);
        } else if (b == 8) {                            /* CLEAR */
            undo_snapshot(a, t);
            if (a->held_step >= 0) {
                bm_event e; bm_event_clear(&e);
                for (int k = 0; k < BM_EVENTS; k++) push_ev(a, t, a->held_step, k, &e);
                a->held_edit = 1; toast(a, "PASO BORRADO");
            } else { push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0); toast(a, "PISTA BORRADA"); }
            edited(a);
        } else if (b == 9) {                            /* FILL */
            push(a, CMD_FILL, 1, 0, 0); a->fill = 1;
        } else if (b == 10) {                           /* UNDO */
            if (a->undo_track >= 0) { push_bulk(a, a->undo_track, a->undo_ev); edited(a); a->undo_track = -1; toast(a, "DESHECHO"); }
            else toast(a, "NADA QUE DESHACER");
        }
        return;
    }
    if (a->held_white >= 0) return;                     /* un paso a la vez */
    int s = a->page * BM_PAGE_STEPS + w;
    if (s >= a->pat.meta.length) { toast(a, "FUERA DEL LARGO"); return; }
    undo_snapshot(a, t);
    a->held_white = (int8_t)w;
    a->held_step = (int8_t)s;
    a->held_long = 0; a->held_edit = 0;
    a->step_t0 = a->now;
    int idx[4];
    int n = step_events(a, s, idx);
    a->held_had_event = n > 0;
    if (!n) {
        int slot = -1;
        for (int k = 0; k < BM_EVENTS; k++) if (bm_event_empty(&a->pat.ev[t][s][k])) { slot = k; break; }
        if (slot < 0) { toast(a, "PASO LLENO (4 EVENTOS)"); return; }
        bm_event e; bm_event_clear(&e);
        e.note = t < 2 ? a->drum_sel : a->last_note[t];
        e.vel = 100;
        push_ev(a, t, s, slot, &e);
        edited(a);
        (void)msg;
    }
}

static void seq_key_up(bm_app *a, int w)
{
    if (w != a->held_white) return;
    int t = a->track, s = a->held_step;
    if (s >= 0 && a->held_had_event && !a->held_long && !a->held_edit) {
        bm_event e; bm_event_clear(&e);
        int idx[4];
        int n = step_events(a, s, idx);
        for (int i = 0; i < n; i++) push_ev(a, t, s, idx[i], &e);
        edited(a);
    }
    a->held_white = -1; a->held_step = -1; a->held_long = 0; a->held_edit = 0;
}

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
        for (int i = 0; i < a->key_nn[k]; i++) push(a, CMD_NOTE_OFF, (uint8_t)t, a->key_notes[k][i], 0);
        if (a->key_rec_step[k] >= 0 && a->key_rec_slot[k] >= 0) {
            int len = a->pat.meta.length * BM_TICKS_STEP;
            int d = ((int)a->ctx.st.tick - a->key_rec_tick[k] + len) % len;
            int steps = bm_clampi((d + BM_TICKS_STEP / 2) / BM_TICKS_STEP, 1, 16);
            int s = a->key_rec_step[k];
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = a->pat.ev[t][s][e];
                for (int i = 0; i < a->key_nn[k]; i++)
                    if (!bm_event_empty(&ev) && ev.note == a->key_notes[k][i]) { ev.len = (uint8_t)steps; push_ev(a, t, s, e, &ev); }
            }
        }
        a->key_nn[k] = 0;
        a->key_rec_step[k] = -1;
    }
    if (a->punch_key == k) { push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    if (b >= 7 && b <= 10) a->chord_mod &= (uint8_t)~(1u << (b - 7));
    if (b == 9 && a->fill) { push(a, CMD_FILL, 0, 0, 0); a->fill = 0; }
    if (w >= 0) seq_key_up(a, w);
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
    switch (v) {
    case V_BANK: if (w >= 0) select_pattern(a, w); return;
    case V_MUTE:
        if (w >= 0 && w < 6) toggle_mute(a, w);
        else if (w >= 8 && w < 14) solo(a, w - 8);
        return;
    case V_PUNCH:
        if (w >= 0) { push(a, CMD_PUNCH, (uint8_t)w, 0, 0); a->punch_key = (int8_t)k; }
        return;
    case V_SONG:
        if (w >= 0) song_append(a, w);
        else if (b == 8) song_delete(a);
        return;
    case V_SEQ:
        seq_key_down(a, w, b);
        return;
    default: break;
    }
    if (v == V_EDIT && a->track == 1 && a->ctx.st.smp_rec) {
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

static void enc_lock(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step;
    int idx[4];
    int n = step_events(a, s, idx);
    if (!n) { toast(a, "PASO VACIO"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[12], ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        bm_event e0 = a->pat.ev[t][s][idx[0]];
        int base = e0.lk[i] != BM_LOCK_NONE ? e0.lk[i] : a->pat.trk[t].macro[i];
        int v = bm_clampi(base + d, 0, 127);
        for (int j = 0; j < n; j++) {
            bm_event e = a->pat.ev[t][s][idx[j]];
            e.lk[i] = (uint8_t)v;
            push_ev(a, t, s, idx[j], &e);
        }
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, bm_macro_name(t, i));
        overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
    } else {
        for (int j = 0; j < n; j++) {
            bm_event e = a->pat.ev[t][s][idx[j]];
            if (id == ENC_SELECT) {
                if (t < 2) { e.note = (uint8_t)bm_clampi(e.note + d, 0, 15); if (j == 0) a->drum_sel = e.note; }
                else e.note = (uint8_t)bm_clampi(e.note + d, 0, 127);
                if (j == 0) { if (t >= 2) { a->last_note[t] = e.note; bm_note_str(e.note, b, sizeof b); } else snprintf(b, sizeof b, "%s", t == 0 ? bm_rom_kits[a->pat.trk[0].source % BM_KITS][e.note].name : "SLICE"); }
            } else if (id == ENC_ALGO) {
                e.prob = (uint8_t)bm_clampi(e.prob + d * 5, 0, 100);
                if (j == 0) snprintf(b, sizeof b, "%d%%", e.prob);
            } else if (id == ENC_PRESETS) {
                int cur = (e.len & BM_LEN_TIE) ? 17 : (e.len & BM_LEN_MASK);
                cur = bm_clampi(cur + d, 1, 17);
                e.len = cur == 17 ? (uint8_t)(1 | BM_LEN_TIE) : (uint8_t)cur;
                if (j == 0) { if (cur == 17) snprintf(b, sizeof b, "TIE"); else snprintf(b, sizeof b, "%d PASOS", cur); }
            }
            push_ev(a, t, s, idx[j], &e);
        }
        const char *nm = id == ENC_SELECT ? (t < 2 ? "SONIDO" : "NOTA") : id == ENC_ALGO ? "PROBABILIDAD" : "LARGO";
        snprintf(ti, sizeof ti, "PASO %02d %s", s + 1, nm);
        overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
    }
    edited(a);
}

static void glo_menu_change(bm_app *a, int d)
{
    bm_settings *s = &a->set;
    switch (a->menu_row) {
    case 0: s->clock = !s->clock; push(a, CMD_CLOCK, s->clock, 0, 0); break;
    case 1: s->sync_mode = !s->sync_mode; break;
    case 2: s->sync_ppqn = s->sync_ppqn == 1 ? (d > 0 ? 2 : 4) : s->sync_ppqn == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 1 : 2); break;
    case 3: s->sync_lat = (uint8_t)bm_clampi(s->sync_lat + d, 0, 20); break;
    case 4: s->drive = (uint8_t)bm_clampi(s->drive + d, 0, 127); push(a, CMD_DRIVE, s->drive, 0, 0); break;
    case 5: s->autosave = !s->autosave; break;
    case 6: s->led_dim = (uint8_t)bm_clampi(s->led_dim + d, 0, 2); break;
    default: return;
    }
    push(a, CMD_SYNC, s->sync_mode, s->sync_ppqn, s->sync_lat);
    a->set_dirty = 1; a->t_edit = a->now;
}

void bm_app_encoder(bm_app *a, int id, int d, uint32_t now)
{
    a->now = now;
    if (!d || a->view == V_BOOT) return;
    bm_gesture_other(&a->ges, now);
    if (d >= 3 || d <= -3) d *= 4;                      /* aceleración */
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    bm_meta *m = &a->pat.meta;
    char b[16];

    if (a->glo_down) {
        switch (id) {
        case ENC_K1: {
            int bpm = bm_clampi(m->bpm_x10 + d * 10, 400, 2400);
            push_i(a, CMD_SET_BPM, bpm); edited(a);
            snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
            overlay(a, 0, "TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, bm_knob_color(0));
            return;
        }
        case ENC_K2: {
            int sw = bm_clampi(m->swing + (d > 0 ? 1 : -1), 50, 75);
            set_meta(a, META_OFF(swing), sw);
            snprintf(b, sizeof b, "%d", sw);
            overlay(a, 1, "SWING  50=RECTO", b, "%", (sw - 50) * 255 / 25, -1, bm_knob_color(1));
            return;
        }
        case ENC_K3: {
            int len = bm_clampi(m->length + (d > 0 ? 16 : -16), 16, 64);
            set_meta(a, META_OFF(length), len);
            if (a->page >= len / 16) a->page = (uint8_t)(len / 16 - 1);
            snprintf(b, sizeof b, "%d", len);
            overlay(a, 2, "LARGO DEL PATRON", b, "PASOS", len * 255 / 64, -1, bm_knob_color(2));
            return;
        }
        case ENC_K4: {
            a->set.drive = (uint8_t)bm_clampi(a->set.drive + d, 0, 127);
            push(a, CMD_DRIVE, a->set.drive, 0, 0);
            a->set_dirty = 1; a->t_edit = now;
            fmt_pct(a->set.drive, b, sizeof b);
            overlay(a, 3, "DRIVE (SOFT-CLIP)", b, "", a->set.drive * 2, -1, bm_knob_color(3));
            return;
        }
        case ENC_ALGO: {
            int lim = t == 0 ? BM_KITS : t == 1 ? 2 : BM_WAVES;
            int v = (tr->source + (d > 0 ? 1 : lim - 1)) % lim;
            set_track(a, t, TRK_OFF(source), v);
            if (t == 1) set_track(a, t, TRK_OFF(interp), v);
            const char *nm = t == 0 ? bm_rom_kit_names[v] : t == 1 ? (v ? "HERMITE" : "DROP") : bm_rom_wave_names[v];
            overlay(a, -1, t == 0 ? "KIT" : t == 1 ? "INTERPOLACION" : "BANCO DE ONDAS", nm, "", v * 255 / (lim - 1 ? lim - 1 : 1), -1, bm_track_color(t));
            return;
        }
        default: return;
        }
    }

    if (a->held_step >= 0 && (id == ENC_K1 || id == ENC_K2 || id == ENC_K3 || id == ENC_K4 || id == ENC_SELECT || id == ENC_ALGO || id == ENC_PRESETS)
        && bm_app_eff_view(a) == V_SEQ) {
        enc_lock(a, id, d);
        return;
    }

    if (id == ENC_ALGO) {                               /* pista */
        select_track(a, bm_clampi(t + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1));
        return;
    }
    if (id == ENC_PRESETS) {                            /* patrón */
        int base = a->pat_sel >= 0 ? a->pat_sel : a->ctx.st.pattern;
        a->pat_sel = (int8_t)bm_clampi(base + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
        a->pat_sel_t = now;
        snprintf(b, sizeof b, "A%02d", a->pat_sel + 1);
        overlay(a, -1, playing(a) ? "PATRON (SE ENCOLA)" : "PATRON", b, "", a->pat_sel * 17, -1, C_WHITE);
        return;
    }

    int v = bm_app_eff_view(a);
    int k = id - ENC_K1;
    switch (v) {
    case V_HOME:
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
    case V_ENV:
        if (k < 0 || k > 3) break;
        {
            char ti[22];
            if (a->env_page == 0) {
                static const uint8_t OFFS[4] = {TRK_OFF(attack), 2, TRK_OFF(sustain), TRK_OFF(release)};
                const uint8_t cur[4] = {tr->attack, tr->macro[2], tr->sustain, tr->release};
                int nv = bm_clampi(cur[k] + d, 0, 127);
                set_track(a, t, OFFS[k], nv);
                if (k == 0) fmt_ms(bm_attack_ms(nv), b, sizeof b);
                else if (k == 2) fmt_pct(nv, b, sizeof b);
                else fmt_ms(bm_decay_ms(nv), b, sizeof b);
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
    case V_FX:
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
    case V_SCL: case V_KEYMAP:
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
    case V_GLO:
        if (id == ENC_SELECT) a->menu_row = (uint8_t)bm_clampi(a->menu_row + (d > 0 ? 1 : -1), 0, 9);
        else if (id == ENC_K1) glo_menu_change(a, d > 0 ? 1 : -1);
        return;
    case V_SAVE:
        if (id == ENC_SELECT) a->save_row = (uint8_t)bm_clampi(a->save_row + (d > 0 ? 1 : -1), 0, 4);
        else if (id == ENC_K1) a->save_target = (uint8_t)bm_clampi(a->save_target + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
        a->clear_armed = 0;
        return;
    case V_SONG:
        if (id == ENC_SELECT && a->song.n) a->song_cur = (uint8_t)bm_clampi(a->song_cur + (d > 0 ? 1 : -1), 0, a->song.n - 1);
        else if (id == ENC_K1 && a->song.n) { a->song.pat[a->song_cur] = (uint8_t)bm_clampi(a->song.pat[a->song_cur] + (d > 0 ? 1 : -1), 0, 15); a->song_dirty = 1; a->t_edit = now; }
        else if (id == ENC_K2 && a->song.n) { a->song.rep[a->song_cur] = (uint8_t)bm_clampi(a->song.rep[a->song_cur] + (d > 0 ? 1 : -1), 1, 16); a->song_dirty = 1; a->t_edit = now; }
        else if (id == ENC_K4) { push(a, CMD_SONG, a->ctx.st.song_on ? 0 : 1, 0, 0); toast(a, a->ctx.st.song_on ? "SONG OFF" : "SONG ON"); }
        return;
    case V_LFO:
        toast(a, "LFO RESERVADO (DECISION 5)");
        return;
    default: break;
    }
    if (id == ENC_SELECT) {
        if (t < 2 && (v == V_SEQ || v == V_EDIT)) {
            a->drum_sel = (uint8_t)bm_clampi(a->drum_sel + (d > 0 ? 1 : -1), 0, 15);
            const char *nm = t == 0 ? bm_rom_kits[tr->source % BM_KITS][a->drum_sel].name : "";
            if (t == 1) { snprintf(b, sizeof b, "SLICE %d", a->drum_sel + 1); nm = b; }
            overlay(a, -1, t == 0 ? "SONIDO A SECUENCIAR" : "SLICE A SECUENCIAR", nm, "", a->drum_sel * 17, -1, bm_track_color(t));
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
        [V_HOME] = BTN_HOME, [V_SEQ] = BTN_SEQ, [V_EDIT] = BTN_EDIT, [V_ENV] = BTN_ENV, [V_LFO] = BTN_LFO,
        [V_FX] = BTN_FX, [V_SCL] = BTN_SCL, [V_ARP] = BTN_ARP, [V_SAVE] = BTN_SAVE, [V_GLO] = BTN_GLO,
        [V_SONG] = BTN_SEQ, [V_BANK] = BTN_SEQ, [V_MUTE] = BTN_HOME, [V_PUNCH] = BTN_FX, [V_KEYMAP] = BTN_SCL,
        [V_ARPLIVE] = BTN_ARP};
    for (int i = 0; i < BTN_COUNT; i++) {
        uint8_t l = LED_DIM;
        if (i == VB[v] && VB[v]) l = LED_ON;
        if (i == BTN_PLAY) l = st->playing ? ((st->tick % BM_PPQN) < 24 ? LED_ON : LED_DIM) : LED_DIM;
        if (i == BTN_REC) l = st->smp_rec ? LED_BLINK : (a->rec ? LED_ON : LED_DIM);
        if (i == BTN_GLO && a->glo_down) l = LED_ON;
        if ((i == BTN_OCT_DN && a->oct < 0) || (i == BTN_OCT_UP && a->oct > 0)) l = LED_ON;
        led(a, i, l);
    }
    int t = a->track;
    int play_step = st->playing ? st->step : -1;
    for (int k = 0; k < BM_KEYS; k++) {
        int w = bm_key_white_index(k), b = bm_key_black_index(k);
        uint8_t l = LED_OFF;
        if (a->keys_down[k]) l = LED_ON;
        else if (v == V_SEQ) {
            if (w >= 0) {
                int s = a->page * BM_PAGE_STEPS + w;
                if (s >= a->pat.meta.length) l = LED_OFF;
                else {
                    int idx[4];
                    int n = step_events(a, s, idx);
                    int locked = 0;
                    for (int j = 0; j < n; j++) for (int i = 0; i < 4; i++) if (a->pat.ev[t][s][idx[j]].lk[i] != BM_LOCK_NONE) locked = 1;
                    l = n ? (locked ? LED_BREATH : LED_ON) : LED_DIM;
                    if (s == play_step) l = LED_BLINK;
                }
            } else {
                if (b < 6) l = b == t ? LED_ON : (a->pat.trk[b].mute ? LED_OFF : LED_DIM);
                else l = LED_DIM;
            }
        } else if (v == V_BANK) {
            if (w >= 0) l = w == st->pattern ? LED_ON : (w == st->queued ? LED_BLINK : (bm_store_has(&a->store, (uint8_t)w) ? LED_DIM : LED_OFF));
        } else if (v == V_MUTE) {
            if (w >= 0 && w < 6) l = a->pat.trk[w].mute ? LED_OFF : LED_ON;
            else if (w >= 8 && w < 14) l = LED_DIM;
        } else if (v == V_PUNCH) {
            if (w >= 0) l = (st->punch == w) ? LED_ON : LED_DIM;
        } else if ((v == V_SCL || v == V_KEYMAP || (a->pat.meta.scale_lock && t >= 2)) && t >= 2) {
            if (w >= 0) l = (w % bm_scale_len(a->pat.meta.scale)) == 0 ? LED_ON : LED_DIM;
            else if (b < 7) l = LED_DIM;
        } else if (t < 2 && w >= 0) {
            l = (t == 0 && w == a->drum_sel) ? LED_ON : LED_DIM;
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
    if (a->pat_sel >= 0 && now - a->pat_sel_t > 500) { select_pattern(a, a->pat_sel); a->pat_sel = -1; }
    if (a->ov.on && (int32_t)(now - a->ov.until) > 0) a->ov.on = 0;
    if (a->toast_until && (int32_t)(now - a->toast_until) > 0) a->toast_until = 0;
    for (int t = 0; t < BM_TRACKS; t++)
        if (st->trig[t] != a->last_trig[t]) { a->last_trig[t] = st->trig[t]; a->trig_until[t] = now + 90; }
    if (st->clip) { st->clip = 0; a->clip_until = now + 200; }
    if (st->swap_fail) { st->swap_fail = 0; toast(a, "SIN CAMBIO: PARA Y GUARDA"); }
    if (a->smp_was_rec && !st->smp_rec && a->ctx.smp_len) { a->smp_was_rec = 0; toast(a, "BUFFER LLENO"); }
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
