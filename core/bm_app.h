/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — aplicación (bucle principal): vistas, controles, guardado. */
#ifndef BM_APP_H
#define BM_APP_H

#include "bm_audio.h"
#include "bm_store.h"
#include "bm_gesture.h"
#include "bm_gfx.h"

typedef struct {
    void (*blit)(void *u, int x, int y, int w, int h, const uint16_t *px);
    void (*led)(void *u, int id, uint8_t level);
    void (*midi_out)(void *u, uint8_t b);                 /* desde la ISR de audio */
    const uint8_t *(*flash_map)(void *u, uint32_t addr);  /* lectura XIP directa */
    const bm_flash_ops *flash;
    void *user;
} bm_platform;

typedef enum {
    V_BOOT,
    /* pantallas (toque) */
    V_HOME, V_SCENES, V_SEQ, V_LOOP, V_EDIT, V_INST, V_ENV, V_LFO, V_FX, V_SCL, V_CHORDS,
    V_ARP, V_SAVE, V_GLO, V_SONG,
    /* pantallas especiales (2 s) */
    V_SYSTEM, V_TIME, V_SAMPLER,
    /* capas (mantener) */
    V_BANK, V_MUTE, V_ERASE, V_PUNCH, V_CHORDLIVE, V_ARPLIVE, V_ROLL, V_LANEMUTE,
    V_COUNT
} bm_view;

typedef struct {
    uint8_t ver, clock, sync_mode, sync_ppqn, sync_lat, drive, led_dim, autosave, last_pat;
    uint8_t rsv[23];
} bm_settings;

typedef struct {
    uint8_t on, enc, pct, ghost;
    uint16_t color;
    char title[22], val[12], unit[6];
    uint32_t until;
} bm_overlay;

typedef struct { char name[10], val[10]; uint8_t pct, locked; } bm_legend;

#define BM_JOBS 10
enum { JOB_NONE, JOB_FLUSH_PEND, JOB_SAVE_CUR, JOB_SAVE_AS, JOB_SAVE_SONG, JOB_SAVE_GLOB, JOB_LOAD,
       JOB_SAVE_SAMPLE, JOB_SAMPLE_MARKS, JOB_SAMPLE_ERASE };

typedef union { bm_tr tr; bm_event ev[BM_STEPS][BM_EVENTS]; } bm_trackbuf;

