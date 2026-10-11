# Qué funciona en el FM-1 y por qué (investigación, 2026-10-11)

Resumen de lo que dicen las fuentes reales sobre cómo se hace un firmware que **ya funciona** en el FM-1, comparado con lo que hizo Bubba Machine en su primera prueba. Solo hechos con su fuente; lo que es inferencia está marcado como tal.

## Las fuentes

| Fuente | Qué es | Licencia |
|---|---|---|
| **Felucca** (`hugelton/Felucca`) | firmware completo, funciona en el aparato, con instalador web | GPL-3.0-only: solo se toman hechos de hardware |
| **X0X** (`charlesvestal/fm1-x0x`) | derivado de Felucca, funciona en el aparato (es el que tenía el FM-1 del autor) | GPL-3.0: solo hechos |
| **FM-1-RE** | ingeniería inversa del firmware de fábrica (V13/V14/V15), con direcciones | WTFPL |
| **SDK AC79 de JieLi** V1.2.1 | el SDK con el que M-VAVE hizo el firmware de fábrica | Apache-2.0, pero el núcleo (`cpu.a`, `system.a`…) es cerrado |
| **jl-uboot-tool / jl-misctools** (kagaimiq) | protocolo del modo del chip, formato de paquetes | MIT |

## Cómo están hechos los firmwares que funcionan

**Felucca y X0X no usan las bibliotecas del SDK.** El programa es propio de punta a punta (arranque `crt0.S`, guion de enlace `app.ld`, interrupciones, y un driver propio para cada periférico, escrito a nivel de registro). Del SDK solo usan **tres archivos para armar el paquete**: `cpu/wl82/tools/uboot.boot`, `cfg_tool.bin` y `cfg/eq_cfg_hw.bin` (Felucca, `BUILDING.md` y `README.md`, sección de créditos). El paquete es su app más su propio cargador de actualización (`loader/ota.bin`).

Lo que cada uno hace a mano, según sus encabezados (`firmware/hal/*.h`):

| Pieza | Cómo lo hacen | Fuente |
|---|---|---|
| USB | controlador USB0 por registros (`0x11800` CON0, `0x11804` CON1, `0x51000` IO_CON0), reloj del USB = PLL 48 MHz (`CLK_CON1 &= ~3`), pull-up de D+ en IO_CON0 bit 6, todo **sin interrupción**: se sondea desde la interrupción de TIMER5 | X0X `fm1_usb.h` |
| Audio | ALNK0 (I2S) hacia un codec externo, canal 3, 24 bits, buffer doble; líneas del codec PC0, PC1, PC2, PC6 | Felucca `fm1_audio.h` |
| Pantalla | SPI1 a 12 MHz, ST7789V | Felucca/X0X `fm1_lcd_hw.h` |
| Teclas | matriz 11×6 con 74HC595, escaneada desde TIMER5 | `fm1_input.h` |
| Flash | rutinas en RAM con el XIP apagado | `fm1_flash.h`, `fm1_xip.h` |
| Actualización | el firmware deja listo el cargador oficial (o el propio) y reinicia | X0X `ota.c`, FM-1-RE `11-ota-protocol.md` |

## Qué pasó con Bubba Machine (primera prueba)

Bubba Machine se armó **sobre las bibliotecas cerradas del SDK** (el mismo camino que el firmware de fábrica, pero sin la configuración ni la licencia de M-VAVE). En el aparato:

1. El instalador web y el cargador oficial escribieron bien (USB `4D4A:4155`, registro de Windows 05:56:03).
2. El firmware arrancó: la pantalla y la flash funcionan (se vio el **modo seguro**).
3. El modo normal se reinició solo dos veces antes de los 10 s.
4. **El USB del firmware nunca apareció** en el computador, ni en modo seguro. Tampoco después de la corrección que lo enciende al arrancar (no hubo ningún dispositivo USB nuevo en el registro de Windows después de las 06:13).
5. El **modo del chip** sí funcionó (OCT− + OCT+ 5 s → `WL82 UBOOT1.00`, `4C4A:8057`), y `rescate.py` leyó la flash entera dos veces, igual las dos.

Lo que se encontró en el código cerrado del SDK, desensamblando nuestro propio `sdk.elf`:

- **`__initcall_sdk_meky_check`**: una comprobación de llave de JieLi que se agrega sola al arrancar y corre **cada 8 segundos** (`sdk_meky_check` → `__sys_timer_add(_mkey_check, 8000)`). Busca una estructura con la firma `JKYS` (`0x53594B4A`) y la compara con datos de `boot_info`. Cuando no coincide, escribe sobre la RAM de `0x01C7FE00` y se vuelve a llamar a sí misma. *Inferencia*: un firmware hecho con el SDK público y sin la llave del fabricante se cae a los ~8 s, lo que coincide con los reinicios antes de los 10 s. No se pudo comprobar en el aparato (no hay consola).
- El arranque del USB del SDK (`usb_g_sie_init`) hace la misma secuencia de registros que X0X, pero **no fija el reloj del USB** (`CLK_CON1`), que en el SDK depende de su configuración de relojes y de la detección `otg`.

## Conclusión

El camino probado en el aparato es el de Felucca y X0X: **programa propio sin las bibliotecas del SDK**, con el arranque del chip de M-VAVE y los tres archivos de paquete del SDK. El camino del SDK depende de piezas cerradas (llave, detección USB, relojes) que no se pueden ver ni arreglar, y en la prueba falló justo en esas piezas.

Por eso el port pasa a ser **sin SDK**: el núcleo (`core/`, C portable) queda igual y la capa `hal/fm1/` se reescribe a nivel de registro con los hechos de esta tabla, con código propio. Se prueba por partes en el aparato, en este orden: pantalla → teclas → USB-MIDI (lo que permite reinstalar sin el modo del chip) → audio → flash → actualización. Mientras tanto, el modo del chip y `tools/fm1/rescate.py` son la vía para volver al firmware de fábrica.
