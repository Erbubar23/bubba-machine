# Motores de sonido de Bubba Machine 0.4

Octubre de 2026. Plan para pasar de sonidos lo-fi a sintetizadores reales, con la variedad de JIANT FM y el directo de X0X, sobre la arquitectura medida en `docs/TRAZA_FM1.md`. Regla: bibliotecas de síntesis probadas y, donde existe, la versión hecha para microcontroladores. No se copian los proyectos base de otros firmwares.

## Qué hacen los firmwares de referencia

| | JIANT FM | X0X |
|---|---|---|
| Base | fork de Felucca (GPL-3.0-only) con el song mode de SLOOP | propio, sin SDK |
| Motores | ANALOG (2 osc, SAW/SQR/TRI/SIN/PWM, SYNC, RING, 3 sierras, filtro por voz con drive) · **FM6** (msfa de Dexed, edita en el aparato, importa .syx, 4 voces por pista) · PHASE (distorsión de fase) · LOFI (bytebeat) · VOICE (formantes, klattsch) · WHEEL (órgano) · NOISE · **DRUM-X** (8 sonidos A/B con MORPH, FOLD, FM, estilo Microtonic) | **909** y **808** modelados (hats y platillos grabados) · dos **303** (Open303) con generador de líneas acid · **slicer** de breaks |
| Filtros | por motor: LP, HP, BP, COMB | — |
| Modulación | LFO, ENV LOOP, secuenciador de modulación de 16 pasos, matriz, macros, dados | grabar movimientos de perillas |
| Efectos | DIST y AUTOPAN por pista; delay con grano; 5 reverbs (ROOM, SPRING, SHIMMER, RESO, CLOUD); chorus; master con CLIP, PUNCH, DUCK y nivelador | reverb, delay de cinta y compresor en el master |

Fuentes: [JIANT](https://github.com/juanjiant-bit/JIANT), [MATRIXSYNTH sobre JIANT](https://www.matrixsynth.com/2026/10/jiant-fm-living-bio-synthetic-firmware.html), [X0X](https://github.com/charlesvestal/fm1-x0x), [lista de firmwares del FM-1](https://github.com/cicloid/awesome-fm-1).

## Bibliotecas elegidas

| Para | Biblioteca | Por qué esa | Licencia |
|---|---|---|---|
| FM de 6 operadores | **msfa** de Synth_Dexed | la misma base que el firmware de fábrica, Dexed, MiniDexed y JIANT; entera, sin dependencias, compatible con bancos DX7 | Apache-2.0 |
| Batería modelada (808, 909, sintética) | **Plaits** (Mutable Instruments): `analog_bass_drum`, `analog_snare_drum`, `hihat`, `synthetic_*` | escrita para Cortex-M4 a 48 kHz, en float, sin tablas grandes | MIT |
| Analógico virtual | osciladores polyBLEP de **Plaits/stmlib** + filtro TPT propio (SVF y escalera) | los osciladores con menos aliasing por ciclo de CPU | MIT |
| Cuerdas y modal | **Plaits** (string, modal; versiones livianas de Rings) | modelado físico que ya corre en un M4 | MIT |
| 303 | **Open303** pasado a float simple | el que usa X0X en este mismo chip; el original es en double | MIT |
| Reverb y chorus | **Clouds/Rings** `FxEngine` (reverb de Griesinger, ensemble) | buffers de 16 bits y unas pocas operaciones por muestra | MIT |
| Master | compresor y limitador propios (o Airwindows si se mide liviano) | | MIT |

## Lo hecho: motor FM (paso 1)

- `core/bm_fm.cpp`: envoltorio de msfa sin memoria dinámica ni constructores globales; una nota, un LFO y un buffer de 64 por voz (msfa calcula de a 64 y Bubba pide tramos más cortos).
- 16 patches propios (`tools/gen_fm.py`): E.PIANO, BAJO FM, SUB BAJO, CAMPANA, METALES, CUERDAS, ORGANO, CLAVI, MARIMBA, PULSADO, SOLISTA, VIDRIO, SUPER FM, TOM FM, FLAUTA, ZUMBIDO. No son los bancos de Yamaha.
- Macros: BRILLO (nivel de los moduladores), REALIM (realimentación), DECAY (todas las envolventes), ALGORITMO (en el centro, el del patch; fuera, cualquiera de los 32). ATAQUE y SALIDA del canal alargan las envolventes. P-Locks por paso como los demás motores.
- Canales: T3 (mono, con glide si liga) y T4-T6 (polifónico, pool de 7 voces).
- Pantalla: los 6 operadores como barras de nivel, portadoras en color, relación de frecuencia de cada uno, algoritmo y realimentación.

| Medido en el FM-1 (E.PIANO) | |
|---|---|
| Al principio (todo `-Oz` con LTO, como el SDK) | 6.6 % de un núcleo por voz |
| msfa con `-O2` sin LTO | 4.3 % |
| y el núcleo de msfa sin multiplicaciones de 64 bits | 4.2 % (casi nada: no eran el cuello de botella) |
| y el camino directo sin el micro-fade por muestra | **3.3 %** por voz |
| Un operador, medido aparte | 90–107 ns por muestra (~35 ciclos) |
| La voz entera de msfa, medida aparte | 654 ns por muestra = 2.9 % |

Compilar el audio del núcleo (`bm_audio.c`, `bm_dsp.c`, `bm_engines.c`, `bm_seq.c`) con `-O2` bajó la demo con PLAY de 36.9 % a **27.7 %** de un núcleo.

## Lo que sigue

1. **Más espacio**: las wavetables ya se arman al arrancar desde su espectro (−56 KiB de programa). Quedan 47 KiB libres; los kits KO-808/909 (172 KiB) y los loops de fábrica (53 KiB) pasarán a la zona de datos de la flash como banco de recursos, y los kits se regenerarán a 44.1 kHz y 16 bits.
2. **Audio en dos núcleos**: repartir las voces (1.83× medido). Con eso caben ~25 voces de FM además del resto.
3. **Batería modelada** (Plaits): un kit 808/909 calculado, con morph A/B como DRUM-X.
4. **ANALOG 2.0**: osciladores polyBLEP con SYNC, RING, PWM, 3 sierras; filtro por voz LP/HP/BP con drive.
5. **ACID 2.0** (Open303 en float) con un generador de líneas propio.
6. **Efectos de master**: reverb y chorus de Mutable, delay de cinta, compresor y nivelador.
7. **Editor de FM y bancos .syx** desde el editor web.
