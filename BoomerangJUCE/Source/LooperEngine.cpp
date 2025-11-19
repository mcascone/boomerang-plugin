#include "LooperEngine.h"

//==============================================================================
LooperEngine::LooperEngine()
{
}

LooperEngine::~LooperEngine()
{
}

//==============================================================================
void LooperEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    this->numChannels = numChannels;
    this->maxLoopSamples = static_cast<int>(sampleRate * maxLoopLengthSeconds);

    // Initialize all loop slots
    for (auto& slot : loopSlots)
    {
        slot.buffer.setSize(numChannels, maxLoopSamples);
        slot.buffer.clear();
        slot.length = 0;
        slot.hasContent = false;
        slot.isRecording = false;
        slot.isPlaying = false;
        slot.playPosition = 0;
        slot.recordPosition = 0;
    }

    reset();
}

void LooperEngine::reset()
{
    currentState = LooperState::Stopped;
    loopMode = LoopMode::Normal;
    currentDirection = DirectionMode::Forward;
    stackMode = StackMode::Off;
    onceMode = OnceMode::Off;
    thruMute = ThruMuteState::Off;
    activeLoopSlot = 0;
    waitingForFirstLoop = true;
    recordingStartDelay = 0;

    for (auto& slot : loopSlots)
    {
        slot.buffer.clear();
        slot.length = 0;
        slot.hasContent = false;
        slot.isRecording = false;
        slot.isPlaying = false;
        slot.playPosition = 0;
        slot.recordPosition = 0;
    }
}

//==============================================================================
void LooperEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    auto& activeSlot = loopSlots[activeLoopSlot];

    switch (currentState)
    {
        case LooperState::Stopped:
            // Pass-through audio when stopped
            break;

        case LooperState::Recording:
            processRecording(buffer, activeSlot);
            break;

        case LooperState::Playing:
            processPlayback(buffer, activeSlot);
            break;

        case LooperState::Overdubbing:
            processOverdubbing(buffer, activeSlot);
            break;
    }

    // Apply output volume
    buffer.applyGain(outputVolume);
}

//==============================================================================
void LooperEngine::onThruMuteButtonPressed()
{
    // Toggle thru/mute state
    // implementation pending
    // in ThruMute mode, input is recorded but not passed through. Only the recorded sound is played back.
    toggleThruMute();
}
//==============================================================================
void LooperEngine::onRecordButtonPressed()
{
    auto& activeSlot = loopSlots[activeLoopSlot];

    switch (currentState)
    {
        case LooperState::Stopped:
            // Start recording first loop
            startRecording();
            break;

        case LooperState::Recording:
            // Stop recording, start playback
            stopRecording();
            startPlayback();
            break;

        case LooperState::Playing:
            // Stop playing and start new recording
            stopPlayback();
            startRecording();
            break;

        case LooperState::Overdubbing:
            // Stop overdubbing, continue playing
            stopOverdubbing();
            break;
    }
}

void LooperEngine::onPlayButtonPressed()
{
    auto& activeSlot = loopSlots[activeLoopSlot];

    switch (currentState)
    {
        case LooperState::Stopped:
            if (activeSlot.hasContent)
            {
                startPlayback();
            }
            break;

        case LooperState::Recording:
            // Stop recording, go idle
            stopRecording();
            break;

        case LooperState::Playing:
        case LooperState::Overdubbing:
            // Stop playback
            stopPlayback();
            break;

        case LooperState::ContinuousReverse:
            // go idle
            stopPlayback();
            break;
    }
}

void LooperEngine::onOnceButtonPressed()
{
    setOnceMode(OnceMode::On);

    if (currentState == LooperState::Playing)
    {
        // Once when playing restarts at the beginning of the loop
        // If currently playing and Once mode is activated, ensure playback stops at end of loop
        // This is handled in processPlayback
        stopPlayback();
        startPlayback();        
    }
    else if (currentState == LooperState::Stopped)
    {
        startPlayback();
    }
}

void LooperEngine::onStackButtonPressed()
{
    stackMode = (stackMode == StackMode::Off) ? StackMode::On : StackMode::Off;

    if (currentState == LooperState::Playing)
    {
        startOverdubbing();
    }
    // else if (currentState == LooperState::Stopped)
    // {
    //     toggleSpeed();
    // }
}

void LooperEngine::onReverseButtonPressed()
{
    currentDirection = (currentDirection == DirectionMode::Forward) ? DirectionMode::Reverse : DirectionMode::Forward;
}

//==============================================================================
void LooperEngine::startRecording()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    activeSlot.isRecording = true;
    activeSlot.recordPosition = 0;
    currentState = LooperState::Recording;
    waitingForFirstLoop = true;
}

void LooperEngine::stopRecording()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    activeSlot.isRecording = false;
    activeSlot.length = activeSlot.recordPosition;
    activeSlot.hasContent = (activeSlot.length > 0);
    currentState = LooperState::Stopped;
    
    if (waitingForFirstLoop)
        waitingForFirstLoop = false;
}

