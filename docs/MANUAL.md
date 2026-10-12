# Bubba Machine 0.3 — Manual

Bubba Machine convierte el FM-1 en una groovebox de directo de 6 canales: **3 fijos** (batería, sampler y bajo) y **3 libres** (ANALOG, FM, ACID, KIT o MIDI). Cada canal tiene su propio largo y su propia velocidad.

La interfaz 3.0 se resume en una frase: **cuatro gestos por botón, y significan lo mismo en todos. HOME en todas partes.**

> La 0.3 cambió el formato de los patrones, los ajustes y los samples. **No lee lo que guardó la 0.2**: al arrancar por primera vez borra esos datos y empieza con la demo.

## Cuatro gestos por botón

| Gesto | Cuándo | Qué hace |
|---|---|---|
| **Toque** | menos de 300 ms | Abre su página en el canal donde estás. Otra vez: la siguiente página del mismo botón |
| **Mantener** | 300 ms o más | Su **capa en vivo**: mientras sostienes, las teclas hacen esa función y la pantalla dibuja el teclado con lo que hace cada tecla. Al soltar, vuelves |
| **Mantener 2 s** | 2 s sin tocar otra cosa | Su configuración o su acción peligrosa. Un anillo se llena arriba; si tocas otra cosa, se cancela. Solo en HOME, SEQ, EDIT, SAVE y REC |
| **HOME + botón** | con HOME sostenido | La **versión global** del botón: para todo el proyecto, no solo el canal |

Nunca hay dos capas a la vez: con una capa abierta, otro botón sostenido no abre la suya.

**Preguntas.** Lo que borra algo pregunta antes: **OCT+ = sí**, **HOME = no** (cualquier otro botón también cancela).

## Las perillas

| Perilla | Qué hace |
|---|---|
| **ALGORITHM** | El canal. En HOME aparece un **recuadro** con el canal destino y a los 400 ms entras. Dentro de un canal también muestra el recuadro y entra conservando la página; en **INSTR** recorre los 16 instrumentos (el número grande, su nombre y modelo) y al entrar lo hace sonar. En una lista, cambia el valor marcado. Con **HOME sostenido**, elige un canal desde cualquier pantalla y entras al soltar HOME |
| **SELECT** | La página (las pestañas de arriba) o la fila de una lista. Con un paso sostenido, su acento |
| **PRESETS** | El sonido del canal (kit, loop A o preset del motor). En HOME, el patrón |
| **K1-K4** | Lo que dice la leyenda de abajo, siempre en el mismo color. Con HOME sostenido: BPM, swing, largo maestro y drive |

**Prioridad de ALGORITHM:** paso sostenido > lista > canal > HOME. El tiempo del recuadro se cambia en GLO o SISTEMA (ENTRADA AUTO: 0, 250, 400, 800 ms u OFF).

## La pantalla

- **Arriba:** transporte, patrón (y el que sigue en cola), compás y tiempo, el canal, BPM. También la escena (A-D), **F** cuando suena el fill, el tono del proyecto si está transpuesto, el anillo de los 2 s y la batería.
- **Dentro de un canal:** sus **pestañas** debajo del estado. La activa está subrayada con el color del canal.
- **Abajo:** qué hace cada perilla K1-K4, en su color y en su orden físico.
- **Borde rojo:** la mezcla saturó.

## Los 14 botones

| Botón | Toque | Otra vez | Mantener (capa) | 2 s | HOME + |
|---|---|---|---|---|---|
| **HOME** | HOME: los 6 canales y el banco | MEZCLA › ESCENAS › CANCIÓN | **Controlador** (ver abajo) | SISTEMA | (es el modificador) |
| **SEQ** | Pasos del canal | Notas: LOOP · sampler: RETRIG | **Largo y velocidad** | TIEMPO de los 6 canales | CANCIÓN |
| **EDIT** | SONIDO | Batería: INSTR · TOMAS: CORTAR | **Borrar** | Sampler: TOMAS · otros: sonido inicial | Copiar el canal a otro |
| **ENV** | ENV (notas) · INSTR (batería) | ENV página 2 | **Acento** | — | — |
| **LFO** | LFO (próxima versión) | — | **Repetir** (rejillas) · **ligar** (notas) | — | — |
| **FX** | MEZCLA del canal | — | **Punch-in** | — | Delay y sidechain globales |
| **SCL** | ESCALA del canal | ACORDES | **Acordes** en vivo | — | Escala del proyecto |
| **ARP** | ARP del canal | — | **Arpegio** (notas) · **roll** (rejillas) | — | — |
| **SAVE** | Menú de guardar | Ejecuta | **Copiar patrón** | Guardado rápido | Guardar como |
| **GLO** | Lista global | Cierra | **Tono** y TAP tempo | — | — |
| **REC** | Grabar (en el sampler: GRABADORA) | Loopera: sobregrabar › play | Grabar solo mientras lo sostienes | Borrar el canal (pregunta) | SONG REC |
| **PLAY** | Play / stop | — | — | — | Volver al paso 1 sin parar |
| **OCT − / +** | Octava · página de pasos · en preguntas OCT+ = sí | — | — | — | Deshacer / rehacer |

