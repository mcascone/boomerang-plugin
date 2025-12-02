# Repository Guidelines

## Project Structure & Module Organization

- `BoomerangJUCE/Source/`: core C++ code (PluginProcessor, PluginEditor, LooperEngine).
- `BoomerangJUCE/build/`: generated build artifacts and Makefile (use this for `make`).
- `build/`: alternate/legacy CMake output; no Makefile by default.
- `Presets/`, `midi/`, `img/`: assets and examples.

## Build, Test, and Development Commands

- Configure/build from repo root: `cmake -S BoomerangJUCE -B build && cmake --build build`.
- From JUCE build dir: `cd BoomerangJUCE/build && make -j4` (reuses generated Makefile).
- Install VST3 locally (macOS): `cmake --build build --target install` (writes to `~/Library/Audio/Plug-Ins/VST3`).
- Tests: none defined yet; manual audio validation required.

## Coding Style & Naming Conventions

- C++17 JUCE code; prefer straightforward JUCE patterns.
- Indent with 4 spaces; brace on same line for functions/methods.
- Use `snake_case` for locals/parameters, `CamelCase` for types, `PascalCase` for JUCE components.
- Avoid shadowing class members; keep signed/unsigned consistent for buffer indices.
- Add brief comments only when intent isn’t obvious.

## Testing Guidelines

- No automated test suite; validate via plugin host or JUCE Standalone build (`Boomerang_artefacts/Standalone`).
- When reproducing issues, note loop length, direction, Once/Stack states, and host buffer size.

## Commit & Pull Request Guidelines

- Keep commits scoped and descriptive (e.g., `Fix reverse playhead wrap`).
- Reference GitHub issues (#32–#36) when applicable.
- PRs: include a short summary, reproduction steps, expected/actual behavior, and screenshots/GIFs if UI-visible.
- Call out audio behavior changes and any new warnings/errors.

## Known Pitfalls (Agent Notes)

- Reverse playback currently bounces mid-loop (issue #32).
- Once mode reliability in overdub/stack needs work (issue #33).
- Numerous compiler warnings about missing switch cases and sign conversions (issues #34–#35); cleanups welcome.
- VST3 signing warnings are benign for local builds; use proper signing for release (issue #36).
- Current release: v2.0.0-alpha-0 (JUCE port). Alpha quality; known issues above. Builds shipped: VST3, AU (macOS), Standalone app (macOS); Windows/Linux use VST3. Installation paths: `~/Library/Audio/Plug-Ins/Components`, `~/Library/Audio/Plug-Ins/VST3`. Please file new issues for any problems.
- Release notes style preference: concise Markdown with top-level title like `v2.0.0-alpha-0 (JUCE port)`, explicit “Alpha” warning, sections for What’s new, Installation (macOS and Windows/Linux), Known issues, and How to help (link to new issue form). Tone should be direct and instructional.
