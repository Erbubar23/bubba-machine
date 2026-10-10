/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — contexto de audio (todo lo que corre en la ISR de audio).
 *
 * Hilos (SPEC_V3 §2):
 *   - ISR de audio: bm_audio_render(). Único escritor del patrón activo,
 *     las voces y el estado del secuenciador.
 *   - Bucle principal (UI): solo LEE el patrón para dibujar y manda cambios
 *     por la cola de comandos bm_cmd (SPSC, sin locks).
 *   - DSP -> UI: bm_status (un solo escritor; la UI solo lee).
 */
#ifndef BM_AUDIO_H
#define BM_AUDIO_H

#include "bm_types.h"
#include "bm_pattern.h"

/* ------------------------------------------------------------ comandos */
typedef enum {
    CMD_NONE,
    CMD_NOTE_ON,        /* a=pista b=nota c=vel */
    CMD_NOTE_OFF,       /* a=pista b=nota */
    CMD_SET_EVENT,      /* a=pista synth b=paso c=slot ev */
    CMD_SET_TRSTEP,     /* a=pista TR b=instrumento c=paso i=valor */
    CMD_TR_LOCK,        /* a=pista TR b=instrumento c=paso i=param<<8|val (bit 16 = borrar) */
    CMD_SET_LANE,       /* a=pista TR b=instrumento c=campo(0 len,1 level,2 tune,3 decay) i=valor */
    CMD_LANE_MUTE,      /* a=pista TR i=máscara */
    CMD_SET_TRACK,      /* a=pista b=offset c=valor */
    CMD_SET_META,       /* b=offset c=valor */
    CMD_SET_BPM,        /* i=bpm_x10 */
    CMD_TRACK_BULK,     /* a=pista ptr = bm_tr o bm_event[64][4] según la pista */
    CMD_TRACK_CLEAR,    /* a=pista */
    CMD_PLAY,           /* a: 0 stop, 1 play, 2 reinicio */
    CMD_PUNCH,          /* a=fx (0xFF = ninguno) */
    CMD_FILL,           /* a=0/1 */
    CMD_QUEUE,          /* a=patrón (0xFF cancela) */
    CMD_LOAD,           /* a=patrón: cargar ya (solo en STOP) */
    CMD_SONG,           /* a=0/1 modo song */
    CMD_SCENE,          /* a=escena (0xFF cancela) */
    CMD_CHAIN,          /* a=largo, ptr=uint8_t[8] escenas */
    CMD_SMP_REC,        /* a=0/1 */
    CMD_SMP_MARK,
    CMD_SMP_SRC,        /* ptr=bm_smp_src (de dónde lee T2) */
    CMD_SMP_SAVED,      /* ptr=bm_smp_src: el sample ya está en flash, se libera la arena */
    CMD_MASTER,         /* i=0..1023 */
    CMD_DRIVE,          /* a=0..127 */
    CMD_SYNC,           /* a=modo b=ppqn c=latencia ms */
    CMD_CLOCK,          /* a=0 interno, 1 externo */
    CMD_MIDI,           /* a=byte MIDI entrante */
    CMD_ARP,            /* a=pista b=0/1 (capa ARP) */
    CMD_ROLL,           /* a=pista TR b=instrumento c=0/1 i=ticks entre golpes */
    CMD_REC,            /* a=0/1: grabación en vivo (para el roll) */
    CMD_LOOP,           /* a=pista synth b=acción (1 REC, 2 borrar, 3 on/off, 4 compases=c) */
    CMD_STRUM,          /* a=pista b=n c=vel i=notas empaquetadas (4 x 8 bit) */
    CMD_ALL_OFF,
    CMD_FADE            /* a=0 subir, 1 bajar (antes de escribir flash) */
} bm_cmd_type;

typedef struct {
    uint8_t type, a, b, c;
    union { bm_event ev; int32_t i; const void *ptr; } u;
} bm_cmd;

typedef struct {
    bm_cmd q[BM_CMDQ_LEN];
    volatile uint16_t w, r;
} bm_cmdq;

bool bm_cmd_push(bm_cmdq *q, const bm_cmd *c);
bool bm_cmd_pop(bm_cmdq *q, bm_cmd *c);

