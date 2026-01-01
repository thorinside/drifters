# Drift Engine PRD
## Granular Sample Explorer for distingNT

*"Build ambient worlds from frozen moments"*

---

## Overview

**Drift Engine** is a granular sample explorer that transforms static samples into slowly evolving ambient soundscapes. Unlike traditional granular processors focused on real-time manipulation, Drift Engine emphasizes **autonomous exploration**—the plugin "wanders" through your samples, generating endless variations with minimal intervention.

The core metaphor is **drift**: multiple independent grain streams slowly navigate through sample material, occasionally converging, diverging, and discovering new territories.

---

## Design Philosophy

### What Makes This Different

| Traditional Granular | Drift Engine |
|---------------------|--------------|
| User controls position | Drifters explore autonomously |
| Add randomness to parameters | Chaos is structural, not additive |
| One grain stream | Multiple drifters interact |
| Patch CV for movement | Internal drift generators |
| Clinical parameter names | Evocative world-building language |

### Core Principles

1. **Autonomous Beauty** - Should sound beautiful with minimal patching
2. **Chaos as Architecture** - Randomness built into the core, not bolted on
3. **Slow Evolution** - Optimized for ambient, not glitch
4. **Deep but Accessible** - Few controls, each with wide expressive range
5. **Sample-First** - Designed around exploring pre-loaded samples, not live input

---

## Architecture

### The Drifters (×4)

Four independent grain generators, each with:
- Its own **position** in the sample (0-100%)
- Its own **drift velocity** (how fast it moves through sample)
- Its own **pitch offset** (relative to master pitch)
- Its own **stereo position** (fixed spread: hard L, mid-L, mid-R, hard R)

Drifters are **not individually controllable**. They respond to global parameters that shape their collective behavior.

### Signal Flow

```
Sample Buffer (up to 32 sec, stereo)
    ↓
┌─────────────────────────────────────────┐
│  Drifter 1 ──→ Grains ──→ ┐            │
│  Drifter 2 ──→ Grains ──→ ├──→ Mix ──→ │──→ Fog (reverb) ──→ Output
│  Drifter 3 ──→ Grains ──→ │            │
│  Drifter 4 ──→ Grains ──→ ┘            │
└─────────────────────────────────────────┘
```

---

## Parameters

### Position Controls

#### ANCHOR (Knob)
Where in the sample the drifters tend to cluster.
- 0% = sample start
- 100% = sample end
- Drifters orbit around this point based on other parameters

#### WANDER (Knob)
How far drifters can stray from the anchor.
- CCW: Tight cluster (all drifters near anchor)
- CW: Wide exploration (drifters spread across entire sample)
- Interacts with GRAVITY to determine actual behavior

#### GRAVITY (Knob, bipolar)
How strongly drifters are pulled toward/pushed from anchor.
- CCW: Repulsion (drifters flee from anchor, spread out)
- Center: Neutral (free drift)
- CW: Attraction (drifters pulled toward anchor)

**Interaction: WANDER × GRAVITY**
- High WANDER + High GRAVITY = Wide swings, always returning
- High WANDER + Low GRAVITY = Drifters wander freely, rarely return
- Low WANDER + High GRAVITY = Tight cluster, small perturbations
- Low WANDER + Repulsion = Ring around anchor, never touching

---

### Time Controls

#### DRIFT (Knob)
Base speed at which drifters move through the sample.
- CCW: Nearly frozen (glacial evolution)
- CW: Continuous wandering
- Individual drifters vary ±50% from this base

#### DENSITY (Knob)
Grain triggering rate × grain size relationship.
- CCW (Sparse): Few long grains, clear texture
- Center: Overlapping grains, smooth clouds
- CW (Dense): Many short grains, thick fog

*Internal: Automatically manages rate/size relationship for consistent output level*

---

### Pitch Controls

#### PITCH (Knob)
Master pitch offset, affects all drifters equally.
- CCW: -2 octaves
- Center: Original pitch
- CW: +2 octaves
- LED: Green at center, Red when offset

