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
#include "bm_lang.h"
#include "bm_snap.h"

typedef struct {
    void (*blit)(void *u, int x, int y, int w, int h, const uint16_t *px);
    void (*led)(void *u, int id, uint8_t level);
    void (*midi_out)(void *u, uint8_t b);                 /* desde la ISR de audio */
    const uint8_t *(*flash_map)(void *u, uint32_t addr);  /* lectura XIP directa */
    const bm_flash_ops *flash;
    void *user;
} bm_platform;

/* Pantallas (1.0, etapa 1). HOME es el mezclador de 6 canales; GLO su menú global (SELECT MACHINE);
 * V_CH es el secuenciador de la batería (el único canal con pantalla propia). PLAY/REC no es una
 * pantalla: es HOME con el teclado tocando el canal del cursor. */
typedef enum {
    V_BOOT,
    V_HOME,                                     /* el mezclador (y PLAY/REC, si a->sloop) */
    V_GLO,                                      /* menú global del mezclador: SELECT MACHINE */
    V_CH,                                       /* el secuenciador de la batería */
    V_SYSTEM, V_SAVE,                           /* HOME 2 s, SAVE */
    V_MACH,                                     /* la máquina de un canal de notas (2026-10-21: la BX-7) */
    V_COUNT
} bm_view;

/* Páginas de la BX-7 (V_MACH), como los botones del FM-1 de fábrica: el mismo botón otra vez vuelve a
 * MAIN; HOME sale a la consola. */
enum { BXP_MAIN, BXP_SEQ, BXP_FX, BXP_ENV, BXP_LFO, BXP_EDIT, BXP_GLO, BXP_ARP, BXP_SCL, BXP_N };   /* SCL: el BB-303 (ACCENT / SLIDE por paso) */
enum { BXE_OP1, BXE_OP6 = 5, BXE_PIT, BXE_GLO, BXE_N };   /* qué edita EDIT: un operador, el pitch EG o lo global */
typedef struct {
    uint8_t page;                 /* BXP_* */
    uint8_t sub[BXP_N];           /* la página de cada botón (SELECT) */
    uint8_t edit;                 /* BXE_* */
    uint8_t esub[BXE_N];          /* la página de cada cosa de EDIT */
    uint8_t fxpos, grab;          /* FX: la casilla elegida y si SCL la tomó para moverla */
    uint8_t armed;                /* REC parado: la primera nota arranca y graba */
    uint8_t step;                 /* SEQ, paso a paso (REC parado): el paso que se escribe */
    uint8_t chord, nslot;         /* paso a paso: hay teclas del paso apretadas y cuántas notas lleva */
    uint8_t blk;                  /* BB-303 SCL: la página de 16 pasos */
    uint8_t vman, vpage;          /* piano roll: K3 eligió la página que se ve (si no, sigue al cabezal / cursor) */
    int8_t vy;                    /* piano roll: K4 corre las alturas que se ven (semitonos) */
    uint8_t szoom, sbars;         /* Slicer, TRIM: el zoom de la onda (0..3) y los compases que graba SAMPLE (1, 2 o 4) */
} bm_bx7ui;

#define BM_MACH_MS 2000u          /* cambio de máquina: la elegida se carga si se queda 2 s */
/* SYSTEM (2026-10-21, más simple): las filas que se ven, de las de siempre (sys_val / system_change):
 * reloj MIDI, sync, luces en reposo, idioma, versión. Las demás quedan como estaban. */
#define BM_SYS_VIS 5
static const uint8_t BM_SYS_ROWS[BM_SYS_VIS] = {0, 1, 5, 9, 10};
#define BM_SAVE_ROWS 3            /* SAVE: guardar (con las escenas), guardar como, borrar el patrón */
#define BM_GLO_PAGES 3
typedef enum { PG_SEQ, PG_COUNT } bm_page;

/* Capas del secuenciador de la batería: un clic en su botón la abre y otro la cierra (o HOME); sostenerlo es lo
 * mismo que un clic. */
typedef enum { L_NONE, L_ACC, L_REP, L_HIT, L_INST, L_MUTE, L_LEN, L_COUNT } bm_layer;   /* LFO, la negra 11, ENV, EDIT, SCL y (Slicer) SEQ otra vez: LENGTH AND SPEED */

