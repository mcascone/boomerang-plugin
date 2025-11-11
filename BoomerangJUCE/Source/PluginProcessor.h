#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "LooperEngine.h"

//==============================================================================
/**
    Boomerang+ Looper Plugin Processor
    
    Professional looper with momentary button controls, multiple loop slots,
    overdubbing, reverse, and stack modes.
*/
class BoomerangAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    BoomerangAudioProcessor();
    ~BoomerangAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter access for UI
    juce::AudioParameterBool* getRecordButton() const { return recordButton; }
    juce::AudioParameterBool* getPlayButton() const { return playButton; }
    juce::AudioParameterBool* getOnceButton() const { return onceButton; }
    juce::AudioParameterBool* getStackButton() const { return stackButton; }
    juce::AudioParameterBool* getReverseButton() const { return reverseButton; }
    juce::AudioParameterFloat* getVolumeParam() const { return volumeParam; }
    juce::AudioParameterFloat* getFeedbackParam() const { return feedbackParam; }
    
    // Looper engine access for UI
    LooperEngine* getLooperEngine() const { return looperEngine.get(); }

private:
    //==============================================================================
    // Core looper engine
    std::unique_ptr<LooperEngine> looperEngine;

    // Parameters - momentary buttons
    juce::AudioParameterBool* recordButton;
    juce::AudioParameterBool* playButton;
    juce::AudioParameterBool* onceButton;
    juce::AudioParameterBool* stackButton;
    juce::AudioParameterBool* reverseButton;

    // Parameters - continuous controls
    juce::AudioParameterFloat* volumeParam;
    juce::AudioParameterFloat* feedbackParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoomerangAudioProcessor)
};