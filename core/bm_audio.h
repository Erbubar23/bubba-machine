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
    CMD_SET_TRSTEP,     /* a=canal de rejilla b=línea c=paso i=valor */
    CMD_TR_LOCK,        /* a=canal de rejilla b=línea c=paso i=param<<8|val (bit 16 = borrar) */
    CMD_SET_LANE,       /* a=canal de rejilla b=línea c=campo LN_* (bm_params.h) i=valor */
    CMD_LANE_MUTE,      /* a=canal de rejilla i=máscara */
    CMD_SET_TRACK,      /* a=pista b=offset c=valor */
    CMD_SET_META,       /* b=offset c=valor */
    CMD_SET_BPM,        /* i=bpm_x10 */
    CMD_TRACK_BULK,     /* a=canal ptr = bm_tr (T1, T2) o bm_notes (T3-T6) */
    CMD_TRACK_CLEAR,    /* a=pista */
    CMD_PLAY,           /* a: 0 stop, 1 play, 2 reinicio */
    CMD_PUNCH,          /* a=fx (0xFF = ninguno) */
    CMD_FILL,           /* a=0/1 */
    CMD_QUEUE,          /* a=patrón (0xFF cancela) */
    CMD_LOAD,           /* a=patrón: cargar ya (solo en STOP) */
    CMD_SONG,           /* a=0/1 modo song */
    CMD_SCENE,          /* a=escena (0xFF cancela) */
    CMD_CHAIN,          /* a=largo, ptr=uint8_t[8] escenas */
    CMD_SMP_REC,        /* a: 0 parar, 1 grabar ya, 2 armar (empieza en el compás) · i=tope en muestras */
    CMD_SMP_TAKES,      /* ptr=bm_smp_src[BM_TAKES]: las tomas guardadas que leen los pads */
    CMD_SMP_SAVED,      /* ptr=bm_smp_src: la toma nueva ya está en flash, se libera la arena */
    CMD_SMP_DISCARD,    /* descarta la toma sin guardar y vuelve al loop A del patrón (ptr) */
    CMD_BRK_LIVE,       /* a=bits: 1 reversa, 2 media velocidad, 4 stutter (negras 9-11 del sampler) */
    CMD_SMP_PREVIEW,    /* a=1 escuchar la toma b (BM_TAKE_NEW = la nueva) desde i&0xFFFF, i>>16 muestras · a=0 soltar */
    CMD_ENGINE,         /* a=canal libre b=motor (borra sus notas) */
    CMD_PCHAIN,         /* a=largo, ptr=uint8_t[8] patrones (dos blancas en HOME) */
    CMD_MIDI_SEND,      /* a,b,c = bytes MIDI de salida (b o c = 0xFF: no se mandan) */
    CMD_MASTER,         /* i=0..1023 */
    CMD_DRIVE,          /* a=0..127 */
    CMD_MOVE,           /* a = MOVE de HOME, con signo (-63..63; 0 = la mezcla tal cual) */
    CMD_SYNC,           /* a=modo b=ppqn c=latencia ms */
    CMD_CLOCK,          /* a=0 interno, 1 externo */
    CMD_MIDI,           /* a=byte MIDI entrante */
    CMD_ARP,            /* a=pista b=0/1 (capa ARP) */
    CMD_ROLL,           /* a=canal de rejilla b=línea c=0/1 (0: sigue la cola) i=ticks entre golpes (0 = los mismos) */
    CMD_REC,            /* a=0/1: grabación en vivo (para el roll) */
    CMD_LOOP,           /* a=pista synth b=acción (1 REC, 2 borrar, 3 on/off, 4 compases=c) */
    CMD_STRUM,          /* a=pista b=n c=vel i=notas empaquetadas (4 x 8 bit) */
    CMD_ALL_OFF,
    CMD_FADE,           /* a=0 subir, 1 bajar (antes de escribir flash) */
    CMD_SET_PRM,        /* a=canal b=parámetro (bm_pattern.prm) c=valor */
    CMD_PRESET,         /* a=canal b=preset de su motor (ANALOG) o kit (rejilla de batería) */
    CMD_SMP_IMPORT,     /* toma desde afuera (consola): a=0 prepara la arena · a=1 lista: i=muestras, b<<8|c = bpm_x10 */
    CMD_PAD_KEY,        /* tecla de un pad: a=1 suena el pad b (vel c) mientras se sostiene · a=0 soltar */
    CMD_ROLL_CFG        /* ROLL: a=rampa (0 no, 1 CRESC, 2 DECRESC) b=swing 50..75 c=cola en ticks i=largo de la rampa */
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

