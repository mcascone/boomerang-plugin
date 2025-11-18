#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BoomerangAudioProcessorEditor::BoomerangAudioProcessorEditor (BoomerangAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), progressBar(progressValue)
{
    setSize (600, 400);

    // Setup title
    titleLabel.setText("Boomerang+", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Setup buttons
    setupButton(thruMuteButton, "THRU MUTE", thruMuteColour);
    setupButton(recordButton, "RECORD", recordColour);
    setupButton(playButton, "PLAY/STOP", playColour);
    setupButton(onceButton, "ONCE", onceColour);
    setupButton(stackButton, "STACK/SPEED", stackColour);
    setupButton(reverseButton, "REVERSE", reverseColour);

    // Setup sliders
    volumeSlider.setSliderStyle(juce::Slider::Rotary);
    volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(1.0);
    addAndMakeVisible(volumeSlider);

    feedbackSlider.setSliderStyle(juce::Slider::Rotary);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    feedbackSlider.setRange(0.0, 1.0, 0.01);
    feedbackSlider.setValue(0.5);
    addAndMakeVisible(feedbackSlider);

    // Setup labels
    volumeLabel.setText("Volume", juce::dontSendNotification);
    volumeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(volumeLabel);

    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    feedbackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(feedbackLabel);

    statusLabel.setText("Stopped", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(juce::FontOptions(16.0f)));
    addAndMakeVisible(statusLabel);

    // Setup progress bar
    addAndMakeVisible(progressBar);

    // For momentary buttons, we'll use direct callbacks instead of parameter attachments
    // This gives us more control over the momentary behavior
    thruMuteButton.onClick = [this]() { audioProcessor.getLooperEngine()->onThruMuteButtonPressed(); };
    recordButton.onClick = [this]() { audioProcessor.getLooperEngine()->onRecordButtonPressed(); };
    playButton.onClick = [this]() { audioProcessor.getLooperEngine()->onPlayButtonPressed(); };
    onceButton.onClick = [this]() { audioProcessor.getLooperEngine()->onOnceButtonPressed(); };
    stackButton.onClick = [this]() { audioProcessor.getLooperEngine()->onStackButtonPressed(); };
    reverseButton.onClick = [this]() { audioProcessor.getLooperEngine()->onReverseButtonPressed(); };

    // Keep parameter attachments for the continuous controls
    volumeAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.getVolumeParam(), volumeSlider);
    
    feedbackAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *audioProcessor.getFeedbackParam(), feedbackSlider);

    // Start timer for UI updates
    startTimer(30); // 30ms refresh rate for smooth progress bar
}

BoomerangAudioProcessorEditor::~BoomerangAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void BoomerangAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Background gradient
    g.fillAll (juce::Colour(0xff2d2d30));
    
    auto bounds = getLocalBounds();
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff404040), bounds.getTopLeft().toFloat(),
        juce::Colour(0xff2d2d30), bounds.getBottomRight().toFloat(),
        false));
    g.fillAll();

    // Draw decorative border
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRect(bounds.reduced(2), 2);

    // Draw loop slot indicators
    auto loopSlotArea = bounds.removeFromBottom(40).reduced(20);
    int slotWidth = loopSlotArea.getWidth() / 4;
    
    for (int i = 0; i < 4; ++i)
    {
        auto slotBounds = loopSlotArea.removeFromLeft(slotWidth).reduced(5);
        
        // Note: Direct access to looperEngine - in production you'd want getter methods
        // For now, we'll just show all slots equally
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.fillRoundedRectangle(slotBounds.toFloat(), 4.0f);
        
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawRoundedRectangle(slotBounds.toFloat(), 4.0f, 1.0f);
        
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawText(juce::String("Loop ") + juce::String(i + 1),
                  slotBounds, juce::Justification::centred, true);
    }
}

void BoomerangAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Title
    titleLabel.setBounds(bounds.removeFromTop(50));
    
    // Status and progress
    statusLabel.setBounds(bounds.removeFromTop(30).reduced(20, 0));
    progressBar.setBounds(bounds.removeFromTop(20).reduced(20, 0));
    
    bounds.removeFromTop(20); // spacer
    
    // Main button area
    auto buttonArea = bounds.removeFromTop(120);
    int buttonWidth = buttonArea.getWidth() / 5;
    
    thruMuteButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    recordButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    playButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    onceButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    stackButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    reverseButton.setBounds(buttonArea.removeFromLeft(buttonWidth).reduced(10));
    
    bounds.removeFromTop(20); // spacer
    
    // Controls area
    auto controlsArea = bounds.removeFromTop(120);
    auto volumeArea = controlsArea.removeFromLeft(controlsArea.getWidth() / 2).reduced(20);
    auto feedbackArea = controlsArea.reduced(20);
    
    volumeLabel.setBounds(volumeArea.removeFromTop(20));
    volumeSlider.setBounds(volumeArea);
    
    feedbackLabel.setBounds(feedbackArea.removeFromTop(20));
    feedbackSlider.setBounds(feedbackArea);
    
    // Loop slots will be drawn at the bottom (handled in paint)
}

void BoomerangAudioProcessorEditor::timerCallback()
{
    updateStatusDisplay();
    
    // Update progress bar
    progressValue = audioProcessor.getLooperEngine()->getLoopProgress();
    
    repaint(); // Refresh loop slot indicators
}

//==============================================================================
void BoomerangAudioProcessorEditor::setupButton(juce::TextButton& button, 
                                               const juce::String& text, 
                                               juce::Colour colour)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, colour.darker(0.8f));
    button.setColour(juce::TextButton::buttonOnColourId, colour);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    button.setClickingTogglesState(false); // This makes buttons momentary!
    addAndMakeVisible(button);
}

void BoomerangAudioProcessorEditor::updateStatusDisplay()
{
    juce::String statusText;
    auto state = audioProcessor.getLooperEngine()->getState();
    auto mode = audioProcessor.getLooperEngine()->getMode();
    
    switch (state)
    {
        case LooperEngine::LooperState::Stopped:
            statusText = "Stopped";
            break;
        case LooperEngine::LooperState::Recording:
            statusText = "Recording";
            break;
        case LooperEngine::LooperState::Playing:
            statusText = "Playing";
            break;
        case LooperEngine::LooperState::Overdubbing:
            statusText = "Overdubbing";
            break;
    }
    
    // Add mode info
    switch (mode)
    {
        case LooperEngine::LoopMode::Once:
            statusText += " (Once)";
            break;
        case LooperEngine::LoopMode::Reverse:
            statusText += " (Reverse)";
            break;
        case LooperEngine::LoopMode::Stack:
            statusText += " (Stack)";
            break;
        case LooperEngine::LoopMode::Normal:
        default:
            break;
    }
    
    statusLabel.setText(statusText, juce::dontSendNotification);
}