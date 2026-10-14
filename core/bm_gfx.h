/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — gráficos para la TFT de 240x240 RGB565 sin framebuffer.
 *
 * Se dibuja en modo inmediato por franjas de BM_STRIP_H filas: toda la escena
 * se vuelve a "dibujar" para cada franja con recorte. Cada franja se divide en
 * tiles de BM_TILE_W px; solo se envían por SPI los tiles cuyo hash cambió
 * (SPEC_V3 §6.3: presupuesto de ~37 KB por frame a 12 MHz). */
#ifndef BM_GFX_H
#define BM_GFX_H

#include "bm_types.h"

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

/* Paleta de Bubba Machine 0.6 (docs/DISENO_0.6.md §5.1): fondo negro carbón azulado, una rejilla
 * apenas visible y cuatro acentos de neón. Los nombres viejos se quedan con los colores nuevos. */
#define C_BLACK   RGB565(10, 11, 16)      /* #0A0B10 Void Black: el fondo */
#define C_DDGREY  RGB565(22, 25, 37)      /* #161925 la rejilla */
#define C_DGREY   RGB565(44, 50, 71)
#define C_GREY    RGB565(124, 134, 162)
#define C_WHITE   RGB565(234, 246, 255)
#define C_CYAN    RGB565(0, 240, 255)     /* #00F0FF Cyber Cyan: lo activo, las ondas, el cursor */
#define C_ORANGE  RGB565(255, 138, 0)     /* #FF8A00 Neon Amber: valores, filtro, el paso que suena */
#define C_RED     RGB565(255, 0, 85)      /* #FF0055 Laser Red: REC, saturación, mute */
#define C_GREEN   RGB565(112, 255, 152)   /* #70FF98 Matrix Mint: notas largas, TIE */
#define C_YELLOW  RGB565(255, 210, 63)
#define C_PINK    RGB565(255, 43, 214)    /* magenta */
#define C_VIOLET  RGB565(157, 123, 255)
#define C_BLUE    RGB565(61, 123, 255)
#define C_ACID    RGB565(182, 255, 0)     /* verde ácido */

typedef struct {
    uint16_t *buf;          /* BM_LCD_W x BM_STRIP_H */
    int y0;                 /* primera fila de la franja */
    int cx0, cy0, cx1, cy1; /* recorte activo (coordenadas de pantalla, exclusivo) */
} bm_gfx;

void gfx_clip(bm_gfx *g, int x, int y, int w, int h);
void gfx_noclip(bm_gfx *g);
void gfx_fill(bm_gfx *g, int x, int y, int w, int h, uint16_t c);
void gfx_rect(bm_gfx *g, int x, int y, int w, int h, uint16_t c);
void gfx_hline(bm_gfx *g, int x, int y, int w, uint16_t c);
void gfx_vline(bm_gfx *g, int x, int y, int h, uint16_t c);
void gfx_line(bm_gfx *g, int x0, int y0, int x1, int y1, uint16_t c);
void gfx_disc(bm_gfx *g, int cx, int cy, int r, uint16_t c);
int  gfx_text(bm_gfx *g, int x, int y, const char *s, uint16_t c, int scale);   /* 5x7 */
int  gfx_text_w(const char *s, int scale);
int  gfx_text_r(bm_gfx *g, int xr, int y, const char *s, uint16_t c, int scale); /* alineado a la derecha */
int  gfx_text_c(bm_gfx *g, int xc, int y, const char *s, uint16_t c, int scale); /* centrado */
int  gfx_small(bm_gfx *g, int x, int y, const char *s, uint16_t c);              /* 3x5 */
int  gfx_small_w(const char *s);
void gfx_logo(bm_gfx *g);
bool gfx_strip_hit(const bm_gfx *g, int y, int h);   /* ¿el rango toca la franja? */

extern const uint8_t bm_font5x7[96][7];
extern const uint8_t bm_font3x5[96][5];
extern const uint16_t bm_logo_pal[48];
extern const uint16_t bm_logo_row[240];
extern const uint8_t bm_logo_rle[];

/* Render de la pantalla completa por tiles con detección de cambios. */
typedef void (*bm_draw_fn)(bm_gfx *g, void *user);
typedef void (*bm_blit_fn)(void *user, int x, int y, int w, int h, const uint16_t *px);

typedef struct {
    uint32_t hash[BM_LCD_H / BM_STRIP_H][BM_LCD_W / BM_TILE_W];
    uint8_t force;
    uint32_t bytes_last;     /* bytes enviados en el último frame (para medir el SPI) */
} bm_screen;

void bm_screen_render(bm_screen *s, bm_draw_fn draw, void *user, bm_blit_fn blit, void *blit_user);

#endif
