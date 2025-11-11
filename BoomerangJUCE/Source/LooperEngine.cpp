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
    currentMode = LoopMode::Normal;
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
void LooperEngine::onRecordButtonPressed()
{
    auto& activeSlot = loopSlots[activeLoopSlot];

    switch (currentState)
    {
        case LooperState::Stopped:
            if (!activeSlot.hasContent)
            {
                // Start recording first loop
                startRecording();
            }
            else
            {
                // Start overdubbing on existing loop
                startOverdubbing();
            }
            break;

        case LooperState::Recording:
            // Stop recording, start playback
            stopRecording();
            startPlayback();
            break;

        case LooperState::Playing:
            // Start overdubbing
            startOverdubbing();
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
            // Stop recording and start playback
            stopRecording();
            startPlayback();
            break;

        case LooperState::Playing:
        case LooperState::Overdubbing:
            // Stop playback
            stopPlayback();
            break;
    }
}

void LooperEngine::onOnceButtonPressed()
{
    if (currentMode == LoopMode::Once)
        currentMode = LoopMode::Normal;
    else
        currentMode = LoopMode::Once;
}

void LooperEngine::onStackButtonPressed()
{
    if (currentMode == LoopMode::Stack)
        currentMode = LoopMode::Normal;
    else
        currentMode = LoopMode::Stack;
}

void LooperEngine::onReverseButtonPressed()
{
    if (currentMode == LoopMode::Reverse)
        currentMode = LoopMode::Normal;
    else
        currentMode = LoopMode::Reverse;
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
    }
}

void LooperEngine::stopOverdubbing()
{
    auto& activeSlot = loopSlots[activeLoopSlot];
    
    activeSlot.isRecording = false;
    currentState = LooperState::Playing;
}

//==============================================================================
void LooperEngine::processRecording(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    int numSamples = buffer.getNumSamples();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (slot.recordPosition >= maxLoopSamples)
            break;

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
            
            if (currentMode == LoopMode::Reverse)
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
            if (currentMode == LoopMode::Stack)
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
        
        if (currentMode == LoopMode::Once)
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