## HOME en todas partes

El FM-1 no tiene SHIFT, así que HOME hace tres trabajos:

1. **Pantalla de inicio:** los 6 canales con su secuencia y el banco de 16 patrones. Otra vez: MEZCLA (los 6 niveles; ALGORITHM elige canal y K1-K4 lo mezclan), ESCENAS y CANCIÓN.
2. **Controlador que te sigue:** mantener HOME trae el controlador a cualquier pantalla. Editas el bajo y, sin salir, muteas o pones el siguiente patrón en cola.
3. **Modificador:** HOME + botón = su versión global (tabla de arriba).

Si usaste HOME como modificador, soltarlo **no** te lleva a la pantalla de inicio: HOME solo navega con un toque limpio.

**Teclas con HOME (en la pantalla HOME, en MEZCLA o sosteniendo HOME):**

| Tecla | Qué hace |
|---|---|
| **Blancas 1-16** | Ponen en cola ese patrón (parado: cambia al instante). Mantén una y toca otra: **cadena** del primero al segundo |
| **Negras 1-6** | Toque: mute de T1-T6. Mantener: **solo** mientras la sostienes |
| **Negras 7-10** | Lanzan las escenas A-D |
| **Negra 11** | FILL mientras la sostienes |

## Canales: 3 fijos y 3 libres

| Canal | Tipo | Qué es | Pestañas |
|---|---|---|---|
| **T1 BATERÍA** | Fijo | Rejilla TR de 16 instrumentos **modelados** (calculados en vivo): kits 808, 909 y SINTE | SEQ · SONIDO · INSTR · MEZCLA · MAS |
| **T2 SAMPLER** | Fijo | Un loop en 8 slices que un generador rearma | SEQ · SONIDO · RETRIG · TOMAS · MEZCLA · MAS |
| **T3 BAJO** | Fijo | **ANALOG** (de una voz, con glide), **ACID** o **FM** | SEQ · SONIDO · MEZCLA · ARP · LOOP · MAS |
| **T4-T6** | Libres | **ANALOG**, **FM**, **ACID**, **KIT** (otra batería) o **MIDI** (manda notas por MIDI) | según el motor |

El motor de un canal libre (y el de T3: ANALOG, ACID o FM) se elige en **MAS › MOTOR** con ALGORITHM. Entre motores de notas (ANALOG, FM, ACID, MIDI) las notas se quedan; pasar a KIT o volver de KIT pregunta, porque el canal empieza vacío.

**MAS** es la lista con todo el canal: motor, acento, largo, velocidad, transponer, canal MIDI y sonido inicial (en el sampler, además: usar el BPM del loop e interpolación). SELECT elige la fila y ALGORITHM la cambia.

**Las negras siguen una sola gramática en SEQ:** 1-8 son los elementos del canal, 9 y 10 lo modifican y **11 rompe el patrón** (FILL en las baterías y las notas, STUTTER en el sampler).

## Los motores de sonido

Desde 0.4 todo se calcula en vivo: no quedan sonidos grabados (solo los loops de fábrica del sampler). Hasta la v1 cada versión puede cambiar el formato: los patrones de una versión anterior se borran al primer arranque.

