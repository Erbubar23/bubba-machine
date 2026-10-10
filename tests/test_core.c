/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pruebas del núcleo en PC.
 * Uso: test_core [carpeta_salida]  -> escribe demo.wav y capturas *.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_util.h"
#include "../core/bm_rom.h"
#include "../core/bm_sample.h"

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

static void put32le(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }

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
static uint32_t acc, next_tick;

static void run_ms(uint32_t ms)
{
    uint32_t frames = ms * 44118u / 1000u;
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
        acc += n;
        while (acc >= 44) { acc -= 44; now_ms++; }
        if ((int32_t)(now_ms - next_tick) >= 0) { bm_app_tick(&app, now_ms); next_tick = now_ms + 33; }
    }
}

static void btn(int id, uint32_t hold_ms) { bm_app_button(&app, id, 1, now_ms); run_ms(hold_ms); bm_app_button(&app, id, 0, now_ms); run_ms(40); }
static void key(int k, uint32_t hold_ms) { bm_app_key(&app, k, 1, now_ms); run_ms(hold_ms); bm_app_key(&app, k, 0, now_ms); run_ms(40); }
static void white(int i, uint32_t hold_ms) { key(bm_white_key(i), hold_ms); }
static void enc(int id, int d) { bm_app_encoder(&app, id, d, now_ms); run_ms(40); }

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
    now_ms = 1000; next_tick = now_ms; acc = 0;
    bm_app_init(&app, &pf, now_ms);
    for (int i = 0; i < 4; i++) bm_app_master(&app, 900);
    run_ms(UI_BOOT_MS + 100);
}

static void goto_view(int btn_id, int view)
{
    for (int i = 0; i < 4 && app.view != view; i++) btn(btn_id, 30);
}

/* ------------------------------------------------------------ pruebas */
static void test_pack(void)
{
    printf("formato disperso (TR, synths, loopera)\n");
    static bm_pattern p, q;
    static uint8_t b1[BM_PENDING_LEN], b2[BM_PENDING_LEN];
    bm_pattern_init(&p);
    for (int s = 0; s < 64; s += 3) p.tr[0].st[s % 16][s] = ST_ON | ST_ACC | (2 << ST_RAT_SH);
    p.tr[1].lane[5].len = 12; p.tr[0].mute = 0x0104;
    CHECK(bm_tr_lock_set(&p.tr[0], 9, 9, 2, 33));
    CHECK(bm_tr_lock_get(&p.tr[0], 9, 9, 2) == 33);
    CHECK(p.tr[0].st[9][9] & ST_LOCK);
    for (int t = 0; t < 4; t++)
        for (int s = 0; s < 64; s += 5) { bm_event *e = &p.ev[t][s][1]; e->note = (uint8_t)(40 + s); e->vel = 99; e->len = 2; e->prob = 70; e->lk[3] = 7; }
    p.loop[1].on = 1; p.loop[1].bars = 4; p.loop[1].n = 3;
    for (int i = 0; i < 3; i++) { p.loop[1].ev[i].tick = (uint16_t)(i * 100); p.loop[1].ev[i].note = 60; p.loop[1].ev[i].vel = (uint8_t)(i & 1 ? 0 : 90); }
    p.meta.bpm_x10 = 1337; p.trk[3].speed = SPD_X3_4; p.trk[3].length = 12;
    int n = bm_pattern_pack(&p, b1, sizeof b1);
    CHECK(n > 0 && n <= (int)BM_SLOT_BODY);
    CHECK(bm_pattern_unpack(&q, b1, n) == n);
    CHECK(bm_pattern_pack(&q, b2, sizeof b2) == n && memcmp(b1, b2, (size_t)n) == 0);
    CHECK(memcmp(&p.ev, &q.ev, sizeof p.ev) == 0);
    CHECK(memcmp(p.tr[0].st, q.tr[0].st, sizeof p.tr[0].st) == 0);
    CHECK(q.trk[3].speed == SPD_X3_4 && q.trk[3].length == 12 && q.tr[1].lane[5].len == 12);
    CHECK(q.loop[1].n == 3 && q.loop[1].ev[2].tick == 200);
    printf("  patrón de ejemplo: %d bytes en flash (máx %u)\n", n, (unsigned)BM_SLOT_BODY);
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
    uint32_t slot = (uint32_t)(used_slot + 1) % BM_STORE_SLOTS;
    uint32_t addr = BM_STORE_LO + slot * BM_SLOT_SIZE;
    pf.flash->erase(addr); pf.flash->erase(addr + BM_SECTOR);
    uint8_t junk[200]; memset(junk, 0x55, sizeof junk);
    pf.flash->program(addr + 16, junk, 200);
    static bm_store st2;
    CHECK(bm_store_mount(&st2, pf.flash) == 0);
    CHECK(st2.live[3] == used_slot);
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
    CHECK(pf.flash->erase(0x000000) != 0);
    CHECK(pf.flash->erase(0x0E8000) != 0);
    printf("  %u slots de patrón + 2 slots de sample de 64 KiB\n", (unsigned)BM_STORE_SLOTS);
}

