# Bubba Machine 3.0: diseño de la interfaz

> **Estado: diseño, sin implementar.** La versión actual (0.2) funciona como se describe en el [manual](MANUAL.md). Este documento es la dirección para la 0.3.

## La idea

**Cuatro gestos por botón, y significan lo mismo en todos. HOME en todas partes.**

Las perillas grandes te llevan, las pequeñas ajustan y las teclas controlan. Se recupera lo mejor de 0.2 (tres pulsaciones, capas en vivo, anillo de 2 s) y se suma lo nuevo: perillas que navegan, HOME como controlador, pestañas y canales fijos y libres.

| Gesto | Cuándo | Qué hace |
|---|---|---|
| **Toque** | menos de 300 ms | Abre su página en el canal donde estás. Otra vez: la siguiente página del mismo botón |
| **Mantener** | 300 ms o más | Su **capa en vivo**: mientras sostienes, las teclas hacen esa función sobre lo que suena. La pantalla dibuja el teclado con lo que hace cada tecla. Al soltar, vuelves |
| **Mantener 2 s** | 2 s sin tocar otra cosa | Su configuración o su acción peligrosa. Un anillo se llena arriba; si tocas otra cosa, se cancela. Solo en HOME, SEQ, EDIT, SAVE y REC |
| **HOME + botón** | con HOME sostenido | La **versión global** del botón: para todo el proyecto, no solo el canal |

## Las perillas

| Perilla | Qué hace |
|---|---|
| **ALGORITHM** | El canal. En HOME, al girar aparece un recuadro con el canal destino y a los 400 ms entras (configurable: 0, 250, 400, 800 ms u OFF). Dentro de un canal cambia al instante y conserva la página. En una lista, ajusta el valor marcado. Con HOME sostenido, elige un canal desde cualquier pantalla y entras al soltar HOME |
| **SELECT** | La página o la fila. Con un paso sostenido, su acento |
| **PRESETS** | El sonido del canal. En HOME, el banco de patrones |
| **K1-K4** | Lo que dice la leyenda de abajo, siempre en el mismo color. Con HOME sostenido: BPM, swing, largo maestro y drive |

**Prioridad de ALGORITHM:** paso sostenido > lista > canal > HOME. El pie de pantalla dice qué hace en cada momento.

## Los 14 botones

| Botón | Toque | Otra vez | Mantener (capa) | 2 s | HOME + |
|---|---|---|---|---|---|
| HOME | HOME: los 6 canales | MEZCLA › ESCENAS › CANCIÓN | Controlador: blancas = patrón, negras = mute, K1-K4 = tempo y globales | SISTEMA | (es el modificador) |
| SEQ | Pasos del canal | Synth: LOOP · sampler: RETRIG | Largo y velocidad: blanca = largo, negras = velocidad | TIEMPO de las 6 pistas | CANCIÓN |
| EDIT | SONIDO | Batería: INSTR | BORRAR: borra al pasar; negras copiar, pegar, limpiar (sampler: 4 = capturar) | Sampler: slots · otros: sonido inicial | Copiar el canal a otro |
| ENV | ENV (notas) · INSTR (batería) | ENV página 2 | ACENTO: pintar acentos | — | — |
| LFO | LFO del canal (futuro) | — | REPETIR (batería, sampler) · LIGAR (notas) | — | — |
| FX | MEZCLA del canal | — | PUNCH-IN: 16 efectos; negras 1-6 siguen muteando | — | Delay y sidechain globales |
| SCL | ESCALA del canal | ACORDES | Acordes en vivo | — | Escala de todo el proyecto |
| ARP | ARP del canal | — | Arpegio en vivo · batería: ROLL | — | — |
| SAVE | Menú de guardar | Ejecuta | COPIAR PATRÓN: blanca = pegar ahí | Guardado rápido | Guardar como |
| GLO | Lista global | Cierra | TONO: el teclado transpone todo; PLAY = TAP | — | — |
| REC | Grabar | Loopera: sobregrabar › play | Grabar solo mientras sostienes | Borrar la pista (pregunta) | SONG REC |
| PLAY | Play / stop | — | — | — | Volver al paso 1 sin parar |
| OCT − / + | Octava · página · en preguntas OCT+ = sí | — | — | — | Deshacer / rehacer |