typedef struct bm_app {
    bm_platform pf;

    /* datos compartidos con el audio */
    bm_pattern pat;
    bm_song song;
    bm_cmdq cmdq;
    bm_ctx ctx;
    bm_audio_mem amem;

    /* almacenamiento */
    bm_store store;
    bm_settings set;
    uint8_t pend[BM_PENDING_LEN];
    volatile int16_t pend_obj;
    volatile uint16_t pend_len;
    volatile uint8_t dirty;
    uint8_t set_dirty, song_dirty;
    uint32_t t_edit;
    uint8_t job[BM_JOBS], job_arg[BM_JOBS], njob, job_fading;
    bm_smp_src smp_info[BM_SMP_SLOTS];
    uint8_t smp_valid[BM_SMP_SLOTS];
    bm_smp_src smp_send;          /* copia estable que lee el audio */
    uint8_t chops_mode;           /* 0 manual, 1: 4, 2: 8, 3: 16 */
    uint8_t wave[3][232];         /* energía del sample por columna: graves, medios, agudos (para dibujar) */
    uint8_t wave_was_rec;

    /* portapapeles, undo y redo (una pista) */
    bm_trackbuf clip;
    bm_event clip_step[BM_EVENTS];
    uint8_t clip_type, clip_track;   /* 0 nada, 1 paso, 2 pista */
    bm_trackbuf undo, redo;
    int8_t undo_track, redo_track;

    /* UI */
    bm_gesture ges;
    bm_screen scr;
    uint8_t view, layer, track, page, env_page;
    uint8_t sel_lane[BM_TR_TRACKS];
    int8_t oct;
    uint8_t rec, rec_before, glo_down, fill;
    int8_t held_step, held_white;
    uint8_t held_edit, held_had_event, held_long;
    uint32_t step_t0;
    uint8_t keys_down[BM_KEYS];
    uint8_t key_notes[BM_KEYS][4], key_nn[BM_KEYS], key_trk[BM_KEYS];
    int8_t key_rec_step[BM_KEYS], key_rec_slot[BM_KEYS];
    uint32_t key_rec_tick[BM_KEYS];
    uint32_t key_t0[BM_KEYS];
    uint8_t chord_mod;
    int8_t last_pad;
    uint8_t prev_chord[4], prev_chord_n;
    char chord_name[10], chord_roman[10];
    uint8_t last_note[BM_TRACKS];
    int8_t pat_sel;
    uint32_t pat_sel_t;
    uint16_t erase_mask;          /* capa BORRAR sonando: instrumentos que se borran al pasar */
    uint8_t erase_syn;
    uint8_t roll_rate;            /* índice 0..4 */
    uint16_t roll_mask;
    uint8_t launch_held, chain[8], chain_n;
    uint8_t song_rec;
    uint32_t song_rec_loops;
    uint8_t time_row, time_lane;
    uint8_t menu_row, save_row, save_target, song_cur, clear_armed, init_armed, erase_slot_armed;
    uint32_t clear_t;
    bm_overlay ov;
    char toast[30];
    uint32_t toast_until;
    uint8_t battery;
    uint16_t master_adc, master_sent;
    uint32_t now, boot_t;
    uint8_t last_trig[BM_TRACKS];
    uint32_t trig_until[BM_TRACKS];
    uint32_t lane_flash[BM_TR_TRACKS][BM_LANES];
    int8_t punch_key;
    uint32_t clip_until;
    uint8_t smp_was_rec;
    uint16_t edits_seen;
} bm_app;

void bm_app_init(bm_app *a, const bm_platform *pf, uint32_t now);
void bm_app_button(bm_app *a, int id, int down, uint32_t now);
void bm_app_key(bm_app *a, int key, int down, uint32_t now);
void bm_app_encoder(bm_app *a, int id, int delta, uint32_t now);
void bm_app_master(bm_app *a, uint16_t adc10);
void bm_app_battery(bm_app *a, uint8_t pct);
void bm_app_midi_in(bm_app *a, uint8_t byte);
void bm_app_tick(bm_app *a, uint32_t now);
void bm_app_audio(bm_app *a, int32_t *out_lr, uint32_t frames);
void bm_app_samples_reload(bm_app *a);   /* tras escribir un sample desde fuera (editor/emulador) */

/* Fuentes de T2: 0..BM_SMP_SLOTS-1 son slots de flash, las siguientes son loops de fábrica. */
int  bm_app_smp_sources(void);
int  bm_app_smp_src(const bm_app *a);
bool bm_app_smp_is_rom(int src);
bool bm_app_smp_ready(const bm_app *a, int src);
const char *bm_app_smp_name(int src);

/* para el dibujo */
int  bm_app_eff_view(const bm_app *a);
void bm_app_legend(const bm_app *a, bm_legend out[4]);
void bm_app_draw(bm_gfx *g, void *user);
void bm_fmt_macro(const bm_app *a, int track, int i, int v, char *buf, int cap);
const char *bm_macro_name(int track, int i);
const char *bm_track_name(int t);
uint16_t bm_track_color(int t);
uint16_t bm_knob_color(int i);
const char *bm_lane_name(const bm_app *a, int t, int lane);
int  bm_app_track_step(const bm_app *a, int t, int lane);   /* paso que suena (lane < 0: pista synth) */
int  bm_app_chord_track(const bm_app *a);
extern const char *const bm_roll_names[5];

#endif
