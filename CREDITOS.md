# Créditos y terceros

## Autoría

Bubba Machine es un proyecto nuevo de [Erbubar23](https://github.com/Erbubar23), publicado bajo [GPL-3.0-or-later](LICENSE). Todo el código, los sonidos de fábrica, las fuentes de pantalla y el logo son originales del proyecto.

## Conocimiento del hardware

- **Felucca** ([hugelton/Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only), de Leo Kuroshita y Hügelton Instruments. Su ingeniería inversa pública documentó el hardware del FM-1. Bubba Machine **no usa su código**: solo hechos de hardware (pines, tiempos, mapa de memoria), citados con su fuente en [`docs/SPEC_V3.md`](docs/SPEC_V3.md) §11.
- **SDK de JieLi AC79** (Apache-2.0): referencia de registros para el port. Cuando el port use binarios del SDK (cargador, configuración), se distribuirán con su licencia Apache-2.0, que es compatible con la GPL-3.0.

## Lo que viaja con el firmware o el emulador

| Componente | Dónde | Licencia |
|---|---|---|
| Runtime de Emscripten | dentro de `web/bm.js` | MIT / University of Illinois |
| Ondas AKWF (opcional, solo si compilas con `--akwf`) | `core/gen/bm_rom.c` | CC0 |

## Herramientas

Python, NumPy, Pillow, Docker, GCC y Emscripten se usan para compilar y generar los datos, pero no se distribuyen con el firmware.

## Marcas

M-VAVE y FM-1 son marcas de sus respectivos dueños. Bubba Machine no está afiliado a M-VAVE ni respaldado por ellos. Los nombres se usan solo para indicar con qué aparato es compatible.

## Sin garantía

Como dice la [licencia](LICENSE) (secciones 15 y 16), este software se entrega **sin garantía**. Instalar firmware no oficial puede dejar el aparato inservible. Hazlo bajo tu propio riesgo, con un respaldo y solo con una versión que diga que está probada en hardware.
