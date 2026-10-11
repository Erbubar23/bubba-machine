# Sonido y arquitectura: investigación para Bubba Machine 0.4

Octubre de 2026. Qué motores de síntesis y qué arquitectura podemos usar en el FM-1 para pasar de un groovebox lo-fi a uno de **máxima calidad de audio**, con lo que ya está probado por la comunidad y con licencias compatibles con la nuestra (GPL-3.0-or-later).

## El aparato, medido

| | Dato | Fuente |
|---|---|---|
| Procesador | JieLi AC791N (WL82), **dos núcleos pi32v2 a 360 MHz**, FPU de precisión simple | consola de Bubba (`clk_get("sys")`); Melodee usa la FPU |
| RAM | ~511 KiB; Bubba usa ~140 KiB | mapa del enlazador |
| Programa | 568 KiB; Bubba 0.3 usa 564 KiB (quedan 17 KiB) | `tools/build.py fm1` |
| Datos | 0x97000–0xE4FFF (~312 KiB): patrones, tomas, ajustes | `hal/fm1/fm1_hw.h` |
| Audio | codec por I2S a 44 117.6 Hz, 24 bits | Felucca; medido: 171 bloques/s de 256 frames |
| Carga de Bubba 0.3 | 10 % del tiempo de un núcleo en reposo, 20–29 % con PLAY (motores a 22 kHz) | consola |

## Qué ocupa el programa hoy

| Parte | Tamaño |
|---|---|
| Kits de batería acústicos (KO-808, KO-909, samples) | ~170 KiB |
| Tablas de ondas lo-fi | 64 KiB |
| Loops de fábrica del sampler (8 bit, 16 kHz) | 54 KiB |
| Interfaz (entrada + dibujo) | ~102 KiB |
| Logo | 18 KiB |
| Motores de sonido | **11 KiB** |
| SDK de JieLi (sistema, USB, flash…) | ~100 KiB |

Los motores son lo más chico. Sacando las tablas lo-fi y los loops quedan ~118 KiB para motores nuevos; los samples acústicos se conservan.

## Motores candidatos (licencia verificada en el código original)

| Biblioteca | Licencia | Qué aporta | Quién la usa en el FM-1 |
|---|---|---|---|
| **Plaits** (Mutable Instruments) | MIT (proyectos STM32 del repositorio `pichenettes/eurorack`) | 16+ modelos: analógico virtual, waveshaping, FM de 2 operadores, wavetable, acordes, cuerdas y modal, bombo/caja/hi-hat analógicos | Lunar Modulator, Hortator |
| **Rings / Clouds** (Mutable) | MIT | modelado físico (cuerdas, resonador), granular y reverbs | Lunar Modulator |
| **msfa** (Google, base de Dexed) | Apache-2.0 | FM de 6 operadores, 32 algoritmos, compatible con bancos DX7 por SysEx; entero, liviano | firmware de fábrica, Felucca, sloopDX |
| **Open303** (Robin Schmidt) | MIT | TB-303: filtro de diodos, acento, slide | X0X, Lunar Modulator |
| **DaisySP** (Electrosmith) | MIT (las partes LGPL están en otro repositorio y no se usan) | osciladores con banda limitada, filtros, batería analógica y sintética | muchos proyectos de audio embebido |
| **Airwindows** | MIT | saturación, compresión, limitador | Lunar Modulator |
| Dexed, 9W9/8W8, Felucca, X0X | GPL-3.0 o GPL-3.0-only | — | **no se copian**: solo datos e ideas |

Todos están en C++. El toolchain de JieLi trae `clang++`, y el emulador web y las pruebas de PC también compilan C++; se usarían sin excepciones ni RTTI.

## Arquitectura: lo que aprendimos y lo que proponen otros proyectos

- **Sobre el SDK, como fm1-nes** (Apache-2.0, corre en el FM-1 con el SDK AC79): arranque, RTOS, USB y periféricos del SDK. Es nuestro camino. Las tres trampas que encontramos ya están resueltas: el reloj del USB, la ventana sin cifrar de la flash y que el envío USB no se bloquee (ver `hal/fm1/INVESTIGACION.md`).
- **La comprobación de llave del SDK** (`sdk_meky_check`) es inerte en el FM-1: sin licencia registrada, sale enseguida (Lunar Modulator, PR #56). Lo confirma que el modo seguro corra minutos sin problema.
- **Dos núcleos**: hoy el audio usa uno. Repartir voces entre los dos (dos tareas que calculan mitades y se suman) duplica la capacidad.
- **Probar sin el aparato**: hay un emulador del AC791N que corre los `.fwsc` sin modificarlos (`shcherbakov7/m-wave-fm1-browser-emulator`, GPL-3.0, se usa como herramienta) y nuestro emulador web corre el núcleo.
- **Medir en el aparato**: la consola de Bubba (`tools/fm1/consola.py`) mide el tiempo de cálculo por bloque. Un firmware de banco de pruebas puede medir cada motor por voz antes de decidir cuántas voces caben.

## Plan propuesto

1. **Banco de pruebas en el aparato**: un firmware que corre cada motor candidato y mide µs por voz con la consola. Con eso se decide cuántas voces y qué motores.
2. **Motor de audio nuevo**: 44.1 kHz en float (sin el ZOH lo-fi), bloques cortos, voces repartidas en los dos núcleos.
3. **Motores**: batería sintetizada (Plaits/DaisySP) además de los samples acústicos; TB-303 (Open303) para el bajo; FM de 6 operadores (msfa, con bancos DX7); un motor macro (Plaits) para los canales libres; efectos de calidad (Airwindows, reverbs de Mutable).
4. **Memoria**: samples y tablas grandes como banco de recursos en la zona de datos (cargado por el instalador o el editor web), para liberar el programa.
5. **Sampler**: estilo X0X (toda la grabación a la vista y cortarla) más bancos según la memoria y carga de archivos desde el editor web. Grabar desde la computadora por audio USB (demostrado por Felucca). Micrófono Bluetooth: investigación, sin promesa.

## Fuentes

- Lista de proyectos del FM-1: <https://github.com/cicloid/awesome-fm-1>
- Lunar Modulator (motores Mutable, msfa, licencias por módulo): <https://github.com/ip2k/lunar-modulator> y su PR #56 sobre la llave del SDK
- fm1-nes (SDK AC79, Apache-2.0): <https://github.com/Keitark/fm1-nes>
- Mutable Instruments: <https://github.com/pichenettes/eurorack>
- msfa: <https://github.com/google/music-synthesizer-for-android>
- Open303: <https://github.com/RobinSchmidt/Open303>
- DaisySP: <https://github.com/electro-smith/DaisySP>
- Airwindows: <https://github.com/airwindows/airwindows>
- Emulador del AC791N: <https://github.com/shcherbakov7/m-wave-fm1-browser-emulator>
