/* Bubba Machine — modelo de patrón (SPEC_V3 §4.1, §4.2). */
#ifndef BM_PATTERN_H
#define BM_PATTERN_H

#include "bm_types.h"

/* StepEvent: 8 bytes exactos. */
typedef struct {
    uint8_t note;      /* 0..127; 0xFF = vacío. T1/T2: índice de sonido o slice 0..15 */
    uint8_t vel;       /* 1..127 */
    uint8_t len;       /* bits 0-6 largo en pasos (1..127); bit 7 = TIE/GLIDE */
    uint8_t prob;      /* 0..100 % */
    uint8_t lk[4];     /* P-Locks de las 4 macros; 0xFF = valor de la pista */
} bm_event;

#define BM_EV_EMPTY   0xFFu
#define BM_LOCK_NONE  0xFFu
#define BM_LEN_TIE    0x80u
#define BM_LEN_MASK   0x7Fu

/* Macros de pista = los 4 P-Locks. El nombre que ve el usuario depende del motor. */
enum { MAC_1, MAC_2, MAC_3, MAC_4 };

/* Parámetros de pista: 32 bytes. */
typedef struct {
    uint8_t macro[4];      /* synth: CUTOFF RESO DECAY WAVE · drums/sampler: TONE TUNE DECAY START */
    uint8_t level, pan, send, duck;
    uint8_t source;        /* T1: kit · T2: interpolación · T3-T6: banco de ondas */
    uint8_t attack, sustain, release, env_amt, glide;
    uint8_t interp;        /* 0 = DROP, 1 = HERMITE */
    uint8_t mute;
    uint8_t transpose;     /* 24 = 0 semitonos */
    uint8_t arp_mode, arp_rate, arp_oct, arp_gate;
    uint8_t rsv[11];
} bm_track;

/* Metadatos del patrón: 64 bytes. */
typedef struct {
    uint16_t bpm_x10;      /* 400..2400 */
    uint8_t swing;         /* 50..75 */
    uint8_t length;        /* pasos: 16, 32, 48, 64 */
    uint8_t dly_time;      /* 0 = 1/16, 1 = 1/8, 2 = 3/16, 3 = 1/4 */
    uint8_t dly_fb, dly_mix;
    uint8_t sc_release;    /* release del ducking */
    uint8_t root, scale, voicing, scale_lock;
    uint8_t rsv[52];
} bm_meta;

typedef struct {
    bm_meta  meta;
    bm_track trk[BM_TRACKS];
    bm_event ev[BM_TRACKS][BM_STEPS][BM_EVENTS];
} bm_pattern;

/* Song: lista de patrones con repeticiones. */
typedef struct {
    uint8_t n;
    uint8_t pat[BM_SONG_MAX];
    uint8_t rep[BM_SONG_MAX];
} bm_song;

void bm_event_clear(bm_event *e);
bool bm_event_empty(const bm_event *e);
void bm_track_defaults(bm_track *t, int track);
void bm_pattern_init(bm_pattern *p);
void bm_track_clear_events(bm_pattern *p, int track);
int  bm_pattern_count_events(const bm_pattern *p);

/* Formato disperso (§4.2). Devuelven bytes escritos/leídos, o -1. */
int bm_pattern_pack(const bm_pattern *p, uint8_t *out, int cap);
int bm_pattern_unpack(bm_pattern *p, const uint8_t *in, int len);

/* Escalas y acordes. */
#define BM_SCALES 9
extern const char *const bm_scale_names[BM_SCALES];
int  bm_scale_len(int scale);
int  bm_scale_note(int root, int scale, int degree, int base_oct);   /* nota MIDI del grado */
int  bm_chord_notes(int root, int scale, int degree, int ext, int base_oct, uint8_t out[4]);
void bm_chord_name(int root, int scale, int degree, int ext, char *buf, int cap);
extern const char *const bm_note_names[12];
void bm_note_str(int note, char *buf, int cap);   /* "F#3" */

#endif
