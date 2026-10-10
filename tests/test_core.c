/* Bubba Machine — pruebas del núcleo en PC.
 * Uso: test_core [carpeta_salida]  -> escribe demo.wav y capturas *.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_util.h"
#include "../core/bm_rom.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static bm_sim sim;
static bm_app app;
static uint32_t now_ms;
static const char *outdir = ".";

/* ------------------------------------------------------------ utilidades */
static int32_t abuf[256 * 2];
static double peak_abs;
static FILE *wav;
static uint32_t wav_frames;

static void wav_open(const char *name)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", outdir, name);
    wav = fopen(p, "wb");
    if (!wav) return;
    uint8_t h[44] = {0};
    fwrite(h, 1, 44, wav);
    wav_frames = 0;
}

static void put32le(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }

static void wav_close(void)
{
    if (!wav) return;
    uint8_t h[44];
    memcpy(h, "RIFF", 4); put32le(h + 4, 36 + wav_frames * 4); memcpy(h + 8, "WAVEfmt ", 8);
    put32le(h + 16, 16); h[20] = 1; h[21] = 0; h[22] = 2; h[23] = 0;
    put32le(h + 24, 44118); put32le(h + 28, 44118 * 4); h[32] = 4; h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4); put32le(h + 40, wav_frames * 4);
    fseek(wav, 0, SEEK_SET); fwrite(h, 1, 44, wav); fclose(wav); wav = NULL;
}

/* Avanza el tiempo: audio en bloques de 128 frames y la UI a ~30 Hz. */
static void run_ms(uint32_t ms)
{
    uint32_t frames = ms * 44118u / 1000u;
    uint32_t next_tick = now_ms;
    while (frames) {
        uint32_t n = frames > 128 ? 128 : frames;
        n &= ~1u; if (!n) break;
        bm_app_audio(&app, abuf, n);
        for (uint32_t i = 0; i < n * 2; i++) {
            double v = abuf[i] / 8388608.0;
            if (v < 0) v = -v;
            if (v > peak_abs) peak_abs = v;
        }
        if (wav) {
            for (uint32_t i = 0; i < n * 2; i++) { int16_t s = (int16_t)(abuf[i] >> 8); fwrite(&s, 2, 1, wav); }
            wav_frames += n;
        }
        frames -= n;
        now_ms += 0;
        static uint32_t acc;
        acc += n;
        while (acc >= 44) { acc -= 44; now_ms++; }
        if ((int32_t)(now_ms - next_tick) >= 0) { bm_app_tick(&app, now_ms); next_tick = now_ms + 33; }
    }
}

static void press_btn(int id, uint32_t hold_ms)
{
    bm_app_button(&app, id, 1, now_ms); run_ms(hold_ms); bm_app_button(&app, id, 0, now_ms); run_ms(40);
}

static void press_key(int k, uint32_t hold_ms)
{
    bm_app_key(&app, k, 1, now_ms); run_ms(hold_ms); bm_app_key(&app, k, 0, now_ms); run_ms(40);
}

