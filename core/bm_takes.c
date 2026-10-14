/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — biblioteca de tomas en flash. */
#include <string.h>
#include "bm_takes.h"
#include <math.h>
#include "bm_util.h"
#include "bm_snap.h"

#define TAKE_MAGIC 0x34544D42u   /* "BMT4": 16 cortes (las de "BMT3" ya no se leen) */

static void put32(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

static uint32_t sec_addr(int s) { return BM_TAKE_LO + (uint32_t)s * BM_SECTOR; }
static int sectors_for(uint32_t len) { return (int)((BM_TAKE_HDR + len + BM_SECTOR - 1) / BM_SECTOR); }

/* cabecera: magic 4 | id 1 | nmarks 1 | gain 1 | bars 1 | bpm 2 | rsv 2 | len 4 | marks 16x4 | crc 4 | relleno */
#define HDR_CRC (16 + 4 * BM_SLICES)
static void make_hdr(uint8_t h[BM_TAKE_HDR], const bm_take *t, uint32_t crc)
{
    memset(h, 0xFF, BM_TAKE_HDR);
    put32(h, TAKE_MAGIC);
    h[4] = t->id; h[5] = t->nmarks; h[6] = t->gain; h[7] = t->bars;
    h[8] = (uint8_t)t->bpm_x10; h[9] = (uint8_t)(t->bpm_x10 >> 8); h[10] = 0; h[11] = 0;
    put32(h + 12, t->len);
    for (int i = 0; i < BM_SLICES; i++) put32(h + 16 + 4 * i, i < t->nmarks ? t->marks[i] : 0);
    put32(h + HDR_CRC, crc);
}

static int program_span(const bm_flash_ops *f, uint32_t a, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t room = BM_PAGE - (a & (BM_PAGE - 1));
        uint32_t k = n < room ? n : room;
        if (f->program(a, src, k)) return -1;
        a += k; src += k; n -= k;
    }
    return 0;
}

static int read_take(const bm_flash_ops *f, bm_map_fn map, void *mu, int sec, bm_take *t)
{
    uint8_t h[HDR_CRC + 4];
    memset(t, 0, sizeof *t);
    if (f->read(sec_addr(sec), h, sizeof h)) return -1;
    uint32_t len = get32(h + 12);
    if (get32(h) != TAKE_MAGIC || h[4] >= BM_TAKES || !len || len > BM_SMP_REC_MAX || h[5] > BM_SLICES) return -1;
    int ns = sectors_for(len);
    if (sec + ns > (int)BM_TAKE_SECTORS) return -1;
    const uint8_t *d = map ? map(mu, sec_addr(sec) + BM_TAKE_HDR) : NULL;
    if (!d || bm_crc32(0, d, len) != get32(h + HDR_CRC)) return -1;
    t->valid = 1; t->id = h[4]; t->nmarks = h[5]; t->gain = h[6] ? h[6] : 64; t->bars = h[7];
    t->bpm_x10 = (uint16_t)(h[8] | h[9] << 8);
    if (t->bpm_x10 < 400 || t->bpm_x10 > 2400) t->bpm_x10 = 0;
    t->len = len; t->sector = (uint8_t)sec; t->nsect = (uint8_t)ns; t->data = (const int8_t *)d;
    for (int i = 0; i < t->nmarks; i++) {
        t->marks[i] = get32(h + 16 + 4 * i);
        if (t->marks[i] >= len || (i && t->marks[i] <= t->marks[i - 1])) { t->nmarks = (uint8_t)i; break; }
    }
    return 0;
}

int bm_takes_scan(const bm_flash_ops *f, bm_map_fn map, void *mu, bm_take tk[BM_TAKES])
{
    memset(tk, 0, sizeof(bm_take) * BM_TAKES);
    int n = 0;
    for (int s = 0; s < (int)BM_TAKE_SECTORS;) {
        bm_take t;
        if (read_take(f, map, mu, s, &t) == 0 && !tk[t.id].valid) { tk[t.id] = t; n++; s += t.nsect; }
        else s++;
    }
    return n;
}

