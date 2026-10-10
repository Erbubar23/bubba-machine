# Roadmap

What is coming for Bubba Machine, in order of importance. Dates depend on getting the hardware working.

## Now: 0.2.x, public preview

- The full firmware in the web emulator.
- Documentation, GPL-3.0 license and website.
- Collecting reports and opinions about how it plays.

## Porting to the FM-1

The most important step: making it run on the device. The plan is in [`hal/fm1/README_PORT.md`](../../hal/fm1/README_PORT.md) (Spanish):

1. A full flash backup and a proven way back to the factory firmware.
2. Boot, clock, screen and button matrix.
3. Audio over I2S, flash and USB-MIDI.
4. Measure real CPU, RAM and voices, and adjust.

## Web installer

- Install over USB-C from the browser, with Web MIDI.
- Mandatory backup before installing and one-click rollback.

## 0.3: the 3.0 interface

The new way to play, already designed in [`DESIGN_3.0.md`](DESIGN_3.0.md):

- Four gestures per button that mean the same thing on every button: tap, hold, hold 2 s and HOME + button.
- HOME everywhere: sequence controller and global modifier.
- ALGORITHM moves between tracks, with a box that confirms where you are going.
- 3 fixed tracks (drums, sampler and bass) and 3 free ones (SYNTH, MONO, KIT or MIDI).
- Sampler 3.0: a break the generator rearranges, with a PLAYS row to see what it picked.
- A dedicated recorder with a take library and an editor to slice later.

## Later

- **Web editor:** upload and download patterns, samples and loops over USB.
- **Free loop and kit packs** (CC0 or GPL), made by the community.
- **Bluetooth MIDI**, if the hardware and the CPU allow it.
