# Drift Engine — Quick Reference

## Concept
4 autonomous drifters wander through a sample, each in its own frequency band and stereo position. Poisson-triggered grains create organic, evolving ambient worlds.

## Controls (12 knobs + 1 button)

### Position
| Control | Function |
|---------|----------|
| ANCHOR | Where drifters cluster (0-100% of sample) |
| WANDER | How far they can stray from anchor |
| GRAVITY | Attract to / repel from anchor (bipolar) |
| DRIFT | Speed of movement through sample |

### Density & Timing
| Control | Function |
|---------|----------|
| DENSITY | Sparse long grains ↔ Dense short grains |
| DEVIATION | Clock-locked ↔ Free Poisson (when clocked) |

### Pitch & Spectrum
| Control | Function |
|---------|----------|
| PITCH | Master pitch ±2 octaves |
| SCATTER | Pitch spread across drifters |
| SPECTRUM | Filter bank separation (fullrange ↔ isolated bands) |
| TILT | Spectral balance (dark ↔ bright) |

### Character
| Control | Function |
|---------|----------|
| SHAPE | Grain envelope (Mist/Cloud/Rain/Hail/Ice) |
| FOG | Reverb send + decay |
| ENTROPY | Master chaos (position, timing, pitch) |
| STORM | Momentary chaos burst (button) |

## CV Inputs
ANCHOR, PITCH (V/Oct), DRIFT, ENTROPY, STORM gate, CLOCK

## CV Outputs
DRIFT OUT (averaged position), PULSE (grain triggers)

## The Four Drifters

| # | Stereo | Frequency Band | Phase (clocked) |
|---|--------|----------------|-----------------|
| 1 | Hard L | Sub (20-200Hz) | 0% |
| 2 | Mid L | Low-Mid (200-800Hz) | 25% |
| 3 | Mid R | Mid-High (800-4kHz) | 50% |
| 4 | Hard R | Air (4-20kHz) | 75% |

## Key Interactions
- **WANDER × GRAVITY** = Position behavior (orbiting, fleeing, clustering)
- **SPECTRUM × SCATTER** = Spectral width (filtering × transposition)
- **DENSITY × CLOCK** = Rate becomes clock mult/div when patched
- **DEVIATION × ENTROPY** = Rhythmic precision vs textural chaos

## Design Principles
1. Sounds beautiful with no patching (autonomous exploration)
2. Poisson triggering = organic, never mechanical
3. Chaos is structural, not bolted-on
4. Filter bank creates worlds, not copies
5. Clock optional—ambient first, rhythmic when needed
