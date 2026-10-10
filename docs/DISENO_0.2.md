# Bubba Machine 0.2 — Diseño

Groovebox de directo: **batería TR**, **sampler con chops que se secuencian como la batería**, bajo y 3 synths. Cada pista tiene **su largo y su velocidad**. Sus reglas: cada botón abre su pantalla, mantener = capa en vivo, colores fijos por perilla, y suma una **tercera pulsación de 2 segundos** para pantallas especiales.

El hardware y las reglas técnicas (memoria, flash, audio) siguen siendo los de `SPEC_V3.md`.

---

## 1. Tres pulsaciones por botón

| Pulsación | Qué hace | Cuándo se dispara |
|---|---|---|
| **Toque** (< 300 ms) | Abre la pantalla del botón; otra vez, su página siguiente | Al soltar |
| **Mantener** (≥ 300 ms) | Abre su **capa en vivo** mientras lo sostienes | Al cumplir 300 ms |
| **Mantener 2 s** | **Pantalla especial** o acción que conviene no hacer sin querer | Al cumplir 2 s, **solo si no tocaste nada más** |

Mientras sostienes, un **anillo** en la barra de estado se llena hasta los 2 s. Si tocas una tecla o giras una perilla, la capa sigue abierta y los 2 s se cancelan: tocar mutes durante un minuto nunca abre otra pantalla por accidente.

## 2. Mapa de botones

| Botón | Toque | Otra vez | Mantener (capa) | Mantener 2 s |
|---|---|---|---|---|
| **HOME** | Pistas + mezclador | **ESCENAS** | MUTE: blancas 1-6 mute, 9-14 solo | **SISTEMA** (reloj, sync, MIDI, LEDs, memoria) |
| **SEQ** | Rejilla (TR o pasos) | Synth: **LOOP** | PATRONES (blanca = patrón en cola) | **TIEMPO**: largo y velocidad de las 6 pistas |
| **EDIT** | Sonido (KIT / SAMPLE / onda) | Instrumento (TR) | BORRAR (estilo TR) + herramientas | T2: **SAMPLER** · otras: INIT del sonido |
| **ENV** | Envolvente | Página 2 | — | — |
| **LFO** | Reservado | — | — | — |
| **FX** | Delay y sidechain | — | PUNCH-IN (16 efectos) | — |
| **SCL** | Escala (teclado) | **ACORDES** (cuadrícula) | Cuadrícula de acordes en vivo | — |
| **ARP** | Arpegiador | — | Synth: arpegio · TR: **ROLL** | — |
| **SAVE** | Menú de guardado | Ejecuta | — | **Guardado rápido** |
| **GLO** | Global: BPM, swing, largo maestro, drive, fills | — | Modificador (GLO + algo) | — |
| **REC** | Grabar en vivo / loopera / sampler | — | — | **Borrar la pista** (con undo) |
| **PLAY** | Play/stop al presionar | — | — | — |
| **OCT − / +** | Octava · en SEQ: página | — | — | — |

## 3. Tiempo por pista (polímetro)

- Cada pista: **largo** 1-64 pasos y **velocidad** ×2, ×3/2 (tresillo), ×1, ×3/4, ×1/2, ×1/4.
- En batería y sampler cada **instrumento** además tiene su largo: un hi-hat de 12 contra un bombo de 16.
- **Largo maestro** (GLO): 16, 32, 48, 64, 128 o ∞ pasos. Al cumplirse, todas las pistas vuelven al paso 1 y ahí cambian patrones y escenas. En ∞ las pistas corren libres y los cambios caen en el siguiente compás.
- Pantalla **TIEMPO** (SEQ 2 s): las 6 pistas como barras de su largo, con el cabezal de cada una, para ver el polímetro de un vistazo. SELECT elige la pista, K1 largo, K2 velocidad, K3 largo del instrumento (TR).

## 4. Batería TR (T1) y sampler (T2)

Cada una tiene **16 instrumentos**, cada uno con su línea de 64 pasos.

**En SEQ:**

| Control | Hace |
|---|---|
| Blancas | Los 16 pasos del instrumento elegido (página actual) |
| Negras 1-8 | Eligen y tocan un instrumento de la sección |
| Negra 9 | Cambia de sección (1-8 / 9-16) |
| Negra 10 | FILL mientras la sostienes |
| Negra 11 | Toque: mute del instrumento · mantener: pantalla de mutes |
| OCT − / + | Página |
| SELECT | Instrumento |

