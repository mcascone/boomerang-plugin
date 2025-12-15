#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

//==============================================================================
/**
    Core looper engine implementing the Boomerang+ functionality
    
    Features:
    - Circular buffer recording/playback
    - Multiple loop slots
    - Overdubbing with crossfading
    - Reverse playback
    - Stack mode
    - Once mode
    - Proper momentary button handling
*/
class LooperEngine
{
public:
    //==============================================================================
    enum class LooperState
    {
        Stopped,
        Recording,
        Playing,
        Overdubbing,
        ContinuousReverse,
        BufferFilled
    };

    enum class LoopMode
    {
        Normal,
        Reverse,
    };

    enum class DirectionMode
    {
        Forward,
        Reverse
    };

    enum class StackMode
    {
        Off,
        On
    };

    enum class OnceMode
    {
        Off,
        On
    };

    enum class ThruMuteState
    {
        Off,
        On
    };

    enum class SpeedMode
    {
        Normal,
        Half
    };

    //==============================================================================
    LooperEngine();
    ~LooperEngine();

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    //==============================================================================
    void processBlock(juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Button event handlers
    void onThruMuteButtonPressed();
    void onRecordButtonPressed();
    void onPlayButtonPressed();
    void onOnceButtonPressed();
    void onStackButtonPressed();      // Momentary: called when pressed
    void onStackButtonReleased();     // Momentary: called when released
    void onReverseButtonPressed();

    //==============================================================================
    // Parameter setters
    void setVolume(float volume) { outputVolume = volume; }
    void setFeedback(float feedback) { feedbackAmount = feedback; }

    //==============================================================================
    // State queries for UI updates
    LooperState getState() const { return currentState; }
    LoopMode getLoopMode() const { return loopMode; }
    DirectionMode getDirection() const { return currentDirection; }
    StackMode getStackMode() const { return stackMode; }
    OnceMode getOnceMode() const { return onceMode; }
    ThruMuteState getThruMuteState() const { return thruMute; }
    SpeedMode getSpeedMode() const { return speedMode; }

    bool isRecording() const { return currentState == LooperState::Recording || currentState == LooperState::Overdubbing; }
    bool isPlaying() const { return currentState == LooperState::Playing || currentState == LooperState::Overdubbing; }
    float getLoopProgress() const;
    int getCurrentLoopSlot() const { return activeLoopSlot; }
    
    // Check and clear loop wrap flag for UI flash indicator
    bool checkAndClearLoopWrapped() 
    { 
        return loopWrapped.exchange(false); 
    }

private:
    //==============================================================================
    struct LoopSlot
    {
        juce::AudioBuffer<float> buffer;
        int length = 0;
        bool hasContent = false;
        bool isRecording = false;
        bool isPlaying = false;
        float playPosition = 0.0f;
        float recordPosition = 0.0f;
        float fadeInGain = 1.0f;
        float fadeOutGain = 1.0f;
    };

    //==============================================================================
    static constexpr int maxLoopSlots = 4;
    static constexpr int maxLoopLengthSeconds = 240; // 4 minutes
    static constexpr int crossfadeSamples = 1024;

    std::array<LoopSlot, maxLoopSlots> loopSlots;
    int activeLoopSlot = 0;

    LooperState currentState = LooperState::Stopped;
    LoopMode loopMode = LoopMode::Normal;
    DirectionMode currentDirection = DirectionMode::Forward;
    StackMode stackMode = StackMode::Off;
    OnceMode onceMode = OnceMode::Off;
    ThruMuteState thruMute = ThruMuteState::Off;
    SpeedMode speedMode = SpeedMode::Normal;

    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    int numChannels = 2;
    int maxLoopSamples = 0;

    // Audio processing parameters
    float outputVolume = 1.0f;
    float feedbackAmount = 0.5f;

    // Timing and synchronization
    bool waitingForFirstLoop = true;
    int recordingStartDelay = 0;
    std::atomic<bool> loopWrapped{false};  // Set when loop cycles to position 0

    //==============================================================================
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void startOverdubbing();
    void stopOverdubbing();
    void toggleThruMute();
    void toggleDirection();
    void toggleOnceMode();
    void setOnceMode(OnceMode mode);
    void toggleStackMode();
    void setStackMode(StackMode mode);
    void toggleSpeedMode();
    void setSpeedMode(SpeedMode mode);
    
    // Preserved toggle behavior for future use
    void onStackButtonToggled();

    void processRecording(juce::AudioBuffer<float>& buffer, LoopSlot& slot);
    void processPlayback(juce::AudioBuffer<float>& buffer, LoopSlot& slot);
    void processOverdubbing(juce::AudioBuffer<float>& buffer, LoopSlot& slot);

    bool advancePosition(float& position, int length, float speed);
    void applyCrossfade(juce::AudioBuffer<float>& buffer, LoopSlot& slot, int startSample, int numSamples);
    void switchToNextLoopSlot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperEngine)
};
