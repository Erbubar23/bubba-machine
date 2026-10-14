# Bubba Machine 0.3 — Manual

Bubba Machine turns the FM-1 into a 6-track live groovebox: **3 fixed tracks** (drums, sampler and bass) and **3 free ones** (ANALOG, FM, ACID, KIT or MIDI). Every track has its own length and its own speed.

Interface 3.0 fits in one sentence: **four gestures per button, and they mean the same thing on every button. HOME everywhere.**

> **The screen is in English.** Names that appear on screen are written here in CAPITALS exactly as you will see them. Other languages are packs you can load (see [Screen language](#screen-language)).

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
| **ALGORITHM** | The track. On HOME a **box** shows the target track and you enter after 400 ms. Inside a track it also shows the box and enters keeping the page; in **INSTR** it steps through the 16 instruments (big number, name and model) and plays the one you land on. On a list, it changes the highlighted value. With **HOME held**, it picks a track from any screen and you enter when you let go of HOME |
| **SELECT** | The page (the tabs at the top) or the row of a list. With a step held, its accent |
| **PRESETS** | The track's sound (kit, loop A or engine preset). On HOME, the pattern |
| **K1-K4** | Whatever the legend at the bottom says, always in the same color. With HOME held: BPM, swing, master length and drive |

**ALGORITHM priority:** held step > list > track > HOME. The box time is set in GLO or SYSTEM: AUTO ENTER 0, 250, 400, 800 ms or OFF.

## The screen

- **Top:** transport, pattern (and the queued one), bar and beat, the track, BPM. Also the scene (A-D), **F** while the fill plays, the project pitch when transposed, the 2 s ring and the battery.
- **Inside a track:** its **tabs** below the status bar. The active one is underlined in the track color.
- **Bottom:** what each knob K1-K4 does, in its color and in its physical order.
- **Red border:** the mix is clipping.

## The 14 buttons

| Button | Tap | Again | Hold (layer) | 2 s | HOME + |
|---|---|---|---|---|---|
| **HOME** | HOME: the 6 tracks and the bank | MIX › SCENES › SONG | **Controller** (see below) | SYSTEM | (it is the modifier) |
| **SEQ** | The track's steps | Notes: LOOP · sampler: BREAK | **Length and speed** | TIME of the 6 tracks | Song |
| **EDIT** | SOUND (sampler: PADS) | Drums: INSTR · PADS: the next page | **Erase** | Sampler: TAKES · others: initial sound | Copy the track to another |
| **ENV** | ENV (notes) · INSTR (drums) | ENV page 2 | **Accent** | — | — |
| **LFO** | LFO (next version) | — | **Repeat** (grids) · **tie** (notes) | — | — |
| **FX** | The track's MIX | — | **Punch-in** | — | Global delay and sidechain |
| **SCL** | The track's SCALE | CHORDS | Live **chords** | — | Project scale |
| **ARP** | The track's ARP | — | **Arpeggio** (notes) · **roll** (grids) | — | — |
| **SAVE** | Save menu | Run it | **Copy pattern** | Quick save | Save as |
| **GLO** | Global list | Close | **Pitch** and TAP tempo | — | — |
| **REC** | Record (on the sampler: the recorder) | Looper: overdub › play | Record only while held | Clear the track (asks) | SONG REC |
| **PLAY** | Play / stop | — | — | — | Back to step 1 without stopping |
| **OCT − / +** | Octave · step page · on questions OCT+ = yes | — | — | — | Undo / redo |

## HOME everywhere

The FM-1 has no SHIFT, so HOME does three jobs:

1. **Home screen:** the 6 tracks with their sequence and the bank of 16 patterns. Again: MIX (the 6 levels; ALGORITHM picks a track and K1-K4 mix it), SCENES and SONG.
2. **A controller that follows you:** holding HOME brings the controller to any screen. You edit the bass and, without leaving, mute or queue the next pattern.
3. **Modifier:** HOME + button = its global version (table above).

If you used HOME as a modifier, letting go does **not** take you to the home screen: HOME only navigates on a clean tap.

**Keys with HOME (on the HOME screen, on MIX or while holding HOME):**

| Key | What it does |
|---|---|
| **White 1-16** | Queue that pattern (when stopped: switches right away). Hold one and tap another: **chain** from the first to the second |
| **Black 1-6** | Tap: mute T1-T6. Hold: **solo** while you hold it |
| **Black 7-10** | Launch scenes A-D |
| **Black 11** | FILL while you hold it |

## Tracks: 3 fixed and 3 free

| Track | Type | What it is | Tabs |
|---|---|---|---|
| **T1 DRUMS** | Fixed | TR grid of 16 **modeled** instruments (computed live): kits 808, 909 and SYNTH | SEQ · SOUND · INSTR · MIX · MORE |
| **T2 SAMPLER** | Fixed | 16 pads made from your takes, on a grid | SEQ · PADS · BREAK · TAKES · MIX · MORE |
| **T3 BASS** | Fixed | **ANALOG** (one voice, with glide), **ACID** or **FM** | SEQ · SOUND · MIX · ARP · LOOP · MORE |
| **T4-T6** | Free | **ANALOG**, **FM**, **ACID**, **KIT** (another drum machine) or **MIDI** (sends notes over MIDI) | depends on the engine |

A free track's engine (and T3's: ANALOG, ACID or FM) is picked in **MORE › ENGINE** with ALGORITHM. Between note engines (ANALOG, FM, ACID, MIDI) the notes stay; switching to KIT or back from KIT asks, because the track starts empty.

**MORE** is the list with everything about the track: engine, accent, length, speed, transpose, MIDI channel and initial sound (on the sampler also: interpolation). SELECT picks the row and ALGORITHM changes it.

**Black keys follow a single grammar in SEQ:** 1-8 are the track's elements, 9 and 10 modify them and **11 breaks the pattern** (FILL).

## The sound engines

| Engine | What it is | K1-K4 on SEQ | PRESETS |
|---|---|---|---|
| **ANALOG** | Virtual analog: two variable-shape oscillators (SAW, SQUARE with pulse width, TRIANGLE, folded SINE, 3 SAWS), sync, ring, sub and noise; LP 24/12, BP or HP filter with resonance and drive; filter and amp ADSR, LFO and POLY, MONO or LEGATO with glide | CUTOFF · RESO · ENV AMT · SHAPE | 16 presets |
| **FM** | 6 operators, DX7-compatible (msfa, the core of Dexed) | BRIGHT · FEEDBACK · DECAY · ALGORITHM | 16 original patches |
| **ACID** | 303-style mono: 4-pole ladder filter, accent opens the filter, slide with TIE | CUTOFF · RESO · DECAY · ENV MOD | Saw or square |
| **Drums** | 16 modeled instruments, each with its own **model**: BD 808, BD 909, SD 808, SD 909, HAT 808, METAL (hats and cymbals), clap, TOM (and congas), cowbell, RESON (rim, clave, block), noise (maracas) or FM (zaps, metal) | TONE · TUNE · DECAY · COLOR | Kits 808, 909 and SYNTH |

Since 0.4 everything is computed live: no recorded sounds are left. Kicks, snares and hats are the **Plaits** models (Mutable Instruments).

**Editing ANALOG:** SOUND is a paged editor (OSC 1, OSC 2, CROSS, FILTER, FILTER +, FILTER ENV, AMP ENV, LFO, VOICE). K1-K4 change the 4 settings of the page, SELECT changes page (at the edges it moves to the next tab), EDIT again = next page, ENV jumps to the envelopes and LFO to the LFO. PRESETS loads a whole preset.

**Editing a drum instrument:** INSTR has two pages: HIT (TUNE, DECAY, TONE and COLOR, named after what it does in that model) and INSTRUMENT (MODEL, LEVEL, CHOKE group and line LENGTH).

## Drums (T1 and KIT tracks)

The SEQ screen is a matrix: 16 instruments × the 16 steps of the page. Each line shows its own playhead, because each instrument can have its own length.

| Control | Does |
|---|---|
| **White keys** | Steps of the selected instrument: tap = set or clear |
| **Black 1-8** | Select (and play) an instrument of the section |
| **Black 9** | Switch section (1-8 / 9-16) |
| **Black 10** | **MUTES** screen: a tap keeps it open (white keys mute or unmute each instrument; leave with black 10 or HOME); hold = only while held. A muted instrument turns red |
| **Black 11** | **FILL** screen: while open the fill plays and the white keys set a hit **only for the fill** (again: removes it). K1 = automatic fill (every 2, 4 or 8 bars). Tap = stays open; hold = only while held |
| **OCT − / +** | Page |

**Hold a step + turn:** K1-K4 = P-Lock of TONE, TUNE, DECAY or START on that step only · SELECT = accent · ALGORITHM = repeat ×1 to ×4 or FLAM · PRESETS = condition (ALWAYS, FILL ONLY, NO FILL).

**INSTR** (EDIT again, or ENV): level, tuning, decay and **line length** of each instrument. For example, a 12-step hi-hat against a 16-step kick.

**SOUND:** the kit's 16 pads; K1-K3 = tone, tuning and decay, K4 = how much the accent stands out.

## Pad sampler (T2)

T2 is a **16-pad sampler**, like a KO II or the drum sampler of an OP-1 field: each pad is a piece of one of your takes, and a grid just like the drums plays them.

**How you work:**

1. **Record** a take (REC on T2, see [Recorder](#recorder-rec-on-the-sampler)) or upload it from the computer.
2. **The take builds the kit:** if the kit is empty, when the take is saved it is cut on the hits and each cut goes to a pad. The grid is written so it plays like the take; you change it from there.
3. In **TAKES** you choose which take goes to which pad: **N1** builds a new kit on the hits, **N2** in 16 equal parts, **N3** in 8, and **N4** puts the whole take on the selected pad. A kit can mix several takes.
4. In **PADS** you tune each pad: where it starts and ends, its pitch, its level, how it answers the key and its envelope.
5. In **SEQ** you program the pads like the drums, and **BREAK** varies them bar by bar.

The tabs: **SEQ · PADS · BREAK · TAKES · MIX · MORE**. The kit lives in the pattern, like the drum kit; the takes, in the library.

### SEQ: the pad grid

Just like the drums: one line per pad, 8 on screen. Each pad has its own length.

| Control | Does |
|---|---|
| **White keys** | Steps of the selected pad: tap = set or clear |
| **Black 1-8** | Select and play a pad of the section |
| **Black 9 · 10 · 11** | Other section (pads 1-8 / 9-16) · MUTES · FILL |
| **K1-K4** | **FILTER** (low-pass) · **PITCH** (±12 semitones) · **DECAY** · **START** (shifts the start of every pad) |
| **Held step + K1-K4** | P-Lock on that step: a pad higher, shorter or from further in, only there |

The layers work as on the drums: accent, repeat, erase, roll (ARP held) and per-line lengths in TIME.

### PADS (EDIT): each pad's editor

At the top, the **map** of the pad's take, with every pad of that take marked and the zoom box. In the middle, the big **waveform**, with the pad in **cyan brackets** and the other pads of the take with their number. At the bottom, **one cell per white key**, in the same place as the key: filled = it has sound, outlined = the selected pad. The orange line is what is playing.

**EDIT again** changes page: **TRIM › SOUND › ENV** (the current one is yellow, top right). ENV also opens the ENV page.

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| **TRIM** | START (the end stays) | END (the length) | ZOOM X1-X8 | SNAP: OFF, 1/16, 1/8 |
| **SOUND** | PITCH (±24 st) | LEVEL | PLAY: ONESHOT, KEY, THRU, LOOP | DIR: FWD or REV |
| **ENV** | ATTACK | RELEASE | CHOKE group | TEMPO: follows the BPM or not |

**How the key answers (PLAY):**

- **ONESHOT:** plays whole even if you let go.
- **KEY:** plays while you hold.
- **THRU:** plays the pad and, held, **keeps going through the rest of the take**.
- **LOOP:** repeats the pad while you hold.

From the grid, KEY and LOOP play for one step.

| Key | Does |
|---|---|
| **White key** | Plays its pad and selects it. With REC + PLAY, the hit goes on its line |
| **N1 · N2 · N3** | CPY copies the pad · PST pastes it on the selected one · CLR empties it |
| **N4 HIT** | Moves the start to the nearest hit |
| **N9 · N10 · N11** | Reverse · half speed · STUTTER, while held |
| **PRESETS** | Another whole take on the selected pad |

**TEMPO:** a pad that follows the tempo plays at the project BPM / the take's BPM (like a chopped loop). Kits built from a take have it on, and their pads choke each other (group 1), like the original loop.

### BREAK (SEQ again)

The idea of X0X's BREAK, on your grid: every bar varies what plays with a seed. **With the four knobs at 0, the grid plays as is** (the default).

- **YOU** row: the pad you programmed on each step.
- **PLAYS** row: what is going to play this bar. In yellow, the steps that changed. Orange line = repeat, **<** = reverse, **H** = half speed, **-** = silence.

| Knob | Does |
|---|---|
| **VARY** | How many steps play another pad of the kit |
| **ANCHOR** | The strong beats (1, 5, 9, 13) stay as they are |
| **REPEAT** | Retrigs from 2X to 8X |
| **BREAK** | Reverse, half speed, silences and repeating the previous step |

**EDIT held + black 4 (CAPT)** writes into the grid what BREAK is doing in this bar. HOME + OCT− undoes it.

**The drawing** is the mirrored waveform, as on a sampler: the peak in the dark shade and the energy (RMS) on top, in full color.

## Recorder (REC on the sampler)

Recording, keeping and building the kit are separate steps.

1. **RECORD:** REC on T2 opens the recorder **armed** (on PADS, REC records what you play into the grid). It starts at the next bar (K1 = START: BAR or NOW). A big number says the seconds **left**: the smaller of the recording RAM (3 s) and the free take memory. White lines mark how many bars fit and the amber one is the limit. REC again = stop; it also stops by itself at the limit. Holding REC on the recorder records only while held.
   - Below the number: the free take memory in seconds and KB, and the maximum per take (3 s).
   - While recording **the keys are silent** and the delay and buffer-based punch-ins pause (they share the memory).
   - The take stays **UNSAVED**: the white keys play it in 16 parts. **It is saved when you stop** the transport, because writing flash stops the audio, and if the kit was empty, it builds it. HOME = discard.
2. **TAKES** (EDIT 2 s on T2, or the tab): the library, with the waveform of the marked take. ALGORITHM picks; K1-K3 = the take's BPM, bars and gain; white keys play it in 16 parts (held: it keeps going). **N1** = kit on the hits, **N2** = 16 equal parts, **N3** = 8, **N4** = the whole take on the selected pad, **N11** = delete (asks; its pads become empty). If the kit already has pads, building a new one asks. A yellow dot = the kit uses that take.

**How much fits:** for now, one take of up to 1.5 s (24 KiB of flash, 8 bits at 16 kHz = 16 KB per second). The emulator shows the free memory next to the button for loading audio.

## Note tracks (T3, ANALOG, FM, ACID, MIDI)

- **SEQ:** the page is only about this track. On top, the page's 16 steps as a **piano roll**: each note at its pitch and with its length; white keys are those steps (up to 4 notes per step). Below, the track's bars and the **dotted tempo follower**. **Hold a step** and play **black 1-8** to write those notes of the track's scale (the same one again removes it). Black 9 and 10 move their octave down or up; 11 = FILL.
- **Free recording, like a looper:** on an **empty** note track, REC records with no fixed length (the screen says FREE REC and counts the bars). REC again closes it: the track keeps the whole bars you played (1 to 4) and **every note keeps how long you held it** (up to 4 bars). If the track already has notes, REC records over it with its length.
- **Length in bars:** SEQ held + black 7-10 = 1, 2, 3 or 4 bars.
- **Held step + knobs:** K1-K4 = P-Lock · SELECT = hit (NORMAL › ACCENT › GHOST) · ALGORITHM = probability or fill · PRESETS = length or TIE (glide).
- **SOUND:** on ANALOG, the paged editor (above). On FM and ACID, K1-K3 = the engine macros and K4 = accent; their envelopes are in ENV.
- **LOOP (SEQ again):** unquantized looper. K1 ON/OFF · K2 bars (1, 2 or 4). **REC** arms (starts at the bar) › when the loop closes it goes to PLAY › REC again: OVERDUB › REC: PLAY. **REC 2 s** erases the loop.
- **MIDI:** the MIDI engine makes no sound inside: it sends its notes over MIDI on its channel. Its page has channel, program and two CCs (1 and 74).

## The layers (hold)

While you hold, the screen draws the keyboard with what each key does and its LEDs. The sequence keeps playing.

| Hold | Layer | The keys |
|---|---|---|
| **HOME** | Controller | White = queue pattern · black 1-6 = mute / solo · 7-10 = scenes · 11 = FILL. ALGORITHM = go to a track when you let go |
| **SEQ** | Length and speed | White = track length (1-16; OCT−/+ = 17-64) · black 1-6 = speed ×2 to ×1/4 · black 7-10 = 1-4 bars |
| **EDIT** | Erase | Playing: holding a white key erases what passes under the playhead; stopped: erases the line or the step · black 1-3 = copy, paste, clear the track · sampler: black 4 = CAPT (what BREAK does goes into the grid) · OCT−/+ = undo / redo |
| **ENV** | Accent | White = accent on / off (empty step = new one with accent; on notes: NORMAL › ACCENT › GHOST). K1 = how much accent |
| **LFO** | Repeat · tie | Grids: white = ×1 › ×2 › ×3 › ×4 › FLAM. Notes: white = tie the note to the next one (K1 = glide) |
| **FX** | Punch-in | White = 16 effects quantized to 1/16 · black 1-6 still mute |
| **SCL** | Chords | White = 16 chords · black 1-4 = +7, +9, SUS2, SUS4 |
| **ARP** | Arpeggio · roll | Notes: what you play is arpeggiated · grids: white = roll that instrument or pad (K1 = rate; with REC it is recorded as repeats) |
| **SAVE** | Copy pattern | White = paste the pattern there · black (1-6) + white = only that track. If the target has notes, it asks |
| **GLO** | Pitch | The whole keyboard transposes the project (C4 = 0) · **PLAY = TAP tempo** without stopping |

## Time (SEQ 2 s)

Shows the 6 tracks as bars of their real duration, each with its playhead: polymeter at a glance. SELECT or ALGORITHM pick the track; K1 = length (1-64); K2 = speed (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4); K3 and K4 (grids): an instrument and its length.

The **master length** (HOME + K3: 16, 32, 48, 64, 128 or INF) restarts all tracks together. That is where patterns and scenes change.

## Scales and chords

- **SCALE** (SCL): each note track can have its **own scale** (K2: PROJECT, or one of 9 scales; K1 = ROOT). **HOME + SCL** changes the project scale, used by tracks without their own.
- **CHORDS** (SCL again): 16 chords on a 4×4 grid with their degree and name. The color tells their function: **blue** rest (I, iii, vi), **green** preparation (ii, IV), **orange** tension (V, vii). K1 = set (DIATONIC, PROGRESSIONS, NEO SOUL), K2 = voicing (CLOSED, OPEN or LED, voice-led), K3 = octave, K4 = strum.

## Scenes and song

**SCENES** (HOME, HOME, HOME): a scene = pattern + mutes. White 1-4 launch A-D at the next master cycle (when stopped: right away); 5-8 store what is playing into A-D; hold one of 1-4 and tap others for a **chain**. From any screen, with HOME, black 7-10 also launch them. **REC** on SCENES (or HOME + REC) = SONG REC.

**SONG** (HOME + SEQ): a white key adds a pattern, black 9 deletes; K1 = step, K2 = repeats, K4 = song ON/OFF.

## Effects

- **Each track's MIX** (FX): level, pan, delay send and **duck** (sidechain from the T1 kick; the sampler can duck too).
- **HOME + FX:** delay time, feedback and mix, and sidechain release.
- **FX held:** 16 punch-ins (stutter, reverse, tape stop, gate, sweeps, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (HOME + K4): saturation of the mix.

## Saving

- The flash is **only written when stopped** (writing pauses the audio for a moment).
- **Autosave:** when stopped, after 10 s without touching anything.
- **SAVE:** save · save as · save song and scenes · erase pattern (SAVE twice) · count events. **SAVE 2 s:** quick save. **HOME + SAVE:** save as.
- **Copying:** SAVE held + white key copies the pattern; HOME + EDIT copies a track onto another one in the same pattern (ALGORITHM picks the target, OCT+ pastes; a free track changes engine if needed).

## System (HOME 2 s)

Internal or external clock · Split Sync (L = pulse, R = mono mix) · pulse PPQN and latency · autosave · idle LED brightness · the box's auto enter time · saved patterns · takes and free memory · **LANGUAGE** · version.

## MIDI

- **Input** (USB and TRS): channels 1-6 = Bubba Machine's tracks. On grids, the note picks the instrument (0-15); on the sampler, the pad.
- **External clock:** follows MIDI Clock and Start/Stop.
- **Output:** MIDI Clock when the clock is internal, and the notes of tracks with the MIDI engine.

## Screen language

The screen is in **English**. Other languages are **packs** that are not loaded by default: one pack lives in its own 16 KiB of flash (it does not touch patterns or takes, and a firmware update does not erase it).

- **Load Spanish on the FM-1:** with the transport stopped, `py -3 tools/fm1/idioma.py lang/es.bmlang`. **SYSTEM › LANGUAGE** then switches between English and the pack. `--quitar` removes it.
- **In the emulator:** the button under *Sampler and memory* loads or removes the Spanish pack.
- **Making a translation:** copy `lang/plantilla.txt` (every phrase on screen) to `lang/<code>.txt`, write the translation under each phrase and build it with `py -3 tools/gen_lang.py`. The FM-1 font is ASCII: no accents. What you don't translate shows in English.