typedef struct {
    uint8_t ver, clock, sync_mode, sync_ppqn, sync_lat, drive, led_dim, autosave, last_pat;
    uint8_t enter;          /* espera del recuadro de instrumento (ALGORITHM en INSTR): 2 = 400 ms; no se cambia desde la pantalla */
    uint8_t lang;           /* 0 = inglés, 1 = el paquete de idioma de la flash */
    uint8_t fast_lcd;       /* 1 = SPI a 30 MHz con DMA (a prueba; el modo seguro lo apaga) */
    uint8_t dual_core;      /* 1 = el audio en los dos núcleos (a prueba; el modo seguro lo apaga) */
    uint8_t factory;        /* los patrones de fábrica que se escribieron (BM_FACTORY_VER); otro número los vuelve a escribir */
    uint8_t rsv[18];
} bm_settings;
#define BM_FACTORY_VER 1          /* 1 (2026-10-24): DA FUNK, BLADE, ENJOY THE SILENCE */

typedef struct {
    uint8_t on, enc, pct, ghost;
    uint8_t beat;                 /* el tempo: cuatro bloques con el beat en vez de la barra */
    uint8_t knob;                 /* la perilla física que lo mostró + 1 (0 = ninguna: TAP, avisos) */
    uint16_t color;
    char title[22], val[12], unit[6];
    uint32_t until;
} bm_overlay;

typedef struct { char name[10], val[10]; uint8_t pct, locked; } bm_legend;

/* Mapa del teclado de una capa: lo que hace cada tecla, para dibujarlo y para los LEDs.
 * live: un golpe que suena ahora; la luz destella al 100 % encima de led. */
typedef struct { char label[6]; uint8_t led, live; uint16_t color; } bm_keyfn;

