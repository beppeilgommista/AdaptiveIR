#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Utils.h"

class DynamicEQ
{
public:
    DynamicEQ() :
        fftOrder(12),
        fftSize(1 << fftOrder),
        fft(fftOrder),
        window(fftSize, juce::dsp::WindowingFunction<float>::hann),
        hopSize(fftSize / 2)
    {
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        writePosition = 0;
        
        fftData.resize(2 * fftSize, 0.0f);
        
        auto numBins = (fftSize / 2) + 1;
        eqCurve.assign(numBins, 1.0f);
        targetEqCurve.assign(numBins, 1.0f);
        inputFifo.assign(spec.numChannels, std::vector<float>(fftSize, 0.0f));
        outputFifo.assign(spec.numChannels, std::vector<float>(fftSize, 0.0f));
        
        setRelease(1.0f); // Default release time
    }

    void setRelease(float releaseTimeSeconds)
    {
        if (sampleRate <= 0.0)
        {
            releaseFactor = 0.0f;
            return;
        }

        auto clampedRelease = juce::jmax(0.001f, releaseTimeSeconds);
        double dt = static_cast<double>(hopSize) / sampleRate;
        releaseFactor = static_cast<float>(std::exp(-dt / clampedRelease));
    }

    void generateCurve(const std::vector<float>& spectrumData, int resolution, int sourceFftSize, double sourceSampleRate)
    {
        if (spectrumData.empty() || sourceSampleRate <= 0.0 || sampleRate <= 0.0)
            return;

        const auto nyquist = static_cast<float>(sourceSampleRate * 0.5);
        const auto maxFreq = juce::jmin(18000.0f, nyquist);
        const auto minFreq = 40.0f;
        // 1. Get octave band definitions
        auto centerFreqs = AudioUtils::getOctaveBandCenterFrequencies(minFreq, maxFreq, resolution);
        if (centerFreqs.empty())
            return;
        std::map<float, float> bandEnergies;
        const auto spectrumSize = static_cast<int>(spectrumData.size());

        // 2. Calculate energy per band from the input spectrum
        for (auto centerFreq : centerFreqs)
        {
            auto limits = AudioUtils::getOctaveBandLimits(centerFreq, resolution);
            int startBin = AudioUtils::frequencyToBin(limits.first, sourceFftSize, sourceSampleRate);
            int endBin = AudioUtils::frequencyToBin(limits.second, sourceFftSize, sourceSampleRate);
            startBin = juce::jlimit(0, spectrumSize - 1, startBin);
            endBin = juce::jlimit(0, spectrumSize - 1, endBin);
            if (endBin < startBin)
                std::swap(startBin, endBin);
            
            float averageEnergy = 0.0f;
            int count = 0;
            for (int i = startBin; i <= endBin; ++i)
            {
                averageEnergy += spectrumData[static_cast<size_t>(i)];
                count++;
            }
            if (count > 0) averageEnergy /= static_cast<float>(count);

            bandEnergies[centerFreq] = averageEnergy;
        }

        // 3. Create the inverse EQ curve from band energies
        // This maps the octave-based gains to the linear FFT bins of this EQ
        for (size_t i = 0; i < targetEqCurve.size(); ++i)
        {
            float freq = AudioUtils::binToFrequency(static_cast<int>(i), static_cast<int>(fftSize), sampleRate);
            
            // Find the two nearest center frequencies
            auto it = bandEnergies.lower_bound(freq);
            if (it == bandEnergies.end()) {
                targetEqCurve[i] = bandEnergies.rbegin()->second;
            } else if (it == bandEnergies.begin()) {
                targetEqCurve[i] = it->second;
            } else {
                auto prev = std::prev(it);
                // Linear interpolation between the two band gains
                float freq1 = prev->first;
                float gain1 = prev->second;
                float freq2 = it->first;
                float gain2 = it->second;
                targetEqCurve[i] = gain1 + (gain2 - gain1) * (freq - freq1) / (freq2 - freq1);
            }
        }

        // 4. Convert energy to inverse gain
        const float gainFactor = 2.0f; // Adjustable factor to control EQ depth
        for (auto& gain : targetEqCurve)
        {
            gain = 1.0f / (1.0f + gain * gainFactor);
        }

        // 5. Instantaneous attack: apply immediately
        eqCurve = targetEqCurve;

        // 6. Set target back to 1.0 so it fades back over the release time
        std::fill(targetEqCurve.begin(), targetEqCurve.end(), 1.0f);
    }

    void process(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        auto numSamples = inputBlock.getNumSamples();
        auto numChannels = inputBlock.getNumChannels();
        if (numChannels == 0 || numSamples == 0)
            return;

        if (inputFifo.size() < numChannels)
        {
            inputFifo.resize(numChannels, std::vector<float>(fftSize, 0.0f));
            outputFifo.resize(numChannels, std::vector<float>(fftSize, 0.0f));
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto* in = inputBlock.getChannelPointer(ch);
                auto* out = outputBlock.getChannelPointer(ch);

                const float inputSample = in[i];
                out[i] = outputFifo[ch][writePosition];
                inputFifo[ch][fftSize - hopSize + writePosition] = inputSample;
            }

            ++writePosition;

            if (writePosition == hopSize)
            {
                // Smooth the EQ curve towards the target once per hop
                for (size_t bin = 0; bin < eqCurve.size(); ++bin)
                    eqCurve[bin] = releaseFactor * eqCurve[bin] + (1.0f - releaseFactor) * targetEqCurve[bin];

                // Shift output buffer (discard samples already output)
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    std::move(outputFifo[ch].begin() + static_cast<std::ptrdiff_t>(hopSize), 
                              outputFifo[ch].end(), 
                              outputFifo[ch].begin());
                    std::fill(outputFifo[ch].begin() + static_cast<std::ptrdiff_t>(fftSize - hopSize), 
                              outputFifo[ch].end(), 
                              0.0f);
                }

                // Process each channel
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    std::fill(fftData.begin(), fftData.end(), 0.0f);
                    std::copy(inputFifo[ch].begin(), inputFifo[ch].end(), fftData.begin());

                    window.multiplyWithWindowingTable(fftData.data(), fftSize);
                    fft.performRealOnlyForwardTransform(fftData.data(), true);

                    for (size_t bin = 0; bin <= fftSize / 2; ++bin)
                    {
                        fftData[bin * 2] *= eqCurve[bin];
                        fftData[bin * 2 + 1] *= eqCurve[bin];
                    }

                    fft.performRealOnlyInverseTransform(fftData.data());

                    for (size_t n = 0; n < fftSize; ++n)
                        outputFifo[ch][n] += fftData[n];

                    std::move(inputFifo[ch].begin() + static_cast<std::ptrdiff_t>(hopSize), 
                              inputFifo[ch].end(), 
                              inputFifo[ch].begin());
                }

                writePosition = 0;
            }
        }
    }

    size_t getNumBins() const { return eqCurve.size(); }

    void getCurve(std::vector<float>& dest) const
    {
        if (dest.size() != eqCurve.size())
            dest.resize(eqCurve.size());
            
        std::copy(eqCurve.begin(), eqCurve.end(), dest.begin());
    }

private:
    const int fftOrder;
    const size_t fftSize;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    const size_t hopSize;

    std::vector<float> fftData;
    std::vector<float> eqCurve;
    std::vector<float> targetEqCurve;

    std::vector<std::vector<float>> inputFifo;
    std::vector<std::vector<float>> outputFifo;
    size_t writePosition = 0;
    double sampleRate = 0.0;
    float releaseFactor = 0.99f;
};
