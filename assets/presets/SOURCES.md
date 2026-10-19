# Factory presets: sources

Data, not user docs. One line per factory sound: where its settings come from. "own" = designed by ear
here (no published setting exists); the sound it evokes is in brackets. Panel settings from published
charts, manuals and articles were translated to our parameters (tools/gen_snaps.mjs, tools/gen_fm.py,
core/bm_params.c); no audio was sampled and no factory data dump was copied (DX7 ROM: recipes only).
Names allude; they never carry an artist name, a song title or a trademark.

Machine attributions checked for the song-inspired sounds (and dropped when they did not hold):
"Thriller" bass = ARP 2600 (not B-MINI); "I Feel Love" and "Lucky Man" = Moog modular; "Take On Me"
riff = Juno-60 and its bass = DX7; "Mentasm" = Alpha Juno (so DARK RAVE is only inspired); LatelyBass =
TX81Z (so HOUSE BS is a 6-op rebuild); "Spastik" = drums only; "Revolution 909" = no 303 documented.

## BB-303 (core/bm_params.c, bm_ac_presets)

TB-303 owner's manual (1982), pages 8 ("Initial Settings") and 34-35 ("Sample Sounds"), knob positions
read from the drawings (7:00 = 0, 5:00 = 127): https://notebook.zoeblade.com/Downloads/Documentation/Roland/TB-303_owners_manual.pdf
SLIDE 7 = the 303's fixed slide (~60 ms, Open303's value): https://www.firstpr.com.au/rwi/dfish/303-slide.html

| Preset | Source |
|---|---|
| CLASSIC | own (the classic acid squelch) |
| E.BASS A | manual: Electric Bass Type A |
| E.BASS B | manual: Electric Bass Type B |
| UPRIGHT | manual: Acoustic Bass |
| SYNTH BASS | manual: Synthesized Bass |
| INIT 1982 | manual: Initial Settings |
| CHICAGO 86 | own [the first acid house jam, 303 verified]: https://djmag.com/content/game-changers-phuture-acid-tracks , https://daily.redbullmusicacademy.com/2012/12/dj-pierre-interview |
| PHILLY 95 | own [the screaming 1995 acid hit, two stock 303s verified]: https://mixmag.net/feature/josh-wink-tells-us-how-he-really-made-higher-state-of-consciousness |
| ROBOT FUNK | own [the French 1995 303 riff, 303 verified]: https://en.wikipedia.org/wiki/Da_Funk , https://musictech.com/news/daft-punk-gear-list-1997-homework-album/ |
| BIG BEAT | own [the big-beat ode to the 303, real 303 verified]: https://musicradar.com/news/fatboy-slim-real-303-not-clone , https://articles.roland.com/everybody-needs-a-303-fatboy-slim/ |
| WINDSOR 93 | own [the minimal 303 + 606 album, verified]: https://store.mute.com/browse/plastikman/products/plastikman-sheet-one-30th-anniversary-edition |
| BOMBAY 82 | own [ragas on a disco beat, 303 verified]: https://ra.co/reviews/7445 |

## B-MINI (tools/gen_snaps.mjs, MINI)

