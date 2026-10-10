/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — utilidades. */
#include <math.h>
#include "bm_util.h"

uint32_t bm_note_inc[128];
uint32_t bm_decay_coef[128];
uint32_t bm_attack_inc[128];
int32_t  bm_svf_g[128];
uint16_t bm_onepole_a[128];
uint16_t bm_pan_l[128], bm_pan_r[128];
uint32_t bm_semi_ratio[49];

#define PI 3.14159265358979323846

uint32_t bm_crc32(uint32_t crc, const void *data, size_t n)
{
    const uint8_t *p = (const uint8_t *)data;
    crc = ~crc;
    while (n--) {
        crc ^= *p++;
        for (int k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

static double cutoff_hz_d(int v)
{
    double f = 20.0 * pow(2.0, v / 127.0 * 10.0);
    double fmax = 0.45 * BM_FS;
    return f > fmax ? fmax : f;
}

static double decay_s(int v) { return 0.005 * pow(800.0, v / 127.0); }
static double attack_s(int v) { return 0.001 * pow(1000.0, v / 127.0); }

uint32_t bm_cutoff_hz(int v) { return (uint32_t)(cutoff_hz_d(bm_clampi(v, 0, 127)) + 0.5); }
uint32_t bm_decay_ms(int v) { return (uint32_t)(decay_s(bm_clampi(v, 0, 127)) * 1000.0 + 0.5); }
uint32_t bm_attack_ms(int v) { return (uint32_t)(attack_s(bm_clampi(v, 0, 127)) * 1000.0 + 0.5); }

void bm_tables_init(void)
{
    for (int n = 0; n < 128; n++) {
        double f = 440.0 * pow(2.0, (n - 69) / 12.0);
        bm_note_inc[n] = (uint32_t)(f / BM_FS * 4294967296.0);
    }
    for (int v = 0; v < 128; v++) {
        double T = decay_s(v);
        bm_decay_coef[v] = (uint32_t)(exp(-1.0 / (T * BM_FS)) * 1073741824.0);
        double A = attack_s(v);
        double inc = 1073741824.0 / (A * BM_FS);
        bm_attack_inc[v] = (uint32_t)(inc > 1073741824.0 ? 1073741824.0 : inc);
        double fc = cutoff_hz_d(v);
        bm_svf_g[v] = (int32_t)(tan(PI * fc / BM_FS) * 65536.0);
        bm_onepole_a[v] = (uint16_t)((1.0 - exp(-2.0 * PI * fc / BM_FS)) * 32767.0);
        double a = v / 127.0 * (PI / 2.0);
        bm_pan_l[v] = (uint16_t)(cos(a) * 32767.0 * 1.4142 > 32767.0 ? 32767 : cos(a) * 32767.0 * 1.4142);
        bm_pan_r[v] = (uint16_t)(sin(a) * 32767.0 * 1.4142 > 32767.0 ? 32767 : sin(a) * 32767.0 * 1.4142);
    }
    for (int i = 0; i < 49; i++)
        bm_semi_ratio[i] = (uint32_t)(pow(2.0, (i - 24) / 12.0) * 65536.0);
}

int bm_key_white_index(int k)
{
    if (k < 0 || k >= BM_KEYS || bm_key_is_black(k)) return -1;
    int w = 0;
    for (int i = 0; i < k; i++) if (!bm_key_is_black(i)) w++;
    return w;
}

int bm_key_black_index(int k)
{
    if (k < 0 || k >= BM_KEYS || !bm_key_is_black(k)) return -1;
    int b = 0;
    for (int i = 0; i < k; i++) if (bm_key_is_black(i)) b++;
    return b;
}

int bm_white_key(int i)
{
    for (int k = 0, w = 0; k < BM_KEYS; k++)
        if (!bm_key_is_black(k) && w++ == i) return k;
    return -1;
}

int bm_black_key(int i)
{
    for (int k = 0, b = 0; k < BM_KEYS; k++)
        if (bm_key_is_black(k) && b++ == i) return k;
    return -1;
}