static void shot(const char *name)
{
    app.scr.force = 1;
    bm_app_tick(&app, now_ms);
    char p[512];
    snprintf(p, sizeof p, "%s/%s.ppm", outdir, name);
    FILE *f = fopen(p, "wb");
    if (!f) return;
    fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = sim.fb[i];
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void boot(int erase)
{
    if (erase) { bm_sim_flash_erase_all(&sim); sim.flash_writes = 0; }
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    now_ms = 1000;
    bm_app_init(&app, &pf, now_ms);
    bm_app_master(&app, 900); bm_app_master(&app, 900); bm_app_master(&app, 900); bm_app_master(&app, 900);
    run_ms(UI_BOOT_MS + 100);
}

/* ------------------------------------------------------------ pruebas */
static void test_pack(void)
{
    printf("formato disperso\n");
    static bm_pattern p, q;
    bm_pattern_init(&p);
    for (int t = 0; t < BM_TRACKS; t++)
        for (int s = 0; s < BM_STEPS; s += 3) {
            bm_event *e = &p.ev[t][s][0];
            e->note = (uint8_t)(40 + s); e->vel = 99; e->len = 2; e->prob = 70; e->lk[2] = 33;
        }
    p.meta.bpm_x10 = 1337; p.trk[3].macro[0] = 11;
    static uint8_t buf[BM_PENDING_LEN];
    int n = bm_pattern_pack(&p, buf, sizeof buf);
    CHECK(n > 0 && n <= (int)BM_SLOT_BODY);
    CHECK(bm_pattern_unpack(&q, buf, n) == n);
    CHECK(memcmp(&p, &q, sizeof p) == 0);
    /* tope de 768 eventos */
    for (int t = 0; t < BM_TRACKS; t++) for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) p.ev[t][s][k].note = 60;
    CHECK(bm_pattern_pack(&p, buf, sizeof buf) < 0);
}

static void test_store(void)
{
    printf("almacenamiento: commit al final y desgaste\n");
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static bm_store st;
    CHECK(bm_store_mount(&st, pf.flash) == 0);
    uint8_t a[300], b[300];
    for (int i = 0; i < 300; i++) a[i] = (uint8_t)i;
    CHECK(bm_store_write(&st, 3, a, 300) == 0);
    CHECK(bm_store_read(&st, 3, b, sizeof b) == 300 && memcmp(a, b, 300) == 0);
    int used_slot = st.live[3];
    /* corte de energía: el cuerpo nuevo se programa pero la cabecera no llega */
    uint32_t slot = (uint32_t)(used_slot + 1) % BM_STORE_SLOTS;
    uint32_t addr = BM_STORE_LO + slot * BM_SLOT_SIZE;
    pf.flash->erase(addr); pf.flash->erase(addr + BM_SECTOR);
    uint8_t junk[200]; memset(junk, 0x55, sizeof junk);
    pf.flash->program(addr + 16, junk, 200);
    static bm_store st2;
    CHECK(bm_store_mount(&st2, pf.flash) == 0);
    CHECK(st2.live[3] == used_slot);
    CHECK(bm_store_read(&st2, 3, b, sizeof b) == 300 && memcmp(a, b, 300) == 0);
    /* desgaste: 200 escrituras del mismo objeto recorren todos los slots */
    int seen[64] = {0}, distinct = 0;
    for (int i = 0; i < 200; i++) {
        a[0] = (uint8_t)i;
        CHECK(bm_store_write(&st2, 3, a, 300) == 0);
        if (!seen[st2.live[3]]++) distinct++;
    }
    CHECK(distinct == (int)BM_STORE_SLOTS);
    static bm_store st3;
    bm_store_mount(&st3, pf.flash);
    CHECK(bm_store_read(&st3, 3, b, sizeof b) == 300 && b[0] == 199);
    /* nunca escribe fuera de las zonas permitidas */
    CHECK(pf.flash->erase(0x000000) != 0);
    CHECK(pf.flash->erase(0x0E8000) != 0);
}

typedef struct { int log[64]; int n; } glog;
static void gcb(void *u, int id, bm_gkind k) { glog *g = (glog *)u; if (g->n < 64) g->log[g->n++] = id * 10 + (int)k; }

