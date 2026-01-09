# GitHub Issues to Create

Based on the code review findings, here are the recommended GitHub issues to create:

## Issues Already Exist ✅
- ❌ **#32** - Reverse playback bounces mid-loop (CLOSED)
- ❌ **#33** - Once mode regressions in overdub/stack path (CLOSED)  
- ✅ **#34** - Missing switch cases for LooperState (OPEN)
- ✅ **#35** - Sign-conversion and shadow warnings in LooperEngine (OPEN)
- ✅ **#36** - VST3 ad-hoc signing / missing resources warning during install (OPEN)

## TODO from Parameter Notification Work 📝

### Build Script
- Git hash message appears in middle of build output, should be at bottom after "Build complete!"

### GP Panel Integration
- **REC widget doesn't blink on loop cycle** - UI LED blinks when loop wraps, but no corresponding parameter update sent to host. Need to add brief parameter pulse or expose loop progress.
- **SLOW mode indicator needed** - Speed mode (Half/Normal) visible in UI LED but no parameter exposed for GP panel. Need to add read-only parameter for speed mode state.
- **Once button toggle widget desync** - When Once is ON and you press Once (to restart), the toggle widget briefly shows OFF while state stays ON. This is because Once has context-dependent behavior (restart vs toggle). Will also affect bidirectional MIDI - controller LEDs will desync. Solutions: GP scripting with dual widgets/overlays, or proper MIDI state feedback implementation. Not blocking; document for future MIDI work.

## New Issues to Create 📝

### Priority 1: Critical Issues

#### 1. Thread Safety Violations Between UI and Audio Threads
**Labels:** `bug`, `critical`, `threading`  
**Milestone:** Beta  
**Description:**
```markdown
## Problem
UI button handlers directly modify LooperEngine state without synchronization, creating data races between the message thread and audio thread.

## Evidence
- Button `onClick` handlers call engine methods directly (PluginEditor.cpp:64-82)
- State variables are not atomic and have no locks (LooperEngine.h:143-158)
- Audio thread reads these in `processBlock()` without synchronization

## JUCE Documentation Says
> "Be very careful about what you do in this callback - it's going to be called by the audio thread, so any kind of interaction with the UI is absolutely out of the question."

## Impact
- Undefined behavior (data races)
- Potential crashes under heavy load or in some DAW hosts
- Works in practice on modern CPUs but not guaranteed

## Solutions
1. Make state variables `std::atomic<>`
2. Use `AudioProcessor::getCallbackLock()` in button handlers
3. Migrate to `AudioProcessorValueTreeState` (recommended)

## Files Affected
- `PluginEditor.cpp` (lines 64-82)
- `LooperEngine.cpp` (lines 104-248)
- `LooperEngine.h` (lines 143-158)
```

#### 2. Missing Host Automation for Button Parameters
**Labels:** `enhancement`, `architecture`  
**Milestone:** v2.0  
**Description:**
```markdown
## Problem
Button parameters are declared in `PluginProcessor.h` but never created or used. UI buttons work but aren't exposed to the host for automation.

## Current Behavior
- ✅ Buttons work in standalone mode
- ❌ Can't automate buttons from DAW
- ❌ Button states not saved with project
- ❌ No MIDI learn for buttons
- ❌ Button states don't appear in automation lanes

## Affected Parameters
- `thruMuteButton`
- `recordButton`
- `playButton`
- `onceButton`
- `stackButton`
- `reverseButton`

## Solution Options
1. Remove unused parameter declarations (if automation not needed for alpha)
2. Properly initialize and wire up parameters
3. Migrate to `AudioProcessorValueTreeState` for proper parameter management (recommended)

## Files Affected
- `PluginProcessor.h` (lines 75-82)
- `PluginProcessor.cpp` (constructor, lines 5-27)
```

### Priority 2: Architecture Improvements

#### 3. Migrate to AudioProcessorValueTreeState (APVTS)
**Labels:** `enhancement`, `architecture`, `refactor`  
**Milestone:** v2.0  
**Description:**
```markdown
## Current Situation
Using manual parameter management with raw pointers instead of JUCE's recommended `AudioProcessorValueTreeState`.

## Benefits of APVTS
- ✅ Thread-safe parameter access (solves issue #[thread-safety-number])
- ✅ Automatic UI synchronization
- ✅ Built-in undo/redo support
- ✅ Cleaner parameter creation
- ✅ Automatic state save/restore
- ✅ Parameter listeners with proper threading

## JUCE Recommendation
> "AudioProcessorValueTreeState provides a ValueTree that acts as a single unified source of truth for all parameter and state data."

## Migration Plan
1. Create APVTS in constructor
2. Add parameters to APVTS
3. Use attachment classes for UI binding
4. Remove manual `addParameter()` calls
5. Simplify state save/restore

## Impact
This would resolve multiple issues including thread safety (#[number]) and parameter automation (#[number]).
```

