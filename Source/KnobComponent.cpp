#include "KnobComponent.h"

KnobComponent::KnobComponent(juce::AudioProcessorValueTreeState& apvts,
                             const juce::String& paramID,
                             const juce::String& labelText)
{
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, paramID, slider);
}

void KnobComponent::resized()
{
    auto r = getLocalBounds();
    label.setBounds(r.removeFromBottom(20));
    slider.setBounds(r);
}
