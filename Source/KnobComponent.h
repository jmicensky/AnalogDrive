#pragma once
#include <JuceHeader.h>

class KnobComponent : public juce::Component
{
public:
    KnobComponent(juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& paramID,
                  const juce::String& labelText);

    void resized() override;

    juce::Slider& getSlider() { return slider; }

private:
    juce::Slider slider  { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KnobComponent)
};
