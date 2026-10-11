# Changelog

Every version of Bubba Machine. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [semantic versioning](https://semver.org/): while the version starts with 0, any minor version may change how it plays or the pattern format.

## [0.3.0] - 2026-10-10

Interface 3.0, complete in the emulator. **It does not read 0.2 patterns, settings or samples:** on first boot it erases them and starts with the demo. The FM-1 port is still untested on hardware.

### Added
- **Four gestures per button** that mean the same thing on every button: tap (its page; again, the next one), hold (its live layer, with the keyboard drawn on screen), hold 2 s and **HOME + button** (its global version).
- **HOME everywhere:** home screen with the pattern bank, a **controller** when held on any screen (white keys = queue a pattern, two whites = chain, black keys = mute or solo, scenes and FILL) and a modifier: undo and redo, back to step 1, SONG REC, song, global delay, project scale, save as and copy a track onto another.
- **ALGORITHM navigates:** on HOME, a box with the target track and entry after 400 ms (0, 250, 400, 800 ms or OFF); inside a track it switches and keeps the page; with HOME held it picks a track from any screen.
- **Tabs per track** that SELECT walks, and the **MAS** (more) tab with everything about the track.
- **3 fixed and 3 free tracks:** T4-T6 pick the **SYNTH, MONO, KIT** (a second TR drum machine) or **MIDI** engine (sends its notes over MIDI).
- **New layers:** SEQ length and speed, ENV paint accents, LFO paint repeats or ties, SAVE copy the pattern (or one track) somewhere else, GLO transpose the whole project with TAP tempo on PLAY.
- **Sampler 3.0:** a loop in 8 slices that a **generator** rearranges bar by bar, with the **TU** row (silence, generator or pinned slice) and the **SUENA** row (what it picked), the VARIAR, ANCLAR, REPETIR and ROMPER knobs (vary, anchor, repeat, break), detected kick and snare (K and S), loop B with its chance, weighted retrigs, live reverse, half speed and stutter, and **CAPTURAR** (capture) the variation that is playing.
- **The loop follows the tempo** and the screen says how many semitones it moved; PITCH corrects it and MAS > USAR BPM DEL LOOP sets the project to its tempo.
- **A dedicated recorder** (REC on the sampler): armed to the bar, with the seconds left in large digits, an unsaved take that already plays and is saved on stop.
- **Take library** of free-length takes (8.1 s in total, up to 3 s per take) with BPM, bars and gain, and the **CORTAR** (slice) editor: cuts on eighth notes, on the hits or even.
- **Three-level accent** on notes (normal, accent, ghost) and how much accent per track.
- **A scale per track**; the project scale lives on HOME + SCL.
- **Questions** before erasing (OCT+ yes, HOME no).
- Website and documentation in English too, with a language switch on every page (emulator included).

### Changed
- **GLO** is no longer the modifier: HOME is. GLO opens the global list and, held, the TONO (pitch) layer.
- **REC** acts on release: tap = record, hold = record while held, 2 s = clear the track (asks).
- A track's black keys follow a single grammar: 1-8 the elements, 9 and 10 modify, 11 FILL or STUTTER.
- The sampler went from 16 chops to 8 slices, and from 2 fixed 4 s slots to the take library.
- The sampler can also duck under the kick's sidechain, and the demo lowered its levels so it does not clip with the break playing.
- **Format 3** for patterns, settings and takes.

### Removed
- The 0.2 SAMPLER screen (now SONIDO, TOMAS, GRABAR and CORTAR) and slots A and B.

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

[0.3.0]: https://github.com/Erbubar23/bubba-machine/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commits/main
