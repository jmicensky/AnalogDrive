#pragma once
#include <JuceHeader.h>
#include "KnobComponent.h"

class FilterComponent : public juce::Component,
                        public juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterComponent(juce::AudioProcessorValueTreeState& apvts);
    ~FilterComponent() override;

    void parameterChanged(const juce::String& paramID, float newValue) override;
    void paint  (juce::Graphics& g) override;
    void resized() override;

private:
    void setFilterType(int type);
    void syncTypeButtons(int type);

    juce::AudioProcessorValueTreeState& apvts;

    KnobComponent    cutoffKnob;
    KnobComponent    airKnob;
    juce::TextButton tiltButton{ "TILT" };
    juce::TextButton hpButton  { "HP" };
    juce::TextButton lpButton  { "LP" };
    juce::TextButton bpButton  { "BP" };

    int btnRowY = 0, btnRowH = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterComponent)
};