| Motor | Qué es | K1-K4 en SEQ | PRESETS |
|---|---|---|---|
| **ANALOG** | Analógico virtual: dos osciladores de forma variable (SIERRA, CUADRADA con ancho de pulso, TRIANGULO, SENO con plegado, 3 SIERRAS), sync, ring, sub y ruido; filtro LP 24/12, BP o HP con resonancia y drive; dos ADSR (filtro y nivel), LFO y modo POLI, MONO o LEGATO con glide | CORTE · RESO · ENVOLV · FORMA | 16 presets: SIERRA, BAJO, SUB BAJO, PLUCK, PAD, CUERDAS, SYNC, SOLISTA, METALES, ORGANO, CAMPANA, PWM, STAB, SUPER, VIENTO, CHIRRIDO |
| **FM** | 6 operadores, compatible con la DX7 (msfa, el núcleo de Dexed) | BRILLO · REALIM · DECAY · ALGORITMO | 16 patches propios |
| **ACID** | Monofónico estilo 303: filtro escalera de 4 polos, acento que abre el filtro y slide con LIGA | CUTOFF · RESO · DECAY · ENV MOD | SIERRA o CUADRADA |
| **Batería** | 16 instrumentos modelados. Cada uno elige su **modelo**: BD 808, BD 909, SD 808, SD 909, HAT 808, METAL (hats y platillos), PALMAS, TOM (y congas), CENCERRO, RESON (rim, clave, bloque), RUIDO (maracas) o FM (zap, metales) | TONO · AFINA · DECAY · COLOR | Kits 808, 909 y SINTE |

Bombos, cajas y hats son los modelos de **Plaits** (Mutable Instruments), que imitan los circuitos de la 808 y versiones sintéticas al estilo de la 909. Con el **acento** pegan distinto, no solo más fuerte.

### Editar el sonido (ANALOG)

**SONIDO** es el editor del motor, en 9 páginas de 4 ajustes: OSC 1, OSC 2, CRUCE (sync, ring, envolvente al osc 2, glide), FILTRO, FILTRO + (envolvente, seguimiento de teclado, velocidad), ENV FILTRO, ENV NIVEL, LFO y VOZ.

- **K1-K4** cambian los 4 ajustes de la página; abajo se ven sus valores y en el medio, un dibujo (la onda, la curva del filtro, la envolvente o el LFO).
- **SELECT** pasa de página; en la primera o la última sigue a la pestaña de al lado.
- **EDIT** otra vez = la página siguiente. **ENV** abre ENV NIVEL (otra vez: ENV FILTRO). **LFO** abre el LFO.
- **PRESETS** carga un preset completo. CORTE, RESO, ENVOLV y FORMA son también las perillas de SEQ y admiten P-Lock por nota.

### Editar un instrumento (batería)

**INSTR** (EDIT otra vez, o ENV) edita el instrumento elegido en dos páginas: **GOLPE** (AFINA, DECAY, TONO y COLOR, que dice qué hace en ese modelo: GOLPE, BORDONA, RUIDO, ESPACIO…) e **INSTRUMENTO** (MODELO, NIVEL, CORTA A —grupo de choke— y LARGO de la línea). EDIT otra vez pasa de página. Las negras 1-8 y 9 eligen el instrumento.

## Batería (T1 y canales KIT)

La pantalla SEQ es una matriz: 16 instrumentos × los 16 pasos de la página. Cada línea muestra su cabezal, porque cada instrumento puede tener su largo.

| Control | Hace |
|---|---|
| **Blancas** | Pasos del instrumento elegido: toque = poner o quitar |
| **Negras 1-8** | Eligen (y tocan) un instrumento de la sección |
| **Negra 9** | Cambia de sección (1-8 / 9-16) |
| **Negra 10** | Pantalla de **MUTES**: un toque la deja abierta (las blancas mutean o hacen sonar cada instrumento; se sale con la negra 10 o HOME); mantenerla = solo mientras la sostienes. El instrumento muteado queda en rojo |
| **Negra 11** | Pantalla de **FILL**: mientras está abierta suena el fill y las blancas dejan un golpe **solo para el fill** (otra vez: se borra). K1 = fill automático (cada 2, 4 u 8 compases). Toque = queda abierta; mantener = solo mientras la sostienes |
| **OCT − / +** | Página |

