#include "LooperEngine.h"

//==============================================================================
LooperEngine::LooperEngine()
{
}

LooperEngine::~LooperEngine()
{
}

//==============================================================================
void LooperEngine::prepare(double newSampleRate, int newSamplesPerBlock, int newNumChannels)
{
    sampleRate = newSampleRate;
    samplesPerBlock = newSamplesPerBlock;
    numChannels = newNumChannels;
    maxLoopSamples = static_cast<int>(sampleRate * maxLoopLengthSeconds);

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
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];

    // Thread-safe state access (issue #38)
    auto state = currentState.load();
    
    switch (state)
    {
        case LooperState::Stopped:
            // When thru mute is on, mute the input passthrough
            if (thruMute.load() == ThruMuteState::On)
                buffer.clear();
            // Otherwise pass-through audio when stopped
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

        case LooperState::ContinuousReverse:
        case LooperState::BufferFilled:
            // No processing defined yet
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
    auto state = currentState.load();
    
    switch (state)
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

        case LooperState::ContinuousReverse:
        case LooperState::BufferFilled:
            break;
    }
}

void LooperEngine::onPlayButtonPressed()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    auto state = currentState.load();

    switch (state)
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

        case LooperState::BufferFilled:
            break;
    }
}

void LooperEngine::onOnceButtonPressed()
{
    auto state = currentState.load();
    auto once = onceMode.load();
    
    if (state == LooperState::Playing)
    {
        if (once == OnceMode::Off)
        {
            // The first time Once is pressed while playing, enable Once mode.
            // Playback continues until the end of the loop.
            // Another Once press will restart the loop.
            toggleOnceMode();
        }
        else
        {
            // Once when playing in once mode restarts at the beginning of the loop
            stopPlayback();
            startPlayback();        
        }
    }
    else if (state == LooperState::Stopped || state == LooperState::Recording)
    {
        if (state == LooperState::Recording)
        {
            stopRecording();
        }

        // If stopped or recording, play once
        setOnceMode(OnceMode::On);
        startPlayback();
    }
}

// Toggle behavior - preserved for future use
void LooperEngine::onStackButtonToggled()
{
    switch (currentState)
    {
        case LooperState::Playing:
            startOverdubbing();
            break;
        
        case LooperState::Overdubbing:
            stopOverdubbing();
            break;
        
        case LooperState::Stopped:
            toggleSpeedMode();
            break;

        case LooperState::Recording:
        case LooperState::ContinuousReverse:
        case LooperState::BufferFilled:
            break;
    }
}

// Momentary behavior - engaged while pressed
void LooperEngine::onStackButtonPressed()
{
    auto state = currentState.load();
    
    if (state == LooperState::Playing)
    {
        startOverdubbing();
    }
    else if (state == LooperState::Stopped)
    {
        toggleSpeedMode();
    }
}

void LooperEngine::onStackButtonReleased()
{
    auto state = currentState.load();
    
    if (state == LooperState::Overdubbing)
    {
        stopOverdubbing();
    }
}

void LooperEngine::onReverseButtonPressed()
{
    toggleDirection();
}

//==============================================================================
void LooperEngine::startRecording()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    activeSlot.isRecording = true;
    activeSlot.recordPosition = 0;
    currentState.store(LooperState::Recording);
    waitingForFirstLoop = true;
}

void LooperEngine::stopRecording()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    activeSlot.isRecording = false;
    activeSlot.length = static_cast<int>(activeSlot.recordPosition);
    activeSlot.hasContent = (activeSlot.length > 0);
    currentState.store(LooperState::Stopped);
    
    if (waitingForFirstLoop)
        waitingForFirstLoop = false;
}

void LooperEngine::startPlayback()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    if (activeSlot.hasContent)
    {
        activeSlot.isPlaying = true;
        activeSlot.playPosition = (loopMode.load() == LoopMode::Reverse)
            ? static_cast<float>(activeSlot.length - 1)
            : 0.0f;
        currentState.store(LooperState::Playing);
    }
}

void LooperEngine::stopPlayback()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    activeSlot.isPlaying = false;
    currentState.store(LooperState::Stopped);
}

