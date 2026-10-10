# Cambios

Todas las versiones de Bubba Machine. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y los números siguen [versionado semántico](https://semver.org/lang/es/): mientras la versión empiece con 0, cualquier versión menor puede cambiar la forma de tocar o el formato de los patrones.

## [0.2.0] - 2026-10-10

Primera versión pública. Corre en PC y en el emulador web; el port al FM-1 aún no se ha probado en hardware.

### Agregado
- **Batería TR** (T1) y **sampler** (T2) con 16 instrumentos, cada uno con su línea y su largo (polímetro), acento, repetición ×2-×4, flam, P-Locks y pasos que solo suenan (o callan) en el fill. Roll en vivo y borrado estilo TR.
- **Tiempo por pista:** largo 1-64 y velocidad ×2 a ×1/4; largo maestro de 16 pasos a infinito.
- **Sampler:** graba la mezcla con chops en vivo; 2 slots de 4 s en flash que se reproducen directo (XIP).
- **Loops de fábrica** en la ROM (BUBBA 160 y BUBBA 128), sintetizados por `tools/gen_loops.py`. El demo arranca con uno.
- **Dibujo del sample por frecuencias:** tres carriles (graves, medios y agudos) en lugar de la forma de onda, que en música muy comprimida es un bloque.
- **Loopera** de notas por pista en los synths (REC › PLAY › OVERDUB).
- **Acordes en cuadrícula** 4×4 con grado, nombre y función por color; juegos de progresiones, voicing conducido y strum.
- **Escenas** A-D con cadenas y SONG REC; fills automáticos.
- **Tercera pulsación de 2 s** (TIEMPO, SISTEMA, SAMPLER, borrar pista, guardado rápido) con anillo de progreso.
- Licencia **GPL-3.0-or-later**, documentación, sitio web y CI.

### Cambiado
- El proyecto se publica como **Bubba Machine**, independiente de cualquier firmware anterior.

## [0.1.0] - 2026-10-09

Primera versión interna.

### Agregado
- Núcleo completo en C99: audio, secuenciador de 6 pistas, interfaz de 240×240, guardado en flash con CRC y wear-leveling.
- Batería sintetizada (2 kits), bajo y 3 synths de wavetable con filtro, arpegiador y escalas.
- Simulador de PC con pruebas automáticas y emulador web en WebAssembly.
- Esqueleto del port al FM-1 (estructura, pines y escaneo de la matriz).

[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commit/3118530
