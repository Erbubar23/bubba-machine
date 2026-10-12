# El audio de Bubba Machine

Bubba Machine tiene un objetivo: **la mejor calidad de audio que permite el FM-1**. Esta página cuenta qué se cambió para llegar ahí en la 0.4.0-beta, cuánto cuesta cada cosa en el aparato y, con la misma franqueza, **lo que todavía no está a la altura**.

Todas las cifras de CPU se midieron en el FM-1 del autor con la consola del firmware (`tools/fm1/traza.py`). Los detalles de cada medición están en [`TRAZA_FM1.md`](TRAZA_FM1.md) y [`MOTORES.md`](MOTORES.md).

## El aparato

| | |
|---|---|
| Procesador | JieLi AC791N, dos núcleos a 360 MHz con FPU de precisión simple |
| Salida | codec externo por I2S, **estéreo de 24 bits a 44 117.6 Hz** (24 MHz / 544, fijo por hardware) |
| Memoria para sonidos grabados | 116 KiB de flash para las tomas del sampler |

## Lo que mejoró

### 1. Todo a 44.1 kHz

En la 0.3 los motores calculaban a 22 kHz y cada muestra se repetía dos veces para llegar a la salida. Eso corta todo arriba de 11 kHz y deja imágenes (agudos falsos) por encima.

Ahora **los motores y la mezcla calculan a la frecuencia real del codec**, 44 117.6 Hz, sin sobremuestreo. Los agudos llegan hasta 20 kHz y la afinación es exacta, porque se calcula con la frecuencia verdadera y no con 44 100.

### 2. Mezcla en coma flotante

| | 0.3 | 0.4 |
|---|---|---|
| Bus de mezcla | entero, con una división por muestra y canal | **float**, ganancias calculadas una vez por bloque |
| Canales en silencio | se calculaban igual | se saltan |
| Captura de los efectos en vivo (stutter, reversa, tape stop) | μ-law de 8 bits | **16 bits** |
| Delay | μ-law de 8 bits | **16 bits** con lectura interpolada |
| Costo de la mezcla con PLAY | 8.6 % de un núcleo | **7.3 %**, calculando el doble de muestras |

Medido en el aparato, la FPU es **1.5× más rápida** que la aritmética entera de 64 bits que usaba la 0.3, así que pasar a float fue más rápido y además más limpio.

### 3. Sonidos calculados en vivo, sin grabaciones de baja resolución

La 0.3 tenía kits de batería grabados a 22 kHz en μ-law de 8 bits (nada arriba de 11 kHz) y tablas de onda de baja resolución. **Se fueron todos.** Ahora cada sonido se calcula en el momento con bibliotecas de síntesis probadas:

| Motor | Qué usa | Licencia |
|---|---|---|
| **Batería modelada** | modelos de bombo, caja y hi-hat de **Plaits** (Mutable Instruments), en float, más palmas, cencerro, resonador, ruido y percusión FM propios | MIT |
| **ANALOG** | osciladores con **polyBLEP** (anti-aliasing), filtro LP 24/12, BP o HP con resonancia y drive, ADSR, LFO, sync y ring | propio y MIT (Plaits/stmlib) |
| **FM de 6 operadores** | **msfa**, el motor de Dexed y la misma base del firmware de fábrica; los 32 algoritmos de la DX7 | Apache-2.0 |
| **ACID** | filtro escalera de 4 polos con resonancia, acento y slide | propio |

El acento no solo sube el volumen: cambia el golpe, como en una caja de ritmos real.

### 4. Una salida sin clics ni escalones

- **Clipper suave** con DRIVE (un polinomio, no un corte duro) y un **techo de −1 dBFS** antes del codec.
- El **volumen MASTER cambia en rampa** dentro de cada bloque: girar la perilla no hace escalones.
- Cuando el firmware escribe la flash el audio tiene que parar: entra y sale con un **fundido de 5 ms**, sin clic.
- La **grabación del sampler pasa por un filtro Butterworth** antes de bajar a 16 kHz, para que no se doblen agudos al grabar.
- La interpolación sin interpolar (DROP) pasó a **LINEAL**, y el sampler también tiene **Hermite**.

### 5. El tiempo, exacto

El secuenciador cuenta a **96 PPQN desde el contador de muestras del audio**: no hay un reloj aparte que pueda derivar, y cada nota cae en su muestra exacta dentro del bloque.

### 6. Rendimiento: lo que hace lugar a la calidad

Calcular el doble de muestras con motores mejores solo cabe si el código rinde. Lo que se midió y cambió:

| Cambio | Antes | Después |
|---|---|---|
| Caché de datos (configuración del SDK) | 4 KiB efectivos | **32 KiB**: lecturas al azar 6-10× más rápidas |
| Audio compilado para velocidad (`-O2`) | demo con PLAY: 36.9 % de un núcleo | **27.7 %** |
| FM, una voz (E.PIANO) | 6.6 % | **3.3 %** |
| ANALOG, una voz | 1757 ns por muestra (10 %) | **654 ns (4.1 %)**: 7 voces caben sin bloques tarde |

El programa quedó 148 KiB más chico y se liberaron 64 KiB de RAM al sacar los kits grabados y las tablas de onda.

## Lo que todavía no está a la altura

Esto es una beta. Lo que hoy limita la calidad, sin adornos:

- **El sampler graba a 16 kHz y 8 bits.** La flash del FM-1 deja 116 KiB para tomas (7.4 s en total); a 16 bits serían la mitad. Con el filtro antes de grabar y el dither de `toma.py` suena limpio, pero **no pasa de unos 7 kHz** y tiene el ruido de 8 bits. Es el punto más débil y el próximo a resolver.
- **El delay trabaja a 22 kHz:** las repeticiones llegan hasta 11 kHz, como un delay de cinta. Es a propósito, para que quepa 1.5 s en la RAM, pero no es alta fidelidad.
- **El clipper no tiene sobremuestreo:** con el DRIVE muy alto puede generar algo de aliasing.
- **ACID es un motor propio, no Open303.** El plan es pasar a Open303 (MIT) en float.
- **El FM de msfa es de punto fijo**, como la DX7 original. Es parte de su carácter, no un defecto, pero no es float.
- **No hay reverb, chorus ni compresor en el master todavía.** Solo el clipper suave.
- **El audio usa un solo núcleo.** El segundo rinde 1.83× medido; usarlo es lo que permitirá más voces y efectos.
- **No medimos la salida analógica** (ruido, distorsión y respuesta del codec y del amplificador del FM-1). Todo lo de arriba es la parte digital.
- **Probado en un solo FM-1.**

## Lo que sigue

1. Audio en los **dos núcleos**.
2. **Efectos de master**: reverb y chorus de Mutable Instruments, delay de cinta, compresor y nivelador.
3. **ACID con Open303** en float.
4. Un **sampler de más calidad**, dentro de lo que deja la flash.
5. **Medir la salida analógica** con una interfaz de audio.

Ver también: [motores de sonido](MOTORES.md) · [investigación de sonido](INVESTIGACION_SONIDO.md) · [mediciones en el FM-1](TRAZA_FM1.md) · [registro de cambios](../CHANGELOG.md).