void LooperEngine::startOverdubbing()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    if (activeSlot.hasContent)
    {
        activeSlot.isRecording = true;
        activeSlot.isPlaying = true;
        currentState.store(LooperState::Overdubbing);
        stackMode.store(StackMode::On);
    }
}

void LooperEngine::stopOverdubbing()
{
    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    
    activeSlot.isRecording = false;
    currentState.store(LooperState::Playing);
    stackMode.store(StackMode::Off);
}

void LooperEngine::toggleThruMute()
// do we even need enable/disable functions separately?
{
    auto current = thruMute.load();
    thruMute.store((current == ThruMuteState::Off) ? ThruMuteState::On : ThruMuteState::Off);
}

void LooperEngine::toggleDirection()
{
    auto currentDir = currentDirection.load();
    auto currentLoop = loopMode.load();
    
    currentDirection.store((currentDir == DirectionMode::Forward) ? DirectionMode::Reverse : DirectionMode::Forward);
    loopMode.store((currentLoop == LoopMode::Normal) ? LoopMode::Reverse : LoopMode::Normal);

    auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot.load())];
    // Ensure playhead stays within bounds after direction change
    if (activeSlot.playPosition < 0.0f && activeSlot.length > 0)
        activeSlot.playPosition += static_cast<float>(activeSlot.length);
    if (activeSlot.playPosition >= static_cast<float>(activeSlot.length) && activeSlot.length > 0)
        activeSlot.playPosition = std::fmod(activeSlot.playPosition, static_cast<float>(activeSlot.length));
}

void LooperEngine::toggleOnceMode()
{
    auto current = onceMode.load();
    onceMode.store((current == OnceMode::Off) ? OnceMode::On : OnceMode::Off);
}

void LooperEngine::setOnceMode(OnceMode mode)
{
    onceMode.store(mode);
}

void LooperEngine::toggleStackMode()
{
    auto current = stackMode.load();
    stackMode.store((current == StackMode::Off) ? StackMode::On : StackMode::Off);
}

void LooperEngine::setStackMode(StackMode mode)
{
    stackMode.store(mode);
}

void LooperEngine::toggleSpeedMode()
{
    auto current = speedMode.load();
    speedMode.store((current == SpeedMode::Normal) ? SpeedMode::Half : SpeedMode::Normal);
}

void LooperEngine::setSpeedMode(SpeedMode mode)
{
    speedMode.store(mode);
}

//==============================================================================
void LooperEngine::processRecording(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    int numSamples = buffer.getNumSamples();
    const int inputChannels = buffer.getNumChannels();
    float speed = (speedMode.load() == SpeedMode::Half) ? 0.5f : 1.0f;
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        int writePos = static_cast<int>(slot.recordPosition);
        
        if (writePos >= maxLoopSamples)
        {
            // Buffer filled - stop recording (state write removed, handled elsewhere)
            stopRecording();
            break;
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            // If we only have a mono input, mirror it across all loop channels
            const int sourceChannel = (inputChannels > 0) ? juce::jmin(channel, inputChannels - 1) : 0;
            const float inputSample = (inputChannels > 0) ? buffer.getSample(sourceChannel, sample) : 0.0f;
            slot.buffer.setSample(channel, writePos, inputSample);
        }
        
        slot.recordPosition += speed;
    }
    
    // When thru mute is on, mute the input passthrough while recording
    if (thruMute.load() == ThruMuteState::On)
        buffer.clear();
}

