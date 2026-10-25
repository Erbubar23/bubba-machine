<p align="center"><img src="assets/logo/bubba-machine-logo.png" alt="Bubba Machine" width="460"></p>

<p align="center"><b>Firmware libre de groovebox hi-fi de 6 máquinas para el M-VAVE FM-1.</b><br>
Batería, sampler de pads, bajo, FM, wavetable y polifónico, calculados en vivo a 44,1 kHz y 24 bits. Escrito desde cero en C99. Licencia GPL-3.0.</p>

<p align="center">
<a href="https://erbubar23.github.io/bubba-machine/">Sitio web</a> ·
<a href="https://erbubar23.github.io/bubba-machine/emulador.html">Probar en el navegador</a> ·
<a href="docs/MANUAL.md">Manual</a> ·
<a href="CHANGELOG.md">Cambios</a> ·
<a href="https://erbubar23.github.io/bubba-machine/en/">English</a>
</p>

<p align="center">
<img src="assets/screens/00-home.png" width="200" alt="HOME: el mezclador de los seis canales">
<img src="assets/screens/02-seq-tr.png" width="200" alt="BR-8: la batería TR">
<img src="assets/screens/81-b303-panel.png" width="200" alt="BB-303: el panel de la TB-303">
<img src="assets/screens/90-slicer-trim.png" width="200" alt="BP-1: el Slicer cortando una toma">
</p>

---

