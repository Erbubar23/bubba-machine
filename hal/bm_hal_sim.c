/* Bubba Machine — HAL simulada. */
#include <string.h>
#include "bm_hal_sim.h"

static bm_sim *g_sim;

static int f_read(uint32_t addr, void *dst, uint32_t n)
{
    if (addr + n > BM_FLASH_SIZE) return -1;
    memcpy(dst, &g_sim->flash[addr], n);
    return 0;
}

static int f_erase(uint32_t addr)
{
    if ((addr & (BM_SECTOR - 1)) || addr + BM_SECTOR > BM_FLASH_SIZE) return -1;
    /* misma regla que el firmware: nunca fuera de las zonas escribibles (SPEC_V3 §4.4) */
    if (!((addr >= BM_STORE_LO && addr < BM_STORE_HI) || (addr >= BM_GLOB_LO && addr < BM_GLOB_HI))) return -1;
    memset(&g_sim->flash[addr], 0xFF, BM_SECTOR);
    g_sim->flash_writes++;
    return 0;
}

static int f_program(uint32_t addr, const void *src, uint32_t n)
{
    if (n == 0 || n > BM_PAGE || ((addr & (BM_PAGE - 1)) + n) > BM_PAGE || addr + n > BM_FLASH_SIZE) return -1;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) g_sim->flash[addr + i] &= s[i];   /* NOR: solo 1 -> 0 */
    return 0;
}

static const bm_flash_ops OPS = {f_read, f_erase, f_program};

static void p_blit(void *u, int x, int y, int w, int h, const uint16_t *px)
{
    bm_sim *s = (bm_sim *)u;
    for (int r = 0; r < h; r++) memcpy(&s->fb[(y + r) * BM_LCD_W + x], &px[r * w], (size_t)w * 2);
    s->spi_bytes += (uint32_t)(w * h * 2);
}

static void p_led(void *u, int id, uint8_t lvl)
{
    bm_sim *s = (bm_sim *)u;
    if (id >= 0 && id < BM_LEDS) s->leds[id] = lvl;
}

static void p_midi(void *u, uint8_t b)
{
    bm_sim *s = (bm_sim *)u;
    if (s->midi_n < sizeof s->midi_out) s->midi_out[s->midi_n++] = b;
}

static const uint8_t *p_map(void *u, uint32_t addr)
{
    bm_sim *s = (bm_sim *)u;
    return addr < BM_FLASH_SIZE ? &s->flash[addr] : NULL;
}

void bm_sim_flash_erase_all(bm_sim *s) { memset(s->flash, 0xFF, sizeof s->flash); }

void bm_sim_platform(bm_sim *s, bm_platform *pf)
{
    g_sim = s;
    memset(pf, 0, sizeof *pf);
    pf->blit = p_blit; pf->led = p_led; pf->midi_out = p_midi; pf->flash_map = p_map;
    pf->flash = &OPS; pf->user = s;
}
