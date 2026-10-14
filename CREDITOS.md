# Créditos y terceros

## Autoría

Bubba Machine es un proyecto nuevo de [Erbubar23](https://github.com/Erbubar23), publicado bajo [GPL-3.0-or-later](LICENSE). El código propio, las fuentes de pantalla y el logo son originales del proyecto; las bibliotecas y los presets de terceros están abajo, con su licencia.

## Conocimiento del hardware

- **Felucca** ([hugelton/Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only), de Leo Kuroshita y Hügelton Instruments. Su ingeniería inversa pública documentó el hardware del FM-1. Bubba Machine **no usa su código**: solo hechos de hardware (pines, tiempos, mapa de memoria), citados con su fuente en [`docs/SPEC_V3.md`](docs/SPEC_V3.md) §11.
- **SDK de JieLi AC79** (Apache-2.0): referencia de registros para el port. Cuando el port use binarios del SDK (cargador, configuración), se distribuirán con su licencia Apache-2.0, que es compatible con la GPL-3.0.

## Bibliotecas de síntesis (en `third_party/`)

| Biblioteca | Para qué | Licencia |
|---|---|---|
| **msfa** («music synthesizer for android», de Google, mantenido por [Dexed](https://codeberg.org/dcoredump/Synth_Dexed)) | El motor de 6 operadores de BX-7 | Apache-2.0 |
| **Plaits** ([Mutable Instruments](https://github.com/pichenettes/eurorack), de Emilie Gillet) | Bombos, cajas y hats de BR-8, el oscilador de forma variable, el seno y el overdrive | MIT |
| **stmlib** (Mutable Instruments) | Filtro de estado variable, polyBLEP, interpolador | MIT |

Los detalles (commits, cambios) están en [`third_party/README.md`](third_party/README.md). La reverb sigue el algoritmo publicado por Jon Dattorro (1997); la implementación es propia.

## Sonidos de fábrica

Los sonidos de BR-8, BX-7 y BUNO-6 son originales del proyecto (los 12 de BUNO-6 se hicieron en Vital). Los 25 de B-MINI y BITTAL salen de **presets de Vital de la comunidad** con licencia libre: se bajan en un commit fijo, se convierten al formato de Bubba Machine (`tools/gen_snaps.mjs`, `assets/presets/banco.json`) y llevan un nombre nuestro. El crédito y la licencia son de sus autores:

| Sonido | Máquina | Preset original | Autor | Licencia |
|---|---|---|---|---|
| SUB ONE | B-MINI | Sub 1 | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| SUB GROWL | B-MINI | Sub 56 | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| CURIOUS | B-MINI | Curious Bass | [Abstraction Mage](https://github.com/abstractionmage/Vital-Presets) | CC0-1.0 |
| 808 BASS | B-MINI | bad 808 | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| HEAVY LOW | B-MINI | low part of heavy bass | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| REESE | B-MINI | reese bass | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| SAW MOTION | BITTAL | Saw Movement 45 | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| WIDE SYNTH | BITTAL | i have an itch | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| SIREN | BITTAL | Sirenengesang | [Abstraction Mage](https://github.com/abstractionmage/Vital-Presets) | CC0-1.0 |
| COOL LEAD | BITTAL | cool synth | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| HARD LEAD | BITTAL | hard lead | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| NICE LEAD | BITTAL | nice lead | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| DIGI PIANO | BITTAL | PIANO | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| SOFT KEYS | BITTAL | pinaon 12 | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| CHORD STAB | BITTAL | Chord 3 | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| BACK KEYS | BITTAL | cute background keys | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| GLASS KEYS | BITTAL | keys per P | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| CHILL CHORD | BITTAL | smooth chill chords | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| MOVE ARP | BITTAL | move arp | [nahush2321](https://github.com/nahush2321/Vitalium-presets) | GPL-3.0 |
| SPACE PLUCK | BITTAL | super space pluck | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| ARP LINE | BITTAL | arp | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| BELL | BITTAL | bishu's better bell | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| WOOD PLUCK | BITTAL | wood pluck | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| SPACY PLUCK | BITTAL | spacy plucks | [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets) | Unlicense |
| LOW PAD | BITTAL | Low-mid Simplicity | [Abstraction Mage](https://github.com/abstractionmage/Vital-Presets) | CC0-1.0 |

Los repositorios:

- [nahush2321](https://github.com/nahush2321/Vitalium-presets), commit `025728f9a630` — GPL-3.0
- [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets), commit `c6e8bc13e474` — Unlicense
- [Abstraction Mage](https://github.com/abstractionmage/Vital-Presets), commit `023763ab9b39` — CC0-1.0

Los .bsnap convertidos de un preset GPL-3.0 siguen siendo GPL-3.0, igual que el firmware. **Vital** es un sintetizador de Matt Tytel; Bubba Machine lee su formato de presets pero no tiene relación con Vital ni con su autor, y no usa su código.

## Lo que viaja con el firmware o el emulador

| Componente | Dónde | Licencia |
|---|---|---|
| Runtime de Emscripten | dentro de `web/bm.js` | MIT / University of Illinois |
| Ondas AKWF (opcional, solo si compilas con `--akwf`) | `core/gen/bm_rom.c` | CC0 |

## Herramientas

Python, NumPy, Pillow, Docker, GCC y Emscripten se usan para compilar y generar los datos, pero no se distribuyen con el firmware.

## Marcas

M-VAVE y FM-1 son marcas de sus respectivos dueños. Bubba Machine no está afiliado a M-VAVE ni respaldado por ellos. Los nombres se usan solo para indicar con qué aparato es compatible.

Los nombres de las máquinas (BR-8, BP-1, BB-303, B-MINI, BX-7, BITTAL, BUNO-6, BX-1) recuerdan a la TR-8, el OP-1, la TB-303, el Minimoog, la DX7, Vital y el Juno-6, que son marcas de sus dueños (Roland, Teenage Engineering, Moog, Yamaha, Vital Audio). Bubba Machine no tiene relación con ellos y no copia su código ni sus sonidos.

## Sin garantía

Como dice la [licencia](LICENSE) (secciones 15 y 16), este software se entrega **sin garantía**. Instalar firmware no oficial puede dejar el aparato inservible. Hazlo bajo tu propio riesgo, con un respaldo y solo con una versión que diga que está probada en hardware.
