# JUCE Plugin Code Review - December 2025

## Executive Summary

The Boomerang+ plugin is functionally operational in standalone mode but has architectural and threading concerns that should be addressed before production release. Current assessment: **Alpha quality - 7/10**

### Working Well ✅

- Audio processing logic is solid
- Buffer handling is correct and safe
- UI updates function properly
- Channel layout handling (mono→stereo) works correctly
- Standalone functionality is complete

### Needs Attention ⚠️

- Thread safety violations between UI and audio threads
- Missing host automation support for button parameters
- Known playback issues (reverse mode, once mode)
- Architecture doesn't follow JUCE best practices (APVTS pattern)

---

## Critical Issues

### 1. Thread Safety Violations (Data Races)

**Severity:** HIGH  
**Status:** ✅ FIXED in issue #38  
**Impact:** Undefined behavior, potential crashes in DAW hosts under load

**Problem:**

```cpp
// UI Thread (Editor):
recordButton.onClick = [this]() { 
    audioProcessor.getLooperEngine()->onRecordButtonPressed(); // No synchronization
};

// Engine method (called from UI thread):
void LooperEngine::onRecordButtonPressed() {
    currentState = LooperState::Recording;  // No lock, not atomic
}

// Audio Thread:
void LooperEngine::processBlock(...) {
    switch (currentState) {  // Reads without synchronization
        // ...
    }
}
```

**Location:** 
- `PluginEditor.cpp` lines 64-82 (button handlers)
- `LooperEngine.cpp` lines 104-248 (state mutation methods)
- `LooperEngine.h` lines 143-158 (non-atomic state variables)

**JUCE Documentation Says:**
> "Be very careful about what you do in this callback - it's going to be called by the audio thread, so any kind of interaction with the UI is absolutely out of the question."

