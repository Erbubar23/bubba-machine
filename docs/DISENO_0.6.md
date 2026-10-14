# Bubba Machine 0.6: identidad propia y máquinas

> **Estado: diseño decidido (2026-10-14), sin implementar** salvo la zona de tomas (sección 7). Parte de un prompt externo ("Prompt Maestro": mezclador MX-1, Bubba Drums, sampler OP-1, bajos 303 / D MINI, FM DX7 y un wavetable que carga presets de Vital) y lo contrasta con lo que medimos en el FM-1. Lo que el prompt dice y el aparato no hace está corregido aquí, con su fuente.

## 1. La idea en una línea

**El FM-1 se convierte en seis máquinas fijas sobre cuatro núcleos de sonido, con una sola cara visual.** Cada máquina lleva un nombre que remite a la original (BX-7 por la DX7, BB-303 por la TB-303...), sin copiar la marca. No se eligen motores: cada canal *es* una máquina. Y la idea del autor: **el núcleo wavetable carga presets de Vital y las otras máquinas de síntesis salen de ese mismo núcleo, limitadas a su función** (un bajo ácido no tiene unísono de 5 voces; un pad no tiene slide).

## 2. Lo que dice el prompt contra nuestros datos

| El prompt dice | Lo medido / lo real | Qué hacemos |
|---|---|---|
| CPU pi32v2 a 240 MHz | **360 MHz**, dos núcleos con FPU (240 es el firmware de fábrica) | Presupuesto con 360 MHz y un solo núcleo hasta que el audio use los dos ([ARQUITECTURA_FM1](ARQUITECTURA_FM1.md)) |
| DSP en coma fija Q15/Q31 | La FPU es **1.5× más rápida** que la aritmética entera de 64 bits que usaba la 0.3 ([AUDIO](AUDIO.md)) | El cálculo sigue en float. Las **tablas** sí en int16 (la mitad de memoria) |
| 44.1 kHz | El codec corre a **44 117.6 Hz** (24 MHz / 544) | Se afina con la frecuencia real, como hoy |
| Encoder con push / shift "en décimas" | **Los 7 encoders no tienen pulsador** ([SPEC_V3](SPEC_V3.md) §2) | Las décimas de BPM con HOME + ALGORITHM (HOME es nuestro modificador) |
| LEDs que iluminan la escala, estela de colores | **Las luces son solo blancas**: encendida, media (15 %), tenue (~4 %), apagada | La estela de un TIE sí (intensidades); lo que hace cada tecla lo dice la pantalla |
| Teclas 1-27 numeradas en fila (1-6 mute, 7-22 escenas) | 11 negras + 16 blancas, y ya tenemos una gramática: negras 1-6 mutes, 7-10 escenas, 11 FILL; blancas = pasos o patrones | Se queda nuestra gramática: es la misma idea con las teclas que existen |
| 4 franjas sin tira de teclas | Regla del proyecto: **cada pantalla dice qué hace cada tecla** | La franja de pasos pasa a ser el teclado dibujado (sección 5) |
| Wavetable de **256 muestras × 8 frames** | 256 muestras = 128 armónicos: un bajo en La1 (55 Hz) se corta en **7 kHz**. Vital usa **2048 muestras** por frame y 257 posiciones (medido en sus presets) | Guardar el **espectro** (hasta 256 armónicos) y armar las tablas en RAM (sección 4) |
| Mezclador con envíos de efectos y Beat FX | **No hay reverb, chorus ni compresor** todavía: solo delay y clipper ([AUDIO](AUDIO.md)) | Los efectos pasan a ser requisito, no extra (sección 6) |
| Telemetría y visor 2.5D en tiempo real | La pantalla va por SPI a 12 MHz: un cuadro completo tarda ~100 ms | Pantalla rápida (SPI 30 MHz + DMA, 2.2× medido) **antes** de la interfaz nueva |
| Micro-tipografía; wireframes de 50 columnas | Con la letra 5×7 caben **40 caracteres** por línea, sin márgenes | Cada franja con ≤ 32 caracteres; los textos del prompt se recortan |
| "VITAL LITE", "MX-1", "TB-303", "D MINI", "OP-1", "DX7" en pantalla | El código de Vital (GPL-3.0 o posterior) **prohíbe usar el nombre "Vital"** para nombrar binarios hechos con él; los otros son marcas de Roland, Behringer, Teenage Engineering y Yamaha | **Nombres que remiten a la original sin ser ella**: la B de Bubba en lugar de la letra de la marca (sección 3.1) |
| Presets de Vital precargados | Los presets de fábrica de Vital tienen una licencia aparte que **no permite redistribuirlos** (README de mtytel/vital) | Solo presets con licencia libre, o propios (sección 4.4) |