> **Estado: 0.10.0-beta.** Corre en el aparato y se instala desde el navegador, plug and play, pero es una **beta**: se probó en un solo FM-1 y todavía cambia mucho (la 0.10.0-beta corre en el FM-1 del autor). **Una actualización puede borrar lo que guardaste** (patrones, escenas, ajustes y tomas): hasta la v1 no hay compatibilidad hacia atrás. Ver [cómo instalar](docs/INSTALAR.md) y [cómo volver al firmware de fábrica](docs/INSTALAR.md#volver-al-firmware-de-fábrica).

## El objetivo: una máquina hi-fi

Bubba Machine busca **el mejor sonido que puede dar el hardware del FM-1**, sin atajos que se oigan:

- **44,1 kHz y 24 bits de punta a punta:** todas las máquinas y la mezcla corren a la frecuencia real del códec, en coma flotante. Los agudos llegan a 20 kHz.
- **Todo se calcula en vivo:** nada de grabaciones de 8 bits ni tablas de baja resolución.
- **El circuito real antes que un preset:** cada máquina tiene su propio motor modelado sobre la que la inspira (el BB-303 usa el filtro y el acento de la TB-303 de Open303) y sonidos históricos recreados a partir de hojas de patches.
- **Todo el hardware trabajando:** los dos núcleos de 360 MHz (TWO CORES), sidechain para que el kick atraviese la mezcla y los voltajes de fábrica de la placa para que no se apague.
- **Medido, no adivinado:** cada cambio se mide en el aparato y en horas de juego simulado. Ver [la calidad de audio](docs/en/AUDIO.md).

## Qué es

Bubba Machine convierte el FM-1 en una groovebox de directo con **seis canales**, **cuatro gestos por botón y HOME en todas partes**:

- **T1 BR-8, batería estilo TR:** 16 instrumentos, cada uno con su largo (polímetro), con acentos, repeticiones y mutes, en un secuenciador por pasos con acceso rápido a cada instrumento.
- **BP-1 Slicer, el último canal:** corta una toma en 16 pads como un KO II o un OP-1 field (AUTO, 8 o 16 cortes) y escribe la secuencia que suena como el loop. Cada pad tiene su tono, nivel, modo, reversa, ataque, release y loop; una rejilla por pasos los toca. Puede grabar la mezcla en un pad.
- **Pantalla en inglés**, para que se entienda en todo el mundo, con el español como paquete de idioma que se carga aparte ([cómo](docs/MANUAL.md#idioma-de-la-pantalla)).
- **Los demás canales eligen su máquina, y cada una es su instrumento:** al entrar, la máquina toma todo el FM-1 con el panel y los menús de la original. **BB-303** (la TB-303 sobre Open303, con ACCENT y SLIDE por paso y overdrive estilo TB-03), **B-MINI** (Minimoog), **BX-7** (el FM del FM-1: 6 operadores, 32 algoritmos, sus efectos y arpegiador), **BITTAL** (wavetable estilo Vital) y **BUNO-6** (Juno-6 con chorus). Sonidos de fábrica, muchos históricos, y MIDI OUT por canal.
- **HOME es el mezclador:** 9 patrones, los seis canales con volumen, filtro, delay y reverb, mutes, solo y 4 escenas de mutes por patrón (de fábrica la B deja todo muteado).
- **Patrones de fábrica:** recreaciones de *Da Funk* (Daft Punk), *Confusion* en la versión de *Blade* (New Order) y *Enjoy the Silence* (Depeche Mode), tocadas por las máquinas.
- **Grabación como SLOOP:** largo fijo que da la vuelta, cuantización de lo que tocas en vivo, paso a paso dentro de SEQ, y REC sostenido para borrar la secuencia del canal.
- **Tiempo por pista:** cada pista tiene su largo (1-64 pasos) y su velocidad.
- **Guardado seguro** en flash (CRC, commit al final, wear-leveling y autoguardado al parar), MIDI IN, reloj externo y salida de sync.

## Pruébalo

Abre el [emulador web](https://erbubar23.github.io/bubba-machine/emulador.html) en Chrome o Edge y pulsa *Encender*. Arranca con el patrón 1 (*Da Funk*); la barra espaciadora es PLAY. También funciona sin internet: abre `web/emulador.html` desde tu copia del repo.

## Documentación

| | |
|---|---|
| [Manual](docs/MANUAL.md) | Cómo se toca, botón por botón |
| [Instalar](docs/INSTALAR.md) | El instalador web, plug and play, y cómo volver al de fábrica |
| [Control](https://erbubar23.github.io/bubba-machine/control.html) | Tomas, sonidos de Vital y DX7, idioma y memoria: preparas los cambios y los mandas al FM-1 |
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

**Bubba Machine** is a free (GPL-3.0-or-later) **hi-fi** groovebox firmware for the M-VAVE FM-1: BR-8 modeled drums, the BP-1 Slicer, and on the other channels your choice of BB-303 (a TB-303 after Open303), B-MINI (Minimoog), BX-7 (the FM-1's own 6-operator FM), BITTAL wavetable and BUNO-6 (Juno-6) — each one taking over the FM-1 with the panel and menus of the real machine — plus a six-channel mixer on HOME and three factory patterns. Written from scratch in C99.

- **The goal is hi-fi:** the best sound the FM-1's hardware can give. Every machine and the mix run at 44.1 kHz / 24-bit in floating point, everything is computed live (no low-res samples), each machine has its own engine modeled on the real one, and both 360 MHz cores can share the work (TWO CORES). Every change is measured on the device ([audio quality](docs/en/AUDIO.md)).

- **Status:** 0.10.0-beta. It runs on the device and installs from the browser, plug and play, but it is a **beta**: tested on a single FM-1 and still changing a lot (0.10.0-beta runs on the author's FM-1). **An update may erase what you saved** (patterns, scenes, settings and takes): there is no backward compatibility until v1. You can also try it in the [web emulator](https://erbubar23.github.io/bubba-machine/emulador.html).
- **Website and docs in English:** [erbubar23.github.io/bubba-machine/en](https://erbubar23.github.io/bubba-machine/en/), with the [manual](docs/en/MANUAL.md), [install status](docs/en/INSTALL.md), [audio quality](docs/en/AUDIO.md) and [roadmap](docs/en/ROADMAP.md). The firmware screens are in English; Spanish is an optional language pack. Issues and pull requests in English are welcome.
- **Clean room:** all project code is original; synthesis libraries (Plaits, msfa, Open303) and the community Vital presets in the factory bank keep their free licenses ([credits](docs/en/CREDITS.md)). Only hardware facts from public FM-1 reverse engineering were used.
- M-VAVE and FM-1 are trademarks of their respective owners. This project is not affiliated with or endorsed by M-VAVE.
