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
#include "../core/bm_mach.h"
#include "../core/bm_util.h"
#include "../core/bm_rom.h"
#include "../core/bm_drums.h"
#include "../core/bm_wave.h"
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

/* El patrón de pruebas (la demo de primer arranque hasta 2026-10-24): acid techno a 160 BPM en La
 * menor. BR-8 con el kit 909 (y un shaker de 12 contra 16), una línea de BB-303 con acentos y slides,
 * el Slicer cortando la primera toma (si hay), acordes de B-MINI, un pluck de BITTAL y un pad de BUNO-6
 * de cuatro compases. Las pruebas lo cargan en cada arranque desde cero; los patrones de fábrica de
 * verdad se prueban en test_factory_and_persist. */
static void fx_ev(bm_pattern *p, int t, int s, int k, int note, int vel, int len, int prob)
{
    bm_event *e = bm_ev(p, t, s, k);
    bm_event_clear(e);
    e->note = (uint8_t)note; e->vel = (uint8_t)vel; e->len = (uint8_t)len; e->prob = (uint8_t)prob;
}

static void fx_tr(bm_tr *d, int lane, const char *pat)
{
    for (int s = 0; pat[s] && s < BM_STEPS; s++)
        if (pat[s] == 'x' || pat[s] == 'X') d->st[lane][s] = (uint8_t)(ST_ON | (pat[s] == 'X' ? ST_ACC : 0));
}

static void test_pattern(void)
{
    bm_pattern *p = &app.pat;
    bm_song *song = &app.song;
    bm_pattern_init(p);
    p->meta.bpm_x10 = 1600; p->meta.swing = 50; p->meta.root = 9; p->meta.scale = 1;
    p->meta.master = 4; p->meta.fill_every = 4; p->meta.chord_set = 0; p->meta.strum = 0;
    p->meta.dly_time = 2; p->meta.dly_fb = 45; p->meta.dly_mix = 40;
    bm_tr *d = &p->tr[0];
    bm_kit_apply(d, 1);
    p->trk[0].source = 1;
    fx_tr(d, 0, "x...x...x...x...");
    d->st[0][14] = ST_ON | (1 << ST_COND_SH);
    fx_tr(d, 2, ".x.x.x.x.x.x.x.x");
    fx_tr(d, 3, "..x...x...x.....");
    d->st[2][14] = ST_ON | ST_ACC | (1 << ST_RAT_SH);
    fx_tr(d, 5, "....x.......x...");
    fx_tr(d, 6, "...x.......x..x.");
    d->lane[6].level = 70;
    fx_tr(d, 12, "xxxxxxxxxxxx");
    d->lane[12].len = 12; d->lane[12].level = 45;
    d->st[15][15] = ST_ON | (1 << ST_COND_SH);
    d->lane[1].level = 0;
    p->trk[1].level = 70;
    for (int i = 0; i < BM_TAKES; i++) if (app.take[i].valid) { app_kit_from_take(&app, i, 0); break; }
    bm_track_set_engine(p, 2, ENG_ACID);
    bm_track *t3 = &p->trk[2];
    t3->macro[0] = 28; t3->macro[1] = 108; t3->macro[2] = 105; t3->macro[3] = 58; t3->glide = 45; t3->accent = 115;
    t3->length = 16; t3->send = 25;
    static const struct { int8_t s, n; uint8_t acc, slide; } L[] = {
        {0, 45, 1, 0}, {1, 45, 0, 0}, {2, 57, 1, 1}, {3, 45, 0, 0}, {4, 48, 0, 0}, {5, 45, 0, 1}, {6, 52, 1, 0},
        {8, 45, 0, 0}, {9, 57, 1, 0}, {10, 55, 0, 1}, {11, 52, 0, 0}, {12, 45, 1, 0}, {13, 48, 0, 1}, {14, 50, 0, 0}, {15, 45, 0, 0}};
    for (unsigned i = 0; i < sizeof L / sizeof L[0]; i++)
        fx_ev(p, 2, L[i].s, 0, L[i].n, L[i].acc ? BM_VEL_ACC : BM_VEL_NORMAL, 1 | (L[i].slide ? BM_LEN_TIE : 0), 100);
    bm_track_set_engine(p, 3, ENG_MINI);
    p->trk[3].source = 0; p->trk[3].length = 32; p->trk[3].send = 45; p->trk[3].rsend = 30;
    for (int k = 0; k < 3; k++) { static const int C[3] = {57, 60, 64}; fx_ev(p, 3, 10, k, C[k], 90, 1, 100); fx_ev(p, 3, 26, k, C[k] - 2 + (k == 1), 80, 1, 100); }
    p->trk[4].source = 13; bm_preset_apply(p, 4, 13);
    p->trk[4].length = 32; p->trk[4].send = 35; p->trk[4].rsend = 60;
    { static const int8_t S[6] = {2, 6, 14, 18, 22, 30}, N[6] = {69, 72, 76, 74, 72, 76};
      for (int i = 0; i < 6; i++) fx_ev(p, 4, S[i], 0, N[i], 85, 1, i % 3 == 2 ? 60 : 100); }
    p->trk[5].source = 2; bm_preset_apply(p, 5, 2);
    p->trk[5].length = 64; p->trk[5].send = 0; p->trk[5].rsend = 70;
    for (int k = 0; k < 3; k++) { static const int C[3] = {57, 60, 64}; fx_ev(p, 5, 0, k, C[k], 75, 64, 100); }
    p->trk[0].level = 84; p->trk[2].level = 66; p->trk[3].level = 32; p->trk[4].level = 36; p->trk[5].level = 26;
    p->meta.rv_mix = 70;
    memset(song->scene, 0, sizeof song->scene);
    for (int s = 0; s < BM_SCENES; s++) song->scene[0][s].flags = SCN_STORED;
    song->scene[0][0].trk_mute = 0x00;
    song->scene[0][1].trk_mute = 0x38;
    song->scene[0][2].trk_mute = 0x01;
    song->scene[0][3].trk_mute = 0x3A;
    bm_engines_sync(&app.ctx);
    bm_seq_set_bpm(&app.ctx, p->meta.bpm_x10);
}

/* erase: la flash desde cero; acid: el patrón de pruebas en vez de los de fábrica */
static void boot_with(int erase, int acid)
{
    if (erase) { bm_sim_flash_erase_all(&sim); sim.flash_writes = 0; }
    bm_platform pf;
    bm_sim_platform(&sim, &pf);
    now_ms = 1000; next_tick = now_ms; acc = 0;
    bm_app_init(&app, &pf, now_ms);
    if (acid) test_pattern();
    sim.flash_writes = 0;                                       /* lo que escribió el arranque (los de fábrica) no cuenta */
    for (int i = 0; i < 4; i++) bm_app_master(&app, 900);
    run_ms(UI_BOOT_MS + 100);
}

static void boot(int erase) { boot_with(erase, erase); }

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
    boot_with(0, 1);
}

static void put_fm_on_t4(void);

/* HOME (la consola, sin PLAY/REC) */
static void to_home(void)
{
    for (int i = 0; i < 6 && !(app.view == V_HOME && !app.sloop); i++) btn(BTN_HOME, 30);
}

/* HOME y SELECT hasta que el cursor del mezclador esté en el canal t */
static void ch(int t)
{
    to_home();
    for (int i = 0; i < BM_TRACKS && app.track != t; i++) enc(ENC_SELECT, 1);
}

/* el secuenciador de la batería (SEQ en HOME con el cursor en T1) */
static void seq0(void)
{
    ch(0);
    btn(BTN_SEQ, 30);
}

/* PLAY/REC del canal t: la blanca de su canal con el cursor ya en él */
static void play_rec(int t)
{
    ch(t);
    white(BM_CH_W0 + app_pos(t), 40);
}

/* las luces y los nombres de las teclas ahora */
static bm_keyfn keys_now[BM_KEYS];
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
    CHECK(p.trk[0].engine == ENG_DRUM && p.trk[1].engine == ENG_BREAK && p.trk[2].engine == ENG_ACID && p.trk[3].engine == ENG_MINI
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
    printf("gestos: toque, mantener (capas de SEQ, EDIT, ENV, LFO), 2 s (HOME, SAVE) y combo\n");
    bm_gesture g; glog L = {{0}, 0};
    bm_gesture_init(&g, gcb, &L);
    bm_gesture_down(&g, BTN_SCL, 0); bm_gesture_poll(&g, 100); bm_gesture_up(&g, BTN_SCL, 150);
    CHECK(L.n == 1 && L.log[0] == BTN_SCL * 10 + G_TAP);
    L.n = 0;
    bm_gesture_down(&g, BTN_GLO, 500); bm_gesture_poll(&g, 1500); bm_gesture_up(&g, BTN_GLO, 1600);
    CHECK(L.n == 1 && L.log[0] == BTN_GLO * 10 + G_TAP);                              /* sin capa: toque aunque dure */
    L.n = 0;
    bm_gesture_down(&g, BTN_ENV, 1000); bm_gesture_poll(&g, 1200); CHECK(L.n == 0);
    bm_gesture_poll(&g, 1300); CHECK(L.n == 1 && L.log[0] == BTN_ENV * 10 + G_HOLD);   /* HOLD al cumplirse */
    bm_gesture_poll(&g, 3100); CHECK(L.n == 1);                                        /* SEQ ya no tiene los 2 s */
    bm_gesture_up(&g, BTN_ENV, 3200); CHECK(L.n == 2 && L.log[1] == BTN_ENV * 10 + G_RELEASE);
    L.n = 0;
    bm_gesture_down(&g, BTN_HOME, 4000); bm_gesture_poll(&g, 4400); CHECK(L.n == 0);   /* HOME ya no tiene capa */
    int id = -1;
    CHECK(bm_gesture_progress(&g, 5000, &id) > 100 && id == BTN_HOME);                /* anillo hacia los 2 s */
    bm_gesture_poll(&g, 6001); CHECK(L.n == 1 && L.log[0] == BTN_HOME * 10 + G_LONG);
    bm_gesture_up(&g, BTN_HOME, 6100); CHECK(L.n == 1);
    L.n = 0;
    bm_gesture_down(&g, BTN_HOME, 7000); bm_gesture_other(&g, 7100);
    bm_gesture_poll(&g, 9500);
    CHECK(L.n == 0);                                                                   /* el combo cancela los 2 s */
    bm_gesture_up(&g, BTN_HOME, 9600); CHECK(L.n == 0);
    L.n = 0;
    bm_gesture_down(&g, BTN_PLAY, 9000);
    CHECK(L.n == 1 && L.log[0] == BTN_PLAY * 10 + G_PRESS);
    bm_gesture_up(&g, BTN_PLAY, 9010);
    L.n = 0;
    bm_gesture_down(&g, BTN_SAVE, 10000); bm_gesture_poll(&g, 10400); CHECK(L.n == 0);
    bm_gesture_poll(&g, 12001); CHECK(L.n == 1 && L.log[0] == BTN_SAVE * 10 + G_LONG);
    bm_gesture_up(&g, BTN_SAVE, 12100); CHECK(L.n == 1);
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
    printf("batería TR: repetición y fill\n");
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

static void test_free_rec(void)
{
    printf("PLAY/REC en un canal vacío: graba sobre su largo (no lo alarga, como SLOOP); al final da la vuelta\n");
    boot(1);
    app_push(&app, CMD_TRACK_CLEAR, 5, 0, 0);                                   /* vaciar T6 */
    app_set_track(&app, 5, TRK_OFF(length), 32);
    run_ms(60);
    play_rec(5);
    CHECK(app.sloop && app.track == 5 && !app.rec && app.view == V_HOME);
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    CHECK(app.rec && app.pat.trk[5].length == 32);
    uint32_t bar = 60000u * 4u / 160u;                           /* ms por compás a 160 BPM */
    key(bm_white_key(4), 600);                                  /* nota larga */
    run_ms(bar / 2);
    key(bm_white_key(7), 150);
    run_ms(bar + bar / 4);
    shot("51-seq-free-rec");
    btn(BTN_REC, 30);
    run_ms(100);
    CHECK(!app.rec && app.pat.trk[5].length == 32 && app.sloop);
    for (int s = 32; s < BM_STEPS; s++) for (int k = 0; k < BM_EVENTS; k++) CHECK(bm_event_empty(bm_evc(&app.pat, 5, s, k)));
    int n = 0, longest = 0;
    for (int s = 0; s < BM_STEPS; s++)
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e = bm_evc(&app.pat, 5, s, k);
            if (bm_event_empty(e)) continue;
            n++;
            if ((int)(e->len & BM_LEN_MASK) > longest) longest = (int)(e->len & BM_LEN_MASK);
        }
    CHECK(n >= 2 && longest >= 4);                              /* la nota larga dura lo que se sostuvo */
    btn(BTN_PLAY, 20);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && !app.sloop && !app.rec);
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

/* BX-7 a mano en T4 (con BITTAL y BUNO-6 de fábrica serían tres pesadas: el selector no lo deja) */
static void put_fm_on_t4(void)
{
    app.pat.trk[3].engine = ENG_FM;
    app.pat.trk[3].source = 0;
    bm_preset_apply(&app.pat, 3, 0);
    bm_engines_sync(&app.ctx);
    run_ms(40);
}

static void test_fm(void)
{
    printf("BX-7 (solo el motor): los 16 patches suenan sin saturar, las voces se liberan\n");
    boot(1);
    CHECK(bm_engine_ok(3, ENG_FM) && app.pat.trk[3].engine == ENG_MINI);
    put_fm_on_t4();
    static const uint8_t notes[3] = {48, 60, 67};
    wav_open("fm.wav");
    int ok = 0;
    for (int p = 0; p < BM_FM_PATCHES; p++) {
        app.pat.trk[3].source = (uint8_t)p;
        memcpy(app.pat.fmv, bm_fm_patch(p), BM_FM_PATCH); app.ctx.fmv_gen++;
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
    /* la voz del patrón es la base: el algoritmo y el LFO se editan en ella (CMD_FMV) */
    int a0 = bm_fm_alg_of(&app.pat);
    CHECK(a0 >= 1 && a0 <= 32);
    uint8_t fp[BM_FM_PATCH];
    bm_cmd q; memset(&q, 0, sizeof q);
    q.type = CMD_FMV; q.a = BM_FM_ALG; q.b = 0; bm_cmd_push(&app.cmdq, &q);   /* el algoritmo 1 */
    q.a = 137; q.b = 50; bm_cmd_push(&app.cmdq, &q);
    q.a = BM_FM_FB; q.b = 200; bm_cmd_push(&app.cmdq, &q);                    /* fuera de rango: queda en 7 */
    run_ms(20);
    bm_fm_patch_of(&app.pat, 3, NULL, fp);
    CHECK(fp[137] == 50 && fp[BM_FM_ALG] == 0 && app.pat.fmv[BM_FM_FB] == 7 && bm_fm_alg_of(&app.pat) == 1);
    uint8_t mods[6]; int8_t fb[2];
    bm_fm_alg_graph(0, mods, fb);                              /* algoritmo 1: 2>1 y 6>5>4>3, realimenta el 6 */
    CHECK(mods[5] == (1u << 4) && mods[3] == (1u << 2) && mods[2] == (1u << 1) && mods[1] == (1u << 0) && fb[0] == 0 && fb[1] == 0);
    bm_fm_alg_graph(31, mods, fb);                             /* algoritmo 32: seis portadoras */
    CHECK(bm_fm_carriers(31) == 0x3F && !mods[0] && !mods[5]);
    /* polifónico: las voces van por máquina, no por canal */
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != 3);
    bm_cmd c; memset(&c, 0, sizeof c);
    static const uint8_t ch3[3] = {48, 55, 64};
    for (int i = 0; i < 3; i++) { c.type = CMD_NOTE_ON; c.a = 3; c.b = ch3[i]; c.c = 100; bm_cmd_push(&app.cmdq, &c); }
    run_ms(60);
    int v3 = 0;
    for (int i = 0; i < BM_VOICES; i++) v3 += app.ctx.voice[i].active && app.ctx.voice[i].track == 3;
    CHECK(v3 == 3);
    c.type = CMD_ALL_OFF; bm_cmd_push(&app.cmdq, &c);
    run_ms(300);
}

/* PRESETS en HOME hasta el sonido i del canal t */
static void preset_to(int t, int i)
{
    ch(t);
    for (int guard = 0; app.pat.trk[t].source != i && guard < 64; guard++) { enc(ENC_PRESETS, 1); run_ms(20); }
}

