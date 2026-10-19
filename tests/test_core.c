/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — pruebas del núcleo en PC (interfaz 3.0).
 * Uso: test_core [carpeta_salida]  -> escribe demo.wav y capturas *.ppm */
#define _GNU_SOURCE                       /* dlsym(RTLD_NEXT): contar las funciones de libm (test_draw_cost) */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "../hal/bm_hal_sim.h"
#include "../core/bm_app_int.h"
#include "../core/bm_util.h"
#include "../core/bm_rom.h"
#include "../core/bm_drums.h"
#include "../core/bm_snap.h"
#include "../core/bm_printf.h"
#include "../core/bm_wt.h"
#include "../core/bm_fm.h"
#include "../core/bm_params.h"
#include <stddef.h>
#include <time.h>

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
    uint32_t budget = app.scr.budget;                            /* la captura: el cuadro entero de una vez */
    app.scr.budget = 0;
    app.scr.force = 1;
    bm_app_tick(&app, now_ms);
    app.scr.budget = budget;
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

/* HOME, SELECT hasta el canal (el cursor del mezclador) y SEQ para entrar (docs/DISENO_0.6.md §5.3). */
static void ch(int t)
{
    for (int i = 0; i < 6 && app.view != V_HOME; i++) btn(BTN_HOME, 30);
    for (int i = 0; i < 8 && app.track != t; i++) enc(ENC_SELECT, t > app.track ? 1 : -1);
    btn(BTN_SEQ, 30);
}

static void page(int btn_id, int pg)
{
    for (int i = 0; i < 4 && !(app.view == V_CH && app.page == pg); i++) btn(btn_id, 30);
}

/* HOME sostenido + un botón */
static void home_with(int id)
{
    down(BTN_HOME); run_ms(40); btn(id, 30); up(BTN_HOME); run_ms(60);
}

/* una tecla sostenida ms milisegundos (las preguntas: 2 s = sí) */
static void hold_key(int k, uint32_t ms) { kdown(k); run_ms(ms); kup(k); run_ms(40); }

/* las luces y los nombres de las teclas ahora */
static bm_keyfn keys_now[BM_KEYS];
static const bm_keyfn *keys(void) { bm_app_keys(&app, keys_now); return keys_now; }

/* ------------------------------------------------------------ formato, flash y tomas */
static void test_pack(void)
{
    printf("formato 8: rejillas, sampler, notas, máquinas fijas, MIDI OUT, loop de pad y loopera\n");
    static bm_pattern p, q;
    static uint8_t b1[BM_PENDING_LEN], b2[BM_PENDING_LEN];
    bm_pattern_init(&p);
    for (int s = 0; s < 64; s += 3) p.tr[0].st[s % 16][s] = ST_ON | ST_ACC | (2 << ST_RAT_SH);
    p.tr[1].st[3][2] = ST_ON; p.tr[1].st[0][6] = ST_ON | ST_ACC;          /* T2: una línea por pad */
    p.pad[3].src = 2; p.pad[3].start = 1000; p.pad[3].len = 500; p.pad[3].mode = PAD_LOOP; p.pad[3].flags = PAD_REV;
    p.pad[3].loop_s = 40; p.pad[3].loop_e = 200;
    p.tr[1].lane[5].len = 12; p.tr[0].mute = 0x0104;
    CHECK(bm_tr_lock_set(&p.tr[0], 9, 9, 2, 33));
    CHECK(bm_tr_lock_get(&p.tr[0], 9, 9, 2) == 33);
    CHECK(p.tr[0].st[9][9] & ST_LOCK);
    CHECK(p.trk[0].engine == ENG_DRUM && p.trk[1].engine == ENG_BREAK && p.trk[2].engine == ENG_ACID && p.trk[3].engine == ENG_FM
          && p.trk[4].engine == ENG_BITTAL && p.trk[5].engine == ENG_BUNO);
    bm_track_set_engine(&p, 0, ENG_FM);                               /* T1 y T2 son fijos */
    bm_track_set_engine(&p, 1, ENG_MINI);
    CHECK(p.trk[0].engine == ENG_DRUM && p.trk[1].engine == ENG_BREAK);
    bm_track_set_engine(&p, 2, ENG_MINI);                             /* T3-T6: cualquiera de las 5 */
    CHECK(p.trk[2].engine == ENG_MINI && bm_kind(&p, 2) == KIND_NOTE && bm_is_grid(&p, 1) && !bm_is_tr(&p, 1));
    p.trk[5].midi_out = 1; p.trk[4].rsend = 77; p.meta.mfilt = 30; p.meta.beatfx = 3;
    for (int t = 2; t < 6; t++) {
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
    CHECK(q.trk[2].engine == ENG_MINI && q.trk[5].midi_out == 1 && q.trk[4].rsend == 77 && q.meta.mfilt == 30 && q.meta.beatfx == 3);
    CHECK(q.pad[3].loop_s == 40 && q.pad[3].loop_e == 200 && bm_pad_has_loop(&q.pad[3]) && bm_pad_loop_end(&q.pad[3]) == 500u * 200u / 255u);
    CHECK(memcmp(q.prm[4], p.prm[4], BM_PRM) == 0 && memcmp(q.prm[2], p.prm[2], BM_PRM) == 0);
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
    printf("zona compartida: tomas desde abajo, sonidos de BITTAL desde arriba, CRC y borrar\n");
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    bm_snaps_scan(pf.flash, pf.flash_map, pf.user);
    static bm_take tk[BM_TAKES];
    static int8_t d[BM_SMP_REC_MAX];
    for (uint32_t i = 0; i < BM_SMP_REC_MAX; i++) d[i] = (int8_t)((i * 7) & 0x7F);
    CHECK(bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk) == 0);
    uint32_t full = bm_takes_free(tk);
    CHECK(full == BM_TAKES_MAX_LEN && bm_takes_room(tk) == BM_SMP_REC_MAX);   /* 6 s en total, aunque la zona tenga más */
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, BM_SMP_REC_MAX, 1600, 2) == 0);   /* 3 s */
    CHECK(bm_takes_write(pf.flash, pf.flash_map, pf.user, tk, d, 20000, 1200, 1) == 1);
    CHECK(tk[0].valid && tk[1].valid && tk[1].len == 20000 && tk[1].data[123] == d[123] && tk[1].gain == 64);
    CHECK(tk[0].sector == 0);                                    /* desde abajo */
    /* un sonido del usuario: desde arriba de todo */
    const uint8_t *src = bm_snap_get(ENG_BITTAL, bm_snap_count(ENG_BITTAL) - 1);
    uint32_t sz = (uint32_t)(src[10] | src[11] << 8);
    static uint8_t one[BM_SNAP_MAX];
    memcpy(one, src, sz);
    memcpy(one + 12, "MY SOUND\0\0\0\0", 12);
    int n0 = bm_snap_count(ENG_BITTAL);
    uint8_t used[25] = {0};
    for (int i = 0; i < BM_TAKES; i++) if (tk[i].valid) for (int k = 0; k < tk[i].nsect; k++) used[tk[i].sector + k] = 1;
    CHECK(bm_snaps_write(pf.flash, pf.flash_map, pf.user, one, sz, used) == 0);
    CHECK(bm_snaps_user_n() == 1 && bm_snap_count(ENG_BITTAL) == n0 + 1 && bm_snap_is_user(ENG_BITTAL, n0));
    CHECK(strcmp(bm_snap_name(ENG_BITTAL, n0), "MY SOUND") == 0);
    uint32_t after = bm_takes_free(tk);
    CHECK(after <= full - 20000 - BM_SMP_REC_MAX);               /* el sonido ocupa lo suyo */
    tk[1].nmarks = 3; tk[1].marks[0] = 0; tk[1].marks[1] = 5000; tk[1].marks[2] = 12000; tk[1].gain = 80;
    static uint8_t scratch[BM_SECTOR];
    CHECK(bm_takes_update(pf.flash, &tk[1], scratch) == 0);
    static bm_take tk2[BM_TAKES];
    bm_snaps_scan(pf.flash, pf.flash_map, pf.user);
    CHECK(bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk2) == 2 && bm_snaps_user_n() == 1);
    CHECK(tk2[1].nmarks == 3 && tk2[1].marks[2] == 12000 && tk2[1].gain == 80 && tk2[1].bpm_x10 == 1200 && tk2[1].data[999] == d[999]);
    int n = 2;
    while (bm_takes_write(pf.flash, pf.flash_map, pf.user, tk2, d, 16000, 1200, 1) >= 0) n++;
    CHECK(n >= 3 && bm_takes_room(tk2) < 16000);
    CHECK(bm_takes_len(tk2) <= BM_TAKES_MAX_LEN && bm_takes_free(tk2) <= BM_TAKES_MAX_LEN - bm_takes_len(tk2));   /* el tope de 6 s */
    bm_snaps_scan(pf.flash, pf.flash_map, pf.user);
    CHECK(bm_snaps_user_n() == 1);                               /* las tomas no pisaron el sonido */
    printf("  %d tomas y 1 sonido en la zona de %u.%u s\n", n, (unsigned)(full / BM_SMP_RATE), (unsigned)(full * 10u / BM_SMP_RATE % 10u));
    CHECK(bm_takes_erase(pf.flash, &tk2[0]) == 0);
    CHECK(bm_takes_room(tk2) == BM_SMP_REC_MAX);                 /* el hueco de 3 s vuelve */
    CHECK(bm_snaps_erase(pf.flash, 0) == 0 && bm_snaps_user_n() == 0 && bm_snap_count(ENG_BITTAL) == n0);
    sim.flash[BM_TAKE_LO + tk2[1].sector * BM_SECTOR + BM_TAKE_HDR + 500] ^= 0x55;   /* byte dañado */
    static bm_take tk3[BM_TAKES];
    bm_takes_scan(pf.flash, pf.flash_map, pf.user, tk3);
    CHECK(!tk3[1].valid);
}

static uint32_t le32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }

static void test_report(void)
{
    printf("informe de memoria para la consola (orden 23) y borrar una toma desde afuera\n");
    boot_loop();
    uint8_t r[BM_REPORT_LEN + 8];
    CHECK(bm_app_report(&app, 0, 0, r, 10) == -1);
    CHECK(bm_app_report(&app, 0, 0, r, sizeof r) == BM_REPORT_LEN);
    CHECK(r[0] == 1 && r[3] == 0);                                   /* versión 1; un núcleo de fábrica (0.6.2) */
    CHECK(le32(r + 12) == BM_TAKES_MAX_LEN && le32(r + 24) == BM_TAKE_HI - BM_TAKE_LO);
    CHECK(le32(r + 8) + le32(r + 16) <= BM_TAKES_MAX_LEN && le32(r + 20) <= BM_SMP_REC_MAX);
    CHECK(strncmp((const char *)r + 58, BM_VERSION_STR, 14) == 0 && r[6] == BM_SNAPS_USER);
    int id = -1;
    for (int i = 0; i < BM_TAKES && id < 0; i++) if (app.take[i].valid) id = i;
    uint8_t t[16];
    CHECK(bm_app_report(&app, 1, BM_TAKES, t, sizeof t) == -1 && bm_app_report(&app, 2, 99, t, 64) == -1);
    if (id >= 0) {
        CHECK(bm_app_report(&app, 1, id, t, sizeof t) == 12 && t[0] == 1 && le32(t + 2) == app.take[id].len);
        bm_app_take_delete(&app, id);
        run_ms(200);
        CHECK(!app.take[id].valid);
        CHECK(bm_app_report(&app, 0, 0, r, sizeof r) == BM_REPORT_LEN && r[4] == 0);
    } else CHECK(0);                                                 /* boot_loop deja una toma */
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

/* Golpes del instrumento lane de T(t+1) mientras corre el audio ms milisegundos (sin la UI, que
 * baja lane_trig): cuántos, y la muestra y la velocidad de cada uno. */
static int roll_hits(int t, int lane, uint32_t ms, uint32_t *at, uint8_t *vel, int cap)
{
    int n = 0;
    uint32_t frames = ms * 44118u / 1000u, start = app.ctx.sample_clock;
    app.ctx.st.lane_trig[t] = 0;
    while (app.ctx.sample_clock - start < frames) {
        bm_app_audio(&app, abuf, 16);
        if (!(app.ctx.st.lane_trig[t] & (1u << lane))) continue;
        if (n < cap) { at[n] = app.ctx.sample_clock; vel[n] = app.ctx.st.roll_vel[t]; }
        n++;
        app.ctx.st.lane_trig[t] = 0;
    }
    return n;
}

static void test_roll(void)
{
    printf("ROLL: ARP en T1/T2 (un toque la deja abierta), velocidades, swing, CRESC, cola y REC\n");
    boot(1);
    ch(0);
    btn(BTN_ARP, 30);                                             /* un toque: abierta */
    CHECK(app.layer == L_ROLL && app.layer_latch && app.view == V_CH && app.track == 0);
    CHECK(sim.leds[BTN_ARP] == LED_BREATH && sim.leds[BTN_HOME] == LED_MID);
    bm_keyfn K[BM_KEYS];
    bm_app_keys(&app, K);
    for (int w = 0; w < 16; w++) {                                /* con pasos MID, vacío DIM (igual hace roll) */
        const bm_tr *tr = &app.pat.tr[0];
        int len = tr->lane[w].len ? tr->lane[w].len : 16, has = 0;
        for (int s = 0; s < len; s++) if (tr->st[w][s] & ST_ON) has = 1;
        CHECK(K[bm_white_key(w)].led == (has ? LED_MID : LED_DIM));
    }
    CHECK(K[bm_black_key(1)].led == LED_ON && K[bm_black_key(0)].led == LED_DIM && K[bm_black_key(5)].led == LED_DIM);
    CHECK(K[bm_black_key(6)].led == LED_DIM && K[bm_black_key(7)].led == LED_DIM && K[bm_black_key(10)].led == LED_DIM);
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s1, "-") && !strcmp(s2, "-") && !strcmp(s3, "-"));
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "RATE") && !strcmp(L[0].val, "1/16") && !strcmp(L[2].name, "SWING") && !strcmp(L[3].name, "TAIL"));
    {
        char title[24]; uint16_t col; const char *h1, *h2;
        bm_app_keymap(&app, K, title, sizeof title, &col, &h1, &h2);
        CHECK(!strcmp(title, "ROLL") && !strncmp(h2, "EXIT:", 5) && strlen(h1) <= 38 && strlen(h2) <= 38);
    }
    btn(BTN_ARP, 30);                                             /* ARP otra vez: cerrada */
    CHECK(app.layer == L_NONE && !app.layer_latch);
    btn(BTN_ARP, 30);
    CHECK(app.layer == L_ROLL);
    btn(BTN_HOME, 30);                                            /* HOME: cerrada, sigues en el canal */
    CHECK(app.layer == L_NONE && !app.layer_latch && app.view == V_CH);
    btn(BTN_LFO, 30);                                             /* REPEAT (LFO en BR-8) abierta: ARP la cambia por ROLL */
    CHECK(app.layer == L_REP && app.layer_latch);
    btn(BTN_ARP, 30);
    CHECK(app.layer == L_ROLL && app.layer_latch);
    btn(BTN_HOME, 30);
    CHECK(app.layer == L_NONE);
    down(BTN_ARP); run_ms(400);                                   /* sostener: solo mientras lo sostienes */
    CHECK(app.layer == L_ROLL && !app.layer_latch);
    up(BTN_ARP); run_ms(60);
    CHECK(app.layer == L_NONE);
    btn(BTN_ARP, 30);                                             /* abierta y luego sostener ARP: se cierra al soltar */
    down(BTN_ARP); run_ms(400);
    CHECK(app.layer == L_ROLL && !app.layer_latch);
    up(BTN_ARP); run_ms(60);
    CHECK(app.layer == L_NONE);

    double fpt = BM_FS * 60.0 / (app.pat.meta.bpm_x10 / 10.0 * BM_PPQN);   /* muestras por tick */
    uint32_t at[64]; uint8_t vel[64];
    btn(BTN_ARP, 30);
    black(4, 40);                                                 /* 1/16T: cada 16 ticks */
    CHECK(app.roll_rate == 4);
    kdown(bm_white_key(5));
    int n = roll_hits(0, 5, 700, at, vel, 64);
    CHECK(n >= 8);
    int ok = 1;
    for (int i = 1; i < n && i < 64; i++) if (fabs((double)(at[i] - at[i - 1]) - 16 * fpt) > 40) ok = 0;
    CHECK(ok);
    black(5, 40);                                                 /* 1/32T, con la tecla sostenida */
    CHECK(app.roll_rate == 5);
    n = roll_hits(0, 5, 300, at, vel, 64);
    ok = n >= 8;
    for (int i = 1; i < n && i < 64; i++) if (fabs((double)(at[i] - at[i - 1]) - 8 * fpt) > 40) ok = 0;
    CHECK(ok);
    kup(bm_white_key(5));
    CHECK(roll_hits(0, 5, 300, at, vel, 64) == 0);                /* sin cola: soltar lo corta */

    black(1, 40);                                                 /* 1/16 con swing 75 %: 36 + 12 ticks */
    enc(ENC_K3, 25);
    CHECK(app.roll_swing == 75 && app.ctx.roll_cfg.swing == 75);
    kdown(bm_white_key(5));
    n = roll_hits(0, 5, 800, at, vel, 64);
    kup(bm_white_key(5));
    ok = n >= 5;
    for (int i = 2; i < n && i < 64; i++) {
        double a1 = (double)(at[i - 1] - at[i - 2]), a2 = (double)(at[i] - at[i - 1]);
        if (fabs(a1 + a2 - 48 * fpt) > 60 || fabs(fabs(a1 - a2) - 24 * fpt) > 60) ok = 0;
    }
    CHECK(ok);
    enc(ENC_K3, -25);
    CHECK(app.roll_swing == 50);
    roll_hits(0, 5, 100, at, vel, 64);

    black(6, 40);                                                 /* CRESC en 1/4: la velocidad sube hasta 127 */
    enc(ENC_K2, -2);
    CHECK(app.roll_ramp == 1 && app.roll_len == 0 && app.ctx.roll_cfg.ramp == 1 && app.ctx.roll_cfg.ramp_len == BM_PPQN);
    bm_app_keys(&app, K);
    CHECK(K[bm_black_key(6)].led == LED_ON && K[bm_black_key(7)].led == LED_DIM);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[1].name, "CRESC") && !strcmp(L[1].val, "1/4"));
    kdown(bm_white_key(5));
    n = roll_hits(0, 5, 900, at, vel, 64);
    ok = n >= 6 && vel[0] < 70 && vel[n - 1] == 127;
    for (int i = 1; i < n && i < 64; i++) if (vel[i] < vel[i - 1]) ok = 0;
    CHECK(ok);
    run_ms(UI_OVERLAY_MS + 100);                                  /* sin el aviso de la perilla */
    shot("22b-roll-latched");
    kup(bm_white_key(5));
    black(7, 40);                                                 /* DECRESC: baja */
    CHECK(app.roll_ramp == 2);
    kdown(bm_white_key(5));
    n = roll_hits(0, 5, 900, at, vel, 64);
    kup(bm_white_key(5));
    CHECK(n >= 6 && vel[0] > vel[n - 1] && vel[n - 1] == 24);
    black(7, 40);
    CHECK(app.roll_ramp == 0 && app.ctx.roll_cfg.ramp == 0);

    enc(ENC_K4, 3);                                               /* cola de 1/4: 4 golpes de 1/16 más */
    CHECK(app.roll_tail == 3 && app.ctx.roll_cfg.tail == BM_PPQN);
    kdown(bm_white_key(5));
    roll_hits(0, 5, 500, at, vel, 64);
    kup(bm_white_key(5));
    n = roll_hits(0, 5, 400, at, vel, 64);
    CHECK(n >= 3 && n <= 5);
    CHECK(roll_hits(0, 5, 300, at, vel, 64) == 0 && app.ctx.roll[0].tail_m == 0);
    enc(ENC_K4, -3);

    kdown(bm_black_key(10));                                      /* negra 11: el fill mientras la sostienes */
    run_ms(60);
    CHECK(app.fill && app.ctx.fill);
    bm_app_keys(&app, K);
    CHECK(K[bm_black_key(10)].led == LED_ON);
    kup(bm_black_key(10));
    run_ms(60);
    CHECK(!app.fill && !app.ctx.fill && app.layer == L_ROLL);

    /* REC: el roll se graba como pasos con repetición, y CRESC como acentos */
    int lane = -1;
    for (int w = 0; w < 16 && lane < 0; w++) {
        int any = 0;
        for (int s = 0; s < BM_STEPS; s++) if (app.pat.tr[0].st[w][s]) any = 1;
        if (!any) lane = w;
    }
    CHECK(lane >= 0);
    if (lane < 0) lane = 15;
    btn(BTN_PLAY, 20);
    btn(BTN_REC, 30);                                             /* REC no cierra ROLL */
    CHECK(app.rec && app.layer == L_ROLL && app.layer_latch);
    black(2, 40);                                                 /* 1/32 = x2 por paso */
    black(6, 40);                                                 /* CRESC */
    kdown(bm_white_key(lane));
    run_ms(1100);
    kup(bm_white_key(lane));
    run_ms(60);
    btn(BTN_REC, 30);
    btn(BTN_PLAY, 20);
    int on = 0, rat = 0, acc = 0, flat = 0;
    for (int s = 0; s < 16; s++) {
        uint8_t b = app.pat.tr[0].st[lane][s];
        if (!(b & ST_ON)) continue;
        on++;
        if (((b & ST_RAT_MASK) >> ST_RAT_SH) == 1) rat++;
        if (b & ST_ACC) acc++; else flat++;
    }
    CHECK(on >= 6 && rat >= 6 && acc >= 1 && flat >= 1);
    black(6, 40);
    btn(BTN_ARP, 30);
    CHECK(app.layer == L_NONE && app.roll_mask == 0);

    /* T2 (pads): lo mismo; REC en ROLL graba lo que tocas, no abre la grabadora */
    ch(1);
    btn(BTN_ARP, 30);
    CHECK(app.track == 1 && app.layer == L_ROLL && app.layer_latch);
    btn(BTN_REC, 30);
    CHECK(app.rec && app.page != PG_REC && app.layer == L_ROLL && !app.ctx.st.smp_armed);
    btn(BTN_REC, 30);
    CHECK(!app.rec);
    kdown(bm_white_key(3));
    run_ms(100);
    CHECK(app.ctx.roll[1].mask == (1u << 3));
    kup(bm_white_key(3));
    btn(BTN_HOME, 30);
    CHECK(app.layer == L_NONE && app.ctx.roll[1].mask == 0);
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
    printf("navegación 0.9: HOME = la consola (MIX y FX), SELECT cursor, ALGORITHM BPM, PRESETS el sonido, GLO de la máquina\n");
    boot(1);
    CHECK(app.view == V_HOME && app.track == 0);
    shot("00-home");
    int bpm = app.pat.meta.bpm_x10;
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);                       /* HOME: ALGORITHM = BPM */
    CHECK(app.pat.meta.bpm_x10 == bpm + 20 && app.view == V_HOME && app.ov.beat && app.ov.color == C_TEMPO);
    shot("05b-home-tempo");
    enc(ENC_ALGO, -2);
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1);                   /* SELECT: el cursor del mezclador */
    CHECK(app.track == 2 && app.view == V_HOME && !app.card);
    int lv = app.pat.trk[2].level;
    enc(ENC_K1, -2);                                          /* K1 = el volumen del canal del cursor */
    CHECK(app.pat.trk[2].level == lv - 2);
    enc(ENC_K2, -2);                                          /* K2 = el filtro DJ del canal: el centro retiene 2 detents */
    CHECK(app.pat.trk[2].djf == 0);
    run_ms(1000);
    enc(ENC_K2, -4);                                          /* a la izquierda pasa-bajos */
    CHECK(app.pat.trk[2].djf == 2);
    enc(ENC_K2, 2);                                           /* volviendo, se para en el centro */
    CHECK(app.pat.trk[2].djf == 0);
    enc(ENC_K2, 3);                                           /* a la derecha pasa-altos */
    CHECK(app.pat.trk[2].djf == 64 + 1);
    enc(ENC_K2, -1);                                          /* al centro: abierto */
    CHECK(app.pat.trk[2].djf == 0);
    int sd = app.pat.trk[2].send, rv2 = app.pat.trk[2].rsend;
    enc(ENC_K3, 2); enc(ENC_K4, 2);                           /* K3 = delay, K4 = reverb */
    CHECK(app.pat.trk[2].send == sd + 2 && app.pat.trk[2].rsend == rv2 + 2);
    int src = app.pat.trk[2].source;                          /* PRESETS = el sonido del canal del cursor (0.9) */
    enc(ENC_PRESETS, 1); run_ms(30);
    CHECK(app.pat.trk[2].source == (src + 1) % BM_AC_PRESETS && app.view == V_HOME && app.move == 0);
    {
        const char *s, *al, *pr;
        bm_app_bigknobs(&app, &s, &al, &pr);
        CHECK(!strcmp(s, "TRACK") && !strcmp(al, "BPM") && !strcmp(pr, "SOUND"));
    }
    enc(ENC_PRESETS, -1); run_ms(30);
    CHECK(app.pat.trk[2].source == src);
    run_ms(UI_OVERLAY_MS + 100);
    shot("01-home-mixer");
    enc(ENC_K3, -2); enc(ENC_K4, -2);
    btn(BTN_SEQ, 30);                                         /* SEQ entra al canal del cursor */
    CHECK(app.view == V_CH && app.track == 2 && app.page == PG_SEQ);
    shot("06-seq-bass");
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_SOUND);
    /* HOME + blanca 14 (T4): a esa máquina al soltar HOME (0.9: en lugar de HOME + SELECT) */
    down(BTN_HOME); run_ms(60);
    white(13, 40);
    CHECK(app.layer == L_HOME && app.home_go == 4 && app.view == V_CH);
    CHECK(keys()[bm_white_key(13)].led == LED_BLINK);
    shot("62-home-go");
    up(BTN_HOME); run_ms(60);
    CHECK(!app.home_go && app.track == 3 && app.view == V_CH && app.layer == L_NONE);
    btn(BTN_EDIT, 30);
    CHECK(app.page == PG_SOUND);
    app.ed_page[3] = 0;                                       /* BX-7: ALGORITHM, con el dibujo */
    shot("12-fm");
    enc(ENC_SELECT, 1);                                       /* SELECT: las páginas de la máquina y luego las pestañas */
    CHECK(app.page == PG_SOUND && app.ed_page[3] == 1);
    enc(ENC_SELECT, -1); enc(ENC_SELECT, -1);                 /* antes de la primera: SEQ (ya no hay MACHINE) */
    CHECK(app.page == PG_SEQ);
    /* GLO dentro de la máquina: su GLO (MORE adentro, sin pestaña); GLO otra vez = de vuelta */
    btn(BTN_GLO, 30);
    CHECK(app.view == V_CH && app.page == PG_MORE && sim.leds[BTN_GLO] == LED_ON);
    shot("45-machine-glo");
    btn(BTN_GLO, 30);
    CHECK(app.view == V_CH && app.page == PG_SEQ);
    {
        uint8_t tabs[8];
        int n = bm_app_tabs(&app, 3, tabs), more = 0;
        for (int i = 0; i < n; i++) more |= tabs[i] == PG_MORE;
        CHECK(!more && n == 5);
    }
    btn(BTN_FX, 30);
    CHECK(app.page == PG_MIX && !app.mix_page);
    lv = app.pat.trk[3].level;
    enc(ENC_K1, -2); run_ms(300); enc(ENC_K1, -2);            /* despacio: sin aceleración */
    CHECK(app.pat.trk[3].level == lv - 4);
    int rv = app.pat.trk[3].rsend;
    enc(ENC_K4, 1); enc(ENC_K4, 1); enc(ENC_K4, 1);           /* K4 = el envío a la reverb */
    CHECK(app.pat.trk[3].rsend == rv + 3);
    shot("14-mix");
    btn(BTN_FX, 30);                                          /* FX otra vez: DUCK */
    CHECK(app.page == PG_MIX && app.mix_page);
    int dk = app.pat.trk[3].duck;
    enc(ENC_K1, 2);
    CHECK(app.pat.trk[3].duck == dk + 2);
    run_ms(UI_OVERLAY_MS + 100);
    shot("14b-mix-duck");
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_HOME, 30);                                        /* HOME otra vez: sigue en MIX (0.9) */
    CHECK(app.view == V_HOME);
    /* FX en HOME: el FX de la consola; otra vez, la página siguiente: CHANNEL > DELAY > REVERB > MASTER */
    btn(BTN_FX, 30); CHECK(app.view == V_FXG && app.fx_page == FXP_CHANNEL);
    btn(BTN_FX, 30); btn(BTN_FX, 30); btn(BTN_FX, 30); CHECK(app.view == V_FXG && app.fx_page == FXP_MASTER);
    enc(ENC_K1, -20);                                          /* MASTER: K1 = el filtro del master */
    CHECK(app.pat.meta.mfilt < 60);
    enc(ENC_K2, 1);                                            /* K2 = BEAT FX */
    CHECK(app.pat.meta.beatfx == 1);
    shot("39-home-fx-master");
    enc(ENC_K1, 20); enc(ENC_K2, -1);
    enc(ENC_PRESETS, -4);                                      /* en el FX, PRESETS = MOVE; el centro retiene 2 */
    run_ms(30);
    CHECK(app.move == -2 && app.ctx.move == -2 && app.view == V_FXG);
    enc(ENC_PRESETS, 2);
    run_ms(30);
    CHECK(app.move == 0 && app.ctx.move == 0);
    btn(BTN_HOME, 30); CHECK(app.view == V_HOME);
    /* HOME + FX: el FX de la consola; otra vez, la página siguiente */
    home_with(BTN_FX);
    CHECK(app.view == V_FXG && app.fx_page == FXP_CHANNEL);
    home_with(BTN_FX); home_with(BTN_FX);
    CHECK(app.view == V_FXG && app.fx_page == FXP_REVERB);
    int rs = app.pat.meta.rv_size;
    enc(ENC_K1, 2);
    CHECK(app.pat.meta.rv_size == rs + 2);
    shot("50-fx-reverb");
    /* GLO de la consola (HOME + GLO desde una máquina): TEMPO > MACHINES > SCALE > SONG */
    ch(3);
    home_with(BTN_GLO);
    CHECK(app.view == V_GLO && app.glo_page == GP_TEMPO);
    int sw = app.pat.meta.swing;
    enc(ENC_SELECT, 1); enc(ENC_ALGO, 1);                     /* SWING +1 */
    CHECK(app.pat.meta.swing == sw + 1);
    run_ms(UI_OVERLAY_MS + 100);
    shot("24-glo-tempo");
    btn(BTN_GLO, 30); CHECK(app.glo_page == GP_MACH);
    btn(BTN_GLO, 30); CHECK(app.glo_page == GP_SCALE);
    btn(BTN_GLO, 30); CHECK(app.glo_page == GP_SONG);
    btn(BTN_GLO, 30); CHECK(app.glo_page == GP_TEMPO);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
}

