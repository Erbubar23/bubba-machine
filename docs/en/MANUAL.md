# Bubba Machine 0.2 — Manual

Bubba Machine turns the FM-1 into a 6-track live groovebox. It has a TR-style drum machine, a sampler whose chops are sequenced just like the drums, and a bass and synths with a looper. Every track can have its own length and its own speed.

> **The firmware's screens are in Spanish for now.** Names that appear on screen are written here in CAPITALS exactly as you will see them, with the English meaning next to them, e.g. BORRAR (erase).

## Three ways to press a button

| Press | What it does |
|---|---|
| **Tap** | Opens the button's screen. Tap again: its next page |
| **Hold** (300 ms) | Opens its **live layer** while you hold it. When you let go, you are back where you were |
| **Hold 2 s** | Opens a **special screen**. A yellow ring at the top fills while you wait. If you touch anything else during the press, the 2 s are cancelled |

## The screen

- **Top:** transport, pattern (and the queued one), bar and beat, track, BPM. Also the scene that is playing (A-D), **F** while the fill plays, the 2 s ring and the battery.
- **Middle:** whatever the current mode does. Values you turn appear here on top and go away by themselves.
- **Bottom:** what each knob K1-K4 does, in its color and in its physical order.
- **Red border:** the mix is clipping.

## The tracks

| Track | What it is |
|---|---|
| **T1 DRUMS** | TR drums: 16 instruments (KO-808 or KO-909 kit), each with its own step line |
| **T2 SAMPLER** | 16 chops of a sample (slot A or B). Sequenced just like the drums |
| **T3 BASS** | Monophonic wavetable with filter and glide |
| **T4-T6 SYN** | Polyphonic wavetable with filter |

**ALGORITHM** changes track from any screen.

## Buttons

| Button | Tap | Again | Hold | 2 s |
|---|---|---|---|---|
| **HOME** | Tracks and mixer | **Scenes** (ESCENAS) | Mute (white keys 1-6) and solo (9-14) | **System** (SISTEMA) |
| **SEQ** | Steps | Synth: **looper** | Patterns (white key = next) | **Time** (TIEMPO) |
| **EDIT** | Sound (kit, sample or wave) | TR: instrument | TR-style **erase** (BORRAR) | T2: **sampler** · others: initial sound |
| **ENV** | Envelope | Page 2 | — | — |
| **FX** | Delay and sidechain | — | **Punch-in** | — |
| **SCL** | Scale | **Chord grid** | Live chords | — |
| **ARP** | Arpeggiator | — | Synth: arpeggio · TR: **roll** | — |
| **SAVE** | Save menu | Run it | — | Quick save |
| **GLO** | Global (BPM, swing, master length, drive, fills) | — | Modifier | — |
| **REC** | Live record (in LOOP: looper · in the sampler: record) | — | — | **Clear the track** |
| **PLAY** | Play/stop | — | — | — |
| **OCT − / +** | Octave · in SEQ: page · in BORRAR: undo / redo | — | — | — |

**Hold GLO +:** KNOB 1-4 = BPM · swing · master length · drive · SELECT = automatic fill · ALGORITHM = kit / slot / wave bank · SEQ = song · OCT = transpose · black keys 1-6 = mute.

## TR drums (SEQ on T1 and T2)

The screen is a matrix: 16 instruments × the 16 steps of the page. Each line shows its own playhead, because each instrument can have its own length.

| Control | Does |
|---|---|
| **White keys** | Steps of the selected instrument: tap = set or clear |
| **Black keys 1-8** | Select (and play) an instrument of the section |
| **Black key 9** | Switches section (1-8 / 9-16) |
| **Black key 10** | FILL while you hold it |
| **Black key 11** | Tap: mute the instrument · hold: mutes for all of them |
| **OCT − / +** | Page |
| **SELECT** | Instrument |

**Hold a step + turn a knob:**
- KNOB 1-4: P-Lock of TONE, TUNE, DECAY or START, on that step only.
- SELECT: accent.
- ALGORITHM: repeat ×1 to ×4, or FLAM.
- PRESETS: condition (SIEMPRE = always, SOLO FILL = only in the fill, SIN FILL = not in the fill).

**EDIT again (INSTRUMENTO):** level, tuning, decay and **line length**. For example, a 12-step hi-hat against a 16-step kick.

**BORRAR (hold EDIT):**
- While playing, holding a white key erases that instrument on every step the playhead passes. When stopped, it erases the whole line.
- Black keys 1-3: copy, paste and clear the track.
- OCT − / +: undo and redo.

**ROLL (hold ARP):** each white key repeats its instrument at the K1 rate (1/8, 1/16, 1/32, triplets). With REC on, it is recorded as steps with repeats.

