# MIDI Sync Implementation Plan

**Issue:** #20 - Feature: MIDI Sync  
**Date:** 2026-02-02  
**Status:** Planning  
**Priority:** High (most requested feature)

---

## Feature Summary

Add the ability to sync loop length to the DAW tempo/transport, so recorded loops align with musical bars/beats.

### User Stories

1. **Quantized Recording**: When I finish recording, the loop length should snap to the nearest bar boundary so it stays in time with my DAW.
2. **Tempo-Aware Playback**: When the DAW tempo changes, the loop should stretch/compress to stay in sync (optional, phase 2).
3. **Transport Sync**: When the DAW starts/stops, the loop should start/stop too (optional, phase 2).

---

## Technical Background

### JUCE Host Transport API

JUCE provides host tempo/transport info via `AudioPlayHead`:

```cpp
// In processBlock():
if (auto* playhead = getPlayHead())
{
    if (auto position = playhead->getPosition())
    {
        if (auto bpm = position->getBpm())
            currentBPM = *bpm;  // e.g., 120.0
        
        if (auto timeSig = position->getTimeSignature())
        {
            beatsPerBar = timeSig->numerator;   // e.g., 4
            beatUnit = timeSig->denominator;    // e.g., 4
        }
        
        if (auto ppq = position->getPpqPosition())
            currentPPQ = *ppq;  // Position in quarter notes
        
        if (auto isPlaying = position->getIsPlaying())
            hostIsPlaying = *isPlaying;
    }
}
```

### Sample/Beat Math (from archive/plug-n-script/looper.cxx)

```cpp
double quarterNotesToSamples(double position, double bpm)
{
    return position * 60.0 * sampleRate / bpm;
}

double samplesToQuarterNotes(double samples, double bpm)
{
    return samples * bpm / (60.0 * sampleRate);
}

// Samples per bar (4/4 time):
double samplesPerBar = quarterNotesToSamples(4.0, bpm);
```

---

## Implementation Phases

### Phase 1: Quantized Recording (MVP)

**Goal:** When recording ends, snap loop length to the nearest bar.

#### Changes Required

**1. PluginProcessor.h/cpp**
- Add method to pass transport info to LooperEngine
- Read `bpm` and `timeSignature` from `getPlayHead()->getPosition()`
- Call `looperEngine->setHostTempo(bpm, timeSig)` in `processBlock()`

**2. LooperEngine.h**
- Add sync-related members:
  ```cpp
  enum class SyncMode { Off, QuantizeEnd, QuantizeStartEnd };
  
  std::atomic<SyncMode> syncMode { SyncMode::Off };
  std::atomic<double> hostBPM { 120.0 };
  std::atomic<int> beatsPerBar { 4 };
  std::atomic<int> beatUnit { 4 };
  std::atomic<bool> hostIsPlaying { false };
  ```
- Add methods:
  ```cpp
  void setSyncMode(SyncMode mode);
  void setHostTempo(double bpm, int numerator, int denominator);
  void setHostPlaying(bool playing);
  ```

**3. LooperEngine.cpp - stopRecording()**
- When `syncMode != Off`:
  1. Calculate samples per bar: `samplesPerBar = (60.0 / bpm) * sampleRate * beatsPerBar`
  2. Quantize loop length to nearest bar: `quantizedLength = round(rawLength / samplesPerBar) * samplesPerBar`
  3. If quantized > raw: zero-fill the gap
  4. If quantized < raw: truncate (with crossfade)

**4. UI Changes**
- Add Sync toggle button (or dropdown: Off / Bars / Beats)
- Show tempo display (BPM from host)
- Visual indicator when sync is active

**5. Parameter**
- Add `sync` parameter to APVTS (choice: Off, Bars, Beats)
- Persist sync preference

#### Acceptance Criteria (Phase 1)
- [ ] Loop length snaps to bar boundaries when Sync is ON
- [ ] Works correctly at various tempos (60-180 BPM)
- [ ] Works with different time signatures (4/4, 3/4, 6/8)
- [ ] Zero-fills cleanly when extending to bar boundary
- [ ] Crossfades cleanly when truncating to bar boundary
- [ ] Sync mode persists across sessions
- [ ] Works in all tested hosts (GP, Logic, Reaper, Renoise, Standalone)

---

### Phase 2: Enhanced Sync Features (Future)

#### 2a: Pre-Roll / Count-In
- Wait for bar boundary before starting to record
- Visual countdown (beats until recording starts)

