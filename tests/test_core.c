/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pruebas del núcleo en PC (interfaz 3.0).
 * Uso: test_core [carpeta_salida]  -> escribe demo.wav y capturas *.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_app_int.h"
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

static void down(int id) { bm_app_button(&app, id, 1, now_ms); }
static void up(int id) { bm_app_button(&app, id, 0, now_ms); }
static void btn(int id, uint32_t hold_ms) { down(id); run_ms(hold_ms); up(id); run_ms(40); }
static void key(int k, uint32_t hold_ms) { bm_app_key(&app, k, 1, now_ms); run_ms(hold_ms); bm_app_key(&app, k, 0, now_ms); run_ms(40); }
static void kdown(int k) { bm_app_key(&app, k, 1, now_ms); }
static void kup(int k) { bm_app_key(&app, k, 0, now_ms); }
static void white(int i, uint32_t hold_ms) { key(bm_white_key(i), hold_ms); }
static void black(int i, uint32_t hold_ms) { key(bm_black_key(i), hold_ms); }
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

/* HOME y luego ALGORITHM hasta el canal: el recuadro entra solo a los 400 ms. */
static void ch(int t)
{
    if (app.view != V_HOME) { btn(BTN_HOME, 30); if (app.view != V_HOME) btn(BTN_HOME, 30); }
    for (int i = 0; i < 6 && app.view != V_HOME; i++) btn(BTN_HOME, 30);
    if (t == app.track) { app.track = (uint8_t)(t ? t - 1 : t + 1); }
    int d = t > app.track ? 1 : -1, n = t > app.track ? t - app.track : app.track - t;
    for (int i = 0; i < n; i++) enc(ENC_ALGO, d);
    run_ms(500);
}

static void page(int btn_id, int pg)
{
    for (int i = 0; i < 4 && !(app.view == V_CH && app.page == pg); i++) btn(btn_id, 30);
}

/* ------------------------------------------------------------ formato, flash y tomas */
static void test_pack(void)
{
    printf("formato 3: rejillas, sampler, notas, KIT libre y loopera\n");
    static bm_pattern p, q;
    static uint8_t b1[BM_PENDING_LEN], b2[BM_PENDING_LEN];
    bm_pattern_init(&p);
    for (int s = 0; s < 64; s += 3) p.tr[0].st[s % 16][s] = ST_ON | ST_ACC | (2 << ST_RAT_SH);
    p.tr[1].st[BRK_GEN][2] = ST_ON; p.tr[1].st[4][6] = ST_ON | ST_ACC;
    p.tr[1].lane[5].len = 12; p.tr[0].mute = 0x0104;
    CHECK(bm_tr_lock_set(&p.tr[0], 9, 9, 2, 33));
    CHECK(bm_tr_lock_get(&p.tr[0], 9, 9, 2) == 33);
    CHECK(p.tr[0].st[9][9] & ST_LOCK);
    bm_track_set_engine(&p, 4, ENG_KIT);                              /* T5 KIT: rejilla en la unión */
    CHECK(bm_is_tr(&p, 4) && bm_kind(&p, 1) == KIND_BRK && bm_kind(&p, 2) == KIND_NOTE);
    bm_trk_tr(&p, 4)->st[3][5] = ST_ON;
    bm_track_set_engine(&p, 5, ENG_MIDI);
    for (int t = 2; t < 6; t++) {
        if (t == 4) continue;
        for (int s = 0; s < 64; s += 5) { bm_event *e = bm_ev(&p, t, s, 1); e->note = (uint8_t)(40 + s); e->vel = 99; e->len = 2; e->prob = 70; e->lk[3] = 7; }
    }
    p.loop[1].on = 1; p.loop[1].bars = 4; p.loop[1].n = 3;
    for (int i = 0; i < 3; i++) { p.loop[1].ev[i].tick = (uint16_t)(i * 100); p.loop[1].ev[i].note = 60; p.loop[1].ev[i].vel = (uint8_t)(i & 1 ? 0 : 90); }
    p.meta.bpm_x10 = 1337; p.trk[3].speed = SPD_X3_4; p.trk[3].length = 12; p.meta.transpose = 26;
    int n = bm_pattern_pack(&p, b1, sizeof b1);
    CHECK(n > 0 && n <= (int)BM_SLOT_BODY);
    CHECK(bm_pattern_unpack(&q, b1, n) == n);
    CHECK(bm_pattern_pack(&q, b2, sizeof b2) == n && memcmp(b1, b2, (size_t)n) == 0);
    CHECK(memcmp(&p.nt[0], &q.nt[0], sizeof p.nt[0]) == 0);
    CHECK(memcmp(p.tr[0].st, q.tr[0].st, sizeof p.tr[0].st) == 0);
    CHECK(q.tr[1].st[4][6] == (ST_ON | ST_ACC) && bm_brk_step(&q.tr[1], 6, NULL) == 4 && bm_brk_step(&q.tr[1], 2, NULL) == 0);
    CHECK(q.trk[4].engine == ENG_KIT && bm_trk_tr(&q, 4)->st[3][5] == ST_ON && q.trk[5].engine == ENG_MIDI);
    CHECK(q.trk[3].speed == SPD_X3_4 && q.trk[3].length == 12 && q.tr[1].lane[5].len == 12 && q.meta.transpose == 26);
    CHECK(q.loop[1].n == 3 && q.loop[1].ev[2].tick == 200);
    b1[offsetof(bm_meta, fmt)] = 2;                                   /* un patrón de 0.2: no se lee */
    CHECK(bm_pattern_unpack(&q, b1, n) < 0);
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
    CHECK(bm_store_format(&st3, pf.flash) == 0 && !bm_store_has(&st3, 3));
    CHECK(pf.flash->erase(0x000000) != 0);
    CHECK(pf.flash->erase(0x0E8000) != 0);
    printf("  %u slots de patrón + %u sectores de tomas\n", (unsigned)BM_STORE_SLOTS, (unsigned)BM_TAKE_SECTORS);
}

