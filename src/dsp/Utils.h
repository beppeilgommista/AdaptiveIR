#pragma once

#include <vector>
#include <cmath>
#include <utility>

namespace AudioUtils
{
    inline int frequencyToBin(const float frequency, const int fftSize, const double sampleRate)
    {
        return static_cast<int>((frequency / static_cast<float>(sampleRate)) * static_cast<float>(fftSize));
    }

    inline float binToFrequency(const int bin, const int fftSize, const double sampleRate)
    {
        return static_cast<float>(bin * sampleRate / fftSize);
    }

    // resolution: 1 = 1 octave, 2 = 1/2 octave, 4 = 1/4 octave, etc.
    inline std::vector<float> getOctaveBandCenterFrequencies(float minFreq, float maxFreq, int bandsPerOctave)
    {
        std::vector<float> frequencies;
        if (bandsPerOctave <= 0 || minFreq <= 0.0f || maxFreq <= minFreq)
            return frequencies;

        float ratio = std::pow(2.0f, 1.0f / static_cast<float>(bandsPerOctave));
        float currentFreq = minFreq;

        while (currentFreq <= maxFreq)
        {
            frequencies.push_back(currentFreq);
            currentFreq *= ratio;
        }

        return frequencies;
    }

    inline std::pair<float, float> getOctaveBandLimits(float centerFreq, int bandsPerOctave)
    {
        float factor = std::pow(2.0f, 1.0f / (2.0f * static_cast<float>(bandsPerOctave)));
        return { centerFreq / factor, centerFreq * factor };
    }
}