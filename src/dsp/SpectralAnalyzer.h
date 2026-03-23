#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../Config.h"

class SpectralAnalyzer
{
public:
    SpectralAnalyzer()
        : fftOrder(AdaptiveIRConfig::FftOrder),
          fftSize(static_cast<size_t>(1u << fftOrder)),
          fft(fftOrder),
          window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        fftData.resize(2 * fftSize, 0.0f);
        spectrumData.resize(fftSize / 2 + 1, 0.0f);
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // mono FIFO, 2x fft window for circular buffer
        fifo.setSize(1, static_cast<int>(fftSize) * 2);
        fifo.clear();
        writePos = 0;
    }

    void pushSamples(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        const int fifoSize    = fifo.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            // Mono mix: average across channels
            float mono = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                mono += buffer.getSample(ch, i);
            mono /= juce::jmax(1, numChannels);

            fifo.setSample(0, writePos, mono);
            writePos = (writePos + 1) % fifoSize;
        }
    }

    void analyze()
    {
        const int fifoSize = fifo.getNumSamples();
        if (fifoSize <= 0 || sampleRate <= 0.0)
            return;

        const int preSamples = AdaptiveIRConfig::PreSamples;

        std::fill(fftData.begin(), fftData.end(), 0.0f);

        int readPos = (writePos - preSamples + fifoSize) % fifoSize;
        for (int i = 0; i < static_cast<int>(fftSize); ++i)
            fftData[static_cast<size_t>(i)] = fifo.getSample(0, (readPos + i) % fifoSize);

        window.multiplyWithWindowingTable(fftData.data(), fftSize);

        // Frequency-only gives magnitude spectrum directly in fftData[0..fftSize/2]
        fft.performFrequencyOnlyForwardTransform(fftData.data(), true);

        for (size_t i = 0; i <= fftSize / 2; ++i)
            spectrumData[i] = fftData[i];
    }

    const std::vector<float>& getSpectrumData() const
    {
        return spectrumData;
    }

    int getFftSize() const
    {
        return static_cast<int>(fftSize);
    }

    double getSampleRate() const
    {
        return sampleRate;
    }

private:
    const int fftOrder;
    const size_t fftSize;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> fftData;
    std::vector<float> spectrumData;

    juce::AudioBuffer<float> fifo;
    int writePos = 0;

    double sampleRate = 0.0;
};