#define BM_JOBS 10
enum { JOB_NONE, JOB_FLUSH_PEND, JOB_SAVE_CUR, JOB_SAVE_AS, JOB_SAVE_SONG, JOB_SAVE_GLOB, JOB_LOAD,
       JOB_SAVE_TAKE, JOB_TAKE_META, JOB_TAKE_ERASE };

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

    /* sampler: tomas, pads y dibujo */
    bm_take take[BM_TAKES];
    bm_smp_src smp_send[1];       /* copia estable que lee el audio: la toma recién guardada */
    bm_smp_src take_send[BM_TAKES];   /* las tomas, para los pads (CMD_SMP_TAKES) */
    uint8_t take_dirty, takes_row;
    volatile uint8_t ext_take_del;  /* toma a borrar pedida desde afuera (id + 1), se borra en STOP */
    int8_t pad_key;               /* tecla que sostiene un pad (o una escucha) */
    uint8_t slice_sens;           /* sensibilidad de los cortes en los golpes (0..127) */
    uint8_t len_page;             /* Slicer, LENGTH AND SPEED: los largos que dan las blancas (0: 1-16 ... 3: 49-64) */

    /* portapapeles, undo y redo (un canal) */
    bm_trackbuf scratch;          /* bolsa de trabajo: un canal entero a medias */

    /* UI */
    bm_gesture ges;
    bm_screen scr;
    uint8_t view, layer, track, step_page;
    uint8_t mach_sel;             /* cambio de máquina: la elegida (ENG_*) */
    uint8_t mach_arm;             /* 1 = la cuenta de 2 s corre (ALGORITHM la movió a otra) */
    uint8_t mach_trk;             /* el canal que cambia de máquina (la tarjeta o GLO > MACHINES) */
    uint32_t mach_t0;             /* cuándo se llegó a la elegida */
    uint8_t sel_lane[BM_TRACKS];
    int8_t oct;
    uint8_t rec, fill;
    uint8_t sloop;                /* SLOOP-REC (REC en la consola): las teclas tocan el canal del cursor */
    uint64_t rec_steps;           /* REC de esta pasada: los pasos donde se grabó (las notas se ven en rojo) */
    uint16_t rec_lanes;           /* y los instrumentos (rejillas) */
    uint8_t box_on, box_track, box_hold;
    uint8_t box_mode, box_lane;   /* recuadro de ALGORITHM en INSTR: BOX_LANE (instrumento) */
    uint8_t layer_latch;          /* la capa quedó abierta con un toque (FILL, ROLL, LFO): se cierra con la misma tecla o HOME */
    uint32_t box_until;
    int8_t held_step, held_white;
    uint8_t held_edit, held_had_event, held_long, held_notes;
    uint32_t step_t0;
    uint8_t keys_down[BM_KEYS];
    uint8_t key_notes[BM_KEYS][4], key_nn[BM_KEYS], key_trk[BM_KEYS];
    int8_t key_rec_step[BM_KEYS], key_rec_slot[BM_KEYS];
    uint32_t key_rec_tick[BM_KEYS];
    uint32_t key_rec_bt[BM_KEYS];   /* btick al grabarla: lo que se sostuvo (rtick vuelve a 0 con el maestro) */
    uint8_t rec_vel;              /* la fuerza de la última nota tocada: 127 se graba con ACCENT */
    uint8_t oct_used;             /* el OCT sostenido ya se usó (transposición o los dos): al soltarlo no cambia la octava */
    uint32_t rec_clr_bt[BM_STEPS];  /* PLAY/REC: btick en que se reescribió cada paso (en la misma vuelta, se suma: un acorde) */
    uint64_t rec_clr_on;          /* los pasos con rec_clr_bt válido */
    int8_t rec_clr_trk;           /* de qué canal son */
    uint32_t key_t0[BM_KEYS];
    uint8_t key_role[BM_KEYS];    /* qué hizo la tecla al bajar (para soltarla bien) */
    uint8_t last_note[BM_TRACKS];
    uint16_t pat_held;            /* blancas sostenidas en HOME: cadena de patrones */
    uint8_t pchain[8], pchain_n;
    int8_t solo_key, solo_track;
    uint8_t solo_on, solo_saved;
    /* ROLL (ARP en T1 y T2): en vivo, no se guarda */
    uint8_t brk_live;
    uint8_t chain[8], chain_n;
    uint8_t menu_row, save_row, save_target, clear_armed;
    uint8_t glo_page;             /* GLO de HOME: 0 MACHINES, 1 TEMPO (y escala), 2 MASTER FX (PRESETS la cambia) */
    uint32_t clear_t;
    bm_overlay ov;
    char toast[30];
    uint32_t toast_until;
    uint32_t autosave_until;          /* marca de autoguardado en el encabezado (sin aviso) */
    uint8_t battery;
    uint16_t master_adc, master_sent;
    uint32_t now, boot_t;
    uint8_t last_trig[BM_TRACKS];
    uint32_t trig_until[BM_TRACKS];
    uint32_t lane_flash[BM_TRACKS][BM_LANES];
    uint32_t clip_until;
    uint16_t edits_seen;
    uint8_t cpu;                  /* carga del audio en % (la cabecera) */
    int8_t move;                  /* MOVE de HOME (PRESETS): -63..63, en vivo, no se guarda */
    uint32_t wt_sig[2];           /* BITTAL: qué tablas están armadas en RAM */
    /* perillas: aceleración por el tiempo entre giros, detents crudos y la perilla que gira */
    uint32_t enc_t[ENC_COUNT];    /* cuándo giró cada perilla por última vez */
    int8_t enc_raw;               /* detents de este giro, sin acelerar (0 fuera de un giro) */
    uint8_t enc_cur;              /* la perilla que está girando + 1 (0 = ninguna) */
    uint8_t enc_new;              /* este giro empieza un gesto nuevo (la perilla estaba quieta) */
    int8_t stick;                 /* el centro de un control bipolar: detents que lleva retenidos */
    uint8_t stick_id;
    uint8_t snap_buf[BM_SNAP_MAX];  /* un sonido que llega de afuera (bm_app_sound_load) */
    uint16_t snap_len;
    bm_bx7ui bx;                  /* la BX-7 (core/bm_bx7.c) */
} bm_app;