**Paso mantenido + perilla:** K1-K4 P-Lock (TONE · TUNE · DECAY · START) · SELECT acento (normal / acento) · ALGORITHM repetición (×1 ×2 ×3 ×4 FLAM) · PRESETS condición (SIEMPRE / SOLO FILL / SIN FILL).

**La pantalla** es una matriz de 16 instrumentos × 16 pasos, con el instrumento elegido resaltado, el largo de cada línea y el cabezal de cada una (pueden ir desfasados).

**EDIT, página 2 (INSTRUMENTO):** K1 NIVEL · K2 AFINACIÓN · K3 DECAY · K4 LARGO de la línea.

**Capa BORRAR (EDIT mantenido, estilo TR):** sonando, mantener una blanca borra el instrumento en cada paso por donde pasa el cabezal; parado, lo borra entero. Negras 1-3: COPIAR, PEGAR, BORRAR pista. OCT − / +: deshacer / rehacer.

**ROLL (ARP mantenido + tecla):** repite el instrumento a 1/8, 1/16, 1/32 o tresillos (K1). Con REC, se graba como pasos con repetición.

**Fills:** pasos con condición SOLO FILL / SIN FILL. El fill suena mientras sostienes la negra 10, o solo cada 2, 4 u 8 compases (GLO).

## 5. Sampler (T2)

- **2 slots** (A y B) de hasta **4 s** a 16 kHz, guardados en flash. La pista T2 elige el slot (GLO + ALGORITHM).
- **Grabar:** EDIT en T2 → mantener REC. Graba la mezcla (resample) hasta 3 s. Mientras grabas, cada blanca marca un chop. El delay y el punch-in se pausan mientras haya un sample sin guardar (comparten la memoria).
- **Guardar:** al parar, el sample se guarda solo en su slot (no se escribe flash sonando).
- **SAMPLER (EDIT 2 s en T2):** slot, cortar en 4, 8 o 16 partes iguales o dejar los chops manuales, y borrar el slot.
- Los 16 chops son los 16 instrumentos de la rejilla TR de T2: **se secuencian igual que la batería** y se guardan con el patrón.

## 6. Synths (T3 bajo, T4-T6)

- SEQ: pasos con notas y acordes (4 eventos por paso), como en 0.1.
- **LOOP (SEQ otra vez):** loopera de notas estilo RC, sin cuantizar. K1 ON/OFF · K2 COMPASES 1/2/4. REC: graba (desde el siguiente compás) → al cerrar el loop pasa a PLAY → REC: sobregraba → REC: PLAY. REC 2 s borra el loop. Se guarda con el patrón.

## 7. Acordes: cuadrícula estilo Maschine

**SCL otra vez** abre la cuadrícula. **SCL mantenido** la activa en las teclas desde cualquier pantalla.

- Las **16 blancas son 16 acordes**, dibujados como una cuadrícula de 4×4 (fila 1 = blancas 1-4…).
- Cada pad muestra el **grado** (I, ii, V7…) y el **nombre** (Am, G7). Se colorea según su **función**: azul = reposo (I, iii, vi), verde = preparación (ii, IV), naranja = tensión (V, vii). Así la pantalla enseña qué va con qué.
- **Juegos de acordes** (K1):
  - **DIATÓNICO:** fila 1-2 tríadas I-VII + octava, fila 3-4 con séptima.
  - **PROGRESIONES:** cada fila es una progresión clásica que se toca de izquierda a derecha (I-V-vi-IV · vi-IV-I-V · ii-V-I-vi · I-vi-IV-V).
  - **NEO SOUL:** acordes de 7ª y 9ª.
- K2 VOICING (cerrado / abierto / conducción de voces: cada acorde en la inversión más cercana al anterior) · K3 OCTAVA · K4 STRUM.
- Negras 1-4 modifican mientras las sostienes: +7, +9, SUS2, SUS4.

## 8. Escenas (HOME otra vez)

- Una escena = **patrón + mutes** (pistas e instrumentos). Hay 4: A-D.
- Blancas 1-4 lanzan en el siguiente ciclo maestro (parado: al instante). Blancas 5-8 guardan lo que suena.
- Mantener una de 1-4 y tocar otras = **cadena** en bucle.
- **REC = SONG REC:** cada escena que lances queda escrita en la canción.

## 9. Memoria

| | RAM | Flash |
|---|---|---|
| Patrón (6 pistas, TR en bytes por paso) | ~12.7 KiB | ≤ 8 KiB disperso |
| Sample en grabación | 48 KiB compartidos con el delay y el punch-in | 2 slots × 64 KiB |
| Patrones | — | 20 slots de 8 KiB: 16 patrones + song/escenas + 3 libres para el wear-leveling |
