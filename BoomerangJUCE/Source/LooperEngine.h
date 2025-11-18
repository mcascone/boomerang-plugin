#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

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
        Once,
        Reverse,
        Stack
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

    //==============================================================================
    LooperEngine();
    ~LooperEngine();

    //==============================================================================
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    //==============================================================================
    void processBlock(juce::AudioBuffer<float>& buffer);

    //==============================================================================
    // Button event handlers (called on press, not state)
    void onThruMuteButtonPressed();
    void onRecordButtonPressed();
    void onPlayButtonPressed();
    void onOnceButtonPressed();
    void onStackButtonPressed();
    void onReverseButtonPressed();

    //==============================================================================
    // Parameter setters
    void setVolume(float volume) { outputVolume = volume; }
    void setFeedback(float feedback) { feedbackAmount = feedback; }

    //==============================================================================
    // State queries for UI updates
    LooperState getState() const { return currentState; }
    LoopMode getMode() const { return loopMode; }
    DirectionMode getDirection() const { return currentDirection; }
    StackMode getStackMode() const { return stackMode; }
    OnceMode getOnceMode() const { return onceMode; }
    ThruMuteState getThruMuteState() const { return thruMuteState; }

    bool isRecording() const { return currentState == LooperState::Recording || currentState == LooperState::Overdubbing; }
    bool isPlaying() const { return currentState == LooperState::Playing || currentState == LooperState::Overdubbing; }
    float getLoopProgress() const;
    int getCurrentLoopSlot() const { return activeLoopSlot; }

private:
    //==============================================================================
    struct LoopSlot
    {
        juce::AudioBuffer<float> buffer;
        int length = 0;
        bool hasContent = false;
        bool isRecording = false;
        bool isPlaying = false;
        int playPosition = 0;
        int recordPosition = 0;
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
    ThruMuteState thruMuteState = ThruMuteState::Off;

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

    //==============================================================================
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void startOverdubbing();
    void stopOverdubbing();

    void processRecording(juce::AudioBuffer<float>& buffer, LoopSlot& slot);
    void processPlayback(juce::AudioBuffer<float>& buffer, LoopSlot& slot);
    void processOverdubbing(juce::AudioBuffer<float>& buffer, LoopSlot& slot);

    void applyCrossfade(juce::AudioBuffer<float>& buffer, LoopSlot& slot, int startSample, int numSamples);
    void switchToNextLoopSlot();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LooperEngine)
};