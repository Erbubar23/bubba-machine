/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — modelo de patrón (SPEC_V3 §4, DISENO_0.2 §3-§6).
 *
 * Un patrón tiene 6 pistas:
 *   T1 batería y T2 sampler: estilo TR, 16 instrumentos con su línea de 64 pasos
 *                            (un byte por paso) y P-Locks en una lista dispersa.
 *   T3 bajo y T4-T6 synths: pasos con hasta 4 eventos de 8 bytes + loopera.
 * Cada pista tiene su largo y su velocidad; cada instrumento TR, su largo. */
#ifndef BM_PATTERN_H
#define BM_PATTERN_H

#include "bm_types.h"

/* ------------------------------------------------------------ synths: StepEvent de 8 bytes */
typedef struct {
    uint8_t note;      /* 0..127; 0xFF = vacío */
    uint8_t vel;       /* 1..127 */
    uint8_t len;       /* bits 0-6 largo en pasos (1..127); bit 7 = TIE/GLIDE */
    uint8_t prob;      /* 0..100 %, o BM_PROB_FILL / BM_PROB_NOFILL */
    uint8_t lk[4];     /* P-Locks de las 4 macros; 0xFF = valor de la pista */
} bm_event;

#define BM_EV_EMPTY    0xFFu
#define BM_LOCK_NONE   0xFFu
#define BM_LEN_TIE     0x80u
#define BM_LEN_MASK    0x7Fu
#define BM_PROB_FILL   120u    /* solo suena en el fill */
#define BM_PROB_NOFILL 121u    /* no suena en el fill */

/* ------------------------------------------------------------ pistas TR: un byte por paso */
#define ST_ON       0x01u
#define ST_ACC      0x02u
#define ST_RAT_SH   2          /* bits 2-3: repetición x1..x4 */
#define ST_RAT_MASK 0x0Cu
#define ST_FLAM     0x10u
#define ST_COND_SH  5          /* bits 5-6: 0 siempre, 1 solo fill, 2 sin fill */
#define ST_COND_MASK 0x60u
#define ST_LOCK     0x80u      /* el paso tiene P-Locks en la lista */

typedef struct { uint8_t len, level, tune, decay; } bm_lane;      /* 4 bytes */
typedef struct { uint8_t step, lane, param, val; } bm_lock;       /* 4 bytes; lane 0xFF = libre */

typedef struct {
    bm_lane lane[BM_LANES];
    uint16_t mute;                       /* bit = instrumento muteado */
    uint8_t rsv[2];
    uint8_t st[BM_LANES][BM_STEPS];
    bm_lock lk[BM_LANE_LOCKS];
} bm_tr;

/* ------------------------------------------------------------ loopera de los synths */
typedef struct { uint16_t tick; uint8_t note, vel; } bm_loopev;   /* vel 0 = note off */
enum { LOOP_OFF, LOOP_ARMED, LOOP_REC, LOOP_PLAY, LOOP_DUB };
typedef struct {
    uint8_t on, bars, n, rsv;
    bm_loopev ev[BM_LOOP_EVENTS];
} bm_loop;

/* ------------------------------------------------------------ parámetros de pista: 32 bytes */
enum { MAC_1, MAC_2, MAC_3, MAC_4 };
enum { SPD_X2, SPD_X3_2, SPD_X1, SPD_X3_4, SPD_X1_2, SPD_X1_4, SPD_COUNT };
extern const uint8_t bm_speed_tps[SPD_COUNT];       /* ticks por paso */
extern const char *const bm_speed_names[SPD_COUNT];

typedef struct {
    uint8_t macro[4];      /* synth: CUTOFF RESO DECAY WAVE · TR: TONE TUNE DECAY START */
    uint8_t level, pan, send, duck;
    uint8_t source;        /* T1: kit · T2: slot de sample · T3-T6: banco de ondas */
    uint8_t attack, sustain, release, env_amt, glide;
    uint8_t interp;        /* 0 = DROP, 1 = HERMITE */
    uint8_t mute;
    uint8_t transpose;     /* 24 = 0 semitonos */
    uint8_t arp_mode, arp_rate, arp_oct, arp_gate;
    uint8_t speed;         /* SPD_* */
    uint8_t length;        /* synths: 1..64 pasos (TR: largo por instrumento) */
    uint8_t rsv[9];
} bm_track;

