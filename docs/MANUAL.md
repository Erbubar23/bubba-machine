# Bubba Machine 0.6 — Manual

Bubba Machine convierte el FM-1 en una groovebox de directo con **seis máquinas fijas**, cada una con un nombre que recuerda a la máquina que la inspiró: **BR-8** (batería), **BP-1** (sampler de pads), **BB-303** o **B-MINI** (bajo), **BX-7** (FM de 6 operadores), **BITTAL** (wavetable que carga presets de Vital) y **BUNO-6** (polifónico con chorus). En HOME está el mezclador **BX-1**. Cada canal tiene su propio largo y su propia velocidad.

> Los nombres recuerdan a máquinas famosas (TR-8, OP-1, TB-303, Minimoog, DX7, Vital, Juno-6), pero Bubba Machine no tiene relación con sus marcas ni con sus fabricantes.

La interfaz se resume en dos frases: **cuatro gestos por botón, y significan lo mismo en todos. HOME en todas partes.**

> **La pantalla viene en inglés.** Este manual usa los nombres del **paquete en español**, que el instalador web carga solo al terminar (o aparte, ver [Idioma de la pantalla](#idioma-de-la-pantalla), con la tabla de los nombres en inglés).

> La 0.6 cambió el formato de los patrones. **No lee lo que guardó una versión anterior**: al arrancar por primera vez borra los patrones y empieza con la demo (las tomas se quedan).

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

| Perilla | En HOME | Dentro de un canal |
|---|---|---|
| **SELECT** | El **cursor**: elige el canal del mezclador | La página (las pestañas de arriba) o la fila de una lista. Con un paso sostenido, su acento |
| **ALGORITHM** | El **BPM** (HOME + ALGORITHM: de a décimas) | El **modo** de la máquina (tabla abajo). En una lista, cambia el valor marcado |
| **PRESETS** | El patrón | El sonido: kit, patch o un sonido del banco |
| **K1-K4** | **VOL** del canal del cursor · **ENVÍO FX** (reverb) · **FILTRO** del master · **BEAT FX** | Lo que dicen los cuatro diales de abajo. Con HOME sostenido: BPM, swing, largo maestro y drive |

**Entrar a un canal:** en HOME, SELECT lleva el cursor al canal y **SEQ** o **EDIT** entran. Desde cualquier pantalla, **HOME + SELECT** muestra un recuadro con el canal destino y entras al soltar HOME.

**ALGORITHM dentro de un canal (el modo):**

| Máquina | ALGORITHM |
|---|---|
| BR-8 | Elige el instrumento (con el recuadro: el número grande, su nombre y modelo) |
| BP-1 | Elige el pad |
| BB-303 / B-MINI | Cambia de una a la otra (las notas se quedan) |
| BX-7 | El algoritmo, de 1 a 32 |
| BITTAL | La tabla del oscilador 1: la propia o la de otro sonido del banco |
| BUNO-6 | El chorus: OFF, I, II o I+II |

**Prioridad:** REC en un canal de notas > paso sostenido > lista > canal > HOME.

## La pantalla

Cuatro franjas, siempre en el mismo lugar:

1. **Cabecera:** la máquina en la que estás (BX-1 en HOME), PLAY/REC, el patrón y el compás, el BPM y la **carga del audio** (CPU). Mientras grabas notas dice el largo con que se graban (TIE o G75%). También el anillo de los 2 s y la batería.
2. **Visor:** lo propio de cada pantalla. Dentro de un canal, sus pestañas arriba.
3. **Teclado:** las 11 negras y las 16 blancas con lo que hace cada una y el mismo estado que su luz. En SEQ, **las blancas son los 16 pasos**; en un canal de notas, una barra verde menta marca las notas largas.
4. **Perillas:** K1-K4 como cuatro diales con su valor y su nombre, en su orden físico. Debajo, qué hacen SELECT, ALGORITHM y PRESETS en esa pantalla.

La paleta: fondo negro azulado, **cian** lo activo, **ámbar** los valores y el paso que suena, **rojo láser** REC y mute, **verde menta** las notas largas. Un **borde rojo** = la mezcla saturó.

## Los 14 botones

| Botón | Toque | Otra vez | Mantener (capa) | 2 s | HOME + |
|---|---|---|---|---|---|
| **HOME** | HOME: el mezclador BX-1 | FADERS › ESCENAS › CANCIÓN | **Controlador** (ver abajo) | SISTEMA | (es el modificador) |
| **SEQ** | Pasos del canal | Notas: LOOP · sampler: ROMPER | **Largo y velocidad** | TIEMPO de los 6 canales | CANCIÓN |
| **EDIT** | SONIDO (BP-1: PADS) | La página siguiente del editor | **Borrar** | Sampler: TOMAS · otros: sonido inicial | Copiar el canal a otro |
| **ENV** | La envolvente de la máquina · INSTR (batería) | La otra envolvente | **Acento** | — | — |
| **LFO** | El LFO de la máquina (B-MINI, BITTAL, BUNO-6) | — | **Repetir** (rejillas) · **ligar** (notas) | — | — |
| **FX** | MEZCLA del canal | DUCK (sidechain) | **Punch-in** | — | FX del master: delay; otra vez: reverb |
| **SCL** | ESCALA del canal | ACORDES | **Acordes** en vivo | — | Escala del proyecto |
| **ARP** | ARP del canal | — | **Arpegio** (notas) · **roll** (rejillas) | — | — |
| **SAVE** | Menú de guardar | Ejecuta | **Copiar patrón** | Guardado rápido | Guardar como |
| **GLO** | Lista global | Cierra | **Tono** y TAP tempo | — | — |
| **REC** | Grabar (en el sampler: GRABADORA) | Loopera: sobregrabar › play | Grabar solo mientras lo sostienes | Borrar el canal (pregunta) | SONG REC |
| **PLAY** | Play / stop | — | — | — | Volver al paso 1 sin parar |
| **OCT − / +** | Octava · página de pasos · en preguntas OCT+ = sí | — | — | — | Deshacer / rehacer |

## HOME en todas partes

El FM-1 no tiene SHIFT, así que HOME hace tres trabajos:

1. **El mezclador BX-1:** una fila por canal con su máquina, su secuencia (pulsos para los golpes, bloques verdes para las notas largas), el efecto que más usa (DLY, REV, DCK) y su volumen con el pico. Abajo, el BEAT FX y el filtro del master. SELECT mueve el cursor, K1-K4 mezclan (tabla de las perillas) y SEQ o EDIT entran al canal. Otra vez HOME: FADERS (los seis volúmenes), ESCENAS y CANCIÓN.
2. **Controlador que te sigue:** mantener HOME trae el controlador a cualquier pantalla. Editas el bajo y, sin salir, muteas o pones el siguiente patrón en cola.
3. **Modificador:** HOME + botón = su versión global (tabla de arriba).

Si usaste HOME como modificador, soltarlo **no** te lleva a la pantalla de inicio: HOME solo navega con un toque limpio.

**Teclas con HOME (en la pantalla HOME, en FADERS o sosteniendo HOME):**

| Tecla | Qué hace |
|---|---|
| **Blancas 1-16** | Ponen en cola ese patrón (parado: cambia al instante). Mantén una y toca otra: **cadena** del primero al segundo |
| **Negras 1-6** | Toque: mute de T1-T6. Mantener: **solo** mientras la sostienes |
| **Negras 7-10** | Lanzan las escenas A-D |
| **Negra 11** | FILL mientras la sostienes |

**BEAT FX** (K4 en HOME): uno de los 16 efectos de punch-in queda puesto (stutter, reversa, tape, gate, barridos, bitcrush…) hasta que lo vuelvas a OFF. **FILTRO** (K3): en el centro está abierto; a la izquierda, pasa-bajos; a la derecha, pasa-altos.

## Las seis máquinas

| Canal | Máquina | Qué es | Pestañas |
|---|---|---|---|
| **T1** | **BR-8** | Batería: rejilla TR de 16 instrumentos **modelados** (calculados en vivo); kits 808, 909 y SINTE | SEQ · SONIDO · INSTR · MEZCLA · MAS |
| **T2** | **BP-1** | 16 pads hechos con tus tomas, en una rejilla | SEQ · PADS · ROMPER · TOMAS · MEZCLA · MAS |
| **T3** | **BB-303** o **B-MINI** | El bajo: ácido estilo 303, o tres osciladores con filtro escalera | SEQ · SONIDO · MEZCLA · ARP · LOOP · MAS |
| **T4** | **BX-7** | FM de 6 operadores, compatible con la DX7 y sus bancos .syx | SEQ · SONIDO · ENV · MEZCLA · ARP · LOOP · MAS |
| **T5** | **BITTAL** | Dos wavetables de un sonido del banco (presets de Vital), con unísono y 4 macros | SEQ · SONIDO · MEZCLA · ARP · LOOP · MAS |
| **T6** | **BUNO-6** | Polifónico de 6 voces: un oscilador por voz, filtro y el chorus de un Juno | SEQ · SONIDO · MEZCLA · ARP · LOOP · MAS |

Las máquinas no se cambian: cada canal es la suya. T3 elige entre BB-303 y B-MINI con ALGORITHM (o en MAS › MÁQUINA) y las notas se quedan.

**MAS** es la lista con todo el canal: máquina (solo T3), acento, largo, velocidad, transponer, **MIDI OUT** (el canal además manda sus notas por MIDI) y su canal MIDI, y sonido inicial (en el sampler, además: interpolación). SELECT elige la fila y ALGORITHM la cambia.

**Las negras siguen una sola gramática en SEQ:** 1-8 son los elementos del canal, 9 y 10 lo modifican y **11 rompe el patrón** (FILL).

## El sonido de cada máquina

Todo se calcula en vivo; el sampler usa tus tomas. Hasta la v1 cada versión puede cambiar el formato: los patrones de una versión anterior se borran al primer arranque.

| Máquina | Qué es | K1-K4 en SEQ | PRESETS |
|---|---|---|---|
| **BR-8** | 16 instrumentos modelados. Cada uno elige su **modelo**: BD 808, BD 909, SD 808, SD 909, HAT 808, METAL, PALMAS, TOM (y congas), CENCERRO, RESON (rim, clave, bloque), RUIDO o FM | TONO · AFINA · DECAY · COLOR | Kits 808, 909 y SINTE |
| **BB-303** | Monofónico estilo 303: filtro escalera de 4 polos, acento que abre el filtro y slide con LIGA | CUTOFF · RESO · DECAY · ENV MOD | SIERRA o CUADRADA |
| **B-MINI** | Tres osciladores (TRI, SHARK, SIERRA, CUADRADA, ANCHO, ANGOSTO) en registros de 32' a 2', ruido, filtro escalera con drive, dos envolvencias (CONTORNO y VOLUMEN), LFO y glide | CUTOFF · ÉNFASIS · AFINA OSC · CONTORNO | 6 bajos del banco |
| **BX-7** | 6 operadores (msfa, el núcleo de Dexed); los 32 algoritmos de la DX7 | BRILLO · REALIM · DECAY · ALGORITMO | 16 patches propios y las voces de tus bancos .syx |
| **BITTAL** | Dos osciladores de tabla (hasta 8 frames, sin aliasing en ninguna nota) con unísono de hasta 4, filtro (LP 24/12, BP, HP o escalera) con drive, dos ADSR, LFO y 4 macros | Las 4 macros del sonido (su nombre dice a dónde van: CUTOFF, POSICIÓN, LFO, SPACE…) | 19 sonidos del banco y los tuyos |
| **BUNO-6** | Un oscilador por voz (sierra, pulso con PWM, sub una octava abajo y ruido), pasa-altos de 4 posiciones, filtro de 24 dB con resonancia, una ADSR, LFO con retardo y chorus estéreo | FREQ · RES · ENV · PWM | 12 sonidos propios |

Bombos, cajas y hats de BR-8 son los modelos de **Plaits** (Mutable Instruments), que imitan los circuitos de la 808 y versiones sintéticas al estilo de la 909. Con el **acento** pegan distinto, no solo más fuerte.

**El banco de sonidos.** B-MINI, BITTAL y BUNO-6 cargan **sonidos** (.bsnap): sus ajustes, sus macros y, en BITTAL, sus tablas. Los de fábrica vienen de presets de Vital con licencia libre (CC0, Unlicense y GPL-3.0; los créditos están en [CREDITOS.md](../CREDITOS.md)) y de presets propios de BUNO-6 hechos en Vital. Los tuyos se agregan desde el editor web ([sonidos](../web/sonidos.html)): PRESETS recorre primero los de fábrica y después los tuyos. La huella del sonido (#A4F9) se ve arriba a la derecha en SONIDO.

### Editar el sonido (B-MINI, BITTAL, BUNO-6)

**SONIDO** es el editor de la máquina, en páginas de 4 ajustes:

- **B-MINI:** OSC 1, OSC 2, OSC 3, FILTRO, FILTRO +, CONTORNO, VOLUMEN, MOD y VOZ.
- **BITTAL:** OSC 1, OSC 1 +, OSC 2, OSC 2 +, FILTRO, FILTRO +, ENV NIVEL, ENV MOD, LFO, MACRO 1 2, MACRO 3 4 y VOZ. Las páginas de los osciladores dibujan la tabla en 2.5D, con el frame que suena en cian.
- **BUNO-6:** DCO, DCO +, VCF, VCF +, ENV y LFO VCA (con el chorus).

**K1-K4** cambian los 4 ajustes de la página; arriba, un dibujo de lo que hace (la onda, la tabla, la curva del filtro, la envolvente o el LFO). **SELECT** pasa de página (en la primera o la última sigue a la pestaña de al lado). **EDIT** otra vez = la página siguiente. **ENV** abre la envolvente de volumen (otra vez: la de filtro). **LFO** abre el LFO. Las macros (K1-K4 de SEQ) admiten P-Lock por nota.

### BX-7: el algoritmo

SONIDO dibuja el **algoritmo como en una DX7**: las portadoras abajo, los moduladores encima, la realimentación en ámbar y el nivel de cada operador; abajo, la relación de frecuencia de cada uno. ALGORITHM recorre los 32 (con BRILLO, REALIM y DECAY encima del patch). Los bancos **.syx** de la DX7 (32 voces) se cargan desde el editor web o con `py -3 tools/fm1/sonido.py banco.syx`.

### Editar un instrumento (batería)

**INSTR** (EDIT otra vez, o ENV) edita el instrumento elegido en dos páginas: **GOLPE** (AFINA, DECAY, TONO y COLOR, que dice qué hace en ese modelo: GOLPE, BORDONA, RUIDO, ESPACIO…) e **INSTRUMENTO** (MODELO, NIVEL, CORTA A —grupo de choke— y LARGO de la línea). EDIT otra vez pasa de página. Las negras 1-8 y 9 eligen el instrumento.

## Batería (BR-8)

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

## Sampler de pads (BP-1)

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

**EDIT otra vez** pasa de página: **RECORTE › SONIDO › ENV › LOOP** (la de arriba a la derecha está en ámbar). ENV también abre la página ENV.

| Página | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| **RECORTE** | INICIO (el final queda) | FINAL (el largo) | ZOOM X1-X8 | PEGAR: OFF, 1/16, 1/8 |
| **SONIDO** | PITCH (±24 st) | NIVEL | PLAY: ONESHOT, KEY, THRU, LOOP | SENTIDO: NORMAL o REV |
| **ENV** | ATAQUE | SALIDA | CORTA A: grupo de corte | TEMPO: sigue el BPM o no |
| **LOOP** | LOOP DESDE | LOOP HASTA (ENTERO = el pad entero) | PLAY | — |

**Cómo responde la tecla (PLAY):**

- **ONESHOT:** suena entero aunque sueltes.
- **KEY:** suena mientras sostienes.
- **THRU:** suena el pad y, sostenido, **sigue por el resto de la toma**.
- **LOOP:** repite el pad mientras sostienes. Con la página **LOOP** repite solo un tramo (el **sustain loop** del OP-1): sostenido, ese tramo se repite; al soltar, sigue hasta el final del pad. El tramo se ve en verde menta sobre la onda.

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

**Cuánto cabe:** las tomas comparten **100 KiB** de flash con tus sonidos del banco: las tomas se guardan desde abajo y los sonidos desde arriba, así que el reparto lo decides tú (8 bits a 16 kHz = 16 KB por segundo, hasta 3 s por toma). TOMAS muestra lo libre y cuántos sonidos tuyos hay arriba.

## Canales de notas (T3-T6)

- **SEQ:** la página es solo del canal. Arriba, los 16 pasos de la página como **piano roll**: cada nota a su altura y con lo que dura (las largas en verde menta, las adelantadas o atrasadas corridas de su paso). Las blancas son esos pasos (hasta 4 notas por paso). Abajo, los compases del canal y el cabezal. **Mantener un paso** y tocar **negras 1-8** escribe esas notas de la escala del canal (otra vez la misma: la quita). Negras 9 y 10 bajan o suben su octava; 11 = FILL.
- **Nota larga con dos blancas:** mantén el paso donde empieza y toca la blanca donde termina: la nota dura de una a la otra. Las luces de las teclas que cubre quedan a media luz, como una estela.
- **Grabación libre, como una loopera:** en un canal de notas **vacío**, REC graba sin largo fijo (la pantalla dice REC LIBRE y cuenta los compases). REC otra vez lo cierra: el canal queda con los compases completos que tocaste (1 a 4) y cada nota con lo que la sostuviste (hasta 4 compases). Si el canal ya tiene notas, REC graba encima con su largo.
- **Largo en compases:** SEQ sostenido + negras 7-10 = 1, 2, 3 o 4 compases.
- **Paso sostenido + perillas:** K1-K4 = P-Lock · SELECT = golpe (NORMAL › ACENTO › FANTASMA) · ALGORITHM = probabilidad o fill · PRESETS = largo o LIGA (glide).
- **LOOP (SEQ otra vez):** loopera sin cuantizar. K1 ON/OFF · K2 compases (1, 2 o 4). **REC** arma (empieza en el compás) › al cerrar pasa a PLAY › REC otra vez: OVERDUB › REC: PLAY. **REC 2 s** borra el loop.
- **MIDI OUT** (en MAS): el canal suena y además manda sus notas por MIDI en su canal.

### REC expresivo

Con **REC** en un canal de notas, lo que tocas queda como lo tocaste:

- **La nota guarda su tiempo real:** el comienzo con su **micro-tiempo** (lo que te adelantaste o atrasaste del paso, en ticks) y el final con su **gate** (cuánto del último paso sonó). Si la nota sigue apretada cuando termina el compás, se **corta en el compás**.
- **Las perillas quedan en la nota:** mientras sostienes una nota grabando, lo que mueves en K1-K4 queda en esa nota (P-Lock).
- **Editar el paso con REC:** **SELECT** elige un paso (recuadro rojo), **ALGORITHM** su largo (de 10 % staccato a 95 % = el paso entero, y 100 % = **TIE**, que la liga con la siguiente) y **PRESETS** su micro-tiempo (de −11 a +11 ticks). La cabecera muestra el largo con que vas grabando.

## Las capas (mantener)

Mientras sostienes, la pantalla dibuja el teclado con lo que hace cada tecla y sus LEDs. La secuencia sigue sonando.

| Mantener | Capa | Las teclas |
|---|---|---|
| **HOME** | Controlador | Blancas = patrón en cola · negras 1-6 = mute / solo · 7-10 = escenas · 11 = FILL. SELECT = ir a un canal al soltar · ALGORITHM = décimas de BPM |
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

- **MEZCLA de cada canal** (FX): nivel, pan, envío al **delay** y envío a la **reverb**. FX otra vez: **duck** (el sidechain del bombo de T1).
- **BUNO-6** tiene su propio **chorus** estéreo (I, II o I+II, con ALGORITHM). La macro **SPACE** de BITTAL suma envío a la reverb.
- **HOME + FX:** el delay (tiempo, realimentación, mezcla) y el release del sidechain. HOME + FX otra vez: la **reverb** (tamaño, amortiguación, mezcla).
- **En HOME:** el **filtro del master** (K3) y el **BEAT FX** (K4).
- **FX sostenido:** 16 punch-in (stutter, reversa, tape stop, gate, barridos, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (HOME + K4): saturación de la mezcla.

## Guardar

- La flash **solo se escribe parado** (escribir pausa el audio un momento).
- **Autoguardado:** parado, a los 10 s sin tocar nada.
- **SAVE:** guardar · guardar como · guardar canción y escenas · borrar patrón (SAVE dos veces) · contar eventos. **SAVE 2 s:** guardado rápido. **HOME + SAVE:** guardar como.
- **Copiar:** SAVE sostenido + blanca copia el patrón; HOME + EDIT copia un canal a otro del mismo patrón (notas a notas, golpes a golpes; ALGORITHM elige el destino, OCT+ pega). Las máquinas no cambian.

## Sistema (HOME 2 s)

Reloj interno o externo · Split Sync (L = pulso, R = mezcla mono) · PPQN y latencia del pulso · autoguardado · brillo de los LEDs en reposo · **PANTALLA RÁPIDA** y **DOS NÚCLEOS** (pruebas, apagadas de fábrica) · tomas, sonidos y memoria libre · **idioma** (LANGUAGE) · versión.

**Las dos pruebas:** **PANTALLA RÁPIDA** sube el SPI de la pantalla a 30 MHz (medido: el cuadro entero pasa de 85 a 39 ms). **DOS NÚCLEOS** calcula T4-T6 en el segundo núcleo del FM-1 (en la computadora da exactamente las mismas muestras). Están a prueba: si el FM-1 se cuelga o arranca en modo seguro, las dos vuelven a apagarse solas.

## MIDI

- **Entrada** (USB y TRS): canales 1-6 = canales de Bubba Machine. En las rejillas, la nota elige el instrumento (0-15); en el sampler, el pad.
- **Reloj externo:** sigue el MIDI Clock y Start/Stop.
- **Salida:** MIDI Clock cuando el reloj es interno, y las notas de los canales con **MIDI OUT**.

## Idioma de la pantalla

La pantalla viene en **inglés**, para que se entienda en todo el mundo. Otros idiomas son **paquetes** que no vienen cargados. El paquete vive en su propia zona de 16 KiB de la flash: no toca patrones ni tomas, y una actualización del firmware no lo borra.

- **Cargar el español en el FM-1:** el [instalador web](../web/instalar.html) lo carga solo al terminar (casilla «la pantalla en español»). También: con el transporte parado, `py -3 tools/fm1/idioma.py lang/es.bmlang`. Después, **SISTEMA › IDIOMA** (LANGUAGE) cambia entre inglés y el paquete. `--quitar` lo borra.
- **En el emulador:** el botón de *Sampler y memoria* carga o quita el español. En la página en español, la primera vez ya arranca en español.
- **Hacer una traducción:** copia `lang/plantilla.txt` (todas las frases de la pantalla) a `lang/<codigo>.txt`, escribe la traducción debajo de cada frase y ármala con `py -3 tools/gen_lang.py`. La fuente del FM-1 es ASCII: sin tildes ni eñe. Lo que no traduzcas se ve en inglés.

**Los nombres en inglés** (lo que se ve sin el paquete):

| Español | Inglés | | Español | Inglés |
|---|---|---|---|---|
| SONIDO | SOUND | | MEZCLA | MIX |
| MAS | MORE | | MAQUINA | MACHINE |
| TOMAS | TAKES | | ROMPER (página) | BREAK |
| GRABAR (grabadora) | RECORD | | RECORTE | TRIM |
| ESCENAS | SCENES | | CANCION | SONG |
| SISTEMA | SYSTEM | | TIEMPO | TIME |
| ESCALA | SCALE | | ACORDES | CHORDS |
| BORRAR | ERASE | | ACENTO | ACCENT |
| REPETIR | REPEAT | | LIGAR / LIGA | TIE |
| FILT MST | MST FILT | | ENVIO FX | FX SEND |
| CONTORNO | CONTOUR | | ENFASIS | EMPHASIS |
| TU / SUENA (ROMPER) | YOU / PLAYS | | VARIAR · ANCLAR · ROMPER | VARY · ANCHOR · BREAK |
| COPIA · PEGA · LIMPIA (pads) | CPY · PST · CLR | | INICIO · LARGO · PEGAR | START · LENGTH · SNAP |
| LARGO · VELOCIDAD | LENGTH · SPEED | | SIN GUARDAR | UNSAVED |