static void test_home(void)
{
    printf("HOME sostenido en todas partes: la consola (patrones, canales, mutes, solo, cadenas) y HOME + botón\n");
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
    CHECK(keys()[bm_black_key(3)].led == LED_ON);
    kup(bm_black_key(3)); run_ms(80);
    CHECK(app.pat.trk[0].mute == 1 && app.pat.trk[2].mute == 0 && app.pat.trk[3].mute == 0);   /* vuelve */
    black(0, 40);
    CHECK(app.pat.trk[0].mute == 0);
    kdown(bm_white_key(1)); run_ms(40); white(2, 40); kup(bm_white_key(1)); run_ms(40);   /* dos blancas: cadena */
    CHECK(app.ctx.st.pchain_n == 2);
    white(12, 40);                                            /* blanca 13 = T3, no un patrón */
    CHECK(app.ctx.st.pchain_n == 2 && app.home_go == 3);
    up(BTN_HOME); run_ms(60);
    CHECK(app.layer == L_NONE && app.view == V_CH && app.track == 2);    /* HOME + T3: la misma máquina */
    btn(BTN_PLAY, 20);
    /* HOME + botón */
    home_with(BTN_SEQ);
    CHECK(app.view == V_GLO && app.glo_page == GP_SONG);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    white(0, 40);                                             /* quita el bombo del paso 1 */
    CHECK(!(app.pat.tr[0].st[0][0] & ST_ON));
    home_with(BTN_OCT_DN);
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);                   /* HOME + OCT- deshace */
    home_with(BTN_FX);
    CHECK(app.view == V_FXG);
    shot("50-fx-global");
    home_with(BTN_SCL);
    CHECK(app.view == V_GLO && app.glo_page == GP_SCALE);
    home_with(BTN_GLO);                                       /* ya en GLO: la página siguiente */
    CHECK(app.view == V_GLO && app.glo_page == GP_SONG);
    home_with(BTN_PLAY);
    CHECK(app.ctx.st.playing);                                /* HOME + PLAY: desde el paso 1 */
    btn(BTN_PLAY, 20);
    /* HOME + EDIT: copiar un canal (T4 BX-7 a T5: las notas y la máquina con su sonido); EDIT 2 s pega */
    ch(3);
    home_with(BTN_EDIT);
    CHECK(app.view == V_COPYCH && app.copy_src == 3);
    for (int i = 0; i < 12 && app.copy_dst != 4; i++) enc(ENC_ALGO, 1);
    CHECK(sim.leds[BTN_EDIT] == LED_BLINK && sim.leds[BTN_HOME] == LED_ON && sim.leds[BTN_OCT_UP] == LED_OFF);
    shot("49-copy-channel");
    btn(BTN_EDIT, 2200); run_ms(100);
    CHECK(app.pat.trk[4].engine == ENG_FM && app.view == V_CH && app.track == 4);
    CHECK(memcmp(app.pat.prm[4], app.pat.prm[3], BM_PRM) == 0 && app.pat.trk[4].source == app.pat.trk[3].source);
    CHECK(!bm_event_empty(bm_evc(&app.pat, 4, 10, 0)) && bm_evc(&app.pat, 4, 10, 0)->note == 57);
}

/* Negras 10 y 11 en la batería: un toque deja abierta la pantalla de MUTE o de FILL; mantener = solo
 * mientras la sostienes. En INSTR, ALGORITHM elige instrumento con el recuadro. */
static int t2_steps(void)
{
    int n = 0;
    for (int l = 0; l < BM_LANES; l++) for (int s = 0; s < BM_STEPS; s++) if (app.pat.tr[1].st[l][s] & ST_ON) n++;
    return n;
}

static void test_button_lights(void)
{
    printf("luces de los botones: SAVE pendiente, capas, página, REC sostenido en la grabadora\n");
    boot(1);
    run_ms(UI_AUTOSAVE_MS + 1500);                               /* el demo ya está guardado */
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    CHECK(sim.leds[BTN_SEQ] == LED_ON && sim.leds[BTN_SAVE] == LED_DIM && sim.leds[BTN_ENV] == LED_DIM);
    white(15, 40);                                               /* una edición */
    CHECK(sim.leds[BTN_SAVE] == LED_DIM);                        /* dentro de la máquina: sin avisos globales */
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && sim.leds[BTN_SAVE] == LED_BREATH);   /* en la consola: algo sin guardar */
    CHECK(sim.leds[BTN_HOME] == LED_ON && sim.leds[BTN_SEQ] == LED_MID && sim.leds[BTN_EDIT] == LED_MID);
    btn(BTN_FX, 30);                                             /* FX en HOME: el FX de la consola */
    CHECK(app.view == V_FXG && sim.leds[BTN_HOME] == LED_ON && sim.leds[BTN_FX] == LED_MID);
    CHECK(sim.leds[BTN_OCT_DN] == LED_OFF && sim.leds[BTN_OCT_UP] == LED_OFF);   /* el cursor en T1 (batería): sin octava */
    ch(0);
    CHECK(sim.leds[BTN_OCT_DN] == LED_OFF && sim.leds[BTN_OCT_UP] == LED_OFF);   /* batería: OCT apagados */
    /* SISTEMA > LED: solo los botones en reposo */
    app.set.led_dim = 2; run_ms(60);
    CHECK(sim.leds[BTN_ENV] == LED_MID && sim.leds[BTN_SEQ] == LED_ON);
    app.set.led_dim = 0; run_ms(60);
    CHECK(sim.leds[BTN_ENV] == LED_OFF && sim.leds[BTN_SEQ] == LED_ON);
    app.set.led_dim = 1; run_ms(60);
    /* MUTE de instrumentos (negra 11 sostenida) no tiene botón: no respira OCT- ni SEQ */
    kdown(bm_black_key(10)); run_ms(400);
    CHECK(app.layer == L_LANEMUTE && !app.layer_latch);
    CHECK(sim.leds[BTN_OCT_DN] == LED_OFF && sim.leds[BTN_SEQ] == LED_OFF && sim.leds[BTN_HOME] == LED_OFF);
    kup(bm_black_key(10)); run_ms(60);
    CHECK(app.layer == L_NONE);
    /* FILL en un canal de notas (negra 11): HOME la cierra; OCT cambia la octava */
    ch(2);
    page(BTN_SEQ, PG_SEQ);
    CHECK(sim.leds[BTN_OCT_DN] == LED_DIM && sim.leds[BTN_OCT_UP] == LED_DIM);
    black(10, 40);
    CHECK(app.layer == L_FILL && sim.leds[BTN_SEQ] == LED_OFF && sim.leds[BTN_HOME] == LED_MID);
    btn(BTN_HOME, 30);
    CHECK(app.layer == L_NONE && app.view == V_CH);
    ch(0);
    /* ENV en la batería abre INSTR: se enciende ENV, no EDIT */
    btn(BTN_ENV, 30);
    CHECK(app.page == PG_INST && sim.leds[BTN_ENV] == LED_ON && sim.leds[BTN_EDIT] == LED_DIM);
    /* la grabadora: REC sostenido 2.5 s graba la mezcla y no borra la pista */
    ch(1);
    if (!t2_steps()) white(0, 40);
    int before = t2_steps();
    CHECK(before > 0);
    btn(BTN_REC, 30);                                            /* a la GRABADORA, armada */
    CHECK(app.page == PG_REC && app.ctx.st.smp_armed && sim.leds[BTN_REC] == LED_BREATH);
    btn(BTN_REC, 30);                                            /* desarmar */
    CHECK(!app.ctx.st.smp_armed);
    down(BTN_REC); run_ms(1000);
    CHECK(app.ctx.st.smp_rec && sim.leds[BTN_REC] == LED_BLINK);
    run_ms(1500);
    CHECK(app.ctx.st.smp_rec);                                   /* a los 2 s sigue grabando */
    up(BTN_REC); run_ms(60);
    CHECK(!app.ctx.st.smp_rec && (app.ctx.st.smp_unsaved || app.take[0].valid) && app.page == PG_REC && app.ask == ASK_NONE);
    CHECK(t2_steps() == before);
}

