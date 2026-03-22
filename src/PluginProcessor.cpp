#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
const juce::StringRef AdaptiveIRAudioProcessor::RELEASE_PARAM = "Release";
const juce::StringRef AdaptiveIRAudioProcessor::SENSITIVITY  = "Sensitivity";
const juce::StringRef AdaptiveIRAudioProcessor::RESOLUTION   = "Resolution";
const juce::StringRef AdaptiveIRAudioProcessor::PROCESS_MODE = "ProcessMode";

//==============================================================================
AdaptiveIRAudioProcessor::AdaptiveIRAudioProcessor()
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
}

AdaptiveIRAudioProcessor::~AdaptiveIRAudioProcessor()
{
}

//==============================================================================
const juce::String AdaptiveIRAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AdaptiveIRAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AdaptiveIRAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AdaptiveIRAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AdaptiveIRAudioProcessor::getTailLengthSeconds() const
{
    if (currentSampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(convolution.getLatency()) / currentSampleRate;
}

int AdaptiveIRAudioProcessor::getNumPrograms()
{
    return 1;
}

int AdaptiveIRAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AdaptiveIRAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String AdaptiveIRAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AdaptiveIRAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AdaptiveIRAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(getTotalNumInputChannels()) };

    transientDetector.prepare(spec);
    spectralAnalyzer.prepare(spec);
    dynamicEQ.prepare(spec);
    convolution.prepare(spec);

    // Initial latency: DynamicEQ (fftSize - hopSize)
    setLatencySamples(2048 + static_cast<int>(convolution.getLatency()));
}

void AdaptiveIRAudioProcessor::releaseResources()
{
    convolution.reset();
    irLoaded = false;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AdaptiveIRAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AdaptiveIRAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (!irLoaded)
    {
        buffer.clear();
        return;
    }

    // --- Get Parameters ---
    auto release = apvts.getRawParameterValue(RELEASE_PARAM)->load();
    auto sensitivity = apvts.getRawParameterValue(SENSITIVITY)->load();
    auto resolution = static_cast<int>(apvts.getRawParameterValue(RESOLUTION)->load());
    auto processMode = apvts.getRawParameterValue(PROCESS_MODE)->load() > 0.5f;

    // --- DSP Signal Flow ---
    
    // Always push samples to the analyzer FIFO
    spectralAnalyzer.pushSamples(buffer);

    // Create a copy for transient detection
    juce::AudioBuffer<float> analysisBuffer;
    analysisBuffer.makeCopyOf(buffer);

    // 1. Transient Detection
    transientDetector.process(analysisBuffer, sensitivity);
    if (transientDetector.wasTransientDetected())
    {
        // 2. Spectral Analysis (on transient)
        spectralAnalyzer.analyze();
        const auto& spectrum = spectralAnalyzer.getSpectrumData();
        
        // 3. Generate Inverse EQ Curve
        dynamicEQ.generateCurve(spectrum, 
                                resolution, 
                                spectralAnalyzer.getFftSize(), 
                                spectralAnalyzer.getSampleRate());
    }
    
    // 4. Apply processing based on mode
    dynamicEQ.setRelease(release);
    
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    if (processMode) // Pre-Convolution EQ
    {
        dynamicEQ.process(context);
        convolution.process(context);
    }
    else // Post-Convolution EQ
    {
        convolution.process(context);
        dynamicEQ.process(context);
    }
}

//==============================================================================
bool AdaptiveIRAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AdaptiveIRAudioProcessor::createEditor()
{
    return new AdaptiveIRAudioProcessorEditor (*this);
}

//==============================================================================
void AdaptiveIRAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AdaptiveIRAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

void AdaptiveIRAudioProcessor::loadIRFromFile()
{
    irChooser = std::make_unique<juce::FileChooser>("Select an Impulse Response file...", juce::File{}, "*.wav;*.aif;*.flac");
    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    irChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (!file.existsAsFile())
            return;

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

        if (reader != nullptr)
        {
            convolution.loadImpulseResponse(file,
                                            juce::dsp::Convolution::Stereo::yes,
                                            juce::dsp::Convolution::Trim::yes,
                                            reader->lengthInSamples);
            loadedIRName = file.getFileName();
            irLoaded = true;
            setLatencySamples(2048 + static_cast<int>(convolution.getLatency()));
            updateHostDisplay();
        }
    });
}


juce::AudioProcessorValueTreeState::ParameterLayout AdaptiveIRAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{RELEASE_PARAM, 1},
        "Release",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f, 0.5f),
        1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{SENSITIVITY, 1},
        "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.5f));
    
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{RESOLUTION, 1},
        "Resolution",
        1, 4, 1));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{PROCESS_MODE, 1},
        "Process Mode",
        false));

    return layout;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AdaptiveIRAudioProcessor();
}
