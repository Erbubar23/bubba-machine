/* Bubba Machine — almacenamiento en flash raw: log circular con commit al final (SPEC_V3 §4.5). */
#ifndef BM_STORE_H
#define BM_STORE_H

#include "bm_types.h"

typedef struct {
    int (*read)(uint32_t addr, void *dst, uint32_t n);
    int (*erase)(uint32_t addr);                              /* sector de 4 KiB */
    int (*program)(uint32_t addr, const void *src, uint32_t n); /* <= 256 B, sin cruzar página */
} bm_flash_ops;

#define BM_OBJ_PATTERN0  0       /* 0..15 */
#define BM_OBJ_SONG      16
#define BM_OBJ_COUNT     17
#define BM_STORE_SLOTS   ((BM_STORE_HI - BM_STORE_LO) / BM_SLOT_SIZE)
#define BM_SLOT_BODY     (BM_SLOT_SIZE - 16u)
#define BM_GLOB_SLOTS    ((BM_GLOB_HI - BM_GLOB_LO) / BM_SECTOR)
#define BM_GLOB_BODY     (BM_SECTOR - 16u)

typedef struct {
    const bm_flash_ops *f;
    uint32_t seq;
    int16_t  live[BM_OBJ_COUNT];
    uint32_t live_seq[BM_OBJ_COUNT];
    uint16_t live_len[BM_OBJ_COUNT];
    uint16_t next;
    int16_t  glob_live;
    uint32_t glob_seq;
    uint16_t glob_len;
    uint32_t writes;          /* sectores borrados desde el arranque (estadística) */
} bm_store;

int  bm_store_mount(bm_store *s, const bm_flash_ops *f);
int  bm_store_write(bm_store *s, uint8_t obj, const uint8_t *body, uint16_t len);
int  bm_store_read(const bm_store *s, uint8_t obj, uint8_t *dst, int cap);   /* -1 si no hay */
bool bm_store_has(const bm_store *s, uint8_t obj);
int  bm_store_used_slots(const bm_store *s);
int  bm_store_write_glob(bm_store *s, const uint8_t *body, uint16_t len);
int  bm_store_read_glob(const bm_store *s, uint8_t *dst, int cap);

#endif
