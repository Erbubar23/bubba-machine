/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Arranques fallidos, modo seguro y salida al modo de actualización del chip.
 *
 * Cada arranque deja una marca en el sector FM1_BOOT_SEC (2 bytes por arranque: "empezó" y,
 * a los FM1_BOOT_OK_MS, "anduvo bien"). Los arranques seguidos sin la segunda marca son
 * arranques fallidos: un cuelgue, un reinicio del watchdog o apagarlo enseguida.
 *   2 seguidos -> modo seguro (sin sonido, USB activo: se puede reinstalar desde el instalador)
 *   4 seguidos -> modo de actualización del chip (USB 4C4A:8057, se rescata desde la computadora)
 * Prender y apagar el FM-1 cuatro veces seguidas (menos de 10 s cada vez) lleva también al modo
 * del chip, sin depender de ningún botón.
 *
 * Programar un byte de 0xFF a 0x00 no necesita borrar: el sector se borra una vez cada 2048
 * arranques. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/includes.h"
#include <string.h>
#include "fm1_hw.h"

#define HDR          16u                         /* "BM-BOOT-1" al principio del sector */
#define SLOTS        ((0x1000u - HDR) / 2u)
#define FAILS_SAFE   2u
#define FAILS_UBOOT  4u

static uint32_t slot = SLOTS, fails;

uint32_t fm1_boot_fails(void) { return fails; }

static const char MAGIC[HDR] = "BM-BOOT-1";

static uint8_t rd(uint32_t i)
{
    uint8_t b = 0;
    fm1_flash_read(FM1_BOOT_SEC + HDR + i, &b, 1);
    return b;
}

static void mark(uint32_t i)
{
    static const uint8_t zero = 0;
    fm1_flash_prog(FM1_BOOT_SEC + HDR + i, &zero, 1);
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

int fm1_boot_begin(int keys_safe)
{
    uint32_t i, k;
    uint8_t blk[64];
    fm1_flash_init();
    fm1_flash_read(FM1_BOOT_SEC, blk, HDR);
    if (memcmp(blk, MAGIC, HDR))
        nuevo();
    /* primer par libre */
    slot = SLOTS;
    for (i = 0; i < SLOTS; i += 32u) {
        fm1_flash_read(FM1_BOOT_SEC + HDR + i * 2u, blk, 64);
        for (k = 0; k < 32u && i + k < SLOTS && blk[k * 2u] != 0xFFu; k++)
            ;
        if (k < 32u && i + k < SLOTS) {
            slot = i + k;
            break;
        }
    }
    /* arranques seguidos sin la marca de "anduvo bien" (slot == SLOTS: sector lleno) */
    fails = 0;
    for (i = slot; i > 0 && fails < 8u && rd((i - 1u) * 2u + 1u) == 0xFFu; i--)
        fails++;
    if (slot >= SLOTS)                        /* lleno: empezar de nuevo */
        nuevo();
    mark(slot * 2u);
    if (fails >= FAILS_UBOOT)
        fm1_enter_uboot();
    return (keys_safe || fails >= FAILS_SAFE) ? FM1_BOOT_SAFE : FM1_BOOT_NORMAL;
}

void fm1_boot_ok(void)
{
    if (slot < SLOTS && rd(slot * 2u + 1u) == 0xFFu)
        mark(slot * 2u + 1u);
}
