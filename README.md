<p align="center"><img src="docs/images/banner.svg" alt="BrickPad: sampler, synth and clip sequencer for the TrimUI Brick" width="100%"></p>

<p align="center">
  <a href="https://github.com/obhateion/brickpad/releases/latest"><b>Download</b></a> ·
  <a href="docs/MANUAL.md"><b>Manual</b></a> ·
  <a href="docs/BrickPad-Manual.pdf"><b>1.0 beta manual (PDF)</b></a> ·
  <a href="README.ru.md"><b>По-русски</b></a> ·
  <a href="CHANGELOG.md"><b>Changelog</b></a> ·
  <a href="LICENSE"><b>Licence</b></a>
</p>

**BrickPad** is a real-time sampler, synthesizer and clip sequencer for the **TrimUI Brick** handheld running
**Knulli**. Everything is played and edited with the d-pad and the buttons, in the spirit of the Dirtywave M8: no touch
screen, no mouse, the hands stay on the device.

![A tour: HOME, scenes, the mixer and its drawer, a pad, the piano roll, SOUND, the jump, FX, LFO, steps, PROJECT](docs/images/tour.gif)

*A tour of the interface from the 1.0 beta; the line under the screen says which keys were pressed.*

> **Beta.** BrickPad is used every day on the device, but the project file format may still change.

## Contents

