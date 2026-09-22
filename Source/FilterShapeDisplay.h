#pragma once
#include <JuceHeader.h>

// Small CRT-green preview of the output filter's shape (HP/LP/BP/TILT), matching
// SpectrumDisplay's visual style. The curve is computed analytically from the
// filter's own parameters (filterType/filterCutoff) — it mirrors AudioEngine's
// filter math closely enough for a preview, without depending on AudioEngine itself.
// Also folds in the Air band's high-shelf contribution (dB-additive, since Air is
// a second biquad in series after the main filter in the real signal chain), since
// Air runs independently of filterType/filterEnabled.
class FilterShapeDisplay : public juce::Component,
                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit FilterShapeDisplay(juce::AudioProcessorValueTreeState& apvts);
    ~FilterShapeDisplay() override;

    void parameterChanged(const juce::String& paramID, float newValue) override;
    void paint(juce::Graphics&) override;

private:
    void recompute();
    static float freqToNorm(float hz) noexcept;

    juce::AudioProcessorValueTreeState& apvts;

    int   filterType   = 3;      // 0=HP 1=LP 2=BP 3=TILT — matches ParamIDs::filterType
    float filterCutoff = 1000.0f;
    bool  filterOn     = false;

    bool  airOn        = false;
    float airGainDb    = 0.0f;

    std::vector<float> freqs;
    std::vector<float> magsDb;

    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;
    static constexpr float kMinDb   = -24.0f;
    static constexpr float kMaxDb   =   9.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterShapeDisplay)
};
