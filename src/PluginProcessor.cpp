#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
AdaptiveIRAudioProcessor::AdaptiveIRAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
    , apvts(*this, nullptr, "Parameters", AdaptiveIRParams::createLayout())
{
}

AdaptiveIRAudioProcessor::~AdaptiveIRAudioProcessor() = default;

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

int AdaptiveIRAudioProcessor::getNumPrograms() { return 1; }
int AdaptiveIRAudioProcessor::getCurrentProgram() { return 0; }
void AdaptiveIRAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String AdaptiveIRAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void AdaptiveIRAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

//==============================================================================
void AdaptiveIRAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumInputChannels())
    };

    transientDetector.prepare(spec);
    spectralAnalyzer.prepare(spec);
    dynamicEQ.prepare(spec);
    convolution.prepare(spec);

    // Initial latency: DynamicEQ (fftSize - hopSize) + convolution latency.
    // For default fftOrder=12 and hop=0.5 => 4096-2048 = 2048
    setLatencySamples(2048 + static_cast<int>(convolution.getLatency()));
}

void AdaptiveIRAudioProcessor::releaseResources()
{
    convolution.reset();
    irLoaded = false;
    loadedIRName = "None";
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AdaptiveIRAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
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

void AdaptiveIRAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (!irLoaded)
    {
        if (AdaptiveIRConfig::BypassIfNoIR)
        {
            // Pass-through: copia input su output
            // (solo se input e output hanno lo stesso numero di canali)
            // Se vuoi gestire casi diversi, copia solo i canali disponibili
            for (int ch = 0; ch < juce::jmin(totalNumInputChannels, totalNumOutputChannels); ++ch)
                ; // non serve copiare, il buffer è già input=output
            return;
        }
        buffer.clear();
        return;
    }

    // --- Get Parameters ---
    const float release = apvts.getRawParameterValue(AdaptiveIRParams::RELEASE)->load();
    const float sensitivity = apvts.getRawParameterValue(AdaptiveIRParams::SENSITIVITY)->load();
    const int resolution = static_cast<int>(apvts.getRawParameterValue(AdaptiveIRParams::RESOLUTION)->load());
    const bool processModePre = apvts.getRawParameterValue(AdaptiveIRParams::PROCESSMODE)->load() > 0.5f;

    // --- DSP Signal Flow ---
    // Always push samples to analyzer FIFO (mono mix inside analyzer)
    spectralAnalyzer.pushSamples(buffer);

    // Copy for transient detection (detector does mono mix internally)
    juce::AudioBuffer<float> analysisBuffer;
    analysisBuffer.makeCopyOf(buffer);

    // 1) Transient detection
    transientDetector.process(analysisBuffer, sensitivity);

    if (transientDetector.wasTransientDetected())
    {
        // 2) Spectral analysis on transient
        spectralAnalyzer.analyze();
        const auto& spectrum = spectralAnalyzer.getSpectrumData();

        // 3) Generate inverse EQ curve
        dynamicEQ.generateCurve(spectrum,
                                resolution,
                                spectralAnalyzer.getFftSize(),
                                spectralAnalyzer.getSampleRate());
    }

    // 4) Apply processing based on mode
    dynamicEQ.setRelease(release);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    if (processModePre)
    {
        // Pre-convolution EQ
        dynamicEQ.process(context);
        convolution.process(context);
    }
    else
    {
        // Post-convolution EQ
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
    return new AdaptiveIRAudioProcessorEditor(*this);
}

//==============================================================================
void AdaptiveIRAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AdaptiveIRAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void AdaptiveIRAudioProcessor::loadIRFromFile()
{
    irChooser = std::make_unique<juce::FileChooser>(
        "Select an Impulse Response file...",
        juce::File{},
        "*.wav;*.aif;*.flac"
    );

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
            convolution.loadImpulseResponse(
                file,
                juce::dsp::Convolution::Stereo::yes,
                juce::dsp::Convolution::Trim::yes,
                reader->lengthInSamples
            );

            loadedIRName = file.getFileName();
            irLoaded = true;

            setLatencySamples(2048 + static_cast<int>(convolution.getLatency()));
            updateHostDisplay();
        }
    });
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AdaptiveIRAudioProcessor();
}