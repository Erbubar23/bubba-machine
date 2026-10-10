# Credits and third parties

## Authorship

Bubba Machine is a new project by [Erbubar23](https://github.com/Erbubar23), released under [GPL-3.0-or-later](../../LICENSE). All code, factory sounds, screen fonts and the logo are original to the project.

## Hardware knowledge

- **Felucca** ([hugelton/Felucca](https://github.com/hugelton/Felucca), GPL-3.0-only), by Leo Kuroshita and Hügelton Instruments. Its public reverse engineering documented the FM-1 hardware. Bubba Machine **does not use its code**: only hardware facts (pins, timings, memory map), cited with their source in `docs/SPEC_V3.md` §11.
- **JieLi AC79 SDK** (Apache-2.0): register reference for the port. When the port uses SDK binaries (bootloader, configuration), they will be distributed under their Apache-2.0 license, which is compatible with GPL-3.0.

## What ships with the firmware or the emulator

| Component | Where | License |
|---|---|---|
| Emscripten runtime | inside `web/bm.js` | MIT / University of Illinois |
| AKWF waves (optional, only if you build with `--akwf`) | `core/gen/bm_rom.c` | CC0 |

## Tools

Python, NumPy, Pillow, Docker, GCC and Emscripten are used to build and generate data, but they are not distributed with the firmware.

## Trademarks

M-VAVE and FM-1 are trademarks of their respective owners. Bubba Machine is not affiliated with or endorsed by M-VAVE. The names are used only to state which device it is compatible with.

## No warranty

As the [license](../../LICENSE) says (sections 15 and 16), this software comes **with no warranty**. Installing unofficial firmware can brick the device. Do it at your own risk, with a backup, and only with a version that says it is tested on hardware.
