# Cambios

Todas las versiones de Bubba Machine. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y los números siguen [versionado semántico](https://semver.org/lang/es/): mientras la versión empiece con 0, cualquier versión menor puede cambiar la forma de tocar o el formato de los patrones.

## [Sin publicar]

Los sonidos se rehicieron: **ya no hay sonidos grabados ni wavetables**, todo se calcula en vivo con bibliotecas de síntesis probadas. **Hasta la v1 no hay compatibilidad hacia atrás:** los patrones de 0.3 no se leen; al primer arranque se borran (con la canción y los ajustes) y arranca la demo. Las tomas del sampler se quedan.

### Agregado
- **Batería modelada** (T1 y los canales KIT): 16 instrumentos, cada uno con su **modelo**: bombo, caja y hat de la 808 y versiones sintéticas al estilo 909 (los modelos de Plaits, de Mutable Instruments), hat metálico y platillos, palmas, tom y conga, cencerro, resonador (rim, clave, bloque), ruido (maracas) y percusión FM. Kits **808**, **909** y **SINTE**. Cada instrumento se edita en INSTR: AFINA, DECAY, TONO, COLOR (lo que corresponda al modelo), MODELO, NIVEL, grupo de corte (choke) y LARGO. El acento pega distinto, no solo más fuerte. Seis voces para la batería (antes cuatro).
- **ANALOG nuevo**, analógico virtual de verdad: dos osciladores de forma variable con polyBLEP (SIERRA, CUADRADA con ancho de pulso, TRIANGULO, SENO con plegado y 3 SIERRAS), sync, ring, sub y ruido; filtro LP 24/12, BP o HP con resonancia y drive; ADSR de filtro y de nivel; LFO a tono, corte, forma o nivel; POLI, MONO o LEGATO con glide. 16 presets.
- **Editor de sonido por páginas** (SONIDO en ANALOG, INSTR en la batería): 4 ajustes por página en K1-K4, SELECT pasa de página, EDIT/ENV/LFO saltan a la que toca, y un dibujo de lo que hace cada página (onda, curva del filtro, envolvente, LFO).
- **Motor FM de 6 operadores** (msfa, la base de Dexed y del firmware de fábrica) para T3 y los canales libres: 16 patches propios (E.PIANO, BAJO FM, CAMPANA, METALES, CUERDAS, ORGANO, CLAVI, MARIMBA...), con las macros BRILLO, REALIM, DECAY y ALGORITMO (los 32 de la DX7) y una pantalla con los 6 operadores. MAS > MOTOR > FM.
- **Sampler:** mantener un paso hace sonar el loop desde ahí, entero y en bucle, mientras lo sostienes.
- **Grabación libre en los canales de notas**, como una loopera: en un canal vacío, REC no tiene largo fijo; al cerrarlo, el canal queda en compases completos (1-4) y cada nota guarda lo que la sostuviste.
- **Largo en compases** en la capa de SEQ (negras 7-10).
- **Motores de sonido nuevos:** **ACID** (estilo 303: filtro escalera de 4 polos con resonancia, acento y slide) para T3 y los canales libres; **ANALOG** (dos osciladores desafinados, sub y ruido, 6 timbres) para los canales libres; y el kit **SINTE**, una batería calculada en vivo. Los kits grabados KO-808 y KO-909 se quedan.
- **Memoria de tomas a la vista:** en la grabadora (segundos y KB libres, máximo por toma) y en el emulador, junto a la carga de audio.
- **FM-1, Bubba Machine completo para el aparato** (en pruebas, sin probar todavía en hardware): el núcleo con sonido por el codec (ALNK0), la matriz y los LEDs por interrupción, pantalla, USB-MIDI, guardado, modo seguro y servicio de actualización.
- **FM-1, firmware de diagnóstico** (en pruebas): sin sonido que muestra cada botón, tecla y perilla, el MASTER, la batería y el USB-MIDI, con modo seguro, salida al modo del chip y el servicio de actualización.
- **Instalador web** (`instalar.html`): arma el paquete en el navegador sobre el FM-1 V15 de M-VAVE (que cada quien descarga) y lo instala con el cargador oficial; lee el FM-1 solo (dice qué firmware tiene, o por qué no lo encuentra), se elige qué instalar (Bubba Machine, diagnóstico o firmware de fábrica) y queda un botón; también vuelve al firmware de fábrica y continúa una instalación cortada. `tools/fm1/servir.py` lo sirve en la computadora con el V15 ya cargado.
- **Herramientas del FM-1:** `tools/fm1/fwsc.py` (empaquetador, comprobado byte por byte contra el V15) y `tools/fm1/rescate.py` (rescate desde Windows por el modo del chip, con respaldo completo antes de escribir).

### Cambiado
- **Negras 10 y 11 en los canales:** abren pantallas. La **10** (batería) abre los **MUTES** de los instrumentos y la **11** abre el **FILL**, donde suena el fill y las blancas arman sus golpes (o marcan notas solo para el fill), con K1 = fill automático. Un toque las deja abiertas (se cierran con la misma tecla o HOME); mantenerlas = solo mientras las sostienes. Un toque ya no mutea a ciegas: cada mute avisa y el instrumento muteado queda en rojo.
- **ALGORITHM dentro de un canal** también pasa por el recuadro con el número antes de cambiar; en INSTR elige instrumento y lo hace sonar.
- **HOME explica las negras** (mutes, solo, escenas, fill, y las pantallas de la batería).
- **Fuera los motores viejos:** BAJO, SYNTH (wavetable), MONO y el ANALOG de 6 timbres pasan al ANALOG nuevo; los kits grabados KO-808 y KO-909 y el kit SINTE, a la batería modelada. El programa del FM-1 ocupa 148 KiB menos y se liberan 64 KiB de RAM.
- Patrones en **formato 4**: cada canal guarda 32 ajustes de sonido y cada instrumento de batería, su modelo y su golpe.
- **ANALOG en el FM-1:** 4.1 % de un núcleo por voz (oscilador polyBLEP propio, filtro recalculado cada 8 muestras, saturación sin división y el cálculo por pasadas cortas; la primera versión costaba 10 %).
- **El audio se compila para velocidad:** en el FM-1 la demo con PLAY pasó de 36.9 % a 27.7 % de un núcleo. Las wavetables se arman al arrancar desde su espectro (el programa ocupa 56 KiB menos).
- **Audio a 44.1 kHz de punta a punta, con el bus en coma flotante.** Los motores calculaban a 22 kHz y la salida se duplicaba; ahora calculan a 44.1 kHz (agudos hasta 20 kHz, sin imágenes del sobremuestreo) y la mezcla, el delay, los efectos y el clipper son de coma flotante, por bloques. El delay y la captura de los efectos (stutter, reversa, tape stop) pasan a 16 bits (antes la captura era μ-law de 8 bits), y la grabación del sampler se filtra con un Butterworth antes de bajar a 16 kHz. En el FM-1, con PLAY, la mezcla cuesta menos que antes aunque calcula el doble de muestras (docs/TRAZA_FM1.md).
- La interpolación **DROP** (sin interpolar) pasa a ser **LINEAL**: a 44.1 kHz, repetir muestras sonaba a lo-fi.
- La página SEQ de los canales de notas es solo de ese canal: piano roll de la página con el largo de cada nota, los compases del canal y el seguidor de tempo punteado. Ya no muestra los otros canales.
- Las notas grabadas pueden durar hasta 4 compases (antes, 1).
- Mapa de la flash para el aparato: los ajustes globales pasan a `0xDD000` y la biblioteca de tomas queda en 116 KiB (7.4 s en total; antes 8.1 s), para dejar lugar al cargador en espera y al almacén del SDK.

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
