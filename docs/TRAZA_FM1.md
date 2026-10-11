# Traza del FM-1: mediciones propias (2026-10-11)

Mediciones hechas en el FM-1 del autor con `tools/fm1/traza.py` (pruebas del laboratorio en `hal/fm1/fm1_bench.c` y puntos de medición `core/bm_prof.h` dentro de `bm_audio_render`). Reloj del sistema 360 MHz, bus lento 60 MHz, timer de medición a 60 MHz (resolución ~17 ns). Los archivos crudos quedan en `build/traza/`.

## 1. La arquitectura

| Prueba | Resultado | Conclusión |
|---|---|---|
| Biquad en coma flotante, código en flash / en RAM | 87.9 / 87.9 ns por muestra | la caché de instrucciones (32 KiB) alcanza para los bucles calientes: mover código a RAM no gana nada en ellos |
| Biquad en enteros de 64 bits, flash / RAM | 131.1 / 131.5 ns por muestra | **la FPU es 1.5× más rápida** que el entero de 64 bits: DSP en float |
| Lectura secuencial flash / RAM | 36 / 32 ns por byte | leer en orden desde la flash es casi gratis |
| Lectura al azar flash / RAM (caché del SDK por defecto) | **1306 / 58 ns** | cada fallo de caché cuesta ~470 ciclos |
| **Tamaño de la caché de datos**, SDK por defecto | rápido hasta 4 KiB (113 ns); 700 ns en 8 KiB, 1274 ns en 64 KiB | **la caché de datos efectiva era de 4 KiB** (7 de 8 vías usadas como RAM) |
| **Tamaño de la caché de datos con `CONFIG_DYNAMIC_SDRAM_ONOFF_ENABLE`** en el guion de enlace | rápido hasta 32 KiB (108–113 ns); 485 ns en 48 KiB, 676 ns en 64 KiB | **32 KiB de caché de datos**: 6–10× más rápido en lecturas al azar de 8–32 KiB. Adoptado. |
| Un cálculo solo / dos a la vez en dos tareas | 13.6 / 14.8 ms; núcleos 0 y 1 | **1.83× con dos tareas**: el segundo núcleo rinde casi el doble |
| Pantalla completa (240×240) con SPI a 12 / 15 / 20 / 30 MHz | 85 / 70 / 54 / **39 ms** | subir el SPI hace la pantalla 2.2× más rápida (falta confirmar a la vista que se ve bien) |

## 2. El audio real (Bubba 0.3, demo)

| Sección | En reposo | Con PLAY (caché 4 KiB) | Con PLAY (caché 32 KiB) |
|---|---|---|---|
| Comandos | 0.1 % | 0.0 % | 0.1 % |
| Secuenciador | 0.2 % | 0.4 % | 0.4 % |
| Canal 1 (batería, samples) | 0.1 % | 1.7 % | **1.4 %** |
| Canal 2 (sampler) | 0.1 % | 3.8 % | 3.7 % |
| Canal 3 (bajo) | 0.1 % | 2.3 % | 2.1 % |
| Canal 4 | 0.1 % | 3.9 % | 3.6 % |
| Canal 5 | 0.1 % | 1.6 % | **1.3 %** |
| Canal 6 | 0.1 % | 2.8 % | 2.6 % |
| **Mezcla y efectos** | **8.4 %** | 8.6 % | 8.6 % |
| **Total** (% de un núcleo) | 9.4 % | 25.1 % | **23.8 %** |
| Pantalla/UI por cuadro | 8.2 ms | 10.3 ms | 9–10 ms |

- **La mezcla es la sección más cara y es fija**: por cada muestra y canal hace una división entera (`x * nivel / 400`), multiplicaciones de 64 bits y codifica la salida en μ-law para los efectos aunque no se usen. Es un problema del cálculo, no del hardware.
- **La pantalla cuesta ~30 % de un núcleo** (9–10 ms por cuadro a 30 cuadros/s), la mayor parte esperando al SPI.
- Los motores actuales cuestan 1.3–3.7 % por canal a 22 kHz: hay mucho margen para motores de calidad.

## 3. Lo que eso cambia del plan