/* Fuente del sampler (flash, ROM o arena en RAM): un loop en hasta 8 slices. */
typedef struct {
    const int8_t *data;
    uint32_t len;
    uint8_t nmarks;             /* 0 = 8 slices iguales */
    uint8_t kslice, sslice;     /* slices con el bombo y la caja (para ANCLAR); 0xFF = no hay */
    uint8_t gain;               /* 64 = 0 dB */
    uint16_t bpm_x10;           /* tempo al que se grabó (0 = sin sync) */
    uint8_t bars;
    uint32_t marks[BM_SLICES];
} bm_smp_src;

/* Lo que eligió el generador del sampler para cada paso del compás que suena. */
#define GEN_RT_MASK 0x07u       /* 0 nada, 1..4 retrig 2X 3X 4X 8X */
#define GEN_REV     0x08u
#define GEN_HALF    0x10u


#define GEN_SKIP    0x80u       /* ROMPER: silencio */

/* Nota del motor del sampler: el pad (bits 0-3), reversa y media velocidad. */
#define BRK_SL_MASK 0x0Fu
#define BRK_REV     0x40u
#define BRK_HALF    0x80u
#define BM_TAKE_NEW 8u          /* la toma recién grabada, todavía sin guardar (smp[0]) */

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
    volatile uint16_t lane_trig[BM_TRACKS];      /* bits de instrumentos/slices que sonaron (la UI los baja) */
    volatile uint8_t clip;
    volatile uint8_t pattern, queued;
    volatile uint8_t scene, scene_q, chain_n, chain_pos;
    volatile uint8_t song_on, song_pos;
    volatile uint8_t fill;
    volatile uint8_t smp_rec, smp_armed, smp_unsaved;
    volatile uint32_t smp_len, smp_cap;
    volatile uint8_t gen_sl[16], gen_fx[16];   /* BREAK: el pad que suena en cada paso del compás (0xFF nada) y su efecto */
    volatile uint8_t gen_blk;                  /* bloque de 16 pasos */
    volatile int8_t brk_slice;                 /* último pad que sonó */
    volatile int8_t smp_pos_src;               /* cabezal del sampler: fuente que suena (-1 nada) */
    volatile uint32_t smp_pos;                 /* y la muestra donde va */
    volatile uint8_t pchain_n, pchain_pos;
    volatile uint8_t punch;
    volatile uint8_t ext_ok;
    volatile uint8_t faded;
    volatile uint8_t swap_fail;
    volatile uint32_t free_ticks;
    volatile uint8_t loop_mode[BM_SYN_TRACKS];
    volatile uint16_t loop_pos[BM_SYN_TRACKS];
    volatile uint16_t edits;            /* el audio editó el patrón (roll/loopera grabando) */
    volatile uint8_t roll_vel[BM_TR_TRACKS];   /* velocidad del último golpe del roll */
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
    uint8_t dir, src, gain;      /* sampler: 1 = reversa, fuente (0 A, 1 B, 2 escucha), ganancia de la toma */
    uint8_t loopv;               /* sampler: 1 = el loop entero en bucle (paso sostenido) · 2 = corte sostenido */
    uint32_t ph[3];              /* ANALOG y SINTE: más osciladores */
    int32_t z[4];                /* ACID: el filtro de 4 polos · SINTE: filtros y envolventes extra */
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

