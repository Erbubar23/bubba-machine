<p align="center"><img src="assets/logo/bubba-machine-logo.png" alt="Bubba Machine" width="460"></p>

<p align="center"><b>Firmware libre de groovebox de 6 máquinas para el M-VAVE FM-1.</b><br>
Batería, sampler de pads, bajo, FM, wavetable y polifónico. Escrito desde cero en C99. Licencia GPL-3.0.</p>

<p align="center">
<a href="https://erbubar23.github.io/bubba-machine/">Sitio web</a> ·
<a href="https://erbubar23.github.io/bubba-machine/emulador.html">Probar en el navegador</a> ·
<a href="docs/MANUAL.md">Manual</a> ·
<a href="CHANGELOG.md">Cambios</a> ·
<a href="https://erbubar23.github.io/bubba-machine/en/">English</a>
</p>

<p align="center">
<img src="assets/screens/00-home.png" width="200" alt="HOME: el mezclador BX-1 con las seis máquinas">
<img src="assets/screens/02-seq-tr.png" width="200" alt="BR-8: la batería TR">
<img src="assets/screens/57-bittal-table.png" width="200" alt="BITTAL: la tabla del oscilador en 2.5D">
<img src="assets/screens/37-pads.png" width="200" alt="BP-1: la onda con el pad entre corchetes">
</p>

---