static void test_engines(void)
{
    printf("máquinas: BB-303, B-MINI, BITTAL y BUNO-6 con el banco de fábrica (PRESETS en HOME), y la batería modelada\n");
    boot(1);
    static const uint8_t bass[4] = {36, 43, 48, 39};
    CHECK(app.pat.trk[2].engine == ENG_ACID);                    /* la demo: acid techno */
    ch(2);
    double p_acid = solo_peak(2, bass, 4, 100, 250);
    double p_acc = solo_peak(2, bass, 4, 127, 250);
    int acid_ok = 0;                                             /* los sonidos de fábrica del BB-303 */
    for (int i = 0; i < BM_AC_PRESETS; i++) {
        preset_to(2, (i + 1) % BM_AC_PRESETS);
        double pk = solo_peak(2, bass, 2, 100, 250);
        if (pk > 0.05 && pk < 1.0) acid_ok++; else printf("  BB-303 %s: pico %.2f\n", bm_sound_name(&app, 2), pk);
    }
    CHECK(acid_ok == BM_AC_PRESETS && app.pat.trk[2].source == 0 && strcmp(bm_sound_name(&app, 2), "CLASSIC") == 0);
    CHECK(app_set_machine(&app, 2, ENG_MINI) == 0);              /* T3: B-MINI */
    run_ms(40);
    CHECK(app.pat.trk[2].engine == ENG_MINI);
    double p_mini = solo_peak(2, bass, 4, 100, 250);
    printf("  pico BB-303 %.2f (acento %.2f) · B-MINI %.2f\n", p_acid, p_acc, p_mini);
    CHECK(p_acid > 0.05 && p_acid < 1.0 && p_acc >= p_acid);
    CHECK(p_mini > 0.05 && p_mini < 1.0);

    /* el banco de fábrica: cada sonido de cada máquina suena sin saturar */
    static const uint8_t chord[3] = {57, 60, 64};
    static const struct { int t, eng; const char *wav; } M[3] = {{2, ENG_MINI, "bmini.wav"}, {4, ENG_BITTAL, "bittal.wav"}, {5, ENG_BUNO, "buno6.wav"}};
    for (int m = 0; m < 3; m++) {
        int t = M[m].t, n = bm_snap_count(M[m].eng);
        CHECK(n >= 6);
        for (int i = 0; i < n; i++) CHECK(bm_snap_ok(bm_snap_get(M[m].eng, i), BM_SNAP_MAX));
        wav_open(M[m].wav);
        int ok = 0;
        for (int i = 0; i < n; i++) {
            preset_to(t, i);
            run_ms(80);                                          /* BITTAL arma sus tablas */
            double pk = t == 2 ? solo_peak(t, bass, 2, 100, 400) : solo_peak(t, chord, 3, 100, 600);
            printf("  %-7s %-12s pico %.2f\n", bm_engine_names[M[m].eng], bm_snap_name(M[m].eng, i), pk);
            if (pk > 0.015 && pk < 1.0) ok++;
        }
        run_ms(1500);
        wav_close();
        CHECK(ok == n);
    }
    /* BITTAL: las tablas en RAM, y otra tabla para el oscilador 1 las vuelve a armar */
    run_ms(80);
    CHECK(bm_wt_ready(0) && bm_wt_frames(0) >= 1);
    uint32_t sig = app.wt_sig[0];
    app_push(&app, CMD_SET_PRM, 4, BT_TAB1, 3);
    run_ms(80);
    CHECK(app.pat.prm[4][BT_TAB1] == 3 && app.wt_sig[0] != sig && bm_wt_ready(0));
    /* BUNO-6: el chorus */
    app_push(&app, CMD_SET_PRM, 5, BN_CHORUS, 1);
    run_ms(40);
    CHECK(app.pat.prm[5][BN_CHORUS] == 1);

    /* batería modelada: los 3 kits, los 16 instrumentos de cada uno suenan */
    ch(0);
    wav_open("bateria.wav");
    for (int k = 0; k < BM_KITS_N; k++) {
        preset_to(0, k);
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

    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    peak_abs = 0;
    wav_open("motores.wav");                                    /* la demo con los motores nuevos */
    btn(BTN_PLAY, 20); run_ms(8000); btn(BTN_PLAY, 20);
    wav_close();
    CHECK(peak_abs > 0.05 && peak_abs <= 1.0);
}

static void test_machines(void)
{
    printf("máquinas: las 6 de la etapa 1, las pesadas una vez, presets por máquina\n");
    /* el núcleo: qué máquina va en cada canal, las pesadas y el cambio */
    /* 2026-10-21: solo la batería y el Slicer son fijos; T3-T6 eligen entre las 5 (BITTAL y BX-7 también) */
    CHECK(BM_MACHINES == 5 && bm_machines[3] == ENG_BITTAL && bm_machines[4] == ENG_FM);
    for (int t = 2; t < BM_TRACKS; t++) for (int i = 0; i < BM_MACHINES; i++) CHECK(bm_engine_ok(t, bm_machines[i]));
    for (int t = 0; t < BM_TRACKS; t++) CHECK(app_fixed(t) == (t == 0 || t == 1) && BM_ORDER[app_pos(t)] == t);
    CHECK(bm_engine_ok(0, ENG_DRUM) && bm_engine_ok(1, ENG_BREAK) && !bm_engine_ok(0, ENG_FM) && !bm_engine_ok(1, ENG_ACID));
    CHECK(!bm_engine_ok(2, ENG_DRUM) && !bm_engine_ok(5, ENG_BREAK));
    CHECK(bm_engine_heavy(ENG_BUNO) && bm_engine_heavy(ENG_BITTAL) && bm_engine_heavy(ENG_FM) && !bm_engine_heavy(ENG_ACID) && !bm_engine_heavy(ENG_MINI));
    static bm_pattern p, q;
    static uint8_t b[BM_PENDING_LEN];
    bm_pattern_init(&p);
    static const char *const FACTORY[BM_TRACKS] = {"BR-8", "BP-1", "BB-303", "B-MINI", "BITTAL", "BUNO-6"};
    for (int t = 0; t < BM_TRACKS; t++) CHECK(strcmp(bm_engine_names[p.trk[t].engine], FACTORY[t]) == 0);
    CHECK(bm_machine_conflict(&p, 2, ENG_BUNO) == 5 && bm_machine_conflict(&p, 2, ENG_BITTAL) == 4);
    CHECK(bm_machine_conflict(&p, 2, ENG_MINI) < 0 && bm_machine_conflict(&p, 5, ENG_BUNO) < 0);
    bm_track_set_engine(&p, 2, ENG_BUNO);                               /* BUNO-6 ya está en T6: no */
    CHECK(p.trk[2].engine == ENG_ACID);
    CHECK(bm_heavy_count(&p, -1) == 2);
    bm_track_set_engine(&p, 2, ENG_FM);                                 /* una tercera pesada: no */
    CHECK(p.trk[2].engine == ENG_ACID);
    bm_track_set_engine(&p, 5, ENG_MINI);                               /* con una menos, BX-7 entra */
    bm_track_set_engine(&p, 2, ENG_FM);
    CHECK(p.trk[2].engine == ENG_FM && bm_heavy_count(&p, -1) == 2);
    bm_track_set_engine(&p, 2, ENG_ACID);
    bm_track_set_engine(&p, 5, ENG_BUNO);
    CHECK(p.trk[2].engine == ENG_ACID && p.trk[5].engine == ENG_BUNO);
    bm_event *e = bm_ev(&p, 3, 4, 0);
    e->note = 60; e->vel = 99; e->len = 3; e->lk[1] = 99;
    p.trk[3].length = 24; p.trk[3].level = 77;
    bm_track_set_engine(&p, 3, ENG_ACID);                               /* las notas quedan, sin P-Locks */
    CHECK(p.trk[3].engine == ENG_ACID && e->note == 60 && e->vel == 99 && e->len == 3 && e->lk[1] == BM_LOCK_NONE);
    CHECK(p.trk[3].length == 24 && p.trk[3].level == 77 && p.trk[3].source == 0);
    /* al leer un patrón, una pesada repetida vuelve a la máquina de fábrica de su canal */
    p.trk[3].engine = ENG_BUNO;
    int n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(n > 0 && bm_pattern_unpack(&q, b, n) == n);
    CHECK(q.trk[5].engine == ENG_BUNO && q.trk[3].engine == ENG_MINI && bm_evc(&q, 3, 4, 0)->note == 60);
    p.trk[5].engine = ENG_ACID; p.trk[2].engine = ENG_BUNO; p.trk[3].engine = ENG_BUNO;   /* ninguna en la suya: queda la primera */
    n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(bm_pattern_unpack(&q, b, n) == n && q.trk[2].engine == ENG_BUNO && q.trk[3].engine == ENG_MINI && q.trk[5].engine == ENG_ACID && q.trk[4].engine == ENG_BITTAL);
    p.trk[3].engine = ENG_FM;                                           /* tres pesadas: la que no es de su canal vuelve a la de fábrica */
    n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(bm_pattern_unpack(&q, b, n) == n && q.trk[3].engine == ENG_MINI && q.trk[2].engine == ENG_BUNO && q.trk[4].engine == ENG_BITTAL);
    p.trk[2].engine = ENG_ACID;                                         /* con dos, la BX-7 se queda */
    n = bm_pattern_pack(&p, b, sizeof b);
    CHECK(bm_pattern_unpack(&q, b, n) == n && q.trk[3].engine == ENG_FM);

    /* PRESETS: solo los sonidos de la máquina del canal */
    boot(1);
    CHECK(bm_sound_count(&app, 2) == BM_AC_PRESETS && bm_sound_count(&app, 3) == bm_snap_count(ENG_MINI));
    CHECK(bm_sound_count(&app, 4) == bm_snap_count(ENG_BITTAL) && bm_sound_count(&app, 5) == bm_snap_count(ENG_BUNO));
    ch(2);
    enc(ENC_PRESETS, -1); run_ms(20);                                   /* da la vuelta dentro del BB-303 */
    CHECK(app.pat.trk[2].source == BM_AC_PRESETS - 1 && app.pat.trk[2].engine == ENG_ACID);
    CHECK(app.pat.trk[2].macro[0] == bm_ac_presets[BM_AC_PRESETS - 1].cut && strcmp(bm_sound_name(&app, 2), bm_ac_presets[BM_AC_PRESETS - 1].name) == 0);

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
    b[offsetof(bm_meta, fmt)] = 8;                                /* 0.9: sin la voz de la BX-7 */
    CHECK(BM_PAT_FMT == 10 && bm_pattern_unpack(&q, b, n) < 0);
    {                                                             /* el 9 (prm de 48) sí se lee: los efectos de panel, de fábrica */
        static uint8_t b9[BM_PENDING_LEN];
        int h = 64 + (int)sizeof p.trk;
        memcpy(b9, b, (size_t)h);
        b9[offsetof(bm_meta, fmt)] = 9;
        for (int t = 0; t < BM_TRACKS; t++) memcpy(b9 + h + t * 48, p.prm[t], 48);
        memcpy(b9 + h + BM_TRACKS * 48, b + h + (int)sizeof p.prm, (size_t)(n - h - (int)sizeof p.prm));
        int n9 = n - BM_TRACKS * (BM_PRM - 48);
        CHECK(bm_pattern_unpack(&q, b9, n9) == n9 && !memcmp(q.prm[3], p.prm[3], 48) && q.prm[3][PX_DS_G] == p.prm[3][PX_DS_G]);
    }
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
    put_fm_on_t4();                                              /* T4 polifónico, como en las pruebas de antes */
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
    while (app.pat.trk[4].source != nb) { enc(ENC_PRESETS, 1); run_ms(20); }
    run_ms(80);
    static const uint8_t chord[3] = {57, 60, 64};
    CHECK(solo_peak(4, chord, 3, 100, 500) > 0.015);
    put_fm_on_t4();
    CHECK(bm_sound_count(&app, 3) == BM_FM_PATCHES);              /* BX-7: los bancos de usuario, apagados por ahora (2026-10-21) */
    memcpy(app.pat.fmv, bm_fm_patch(BM_FM_PATCHES + 20), BM_FM_PATCH); app.ctx.fmv_gen++;   /* la voz igual se puede cargar */
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
    printf("REC expresivo (PLAY/REC): la nota guarda su largo real, el compás la corta, el REC sigue lo que se toca\n");
    boot(1);
    for (int k = 0; k < BM_EVENTS; k++)                            /* un canal de notas casi vacío: REC con largo fijo */
        for (int s = 0; s < 64; s++) if (!bm_event_empty(bm_evc(&app.pat, 3, s, k))) { bm_event e; bm_event_clear(&e); app_push_ev(&app, 3, s, k, &e); run_ms(1); }
    { bm_event e; bm_event_clear(&e); e.note = 60; app_push_ev(&app, 3, 40, 0, &e); }
    run_ms(40);
    play_rec(3);
    CHECK(app.sloop && app.track == 3 && !app.rec);
    btn(BTN_REC, 30);
    CHECK(app.rec);
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
            printf("  grabada en el paso %d: %d pasos, gate %d, micro %d\n", s + 1, e->len & BM_LEN_MASK, e->gate, e->micro);
            CHECK(s + (e->len & BM_LEN_MASK) <= 16);                /* no pasa del compás */
        }
    CHECK(found);
    CHECK(!app.rec && app.sloop);
    /* QUANT (GLO pág 2): de fábrica 100 % = justo en el paso; 50 % deja la mitad de lo corrido */
    {
        static const uint8_t n1[1] = {62};
        int tps = bm_speed_tps[app.pat.trk[3].speed % SPD_COUNT], q0 = -99, q2 = -99;
        CHECK(app.pat.trk[3].qfree == 0);
        app_rec_at(&app, 0, 3, n1, 1, (uint32_t)(4 * tps + 8)); run_ms(5);
        app_set_track(&app, 3, (uint8_t)offsetof(bm_track, qfree), 2); run_ms(5);
        app_rec_at(&app, 0, 3, n1, 1, (uint32_t)(6 * tps - 8)); run_ms(5);
        for (int k = 0; k < BM_EVENTS; k++) {
            const bm_event *e4 = bm_evc(&app.pat, 3, 4, k), *e6 = bm_evc(&app.pat, 3, 6, k);
            if (!bm_event_empty(e4) && e4->note == 62) q0 = e4->micro;
            if (!bm_event_empty(e6) && e6->note == 62) q2 = e6->micro;
        }
        printf("  QUANT 100 %%: micro %d · 50 %%: micro %d\n", q0, q2);
        CHECK(q0 == 0 && q2 == -4);
        app_set_track(&app, 3, (uint8_t)offsetof(bm_track, qfree), 0); run_ms(5);
    }
    /* el micro-timing suena corrido: una nota con micro 10 en el paso 1 entra tarde */
    btn(BTN_HOME, 30);
    for (int k = 0; k < BM_EVENTS; k++)
        for (int s = 0; s < 64; s++) if (!bm_event_empty(bm_evc(&app.pat, 3, s, k))) { bm_event e; bm_event_clear(&e); app_push_ev(&app, 3, s, k, &e); run_ms(1); }
    { bm_event e; bm_event_clear(&e); e.note = 60; e.len = 1; e.micro = 10; app_push_ev(&app, 3, 0, 0, &e); }
    run_ms(400);
    down(BTN_PLAY); run_ms(1); up(BTN_PLAY);                  /* PLAY: el paso 1 empieza ya */
    int early = 0, later = 0;
    for (int i = 0; i < 4; i++) { run_ms(5); for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) early = 1; }
    run_ms(50);
    for (int v = 0; v < BM_VOICES; v++) if (app.ctx.voice[v].active && app.ctx.voice[v].track == 3) later = 1;
    CHECK(!early && later);
    btn(BTN_PLAY, 20);
}

static void test_factory_and_persist(void)
{
    printf("patrones de fábrica: DA FUNK, BLADE, ENJOY THE SILENCE; escena B muteada; audio, autoguardado y recarga\n");
    boot_with(1, 0);
    CHECK(app.pat.meta.bpm_x10 == 1110 && app.ctx.st.pattern == 0 && app.set.factory == BM_FACTORY_VER);
    CHECK(app.pat.trk[2].engine == ENG_ACID && bm_acid_fx_on(app.pat.prm[2], ACF_OD) && app.pat.trk[5].engine == ENG_BUNO);
    for (int p = 0; p < BM_PATTERNS; p++) CHECK((app.song.scene[p][1].flags & SCN_STORED) && app.song.scene[p][1].trk_mute == 0x3F);
    CHECK((app.song.scene[0][0].flags & SCN_STORED) && !(app.song.scene[3][0].flags & SCN_STORED));
    static const uint16_t BPM[3] = {1110, 1360, 1130};
    static uint8_t b[BM_PENDING_LEN];
    static bm_pattern q;
    for (int i = 1; i < 3; i++) {                                   /* el 2 y el 3 ya están en la flash */
        int n = bm_store_read(&app.store, (uint8_t)i, b, sizeof b);
        CHECK(n > 0 && bm_pattern_unpack(&q, b, n) >= 0 && q.meta.bpm_x10 == BPM[i]);
    }
    CHECK(!bm_store_has(&app.store, 3));
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
    CHECK((app.song.scene[0][1].flags & SCN_STORED) && app.song.scene[0][1].trk_mute == 0x3F && app.song.scene[0][2].trk_mute == 0x04);
    CHECK(app.pat.meta.bpm_x10 == 1110 && bm_store_has(&app.store, 1) && bm_store_has(&app.store, 2));

    /* otros patrones de fábrica (ajustes con otro BM_FACTORY_VER): los guardados se borran una vez;
     * los ajustes se quedan */
    bm_store_write(&app.store, 6, b1, (uint16_t)n1);
    bm_settings st = app.set;
    st.factory = 0; st.led_dim = 3;
    bm_store_write_glob(&app.store, (const uint8_t *)&st, sizeof st);
    boot(0);
    CHECK(!bm_store_has(&app.store, 6) && bm_store_has(&app.store, 1) && app.pat.meta.bpm_x10 == 1110);
    CHECK(app.set.factory == BM_FACTORY_VER && app.set.led_dim == 3);
    run_ms(UI_AUTOSAVE_MS + 1500);
    bm_store_write(&app.store, 6, b1, (uint16_t)n1);
    boot(0);
    CHECK(bm_store_has(&app.store, 6));                             /* la segunda vez ya no borra */
}

