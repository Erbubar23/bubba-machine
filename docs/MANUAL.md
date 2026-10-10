# Bubba Machine — Manual

Bubba Machine convierte el FM-1 en una groovebox de 6 pistas para tocar en vivo. Cada botón abre la pantalla de su nombre; si lo **mantienes**, activa su modo en vivo sobre las teclas, y al soltarlo vuelves a donde estabas.

## La pantalla

```
┌────────────────────────────────────┐
│ ▶ A01 2.3  [T1] 112 BPM      ▭     │  estado: siempre igual
├────────────────────────────────────┤
│                                    │
│   lo que hace el modo actual       │  contexto: cambia con cada botón
│   (los valores que giras aparecen  │
│    aquí encima y se van solos)     │
├────────┬────────┬────────┬─────────┤
│ KNOB 1 │ KNOB 2 │ KNOB 3 │ KNOB 4  │  qué hace cada perilla ahora,
└────────┴────────┴────────┴─────────┘  en su color y en su orden físico
```

- **Estado**: transporte, patrón (y el siguiente en cola), compás.tiempo, pista, BPM, reloj externo, song, batería.
- **Borde rojo**: la mezcla saturó (baja niveles o el DRIVE).

## Las pistas

| Pista | Motor | Teclas fuera de SEQ |
|---|---|---|
| **T1 DRUMS** | ROMpler: kit KO-808 o KO-909 (16 sonidos). El hi-hat cerrado corta al abierto. | Blancas 1-16 = sonidos |
| **T2 SAMPLER** | Graba la mezcla y la corta en 16 partes | Blancas 1-16 = partes |
| **T3 BASS** | Wavetable mono con filtro y glide | Cromático (una octava abajo) |
| **T4-T6 SYN A/B/C** | Wavetable polifónico (pool de 7 voces) con filtro | Cromático, o en escala |

Cambia de pista con **ALGORITHM**. En SEQ también con las negras 1-6.

## Los botones

| Botón | Toque | Mantener | GLO + botón |
|---|---|---|---|
| **PLAY** | Play/stop (al presionar) | — | Reinicia desde el paso 1 |
| **REC** | Grabación en vivo de notas | En EDIT de T2: graba el sampler mientras lo mantienes | — |
| **SEQ** | Pasos | **Patrones**: blanca = elegir (si suena, entra al final) | **Song** |
| **HOME** | Las 6 pistas + mezclador | **Mute**: blancas 1-6 mute, 9-14 solo | — |
| **EDIT** | El sonido de la pista | — | — |
| **ENV** | Envolvente (otra vez: página 2) | — | — |
| **LFO** | Reservado | — | — |
| **FX** | Delay y sidechain | **Punch-in**: cada blanca es un efecto mientras la sostienes | — |
| **SCL** | Escala y acordes | **Acordes en vivo** | — |
| **ARP** | Arpegiador | **Arpegio en vivo** con las notas que sostengas | — |
| **SAVE** | Menú de guardado (SAVE otra vez = ejecutar) | 1 s = guardado rápido | — |
| **GLO** | Ajustes globales | Es el modificador | — |
| **OCT − / +** | Octava · en SEQ: página | — | Transponer la pista ± 1 semitono |

Si mantienes un botón y tocas otra cosa antes de los 300 ms, su modo en vivo se activa al instante.

## Las perillas

| Perilla | Siempre | Con GLO |
|---|---|---|
| **KNOB 1-4** | Lo que dice la leyenda de abajo | BPM · SWING · LARGO · DRIVE |
| **SELECT** | Mover en listas · en SEQ/EDIT de T1/T2: sonido o parte a secuenciar | — |
| **ALGORITHM** | Pista | Kit (T1), interpolación (T2) o banco de ondas (T3-T6) |
| **PRESETS** | Patrón A01-A16 | — |
| **MASTER** | Volumen final (no se guarda) | — |

Macros de las pistas (KNOB 1-4 en SEQ y EDIT): synths **CUTOFF · RESO · DECAY · WAVE**; batería y sampler **TONE · TUNE · DECAY · START**.

## Secuenciar (SEQ)

