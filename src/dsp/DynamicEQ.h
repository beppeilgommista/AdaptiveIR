#pragma once
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <algorithm>
#include <cmath>
#include "../Config.h"
#include "Utils.h"

class DynamicEQ
{
public:
    DynamicEQ()
        : fftOrder(AdaptiveIRConfig::FftOrder),
          fftSize(static_cast<size_t>(1u << fftOrder)),
          fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann),
          hopSize(computeHopSize(fftSize))
    {
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        writePosition = 0;

        fftData.assign(2 * fftSize, 0.0f);

        const auto numBins = (fftSize / 2) + 1;
        eqCurve.assign(numBins, 1.0f);
        targetEqCurve.assign(numBins, 1.0f);

        inputFifo.assign(spec.numChannels, std::vector<float>(fftSize, 0.0f));
        outputFifo.assign(spec.numChannels, std::vector<float>(fftSize, 0.0f));

        setRelease(AdaptiveIRConfig::ReleaseDefault);
    }

    void setRelease(float releaseTimeSeconds)
    {
        if (sampleRate <= 0.0)
        {
            releaseFactor = 0.0f;
            return;
        }

        const float clampedRelease = juce::jmax(0.001f, releaseTimeSeconds);
        const double dt = static_cast<double>(hopSize) / sampleRate;
        releaseFactor = static_cast<float>(std::exp(-dt / clampedRelease));
    }

    void generateCurve(const std::vector<float>& spectrumData,
                       int resolution,
                       int sourceFftSize,
                       double sourceSampleRate)
    {
        if (spectrumData.empty() || sourceSampleRate <= 0.0 || sampleRate <= 0.0)
            return;

        const float nyquist = static_cast<float>(sourceSampleRate * 0.5);
        const float maxFreq = juce::jmin(AdaptiveIRConfig::MaxFreqCapHz, nyquist);
        const float minFreq = AdaptiveIRConfig::MinFreqHz;

        auto centerFreqs = AudioUtils::getOctaveBandCenterFrequencies(minFreq, maxFreq, resolution);
        if (centerFreqs.empty())
            return;

        std::map<float, float> bandEnergies;
        const int spectrumSize = static_cast<int>(spectrumData.size());

        for (float centerFreq : centerFreqs)
        {
            auto limits = AudioUtils::getOctaveBandLimits(centerFreq, resolution);

            int startBin = AudioUtils::frequencyToBin(limits.first, sourceFftSize, sourceSampleRate);
            int endBin   = AudioUtils::frequencyToBin(limits.second, sourceFftSize, sourceSampleRate);

            startBin = juce::jlimit(0, spectrumSize - 1, startBin);
            endBin   = juce::jlimit(0, spectrumSize - 1, endBin);
            if (endBin < startBin)
                std::swap(startBin, endBin);

            float averageEnergy = 0.0f;
            int count = 0;

            for (int i = startBin; i <= endBin; ++i)
            {
                averageEnergy += spectrumData[static_cast<size_t>(i)];
                ++count;
            }

            if (count > 0)
                averageEnergy /= static_cast<float>(count);

            bandEnergies[centerFreq] = averageEnergy;
        }

        for (size_t i = 0; i < targetEqCurve.size(); ++i)
        {
            const float freq = AudioUtils::binToFrequency(static_cast<int>(i),
                                                         static_cast<int>(fftSize),
                                                         sampleRate);

            auto it = bandEnergies.lower_bound(freq);

            if (it == bandEnergies.end())
            {
                targetEqCurve[i] = bandEnergies.rbegin()->second;
            }
            else if (it == bandEnergies.begin())
            {
                targetEqCurve[i] = it->second;
            }
            else
            {
                auto prev = std::prev(it);
                const float freq1 = prev->first;
                const float gain1 = prev->second;
                const float freq2 = it->first;
                const float gain2 = it->second;

                const float denom = (freq2 - freq1);
                const float t = (denom != 0.0f) ? ((freq - freq1) / denom) : 0.0f;

                targetEqCurve[i] = gain1 + (gain2 - gain1) * t;
            }
        }

        const float gainFactor = AdaptiveIRConfig::EqDepthFactor;
        for (auto& gain : targetEqCurve)
            gain = 1.0f / (1.0f + gain * gainFactor);

        // attacco istantaneo
        eqCurve = targetEqCurve;

        // target torna neutro per il release
        std::fill(targetEqCurve.begin(), targetEqCurve.end(), 1.0f);
    }

    void process(const juce::dsp::ProcessContextReplacing<float>& context)
    {
        auto& inputBlock  = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();

        const size_t numSamples  = inputBlock.getNumSamples();
        const size_t numChannels = inputBlock.getNumChannels();

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
                const float* in = inputBlock.getChannelPointer(ch);
                float* out      = outputBlock.getChannelPointer(ch);

                const float inputSample = in[i];

                // out = overlap-add buffer (con latenza)
                out[i] = outputFifo[ch][writePosition];

                // push input negli ultimi hopSize campioni del frame
                inputFifo[ch][fftSize - hopSize + writePosition] = inputSample;
            }

            ++writePosition;

            if (writePosition == hopSize)
            {
                // smoothing verso il target (release)
                for (size_t bin = 0; bin < eqCurve.size(); ++bin)
                    eqCurve[bin] = releaseFactor * eqCurve[bin]
                                 + (1.0f - releaseFactor) * targetEqCurve[bin];

                // shift output buffer a sinistra di hopSize
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    std::move(outputFifo[ch].begin() + static_cast<std::ptrdiff_t>(hopSize),
                              outputFifo[ch].end(),
                              outputFifo[ch].begin());

                    std::fill(outputFifo[ch].begin() + static_cast<std::ptrdiff_t>(fftSize - hopSize),
                              outputFifo[ch].end(),
                              0.0f);
                }

                // process block per canale
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    std::fill(fftData.begin(), fftData.end(), 0.0f);
                    std::copy(inputFifo[ch].begin(), inputFifo[ch].end(), fftData.begin());

                    window.multiplyWithWindowingTable(fftData.data(), fftSize);

                    // IMPORTANTISSIMO: serve lo spettro complesso, NON magnitude-only
                    fft.performRealOnlyForwardTransform(fftData.data(), false);

                    // applica curva EQ (coppie re/imm)
                    for (size_t bin = 0; bin <= fftSize / 2; ++bin)
                    {
                        fftData[bin * 2]     *= eqCurve[bin];
                        fftData[bin * 2 + 1] *= eqCurve[bin];
                    }

                    fft.performRealOnlyInverseTransform(fftData.data());

                    // NOTA: JUCE già normalizza l'inversa (1/N) nei backend
                    // quindi NON applicare un ulteriore 1/N qui, altrimenti vai a 1/N^2 (silenzio). [3](https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/frequency/juce_FFT.cpp)
                    // Se in futuro cambi FFT engine o vuoi forzare, valuta un toggle, ma per ora: niente scaling extra.

                    for (size_t n = 0; n < fftSize; ++n)
                        outputFifo[ch][n] += fftData[n];

                    // shift input FIFO a sinistra di hopSize
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
        dest.resize(eqCurve.size());
        std::copy(eqCurve.begin(), eqCurve.end(), dest.begin());
    }

    int getLatencySamples() const
    {
        return static_cast<int>(fftSize - hopSize);
    }

private:
    static size_t computeHopSize(size_t fftSizeIn)
    {
        const float hopF = AdaptiveIRConfig::HopFraction;
        size_t hop = static_cast<size_t>(static_cast<double>(fftSizeIn) * static_cast<double>(hopF));
        if (hop < 1) hop = 1;
        if (hop > fftSizeIn) hop = fftSizeIn;
        return hop;
    }

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