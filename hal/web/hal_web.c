/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — emulador web (Emscripten). La interfaz JS está en web/emu.js. */
#include <emscripten/emscripten.h>
#include <string.h>
#include "../bm_hal_sim.h"
#include "../../core/bm_sample.h"

static bm_sim sim;
static bm_app app;
static int32_t ibuf[4096 * 2];
static float fbuf[4096 * 2];
static int ready;

EMSCRIPTEN_KEEPALIVE uint8_t *bm_web_flash(void) { return sim.flash; }
EMSCRIPTEN_KEEPALIVE int bm_web_flash_size(void) { return BM_FLASH_SIZE; }
EMSCRIPTEN_KEEPALIVE void bm_web_flash_blank(void) { bm_sim_flash_erase_all(&sim); }
EMSCRIPTEN_KEEPALIVE uint32_t bm_web_flash_writes(void) { return sim.flash_writes; }

EMSCRIPTEN_KEEPALIVE void bm_web_init(uint32_t now)
{
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    bm_app_init(&app, &pf, now);
    ready = 1;
}

EMSCRIPTEN_KEEPALIVE void bm_web_button(int id, int down, uint32_t now) { if (ready) bm_app_button(&app, id, down, now); }
EMSCRIPTEN_KEEPALIVE void bm_web_key(int k, int down, uint32_t now) { if (ready) bm_app_key(&app, k, down, now); }
EMSCRIPTEN_KEEPALIVE void bm_web_encoder(int id, int d, uint32_t now) { if (ready) bm_app_encoder(&app, id, d, now); }
EMSCRIPTEN_KEEPALIVE void bm_web_master(int v) { if (ready) bm_app_master(&app, (uint16_t)v); }
EMSCRIPTEN_KEEPALIVE void bm_web_midi(int b) { if (ready) bm_app_midi_in(&app, (uint8_t)b); }

EMSCRIPTEN_KEEPALIVE uint32_t bm_web_tick(uint32_t now)
{
    if (!ready) return 0;
    sim.spi_bytes = 0;
    bm_app_tick(&app, now);
    return sim.spi_bytes;
}

/* Devuelve frames estéreo planos [L..., R...] en float a 44.1 kHz. */
EMSCRIPTEN_KEEPALIVE float *bm_web_render(int frames)
{
    if (frames > 4096) frames = 4096;
    frames &= ~1;
    if (!ready) { memset(fbuf, 0, sizeof fbuf); return fbuf; }
    bm_app_audio(&app, ibuf, (uint32_t)frames);
    for (int i = 0; i < frames; i++) {
        fbuf[i] = (float)ibuf[2 * i] / 8388608.0f;
        fbuf[frames + i] = (float)ibuf[2 * i + 1] / 8388608.0f;
    }
    return fbuf;
}

EMSCRIPTEN_KEEPALIVE uint16_t *bm_web_fb(void) { return sim.fb; }
EMSCRIPTEN_KEEPALIVE uint8_t *bm_web_leds(void) { return sim.leds; }

/* Sampler: el JS escribe aquí un audio ya convertido a int8 a 16 kHz y lo guarda en un slot
 * de flash, igual que lo haría el editor web por USB en el equipo. */
static int8_t smp_buf[BM_SMP_MAX];
EMSCRIPTEN_KEEPALIVE int8_t *bm_web_sampler(void) { return smp_buf; }
EMSCRIPTEN_KEEPALIVE int bm_web_sampler_cap(void) { return BM_SMP_MAX; }
EMSCRIPTEN_KEEPALIVE int bm_web_sampler_set(int len, int slot)
{
    if (len < 1 || len > (int)BM_SMP_MAX || slot < 0 || slot >= BM_SMP_SLOTS) return -1;
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    uint32_t marks[BM_SLICES];
    for (int i = 0; i < BM_SLICES; i++) marks[i] = (uint32_t)len * (uint32_t)i / BM_SLICES;
    if (bm_sample_write(pf.flash, slot, smp_buf, (uint32_t)len, marks, BM_SLICES)) return -1;
    bm_app_samples_reload(&app);
    return 0;
}

EMSCRIPTEN_KEEPALIVE int bm_web_midi_out(uint8_t *dst, int cap)
{
    int n = sim.midi_n < cap ? sim.midi_n : cap;
    memcpy(dst, sim.midi_out, (size_t)n);
    sim.midi_n = 0;
    return n;
}

EMSCRIPTEN_KEEPALIVE int bm_web_view(void) { return bm_app_eff_view(&app); }
EMSCRIPTEN_KEEPALIVE int bm_web_playing(void) { return app.ctx.st.playing; }