static void test_samples(void)
{
    printf("slots de sample: escritura, CRC y chops\n");
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static int8_t d[20000];
    for (int i = 0; i < 20000; i++) d[i] = (int8_t)((i * 7) & 0x7F);
    uint32_t marks[3] = {0, 5000, 12000};
    CHECK(bm_sample_write(pf.flash, 1, d, 20000, marks, 3) == 0);
    bm_smp_src s;
    CHECK(bm_sample_info(pf.flash, pf.flash_map, pf.user, 1, &s) == 0);
    CHECK(s.len == 20000 && s.nmarks == 3 && s.marks[2] == 12000 && s.data[123] == d[123]);
    CHECK(bm_sample_info(pf.flash, pf.flash_map, pf.user, 0, &s) != 0);
    static uint8_t scratch[BM_SECTOR];
    uint32_t m2[4] = {0, 5000, 10000, 15000};
    CHECK(bm_sample_set_marks(pf.flash, 1, m2, 4, scratch) == 0);
    CHECK(bm_sample_info(pf.flash, pf.flash_map, pf.user, 1, &s) == 0 && s.nmarks == 4 && s.marks[3] == 15000 && s.data[123] == d[123]);
    sim.flash[BM_SMP_LO + BM_SMP_SLOT + BM_SMP_HDR + 500] ^= 0x55;     /* un byte dañado: CRC falla */
    CHECK(bm_sample_info(pf.flash, pf.flash_map, pf.user, 1, &s) != 0);
}

typedef struct { int log[64]; int n; } glog;
static void gcb(void *u, int id, bm_gkind k) { glog *g = (glog *)u; if (g->n < 64) g->log[g->n++] = id * 10 + (int)k; }

