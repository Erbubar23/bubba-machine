# Roadmap

What is coming for Bubba Machine, in order of importance. Dates depend on getting the hardware working.

## Now: 0.3.x, interface 3.0

- The full 3.0 interface in the web emulator: four gestures per button, HOME everywhere, fixed and free tracks, sampler 3.0 and a recorder with a take library.
- Collecting reports and opinions about how it plays.
- Open decisions: more pattern banks (the flash holds 16) and ghost notes on drums.

## Porting to the FM-1

The most important step: making it run on the device. The plan is in [`hal/fm1/README_PORT.md`](../../hal/fm1/README_PORT.md) (Spanish):

1. A full flash backup and a proven way back to the factory firmware.
2. Boot, clock, screen and button matrix.
3. Audio over I2S, flash and USB-MIDI.
4. Measure real CPU, RAM and voices, and adjust.

## Web installer

- Install over USB-C from the browser, with Web MIDI.
- Mandatory backup before installing and one-click rollback.

## Done: 0.3, interface 3.0

See the [changelog](CHANGELOG.md) and the [manual](MANUAL.md). The design is in [`DESIGN_3.0.md`](DESIGN_3.0.md).

## After 0.3

- **Per-track LFO** (the button already has its layer: repeat and tie).
- **Pattern banks:** a bigger flash map or smaller patterns.
- **Ghost notes on drums:** a level P-Lock or a bigger format.

## Later

- **Web editor:** upload and download patterns, samples and loops over USB.
- **Free loop and kit packs** (CC0 or GPL), made by the community.
- **Bluetooth MIDI**, if the hardware and the CPU allow it.
