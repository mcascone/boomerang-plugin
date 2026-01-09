# v2.0.0-alpha-5 (JUCE port)

**⚠️ Alpha Release** - This is an early alpha release with known issues. Please test thoroughly and report any problems.

## What's New

### Host Parameter Notification
- **Bidirectional parameter sync**: Plugin state changes now properly notify the host DAW
- Perfect synchronization between plugin UI, MIDI controllers, and DAW automation panels (e.g., Gig Performer widgets)
- Thread-safe implementation using `beginChangeGesture()` / `endChangeGesture()` pattern
- Prevents circular notification loops with internal state flag

### New Output Parameters for External Indicators
- **`onceState`**: Exposes ONCE mode state for external LED control
- **`slowMode`**: Exposes SLOW mode state for external LED indicator  
- **`loopCycle`**: Pulses on each loop cycle for external REC blink indicator

### LED Overlay System
- Visual LED indicators on plugin UI showing button states
- Right-click context menu to toggle "Show Overlays" on/off
- Record LED flashes on loop cycle boundaries

### Build Improvements
- **CMakeLists.txt refactored**: Cleaner structure with section headers, DRY path variables, modern CMake patterns
- **Build-time git hash**: Only recompiles PluginEditor.cpp when hash changes (faster incremental builds)
- Warning for uncommitted changes during build

### Developer Experience
- Added VS Code Agent Skill for release workflow (`.github/skills/create-release/`)
- Cleaned up AGENTS.md documentation
- Improved dequarantine script (no sudo required)

## Installation

### macOS
Download and run `Boomerang-2.0.0-alpha-5.pkg` installer.

Installation locations:
- VST3 → `/Library/Audio/Plug-Ins/VST3/`
- AU → `/Library/Audio/Plug-Ins/Components/`
- Standalone → `/Applications/`

### Windows/Linux
VST3 builds for other platforms coming soon.

## Known Issues

- VST3 signing warnings (benign for local builds) (#36)
- Smoother mute switch needed (#30)
- Output level control could be smoother (#21)

## How to Help

Found a bug? Please [file an issue](https://github.com/mcascone/boomerang-plugin/issues/new) with:
- Steps to reproduce
- Expected vs actual behavior
- Loop length, direction, mode settings
- Host/DAW and buffer size

---

**Full Changelog**: https://github.com/mcascone/boomerang-plugin/compare/v2.0.0-alpha-4...v2.0.0-alpha-5