static void test_key_screens(void)
{
    printf("negra 11: HI/LO y MUTE de instrumentos (batería), FILL (notas); B9/B10 = páginas; ALGORITHM en INSTR\n");
    boot(1);
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    app.sel_lane[0] = 2;                                         /* CL HH */
    black(10, 40);                                               /* un toque: el otro banco */
    CHECK(app.layer == L_NONE && app.sel_lane[0] == 10);
    black(10, 40);
    CHECK(app.sel_lane[0] == 2);
    kdown(bm_black_key(10)); run_ms(400);                        /* sostenida: MUTE mientras la sostienes */
    CHECK(app.layer == L_LANEMUTE && !app.layer_latch);
    shot("63-mutes");
    {   /* luces de MUTE: con pasos y sonando MID (y destella con cada golpe), muteado DIM, vacío OFF */
        bm_keyfn K[BM_KEYS];
        int full = -1;
        memset(app.pat.tr[0].st[15], 0, sizeof app.pat.tr[0].st[15]);          /* el 16 queda vacío */
        for (int l = 0; l < 15 && full < 0; l++) {
            if ((app.pat.tr[0].mute >> l) & 1) continue;
            int len = app.pat.tr[0].lane[l].len ? app.pat.tr[0].lane[l].len : 16;
            for (int s = 0; s < len; s++) if (app.pat.tr[0].st[l][s] & ST_ON) { full = l; break; }
        }
        CHECK(full >= 0);
        bm_app_keys(&app, K);
        CHECK(K[bm_white_key(full)].led == LED_MID && K[bm_white_key(15)].led == LED_OFF);
        CHECK(K[bm_black_key(10)].led == LED_BREATH && strcmp(K[bm_black_key(10)].label, "X") == 0);
        app.lane_flash[0][full] = app.now + 90;                                 /* acaba de sonar */
        bm_app_keys(&app, K);
        CHECK(K[bm_white_key(full)].live);
        white(full, 40);                                                        /* muteado: tenue y sin destello */
        app.lane_flash[0][full] = app.now + 90;
        bm_app_keys(&app, K);
        CHECK(K[bm_white_key(full)].led == LED_DIM && !K[bm_white_key(full)].live);
        white(full, 40);
        app.lane_flash[0][full] = 0;
    }
    white(2, 40);                                                /* mutea el hat cerrado */
    CHECK((app.pat.tr[0].mute >> 2) & 1);
    white(2, 40);
    CHECK(!((app.pat.tr[0].mute >> 2) & 1));
    kup(bm_black_key(10)); run_ms(40);                           /* soltar: sale, sin cambiar de banco */
    CHECK(app.layer == L_NONE && app.sel_lane[0] == 2);
    /* B9 / B10: las páginas de pasos (MID si hay página para ese lado, OFF si no) */
    down(BTN_SEQ); run_ms(400);
    black(10, 40);                                               /* capa LENGTH: negra 11 = +16 */
    CHECK(app.len_page == 1);
    white(15, 40);                                               /* largo 32 */
    up(BTN_SEQ); run_ms(60);
    CHECK(app.pat.tr[0].lane[2].len == 32 && app.view == V_CH && app.page == PG_SEQ);
    CHECK(keys()[bm_black_key(8)].led == LED_OFF && keys()[bm_black_key(9)].led == LED_MID);
    CHECK(!strcmp(keys()[bm_black_key(8)].label, "<") && !strcmp(keys()[bm_black_key(9)].label, ">"));
    black(9, 40);
    CHECK(app.step_page == 1 && keys()[bm_black_key(8)].led == LED_MID && keys()[bm_black_key(9)].led == LED_OFF);
    black(9, 40);
    CHECK(app.step_page == 1);
    white(0, 40);                                                /* la blanca 1 es el paso 17 */
    CHECK(app.pat.tr[0].st[2][16] & ST_ON);
    black(8, 40);
    CHECK(app.step_page == 0);
    /* FILL de notas: la negra 11 en un canal de notas (la batería ya no tiene pantalla de FILL) */
    ch(2);
    page(BTN_SEQ, PG_SEQ);
    black(10, 40);
    CHECK(app.layer == L_FILL && app.layer_latch && app.fill && app.ctx.fill);
    app.step_page = 0;
    white(0, 40);                                                /* paso 1 (con nota): solo en el fill */
    CHECK(bm_evc(&app.pat, 2, 0, 0)->prob == BM_PROB_FILL);
    shot("64-fill");
    white(0, 40);                                                /* otra vez: siempre */
    CHECK(bm_evc(&app.pat, 2, 0, 0)->prob == 100);
    black(10, 40);
    CHECK(app.layer == L_NONE && !app.fill && !app.ctx.fill);
    /* INSTR: ALGORITHM recorre los instrumentos con el recuadro */
    ch(0);
    page(BTN_EDIT, PG_SOUND); btn(BTN_EDIT, 30);
    CHECK(app.page == PG_INST);
    int l0 = app.sel_lane[0];
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.box_on && app.box_mode == BOX_LANE && app.box_lane == l0 + 2 && app.sel_lane[0] == l0);
    shot("65-box-instrumento");
    run_ms(500);
    CHECK(!app.box_on && app.sel_lane[0] == l0 + 2 && app.track == 0 && app.page == PG_INST);
    shot("66-home-guia");
    /* LOCK de escala: soltar un acorde (negras 1-4) no suelta el +7 que sigue sostenido */
    ch(3);
    page(BTN_EDIT, PG_SOUND);
    app.pat.meta.scale_lock = 1;
    kdown(bm_black_key(7)); run_ms(40);
    CHECK(app.chord_mod & 1);
    black(0, 40);
    CHECK(app.chord_mod & 1);
    kup(bm_black_key(7)); run_ms(40);
    CHECK(!(app.chord_mod & 1));
    app.pat.meta.scale_lock = 0;
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
    /* BR-8 no tiene LFO: un toque deja abierta la capa REPETIR (respira), LFO otra vez o HOME la cierran */
    {
        bm_keyfn K[BM_KEYS]; char title[24]; uint16_t col; const char *h1, *h2;
        btn(BTN_LFO, 30);
        CHECK(app.layer == L_REP && app.layer_latch && app.view == V_CH && sim.leds[BTN_LFO] == LED_BREATH);
        CHECK(sim.leds[BTN_HOME] == LED_MID);
        bm_app_keymap(&app, K, title, sizeof title, &col, &h1, &h2);
        CHECK(strcmp(title, "REPEAT") == 0 && strncmp(h2, "EXIT: LFO OR HOME", 17) == 0 && strlen(h2) <= 38 && strlen(h1) <= 38);
        white(0, 40);                                         /* abierta sin sostener: sigue pintando */
        CHECK(((app.pat.tr[0].st[0][0] & ST_RAT_MASK) >> ST_RAT_SH) == 3);
        black(9, 40);                                         /* B10 pasa páginas sin cerrarla (aquí hay una sola) */
        CHECK(app.layer == L_REP && app.layer_latch && app.step_page == 0);
        CHECK(K[bm_black_key(9)].led == LED_OFF);
        btn(BTN_OCT_UP, 20);                                  /* OCT en la batería: nada, y no la cierra */
        CHECK(app.layer == L_REP && app.layer_latch && app.oct == 0);
        btn(BTN_LFO, 30);                                     /* LFO otra vez: sale */
        CHECK(app.layer == L_NONE && !app.layer_latch && app.view == V_CH && app.page == PG_SEQ);
        btn(BTN_LFO, 30);
        CHECK(app.layer == L_REP && app.layer_latch);
        btn(BTN_HOME, 30);                                    /* HOME: sale, en el canal */
        CHECK(app.layer == L_NONE && !app.layer_latch && app.view == V_CH);
        CHECK(sim.leds[BTN_LFO] == LED_DIM);
    }
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
    /* A06 ya tiene datos: pregunta. 0.9: se acepta sosteniendo 2 s la tecla que preguntó; soltarla antes = no */
    down(BTN_SAVE); run_ms(400); kdown(bm_black_key(2)); white(5, 40);
    CHECK(app.ask == ASK_NONE && strcmp(app.toast, "CANCELED") == 0);     /* soltada enseguida: no */
    kdown(bm_white_key(5)); run_ms(600);
    CHECK(app.ask == ASK_COPY_TRK && app.ask_live && bm_app_ask_progress(&app) > 50);
    up(BTN_SAVE); run_ms(40);                                 /* soltar SAVE (cierra la capa) no contesta */
    CHECK(app.ask == ASK_COPY_TRK && app.layer == L_NONE);
    shot("46-ask");
    CHECK(sim.leds[BM_LED_KEY0 + bm_white_key(5)] == LED_BLINK && sim.leds[BTN_OCT_UP] == LED_OFF && sim.leds[BTN_HOME] == LED_OFF);
    CHECK(sim.leds[BTN_SEQ] == LED_OFF && sim.leds[BTN_SAVE] == LED_OFF);   /* en la pregunta, solo la tecla que preguntó */
    {
        char h[40]; bm_app_ask_hint(&app, h, sizeof h);
        CHECK(strcmp(h, "KEEP HOLDING WHITE 6 = YES") == 0);
    }
    run_ms(1500);                                             /* 2 s sostenida: sí */
    CHECK(app.ask == ASK_NONE && strcmp(app.toast, "TRACK COPIED") == 0);
    kup(bm_white_key(5)); kup(bm_black_key(2)); run_ms(600);
    CHECK(app.ask == ASK_NONE);
    /* REC 2 s sin grabar nada: borra la pista al instante, sin preguntar (en vivo); HOME + OCT- la devuelve */
    ch(0);
    run_ms(100);
    int hits0 = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s] & ST_ON) hits0++;
    CHECK(hits0 > 0);
    down(BTN_REC); run_ms(1500);
    {
        int hid = -1;
        CHECK(bm_gesture_progress(&app.ges, app.now, &hid) >= 0 && hid == BTN_REC);   /* el anillo: ERASE T1 */
        CHECK(sim.leds[BTN_REC] == LED_BLINK);
    }
    run_ms(1000); up(BTN_REC); run_ms(60);
    CHECK(app.ask == ASK_NONE && app.view == V_CH && !app.rec);
    any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(!any);
    CHECK(strncmp(app.toast, "T1 ERASED", 9) == 0);
    down(BTN_HOME); run_ms(40); btn(BTN_OCT_DN, 20); up(BTN_HOME); run_ms(60);
    int hits1 = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s] & ST_ON) hits1++;
    CHECK(hits1 == hits0);
    /* EDIT sostenido + CLR también borra la pista */
    down(BTN_EDIT); run_ms(400);
    CHECK(app.layer == L_ERASE && sim.leds[BTN_EDIT] == LED_BREATH && sim.leds[BTN_SEQ] == LED_OFF);
    black(2, 40);                                             /* CLR */
    CHECK(keys()[bm_black_key(5)].led == LED_MID && keys()[bm_black_key(6)].led == LED_OFF);   /* B6 deshacer sí, B7 rehacer no */
    CHECK(!strcmp(keys()[bm_black_key(5)].label, "UND") && !strcmp(keys()[bm_black_key(6)].label, "RED"));
    CHECK(sim.leds[BTN_OCT_DN] == LED_OFF && sim.leds[BTN_OCT_UP] == LED_OFF);   /* OCT: la octava (en la batería, nada) */
    black(5, 40);                                             /* B6 = UNDO */
    any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(any && keys()[bm_black_key(6)].led == LED_MID);
    black(6, 40);                                             /* B7 = REDO */
    up(BTN_EDIT); run_ms(60);
    any = 0;
    for (int l = 0; l < 16; l++) for (int s = 0; s < 64; s++) if (app.pat.tr[0].st[l][s]) any = 1;
    CHECK(!any);
    /* HOME sostenido: solo los botones con HOME + */
    down(BTN_HOME); run_ms(400);
    CHECK(app.layer == L_HOME && sim.leds[BTN_HOME] == LED_ON);
    CHECK(sim.leds[BTN_SEQ] == LED_MID && sim.leds[BTN_FX] == LED_MID && sim.leds[BTN_GLO] == LED_MID && sim.leds[BTN_OCT_DN] == LED_MID);
    CHECK(sim.leds[BTN_ENV] == LED_OFF && sim.leds[BTN_LFO] == LED_OFF && sim.leds[BTN_ARP] == LED_OFF && sim.leds[BTN_OCT_UP] == LED_OFF);
    up(BTN_HOME); run_ms(60);
}

static void test_free_engines(void)
{
    printf("máquinas: BR-8 BP-1 y las de notas, ALGORITHM = el modo de cada una, MORE > MACHINE, MIDI OUT\n");
    boot(1);
    static const char *const NM[BM_TRACKS] = {"BR-8", "BP-1", "BB-303", "BX-7", "BITTAL", "BUNO-6"};
    for (int t = 0; t < BM_TRACKS; t++) CHECK(strcmp(bm_track_name(&app, t), NM[t]) == 0);
    /* T3 BB-303: ALGORITHM = la forma de onda (ya no cambia de máquina) */
    ch(2);
    enc(ENC_ALGO, 1);
    CHECK(app.pat.trk[2].engine == ENG_ACID && app.pat.prm[2][AC_WAVE] == 1);
    enc(ENC_ALGO, -1);
    CHECK(app.pat.prm[2][AC_WAVE] == 0);
    /* el GLO de la máquina (MORE adentro, 0.9): notas = VIEW LENGTH SPEED TRANSPOSE ACCENT MIDI OUT INIT SOUND */
    btn(BTN_GLO, 30);
    const char *nm; char v[24];
    static const char *const NOTE_ROWS[7] = {"VIEW", "LENGTH", "SPEED", "TRANSPOSE", "ACCENT", "MIDI OUT", "INIT SOUND"};
    CHECK(app.page == PG_MORE && bm_app_more_count(&app) == 7);
    for (int i = 0; i < 7; i++) { bm_app_more_row(&app, i, &nm, v, sizeof v); CHECK(strcmp(nm, NOTE_ROWS[i]) == 0); }
    bm_app_more_row(&app, 0, &nm, v, sizeof v);
    CHECK(strcmp(v, "NOTES") == 0);
    shot("45b-machine-glo-note");
    btn(BTN_GLO, 30);                                           /* GLO otra vez: de vuelta */
    CHECK(app.page == PG_SEQ);
    ch(0);                                                      /* BR-8: LENGTH SPEED ACCENT INIT KIT */
    btn(BTN_GLO, 30);
    CHECK(bm_app_more_count(&app) == 4);
    bm_app_more_row(&app, 3, &nm, v, sizeof v);
    CHECK(strcmp(nm, "INIT KIT") == 0);
    ch(1);                                                      /* BP-1: LENGTH SPEED ACCENT INTERPOLATION INIT */
    btn(BTN_GLO, 30);
    CHECK(bm_app_more_count(&app) == 5);
    bm_app_more_row(&app, 3, &nm, v, sizeof v);
    CHECK(strcmp(nm, "INTERPOLATION") == 0);
    bm_app_more_row(&app, 4, &nm, v, sizeof v);
    CHECK(strcmp(nm, "INIT") == 0);
    /* T6 con MIDI OUT: suena y además manda sus notas por MIDI (y aparece la fila MIDI CHANNEL) */
    ch(5);
    btn(BTN_GLO, 30);
    CHECK(app.page == PG_MORE);
    int row = -1;
    for (int i = 0; i < bm_app_more_count(&app); i++) { bm_app_more_row(&app, i, &nm, v, sizeof v); if (strcmp(nm, "MIDI OUT") == 0) row = i; }
    CHECK(row >= 0);
    while (app.more_row < row) enc(ENC_SELECT, 1);
    enc(ENC_ALGO, 1); run_ms(40);
    CHECK(app.pat.trk[5].midi_out == 1 && bm_app_more_count(&app) == 8);
    sim.midi_n = 0;
    btn(BTN_PLAY, 20); run_ms(1500);
    int on = 0, voice = 0;
    for (int i = 0; i + 2 < sim.midi_n; i++) if (sim.midi_out[i] == (0x90 | 5)) on = 1;
    for (int i = 0; i < BM_VOICES; i++) if (app.ctx.voice[i].active && app.ctx.voice[i].track == 5) voice = 1;
    CHECK(on && voice);
    btn(BTN_PLAY, 20);
    shot("48-midi-out");
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
    enc(ENC_K4, -1);                                             /* a la izquierda OFF, sin dar la vuelta */
    CHECK(!(app.pat.pad[1].flags & PAD_TEMPO));
    enc(ENC_K4, -1);
    CHECK(!(app.pat.pad[1].flags & PAD_TEMPO));
    run_ms(UI_TOAST_MS);
    shot("37c-pads-env");
    btn(BTN_EDIT, 30);                                           /* LOOP: el sustain dentro del pad */
    CHECK(app.ed_page[1] == 3);
    enc(ENC_K1, 40); enc(ENC_K2, 120);
    CHECK(app.pat.pad[1].mode == PAD_LOOP && bm_pad_has_loop(&app.pat.pad[1]));
    shot("37e-pads-loop");
    kdown(bm_white_key(1)); run_ms(700);                         /* sostenido: repite el tramo del loop */
    CHECK(t2_voices(NULL, NULL) == 1 && app.ctx.st.smp_pos < app.pat.pad[1].start + bm_pad_loop_end(&app.pat.pad[1]) + 64);
    kup(bm_white_key(1)); run_ms(500);
    CHECK(t2_voices(NULL, NULL) == 0);
    app.pat.pad[1].loop_s = 0; app.pat.pad[1].loop_e = 0; app.pat.pad[1].mode = PAD_ONESHOT;
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
    /* MAS, BREAK y TAKES con SELECT: primero las sub-páginas de PADS, después la pestaña */
    int pg_steps = 0;
    while (app.page == PG_PADS && pg_steps < 6) { enc(ENC_SELECT, 1); pg_steps++; }
    CHECK(pg_steps >= 2);                                        /* PADS tenía sub-páginas por recorrer */
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
    /* N2: un kit de 16 partes iguales; el kit ya tenía pads, así que pregunta (2 s sostenida = sí) */
    kdown(bm_black_key(1)); run_ms(100);
    CHECK(app.ask == ASK_KIT);
    run_ms(2000); kup(bm_black_key(1)); run_ms(60);
    CHECK(app.ask == ASK_NONE);
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
    hold_key(bm_black_key(10), 2200);                        /* DEL: 2 s sostenida = sí */
    CHECK(app.ask == ASK_NONE);
    run_ms(600);
    CHECK(!app.take[0].valid && app_kit_empty(&app));
    /* HOME en una toma sin guardar: pregunta (HOME otra vez = no: la toma queda; HOME sostenido 2 s = sí) */
    btn(BTN_REC, 30); btn(BTN_PLAY, 20); run_ms(800); btn(BTN_REC, 30);
    CHECK(app.ctx.st.smp_unsaved);
    btn(BTN_HOME, 30); run_ms(60);
    CHECK(app.ask == ASK_DISCARD && app.ctx.st.smp_unsaved && !app.ask_live);
    shot("46b-ask-discard");
    btn(BTN_HOME, 30); run_ms(60);
    CHECK(app.ask == ASK_NONE && app.ctx.st.smp_unsaved && app.view == V_HOME);   /* NO: la toma queda y HOME sale */
    app_enter(&app, 1, PG_REC);
    btn(BTN_HOME, 30); run_ms(60);
    CHECK(app.ask == ASK_DISCARD && sim.leds[BTN_HOME] == LED_BLINK);
    down(BTN_HOME); run_ms(1000);
    CHECK(app.ask == ASK_DISCARD && app.layer == L_NONE);    /* sostenerla no abre la capa de HOME */
    run_ms(1200); up(BTN_HOME); run_ms(60);
    CHECK(app.ask == ASK_NONE && app.view == V_CH && app.layer == L_NONE);   /* ni SYSTEM a los 2 s */
    CHECK(!app.ctx.st.smp_unsaved && app.ctx.smp[0].len == 0);
    btn(BTN_PLAY, 20);
}

static void test_free_rec(void)
{
    printf("grabación libre: el largo se cierra en compases y cada nota guarda lo que se sostuvo\n");
    boot(1);
    ch(5);
    down(BTN_EDIT); run_ms(400); black(2, 40); up(BTN_EDIT); run_ms(60);   /* vaciar T6 (BORRAR + CLR) */
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
    printf("BX-7: los 16 patches suenan sin saturar, las voces se liberan y ALGORITHM elige el algoritmo\n");
    boot(1);
    ch(3);
    CHECK(app.pat.trk[3].engine == ENG_FM);
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
    /* ALGORITHM: uno por uno de los 32, y el dibujo de la topología */
    app.pat.prm[3][FX_ALG] = 0;                                /* 0 = el del patch */
    int a0 = bm_fm_alg_of(&app.pat.trk[3], app.pat.prm[3]);
    enc(ENC_ALGO, 1);
    CHECK(bm_fm_alg_of(&app.pat.trk[3], app.pat.prm[3]) == (a0 < 32 ? a0 + 1 : 32) && app.pat.prm[3][FX_ALG] == bm_fm_alg_of(&app.pat.trk[3], app.pat.prm[3]));
    for (int i = 0; i < 40; i++) enc(ENC_ALGO, -1);
    CHECK(bm_fm_alg_of(&app.pat.trk[3], app.pat.prm[3]) == 1);
    /* el LFO encima del patch: 0 = el del patch, si no el valor - 1 */
    uint8_t fp[BM_FM_PATCH];
    app.pat.prm[3][FX_LSPD] = 51;
    bm_fm_patch_of(&app.pat.trk[3], app.pat.prm[3], NULL, fp);
    CHECK(fp[137] == 50 && fp[BM_FM_ALG] == 0);
    app.pat.prm[3][FX_LSPD] = 0;
    bm_fm_patch_of(&app.pat.trk[3], app.pat.prm[3], NULL, fp);
    CHECK(fp[137] == bm_fm_patch(app.pat.trk[3].source)[137]);
    uint8_t mods[6]; int8_t fb[2];
    bm_fm_alg_graph(0, mods, fb);                              /* algoritmo 1: 2>1 y 6>5>4>3, realimenta el 6 */
    CHECK(mods[5] == (1u << 4) && mods[3] == (1u << 2) && mods[2] == (1u << 1) && mods[1] == (1u << 0) && fb[0] == 0 && fb[1] == 0);
    bm_fm_alg_graph(31, mods, fb);                             /* algoritmo 32: seis portadoras */
    CHECK(bm_fm_carriers(31) == 0x3F && !mods[0] && !mods[5]);
    for (int i = 0; i < 4; i++) enc(ENC_ALGO, 1);
    shot("56b-fm-alg5");
}

