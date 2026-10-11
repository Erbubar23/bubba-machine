/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Servicio de actualización por USB-MIDI, compatible con M-UPGRADE y con web/instalar.html.
 *
 * Protocolo (FM-1-RE, docs/io/11-ota-protocol.md): el computador saluda y el FM-1 contesta su
 * identidad; con la orden de actualizar, el FM-1 pide partes del paquete (lecturas con
 * dirección y largo) y el computador contesta. Aquí se hace el paso 1:
 *   1. cabecera del paquete y lista de archivos (CRC);
 *   2. el cargador del paquete tiene que ser el oficial de M-VAVE (CRC conocidos);
 *   3. los primeros 16 KiB de flash.bin tienen que ser iguales a los del aparato (el cargador
 *      oficial los reescribe: así nunca cambia el arranque);
 *   4. el cargador se copia a la zona de espera (FM1_OTA_AREA), se relee y se comprueba;
 *   5. el computador confirma ("success" en la dirección 0xE0000000);
 *   6. registro de actualización (UPDATA_PARM del SDK) en flash y en RAM, y reinicio: el
 *      arranque del chip corre el cargador, que pide el paquete entero y lo escribe.
 * Hasta el paso 5 no se compromete nada: cualquier falla borra la zona de espera y el firmware
 * sigue como estaba. Los valores del registro (tipo 0x5A0D, identidad "ota-FM-1_015", dirección
 * del cargador y copia a 256 bytes del final de su zona) son los verificados en el FM-1 por X0X. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "update.h"
#include "fm1_hw.h"

#define LDR_HCRC 0xEBAAu                /* cargador oficial (usb_hid_ota.bin): cabecera y datos */
#define LDR_DCRC 0x5881u
#define LDR_LEN  0x4DE1u
#define LDR_LOAD 0x01C0A800u
#define REQ_MAX  512u

static uint16_t crc16(const uint8_t *p, uint32_t n, uint16_t c)
{
    while (n--) {
        c ^= (uint16_t)(*p++ << 8);
        for (int i = 0; i < 8; i++)
            c = (uint16_t)(c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1);
    }
    return c;
}

static void enc(uint8_t *p, uint32_t n)       /* cifrado de la cabecera UFW, llave 0xFFFF */
{
    uint16_t k = 0xFFFFu;
    while (n--) {
        *p++ ^= (uint8_t)k;
        k = (uint16_t)((k << 1) ^ (k & 0x8000u ? 0x1021u : 0u));
    }
}

static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd24(const uint8_t *p) { return rd16(p) | (uint32_t)p[2] << 16; }
static uint32_t rd32(const uint8_t *p) { return rd24(p) | (uint32_t)p[3] << 24; }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) {
            out[o++] = (uint8_t)(acc & 0x7F);
            acc >>= 7;
            nb -= 7;
        }
    }
    if (nb)
        out[o++] = (uint8_t)(acc & 0x7F);
    return o;
}

static uint32_t unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7F) << nb;
        nb += 7;
        if (nb >= 8) {
            if (o < max)
                out[o] = (uint8_t)acc;
            o++;
            acc >>= 8;
            nb -= 8;
        }
    }
    return o;
}

static uint8_t rx[REQ_MAX * 2], dec[REQ_MAX + 32], wire[64];

/* [00 59 cmd len24 cuerpo chk] empaquetado entre F0 y F7 */
static void enviar(uint8_t cmd, const uint8_t *body, uint32_t n)
{
    uint8_t m[8 + 32];
    uint32_t s = 0, w;
    m[0] = 0; m[1] = 0x59; m[2] = cmd; m[3] = (uint8_t)n; m[4] = 0; m[5] = 0;
    for (uint32_t i = 0; i < n; i++) {
        m[6 + i] = body[i];
        s += body[i];
    }
    m[6 + n] = (uint8_t)~s;
    wire[0] = 0xF0;
    w = 1 + pack7(m, 7 + n, wire + 1);
    wire[w++] = 0xF7;
    fm1_midi_send(wire, w);
}

static void identidad(void)
{
    uint8_t id[27] = {0};
    for (uint32_t i = 0; FM1_IDENT[i] && i < sizeof id; i++)
        id[i] = (uint8_t)FM1_IDENT[i];
    enviar(0x11, id, sizeof id);
}