> **Estado: 0.9.0-beta.** Corre en el aparato y se instala desde el navegador, plug and play, pero es una **beta**: se probó en un solo FM-1 y todavía cambia mucho (la 0.9.0-beta, todavía sin probar del todo en el aparato). **Una actualización puede borrar lo que guardaste** (patrones, canción, ajustes y tomas): hasta la v1 no hay compatibilidad hacia atrás. Ver [cómo instalar](docs/INSTALAR.md) y [cómo volver al firmware de fábrica](docs/INSTALAR.md#volver-al-firmware-de-fábrica).

## Qué es

Bubba Machine convierte el FM-1 en una groovebox de directo con **seis máquinas fijas**, **cuatro gestos por botón y HOME en todas partes**:

- **T1 BR-8, batería estilo TR:** 16 instrumentos con su propia línea y su propio largo (polímetro), acento, repetición, flam, P-Locks y pasos que solo suenan en el fill. Roll y borrado estilo TR mientras suena.
- **T2 BP-1, sampler de pads:** 16 pads hechos con tus tomas, como un KO II o un OP-1 field. Cada pad tiene su inicio, su largo, su tono y su modo de tecla (sostenido puede seguir por la toma). Una rejilla como la de la batería los toca y BREAK la varía compás a compás. Grabadora propia y biblioteca de tomas.
- **Pantalla en inglés**, para que se entienda en todo el mundo, con el español como paquete de idioma que se carga aparte ([cómo](docs/MANUAL.md#idioma-de-la-pantalla)).
- **T3 BB-303 o B-MINI** (bajo ácido o tres osciladores con filtro escalera), **T4 BX-7** (FM de 6 operadores, lee bancos .syx de la DX7), **T5 BITTAL** (wavetable que carga presets de Vital) y **T6 BUNO-6** (polifónico con chorus). Banco de 37 sonidos y un [editor web](https://erbubar23.github.io/bubba-machine/sonidos.html) para sumar los tuyos. Arpegiador, loopera, escala propia y MIDI OUT por canal.
- **HOME es el mezclador BX-1:** volumen, envío a la reverb, filtro del master y BEAT FX a mano.
- **REC expresivo:** cada nota guarda su micro-tiempo, su largo y lo que moviste mientras la tocabas.
- **Tiempo por pista:** cada pista tiene su largo (1-64) y su velocidad (×2 a ×1/4).
- **Acordes en cuadrícula:** 16 acordes con su grado y su función por color.
- **Directo:** 4 escenas con cadenas y grabación de la canción, fills automáticos, 16 efectos punch-in y mutes por pista e instrumento.
- **Guardado seguro** en flash (CRC, commit al final, wear-leveling y autoguardado al parar), MIDI IN, reloj externo y Split Sync.

## Pruébalo

Abre el [emulador web](https://erbubar23.github.io/bubba-machine/emulador.html) en Chrome o Edge y pulsa *Encender*. Arranca con una demo; la barra espaciadora es PLAY. También funciona sin internet: abre `web/emulador.html` desde tu copia del repo.

## Documentación

| | |
|---|---|
| [Manual](docs/MANUAL.md) | Cómo se toca, botón por botón |
| [Instalar](docs/INSTALAR.md) | El instalador web, plug and play, y cómo volver al de fábrica |
| [Control](https://erbubar23.github.io/bubba-machine/control.html) | Con el FM-1 conectado: memoria, tomas, sonidos de Vital y DX7, idioma |
| [Desarrollo](docs/DESARROLLO.md) | Compilar, probar, generar los sonidos y el sitio |
| [Hoja de ruta](docs/HOJA_DE_RUTA.md) | Todo lo contemplado, de audio y de interfaz |
| [Diseño 0.6](docs/DISENO_0.6.md) | Las seis máquinas, la cara nueva y el banco de sonidos |
| [Diseño 3.0](docs/DISENO_3.0.md) | Por qué la interfaz es así: cuatro gestos por botón y HOME en todas partes |
| [Audio](docs/AUDIO.md) | Calidad de audio: qué se mejoró, cuánto cuesta en el FM-1 y qué falta |
| [Especificación](docs/SPEC_V3.md) | Hardware, memoria, audio y decisiones técnicas |
| [Cambios](CHANGELOG.md) | Qué trae cada versión |
| [Contribuir](CONTRIBUTING.md) | Cómo ayudar y qué reglas seguimos |
| [Créditos](CREDITOS.md) | Agradecimientos, terceros y marcas |

## Licencia

Bubba Machine es **software libre** bajo la [GNU General Public License v3.0 o posterior](LICENSE) (`GPL-3.0-or-later`). Puedes usarlo, estudiarlo, modificarlo y compartirlo; si distribuyes una versión modificada, tiene que seguir siendo libre y con su código fuente.

- **Todo el código es propio**, escrito desde cero. De la ingeniería inversa pública del FM-1 solo se usaron hechos de hardware (pines, tiempos, mapa de memoria). Ver [créditos](CREDITOS.md).
- **Los sonidos se calculan en vivo** (Plaits, msfa y modelos propios); el firmware no lleva audio grabado de terceros. Los sonidos de fábrica de B-MINI y BITTAL salen de presets de Vital de la comunidad con licencia libre (CC0, Unlicense y GPL-3.0), con su crédito en [CREDITOS.md](CREDITOS.md).

M-VAVE y FM-1 son marcas de sus respectivos dueños. Este proyecto no está afiliado a M-VAVE ni respaldado por ellos. Instalar firmware no oficial puede dejar tu aparato inservible: hazlo bajo tu propio riesgo y solo cuando exista una versión probada.

---

## In English

**Bubba Machine** is a free (GPL-3.0-or-later) groovebox firmware for the M-VAVE FM-1 with six fixed machines: BR-8 modeled drums, BP-1 pad sampler, BB-303 / B-MINI bass, BX-7 FM (reads DX7 .syx banks), BITTAL wavetable (loads Vital presets) and BUNO-6 polysynth with chorus, plus the BX-1 console on HOME. Written from scratch in C99.

- **Status:** 0.9.0-beta. It runs on the device and installs from the browser, plug and play, but it is a **beta**: tested on a single FM-1 and still changing a lot (0.9.0-beta, not fully tried on the device yet). **An update may erase what you saved** (patterns, song, settings and takes): there is no backward compatibility until v1. You can also try it in the [web emulator](https://erbubar23.github.io/bubba-machine/emulador.html).
- **Website and docs in English:** [erbubar23.github.io/bubba-machine/en](https://erbubar23.github.io/bubba-machine/en/), with the [manual](docs/en/MANUAL.md), [install status](docs/en/INSTALL.md), [audio quality](docs/en/AUDIO.md) and [roadmap](docs/en/ROADMAP.md). The firmware screens are in English; Spanish is an optional language pack. Issues and pull requests in English are welcome.
- **Clean room:** all project code is original; synthesis libraries (Plaits, msfa, Open303) and the community Vital presets in the factory bank keep their free licenses ([credits](docs/en/CREDITS.md)). Only hardware facts from public FM-1 reverse engineering were used.
- M-VAVE and FM-1 are trademarks of their respective owners. This project is not affiliated with or endorsed by M-VAVE.
