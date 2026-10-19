/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — wavetables de BITTAL armadas desde su espectro (core/bm_wt.h). */
#include <math.h>
#include <string.h>
#include "bm_wt.h"

const uint16_t bm_wt_off[BM_WT_LEVELS] = {0, 1024, 1536, 1792, 1920, 1984, 2016};

/* 0.9: one pool of BM_WT_POOL frames for both slots (it was 8 per slot): slot 0 takes its frames
 * from the front, slot 1 from the back. When the two tables want more than the pool, each keeps
 * frames spread over its whole sweep: the smaller table up to half the pool, the other the rest
 * (8 + 8 -> 6 + 6, 8 + 1 -> all). The split depends only on the two tables, not on which came
 * first: when a slot is rebuilt, the other one shrinks in place (or is rebuilt from its spectrum
 * to grow back). */
static int16_t pool[BM_WT_POOL][BM_WT_TOT];
static struct {
    const uint8_t *spec;           /* the spectrum it was built from (ROM or flash: it stays mapped) */
    uint8_t want, nf;              /* frames in the spectrum (up to 8) and frames built */
    volatile uint8_t ready;
} wt[BM_WT_SLOTS];

static int base_of(int slot) { return slot ? BM_WT_POOL - wt[1].nf : 0; }
static int16_t *frame_at(int slot, int f) { return pool[base_of(slot) + f]; }

/* the frames each slot keeps when they both want theirs */
static void caps(int c[2])
{
    int w0 = wt[0].want, w1 = wt[1].want;
    if (w0 + w1 <= BM_WT_POOL) { c[0] = w0; c[1] = w1; return; }
    int small = w0 <= w1 ? 0 : 1, ws = small ? w1 : w0;
    c[small] = ws < BM_WT_POOL / 2 ? ws : BM_WT_POOL / 2;
    c[!small] = BM_WT_POOL - c[small];
}

/* frame i of c taken from want frames: spread over the whole sweep, the ends kept */
static int pick(int i, int want, int c) { return c <= 1 || want <= 1 ? 0 : (i * (want - 1) * 2 + (c - 1)) / ((c - 1) * 2); }

/* FFT compleja de base 2, en el lugar (n <= 1024); inv = 1 para la inversa (sin escalar). */
static void fft(float *re, float *im, int n, int inv)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = (inv ? 6.28318530718f : -6.28318530718f) / (float)len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = a + len / 2;
                float xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr; im[a] += xi;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
}

static inline float mag_of(uint8_t v) { return v ? powf(10.0f, (float)((int)v - 255) * 0.375f / 20.0f) : 0.0f; }

/* un frame f del espectro, en el nivel l, en re[] (n = BM_WT_N >> l muestras) */
static float re[BM_WT_N], im[BM_WT_N];
static void synth_frame(const uint8_t *spec, int f, int l)
{
    int nh = bm_wt_spec_nh(spec), n = BM_WT_N >> l, hmax = BM_WT_HARM >> l;
    const uint8_t *d = spec + 2 + f * nh * 2;
    for (int i = 0; i < n; i++) { re[i] = 0.0f; im[i] = 0.0f; }
    for (int h = 1; h <= nh && h <= hmax && h < n / 2; h++) {
        float a = mag_of(d[(h - 1) * 2]), p = (float)d[(h - 1) * 2 + 1] * (6.28318530718f / 256.0f);
        re[h] = a * cosf(p); im[h] = a * sinf(p);
        re[n - h] = re[h]; im[n - h] = -im[h];
    }
    fft(re, im, n, 1);
}