#### SCATTER (Knob)
Pitch spread across the four drifters.
- CCW: Unison (all same pitch)
- Center: ±semitone spread (subtle chorus)
- CW: ±octave spread (wide harmonic field)

*Internal distribution: Drifters 1&4 get positive offset, 2&3 get negative, creating stereo pitch width*

---

### Spectral Controls (Filter Bank)

Each drifter lives in its own **frequency band**, creating layered worlds rather than copies:

```
                    SPECTRUM
    0% ──────────────────────────────── 100%
    │                                    │
    All fullrange              Separated bands

    ┌─────────────────────────────────────┐
    │  D1: ░░░░                           │  Sub/Bass (< 200Hz)
    │  D2:     ░░░░░░                     │  Low-Mid (200-800Hz)
    │  D3:            ░░░░░░░░            │  Mid-High (800-4kHz)
    │  D4:                     ░░░░░░░░░░ │  Air (> 4kHz)
    └─────────────────────────────────────┘
```

#### SPECTRUM (Knob)
Controls filter bank separation.
- **CCW (0%)**: All drifters are fullrange (bypass filters)
- **Center (50%)**: Gentle band separation, overlapping
- **CW (100%)**: Strict band isolation, no overlap

| Drifter | Role | Center Freq | Bandwidth (at 100%) |
|---------|------|-------------|---------------------|
| 1 | Sub/Bass | 100 Hz | 20-200 Hz |
| 2 | Low-Mid | 400 Hz | 150-1000 Hz |
| 3 | Mid-High | 2 kHz | 800-5000 Hz |
| 4 | Air/Presence | 8 kHz | 4000-20000 Hz |

**Filter Type:** 2-pole state-variable (bandpass mode), resonance fixed at moderate Q

#### TILT (Knob, bipolar) — *replaces TONE*
Spectral balance across the filter bank.
- **CCW**: Boost low drifters, attenuate high (dark, subby)
- **Center**: Flat balance across all bands
- **CW**: Boost high drifters, attenuate low (bright, airy)

```
TILT CCW          TILT Center       TILT CW
████
████ ███          ███ ███ ███ ███      ███
████ ███ ██       ███ ███ ███ ███  ██  ███ ████
████ ███ ██  ·    ███ ███ ███ ███  ██  ███ ████
 D1  D2  D3  D4    D1  D2  D3  D4   D1  D2  D3  D4
```

**SPECTRUM + SCATTER Interaction:**
- SPECTRUM separates frequency bands (filtering)
- SCATTER shifts pitch per drifter (transposition)
- Combined: D1 plays bass content pitched down, D4 plays highs pitched up = massive spread

**Musical Implications:**
- Low SPECTRUM + Low SCATTER: Thick unison, all drifters similar
- High SPECTRUM + Low SCATTER: Clear band separation, but each band at natural pitch
- Low SPECTRUM + High SCATTER: Fullrange but pitch-spread (chorusing/harmonics)
- High SPECTRUM + High SCATTER: Full spectral + pitch separation (huge evolving world)

---

### Texture Controls

#### SHAPE (Knob, 5 positions)
Grain envelope character. Detented or smooth depending on hardware.

| Position | Name | Character |
|----------|------|-----------|
| 1 | Mist | Soft gaussian, maximum smoothness |
| 2 | Cloud | Tukey window, balanced |
| 3 | Rain | Triangle, some attack definition |
| 4 | Hail | Sharp attack, soft decay |
| 5 | Ice | Square-ish, hard edges for glitch moments |

#### TONE (Knob, bipolar)
Global filter applied per-grain.
- CCW: Low-pass (darker, distant)
- Center: Bypass
- CW: High-pass (brighter, closer)

#### FOG (Knob)
Send to internal reverb + reverb decay time (combined control).
- CCW: Dry, no reverb
- CW: Heavy reverb with long decay, sample becomes vapor

---

### Chaos Controls

#### ENTROPY (Knob)
Master chaos control affecting multiple systems:
- **Position chaos**: How erratically drifters move
- **Timing chaos**: Grain trigger jitter
- **Pitch chaos**: Per-grain pitch variation

