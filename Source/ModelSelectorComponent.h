#pragma once
#include <JuceHeader.h>

// Dropdown bound to a choice/int parameter — used for both the distortion
// section's algorithmic mode (ParamIDs::algoMode) and the modeling section's
// hardware profile (ParamIDs::modelSwitch).
class ModelSelectorComponent : public juce::Component
{
public:
    ModelSelectorComponent(juce::AudioProcessorValueTreeState& apvts,
                           const juce::String& paramID,
                           const juce::StringArray& itemNames);

    void resized() override;

    // Fired whenever the selection changes (0-based index) — by the user OR by
    // host automation/state restore, since the attachment updates the combo
    // with a synchronous notification.
    std::function<void(int)> onSelectionChanged;

private:
    juce::ComboBox combo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelSelectorComponent)
};