Lo que el prompt pide y **ya existe**: Bubba Drums (T1, Plaits, 16 instrumentos, rejilla TR); el sampler de pads (T2) con inicio, largo, tono y modo LOOP; ACID (filtro escalera, acento, slide); FM de 6 operadores con los 32 algoritmos (msfa); la grabación libre con el largo de cada nota (hasta 4 compases); P-Locks manteniendo un paso; MEZCLA y ESCENAS en HOME; el secuenciador a 96 PPQN desde el contador de muestras.

## 3. Arquitectura: seis máquinas, cuatro núcleos

| Canal | Máquina | Remite a | Núcleo |
|---|---|---|---|
| — | **BX-1** (HOME, el mezclador) | Roland AIRA MX-1 | — |
| T1 | **BR-8** (Bubba Drums, kits 808 y 909) | Roland TR-8 / TR-808 / TR-909 | DRUMS (Plaits) |
| T2 | **BP-1** (sampler de pads) | Teenage Engineering OP-1 / KO II | SAMPLER |
| T3 | **BB-303** o **B-MINI** (ALGORITHM las cambia) | Roland TB-303 / Minimoog Model D (D MINI) | **WAVE** |
| T4 | **BX-7** | Yamaha DX7 | FM (msfa) |
| T5 | **BITTAL** | Vital | **WAVE** |
| T6 | **BUNO-6** | Roland Juno-6 / 60 / 106 | **WAVE** |

### 3.1 Los nombres

La regla: **la B de Bubba toma el lugar de la primera letra** y se conserva el número o la forma, para que se reconozca la máquina sin usar su marca. En la cabecera de cada pantalla va el nombre corto (≤ 8 caracteres). El sitio y el manual dicen "inspirado en" y que Bubba no tiene relación con esas marcas. **BITTAL** es la elección del autor (2026-10-14). Con dos T se distingue más de "Vital", que su README pide no usar para nombrar binarios.

### 3.2 Un núcleo WAVE, cuatro máquinas

**Por qué un núcleo para todas las máquinas sustractivas y de tablas.** Hoy ANALOG y ACID son dos motores con dos bucles calientes. En la 0.4 aprendimos que cada bucle se optimiza a mano (pasadas cortas: casi 2×; filtro cada 8 muestras; coeficientes en caché). Con un solo núcleo:

- **un bucle que optimizar y medir**, con caminos especializados por cara (en C++ con plantillas, como ya compilamos los motores), para que BB-303 no pague el unísono de BITTAL;
- **un formato de sonido para todo** (el snapshot, sección 4): un preset de Vital se carga en la máquina cuyos límites respeta (un bajo de un oscilador en B-MINI, una cuerda de sierra y pulso en BUNO-6);
- **menos programa**: ANALOG y ACID se van; su oscilador polyBLEP y su filtro escalera son el modo "clásico" de WAVE.

