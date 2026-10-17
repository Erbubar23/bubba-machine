/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — modelo de patrón (DISENO_3.0).
 *
 * Un patrón tiene 6 canales y cada uno es una máquina fija (docs/DISENO_0.6.md):
 *   T1 BR-8:    batería estilo TR, 16 instrumentos con su línea de 64 pasos (un byte por paso)
 *               y P-Locks en una lista dispersa.
 *   T2 BP-1:    16 pads (trozos de las tomas, como un KO II o un OP-1 field) en una rejilla TR
 *               como la de T1: una línea por pad. La página BREAK varía lo que suena.
 *   T3-T6: una máquina de notas cada uno (de fábrica BB-303, BX-7, BITTAL, BUNO-6; se elige en
 *               MACHINE): pasos con hasta 4 notas de 8 bytes + loopera.
 * Cada canal tiene su largo y su velocidad; cada instrumento TR, su largo. */
#ifndef BM_PATTERN_H
#define BM_PATTERN_H

#include "bm_types.h"

/* ------------------------------------------------------------ notas: StepEvent de 10 bytes */
typedef struct {
    uint8_t note;      /* 0..127; 0xFF = vacío */
    uint8_t vel;       /* 1..127: BM_VEL_GHOST, BM_VEL_NORMAL o BM_VEL_ACC */
    uint8_t len;       /* bits 0-6 largo en pasos (1..127); bit 7 = TIE/GLIDE */
    uint8_t prob;      /* 0..100 %, o BM_PROB_FILL / BM_PROB_NOFILL */
    uint8_t lk[4];     /* P-Locks de las 4 macros; 0xFF = valor del canal */
    uint8_t gate;      /* REC expresivo: 0 = el último paso entero; 10..99 = % del último paso que suena */
    int8_t micro;      /* REC expresivo: ticks antes (-) o después (+) del paso; -11..11 a 96 PPQN */
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

/* Un instrumento de la rejilla. Batería: modelo y su golpe (core/bm_drums.h, core/bm_params.h). */
typedef struct { uint8_t len, level, tune, decay, model, tone, color, choke; } bm_lane;   /* 8 bytes */
typedef struct { uint8_t step, lane, param, val; } bm_lock;       /* 4 bytes; lane 0xFF = libre */

typedef struct {
    bm_lane lane[BM_LANES];
    uint16_t mute;                       /* bit = instrumento muteado */
    uint8_t rsv[2];
    uint8_t st[BM_LANES][BM_STEPS];
    bm_lock lk[BM_LANE_LOCKS];
} bm_tr;

/* T2: un pad del sampler = un trozo de una toma guardada. El nivel y el grupo de corte del pad son
 * los de su línea (bm_lane.level, bm_lane.choke). */
enum { PAD_ONESHOT, PAD_KEY, PAD_THRU, PAD_LOOP, PAD_MODES };
#define PAD_REV     0x01u      /* flags: suena al revés */
#define PAD_TEMPO   0x02u      /* sigue el tempo: velocidad = BPM del proyecto / BPM de la toma */
typedef struct {
    uint8_t src;               /* toma 0..7; BM_SRC_NONE = pad vacío */
    uint8_t mode;              /* PAD_*: cómo responde a la tecla sostenida */
    uint8_t pitch;             /* 24 = 0 semitonos (0..48) */
    uint8_t flags;
    uint8_t attack, release;   /* 0..127 */
    uint16_t start, len;       /* muestras dentro de la toma */
    uint8_t loop_s, loop_e;    /* LOOP: el tramo que se repite al sostener, en 1/255 del pad (loop_e 0 = el pad entero) */
} bm_pad;                      /* 12 bytes */

/* ------------------------------------------------------------ loopera de los canales de notas */
typedef struct { uint16_t tick; uint8_t note, vel; } bm_loopev;   /* vel 0 = note off */
enum { LOOP_OFF, LOOP_ARMED, LOOP_REC, LOOP_PLAY, LOOP_DUB };
typedef struct {
    uint8_t on, bars, n, rsv;
    bm_loopev ev[BM_LOOP_EVENTS];
} bm_loop;

/* ------------------------------------------------------------ motores y tipos de canal */
/* Cada canal es una máquina. T3-T6 eligen la suya en la página MACHINE (por patrón). */
enum { ENG_DRUM, ENG_BREAK, ENG_ACID, ENG_MINI, ENG_FM, ENG_BITTAL, ENG_BUNO, ENG_COUNT };
enum { KIND_TR, KIND_NOTE };
extern const char *const bm_engine_names[ENG_COUNT];     /* nombres de las máquinas: BR-8, BP-1, BB-303... */

/* ------------------------------------------------------------ parámetros de canal: 48 bytes */
enum { MAC_1, MAC_2, MAC_3, MAC_4 };
enum { SPD_X2, SPD_X3_2, SPD_X1, SPD_X3_4, SPD_X1_2, SPD_X1_4, SPD_COUNT };
extern const uint8_t bm_speed_tps[SPD_COUNT];       /* ticks por paso */
extern const char *const bm_speed_names[SPD_COUNT];

#define BM_SRC_NONE  0xFFu     /* fuente del sampler: 0..7 toma, 0x80|i loop de fábrica */
#define BM_SRC_ROM   0x80u

typedef struct {
    uint8_t macro[4];      /* notas: las de la máquina (bm_engine_macro) · batería: TONE TUNE DECAY COLOR · pads: FILTER PITCH DECAY START */
    uint8_t level, pan, send, duck;
    uint8_t source;        /* batería: kit · sampler: loop A · notas: banco de ondas · FM: patch */
    uint8_t attack, sustain, release, env_amt, glide;
    uint8_t interp;        /* 0 = lineal, 1 = HERMITE */
    uint8_t mute;
    uint8_t transpose;     /* 24 = 0 semitonos */
    uint8_t arp_mode, arp_rate, arp_oct, arp_gate;
    uint8_t speed;         /* SPD_* */
    uint8_t length;        /* notas: 1..64 pasos (TR: largo por instrumento) */
    uint8_t engine;        /* ENG_*: T1 y T2 fijos; T3-T6 una de las 5 máquinas de notas */
    uint8_t accent;        /* cuánto más fuerte suena un acento (0 = igual) */
    uint8_t midi_ch;       /* MIDI: canal 0..15 */
    uint8_t scl_root, scl; /* escala propia del canal; scl 0xFF = la del proyecto */
    uint8_t gen[4];        /* sampler, BREAK: VARY ANCHOR REPEAT BREAK (0..127; todo en 0 = la rejilla tal cual) */
    uint8_t rt[4];         /* sampler, BREAK: peso de los retrig 2X 3X 4X 8X */
    uint8_t loop_b, chance_b, pitch;   /* sin uso desde los pads */
    uint8_t rsend;         /* envío a la reverb (el delay es send) */
    uint8_t midi_out;      /* 1 = además manda sus notas por MIDI (canal midi_ch) */
    uint8_t djf;           /* filtro DJ del canal (K2 de HOME): 0 abierto, 1-63 pasa-bajos, 65-127 pasa-altos */
    uint8_t rsv[6];
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
    uint8_t rv_size, rv_damp, rv_mix;  /* reverb del master */
    uint8_t mfilt;         /* filtro del master: 64 = abierto, menos = pasa-bajos, más = pasa-altos */
    uint8_t beatfx;        /* BEAT FX de HOME (K4): 0 = apagado, 1..16 un efecto de PUNCH sostenido */
    uint8_t rsv[40];
} bm_meta;

/* T3-T6: notas (la unión con la rejilla deja copiar canales enteros con el mismo buffer). */
typedef union { bm_event ev[BM_STEPS][BM_EVENTS]; bm_tr tr; } bm_notes;

typedef struct {
    bm_meta  meta;
    bm_track trk[BM_TRACKS];
    bm_tr    tr[BM_TR_TRACKS];                       /* T1, T2 (una línea por pad) */
    bm_pad   pad[BM_LANES];                          /* T2: los 16 pads */
    bm_notes nt[BM_SYN_TRACKS];                      /* T3..T6 */
    bm_loop  loop[BM_SYN_TRACKS];
    uint8_t  prm[BM_TRACKS][BM_PRM];                 /* parámetros del motor de cada canal (bm_params.h) */
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
    (void)p;
    return t <= 1 ? KIND_TR : KIND_NOTE;
}
static inline bool bm_is_pads(int t) { return t == 1; }                                         /* T2: rejilla de pads */
static inline bool bm_is_tr(const bm_pattern *p, int t) { return bm_kind(p, t) == KIND_TR && !bm_is_pads(t); }   /* batería */
static inline bool bm_is_grid(const bm_pattern *p, int t) { return bm_kind(p, t) == KIND_TR; }   /* batería o pads */
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
bool bm_engine_ok(int track, int engine);       /* T1 BR-8, T2 BP-1; T3-T6 cualquiera de las 5 máquinas de notas */
int  bm_track_engine(int track);                 /* la máquina de fábrica del canal */
/* BUNO-6 (~32 % de CPU, un solo chorus) y BITTAL (sus tablas en RAM): una sola vez por patrón */
static inline bool bm_engine_heavy(int engine) { return engine == ENG_BUNO || engine == ENG_BITTAL; }
/* el otro canal que ya tiene esa máquina pesada (-1 si no hay o si no es pesada) */
int  bm_machine_conflict(const bm_pattern *p, int track, int engine);
/* T3-T6: cambia de máquina. Las notas, el largo y la mezcla quedan; el sonido pasa al primer preset
 * de la máquina y se borran los P-Locks de las notas. No hace nada si choca con una pesada. */
void bm_track_set_engine(bm_pattern *p, int track, int engine);
int  bm_pattern_count_events(const bm_pattern *p);
int  bm_master_steps(const bm_meta *m);       /* 0 = infinito */

/* Formato disperso en flash. Devuelven bytes, o -1 si no cabe / es inválido o de otra versión. */
int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap);
int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len);

/* P-Locks de las rejillas TR */
int  bm_tr_lock_get(const bm_tr *tr, int step, int lane, int param);   /* -1 si no hay */
bool bm_tr_lock_set(bm_tr *tr, int step, int lane, int param, int val); /* val < 0 borra */
void bm_tr_lock_clear_step(bm_tr *tr, int step, int lane);

/* Pads: vacía uno · ¿suena? (tiene toma y largo) */
void bm_pad_clear(bm_pad *p);
static inline bool bm_pad_ok(const bm_pad *p) { return p->src < 8 && p->len > 0; }
/* el loop de sustain (como el OP-1): sostenido repite [inicio, fin) y al soltar sigue hasta el final */
static inline bool bm_pad_has_loop(const bm_pad *p) { return p->loop_e > p->loop_s; }
static inline uint32_t bm_pad_loop_start(const bm_pad *p) { return (uint32_t)p->len * p->loop_s / 255u; }
static inline uint32_t bm_pad_loop_end(const bm_pad *p) { return bm_pad_has_loop(p) ? (uint32_t)p->len * p->loop_e / 255u : p->len; }

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
