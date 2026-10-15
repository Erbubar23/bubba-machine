/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — snapshots y su banco (core/bm_snap.h). */
#include <string.h>
#include "bm_snap.h"
#include "bm_pattern.h"
#include "bm_util.h"
#include "bm_wt.h"
#include "bm_rom.h"

const char *const bm_snap_lic_names[SN_LICS] = {"BUBBA", "CC0", "UNLICENSE", "GPL-3.0"};

static uint32_t get32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static uint16_t get16(const uint8_t *b) { return (uint16_t)(b[0] | b[1] << 8); }

int bm_snap_ok(const uint8_t *p, uint32_t len)
{
    if (!p || len < BM_SNAP_HDR || get32(p) != BM_SNAP_MAGIC) return 0;
    uint32_t size = get16(p + 10);
    if (size < BM_SNAP_HDR || size > len || size > BM_SNAP_MAX) return 0;
    int eng = p[4], ntab = p[5];
    if (eng == ENG_FM) return ntab == 0 && size == BM_FM_BANK;
    if (eng != ENG_MINI && eng != ENG_BITTAL && eng != ENG_BUNO) return 0;
    if (ntab > 2 || (eng != ENG_BITTAL && ntab)) return 0;
    uint32_t at = BM_SNAP_HDR;
    for (int i = 0; i < ntab; i++) {
        if (at + 2 > size) return 0;
        const uint8_t *t = p + at;
        if (!t[0] || t[0] > BM_WT_FRAMES) return 0;
        at += (uint32_t)bm_wt_spec_size(t);
        if (at > size) return 0;
    }
    return 1;
}

const uint8_t *bm_snap_table(const uint8_t *p, int i)
{
    int ntab = p[5];
    if (!ntab) return NULL;
    if (i >= ntab) i = 0;
    const uint8_t *t = p + BM_SNAP_HDR;
    for (int k = 0; k < i; k++) t += bm_wt_spec_size(t);
    return t;
}

uint16_t bm_snap_hash(const uint8_t *p, uint32_t len)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t b = (i == 8 || i == 9) ? 0 : p[i];          /* el id no entra en su propia huella */
        h = (h ^ b) * 16777619u;
    }
    return (uint16_t)(h ^ (h >> 16));
}

/* ------------------------------------------------------------ los del usuario, en la zona compartida */
#define ZONE_SECTORS ((BM_TAKE_HI - BM_TAKE_LO) / BM_SECTOR)
static struct { const uint8_t *p; uint8_t sector, nsect; } user[BM_SNAPS_USER];
static int nuser;

static uint32_t sec_addr(int s) { return BM_TAKE_LO + (uint32_t)s * BM_SECTOR; }
static int sectors_for(uint32_t bytes) { return (int)((bytes + 4 + BM_SECTOR - 1) / BM_SECTOR); }   /* + CRC */

int bm_snaps_scan(const bm_flash_ops *f, bm_snap_map_fn map, void *mu)
{
    nuser = 0;
    for (int s = 0; s < (int)ZONE_SECTORS && nuser < BM_SNAPS_USER; s++) {
        uint8_t h[12];
        if (f->read(sec_addr(s), h, sizeof h) || get32(h) != BM_SNAP_MAGIC) continue;
        uint32_t size = get16(h + 10);
        if (size < BM_SNAP_HDR || size > BM_SNAP_MAX) continue;
        int ns = sectors_for(size);
        if (s + ns > (int)ZONE_SECTORS) continue;
        const uint8_t *p = map ? map(mu, sec_addr(s)) : NULL;
        if (!p || !bm_snap_ok(p, size) || bm_crc32(0, p, size) != get32(p + size)) continue;
        user[nuser].p = p; user[nuser].sector = (uint8_t)s; user[nuser].nsect = (uint8_t)ns;
        nuser++;
        s += ns - 1;
    }
    return nuser;
}

int bm_snaps_user_n(void) { return nuser; }

const uint8_t *bm_snaps_user_p(int i, int *nsect)
{
    if (i < 0 || i >= nuser) return NULL;
    if (nsect) *nsect = user[i].nsect;
    return user[i].p;
}

void bm_snaps_mark(uint8_t *used, int nsect)
{
    for (int i = 0; i < nuser; i++)
        for (int k = 0; k < user[i].nsect; k++) if (user[i].sector + k < nsect) used[user[i].sector + k] = 1;
}

void bm_snaps_format(void) { nuser = 0; }

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

