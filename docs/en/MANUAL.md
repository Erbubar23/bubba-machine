# Bubba Machine 0.3 — Manual

Bubba Machine turns the FM-1 into a 6-track live groovebox: **3 fixed tracks** (drums, sampler and bass) and **3 free ones** (SYNTH, MONO, KIT or MIDI). Every track has its own length and its own speed.

Interface 3.0 fits in one sentence: **four gestures per button, and they mean the same thing on every button. HOME everywhere.**

> **The firmware's screens are in Spanish for now.** Names that appear on screen are written here in CAPITALS exactly as you will see them, with the English meaning next to them, e.g. BORRAR (erase).

> 0.3 changed the format of patterns, settings and samples. **It does not read what 0.2 saved**: on first boot it erases that data and starts with the demo.

## Four gestures per button

| Gesture | When | What it does |
|---|---|---|
| **Tap** | under 300 ms | Opens its page on the track you are on. Again: the next page of the same button |
| **Hold** | 300 ms or more | Its **live layer**: while you hold it, the keys do that job and the screen draws the keyboard with what each key does. Let go and you are back |
| **Hold 2 s** | 2 s without touching anything else | Its settings or its dangerous action. A ring fills at the top; touching anything cancels it. Only on HOME, SEQ, EDIT, SAVE and REC |
| **HOME + button** | with HOME held | The **global version** of the button: for the whole project, not just the track |

There are never two layers at once: with a layer open, holding another button does not open its own.

**Questions.** Anything that erases something asks first: **OCT+ = yes**, **HOME = no** (any other button also cancels).

## The knobs

| Knob | What it does |
|---|---|
| **ALGORITHM** | The track. On HOME a **box** shows the target track and you enter after 400 ms. Inside a track it switches right away and keeps the page. On a list, it changes the highlighted value. With **HOME held**, it picks a track from any screen and you enter when you let go of HOME |
| **SELECT** | The page (the tabs at the top) or the row of a list. With a step held, its accent |
| **PRESETS** | The track's sound (kit, loop A or wave bank). On HOME, the pattern |
| **K1-K4** | Whatever the legend at the bottom says, always in the same color. With HOME held: BPM, swing, master length and drive |

**ALGORITHM priority:** held step > list > track > HOME. The box time is set in GLO or SISTEMA (system): ENTRADA AUTO (auto enter) 0, 250, 400, 800 ms or OFF.

## The screen

- **Top:** transport, pattern (and the queued one), bar and beat, the track, BPM. Also the scene (A-D), **F** while the fill plays, the project pitch when transposed, the 2 s ring and the battery.
- **Inside a track:** its **tabs** below the status bar. The active one is underlined in the track color.
- **Bottom:** what each knob K1-K4 does, in its color and in its physical order.
- **Red border:** the mix is clipping.

## The 14 buttons

| Button | Tap | Again | Hold (layer) | 2 s | HOME + |
|---|---|---|---|---|---|
| **HOME** | HOME: the 6 tracks and the bank | MEZCLA (mixer) › ESCENAS (scenes) › CANCION (song) | **Controller** (see below) | SISTEMA (system) | (it is the modifier) |
| **SEQ** | The track's steps | Notes: LOOP · sampler: RETRIG | **Length and speed** | TIEMPO (time) of the 6 tracks | Song |
| **EDIT** | SONIDO (sound) | Drums: INSTR · TOMAS: CORTAR (slice) | **Erase** | Sampler: TOMAS (takes) · others: initial sound | Copy the track to another |
| **ENV** | ENV (notes) · INSTR (drums) | ENV page 2 | **Accent** | — | — |
| **LFO** | LFO (next version) | — | **Repeat** (grids) · **tie** (notes) | — | — |
| **FX** | The track's MEZCLA (mix) | — | **Punch-in** | — | Global delay and sidechain |
| **SCL** | The track's ESCALA (scale) | ACORDES (chords) | Live **chords** | — | Project scale |
| **ARP** | The track's ARP | — | **Arpeggio** (notes) · **roll** (grids) | — | — |
| **SAVE** | Save menu | Run it | **Copy pattern** | Quick save | Save as |
| **GLO** | Global list | Close | **Pitch** and TAP tempo | — | — |
| **REC** | Record (on the sampler: the recorder) | Looper: overdub › play | Record only while held | Clear the track (asks) | SONG REC |
| **PLAY** | Play / stop | — | — | — | Back to step 1 without stopping |
| **OCT − / +** | Octave · step page · on questions OCT+ = yes | — | — | — | Undo / redo |

## HOME everywhere

The FM-1 has no SHIFT, so HOME does three jobs:

