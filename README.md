# AdaptiveIR - Dynamic Reverb Adaptivity Plugin

**AdaptiveIR** is a specialized VST3/AU audio plugin for parallel processing. It applies a convolution reverb (Impulse Response) that dynamically adapts its spectral content to the input signal to prevent frequency masking and "muddy" tails.

## Features
- **Dynamic Spectral Matching**: Automatically carves out space in the reverb tail for the dry signal's transient.
- **Adjustable Octave Resolution**: From coarse (1/oct) to fine (1/4 oct) spectral analysis.
- **Visual Feedback**: Real-time EQ magnitude display showing the dynamic curve being applied.
- **Workflow-Focused**: Designed for 100% Wet use on parallel FX tracks.
- **Flexible Routing**: Apply adaptive EQ either to the dry signal (Pre-Convolution) or to the reverb tail (Post-Convolution).

## Build Instructions

### Prerequisites
- **CMake** (v3.22+)
- **GCC/G++** (Linux) or **Visual Studio 2022** (Windows)
- **Linux Dependencies** (for JUCE/GTK): `libgtk-3-dev`, `libwebkit2gtk-4.0-dev`, `libcurl4-openssl-dev`, `libasound2-dev`, `libfreetype6-dev`, `libfontconfig1-dev`.

### Linux Build
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```
The plugin will be installed automatically to `~/.vst3/AdaptiveIR.vst3`.

### Windows Build
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

## Usage
1. Load an **Impulse Response** (WAV) file.
2. Set the **Sensitivity** to trigger analysis on transients.
3. Adjust the **Release** to control how fast the EQ returns to neutral.
4. Use **Resolution** to define the spectral detail of the adaptivity.

## License
MIT
