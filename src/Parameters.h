#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Config.h"

namespace AdaptiveIRParams
{
    inline const juce::StringRef RELEASE     = "Release";
    inline const juce::StringRef SENSITIVITY = "Sensitivity";
    inline const juce::StringRef RESOLUTION  = "Resolution";
    inline const juce::StringRef PROCESSMODE = "ProcessMode";

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ RELEASE, 1 },
            "Release",
            juce::NormalisableRange<float>(
                AdaptiveIRConfig::ReleaseMin,
                AdaptiveIRConfig::ReleaseMax,
                AdaptiveIRConfig::ReleaseStep,
                AdaptiveIRConfig::ReleaseSkew
            ),
            AdaptiveIRConfig::ReleaseDefault
        ));

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{ SENSITIVITY, 1 },
            "Sensitivity",
            juce::NormalisableRange<float>(
                AdaptiveIRConfig::SensMin,
                AdaptiveIRConfig::SensMax,
                AdaptiveIRConfig::SensStep
            ),
            AdaptiveIRConfig::SensDefault
        ));

        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{ RESOLUTION, 1 },
            "Resolution",
            AdaptiveIRConfig::ResolutionMin,
            AdaptiveIRConfig::ResolutionMax,
            AdaptiveIRConfig::ResolutionDefault
        ));

        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{ PROCESSMODE, 1 },
            "Process Mode",
            AdaptiveIRConfig::ProcessModeDefault
        ));

        return layout;
    }
}