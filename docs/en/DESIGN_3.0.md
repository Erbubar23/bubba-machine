# Bubba Machine 3.0: interface design

> **Status: implemented in 0.3.0.** The [manual](MANUAL.md) explains how to play it. This document keeps the reasoning behind each decision. On-screen names stay in Spanish, with their meaning in parentheses.

## The idea

**Four gestures per button, meaning the same on every button. HOME everywhere.**

The big knobs take you places, the small ones adjust, and the keys play and control. It brings back the best of 0.2 (three kinds of press, live layers, the 2 s ring) and adds the new: knobs that navigate, HOME as a controller, tabs, and fixed and free tracks.

| Gesture | When | What it does |
|---|---|---|
| **Tap** | under 300 ms | Opens its page on the track you are on. Again: the same button's next page |
| **Hold** | 300 ms or more | Its **live layer**: while you hold it, the keys do that job over what is playing. The screen draws the keyboard with what each key does. Let go and you are back |
| **Hold 2 s** | 2 s without touching anything else | Its setup or its dangerous action. A ring fills at the top; touching anything else cancels it. Only on HOME, SEQ, EDIT, SAVE and REC |
| **HOME + button** | with HOME held | The button's **global version**: for the whole project, not just the track |

## The knobs

| Knob | What it does |
|---|---|
| **ALGORITHM** | The track. On HOME, turning it shows a box with the target track and you enter it after 400 ms (configurable: 0, 250, 400, 800 ms or OFF). Inside a track it switches right away and keeps the page. In a list, it changes the marked value. With HOME held, it picks a track from any screen and you enter it when you release HOME |
| **SELECT** | The page or the row. With a step held, its accent |
| **PRESETS** | The track's sound. On HOME, the pattern bank |
| **K1-K4** | Whatever the bottom legend says, always in the same color. With HOME held: BPM, swing, master length and drive |

**ALGORITHM priority:** held step > list > track > HOME. The footer says what it does at each moment.

## The 14 buttons

| Button | Tap | Again | Hold (layer) | 2 s | HOME + |
|---|---|---|---|---|---|
| HOME | HOME: the 6 tracks | MIXER › SCENES › SONG | Controller: white keys = pattern, black keys = mute, K1-K4 = tempo and globals | SYSTEM | (it is the modifier) |
| SEQ | Track steps | Synth: LOOP · sampler: RETRIG | Length and speed: white key = length, black keys = speed | TIME for the 6 tracks | SONG |
| EDIT | SOUND | Drums: INSTR | ERASE: erases as it passes; black keys copy, paste, clear (sampler: 4 = capture) | Sampler: slots · others: initial sound | Copy the track to another |
| ENV | ENV (notes) · INSTR (drums) | ENV page 2 | ACCENT: paint accents | — | — |
| LFO | Track LFO (future) | — | REPEAT (drums, sampler) · TIE (notes) | — | — |
| FX | Track MIXER | — | PUNCH-IN: 16 effects; black keys 1-6 still mute | — | Global delay and sidechain |
| SCL | Track SCALE | CHORDS | Live chords | — | Scale for the whole project |
| ARP | Track ARP | — | Live arpeggio · drums: ROLL | — | — |
| SAVE | Save menu | Run it | COPY PATTERN: white key = paste there | Quick save | Save as |
| GLO | Global list | Close | TONE: the keyboard transposes everything; PLAY = TAP | — | — |
| REC | Record | Looper: overdub › play | Record only while held | Clear the track (asks first) | SONG REC |
| PLAY | Play / stop | — | — | — | Back to step 1 without stopping |
| OCT − / + | Octave · page · in questions OCT+ = yes | — | — | — | Undo / redo |

An empty cell is on purpose: that gesture does nothing on that button, so it never fires by accident.

## HOME everywhere

The FM-1 has no SHIFT, so HOME does three jobs:

1. **Home screen:** the 6 tracks with their sequence, and the pattern bank.
2. **A controller that follows you:** holding HOME brings the controller to any screen. You are editing the bass and, without leaving, you mute or queue the next pattern.
3. **Modifier:** HOME + button = its global version.

If HOME was used as a modifier, releasing it does **not** take you to the home screen: HOME only navigates on a clean tap.

**Keys on HOME:** black keys 1-6 mute T1-T6 (hold = solo), 7-10 launch scenes A-D and 11 is FILL. White keys queue a pattern from the bank; two white keys chain from the first to the second.

## Tracks: 3 fixed and 3 free

| Track | Type | What it is |
|---|---|---|
| T1 DRUMS | Fixed | TR grid with 16 instruments |
| T2 SAMPLER | Fixed | 16 pads made from the takes, on a grid |
| T3 BASS | Fixed | Monophonic wavetable with glide |
| T4-T6 | Free | SYNTH, MONO, KIT or MIDI, chosen in the MAS (more) tab |

Each track shows its tabs at the top: SELECT steps through them and the buttons jump straight to them. The last one, **MAS** (more), is the list with everything about the track.

**Black keys follow one grammar:** 1-8 are the track's elements, 9 and 10 modify it, and **11 breaks the pattern** (FILL).

## Pad sampler

The 0.3 and 0.4 sampler was a sliced loop that a generator reordered (the idea of X0X's BREAK). On the device it was hard to follow: you played a cut and another number showed on screen, because the row of what plays mixed what you programmed with what the generator picked. From 0.5.0-beta, T2 is a **pad sampler**, like a KO II or the drum sampler of an OP-1 field:

- **Pads, not slices:** each pad is a piece of a take, with its own start and length (they can overlap), pitch, level, key mode (ONESHOT, KEY, THRU, LOOP), attack, release, choke group and tempo. A kit can mix takes.
- **The same grid as the drums:** one line per pad. Everything you learn on T1 works on T2: steps, accents, repeats, mutes, fill, P-Lock.
- **The take builds the kit:** when a take is saved with the kit empty, it is cut on the hits and the grid is written so it plays like the take. TAKES builds other kits or puts a whole take on one pad.
- **The generator stays as BREAK:** a separate page, off by default, that varies the grid bar by bar. The PLAYS row shows exactly what is going to play and CAPT writes it into the grid.
- **The waveform as on a sampler:** mirrored and big, in the style of X0X, with the peak in a dark shade and the RMS on top.

## Recorder

Recording, saving and building the kit are separate steps:

1. **RECORD:** a big number with the seconds left, which is the smaller of the recording RAM (3 s) and the free take memory. Marks show how many bars fit. White keys play the new take.
2. **TAKES:** a library of recordings with their BPM, bars and waveform. Kits come from here.
3. **PADS:** later, calmly, each pad on its take's waveform.

A take stays unsaved until you stop the transport, because writing the flash pauses the audio.

## Decisions

- **Entry box timing:** 400 ms by default; changed in GLO or SISTEMA (0, 250, 400, 800 ms or OFF). *Resolved in 0.3.0.*
- **Take library:** the 128 KiB of samples became a free-length take library (format 3). *Resolved in 0.3.0.*
- **Pattern banks:** the flash stores 16 patterns; more banks need a bigger flash map or smaller patterns. *Open:* in 0.3, PRESETS on HOME walks the 16.
- **From a sliced loop to pads:** SLICER, CORTAR and then CORTES tried to edit a loop's cuts; none was easy to follow on the device next to the generator. The sampler became a pad sampler (above). *Resolved in 0.5.0-beta.*
- **Ghost notes on drums:** each step's byte is full; it needs a level P-Lock or a bigger format. *Open:* notes already have three levels.