static void test_old_version(void)
{
    printf("datos de 0.2: borra y arranca con los de fábrica\n");
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
    CHECK(app.pat.trk[1].source == BM_SRC_NONE && app.pat.tr[0].st[0][0] & ST_ON);   /* DA FUNK (sin tomas) */
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

static void test_draw_cost(void)
{
    printf("pantalla: costo de dibujar un cuadro entero (PC)\n");
    boot(1);
    uint32_t budget = app.scr.budget;
    app.scr.budget = 0;                                          /* aquí se mide el cuadro entero */
    btn(BTN_PLAY, 20);
    run_ms(300);
    draw_one("HOME");
    seq0();
    draw_one("SEQ TR");
    ch(3);                                                       /* PLAY/REC: PIANO ROLL de un canal de notas */
    white(BM_CH_W0 + app_pos(3), 40);
    draw_one("PLAY");
    btn(BTN_REC, 30);
    draw_one("REC");
    to_home();
    btn(BTN_GLO, 30);
    draw_one("GLO");
    btn(BTN_HOME, 30);
    btn(BTN_SAVE, 30);
    draw_one("SAVE");
    btn(BTN_HOME, 30);
    ch(1);
    white(BM_CH_W0 + app_pos(1), 40);
    draw_one("PLAY T2");
    btn(BTN_HOME, 30);
    app.scr.budget = budget;
    btn(BTN_PLAY, 20);
    run_ms(2000);
    /* cambiar de pantalla: el redibujo entero se reparte en varios ticks y termina igual que de una vez */
    static uint16_t ref[240 * 240];
    app.ov.on = 0; app.toast_until = 0;
    memset(sim.fb, 0, sizeof sim.fb);
    app.scr.force = 1; sim.spi_bytes = 0;
    bm_app_tick(&app, now_ms);
    CHECK(sim.spi_bytes <= BM_SCR_BUDGET && app.scr.pending);
    int n = 1;
    while (app.scr.pending && n < 10) { bm_app_tick(&app, now_ms); n++; }
    printf("  redibujo entero: %d ticks de hasta %u B\n", n, (unsigned)BM_SCR_BUDGET);
    CHECK(!app.scr.pending && n >= 2 && n <= 4);
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

static void spi_avg(const char *name)
{
    uint32_t maxb = 0, sum = 0;
    run_ms(300);
    for (int i = 0; i < 60; i++) { sim.spi_bytes = 0; run_ms(33); sum += sim.spi_bytes; if (sim.spi_bytes > maxb) maxb = sim.spi_bytes; }
    printf("  %-10s promedio %u B/frame, máximo %u B (presupuesto ~37 KB a 12 MHz)\n", name, sum / 60, maxb);
    CHECK(sum / 60 < 37000);
}

static void test_spi_budget(void)
{
    printf("pantalla: bytes por frame sonando\n");
    boot(1);
    btn(BTN_PLAY, 20);
    spi_avg("HOME");
    seq0();
    spi_avg("SEQ");
    ch(3);
    white(BM_CH_W0 + app_pos(3), 40);
    spi_avg("PLAY T4");
    btn(BTN_REC, 30);
    spi_avg("REC T4");
    btn(BTN_HOME, 30);
    ch(1);
    white(BM_CH_W0 + app_pos(1), 40);
    spi_avg("PLAY T2");
    btn(BTN_HOME, 30);
    btn(BTN_PLAY, 20);
}

/* ------------------------------------------------------------ etapa 1: HOME, PLAY/REC, batería, GLO */
/* nada cambió: ni pantalla, ni aviso, ni capa, ni aviso de perilla (un control sin función no hace nada) */
typedef struct { uint8_t view, layer, sloop, rec; uint32_t toast; int ov; uint8_t track; } ui_snap;
static ui_snap ui_take(void)
{
    ui_snap s = {app.view, app.layer, app.sloop, app.rec, app.toast_until, app.ov.on, app.track};
    return s;
}
static int ui_same(ui_snap a)
{
    ui_snap b = ui_take();
    return a.view == b.view && a.layer == b.layer && a.sloop == b.sloop && a.rec == b.rec && a.toast == b.toast && a.ov == b.ov && a.track == b.track;
}

/* espera a que el patrón p esté cargado (parado: lo carga un trabajo) */
static int wait_pattern(int p)
{
    for (int i = 0; i < 40 && app.ctx.st.pattern != p; i++) run_ms(50);
    return app.ctx.st.pattern == p;
}

static void test_home(void)
{
    printf("HOME (etapa 1): SELECT recorre los 6 canales, ALGORITHM el BPM global, PRESETS el sonido, K1-K4 la tira del canal\n");
    boot(1);
    CHECK(app.view == V_HOME && app.track == 0 && !app.sloop);
    shot("00-home");
    for (int i = 1; i <= 5; i++) { enc(ENC_SELECT, 1); CHECK(app.track == BM_ORDER[i]); }   /* batería, 3 que eligen, Vital, Slicer */
    enc(ENC_SELECT, 1);
    CHECK(app.track == 0);                                       /* de arriba a abajo, y vuelve al 1 */
    enc(ENC_SELECT, -1);
    CHECK(app.track == BM_ORDER[5]);                             /* y al revés */
    enc(ENC_SELECT, 1);
    /* ALGORITHM: el BPM del proyecto, el mismo en cualquier canal */
    int bpm = app.pat.meta.bpm_x10;
    enc(ENC_ALGO, 1); enc(ENC_ALGO, 1);
    CHECK(app.pat.meta.bpm_x10 == bpm + 20 && app.view == V_HOME && app.ov.beat && app.ov.color == C_TEMPO);
    enc(ENC_SELECT, 1);
    enc(ENC_ALGO, -1);
    CHECK(app.pat.meta.bpm_x10 == bpm + 10 && app.track == BM_ORDER[1]);
    enc(ENC_SELECT, -1);
    /* PRESETS: el kit de la batería, el preset de cada máquina; la máquina sigue siendo la misma */
    int kit = app.pat.trk[0].source;
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.trk[0].source == (kit + 1) % BM_KITS_N && app.pat.trk[0].engine == ENG_DRUM && app.view == V_HOME);
    ch(2);
    int sound = app.pat.trk[2].source;
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.trk[2].source == (sound + 1) % BM_AC_PRESETS && app.pat.trk[2].engine == ENG_ACID);
    ch(3);
    sound = app.pat.trk[3].source;
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.trk[3].source == (sound + 1) % bm_snap_count(ENG_MINI) && app.pat.trk[3].engine == ENG_MINI);
    /* K1-K4: GAIN, FILTER, DELAY, REVERB del canal del cursor (y solo de ese) */
    ch(2);
    uint8_t l0 = app.pat.trk[2].level, s0 = app.pat.trk[2].send, r0 = app.pat.trk[2].rsend, o3 = app.pat.trk[3].level;
    enc(ENC_K1, -4); enc(ENC_K3, 4); enc(ENC_K4, 4);
    CHECK(app.pat.trk[2].level < l0 && app.pat.trk[2].send > s0 && app.pat.trk[2].rsend > r0 && app.pat.trk[3].level == o3);
    for (int i = 0; i < 10; i++) enc(ENC_K2, -3);                /* el filtro: pasa-bajos */
    CHECK(app.pat.trk[2].djf >= 1 && app.pat.trk[2].djf < 64);
    for (int i = 0; i < 30; i++) enc(ENC_K2, 3);
    CHECK(app.pat.trk[2].djf > 64);                              /* y cruza al pasa-altos */
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "VOLUME") && !strcmp(L[1].name, "FILTER") && !strcmp(L[2].name, "DELAY") && !strcmp(L[3].name, "REVERB"));
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s1, "TRACK") && !strcmp(s2, "BPM") && !strcmp(s3, "SOUND"));
    ch(0);
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s3, "KIT"));
    shot("01-home-mixer");
}

static void test_no_function(void)
{
    printf("los controles sin función no hacen nada: sin pantalla, sin aviso y sin marcador de posición\n");
    boot(1);
    ch(2);                                                       /* T3 (BB-303): no tiene ENV */
    run_ms(UI_TOAST_MS + UI_OVERLAY_MS + 200);
    ui_snap s0 = ui_take();
    CHECK(s0.toast == 0 && !s0.ov);
    static const int BTNS[] = {BTN_ENV};
    for (unsigned i = 0; i < sizeof BTNS / sizeof BTNS[0]; i++) {
        btn(BTNS[i], 30);                                        /* un toque */
        CHECK(ui_same(s0));
        btn(BTNS[i], 600);                                       /* mantenido */
        CHECK(ui_same(s0));
        btn(BTNS[i], 2300);                                      /* y 2 s */
        CHECK(ui_same(s0));
    }
    ch(2);                                                       /* T3 (BB-303): la 303 no tiene ENV */
    run_ms(UI_TOAST_MS + UI_OVERLAY_MS + 200);
    s0 = ui_take();
    static const int B303[] = {BTN_ENV};
    for (unsigned i = 0; i < sizeof B303 / sizeof B303[0]; i++) {
        btn(B303[i], 30);
        CHECK(ui_same(s0));
    }
    btn(BTN_OCT_DN, 30); btn(BTN_OCT_UP, 30);                    /* OCT en el mezclador */
    CHECK(ui_same(s0) && app.oct == 0);
    btn(BTN_GLO, 600);                                           /* GLO largo: lo mismo que un toque (abre y nada más) */
    CHECK(app.view == V_GLO && app.glo_page == 0);
    btn(BTN_GLO, 30);                                            /* GLO otra vez: la página siguiente (no sale) */
    CHECK(app.view == V_GLO && app.glo_page == 1);
    btn(BTN_HOME, 30);                                           /* sale HOME */
    CHECK(app.view == V_HOME);
    down(BTN_HOME); run_ms(40); btn(BTN_EDIT, 30); btn(BTN_SEQ, 30); btn(BTN_SAVE, 30); up(BTN_HOME); run_ms(60);   /* HOME + botón: ya no hay combos */
    CHECK(app.view == V_SAVE && app.layer == L_NONE);   /* SAVE abre lo suyo, sin versión global */
    to_home();
    /* la negra 7 y las teclas del mezclador sin función */
    run_ms(UI_TOAST_MS + 200);
    ui_snap s1 = ui_take();
    black(6, 40); black(6, 700);
    CHECK(ui_same(s1) && app.ctx.st.queued == 0xFF);
    bm_app_keys(&app, keys_now);
    CHECK(!keys_now[bm_black_key(6)].label[0] && keys_now[bm_black_key(6)].led == LED_OFF);
    /* en el secuenciador de la batería: lo que no existe tampoco hace nada */
    seq0();
    CHECK(app.view == V_CH && app.track == 0);
    run_ms(UI_TOAST_MS + 200);
    s0 = ui_take();
    static const int BTNS2[] = {BTN_FX, BTN_ARP, BTN_GLO};
    for (unsigned i = 0; i < sizeof BTNS2 / sizeof BTNS2[0]; i++) { btn(BTNS2[i], 30); btn(BTNS2[i], 600); CHECK(ui_same(s0)); }
    for (int b = 8; b <= 9; b++) { black(b, 40); CHECK(app.step_page == 0 && !strcmp(app.toast, "ONE BAR ONLY")); }   /* los compases, con un solo compás: lo dicen */
    run_ms(UI_TOAST_MS + 200);
    s0 = ui_take();
    btn(BTN_REC, 30);
    CHECK(ui_same(s0));                                          /* REC dentro del secuenciador: nada */
    btn(BTN_HOME, 30);
}

static void test_patterns(void)
{
    printf("patrones: blancas 1-10 (un toque), el cambio entra al terminar el compás en curso\n");
    boot(1);
    CHECK(app.ctx.st.pattern == 0);
    white(3, 40);                                                /* parado: ya */
    CHECK(wait_pattern(3));
    white(0, 40);
    CHECK(wait_pattern(0));
    /* sonando: en cola hasta el final del compás (no del ciclo maestro) */
    btn(BTN_PLAY, 20);
    run_ms(1500);                                                /* 160 BPM: un compás son 1500 ms */
    uint32_t bar = BM_BAR_TICKS;
    CHECK(app.ctx.st.pattern == 0);
    while (app.ctx.st.btick % bar > bar / 3 || app.ctx.st.btick % bar < bar / 4) run_ms(20);   /* a mitad del compás */
    uint32_t bt0 = app.ctx.st.btick;
    white(1, 40);
    CHECK(app.ctx.st.queued == 1 && app.ctx.st.pattern == 0);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(1)].led == LED_BLINK);          /* el que espera parpadea */
    uint32_t change = 0;
    for (int i = 0; i < 400 && app.ctx.st.pattern != 1; i++) { bm_app_audio(&app, abuf, 32); if (!(i % 8)) bm_app_tick(&app, now_ms); }
    for (int i = 0; i < 200 && app.ctx.st.pattern != 1; i++) run_ms(10);
    change = app.ctx.st.btick;
    CHECK(app.ctx.st.pattern == 1 && app.ctx.st.queued == 0xFF);
    /* entró en el compás siguiente al que se pidió, desde su primer paso */
    CHECK(change / bar == bt0 / bar + 1 && change % bar < BM_PPQN && app.ctx.st.rtick < BM_PPQN * 2);
    printf("  pedido en el tick %u, entró en el tick %u\n", (unsigned)bt0, (unsigned)change);
    /* el mismo patrón otra vez cancela la cola */
    run_ms(100);
    white(2, 40);
    CHECK(app.ctx.st.queued == 2);
    white(1, 40);                                                /* el que suena: cancela */
    CHECK(app.ctx.st.queued == 0xFF);
    btn(BTN_PLAY, 20);
    /* el patrón es de la consola: mutes y volumen siguen al cambiar de patrón (0.9) */
    black(app_pos(0), 40);
    CHECK(app.pat.trk[0].mute == 1);
    white(2, 40);
    CHECK(wait_pattern(2) && app.pat.trk[0].mute == 1);
    black(app_pos(0), 40);
}

static void test_solo(void)
{
    printf("mutes: un toque silencia, mantener = SOLO; las teclas pulsadas mientras tanto se suman; al soltar la primera vuelve todo\n");
    boot(1);
    black(app_pos(2), 40);
    CHECK(app.pat.trk[2].mute == 1);
    black(app_pos(2), 40);
    CHECK(app.pat.trk[2].mute == 0);
    app.pat.trk[5].mute = 1;                                     /* T6 ya estaba en mute */
    uint8_t before[BM_TRACKS];
    for (int t = 0; t < BM_TRACKS; t++) before[t] = app.pat.trk[t].mute;
    kdown(bm_black_key(app_pos(2))); run_ms(500);                         /* sostenida: SOLO T3 */
    CHECK(app.solo_on && app.solo_track == 2);
    for (int t = 0; t < BM_TRACKS; t++) CHECK(app.pat.trk[t].mute == (t != 2));
    black(app_pos(4), 40);                                                /* otra tecla mientras tanto: T5 queda activa */
    CHECK(app.pat.trk[4].mute == 0 && app.pat.trk[2].mute == 0 && app.pat.trk[0].mute == 1 && app.solo_on);
    kdown(bm_black_key(app_pos(0))); run_ms(500);                         /* sostener otra no empieza otro solo */
    CHECK(app.solo_track == 2);
    kup(bm_black_key(app_pos(0))); run_ms(40);
    CHECK(app.pat.trk[0].mute == 0 && app.solo_on);              /* ...pero la suma */
    kup(bm_black_key(app_pos(2))); run_ms(60);                            /* al soltar la primera: todo como estaba */
    CHECK(!app.solo_on);
    for (int t = 0; t < BM_TRACKS; t++) CHECK(app.pat.trk[t].mute == before[t]);
    app.pat.trk[5].mute = 0;
    /* la luz: ON el solo, DIM el muteado */
    kdown(bm_black_key(app_pos(1))); run_ms(500);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_black_key(app_pos(1))].led == LED_ON && keys_now[bm_black_key(app_pos(3))].led == LED_DIM);
    kup(bm_black_key(app_pos(1))); run_ms(60);
}

static void test_scenes(void)
{
    printf("escenas: guardar = un patrón sostenido + la negra; un toque la lanza; 2 s la borra\n");
    boot(1);
    for (int s = 0; s < BM_SCENES; s++) CHECK(app.song.scene[0][s].flags & SCN_STORED);   /* la demo trae las 4 del patrón 1 */
    CHECK(!(app.song.scene[1][0].flags & SCN_STORED));
    /* guardar: T1 y T2 en mute, patrón 1 sostenido, escena C */
    black(app_pos(0), 40); black(app_pos(1), 40);
    CHECK(app.pat.trk[0].mute && app.pat.trk[1].mute);
    kdown(bm_white_key(0)); run_ms(100);
    bm_app_keys(&app, keys_now);
    CHECK(!strcmp(keys_now[bm_black_key(9)].label, "SC"));       /* mientras el patrón está sostenido, las escenas dicen SAVE */
    black(9, 40);                                                /* escena C */
    kup(bm_white_key(0)); run_ms(60);
    CHECK(app.song.scene[0][2].trk_mute == 0x03 && (app.song.scene[0][2].flags & SCN_STORED));
    CHECK(app.ctx.st.queued == 0xFF && app.ctx.st.pattern == 0);  /* el patrón sostenido no se eligió */
    CHECK(!strncmp(app.toast, "SCENE C A01 STORED", 18));
    /* otro patrón: sus escenas son suyas */
    black(app_pos(3), 40);
    kdown(bm_white_key(4)); run_ms(100);
    black(8, 40);                                                /* escena B del patrón 5 */
    kup(bm_white_key(4)); run_ms(60);
    CHECK((app.song.scene[4][1].flags & SCN_STORED) && app.song.scene[4][1].trk_mute == 0x0B && app.song.scene[0][1].trk_mute == 0x38);
    CHECK(app.ctx.st.queued == 0xFF && app.ctx.st.pattern == 0);
    black(app_pos(3), 40);
    /* un toque sin patrón: la lanza (parado: ya) */
    black(app_pos(0), 40); black(app_pos(1), 40);
    CHECK(!app.pat.trk[0].mute && !app.pat.trk[1].mute);
    black(9, 40); run_ms(60);
    CHECK(app.pat.trk[0].mute && app.pat.trk[1].mute && !app.pat.trk[2].mute && app.ctx.st.scene == 2);
    /* sonando: en el próximo compás */
    btn(BTN_PLAY, 20); run_ms(300);
    CHECK(app.pat.trk[0].mute && app.pat.trk[1].mute && !app.pat.trk[2].mute);
    black(7, 40);                                                /* la A (todo suena) */
    CHECK(app.ctx.st.scene_q == 0 && app.pat.trk[0].mute && app.pat.trk[1].mute);   /* todavía no */
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_black_key(7)].led == LED_BLINK && keys_now[bm_black_key(9)].led == LED_ON);
    black(app_pos(2), 40);                                                /* mientras espera, los mutes siguen en vivo */
    CHECK(app.pat.trk[2].mute);
    run_ms(1700);
    CHECK(app.ctx.st.scene == 0 && app.ctx.st.scene_q == 0xFF);
    for (int t = 0; t < BM_TRACKS; t++) CHECK(!app.pat.trk[t].mute);
    btn(BTN_PLAY, 20);
    /* sostener 2 s: la borra, con su anillo, y no la lanza */
    kdown(bm_black_key(10)); run_ms(800);
    char lb[24];
    int ring = bm_app_key_ring(&app, lb, sizeof lb);
    CHECK(ring >= 0 && !strcmp(lb, "DELETE SCENE D"));
    CHECK(app.song.scene[0][3].flags & SCN_STORED);
    run_ms(1500);                                                /* a los 2.3 s todavía no */
    CHECK(app.song.scene[0][3].flags & SCN_STORED);
    run_ms(2400);                                                /* 3 s en total */
    CHECK(!(app.song.scene[0][3].flags & SCN_STORED) && !strncmp(app.toast, "SCENE D DELETED", 15));
    kup(bm_black_key(10)); run_ms(60);
    CHECK(app.ctx.st.scene_q == 0xFF && app.ctx.st.scene == 0);
    CHECK(bm_app_key_ring(&app, lb, sizeof lb) < 0);
    /* una vacía: un toque no hace nada */
    run_ms(UI_TOAST_MS + 200);
    ui_snap s0 = ui_take();
    uint8_t m0[BM_TRACKS];
    for (int t = 0; t < BM_TRACKS; t++) m0[t] = app.pat.trk[t].mute;
    black(10, 40);
    CHECK(ui_same(s0) && app.ctx.st.scene_q == 0xFF);
    for (int t = 0; t < BM_TRACKS; t++) CHECK(app.pat.trk[t].mute == m0[t]);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_black_key(10)].led == LED_DIM);
    shot("23-scenes");
    /* se guardan con el proyecto */
    run_ms(UI_AUTOSAVE_MS + 1500);
    boot(0);
    CHECK((app.song.scene[0][2].flags & SCN_STORED) && app.song.scene[0][2].trk_mute == 0x03 && !(app.song.scene[0][3].flags & SCN_STORED));
}

