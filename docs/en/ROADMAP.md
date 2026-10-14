# Roadmap

Everything planned for Bubba Machine. **All of it will be done, step by step**, with no dates: each step ships when it has been tested on the FM-1. The order inside each list is the order that matters today and may change.

Items marked ✓ are already in the current version (0.6.0-beta); items marked **in testing** are there but off by default until measured on the FM-1. Until v1, any version may erase what you saved (see [install](INSTALL.md)).

## Audio

The goal is the best quality the FM-1 allows. What improved, what it costs and what limits it today: [audio](AUDIO.md).

**Done**
- ✓ Engines and mix at 44.1 kHz, floating-point bus, 16-bit delay and effects capture.
- ✓ Modeled drums (Plaits), 6-operator FM (msfa) and the ACID bass.
- ✓ 32 KiB data cache and audio compiled for speed.
- ✓ **The 0.6 machines:** B-MINI, BITTAL (alias-free wavetable, from Vital presets) and BUNO-6 (with stereo chorus).
- ✓ **Reverb** with per-track send, **master filter** and **BEAT FX**.
- **In testing:** audio on both cores (T4-T6 on the second; on the computer it gives the same samples) and the screen with SPI at 30 MHz.

**Planned**
1. **Measure 0.6 on the FM-1:** each machine's sound and load, BUNO-6's chorus and high-pass (approximate values today) and even volume across BITTAL sounds. Then turn both cores and the fast screen on by default.
2. **Master effects:** tape delay, compressor and leveler.
3. **ACID 2.0:** Open303 in floating point, with an acid line generator.
4. **A higher-quality sampler with more seconds**, within what the flash allows (today it records at 16 kHz and 8 bits).
5. **A/B morph per drum instrument:** two versions of each hit and a knob that moves from one to the other.
6. **CUERDA (string) and MODAL engines** (Plaits physical modeling): they already have a place in the format, they just aren't selectable yet.
7. **Oversampled clipper**, so high DRIVE doesn't alias.
8. **Screen over DMA:** SPI at 30 MHz is already in testing; with DMA the transfer leaves the processor free for audio.
9. **Measure the FM-1's analog output** (noise, distortion, response) with an audio interface.

## Interface

The interface organizes the FM-1 into two places: **playing live** (HOME and its pages) and **working on a track** (inside a track). The key lights are white only and don't say what each key does, so every screen shows it on the key strip.

**Done**
- ✓ **Key strip on screen:** the 11 black and 16 white keys with a short name and the same intensity as their light.
- ✓ **English screen, with language packs:** Spanish is loaded separately (`tools/fm1/idioma.py`) and switched in SYSTEM › LANGUAGE.
- ✓ **Pad sampler:** 16 pads made from your takes, like a KO II or an OP-1 field, on a grid like the drums, with BREAK to vary it.
- ✓ **MUTES** and **FILL** screens, and drums in sections of 8.
- ✓ **The 0.6 face:** header, viewport, keyboard and dials always in the same place; the **BX-1** mixer on HOME; ALGORITHM as each machine's mode.
- ✓ **Expressive REC:** micro-timing, gate and P-Locks of what you turn while holding the note; long notes with two white keys.

**Planned: playing live (HOME again = the next page)**
1. **MORPH A·B:** move from one sound state to another with a knob (HOME's fourth macro).
2. **SCENES AND SONG:** build and play the song with the scenes.

**Planned: working on a track (the button again = the next page)**
1. **SEQ › LOOP › DICE:** controlled random variations, and the ACID generator for bass lines.
2. **EDIT › SOUND › INSTR:** the A/B drum instrument.
3. **LFO › MOTION:** a modulation sequencer and knob-movement recording.
4. **FX › TRACK:** per-track mix and effects.

**Other**
- **Pattern banks:** more than 20 patterns, by growing the flash map or shrinking the patterns.
- **Ghost notes on drums:** a level P-Lock or a bigger format.

## Tools and ecosystem

- **Back up what you saved from the browser** (patterns, song and takes), so nothing is lost when updating.
- **Recording straight onto a pad** (REC + pad, like the KO II) and undo for pad edits.
- ✓ **Plug and play web installer:** installs the ready-made package without the V15 and loads Spanish at the end.
- ✓ **Web sound editor:** Vital presets and DX7 `.syx` banks, sent to the FM-1 over WebMIDI.
- **More languages** made by the community.
- **Web pattern and take editor:** load and download them over USB (today takes are uploaded with `tools/fm1/toma.py`).
- **Free kit and take packs** (CC0 or GPL), made by the community.
- **Bluetooth MIDI**, if the hardware and processor allow it (no promise: no free firmware has shown it yet).

## Done

- **0.6.0-beta:** six fixed machines, the new face, the sound bank and the plug and play installer. The design is in [`DISENO_0.6.md`](../DISENO_0.6.md) (in Spanish).
- **0.5.0-beta:** the pad sampler and the English screen, with Spanish as a language pack.
- **0.4.0-beta:** the first version for the FM-1, sounds computed live and 44.1 kHz audio. See the [changelog](CHANGELOG.md).
- **0.3:** interface 3.0, with four gestures per button and HOME everywhere. The design is in [`DESIGN_3.0.md`](DESIGN_3.0.md).
