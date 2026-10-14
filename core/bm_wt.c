/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — wavetables de BITTAL armadas desde su espectro (core/bm_wt.h). */
#include <math.h>
#include <string.h>
#include "bm_wt.h"

const uint16_t bm_wt_off[BM_WT_LEVELS] = {0, 1024, 1536, 1792, 1920, 1984, 2016};

static struct {
    int16_t s[BM_WT_FRAMES][BM_WT_TOT];
    uint8_t nf;
    volatile uint8_t ready;
} wt[BM_WT_SLOTS];

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

void bm_wt_build(int slot, const uint8_t *spec)
{
    if (slot < 0 || slot >= BM_WT_SLOTS) return;
    wt[slot].ready = 0;
    if (!spec || !spec[0]) { memset(wt[slot].s, 0, sizeof wt[slot].s); wt[slot].nf = 0; return; }
    int nf = spec[0] > BM_WT_FRAMES ? BM_WT_FRAMES : spec[0];
    float peak = 0.0f;                              /* el pico del nivel 0 de todos los frames */
    for (int f = 0; f < nf; f++) {
        synth_frame(spec, f, 0);
        for (int i = 0; i < BM_WT_N; i++) { float a = fabsf(re[i]); if (a > peak) peak = a; }
    }
    float g = peak > 0.0f ? 30000.0f / peak : 0.0f;
    for (int f = 0; f < nf; f++)
        for (int l = 0; l < BM_WT_LEVELS; l++) {
            synth_frame(spec, f, l);
            int16_t *o = wt[slot].s[f] + bm_wt_off[l];
            for (int i = 0, n = BM_WT_N >> l; i < n; i++) {
                float v = re[i] * g;
                o[i] = (int16_t)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
            }
        }
    wt[slot].nf = (uint8_t)nf;
    wt[slot].ready = 1;
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
    return wt[slot].s[f] + bm_wt_off[l];
}

int bm_wt_level_for(float hz)
{
    int l = 0;
    while (l < BM_WT_LEVELS - 1 && (float)(BM_WT_HARM >> l) * hz > 19000.0f) l++;
    return l;
}