int bm_snaps_write(const bm_flash_ops *f, bm_snap_map_fn map, void *mu, const uint8_t *p, uint32_t len,
                   const uint8_t *takes_used)
{
    if (!bm_snap_ok(p, len) || nuser >= BM_SNAPS_USER) return -1;
    uint32_t size = get16(p + 10);
    int need = sectors_for(size);
    uint8_t used[ZONE_SECTORS];
    memset(used, 0, sizeof used);
    if (takes_used) memcpy(used, takes_used, sizeof used);
    bm_snaps_mark(used, (int)ZONE_SECTORS);
    int at = -1;
    for (int s = (int)ZONE_SECTORS - need; s >= 0 && at < 0; s--) {     /* desde arriba */
        int ok = 1;
        for (int k = 0; k < need; k++) if (used[s + k]) { ok = 0; break; }
        if (ok) at = s;
    }
    if (at < 0) return -1;
    for (int k = 0; k < need; k++) if (f->erase(sec_addr(at + k))) return -1;
    uint32_t crc = bm_crc32(0, p, size);
    uint8_t c4[4] = {(uint8_t)crc, (uint8_t)(crc >> 8), (uint8_t)(crc >> 16), (uint8_t)(crc >> 24)};
    if (program_span(f, sec_addr(at) + 4, p + 4, size - 4)) return -1;
    if (program_span(f, sec_addr(at) + size, c4, 4)) return -1;
    if (program_span(f, sec_addr(at), p, 4)) return -1;                   /* la firma al final: commit */
    bm_snaps_scan(f, map, mu);
    for (int i = 0; i < nuser; i++) if (user[i].sector == at) return i;
    return -1;
}

int bm_snaps_user_of(const uint8_t *p)
{
    for (int i = 0; i < nuser; i++) if (user[i].p == p) return i;
    return -1;
}

int bm_snaps_erase(const bm_flash_ops *f, int i)
{
    if (i < 0 || i >= nuser) return -1;
    for (int k = 0; k < user[i].nsect; k++) if (f->erase(sec_addr(user[i].sector + k))) return -1;
    for (int k = i; k < nuser - 1; k++) user[k] = user[k + 1];
    nuser--;
    return 0;
}

/* ------------------------------------------------------------ por máquina */
int bm_snap_count(int eng)
{
    int n = 0;
    for (int i = 0; i < bm_snap_rom_n; i++) n += bm_snap_rom[i].p[4] == eng;
    for (int i = 0; i < nuser; i++) n += user[i].p[4] == eng;
    return n;
}

static const uint8_t *nth(int eng, int idx, int *is_user)
{
    for (int i = 0; i < bm_snap_rom_n; i++) if (bm_snap_rom[i].p[4] == eng && idx-- == 0) { *is_user = 0; return bm_snap_rom[i].p; }
    for (int i = 0; i < nuser; i++) if (user[i].p[4] == eng && idx-- == 0) { *is_user = 1; return user[i].p; }
    return NULL;
}

const uint8_t *bm_snap_get(int eng, int idx) { int u; return idx < 0 ? NULL : nth(eng, idx, &u); }
int bm_snap_is_user(int eng, int idx) { int u = 0; return idx >= 0 && nth(eng, idx, &u) ? u : 0; }

int bm_fm_count(void) { return BM_FM_PATCHES + 32 * bm_snap_count(ENG_FM); }

const uint8_t *bm_fm_patch(int idx)
{
    int n = bm_fm_count();
    idx = (idx % n + n) % n;
    if (idx < BM_FM_PATCHES) return bm_fm_rom[idx];
    idx -= BM_FM_PATCHES;
    const uint8_t *bank = bm_snap_get(ENG_FM, idx / 32);
    return bank ? bank + BM_SNAP_HDR + (idx % 32) * 156 : bm_fm_rom[0];
}

const char *bm_fm_patch_name(int idx)
{
    static char b[11];
    int n = bm_fm_count();
    idx = (idx % n + n) % n;
    if (idx < BM_FM_PATCHES) return bm_fm_names[idx];
    const uint8_t *p = bm_fm_patch(idx);
    int k = 0;
    for (int i = 0; i < 10; i++) { char c = (char)p[145 + i]; b[k++] = c >= 'a' && c <= 'z' ? (char)(c - 32) : (c >= 32 && c < 127 ? c : ' '); }
    while (k && b[k - 1] == ' ') k--;
    b[k] = 0;
    return b;
}

const char *bm_snap_name(int eng, int idx)
{
    static char b[13];
    const uint8_t *p = bm_snap_get(eng, idx);
    if (!p) return "INIT";
    memcpy(b, p + 12, 12); b[12] = 0;
    return b;
}
