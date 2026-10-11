/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — entrada (DISENO_3.0): cuatro gestos por botón, HOME en todas partes,
 * ALGORITHM que navega, teclas por página y capas, perillas y su leyenda. */
#include <string.h>
#include <stdio.h>
#include "bm_app_int.h"
#include "bm_util.h"
#include "bm_rom.h"

static const char *const MAC_SYN[4] = {"CUTOFF", "RESO", "DECAY", "ONDA"};
static const char *const MAC_ACID[4] = {"CUTOFF", "RESO", "DECAY", "ENV MOD"};
static const char *const MAC_VA[4] = {"CUTOFF", "RESO", "DECAY", "DETUNE"};
static const char *const MAC_TR[4] = {"TONE", "TUNE", "DECAY", "START"};
static const char *const GEN_NAMES[4] = {"VARIAR", "ANCLAR", "REPETIR", "ROMPER"};
static const char *const RT_NAMES[4] = {"2X", "3X", "4X", "8X"};
static const char *const DLY_NAMES[4] = {"1/16", "1/8", "3/16", "1/4"};
static const char *const EXT_NAMES[5] = {"TRIADA", "+7", "+9", "SUS2", "SUS4"};
static const char *const ARP_MODES[5] = {"OFF", "UP", "DOWN", "UP-DN", "RAND"};
static const char *const ARP_RATES[3] = {"1/8", "1/16", "1/32"};
static const char *const VOICINGS[3] = {"CERRADO", "ABIERTO", "CONDUCIDO"};
static const char *const RAT_NAMES[5] = {"X1", "X2", "X3", "X4", "FLAM"};
static const char *const COND_NAMES[3] = {"SIEMPRE", "SOLO FILL", "SIN FILL"};
static const char *const SNAP_NAMES[3] = {"OFF", "1/16", "1/8"};
static const char *const PUNCH_NAMES[16] = {"S4", "S8", "S16", "S32", "R8", "R4", "TAPE", "GATE", "LPF", "HPF", "BIT", "SR", "DLY", "ECHO", "K-LO", "K-HI"};

enum { ROLE_NONE, ROLE_MUTE, ROLE_FILL, ROLE_PAT, ROLE_SCENE, ROLE_BRKFX, ROLE_LMUTE, ROLE_NOTE };

const char *bm_macro_name(const bm_app *a, int t, int i)
{
    if (t == 1) return GEN_NAMES[i & 3];
    if (bm_is_tr(&a->pat, t)) return MAC_TR[i & 3];
    if (a->pat.trk[t].engine == ENG_ACID) return MAC_ACID[i & 3];
    if (a->pat.trk[t].engine == ENG_VA) return MAC_VA[i & 3];
    return MAC_SYN[i & 3];
}

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

static void fmt_semi(int v, char *b, int cap) { snprintf(b, (size_t)cap, "%+d ST", v - 24); }

void bm_fmt_macro(const bm_app *a, int t, int i, int v, char *b, int cap)
{
    if (t == 1 || bm_is_tr(&a->pat, t)) {
        if (t != 1 && i == 1) snprintf(b, (size_t)cap, "%+d ST", ((v - 64) * 12) / 64);
        else fmt_pct(v, b, cap);
        return;
    }
    switch (i) {
    case 0: fmt_hz(bm_cutoff_hz(v), b, cap); break;
    case 1: fmt_pct(v, b, cap); break;
    case 2: fmt_ms(bm_decay_ms(v), b, cap); break;
    default: {
        int e = a->pat.trk[t].engine;
        if (e == ENG_ACID || e == ENG_VA) { fmt_pct(v, b, cap); break; }
        int p = v * 3 * 256 / 127;
        int w = (a->pat.trk[t].source + (p >> 8)) % BM_WAVES;
        snprintf(b, (size_t)cap, "%s", bm_rom_wave_names[w]);
    }
    }
}

static bool lock_mode(const bm_app *a) { return a->held_step >= 0 && a->held_long; }
static int tr_lane(const bm_app *a) { return a->sel_lane[a->track] & 15; }
static int seq_page(const bm_app *a) { return a->view == V_CH && a->page == PG_SEQ; }

static int syn_step_events(const bm_app *a, int t, int s, int idx[4])
{
    int n = 0;
    for (int k = 0; k < BM_EVENTS; k++)
        if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) idx[n++] = k;
    return n;
}

/* ------------------------------------------------------------ filas de MAS */
enum { ROW_ENGINE, ROW_ACC, ROW_LEN, ROW_SPEED, ROW_TRANSP, ROW_MIDICH, ROW_LOOPBPM, ROW_INTERP, ROW_INIT };

static int more_rows(const bm_app *a, int t, uint8_t out[10])
{
    int n = 0;
    if (bm_is_free(t) || t == 2) out[n++] = ROW_ENGINE;
    out[n++] = ROW_ACC; out[n++] = ROW_LEN; out[n++] = ROW_SPEED;
    if (app_kind(a, t) == KIND_NOTE) out[n++] = ROW_TRANSP;
    if (app_midi(a, t)) out[n++] = ROW_MIDICH;
    if (t == 1) { out[n++] = ROW_LOOPBPM; out[n++] = ROW_INTERP; }
    out[n++] = ROW_INIT;
    return n;
}

static const char *const ROW_NAMES[] = {"MOTOR", "ACENTO", "LARGO", "VELOCIDAD", "TRANSPONER", "CANAL MIDI", "USAR BPM DEL LOOP", "INTERPOLACION", "SONIDO INICIAL"};

void bm_app_more_row(const bm_app *a, int i, const char **name, char *val, int cap)
{
    uint8_t rows[10];
    int n = more_rows(a, a->track, rows), t = a->track;
    const bm_track *tk = &a->pat.trk[t];
    val[0] = 0;
    if (i < 0 || i >= n) { *name = ""; return; }
    *name = ROW_NAMES[rows[i]];
    switch (rows[i]) {
    case ROW_ENGINE: snprintf(val, (size_t)cap, "%s", bm_engine_names[tk->engine % ENG_COUNT]); break;
    case ROW_ACC: fmt_pct(tk->accent, val, cap); break;
    case ROW_LEN: snprintf(val, (size_t)cap, "%d PASOS", bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? 0 : -1)); break;
    case ROW_SPEED: snprintf(val, (size_t)cap, "%s", bm_speed_names[tk->speed % SPD_COUNT]); break;
    case ROW_TRANSP: fmt_semi(tk->transpose, val, cap); break;
    case ROW_MIDICH: snprintf(val, (size_t)cap, "%d", tk->midi_ch + 1); break;
    case ROW_LOOPBPM: {
        const bm_smp_src *m = &a->ctx.smp[0];
        if (m->bpm_x10) snprintf(val, (size_t)cap, "%d.%d", m->bpm_x10 / 10, m->bpm_x10 % 10); else snprintf(val, (size_t)cap, "--");
        break;
    }
    case ROW_INTERP: snprintf(val, (size_t)cap, "%s", tk->interp ? "HERMITE" : "DROP"); break;
    default: snprintf(val, (size_t)cap, "GIRA = SI"); break;
    }
}

int bm_app_more_count(const bm_app *a) { uint8_t r[10]; return more_rows(a, a->track, r); }

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

static void legend_mix(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    snprintf(b, sizeof b, "%d", tr->level); L4(L, 0, "NIVEL", b, tr->level * 2);
    fmt_pan(tr->pan, b, sizeof b); L4(L, 1, "PAN", b, tr->pan * 2);
    fmt_pct(tr->send, b, sizeof b); L4(L, 2, "ENVIO", b, tr->send * 2);
    if (t >= 1) { fmt_pct(tr->duck, b, sizeof b); L4(L, 3, "DUCK", b, tr->duck * 2); } else L4(L, 3, "DUCK", "FUENTE", 0);
}

static void legend_fx(const bm_app *a, bm_legend *L)
{
    const bm_meta *m = &a->pat.meta;
    char b[12];
    L4(L, 0, "DLY TIME", DLY_NAMES[m->dly_time & 3], (m->dly_time & 3) * 85);
    fmt_pct(m->dly_fb, b, sizeof b); L4(L, 1, "FEEDBACK", b, m->dly_fb * 2);
    fmt_pct(m->dly_mix, b, sizeof b); L4(L, 2, "DLY MIX", b, m->dly_mix * 2);
    fmt_ms(bm_decay_ms(m->sc_release / 2), b, sizeof b); L4(L, 3, "SC REL", b, m->sc_release * 2);
}

static void legend_macros(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    for (int i = 0; i < 4; i++) { bm_fmt_macro(a, t, i, tr->macro[i], b, sizeof b); L4(L, i, bm_macro_name(a, t, i), b, tr->macro[i] * 2); }
}

static void legend_midi(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    snprintf(b, sizeof b, "%d", tr->midi_ch + 1); L4(L, 0, "CANAL", b, tr->midi_ch * 17);
    snprintf(b, sizeof b, "%d", tr->macro[1] + 1); L4(L, 1, "PROG", b, tr->macro[1] * 2);
    snprintf(b, sizeof b, "%d", tr->macro[2]); L4(L, 2, "CC1 MOD", b, tr->macro[2] * 2);
    snprintf(b, sizeof b, "%d", tr->macro[3]); L4(L, 3, "CC74", b, tr->macro[3] * 2);
}

static void legend_chords(const bm_app *a, bm_legend *L)
{
    const bm_meta *m = &a->pat.meta;
    char b[12];
    snprintf(b, sizeof b, "%.8s", bm_chord_set_names[m->chord_set % BM_CHORD_SETS]); L4(L, 0, "JUEGO", b, m->chord_set * 127);
    snprintf(b, sizeof b, "%.8s", VOICINGS[m->chord_voicing % 3]); L4(L, 1, "VOICING", b, m->chord_voicing * 127);
    snprintf(b, sizeof b, "%d", m->chord_oct); L4(L, 2, "OCTAVA", b, (m->chord_oct - 1) * 51);
    snprintf(b, sizeof b, "%dMS", m->strum); L4(L, 3, "STRUM", b, m->strum * 4);
}

static void legend_arp(const bm_app *a, int t, bm_legend *L)
{
    const bm_track *tr = &a->pat.trk[t];
    char b[12];
    L4(L, 0, "MODO", ARP_MODES[tr->arp_mode % 5], tr->arp_mode * 63);
    L4(L, 1, "RATE", ARP_RATES[tr->arp_rate % 3], tr->arp_rate * 127);
    snprintf(b, sizeof b, "%d", tr->arp_oct); L4(L, 2, "OCTAVAS", b, tr->arp_oct * 85);
    snprintf(b, sizeof b, "%d%%", tr->arp_gate); L4(L, 3, "GATE", b, tr->arp_gate * 255 / 100);
}

static void legend_len(const bm_app *a, bm_legend *L)
{
    int t = a->track;
    char b[12];
    int len = bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? (t == 1 ? 0 : tr_lane(a)) : -1);
    snprintf(b, sizeof b, "%d", len); L4(L, 0, "LARGO", b, len * 4);
    L4(L, 1, "VELOC", bm_speed_names[a->pat.trk[t].speed % SPD_COUNT], (SPD_COUNT - 1 - a->pat.trk[t].speed) * 51);
}

uint8_t bm_app_takes_sel(const bm_app *a)
{
    uint8_t list[BM_TAKES + 8];
    int n = bm_app_sources(a, list);
    if (!n) return BM_SRC_NONE;
    return list[a->takes_row % n];
}

void bm_app_legend(const bm_app *a, bm_legend L[4])
{
    memset(L, 0, sizeof(bm_legend) * 4);
    int t = a->track;
    const bm_track *tr = &a->pat.trk[t];
    const bm_meta *m = &a->pat.meta;
    char b[12];
    switch (a->layer) {
    case L_HOME: legend_global(a, L); return;
    case L_TONE:
        fmt_semi(m->transpose, b, sizeof b); L4(L, 0, "TRANSP", b, m->transpose * 5);
        snprintf(b, sizeof b, "%+d", ((int)m->transpose - 24) / 12); L4(L, 1, "OCTAVA", b, m->transpose * 5);
        snprintf(b, sizeof b, "%d", (m->bpm_x10 + 5) / 10); L4(L, 2, "BPM", b, (m->bpm_x10 - 400) * 255 / 2000);
        snprintf(b, sizeof b, "%d%%", m->swing); L4(L, 3, "SWING", b, (m->swing - 50) * 255 / 25);
        return;
    case L_LEN: legend_len(a, L); return;
    case L_ACC: fmt_pct(tr->accent, b, sizeof b); L4(L, 0, "ACENTO", b, tr->accent * 2); return;
    case L_REP: if (app_kind(a, t) == KIND_NOTE) { fmt_ms((uint32_t)tr->glide * 500 / 127, b, sizeof b); L4(L, 0, "GLIDE", b, tr->glide * 2); } return;
    case L_PUNCH: legend_fx(a, L); return;
    case L_CHORDS: legend_chords(a, L); return;
    case L_ARP: legend_arp(a, t, L); return;
    case L_ROLL: L4(L, 0, "RATE", bm_roll_names[a->roll_rate % 5], a->roll_rate * 63); return;
    case L_COPY: snprintf(b, sizeof b, "A%02d", a->ctx.st.pattern + 1); L4(L, 0, "DESDE", b, 0); return;
    case L_ERASE: case L_LANEMUTE: return;
    default: break;
    }
    switch (a->view) {
    case V_HOME: case V_SCENES: case V_GLO: legend_global(a, L); return;
    case V_MIX: legend_mix(a, t, L); return;
    case V_FXG: legend_fx(a, L); return;
    case V_SCLG:
        L4(L, 0, "TONICA", bm_note_names[m->root % 12], m->root * 23);
        snprintf(b, sizeof b, "%.8s", bm_scale_names[m->scale % BM_SCALES]); L4(L, 1, "ESCALA", b, m->scale * 31);
        L4(L, 2, "ACORDE", EXT_NAMES[m->voicing % 5], m->voicing * 63);
        L4(L, 3, "LOCK", m->scale_lock ? "ON" : "OFF", m->scale_lock ? 255 : 0);
        return;
    case V_TIME: {
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        if (bm_is_grid(&a->pat, row)) {
            const bm_tr *g = bm_trk_trc(&a->pat, row);
            snprintf(b, sizeof b, "%d", g->lane[0].len); L4(L, 0, "LARGO", b, g->lane[0].len * 4);
            L4(L, 1, "VELOCID", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
            if (row != 1) {
                L4(L, 2, "INSTRUM", bm_lane_name(a, row, a->time_lane), a->time_lane * 17);
                snprintf(b, sizeof b, "%d", g->lane[a->time_lane].len); L4(L, 3, "SU LARGO", b, g->lane[a->time_lane].len * 4);
            }
        } else {
            snprintf(b, sizeof b, "%d", rt->length); L4(L, 0, "LARGO", b, rt->length * 4);
            L4(L, 1, "VELOCID", bm_speed_names[rt->speed % SPD_COUNT], (SPD_COUNT - 1 - rt->speed) * 51);
        }
        return;
    }
    case V_SAVE: snprintf(b, sizeof b, "A%02d", a->save_target + 1); L4(L, 0, "DESTINO", b, a->save_target * 17); return;
    case V_SYSTEM: L4(L, 0, "VALOR", "ALGO", 0); return;
    case V_SONG:
        if (a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            if (e & 0x80) snprintf(b, sizeof b, "ESC %c", 'A' + (e & 3)); else snprintf(b, sizeof b, "A%02d", e + 1);
            L4(L, 0, "PASO", b, 0);
            snprintf(b, sizeof b, "x%d", a->song.rep[a->song_cur]); L4(L, 1, "VECES", b, 0);
        }
        L4(L, 3, "SONG", a->ctx.st.song_on ? "ON" : "OFF", a->ctx.st.song_on ? 255 : 0);
        return;
    case V_CH: break;
    default: return;
    }
    if (lock_mode(a) && a->page == PG_SEQ && t != 1) {
        for (int i = 0; i < 4; i++) {
            int val = tr->macro[i];
            if (bm_is_tr(&a->pat, t)) {
                int lv = bm_tr_lock_get(bm_trk_trc(&a->pat, t), a->held_step, tr_lane(a), i);
                if (lv >= 0) { val = lv; L[i].locked = 1; }
            } else {
                int idx[4];
                int n = syn_step_events(a, t, a->held_step, idx);
                if (n && bm_evc(&a->pat, t, a->held_step, idx[0])->lk[i] != BM_LOCK_NONE) { val = bm_evc(&a->pat, t, a->held_step, idx[0])->lk[i]; L[i].locked = 1; }
            }
            bm_fmt_macro(a, t, i, val, b, sizeof b);
            L4(L, i, bm_macro_name(a, t, i), b, val * 2);
        }
        return;
    }
    int kind = app_kind(a, t);
    switch (a->page) {
    case PG_SEQ:
        if (app_midi(a, t)) legend_midi(a, t, L);
        else if (t == 1) for (int i = 0; i < 4; i++) { fmt_pct(tr->gen[i], b, sizeof b); L4(L, i, GEN_NAMES[i], b, tr->gen[i] * 2); }
        else legend_macros(a, t, L);
        return;
    case PG_SOUND:
        if (t == 1) {
            char n1[12], n2[12];
            bm_app_src_name(a, tr->source, n1, sizeof n1); L4(L, 0, "LOOP A", n1, 0);
            bm_app_src_name(a, tr->loop_b, n2, sizeof n2); L4(L, 1, "LOOP B", tr->loop_b == BM_SRC_NONE ? "-" : n2, 0);
            snprintf(b, sizeof b, "%d%%", tr->chance_b); L4(L, 2, "CHANCE B", b, tr->chance_b * 255 / 100);
            snprintf(b, sizeof b, "%+.1f ST", (double)bm_app_brk_semis(a)); L4(L, 3, "PITCH", b, tr->pitch * 5);
        } else if (kind == KIND_TR) {
            for (int i = 0; i < 3; i++) { bm_fmt_macro(a, t, i, tr->macro[i], b, sizeof b); L4(L, i, MAC_TR[i], b, tr->macro[i] * 2); }
            fmt_pct(tr->accent, b, sizeof b); L4(L, 3, "ACENTO", b, tr->accent * 2);
        } else {
            bm_fmt_macro(a, t, 3, tr->macro[3], b, sizeof b);
            L4(L, 0, tr->engine == ENG_ACID || tr->engine == ENG_VA ? bm_macro_name(a, t, 3) : "POS ONDA", b, tr->macro[3] * 2);
            bm_fmt_macro(a, t, 0, tr->macro[0], b, sizeof b); L4(L, 1, "CUTOFF", b, tr->macro[0] * 2);
            fmt_pct(tr->macro[1], b, sizeof b); L4(L, 2, "RESO", b, tr->macro[1] * 2);
            fmt_pct(tr->accent, b, sizeof b); L4(L, 3, "ACENTO", b, tr->accent * 2);
        }
        return;
    case PG_INST: {
        const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[tr_lane(a)];
        snprintf(b, sizeof b, "%d", ln->level); L4(L, 0, "NIVEL", b, ln->level * 2);
        snprintf(b, sizeof b, "%+d ST", ((ln->tune - 64) * 12) / 64); L4(L, 1, "AFINA", b, ln->tune * 2);
        snprintf(b, sizeof b, "%d%%", ln->decay * 100 / 127); L4(L, 2, "DECAY", b, ln->decay * 2);
        snprintf(b, sizeof b, "%d", ln->len); L4(L, 3, "LARGO", b, ln->len * 4);
        return;
    }
    case PG_MIX: legend_mix(a, t, L); return;
    case PG_MORE: {
        bm_legend tmp[4];
        memset(tmp, 0, sizeof tmp);
        legend_len(a, tmp);
        fmt_pct(tr->accent, b, sizeof b); L4(L, 0, "ACENTO", b, tr->accent * 2);
        L[1] = tmp[0]; L[2] = tmp[1];
        if (kind == KIND_NOTE) { fmt_semi(tr->transpose, b, sizeof b); L4(L, 3, "TRANSP", b, tr->transpose * 5); }
        return;
    }
    case PG_RETRIG:
        for (int i = 0; i < 4; i++) { fmt_pct(tr->rt[i], b, sizeof b); L4(L, i, RT_NAMES[i], b, tr->rt[i] * 2); }
        return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (src != BM_SRC_NONE && !(src & BM_SRC_ROM)) {
            const bm_take *tk = &a->take[src];
            snprintf(b, sizeof b, "%d", (tk->bpm_x10 + 5) / 10); L4(L, 0, "BPM", b, (tk->bpm_x10 - 400) * 255 / 2000);
            snprintf(b, sizeof b, "%d", tk->bars); L4(L, 1, "COMPASES", b, tk->bars * 31);
            int db = ((int)tk->gain - 64) * 6 / 64;
            snprintf(b, sizeof b, "%+d DB", db); L4(L, 2, "GANANCIA", b, tk->gain * 2);
        } else if (src != BM_SRC_NONE) {
            const bm_rom_loop *l = &bm_rom_loops[src & 0x7F];
            snprintf(b, sizeof b, "%d", l->bpm_x10 / 10); L4(L, 0, "BPM", b, 0);
            snprintf(b, sizeof b, "%d", l->bars); L4(L, 1, "COMPASES", b, 0);
            L4(L, 2, "FABRICA", "FIJO", 0);
        }
        return;
    }
    case PG_ENV:
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
    case PG_ARP: if (kind == KIND_NOTE) legend_arp(a, t, L); else L4(L, 0, "ROLL", bm_roll_names[a->roll_rate % 5], a->roll_rate * 63); return;
    case PG_LOOP: {
        const bm_loop *lp = &a->pat.loop[t >= 2 ? t - 2 : 0];
        L4(L, 0, "LOOP", lp->on ? "ON" : "OFF", lp->on ? 255 : 0);
        snprintf(b, sizeof b, "%d", lp->bars); L4(L, 1, "COMPASES", b, lp->bars * 63);
        return;
    }
    case PG_MIDI: legend_midi(a, t, L); return;
    case PG_SCL: {
        int root, sc = app_scale(a, t, &root);
        L4(L, 0, "TONICA", bm_note_names[root], root * 23);
        if (tr->scl == 0xFF) snprintf(b, sizeof b, "PROYECTO"); else snprintf(b, sizeof b, "%.8s", bm_scale_names[sc]);
        L4(L, 1, "ESCALA", b, tr->scl == 0xFF ? 0 : (sc + 1) * 25);
        L4(L, 2, "ACORDE", EXT_NAMES[m->voicing % 5], m->voicing * 63);
        L4(L, 3, "LOCK", m->scale_lock ? "ON" : "OFF", m->scale_lock ? 255 : 0);
        return;
    }
    case PG_CHORDS: legend_chords(a, L); return;
    case PG_REC: L4(L, 0, "INICIO", a->rec_now ? "YA" : "COMPAS", a->rec_now ? 255 : 0); L4(L, 1, "FUENTE", "MEZCLA", 0); return;
    case PG_CUT: {
        const bm_take *tk = &a->take[a->cut_take % BM_TAKES];
        int n = tk->nmarks ? tk->nmarks : BM_SLICES;
        snprintf(b, sizeof b, "%d/%d", a->cut_sel + 1, n); L4(L, 0, "CORTE", b, a->cut_sel * 255 / (n > 1 ? n - 1 : 1));
        uint32_t pos = tk->nmarks ? tk->marks[a->cut_sel % BM_SLICES] : tk->len * (a->cut_sel % BM_SLICES) / BM_SLICES;
        unsigned ms = (unsigned)(pos * 1000u / BM_SMP_RATE);
        snprintf(b, sizeof b, "%u.%02uS", ms / 1000, ms % 1000 / 10); L4(L, 1, "MOVER", b, tk->len ? (int)(pos * 255u / tk->len) : 0);
        snprintf(b, sizeof b, "X%d", 1 << a->cut_zoom); L4(L, 2, "ZOOM", b, a->cut_zoom * 127);
        L4(L, 3, "PEGAR", SNAP_NAMES[a->cut_snap % 3], a->cut_snap * 127);
        return;
    }
    default: return;
    }
}