static void test_play_rec(void)
{
    printf("PLAY/REC: la blanca del canal del cursor, el teclado toca, REC sobregraba, HOME sale\n");
    boot(1);
    /* la blanca de un canal que no es el del cursor solo lo mueve */
    white(BM_CH_W0 + app_pos(2), 40);
    CHECK(app.track == 2 && !app.sloop && app.view == V_HOME);
    /* con el cursor en él: entra, en la misma pantalla */
    white(BM_CH_W0 + app_pos(2), 40);
    CHECK(app.sloop && app.view == V_HOME && !app.rec && app.track == 2);
    shot("01-home-play");
    bm_app_keys(&app, keys_now);
    CHECK(!strcmp(keys_now[bm_white_key(0)].label, bm_note_names[((BM_KEY_BASE_NOTE + 0 - 12) % 12 + 12) % 12]));   /* notas, no el mezclador */
    /* las teclas tocan la máquina del cursor */
    int k = bm_white_key(4);
    kdown(k); run_ms(80);
    int v = 0;
    for (int i = 0; i < BM_VOICES; i++) v += app.ctx.voice[i].active && app.ctx.voice[i].track == 2;
    CHECK(v >= 1 && app.ctx.st.queued == 0xFF);                  /* suena T3, y no cambió ningún patrón */
    kup(k); run_ms(300);
    /* OCT corre la octava de las notas */
    int note0 = BM_KEY_BASE_NOTE + k - 12;
    btn(BTN_OCT_UP, 20);
    CHECK(app.oct == 1);
    kdown(k); run_ms(80);
    int hi = 0;
    for (int i = 0; i < BM_VOICES; i++) if (app.ctx.voice[i].active && app.ctx.voice[i].track == 2 && app.ctx.voice[i].note == note0 + 12) hi = 1;
    CHECK(hi);
    kup(k); run_ms(100);
    btn(BTN_OCT_DN, 20); CHECK(app.oct == 0);
    /* SELECT: el teclado sigue al canal, sin salir de PLAY/REC */
    enc(ENC_SELECT, 1);
    CHECK(app.track == 3 && app.sloop);
    enc(ENC_SELECT, -1);
    /* REC: grabar mientras suena el patrón: el paso que tocas se reescribe, lo demás queda */
    int before = 0, before_n[BM_STEPS] = {0};
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) before_n[s] += !bm_event_empty(bm_evc(&app.pat, 2, s, e));
    for (int s = 0; s < BM_STEPS; s++) before += before_n[s];
    CHECK(before > 0);
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    CHECK(app.rec && app.sloop && app.view == V_HOME);
    key(bm_white_key(9), 120);
    run_ms(600);
    key(bm_white_key(11), 120);
    btn(BTN_REC, 30);                                            /* REC otra vez: deja de grabar, sigue PLAY/REC */
    CHECK(!app.rec && app.sloop);
    int has9 = 0, kept = 1, clean = 1;
    int n9 = BM_KEY_BASE_NOTE + bm_white_key(9) - 12, n11 = BM_KEY_BASE_NOTE + bm_white_key(11) - 12;
    for (int s = 0; s < BM_STEPS; s++) {
        int nn = 0, mine = 0, other = 0;
        for (int e = 0; e < BM_EVENTS; e++) {
            const bm_event *ev = bm_evc(&app.pat, 2, s, e);
            if (bm_event_empty(ev)) continue;
            nn++;
            if (ev->note == n9 || ev->note == n11) mine = 1; else other = 1;
            if (ev->note == n9) has9 = 1;
        }
        if (!mine) kept &= nn == before_n[s];
        else clean &= !other;
    }
    CHECK(has9 && kept && clean);                                /* el paso tocado quedó solo con lo nuevo; lo demás, igual */
    btn(BTN_PLAY, 20);
    /* HOME: sale al mezclador */
    btn(BTN_HOME, 30);
    CHECK(!app.sloop && !app.rec && app.view == V_HOME);
    /* REC 2 s: borra el canal del cursor (sonando o parado), con su anillo */
    {
        int t0 = app.track, any = 0;
        down(BTN_REC); run_ms(1200);
        int hid = -1;
        CHECK(bm_gesture_progress(&app.ges, now_ms, &hid) >= 0 && hid == BTN_REC);
        run_ms(1000); up(BTN_REC); run_ms(60);
        for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) any |= !bm_event_empty(bm_evc(&app.pat, t0, s, e));
        CHECK(!any && !app.rec && strstr(app.toast, "CLEARED") != NULL);
    }
    /* REC en el mezclador: entra a PLAY/REC ya grabando */
    btn(BTN_REC, 30);
    CHECK(app.sloop && app.rec);
    btn(BTN_HOME, 30);
    CHECK(!app.sloop && !app.rec);
    /* batería: las blancas son los instrumentos; OCT no cambia nada (16 sonidos en una sola página) */
    ch(0);
    white(BM_CH_W0 + app_pos(0), 40);
    CHECK(app.sloop && app.track == 0);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(2)].label[0] && !keys_now[bm_black_key(0)].label[0]);   /* las negras de la batería: sin función */
    int lane = 9;                                                /* un instrumento sin pasos en el kit de la demo */
    while (lane < 16) { int any = 0; for (int s = 0; s < BM_STEPS; s++) any |= app.pat.tr[0].st[lane][s]; if (!any) break; lane++; }
    CHECK(lane < 16);
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    app.ctx.st.lane_trig[0] = 0;
    kdown(bm_white_key(lane)); run_ms(60);
    CHECK(app.ctx.st.lane_trig[0] & (1u << lane) || app.lane_flash[0][lane] > now_ms);
    kup(bm_white_key(lane)); run_ms(40);
    btn(BTN_REC, 30); btn(BTN_PLAY, 20);
    int any = 0;
    for (int s = 0; s < BM_STEPS; s++) any |= app.pat.tr[0].st[lane][s] & ST_ON;
    CHECK(any);                                                  /* el golpe quedó grabado, sin borrar los demás */
    CHECK(app.pat.tr[0].st[0][0] & ST_ON && app.pat.tr[0].st[0][4] & ST_ON);
    int sel = app.sel_lane[0];
    btn(BTN_OCT_UP, 20); btn(BTN_OCT_DN, 20);
    CHECK(app.sel_lane[0] == sel && app.oct == 0);
    /* OCT+ sostenido: golpes fuertes, que se graban con acento (como en SLOOP) */
    for (int s = 0; s < BM_STEPS; s++) app.pat.tr[0].st[lane][s] = 0;
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    down(BTN_OCT_UP); run_ms(40);
    shot("01-home-play-drums");
    kdown(bm_white_key(lane)); run_ms(60); kup(bm_white_key(lane)); run_ms(40);
    up(BTN_OCT_UP); run_ms(40);
    btn(BTN_REC, 30); btn(BTN_PLAY, 20);
    int acc = 0;
    for (int s = 0; s < BM_STEPS; s++) acc |= (app.pat.tr[0].st[lane][s] & (ST_ON | ST_ACC)) == (ST_ON | ST_ACC);
    CHECK(acc && app.oct == 0);
    btn(BTN_HOME, 30);
    /* el sampler: pad por tecla */
    ch(1);
    white(BM_CH_W0 + app_pos(1), 40);
    CHECK(app.sloop && app.track == 1);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && !app.sloop);
}

static void test_free_rec_drums(void)
{
    printf("batería vacía + REC: graba sobre el largo que tiene (no lo alarga)\n");
    boot(1);
    app_push(&app, CMD_TRACK_CLEAR, 0, 0, 0);
    run_ms(60);
    int len0 = bm_trk_len(&app.pat, 0, 0);
    play_rec(0);
    btn(BTN_PLAY, 20); run_ms(200);
    btn(BTN_REC, 30);
    CHECK(app.rec && bm_trk_len(&app.pat, 0, 0) == len0);
    uint32_t bar = 60000u * 4u / 160u;
    run_ms(bar / 3);
    key(bm_white_key(2), 100);
    run_ms(bar);
    key(bm_white_key(5), 100);
    run_ms(bar / 2);
    btn(BTN_REC, 30);
    run_ms(100);
    CHECK(!app.rec && bm_trk_len(&app.pat, 0, 0) == len0 && bm_trk_len(&app.pat, 0, 15) == len0);
    int n2 = 0, n5 = 0;
    for (int s = 0; s < BM_STEPS; s++) { n2 += app.pat.tr[0].st[2][s] & ST_ON; n5 += app.pat.tr[0].st[5][s] & ST_ON; }
    CHECK(n2 >= 1 && n5 >= 1);
    for (int l = 0; l < BM_LANES; l++) for (int s = len0; s < BM_STEPS; s++) CHECK(!(app.pat.tr[0].st[l][s] & ST_ON));
    btn(BTN_PLAY, 20);
    btn(BTN_HOME, 30);
}

static void test_drum_seq(void)
{
    printf("secuenciador de la batería: SEQ entra, blancas = pasos, OCT = página de sonidos, negras = compases, capas\n");
    boot(1);
    ch(2);
    btn(BTN_SEQ, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_SEQ);         /* T3 (BB-303): su piano roll */
    btn(BTN_HOME, 30);
    ch(0);
    btn(BTN_SEQ, 30);
    CHECK(app.view == V_CH && app.track == 0 && app.layer == L_NONE);
    shot("02-seq-tr");
    /* desde HOME, con el cursor en la batería, cada botón del menú lleva directo a lo suyo */
    static const struct { int id, layer; } DIRECTO[] = {{BTN_LFO, L_ACC}, {BTN_ENV, L_HIT}, {BTN_EDIT, L_INST}, {BTN_SCL, L_MUTE}};
    for (unsigned i = 0; i < sizeof DIRECTO / sizeof DIRECTO[0]; i++) {
        btn(BTN_HOME, 30);
        CHECK(app.view == V_HOME);
        btn(DIRECTO[i].id, 30);
        CHECK(app.view == V_CH && app.track == 0 && app.layer == DIRECTO[i].layer && app.layer_latch);
        btn(DIRECTO[i].id, 30);                                  /* otro clic cierra la capa y queda en el secuenciador */
        CHECK(app.view == V_CH && app.layer == L_NONE);
    }
    btn(BTN_HOME, 30);
    white(BM_CH_W0, 40);                                      /* en PLAY/REC de la batería también */
    CHECK(app.sloop);
    btn(BTN_LFO, 30);
    CHECK(!app.sloop && app.view == V_CH && app.layer == L_ACC);
    btn(BTN_HOME, 30); btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_SEQ, 30);
    app.sel_lane[0] = 7;
    CHECK(!(app.pat.tr[0].st[7][3] & ST_ON));
    white(3, 40);
    CHECK(app.pat.tr[0].st[7][3] & ST_ON);                       /* un toque: pone el golpe */
    white(3, 40);
    CHECK(!(app.pat.tr[0].st[7][3] & ST_ON));                    /* otro: lo quita */
    /* OCT: la página de sonidos (los instrumentos 1-8 o 9-16) */
    CHECK(app.sel_lane[0] == 7);
    btn(BTN_OCT_UP, 20);
    CHECK(app.sel_lane[0] == 15 && app.oct == 0);
    btn(BTN_OCT_UP, 20);
    CHECK(app.sel_lane[0] == 15);                                /* no hay más páginas */
    btn(BTN_OCT_DN, 20);
    CHECK(app.sel_lane[0] == 7);
    btn(BTN_OCT_DN, 20);
    CHECK(app.sel_lane[0] == 7);
    /* ALGORITHM elige el instrumento (sin teclas negras) */
    enc(ENC_ALGO, -2); run_ms(700);
    CHECK(app.sel_lane[0] == 5);
    /* con un patrón de un compás, las negras 9 y 10 se ven (tenues) y al tocarlas lo dicen */
    int len = bm_trk_len(&app.pat, 0, 5);
    CHECK(len == 16);
    for (int b = 8; b < 10; b++) { black(b, 40); CHECK(app.sel_lane[0] == 5 && app.step_page == 0 && !strcmp(app.toast, "ONE BAR ONLY")); }
    bm_app_keys(&app, keys_now);
    for (int b = 0; b < 8; b++) CHECK(keys_now[bm_black_key(b)].label[0]);
    CHECK(keys_now[bm_black_key(5)].led == LED_ON && !strcmp(keys_now[bm_black_key(8)].label, "<") && keys_now[bm_black_key(8)].led == LED_DIM
          && !strcmp(keys_now[bm_black_key(10)].label, "RPT"));
    black(2, 40);                                                /* negras 1-8: eligen el instrumento de la página */
    CHECK(app.sel_lane[0] == 2);
    black(5, 40);
    /* las blancas vacías de los tiempos 1-4 (pasos 1, 5, 9, 13) quedan tenues; las otras vacías, apagadas */
    bm_app_keys(&app, keys_now);
    for (int w = 0; w < 16; w++)
        if (!(app.pat.tr[0].st[5][w] & ST_ON) && w != bm_app_track_step(&app, 0, 5))
            CHECK(keys_now[bm_white_key(w)].led == (w % 4 == 0 ? LED_DIM : LED_OFF));
    /* SEQ: un clic (o sostenido) sale al mezclador y otro vuelve a entrar */
    down(BTN_SEQ); run_ms(400); up(BTN_SEQ); run_ms(60);
    CHECK(app.layer == L_NONE && app.view == V_HOME);
    btn(BTN_SEQ, 30);
    CHECK(app.layer == L_NONE && app.view == V_CH && app.sel_lane[0] == 5);
    for (int l = 0; l < BM_LANES; l++) app_push_i(&app, CMD_SET_LANE, 0, (uint8_t)l, LN_LEN, 32);   /* dos compases */
    run_ms(60);
    /* con más de un compás, las negras 9 y 10 recorren los compases en círculo */
    bm_app_keys(&app, keys_now);
    CHECK(!strcmp(keys_now[bm_black_key(8)].label, "<") && !strcmp(keys_now[bm_black_key(9)].label, ">"));
    black(9, 40);
    CHECK(app.step_page == 1);
    white(2, 40);                                                /* el paso 19 */
    CHECK(app.pat.tr[0].st[5][18] & ST_ON && !(app.pat.tr[0].st[5][2] & ST_ON));
    black(9, 40);
    CHECK(app.step_page == 0);                                   /* del último vuelve al primero */
    black(8, 40);
    CHECK(app.step_page == 1);                                   /* y hacia atrás, también en círculo */
    black(8, 40);
    CHECK(app.step_page == 0);
    shot("03-seq-bars");
    /* un paso sostenido: K1 le pone acento (sin P-Locks: se fueron con lo viejo) */
    kdown(bm_white_key(4)); run_ms(500);
    enc(ENC_K1, 5);
    CHECK(bm_tr_lock_get(&app.pat.tr[0], 4, 5, 0) < 0 && (app.pat.tr[0].st[5][4] & ST_ACC));
    enc(ENC_K1, -1);
    CHECK(!(app.pat.tr[0].st[5][4] & ST_ACC) && (app.pat.tr[0].st[5][4] & ST_ON));
    kup(bm_white_key(4)); run_ms(60);
    /* LFO: acentos (con navegación: las negras eligen instrumento y compás) */
    btn(BTN_LFO, 30);
    CHECK(app.layer == L_ACC && app.layer_latch);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_black_key(5)].label[0] && !strcmp(keys_now[bm_black_key(9)].label, "<") && !strcmp(keys_now[bm_black_key(10)].label, ">"));
    CHECK(!keys_now[bm_black_key(8)].label[0]);                  /* la 9: nada */
    white(4, 40);
    CHECK(app.pat.tr[0].st[5][4] & ST_ACC);
    shot("03-seq-accents");
    bm_app_keys(&app, keys_now);                                 /* con acento ON, con golpe MID, vacíos apagados */
    for (int w = 0; w < 16; w++) {
        uint8_t v = app.pat.tr[0].st[5][w];
        CHECK(keys_now[bm_white_key(w)].led == ((v & ST_ACC) ? LED_ON : (v & ST_ON) ? LED_MID : LED_OFF));
    }
    black(1, 40);
    CHECK(app.sel_lane[0] == 1 && app.layer == L_ACC);
    black(5, 40);
    btn(BTN_LFO, 30);
    CHECK(app.layer == L_NONE);
    /* negra 11: repeticiones (un toque la deja abierta), también con navegación */
    black(10, 40);
    CHECK(app.layer == L_REP && app.layer_latch);
    bm_app_keys(&app, keys_now);
    CHECK(!strcmp(keys_now[bm_black_key(10)].label, "RPT") && keys_now[bm_black_key(10)].led == LED_BREATH);
    white(4, 40);
    shot("03-seq-repeat");
    bm_app_keys(&app, keys_now);                                 /* con repetición ON, con golpe MID, vacíos apagados */
    for (int w = 0; w < 16; w++) {
        uint8_t v = app.pat.tr[0].st[5][w];
        CHECK(keys_now[bm_white_key(w)].led == ((v & (ST_RAT_MASK | ST_FLAM)) ? LED_ON : (v & ST_ON) ? LED_MID : LED_OFF));
    }
    black(9, 40);
    CHECK(app.step_page == 1);
    black(9, 40);
    black(10, 40);
    CHECK(app.layer == L_NONE && ((app.pat.tr[0].st[5][4] & ST_RAT_MASK) >> ST_RAT_SH) == 1);
    /* ENV: el sonido del instrumento elegido (el golpe); sostenido es lo mismo que un clic */
    down(BTN_ENV); run_ms(400);
    CHECK(app.layer == L_HIT && app.layer_latch);
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "TUNE") && !strcmp(L[1].name, "DECAY") && !strcmp(L[2].name, "TONE"));
    int dec = app.pat.tr[0].lane[5].decay;
    enc(ENC_K2, 4);
    CHECK(app.pat.tr[0].lane[5].decay == dec + 4);
    white(3, 40);                                                /* otra blanca: ese instrumento */
    CHECK(app.sel_lane[0] == 3);
    black(5, 40);
    up(BTN_ENV); run_ms(60);
    CHECK(app.layer == L_HIT);                                   /* soltarlo no la cierra: otro clic */
    shot("03-seq-hit");
    btn(BTN_ENV, 30);
    CHECK(app.layer == L_NONE);
    /* EDIT: ALGORITHM cambia el sonido (el modelo) de esta batería, se puede combinar cualquiera */
    btn(BTN_EDIT, 30);
    CHECK(app.layer == L_INST && app.layer_latch);
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s2, "MODEL"));
    int model = app.pat.tr[0].lane[5].model, other = app.pat.tr[0].lane[4].model;
    enc(ENC_ALGO, 1);
    CHECK(app.pat.tr[0].lane[5].model == (model + 1) % DRM_COUNT && app.pat.tr[0].lane[4].model == other);
    enc(ENC_K1, -3);
    CHECK(app.pat.tr[0].lane[5].level < 127);
    btn(BTN_EDIT, 30);
    CHECK(app.layer == L_NONE);
    /* negra 11 sostenida: REPEAT solo mientras la sostienes */
    kdown(bm_black_key(10)); run_ms(500);
    CHECK(app.layer == L_REP);
    white(6, 40);
    kup(bm_black_key(10)); run_ms(60);
    CHECK(app.layer == L_NONE && ((app.pat.tr[0].st[5][6] & ST_RAT_MASK) >> ST_RAT_SH) == 1);
    /* SCL: silenciar sonidos de la batería */
    btn(BTN_SCL, 30);
    CHECK(app.layer == L_MUTE && app.layer_latch);
    white(5, 40);
    CHECK((app.pat.tr[0].mute >> 5) & 1);
    white(5, 40);
    CHECK(!((app.pat.tr[0].mute >> 5) & 1));
    {                                                            /* luces fijas: suena = fuerte, muteado = tenue */
        int lb = (app.sel_lane[0] / 8) * 8;
        bm_app_keys(&app, keys_now);
        CHECK(keys_now[bm_white_key(5)].led == LED_ON && !keys_now[bm_white_key(5)].live);
        int act = 0;
        for (int v = 0; v < BM_VOICES; v++) act += app.ctx.voice[v].active;
        black(5 - lb >= 0 && 5 - lb < 8 ? 5 - lb : 0, 40);       /* la negra: acceso rápido, mutea sin sonar */
        int act2 = 0;
        for (int v = 0; v < BM_VOICES; v++) act2 += app.ctx.voice[v].active;
        CHECK(lb == 0 && ((app.pat.tr[0].mute >> 5) & 1) && act2 <= act);
        bm_app_keys(&app, keys_now);
        CHECK(keys_now[bm_white_key(5)].led == LED_DIM && keys_now[bm_black_key(5)].led == LED_DIM);
        black(5, 40);
        CHECK(!((app.pat.tr[0].mute >> 5) & 1));
        int ew = -1;                                             /* uno sin golpes: muteado también queda tenue */
        for (int w = 15; w >= 0 && ew < 0; w--) {
            int any = 0;
            for (int s2 = 0; s2 < BM_STEPS; s2++) any |= app.pat.tr[0].st[w][s2] & ST_ON;
            if (!any) ew = w;
        }
        CHECK(ew >= 0);
        bm_app_keys(&app, keys_now);
        CHECK(keys_now[bm_white_key(ew)].led == LED_OFF);
        white(ew, 40);
        bm_app_keys(&app, keys_now);
        CHECK(((app.pat.tr[0].mute >> ew) & 1) && keys_now[bm_white_key(ew)].led == LED_DIM);
        white(ew, 40);
        bm_app_keys(&app, keys_now);
        CHECK(keys_now[bm_white_key(ew)].led == LED_OFF);
    }
    btn(BTN_SCL, 30);
    CHECK(app.layer == L_NONE);
    black(10, 40);                                               /* otro botón cierra y hace lo suyo */
    btn(BTN_LFO, 30);
    CHECK(app.layer == L_ACC);
    btn(BTN_HOME, 30);                                           /* HOME: primero cierra la capa, luego sale */
    CHECK(app.layer == L_NONE && app.view == V_CH);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && app.layer == L_NONE);
    seq0();

    /* la perilla de la batería: SELECT no hace nada aquí; PRESETS el kit; K1-K4 las macros */
    int kit = app.pat.trk[0].source, mac = app.pat.trk[0].macro[0], pg = app.step_page;
    enc(ENC_SELECT, 1);
    CHECK(app.view == V_CH && app.track == 0 && app.step_page == pg);
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.trk[0].source == (kit + 1) % BM_KITS_N);
    /* sin capa, K1-K4 = LEVEL del elegido, LENGTH del kit entero, DECAY y TONE del elegido (no las macros) */
    int ln = app.sel_lane[0] & 15, lv = app.pat.tr[0].lane[ln].level, len0 = bm_trk_len(&app.pat, 0, 0);
    enc(ENC_K1, -3);
    enc(ENC_K2, 1);
    CHECK(app.pat.trk[0].macro[0] == mac && app.pat.tr[0].lane[ln].level == lv - 3);
    CHECK(bm_trk_len(&app.pat, 0, 0) == len0 + 1 && bm_trk_len(&app.pat, 0, 15) == len0 + 1);
    enc(ENC_K2, -1);
    CHECK(bm_trk_len(&app.pat, 0, 0) == len0 && bm_trk_len(&app.pat, 0, 7) == len0);
    bm_legend LG[4];
    bm_app_legend(&app, LG);
    CHECK(!strcmp(LG[0].name, "LEVEL") && !strcmp(LG[1].name, "LENGTH") && !strcmp(LG[2].name, "DECAY") && !strcmp(LG[3].name, "TONE"));
    /* paso sostenido: K1 su acento, K2 su repetición; soltarlo no lo borra */
    white(7, 40);
    CHECK(app.pat.tr[0].st[ln][app.step_page * 16 + 7] & ST_ON);
    kdown(bm_white_key(7)); run_ms(500);
    bm_app_legend(&app, LG);
    CHECK(!strcmp(LG[0].name, "ACCENT") && !strcmp(LG[1].name, "REPEAT"));
    enc(ENC_K1, 1);
    enc(ENC_K2, 1); run_ms(900);
    enc(ENC_K2, 1);
    kup(bm_white_key(7)); run_ms(60);
    uint8_t sv = app.pat.tr[0].st[ln][app.step_page * 16 + 7];
    CHECK((sv & ST_ON) && (sv & ST_ACC) && ((sv & ST_RAT_MASK) >> ST_RAT_SH) == 2);
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s1, "-") && !strcmp(s2, "INSTR") && !strcmp(s3, "KIT"));
    shot("02-seq-knobs");
    /* HOME vuelve al mezclador; la capa abierta se cierra */
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && app.layer == L_NONE);
}

