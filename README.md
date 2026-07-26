# LIFT-X

![Release](https://img.shields.io/badge/release-v0.4.2-blue)
![License](https://img.shields.io/badge/license-AGPLv3-green)
![JUCE](https://img.shields.io/badge/JUCE-8.0.x-blue)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)

##
<img src="Source/Assets/Main.jpg" width="700">

## Overview

**LIFT-X** is a riser-dedicated MIDI synthesiser VST3 built around one idea: **a riser is not an envelope — it is thirty-one envelopes moving together.**

Most instruments give you one or two modulation sources and ask you to build a riser out of them. LIFT-X inverts that. Every parameter worth sweeping — three oscillator pitches, their levels, detune and stereo spread, noise pitch/level/resonance, four filters and twelve FX parameters — gets its own multi-point curve, and all thirty-one are read from the same playhead. Draw the shape you want on each one and they arrive together, locked to the host transport.

Pitch is handled differently too. The MIDI note is a **trigger only**; the actual pitch comes from a per-oscillator **Start Key → End Key** range, so a riser sweeps from exactly D2 to exactly G6 no matter which key you press. Turn on **Scale Quantize** and that sweep snaps to the notes of any of **70 scales**, turning a smooth glide into a stepped, in-key climb.

**Design goal:** total control over the shape of a transition, without leaving the plugin.


## Key Features

### 31 Multi-Point Envelopes on One Playhead

* **One playhead, thirty-one curves.** LIFT is the evaluation position on the X axis of every curve. In AUTO it follows the host transport across the chosen bar length; in MANUAL it is a knob you can automate.
* **Up to 128 points per curve**, each segment with its own tension handle. The same `evaluate()` code runs in the DSP and in the drawing, so what you see is exactly what you hear.
* **Curves live outside the host parameter list.** They are stored in a lock-free `CurveStore` rather than APVTS, which structurally isolates them from Ableton Live's automation rewind behaviour.
* **28 curve presets** (Linear / Exp / Log / S-Curve / Steps 4–32 / Saw 4–32 / Pulse 4–32 / Zigzag …) plus Save/Load of your own shapes, with an optional snap grid at 4/8/16/32/64 divisions.

### Absolute-Pitch Risers

* **Start Key → End Key per oscillator.** Press the ST or END button then play a note on your keyboard to set it (MIDI learn). The Pitch curve interpolates between them — bottom of the curve is Start, top is End.
* **MIDI note is trigger only.** The riser lands on the same notes every time regardless of which key fires it.
* **PITCH RAIL.** A live bar under each oscillator shows the current pitch as a note name, with tick marks at every scale tone in the Start–End span when quantizing.
* **Reverse.** One button flips the evaluation position to `1-pos`, reading all thirty-one curves backwards — a riser becomes a downer and vice versa.

### Scale Quantize — 70 Scales

<img src="Source/Assets/Config.jpg" width="700">

* **Stepped, in-key risers.** With quantize off the pitch glides along the curve; with it on the pitch snaps to the nearest note of the chosen Key and Scale, producing a staircase climb that always lands in key.
* **70 scales** across seven groups: Basic, Modes & Variants, World, Indian, Japan/Asia, Symmetric/Bebop and Chord Tones — from Major and Minor Pentatonic through Hirajoshi, Iwato and Ryukyu to Bebop Dominant and chord-tone sets like Minor 7th and Major 9th.
* **Per-oscillator apply.** Leave one oscillator unquantized to layer a smooth glide underneath a stepped one.
* **Quantize before COARSE.** The offset is added after snapping, so an oscillator set to +12 st stays exactly one octave above.
* **Automatic key snapping.** Changing Key, Scale or the apply toggles re-snaps each oscillator's Start and End keys to the closest scale tone. You can freely edit them afterwards, and preset loading never triggers it.

### Three Oscillators + Noise

* **Six wave modes** per oscillator — Sine, Triangle, Square, Saw, FM and **Wavetable** — with a POSITION knob that morphs continuously between frames.
* **Custom wavetables per oscillator**, loaded from a two-pane category browser with a RANDOM button. Your wavetable folder is remembered in a global settings file, so you register it once.
* **10-level mipmapped, FFT band-limited tables** — the harmonic count is chosen from the playback frequency, so a five-octave sweep does not alias at the top.
* **Up to 7-voice unison** with detune and stereo spread, both sweepable by their own curves.
* **White / Pink / Brown noise** through a dedicated band-pass, with its own pitch, resonance and range. Noise generation runs on a fixed 44.1 kHz clock and is interpolated up, so the spectrum is identical at every sample rate.

### Per-Source Filter Routing

<img src="Source/Assets/FILTER.jpg" width="700">

* **Four ZDF/TPT state-variable filters** (LP / HP / BP / Notch), Cytomic-style trapezoidal integration that stays stable under the fastest curve sweeps.
* **Each filter routes per source.** OSC 1–3 and Noise can be passed through or bypassed independently, so internally there are 4 × 4 = 16 filter instances.
* **Full-range envelope.** Cutoff × ENV AMT spans ±10 octaves, so the top of the curve always reaches maximum regardless of where the knob sits.

### Five-Slot FX Rack with Per-Effect Source Routing

<img src="Source/Assets/FX.jpg" width="700">

* **Choose the order.** Five slots, each assigned Saturation, Chorus, Delay, Reverb or Ducking.
* **Every effect routes per source.** Keep the sub oscillator out of the reverb, send only the noise to the delay, saturate OSC 1 while OSC 2 goes clean — 5 effects × 4 sources, individually switchable.
* **Saturation** with 10 ADAA algorithms (Soft Tanh, Hard Clip, Triode, Tape, Transformer, JFET, BJT, Wavefold, Exciter, Cubic), pre-HPF and output trim.
* **Tempo-locked Delay and Ducking.** Delay times and duck rates lock to the host PPQ from 1 Bar down to 1/64 with dotted and triplet values. Ducking needs no sidechain input.
* **Shimmer Reverb** — a 16-channel FDN with velvet-noise diffusion and an octave shifter.
* **Twelve FX parameters are curve-modulated**, including Delay Time (±2 octaves of beat length) and Duck Rate (quantised to musical multiples).

### Riser Capture & WAV Export

* The plugin's own output is recorded from note-on, cut exactly at the chosen bar length, plus 1.5 s of FX tail.
* **Drag the waveform strip into your DAW** to drop it as a 32-bit float WAV at the session sample rate.

### 70 Factory Presets

<img src="Source/Assets/PRESET.jpg" width="700">

Eleven categories — EDM, Trance, Bass, Techno, Cinematic, Downer, **Scale Riser**, Dubstep, DnB, Hardstyle and Ambient — covering waveforms, noise, filters, FX and curves. A three-column browser with subcategories, search, favourites and user presets sits on its own tab, and ◀▶ in the header steps through factory and user presets in one list.


## Parameter Reference

### Global (MAIN)

| Parameter | Range | Notes |
|---|---|---|
| **LIFT** | 0–100 % | Evaluation position of all 31 curves. Disabled in AUTO |
| **LIFT: AUTO / MANUAL** | toggle | AUTO follows the transport; MANUAL is host-automatable |
| **REVERSE** | on/off | Reads every curve backwards — riser ↔ downer |
| **RANDOM** | button | Randomises MAIN and OSC ENV only, never Master/FX/Config |
| **BARS** | 1/32 … 16 | Length of one full 0→1 sweep (10 steps) |
| **ATTACK** | 0.1–500 ms | Amp envelope attack |
| **RELEASE** | 5–4000 ms | Amp envelope release |
| **OUT** | −24 … +12 dB | Master output |
| **CEILING** | −12 … 0 dB | Limiter ceiling (mirrors CONFIG) |

### Oscillator 1–3

| Parameter | Range | Notes |
|---|---|---|
| **ON / S / M** | toggle | Enable, Solo, Mute |
| **WAVE** | 6 modes | Sine, Triangle, Square, Saw, FM, Wavetable |
| **POS** | 0–100 % | Morph position (Wavetable mode) |
| **LEVEL** | 0–100 % | Curve-modulated |
| **COARSE** | −24 … +24 st | Added after scale quantizing |
| **UNISON** | 1–7 | Voice count |
| **DETUNE** | 0–100 ct | Curve-modulated |
| **SPREAD** | 0–100 % | Stereo width, curve-modulated |
| **ST / END** | C−2 … G8 | Start/End Key, set by MIDI learn |

### Noise

| Parameter | Range | Notes |
|---|---|---|
| **TYPE** | White / Pink / Brown | Fixed 44.1 kHz generation, SR-independent |
| **LEVEL** | 0–100 % | Curve-modulated |
| **PITCH** | 20 Hz – 20 kHz | Band-pass centre, curve-modulated |
| **RES** | 0.5–12 | Band-pass Q, curve-modulated |
| **RANGE** | 0–10 oct | Depth of the pitch curve |

### Filter 1–4

| Parameter | Range | Notes |
|---|---|---|
| **ENABLE** | on/off | |
| **ROUTE** | OSC1/2/3/Noise | Per-source pass or bypass |
| **TYPE** | LP / HP / BP / Notch | ZDF/TPT SVF |
| **CUTOFF** | 20 Hz – 20 kHz | |
| **RES** | 0.5–12 | |
| **ENV AMT** | −100 … +100 % | Curve × ±10 octaves |

### FX

| Effect | Parameters |
|---|---|
| **Saturation** | AMT, ALGO (10), DRIVE 1–12, PRE HPF 20–2000 Hz, TRIM ±12 dB |
| **Chorus** | AMT, RATE 0.05–8 Hz, DEPTH, WIDTH |
| **Delay** | AMT, TIME (1/2 … 1/16T), FB 0–95 %, DUCK, DAMP |
| **Reverb** | AMT, DECAY, SHIMMER, DAMP, MOD |
| **Ducking** | AMT, RATE (1 Bar … 1/64), SHAPE 0.5–8 |

All five have a **ROUTE** row for OSC 1–3 and Noise.

### Config

| Parameter | Range | Notes |
|---|---|---|
| **SCALE QUANTIZE** | on/off | Default off |
| **KEY** | C … B | |
| **SCALE** | 70 scales | Grouped by category |
| **APPLY TO** | OSC 1/2/3 | Per-oscillator |
| **LIMITER ON** | on/off | Brick-wall, zero latency, no PDC |
| **CEILING** | −12 … 0 dB | |
| **RELEASE** | 20–1000 ms | |
| **COLOR THEME** | 10 themes | Midnight, Sakura, Ocean, Forest, Sunset, Mono, Neon, Vaporwave, Amber, Arctic |

### Curve Editor

<img src="Source/Assets/OSCEnv.jpg" width="700">

* **Double-click** to add or remove a point, **drag the diamond** on a segment to bend it.
* **CURVES** opens 28 factory shapes plus "Save Current…" and your saved user curves.
* **SNAP** locks point X positions to a 4/8/16/32/64 grid with guide lines.
* Bipolar curves show a centre line: **centre = knob value, top = maximum, bottom = minimum**.


## Signal Flow

```
MIDI note (trigger only)
   │
   ├─ Transport sync (PPQ) ──► Progress 0→1 over BARS
   │                              │
   │                    LIFT (AUTO=Progress / MANUAL=knob)
   │                              │  × REVERSE → 1-pos
   │                              ▼
   │                    ┌── 31 curves evaluated at one playhead ──┐
   │                    │                                          │
   ▼                    ▼                                          ▼
OSC 1 ─ wavetable ─ unison ─┐                            Filter cutoff/res
OSC 2 ─ wavetable ─ unison ─┤─ per-source filter routing        FX params
OSC 3 ─ wavetable ─ unison ─┤   (4 filters × 4 sources)
Noise ─ band-pass ──────────┘
   │
   ▼
4 separate source buses ──► FX chain (5 slots, per-effect source routing)
   │                          Saturation / Chorus / Delay / Reverb / Ducking
   ▼
Bus sum ──► Master gain ──► Safety clip ──► Brick-wall limiter ──► Output
                                                     │
                                                     └─► Riser capture → WAV drag
```

Pitch is computed as `StartKey + (EndKey − StartKey) × curve`, optionally snapped to Key + Scale, then COARSE is added.


## Real-Time Safety

* **No allocation or locking in `processBlock`.** Every buffer — oscillator tables, filter states, source buses, FX delay lines, the capture buffer — is sized in `prepareToPlay`.
* **Lock-free curve publishing.** The editor writes into an 8-slot ring per curve and publishes an atomic index; the audio thread only ever reads.
* **Sample-rate independent smoothing.** Control-tick coefficients are derived from real-time constants, so 44.1 kHz and 192 kHz behave identically. Noise is generated on a fixed 44.1 kHz clock and interpolated, keeping the Pink/Brown spectrum and White level constant across sample rates.
* **Two-stage declick on retrigger.** A note-on during playback fades out over 1.5 ms, performs every reset while the output is exactly zero, then fades back in — so phase resets, smoother snaps and filter clears can never produce a step.
* **DAW fail-safe layer.** Sample-rate mismatch or an absurd block size clears the buffer and resets the engine instead of producing garbage. Block sizes larger than the host advertised are absorbed by generous pre-allocation.
* **Bounded state.** Filter and noise cutoff targets are clamped to 20 Hz – 0.45 × SR, delay feedback tops out at 0.95, inter-slot soft clipping sits between FX slots, and a brick-wall limiter guards the output.
* **Background-safe file work.** Wavetable decoding, FFT mip building and preset I/O run on the message thread; the audio thread reads an atomic pointer that is never freed while it might be in use.


## 📚 Manual

Quick manuals covering every tab and parameter, plus starting-point settings and troubleshooting:

[ ![Manual (EN)](https://img.shields.io/badge/Manual-English-blue?style=for-the-badge) ](Source/Assets/LIFTX_Manual_EN.md)
[ ![Manual (JP)](https://img.shields.io/badge/Manual-日本語-red?style=for-the-badge) ](Source/Assets/LIFTX_Manual_JP.md)


## Installation

1. Download `LIFT-X.vst3` from the Releases page.
2. Copy it to your VST3 directory:
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. Rescan plugins in your DAW.

### Build Requirements

* **JUCE** 8.0.x — place at `C:/JUCE` or update `JUCE_PATH` in `CMakeLists.txt`
* **CMake** 3.22 or higher
* **Visual Studio** 2022 (MSVC, C++20)

```bash
cmake -B build -DJUCE_PATH=C:/JUCE
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD` is enabled, so the VST3 is copied to the system VST3 folder automatically after a successful build.


## System Requirements

* **OS:** Windows 10 / 11 (64-bit)
* **Format:** VST3 / Standalone
* **Sample rates:** 44.1 – 192 kHz
* **Tested Host:** Ableton Live 11 / 12

> ⚠️ **Compatibility Notice:** Verified operation is confirmed in **Ableton Live**. Other DAWs may work but are currently unverified.


## Tips

* **Start from a preset, then redraw one curve.** The factory bank is built so that changing a single Pitch or Filter curve gives you a different riser rather than a broken one.
* **Stepped climbs that stay in key:** turn on Scale Quantize, pick Minor Pentatonic, and set an oscillator's Start/End three octaves apart. The pitch walks up the scale instead of gliding.
* **Layer stepped and smooth:** leave OSC 2 out of APPLY TO so it glides underneath the quantized OSC 1. The two together read as one instrument.
* **Turn any riser into a downer:** press REVERSE. Every curve — pitch, filter, FX — flips at once, which is far more convincing than reversing the pitch alone.
* **Feed only the noise to the delay.** On the DELAY tab switch OSC 1–3 off in ROUTE and leave NOISE on. The tonal layer stays dry while the noise builds a rhythmic tail.
* **Keep the sub clean:** on the REVERB tab, switch off the oscillator carrying the low octave. The top stays wide and the bottom stays defined.
* **RANDOM as a starting point.** It only touches MAIN and OSC ENV, so you can keep an FX chain you like and reroll the tonal layer underneath it.
* **Draw the shape you hear.** Use a Steps 8 or 16 curve preset on Pitch for machine-gun risers, and an Exp Up (Hard) on Filter for the classic late-opening sweep.
* **Bounce without rendering:** trigger the riser once, then drag the waveform strip straight into your arrangement as a WAV.


## License

This project is licensed under the GNU Affero General Public License v3.0 (AGPLv3) — see [LICENSE](LICENSE) for details.

This software is built with the **JUCE 8** framework. In accordance with JUCE 8's open-source licensing terms, this entire project is distributed under the AGPLv3.


## Credits

**Developer:** @kijyoumusic (OTODESK)

**Framework:** JUCE 8.0.x

**Target DAW:** Ableton Live 11 / 12

**DSP References:**
- Zavalishin — *"The Art of VA Filter Design"* (TPT/SVF topology)
- Simper (Cytomic) — *Trapezoidal integrated state-variable filters*
- Parker, Zavalishin & Le Bivic — *"Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution"* (ADAA, 2016)
- Kellett — *Pink noise filter coefficients*
- Jot & Chaigne — *"Digital Delay Networks for Designing Artificial Reverberators"* (FDN, 1991)


## Support

Found a bug or have a feature request? Please open an issue on the repository.