| Value | Behavior |
|-------|----------|
| 0% | Perfectly regular, almost sequenced feel |
| 25% | Subtle variation, organic |
| 50% | Clear randomness, still musical |
| 75% | Unpredictable, exploratory |
| 100% | Maximum chaos, constantly surprising |

#### STORM (Button + LED)
Momentary chaos injection.
- **Press**: Temporarily maximizes entropy, scatters drifters
- **Release**: Gradually returns to normal (5-10 second decay)
- **LED**: Shows current chaos level (DIM to BRIGHT)

*Use to break out of repetitive patterns or trigger dramatic transitions*

---

## CV Inputs (accent marks on panel indicate modulation)

| Input | Function | Range |
|-------|----------|-------|
| ANCHOR CV | Offset anchor position | ±5V = full range |
| PITCH CV | V/Oct pitch control | 1V/Oct |
| DRIFT CV | Modulate drift speed | ±5V = ±100% |
| ENTROPY CV | Modulate chaos amount | 0-5V = 0-100% |
| STORM GATE | Gate input for STORM | >2V triggers |
| CLOCK | External clock for grain sync | >2V triggers |

---

## Clock Sync & Deviation

When a cable is patched to **CLOCK**, a new behavior emerges:

#### DEVIATION (Knob, replaces/shares with existing control when clocked)

Controls how closely grain triggers follow the external clock vs free Poisson.

| Value | Behavior |
|-------|----------|
| 0% | **Locked**: All drifters trigger exactly on clock (4 grains per pulse) |
| 25% | **Tight**: Triggers cluster around clock, ±10% jitter |
| 50% | **Loose**: Clock acts as gentle attractor, grains drift early/late |
| 75% | **Suggestion**: Clock influences probability, not timing |
| 100% | **Free**: Pure Poisson, clock ignored (but still sets base λ) |

**Implementation: Phase Attraction Model**

```cpp
// On each sample, calculate "pull" toward next clock
float clock_phase = time_since_clock / clock_period;  // 0.0 to 1.0
float poisson_impulse = poisson_process();            // when to fire naturally

// Deviation controls blend
if (deviation < 0.5) {
    // Low deviation: quantize toward clock boundaries
    float quantize_strength = 1.0 - (deviation * 2);
    trigger_phase = lerp(poisson_impulse, round(poisson_impulse), quantize_strength);
} else {
    // High deviation: clock just influences rate
    float rate_influence = (1.0 - deviation) * 2;
    lambda = lerp(base_lambda, clock_rate, rate_influence);
}
```

**Musical Uses:**
- 0% DEVIATION: Rhythmic granular, grains land on beats
- 25%: Humanized rhythm, grains cluster around beats
- 50%: Polyrhythmic feel, grains push and pull against clock
- 75%: Clock as tempo reference, mostly free
- 100%: Ambient, clock only sets overall density

**Per-Drifter Variation:**
Even at 0% deviation, drifters don't fire simultaneously:
- Drifter 1: On clock
- Drifter 2: +25% phase offset
- Drifter 3: +50% phase offset
- Drifter 4: +75% phase offset

This creates rhythmic interest even when fully locked.

**DENSITY Interaction:**
When clocked, DENSITY becomes a **clock multiplier/divider**:
| DENSITY | Triggers per clock |
|---------|-------------------|
| 0% | ÷4 (every 4th clock) |
| 25% | ÷2 |
| 50% | ×1 (on each clock) |
| 75% | ×2 |
| 100% | ×4 |

---

## CV Outputs

| Output | Function |
|--------|----------|
| DRIFT OUT | Averaged position of all drifters (0-5V) |
| PULSE OUT | Trigger on each grain (any drifter) |

*DRIFT OUT is useful for patching position-dependent modulation to external modules*

---

## Sample Management

### Loading
- Reads WAV files from SD card `/samples/drift/` folder
- Supports: Stereo, 48kHz, 16-bit, up to 32 seconds
- Sample selected via parameter (SAMPLE knob or CV)

### Sample Slots
8 sample slots, navigated with encoder or CV:
- Slots 1-8 map to first 8 WAV files alphabetically
- Empty slots produce silence
- Hot-swap: changing sample crossfades (500ms)