static void test_gesture(void)
{
    printf("gestos: toque, mantener, 2 s y combo\n");
    bm_gesture g; glog L = {{0}, 0};
    bm_gesture_init(&g, gcb, &L);
    bm_gesture_down(&g, BTN_SCL, 0); bm_gesture_poll(&g, 100); bm_gesture_up(&g, BTN_SCL, 150);
    CHECK(L.n == 1 && L.log[0] == BTN_SCL * 10 + G_TAP);
    L.n = 0;
    bm_gesture_down(&g, BTN_SEQ, 1000); bm_gesture_poll(&g, 1200); CHECK(L.n == 0);
    bm_gesture_poll(&g, 1300); CHECK(L.n == 1 && L.log[0] == BTN_SEQ * 10 + G_HOLD);   /* HOLD al cumplirse */
    bm_gesture_poll(&g, 2600); CHECK(L.n == 1);
    int id = -1;
    CHECK(bm_gesture_progress(&g, 2650, &id) > 100 && id == BTN_SEQ);                 /* anillo hacia los 2 s */
    bm_gesture_poll(&g, 3001); CHECK(L.n == 2 && L.log[1] == BTN_SEQ * 10 + G_LONG);  /* 2 s: pantalla especial */
    bm_gesture_up(&g, BTN_SEQ, 3100); CHECK(L.n == 2);
    L.n = 0;
    bm_gesture_down(&g, BTN_HOME, 4000); bm_gesture_poll(&g, 4400);
    bm_gesture_other(&g, 4500);                                                       /* tocar mutes */
    bm_gesture_poll(&g, 7000);
    CHECK(L.n == 1 && L.log[0] == BTN_HOME * 10 + G_HOLD);                            /* el combo cancela los 2 s */
    bm_gesture_up(&g, BTN_HOME, 7100); CHECK(L.n == 2 && L.log[1] == BTN_HOME * 10 + G_RELEASE);
    L.n = 0;
    bm_gesture_down(&g, BTN_FX, 8000); bm_gesture_other(&g, 8050);
    CHECK(L.n == 1 && L.log[0] == BTN_FX * 10 + G_HOLD);                              /* combo: HOLD inmediato */
    bm_gesture_up(&g, BTN_FX, 8100);
    L.n = 0;
    bm_gesture_down(&g, BTN_PLAY, 9000);
    CHECK(L.n == 1 && L.log[0] == BTN_PLAY * 10 + G_PRESS);
    bm_gesture_up(&g, BTN_PLAY, 9010);
    L.n = 0;
    bm_gesture_down(&g, BTN_SAVE, 10000); bm_gesture_poll(&g, 11000); CHECK(L.n == 0);
    bm_gesture_poll(&g, 12001); CHECK(L.n == 1 && L.log[0] == BTN_SAVE * 10 + G_LONG);
    bm_gesture_up(&g, BTN_SAVE, 12100); CHECK(L.n == 1);
}

static void test_timing(void)
{
    printf("secuenciador: tiempo exacto, velocidades y polímetro\n");
    boot(1);
    bm_cmd c; memset(&c, 0, sizeof c);
    c.type = CMD_SET_BPM; c.u.i = 1200; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_SET_META; c.b = (uint8_t)offsetof(bm_meta, swing); c.c = 50; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_SET_TRACK; c.a = 2; c.b = (uint8_t)offsetof(bm_track, speed); c.c = SPD_X2; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_SET_TRACK; c.a = 5; c.b = (uint8_t)offsetof(bm_track, speed); c.c = SPD_X3_2; bm_cmd_push(&app.cmdq, &c);
    c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);
    bm_app_audio(&app, abuf, 2);
    uint32_t start = app.ctx.sample_clock;
    int steps = 0;
    uint16_t last = app.ctx.st.tn[0];
    uint32_t at[8];
    while (steps < 8) {
        bm_app_audio(&app, abuf, 2);
        if (app.ctx.st.tn[0] != last) { at[steps++] = app.ctx.sample_clock - start; last = app.ctx.st.tn[0]; }
    }
    for (int i = 1; i < 8; i++) { double d = (double)(at[i] - at[i - 1]); CHECK(d > 2757.4 - 2 && d < 2757.4 + 2); }
    CHECK(app.ctx.st.tn[2] == 2 * app.ctx.st.tn[0] || app.ctx.st.tn[2] == 2 * app.ctx.st.tn[0] + 1);    /* x2 */
    CHECK(app.ctx.st.tn[5] * 2 >= app.ctx.st.tn[0] * 3 - 2 && app.ctx.st.tn[5] * 2 <= app.ctx.st.tn[0] * 3 + 2);  /* x3/2 */
    /* polímetro: el shaker (línea 12) dura 12 pasos y el bombo 16 */
    while (app.ctx.st.tn[0] < 20) bm_app_audio(&app, abuf, 64);
    int s_kick = bm_app_track_step(&app, 0, 0), s_shk = bm_app_track_step(&app, 0, 12);
    CHECK(s_kick == (int)(app.ctx.st.tn[0] % 16) && s_shk == (int)(app.ctx.st.tn[0] % 12));
    uint32_t r1 = 0xB0BBAu, r2 = 0xB0BBAu;
    int same = 1;
    for (int i = 0; i < 1000; i++) if (bm_xorshift32(&r1) != bm_xorshift32(&r2)) same = 0;
    CHECK(same);
}

