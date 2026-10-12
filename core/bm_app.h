/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — aplicación (bucle principal): vistas, controles, guardado (DISENO_3.0). */
#ifndef BM_APP_H
#define BM_APP_H

#include "bm_audio.h"
#include "bm_store.h"
#include "bm_takes.h"
#include "bm_gesture.h"
#include "bm_gfx.h"
#include "bm_params.h"

typedef struct {
    void (*blit)(void *u, int x, int y, int w, int h, const uint16_t *px);
    void (*led)(void *u, int id, uint8_t level);
    void (*midi_out)(void *u, uint8_t b);                 /* desde la ISR de audio */
    const uint8_t *(*flash_map)(void *u, uint32_t addr);  /* lectura XIP directa */
    const bm_flash_ops *flash;
    void *user;
} bm_platform;

/* Pantallas. Los canales usan V_CH con su página (pestaña). */
typedef enum {
    V_BOOT,
    V_HOME, V_MIX, V_SCENES, V_SONG,           /* HOME, y HOME otra vez */
    V_CH,                                       /* un canal: a->page */
    V_GLO, V_SYSTEM, V_TIME, V_SAVE,           /* globales */
    V_FXG, V_SCLG, V_COPYCH,                   /* HOME + FX, HOME + SCL, HOME + EDIT */
    V_COUNT
} bm_view;

/* Páginas de un canal. Las pestañas dependen del tipo de canal (bm_app_tabs). */
typedef enum {
    PG_SEQ, PG_SOUND, PG_INST, PG_MIX, PG_MORE, PG_RETRIG, PG_TAKES, PG_ENV, PG_ARP, PG_LOOP, PG_MIDI,
    PG_SCL, PG_CHORDS, PG_LFO, PG_REC, PG_CUT, PG_COUNT
} bm_page;

/* Capas en vivo (mantener un botón). */
typedef enum {
    L_NONE, L_HOME, L_LEN, L_ERASE, L_ACC, L_REP, L_PUNCH, L_CHORDS, L_ARP, L_ROLL, L_COPY, L_TONE, L_LANEMUTE, L_FILL,
    L_COUNT
} bm_layer;

/* Preguntas: OCT+ = sí, HOME = no. */
enum { ASK_NONE, ASK_CLEAR, ASK_ENGINE, ASK_COPY_PAT, ASK_COPY_TRK, ASK_TAKE_DEL, ASK_INIT };

typedef struct {
    uint8_t ver, clock, sync_mode, sync_ppqn, sync_lat, drive, led_dim, autosave, last_pat;
    uint8_t enter;          /* recuadro de ALGORITHM: 0, 250, 400, 800 ms u OFF */
    uint8_t rsv[22];
} bm_settings;

typedef struct {
    uint8_t on, enc, pct, ghost;
    uint16_t color;
    char title[22], val[12], unit[6];
    uint32_t until;
} bm_overlay;

typedef struct { char name[10], val[10]; uint8_t pct, locked; } bm_legend;

/* Mapa del teclado de una capa: lo que hace cada tecla, para dibujarlo y para los LEDs. */
typedef struct { char label[6]; uint8_t led; uint16_t color; } bm_keyfn;

#define BM_JOBS 10
enum { JOB_NONE, JOB_FLUSH_PEND, JOB_SAVE_CUR, JOB_SAVE_AS, JOB_SAVE_SONG, JOB_SAVE_GLOB, JOB_LOAD,
       JOB_SAVE_TAKE, JOB_TAKE_META, JOB_TAKE_ERASE, JOB_COPY_PAT, JOB_COPY_TRK };

