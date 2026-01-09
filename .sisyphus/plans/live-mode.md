# Live Mode Feature for Drifters

**STATUS: COMPLETE** - Implemented 2026-01-08

## Implementation Summary
Four commits implementing Live Mode:
1. `feat(live): add Live Mode parameters to Sample page` - Added kParamLiveMode, kParamInputBus, kParamFreeze
2. `feat(live): implement circular buffer and audio capture` - Write pointer, freeze handling, dry monitoring
3. `feat(live): add stereo grain reading and relative positioning` - Stereo interpolation, 128-sample safety zone
4. `feat(live): implement mode switching with 50ms crossfades` - Gain ramps, UI status indicators

---

## Context

### Original Request
Add Live Mode to Drifters plugin - switch from sample playback to live audio input with circular buffer, freeze gate, and crossfade transitions.

### Interview Summary
**Key Discussions**:
- Musical model: Live looper that drifters explore
- Input monitoring: Yes, dry input alongside grains
- Parameter placement: Add to Sample page (Live replaces Folder/Sample when active)
- Audio format: True stereo capture
- Position model: Relative to write head (tape delay style)
- Freeze trigger: Level-triggered gate
- Safe zone: 128 samples behind write pointer
- Fade duration: 50ms linear crossfades
- Buffer startup: Immediately circular with existing data

**Research Findings**:
- Audio input parameter: Need `kNT_unitAudioInput` with bus selection (L/R/None)
- Existing grain reading at lines 1165-1176 reads mono from sampleBufferL
- Need to update grain reader for stereo interpolation
- Circular buffer: Write pointer advances, drifters read relative to it with 128-sample safety zone

### Metis Review
**Identified Gaps** (addressed):
- Input monitoring: Include dry signal alongside grains
- Safe zone enforcement: 128 samples minimum
- UI performance: Update at low rate to avoid CPU spikes
- Crossfade strategy: Linear ramps, state per block
- Position reference: Documented as relative to write head

---

## Work Objectives

### Core Objective
Add Live Mode that switches Drifters from sample playback to live audio input capture, enabling real-time granular processing of incoming audio with freeze capability and click-free transitions.

### Concrete Deliverables
- `kParamLiveMode` boolean parameter on Sample page
- `kParamInputBus` stereo input selection (L/R/None)
- `kParamFreeze` gate parameter for stopping capture
- Circular buffer write pointer management
- Stereo grain reading with interpolation
- 50ms crossfade implementation
- Dry input monitoring mix
- Live/Frozen UI indicators

### Definition of Done
- [x] Live Mode parameter appears on Sample page
- [x] Input bus selection works (L/R/None)
- [x] Freeze gate stops write pointer when high
- [x] Mode switches with 50ms crossfades (no clicks)
- [x] Dry input audible alongside grains
- [x] Drifters read from live buffer relative to write head
- [x] 128-sample safety zone enforced
- [x] UI shows Live/Frozen status
- [x] Build succeeds without warnings

### Must Have
- Live Mode boolean parameter
- Stereo input bus selection
- Freeze gate (level-triggered)
- 50ms crossfade transitions
- Dry input monitoring
- Relative position model (write head reference)
- 128-sample safety zone
- UI status indicators

### Must NOT Have (Guardrails)
- Separate dry/wet mix control (fixed monitoring)
- Per-drifter input selection (global only)
- Input limiting/DC blocking (user responsibility)
- SD card recording of live audio (ephemeral only)
- Clock re-anchoring (clock stays absolute)
- Manual Freeze parameter override (CV always wins)
- Zero-crossing alignment in Live mode (disabled for safety)
- Custom UI page (stays on Sample page only)
- Complex crossfade curves (linear only)
- Real-time waveform display updates (low rate only)

---

## Verification Strategy

### Test Decision
- **Infrastructure exists**: YES (make hardware builds for ARM)
- **User wants tests**: Manual verification via nt_emu emulator
- **QA approach**: Manual verification with specific test scenarios

### Manual QA Procedures
Each TODO includes verification steps using the nt_emu VCV Rack emulator:
1. Build with `make test` (creates .dylib for emulator)
2. Load plugin in nt_emu module in VCV Rack
3. Test audio routing and mode switching
4. Verify freeze behavior and crossfades
5. Build for hardware with `make hardware` when verified

---

## Task Flow

```
Task 1 (Parameter Definitions)
    |
    v
Task 2 (Buffer Management) --> Task 3 (Stereo Grain Reading) --> Task 4 (Mode Switching) --> Task 5 (Integration Test)
```

## Parallelization

