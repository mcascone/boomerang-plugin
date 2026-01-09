# GitHub Issues to Create

Based on the code review findings, here are the recommended GitHub issues to create:

## Issues Already Exist
- ✅ **#32** - Reverse playback bounces mid-loop (CLOSED)
- ✅ **#33** - Once mode regressions in overdub/stack path (CLOSED)  
- ✅ **#34** - Missing switch cases for LooperState (CLOSED)
- ✅ **#35** - Sign-conversion and shadow warnings in LooperEngine (CLOSED)
- ❌ **#36** - VST3 ad-hoc signing / missing resources warning during install (OPEN)

## TODO from Parameter Notification Work 📝

### Build Script
- Git hash message appears in middle of build output, should be at bottom after "Build complete!"

### GP Panel Integration
- ✅ ~~**REC widget doesn't blink on loop cycle**~~ - DONE: loopCycle parameter pulses on loop wrap
- ✅ ~~**SLOW mode indicator needed**~~ - DONE: slowMode parameter exposed for GP panel
- **Once button toggle widget desync** - When Once is ON and you press Once (to restart), the toggle widget briefly shows OFF while state stays ON. This is because Once has context-dependent behavior (restart vs toggle). Will also affect bidirectional MIDI - controller LEDs will desync. Solutions: GP scripting with dual widgets/overlays, or proper MIDI state feedback implementation. Not blocking; document for future MIDI work.

## New Issues to Create 📝

### Priority 1: Critical Issues

#### ✅ ~~1. Thread Safety Violations Between UI and Audio Threads~~ - FIXED: All state variables now use `std::atomic<>`

#### ✅ ~~2. Missing Host Automation for Button Parameters~~ - FIXED: All buttons exposed via APVTS (thruMute, record, play, once, stack, reverse + indicator params loopCycle, slowMode, onceState)

### Priority 2: Architecture Improvements

#### ✅ ~~3. Migrate to AudioProcessorValueTreeState (APVTS)~~ - DONE: All parameters use APVTS with createParameterLayout(), SliderAttachment for volume

#### 🔮 ~~4. Incomplete State Persistence~~ - AS DESIGNED: Original hardware doesn't persist loop content or mode state on power cycle. Future enhancement to optionally save/restore loop buffers.

### Priority 3: Code Quality

#### ✅ ~~5. Remove Dead/Unused Code~~ - PARTIAL: Removed `crossfadeSamples`, `recordingStartDelay`. Kept `switchToNextLoopSlot()` (for A/B feature), `progressBar` (future UI)

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

**Completed:**
- ✅ 1. Thread Safety Violations - FIXED
- ✅ 2. Missing Host Automation - FIXED
- ✅ 3. Migrate to APVTS - DONE
- 🔮 4. Incomplete State Persistence - AS DESIGNED
- ✅ 5. Remove Dead Code - PARTIAL

**Future:**
- 6. Inconsistent Naming
- 7. Add Unit Tests
- 8. Add Debug Assertions

## Notes
- Issues #32-35 are resolved (closed)
- Issue #36 (VST3 signing) still open
- All new issues reference CODE_REVIEW_FINDINGS.md for details
