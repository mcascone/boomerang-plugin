# Parameter Notification Implementation Summary

**Branch:** `parameter-host-notify`  
**Date:** January 8, 2026  
**Status:** ✅ Working - All buttons update correctly between host and plugin

---

## What Was Implemented

Successfully implemented bidirectional parameter state synchronization between the plugin's internal state machine and the host (Gig Performer panel widgets).

### Core Changes

1. **LooperEngine.h**
   - Added `ParameterNotifyCallback` function type
   - Added `setParameterNotifyCallback()` method
   - Added `parameterNotifyCallback` member variable

2. **PluginProcessor.h**
   - Moved `ParameterIDs` namespace from .cpp to .h (shared access)
   - Added `updatingFromInternalState` atomic flag to prevent circular notifications
   - Added `prevStackValue` for edge detection (already existed)

3. **PluginProcessor.cpp**
   - Set up callback in constructor that:
     - Defers to message thread using `MessageManager::callAsync()`
     - Wraps in `beginChangeGesture()` / `endChangeGesture()`
     - Looks up parameter safely by ID (not raw pointer)
     - Sets `updatingFromInternalState` flag to prevent circular calls
   - Handles toggle widgets that send parameter changes on every toggle

4. **LooperEngine.cpp**
   - Added `#include "PluginProcessor.h"` for ParameterIDs access
   - Added notification calls in state transition functions:
     - `startRecording()` → `record = 1.0`
     - `stopRecording()` → `record = 0.0`
     - `startPlayback()` → `play = 1.0`
     - `stopPlayback()` → `play = 0.0`
     - `toggleThruMute()` → `thruMute = 1.0 or 0.0`
     - `toggleDirection()` → `reverse = 1.0 or 0.0`
     - `toggleOnceMode()` / `setOnceMode()` → `once = 1.0 or 0.0`
     - `startOverdubbing()` → `stack = 1.0`
     - `stopOverdubbing()` → `stack = 0.0`

---

## How It Works

### The Flow - Plugin UI → Host

1. User clicks button in plugin UI
2. `PluginEditor` calls `looperEngine->onXButtonPressed()`
3. Engine changes internal state
4. Engine calls `parameterNotifyCallback(paramID, value)`
5. Callback schedules async task on message thread
6. Message thread: `beginChangeGesture()` → `setValueNotifyingHost()` → `endChangeGesture()`
7. Host receives notification and updates panel widget ✓

### The Flow - Host Panel → Plugin

1. User clicks button in GP panel
2. Host sends parameter change → `parameterChanged(paramID, value)`
3. Check `updatingFromInternalState` flag (false) → continue
4. Call `looperEngine->onXButtonPressed()`
5. Engine changes internal state
6. Engine calls `parameterNotifyCallback(paramID, value)`
7. Callback sets flag = true, schedules async task
8. Async task: `beginChangeGesture()` → `setValueNotifyingHost()` → `endChangeGesture()`
9. This triggers `parameterChanged()` again, but flag is true → **early return** (prevents circular call)
10. Host receives notification and updates panel widget ✓

### Critical Design Decisions

**Thread Safety:**
- All `setValueNotifyingHost()` calls deferred to message thread via `MessageManager::callAsync()`
- Parameter ID captured (safe), not raw pointer
- Flag prevents circular notifications

**Toggle Widget Compatibility:**
- GP widgets configured as toggles send changes on every toggle (0→1 and 1→0)
- Record/Play handlers called on every change (not just rising edge)
- Flag prevents the echo-back from triggering handler again

**Change Gestures:**
- `beginChangeGesture()` / `endChangeGesture()` wrap the notification
- Tells host this is automated/internal change (not user action)
- Enables proper automation recording and undo/redo

---

## Testing Results

✅ **Working Correctly:**
- All buttons trigger correct state changes from both panel and plugin UI
- Once mode properly disables in panel when plugin disengages it
- Play button turns ON when Record is pressed twice (Recording → Playing)
- Both Play and Once turn OFF when playback stops at end of loop
- Panel widgets update when controlled from plugin UI
- Plugin updates when controlled from panel widgets

---

## Known Limitations / TODO

### Minor Issues (tracked in GITHUB_ISSUES_TODO.md)

1. **REC widget doesn't blink on loop cycle**
   - Plugin UI LED blinks when loop wraps
   - No corresponding parameter pulse sent to host
   - Would need: Brief parameter pulse or loop progress parameter

2. **SLOW mode indicator needed for GP**
   - Speed mode (Half/Normal) shown by LED in UI
   - No parameter exposed for GP panel
   - Would need: Read-only parameter for speed mode state

3. **Build script git hash placement**
   - Git hash message appears mid-build
   - Should appear at bottom after "Build complete!"
   - Attempted fix didn't work - needs revisiting

---

## File Changes Summary

**Modified:**
- `Source/LooperEngine.h` - Added callback mechanism
- `Source/LooperEngine.cpp` - Added notification calls in state transitions
- `Source/PluginProcessor.h` - Moved ParameterIDs, added flag
- `Source/PluginProcessor.cpp` - Set up callback with proper threading
- `build.sh` - Attempted git hash placement (WIP)
- `GITHUB_ISSUES_TODO.md` - Added TODO items

**Commits on branch:**
- `a87c4f1` - Fix host parameter notifications with proper thread safety
- `d097836` - move hash print
- `2ba5086` - Add beginChangeGesture() / endChangeGesture()
- `66d2dc2` - revert to calling on every change
- `76fd04f` - Add warning for uncommitted changes in build

---

## To Resume Work

1. **Merge to main when ready:**
   ```bash
   git checkout main
   git merge parameter-host-notify
   ```

2. **If adding REC blink feature:**
   - Consider brief parameter pulse when `loopWrapped` flag is set
   - Or add read-only `loopProgress` parameter (0.0-1.0)
   - GP panel could show progress or trigger flash animation

3. **If adding SLOW mode indicator:**
   - Add read-only boolean parameter `speedMode` 
   - Update in `toggleSpeedMode()` and `setSpeedMode()`
   - Map to LED indicator in GP panel

4. **If fixing build script:**
   - Investigate why grep filter didn't work
   - May need to modify CMake script instead
   - Or post-process entire build output

---

## Key Learnings

1. **JUCE Parameter Notifications Must Be on Message Thread**
   - Use `MessageManager::callAsync()` for deferred execution
   - Never call `setValueNotifyingHost()` directly from audio thread

2. **Change Gestures Are Important**
   - Wrap notifications in `beginChangeGesture()` / `endChangeGesture()`
   - Distinguishes internal changes from user automation
   - Required for proper host behavior

3. **Circular Notification Prevention**
   - `setValueNotifyingHost()` triggers `parameterChanged()` listener
   - Need flag to break the circular call chain
   - Set flag before call, clear after

4. **Toggle vs Momentary Widget Behavior**
   - Toggle widgets send parameter changes on every toggle
   - Must handle both edges if using toggles
   - Could use momentary for action buttons, but toggles work with flag

5. **Safe Lambda Captures**
   - Don't capture raw parameter pointers across thread boundaries
   - Capture parameter ID string instead
   - Look up parameter in the async callback context