**Mantener un paso + girar:** K1-K4 = P-Lock de TONE, TUNE, DECAY o START solo en ese paso · SELECT = acento · ALGORITHM = repetición ×1 a ×4 o FLAM · PRESETS = condición (SIEMPRE, SOLO FILL, SIN FILL).

**INSTR** (EDIT otra vez, o ENV): el modelo y el golpe de cada instrumento, su nivel y el **largo de la línea**. Por ejemplo, un hat de 12 pasos contra un bombo de 16.

**SONIDO:** los 16 pads del kit con su modelo; K1-K4 = TONO, AFINA, DECAY y COLOR de todo el kit (en el centro, el kit tal cual). PRESETS = otro kit.

## Sampler 3.0 (T2)

La idea viene del BREAK de X0X (un loop en slices que un generador reordena), con una pantalla que deja ver lo que pasa.

- **Slices:** el loop A partido en 8. Las barras muestran la energía de cada slice; **K** y **S** marcan dónde están el bombo y la caja.
- **Fila TU** (lo que programaste): hueco = silencio, recuadro = **el generador elige**, lleno con número = **slice fijo**. Raya blanca arriba = acento.
- **Fila SUENA:** el slice que eligió el generador en cada paso de este compás. Raya naranja = retrig, **<** = reversa, **H** = media velocidad, **-** = silencio de ROMPER.

| Control | Hace |
|---|---|
| **Blanca** | Silencio ↔ generador. En un paso fijo, lo suelta al generador |
| **Blanca sostenida** | El loop suena desde ese paso, entero y en bucle, mientras la sostienes (el secuenciador del sampler se calla). Sirve para escuchar dónde estás antes de fijar un slice |
| **Blanca sostenida + negra 1-8** | Fija ese paso a ese slice |
| **Negras 1-8** | Tocan ese slice. Con REC sostenido se escriben como pasos fijos |
| **Negras 9 · 10 · 11** | Reversa · media velocidad · STUTTER, mientras las sostienes |
| **K1-K4** | **VARIAR** (cuánto cambia el orden) · **ANCLAR** (respeta los tiempos fuertes, con K y S) · **REPETIR** (retrigs) · **ROMPER** (reversa, media velocidad, silencios y repeticiones) |
| **EDIT sostenido + negra 4** | **CAPTURAR:** lo que suena en este compás queda fijo. HOME + OCT− lo deshace |

El generador decide cada compás con una semilla: la fila SUENA es exactamente lo que va a sonar, y cambia mientras giras las perillas. Con todo en 0, el loop suena en orden.

**RETRIG** (SEQ otra vez): cuando el generador repite, elige 2X, 3X, 4X u 8X con estos pesos.

**SONIDO:** K1 = LOOP A, K2 = LOOP B, K3 = cuántos compases usan el B, K4 = PITCH. El loop sigue el tempo: suena a BPM / BPM del loop, y la pantalla dice cuántos semitonos se movió. **MAS › USAR BPM DEL LOOP** pone el tempo del proyecto al del loop. PRESETS también elige el loop A.

**Loops de fábrica:** en la ROM vienen BUBBA 160 y BUBBA 128, sintetizados en el repositorio. No gastan memoria de tomas.

**El dibujo** muestra tres carriles por frecuencia: graves abajo (naranja), medios (gris) y agudos arriba (cian). Con música muy comprimida la forma de onda es un bloque; por bandas se ven el bombo, el rumble y los hats.

## Grabadora (REC en el sampler)

Grabar, guardar y cortar son tres pantallas, una tarea cada una.

1. **GRABAR:** REC en T2 abre la grabadora **armada**. Empieza en el siguiente compás (K1 = INICIO: COMPÁS o YA). Un número grande dice los segundos que **quedan**: el menor entre la RAM de grabación (3 s) y la memoria libre de tomas. Las rayas blancas marcan cuántos compases caben y la ámbar, el tope. REC otra vez = parar; también se para sola en el tope. Mantener REC en la grabadora graba solo mientras lo sostienes.
   - Debajo del número: la memoria libre de tomas en segundos y KB, y el máximo por toma (3 s).
   - Mientras graba, **las teclas no suenan** y el delay y los punch-in con buffer se pausan (comparten la memoria).
   - La toma queda **SIN GUARDAR** y ya suena como loop A. **Se guarda al parar** el transporte, porque escribir la flash detiene el audio. HOME = descartar.