/* Fuente del sampler (flash o arena en RAM). */
typedef struct {
    const int8_t *data;
    uint32_t len;
    uint8_t nmarks;
    uint32_t marks[BM_SLICES];
} bm_smp_src;

/* ------------------------------------------------------------ estado DSP -> UI */
typedef struct {
    volatile uint8_t playing;
    volatile uint32_t rtick;            /* ticks desde el último reinicio maestro */
    volatile uint32_t btick;            /* ticks desde PLAY (compases para fills y loopera) */
    volatile uint16_t tn[BM_TRACKS];    /* contador de pasos de cada pista (sin módulo) */
    volatile int8_t step;               /* último paso de la pista T1 */
    volatile uint32_t loops;
    volatile uint8_t trig[BM_TRACKS];
    volatile uint8_t peak[BM_TRACKS];
    volatile uint16_t lane_trig[BM_TR_TRACKS];   /* bits de instrumentos que sonaron (la UI los baja) */
    volatile uint8_t clip;
    volatile uint8_t pattern, queued;
    volatile uint8_t scene, scene_q, chain_n, chain_pos;
    volatile uint8_t song_on, song_pos;
    volatile uint8_t fill;
    volatile uint8_t smp_rec, smp_marks, smp_unsaved;
    volatile uint32_t smp_len;
    volatile uint8_t punch;
    volatile uint8_t ext_ok;
    volatile uint8_t faded;
    volatile uint8_t swap_fail;
    volatile uint32_t free_ticks;
    volatile uint8_t loop_mode[BM_SYN_TRACKS];
    volatile uint16_t loop_pos[BM_SYN_TRACKS];
    volatile uint16_t edits;            /* el audio editó el patrón (roll/loopera grabando) */
} bm_status;

/* ------------------------------------------------------------ voces y motores */
enum { ENV_OFF, ENV_ATK, ENV_DEC, ENV_SUS, ENV_REL };

typedef struct {
    uint8_t active, track, note, vel, stage, gate;
    uint16_t fade;
    uint8_t pend, pend_track, pend_note, pend_vel, pend_legato;
    uint8_t pend_lk[4];
    uint8_t lk[4];
    uint32_t age;
    int32_t env, fenv;
    uint32_t phase, inc, inc_target;
    int32_t ic1, ic2;
    uint32_t pos, step, end;
    uint32_t start;
    int32_t lp;
    uint8_t sound, choke;
} bm_voice;

struct bm_ctx;
typedef struct bm_engine bm_engine;
typedef struct {
    void (*init)(bm_engine *e);
    void (*note_on)(bm_engine *e, uint8_t note, uint8_t vel);
    void (*note_off)(bm_engine *e, uint8_t note);
    void (*set_macro)(bm_engine *e, uint8_t id, uint8_t val);
    bool (*process)(bm_engine *e, int32_t *buf, uint16_t frames);
    void (*voice_start)(bm_engine *e, bm_voice *v, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato);
} bm_engine_vt;

#define BM_MACRO_LOCK   0x80u
#define BM_MACRO_LEGATO 0x40u

struct bm_engine {
    const bm_engine_vt *vt;
    struct bm_ctx *ctx;
    uint8_t track;
    uint8_t lock_next[4];
    uint8_t legato_next;
};

extern const bm_engine_vt bm_eng_drum, bm_eng_sampler, bm_eng_wave;

/* ------------------------------------------------------------ estado por pista */
typedef struct {
    uint8_t on;
    uint8_t held[8], n;
    uint8_t idx, cur, playing;
    uint16_t gate_left;
} bm_arp;

typedef struct { uint8_t lane, vel; uint16_t ticks; } bm_retrig;

typedef struct {
    uint8_t mode;               /* LOOP_* */
    uint32_t start;             /* btick del inicio */
    uint8_t held[8], nheld;     /* notas que sonó la loopera (para soltarlas al cerrar) */
} bm_looper;

typedef struct { uint8_t track, note, vel; uint16_t wait; } bm_strum;

/* ------------------------------------------------------------ contexto */
typedef int (*bm_swap_fn)(void *user, int from, int to, bm_pattern *pat);
typedef void (*bm_midi_out_fn)(void *user, uint8_t byte);