/* un mensaje del computador ya decodificado; 0 si no es del protocolo */
static uint32_t tomar(uint32_t n)
{
    uint32_t d, s = 0;
    if (n < 3 || rx[0] != 0xF0 || rx[n - 1] != 0xF7)
        return 0;
    d = unpack7(rx + 1, n - 2, dec, sizeof dec);
    if (d < 7 || d > sizeof dec || dec[0] != 0 || dec[1] != 0x59 || rd24(dec + 3) + 7u != d)
        return 0;
    for (uint32_t i = 6; i + 1 < d; i++)
        s += dec[i];
    return (uint8_t)~s == dec[d - 1] ? d : 0;
}

static int es_actualizar(uint32_t n)
{
    static const uint8_t UP[6] = {0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7};
    return n == 6 && !memcmp(rx, UP, 6);
}

static uint32_t limite;                         /* fin de la sesión (ms) */

/* pide `len` bytes del paquete en `addr` (3 intentos de 5 s) */
static int leer(uint32_t addr, uint8_t *dst, uint32_t len)
{
    uint8_t q[8];
    if ((int32_t)(timer_get_ms() - limite) > 0)
        return -1;
    q[0] = 0;
    wr32(q + 1, addr);
    q[5] = (uint8_t)len; q[6] = (uint8_t)(len >> 8); q[7] = 0;
    for (int t = 0; t < 3; t++) {
        enviar(0x30, q, 8);
        for (uint32_t t0 = timer_get_ms(); timer_get_ms() - t0 < 5000u;) {
            int n = fm1_midi_get_sysex(rx, sizeof rx);
            uint32_t d;
            if (n <= 0) {
                os_time_dly(1);
                continue;
            }
            d = tomar((uint32_t)n);
            if (d == 7 && dec[2] == 0x11) {
                identidad();
                continue;
            }
            if (d == len + 15u && dec[2] == 0x30 && rd24(dec + 3) == len + 8u && rd32(dec + 7) == addr &&
                rd24(dec + 11) == len) {
                memcpy(dst, dec + 14, len);
                return 0;
            }
        }
    }
    return -1;
}

static void zona_borrar(void)
{
    for (uint32_t a = 0; a < FM1_OTA_LEN; a += 0x1000u)
        fm1_flash_erase(FM1_OTA_AREA + a);
}

/* pasos 1..5; 0 = el computador confirmó */
static int preparar(fm1_update_show show)
{
    static uint8_t hdr[0x400], b[REQ_MAX], mio[REQ_MAX];
    uint32_t i, n, nent, fl_off = 0, ota_off = 0, ota_len = 0, len, c, crc;
    show(1, 0);
    if (leer(0, hdr, 512) || leer(512, hdr + 512, 512))
        return -1;
    enc(hdr, 0x40);
    nent = rd16(hdr + 8);
    if (crc16(hdr + 2, 0x3E, 0) != rd16(hdr) || !nent || nent > 11u ||
        crc16(hdr + 0x40, nent * 0x50u, 0) != rd16(hdr + 2))
        return -2;
    for (i = 0; i < nent; i++) {
        uint8_t *e = hdr + 0x40 + i * 0x50u;
        enc(e, 0x50);
        if (rd16(e) == 0)
            fl_off = rd32(e + 8);
        if (rd16(e) == 100) {
            ota_off = rd32(e + 8);
            ota_len = rd32(e + 12);
        }
    }
    if (!fl_off || !ota_off)
        return -3;
    /* el cargador: solo el oficial */
    if (leer(ota_off, b, 512))
        return -4;
    len = rd32(b + 8);
    if (crc16(b + 2, 30, 0) != rd16(b) || rd32(b + 4) != 0x20u || b[12] != 0x41u || ota_len != len + 0x20u ||
        crc16(b + 0x22, 30, 0) != rd16(b + 0x20) || rd32(b + 0x28) != LDR_LOAD ||
        rd16(b) != LDR_HCRC || rd16(b + 2) != LDR_DCRC || len != LDR_LEN || len > FM1_OTA_LEN - 0x200u)
        return -5;
    c = rd16(b + 2);
    /* el arranque del paquete tiene que ser el del aparato */
    show(2, 0);
    for (i = 0; i < 0x4000u; i += REQ_MAX) {
        if (leer(fl_off + i, b, REQ_MAX) || fm1_flash_read(i, mio, REQ_MAX))
            return -6;
        if (memcmp(b, mio, REQ_MAX))
            return -7;
    }
    /* copiar el cargador a la zona de espera, releerlo y la cabecera al final */
    show(3, 0);
    zona_borrar();
    for (i = 0, crc = 0; i < len; i += n) {
        n = len - i > REQ_MAX ? REQ_MAX : len - i;
        if (leer(ota_off + 0x20u + i, b, n))
            return -8;
        crc = crc16(b, n, (uint16_t)crc);
        if (fm1_flash_prog(FM1_OTA_AREA + 32u + i, b, n))
            return -9;
    }
    if (crc != c)
        return -10;
    for (i = 0, crc = 0; i < len; i += n) {
        n = len - i > REQ_MAX ? REQ_MAX : len - i;
        if (fm1_flash_read(FM1_OTA_AREA + 32u + i, b, n))
            return -11;
        crc = crc16(b, n, (uint16_t)crc);
    }
    if (crc != c)
        return -11;
    {
        static const char NAME[16] = "LOADER.BIN";
        uint8_t h[32] = {0};
        wr16(h + 2, 0xFFFFu);
        wr32(h + 4, FM1_OTA_AREA);
        wr32(h + 8, len + 32u);
        h[12] = 0x41u;
        h[13] = 0xFFu;
        wr16(h + 14, 0xFFFFu);
        memcpy(h + 16, NAME, 16);
        wr16(h, crc16(h + 2, 30, 0));
        if (fm1_flash_prog(FM1_OTA_AREA, h, 32))
            return -12;
    }
    /* el computador confirma */
    show(4, 0);
    for (i = 0; i < 4u; i++)
        if (!leer(0xE0000000u, b, 8) && !memcmp(b, "success", 8))
            return 0;
    return -13;
}

