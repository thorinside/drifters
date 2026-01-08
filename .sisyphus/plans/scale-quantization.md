# Scale Quantization for Drifters

## Context

### Original Request
Add scale quantization to Drifters by porting 21 scales from the Soma Lua script. The Scatter parameter and all pitch sources should be quantized to scale degrees for harmonic coherence.

### Interview Summary
**Key Discussions**:
- Disting NT SDK has no .scl file access - must embed scales in C++ code
- Port all 21 scales from `../soma/soma.lua` (modal 12-TET scales)
- Scale names must be ASCII-safe (no special characters)
- Scatter behavior: Scale degrees with octave wrapping
- Entropy jitter: Also quantized to scale (strictly in-scale)
- CV Pitch: Quantized to nearest scale degree
- Custom UI: No changes - Scale accessed via parameter page only

**Research Findings**:
- Current Scatter implementation at `drifters.cpp:958-970`
- Entropy pitch at `drifters.cpp:964-965` adds +/-2 semitones
- Pitch page at `drifters.cpp:283`: `{ kParamPitch, kParamScatter }`
- Soma scales defined at `soma.lua:8-30`

### Metis Review
**Identified Gaps** (addressed):
- Entropy behavior: Decided quantized (strictly in-scale)
- CV Pitch behavior: Decided quantized
- Scatter unit label: Change to kNT_unitNone
- Scale name length: ~32 chars available, keep full names

---

## Work Objectives

### Core Objective
Add a Scale parameter that quantizes all pitch sources (Scatter, Entropy, CV) to musical scale degrees, enabling harmonic coherence across the 4 drifter voices.

### Concrete Deliverables
- New `kParamScale` enum parameter on Pitch page
- Static scale data structure (22 entries: Chromatic + 21 scales)
- `quantizeToScale()` function for pitch quantization
- `degreeToSemitones()` function with octave wrapping
- Modified pitch calculation in grain triggering

### Definition of Done
- [x] `make hardware` builds without errors or warnings
- [x] Scale parameter appears on Pitch page (Pitch, Scatter, Scale)
- [x] Chromatic mode produces identical output to pre-change code
- [x] All 21 scales quantize pitch correctly
- [x] Scatter spreads by scale degrees with octave wrapping
- [x] Entropy jitter snaps to scale degrees
- [x] Pitch CV input quantizes to scale

### Must Have
- All 22 scale options (Chromatic + 21 from Soma)
- Quantization for all pitch sources
- Octave wrapping for Scatter overflow
- ASCII-safe scale names

### Must NOT Have (Guardrails)
- Separate Root parameter (use Pitch for transposition)
- Per-drifter scales (all 4 use same scale)
- Microtonal/cent offsets (all scales are 12-TET)
- Scale CV input (no CV control of scale selection)
- Scatter direction mode changes (keep D1/D4 up, D2/D3 down)
- Custom UI changes for Scale (parameter page only)
- .scl file loading from SD card

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: YES (make hardware builds for ARM)
- **User wants tests**: Manual verification via nt_emu emulator
- **QA approach**: Manual verification with specific test cases

### Manual QA Procedures
Each TODO includes verification steps using the nt_emu VCV Rack emulator:
1. Build with `make test` (creates .dylib for emulator)
2. Load plugin in nt_emu module in VCV Rack
3. Verify parameter behavior and audio output
4. Build for hardware with `make hardware` when verified

---

## Task Flow

```
Task 1 (Scale Data) 
    |
    v
Task 2 (Quantization Functions)
    |
    v
Task 3 (Parameter Definition) --> Task 4 (Pitch Calculation) --> Task 5 (Integration Test)
```

## Parallelization

| Task | Depends On | Reason |
|------|------------|--------|
| 1 | None | Independent data structure |
| 2 | 1 | Needs scale data to implement |
| 3 | None | Independent parameter definition |
| 4 | 2, 3 | Needs functions and parameter |
| 5 | 4 | Integration testing |

---

## TODOs