| Cara | Osciladores | Filtro | Envolventes y modulación | Voces | Lo propio de la cara |
|---|---|---|---|---|---|
| **BB-303** | 1, sierra o cuadrada (polyBLEP) | escalera 4 polos | ENV MOD + DECAY; ACCENT | mono | slide con las notas ligadas; el acento carga la envolvente como una 303 |
| **B-MINI** | 3 clásicos (polyBLEP), desafinación y mezcla | escalera 4 polos con drive cálido (CUTOFF, EMPHASIS, CONTOUR) | 2 ADSR con sustain real | mono / legato | notas largas con sustain; glide |
| **BITTAL** | 2 wavetables (de un snapshot) + unísono ≤ 4 | 1 filtro: ANALOG, LADDER, DIGITAL, DIRTY o DIODE | 2 ADSR, 1 LFO, **4 macros** | 4 por canal | carga presets de Vital (sección 4) |
| **BUNO-6** | 1 DCO por voz: sierra, pulso con PWM, sub una octava abajo y ruido | pasa-altos de 4 posiciones + pasa-bajos de 24 dB con resonancia | 1 ADSR (al filtro y al volumen, o volumen en GATE), 1 LFO con retardo | **6** | el **chorus I / II / I+II** de BBD, que es la mitad del sonido; ALGORITHM lo elige |

**BUNO-6 sale de presets de fábrica propios hechos en Vital**, como pidió el autor: en Vital se arma un patch "a lo Juno" (oscilador 1 con sierra y pulso, oscilador 2 cuadrado una octava abajo como sub, ruido, filtro 1 analógico de 24 dB, filtro 2 pasa-altos, ENV 1, LFO 1 y chorus) y el editor lo traduce a los ajustes de BUNO-6; lo que quede fuera de esos límites, lo avisa y lo deja afuera. Como sus ondas son clásicas (polyBLEP), **el snapshot no lleva tabla: mide ~150 B**, y caben cientos en el banco.

El costo esperado es el de un oscilador de ANALOG por voz (ANALOG, con dos osciladores, mide 4.1 % de un núcleo): seis voces, del orden de 15-25 % de un núcleo, a medir. El chorus de BBD hay que modelarlo (dos líneas de retardo moduladas y filtradas, en estéreo); sus velocidades y profundidades se toman del manual de servicio de Roland, no de clones.

**Lo que no entra en WAVE:** BX-7 se queda en msfa (3.3 % de un núcleo por voz, lee `.syx`; la FM de Vital no son 6 operadores con 32 algoritmos), BR-8 en Plaits y BP-1 en el sampler. Esos tres ya existen y suenan.

**Lo que se quita** (lo pidió el autor: "si hay que quitar cosas como cambiar de sintes, hay que hacerlo"):

- **MAS › MOTOR** y la elección de motor en T4-T6: cada canal es fijo.
- Los motores **ANALOG** y **ACID** como motores aparte (pasan a ser B-MINI, BUNO-6 y BB-303), y los huecos de **CUERDA** y **MODAL** del formato.
- **KIT** en los canales libres (una segunda batería) y **MIDI** como motor: si se quiere MIDI, pasa a ser una opción por canal ("también manda por MIDI"), no una máquina.
- La paleta por canal de hoy (azul, verde, amarillo...): la reemplazan los cuatro acentos (sección 5).
- El formato de patrón cambia (canales fijos): se borra al arrancar, sin conversión (hasta la v1 no hay compatibilidad hacia atrás).

## 4. Snapshots: de un preset de Vital al FM-1

El editor web hace lo que el prompt llama "Git-snapshot": lee un `.vital`, se queda con lo que el FM-1 puede tocar y lo empaqueta. **Todo el cálculo pesado pasa en el navegador**; el FM-1 solo carga.

### 4.1 Qué trae un `.vital` (medido en 356 presets)

- Un JSON con ~780 ajustes (`settings`), `preset_style`, autor y los nombres de 4 macros.
- **Las wavetables viajan dentro del preset**, pero no ya calculadas: son *componentes* del editor de Vital. `Wave Source` trae los frames (2048 floats en base64) y se usa tal cual; `Line Source` son puntos; `Audio File Source` trae audio y lo corta; encima van modificadores (`Wave Warp`, `Frequency Filter`, `Wave Folder`, `Slew Limiter`...).
- 218 de 356 presets usan solo `Wave Source` o `Line Source`: esos se leen sin el editor de Vital. Para el resto hay que **compilar el código de wavetables de Vital a WASM** (es GPL-3.0 o posterior, compatible con Bubba, y ya usamos emsdk).

### 4.2 El paquete `.bsnap`