/* ------------------------------------------------------------ mapa del teclado (capas, HOME) */
static void KW(bm_keyfn *K, int w, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_white_key(w)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
}

static void KB(bm_keyfn *K, int b, const char *l, uint8_t led, uint16_t c)
{
    bm_keyfn *k = &K[bm_black_key(b)];
    snprintf(k->label, sizeof k->label, "%s", l); k->led = led; k->color = c;
}

static void map_home(const bm_app *a, bm_keyfn *K)
{
    const bm_status *st = &a->ctx.st;
    char b[6];
    for (int w = 0; w < 16; w++) {
        bool has = bm_store_has(&a->store, (uint8_t)w) || w == st->pattern || a->pend_obj == w;
        uint8_t led = w == st->pattern ? LED_ON : (w == st->queued ? LED_BLINK : (has ? LED_MID : LED_OFF));
        snprintf(b, sizeof b, "%02d", w + 1);
        KW(K, w, b, led, w == st->pattern ? C_WHITE : w == st->queued ? C_CYAN : 0);
    }
    for (int t = 0; t < BM_TRACKS; t++) {
        snprintf(b, sizeof b, "T%d", t + 1);
        KB(K, t, b, a->pat.trk[t].mute ? LED_OFF : LED_ON, bm_track_color(t));
    }
    for (int s = 0; s < 4; s++) {
        snprintf(b, sizeof b, "%c", 'A' + s);
        KB(K, 6 + s, b, st->scene == s ? LED_ON : (st->scene_q == s ? LED_BLINK : (a->song.scene[s].valid ? LED_MID : LED_OFF)), C_CYAN);
    }
    KB(K, 10, "F", a->fill ? LED_ON : LED_DIM, C_ORANGE);
}

static void map_steps(const bm_app *a, bm_keyfn *K, int mode)
{
    /* mode 0: acento · 1: repetir/ligar · 2: borrar */
    int t = a->track, kind = app_kind(a, t);
    for (int w = 0; w < 16; w++) {
        int s = a->step_page * 16 + w;
        const char *l = "";
        uint8_t led = LED_OFF;
        if (kind == KIND_NOTE) {
            if (s >= a->pat.trk[t].length) { KW(K, w, "", LED_OFF, 0); continue; }
            int idx[4], n = syn_step_events(a, t, s, idx);
            if (n) {
                const bm_event *e = bm_evc(&a->pat, t, s, idx[0]);
                if (mode == 0) { l = e->vel >= BM_VEL_ACC ? "A" : e->vel <= BM_VEL_GHOST ? "G" : ""; led = e->vel >= BM_VEL_ACC ? LED_ON : LED_MID; }
                else if (mode == 1) { l = (e->len & BM_LEN_TIE) ? "LIG" : ""; led = (e->len & BM_LEN_TIE) ? LED_ON : LED_MID; }
                else { l = "X"; led = LED_MID; }
            }
        } else {
            const bm_tr *tr = bm_trk_trc(&a->pat, t);
            int lane = t == 1 ? 0 : tr_lane(a);
            if (s >= tr->lane[lane].len) { KW(K, w, "", LED_OFF, 0); continue; }
            uint8_t v = 0;
            int on = t == 1 ? bm_brk_step(tr, s, &v) >= 0 : ((v = tr->st[lane][s]) & ST_ON) != 0;
            if (on) {
                int rat = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
                if (mode == 0) { l = (v & ST_ACC) ? "A" : ""; led = (v & ST_ACC) ? LED_ON : LED_MID; }
                else if (mode == 1) { l = RAT_NAMES[rat]; if (!rat) l = ""; led = rat ? LED_ON : LED_MID; }
                else { l = "X"; led = LED_MID; }
            }
        }
        if (s == bm_app_track_step(a, t, t == 1 ? 0 : (kind == KIND_NOTE ? -1 : tr_lane(a)))) led = LED_BLINK;
        KW(K, w, l, led, mode == 0 ? C_WHITE : mode == 1 ? C_YELLOW : C_RED);
    }
}

static void map_lanes(const bm_app *a, bm_keyfn *K, int t, uint16_t act)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    for (int w = 0; w < 16; w++) {
        char b[6];
        if (t == 1) { if (w >= BM_SLICES) { KW(K, w, "", LED_OFF, 0); continue; } snprintf(b, sizeof b, "S%d", w + 1); }
        else snprintf(b, sizeof b, "%.4s", bm_lane_name(a, t, w));
        uint8_t led = (act >> w) & 1 ? LED_ON : ((tr->mute >> w) & 1 ? LED_OFF : LED_MID);
        KW(K, w, b, led, bm_track_color(t));
    }
}

static void map_chords(const bm_app *a, bm_keyfn *K)
{
    static const uint16_t FN[3] = {C_BLUE, C_GREEN, C_ORANGE};
    const bm_meta *m = &a->pat.meta;
    int root, sc = app_scale(a, bm_app_chord_track(a), &root);
    for (int p = 0; p < 16; p++) {
        bm_pad_chord pc = bm_chord_pad(m->chord_set, p);
        char rn[10];
        bm_roman(root, sc, pc.degree, pc.ext, rn, sizeof rn);
        rn[4] = 0;
        KW(K, p, rn, p == a->last_pad ? LED_ON : LED_DIM, FN[bm_chord_function(pc.degree)]);
    }
    static const char *const MODS[4] = {"+7", "+9", "S2", "S4"};
    for (int b = 0; b < 4; b++) KB(K, b, MODS[b], (a->chord_mod >> b) & 1 ? LED_ON : LED_DIM, C_YELLOW);
}

void bm_app_keymap(const bm_app *a, bm_keyfn K[BM_KEYS], char *title, int cap, uint16_t *color, const char **h1, const char **h2)
{
    memset(K, 0, sizeof(bm_keyfn) * BM_KEYS);
    int t = a->track, kind = app_kind(a, t);
    const bm_meta *m = &a->pat.meta;
    char b[8];
    *h1 = ""; *h2 = "";
    *color = C_WHITE;
    title[0] = 0;
    int L = a->layer;
    if (!L && (a->view == V_HOME || a->view == V_MIX)) L = L_HOME;
    switch (L) {
    case L_HOME:
        snprintf(title, (size_t)cap, "HOME");
        *h1 = "BLANCA = PATRON EN COLA - DOS = CADENA";
        *h2 = "NEGRA = MUTE, MANTENER = SOLO - F = FILL";
        map_home(a, K);
        return;
    case L_LEN: {
        snprintf(title, (size_t)cap, "LARGO Y VELOCIDAD");
        *color = bm_track_color(t);
        *h1 = "BLANCA = LARGO - NEGRAS 1-6 = VELOCIDAD";
        *h2 = "NEGRAS 7-10 = 1-4 COMPASES - OCT-/+ = 17-64";
        int len = bm_trk_len(&a->pat, t, kind == KIND_NOTE ? -1 : (t == 1 ? 0 : tr_lane(a)));
        for (int w = 0; w < 16; w++) {
            int v = a->len_page * 16 + w + 1;
            snprintf(b, sizeof b, "%d", v);
            KW(K, w, b, v == len ? LED_BLINK : (v <= len ? LED_ON : LED_DIM), bm_track_color(t));
        }
        static const char *const SP[SPD_COUNT] = {"X2", "3/2", "X1", "3/4", "1/2", "1/4"};
        for (int s = 0; s < SPD_COUNT; s++) KB(K, s, SP[s], a->pat.trk[t].speed == s ? LED_ON : LED_DIM, C_WHITE);
        int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
        for (int c = 1; c <= 4; c++) { snprintf(b, sizeof b, "%dC", c); KB(K, 5 + c, b, len == c * spb ? LED_ON : LED_DIM, C_YELLOW); }
        return;
    }
    case L_ERASE:
        snprintf(title, (size_t)cap, "BORRAR");
        *color = C_RED;
        *h1 = app_playing(a) ? "SONANDO: BLANCA SOSTENIDA BORRA AL PASAR" : "PARADO: BLANCA BORRA LA LINEA O EL PASO";
        *h2 = "OCT- DESHACER - OCT+ REHACER";
        if (kind == KIND_TR) map_lanes(a, K, t, a->erase_mask);
        else map_steps(a, K, 2);
        KB(K, 0, "COPIA", LED_DIM, C_WHITE); KB(K, 1, "PEGA", LED_DIM, C_WHITE); KB(K, 2, "LIMPIA", LED_DIM, C_RED);
        if (t == 1) KB(K, 3, "CAPT", LED_ON, C_YELLOW);
        return;
    case L_ACC:
        snprintf(title, (size_t)cap, "ACENTO");
        *h1 = "BLANCA = ACENTO SI / NO - PASO VACIO = NUEVO";
        *h2 = kind == KIND_NOTE ? "EN NOTAS: NORMAL > ACENTO > FANTASMA" : "CONVIVE CON REPETICION, FLAM Y CONDICION";
        map_steps(a, K, 0);
        return;
    case L_REP:
        snprintf(title, (size_t)cap, kind == KIND_NOTE ? "LIGAR" : "REPETIR");
        *color = C_YELLOW;
        *h1 = kind == KIND_NOTE ? "BLANCA = LIGA LA NOTA CON LA SIGUIENTE" : "BLANCA = X1 > X2 > X3 > X4 > FLAM";
        *h2 = kind == KIND_NOTE ? "EL GLIDE SE AJUSTA CON K1" : "LO MISMO QUE PASO + ALGORITHM, PINTANDO";
        map_steps(a, K, 1);
        return;
    case L_PUNCH:
        snprintf(title, (size_t)cap, "PUNCH-IN");
        *color = C_RED;
        *h1 = "BLANCAS = 16 EFECTOS, CUANTIZADOS A 1/16";
        *h2 = "NEGRAS 1-6 SIGUEN MUTEANDO";
        for (int w = 0; w < 16; w++) KW(K, w, PUNCH_NAMES[w], a->ctx.st.punch == w ? LED_ON : (a->ctx.arena_busy && w <= 6 ? LED_OFF : LED_DIM), C_YELLOW);
        for (int i = 0; i < BM_TRACKS; i++) { snprintf(b, sizeof b, "T%d", i + 1); KB(K, i, b, a->pat.trk[i].mute ? LED_OFF : LED_ON, bm_track_color(i)); }
        return;
    case L_CHORDS:
        snprintf(title, (size_t)cap, "ACORDES");
        *color = C_GREEN;
        *h1 = "BLANCAS = 16 ACORDES - NEGRAS 1-4 MODIFICAN";
        *h2 = "AZUL REPOSO - VERDE PREPARA - NARANJA TENSION";
        map_chords(a, K);
        return;
    case L_ARP: case L_ROLL:
        if (L == L_ROLL) {
            snprintf(title, (size_t)cap, "ROLL %s", bm_roll_names[a->roll_rate % 5]);
            *color = bm_track_color(t);
            *h1 = "BLANCA = REPITE MIENTRAS LA SOSTIENES";
            *h2 = a->rec ? "REC: SE GRABA COMO REPETICION" : "K1 = VELOCIDAD DEL ROLL";
            map_lanes(a, K, t, a->roll_mask);
        } else {
            snprintf(title, (size_t)cap, "ARPEGIO");
            *color = bm_track_color(t);
            *h1 = "MANTEN UN ACORDE: SE ARPEGIA AL TEMPO";
            *h2 = "AL SOLTAR ARP SE CALLA";
            for (int k = 0; k < BM_KEYS; k++) {
                int note = BM_KEY_BASE_NOTE + k + 12 * a->oct;
                snprintf(K[k].label, sizeof K[k].label, "%s", bm_note_names[note % 12]);
                K[k].led = a->keys_down[k] ? LED_ON : LED_OFF; K[k].color = bm_track_color(t);
            }
        }
        return;
    case L_COPY:
        snprintf(title, (size_t)cap, "COPIAR PATRON");
        *color = C_YELLOW;
        *h1 = "BLANCA = PEGAR AHI - NEGRA + BLANCA = UN CANAL";
        *h2 = "SI EL DESTINO TIENE NOTAS: OCT+ SI, HOME NO";
        for (int w = 0; w < 16; w++) {
            snprintf(b, sizeof b, "%02d", w + 1);
            KW(K, w, b, w == a->ctx.st.pattern ? LED_ON : (bm_store_has(&a->store, (uint8_t)w) ? LED_MID : LED_OFF), C_YELLOW);
        }
        for (int i = 0; i < BM_TRACKS; i++) {
            snprintf(b, sizeof b, "T%d", i + 1);
            KB(K, i, b, a->keys_down[bm_black_key(i)] ? LED_ON : LED_DIM, bm_track_color(i));
        }
        return;
    case L_TONE:
        snprintf(title, (size_t)cap, "TONO");
        *color = C_CYAN;
        *h1 = "EL TECLADO TRANSPONE EL PROYECTO - C4 = 0";
        *h2 = "PLAY = TAP TEMPO, SIN PARAR";
        for (int k = 0; k < BM_KEYS; k++) {
            int off = k - 7;
            if (off == 0) snprintf(K[k].label, sizeof K[k].label, "0"); else snprintf(K[k].label, sizeof K[k].label, "%+d", off);
            K[k].led = (24 + off == m->transpose) ? LED_ON : (off == 0 ? LED_DIM : LED_OFF);
            K[k].color = C_CYAN;
        }
        return;
    case L_LANEMUTE:
        snprintf(title, (size_t)cap, "MUTE INSTRUMENTOS");
        *color = bm_track_color(t);
        *h1 = "BLANCA = MUTE DE ESE INSTRUMENTO";
        *h2 = "SUELTA LA NEGRA 10 PARA VOLVER";
        map_lanes(a, K, t, 0);
        return;
    default: break;
    }
    if (a->view == V_CH && a->page == PG_CHORDS) map_chords(a, K);
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
            const bm_event *ev = bm_evc(&a->pat, t, s, e);
            if (!bm_event_empty(ev) && ev->note == notes[i]) { slot = e; break; }
            if (slot < 0 && bm_event_empty(ev)) slot = e;
        }
        if (slot < 0) continue;
        bm_event ev;
        bm_event_clear(&ev);
        ev.note = notes[i]; ev.len = 1;
        app_push_ev(a, t, s, slot, &ev);
        if (a->key_rec_slot[k] < 0) a->key_rec_slot[k] = (int8_t)slot;
    }
    app_edited(a);
}

static int quant_step(const bm_app *a, int t, int len)
{
    int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    return (int)(((a->ctx.st.rtick + (uint32_t)tps / 2) / (uint32_t)tps) % (uint32_t)(len ? len : 16));
}

static void rec_tr(bm_app *a, int t, int lane)
{
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int s = quant_step(a, t, tr->lane[lane].len);
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (uint8_t)(tr->st[lane][s] | ST_ON));
    app_edited(a);
}

/* sampler: fija el slice en el paso (pasos fijos de la fila TU) */
static void brk_pin(bm_app *a, int s, int slice)
{
    const bm_tr *tr = &a->pat.tr[1];
    uint8_t keep = 0;
    bm_brk_step(tr, s, &keep);
    keep = (uint8_t)((keep & ~ST_LOCK) | ST_ON);
    for (int l = 0; l <= BM_SLICES; l++) if (tr->st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0);
    app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)(slice + 1), (uint8_t)s, keep);
    app_edited(a);
}

