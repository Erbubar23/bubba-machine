/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — escaneo de la matriz del FM-1 (lógica portable).
 *
 * Hechos de hardware (SPEC_V3 §1.3): matriz con diodos de 11 columnas x 6 filas
 * detrás de 2x 74HC595, escaneada por TIMER5 a ~10 kHz, una columna por tick.
 * 41 IDs: botones 0..13, teclas 14..40 (F3..G5). 7 encoders en la propia matriz
 * (fila PA0 y fila PB7), 1 detent = 4 transiciones de cuadratura.
 *
 * Este módulo NO toca registros: recibe la lectura cruda de cada columna y
 * produce flancos de tecla y contadores de encoder. Los IDs exactos de cada
 * cruce fila/columna se cargan en fm1_scan_map (ver hal/fm1/README_PORT.md). */
#ifndef FM1_SCAN_H
#define FM1_SCAN_H

#include <stdint.h>

#define FM1_COLS 11
#define FM1_ROWS 6
#define FM1_IDS 41
#define FM1_ENCS 7
#define FM1_DEB_PRESS 2
#define FM1_DEB_RELEASE 8

typedef struct {
    int8_t id[FM1_ROWS][FM1_COLS];          /* ID de tecla/botón o -1 */
    int8_t enc_a[FM1_ENCS][2], enc_b[FM1_ENCS][2];  /* (columna, fila) de A y B */
} fm1_scan_map;

typedef struct {
    const fm1_scan_map *map;
    uint8_t col;
    uint8_t raw[FM1_COLS];                  /* bit r = fila r cerrada */
    uint8_t cnt[FM1_IDS], state[FM1_IDS];
    volatile uint32_t down_bits[2];         /* estado estable (para la UI) */
    volatile uint32_t edge_bits[2];         /* flancos pendientes; la UI los consume */
    uint8_t q[FM1_ENCS];                    /* último estado AB */
    int8_t qsub[FM1_ENCS];                  /* transiciones acumuladas hacia un detent */
    volatile int16_t enc_acc[FM1_ENCS];     /* la UI lee y pone a cero */
} fm1_scan;

void fm1_scan_init(fm1_scan *s, const fm1_scan_map *m);
/* Llamar desde la ISR con la lectura de la columna actual; devuelve la siguiente columna. */
uint8_t fm1_scan_column(fm1_scan *s, uint8_t rows_closed);
/* Desde el bucle principal: devuelve un flanco (id, down) o -1 si no hay. */
int fm1_scan_next_edge(fm1_scan *s, int *down);
int16_t fm1_scan_take_encoder(fm1_scan *s, int e);

#endif
