/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Arranques fallidos, modo seguro, registro de hasta dónde llegó cada arranque y salida al modo
 * de actualización del chip.
 *
 * Cada arranque usa 32 bytes del sector FM1_BOOT_SEC ("BM-BOOT-3"; 0.8.1, antes 8):
 *   [0] empezó (0x00)         [1] anduvo bien (0x00, a los 10 s)
 *   [2] etapas: cada etapa alcanzada pone en 0 un bit más (0xFF -> 0xFE -> 0xFC ...)
 *   [3] por qué se reinició el chip antes de este arranque (FM1_RST_*)
 *   [4..7] lo que anotó el perro guardián del arranque anterior
 *   [8..12] the raw reset sources: P3_RST_SRC, JL_SYSTEM->RST_SRC, R3_WKUP_SRC, P3_PMU_CON1 (after
 *           the SDK set its bit 6) and P3_VLVD_CON (bit 7: low-voltage detect pending)
 *   [13] FM1_BF_* that happened in this boot, inverted (a bit goes to 0 when it happens)
 *   [16..23] the RAM breadcrumb the previous run left (fm1_rastro), 0xFF if there was none
 * Programar bits de 1 a 0 no necesita borrar: el sector se borra una vez cada 127 arranques.
 * Los arranques seguidos sin "anduvo bien" son arranques fallidos:
 *   2 seguidos -> modo seguro (sin sonido, USB activo: se puede reinstalar desde el instalador)
 *   4 seguidos -> modo de actualización del chip (USB 4C4A:8057, se rescata desde la computadora)
 * Prender y apagar el FM-1 cuatro veces seguidas (menos de 10 s cada vez) lleva también al modo
 * del chip, sin depender de ningún botón. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include "asm/system_reset_reason.h"
#include <string.h>
#include "fm1_hw.h"

#define HDR          16u                         /* "BM-BOOT-3" al principio del sector */
#define REC          32u
#define SLOTS        ((0x1000u - HDR) / REC)
#define FAILS_SAFE   2u
#define FAILS_UBOOT  4u

static uint32_t slot = SLOTS, fails;
static uint8_t etapas = 0xFFu;

uint32_t fm1_boot_fails(void) { return fails; }

static const char MAGIC[HDR] = "BM-BOOT-3";
static uint8_t raw[5], c1_retirado;

static uint8_t rd(uint32_t off)
{
    uint8_t b = 0xFFu;
    fm1_flash_read(FM1_BOOT_SEC + HDR + off, &b, 1);
    return b;
}

static void wr(uint32_t off, uint8_t v)
{
    fm1_flash_prog(FM1_BOOT_SEC + HDR + off, &v, 1);
}

/* sector nuevo: borrado y con la firma (lo que haya quedado del firmware anterior no cuenta) */
static void nuevo(void)
{
    fm1_flash_erase(FM1_BOOT_SEC);
    fm1_flash_prog(FM1_BOOT_SEC, MAGIC, HDR);
    slot = 0;
}

/* Modo de actualización del chip: el arranque (uboot.boot) lo toma de la marca
 * "usb_update_mode" en 0x01C7FD80 después de un reinicio (dato verificado en el FM-1 por X0X,
 * fm1_enter_uboot). Antes se deja este arranque como bueno para que, al salir del modo del chip
 * sin cambiar nada, el firmware vuelva a tener sus cuatro intentos. */
void fm1_enter_uboot(void)
{
    static const char k[16] = "usb_update_mode";
    volatile uint8_t *st = (volatile uint8_t *)0x01C7FD80u;
    uint32_t i;
    fm1_boot_ok();
    local_irq_disable();
    for (i = 0; i < 16u; i++)
        st[i] = (uint8_t)k[i];
    cpu_reset();
}

/* The SDK folds power-on, VDDIO low (brownout) and long press together; here each one is apart
 * (the brownout first: it is the one that matters). The raw bytes go in the record as well. */
static uint8_t motivo(void)
{
    int r = system_reset_reason_get();
    if (r & SYS_RST_WDT) return FM1_RST_WDT;
    if (r & SYS_RST_SOFT) return FM1_RST_SOFT;
    if (r & SYS_RST_VDDIO_LOW_PWR) return FM1_RST_LOWV;
    if (r & SYS_RST_LONG_PRESS) return FM1_RST_LONG;
    if (r & SYS_RST_VDDIO_PWR_ON) return FM1_RST_POWER;
    return FM1_RST_OTHER;
}

