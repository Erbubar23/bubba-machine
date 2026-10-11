/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Flash del FM-1 con la API norflash del SDK, y una sola puerta para escribir:
 * fuera de [FM1_W_LO, FM1_W_HI) no se borra ni se programa nada, venga de donde venga.
 *
 * Lectura directa (XIP, fm1_flash_map): el controlador de flash descifra todo lo que lee por el
 * mapeo, salvo una "ventana sin cifrar" (JL_SFCENC: UNENC_ADRL = inicio mapeado, UNENC_ADRH =
 * 0x7FFFFFF, CON bit 1). Los datos del firmware se escriben sin cifrar, así que la zona de datos
 * tiene que estar dentro de esa ventana; si no, el núcleo lee basura (comprobado en el aparato
 * con la consola: "BM-BOOT-2" en 0xBF000 se leía 4d 53 30 59… por el mapeo, y un patrón guardado
 * leído así tiraba una excepción al dar PLAY). El SDK pone la ventana desde el inicio de su
 * almacén (norflash_ioctl IOCTL_SET_VM_INFO); el firmware de fábrica la tenía desde 0x97000, y
 * como el empaquetador movió el almacén a 0xE5000, aquí se vuelve a poner desde 0x97000 (dato
 * también de Felucca, fm1_flash.h fl_plain_window_init). Después de borrar o escribir se
 * invalida la caché de esas líneas para no leer lo viejo. */
#include "app_config.h"
#include "system/includes.h"
#include "asm/sfc_norflash_api.h"
#include "asm/cache.h"
#include "fm1_hw.h"

static int abierta;

void fm1_flash_plain_window(void)
{
    CPU_CRITICAL_ENTER();
    JL_SFCENC->CON &= ~2u;
    JL_SFCENC->UNENC_ADRL = (u32)flash_addr2cpu_addr(FM1_W_LO);
    JL_SFCENC->UNENC_ADRH = 0x07FFFFFFu;
    JL_SFCENC->CON |= 2u;
    CPU_CRITICAL_EXIT();
    flushinv_dcache((void *)flash_addr2cpu_addr(FM1_W_LO), 0x100000 - FM1_W_LO);
}

int fm1_flash_init(void)
{
    if (!abierta) {
        norflash_open(NULL, NULL, NULL);
        fm1_flash_plain_window();
        abierta = 1;
    }
    return 0;
}

static void inval(uint32_t a, uint32_t n)
{
    flushinv_dcache((void *)flash_addr2cpu_addr(a & ~31u), (int)(((a & 31u) + n + 31u) & ~31u));
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
    int r = norflash_ioctl(NULL, IOCTL_ERASE_SECTOR, a) < 0 ? -1 : 0;
    inval(a, 0x1000u);
    return r;
}

int fm1_flash_prog(uint32_t a, const void *s, uint32_t n)
{
    if (!n)
        return 0;
    if (!dentro(a, n))
        return -1;
    fm1_flash_init();
    norflash_ioctl(NULL, IOCTL_SET_WRITE_PROTECT, 0);
    int r = norflash_write(NULL, (void *)s, n, a) < 0 ? -1 : 0;
    inval(a, n);
    return r;
}

const uint8_t *fm1_flash_map(uint32_t a)
{
    return (const uint8_t *)flash_addr2cpu_addr(a);
}