typedef union {
    uint8_t arena[BM_ARENA_LEN];
    struct { int16_t dly[BM_DELAY_LEN]; uint8_t fx[BM_FXBUF_LEN]; } fx;
} bm_audio_mem;

typedef struct bm_ctx {
    bm_pattern *pat;
    bm_song *song;
    bm_cmdq *cmdq;
    bm_status st;
    bm_engine eng[BM_TRACKS];
    bm_voice voice[BM_VOICES];
    uint32_t age;

    /* reloj */
    uint32_t spt_q16, acc_q16, sample_clock;
    uint8_t playing, fill, ext_clock, song_on, rec;
    uint32_t rtick, btick;
    int8_t queued;
    int8_t scene_q;
    uint8_t chain[8], chain_n, chain_pos;
    uint8_t song_pos, song_rep, cur_pat;
    uint32_t rng;
    uint32_t ext_last, ext_budget, ext_seen;
    struct { uint8_t note, tie; uint16_t ticks; } gate[BM_SYN_TRACKS][BM_EVENTS];
    bm_retrig rq[BM_TR_TRACKS][16];
    struct { uint16_t mask; uint8_t rate; } roll[BM_TR_TRACKS];
    bm_looper lp[BM_SYN_TRACKS];
    bm_arp arp[BM_TRACKS];
    bm_strum strum[8];

    /* mezcla y FX */
    int32_t tbuf[BM_TRACKS][BM_BLOCK];
    int32_t sc_env;
    int16_t sc_trig_at;
    uint16_t blk_off;
    bm_audio_mem *mem;
    uint8_t arena_busy;          /* la arena tiene un sample sin guardar: sin delay ni punch con buffer */
    uint32_t dly_w;
    int32_t d_out, d_acc;
    int32_t prev_l, prev_r;
    uint32_t fx_w;
    uint8_t punch_req, punch_cur;
    uint32_t punch_t, punch_w0, punch_rp;
    int32_t pf_ic1[2], pf_ic2[2];
    int32_t sh_l, sh_r, gate_g;
    uint16_t master_target, master_gain;
    uint8_t drive, clip_hold;
    uint8_t sync_mode, sync_ppqn, sync_lat;
    uint32_t sync_wait, sync_left;
    uint8_t fade_req;
    int32_t fade_gain;
    uint32_t peak_acc[BM_TRACKS];

    /* sampler */
    bm_smp_src smp;
    uint32_t smp_base_inc;       /* 16 kHz -> BM_FS en Q16 */
    uint8_t smp_rec;
    uint32_t smp_rec_pos, smp_dec_acc;
    int32_t smp_lp;

    /* callbacks */
    bm_swap_fn swap; void *swap_user;
    bm_midi_out_fn midi_out; void *midi_user;
} bm_ctx;

void bm_audio_init(bm_ctx *c, bm_pattern *pat, bm_song *song, bm_cmdq *q, bm_audio_mem *mem);
void bm_audio_render(bm_ctx *c, int32_t *out_lr, uint32_t out_frames);
void bm_audio_overload(bm_ctx *c);

/* internos */
bm_voice *bm_voice_get(bm_ctx *c, int track, uint8_t note, uint8_t vel, const uint8_t lk[4], uint8_t legato);
void bm_voice_fade(bm_voice *v);
void bm_seq_tick(bm_ctx *c);
void bm_seq_start(bm_ctx *c, int restart);
void bm_seq_stop(bm_ctx *c);
void bm_seq_set_bpm(bm_ctx *c, uint16_t bpm_x10);
void bm_seq_strum_block(bm_ctx *c, uint32_t frames);
void bm_dsp_mix(bm_ctx *c, int32_t *out_lr, uint32_t n);
void bm_dsp_tick(bm_ctx *c);
void bm_live_note(bm_ctx *c, int track, uint8_t note, uint8_t vel, int on);
uint32_t bm_ticks_to_samples(const bm_ctx *c, uint32_t ticks);
int bm_track_tps(const bm_ctx *c, int t);
void bm_loop_cmd(bm_ctx *c, int t, int action, int val);
void bm_scene_cmd(bm_ctx *c, int idx);
void bm_chain_cmd(bm_ctx *c, const uint8_t *chain, int n);
void bm_strum_cmd(bm_ctx *c, int t, const uint8_t *notes, int n, uint8_t vel);

#endif
