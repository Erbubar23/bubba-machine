/* Bubba Machine — aplicación (bucle principal): vistas, controles, guardado. */
#ifndef BM_APP_H
#define BM_APP_H

#include "bm_audio.h"
#include "bm_store.h"
#include "bm_gesture.h"
#include "bm_gfx.h"

/* Lo que la plataforma (HAL) le da a la aplicación. */
typedef struct {
    void (*blit)(void *u, int x, int y, int w, int h, const uint16_t *px);
    void (*led)(void *u, int id, uint8_t level);
    void (*midi_out)(void *u, uint8_t b);                 /* se llama desde la ISR de audio */
    const uint8_t *(*flash_map)(void *u, uint32_t addr);  /* lectura XIP directa */
    const bm_flash_ops *flash;
    void *user;
} bm_platform;

typedef enum {
    V_BOOT, V_HOME, V_SEQ, V_EDIT, V_ENV, V_LFO, V_FX, V_SCL, V_ARP, V_SAVE, V_GLO, V_SONG,
    V_BANK, V_MUTE, V_PUNCH, V_KEYMAP, V_ARPLIVE, V_COUNT
} bm_view;

typedef struct {
    uint8_t ver, clock, sync_mode, sync_ppqn, sync_lat, drive, led_dim, autosave, last_pat;
    uint8_t rsv[23];
} bm_settings;

typedef struct {
    uint8_t on, enc, pct, ghost;   /* ghost 255 = ninguno */
    uint16_t color;
    char title[22], val[12], unit[6];
    uint32_t until;
} bm_overlay;

typedef struct { char name[10], val[10]; uint8_t pct, locked; } bm_legend;

#define BM_JOBS 8
enum { JOB_NONE, JOB_FLUSH_PEND, JOB_SAVE_CUR, JOB_SAVE_AS, JOB_SAVE_SONG, JOB_SAVE_GLOB, JOB_LOAD };

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

    /* portapapeles y undo */
    bm_event clip_ev[BM_STEPS][BM_EVENTS];
    bm_event clip_step[BM_EVENTS];
    bm_track clip_trk;
    uint8_t clip_type;            /* 0 nada, 1 paso, 2 pista */
    bm_event undo_ev[BM_STEPS][BM_EVENTS];
    int8_t undo_track;

    /* UI */
    bm_gesture ges;
    bm_screen scr;
    uint8_t view, layer, track, page, drum_sel, env_page;
    int8_t oct;
    uint8_t rec, glo_down, fill;
    int8_t held_step;             /* paso absoluto mantenido (-1) */
    int8_t held_white;
    uint8_t held_edit, held_had_event, held_long;
    uint32_t step_t0;
    uint8_t keys_down[BM_KEYS];
    uint8_t key_notes[BM_KEYS][4], key_nn[BM_KEYS], key_trk[BM_KEYS];
    int8_t key_rec_step[BM_KEYS], key_rec_slot[BM_KEYS];
    uint16_t key_rec_tick[BM_KEYS];
    uint8_t chord_mod;            /* bits de los modificadores 8-11 mantenidos */
    int8_t last_degree;
    char chord_name[10];
    uint8_t last_note[BM_TRACKS];
    int8_t pat_sel;
    uint32_t pat_sel_t;
    uint8_t menu_row, save_row, save_target, song_cur, clear_armed;
    uint32_t clear_t;
    bm_overlay ov;
    char toast[30];
    uint32_t toast_until;
    uint8_t battery;
    uint16_t master_adc, master_sent;
    uint32_t now, boot_t;
    uint8_t last_trig[BM_TRACKS];
    uint32_t trig_until[BM_TRACKS];
    int8_t punch_key;
    uint32_t clip_until;
    uint8_t smp_was_rec;
} bm_app;

void bm_app_init(bm_app *a, const bm_platform *pf, uint32_t now);
void bm_app_button(bm_app *a, int id, int down, uint32_t now);
void bm_app_key(bm_app *a, int key, int down, uint32_t now);
void bm_app_encoder(bm_app *a, int id, int delta, uint32_t now);
void bm_app_master(bm_app *a, uint16_t adc10);
void bm_app_battery(bm_app *a, uint8_t pct);
void bm_app_midi_in(bm_app *a, uint8_t byte);
void bm_app_tick(bm_app *a, uint32_t now);                     /* bucle principal, ~30 Hz */
void bm_app_audio(bm_app *a, int32_t *out_lr, uint32_t frames); /* ISR de audio */

/* para el dibujo */
int  bm_app_eff_view(const bm_app *a);
void bm_app_legend(const bm_app *a, bm_legend out[4]);
void bm_app_draw(bm_gfx *g, void *user);
void bm_fmt_macro(const bm_app *a, int track, int i, int v, char *buf, int cap);
const char *bm_macro_name(int track, int i);
const char *bm_track_name(int t);
uint16_t bm_track_color(int t);
uint16_t bm_knob_color(int i);
int bm_app_flash_events(const bm_app *a);

#endif
