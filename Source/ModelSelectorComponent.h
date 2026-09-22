#pragma once
#include <JuceHeader.h>

// Dropdown for the distortion section's algorithmic mode (ParamIDs::algoMode).
class ModelSelectorComponent : public juce::Component
{
public:
    ModelSelectorComponent(juce::AudioProcessorValueTreeState& apvts,
                           const juce::StringArray& modeNames);

    void resized() override;

    // Fired when the user picks a new mode (0-based index).
    std::function<void(int)> onSelectionChanged;

private:
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelSelectorComponent)
};