**Fills:** steps with a condition play or stay silent in the fill. The fill plays while you hold black key 10, or by itself every 2, 4 or 8 bars (GLO + SELECT).

## Time (SEQ 2 s)

Shows the 6 tracks as bars of their real duration, each with its playhead: polymeter at a glance.

- SELECT or ALGORITHM: choose the track.
- K1: length (1-64).
- K2: speed (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4).
- K3 and K4 (TR tracks only): choose an instrument and change its length.

The **master length** (GLO + K3: 16, 32, 48, 64, 128 or INF) restarts all tracks together. That is where patterns and scenes change.

## Sampler (T2)

1. Select T2 and open **EDIT**.
2. **Hold REC:** records whatever is playing (up to 3 s, 16 kHz).
3. While recording, each **white key** marks a chop. Without chops, the sample is split into 16 equal parts.
4. When you **stop**, the sample is saved to its slot by itself (A or B, up to 4 s each). Until it is saved, the delay and the buffer-based punch-ins are paused, because they share the memory.
5. The chops are the 16 instruments of T2's TR grid.

**Factory loops:** besides slots A and B, T2 has read-only loops in ROM (F1, F2…). The demo starts on F1. If you record while a factory loop is selected, the recording goes to slot A. Factory loops are always cut into 16 equal chops and cannot be erased.

**The sample drawing** shows three lanes by frequency: lows at the bottom (orange), mids (grey) and highs at the top (cyan). With heavily compressed music the waveform is a solid block; by bands you can see the kick, the rumble and the hats.

**SAMPLER (EDIT 2 s on T2):**
- K1: source (slot A, slot B or a factory loop).
- K2: manual chops or 4, 8 or 16 equal parts.
- Black key 11 twice: erases the slot.

## Synths

- **SEQ:** up to 4 notes per step. Hold a step: KNOB = P-Lock, SELECT = note, ALGORITHM = probability or fill, PRESETS = length or TIE (glide).
- **LOOP (SEQ again):** unquantized note looper.
  - K1 ON/OFF · K2 bars (1, 2 or 4).
  - **REC** arms recording (it starts at the next bar). When the loop closes it switches to PLAY. REC again: overdub. REC again: PLAY.
  - **REC 2 s** erases the loop.

## Chords: the grid (SCL again)

The 16 white keys are 16 chords, drawn as a 4×4 grid (row 1 = white keys 1-4). Each pad shows the chord's **degree** (I, ii, V7…) and **name**. Its color tells its function:
- **Blue (rest):** I, iii, vi.
- **Green (preparation):** ii, IV.
- **Orange (tension):** V, vii.

It reads like a map: green to orange to blue sounds like "going and coming back".

- **K1:** chord set.
  - DIATÓNICO (diatonic): triads and sevenths.
  - PROGRESIONES (progressions): each row is a classic progression, left to right.
  - NEO SOUL: sevenths and ninths.
- **K2:** voicing (closed, open or voice-led: each chord in the inversion closest to the previous one).
- **K3:** octave · **K4:** strum.
- **Black keys 1-4** (while held): +7, +9, SUS2, SUS4.
- Chords play on the selected synth track (or on T4 if you are on a TR track).
- **Hold SCL** turns on the chord grid on the keys from any screen.

## Scenes (HOME again)

A scene = **pattern + mutes** (of the tracks and of the TR instruments).

- White keys 1-4: launch A-D at the next master cycle (when stopped: right away).
- White keys 5-8: store what is playing into A-D.
- Hold one of 1-4 and tap others: looping **chain**.
- **REC:** SONG REC. Every cycle that plays is written into the song (GLO + SEQ to view and edit it).

## Effects

- **FX:** delay time, feedback, mix and sidechain release. Each track's delay send and ducking are set in HOME (K3, K4).
- **Hold FX + white key:** 16 punch-ins quantized to 1/16 (stutter, reverse, tape stop, gate, sweeps, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (GLO + K4): saturation of the mix.

## Saving

- The flash is **only written when stopped** (writing pauses the audio for a moment).
- **Autosave:** when stopped, after 10 s without touching anything.
- **SAVE:** save · save as · save song and scenes · erase pattern (SAVE twice) · count events.
- **SAVE 2 s:** quick save.

## System (HOME 2 s)

Internal or external clock · Split Sync (L = pulse, R = mono mix) · pulse PPQN and latency · autosave · idle LED brightness · sampler source · saved patterns · samples · version.

## MIDI

- Input (USB and TRS): channels 1-6 = tracks. On T1 and T2, the note selects the instrument (0-15).
- External clock: follows MIDI Clock and Start/Stop.
- Output: MIDI Clock when the clock is internal.
