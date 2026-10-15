# El procesador y la memoria del FM-1: investigación para la reingeniería

Octubre de 2026. Qué tiene el chip del FM-1 a nivel de procesador y memoria, cómo lo usan el firmware de fábrica y los proyectos que mejor lo conocen, y qué cambia en Bubba Machine para sacarle el máximo. Cada dato lleva su fuente; lo inferido está marcado.

## 1. El chip: JieLi AC791N (familia WL82)

| | Dato | Fuente |
|---|---|---|
| CPU | **2 núcleos** pi32v2, RISC de 32 bits **con FPU** (precisión simple) | hojas de datos oficiales AC791N (en el SDK, `doc/datasheet`); Melodee usa la FPU |
| Frecuencia | **«DC-320MHz operation»** en todas las variantes; el SDK trae además un modo oficial de **sobrerreloj a 396 MHz** (`CONFIG_OVERCLOCKING_ENABLE`: `SYS_CLK=396MHz`, `HSB_DIV=1`, `LSB_DIV=1`) | hojas de datos; documentación del SDK, «功耗优化说明» |
| Interrupciones | **128 vectorizadas, 4 niveles de prioridad** | hojas de datos |
| ISA | derivada de Blackfin: instrucciones de 16/32/48 bits, **ejecución en paralelo** (`ins1 # ins2`), MAC 16×16→64, saturación, operaciones empaquetadas de media palabra, bucles `rep` sin costo, registros de 64 bits por pares | kagaimiq/jielie `cpu/pi32v2.md`; Quarkslab |
| SRAM | **578 KiB** en el chip | descripción oficial del SDK (Gitee) |
| Cachés | **32 KiB de instrucciones + 32 KiB de datos**, 8 vías de 4 KiB cada una | descripción oficial; `sdk_ld.c` |
| Flash | 1 MiB SPI NOR, se ejecuta en sitio (XIP) por el SFC, **cifrada** salvo una ventana | FM-1-RE; medido con la consola |
| Bus | `hsb_clk ≤ 160 MHz`, `sfc_clk ≤ 80 MHz`, `lsb_clk ≤ 80 MHz` | comentario de `system_vdd_clock.c` en el SDK |

### Lo que corre hoy en el FM-1 (medido con la consola de Bubba)

| | Valor |
|---|---|
| Reloj del sistema | **360 MHz** (`clk_get("sys")`); bus lento 60 MHz |
| De dónde viene | el `isd_config.ini` de M-VAVE dentro del paquete dice `SYS_CLK = 0x15752A00` = 360 MHz; el arranque del chip lo aplica y Bubba lo hereda |
| Firmware de fábrica | baja a **240 MHz** con su propia rutina de PLL (FM-1-RE `io/01-boot.md` §5.1) |

360 MHz está por encima del máximo de la hoja de datos (320) pero por debajo del sobrerreloj que el propio SDK contempla (396). Anduvo estable en todas las pruebas de hoy. Es una decisión a tomar: más potencia contra margen térmico y batería.

## 2. Memoria

### Mapa de RAM (Bubba hoy, `build/fm1/elf/sdk.ld` y el mapa del enlazador)

| Zona | Dirección | Tamaño | Uso |
|---|---|---|---|
| RAM0 | `0x01C00000` | ~511 KiB | `.data`, `.bss` y el montón del SDK |
| — de eso, Bubba | | ~140 KiB | núcleo (~81 KiB), buffers, pilas |
| boot_info, registro de actualización, vectores | `0x01C7FD4C`–`0x01C7FFFF` | | reservado (el perro guardián usa `0x01C7FDF8`) |
| **Cache RAM** | `0x01F28000` | **28 KiB** | 7 de las 8 vías de la caché de datos convertidas en RAM rápida (`FREE_DACHE_WAY = 7`) |

### Un hallazgo: la caché de datos casi no existe

