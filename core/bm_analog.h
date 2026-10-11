/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — ANALOG: sintetizador analógico virtual por voz. Dos osciladores de forma variable
 * con polyBLEP y sync (Plaits, MIT), 3 sierras, sub, ruido y ring; filtro de estado variable (stmlib)
 * LP 24/12, BP o HP con drive; dos ADSR y un LFO. Los ajustes son los de bm_params.h (AN_*). */
#ifndef BM_ANALOG_H
#define BM_ANALOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bm_an_init(float fs);
/* Voz v con los parámetros prm[] (AN_*) y las 4 macros. note en semitonos MIDI (con decimales),
 * vel 0..1. legato 0: voz nueva · 1: sigue sonando y solo cambia de nota (con glide) · 2: mono,
 * redispara las envolventes pero el tono sale de la nota anterior (glide). */
void bm_an_start(int v, const uint8_t *prm, const uint8_t mac[4], float note, float vel, int legato);
void bm_an_release(int v);
/* Escribe n muestras (n <= 64) en out. Devuelve 0 cuando la voz terminó de sonar. */
int  bm_an_render(int v, const uint8_t *prm, const uint8_t mac[4], float *out, int n);
float bm_an_level(int v);          /* nivel de la envolvente (para robar voces) */

#ifdef __cplusplus
}
#endif

#endif