| Task | Depends On | Reason |
|------|------------|--------|
| 1 | None | Independent parameter definitions |
| 2 | 1 | Needs parameters for buffer config |
| 3 | 2 | Needs buffer management for stereo reading |
| 4 | 2, 3 | Needs both buffer and grain reading ready |
| 5 | 4 | Integration testing of complete system |

---

## TODOs

- [ ] 1. Add Live Mode Parameters to Sample Page

  **What to do**:
  - Add `kParamLiveMode` boolean parameter to Sample page
  - Add `kParamInputBus` enum parameter (None=0, Left=1, Right=2, Stereo=3)
  - Add `kParamFreeze` gate parameter for freeze control
  - Update parameter enum, definitions, and page structure
  - Hide Folder/Sample parameters when Live Mode is active

  **Must NOT do**:
  - Add to custom UI (parameter page only)
  - Create separate mix controls
  - Add per-drifter input selection

  **Parallelizable**: NO (affects parameter structure)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:217-219` - Parameter enum pattern (add after existing Sample params)
  - `drifters.cpp:264-265` - Parameter definitions in parameters[] array
  - `drifters.cpp:281` - pageSample array definition (modify to conditionally show params)
  - `drifters.cpp:272` - Example enum parameter (Shape with enumStrings)
  
  **API References**:
  - `distingNT_API/examples/gainCustomUI.cpp` - Audio input parameter pattern
  - `distingNT_API/examples/flexSeqSwitch.cpp` - Bus selection with None=0 pattern
  
  **Implementation**:
  ```cpp
  // In enum (after sample parameters):
  kParamLiveMode,
  kParamInputBus,
  kParamFreeze,
  
  // In parameters array:
  { .name = "Live Mode", .min = 0, .max = 1, .def = 0, .unit = kNT_unitBool },
  { .name = "Input", .min = 0, .max = 3, .def = 0, .unit = kNT_unitEnum, .enumStrings = inputBusNames },
  { .name = "Freeze", .min = 0, .max = 1, .def = 0, .unit = kNT_unitGate },
  
  // Input bus names:
  const char* inputBusNames[] = { "None", "Left", "Right", "Stereo", NULL };
  ```

  **Acceptance Criteria**:
  - [ ] Live Mode parameter shows on Sample page
  - [ ] Input bus dropdown shows None/Left/Right/Stereo
  - [ ] Freeze gate parameter visible
  - [ ] Folder/Sample params hidden when Live Mode=On
  - [ ] Parameters return to visible when Live Mode=Off
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(live): add Live Mode parameters to Sample page`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [ ] 2. Implement Circular Buffer Management

  **What to do**:
  - Add write pointer variable for live audio capture
  - Implement audio input reading from selected bus
  - Create circular buffer write with 128-sample safety zone
  - Handle freeze state (stop write pointer when frozen)
  - Mix dry input to output for monitoring

  **Must NOT do**:
  - Modify existing sample loading code
  - Add input limiting or DC blocking
  - Create separate analysis buffers

  **Parallelizable**: NO (depends on Task 1)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:746` - CV input reading pattern
  - `drifters.cpp:1165-1176` - Grain reading with position wrapping
  
  **API References**:
  - `distingNT_API/examples/gain.cpp` - Audio input reading from busFrames
  - `distingNT_API/airwindows/src/TapeDelay.cpp` - Circular buffer patterns
  
  **Implementation**:
  ```cpp
  // Add to plugin state:
  int writePointer = 0;
  bool frozen = false;
  float crossfadeGain = 1.0f;
  float dryMix = 0.5f;  // Fixed dry monitoring level
  
  // In audio processing:
  int inputBus = pThis->v[kParamInputBus];
  bool liveMode = pThis->v[kParamLiveMode];
  bool freezeGate = pThis->v[kParamFreeze];
  
  // Handle freeze state
  if (freezeGate && !frozen) {
      frozen = true;
  } else if (!freezeGate && frozen) {
      frozen = false;
  }
  
  // Capture audio if live mode and not frozen
  if (liveMode && inputBus > 0 && !frozen) {
      // Read from selected bus
      const float* inputL = busFrames + (inputBus == 1 || inputBus == 3 ? 0 : 1) * numFrames;
      const float* inputR = busFrames + (inputBus == 2 || inputBus == 3 ? 1 : 0) * numFrames;
      
      // Write to circular buffer with safety zone
      for (int i = 0; i < numFrames; i++) {
          int safeWritePos = (writePointer + 128) % kMaxSampleFrames;
          sampleBufferL[safeWritePos] = inputL[i];
          if (inputBus == 3) {  // Stereo
              sampleBufferR[safeWritePos] = inputR[i];
          } else {
              sampleBufferR[safeWritePos] = inputL[i];  // Duplicate mono
          }
          writePointer = (writePointer + 1) % kMaxSampleFrames;
      }
  }
  
  // Mix dry input to output for monitoring
  if (liveMode && inputBus > 0) {
      for (int i = 0; i < numFrames; i++) {
          outL[i] += inputL[i] * dryMix;
          outR[i] += inputR[i] * dryMix;
      }
  }
  ```

  **Acceptance Criteria**:
  - [ ] Write pointer advances correctly in circular fashion
  - [ ] Audio captures from selected input bus
  - [ ] Freeze stops write pointer advancement
  - [ ] 128-sample safety zone maintained
  - [ ] Dry input mixed to output for monitoring
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(live): implement circular buffer and audio capture`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [ ] 3. Update Grain Reading for Stereo and Live Mode

  **What to do**:
  - Modify grain reading to support stereo interpolation
  - Implement relative position calculation (from write head)
  - Add stereo grain output mixing
  - Handle buffer wrap-around correctly
  - Maintain existing mono sample compatibility

  **Must NOT do**:
  - Remove existing mono sample support
  - Add complex stereo width controls
  - Modify grain envelope or density logic

  **Parallelizable**: NO (depends on Task 2)

  **References**:
  
  **Pattern References**:
  - `drifters.cpp:1165-1176` - Current grain reading with linear interpolation
  - `drifters.cpp:958-970` - Grain triggering and position calculation
  
  **Implementation**:
  ```cpp
  // Modify grain position calculation for Live Mode
  if (liveMode) {
      // Calculate position relative to write head
      float relativePos = grain.position * kMaxSampleFrames;
      int readPos = (writePointer - (int)relativePos + kMaxSampleFrames) % kMaxSampleFrames;
      
      // Ensure safe reading (outside write zone)
      int safeDistance = 128;
      if (readPos > writePointer - safeDistance && readPos < writePointer + safeDistance) {
          // Too close to write head, use older position
          readPos = (writePointer - safeDistance + kMaxSampleFrames) % kMaxSampleFrames;
      }
      
      // Read stereo samples with interpolation
      float sampleL = readBufferInterpolated(sampleBufferL, readPos);
      float sampleR = readBufferInterpolated(sampleBufferR, readPos);
      
      // Apply grain envelope and mix to output
      float env = grainEnvelope(grain);
      outL[sample] += sampleL * env * grain.amplitude;
      outR[sample] += sampleR * env * grain.amplitude;
  } else {
      // Existing mono sample reading (unchanged)
      float sample = readBufferInterpolated(sampleBufferL, grain.position);
      float env = grainEnvelope(grain);
      outL[sample] += sample * env * grain.amplitude;
      outR[sample] += sample * env * grain.amplitude;
  }
  ```

  **Acceptance Criteria**:
  - [ ] Stereo grain reading works with interpolation
  - [ ] Position calculated relative to write head
  - [ ] Safe zone enforcement prevents tearing
  - [ ] Buffer wrap-around handled correctly
  - [ ] Existing mono samples still work unchanged
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(live): add stereo grain reading and relative positioning`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [ ] 4. Implement Mode Switching and Crossfades

  **What to do**:
  - Add 50ms crossfade when switching between Sample and Live modes
  - Implement smooth parameter transitions
  - Handle freeze/unfreeze with gain ramps
  - Add UI state tracking for Live/Frozen indicators
  - Ensure click-free mode changes

  **Must NOT do**:
  - Add complex crossfade curves (linear only)
  - Modify grain spawning timing
  - Add per-sample branching in audio loops

  **Parallelizable**: NO (depends on Tasks 2 and 3)

  **References**:
  
  **Pattern References**:
  - `distingNT_API/examples/flexSeqSwitch.cpp` - Crossfade implementation patterns
  
  **Implementation**:
  ```cpp
  // Add crossfade state
  float crossfadeTime = 0.05f * sampleRate;  // 50ms in samples
  float crossfadeCounter = 0;
  bool crossfadeActive = false;
  float oldModeGain = 1.0f;
  float newModeGain = 0.0f;
  
  // In mode switching logic:
  if (liveMode != previousLiveMode) {
      crossfadeActive = true;
      crossfadeCounter = 0;
      if (liveMode) {
          oldModeGain = 1.0f;  // Sample mode fading out
          newModeGain = 0.0f;  // Live mode fading in
      } else {
          oldModeGain = 0.0f;  // Sample mode fading in
          newModeGain = 1.0f;  // Live mode fading out
      }
  }
  
  // In audio loop:
  if (crossfadeActive) {
      float fade = crossfadeCounter / crossfadeTime;
      fade = fminf(fade, 1.0f);
      
      if (liveMode) {
          oldModeGain = 1.0f - fade;  // Sample fades out
          newModeGain = fade;           // Live fades in
      } else {
          oldModeGain = fade;           // Sample fades in
          newModeGain = 1.0f - fade;    // Live fades out
      }
      
      crossfadeCounter += numFrames;
      if (crossfadeCounter >= crossfadeTime) {
          crossfadeActive = false;
          oldModeGain = liveMode ? 0.0f : 1.0f;
          newModeGain = liveMode ? 1.0f : 0.0f;
      }
  }
  
  // Apply gains to respective audio paths
  // Mix sample output with oldModeGain
  // Mix live grains with newModeGain
  ```

  **Acceptance Criteria**:
  - [ ] 50ms crossfade implemented for mode switching
  - [ ] Linear gain ramps applied correctly
  - [ ] Freeze/unfreeze uses gain ramps
  - [ ] No clicks or pops during transitions
  - [ ] Crossfade state resets properly
  - [ ] Code compiles without warnings

  **Commit**: YES
  - Message: `feat(live): implement mode switching with 50ms crossfades`
  - Files: `drifters.cpp`
  - Pre-commit: `make hardware`

---

- [ ] 5. Integration Testing and Verification

  **What to do**:
  - Build for nt_emu testing: `make test`
  - Test all mode switching scenarios
  - Verify freeze behavior and crossfades
  - Test input routing and dry monitoring
  - Build for hardware: `make hardware`
  - Document any issues or limitations

  **Must NOT do**:
  - Skip edge case testing
  - Ignore performance issues
  - Skip hardware build verification

  **Parallelizable**: NO (depends on Task 4)

  **References**:
  
  **Testing Guide**:
  - `/Users/nealsanche/.claude/skills/disting-nt-cpp-plugin-writer/testing.md` - nt_emu testing workflow
  
  **Test Matrix**:
  | Test | Mode | Input | Freeze | Expected |
  |------|------|-------|---------|----------|
  | Mode Switch | Sample→Live | Any | Off | 50ms crossfade, no clicks |
  | Input Routing | Live | Left→Right→Stereo | Off | Correct input captured |
  | Freeze | Live | Any | On→Off | Write stops/starts, no clicks |
  | Dry Monitor | Live | Any | Off | Dry input audible |
  | Position | Live | Any | Off | Drifters move relative to write head |
  | Safety Zone | Live | Any | Off | No tearing within 128 samples |
  | Wraparound | Live | Any | Off | Buffer wraps correctly |
  | Preset Change | Any | Any | Any | Mode state preserved |

  **Acceptance Criteria**:
  - [ ] `make test` builds without errors
  - [ ] Plugin loads in nt_emu without crashes
  - [ ] All test matrix scenarios pass
  - [ ] `make hardware` builds without errors
  - [ ] .o file size is under 64KB limit
  - [ ] No CPU spikes or audio glitches
  - [ ] Documented any known limitations

  **Commit**: NO (testing only, no code changes)

---

## Commit Strategy

| After Task | Message | Files | Verification |
|------------|---------|-------|--------------|
| 1 | `feat(live): add Live Mode parameters to Sample page` | drifters.cpp | make hardware |
| 2 | `feat(live): implement circular buffer and audio capture` | drifters.cpp | make hardware |
| 3 | `feat(live): add stereo grain reading and relative positioning` | drifters.cpp | make hardware |
| 4 | `feat(live): implement mode switching with 50ms crossfades` | drifters.cpp | make hardware |
| 5 | (no commit - testing only) | - | make test, nt_emu |

---

## Success Criteria

### Verification Commands
```bash
make hardware  # Expected: Build succeeds, .o file created
make test      # Expected: Build succeeds, .dylib created for nt_emu
```

### Final Checklist
- [ ] All "Must Have" items present
- [ ] All "Must NOT Have" items absent
- [ ] Live Mode parameter functional
- [ ] Input routing works correctly
- [ ] Freeze gate stops capture
- [ ] Mode switches without clicks
- [ ] Dry monitoring audible
- [ ] Stereo grain reading works
- [ ] Position relative to write head
- [ ] 128-sample safety zone enforced
- [ ] Build succeeds without warnings
- [ ] Plugin size under 64KB limit