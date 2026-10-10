# Presupuesto de memoria

## RAM (96 KiB de `.data/.bss`)

Medido con `sizeof` en el build de PC (punteros de 8 bytes; en el FM-1 son de 4, así que será un poco menos).

| Bloque | Bytes | Notas |
|---|---:|---|
| Patrón activo (`bm_pattern`) | 12 544 | 64 + 6×32 + 6×64×4×8 |
| Sampler | 24 576 | int8, 1.11 s |
| Delay | 16 384 | int16, 8192 muestras |
| Buffer de punch-in | 8 192 | mu-law, 0.37 s |
| Caché de patrón sin guardar | 8 192 | formato disperso, para cambiar de patrón sonando |
| Portapapeles (pista + paso) | 2 112 | |
| Undo (una pista) | 2 048 | |
| Cola de comandos UI → audio | ~2 048 | 128 comandos |
| Contexto de audio (voces, buffers de pista, estado) | ~3 900 | |
| Resto de la app (UI, gestos, tiles) | ~1 000 | |
| **Total `bm_app`** | **80 968** | |
| Franja de render + tile | 4 800 | 240×8×2 + 60×8×2 |
| Tabla mu-law, LEDs, MIDI | ~1 100 | |
| **Total estático** | **~86 900** | |
| Libre para pila e ISR | ~11 400 | `VERIFICAR` con el mapa del linker del port |

Si en el FM-1 falta RAM, lo primero que se achica es el sampler (`BM_SAMPLER_LEN` en `core/bm_config.h`) y después el delay.

## Flash (1 MiB)

| Zona | Tamaño | Contenido |
|---|---:|---|
| ROM de batería | 170 938 B | 2 kits × 16 sonidos, mu-law a 22 kHz (presupuesto: 192 KiB) |
| ROM de ondas | 65 536 B | 16 ondas × 8 niveles band-limited × 256 muestras |
| Logo de arranque | ~19 KB | RLE con paleta de 48 colores |
| Fuentes | ~1 KB | 5×7 y 3×5 |
| Código | `VERIFICAR` | Depende del toolchain de JieLi; el límite es ~340 KiB (SPEC §4.4) |
| Store de usuario | 292 KiB | 36 slots de 8 KiB: 16 patrones + song + espacio para el wear-leveling |
| Globales | 12 KiB | 3 sectores en rotación |

## Pantalla

Presupuesto de SPI a 12 MHz: ~37 KB por frame a 30 FPS. Medido en las pruebas con SEQ sonando: **3.7 KB de promedio por frame** y 28.8 KB de pico al cambiar de pantalla, gracias al envío por tiles de 60×8 solo cuando cambian.