typedef union { bm_tr tr; bm_notes nt; } bm_trackbuf;

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

    /* sampler: tomas, fuentes y dibujo */
    bm_take take[BM_TAKES];
    bm_smp_src smp_send[3];       /* copias estables que lee el audio: loop A, loop B, escucha */
    uint8_t wave[3][232];         /* loop A por columna: graves, medios, agudos */
    uint8_t wave_cut[3][232];     /* la toma que se está cortando */
    uint8_t slice_e[BM_SLICES][5];/* energía de cada slice del loop A (barras de SEQ) */
    uint8_t wave_was_rec, take_dirty, takes_row, cut_take, cut_sel, cut_snap, cut_zoom;
    uint8_t sent_src[2], src_dirty, wave_src;

    /* portapapeles, undo y redo (un canal) */
    bm_trackbuf clip;
    bm_track clip_trk;
    bm_event clip_step[BM_EVENTS];
    uint8_t clip_type, clip_track;   /* 0 nada, 1 paso, 2 canal */
    bm_trackbuf undo, redo;
    int8_t undo_track, redo_track;

    /* UI */
    bm_gesture ges;
    bm_screen scr;
    uint8_t view, page, layer, track, step_page, env_page;
    uint8_t ch_page[BM_TRACKS];   /* última página de cada canal */
    uint8_t prev_view, prev_page;
    uint8_t sel_lane[BM_TRACKS];
    uint8_t ed_page[BM_TRACKS];   /* página del editor de sonido (ANALOG) o del instrumento (batería) */
    int8_t oct, note_oct;
    uint8_t rec, rec_before, rec_hold, fill;
    uint16_t home_mod;            /* botones que se apretaron con HOME sostenido */
    uint8_t home_used;
    uint8_t box_on, box_track, box_hold;
    uint8_t box_mode, box_lane;   /* recuadro de ALGORITHM: BOX_HOME, BOX_CH (dentro de un canal) o BOX_LANE (instrumento) */
    uint8_t layer_latch;          /* la capa quedó abierta con un toque (negras 10 y 11): se cierra con la misma tecla o HOME */
    uint32_t box_until;
    int8_t held_step, held_white;
    uint8_t held_edit, held_had_event, held_long, held_notes;
    uint32_t step_t0;
    uint8_t keys_down[BM_KEYS];
    uint8_t key_notes[BM_KEYS][4], key_nn[BM_KEYS], key_trk[BM_KEYS];
    int8_t key_rec_step[BM_KEYS], key_rec_slot[BM_KEYS];
    uint32_t key_rec_tick[BM_KEYS];
    uint32_t key_t0[BM_KEYS];
    uint8_t key_role[BM_KEYS];    /* qué hizo la tecla al bajar (para soltarla bien) */
    uint16_t home_done;
    uint8_t eng_pending, rec_now, solo_mask;
    uint8_t hold_play;            /* sampler: un paso sostenido está haciendo sonar el loop */
    uint8_t rec_free;             /* canal + 1 que graba en libre: el largo se cierra al parar REC */
    uint32_t rec_free_t0;         /* btick del inicio (0xFFFFFFFF = empieza con PLAY) */
    uint8_t chord_mod;
    int8_t last_pad;
    uint8_t prev_chord[4], prev_chord_n;
    char chord_name[10], chord_roman[10];
    uint8_t last_note[BM_TRACKS];
    int8_t pat_sel;
    uint32_t pat_sel_t;
    uint16_t pat_held;            /* blancas sostenidas en HOME: cadena de patrones */
    uint8_t pchain[8], pchain_n;
    int8_t solo_key, solo_track;
    uint8_t solo_on, solo_saved;
    uint16_t erase_mask;          /* capa BORRAR sonando: instrumentos que se borran al pasar */
    uint8_t erase_syn;
    uint8_t roll_rate;            /* índice 0..4 */
    uint16_t roll_mask;
    uint8_t brk_live;
    uint8_t len_page;             /* capa LARGO: OCT +/- = largos 17-64 */
    uint8_t launch_held, chain[8], chain_n;
    uint8_t song_rec;
    uint32_t song_rec_loops;
    uint32_t tap_t[4];
    uint8_t tap_n;
    uint8_t time_row, time_lane;
    uint8_t menu_row, glo_row, more_row, save_row, save_target, song_cur, clear_armed;
    uint8_t copy_src, copy_dst, copy_pat_dst, copy_trk;
    uint8_t ask, ask_arg, ask_arg2;
    char ask_text[2][30];
    uint32_t clear_t;
    bm_overlay ov;
    char toast[30];
    uint32_t toast_until;
    uint8_t battery;
    uint16_t master_adc, master_sent;
    uint32_t now, boot_t;
    uint8_t last_trig[BM_TRACKS];
    uint32_t trig_until[BM_TRACKS];
    uint32_t lane_flash[BM_TRACKS][BM_LANES];
    int8_t punch_key;
    uint32_t clip_until;
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
void bm_app_takes_reload(bm_app *a);   /* tras escribir una toma desde fuera (editor/emulador) */

/* Fuentes del sampler: 0..7 = toma, BM_SRC_ROM|i = loop de fábrica. */
int  bm_app_sources(const bm_app *a, uint8_t out[BM_TAKES + 8]);   /* lista de fuentes que existen */
bool bm_app_src_ok(const bm_app *a, uint8_t src);
void bm_app_src_name(const bm_app *a, uint8_t src, char *buf, int cap);
bool bm_app_src_info(const bm_app *a, uint8_t src, bm_smp_src *out);
float bm_app_brk_semis(const bm_app *a);   /* cuántos semitonos se movió el loop A (tempo + PITCH) */

/* para el dibujo */
int  bm_app_layer(const bm_app *a);
int  bm_app_tabs(const bm_app *a, int t, uint8_t out[8]);   /* pestañas de un canal */
const char *bm_page_name(int page);
void bm_app_legend(const bm_app *a, bm_legend out[4]);
void bm_app_keymap(const bm_app *a, bm_keyfn out[BM_KEYS], char *title, int cap, uint16_t *color, const char **h1, const char **h2);
void bm_app_draw(bm_gfx *g, void *user);
void bm_fmt_macro(const bm_app *a, int track, int i, int v, char *buf, int cap);
const char *bm_macro_name(const bm_app *a, int track, int i);
/* editor de sonido por páginas (ANALOG en SONIDO, el instrumento de batería en INSTR) */
const bm_ppage *bm_app_ed_pages(const bm_app *a, int t, int *n);
int  bm_app_ed_on(const bm_app *a);
int  bm_app_ed_value(const bm_app *a, int t, const bm_pdef *d);
const char *bm_app_ed_name(const bm_app *a, int t, const bm_pdef *d);
const char *bm_track_name(const bm_app *a, int t);
const char *bm_kit_name(int kit);                       /* KO-808, KO-909 (ROM) o SINTE */
const char *bm_sound_name(const bm_app *a, int t);      /* kit, timbre o banco de ondas del canal */
int  bm_sound_count(const bm_app *a, int t);            /* cuántos hay para PRESETS */
uint16_t bm_track_color(int t);
uint16_t bm_knob_color(int i);
const char *bm_lane_name(const bm_app *a, int t, int lane);
int  bm_app_track_step(const bm_app *a, int t, int lane);   /* paso que suena (lane < 0: canal de notas) */
int  bm_app_chord_track(const bm_app *a);
int  bm_app_enter_ms(const bm_app *a);                       /* -1 = OFF */
uint32_t bm_app_rec_room(const bm_app *a);                   /* muestras que caben en la próxima toma */
uint8_t bm_app_takes_sel(const bm_app *a);                    /* fuente marcada en TOMAS */
int  bm_app_more_count(const bm_app *a);
void bm_app_more_row(const bm_app *a, int i, const char **name, char *val, int cap);
extern const char *const bm_roll_names[5];

#endif
