/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — idiomas de la pantalla.
 *
 * El firmware habla inglés: cada texto de pantalla está escrito en inglés en el código y marcado
 * con T() (o TF() si es un formato de printf, o N_() dentro de una tabla). Otro idioma es un
 * paquete en la flash (BM_LANG_LO..BM_LANG_HI) que no viene cargado: se sube por USB o desde el
 * emulador. Para cada texto, el paquete guarda el hash de la frase en inglés y su traducción;
 * lo que no está traducido se ve en inglés. tools/gen_lang.py junta las frases y arma paquetes.
 *
 * Paquete (.bmlang), todo little-endian:
 *   cabecera  "BMLG", ver (1), 0, n (2), nombre[12] (ASCII, termina en 0), tamaño total (4), crc32 (4)
 *   hash[n]   uint32, FNV-1a de la frase en inglés, ordenados
 *   off[n]    uint16, dónde empieza cada traducción dentro de los textos
 *   textos    terminados en 0
 * El crc32 cubre desde hash[0] hasta el final. */
#ifndef BM_LANG_H
#define BM_LANG_H

#include "bm_types.h"

#define BM_LANG_MAGIC   0x474C4D42u   /* "BMLG" */
#define BM_LANG_VER     1
#define BM_LANG_HDR     28u

/* El texto en el idioma activo. T: texto fijo · TF: formato de printf (si la traducción no tiene
 * los mismos %, se usa el inglés) · N_: solo marca una frase en una tabla (se traduce al usarla). */
const char *bm_txt(const char *en);
const char *bm_txtf(const char *en);
#define T(s)  bm_txt(s)
#define TF(s) bm_txtf(s)
#define N_(s) (s)

uint32_t bm_lang_hash(const char *s);
uint32_t bm_lang_crc(const uint8_t *d, uint32_t n);
/* Revisa un paquete. Devuelve su tamaño (0 = no sirve) y copia el nombre si name != NULL. */
uint32_t bm_lang_check(const uint8_t *p, uint32_t cap, char name[12]);
/* Activa un paquete ya revisado (NULL = inglés). El puntero tiene que seguir vivo (flash XIP). */
void bm_lang_use(const uint8_t *p);
const char *bm_lang_name(void);   /* "ENGLISH" o el nombre del paquete activo */

#endif