static void test_takes(void)
{
    printf("biblioteca de tomas: largo libre, CRC, cortes y borrar\n");
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static bm_take tk[BM_TAKES];
    static int8_t d[BM_SMP_REC_MAX];
    for (uint32_t i = 0; i < BM_SMP_REC_MAX; i++) d[i] = (int8_t)((i * 7) & 0x7F);
    CHECK(bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk) == 0);
    uint32_t full = bm_takes_free(tk);
    CHECK(bm_takes_room(tk) == BM_SMP_REC_MAX);
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, BM_SMP_REC_MAX, 1600, 2) == 0);   /* 3 s */
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, 20000, 1200, 1) == 1);
    CHECK(tk[0].valid && tk[1].valid && tk[1].len == 20000 && tk[1].data[123] == d[123] && tk[1].gain == 64);
    CHECK(bm_takes_free(tk) < full);
    tk[1].nmarks = 3; tk[1].marks[0] = 0; tk[1].marks[1] = 5000; tk[1].marks[2] = 12000; tk[1].gain = 80;
    static uint8_t scratch[BM_SECTOR];
    CHECK(bm_takes_update(pf.flash, &tk[1], scratch) == 0);
    static bm_take tk2[BM_TAKES];
    CHECK(bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk2) == 2);
    CHECK(tk2[1].nmarks == 3 && tk2[1].marks[2] == 12000 && tk2[1].gain == 80 && tk2[1].bpm_x10 == 1200 && tk2[1].data[999] == d[999]);
    int n = 2;
    while (bm_takes_write(pf.flash, pf.flash_map, pf.user, tk2, d, 16000, 1200, 1) >= 0) n++;
    CHECK(n >= 5 && bm_takes_room(tk2) < 16000);
    printf("  %d tomas (3 s + 1.25 s + 1 s...) llenan la biblioteca de %u.%u s\n", n,
           (unsigned)(full / BM_SMP_RATE), (unsigned)(full * 10u / BM_SMP_RATE % 10u));
    CHECK(bm_takes_erase(pf.flash, &tk2[0]) == 0);
    CHECK(bm_takes_room(tk2) == BM_SMP_REC_MAX);                       /* el hueco de 3 s vuelve */
    sim.flash[BM_TAKE_LO + tk2[1].sector * BM_SECTOR + BM_TAKE_HDR + 500] ^= 0x55;   /* byte dañado */
    static bm_take tk3[BM_TAKES];
    bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk3);
    CHECK(!tk3[1].valid);
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
    bm_gesture_other(&g, 4500);
    bm_gesture_poll(&g, 7000);
    CHECK(L.n == 1 && L.log[0] == BTN_HOME * 10 + G_HOLD);                            /* el combo cancela los 2 s */
    bm_gesture_up(&g, BTN_HOME, 7100); CHECK(L.n == 2 && L.log[1] == BTN_HOME * 10 + G_RELEASE);
    L.n = 0;
    bm_gesture_down(&g, BTN_GLO, 8000); bm_gesture_other(&g, 8050);
    CHECK(L.n == 1 && L.log[0] == BTN_GLO * 10 + G_HOLD);                             /* GLO ahora tiene capa */
    bm_gesture_up(&g, BTN_GLO, 8100);
    L.n = 0;
    bm_gesture_down(&g, BTN_PLAY, 9000);
    CHECK(L.n == 1 && L.log[0] == BTN_PLAY * 10 + G_PRESS);
    bm_gesture_up(&g, BTN_PLAY, 9010);
    L.n = 0;
    bm_gesture_down(&g, BTN_SAVE, 10000); bm_gesture_poll(&g, 10400); CHECK(L.n == 1 && L.log[0] == BTN_SAVE * 10 + G_HOLD);
    bm_gesture_poll(&g, 12001); CHECK(L.n == 2 && L.log[1] == BTN_SAVE * 10 + G_LONG);
    bm_gesture_up(&g, BTN_SAVE, 12100); CHECK(L.n == 2);
    L.n = 0;
    bm_gesture_down(&g, BTN_REC, 13000); bm_gesture_up(&g, BTN_REC, 13100);
    CHECK(L.n == 1 && L.log[0] == BTN_REC * 10 + G_TAP);                              /* REC: toque (al soltar) */
}

/* ------------------------------------------------------------ audio */
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
    while (app.ctx.st.tn[0] < 20) bm_app_audio(&app, abuf, 64);
    int s_kick = bm_app_track_step(&app, 0, 0), s_shk = bm_app_track_step(&app, 0, 12);
    CHECK(s_kick == (int)(app.ctx.st.tn[0] % 16) && s_shk == (int)(app.ctx.st.tn[0] % 12));
}

