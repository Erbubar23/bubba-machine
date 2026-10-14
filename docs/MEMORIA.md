# Presupuesto de memoria

## RAM

Medido con `sizeof` en el build de PC de la 0.6.0-beta (punteros de 8 bytes; en el FM-1 son de 4, así que es un poco menos).

| Bloque | Bytes | Notas |
|---|---:|---|
| Arena de audio (`bm_audio_mem`) | 131 072 | Delay y captura de los punch-in a 16 bits; mientras graba la GRABADORA, la arena es la toma (3 s a 16 kHz) |
| Patrón activo (`bm_pattern`) | 15 448 | Formato 7: 6 canales, rejillas TR de BR-8 y BP-1, eventos de notas de 10 bytes (gate y micro-tiempo), 48 ajustes por canal y 4 looperas |
| Sonido que llega de afuera (`snap_buf`) | 8 800 | Un .bsnap o un banco .syx mientras se carga por USB |
| Escritura pendiente (`pend`) | 8 192 | El patrón sin guardar, para cambiar de patrón sonando |
| Contexto de audio (`bm_ctx`) | 6 280 | 22 voces, secuenciador y fuentes del sampler |
| Cola de comandos UI → audio | 3 080 | |
| Portapapeles del canal | 2 600 | |
| Tomas y fuentes del sampler | ~1 500 | |
| Resto de la app (UI, gestos, teclas, canción) | ~8 400 | |
| **Total `bm_app`** | **185 368** | 0.3 usaba 86 984 |

Fuera de `bm_app` están los búferes de BITTAL (las dos tablas armadas, 7 niveles cada una), los de cada núcleo para DOS NÚCLEOS, la franja de render y el MIDI. La RAM del AC791N alcanza con margen; si faltara, lo primero que se achica es la arena (`BM_ARENA_LEN` en `core/bm_config.h`): menos delay y menos segundos de grabación.

## Flash (1 MiB)

| Zona | Tamaño | Contenido |
|---|---:|---|
| Banco de fábrica | 65 268 B | Dentro del programa: los 37 sonidos de B-MINI, BITTAL y BUNO-6 (`core/gen/bm_snap_rom.c`, con sus tablas de BITTAL) |
| Logo de arranque | ~19 KB | RLE con paleta de 48 colores |
| Fuentes | ~1 KB | 5×7 y 3×5 |
| Programa (app.bin) | hasta 568 KiB | Firmware sobre el SDK de JieLi: la 0.6.0-beta mide 423 692 B (quedan 158 428 libres); el de diagnóstico, 121 KiB. Lo escribe el cargador oficial dentro de `0x000000-0x092FFF` junto con el arranque del chip |
| Patrones | 160 KiB | `0x97000`: 20 slots de 8 KiB: 16 patrones + canción/escenas + 3 libres para el wear-leveling |
| Registro de arranques | 4 KiB | `0xBF000`: cuenta los arranques fallidos (modo seguro y modo del chip) |
| Tomas y sonidos del usuario | 100 KiB | `0xC0000-0xD8FFF`, compartida: las tomas se ubican desde abajo (sectores seguidos con la cabecera al inicio, hasta 3 s cada una) y los sonidos del usuario (.bsnap y bancos de BX-7) desde arriba ([DISENO_0.6](DISENO_0.6.md)). Las tomas se reproducen directo desde la flash |
| Idioma | 16 KiB | `0xD9000`: el paquete de idioma de la pantalla (`core/bm_lang.h`), si se cargó uno. Se lee directo de la flash |
| Globales | 12 KiB | `0xDD000`: 3 sectores en rotación |
| Cargador en espera | 20 KiB | `0xE0000`: el cargador de actualización mientras se instala una versión nueva |
| Ajustes del SDK (VM) | 32 KiB | `0xE5000`: el almacén del SDK, con su tamaño por defecto; el empaquetador lo mueve ahí |
| Del aparato | — | `0xED000` en adelante (BTIF, USR, MAC): Bubba Machine no lo toca nunca |

## Pantalla

Con el SPI a 30 MHz (SISTEMA › PANTALLA RÁPIDA, a prueba) el cuadro entero baja de 85 a 39 ms. Presupuesto de SPI a 12 MHz: ~37 KB por frame a 30 FPS. Medido en las pruebas con la matriz TR sonando: **6.8 KB de promedio por frame** y 43 KB de pico al cambiar de pantalla (≈ 29 ms a 12 MHz, todavía dentro del frame de 33 ms), gracias al envío por tiles de 60×8 solo cuando cambian.