/* ------------------------------------------------------------------ breadcrumb in RAM */
/* The 8 bytes at 0x01C7FDF8 (the end of the SDK's update block) survive a reset that does not cut
 * the RAM; the watchdog (0xB0BA, fm1_scanhw.c) and the fault handler (0xB0BB, sdk/wl82_debug.c)
 * use them too and always win: they write over the breadcrumb.
 *   [0] 0xBC  [1] CRC-8 of [2..7]  [2..3] the main loop's step (fm1_miga)
 *   [4] lowest SYSVDD, (mV - 1000) / 4   [5] lowest VDC14, (mV - 1000) / 4
 *   [6] lowest VBAT, (mV - 2000) / 16     [7] uptime: seconds up to 127, then 128 + s / 30
 *   (0xFF in [4..6]: not measured yet) */
#define RASTRO_ADR 0x01C7FDF8u
#define RASTRO_MAG 0xBCu

static uint8_t crc8(const volatile uint8_t *p, int n)
{
    uint8_t c = 0x5Au;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++)
            c = (uint8_t)(c & 0x80u ? (c << 1) ^ 0x07u : c << 1);
    }
    return c;
}

static uint8_t sat8(int32_t v) { return (uint8_t)(v < 0 ? 0 : v > 254 ? 254 : v); }

void fm1_rastro(const char miga[2], uint16_t sysvdd_mv, uint16_t vdc14_mv, uint16_t vbat_mv, uint32_t ms)
{
    volatile uint8_t *m = (volatile uint8_t *)RASTRO_ADR;
    uint8_t b[6];
    uint32_t sec = ms / 1000u;
    if ((m[0] == 0xBAu || m[0] == 0xBBu) && m[1] == 0xB0u)   /* a watchdog or fault record: it stays */
        return;
    b[0] = (uint8_t)miga[0]; b[1] = (uint8_t)miga[1];
    b[2] = sysvdd_mv ? sat8(((int32_t)sysvdd_mv - 1000) / 4) : 0xFF;
    b[3] = vdc14_mv ? sat8(((int32_t)vdc14_mv - 1000) / 4) : 0xFF;
    b[4] = vbat_mv ? sat8(((int32_t)vbat_mv - 2000) / 16) : 0xFF;
    b[5] = sec < 128u ? (uint8_t)sec : sat8(128 + (int32_t)(sec / 30u));
    m[0] = 0;                                     /* invalid while it changes */
    for (int i = 0; i < 6; i++)
        m[2 + i] = b[i];
    m[1] = crc8(b, 6);
    m[0] = RASTRO_MAG;
}

static uint8_t rastro_prev[8], rastro_ok;

int fm1_rastro_previo(uint8_t out[8])
{
    for (int i = 0; i < 8; i++)
        out[i] = rastro_prev[i];
    return rastro_ok;
}

static void rastro_leer(void)
{
    volatile uint8_t *m = (volatile uint8_t *)RASTRO_ADR;
    if (m[0] != RASTRO_MAG || m[1] != crc8(m + 2, 6))
        return;
    for (int i = 0; i < 8; i++)
        rastro_prev[i] = m[i];
    rastro_ok = 1;
    m[0] = 0;
}

