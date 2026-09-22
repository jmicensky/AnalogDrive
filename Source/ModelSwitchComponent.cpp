#include "ModelSwitchComponent.h"
#include "PluginParameters.h"

ModelSwitchComponent::ModelSwitchComponent(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    for (auto* btn : { &capiButton, &amptekButton })
    {
        btn->setRadioGroupId(77);
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }
    capiButton.onClick   = [this] { setChoice(0); };
    amptekButton.onClick = [this] { setChoice(1); };

    syncButtons(static_cast<int>(*apvts.getRawParameterValue(ParamIDs::modelSwitch.getParamID())));
    apvts.addParameterListener(ParamIDs::modelSwitch.getParamID(), this);
}

ModelSwitchComponent::~ModelSwitchComponent()
{
    apvts.removeParameterListener(ParamIDs::modelSwitch.getParamID(), this);
}

void ModelSwitchComponent::parameterChanged(const juce::String&, float newValue)
{
    juce::MessageManager::callAsync([this, newValue] { syncButtons(static_cast<int>(newValue)); });
}

void ModelSwitchComponent::setChoice(int choice)
{
    if (auto* param = apvts.getParameter(ParamIDs::modelSwitch.getParamID()))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(choice)));
    syncButtons(choice);
}

void ModelSwitchComponent::syncButtons(int choice)
{
    capiButton.setToggleState  (choice == 0, juce::dontSendNotification);
    amptekButton.setToggleState(choice == 1, juce::dontSendNotification);
    if (onSelectionChanged)
        onSelectionChanged(choice);
}

// No "MODEL" label here — the "MODELING" section header above this component
// already makes it clear these two buttons are the models.
void ModelSwitchComponent::paint(juce::Graphics&) {}

void ModelSwitchComponent::resized()
{
    const int btnH = 24;
    const int y    = (getHeight() - btnH) / 2;
    capiButton  .setBounds(0,                       y, getWidth() / 2 - 2, btnH);
    amptekButton.setBounds(getWidth() / 2 + 2,       y, getWidth() / 2 - 2, btnH);
}