- Las **16 blancas son los pasos** de la página actual. **OCT − / +** cambia de página. Los LEDs muestran los pasos: encendido = nota, tenue = vacío, parpadeo = paso que suena, respiración = paso con P-Lock.
- **Toque** en un paso vacío = crea una nota. **Toque** en un paso con nota = la borra.
- En T1 y T2, el paso usa el sonido elegido con **SELECT** (también puedes tocarlo en otra pantalla para elegirlo).
- **Mantén un paso** y gira:
  - **KNOB 1-4**: P-Lock (ese valor solo en ese paso; la marca blanca muestra el valor de la pista).
  - **SELECT**: la nota (o el sonido).
  - **ALGORITHM**: probabilidad (de 5 en 5 %).
  - **PRESETS**: largo de 1 a 16 pasos, o **TIE** (se liga con la siguiente: glide en el bajo).
- Negras: **1-6** pista (GLO + negra = mute) · **7** COPY · **8** PASTE · **9** CLEAR · **10** FILL (mientras la sostienes, todo suena aunque tenga probabilidad) · **11** UNDO.
- Con un paso mantenido, COPY/PASTE/CLEAR actúan sobre el paso; si no, sobre la pista entera.
- **REC** + tocar en otra pantalla graba en vivo, cuantizado al paso más cercano.

## Escala y acordes (SCL)

- KNOB 1 tónica · KNOB 2 escala (9 escalas) · KNOB 3 acorde base · KNOB 4 **LOCK** (el teclado queda en escala en todas las pantallas).
- Blancas: notas de la escala. Negras 1-7: los acordes de la escala. Negras 8-11 (+7, +9, SUS2, SUS4) modifican el acorde mientras las sostienes.
- La pantalla dibuja el teclado real con el nombre de cada nota y acorde.

## Sampler (T2)

1. Elige T2 y pulsa **EDIT**.
2. **Mantén REC**: graba lo que está sonando (hasta 1.1 s).
3. Mientras grabas, cada **blanca** marca un corte (hasta 16). Sin cortes, se divide en 16 partes iguales.
4. Suelta REC. Toca las blancas para disparar las partes o secuéncialas en SEQ.

El sample es **volátil**: se pierde al apagar.

## Efectos

- **FX** (toque): tiempo del delay (1/16 a 1/4), feedback, mezcla y release del sidechain. El envío al delay y el ducking de cada pista están en HOME (KNOB 3 y 4).
- **FX** (mantener) + blanca: punch-in, cuantizado a 1/16: STUTTER 1/4 · 1/8 · 1/16 · 1/32 · REVERSE 1/8 · 1/4 · TAPE STOP · GATE · LPF SWEEP · HPF SWEEP · BITCRUSH · SR DOWN · DELAY THROW · ECHO FREEZE · KILL LOW · KILL HIGH.
- **Sidechain**: cada bombo de T1 baja el volumen de T3-T6, según el DUCK de cada pista.
- **DRIVE** (GLO + KNOB 4): saturación suave de la mezcla.

## Patrones y song

- **PRESETS** o **SEQ mantenido + blanca**: elige patrón. Si está sonando, entra al final del patrón.
- Los cambios no guardados se conservan al cambiar de patrón sonando. Se guardan solos al parar (autoguardado tras 10 s sin tocar nada).
- **GLO + SEQ**: song. Blanca = agrega un patrón · SELECT = elegir fila · KNOB 1 patrón · KNOB 2 repeticiones · KNOB 4 song ON/OFF · negra 9 = borrar fila.

## Guardar

- La flash **solo se escribe en STOP** (escribir detiene el audio unos instantes).
- **SAVE**: guardar · guardar como (KNOB 1 elige destino) · guardar song · borrar patrón (SAVE dos veces) · contar eventos (máximo 768 por patrón).
- **SAVE mantenido 1 s**: guardado rápido.

## Ajustes (GLO)

Reloj interno/externo · salida Split Sync (L = pulso, R = mezcla mono) · PPQN del pulso (1, 2 o 4) · latencia del pulso · drive · autoguardado · brillo de LEDs en reposo · fuente del sampler · memoria usada · versión.

## MIDI

- Entrada (USB y TRS): canales 1-6 = pistas 1-6. Con reloj externo, sigue el MIDI Clock y Start/Stop.
- Salida: MIDI Clock con reloj interno.
