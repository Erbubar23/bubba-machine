/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Configuración del SDK AC79 de JieLi para Bubba Machine en el FM-1.
 * Los nombres de las opciones son los del SDK (apps/demo/demo_hello/include/app_config.h). */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Lugar para el programa: app.bin vive entre la lista de la zona de la app (0x4120) y los
 * datos fijos del paquete (cfg_tool.bin y cfg), dentro de los 0x93000 bytes que escribe el
 * cargador oficial. Caben 582120 bytes (tools/fm1/fwsc.py info); con este límite el
 * enlazador falla antes de que salga un firmware que no entra. */
#define __FLASH_SIZE__    (568 * 1024)
#define __SDRAM_SIZE__    0                   /* el FM-1 no tiene SDRAM */

/* Energía: no se toca. El arranque del chip (uboot.boot de M-VAVE) ya la deja lista y el
 * firmware no llama a power_init(): así no se cambia nada que dependa de la placa. */
#define TCFG_LOWPOWER_BTOSC_DISABLE  0
#define TCFG_LOWPOWER_LOWPOWER_SEL   0
#define TCFG_LOWPOWER_VDDIOM_LEVEL   VDDIOM_VOL_32V
#define TCFG_LOWPOWER_VDDIOW_LEVEL   VDDIOW_VOL_21V
#define VDC14_VOL_SEL_LEVEL          VDC14_VOL_SEL_140V
#define SYSVDD_VOL_SEL_LEVEL         SYSVDD_VOL_SEL_126V

/* Sin UART de depuración: el SDK la saca por PB03 y no sabemos qué hay en ese pin en el FM-1. */
#undef CONFIG_DEBUG_ENABLE
#define LIB_DEBUG    0
#define CONFIG_DEBUG_LIB(x)         (x & LIB_DEBUG)

/* USB: el USB0 de velocidad completa como dispositivo; la clase MIDI es la de hal/fm1. */
#define TCFG_PC_ENABLE               1
#define USB_PC_NO_APP_MODE           2
#define USB_MALLOC_ENABLE            1
#define USB_DEVICE_CLASS_CONFIG      0
#define TCFG_FUSB_EN                 1
#define TCFG_HUSB_EN                 0
#include "usb_std_class_def.h"
#include "usb_common_def.h"

#endif