/* ------------------------------------------------------------ metadatos: 64 bytes */
typedef struct {
    uint16_t bpm_x10;      /* 400..2400 */
    uint8_t swing;         /* 50..75 */
    uint8_t master;        /* largo maestro en compases de 16 pasos: 1,2,3,4,8; 0 = infinito */
    uint8_t dly_time;      /* 0 = 1/16, 1 = 1/8, 2 = 3/16, 3 = 1/4 */
    uint8_t dly_fb, dly_mix;
    uint8_t sc_release;
    uint8_t root, scale, voicing, scale_lock;
    uint8_t fill_every;    /* 0, 2, 4, 8 compases */
    uint8_t chord_set, chord_voicing, chord_oct, strum;
    uint8_t rsv[47];
} bm_meta;

typedef struct {
    bm_meta  meta;
    bm_track trk[BM_TRACKS];
    bm_tr    tr[BM_TR_TRACKS];                       /* T1, T2 */
    bm_event ev[BM_SYN_TRACKS][BM_STEPS][BM_EVENTS]; /* T3..T6 */
    bm_loop  loop[BM_SYN_TRACKS];
} bm_pattern;

/* Escenas y canción. Una escena = patrón + mutes. */
typedef struct { uint8_t valid, pattern, trk_mute, rsv; uint16_t lane_mute[BM_TR_TRACKS]; } bm_scene;
typedef struct {
    uint8_t n;
    uint8_t pat[BM_SONG_MAX];      /* 0..15 patrón · 0x80|s escena */
    uint8_t rep[BM_SONG_MAX];
    bm_scene scene[BM_SCENES];
} bm_song;

static inline bool bm_is_tr(int t) { return t < BM_TR_TRACKS; }

void bm_event_clear(bm_event *e);
bool bm_event_empty(const bm_event *e);
void bm_track_defaults(bm_track *t, int track);
void bm_tr_defaults(bm_tr *tr);
void bm_pattern_init(bm_pattern *p);
void bm_track_clear_events(bm_pattern *p, int track);
int  bm_pattern_count_events(const bm_pattern *p);
int  bm_master_steps(const bm_meta *m);       /* 0 = infinito */

/* Formato disperso en flash. Devuelven bytes, o -1 si no cabe / es inválido. */
int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap);
int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len);

/* P-Locks de las pistas TR */
int  bm_tr_lock_get(const bm_tr *tr, int step, int lane, int param);   /* -1 si no hay */
bool bm_tr_lock_set(bm_tr *tr, int step, int lane, int param, int val); /* val < 0 borra */
void bm_tr_lock_clear_step(bm_tr *tr, int step, int lane);

/* Escalas y acordes */
#define BM_SCALES 9
extern const char *const bm_scale_names[BM_SCALES];
int  bm_scale_len(int scale);
int  bm_scale_note(int root, int scale, int degree, int base_oct);
int  bm_chord_notes(int root, int scale, int degree, int ext, int base_oct, uint8_t out[4]);
void bm_chord_name(int root, int scale, int degree, int ext, char *buf, int cap);
extern const char *const bm_note_names[12];
void bm_note_str(int note, char *buf, int cap);

/* Cuadrícula de acordes (estilo Maschine): 16 pads por juego */
#define BM_CHORD_SETS 3
extern const char *const bm_chord_set_names[BM_CHORD_SETS];
typedef struct { uint8_t degree, ext; } bm_pad_chord;
bm_pad_chord bm_chord_pad(int set, int pad);
int  bm_chord_function(int degree);           /* 0 reposo, 1 preparación, 2 tensión */
void bm_roman(int root, int scale, int degree, int ext, char *buf, int cap);

#endif
