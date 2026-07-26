# LIFT-X Quick Manual (English)

**English** | [日本語](LIFTX_Manual_JP.md)

---

## Contents

1. [Making a Sound](#1-making-a-sound)
2. [How the Screen Is Laid Out](#2-how-the-screen-is-laid-out)
3. [MAIN Tab](#3-main-tab)
4. [OSC ENV Tab](#4-osc-env-tab)
5. [FILTER Tab](#5-filter-tab)
6. [FX Tab](#6-fx-tab)
7. [CONFIG Tab](#7-config-tab)
8. [Presets](#8-presets)
9. [Recipes](#9-recipes)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Making a Sound

**OTODESK / Riser-dedicated MIDI synthesiser (VST3 / Standalone)**

LIFT-X builds transitions. Thirty-one multi-point envelopes sweep pitch, level, detune, spread, filters and FX together, locked to the host transport. It ships with 70 factory presets, 70 scales for quantized pitch climbs, and drag-to-DAW WAV export.

### Install

1. Copy `LIFT-X.vst3` to:
   ```
   C:\Program Files\Common Files\VST3\
   ```
2. Rescan plugins in your DAW.

### First Steps

1. Load LIFT-X on an instrument track and open the **PRESET** tab.
2. Pick something from **EDM → Classic Saw Riser** and press **Close**.
3. Start the transport and hold a MIDI note for four bars.

The riser sweeps from its Start Key to its End Key over the length set by **BARS**, and the PROGRESS bar shows how far along it is.

> **The MIDI note is a trigger only.** Which key you press does not change the pitch — the Start Key and End Key of each oscillator do. Hold the note for the whole riser; releasing it starts the release stage.

> **Not hearing anything?** LIFT-X needs the host transport running to advance in AUTO mode. If your DAW is stopped, switch **LIFT: AUTO** to **LIFT: MANUAL** and turn the LIFT knob by hand.

---

## 2. How the Screen Is Laid Out

The header holds the **LIFT: AUTO / MANUAL** toggle, six tabs, and preset navigation (◀ name ▶) on the right.

| Tab | Contents |
|---|---|
| **MAIN** | LIFT / BARS / REVERSE / RANDOM / ATTACK / RELEASE, progress and waveform, three oscillators, noise, master |
| **OSC ENV** | Pitch / Level / Detune / Spread curves for OSC 1–3 and Noise |
| **FILTER** | Four filters with per-source routing and their cutoff curves |
| **FX** | Five-slot chain, per-effect source routing, FX parameters and their curves |
| **PRESET** | Three-column browser: category, subcategory, preset list |
| **CONFIG** | Scale quantize, master limiter, colour theme |

### The One Idea Worth Understanding

**LIFT is a playhead.** It is a position from 0 to 1 on the X axis, and *every* curve in the plugin is read at that same position.

* **AUTO** — the playhead follows the host transport across the length set by BARS. The LIFT knob moves on its own and cannot be dragged.
* **MANUAL** — the LIFT knob *is* the playhead. Stop it at 68 % and the sound freezes there. This is the mode to automate from your DAW if you want non-linear timing.

---

## 3. MAIN Tab

<img src="Main.jpg" width="700">

### Global Controls

| Control | What it does |
|---|---|
| **LIFT** | Playhead position, 0–100 % |
| **BARS** | How long one full sweep takes: 1/32, 1/16, 1/8, 1/4, 1/2, 1, 2, 4, 8, 16 bars |
| **REVERSE** | Reads all 31 curves backwards. A riser becomes a downer and vice versa |
| **RANDOM** | Randomises MAIN and OSC ENV within musical limits. Never touches Master, FX, Filter or Config |
| **ATTACK** | 0.1–500 ms |
| **RELEASE** | 5–4000 ms |
| **PROGRESS** | Position in time, always 0→100 % even with REVERSE on |
| **Waveform strip** | The recorded riser. **Drag it into your DAW** to export a 32-bit float WAV |

> **REVERSE vs. swapping keys.** You could swap Start and End Key by hand, but REVERSE also flips the filter sweep, the FX curves and the level shape — the whole gesture inverts, which is what actually makes a downer sound like a downer.

### Oscillator 1–3

| Control | What it does |
|---|---|
| **OSC n / S / M** | Enable, Solo, Mute |
| **Wave menu** | Sine, Triangle, Square, Saw, FM, Wavetable |
| **Waveform display** | Live preview of the current wave and morph position |
| **BROWSE / RND** | Load a custom wavetable, or pick one at random |
| **POS** | Morph position through the wavetable (Wavetable mode) |
| **LEVEL** | 0–100 % |
| **COARSE** | −24 … +24 semitones, added *after* scale quantizing |
| **UNISON** | 1–7 voices |
| **DETUNE** | 0–100 cents |
| **SPREAD** | Stereo width of the unison voices |
| **PITCH rail** | Live pitch as a note name, with scale tick marks when quantizing |
| **ST / END** | Start and End Key. Click, then play a note on your keyboard |

> **Setting the pitch range:** click **ST**, play the note you want the riser to start on, then click **END** and play the note it should reach. Click a button twice to cancel.

### Noise

White, Pink or Brown noise through a band-pass filter with its own **PITCH** (centre frequency), **RES** (Q) and **RANGE** (how many octaves the pitch curve sweeps).

### Master

**OUT** is the final output level; **CEILING** mirrors the limiter ceiling on the CONFIG tab.

---

## 4. OSC ENV Tab

<img src="OSCEnv.jpg" width="700">

Four curves per oscillator, three for noise — fifteen in total on this tab.

| Row | Curve | Meaning |
|---|---|---|
| Source | OSC 1 / OSC 2 / OSC 3 / NOISE | Which source to edit |
| Target | PITCH / LEVEL / DETUNE / SPREAD | Which parameter (Noise: PITCH / LEVEL / RES) |

### Reading a Curve

* **PITCH** is unipolar: the **bottom of the graph is the Start Key, the top is the End Key**. A straight line from bottom-left to top-right is a plain linear riser.
* **Everything else is bipolar**: the **centre line is the current knob value**, the top pushes toward maximum and the bottom toward minimum. A flat line through the centre means no modulation at all.

### Editing

| Action | Result |
|---|---|
| **Drag a point** | Move it |
| **Double-click empty space** | Add a point |
| **Double-click a point** | Remove it |
| **Drag the diamond** on a segment | Bend that segment (tension) |
| **CURVES** | 28 factory shapes, "Save Current…", and your saved user curves |
| **SNAP** | Lock point X positions to a 4/8/16/32/64 grid |

The 28 factory shapes cover Linear Up/Down, Exp and Log in both directions, S-Curves, Ramp+Hold, Triangle, V Shape, **Steps 4/8/16/32**, **Saw 4/8/16/32**, **Pulse 4/8/16/32**, Zigzag Up, Flat Center and Flat Max.

> **Steps and Pulse shapes are how you get rhythmic risers** without touching the FX. A Steps 16 on Pitch gives a machine-gun climb; a Pulse 8 on Level gives a gated one.

---

## 5. FILTER Tab

<img src="FILTER.jpg" width="700">

Four independent ZDF/TPT state-variable filters.

| Control | What it does |
|---|---|
| **FLT 1–4** | Which filter to edit |
| **ENABLE** | Filter on/off |
| **ROUTE** | Which sources pass through this filter — OSC 1, OSC 2, OSC 3, NOISE |
| **TYPE** | LowPass / HighPass / BandPass / Notch |
| **CUTOFF** | 20 Hz – 20 kHz |
| **RES** | 0.5–12 |
| **ENV AMT** | −100 … +100 %, how far the curve moves the cutoff |

**ROUTE is the important one.** Each filter can be applied to any combination of sources, so you can high-pass only the noise while the oscillators stay full-range, or run two different low-passes on two oscillators.

The curve × ENV AMT spans ±10 octaves, which means the top of the curve reaches the maximum cutoff no matter where the CUTOFF knob sits. Negative ENV AMT inverts the sweep.

---

## 6. FX Tab

<img src="FX.jpg" width="700">

### CHAIN

Five slots, processed left to right. Each slot is set to None, Saturation, Chorus, Delay, Reverb or Ducking, so you control the order.

### ROUTE

Every effect has its own **ROUTE: OSC 1 / OSC 2 / OSC 3 / NOISE** row. **Sources switched off bypass that effect entirely** and pass through clean. This is per effect, not per chain — you can saturate OSC 1, chorus OSC 2, and send only the noise to the delay.

### Effects

| Effect | Parameters |
|---|---|
| **SAT** | AMT, ALGO (Soft Tanh, Hard Clip, Triode, Tape, Transformer, JFET, BJT, Wavefold, Exciter, Cubic), DRIVE 1–12, PRE HPF 20–2000 Hz, TRIM ±12 dB |
| **CHORUS** | AMT, RATE 0.05–8 Hz, DEPTH, WIDTH |
| **DELAY** | AMT, TIME (1/2 … 1/16T, dotted and triplet), FB 0–95 %, DUCK, DAMP |
| **REVERB** | AMT, DECAY, SHIMMER, DAMP, MOD |
| **DUCK** | AMT, RATE (1 Bar … 1/64), SHAPE 0.5–8 |

Delay and Ducking lock to the host PPQ, so they stay on the grid without a sidechain input.

### FX Curves

The **ENV:** sub-tabs open the curves for that effect. All twelve curve-modulated FX parameters are bipolar — centre is the knob value. Delay **TIME** and Duck **RATE** are special: the curve shifts them by ±2 octaves of beat length, with lower meaning faster.

> **A Delay Time curve that ramps down** gives the classic accelerating-echo build.

---

## 7. CONFIG Tab

<img src="Config.jpg" width="700">

### PITCH ENV — SCALE

| Control | What it does |
|---|---|
| **SCALE QUANTIZE** | Off: pitch glides along the curve. On: pitch snaps to scale tones |
| **KEY** | C … B |
| **SCALE** | 70 scales, grouped: Basic, Modes & Variants, World, Indian, Japan/Asia, Symmetric/Bebop, Chord Tones |
| **APPLY TO** | Which oscillators are quantized |

With quantize on, a linear pitch curve becomes a **staircase** — the riser walks up the scale instead of sliding. COARSE is added *after* quantizing, so an oscillator at +12 st stays exactly one octave above.

Changing KEY, SCALE or APPLY TO also **re-snaps each oscillator's Start and End keys** to the closest scale tone. You can edit them freely afterwards, and loading a preset never triggers it.

> **Chord Tones scales** (Major Triad, Minor 7th, Major 9th …) turn a Pitch curve into an arpeggio — the sweep only lands on chord tones.

### MASTER LIMITER

Brick-wall with instant attack and zero latency, so no plugin delay compensation is needed. **CEILING** −12…0 dB, **RELEASE** 20–1000 ms.

### APPEARANCE

Ten colour themes: Midnight, Sakura, Ocean, Forest, Sunset, Mono, Neon, Vaporwave, Amber, Arctic. Reopen the plugin window to apply a theme fully.

---

## 8. Presets

<img src="PRESET.jpg" width="700">

70 factory presets across eleven categories: **EDM, Trance, Bass, Techno, Cinematic, Downer, Scale Riser, Dubstep, DnB, Hardstyle, Ambient**.

| Action | How |
|---|---|
| **Load** | Click a preset in the right column |
| **Filter** | Category (All / Factory / User / Favorites) then Subcategory |
| **Search** | Type in the search box |
| **Favourite** | Click the ☆ |
| **Save** | Type a name, optionally a subcategory, press **Save** |
| **Delete** | Right-click a user preset |
| **Init** | Reset everything to defaults |
| **Step** | ◀▶ in the header walks through factory and user presets in one list |

User presets are stored in `%APPDATA%\LIFT-X\Presets`, user curves in `%APPDATA%\LIFT-X\Curves`.

---

## 9. Recipes

**Classic supersaw riser**
Saw wave, UNISON 7, DETUNE around 40 ct, SPREAD 100 %. Start Key C2, End Key C6, BARS 4. Filter 1 as LowPass with CUTOFF 600 Hz and ENV AMT 80 %, curve rising with a slight upward bend. Add Reverb and Ducking at 1/4.

**Stepped, in-key climb**
CONFIG → SCALE QUANTIZE on, KEY to your track's key, SCALE Minor Pentatonic. Square wave, UNISON 3, Start/End three octaves apart, BARS 8. Give Pitch a slightly exponential curve so the steps accelerate. Add Delay at 1/16 with FB 60 %.

**Noise sweep with a rhythmic tail**
Turn all three oscillators off. Noise LEVEL 90 %, PITCH 250 Hz, RANGE 6 oct, with a rising Noise Pitch curve. On the DELAY tab set ROUTE to NOISE only, TIME 1/16, FB 60 %. Add Ducking at 1/16.

**Downer from any riser**
Load any riser preset and press **REVERSE**. Shorten BARS to 1/2 or 1 and RELEASE to around 300 ms for a fast drop.

**Layered stepped and smooth**
Set up a quantized OSC 1 as above, then enable OSC 2 with COARSE −12, and switch OSC 2 **off** in CONFIG → APPLY TO. The sub glides while the top steps.

**Cinematic swell**
Saw with UNISON 7 and low DETUNE, plus a sine an octave down on OSC 3. ATTACK 200 ms, RELEASE 1200 ms, BARS 8. Reverb AMT 65 %, DECAY 90 %, SHIMMER 50 %.

---

## 10. Troubleshooting

**No sound, or the riser never moves**
In AUTO the playhead only advances while the host transport is running. Start playback, or switch to MANUAL and move the LIFT knob.

**The riser starts on the wrong note**
The MIDI note is a trigger only. Set the pitch with **ST** and **END** on each oscillator, not by playing a different key.

**The pitch does not step even with Scale Quantize on**
Check **APPLY TO** on the CONFIG tab — the oscillator you are listening to may be excluded.

**Start/End keys moved on their own**
Changing KEY, SCALE or APPLY TO re-snaps them to the nearest scale tone by design. Edit them afterwards and they stay put. Loading a preset never re-snaps.

**A source is not being affected by an effect**
Check the **ROUTE** row on that effect's tab. Sources switched off bypass the effect completely.

**The preset name shows "Init" after reopening the project**
This was fixed in v0.4.0. Projects saved with an older build have no stored name.

**Wavetable did not reload**
Custom wavetables are referenced by file path. If the file moved or the drive was disconnected, the oscillator falls back to its built-in wave. Reload it from **BROWSE**.

**Theme change looks incomplete**
Close and reopen the plugin window.

**CPU is high**
Unison multiplies oscillator cost — three oscillators at 7 voices is 21 voices. Reverb and Delay are the heaviest FX; the source routing does not multiply their cost, but disabling unused slots does help.

---

*LIFT-X — OTODESK*