void LooperEngine::processPlayback(juce::AudioBuffer<float>& buffer, LoopSlot& slot)
{
    if (!slot.hasContent || slot.length == 0)
    {
        buffer.clear();
        return;
    }

    // If stack mode is active, handle the whole buffer in the overdub path to
    // avoid re-processing the buffer per channel/sample.
    if (stackMode.load() == StackMode::On)
    {
        processOverdubbing(buffer, slot);
        return;
    }

    int numSamples = buffer.getNumSamples();
    float speed = (speedMode.load() == SpeedMode::Half) ? 0.5f : 1.0f;
    auto loopDirection = loopMode.load();
    auto thruMuteState = thruMute.load();
    auto once = onceMode.load();
    
    for (int sampleNum = 0; sampleNum < numSamples; ++sampleNum)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            int pos = static_cast<int>(slot.playPosition);
            float frac = slot.playPosition - pos;
            
            float loopSample;
            
            if (loopDirection == LoopMode::Reverse)
            {
                // Read directly at the decreasing playhead position
                int prevPos = (pos > 0) ? pos - 1 : (slot.length - 1);
                float sample1 = slot.buffer.getSample(channel, pos);
                float sample2 = slot.buffer.getSample(channel, prevPos);
                loopSample = sample1 + frac * (sample2 - sample1);
            }
            else
            {
                int nextPos = (pos + 1) % slot.length;
                float sample1 = slot.buffer.getSample(channel, pos);
                float sample2 = slot.buffer.getSample(channel, nextPos);
                loopSample = sample1 + frac * (sample2 - sample1);
            }
            
            // Thru mute handling: when ON, only play loop; when OFF, mix input with loop
            if (thruMuteState == ThruMuteState::On)
            {
                buffer.setSample(channel, sampleNum, loopSample);
            }
            else
            {
                float inputSample = buffer.getSample(channel, sampleNum);
                buffer.setSample(channel, sampleNum, loopSample + inputSample);
            }
        }
        
        bool wrapped = advancePosition(slot.playPosition, slot.length, speed);
        
        if (wrapped)
            loopWrapped.store(true);
        
        // Once mode: request stop via flag instead of direct state write (issue #38)
        if (once == OnceMode::On && wrapped)
        {
            stopPlayback();
            shouldDisableOnce.store(true);  // UI timer will call toggleOnceMode()
            break;
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
    const int inputChannels = buffer.getNumChannels();
    float speed = (speedMode.load() == SpeedMode::Half) ? 0.5f : 1.0f;
    auto thruMuteState = thruMute.load();
    auto once = onceMode.load();
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        int pos = static_cast<int>(slot.playPosition);
        
        for (int channel = 0; channel < numChannels; ++channel)
        {
            // Mirror mono input across all loop channels when needed
            const int sourceChannel = (inputChannels > 0) ? juce::jmin(channel, inputChannels - 1) : 0;
            float inputSample = (inputChannels > 0) ? buffer.getSample(sourceChannel, sample) : 0.0f;
            float loopSample = slot.buffer.getSample(channel, pos);
            
            // Overdub: mix input with existing content
            // Attenuate existing loop by 2.5dB to prevent overloading when stacking
            constexpr float stackAttenuation = 0.74989420933f; // -2.5dB
            float attenuatedLoop = loopSample * stackAttenuation;
            float overdubSample = attenuatedLoop + (inputSample * feedbackAmount);
            slot.buffer.setSample(channel, pos, overdubSample);
            
            // Thru mute handling: when ON, only output loop; when OFF, mix with input
            if (thruMuteState == ThruMuteState::On)
            {
                buffer.setSample(channel, sample, overdubSample);
            }
            else
            {
                buffer.setSample(channel, sample, overdubSample + inputSample);
            }
        }
        
        bool wrapped = advancePosition(slot.playPosition, slot.length, speed);

        if (wrapped)
            loopWrapped.store(true);

        // Once mode: request stop via flag instead of direct state write (issue #38)
        if (once == OnceMode::On && wrapped)
        {
            stopPlayback();
            shouldDisableOnce.store(true);  // UI timer will call toggleOnceMode()
            break;
        }
    }
}

bool LooperEngine::advancePosition(float& position, int length, float speed)
{
    bool wrapped = false;
    auto loopDirection = loopMode.load();

    if (loopDirection == LoopMode::Reverse)
    {
        position -= speed;
        // Wrap to end when going below 0
        if (position < 0)
        {
            position += length;
            wrapped = true;
        }
    }
    else
    {
        position += speed;
        // Wrap to beginning when going past end
        if (position >= length)
        {
            position = 0.0f;
            wrapped = true;
        }
    }

    return wrapped;
}

void LooperEngine::applyCrossfade(juce::AudioBuffer<float>& buffer, LoopSlot& slot, int startSample, int numSamples)
{
    juce::ignoreUnused(slot);
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
    const auto& activeSlot = loopSlots[static_cast<size_t>(activeLoopSlot)];
    
    if (!activeSlot.hasContent || activeSlot.length == 0)
        return 0.0f;
    
    return activeSlot.playPosition / static_cast<float>(activeSlot.length);
}
