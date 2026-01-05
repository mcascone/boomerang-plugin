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
    
    // Set default size and make resizable with aspect ratio constraint
    // Base size: 700x240, default at 1.5x scale (1050x360)
    setSize (1050, 360);
    setResizable(true, true);
    setResizeLimits(350, 120, 1400, 480);  // 0.5x to 2x scale
    getConstrainer()->setFixedAspectRatio(700.0 / 240.0);

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
    // Draw background image scaled to window size
    if (backgroundImage.isValid())
    {
        // Calculate scaled image height (proportional to window width)
        float scale = getWidth() / 700.0f;
        int scaledImageHeight = static_cast<int>(200 * scale);
        
        // drawImage(image, destX, destY, destW, destH, srcX, srcY, srcW, srcH)
        g.drawImage(backgroundImage, 
                   0, 0, getWidth(), scaledImageHeight,
                   0, 0, backgroundImage.getWidth(), backgroundImage.getHeight(),
                   false);
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
    
    // Volume slider overlay when interacting - small box that follows the slider position
    if (volumeSlider.isMouseOver() || volumeSlider.isMouseButtonDown())
    {
        auto sliderBounds = volumeSlider.getBounds();
        double proportion = (volumeSlider.getValue() - volumeSlider.getMinimum()) / 
                           (volumeSlider.getMaximum() - volumeSlider.getMinimum());
        
        // Calculate thumb position (inverted for vertical slider - top is max)
        int thumbHeight = sliderBounds.getHeight() / 8;  // Small box
        int thumbWidth = sliderBounds.getWidth() / 2;     // Half width
        int thumbX = sliderBounds.getX() + (sliderBounds.getWidth() - thumbWidth) / 2;  // Center horizontally
        int thumbY = sliderBounds.getY() + static_cast<int>((1.0 - proportion) * (sliderBounds.getHeight() - thumbHeight));
        
        juce::Rectangle<int> thumbRect(thumbX, thumbY, thumbWidth, thumbHeight);
        
        g.setColour(juce::Colours::cyan.withAlpha(volumeSlider.isMouseButtonDown() ? 0.5f : 0.3f));
        g.fillRect(thumbRect);
    }
}

void BoomerangAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Calculate scale factor based on current width (base width is 700)
    float scale = getWidth() / 700.0f;
    
    // Helper lambda to scale positions
    auto scaleRect = [scale](int x, int y, int w, int h) {
        return juce::Rectangle<int>(
            static_cast<int>(x * scale),
            static_cast<int>(y * scale),
            static_cast<int>(w * scale),
            static_cast<int>(h * scale)
        );
    };
    
    // Position transparent buttons over foot switches in the background image
    // Base positions for 700x240 window:
    
    // Thru/Mute button (left side, above OUTPUT LEVEL)
    thruMuteButton.setBounds(scaleRect(100, 20, 50, 30));
    
    // Volume slider overlays OUTPUT LEVEL knob on left side
    // Wide clickable area to emulate rolling the volume knob
    volumeSlider.setBounds(scaleRect(70, 60, 120, 90));
    
    // Main foot switches (centered horizontally)
    // Base positions: startX=175, spacing=95, buttonY=120, width=60, height=60
    int baseStartX = 175;
    int baseSpacing = 95;
    int baseButtonY = 120;
    int baseButtonWidth = 60;
    int baseButtonHeight = 60;
    
    recordButton.setBounds(scaleRect(baseStartX, baseButtonY, baseButtonWidth, baseButtonHeight));
    playButton.setBounds(scaleRect(baseStartX + baseSpacing, baseButtonY, baseButtonWidth, baseButtonHeight));
    onceButton.setBounds(scaleRect(baseStartX + baseSpacing * 2, baseButtonY, baseButtonWidth, baseButtonHeight));
    reverseButton.setBounds(scaleRect(baseStartX + baseSpacing * 3, baseButtonY, baseButtonWidth, baseButtonHeight));
    stackButton.setBounds(scaleRect(baseStartX + baseSpacing * 4, baseButtonY, baseButtonWidth, baseButtonHeight));
    
    // Controls area below the image (base: skip 210px for image, then 30px for controls)
    auto controlsArea = bounds;
    controlsArea.removeFromTop(static_cast<int>(210 * scale));
    controlsArea = controlsArea.removeFromTop(static_cast<int>(30 * scale)).reduced(
        static_cast<int>(20 * scale),
        static_cast<int>(5 * scale)
    );
    
    // Status label
    statusLabel.setBounds(controlsArea);
    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f * scale)));
    
    // Version label (bottom right, scaled)
    versionLabel.setBounds(
        getWidth() - static_cast<int>(150 * scale),
        getHeight() - static_cast<int>(20 * scale),
        static_cast<int>(140 * scale),
        static_cast<int>(15 * scale)
    );
    versionLabel.setFont(juce::Font(juce::FontOptions(10.0f * scale)));
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
