#pragma once

#include <vector>
#include <cmath>
#include <map>
#include <numeric>

namespace AudioUtils
{
    // Converts a frequency in Hz to its corresponding FFT bin index
    inline int frequencyToBin(const float frequency, const int fftSize, const double sampleRate)
    {
        return static_cast<int>((frequency / sampleRate) * fftSize);
    }

    // Converts an FFT bin index to its corresponding frequency in Hz
    inline float binToFrequency(const int bin, const int fftSize, const double sampleRate)
    {
        return static_cast<float>(bin * sampleRate / fftSize);
    }

    // Generates center frequencies for octave bands
    // resolution: 1 = 1 octave, 2 = 1/2 octave, 4 = 1/4 octave, etc.
    inline std::vector<float> getOctaveBandCenterFrequencies(float minFreq, float maxFreq, int bandsPerOctave)
    {
        std::vector<float> frequencies;
        float ratio = std::pow(2.0f, 1.0f / bandsPerOctave);
        float currentFreq = minFreq;

        while (currentFreq <= maxFreq)
        {
            frequencies.push_back(currentFreq);
            currentFreq *= ratio;
        }
        return frequencies;
    }
    
    // Gets the lower and upper frequency for a given center frequency
    inline std::pair<float, float> getOctaveBandLimits(float centerFreq, int bandsPerOctave)
    {
        float factor = std::pow(2.0f, 1.0f / (2.0f * bandsPerOctave));
        return { centerFreq / factor, centerFreq * factor };
    }
}
