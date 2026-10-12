/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — emulador web (Emscripten). La interfaz JS está en web/emu.js. */
#include <emscripten/emscripten.h>
#include <string.h>
#include "../bm_hal_sim.h"
#include "../../core/bm_takes.h"
#include "../../core/bm_app_int.h"

static bm_sim sim;
static bm_app app;
static int32_t ibuf[4096 * 2];
static float fbuf[4096 * 2];
static int ready;
static uint8_t lang_buf[BM_LANG_HI - BM_LANG_LO];

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

/* Tomas: el JS escribe aquí un audio ya convertido a int8 a 16 kHz y lo guarda en la biblioteca
 * de tomas, igual que lo haría el editor web por USB en el equipo. */
static int8_t smp_buf[BM_SMP_REC_MAX];
EMSCRIPTEN_KEEPALIVE int8_t *bm_web_sampler(void) { return smp_buf; }
EMSCRIPTEN_KEEPALIVE int bm_web_sampler_cap(void) { return (int)bm_app_rec_room(&app); }
EMSCRIPTEN_KEEPALIVE int bm_web_bpm(void) { return app.pat.meta.bpm_x10; }
EMSCRIPTEN_KEEPALIVE int bm_web_takes_free(void) { return (int)bm_takes_free(app.take); }      /* bytes = muestras */
EMSCRIPTEN_KEEPALIVE int bm_web_takes_total(void) { return (int)(BM_TAKE_HI - BM_TAKE_LO); }
EMSCRIPTEN_KEEPALIVE int bm_web_sampler_set(int len, int bpm_x10)
{
    if (len < 1 || (uint32_t)len > bm_app_rec_room(&app)) return -1;
    if (bpm_x10 < 400 || bpm_x10 > 2400) bpm_x10 = app.pat.meta.bpm_x10;
    uint32_t bar = (uint32_t)BM_SMP_RATE * 2400u / (uint32_t)bpm_x10;
    uint32_t bars = ((uint32_t)len + bar / 2) / bar;
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    int id = bm_takes_write(pf.flash, pf.flash_map, pf.user, app.take, smp_buf, (uint32_t)len, (uint16_t)bpm_x10, (uint8_t)(bars < 1 ? 1 : bars > 8 ? 8 : bars));
    if (id < 0) return -1;
    bm_app_takes_reload(&app);
    if (app_kit_empty(&app)) app_kit_from_take(&app, id, 0);   /* con el kit vacío, la toma lo arma */
    return id;
}

EMSCRIPTEN_KEEPALIVE int bm_web_midi_out(uint8_t *dst, int cap)
{
    int n = sim.midi_n < cap ? sim.midi_n : cap;
    memcpy(dst, sim.midi_out, (size_t)n);
    sim.midi_n = 0;
    return n;
}

/* Idioma de la pantalla: el JS deja un paquete .bmlang en lang_buf y lo carga (n = 0: lo quita). */
EMSCRIPTEN_KEEPALIVE uint8_t *bm_web_lang_buf(void) { return lang_buf; }
EMSCRIPTEN_KEEPALIVE int bm_web_lang_load(int n)
{
    if (!ready) return -1;
    if (n <= 0) return bm_app_lang_load(&app, 3, 0, NULL, 0);
    if ((uint32_t)n > sizeof lang_buf) return -1;
    if (bm_app_lang_load(&app, 0, 0, NULL, 0) || bm_app_lang_load(&app, 1, 0, lang_buf, (uint32_t)n)) return -1;
    return bm_app_lang_load(&app, 2, 0, NULL, 0);
}
EMSCRIPTEN_KEEPALIVE int bm_web_lang_on(void) { return ready && app.set.lang && bm_app_lang_pack(&app) != NULL; }

EMSCRIPTEN_KEEPALIVE int bm_web_view(void) { return app.view; }
EMSCRIPTEN_KEEPALIVE int bm_web_playing(void) { return app.ctx.st.playing; }