static void test_engines(void)
{
    printf("máquinas 0.6: BB-303, B-MINI, BITTAL y BUNO-6 con el banco de fábrica, y la batería modelada\n");
    boot(1);
    static const uint8_t bass[4] = {36, 43, 48, 39};
    CHECK(app.pat.trk[2].engine == ENG_ACID);                    /* la demo: acid techno */
    ch(2);
    page(BTN_EDIT, PG_SOUND);
    shot("53-acid");
    double p_acid = solo_peak(2, bass, 4, 100, 250);
    double p_acc = solo_peak(2, bass, 4, 127, 250);
    int acid_ok = 0;                                             /* los sonidos de fábrica del BB-303 */
    for (int i = 0; i < BM_AC_PRESETS; i++) {
        while (app.pat.trk[2].source != (i + 1) % BM_AC_PRESETS) { enc(ENC_PRESETS, 1); run_ms(20); }
        double pk = solo_peak(2, bass, 2, 100, 250);
        if (pk > 0.05 && pk < 1.0) acid_ok++; else printf("  BB-303 %s: pico %.2f\n", bm_sound_name(&app, 2), pk);
    }
    CHECK(acid_ok == BM_AC_PRESETS && app.pat.trk[2].source == 0 && strcmp(bm_sound_name(&app, 2), "CLASSIC") == 0);
    app.ed_page[2] = 1;
    run_ms(1600);                                                /* sin el aviso de PRESETS encima */
    shot("53b-acid-env");
    CHECK(app_set_machine(&app, 2, ENG_MINI) == 0);              /* T3: B-MINI */
    run_ms(40);
    app.ed_page[2] = 5;                                          /* FILTER */
    CHECK(app.pat.trk[2].engine == ENG_MINI && bm_app_ed_on(&app));
    double p_mini = solo_peak(2, bass, 4, 100, 250);
    printf("  pico BB-303 %.2f (acento %.2f) · B-MINI %.2f\n", p_acid, p_acc, p_mini);
    CHECK(p_acid > 0.05 && p_acid < 1.0 && p_acc >= p_acid);
    CHECK(p_mini > 0.05 && p_mini < 1.0);
    shot("54-bmini");
    app.ed_page[2] = 0;                                          /* CONTROL: MOD AMT lleva el destino */
    app.pat.prm[2][MN_LAMT] = 0;
    enc(ENC_K4, -10); run_ms(20);
    CHECK(app.pat.prm[2][MN_LAMT] > 0 && app.pat.prm[2][MN_LDEST] == MN_D_PITCH);
    enc(ENC_K4, 30); run_ms(20);
    CHECK(app.pat.prm[2][MN_LAMT] > 0 && app.pat.prm[2][MN_LDEST] == MN_D_CUT);
    shot("54b-bmini-control");
    app.ed_page[2] = 5;

    /* el banco de fábrica: cada sonido de cada máquina suena sin saturar */
    static const uint8_t chord[3] = {57, 60, 64};
    static const struct { int t, eng; const char *wav; } M[3] = {{2, ENG_MINI, "bmini.wav"}, {4, ENG_BITTAL, "bittal.wav"}, {5, ENG_BUNO, "buno6.wav"}};
    for (int m = 0; m < 3; m++) {
        int t = M[m].t, n = bm_snap_count(M[m].eng);
        CHECK(n >= 6);
        for (int i = 0; i < n; i++) CHECK(bm_snap_ok(bm_snap_get(M[m].eng, i), BM_SNAP_MAX));
        ch(t);
        page(BTN_EDIT, PG_SOUND);
        wav_open(M[m].wav);
        int ok = 0;
        for (int i = 0; i < n; i++) {
            while (app.pat.trk[t].source != i) { enc(ENC_PRESETS, 1); run_ms(20); }
            run_ms(80);                                          /* BITTAL arma sus tablas */
            double pk = t == 2 ? solo_peak(t, bass, 2, 100, 400) : solo_peak(t, chord, 3, 100, 600);
            printf("  %-7s %-12s pico %.2f\n", bm_engine_names[M[m].eng], bm_snap_name(M[m].eng, i), pk);
            if (pk > 0.015 && pk < 1.0) ok++;
        }
        run_ms(1500);
        wav_close();
        CHECK(ok == n);
    }
    /* BITTAL: las tablas en RAM, SELECT recorre las páginas, ENV y LFO saltan a la suya, ALGORITHM elige tabla */
    ch(4);
    page(BTN_EDIT, PG_SOUND);
    run_ms(80);
    CHECK(bm_wt_ready(0) && bm_wt_frames(0) >= 1);
    app.ed_page[4] = 0;
    shot("57-bittal-table");
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1); enc(ENC_SELECT, 1); enc(ENC_SELECT, 1);
    CHECK(app.page == PG_SOUND && app.ed_page[4] == 4);              /* FILTER */
    int ft = app.pat.prm[4][BT_FT];
    enc(ENC_K1, 1); run_ms(20);
    CHECK(app.pat.prm[4][BT_FT] == ft + 1);
    btn(BTN_ENV, 20); run_ms(20);
    CHECK(app.ed_page[4] == bm_engine_page_of(ENG_BITTAL, EPG_ENV1) && app.ed_page[4] == 6);   /* ENV 1 */
    btn(BTN_ENV, 20); run_ms(20);
    CHECK(app.ed_page[4] == bm_engine_page_of(ENG_BITTAL, EPG_ENV2) && app.ed_page[4] == 7);   /* otra vez: ENV 2 */
    shot("58-bittal-env");
    btn(BTN_LFO, 20); run_ms(20);
    CHECK(app.ed_page[4] == bm_engine_page_of(ENG_BITTAL, EPG_LFO));
    app.ed_page[4] = 9;
    shot("59-bittal-macros");
    uint32_t sig = app.wt_sig[0];
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1); enc(ENC_ALGO, 1); run_ms(80);                                   /* otra tabla para el oscilador 1 */
    CHECK(app.pat.prm[4][BT_TAB1] == 3 && app.wt_sig[0] != sig && bm_wt_ready(0));
    app.ed_page[4] = BM_BT_PAGES - 1;
    enc(ENC_SELECT, 1);
    CHECK(app.page != PG_SOUND);
    /* BUNO-6: ALGORITHM = el chorus */
    ch(5);
    page(BTN_EDIT, PG_SOUND);
    int chm = app.pat.prm[5][BN_CHORUS];
    enc(ENC_ALGO, 1);
    CHECK(app.pat.prm[5][BN_CHORUS] == (chm + 1) % BN_CHORUSES);
    app.ed_page[5] = 2;
    run_ms(1600);                                                /* sin el aviso del chorus encima */
    shot("60-buno-dco");
    app.ed_page[5] = 7;
    shot("60b-buno-chorus");
    btn(BTN_LFO, 20); run_ms(20);                                /* LFO: la página LFO (la primera, como en el panel) */
    CHECK(app.page == PG_SOUND && app.ed_page[5] == 0);
    btn(BTN_ENV, 20); run_ms(20);
    CHECK(app.ed_page[5] == 6);
    app.ed_page[5] = 4;                                          /* VCF MOD: K2 = la macro LFO (la de SEQ) */
    int lfo = app.pat.trk[5].macro[3];
    enc(ENC_K2, 2); run_ms(20);
    CHECK(app.pat.trk[5].macro[3] == lfo + 2);
    shot("60c-buno-vcf-mod");

    /* batería modelada: los 3 kits, los 16 instrumentos de cada uno suenan */
    ch(0);
    page(BTN_EDIT, PG_SOUND);
    shot("55-bateria");
    wav_open("bateria.wav");
    for (int k = 0; k < BM_KITS_N; k++) {
        while (app.pat.trk[0].source != k) { enc(ENC_PRESETS, 1); run_ms(20); }
        int ok = 0;
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
    shot("61-instrumento");
    int tune = app.pat.tr[0].lane[app.sel_lane[0]].tune;
    enc(ENC_K1, 2); run_ms(20);
    CHECK(app.pat.tr[0].lane[app.sel_lane[0]].tune == tune + 2);
    btn(BTN_EDIT, 20); run_ms(20);                               /* página INSTRUMENTO: el modelo */
    CHECK(app.page == PG_INST && app.ed_page[0] == 1);
    int model = app.pat.tr[0].lane[app.sel_lane[0]].model;
    enc(ENC_K1, 1); run_ms(20);
    CHECK(app.pat.tr[0].lane[app.sel_lane[0]].model == (model + 1) % DRM_COUNT || model == DRM_COUNT - 1);
    shot("61b-instrumento-modelo");

    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    peak_abs = 0;
    wav_open("motores.wav");                                    /* la demo con los motores nuevos */
    btn(BTN_PLAY, 20); run_ms(8000); btn(BTN_PLAY, 20);
    wav_close();
    CHECK(peak_abs > 0.05 && peak_abs <= 1.0);
}

/* ------------------------------------------------------------ MACHINE: la máquina de T3-T6 */
static int notes_of(int t)
{
    int n = 0;
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) n += !bm_event_empty(bm_evc(&app.pat, t, s, k));
    return n;
}

static void test_machines(void)
{
    printf("máquinas en T3-T6: MACHINE (2 s en una la carga), las pesadas una vez, presets por máquina\n");
    /* el núcleo: qué máquina va en cada canal, las pesadas y el cambio */
    for (int t = 2; t < BM_TRACKS; t++)
        for (int e = ENG_ACID; e < ENG_COUNT; e++) CHECK(bm_engine_ok(t, e));
    CHECK(bm_engine_ok(0, ENG_DRUM) && bm_engine_ok(1, ENG_BREAK) && !bm_engine_ok(0, ENG_FM) && !bm_engine_ok(1, ENG_ACID));
    CHECK(!bm_engine_ok(2, ENG_DRUM) && !bm_engine_ok(5, ENG_BREAK));
    CHECK(bm_engine_heavy(ENG_BUNO) && bm_engine_heavy(ENG_BITTAL) && !bm_engine_heavy(ENG_FM) && !bm_engine_heavy(ENG_ACID) && !bm_engine_heavy(ENG_MINI));
    static bm_pattern p, q;
    static uint8_t b[BM_PENDING_LEN];
    bm_pattern_init(&p);
    CHECK(bm_machine_conflict(&p, 2, ENG_BUNO) == 5 && bm_machine_conflict(&p, 2, ENG_BITTAL) == 4);
    CHECK(bm_machine_conflict(&p, 2, ENG_FM) < 0 && bm_machine_conflict(&p, 5, ENG_BUNO) < 0);
    bm_track_set_engine(&p, 2, ENG_BUNO);                               /* BUNO-6 ya está en T6: no */
    CHECK(p.trk[2].engine == ENG_ACID);
    bm_event *e = bm_ev(&p, 3, 4, 0);
    e->note = 60; e->vel = 99; e->len = 3; e->lk[1] = 99;
    p.trk[3].length = 24; p.trk[3].level = 77;
    bm_track_set_engine(&p, 3, ENG_MINI);                               /* las notas quedan, sin P-Locks */
    CHECK(p.trk[3].engine == ENG_MINI && e->note == 60 && e->vel == 99 && e->len == 3 && e->lk[1] == BM_LOCK_NONE);
    CHECK(p.trk[3].length == 24 && p.trk[3].level == 77 && p.trk[3].source == 0);
    CHECK(memcmp(p.trk[3].macro, bm_snap_get(ENG_MINI, 0) + 36, 4) == 0);
    /* al leer un patrón, una pesada repetida vuelve a la máquina de fábrica de su canal */
    p.trk[3].engine = ENG_BUNO;
    int n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(n > 0 && bm_pattern_unpack(&q, b, n) == n);
    CHECK(q.trk[5].engine == ENG_BUNO && q.trk[3].engine == ENG_FM && bm_evc(&q, 3, 4, 0)->note == 60);
    p.trk[4].engine = ENG_FM; p.trk[2].engine = ENG_BITTAL; p.trk[3].engine = ENG_BITTAL;   /* ninguna en la suya: queda la primera */
    n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(bm_pattern_unpack(&q, b, n) == n && q.trk[2].engine == ENG_BITTAL && q.trk[3].engine == ENG_FM && q.trk[4].engine == ENG_FM);

    /* PRESETS: solo los sonidos de la máquina del canal */
    boot(1);
    CHECK(bm_sound_count(&app, 2) == BM_AC_PRESETS && bm_sound_count(&app, 3) == bm_fm_count());
    CHECK(bm_sound_count(&app, 4) == bm_snap_count(ENG_BITTAL) && bm_sound_count(&app, 5) == bm_snap_count(ENG_BUNO));
    ch(2);
    enc(ENC_PRESETS, -1); run_ms(20);                                   /* da la vuelta dentro del BB-303 */
    CHECK(app.pat.trk[2].source == BM_AC_PRESETS - 1 && app.pat.trk[2].engine == ENG_ACID);
    CHECK(app.pat.trk[2].macro[0] == bm_ac_presets[BM_AC_PRESETS - 1].cut && strcmp(bm_sound_name(&app, 2), bm_ac_presets[BM_AC_PRESETS - 1].name) == 0);

    /* la tarjeta del canal (0.9): en HOME, la blanca de T4 sostenida + ALGORITHM; 2 s en una la carga */
    for (int i = 0; i < 4 && app.view != V_HOME; i++) btn(BTN_HOME, 30);
    int n4 = notes_of(3);
    CHECK(n4 > 0);
    int k4 = bm_white_key(BM_PATTERNS + 3);
    kdown(k4); run_ms(150);
    CHECK(app.track == 3 && !app.card);                                 /* al bajar: el cursor */
    run_ms(300);
    CHECK(app.card == 4 && app.mach_trk == 3 && app.mach_sel == ENG_FM && !app.mach_arm);
    CHECK(keys()[k4].led == LED_BREATH);
    const char *s, *al, *pr;
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(al, "MACHINE") == 0 && strcmp(pr, "PATCH") == 0);
    shot("67-card");
    enc(ENC_ALGO, -1);                                                  /* a B-MINI: corre la cuenta */
    CHECK(app.mach_sel == ENG_MINI && app.mach_arm == 1 && app.card_turn);
    run_ms(1000);
    shot("67b-card-countdown");
    CHECK(app.pat.trk[3].engine == ENG_FM);
    enc(ENC_ALGO, 1);                                                   /* irse antes de los 2 s: nada */
    CHECK(app.mach_sel == ENG_FM && !app.mach_arm);
    run_ms(2500);
    CHECK(app.pat.trk[3].engine == ENG_FM);
    enc(ENC_ALGO, -1); run_ms(1200); enc(ENC_ALGO, -1); run_ms(1200);   /* B-MINI 1.2 s y luego BB-303 1.2 s */
    CHECK(app.pat.trk[3].engine == ENG_FM && app.mach_sel == ENG_ACID);
    enc(ENC_ALGO, 1); run_ms(2200);                                     /* quedarse 2 s: T4 es B-MINI */
    CHECK(app.pat.trk[3].engine == ENG_MINI && notes_of(3) == n4 && app.card == 4 && !app.mach_arm);
    CHECK(strcmp(app.toast, "T4 = B-MINI") == 0 && app.pat.trk[3].source == 0);
    enc(ENC_ALGO, 2);                                                   /* BITTAL ya está en T5: apagada, sin cuenta */
    CHECK(app.mach_sel == ENG_BITTAL && !app.mach_arm && strcmp(app.toast, "BITTAL ALREADY ON T5") == 0);
    shot("67c-card-heavy");
    run_ms(2500);
    CHECK(app.pat.trk[3].engine == ENG_MINI);
    enc(ENC_ALGO, -1); run_ms(500);                                     /* soltar la tarjeta corta la cuenta */
    CHECK(app.mach_arm == 1);
    kup(k4); run_ms(60);
    CHECK(app.view == V_HOME && !app.card && !app.mach_arm);            /* se giró una perilla: soltar no entra */
    run_ms(2000);
    CHECK(app.pat.trk[3].engine == ENG_MINI);
    kdown(k4); run_ms(400);                                             /* sin girar nada: soltar = entrar */
    CHECK(app.card == 4);
    kup(k4); run_ms(60);
    CHECK(app.view == V_CH && app.track == 3 && !app.card);
    btn(BTN_HOME, 30);
    /* T1 y T2: máquinas fijas; PRESETS en la tarjeta = su sonido */
    int k1 = bm_white_key(BM_PATTERNS);
    kdown(k1); run_ms(400);
    enc(ENC_ALGO, 1);
    CHECK(app.card == 1 && app.pat.trk[0].engine == ENG_DRUM && strcmp(app.toast, "T1 AND T2 ARE FIXED MACHINES") == 0);
    kup(k1); run_ms(60);
    CHECK(app.view == V_HOME);
    int k3 = bm_white_key(BM_PATTERNS + 2), src3 = app.pat.trk[2].source;
    kdown(k3); run_ms(400);
    enc(ENC_PRESETS, 1); run_ms(40);
    CHECK(app.card == 3 && app.pat.trk[2].source == (src3 + 1) % BM_AC_PRESETS);
    kup(k3); run_ms(60);
    CHECK(app.view == V_HOME && app.track == 2);
    /* GLO > MACHINES: SELECT el canal (T3-T6), ALGORITHM la máquina con la misma espera */
    btn(BTN_GLO, 30); btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO && app.glo_page == GP_MACH);
    enc(ENC_SELECT, -3);
    CHECK(app.track == 2);                                              /* no baja a T1 / T2: fijos */
    enc(ENC_ALGO, 1);
    CHECK(app.mach_trk == 2 && app.mach_sel == ENG_MINI && app.mach_arm == 1);
    run_ms(900);
    shot("68-glo-machines");
    run_ms(1300);
    CHECK(app.pat.trk[2].engine == ENG_MINI && app.pat.trk[3].engine == ENG_MINI);
    btn(BTN_HOME, 30);

    /* BX-7 en T3: polifónico (las voces van por máquina, no por canal) */
    CHECK(app_set_machine(&app, 2, ENG_FM) == 0);
    run_ms(40);
    CHECK(app.pat.trk[2].engine == ENG_FM);
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != 2);
    bm_cmd c; memset(&c, 0, sizeof c);
    static const uint8_t ch3[3] = {48, 55, 64};
    for (int i = 0; i < 3; i++) { c.type = CMD_NOTE_ON; c.a = 2; c.b = ch3[i]; c.c = 100; bm_cmd_push(&app.cmdq, &c); }
    run_ms(60);
    int v3 = 0;
    for (int i = 0; i < BM_VOICES; i++) v3 += app.ctx.voice[i].active && app.ctx.voice[i].track == 2;
    CHECK(v3 == 3);
    c.type = CMD_ALL_OFF; bm_cmd_push(&app.cmdq, &c);
    run_ms(300);

    /* HOME + EDIT: copiar T6 (BUNO-6) a otro canal no duplica la pesada */
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    ch(5);
    down(BTN_HOME); run_ms(40); btn(BTN_EDIT, 30); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_COPYCH && app.copy_src == 5);
    int dst = app.copy_dst, de = app.pat.trk[dst].engine;
    btn(BTN_EDIT, 2200); run_ms(100);                                   /* EDIT 2 s = pegar */
    CHECK(app.pat.trk[dst].engine == de && strcmp(app.toast, "BUNO-6 ALREADY ON T6") == 0);
    CHECK(app_machine_track(&app, ENG_BUNO) == 5);

    /* copiar T6 a otro patrón que ya tiene BUNO-6 en otro canal: no se copia */
    btn(BTN_HOME, 30);
    CHECK(app_set_machine(&app, 5, ENG_ACID) == 0); run_ms(40);
    CHECK(app_set_machine(&app, 2, ENG_BUNO) == 0); run_ms(40);
    app_job(&app, JOB_COPY_PAT, 1); run_ms(400);                        /* A02: BUNO-6 en T3 */
    CHECK(app_set_machine(&app, 2, ENG_ACID) == 0); run_ms(40);
    CHECK(app_set_machine(&app, 5, ENG_BUNO) == 0); run_ms(40);
    app_job(&app, JOB_COPY_TRK, 1 | (5 << 4)); run_ms(400);
    CHECK(strcmp(app.toast, "BUNO-6 ALREADY ON T3") == 0);
    int len = bm_store_read(&app.store, 1, b, sizeof b);
    CHECK(len > 0 && bm_pattern_unpack(&q, b, len) == len && q.trk[2].engine == ENG_BUNO && q.trk[5].engine == ENG_ACID);
    CHECK(app.pat.trk[5].engine == ENG_BUNO && app.pat.trk[2].engine == ENG_ACID);
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
    b[offsetof(bm_meta, fmt)] = 7;                                /* 0.7.0: otras páginas y máquinas fijas */
    CHECK(BM_PAT_FMT == 8 && bm_pattern_unpack(&q, b, n) < 0);
    /* al arrancar, un patrón de formato 7 en la flash se borra (sin conversión) */
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static bm_store st;
    bm_store_mount(&st, pf.flash);
    bm_settings s; memset(&s, 0, sizeof s); s.ver = BM_FMT;
    bm_store_write_glob(&st, (const uint8_t *)&s, sizeof s);
    bm_store_write(&st, 3, b, (uint16_t)n);
    CHECK(bm_store_has(&st, 3));
    boot(0);
    CHECK(!bm_store_has(&app.store, 3) && app.pat.trk[2].engine == ENG_ACID);
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
    /* REC sostenido 2.5 s mientras graba notas: solo graba, nunca borra la pista */
    int ev0 = 0, ev1 = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) ev0 += !bm_event_empty(bm_evc(&app.pat, 2, s, k));
    CHECK(ev0 > 0);
    run_ms(100);
    down(BTN_REC); run_ms(400);
    CHECK(app.rec && app.rec_hold);
    key(bm_white_key(4), 150);                                     /* una nota grabada */
    run_ms(2000);
    {
        int hid = -1;
        CHECK(bm_gesture_progress(&app.ges, app.now, &hid) < 0);   /* sin anillo: tocar canceló los 2 s */
    }
    up(BTN_REC); run_ms(60);
    for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) ev1 += !bm_event_empty(bm_evc(&app.pat, 2, s, k));
    CHECK(!app.rec);
    CHECK(strncmp(app.toast, "T3 ERASED", 9) != 0);
    CHECK(ev1 >= ev0 - 2);                                         /* la nota nueva puede reemplazar un paso, no borra la pista */
    btn(BTN_PLAY, 20);
    /* BB-303 no tiene LFO: un toque deja abierta la capa LIGAR */
    CHECK(strcmp(bm_track_name(&app, 2), "BB-303") == 0);
    btn(BTN_LFO, 30);
    {
        bm_keyfn K[BM_KEYS]; char title[24]; uint16_t col; const char *h1, *h2;
        bm_app_keymap(&app, K, title, sizeof title, &col, &h1, &h2);
        CHECK(app.layer == L_REP && app.layer_latch && strcmp(title, "TIE") == 0 && sim.leds[BTN_LFO] == LED_BREATH);
    }
    btn(BTN_LFO, 30);
    CHECK(app.layer == L_NONE && !app.layer_latch);
}

