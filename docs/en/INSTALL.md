# Installing Bubba Machine

## Current status: beta (0.7.0-beta)

Bubba Machine for the FM-1 has a **full firmware, with sound**, a **diagnostic firmware** and a **web installer**. It runs on the author's FM-1, but it is a **beta**: tested on a single device and still changing a lot. **0.7.0-beta has not been fully tried on the device yet** (it passes the automated tests and the emulator; do not install 0.6.0-beta: it was short of RAM); if something goes wrong, go back to 0.6.2-beta or the stock firmware from the installer. Technical progress is in [`hal/fm1/README_PORT.md`](../../hal/fm1/README_PORT.md) (Spanish).

> **What you save may be lost when you update.** There is no backward compatibility until v1: if a new version changes how patterns, the song, settings or takes are stored, the old data is erased on first boot and the demo comes back. If something matters to you, record it from the audio output before updating.

Every version is also published as `bubba-machine-<version>.fwsc` in the [GitHub releases](https://github.com/Erbubar23/bubba-machine/releases), for installers that use that package (such as the [FM-1 Firmware Hub](https://fm1.designburgapps.com/)).

If you don't want to risk your FM-1, use the emulator (below): it runs the same code.

## Meanwhile: the emulator

1. Open the [emulator](https://erbubar23.github.io/bubba-machine/emulador.html?lang=en) in Chrome or Edge.
2. Press **Power on**. It starts with a demo.
3. With the computer keyboard:
   - **Space** is PLAY, **Enter** is REC and **Shift** is HOME (the modifier: hold it and turn SELECT to jump to another track).
   - Numbers **1-9 and 0** are HOME, SEQ, EDIT, ENV, FX, SCL, ARP, SAVE, LFO and GLO, and the **arrows** are OCT− and OCT+.
   - The **Z-/** and **Q-P** rows play the FM-1 keys.
   - The **mouse wheel** over a knob turns it.
4. In **Sampler and memory** you can load a WAV of up to 3 s as a new take.

The emulator runs exactly the same C code that will run on the device.

## How to install

It is **plug and play**: you need a computer with **Chrome or Edge** (Web MIDI) and a **USB data cable**. Nothing else.

1. Connect the FM-1 over USB and switch it on. It may run the factory firmware, X0X or Bubba Machine. Close any other app using its MIDI.
2. Open the **installer** (`instalar.html?lang=en` on the site) and accept the MIDI permission. The page **reads the FM-1 by itself** and says which firmware it runs.
3. The page has already picked the newest **Bubba Machine** (you can also pick the **Diagnostic**, no sound, to check buttons, keys and knobs, or M-VAVE's **Factory firmware**).
4. Press **Install** and confirm. Don't unplug until it says "Done".
5. Then the [control platform](../../web/control.html) loads the screen language, takes and sounds, and shows the memory.

If the page doesn't find the FM-1, under "Your FM-1" it shows which MIDI ports it sees and what is wrong with them (for example "busy" when REAPER or another app has it open).

The page downloads **the same `.fwsc` package as the release** (the one the FM-1 Firmware Hub uses) and checks its SHA-256 before writing anything.

**M-VAVE's FM-1.fwsc** (FM-1 V15) is only needed to **go back to the factory firmware** or for an old version without a published package: then the page asks for it (M-VAVE publishes it on its site), checks it is exactly that file and the browser remembers it for next time.

**From a copy of the repository** (for example, for a version you built yourself): `py -3 tools/fm1/servir.py` opens the installer at `http://localhost:8765/instalar.html` and hands it V15 directly (taken from `~/jieli/stock/FM-1.fwsc` or downloaded from M-VAVE once), so there is nothing to choose. Under **More options** you can install a file of your own.

How it works inside:

- The running firmware prepares **M-VAVE's official update loader** and restarts into it.
- That loader writes the new program.
- The chip's boot area and the device data (above `0xED000`) are not touched.
- The package refuses to install if the device's boot area is not identical to the file's.

## Back to the factory firmware

There are three ways, from the easiest to the emergency one:

- **From the same installer**: choose **Factory firmware** and press **Restore the factory firmware**.
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