#### 4. Incomplete State Persistence
**Labels:** `bug`, `state-management`  
**Milestone:** v2.0  
**Description:**
```markdown
## Problem
Only volume and feedback parameters are saved. All other state is lost on project reload.

## Currently Saved ✅
- Volume
- Feedback

## Not Saved ❌
- Loop buffer content
- Active loop slot
- Current state (recording/playing/stopped)
- Button states (thru/mute, reverse, once, stack)
- Speed mode

## Impact
- User loses all work when reloading project
- Can't restore previous session state
- No preset system possible

## Considerations
- Loop buffer can be large (up to 16.6 MB)
- May want to make loop content persistence optional
- Consider compression for loop data

## Files Affected
- `PluginProcessor.cpp` (lines 189-212)
```

### Priority 3: Code Quality

#### 5. Remove Dead/Unused Code
**Labels:** `cleanup`, `maintenance`  
**Milestone:** v2.0  
**Description:**
```markdown
## Unused Functions
- `onStackButtonToggled()` - LooperEngine.cpp:201-222
- `applyCrossfade()` - LooperEngine.cpp:577-590
- `switchToNextLoopSlot()` - LooperEngine.cpp:592-595

## Unused Variables
- `recordingStartDelay` - LooperEngine.h:161
- `crossfadeSamples` constant - LooperEngine.h:137

## Commented-Out Code
- Progress bar - PluginEditor.cpp:59, 172, 174
- Loop slot indicators - PluginEditor.cpp:117-134

## Recommendation
Remove or wrap in `#if 0` with clear TODO comments explaining future plans.
```

#### 6. Inconsistent Naming Conventions
**Labels:** `style`, `cleanup`  
**Milestone:** v2.1  
**Description:**
```markdown
## Project Style Guide
- `snake_case` for variables
- `CamelCase` for types

## Current Violations
- `currentState` → should be `current_state`
- `loopMode` → should be `loop_mode`
- `volumeParam` → should be `volume_param`
- `feedbackAmount` → should be `feedback_amount`

See AGENTS.md for full style guidelines.

## Recommendation
Gradually migrate to consistent style. Prioritize new code following conventions.
```

### Priority 4: Testing & Validation

#### 7. Add Automated Unit Tests
**Labels:** `enhancement`, `testing`  
**Milestone:** v2.1  
**Description:**
```markdown
## Current Situation
No automated test suite. All testing is manual.

## Recommended Tests
Using JUCE `UnitTestRunner`:
- Position wrapping (forward/reverse)
- State transitions
- Buffer overflow protection
- Overdub mixing calculations
- Speed mode calculations
- Once mode behavior

## Benefits
- Catch regressions early
- Document expected behavior
- Enable confident refactoring
- CI/CD integration
```

#### 8. Add Debug Assertions
**Labels:** `enhancement`, `robustness`  
**Milestone:** v2.0  
**Description:**
```markdown
## Problem
No defensive programming with `jassert()` for invariants.

## Recommended Assertions
```cpp
void LooperEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels) {
    jassert(sampleRate > 0);
    jassert(samplesPerBlock > 0);
    jassert(numChannels > 0 && numChannels <= 2);
    // ...
}

void LooperEngine::processBlock(...) {
    jassert(activeLoopSlot >= 0 && activeLoopSlot < maxLoopSlots);
    // ...
}
```

## Benefits
- Catch bugs in debug builds
- Document assumptions
- Help with debugging
```

## Issue Creation Order

**Immediate:**
1. Thread Safety Violations (Critical)
2. Missing Host Automation (Feature Gap)

**Next Sprint:**
3. Migrate to APVTS (solves multiple issues)
4. Incomplete State Persistence
5. Remove Dead Code

**Future:**
6. Inconsistent Naming
7. Add Unit Tests
8. Add Debug Assertions

## Notes
- Issues #32 and #33 are already resolved (closed)
- Issues #34, #35, #36 already exist
- All new issues reference CODE_REVIEW_FINDINGS.md for details
