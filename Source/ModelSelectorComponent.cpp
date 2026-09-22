#include "ModelSelectorComponent.h"
#include "PluginParameters.h"

ModelSelectorComponent::ModelSelectorComponent(juce::AudioProcessorValueTreeState& apvts,
                                               const juce::StringArray& modeNames)
{
    for (int i = 0; i < modeNames.size(); ++i)
        combo.addItem(modeNames[i], i + 1);

    combo.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(combo);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::algoMode.getParamID(), combo);

    combo.onChange = [this] {
        if (onSelectionChanged)
            onSelectionChanged(combo.getSelectedItemIndex());
    };
}

void ModelSelectorComponent::resized()
{
    combo.setBounds(getLocalBounds());
}
