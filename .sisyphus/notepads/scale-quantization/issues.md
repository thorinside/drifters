# Scale Quantization - Issues Encountered

## [2026-01-08] Task 1 First Attempt - CATASTROPHIC FAILURE

**Problem**: First delegation of Task 1 DELETED 938 lines from drifters.cpp instead of just adding scale data.

**Root Cause**: Agent misunderstood "insert after shapeNames" and replaced large sections of code.

**Impact**: 
- File reduced from 1381 lines to 526 lines
- customUi function completely deleted
- Most of step() function deleted
- Build completely broken

**Resolution**:
- Immediately reverted with `git restore drifters.cpp`
- Re-delegated with STRICT constraints: "INSERT only, DO NOT DELETE"
- Emphasized verification: git diff must show ONLY additions (+), NO deletions (-)
- Second attempt succeeded: +80 lines, -0 deletions

**Lesson**: When delegating insertion tasks, explicitly forbid deletions and require git diff verification.

---

## [2026-01-08] Task 3 Build Error - Stale Artifacts

**Problem**: After Task 3 completion, `make hardware` showed error about kParamScale being defined twice.

**Root Cause**: Stale build artifacts in build/ directory from previous failed build.

**Resolution**:
- `make clean` to remove stale artifacts
- Rebuild succeeded cleanly
- Error was false alarm from cached state

**Lesson**: After major file changes or reverts, always clean build artifacts.

---

## [2026-01-08] No Critical Issues in Tasks 2, 4, 5

Tasks 2 (quantization functions), 4 (DSP integration), and 5 (testing) completed without issues on first attempt.