static void test_tr_features(void)
{
    printf("batería TR: repetición, fill y roll\n");
    boot(1);
    btn(BTN_PLAY, 20);
    int hits = 0, prev = 0;
    for (int i = 0; i < 40000 && app.ctx.st.tn[0] < 32; i++) {
        bm_app_audio(&app, abuf, 32);
        int cur = (app.ctx.st.lane_trig[0] >> 2) & 1;
        if (cur && !prev && app.ctx.st.tn[0] % 16 == 14) hits++;
        prev = cur; app.ctx.st.lane_trig[0] = 0;
    }
    CHECK(hits >= 2);
    CHECK(app.pat.meta.fill_every == 4);
    btn(BTN_PLAY, 20);
    ch(0);
    down(BTN_ARP); run_ms(400);
    CHECK(app.layer == L_ROLL);
    kdown(bm_white_key(4));
    int n = 0;
    for (int i = 0; i < 40; i++) { run_ms(20); if (app.lane_flash[0][4] > now_ms) n++; }
    kup(bm_white_key(4));
    up(BTN_ARP); run_ms(100);
    CHECK(n > 5);
    CHECK(app.ctx.roll[0].mask == 0 && app.layer == L_NONE);
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

/* ------------------------------------------------------------ navegación 3.0 */
static void test_navigation(void)
{
    printf("navegación: ALGORITHM con recuadro, pestañas, HOME otra vez, GLO\n");
    boot(1);
    CHECK(app.view == V_HOME && app.track == 0);
    shot("00-home");
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);                       /* recuadro: T3 */
    CHECK(app.box_on && app.box_track == 2 && app.view == V_HOME);
    shot("38-box");
    run_ms(200);
    CHECK(app.view == V_HOME);                                /* todavía no */
    run_ms(300);
    CHECK(app.view == V_CH && app.track == 2 && app.page == PG_SEQ);   /* a los 400 ms entra */
    shot("06-seq-bass");
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_SOUND);
    enc(ENC_ALGO, 1);                                         /* dentro de un canal: cambia y conserva la página */
    CHECK(app.track == 3 && app.page == PG_SOUND && app.view == V_CH);
    shot("12-wave");
    enc(ENC_SELECT, 1);                                       /* SELECT recorre las pestañas */
    CHECK(app.page == PG_ENV);
    enc(ENC_SELECT, -1); enc(ENC_SELECT, -1); enc(ENC_SELECT, -1);
    CHECK(app.page == PG_MORE);                               /* la última: MAS */
    shot("45-more");
    btn(BTN_FX, 30);
    CHECK(app.page == PG_MIX);
    int lv = app.pat.trk[3].level;
    enc(ENC_K1, -2); enc(ENC_K1, -2);
    CHECK(app.pat.trk[3].level == lv - 4);
    shot("14-mix");
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_HOME, 30); CHECK(app.view == V_MIX);
    shot("39-mezcla");
    btn(BTN_HOME, 30); CHECK(app.view == V_SCENES);
    btn(BTN_HOME, 30); CHECK(app.view == V_SONG);
    btn(BTN_HOME, 30); CHECK(app.view == V_HOME);
    /* HOME + ALGORITHM: elige canal desde cualquier pantalla y entra al soltar HOME */
    ch(0);
    btn(BTN_SCL, 30);
    down(BTN_HOME); run_ms(60);
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.layer == L_HOME && app.box_on && app.box_hold == 1);
    run_ms(800);
    CHECK(app.view == V_CH && app.track == 0);                /* espera a soltar HOME */
    up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_CH && app.track == 3 && app.layer == L_NONE);
    /* GLO: lista global y otra vez cierra */
    btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO);
    enc(ENC_SELECT, 1); enc(ENC_ALGO, 1);                     /* SWING +1 */
    CHECK(app.pat.meta.swing == 57);
    shot("24-glo");
    btn(BTN_GLO, 30);
    CHECK(app.view == V_CH && app.track == 3);
    /* entrada automática OFF: ALGORITHM en HOME solo elige */
    app.set.enter = 4;
    btn(BTN_HOME, 30);
    enc(ENC_ALGO, -1);
    run_ms(900);
    CHECK(app.view == V_HOME && app.track == 2);
    app.set.enter = 2;
}

