/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — modelo de patrón (DISENO_3.0).
 *
 * Un patrón tiene 6 canales: 3 fijos y 3 libres.
 *   T1 batería: estilo TR, 16 instrumentos con su línea de 64 pasos (un byte por paso)
 *               y P-Locks en una lista dispersa.
 *   T2 sampler: un loop en 8 slices que un generador rearma. Usa la misma rejilla TR:
 *               la línea 0 son los pasos del generador y las líneas 1-8, los slices fijos.
 *   T3 bajo:    pasos con hasta 4 notas de 8 bytes + loopera.
 *   T4-T6:      libres. SYNTH, MONO, MIDI (notas, como T3) o KIT (rejilla TR, como T1).
 * Cada canal tiene su largo y su velocidad; cada instrumento TR, su largo. */
#ifndef BM_PATTERN_H
#define BM_PATTERN_H

#include "bm_types.h"

/* ------------------------------------------------------------ notas: StepEvent de 8 bytes */
typedef struct {
    uint8_t note;      /* 0..127; 0xFF = vacío */
    uint8_t vel;       /* 1..127: BM_VEL_GHOST, BM_VEL_NORMAL o BM_VEL_ACC */
    uint8_t len;       /* bits 0-6 largo en pasos (1..127); bit 7 = TIE/GLIDE */
    uint8_t prob;      /* 0..100 %, o BM_PROB_FILL / BM_PROB_NOFILL */
    uint8_t lk[4];     /* P-Locks de las 4 macros; 0xFF = valor del canal */
} bm_event;

#define BM_EV_EMPTY    0xFFu
#define BM_LOCK_NONE   0xFFu
#define BM_LEN_TIE     0x80u
#define BM_LEN_MASK    0x7Fu
#define BM_PROB_FILL   120u    /* solo suena en el fill */
#define BM_PROB_NOFILL 121u    /* no suena en el fill */
#define BM_VEL_GHOST   50u
#define BM_VEL_NORMAL  100u
#define BM_VEL_ACC     127u

/* ------------------------------------------------------------ rejilla TR: un byte por paso */
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

/* Sampler: línea 0 = el generador elige; líneas 1..8 = ese slice, fijo. */
#define BRK_GEN     0

/* ------------------------------------------------------------ loopera de los canales de notas */
typedef struct { uint16_t tick; uint8_t note, vel; } bm_loopev;   /* vel 0 = note off */
enum { LOOP_OFF, LOOP_ARMED, LOOP_REC, LOOP_PLAY, LOOP_DUB };
typedef struct {
    uint8_t on, bars, n, rsv;
    bm_loopev ev[BM_LOOP_EVENTS];
} bm_loop;

/* ------------------------------------------------------------ motores y tipos de canal */
enum { ENG_DRUM, ENG_BREAK, ENG_BASS, ENG_SYNTH, ENG_MONO, ENG_KIT, ENG_MIDI, ENG_ACID, ENG_VA, ENG_COUNT };
enum { KIND_TR, KIND_BRK, KIND_NOTE };
extern const char *const bm_engine_names[ENG_COUNT];
#define BM_FREE0   3           /* T4: primer canal libre */

/* ------------------------------------------------------------ parámetros de canal: 48 bytes */
enum { MAC_1, MAC_2, MAC_3, MAC_4 };
enum { SPD_X2, SPD_X3_2, SPD_X1, SPD_X3_4, SPD_X1_2, SPD_X1_4, SPD_COUNT };
extern const uint8_t bm_speed_tps[SPD_COUNT];       /* ticks por paso */
extern const char *const bm_speed_names[SPD_COUNT];

#define BM_SRC_NONE  0xFFu     /* fuente del sampler: 0..7 toma, 0x80|i loop de fábrica */
#define BM_SRC_ROM   0x80u

