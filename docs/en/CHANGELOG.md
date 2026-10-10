# Changelog

Every version of Bubba Machine. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [semantic versioning](https://semver.org/): while the version starts with 0, any minor version may change how it plays or the pattern format.

## [Unreleased]

### Added
- Website and documentation in English too, with a language switch on every page (emulator included).

## [0.2.0] - 2026-10-10

First public version. Runs on PC and in the web emulator; the FM-1 port has not been tested on hardware yet.

### Added
- **TR drums** (T1) and **sampler** (T2) with 16 instruments, each with its own line and length (polymeter), accent, ×2-×4 repeats, flam, P-Locks and steps that only play (or stay silent) in the fill. Live roll and TR-style erase.
- **Per-track time:** length 1-64 and speed ×2 to ×1/4; master length from 16 steps to infinite.
- **Sampler:** records the mix with live chops; 2 slots of 4 s in flash, played directly (XIP).
- **Factory loops** in ROM (BUBBA 160 and BUBBA 128), synthesized by `tools/gen_loops.py`. The demo starts with one.
- **Sample drawing by frequency:** three lanes (lows, mids and highs) instead of the waveform, which is a solid block with heavily compressed music.
- Per-track note **looper** on the synths (REC › PLAY › OVERDUB).
- **Chord grid** 4×4 with degree, name and function by color; progression sets, voice leading and strum.
- **Scenes** A-D with chains and SONG REC; automatic fills.
- **Third press of 2 s** (TIME, SYSTEM, SAMPLER, clear track, quick save) with a progress ring.
- **GPL-3.0-or-later** license, documentation, website and CI.

### Changed
- The project is released as **Bubba Machine**, independent from any earlier firmware.

## [0.1.0] - 2026-10-09

First internal version.

### Added
- Full C99 core: audio, 6-track sequencer, 240×240 interface, flash storage with CRC and wear leveling.
- Synthesized drums (2 kits), bass and 3 wavetable synths with filter, arpeggiator and scales.
- PC simulator with automated tests and a WebAssembly web emulator.
- FM-1 port skeleton (structure, pins and matrix scanning).

[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commits/main