static void test_glo(void)
{
    printf("GLO del mezclador: SELECT MACHINE asigna la máquina al canal elegido (2 s en una la carga)\n");
    boot(1);
    ch(3);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO && app.track == 3);
    shot("24-glo-machines");
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(BM_CH_W0 + app_pos(3))].led == LED_ON && keys_now[bm_black_key(6)].led == LED_OFF);   /* el teclado sigue siendo el del mezclador */
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s1, "TRACK") && !strcmp(s2, "MACHINE"));
    int bpm = app.pat.meta.bpm_x10;
    CHECK(!strcmp(s3, "-") && app.glo_page == 0);
    btn(BTN_GLO, 30);                                            /* GLO otra vez: la página TEMPO */
    CHECK(app.pat.meta.bpm_x10 == bpm && app.glo_page == 1);
    enc(ENC_ALGO, 2);
    CHECK(app.pat.meta.bpm_x10 == bpm + 20);
    int sw = app.pat.meta.swing;
    enc(ENC_K1, 3);
    CHECK(app.pat.meta.swing == sw + 3);
    shot("24e-glo-tempo");
    btn(BTN_GLO, 30);                                            /* MASTER FX */
    CHECK(app.glo_page == 2);
    int rm = app.pat.meta.rv_mix;
    enc(ENC_K4, -4);
    CHECK(app.pat.meta.rv_mix == rm - 4);
    shot("24f-glo-fx");
    btn(BTN_GLO, 30);                                            /* y vuelve a MACHINES */
    CHECK(app.glo_page == 0);
    /* SELECT: el canal (todos, con vuelta) */
    enc(ENC_SELECT, 1); CHECK(app.track == 5);
    enc(ENC_SELECT, 2); CHECK(app.track == 1);                   /* el Slicer, al final de la cadena */
    enc(ENC_ALGO, 1);                                            /* fija */
    CHECK(!app.mach_arm && app.pat.trk[1].engine == ENG_BREAK && !strcmp(app.toast, "THIS MACHINE IS FIXED"));
    enc(ENC_SELECT, -1); CHECK(app.track == 4);                  /* BITTAL ya no es fija: después viene la BX-7 */
    enc(ENC_ALGO, 1);
    CHECK(app.mach_sel == ENG_FM && app.mach_arm == 1 && app.pat.trk[4].engine == ENG_BITTAL);
    enc(ENC_SELECT, -2);
    CHECK(app.track == 3 && app.mach_trk == 3 && app.mach_sel == ENG_MINI);
    /* una máquina: la cuenta de 2 s y se carga */
    enc(ENC_ALGO, -1);
    CHECK(app.mach_sel == ENG_ACID && app.mach_arm == 1);
    run_ms(1000);
    shot("24c-glo-machines");
    CHECK(app.pat.trk[3].engine == ENG_MINI);
    run_ms(1300);
    CHECK(app.pat.trk[3].engine == ENG_ACID && !app.mach_arm && !strcmp(app.toast, "T3 = BB-303") && app.view == V_GLO);
    /* irse antes de los 2 s: nada */
    enc(ENC_ALGO, 1);
    CHECK(app.mach_sel == ENG_MINI && app.mach_arm == 1);
    run_ms(900);
    enc(ENC_SELECT, 1);                                          /* otro canal: cancela */
    CHECK(!app.mach_arm);
    run_ms(2500);
    CHECK(app.pat.trk[3].engine == ENG_ACID);
    enc(ENC_SELECT, -1);
    enc(ENC_ALGO, 1);
    run_ms(900);
    btn(BTN_HOME, 30);                                           /* HOME: de vuelta, y cancela la cuenta */
    CHECK(app.view == V_HOME && !app.mach_arm);
    run_ms(2500);
    CHECK(app.pat.trk[3].engine == ENG_ACID);
    /* la pesada, una vez: BUNO-6 ya está en el canal 4 (Vital queda fijo en el 5) */
    btn(BTN_GLO, 30);
    enc(ENC_ALGO, 2);                                            /* BB-303 > B-MINI > BUNO-6 */
    CHECK(app.mach_sel == ENG_BUNO && !app.mach_arm && !strcmp(app.toast, "BUNO-6 ALREADY ON T4"));
    shot("24d-glo-heavy");
    enc(ENC_ALGO, 2);                                            /* BUNO-6 > BITTAL > BX-7: ya hay dos pesadas */
    CHECK(app.mach_sel == ENG_FM && !app.mach_arm && !strcmp(app.toast, "2 HEAVY ON T4 T5"));
    run_ms(2500);
    CHECK(app.pat.trk[3].engine == ENG_ACID);
    /* la máquina queda por patrón y las notas del canal quedan */
    int n3 = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) n3 += !bm_event_empty(bm_evc(&app.pat, 3, s, e));
    CHECK(n3 > 0);
    /* lo que ya no está: nada de TEMPO, SCALE ni SONG en GLO */
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
}

static void test_leds(void)
{
    printf("luces: los botones sin función apagados, los de la pantalla en DIM, la pantalla actual en ON\n");
    boot(1);
    CHECK(sim.leds[BTN_HOME] == LED_ON && sim.leds[BTN_GLO] == LED_DIM && sim.leds[BTN_SAVE] != LED_OFF && sim.leds[BTN_REC] == LED_DIM);
    CHECK(sim.leds[BTN_SEQ] == LED_DIM);                         /* el cursor está en la batería */
    CHECK(sim.leds[BTN_FX] == LED_OFF && sim.leds[BTN_ARP] == LED_OFF);
    CHECK(sim.leds[BTN_SCL] == LED_DIM && sim.leds[BTN_ENV] == LED_DIM && sim.leds[BTN_LFO] == LED_DIM && sim.leds[BTN_EDIT] == LED_DIM);   /* llevan directo a su capa */
    CHECK(sim.leds[BTN_OCT_DN] == LED_OFF && sim.leds[BTN_OCT_UP] == LED_OFF);
    enc(ENC_SELECT, 2);
    run_ms(60);
    CHECK(sim.leds[BTN_SEQ] == LED_DIM && sim.leds[BTN_EDIT] == LED_DIM && sim.leds[BTN_FX] == LED_DIM && sim.leds[BTN_ARP] == LED_DIM);   /* T4 (B-MINI): sus menús */
    CHECK(sim.leds[BTN_LFO] == LED_DIM && sim.leds[BTN_ENV] == LED_DIM && sim.leds[BTN_SCL] == LED_DIM);
    enc(ENC_SELECT, -1);
    run_ms(60);
    CHECK(sim.leds[BTN_SEQ] == LED_DIM && sim.leds[BTN_EDIT] == LED_DIM && sim.leds[BTN_FX] == LED_DIM && sim.leds[BTN_ARP] == LED_DIM);   /* T3 (BB-303): sus páginas */
    CHECK(sim.leds[BTN_LFO] == LED_DIM && sim.leds[BTN_SCL] == LED_DIM && sim.leds[BTN_ENV] == LED_OFF);
    enc(ENC_SELECT, 1);
    run_ms(60);
    btn(BTN_GLO, 30);
    CHECK(sim.leds[BTN_GLO] == LED_ON && sim.leds[BTN_HOME] == LED_DIM && sim.leds[BTN_REC] == LED_OFF);
    btn(BTN_HOME, 30);
    /* PLAY/REC: OCT = la octava de las notas */
    white(BM_CH_W0 + 2, 40);
    CHECK(app.sloop && sim.leds[BTN_OCT_DN] == LED_DIM && sim.leds[BTN_OCT_UP] == LED_DIM && sim.leds[BTN_HOME] == LED_MID);
    btn(BTN_REC, 30);
    CHECK(sim.leds[BTN_REC] == LED_ON);
    btn(BTN_REC, 30);
    btn(BTN_HOME, 30);
    /* el secuenciador de la batería */
    seq0();
    CHECK(sim.leds[BTN_SEQ] == LED_ON && sim.leds[BTN_EDIT] == LED_DIM && sim.leds[BTN_ENV] == LED_DIM && sim.leds[BTN_LFO] == LED_DIM && sim.leds[BTN_SCL] == LED_DIM);
    CHECK(sim.leds[BTN_OCT_DN] == LED_ON && sim.leds[BTN_OCT_UP] == LED_DIM && sim.leds[BTN_REC] == LED_OFF && sim.leds[BTN_GLO] == LED_OFF);
    btn(BTN_OCT_UP, 20);
    CHECK(sim.leds[BTN_OCT_UP] == LED_ON && sim.leds[BTN_OCT_DN] == LED_DIM);
    btn(BTN_OCT_DN, 20);
    /* las teclas: los pasos con golpe ON, el cabezal parpadea */
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(0)].led == LED_ON && keys_now[bm_white_key(1)].led == LED_OFF && keys_now[bm_white_key(4)].led == LED_ON);
    down(BTN_EDIT); run_ms(400);
    CHECK(sim.leds[BTN_EDIT] == LED_BREATH && sim.leds[BTN_SEQ] == LED_OFF);
    up(BTN_EDIT); run_ms(60);
    btn(BTN_HOME, 30);
    /* SAVE */
    btn(BTN_SAVE, 30);
    CHECK(app.view == V_SAVE && sim.leds[BTN_SAVE] == LED_ON && sim.leds[BTN_HOME] == LED_DIM);
    btn(BTN_HOME, 30);
}

static void test_roll_audio(void)
{
    printf("ROLL (el motor, sin pantalla): velocidades y cola, ordenados por el audio\n");
    boot(1);
    double fpt = BM_FS * 60.0 / (app.pat.meta.bpm_x10 / 10.0 * BM_PPQN);   /* muestras por tick */
    uint32_t at[64]; uint8_t vel[64];
    app_push_i(&app, CMD_ROLL_CFG, 0, 50, 0, BM_BAR_TICKS);
    app_push_i(&app, CMD_ROLL, 0, 5, 1, 16);                     /* 1/16T: cada 16 ticks */
    int n = roll_hits(0, 5, 700, at, vel, 64);
    CHECK(n >= 8);
    int ok = 1;
    for (int i = 1; i < n && i < 64; i++) if (fabs((double)(at[i] - at[i - 1]) - 16 * fpt) > 40) ok = 0;
    CHECK(ok);
    app_push_i(&app, CMD_ROLL, 0, 5, 1, 8);                      /* 1/32T */
    n = roll_hits(0, 5, 300, at, vel, 64);
    ok = n >= 8;
    for (int i = 1; i < n && i < 64; i++) if (fabs((double)(at[i] - at[i - 1]) - 8 * fpt) > 40) ok = 0;
    CHECK(ok);
    app_push_i(&app, CMD_ROLL, 0, 5, 0, 0);
    CHECK(roll_hits(0, 5, 300, at, vel, 64) == 0 && app.ctx.roll[0].mask == 0);
}

/* ------------------------------------------------------------ idioma */
static int lang_upload(const uint8_t *p, uint32_t n)
{
    if (bm_app_lang_load(&app, 0, 0, NULL, 0)) return -1;
    for (uint32_t off = 0; off < n; off += 200)                  /* de a pedazos, como por USB */
        if (bm_app_lang_load(&app, 1, off, p + off, n - off < 200 ? n - off : 200)) return -1;
    return bm_app_lang_load(&app, 2, 0, NULL, 0);
}

/* El Slicer (2026-10-22): la rejilla por pasos, LENGTH AND SPEED, EDIT (TRIM: cortar la toma en los pads
 * y escribir la secuencia; SOUND), ENV, FX, GLO y SAMPLE */
