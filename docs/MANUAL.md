# Bubba Machine 0.2 — Manual

Bubba Machine convierte el FM-1 en una groovebox de directo de 6 pistas. Tiene batería estilo TR, un sampler cuyos chops se secuencian igual que la batería, y bajo y synths con loopera. Cada pista puede tener su propio largo y su propia velocidad.

## Tres formas de apretar un botón

| Pulsación | Qué hace |
|---|---|
| **Toque** | Abre la pantalla del botón. Otro toque: su página siguiente |
| **Mantener** (300 ms) | Abre su **capa en vivo** mientras lo sostienes. Al soltar, vuelves a donde estabas |
| **Mantener 2 s** | Abre una **pantalla especial**. Un anillo amarillo arriba se llena mientras esperas. Si tocas otra cosa durante la pulsación, los 2 s se cancelan |

## La pantalla

- **Arriba:** transporte, patrón (y el que sigue en cola), compás y tiempo, pista, BPM. También la escena que suena (A-D), **F** cuando suena el fill, el anillo de los 2 s y la batería.
- **En medio:** lo que hace el modo actual. Los valores que giras aparecen aquí encima y se van solos.
- **Abajo:** qué hace cada perilla K1-K4, en su color y en su orden físico.
- **Borde rojo:** la mezcla saturó.

## Las pistas

| Pista | Qué es |
|---|---|
| **T1 DRUMS** | Batería TR: 16 instrumentos (kit KO-808 o KO-909), cada uno con su línea de pasos |
| **T2 SAMPLER** | 16 chops de un sample (slot A o B). Se secuencian igual que la batería |
| **T3 BASS** | Wavetable monofónico con filtro y glide |
| **T4-T6 SYN** | Wavetable polifónico con filtro |

**ALGORITHM** cambia de pista desde cualquier pantalla.

## Botones

| Botón | Toque | Otra vez | Mantener | 2 s |
|---|---|---|---|---|
| **HOME** | Pistas y mezclador | **Escenas** | Mute (blancas 1-6) y solo (9-14) | **Sistema** |
| **SEQ** | Pasos | Synth: **loopera** | Patrones (blanca = siguiente) | **Tiempo** |
| **EDIT** | Sonido (kit, sample u onda) | TR: instrumento | **Borrar** estilo TR | T2: **sampler** · otras: sonido inicial |
| **ENV** | Envolvente | Página 2 | — | — |
| **FX** | Delay y sidechain | — | **Punch-in** | — |
| **SCL** | Escala | **Cuadrícula de acordes** | Acordes en vivo | — |
| **ARP** | Arpegiador | — | Synth: arpegio · TR: **roll** | — |
| **SAVE** | Menú de guardado | Ejecuta | — | Guardado rápido |
| **GLO** | Global (BPM, swing, largo maestro, drive, fills) | — | Modificador | — |
| **REC** | Grabar en vivo (en LOOP: loopera · en el sampler: grabar) | — | — | **Borrar la pista** |
| **PLAY** | Play/stop | — | — | — |
| **OCT − / +** | Octava · en SEQ: página · en BORRAR: deshacer / rehacer | — | — | — |

**GLO sostenido +:** KNOB 1-4 = BPM · swing · largo maestro · drive · SELECT = fill automático · ALGORITHM = kit / slot / banco de ondas · SEQ = canción · OCT = transponer · negras 1-6 = mute.

## Batería TR (SEQ en T1 y T2)

La pantalla es una matriz: 16 instrumentos × los 16 pasos de la página. Cada línea muestra su cabezal, porque cada instrumento puede tener su propio largo.

| Control | Hace |
|---|---|
| **Blancas** | Pasos del instrumento elegido: toque = poner o quitar |
| **Negras 1-8** | Eligen (y tocan) un instrumento de la sección |
| **Negra 9** | Cambia de sección (1-8 / 9-16) |
| **Negra 10** | FILL mientras la sostienes |
| **Negra 11** | Toque: mute del instrumento · mantener: mutes de todos |
| **OCT − / +** | Página |
| **SELECT** | Instrumento |

**Paso sostenido + perilla:**
- KNOB 1-4: P-Lock de TONE, TUNE, DECAY o START, solo en ese paso.
- SELECT: acento.
- ALGORITHM: repetición ×1 a ×4, o FLAM.
- PRESETS: condición (SIEMPRE, SOLO FILL, SIN FILL).

**EDIT otra vez (INSTRUMENTO):** nivel, afinación, decay y **largo de la línea**. Por ejemplo, un hi-hat de 12 pasos contra un bombo de 16.

**BORRAR (EDIT sostenido):**
- Sonando, sostener una blanca borra ese instrumento en cada paso por donde pasa el cabezal. Parado, lo borra entero.
- Negras 1-3: copiar, pegar y limpiar la pista.
- OCT − / +: deshacer y rehacer.

**ROLL (ARP sostenido):** cada blanca repite su instrumento a la velocidad de K1 (1/8, 1/16, 1/32, tresillos). Con REC encendido, se graba como pasos con repetición.

