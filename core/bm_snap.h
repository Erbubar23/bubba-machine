/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — snapshots: sonidos de B-MINI, BITTAL y BUNO-6 (docs/DISENO_0.6.md §4).
 *
 * Un snapshot (.bsnap) es el sonido entero de una máquina: sus ajustes (bm_pattern.prm), sus 4 macros
 * y, en BITTAL, una o dos tablas en espectro (core/bm_wt.h). Los hace el editor web desde un preset
 * de Vital (web/bsnap.js) y se guardan:
 *   - de fábrica, en la ROM (core/gen/bm_snap_rom.c, armado por tools/gen_snaps.mjs);
 *   - del usuario, en la zona que comparten con las tomas (0xC0000-0xD8FFF): las tomas se ubican
 *     desde abajo y los snapshots desde arriba, cada uno en sectores seguidos.
 * PRESETS recorre, para la máquina del canal, primero los de fábrica y después los del usuario.
 * Un banco .syx de la DX7 viaja igual (máquina ENG_FM, sin tablas, 32 voces detrás de la cabecera). */
#ifndef BM_SNAP_H
#define BM_SNAP_H

#include "bm_store.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BM_SNAP_MAGIC  0x31534E42u      /* "BNS1" */
#define BM_SNAP_HDR    88
#define BM_SNAP_VER    2                 /* 2: B-MINI con TUNE y macros CUTOFF EMPHASIS CONTOUR F DECAY; BUNO-6 con PWM y macro LFO */
#define BM_SNAP_PRM    48
#define BM_SNAP_MAX    8800              /* cabecera + dos tablas de 8 frames x 256 armónicos */
#define BM_SNAPS_USER  12                /* snapshots del usuario como máximo */
#define BM_FM_BANK     (BM_SNAP_HDR + 32 * 156)   /* un banco de BX-7: 32 voces de la DX7 sin empaquetar */

enum { SN_LIC_OWN, SN_LIC_CC0, SN_LIC_UNLICENSE, SN_LIC_GPL3, SN_LICS };
extern const char *const bm_snap_lic_names[SN_LICS];

/* La cabecera tal como está en el archivo (88 bytes, little-endian). */
typedef struct {
    uint32_t magic;
    uint8_t eng;            /* ENG_MINI, ENG_BITTAL, ENG_BUNO (bm_pattern.h) */
    uint8_t ntab;           /* tablas que siguen (0..2) */
    uint8_t lic;            /* SN_LIC_* */
    uint8_t ver;
    uint16_t id;            /* huella del contenido: los 4 hex que muestra la pantalla */
    uint16_t size;          /* bytes del snapshot entero */
    char name[12], author[12];
    uint8_t macro[4];
    uint8_t prm[BM_SNAP_PRM];
} bm_snap_hdr;

typedef struct { const uint8_t *p; uint16_t size; } bm_snap_ref;

/* banco de fábrica (ROM) */
extern const bm_snap_ref bm_snap_rom[];
extern const int bm_snap_rom_n;

/* ¿son bytes de un snapshot sano? (firma, largo, tablas que caben) */
int  bm_snap_ok(const uint8_t *p, uint32_t len);
/* la tabla i (0 o 1) del snapshot, o NULL. Con una sola tabla, la 1 es la 0. */
const uint8_t *bm_snap_table(const uint8_t *p, int i);
uint16_t bm_snap_hash(const uint8_t *p, uint32_t len);   /* huella (se calcula con id = 0) */

/* ------------------------------------------------------------ el banco: fábrica + usuario */
typedef const uint8_t *(*bm_snap_map_fn)(void *user, uint32_t addr);
/* lee los snapshots del usuario de la zona compartida */
int  bm_snaps_scan(const bm_flash_ops *f, bm_snap_map_fn map, void *mu);
int  bm_snaps_user_n(void);
const uint8_t *bm_snaps_user_p(int i, int *nsect);    /* el sonido del usuario i y sus sectores, o NULL */
/* sectores de la zona que ocupan los snapshots del usuario (para la biblioteca de tomas) */
void bm_snaps_mark(uint8_t *used, int nsect);
/* guarda uno nuevo (arriba de todo lo libre). Devuelve su índice de usuario o -1 si no cabe. */
int  bm_snaps_write(const bm_flash_ops *f, bm_snap_map_fn map, void *mu, const uint8_t *p, uint32_t len,
                    const uint8_t *takes_used);
int  bm_snaps_erase(const bm_flash_ops *f, int user_idx);
int  bm_snaps_user_of(const uint8_t *p);                  /* el índice de usuario de un snapshot, o -1 */
void bm_snaps_format(void);                               /* olvida los del usuario (la zona se borró) */

/* los de una máquina, en orden: fábrica y después usuario */
int  bm_snap_count(int eng);
const uint8_t *bm_snap_get(int eng, int idx);
int  bm_snap_is_user(int eng, int idx);
const char *bm_snap_name(int eng, int idx);

/* BX-7: los 16 patches de fábrica y después las voces de los bancos del usuario */
int  bm_fm_count(void);
const uint8_t *bm_fm_patch(int idx);
const char *bm_fm_patch_name(int idx);

#ifdef __cplusplus
}
#endif

#endif
