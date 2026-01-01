# Drift Engine

A granular sample explorer plugin for the Expert Sleepers distingNT Eurorack module.

## Overview

Drift Engine creates ambient, evolving textures from samples using 4 autonomous "drifters" that wander through the audio, each in its own frequency band and stereo position. Poisson-triggered grains create organic, ever-changing soundscapes.

## Features

- **4 Autonomous Drifters**: Each explores a different region of the sample with independent movement
- **Frequency Band Separation**: Drifters operate in distinct frequency bands (250Hz, 750Hz, 1550Hz, 4000Hz)
- **Stereo Spread**: Each drifter has a fixed stereo position for spatial width
- **Drifter Repulsion**: Drifters gently push apart when close, preventing clustering
- **Clock Sync**: Grain triggering can sync to external clock with variable deviation
- **Waveform Display**: Visual overview of sample amplitude in the position bar

## Parameters

### Sample Page
- **Folder**: Sample folder selection
- **Sample**: Sample selection within folder

### Position Page
- **Anchor**: Centre point for drifter movement (0-100%)
- **Wander**: Range of movement around anchor (0-100%)
- **Gravity**: Pull toward/away from anchor (-100 to +100%)
- **Drift**: Base movement speed (0-100%)

### Density Page
- **Density**: Grain rate and size (0-100%)
- **Deviation**: Clock sync tightness (0% = strict clock, 100% = free Poisson)

### Pitch Page
- **Pitch**: Master pitch offset (-24 to +24 semitones)
- **Scatter**: Per-drifter pitch spread (0-12 semitones)

### Spectral Page
- **Spectrum**: Filter bank separation amount (0-100%)
- **Tilt**: Spectral balance, dark to bright (-100 to +100%)

### Character Page
- **Shape**: Grain envelope (Mist, Cloud, Rain, Hail, Ice)
- **Entropy**: Randomness/chaos amount (0-100%)

### Routing Page
- Audio outputs (L/R) with replace/add modes
- CV inputs for modulation (Anchor, Pitch, Drift, Entropy, Storm, Clock)
- CV outputs (Position, Pulse)

## Hardware Controls

| Control | Normal | Push+Turn | Press |
|---------|--------|-----------|-------|
| Pot L | Density | Deviation | - |
| Pot C | Anchor | Wander | - |
| Pot R | Spectrum | Tilt | - |
| Enc L | Gravity | - | Prev Sample |
| Enc R | Entropy | - | Next Sample |

## CV Inputs

- **Anchor CV**: Modulate playback position (±5V = ±50%)
- **Pitch CV**: 1V/octave pitch control
- **Drift CV**: Modulate drift speed (±5V = ±100%)
- **Entropy CV**: Add chaos (0-5V = 0-100%)
- **Storm Gate**: Gate high instantly maxes entropy
- **Clock**: Sync grain triggers to external clock

## CV Outputs

- **Position**: Average drifter position (0-5V)
- **Pulse**: Trigger on each grain spawn (5V pulse)

## Display

The main display shows:
- Sample folder and filename
- Sample duration
- Position bar with:
  - Waveform amplitude overview
  - Wander range (dim region)
  - Anchor position (vertical line)
  - Drifter positions (markers above/below bar)
- Active grain count
- Gravity indicator (bipolar bar)
- Entropy indicator (bar)
- Storm indicator (when active)

## Building

Requires ARM toolchain for distingNT:

```bash
# Install toolchain (macOS)
brew install arm-none-eabi-gcc

# Build for hardware
make hardware

# Copy plugins/drift_engine.o to distingNT SD card
```

## Credits

- Developer: Ns (nealsanche)
- Plugin ID: NsDr
- Built with distingNT API v9