1. **Home screen:** the 6 tracks with their sequence and the bank of 16 patterns. Again: MEZCLA (the 6 levels; ALGORITHM picks a track and K1-K4 mix it), ESCENAS and CANCION.
2. **A controller that follows you:** holding HOME brings the controller to any screen. You edit the bass and, without leaving, mute or queue the next pattern.
3. **Modifier:** HOME + button = its global version (table above).

If you used HOME as a modifier, letting go does **not** take you to the home screen: HOME only navigates on a clean tap.

**Keys with HOME (on the HOME screen, on MEZCLA or while holding HOME):**

| Key | What it does |
|---|---|
| **White 1-16** | Queue that pattern (when stopped: switches right away). Hold one and tap another: **chain** from the first to the second |
| **Black 1-6** | Tap: mute T1-T6. Hold: **solo** while you hold it |
| **Black 7-10** | Launch scenes A-D |
| **Black 11** | FILL while you hold it |

## Tracks: 3 fixed and 3 free

| Track | Type | What it is | Tabs |
|---|---|---|---|
| **T1 BATERIA** (drums) | Fixed | TR grid of 16 instruments (KO-808 or KO-909 kit) | SEQ · SONIDO · INSTR · MEZCLA · MAS |
| **T2 SAMPLER** | Fixed | A loop in 8 slices that a generator rearranges | SEQ · SONIDO · RETRIG · TOMAS · MEZCLA · MAS |
| **T3 BAJO** (bass) | Fixed | Monophonic wavetable with glide | SEQ · SONIDO · ENV · MEZCLA · ARP · LOOP · MAS |
| **T4-T6** | Free | **SYNTH** (polyphonic), **MONO**, **KIT** (another drum machine) or **MIDI** (sends notes over MIDI) | depends on the engine |

A free track's engine is picked in **MAS › MOTOR** (more › engine) with ALGORITHM. Between note engines (SYNTH, MONO, MIDI) the notes stay; switching to KIT or back from KIT asks, because the track starts empty.

