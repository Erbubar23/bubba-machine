# Bubba Machine 0.8 — Manual

Bubba Machine turns the FM-1 into a live groovebox with **six tracks**, each one a machine named after the one that inspired it. The drums (**BR-8**) and the pad sampler (**BP-1**) are fixed; **T3-T6 choose their instrument** among **BB-303** (acid bass), **B-MINI** (three-oscillator mono), **BX-7** (6-operator FM), **BITTAL** (wavetable that loads Vital presets) and **BUNO-6** (polysynth with chorus). HOME holds the **BX-1** mixer. Every track has its own length and its own speed.

> The names recall famous machines (TR-8, OP-1, TB-303, Minimoog, DX7, Vital, Juno-6), but Bubba Machine has no relation to their brands or makers.

The interface fits in two sentences: **four gestures per button, and they mean the same thing on every button. HOME everywhere.**

**Every machine is its own instrument.** On HOME you are at the BX-1 console. Put the cursor on a track and enter it (SEQ or EDIT): from then on **the whole FM-1 belongs to that machine**. Its keys, buttons, knobs and lights work like the real machine it is named after. **HOME is the way out**, back to the console. A global screen you open from inside a machine (TEMPO, SAVE, TIME, SCALE, SONG, SYSTEM) takes you back to that machine when you leave it.

