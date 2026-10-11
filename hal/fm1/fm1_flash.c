/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Flash del FM-1 con la API norflash del SDK, y una sola puerta para escribir:
 * fuera de [FM1_W_LO, FM1_W_HI) no se borra ni se programa nada, venga de donde venga. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/sfc_norflash_api.h"
#include "fm1_hw.h"

static int abierta;

int fm1_flash_init(void)
{
    if (!abierta) {
        norflash_open(NULL, NULL, NULL);
        abierta = 1;
    }
    return 0;
}

static int dentro(uint32_t a, uint32_t n)
{
    return a >= FM1_W_LO && n <= FM1_W_HI - FM1_W_LO && a + n <= FM1_W_HI;
}

int fm1_flash_read(uint32_t a, void *d, uint32_t n)
{
    if (a >= 0x100000u || n > 0x100000u - a)
        return -1;
    fm1_flash_init();
    return norflash_origin_read(d, a, n) < 0 ? -1 : 0;
}

int fm1_flash_erase(uint32_t a)
{
    if ((a & 0xFFFu) || !dentro(a, 0x1000u))
        return -1;
    fm1_flash_init();
    norflash_ioctl(NULL, IOCTL_SET_WRITE_PROTECT, 0);
    return norflash_ioctl(NULL, IOCTL_ERASE_SECTOR, a) < 0 ? -1 : 0;
}

int fm1_flash_prog(uint32_t a, const void *s, uint32_t n)
{
    if (!n)
        return 0;
    if (!dentro(a, n))
        return -1;
    fm1_flash_init();
    norflash_ioctl(NULL, IOCTL_SET_WRITE_PROTECT, 0);
    return norflash_write(NULL, (void *)s, n, a) < 0 ? -1 : 0;
}

const uint8_t *fm1_flash_map(uint32_t a)
{
    return (const uint8_t *)flash_addr2cpu_addr(a);
}
