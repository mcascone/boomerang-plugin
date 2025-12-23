#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
BoomerangAudioProcessorEditor::BoomerangAudioProcessorEditor (BoomerangAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), progressBar(progressValue)
{
    // Load background image from embedded binary data
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::boomerang_jpg, 
                                                      BinaryData::boomerang_jpgSize);
    
    // Set size to match image (700x200) with some padding for controls
    setSize (700, 300);

    // Setup buttons as transparent overlays
    // Position them over the foot switches in the image
    // Image dimensions: 700x200, buttons are roughly centered vertically at y~130
    
    setupButton(thruMuteButton, "", juce::Colours::transparentBlack, true);  // No text, transparent
    setupButton(recordButton, "", juce::Colours::transparentBlack, false);
    setupButton(playButton, "", juce::Colours::transparentBlack, false);
    setupButton(onceButton, "", juce::Colours::transparentBlack, false);
    setupButton(stackButton, "", juce::Colours::transparentBlack, false);  // Momentary, not toggle
    setupButton(reverseButton, "", juce::Colours::transparentBlack, true);
    
    // Make buttons semi-transparent for hover feedback
    thruMuteButton.setAlpha(0.0f);  // Fully transparent by default
    recordButton.setAlpha(0.0f);
    playButton.setAlpha(0.0f);
    onceButton.setAlpha(0.0f);
    stackButton.setAlpha(0.0f);
    reverseButton.setAlpha(0.0f);

    // Setup sliders
    // Volume slider overlays the OUTPUT LEVEL knob in the image
    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(1.0);
    volumeSlider.setAlpha(1.0f);  // Semi-transparent to show device knob
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
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    addAndMakeVisible(statusLabel);

    // Setup version label
    versionLabel.setText("v" BOOMERANG_VERSION " (" BOOMERANG_GIT_HASH ")", juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::bottomRight);
    versionLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.5f));
    addAndMakeVisible(versionLabel);

    // Setup progress bar
    // addAndMakeVisible(progressBar);  // Hidden for now

    // Attach continuous controls to APVTS
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "volume", volumeSlider);
    
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "feedback", feedbackSlider);
    
    // All buttons use onClick - manual control, visual state synced in timer
    thruMuteButton.onClick = [this]() {
        audioProcessor.getLooperEngine()->onThruMuteButtonPressed();
    };
    
    recordButton.onClick = [this]() {
        audioProcessor.getLooperEngine()->onRecordButtonPressed();
    };
    
    playButton.onClick = [this]() {
        audioProcessor.getLooperEngine()->onPlayButtonPressed();
    };
    
    onceButton.onClick = [this]() {
        audioProcessor.getLooperEngine()->onOnceButtonPressed();
    };
    
    reverseButton.onClick = [this]() {
        audioProcessor.getLooperEngine()->onReverseButtonPressed();
    };
    
    // Stack button - use onClick for stopped (speed toggle), onStateChange for playing (overdub)
    stackButton.onClick = [this]() {
        auto state = audioProcessor.getLooperEngine()->getState();
        // onClick only does something when stopped - toggles speed
        if (state == LooperEngine::LooperState::Stopped)
        {
            audioProcessor.getLooperEngine()->onStackButtonPressed();
        }
    };
    
    // Handle press/release for overdub while playing
    stackButton.onStateChange = [this]() {
        bool isDown = stackButton.isDown();
        bool wasDown = stackButtonWasDown.load();
        auto state = audioProcessor.getLooperEngine()->getState();
        
        // Only handle state changes when playing/overdubbing
        if (state == LooperEngine::LooperState::Playing || state == LooperEngine::LooperState::Overdubbing)
        {
            if (isDown && !wasDown)
            {
                audioProcessor.getLooperEngine()->onStackButtonPressed();
                stackButtonWasDown.store(true);
            }
            else if (!isDown && wasDown)
            {
                audioProcessor.getLooperEngine()->onStackButtonReleased();
                stackButtonWasDown.store(false);
            }
        }
        else
        {
            // Keep tracking synced when not playing
            stackButtonWasDown.store(isDown);
        }
    };

    // Start timer for UI updates and audio thread request processing (issue #38)
    startTimer(16); // 16ms (~60Hz) for responsive UI and Once mode auto-off
}