static void test_gesture(void)
{
    printf("gestos: tap, hold, combo\n");
    bm_gesture g; glog L = {{0}, 0};
    bm_gesture_init(&g, gcb, &L);
    bm_gesture_down(&g, BTN_SCL, 0); bm_gesture_poll(&g, 100); bm_gesture_up(&g, BTN_SCL, 150);
    CHECK(L.n == 1 && L.log[0] == BTN_SCL * 10 + G_TAP);
    L.n = 0;
    bm_gesture_down(&g, BTN_SCL, 1000); bm_gesture_poll(&g, 1200);
    CHECK(L.n == 0);
    bm_gesture_poll(&g, 1300);
    CHECK(L.n == 1 && L.log[0] == BTN_SCL * 10 + G_HOLD);       /* HOLD al vencer, no al soltar */
    bm_gesture_up(&g, BTN_SCL, 2000);
    CHECK(L.n == 2 && L.log[1] == BTN_SCL * 10 + G_RELEASE);
    L.n = 0;
    bm_gesture_down(&g, BTN_FX, 3000); bm_gesture_other(&g, 3050);  /* combo antes de 300 ms */
    CHECK(L.n == 1 && L.log[0] == BTN_FX * 10 + G_HOLD);
    bm_gesture_up(&g, BTN_FX, 3100);
    CHECK(L.n == 2 && L.log[1] == BTN_FX * 10 + G_RELEASE);
    L.n = 0;
    bm_gesture_down(&g, BTN_EDIT, 4000); bm_gesture_other(&g, 4010); bm_gesture_up(&g, BTN_EDIT, 4020);
    CHECK(L.n == 0);                                              /* sin capa: el combo cancela el TAP */
    L.n = 0;
    bm_gesture_down(&g, BTN_PLAY, 5000);
    CHECK(L.n == 1 && L.log[0] == BTN_PLAY * 10 + G_PRESS);       /* instantáneo */
    bm_gesture_up(&g, BTN_PLAY, 5010);
    L.n = 0;
    bm_gesture_down(&g, BTN_SAVE, 6000); bm_gesture_poll(&g, 6500); CHECK(L.n == 0);
    bm_gesture_poll(&g, 7001); CHECK(L.n == 1 && L.log[0] == BTN_SAVE * 10 + G_HOLD);   /* 1 s */
    bm_gesture_up(&g, BTN_SAVE, 7100);
}

static void test_timing_and_rng(void)
{
    printf("secuenciador: tiempo exacto y probabilidad determinista\n");
    boot(1);
    /* 120 BPM: un paso = 22059*60/(120*4) = 2757.4 muestras de motor */
    bm_cmd c; memset(&c, 0, sizeof c);
    c.type = CMD_SET_BPM; c.u.i = 1200; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_SET_META; c.b = (uint8_t)offsetof(bm_meta, swing); c.c = 50; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);
    bm_app_audio(&app, abuf, 2);
    uint32_t start = app.ctx.sample_clock;
    int steps = 0, last = app.ctx.st.step;
    uint32_t at[8];
    while (steps < 8) {
        bm_app_audio(&app, abuf, 2);
        if (app.ctx.st.step != last) { at[steps++] = app.ctx.sample_clock - start; last = app.ctx.st.step; }
    }
    for (int i = 1; i < 8; i++) {
        double d = (double)(at[i] - at[i - 1]);
        CHECK(d > 2757.4 - 2 && d < 2757.4 + 2);
    }
    /* dos corridas con la misma semilla disparan lo mismo */
    uint32_t r1 = 0xB0BBAu, r2 = 0xB0BBAu;
    int same = 1;
    for (int i = 0; i < 1000; i++) if (bm_xorshift32(&r1) != bm_xorshift32(&r2)) same = 0;
    CHECK(same);
}

static void test_voices(void)
{
    printf("voces: pool de 7 con robo y fade\n");
    boot(1);
    bm_cmd c; memset(&c, 0, sizeof c);
    for (int i = 0; i < 20; i++) { c.type = CMD_NOTE_ON; c.a = 3; c.b = (uint8_t)(48 + i); c.c = 100; bm_cmd_push(&app.cmdq, &c); }
    run_ms(50);
    int pool = 0;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N; i++) if (app.ctx.voice[i].active) pool++;
    CHECK(pool <= BM_V_POOL_N && pool >= 1);
    CHECK(!app.ctx.voice[BM_V_BASS].active || app.ctx.voice[BM_V_BASS].track == 2);
    for (int i = 0; i < BM_V_DRUM_N; i++) CHECK(!app.ctx.voice[i].active || app.ctx.voice[i].track == 0);
    c.type = CMD_ALL_OFF; bm_cmd_push(&app.cmdq, &c);
    run_ms(3000);
}

