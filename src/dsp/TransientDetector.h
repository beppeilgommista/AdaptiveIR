#pragma once

#include <juce_dsp/juce_dsp.h>

class TransientDetector
{
public:
    TransientDetector() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        
        // Ballistics filter for envelope following
        // Attack time is very short to catch the transient
        // Release time can be adjusted, but this is a starting point
        envelopeFollower.prepare(spec);
        envelopeFollower.setAttackTime(1.0f); // ms
        envelopeFollower.setReleaseTime(50.0f); // ms
    }

    // Process a block of audio and update the transient detection flag
    void process(const juce::AudioBuffer<float>& buffer, float sensitivity)
    {
        transientHasOccurred = false;
        
        if (refractoryCounter > 0)
        {
            int numSamples = buffer.getNumSamples();
            refractoryCounter = std::max(0, refractoryCounter - numSamples);
            if (refractoryCounter > 0) return;
        }

        const float* input = buffer.getReadPointer(0);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float envelope = envelopeFollower.processSample(0, std::abs(input[i]));
            float threshold = 1.0f - sensitivity;
            
            if (envelope > threshold && previousEnvelopeValue <= threshold)
            {
                transientHasOccurred = true;
                refractoryCounter = static_cast<int>(sampleRate * 0.2); // 200ms refractory period
                break;
            }
            
            previousEnvelopeValue = envelope;
        }
    }

    // Returns true if a transient was detected in the last processed block
    bool wasTransientDetected() const
    {
        return transientHasOccurred;
    }

private:
    juce::dsp::BallisticsFilter<float> envelopeFollower;
    double sampleRate = 0.0;
    float previousEnvelopeValue = 0.0f;
    bool transientHasOccurred = false;
    int refractoryCounter = 0;
};