/* builds frames [0, c) of slot from its spectrum (the frames picked over its whole sweep) */
static void build_frames(int slot, int c)
{
    const uint8_t *spec = wt[slot].spec;
    int want = wt[slot].want;
    wt[slot].nf = (uint8_t)c;
    if (!c) return;
    float peak = 0.0f;                              /* el pico del nivel 0 de todos los frames */
    for (int f = 0; f < want; f++) {
        synth_frame(spec, f, 0);
        for (int i = 0; i < BM_WT_N; i++) { float a = fabsf(re[i]); if (a > peak) peak = a; }
    }
    float g = peak > 0.0f ? 30000.0f / peak : 0.0f;
    for (int f = 0; f < c; f++)
        for (int l = 0; l < BM_WT_LEVELS; l++) {
            synth_frame(spec, pick(f, want, c), l);
            int16_t *o = frame_at(slot, f) + bm_wt_off[l];
            for (int i = 0, n = BM_WT_N >> l; i < n; i++) {
                float v = re[i] * g;
                o[i] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
            }
        }
}

/* the spectrum a slot was built from still looks like one (a user sound erased from flash does not) */
static int spec_ok(int slot)
{
    const uint8_t *s = wt[slot].spec;
    return s && s[0] && (s[0] > BM_WT_FRAMES ? BM_WT_FRAMES : s[0]) == wt[slot].want;
}

/* the other slot to c frames: in place when it holds all its frames (the same ones a build to c
 * picks), else from its spectrum */
static void refit(int slot, int c)
{
    if (wt[slot].nf == c || !wt[slot].want) return;
    wt[slot].ready = 0;
    if (c < wt[slot].nf && wt[slot].nf == wt[slot].want) {
        int nf = wt[slot].nf, old = slot ? BM_WT_POOL - nf : 0, nw = slot ? BM_WT_POOL - c : 0;
        if (slot) for (int i = c - 1; i >= 0; i--) memmove(pool[nw + i], pool[old + pick(i, nf, c)], sizeof pool[0]);
        else for (int i = 0; i < c; i++) memmove(pool[nw + i], pool[old + pick(i, nf, c)], sizeof pool[0]);
        wt[slot].nf = (uint8_t)c;
    } else if (spec_ok(slot)) build_frames(slot, c);
    else { wt[slot].nf = 0; wt[slot].want = 0; return; }
    wt[slot].ready = 1;
}

void bm_wt_build(int slot, const uint8_t *spec)
{
    if (slot < 0 || slot >= BM_WT_SLOTS) return;
    wt[slot].ready = 0;
    wt[slot].nf = 0;
    if (!spec || !spec[0]) { wt[slot].spec = NULL; wt[slot].want = 0; return; }
    wt[slot].spec = spec;
    wt[slot].want = (uint8_t)(spec[0] > BM_WT_FRAMES ? BM_WT_FRAMES : spec[0]);
    int c[2];
    caps(c);
    int o = !slot;
    if (wt[o].nf > c[o]) refit(o, c[o]);            /* room first */
    build_frames(slot, c[slot]);
    wt[slot].ready = 1;
    if (wt[o].nf < c[o]) refit(o, c[o]);            /* and the other one grows back if it can */
}

int bm_wt_ready(int slot) { return slot >= 0 && slot < BM_WT_SLOTS && wt[slot].ready && wt[slot].nf; }
int bm_wt_frames(int slot) { return slot >= 0 && slot < BM_WT_SLOTS ? wt[slot].nf : 0; }

const int16_t *bm_wt_data(int slot, int f, int l, int *n)
{
    int nf = wt[slot].nf ? wt[slot].nf : 1;
    if (f >= nf) f = nf - 1;
    if (f < 0) f = 0;
    if (l < 0) l = 0;
    if (l >= BM_WT_LEVELS) l = BM_WT_LEVELS - 1;
    *n = BM_WT_N >> l;
    int at = slot ? BM_WT_POOL - nf + f : f;
    if (at < 0) at = 0;
    return pool[at] + bm_wt_off[l];
}

int bm_wt_level_for(float hz)
{
    int l = 0;
    while (l < BM_WT_LEVELS - 1 && (float)(BM_WT_HARM >> l) * hz > 19000.0f) l++;
    return l;
}