extern const bm_engine_vt bm_eng_drum, bm_eng_sampler, bm_eng_wave, bm_eng_acid, bm_eng_fm, bm_eng_midi_out;
/* FM: lo que hacen las macros sobre el patch (para la pantalla) */
int bm_fm_alg_of(const bm_track *t, int m4);          /* algoritmo 1..32 */
int bm_fm_fb_of(const bm_track *t, int m2);           /* realimentación 0..7 */
void bm_fm_patch_of(const bm_track *t, const bm_voice *v, uint8_t *p);   /* patch con las macros (v NULL = las del canal) */
extern const char *const bm_acid_names[2];             /* sierra o cuadrada */
const bm_engine_vt *bm_engine_vt_for(int engine);

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
    struct { int16_t dly[BM_DELAY_LEN]; int16_t fx[BM_FXBUF_LEN]; } fx;
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
    bm_retrig rq[BM_TRACKS][16];
    /* ROLL de las rejillas (T1, T2): mask = instrumentos sostenidos, tail_m = sonando la cola tras
     * soltarlos; age = ticks desde que empezó cada uno (la rampa de CRESC / DECRESC) */
    struct { uint16_t mask, tail_m; uint8_t rate; uint8_t tail[BM_LANES]; uint16_t age[BM_LANES]; } roll[BM_TR_TRACKS];
    struct { uint8_t ramp, swing, tail; uint16_t ramp_len; } roll_cfg;
    bm_looper lp[BM_SYN_TRACKS];
    bm_arp arp[BM_TRACKS];
    bm_strum strum[8];
    struct { uint8_t t, s, k, fill; uint8_t wait; } dq[16];   /* notas corridas en el tiempo (micro) */
    uint8_t dq_n;

    /* mezcla y FX (bus en coma flotante, 1.0 = 32768 de los motores) */
    int32_t tbuf[BM_TRACKS][BM_BLOCK];
    uint8_t tact;                /* canales que sonaron en el bloque (bit t) */
    float sc_env;
    int16_t sc_trig_at;
    uint16_t blk_off;
    bm_audio_mem *mem;
    uint8_t arena_busy;          /* la arena tiene un sample sin guardar: sin delay ni punch con buffer */
    uint32_t dly_w;
    float d_acc, d_prev, d_cur;  /* delay a Fs/2: suma de dos muestras y las dos últimas leídas */
    uint32_t fx_w;
    uint8_t punch_req, punch_cur;
    uint32_t punch_t, punch_w0, punch_rp;
    float pf_ic1[2], pf_ic2[2];
    float mf_ic1[2], mf_ic2[2];  /* filtro del master */
    float dj_ic1[BM_TRACKS], dj_ic2[BM_TRACKS];   /* el filtro DJ de cada canal (K2 de HOME) */
    int8_t move;                 /* MOVE (PRESETS en HOME): < 0 oscuro y con eco, > 0 brillante y con reverb */
    float sh_l, sh_r, gate_g;
    uint16_t master_target, master_gain;
    uint8_t drive, clip_hold;
    uint8_t sync_mode, sync_ppqn, sync_lat;
    uint32_t sync_wait, sync_left;
    uint8_t fade_req;
    float fade_gain;
    uint32_t peak_acc[BM_TRACKS];

    /* sampler */
    bm_smp_src smp[1];           /* la toma recién grabada (en la arena, hasta que se guarda) */
    bm_smp_src takes[BM_TAKES];  /* las tomas guardadas (flash XIP), para los pads */
    bm_pad preview_pad;          /* lo que suena al escuchar una toma (TAKES, grabadora) */
    uint32_t smp_base_inc;       /* 16 kHz -> BM_FS en Q16 */
    uint8_t smp_rec, smp_armed;
    uint32_t smp_rec_pos, smp_dec_acc, smp_cap;
    float smp_z[4];              /* pasa-bajos antes de diezmar la grabación (biquad) */
    uint8_t brk_live;            /* negras 9-11 de PADS: reversa, media velocidad, stutter */
    uint8_t brk_last;            /* última nota del sampler (la repite el stutter) */
    uint8_t gen_sl[16], gen_fx[16];   /* BREAK: pad que reemplaza (0xFF = el de la rejilla) y efectos */
    uint32_t gen_blk;            /* bloque de 16 pasos que ya se decidió (+1; 0 = ninguno) */
    uint8_t pchain[8], pchain_n, pchain_pos;

    /* audio en dos núcleos (a prueba): los canales de par_mask los calcula el otro núcleo. par_go
     * le entrega el tramo [done, done + n) y par_wait espera a que termine. */
    void (*par_go)(void *u, uint32_t done, uint32_t n);
    void (*par_wait)(void *u);
    void *par_user;
    uint8_t par_mask;
    volatile uint8_t par_on, par_tact, par_defer;
    /* Reparto que se ajusta solo (par_auto, con un reloj en now): cada BM_PAR_MIDE renders se mide
     * lo que costó cada canal y lo fijo del núcleo 0 (comandos, secuenciador, mezcla), y cada
     * BM_PAR_ELIGE se elige el reparto de los 64 posibles que deja más parejos los dos núcleos.
     * Los costos suben de golpe y bajan despacio: los picos (fills, acordes nuevos) cuentan. */
    uint32_t (*now)(void);
    uint8_t par_auto;
    uint16_t par_n;
    uint32_t cost[BM_TRACKS], cost_over;          /* ticks de now por render */
    volatile uint32_t cost_acc[BM_TRACKS];        /* los de par_mask los suma el otro núcleo */
    uint32_t over_acc;

    /* callbacks */
    bm_swap_fn swap; void *swap_user;
    bm_midi_out_fn midi_out; void *midi_user;
} bm_ctx;

