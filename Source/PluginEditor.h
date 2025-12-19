#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    Boomerang+ Plugin Editor with perfect parameter synchronization
    
    Features ButtonParameterAttachment for momentary button behavior
    that automatically syncs with the processor state.
*/
class BoomerangAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    BoomerangAudioProcessorEditor (BoomerangAudioProcessor&);
    ~BoomerangAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    //==============================================================================
    BoomerangAudioProcessor& audioProcessor;

    // UI Components
    juce::TextButton thruMuteButton;
    juce::TextButton recordButton;
    juce::TextButton playButton;
    juce::TextButton onceButton;
    juce::TextButton stackButton;
    juce::TextButton reverseButton;
    
    juce::Slider volumeSlider;
    juce::Slider feedbackSlider;
    
    juce::Label volumeLabel;
    juce::Label feedbackLabel;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label versionLabel;

    // Progress indicator
    juce::ProgressBar progressBar;
    double progressValue = 0.0;

    // Parameter Attachments - Only for continuous controls (using APVTS)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;

    // Visual state
    juce::Colour recordColour = juce::Colours::red;
    juce::Colour playColour = juce::Colours::green;
    juce::Colour onceColour = juce::Colours::blue;
    juce::Colour stackColour = juce::Colours::orange;
    juce::Colour reverseColour = juce::Colours::purple;
    juce::Colour thruMuteColour = juce::Colours::grey;
    juce::Colour thruMuteColour_OFF = juce::Colours::lightgrey;
    
    int recordFlashCounter = 0;  // For loop wrap flash indicator

    void setupButton(juce::TextButton& button, const juce::String& text, juce::Colour colour, bool isToggle = false);
    void updateStatusDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomerangAudioProcessorEditor)
};