Minimoog factory "Sound Charts" (Tom Rhea) as redrawn by Sound On Sound and MatrixSynth; Reverb's video
recreations for the song sounds (Minimoog verified on each record, settings are Reverb's, not studio notes).
Panel mapping: cutoff -5..+5 -> 60 Hz..20 kHz, emphasis / contour 0-10 -> 0..127, mixer 0-10, contour
times in ms, KB1 / KB2 -> KBD CTRL 1/3 / 2/3, DECAY switch -> release.

| Preset | Source |
|---|---|
| WIZARD BASS | Reverb recreation [the 1977 funk bass played on stacked Minimoogs]: https://reverb.com/news/video-the-synth-sounds-of-5-classic-minimoog-tracks , https://en.wikipedia.org/wiki/Flash_Light_(song) |
| FAT UNISON | Rhea chart "Emerson's Fat Bass": https://www.matrixsynth.com/2013/05/sound-charts-for-minimoog-elp-trilogy.html |
| TUTTI BRASS | Rhea chart "Trumpet" (SOS redraw): https://www.soundonsound.com/techniques/brass-synthesis-minimoog |
| RASP BRASS | Gordon Reid's brass patch, SOS: https://www.soundonsound.com/techniques/brass-synthesis-minimoog |
| SLIDE BONE | Rhea chart "Trombone (Jazz)" (SOS redraw): same SOS article |
| NOISE TUBA | Rhea chart "Tuba" (SOS redraw; the noise filter modulation becomes a little noise in the mixer): same SOS article |
| OCTAVE GLIDE | Rhea chart "ELP Trilogy": https://www.matrixsynth.com/2013/05/sound-charts-for-minimoog-elp-trilogy.html |
| FIFTHS | Rhea chart "Hindemoog" without its trill: https://www.matrixsynth.com/2015/01/vintage-moog-minimoog-model-d.html |
| TLR SOUND | Rhea chart "The T.L.R. Sound" (without the osc 3 ramp): https://www.matrixsynth.com/2015/01/vintage-moog-minimoog-model-d.html |
| COLD FRIEND | Reverb recreation [the 1979 electro-pop number one lead]: https://reverb.com/news/video-the-synth-sounds-of-5-classic-minimoog-tracks |
| MUTANT ARP | Reverb recreation [the 1978 new-wave patrol riff] (osc 3 ramp left out): same Reverb video |
| SOFT HORN | Reverb recreation [the 1975 long-form solo]: same Reverb video |
| WHINE LEAD | Reverb-style G-funk lead (genre sound; the Minimoog on that album is partly verified): https://en.wikipedia.org/wiki/Bitches_Ain%27t_Shit |
| MOTOR RIFF | own [the 1979 car riff, Minimoog verified]: https://en.wikipedia.org/wiki/Cars_(song) |
| HIGHWAY BASS | own [the 1974 motorway octave bass, Minimoog verified]: https://en.wikipedia.org/wiki/Autobahn_(song) |
| BENDER LEAD | own [the 80s TV-theme bent lead, through a driven amp]: https://www.soundonsound.com/people/jan-hammer |
| SUB ONE | community Vital preset (assets/presets/banco.json) |
| REESE | community Vital preset (assets/presets/banco.json) |

## BX-7 (tools/gen_fm.py)

Programmed by parameters from published recipes; no DX7 ROM data.

| Preset | Source |
|---|---|
| TINEKEYS | E.PIANO 1 recipe [the 80s ballad e-piano, ~40 % of 1986 US number ones]: https://www.muzines.co.uk/articles/yamaha-dx7/4733 , https://yamahasynth.com/learn/synth-programming/fm-101-part-five-expanding-your-sound/ , https://meganlavengood.com/wp-content/uploads/2019/09/Lavengood-2019-%E2%80%9CWhat-Makes-It-Sound-%E2%80%9980s%E2%80%9D-The-Yamaha-DX7-Electr.pdf |
| FULLTINE | STK Rhodey ratios, levels and decays: https://github.com/thestk/stk/blob/master/src/Rhodey.cpp |
| POP BASS | generic BASS 1 recipe [the 80s pop / film synth bass, DX7 BASS 1 verified on "Take On Me"]: https://musictech.com/tutorials/fm-synthesis-basics-dexed/ , https://www.soundonsound.com/techniques/classic-tracks-ha-take-me |
| HOUSE BS | 6-op rebuild of the TX81Z house bass, MusicTech recipe: https://musictech.com/tutorials/learn-advanced-fm-synthesis-with-dexed/ , https://www.polynominal.com/lately-bass-yamaha-tx81z-dx100/index.html |
| SUB BASS | own (kept from 0.8) |
| TUBECHIM | STK TubeBell ratios and levels: https://github.com/thestk/stk/blob/master/src/TubeBell.cpp |
| CHIMES | SOS "Practically FM" part 5: https://www.muzines.co.uk/articles/practically-fm/3979 |
| BRASS 80 | SOS "Practically FM" part 4: https://www.muzines.co.uk/articles/practically-fm/3961 |
| STRINGS | own (kept from 0.8; 1:1 pairs with a slow attack, as SOS describes) |
| MARIMBA | DecentSampler FM recipe (1:3.5, fast modulator decay), shortened: https://decentsampler-developers-guide.readthedocs.io/en/1.21.1/topic-how-to-use-fm-synthesis.html |
| FLUTE | own (kept from 0.8; feedback-noise breath as Dave Bristow describes): https://www.muzines.co.uk/articles/dave-bristow-on-the-yamaha-dx7/6076 |
| PIPE ORG | algorithm 19 recipe (ratios), levels own: https://www.muzines.co.uk/articles/yamaha-dx7/4733 |
| ORGAN | drawbar recipe, algorithm 32 at 1 / 2 / 3 / 4 / 6 / 8: https://www.muzines.co.uk/articles/yamaha-dx7/4733 |
| REED HRP | own (no published recipe) |
| CLAVI | own (kept from 0.8) |
| PLUCKED | own (kept from 0.8) |
| GLASS | own (kept from 0.8) |
| LEAD | own (kept from 0.8) |
| SUPER FM | own (kept from 0.8) |
| BUZZ | own (kept from 0.8) |

## BUNO-6 (tools/gen_snaps.mjs, BUNO)

"decoded" = based on a Roland factory patch, decoded by Ultramaster KR-106 (GPL), used as a starting point
and mapped to our engine (sliders 0-10; LFO 0.3-20 Hz, attack 1.5 ms-3 s, decay / release 1.5 ms-12 s):
https://github.com/kayrockscreenprinting/ultramaster_kr106/blob/HEAD/Source/KR106_Presets_JUCE.h , https://kayrock.org/kr106/guide.html

| Preset | Source |
|---|---|
| POLY BRASS | decoded (Juno-106 A11), 8' by ear |
| SAW STRINGS | decoded (Juno-60 A11) |
| SOFT PAD | Bonedo's Juno-60 pad (VCF LFO, LFO rate and delay, chorus I), the rest own: https://www.bonedo.de/artikel/sound-workshop-pad-lorde-team |
| PWM STRINGS | decoded (Juno-60 A13) |
| MOVING STRS | decoded (Juno-106 A15) |
| VOX CHOIR | decoded (Juno-106 A17) |
| DEEP CHORDS | own [the 1986 deep house chords, Juno-60 verified]: https://equipboard.com/pros/larry-heard , https://articles.roland.com/?p=17260 |
| PHASE BRASS | decoded (Juno-60 A18) |
| PULSE ORGAN | decoded (Juno-106 A21) |
| FUNKY PLUCK | decoded (Juno-106 A47) |
| POP RIFF | own [the 1985 synth-pop riff, Juno-60 verified]: https://www.soundonsound.com/techniques/classic-tracks-ha-take-me |
| DELAY LEAD | decoded (Juno-106 A51) |
| PULSE BASS | decoded (Juno-106 A48) |
| ROUND BASS | decoded (Juno-60 A31) |
| DARK RAVE | own [inspired by the early-90s rave "hoover", an Alpha Juno sound]: https://en.wikipedia.org/wiki/Hoover_sound |
| NOISE SWEEP | own (kept from 0.8) |

## BITTAL (tools/gen_snaps.mjs, BITTAL; community presets in assets/presets/banco.json)

Own wavetables, built additively in the style of the PPG Wave 2.2 tables named in its manual (no PPG ROM
data): https://www.hermannseib.com/documents/wave22getxt.pdf . Table names: https://www.presetpatch.com/article/ppg-wavetable-list

| Preset | Source |
|---|---|
| SAW MOTION ... LOW PAD (19) | community Vital presets (assets/presets/banco.json: repository, commit, license) |
| WAVE SWEEP | PPG Wave 2.2 manual 7.3.4.1 (table 00 harmonics with a resonant peak, ENV1-WAVES at maximum, ENV1 A28 D34 S0 R0), envelope times by ear |
| GLASS BELLS | own, in the style of PPG table 04 (high overtones, bells) [the PPG bells on mid-80s synth-pop, verified]: https://side-line.com/?p=24136 |
| MALLET VIBES | own, in the style of PPG table 02 (vibraphone) with the factory tremolo idea |
| MIXED CHOIR | own, in the style of PPG table 27 (choir formants) [the PPG choir of a 1987 synth-pop album, verified]: https://side-line.com/?p=24136 |
| WAVE BRASS | own, in the style of PPG table 26 (brass formants) |
| WAVE STRINGS | own, in the style of PPG table 29 (square to pulse) |
| CLICK BASS | own [the PPG + DX7 bass of a 1985 hit, verified]: https://www.mixonline.com/recording/classic-tracks-tears-fears-everybody-wants-rule-world-365857 |
