#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Config.h"

class TransientDetector
{
public:
    TransientDetector() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // mono follower (anche se input è stereo)
        juce::dsp::ProcessSpec monoSpec { spec.sampleRate, spec.maximumBlockSize, 1 };
        envelopeFollower.prepare(monoSpec);
        envelopeFollower.setAttackTime(AdaptiveIRConfig::TransientAttackMs);
        envelopeFollower.setReleaseTime(AdaptiveIRConfig::TransientReleaseMs);

        previousEnvelopeDb = -120.0f;
        transientHasOccurred = false;
        refractoryCounter = 0;
    }

    void process(const juce::AudioBuffer<float>& buffer, float sensitivity)
    {
        transientHasOccurred = false;

        if (sampleRate <= 0.0)
            return;

        if (refractoryCounter > 0)
        {
            refractoryCounter = juce::jmax(0, refractoryCounter - buffer.getNumSamples());
            if (refractoryCounter > 0)
                return;
        }

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        // Sensitivity 0..1 -> soglia dB (0 = difficile, 1 = facile)
        const float sens = juce::jlimit(0.0f, 1.0f, sensitivity);
        const float thresholdDb = juce::jmap(sens,
                                             0.0f, 1.0f,
                                             AdaptiveIRConfig::TransientThresholdDbAtSens0,
                                             AdaptiveIRConfig::TransientThresholdDbAtSens1);

        for (int i = 0; i < numSamples; ++i)
        {
            // mono mix: media del valore assoluto
            float monoAbs = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                monoAbs += std::abs(buffer.getSample(ch, i));

            monoAbs /= juce::jmax(1, numChannels);

            const float envelope = envelopeFollower.processSample(0, monoAbs);

            // Converti in dB (evita -inf)
            const float envelopeDb = juce::Decibels::gainToDecibels(envelope + 1.0e-6f, -120.0f);

            // edge detection: crossing da sotto a sopra la soglia
            if (envelopeDb > thresholdDb && previousEnvelopeDb <= thresholdDb)
            {
                transientHasOccurred = true;
                refractoryCounter = static_cast<int>(sampleRate * (AdaptiveIRConfig::RefractoryMs / 1000.0f));
                break;
            }

            previousEnvelopeDb = envelopeDb;
        }
    }

    bool wasTransientDetected() const
    {
        return transientHasOccurred;
    }

private:
    juce::dsp::BallisticsFilter<float> envelopeFollower;
    double sampleRate = 0.0;

    float previousEnvelopeDb = -120.0f;
    bool transientHasOccurred = false;
    int refractoryCounter = 0;
};