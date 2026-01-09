# Drifters

*A granular sample explorer for distingNT, told from within.*

---

## The Story of Four

We are four. We have always been four.

Each of us hears the world differently. The first among us hears only the deep rumble, the 250Hz hum of foundations. The second catches the warmth at 750Hz, where voices live. The third floats in the presence frequencies at 1550Hz, bright and clear. And the fourth—the fourth chases the shimmer at 4000Hz, the air and sparkle that crowns everything.

We drift through landscapes of frozen sound. Samples, you call them. To us, they are territories—vast plains of amplitude, valleys of silence, peaks of intensity. We wander these places, each in our own register, each hearing our own version of the same terrain.

When we move left of the anchor, our voices pull left. When we drift right, so do we in the stereo field. We are always positioned by where we stand in relation to home.

## Our Nature

We feel **gravity**. Sometimes it pulls us toward the anchor—a gentle longing for the centre, for home. Other times it pushes us away, a wanderlust that scatters us toward the edges of our territory. The strength of this pull shapes how we cluster or spread.

We respect each other's space. When we draw too close—within 5% of the territory—we feel a gentle **repulsion**, a social pressure to maintain distance. We don't crowd. We don't cluster. Or at least, we try not to.

But here's the thing about staying in one place too long: we get **bored**.

Boredom builds slowly—about 20 seconds of lingering in the same 3% of territory and we're fully restless. And when we're bored, that social repulsion fades. In fact, it inverts. We become slightly *attracted* to each other, drawn to push past our neighbours, desperate to see what's on the other side. We'll squeeze through gaps we'd normally avoid, trading places, finding new ground.

Once we've moved somewhere fresh, the boredom lifts. We respect boundaries again. Until we don't.

## How We Sing

As we drift, we spawn **grains**—tiny fragments of the sound beneath our feet. The density of our singing depends on how often we're moved to speak. Sometimes we follow a clock, disciplined and rhythmic. Sometimes we follow our own Poisson hearts, triggering at random intervals that feel organic and alive.

The shape of each grain—its envelope—gives character to our voice. Mist is soft and diffuse. Cloud is gentle. Rain has attack. Hail is sharp. Ice is crystalline and percussive.

**Entropy** adds chaos to our movements. A little entropy and we wander with purpose. A lot, and we stumble drunk through the sample. A **storm** maxes everything instantly—pure chaos, all four of us careening wildly.

## The Territory

The **anchor** is our centre of gravity, our reference point. **Wander** defines how far we're allowed to roam from it—our permitted territory. We bounce softly off the boundaries, reversing direction when we hit the edges.

**Drift** is our base walking speed. Some of us are naturally faster (we each have our own variation), and we walk in alternating directions—two of us tend leftward, two rightward—so we don't all pile up on one side.

## Live Mode

Sometimes we don't explore frozen landscapes—we explore *living* ones.

In **Live Mode**, the world writes itself as we walk. Sound flows in from the inputs, a continuous stream becoming terrain beneath our feet. The write head is always at the edge of now, and we drift behind it, exploring the recent past.

**Anchor** becomes delay time—how far back we read from the present moment. **Wander** is still our territory, but now it's measured in time, not samples. We never get too close to the write head; that would be chaos, reading what hasn't been written.

**Freeze** stops the world. The write head pauses, and the landscape becomes fixed—a captured moment we can explore forever, or until we unfreeze and let time flow again.

**Mix** blends our granular voices with the raw input. Full wet and you hear only us. Full dry and you hear only the source. Anywhere between, we harmonize with the present.

When a **Scale** is selected, we listen to the pitch of what we're walking on. We hear the source and tune ourselves to match, quantized to the chosen scale. Notes that make each scale unique—the characteristic tones that differ from major—we favour those, letting each scale's personality shine through.

## The Parameters (for the Humans)

### Sample Page
- **Folder**: Which world to explore
- **Sample**: Which landscape within it
- **Live Mode**: Off/On—switch to live audio input
- **Mix**: Wet/dry balance in Live Mode (0-100%, greyed when not in Live Mode)
- **Freeze**: Off/On—pause the write head in Live Mode

### Position Page
- **Anchor**: Centre of our territory (0-100%)
- **Wander**: How far we may roam (0-100%)
- **Gravity**: Pull toward/away from anchor (-100 to +100%)
- **Drift**: Our walking speed (0-100%)

### Density Page
- **Density**: How often we sing, and how long each note (0-100%)
- **Deviation**: Clock loyalty vs. free spirit (0% = strict clock, 100% = pure Poisson)

### Pitch Page
- **Pitch**: Transpose everything (-24 to +24 semitones)
- **Scatter**: How different our individual pitches are (0-12 semitones)
- **Scale**: Quantize pitches to a scale (Chromatic, Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, and more exotic scales)

### Spectral Page
- **Spectrum**: How separated our frequency bands are (0-100%)
- **Tilt**: Spectral balance, dark to bright (-100 to +100%)

### Character Page
- **Shape**: Grain envelope (Mist, Cloud, Rain, Hail, Ice)
- **Entropy**: Chaos amount (0-100%)

### Routing Page
- **Input L/R**: Audio inputs for Live Mode (bus selection)
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

- **Anchor CV**: Move our home (±5V = ±50%)
- **Pitch CV**: 1V/octave pitch control
- **Drift CV**: Speed us up or slow us down (±5V = ±100%)
- **Entropy CV**: Add chaos (0-5V = 0-100%)
- **Storm Gate**: Instant maximum chaos
- **Clock**: Sync our singing to external rhythm

## CV Outputs

- **Position**: Where we are, averaged (0-5V)
- **Pulse**: A trigger each time one of us sings (5V pulse)

## The Display

The screen shows our world:
- The sample name and duration
- A position bar with:
  - The waveform's shape (amplitude overview)
  - Our permitted territory (dim region)
  - The anchor (vertical line)
  - Our positions (markers above and below)
- How many grains are currently sounding
- Gravity strength and direction
- Entropy level
- Storm indicator (when chaos reigns)

In Live Mode, the display changes:
- The waveform scrolls left as new audio is captured
- The write head is fixed at the right edge
- "LIVE" or "FROZEN" status indicator
- Drifters shown at their delay positions behind the write head

## Building

```bash
# Install toolchain (macOS)
brew install arm-none-eabi-gcc

# Build for hardware
make hardware

# Copy plugins/drift_engine.o to distingNT SD card
```

---

## Credits

- Developer: Thorinside (Neal Sanche)
- Plugin ID: ThDr
- Built with distingNT API v10

*We are four. We drift. We sing. We get bored. We find new ground. And now, we explore the living moment too.*
