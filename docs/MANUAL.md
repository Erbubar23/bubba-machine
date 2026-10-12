# Bubba Machine 0.3 — Manual

Bubba Machine convierte el FM-1 en una groovebox de directo de 6 canales: **3 fijos** (batería, sampler y bajo) y **3 libres** (ANALOG, FM, ACID, KIT o MIDI). Cada canal tiene su propio largo y su propia velocidad.

La interfaz 3.0 se resume en una frase: **cuatro gestos por botón, y significan lo mismo en todos. HOME en todas partes.**

> **La pantalla viene en inglés.** Este manual usa los nombres del **paquete en español**, que se carga aparte (ver [Idioma de la pantalla](#idioma-de-la-pantalla), con la tabla de los nombres en inglés).

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
| **SEQ** | Pasos del canal | Notas: LOOP · sampler: ROMPER | **Largo y velocidad** | TIEMPO de los 6 canales | CANCIÓN |
| **EDIT** | SONIDO (sampler: PADS) | Batería: INSTR · PADS: la página siguiente | **Borrar** | Sampler: TOMAS · otros: sonido inicial | Copiar el canal a otro |
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
| **T2 SAMPLER** | Fijo | 16 pads hechos con tus tomas, en una rejilla | SEQ · PADS · ROMPER · TOMAS · MEZCLA · MAS |
| **T3 BAJO** | Fijo | **ANALOG** (de una voz, con glide), **ACID** o **FM** | SEQ · SONIDO · MEZCLA · ARP · LOOP · MAS |
| **T4-T6** | Libres | **ANALOG**, **FM**, **ACID**, **KIT** (otra batería) o **MIDI** (manda notas por MIDI) | según el motor |

El motor de un canal libre (y el de T3: ANALOG, ACID o FM) se elige en **MAS › MOTOR** con ALGORITHM. Entre motores de notas (ANALOG, FM, ACID, MIDI) las notas se quedan; pasar a KIT o volver de KIT pregunta, porque el canal empieza vacío.

**MAS** es la lista con todo el canal: motor, acento, largo, velocidad, transponer, canal MIDI y sonido inicial (en el sampler, además: interpolación). SELECT elige la fila y ALGORITHM la cambia.

**Las negras siguen una sola gramática en SEQ:** 1-8 son los elementos del canal, 9 y 10 lo modifican y **11 rompe el patrón** (FILL).

## Los motores de sonido

Desde 0.4 todo se calcula en vivo: no quedan sonidos grabados (el sampler usa tus tomas). Hasta la v1 cada versión puede cambiar el formato: los patrones de una versión anterior se borran al primer arranque.

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

## Sampler de pads (T2)

T2 es un **sampler de 16 pads**, como un KO II o el drum sampler de un OP-1 field: cada pad es un trozo de una de tus tomas, y una rejilla igual a la de la batería los toca.

**Cómo se trabaja:**

1. **Grabas** una toma (REC en T2, ver [Grabadora](#grabadora-rec-en-el-sampler)) o la subes desde la computadora.
2. **La toma arma el kit:** si el kit está vacío, al guardarse la toma se corta en los golpes y cada corte queda en un pad. La rejilla queda escrita para que suene como la toma; desde ahí la cambias.
3. En **TOMAS** eliges qué toma va a qué pad: **N1** arma un kit nuevo en los golpes, **N2** en 16 partes iguales, **N3** en 8, y **N4** pone la toma entera en el pad elegido. Un kit puede mezclar varias tomas.
4. En **PADS** ajustas cada pad: dónde empieza y termina, su tono, su nivel, cómo responde a la tecla y su envolvente.
5. En **SEQ** programas los pads como la batería, y **ROMPER** (BREAK) los varía compás a compás.

Las pestañas: **SEQ · PADS · ROMPER · TOMAS · MEZCLA · MAS**. El kit vive en el patrón, como el kit de la batería; las tomas, en la biblioteca.

### SEQ: la rejilla de pads

Igual que la batería: una línea por pad, 8 a la vista. Cada pad tiene su largo.

| Control | Hace |
|---|---|
| **Blancas** | Pasos del pad elegido: toque = poner o quitar |
| **Negras 1-8** | Eligen y tocan un pad de la sección |
| **Negra 9 · 10 · 11** | Otra sección (pads 1-8 / 9-16) · MUTES · FILL |
| **K1-K4** | **FILTRO** (pasa-bajos) · **PITCH** (±12 semitonos) · **DECAY** · **INICIO** (corre el comienzo de todos los pads) |
| **Paso sostenido + K1-K4** | P-Lock en ese paso: un pad más agudo, más corto o desde más adelante solo ahí |

Las capas funcionan igual que en la batería: acento, repetir, borrar, roll (ARP sostenido) y los largos por línea en TIEMPO.

### PADS (EDIT): el editor de cada pad

Arriba, el **mapa** de la toma del pad, con todos los pads de esa toma marcados y el recuadro del zoom. En el medio, la **onda** grande, con el pad entre **corchetes cian** y los demás pads de la toma con su número. Abajo, **una casilla por blanca**, en el mismo lugar que la tecla: llena = tiene sonido, recuadro = el pad elegido. La raya naranja es lo que va sonando.

**EDIT otra vez** pasa de página: **RECORTE › SONIDO › ENV** (la de arriba a la derecha está en amarillo). ENV también abre la página ENV.

| Página | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| **RECORTE** | INICIO (el final queda) | FINAL (el largo) | ZOOM X1-X8 | PEGAR: OFF, 1/16, 1/8 |
| **SONIDO** | PITCH (±24 st) | NIVEL | PLAY: ONESHOT, KEY, THRU, LOOP | SENTIDO: NORMAL o REV |
| **ENV** | ATAQUE | SALIDA | CORTA A: grupo de corte | TEMPO: sigue el BPM o no |

**Cómo responde la tecla (PLAY):**

- **ONESHOT:** suena entero aunque sueltes.
- **KEY:** suena mientras sostienes.
- **THRU:** suena el pad y, sostenido, **sigue por el resto de la toma**.
- **LOOP:** repite el pad mientras sostienes.

Desde la rejilla, KEY y LOOP suenan un paso.

| Tecla | Hace |
|---|---|
| **Blanca** | Toca su pad y lo elige. Con REC + PLAY, el golpe queda en su línea |
| **N1 · N2 · N3** | COPIA el pad · lo PEGA en el elegido · lo LIMPIA (vacío) |
| **N4 GOLPE** | Lleva el inicio al golpe más cercano |
| **N9 · N10 · N11** | Reversa · media velocidad · STUTTER, mientras las sostienes |
| **PRESETS** | Otra toma entera en el pad elegido |

**TEMPO:** un pad que sigue el tempo suena a BPM del proyecto / BPM de la toma (como un loop cortado). Los kits que salen de una toma lo traen prendido y sus pads se cortan entre sí (grupo 1), como el loop original.

### ROMPER (BREAK; SEQ otra vez)

La idea del BREAK de X0X, sobre tu rejilla: cada compás varía lo que suena con una semilla. **Con las cuatro perillas en 0, suena la rejilla tal cual** (así viene).

- Fila **TU**: el pad que programaste en cada paso.
- Fila **SUENA**: lo que va a sonar este compás. En amarillo, los pasos que cambiaron. Raya naranja = repetición, **<** = reversa, **H** = media velocidad, **-** = silencio.

| Perilla | Hace |
|---|---|
| **VARIAR** | Cuántos pasos tocan otro pad del kit |
| **ANCLAR** | Los tiempos fuertes (1, 5, 9, 13) quedan como están |
| **REPETIR** | Retrigs de 2X a 8X |
| **ROMPER** | Reversa, media velocidad, silencios y repetir el paso anterior |

**EDIT sostenido + negra 4 (CAPT)** escribe en la rejilla lo que ROMPER está haciendo en este compás. HOME + OCT− lo deshace.

**El dibujo** es la onda espejada, como en un sampler: el pico en el tono oscuro y la energía (RMS) encima, a todo color.

## Grabadora (REC en el sampler)

Grabar, guardar y armar el kit son pasos separados.

1. **GRABAR:** REC en T2 abre la grabadora **armada** (en PADS, REC graba lo que tocas en la rejilla). Empieza en el siguiente compás (K1 = INICIO: COMPÁS o YA). Un número grande dice los segundos que **quedan**: el menor entre la RAM de grabación (3 s) y la memoria libre de tomas. Las rayas blancas marcan cuántos compases caben y la ámbar, el tope. REC otra vez = parar; también se para sola en el tope. Mantener REC en la grabadora graba solo mientras lo sostienes.
   - Debajo del número: la memoria libre de tomas en segundos y KB, y el máximo por toma (3 s).
   - Mientras graba, **las teclas no suenan** y el delay y los punch-in con buffer se pausan (comparten la memoria).
   - La toma queda **SIN GUARDAR**: las blancas la escuchan en 16 partes. **Se guarda al parar** el transporte, porque escribir la flash detiene el audio, y si el kit estaba vacío, lo arma. HOME = descartar.
2. **TOMAS** (EDIT 2 s en T2, o la pestaña): la biblioteca, con la onda de la toma marcada. ALGORITHM elige; K1-K3 = BPM, compases y ganancia de la toma; las blancas la escuchan en 16 partes (sostenida: sigue). **N1** = kit en los golpes, **N2** = 16 partes iguales, **N3** = 8, **N4** = la toma entera en el pad elegido, **N11** = borrar (pregunta; sus pads quedan vacíos). Si el kit ya tiene pads, armar uno nuevo pregunta. Un punto amarillo = el kit usa esa toma.

**Cuánto cabe:** cada toma, hasta 3 s (la RAM de grabación, 48 KB); todas juntas, unos 6.4 s (100 KiB de flash, 8 bits a 16 kHz = 16 KB por segundo). El emulador muestra la memoria libre junto al botón para cargar audio.

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
| **EDIT** | Borrar | Sonando: mantener una blanca borra lo que pasa bajo el cabezal; parado: borra la línea o el paso · negras 1-3 = copiar, pegar, limpiar el canal · sampler: negra 4 = CAPT (lo que hace ROMPER queda en la rejilla) · OCT−/+ = deshacer / rehacer |
| **ENV** | Acento | Blanca = acento sí / no (paso vacío = nuevo con acento; en notas: NORMAL › ACENTO › FANTASMA). K1 = cuánto acento |
| **LFO** | Repetir · ligar | Rejillas: blanca = ×1 › ×2 › ×3 › ×4 › FLAM. Notas: blanca = liga la nota con la siguiente (K1 = glide) |
| **FX** | Punch-in | Blancas = 16 efectos cuantizados a 1/16 · negras 1-6 siguen muteando |
| **SCL** | Acordes | Blancas = 16 acordes · negras 1-4 = +7, +9, SUS2, SUS4 |
| **ARP** | Arpegio · roll | Notas: lo que tocas se arpegia · rejillas: blanca = roll de ese instrumento o pad (K1 = velocidad; con REC se graba como repeticiones) |
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

Reloj interno o externo · Split Sync (L = pulso, R = mezcla mono) · PPQN y latencia del pulso · autoguardado · brillo de los LEDs en reposo · entrada automática del recuadro · patrones guardados · tomas y memoria libre · **idioma** (LANGUAGE) · versión.

## MIDI

- **Entrada** (USB y TRS): canales 1-6 = canales de Bubba Machine. En las rejillas, la nota elige el instrumento (0-15); en el sampler, el pad.
- **Reloj externo:** sigue el MIDI Clock y Start/Stop.
- **Salida:** MIDI Clock cuando el reloj es interno, y las notas de los canales con motor MIDI.

## Idioma de la pantalla

La pantalla viene en **inglés**, para que se entienda en todo el mundo. Otros idiomas son **paquetes** que no vienen cargados. El paquete vive en su propia zona de 16 KiB de la flash: no toca patrones ni tomas, y una actualización del firmware no lo borra.

- **Cargar el español en el FM-1:** con el transporte parado, `py -3 tools/fm1/idioma.py lang/es.bmlang`. Después, **SISTEMA › IDIOMA** (LANGUAGE) cambia entre inglés y el paquete. `--quitar` lo borra.
- **En el emulador:** el botón de *Sampler y memoria* carga o quita el español. En la página en español, la primera vez ya arranca en español.
- **Hacer una traducción:** copia `lang/plantilla.txt` (todas las frases de la pantalla) a `lang/<codigo>.txt`, escribe la traducción debajo de cada frase y ármala con `py -3 tools/gen_lang.py`. La fuente del FM-1 es ASCII: sin tildes ni eñe. Lo que no traduzcas se ve en inglés.

**Los nombres en inglés** (lo que se ve sin el paquete):

| Español | Inglés | | Español | Inglés |
|---|---|---|---|---|
| SONIDO | SOUND | | MEZCLA | MIX |
| MAS | MORE | | MOTOR | ENGINE |
| TOMAS | TAKES | | ROMPER (página) | BREAK |
| GRABAR (grabadora) | RECORD | | RECORTE | TRIM |
| ESCENAS | SCENES | | CANCION | SONG |
| SISTEMA | SYSTEM | | TIEMPO | TIME |
| ESCALA | SCALE | | ACORDES | CHORDS |
| BORRAR | ERASE | | ACENTO | ACCENT |
| REPETIR | REPEAT | | LIGAR / LIGA | TIE |
| BATERIA | DRUMS | | BAJO | BASS |
| TU / SUENA (ROMPER) | YOU / PLAYS | | VARIAR · ANCLAR · ROMPER | VARY · ANCHOR · BREAK |
| COPIA · PEGA · LIMPIA (pads) | CPY · PST · CLR | | INICIO · LARGO · PEGAR | START · LENGTH · SNAP |
| LARGO · VELOCIDAD | LENGTH · SPEED | | SIN GUARDAR | UNSAVED |
