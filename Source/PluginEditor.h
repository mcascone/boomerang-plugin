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
    void mouseDown(const juce::MouseEvent& event) override;

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
    
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label versionLabel;

    // Progress indicator
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    
    // Background image
    juce::Image backgroundImage;
    
    // LED states
    bool recordLED  = false;
    bool playLED    = false;
    bool onceLED    = false;
    bool reverseLED = false;
    bool stackLED   = false;
    bool slowLED    = false;

    // Parameter Attachments - only for continuous controls
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;

    // Visual state
    bool showButtonOverlays = false;  // Toggle for button hover/state overlays
    
    juce::Colour recordColour = juce::Colours::red;
    juce::Colour playColour = juce::Colours::green;
    juce::Colour onceColour = juce::Colours::blue;
    juce::Colour stackColour = juce::Colours::orange;
    juce::Colour reverseColour = juce::Colours::purple;
    juce::Colour thruMuteColour = juce::Colours::grey;
    juce::Colour thruMuteColour_OFF = juce::Colours::lightgrey;
    
    int recordFlashCounter = 0;  // For loop wrap flash indicator
    std::atomic<bool> stackButtonWasDown { false };  // Track STACK button state for proper press/release

    void setupButton(juce::TextButton& button, const juce::String& text, juce::Colour colour, bool isToggle = false);
    void updateStatusDisplay();
    void drawLED(juce::Graphics& g, int x, int y, int size, juce::Colour colour, bool isLit);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BoomerangAudioProcessorEditor)
};