/* Bubba Machine — slots de sample en flash. */
#include <string.h>
#include "bm_sample.h"
#include "bm_util.h"

#define SMP_MAGIC 0x344D534Bu   /* "KSM4" */

static void put32(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

uint32_t bm_sample_addr(int slot) { return BM_SMP_LO + (uint32_t)slot * BM_SMP_SLOT; }

/* cabecera: magic 4 | len 4 | rate 2 | nmarks 1 | rsv 1 | marks 16x4 | crc 4 | relleno */
static void make_hdr(uint8_t h[BM_SMP_HDR], uint32_t len, const uint32_t *marks, int nmarks, uint32_t crc)
{
    memset(h, 0xFF, BM_SMP_HDR);
    put32(h, SMP_MAGIC); put32(h + 4, len);
    h[8] = (uint8_t)BM_SMP_RATE; h[9] = (uint8_t)(BM_SMP_RATE >> 8);
    h[10] = (uint8_t)nmarks; h[11] = 0;
    for (int i = 0; i < BM_SLICES; i++) put32(h + 12 + 4 * i, i < nmarks ? marks[i] : 0);
    put32(h + 76, crc);
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

int bm_sample_info(const bm_flash_ops *f, const uint8_t *(*map)(void *, uint32_t), void *mu, int slot, bm_smp_src *out)
{
    memset(out, 0, sizeof *out);
    if (slot < 0 || slot >= BM_SMP_SLOTS) return -1;
    uint8_t h[BM_SMP_HDR];
    uint32_t a = bm_sample_addr(slot);
    if (f->read(a, h, BM_SMP_HDR)) return -1;
    uint32_t len = get32(h + 4);
    if (get32(h) != SMP_MAGIC || len == 0 || len > BM_SMP_MAX || h[10] > BM_SLICES) return -1;
    const uint8_t *d = map ? map(mu, a + BM_SMP_HDR) : NULL;
    if (!d) return -1;
    if (bm_crc32(0, d, len) != get32(h + 76)) return -1;
    out->data = (const int8_t *)d;
    out->len = len;
    out->nmarks = h[10];
    for (int i = 0; i < out->nmarks; i++) {
        out->marks[i] = get32(h + 12 + 4 * i);
        if (out->marks[i] >= len) { out->nmarks = (uint8_t)i; break; }
    }
    return 0;
}

int bm_sample_write(const bm_flash_ops *f, int slot, const int8_t *data, uint32_t len, const uint32_t *marks, int nmarks)
{
    if (slot < 0 || slot >= BM_SMP_SLOTS || len > BM_SMP_MAX) return -1;
    uint32_t a = bm_sample_addr(slot);
    uint32_t sectors = (BM_SMP_HDR + len + BM_SECTOR - 1) / BM_SECTOR;
    for (uint32_t k = 0; k < sectors; k++) if (f->erase(a + k * BM_SECTOR)) return -1;
    if (len && program_span(f, a + BM_SMP_HDR, (const uint8_t *)data, len)) return -1;
    uint8_t h[BM_SMP_HDR];
    make_hdr(h, len, marks, nmarks, bm_crc32(0, data, len));
    return program_span(f, a, h, BM_SMP_HDR);                 /* commit al final */
}

int bm_sample_erase(const bm_flash_ops *f, int slot)
{
    if (slot < 0 || slot >= BM_SMP_SLOTS) return -1;
    return f->erase(bm_sample_addr(slot));
}

int bm_sample_set_marks(const bm_flash_ops *f, int slot, const uint32_t *marks, int nmarks, uint8_t *scratch)
{
    uint32_t a = bm_sample_addr(slot);
    if (f->read(a, scratch, BM_SECTOR)) return -1;
    if (get32(scratch) != SMP_MAGIC) return -1;
    uint32_t len = get32(scratch + 4), crc = get32(scratch + 76);
    make_hdr(scratch, len, marks, nmarks, crc);
    uint8_t hdr[BM_SMP_HDR];
    memcpy(hdr, scratch, BM_SMP_HDR);
    if (f->erase(a)) return -1;
    if (program_span(f, a + BM_SMP_HDR, scratch + BM_SMP_HDR, BM_SECTOR - BM_SMP_HDR)) return -1;
    return program_span(f, a, hdr, BM_SMP_HDR);
}
