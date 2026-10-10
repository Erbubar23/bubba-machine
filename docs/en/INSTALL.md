# Installing Bubba Machine

## Current status: it cannot be installed yet

**There is no installer and no binary for the FM-1.** The firmware runs in full in the [web emulator](https://erbubar23.github.io/bubba-machine/emulador.html?lang=en) and in the PC tests, but the hardware port is a skeleton that **has never been built with the JieLi toolchain or tested on an FM-1**. Progress is tracked in [`hal/fm1/README_PORT.md`](../../hal/fm1/README_PORT.md) (Spanish).

Do not try to flash anything from this repository onto your device.

## Meanwhile: the emulator

1. Open the [emulator](https://erbubar23.github.io/bubba-machine/emulador.html?lang=en) in Chrome or Edge.
2. Press **Power on**. It starts with a demo.
3. With the computer keyboard:
   - **Space** is PLAY, **Enter** is REC and **Shift** is GLO.
   - Numbers **1-8** are HOME, SEQ, EDIT, ENV, FX, SCL, ARP and SAVE, and the **arrows** are OCT− and OCT+.
   - The **Z-/** and **Q-P** rows play the FM-1 keys.
   - The **mouse wheel** over a knob turns it.
4. In **Sampler and memory** you can load a WAV of up to 4 s into slot A.

The emulator runs exactly the same C code that will run on the device.

## How installing will work

Once there is a version tested on hardware:

1. **Backup first.** The installer will refuse to install unless there is a full backup of your FM-1's flash (1 MiB).
2. **Install over USB-C**, from the browser (Web MIDI), with no extra programs.
3. **Going back:** you will always be able to return to the factory firmware with M-VAVE's official updater or, if something goes wrong, restore the backup with the chip's boot mode or with the *FM-1 Transporter* (RP2040).
4. **Safe mode:** holding OCT− and OCT+ at power-on boots without loading the patterns, in case one of them were damaged.

Bubba Machine only writes to the flash areas reserved for patterns, samples and settings. It never touches the bootloader or the factory firmware backup.

## Safety

Installing unofficial firmware **can brick your FM-1** and will probably void the warranty. This software comes with no warranty (see the [license](../../LICENSE), sections 15 and 16). Only install versions marked as **tested on hardware** in the [changelog](CHANGELOG.md).
