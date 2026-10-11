/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — tipos e identificadores de controles. */
#ifndef BM_TYPES_H
#define BM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "bm_config.h"

/* Botones: el orden coincide con los IDs 0..13 de la matriz (SPEC_V3 §1.3). */
enum {
    BTN_OCT_DN, BTN_OCT_UP, BTN_FX, BTN_SCL, BTN_ENV, BTN_LFO, BTN_EDIT,
    BTN_GLO, BTN_HOME, BTN_SAVE, BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC,
    BTN_COUNT
};

/* Encoders (7, sin pulsador). */
enum { ENC_SELECT, ENC_ALGO, ENC_K1, ENC_K2, ENC_K3, ENC_K4, ENC_PRESETS, ENC_COUNT };

/* Teclas: 27, de F3 (MIDI 53) a G5. Índice cromático 0..26. */
#define BM_KEYS          27
#define BM_KEY_BASE_NOTE 53
#define BM_WHITES        16
#define BM_BLACKS        11

/* IDs de LED: 0..13 botones, 14..40 teclas (igual que la matriz). */
#define BM_LED_KEY0      14
#define BM_LEDS          41

/* Niveles de LED. */
/* Luces: OFF vacío · DIM (~12 %) disponible · MID (50 %) tiene contenido · ON (100 %) activo o elegido ·
 * BLINK el paso que suena · BREATH armado o con algo especial (P-Lock). */
enum { LED_OFF, LED_DIM, LED_ON, LED_BLINK, LED_BREATH, LED_MID };

static inline bool bm_key_is_black(int k)
{
    int s = k % 12;              /* 0 = F */
    return s == 1 || s == 3 || s == 5 || s == 8 || s == 10;
}
int bm_key_white_index(int k);   /* -1 si es negra */
int bm_key_black_index(int k);   /* -1 si es blanca */
int bm_white_key(int i);         /* blanca i (0..15) -> tecla */
int bm_black_key(int i);         /* negra i (0..10) -> tecla */

static inline int bm_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

#endif