static void test_home(void)
{
    printf("HOME en todas partes: controlador, mutes, solo, cadenas y versión global\n");
    boot(1);
    ch(2);
    btn(BTN_PLAY, 20);
    run_ms(300);
    down(BTN_HOME); run_ms(400);
    CHECK(app.layer == L_HOME && app.view == V_CH);
    shot("20-layer-home");
    white(4, 40);                                             /* patrón 5 en cola */
    CHECK(app.ctx.st.queued == 4);
    black(0, 40);                                             /* toque = mute T1 */
    CHECK(app.pat.trk[0].mute == 1);
    kdown(bm_black_key(3)); run_ms(500);                      /* mantener negra 4 = solo T4 */
    CHECK(app.pat.trk[3].mute == 0 && app.pat.trk[2].mute == 1 && app.pat.trk[5].mute == 1);
    kup(bm_black_key(3)); run_ms(80);
    CHECK(app.pat.trk[0].mute == 1 && app.pat.trk[2].mute == 0 && app.pat.trk[3].mute == 0);   /* vuelve */
    black(0, 40);
    CHECK(app.pat.trk[0].mute == 0);
    kdown(bm_white_key(1)); run_ms(40); white(2, 40); kup(bm_white_key(1)); run_ms(40);   /* dos blancas: cadena */
    CHECK(app.ctx.st.pchain_n == 2);
    up(BTN_HOME); run_ms(60);
    CHECK(app.layer == L_NONE && app.view == V_CH && app.track == 2);    /* soltar HOME no lleva a HOME */
    btn(BTN_PLAY, 20);
    /* HOME + botón */
    down(BTN_HOME); run_ms(60); btn(BTN_SEQ, 30); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SONG);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    white(0, 40);                                             /* quita el bombo del paso 1 */
    CHECK(!(app.pat.tr[0].st[0][0] & ST_ON));
    down(BTN_HOME); run_ms(40); btn(BTN_OCT_DN, 20); up(BTN_HOME); run_ms(60);
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);                   /* HOME + OCT- deshace */
    down(BTN_HOME); run_ms(40); btn(BTN_FX, 30); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_FXG);
    shot("50-fx-global");
    down(BTN_HOME); run_ms(40); btn(BTN_SCL, 30); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SCLG);
    down(BTN_HOME); run_ms(40); btn(BTN_PLAY, 20); up(BTN_HOME); run_ms(60);
    CHECK(app.ctx.st.playing);                                /* HOME + PLAY: desde el paso 1 */
    btn(BTN_PLAY, 20);
    /* HOME + EDIT: copiar el canal (T1 batería a T5: pasa a KIT) */
    ch(0);
    down(BTN_HOME); run_ms(40); btn(BTN_EDIT, 30); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_COPYCH && app.copy_src == 0);
    for (int i = 0; i < 12 && app.copy_dst != 4; i++) enc(ENC_ALGO, 1);
    shot("49-copy-channel");
    btn(BTN_OCT_UP, 20); run_ms(100);
    CHECK(app.pat.trk[4].engine == ENG_KIT && app.view == V_CH && app.track == 4);
    CHECK(bm_trk_tr(&app.pat, 4)->st[0][0] & ST_ON);
}

static void test_layers(void)
{
    printf("capas: largo, acento, repetir, borrar, copiar patrón, tono y tap\n");
    boot(1);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    shot("02-seq-tr");
    down(BTN_SEQ); run_ms(400);
    CHECK(app.layer == L_LEN);
    shot("40-layer-len");
    white(11, 40);                                            /* blanca 12 = largo 12 */
    black(3, 40);                                             /* negra 4 = X3/4 */
    up(BTN_SEQ); run_ms(60);
    CHECK(app.pat.tr[0].lane[0].len == 12 && app.pat.tr[0].lane[7].len == 12 && app.pat.trk[0].speed == SPD_X3_4);
    CHECK(app.view == V_CH && app.page == PG_SEQ);
    down(BTN_ENV); run_ms(400);
    CHECK(app.layer == L_ACC);
    white(0, 40);
    shot("41-layer-acc");
    up(BTN_ENV); run_ms(60);
    CHECK(app.pat.tr[0].st[0][0] & ST_ACC);
    down(BTN_LFO); run_ms(400);
    white(0, 40); white(0, 40);
    shot("42-layer-rep");
    up(BTN_LFO); run_ms(60);
    CHECK(((app.pat.tr[0].st[0][0] & ST_RAT_MASK) >> ST_RAT_SH) == 2);
    down(BTN_EDIT); run_ms(400);
    shot("21-erase");
    white(0, 40);                                             /* parado: borra la línea del bombo */
    up(BTN_EDIT); run_ms(60);
    int any = 0;
    for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[0][s]) any = 1;
    CHECK(!any);
    down(BTN_HOME); run_ms(40); btn(BTN_OCT_DN, 20); up(BTN_HOME); run_ms(60);
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);
    down(BTN_FX); run_ms(400);
    kdown(bm_white_key(2)); run_ms(400);
    shot("18-punch");
    CHECK(app.ctx.st.punch == 2);
    kup(bm_white_key(2)); up(BTN_FX); run_ms(200);
    CHECK(app.ctx.st.punch == 0xFF);
    /* GLO: TONO y TAP tempo */
    down(BTN_GLO); run_ms(400);
    CHECK(app.layer == L_TONE);
    key(9, 40);                                               /* tecla 9 = +2 */
    btn(BTN_PLAY, 10); run_ms(450); btn(BTN_PLAY, 10); run_ms(450); btn(BTN_PLAY, 10);
    shot("44-layer-tone");
    up(BTN_GLO); run_ms(60);
    CHECK(app.pat.meta.transpose == 26 && !app.ctx.st.playing);
    CHECK(app.pat.meta.bpm_x10 > 1150 && app.pat.meta.bpm_x10 < 1300);
    /* SAVE sostenido: copiar el patrón a otro lugar (parado) */
    run_ms(UI_AUTOSAVE_MS + 1500);
    down(BTN_SAVE); run_ms(400);
    CHECK(app.layer == L_COPY);
    white(5, 40);
    shot("43-layer-copy");
    up(BTN_SAVE); run_ms(600);
    CHECK(bm_store_has(&app.store, 5) && app.ctx.st.pattern == 0);
    down(BTN_SAVE); run_ms(400); kdown(bm_black_key(2)); white(5, 40); kup(bm_black_key(2)); up(BTN_SAVE); run_ms(60);
    CHECK(app.ask == ASK_COPY_TRK);                           /* A06 ya tiene datos: pregunta */
    shot("46-ask");
    btn(BTN_OCT_UP, 20); run_ms(600);
    CHECK(app.ask == ASK_NONE);
    /* REC 2 s: pregunta y borra el canal */
    ch(0);
    down(BTN_REC); run_ms(2200); up(BTN_REC); run_ms(60);
    CHECK(app.ask == ASK_CLEAR);
    btn(BTN_OCT_UP, 20);
    any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(!any);
    down(BTN_REC); run_ms(2200); up(BTN_REC); run_ms(60);
    btn(BTN_HOME, 30);                                        /* HOME = no */
    CHECK(app.ask == ASK_NONE && app.view == V_CH);
}