static void sesion(fm1_update_show show)
{
    static uint8_t parm[112], back[112];
    int rc;
    limite = timer_get_ms() + 120000u;
    rc = preparar(show);
    if (rc) {
        zona_borrar();
        show(9, rc);
        return;
    }
    memset(parm, 0, sizeof parm);
    wr16(parm + 2, 0x5A0Du);
    wr16(parm + 4, UPDATA_READY);
    wr16(parm + 6, UPDATE_PARAM_MAGIC);
    memcpy(parm + 8, "ota-FM-1_015", 12);  /* identidad USB del cargador */
    wr32(parm + 72, FM1_OTA_AREA);
    wr16(parm, crc16(parm + 2, 78, 0));
    if (fm1_flash_prog(FM1_OTA_RES, parm, sizeof parm) || fm1_flash_read(FM1_OTA_RES, back, sizeof back) ||
        memcmp(back, parm, sizeof parm)) {
        zona_borrar();
        show(9, -14);
        return;
    }
    show(5, 0);
    os_time_dly(30);
    local_irq_disable();
    memcpy((void *)UPDATA_FLAG_ADDR, parm, sizeof parm);
    *(volatile uint32_t *)0x1EEE004u |= 0x2u;      /* el segundo núcleo, quieto mientras escribe */
    *(volatile uint32_t *)0x1EEE004u &= ~0x8u;
    JL_CLOCK->PWR_CON |= BIT(4);                    /* reinicio del núcleo: el arranque corre el cargador */
    while (1)
        ;
}

void fm1_update_poll(fm1_update_show show)
{
    int n;
    while ((n = fm1_midi_get_sysex(rx, sizeof rx)) > 0) {
        if (es_actualizar((uint32_t)n)) {
            sesion(show);
            continue;
        }
        if (tomar((uint32_t)n) == 7 && dec[2] == 0x11)
            identidad();
    }
}

/* al arrancar: si quedó algo en la zona de espera (una actualización que terminó o que no
 * empezó), se borra para que el arranque del chip no vuelva a correr el cargador */
void fm1_update_boot_cleanup(void)
{
    uint8_t a[4], r[8];
    int usado = 0;
    if (fm1_flash_read(FM1_OTA_AREA, a, 4) || fm1_flash_read(FM1_OTA_RES, r, 8))
        return;
    for (int i = 0; i < 4; i++)
        usado |= a[i] != 0xFFu;
    for (int i = 0; i < 8; i++)
        usado |= r[i] != 0xFFu;
    if (usado)
        zona_borrar();
}
