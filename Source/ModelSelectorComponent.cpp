#include "ModelSelectorComponent.h"

ModelSelectorComponent::ModelSelectorComponent(juce::AudioProcessorValueTreeState& apvts,
                                               const juce::String& paramID,
                                               const juce::StringArray& itemNames)
{
    for (int i = 0; i < itemNames.size(); ++i)
        combo.addItem(itemNames[i], i + 1);

    combo.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(combo);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, paramID, combo);

    combo.onChange = [this] {
        if (onSelectionChanged)
            onSelectionChanged(combo.getSelectedItemIndex());
    };
}

void ModelSelectorComponent::resized()
{
    combo.setBounds(getLocalBounds());
}