static void test_free_engines(void)
{
    printf("canales libres: MAS > MOTOR (KIT, MIDI) y canales fijos\n");
    boot(1);
    ch(3);
    enc(ENC_SELECT, -1);
    CHECK(app.page == PG_MORE && app.more_row == 0);
    enc(ENC_ALGO, 1);                                         /* SYNTH > MONO: notas a notas, sin preguntar */
    CHECK(app.pat.trk[3].engine == ENG_MONO && app.ask == ASK_NONE);
    enc(ENC_ALGO, 1);                                         /* MONO > KIT: pregunta */
    CHECK(app.ask == ASK_ENGINE);
    shot("46-ask-engine");
    btn(BTN_OCT_UP, 20); run_ms(60);
    CHECK(app.pat.trk[3].engine == ENG_KIT && bm_is_tr(&app.pat, 3));
    page(BTN_SEQ, PG_SEQ);
    white(0, 40); white(4, 40);
    CHECK(bm_trk_tr(&app.pat, 3)->st[0][0] & ST_ON);
    shot("47-kit-free");
    btn(BTN_PLAY, 20); run_ms(300);
    int drum = 0;
    for (int i = 0; i < 400 && !drum; i++) { run_ms(5); for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) drum = 1; }
    CHECK(drum);
    btn(BTN_PLAY, 20);
    /* T6 a MIDI: manda notas por MIDI */
    ch(5);
    enc(ENC_SELECT, -1);
    enc(ENC_ALGO, -1);                                         /* SYNTH < MIDI: las notas se quedan */
    CHECK(app.pat.trk[5].engine == ENG_MIDI && app.page == PG_MORE && app.ask == ASK_NONE);
    page(BTN_EDIT, PG_MIDI);
    shot("48-midi");
    sim.midi_n = 0;
    btn(BTN_PLAY, 20); run_ms(1500);
    int on = 0;
    for (int i = 0; i + 2 < sim.midi_n; i++) if (sim.midi_out[i] == (0x90 | 5)) on = 1;
    CHECK(on);
    btn(BTN_PLAY, 20);
    /* los canales fijos no tienen MOTOR */
    ch(2);
    enc(ENC_SELECT, -1);
    const char *nm; char v[24];
    bm_app_more_row(&app, 0, &nm, v, sizeof v);
    CHECK(strcmp(nm, "MOTOR") != 0);
}

