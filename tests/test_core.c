/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pruebas del núcleo en PC (interfaz 3.0).
 * Uso: test_core [carpeta_salida]  -> escribe demo.wav y capturas *.ppm */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_app_int.h"
#include "../core/bm_util.h"
#include "../core/bm_rom.h"
#include "../core/bm_drums.h"
#include <stddef.h>

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

/* Arranque con una toma de prueba: un compás a 161 BPM hecho aquí (bombo en 1 y 3, caja en 2 y 4,
 * hats en corcheas), como si se hubiera grabado. Mide como la toma de prueba: un compás a 161 BPM. */
static void boot_loop(void)
{
    static int8_t d[23851];
    memset(d, 0, sizeof d);
    for (int b = 0; b < 8; b++) {
        int at = b * 2981;
        for (int i = 0; i < 1500; i++) {
            float e = 1.0f - i / 1500.0f, v;
            if (b % 2 == 0 && (b / 2) % 2 == 0) v = 100.0f * e * e * (float)sin(6.2831853 * 60.0 * i / 16000.0);       /* bombo */
            else if (b % 2 == 0) v = 70.0f * e * e * e * (float)(((i * 7919) % 200) - 100) / 100.0f;                    /* caja */
            else v = 30.0f * e * e * e * e * (float)(((i * 104729) % 200) - 100) / 100.0f;                             /* hat */
            d[at + i] = (int8_t)v;
        }
    }
    bm_sim_flash_erase_all(&sim); sim.flash_writes = 0;
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    bm_take tk[BM_TAKES];
    memset(tk, 0, sizeof tk);
    bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, sizeof d, 1610, 1);
    boot(0);
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
    p.tr[1].st[3][2] = ST_ON; p.tr[1].st[0][6] = ST_ON | ST_ACC;          /* T2: una línea por pad */
    p.pad[3].src = 2; p.pad[3].start = 1000; p.pad[3].len = 500; p.pad[3].mode = PAD_LOOP; p.pad[3].flags = PAD_REV;
    p.tr[1].lane[5].len = 12; p.tr[0].mute = 0x0104;
    CHECK(bm_tr_lock_set(&p.tr[0], 9, 9, 2, 33));
    CHECK(bm_tr_lock_get(&p.tr[0], 9, 9, 2) == 33);
    CHECK(p.tr[0].st[9][9] & ST_LOCK);
    bm_track_set_engine(&p, 4, ENG_KIT);                              /* T5 KIT: rejilla en la unión */
    CHECK(bm_is_tr(&p, 4) && !bm_is_tr(&p, 1) && bm_is_grid(&p, 1) && bm_kind(&p, 2) == KIND_NOTE);
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
    CHECK(q.tr[1].st[0][6] == (ST_ON | ST_ACC) && q.tr[1].st[3][2] == ST_ON);
    CHECK(memcmp(&q.pad[3], &p.pad[3], sizeof p.pad[3]) == 0 && !bm_pad_ok(&q.pad[0]) && bm_pad_ok(&q.pad[3]));
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
    CHECK(bm_takes_room(tk) == BM_SMP_REC_MAX && full == BM_SMP_REC_MAX);   /* la zona es una toma */
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, BM_SMP_REC_MAX + 1, 1600, 1) == -1);
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, 20000, 1600, 1) == 0);     /* 5 sectores */
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, 3000, 1200, 1) == 1);      /* el último */
    CHECK(tk[0].valid && tk[1].valid && tk[1].len == 3000 && tk[1].data[123] == d[123] && tk[1].gain == 64);
    CHECK(bm_takes_free(tk) == 0 && bm_takes_room(tk) == 0);
    tk[1].nmarks = 3; tk[1].marks[0] = 0; tk[1].marks[1] = 1000; tk[1].marks[2] = 2000; tk[1].gain = 80;
    static uint8_t scratch[BM_SECTOR];
    CHECK(bm_takes_update(pf.flash, &tk[1], scratch) == 0);
    static bm_take tk2[BM_TAKES];
    CHECK(bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk2) == 2);
    CHECK(tk2[1].nmarks == 3 && tk2[1].marks[2] == 2000 && tk2[1].gain == 80 && tk2[1].bpm_x10 == 1200 && tk2[1].data[999] == d[999]);
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk2, d, 16000, 1200, 1) < 0);    /* llena */
    printf("  la zona guarda %u.%02u s (una toma)\n", (unsigned)(full / BM_SMP_RATE), (unsigned)(full * 100u / BM_SMP_RATE % 100u));
    CHECK(bm_takes_erase(pf.flash, &tk2[0]) == 0);
    CHECK(bm_takes_room(tk2) == 5u * BM_SECTOR - BM_TAKE_HDR);                 /* el hueco vuelve */
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
    for (int i = 1; i < 8; i++) { double d = (double)(at[i] - at[i - 1]); double e = BM_FS * 0.125; CHECK(d > e - 2 && d < e + 2); }   /* 1/16 a 120 BPM */
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
    enc(ENC_ALGO, 1);                                         /* dentro de un canal: el recuadro, y entra conservando la página */
    CHECK(app.box_on && app.box_mode == BOX_CH && app.box_track == 3 && app.track == 2);
    shot("62-box-canal");
    run_ms(500);
    CHECK(!app.box_on && app.track == 3 && app.page == PG_SOUND && app.view == V_CH);
    shot("12-wave");
    enc(ENC_SELECT, 1);                                       /* SELECT: las páginas de SONIDO y después las pestañas */
    CHECK(app.page == PG_SOUND && app.ed_page[3] == 1);
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
    int sw = app.pat.meta.swing;
    enc(ENC_SELECT, 1); enc(ENC_ALGO, 1);                     /* SWING +1 */
    CHECK(app.pat.meta.swing == sw + 1);
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

