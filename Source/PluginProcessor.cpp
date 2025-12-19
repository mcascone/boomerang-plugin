#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter IDs
namespace ParameterIDs
{
    const juce::String thruMute   = "thruMute";
    const juce::String record     = "record";
    const juce::String play       = "play";
    const juce::String once       = "once";
    const juce::String stack      = "stack";
    const juce::String reverse    = "reverse";
    const juce::String volume     = "volume";
    const juce::String feedback   = "feedback";
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BoomerangAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Button parameters - use bool parameters that can be MIDI mapped
    // Note: For momentary buttons, we'll handle press/release in parameter change callback
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::thruMute, 1),
        "Thru/Mute",
        false));  // toggle button

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::record, 1),
        "Record",
        false));  // momentary button

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::play, 1),
        "Play/Stop",
        false));  // momentary button

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::once, 1),
        "Once",
        false));  // momentary button

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::stack, 1),
        "Stack/Speed",
        false));  // momentary button (tracks press/release)

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(ParameterIDs::reverse, 1),
        "Reverse",
        false));  // toggle button

    // Continuous parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::volume, 1),
        "Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(ParameterIDs::feedback, 1),
        "Feedback",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));

    return layout;
}

//==============================================================================
BoomerangAudioProcessor::BoomerangAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    // Initialize looper engine
    looperEngine = std::make_unique<LooperEngine>();
}

BoomerangAudioProcessor::~BoomerangAudioProcessor()
{
}

//==============================================================================
const juce::String BoomerangAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BoomerangAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool BoomerangAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool BoomerangAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double BoomerangAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BoomerangAudioProcessor::getNumPrograms()
{
    return 1;
}

int BoomerangAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BoomerangAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String BoomerangAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void BoomerangAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void BoomerangAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use whichever is larger so we can still play back in stereo even if the host only
    // provides a mono input (common with a single mic).
    const int maxChannels = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    looperEngine->prepare(sampleRate, samplesPerBlock, maxChannels);
}

void BoomerangAudioProcessor::releaseResources()
{
    looperEngine->reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BoomerangAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    const auto& outSet = layouts.getMainOutputChannelSet();
    const auto& inSet  = layouts.getMainInputChannelSet();

    // Allow mono or stereo output only
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;

    #if ! JucePlugin_IsSynth
    // Permit mono-in→stereo-out or matching mono/mono, stereo/stereo.
    const bool inputIsMonoOrStereo = (inSet == juce::AudioChannelSet::mono() || inSet == juce::AudioChannelSet::stereo());
    if (! inputIsMonoOrStereo)
        return false;

    // NOTE: We don't currently support stereo-in → mono-out because the engine assumes
    // output channels >= input channels. If we add downmixing, we can relax this.
    const bool matches = (inSet == outSet) || (inSet == juce::AudioChannelSet::mono() && outSet == juce::AudioChannelSet::stereo());
    if (! matches)
        return false;
    #endif

    return true;
  #endif
}
#endif

void BoomerangAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Update continuous parameters from APVTS
    auto* volumeParam = apvts.getRawParameterValue(ParameterIDs::volume);
    auto* feedbackParam = apvts.getRawParameterValue(ParameterIDs::feedback);
    
    if (volumeParam != nullptr)
        looperEngine->setVolume(volumeParam->load());
    
    if (feedbackParam != nullptr)
        looperEngine->setFeedback(feedbackParam->load());

    // Process audio through looper engine
    looperEngine->processBlock(buffer);

    // Upmix mono input to stereo output so users with a single mic still hear both channels
    if (totalNumInputChannels == 1 && totalNumOutputChannels >= 2)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
bool BoomerangAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* BoomerangAudioProcessor::createEditor()
{
    return new BoomerangAudioProcessorEditor (*this);
}

//==============================================================================
void BoomerangAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save APVTS state - this automatically handles all parameters
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BoomerangAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore APVTS state - this automatically handles all parameters
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoomerangAudioProcessor();
}