/* ------------------------------------------------------------ sampler 3.0 */
static void test_sampler3(void)
{
    printf("sampler 3.0: generador, TU y SUENA, fijos, capturar y efectos\n");
    boot(1);
    CHECK(bm_rom_nloops >= 1 && app.pat.trk[1].source == BM_SRC_ROM);
    CHECK(app.ctx.smp[0].data == bm_rom_loops[0].data && app.ctx.smp[0].len == bm_rom_loops[0].len);
    int lo = 0, hi = 0;
    for (int x = 0; x < 232; x++) { lo += app.wave[0][x] > 200; hi += app.wave[2][x] > 200; }
    CHECK(lo > 0 && hi > 0);                                  /* el dibujo por bandas */
    CHECK(app.smp_send[0].kslice < BM_SLICES && app.smp_send[0].sslice < BM_SLICES && app.smp_send[0].kslice != app.smp_send[0].sslice);
    ch(1);
    CHECK(app.page == PG_SEQ);
    run_ms(200);
    const bm_status *st = &app.ctx.st;
    CHECK(st->gen_sl[0] == 0 && (st->gen_fx[0] & GEN_PIN));  /* paso 1 fijo al slice 1 */
    CHECK(st->gen_sl[12] == 4 && (st->gen_fx[12] & GEN_PIN) && (st->gen_fx[12] & GEN_ACC));
    CHECK(st->gen_sl[1] == 0xFF && st->gen_sl[2] != 0xFF);   /* hueco: silencio · generador: elige */
    for (int i = 0; i < 4; i++) { app.pat.trk[1].gen[i] = 0; }
    run_ms(200);
    CHECK(st->gen_sl[2] == 1 && st->gen_sl[4] == 2 && st->gen_sl[8] == 4);   /* sin variar: el loop en orden */
    enc(ENC_K1, 127);                                         /* VARIAR al máximo */
    run_ms(200);
    int diff = 0;
    for (int k = 2; k < 16; k += 2) if (k != 12 && st->gen_sl[k] != k / 2) diff++;
    CHECK(diff > 0);
    shot("31a-sampler-variar");
    run_ms(1700);
    shot("31-sampler-seq");
    /* blanca: generador <-> silencio · blanca + negra: fijo */
    white(1, 40);
    CHECK(bm_brk_step(&app.pat.tr[1], 1, NULL) == 0);
    white(1, 40);
    CHECK(bm_brk_step(&app.pat.tr[1], 1, NULL) < 0);
    kdown(bm_white_key(3)); run_ms(40); black(6, 40); kup(bm_white_key(3)); run_ms(60);
    CHECK(bm_brk_step(&app.pat.tr[1], 3, NULL) == 7);        /* paso 4 fijo al slice 7 */
    white(3, 40);
    CHECK(bm_brk_step(&app.pat.tr[1], 3, NULL) == 0);        /* fijo -> vuelve al generador */
    /* tocar slices y efectos en vivo */
    black(2, 40);
    CHECK(app.ctx.st.brk_slice == 2);
    btn(BTN_PLAY, 20);
    kdown(bm_black_key(8)); run_ms(60);
    CHECK(app.ctx.brk_live & 1);                              /* negra 9: reversa */
    int rev = 0;
    for (int j = 0; j < 100 && !rev; j++) {
        run_ms(5);
        for (int i = 0; i < BM_VOICES; i++) if (app.ctx.voice[i].active && app.ctx.voice[i].track == 1 && app.ctx.voice[i].dir) rev = 1;
    }
    kup(bm_black_key(8)); run_ms(300);
    CHECK(rev);
    kdown(bm_black_key(10)); run_ms(300);
    CHECK(app.ctx.brk_live & 4);
    shot("31b-stutter");
    kup(bm_black_key(10)); run_ms(60);
    /* CAPTURAR: EDIT sostenido + negra 4 deja fija la variación que suena */
    down(BTN_EDIT); run_ms(400);
    int before = 0, after = 0;
    for (int s = 0; s < 16; s++) before += bm_brk_step(&app.pat.tr[1], s, NULL) > 0;
    black(3, 40);
    up(BTN_EDIT); run_ms(100);
    for (int s = 0; s < 16; s++) after += bm_brk_step(&app.pat.tr[1], s, NULL) > 0;
    CHECK(after > before);
    btn(BTN_PLAY, 20);
    /* RETRIG (SEQ otra vez) y SONIDO */
    btn(BTN_SEQ, 30);
    CHECK(app.page == PG_RETRIG);
    shot("32-retrig");
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_SOUND);
    enc(ENC_K4, 3);
    CHECK(app.pat.trk[1].pitch == 25);
    uint8_t lb = app.pat.trk[1].loop_b;
    enc(ENC_K2, 1);                                           /* LOOP B */
    CHECK(app.pat.trk[1].loop_b != lb);
    shot("30-sampler-sound");
    enc(ENC_PRESETS, 1);                                      /* PRESETS: loop A */
    CHECK(app.pat.trk[1].source == (BM_SRC_ROM | 1));
}