static void used_map(const bm_take tk[BM_TAKES], uint8_t used[BM_TAKE_SECTORS])
{
    memset(used, 0, BM_TAKE_SECTORS);
    bm_snaps_mark(used, (int)BM_TAKE_SECTORS);                /* la zona es compartida con los sonidos */
    for (int i = 0; i < BM_TAKES; i++)
        if (tk[i].valid) for (int k = 0; k < tk[i].nsect; k++) used[tk[i].sector + k] = 1;
}

static int largest_run(const uint8_t used[BM_TAKE_SECTORS], int *at)
{
    int best = 0, run = 0;
    for (int s = 0; s <= (int)BM_TAKE_SECTORS; s++) {
        if (s < (int)BM_TAKE_SECTORS && !used[s]) { run++; continue; }
        if (run > best) { best = run; if (at) *at = s - run; }
        run = 0;
    }
    return best;
}

void bm_takes_used(const bm_take tk[BM_TAKES], uint8_t *used)
{
    memset(used, 0, BM_TAKE_SECTORS);
    for (int i = 0; i < BM_TAKES; i++)
        if (tk[i].valid) for (int k = 0; k < tk[i].nsect; k++) used[tk[i].sector + k] = 1;
}

uint32_t bm_takes_room(const bm_take tk[BM_TAKES])
{
    int ids = 0;
    for (int i = 0; i < BM_TAKES; i++) ids += tk[i].valid;
    if (ids >= BM_TAKES) return 0;
    uint8_t used[BM_TAKE_SECTORS];
    used_map(tk, used);
    int run = largest_run(used, NULL);
    uint32_t bytes = run ? (uint32_t)run * BM_SECTOR - BM_TAKE_HDR : 0;
    return bytes > BM_SMP_REC_MAX ? BM_SMP_REC_MAX : bytes;
}

uint32_t bm_takes_free(const bm_take tk[BM_TAKES])
{
    uint8_t used[BM_TAKE_SECTORS];
    used_map(tk, used);
    uint32_t n = 0;
    for (int s = 0; s < (int)BM_TAKE_SECTORS; s++) n += !used[s];
    return n ? n * BM_SECTOR - BM_TAKE_HDR : 0;
}

int bm_takes_write(const bm_flash_ops *f, bm_map_fn map, void *mu, bm_take tk[BM_TAKES],
                   const int8_t *data, uint32_t len, uint16_t bpm_x10, uint8_t bars)
{
    if (!len || len > BM_SMP_REC_MAX) return -1;
    int id = -1;
    for (int i = 0; i < BM_TAKES && id < 0; i++) if (!tk[i].valid) id = i;
    if (id < 0) return -1;
    uint8_t used[BM_TAKE_SECTORS];
    used_map(tk, used);
    int need = sectors_for(len), at = -1;
    for (int s = 0; s + need <= (int)BM_TAKE_SECTORS && at < 0; s++) {     /* primer hueco que cabe */
        int ok = 1;
        for (int k = 0; k < need; k++) if (used[s + k]) { ok = 0; break; }
        if (ok) at = s;
    }
    if (at < 0) return -1;
    for (int k = 0; k < need; k++) if (f->erase(sec_addr(at + k))) return -1;
    if (program_span(f, sec_addr(at) + BM_TAKE_HDR, (const uint8_t *)data, len)) return -1;
    bm_take t;
    memset(&t, 0, sizeof t);
    t.id = (uint8_t)id; t.len = len; t.gain = 64; t.bpm_x10 = bpm_x10; t.bars = bars;
    uint8_t h[BM_TAKE_HDR];
    make_hdr(h, &t, bm_crc32(0, data, len));
    if (program_span(f, sec_addr(at), h, BM_TAKE_HDR)) return -1;          /* commit al final */
    if (read_take(f, map, mu, at, &tk[id])) return -1;
    return id;
}