static void brk_to_gen(bm_app *a, int s)
{
    const bm_tr *tr = &a->pat.tr[1];
    uint8_t keep = 0;
    bm_brk_step(tr, s, &keep);
    for (int l = 1; l <= BM_SLICES; l++) if (tr->st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0);
    app_push_i(a, CMD_SET_TRSTEP, 1, BRK_GEN, (uint8_t)s, (uint8_t)((keep & ~ST_LOCK) | ST_ON));
    app_edited(a);
}

static void play_notes(bm_app *a, int k, int t, const uint8_t *notes, int n, int vel, int strum)
{
    if (strum && n > 1) {
        int32_t packed = 0;
        for (int i = 0; i < n && i < 4; i++) packed |= (int32_t)notes[i] << (8 * i);
        app_push_i(a, CMD_STRUM, (uint8_t)t, (uint8_t)n, (uint8_t)vel, packed);
    } else {
        for (int i = 0; i < n; i++) app_push(a, CMD_NOTE_ON, (uint8_t)t, notes[i], (uint8_t)vel);
    }
    memcpy(a->key_notes[k], notes, (size_t)n);
    a->key_nn[k] = (uint8_t)n;
    a->key_trk[k] = (uint8_t)t;
    if (n && app_kind(a, t) == KIND_NOTE) a->last_note[t] = notes[0];
    a->key_rec_step[k] = -1;
    if (a->rec && app_playing(a) && !seq_page(a)) {
        if (t == 1) brk_pin(a, quant_step(a, 1, a->pat.tr[1].lane[0].len), notes[0] & 7);
        else if (bm_is_tr(&a->pat, t)) rec_tr(a, t, notes[0]);
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
    int t = bm_app_chord_track(a), root, sc = app_scale(a, t, &root);
    bm_pad_chord pc = bm_chord_pad(m->chord_set, pad);
    int ext = chord_ext(a, pc.ext);
    uint8_t notes[4];
    int n = bm_chord_notes(root, sc, pc.degree, ext, m->chord_oct + a->oct, notes);
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
    bm_chord_name(root, sc, pc.degree, ext, a->chord_name, sizeof a->chord_name);
    bm_roman(root, sc, pc.degree, ext, a->chord_roman, sizeof a->chord_roman);
    a->last_pad = (int8_t)pad;
    play_notes(a, k, t, notes, n, 95, 1);
}

static int base_oct(const bm_app *a, int t) { return (t == 2 ? 2 : 3) + a->oct; }

/* Tocar el instrumento del canal con una tecla (páginas que no son SEQ). */
static void play_key_down(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    const bm_meta *m = &a->pat.meta;
    if (kind == KIND_TR) {
        if (w >= 0) {
            uint8_t n = (uint8_t)w;
            a->sel_lane[t] = (uint8_t)w;
            play_notes(a, k, t, &n, 1, 110, 0);
        }
        return;
    }
    if (kind == KIND_BRK) {
        int sl = w >= 0 ? w : b;
        if (sl < 0 || sl >= BM_SLICES) return;
        uint8_t n = (uint8_t)sl;
        play_notes(a, k, 1, &n, 1, 110, 0);
        return;
    }
    int root, sc = app_scale(a, t, &root);
    if ((a->view == V_CH && a->page == PG_SCL) || m->scale_lock) {
        if (w >= 0) {
            uint8_t n = (uint8_t)bm_scale_note(root, sc, w, base_oct(a, t));
            play_notes(a, k, t, &n, 1, 105, 0);
        } else if (b < 7) {
            uint8_t notes[4];
            int ext = chord_ext(a, m->voicing % 5);
            int n = bm_chord_notes(root, sc, b, ext, base_oct(a, t), notes);
            bm_chord_name(root, sc, b, ext, a->chord_name, sizeof a->chord_name);
            bm_roman(root, sc, b, ext, a->chord_roman, sizeof a->chord_roman);
            play_notes(a, k, t, notes, n, 95, 1);
        } else a->chord_mod |= (uint8_t)(1u << (b - 7));
        return;
    }
    int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
    uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
    play_notes(a, k, t, &n, 1, 105, 0);
}

/* ------------------------------------------------------------ SEQ: pasos */
static void seq_step_down(bm_app *a, int w)
{
    int t = a->track, kind = app_kind(a, t);
    if (a->held_white >= 0) return;
    int s = a->step_page * BM_PAGE_STEPS + w;
    int len = bm_trk_len(&a->pat, t, kind == KIND_NOTE ? -1 : (t == 1 ? 0 : tr_lane(a)));
    if (s >= len) { app_toast(a, "FUERA DEL LARGO"); return; }
    app_undo_snapshot(a, t);
    a->held_white = (int8_t)w; a->held_step = (int8_t)s;
    a->held_long = 0; a->held_edit = 0; a->held_notes = 0; a->step_t0 = a->now;
    if (t == 1) {
        a->held_had_event = bm_brk_step(&a->pat.tr[1], s, NULL) >= 0;
        if (!a->held_had_event) { app_push_i(a, CMD_SET_TRSTEP, 1, BRK_GEN, (uint8_t)s, ST_ON); app_edited(a); }
        return;
    }
    if (kind == KIND_TR) {
        int lane = tr_lane(a);
        a->held_had_event = (bm_trk_trc(&a->pat, t)->st[lane][s] & ST_ON) != 0;
        if (!a->held_had_event) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, ST_ON); app_edited(a); }
        return;
    }
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    a->held_had_event = n > 0;
    if (!n) {
        bm_event e; bm_event_clear(&e);
        e.note = a->last_note[t];
        app_push_ev(a, t, s, 0, &e);
        app_edited(a);
    }
}

static void seq_step_up(bm_app *a, int w)
{
    if (w != a->held_white) return;
    int t = a->track, s = a->held_step;
    if (s >= 0 && a->held_had_event && !a->held_long && !a->held_edit) {
        if (t == 1) {
            if (bm_brk_step(&a->pat.tr[1], s, NULL) > 0) brk_to_gen(a, s);     /* fijo: vuelve al generador */
            else { for (int l = 0; l <= BM_SLICES; l++) if (a->pat.tr[1].st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0); app_edited(a); }
        } else if (bm_is_tr(&a->pat, t)) { app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)tr_lane(a), (uint8_t)s, 0); app_edited(a); }
        else {
            bm_event e; bm_event_clear(&e);
            for (int k = 0; k < BM_EVENTS; k++) app_push_ev(a, t, s, k, &e);
            app_edited(a);
        }
    }
    a->held_white = -1; a->held_step = -1; a->held_long = 0; a->held_edit = 0;
}

/* nota en el paso sostenido (negras 1-8 = grados de la escala) */
static void step_note(bm_app *a, int note)
{
    int t = a->track, s = a->held_step;
    int idx[4], n = syn_step_events(a, t, s, idx);
    a->held_edit = 1;
    for (int j = 0; j < n; j++)
        if (bm_evc(&a->pat, t, s, idx[j])->note == note && (a->held_had_event || a->held_notes)) {   /* ya estaba: se quita */
            bm_event e; bm_event_clear(&e);
            app_push_ev(a, t, s, idx[j], &e); app_edited(a);
            return;
        }
    int slot = -1;
    if (!a->held_had_event && !a->held_notes) slot = n ? idx[0] : 0;   /* el primero reemplaza la nota de relleno */
    else for (int e = 0; e < BM_EVENTS && slot < 0; e++) if (bm_event_empty(bm_evc(&a->pat, t, s, e))) slot = e;
    if (slot < 0) { app_toast(a, "4 NOTAS POR PASO"); return; }
    bm_event e;
    if (n) e = *bm_evc(&a->pat, t, s, idx[0]); else bm_event_clear(&e);
    e.note = (uint8_t)note;
    app_push_ev(a, t, s, slot, &e);
    a->held_notes++;
    a->last_note[t] = (uint8_t)note;
    app_edited(a);
}

/* ------------------------------------------------------------ capas: pasos pintados */
static void paint_accent(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w, kind = app_kind(a, t);
    app_undo_snapshot(a, t);
    if (kind == KIND_NOTE) {
        if (s >= a->pat.trk[t].length) return;
        int idx[4], n = syn_step_events(a, t, s, idx);
        if (!n) { bm_event e; bm_event_clear(&e); e.note = a->last_note[t]; e.vel = BM_VEL_ACC; app_push_ev(a, t, s, 0, &e); app_edited(a); return; }
        for (int j = 0; j < n; j++) {
            bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
            e.vel = e.vel >= BM_VEL_ACC ? BM_VEL_GHOST : e.vel <= BM_VEL_GHOST ? BM_VEL_NORMAL : BM_VEL_ACC;
            app_push_ev(a, t, s, idx[j], &e);
        }
        app_edited(a);
        return;
    }
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = t == 1 ? 0 : tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    if (t == 1) {
        uint8_t v; int p = bm_brk_step(tr, s, &v);
        if (p < 0) { app_push_i(a, CMD_SET_TRSTEP, 1, BRK_GEN, (uint8_t)s, ST_ON | ST_ACC); }
        else app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)p, (uint8_t)s, v ^ ST_ACC);
    } else {
        uint8_t v = tr->st[lane][s];
        app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (v & ST_ON) ? (v ^ ST_ACC) : (ST_ON | ST_ACC));
    }
    app_edited(a);
}

static void paint_repeat(bm_app *a, int w)
{
    int t = a->track, s = a->step_page * 16 + w, kind = app_kind(a, t);
    app_undo_snapshot(a, t);
    if (kind == KIND_NOTE) {
        if (s >= a->pat.trk[t].length) return;
        int idx[4], n = syn_step_events(a, t, s, idx);
        for (int j = 0; j < n; j++) {
            bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
            e.len = (e.len & BM_LEN_TIE) ? (uint8_t)(e.len & BM_LEN_MASK) : (uint8_t)(e.len | BM_LEN_TIE);
            app_push_ev(a, t, s, idx[j], &e);
        }
        if (n) app_edited(a);
        return;
    }
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    int lane = t == 1 ? 0 : tr_lane(a);
    if (s >= tr->lane[lane].len) return;
    uint8_t v = 0;
    int ln = lane;
    if (t == 1) { int p = bm_brk_step(tr, s, &v); ln = p < 0 ? 0 : p; } else v = tr->st[lane][s];
    if (!(v & ST_ON)) v = ST_ON;
    int r = (v & ST_FLAM) ? 4 : (v & ST_RAT_MASK) >> ST_RAT_SH;
    r = (r + 1) % 5;
    v = (uint8_t)(v & ~(ST_RAT_MASK | ST_FLAM));
    if (r == 4) v |= ST_FLAM; else v |= (uint8_t)(r << ST_RAT_SH);
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)ln, (uint8_t)s, v);
    app_edited(a);
}

static void set_len(bm_app *a, int t, int len)
{
    len = bm_clampi(len, 1, BM_STEPS);
    if (bm_is_grid(&a->pat, t)) { for (int l = 0; l < BM_LANES; l++) app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)l, 0, len); app_edited(a); }
    else app_set_track(a, t, TRK_OFF(length), len);
    char b[24]; snprintf(b, sizeof b, "T%d LARGO %d", t + 1, len);
    app_toast(a, b);
}

/* El sampler: CAPTURAR deja fija la variación que está sonando en este compás. */
static void capture(bm_app *a)
{
    const bm_tr *tr = &a->pat.tr[1];
    int len = tr->lane[0].len ? tr->lane[0].len : 16;
    app_undo_snapshot(a, 1);
    uint32_t blk = a->ctx.st.gen_blk;
    int n = 0;
    for (int k = 0; k < 16; k++) {
        int s = (int)((blk * 16u + (uint32_t)k) % (uint32_t)len);
        uint8_t sl = a->ctx.st.gen_sl[k], fx = a->ctx.st.gen_fx[k];
        if (sl == 0xFF || (fx & GEN_PIN)) continue;
        if (fx & GEN_SKIP) { for (int l = 0; l <= BM_SLICES; l++) if (tr->st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0); continue; }
        uint8_t v = ST_ON | ((fx & GEN_ACC) ? ST_ACC : 0);
        int rt = fx & GEN_RT_MASK;
        if (rt >= 1 && rt <= 3) v |= (uint8_t)(rt << ST_RAT_SH);
        for (int l = 0; l <= BM_SLICES; l++) if (tr->st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0);
        app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)(sl + 1), (uint8_t)s, v);
        n++;
    }
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, "CAPTURADO: %d PASOS FIJOS", n);
    app_toast(a, b);
}

/* ------------------------------------------------------------ CORTAR */
static uint32_t snap_len(const bm_app *a, const bm_take *tk)
{
    uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10;
    uint32_t eighth = (uint32_t)BM_SMP_RATE * 300u / bpm;        /* muestras por corchea */
    return a->cut_snap == 1 ? eighth / 2 : a->cut_snap == 2 ? eighth : 0;
}

static void take_changed(bm_app *a, int id)
{
    a->take_dirty |= (uint8_t)(1u << id);
    a->t_edit = a->now;
    if (a->pat.trk[1].source == id || a->pat.trk[1].loop_b == id) a->src_dirty = 1;
    if (a->view == V_CH && (a->page == PG_CUT || a->page == PG_TAKES)) app_preview_take(a, (uint8_t)id);
    if (a->page == PG_CUT) app_cut_wave(a);
}

static void cuts_set(bm_app *a, bm_take *tk, const uint32_t *m, int n)
{
    /* ordena, sin repetidos; el primero siempre en 0 */
    uint32_t v[BM_SLICES + 1];
    int k = 0;
    v[k++] = 0;
    for (int i = 0; i < n && k < BM_SLICES; i++) if (m[i] > 0 && m[i] < tk->len) v[k++] = m[i];
    for (int i = 1; i < k; i++) for (int j = i; j > 0 && v[j] < v[j - 1]; j--) { uint32_t x = v[j]; v[j] = v[j - 1]; v[j - 1] = x; }
    int o = 0;
    for (int i = 0; i < k; i++) if (!o || v[i] > tk->marks[o - 1] + 64) tk->marks[o++] = v[i];
    tk->nmarks = (uint8_t)o;
    if (a->cut_sel >= o) a->cut_sel = (uint8_t)(o ? o - 1 : 0);
    take_changed(a, tk->id);
}

static void cut_marks(const bm_take *tk, uint32_t out[BM_SLICES], int *n)
{
    if (tk->nmarks) { memcpy(out, tk->marks, sizeof(uint32_t) * tk->nmarks); *n = tk->nmarks; return; }
    for (int i = 0; i < BM_SLICES; i++) out[i] = tk->len * (uint32_t)i / BM_SLICES;
    *n = BM_SLICES;
}

static void cut_toggle_eighth(bm_app *a, int w)
{
    bm_take *tk = &a->take[a->cut_take % BM_TAKES];
    if (!tk->valid) return;
    uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10;
    uint32_t pos = (uint32_t)w * ((uint32_t)BM_SMP_RATE * 300u / bpm);
    if (pos >= tk->len) { app_toast(a, "MAS ALLA DEL FINAL"); return; }
    uint32_t m[BM_SLICES]; int n;
    if (tk->nmarks) cut_marks(tk, m, &n);
    else { m[0] = 0; n = 1; }                                    /* de 8 iguales a cortes a mano */
    uint32_t tol = (uint32_t)BM_SMP_RATE * 60u / bpm;           /* media corchea */
    for (int i = 0; i < n; i++)
        if ((m[i] > pos ? m[i] - pos : pos - m[i]) < tol) {
            if (i == 0) { app_toast(a, "EL CORTE 1 ES EL INICIO"); return; }
            m[i] = m[--n];
            cuts_set(a, tk, m, n);
            return;
        }
    if (n >= BM_SLICES) { app_toast(a, "MAXIMO 8 CORTES"); return; }
    m[n++] = pos;
    cuts_set(a, tk, m, n);
}

/* Cortes en los golpes: los 7 ataques más fuertes (energía que sube de una columna a la siguiente). */
static void cut_hits(bm_app *a)
{
    bm_take *tk = &a->take[a->cut_take % BM_TAKES];
    if (!tk->valid) return;
    int rise[232];
    for (int x = 0; x < 232; x++) {
        int e = a->wave_cut[0][x] * 2 + a->wave_cut[1][x] + a->wave_cut[2][x];
        int p = x ? a->wave_cut[0][x - 1] * 2 + a->wave_cut[1][x - 1] + a->wave_cut[2][x - 1] : 0;
        rise[x] = e - p;
    }
    uint32_t m[BM_SLICES]; int n = 0;
    for (int k = 0; k < BM_SLICES - 1; k++) {
        int best = -1;
        for (int x = 2; x < 232; x++) if (rise[x] > 40 && (best < 0 || rise[x] > rise[best])) best = x;
        if (best < 0) break;
        m[n++] = (uint32_t)((uint64_t)tk->len * (uint32_t)best / 232u);
        for (int x = best - 6; x <= best + 6; x++) if (x >= 0 && x < 232) rise[x] = 0;
    }
    cuts_set(a, tk, m, n);
    char b[24]; snprintf(b, sizeof b, "%d CORTES EN LOS GOLPES", tk->nmarks);
    app_toast(a, b);
}

static void open_cut(bm_app *a, uint8_t src)
{
    if (src == BM_SRC_NONE || (src & BM_SRC_ROM) || !a->take[src].valid) { app_toast(a, "LA FABRICA NO SE CORTA"); return; }
    a->cut_take = src; a->cut_sel = 0;
    app_enter(a, 1, PG_CUT);
    app_preview_take(a, src);
    app_cut_wave(a);
}

/* ------------------------------------------------------------ teclas */
static void song_append(bm_app *a, int p)
{
    if (a->song.n >= BM_SONG_MAX) { app_toast(a, "SONG LLENA"); return; }
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

/* Grabación libre (como una loopera): en un canal de notas vacío, REC no tiene largo fijo.
 * Al parar REC, el canal queda con los compases completos que tocaste (1-4) y cada nota
 * con lo que la sostuviste. */
static int notes_empty(const bm_app *a, int t)
{
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) if (!bm_event_empty(bm_evc(&a->pat, t, s, k))) return 0;
    return 1;
}

static void free_rec_start(bm_app *a)
{
    int t = a->track;
    if (app_kind(a, t) != KIND_NOTE || !notes_empty(a, t)) return;
    a->rec_free = (uint8_t)(t + 1);
    a->rec_free_t0 = app_playing(a) ? a->ctx.st.btick : 0xFFFFFFFFu;
    app_set_track(a, t, TRK_OFF(length), BM_STEPS);
    app_toast(a, "REC LIBRE: REC OTRA VEZ CIERRA");
}

static void free_rec_close(bm_app *a)
{
    int t = a->rec_free - 1;
    a->rec_free = 0;
    int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
    int bars = 1;
    if (a->rec_free_t0 != 0xFFFFFFFFu) {
        uint32_t el = a->ctx.st.btick - a->rec_free_t0;
        bars = bm_clampi((int)((el + BM_BAR_TICKS / 2) / BM_BAR_TICKS), 1, 4);
    }
    int len = bm_clampi(bars * spb, 1, BM_STEPS);
    /* lo grabado se dobla sobre el largo nuevo, en su mismo lugar del compás */
    bm_notes *nb = &a->redo.nt;
    a->redo_track = -1;
    for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) bm_event_clear(&nb->ev[s][k]);
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&a->pat, t, s, k);
            if (bm_event_empty(e)) continue;
            int d = s % len;
            for (int j = 0; j < BM_EVENTS; j++)
                if (bm_event_empty(&nb->ev[d][j])) { nb->ev[d][j] = *e; break; }
                else if (nb->ev[d][j].note == e->note) break;
        }
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)t, nb);
    app_set_track(a, t, TRK_OFF(length), len);
    char b[30]; snprintf(b, sizeof b, "T%d: %d COMPAS%s", t + 1, bars, bars > 1 ? "ES" : "");
    app_toast(a, b);
}

