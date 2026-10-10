/* Bubba Machine — emulador web (Emscripten). La interfaz JS está en web/emu.js. */
#include <emscripten/emscripten.h>
#include <string.h>
#include "../bm_hal_sim.h"

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

/* Sampler: el JS escribe aquí un WAV ya convertido a int8 a 22.05 kHz (solo emulador). */
EMSCRIPTEN_KEEPALIVE int8_t *bm_web_sampler(void) { return app.amem.smp; }
EMSCRIPTEN_KEEPALIVE int bm_web_sampler_cap(void) { return BM_SAMPLER_LEN; }
EMSCRIPTEN_KEEPALIVE void bm_web_sampler_set(int len)
{
    if (len < 0) len = 0;
    if (len > (int)BM_SAMPLER_LEN) len = BM_SAMPLER_LEN;
    app.ctx.smp_len = (uint32_t)len;
    app.ctx.smp_marks = 0;
    app.ctx.st.smp_marks = 0;
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