BoomerangAudioProcessorEditor::~BoomerangAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void BoomerangAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Draw background image if loaded
    if (backgroundImage.isValid())
    {
        g.drawImage(backgroundImage, 0, 0, 700, 200, 0, 0, 
                   backgroundImage.getWidth(), backgroundImage.getHeight());
    }
    else
    {
        // Fallback: solid color
        g.fillAll(juce::Colour(0xff404040));
        g.setColour(juce::Colours::white);
        g.drawText("Background image not found", getLocalBounds(), juce::Justification::centred);
    }
    
    // Draw button hover overlays (when mouse is over)
    // This provides visual feedback since buttons are transparent
    auto drawHoverOverlay = [&](juce::TextButton& button, juce::Colour colour) {
        if (button.isMouseOver())
        {
            g.setColour(colour.withAlpha(0.3f));
            g.fillRect(button.getBounds());
        }
        if (button.getToggleState())
        {
            g.setColour(colour.withAlpha(0.5f));
            g.fillRect(button.getBounds());
        }
    };
    
    // Special case: RECORD button flash on loop wrap
    if (recordFlashCounter > 0)
    {
        g.setColour(juce::Colours::red.withAlpha(0.7f));
        g.fillRect(recordButton.getBounds());
    }
    else
    {
        drawHoverOverlay(recordButton, juce::Colours::red);
    }
    
    drawHoverOverlay(thruMuteButton, juce::Colours::yellow);
    drawHoverOverlay(playButton, juce::Colours::green);
    drawHoverOverlay(onceButton, juce::Colours::blue);
    drawHoverOverlay(stackButton, juce::Colours::orange);
    drawHoverOverlay(reverseButton, juce::Colours::purple);
}

void BoomerangAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Position transparent buttons over foot switches in the background image
    // Image is 700x200 at top of window
    // Foot switches are roughly at y=115-175, evenly spaced horizontally
    
    // Button positions measured from the image:
    // Each button is ~100px wide, centered on the foot switch
    // Y position ~115, height ~60 to cover the foot switch
    
    int buttonY = 120;
    int buttonHeight = 60;
    int buttonWidth = 60;
    
    // Calculate positions (left to right):
    // RECORD, PLAY, ONCE, DIRECTION, STACK
    // Note: THRU MUTE is on the left side, separate from the main buttons
    
    // Thru/Mute button (left side, above OUTPUT LEVEL)
    thruMuteButton.setBounds(100, 20, 50, 30);
    
    // Volume slider overlays OUTPUT LEVEL knob on left side
    volumeSlider.setBounds(100, 80, 55, 55);
    
    // Main foot switches (centered horizontally)
    int startX = 175;  // Starting X position for first button
    int spacing = 95; // Space between button centers
    
    recordButton.setBounds(startX, buttonY, buttonWidth, buttonHeight);
    playButton.setBounds(startX + spacing, buttonY, buttonWidth, buttonHeight);
    onceButton.setBounds(startX + spacing * 2, buttonY, buttonWidth, buttonHeight);
    reverseButton.setBounds(startX + spacing * 3, buttonY, buttonWidth, buttonHeight);
    stackButton.setBounds(startX + spacing * 4, buttonY, buttonWidth, buttonHeight);
    
    // Controls area below the image
    auto controlsArea = bounds;
    controlsArea.removeFromTop(210); // Skip image area
    controlsArea = controlsArea.removeFromTop(70).reduced(20, 10);
    
    // Status label
    statusLabel.setBounds(controlsArea.removeFromTop(20));
    
    // Feedback slider in controls area (volume is now on device image)
    auto sliderArea = controlsArea.reduced(0, 5);
    auto feedbackArea = sliderArea.reduced(10, 0);
    
    feedbackLabel.setBounds(feedbackArea.removeFromTop(15));
    feedbackSlider.setBounds(feedbackArea);
    
    // Version label (bottom right)
    versionLabel.setBounds(getWidth() - 150, getHeight() - 20, 140, 15);
}