static void test_slicer(void)
{
    printf("Slicer: rejilla, LENGTH AND SPEED, TRIM, SOUND, ENV, FX, GLO y SAMPLE\n");
    boot_loop();
    ch(1);                                                        /* el Slicer (pista interna T2, el último en pantalla) */
    CHECK(app.track == 1);
    btn(BTN_EDIT, 30);                                           /* desde HOME, EDIT: TRIM */
    CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN && slicer_sub(&app) == 0 && mach_knobs_shown(&app));
    for (int i = 0; i < BM_LANES; i++) bm_pad_clear(&app.pat.pad[i]);
    app.sel_lane[1] = 0;
    enc(ENC_PRESETS, 1);                                         /* PRESETS: la toma entera en el pad 1 */
    CHECK(bm_pad_ok(&app.pat.pad[0]) && app.pat.pad[0].src == 0 && app.pat.pad[0].start == 0);
    uint32_t tlen = app.pat.pad[0].len;
    CHECK(tlen == app.take[0].len);
    shot("90-slicer-trim");
    black(0, 40);                                                /* AUTO: en los golpes, y la secuencia */
    int n = 0, on = 0;
    for (int i = 0; i < BM_LANES; i++) n += bm_pad_ok(&app.pat.pad[i]);
    for (int l = 0; l < BM_LANES; l++) for (int s = 0; s < BM_STEPS; s++) on += (app.pat.tr[1].st[l][s] & ST_ON) != 0;
    CHECK(n >= 4 && on >= n && strstr(app.toast, "PADS + SEQUENCE"));
    black(2, 40);                                                /* 16 partes iguales: los 16 pads y su secuencia */
    n = 0; on = 0;
    for (int i = 0; i < BM_LANES; i++) n += bm_pad_ok(&app.pat.pad[i]);
    for (int l = 0; l < BM_LANES; l++) for (int s = 0; s < BM_STEPS; s++) on += (app.pat.tr[1].st[l][s] & ST_ON) != 0;
    CHECK(n == 16 && on == 16 && app.pat.pad[1].start == tlen / 16 && (app.pat.tr[1].st[1][1] & ST_ON));
    /* TRIM: ZOOM, LENGTH mueve el final, START el inicio (el final queda) */
    uint16_t s0 = app.pat.pad[0].start, l0 = app.pat.pad[0].len;
    enc(ENC_K3, 1);
    CHECK(app.bx.szoom == 1);
    enc(ENC_K2, -1);
    CHECK(app.pat.pad[0].start == s0 && app.pat.pad[0].len < l0);
    uint32_t end = (uint32_t)app.pat.pad[0].start + app.pat.pad[0].len;
    enc(ENC_K1, 1);
    CHECK(app.pat.pad[0].start > s0 && (uint32_t)app.pat.pad[0].start + app.pat.pad[0].len == end);
    shot("90-slicer-trim-zoom");
    enc(ENC_ALGO, 2);                                            /* ALGORITHM: el pad */
    CHECK(app.sel_lane[1] == 2);
    key(bm_white_key(5), 60);                                    /* una blanca: toca y elige su pad */
    CHECK(app.sel_lane[1] == 5);
    btn(BTN_EDIT, 30);                                           /* EDIT otra vez: SOUND */
    CHECK(app.bx.page == BXP_MAIN && slicer_sub(&app) == 1 && mach_knobs_shown(&app));
    enc(ENC_K1, 3); enc(ENC_K4, 1); enc(ENC_K3, 3);
    const bm_pad *p5 = &app.pat.pad[5];
    CHECK(p5->pitch == 27 && (p5->flags & PAD_REV) && p5->mode == PAD_LOOP);
    shot("91-slicer-sound");
    btn(BTN_EDIT, 30);                                           /* y otra vez: TRIM, en círculo */
    CHECK(slicer_sub(&app) == 0);
    btn(BTN_ENV, 30);                                            /* ENV: ataque, release y el loop (modo LOOP) */
    CHECK(app.bx.page == BXP_ENV && !mach_knobs_shown(&app));
    enc(ENC_K1, 10); enc(ENC_K3, 40);
    CHECK(p5->attack > 0 && p5->loop_s > 0);
    shot("92-slicer-env");
    app.pat.pad[5].mode = PAD_ONESHOT;                           /* sin LOOP, el loop no tiene función */
    uint8_t ls = p5->loop_s;
    enc(ENC_K3, 10);
    CHECK(p5->loop_s == ls);
    btn(BTN_ENV, 30);                                            /* ENV otra vez: la rejilla */
    CHECK(app.view == V_CH && app.track == 1 && app.layer == L_NONE);
    shot("93-slicer-seq");
    white(3, 40);                                                /* un paso */
    CHECK(app.pat.tr[1].st[5][3] & ST_ON);
    enc(ENC_K3, 2);                                              /* K3 en la rejilla: PITCH del pad */
    CHECK(p5->pitch == 29);
    btn(BTN_SEQ, 30);                                            /* SEQ otra vez: LENGTH AND SPEED */
    CHECK(app.view == V_CH && app.layer == L_LEN);
    white(11, 40);
    CHECK(app.pat.tr[1].lane[5].len == 12 && app.pat.tr[1].lane[0].len == 16);
    black(4, 40);
    CHECK(app.pat.trk[1].speed == SPD_X1_2);
    black(10, 40); white(0, 40);                                 /* +16: 17 */
    CHECK(app.pat.tr[1].lane[5].len == 17);
    shot("94-slicer-length");
    btn(BTN_SEQ, 30);                                            /* otra vez: la rejilla */
    CHECK(app.view == V_CH && app.layer == L_NONE);
    btn(BTN_LFO, 30);
    CHECK(app.layer == L_ACC);
    btn(BTN_SCL, 30);                                            /* otro botón: cierra y hace lo suyo */
    CHECK(app.layer == L_MUTE);
    btn(BTN_FX, 30);                                             /* FX: la máquina */
    CHECK(app.view == V_MACH && app.bx.page == BXP_FX && app.layer == L_NONE);
    enc(ENC_K4, 1);
    CHECK(app.pat.prm[1][PX_FX] & 1);
    btn(BTN_PLAY, 30); run_ms(800);
    CHECK(app.ctx.st.playing);
    shot("95-slicer-fx");
    btn(BTN_PLAY, 30);
    btn(BTN_GLO, 30);                                            /* GLO: LENGTH de todos los pads */
    CHECK(app.bx.page == BXP_GLO && mach_knobs_shown(&app));
    enc(ENC_K1, 1);
    CHECK(app.pat.tr[1].lane[0].len == app.pat.tr[1].lane[5].len);
    shot("96-slicer-glo");
    /* SAMPLE: graba lo que suena y queda en el pad elegido */
    btn(BTN_EDIT, 30);
    int before = 0;
    for (int i = 0; i < BM_TAKES; i++) before += app.take[i].valid;
    app.sel_lane[1] = 9;
    black(10, 40); run_ms(40);
    CHECK(app.ctx.st.smp_armed);
    btn(BTN_PLAY, 30);
    run_ms(2 * 60000u * 4u * 10u / app.pat.meta.bpm_x10 + 300);
    CHECK(!app.ctx.st.smp_rec && app.ctx.st.smp_unsaved);
    btn(BTN_PLAY, 30); run_ms(1500);
    int after = 0;
    for (int i = 0; i < BM_TAKES; i++) after += app.take[i].valid;
    CHECK(after == before + 1 && bm_pad_ok(&app.pat.pad[9]) && app.pat.pad[9].src != 0);
    /* REC sostenido 2 s: borra la secuencia (los pads quedan) */
    btn(BTN_REC, 2300);
    on = 0;
    for (int l = 0; l < BM_LANES; l++) for (int s = 0; s < BM_STEPS; s++) on += (app.pat.tr[1].st[l][s] & ST_ON) != 0;
    CHECK(on == 0 && bm_pad_ok(&app.pat.pad[0]));
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_SCL, 30);                                            /* HOME, SCL: la rejilla con MUTE */
    CHECK(app.view == V_CH && app.layer == L_MUTE);
    to_home();
    btn(BTN_OCT_UP, 30);
    CHECK(app.view == V_HOME);
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
    CHECK(strcmp(T("CLOCK"), "CLOCK") == 0);
    shot("25-system-en");
    btn(BTN_PLAY, 20);
    CHECK(lang_upload(es, n) < 0);                               /* sonando no se escribe la flash */
    btn(BTN_PLAY, 20); run_ms(100);
    CHECK(lang_upload(es, n) == 0 && app.set.lang == 1);
    CHECK(strcmp(bm_lang_name(), "ESPANOL") == 0);
    CHECK(strcmp(T("CLOCK"), "RELOJ") == 0 && strcmp(T("NOT A PHRASE"), "NOT A PHRASE") == 0);
    char b[32];
    snprintf(b, sizeof b, TF("SAVED A%02d"), 7);
    CHECK(strcmp(b, "GUARDADO A07") == 0);
    /* SISTEMA > LANGUAGE: inglés y vuelta */
    to_home();
    down(BTN_HOME); run_ms(2200); up(BTN_HOME); run_ms(60);
    CHECK(app.view == V_SYSTEM);
    for (int i = 0; i < 3; i++) enc(ENC_SELECT, 1);              /* SYSTEM: reloj, sync, luces, idioma, versión */
    CHECK(app.menu_row == 3 && BM_SYS_ROWS[3] == 9);
    shot("25-system-es");
    enc(ENC_ALGO, -1);                                           /* a la izquierda: inglés */
    CHECK(app.set.lang == 0 && strcmp(T("CLOCK"), "CLOCK") == 0);
    enc(ENC_ALGO, 1);                                            /* a la derecha: el paquete */
    CHECK(app.set.lang == 1 && strcmp(T("CLOCK"), "RELOJ") == 0);
    enc(ENC_ALGO, 1);                                            /* otra vez a la derecha: sigue igual */
    CHECK(app.set.lang == 1);
    /* queda guardado */
    run_ms(UI_AUTOSAVE_MS + 500);
    boot(0);
    CHECK(app.set.lang == 1 && strcmp(T("CLOCK"), "RELOJ") == 0);
    /* un paquete dañado no se usa */
    sim.flash[BM_LANG_LO + 200] ^= 0x55;
    boot(0);
    CHECK(!bm_app_lang_pack(&app) && strcmp(T("CLOCK"), "CLOCK") == 0);
    /* quitarlo */
    sim.flash[BM_LANG_LO + 200] ^= 0x55;
    boot(0);
    CHECK(strcmp(T("CLOCK"), "RELOJ") == 0);
    CHECK(bm_app_lang_load(&app, 3, 0, NULL, 0) == 0 && app.set.lang == 0 && !bm_app_lang_pack(&app));
    CHECK(strcmp(T("CLOCK"), "CLOCK") == 0);
}

/* PIANO ROLL (GLO de la máquina > VIEW) */
static double draw_us(long *math);

static int fx_name_is(int f) { return app.pat.prm[3][BX_ORD0 + app.bx.fxpos] == f; }

static void test_b303(void)
{
    printf("BB-303: la TB-303 en el FM-1 (panel, ACCENT con LFO, SLIDE ligado, FX del TB-03, piano roll)\n");
    boot(1);
    ch(2);
    CHECK(app.pat.trk[2].engine == ENG_ACID);
    btn(BTN_EDIT, 30);                                           /* desde HOME, EDIT: el panel */
    CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN && app.track == 2);
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s1, "-") && !strcmp(s2, "ACCENT") && !strcmp(s3, "PRESET"));
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "CUTOFF") && !strcmp(L[1].name, "RESO") && !strcmp(L[2].name, "ENV MOD") && !strcmp(L[3].name, "DECAY"));
    int cut = app.pat.trk[2].macro[0];
    enc(ENC_K1, 5);
    CHECK(app.pat.trk[2].macro[0] == cut + 5 && !app.ov.on);    /* el panel a la vista: sin el recuadro grande */
    int acc = app.pat.trk[2].accent;
    enc(ENC_ALGO, -3);                                           /* ALGORITHM = ACCENT */
    CHECK(app.pat.trk[2].accent == acc - 3 && !app.ov.on);
    run_ms(100);
    shot("81-b303-panel");
    btn(BTN_EDIT, 30);                                           /* EDIT otra vez: la página 2 (no sale) */
    CHECK(app.bx.page == BXP_MAIN && app.bx.sub[BXP_MAIN] == 1);
    btn(BTN_EDIT, 30);                                           /* y otra vez: la 1, en círculo */
    CHECK(app.bx.page == BXP_MAIN && app.bx.sub[BXP_MAIN] == 0);
    enc(ENC_SELECT, 1);                                          /* SELECT ya no pasa de página */
    CHECK(app.bx.sub[BXP_MAIN] == 0);
    btn(BTN_EDIT, 30);                                           /* la página 2 */
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "TUNING") && !strcmp(L[1].name, "WAVE") && !strcmp(L[2].name, "SLIDE") && !strcmp(L[3].name, "VOLUME"));
    enc(ENC_K2, 1);
    CHECK(app.pat.prm[2][AC_WAVE] == 1);
    shot("81-b303-panel2");
    enc(ENC_K2, -1);
    down(BTN_OCT_UP); run_ms(30);                                /* OCT sostenido + ALGORITHM: TUNING */
    enc(ENC_ALGO, 2);
    up(BTN_OCT_UP); run_ms(30);
    CHECK(app.pat.trk[2].transpose == 26);
    down(BTN_OCT_DN); run_ms(30); down(BTN_OCT_UP); run_ms(30); up(BTN_OCT_UP); up(BTN_OCT_DN); run_ms(40);
    CHECK(app.pat.trk[2].transpose == 24 && app.oct == 0);
    /* FX: OVERDRIVE, DELAY, REVERB, como el TB-03; DELAY y REVERB son los envíos del canal */
    btn(BTN_FX, 30);
    CHECK(app.bx.page == BXP_FX && app.bx.sub[BXP_FX] == 0);
    CHECK(!bm_acid_fx_on(app.pat.prm[2], ACF_OD) && bm_send_on(&app.pat, 2, 0) && bm_send_on(&app.pat, 2, 1));
    CHECK(app.pat.prm[2][AC_OD_G] && app.pat.prm[2][AC_OD_L]);  /* el demo de antes de los FX trae los de fábrica */
    enc(ENC_K4, 1);
    CHECK(bm_acid_fx_on(app.pat.prm[2], ACF_OD));
    enc(ENC_K1, 20);
    shot("81-b303-fx");
    static const uint8_t nt[2] = {45, 57};
    CHECK(solo_peak(2, nt, 1, 100, 300) > 0.02);                 /* con OVERDRIVE suena */
    btn(BTN_FX, 30);                                             /* FX otra vez: la página 2, DELAY y REVERB */
    CHECK(app.bx.page == BXP_FX && app.bx.sub[BXP_FX] == 1);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "DELAY") && !strcmp(L[1].name, "DLY ON") && !strcmp(L[2].name, "REVERB") && !strcmp(L[3].name, "REV ON"));
    shot("81-b303-fx2");
    enc(ENC_K2, -1);                                             /* DELAY apagado: sin envío */
    CHECK(!bm_send_on(&app.pat, 2, 0) && bm_send_on(&app.pat, 2, 1));
    enc(ENC_PRESETS, 1);                                         /* otro sonido: los efectos quedan */
    CHECK(bm_acid_fx_on(app.pat.prm[2], ACF_OD) && !bm_send_on(&app.pat, 2, 0));
    btn(BTN_FX, 30);                                             /* y otra vez: la página 1, sin salir */
    CHECK(app.bx.page == BXP_FX && app.bx.sub[BXP_FX] == 0);
    btn(BTN_EDIT, 30);                                           /* otro menú: sale */
    CHECK(app.bx.page == BXP_MAIN && app.bx.sub[BXP_MAIN] == 0);
    btn(BTN_ENV, 30);                                            /* la 303 no lo tiene */
    CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN);
    /* LFO (un añadido): RATE, DEPTH, WAVE (formas y patrones), DEST */
    btn(BTN_LFO, 30);
    CHECK(app.bx.page == BXP_LFO);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "RATE") && !strcmp(L[1].name, "DEPTH") && !strcmp(L[2].name, "WAVE") && !strcmp(L[3].name, "DEST"));
    CHECK(!strcmp(L[0].val, "1 BAR") && app.pat.prm[2][AC_LFO_DEPTH] == 0);
    for (int i = 0; i < ACW_GATE; i++) enc(ENC_K3, 1);
    for (int i = 0; i < ACL_VOLUME; i++) enc(ENC_K4, 1);
    CHECK(app.pat.prm[2][AC_LFO_WAVE] == ACW_GATE && app.pat.prm[2][AC_LFO_DEST] == ACL_VOLUME);
    enc(ENC_K2, 127);
    CHECK(app.pat.prm[2][AC_LFO_DEPTH] == 127);
    run_ms(UI_OVERLAY_MS + 100);
    shot("81-b303-lfo");
    btn(BTN_PLAY, 20);                                           /* GATE en VOLUME a fondo: hay pasos callados */
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != 2);
    kdown(bm_white_key(0)); run_ms(100);
    {
        double mn = 1e9, mx = 0;
        for (int i = 0; i < 40; i++) {
            peak_abs = 0; run_ms(25);
            if (peak_abs < mn) mn = peak_abs;
            if (peak_abs > mx) mx = peak_abs;
        }
        CHECK(mx > 0.02 && mn < mx * 0.2);
    }
    kup(bm_white_key(0));
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    btn(BTN_PLAY, 20);
    enc(ENC_K2, -127);
    btn(BTN_LFO, 30);
    CHECK(app.bx.page == BXP_MAIN);
    for (int w = 0; w < ACW_N; w++)
        for (int i = 0; i <= 64; i++) {
            float u = bm_acid_lfo_shape(w, (float)i / 64.0f, 3);
            CHECK(u >= 0.0f && u <= 1.0f);
            if (u < 0.0f || u > 1.0f) break;
        }
    /* ARP y GLO */
    btn(BTN_ARP, 30);
    btn(BTN_ARP, 30);                                            /* su página 2 */
    enc(ENC_K1, 1);
    CHECK(app.pat.prm[2][AC_LATCH] == 1);
    enc(ENC_K1, -1);
    btn(BTN_GLO, 30);
    CHECK(app.bx.page == BXP_GLO);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "LENGTH") && !strcmp(L[1].name, "MACHINE") && !strcmp(L[2].name, "MIDI CH") && !strcmp(L[3].name, "MIDI OUT"));
    btn(BTN_GLO, 30);                                            /* su página 2: QUANT y RESET */
    bm_app_legend(&app, L);
    CHECK(app.bx.sub[BXP_GLO] == 1 && !strcmp(L[2].name, "QUANT") && !strcmp(L[3].name, "RESET"));
    CHECK(app.pat.trk[2].qfree == 0);                            /* de fábrica: 100 % */
    enc(ENC_K3, -1);
    CHECK(app.pat.trk[2].qfree == 1 && !strcmp(app.ov.val, "75%"));
    enc(ENC_K3, 1);
    CHECK(app.pat.trk[2].qfree == 0);
    /* REC armado en el panel: la primera nota arranca y graba; tocar ligado = SLIDE */
    btn(BTN_EDIT, 30);
    CHECK(app.bx.page == BXP_MAIN);
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) { bm_event z; bm_event_clear(&z); app_push_ev(&app, 2, s, e, &z); }
    run_ms(40);
    btn(BTN_REC, 30);
    CHECK(app.bx.armed);
    int ka = bm_white_key(0), kb = bm_white_key(4);
    kdown(ka); run_ms(60);
    CHECK(app.ctx.st.playing && app.rec);
    run_ms(300);
    kdown(kb); run_ms(80);                                       /* la anterior sigue sostenida: SLIDE */
    int v = 0;
    for (int i = 0; i < BM_VOICES; i++) v += app.ctx.voice[i].active && app.ctx.voice[i].track == 2 && app.ctx.voice[i].gate;
    CHECK(v == 1);
    kup(ka); run_ms(200);
    kup(kb); run_ms(60);
    shot("81-b303-seq");
    btn(BTN_REC, 30);
    btn(BTN_PLAY, 20);
    const bm_event *ea = NULL, *eb = NULL;
    int sa = -1, sb = -1;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&app.pat, 2, s, e);
        if (bm_event_empty(ev)) continue;
        if (ev->note == BM_KEY_BASE_NOTE + ka) { ea = ev; sa = s; }
        if (ev->note == BM_KEY_BASE_NOTE + kb) { eb = ev; sb = s; }
    }
    CHECK(ea && eb && sa == 0 && sb > 0 && sb < 16);
    CHECK(ea && ea->vel != BM_VEL_ACC && (ea->len & BM_LEN_TIE) && (int)(ea->len & BM_LEN_MASK) == sb);
    CHECK(eb && eb->vel != BM_VEL_ACC && !(eb->len & BM_LEN_TIE));
    /* SCL: el ACCENT de cada paso con las blancas; SCL otra vez, el SLIDE */
    btn(BTN_SCL, 30);
    CHECK(app.bx.page == BXP_SCL && app.bx.sub[BXP_SCL] == 0 && app.bx.blk == 0);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(0)].led == LED_DIM && keys_now[bm_white_key(sb)].led == LED_DIM);
    white(0, 40);
    CHECK(ea->vel == BM_VEL_ACC && eb->vel != BM_VEL_ACC);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(0)].led == LED_ON);
    int empty = sb + 1 < 16 ? sb + 1 : 15;
    white(empty, 40);
    CHECK(strstr(app.toast, "NO NOTE") != NULL);
    shot("81-b303-accent");
    btn(BTN_SCL, 30);
    CHECK(app.bx.page == BXP_SCL && app.bx.sub[BXP_SCL] == 1);
    white(sb, 40);
    CHECK(eb->len & BM_LEN_TIE);
    white(sb, 40);
    CHECK(!(eb->len & BM_LEN_TIE));
    shot("81-b303-slide");
    btn(BTN_SCL, 30);                                            /* SCL otra vez: ACCENT, en círculo */
    CHECK(app.bx.page == BXP_SCL && app.bx.sub[BXP_SCL] == 0);
    btn(BTN_ARP, 30);
    CHECK(app.bx.page == BXP_ARP && app.bx.sub[BXP_ARP] == 0);
    btn(BTN_ARP, 30);
    CHECK(app.bx.page == BXP_ARP && app.bx.sub[BXP_ARP] == 1);
    btn(BTN_LFO, 30); btn(BTN_LFO, 30);                          /* una sola página: otra vez, el panel */
    CHECK(app.bx.page == BXP_MAIN);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
}

