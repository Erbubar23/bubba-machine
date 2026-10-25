/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Consola por USB-MIDI: mediciones del firmware en el aparato, para no adivinar.
 *
 * Mensajes SysEx con el ID de uso no comercial (0x7D) y la firma "BM":
 *   pedido   F0 7D 42 4D cmd datos… F7        respuesta  F0 7D 42 4D (cmd|0x40) datos… F7
 * Los datos van en nibbles (cada byte = nibble alto, nibble bajo), así cualquier byte cabe en
 * SysEx. Órdenes:
 *   01            estado: FM1_STAT_N palabras de 32 bits (ver fm1_hw.h, FM1_STAT_*)
 *   02 adr32 n8   leer n bytes (<= 64) de memoria o registros desde adr
 *   03 adr32 n8   leer n bytes (<= 64) de la flash desde adr
 *   04            poner a cero los máximos
 *   07 off16      64 bytes del registro de texto desde off (respuesta: off16 n8 texto)
 *   08            perfil del audio: BM_P_N acumulados, ticks transcurridos, ticks por segundo
 *   20 sub8 a32 b16 datos   toma del sampler (fm1_toma; tools/fm1/toma.py)
 *   21 sub8 off32 0016 datos  paquete de idioma (fm1_idioma; tools/fm1/idioma.py)
 *   22 sub8 off32 0016 datos  un sonido .bsnap o un banco de BX-7 (fm1_sonido; tools/fm1/sonido.py)
 *   23 sub8 idx8  memoria y biblioteca (fm1_informe; web/control.html): 0 resumen, 1 toma, 2 sonido,
 *                 3 borrar la toma idx (respuesta: sub8 ok8 datos…)
 *   10 cmd8 arg8  laboratorio (fm1_lab): 1 barrido, 2 audio en silencio, 3 tono, 4 flash arg
 *                 veces, 5 audio detenido, 6 arrancar Bubba con audio (respuesta: resultado8)
 * Lee y no escribe nada, salvo la prueba de flash del laboratorio (solo en el sector de prueba
 * 0xE1000, dentro de la zona del cargador en espera). La herramienta es tools/fm1/consola.py. */
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "asm/clock.h"
#include "fm1_hw.h"

/* Memoria libre del montón del SDK (dlmalloc sobre sbrk, system.a). malloc no sirve para medirla:
 * devuelve un puntero hasta para 512 KiB. Desde una tarea, no desde la interrupción del USB. */
extern unsigned int get_malloc_remain_heap_size(void);
extern unsigned int get_ram_free_heap_size(void);

void fm1_heap_medir(void)
{
    fm1_stat[FM1_STAT_HEAP_FREE] = get_malloc_remain_heap_size();
    fm1_stat[FM1_STAT_HEAP_MIN] = get_ram_free_heap_size();
}

volatile uint32_t fm1_stat[FM1_STAT_N];

static uint8_t tx[8 + 2 * (FM1_STAT_N * 4 + 8) + 1];

static uint32_t nib_in(const uint8_t *p, int n)    /* n nibbles -> valor */
{
    uint32_t v = 0;
    for (int i = 0; i < n; i++)
        v = (v << 4) | (p[i] & 0x0Fu);
    return v;
}

static void responder(uint8_t cmd, const uint8_t *a, uint32_t na, const uint8_t *b, uint32_t nb)
{
    uint32_t n = 0;
    tx[n++] = 0xF0; tx[n++] = 0x7D; tx[n++] = 0x42; tx[n++] = 0x4D; tx[n++] = (uint8_t)(cmd | 0x40);
    for (uint32_t i = 0; i < na; i++) { tx[n++] = a[i] >> 4; tx[n++] = a[i] & 0x0Fu; }
    for (uint32_t i = 0; i < nb; i++) { tx[n++] = b[i] >> 4; tx[n++] = b[i] & 0x0Fu; }
    tx[n++] = 0xF7;
    fm1_midi_send(tx, n);
}