Una celda vacía es a propósito: ese gesto no hace nada en ese botón, para que no se dispare sin querer.

## HOME en todas partes

El FM-1 no tiene SHIFT, así que HOME hace tres trabajos:

1. **Pantalla de inicio:** los 6 canales con su secuencia y el banco de patrones.
2. **Controlador que te sigue:** mantener HOME trae el controlador a cualquier pantalla. Editas el bajo y, sin salir, muteas o pones el siguiente patrón en cola.
3. **Modificador:** HOME + botón = su versión global.

Si se usó HOME como modificador, soltarlo **no** te lleva a la pantalla de inicio: HOME solo navega con un toque limpio.

**Teclas en HOME:** negras 1-6 mutean T1-T6 (mantener = solo), 7-10 lanzan las escenas A-D y 11 es FILL. Las blancas ponen en cola el patrón del banco; dos blancas encadenan del primero al segundo.

## Canales: 3 fijos y 3 libres

| Canal | Tipo | Qué es |
|---|---|---|
| T1 BATERÍA | Fijo | Rejilla TR de 16 instrumentos |
| T2 SAMPLER | Fijo | Loop en 8 slices que un generador rearma |
| T3 BAJO | Fijo | Wavetable monofónico con glide |
| T4-T6 | Libres | SYNTH, MONO, KIT o MIDI, elegido en la pestaña MAS |

Cada canal muestra sus pestañas arriba: SELECT las recorre y los botones saltan directo. La última, **MAS**, es la lista con todo el canal.

**Las negras siguen una sola gramática:** 1-8 son los elementos del canal, 9 y 10 lo modifican, y **11 rompe el patrón** (FILL en las baterías, STUTTER en el sampler).

## Sampler 3.0

La idea viene del BREAK de X0X (un loop en slices que un generador reordena) con una interfaz que deja ver lo que pasa:

- **Dos filas de pasos:** TU (lo que programaste: hueco = el generador elige, lleno = slice fijo) y **SUENA** (el slice que eligió el generador en este compás).
- **Perillas con nombres claros:** VARIAR, ANCLAR, REPETIR y ROMPER.
- **CAPTURAR:** mantener EDIT + negra 4 deja fija la variación que está sonando.
- **Fijar slices tocando:** con REC, las negras 1-8 se escriben como pasos fijos.
- **El tono a la vista:** BPM original del loop y cuántos semitonos se movió.
- **Dibujo por frecuencias:** graves, medios y agudos en carriles separados (ya funciona en 0.2).

## Grabadora

Grabar, guardar y cortar son tres pantallas, una tarea cada una:

1. **GRABAR:** un número grande con los segundos que quedan, que es el menor entre la RAM de grabación (3 s) y la memoria libre de tomas. Rayas marcan cuántos compases caben.
2. **TOMAS:** biblioteca de grabaciones con su BPM y sus compases. Los loops de fábrica no gastan memoria.
3. **CORTAR:** después, con calma. Las blancas ponen cortes por corchea, la negra 9 los pone en los golpes y la 10 los reparte iguales.

La toma queda sin guardar hasta que paras el transporte, porque escribir la flash detiene el audio.

## Decisiones pendientes

- **Tiempo del recuadro de entrada:** 400 ms propuesto, a probar.
- **Bancos de patrones:** la flash guarda 16 patrones; más bancos piden ampliar el mapa de flash o achicar los patrones.
- **Biblioteca de tomas:** hoy son 2 slots fijos de 4 s; una biblioteca de largo libre necesita cambiar el formato de la flash.
- **Fantasma en la batería:** el byte de cada paso está lleno; hace falta un P-Lock de nivel o ampliar el formato.