Con la configuración por defecto del SDK (`FREE_DACHE_WAY = 7`), la caché de datos queda en **una sola vía de 4 KiB**: el resto es «cache RAM». Toda lectura de datos desde la flash pasa por esa vía: los samples de los kits, las tablas de ondas, las constantes. En el audio eso significa fallos de caché continuos y esperas al SFC (≤ 80 MHz, 4 líneas). *Inferencia a medir*: es probablemente el mayor freno escondido del cálculo de audio. Hay dos salidas, que se pueden combinar:

- **Más vías de caché de datos** (`FREE_DACHE_WAY` más bajo): menos cache RAM, más caché.
- **Lo caliente en RAM**: copiar a RAM los samples y tablas en uso (hay ~370 KiB libres) y leer de ahí.

### Código en RAM

El SDK lo prevé: `SEC_USED(.volatile_ram_code)` antes de una función la ubica en la RAM interna (documentación «代码运行在FLASH或SDRAM配置方法»). Lo usan:

- **El firmware de fábrica**: los caminos calientes (interrupciones, bloqueos, caché, flash) corren en RAM (FM-1-RE `io/01-boot.md`).
- **Felucca**: reserva 24 KiB de `.ram_text` (FM-1-RE y su `app.ld`).
- **El SDK**: sus rutinas de flash ya están en RAM (`_norflash_write`, `norflash_erase` en `0x01C0…`, comprobado en nuestro binario). Nuestras interrupciones (`scan_isr`, `alnk0_isr`) y el cálculo de audio, en cambio, corren desde la flash.

### Flash

| Zona | Uso |
|---|---|
| `0x00000–0x03FFF` | arranque del chip y `isd_config` (de M-VAVE: no se toca) |
| `0x04000–0x92FFF` | programa (`app.bin` hasta 582 120 bytes; Bubba usa 564 620) |
| `0x97000–0xE4FFF` | datos de Bubba (patrones, tomas, ajustes, cargador en espera) — ventana sin cifrar |
| `0xE5000–0xECFFF` | almacén del SDK |
| `0xED000–0xFFFFF` | datos del aparato (no se tocan) |

## 3. Los dos núcleos

