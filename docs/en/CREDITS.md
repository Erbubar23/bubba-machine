# Credits and third parties

## Authorship

Bubba Machine is a new project by [Erbubar23](https://github.com/Erbubar23), released under [GPL-3.0-or-later](../../LICENSE). The project's own code, screen fonts and logo are original; third-party libraries and presets are listed below with their license.

## Hardware knowledge

- **Felucca** ([hugelton/Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only), by Leo Kuroshita and Hügelton Instruments. Its public reverse engineering documented the FM-1 hardware. Bubba Machine **does not use its code**: only hardware facts (pins, timings, memory map), cited with their source in `docs/SPEC_V3.md` §11.
- **JieLi AC79 SDK** (Apache-2.0): register reference for the port. When the port uses SDK binaries (bootloader, configuration), they will be distributed under their Apache-2.0 license, which is compatible with GPL-3.0.

## Synthesis libraries (in `third_party/`)

| Library | What for | License |
|---|---|---|
| **msfa** ("music synthesizer for android", by Google, maintained by [Dexed](https://codeberg.org/dcoredump/Synth_Dexed)) | BX-7's 6-operator engine | Apache-2.0 |
| **Plaits** ([Mutable Instruments](https://github.com/pichenettes/eurorack), by Emilie Gillet) | BR-8's kicks, snares and hats, the variable-shape oscillator, the sine and the overdrive | MIT |
| **stmlib** (Mutable Instruments) | State-variable filter, polyBLEP, interpolator | MIT |

Details (commits, changes) are in [`third_party/README.md`](../../third_party/README.md). The reverb follows the algorithm published by Jon Dattorro (1997); the implementation is original.

## Factory sounds

BR-8, BX-7 and BUNO-6 sounds are original to the project (BUNO-6's 12 were made in Vital). The 25 B-MINI and BITTAL sounds come from **community Vital presets** under free licenses: they are downloaded at a pinned commit, converted to the Bubba Machine format (`tools/gen_snaps.mjs`, `assets/presets/banco.json`) and given our own name. Credit and license belong to their authors:

| Sound | Machine | Original preset | Author | License |
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

The repositories:

- [nahush2321](https://github.com/nahush2321/Vitalium-presets), commit `025728f9a630` — GPL-3.0
- [Mxm40b (mxmfrpr)](https://github.com/Mxm40b/vital-presets), commit `c6e8bc13e474` — Unlicense
- [Abstraction Mage](https://github.com/abstractionmage/Vital-Presets), commit `023763ab9b39` — CC0-1.0

.bsnap files converted from a GPL-3.0 preset stay GPL-3.0, like the firmware. **Vital** is a synthesizer by Matt Tytel; Bubba Machine reads its preset format but has no relation to Vital or its author, and does not use its code.

## What ships with the firmware or the emulator

| Component | Where | License |
|---|---|---|
| Emscripten runtime | inside `web/bm.js` | MIT / University of Illinois |
| AKWF waves (optional, only if you build with `--akwf`) | `core/gen/bm_rom.c` | CC0 |

## Tools

Python, NumPy, Pillow, Docker, GCC and Emscripten are used to build and generate data, but they are not distributed with the firmware.

## Trademarks

M-VAVE and FM-1 are trademarks of their respective owners. Bubba Machine is not affiliated with or endorsed by M-VAVE. The names are used only to state which device it is compatible with.

The machine names (BR-8, BP-1, BB-303, B-MINI, BX-7, BITTAL, BUNO-6, BX-1) recall the TR-8, OP-1, TB-303, Minimoog, DX7, Vital and Juno-6, which are trademarks of their owners (Roland, Teenage Engineering, Moog, Yamaha, Vital Audio). Bubba Machine has no relation to them and does not copy their code or sounds.

## No warranty

As the [license](../../LICENSE) says (sections 15 and 16), this software comes **with no warranty**. Installing unofficial firmware can brick the device. Do it at your own risk, with a backup, and only with a version that says it is tested on hardware.