static void test_demo_and_persist(void)
{
    printf("demo: audio, autoguardado y recarga\n");
    boot(1);
    peak_abs = 0;
    wav_open("demo.wav");
    press_btn(BTN_PLAY, 20);
    run_ms(17000);
    wav_close();
    CHECK(peak_abs > 0.05);
    CHECK(peak_abs <= 1.0);
    uint32_t w0 = sim.flash_writes;
    CHECK(w0 == 0);                            /* nunca escribe flash sonando */
    press_btn(BTN_PLAY, 20);
    run_ms(UI_AUTOSAVE_MS + 1500);
    CHECK(sim.flash_writes > w0);
    CHECK(!app.dirty);
    static bm_pattern saved;
    saved = app.pat;
    boot(0);                                   /* reinicio con la misma flash */
    CHECK(memcmp(&saved.ev, &app.pat.ev, sizeof saved.ev) == 0);
    for (int t = 0; t < BM_TRACKS; t++) for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++)
        if (memcmp(&saved.ev[t][s][e], &app.pat.ev[t][s][e], 8)) {
            const uint8_t *x = (const uint8_t *)&saved.ev[t][s][e], *y = (const uint8_t *)&app.pat.ev[t][s][e];
            printf("  difiere t%d s%d e%d: %02x%02x%02x%02x %02x%02x%02x%02x vs %02x%02x%02x%02x %02x%02x%02x%02x\n", t, s, e,
                   x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7]);
            t = BM_TRACKS; s = BM_STEPS; break;
        }
    CHECK(saved.meta.bpm_x10 == app.pat.meta.bpm_x10);
}