int fm1_boot_begin(int keys_safe)
{
    uint32_t i, k;
    uint8_t blk[64];
    rastro_leer();                            /* before anything writes those 8 bytes */
    raw[0] = p33_rx_1byte(P3_RST_SRC);
    raw[1] = (uint8_t)JL_SYSTEM->RST_SRC;
    raw[2] = p33_rx_1byte(R3_WKUP_SRC);
    raw[3] = p33_rx_1byte(P3_PMU_CON1);
    raw[4] = p33_rx_1byte(P3_VLVD_CON);
    fm1_flash_init();
    fm1_flash_read(FM1_BOOT_SEC, blk, HDR);
    if (memcmp(blk, MAGIC, HDR))
        nuevo();
    /* primer registro libre */
    slot = SLOTS;
    for (i = 0; i < SLOTS; i += 2u) {
        fm1_flash_read(FM1_BOOT_SEC + HDR + i * REC, blk, 64);
        for (k = 0; k < 2u && i + k < SLOTS && blk[k * REC] != 0xFFu; k++)
            ;
        if (k < 2u && i + k < SLOTS) {
            slot = i + k;
            break;
        }
    }
    /* arranques seguidos sin la marca de "anduvo bien" (slot == SLOTS: sector lleno) */
    fails = 0;
    for (i = slot; i > 0 && fails < 8u && rd((i - 1u) * REC + 1u) == 0xFFu; i--)
        fails++;
    uint8_t mot = motivo();
    /* core 1 stays retired across resets until the FM-1 is powered on from off */
    c1_retirado = slot > 0 && slot <= SLOTS && !(rd((slot - 1u) * REC + 13u) & FM1_BF_C1_RETIRED) && mot != FM1_RST_POWER;
    if (slot >= SLOTS)                        /* lleno: empezar de nuevo */
        nuevo();
    wr(slot * REC, 0);
    wr(slot * REC + 3u, mot);
    fm1_flash_prog(FM1_BOOT_SEC + HDR + slot * REC + 8u, raw, 5);
    if (rastro_ok) {
        fm1_flash_prog(FM1_BOOT_SEC + HDR + slot * REC + 16u, rastro_prev, 8);
        fm1_boot_flag(FM1_BF_RASTRO);
    }
    if (c1_retirado)
        fm1_boot_flag(FM1_BF_C1_RETIRED);
    if (fails >= FAILS_UBOOT)
        fm1_enter_uboot();
    return (keys_safe || fails >= FAILS_SAFE) ? FM1_BOOT_SAFE : FM1_BOOT_NORMAL;
}

void fm1_boot_ok(void)
{
    if (slot < SLOTS && rd(slot * REC + 1u) == 0xFFu)
        wr(slot * REC + 1u, 0);
}

void fm1_boot_flag(uint8_t f)
{
    uint8_t v;
    if (slot >= SLOTS)
        return;
    v = rd(slot * REC + 13u);
    if (v & f)
        wr(slot * REC + 13u, (uint8_t)(v & ~f));
}

int fm1_boot_c1_retired(void) { return c1_retirado; }

int fm1_boot_raw(int k, uint8_t rst[5], uint8_t *flags)
{
    if (slot >= SLOTS || k < 0 || (uint32_t)k > slot)
        return 0;
    fm1_flash_read(FM1_BOOT_SEC + HDR + (slot - (uint32_t)k) * REC + 8u, rst, 5);
    *flags = (uint8_t)~rd((slot - (uint32_t)k) * REC + 13u);
    return 1;
}

void fm1_boot_note(const char c[4])
{
    if (slot >= SLOTS)
        return;
    for (int i = 0; i < 4; i++)
        wr(slot * REC + 4u + (uint32_t)i, (uint8_t)c[i]);
}

void fm1_boot_stage(int n)                   /* etapa n (1..8) alcanzada en este arranque */
{
    uint8_t v;
    if (slot >= SLOTS || n < 1 || n > 8)
        return;
    v = (uint8_t)(0xFFu << n);
    if ((etapas & v) == etapas)
        return;
    etapas &= v;
    wr(slot * REC + 2u, etapas);
}

/* arranque k hacia atrás (1 = el anterior a este): etapa alcanzada, cómo terminó y si anduvo
 * bien. 0 si no hay registro. */
int fm1_boot_past(int k, uint8_t *etapa, uint8_t *fin, uint8_t *ok)
{
    uint8_t e;
    int n = 0;
    if (slot >= SLOTS || k < 1 || (uint32_t)k > slot)
        return 0;
    e = rd((slot - (uint32_t)k) * REC + 2u);
    while (n < 8 && !((e >> n) & 1u))
        n++;
    *etapa = (uint8_t)n;
    *fin = rd((slot - (uint32_t)k + 1u) * REC + 3u);   /* el motivo lo anota el arranque siguiente */
    *ok = rd((slot - (uint32_t)k) * REC + 1u) == 0;
    return 1;
}
