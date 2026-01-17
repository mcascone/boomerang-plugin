# Repository Guidelines

You are an expert software engineer with guru-like knowledge of C++ audio plugin development using JUCE.

This document outlines the structure, build process, coding style, testing, and contribution guidelines for the Boomerang audio looper plugin repository.

Read the README.md first for an overview of the project, including detailed behavioral requirements, then refer to this document for development-specific information.

**Also read `.session-log.md` for recent session progress and open items.**

Also review the CODE_REVIEW_FINDINGS.md for a summary and details on known issues in the codebase. The JUCE implementation is now on the main branch.

Refer to JUCE_TIPS_AND_TRICKS.md for JUCE-specific best practices, debugging techniques, and common pitfalls.

## Project Structure & Module Organization

- `Source/`: core C++ code (PluginProcessor, PluginEditor, LooperEngine).
- `build/`: generated build artifacts and Makefile.
- `JUCE/`: JUCE framework (git submodule).
- `archive/plug-n-script/`: archived legacy Plug n Script implementation.
- `Presets/`, `midi/`, `img/`: assets and examples.

## Build, Test, and Development Commands

- Full build: `./build.sh` (cleans, configures, and builds; installs via COPY_PLUGIN_AFTER_BUILD).
- Incremental build from build dir: `cd build && make -j8` (builds and auto-installs changed targets).
- After committing: `cd build && make -j8` (git hash updates automatically at build time; only PluginEditor.cpp recompiles).
- Note: Plugins auto-install to system directories when rebuilt via COPY_PLUGIN_AFTER_BUILD.
- Build installer: `./build-installer.sh` (creates macOS .pkg installer with VST3, AU, and Standalone).
- Tests: `pluginval --validate build/Boomerang_artefacts/VST3/Boomerang.vst3` (run VST3 compliance tests).
- Version is set in `CMakeLists.txt` via `VERSION` and `BOOMERANG_VERSION_SUFFIX` variables.

## Coding Style & Naming Conventions

- C++17 JUCE code; prefer straightforward JUCE patterns.
- Indent with 2 spaces; brace on same line for functions/methods.
- Use `snake_case` for locals/parameters, `CamelCase` for types, `PascalCase` for JUCE components.
- Avoid shadowing class members; keep signed/unsigned consistent for buffer indices.
- Add brief comments when intent isn’t obvious.
- Prefer simpler code over clever constructs. Easy-to-maintain code is better than complex optimizations.
- Re-use existing patterns in the codebase for ease of understanding and consistency. Re-use existing JUCE functionality where possible. Don't reinvent the wheel. Do not introduce new dependencies unless absolutely necessary.
- Follow DRY and YAGNI principles.

## Agent Best Practices

- check where you are before running location-dependent commands; or use full path when appropriate.
- check paths and filenames for typos and existence before running commands that depend on them.
- don't do that thing where you run a command and then immediately run another command that depends on the first command having completed successfully; instead, check the exit status of the first command before proceeding.
- When asked to "quit and restart GP" or similar, run: `osascript -e 'quit app "GigPerformer5"' && sleep 2 && open -a "GigPerformer5"`
- When asked to "restart standalone" or similar, run: `osascript -e 'quit app "Boomerang+"' && sleep 2 && open -a $BOOM_STANDALONE_PATH` (the env var is exported in my `.alias` file).

## Testing Guidelines

- No automated test suite; validate via plugin host or JUCE Standalone build (`Boomerang_artefacts/Standalone`).
- When reproducing issues, note loop length, direction, Once/Stack states, and host buffer size.

## Commit & Pull Request Guidelines

- Keep commits scoped and descriptive (e.g., `Fix reverse playhead wrap`).
- Reference GitHub issues when applicable.
- PRs: include a short summary, reproduction steps, expected/actual behavior, and screenshots/GIFs if UI-visible.
- Call out audio behavior changes and any new warnings/errors.

## Known Pitfalls (Agent Notes)

- Issue #36 (VST3 signing warnings) is benign for local builds; use proper signing for release.
- Issue #44 (volume controls entire signal) is a known bug being investigated.
- Issue #43 (REC LED when stacking) is an enhancement request.
- Current release: v2.0.0-beta-1 (JUCE port). Beta quality - core functionality stable, polishing phase. Builds shipped: VST3, AU (macOS), Standalone app (macOS); Windows/Linux use VST3. Installation paths: `~/Library/Audio/Plug-Ins/Components`, `~/Library/Audio/Plug-Ins/VST3`. Please file new issues for any problems.
- releases/ folder is git-ignored; use GitHub Releases for distribution instead.
- Release notes style preference: concise Markdown with top-level title like `v2.0.0-beta-1 (JUCE port)`, sections for What's new, Installation (macOS and Windows/Linux), Known issues, and How to help (link to new issue form). Tone should be direct and instructional.

## Aligned Operators and Symbols

I prefer code to be aligned like this:

```cpp
thruMuteButton.onClick = [this]() { audioProcessor.getLooperEngine()->onThruMuteButtonPressed(); };
recordButton.onClick   = [this]() { audioProcessor.getLooperEngine()->onRecordButtonPressed(); };
playButton.onClick     = [this]() { audioProcessor.getLooperEngine()->onPlayButtonPressed(); };
onceButton.onClick     = [this]() { audioProcessor.getLooperEngine()->onOnceButtonPressed(); };
reverseButton.onClick  = [this]() { audioProcessor.getLooperEngine()->onReverseButtonPressed(); };
```

Notice how the `=` signs are vertically aligned. This makes it easier to scan and compare similar lines of code.

Incorrect:

```cpp
thruMuteButton.onClick = [this]() { audioProcessor.getLooperEngine()->onThruMuteButtonPressed(); };
recordButton.onClick = [this]() { audioProcessor.getLooperEngine()->onRecordButtonPressed(); };
playButton.onClick = [this]() { audioProcessor.getLooperEngine()->onPlayButtonPressed(); };
onceButton.onClick = [this]() { audioProcessor.getLooperEngine()->onOnceButtonPressed(); };
reverseButton.onClick = [this]() { audioProcessor.getLooperEngine()->onReverseButtonPressed(); };
```
