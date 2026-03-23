#include "PluginEditor.h"

//==============================================================================
AdaptiveIRAudioProcessorEditor::AdaptiveIRAudioProcessorEditor(AdaptiveIRAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // --- Sliders ---
    releaseSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    releaseSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
    releaseSlider.setTextValueSuffix(" s");
    addAndMakeVisible(releaseSlider);

    sensitivitySlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 80, 20);
    addAndMakeVisible(sensitivitySlider);

    // --- ComboBox for Resolution ---
    resolutionComboBox.addItem("1 / Octave", 1);
    resolutionComboBox.addItem("1/2 Octave", 2);
    resolutionComboBox.addItem("1/3 Octave", 3);
    resolutionComboBox.addItem("1/4 Octave", 4);
    addAndMakeVisible(resolutionComboBox);

    // --- Buttons ---
    processModeButton.setButtonText("Toggle EQ Routing");
    processModeButton.setClickingTogglesState(true);
    addAndMakeVisible(processModeButton);

    loadIRButton.setButtonText("Load Impulse Response");
    addAndMakeVisible(loadIRButton);
    loadIRButton.onClick = [this] { audioProcessor.loadIRFromFile(); };

    // --- Labels ---
    releaseLabel.setText("Release", juce::dontSendNotification);
    releaseLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(releaseLabel);

    sensitivityLabel.setText("Sensitivity", juce::dontSendNotification);
    sensitivityLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(sensitivityLabel);

    resolutionLabel.setText("Resolution", juce::dontSendNotification);
    resolutionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(resolutionLabel);

    irNameLabel.setText("IR: None", juce::dontSendNotification);
    irNameLabel.setJustificationType(juce::Justification::centred);
    irNameLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible(irNameLabel);

    processModeLabel.setText("EQ Target: Wet Signal", juce::dontSendNotification);
    processModeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(processModeLabel);

    addAndMakeVisible(eqCurveDisplay);

    // --- Attachments ---
    releaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, AdaptiveIRParams::RELEASE, releaseSlider);
    sensitivityAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, AdaptiveIRParams::SENSITIVITY, sensitivitySlider);
    resolutionAttachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, AdaptiveIRParams::RESOLUTION, resolutionComboBox);
    processModeAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, AdaptiveIRParams::PROCESSMODE, processModeButton);

    // Timer for dynamic labels / curve
    startTimer(AdaptiveIRConfig::UiRefreshMs);

    setSize(500, 400);
}

AdaptiveIRAudioProcessorEditor::~AdaptiveIRAudioProcessorEditor()
{
    stopTimer();
}

void AdaptiveIRAudioProcessorEditor::timerCallback()
{
    // Update IR label
    const auto currentIR = audioProcessor.getLoadedIRName();
    const juce::String newIrText = "IR: " + currentIR;

    if (irNameLabel.getText() != newIrText)
        irNameLabel.setText(newIrText, juce::dontSendNotification);

    // Update process mode label
    const bool isPreConv = audioProcessor.apvts.getRawParameterValue(AdaptiveIRParams::PROCESSMODE)->load() > 0.5f;
    const juce::String modeText = isPreConv ? "EQ Target: Dry (Pre-IR)" : "EQ Target: Wet (Post-IR)";

    if (processModeLabel.getText() != modeText)
        processModeLabel.setText(modeText, juce::dontSendNotification);

    // Update curve display
    audioProcessor.getCurrentEQCurve(curveCopy);
    eqCurveDisplay.updateCurve(curveCopy, audioProcessor.getSampleRate());
}

//==============================================================================
void AdaptiveIRAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void AdaptiveIRAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    auto topArea = bounds.removeFromTop(100);
    loadIRButton.setBounds(topArea.removeFromTop(30));
    irNameLabel.setBounds(topArea.removeFromTop(30));

    bounds.removeFromTop(10);

    // EQ Display
    eqCurveDisplay.setBounds(bounds.removeFromTop(120));

    bounds.removeFromTop(10);

    auto controlsArea = bounds.removeFromTop(120);
    auto knobWidth = controlsArea.getWidth() / 3;

    auto releaseArea = controlsArea.removeFromLeft(knobWidth);
    releaseLabel.setBounds(releaseArea.removeFromTop(20));
    releaseSlider.setBounds(releaseArea);

    auto sensitivityArea = controlsArea.removeFromLeft(knobWidth);
    sensitivityLabel.setBounds(sensitivityArea.removeFromTop(20));
    sensitivitySlider.setBounds(sensitivityArea);

    auto rightSideArea = controlsArea;
    resolutionLabel.setBounds(rightSideArea.removeFromTop(20));
    resolutionComboBox.setBounds(rightSideArea.removeFromTop(30));

    bounds.removeFromTop(20);

    auto bottomArea = bounds;
    processModeLabel.setBounds(bottomArea.removeFromTop(20));
    processModeButton.setBounds(bottomArea.removeFromTop(30));
}