void bm_audio_init(bm_ctx *c, bm_pattern *pat, bm_song *song, bm_cmdq *q, bm_audio_mem *mem);
void bm_audio_render(bm_ctx *c, int32_t *out_lr, uint32_t out_frames);
void bm_audio_overload(bm_ctx *c);
/* la otra tarea del audio en dos núcleos: calcula los canales de par_mask en el tramo pedido */
void bm_audio_par_process(bm_ctx *c, uint32_t done, uint32_t n);
/* el reparto que deja más parejos los dos núcleos con esos costos; cur si no mejora un 20 % */
uint8_t bm_audio_par_pick(const uint32_t cost[BM_TRACKS], uint32_t over, uint8_t cur);
#define BM_PAR_MIDE  4u
#define BM_PAR_ELIGE 64u

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
void bm_dsp_reset(void);
void bm_live_note(bm_ctx *c, int track, uint8_t note, uint8_t vel, int on);
uint32_t bm_ticks_to_samples(const bm_ctx *c, uint32_t ticks);
int bm_track_tps(const bm_ctx *c, int t);
void bm_loop_cmd(bm_ctx *c, int t, int action, int val);
void bm_scene_cmd(bm_ctx *c, int idx);
void bm_chain_cmd(bm_ctx *c, const uint8_t *chain, int n);
void bm_strum_cmd(bm_ctx *c, int t, const uint8_t *notes, int n, uint8_t vel);
void bm_engines_sync(bm_ctx *c);                  /* motor de cada canal según el patrón */
void bm_gen_decide(bm_ctx *c, uint32_t blk);      /* BREAK del sampler: decide un bloque de 16 pasos */
int  bm_vel(const bm_ctx *c, int t, int acc);     /* velocidad de un paso normal / con acento */
void bm_pchain_cmd(bm_ctx *c, const uint8_t *list, int n);
void bm_smp_rec_start(bm_ctx *c);
void bm_smp_rec_stop(bm_ctx *c);                 /* la toma queda como loop A, sin guardar */
const bm_smp_src *bm_smp_take(const bm_ctx *c, int src);   /* toma 0..7, BM_TAKE_NEW o NULL */
void bm_pad_key(bm_ctx *c, int pad, uint8_t vel, int on);   /* tecla de un pad: sostenida según su modo */
void bm_smp_preview(bm_ctx *c, int src, uint32_t start, uint32_t len, int on);   /* escuchar un trozo de una toma */

#endif
