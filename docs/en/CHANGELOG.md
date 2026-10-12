# Changelog

Every version of Bubba Machine. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow [semantic versioning](https://semver.org/): while the version starts with 0, any minor version may change how it plays or the pattern format.

## [0.5.0-beta] - 2026-10-12

**Beta, not yet tried on the device:** it passes the automated tests and the emulator. If something goes wrong on the FM-1, go back to 0.4.0-beta or the stock firmware from the installer.

**The screen switches to English**, so Bubba Machine can be understood worldwide. Spanish is still available as a language pack, not loaded by default. **Take memory goes down from 7.4 s to 6.4 s**: 16 KiB of flash now belong to the language pack, and a take that used that space is lost. **The sampler becomes a pad sampler** and the pattern format changes: saved patterns are erased at the first boot (takes stay).

### Added
- **Language packs.** The screen's phrases are in English and a pack (`.bmlang`) translates them. It lives in its own flash zone (`0xD9000`, 16 KiB), so it does not touch patterns or takes and a firmware update does not erase it. **SYSTEM › LANGUAGE** switches between English and the loaded pack. Whatever the pack does not translate shows in English.
  - **Spanish** (`lang/es.txt`, built into `lang/es.bmlang`): the screen looks as before.
  - It is loaded on the FM-1 with `tools/fm1/idioma.py` (over the USB-MIDI console, with the transport stopped) and in the emulator with a button; on the Spanish emulator page it starts in Spanish the first time.
  - `tools/gen_lang.py` collects every phrase into `lang/plantilla.txt`, checks each translation (ASCII only, the same `%`) and builds the packs. Anyone can make another language.

### Changed
- **T2 is a 16-pad sampler**, like a KO II or the drum sampler of an OP-1 field. It replaces the sliced loop with a generator (and SLICER, CORTAR and CHOPS).
  - **Each pad is a piece of a take**, with its own start and length (they can overlap), pitch, level, key mode (**ONESHOT**, **KEY**, **THRU**: held, it keeps going through the take, **LOOP**), direction, attack, release, choke group and whether it follows the tempo. A kit can mix takes.
  - **The take builds the kit:** when a take is saved with the kit empty, it is cut on the hits, each cut goes to a pad and the grid is written so it plays like the take. In **TAKES**: N1 kit on the hits, N2 in 16, N3 in 8, N4 the whole take on the selected pad; white keys play it in 16 parts.
  - **SEQ is a grid like the drums:** one line per pad, with its length, accents, repeats, conditions, mutes, fill and per-step P-Lock of FILTER, PITCH, DECAY and START. No more loose numbers: you see which pad plays on which step.
  - **PADS** (EDIT): the take's waveform with the pad in brackets and the other pads marked, and three pages (TRIM, SOUND, ENV; EDIT again changes page). Black keys: copy, paste, empty, start on the hit; 9-11 reverse, half speed and stutter.
  - **BREAK** (SEQ again): the old generator, now on the grid and off by default. VARY changes pads, ANCHOR keeps the strong beats, REPEAT and BREAK. The PLAYS row shows what is going to play and EDIT + black 4 writes it into the grid.
  - On the recorder, white keys play the new take before it is saved.
- The sampler drawing (SOUND) is the same mirrored waveform; the three frequency lanes are gone.
- Drum model names, FM patches and waveforms are in English too (translated in the pack).

## [0.4.0-beta] - 2026-10-11

**Beta: the first version that runs on the FM-1.** It was tested on a single device (the author's) and still changes a lot. **An update may erase what you saved** (patterns, song, settings and sampler takes): until v1 there is no backward compatibility, and when a format changes the old data is erased at boot and the demo comes back. If something matters to you, record it from the audio output before updating.

The sounds were rebuilt: **no more recorded sounds or wavetables**, everything is computed live with proven synthesis libraries. What 0.3 saved (patterns, song, settings **and takes**) is not read: it is erased on first boot and the demo starts.

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
- **SLICER** (SEQ again on T2): the sampler cuts the take into **16 slices**, one per white key. White keys play the slices and, with REC and PLAY, set them on the step; K1 picks, K2 moves, K3 sensitivity, K4 gain; black 1-3: slices on the hits, 16 or 8 equal; 9-11 reverse, half speed and stutter. Slicing on the hits looks for the attacks (it also works for vocals).
- **Key strip on screen:** every track page, MEZCLA (mix) and ESCENAS (scenes) draws the 11 black and 16 white keys with a short name and the same state as their light (the lights are white and don't say what a key does). On HOME the strip is large. Where the white keys are already on screen (SEQ, CORTAR, SLICER), the strip shows only the black keys.
- **Drums in sections of 8:** the drum SEQ shows 8 instruments with their names in regular type; black 9 switches section.
- **Upload a take from the computer:** `tools/fm1/toma.py` converts any audio (with ffmpeg) and sends it to the FM-1 over USB-MIDI; it arrives as a freshly recorded take.
- **New demo:** acid techno at 160 BPM (909, a 303 line with accents and slides and the sampler slicing the first take).
- **FM-1, full Bubba Machine for the device** (beta, tested on the author's FM-1): the core with sound through the codec (ALNK0), matrix and LEDs on interrupts, screen, USB-MIDI, saving, safe mode and the update service.
- **FM-1, diagnostic firmware**: no sound that shows every button, key and knob, MASTER, the battery and USB-MIDI, with safe mode, a way into chip mode and the update service.
- **Web installer** (`instalar.html`): builds the package in the browser on top of M-VAVE's FM-1 V15 (which each user downloads) and installs it with the official loader; it reads the FM-1 by itself (says which firmware it runs, or why it can't find it), you choose what to install (Bubba Machine, diagnostic or factory firmware) and press one button; it also goes back to the factory firmware and resumes an interrupted install. `tools/fm1/servir.py` serves it on the computer with V15 already loaded.
- **FM-1 tools:** `tools/fm1/fwsc.py` (packer, checked byte for byte against V15) and `tools/fm1/rescate.py` (rescue from Windows through chip mode, with a full backup before writing).

### Changed
- **Black 10 and 11 on tracks** open screens. **10** (drums) opens the instrument **MUTES** and **11** opens the **FILL** screen, where the fill plays and the white keys build its hits (or mark notes as fill-only), with K1 = automatic fill. A tap keeps them open (close with the same key or HOME); holding = only while held. A tap no longer mutes blindly: every mute shows a message and a muted instrument turns red.
- **ALGORITHM inside a track** also goes through the box with the number before switching; in INSTR it picks an instrument and plays it.
- **HOME explains the black keys** (mutes, solo, scenes, fill and the drum screens).
- **Old engines removed:** BASS, SYNTH (wavetable), MONO and the 6-timbre ANALOG become the new ANALOG; the recorded KO-808/KO-909 kits and the SINTE kit become the modeled drums. The FM-1 program is 148 KiB smaller and 64 KiB of RAM are freed.
- Patterns in **format 5**: each track stores 32 sound settings, each drum instrument its model and hit, and each sampler note its slice (4 bits). Takes move to the **BMT4** signature (16 slices in the header).
- **Factory loops removed:** the sampler plays only takes (recorded or uploaded with `tools/fm1/toma.py`).
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

[0.4.0-beta]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.4.0-beta
[0.3.0]: https://github.com/Erbubar23/bubba-machine/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/Erbubar23/bubba-machine/releases/tag/v0.2.0
[0.1.0]: https://github.com/Erbubar23/bubba-machine/commits/main
