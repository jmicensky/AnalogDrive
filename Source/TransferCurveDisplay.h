#pragma once
#include <JuceHeader.h>
#include "SaturationModeEngine.h"

// Small CRT-green preview of the currently-selected algorithmic distortion mode's
// waveshaping transfer curve (input -> output), matching FilterShapeDisplay's
// visual style. Unlike a measured frequency response, this is computed directly
// from the same static functions SaturationModeEngine uses at runtime — see
// SaturationModeEngine::computeTransferCurve().
class TransferCurveDisplay : public juce::Component,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit TransferCurveDisplay(juce::AudioProcessorValueTreeState& apvts);
    ~TransferCurveDisplay() override;

    void parameterChanged(const juce::String& paramID, float newValue) override;
    void paint(juce::Graphics& g) override;

private:
    void recompute();

    juce::AudioProcessorValueTreeState& apvts;
    int algoMode = 0;

    std::vector<float> ys;   // sampled output values, x implicit (evenly spaced)
    float yRange = 1.0f;     // symmetric plot range, auto-fit per mode: [-yRange, yRange]

    static constexpr float kDomain = 4.0f;  // matches SaturationModeEngine::computeTransferCurve's default

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransferCurveDisplay)
};