| Parte | Tamaño | Qué es |
|---|---:|---|
| Cabecera | 16 B | firma, versión, máquina (BB-303, B-MINI, BITTAL, BUNO-6), id de contenido (los 4 hex que muestra la pantalla, "#A4F9") |
| Ajustes | ~96 B | niveles, transposición, unísono, posición de la tabla, filtro, 2 ADSR, LFO, glide, 4 macros (destino y cantidad) |
| Nombre, autor, licencia | 32 B | para los créditos en la pantalla y en el sitio |
| Tabla (×1 o ×2; BB-303, B-MINI y BUNO-6 no llevan) | ~4 KiB c/u | 8 frames como **espectro**: hasta 256 armónicos, magnitud y fase de 8 bits |

Con una tabla, el snapshot mide ~4.2 KB (lo mismo que dice el prompt, pero con 256 armónicos en vez de 128). Las tablas repetidas entre snapshots se guardan una vez.

**Por qué espectro y no muestras:** 256 armónicos llegan a 20 kHz desde los 78 Hz y, más abajo, sobran para un grave. Al cargar, el FM-1 arma en RAM cada frame en 1024 muestras y sus versiones por octava (sin aliasing, como un mipmap), con una IFFT por frame: unos milisegundos, una sola vez.

### 4.3 Qué se traduce y qué no

| Se traduce | Se aproxima | No entra (el editor lo dice) |
|---|---|---|
| Osciladores 1-2 (nivel, transposición, afinación, frame), unísono hasta 4, ENV 1-2, LFO 1, filtro 1, glide, legato, macros 1-4 | Filtro ANALOG, DIRTY, LADDER, DIGITAL y DIODE a los nuestros; reverb, chorus, delay y drive a los envíos del canal | Oscilador 3 (se mezcla en el 2 si es estático), sampler de Vital, morph espectral, distorsión de fase, LFO 2-8, ENV 3-6, aleatorios, filtros FORMANT, COMB y PHASER, EQ, flanger, phaser |

El editor muestra lo que se perdió y un costo estimado de CPU antes de empaquetar.

**Las macros casi nunca vienen hechas:** solo 84 de los 356 presets usan alguna y 16 les ponen nombre. Si el preset no las trae, el editor arma cuatro: **TONE** (corte), **WAVE** (posición en la tabla), **MOVE** (cantidad del LFO) y **SPACE** (envío de efectos).

### 4.4 Presets con licencia libre en GitHub