static void test_tr_features(void)
{
    printf("batería TR: acento, repetición, fill y roll\n");
    boot(1);
    /* repetición x2 en el hat del paso 15: dos golpes del hat cerrado (línea 2) dentro de ese paso */
    btn(BTN_PLAY, 20);
    int hits = 0, prev = 0;
    for (int i = 0; i < 40000 && app.ctx.st.tn[0] < 32; i++) {
        bm_app_audio(&app, abuf, 32);
        int cur = (app.ctx.st.lane_trig[0] >> 2) & 1;
        if (cur && !prev && app.ctx.st.tn[0] % 16 == 14) hits++;
        prev = cur; app.ctx.st.lane_trig[0] = 0;
    }
    CHECK(hits >= 2);
    /* fill automático cada 4 compases: el bombo del paso 15 (solo en fill) suena en el compás 4 */
    CHECK(app.pat.meta.fill_every == 4);
    btn(BTN_PLAY, 20);
    /* ROLL: ARP sostenido en T1 + blanca */
    bm_app_button(&app, BTN_ARP, 1, now_ms); run_ms(400);
    CHECK(bm_app_eff_view(&app) == V_ROLL);
    bm_app_key(&app, bm_white_key(4), 1, now_ms);
    int n = 0;
    for (int i = 0; i < 40; i++) { run_ms(20); if (app.lane_flash[0][4] > now_ms) n++; }
    bm_app_key(&app, bm_white_key(4), 0, now_ms);
    bm_app_button(&app, BTN_ARP, 0, now_ms); run_ms(100);
    CHECK(n > 5);
    CHECK(app.ctx.roll[0].mask == 0);
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
    for (int i = 0; i < BM_V_DRUM_N; i++) CHECK(!app.ctx.voice[i].active || app.ctx.voice[i].track == 0);
    c.type = CMD_ALL_OFF; bm_cmd_push(&app.cmdq, &c);
    run_ms(500);
}

static void test_loop(void)
{
    printf("loopera: graba, cierra y vuelve a sonar\n");
    boot(1);
    bm_app_encoder(&app, ENC_ALGO, 3, now_ms); run_ms(50);       /* T4 */
    while (app.track != 3) { bm_app_encoder(&app, ENC_ALGO, app.track < 3 ? 1 : -1, now_ms); run_ms(40); }
    goto_view(BTN_SEQ, V_SEQ);
    btn(BTN_SEQ, 30);
    CHECK(app.view == V_LOOP);
    enc(ENC_K2, -1);                                               /* 1 compás */
    btn(BTN_REC, 30);
    CHECK(app.ctx.st.loop_mode[1] == LOOP_ARMED);
    btn(BTN_PLAY, 20);
    run_ms(100);
    CHECK(app.ctx.st.loop_mode[1] == LOOP_REC);
    key(bm_white_key(2), 120); run_ms(300); key(bm_white_key(5), 120);
    run_ms(2600);                                                  /* 1 compás a 112 BPM = 2.14 s */
    CHECK(app.ctx.st.loop_mode[1] == LOOP_PLAY);
    CHECK(app.pat.loop[1].n >= 4);
    shot("16-loop");
    bm_app_button(&app, BTN_REC, 1, now_ms); run_ms(2200); bm_app_button(&app, BTN_REC, 0, now_ms); run_ms(60);
    CHECK(app.pat.loop[1].n == 0);                                 /* REC 2 s en LOOP: borra el loop */
    btn(BTN_PLAY, 20);
}