/* Negras 10 y 11 en la batería: un toque deja abierta la pantalla de MUTE o de FILL; mantener = solo
 * mientras la sostienes. En INSTR, ALGORITHM elige instrumento con el recuadro. */
static void test_key_screens(void)
{
    printf("negras 10 y 11: pantallas de MUTE y FILL; ALGORITHM en INSTR\n");
    boot(1);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    app.sel_lane[0] = 2;                                         /* CL HH */
    black(9, 40);                                                /* un toque: queda abierta */
    CHECK(app.layer == L_LANEMUTE && app.layer_latch);
    shot("63-mutes");
    white(2, 40);                                                /* mutea el hat cerrado */
    CHECK((app.pat.tr[0].mute >> 2) & 1);
    white(2, 40);
    CHECK(!((app.pat.tr[0].mute >> 2) & 1));
    black(9, 40);                                                /* la misma tecla: sale */
    CHECK(app.layer == L_NONE && !app.layer_latch);
    kdown(bm_black_key(9)); run_ms(500);                         /* mantenida: solo mientras la sostienes */
    CHECK(app.layer == L_LANEMUTE);
    white(3, 40);
    CHECK((app.pat.tr[0].mute >> 3) & 1);
    kup(bm_black_key(9)); run_ms(40);
    CHECK(app.layer == L_NONE);
    black(9, 40); white(3, 40); btn(BTN_HOME, 30);               /* HOME también cierra, y no cambia de vista */
    CHECK(app.layer == L_NONE && !((app.pat.tr[0].mute >> 3) & 1) && app.view == V_CH);
    /* FILL: suena mientras está abierta y las blancas arman los golpes del fill */
    black(10, 40);
    CHECK(app.layer == L_FILL && app.fill && app.ctx.fill);
    app.step_page = 0;
    white(5, 40);                                                /* paso 6 vacío: golpe solo en el fill */
    CHECK(app.pat.tr[0].st[2][5] == (ST_ON | (1 << ST_COND_SH)));
    enc(ENC_K1, -1);                                             /* FILL AUTOMATICO: de cada 4 compases (demo) a cada 2 */
    CHECK(app.pat.meta.fill_every == 2);
    shot("64-fill");
    white(5, 40);                                                /* otra vez: se borra */
    CHECK(app.pat.tr[0].st[2][5] == 0);
    black(10, 40);
    CHECK(app.layer == L_NONE && !app.fill && !app.ctx.fill);
    /* INSTR: ALGORITHM recorre los instrumentos con el recuadro */
    page(BTN_EDIT, PG_SOUND); btn(BTN_EDIT, 30);
    CHECK(app.page == PG_INST);
    int l0 = app.sel_lane[0];
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.box_on && app.box_mode == BOX_LANE && app.box_lane == l0 + 2 && app.sel_lane[0] == l0);
    shot("65-box-instrumento");
    run_ms(500);
    CHECK(!app.box_on && app.sel_lane[0] == l0 + 2 && app.track == 0 && app.page == PG_INST);
    shot("66-home-guia");
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
    /* REC 2 s: borra el canal sin preguntar (HOME + OCT- lo deshace) */
    ch(0);
    down(BTN_REC); run_ms(2200); up(BTN_REC); run_ms(60);
    CHECK(app.ask == ASK_NONE && app.view == V_CH);
    any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(!any);
}

static void test_free_engines(void)
{
    printf("canales libres: MAS > MOTOR (KIT, MIDI) y canales fijos\n");
    boot(1);
    ch(3);
    enc(ENC_SELECT, -1);
    CHECK(app.page == PG_MORE && app.more_row == 0);
    enc(ENC_ALGO, 1);                                         /* ANALOG > FM: notas a notas, sin preguntar */
    CHECK(app.pat.trk[3].engine == ENG_FM && app.ask == ASK_NONE);
    enc(ENC_ALGO, 1);                                         /* > ACID */
    CHECK(app.pat.trk[3].engine == ENG_ACID && app.ask == ASK_NONE);
    enc(ENC_ALGO, 1);                                         /* ACID > KIT: pregunta */
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
    enc(ENC_ALGO, -1);                                         /* ANALOG < MIDI: las notas se quedan */
    CHECK(app.pat.trk[5].engine == ENG_MIDI && app.page == PG_MORE && app.ask == ASK_NONE);
    page(BTN_EDIT, PG_MIDI);
    shot("48-midi");
    sim.midi_n = 0;
    btn(BTN_PLAY, 20); run_ms(1500);
    int on = 0;
    for (int i = 0; i + 2 < sim.midi_n; i++) if (sim.midi_out[i] == (0x90 | 5)) on = 1;
    CHECK(on);
    btn(BTN_PLAY, 20);
    /* T3 elige entre BAJO y ACID; la batería no tiene MOTOR */
    ch(2);
    enc(ENC_SELECT, -1);
    const char *nm; char v[24];
    bm_app_more_row(&app, 0, &nm, v, sizeof v);
    CHECK(strcmp(nm, "ENGINE") == 0 && strcmp(v, "ACID") == 0);
    ch(0);
    enc(ENC_SELECT, -1);
    bm_app_more_row(&app, 0, &nm, v, sizeof v);
    CHECK(strcmp(nm, "ENGINE") != 0);
}

