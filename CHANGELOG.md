# Cambios

Todas las versiones de Bubba Machine. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y los números siguen [versionado semántico](https://semver.org/lang/es/): mientras la versión empiece con 0, cualquier versión menor puede cambiar la forma de tocar o el formato de los patrones.

## [0.3.0] - 2026-10-10

La interfaz 3.0, completa en el emulador. **No lee los patrones, ajustes ni samples de la 0.2:** al primer arranque los borra y empieza con la demo. El port al FM-1 sigue sin probarse en hardware.

### Agregado
- **Cuatro gestos por botón** que significan lo mismo en todos: toque (su página; otra vez, la siguiente), mantener (su capa en vivo, con el teclado dibujado en la pantalla), mantener 2 s y **HOME + botón** (su versión global).
- **HOME en todas partes:** pantalla de inicio con el banco de patrones, **controlador** al mantenerlo en cualquier pantalla (blancas = patrón en cola, dos blancas = cadena, negras = mute o solo, escenas y FILL) y modificador: deshacer y rehacer, volver al paso 1, SONG REC, canción, delay global, escala del proyecto, guardar como y copiar un canal a otro.
- **ALGORITHM navega:** en HOME, un recuadro con el canal destino y entrada a los 400 ms (0, 250, 400, 800 ms u OFF); dentro de un canal cambia y conserva la página; con HOME sostenido elige canal desde cualquier pantalla.
- **Pestañas por canal** que SELECT recorre, y la pestaña **MAS** con todo el canal.
- **3 canales fijos y 3 libres:** T4-T6 eligen motor **SYNTH, MONO, KIT** (una segunda batería TR) o **MIDI** (manda sus notas por MIDI).
- **Capas nuevas:** SEQ largo y velocidad, ENV pintar acentos, LFO pintar repeticiones o ligaduras, SAVE copiar patrón (o un solo canal) a otro lugar, GLO transponer todo el proyecto con TAP tempo en PLAY.
- **Sampler 3.0:** un loop en 8 slices que un **generador** rearma compás a compás, con las filas **TU** (silencio, generador o slice fijo) y **SUENA** (lo que eligió), las perillas VARIAR, ANCLAR, REPETIR y ROMPER, bombo y caja detectados (K y S), loop B con su probabilidad, retrigs con pesos, reversa, media velocidad y stutter en vivo, y **CAPTURAR** la variación que suena.
- **El loop sigue el tempo** y la pantalla dice cuántos semitonos se movió; PITCH lo corrige y MAS > USAR BPM DEL LOOP lleva el proyecto a su tempo.
- **Grabadora propia** (REC en el sampler): armada al compás, con los segundos que quedan en grande, toma sin guardar que ya suena y se guarda al parar.
- **Biblioteca de tomas** de largo libre (8.1 s en total, hasta 3 s por toma) con BPM, compases y ganancia, y el editor **CORTAR**: cortes por corchea, en los golpes o iguales.
- **Acento en tres niveles** en las notas (normal, acento, fantasma) y cuánto acento por canal.
- **Escala propia por canal**; la del proyecto queda en HOME + SCL.
- **Preguntas** antes de borrar (OCT+ sí, HOME no).
- Sitio y documentación también en inglés, con selector de idioma en cada página (el emulador incluido).

### Cambiado
- **GLO** dejó de ser el modificador: ahora es HOME. GLO abre la lista global y, sostenido, la capa TONO.
- **REC** se toca al soltar: toque = grabar, mantener = grabar mientras lo sostienes, 2 s = borrar el canal (pregunta).
- Las negras del canal siguen una sola gramática: 1-8 los elementos, 9 y 10 modifican, 11 FILL o STUTTER.
- El sampler pasó de 16 chops a 8 slices, y de 2 slots fijos de 4 s a la biblioteca de tomas.
- El sampler también puede agacharse con el sidechain del bombo, y la demo bajó sus niveles para no saturar con el break sonando.
- **Formato 3** de patrones, ajustes y tomas.

### Quitado
- La pantalla SAMPLER de la 0.2 (ahora son SONIDO, TOMAS, GRABAR y CORTAR) y los slots A y B.

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

[0.3.0]: https://github.com/Erbubar23/bubba-machine/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commit/3118530