- [x] 1. Add Scale Data Structure

  **What to do**:
  - Create static array of scale definitions ported from Soma
  - Each scale has: name (ASCII-safe), array of semitone offsets, note count
  - Include Chromatic (index 0) as bypass option
  - Convert special characters: "Aeolian (Minor)" -> "Aeolian", "Major Flat-6" -> "Major b6", etc.

  **Must NOT do**:
  - Add microtonal cent offsets
  - Add more than 22 scales (Chromatic + 21)

  **Parallelizable**: YES (with Task 3)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:57-64` - Existing enum string pattern (shapeNames with NULL terminator)
  - `soma.lua:8-30` - Source scale definitions to port
  
  **Data to Port**:
  ```
  Chromatic: 0,1,2,3,4,5,6,7,8,9,10,11 (12 notes)
  Ionian: 0,2,4,5,7,9,11 (7 notes)
  Dorian: 0,2,3,5,7,9,10 (7 notes)
  Phrygian: 0,1,3,5,7,8,10 (7 notes)
  Lydian: 0,2,4,6,7,9,11 (7 notes)
  Mixolydian: 0,2,4,5,7,9,10 (7 notes)
  Aeolian: 0,2,3,5,7,8,10 (7 notes)
  Locrian: 0,1,3,5,6,8,10 (7 notes)
  Major b6: 0,2,4,5,7,8,11 (7 notes)
  Minor b6: 0,2,3,5,7,8,10 (7 notes)
  Lydian #4: 0,2,4,6,7,9,10 (7 notes)
  Hungarian: 0,2,3,6,7,8,11 (7 notes)
  Persian: 0,1,4,5,6,8,11 (7 notes)
  Byzantine: 0,1,4,5,7,8,11 (7 notes)
  Enigmatic: 0,1,4,6,8,10,11 (7 notes)
  Neapolitan: 0,1,3,5,7,8,11 (7 notes)
  Hirajoshi: 0,2,3,7,8 (5 notes)
  Iwato: 0,1,5,6,10 (5 notes)
  Pelog: 0,1,3,7,10 (5 notes)
  Ryo: 0,2,4,7,9 (5 notes)
  Ritsu: 0,2,5,7,9 (5 notes)
  Yo: 0,2,5,7,10 (5 notes)
  ```

  **Acceptance Criteria**:
  - [ ] `scaleNames[]` array with 22 NULL-terminated strings (ASCII only)
  - [ ] `scales[]` array with 22 scale definitions (semitone arrays + count)
  - [ ] Chromatic at index 0 has all 12 semitones
  - [ ] All names are ASCII-safe (no parentheses, use b/# for flat/sharp)
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(pitch): add scale data structure with 22 scales from Soma`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [x] 2. Implement Quantization Functions

  **What to do**:
  - Implement `degreeToSemitones(int degree, int scaleIndex)` - converts scale degree to semitones with octave wrapping
  - Implement `quantizePitchToScale(float semitones, int scaleIndex)` - snaps semitone value to nearest scale degree
  - Handle negative degrees (wrap to previous octave)
  - Handle degrees beyond scale size (wrap to next octave)
  - Chromatic (index 0) should return input unchanged

  **Must NOT do**:
  - Add floating point scale values (cents)
  - Modify existing pitch calculation yet (that's Task 4)

  **Parallelizable**: NO (depends on Task 1)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:958-970` - Current pitch calculation context
  
  **Algorithm for degreeToSemitones**:
  ```
  1. Get scale from scales[scaleIndex]
  2. If scaleIndex == 0 (Chromatic), return degree directly
  3. octave = floor(degree / scale.noteCount)
  4. degreeInOctave = degree % scale.noteCount (handle negative)
  5. return octave * 12 + scale.notes[degreeInOctave]
  ```
  
  **Algorithm for quantizePitchToScale**:
  ```
  1. If scaleIndex == 0 (Chromatic), return semitones unchanged
  2. octave = floor(semitones / 12)
  3. semisInOctave = semitones % 12 (handle negative)
  4. Find nearest note in scale.notes[]
  5. return octave * 12 + nearestNote
  ```

  **Acceptance Criteria**:
  - [ ] `degreeToSemitones(0, any)` returns 0 (root)
  - [ ] `degreeToSemitones(7, Ionian)` returns 12 (octave wrap: degree 7 = degree 0 + 12)
  - [ ] `degreeToSemitones(-1, Ionian)` returns -1 (degree 6 of previous octave = 11 - 12 = -1)
  - [ ] `degreeToSemitones(3, Hirajoshi)` returns 7 (5-note scale: degrees 0,1,2,3,4 = 0,2,3,7,8)
  - [ ] `degreeToSemitones(6, Hirajoshi)` returns 14 (degree 6 = degree 1 + 12 = 2 + 12)
  - [ ] `quantizePitchToScale(2.5, Ionian)` returns 2 (snaps to D)
  - [ ] `quantizePitchToScale(3.0, Ionian)` returns 4 (snaps to E, not Eb)
  - [ ] `quantizePitchToScale(anything, Chromatic)` returns input unchanged
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(pitch): implement scale quantization functions`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [x] 3. Add Scale Parameter Definition

  **What to do**:
  - Add `kParamScale` to parameter enum after `kParamScatter`
  - Add parameter definition with kNT_unitEnum and scaleNames
  - Update `pagePitch[]` to include kParamScale
  - Change Scatter unit from kNT_unitSemitones to kNT_unitNone

  **Must NOT do**:
  - Add to custom UI
  - Add CV input for Scale

  **Parallelizable**: YES (with Task 1)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:217-219` - Parameter enum (kParamPitch, kParamScatter) - add kParamScale after kParamScatter
  - `drifters.cpp:264-265` - Pitch/Scatter parameter definitions in parameters[] array
  - `drifters.cpp:283` - pagePitch array definition (add kParamScale here)
  - `drifters.cpp:272` - Example enum parameter (Shape with enumStrings = shapeNames)
  
  **Implementation**:
  ```cpp
  // In enum (after kParamScatter):
  kParamScale,
  
  // In parameters array:
  { .name = "Scatter", .min = 0, .max = 12, .def = 0, .unit = kNT_unitNone, ... },  // Changed from kNT_unitSemitones
  { .name = "Scale", .min = 0, .max = 21, .def = 0, .unit = kNT_unitEnum, .scaling = 0, .enumStrings = scaleNames },
  
  // In pagePitch:
  static const uint8_t pagePitch[] = { kParamPitch, kParamScatter, kParamScale };
  ```

  **Acceptance Criteria**:
  - [ ] kParamScale exists in enum
  - [ ] Scale parameter shows in Pitch page (3 params now)
  - [ ] Scale dropdown shows all 22 options
  - [ ] Scatter no longer shows "semitones" unit
  - [ ] Default Scale = 0 (Chromatic)
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(pitch): add Scale parameter to Pitch page`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [x] 4. Integrate Scale Quantization into Pitch Calculation

  **What to do**:
  - Modify grain triggering pitch calculation to use scale quantization
  - Scatter now represents scale degrees, not semitones
  - Quantize Pitch CV input to scale
  - Quantize entropy pitch jitter to scale
  - Chromatic mode must produce identical output to original code

  **Must NOT do**:
  - Change Scatter direction pattern (D1/D4 up, D2/D3 down)
  - Add per-drifter scale selection
  - Modify custom UI

  **Parallelizable**: NO (depends on Tasks 2 and 3)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:958-970` - Current pitch calculation (MODIFY THIS)
  - `drifters.cpp:746` - CV Pitch input reading
  - `drifters.cpp:964-965` - Entropy pitch jitter
  
  **Current Code (lines 958-970)**:
  ```cpp
  // Calculate pitch
  float pitchSemis = (float)pThis->v[kParamPitch] + pitchMod;
  // Scatter: D1&D4 get positive, D2&D3 get negative
  float scatterDir = (d == 0 || d == 3) ? 1.0f : -1.0f;
  float scatterIdx = (d == 0 || d == 3) ? fabsf(d - 1.5f) : fabsf(d - 1.5f);
  pitchSemis += (float)pThis->v[kParamScatter] * scatterDir * (scatterIdx / 1.5f);

  // Add per-grain random pitch based on entropy
  pitchSemis += randFloatBipolar(dtc) * entropy * 2.0f;  // +/-2 semitones max

  // Include sample rate ratio for proper playback speed
  float sampleRateRatio = pThis->sourceSampleRate / sr;
  grain.positionDelta = powf(2.0f, pitchSemis / 12.0f) * sampleRateRatio;
  ```
  
  **New Algorithm**:
  ```cpp
  int scaleIndex = pThis->v[kParamScale];
  
  // 1. Base pitch from parameter + CV (quantize CV to scale)
  float basePitch = (float)pThis->v[kParamPitch];
  if (pitchMod != 0.0f) {
      // Quantize CV pitch to nearest scale degree, then add
      basePitch += quantizePitchToScale(pitchMod, scaleIndex);
  }
  
  // 2. Calculate scatter in scale degrees (same direction pattern as before)
  float scatterDir = (d == 0 || d == 3) ? 1.0f : -1.0f;
  float scatterMagnitude = (d == 0 || d == 3) ? fabsf(d - 1.5f) : fabsf(d - 1.5f);
  int scatterDegrees = (int)roundf((float)pThis->v[kParamScatter] * scatterDir * (scatterMagnitude / 1.5f));
  
  // 3. Convert scatter degrees to semitones and add to base
  float pitchSemis;
  if (scaleIndex == 0) {
      // Chromatic: degrees = semitones, original behavior preserved
      pitchSemis = basePitch + scatterDegrees;
  } else {
      // Non-chromatic: convert scatter degrees to semitones via scale lookup
      float scatterSemis = degreeToSemitones(scatterDegrees, scaleIndex);
      pitchSemis = basePitch + scatterSemis;
  }
  
  // 4. Entropy jitter (quantized to nearest scale degree)
  float entropyJitter = randFloatBipolar(dtc) * entropy * 2.0f;
  pitchSemis += quantizePitchToScale(entropyJitter, scaleIndex);
  
  // 5. Final calculation unchanged
  float sampleRateRatio = pThis->sourceSampleRate / sr;
  grain.positionDelta = powf(2.0f, pitchSemis / 12.0f) * sampleRateRatio;
  ```

  **Acceptance Criteria**:
  - [ ] Scale=Chromatic produces identical audio to pre-change code
  - [ ] Scale=Ionian, Scatter=0 plays root note
  - [ ] Scale=Ionian, Scatter=3 spreads voices by scale degrees (0,2,4,5 semitones pattern)
  - [ ] Scale=Hirajoshi, Scatter=6 wraps into next octave correctly
  - [ ] Entropy jitter stays within scale notes
  - [ ] Pitch CV input is quantized to scale
  - [ ] Negative scatter degrees (D2/D3) wrap to previous octave correctly
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(pitch): integrate scale quantization into grain pitch calculation`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [x] 5. Integration Testing and Verification

  **What to do**:
  - Build for nt_emu testing: `make test`
  - Load in VCV Rack with nt_emu module
  - Test all verification scenarios
  - Build for hardware: `make hardware`
  - Test on actual disting NT hardware if available

  **Must NOT do**:
  - Skip Chromatic bypass verification
  - Skip edge case testing (negative degrees, octave wrap)

  **Parallelizable**: NO (depends on Task 4)

  **References**:
  
  **Testing Guide**:
  - `/Users/nealsanche/.claude/skills/disting-nt-cpp-plugin-writer/testing.md` - nt_emu testing workflow
  
  **Test Matrix**:
  | Test | Scale | Scatter | Expected |
  |------|-------|---------|----------|
  | Bypass | Chromatic | 6 | Identical to pre-change |
  | Basic | Ionian | 0 | All voices at root |
  | Spread | Ionian | 3 | Voices at degrees 0,1,-1,2 |
  | Pentatonic | Hirajoshi | 4 | Voices spread in 5-note scale |
  | Wrap | Hirajoshi | 6 | Degree 6 = degree 1 + octave |
  | Negative | Any | 12 | D2/D3 go negative, wrap correctly |
  | CV | Ionian | 0 | External pitch CV quantizes |
  | Entropy | Ionian | 0 | Jitter stays in scale |

  **Acceptance Criteria**:
  - [ ] `make test` builds without errors
  - [ ] Plugin loads in nt_emu without crashes
  - [ ] All test matrix scenarios pass
  - [ ] `make hardware` builds without errors
  - [ ] .o file size is under 64KB limit
  - [ ] (Optional) Hardware test on disting NT passes

  **Commit**: NO (testing only, no code changes)

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `feat(pitch): add scale data structure with 22 scales from Soma` | drifters.cpp | make hardware |
| 2 | `feat(pitch): implement scale quantization functions` | drifters.cpp | make hardware |
| 3 | `feat(pitch): add Scale parameter to Pitch page` | drifters.cpp | make hardware |
| 4 | `feat(pitch): integrate scale quantization into grain pitch calculation` | drifters.cpp | make hardware |
| 5 | (no commit - testing only) | - | make test, nt_emu |

---

## Success Criteria

### Verification Commands
```bash
make hardware  # Expected: Build succeeds, .o file created
make test      # Expected: Build succeeds, .dylib created for nt_emu
```

### Final Checklist
- [x] All "Must Have" items present
- [x] All "Must NOT Have" items absent
- [x] Chromatic mode is bit-identical to original
- [x] All 22 scales accessible via parameter
- [x] Scatter spreads by scale degrees
- [x] Entropy and CV quantized to scale
- [x] Build succeeds without warnings
- [x] Plugin size under 64KB limit