**Recommendation:**
- ✅ IMPLEMENTED: Made all state variables `std::atomic<>` for lock-free thread safety
- ✅ IMPLEMENTED: Added `shouldDisableOnce` request flag for audio→UI communication  
- ✅ IMPLEMENTED: Added 16ms UI timer to process audio thread requests safely
- ✅ IMPLEMENTED: Removed all direct state writes from audio thread
- Future: Consider migrating to `AudioProcessorValueTreeState` (tracked in issue #39)

**Resolution:** All state enums are now atomic, Once mode auto-off uses request flags, and audio thread no longer writes to user-facing state. Thread-safe per C++ memory model.

---

### 2. Missing Host Automation for Buttons
**Severity:** MEDIUM (Feature Gap)  
**Status:** Buttons work in UI but aren't exposed to DAW  
**Impact:** No automation recording, no parameter save/restore for button states

**Problem:**
- Button parameter pointers are declared in `PluginProcessor.h` (lines 75-82)
- They are **never created** in the constructor
- They are **never used** anywhere in the code
- UI buttons are separate `juce::TextButton` components

**Location:**
- `PluginProcessor.h` lines 75-82 (unused parameter declarations)
- `PluginProcessor.cpp` lines 5-27 (constructor - only creates volume/feedback)

**Impact:**
- ✅ Standalone mode works (uses UI buttons directly)
- ⚠️ DAW hosts can't automate button presses
- ❌ Button states not saved with project
- ❌ No MIDI learn for buttons
- ❌ Button states don't appear in automation lanes

**Recommendation:**
- Either remove unused parameter declarations (if automation not needed)
- Or properly initialize parameters and connect to UI (if automation desired)
- Consider implementing `AudioProcessorValueTreeState` for clean parameter management

---

## Known Bugs (From GitHub Issues)

### 3. Reverse Playback Position Wrapping (#32)
**Severity:** HIGH (Core Functionality)  
**Status:** Known issue, needs fix  
**Impact:** Playback bounces/jumps mid-loop in reverse mode

**Location:** `LooperEngine.cpp` lines 559-575 (`advancePosition()`)

**Current Logic:**
```cpp
if (loopMode == LoopMode::Reverse) {
    position -= speed;
    if (position < 0) {
        position += length;  // Wraps correctly
        wrapped = true;
    }
}
```

**Additional Issue:** Interpolation in `processPlayback()` (lines 438-445) may cause jitter at wrap boundaries.

**Recommendation:** Review fractional position handling and interpolation near boundaries.

---

### 4. Once Mode Reliability in Overdub/Stack (#33)
**Severity:** MEDIUM  
**Status:** Known issue, behavior undefined  
**Impact:** Once mode doesn't work reliably when combined with overdub

**Location:** 
- `LooperEngine.cpp` lines 182-207 (`onOnceButtonPressed()`)
- `LooperEngine.cpp` lines 470-476 (once mode stop logic in playback)
- `LooperEngine.cpp` lines 527-533 (once mode stop logic in overdub)

**Problem:** Once mode is toggled off after playback ends in audio thread without proper state separation.

**Recommendation:** Separate "once armed" from "once executing" states, handle atomically.

---

## Architecture & Best Practices Issues

### 5. Not Using AudioProcessorValueTreeState (APVTS)
**Severity:** MEDIUM (Architecture)  
**Status:** Using manual parameter management  
**Impact:** More code, harder to maintain, missing JUCE features

**JUCE Recommendation:**
> "AudioProcessorValueTreeState provides a ValueTree that acts as a single unified source of truth for all parameter and state data."

**Current Approach:**
- Manual `addParameter()` with raw pointers
- Manual attachment creation in editor
- No undo/redo support
- Manual state save/restore

**Benefits of APVTS:**
- Automatic thread-safe parameter access
- Built-in undo/redo
- Cleaner parameter creation
- Automatic state management
- Parameter listeners with proper threading

**Recommendation:** Migrate to APVTS for cleaner architecture and better host compatibility.

---

### 6. Incomplete State Persistence
**Severity:** MEDIUM  
**Status:** Only saves volume/feedback  
**Impact:** Loop content and button states not restored

**Location:** `PluginProcessor.cpp` lines 189-212

**Currently Saved:**
- ✅ Volume parameter
- ✅ Feedback parameter

**Not Saved:**
- ❌ Loop buffer content
- ❌ Active loop slot
- ❌ Current state (recording/playing/stopped)
- ❌ Button states (thru/mute, reverse, once, stack)
- ❌ Speed mode

**Recommendation:** 
- Decide on loop content persistence strategy (can be large)
- Add button states and current mode to state save
- Consider adding a preset system

---

### 7. Compiler Warning Suppressions
**Severity:** LOW (Code Quality)  
**Status:** Warnings suppressed instead of fixed  
**Impact:** Hidden potential issues

**Location:** `CMakeLists.txt` lines 66-72

**Suppressed Warnings:**
```cmake
-Wno-unused-parameter
-Wno-shadow
-Wno-sign-conversion
-Wno-switch
-Wno-unused-variable
```

**Recommendation:** Address warnings properly:
- Add `default:` cases to switch statements
- Use `juce::ignoreUnused()` for intentionally unused parameters
- Fix sign conversions with proper casts
- Remove unused variables

---

## Performance Considerations

### 8. Float Division in Audio Thread
**Severity:** LOW (Optimization)  
**Status:** Minor performance opportunity  
**Impact:** Negligible on modern CPUs

**Location:** `LooperEngine.cpp` line 472 and others

**Current:**
```cpp
float progress = activeSlot.playPosition / static_cast<float>(activeSlot.length);
```

**Optimization:**
```cpp
// Cache in prepare():
float lengthReciprocal = 1.0f / static_cast<float>(length);

// Use in processBlock():
float progress = activeSlot.playPosition * lengthReciprocal;
```

**Recommendation:** Low priority optimization for future improvement.

---

### 9. Cache Locality in Playback Loop
**Severity:** LOW (Optimization)  
**Status:** Sample-then-channel loop order  
**Impact:** Minor cache misses

**Location:** `LooperEngine.cpp` lines 421-465

**Current Structure:**
```cpp
for (int sample = 0; sample < numSamples; ++sample) {
    for (int channel = 0; channel < numChannels; ++channel) {
        // Process
    }
}
```

**Better for Cache:**
```cpp
for (int channel = 0; channel < numChannels; ++channel) {
    for (int sample = 0; sample < numSamples; ++sample) {
        // Process
    }
}
```

**Recommendation:** Low priority, measure before optimizing.

---

## Code Quality Issues

### 10. Dead/Unused Code
**Severity:** LOW (Maintenance)  
**Status:** Multiple unused functions and variables  
**Impact:** Code clutter, confusion

**Unused Functions:**
- `onStackButtonToggled()` - LooperEngine.cpp:201-222
- `applyCrossfade()` - LooperEngine.cpp:577-590
- `switchToNextLoopSlot()` - LooperEngine.cpp:592-595

**Unused Variables:**
- `recordingStartDelay` - LooperEngine.h:161
- `crossfadeSamples` constant - LooperEngine.h:137

**Commented-Out Code:**
- Progress bar - PluginEditor.cpp:59, 172, 174
- Loop slot indicators - PluginEditor.cpp:117-134

**Recommendation:** Remove unused code or wrap in `#if 0` with TODO comments.

---

### 11. Inconsistent Naming Conventions
**Severity:** LOW (Style)  
**Status:** Mix of camelCase and snake_case  
**Impact:** Reduced readability

**Project Style Guide:** `snake_case` for variables, `CamelCase` for types

**Violations:**
- `currentState` → should be `current_state`
- `loopMode` → should be `loop_mode`
- `volumeParam` → should be `volume_param`
- `feedbackAmount` → should be `feedback_amount`

**Recommendation:** Gradually migrate to consistent style, prioritize new code.

---

### 12. Missing Assertions and Validation
**Severity:** LOW (Robustness)  
**Status:** No defensive programming  
**Impact:** Harder to debug issues

**Recommendations:**
```cpp
void LooperEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    jassert(sampleRate > 0);
    jassert(samplesPerBlock > 0);
    jassert(numChannels > 0 && numChannels <= 2);
    
    jassert(activeLoopSlot >= 0 && activeLoopSlot < maxLoopSlots);
    // ...
}
```

---

## Testing Gaps

### 13. No Automated Tests
**Severity:** MEDIUM  
**Status:** No test suite  
**Impact:** Manual testing only, regression risk

**Recommendation:**
- Add JUCE `UnitTestRunner` tests
- Test critical logic:
  - Position wrapping (forward/reverse)
  - State transitions
  - Buffer overflow protection
  - Overdub mixing calculations

---

## Priority Summary

### Must Fix Before Beta
1. ✅ Thread safety (atomic states or locks)
2. ✅ Reverse playback bug (#32)
3. ⚠️ Decide on parameter automation strategy

### Should Fix Before Release
4. ✅ Once mode reliability (#33)
5. ✅ Complete state persistence
6. ✅ Fix compiler warnings
7. ✅ Remove dead code

### Nice to Have
8. ⚠️ Migrate to APVTS
9. ⚠️ Add automated tests
10. ⚠️ Performance optimizations
11. ⚠️ Consistent naming conventions

---

## Conclusion

The Boomerang+ plugin demonstrates solid understanding of JUCE audio processing fundamentals. The core audio engine is well-structured and the basic functionality works correctly.

**Primary concerns are:**
1. Thread safety between UI and audio threads (real but subtle issue)
2. Incomplete host integration (no parameter automation)
3. Known playback bugs that need addressing

**The code is production-ready for standalone use** but needs threading fixes and better host integration for professional DAW deployment.

**Estimated effort to address critical issues:** 2-3 days
**Estimated effort for full best-practices compliance:** 1-2 weeks

---

*Review conducted: December 16, 2025*  
*Reviewer: AI Code Analysis with JUCE Documentation Cross-Reference*  
*Plugin Version: v2.0.0-alpha-0*
