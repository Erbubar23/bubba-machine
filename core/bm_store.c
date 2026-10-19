/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — almacenamiento en flash.
 *
 * Cada objeto se escribe SIEMPRE en un slot nuevo del log (nunca encima del
 * vigente). Orden: borrar, programar el cuerpo, y al final la cabecera con
 * seq + CRC32. Si se corta la energía a la mitad, la cabecera no existe y el
 * slot se ignora al montar: la copia anterior sigue siendo la vigente.
 */
#include <string.h>
#include "bm_store.h"
#include "bm_util.h"

#define MAGIC 0x33504F4Bu   /* "KOP3" */

typedef struct {
    uint32_t magic;
    uint8_t obj, rsv;
    uint16_t len;
    uint32_t seq;
    uint32_t crc;
} hdr_t;

static int read_hdr(const bm_flash_ops *f, uint32_t a, hdr_t *h)
{
    uint8_t b[16];
    if (f->read(a, b, 16)) return -1;
    h->magic = (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
    h->obj = b[4]; h->rsv = b[5];
    h->len = (uint16_t)(b[6] | b[7] << 8);
    h->seq = (uint32_t)b[8] | (uint32_t)b[9] << 8 | (uint32_t)b[10] << 16 | (uint32_t)b[11] << 24;
    h->crc = (uint32_t)b[12] | (uint32_t)b[13] << 8 | (uint32_t)b[14] << 16 | (uint32_t)b[15] << 24;
    return 0;
}

static void put32(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }

static uint32_t crc_region(const bm_flash_ops *f, uint32_t a, uint32_t n)
{
    uint8_t buf[128];
    uint32_t crc = 0;
    while (n) {
        uint32_t k = n > sizeof buf ? (uint32_t)sizeof buf : n;
        if (f->read(a, buf, k)) return 0xFFFFFFFFu;
        crc = bm_crc32(crc, buf, k);
        a += k; n -= k;
    }
    return crc;
}

static bool valid(const bm_flash_ops *f, uint32_t a, const hdr_t *h, uint32_t max_body)
{
    return h->magic == MAGIC && h->len <= max_body && crc_region(f, a + 16, h->len) == h->crc;
}

int bm_store_mount(bm_store *s, const bm_flash_ops *f)
{
    memset(s, 0, sizeof *s);
    s->f = f;
    for (int i = 0; i < BM_OBJ_COUNT; i++) s->live[i] = -1;
    s->glob_live = -1;
    uint32_t best = 0; int best_slot = -1;
    for (uint32_t i = 0; i < BM_STORE_SLOTS; i++) {
        uint32_t a = BM_STORE_LO + i * BM_SLOT_SIZE;
        hdr_t h;
        if (read_hdr(f, a, &h)) return -1;
        if (h.obj >= BM_OBJ_COUNT || !valid(f, a, &h, BM_SLOT_BODY)) continue;
        bool gone = h.obj >= BM_PATTERNS && h.obj < BM_OBJ_SONG;   /* 0.9: los patrones 11-16 ya no existen (su slot se reusa) */
        if (!gone && (s->live[h.obj] < 0 || h.seq > s->live_seq[h.obj])) {
            s->live[h.obj] = (int16_t)i; s->live_seq[h.obj] = h.seq; s->live_len[h.obj] = h.len;
        }
        if (h.seq >= best) { best = h.seq; best_slot = (int)i; }
        if (h.seq > s->seq) s->seq = h.seq;
    }
    s->next = (uint16_t)((best_slot + 1) % (int)BM_STORE_SLOTS);
    for (uint32_t i = 0; i < BM_GLOB_SLOTS; i++) {
        uint32_t a = BM_GLOB_LO + i * BM_SECTOR;
        hdr_t h;
        if (read_hdr(f, a, &h)) return -1;
        if (h.obj != 0xEE || !valid(f, a, &h, BM_GLOB_BODY)) continue;
        if (s->glob_live < 0 || h.seq > s->glob_seq) { s->glob_live = (int16_t)i; s->glob_seq = h.seq; s->glob_len = h.len; }
        if (h.seq > s->seq) s->seq = h.seq;
    }
    return 0;
}

static bool slot_is_live(const bm_store *s, int slot)
{
    for (int i = 0; i < BM_OBJ_COUNT; i++) if (s->live[i] == slot) return true;
    return false;
}

/* Programa n bytes respetando páginas de 256 B. */
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

static int write_at(bm_store *s, uint32_t a, uint32_t sectors, uint8_t obj, const uint8_t *body, uint16_t len)
{
    const bm_flash_ops *f = s->f;
    for (uint32_t k = 0; k < sectors; k++) {
        if (f->erase(a + k * BM_SECTOR)) return -1;
        s->writes++;
    }
    if (len && program_span(f, a + 16, body, len)) return -1;
    uint8_t h[16];
    put32(h, MAGIC); h[4] = obj; h[5] = 0; h[6] = (uint8_t)len; h[7] = (uint8_t)(len >> 8);
    put32(h + 8, ++s->seq);
    put32(h + 12, bm_crc32(0, body, len));
    if (program_span(f, a, h, 16)) return -1;          /* commit: cabecera al final */
    hdr_t back;
    if (read_hdr(f, a, &back) || !valid(f, a, &back, sectors * BM_SECTOR - 16)) return -1;
    return 0;
}

int bm_store_write(bm_store *s, uint8_t obj, const uint8_t *body, uint16_t len)
{
    if (obj >= BM_OBJ_COUNT || len > BM_SLOT_BODY) return -1;
    for (uint32_t tries = 0; tries < BM_STORE_SLOTS; tries++) {
        int slot = s->next;
        s->next = (uint16_t)((s->next + 1) % BM_STORE_SLOTS);
        if (slot_is_live(s, slot)) continue;
        uint32_t a = BM_STORE_LO + (uint32_t)slot * BM_SLOT_SIZE;
        if (write_at(s, a, BM_SLOT_SIZE / BM_SECTOR, obj, body, len)) continue;   /* slot malo: siguiente */
        s->live[obj] = (int16_t)slot; s->live_seq[obj] = s->seq; s->live_len[obj] = len;
        return 0;
    }
    return -1;
}

int bm_store_read(const bm_store *s, uint8_t obj, uint8_t *dst, int cap)
{
    if (obj >= BM_OBJ_COUNT || s->live[obj] < 0) return -1;
    int len = s->live_len[obj];
    if (len > cap) return -1;
    if (s->f->read(BM_STORE_LO + (uint32_t)s->live[obj] * BM_SLOT_SIZE + 16, dst, (uint32_t)len)) return -1;
    return len;
}

bool bm_store_has(const bm_store *s, uint8_t obj)
{
    return obj < BM_OBJ_COUNT && s->live[obj] >= 0 && s->live_len[obj] > 0;
}

int bm_store_used_slots(const bm_store *s)
{
    int n = 0;
    for (int i = 0; i < BM_OBJ_COUNT; i++) if (s->live[i] >= 0) n++;
    return n;
}

int bm_store_write_glob(bm_store *s, const uint8_t *body, uint16_t len)
{
    if (len > BM_GLOB_BODY) return -1;
    int slot = (s->glob_live + 1) % (int)BM_GLOB_SLOTS;
    uint32_t a = BM_GLOB_LO + (uint32_t)slot * BM_SECTOR;
    if (write_at(s, a, 1, 0xEE, body, len)) return -1;
    s->glob_live = (int16_t)slot; s->glob_seq = s->seq; s->glob_len = len;
    return 0;
}

int bm_store_read_glob(const bm_store *s, uint8_t *dst, int cap)
{
    if (s->glob_live < 0 || s->glob_len > cap) return -1;
    if (s->f->read(BM_GLOB_LO + (uint32_t)s->glob_live * BM_SECTOR + 16, dst, s->glob_len)) return -1;
    return s->glob_len;
}

int bm_store_format(bm_store *s, const bm_flash_ops *f)
{
    for (uint32_t a = BM_STORE_LO; a < BM_STORE_HI; a += BM_SECTOR) if (f->erase(a)) return -1;
    for (uint32_t a = BM_GLOB_LO; a < BM_GLOB_HI; a += BM_SECTOR) if (f->erase(a)) return -1;
    return bm_store_mount(s, f);
}