static void test_sampler_flow(void)
{
    printf("sampler: graba la mezcla, chops, se guarda al parar\n");
    boot(1);
    while (app.track != 1) { bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(40); }
    goto_view(BTN_EDIT, V_EDIT);
    btn(BTN_PLAY, 20);
    run_ms(300);
    bm_app_button(&app, BTN_REC, 1, now_ms); run_ms(200);
    white(0, 30); run_ms(250); white(1, 30); run_ms(250); white(2, 30); run_ms(250);
    shot("10-sampler-rec");
    bm_app_button(&app, BTN_REC, 0, now_ms); run_ms(100);
    CHECK(app.ctx.st.smp_unsaved && app.ctx.smp.nmarks == 3);
    CHECK(app.ctx.arena_busy);
    btn(BTN_PLAY, 20);                                              /* parar: se guarda solo */
    run_ms(600);
    CHECK(!app.ctx.st.smp_unsaved && app.smp_valid[0]);
    CHECK(app.smp_info[0].nmarks == 3 && app.smp_info[0].len > 10000);
    CHECK(!app.ctx.arena_busy);
    white(1, 60);                                                   /* el chop 2 suena desde la flash */
    bm_app_button(&app, BTN_EDIT, 1, now_ms); run_ms(2200); bm_app_button(&app, BTN_EDIT, 0, now_ms); run_ms(60);
    CHECK(app.view == V_SAMPLER);
    shot("11-sampler");
    enc(ENC_K2, 1); enc(ENC_K2, 1);                                 /* 8 partes iguales */
    run_ms(300);
    CHECK(app.smp_info[0].nmarks == 8);
    boot(0);
    CHECK(app.smp_valid[0] && app.smp_info[0].nmarks == 8);         /* sobrevive al reinicio */
}

static void test_factory_loop(void)
{
    printf("sampler: loop de fábrica en la ROM y dibujo por bandas\n");
    boot(1);
    CHECK(bm_rom_nloops >= 1 && bm_app_smp_src(&app) == BM_SMP_SLOTS);   /* el demo arranca con FAB 1 */
    CHECK(app.ctx.smp.data == bm_rom_loops[0].data && app.ctx.smp.len == bm_rom_loops[0].len);
    int lo = 0, hi = 0;
    for (int x = 0; x < 232; x++) { lo += app.wave[0][x] > 200; hi += app.wave[2][x] > 200; }
    CHECK(lo > 0 && hi > 0);                                         /* se ven golpes graves y agudos */
    while (app.track != 1) { bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(40); }
    goto_view(BTN_EDIT, V_EDIT);
    bm_app_key(&app, bm_white_key(4), 1, now_ms); run_ms(20);
    int on = 0;
    for (int i = 0; i < BM_VOICES; i++) on += app.ctx.voice[i].active && app.ctx.voice[i].track == 1;
    CHECK(on == 1);                                                  /* el chop 5 suena desde la ROM */
    bm_app_key(&app, bm_white_key(4), 0, now_ms); run_ms(40);
    bm_app_button(&app, BTN_EDIT, 1, now_ms); run_ms(2200); bm_app_button(&app, BTN_EDIT, 0, now_ms); run_ms(60);
    CHECK(app.view == V_SAMPLER);
    shot("30-sampler-fab");
    enc(ENC_K2, 1);                                                  /* chops: la fábrica es de solo lectura */
    run_ms(300);
    CHECK(bm_app_smp_src(&app) == BM_SMP_SLOTS && !app.smp_valid[0]);
}

static void test_demo_and_persist(void)
{
    printf("demo: audio, autoguardado y recarga\n");
    boot(1);
    peak_abs = 0;
    wav_open("demo.wav");
    btn(BTN_PLAY, 20);
    run_ms(17000);
    wav_close();
    CHECK(peak_abs > 0.05 && peak_abs <= 1.0);
    CHECK(sim.flash_writes == 0);                                   /* nunca escribe flash sonando */
    btn(BTN_PLAY, 20);
    run_ms(UI_AUTOSAVE_MS + 1500);
    CHECK(sim.flash_writes > 0 && !app.dirty);
    static uint8_t b1[BM_PENDING_LEN], b2[BM_PENDING_LEN];
    int n1 = bm_pattern_pack(&app.pat, b1, sizeof b1);
    boot(0);
    int n2 = bm_pattern_pack(&app.pat, b2, sizeof b2);
    CHECK(n1 > 0 && n1 == n2 && memcmp(b1, b2, (size_t)n1) == 0);
    CHECK(app.song.scene[1].valid && app.song.scene[1].trk_mute == 0x3C);
}