static void toggle_mute(bm_app *a, int t)
{
    app_set_track(a, t, TRK_OFF(mute), a->pat.trk[t].mute ? 0 : 1);
    char b[24]; snprintf(b, sizeof b, "T%d %s", t + 1, a->pat.trk[t].mute ? "SUENA" : "MUTE");
    app_toast(a, b);
}

static void toggle_lane_mute(bm_app *a, int t, int lane)
{
    uint16_t m = (uint16_t)(bm_trk_trc(&a->pat, t)->mute ^ (1u << lane));
    app_push_i(a, CMD_LANE_MUTE, (uint8_t)t, 0, 0, m);
    app_edited(a);
}

static void scene_store(bm_app *a, int s)
{
    bm_scene *sc = &a->song.scene[s];
    sc->valid = 1; sc->pattern = a->ctx.st.pattern; sc->trk_mute = 0;
    for (int t = 0; t < BM_TRACKS; t++) if (a->pat.trk[t].mute) sc->trk_mute |= (uint8_t)(1u << t);
    for (int t = 0; t < BM_TR_TRACKS; t++) sc->lane_mute[t] = a->pat.tr[t].mute;
    a->song_dirty = 1; a->t_edit = a->now;
    char b[24]; snprintf(b, sizeof b, "ESCENA %c GUARDADA", 'A' + s);
    app_toast(a, b);
}

static void scene_launch(bm_app *a, int s)
{
    if (!a->song.scene[s].valid) { app_toast(a, "ESCENA VACIA (HOME > ESCENAS)"); return; }
    if (a->launch_held & ~(1u << s)) {               /* otra sostenida: cadena */
        if (a->chain_n < 8) a->chain[a->chain_n++] = (uint8_t)s;
        app_push_ptr(a, CMD_CHAIN, a->chain_n, a->chain);
        app_toast(a, "CADENA DE ESCENAS");
    } else {
        a->chain_n = 0;
        a->chain[a->chain_n++] = (uint8_t)s;
        app_push(a, CMD_SCENE, (uint8_t)s, 0, 0);
        char b[16]; snprintf(b, sizeof b, "ESCENA %c", 'A' + s);
        app_toast(a, b);
    }
    a->launch_held |= (uint8_t)(1u << s);
}

/* Gramática de HOME (pantalla, MEZCLA y capa): blancas patrones, negras mutes, escenas y FILL. */
static void home_key(bm_app *a, int k, int w, int b)
{
    if (w >= 0) {
        a->key_role[k] = ROLE_PAT;
        if (a->pat_held & ~(1u << w)) {                     /* otra blanca sostenida: cadena */
            if (a->pchain_n < 8) a->pchain[a->pchain_n++] = (uint8_t)w;
            app_push_ptr(a, CMD_PCHAIN, a->pchain_n, a->pchain);
            char t[24]; snprintf(t, sizeof t, "CADENA DE %d PATRONES", a->pchain_n);
            app_toast(a, t);
        } else {
            a->pchain_n = 0;
            a->pchain[a->pchain_n++] = (uint8_t)w;
            app_select_pattern(a, w);
        }
        a->pat_held |= (uint16_t)(1u << w);
        return;
    }
    if (b < 6) { a->key_role[k] = ROLE_MUTE; a->key_t0[k] = a->now; return; }
    if (b < 10) { a->key_role[k] = ROLE_SCENE; scene_launch(a, b - 6); return; }
    a->key_role[k] = ROLE_FILL;
    app_push(a, CMD_FILL, 1, 0, 0); a->fill = 1;
}

static void key_release(bm_app *a, int k, int w, int b)
{
    if (a->key_nn[k]) {
        int t = a->key_trk[k];
        if (app_kind(a, t) == KIND_NOTE) for (int i = 0; i < a->key_nn[k]; i++) app_push(a, CMD_NOTE_OFF, (uint8_t)t, a->key_notes[k][i], 0);
        if (a->key_rec_step[k] >= 0 && a->key_rec_slot[k] >= 0 && app_kind(a, t) == KIND_NOTE) {
            int tps = bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            int steps = bm_clampi((int)((a->ctx.st.rtick - a->key_rec_tick[k] + (uint32_t)tps / 2) / (uint32_t)tps), 1, BM_STEPS);   /* lo que la sostuviste */
            int s = a->key_rec_step[k];
            for (int e = 0; e < BM_EVENTS; e++) {
                bm_event ev = *bm_evc(&a->pat, t, s, e);
                for (int i = 0; i < a->key_nn[k]; i++)
                    if (!bm_event_empty(&ev) && ev.note == a->key_notes[k][i]) { ev.len = (uint8_t)steps; app_push_ev(a, t, s, e, &ev); }
            }
        }
        a->key_nn[k] = 0;
        a->key_rec_step[k] = -1;
    }
    if (a->punch_key == k) { app_push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    switch (a->key_role[k]) {
    case ROLE_MUTE:
        if (a->solo_key == k && a->solo_on) {                 /* fin del solo: vuelven los mutes de antes */
            for (int t = 0; t < BM_TRACKS; t++) app_set_track(a, t, TRK_OFF(mute), (a->solo_saved >> t) & 1);
            a->solo_on = 0; a->solo_key = -1;
        } else if (b >= 0 && b < 6) toggle_mute(a, b);
        if (a->solo_key == k) a->solo_key = -1;
        break;
    case ROLE_FILL: app_push(a, CMD_FILL, 0, 0, 0); a->fill = 0; break;
    case ROLE_SCENE: if (b >= 6 && b < 10) a->launch_held &= (uint8_t)~(1u << (b - 6)); break;
    case ROLE_BRKFX:
        if (b >= 8) { a->brk_live &= (uint8_t)~(1u << (b - 8)); app_push(a, CMD_BRK_LIVE, a->brk_live, 0, 0); }
        break;
    case ROLE_LMUTE:
        if (a->layer == L_LANEMUTE) a->layer = L_NONE;
        else if (a->now - a->key_t0[k] < UI_HOLD_MS) toggle_lane_mute(a, a->track, tr_lane(a));
        break;
    default: break;
    }
    a->key_role[k] = ROLE_NONE;
    a->key_t0[k] = 0;
    if (b >= 0 && b < 4) a->chord_mod &= (uint8_t)~(1u << b);
    if (b >= 7 && b <= 10) a->chord_mod &= (uint8_t)~(1u << (b - 7));
    if (w >= 0) {
        if (a->roll_mask & (1u << w)) { a->roll_mask &= (uint16_t)~(1u << w); app_push_i(a, CMD_ROLL, a->track, (uint8_t)w, 0, 0); }
        a->erase_mask &= (uint16_t)~(1u << w);
        if (!a->erase_mask) a->erase_syn = 0;
        a->pat_held &= (uint16_t)~(1u << w);
        seq_step_up(a, w);
        if (w < 4) a->launch_held &= (uint8_t)~(1u << w);
    }
}

static void layer_key(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    switch (a->layer) {
    case L_HOME: home_key(a, k, w, b); return;
    case L_LEN:
        if (w >= 0) set_len(a, t, a->len_page * 16 + w + 1);
        else if (b >= 6 && b <= 9) {                                 /* compases completos */
            int spb = BM_BAR_TICKS / bm_speed_tps[a->pat.trk[t].speed % SPD_COUNT];
            set_len(a, t, (b - 5) * spb);
        } else if (b < SPD_COUNT) {
            app_set_track(a, t, TRK_OFF(speed), b);
            char s[24]; snprintf(s, sizeof s, "T%d VELOCIDAD %s", t + 1, bm_speed_names[b]);
            app_toast(a, s);
        }
        return;
    case L_ACC: if (w >= 0) paint_accent(a, w); return;
    case L_REP: if (w >= 0) paint_repeat(a, w); return;
    case L_LANEMUTE: if (w >= 0 && kind == KIND_TR) toggle_lane_mute(a, t, w); return;
    case L_PUNCH:
        if (w >= 0) { app_push(a, CMD_PUNCH, (uint8_t)w, 0, 0); a->punch_key = (int8_t)k; }
        else if (b < 6) toggle_mute(a, b);
        return;
    case L_ERASE:
        if (b >= 0) {
            if (b == 0) app_copy_track(a, t);
            else if (b == 1) app_paste_track(a, t);
            else if (b == 2) app_clear_track(a, t);
            else if (b == 3 && t == 1) capture(a);
            return;
        }
        if (kind == KIND_TR) {
            if (app_playing(a)) { if (!a->erase_mask) app_undo_snapshot(a, t); a->erase_mask |= (uint16_t)(1u << w); }
            else {
                const bm_tr *tr = bm_trk_trc(&a->pat, t);
                app_undo_snapshot(a, t);
                for (int s = 0; s < BM_STEPS; s++) if (tr->st[w][s]) app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)w, (uint8_t)s, 0);
                app_edited(a);
                char m[24]; snprintf(m, sizeof m, "%s BORRADO", bm_lane_name(a, t, w)); app_toast(a, m);
            }
        } else {
            if (app_playing(a)) { if (!a->erase_syn) app_undo_snapshot(a, t); a->erase_syn = 1; a->erase_mask |= (uint16_t)(1u << w); }
            else {
                int s = a->step_page * 16 + w;
                app_undo_snapshot(a, t);
                if (t == 1) { for (int l = 0; l <= BM_SLICES; l++) if (a->pat.tr[1].st[l][s]) app_push_i(a, CMD_SET_TRSTEP, 1, (uint8_t)l, (uint8_t)s, 0); }
                else { bm_event e; bm_event_clear(&e); for (int kk = 0; kk < BM_EVENTS; kk++) app_push_ev(a, t, s, kk, &e); }
                app_edited(a);
            }
        }
        return;
    case L_ROLL:
        if (w >= 0 && (kind == KIND_TR || w < BM_SLICES)) {
            a->roll_mask |= (uint16_t)(1u << w);
            if (kind == KIND_TR) a->sel_lane[t] = (uint8_t)w;
            app_push_i(a, CMD_ROLL, (uint8_t)t, (uint8_t)w, 1, bm_roll_ticks[a->roll_rate % 5]);
        }
        return;
    case L_CHORDS:
        if (w >= 0) play_pad(a, k, w);
        else if (b < 4) a->chord_mod |= (uint8_t)(1u << b);
        return;
    case L_ARP: {
        int note = BM_KEY_BASE_NOTE + k + 12 * a->oct - (t == 2 ? 12 : 0);
        uint8_t n = (uint8_t)bm_clampi(note, 0, 127);
        play_notes(a, k, t, &n, 1, 105, 0);
        return;
    }
    case L_COPY:
        if (w >= 0) {
            if (app_playing(a)) { app_toast(a, "PARA (STOP) PARA COPIAR"); return; }
            if (w == a->ctx.st.pattern) { app_toast(a, "ES EL PATRON ACTUAL"); return; }
            int trk = -1;
            for (int i = 0; i < BM_TRACKS; i++) if (a->keys_down[bm_black_key(i)]) trk = i;
            char l1[30], l2[30];
            if (trk >= 0) snprintf(l1, sizeof l1, "COPIAR T%d A A%02d?", trk + 1, w + 1);
            else snprintf(l1, sizeof l1, "COPIAR A%02d EN A%02d?", a->ctx.st.pattern + 1, w + 1);
            snprintf(l2, sizeof l2, "A%02d YA TIENE DATOS", w + 1);
            if (bm_store_has(&a->store, (uint8_t)w)) app_ask(a, trk >= 0 ? ASK_COPY_TRK : ASK_COPY_PAT, w, trk, l1, l2);
            else if (trk >= 0) { app_job(a, JOB_COPY_TRK, w | (trk << 4)); app_toast(a, "CANAL COPIADO"); }
            else { app_job(a, JOB_COPY_PAT, w); app_toast(a, "PATRON COPIADO"); }
        }
        return;
    case L_TONE: {
        int tp = bm_clampi(24 + k - 7, 0, 48);
        app_set_meta(a, META_OFF(transpose), tp);
        char s[24]; snprintf(s, sizeof s, "TONO %+d", tp - 24);
        app_toast(a, s);
        return;
    }
    default: return;
    }
}

static void seq_keys(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    if (w >= 0) { seq_step_down(a, w); return; }
    if (t == 1) {
        if (b < BM_SLICES) {
            if (a->held_step >= 0) {                                     /* blanca + negra: paso fijo */
                int cur = bm_brk_step(&a->pat.tr[1], a->held_step, NULL);
                if (cur == b + 1) brk_to_gen(a, a->held_step); else brk_pin(a, a->held_step, b);
                a->held_edit = 1;
                return;
            }
            uint8_t n = (uint8_t)b;
            a->key_role[k] = ROLE_NOTE;
            app_push(a, CMD_NOTE_ON, 1, n, 110);
            a->key_trk[k] = 1;
            if (a->rec && app_playing(a)) brk_pin(a, quant_step(a, 1, a->pat.tr[1].lane[0].len), b);
            return;
        }
        a->key_role[k] = ROLE_BRKFX;
        a->brk_live |= (uint8_t)(1u << (b - 8));
        app_push(a, CMD_BRK_LIVE, a->brk_live, 0, 0);
        return;
    }
    if (kind == KIND_TR) {
        if (b < 8) {
            int lane = (tr_lane(a) / 8) * 8 + b;
            a->sel_lane[t] = (uint8_t)lane;
            uint8_t n = (uint8_t)lane;
            play_notes(a, k, t, &n, 1, 100, 0);
        } else if (b == 8) {
            a->sel_lane[t] = (uint8_t)((tr_lane(a) + 8) % 16);
            app_toast(a, tr_lane(a) < 8 ? "INSTRUMENTOS 1-8" : "INSTRUMENTOS 9-16");
        } else if (b == 9) { a->key_role[k] = ROLE_LMUTE; a->key_t0[k] = a->now; }
        else { a->key_role[k] = ROLE_FILL; app_push(a, CMD_FILL, 1, 0, 0); a->fill = 1; }
        return;
    }
    if (b < 8) {                                                         /* notas: grados de la escala */
        int root, sc = app_scale(a, t, &root);
        int note = bm_scale_note(root, sc, b, base_oct(a, t) + a->note_oct);
        if (a->held_step >= 0) { step_note(a, note); return; }
        uint8_t n = (uint8_t)note;
        play_notes(a, k, t, &n, 1, 105, 0);
        if (a->rec && app_playing(a) && a->ctx.st.loop_mode[t - 2] == LOOP_OFF) rec_syn(a, k, t, &n, 1);
    } else if (b == 8 || b == 9) {
        a->note_oct = (int8_t)bm_clampi(a->note_oct + (b == 9 ? 1 : -1), -2, 2);
        char s[24]; snprintf(s, sizeof s, "NOTAS OCTAVA %+d", a->note_oct);
        app_toast(a, s);
    } else { a->key_role[k] = ROLE_FILL; app_push(a, CMD_FILL, 1, 0, 0); a->fill = 1; }
}

static void page_keys(bm_app *a, int k, int w, int b)
{
    int t = a->track, kind = app_kind(a, t);
    switch (a->page) {
    case PG_SEQ: seq_keys(a, k, w, b); return;
    case PG_CHORDS:
        if (w >= 0) play_pad(a, k, w);
        else if (b < 4) a->chord_mod |= (uint8_t)(1u << b);
        return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (w >= 0 && w < BM_SLICES) { app_push(a, CMD_BRK_SLICE, (uint8_t)w, 110, 2); return; }
        if (b == 0 || b == 1) {
            if (src == BM_SRC_NONE) return;
            app_set_track(a, 1, b == 0 ? TRK_OFF(source) : TRK_OFF(loop_b), src);
            if (b == 0) a->pat.trk[1].source = src; else a->pat.trk[1].loop_b = src;
            char n[16], s[30]; bm_app_src_name(a, src, n, sizeof n);
            snprintf(s, sizeof s, "%s ES EL LOOP %c", n, b == 0 ? 'A' : 'B');
            app_toast(a, s);
        } else if (b == 10) {
            if (src == BM_SRC_NONE || (src & BM_SRC_ROM)) { app_toast(a, "LA FABRICA NO SE BORRA"); return; }
            char l1[30]; snprintf(l1, sizeof l1, "BORRAR LA TOMA %02d?", src + 1);
            app_ask(a, ASK_TAKE_DEL, src, 0, l1, "NO SE PUEDE DESHACER");
        }
        return;
    }
    case PG_REC: return;                                     /* grabando, las teclas no suenan */
    case PG_CUT:
        if (w >= 0) cut_toggle_eighth(a, w);
        else if (b < BM_SLICES) app_push(a, CMD_BRK_SLICE, (uint8_t)b, 110, 2);
        else if (b == 8) cut_hits(a);
        else if (b == 9) { bm_take *tk = &a->take[a->cut_take % BM_TAKES]; tk->nmarks = 0; a->cut_sel = 0; take_changed(a, tk->id); app_toast(a, "8 PARTES IGUALES"); }
        return;
    default: break;
    }
    if (kind == KIND_TR && b >= 0) { seq_keys(a, k, -1, b); return; }
    if (kind == KIND_BRK && b >= 8) { seq_keys(a, k, -1, b); return; }
    play_key_down(a, k, w, b);
}

void bm_app_key(bm_app *a, int k, int down, uint32_t now)
{
    a->now = now;
    if (k < 0 || k >= BM_KEYS) return;
    if (down) {
        if (a->keys_down[k]) return;
        a->keys_down[k] = 1;
        bm_gesture_other(&a->ges, now);
        if (bm_gesture_is_down(&a->ges, BTN_HOME)) a->home_used = 1;
    } else {
        if (!a->keys_down[k]) return;
        a->keys_down[k] = 0;
    }
    if (a->view == V_BOOT) return;
    int w = bm_key_white_index(k), b = bm_key_black_index(k);
    if (!down) { key_release(a, k, w, b); return; }
    if (a->layer) { layer_key(a, k, w, b); return; }
    switch (a->view) {
    case V_HOME: case V_MIX: home_key(a, k, w, b); return;
    case V_SCENES:
        if (w >= 0 && w < 4) scene_launch(a, w);
        else if (w >= 4 && w < 8) scene_store(a, w - 4);
        else if (b >= 0) home_key(a, k, -1, b);
        return;
    case V_SONG:
        if (w >= 0) song_append(a, w);
        else if (b == 8) song_delete(a);
        return;
    case V_CH: page_keys(a, k, w, b); return;
    default: play_key_down(a, k, w, b); return;
    }
}

/* ------------------------------------------------------------ botones */
static int layer_of(const bm_app *a, int id)
{
    switch (id) {
    case BTN_HOME: return L_HOME;
    case BTN_SEQ: return L_LEN;
    case BTN_EDIT: return L_ERASE;
    case BTN_ENV: return L_ACC;
    case BTN_LFO: return L_REP;
    case BTN_FX: return L_PUNCH;
    case BTN_SCL: return L_CHORDS;
    case BTN_ARP: return bm_is_grid(&a->pat, a->track) ? L_ROLL : L_ARP;
    case BTN_SAVE: return L_COPY;
    case BTN_GLO: return L_TONE;
    default: return L_NONE;
    }
}