/* ------------------------------------------------------------ sampler de pads */
static int t2_voices(int *gated, uint32_t *pos)
{
    int n = 0;
    if (gated) *gated = 0;
    for (int i = 0; i < BM_VOICES; i++) {
        const bm_voice *v = &app.ctx.voice[i];
        if (!v->active || v->track != 1) continue;
        n++;
        if (gated && v->gate) (*gated)++;
    }
    if (pos) *pos = app.ctx.st.smp_pos;
    return n;
}

static void pad_mode(int pad, int mode)                       /* PADS > SOUND > K3 hasta el modo */
{
    app.pat.pad[pad].mode = (uint8_t)mode;
}

static void test_pads(void)
{
    printf("sampler de pads: kit desde una toma, rejilla, modos de tecla, TRIM, BREAK y captura\n");
    boot_loop();
    const bm_take *tk = &app.take[0];
    CHECK(tk->valid && tk->len == 23851);
    /* la demo arma el kit desde la toma (en los golpes) y la rejilla la toca en orden */
    int n = 0;
    for (int i = 0; i < BM_LANES; i++) n += bm_pad_ok(&app.pat.pad[i]);
    CHECK(n >= 6 && n <= 8 && app.pat.pad[0].src == 0 && app.pat.pad[0].start == 0);
    CHECK(app.pat.pad[1].start > 2500 && app.pat.pad[1].start < 3500);      /* el 2.o golpe del loop, a los 2981 */
    CHECK((app.pat.tr[1].st[0][0] & ST_ON) && (app.pat.tr[1].st[1][2] & ST_ON) && app.pat.tr[1].lane[0].len == 16);
    CHECK(app.pat.tr[1].lane[0].choke == 1 && (app.pat.pad[0].flags & PAD_TEMPO));
    ch(1);
    CHECK(app.page == PG_SEQ && app.sel_lane[1] == 0);
    /* suena: la rejilla dispara los pads */
    btn(BTN_PLAY, 20);
    uint32_t seen = 0;
    for (int j = 0; j < 60; j++) { run_ms(25); for (int l = 0; l < BM_LANES; l++) if (app.lane_flash[1][l] > now_ms) seen |= 1u << l; }
    int distinct = 0;
    for (int l = 0; l < BM_LANES; l++) distinct += (seen >> l) & 1;
    CHECK(distinct >= 4);
    for (int k = 0; k < 16; k++) {                              /* BREAK apagado: suena la rejilla tal cual */
        int main = -1;
        for (int l = 0; l < BM_LANES && main < 0; l++) if (app.pat.tr[1].st[l][k] & ST_ON) main = l;
        CHECK(app.ctx.st.gen_sl[k] == (main < 0 ? 0xFF : main) && app.ctx.st.gen_fx[k] == 0);
    }
    run_ms(300);
    shot("31-sampler-seq");
    /* SEQ como la batería: negra elige y toca el pad, blanca pone el paso en su línea */
    black(2, 40);
    CHECK(app.sel_lane[1] == 2);
    white(5, 40);
    CHECK(app.pat.tr[1].st[2][5] & ST_ON);
    white(5, 40);
    CHECK(!(app.pat.tr[1].st[2][5] & ST_ON));
    /* paso sostenido + PITCH: P-Lock en ese paso, como en la batería */
    kdown(bm_white_key(2)); run_ms(400);
    enc(ENC_K2, 2);
    kup(bm_white_key(2)); run_ms(60);
    CHECK(bm_tr_lock_get(&app.pat.tr[1], 2, 2, 1) == 66);
    /* BREAK (SEQ otra vez): VARY cambia los pads; CAPT lo deja escrito en la rejilla */
    btn(BTN_SEQ, 30);
    CHECK(app.page == PG_BREAK);
    enc(ENC_K1, 127); enc(ENC_K3, 60);
    run_ms(600);
    int diff = 0;
    for (int k = 0; k < 16; k++) diff += app.ctx.gen_sl[k] != 0xFF || app.ctx.gen_fx[k] != 0;
    CHECK(diff > 0);
    run_ms(UI_OVERLAY_MS + 100);
    shot("31a-break");
    static uint8_t before[BM_LANES][BM_STEPS];
    memcpy(before, app.pat.tr[1].st, sizeof before);
    down(BTN_EDIT); run_ms(400); black(3, 40); up(BTN_EDIT); run_ms(100);
    CHECK(memcmp(before, app.pat.tr[1].st, sizeof before) != 0);
    for (int i = 0; i < 4; i++) { enc(ENC_K1, -127); enc(ENC_K2, -127); enc(ENC_K3, -127); enc(ENC_K4, -127); }
    CHECK(!app.pat.trk[1].gen[0] && !app.pat.trk[1].gen[2]);
    btn(BTN_PLAY, 20);
    /* PADS (EDIT): las blancas tocan y eligen el pad */
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_PADS && app.ed_page[1] == 0);
    white(4, 40);
    CHECK(app.sel_lane[1] == 4);
    run_ms(200);
    /* ONESHOT: suena entero aunque se suelte enseguida, y termina en su final */
    pad_mode(1, PAD_ONESHOT);
    uint32_t e1 = (uint32_t)app.pat.pad[1].start + app.pat.pad[1].len, far = 0;
    kdown(bm_white_key(1)); run_ms(10); kup(bm_white_key(1));
    run_ms(20);
    CHECK(t2_voices(NULL, NULL) == 1);
    for (int j = 0; j < 80; j++) { run_ms(5); if (app.ctx.st.smp_pos_src == 0 && app.ctx.st.smp_pos > far) far = app.ctx.st.smp_pos; }
    CHECK(far < e1 && far > e1 - 1500 && t2_voices(NULL, NULL) == 0);
    /* KEY: suena mientras se sostiene */
    pad_mode(1, PAD_KEY);
    int gated;
    kdown(bm_white_key(1)); run_ms(60);
    CHECK(t2_voices(&gated, NULL) == 1 && gated == 1);
    kup(bm_white_key(1)); run_ms(300);
    CHECK(t2_voices(NULL, NULL) == 0);
    /* THRU: sostenido sigue por la toma más allá del pad */
    pad_mode(1, PAD_THRU);
    kdown(bm_white_key(1)); run_ms(600);
    CHECK(t2_voices(NULL, NULL) == 1 && app.ctx.st.smp_pos > e1 + 2000);
    shot("38-pads-thru");
    kup(bm_white_key(1)); run_ms(400);                       /* ya pasó el pad: se apaga con su release */
    CHECK(t2_voices(NULL, NULL) == 0);
    /* LOOP: sostenido repite el pad */
    pad_mode(1, PAD_LOOP);
    kdown(bm_white_key(1)); run_ms(900);
    CHECK(t2_voices(NULL, NULL) == 1 && app.ctx.st.smp_pos >= app.pat.pad[1].start && app.ctx.st.smp_pos < e1);
    kup(bm_white_key(1)); run_ms(400);
    CHECK(t2_voices(NULL, NULL) == 0);
    /* SOUND (EDIT otra vez): K3 = PLAY cambia el modo; ENV: K4 = TEMPO */
    white(1, 40);
    btn(BTN_EDIT, 30);
    CHECK(app.ed_page[1] == 1);
    enc(ENC_K3, 1);
    CHECK(app.pat.pad[1].mode == PAD_ONESHOT);
    enc(ENC_K1, 1); enc(ENC_K1, 1); enc(ENC_K1, 1);
    CHECK(app.pat.pad[1].pitch == 27);
    btn(BTN_EDIT, 30);
    CHECK(app.ed_page[1] == 2);
    enc(ENC_K4, 1);
    CHECK(!(app.pat.pad[1].flags & PAD_TEMPO));
    run_ms(UI_TOAST_MS);
    shot("37c-pads-env");
    btn(BTN_EDIT, 30);
    CHECK(app.ed_page[1] == 0);
    /* TRIM: K1 mueve el inicio (el final queda), K2 el final, K3 zoom */
    uint32_t s1 = app.pat.pad[1].start, end1 = s1 + app.pat.pad[1].len;
    enc(ENC_K1, 2);
    CHECK(app.pat.pad[1].start > s1 && (uint32_t)app.pat.pad[1].start + app.pat.pad[1].len == end1);
    enc(ENC_K2, -2);
    CHECK((uint32_t)app.pat.pad[1].start + app.pat.pad[1].len < end1);
    enc(ENC_K3, 1);
    run_ms(60);
    CHECK(app.cut_v1 - app.cut_v0 == tk->len / 2);
    /* HIT (N4): el inicio vuelve al golpe */
    black(3, 40);
    CHECK(app.pat.pad[1].start > 2600 && app.pat.pad[1].start < 3300);   /* el golpe, a los 2981 (con su ventana antes) */
    run_ms(UI_TOAST_MS + 100);
    shot("37-pads");
    enc(ENC_K3, -3);
    /* N1 copia, N2 pega en otro pad, N3 lo vacía */
    black(0, 40);
    white(12, 40);
    CHECK(app.sel_lane[1] == 12 && !bm_pad_ok(&app.pat.pad[12]));
    shot("37d-pads-empty");
    black(1, 40);
    CHECK(memcmp(&app.pat.pad[12], &app.pat.pad[1], sizeof app.pat.pad[1]) == 0);
    black(2, 40);
    CHECK(!bm_pad_ok(&app.pat.pad[12]));
    /* PRESETS: la toma entera en el pad elegido */
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.pad[12].src == 0 && app.pat.pad[12].start == 0 && app.pat.pad[12].len == tk->len);
    /* negras 9-11 en vivo */
    kdown(bm_black_key(8)); run_ms(40);
    CHECK(app.ctx.brk_live & 1);
    kup(bm_black_key(8)); run_ms(40);
    CHECK(!(app.ctx.brk_live & 1));
    /* REC en PADS + PLAY: lo que tocas queda en la línea del pad */
    btn(BTN_REC, 30);
    CHECK(app.rec && app.page == PG_PADS);
    btn(BTN_PLAY, 20); run_ms(300);
    white(6, 40);
    btn(BTN_PLAY, 20); btn(BTN_REC, 30);
    int rec6 = 0;
    for (int s2 = 0; s2 < 16; s2++) rec6 += (app.pat.tr[1].st[6][s2] & ST_ON) != 0;
    CHECK(rec6 >= 1);
    /* MAS, BREAK y TAKES con SELECT */
    enc(ENC_SELECT, 1);
    CHECK(app.page == PG_BREAK);
    enc(ENC_SELECT, 1);
    CHECK(app.page == PG_TAKES);
}

