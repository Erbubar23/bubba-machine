# Hoja de ruta

Todo lo que está contemplado para Bubba Machine. **Se va a hacer todo, poco a poco**, sin fechas: cada paso sale cuando está probado en el FM-1. El orden dentro de cada lista es el orden en que importa hoy y puede cambiar.

Lo marcado ✓ ya está en la versión actual (0.6.0-beta); lo marcado **a prueba** está, pero apagado de fábrica hasta medirlo en el FM-1. Hasta la v1, cualquier versión puede borrar lo guardado (ver [instalar](INSTALAR.md)).

## Audio

El objetivo es la mejor calidad que permite el FM-1. Qué se mejoró, cuánto cuesta y qué limita hoy: [audio](AUDIO.md).

**Hecho**
- ✓ Motores y mezcla a 44.1 kHz, bus en coma flotante, delay y captura de efectos a 16 bits.
- ✓ Batería modelada (Plaits), FM de 6 operadores (msfa) y el bajo ACID.
- ✓ Caché de datos de 32 KiB y audio compilado para velocidad.
- ✓ **Las máquinas de la 0.6:** B-MINI, BITTAL (wavetable sin aliasing, desde presets de Vital) y BUNO-6 (con chorus estéreo).
- ✓ **Reverb** con envío por canal, **filtro del master** y **BEAT FX**.
- **A prueba:** audio en los dos núcleos (T4-T6 en el segundo; en la computadora da las mismas muestras) y la pantalla con SPI a 30 MHz.

**Contemplado**
1. **Medir la 0.6 en el FM-1:** el sonido y la carga de cada máquina, el chorus y el pasa-altos de BUNO-6 (hoy con valores aproximados) y el volumen parejo entre los sonidos de BITTAL. Después, prender de fábrica los dos núcleos y la pantalla rápida.
2. **Efectos de master:** delay de cinta, compresor y nivelador.
3. **ACID 2.0:** Open303 en coma flotante, con un generador de líneas acid.
4. **Sampler de más calidad y más segundos**, dentro de lo que deja la flash (hoy graba a 16 kHz y 8 bits).
5. **Morph A/B por instrumento de batería:** dos versiones de cada golpe y una perilla que pasa de una a otra.
6. **Motores CUERDA y MODAL** (modelado físico de Plaits): ya tienen lugar en el formato, falta elegirlos.
7. **Clipper con sobremuestreo**, para que el DRIVE alto no genere aliasing.
8. **Pantalla por DMA:** el SPI a 30 MHz ya está a prueba; con DMA el envío deja libre al procesador para el audio.
9. **Medir la salida analógica** del FM-1 (ruido, distorsión, respuesta) con una interfaz de audio.

## Interfaz

La interfaz ordena el FM-1 en dos lugares: **tocar en vivo** (HOME y sus vueltas) y **trabajar la pista** (dentro de un canal). Las luces de las teclas son solo blancas y no dicen qué hace cada tecla, así que cada pantalla lo muestra en la tira de teclas.

**Hecho**
- ✓ **Tira de teclas en pantalla:** las 11 negras y las 16 blancas con su nombre corto y la misma intensidad que su luz.
- ✓ **Pantalla en inglés, con paquetes de idioma:** el español se carga aparte (`tools/fm1/idioma.py`) y se cambia en SISTEMA › LANGUAGE.
- ✓ **Sampler de pads:** 16 pads hechos con tus tomas, como un KO II o un OP-1 field, en una rejilla como la de la batería, con BREAK para variarla.
- ✓ Pantallas de **MUTES** y **FILL**, y la batería por secciones de 8.
- ✓ **La cara de la 0.6:** cabecera, visor, teclado y diales siempre en el mismo lugar; el mezclador **BX-1** en HOME; ALGORITHM como el modo de cada máquina.
- ✓ **REC expresivo:** micro-tiempo, gate y P-Lock de lo que mueves mientras sostienes la nota; notas largas con dos blancas.

**Contemplado: tocar en vivo (HOME otra vez = la página siguiente)**
1. **MORPH A·B:** pasar de un estado del sonido a otro con una perilla (la cuarta macro de HOME).
2. **ESCENAS Y CANCIÓN:** armar y tocar la canción con las escenas.

**Contemplado: trabajar la pista (el botón otra vez = la página siguiente)**
1. **SEQ › LOOP › DADOS:** variaciones al azar controladas, y el generador ACID para las líneas de bajo.
2. **EDIT › SONIDO › INSTR:** el instrumento A/B de la batería.
3. **LFO › MOVIMIENTO:** un secuenciador de modulación y grabar el movimiento de las perillas.
4. **FX › PISTA:** mezcla y efectos por pista.

**Otros**
- **Bancos de patrones:** más de 20 patrones, ampliando el mapa de la flash o achicando los patrones.
- **Notas fantasma en la batería:** un P-Lock de nivel o un formato más grande.

## Herramientas y ecosistema

- **Respaldo de lo guardado desde el navegador** (patrones, canción y tomas), para no perder nada al actualizar.
- **Grabar directo a un pad** (REC + pad, como el KO II) y deshacer los cambios de un pad.
- ✓ **Instalador web plug and play:** instala el paquete armado sin el V15 y carga el español al terminar.
- ✓ **Editor de sonidos web:** presets de Vital y bancos `.syx` de la DX7, mandados al FM-1 por WebMIDI.
- **Más idiomas** hechos por la comunidad.
- **Editor web de patrones y tomas:** cargarlos y bajarlos por USB (hoy las tomas se suben con `tools/fm1/toma.py`).
- **Packs de kits y tomas libres** (CC0 o GPL), hechos por la comunidad.
- **Bluetooth MIDI**, si el hardware y el procesador lo permiten (sin promesa: ningún firmware libre lo ha demostrado).

## Hecho

- **0.6.0-beta:** seis máquinas fijas, la cara nueva, el banco de sonidos y el instalador plug and play. El diseño está en [`DISENO_0.6.md`](DISENO_0.6.md).
- **0.5.0-beta:** el sampler de pads y la pantalla en inglés, con el español como paquete de idioma.
- **0.4.0-beta:** la primera versión para el FM-1, sonidos calculados en vivo y audio a 44.1 kHz. Ver el [registro de cambios](../CHANGELOG.md).
- **0.3:** la interfaz 3.0, con cuatro gestos por botón y HOME en todas partes. El diseño está en [`DISENO_3.0.md`](DISENO_3.0.md).
