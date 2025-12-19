# Boomerang+ JUCE Plugin

A professional looper plugin implemented in JUCE, providing all the functionality of the Boomerang+ looper pedal with perfect momentary button behavior and commercial-grade features.

## Features

### Core Looping
- **Multi-slot recording**: 4 independent loop slots
- **Seamless overdubbing**: Layer sounds with adjustable feedback
- **Perfect momentary buttons**: True momentary behavior with UI synchronization
- **Professional audio quality**: 32-bit floating point processing

### Advanced Modes
- **Stack Mode**: Layer input with loop playback
- **Reverse Mode**: Backward loop playback
- **Once Mode**: Single playback, no looping
- **Volume/Feedback controls**: Real-time parameter adjustment

### Technical Advantages over Blue Cat
- ✅ **Real plugin formats**: VST3, AU, AAX for commercial distribution
- ✅ **Perfect UI sync**: ButtonParameterAttachment for bidirectional control
- ✅ **No parameter limitations**: Full programmatic control
- ✅ **Professional performance**: Compiled C++ vs interpreted scripts
- ✅ **Automation support**: Full DAW automation compatibility
- ✅ **Commercial licensing**: Proper licensing for product sales

## Building

### Requirements
- CMake 3.22+
- Modern C++ compiler (Xcode on macOS)
- JUCE framework (automatically downloaded)

### Setup
```bash
chmod +x setup.sh
./setup.sh
```

### Build
```bash
cd build
cmake ..
make
```

The plugin will be automatically installed to the system plugin directories.

## Usage

### Recording Your First Loop
1. Press **RECORD** to start recording
2. Press **RECORD** again to stop recording and start playback
3. Press **PLAY** to stop playback

### Overdubbing
1. While a loop is playing, press **RECORD** to start overdubbing
2. Press **RECORD** again to stop overdubbing and continue playing

### Advanced Features
- **ONCE**: Toggle once-mode (plays loop once then stops)
- **STACK**: Toggle stack-mode (mixes input with loop)
- **REVERSE**: Toggle reverse playback
- **Volume**: Adjust output level
- **Feedback**: Control overdub mix amount

## Implementation Details

### Momentary Button Logic
Unlike the Blue Cat implementation which had UI sync issues, this JUCE version uses `ButtonParameterAttachment` for perfect bidirectional synchronization:

```cpp
// Button press detection
if (currentRecordState && !previousRecordState)
    looperEngine->onRecordButtonPressed();

// UI state update (the JUCE advantage!)
recordButton->setValueNotifyingHost(looperEngine->isRecording() ? 1.0f : 0.0f);
```

### Audio Processing
The core audio engine processes in real-time with:
- Circular buffer management
- Crossfading for smooth loops
- Multiple loop slots
- Professional overdubbing

### Parameter Management
All parameters support full automation and preset recall:
- Momentary buttons: `AudioParameterBool`
- Continuous controls: `AudioParameterFloat`
- Perfect state synchronization
- Undo/redo support

## Commercial Deployment

This JUCE implementation provides everything needed for commercial distribution:
- Professional plugin formats
- Proper licensing framework
- Copy protection support
- Installer generation
- Code signing compatibility

## Architecture

```
BoomerangAudioProcessor
├── Parameter management and button detection
├── LooperEngine
│   ├── Multi-slot audio buffers
│   ├── State management
│   └── Audio processing
└── BoomerangAudioProcessorEditor
    ├── ButtonParameterAttachment (perfect sync!)
    ├── Real-time status display
    └── Professional UI design
```

## Migration from Blue Cat

The JUCE version solves all Blue Cat limitations:

| Blue Cat Issue | JUCE Solution |
|---|---|
| Read-only parameters | Full bidirectional control |
| UI sync problems | ButtonParameterAttachment |
| Script limitations | Compiled C++ performance |
| No automation | Full automation support |
| Limited distribution | Professional plugin formats |

## License

Commercial plugin ready for distribution. See licensing terms for details.