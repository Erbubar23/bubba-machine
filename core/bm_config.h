/* SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Erbubar23 */
/* Bubba Machine — constantes del sistema (SPEC_V3). Un solo lugar para los números. */
#ifndef BM_CONFIG_H
#define BM_CONFIG_H

#define BM_VERSION_STR "0.9.0-beta"
#define BM_FMT           3        /* generación de ajustes y tomas (0.3 no lee los de 0.2) */
#define BM_PAT_FMT       8        /* formato de patrones. Hasta la v1 no se leen formatos anteriores */
#define BM_PRM           48       /* parámetros de sonido por canal, además de las 4 macros (= BM_SNAP_PRM) */

/* ---- Audio (SPEC_V3 §1.4, §3.1) ---- */
#define BM_FS_OUT        44118u   /* ≈ 24 MHz / 544, fijo por hardware */
#define BM_OVERSAMPLE    1u       /* 0.4: los motores corren a la frecuencia de salida (antes Fs/2) */
#define BM_FS            44118u   /* frecuencia de los motores y del bus de mezcla (float) */
#define BM_ROM_RATE      22059u   /* frecuencia de los kits de la ROM (tools/gen_assets.py) */
#define BM_BLOCK         64u      /* frames por bloque = 1.45 ms */

/* ---- Secuenciador (§4) ---- */
#define BM_TRACKS        6
#define BM_STEPS         64
#define BM_PAGE_STEPS    16
#define BM_PAGES         4
#define BM_EVENTS        4        /* eventos por paso */
#define BM_PPQN          96
#define BM_TICKS_STEP    24       /* 96 PPQN / 4 semicorcheas */
#define BM_PATTERNS      10       /* 0.9: W1-W10 en HOME (los objetos 10-15 de la flash se ignoran) */
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
#define BM_VOICES        22
#define BM_V_DRUM0       0        /* 0..5 reservadas a T1 */
#define BM_V_DRUM_N      6
#define BM_V_POOL0       6        /* 6..21 pool compartido T2-T6, con un tope por máquina (bm_engines.c) */
#define BM_V_POOL_N      16
#define BM_STEAL_FADE    88       /* 2 ms a BM_FS */

/* ---- Memoria (docs/TRAZA_FM1.md) ----
 * Arena de 128 KiB compartida: normalmente delay (64 KiB) + captura del punch-in (64 KiB);
 * mientras se graba un sample, la arena es el buffer de grabación. */
#define BM_DELAY_LEN     17648u   /* int16 a BM_FS/2 = 0.80 s (el delay filtra a 11 kHz, como uno de cinta) */
#define BM_FXBUF_LEN     32768u   /* int16 a BM_FS ≈ 0.74 s (stutter/reverse/tape) */
#define BM_ARENA_LEN     (BM_DELAY_LEN * 2u + BM_FXBUF_LEN * 2u)
#define BM_SMP_RATE      16000u   /* sampler a 16 kHz, 8 bit */
#define BM_SMP_REC_MAX   49152u   /* ≈ 3.07 s grabando: la RAM de grabación (la zona la comparten tomas y sonidos) */
#define BM_SLICES        16       /* cortes de un loop del sampler (uno por blanca) */
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
#define UI_ENTER_MS      400u    /* recuadro de ALGORITHM en HOME antes de entrar al canal */

/* ---- Pantalla (§1.2, §6.3) ---- */
#define BM_LCD_W         240
#define BM_LCD_H         240
/* 0.6: cuatro franjas (docs/DISENO_0.6.md §5.2) */
#define BM_STATUS_H      24       /* cabecera: máquina, PLAY/REC, patrón, BPM, CPU */
#define BM_CTX_Y         24       /* visor: lo propio de cada pantalla */
#define BM_CTX_H         112
#define BM_KEYS_Y        136      /* teclado: negras 136-149, blancas = pasos 151-167 */
#define BM_KEYS_H        32
#define BM_LEGEND_Y      168      /* perillas K1-K4: dial, valor y nombre */
#define BM_LEGEND_H      72
#define BM_STRIP_H       8        /* franja de render */
#define BM_TILE_W        60       /* 4 tiles por franja: solo se envía lo que cambió */

/* ---- Flash (§4.4) ---- */
#define BM_FLASH_SIZE    0x100000u
#define BM_SECTOR        0x1000u
#define BM_PAGE          0x100u
/* Mapa de la zona de datos (hal/fm1/fm1_hw.h): patrones, registro de arranques (0xBF000),
 * tomas, idioma (0xD9000), globales, cargador en espera (0xE0000) y ajustes del SDK (0xE5000-0xECFFF). */
#define BM_STORE_LO      0x097000u
#define BM_STORE_HI      0x0BF000u  /* log de patrones: 20 slots de 8 KiB */
#define BM_TAKE_LO       0x0C0000u  /* zona compartida: tomas desde abajo y snapshots del usuario desde arriba, 100 KiB */
#define BM_TAKE_HI       0x0D9000u
#define BM_LANG_LO       0x0D9000u  /* paquete de idioma (bm_lang.h): 16 KiB */
#define BM_LANG_HI       0x0DD000u
#define BM_TAKE_HDR      128u       /* cabecera al inicio del primer sector de cada toma */
#define BM_TAKES         8          /* tomas como máximo */
#define BM_TAKES_MAX_LEN 96000u     /* 6 s de tomas en total (16 kHz, 8 bit); el resto de la zona es de los sonidos */
#define BM_GLOB_LO       0x0DD000u
#define BM_GLOB_HI       0x0E0000u
#define BM_SLOT_SIZE     0x2000u  /* 8 KiB = 2 sectores */

#endif
