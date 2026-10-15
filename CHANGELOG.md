# Cambios

Todas las versiones de Bubba Machine. El formato sigue [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y los números siguen [versionado semántico](https://semver.org/lang/es/): mientras la versión empiece con 0, cualquier versión menor puede cambiar la forma de tocar o el formato de los patrones.

## [0.6.1-beta] - 2026-10-14

**La 0.6.0-beta no debe instalarse:** dejaba 38 KiB de RAM para el sistema del FM-1, que necesita unos 50 KiB al arrancar, así que lo más probable es que no arrancara bien. La 0.6.1-beta deja 126 KiB y el instalador ya no ofrece la 0.6.0. Beta, todavía no probada en el aparato.

### Agregado
- **Plataforma de control** (`control.html`): con el FM-1 conectado por USB, muestra la memoria (RAM libre, carga del audio, tomas, la zona compartida y el idioma) con avisos según lo que estás haciendo, sube y borra tomas (convierte el audio en el navegador), carga y borra sonidos (Vital, .syx, .bsnap) y carga o quita el paquete de idioma. Sin el FM-1 no hace nada.
- **Consola, orden 23:** el informe de memoria y la biblioteca (tomas y sonidos del usuario) y borrar una toma; la orden 22 borra un sonido del usuario por su número.
- **Control de RAM al compilar el firmware:** `build.py fm1` falla si el montón del sistema queda por debajo de 120 KiB. Las pruebas en la computadora no podían verlo.

### Cambiado
- **RAM:** los búferes del laboratorio (~59 KiB) se piden al montón solo mientras corre una prueba, y el **delay baja a 0.80 s** (antes 1.49 s; con 1/4, por debajo de 75 BPM se acorta). La RAM fija baja de 472 a 384 KiB.
- **El sampler tiene un tope de 6 s de tomas en total**; el resto de la zona de 100 KiB queda para los sonidos.
- **DOS NÚCLEOS viene encendido de fábrica** para probarlo en el aparato. Un cuelgue o el modo seguro lo apagan.
- **El paquete de idioma se administra en la plataforma de control**: el instalador ya no lo carga.

## [0.6.0-beta] - 2026-10-14

**Beta, todavía no probada en el aparato:** pasa las pruebas automáticas y el emulador. Si algo falla en el FM-1, vuelve a la 0.5.0-beta o al firmware de fábrica desde el instalador.

**Seis máquinas fijas en vez de motores a elegir**, una cara nueva y un banco de sonidos. El formato de los patrones cambia (formato 7): los patrones guardados se borran al primer arranque (las tomas se quedan). El diseño completo está en [DISENO_0.6](docs/DISENO_0.6.md).

### Agregado
- **Seis máquinas, una por canal**, con nombres que recuerdan a las que las inspiraron: **BR-8** (T1, batería modelada), **BP-1** (T2, sampler de pads), **BB-303** o **B-MINI** (T3, se cambia con ALGORITHM y las notas se quedan), **BX-7** (T4, FM), **BITTAL** (T5, wavetable) y **BUNO-6** (T6, polifónico). En HOME, el mezclador **BX-1**.
- **B-MINI:** tres osciladores (TRI, SHARK, SAW, SQUARE, WIDE, NARROW) en registros de 32' a 2', ruido, filtro escalera con drive, CONTOUR y LOUDNESS, LFO y glide.
- **BITTAL:** dos osciladores de tabla (hasta 8 frames, armados desde un espectro de 256 armónicos y sin aliasing en ninguna nota), unísono de hasta 4, filtro LP 24/12, BP, HP o escalera, dos ADSR, LFO y 4 macros con destino. Las páginas de los osciladores dibujan la tabla en 2.5D. ALGORITHM elige la tabla del oscilador 1 entre las del banco.
- **BUNO-6:** 6 voces con un oscilador cada una (sierra, pulso con PWM, sub y ruido), pasa-altos de 4 posiciones, filtro de 24 dB, una ADSR, LFO con retardo y **chorus estéreo** I, II o I+II (ALGORITHM).
- **Banco de sonidos** (.bsnap) para B-MINI, BITTAL y BUNO-6: 37 de fábrica. Los de BITTAL y B-MINI salen de presets de Vital con licencia libre (CC0, Unlicense y GPL-3.0), renombrados y con su crédito en [CREDITOS.md](CREDITOS.md); los 12 de BUNO-6 son propios, hechos en Vital. `tools/gen_snaps.mjs` los arma con los commits de origen fijados.
- **Sonidos propios:** el editor web (`sonidos.html`) convierte un preset de Vital en un sonido de Bubba Machine y lo manda al FM-1 por WebMIDI; también carga **bancos .syx de la DX7** (32 voces) en BX-7. En la computadora: `tools/vital2bsnap.mjs` y `tools/fm1/sonido.py`. En el emulador, el botón *Sonido*.
- **BX-7:** el dibujo del algoritmo como en una DX7 (portadoras, moduladores, realimentación y nivel de cada operador) y PRESETS con las voces de tus bancos.
- **Reverb** (Dattorro) con envío por canal en MEZCLA y su página en HOME + FX; **filtro del master** (pasa-bajos a la izquierda, pasa-altos a la derecha) y **BEAT FX** (un punch-in que queda puesto) en HOME.
- **REC expresivo** en los canales de notas: cada nota guarda su micro-tiempo y su gate reales, se corta en el compás si sigue apretada y lo que mueves en K1-K4 mientras la sostienes queda como P-Lock. Con REC, SELECT elige un paso, ALGORITHM su largo (hasta TIE) y PRESETS su micro-tiempo.
- **Nota larga con dos blancas:** mantén el paso donde empieza y toca la blanca donde termina.
- **BP-1, loop de sustain** (página LOOP del pad): sostenido repite un tramo y al soltar sigue hasta el final.
- **MIDI OUT por canal** (MAS): el canal suena y además manda sus notas por MIDI. Reemplaza al motor MIDI.
- **Carga del audio** (CPU) en la cabecera.
- **Dos pruebas en SISTEMA**, apagadas de fábrica: **PANTALLA RÁPIDA** (SPI a 30 MHz: el cuadro baja de 85 a 39 ms) y **DOS NÚCLEOS** (T4-T6 en el segundo núcleo; en la computadora da las mismas muestras). Si el FM-1 se cuelga o arranca en modo seguro, las dos se apagan solas.
- **Instalador web plug and play:** la página baja el paquete ya armado de Bubba Machine (sin pedir el V15 de M-VAVE), lo instala y, si está en español, carga el paquete de idioma al terminar. El V15 solo hace falta para volver al firmware de fábrica.

### Cambiado
- **La cara nueva:** cabecera, visor, teclado y diales siempre en el mismo lugar, con paleta neón (cian lo activo, ámbar los valores, rojo REC y mute, verde menta las notas largas). En SEQ las blancas son los pasos; los diales de K1-K4 muestran valor y nombre.
- **Las perillas:** en HOME, SELECT es el cursor del mezclador, ALGORITHM el BPM y K1-K4 volumen, envío FX, filtro y BEAT FX. Dentro de un canal, ALGORITHM es el modo de la máquina (instrumento, pad, algoritmo, tabla, chorus). Se cambia de canal con HOME + SELECT. Fuera el recuadro de 400 ms y ENTRADA AUTO.
- **Las tomas comparten 100 KiB de flash con los sonidos del banco** (las tomas desde abajo, los sonidos desde arriba). Hasta 3 s por toma.
- Fuera **ANALOG**, los canales libres, **KIT** en T4-T6 y el motor **MIDI**: cada canal es su máquina.
- 22 voces en total, con un tope por canal.
- Audio determinista: suena igual en cada arranque.

## [0.5.0-beta] - 2026-10-12

**Beta, todavía no probada en el aparato:** pasa las pruebas automáticas y el emulador. Si algo falla en el FM-1, vuelve a la 0.4.0-beta o al firmware de fábrica desde el instalador.

**La pantalla pasa al inglés**, para que Bubba Machine se entienda en todo el mundo. El español sigue disponible como paquete de idioma, que no viene cargado. **La memoria de tomas baja de 7.4 s a 6.4 s**: 16 KiB de la flash son ahora del paquete de idioma, y una toma que usaba ese lugar se pierde. **El sampler pasa a ser de pads** y el formato de los patrones cambia: los patrones guardados se borran al primer arranque (las tomas se quedan).

### Agregado
- **Paquetes de idioma.** Las frases de la pantalla están en inglés y un paquete (`.bmlang`) las traduce. Vive en su propia zona de la flash (`0xD9000`, 16 KiB), así que no toca patrones ni tomas y una actualización del firmware no lo borra. **SISTEMA › LANGUAGE** cambia entre el inglés y el paquete cargado. Lo que el paquete no traduce se ve en inglés.
  - **Español** (`lang/es.txt`, armado en `lang/es.bmlang`): la pantalla queda como antes.
  - Se carga en el FM-1 con `tools/fm1/idioma.py` (por la consola USB-MIDI, con el transporte parado) y en el emulador con un botón; en la página del emulador en español, la primera vez ya arranca en español.
  - `tools/gen_lang.py` junta todas las frases en `lang/plantilla.txt`, revisa cada traducción (solo ASCII, los mismos `%`) y arma los paquetes. Cualquiera puede hacer otro idioma.

### Cambiado
- **T2 es un sampler de 16 pads**, como un KO II o el drum sampler de un OP-1 field. Reemplaza al loop en slices con generador (y a SLICER, CORTAR y CHOPS).
  - **Cada pad es un trozo de una toma**, con su inicio y su largo (se pueden encimar), tono, nivel, modo de tecla (**ONESHOT**, **KEY**, **THRU**: sostenido sigue por la toma, **LOOP**), sentido, ataque, salida, grupo de corte y si sigue el tempo. Un kit puede mezclar tomas.
  - **La toma arma el kit:** al guardarse una toma con el kit vacío, se corta en los golpes, cada corte va a un pad y la rejilla queda escrita para que suene como la toma. En **TAKES**: N1 kit en los golpes, N2 en 16, N3 en 8, N4 la toma entera en el pad elegido; las blancas la escuchan en 16 partes.
  - **SEQ es una rejilla como la de la batería:** una línea por pad, con su largo, acentos, repeticiones, condiciones, mutes, fill y P-Lock por paso de FILTER, PITCH, DECAY y START. Se acabaron los números sueltos: se ve qué pad suena en qué paso.
  - **PADS** (EDIT): la onda de la toma con el pad entre corchetes y los demás pads marcados, y tres páginas (TRIM, SOUND, ENV; EDIT otra vez pasa). Negras: copiar, pegar, vaciar, el inicio al golpe; 9-11 reversa, media velocidad y stutter.
  - **BREAK** (SEQ otra vez): el generador de antes, ahora sobre la rejilla y apagado de fábrica. VARY cambia pads, ANCHOR deja los tiempos fuertes, REPEAT y BREAK. La fila PLAYS muestra lo que va a sonar y EDIT + negra 4 lo escribe en la rejilla.
  - En la grabadora, las blancas escuchan la toma nueva antes de guardarla.
- El dibujo del sampler (SOUND) es la misma onda espejada; ya no hay tres carriles por frecuencia.
- Los nombres de los modelos de batería, los patches de FM y las ondas también pasan al inglés (con su traducción en el paquete).

## [0.4.0-beta] - 2026-10-11

**Beta: la primera versión que corre en el FM-1.** Se probó en un solo aparato (el del autor) y todavía cambia mucho. **Una actualización puede borrar lo que guardaste** (patrones, canción, ajustes y tomas del sampler): hasta la v1 no hay compatibilidad hacia atrás, y cuando cambia un formato lo anterior se borra al arrancar y vuelve la demo. Si algo te importa, grábalo por la salida de audio antes de actualizar.

Los sonidos se rehicieron: **ya no hay sonidos grabados ni wavetables**, todo se calcula en vivo con bibliotecas de síntesis probadas. Lo guardado con la 0.3 (patrones, canción, ajustes **y tomas**) no se lee: se borra al primer arranque y arranca la demo.

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
- **SLICER** (SEQ otra vez en T2): el sampler corta la toma en **16 cortes**, uno por blanca. Las blancas tocan los cortes y, con REC y PLAY, los fijan en el paso; K1 elige, K2 mueve, K3 sensibilidad, K4 ganancia; negras 1-3: cortes en los golpes, 16 u 8 iguales; 9-11 reversa, media velocidad y stutter. Los cortes en los golpes buscan los ataques (sirven también para voces).
- **Tira de teclas en pantalla:** cada página de canal, MEZCLA y ESCENAS dibuja las 11 negras y las 16 blancas con su nombre corto y el mismo estado que su luz (las luces son blancas y no dicen qué hace la tecla). En HOME la tira es grande. Donde las blancas ya se ven en la pantalla (SEQ, CORTAR, SLICER), la tira muestra solo las negras.
- **Batería por secciones de 8:** el SEQ de la batería muestra 8 instrumentos con su nombre en letra normal; la negra 9 cambia de sección.
- **Subir una toma desde la computadora:** `tools/fm1/toma.py` convierte cualquier audio (con ffmpeg) y lo manda al FM-1 por USB-MIDI; queda como una toma recién grabada.
- **Demo nueva:** acid techno a 160 BPM (909, una línea 303 con acentos y slides y el sampler cortando la primera toma).
- **FM-1, Bubba Machine completo para el aparato** (beta, probado en el FM-1 del autor): el núcleo con sonido por el codec (ALNK0), la matriz y los LEDs por interrupción, pantalla, USB-MIDI, guardado, modo seguro y servicio de actualización.
- **FM-1, firmware de diagnóstico**: sin sonido que muestra cada botón, tecla y perilla, el MASTER, la batería y el USB-MIDI, con modo seguro, salida al modo del chip y el servicio de actualización.
- **Instalador web** (`instalar.html`): arma el paquete en el navegador sobre el FM-1 V15 de M-VAVE (que cada quien descarga) y lo instala con el cargador oficial; lee el FM-1 solo (dice qué firmware tiene, o por qué no lo encuentra), se elige qué instalar (Bubba Machine, diagnóstico o firmware de fábrica) y queda un botón; también vuelve al firmware de fábrica y continúa una instalación cortada. `tools/fm1/servir.py` lo sirve en la computadora con el V15 ya cargado.
- **Herramientas del FM-1:** `tools/fm1/fwsc.py` (empaquetador, comprobado byte por byte contra el V15) y `tools/fm1/rescate.py` (rescate desde Windows por el modo del chip, con respaldo completo antes de escribir).

### Cambiado
- **Negras 10 y 11 en los canales:** abren pantallas. La **10** (batería) abre los **MUTES** de los instrumentos y la **11** abre el **FILL**, donde suena el fill y las blancas arman sus golpes (o marcan notas solo para el fill), con K1 = fill automático. Un toque las deja abiertas (se cierran con la misma tecla o HOME); mantenerlas = solo mientras las sostienes. Un toque ya no mutea a ciegas: cada mute avisa y el instrumento muteado queda en rojo.
- **ALGORITHM dentro de un canal** también pasa por el recuadro con el número antes de cambiar; en INSTR elige instrumento y lo hace sonar.
- **HOME explica las negras** (mutes, solo, escenas, fill, y las pantallas de la batería).
- **Fuera los motores viejos:** BAJO, SYNTH (wavetable), MONO y el ANALOG de 6 timbres pasan al ANALOG nuevo; los kits grabados KO-808 y KO-909 y el kit SINTE, a la batería modelada. El programa del FM-1 ocupa 148 KiB menos y se liberan 64 KiB de RAM.
- Patrones en **formato 5**: cada canal guarda 32 ajustes de sonido, cada instrumento de batería su modelo y su golpe, y la nota del sampler su corte (4 bits). Las tomas pasan a la firma **BMT4** (16 cortes en la cabecera).
- **Fuera los loops de fábrica:** el sampler toca solo tomas (grabadas o subidas con `tools/fm1/toma.py`).
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

[0.4.0-beta]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.4.0-beta
[0.3.0]: https://github.com/Erbubar23/bubba-machine/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commit/3118530
