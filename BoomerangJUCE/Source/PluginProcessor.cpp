#include "PluginProcessor.h"
#include "PluginEditor.h"

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
                       )
#endif
{
    // Initialize looper engine
    looperEngine = std::make_unique<LooperEngine>();

    // Create continuous parameters
    addParameter(volumeParam = new juce::AudioParameterFloat(
        juce::ParameterID("volume", 1), "Volume", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    
    addParameter(feedbackParam = new juce::AudioParameterFloat(
        juce::ParameterID("feedback", 1), "Feedback", 
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
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
}

const juce::String BoomerangAudioProcessor::getProgramName (int index)
{
    return {};
}

void BoomerangAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
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

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Update continuous parameters
    looperEngine->setVolume(volumeParam->get());
    looperEngine->setFeedback(feedbackParam->get());

    // Process audio through looper engine
    looperEngine->processBlock(buffer);
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
    auto state = juce::ValueTree("BoomerangState");
    
    state.setProperty("volume", volumeParam->get(), nullptr);
    state.setProperty("feedback", feedbackParam->get(), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void BoomerangAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName("BoomerangState"))
        {
            auto state = juce::ValueTree::fromXml(*xmlState);
            
            volumeParam->setValueNotifyingHost((float)state.getProperty("volume", 1.0f));
            feedbackParam->setValueNotifyingHost((float)state.getProperty("feedback", 0.5f));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoomerangAudioProcessor();
}