void LooperEngine::startPlayback()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    if (activeSlot.hasContent)
    {
        activeSlot.isPlaying = true;
        activeSlot.playPosition = 0;
        currentState = LooperState::Playing;
    }
}

void LooperEngine::stopPlayback()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    activeSlot.isPlaying = false;
    currentState = LooperState::Stopped;
}

void LooperEngine::startOverdubbing()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    if (activeSlot.hasContent)
    {
        activeSlot.isRecording = true;
        activeSlot.isPlaying = true;
        currentState = LooperState::Overdubbing;
        stackMode = StackMode::On;
    }
}

void LooperEngine::stopOverdubbing()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    activeSlot.isRecording = false;
    currentState = LooperState::Playing;
    stackMode = StackMode::Off;
}

void LooperEngine::toggleThruMute()
// do we even need enable/disable functions separately?
{
    thruMute = (thruMute == ThruMuteState::Off) ? ThruMuteState::On : ThruMuteState::Off;
}

void LooperEngine::toggleDirection()
{
    currentDirection = (currentDirection == DirectionMode::Forward) ? DirectionMode::Reverse : DirectionMode::Forward;
}

void LooperEngine::toggleOnceMode()
{
    onceMode = (onceMode == OnceMode::Off) ? OnceMode::On : OnceMode::Off;
}

void LooperEngine::setOnceMode(OnceMode mode)
{
    onceMode = mode;
}

void LooperEngine::toggleStackMode()
{
    stackMode = (stackMode == StackMode::Off) ? StackMode::On : StackMode::Off;
}

void LooperEngine::setStackMode(StackMode mode)
{
    stackMode = mode;
}

//==============================================================================
void LooperEngine::processRecording(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    int numSamples = buffer.getNumSamples();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (slot.recordPosition >= maxLoopSamples)
        {
            currentState = LooperState::BufferFilled;
            stopRecording();
            break; // break exits the for loop
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            float inputSample = buffer.getSample(channel, sample);
            slot.buffer.setSample(channel, slot.recordPosition, inputSample);
        }
        
        slot.recordPosition++;
    }
}

void LooperEngine::processPlayback(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    if (!slot.hasContent || slot.length == 0)
    {
        buffer.clear();
        return;
    }

    int numSamples = buffer.getNumSamples();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float loopSample;
            
            if (loopMode == LoopMode::Reverse)
            {
                // Reverse playback
                int reversePos = slot.length - 1 - slot.playPosition;
                loopSample = slot.buffer.getSample(channel, reversePos);
            }
            else
            {
                loopSample = slot.buffer.getSample(channel, slot.playPosition);
            }
            
            // Mix with input or replace based on mode
            if (loopMode == LoopMode::Stack)
            {
                // Stack mode: mix loop with input
                float inputSample = buffer.getSample(channel, sample);
                buffer.setSample(channel, sample, inputSample + loopSample);
            }
            else
            {
                buffer.setSample(channel, sample, loopSample);
            }
        }
        
        // Advance playback position
        slot.playPosition++;
        
        if (loopMode == LoopMode::Once)
        {
            // Once mode: stop at end
            if (slot.playPosition >= slot.length)
            {
                stopPlayback();
                break;
            }
        }
        else
        {
            // Normal loop: wrap around
            if (slot.playPosition >= slot.length)
                slot.playPosition = 0;
        }
    }
}

void LooperEngine::processOverdubbing(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    if (!slot.hasContent || slot.length == 0)
    {
        processRecording(buffer, slot);
        return;
    }

    int numSamples = buffer.getNumSamples();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float inputSample = buffer.getSample(channel, sample);
            float loopSample = slot.buffer.getSample(channel, slot.playPosition);
            
            // Overdub: mix input with existing content
            float overdubSample = loopSample + (inputSample * feedbackAmount);
            slot.buffer.setSample(channel, slot.playPosition, overdubSample);
            
            // Output mixed signal
            buffer.setSample(channel, sample, overdubSample);
        }
        
        // Advance position
        slot.playPosition++;
        if (slot.playPosition >= slot.length)
            slot.playPosition = 0;
    }
}

void LooperEngine::applyCrossfade(juce::AudioBuffer<float>& buffer, LoopSlot& slot, int startSample, int numSamples)
{
    // Implement crossfading for smooth loop transitions
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float fadeGain = static_cast<float>(sample) / static_cast<float>(numSamples);
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float currentSample = buffer.getSample(channel, startSample + sample);
            buffer.setSample(channel, startSample + sample, currentSample * fadeGain);
        }
    }
}

void LooperEngine::switchToNextLoopSlot()
{
    activeLoopSlot = (activeLoopSlot + 1) % maxLoopSlots;
}

float LooperEngine::getLoopProgress() const
{
    const auto& activeSlot = loopSlots[activeLoopSlot];
    
    if (!activeSlot.hasContent || activeSlot.length == 0)
        return 0.0f;
    
    return static_cast<float>(activeSlot.playPosition) / static_cast<float>(activeSlot.length);
}