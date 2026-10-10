/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — constantes del sistema (SPEC_V3). Un solo lugar para los números. */
#ifndef BM_CONFIG_H
#define BM_CONFIG_H

#define BM_VERSION_STR "0.2.0"

/* ---- Audio (SPEC_V3 §1.4, §3.1) ---- */
#define BM_FS_OUT        44118u   /* ≈ 24 MHz / 544, fijo por hardware */
#define BM_OVERSAMPLE    2u       /* motores a Fs/2, salida ZOH x2 */
#define BM_FS            22059u   /* frecuencia de los motores */
#define BM_BLOCK         64u      /* frames de motor por bloque = 128 de salida = 2.9 ms */

/* ---- Secuenciador (§4) ---- */
#define BM_TRACKS        6
#define BM_STEPS         64
#define BM_PAGE_STEPS    16
#define BM_PAGES         4
#define BM_EVENTS        4        /* eventos por paso */
#define BM_PPQN          96
#define BM_TICKS_STEP    24       /* 96 PPQN / 4 semicorcheas */
#define BM_PATTERNS      16
#define BM_SONG_MAX      64
#define BM_PAT_MAX_EVENTS 768
#define BM_LANES         16       /* instrumentos de las pistas TR (T1 batería, T2 sampler) */
#define BM_TR_TRACKS     2
#define BM_SYN_TRACKS    4        /* T3..T6 */
#define BM_LANE_LOCKS    64       /* P-Locks por pista TR (lista dispersa) */
#define BM_LOOP_EVENTS   96       /* eventos de la loopera por synth */
#define BM_SCENES        4
#define BM_BAR_TICKS     (BM_PPQN * 4)

/* ---- Voces (§3.4) ---- */
#define BM_VOICES        12
#define BM_V_DRUM0       0        /* 0..3 reservadas a T1 */
#define BM_V_DRUM_N      4
#define BM_V_BASS        4        /* 4 reservada a T3 (mono) */
#define BM_V_POOL0       5        /* 5..11 pool compartido T2, T4-T6 */
#define BM_V_POOL_N      7
#define BM_STEAL_FADE    44       /* 2 ms a BM_FS */

/* ---- Memoria (§1.1: 96 KiB de RAM; ver el presupuesto en docs/MEMORIA.md) ----
 * Arena de 48 KiB compartida: normalmente delay (32 KiB) + punch-in (16 KiB);
 * mientras se graba un sample, toda la arena es el buffer de grabación. */
#define BM_ARENA_LEN     49152u
#define BM_DELAY_LEN     16384u   /* int16 a BM_FS/2 ≈ 1.48 s */
#define BM_FXBUF_LEN     16384u   /* mu-law 8 bit a BM_FS ≈ 0.74 s (stutter/reverse) */
#define BM_SMP_RATE      16000u   /* sampler a 16 kHz, 8 bit */
#define BM_SMP_REC_MAX   BM_ARENA_LEN   /* ≈ 3.07 s grabando */
#define BM_SLICES        16
#define BM_PENDING_LEN   8192u    /* caché de un patrón sucio en formato disperso */
#define BM_CMDQ_LEN      128

/* ---- UI (§6) ---- */
#define UI_HOLD_MS       300u
#define UI_LONG_MS       2000u   /* pulsación larga: pantallas especiales */
#define UI_OVERLAY_MS    1500u
#define UI_TOAST_MS      1500u
#define UI_FPS           30u
#define UI_AUTOSAVE_MS   10000u
#define UI_BOOT_MS       1200u

/* ---- Pantalla (§1.2, §6.3) ---- */
#define BM_LCD_W         240
#define BM_LCD_H         240
#define BM_STATUS_H      24
#define BM_CTX_Y         24
#define BM_CTX_H         176
#define BM_LEGEND_Y      200
#define BM_LEGEND_H      40
#define BM_STRIP_H       8        /* franja de render */
#define BM_TILE_W        60       /* 4 tiles por franja: solo se envía lo que cambió */

/* ---- Flash (§4.4) ---- */
#define BM_FLASH_SIZE    0x100000u
#define BM_SECTOR        0x1000u
#define BM_PAGE          0x100u
#define BM_STORE_LO      0x097000u
#define BM_STORE_HI      0x0C0000u  /* log de patrones: 20 slots de 8 KiB */
#define BM_SMP_LO        0x0C0000u  /* 2 slots de sample de 64 KiB */
#define BM_SMP_SLOT      0x10000u
#define BM_SMP_SLOTS     2
#define BM_SMP_HDR       128u
#define BM_SMP_MAX       (BM_SMP_SLOT - BM_SMP_HDR)   /* 65408 muestras ≈ 4.09 s */
#define BM_GLOB_LO       0x0FC000u
#define BM_GLOB_HI       0x0FF000u
#define BM_SLOT_SIZE     0x2000u  /* 8 KiB = 2 sectores */

#endif
