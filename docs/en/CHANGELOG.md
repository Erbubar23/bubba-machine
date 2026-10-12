# Changelog

Every version of Bubba Machine. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [semantic versioning](https://semver.org/): while the version starts with 0, any minor version may change how it plays or the pattern format.

## [Unreleased]

The sounds were rebuilt: **no more recorded sounds or wavetables**, everything is computed live with proven synthesis libraries. **Until v1 there is no backward compatibility:** 0.3 patterns are not read; on first boot they are erased (with the song and settings) and the demo starts. Sampler takes are kept.

### Added
- **Modeled drums** (T1 and KIT tracks): 16 instruments, each with its own **model**: 808 kick, snare and hat plus 909-style synthetic versions (the Plaits models by Mutable Instruments), metallic hat and cymbals, clap, tom and conga, cowbell, resonator (rim, clave, block), noise (maracas) and FM percussion. Kits **808**, **909** and **SINTE**. Each instrument is edited in INSTR: TUNE, DECAY, TONE, COLOR (whatever fits the model), MODEL, LEVEL, choke group and LENGTH. Accents hit differently, not just louder. Six drum voices (was four).
- **New ANALOG**, a real virtual analog: two variable-shape polyBLEP oscillators (SAW, SQUARE with pulse width, TRIANGLE, folded SINE and 3 SAWS), sync, ring, sub and noise; LP 24/12, BP or HP filter with resonance and drive; filter and amp ADSR; LFO to pitch, cutoff, shape or level; POLY, MONO or LEGATO with glide. 16 presets.
- **Paged sound editor** (SOUND for ANALOG, INSTR for drums): 4 settings per page on K1-K4, SELECT changes page, EDIT/ENV/LFO jump to the right one, with a drawing of what each page does (wave, filter curve, envelope, LFO).
- **6-operator FM engine** (msfa, the base of Dexed and of the factory firmware) for T3 and the free channels: 16 original patches (E.PIANO, FM BASS, BELL, BRASS, STRINGS, ORGAN, CLAVI, MARIMBA...), with the BRILLO (brightness), REALIM (feedback), DECAY and ALGORITMO (all 32 DX7 algorithms) macros and a screen showing the 6 operators. MORE > ENGINE > FM.
- **Sampler:** holding a step plays the loop from there, whole and looping, while you hold it.
- **Free recording on note tracks**, like a looper: on an empty track, REC has no fixed length; when you close it, the track gets whole bars (1-4) and every note keeps how long you held it.
- **Length in bars** on the SEQ layer (black 7-10).
- **New sound engines:** **ACID** (303-style: 4-pole ladder filter with resonance, accent and slide) for T3 and the free tracks; **ANALOG** (two detuned oscillators, sub and noise, 6 timbres) for the free tracks; and the **SINTE** kit, drums computed live. The recorded KO-808 and KO-909 kits stay.
- **Take memory in view:** on the recorder (free seconds and KB, maximum per take) and in the emulator, next to audio loading.
- **FM-1, full Bubba Machine for the device** (in testing, not yet tried on hardware): the core with sound through the codec (ALNK0), matrix and LEDs on interrupts, screen, USB-MIDI, saving, safe mode and the update service.
- **FM-1, diagnostic firmware** (in testing): no sound that shows every button, key and knob, MASTER, the battery and USB-MIDI, with safe mode, a way into chip mode and the update service.
- **Web installer** (`instalar.html`): builds the package in the browser on top of M-VAVE's FM-1 V15 (which each user downloads) and installs it with the official loader; it reads the FM-1 by itself (says which firmware it runs, or why it can't find it), you choose what to install (Bubba Machine, diagnostic or factory firmware) and press one button; it also goes back to the factory firmware and resumes an interrupted install. `tools/fm1/servir.py` serves it on the computer with V15 already loaded.
- **FM-1 tools:** `tools/fm1/fwsc.py` (packer, checked byte for byte against V15) and `tools/fm1/rescate.py` (rescue from Windows through chip mode, with a full backup before writing).

### Changed
- **Black 10 on drums:** a tap no longer mutes the instrument (it happened by accident and silently); **hold** it and pick with the white keys. Every mute shows a message and a muted instrument turns red in the grid.
- **Old engines removed:** BASS, SYNTH (wavetable), MONO and the 6-timbre ANALOG become the new ANALOG; the recorded KO-808/KO-909 kits and the SINTE kit become the modeled drums. The FM-1 program is 148 KiB smaller and 64 KiB of RAM are freed.
- Patterns in **format 4**: each track stores 32 sound settings and each drum instrument its model and hit.
- **ANALOG on the FM-1:** 4.1 % of a core per voice (own polyBLEP oscillator, filter coefficients every 8 samples, division-free saturation and short per-block passes; the first version cost 10 %).
- **Audio code is compiled for speed:** on the FM-1 the demo with PLAY went from 36.9 % to 27.7 % of a core. Wavetables are built at boot from their spectrum (56 KiB less program).
- **44.1 kHz audio end to end, with a floating-point bus.** The engines used to run at 22 kHz and the output was doubled; they now run at 44.1 kHz (highs up to 20 kHz, no oversampling images) and the mix, delay, effects and clipper are floating point, block based. The delay and the effects capture (stutter, reverse, tape stop) are now 16-bit (the capture used to be 8-bit μ-law), and sampler recording goes through a Butterworth filter before dropping to 16 kHz. On the FM-1, with PLAY, the mix costs less than before even though it computes twice the samples (docs/TRAZA_FM1.md).
- **DROP** interpolation (no interpolation) becomes **LINEAR**: at 44.1 kHz, repeating samples sounded lo-fi.
- The SEQ page of note tracks is only about that track: a piano roll of the page with each note's length, the track's bars and the dotted tempo follower. It no longer shows the other tracks.
- Recorded notes can last up to 4 bars (before, 1).
- Flash map for the device: global settings move to `0xDD000` and the take library becomes 116 KiB (7.4 s in total; before 8.1 s), to make room for the waiting loader and the SDK store.

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