---

## Display

### Main View
```
┌────────────────────────────────┐
│ DRIFT ENGINE      [Sample: 3] │
│ ════════════════════════════  │
│ ░░░░▓░░░░░░░░░░░░░░░░▓░░░░░░ │  ← Sample with drifter positions
│     1           A   2         │  ← Numbers = drifters, A = anchor
│                               │
│ ANCHOR: 42%    ENTROPY: 35%   │
│ DRIFT:  Med    FOG: Heavy     │
└────────────────────────────────┘
```

### Drifter Visualization
- Horizontal bar shows sample length
- Four markers show current drifter positions
- Anchor shown as different symbol
- Markers move in real-time, showing the "wandering"

---

## Preset Suggestions (Factory)

| # | Name | Character |
|---|------|-----------|
| 1 | First Light | Gentle, minimal movement, sparse |
| 2 | Weather System | Moderate drift, medium density |
| 3 | Deep Time | Very slow, low pitch, heavy fog |
| 4 | Particle Field | High scatter, dense, bright |
| 5 | Event Horizon | Strong gravity, clustered, dark |
| 6 | Brownian | Maximum entropy, unpredictable |
| 7 | Frozen Lake | Nearly static, ice shape, sparse |
| 8 | Chaos Garden | Full chaos, all parameters active |

---

## Implementation Notes

### Drifter Position Algorithm
```
For each drifter:
  1. Calculate gravity force toward/away from anchor
  2. Add base drift velocity (with per-drifter variation)
  3. Add entropy-scaled random walk
  4. Constrain to [anchor - wander, anchor + wander]
  5. Wrap or bounce at sample boundaries
```

### Grain Triggering (Poisson Process)

Each drifter uses a **Poisson process** for grain triggering:

```cpp
// Time until next grain (exponential distribution)
float lambda = density_to_rate(DENSITY);  // grains per second
float wait_time = -log(random_uniform()) / lambda;
```

**Why Poisson?**
- Natural clustering: Sometimes grains bunch up, sometimes gaps
- Memoryless: Each grain independent of previous
- Scales beautifully with rate parameter
- Sounds organic, never mechanical

**ENTROPY interaction:**
- Low entropy: Multiple Poisson streams (per drifter) give gentle variation
- High entropy: Add time-varying λ (rate wobbles over time)
- Maximum entropy: Burst mode—occasional λ spikes create grain "storms"

**DENSITY mapping:**
| DENSITY | λ (grains/sec) | Avg grain size |
|---------|----------------|----------------|
| 0% | 0.5 | 2000ms |
| 25% | 2 | 500ms |
| 50% | 8 | 125ms |
| 75% | 20 | 50ms |
| 100% | 50 | 20ms |

*Size is approximate—actual size varies with entropy*

### Resource Budget (distingNT)
- 4 drifters × up to 8 overlapping grains = 32 simultaneous grains max
- Per-grain: position interpolation, envelope, filter
- Reverb: lightweight algorithmic (similar to existing NT reverbs)
- Target: <50% CPU at 48kHz

---

## Open Questions

1. **Drifter count**: 4 is proposed. Would 2 be sufficient? Would 6 be better?

2. **Sample navigation**: Encoder vs CV-only for sample selection?

3. **Sync**: Should DENSITY have a sync-to-clock option? (Conflicts with ambient philosophy but adds utility)

4. **Quantization**: Should PITCH have optional scale quantization? Or leave to external quantizer?

5. **Stereo samples**: Use L/R independently per drifter pair, or mono-sum?

---

## Success Criteria

1. **10-second test**: Load any sample, make sound immediately. Sound interesting with all knobs at noon.

2. **Hands-off test**: Leave running for 5 minutes. Should remain engaging without touching anything.

3. **Character range**: Same sample should sound radically different between "First Light" and "Chaos Garden" presets.

4. **CPU headroom**: Comfortable operation with room for other slots.

5. **Learnable**: New user understands basic operation within 2 minutes.

---

## Version History

| Version | Date | Notes |
|---------|------|-------|
| 0.1 | 2024-12-31 | Initial PRD draft |