[Features](#features) · [Screens](#screens) · [Engines](#engines) · [Effects and modulation](#effects-and-modulation) ·
[Install](#install) · [Buttons](#buttons) · [Build](#build-from-source) · [Licence](#licence)

## Features

| | |
|---|---|
| **12 pads** | each pad holds one instrument and 16 patterns; a playing pattern is a clip |
| **16 scenes** | a scene is a whole set of twelve pads with their sounds, effects, mix and patterns; it changes on the bar, without a stop |
| **4 engines** | SAMPLER, GRAND (two-oscillator synth with a patch bay), ACID (303-style line), CLOUD (granular) |
| **Sequencer** | steps with note, velocity, chance, ratchets, slices, accent and slide, or a piano roll with note lengths; follow actions between patterns |
| **Mixer** | 12 strips with inserts, sends and levels in dB; a drawer with reverb, delay, the master and the pad's effects |
| **Effects** | two inserts per pad from 11 types, reverb and tempo delay, master compressor and limiter |
| **Modulation** | four LFOs per pad, four automation lanes per pattern with bends, DUCK and keyed compression |
| **Freeze** | a pad is rendered to audio with its effects: a full song drops from about 40 % to 7 % of the CPU |
| **Recording** | record what you play with a count-in, or capture it afterwards |
| **Files** | projects, WAV export of every scene in turn or one file per pad, themes, presets, pad icons and names |
| **Views** | the advanced view, a simple large view (PLAY, NOTES, SOUND) with a kid lock, five themes |

## Screens

![Screens](docs/images/screens.svg)

**HOME** has three screens, **F1 / F2** step through them: PADS, SCENES and MIXER. **EDIT** on a pad opens it: its
categories PATTERN, SOUND, FX, MOD, MIX and LOOK, each with pages of four cards. The breadcrumbs on top say where you are,
the footer shows the keys of the screen, **Y** goes back one level, **F1+F2** opens PROJECT from anywhere and returns to
the same place.

| | |
|---|---|
| ![PADS](docs/screenshots/pads.png) | **PADS**: twelve pads with name, engine, icon, pattern and how far the clip has played. EDIT opens a pad (an empty one: choose its engine), X starts or stops its clip, PLAY all clips, Y mute, SHIFT+Y freeze, SHIFT+X name, SHIFT+d-pad selects several pads. MENU turns the buttons into pads to play, again into keys of the scale. |
| ![SCENES](docs/screenshots/scenes.png) | **SCENES**: a row per scene, a dot per pad. EDIT launches a scene on the next bar while playing; all twelve pads change at once. `+ NEW SCENE` makes an empty one, X copies a scene, OPT+EDIT deletes it after a YES / NO window (the open one hands over to the next first). Tempo, key, the master and the returns belong to the project. |
| ![MIXER](docs/screenshots/mixer.png) | **MIXER**: one strip per pad with its two inserts, sends A and B, the fader in dB and the level now in dB. EDIT+up/down the level, Y mute, X solo. The tab at the edge is the drawer with the master's meter. |
| ![Mixer drawer](docs/screenshots/mixer-drawer.png) | **The drawer** (EDIT on a strip, or R1): reverb, delay and the master as faders with their parameters, and the selected pad's inserts, sends and duck in full. It lies over the strips; they slide so the pad stays in view. With SCENE MIX on, the same sound in another scene shares its mix. |
| ![A pad](docs/screenshots/pad.png) | **A pad**: its categories PATTERN, SOUND, FX, MOD, MIX and LOOK with a summary of each. EDIT opens one, F1/F2 go to the next pad on the same page. |
| ![Tree](docs/screenshots/tree.png) | **PAD MENU = TREE** (PROJECT > INTERFACE): the pad as a tree with every page, from HOME to the pages. Left / right the level, up / down choose, EDIT open. |
| ![Jump](docs/screenshots/jump.png) | **SHIFT held** inside a pad shows a cross: SHIFT+arrow jumps straight to PATTERN, SOUND, FX or MIX. |
| ![STEPS](docs/screenshots/steps.png) | **PATTERN > STEPS**: 16 steps a page. L1/R1 the lane: NOTE, VELOCITY, CHANCE, REPEAT, and SLICE, ACCENT, SLIDE where the engine has them. EDIT puts a step, EDIT+arrows change it, SHIFT+X a random pattern. |
| ![ROLL](docs/screenshots/roll.png) | **PATTERN > ROLL**: a piano roll. EDIT adds or removes a note, EDIT+arrows its length and pitch, X+arrows velocity and chance, X+EDIT a chord, L1/R1 the grid from 1/128 to 1/4. |
| ![PROJECT](docs/screenshots/project.png) | **PROJECT**: SONG (tempo with tap, swing, key, scale), PLAY, RECORD, FILE (open, save, export, new), INTERFACE, MIXER, SYSTEM (themes, library) and ABOUT, every key by topic. |

## Engines

| | |
|---|---|
| ![GRAND](docs/screenshots/grand.png) | **GRAND**: two oscillators, noise, ladder filter, envelope, glide, MONO or POLY; LFO, sync, high-pass, key tracking, arpeggiator; a patch bay with cables. Live pictures of the waves, the filter and the envelope. |
| ![ACID](docs/screenshots/acid.png) | **ACID**: a 303-style line with saw or square, a filter with its envelope, ACCENT and SLIDE per step, four distortions and a sub oscillator. |
| ![SAMPLER](docs/screenshots/sampler.png) | **SAMPLER** plays WAV and OGG: pitch, root (from the name or the sound), gate or poly, direction; SLICES (4, 8, 16, 32) by EQUAL, TRANSIENTS or BY HAND; WARP to the tempo (BEATS, REPITCH, TONES, TEXTURE, COMPLEX); loop bars and BPM; NORMALIZE; fades. The picture shows where the sample is read now. |
| ![CLOUD](docs/screenshots/cloud.png) | **CLOUD** plays a sample as a cloud of grains: position, spray, size, density, shimmer, motion; window, reverse, pitch, intervals, sync, freeze and tone on the next pages. The picture shows the grains that sound now. |

Every synth engine has factory presets in categories and room for your own.

## Effects and modulation

![Signal path](docs/images/signal.svg)

| | |
|---|---|
| ![FX](docs/screenshots/fx.png) | **FX**: two inserts per pad, X switches one on or off, L1/R1 the other slot. Compressor (can be keyed from another pad), distortion, chorus, flanger, phaser, filter (LOW, HIGH, BAND, NOTCH, with Q), bitcrusher, tape, autopan, ring modulator, multiband. The picture shows what the effect does now: here the filter's response. |
| ![LFO](docs/screenshots/lfo.png) | **MOD > LFO**: four LFOs per pad with shape, rate or tempo division (up to 4 bars), depth and a start on the notes of any pad. Any card can be the target: EDIT on TARGET, then point at it. Four automation lanes per pattern, straight or bent, sit beside them. |
| ![DUCK](docs/screenshots/duck.png) | **MOD > DUCK**: a volume curve of 16 points started by the notes of another pad, by its sound passing a threshold, or by the tempo; depth and length. EDIT+up/down the level, EDIT+left/right draws. |

- **Sends**: A is a reverb, B a delay in tempo. **Master**: compressor and limiter.
- **Freeze**: SHIFT+Y on a pad renders it with its effects into audio; volume, pan, sends, mute and duck stay live.
- **Recording**: MENU+PLAY records what you play, with a count-in if set; what was played without recording can be
  captured afterwards. **Export** writes a WAV of every scene in turn, or one per pad.

## Simple view

PROJECT > INTERFACE > VIEW = SIMPLE, or hold F1+F2 for 3 seconds: three large screens for playing, also for a child (with the kid lock nothing gets
erased). F1/F2 change the screen, L1/R1 the pad, hold F1+F2 for 3 seconds to leave.

| PLAY | NOTES | SOUND |
|---|---|---|
| ![PLAY](docs/screenshots/simple-play.png) | ![NOTES](docs/screenshots/simple-notes.png) | ![SOUND](docs/screenshots/simple-sound.png) |
| EDIT plays a pad, EDIT+up/down its volume | EDIT a note, EDIT+up/down higher or lower | left/right a knob, up/down turn it, SHIFT+X dice |

## Install

You need a TrimUI Brick with Knulli and a way to copy files to it: the SD card in a computer, or the Knulli network
share.

1. Download `brickpad-1.1-beta-trimui-brick.zip` from the
   [latest release](https://github.com/obhateion/brickpad/releases/latest) and unpack it:
   ```
   BrickPad 1.1 Beta.sh                       the launcher
   brickpad-1.1/brickpad                      the program
   brickpad-1.1/projects/DEMO/SWITCHBACK.bpp demo: four scenes
   brickpad-1.1/projects/DEMO/NEON.bpp        demo
   ```
2. Copy `BrickPad 1.1 Beta.sh` and the folder `brickpad-1.1` into `roms/ports/` on the card (on the Brick itself:
   `/userdata/roms/ports/`). It has its own folder: an older BrickPad stays as it is.
3. Add your own WAV or OGG files in `roms/ports/brickpad-1.1/samples/`. Drums are
   recognised by their names and folders (kick, snare, hat...), loops by words or the tempo in the name (`120bpm`,
   `Amin`). The original SWITCHBACK drum kit is included.
4. On the Brick, update the game list and start **BrickPad 1.1 Beta** from **Ports**. Hold **MENU** for 3 seconds to
   quit (a ring shows how long is left).

**Demo projects.** Open `DEMO/SWITCHBACK` in PROJECT → FILE, then press PLAY. It is a four-scene electro/breakbeat
project at 124 BPM and plays with the included drum kit. `DEMO/NEON` uses its original sample packs. Without those packs,
its synth pads still play.

**Sound.** BrickPad plays through PipeWire. If a Dirtywave M8 is plugged in and took the default output, the launcher
switches the output back to the Brick's speaker and headphones. While BrickPad runs, the launcher keeps the CPU at full
speed so that busy songs do not break up, and restores the old setting on exit.

**Where your work is kept** (inside `roms/ports/brickpad-1.1/`): `state.bin` and `scenes.bin` are the autosave of what
is open, `projects/` the saved projects, `exports/` the WAV files, `frozen/` the cache of frozen pads, `themes/` your
themes. To update, replace the launcher and `brickpad-1.1/brickpad` only.

## Buttons

| Button | Does |
|---|---|
| D-pad | move the cursor |
| EDIT (B) | tap: open, act; hold + d-pad: change the value (left/right fine, up/down coarse) |
| OPTION (A) | listen to the pad; OPTION+EDIT: delete, or back to the default |
| PLAY | play / stop. With playback running, stops follow the clip launch grid |
| Y | back one level; on HOME and in the mixer: mute |
| X | HOME: the pad's clip; per screen: solo, insert on/off, chord |
| F1 / F2 | HOME: the next screen; inside a pad: the next pad; F1+F2: PROJECT; F1+F2 held 3 s: the simple view and back |
| SHIFT | the second layer; inside a pad SHIFT+arrow jumps to a category |
| L1 / R1 | pages, lanes, slots, the mixer drawer |
| SHIFT+L2 | undo |
| MENU | pads to play, again keys, again back; MENU+PLAY records; hold 3 s: quit |

![Button map](docs/images/buttons.svg)

Every screen shows its keys in the footer; PROJECT > ABOUT lists all of them by topic, and the
[manual](docs/MANUAL.md) explains every screen.

## Build from source

Cross-compiled on a PC (Windows, Linux or macOS) with [Zig](https://ziglang.org/) as the C compiler:

```sh
ZIG=/path/to/zig sh build.sh      # writes build/brickpad (aarch64 Linux, glibc)
```

The code is C11 with SDL2. `third_party/` holds the SDL2 headers and the device's `libSDL2-2.0.so.0` to link against
(SDL2, zlib licence), `stb_vorbis.c` and `font8x8_basic.h` (public domain). Building is allowed for your own devices
only: see the licence.

## Licence

Copyright (c) 2026 Timothy Gurto (obhateion). All rights reserved: see [LICENSE](LICENSE). You may read the code,
install the official releases and build it for your own devices, and send changes as pull requests; copies, ports,
modified versions, commercial use and the name BrickPad need written permission. Music you make with it is yours.