void BoomerangAudioProcessorEditor::timerCallback()
{
    // Process audio thread requests (issue #38) - UI thread only
    audioProcessor.getLooperEngine()->processAudioThreadRequests();
    
    updateStatusDisplay();
    
    // Update progress bar
    progressValue = audioProcessor.getLooperEngine()->getLoopProgress();
    
    // Update button toggle states to match engine state
    auto thruState = audioProcessor.getLooperEngine()->getThruMuteState();
    thruMuteButton.setToggleState(thruState == LooperEngine::ThruMuteState::On, juce::dontSendNotification);
    
    auto looperState = audioProcessor.getLooperEngine()->getState();
    bool isRecording = (looperState == LooperEngine::LooperState::Recording || 
                        looperState == LooperEngine::LooperState::Overdubbing);
    recordButton.setToggleState(isRecording, juce::dontSendNotification);
    
    bool isPlaying = (looperState == LooperEngine::LooperState::Playing || 
                      looperState == LooperEngine::LooperState::Overdubbing);
    playButton.setToggleState(isPlaying, juce::dontSendNotification);
    
    auto loopMode = audioProcessor.getLooperEngine()->getLoopMode();
    bool isReverse = (loopMode == LooperEngine::LoopMode::Reverse);
    reverseButton.setToggleState(isReverse, juce::dontSendNotification);
    
    auto onceMode = audioProcessor.getLooperEngine()->getOnceMode();
    bool isOnce = (onceMode == LooperEngine::OnceMode::On);
    onceButton.setToggleState(isOnce, juce::dontSendNotification);
    
    auto thruMuteState = audioProcessor.getLooperEngine()->getThruMuteState();
    bool isThruMuted = (thruMuteState == LooperEngine::ThruMuteState::On);
    thruMuteButton.setToggleState(isThruMuted, juce::dontSendNotification);
    
    // Flash record button when loop wraps around
    if (audioProcessor.getLooperEngine()->checkAndClearLoopWrapped())
    {
        recordFlashCounter = 10;  // Flash for ~160ms (10 frames at 16ms)
    }
    else if (recordFlashCounter > 0)
    {
        recordFlashCounter--;
    }
    
    repaint(); // Refresh overlays and flash indicators
}

//==============================================================================
void BoomerangAudioProcessorEditor::setupButton(juce::TextButton& button, 
                                               const juce::String& text, 
                                               juce::Colour colour,
                                               bool isToggle)
{
    button.setButtonText(text);
    button.setColour(juce::TextButton::buttonColourId, colour.darker(0.8f));
    button.setColour(juce::TextButton::buttonOnColourId, colour);
    button.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    button.setClickingTogglesState(isToggle);
    addAndMakeVisible(button);
}

void BoomerangAudioProcessorEditor::updateStatusDisplay()
{
    juce::String statusText;
    auto state = audioProcessor.getLooperEngine()->getState();
    auto loopMode = audioProcessor.getLooperEngine()->getLoopMode();
    auto once = audioProcessor.getLooperEngine()->getOnceMode();
    auto stack = audioProcessor.getLooperEngine()->getStackMode();
    auto speed = audioProcessor.getLooperEngine()->getSpeedMode();
    auto thru = audioProcessor.getLooperEngine()->getThruMuteState();
    
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
        case LooperEngine::LooperState::ContinuousReverse:
            statusText = "Continuous Reverse";
            break;
        case LooperEngine::LooperState::BufferFilled:
            statusText = "Buffer Filled";
            break;
        default:
            break;
    }
    
    // Add mode info
    if (loopMode == LooperEngine::LoopMode::Reverse)
        statusText += " [Reverse]";

    if (once == LooperEngine::OnceMode::On)
        statusText += " [Once]";
    
    if (stack == LooperEngine::StackMode::On)
        statusText += " [Stack]";

    if (speed == LooperEngine::SpeedMode::Half)
        statusText += " [1/2 Speed]";

    if (thru == LooperEngine::ThruMuteState::On)
        statusText += " [Thru Mute]";
    
    statusLabel.setText(statusText, juce::dontSendNotification);
}