void bm_app_init(bm_app *a, const bm_platform *pf, uint32_t now);
void bm_app_button(bm_app *a, int id, int down, uint32_t now);
void bm_app_key(bm_app *a, int key, int down, uint32_t now);
void bm_app_encoder(bm_app *a, int id, int delta, uint32_t now);
void bm_app_master(bm_app *a, uint16_t adc10);
void bm_app_battery(bm_app *a, uint8_t pct);
void bm_app_cpu(bm_app *a, int pct);                  /* la carga del audio, para la cabecera */
void bm_app_midi_in(bm_app *a, uint8_t byte);
void bm_app_tick(bm_app *a, uint32_t now);
void bm_app_audio(bm_app *a, int32_t *out_lr, uint32_t frames);
void bm_app_takes_reload(bm_app *a);   /* tras escribir una toma desde fuera (editor/emulador) */
/* Paquete de idioma desde afuera (consola USB o emulador), solo en STOP: sub 0 = empezar (borra la
 * zona), 1 = n bytes en off, 2 = terminar (lo revisa y lo activa), 3 = quitarlo. 0 = salió bien. */
int  bm_app_lang_load(bm_app *a, int sub, uint32_t off, const uint8_t *d, uint32_t n);
const uint8_t *bm_app_lang_pack(const bm_app *a);   /* el paquete de la flash si está sano, o NULL */
/* Un sonido (.bsnap, o un banco de BX-7) desde afuera, solo en STOP: sub 0 = empezar, 1 = n bytes en
 * off, 2 = guardarlo en la zona compartida (lo revisa), 3 = borrar uno del usuario: off = máquina << 8 |
 * su número en PRESETS (en BX-7, el número de banco del usuario); 4 = borrar el del usuario número off.
 * 0 = salió bien. */
int  bm_app_sound_load(bm_app *a, int sub, uint32_t off, const uint8_t *d, uint32_t n);
/* Informe para la consola y la plataforma de control (web/control.html), little-endian:
 *   what 0 = resumen (BM_REPORT_LEN bytes), 1 = la toma idx, 2 = el sonido del usuario idx.
 * Devuelve el largo, o -1 si idx no existe o no cabe en cap. */
#define BM_REPORT_LEN 72
int  bm_app_report(const bm_app *a, int what, int idx, uint8_t *out, int cap);
void bm_app_take_delete(bm_app *a, int id);     /* desde afuera: se borra en STOP, como N11 en TOMAS */

/* Fuentes del sampler: 0..7 = toma, BM_SRC_ROM|i = loop de fábrica. */
int  bm_app_sources(const bm_app *a, uint8_t out[BM_TAKES + 8]);   /* lista de fuentes que existen */
bool bm_app_src_ok(const bm_app *a, uint8_t src);
void bm_app_src_name(const bm_app *a, uint8_t src, char *buf, int cap);
bool bm_app_src_info(const bm_app *a, uint8_t src, bm_smp_src *out);

/* para el dibujo */
void bm_app_legend(const bm_app *a, bm_legend out[4]);
/* Qué hacen ahora las tres perillas grandes (SELECT, ALGORITHM, PRESETS): un nombre corto, o "-"
 * si no hacen nada aquí. Sale del mismo reparto que bm_app_encoder. */
void bm_app_bigknobs(const bm_app *a, const char **sel, const char **alg, const char **pre);
void bm_app_keymap(const bm_app *a, bm_keyfn out[BM_KEYS], char *title, int cap, uint16_t *color, const char **h1, const char **h2);
/* qué hace cada tecla ahora (nombre corto) y su luz: lo mismo que se ve en la tira de teclas */
void bm_app_keys(const bm_app *a, bm_keyfn out[BM_KEYS]);
void bm_app_draw(bm_gfx *g, void *user);
const char *bm_track_name(const bm_app *a, int t);
const char *bm_kit_name(int kit);                       /* KO-808, KO-909 (ROM) o SINTE */
const char *bm_sound_name(const bm_app *a, int t);      /* kit, timbre o banco de ondas del canal */
int  bm_sound_count(const bm_app *a, int t);            /* cuántos hay para PRESETS */
uint16_t bm_track_color(int t);
uint16_t bm_knob_color(int i);
const char *bm_lane_name(const bm_app *a, int t, int lane);
int  bm_app_track_step(const bm_app *a, int t, int lane);   /* paso que suena (lane < 0: canal de notas) */
int  bm_app_enter_ms(const bm_app *a);                       /* -1 = OFF */
uint32_t bm_app_rec_room(const bm_app *a);                   /* muestras que caben en la próxima toma */
/* una escena guardada sostenida: el anillo hacia los 2 s en que se borra (0..255), o -1 */
int  bm_app_key_ring(const bm_app *a, char *label, int cap);

#endif