static void open_layer(bm_app *a, int id)
{
    if (a->layer) return;                                    /* nunca dos capas a la vez */
    int L = layer_of(a, id);
    if (!L) return;
    if (L == L_ARP && app_kind(a, a->track) != KIND_NOTE) return;
    a->layer = (uint8_t)L;
    a->ov.on = 0;
    if (L == L_ARP) app_push(a, CMD_ARP, a->track, 1, 0);
    if (L == L_ERASE) { a->erase_mask = 0; a->erase_syn = 0; }
    if (L == L_LEN) {
        int len = bm_trk_len(&a->pat, a->track, app_kind(a, a->track) == KIND_NOTE ? -1 : 0);
        a->len_page = (uint8_t)((len - 1) / 16);
    }
}

static void close_layer(bm_app *a, int id)
{
    int L = layer_of(a, id);
    if (L == L_ROLL && a->layer == L_ARP) L = L_ARP;
    if (L == L_ARP && a->layer == L_ROLL) L = L_ROLL;
    if (!L || a->layer != L) return;
    if (L == L_PUNCH) { app_push(a, CMD_PUNCH, 0xFF, 0, 0); a->punch_key = -1; }
    if (L == L_ARP) app_push(a, CMD_ARP, a->track, 0, 0);
    if (L == L_ROLL) {
        for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) app_push_i(a, CMD_ROLL, a->track, (uint8_t)l, 0, 0);
        a->roll_mask = 0;
    }
    if (L == L_ERASE) { a->erase_mask = 0; a->erase_syn = 0; }
    a->layer = L_NONE;
}

/* Toque de un botón: su página en el canal donde estás. Otra vez: la siguiente. */
static void button_page(bm_app *a, int id)
{
    int t = a->track, kind = app_kind(a, t), midi = app_midi(a, t);
    int cur = a->view == V_CH ? a->page : -1, pg = PG_SEQ;
    switch (id) {
    case BTN_SEQ:
        if (cur == PG_SEQ) pg = kind == KIND_BRK ? PG_RETRIG : (kind == KIND_NOTE && !midi) ? PG_LOOP : PG_SEQ;
        else pg = PG_SEQ;
        break;
    case BTN_EDIT:
        if (cur == PG_TAKES) { open_cut(a, bm_app_takes_sel(a)); return; }
        if (cur == PG_REC) {
            if (a->ctx.st.smp_unsaved) { app_toast(a, "SE GUARDA AL PARAR"); return; }
            open_cut(a, a->pat.trk[1].source); return;
        }
        if (midi) pg = PG_MIDI;
        else if (cur == PG_SOUND && kind == KIND_TR) pg = PG_INST;
        else pg = PG_SOUND;
        break;
    case BTN_ENV:
        if (kind == KIND_TR) pg = PG_INST;
        else if (kind == KIND_BRK) pg = PG_SOUND;
        else if (midi) pg = PG_MIDI;
        else { if (cur == PG_ENV) a->env_page ^= 1; else a->env_page = 0; pg = PG_ENV; }
        break;
    case BTN_LFO: pg = PG_LFO; break;
    case BTN_FX:
        if (midi) { app_toast(a, "MIDI: SIN MEZCLA"); pg = PG_MIDI; } else pg = PG_MIX;
        break;
    case BTN_SCL: pg = cur == PG_SCL ? PG_CHORDS : PG_SCL; break;
    case BTN_ARP: pg = PG_ARP; break;
    default: return;
    }
    app_enter(a, t, pg);
}

static void home_combo(bm_app *a, int id)
{
    a->home_used = 1;
    switch (id) {
    case BTN_OCT_DN: app_do_undo(a); break;
    case BTN_OCT_UP: app_do_redo(a); break;
    case BTN_PLAY: app_push(a, CMD_PLAY, app_playing(a) ? 2 : 1, 0, 0); app_toast(a, "DESDE EL PASO 1"); break;
    case BTN_REC: a->song_rec = !a->song_rec; a->song_rec_loops = a->ctx.st.loops; app_toast(a, a->song_rec ? "SONG REC ON" : "SONG REC OFF"); break;
    case BTN_SEQ: app_set_view(a, V_SONG); break;
    case BTN_FX: app_set_view(a, V_FXG); break;
    case BTN_SCL: app_set_view(a, V_SCLG); break;
    case BTN_SAVE: a->save_row = 1; a->save_target = (uint8_t)((a->ctx.st.pattern + 1) % BM_PATTERNS); app_set_view(a, V_SAVE); break;
    case BTN_EDIT: {
        a->copy_src = a->track;
        int k = app_kind(a, a->track);
        a->copy_dst = 0xFF;
        for (int i = 1; i < BM_TRACKS && a->copy_dst == 0xFF; i++) {
            int d = (a->track + i) % BM_TRACKS;
            if (d == 1 || k == KIND_BRK) continue;
            if (bm_is_free(d) || app_kind(a, d) == k) a->copy_dst = (uint8_t)d;
        }
        if (a->copy_dst == 0xFF) { app_toast(a, "EL SAMPLER NO SE COPIA"); break; }
        app_set_view(a, V_COPYCH);
        break;
    }
    default: app_toast(a, "HOME + ESTE BOTON: NADA"); break;
    }
}

static void copy_channel(bm_app *a)
{
    int s = a->copy_src, d = a->copy_dst;
    int ks = app_kind(a, s);
    app_undo_snapshot(a, d);
    if (bm_is_free(d)) {
        int want = ks == KIND_TR ? ENG_KIT : (a->pat.trk[s].engine == ENG_BASS ? ENG_MONO : a->pat.trk[s].engine);
        if (a->pat.trk[d].engine != want) {
            app_push(a, CMD_ENGINE, (uint8_t)d, (uint8_t)want, 0);
        }
    }
    memcpy(&a->clip, app_track_data(a, s), app_track_size(s));
    app_push_ptr(a, CMD_TRACK_BULK, (uint8_t)d, &a->clip);
    const uint8_t *src = (const uint8_t *)&a->pat.trk[s];
    for (unsigned off = 0; off < sizeof(bm_track); off++)
        if (off != TRK_OFF(engine) && off != TRK_OFF(mute) && off != TRK_OFF(midi_ch)) app_push(a, CMD_SET_TRACK, (uint8_t)d, (uint8_t)off, src[off]);
    app_edited(a);
    char b[30]; snprintf(b, sizeof b, "T%d COPIADO EN T%d", s + 1, d + 1);
    app_toast(a, b);
    app_enter(a, d, PG_SEQ);
}

static void ask_yes(bm_app *a)
{
    int k = a->ask, x = a->ask_arg, y = a->ask_arg2;
    a->ask = ASK_NONE;
    char b[30];
    switch (k) {
    case ASK_CLEAR: app_clear_track(a, x); break;
    case ASK_ENGINE:
        a->eng_pending = 0xFF;
        app_undo_snapshot(a, x);
        app_push(a, CMD_ENGINE, (uint8_t)x, (uint8_t)y, 0);
        app_edited(a);
        snprintf(b, sizeof b, "T%d AHORA ES %s", x + 1, bm_engine_names[y]);
        app_toast(a, b);
        break;
    case ASK_COPY_PAT: app_job(a, JOB_COPY_PAT, x); app_toast(a, "PATRON COPIADO"); break;
    case ASK_COPY_TRK: app_job(a, JOB_COPY_TRK, x | (y << 4)); app_toast(a, "CANAL COPIADO"); break;
    case ASK_TAKE_DEL:
        app_job(a, JOB_TAKE_ERASE, x);
        if (a->pat.trk[1].source == x) { uint8_t s = bm_rom_nloops ? BM_SRC_ROM : BM_SRC_NONE; app_set_track(a, 1, TRK_OFF(source), s); a->pat.trk[1].source = s; }
        if (a->pat.trk[1].loop_b == x) { app_set_track(a, 1, TRK_OFF(loop_b), BM_SRC_NONE); a->pat.trk[1].loop_b = BM_SRC_NONE; }
        snprintf(b, sizeof b, "TOMA %02d BORRADA", x + 1);
        app_toast(a, b);
        break;
    case ASK_INIT: app_init_sound(a, x); break;
    default: break;
    }
}

