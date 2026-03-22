#pragma once

#include "PluginProcessor.h"
#include "components/EQCurveDisplay.h"

class AdaptiveIRAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit AdaptiveIRAudioProcessorEditor (AdaptiveIRAudioProcessor&);
    ~AdaptiveIRAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AdaptiveIRAudioProcessor& audioProcessor;

    // UI Components
    EQCurveDisplay eqCurveDisplay;
    juce::Slider releaseSlider;
    juce::Slider sensitivitySlider;
    juce::ComboBox resolutionComboBox;
    juce::TextButton processModeButton;
    juce::TextButton loadIRButton;

    // Labels for controls
    juce::Label releaseLabel;
    juce::Label sensitivityLabel;
    juce::Label resolutionLabel;
    juce::Label irNameLabel;
    juce::Label processModeLabel;
    
    // Parameter Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> sensitivityAttachment;
    std::unique_ptr<ComboBoxAttachment> resolutionAttachment;
    std::unique_ptr<ButtonAttachment> processModeAttachment;

    std::vector<float> curveCopy;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdaptiveIRAudioProcessorEditor)
};