> **The screen is in English.** Names that appear on screen are written here in CAPITALS exactly as you will see them. Other languages are packs you load from the [control platform](../../web/control.html) (see [Screen language](#screen-language)).

> 0.6 changed the pattern format. **It does not read what an earlier version saved**: on first boot it erases the patterns and starts with the demo (takes are kept).

## Four gestures per button

| Gesture | When | What it does |
|---|---|---|
| **Tap** | under 300 ms | Opens its page on the track you are on. Again: the next page of the same button |
| **Hold** | 300 ms or more | Its **live layer**: while you hold it, the keys do that job and the screen draws the keyboard with what each key does. Let go and you are back |
| **Hold 2 s** | 2 s without touching anything else | Its settings or its dangerous action. A ring fills at the top with the name of what it opens; touching anything cancels it. Only on HOME, SEQ, EDIT, SAVE and REC |
| **HOME + button** | with HOME held | The **global version** of the button: for the whole project, not just the track |

There are never two layers at once: with a layer open, holding another button does not open its own.

**Questions.** Anything that erases something asks first: **OCT+ = yes**, **HOME = no** (any other button also cancels). While a question is open, only those buttons are lit: OCT+ blinks, HOME is on.

## The lights

The FM-1's lights are white only: they never say *what* a key or button does (the screen does), only its state. The same six levels mean the same thing on every screen:

| Light | Keys | Buttons |
|---|---|---|
| **Off** | Does nothing here: no function, an empty instrument, outside the length | Does nothing on this screen |
| **Dim** | Available but empty, or has something but is silenced (a muted instrument or track) | Available |
| **Medium** | Has something and will play: an instrument with steps, a saved pattern, the root note | Has a special job right now (with HOME held, the ones that have a HOME + function) |
| **On** | Selected, held, active now, or **a flash on every hit** | Where you are, or a state that stays (octave shifted, note REC) |
| **Blink** | The playhead, or something queued | Needs you now: OCT+ on a question, REC capturing, HOME with a pattern or scene in the queue |
| **Breath** | A step with P-Lock, the EXIT key of a layer opened with a tap | Armed or pending: REC armed, the layer you hold, SAVE with unsaved changes (on the console), PLAY waiting for external clock |

**PLAY** flashes on with the first beat of each bar and medium on the other beats. On the screen's key strip, an off key has a dark name, dim has a grey name with a border, medium a white name and on is filled.

## The knobs

| Knob | On HOME | Inside a track |
|---|---|---|
| **SELECT** | The **cursor**: picks the mixer track | The page (its name and dots at the top) or the row of a list. With a step held, its accent |
| **ALGORITHM** | The **tempo** (HOME + ALGORITHM: in tenths) | The machine's **mode** (table below). On a list, it changes the highlighted value |
| **PRESETS** | **MOVE**: one knob that moves the whole mix (below). Patterns are on the white keys | The sound: kit, patch or a sound from the bank |
| **K1-K4** | The cursor track as a mixer strip: **LEVEL** · **FILTER** (DJ: left low-pass, right high-pass) · **DELAY** · **REVERB** | Whatever the four dials at the bottom say. With HOME held: BPM, swing, master length and drive |

**Entering a track:** on HOME, SELECT moves the cursor to the track and **SEQ** or **EDIT** enter it. From any screen, **HOME + SELECT** shows a box with the target track and you enter when you let go of HOME.

**ALGORITHM inside a track (the mode):**

| Machine | ALGORITHM |
|---|---|
| BR-8 | Picks the instrument (with the box: big number, its name and model) |
| BP-1 | Picks the pad |
| BB-303 | The waveform: SAW or SQUARE |
| B-MINI | MONO or LEGATO |
| BX-7 | The algorithm, 1 to 32 |
| On the MACHINE page | Picks the instrument (see below) |
| BITTAL | Oscillator 1's table: its own or another bank sound's |
| BUNO-6 | The chorus: OFF, I, II or I+II |

**Priority:** REC on a note track > held step > list > track > HOME.

**The big knobs say what they do:** the bottom line of the screen reads **SEL · ALG · PRE** with their job on that screen (for example SEL PAGE · ALG INSTR · PRE KIT), and "-" when the knob does nothing there.

**How they feel:**
- **Turning fast speeds up** continuous values (BPM, cutoff, start); **lists move one item per click**, never skipping.
- **On/off settings:** right = on (or next), left = off. They don't wrap around.
- **MOVE, the DJ filter and PAN stop at the center** for two clicks, so 0 is easy to find.
- **SELECT goes through a page's sub-pages first** (SOUND, INSTR, PADS, ENV, MIX › DUCK), then to the next tab.
- Turning K1-K4 shows the name and value on the top line, without covering the screen; the big box is only for SELECT, ALGORITHM, PRESETS and the tempo, and it names the knob you turned.

## The screen

Four strips, always in the same place:

1. **Header:** the machine you are on (BX-1 on HOME; inside a track or a screen you opened from it, "<" and its name: HOME goes back), PLAY/REC, pattern and bar and the tempo in its own yellow.
   - A queued pattern reads **A01>A05**, with 4 blocks counting the beats down to the change.
   - **Six tiny cells** show the tracks: their color = playing, red = muted, white = a hit, outline = solo.
   - **SONG** when the song is on; a **green dot** = autosaved; "CPU!" only if the audio does not keep up; while recording notes, the gate they are recorded with (TIE or G75%).
   - The 2 s ring, with the name of what it opens, and the battery.
2. **Viewport:** what each screen shows. Inside a track, the page name and one dot per page, **O+1 / O-1** when the octave is shifted and **AGAIN: ...** with what tapping the page's button again does; on HOME, BX-1's pages. The usage hints show for the first 4 seconds; **the line that says how to exit stays**. A layer opened with a tap says **LATCHED**.
3. **Keyboard:** the 11 black and 16 white keys with what each one does and the same state as its light. On SEQ **the white keys are the 16 steps**; on a note track, a mint bar marks long notes. Short fixed names: drums BD, SD, CH, OH, CP, LT...; HI / LO = the other section; O- / O+ = octave; FIL = fill; X = exit.
4. **Knobs:** K1-K4 as four dials with their value and name, in physical order; under them, what SELECT, ALGORITHM and PRESETS do.

The palette: blue-black background, **cyan** for what is active, **amber** for values and the playing step, **laser red** for REC and mutes, **mint green** for long notes. A **red border** = the mix clipped.

## The 14 buttons

| Button | Tap | Again | Hold (layer) | 2 s | HOME + |
|---|---|---|---|---|---|
| **HOME** | Inside a machine: back to the BX-1 console. On the console: MIX | MIX › FX › TEMPO › SCENES › SONG | **Controller** (see below) | SYSTEM | (it is the modifier) |
| **SEQ** | The track's steps | Notes: LOOP · sampler: BREAK | **Length and speed** | TIME of the 6 tracks | Song |
| **EDIT** | SOUND (BP-1: PADS) | The editor's next page | **Erase** | Sampler: TAKES · others: initial sound | Copy the track to another |
| **ENV** | The machine's envelope · INSTR (drums) | The other envelope | **Accent** | — | — |
| **LFO** | The machine's LFO (B-MINI, BITTAL, BUNO-6). On machines without an LFO it **latches its layer**: repeat (BR-8, BP-1) or tie (BB-303, BX-7); LFO again or HOME closes it | — | **Repeat** (grids) · **tie** (notes) | — | — |
| **FX** | The track's MIX | DUCK (sidechain) | **Punch-in** | — | Master FX: delay; again: reverb |
| **SCL** | The track's SCALE | CHORDS | Live **chords** | — | Project scale |
| **ARP** | Notes: the track's ARP. BR-8 and BP-1: the **ROLL** screen, kept open (ARP again or HOME closes it) | — | **Arpeggio** (notes) · **ROLL** while held (grids) | — | — |
| **SAVE** | Save menu | Run it | **Copy pattern** | Quick save | Save as |
| **GLO** | Global list | Close | **Pitch** and TAP tempo | — | — |
| **REC** | Record (on the sampler: the recorder) | Looper: overdub › play | Record only while held | **Erase the track at once, no question** (for live use; HOME + OCT− undoes it). Not while anything is recording, nor on RECORD or TAKES. LOOP page: erase the loop | SONG REC |
| **PLAY** | Play / stop | — | — | — | Back to step 1 without stopping |
| **OCT − / +** | Octave · step page · on questions OCT+ = yes | — | — | — | Undo / redo |

## HOME everywhere

The FM-1 has no SHIFT, so HOME does three jobs:

1. **The BX-1 mixer:** HOME is a machine of its own, with five pages: **MIX › FX › TEMPO › SCENES › SONG** (HOME again goes to the next one). MIX has one row per track with its machine, its sequence (pulses for hits, mint blocks for long notes), LP or HP when its filter is on, and its volume with the peak. SELECT moves the cursor, K1-K4 mix it (see the knobs table) and SEQ or EDIT enter that machine; HOME brings you back.
2. **A controller that follows you:** holding HOME brings the controller to any screen. You edit the bass and, without leaving, mute or queue the next pattern.
3. **Modifier:** HOME + button = its global version (table above).

If you used HOME as a modifier, letting go does **not** take you to the home screen: HOME only navigates on a clean tap.

**Where a HOME tap goes:** inside a machine, to the console. On a screen you opened from a machine (GLO, HOME + FX, SAVE, TIME, SCALE, SONG, SYSTEM), back to that machine and page. On the console, to the next console page. While you hold HOME, only the buttons with a HOME + function are lit (OCT−/+ only when there is something to undo or redo), and turning PRESETS picks a pattern that loads when you let go.

To clear a track: REC held 2 s (instant) or EDIT held + black 3 (CLR); HOME + OCT− undoes either.

**Keys with HOME (on the HOME screen or while holding HOME):**

| Key | What it does |
|---|---|
| **White 1-16** | Queue that pattern (when stopped: switches right away). Hold one and tap another: **chain** from the first to the second |
| **Black 1-6** | Tap: mute T1-T6. Hold: **solo** while you hold it |
| **Black 7-10** | Launch scenes A-D |
| **Black 11** | FILL while you hold it |

**MOVE** (PRESETS on HOME): at 0 the mix is untouched. To the left the master low-pass closes and the whole mix goes into the echo (darker, more repeats); to the right the master high-pass opens and the mix washes into the reverb. It is a live control: it is not saved. The tempo box is yellow and its four blocks flash with the beat.

**FX page** (HOME › FX): FX again goes **DELAY › REVERB › MASTER**. MASTER has the master **FILTER** (open in the middle; left low-pass, right high-pass), **BEAT FX** (one of the 16 punch-in effects stays on until you turn it back to OFF) and **DRIVE**.

## The six machines

| Track | Machine | What it is | Tabs |
|---|---|---|---|
| **T1** | **BR-8** | Drums: TR grid of 16 **modeled** instruments (computed live); kits 808, 909 and SYNTH | SEQ · SOUND · INSTR · MIX · MORE |
| **T2** | **BP-1** | 16 pads made from your takes, on a grid | SEQ · PADS · BREAK · TAKES · MIX · MORE |
| **T3-T6** | **your choice** | BB-303, B-MINI, BX-7, BITTAL or BUNO-6 (factory: BB-303, BX-7, BITTAL, BUNO-6) | SEQ · MACHINE · SOUND · MIX · ARP · LOOP · MORE |

**Choosing the instrument (T3-T6).** The **MACHINE** page sits just before SOUND (SELECT to the left of SOUND, or MORE › MACHINE). It lists the five machines with what inspired each one. **Turn ALGORITHM to a machine and leave it there 2 seconds**: a bar fills ("LOAD IN 1.4 S") and the track becomes that machine. Moving away cancels it. The notes, length, loop, ARP, scale and mix stay; the sound goes to the machine's first preset and per-note P-Locks are cleared. The machine is saved with the pattern, like the sound.

**Heavy machines, one at a time:** **BUNO-6** (it uses about a third of the processor) and **BITTAL** (its wavetables fill the memory) can only be on one track. On another track they show dimmed with "ON T6" and the screen says "BUNO-6 ALREADY ON T6". BB-303, B-MINI and BX-7 can repeat (a BX-7 on T3 is polyphonic too). The same rule applies when copying a track or loading a pattern.

**MORE** is the list with everything about the track: machine (T3-T6, it opens MACHINE), accent, length, speed, transpose, **MIDI OUT** (the track also sends its notes over MIDI) and its MIDI channel, and initial sound (on the sampler also: interpolation). SELECT picks the row and ALGORITHM changes it.

**Black keys follow a single grammar in SEQ:** 1-8 are the track's elements, 9 and 10 modify them and **11 breaks the pattern** (FILL).

## The sound of each machine

Everything is computed live; the sampler uses your takes. Until v1 each version may change the format: patterns from an earlier version are erased on first boot.

| Machine | What it is | K1-K4 on SEQ | PRESETS |
|---|---|---|---|
| **BR-8** | 16 modeled instruments, each with its own **model**: BD 808, BD 909, SD 808, SD 909, HAT 808, METAL, clap, TOM (and congas), cowbell, RESON (rim, clave, block), noise or FM | TONE · TUNE · DECAY · COLOR | Kits 808, 909 and SYNTH |
| **BB-303** | 303-style mono: 4-pole ladder filter, accent opens the filter, slide with TIE | CUTOFF · RESO · ENV MOD · DECAY | 8 sounds: CLASSIC, SQUELCH, DEEP SAW, RUBBER, SQUARE, SCREAM, DUB, PLUCK |
| **B-MINI** | Three oscillators (TRI, SHARK, SAW, SQUARE, WIDE, NARROW) in 32' to 2' ranges, noise, ladder filter with drive, two envelopes (CONTOUR and LOUDNESS), LFO and glide | CUTOFF · EMPHASIS · CONTOUR · F DECAY | 6 basses from the bank |
| **BX-7** | 6 operators (msfa, the core of Dexed); the DX7's 32 algorithms | MOD LVL · FEEDBACK · DECAY · RELEASE | 16 original patches and the voices of your .syx banks |
| **BITTAL** | Two wavetable oscillators (up to 8 frames, alias-free on every note) with up to 4-voice unison, filter (LP 24/12, BP, HP or ladder) with drive, two ADSRs, LFO and 4 macros | The sound's 4 macros (their name says where they go: CUTOFF, POSITION, LFO, SPACE…) | 19 bank sounds and yours |
| **BUNO-6** | One oscillator per voice (saw, pulse with PWM, sub one octave down and noise), 4-position high-pass, 24 dB resonant filter, one ADSR, delayed LFO and stereo chorus | FREQ · RES · ENV · LFO | 12 original sounds |

BR-8's kicks, snares and hats are the **Plaits** models (Mutable Instruments), which imitate the 808 circuits and 909-style synthetic versions. With **accent** they hit differently, not just louder.

**The sound bank.** B-MINI, BITTAL and BUNO-6 load **sounds** (.bsnap): their settings, their macros and, on BITTAL, their tables. The factory ones come from freely licensed Vital presets (CC0, Unlicense and GPL-3.0; credits in [CREDITS.md](CREDITS.md)) and from original BUNO-6 presets made in Vital. Yours are added from the web editor ([sounds](../../web/sonidos.html)): PRESETS goes through the factory ones first and then yours. The sound's fingerprint (#A4F9) shows at the top right of SOUND.

### Editing the sound

**SOUND** is the machine's editor. Its pages follow **the real machine's panel**, in the same order and with the same names, 4 settings per page:

| Machine | Pages (K1-K4) |
|---|---|
| **BB-303** | **VCO VCF** (TUNING, CUTOFF, RESO, ENV MOD) · **ENV** (DECAY, ACCENT, SLIDE, WAVE) |
| **B-MINI** | **CONTROL** (TUNE, GLIDE, MOD RATE, MOD AMT: left = oscillators, right = filter) · **OSC 1-3** (RANGE, FREQ, WAVE) · **MIXER** (OSC 1, OSC 2, OSC 3, NOISE) · **FILTER** (CUTOFF, EMPHASIS, CONTOUR, KBD CTRL) · **F CONTOUR** and **L CONTOUR** (ATTACK, DECAY, SUSTAIN, RELEASE) · **OUTPUT** (DRIVE, MODE, VEL FILT, VEL LVL) |
| **BX-7** | **ALGORITHM** (ALG, FEEDBACK, MOD LVL, KEY VEL) · **EG** (R1 ATK, R2-3 DEC, R4 REL) · **VOICE** (PORTA, TRANSP) · **LFO** (SPEED, DELAY, PMD, AMD; on top of the patch's own LFO) |
| **BITTAL** | **OSC 1 / OSC 1 +** · **OSC 2 / OSC 2 +** (LEVEL, TRANSP, FRAME, UNISON · DETUNE, FINE, TABLE) · **FILTER** (MODEL, CUTOFF, RESO, DRIVE) · **FILTER +** · **ENV 1 / ENV 2** (A, D, S, R) · **LFO 1** (SHAPE, RATE, DEST, AMOUNT) · **MACRO 1-2 / 3-4** · **VOICE**. The oscillator pages draw the table in 2.5D, with the playing frame in cyan |
| **BUNO-6** | **LFO** (RATE, DELAY) · **DCO** (LFO, PWM, PWM MODE, RANGE) · **DCO WAVE** (PULSE, SAW, SUB, NOISE) · **HPF VCF** (HPF, FREQ, RES, ENV) · **VCF MOD** (ENV +/-, LFO, KYBD, VELO) · **VCA** (ENV/GATE, VELO) · **ENV** (A, D, S, R) · **CHORUS** |

**K1-K4** change the page's 4 settings; above them, a drawing of what they do (the wave, the table, the filter curve, the envelope or the LFO). **SELECT** changes page (at the first or last one it moves to the next tab). **EDIT** again = next page (EDIT never lands on MACHINE, so you can't change the instrument by accident). **ENV** opens the machine's first envelope (again: the second one). **LFO** opens the LFO page; the BB-303 has none, so LFO keeps its tie layer. The macros (K1-K4 on SEQ) take P-Locks per note.

### BX-7: the algorithm

SOUND draws the **algorithm as on a DX7**: carriers at the bottom, modulators above, feedback in amber and each operator's level; below, each one's frequency ratio. ALGORITHM steps through all 32 (with MOD LVL, FEEDBACK, DECAY and RELEASE on top of the patch). DX7 **.syx** banks (32 voices) load from the web editor or with `py -3 tools/fm1/sonido.py bank.syx`.

### Editing a drum instrument

**INSTR** (EDIT again, or ENV) has two pages: **HIT** (TUNE, DECAY, TONE and COLOR, named after what it does in that model) and **INSTRUMENT** (MODEL, LEVEL, CHOKE group and line LENGTH). EDIT again changes page. Black 1-8 and 9 pick the instrument.

## Drums (BR-8)

The SEQ screen is a matrix: 16 instruments × the 16 steps of the page. Each line shows its own playhead, because each instrument can have its own length.

| Control | Does |
|---|---|
| **White keys** | Steps of the selected instrument: tap = set or clear |
| **Black 1-8** | Select (and play) an instrument of the section |
| **Black 9** | Switch section (1-8 / 9-16) |
| **Black 10** | **MUTES** screen: a tap keeps it open (white keys mute or unmute each instrument; leave with black 10 or HOME); hold = only while held. **Only instruments with steps are lit:** medium and flashing on every hit = plays, dim = muted, off = empty. On screen a muted instrument turns red. The knobs keep their SEQ jobs |
| **Black 11** | **FILL** screen: while open the fill plays and the white keys set a hit **only for the fill** (again: removes it; on = fill only, medium = normal, dim = not in the fill). K1 = automatic fill (every 2, 4 or 8 bars). Tap = stays open; hold = only while held. The fill stops when the screen closes |
| **OCT − / +** | Page |

**Hold a step + turn:** K1-K4 = P-Lock of TONE, TUNE, DECAY or COLOR on that step only · SELECT = accent · ALGORITHM = repeat ×1 to ×4 or FLAM · PRESETS = condition (ALWAYS, FILL ONLY, NO FILL).

**INSTR** (EDIT again, or ENV): level, tuning, decay and **line length** of each instrument. For example, a 12-step hi-hat against a 16-step kick.

**SOUND:** the kit's 16 pads; K1-K3 = tone, tuning and decay, K4 = COLOR (shifts each instrument's color). Changing the kit with PRESETS can be undone (HOME + OCT−).

### ROLL (ARP)

Like a drum machine's roll button. A tap on **ARP** keeps the screen open (ARP again or HOME closes it); holding ARP shows it only while held.

| Control | Does |
|---|---|
| **White keys** | The 16 instruments: hold one and it rolls, locked to the bar. Lit medium if it has steps, dim if empty; every hit flashes |
| **Black 1-6** | Rate: 1/8, 1/16, 1/32 and their triplets (8T, 16T, 32T) |
| **Black 7 · 8** | **CRESC** (the roll gets louder) · **DECRESC** (it fades) |
| **Black 11** | The fill plays while held: FILL + ROLL for the build-up |
| **K1-K4** | **RATE** · ramp length of the crescendo (1/4 to 2 bars) · **SWING** (50-75 %) · **TAIL** (the roll keeps going after you let go: OFF, 1/16, 1/8, 1/4) |

With **REC** on, the roll is written into the steps as repeats; with a crescendo, the loud hits become accents. The settings are live: they are not saved.

## Pad sampler (BP-1)

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

The layers work as on the drums: accent, repeat, erase, **ROLL** (ARP; the white keys are the pads) and per-line lengths in TIME. Inside ROLL, REC records what you play into the pattern, not a sample.

### PADS (EDIT): each pad's editor

At the top, the **map** of the pad's take, with every pad of that take marked and the zoom box. In the middle, the big **waveform**, with the pad in **cyan brackets** and the other pads of the take with their number. At the bottom, **one cell per white key**, in the same place as the key: filled = it has sound, outlined = the selected pad. The orange line is what is playing.

**EDIT again** changes page: **TRIM › SOUND › ENV › LOOP** (the current one is amber, top right). ENV also opens the ENV page.

| Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|
| **TRIM** | START (the end stays) | END (the length) | ZOOM X1-X8 | SNAP: OFF, 1/16, 1/8 |
| **SOUND** | PITCH (±24 st) | LEVEL | PLAY: ONESHOT, KEY, THRU, LOOP | DIR: FWD or REV |
| **ENV** | ATTACK | RELEASE | CHOKE group | TEMPO: follows the BPM or not |
| **LOOP** | LOOP IN | LOOP OUT (WHOLE = the whole pad) | PLAY | — |

**How the key answers (PLAY):**

- **ONESHOT:** plays whole even if you let go.
- **KEY:** plays while you hold.
- **THRU:** plays the pad and, held, **keeps going through the rest of the take**.
- **LOOP:** repeats the pad while you hold. With the **LOOP** page it repeats only a section (the OP-1's **sustain loop**): held, that section repeats; when you let go, it plays on to the end of the pad. The section shows in mint over the waveform.

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
   - The take stays **UNSAVED**: the white keys play it in 16 parts. **It is saved when you stop** the transport, because writing flash stops the audio, and if the kit was empty, it builds it. HOME asks before discarding it (OCT+ = discard; HOME = keep it and go to the console).
2. **TAKES** (EDIT 2 s on T2, or the tab): the library, with the waveform of the marked take. ALGORITHM picks; K1-K3 = the take's BPM, bars and gain; white keys play it in 16 parts (held: it keeps going). **Black 1 (HIT)** = kit on the hits, **black 2 (16)** = 16 equal parts, **black 3 (8)** = 8, **black 4 (PAD)** = the whole take on the selected pad, **black 11 (DEL)** = delete (asks; its pads become empty). If the kit already has pads, building a new one asks. A yellow dot = the kit uses that take.

**How much fits:** up to **6 s of takes in total** (8 bits at 16 kHz = 16 KB per second, up to 3 s per take). They live in the 100 KiB flash zone shared with your bank sounds: takes from the bottom and sounds from the top. TAKES shows what is free of the 6 s and how many of your sounds are on top; the [control platform](../../web/control.html) shows all the memory and warns before it fills up.

## Note tracks (T3-T6)

- **SEQ:** the page is only about this track. On top, the page's 16 steps as a **piano roll**: each note at its pitch and with its length (long ones in mint, early or late ones shifted from their step). White keys are those steps (up to 4 notes per step). Below, the track's bars and the playhead. **Hold a step** and play **black 1-8** to write those notes of the track's scale (the same one again removes it). Black 9 and 10 move their octave down or up; 11 = FILL.
- **Long note with two white keys:** hold the step where it starts and tap the white key where it ends: the note lasts from one to the other. The lights of the keys it covers stay dimmed, like a trail.
- **Free recording, like a looper:** on an **empty** note track, REC records with no fixed length (the screen says FREE REC and counts the bars). REC again closes it: the track keeps the whole bars you played (1 to 4) and every note keeps how long you held it (up to 4 bars). If the track already has notes, REC records over it with its length.
- **Length in bars:** SEQ held + black 7-10 = 1, 2, 3 or 4 bars.
- **Held step + knobs:** K1-K4 = P-Lock · SELECT = hit (NORMAL › ACCENT › GHOST) · ALGORITHM = probability or fill · PRESETS = length or TIE (glide).
- **LOOP (SEQ again):** unquantized looper. K1 ON/OFF · K2 bars (1, 2 or 4). **REC** arms (starts at the bar) › when the loop closes it goes to PLAY › REC again: OVERDUB › REC: PLAY. **REC 2 s** erases the loop.
- **MIDI OUT** (in MORE): the track plays and also sends its notes over MIDI on its channel.

### Expressive REC

With **REC** on a note track, what you play stays as you played it:

- **The note keeps its real timing:** the start with its **micro-timing** (how early or late you were, in ticks) and the end with its **gate** (how much of the last step sounded). If the note is still held when the bar ends, it is **cut at the bar**.
- **The knobs stay on the note:** while you hold a note while recording, whatever you turn on K1-K4 stays on that note (P-Lock).
- **Editing the step with REC:** **SELECT** picks a step (red box), **ALGORITHM** its gate (from 10 % staccato to 95 % = the whole step, and 100 % = **TIE**, tied to the next one) and **PRESETS** its micro-timing (−11 to +11 ticks). The header shows the gate you are recording with.

## The layers (hold)

While you hold, the screen draws the keyboard with what each key does and its LEDs; on the drums it shows the 16 instruments. Only the buttons that do something in the layer stay lit. The sequence keeps playing.

| Hold | Layer | The keys |
|---|---|---|
| **HOME** | Controller | White = queue pattern · black 1-6 = mute / solo · 7-10 = scenes · 11 = FILL. SELECT = go to a track when you let go · ALGORITHM = BPM in tenths |
| **SEQ** | Length and speed | White = track length (1-16; OCT−/+ = 17-64): the length is on, inside it medium, outside dim · black 1-6 = speed ×2 to ×1/4 · black 7-10 = 1-4 bars (1BR-4BR) |
| **EDIT** | Erase | Playing: holding a white key erases what passes under the playhead; stopped: erases the line or the step · black 1-3 = copy, paste, clear the track · sampler: black 4 = CAPT (what BREAK does goes into the grid) · OCT−/+ = undo / redo |
| **ENV** | Accent | White = accent on / off (empty step = new one with accent; on notes: NORMAL › ACCENT › GHOST). K1 = how much accent |
| **LFO** | Repeat · tie | Grids: white = ×1 › ×2 › ×3 › ×4 › FLAM. Notes: white = tie the note to the next one (K1 = glide). On machines without an LFO page a tap on LFO keeps this layer open |
| **FX** | Punch-in | White = 16 effects quantized to 1/16 · black 1-6 still mute |
| **SCL** | Chords | White = 16 chords · black 1-4 = +7, +9, SUS2, SUS4 |
| **ARP** | Arpeggio · ROLL | Notes: what you play is arpeggiated · grids: the ROLL screen (see Drums): white = roll, black 1-6 = rate, 7-8 = CRESC / DECRESC, 11 = FILL; with REC it is recorded as repeats |
| **SAVE** | Copy pattern | White = paste the pattern there · black (1-6) + white = only that track. If the target has notes, it asks |
| **GLO** | Pitch | The whole keyboard transposes the project (C4 = 0) · **PLAY = TAP tempo** without stopping · K1 = BPM, K2 = SWING, K3 = transpose, K4 = octave |

## Time (SEQ 2 s)

Shows the 6 tracks as bars of their real duration, each with its playhead: polymeter at a glance. SELECT or ALGORITHM pick the track; K1 = length (1-64); K2 = speed (×2, ×3/2, ×1, ×3/4, ×1/2, ×1/4); K3 and K4 (grids): an instrument and its length.

The **master length** (HOME + K3: 16, 32, 48, 64, 128 or INF) restarts all tracks together. That is where patterns and scenes change.

## Scales and chords

- **SCALE** (SCL): each note track can have its **own scale** (K2: PROJECT, or one of 9 scales; K1 = ROOT). **HOME + SCL** changes the project scale, used by tracks without their own.
- **CHORDS** (SCL again): 16 chords on a 4×4 grid with their degree and name. The color tells their function: **blue** rest (I, iii, vi), **green** preparation (ii, IV), **orange** tension (V, vii). K1 = set (DIATONIC, PROGRESSIONS, NEO SOUL), K2 = voicing (CLOSED, OPEN or LED, voice-led), K3 = octave, K4 = strum.

## Scenes and song

**SCENES** (HOME, HOME, HOME): a scene = pattern + mutes. White 1-4 launch A-D at the next master cycle (when stopped: right away); 5-8 store what is playing into A-D; hold one of 1-4 and tap others for a **chain**. From any screen, with HOME, black 7-10 also launch them. **REC** on SCENES (or HOME + REC) = SONG REC.

**SONG** (HOME + SEQ): a white key adds a pattern (01-16; the ones with notes are medium), black 9 (DEL) deletes; K1 = step, K2 = repeats, K4 = song ON/OFF.

## Effects

- **Each track's MIX** (FX): level, pan, **delay** send and **reverb** send. FX again: **duck** (sidechain from the T1 kick).
- **BUNO-6** has its own stereo **chorus** (I, II or I+II, with ALGORITHM). BITTAL's **SPACE** macro adds reverb send.
- **HOME + FX:** the delay (time, feedback, mix; up to 0.80 s, so 1/4 gets shorter below 75 BPM) and the sidechain release. HOME + FX again: the **reverb** (size, damping, mix).
- **On HOME:** each track's **filter** (K2) and **MOVE** (PRESETS). The **master filter**, **BEAT FX** and **DRIVE** are on HOME › FX › MASTER.
- **FX held:** 16 punch-ins (stutter, reverse, tape stop, gate, sweeps, bitcrush, SR down, delay throw, echo freeze, kill low/high).
- **DRIVE** (HOME › FX › MASTER, or HOME + K4): saturation of the mix.

## Saving

- The flash is **only written when stopped** (writing pauses the audio for a moment).
- **Autosave:** when stopped, after 10 s without touching anything.
- **SAVE:** save · save as · save song and scenes · erase pattern (SAVE twice) · count events. **SAVE 2 s:** quick save. **HOME + SAVE:** save as.
- **Copying:** SAVE held + white key copies the pattern; HOME + EDIT copies a track onto another one in the same pattern (notes to notes, hits to hits; ALGORITHM picks the target, OCT+ pastes). Machines don't change.

## System (HOME 2 s)

Internal or external clock · Split Sync (L = pulse, R = mono mix) · pulse PPQN and latency · autosave · idle **button** brightness (OFF, DIM or MEDIUM; the keys are not affected) · **FAST SCREEN** (test, off by default) and **TWO CORES** (test, off by default) · takes, sounds and free memory · **LANGUAGE** · version.

**The two tests:** **FAST SCREEN** raises the screen's SPI to 30 MHz (measured: a full frame goes from 85 to 39 ms). **TWO CORES** computes T4-T6 on the FM-1's second core (on the computer it gives exactly the same samples). Since 0.6.2 the second core works through an interrupt: the first time you turn it on it is checked to answer, and if it does not answer or is late, it goes on with one. They are being tested: if the FM-1 hangs or boots into safe mode, both switch themselves off. The control platform says whether TWO CORES is on and which core ran the second job.

## MIDI

- **Input** (USB and TRS): channels 1-6 = Bubba Machine's tracks. On grids, the note picks the instrument (0-15); on the sampler, the pad.
- **External clock:** follows MIDI Clock and Start/Stop.
- **Output:** MIDI Clock when the clock is internal, and the notes of tracks with **MIDI OUT**.

## Control platform

The [control platform](../../web/control.html) manages the FM-1 from Chrome or Edge, over USB. **It only works with the FM-1 connected** and Bubba Machine 0.6.1-beta or newer; there is no emulator mode.

- **Memory:** free system RAM, audio load, the 6 s of takes, the 100 KiB zone shared by takes and sounds, and the language pack. It refreshes every 2 s.
- **Warnings based on what you are doing:** low RAM (the FM-1 could hang), high audio load or late blocks (dropouts), takes or zone nearly full, the 12-sound maximum, whether TWO CORES is on and, before sending anything, whether it fits or what to delete.
- **Takes:** drop an audio file (WAV, MP3, FLAC…); the page turns it into 16 kHz, 8 bits as the FM-1 records, you pick where it starts and how long, listen to it exactly as it will sound and send it with its BPM. You can delete them too.
- **Sounds:** Vital presets (.vital) for BITTAL, B-MINI or BUNO-6, DX7 banks (.syx) for BX-7 and .bsnap files. You can delete yours too.
- **Language:** load Spanish, another .bmlang pack, or go back to English.

To write (send or delete) the transport has to be stopped: writing flash cuts the audio. The page says so and keeps the buttons off while it plays.

## Screen language

The screen is in **English**. Other languages are **packs** that are not loaded by default: one pack lives in its own 16 KiB of flash (it does not touch patterns or takes, and a firmware update does not erase it).

- **Load Spanish on the FM-1:** the language pack is a separate add-on with its own 16 KiB zone: load or remove it on the [control platform](../../web/control.html), with the FM-1 connected and the transport stopped. Or from the computer, with the transport stopped, `py -3 tools/fm1/idioma.py lang/es.bmlang`. **SYSTEM › LANGUAGE** then switches between English and the pack. `--quitar` removes it.
- **In the emulator:** the button under *Sampler and memory* loads or removes the Spanish pack.
- **Making a translation:** copy `lang/plantilla.txt` (every phrase on screen) to `lang/<code>.txt`, write the translation under each phrase and build it with `py -3 tools/gen_lang.py`. The FM-1 font is ASCII: no accents. What you don't translate shows in English.