2. **TOMAS** (EDIT 2 s en T2, o la pestaña): la biblioteca, con los loops de fábrica (FAB) primero. ALGORITHM elige; K1-K3 = BPM, compases y ganancia de la toma; las blancas 1-8 escuchan sus slices. **N1** = usar como loop A, **N2** = como loop B, **N11** = borrar (pregunta). Abajo, la memoria libre de 7.4 s.
**Cuánto cabe:** cada toma, hasta 3 s (la RAM de grabación, 48 KB); todas juntas, unos 7.4 s (116 KiB de flash, 8 bits a 16 kHz = 16 KB por segundo). Los loops de fábrica no gastan ese espacio. El emulador muestra la memoria libre junto al botón para cargar audio.

3. **CORTAR** (EDIT sobre una toma): las **blancas** ponen o quitan un corte en esa corchea, la **negra 9** pone los cortes en los golpes y la **10** vuelve a 8 partes iguales. Las negras 1-8 escuchan cada slice. SELECT o K1 eligen el corte, K2 lo mueve, K3 = zoom, K4 = pegar a 1/16 o 1/8. Se guarda al salir.

## Canales de notas (T3, ANALOG, FM, ACID, MIDI)

- **SEQ:** la página es solo del canal. Arriba, los 16 pasos de la página como **piano roll**: cada nota a su altura y con lo que dura; las blancas son esos pasos (hasta 4 notas por paso). Abajo, los compases del canal y el **seguidor de tempo punteado**. **Mantener un paso** y tocar **negras 1-8** escribe esas notas de la escala del canal (otra vez la misma: la quita). Negras 9 y 10 bajan o suben su octava; 11 = FILL.
- **Grabación libre, como una loopera:** en un canal de notas **vacío**, REC graba sin largo fijo (la pantalla dice REC LIBRE y cuenta los compases). REC otra vez lo cierra: el canal queda con los compases completos que tocaste (1 a 4) y **cada nota con lo que la sostuviste** (hasta 4 compases). Si el canal ya tiene notas, REC graba encima con su largo.
- **Largo en compases:** SEQ sostenido + negras 7-10 = 1, 2, 3 o 4 compases.
- **Paso sostenido + perillas:** K1-K4 = P-Lock · SELECT = golpe (NORMAL › ACENTO › FANTASMA) · ALGORITHM = probabilidad o fill · PRESETS = largo o LIGA (glide).
- **SONIDO:** en ANALOG, el editor por páginas (arriba). En FM y ACID, K1-K3 = las macros del motor y K4 = acento; sus envolventes están en ENV.
- **LOOP (SEQ otra vez):** loopera sin cuantizar. K1 ON/OFF · K2 compases (1, 2 o 4). **REC** arma (empieza en el compás) › al cerrar pasa a PLAY › REC otra vez: OVERDUB › REC: PLAY. **REC 2 s** borra el loop.
- **MIDI:** el motor MIDI no suena por dentro: manda sus notas por MIDI en su canal. Su página tiene canal, programa y dos CC (1 y 74).

## Las capas (mantener)

Mientras sostienes, la pantalla dibuja el teclado con lo que hace cada tecla y sus LEDs. La secuencia sigue sonando.

| Mantener | Capa | Las teclas |
|---|---|---|
| **HOME** | Controlador | Blancas = patrón en cola · negras 1-6 = mute / solo · 7-10 = escenas · 11 = FILL. ALGORITHM = ir a un canal al soltar |
| **SEQ** | Largo y velocidad | Blanca = largo del canal (1-16; OCT−/+ = 17-64) · negras 1-6 = velocidad ×2 a ×1/4 · negras 7-10 = 1-4 compases |
| **EDIT** | Borrar | Sonando: mantener una blanca borra lo que pasa bajo el cabezal; parado: borra la línea o el paso · negras 1-3 = copiar, pegar, limpiar el canal · sampler: negra 4 = CAPTURAR · OCT−/+ = deshacer / rehacer |
| **ENV** | Acento | Blanca = acento sí / no (paso vacío = nuevo con acento; en notas: NORMAL › ACENTO › FANTASMA). K1 = cuánto acento |
| **LFO** | Repetir · ligar | Rejillas: blanca = ×1 › ×2 › ×3 › ×4 › FLAM. Notas: blanca = liga la nota con la siguiente (K1 = glide) |
| **FX** | Punch-in | Blancas = 16 efectos cuantizados a 1/16 · negras 1-6 siguen muteando |
| **SCL** | Acordes | Blancas = 16 acordes · negras 1-4 = +7, +9, SUS2, SUS4 |
| **ARP** | Arpegio · roll | Notas: lo que tocas se arpegia · rejillas: blanca = roll de ese instrumento o slice (K1 = velocidad; con REC se graba como repeticiones) |
| **SAVE** | Copiar patrón | Blanca = pegar el patrón ahí · negra (1-6) + blanca = solo ese canal. Si el destino tiene notas, pregunta |
| **GLO** | Tono | Todo el teclado transpone el proyecto (C4 = 0) · **PLAY = TAP tempo** sin parar |