static void test_recorder(void)
{
    printf("grabadora: armada, al compás, toma sin guardar, kit, TAKES\n");
    boot(1);
    CHECK(app_kit_empty(&app));
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
    uint32_t len = app.ctx.st.smp_len;
    CHECK(len > 16000);
    kdown(bm_white_key(0)); run_ms(60);                      /* las blancas escuchan la toma nueva */
    int heard = 0;
    for (int i = 0; i < BM_VOICES; i++) heard |= app.ctx.voice[i].active && app.ctx.voice[i].track == 1 && app.ctx.voice[i].src == BM_TAKE_NEW;
    kup(bm_white_key(0)); run_ms(60);
    CHECK(heard);
    shot("35-rec-unsaved");
    btn(BTN_PLAY, 20);                                       /* parar: se guarda y, con el kit vacío, lo arma */
    run_ms(800);
    CHECK(!app.ctx.st.smp_unsaved && !app.ctx.arena_busy);
    CHECK(app.take[0].valid && app.take[0].len == len);
    CHECK(bm_pad_ok(&app.pat.pad[0]) && app.pat.pad[0].src == 0);
    /* TAKES (EDIT 2 s en el sampler) */
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
    /* N2: un kit de 16 partes iguales; el kit ya tenía pads, así que pregunta */
    black(1, 40);
    CHECK(app.ask == ASK_KIT);
    btn(BTN_OCT_UP, 20); run_ms(60);
    int pads = 0;
    for (int i = 0; i < BM_LANES; i++) pads += bm_pad_ok(&app.pat.pad[i]);
    CHECK(pads == 16 && app.pat.pad[1].start == len / 16);
    /* N4: la toma entera en el pad elegido */
    app.sel_lane[1] = 5;
    black(3, 40);
    CHECK(app.pat.pad[5].start == 0 && app.pat.pad[5].len == len);
    btn(BTN_EDIT, 30);                                       /* EDIT: a PADS */
    CHECK(app.page == PG_PADS);
    btn(BTN_HOME, 30);
    run_ms(2200);                                            /* en reposo se guarda la toma (la ganancia) */
    CHECK(!app.take_dirty);
    run_ms(UI_AUTOSAVE_MS);                                  /* y el patrón con su kit */
    boot(0);
    CHECK(app.take[0].valid && app.take[0].gain == 66);
    CHECK(app.pat.pad[5].len == len && app.pat.pad[1].start == len / 16);   /* el kit sobrevive al reinicio */
    /* borrar la toma: pregunta, y sus pads quedan vacíos */
    ch(1);
    down(BTN_EDIT); run_ms(2200); up(BTN_EDIT); run_ms(60);
    for (int i = 0; i < 12 && bm_app_takes_sel(&app) != 0; i++) enc(ENC_ALGO, 1);
    black(10, 40);
    CHECK(app.ask == ASK_TAKE_DEL);
    btn(BTN_OCT_UP, 20); run_ms(600);
    CHECK(!app.take[0].valid && app_kit_empty(&app));
    /* HOME descarta una toma sin guardar */
    btn(BTN_REC, 30); btn(BTN_PLAY, 20); run_ms(800); btn(BTN_REC, 30);
    CHECK(app.ctx.st.smp_unsaved);
    btn(BTN_HOME, 30); run_ms(60);
    CHECK(!app.ctx.st.smp_unsaved && app.ctx.smp[0].len == 0);
    btn(BTN_PLAY, 20);
}