**Fills:** los pasos con condición suenan o callan en el fill. El fill suena mientras sostienes la negra 10, o solo cada 2, 4 u 8 compases (GLO + SELECT).

## Tiempo (SEQ 2 s)

Muestra las 6 pistas como barras de su duración real, con el cabezal de cada una: el polímetro a la vista.

- SELECT o ALGORITHM: elige la pista.
- K1: largo (1-64).
- K2: velocidad (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4).
- K3 y K4 (solo pistas TR): elige un instrumento y cambia su largo.

El **largo maestro** (GLO + K3: 16, 32, 48, 64, 128 o INF) reinicia todas las pistas a la vez. Ahí cambian los patrones y las escenas.

## Sampler (T2)

1. Elige T2 y abre **EDIT**.
2. **Mantén REC:** graba lo que está sonando (hasta 3 s, 16 kHz).
3. Mientras grabas, cada **blanca** marca un chop. Sin chops, el sample se divide en 16 partes iguales.
4. Al **parar**, el sample se guarda solo en su slot (A o B, hasta 4 s cada uno). Mientras no se guarda, el delay y los punch-in con buffer quedan en pausa, porque comparten la memoria.
5. Los chops son los 16 instrumentos de la rejilla TR de T2.

**SAMPLER (EDIT 2 s en T2):**
- K1: slot A o B.
- K2: chops manuales o 4, 8 o 16 partes iguales.
- Negra 11 dos veces: borra el slot.

## Synths

- **SEQ:** como en 0.1. Hasta 4 notas por paso. Paso sostenido: KNOB = P-Lock, SELECT = nota, ALGORITHM = probabilidad o fill, PRESETS = largo o TIE (glide).
- **LOOP (SEQ otra vez):** loopera de notas sin cuantizar.
  - K1 ON/OFF · K2 compases (1, 2 o 4).
  - **REC** arma la grabación (empieza en el siguiente compás). Al cerrar el loop pasa a PLAY. REC otra vez: sobregrabar (OVERDUB). REC otra vez: PLAY.
  - **REC 2 s** borra el loop.

## Acordes: la cuadrícula (SCL otra vez)

Las 16 blancas son 16 acordes, dibujados como una cuadrícula de 4×4 (fila 1 = blancas 1-4). Cada pad muestra el **grado** (I, ii, V7…) y el **nombre** del acorde. Su color dice su función:
- **Azul (reposo):** I, iii, vi.
- **Verde (preparación):** ii, IV.
- **Naranja (tensión):** V, vii.

Se lee como un mapa: de verde a naranja a azul suena a "ir y volver".

- **K1:** juego de acordes.
  - DIATÓNICO: tríadas y séptimas.
  - PROGRESIONES: cada fila es una progresión clásica, de izquierda a derecha.
  - NEO SOUL: séptimas y novenas.
- **K2:** voicing (cerrado, abierto o conducido: cada acorde en la inversión más cercana al anterior).
- **K3:** octava · **K4:** strum.
- **Negras 1-4** (mientras las sostienes): +7, +9, SUS2, SUS4.
- Los acordes suenan en la pista synth elegida (o en T4 si estás en una pista TR).
- **SCL sostenido** activa la cuadrícula en las teclas desde cualquier pantalla.

## Escenas (HOME otra vez)

Una escena = **patrón + mutes** (de las pistas y de los instrumentos TR).

- Blancas 1-4: lanzan A-D en el siguiente ciclo maestro (parado: al instante).
- Blancas 5-8: guardan lo que suena en A-D.
- Mantener una de 1-4 y tocar otras: **cadena** en bucle.
- **REC:** SONG REC. Cada ciclo que suena queda escrito en la canción (GLO + SEQ para verla y editarla).

## Efectos

- **FX:** tiempo del delay, feedback, mezcla y release del sidechain. El envío al delay y el ducking de cada pista se ajustan en HOME (K3, K4).
- **FX sostenido + blanca:** 16 punch-in cuantizados a 1/16 (stutter, reverse, tape stop, gate, sweeps, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (GLO + K4): saturación de la mezcla.

## Guardar

- La flash **solo se escribe en STOP** (escribir detiene el audio unos instantes).
- **Autoguardado:** parado, después de 10 s sin tocar nada.
- **SAVE:** guardar · guardar como · guardar canción y escenas · borrar patrón (SAVE dos veces) · contar eventos.
- **SAVE 2 s:** guardado rápido.

## Sistema (HOME 2 s)

Reloj interno o externo · Split Sync (L = pulso, R = mezcla mono) · PPQN y latencia del pulso · autoguardado · brillo de los LEDs en reposo · fuente del sampler · patrones guardados · samples · versión.

## MIDI

- Entrada (USB y TRS): canales 1-6 = pistas. En T1 y T2, la nota elige el instrumento (0-15).
- Reloj externo: sigue el MIDI Clock y Start/Stop.
- Salida: MIDI Clock cuando el reloj es interno.
