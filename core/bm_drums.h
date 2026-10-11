/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — batería modelada: cada instrumento es un modelo calculado en vivo. Los bombos,
 * cajas y hats son los de Plaits (third_party/plaits, MIT, de Mutable Instruments), que modelan los
 * circuitos de la 808 y versiones sintéticas al estilo 909; palmas, cencerro, resonador, ruido y
 * percusión FM son propios. Todo en float, por voz. */
#ifndef BM_DRUMS_H
#define BM_DRUMS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DRM_BD_ANALOG,      /* bombo 808: resonador en puente T con FM al golpe */
    DRM_BD_SINTE,       /* bombo 909: seno con barrido, transitorio y suciedad */
    DRM_SD_ANALOG,      /* caja 808: 5 modos de parche y ruido filtrado */
    DRM_SD_SINTE,       /* caja 909: dos senos con FM y ruido */
    DRM_HH_ANALOG,      /* hat 808: 6 cuadradas en Schmitt, pasa-banda y VCA */
    DRM_HH_METAL,       /* hat metálico: modulación en anillo, envolvente en dos tramos (platillos) */
    DRM_CLAP,           /* palmas: ráfagas de ruido filtrado y cola */
    DRM_TOM,            /* tom y conga: parche de 5 modos sin bordona */
    DRM_COWBELL,        /* cencerro 808: dos cuadradas y pasa-banda */
    DRM_RESON,          /* rim, clave, bloque: dos resonadores golpeados */
    DRM_NOISE,          /* maracas y shaker: ruido con filtro que barre de grave a agudo */
    DRM_FM,             /* percusión FM de 2 operadores con barrido (zap, metal, blip) */
    DRM_COUNT
};

extern const char *const bm_drm_names[DRM_COUNT];     /* nombre largo del modelo */
extern const char *const bm_drm_short[DRM_COUNT];     /* 5 letras, para la rejilla */
extern const char *const bm_drm_color[DRM_COUNT];     /* qué hace COLOR en ese modelo */
extern const uint16_t bm_drm_base_hz[DRM_COUNT];      /* tono con AFINA en el centro */

void bm_drm_init(float fs);
/* Voz v (0..BM_VOICES-1). hz = tono; tone, decay, color 0..1; accent 0..1 (golpe y nivel). */
void bm_drm_trigger(int v, int model, float hz, float tone, float decay, float color, float accent);
/* Escribe n muestras (n <= 64) en out, en ±1. Devuelve 0 cuando el golpe terminó. */
int  bm_drm_render(int v, float *out, int n);

#ifdef __cplusplus
}
#endif

#endif
