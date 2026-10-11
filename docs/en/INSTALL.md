# Installing Bubba Machine

## Current status: in testing, no sound yet

There is a **diagnostic firmware** for the FM-1 and a **web installer**. Neither has been tested on a device yet: the author does the first test. The firmware with sound comes once the diagnostic confirms the screen, the buttons and USB. Technical progress is in [`hal/fm1/README_PORT.md`](../../hal/fm1/README_PORT.md) (Spanish).

If you don't want to risk your FM-1, use the emulator (below): it runs the same code.

## Meanwhile: the emulator

1. Open the [emulator](https://erbubar23.github.io/bubba-machine/emulador.html?lang=en) in Chrome or Edge.
2. Press **Power on**. It starts with a demo.
3. With the computer keyboard:
   - **Space** is PLAY, **Enter** is REC and **Shift** is HOME (the modifier: hold it and turn ALGORITHM to jump to another track).
   - Numbers **1-9 and 0** are HOME, SEQ, EDIT, ENV, FX, SCL, ARP, SAVE, LFO and GLO, and the **arrows** are OCT− and OCT+.
   - The **Z-/** and **Q-P** rows play the FM-1 keys.
   - The **mouse wheel** over a knob turns it.
4. In **Sampler and memory** you can load a WAV of up to 3 s as a new take.

The emulator runs exactly the same C code that will run on the device.

## How to install

You need a computer with **Chrome or Edge** (Web MIDI), a **USB data cable** and the **FM-1.fwsc** file (FM-1 V15) that M-VAVE publishes on its site.

1. Connect the FM-1 over USB and switch it on. It may run the factory firmware, X0X or Bubba Machine. Close any other app using its MIDI.
2. Open the **installer** (`instalar.html?lang=en` on the site) and accept the MIDI permission. The page **recognizes the FM-1 by itself** and builds the package.
3. Press **Install Bubba Machine** and confirm. Don't unplug until it says "Done".

The first time, the page asks for **M-VAVE's FM-1.fwsc** (FM-1 V15), which M-VAVE publishes on its site: the package is **built in your browser** on top of that file, because this site only publishes our part, never M-VAVE's. It is checked to be exactly that file, and the browser remembers it for next time.

**From a copy of the repository** (for example, for a version you built yourself): `py -3 tools/fm1/servir.py` opens the installer at `http://localhost:8765/instalar.html` and hands it V15 directly (taken from `~/jieli/stock/FM-1.fwsc` or downloaded from M-VAVE once), so there is nothing to choose. The version is changed under **More options**.

How it works inside:

- The running firmware prepares **M-VAVE's official update loader** and restarts into it.
- That loader writes the new program.
- The chip's boot area and the device data (above `0xED000`) are not touched.
- The package refuses to install if the device's boot area is not identical to the file's.

## Back to the factory firmware

There are three ways, from the easiest to the emergency one:

- **From the same installer**: under **More options**, press **Back to the factory firmware**.
- **With M-UPGRADE**, M-VAVE's official updater. Bubba Machine presents itself as FM-1 version 3 and speaks the same protocol, so M-UPGRADE should offer V15 (not tested yet).
- **From X0X or its safe mode**, with its own installer.

## If something goes wrong

- **The install was interrupted** (the FM-1 shows up as "ota-FM-1"): reconnect it with the installer open: the button changes to **Resume the install**. The FM-1 stays in that mode until it finishes.
- **Safe mode:** if Bubba Machine fails to start properly twice in a row, it starts with no sound and USB on, so you can reinstall or go back to the factory firmware. You also get there by switching it on with OCT− and OCT+ held.
- **Chip mode:** after four failed starts, or with OCT− and OCT+ held together for 5 s, the FM-1 enters the chip's own update mode and doesn't show up as MIDI. From a Windows console **run as administrator**:

  ```
  py -3 tools/fm1/rescate.py respaldo
  py -3 tools/fm1/rescate.py restaurar FM-1.fwsc
  ```

  It always saves a full flash backup first. Then it writes only the program area, and checks what it wrote.
- **Nothing above responds:** the *FM-1 Transporter* remains, an RP2040 board wired to the USB lines.

## Safety

Installing unofficial firmware **can brick your FM-1** and will probably void the warranty. This software comes with no warranty (see the [license](../../LICENSE), sections 15 and 16). Until the [changelog](CHANGELOG.md) marks a version as **tested on hardware**, it is a test.