**MAS** (more) is the list with everything about the track: engine, accent, length, speed, transpose, MIDI channel and initial sound (on the sampler also: use the loop's BPM, and interpolation). SELECT picks the row and ALGORITHM changes it.

**Black keys follow a single grammar in SEQ:** 1-8 are the track's elements, 9 and 10 modify them and **11 breaks the pattern** (FILL on drums and notes, STUTTER on the sampler).

## Drums (T1 and KIT tracks)

The SEQ screen is a matrix: 16 instruments × the 16 steps of the page. Each line shows its own playhead, because each instrument can have its own length.

| Control | Does |
|---|---|
| **White keys** | Steps of the selected instrument: tap = set or clear |
| **Black 1-8** | Select (and play) an instrument of the section |
| **Black 9** | Switch section (1-8 / 9-16) |
| **Black 10** | Tap: mute the instrument · hold: mutes for all of them (white keys) |
| **Black 11** | FILL while held |
| **OCT − / +** | Page |

**Hold a step + turn:** K1-K4 = P-Lock of TONE, TUNE, DECAY or START on that step only · SELECT = accent · ALGORITHM = repeat ×1 to ×4 or FLAM · PRESETS = condition (SIEMPRE = always, SOLO FILL = only in the fill, SIN FILL = not in the fill).

**INSTR** (EDIT again, or ENV): level, tuning, decay and **line length** of each instrument. For example, a 12-step hi-hat against a 16-step kick.

**SONIDO** (sound): the kit's 16 pads; K1-K3 = tone, tuning and decay, K4 = how much the accent stands out.

## Sampler 3.0 (T2)

The idea comes from X0X's BREAK (a loop in slices that a generator reorders), with a screen that shows what is going on.

- **Slices:** loop A cut in 8. The bars show each slice's energy; **K** and **S** mark where the kick and the snare are.
- **TU row** (yours, what you programmed): empty = silence, outline = **the generator picks**, filled with a number = **pinned slice**. White line on top = accent.
- **SUENA row** (plays): the slice the generator picked for each step of this bar. Orange line = retrig, **<** = reverse, **H** = half speed, **-** = a silence from ROMPER.

| Control | Does |
|---|---|
| **White key** | Silence ↔ generator. On a pinned step, gives it back to the generator |
| **White key held** | The loop plays from that step, whole and looping, while you hold it (the sampler's sequencer goes quiet). Useful to hear where you are before pinning a slice |
| **White held + black 1-8** | Pins that step to that slice |
| **Black 1-8** | Play that slice. With REC held they are written as pinned steps |
| **Black 9 · 10 · 11** | Reverse · half speed · STUTTER, while held |
| **K1-K4** | **VARIAR** (vary: how much the order changes) · **ANCLAR** (anchor: keeps the strong beats, using K and S) · **REPETIR** (repeat: retrigs) · **ROMPER** (break: reverse, half speed, silences and repeats) |
| **EDIT held + black 4** | **CAPTURAR** (capture): what plays in this bar becomes pinned. HOME + OCT− undoes it |

The generator decides each bar with a seed: the SUENA row is exactly what is going to play, and it changes while you turn the knobs. With everything at 0, the loop plays in order.

**RETRIG** (SEQ again): when the generator repeats, it picks 2X, 3X, 4X or 8X with these weights.

**SONIDO** (sound): K1 = LOOP A, K2 = LOOP B, K3 = how many bars use B, K4 = PITCH. The loop follows the tempo: it plays at BPM / the loop's BPM, and the screen says how many semitones it moved. **MAS › USAR BPM DEL LOOP** (use the loop's BPM) sets the project tempo to the loop's. PRESETS also picks loop A.

**Factory loops:** the ROM has BUBBA 160 and BUBBA 128, synthesized in the repository. They don't use take memory.

**The drawing** shows three lanes by frequency: lows at the bottom (orange), mids (grey) and highs at the top (cyan). With heavily compressed music the waveform is a solid block; by bands you can see the kick, the rumble and the hats.

## Recorder (REC on the sampler)

Recording, keeping and slicing are three screens, one job each.

1. **GRABAR** (record): REC on T2 opens the recorder **armed**. It starts at the next bar (K1 = INICIO: COMPAS = bar, or YA = now). A big number says the seconds **left**: the smaller of the recording RAM (3 s) and the free take memory. White lines mark how many bars fit and the amber one is the limit. REC again = stop; it also stops by itself at the limit. Holding REC on the recorder records only while held.
   - Below the number: the free take memory in seconds and KB, and the maximum per take (3 s).
   - While recording **the keys are silent** and the delay and buffer-based punch-ins pause (they share the memory).
   - The take stays **SIN GUARDAR** (unsaved) and already plays as loop A. **It is saved when you stop** the transport, because writing flash stops the audio. HOME = discard.
2. **TOMAS** (takes; EDIT 2 s on T2, or the tab): the library, with the factory loops (FAB) first. ALGORITHM picks; K1-K3 = the take's BPM, bars and gain; white keys 1-8 play its slices. **N1** = use as loop A, **N2** = as loop B, **N11** = delete (asks). At the bottom, the free memory out of 8.1 s.
**How much fits:** each take, up to 3 s (the recording RAM, 48 KB); all together, about 8.1 s (128 KiB of flash, 8 bits at 16 kHz = 16 KB per second). Factory loops don't use that space. The emulator shows the free memory next to the button for loading audio.

3. **CORTAR** (slice; EDIT on a take): **white keys** set or remove a cut on that eighth note, **black 9** puts the cuts on the hits and **black 10** goes back to 8 equal parts. Black 1-8 play each slice. SELECT or K1 pick the cut, K2 moves it, K3 = zoom, K4 = snap to 1/16 or 1/8. It is saved when you leave.

## Note tracks (T3 and SYNTH, MONO, MIDI)

- **SEQ:** the page is only about this track. On top, the page's 16 steps as a **piano roll**: each note at its pitch and with its length; white keys are those steps (up to 4 notes per step). Below, the track's bars and the **dotted tempo follower**. **Hold a step** and play **black 1-8** to write those notes of the track's scale (the same one again removes it). Black 9 and 10 move their octave down or up; 11 = FILL.
- **Free recording, like a looper:** on an **empty** note track, REC records with no fixed length (the screen says REC LIBRE and counts the bars). REC again closes it: the track keeps the whole bars you played (1 to 4) and **every note keeps how long you held it** (up to 4 bars). If the track already has notes, REC records over it with its length.
- **Length in bars:** SEQ held + black 7-10 = 1, 2, 3 or 4 bars.
- **Held step + knobs:** K1-K4 = P-Lock · SELECT = hit (NORMAL › ACENTO = accent › FANTASMA = ghost) · ALGORITHM = probability or fill · PRESETS = length or LIGA (tie, glide).
- **SONIDO** (sound): K1 = wave position, K2-K3 = filter, K4 = accent. PRESETS = wave bank.
- **LOOP (SEQ again):** unquantized looper. K1 ON/OFF · K2 bars (1, 2 or 4). **REC** arms (starts at the bar) › when the loop closes it goes to PLAY › REC again: OVERDUB › REC: PLAY. **REC 2 s** erases the loop.
- **MIDI:** the MIDI engine makes no sound inside: it sends its notes over MIDI on its channel. Its page has channel, program and two CCs (1 and 74).

## The layers (hold)

While you hold, the screen draws the keyboard with what each key does and its LEDs. The sequence keeps playing.

| Hold | Layer | The keys |
|---|---|---|
| **HOME** | Controller | White = queue pattern · black 1-6 = mute / solo · 7-10 = scenes · 11 = FILL. ALGORITHM = go to a track when you let go |
| **SEQ** | Length and speed | White = track length (1-16; OCT−/+ = 17-64) · black 1-6 = speed ×2 to ×1/4 · black 7-10 = 1-4 bars |
| **EDIT** | Erase | Playing: holding a white key erases what passes under the playhead; stopped: erases the line or the step · black 1-3 = copy, paste, clear the track · sampler: black 4 = CAPTURAR · OCT−/+ = undo / redo |
| **ENV** | Accent | White = accent on / off (empty step = new one with accent; on notes: NORMAL › ACENTO › FANTASMA). K1 = how much accent |
| **LFO** | Repeat · tie | Grids: white = ×1 › ×2 › ×3 › ×4 › FLAM. Notes: white = tie the note to the next one (K1 = glide) |
| **FX** | Punch-in | White = 16 effects quantized to 1/16 · black 1-6 still mute |
| **SCL** | Chords | White = 16 chords · black 1-4 = +7, +9, SUS2, SUS4 |
| **ARP** | Arpeggio · roll | Notes: what you play is arpeggiated · grids: white = roll that instrument or slice (K1 = rate; with REC it is recorded as repeats) |
| **SAVE** | Copy pattern | White = paste the pattern there · black (1-6) + white = only that track. If the target has notes, it asks |
| **GLO** | Pitch | The whole keyboard transposes the project (C4 = 0) · **PLAY = TAP tempo** without stopping |

## Time (SEQ 2 s)

Shows the 6 tracks as bars of their real duration, each with its playhead: polymeter at a glance. SELECT or ALGORITHM pick the track; K1 = length (1-64); K2 = speed (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4); K3 and K4 (grids): an instrument and its length.

The **master length** (HOME + K3: 16, 32, 48, 64, 128 or INF) restarts all tracks together. That is where patterns and scenes change.

## Scales and chords

- **ESCALA** (scale, SCL): each note track can have its **own scale** (K2: PROYECTO = project, or one of 9 scales; K1 = root). **HOME + SCL** changes the project scale, used by tracks without their own.
- **ACORDES** (chords, SCL again): 16 chords on a 4×4 grid with their degree and name. The color tells their function: **blue** rest (I, iii, vi), **green** preparation (ii, IV), **orange** tension (V, vii). K1 = set (DIATONICO, PROGRESIONES, NEO SOUL), K2 = voicing (closed, open or voice-led), K3 = octave, K4 = strum.

## Scenes and song

**ESCENAS** (scenes; HOME, HOME, HOME): a scene = pattern + mutes. White 1-4 launch A-D at the next master cycle (when stopped: right away); 5-8 store what is playing into A-D; hold one of 1-4 and tap others for a **chain**. From any screen, with HOME, black 7-10 also launch them. **REC** on ESCENAS (or HOME + REC) = SONG REC.

**CANCION** (song; HOME + SEQ): a white key adds a pattern, black 9 deletes; K1 = step, K2 = repeats, K4 = song ON/OFF.

## Effects

- **Each track's MEZCLA** (mix; FX): level, pan, delay send and **duck** (sidechain from the T1 kick; the sampler can duck too).
- **HOME + FX:** delay time, feedback and mix, and sidechain release.
- **FX held:** 16 punch-ins (stutter, reverse, tape stop, gate, sweeps, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (HOME + K4): saturation of the mix.

## Saving

- The flash is **only written when stopped** (writing pauses the audio for a moment).
- **Autosave:** when stopped, after 10 s without touching anything.
- **SAVE:** save · save as · save song and scenes · erase pattern (SAVE twice) · count events. **SAVE 2 s:** quick save. **HOME + SAVE:** save as.
- **Copying:** SAVE held + white key copies the pattern; HOME + EDIT copies a track onto another one in the same pattern (ALGORITHM picks the target, OCT+ pastes; a free track changes engine if needed).

## System (HOME 2 s)

Internal or external clock · Split Sync (L = pulse, R = mono mix) · pulse PPQN and latency · autosave · idle LED brightness · the box's auto enter time · saved patterns · takes and free memory · version.

## MIDI

- **Input** (USB and TRS): channels 1-6 = Bubba Machine's tracks. On grids, the note picks the instrument (0-15); on the sampler, the slice.
- **External clock:** follows MIDI Clock and Start/Stop.
- **Output:** MIDI Clock when the clock is internal, and the notes of tracks with the MIDI engine.