- **El RTOS es SMP**: un FreeRTOS modificado por JieLi con una lista de tareas por núcleo, `lockset`/`lockclr` como candado entre núcleos y `csync` como barrera. El tick es de 10 ms (IRQ3) y cada núcleo tiene su tarea ociosa con la instrucción `idle` (FM-1-RE `io/02-rtos.md`).
- **No hay una API pública para fijar una tarea a un núcleo** en este SDK (búsqueda en `include_lib/system/os`). El planificador reparte solo.
- **Comprobado en Bubba**: la tarea de audio corre a veces en el núcleo 0 y a veces en el 1 (campo «núcleo» de la consola).
- **Dos tareas no alcanzan (medido en el aparato, 0.6.1, 2026-10-15).** La 0.6.1 calculaba T4-T6 en una segunda tarea despertada con semáforos, con un traspaso por cada tramo de 64 muestras. Con un patrón de las seis máquinas: un núcleo 88-115 % de carga, **TWO CORES 123 %**. Las dos tareas se turnaban en vez de correr a la vez. El 1,83× de la prueba 24 (`docs/TRAZA_FM1.md`) es con dos cálculos largos e independientes; el audio se sincroniza varias veces por bloque, y cada despertar pasa por el planificador.
- **X0X, Felucca y Melodee** no usan el sistema operativo: arrancan el núcleo 1 ellos mismos (la palabra de arranque en `0x01C7FFF8`, `C1_CON`), lo dejan girando en un bucle en RAM y le pasan un trabajo por bloque en un buzón con `csync`, con una palabra de control y un tiempo máximo. Algunos FM-1 leen mal la RAM desde el núcleo 1 (fm1-x0x#10): por eso, si algo falla, siguen con un núcleo. No se puede copiar tal cual: el SDK ya corre su sistema en el núcleo 1.
- **Lo que sí da el SDK**: `request_irq(..., cpu_id)` registra una interrupción en el núcleo 1, y hay ocho interrupciones por software (`IRQ_SOFT0..7`; el sistema usa la 4 y la 5 para cambiar de tarea, `vSoftwareInterruptISR`). Cada núcleo tiene su banco de registros de interrupción (`q32DSP(n)`, con `ILAT_SET`/`ILAT_CLR`).
- **0.6.2: el núcleo 1 por interrupción** (`hal/fm1/fm1_cpu1.c`). El núcleo 0 deja el trabajo en un buzón (el de Felucca/X0X), levanta `IRQ_SOFT2` en el banco del núcleo 1 y espera girando, con 4 ms de tiempo máximo. Sin planificador de por medio. Al prender TWO CORES se prueba que conteste (y qué bit de `ILAT_SET` la levanta); si no, un núcleo.
- **La coma flotante vive en los registros enteros.** El cambio de tarea del SDK solo guarda `r0-r15` y `psr/rets/reti`, y las instrucciones de la FPU (`3f e5 …`, el desensamblador no las conoce) operan sobre ellos. Una interrupción que guarda sus registros respeta las cuentas de la tarea que interrumpe. La prueba 35 del laboratorio lo comprueba en el aparato.
- **La pila de interrupciones del núcleo 1 es de 4 KiB** (`.cpu1_sstack`). El camino más hondo de T4-T6 (BX-7, `fm_process`) usa ~0.8 KiB más las llamadas indirectas: alcanza.

## 4. Lo que hace cada proyecto (los más usados y mejor documentados)

| Proyecto | Base | Lo que aprendemos para la arquitectura |
|---|---|---|
| Firmware de fábrica (FM-1-RE) | SDK, 240 MHz | caminos calientes en RAM; FM entera (msfa) en bloques de 64; 12 voces a 44 118 Hz |
| **Felucca** | sin SDK | `.ram_text` de 24 KiB, «pool» de 336 KiB para buffers grandes, **zona NOINIT que sobrevive reinicios**, bandas de guarda en las pilas y **protecciones del hardware** (límite de pila, límites de escritura y de PC, accesos inválidos al bus) que convierten un error en un informe en vez de un cuelgue; presupuesto de ciclos medido por función (`tests/target_budget.txt`) |
| **X0X** | sin SDK | medio buffer de 512 frames (11,6 ms) porque un paso pesado pasaba el 80 % del tiempo; mide el peor caso y el promedio de cada bloque; el USB anidado dentro del render |
| **fm1-nes** (Apache-2.0) | **SDK AC79, como Bubba** | arranque y periféricos del SDK; auditoría del binario; escrituras de flash planificadas |
| **Lunar Modulator** (MIT) | SDK V1.2.13 | auditoría del enlace; la llave del SDK es inerte en el FM-1 |
| **Emulador del AC791N** (GPL-3.0, herramienta) | — | corre `.fwsc` sin modificar: probar sin tocar el aparato |

### Protecciones del hardware que ya trae el SDK

`cpu/wl82/debug.c` (Apache-2.0, ya compilado en Bubba) configura y lee los límites de pila (`EMU_SSP/USP`), los límites de escritura por núcleo (`WR_LIMIT`), los límites de PC y los accesos inválidos al bus (`AXI_*_INV_ID`), y con `CONFIG_SAVE_EXCEPTION_LOG_IN_FLASH` guarda un informe de la excepción en la flash. Hoy una excepción **cuelga** el aparato (`exception_analyze` termina en un bucle). Con una versión propia, una excepción deja su informe en la zona del perro guardián y **reinicia sola**.

## 5. Plan de reingeniería, ajustado con lo investigado

Cada paso se mide con la consola en el aparato, antes y después.

| # | Cambio | Por qué (fuente) | Se mide con |
|---|---|---|---|
| 1 | **Perfil completo** | base para decidir | tiempos por parte; fallos de caché a ojo con pruebas A/B |
| 2 | **Excepciones que se informan y reinician** (copia propia de `exception_analyze`) | hoy una excepción cuelga; Felucca y el SDK ya usan las protecciones del hardware | fallo provocado en el laboratorio |
| 3 | **Caché de datos**: probar `FREE_DACHE_WAY` 7 → 4 → 0 | 4 KiB de caché de datos para todo el audio (sección 2) | µs del render con PLAY |
| 4 | **Código caliente en RAM**: interrupciones y bucles de audio con `.volatile_ram_code` | firmware de fábrica y Felucca; documentación del SDK | µs del render y de las interrupciones |
| 5 | **Compilador**: `-O2` en el audio (hoy todo es `-Oz`) | | µs contra bytes |
| 6 | **Datos calientes en RAM**: samples y tablas en uso copiados a RAM | lectura por caché/SFC | µs del render |
| 7 | **Audio en los dos núcleos**: el núcleo 1 por interrupción, un buzón con tiempo máximo (sección 6) | dos tareas se turnaban (sección 3) | carga por núcleo, espera máxima |
| 8 | **Pantalla sin bloqueo**: reloj SPI más alto y DMA | hoy un cuadro completo tarda ~100 ms | µs de la UI |
| 9 | **Reloj**: decidir 320 / 360 / 396 MHz | hoja de datos y SDK | carga, temperatura, batería |
| 10 | **Memoria del programa**: samples y tablas a la zona de datos como banco de recursos | programa casi lleno (564 de 568 KiB) | KiB libres |

Con esa base se monta el bus de audio a 44,1 kHz en coma flotante y los motores nuevos (`docs/INVESTIGACION_SONIDO.md`).

## 6. Arquitectura en paralelo (desde 0.6.2)

El punto de partida, medido en el aparato con la 0.6.1 y un patrón de las seis máquinas (`tools/fm1/traza.py perfil`): el audio no alcanza (88-115 %, picos de 142 %, ~70 bloques perdidos por segundo). Reparto del cálculo: **BUNO-6 32 %**, mezcla y efectos 20 %, BR-8 19 %, BP-1 8 %, B-MINI/BB-303, BX-7 y BITTAL ~6 % cada uno.

Cada núcleo tiene un papel fijo, y todo lo que puede esperar sale del audio. Ningún trabajo en paralelo espera para siempre: tiempo máximo y vuelta a un núcleo.

| Capa | Qué corre | Dónde | Cómo |
|---|---|---|---|
| 1. Audio, tiempo real | el render de cada medio buffer (5,8 ms) | núcleo 0 (T1-T3, secuenciador, mezcla) + núcleo 1 (T4-T6) | el núcleo 1 dentro de una interrupción por software; un traspaso por tramo |
| 2. Control | coeficientes de filtros, `expf`, LFO, envolventes | dentro del audio, pero una vez por bloque, no por muestra | revisar máquina por máquina, empezando por BUNO-6 |
| 3. Segundo plano | pantalla, guardado, tablas de BITTAL, tomas, sonidos, consola USB | tareas del RTOS en cualquier núcleo | asíncronas: la pantalla por DMA, el guardado en tramos |
| 4. Seguridad | freno de voces, perro guardián, modo seguro | las tres capas | el freno apaga la voz más débil si un bloque pasa del 85 % |

Lo que bloquea la capa 3 hoy:
- **La pantalla espera cada envío por SPI** (12-25 ms por cuadro). Con DMA, el próximo cuadro se dibuja mientras sale el anterior.
- **Guardar apaga el audio** (`fm1_quiet`): el código corre desde la flash y mientras se escribe no se puede leer. Cada guardado es un corte. Con el código del audio en RAM (`.volatile_ram_code`, como Felucca) el audio sigue; lo limita la RAM libre (sección 7).

### Fases

| Versión | Contenido | Se mide con |
|---|---|---|
| **0.6.2** | Freno de voces; TWO CORES apagado de fábrica; el núcleo 1 por interrupción (a prueba); el laboratorio cabe en el montón y la prueba 35 (núcleo 1 y coma flotante) | `consola.py estado`, `traza.py perfil`, `traza.py una 35` |
| 0.6.3 | Reparto que se ajusta solo (por el costo de cada canal en el bloque anterior, que el perfil ya mide); BUNO-6 partido por voces entre los dos núcleos y más barato (voces a media frecuencia, coeficientes por bloque); reverb a media frecuencia | carga con el mismo patrón |
| 0.7 | Pantalla por DMA; guardado sin cortar el audio, si alcanza la RAM | µs de la UI, cortes al guardar |

## 7. Memoria: qué ocupa la RAM y dónde ayuda un caché por contenido

La RAM fija de la 0.6.2 son 385 KiB (`build.py fm1`, `nm` del ELF); el montón del SDK queda en 126 KiB y en marcha le sobran ~58 KiB.

| Qué | KiB | Idea |
|---|---|---|
| Arena del audio: delay (0,8 s) y efectos, o la grabación del sampler | 98 | delay a media frecuencia: −35 KiB y menos cálculo |
| Tablas de BITTAL (2 × 8 cuadros × 7 niveles) | 64 | una tabla compartida si los dos osciladores usan la misma: −32 KiB en ese caso |
| Reverb | 32 | a media frecuencia: −16 KiB |
| Tablas de la FM (`exp2tab`, `sintab`, `tanhtab`, `lut`) | 28 | se calculan al arrancar: como constantes en la flash quedan libres, a cambio de leerlas por la caché (medir) |
| Voces de la FM (msfa) | 21 | |
| Patrón, canción, comandos pendientes | 24 | |
| COPY, UNDO y REDO (tres copias enteras de un canal) | 7,5 | guardar diferencias en vez de copias |

**El caché por contenido (la idea de git) sirve donde los datos se repiten o cambian poco**, no en los búferes del audio, que cambian en cada muestra:
- **Ya existe en pequeño**: las tablas de BITTAL se arman desde su espectro solo si cambió la firma (`wt_sig`), como un objeto de git que se reconstruye solo si cambió su hash. Extenderlo a un caché de tablas por firma permite compartir una tabla entre osciladores y canales.
- **UNDO, REDO y COPY como diferencias** (los commits de git guardan lo que cambió): de 7,5 KiB a unos cientos de bytes.
- **En la flash es donde más rinde**: patrones, sonidos y tomas como objetos por contenido (lo repetido se guarda una vez) y el guardado automático escribiendo solo los sectores que cambiaron. Menos bytes escritos = pausas del audio más cortas y menos desgaste.

## Fuentes

- Hojas de datos AC791N (AC7911B, AC7912A, AC7913x, AC7915x, AC7916A): `fw-AC79_AIoT_SDK/doc/datasheet`
- SDK AC79 de JieLi: <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>; documentación: <https://doc.zh-jieli.com/AC79/zh-cn/master/>, en particular «功耗优化说明» (caché y relojes) y «代码运行在FLASH或SDRAM配置方法» (código en RAM)
- FM-1-RE (`docs/architecture.md`, `docs/io/01-boot.md`, `docs/io/02-rtos.md`): <https://github.com/AL-255/FM-1-RE>
- ISA pi32v2: <https://github.com/kagaimiq/jielie/blob/main/cpu/pi32v2.md>; Quarkslab: <https://blog.quarkslab.com/nerd-life-weeks-firmware-teardown-we-were-right.html>
- Felucca (`firmware/app.ld`, `firmware/hal/fm1_guard.h`, `tests/target_budget.txt`): <https://github.com/hugelton/Felucca>
- X0X (`firmware/src/app/main_fm1.c`): <https://github.com/charlesvestal/fm1-x0x>
- fm1-nes: <https://github.com/Keitark/fm1-nes>
- Lunar Modulator, PR #56: <https://github.com/ip2k/lunar-modulator/pull/56>
- ghidra-jieli (perfil AC7911B8): <https://github.com/edco/ghidra-jieli>
- Emulador del AC791N: <https://github.com/shcherbakov7/m-wave-fm1-browser-emulator>
- Lista de proyectos: <https://github.com/cicloid/awesome-fm-1>