#### 2b: Transport Sync
- Start loop when host starts
- Stop loop when host stops
- Optional: reset loop position on host stop

#### 2c: Tempo Tracking (Advanced)
- Stretch/compress loop when tempo changes
- Requires real-time time-stretching (expensive)
- Could use JUCE's `TimeStretcher` or external library

---

## Task Breakdown (Phase 1)

### Sprint 1: Infrastructure (Est: 2-3 hours)
1. [ ] Add host tempo reading in PluginProcessor::processBlock()
2. [ ] Add `SyncMode` enum and atomics to LooperEngine.h
3. [ ] Add `setHostTempo()` and `setSyncMode()` methods
4. [ ] Wire up tempo reading → engine

### Sprint 2: Quantization Logic (Est: 3-4 hours)
1. [ ] Implement `calculateQuantizedLoopLength()`
2. [ ] Modify `stopRecording()` to use quantization when enabled
3. [ ] Handle zero-fill for extension
4. [ ] Handle crossfade for truncation
5. [ ] Unit test the quantization math

### Sprint 3: UI & Parameters (Est: 2-3 hours)
1. [ ] Add `sync` parameter to APVTS
2. [ ] Add Sync button to UI (similar to existing buttons)
3. [ ] Add tempo display (optional)
4. [ ] Verify parameter persistence

### Sprint 4: Testing & Polish (Est: 2-3 hours)
1. [ ] Test in Gig Performer
2. [ ] Test in Logic Pro
3. [ ] Test in Reaper
4. [ ] Test in Renoise (Linux user!)
5. [ ] Test in Standalone (no host tempo)
6. [ ] Edge cases: tempo changes during recording, very slow/fast tempos
7. [ ] Update README with sync feature docs

---

## Edge Cases to Handle

| Scenario | Behavior |
|----------|----------|
| No host tempo available | Sync disabled, fall back to free-form recording |
| Tempo changes during recording | Use tempo at recording END for quantization |
| Very short recording (&lt;1 bar) | Extend to 1 bar minimum |
| Very long recording (&gt;16 bars) | Quantize normally |
| Standalone app (no host) | Sync feature hidden or disabled |
| Host tempo = 0 | Treat as "no tempo available" |
| Time signature changes mid-recording | Use time sig at recording END |

---

## UI Design Options

### Option A: Simple Toggle
- Single "Sync" button next to other controls
- When ON: quantize to bars
- Matches hardware looper simplicity

### Option B: Dropdown
- Off / Bars / Beats
- More flexible, slightly more complex

### Option C: Settings Page (Future)
- Sync mode in a settings/preferences panel
- Keeps main UI clean

**Recommendation:** Start with **Option A** (simple toggle) for MVP, align with the Boomerang philosophy of simplicity.

---

## Files to Modify

| File | Changes |
|------|---------|
| `Source/LooperEngine.h` | Add SyncMode enum, tempo atomics, new methods |
| `Source/LooperEngine.cpp` | Implement quantization in stopRecording() |
| `Source/PluginProcessor.h` | (minimal) |
| `Source/PluginProcessor.cpp` | Read host tempo, pass to engine |
| `Source/PluginEditor.h` | Add sync button member |
| `Source/PluginEditor.cpp` | Draw sync button, handle click |
| `CMakeLists.txt` | (no changes expected) |

---

## Open Questions

1. **What happens if user records 1.5 bars?** → Round to nearest (2 bars) or truncate (1 bar)?
   - Recommendation: Round to nearest, with minimum of 1 bar

2. **Should quantization apply to overdubs?** → Probably not for MVP (overdub uses existing loop length)

3. **Should we show a "waiting for bar" indicator during pre-roll?** → Phase 2

4. **Tempo display on UI?** → Nice to have for debugging, but not essential for MVP

---

## References

- JUCE `AudioPlayHead` docs: https://docs.juce.com/master/classAudioPlayHead.html
- JUCE `PositionInfo` docs: https://docs.juce.com/master/structAudioPlayHead_1_1PositionInfo.html
- Original Plug'n'Script code: `archive/plug-n-script/looper.cxx` (lines 111-119)
- User feedback: Issue #20 comments

---

## Checklist Before Starting

- [ ] Read the brainstorming skill if needed for creative exploration
- [ ] Create feature branch: `git checkout -b feature/midi-sync`
- [ ] Review `AudioPlayHead` usage in JUCE examples
- [ ] Confirm approach with user before coding
