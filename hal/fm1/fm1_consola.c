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
 * Es solo lectura: no escribe memoria ni flash. La herramienta es tools/fm1/consola.py. */
#include <string.h>
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "asm/clock.h"
#include "fm1_hw.h"

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
        for (int k = 1; k <= 4; k++) {
            uint8_t e = 0, fin = 0, ok = 0;
            fm1_stat[FM1_STAT_PAST1 + k - 1] = fm1_boot_past(k, &e, &fin, &ok) ?
                (uint32_t)e | (uint32_t)fin << 8 | (uint32_t)ok << 16 | 1u << 24 : 0;
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
    case 0x04:
        fm1_stat[FM1_STAT_RENDER_MAX] = 0;
        fm1_stat[FM1_STAT_UI_MAX] = 0;
        fm1_stat[FM1_STAT_SCANISR_MAX] = 0;
        responder(0x04, NULL, 0, NULL, 0);
        break;
    }
}