| Repositorio | Licencia | Presets | ¿Se puede usar? |
|---|---|---:|---|
| [nahush2321/Vitalium-presets](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 | 221 | sí, con crédito |
| [Mxm40b/vital-presets](https://github.com/Mxm40b/vital-presets) | Unlicense | 125 | sí |
| [abstractionmage/Vital-Presets](https://github.com/abstractionmage/Vital-Presets) | CC0-1.0 | 8 | sí |
| [christ-offer/vitalium-presets](https://github.com/christ-offer/vitalium-presets) | CC0-1.0 | 3 | sí |
| [Miserlou/VitalPresets](https://github.com/Miserlou/VitalPresets) | sin licencia (y trae presets de otros packs) | 2124 | **no** |
| [jpriebe/qub1t-vital-presets](https://github.com/jpriebe/qub1t-vital-presets) | sin licencia | 195 | **no** |
| Presets de fábrica de Vital | licencia que prohíbe redistribuir | — | **no** |

Lo que muestran esos 356 presets (lo que el FM-1 tendrá que hacer para que suenen como en Vital):

- **Efectos en casi todos:** distorsión en 252, compresor en 221, EQ en 176, reverb en 152, chorus en 115, delay en 80. Sin reverb, chorus y drive un snapshot va a sonar seco.
- **Unísono:** la mitad usa 4 voces o más; hay hasta 16.
- **Filtros:** ANALOG en 300, COMB en 45, DIRTY en 32, DIGITAL en 17, LADDER y FORMANT en 12.
- 72 usan el sampler de Vital y 146 componentes son audio cortado en tablas.

**Lista inicial** (caben como están: sin sampler, solo `Wave`/`Line Source`, filtros que tenemos, unísono ≤ 6). Ninguno se escuchó todavía y varios nombres son malos: en el banco de fábrica se renombran (las tres licencias lo permiten) y llevan crédito.

| Máquina | Preset (autor, licencia) |
|---|---|
| BB-303 / B-MINI | Sub 1 (nahush, GPL-3.0) · Sub 56 (nahush, GPL-3.0) · Curious Bass (Abstraction Mage, CC0) · bad 808 (mxmfrpr, Unlicense) · low part of heavy bass (mxmfrpr, Unlicense) · reese bass (mxmfrpr, Unlicense) |
| BITTAL lead | Saw Movement 45 (nahush) · Synth8 (nahush) · Sirenengesang (Abstraction Mage) · cool synth · hard lead · nice lead (mxmfrpr) |
| BITTAL keys | PIANO · pinaon 12 · Chord 3 (nahush) · cute background keys · keys per P · smooth chill chords (mxmfrpr) |
| BITTAL pluck | move arp (nahush) · super space pluck · arp · bishu's better bell · wood pluck · spacy plucks (mxmfrpr) |
| BITTAL pad | Low-mid Simplicity (Abstraction Mage) — el único pad que cabe: los demás usan sampler o mucho unísono |

**El banco de fábrica es esta lista inicial** (decidido por el autor, 2026-10-14): se escucha, se renombra y se acredita. BUNO-6 no tiene ninguno en la lista, así que sus presets de fábrica son los propios hechos en Vital (sección 3.2). Cualquiera podrá subir los suyos desde el editor.

## 5. La cara visual

### 5.1 Paleta (pantalla; las luces de las teclas siguen blancas)

| Uso | Hex | RGB565 |
|---|---|---|
| Fondo (Void Black) | `#0A0B10` | `0x0842` |
| Rejilla sutil | `#161925` | `0x10C4` |
| Activo, ondas, cursor (Cyber Cyan) | `#00F0FF` | `0x079F` |
| Valores, filtro, paso que suena (Neon Amber) | `#FF8A00` | `0xFC40` |
| REC, saturación, mute (Laser Red) | `#FF0055` | `0xF80A` |
| Notas largas, TIE (Matrix Mint) | `#70FF98` | `0x77F3` |

### 5.2 Las franjas, con el teclado dentro

El prompt pone una barra de 16 pasos donde nosotros necesitamos la tira de teclas. Coinciden: **16 pasos = 16 blancas**. Arriba van las 11 negras, como en el aparato.

| Franja | Y | Contenido |
|---|---|---|
| Cabecera | 0-23 | máquina, PLAY/REC, TIE/GATE, BPM y CPU (≤ 32 caracteres) |
| Visor | 24-135 | lo propio de cada máquina (mezclador, rollo de notas, onda, algoritmo, tabla) |
| Teclado | 136-167 | negras (136-149, 11 celdas con su nombre) y blancas = pasos (150-167, 16 celdas de 15 px, con las barras de TIE) |
| Perillas | 168-239 | K1-K4: dial, valor y nombre, 60 px cada una |

El mezclador entra justo: una fila de títulos y **6 canales de 16 px** en los 112 px del visor (nombre, secuencia de 16 pasos, efecto, volumen).

### 5.3 Las perillas grandes: hoy y lo que propone el prompt

| | Hoy (0.5) | Prompt | Recomendación |
|---|---|---|---|
| **SELECT** | la página o la fila | HOME: cursor de canal · dentro: la página | **como el prompt** |
| **PRESETS** | el sonido · en HOME, el patrón | HOME: escena/patrón · dentro: el preset o el snapshot | igual que hoy (coinciden) |
| **ALGORITHM** | el canal, con el recuadro de 400 ms | HOME: BPM · dentro: el modo (algoritmo de BX-7, BB-303 ↔ B-MINI, tabla de BITTAL, chorus de BUNO-6) | **como el prompt**: el nombre de la perilla es justo eso |
| Entrar a un canal | girar ALGORITHM | no lo dice | en HOME, SEQ o EDIT abren el canal del cursor; **HOME + SELECT** cambia de canal desde cualquier pantalla |

**Decidido por el autor (2026-10-14): se adopta el mapa del prompt**, con la entrada a los canales de la última fila. Es el cambio más grande para quien ya toca la 0.5: el manual y la guía de HOME lo explican desde el primer arranque. Se va el recuadro de 400 ms de ALGORITHM (y su ajuste en GLO y SISTEMA).

## 6. Optimizaciones que pide el cambio

En el orden en que hacen falta, cada una medida en el FM-1 antes y después:

1. **Pantalla rápida:** SPI a 30 MHz y envío por DMA (2.2× medido). La interfaz nueva anima más (secuencias en el mezclador, la tabla girando, diales).
2. **Audio en los dos núcleos** (1.83× medido). BITTAL con unísono más las seis voces de BUNO-6 no caben en uno solo.
3. **El núcleo WAVE**: tablas en RAM (nunca leídas de la flash en el audio), int16, interpolación entre frames calculada por bloque, pasadas cortas como ANALOG. Límites: 4 voces por canal, unísono ≤ 4. Costo a medir; estimado de 5-9 % de un núcleo por voz con 2 osciladores.
4. **Efectos:** reverb de Griesinger y chorus (Clouds/Rings, MIT), drive y la mezcla de envíos por canal; además el chorus de BBD de BUNO-6, propio del canal. Sin ellos los snapshots no se parecen a Vital ni BUNO-6 a un Juno.
5. **RAM:** una tabla armada ocupa ~32 KiB (8 frames de 1024 muestras con sus octavas); dos osciladores en BITTAL, ~64 KiB. BB-303, B-MINI y BUNO-6 no usan tablas. Hay ~370 KiB libres: sobra, y hay que medirlo.
6. **Programa:** hoy 340 KB de 568 KiB. ANALOG y ACID se van; entran WAVE, los efectos y la interfaz.

## 7. Memoria: una sola toma por ahora (hecho)

A pedido del autor, la zona de tomas queda del tamaño de la toma de prueba: **un compás de *Buenos Aires No Duerme* a 161 BPM** (1.49 s, 23 851 muestras a 16 kHz), sacado de la pista instrumental del proyecto del autor. El resto queda reservado para el banco de snapshots.

Hay cuatro candidatos de un compás, cada uno empezando en un golpe, en `assets/test-samples/` (fuera de git: el audio no se sube): `buenos_aires_a_75s`, `_b_62s`, `_c_123s` y `_d_198s`, en `_hq.wav` (44.1 kHz) y `_fm1.wav` (16 kHz y 8 bits, como suena en el FM-1). **Para esta versión se usa el primero, `buenos_aires_a_75s`** (decisión del autor, 2026-10-14): empieza en el segundo 74.86 de la pista, en la parte más fuerte y con más golpes. Los otros tres quedan de repuesto. Se sube con `py -3 tools/fm1/toma.py assets/test-samples/buenos_aires_a_75s_hq.wav --bpm 161`, o en el emulador con WAV a TAKES.

| Zona | Antes | Ahora |
|---|---|---|
| Tomas | `0xC0000-0xD8FFF`, 100 KiB, 6.4 s, hasta 3 s por toma | `0xC0000-0xC5FFF`, **24 KiB, una toma de hasta 1.53 s** |
| Banco de sonidos | — | `0xC6000-0xD8FFF`, **76 KiB reservados** (~16 snapshots de una tabla; más si comparten tablas) |

`BM_SMP_REC_MAX` (lo que graba la grabadora y lo que acepta `tools/fm1/toma.py`) baja a 24 448 muestras. Las tomas guardadas fuera de los primeros 24 KiB se pierden al actualizar.

### 7.1 Después: tomas y sonidos comparten la zona (decidido)

Cuando exista el banco (fase 5), **las tomas y los snapshots comparten los 100 KiB** (`0xC0000-0xD8FFF`, 25 sectores) y el usuario decide cómo se reparten:

- Un solo mapa de sectores para las dos bibliotecas: **las tomas se ubican desde abajo y los snapshots desde arriba**, cada una en sectores seguidos con su cabecera, así que el límite se mueve solo y no hay que mover datos al cambiarlo.
- La pantalla de memoria muestra las dos partes (segundos de toma libres y sonidos que caben); al grabar o al cargar un sonido, si no cabe, ofrece borrar algo de la otra parte (pregunta, como todo borrado).
- El banco de fábrica se escribe al instalar y ocupa lo suyo (la lista inicial: ~25 snapshots, entre ~150 B y ~8 KiB cada uno); lo que sobre es para tomas.
- La toma máxima sigue limitada por la RAM de grabación (3 s), no por la zona.

Hasta la fase 5 queda como hoy: una toma de 1.5 s y el resto reservado.

## 8. Plan por fases

Cada fase sale cuando se probó en el FM-1. La beta que "ya integra todo" es el final de la fase 7.

| Fase | Qué | Se prueba con |
|---|---|---|
| 0 ✓ | Zona de tomas de 24 KiB y banco reservado | pruebas de PC |
| 1 | Pantalla rápida y audio en dos núcleos | consola: µs de la UI, carga por núcleo |
| 2 | **Cara visual:** paleta, franjas, componentes (cabecera, teclado, diales) y el mezclador BX-1 en HOME, sin tocar el sonido | capturas y el aparato |
| 3 | **Canales fijos:** fuera MOTOR, KIT y MIDI como motor; formato nuevo | demo nueva |
| 4 | **Núcleo WAVE** con BB-303 y B-MINI (reemplaza a ANALOG y ACID) | mismo sonido o mejor, costo por voz |
| 5 | **Editor web de snapshots** (`.vital` → `.bsnap`), banco de fábrica, subir por USB, y la zona compartida con las tomas (7.1) | presets de la lista inicial y los patches de BUNO-6 hechos en Vital |
| 6 | **BITTAL** (T5) con snapshots, macros y LFO, y **BUNO-6** (T6) con su chorus | costo con 4 voces y unísono; las 6 voces de BUNO-6 |
| 7 | **Efectos:** envíos por canal, reverb, chorus, drive, Beat FX | la demo con efectos, sin bloques tarde |
| 8 | **REC expresivo:** TIE con paso inicial + final, gate fino, corte al final del compás, grabar las perillas mientras se sostiene la nota | grabar una línea larga |
| 9 | **BX-7:** dibujo de los 32 algoritmos, edición y bancos `.syx` | un banco de DX7 |
| 10 | **BP-1:** puntos de loop (sustain loop) y la vista de onda grande | notas largas desde REC |

## 9. Decisiones (todas tomadas el 2026-10-14)

1. ~~T6~~: **BUNO-6**, un polifónico a lo Juno con presets propios hechos en Vital (decidido por el autor, 2026-10-14).
2. ~~Perillas grandes~~: se adopta el mapa del prompt (SELECT = canal en HOME, ALGORITHM = BPM / modo; sección 5.3) (decidido por el autor, 2026-10-14).
3. ~~Nombres~~: BX-1, BR-8, BP-1, BB-303 / B-MINI, BX-7, **BITTAL**, BUNO-6 (decidido por el autor, 2026-10-14).
4. ~~Tablas de BITTAL~~: espectro de hasta 256 armónicos, armado en RAM al cargar (sección 4.2) (decidido por el autor, 2026-10-14).
5. ~~Banco de fábrica~~: la lista inicial (4.4); BUNO-6 con presets propios hechos en Vital (decidido por el autor, 2026-10-14).
6. ~~La toma de prueba~~: `buenos_aires_a_75s`, el primer corte (decidido por el autor, 2026-10-14).
7. ~~Tomas~~: comparten los 100 KiB con el banco y el usuario elige el reparto (7.1) (decidido por el autor, 2026-10-14).

## Fuentes

- Código de Vital: <https://github.com/mtytel/vital> (README: licencia, nombre y presets de fábrica; `src/common/synth_constants.h`: modelos de filtro)
- Presets analizados: los cuatro repositorios con licencia de la sección 4.4 (análisis propio con un script sobre el JSON)
- Mediciones del FM-1: [AUDIO](AUDIO.md), [MOTORES](MOTORES.md), [ARQUITECTURA_FM1](ARQUITECTURA_FM1.md), [TRAZA_FM1](TRAZA_FM1.md)