static int gated(int t)
{
    int v = 0;
    for (int i = 0; i < BM_VOICES; i++) v += app.ctx.voice[i].active && app.ctx.voice[i].track == t && app.ctx.voice[i].gate;
    return v;
}

static void test_fixes_1022(void)
{
    printf("arreglos 2026-10-22 (manuales de las máquinas, SLOOP, Felucca): acento de la 303, LATCH, notas colgadas, grabar ligado\n");
    boot(1);
    /* BB-303: con ACCENT bajo, un paso normal no suena acentuado (la 303 acentúa solo el paso con acento) */
    ch(2);
    app_set_track(&app, 2, TRK_OFF(accent), 10);
    run_ms(40);
    btn(BTN_PLAY, 20);
    int hi = 0, any = 0;
    for (int i = 0; i < 40; i++) {
        run_ms(10);
        for (int v = 0; v < BM_VOICES; v++)
            if (app.ctx.voice[v].active && app.ctx.voice[v].track == 2 && app.ctx.voice[v].gate) { any = 1; hi |= app.ctx.voice[v].vel >= 120 && app.ctx.voice[v].vel != 127; }
    }
    btn(BTN_PLAY, 20);
    CHECK(any && !hi);
    /* ARP con LATCH: apagar LATCH con las teclas sueltas lo para (manual del FM-1) */
    btn(BTN_ARP, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_ARP);
    enc(ENC_K1, 1);                                              /* MODE UP */
    btn(BTN_ARP, 30);
    enc(ENC_K1, 1);                                              /* LATCH ON */
    CHECK(app.pat.prm[2][AC_LATCH] == 1);
    kdown(bm_white_key(0)); kdown(bm_white_key(2)); run_ms(60);
    kup(bm_white_key(0)); kup(bm_white_key(2)); run_ms(400);
    CHECK(app.ctx.arp[2].n == 2);                                /* sigue arpegiando */
    enc(ENC_K1, -1);                                             /* LATCH OFF */
    run_ms(300);
    CHECK(app.ctx.arp[2].n == 0 && gated(2) == 0);
    /* una tecla apretada antes de prender el arpegiador se suelta normal (no queda colgada) */
    btn(BTN_ARP, 30);
    enc(ENC_K1, -1);                                             /* MODE OFF */
    run_ms(40);
    kdown(bm_white_key(4)); run_ms(60);
    CHECK(gated(2) == 1);
    enc(ENC_K1, 1);                                              /* MODE UP con la tecla apretada */
    run_ms(40);
    kup(bm_white_key(4)); run_ms(200);
    CHECK(gated(2) == 0);
    enc(ENC_K1, -1);
    btn(BTN_HOME, 30);
    /* grabar ligado en B-MINI: la segunda nota no comparte casilla con la que sigue apretada */
    ch(3);
    btn(BTN_SEQ, 30);
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) { bm_event z; bm_event_clear(&z); app_push_ev(&app, 3, s, e, &z); }
    run_ms(40);
    btn(BTN_REC, 30);
    int ka = bm_white_key(0), kb = bm_white_key(2);
    kdown(ka); run_ms(300);
    kdown(kb); run_ms(300);
    kup(ka); run_ms(100);
    kup(kb); run_ms(60);
    btn(BTN_REC, 30); btn(BTN_PLAY, 20);
    int sa = -1, sb = -1;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&app.pat, 3, s, e);
        if (bm_event_empty(ev)) continue;
        if (ev->note == BM_KEY_BASE_NOTE + ka) sa = e;
        if (ev->note == BM_KEY_BASE_NOTE + kb) sb = e;
    }
    CHECK(sa >= 0 && sb >= 0 && sa != sb);
    btn(BTN_HOME, 30);
}

static int step_n(int t, int s)
{
    int n = 0;
    for (int e = 0; e < BM_EVENTS; e++) n += !bm_event_empty(bm_evc(&app.pat, t, s, e));
    return n;
}

static int step_has(int t, int s, int note)
{
    for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&app.pat, t, s, e);
        if (!bm_event_empty(ev) && ev->note == note) return 1;
    }
    return 0;
}

static void test_roll_machines(void)
{
    printf("B-MINI, BUNO-6 y BITTAL: SEQ abre su piano roll (REC tipo SLOOP); SEQ otra vez, su panel\n");
    boot(1);
    static const int TT[3] = {3, 5, 4};
    for (int i = 0; i < 3; i++) {
        ch(TT[i]);
        btn(BTN_SEQ, 30);
        CHECK(app.view == V_MACH && app.bx.page == BXP_SEQ && app.track == TT[i]);
        run_ms(60);
        CHECK(sim.leds[BTN_SEQ] == LED_ON && sim.leds[BTN_FX] == LED_DIM && sim.leds[BTN_EDIT] == LED_DIM && sim.leds[BTN_REC] == LED_DIM);   /* sus menús */
        if (i == 0) {
            bm_legend L[4];
            bm_app_legend(&app, L);
            CHECK(!L[0].name[0] && !strcmp(L[1].name, "SPEED") && !strcmp(L[2].name, "PAGE") && !strcmp(L[3].name, "SCROLL"));
            enc(ENC_K4, 3);                                      /* K4: las alturas que se ven */
            CHECK(app.bx.vy == 3);
            enc(ENC_K4, -3);
            /* GLO: LENGTH (en pasos), MACHINE (solo las que se pueden), MIDI CH, MIDI OUT; pág 2 RESET */
            btn(BTN_GLO, 30);
            CHECK(app.bx.page == BXP_GLO && sim.leds[BTN_GLO] != LED_OFF);
            bm_app_legend(&app, L);
            CHECK(!strcmp(L[0].name, "LENGTH") && !strcmp(L[1].name, "MACHINE") && !strcmp(L[2].name, "MIDI CH") && !strcmp(L[3].name, "MIDI OUT"));
            int len = app.pat.trk[3].length;
            enc(ENC_K1, 1);
            CHECK(app.pat.trk[3].length == len + 1);            /* en pasos: 16, 17, 30... */
            shot("82-mini-glo");
            enc(ENC_K1, -1);
            for (int n = 0; n < 4; n++) enc(ENC_K2, 1);          /* las pesadas que no se pueden no aparecen */
            int shown = app_mach_shown(&app);
            CHECK(shown == ENG_MINI || shown == ENG_BUNO || shown == ENG_BITTAL || shown == ENG_FM);
            CHECK(shown == ENG_MINI || !(bm_machine_conflict(&app.pat, 3, shown) >= 0));
            for (int n = 0; n < 6; n++) enc(ENC_K2, -1);
            CHECK(app_mach_shown(&app) == ENG_ACID && app.mach_arm);
            run_ms(2300);                                        /* 2 s en una: la carga */
            CHECK(app.pat.trk[3].engine == ENG_ACID && app.view == V_MACH && app.bx.page == BXP_GLO);
            enc(ENC_K2, 1);
            run_ms(2300);
            CHECK(app.pat.trk[3].engine == ENG_MINI && app.view == V_MACH);
            btn(BTN_GLO, 30);
            bm_app_legend(&app, L);
            CHECK(app.bx.sub[BXP_GLO] == 1 && !strcmp(L[2].name, "QUANT") && !strcmp(L[3].name, "RESET"));
            btn(BTN_SEQ, 30);                                    /* de vuelta al piano roll */
            CHECK(app.view == V_MACH && app.bx.page == BXP_SEQ);
            run_ms(UI_OVERLAY_MS + 100);
            shot("82-mini-seq");
            /* parado en el SEQ: REC = paso a paso */
            for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) { bm_event z; bm_event_clear(&z); app_push_ev(&app, 3, s, e, &z); }
            run_ms(40);
            btn(BTN_REC, 30);
            CHECK(app.rec && !app.ctx.st.playing && mach_step_mode(&app) && app.bx.step == 0);
            int ka = bm_white_key(7), kb = bm_white_key(9), na = BM_KEY_BASE_NOTE + ka, nb = BM_KEY_BASE_NOTE + kb;
            key(ka, 60);
            CHECK(app.bx.step == 1);
            kdown(ka); kdown(kb); run_ms(60); kup(ka); run_ms(30);
            CHECK(app.bx.step == 1);                            /* el acorde sigue apretado */
            kup(kb); run_ms(40);
            CHECK(app.bx.step == 2);
            enc(ENC_SELECT, 2);                                  /* SELECT mueve el cursor: un silencio */
            CHECK(app.bx.step == 4);
            key(kb, 60);
            CHECK(step_has(3, 0, na) && step_has(3, 1, na) && step_has(3, 1, nb) && step_has(3, 4, nb));
            CHECK(step_n(3, 2) == 0 && step_n(3, 3) == 0 && step_n(3, 0) == 1);
            shot("82-mini-step");
            enc(ENC_SELECT, -5);                                 /* el paso 1 otra vez: se reescribe */
            CHECK(app.bx.step == 0);
            key(kb, 60);
            CHECK(step_has(3, 0, nb) && !step_has(3, 0, na) && step_n(3, 0) == 1);
            /* PLAY: sigue grabando en vivo, con el largo de lo que sostienes */
            btn(BTN_PLAY, 20);
            CHECK(app.ctx.st.playing && app.rec && !mach_step_mode(&app));
            key(bm_white_key(11), 300);
            btn(BTN_REC, 30); btn(BTN_PLAY, 20);
            int found = 0;
            for (int s2 = 0; s2 < BM_STEPS; s2++) found |= step_has(3, s2, BM_KEY_BASE_NOTE + bm_white_key(11));
            CHECK(found && !app.rec);
        }
        btn(BTN_SEQ, 30);                                        /* SEQ otra vez: su panel */
        CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN);
        btn(BTN_HOME, 30);
        CHECK(app.view == V_HOME);
    }
}

static void test_panels(void)
{
    printf("BUNO-6 y BITTAL: los menús del B-MINI (EDIT por secciones, ENV, LFO, FX, SCL); BITTAL dibuja sus tablas\n");
    boot(1);
    bm_legend L[4];
    char ti[24];
    const char *s1, *s2, *s3;
    static const uint8_t nt[1] = {48};
    /* BUNO-6: el Juno-6 */
    ch(5);
    CHECK(app.pat.trk[5].engine == ENG_BUNO);
    btn(BTN_EDIT, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN && !strcmp(mach_title(&app, ti, sizeof ti), "VCF 1/5"));
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "FREQ") && !strcmp(L[1].name, "RES") && !strcmp(L[2].name, "ENV") && !strcmp(L[3].name, "LFO"));
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s2, "FREQ"));
    int f0 = app.pat.trk[5].macro[0];
    enc(ENC_ALGO, -3);
    CHECK(app.pat.trk[5].macro[0] == f0 - 3 && !app.ov.on);
    run_ms(100);
    shot("84-buno-vcf");
    static const char *const BE[5][4] = {{"FREQ", "RES", "ENV", "LFO"}, {"RANGE", "PULSE", "SAW", "SUB"}, {"LFO", "PWM", "PWM MODE", "NOISE"},
                                         {"HPF", "ENV +/-", "KYBD", "VELO"}, {"ENV/GATE", "VELO", "", "VOLUME"}};
    for (int pg = 1; pg <= 5; pg++) {
        btn(BTN_EDIT, 30);
        bm_app_legend(&app, L);
        for (int k = 0; k < 4; k++) CHECK(!strcmp(L[k].name, BE[pg % 5][k]));
        if (pg == 1) shot("84-buno-dco");
    }
    btn(BTN_ENV, 30);                                            /* una sola envolvente: ENV otra vez, el panel */
    CHECK(app.bx.page == BXP_ENV && !strcmp(mach_title(&app, ti, sizeof ti), "ENV"));
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "ATTACK") && !strcmp(L[3].name, "RELEASE"));
    shot("84-buno-env");
    btn(BTN_ENV, 30);
    CHECK(app.bx.page == BXP_MAIN);
    btn(BTN_LFO, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "RATE") && !strcmp(L[1].name, "DELAY"));
    enc(ENC_K2, 30);
    run_ms(UI_OVERLAY_MS + 100);
    shot("84-buno-lfo");
    btn(BTN_FX, 30);
    CHECK(!bm_px_fx_on(app.pat.prm[5], ENG_BUNO, PXF_DIST) && bm_send_on(&app.pat, 5, 1));
    enc(ENC_K4, 1);
    CHECK(bm_px_fx_on(app.pat.prm[5], ENG_BUNO, PXF_DIST));
    CHECK(solo_peak(5, nt, 1, 100, 300) > 0.01);
    btn(BTN_FX, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "CHORUS") && !strcmp(L[2].name, "REVERB"));
    for (int i = 0; i < 4; i++) enc(ENC_K1, 1);
    CHECK(app.pat.prm[5][BN_CHORUS] == BN_CH_BOTH && !strcmp(app.ov.val, "I+II"));
    run_ms(UI_OVERLAY_MS + 100);
    shot("84-buno-fx2");
    enc(ENC_K1, -2);
    btn(BTN_FX, 30); enc(ENC_K4, -1);
    btn(BTN_SCL, 30);
    CHECK(app.bx.page == BXP_SCL && !strcmp(mach_title(&app, ti, sizeof ti), "ACCENT 1-16"));
    btn(BTN_HOME, 30);
    /* BITTAL: Vital */
    ch(4);
    CHECK(app.pat.trk[4].engine == ENG_BITTAL);
    btn(BTN_EDIT, 30);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "OSC 1 1/7"));
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "LEVEL") && !strcmp(L[1].name, "TRANSP") && !strcmp(L[2].name, "FRAME") && !strcmp(L[3].name, "UNISON"));
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s2, "CUTOFF"));
    int c0 = app.pat.prm[4][BT_CUT];
    enc(ENC_ALGO, -4);
    CHECK(app.pat.prm[4][BT_CUT] == c0 - 4 && app.ov.on);       /* el corte no está en OSC 1: el recuadro */
    run_ms(UI_OVERLAY_MS + 100);
    for (int i = 0; i < 10; i++) enc(ENC_K3, 10);                /* FRAME: la onda de abajo cambia */
    run_ms(UI_OVERLAY_MS + 100);
    shot("85-bittal-osc1");
    static const char *const TE[7][2] = {{"LEVEL", "UNISON"}, {"DETUNE", ""}, {"LEVEL", "UNISON"}, {"DETUNE", ""},
                                         {"MODEL", "DRIVE"}, {"ENV2 AMT", "ENV2 POS"}, {"MODE", "VOLUME"}};
    for (int pg = 1; pg <= 7; pg++) {
        btn(BTN_EDIT, 30);
        bm_app_legend(&app, L);
        CHECK(!strcmp(L[0].name, TE[pg % 7][0]) && !strcmp(L[3].name, TE[pg % 7][1]));
        if (pg == 4) {
            enc(ENC_ALGO, 4);                                    /* en FILTER el corte se ve en su perilla */
            CHECK(app.pat.prm[4][BT_CUT] == c0 && !app.ov.on);
            shot("85-bittal-filter");
        }
    }
    btn(BTN_LFO, 30);                                            /* LFO y los destinos de las 4 macros */
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "LFO 1/3"));
    shot("85-bittal-lfo");
    btn(BTN_LFO, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "LFO 2/3 MACRO 1-2") && !strcmp(L[0].name, "M1 TO") && !strcmp(L[3].name, "M2 AMT"));
    btn(BTN_LFO, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "M3 TO") && !strcmp(L[3].name, "M4 AMT"));
    btn(BTN_ENV, 30);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "ENV 1/2 ENV 1"));
    btn(BTN_ENV, 30);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "ENV 2/2 ENV 2"));
    shot("85-bittal-env2");
    btn(BTN_FX, 30);
    enc(ENC_K4, 1);
    btn(BTN_FX, 30);
    enc(ENC_K1, 1);
    CHECK(bm_px_fx_on(app.pat.prm[4], ENG_BITTAL, PXF_DIST) && app.pat.prm[4][PX_CHORUS] == 1);
    CHECK(solo_peak(4, nt, 1, 100, 300) > 0.01);
    shot("85-bittal-fx2");
    enc(ENC_K1, -1);
    btn(BTN_FX, 30); enc(ENC_K4, -1);
    btn(BTN_EDIT, 30);                                           /* sonando: la onda que suena, con lo que la mueve */
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != 4);
    uint32_t sq = bm_wv_btseq;
    kdown(bm_white_key(4)); run_ms(300);
    CHECK(bm_wv_btseq != sq && bm_wv_btpos[0] >= 0.0f && bm_wv_btpos[0] <= 1.0f);
    shot("85-bittal-playing");
    kup(bm_white_key(4)); run_ms(100);
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
}

