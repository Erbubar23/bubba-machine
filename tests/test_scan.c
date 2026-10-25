/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — prueba del escaneo de matriz del FM-1 (antirrebote y cuadratura). */
#include <stdio.h>
#include <string.h>
#include "../hal/fm1/fm1_scan.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("  FALLO %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static fm1_scan_map map;
static fm1_scan s;
static uint8_t matrix[FM1_COLS];   /* estado físico simulado: bit = fila cerrada */

static void frame(void)
{
    for (int c = 0; c < FM1_COLS; c++) fm1_scan_column(&s, matrix[s.col]);
}

int main(void)
{
    memset(map.id, -1, sizeof map.id);
    map.id[1][3] = 20;                       /* una tecla en fila 1, columna 3 */
    map.enc_a[0][0] = 0; map.enc_a[0][1] = 0; /* encoder 0: A en (col 0, fila 0), B en (col 1, fila 0) */
    map.enc_b[0][0] = 1; map.enc_b[0][1] = 0;
    for (int e = 1; e < FM1_ENCS; e++) { map.enc_a[e][0] = 10; map.enc_a[e][1] = 5; map.enc_b[e][0] = 10; map.enc_b[e][1] = 5; }
    fm1_scan_init(&s, &map);
    frame();                                       /* primer frame: las perillas en su detent */

    printf("antirrebote\n");
    int down, id;
    matrix[3] = 1 << 1;
    frame();
    CHECK(fm1_scan_next_edge(&s, &down) == -1);   /* 1 frame: aún no */
    frame();
    id = fm1_scan_next_edge(&s, &down);
    CHECK(id == 20 && down == 1);                  /* 2 frames: presionada */
    matrix[3] = 0; frame(); matrix[3] = 1 << 1; frame(); matrix[3] = 0;   /* rebote al soltar */
    for (int i = 0; i < FM1_DEB_RELEASE - 1; i++) frame();
    CHECK(fm1_scan_next_edge(&s, &down) == -1);
    frame();
    id = fm1_scan_next_edge(&s, &down);
    CHECK(id == 20 && down == 0);                  /* FM1_DEB_RELEASE frames abiertos: suelta */

    printf("cuadratura: 1 detent = 4 transiciones\n");
    static const uint8_t CW[4] = {0, 2, 3, 1};     /* AB: 00 -> 10 -> 11 -> 01 -> 00 */
    for (int det = 0; det < 3; det++)
        for (int i = 1; i <= 4; i++) {
            uint8_t ab = CW[i % 4];
            matrix[0] = (ab >> 1) & 1; matrix[1] = ab & 1;
            frame();
        }
    int16_t v = fm1_scan_take_encoder(&s, 0);
    CHECK(v == 3 || v == -3);
    int dir = v > 0 ? 1 : -1;
    for (int det = 0; det < 2; det++)
        for (int i = 3; i >= 0; i--) {
            uint8_t ab = CW[i];
            matrix[0] = (ab >> 1) & 1; matrix[1] = ab & 1;
            frame();
        }
    v = fm1_scan_take_encoder(&s, 0);
    CHECK(v == -2 * dir);
    CHECK(fm1_scan_take_encoder(&s, 1) == 0);

    printf("cuadratura: una pausa a medio clic no hace que cuente doble (Felucca #23)\n");
    for (int i = 1; i <= 2; i++) {                 /* 00 -> 10 -> 11 y una pausa corta ahí */
        uint8_t ab = CW[i];
        matrix[0] = (ab >> 1) & 1; matrix[1] = ab & 1;
        frame();
    }
    for (int i = 0; i < 60; i++) frame();
    CHECK(fm1_scan_take_encoder(&s, 0) == 0);      /* a medio clic: todavía nada */
    for (int i = 3; i <= 4; i++) {                 /* 11 -> 01 -> 00: termina el clic */
        uint8_t ab = CW[i % 4];
        matrix[0] = (ab >> 1) & 1; matrix[1] = ab & 1;
        frame();
    }
    CHECK(fm1_scan_take_encoder(&s, 0) == dir);
    for (int det = 0; det < 2; det++)              /* y los clics de después cuentan uno cada uno */
        for (int i = 1; i <= 4; i++) {
            uint8_t ab = CW[i % 4];
            matrix[0] = (ab >> 1) & 1; matrix[1] = ab & 1;
            frame();
        }
    CHECK(fm1_scan_take_encoder(&s, 0) == 2 * dir);

    printf("cuadratura: giro rápido (saltos de dos estados) no pierde clics\n");
    for (int det = 0; det < 3; det++) {            /* 00 -> 11 -> 00: cada medio ciclo de un salto */
        matrix[0] = 1; matrix[1] = 0; frame();     /* uno normal para saber la dirección */
        matrix[0] = 0; matrix[1] = 1; frame();     /* salto */
        matrix[0] = 0; matrix[1] = 0; frame();
    }
    v = fm1_scan_take_encoder(&s, 0);
    CHECK(v == 3 * dir);

    printf("\n%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
