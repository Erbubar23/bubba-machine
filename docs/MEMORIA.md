# Presupuesto de memoria

## RAM (96 KiB de `.data/.bss`)

Medido con `sizeof` en el build de PC (punteros de 8 bytes; en el FM-1 son de 4, así que será un poco menos).

| Bloque | Bytes | Notas |
|---|---:|---|
| Patrón activo (`bm_pattern`) | 12 792 | T1 batería y T2 sampler en rejilla TR (16 líneas × 64 pasos, 1 byte por paso + P-Locks) + 4 canales de notas o KIT (unión) + 4 looperas |
| Arena compartida | 49 152 | Delay (32 KiB) + punch-in (16 KiB); mientras graba la GRABADORA, toda la arena es la toma (3 s a 16 kHz) |
| Caché de patrón sin guardar | 8 192 | para cambiar de patrón sonando; también sirve de buffer al reescribir la cabecera de una toma |
| Portapapeles, undo y redo | ~6 200 | un canal cada uno |
| Cola de comandos UI → audio | ~2 056 | 128 comandos |
| Contexto de audio (voces, secuenciador, generador, 3 fuentes del sampler) | ~4 100 | |
| Tomas, fuentes y dibujos por bandas | ~2 050 | 8 tomas, 3 fuentes estables, el loop A y la toma que se corta |
| Resto de la app (UI, gestos, teclas, canción) | ~2 500 | |
| **Total `bm_app`** | **86 984** | 0.2 usaba 84 776 |
| Franja de render + tile | 4 800 | 240×8×2 + 60×8×2 |
| Tabla mu-law, LEDs, MIDI | ~1 100 | |
| **Total estático** | **~92 900** | |
| Libre para pila e ISR | ~5 400 | `VERIFICAR` con el mapa del linker del port |

Si en el FM-1 falta RAM, lo primero que se achica es la arena (`BM_ARENA_LEN` en `core/bm_config.h`): menos delay y menos segundos de grabación.

## Flash (1 MiB)

| Zona | Tamaño | Contenido |
|---|---:|---|
| ROM de batería | 170 938 B | 2 kits × 16 sonidos, mu-law a 22 kHz (presupuesto: 192 KiB) |
| ROM de ondas | 65 536 B | 16 ondas × 8 niveles band-limited × 256 muestras |
| Logo de arranque | ~19 KB | RLE con paleta de 48 colores |
| Fuentes | ~1 KB | 5×7 y 3×5 |
| Programa (app.bin) | hasta 568 KiB | Firmware sobre el SDK de JieLi; el de diagnóstico mide 121 KiB. Lo escribe el cargador oficial dentro de `0x000000-0x092FFF` junto con el arranque del chip |
| Patrones | 160 KiB | `0x97000`: 20 slots de 8 KiB: 16 patrones + canción/escenas + 3 libres para el wear-leveling |
| Registro de arranques | 4 KiB | `0xBF000`: cuenta los arranques fallidos (modo seguro y modo del chip) |
| Tomas | 24 KiB | `0xC0000`: por ahora una toma de hasta 1.53 s (sectores seguidos con la cabecera al inicio). Se reproduce directo desde la flash |
| Banco de sonidos | 76 KiB | `0xC6000`: reservado para los snapshots de la 0.6 ([DISENO_0.6](DISENO_0.6.md)); sin usar todavía |
| Idioma | 16 KiB | `0xD9000`: el paquete de idioma de la pantalla (`core/bm_lang.h`), si se cargó uno. Se lee directo de la flash |
| Globales | 12 KiB | `0xDD000`: 3 sectores en rotación |
| Cargador en espera | 20 KiB | `0xE0000`: el cargador de actualización mientras se instala una versión nueva |
| Ajustes del SDK (VM) | 32 KiB | `0xE5000`: el almacén del SDK, con su tamaño por defecto; el empaquetador lo mueve ahí |
| Del aparato | — | `0xED000` en adelante (BTIF, USR, MAC): Bubba Machine no lo toca nunca |

## Pantalla

Presupuesto de SPI a 12 MHz: ~37 KB por frame a 30 FPS. Medido en las pruebas con la matriz TR sonando: **6.8 KB de promedio por frame** y 43 KB de pico al cambiar de pantalla (≈ 29 ms a 12 MHz, todavía dentro del frame de 33 ms), gracias al envío por tiles de 60×8 solo cuando cambian.