static void test_bmini(void)
{
    printf("B-MINI: el Minimoog con los menús del POLY D (EDIT, ENV, LFO, FX, SCL, SEQ, ARP, GLO)\n");
    boot(1);
    ch(3);
    CHECK(app.pat.trk[3].engine == ENG_MINI);
    btn(BTN_EDIT, 30);                                           /* EDIT: el panel, FILTER */
    CHECK(app.view == V_MACH && app.bx.page == BXP_MAIN && app.track == 3);
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s2, "CUTOFF") && !strcmp(s3, "PRESET"));
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "CUTOFF") && !strcmp(L[1].name, "EMPHASIS") && !strcmp(L[2].name, "CONTOUR") && !strcmp(L[3].name, "KBD CTRL"));
    int cut = app.pat.trk[3].macro[0];
    enc(ENC_K1, 4);
    CHECK(app.pat.trk[3].macro[0] == cut + 4 && !app.ov.on);
    char ti[24];
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "FILTER 1/5"));
    run_ms(100);
    shot("83-bmini-filter");
    static const char *const EN[5][4] = {{"CUTOFF", "EMPHASIS", "CONTOUR", "KBD CTRL"}, {"RANGE", "WAVE", "LEVEL", "TUNE"},
                                         {"RANGE", "FREQ", "WAVE", "LEVEL"}, {"RANGE", "FREQ", "WAVE", "LEVEL"},
                                         {"NOISE", "GLIDE", "MODE", "VOLUME"}};
    for (int pg = 1; pg <= 5; pg++) {                            /* EDIT otra vez: la siguiente, en círculo */
        btn(BTN_EDIT, 30);
        bm_app_legend(&app, L);
        int q = pg % 5;
        CHECK(app.bx.page == BXP_MAIN && app.bx.sub[BXP_MAIN] == q);
        for (int k = 0; k < 4; k++) CHECK(!strcmp(L[k].name, EN[q][k]));
        if (q == 3) {                                            /* OSC 3: el rango LO primero */
            int r = app.pat.prm[3][MN_R3];
            for (int i = 0; i < 6; i++) enc(ENC_K1, -1);
            bm_app_legend(&app, L);
            CHECK(app.pat.prm[3][MN_R3] == MN_R_LO && !strcmp(L[0].val, "LO"));
            enc(ENC_K1, 1);
            CHECK(app.pat.prm[3][MN_R3] == 0);
            app_push(&app, CMD_SET_PRM, 3, MN_R3, (uint8_t)r); run_ms(5);
            shot("83-bmini-osc3");
        }
    }
    enc(ENC_ALGO, -2);                                           /* ALGORITHM: CUTOFF, en cualquier página */
    CHECK(app.pat.trk[3].macro[0] == cut + 2);
    /* ENV: FILTER CONTOUR y LOUDNESS CONTOUR */
    btn(BTN_ENV, 30);
    CHECK(app.bx.page == BXP_ENV && app.bx.sub[BXP_ENV] == 0);
    int fa = app.pat.prm[3][MN_FA];
    enc(ENC_K1, 3);
    CHECK(app.pat.prm[3][MN_FA] == fa + 3 && app.ov.on);
    run_ms(UI_OVERLAY_MS + 100);
    shot("83-bmini-env");
    btn(BTN_ENV, 30);
    int ar = app.pat.prm[3][MN_AR];
    enc(ENC_K4, -2);
    CHECK(app.bx.sub[BXP_ENV] == 1 && app.pat.prm[3][MN_AR] == ar - 2);
    /* LFO: RATE, WAVE, MOD MIX (OSC 3 <-> LFO), AMOUNT; pág 2 DEST */
    btn(BTN_LFO, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "RATE") && !strcmp(L[1].name, "WAVE") && !strcmp(L[2].name, "MOD MIX") && !strcmp(L[3].name, "AMOUNT"));
    CHECK(!strcmp(L[2].val, "LFO") && app.pat.prm[3][MN_MIX] == 0);   /* de fábrica: solo el LFO, como antes */
    enc(ENC_K2, 1);
    CHECK(app.pat.prm[3][MN_LWAVE] == 1);
    for (int i = 0; i < 6; i++) enc(ENC_K3, -30);
    bm_app_legend(&app, L);
    CHECK(app.pat.prm[3][MN_MIX] == 127 && !strcmp(L[2].val, "OSC 3"));
    for (int i = 0; i < 6; i++) enc(ENC_K4, 30);
    run_ms(UI_OVERLAY_MS + 100);
    shot("83-bmini-lfo");
    static const uint8_t nt[1] = {48};
    CHECK(solo_peak(3, nt, 1, 100, 300) > 0.01);                 /* el OSC 3 modulando: suena */
    btn(BTN_LFO, 30);
    bm_app_legend(&app, L);
    CHECK(app.bx.sub[BXP_LFO] == 1 && !strcmp(L[0].name, "DEST"));
    enc(ENC_K1, 2);
    CHECK(app.pat.prm[3][MN_LDEST] == MN_D_BOTH);
    CHECK(solo_peak(3, nt, 1, 100, 300) > 0.01);
    enc(ENC_K1, -2);
    btn(BTN_LFO, 30);
    for (int i = 0; i < 6; i++) { enc(ENC_K4, -30); enc(ENC_K3, 30); }
    enc(ENC_K2, -1);
    /* FX: DISTORTION; pág 2 CHORUS I / II y REVERB (el DELAY, desde HOME) */
    btn(BTN_FX, 30);
    CHECK(app.bx.page == BXP_FX && !bm_px_fx_on(app.pat.prm[3], ENG_MINI, PXF_DIST) && bm_send_on(&app.pat, 3, 1) && bm_send_on(&app.pat, 3, 0));
    CHECK(app.pat.prm[3][PX_DS_G] && app.pat.prm[3][PX_DS_L]);
    enc(ENC_K4, 1);
    CHECK(bm_px_fx_on(app.pat.prm[3], ENG_MINI, PXF_DIST) && app.pat.prm[3][PX_FX] == 1);
    enc(ENC_K1, 30);
    CHECK(solo_peak(3, nt, 1, 100, 300) > 0.01);
    shot("83-bmini-fx");
    btn(BTN_FX, 30);
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "CHORUS") && !L[1].name[0] && !strcmp(L[2].name, "REVERB") && !strcmp(L[3].name, "REV ON"));
    enc(ENC_K1, 2);
    CHECK(app.pat.prm[3][PX_CHORUS] == 2 && bm_px_fx_on(app.pat.prm[3], ENG_MINI, PXF_CHORUS));
    CHECK(solo_peak(3, nt, 1, 100, 300) > 0.01);                 /* con CHORUS II */
    enc(ENC_K4, -1);
    CHECK(!bm_send_on(&app.pat, 3, 1) && bm_send_on(&app.pat, 3, 0) && (app.pat.prm[3][PX_FX] & 2));
    shot("83-bmini-fx2");
    enc(ENC_PRESETS, 1);                                         /* otro sonido: los efectos quedan */
    CHECK(bm_px_fx_on(app.pat.prm[3], ENG_MINI, PXF_DIST) && app.pat.prm[3][PX_CHORUS] == 2 && !bm_send_on(&app.pat, 3, 1));
    enc(ENC_K4, 1); enc(ENC_K1, -2);
    btn(BTN_FX, 30); enc(ENC_K4, -1);
    CHECK(app.pat.prm[3][PX_FX] == 0 && app.pat.prm[3][PX_CHORUS] == 0);
    /* SCL: ACCENT de cada paso; SCL otra vez: GLIDE */
    for (int st = 0; st < BM_STEPS; st++) for (int e = 0; e < BM_EVENTS; e++)
        if (!bm_event_empty(bm_evc(&app.pat, 3, st, e))) { bm_event z; bm_event_clear(&z); app_push_ev(&app, 3, st, e, &z); run_ms(1); }
    { bm_event e; bm_event_clear(&e); e.note = 48; e.len = 1; e.vel = 100; app_push_ev(&app, 3, 0, 0, &e); e.note = 55; app_push_ev(&app, 3, 1, 0, &e); }
    run_ms(20);
    btn(BTN_SCL, 30);
    CHECK(app.bx.page == BXP_SCL && app.bx.sub[BXP_SCL] == 0);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "ACCENT 1-16"));
    white(0, 40);
    CHECK(bm_evc(&app.pat, 3, 0, 0)->vel == BM_VEL_ACC);
    bm_app_keys(&app, keys_now);
    CHECK(keys_now[bm_white_key(0)].led == LED_ON && keys_now[bm_white_key(1)].led == LED_DIM && keys_now[bm_white_key(2)].led == LED_OFF);
    btn(BTN_SCL, 30);
    CHECK(!strcmp(mach_title(&app, ti, sizeof ti), "GLIDE 1-16"));
    white(0, 40);
    CHECK(bm_evc(&app.pat, 3, 0, 0)->len & BM_LEN_TIE);
    run_ms(60);
    shot("83-bmini-scl");
    btn(BTN_PLAY, 20);                                           /* acento y glide suenan */
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = (uint8_t)(i != 3);
    peak_abs = 0; run_ms(600);
    CHECK(peak_abs > 0.01);
    btn(BTN_PLAY, 20);
    for (int i = 0; i < BM_TRACKS; i++) app.pat.trk[i].mute = 0;
    /* ARP: su LATCH */
    btn(BTN_ARP, 30); btn(BTN_ARP, 30);
    enc(ENC_K1, 1);
    CHECK(app.pat.prm[3][PX_LATCH] == 1);
    enc(ENC_K1, -1);
    /* SEQ y SEQ otra vez: el panel */
    btn(BTN_SEQ, 30);
    CHECK(app.bx.page == BXP_SEQ);
    btn(BTN_SEQ, 30);
    CHECK(app.bx.page == BXP_MAIN && app.view == V_MACH);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
}

static void test_bx7(void)
{
    printf("BX-7: el FM-1 de fábrica dentro de Bubba (páginas, ALGORITHM, FX, EDIT, ARP, REC tipo SLOOP)\n");
    boot(1);
    put_fm_on_t4();
    ch(3);
    CHECK(app.track == 3 && app.pat.trk[3].engine == ENG_FM);
    /* desde HOME, cada botón de menú lleva a su página; GLO sigue siendo el de la consola */
    btn(BTN_LFO, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_LFO && app.track == 3);
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME);
    btn(BTN_GLO, 30);
    CHECK(app.view == V_GLO);
    btn(BTN_HOME, 30);
    btn(BTN_SEQ, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_SEQ);
    shot("80-bx7-seq");
    btn(BTN_SEQ, 30);                                            /* el mismo botón: MAIN */
    CHECK(app.bx.page == BXP_MAIN);
    int alg = app.pat.fmv[BM_FM_ALG];
    enc(ENC_ALGO, 1);
    CHECK(app.pat.fmv[BM_FM_ALG] == (alg + 1) % 32 && bm_fm_alg_of(&app.pat) == (alg + 1) % 32 + 1);
    CHECK(!app.ov.on);                                           /* con el dibujo a la vista, sin el número grande */
    run_ms(100);
    shot("80-bx7-main");
    const char *s1, *s2, *s3;
    bm_app_bigknobs(&app, &s1, &s2, &s3);
    CHECK(!strcmp(s2, "ALG") && !strcmp(s3, "PRESET"));
    /* OCT sostenido + ALGORITHM: transposición; los dos OCT juntos: todo a 0 */
    down(BTN_OCT_UP); run_ms(30);
    enc(ENC_ALGO, 2);
    up(BTN_OCT_UP); run_ms(30);
    CHECK(app.pat.trk[3].transpose == 26 && app.oct == 0);      /* sostenido con ALGORITHM: no cambia la octava */
    btn(BTN_OCT_UP, 30);                                         /* un toque solo: la octava */
    CHECK(app.oct == 1);
    down(BTN_OCT_DN); run_ms(30); down(BTN_OCT_UP); run_ms(30); up(BTN_OCT_UP); up(BTN_OCT_DN); run_ms(40);
    CHECK(app.pat.trk[3].transpose == 24 && app.oct == 0);
    /* PRESETS: solo los de fábrica; la voz se carga en el patrón */
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.trk[3].source == 1 && !memcmp(app.pat.fmv, bm_fm_patch(1), BM_FM_PATCH));
    /* FX: la casilla 1 es el filtro; K4 lo prende; SCL + SELECT lo mueve en la cadena */
    btn(BTN_FX, 30);
    CHECK(app.bx.page == BXP_FX && app.bx.fxpos == 0);
    enc(ENC_K4, 1);
    CHECK(app.pat.prm[3][BX_FXON] & (1u << BXF_FILTER));
    enc(ENC_K2, -5);
    CHECK(app.pat.prm[3][BX_FLT_C] < 90);
    btn(BTN_SCL, 30);
    CHECK(app.bx.grab);
    enc(ENC_SELECT, 1);
    CHECK(app.pat.prm[3][BX_ORD0] == BXF_REVERB && app.pat.prm[3][BX_ORD1] == BXF_FILTER && app.bx.fxpos == 1);
    btn(BTN_SCL, 30);
    CHECK(!app.bx.grab);
    enc(ENC_SELECT, 2);                                          /* la casilla 4: la distorsión */
    enc(ENC_K4, 1);
    CHECK(app.pat.prm[3][BX_FXON] & (1u << BXF_DIST));
    shot("80-bx7-fx");
    static const uint8_t chord[3] = {57, 60, 64};
    CHECK(solo_peak(3, chord, 3, 100, 300) > 0.02);              /* con filtro y distorsión, suena */
    /* la reverb apagada en su FX: el envío del canal no suena (no se procesa dos veces) */
    enc(ENC_SELECT, -3);
    CHECK(fx_name_is(BXF_REVERB));
    enc(ENC_K4, -1);
    CHECK(!(app.pat.prm[3][BX_FXON] & (1u << BXF_REVERB)));
    /* cambiar de preset no borra los efectos */
    enc(ENC_PRESETS, 1);
    CHECK(app.pat.prm[3][BX_FXON] & (1u << BXF_DIST));
    /* ENV y LFO */
    btn(BTN_ENV, 30);
    CHECK(app.bx.page == BXP_ENV);
    enc(ENC_K3, -20);
    CHECK(app.pat.trk[3].sustain < 64);
    shot("80-bx7-env");
    btn(BTN_LFO, 30);
    enc(ENC_K1, 3);
    CHECK(app.pat.fmv[142] == 3);
    btn(BTN_LFO, 30);                                            /* LFO otra vez: su página 2 */
    CHECK(app.bx.page == BXP_LFO && app.bx.sub[BXP_LFO] == 1);
    bm_legend L[4];
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "DELAY") && !strcmp(L[3].name, "OSC SYNC"));
    btn(BTN_LFO, 30);
    CHECK(app.bx.page == BXP_LFO && app.bx.sub[BXP_LFO] == 0);
    shot("80-bx7-lfo");
    /* EDIT: las negras eligen OP1-6, PIT, GLO, MONO, POLY; las blancas tocan */
    btn(BTN_EDIT, 30);
    CHECK(app.bx.page == BXP_EDIT);
    bm_app_keys(&app, keys_now);
    CHECK(!strcmp(keys_now[bm_black_key(2)].label, "OP3") && !strcmp(keys_now[bm_black_key(9)].label, "POLY"));
    black(2, 40);
    CHECK(app.bx.edit == 2);
    int r1 = app.pat.fmv[3 * BM_FM_OP + FMO_R1];                    /* OP3 = el dato 3 */
    enc(ENC_K1, -5);
    CHECK(app.pat.fmv[3 * BM_FM_OP + FMO_R1] == r1 - 5);
    btn(BTN_EDIT, 30); btn(BTN_EDIT, 30);                        /* la página FREQ (EDIT pasa de página) */
    bm_app_legend(&app, L);
    CHECK(!strcmp(L[0].name, "DETUNE") && !strcmp(L[1].name, "COARSE"));   /* el orden del manual del FM-1 */
    shot("80-bx7-edit");
    black(6, 40);
    CHECK(app.bx.edit == BXE_PIT);
    shot("80-bx7-pitch");
    black(8, 40);
    CHECK(app.pat.prm[3][BX_MONO] == 1);
    kdown(bm_white_key(3)); run_ms(40);
    kdown(bm_white_key(5)); run_ms(40);
    int v = 0;
    for (int i = 0; i < BM_VOICES; i++) v += app.ctx.voice[i].active && app.ctx.voice[i].track == 3 && app.ctx.voice[i].gate;
    CHECK(v == 1);                                               /* MONO: una voz */
    kup(bm_white_key(3)); kup(bm_white_key(5)); run_ms(200);
    black(9, 40);
    CHECK(app.pat.prm[3][BX_MONO] == 0);
    /* GLO de la máquina y ARP */
    btn(BTN_GLO, 30);
    CHECK(app.view == V_MACH && app.bx.page == BXP_GLO);
    shot("80-bx7-glo");
    btn(BTN_ARP, 30);
    enc(ENC_K1, 1);
    CHECK(app.pat.trk[3].arp_mode == ARP_UP);
    shot("80-bx7-arp");
    /* REC parado: se arma; la primera nota arranca y graba con su largo */
    enc(ENC_K1, -1);                                             /* ARP apagado */
    CHECK(app.pat.trk[3].arp_mode == ARP_OFF);
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) { bm_event z; bm_event_clear(&z); app_push_ev(&app, 3, s, e, &z); }
    run_ms(40);
    CHECK(!app.ctx.st.playing);
    btn(BTN_REC, 30);
    CHECK(app.bx.armed && !app.rec);
    key(bm_white_key(7), 400);
    CHECK(app.ctx.st.playing && app.rec);
    run_ms(500);
    btn(BTN_REC, 30);                                            /* toma libre: REC cierra el largo */
    CHECK(!app.rec);
    int got = 0, longn = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&app.pat, 3, s, e);
        if (!bm_event_empty(ev)) { got++; if ((ev->len & BM_LEN_MASK) >= 2) longn = 1; }
    }
    CHECK(got >= 1 && longn);
    btn(BTN_SEQ, 30);
    shot("80-bx7-seq-rec");
    btn(BTN_PLAY, 20);
    /* REC sostenido 2 s: borra las notas del canal */
    down(BTN_REC); run_ms(2300); up(BTN_REC); run_ms(40);
    got = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) got += !bm_event_empty(bm_evc(&app.pat, 3, s, e));
    CHECK(got == 0);
    /* el arpegiador graba lo que toca */
    btn(BTN_ARP, 30);
    enc(ENC_K1, 1);
    btn(BTN_PLAY, 20); run_ms(100);
    btn(BTN_REC, 30);
    kdown(bm_white_key(2)); kdown(bm_white_key(6)); run_ms(1500);
    kup(bm_white_key(2)); kup(bm_white_key(6)); run_ms(40);
    btn(BTN_REC, 30); btn(BTN_PLAY, 20);
    got = 0;
    int n2 = BM_KEY_BASE_NOTE + bm_white_key(2), n6 = BM_KEY_BASE_NOTE + bm_white_key(6), both = 0;
    for (int s = 0; s < BM_STEPS; s++) for (int e = 0; e < BM_EVENTS; e++) {
        const bm_event *ev = bm_evc(&app.pat, 3, s, e);
        if (bm_event_empty(ev)) continue;
        got++;
        if (ev->note == n2) both |= 1;
        if (ev->note == n6) both |= 2;
    }
    CHECK(got >= 4 && both == 3);
    /* HOME: a la consola */
    btn(BTN_HOME, 30);
    CHECK(app.view == V_HOME && !app.rec);
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
    test_roll_audio();
    test_voices();
    test_home();
    test_no_function();
    test_patterns();
    test_solo();
    test_scenes();
    test_play_rec();
    test_free_rec_drums();
    test_drum_seq();
    test_glo();
    test_bx7();
    test_b303();
    test_roll_machines();
    test_bmini();
    test_panels();
    test_slicer();
    test_fixes_1022();
    test_leds();
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
    test_migrate();
    test_fm();
    test_factory_and_persist();
    test_old_version();
    test_draw_cost();
    test_spi_budget();
    test_lang();
    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    printf("RAM del firmware (estructura bm_app): %u bytes\n", (unsigned)sizeof(bm_app));
    return fails ? 1 : 0;
}