static void test_recorder(void)
{
    printf("grabadora: armada, al compás, toma sin guardar, TOMAS y CORTAR\n");
    boot(1);
    ch(1);
    btn(BTN_REC, 30);
    CHECK(app.page == PG_REC && app.ctx.st.smp_armed && !app.ctx.st.smp_rec);
    shot("33-rec-armed");
    btn(BTN_PLAY, 20);
    run_ms(100);
    CHECK(app.ctx.st.smp_rec);                               /* empieza en el compás (el primero) */
    run_ms(1200);
    shot("34-rec");
    white(3, 40);
    CHECK(app.key_nn[bm_white_key(3)] == 0);                 /* las teclas no suenan */
    btn(BTN_REC, 30);
    CHECK(!app.ctx.st.smp_rec && app.ctx.st.smp_unsaved && app.ctx.arena_busy);
    CHECK(app.ctx.smp[0].data == (const int8_t *)app.amem.arena && app.ctx.smp[0].bars >= 1);
    shot("35-rec-unsaved");
    uint32_t len = app.ctx.st.smp_len;
    CHECK(len > 16000);
    btn(BTN_PLAY, 20);                                       /* parar: se guarda en TOMAS */
    run_ms(800);
    CHECK(!app.ctx.st.smp_unsaved && !app.ctx.arena_busy);
    CHECK(app.take[0].valid && app.take[0].len == len && app.pat.trk[1].source == 0);
    CHECK(app.ctx.smp[0].data == app.take[0].data);
    /* TOMAS (EDIT 2 s en el sampler) */
    down(BTN_EDIT); run_ms(2200); up(BTN_EDIT); run_ms(60);
    CHECK(app.page == PG_TAKES);
    uint8_t list[16];
    int n = bm_app_sources(&app, list);
    CHECK(n == bm_rom_nloops + 1);
    for (int i = 0; i < 12 && bm_app_takes_sel(&app) != 0; i++) enc(ENC_ALGO, 1);
    enc(ENC_K3, 2);                                          /* ganancia */
    CHECK(app.take[0].gain == 66);
    run_ms(1700);
    shot("36-takes");
    black(1, 40);                                            /* N2: la toma como loop B */
    CHECK(app.pat.trk[1].loop_b == 0);
    /* CORTAR */
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_CUT && app.cut_take == 0);
    white(2, 40);                                            /* corte en la corchea 3 */
    CHECK(app.take[0].nmarks == 2);
    white(4, 40);
    CHECK(app.take[0].nmarks == 3);
    white(2, 40);                                            /* otra vez: se quita */
    CHECK(app.take[0].nmarks == 2);
    black(8, 40);                                            /* en los golpes */
    CHECK(app.take[0].nmarks >= 2);
    shot("37-cut");
    enc(ENC_K2, 3);
    black(9, 40);                                            /* 8 iguales */
    CHECK(app.take[0].nmarks == 0);
    white(1, 40); white(5, 40);
    CHECK(app.take[0].nmarks == 3);
    btn(BTN_HOME, 30);
    run_ms(2200);                                            /* al salir y en reposo se guarda */
    CHECK(!app.take_dirty);
    run_ms(UI_AUTOSAVE_MS);                                  /* y el patrón que la usa */
    boot(0);
    CHECK(app.take[0].valid && app.take[0].nmarks == 3 && app.take[0].gain == 66);    /* sobrevive al reinicio */
    CHECK(app.ctx.smp[0].nmarks == 3);
    /* borrar la toma: pregunta y vuelve a la fábrica */
    ch(1);
    down(BTN_EDIT); run_ms(2200); up(BTN_EDIT); run_ms(60);
    for (int i = 0; i < 12 && bm_app_takes_sel(&app) != 0; i++) enc(ENC_ALGO, 1);
    black(10, 40);
    CHECK(app.ask == ASK_TAKE_DEL);
    btn(BTN_OCT_UP, 20); run_ms(600);
    CHECK(!app.take[0].valid && app.pat.trk[1].source == BM_SRC_ROM && app.pat.trk[1].loop_b == BM_SRC_NONE);
    /* HOME descarta una toma sin guardar */
    btn(BTN_REC, 30); btn(BTN_PLAY, 20); run_ms(800); btn(BTN_REC, 30);
    CHECK(app.ctx.st.smp_unsaved);
    btn(BTN_HOME, 30); run_ms(60);
    CHECK(!app.ctx.st.smp_unsaved && app.ctx.smp[0].data == bm_rom_loops[0].data);
    btn(BTN_PLAY, 20);
}

static void test_loop(void)
{
    printf("loopera: graba, cierra y vuelve a sonar\n");
    boot(1);
    ch(3);
    page(BTN_SEQ, PG_SEQ);
    btn(BTN_SEQ, 30);
    CHECK(app.page == PG_LOOP);
    enc(ENC_K2, -1);                                               /* 1 compás */
    btn(BTN_REC, 30);
    CHECK(app.ctx.st.loop_mode[1] == LOOP_ARMED);
    btn(BTN_PLAY, 20);
    run_ms(100);
    CHECK(app.ctx.st.loop_mode[1] == LOOP_REC);
    key(bm_white_key(2), 120); run_ms(300); key(bm_white_key(5), 120);
    run_ms(2600);
    CHECK(app.ctx.st.loop_mode[1] == LOOP_PLAY);
    CHECK(app.pat.loop[1].n >= 4);
    shot("16-loop");
    down(BTN_REC); run_ms(2200); up(BTN_REC); run_ms(60);
    CHECK(app.pat.loop[1].n == 0);                                 /* REC 2 s en LOOP: borra el loop */
    btn(BTN_PLAY, 20);
}

static void test_notes(void)
{
    printf("notas: negras = escala en el paso, acento de 3 niveles, REC sostenido\n");
    boot(1);
    ch(2);
    page(BTN_SEQ, PG_SEQ);
    kdown(bm_white_key(1)); run_ms(400);
    black(0, 40); black(2, 40);                                    /* paso 2: dos notas de la escala */
    enc(ENC_SELECT, 1);                                            /* acento */
    shot("03-seq-hold");
    kup(bm_white_key(1)); run_ms(60);
    int n = 0, acc = 0;
    for (int k = 0; k < BM_EVENTS; k++) {
        const bm_event *e = bm_evc(&app.pat, 2, 1, k);
        if (!bm_event_empty(e)) { n++; if (e->vel == BM_VEL_ACC) acc++; }
    }
    CHECK(n == 2 && acc == 2);
    /* REC sostenido: graba solo mientras lo sostienes */
    btn(BTN_PLAY, 20); run_ms(200);
    down(BTN_REC); run_ms(400);
    CHECK(app.rec);
    up(BTN_REC); run_ms(60);
    CHECK(!app.rec);
    btn(BTN_PLAY, 20);
}

/* ------------------------------------------------------------ guardado y versiones */
static void test_demo_and_persist(void)
{
    printf("demo: audio, autoguardado y recarga\n");
    boot(1);
    peak_abs = 0;
    wav_open("demo.wav");
    btn(BTN_PLAY, 20);
    int clips = 0;
    for (int i = 0; i < 500; i++) { run_ms(34); if (app.clip_until > now_ms) clips++; }
    wav_close();
    printf("  saturación: %d de 500 cuadros\n", clips);
    CHECK(clips < 25);
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
    CHECK(app.pat.trk[4].engine == ENG_MONO);
}