| Antes (docs/ARQUITECTURA_FM1.md) | Ahora, con datos |
|---|---|
| Código caliente en RAM | **No hace falta para los bucles**: la caché de instrucciones alcanza. Solo para lo que deba correr mientras se escribe la flash. |
| Probar las vías de la caché de datos | **Hecho**: 32 KiB, medido. |
| `-O2` en el audio | Sigue; medir. |
| Dos núcleos | **Confirmado 1.83×**: repartir las voces en dos tareas. |
| Pantalla | **Subir el SPI** (2.2× medido) y DMA para no esperar. |
| Bus de audio en coma flotante | **Confirmado**: la FPU gana 1.5× al entero de 64 bits. |
| Mezcla | **Nuevo**: reescribirla en float por bloques (ganancias por bloque, sin divisiones por muestra, efectos solo si están activos). |

## Plan reestructurado (prioridad: calidad de audio)

1. ~~Caché de datos de 32 KiB~~ (hecho y medido).
2. ~~**Bus de audio en coma flotante a 44.1 kHz**~~ (hecho y medido, sección 4).
3. **Audio en dos núcleos**: dos tareas, cada una con la mitad de las voces (1.83× medido).
4. **Pantalla**: SPI más rápido (hasta 30 MHz medido) y envío por DMA sin bloquear.
5. **Compilador**: `-O2` en el audio, medido contra el espacio.
6. **Reloj**: 360 MHz hoy (la batería no es prioridad); medir 396 MHz, el modo de sobrerreloj del propio SDK, con un cambio en tiempo de ejecución, porque el `isd_config` del paquete no se puede cambiar.
7. **Memoria del programa**: datos grandes a la zona de datos, para hacer lugar a los motores nuevos.
8. **Motores de calidad** (`docs/INVESTIGACION_SONIDO.md`), medidos voz por voz con el mismo perfil.

## 4. Paso 2: los motores a 44.1 kHz y el bus en coma flotante

Cambio: `BM_FS` de 22 059 a 44 118 Hz (sin sobremuestreo x2); la mezcla (`core/bm_dsp.c`) en float, canal por canal con las ganancias calculadas una vez por bloque y sin recorrer los canales en silencio (`bm_ctx.tact`); delay a Fs/2 en 16 bits con escritura promediada y lectura interpolada; captura de los punch-in en 16 bits (antes μ-law de 8); los kits de la ROM (22 kHz) se leen a medio paso con interpolación lineal o Hermite.

| Sección | 22 kHz + entero (antes) | **44.1 kHz + float** |
|---|---|---|
| Mezcla y efectos, en reposo | 8.4 % | **4.8 %** |
| Mezcla y efectos, con PLAY | 8.6 % | **7.3 %** (el doble de muestras) |
| Canales, con PLAY (suma) | 14.7 % | 29.0 % (el doble de muestras, motores sin cambiar) |
| **Total con PLAY** | 23.8 % | **36.9 %** de un núcleo; 0 bloques tarde; peor bloque 53 % del presupuesto |
| RAM estática (`.ram0_bss`) | 180 KiB | 260 KiB (arena de 128 KiB: delay 64 + captura 64) |
| Montón libre (`get_malloc_remain_heap_size`) | — | **192 KiB** |

- El costo por muestra de la mezcla bajó a menos de la mitad (7.3 % con el doble de muestras contra 8.6 %).
- Los motores cuestan el doble porque calculan el doble de muestras: siguen en entero de 64 bits. Pasarlos a float (1.5× medido) y repartirlos en los dos núcleos (1.83× medido) son los pasos 3 y 8.
- `malloc` del SDK devuelve un puntero hasta para 512 KiB: no sirve para medir la memoria libre. La consola usa `get_malloc_remain_heap_size()` de `system.a`, desde la tarea principal (la consola corre en la interrupción del USB).
- Pendiente para el paso 8: los kits KO-808/909 siguen grabados a 22 kHz en μ-law de 8 bits (no tienen nada arriba de 11 kHz). Hay que regenerarlos a 44.1 kHz y 16 bits en la zona de datos de la flash (paso 7).
