/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — FM de 6 operadores (motor FM): interfaz en C sobre msfa (third_party/msfa,
 * Apache-2.0, versión de Synth_Dexed). Patches de 156 bytes en el formato de voz de la DX7
 * sin empaquetar (VCED): 6 operadores de 21 bytes (el primero es el OP6) y 30 de voz. */
#ifndef BM_FM_H
#define BM_FM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BM_FM_PATCH   156
#define BM_FM_OP      21          /* bytes por operador */
#define BM_FM_ALG     134
#define BM_FM_FB      135
#define BM_FM_TRSP    144
#define BM_FM_NAME    145
/* dentro de cada operador */
enum { FMO_R1, FMO_R2, FMO_R3, FMO_R4, FMO_L1, FMO_L2, FMO_L3, FMO_L4, FMO_BP, FMO_LD, FMO_RD, FMO_LC, FMO_RC,
       FMO_RS, FMO_AMS, FMO_KVS, FMO_OL, FMO_MODE, FMO_FC, FMO_FF, FMO_DT };

void bm_fm_init(float fs);                                     /* tablas (una vez) */
/* Voz v (0..BM_VOICES-1) del canal t: arranca, cambia de patch en vivo, suelta. src = nota de la
 * que viene el portamento (o la misma), porta = 0..127 velocidad del glide, -1 sin glide. */
void bm_fm_start(int v, int t, const uint8_t *patch, int note, int vel, int src, int porta);
void bm_fm_update(int v, const uint8_t *patch, int note, int vel, int porta);
void bm_fm_release(int v);
/* Suma n muestras de la voz a dst (escala de los motores: 32768 ~ una voz a todo volumen).
 * Calcula de a 64 por dentro. Devuelve 0 cuando, ya suelta, todas sus portadoras callaron. */
int  bm_fm_render(int v, int32_t *dst, int n);
uint8_t bm_fm_carriers(int alg);                               /* bit i: el operador i (dato i*21) es portadora */
/* Medición (hal/fm1/fm1_bench.c): ticks de now() para reps bloques de 64 de cada pieza:
 * [0] operador sin entrada, [1] operador modulado, [2] operador con realimentación,
 * [3] la voz entera (Dx7Note::compute con el patch), [4] el bucle vacío. */
void bm_fm_bench(const uint8_t *patch, uint32_t (*now)(void), uint32_t out[5], int reps);

#ifdef __cplusplus
}
#endif

#endif