static void test_ui_flow(void)
{
    printf("interfaz: recorrido de pantallas (capturas)\n");
    boot(1);
    shot("00-home");
    press_btn(BTN_PLAY, 20);
    run_ms(900);
    shot("01-home-play");
    press_btn(BTN_SEQ, 30);
    run_ms(300);
    shot("02-seq");
    /* mantener un paso + girar KNOB 1 = P-Lock */
    int k = bm_white_key(4);
    bm_app_key(&app, k, 1, now_ms); run_ms(400);
    bm_app_encoder(&app, ENC_K2, 5, now_ms); run_ms(60);
    shot("03-seq-plock");
    CHECK(app.pat.ev[0][4][0].lk[1] != BM_LOCK_NONE || app.pat.ev[0][4][1].lk[1] != BM_LOCK_NONE);
    bm_app_key(&app, k, 0, now_ms); run_ms(60);
    /* tocar un paso vacío lo crea; tocarlo otra vez lo borra */
    int k2 = bm_white_key(1);
    press_key(k2, 60);
    int idx_has = 0;
    for (int e = 0; e < 4; e++) if (!bm_event_empty(&app.pat.ev[0][1][e]) && app.pat.ev[0][1][e].note == app.drum_sel) idx_has = 1;
    CHECK(idx_has);
    press_key(k2, 60);
    idx_has = 0;
    for (int e = 0; e < 4; e++) if (!bm_event_empty(&app.pat.ev[0][1][e]) && app.pat.ev[0][1][e].note == app.drum_sel) idx_has = 1;
    CHECK(!idx_has);
    /* ALGORITHM cambia de pista */
    bm_app_encoder(&app, ENC_ALGO, 1, now_ms); bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(100);
    CHECK(app.track == 2);
    shot("04-seq-bass");
    bm_app_encoder(&app, ENC_K1, -6, now_ms); run_ms(100);
    shot("05-overlay");
    run_ms(1700);
    press_btn(BTN_EDIT, 30); bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(200);
    shot("06-edit-wave");
    for (int i = 0; i < 3; i++) bm_app_encoder(&app, ENC_ALGO, -1, now_ms);
    run_ms(200);
    shot("07-edit-kit");
    bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(100);
    bm_app_button(&app, BTN_REC, 1, now_ms); run_ms(250);
    press_key(bm_white_key(0), 30); run_ms(150); press_key(bm_white_key(1), 30); run_ms(150); press_key(bm_white_key(2), 30);
    run_ms(150);
    shot("08-sampler-rec");
    bm_app_button(&app, BTN_REC, 0, now_ms); run_ms(200);
    CHECK(app.ctx.smp_len > 0 && app.ctx.smp_marks == 3);
    shot("09-sampler");
    bm_app_encoder(&app, ENC_ALGO, 2, now_ms); run_ms(100);
    press_btn(BTN_ENV, 30); run_ms(100);
    shot("10-env");
    press_btn(BTN_FX, 30); run_ms(100);
    shot("11-fx");
    press_btn(BTN_SCL, 30); run_ms(100);
    bm_app_key(&app, bm_black_key(3), 1, now_ms); run_ms(100);
    shot("12-scl-chord");
    bm_app_key(&app, bm_black_key(3), 0, now_ms); run_ms(100);
    /* capas: mantener */
    bm_app_button(&app, BTN_FX, 1, now_ms); run_ms(400);
    bm_app_key(&app, bm_white_key(2), 1, now_ms); run_ms(400);
    shot("13-punch");
    CHECK(app.ctx.st.punch == 2);
    bm_app_key(&app, bm_white_key(2), 0, now_ms); bm_app_button(&app, BTN_FX, 0, now_ms); run_ms(200);
    CHECK(app.ctx.st.punch == 0xFF);
    bm_app_button(&app, BTN_SEQ, 1, now_ms); run_ms(400);
    press_key(bm_white_key(1), 40);
    shot("14-bank");
    bm_app_button(&app, BTN_SEQ, 0, now_ms); run_ms(100);
    bm_app_button(&app, BTN_HOME, 1, now_ms); run_ms(400);
    press_key(bm_white_key(4), 40);
    shot("15-mute");
    bm_app_button(&app, BTN_HOME, 0, now_ms); run_ms(100);
    CHECK(app.pat.trk[4].mute == 1);
    press_btn(BTN_ARP, 30); run_ms(100);
    shot("16-arp");
    press_btn(BTN_GLO, 30); run_ms(100);
    shot("17-glo");
    press_btn(BTN_SAVE, 30); run_ms(100);
    shot("18-save");
    bm_app_button(&app, BTN_GLO, 1, now_ms); press_btn(BTN_SEQ, 30); bm_app_button(&app, BTN_GLO, 0, now_ms);
    press_key(bm_white_key(0), 30); press_key(bm_white_key(1), 30); press_key(bm_white_key(0), 30);
    shot("19-song");
    press_btn(BTN_LFO, 30);
    shot("20-lfo");
    /* pantalla de arranque */
    bm_platform pf; bm_sim_platform(&sim, &pf);
    now_ms += 10; bm_app_init(&app, &pf, now_ms); run_ms(300);
    shot("21-boot");
}

static void test_spi_budget(void)
{
    printf("pantalla: bytes por frame en SEQ sonando\n");
    boot(1);
    press_btn(BTN_SEQ, 30);
    press_btn(BTN_PLAY, 20);
    run_ms(500);
    uint32_t maxb = 0, sum = 0;
    for (int i = 0; i < 60; i++) {
        sim.spi_bytes = 0;
        run_ms(33);
        if (sim.spi_bytes > maxb) maxb = sim.spi_bytes;
        sum += sim.spi_bytes;
    }
    printf("  promedio %u B/frame, máximo %u B (presupuesto ~37 KB a 12 MHz)\n", sum / 60, maxb);
    CHECK(sum / 60 < 37000);
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];
    bm_tables_init();
    test_pack();
    test_store();
    test_gesture();
    test_timing_and_rng();
    test_voices();
    test_demo_and_persist();
    test_spi_budget();
    test_ui_flow();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
