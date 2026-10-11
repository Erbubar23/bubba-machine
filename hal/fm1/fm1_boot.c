/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Arranques fallidos, modo seguro, registro de hasta dónde llegó cada arranque y salida al modo
 * de actualización del chip.
 *
 * Cada arranque usa 8 bytes del sector FM1_BOOT_SEC:
 *   [0] empezó (0x00)         [1] anduvo bien (0x00, a los 10 s)
 *   [2] etapas: cada etapa alcanzada pone en 0 un bit más (0xFF -> 0xFE -> 0xFC ...)
 *   [3] por qué se reinició el chip antes de este arranque (FM1_RST_*)
 * Programar bits de 1 a 0 no necesita borrar: el sector se borra una vez cada 510 arranques.
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

#define HDR          16u                         /* "BM-BOOT-2" al principio del sector */
#define REC          8u
#define SLOTS        ((0x1000u - HDR) / REC)
#define FAILS_SAFE   2u
#define FAILS_UBOOT  4u

static uint32_t slot = SLOTS, fails;
static uint8_t etapas = 0xFFu;

uint32_t fm1_boot_fails(void) { return fails; }

static const char MAGIC[HDR] = "BM-BOOT-2";

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

static uint8_t motivo(void)
{
    int r = system_reset_reason_get();
    if (r & SYS_RST_WDT) return FM1_RST_WDT;
    if (r & SYS_RST_SOFT) return FM1_RST_SOFT;
    if (r & (SYS_RST_VDDIO_PWR_ON | SYS_RST_VDDIO_LOW_PWR | SYS_RST_LONG_PRESS)) return FM1_RST_POWER;
    return FM1_RST_OTHER;
}

int fm1_boot_begin(int keys_safe)
{
    uint32_t i, k;
    uint8_t blk[64];
    fm1_flash_init();
    fm1_flash_read(FM1_BOOT_SEC, blk, HDR);
    if (memcmp(blk, MAGIC, HDR))
        nuevo();
    /* primer registro libre */
    slot = SLOTS;
    for (i = 0; i < SLOTS; i += 8u) {
        fm1_flash_read(FM1_BOOT_SEC + HDR + i * REC, blk, 64);
        for (k = 0; k < 8u && i + k < SLOTS && blk[k * REC] != 0xFFu; k++)
            ;
        if (k < 8u && i + k < SLOTS) {
            slot = i + k;
            break;
        }
    }
    /* arranques seguidos sin la marca de "anduvo bien" (slot == SLOTS: sector lleno) */
    fails = 0;
    for (i = slot; i > 0 && fails < 8u && rd((i - 1u) * REC + 1u) == 0xFFu; i--)
        fails++;
    if (slot >= SLOTS)                        /* lleno: empezar de nuevo */
        nuevo();
    wr(slot * REC, 0);
    wr(slot * REC + 3u, motivo());
    if (fails >= FAILS_UBOOT)
        fm1_enter_uboot();
    return (keys_safe || fails >= FAILS_SAFE) ? FM1_BOOT_SAFE : FM1_BOOT_NORMAL;
}

void fm1_boot_ok(void)
{
    if (slot < SLOTS && rd(slot * REC + 1u) == 0xFFu)
        wr(slot * REC + 1u, 0);
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
