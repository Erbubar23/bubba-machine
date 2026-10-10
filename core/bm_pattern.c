/* Bubba Machine — patrón, formato disperso, escalas y acordes. */
#include <string.h>
#include <stdio.h>
#include "bm_pattern.h"

typedef char bm_assert_event[(sizeof(bm_event) == 8) ? 1 : -1];
typedef char bm_assert_track[(sizeof(bm_track) == 32) ? 1 : -1];
typedef char bm_assert_meta[(sizeof(bm_meta) == 64) ? 1 : -1];

void bm_event_clear(bm_event *e)
{
    e->note = BM_EV_EMPTY; e->vel = 100; e->len = 1; e->prob = 100;
    e->lk[0] = e->lk[1] = e->lk[2] = e->lk[3] = BM_LOCK_NONE;
}

bool bm_event_empty(const bm_event *e) { return e->note == BM_EV_EMPTY; }

void bm_track_defaults(bm_track *t, int track)
{
    memset(t, 0, sizeof *t);
    t->macro[0] = 96; t->macro[1] = 30; t->macro[2] = 60; t->macro[3] = 0;
    t->level = 100; t->pan = 64; t->send = 0; t->duck = (track >= 2) ? 60 : 0;
    t->source = 0;
    t->attack = 0; t->sustain = 80; t->release = 40; t->env_amt = 40; t->glide = 30;
    t->interp = (track == 1) ? 1 : 0;
    t->transpose = 24;
    t->arp_mode = 0; t->arp_rate = 1; t->arp_oct = 1; t->arp_gate = 60;
    if (track == 0) { t->macro[0] = 127; t->macro[1] = 64; t->macro[2] = 90; }
    if (track == 1) { t->macro[0] = 127; t->macro[1] = 64; t->macro[2] = 100; }
    if (track == 2) { t->source = 0; t->macro[0] = 60; t->macro[1] = 70; t->macro[2] = 50; t->sustain = 60; }
    if (track == 3) { t->source = 11; t->macro[0] = 80; t->macro[2] = 70; t->send = 30; }
    if (track == 4) { t->source = 7; t->macro[0] = 90; t->attack = 20; t->release = 60; t->send = 20; }
    if (track == 5) { t->source = 9; t->macro[0] = 75; t->attack = 40; t->release = 70; t->send = 40; }
}

void bm_track_clear_events(bm_pattern *p, int track)
{
    for (int s = 0; s < BM_STEPS; s++)
        for (int e = 0; e < BM_EVENTS; e++)
            bm_event_clear(&p->ev[track][s][e]);
}

void bm_pattern_init(bm_pattern *p)
{
    memset(&p->meta, 0, sizeof p->meta);
    p->meta.bpm_x10 = 1200; p->meta.swing = 50; p->meta.length = 64;
    p->meta.dly_time = 2; p->meta.dly_fb = 50; p->meta.dly_mix = 60;
    p->meta.sc_release = 50; p->meta.root = 9; p->meta.scale = 1; p->meta.voicing = 0;
    for (int t = 0; t < BM_TRACKS; t++) {
        bm_track_defaults(&p->trk[t], t);
        bm_track_clear_events(p, t);
    }
}

int bm_pattern_count_events(const bm_pattern *p)
{
    int n = 0;
    for (int t = 0; t < BM_TRACKS; t++)
        for (int s = 0; s < BM_STEPS; s++)
            for (int e = 0; e < BM_EVENTS; e++)
                if (!bm_event_empty(&p->ev[t][s][e])) n++;
    return n;
}

/* Cuerpo: meta (64, bpm en little-endian) | pistas (6 x 32) |
 * por pista: n u16 + n x (byte de posición + evento 8). Posición: bits 0-5 paso, bits 6-7 slot. */
int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap)
{
    int o = 0;
    if (cap < 64 + 192 + 12) return -1;
    memcpy(out, &p->meta, 64);
    out[0] = (uint8_t)(p->meta.bpm_x10 & 0xFF);
    out[1] = (uint8_t)(p->meta.bpm_x10 >> 8);
    o = 64;
    memcpy(out + o, p->trk, sizeof p->trk); o += (int)sizeof p->trk;
    int total = 0;
    for (int t = 0; t < BM_TRACKS; t++) {
        int at = o; o += 2;
        int n = 0;
        for (int s = 0; s < BM_STEPS; s++)
            for (int e = 0; e < BM_EVENTS; e++) {
                const bm_event *ev = &p->ev[t][s][e];
                if (bm_event_empty(ev)) continue;
                if (o + 9 > cap || total >= BM_PAT_MAX_EVENTS) return -1;
                out[o++] = (uint8_t)(s | (e << 6));
                memcpy(out + o, ev, 8); o += 8;
                n++; total++;
            }
        out[at] = (uint8_t)(n & 0xFF); out[at + 1] = (uint8_t)(n >> 8);
    }
    return o;
}

int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len)
{
    if (len < 64 + 192 + 12) return -1;
    memcpy(&p->meta, in, 64);
    p->meta.bpm_x10 = (uint16_t)(in[0] | (in[1] << 8));
    int o = 64;
    memcpy(p->trk, in + o, sizeof p->trk); o += (int)sizeof p->trk;
    for (int t = 0; t < BM_TRACKS; t++) {
        bm_track_clear_events(p, t);
        if (o + 2 > len) return -1;
        int n = in[o] | (in[o + 1] << 8); o += 2;
        for (int i = 0; i < n; i++) {
            if (o + 9 > len) return -1;
            int s = in[o] & 0x3F, slot = in[o] >> 6;
            o++;
            memcpy(&p->ev[t][s][slot], in + o, 8);
            o += 8;
        }
    }
    /* saneo de rangos por si el dato viene de una versión futura */
    if (p->meta.bpm_x10 < 400 || p->meta.bpm_x10 > 2400) p->meta.bpm_x10 = 1200;
    if (p->meta.swing < 50 || p->meta.swing > 75) p->meta.swing = 50;
    if (p->meta.length != 16 && p->meta.length != 32 && p->meta.length != 48) p->meta.length = 64;
    if (p->meta.scale >= BM_SCALES) p->meta.scale = 0;
    p->meta.root %= 12;
    return o;
}

/* ------------------------------------------------------------------ escalas */
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

/* ext: 0 triada, 1 +7, 2 +9, 3 SUS2, 4 SUS4 */
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

void bm_chord_name(int root, int scale, int degree, int ext, char *buf, int cap)
{
    uint8_t n[4];
    bm_chord_notes(root, scale, degree, 0, 3, n);
    int third = (n[1] - n[0] + 24) % 12, fifth = (n[2] - n[0] + 24) % 12;
    const char *q = "";
    if (third == 3 && fifth == 6) q = "~";          /* disminuido (° en la fuente) */
    else if (third == 3) q = "m";
    else if (third == 4 && fifth == 8) q = "+";
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