static void test_free_rec(void)
{
    printf("grabación libre: el largo se cierra en compases y cada nota guarda lo que se sostuvo\n");
    boot(1);
    ch(5);
    down(BTN_REC); run_ms(2200); up(BTN_REC); run_ms(60);       /* vaciar T6 */
    btn(BTN_OCT_UP, 20);
    page(BTN_EDIT, PG_SOUND);
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    CHECK(app.rec && app.rec_free == 6 && app.pat.trk[5].length == 64);
    uint32_t bar = 60000u * 4u / 160u;                           /* ms por compás a 160 BPM */
    key(bm_white_key(4), 600);                                  /* nota larga */
    run_ms(bar / 2);
    key(bm_white_key(7), 150);
    run_ms(bar + bar / 4);
    shot("51-seq-free-rec");
    btn(BTN_REC, 30);                                           /* cierra: unos 2 compases */
    run_ms(100);
    CHECK(!app.rec && !app.rec_free && app.pat.trk[5].length == 32);
    int n = 0, longest = 0;
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&app.pat, 5, s, k);
            if (bm_event_empty(e)) continue;
            n++;
            if ((int)(e->len & BM_LEN_MASK) > longest) longest = (int)(e->len & BM_LEN_MASK);
        }
    CHECK(n >= 2 && longest >= 4);                              /* la nota larga dura lo que se sostuvo */
    page(BTN_SEQ, PG_SEQ);
    shot("52-seq-notes-roll");
    btn(BTN_PLAY, 20);
}

