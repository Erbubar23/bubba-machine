# Roadmap

Everything planned for Bubba Machine. **All of it will be done, step by step**, with no dates: each step ships when it has been tested on the FM-1. The order inside each list is the order that matters today and may change.

Items marked ✓ are already in the current version (0.4.0-beta). Until v1, any version may erase what you saved (see [install](INSTALL.md)).

## Audio

The goal is the best quality the FM-1 allows. What improved, what it costs and what limits it today: [audio](AUDIO.md).

**Done**
- ✓ Engines and mix at 44.1 kHz, floating-point bus, 16-bit delay and effects capture.
- ✓ Modeled drums (Plaits), ANALOG with polyBLEP, 6-operator FM (msfa) and ACID.
- ✓ 32 KiB data cache and audio compiled for speed.

**Planned**
1. **Audio on both cores.** The second core gives 1.83× (measured): more voices and room for effects.
2. **Master effects:** reverb and chorus (from Mutable Instruments), tape delay, compressor and leveler.
3. **ACID 2.0:** Open303 in floating point, with an acid line generator.
4. **A higher-quality sampler with more seconds**, within what the flash allows (today it records at 16 kHz and 8 bits).
5. **A/B morph per drum instrument:** two versions of each hit and a knob that moves from one to the other.
6. **CUERDA (string) and MODAL engines** (Plaits physical modeling): they already have a place in the format, they just aren't selectable yet.
7. **Oversampled clipper**, so high DRIVE doesn't alias.
8. **Faster screen:** SPI at 30 MHz (2.2× measured) and DMA transfers, which frees processor time for audio.
9. **Measure the FM-1's analog output** (noise, distortion, response) with an audio interface.

## Interface: Live 0.5

0.5 organizes the FM-1 into two places: **playing live** (HOME and its pages) and **working on a track** (inside a track). The key lights are white only and don't say what each key does, so every screen shows it on the key strip.

**Done**
- ✓ **Key strip on screen:** the 11 black and 16 white keys with a short name and the same intensity as their light.
- ✓ **SLICER:** a take in 16 slices played with the white keys.
- ✓ **MUTES** and **FILL** screens, and drums in sections of 8.

**Planned: playing live (HOME again = the next page)**
1. **PERFORM:** K1-K4 as show macros that never move (DJ FILTER, DELAY, REVERB, MORPH). On every HOME page, the same black keys: 1-6 mutes (hold = solo), 7-10 scenes, 11 FILL.
2. **MORPH A·B:** move from one sound state to another with a knob.
3. **FX MASTER:** the master effects on one page.
4. **SCENES AND SONG:** build and play the song with the scenes.

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
- **Web editor:** load and download patterns and takes over USB (today takes are uploaded with `tools/fm1/toma.py`), and DX7 `.syx` FM banks.
- **Free kit and take packs** (CC0 or GPL), made by the community.
- **Bluetooth MIDI**, if the hardware and processor allow it (no promise: no free firmware has shown it yet).

## Done

- **0.4.0-beta:** the first version for the FM-1, sounds computed live and 44.1 kHz audio. See the [changelog](CHANGELOG.md).
- **0.3:** interface 3.0, with four gestures per button and HOME everywhere. The design is in [`DESIGN_3.0.md`](DESIGN_3.0.md).