void fm1_consola(const uint8_t *m, uint32_t n)
{
    static uint8_t buf[64];
    uint8_t cab[5];
    if (n < 6 || m[1] != 0x7D || m[2] != 0x42 || m[3] != 0x4D)
        return;
    switch (m[4]) {
    case 0x01: {
        static uint32_t w[FM1_STAT_N];
        fm1_stat[FM1_STAT_UPTIME] = timer_get_ms();
        fm1_stat[FM1_STAT_SYSCLK] = (uint32_t)clk_get("sys");
        fm1_stat[FM1_STAT_LSBCLK] = (uint32_t)clk_get("lsb");
        fm1_stat[FM1_STAT_HALVES] = fm1_audio_halves();
        fm1_stat[FM1_STAT_LATE] = fm1_audio_late();
        fm1_stat[FM1_STAT_SCAN] = fm1_scan_ticks();
        fm1_stat[FM1_STAT_USB] = (uint32_t)fm1_midi_ready() | (uint32_t)fm1_midi_on() << 1;
        fm1_stat[FM1_STAT_FAILS] = fm1_boot_fails();
        for (int k = 1; k <= 4; k++) {                /* 0.8.1: etapa | ok << 4 | 0x80, fin, raw P3_RST_SRC that ended it, FM1_BF_* */
            uint8_t e = 0, fin = 0, ok = 0, rst[5] = {0}, fl = 0, sig[5] = {0}, f2 = 0;
            if (fm1_boot_past(k, &e, &fin, &ok)) {
                fm1_boot_raw(k, rst, &fl);
                fm1_boot_raw(k - 1, sig, &f2);           /* the reset that ended boot k is written by boot k - 1 */
                fm1_stat[FM1_STAT_PAST1 + k - 1] = (uint32_t)(e & 15u) | (uint32_t)(ok ? 16u : 0u) | 0x80u |
                    (uint32_t)fin << 8 | (uint32_t)sig[0] << 16 | (uint32_t)fl << 24;
            } else {
                fm1_stat[FM1_STAT_PAST1 + k - 1] = 0;
            }
        }
        for (int i = 0; i < FM1_STAT_N; i++) {       /* bytes en orden little-endian */
            uint32_t v = fm1_stat[i];
            ((uint8_t *)w)[i * 4 + 0] = (uint8_t)v;
            ((uint8_t *)w)[i * 4 + 1] = (uint8_t)(v >> 8);
            ((uint8_t *)w)[i * 4 + 2] = (uint8_t)(v >> 16);
            ((uint8_t *)w)[i * 4 + 3] = (uint8_t)(v >> 24);
        }
        responder(0x01, (const uint8_t *)w, sizeof w, NULL, 0);
        break;
    }
    case 0x02:
    case 0x03: {
        if (n < 5 + 8 + 2 + 1)
            return;
        uint32_t adr = nib_in(m + 5, 8), cnt = nib_in(m + 13, 2);
        if (cnt > sizeof buf)
            cnt = sizeof buf;
        if (m[4] == 0x02)
            memcpy(buf, (const void *)(uintptr_t)adr, cnt);
        else if (fm1_flash_read(adr, buf, cnt))
            cnt = 0;
        cab[0] = (uint8_t)(adr >> 24); cab[1] = (uint8_t)(adr >> 16); cab[2] = (uint8_t)(adr >> 8); cab[3] = (uint8_t)adr;
        cab[4] = (uint8_t)cnt;
        responder(m[4], cab, 5, buf, cnt);
        break;
    }
    case 0x07: {
        if (n < 5 + 4 + 1)
            return;
        uint32_t off = nib_in(m + 5, 4), cnt = fm1_log_read(off, buf, sizeof buf);
        cab[0] = (uint8_t)(off >> 8); cab[1] = (uint8_t)off; cab[2] = (uint8_t)cnt;
        responder(0x07, cab, 3, buf, cnt);
        break;
    }
    case 0x10: {
        if (n < 5 + 4 + 1)
            return;
        int c = (int)nib_in(m + 5, 2), a = (int)nib_in(m + 7, 2);
        cab[0] = 0xEE;
        responder(0x10, cab, 1, NULL, 0);           /* recibido; el resultado queda en el registro */
        cab[0] = (uint8_t)fm1_lab(c, a);
        break;
    }
    case 0x08: {
        static uint32_t w[16];
        int k = fm1_prof_read(w);
        static uint8_t b8[64];
        for (int i = 0; i < k; i++) {
            b8[i * 4] = (uint8_t)w[i]; b8[i * 4 + 1] = (uint8_t)(w[i] >> 8);
            b8[i * 4 + 2] = (uint8_t)(w[i] >> 16); b8[i * 4 + 3] = (uint8_t)(w[i] >> 24);
        }
        responder(0x08, b8, (uint32_t)k * 4u, NULL, 0);
        break;
    }
    case 0x20: {                                    /* toma: sub (1 byte), a (4), b (2), datos */
        if (n < 5 + 2 + 8 + 4 + 1)
            return;
        int sub = (int)nib_in(m + 5, 2);
        uint32_t a = nib_in(m + 7, 8), b = nib_in(m + 15, 4);
        int r = fm1_toma(sub, a, b, m + 19, n - 19 - 1);
        cab[0] = (uint8_t)sub; cab[1] = (uint8_t)(r == 0);
        cab[2] = (uint8_t)(a >> 16); cab[3] = (uint8_t)(a >> 8); cab[4] = (uint8_t)a;
        responder(0x20, cab, 5, NULL, 0);
        break;
    }
    case 0x21: {                                    /* idioma: sub (1 byte), desde (4), 0 (2), datos */
        if (n < 5 + 2 + 8 + 4 + 1)
            return;
        int sub = (int)nib_in(m + 5, 2);
        uint32_t a = nib_in(m + 7, 8);
        int r = fm1_idioma(sub, a, m + 19, n - 19 - 1);
        cab[0] = (uint8_t)sub; cab[1] = (uint8_t)(r == 0);
        cab[2] = (uint8_t)(a >> 16); cab[3] = (uint8_t)(a >> 8); cab[4] = (uint8_t)a;
        responder(0x21, cab, 5, NULL, 0);
        break;
    }
    case 0x22: {                                    /* sonido: sub (1 byte), desde (4), 0 (2), datos */
        if (n < 5 + 2 + 8 + 4 + 1)
            return;
        int sub = (int)nib_in(m + 5, 2);
        uint32_t a = nib_in(m + 7, 8);
        int r = fm1_sonido(sub, a, m + 19, n - 19 - 1);
        cab[0] = (uint8_t)sub; cab[1] = (uint8_t)(r == 0);
        cab[2] = (uint8_t)(a >> 16); cab[3] = (uint8_t)(a >> 8); cab[4] = (uint8_t)a;
        responder(0x22, cab, 5, NULL, 0);
        break;
    }
    case 0x23: {                                    /* informe: sub (1 byte), idx (1 byte) */
        static uint8_t rep[96];
        if (n < 5 + 4 + 1)
            return;
        int sub = (int)nib_in(m + 5, 2), idx = (int)nib_in(m + 7, 2);
        int r = fm1_informe(sub, idx, rep, sizeof rep);
        cab[0] = (uint8_t)sub; cab[1] = (uint8_t)(r >= 0);
        responder(0x23, cab, 2, rep, r > 0 ? (uint32_t)r : 0);
        break;
    }
    case 0x04:
        fm1_stat[FM1_STAT_RENDER_MAX] = 0;
        fm1_stat[FM1_STAT_UI_MAX] = 0;
        fm1_stat[FM1_STAT_LOOP_US_MAX] = 0;
        fm1_stat[FM1_STAT_SCANISR_MAX] = 0;
        fm1_stat[FM1_STAT_SCAN_LAT] = 0;
        responder(0x04, NULL, 0, NULL, 0);
        break;
    }
}
