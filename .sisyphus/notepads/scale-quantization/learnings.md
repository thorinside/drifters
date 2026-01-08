# Learnings - Scale Quantization

## Task 2: Add Scale Parameter Definition

### What Was Done
- Added `kParamScale` enum after `kParamScatter` in parameter enum (line ~299)
- Changed Scatter parameter unit from `kNT_unitSemitones` to `kNT_unitNone` (line ~345)
- Added Scale parameter definition: min=0, max=21, def=0, unit=kNT_unitEnum, enumStrings=scaleNames (line ~346)
- Updated `pagePitch[]` array to include `kParamScale` as third element (line ~364)
- Build verified clean with `make hardware`

### Key Patterns
- Parameter enum order must match parameters[] array order
- Enum parameters use `kNT_unitEnum` and reference a NULL-terminated string array
- Page arrays automatically size via `ARRAY_SIZE()` macro
- `kNumParameters` auto-adjusts when enum entries added before it

### Dependencies
- Task 1 (Scale Data Structure) had already added `scaleNames[]` array with 22 scale names (lines 75-99)
- `scaleNames` follows same pattern as `shapeNames` (NULL-terminated const char* array)

### Build Notes
- LSP shows errors for missing distingNT API headers (expected in development environment)
- Actual hardware build with arm-none-eabi-g++ compiles cleanly
- No warnings or errors from compiler

## Task 3: Add Scale Quantization Helpers

### What Was Done
- Inserted `degreeToSemitones(int degree, int scaleIndex)` and `quantizePitchToScale(float semitones, int scaleIndex)` in global scope immediately before `step()` in `drifters.cpp`.
- Implemented chromatic bypass for `scaleIndex == 0`.
- `degreeToSemitones` handles negative degrees (wrap to previous octave) and degree overflow (wrap to next octave) via octave/div/mod logic.
- `quantizePitchToScale` quantizes within the octave by scanning `scale.notes[]` and choosing the minimum absolute distance.

### Acceptance Logic Checks
- `degreeToSemitones(0, any)` returns `0` because `octave=0`, `degreeInOctave=0`, and `scale.notes[0]==0`.
- `degreeToSemitones(7, 1)` (Ionian, 7 notes) returns `12` because `octave=1`, `degreeInOctave=0`.
- `degreeToSemitones(-1, 1)` returns `-1` because modulo correction yields `degreeInOctave=6` and `octave=-1` → `-12 + 11`.
- `degreeToSemitones(3, 16)` (Hirajoshi, 5 notes) returns `7` because `scaleHirajoshi[3]==7`.
- `degreeToSemitones(6, 16)` returns `14` because `octave=1`, `degreeInOctave=1` → `12 + 2`.
- `quantizePitchToScale(2.5, 1)` returns `2` because it’s closer to `2` than `4`.
- `quantizePitchToScale(3.0, 1)` returns `4` because ties don’t occur here and `|3-4| < |3-2|`.
- `quantizePitchToScale(x, 0)` returns `x` due to bypass.

### Build Notes
- `make hardware` builds successfully after adding the helpers.
