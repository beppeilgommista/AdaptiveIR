#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/DynamicEQ.h"
#include "dsp/SpectralAnalyzer.h"
#include "dsp/TransientDetector.h"
#include "dsp/Utils.h"
#include <juce_dsp/juce_dsp.h>

class AdaptiveIRAudioProcessor  : public juce::AudioProcessor
{
public:
    AdaptiveIRAudioProcessor();
    ~AdaptiveIRAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    void loadIRFromFile();
    juce::String getLoadedIRName() const { return loadedIRName; }
    void getCurrentEQCurve(std::vector<float>& dest) const { dynamicEQ.getCurve(dest); }
    
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

    // Parameter IDs
    static const juce::StringRef RELEASE_PARAM;
    static const juce::StringRef SENSITIVITY;
    static const juce::StringRef RESOLUTION;
    static const juce::StringRef PROCESS_MODE;

private:
    //==============================================================================
    // DSP Modules
    TransientDetector transientDetector;
    SpectralAnalyzer spectralAnalyzer;
    DynamicEQ dynamicEQ;
    juce::dsp::Convolution convolution;
    std::unique_ptr<juce::FileChooser> irChooser;
    juce::String loadedIRName = "None";
    bool irLoaded = false;
    double currentSampleRate = 0.0;
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveIRAudioProcessor)
};