static void tap_tempo(bm_app *a)
{
    if (a->tap_n && a->now - a->tap_t[(a->tap_n - 1) & 3] > 2000) a->tap_n = 0;
    a->tap_t[a->tap_n & 3] = a->now;
    a->tap_n++;
    if (a->tap_n < 2) { app_toast(a, "TAP"); return; }
    int n = a->tap_n > 4 ? 4 : a->tap_n;
    uint32_t first = a->tap_t[(a->tap_n - n) & 3], last = a->tap_t[(a->tap_n - 1) & 3];
    uint32_t iv = (last - first) / (uint32_t)(n - 1);
    if (!iv) return;
    int bpm = bm_clampi((int)(600000u / iv), 400, 2400);
    app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
    char b[16]; snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
    app_overlay(a, -1, "TAP TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, C_CYAN);
}

void app_on_gesture(void *u, int id, bm_gkind k)
{
    bm_app *a = (bm_app *)u;
    if (a->view == V_BOOT) return;
    uint16_t bit = (uint16_t)(1u << id);
    if (id != BTN_HOME && (a->home_mod & bit)) {             /* HOME + botón = su versión global */
        if ((k == G_TAP || k == G_HOLD || k == G_PRESS) && !(a->home_done & bit)) { a->home_done |= bit; home_combo(a, id); }
        return;
    }
    if (a->ask) {                                            /* pregunta: OCT+ sí, HOME no, otro botón la cierra */
        if (id == BTN_OCT_UP && k == G_PRESS) { ask_yes(a); return; }
        if (k == G_PRESS || k == G_TAP || k == G_HOLD) {
            a->ask = ASK_NONE;
            if (id == BTN_HOME || id == BTN_OCT_DN) { app_toast(a, "CANCELADO"); return; }
        }
    }
    char b[30];
    switch (id) {
    case BTN_PLAY:
        if (k != G_PRESS) return;
        if (a->layer == L_TONE) { tap_tempo(a); return; }
        app_push(a, CMD_PLAY, app_playing(a) ? 0 : 1, 0, 0);
        return;
    case BTN_OCT_DN: case BTN_OCT_UP: {
        if (k != G_PRESS) return;
        int d = id == BTN_OCT_UP ? 1 : -1;
        if (a->layer == L_ERASE) { if (d < 0) app_do_undo(a); else app_do_redo(a); return; }
        if (a->layer == L_LEN) {
            a->len_page = (uint8_t)bm_clampi(a->len_page + d, 0, 3);
            snprintf(b, sizeof b, "LARGOS %d-%d", a->len_page * 16 + 1, a->len_page * 16 + 16); app_toast(a, b);
            return;
        }
        if (a->view == V_COPYCH) { if (d > 0) copy_channel(a); else app_set_view(a, V_CH); return; }
        bool steps = (a->view == V_CH && a->page == PG_SEQ) || a->layer == L_ACC || a->layer == L_REP || a->layer == L_ERASE;
        if (steps) {
            int t = a->track, len = bm_trk_len(&a->pat, t, app_kind(a, t) == KIND_NOTE ? -1 : (t == 1 ? 0 : tr_lane(a)));
            int pages = (len + 15) / 16;
            a->step_page = (uint8_t)bm_clampi(a->step_page + d, 0, pages > 0 ? pages - 1 : 0);
            snprintf(b, sizeof b, "PAGINA %d/%d", a->step_page + 1, pages);
        } else {
            a->oct = (int8_t)bm_clampi(a->oct + d, -2, 2);
            snprintf(b, sizeof b, "OCTAVA %+d", a->oct);
        }
        app_toast(a, b);
        return;
    }
    case BTN_REC:
        if (k == G_TAP) {
            if (a->view == V_CH && a->track == 1) {
                if (a->page != PG_REC) { app_enter(a, 1, PG_REC); }
                if (a->ctx.st.smp_rec || a->ctx.st.smp_armed) { app_push(a, CMD_SMP_REC, 0, 0, 0); return; }
                uint32_t room = bm_app_rec_room(a);
                if (!room) { app_toast(a, "TOMAS LLENAS: BORRA UNA"); return; }
                app_push_i(a, CMD_SMP_REC, a->rec_now ? 1 : 2, 0, 0, (int32_t)room);
                app_toast(a, a->rec_now ? "GRABANDO" : app_playing(a) ? "EMPIEZA EN EL COMPAS" : "ARMADA: PLAY PARA GRABAR");
            } else if (a->view == V_CH && a->page == PG_LOOP) app_push(a, CMD_LOOP, a->track, 1, 0);
            else if (a->view == V_SCENES) { a->song_rec = !a->song_rec; a->song_rec_loops = a->ctx.st.loops; app_toast(a, a->song_rec ? "SONG REC ON" : "SONG REC OFF"); }
            else {
                a->rec = !a->rec;
                app_push(a, CMD_REC, a->rec, 0, 0);
                if (a->rec) app_undo_snapshot(a, a->track);
                app_toast(a, a->rec ? "REC ON" : "REC OFF");
                if (a->rec) free_rec_start(a);
                else if (a->rec_free) free_rec_close(a);
            }
        } else if (k == G_HOLD) {                           /* grabar solo mientras lo sostienes */
            if (a->view == V_CH && a->page == PG_REC) {     /* en la GRABADORA: la mezcla, ya */
                uint32_t room = bm_app_rec_room(a);
                if (!room) { app_toast(a, "TOMAS LLENAS: BORRA UNA"); return; }
                app_push_i(a, CMD_SMP_REC, 1, 0, 0, (int32_t)room);
                a->rec_hold = 2;
            } else {                                        /* notas, golpes o slices fijos */
                a->rec_before = a->rec; a->rec = 1; a->rec_hold = 1;
                app_push(a, CMD_REC, 1, 0, 0);
                app_undo_snapshot(a, a->track);
            }
        } else if (k == G_RELEASE || k == G_LONG) {
            if (a->rec_hold == 2) app_push(a, CMD_SMP_REC, 0, 0, 0);
            else if (a->rec_hold == 1) { a->rec = a->rec_before; app_push(a, CMD_REC, a->rec, 0, 0); }
            a->rec_hold = 0;
            if (k == G_LONG) {
                if (a->view == V_CH && a->page == PG_LOOP) { app_push(a, CMD_LOOP, a->track, 2, 0); app_toast(a, "LOOP BORRADO"); return; }
                app_clear_track(a, a->track);   /* sin preguntar: HOME + OCT- lo deshace */
                snprintf(b, sizeof b, "T%d BORRADO", a->track + 1);
                app_toast(a, b);
            }
        }
        return;
    case BTN_GLO:
        if (k == G_TAP) {
            if (a->view == V_GLO) { a->view = a->prev_view == V_GLO || a->prev_view == V_BOOT ? V_HOME : a->prev_view; a->page = a->prev_page; }
            else app_set_view(a, V_GLO);
        } else if (k == G_HOLD) open_layer(a, id);
        else if (k == G_RELEASE) close_layer(a, id);
        return;
    case BTN_SAVE:
        if (k == G_TAP) {
            if (a->view == V_SAVE) {
                switch (a->save_row) {
                case 0:
                    app_save_jobs(a, 1);
                    snprintf(b, sizeof b, "GUARDADO A%02d", a->ctx.st.pattern + 1);
                    app_toast(a, app_playing(a) ? "PARA (STOP) PARA GUARDAR" : b);
                    break;
                case 1:
                    app_save_jobs(a, 0);
                    app_job(a, JOB_SAVE_AS, a->save_target);
                    snprintf(b, sizeof b, "GUARDADO COMO A%02d", a->save_target + 1);
                    app_toast(a, app_playing(a) ? "PARA (STOP) PARA GUARDAR" : b);
                    break;
                case 2: app_job(a, JOB_SAVE_SONG, 0); app_toast(a, "SONG Y ESCENAS GUARDADAS"); break;
                case 3:
                    if (a->clear_armed && a->now - a->clear_t < 3000) {
                        for (int t = 0; t < BM_TRACKS; t++) app_push(a, CMD_TRACK_CLEAR, (uint8_t)t, 0, 0);
                        app_edited(a); a->clear_armed = 0;
                        app_toast(a, "PATRON BORRADO");
                    } else { a->clear_armed = 1; a->clear_t = a->now; app_toast(a, "SAVE OTRA VEZ = BORRAR"); }
                    break;
                default:
                    snprintf(b, sizeof b, "%d EVENTOS", bm_pattern_count_events(&a->pat));
                    app_toast(a, b);
                }
            } else app_set_view(a, V_SAVE);
        } else if (k == G_HOLD) open_layer(a, id);
        else if (k == G_RELEASE) close_layer(a, id);
        else if (k == G_LONG) {
            close_layer(a, id);
            app_save_jobs(a, 1);
            app_toast(a, app_playing(a) ? "PARA (STOP) PARA GUARDAR" : "GUARDADO RAPIDO");
        }
        return;
    case BTN_HOME:
        if (k == G_TAP) {
            if (a->view == V_CH && a->page == PG_REC && a->ctx.st.smp_unsaved && !a->ctx.st.smp_rec) {
                app_push_ptr(a, CMD_SMP_DISCARD, 0, &a->smp_send[0]);
                app_toast(a, "TOMA DESCARTADA");
                return;
            }
            if (a->view == V_COPYCH) { app_set_view(a, V_CH); app_toast(a, "CANCELADO"); return; }
            int nv = V_HOME;
            if (a->view == V_HOME) nv = V_MIX;
            else if (a->view == V_MIX) nv = V_SCENES;
            else if (a->view == V_SCENES) nv = V_SONG;
            app_set_view(a, nv);
        } else if (k == G_HOLD) { a->home_used = 0; open_layer(a, id); }
        else if (k == G_RELEASE) {
            close_layer(a, id);
            if (a->box_on && a->box_hold == 1) { a->box_on = 0; app_enter(a, a->box_track, a->ch_page[a->box_track]); }
        } else if (k == G_LONG) { close_layer(a, id); app_set_view(a, V_SYSTEM); }
        return;
    default: break;
    }
    if (k == G_TAP) button_page(a, id);
    else if (k == G_HOLD) open_layer(a, id);
    else if (k == G_RELEASE) close_layer(a, id);
    else if (k == G_LONG) {
        close_layer(a, id);
        if (id == BTN_SEQ) { a->time_row = a->track; app_set_view(a, V_TIME); }
        else if (id == BTN_EDIT) {
            if (a->track == 1) app_enter(a, 1, PG_TAKES);
            else { snprintf(b, sizeof b, "SONIDO INICIAL EN T%d?", a->track + 1); app_ask(a, ASK_INIT, a->track, 0, b, "PIERDE LOS AJUSTES DEL SONIDO"); }
        }
    }
}

void bm_app_button(bm_app *a, int id, int down, uint32_t now)
{
    a->now = now;
    if (id < 0 || id >= BTN_COUNT) return;
    uint16_t bit = (uint16_t)(1u << id);
    if (down && id != BTN_HOME && bm_gesture_is_down(&a->ges, BTN_HOME)) { a->home_mod |= bit; a->home_done &= (uint16_t)~bit; a->home_used = 1; }
    if (down) bm_gesture_down(&a->ges, id, now);
    else bm_gesture_up(&a->ges, id, now);
    if (!down) { a->home_mod &= (uint16_t)~bit; a->home_done &= (uint16_t)~bit; }
}

/* ------------------------------------------------------------ perillas */
static void enc_macro(bm_app *a, int t, int i, int d)
{
    int v = bm_clampi(a->pat.trk[t].macro[i] + d, 0, 127);
    app_set_track(a, t, (uint8_t)i, v);
    char b[12], ti[22];
    bm_fmt_macro(a, t, i, v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", t + 1, bm_macro_name(a, t, i));
    app_overlay(a, i, ti, b, "", v * 2, -1, bm_knob_color(i));
}

static void enc_track_u8(bm_app *a, int t, int k, uint8_t off, int cur, int d, int lo, int hi, const char *name, int pct_mul)
{
    int v = bm_clampi(cur + d, lo, hi);
    app_set_track(a, t, off, v);
    char b[12], ti[22];
    fmt_pct(v, b, sizeof b);
    snprintf(ti, sizeof ti, "T%d %s", t + 1, name);
    app_overlay(a, k, ti, b, "", v * pct_mul, -1, k >= 0 ? bm_knob_color(k) : C_WHITE);
}

static void enc_step_grid(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step, lane = tr_lane(a);
    const bm_tr *tr = bm_trk_trc(&a->pat, t);
    uint8_t bb;
    if (t == 1) { int p = bm_brk_step(tr, s, &bb); lane = p < 0 ? 0 : p; } else bb = tr->st[lane][s];
    if (!(bb & ST_ON)) { app_toast(a, "PASO VACIO"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16], ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        if (t == 1) { app_toast(a, "SAMPLER: SELECT ALGO PRESETS"); return; }
        int i = id - ENC_K1;
        int cur = bm_tr_lock_get(tr, s, lane, i);
        if (cur < 0) cur = a->pat.trk[t].macro[i];
        int v = bm_clampi(cur + d, 0, 127);
        app_push_i(a, CMD_TR_LOCK, (uint8_t)t, (uint8_t)lane, (uint8_t)s, (i << 8) | v);
        app_edited(a);
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, bm_macro_name(a, t, i));
        app_overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
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
    app_push_i(a, CMD_SET_TRSTEP, (uint8_t)t, (uint8_t)lane, (uint8_t)s, bb);
    app_edited(a);
    app_overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
}

static void enc_step_syn(bm_app *a, int id, int d)
{
    int t = a->track, s = a->held_step;
    int idx[4];
    int n = syn_step_events(a, t, s, idx);
    if (!n) { app_toast(a, "PASO VACIO"); return; }
    a->held_long = 1; a->held_edit = 1;
    char b[16] = "", ti[22];
    if (id >= ENC_K1 && id <= ENC_K4) {
        int i = id - ENC_K1;
        const bm_event *e0 = bm_evc(&a->pat, t, s, idx[0]);
        int base = e0->lk[i] != BM_LOCK_NONE ? e0->lk[i] : a->pat.trk[t].macro[i];
        int v = bm_clampi(base + d, 0, 127);
        for (int j = 0; j < n; j++) { bm_event e = *bm_evc(&a->pat, t, s, idx[j]); e.lk[i] = (uint8_t)v; app_push_ev(a, t, s, idx[j], &e); }
        bm_fmt_macro(a, t, i, v, b, sizeof b);
        snprintf(ti, sizeof ti, "P-LOCK %02d %s", s + 1, bm_macro_name(a, t, i));
        app_overlay(a, i, ti, b, "", v * 2, a->pat.trk[t].macro[i] * 2, bm_knob_color(i));
        app_edited(a);
        return;
    }
    const char *nm = "";
    for (int j = 0; j < n; j++) {
        bm_event e = *bm_evc(&a->pat, t, s, idx[j]);
        if (id == ENC_SELECT) {                                        /* acento: normal > acento > fantasma */
            int lv = e.vel >= BM_VEL_ACC ? 2 : e.vel <= BM_VEL_GHOST ? 0 : 1;
            lv = bm_clampi(lv + (d > 0 ? 1 : -1), 0, 2);
            e.vel = lv == 2 ? BM_VEL_ACC : lv == 0 ? BM_VEL_GHOST : BM_VEL_NORMAL;
            if (j == 0) snprintf(b, sizeof b, "%s", lv == 2 ? "ACENTO" : lv == 0 ? "FANTASMA" : "NORMAL");
            nm = "GOLPE";
        } else if (id == ENC_ALGO) {
            int p = e.prob == BM_PROB_FILL ? 101 : e.prob == BM_PROB_NOFILL ? 102 : e.prob;
            if (p <= 100) p = bm_clampi(p + d * 5, 0, 102); else p = bm_clampi(p + (d > 0 ? 1 : -1), 95, 102);
            e.prob = p == 101 ? BM_PROB_FILL : p == 102 ? BM_PROB_NOFILL : (uint8_t)bm_clampi(p, 0, 100);
            if (j == 0) { if (p == 101) snprintf(b, sizeof b, "SOLO FILL"); else if (p == 102) snprintf(b, sizeof b, "SIN FILL"); else snprintf(b, sizeof b, "%d%%", e.prob); }
            nm = "PROB/FILL";
        } else {
            int cur = (e.len & BM_LEN_TIE) ? 17 : (e.len & BM_LEN_MASK);
            cur = bm_clampi(cur + d, 1, 17);
            e.len = cur == 17 ? (uint8_t)(1 | BM_LEN_TIE) : (uint8_t)cur;
            if (j == 0) { if (cur == 17) snprintf(b, sizeof b, "LIGA"); else snprintf(b, sizeof b, "%d PASOS", cur); }
            nm = "LARGO";
        }
        app_push_ev(a, t, s, idx[j], &e);
    }
    snprintf(ti, sizeof ti, "PASO %02d %s", s + 1, nm);
    app_overlay(a, -1, ti, b, "", 128, -1, C_WHITE);
    app_edited(a);
}

static bool enc_global(bm_app *a, int k, int d)
{
    bm_meta *m = &a->pat.meta;
    char b[16];
    switch (k) {
    case 0: {
        int bpm = bm_clampi(m->bpm_x10 + d * 10, 400, 2400);
        app_push_i(a, CMD_SET_BPM, 0, 0, 0, bpm); app_edited(a);
        snprintf(b, sizeof b, "%d.%d", bpm / 10, bpm % 10);
        app_overlay(a, 0, "TEMPO", b, "BPM", (bpm - 400) * 255 / 2000, -1, bm_knob_color(0));
        return true;
    }
    case 1: {
        int sw = bm_clampi(m->swing + (d > 0 ? 1 : -1), 50, 75);
        app_set_meta(a, META_OFF(swing), sw);
        snprintf(b, sizeof b, "%d", sw);
        app_overlay(a, 1, "SWING  50=RECTO", b, "%", (sw - 50) * 255 / 25, -1, bm_knob_color(1));
        return true;
    }
    case 2: {
        int i = 0;
        while (i < 6 && bm_master_steps_tab[i] != m->master) i++;
        i = bm_clampi((i >= 6 ? 3 : i) + (d > 0 ? 1 : -1), 0, 5);
        app_set_meta(a, META_OFF(master), bm_master_steps_tab[i]);
        fmt_master(bm_master_steps_tab[i], b, sizeof b);
        app_overlay(a, 2, "LARGO MAESTRO", b, bm_master_steps_tab[i] ? "PASOS" : "", i * 51, -1, bm_knob_color(2));
        return true;
    }
    case 3:
        a->set.drive = (uint8_t)bm_clampi(a->set.drive + d, 0, 127);
        app_push(a, CMD_DRIVE, a->set.drive, 0, 0);
        a->set_dirty = 1; a->t_edit = a->now;
        fmt_pct(a->set.drive, b, sizeof b);
        app_overlay(a, 3, "DRIVE (SOFT-CLIP)", b, "", a->set.drive * 2, -1, bm_knob_color(3));
        return true;
    default: return false;
    }
}

static void enc_fill_every(bm_app *a, int d)
{
    static const uint8_t FE[4] = {0, 2, 4, 8};
    bm_meta *m = &a->pat.meta;
    char b[16];
    int i = 0;
    while (i < 4 && FE[i] != m->fill_every) i++;
    i = bm_clampi((i >= 4 ? 0 : i) + (d > 0 ? 1 : -1), 0, 3);
    app_set_meta(a, META_OFF(fill_every), FE[i]);
    if (FE[i]) snprintf(b, sizeof b, "CADA %d", FE[i]); else snprintf(b, sizeof b, "OFF");
    app_overlay(a, -1, "FILL AUTOMATICO", b, FE[i] ? "COMP" : "", i * 85, -1, C_WHITE);
}

static void enc_transpose(bm_app *a, int d)
{
    int tp = bm_clampi(a->pat.meta.transpose + d, 0, 48);
    app_set_meta(a, META_OFF(transpose), tp);
    char b[12]; snprintf(b, sizeof b, "%+d", tp - 24);
    app_overlay(a, -1, "TONO DEL PROYECTO", b, "ST", tp * 5, 120, C_CYAN);
}

static void enc_enter(bm_app *a, int d)
{
    a->set.enter = (uint8_t)bm_clampi(a->set.enter + (d > 0 ? 1 : -1), 0, 4);
    a->set_dirty = 1; a->t_edit = a->now;
    int ms = bm_app_enter_ms(a);
    char b[12];
    if (ms < 0) snprintf(b, sizeof b, "OFF"); else snprintf(b, sizeof b, "%d", ms);
    app_overlay(a, -1, "ENTRADA AUTO", b, ms < 0 ? "" : "MS", a->set.enter * 63, -1, C_WHITE);
}

static void enc_pattern(bm_app *a, int d)
{
    int base = a->pat_sel >= 0 ? a->pat_sel : a->ctx.st.pattern;
    a->pat_sel = (int8_t)bm_clampi(base + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
    a->pat_sel_t = a->now;
    char b[8]; snprintf(b, sizeof b, "A%02d", a->pat_sel + 1);
    app_overlay(a, -1, app_playing(a) ? "PATRON (SE ENCOLA)" : "PATRON", b, "", a->pat_sel * 17, -1, C_WHITE);
}

/* ALGORITHM en HOME: un recuadro dice a qué canal vas; al rato (o al soltar HOME) entras. */
static void box_turn(bm_app *a, int d, int hold)
{
    if (!a->box_on) a->box_track = a->track;
    a->box_track = (uint8_t)bm_clampi(a->box_track + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1);
    int ms = bm_app_enter_ms(a);
    a->box_on = 1;
    if (hold) { a->box_hold = 1; return; }
    if (ms == 0) { a->box_on = 0; app_enter(a, a->box_track, a->ch_page[a->box_track]); return; }
    if (ms < 0) { a->box_hold = 2; a->track = a->box_track; a->box_until = a->now + 700; return; }
    a->box_hold = 0;
    a->box_until = a->now + (uint32_t)ms;
}

static void enc_source(bm_app *a, int t, int d)
{
    char b[16];
    int kind = app_kind(a, t);
    if (app_midi(a, t)) {
        int v = bm_clampi(a->pat.trk[t].macro[1] + (d > 0 ? 1 : -1), 0, 127);
        app_set_track(a, t, 1, v);
        app_push(a, CMD_MIDI_SEND, (uint8_t)(0xC0 | (a->pat.trk[t].midi_ch & 15)), (uint8_t)v, 0xFF);
        snprintf(b, sizeof b, "%d", v + 1);
        app_overlay(a, -1, "PROGRAMA MIDI", b, "", v * 2, -1, bm_track_color(t));
        return;
    }
    if (kind == KIND_BRK) {
        if (a->ctx.st.smp_unsaved) { app_toast(a, "PRIMERO SE GUARDA LA TOMA"); return; }
        uint8_t list[BM_TAKES + 8];
        int n = bm_app_sources(a, list), i = 0;
        if (!n) { app_toast(a, "NO HAY LOOPS"); return; }
        while (i < n && list[i] != a->pat.trk[1].source) i++;
        i = i >= n ? 0 : (i + (d > 0 ? 1 : n - 1)) % n;
        app_set_track(a, 1, TRK_OFF(source), list[i]);
        a->pat.trk[1].source = list[i];
        bm_app_src_name(a, list[i], b, sizeof b);
        app_overlay(a, -1, "LOOP A", b, "", i * 255 / (n > 1 ? n - 1 : 1), -1, bm_track_color(1));
        return;
    }
    int lim = bm_sound_count(a, t), e = a->pat.trk[t].engine;
    int v = (a->pat.trk[t].source % lim + (d > 0 ? 1 : lim - 1)) % lim;
    app_set_track(a, t, TRK_OFF(source), v);
    a->pat.trk[t].source = (uint8_t)v;
    app_overlay(a, -1, kind == KIND_TR ? "KIT" : e == ENG_ACID ? "ONDA ACID" : e == ENG_VA ? "TIMBRE ANALOG" : "BANCO DE ONDAS",
                bm_sound_name(a, t), "", v * 255 / (lim - 1), -1, bm_track_color(t));
}

static void cycle_src(bm_app *a, uint8_t off, uint8_t cur, int d, int allow_none, const char *title)
{
    uint8_t list[BM_TAKES + 9];
    int n = bm_app_sources(a, list), i = 0;
    if (allow_none) list[n++] = BM_SRC_NONE;
    if (!n) return;
    while (i < n && list[i] != cur) i++;
    i = i >= n ? 0 : (i + (d > 0 ? 1 : n - 1)) % n;
    app_set_track(a, 1, off, list[i]);
    ((uint8_t *)&a->pat.trk[1])[off] = list[i];
    char b[16]; bm_app_src_name(a, list[i], b, sizeof b);
    app_overlay(a, off == TRK_OFF(source) ? 0 : 1, title, b, "", i * 255 / (n > 1 ? n - 1 : 1), -1, bm_knob_color(off == TRK_OFF(source) ? 0 : 1));
}

static void more_change(bm_app *a, int d)
{
    uint8_t rows[10];
    int n = more_rows(a, a->track, rows), t = a->track;
    if (a->more_row >= n) a->more_row = (uint8_t)(n - 1);
    const bm_track *tk = &a->pat.trk[t];
    char b[30];
    switch (rows[a->more_row]) {
    case ROW_ENGINE: {
        static const uint8_t FREE[] = {ENG_SYNTH, ENG_MONO, ENG_VA, ENG_ACID, ENG_KIT, ENG_MIDI};
        static const uint8_t BASS[] = {ENG_BASS, ENG_ACID};
        const uint8_t *list = t == 2 ? BASS : FREE;
        int nl = t == 2 ? 2 : (int)sizeof FREE;
        int e = a->ask == ASK_ENGINE && a->ask_arg == t && bm_engine_ok(t, a->eng_pending) ? a->eng_pending : tk->engine, i = 0;
        while (i < nl && list[i] != e) i++;
        e = list[((i < nl ? i : 0) + (d > 0 ? 1 : nl - 1)) % nl];
        a->eng_pending = (uint8_t)e;
        if (e == tk->engine) { a->ask = ASK_NONE; app_toast(a, "SIN CAMBIO"); return; }
        if ((e == ENG_KIT) == (tk->engine == ENG_KIT)) {          /* notas a notas: las notas se quedan */
            a->ask = ASK_NONE;
            app_push(a, CMD_ENGINE, (uint8_t)t, (uint8_t)e, 0);
            app_edited(a);
            snprintf(b, sizeof b, "T%d AHORA ES %s", t + 1, bm_engine_names[e]);
            app_toast(a, b);
            return;
        }
        snprintf(b, sizeof b, "T%d PASA A %s?", t + 1, bm_engine_names[e]);
        app_ask(a, ASK_ENGINE, t, e, b, e == ENG_KIT ? "LAS NOTAS DEL CANAL SE BORRAN" : "LOS GOLPES DEL CANAL SE BORRAN");
        return;
    }
    case ROW_ACC: enc_track_u8(a, t, -1, TRK_OFF(accent), tk->accent, d, 0, 127, "ACENTO", 2); return;
    case ROW_LEN: set_len(a, t, bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? 0 : -1) + (d > 0 ? 1 : -1)); return;
    case ROW_SPEED: {
        int v = bm_clampi(tk->speed + (d > 0 ? -1 : 1), 0, SPD_COUNT - 1);
        app_set_track(a, t, TRK_OFF(speed), v);
        snprintf(b, sizeof b, "T%d VELOCIDAD", t + 1);
        app_overlay(a, -1, b, bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, C_WHITE);
        return;
    }
    case ROW_TRANSP: {
        int v = bm_clampi(tk->transpose + d, 0, 48);
        app_set_track(a, t, TRK_OFF(transpose), v);
        char vb[12]; fmt_semi(v, vb, sizeof vb);
        snprintf(b, sizeof b, "T%d TRANSPONER", t + 1);
        app_overlay(a, -1, b, vb, "", v * 5, 120, C_WHITE);
        return;
    }
    case ROW_MIDICH: {
        int v = bm_clampi(tk->midi_ch + (d > 0 ? 1 : -1), 0, 15);
        app_set_track(a, t, TRK_OFF(midi_ch), v);
        char vb[8]; snprintf(vb, sizeof vb, "%d", v + 1);
        app_overlay(a, -1, "CANAL MIDI", vb, "", v * 17, -1, C_WHITE);
        return;
    }
    case ROW_LOOPBPM:
        if (a->ctx.smp[0].bpm_x10) {
            app_push_i(a, CMD_SET_BPM, 0, 0, 0, a->ctx.smp[0].bpm_x10); app_edited(a);
            snprintf(b, sizeof b, "BPM = %d (EL DEL LOOP)", a->ctx.smp[0].bpm_x10 / 10);
            app_toast(a, b);
        }
        return;
    case ROW_INTERP: app_set_track(a, t, TRK_OFF(interp), !tk->interp); return;
    default:
        snprintf(b, sizeof b, "SONIDO INICIAL EN T%d?", t + 1);
        app_ask(a, ASK_INIT, t, 0, b, "PIERDE LOS AJUSTES DEL SONIDO");
        return;
    }
}

static void tabs_turn(bm_app *a, int d)
{
    uint8_t tabs[8];
    int n = bm_app_tabs(a, a->track, tabs), i = 0;
    while (i < n && tabs[i] != a->page) i++;
    if (i >= n) i = d > 0 ? 0 : n - 1;
    else i = (i + (d > 0 ? 1 : n - 1)) % n;
    if (tabs[i] == PG_MORE) a->more_row = 0;
    app_enter(a, a->track, tabs[i]);
}

static void system_change(bm_app *a, int d)
{
    bm_settings *s = &a->set;
    switch (a->menu_row) {
    case 0: s->clock = !s->clock; app_push(a, CMD_CLOCK, s->clock, 0, 0); break;
    case 1: s->sync_mode = !s->sync_mode; break;
    case 2: s->sync_ppqn = s->sync_ppqn == 1 ? (d > 0 ? 2 : 4) : s->sync_ppqn == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 1 : 2); break;
    case 3: s->sync_lat = (uint8_t)bm_clampi(s->sync_lat + d, 0, 20); break;
    case 4: s->autosave = !s->autosave; break;
    case 5: s->led_dim = (uint8_t)bm_clampi(s->led_dim + d, 0, 2); break;
    case 6: enc_enter(a, d); return;
    default: return;
    }
    app_push(a, CMD_SYNC, s->sync_mode, s->sync_ppqn, s->sync_lat);
    a->set_dirty = 1; a->t_edit = a->now;
}

static void glo_change(bm_app *a, int d)
{
    switch (a->glo_row) {
    case 0: case 1: case 2: case 3: enc_global(a, a->glo_row, d); break;
    case 4: enc_fill_every(a, d); break;
    case 5: enc_transpose(a, d); break;
    default: enc_enter(a, d); break;
    }
}

static void enc_channel(bm_app *a, int id, int d)
{
    int t = a->track, kind = app_kind(a, t), k = id - ENC_K1;
    const bm_track *tr = &a->pat.trk[t];
    bm_meta *m = &a->pat.meta;
    char b[16], ti[22];
    if (id == ENC_PRESETS) { enc_source(a, t, d); return; }
    if (id == ENC_SELECT) {
        if (a->page == PG_MORE) {
            if (d < 0 && a->more_row == 0) { tabs_turn(a, -1); return; }
            a->more_row = (uint8_t)bm_clampi(a->more_row + (d > 0 ? 1 : -1), 0, bm_app_more_count(a) - 1);
            a->ask = ASK_NONE; a->eng_pending = 0xFF;
            return;
        }
        if (a->page == PG_CUT) { const bm_take *tk = &a->take[a->cut_take % BM_TAKES]; int n = tk->nmarks ? tk->nmarks : BM_SLICES; a->cut_sel = (uint8_t)bm_clampi(a->cut_sel + (d > 0 ? 1 : -1), 0, n - 1); return; }
        tabs_turn(a, d);
        return;
    }
    if (id == ENC_ALGO) {
        if (a->page == PG_MORE) { more_change(a, d); return; }
        if (a->page == PG_TAKES) {
            uint8_t list[BM_TAKES + 8];
            int n = bm_app_sources(a, list);
            if (!n) return;
            a->takes_row = (uint8_t)bm_clampi(a->takes_row + (d > 0 ? 1 : -1), 0, n - 1);
            app_preview_take(a, list[a->takes_row]);
            return;
        }
        if (a->page == PG_CUT || a->page == PG_REC) { app_toast(a, "EL SAMPLER SE QUEDA AQUI"); return; }
        app_select_track(a, bm_clampi(t + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1), 1);
        return;
    }
    if (k < 0 || k > 3) return;
    switch (a->page) {
    case PG_SEQ:
        if (app_midi(a, t)) goto midi;
        if (t == 1) { enc_track_u8(a, 1, k, (uint8_t)(TRK_OFF(gen) + k), tr->gen[k], d, 0, 127, GEN_NAMES[k], 2); return; }
        enc_macro(a, t, k, d);
        return;
    case PG_SOUND:
        if (t == 1) {
            if (k == 0) { if (a->ctx.st.smp_unsaved) { app_toast(a, "PRIMERO SE GUARDA LA TOMA"); return; } cycle_src(a, TRK_OFF(source), tr->source, d, 0, "LOOP A"); }
            else if (k == 1) cycle_src(a, TRK_OFF(loop_b), tr->loop_b, d, 1, "LOOP B");
            else if (k == 2) { int v = bm_clampi(tr->chance_b + d, 0, 100); app_set_track(a, 1, TRK_OFF(chance_b), v); snprintf(b, sizeof b, "%d", v); app_overlay(a, 2, "COMPASES CON B", b, "%", v * 255 / 100, -1, bm_knob_color(2)); }
            else {
                int v = bm_clampi(tr->pitch + (d > 0 ? 1 : -1), 0, 48);
                app_set_track(a, 1, TRK_OFF(pitch), v);
                a->pat.trk[1].pitch = (uint8_t)v;
                snprintf(b, sizeof b, "%+.1f", (double)bm_app_brk_semis(a));
                app_overlay(a, 3, "TONO DEL LOOP", b, "ST", v * 5, 120, bm_knob_color(3));
            }
            return;
        }
        if (k == 3) { enc_track_u8(a, t, 3, TRK_OFF(accent), tr->accent, d, 0, 127, "ACENTO", 2); return; }
        if (kind == KIND_TR) { enc_macro(a, t, k, d); return; }
        enc_macro(a, t, k == 0 ? 3 : k - 1, d);
        return;
    case PG_INST: {
        int lane = tr_lane(a);
        const bm_lane *ln = &bm_trk_trc(&a->pat, t)->lane[lane];
        const uint8_t cur[4] = {ln->level, ln->tune, ln->decay, ln->len};
        static const uint8_t FIELD[4] = {1, 2, 3, 0};
        int nv = k == 3 ? bm_clampi(cur[k] + (d > 0 ? 1 : -1), 1, 64) : bm_clampi(cur[k] + d, 0, 127);
        app_push_i(a, CMD_SET_LANE, (uint8_t)t, (uint8_t)lane, FIELD[k], nv); app_edited(a);
        static const char *const NM[4] = {"NIVEL", "AFINACION", "DECAY", "LARGO"};
        if (k == 1) snprintf(b, sizeof b, "%+d ST", ((nv - 64) * 12) / 64); else if (k == 2) snprintf(b, sizeof b, "%d%%", nv * 100 / 127); else snprintf(b, sizeof b, "%d", nv);
        snprintf(ti, sizeof ti, "%s %s", bm_lane_name(a, t, lane), NM[k]);
        app_overlay(a, k, ti, b, k == 3 ? "PASOS" : "", k == 3 ? nv * 4 : nv * 2, -1, bm_knob_color(k));
        return;
    }
    case PG_MIX: goto mix;
    case PG_MORE:
        if (k == 0) enc_track_u8(a, t, 0, TRK_OFF(accent), tr->accent, d, 0, 127, "ACENTO", 2);
        else if (k == 1) set_len(a, t, bm_trk_len(&a->pat, t, bm_is_grid(&a->pat, t) ? (t == 1 ? 0 : tr_lane(a)) : -1) + (d > 0 ? 1 : -1));
        else if (k == 2) { int v = bm_clampi(tr->speed + (d > 0 ? -1 : 1), 0, SPD_COUNT - 1); app_set_track(a, t, TRK_OFF(speed), v); app_overlay(a, 2, "VELOCIDAD", bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, bm_knob_color(2)); }
        else if (kind == KIND_NOTE) { int v = bm_clampi(tr->transpose + d, 0, 48); app_set_track(a, t, TRK_OFF(transpose), v); fmt_semi(v, b, sizeof b); app_overlay(a, 3, "TRANSPONER", b, "", v * 5, 120, bm_knob_color(3)); }
        return;
    case PG_RETRIG: enc_track_u8(a, 1, k, (uint8_t)(TRK_OFF(rt) + k), tr->rt[k], d, 0, 127, RT_NAMES[k], 2); return;
    case PG_TAKES: {
        uint8_t src = bm_app_takes_sel(a);
        if (src == BM_SRC_NONE || (src & BM_SRC_ROM)) { app_toast(a, "LA FABRICA ES DE SOLO LECTURA"); return; }
        bm_take *tk = &a->take[src];
        if (k == 0) { tk->bpm_x10 = (uint16_t)bm_clampi(tk->bpm_x10 + d * 10, 400, 2400); snprintf(b, sizeof b, "%d", tk->bpm_x10 / 10); app_overlay(a, 0, "BPM DE LA TOMA", b, "BPM", (tk->bpm_x10 - 400) * 255 / 2000, -1, bm_knob_color(0)); }
        else if (k == 1) { tk->bars = (uint8_t)bm_clampi(tk->bars + (d > 0 ? 1 : -1), 1, 8); snprintf(b, sizeof b, "%d", tk->bars); app_overlay(a, 1, "COMPASES", b, "", tk->bars * 31, -1, bm_knob_color(1)); }
        else if (k == 2) { tk->gain = (uint8_t)bm_clampi(tk->gain + d, 8, 127); snprintf(b, sizeof b, "%+d", ((int)tk->gain - 64) * 6 / 64); app_overlay(a, 2, "GANANCIA", b, "DB", tk->gain * 2, 128, bm_knob_color(2)); }
        else return;
        take_changed(a, src);
        return;
    }
    case PG_ENV:
        if (a->env_page == 0) {
            static const uint8_t OFFS[4] = {TRK_OFF(attack), 2, TRK_OFF(sustain), TRK_OFF(release)};
            const uint8_t cur[4] = {tr->attack, tr->macro[2], tr->sustain, tr->release};
            int nv = bm_clampi(cur[k] + d, 0, 127);
            app_set_track(a, t, OFFS[k], nv);
            if (k == 0) fmt_ms(bm_attack_ms(nv), b, sizeof b); else if (k == 2) fmt_pct(nv, b, sizeof b); else fmt_ms(bm_decay_ms(nv), b, sizeof b);
            static const char *const NM[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
            snprintf(ti, sizeof ti, "T%d %s", t + 1, NM[k]);
            app_overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
        } else if (k < 2) {
            const uint8_t cur = k == 0 ? tr->env_amt : tr->glide;
            int nv = bm_clampi(cur + d, 0, 127);
            app_set_track(a, t, k == 0 ? TRK_OFF(env_amt) : TRK_OFF(glide), nv);
            if (k == 0) fmt_pct(nv, b, sizeof b); else fmt_ms((uint32_t)nv * 500 / 127, b, sizeof b);
            snprintf(ti, sizeof ti, "T%d %s", t + 1, k == 0 ? "ENV > FILTRO" : "GLIDE");
            app_overlay(a, k, ti, b, "", nv * 2, -1, bm_knob_color(k));
        }
        return;
    case PG_ARP:
        if (kind != KIND_NOTE) {
            if (k == 0) { a->roll_rate = (uint8_t)bm_clampi(a->roll_rate + (d > 0 ? 1 : -1), 0, 4); app_overlay(a, 0, "ROLL", bm_roll_names[a->roll_rate], "", a->roll_rate * 63, -1, bm_knob_color(0)); }
            return;
        }
        goto arp;
    case PG_LOOP:
        if (k == 0) { app_push(a, CMD_LOOP, (uint8_t)t, 3, 0); app_edited(a); }
        if (k == 1) {
            const bm_loop *lp = &a->pat.loop[t - 2];
            int nb = lp->bars == 1 ? (d > 0 ? 2 : 1) : lp->bars == 2 ? (d > 0 ? 4 : 1) : (d > 0 ? 4 : 2);
            app_push(a, CMD_LOOP, (uint8_t)t, 4, (uint8_t)nb); app_edited(a);
            snprintf(b, sizeof b, "%d", nb);
            app_overlay(a, 1, "LOOP COMPASES", b, "", nb * 63, -1, bm_knob_color(1));
        }
        return;
    case PG_MIDI: goto midi;
    case PG_SCL: {
        const char *val = "";
        if (k == 0) {
            if (tr->scl == 0xFF) { int nv = (m->root + (d > 0 ? 1 : 11)) % 12; app_set_meta(a, META_OFF(root), nv); val = bm_note_names[nv]; }
            else { int nv = (tr->scl_root + (d > 0 ? 1 : 11)) % 12; app_set_track(a, t, TRK_OFF(scl_root), nv); val = bm_note_names[nv]; }
        } else if (k == 1) {
            int cur = tr->scl == 0xFF ? -1 : tr->scl;
            int nv = bm_clampi(cur + (d > 0 ? 1 : -1), -1, BM_SCALES - 1);
            if (cur == -1 && nv == 0) app_set_track(a, t, TRK_OFF(scl_root), m->root);
            app_set_track(a, t, TRK_OFF(scl), nv < 0 ? 0xFF : nv);
            val = nv < 0 ? "PROYECTO" : bm_scale_names[nv];
        } else if (k == 2) { int nv = (m->voicing + (d > 0 ? 1 : 4)) % 5; app_set_meta(a, META_OFF(voicing), nv); val = EXT_NAMES[nv]; }
        else { int nv = !m->scale_lock; app_set_meta(a, META_OFF(scale_lock), nv); val = nv ? "ON" : "OFF"; }
        static const char *const NM[4] = {"TONICA", "ESCALA DEL CANAL", "ACORDE BASE", "TECLADO EN ESCALA"};
        app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        return;
    }
    case PG_CHORDS: goto chords;
    case PG_REC: if (k == 0) { a->rec_now = !a->rec_now; app_toast(a, a->rec_now ? "EMPIEZA AL TOCAR REC" : "EMPIEZA EN EL COMPAS"); } return;
    case PG_CUT: {
        bm_take *tk = &a->take[a->cut_take % BM_TAKES];
        if (!tk->valid) return;
        if (k == 0) { int n = tk->nmarks ? tk->nmarks : BM_SLICES; a->cut_sel = (uint8_t)bm_clampi(a->cut_sel + (d > 0 ? 1 : -1), 0, n - 1); }
        else if (k == 1) {
            uint32_t mk[BM_SLICES]; int n;
            cut_marks(tk, mk, &n);
            int i = a->cut_sel % n;
            if (i == 0) { app_toast(a, "EL CORTE 1 ES EL INICIO"); return; }
            uint32_t step = snap_len(a, tk);
            int32_t pos = (int32_t)mk[i];
            if (step) pos = (int32_t)((((uint32_t)pos + step / 2) / step) * step) + (d > 0 ? (int32_t)step : -(int32_t)step);
            else pos += d * 40;
            int32_t lo = (int32_t)mk[i - 1] + 64, hi = i + 1 < n ? (int32_t)mk[i + 1] - 64 : (int32_t)tk->len - 64;
            mk[i] = (uint32_t)bm_clampi(pos, lo, hi);
            uint32_t keep = mk[i];
            cuts_set(a, tk, mk, n);
            for (int j = 0; j < tk->nmarks; j++) if (tk->marks[j] == keep) a->cut_sel = (uint8_t)j;
        } else if (k == 2) a->cut_zoom = (uint8_t)bm_clampi(a->cut_zoom + (d > 0 ? 1 : -1), 0, 2);
        else a->cut_snap = (uint8_t)bm_clampi(a->cut_snap + (d > 0 ? 1 : -1), 0, 2);
        return;
    }
    case PG_LFO: app_toast(a, "LFO: EN UNA PROXIMA VERSION"); return;
    default: return;
    }
mix:
    {
        static const uint8_t OFFS[4] = {TRK_OFF(level), TRK_OFF(pan), TRK_OFF(send), TRK_OFF(duck)};
        const uint8_t cur[4] = {tr->level, tr->pan, tr->send, tr->duck};
        if (k == 3 && t == 0) { app_overlay(a, 3, "DUCK", "T1 ES LA FUENTE", "", 0, -1, bm_knob_color(3)); return; }
        int nv = bm_clampi(cur[k] + d, 0, 127);
        app_set_track(a, t, OFFS[k], nv);
        if (k == 1) fmt_pan(nv, b, sizeof b); else if (k == 0) snprintf(b, sizeof b, "%d", nv); else fmt_pct(nv, b, sizeof b);
        static const char *const NM[4] = {"NIVEL", "PAN", "ENVIO DELAY", "DUCK (SIDECHAIN)"};
        snprintf(ti, sizeof ti, "T%d %s", t + 1, NM[k]);
        app_overlay(a, k, ti, b, "", nv * 2, k == 1 ? 128 : -1, bm_knob_color(k));
        return;
    }
midi:
    {
        static const uint8_t CC[4] = {0, 0, 1, 74};
        if (k == 0) { int v = bm_clampi(tr->midi_ch + (d > 0 ? 1 : -1), 0, 15); app_set_track(a, t, TRK_OFF(midi_ch), v); snprintf(b, sizeof b, "%d", v + 1); app_overlay(a, 0, "CANAL MIDI", b, "", v * 17, -1, bm_knob_color(0)); return; }
        if (k == 1) { enc_source(a, t, d); return; }
        int v = bm_clampi(tr->macro[k] + d, 0, 127);
        app_set_track(a, t, (uint8_t)k, v);
        app_push(a, CMD_MIDI_SEND, (uint8_t)(0xB0 | (tr->midi_ch & 15)), CC[k], (uint8_t)v);
        snprintf(b, sizeof b, "%d", v); snprintf(ti, sizeof ti, "CC %d", CC[k]);
        app_overlay(a, k, ti, b, "", v * 2, -1, bm_knob_color(k));
        return;
    }
arp:
    {
        int nv;
        if (k == 0) { nv = (tr->arp_mode + (d > 0 ? 1 : 4)) % 5; app_set_track(a, t, TRK_OFF(arp_mode), nv); app_overlay(a, 0, "ARP MODO", ARP_MODES[nv], "", nv * 63, -1, bm_knob_color(0)); }
        if (k == 1) { nv = bm_clampi(tr->arp_rate + (d > 0 ? 1 : -1), 0, 2); app_set_track(a, t, TRK_OFF(arp_rate), nv); app_overlay(a, 1, "ARP RATE", ARP_RATES[nv], "", nv * 127, -1, bm_knob_color(1)); }
        if (k == 2) { nv = bm_clampi(tr->arp_oct + (d > 0 ? 1 : -1), 1, 3); app_set_track(a, t, TRK_OFF(arp_oct), nv); snprintf(b, sizeof b, "%d", nv); app_overlay(a, 2, "ARP OCTAVAS", b, "", nv * 85, -1, bm_knob_color(2)); }
        if (k == 3) { nv = bm_clampi(tr->arp_gate + d, 10, 100); app_set_track(a, t, TRK_OFF(arp_gate), nv); snprintf(b, sizeof b, "%d", nv); app_overlay(a, 3, "ARP GATE", b, "%", nv * 255 / 100, -1, bm_knob_color(3)); }
        return;
    }
chords:
    {
        const char *val = "";
        if (k == 0) { int nv = (m->chord_set + (d > 0 ? 1 : BM_CHORD_SETS - 1)) % BM_CHORD_SETS; app_set_meta(a, META_OFF(chord_set), nv); val = bm_chord_set_names[nv]; }
        if (k == 1) { int nv = (m->chord_voicing + (d > 0 ? 1 : 2)) % 3; app_set_meta(a, META_OFF(chord_voicing), nv); val = VOICINGS[nv]; a->prev_chord_n = 0; }
        if (k == 2) { int nv = bm_clampi(m->chord_oct + (d > 0 ? 1 : -1), 1, 5); app_set_meta(a, META_OFF(chord_oct), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
        if (k == 3) { int nv = bm_clampi(m->strum + (d > 0 ? 2 : -2), 0, 60); app_set_meta(a, META_OFF(strum), nv); snprintf(b, sizeof b, "%d", nv); val = b; }
        static const char *const NM[4] = {"JUEGO DE ACORDES", "VOICING", "OCTAVA", "STRUM (MS)"};
        app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
    }
}

void bm_app_encoder(bm_app *a, int id, int d, uint32_t now)
{
    a->now = now;
    if (!d || a->view == V_BOOT) return;
    bm_gesture_other(&a->ges, now);
    if (bm_gesture_is_down(&a->ges, BTN_HOME)) a->home_used = 1;
    if (d >= 3 || d <= -3) d *= 4;
    int k = id - ENC_K1;
    bm_meta *m = &a->pat.meta;
    char b[16];

    /* capas primero: las perillas siguen a la capa */
    switch (a->layer) {
    case L_HOME:
        if (id == ENC_ALGO) box_turn(a, d, 1);
        else if (id == ENC_PRESETS) enc_pattern(a, d);
        else if (k >= 0 && k <= 3) enc_global(a, k, d);
        return;
    case L_TONE:
        if (k == 0) enc_transpose(a, d);
        else if (k == 1) enc_transpose(a, d > 0 ? 12 : -12);
        else if (k == 2 || k == 3) enc_global(a, k - 2, d);
        return;
    case L_LEN:
        if (k == 0) set_len(a, a->track, bm_trk_len(&a->pat, a->track, app_kind(a, a->track) == KIND_NOTE ? -1 : 0) + (d > 0 ? 1 : -1));
        else if (k == 1) { int v = bm_clampi(a->pat.trk[a->track].speed + (d > 0 ? -1 : 1), 0, SPD_COUNT - 1); app_set_track(a, a->track, TRK_OFF(speed), v); app_overlay(a, 1, "VELOCIDAD", bm_speed_names[v], "", (SPD_COUNT - 1 - v) * 51, -1, bm_knob_color(1)); }
        return;
    case L_ACC: if (k == 0) enc_track_u8(a, a->track, 0, TRK_OFF(accent), a->pat.trk[a->track].accent, d, 0, 127, "ACENTO", 2); return;
    case L_REP: if (k == 0 && app_kind(a, a->track) == KIND_NOTE) enc_track_u8(a, a->track, 0, TRK_OFF(glide), a->pat.trk[a->track].glide, d, 0, 127, "GLIDE", 2); return;
    case L_ROLL:
        if (k == 0) {
            a->roll_rate = (uint8_t)bm_clampi(a->roll_rate + (d > 0 ? 1 : -1), 0, 4);
            app_overlay(a, 0, "ROLL", bm_roll_names[a->roll_rate], "", a->roll_rate * 63, -1, bm_knob_color(0));
            for (int l = 0; l < BM_LANES; l++) if (a->roll_mask & (1u << l)) app_push_i(a, CMD_ROLL, a->track, (uint8_t)l, 1, bm_roll_ticks[a->roll_rate]);
        }
        return;
    case L_PUNCH: case L_CHORDS: case L_ARP: case L_ERASE: case L_COPY: case L_LANEMUTE:
        if (a->layer == L_PUNCH && k >= 0) break;
        if ((a->layer == L_CHORDS || a->layer == L_ARP) && k >= 0) break;
        return;
    default: break;
    }
    if (a->layer == L_PUNCH) goto fxg;
    if (a->layer == L_CHORDS) { uint8_t pg = a->page; a->page = PG_CHORDS; enc_channel(a, id, d); a->page = pg; return; }
    if (a->layer == L_ARP) { uint8_t pg = a->page; a->page = PG_ARP; enc_channel(a, id, d); a->page = pg; return; }

    /* paso sostenido > lista > canal > HOME */
    if (a->view == V_CH && a->page == PG_SEQ && a->held_step >= 0 && id != ENC_PRESETS) {
        if (bm_is_grid(&a->pat, a->track)) enc_step_grid(a, id, d); else enc_step_syn(a, id, d);
        return;
    }
    switch (a->view) {
    case V_HOME: case V_SCENES:
        if (id == ENC_ALGO) box_turn(a, d, 0);
        else if (id == ENC_PRESETS) enc_pattern(a, d);
        else if (id == ENC_SELECT) app_set_view(a, d > 0 ? (a->view == V_HOME ? V_MIX : V_SONG) : (a->view == V_HOME ? V_SONG : V_MIX));
        else enc_global(a, k, d);
        return;
    case V_MIX:
        if (id == ENC_ALGO) { a->track = (uint8_t)bm_clampi(a->track + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1); return; }
        if (id == ENC_SELECT) { app_set_view(a, d > 0 ? V_SCENES : V_HOME); return; }
        if (id == ENC_PRESETS) { enc_pattern(a, d); return; }
        { uint8_t v = a->view, pg = a->page; a->view = V_CH; a->page = PG_MIX; enc_channel(a, id, d); a->view = v; a->page = pg; }
        return;
    case V_GLO:
        if (id == ENC_SELECT) a->glo_row = (uint8_t)bm_clampi(a->glo_row + (d > 0 ? 1 : -1), 0, 6);
        else if (id == ENC_ALGO) glo_change(a, d);
        else if (k >= 0) enc_global(a, k, d);
        return;
    case V_SYSTEM:
        if (id == ENC_SELECT) a->menu_row = (uint8_t)bm_clampi(a->menu_row + (d > 0 ? 1 : -1), 0, 9);
        else if (id == ENC_ALGO || id == ENC_K1) system_change(a, d > 0 ? 1 : -1);
        return;
    case V_SAVE:
        if (id == ENC_SELECT) a->save_row = (uint8_t)bm_clampi(a->save_row + (d > 0 ? 1 : -1), 0, 4);
        else if (id == ENC_ALGO || id == ENC_K1) a->save_target = (uint8_t)bm_clampi(a->save_target + (d > 0 ? 1 : -1), 0, BM_PATTERNS - 1);
        a->clear_armed = 0;
        return;
    case V_SONG:
        if (id == ENC_SELECT && a->song.n) a->song_cur = (uint8_t)bm_clampi(a->song_cur + (d > 0 ? 1 : -1), 0, a->song.n - 1);
        else if ((id == ENC_K1 || id == ENC_ALGO) && a->song.n) {
            uint8_t e = a->song.pat[a->song_cur];
            int code = (e & 0x80) ? 16 + (e & 3) : e;               /* 0..15 patrones, 16..19 escenas */
            code = bm_clampi(code + (d > 0 ? 1 : -1), 0, 19);
            a->song.pat[a->song_cur] = (uint8_t)(code >= 16 ? (0x80 | (code - 16)) : code);
            a->song_dirty = 1; a->t_edit = now;
        }
        else if (id == ENC_K2 && a->song.n) { a->song.rep[a->song_cur] = (uint8_t)bm_clampi(a->song.rep[a->song_cur] + (d > 0 ? 1 : -1), 1, 16); a->song_dirty = 1; a->t_edit = now; }
        else if (id == ENC_K4) { app_push(a, CMD_SONG, a->ctx.st.song_on ? 0 : 1, 0, 0); app_toast(a, a->ctx.st.song_on ? "SONG OFF" : "SONG ON"); }
        return;
    case V_TIME: {
        if (id == ENC_SELECT || id == ENC_ALGO) { a->time_row = (uint8_t)bm_clampi(a->time_row + (d > 0 ? 1 : -1), 0, BM_TRACKS - 1); return; }
        int row = a->time_row;
        const bm_track *rt = &a->pat.trk[row];
        char ti[22];
        if (k == 0) set_len(a, row, bm_trk_len(&a->pat, row, bm_is_grid(&a->pat, row) ? 0 : -1) + (d > 0 ? 1 : -1));
        else if (k == 1) {
            int nv = bm_clampi(rt->speed + (d > 0 ? -1 : 1), 0, SPD_COUNT - 1);   /* a la derecha = más rápido */
            app_set_track(a, row, TRK_OFF(speed), nv);
            snprintf(ti, sizeof ti, "T%d VELOCIDAD", row + 1);
            app_overlay(a, 1, ti, bm_speed_names[nv], "", (SPD_COUNT - 1 - nv) * 51, -1, bm_knob_color(1));
        } else if (k == 2 && bm_is_tr(&a->pat, row)) {
            a->time_lane = (uint8_t)bm_clampi(a->time_lane + (d > 0 ? 1 : -1), 0, 15);
        } else if (k == 3 && bm_is_tr(&a->pat, row)) {
            const bm_lane *ln = &bm_trk_trc(&a->pat, row)->lane[a->time_lane];
            int nv = bm_clampi(ln->len + (d > 0 ? 1 : -1), 1, 64);
            app_push_i(a, CMD_SET_LANE, (uint8_t)row, a->time_lane, 0, nv); app_edited(a);
            snprintf(b, sizeof b, "%d", nv); snprintf(ti, sizeof ti, "%s LARGO", bm_lane_name(a, row, a->time_lane));
            app_overlay(a, 3, ti, b, "PASOS", nv * 4, -1, bm_knob_color(3));
        }
        return;
    }
    case V_FXG: goto fxg;
    case V_SCLG:
        if (k < 0) return;
        {
            const char *val = "";
            if (k == 0) { int nv = (m->root + (d > 0 ? 1 : 11)) % 12; app_set_meta(a, META_OFF(root), nv); val = bm_note_names[nv]; }
            if (k == 1) { int nv = (m->scale + (d > 0 ? 1 : BM_SCALES - 1)) % BM_SCALES; app_set_meta(a, META_OFF(scale), nv); val = bm_scale_names[nv]; }
            if (k == 2) { int nv = (m->voicing + (d > 0 ? 1 : 4)) % 5; app_set_meta(a, META_OFF(voicing), nv); val = EXT_NAMES[nv]; }
            if (k == 3) { int nv = !m->scale_lock; app_set_meta(a, META_OFF(scale_lock), nv); val = nv ? "ON" : "OFF"; }
            static const char *const NM[4] = {"TONICA DEL PROYECTO", "ESCALA DEL PROYECTO", "ACORDE BASE", "TECLADO EN ESCALA"};
            app_overlay(a, k, NM[k], val, "", 128, -1, bm_knob_color(k));
        }
        return;
    case V_COPYCH:
        if (id == ENC_ALGO || id == ENC_SELECT) {
            int ks = app_kind(a, a->copy_src), dst = a->copy_dst;
            for (int i = 0; i < BM_TRACKS; i++) {
                dst = (dst + (d > 0 ? 1 : BM_TRACKS - 1)) % BM_TRACKS;
                if (dst == a->copy_src || dst == 1) continue;
                if (bm_is_free(dst) || app_kind(a, dst) == ks) break;
            }
            a->copy_dst = (uint8_t)dst;
        }
        return;
    case V_CH: enc_channel(a, id, d); return;
    default: return;
    }
fxg:
    if (k < 0 || k > 3) return;
    if (k == 0) {
        int nv = bm_clampi(m->dly_time + (d > 0 ? 1 : -1), 0, 3);
        app_set_meta(a, META_OFF(dly_time), nv);
        app_overlay(a, 0, "DELAY TIEMPO", DLY_NAMES[nv], "", nv * 85, -1, bm_knob_color(0));
    } else {
        static const uint8_t OFFS[4] = {0, META_OFF(dly_fb), META_OFF(dly_mix), META_OFF(sc_release)};
        const uint8_t cur[4] = {0, m->dly_fb, m->dly_mix, m->sc_release};
        int nv = bm_clampi(cur[k] + d, 0, 127);
        app_set_meta(a, OFFS[k], nv);
        if (k == 3) fmt_ms(bm_decay_ms(nv / 2), b, sizeof b); else fmt_pct(nv, b, sizeof b);
        static const char *const NM[4] = {"", "DELAY FEEDBACK", "DELAY MIX", "SIDECHAIN RELEASE"};
        app_overlay(a, k, NM[k], b, "", nv * 2, -1, bm_knob_color(k));
    }
}

/* ------------------------------------------------------------ tick de la entrada */
void app_input_tick(bm_app *a)
{
    uint32_t now = a->now;
    if (a->held_white >= 0 && !a->held_long && now - a->step_t0 >= UI_HOLD_MS) a->held_long = 1;
    if (a->rec_free && a->rec_free_t0 == 0xFFFFFFFFu && app_playing(a)) a->rec_free_t0 = a->ctx.st.btick;   /* empieza con PLAY */
    bool brk_held = a->view == V_CH && a->page == PG_SEQ && a->track == 1 && a->held_step >= 0 && a->held_long && !a->layer;
    if (brk_held && !a->hold_play) {                          /* sampler: el loop suena desde el paso sostenido */
        const bm_smp_src *m = &a->ctx.smp[0];
        int spl = 16 * (m->bars ? m->bars : 1);
        uint32_t pos = (uint32_t)((uint64_t)m->len * (uint32_t)(a->held_step % spl) / (uint32_t)spl);
        app_push_i(a, CMD_BRK_HOLD, 1, 0, 0, (int32_t)pos);
        a->hold_play = 1;
    } else if (!brk_held && a->hold_play) {
        app_push(a, CMD_BRK_HOLD, 0, 0, 0);
        a->hold_play = 0;
    }
    for (int k = 0; k < BM_KEYS; k++) {
        if (!a->keys_down[k] || !a->key_t0[k] || now - a->key_t0[k] < UI_HOLD_MS) continue;
        if (a->key_role[k] == ROLE_LMUTE && !a->layer && bm_is_tr(&a->pat, a->track)) a->layer = L_LANEMUTE;
        if (a->key_role[k] == ROLE_MUTE && !a->solo_on) {           /* negra sostenida en HOME: solo */
            int t = bm_key_black_index(k);
            a->solo_saved = 0;
            for (int i = 0; i < BM_TRACKS; i++) if (a->pat.trk[i].mute) a->solo_saved |= (uint8_t)(1u << i);
            for (int i = 0; i < BM_TRACKS; i++) app_set_track(a, i, TRK_OFF(mute), i != t);
            a->solo_on = 1; a->solo_key = (int8_t)k; a->solo_track = (int8_t)t;
            char b[16]; snprintf(b, sizeof b, "SOLO T%d", t + 1);
            app_toast(a, b);
        }
    }
    if (a->box_on && a->box_hold != 1 && (int32_t)(now - a->box_until) >= 0) {
        a->box_on = 0;
        if (a->box_hold == 0) app_enter(a, a->box_track, a->ch_page[a->box_track]);
    }
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

static int page_button(int page)
{
    switch (page) {
    case PG_SEQ: case PG_LOOP: case PG_RETRIG: return BTN_SEQ;
    case PG_SOUND: case PG_INST: case PG_TAKES: case PG_CUT: case PG_MIDI: return BTN_EDIT;
    case PG_ENV: return BTN_ENV;
    case PG_MIX: return BTN_FX;
    case PG_SCL: case PG_CHORDS: return BTN_SCL;
    case PG_ARP: return BTN_ARP;
    case PG_LFO: return BTN_LFO;
    case PG_REC: return BTN_REC;
    default: return -1;
    }
}

/* un carril de batería: el elegido al 100 %, con pasos al 50 %, con pasos pero muteado tenue,
 * sin pasos apagado */
static uint8_t lane_led(const bm_tr *tr, int ln, int sel)
{
    if (ln == sel)
        return LED_ON;
    if (ln < 0 || ln >= BM_LANES)
        return LED_OFF;
    for (int s = 0; s < tr->lane[ln].len && s < BM_STEPS; s++)
        if (tr->st[ln][s] & ST_ON)
            return ((tr->mute >> ln) & 1) ? LED_DIM : LED_MID;
    return LED_OFF;
}

void app_leds(bm_app *a)
{
    const bm_status *st = &a->ctx.st;
    if (a->view == V_BOOT) {
        int lit = (int)((a->now - a->boot_t) / 40u) % (BM_KEYS + 6);
        for (int k = 0; k < BM_KEYS; k++) led(a, BM_LED_KEY0 + k, k == lit ? LED_ON : LED_OFF);
        for (int i = 0; i < BTN_COUNT; i++) led(a, i, LED_DIM);
        return;
    }
    static const int8_t LB[L_COUNT] = {-1, BTN_HOME, BTN_SEQ, BTN_EDIT, BTN_ENV, BTN_LFO, BTN_FX, BTN_SCL, BTN_ARP, BTN_ARP, BTN_SAVE, BTN_GLO, BTN_SEQ};
    int vb = -1;
    switch (a->view) {
    case V_HOME: case V_MIX: case V_SCENES: case V_SYSTEM: vb = BTN_HOME; break;
    case V_SONG: case V_TIME: vb = BTN_SEQ; break;
    case V_GLO: vb = BTN_GLO; break;
    case V_SAVE: vb = BTN_SAVE; break;
    case V_FXG: vb = BTN_FX; break;
    case V_SCLG: vb = BTN_SCL; break;
    case V_COPYCH: vb = BTN_EDIT; break;
    case V_CH: vb = page_button(a->page); break;
    default: break;
    }
    for (int i = 0; i < BTN_COUNT; i++) {
        uint8_t l = LED_DIM;
        if (i == vb) l = LED_ON;
        if (a->layer && i == LB[a->layer]) l = LED_BREATH;
        if (i == BTN_PLAY) l = st->playing ? ((st->btick % BM_PPQN) < 24 ? LED_ON : LED_DIM) : LED_DIM;
        if (i == BTN_REC) l = (st->smp_rec || a->song_rec) ? LED_BLINK : st->smp_armed ? LED_BREATH : (a->rec ? LED_ON : LED_DIM);
        if ((i == BTN_OCT_DN && a->oct < 0) || (i == BTN_OCT_UP && a->oct > 0)) l = LED_ON;
        if (a->ask && i == BTN_OCT_UP) l = LED_BLINK;
        led(a, i, l);
    }
    int t = a->track, kind = app_kind(a, t);
    bm_keyfn K[BM_KEYS];
    char title[24]; uint16_t col; const char *h1, *h2;
    bool mapped = a->layer || a->view == V_HOME || a->view == V_MIX || (a->view == V_CH && a->page == PG_CHORDS);
    if (mapped) bm_app_keymap(a, K, title, sizeof title, &col, &h1, &h2);
    for (int k = 0; k < BM_KEYS; k++) {
        int w = bm_key_white_index(k), b = bm_key_black_index(k);
        uint8_t l = LED_OFF;
        if (a->keys_down[k]) l = LED_ON;
        else if (mapped) l = K[k].led;
        else if (a->view == V_CH && a->page == PG_SEQ) {
            if (kind == KIND_NOTE) {
                if (w >= 0) {
                    int s = a->step_page * 16 + w, idx[4];
                    if (s < a->pat.trk[t].length) {
                        l = syn_step_events(a, t, s, idx) ? LED_ON : LED_OFF;
                        if (s == bm_app_track_step(a, t, -1)) l = LED_BLINK;
                    }
                } else l = b < 8 ? LED_DIM : LED_OFF;
            } else if (t == 1) {
                const bm_tr *tr = &a->pat.tr[1];
                if (w >= 0) {
                    int s = a->step_page * 16 + w;
                    if (s < tr->lane[0].len) {
                        int p = bm_brk_step(tr, s, NULL);
                        l = p > 0 ? LED_ON : p == 0 ? LED_BREATH : LED_OFF;
                        if (s == bm_app_track_step(a, 1, 0)) l = LED_BLINK;
                    }
                } else if (b < BM_SLICES) l = st->brk_slice == b && st->playing ? LED_ON : LED_MID;
                else l = (a->brk_live >> (b - 8)) & 1 ? LED_ON : LED_DIM;
            } else {
                const bm_tr *tr = bm_trk_trc(&a->pat, t);
                int lane = tr_lane(a);
                if (w >= 0) {
                    int s = a->step_page * 16 + w, len = tr->lane[lane].len;
                    if (s < len) {
                        uint8_t bb = tr->st[lane][s];
                        l = (bb & ST_ON) ? ((bb & ST_LOCK) ? LED_BREATH : LED_ON) : LED_OFF;
                        if (s == bm_app_track_step(a, t, lane)) l = LED_BLINK;
                    }
                } else if (b < 8) {
                    l = lane_led(tr, (lane / 8) * 8 + b, lane);
                } else l = (b == 8 && lane >= 8) ? LED_ON : LED_DIM;
            }
        } else if (a->view == V_SCENES) {
            if (w >= 0 && w < 4) l = w == st->scene ? LED_ON : (w == st->scene_q ? LED_BLINK : (a->song.scene[w].valid ? LED_MID : LED_OFF));
            else if (w >= 4 && w < 8) l = LED_DIM;
            else if (b >= 0 && b < 6) l = a->pat.trk[b].mute ? LED_OFF : LED_ON;
        } else if (a->view == V_CH && a->page == PG_CUT) {
            const bm_take *tk = &a->take[a->cut_take % BM_TAKES];
            if (w >= 0 && tk->valid) {
                uint32_t bpm = tk->bpm_x10 ? tk->bpm_x10 : a->pat.meta.bpm_x10, e8 = (uint32_t)BM_SMP_RATE * 300u / bpm;
                uint32_t pos = (uint32_t)w * e8;
                l = pos < tk->len ? LED_MID : LED_OFF;
                for (int i = 0; i < tk->nmarks; i++) if ((tk->marks[i] > pos ? tk->marks[i] - pos : pos - tk->marks[i]) < e8 / 2) l = LED_ON;
            } else if (b >= 0 && b < 10) l = LED_DIM;
        } else if (kind == KIND_TR && w >= 0) {
            l = lane_led(bm_trk_trc(&a->pat, t), w, tr_lane(a));
        } else if (kind == KIND_NOTE && (a->pat.meta.scale_lock || (a->view == V_CH && a->page == PG_SCL))) {
            int root, sc = app_scale(a, t, &root);
            (void)root;
            if (w >= 0) l = (w % bm_scale_len(sc)) == 0 ? LED_ON : LED_DIM;
            else if (b < 7) l = LED_DIM;
        }
        led(a, BM_LED_KEY0 + k, l);
    }
}