/* ------------------------------------------------------------ guardado y versiones */
/* el audio en dos núcleos: repartido (aquí en la misma tarea) suena exactamente igual */
static void par_go_sim(void *u, uint32_t done, uint32_t n) { bm_audio_par_process((bm_ctx *)u, done, n); }
static int par_wait_sim(void *u) { (void)u; return 0; }

/* el snprintf propio (el del FM-1 no entiende %+d ni %f): igual que el de la biblioteca */
static void test_printf(void)
{
    printf("snprintf propio: igual que el de la biblioteca en los formatos de la pantalla\n");
    char a[64], b[64];
    int bad = 0, n = 0;
#define CMP(...) do { int ra = bm_snprintf(a, sizeof a, __VA_ARGS__), rb = snprintf(b, sizeof b, __VA_ARGS__); n++; \
        if (ra != rb || strcmp(a, b)) { bad++; printf("  distinto: [%s] [%s]\n", a, b); } } while (0)
    CMP("%s  %+d ST  %u MS%s%s", "ONESHOT", 3, 162u, "  REV", "");
    CMP("%+d", 0); CMP("%+d", -7); CMP("%+d ST", 12); CMP("% d", 5); CMP("%d", -2147483647 - 1);
    CMP("%02d", 7); CMP("%02d", -3); CMP("%03u", 5u); CMP("%2d", 123); CMP("%-4d|", 9); CMP("%04X", 0xABu);
    CMP("%x %X %#x %o", 255u, 255u, 255u, 8u); CMP("%u", 4294967295u); CMP("%lu %ld %lld", 7ul, -8l, -9ll);
    CMP("%c%c", 'O', 'K'); CMP("%5c|%-3c|", 'x', 'y'); CMP("%%"); CMP("100%%");
    CMP("%.9s", "ABCDEFGHIJKLMNOP"); CMP("%.*s", 3, "abcdef"); CMP("%8s|%-8s|", "ab", "cd"); CMP("%.0s|", "zz");
    CMP("%.1f KHZ", 1.25); CMP("%.2f HZ", 0.333); CMP("%.1f", -2.95); CMP("%f", 3.5); CMP("%6.2f|", 1.5); CMP("%.0f", 2.5);
    CMP("%d%%", 85); CMP("T%d LEVEL", 3); CMP("A%02d", 16); CMP("%u.%03uS", 1u, 45u);
    /* corto: trunca igual y devuelve lo que habría escrito */
    { int ra = bm_snprintf(a, 5, "%s", "ABCDEFGH"), rb = snprintf(b, 5, "%s", "ABCDEFGH"); n++; if (ra != rb || strcmp(a, b)) bad++; }
#undef CMP
    printf("  %d formatos, %d distintos\n", n, bad);
    CHECK(bad == 0);
}

/* 0.8.1: a kick played live from the keys triggers the sidechain duck (before, the render reset
 * the trigger after applying the commands, so only sequenced kicks ducked) */
static void test_live_kick_duck(void)
{
    printf("live kick: triggers the sidechain like a sequenced one\n");
    static int32_t out[BM_BLOCK * 2];
    boot(1);
    bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_PLAY; c.a = 0; bm_cmd_push(&app.cmdq, &c);
    for (int i = 0; i < 2000 && (i < 4 || app.ctx.sc_env != 0.0f); i++) bm_app_audio(&app, out, BM_BLOCK);
    int lane = -1;
    for (int l = 0; l < BM_LANES && lane < 0; l++) {
        int m = bm_trk_tr(&app.pat, 0)->lane[l].model % DRM_COUNT;
        if (m == DRM_BD_ANALOG || m == DRM_BD_SINTE) lane = l;
    }
    CHECK(lane >= 0 && app.ctx.sc_env == 0.0f);
    memset(&c, 0, sizeof c); c.type = CMD_NOTE_ON; c.a = 0; c.b = (uint8_t)(lane < 0 ? 0 : lane); c.c = 110;
    bm_cmd_push(&app.cmdq, &c);
    bm_app_audio(&app, out, BM_BLOCK);
    printf("  sidechain envelope after a live kick: %.3f\n", app.ctx.sc_env);
    CHECK(app.ctx.sc_env > 0.05f);
}

/* TWO CORES: the other core does not answer once. Its tracks go silent (no waiting), the render
 * goes on with one core, and they come back when the platform clears par_quar */
static int fail_wait_n;
static void par_go_none(void *u, uint32_t done, uint32_t n) { (void)u; (void)done; (void)n; }
static int par_wait_fail(void *u) { (void)u; fail_wait_n++; return -1; }

static void test_dual_core_fail(void)
{
    printf("two cores: the other core does not answer -> its tracks silent, one core, no wait\n");
    static int32_t out[256 * 2];
    boot(1);
    bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);
    for (int i = 0; i < 40; i++) bm_app_audio(&app, out, 256);
    app.ctx.par_go = par_go_none; app.ctx.par_wait = par_wait_fail; app.ctx.par_user = &app.ctx;
    app.ctx.par_mask = 0x3E; app.ctx.par_on = 1; fail_wait_n = 0;
    bm_app_audio(&app, out, 256);
    CHECK(fail_wait_n == 1 && app.ctx.par_on == 0 && app.ctx.par_quar == 0x3E);
    int heard = 0, other = 0;
    for (int i = 0; i < 200; i++) {
        bm_app_audio(&app, out, 256);
        heard |= app.ctx.tact & 1;
        other |= app.ctx.tact & 0x3E;
    }
    printf("  while quarantined: T1 sounds %d, T2-T6 sound %d, waits %d\n", heard, other != 0, fail_wait_n);
    CHECK(heard && !other && fail_wait_n == 1);
    app.ctx.par_quar = 0;                     /* the platform saw the other core idle */
    for (int i = 0; i < 400; i++) { bm_app_audio(&app, out, 256); other |= app.ctx.tact & 0x3E; }
    CHECK(other != 0);
    app.ctx.par_go = NULL; app.ctx.par_wait = NULL;
}

static void test_dual_core(void)
{
    printf("audio en dos núcleos: T4-T6 (o T1-T3) aparte dan las mismas muestras\n");
    static int32_t a1[44118 * 2], a2[44118 * 2];
    static const uint8_t MK[2] = {0x38, 0x07};
    int diff = 0;
    double pk = 0;
    for (int mk = 0; mk < 2; mk++) {
        for (int pass = 0; pass < 2; pass++) {
            boot(1);
            app.ctx.par_go = par_go_sim; app.ctx.par_wait = par_wait_sim; app.ctx.par_user = &app.ctx;
            app.ctx.par_mask = MK[mk]; app.ctx.par_on = (uint8_t)pass;
            bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);
            int32_t *dst = pass ? a2 : a1;
            for (uint32_t o = 0; o < 44118; o += 128) bm_app_audio(&app, dst + o * 2, 44118 - o < 128 ? (44118 - o) & ~1u : 128);
            app.ctx.par_on = 0;
        }
        for (int i = 0; i < 44118 * 2; i++) { if (a1[i] != a2[i]) diff++; if (fabs(a1[i] / 8388608.0) > pk) pk = fabs(a1[i] / 8388608.0); }
    }
    printf("  1 s de la demo, dos repartos: %d muestras distintas, pico %.2f\n", diff, pk);
    CHECK(diff == 0 && pk > 0.05);

    /* el reparto cambia entre renders (como el que se ajusta solo): los canales pasan de un
     * núcleo al otro con voces sonando y tiene que dar lo mismo que un núcleo */
    diff = 0;
    uint32_t r = 12345u;
    for (int pass = 0; pass < 2; pass++) {
        boot(1);
        app.ctx.par_go = par_go_sim; app.ctx.par_wait = par_wait_sim; app.ctx.par_user = &app.ctx;
        app.ctx.par_on = (uint8_t)pass;
        bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);
        int32_t *dst = pass ? a2 : a1;
        for (uint32_t o = 0; o < 44118; o += 128) {
            r = r * 1664525u + 1013904223u;
            app.ctx.par_mask = (uint8_t)((r >> 24) & 0x3Fu);
            bm_app_audio(&app, dst + o * 2, 44118 - o < 128 ? (44118 - o) & ~1u : 128);
        }
        app.ctx.par_on = 0;
    }
    for (int i = 0; i < 44118 * 2; i++) if (a1[i] != a2[i]) diff++;
    printf("  reparto distinto en cada render: %d muestras distintas\n", diff);
    CHECK(diff == 0);

    /* la elección: el más caro (la batería) se va solo al núcleo 1, y sin mejora no se mueve */
    static const uint32_t COST[BM_TRACKS] = {230, 80, 55, 50, 40, 20};
    uint8_t m = bm_audio_par_pick(COST, 180, 0x38);
    uint32_t l0 = 180, l1 = 0;
    for (int t = 0; t < BM_TRACKS; t++) { if ((m >> t) & 1) l1 += COST[t]; else l0 += COST[t]; }
    printf("  elección con los costos del aparato: reparto 0x%02X, núcleos %u y %u\n", m, l0, l1);
    CHECK((m & 1) && (l0 > l1 ? l0 - l1 : l1 - l0) <= 60);
    CHECK(bm_audio_par_pick(COST, 180, m) == m);
}

/* 0.7.1: the BUNO-6 chorus kept its write position as a free-running counter and read with
 * (float)ch_w - delay: past 2^24 samples (6.3 minutes of BUNO-6) float drops the units and the
 * interpolated read jitters (measured: 57x the high-frequency noise at minute 7). One held note
 * through CHORUS I, the second difference energy over the energy, at the start and after 6.5 min. */
static double hf_ratio(uint32_t frames)
{
    static int32_t b[128 * 2];
    double e = 0, h = 0;
    int32_t p1 = 0, p2 = 0;
    for (uint32_t f = 0; f < frames; f += 128) {
        bm_app_audio(&app, b, 128);
        for (int i = 0; i < 128; i++) {
            int32_t x = b[2 * i] >> 8;
            double d2 = (double)x - 2.0 * p1 + p2;
            e += (double)x * x; h += d2 * d2; p2 = p1; p1 = x;
        }
    }
    return e > 0 ? h / e : 0;
}

static void test_chorus_long(void)
{
    printf("BUNO-6 chorus: the same after 6.5 minutes playing\n");
    boot(1);
    for (int t = 0; t < 5; t++) app.pat.trk[t].mute = 1;
    app.pat.trk[5].macro[0] = 50; app.pat.trk[5].macro[1] = 0;
    app.pat.prm[5][BN_CHORUS] = BN_CH_I; app.pat.prm[5][BN_S] = 127; app.pat.prm[5][BN_VCA] = 1;
    app.pat.trk[5].rsend = 0; app.pat.trk[5].send = 0; app.pat.meta.rv_mix = 0;
    bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_NOTE_ON; c.a = 5; c.b = 45; c.c = 100; bm_cmd_push(&app.cmdq, &c);
    hf_ratio(44118);
    double a = hf_ratio(20u * 44118u);
    for (int k = 0; k < 19; k++) hf_ratio(20u * 44118u);                 /* to 6.7 min: ch_w past 2^24 */
    double b = hf_ratio(20u * 44118u);
    printf("  high-frequency ratio: start %.3g, after 6.7 min %.3g (%.1fx)\n", a, b, a > 0 ? b / a : 0);
    CHECK(a > 0 && b < a * 3.0);
}

/* The kick's sidechain: BUNO-6 with its chorus skipped the duck (the biggest pad was never ducked)
 * and the duck let go on the same sample it hit. Now the chorus is ducked too and the duck holds
 * 30 ms. And a NaN in a shared state of the mix is reset (and counted) instead of silencing all. */
static double rms_of(const int32_t *b, int from, int to)
{
    double e = 0;
    for (int i = from; i < to; i++) { double x = (b[2 * i] + (double)b[2 * i + 1]) * 0.5; e += x * x; }
    return sqrt(e / (to - from));
}

static void note(int t, int n, int on);

static void test_duck_and_guard(void)
{
    printf("sidechain: BUNO-6 with chorus ducks and holds; a NaN in the mix comes back\n");
    static int32_t b[344 * 64 * 2];
    const int N = 344 * 64, MS = 44;
    boot(1);
    for (int t = 1; t < 5; t++) app.pat.trk[t].mute = 1;
    for (int l = 0; l < BM_LANES; l++) app.pat.tr[0].lane[l].level = 0;    /* T1 only fires the sidechain (the demo's kick) */
    bm_track_clear_events(&app.pat, 5);                                     /* T6: a held note instead of the demo's */
    app.pat.trk[5].macro[0] = 70; app.pat.trk[5].macro[1] = 0; app.pat.trk[5].duck = 64;
    app.pat.prm[5][BN_CHORUS] = BN_CH_I; app.pat.prm[5][BN_S] = 127; app.pat.prm[5][BN_VCA] = 1;
    app.pat.trk[5].rsend = 0; app.pat.trk[5].send = 0; app.pat.meta.rv_mix = 0; app.pat.meta.sc_release = 50;
    note(5, 57, 1);
    for (int i = 0; i < 20; i++) bm_app_audio(&app, b, 2048);                 /* the pad settles */
    bm_app_audio(&app, b, 40 * MS);
    double pre = rms_of(b, 0, 40 * MS);
    bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_PLAY; c.a = 1; bm_cmd_push(&app.cmdq, &c);   /* the kick on the first step */
    for (int o = 0; o < N; o += 64) bm_app_audio(&app, b + 2 * o, 64);
    double hit = rms_of(b, 2 * MS, 12 * MS), hold = rms_of(b, 18 * MS, 28 * MS), back = rms_of(b, 200 * MS, 240 * MS);
    printf("  pad rms: before %.0f, 2-12 ms %.2fx, 18-28 ms %.2fx, 200-240 ms %.2fx\n", pre, hit / pre, hold / pre, back / pre);
    CHECK(pre > 1000.0);
    CHECK(hit < pre * 0.65 && hold < pre * 0.65);                          /* ducked ~50 % and still down at 28 ms */
    CHECK(back > pre * 0.85);                                              /* and back after the release */

    /* the delay heals by itself (it rereads its line); the master filter (closed a bit), a DJ
     * filter and the sidechain keep a NaN forever without the guard */
    c.a = 0; bm_cmd_push(&app.cmdq, &c);
    app.pat.meta.mfilt = 50;
    bm_app_audio(&app, b, 2048);
    double pre2 = rms_of(b, 1024, 2048);
    uint32_t fix0 = app.ctx.st.nan_fix;
    app.ctx.d_cur = NAN; app.ctx.mf_ic1[0] = INFINITY; app.ctx.dj_ic2[3] = NAN; app.ctx.sc_env = NAN;
    bm_app_audio(&app, b, 128);
    bm_app_audio(&app, b, 2048);
    int ok = 1;
    for (int i = 0; i < 2048 * 2; i++) if (b[i] > 8388607 || b[i] < -8388608) ok = 0;
    printf("  NaN in the delay, master filter, DJ filter and sidechain: %u fixed, pad rms after %.2fx\n",
           app.ctx.st.nan_fix - fix0, rms_of(b, 1024, 2048) / pre2);
    CHECK(app.ctx.st.nan_fix - fix0 == 3 && ok && isfinite(app.ctx.d_cur) && isfinite(app.ctx.mf_ic1[0]) && isfinite(app.ctx.sc_env));
    CHECK(pre2 > 1000.0 && rms_of(b, 1024, 2048) > pre2 * 0.5);
    app.pat.meta.mfilt = 64;
    note(5, 57, 0);
}

/* 0.7.1: the brake lowers a cap on the pool. Before, each overload faded a voice and the next notes
 * started anyway (and were faded again): with the load high all the time the poly machines went
 * quiet (on the device, 6 voices a second after 9 minutes). Now new notes steal inside the cap
 * (tails first) and the cap goes back up when the load drops. */
static int pool_voices(int track)
{
    int n = 0;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N; i++)
        n += app.ctx.voice[i].active && !app.ctx.voice[i].fade && (track < 0 || app.ctx.voice[i].track == track);
    return n;
}

static void note(int t, int n, int on)
{
    bm_cmd c; memset(&c, 0, sizeof c);
    c.type = on ? CMD_NOTE_ON : CMD_NOTE_OFF; c.a = (uint8_t)t; c.b = (uint8_t)n; c.c = 110;
    bm_cmd_push(&app.cmdq, &c);
}

static void test_brake(void)
{
    printf("voice brake: a cap that new notes respect, and that goes back up\n");
    static int32_t b[256 * 2];
    boot(1);
    bm_audio_overload(&app.ctx);                                 /* load from elsewhere, no pool voices: not below the floor */
    CHECK(app.ctx.pool_cap == BM_POOL_MIN);
    for (int i = 0; i < BM_V_POOL_N; i++) bm_audio_relief(&app.ctx);
    CHECK(app.ctx.pool_cap == 0);
    for (int k = 0; k < 6; k++) { note(3, 50 + k * 3, 1); note(5, 48 + k * 4, 1); }
    for (int k = 0; k < 4; k++) note(4, 60 + k * 3, 1);
    for (int i = 0; i < 20; i++) bm_app_audio(&app, b, 256);
    int n0 = pool_voices(-1);
    for (int i = 0; i < 40; i++) { bm_audio_overload(&app.ctx); for (int k = 0; k < 4; k++) bm_app_audio(&app, b, 256); }
    int n1 = pool_voices(-1);
    printf("  held voices %d, after the brake %d (cap %d)\n", n0, n1, app.ctx.pool_cap);
    CHECK(n0 >= 12 && app.ctx.pool_cap == BM_POOL_MIN && n1 <= BM_POOL_MIN);
    /* new notes still sound, inside the cap */
    for (int k = 0; k < 3; k++) note(5, 70 + k * 2, 1);
    for (int i = 0; i < 4; i++) bm_app_audio(&app, b, 256);
    int fresh = 0;
    for (int i = BM_V_POOL0; i < BM_V_POOL0 + BM_V_POOL_N; i++)
        fresh += app.ctx.voice[i].active && !app.ctx.voice[i].fade && app.ctx.voice[i].track == 5 && app.ctx.voice[i].note >= 70;
    printf("  three new notes on T6: %d sound, pool %d\n", fresh, pool_voices(-1));
    CHECK(fresh == 3 && pool_voices(-1) <= BM_POOL_MIN);
    /* the load dropped: the cap goes back up and a new chord gets all its voices */
    for (int i = 0; i < BM_V_POOL_N; i++) bm_audio_relief(&app.ctx);
    CHECK(app.ctx.pool_cap == 0);
    for (int k = 0; k < 6; k++) note(3, 80 + k, 1);
    for (int i = 0; i < 4; i++) bm_app_audio(&app, b, 256);
    printf("  after relief: T4 %d voices, pool %d\n", pool_voices(3), pool_voices(-1));
    CHECK(pool_voices(3) == 6 && pool_voices(-1) > BM_POOL_MIN + 4);
}

/* 0.7.1: a pad being stolen (its 2 ms fade, a note waiting) that reached the end of its take in the
 * fade dropped the waiting note: it never sounded. Now the waiting note starts. */
static void test_steal_at_end(void)
{
    printf("a voice stolen right at the end of its pad still hands over to the new note\n");
    static int32_t b[64 * 2];
    boot_loop();
    bm_cmd c; memset(&c, 0, sizeof c); c.type = CMD_NOTE_ON; c.a = 1; c.b = 0; c.c = 110; bm_cmd_push(&app.cmdq, &c);
    bm_app_audio(&app, b, 64);
    bm_voice *v = NULL;
    for (int i = 0; i < BM_VOICES; i++) if (app.ctx.voice[i].active && app.ctx.voice[i].track == 1) v = &app.ctx.voice[i];
    CHECK(v != NULL);
    if (!v) return;
    v->pos = ((v->end - v->start) << 16) - 1u;                     /* the take ends in the next sample */
    v->fade = BM_STEAL_FADE; v->pend = 1; v->pend_track = 5; v->pend_note = 60; v->pend_vel = 100;
    memset(v->pend_lk, BM_LOCK_NONE, 4); v->pend_legato = 0;
    bm_app_audio(&app, b, 64);
    printf("  after the pad ends: active %d, channel T%d, note %d\n", v->active, v->track + 1, v->note);
    CHECK(v->active && v->track == 5 && v->note == 60 && !v->pend);
}