## Tiempo (SEQ 2 s)

Muestra los 6 canales como barras de su duración real, cada una con su cabezal: el polímetro a la vista. SELECT o ALGORITHM eligen el canal; K1 = largo (1-64); K2 = velocidad (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4); K3 y K4 (rejillas): un instrumento y su largo.

El **largo maestro** (HOME + K3: 16, 32, 48, 64, 128 o INF) reinicia todos los canales juntos. Ahí cambian los patrones y las escenas.

## Escalas y acordes

- **ESCALA** (SCL): cada canal de notas puede tener su **escala propia** (K2: PROYECTO o una de 9 escalas, K1 = tónica). **HOME + SCL** cambia la escala del proyecto, que usan los canales sin escala propia.
- **ACORDES** (SCL otra vez): 16 acordes en una cuadrícula 4×4 con su grado y su nombre. El color dice su función: **azul** reposo (I, iii, vi), **verde** preparación (ii, IV), **naranja** tensión (V, vii). K1 = juego (DIATÓNICO, PROGRESIONES, NEO SOUL), K2 = voicing (cerrado, abierto o conducido), K3 = octava, K4 = strum.

## Escenas y canción

**ESCENAS** (HOME, HOME, HOME): una escena = patrón + mutes. Blancas 1-4 lanzan A-D al siguiente ciclo maestro (parado: ya); 5-8 guardan lo que suena en A-D; mantén una de 1-4 y toca otras para una **cadena**. Desde cualquier pantalla, con HOME, las negras 7-10 también las lanzan. **REC** en ESCENAS (o HOME + REC) = SONG REC.

**CANCIÓN** (HOME + SEQ): blanca agrega un patrón, negra 9 borra; K1 = paso, K2 = repeticiones, K4 = canción ON/OFF.

## Efectos

- **MEZCLA de cada canal** (FX): nivel, pan, envío al delay y **duck** (el sidechain del bombo de T1; el sampler también se puede agachar).
- **HOME + FX:** tiempo, feedback y mezcla del delay, y release del sidechain.
- **FX sostenido:** 16 punch-in (stutter, reversa, tape stop, gate, barridos, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (HOME + K4): saturación de la mezcla.

## Guardar

- La flash **solo se escribe parado** (escribir pausa el audio un momento).
- **Autoguardado:** parado, a los 10 s sin tocar nada.
- **SAVE:** guardar · guardar como · guardar canción y escenas · borrar patrón (SAVE dos veces) · contar eventos. **SAVE 2 s:** guardado rápido. **HOME + SAVE:** guardar como.
- **Copiar:** SAVE sostenido + blanca copia el patrón; HOME + EDIT copia un canal a otro del mismo patrón (ALGORITHM elige el destino, OCT+ pega; un canal libre cambia de motor si hace falta).

## Sistema (HOME 2 s)

Reloj interno o externo · Split Sync (L = pulso, R = mezcla mono) · PPQN y latencia del pulso · autoguardado · brillo de los LEDs en reposo · entrada automática del recuadro · patrones guardados · tomas y memoria libre · versión.

## MIDI

- **Entrada** (USB y TRS): canales 1-6 = canales de Bubba Machine. En las rejillas, la nota elige el instrumento (0-15); en el sampler, el slice.
- **Reloj externo:** sigue el MIDI Clock y Start/Stop.
- **Salida:** MIDI Clock cuando el reloj es interno, y las notas de los canales con motor MIDI.
