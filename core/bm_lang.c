/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — idiomas de la pantalla (ver bm_lang.h). */
#include <string.h>
#include "bm_lang.h"

static const uint8_t *pack;          /* paquete activo en la flash, o NULL = inglés */
static uint16_t pack_n;
static char pack_name[12];

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

uint32_t bm_lang_hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

uint32_t bm_lang_crc(const uint8_t *d, uint32_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

uint32_t bm_lang_check(const uint8_t *p, uint32_t cap, char name[12])
{
    if (!p || cap < BM_LANG_HDR || rd32(p) != BM_LANG_MAGIC || p[4] != BM_LANG_VER) return 0;
    uint32_t n = rd16(p + 6), size = rd32(p + 20);
    if (size > cap || size < BM_LANG_HDR + n * 6u + 1u || p[8 + 11] != 0) return 0;
    if (bm_lang_crc(p + BM_LANG_HDR, size - BM_LANG_HDR) != rd32(p + 24)) return 0;
    uint32_t txt = BM_LANG_HDR + n * 6u, tlen = size - txt;
    if (p[size - 1] != 0) return 0;                          /* el último texto termina dentro */
    for (uint32_t i = 0; i < n; i++) {
        if (rd16(p + BM_LANG_HDR + n * 4u + i * 2u) >= tlen) return 0;
        if (i && rd32(p + BM_LANG_HDR + i * 4u) <= rd32(p + BM_LANG_HDR + (i - 1) * 4u)) return 0;
    }
    if (name) memcpy(name, p + 8, 12);
    return size;
}

void bm_lang_use(const uint8_t *p)
{
    pack = p;
    pack_n = p ? rd16(p + 6) : 0;
    if (p) memcpy(pack_name, p + 8, 12);
}

const char *bm_lang_name(void) { return pack ? pack_name : "ENGLISH"; }

const char *bm_txt(const char *en)
{
    if (!pack || !en || !*en) return en;
    uint32_t h = bm_lang_hash(en);
    const uint8_t *hs = pack + BM_LANG_HDR;
    int lo = 0, hi = (int)pack_n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t v = rd32(hs + mid * 4);
        if (v == h) return (const char *)(pack + BM_LANG_HDR + pack_n * 6u + rd16(hs + pack_n * 4u + mid * 2));
        if (v < h) lo = mid + 1; else hi = mid - 1;
    }
    return en;
}

/* los % de un formato, en orden (sin %%): tienen que ser los mismos en la traducción */
static int same_specs(const char *a, const char *b)
{
    for (;;) {
        while (*a && !(a[0] == '%' && a[1] != '%')) a += (a[0] == '%') ? 2 : 1;
        while (*b && !(b[0] == '%' && b[1] != '%')) b += (b[0] == '%') ? 2 : 1;
        if (!*a || !*b) return !*a && !*b;
        a++; b++;
        while (*a && !((*a >= 'a' && *a <= 'z') || (*a >= 'A' && *a <= 'Z'))) { if (*a != *b) return 0; a++; b++; }
        while (*a == 'l' || *a == 'h' || *a == 'z') { if (*a != *b) return 0; a++; b++; }
        if (*a != *b) return 0;
        if (*a) { a++; b++; }
    }
}

const char *bm_txtf(const char *en)
{
    const char *t = bm_txt(en);
    return t == en || same_specs(en, t) ? t : en;
}