/* Hace sonar un canal solo y devuelve el pico (0..1). */
static double solo_peak(int t, const uint8_t *notes, int n, int vel, uint32_t ms)
{
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != t);
    bm_cmd c; memset(&c, 0, sizeof c);
    run_ms(300);
    peak_abs = 0;
    for (int i = 0; i < n; i++) {
        c.type = CMD_NOTE_ON; c.a = (uint8_t)t; c.b = notes[i]; c.c = (uint8_t)vel; bm_cmd_push(&app.cmdq, &c);
        run_ms(ms);
        c.type = CMD_NOTE_OFF; bm_cmd_push(&app.cmdq, &c);
        run_ms(60);
    }
    return peak_abs;
}

static void test_fm(void)
{
    printf("motor FM: los 16 patches suenan, sin saturar, y las voces se liberan al soltar\n");
    boot(1);
    ch(3);
    enc(ENC_SELECT, -1);
    enc(ENC_ALGO, 1);                                           /* ANALOG > FM */
    CHECK(app.pat.trk[3].engine == ENG_FM && app.ask == ASK_NONE);
    page(BTN_EDIT, PG_SOUND);
    shot("56-fm");
    static const uint8_t notes[3] = {48, 60, 67};
    wav_open("fm.wav");
    int ok = 0;
    for (int p = 0; p < BM_FM_PATCHES; p++) {
        app.pat.trk[3].source = (uint8_t)p;
        double pk = solo_peak(3, notes, 3, 110, 450);
        printf("  %-8s pico %.2f\n", bm_fm_names[p], pk);
        if (pk > 0.03 && pk < 0.99) ok++;
    }
    run_ms(4000);
    wav_close();
    CHECK(ok == BM_FM_PATCHES);
    int vivas = 0;
    for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) vivas++;
    CHECK(vivas == 0);
    /* T3 en FM: mono */
    ch(2);
    enc(ENC_SELECT, -1);
    enc(ENC_ALGO, 1);                                           /* ACID > FM */
    CHECK(app.pat.trk[2].engine == ENG_FM);
    static const uint8_t bass[2] = {36, 43};
    CHECK(solo_peak(2, bass, 2, 100, 250) > 0.03);
}

