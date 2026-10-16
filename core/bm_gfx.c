/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — primitivas gráficas y render por tiles. */
#include <string.h>
#include "bm_gfx.h"
#include "bm_lang.h"

static uint16_t strip_buf[BM_LCD_W * BM_STRIP_H];
static uint16_t tile_buf[BM_TILE_W * BM_STRIP_H];

void gfx_noclip(bm_gfx *g)
{
    g->cx0 = 0; g->cx1 = BM_LCD_W;
    g->cy0 = g->y0; g->cy1 = g->y0 + BM_STRIP_H;
}

void gfx_clip(bm_gfx *g, int x, int y, int w, int h)
{
    gfx_noclip(g);
    if (x > g->cx0) g->cx0 = x;
    if (y > g->cy0) g->cy0 = y;
    if (x + w < g->cx1) g->cx1 = x + w;
    if (y + h < g->cy1) g->cy1 = y + h;
}

bool gfx_strip_hit(const bm_gfx *g, int y, int h)
{
    return y < g->y0 + BM_STRIP_H && y + h > g->y0;
}

static inline void px(bm_gfx *g, int x, int y, uint16_t c)
{
    if (x >= g->cx0 && x < g->cx1 && y >= g->cy0 && y < g->cy1)
        g->buf[(y - g->y0) * BM_LCD_W + x] = c;
}

void gfx_fill(bm_gfx *g, int x, int y, int w, int h, uint16_t c)
{
    int x0 = x < g->cx0 ? g->cx0 : x, x1 = x + w > g->cx1 ? g->cx1 : x + w;
    int y0 = y < g->cy0 ? g->cy0 : y, y1 = y + h > g->cy1 ? g->cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *row = &g->buf[(yy - g->y0) * BM_LCD_W];
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
}

void gfx_hline(bm_gfx *g, int x, int y, int w, uint16_t c) { gfx_fill(g, x, y, w, 1, c); }
void gfx_vline(bm_gfx *g, int x, int y, int h, uint16_t c) { gfx_fill(g, x, y, 1, h, c); }

void gfx_rect(bm_gfx *g, int x, int y, int w, int h, uint16_t c)
{
    gfx_hline(g, x, y, w, c); gfx_hline(g, x, y + h - 1, w, c);
    gfx_vline(g, x, y, h, c); gfx_vline(g, x + w - 1, y, h, c);
}

void gfx_line(bm_gfx *g, int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int e = dx + dy;
    for (;;) {
        px(g, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * e;
        if (e2 >= dy) { e += dy; x0 += sx; }
        if (e2 <= dx) { e += dx; y0 += sy; }
    }
}

void gfx_disc(bm_gfx *g, int cx, int cy, int r, uint16_t c)
{
    for (int y = -r; y <= r; y++) {
        int yy = cy + y;
        if (yy < g->cy0 || yy >= g->cy1) continue;
        int w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r) w++;
        gfx_hline(g, cx - w, yy, 2 * w + 1, c);
    }
}

static int glyph(int ch) { return (ch < 32 || ch > 127) ? 0 : ch - 32; }

/* Los textos se traducen al dibujarlos (bm_lang.h): una frase en inglés que tiene traducción en
 * el paquete activo se ve traducida, también la que viene de una tabla o de un búfer. */
static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

int gfx_text_w(const char *s, int scale) { return text_w(bm_txt(s), scale); }

int gfx_text(bm_gfx *g, int x, int y, const char *s, uint16_t c, int scale)
{
    int x0 = x;
    s = bm_txt(s);
    if (y >= g->cy1 || y + 7 * scale <= g->cy0) return text_w(s, scale);
    for (; *s; s++) {
        const uint8_t *gl = bm_font5x7[glyph((unsigned char)*s)];
        for (int r = 0; r < 7; r++) {
            uint8_t bits = gl[r];
            if (!bits) continue;
            for (int col = 0; col < 5; col++)
                if (bits & (0x10 >> col)) {
                    if (scale == 1) px(g, x + col, y + r, c);
                    else gfx_fill(g, x + col * scale, y + r * scale, scale, scale, c);
                }
        }
        x += 6 * scale;
    }
    return x - x0 - scale;
}

int gfx_text_r(bm_gfx *g, int xr, int y, const char *s, uint16_t c, int scale)
{
    return gfx_text(g, xr - gfx_text_w(s, scale), y, s, c, scale);
}

int gfx_text_c(bm_gfx *g, int xc, int y, const char *s, uint16_t c, int scale)
{
    return gfx_text(g, xc - gfx_text_w(s, scale) / 2, y, s, c, scale);
}

int gfx_small_w(const char *s)
{
    int n = 0;
    for (s = bm_txt(s); *s; s++) n += *s == '#' ? 5 : 4;   /* el sostenido es más ancho */
    return n ? n - 1 : 0;
}

int gfx_small(bm_gfx *g, int x, int y, const char *s, uint16_t c)
{
    int x0 = x;
    for (s = bm_txt(s); *s; s++) {
        if (*s == '#') {
            for (int r = 0; r < 5; r++)
                for (int col = 0; col < 4; col++)
                    if (bm_sharp4x5[r] & (0x8 >> col)) px(g, x + col, y + r, c);
            x += 5;
            continue;
        }
        const uint8_t *gl = bm_font3x5[glyph((unsigned char)*s)];
        for (int r = 0; r < 5; r++)
            for (int col = 0; col < 3; col++)
                if (gl[r] & (0x4 >> col)) px(g, x + col, y + r, c);
        x += 4;
    }
    return x - x0 - 1;
}

void gfx_logo(bm_gfx *g)
{
    for (int y = g->cy0; y < g->cy1; y++) {
        const uint8_t *p = &bm_logo_rle[bm_logo_row[y]];
        int x = 0;
        while (x < BM_LCD_W) {
            int n = p[0];
            uint16_t c = bm_logo_pal[p[1]];
            gfx_hline(g, x, y, n, c);
            x += n; p += 2;
        }
    }
}

static uint32_t fnv(const uint16_t *p, int n)
{
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

void bm_screen_render(bm_screen *s, bm_draw_fn draw, void *user, bm_blit_fn blit, void *blit_user)
{
    bm_gfx g;
    g.buf = strip_buf;
    s->bytes_last = 0;
    for (int st = 0; st < BM_LCD_H / BM_STRIP_H; st++) {
        g.y0 = st * BM_STRIP_H;
        gfx_noclip(&g);
        for (int i = 0; i < BM_LCD_W * BM_STRIP_H; i++) strip_buf[i] = C_BLACK;
        draw(&g, user);
        for (int tx = 0; tx < BM_LCD_W / BM_TILE_W; tx++) {
            for (int r = 0; r < BM_STRIP_H; r++)
                memcpy(&tile_buf[r * BM_TILE_W], &strip_buf[r * BM_LCD_W + tx * BM_TILE_W], BM_TILE_W * 2);
            uint32_t h = fnv(tile_buf, BM_TILE_W * BM_STRIP_H);
            if (h != s->hash[st][tx] || s->force) {
                s->hash[st][tx] = h;
                blit(blit_user, tx * BM_TILE_W, g.y0, BM_TILE_W, BM_STRIP_H, tile_buf);
                s->bytes_last += BM_TILE_W * BM_STRIP_H * 2;
            }
        }
    }
    s->force = 0;
}