static void test_ui_flow(void)
{
    printf("interfaz: recorrido de pantallas (capturas)\n");
    boot(1);
    shot("00-home");
    btn(BTN_PLAY, 20);
    run_ms(900);
    shot("01-home-play");
    btn(BTN_SEQ, 30); run_ms(300);
    shot("02-seq-tr");
    /* mantener un paso de la caja + girar = P-Lock; ALGORITHM = repetición */
    key(bm_black_key(4), 30);                                       /* negra 5: SNARE */
    CHECK(app.sel_lane[0] == 4);
    int k = bm_white_key(4);
    bm_app_key(&app, k, 1, now_ms); run_ms(400);
    bm_app_encoder(&app, ENC_K2, 5, now_ms); run_ms(60);
    bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(60);
    shot("03-seq-plock");
    bm_app_key(&app, k, 0, now_ms); run_ms(60);
    CHECK(bm_tr_lock_get(&app.pat.tr[0], 4, 4, 1) >= 0);
    CHECK(((app.pat.tr[0].st[4][4] & ST_RAT_MASK) >> ST_RAT_SH) == 1);
    white(1, 40);
    CHECK(app.pat.tr[0].st[4][1] & ST_ON);
    white(1, 40);
    CHECK(!(app.pat.tr[0].st[4][1] & ST_ON));
    /* SEQ 2 s: TIEMPO */
    bm_app_button(&app, BTN_SEQ, 1, now_ms); run_ms(1200);
    shot("04-hold-ring");
    run_ms(1000); bm_app_button(&app, BTN_SEQ, 0, now_ms); run_ms(60);
    CHECK(app.view == V_TIME);
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1); enc(ENC_K2, 1);         /* T3 más rápido */
    CHECK(app.pat.trk[2].speed == SPD_X3_2);
    enc(ENC_K1, -1);
    shot("05-time");
    btn(BTN_HOME, 30);
    /* pistas synth */
    while (app.track != 2) { bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(40); }
    btn(BTN_SEQ, 30);
    shot("06-seq-bass");
    enc(ENC_K1, -6);
    shot("07-overlay");
    run_ms(1700);
    while (app.track != 0) { bm_app_encoder(&app, ENC_ALGO, -1, now_ms); run_ms(40); }
    goto_view(BTN_EDIT, V_EDIT);
    shot("08-kit");
    btn(BTN_EDIT, 30);
    CHECK(app.view == V_INST);
    shot("09-inst");
    while (app.track != 3) { bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(40); }
    goto_view(BTN_EDIT, V_EDIT);
    shot("12-wave");
    btn(BTN_ENV, 30);
    shot("13-env");
    btn(BTN_FX, 30);
    shot("14-fx");
    goto_view(BTN_SCL, V_SCL);
    key(bm_black_key(3), 100);
    shot("15-scl");
    btn(BTN_SCL, 30);
    CHECK(app.view == V_CHORDS);
    bm_app_key(&app, bm_white_key(5), 1, now_ms); run_ms(150);
    CHECK(app.chord_name[0] != 0);
    shot("17-chords");
    bm_app_key(&app, bm_white_key(5), 0, now_ms); run_ms(60);
    bm_app_button(&app, BTN_FX, 1, now_ms); run_ms(400);
    bm_app_key(&app, bm_white_key(2), 1, now_ms); run_ms(400);
    shot("18-punch");
    CHECK(app.ctx.st.punch == 2);
    bm_app_key(&app, bm_white_key(2), 0, now_ms); bm_app_button(&app, BTN_FX, 0, now_ms); run_ms(200);
    CHECK(app.ctx.st.punch == 0xFF);
    bm_app_button(&app, BTN_SEQ, 1, now_ms); run_ms(400);
    white(1, 40);
    shot("19-bank");
    bm_app_button(&app, BTN_SEQ, 0, now_ms); run_ms(100);
    bm_app_button(&app, BTN_HOME, 1, now_ms); run_ms(400);
    white(4, 40);
    shot("20-mute");
    bm_app_button(&app, BTN_HOME, 0, now_ms); run_ms(100);
    CHECK(app.pat.trk[4].mute == 1);
    while (app.track != 0) { bm_app_encoder(&app, ENC_ALGO, -1, now_ms); run_ms(40); }
    bm_app_button(&app, BTN_EDIT, 1, now_ms); run_ms(400);
    shot("21-erase");
    bm_app_button(&app, BTN_EDIT, 0, now_ms); run_ms(100);
    bm_app_button(&app, BTN_ARP, 1, now_ms); run_ms(400);
    bm_app_key(&app, bm_white_key(2), 1, now_ms); run_ms(300);
    shot("22-roll");
    bm_app_key(&app, bm_white_key(2), 0, now_ms); bm_app_button(&app, BTN_ARP, 0, now_ms); run_ms(100);
    /* escenas */
    goto_view(BTN_HOME, V_HOME);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_SCENES);
    white(5, 40);                                                   /* guardar en B */
    white(0, 40);                                                   /* lanzar A */
    shot("23-scenes");
    run_ms(9000);
    CHECK(app.ctx.st.scene == 0);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO);
    shot("24-glo");
    bm_app_button(&app, BTN_HOME, 1, now_ms); run_ms(2200); bm_app_button(&app, BTN_HOME, 0, now_ms); run_ms(60);
    CHECK(app.view == V_SYSTEM);
    shot("25-system");
    btn(BTN_ARP, 30);
    shot("26-arp");
    btn(BTN_SAVE, 30);
    shot("27-save");
    bm_app_button(&app, BTN_GLO, 1, now_ms); btn(BTN_SEQ, 30); bm_app_button(&app, BTN_GLO, 0, now_ms); run_ms(40);
    white(0, 30); white(1, 30);
    shot("28-song");
    /* REC 2 s: borra la pista (con deshacer) */
    btn(BTN_PLAY, 20);
    while (app.track != 0) { bm_app_encoder(&app, ENC_ALGO, -1, now_ms); run_ms(40); }
    bm_app_button(&app, BTN_REC, 1, now_ms); run_ms(2200); bm_app_button(&app, BTN_REC, 0, now_ms); run_ms(60);
    int any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(!any && !app.rec);
    bm_app_button(&app, BTN_EDIT, 1, now_ms); run_ms(400);
    bm_app_button(&app, BTN_OCT_DN, 1, now_ms); bm_app_button(&app, BTN_OCT_DN, 0, now_ms); run_ms(60);
    bm_app_button(&app, BTN_EDIT, 0, now_ms); run_ms(100);
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);                          /* deshecho */
    bm_platform pf; bm_sim_platform(&sim, &pf);
    now_ms += 10; next_tick = now_ms; bm_app_init(&app, &pf, now_ms); run_ms(300);
    shot("29-boot");
}

static void test_spi_budget(void)
{
    printf("pantalla: bytes por frame en SEQ sonando\n");
    boot(1);
    btn(BTN_SEQ, 30);
    btn(BTN_PLAY, 20);
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
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) outdir = argv[1];
    bm_tables_init();
    test_pack();
    test_store();
    test_samples();
    test_gesture();
    test_timing();
    test_tr_features();
    test_voices();
    test_loop();
    test_factory_loop();
    test_sampler_flow();
    test_demo_and_persist();
    test_spi_budget();
    test_ui_flow();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
