# Scale Quantization - Design Decisions

## [2026-01-08] Core Decisions (from planning phase)

### Scatter Behavior: Scale Degrees (Option A)
**Decision**: Scatter parameter represents scale degrees, not semitone range.

**Rationale**: More musically intuitive - "Scatter=3" means "spread by 3 scale steps" regardless of scale. The variation between scales becomes part of each scale's character.

**Alternative Rejected**: Scatter as semitone range with quantization (Option B) - would create uneven jumps on sparse scales.

---

### Entropy: Quantized (Option B)
**Decision**: Entropy jitter is quantized to scale degrees (strictly in-scale).

**Rationale**: User requested all pitch sources stay in-scale for harmonic coherence.

**Alternative Rejected**: Continuous entropy after quantization - would allow off-scale notes.

---

### CV Pitch: Quantized (Option A)
**Decision**: External CV pitch input is quantized to nearest scale degree.

**Rationale**: Consistent with entropy behavior - everything stays in-scale.

**Alternative Rejected**: CV pass-through - external sequencer may not be quantized.

---

### Scatter Unit Label: kNT_unitNone (Option A)
**Decision**: Changed Scatter from kNT_unitSemitones to kNT_unitNone.

**Rationale**: With scale degrees, "semitones" label is misleading. No unit is clearer.

**Alternative Rejected**: Keep "semitones" - technically wrong but users might not notice.

---

### Custom UI: Parameter Page Only (Option A)
**Decision**: Scale accessed via standard parameter navigation, not custom UI.

**Rationale**: Scale is "set and forget" - doesn't need live tweaking. Keeps implementation simpler.

**Alternative Rejected**: Add encoder control for Scale - would require custom UI changes.

---

### Root Parameter: Use Pitch for Transposition
**Decision**: No separate Root parameter. Pitch parameter handles transposition.

**Rationale**: Pitch=-5 effectively means "root is G". Simpler than adding another parameter.

**Alternative Rejected**: Separate Root parameter (C-B) - adds complexity.

---

## [2026-01-08] Implementation Decisions

### Chromatic Bypass: Separate Code Path
**Decision**: Implemented chromatic (scaleIndex == 0) as completely separate code path in DSP.

**Rationale**: Cleaner than inline conditionals. Guarantees bit-identical behavior to original code.

**Code Structure**:
```cpp
if (scaleIndex == 0) {
    // Original pitch calculation (unchanged)
} else {
    // Scale quantization path
}
```

---

### Scatter Overflow: Octave Wrapping
**Decision**: Scatter degrees beyond scale size wrap into next octave.

**Example**: Hirajoshi (5 notes), Scatter=6 → degree 6 = degree 1 + octave = 2 + 12 = 14 semitones.

**Rationale**: Musically meaningful - allows wide spreads on any scale.

**Alternative Rejected**: Cap at scale size - would limit spread on pentatonics.

---

### Scale Name Length: Keep Full Names
**Decision**: Keep full scale names (up to ~32 chars available).

**Rationale**: Display has enough space. Only truncate if issues arise.

**Names Used**: "Chromatic", "Ionian", "Aeolian", "Major b6", "Lydian #4", "Hungarian", etc.