static void test_sounds(void)
{
    printf("sonidos del usuario: un .bsnap de BITTAL y un banco de BX-7, guardados y elegidos con PRESETS\n");
    boot(1);
    int nb = bm_snap_count(ENG_BITTAL), nf = bm_fm_count();
    /* un sonido: una copia de uno de fábrica con otro nombre */
    const uint8_t *src = bm_snap_get(ENG_BITTAL, 12);
    uint32_t sz = (uint32_t)(src[10] | src[11] << 8);
    static uint8_t s[BM_SNAP_MAX];
    memcpy(s, src, sz);
    memcpy(s + 12, "MY ARP\0\0\0\0\0\0", 12);
    CHECK(bm_app_sound_load(&app, 0, 0, NULL, 0) == 0);
    for (uint32_t o = 0; o < sz; o += 256) CHECK(bm_app_sound_load(&app, 1, o, s + o, sz - o < 256 ? sz - o : 256) == 0);
    CHECK(bm_app_sound_load(&app, 2, 0, NULL, 0) == 0);
    CHECK(bm_snap_count(ENG_BITTAL) == nb + 1 && strcmp(bm_snap_name(ENG_BITTAL, nb), "MY ARP") == 0);
    /* un banco de BX-7: 32 voces (aquí, los 16 patches de fábrica dos veces) */
    static uint8_t bank[BM_FM_BANK];
    memset(bank, 0, sizeof bank);
    memcpy(bank, "BNS1", 4); bank[4] = ENG_FM; bank[7] = 1;
    bank[10] = (uint8_t)(BM_FM_BANK & 255); bank[11] = (uint8_t)(BM_FM_BANK >> 8);
    memcpy(bank + 12, "MY DX7 BANK", 11);
    for (int v = 0; v < 32; v++) memcpy(bank + BM_SNAP_HDR + v * 156, bm_fm_rom[v % BM_FM_PATCHES], 156);
    memcpy(bank + BM_SNAP_HDR + 20 * 156 + 145, "MY BELL   ", 10);
    CHECK(bm_app_sound_load(&app, 0, 0, NULL, 0) == 0);
    CHECK(bm_app_sound_load(&app, 1, 0, bank, sizeof bank) == 0);
    CHECK(bm_app_sound_load(&app, 2, 0, NULL, 0) == 0);
    CHECK(bm_fm_count() == nf + 32 && strcmp(bm_fm_patch_name(BM_FM_PATCHES + 20), "MY BELL") == 0);
    /* sonando: con el transporte en marcha no se escribe la flash */
    btn(BTN_PLAY, 20);
    CHECK(bm_app_sound_load(&app, 0, 0, NULL, 0) != 0);
    btn(BTN_PLAY, 20);
    /* PRESETS en T5 llega al sonido del usuario; en T4, a la voz del banco */
    ch(4);
    page(BTN_EDIT, PG_SOUND);
    while (app.pat.trk[4].source != nb) { enc(ENC_PRESETS, 1); run_ms(20); }
    run_ms(80);
    static const uint8_t chord[3] = {57, 60, 64};
    CHECK(solo_peak(4, chord, 3, 100, 500) > 0.015);
    ch(3);
    page(BTN_EDIT, PG_SOUND);
    app.pat.trk[3].source = (uint8_t)(BM_FM_PATCHES + 20);
    CHECK(strcmp(bm_sound_name(&app, 3), "MY BELL") == 0);
    CHECK(solo_peak(3, chord, 3, 100, 400) > 0.02);
    /* se quedan al reiniciar */
    boot(0);
    CHECK(bm_snap_count(ENG_BITTAL) == nb + 1 && bm_fm_count() == nf + 32);
    CHECK(bm_app_sound_load(&app, 3, (uint32_t)(ENG_BITTAL << 8), NULL, 0) != 0);          /* los de fábrica no se borran */
    CHECK(bm_app_sound_load(&app, 3, (uint32_t)(ENG_BITTAL << 8 | nb), NULL, 0) == 0 && bm_snap_count(ENG_BITTAL) == nb);
    CHECK(bm_app_sound_load(&app, 3, (uint32_t)(ENG_FM << 8), NULL, 0) == 0 && bm_fm_count() == nf);
}

static void test_rec_expressive(void)
{
    printf("REC expresivo: nota larga con dos blancas, gate y micro en REC, grabación en vivo y corte al compás\n");
    boot(1);
    ch(3);                                                    /* BX-7: notas en los pasos 11 y 27 */
    CHECK(app.page == PG_SEQ);
    for (int k = 0; k < BM_EVENTS; k++) { bm_event e; bm_event_clear(&e); app_push_ev(&app, 3, 0, k, &e); }
    run_ms(40);
    /* blanca 1 sostenida + blanca 5: una nota de 5 pasos, y la estela en las luces */
    kdown(bm_white_key(0)); run_ms(60);
    kdown(bm_white_key(4)); run_ms(60); kup(bm_white_key(4)); run_ms(40);
    kup(bm_white_key(0)); run_ms(60);
    const bm_event *e0 = bm_evc(&app.pat, 3, 0, 0);
    CHECK(!bm_event_empty(e0) && (e0->len & BM_LEN_MASK) == 5);
    bm_keyfn K[BM_KEYS];
    bm_app_keys(&app, K);
    CHECK(K[bm_white_key(0)].led == LED_ON && K[bm_white_key(2)].led == LED_MID && K[bm_white_key(6)].led == LED_OFF);
    shot("52-seq-long-note");
    /* REC: SELECT el paso, ALGORITHM el gate, PRESETS el micro */
    btn(BTN_REC, 30);
    CHECK(app.rec);
    enc(ENC_SELECT, -1);
    CHECK(app.rec_step == 0);
    enc(ENC_ALGO, -1); enc(ENC_ALGO, -1); enc(ENC_ALGO, -1); enc(ENC_ALGO, -1);   /* el paso entero (95) -> 75 */
    CHECK(e0->gate == 75 && !(e0->len & BM_LEN_TIE) && app.rec_gate == 75);
    for (int i = 0; i < 6; i++) enc(ENC_ALGO, 1);             /* hasta 100 = TIE */
    CHECK((e0->len & BM_LEN_TIE) && e0->gate == 0);
    enc(ENC_ALGO, -1); enc(ENC_ALGO, -1); enc(ENC_ALGO, -1);   /* 100 -> 85 */
    enc(ENC_PRESETS, 1); enc(ENC_PRESETS, 1); enc(ENC_PRESETS, 1);
    CHECK(e0->micro == 3);
    shot("51-rec-gate-micro");
    static uint8_t b1[BM_PENDING_LEN];
    static bm_pattern q;
    int n = bm_pattern_pack(&app.pat, b1, sizeof b1);
    CHECK(n > 0 && bm_pattern_unpack(&q, b1, n) == n);
    CHECK(bm_evc(&q, 3, 0, 0)->gate == 85 && bm_evc(&q, 3, 0, 0)->micro == 3 && (bm_evc(&q, 3, 0, 0)->len & BM_LEN_MASK) == 5);
    btn(BTN_REC, 30);
    CHECK(!app.rec);
    /* el micro-timing suena corrido: la nota del paso 1 entra 3 ticks tarde */
    for (int k = 0; k < BM_EVENTS; k++)                      /* (solo los que hay: la cola tiene 128 comandos) */
        for (int s = 1; s < 64; s++) if (!bm_event_empty(bm_evc(&app.pat, 3, s, k))) { bm_event e; bm_event_clear(&e); app_push_ev(&app, 3, s, k, &e); run_ms(1); }
    { bm_event e = *e0; e.micro = 10; app_push_ev(&app, 3, 0, 0, &e); }
    run_ms(400);
    down(BTN_PLAY); run_ms(1); up(BTN_PLAY);                  /* PLAY: el paso 1 empieza ya */
    int early = 0, later = 0;
    for (int i = 0; i < 4; i++) { run_ms(5); for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) early = 1; }
    run_ms(50);
    for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) later = 1;
    CHECK(!early && later);
    btn(BTN_PLAY, 20);
    /* REC en vivo: la nota guarda su largo real y el compás la corta */
    for (int k = 0; k < BM_EVENTS; k++)
        for (int s = 0; s < 64; s++) if (!bm_event_empty(bm_evc(&app.pat, 3, s, k))) { bm_event e; bm_event_clear(&e); app_push_ev(&app, 3, s, k, &e); run_ms(1); }
    { bm_event e; bm_event_clear(&e); e.note = 60; app_push_ev(&app, 3, 40, 0, &e); }   /* canal no vacío: REC con largo fijo */
    run_ms(40);
    page(BTN_EDIT, PG_SOUND);                                 /* aquí las blancas tocan notas */
    btn(BTN_REC, 30);
    down(BTN_PLAY); run_ms(1); up(BTN_PLAY);
    run_ms(1125 - 60);                                        /* hasta el paso 13 (93.75 ms por paso a 160) */
    int k5 = bm_white_key(5);
    kdown(k5); run_ms(750);                                   /* 8 pasos: cruza el compás */
    kup(k5); run_ms(40);
    btn(BTN_PLAY, 20); btn(BTN_REC, 30);
    int found = 0;
    for (int s = 0; s < 16; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&app.pat, 3, s, k);
            if (bm_event_empty(e) || s == 40) continue;
            found = 1;
            printf("  grabada en el paso %d: %d pasos, gate %d, micro %+d\n", s + 1, e->len & BM_LEN_MASK, e->gate, e->micro);
            CHECK(s + (e->len & BM_LEN_MASK) <= 16);                /* no pasa del compás */
        }
    CHECK(found);
}

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
    CHECK((app.song.scene[1].flags & SCN_STORED) && app.song.scene[1].trk_mute == 0x38);
    CHECK(app.pat.trk[4].engine == ENG_BITTAL && app.pat.trk[2].engine == ENG_ACID && app.pat.trk[5].engine == ENG_BUNO && app.pat.meta.bpm_x10 == 1600);
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

/* Lo que cuesta dibujar cada pantalla (el cuadro entero, las 30 franjas), en la PC. El FM-1 es
 * unas 20-40 veces más lento: lo que importa es la proporción entre pantallas y el antes/después. */
static void null_blit(void *u, int x, int y, int w, int h, const uint16_t *px) { (void)u; (void)x; (void)y; (void)w; (void)h; (void)px; }

/* Las funciones caras de libm (en el FM-1 son de software): cuántas llama un cuadro. En la PC son
 * casi gratis, así que el tiempo de la PC no las muestra; la cuenta sí. Cuentan solo si math_on. */
static int math_on;
static long math_n;
#define MATH_WRAP(f)                                                                     \
    float f(float x) {                                                                   \
        static float (*real)(float);                                                     \
        if (!real) *(void **)&real = dlsym(RTLD_NEXT, #f);                               \
        math_n += math_on;                                                               \
        return real(x);                                                                  \
    }
MATH_WRAP(expf)
MATH_WRAP(exp2f)
MATH_WRAP(log10f)
MATH_WRAP(log2f)
MATH_WRAP(logf)
MATH_WRAP(sinf)
MATH_WRAP(cosf)
MATH_WRAP(tanhf)

static double draw_us(long *math)
{
    enum { N = 200 };
    app.ov.on = 0; app.toast_until = 0;
    bm_screen_render(&app.scr, bm_app_draw, &app, null_blit, NULL);     /* el primero llena los cachés */
    math_n = 0; math_on = 1;
    app.scr.force = 1; bm_screen_render(&app.scr, bm_app_draw, &app, null_blit, NULL);
    math_on = 0;
    if (math) *math = math_n;
    clock_t c0 = clock();
    for (int i = 0; i < N; i++) { app.scr.force = 1; bm_screen_render(&app.scr, bm_app_draw, &app, null_blit, NULL); }
    return (double)(clock() - c0) * 1e6 / CLOCKS_PER_SEC / N;
}

static void draw_one(const char *name)
{
    long m;
    double us = draw_us(&m);
    printf("  %-7s %5.0f us %5ld exp/log/sin\n", name, us, m);
}

static void draw_pages(int t, int pg, const char *name)
{
    int n = 0;
    ch(t);
    page(BTN_EDIT, pg);
    run_ms(80);
    bm_app_ed_pages(&app, t, &n);
    long m[16];
    printf("  %-7s", name);
    for (int p = 0; p < n && p < 16; p++) { app.ed_page[t] = (uint8_t)p; printf(" %5.0f", draw_us(&m[p])); }
    printf("  us/cuadro por pagina de SOUND\n  %-7s", "");
    for (int p = 0; p < n && p < 16; p++) printf(" %5ld", m[p]);
    printf("  exp/log/sin por cuadro\n");
}

static void test_draw_cost(void)
{
    printf("pantalla: costo de dibujar un cuadro entero (PC)\n");
    boot(1);
    uint32_t budget = app.scr.budget;
    app.scr.budget = 0;                                          /* aquí se mide el cuadro entero */
    btn(BTN_PLAY, 20);
    run_ms(300);
    draw_one("HOME");
    ch(0); page(BTN_SEQ, PG_SEQ);
    draw_one("SEQ TR");
    ch(2); page(BTN_SEQ, PG_SEQ);
    draw_one("SEQ T3");
    ch(1); page(BTN_EDIT, PG_PADS);
    draw_one("PADS");
    draw_pages(0, PG_INST, "BR-8");
    draw_pages(2, PG_SOUND, "BB-303");
    draw_pages(3, PG_SOUND, "BX-7");
    draw_pages(4, PG_SOUND, "BITTAL");
    draw_pages(5, PG_SOUND, "BUNO-6");
    ch(3);                                                       /* 0.9: PIANO ROLL, SLOOP, la tarjeta y el GLO de la consola */
    app.set.note_view |= 1u << 3;
    page(BTN_SEQ, PG_SEQ);
    draw_one("ROLL");
    app.set.note_view &= (uint8_t)~(1u << 3);
    btn(BTN_HOME, 30);
    btn(BTN_REC, 30);
    draw_one("SLOOP");
    btn(BTN_REC, 30);
    kdown(bm_white_key(BM_PATTERNS + 3)); run_ms(400);
    draw_one("CARD");
    enc(ENC_K1, 1); kup(bm_white_key(BM_PATTERNS + 3)); run_ms(40);   /* girar: soltar no entra */
    btn(BTN_GLO, 30); btn(BTN_GLO, 30);
    draw_one("GLO");
    btn(BTN_HOME, 30);
    CHECK(app_set_machine(&app, 2, ENG_MINI) == 0);
    run_ms(40);
    draw_pages(2, PG_SOUND, "B-MINI");
    /* el caché sigue a los parámetros: otro corte cambia la curva, volver deja el mismo dibujo */
    app.ed_page[2] = 5;                                          /* B-MINI: FILTER */
    app.ov.on = 0; app.toast_until = 0;
    app.scr.force = 1; bm_screen_render(&app.scr, bm_app_draw, &app, app.pf.blit, app.pf.user);
    static uint16_t ref[240 * 240];
    memcpy(ref, sim.fb, sizeof ref);
    uint8_t cut = app.pat.trk[2].macro[0];
    app.pat.trk[2].macro[0] = (uint8_t)(cut > 60 ? cut - 40 : cut + 40);
    app.scr.force = 1; bm_screen_render(&app.scr, bm_app_draw, &app, app.pf.blit, app.pf.user);
    CHECK(memcmp(ref + 57 * 240, sim.fb + 57 * 240, 78 * 240 * 2) != 0);   /* la curva, no solo la perilla */
    app.pat.trk[2].macro[0] = cut;
    app.scr.force = 1; bm_screen_render(&app.scr, bm_app_draw, &app, app.pf.blit, app.pf.user);
    CHECK(memcmp(ref, sim.fb, sizeof ref) == 0);
    app.scr.budget = budget;
    btn(BTN_PLAY, 20);
    run_ms(2000);
    /* entrar a una página: el redibujo entero se reparte en varios ticks y termina igual que de una vez */
    app.ov.on = 0; app.toast_until = 0;
    memset(sim.fb, 0, sizeof sim.fb);
    app.scr.force = 1; sim.spi_bytes = 0;
    bm_app_tick(&app, now_ms);
    CHECK(sim.spi_bytes <= BM_SCR_BUDGET && app.scr.pending);
    int n = 1;
    while (app.scr.pending && n < 10) { bm_app_tick(&app, now_ms); n++; }
    printf("  redibujo entero: %d ticks de hasta %u B\n", n, (unsigned)BM_SCR_BUDGET);
    CHECK(!app.scr.pending && n == 3);
    memcpy(ref, sim.fb, sizeof ref);
    app.scr.budget = 0; app.scr.force = 1;
    bm_app_tick(&app, now_ms);
    CHECK(memcmp(ref, sim.fb, sizeof ref) == 0);
    app.scr.budget = budget;
    /* 25 cuadros por segundo: un tick cada 10 ms dibuja uno de cada cuatro */
    uint32_t f0 = app.scr.frames;
    for (int i = 0; i < 100; i++) { now_ms += 10; bm_app_tick(&app, now_ms); }
    CHECK(app.scr.frames - f0 == 25);
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
    /* 0.9: el PIANO ROLL (el loop entero) y SLOOP (la consola tocando una máquina), sonando */
    ch(3);
    app.set.note_view |= 1u << 3;
    page(BTN_SEQ, PG_SEQ);
    run_ms(300);
    sum = 0; maxb = 0;
    for (int i = 0; i < 60; i++) { sim.spi_bytes = 0; run_ms(33); sum += sim.spi_bytes; if (sim.spi_bytes > maxb) maxb = sim.spi_bytes; }
    printf("  PIANO ROLL: promedio %u B/frame, máximo %u B\n", sum / 60, maxb);
    CHECK(sum / 60 < 37000);
    app.set.note_view &= (uint8_t)~(1u << 3);
    btn(BTN_HOME, 30);
    btn(BTN_REC, 30);
    run_ms(300);
    sum = 0; maxb = 0;
    for (int i = 0; i < 60; i++) { sim.spi_bytes = 0; run_ms(33); sum += sim.spi_bytes; if (sim.spi_bytes > maxb) maxb = sim.spi_bytes; }
    printf("  SLOOP: promedio %u B/frame, máximo %u B\n", sum / 60, maxb);
    CHECK(sum / 60 < 37000);
    btn(BTN_REC, 30);
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
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    black(7, 40);                                                   /* negra 8 = escena B: entra en el próximo compás */
    CHECK(app.ctx.st.scene_q == 1);
    shot("23-scenes");
    run_ms(3000);
    CHECK(app.ctx.st.scene == 1 && app.pat.trk[3].mute && app.pat.trk[5].mute && !app.pat.trk[0].mute);
    black(6, 40);                                                   /* A: todo suena otra vez */
    run_ms(3000);
    CHECK(app.ctx.st.scene == 0 && !app.pat.trk[3].mute);
    down(BTN_HOME); run_ms(2200); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SYSTEM);
    shot("25-system");
    btn(BTN_SAVE, 30);
    shot("27-save");
    home_with(BTN_SEQ);
    CHECK(app.view == V_GLO && app.glo_page == GP_SONG);
    white(0, 30); white(1, 30);
    black(7, 30);                                                   /* la entrada 2 lanza la escena B */
    CHECK(app.song.n == 2 && bm_song_pat(app.song.pat[1]) == 1 && bm_song_scene(app.song.pat[1]) == 1);
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
    enc(ENC_ALGO, -1);                                           /* a la izquierda: inglés */
    CHECK(app.set.lang == 0 && strcmp(T("TAKES"), "TAKES") == 0);
    enc(ENC_ALGO, 1);                                            /* a la derecha: el paquete */
    CHECK(app.set.lang == 1 && strcmp(T("TAKES"), "TOMAS") == 0);
    enc(ENC_ALGO, 1);                                            /* otra vez a la derecha: sigue igual */
    CHECK(app.set.lang == 1);
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

/* Perillas y navegación: paso sostenido + PRESETS, aceleración, interruptores, HOME que vuelve */
static void test_knobs_nav(void)
{
    printf("perillas: paso + PRESETS, aceleración, interruptores, HOME siempre a MIX\n");
    boot(1);
    const char *s, *al, *pr;
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(s, "TRACK") == 0 && strcmp(al, "BPM") == 0 && strcmp(pr, "KIT") == 0);   /* el cursor en T1 */
    /* aceleración: dos detents rápidos en una lista = dos pasos cada vez (ni uno, ni ocho) */
    CHECK(app.track == 0);
    bm_app_encoder(&app, ENC_SELECT, 2, now_ms); run_ms(8);
    bm_app_encoder(&app, ENC_SELECT, 2, now_ms); run_ms(40);
    CHECK(app.track == 4);
    for (int i = 0; i < 6 && app.track; i++) enc(ENC_SELECT, -1);
    run_ms(1000);
    int lv = app.pat.trk[0].level;                               /* en un valor continuo, rápido = más */
    bm_app_encoder(&app, ENC_K1, -2, now_ms); run_ms(8);
    bm_app_encoder(&app, ENC_K1, -2, now_ms); run_ms(40);
    CHECK(app.pat.trk[0].level < lv - 4);
    /* paso sostenido + PRESETS en la batería: la condición, nunca el kit */
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(s, "PAGE") == 0 && strcmp(al, "INSTR") == 0 && strcmp(pr, "KIT") == 0);
    app.sel_lane[0] = 0; app.step_page = 0;
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);
    static bm_lane lanes[BM_LANES];
    memcpy(lanes, app.pat.tr[0].lane, sizeof lanes);
    uint8_t src = app.pat.trk[0].source;
    kdown(bm_white_key(0)); run_ms(400);
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(pr, "COND") == 0);
    enc(ENC_PRESETS, 1);
    CHECK(memcmp(lanes, app.pat.tr[0].lane, sizeof lanes) == 0 && app.pat.trk[0].source == src);
    CHECK(((app.pat.tr[0].st[0][0] & ST_COND_MASK) >> ST_COND_SH) == 1);
    kup(bm_white_key(0)); run_ms(60);
    CHECK(app.pat.tr[0].st[0][0] & ST_ON);
    enc(ENC_PRESETS, -1);                                        /* sin paso: el kit, y HOME + OCT- lo deshace */
    run_ms(1000);
    memcpy(lanes, app.pat.tr[0].lane, sizeof lanes);
    enc(ENC_PRESETS, 1); enc(ENC_PRESETS, 1);
    CHECK(memcmp(lanes, app.pat.tr[0].lane, sizeof lanes) != 0);
    home_with(BTN_OCT_DN);
    CHECK(memcmp(lanes, app.pat.tr[0].lane, sizeof lanes) == 0);
    /* el kit también desde HOME (PRESETS = el sonido del cursor), y también se deshace */
    btn(BTN_HOME, 30);
    run_ms(1000);
    enc(ENC_PRESETS, -1);                                        /* (el deshacer devolvió los instrumentos, no el número de kit) */
    CHECK(memcmp(lanes, app.pat.tr[0].lane, sizeof lanes) != 0 && app.view == V_HOME);
    home_with(BTN_OCT_DN);
    CHECK(memcmp(lanes, app.pat.tr[0].lane, sizeof lanes) == 0);
    /* MUTE de instrumentos (negra 11 sostenida): las perillas siguen con lo de SEQ */
    ch(0);
    page(BTN_SEQ, PG_SEQ);
    kdown(bm_black_key(10)); run_ms(400);
    CHECK(app.layer == L_LANEMUTE);
    int m0 = app.pat.trk[0].macro[0];
    enc(ENC_K1, 1);
    CHECK(app.pat.trk[0].macro[0] == m0 + 1);
    kup(bm_black_key(10)); run_ms(60);
    /* FILL (notas): HOME sostenido la cierra y la apaga */
    ch(2);
    page(BTN_SEQ, PG_SEQ);
    black(10, 40);
    CHECK(app.layer == L_FILL && app.fill);
    down(BTN_HOME); run_ms(400);
    CHECK(app.layer == L_HOME && !app.fill);
    up(BTN_HOME); run_ms(60);
    CHECK(!app.fill && !app.ctx.fill && app.view == V_CH);
    /* HOME siempre vuelve a MIX (0.9): desde la máquina, su GLO, TIME y el FX de la consola */
    btn(BTN_EDIT, 30);
    CHECK(app.view == V_CH && app.page == PG_SOUND);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_CH && app.page == PG_MORE);
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(s, "ROW") == 0 && strcmp(al, "VALUE") == 0);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_CH && app.track == 2 && app.page == PG_SOUND);
    home_with(BTN_FX);
    CHECK(app.view == V_FXG);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    ch(2);
    down(BTN_SEQ); run_ms(2200); up(BTN_SEQ); run_ms(60);
    CHECK(app.view == V_TIME);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_HOME, 30);                                           /* en la consola, HOME se queda en MIX */
    CHECK(app.view == V_HOME);
    btn(BTN_FX, 30);
    CHECK(app.view == V_FXG && app.fx_page == FXP_CHANNEL);
    enc(ENC_SELECT, 1);                                          /* SELECT: CHANNEL > DELAY */
    CHECK(app.fx_page == FXP_DELAY);
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(s, "PAGE") == 0 && strcmp(pr, "MOVE") == 0);
    btn(BTN_GLO, 30);                                            /* GLO en la consola: su GLO */
    CHECK(app.view == V_GLO && app.glo_page == GP_TEMPO);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    /* interruptores: a la derecha dos veces sigue encendido */
    down(BTN_HOME); run_ms(2200); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SYSTEM && app.menu_row == 0);
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.set.clock == 1);
    enc(ENC_ALGO, -1); enc(ENC_ALGO, -1);
    CHECK(app.set.clock == 0);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    /* PRESETS con HOME sostenido: el patrón se elige al soltar, no uno por uno */
    int p0 = app.ctx.st.pattern;
    down(BTN_HOME); run_ms(400);
    bm_app_bigknobs(&app, &s, &al, &pr);
    CHECK(strcmp(pr, "PATTERN") == 0 && strcmp(al, "BPM .1") == 0);
    enc(ENC_PRESETS, 1); run_ms(600); enc(ENC_PRESETS, 1); run_ms(600);
    CHECK(app.ctx.st.pattern == p0 && app.pat_sel == p0 + 2 && app.njob == 0);
    up(BTN_HOME); run_ms(60);
    CHECK(app.pat_sel == -1);
}