static void test_engines(void)
{
    printf("motores 0.4: ANALOG (presets y editor), ACID y la batería modelada\n");
    boot(1);
    static const uint8_t bass[4] = {36, 43, 48, 39};
    CHECK(app.pat.trk[2].engine == ENG_ACID);                    /* la demo: acid techno */
    ch(2);
    enc(ENC_SELECT, -1);
    enc(ENC_ALGO, -1);                                           /* T3: ACID > ANALOG, sin preguntar */
    CHECK(app.pat.trk[2].engine == ENG_ANALOG && app.pat.prm[2][AN_MODE] == AN_M_MONO && app.ask == ASK_NONE);
    double p_bass = solo_peak(2, bass, 4, 100, 250);
    enc(ENC_ALGO, 1);                                            /* y otra vez ACID */
    CHECK(app.pat.trk[2].engine == ENG_ACID && app.ask == ASK_NONE);
    page(BTN_EDIT, PG_SOUND);
    shot("53-acid");
    double p_acid = solo_peak(2, bass, 4, 100, 250);
    double p_acc = solo_peak(2, bass, 4, 127, 250);
    printf("  pico ANALOG %.2f · ACID %.2f · ACID con acento %.2f\n", p_bass, p_acid, p_acc);
    CHECK(p_bass > 0.05 && p_bass < 1.0);
    CHECK(p_acid > 0.05 && p_acid < 1.0 && p_acc >= p_acid);
    enc(ENC_PRESETS, 1);                                         /* cuadrada */
    CHECK(app.pat.trk[2].source == 1);
    CHECK(solo_peak(2, bass, 2, 100, 250) > 0.05);

    /* ANALOG en T4: los presets suenan sin saturar */
    ch(3);
    page(BTN_EDIT, PG_SOUND);
    CHECK(app.pat.trk[3].engine == ENG_ANALOG && bm_app_ed_on(&app));
    shot("54-analog");
    static const uint8_t chord[3] = {57, 60, 64};
    wav_open("analog.wav");
    int ok = 0;
    for (int pr = 0; pr < bm_an_npresets; pr++) {
        while (app.pat.trk[3].source != pr) { enc(ENC_PRESETS, 1); run_ms(20); }
        double pk = solo_peak(3, chord, 3, 100, 600);
        printf("  %-9s pico %.2f\n", bm_an_presets[pr].name, pk);
        if (pk > 0.03 && pk < 1.0) ok++;
    }
    run_ms(3000);
    wav_close();
    CHECK(ok == bm_an_npresets);
    /* editor: SELECT recorre las páginas, K1 cambia el ajuste de la página, EDIT pasa a la siguiente */
    app.ed_page[3] = 0;
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1); enc(ENC_SELECT, 1);   /* OSC 1 > OSC 2 > CRUCE > FILTRO */
    CHECK(app.page == PG_SOUND && app.ed_page[3] == AN_PG_FILTER);
    shot("57-analog-filtro");
    int ft = app.pat.prm[3][AN_FTYPE];
    enc(ENC_K1, 1); run_ms(20);                                  /* TIPO: LP 24 > LP 12 */
    CHECK(app.pat.prm[3][AN_FTYPE] == ft + 1);
    int cut = app.pat.trk[3].macro[0];
    enc(ENC_K2, -2); run_ms(20);                                 /* CORTE es la macro 1 */
    CHECK(app.pat.trk[3].macro[0] == cut - 2);
    btn(BTN_ENV, 20); run_ms(20);                                /* ENV: la envolvente de nivel */
    CHECK(app.page == PG_SOUND && app.ed_page[3] == AN_PG_AMP);
    shot("58-analog-env");
    btn(BTN_ENV, 20); run_ms(20);                                /* otra vez: la de filtro */
    CHECK(app.ed_page[3] == AN_PG_FILT);
    btn(BTN_LFO, 20); run_ms(20);
    CHECK(app.ed_page[3] == AN_PG_LFO);
    shot("59-analog-lfo");
    app.ed_page[3] = BM_AN_PAGES - 1;
    enc(ENC_SELECT, 1);                                          /* desde la última página, a la pestaña siguiente */
    CHECK(app.page != PG_SOUND);

    /* batería modelada: los 3 kits, los 16 instrumentos de cada uno suenan */
    ch(0);
    page(BTN_EDIT, PG_SOUND);
    shot("55-bateria");
    wav_open("bateria.wav");
    for (int k = 0; k < BM_KITS_N; k++) {
        while (app.pat.trk[0].source != k) { enc(ENC_PRESETS, 1); run_ms(20); }
        ok = 0;
        for (int l = 0; l < BM_LANES; l++) {
            uint8_t n = (uint8_t)l;
            double pk = solo_peak(0, &n, 1, 110, 300);
            printf(" %.2f", pk);
            if (pk > 0.02 && pk < 1.0) ok++; else printf("  kit %s, %s: pico %.3f\n", bm_kits[k].name, bm_kits[k].lane[l].name, pk);
        }
        printf("  kit %-5s %d/16\n", bm_kits[k].name, ok);
        CHECK(ok == BM_LANES);
    }
    wav_close();
    /* INSTR: el instrumento elegido con su modelo */
    page(BTN_EDIT, PG_INST);
    CHECK(app.ed_page[0] == 0 && bm_app_ed_on(&app));
    shot("60-instrumento");
    int tune = app.pat.tr[0].lane[app.sel_lane[0]].tune;
    enc(ENC_K1, 2); run_ms(20);
    CHECK(app.pat.tr[0].lane[app.sel_lane[0]].tune == tune + 2);
    btn(BTN_EDIT, 20); run_ms(20);                               /* página INSTRUMENTO: el modelo */
    CHECK(app.page == PG_INST && app.ed_page[0] == 1);
    int model = app.pat.tr[0].lane[app.sel_lane[0]].model;
    enc(ENC_K1, 1); run_ms(20);
    CHECK(app.pat.tr[0].lane[app.sel_lane[0]].model == (model + 1) % DRM_COUNT || model == DRM_COUNT - 1);
    shot("61-instrumento-modelo");

    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    peak_abs = 0;
    wav_open("motores.wav");                                    /* la demo con los motores nuevos */
    btn(BTN_PLAY, 20); run_ms(8000); btn(BTN_PLAY, 20);
    wav_close();
    CHECK(peak_abs > 0.05 && peak_abs <= 1.0);
}

