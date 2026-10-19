# Bubba Machine's audio

Bubba Machine has one goal: **a hi-fi machine, the best audio quality the FM-1 allows**: studio-grade sound computed live on a small groovebox, with no shortcut you can hear. This page covers what changed in 0.4.0-beta to get there, what each part costs on the device and, just as plainly, **what isn't there yet**.

All CPU figures were measured on the author's FM-1 with the firmware console (`tools/fm1/traza.py`). The details of each measurement are in [`TRAZA_FM1.md`](../TRAZA_FM1.md) and [`MOTORES.md`](../MOTORES.md) (Spanish).

## The device

| | |
|---|---|
| Processor | JieLi AC791N, two cores at 360 MHz with a single-precision FPU |
| Output | external codec over I2S, **24-bit stereo at 44,117.6 Hz** (24 MHz / 544, fixed by hardware) |
| Memory for recorded sound | 100 KiB of flash shared between takes (up to 3 s each) and user sounds |

## What improved

### 1. Everything at 44.1 kHz

In 0.3 the engines ran at 22 kHz and every sample was repeated twice to reach the output. That cuts everything above 11 kHz and leaves images (false highs) above it.

Now **the engines and the mix run at the codec's real rate**, 44,117.6 Hz, with no oversampling. Highs reach 20 kHz and tuning is exact, because it is computed with the true rate, not with 44,100.

### 2. Floating-point mixing

| | 0.3 | 0.4 |
|---|---|---|
| Mix bus | integer, one division per sample and track | **float**, gains computed once per block |
| Silent tracks | still computed | skipped |
| Live effects capture (stutter, reverse, tape stop) | 8-bit μ-law | **16-bit** |
| Delay | 8-bit μ-law | **16-bit** with interpolated reads |
| Mix cost with PLAY | 8.6 % of a core | **7.3 %**, computing twice the samples |

Measured on the device, the FPU is **1.5× faster** than the 64-bit integer math 0.3 used, so moving to float was both faster and cleaner.

### 3. Sounds computed live, no low-resolution recordings

0.3 had drum kits recorded at 22 kHz in 8-bit μ-law (nothing above 11 kHz) and low-resolution wavetables. **They are all gone.** Every sound is now computed on the spot with proven synthesis libraries:

| Engine | What it uses | License |
|---|---|---|
| **Modeled drums** | kick, snare and hi-hat models from **Plaits** (Mutable Instruments), in float, plus our own clap, cowbell, resonator, noise and FM percussion | MIT |
| **B-MINI** | three **polyBLEP** oscillators (anti-aliasing), ladder filter with drive, two envelopes and LFO | own and MIT (Plaits/stmlib) |
| **BITTAL** | tables built from a 256-harmonic spectrum in **7 levels, one per octave**: each note uses the level whose highest harmonic stays below 19 kHz, so there is **no aliasing** on any note; unison, filter and macros | own; factory sounds from freely licensed Vital presets |
| **BUNO-6** | one polyBLEP oscillator per voice, high-pass, 24 dB filter and a two-line modulated **stereo chorus**, like a Juno's | own |
| **6-operator FM** | **msfa**, the engine of Dexed and the same base as the factory firmware; all 32 DX7 algorithms | Apache-2.0 |
| **BB-303** | the TB-303 filter, accent circuit, envelopes and declicker adapted from **Open303** in float, with polyBLEP oscillators | MIT (Open303) |

Accent doesn't just raise the level: it changes the hit, like a real drum machine.

### 4. An output without clicks or steps

- **Soft clipper** with DRIVE (a polynomial, not a hard cut) and a **−1 dBFS ceiling** before the codec.
- The **MASTER volume ramps** inside each block: turning the knob makes no steps.
- When the firmware writes the flash, audio has to stop: it goes out and back with a **5 ms fade**, no click.
- **Sampler recording goes through a Butterworth filter** before dropping to 16 kHz, so highs don't fold back while recording.
- No-interpolation playback (DROP) became **LINEAR**, and the sampler also has **Hermite**.

### 5. Exact timing

The sequencer counts **96 PPQN from the audio sample counter**: there is no separate clock to drift, and every note lands on its exact sample inside the block.

### 6. Performance: what makes room for quality

Computing twice the samples with better engines only fits if the code is fast. What was measured and changed:

| Change | Before | After |
|---|---|---|
| Data cache (SDK setting) | 4 KiB effective | **32 KiB**: random reads 6-10× faster |
| Audio compiled for speed (`-O2`) | demo with PLAY: 36.9 % of a core | **27.7 %** |
| FM, one voice (E.PIANO) | 6.6 % | **3.3 %** |
| ANALOG, one voice | 1757 ns per sample (10 %) | **654 ns (4.1 %)**: 7 voices fit with no late blocks |

Removing the recorded kits and wavetables made the program 148 KiB smaller and freed 64 KiB of RAM.

## What isn't there yet

This is a beta. What limits quality today, plainly:

- **The sampler records at 16 kHz and 8 bits.** Takes share 100 KiB of flash with user sounds (about 6 s in total); at 16 bits it would be half that. With the filter before recording and the dither in `toma.py` it sounds clean, but **it stops around 7 kHz** and has 8-bit noise. It is the weakest point and the next one to solve.
- **The delay runs at 22 kHz:** repeats reach 11 kHz, like a tape delay. That is on purpose, so 1.5 s fits in RAM, but it isn't hi-fi.
- **The clipper has no oversampling:** with very high DRIVE it can alias a little.
- **BB-303 runs Open303 without oversampling:** its cutoff stops at 10 kHz to stay clean at 44.1 kHz.
- **msfa's FM is fixed point**, like the original DX7. That's part of its character, not a flaw, but it isn't float.
- **The reverb (Dattorro) runs at half rate** to fit the budget: its tails reach 11 kHz. No master compressor yet.
- **BUNO-6's chorus and high-pass use approximate values** (Juno-6 times and depth from published measurements); they still need an ear check on the device.
- **Audio on both cores is in testing** (SYSTEM › TWO CORES, off by default): on the computer it gives the same samples, but it still has to be measured on the FM-1.
- **We haven't measured the analog output** (noise, distortion and response of the FM-1's codec and amplifier). Everything above is the digital side.
- **Tested on a single FM-1.**

## What's next

1. **Measure 0.6 on the FM-1:** each machine's load, both cores and even volume across the bank sounds.
2. **Master effects**: tape delay, compressor and leveler.
3. A **higher-quality sampler**, within what the flash allows.
4. **Measure the analog output** with an audio interface.

See also: [sound engines](../MOTORES.md) · [sound research](../INVESTIGACION_SONIDO.md) · [FM-1 measurements](../TRAZA_FM1.md) (Spanish) · [changelog](CHANGELOG.md).