/* ------------------------------------------------------------ 0.9: la consola */
/* cuántos píxeles de un color hay en un rectángulo de la última captura */
static int count_px(int x0, int y0, int x1, int y1, uint16_t c)
{
    int n = 0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) n += sim.fb[y * 240 + x] == c;
    return n;
}

static int notes_in(int t)
{
    int n = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) n += !bm_event_empty(bm_evc(&app.pat, t, s, k));
    return n;
}

static int hits_in(int t)
{
    int n = 0;
    for (int l = 0; l < BM_LANES; l++) for (int s = 0; s < BM_STEPS; s++) n += (bm_trk_trc(&app.pat, t)->st[l][s] & ST_ON) != 0;
    return n;
}

static void test_console_patterns(void)
{
    printf("consola 0.9: W1-W10 patrones (cola, cadena), W11-W16 canales; los objetos 11-16 de la flash se ignoran\n");
    /* la flash: un patrón viejo en el objeto 12 ya no cuenta y su lugar se reusa */
    bm_sim_flash_erase_all(&sim);
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    static bm_store st, st2;
    bm_store_mount(&st, pf.flash);
    uint8_t body[64];
    memset(body, 7, sizeof body);
    CHECK(bm_store_write(&st, 12, body, sizeof body) == 0 && bm_store_has(&st, 12));
    bm_store_mount(&st2, pf.flash);
    CHECK(!bm_store_has(&st2, 12) && bm_store_used_slots(&st2) == 0);
    boot(1);
    CHECK(BM_PATTERNS == 10 && app.view == V_HOME);
    const bm_keyfn *K = keys();
    CHECK(!strcmp(K[bm_white_key(9)].label, "10") && !strcmp(K[bm_white_key(10)].label, "T1") && !strcmp(K[bm_white_key(15)].label, "T6"));
    CHECK(!strcmp(K[bm_black_key(0)].label, "M1") && !strcmp(K[bm_black_key(6)].label, "A") && !strcmp(K[bm_black_key(10)].label, "CUE"));
    CHECK(K[bm_white_key(0)].led == LED_ON && K[bm_white_key(1)].led == LED_DIM);          /* el actual ON, vacío DIM */
    CHECK(K[bm_white_key(10)].led == LED_ON && K[bm_white_key(12)].led == LED_MID);        /* el cursor ON, con pasos MID */
    CHECK(K[bm_white_key(11)].led == LED_DIM);                                             /* BP-1 sin tomas: vacío */
    CHECK(K[bm_black_key(0)].led == LED_MID && K[bm_black_key(10)].led == LED_DIM);
    /* parado: cambia ya */
    white(2, 40); run_ms(300);
    CHECK(app.ctx.st.pattern == 2 && keys()[bm_white_key(2)].led == LED_ON && keys()[bm_white_key(0)].led == LED_MID);
    white(0, 40); run_ms(300);
    CHECK(app.ctx.st.pattern == 0);
    /* sonando: en cola para el ciclo maestro (4 compases en la demo) */
    btn(BTN_PLAY, 20); run_ms(200);
    white(4, 40);
    CHECK(app.ctx.st.queued == 4 && app.ctx.st.pattern == 0 && keys()[bm_white_key(4)].led == LED_BLINK);
    run_ms(6200);
    CHECK(app.ctx.st.pattern == 4);
    /* una sostenida + otra = cadena: el siguiente de la cadena parpadea */
    kdown(bm_white_key(0)); run_ms(40); white(1, 40); kup(bm_white_key(0)); run_ms(40);
    CHECK(app.ctx.st.pchain_n == 2);
    run_ms(6200);
    CHECK(app.ctx.st.pattern == 0 && keys()[bm_white_key(1)].led == LED_BLINK);
    run_ms(6000);
    CHECK(app.ctx.st.pattern == 1);
    btn(BTN_PLAY, 20);
    /* las blancas 11-16 no son patrones: mueven el cursor al bajar */
    int p = app.ctx.st.pattern;
    kdown(bm_white_key(12)); run_ms(20);
    CHECK(app.track == 2 && app.ctx.st.pattern == p);
    kup(bm_white_key(12)); run_ms(40);
    CHECK(app.view == V_HOME && !app.card);                      /* un toque: solo el cursor */
}

static void test_scenes(void)
{
    printf("escenas 0.9: solo mutes; toque guarda (vacía) o lanza en el compás; 2 s guarda encima; EDIT + escena; cadena; CUE; mutes y cambio de patrón\n");
    boot(1);
    memset(app.song.scene, 0, sizeof app.song.scene);            /* sin las de la demo */
    CHECK(keys()[bm_black_key(6)].led == LED_DIM);               /* vacía: DIM */
    black(0, 40);                                                /* parado: mute T1 */
    CHECK(app.pat.trk[0].mute);
    black(6, 40);                                                /* A vacía: guarda los mutes */
    CHECK((app.song.scene[0].flags & SCN_STORED) && app.song.scene[0].trk_mute == 0x01 && !strcmp(app.toast, "SCENE A STORED"));
    CHECK(keys()[bm_black_key(6)].led == LED_MID && app.song_dirty);
    black(0, 40);
    CHECK(!app.pat.trk[0].mute);
    black(6, 40);                                                /* A guardada: la lanza (parado: ya) */
    CHECK(app.pat.trk[0].mute && app.ctx.st.scene == 0 && keys()[bm_black_key(6)].led == LED_ON);
    black(0, 40); black(3, 40);                                  /* B: T4 muteado */
    black(7, 40);
    CHECK(app.song.scene[1].trk_mute == 0x08);
    /* 2 s sostenida: guarda encima, con el anillo */
    black(3, 40); black(4, 40);                                  /* ahora T5 */
    kdown(bm_black_key(7)); run_ms(1200);
    {
        char lb[24];
        CHECK(bm_app_key_ring(&app, lb, sizeof lb) > 100 && !strcmp(lb, "STORE SCENE B"));
    }
    shot("69-scene-store-ring");
    run_ms(1000);
    CHECK(app.song.scene[1].trk_mute == 0x10);
    kup(bm_black_key(7)); run_ms(60);
    CHECK(app.pat.trk[4].mute && app.ctx.st.scene == 0);         /* soltarla no la lanza */
    black(4, 40);
    for (int t = 0; t < BM_TRACKS; t++) CHECK(!app.pat.trk[t].mute);
    /* los mutes de la consola sobreviven al cambio de patrón: A02 guardado con T1 muteado y T5 no */
    static bm_pattern q;
    static uint8_t pk[BM_PENDING_LEN];
    q = app.pat; q.trk[0].mute = 1; q.trk[4].mute = 0; q.tr[0].mute = 0;
    int n = bm_pattern_pack(&q, pk, sizeof pk);
    CHECK(n > 0 && bm_store_write(&app.store, 1, pk, (uint16_t)n) == 0);
    /* sonando: la escena entra en el próximo compás */
    btn(BTN_PLAY, 20); run_ms(200);
    black(7, 40);                                                /* B (T5) */
    CHECK(app.ctx.st.scene_q == 1 && keys()[bm_black_key(7)].led == LED_BLINK && !app.pat.trk[4].mute);
    run_ms(1600);
    CHECK(app.ctx.st.scene == 1 && app.ctx.st.scene_q == 0xFF && app.pat.trk[4].mute);
    app_push_i(&app, CMD_LANE_MUTE, 0, 0, 0, 0x0004); run_ms(40); /* y el hat de BR-8 (en vivo) */
    white(1, 40);                                                /* A02 en cola */
    run_ms(6200);
    CHECK(app.ctx.st.pattern == 1 && app.pat.trk[4].mute && !app.pat.trk[0].mute && app.pat.tr[0].mute == 0x0004);
    /* EDIT sostenido + escena = vaciarla */
    down(BTN_EDIT); run_ms(400);
    CHECK(app.layer == L_ERASE && !strcmp(keys()[bm_black_key(7)].label, "XB"));
    black(7, 40);
    up(BTN_EDIT); run_ms(60);
    CHECK(!(app.song.scene[1].flags & SCN_STORED));
    /* una cadena: A sostenida + C, D (C y D guardadas antes) */
    black(2, 40); black(8, 40); black(2, 40);                    /* C: T3 */
    black(5, 40); black(9, 40); black(5, 40);                    /* D: T6 */
    CHECK(app.song.scene[2].trk_mute == 0x14 && app.song.scene[3].trk_mute == 0x30);
    kdown(bm_black_key(6)); run_ms(40); black(8, 40); black(9, 40); kup(bm_black_key(6)); run_ms(40);
    CHECK(app.ctx.st.chain_n == 3 && app.chain_n == 3 && app.chain[0] == 0 && app.chain[1] == 2 && app.chain[2] == 3);
    run_ms(1600);
    CHECK(app.ctx.st.scene == 0);                                /* la primera, en el próximo compás */
    run_ms(6200);
    CHECK(app.ctx.st.scene == 2 && app.pat.trk[2].mute);         /* la siguiente, con el ciclo maestro */
    /* CUE: con la negra 11 sostenida, los mutes caen juntos en el próximo compás */
    black(6, 40); run_ms(1600);                                  /* A sola: sin cadena */
    CHECK(app.ctx.st.chain_n == 0 && app.ctx.st.scene == 0);
    uint8_t m0 = app.pat.trk[1].mute, m1 = app.pat.trk[3].mute;
    while (app.ctx.st.btick % BM_BAR_TICKS > 24) run_ms(5);      /* justo después de un compás: el próximo está a 1.5 s */
    kdown(bm_black_key(10)); run_ms(350);                        /* sostenida (más que un toque) */
    CHECK(app.cue && keys()[bm_black_key(10)].led == LED_BREATH);
    black(1, 40); black(3, 40);
    CHECK(app.pat.trk[1].mute == m0 && app.pat.trk[3].mute == m1 && app.ctx.st.cue_x == 0x0A);
    CHECK(keys()[bm_black_key(1)].led == LED_BLINK);
    kup(bm_black_key(10)); run_ms(40);
    CHECK(!app.cue);                                             /* sostenida: no queda fija */
    run_ms(1600);
    CHECK(app.pat.trk[1].mute != m0 && app.pat.trk[3].mute != m1 && app.ctx.st.cue_x == 0);
    while (app.ctx.st.btick % BM_BAR_TICKS > 24) run_ms(5);
    black(10, 40);                                               /* un toque: CUE fija */
    CHECK(app.cue == 2);
    black(1, 40);
    CHECK(app.ctx.st.cue_x == 0x02);
    shot("70-cue");
    black(10, 40);                                               /* otro toque: suelta (lo que está en cola igual cae) */
    CHECK(app.cue == 0);
    run_ms(1600);
    CHECK(app.pat.trk[1].mute == m0 && app.ctx.st.cue_x == 0);
    btn(BTN_PLAY, 20);
    /* parado, CUE no espera */
    black(10, 40); black(1, 40);
    CHECK(app.pat.trk[1].mute != m0);
    black(10, 40);
    /* el objeto canción + escenas: versión nueva; uno viejo se ignora */
    app_job(&app, JOB_SAVE_SONG, 0); app_job(&app, JOB_SAVE_GLOB, 0); run_ms(300);
    boot(0);
    CHECK((app.song.scene[2].flags & SCN_STORED) && app.song.scene[2].trk_mute == 0x14);
    {
        uint8_t old[1 + 2 * BM_SONG_MAX + 32];
        memset(old, 0, sizeof old); old[0] = 3;                  /* 0.8: escenas con patrón, sin versión */
        bm_store_write(&app.store, BM_OBJ_SONG, old, sizeof old);
    }
    boot(0);
    CHECK(!(app.song.scene[2].flags & SCN_STORED) && app.song.n == 0);
}

/* SLOOP-REC (REC en MIX): las teclas tocan el canal del cursor */
static void test_sloop(void)
{
    printf("SLOOP-REC: REC en MIX arma el canal del cursor; notas cromáticas desde F, batería con ACC, REC expresivo\n");
    boot(1);
    enc(ENC_SELECT, 1); enc(ENC_SELECT, 1);                      /* T3 BB-303 */
    btn(BTN_REC, 30);
    CHECK(app.sloop && app.rec && app.view == V_HOME && sim.leds[BTN_REC] == LED_ON);
    const bm_keyfn *K = keys();
    CHECK(!strcmp(K[0].label, "F") && !strcmp(K[bm_white_key(15)].label, "G"));   /* 27 teclas desde F */
    kdown(0); run_ms(40);
    CHECK(app.key_nn[0] == 1 && app.key_notes[0][0] == BM_KEY_BASE_NOTE - 12);   /* máquina de bajo: una octava abajo */
    kup(0); run_ms(40);
    btn(BTN_OCT_UP, 20);
    CHECK(app.oct == 1 && sim.leds[BTN_OCT_UP] == LED_ON);
    kdown(0); run_ms(40);
    CHECK(app.key_notes[0][0] == BM_KEY_BASE_NOTE);
    kup(0); run_ms(40);
    btn(BTN_OCT_DN, 20);
    CHECK(app.oct == 0);
    /* KEYS IN SCALE: las blancas la escala, las negras acordes */
    app.pat.meta.scale_lock = 1;
    CHECK(!strcmp(keys()[bm_black_key(0)].label, "I"));
    kdown(bm_black_key(0)); run_ms(40);
    CHECK(app.key_nn[bm_black_key(0)] >= 3);
    kup(bm_black_key(0)); run_ms(40);
    app.pat.meta.scale_lock = 0;
    /* grabar: REC expresivo (lo grabado se ve en rojo) */
    int n0 = notes_in(2);
    btn(BTN_PLAY, 20); run_ms(300);
    hold_key(bm_white_key(4), 300); run_ms(200);
    hold_key(bm_white_key(6), 150); run_ms(300);
    CHECK(app.rec_steps != 0);
    shot("71-sloop-note");
    CHECK(count_px(15, 40, 240, 135, C_RED) > 0);                /* las notas de esta pasada, en rojo */
    btn(BTN_PLAY, 20); run_ms(60);
    CHECK(notes_in(2) >= n0);
    /* SELECT: otro canal, el teclado lo sigue (T1 batería: las blancas son los instrumentos, B1 = ACC) */
    enc(ENC_SELECT, -2);
    CHECK(app.track == 0 && app.sloop && app.rec_steps == 0);
    K = keys();
    CHECK(!strcmp(K[bm_white_key(0)].label, "BD") && !strcmp(K[bm_black_key(0)].label, "ACC") && K[bm_black_key(1)].label[0] == 0);
    int lane = -1;                                               /* un instrumento sin golpes */
    for (int l = 0; l < BM_LANES && lane < 0; l++) {
        int any = 0;
        for (int s = 0; s < BM_STEPS; s++) any |= app.pat.tr[0].st[l][s] & ST_ON;
        if (!any) lane = l;
    }
    CHECK(lane >= 0);
    btn(BTN_PLAY, 20); run_ms(250);
    kdown(bm_black_key(0)); run_ms(20);                          /* ACC sostenida */
    hold_key(bm_white_key(lane), 60);
    kup(bm_black_key(0)); run_ms(20);
    run_ms(300);
    hold_key(bm_white_key(lane), 60);                            /* sin ACC */
    run_ms(200);
    shot("72-sloop-drums");
    btn(BTN_PLAY, 20);
    int acc = 0, flat = 0;
    for (int s = 0; s < BM_STEPS; s++) {
        uint8_t v = app.pat.tr[0].st[lane][s];
        if (!(v & ST_ON)) continue;
        if (v & ST_ACC) acc++; else flat++;
    }
    CHECK(acc >= 1 && flat >= 1);
    /* HOME sostenido: las teclas vuelven a ser del mezclador */
    int hb = hits_in(0);
    down(BTN_HOME); run_ms(400);
    black(2, 40);                                                /* negra 3 = el mute de T3 */
    CHECK(app.layer == L_HOME && app.pat.trk[2].mute && !strcmp(keys()[bm_black_key(2)].label, "M3"));
    black(2, 40);
    up(BTN_HOME); run_ms(60);
    CHECK(!app.pat.trk[2].mute && !strcmp(keys()[bm_black_key(0)].label, "ACC"));   /* otra vez las del canal */
    white(0, 40);                                                /* parado: suena, no graba */
    CHECK(app.sloop && hits_in(0) == hb);
    /* REC 2 s: borra el canal del cursor (se deshace) */
    int h0 = hits_in(0);
    CHECK(h0 > 0);
    btn(BTN_REC, 2200); run_ms(60);
    CHECK(hits_in(0) == 0 && app.sloop);
    home_with(BTN_OCT_DN);
    CHECK(hits_in(0) == h0);
    /* REC otra vez: el mezclador */
    btn(BTN_REC, 30);
    CHECK(!app.sloop && !app.rec && !strcmp(keys()[bm_white_key(0)].label, "01"));
    /* REC sostenido en MIX: SLOOP solo mientras lo sostienes */
    down(BTN_REC); run_ms(400);
    CHECK(app.sloop && app.rec);
    up(BTN_REC); run_ms(60);
    CHECK(!app.sloop && !app.rec);
    /* un canal de notas vacío: FREE REC (el largo se cierra al salir de SLOOP) */
    app_clear_track(&app, 5); run_ms(40);
    enc(ENC_SELECT, 5);
    CHECK(app.track == 5);
    btn(BTN_REC, 30);
    CHECK(app.sloop && app.rec_free == 6 && app.pat.trk[5].length == 64);
    btn(BTN_PLAY, 20); run_ms(200);
    hold_key(bm_white_key(3), 500); run_ms(1400);
    btn(BTN_REC, 30); run_ms(60);
    CHECK(!app.sloop && !app.rec_free && app.pat.trk[5].length == 16 && notes_in(5) >= 1);
    btn(BTN_PLAY, 20);
    /* entrar a una máquina apaga SLOOP */
    btn(BTN_REC, 30);
    btn(BTN_SEQ, 30);
    CHECK(app.view == V_CH && !app.sloop && !app.rec);
}