static void test_old_version(void)
{
    printf("0.3 no lee lo de 0.2: borra y arranca con la demo\n");
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static bm_store st;
    bm_store_mount(&st, pf.flash);
    uint8_t old[32]; memset(old, 0, sizeof old); old[0] = 2;     /* ajustes de 0.2 (ver 2) */
    bm_store_write_glob(&st, old, sizeof old);
    uint8_t pat[300]; memset(pat, 0x11, sizeof pat);
    bm_store_write(&st, 0, pat, sizeof pat);
    bm_store_write(&st, 7, pat, sizeof pat);
    boot(0);
    CHECK(!bm_store_has(&app.store, 7));
    CHECK(app.pat.trk[1].source == BM_SRC_ROM && app.pat.tr[0].st[0][0] & ST_ON);   /* la demo */
}

static void test_spi_budget(void)
{
    printf("pantalla: bytes por frame en SEQ sonando\n");
    boot(1);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
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
    ch(1);
    sum = 0;
    for (int i = 0; i < 60; i++) { sim.spi_bytes = 0; run_ms(33); sum += sim.spi_bytes; }
    printf("  sampler SEQ: promedio %u B/frame\n", sum / 60);
    CHECK(sum / 60 < 37000);
    btn(BTN_PLAY, 20);
}

static void test_ui_flow(void)
{
    printf("interfaz: recorrido de pantallas (capturas)\n");
    boot(1);
    btn(BTN_PLAY, 20);
    run_ms(900);
    shot("01-home-play");
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    black(4, 30);                                                   /* negra 5: SNARE */
    CHECK(app.sel_lane[0] == 4);
    int k = bm_white_key(4);
    kdown(k); run_ms(400);
    bm_app_encoder(&app, ENC_K2, 5, now_ms); run_ms(60);
    bm_app_encoder(&app, ENC_ALGO, 1, now_ms); run_ms(60);
    shot("03-seq-plock");
    kup(k); run_ms(60);
    CHECK(bm_tr_lock_get(&app.pat.tr[0], 4, 4, 1) >= 0);
    CHECK(((app.pat.tr[0].st[4][4] & ST_RAT_MASK) >> ST_RAT_SH) == 1);
    down(BTN_SEQ); run_ms(1200);
    shot("04-hold-ring");
    run_ms(1000); up(BTN_SEQ); run_ms(60);
    CHECK(app.view == V_TIME);
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1); enc(ENC_K2, 1);
    CHECK(app.pat.trk[2].speed == SPD_X3_2);
    shot("05-time");
    ch(2);
    enc(ENC_K1, -6);
    shot("07-overlay");
    run_ms(1700);
    ch(0);
    page(BTN_EDIT, PG_SOUND);
    shot("08-kit");
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_INST);
    shot("09-inst");
    ch(3);
    btn(BTN_ENV, 30);
    shot("13-env");
    page(BTN_SCL, PG_SCL);
    key(bm_black_key(3), 100);
    enc(ENC_K2, 1);                                                 /* escala propia del canal */
    CHECK(app.pat.trk[3].scl == 0);
    shot("15-scl");
    btn(BTN_SCL, 30);
    CHECK(app.page == PG_CHORDS);
    kdown(bm_white_key(5)); run_ms(150);
    CHECK(app.chord_name[0] != 0);
    shot("17-chords");
    kup(bm_white_key(5)); run_ms(60);
    down(BTN_ARP); run_ms(400);
    CHECK(app.layer == L_ARP);
    up(BTN_ARP); run_ms(60);
    btn(BTN_ARP, 30);
    shot("26-arp");
    ch(0);
    down(BTN_ARP); run_ms(400);
    kdown(bm_white_key(2)); run_ms(300);
    shot("22-roll");
    kup(bm_white_key(2)); up(BTN_ARP); run_ms(100);
    btn(BTN_HOME, 30); btn(BTN_HOME, 30);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_SCENES);
    white(5, 40);
    white(0, 40);
    shot("23-scenes");
    run_ms(9000);
    CHECK(app.ctx.st.scene == 0);
    down(BTN_HOME); run_ms(2200); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SYSTEM);
    shot("25-system");
    btn(BTN_SAVE, 30);
    shot("27-save");
    down(BTN_HOME); run_ms(40); btn(BTN_SEQ, 30); up(BTN_HOME); run_ms(40);
    white(0, 30); white(1, 30);
    shot("28-song");
    btn(BTN_PLAY, 20);
    bm_platform pf; bm_sim_platform(&sim, &pf);
    now_ms += 10; next_tick = now_ms; bm_app_init(&app, &pf, now_ms); run_ms(300);
    shot("29-boot");
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) outdir = argv[1];
    bm_tables_init();
    test_pack();
    test_store();
    test_takes();
    test_gesture();
    test_timing();
    test_tr_features();
    test_voices();
    test_navigation();
    test_home();
    test_layers();
    test_free_engines();
    test_sampler3();
    test_recorder();
    test_loop();
    test_notes();
    test_demo_and_persist();
    test_old_version();
    test_spi_budget();
    test_ui_flow();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
