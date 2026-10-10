# Presupuesto de memoria

## RAM (96 KiB de `.data/.bss`)

Medido con `sizeof` en el build de PC (punteros de 8 bytes; en el FM-1 son de 4, así que será un poco menos).

| Bloque | Bytes | Notas |
|---|---:|---|
| Patrón activo (`bm_pattern`) | 12 696 | 2 pistas TR (16 líneas × 64 pasos, 1 byte por paso + P-Locks) + 4 synths + 4 looperas |
| Arena compartida | 49 152 | Delay (32 KiB) + punch-in (16 KiB); durante una grabación de sample, toda la arena es el buffer (3 s a 16 kHz) |
| Caché de patrón sin guardar | 8 192 | para cambiar de patrón sonando; también sirve de buffer al reescribir los chops |
| Portapapeles, undo y redo | ~6 200 | una pista cada uno |
| Cola de comandos UI → audio | ~2 048 | 128 comandos |
| Contexto de audio (voces, buffers, secuenciador) | ~4 300 | |
| Resto de la app (UI, gestos, forma de onda) | ~1 700 | |
| **Total `bm_app`** | **84 312** | |
| Franja de render + tile | 4 800 | 240×8×2 + 60×8×2 |
| Tabla mu-law, LEDs, MIDI | ~1 100 | |
| **Total estático** | **~90 200** | |
| Libre para pila e ISR | ~8 100 | `VERIFICAR` con el mapa del linker del port |

Si en el FM-1 falta RAM, lo primero que se achica es la arena (`BM_ARENA_LEN` en `core/bm_config.h`): menos delay y menos segundos de grabación.

## Flash (1 MiB)

| Zona | Tamaño | Contenido |
|---|---:|---|
| ROM de batería | 170 938 B | 2 kits × 16 sonidos, mu-law a 22 kHz (presupuesto: 192 KiB) |
| ROM de ondas | 65 536 B | 16 ondas × 8 niveles band-limited × 256 muestras |
| Logo de arranque | ~19 KB | RLE con paleta de 48 colores |
| Fuentes | ~1 KB | 5×7 y 3×5 |
| Código | `VERIFICAR` | Depende del toolchain de JieLi; el límite es ~340 KiB (SPEC §4.4) |
| Patrones | 164 KiB | 20 slots de 8 KiB: 16 patrones + canción/escenas + 3 libres para el wear-leveling |
| Samples | 128 KiB | 2 slots de 64 KiB (4 s a 16 kHz cada uno), se reproducen directo desde la flash |
| Globales | 12 KiB | 3 sectores en rotación |

## Pantalla

Presupuesto de SPI a 12 MHz: ~37 KB por frame a 30 FPS. Medido en las pruebas con la matriz TR sonando: **6.8 KB de promedio por frame** y 43 KB de pico al cambiar de pantalla (≈ 29 ms a 12 MHz, todavía dentro del frame de 33 ms), gracias al envío por tiles de 60×8 solo cuando cambian.
