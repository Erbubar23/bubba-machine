# Port al FM-1 — plan de bring-up

**Estado:** esqueleto. `fm1_main.c` tiene la estructura completa (bucle principal, ISR de audio y de escaneo, plataforma) y `fm1_scan.c` la lógica de matriz probada en PC. Todo acceso a registros está marcado `TODO(HW-VERIFY)`. **Nada de esto se ha compilado con el toolchain de JieLi ni probado en un FM-1.**

Regla del proyecto: el código es propio. Para los registros se usa el **SDK de JieLi AC79 (Apache-2.0)** y los datasheets; de Felucca solo hechos (pines, tiempos, mapa de memoria), nunca su código.

## 0. Antes de tocar nada

1. **Respaldo completo de la flash** (1 MiB) con un método ya probado: modo boot del chip (`WL80UBOOT` / USB `4C4A:8057`) o el *FM-1 Transporter* (RP2040).
2. Confirmar que se puede volver al firmware de fábrica con el actualizador de M-VAVE.
3. **Nunca escribir** fuera de `0x097000-0x0DFFFF` y `0x0FC000-0x0FEFFF` (SPEC §4.4). La HAL simulada ya lo bloquea; el driver real debe hacer lo mismo.

## 1. Toolchain y paquete

- Compilador: JieLi clang 4.0.1 para pi32v2. SDK `AC79NN_SDK_V1.2.1_2023-12-13`.
- Paquete `.fwsc` = app + loader + `uboot.boot`, `cfg_tool.bin` y `eq_cfg_hw.bin` del SDK (Apache-2.0, incluir sus licencias), identidad `FM-1_910`.
- Linker: código XIP desde `0x02000000`, RAM en `0x01C08000` (96 KiB), RAM ejecutable en `0x01C00000` (24 KiB) para `.ram_text`.

## 2. Orden de bring-up (cada paso se prueba antes del siguiente)

| Paso | Qué | Cómo se sabe que funciona |
|---|---|---|
| 1 | Arranque, relojes, `.data/.bss`, un LED | Un LED parpadea a 1 Hz |
| 2 | TIMER4 libre a 24 MHz → `millis()` | El parpadeo dura lo que debe |
| 3 | SPI1 + ST7789V (PC9 CLK, PC10 MOSI, PC7 CS, PC8 D/C, PA2 BL activo en bajo) | `gfx_logo` sale en pantalla |
| 4 | Matriz: 74HC595 + filas PA0, PA5-PA8, PB7, TIMER5 a 10 kHz | La UI responde; calibrar `fm1_scan_map` (ver §3) |
| 5 | ADC 10 bits: MASTER PB6 (canal 4), batería PB1 (canal 3, solo analógica) | La barra de batería y el MASTER se mueven |
| 6 | I2S ALNK0 + codec (control PC0/1/2/6), DMA de doble buffer, 128 frames, 24 bit alineado a la izquierda | La demo suena con PLAY |
| 7 | Flash: borrado (0x20) y programación (0x02) en `.ram_text` con IRQ apagadas | Guardar, apagar, prender: el patrón sigue ahí |
| 8 | USB-MIDI (instalación + clock) y UART del MIDI TRS | Reloj externo y notas por MIDI |

## 3. Calibrar la matriz

Los IDs de botones (0-13) y teclas (14-40) son hechos de hardware, pero el cruce exacto fila/columna de cada uno se mide en el equipo:

1. Firmware de diagnóstico: muestra en pantalla la columna y la fila de cada contacto que se cierra.
2. Pulsar cada botón y tecla en orden y anotar el cruce en `fm1_scan_map.id`.
3. Girar cada encoder y anotar sus contactos A y B en `enc_a` / `enc_b`.
4. Guardar la tabla en flash (globales) para no recompilar.

## 4. Medir

- CPU del audio por bloque (meta < 85 %). Si se pasa: `bm_audio_overload()` mata voces del pool; luego bajar `BM_VOICES` o pasar los synths a la mitad de frecuencia.
- RAM libre real (mapa del linker) contra `docs/MEMORIA.md`.
- Que el SPI quepa en ~37 KB por frame (las pruebas de PC dan 3.7 KB de promedio).