int bm_takes_update(const bm_flash_ops *f, bm_take *t, uint8_t *scratch)
{
    if (!t->valid) return -1;
    uint32_t a = sec_addr(t->sector);
    if (f->read(a, scratch, BM_SECTOR)) return -1;
    if (get32(scratch) != TAKE_MAGIC) return -1;
    uint32_t crc = get32(scratch + HDR_CRC);
    uint8_t h[BM_TAKE_HDR];
    make_hdr(h, t, crc);
    if (f->erase(a)) return -1;
    if (program_span(f, a + BM_TAKE_HDR, scratch + BM_TAKE_HDR, BM_SECTOR - BM_TAKE_HDR)) return -1;
    return program_span(f, a, h, BM_TAKE_HDR);
}

int bm_takes_erase(const bm_flash_ops *f, bm_take *t)
{
    if (!t->valid) return -1;
    for (int k = 0; k < t->nsect; k++) if (f->erase(sec_addr(t->sector + k))) return -1;
    memset(t, 0, sizeof *t);
    return 0;
}

int bm_takes_format(const bm_flash_ops *f)
{
    for (int s = 0; s < (int)BM_TAKE_SECTORS; s++) if (f->erase(sec_addr(s))) return -1;
    bm_snaps_format();
    return 0;
}

/* Cortes en los ataques, pensado también para voces: energía y energía de la derivada (las
 * consonantes y los golpes) en ventanas de 16 ms cada 4 ms; un corte donde la energía salta
 * respecto de los 16 ms anteriores y supera un umbral que baja con la sensibilidad. Queda un poco
 * antes del ataque y en un cruce por cero, para que el corte no haga click. */
int bm_slice_onsets(const int8_t *d, uint32_t len, uint32_t min_gap, int sens, uint32_t out[BM_SLICES])
{
    enum { HOP = 64, WIN = 256, MAXF = 1024 };
    static float on[MAXF];
    int nf = (int)((len > WIN ? len - WIN : 0) / HOP);
    if (nf > MAXF) nf = MAXF;
    float prev[4] = {0, 0, 0, 0};
    double sum = 0, sum2 = 0;
    for (int f = 0; f < nf; f++) {
        const int8_t *x = d + (uint32_t)f * HOP;
        float e = 0, h = 0;
        for (int i = 0; i < WIN; i++) {
            float v = x[i], dv = i ? (float)(x[i] - x[i - 1]) : 0.0f;
            e += v * v; h += dv * dv;
        }
        float lv = logf(1.0f + e + 0.5f * h);
        float ref = (prev[0] + prev[1] + prev[2] + prev[3]) * 0.25f;
        float o = lv - ref;
        on[f] = o > 0.0f && f >= 4 ? o : 0.0f;
        prev[f & 3] = lv;
        sum += on[f]; sum2 += (double)on[f] * on[f];
    }
    int n = 0;
    out[n++] = 0;
    if (nf < 8) return n;
    float mean = (float)(sum / nf), sd = sqrtf((float)(sum2 / nf) - mean * mean);
    float thr = mean + sd * (2.4f - 2.0f * (float)bm_clampi(sens, 0, 127) / 127.0f);
    int gapf = (int)(min_gap / HOP);
    if (gapf < 2) gapf = 2;
    /* los picos más fuertes primero, separados por al menos min_gap */
    static uint8_t used[MAXF];
    memset(used, 0, sizeof used);
    while (n < BM_SLICES) {
        int best = -1;
        for (int f = 4; f < nf; f++) if (!used[f] && on[f] > thr && (best < 0 || on[f] > on[best])) best = f;
        if (best < 0) break;
        for (int f = best - gapf; f <= best + gapf; f++) if (f >= 0 && f < nf) used[f] = 1;
        int32_t pos = best * HOP + WIN / 2 - (int32_t)(BM_SMP_RATE * 6 / 1000);    /* 6 ms antes del ataque */
        if (pos < (int32_t)min_gap / 2) continue;
        uint32_t p = (uint32_t)pos, z = p;
        for (uint32_t k = 0; k < BM_SMP_RATE / 500 && p >= k + 1; k++)          /* cruce por cero, hasta 2 ms antes */
            if ((d[p - k] >= 0) != (d[p - k - 1] >= 0)) { z = p - k; break; }
        out[n++] = z;
    }
    for (int i = 1; i < n; i++) for (int j = i; j > 1 && out[j] < out[j - 1]; j--) { uint32_t t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}
