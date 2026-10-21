# RaveForge

A JUCE audio effect (VST3 / AU / Standalone) in the spirit of Rave Generator:
it turns any input into a big detuned "rave" stack, then runs it through a
filter, distortion, LFO, trance gate, delay and reverb.

![RaveForge UI](docs/screenshot.png)

## Controls

| Section | Controls |
| --- | --- |
| **Rave** | **Power** fades in up to 6 detuned copies of the input (widest pair first). **Width** spreads them across the stereo field. **Detune** (0–50 cents). Pitched layers: **Sub Oct**, **Fifth**, **Oct Up**. |
| **Filter** | Low / band / high pass, **Cutoff**, **Resonance**. An envelope follower on the input moves the cutoff (**Env Amt**, **Attack**, **Release**). |
| **Dirt** | **Drive** into Soft / Hard / Fold / Tube shapes, **Bits** reduction, **Downsample**, **Mix**. |
| **LFO** | Sine / Triangle / Saw / Square / S&H. Tempo-synced (**Division**) or free (**Rate** in Hz). Routes to **Cutoff**, **Detune** and **Volume**. |
| **Trance Gate** | 16 steps, 6 patterns, **Rate** (1/8, 1/16, 1/32), **Depth**, **Smooth**. |
| **Delay** | Tempo-synced **Time**, **Feedback**, **Ping Pong**, **Tone** (darkens the repeats), **Mix**. |
| **Reverb** | **Size**, **Damping**, **Width**, **Mix**. |
| **Master** | **Dry/Wet**, **Output**. |

The LFO, gate and delay follow the host tempo and bar position while the
transport is playing, and run free at the host tempo (or 128 BPM) when it is stopped.

Signal flow:

```
input → rave stack → filter (env + LFO) → dirt → LFO volume × gate → delay → reverb → dry/wet → output
```

## Building

Needs CMake 3.22+ and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on
Windows, GCC/Clang on Linux). JUCE 8 is downloaded automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

To use a JUCE checkout you already have instead of downloading it, add
`-DJUCE_DIR=/path/to/JUCE` to the first command.

Built plugins land in `build/RaveForge_artefacts/Release/` (`VST3/`, `AU/`,
`Standalone/`). Copy them to your plugin folder:

- macOS: `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`
- Windows: `C:\Program Files\Common Files\VST3`
- Linux: `~/.vst3`

On Linux, install JUCE's dependencies first (`libasound2-dev libfreetype-dev
libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl-dev` and friends).

## Testing

`RaveForgeTest` renders a saw riff through the defaults, every parameter maxed,
a "showcase" patch and 20 random settings, and fails on any NaN/inf or runaway
level. It also writes `defaults.wav`, `showcase.wav` and a `ui.png` snapshot.

```bash
cmake --build build --target RaveForgeTest
./build/RaveForgeTest_artefacts/Release/RaveForgeTest out
```

## Layout

```
Source/
  Parameters.h        parameter IDs, choice lists, APVTS layout
  PluginProcessor.*   reads parameters + host tempo, runs the engine
  PluginEditor.*      UI (sections, knobs, gate step display)
  RaveLookAndFeel.h   colours and knob drawing
  DSP/
    Engine.h          the full effect chain
    RaveStack.h       pitch-shift voices for the detuned stack and layers
    Dirt.h            waveshaper, bitcrush, downsample
    Modulation.h      envelope follower, LFO, trance gate
    Delay.h           synced stereo / ping-pong delay
Tests/RenderTest.cpp  offline render smoke test
```

## Notes

- The distortion isn't oversampled, so heavy drive on bright material will alias.
- The pitch-shift voices add a little latency-like smear (the grain window is
  30–60 ms), which is part of the chorus character; it isn't reported to the host.