/* Hasta la v1 no hay compatibilidad hacia atrás: un patrón de 0.3 (formato 3) no se lee. */
static void test_migrate(void)
{
    printf("patrones de 0.3: no se leen (sin compatibilidad hasta la v1)\n");
    static uint8_t b[BM_PENDING_LEN];
    bm_pattern p, q;
    bm_pattern_init(&p);
    int n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(n > 0 && bm_pattern_unpack(&q, b, n) == n);
    b[offsetof(bm_meta, fmt)] = 3;
    CHECK(bm_pattern_unpack(&q, b, n) < 0);
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
    kdown(bm_white_key(7)); run_ms(400);
    black(0, 40); black(2, 40);                                    /* paso 8 (vacío): dos notas de la escala */
    enc(ENC_SELECT, 1);                                            /* acento */
    shot("03-seq-hold");
    kup(bm_white_key(7)); run_ms(60);
    int n = 0, acc = 0;
    for (int k = 0; k < BM_EVENTS; k++) {
        const bm_event *e = bm_evc(&app.pat, 2, 7, k);
        if (!bm_event_empty(e)) { n++; if (e->vel == BM_VEL_ACC) acc++; }
    }
    CHECK(n == 2 && acc == 2);
    /* capa LARGO: negras 7-10 = compases completos */
    down(BTN_SEQ); run_ms(400); black(7, 40); up(BTN_SEQ); run_ms(60);
    CHECK(app.pat.trk[2].length == 32);
    shot("06b-seq-notes");
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
    CHECK(app.song.scene[1].valid && app.song.scene[1].trk_mute == 0x38);
    CHECK(app.pat.trk[4].engine == ENG_FM && app.pat.trk[2].engine == ENG_ACID && app.pat.meta.bpm_x10 == 1600);
}

static void test_old_version(void)
{
    printf("datos de 0.2: borra y arranca con la demo\n");
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
    CHECK(app.pat.trk[1].source == BM_SRC_NONE && app.pat.tr[0].st[0][0] & ST_ON);   /* la demo (sin tomas) */
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

/* ------------------------------------------------------------ idioma */
static int lang_upload(const uint8_t *p, uint32_t n)
{
    if (bm_app_lang_load(&app, 0, 0, NULL, 0)) return -1;
    for (uint32_t off = 0; off < n; off += 200)                  /* de a pedazos, como por USB */
        if (bm_app_lang_load(&app, 1, off, p + off, n - off < 200 ? n - off : 200)) return -1;
    return bm_app_lang_load(&app, 2, 0, NULL, 0);
}

static void test_lang(void)
{
    printf("idioma: ingles de fabrica, paquete de espanol por USB, SISTEMA > LANGUAGE\n");
    static uint8_t es[BM_LANG_HI - BM_LANG_LO];
    FILE *f = fopen("lang/es.bmlang", "rb");
    CHECK(f != NULL);
    if (!f) return;
    uint32_t n = (uint32_t)fread(es, 1, sizeof es, f);
    fclose(f);
    CHECK(bm_lang_check(es, n, NULL) == n);
    boot(1);
    CHECK(strcmp(bm_lang_name(), "ENGLISH") == 0 && !bm_app_lang_pack(&app));
    CHECK(strcmp(T("TAKES"), "TAKES") == 0);
    shot("25-system-en");
    btn(BTN_PLAY, 20);
    CHECK(lang_upload(es, n) < 0);                               /* sonando no se escribe la flash */
    btn(BTN_PLAY, 20); run_ms(100);
    CHECK(lang_upload(es, n) == 0 && app.set.lang == 1);
    CHECK(strcmp(bm_lang_name(), "ESPANOL") == 0);
    CHECK(strcmp(T("TAKES"), "TOMAS") == 0 && strcmp(T("NOT A PHRASE"), "NOT A PHRASE") == 0);
    char b[32];
    snprintf(b, sizeof b, TF("PAD %d EMPTY"), 7);
    CHECK(strcmp(b, "PAD 7 VACIO") == 0);
    ch(1);
    btn(BTN_EDIT, 30);
    shot("37b-pads-es");
    /* SISTEMA > LANGUAGE: inglés y vuelta */
    btn(BTN_HOME, 30); btn(BTN_HOME, 30);
    down(BTN_HOME); run_ms(2200); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SYSTEM);
    for (int i = 0; i < 9; i++) enc(ENC_SELECT, 1);
    CHECK(app.menu_row == 9);
    shot("25-system-es");
    enc(ENC_ALGO, 1);
    CHECK(app.set.lang == 0 && strcmp(T("TAKES"), "TAKES") == 0);
    enc(ENC_ALGO, 1);
    CHECK(app.set.lang == 1 && strcmp(T("TAKES"), "TOMAS") == 0);
    /* queda guardado */
    run_ms(UI_AUTOSAVE_MS + 500);
    boot(0);
    CHECK(app.set.lang == 1 && strcmp(T("TAKES"), "TOMAS") == 0);
    /* un paquete dañado no se usa */
    sim.flash[BM_LANG_LO + 200] ^= 0x55;
    boot(0);
    CHECK(!bm_app_lang_pack(&app) && strcmp(T("TAKES"), "TAKES") == 0);
    /* quitarlo */
    sim.flash[BM_LANG_LO + 200] ^= 0x55;
    boot(0);
    CHECK(strcmp(T("TAKES"), "TOMAS") == 0);
    CHECK(bm_app_lang_load(&app, 3, 0, NULL, 0) == 0 && app.set.lang == 0 && !bm_app_lang_pack(&app));
    CHECK(strcmp(T("TAKES"), "TAKES") == 0);
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
    test_pads();
    test_recorder();
    test_loop();
    test_notes();
    test_free_rec();
    test_engines();
    test_key_screens();
    test_migrate();
    test_fm();
    test_demo_and_persist();
    test_old_version();
    test_spi_budget();
    test_ui_flow();
    test_lang();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
