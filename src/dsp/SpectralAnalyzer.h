#pragma once

#include <juce_dsp/juce_dsp.h>

class SpectralAnalyzer
{
public:
    SpectralAnalyzer() : 
        fftOrder(12), // 2^12 = 4096
        fftSize(1 << fftOrder),
        fft(fftOrder),
        window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
        fftData.resize(2 * fftSize);
        spectrumData.resize(fftSize / 2 + 1);
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        fifo.setSize(1, (int)fftSize * 2);
        fifo.clear();
        writePos = 0;
    }

    void pushSamples(const juce::AudioBuffer<float>& buffer)
    {
        auto numSamples = buffer.getNumSamples();
        auto fifoSize = fifo.getNumSamples();
        
        // Simple push into circular buffer (first channel only)
        for (int i = 0; i < numSamples; ++i)
        {
            fifo.setSample(0, writePos, buffer.getSample(0, i));
            writePos = (writePos + 1) % fifoSize;
        }
    }

    void analyze()
    {
        auto fifoSize = fifo.getNumSamples();
        
        // Offset analysis window so that the current detection is closer to the beginning
        // We capture e.g. 512 samples before and the rest after.
        const int preSamples = 512;
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        
        int readPos = (writePos - preSamples + fifoSize) % fifoSize;
        for (int i = 0; i < (int)fftSize; ++i)
        {
            fftData[i] = fifo.getSample(0, (readPos + i) % fifoSize);
        }

        // Apply windowing
        window.multiplyWithWindowingTable(fftData.data(), fftSize);

        // Perform forward FFT and compute magnitude spectrum (non-negative freqs only)
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
        return fftSize;
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
