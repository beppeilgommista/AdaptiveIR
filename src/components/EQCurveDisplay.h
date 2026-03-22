#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/Utils.h"

class EQCurveDisplay : public juce::Component
{
public:
    EQCurveDisplay() = default;

    void updateCurve(const std::vector<float>& newCurve, double sampleRate)
    {
        curveData = newCurve;
        currentSampleRate = sampleRate;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        // Background and border
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        if (curveData.empty() || currentSampleRate <= 0.0)
            return;

        // Draw frequency grid (logarithmic)
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            float x = frequencyToX(f);
            g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
        }

        // Draw EQ Curve Path
        juce::Path p;
        const int numPoints = 200; // Resampling the curve for smooth drawing
        const float minFreq = 40.0f;
        const float maxFreq = 20000.0f;
        
        bool first = true;
        for (int i = 0; i <= numPoints; ++i)
        {
            float proportion = (float)i / (float)numPoints;
            float freq = minFreq * std::pow(maxFreq / minFreq, proportion);
            
            float x = bounds.getX() + proportion * bounds.getWidth();
            float gain = getGainAtFrequency(freq);
            
            // Map gain (0.0 to 1.0+) to Y
            // 1.0 is the center, 0.0 is bottom. For our inverse EQ, mostly 0.0 to 1.0.
            float y = bounds.getBottom() - (gain * bounds.getHeight() * 0.8f) - (bounds.getHeight() * 0.1f);
            
            if (first) {
                p.startNewSubPath(x, y);
                first = false;
            } else {
                p.lineTo(x, y);
            }
        }

        g.setColour(juce::Colours::cyan);
        g.strokePath(p, juce::PathStrokeType(2.0f));

        // Draw labels
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(12.0f);
        g.drawText("20kHz", static_cast<int>(bounds.getRight() - 50), 5, 45, 15, juce::Justification::right);
        g.drawText("40Hz", 5, 5, 40, 15, juce::Justification::left);
    }

private:
    float frequencyToX(float freq)
    {
        const float minFreq = 40.0f;
        const float maxFreq = 20000.0f;
        float logMin = std::log10(minFreq);
        float logMax = std::log10(maxFreq);
        float logFreq = std::log10(juce::jlimit(minFreq, maxFreq, freq));
        
        return getLocalBounds().toFloat().getX() + 
               ((logFreq - logMin) / (logMax - logMin)) * getLocalBounds().toFloat().getWidth();
    }

    float getGainAtFrequency(float freq)
    {
        int bin = AudioUtils::frequencyToBin(freq, (int)curveData.size() * 2 - 2, currentSampleRate);
        bin = juce::jlimit(0, (int)curveData.size() - 1, bin);
        return curveData[static_cast<size_t>(bin)];
    }

    std::vector<float> curveData;
    double currentSampleRate = 0.0;
};