static void test_oct_paging(void)
{
    printf("OCT -/+ = la octava en todas partes (batería y pads: nada); B9/B10 = páginas; LENGTH +16; ERASE UND/RED\n");
    boot(1);
    enc(ENC_SELECT, 2);                                          /* consola, cursor en T3 (notas): la octava de SLOOP */
    btn(BTN_OCT_UP, 20);
    CHECK(app.oct == 1 && sim.leds[BTN_OCT_UP] == LED_ON && sim.leds[BTN_OCT_DN] == LED_DIM);
    enc(ENC_SELECT, -2);                                         /* cursor en T1 (batería): nada */
    btn(BTN_OCT_UP, 20);
    CHECK(app.oct == 1 && sim.leds[BTN_OCT_UP] == LED_OFF && sim.leds[BTN_OCT_DN] == LED_OFF);
    /* dentro de una máquina de notas, también en SEQ: los grados de las negras siguen la octava */
    ch(2);
    page(BTN_SEQ, PG_SEQ);
    int root, sc = app_scale(&app, 2, &root);
    kdown(bm_black_key(0)); run_ms(40);
    CHECK(app.key_notes[bm_black_key(0)][0] == bm_scale_note(root, sc, 0, 2 + 1));
    kup(bm_black_key(0)); run_ms(40);
    btn(BTN_OCT_DN, 20); btn(BTN_OCT_DN, 20);
    CHECK(app.oct == -1 && app.step_page == 0);                  /* ya no pasa páginas */
    kdown(bm_black_key(0)); run_ms(40);
    CHECK(app.key_notes[bm_black_key(0)][0] == bm_scale_note(root, sc, 0, 2 - 1));
    kup(bm_black_key(0)); run_ms(40);
    btn(BTN_OCT_UP, 20);
    /* dos compases: B9 / B10 en ACCENT, TIE, ERASE y FILL */
    down(BTN_SEQ); run_ms(400); black(7, 40); up(BTN_SEQ); run_ms(60);   /* 2BR */
    CHECK(app.pat.trk[2].length == 32);
    down(BTN_ENV); run_ms(400);
    CHECK(app.layer == L_ACC && keys()[bm_black_key(9)].led == LED_MID && keys()[bm_black_key(8)].led == LED_OFF);
    black(9, 40);
    CHECK(app.step_page == 1);
    white(0, 40);                                                /* paso 17: un acento */
    up(BTN_ENV); run_ms(60);
    CHECK(!bm_event_empty(bm_evc(&app.pat, 2, 16, 0)) && bm_evc(&app.pat, 2, 16, 0)->vel == BM_VEL_ACC);
    down(BTN_LFO); run_ms(400);
    CHECK(app.layer == L_REP);
    black(8, 40);
    CHECK(app.step_page == 0);
    up(BTN_LFO); run_ms(60);
    down(BTN_EDIT); run_ms(400);
    black(9, 40);
    white(0, 40);                                                /* parado: borra el paso 17 */
    CHECK(bm_event_empty(bm_evc(&app.pat, 2, 16, 0)) && keys()[bm_black_key(5)].led == LED_MID);
    black(5, 40);                                                /* B6 = UNDO */
    CHECK(!bm_event_empty(bm_evc(&app.pat, 2, 16, 0)) && keys()[bm_black_key(6)].led == LED_MID);
    up(BTN_EDIT); run_ms(60);
    black(10, 40);                                               /* FILL abierta: también pasa páginas */
    CHECK(app.layer == L_FILL);
    black(8, 40);
    CHECK(app.step_page == 0 && app.layer == L_FILL);
    black(10, 40);
    /* LENGTH: B11 = +16 recorre 1-16 > 17-32 > 33-48 > 49-64; K1 cualquiera */
    down(BTN_SEQ); run_ms(400);
    CHECK(app.len_page == 1 && !strcmp(keys()[bm_black_key(10)].label, "+16"));
    black(10, 40);
    CHECK(app.len_page == 2);
    white(15, 40);
    CHECK(app.pat.trk[2].length == 48);
    black(10, 40); black(10, 40);
    CHECK(app.len_page == 0);
    enc(ENC_K1, 1);
    CHECK(app.pat.trk[2].length == 49);
    up(BTN_SEQ); run_ms(60);
    /* la batería: OCT apagado y sin efecto */
    ch(0);
    int o = app.oct;
    btn(BTN_OCT_UP, 20);
    CHECK(app.oct == o && sim.leds[BTN_OCT_UP] == LED_OFF);
}

static void test_asks(void)
{
    printf("preguntas 0.9: el control que preguntó, sostenido 2 s = sí; soltarlo antes u otro control = no\n");
    boot(1);
    ch(3);
    page(BTN_EDIT, PG_SOUND);
    down(BTN_EDIT); run_ms(2100);                                /* EDIT 2 s: INIT SOUND? */
    CHECK(app.ask == ASK_INIT && app.ask_live && app.ask_ctl == BTN_EDIT && sim.leds[BTN_EDIT] == LED_BLINK);
    up(BTN_EDIT); run_ms(60);                                    /* soltarlo antes = no */
    CHECK(app.ask == ASK_NONE && !strcmp(app.toast, "CANCELED") && app.layer == L_NONE);
    down(BTN_EDIT); run_ms(2100);
    CHECK(app.ask == ASK_INIT);
    run_ms(1000);
    shot("46a-ask-hold");
    run_ms(1100);                                                /* 2 s más sostenido = sí */
    CHECK(app.ask == ASK_NONE && !strcmp(app.toast, "INIT SOUND"));
    up(BTN_EDIT); run_ms(60);
    CHECK(app.layer == L_NONE && app.view == V_CH && app.page == PG_SOUND);
    down(BTN_EDIT); run_ms(2100);
    CHECK(app.ask == ASK_INIT);
    btn(BTN_PLAY, 20);                                           /* otro control = no (y no hace lo suyo) */
    CHECK(app.ask == ASK_NONE && !app.ctx.st.playing);
    up(BTN_EDIT); run_ms(60);
    /* una perilla preguntó (GLO de la máquina > INIT SOUND): 2 s sin tocar nada = sí; otra perilla = no */
    btn(BTN_GLO, 30);
    enc(ENC_SELECT, 10);
    const char *nm; char v[24];
    bm_app_more_row(&app, app.more_row, &nm, v, sizeof v);
    CHECK(!strcmp(nm, "INIT SOUND"));
    enc(ENC_ALGO, 1);
    CHECK(app.ask == ASK_INIT && app.ask_ctl == ASK_ENC);
    enc(ENC_K1, 1);
    CHECK(app.ask == ASK_NONE);
    enc(ENC_ALGO, 1); run_ms(800);
    enc(ENC_ALGO, 1);                                            /* seguir a la derecha: sigue la cuenta */
    CHECK(app.ask == ASK_INIT);
    shot("46c-ask-knob");
    run_ms(1400);
    CHECK(app.ask == ASK_NONE && !strcmp(app.toast, "INIT SOUND"));
}

static void test_console_glo(void)
{
    printf("GLO de la consola: TEMPO (con AUTO FILL), MACHINES, SCALE y SONG (entradas con escena, SONG REC)\n");
    boot(1);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO && app.glo_page == GP_TEMPO && sim.leds[BTN_HOME] == LED_ON && sim.leds[BTN_GLO] == LED_MID);
    static const char *const TR[5] = {"BPM", "SWING", "MASTER LENGTH", "TRANSPOSE", "AUTO FILL"};
    const char *nm; char v[20];
    CHECK(bm_app_glo_count(&app) == 5);
    for (int i = 0; i < 5; i++) { bm_app_glo_row(&app, i, &nm, v, sizeof v); CHECK(!strcmp(nm, TR[i])); }
    enc(ENC_SELECT, 4);
    enc(ENC_ALGO, -1);                                           /* AUTO FILL: de cada 4 (demo) a cada 2 */
    CHECK(app.pat.meta.fill_every == 2);
    enc(ENC_SELECT, -1); enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.pat.meta.transpose == 26);
    int bpm = app.pat.meta.bpm_x10;
    enc(ENC_K1, 1);
    CHECK(app.pat.meta.bpm_x10 == bpm + 10);
    run_ms(UI_OVERLAY_MS + 100);
    shot("24b-glo-tempo-fill");
    btn(BTN_GLO, 30);
    CHECK(app.glo_page == GP_MACH);
    shot("24c-glo-machines");
    btn(BTN_GLO, 30);
    CHECK(app.glo_page == GP_SCALE && sim.leds[BTN_SCL] == LED_MID);
    enc(ENC_SELECT, 2); enc(ENC_ALGO, 1);                        /* KEYS IN SCALE */
    CHECK(app.pat.meta.scale_lock == 1);
    enc(ENC_ALGO, -1);
    CHECK(app.pat.meta.scale_lock == 0);
    int sc = app.pat.meta.scale;
    enc(ENC_K2, 1);
    CHECK(app.pat.meta.scale == (sc + 1) % BM_SCALES);
    run_ms(UI_OVERLAY_MS + 100);
    shot("24d-glo-scale");
    btn(BTN_HOME, 30);
    btn(BTN_SCL, 30);                                            /* SCL en HOME: la misma página */
    CHECK(app.view == V_GLO && app.glo_page == GP_SCALE);
    btn(BTN_GLO, 30);
    CHECK(app.glo_page == GP_SONG && sim.leds[BTN_SEQ] == LED_MID);
    white(0, 40); white(2, 40);                                  /* dos entradas */
    CHECK(app.song.n == 2 && app.song_cur == 1);
    black(7, 40);                                                /* B8: la escena B en la entrada 2 */
    CHECK(bm_song_scene(app.song.pat[1]) == 1 && bm_song_pat(app.song.pat[1]) == 2);
    CHECK(keys()[bm_black_key(7)].led == LED_ON);
    black(7, 40);                                                /* otra vez: sin escena */
    CHECK(bm_song_scene(app.song.pat[1]) < 0);
    black(8, 40);
    CHECK(bm_song_scene(app.song.pat[1]) == 2);
    enc(ENC_K2, 1);                                              /* K2 = vueltas */
    CHECK(app.song.rep[1] == 2);
    enc(ENC_ALGO, 1);                                            /* ALGORITHM = el patrón de la entrada */
    CHECK(bm_song_pat(app.song.pat[1]) == 3 && bm_song_scene(app.song.pat[1]) == 2);
    shot("24e-glo-song");
    black(10, 40);                                               /* B11 borra la elegida */
    CHECK(app.song.n == 1);
    black(10, 40);
    CHECK(app.song.n == 0);
    /* SONG REC (HOME + REC): los cambios de patrón y los lanzamientos de escena */
    home_with(BTN_REC);
    CHECK(app.song_rec == 2);                                    /* parado: armado, empieza con PLAY */
    btn(BTN_HOME, 30);
    btn(BTN_PLAY, 20); run_ms(200);
    CHECK(app.song_rec == 1 && app.song.n == 1 && bm_song_pat(app.song.pat[0]) == 0 && bm_song_scene(app.song.pat[0]) < 0);
    black(7, 40);                                                /* escena B */
    white(1, 40);                                                /* y A02 en cola */
    run_ms(6200);
    CHECK(app.song.n == 2 && bm_song_pat(app.song.pat[1]) == 1 && bm_song_scene(app.song.pat[1]) == 1);
    run_ms(6000);
    CHECK(app.song.n == 2 && app.song.rep[1] == 2);              /* el mismo patrón sin escena nueva: una vuelta más */
    btn(BTN_PLAY, 20);
    home_with(BTN_REC);
    CHECK(!app.song_rec);
}

/* render de bloques sin la interfaz: para comparar dos mezclas muestra a muestra */
static void render(int32_t *dst, int frames)
{
    for (int o = 0; o < frames; o += 128) bm_app_audio(&app, dst + o * 2, frames - o < 128 ? (uint32_t)(frames - o) : 128u);
}

static void fx_solo(int t, int beatfx, int off, int drive)
{
    boot(1);
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != t);
    app.pat.trk[t].send = 0; app.pat.trk[t].rsend = 0; app.pat.meta.rv_mix = 0; app.pat.meta.dly_mix = 0;
    app.pat.meta.beatfx = (uint8_t)beatfx; app.pat.trk[t].bfx_off = (uint8_t)off; app.pat.trk[t].drive = (uint8_t)drive;
    note(t, 57, 1); note(t, 60, 1); note(t, 64, 1);
}

static void test_fx_channel(void)
{
    printf("HOME FX > CHANNEL: DRIVE (soft-clip del canal), PAN, DUCK y BEAT FX por canal (como un MX-1)\n");
    boot(1);
    enc(ENC_SELECT, 3);                                          /* T4 BX-7 */
    btn(BTN_FX, 30);
    CHECK(app.view == V_FXG && app.fx_page == FXP_CHANNEL);
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "DRIVE") && !strcmp(L[0].val, "OFF") && !strcmp(L[1].name, "PAN") && !strcmp(L[2].name, "DUCK")
          && !strcmp(L[3].name, "BEAT FX") && !strcmp(L[3].val, "ON"));
    run_ms(1000);
    enc(ENC_K1, 10);
    CHECK(app.pat.trk[3].drive == 10);
    enc(ENC_K2, 3);                                              /* PAN: el centro retiene 2 */
    CHECK(app.pat.trk[3].pan == 65);
    int dk = app.pat.trk[3].duck;
    enc(ENC_K3, 2);
    CHECK(app.pat.trk[3].duck == dk + 2);
    enc(ENC_K4, -1);                                             /* a la izquierda: BEAT FX OFF en este canal */
    CHECK(app.pat.trk[3].bfx_off == 1);
    enc(ENC_K4, -1);
    CHECK(app.pat.trk[3].bfx_off == 1);
    run_ms(UI_OVERLAY_MS + 100);
    shot("39a-home-fx-channel");
    enc(ENC_K4, 1);
    CHECK(app.pat.trk[3].bfx_off == 0);
    /* los formatos: DRIVE y BEAT FX viajan con el patrón */
    static uint8_t b[BM_PENDING_LEN];
    static bm_pattern q;
    app.pat.trk[3].bfx_off = 1;
    int n = bm_pattern_pack(&app.pat, b, sizeof b);
    CHECK(n > 0 && bm_pattern_unpack(&q, b, n) == n && q.trk[3].drive == 10 && q.trk[3].bfx_off == 1);
    /* el audio: BEAT FX OFF = el STUTTER no toca el canal (igual que sin efecto); ON sí lo toca */
    enum { N = 30000 };
    static int32_t ref[N * 2], off[N * 2], on[N * 2], drv[N * 2];
    fx_solo(3, 0, 0, 0); render(ref, N);
    fx_solo(3, 1, 1, 0); render(off, N);
    fx_solo(3, 1, 0, 0); render(on, N);
    fx_solo(3, 0, 0, 127); render(drv, N);
    int d_off = 0, d_on = 0, d_drv = 0;
    double pr = 0, pd = 0, er = 0, ed = 0;
    for (int i = 0; i < N * 2; i++) {
        d_off += ref[i] != off[i]; d_on += ref[i] != on[i]; d_drv += ref[i] != drv[i];
        if (fabs((double)ref[i]) > pr) pr = fabs((double)ref[i]);
        if (fabs((double)drv[i]) > pd) pd = fabs((double)drv[i]);
        er += (double)ref[i] * ref[i]; ed += (double)drv[i] * drv[i];
    }
    double cr = er > 0 ? pr / sqrt(er / (N * 2)) : 0, cd = ed > 0 ? pd / sqrt(ed / (N * 2)) : 0;   /* pico / RMS */
    printf("  STUTTER 4: muestras distintas con BEAT FX OFF %d, ON %d · DRIVE 127: pico/RMS %.2f -> %.2f\n", d_off, d_on, cr, cd);
    CHECK(d_off == 0 && d_on > N / 4 && d_drv > N / 4 && cd < cr && cd > 0);
}

/* PIANO ROLL (GLO de la máquina > VIEW) */
static double draw_us(long *math);

static void test_pianoroll(void)
{
    printf("PIANO ROLL: GLO > VIEW; el loop entero, alturas ajustadas, teclado que destella, REC en rojo; barato\n");
    boot(1);
    ch(3);                                                       /* BX-7: dos compases, acordes en 11 y 27 */
    btn(BTN_GLO, 30);
    CHECK(app.page == PG_MORE && app.more_row == 0);
    enc(ENC_ALGO, 1);                                            /* VIEW: PIANO ROLL */
    CHECK(bm_app_roll_view(&app, 3) && app.set_dirty);
    shot("45c-machine-glo-view");
    enc(ENC_ALGO, 1);                                            /* a la derecha otra vez: sigue */
    CHECK(bm_app_roll_view(&app, 3));
    btn(BTN_GLO, 30);
    CHECK(app.page == PG_SEQ);
    run_ms(UI_TOAST_MS + 100);
    shot("73-pianoroll");
    uint16_t col = bm_track_color(3);
    CHECK(count_px(15, 48, 240, 135, col) > 20);                 /* las notas */
    CHECK(count_px(15, 46, 240, 49, C_WHITE) > 20);              /* el corchete de las 16 blancas */
    CHECK(count_px(15, 60, 128, 120, C_DDGREY) > 1000);          /* y su fondo */
    CHECK(count_px(0, 48, 15, 135, C_GREY) > 50);                /* el teclado vertical */
    black(9, 40);                                                /* B10: el corchete pasa a 17-32 */
    CHECK(app.step_page == 1);
    white(10, 40);                                               /* la blanca 11 = el paso 27 (tiene acorde: se quita) */
    CHECK(bm_event_empty(bm_evc(&app.pat, 3, 26, 0)));
    white(10, 40);
    CHECK(!bm_event_empty(bm_evc(&app.pat, 3, 26, 0)));
    black(8, 40);
    /* sonando: el teclado destella con lo que suena */
    btn(BTN_PLAY, 20);
    int lit = 0;
    for (int i = 0; i < 40 && !lit; i++) { run_ms(25); shot("73a-pianoroll-play"); lit = count_px(0, 48, 15, 135, col) > 0; }
    CHECK(lit);
    /* REC: lo de esta pasada, en rojo */
    btn(BTN_REC, 30);
    hold_key(bm_black_key(0), 200); run_ms(300);
    CHECK(app.rec_steps != 0);
    shot("73b-pianoroll-rec");
    CHECK(count_px(15, 48, 240, 135, C_RED) > 0);
    btn(BTN_REC, 30); btn(BTN_PLAY, 20);
    /* barato: ninguna función cara de libm y el reparto guardado */
    long m = -1;
    double us = draw_us(&m);
    printf("  PIANO ROLL: %.0f us por cuadro en la PC, %ld exp/log/sin\n", us, m);
    CHECK(m == 0);
    /* la batería y los pads en SLOOP: una fila por instrumento con golpes */
    btn(BTN_HOME, 30);
    enc(ENC_SELECT, -3);
    btn(BTN_REC, 30);
    shot("72b-sloop-drum-roll");
    CHECK(count_px(15, 39, 240, 135, bm_track_color(0)) > 20);
    us = draw_us(&m);
    CHECK(m == 0);
    btn(BTN_REC, 30);
    /* VIEW es un ajuste: queda al reiniciar */
    run_ms(UI_AUTOSAVE_MS + 1500);
    boot(0);
    CHECK(bm_app_roll_view(&app, 3) && !bm_app_roll_view(&app, 2));
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) outdir = argv[1];
    bm_tables_init();
    test_pack();
    test_store();
    test_takes();
    test_report();
    test_gesture();
    test_timing();
    test_tr_features();
    test_roll();
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
    test_rec_expressive();
    test_sounds();
    test_printf();
    test_dual_core();
    test_dual_core_fail();
    test_live_kick_duck();
    test_chorus_long();
    test_duck_and_guard();
    test_brake();
    test_steal_at_end();
    test_engines();
    test_machines();
    test_key_screens();
    test_button_lights();
    test_knobs_nav();
    test_console_patterns();
    test_scenes();
    test_sloop();
    test_oct_paging();
    test_asks();
    test_console_glo();
    test_fx_channel();
    test_pianoroll();
    test_migrate();
    test_fm();
    test_demo_and_persist();
    test_old_version();
    test_draw_cost();
    test_spi_budget();
    test_ui_flow();
    test_lang();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