typedef struct {
    uint8_t macro[4];      /* notas: CUTOFF RESO DECAY WAVE · batería: TONE TUNE DECAY START */
    uint8_t level, pan, send, duck;
    uint8_t source;        /* batería: kit · sampler: loop A · notas: banco de ondas */
    uint8_t attack, sustain, release, env_amt, glide;
    uint8_t interp;        /* 0 = lineal, 1 = HERMITE */
    uint8_t mute;
    uint8_t transpose;     /* 24 = 0 semitonos */
    uint8_t arp_mode, arp_rate, arp_oct, arp_gate;
    uint8_t speed;         /* SPD_* */
    uint8_t length;        /* notas: 1..64 pasos (TR: largo por instrumento) */
    uint8_t engine;        /* ENG_*: fijo en T1-T3, elegible en T4-T6 */
    uint8_t accent;        /* cuánto más fuerte suena un acento (0 = igual) */
    uint8_t midi_ch;       /* MIDI: canal 0..15 */
    uint8_t scl_root, scl; /* escala propia del canal; scl 0xFF = la del proyecto */
    uint8_t gen[4];        /* sampler: VARIAR ANCLAR REPETIR ROMPER (0..127) */
    uint8_t rt[4];         /* sampler: peso de los retrig 2X 3X 4X 8X */
    uint8_t loop_b, chance_b, pitch;   /* sampler: loop B, % de compases con B, tono (24 = 0) */
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
    uint8_t fmt;           /* BM_FMT */
    uint8_t transpose;     /* tono de todo el proyecto, 24 = 0 (capa TONO de GLO) */
    uint8_t rsv[45];
} bm_meta;

/* T3-T6: notas, o con el motor KIT, una rejilla TR. */
typedef union { bm_event ev[BM_STEPS][BM_EVENTS]; bm_tr tr; } bm_notes;

typedef struct {
    bm_meta  meta;
    bm_track trk[BM_TRACKS];
    bm_tr    tr[BM_TR_TRACKS];                       /* T1, T2 */
    bm_notes nt[BM_SYN_TRACKS];                      /* T3..T6 */
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

/* ------------------------------------------------------------ acceso según el tipo de canal */
static inline int bm_kind(const bm_pattern *p, int t)
{
    if (t == 1) return KIND_BRK;
    if (t == 0 || p->trk[t].engine == ENG_KIT) return KIND_TR;
    return KIND_NOTE;
}
static inline bool bm_is_tr(const bm_pattern *p, int t) { return bm_kind(p, t) == KIND_TR; }
static inline bool bm_is_grid(const bm_pattern *p, int t) { return bm_kind(p, t) != KIND_NOTE; }   /* TR o sampler */
static inline bool bm_is_free(int t) { return t >= BM_FREE0; }
static inline bm_tr *bm_trk_tr(bm_pattern *p, int t) { return t < BM_TR_TRACKS ? &p->tr[t] : &p->nt[t - BM_TR_TRACKS].tr; }
static inline const bm_tr *bm_trk_trc(const bm_pattern *p, int t) { return t < BM_TR_TRACKS ? &p->tr[t] : &p->nt[t - BM_TR_TRACKS].tr; }
static inline bm_event *bm_ev(bm_pattern *p, int t, int s, int k) { return &p->nt[t - BM_TR_TRACKS].ev[s][k]; }
static inline const bm_event *bm_evc(const bm_pattern *p, int t, int s, int k) { return &p->nt[t - BM_TR_TRACKS].ev[s][k]; }
/* Largo del canal: notas = length; rejilla = largo de la línea (el sampler usa la 0). */
int  bm_trk_len(const bm_pattern *p, int t, int lane);

void bm_event_clear(bm_event *e);
bool bm_event_empty(const bm_event *e);
void bm_track_defaults(bm_track *t, int track);
void bm_tr_defaults(bm_tr *tr);
void bm_pattern_init(bm_pattern *p);
void bm_track_clear_events(bm_pattern *p, int track);
bool bm_engine_ok(int track, int engine);       /* T3: BAJO o ACID · T4-T6: cualquiera de notas o KIT */
void bm_track_set_engine(bm_pattern *p, int track, int engine);   /* entre notas y KIT, el canal empieza vacío */
int  bm_pattern_count_events(const bm_pattern *p);
int  bm_master_steps(const bm_meta *m);       /* 0 = infinito */

/* Formato disperso en flash. Devuelven bytes, o -1 si no cabe / es inválido o de otra versión. */
int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap);
int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len);

/* P-Locks de las rejillas TR */
int  bm_tr_lock_get(const bm_tr *tr, int step, int lane, int param);   /* -1 si no hay */
bool bm_tr_lock_set(bm_tr *tr, int step, int lane, int param, int val); /* val < 0 borra */
void bm_tr_lock_clear_step(bm_tr *tr, int step, int lane);

/* Sampler: qué hay en el paso s de la fila TU. Devuelve -1 silencio, 0 generador o 1..8 slice fijo;
 * en *b deja el byte del paso (acento, repetición...). */
int  bm_brk_step(const bm_tr *tr, int s, uint8_t *b);

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
