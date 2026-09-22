#include "PresetBarComponent.h"

PresetBarComponent::PresetBarComponent(PresetManager& pm) : presetManager(pm)
{
    addAndMakeVisible(prevButton);
    addAndMakeVisible(nextButton);
    addAndMakeVisible(presetCombo);
    addAndMakeVisible(saveButton);

    presetCombo.setJustificationType(juce::Justification::centred);
    refreshCombo();

    presetCombo.onChange = [this] { loadSelected(); };
    prevButton.onClick   = [this] { stepPreset(-1); };
    nextButton.onClick   = [this] { stepPreset(1); };
    saveButton.onClick   = [this] { promptAndSave(); };
}

void PresetBarComponent::refreshCombo()
{
    presetCombo.clear(juce::dontSendNotification);

    int itemId = 1;
    bool sawFactory = false, sawUser = false;
    const int n = presetManager.getNumPresets();

    for (int i = 0; i < n; ++i)
    {
        const bool isFactory = presetManager.isFactoryPreset(i);
        if (isFactory && !sawFactory)       { presetCombo.addSectionHeading("Factory"); sawFactory = true; }
        else if (!isFactory && !sawUser)    { presetCombo.addSectionHeading("User");    sawUser    = true; }
        presetCombo.addItem(presetManager.getPresetName(i), itemId++);
    }

    const int cur = presetManager.getCurrentIndex();
    if (cur >= 0)
        presetCombo.setSelectedItemIndex(cur, juce::dontSendNotification);
    else
        presetCombo.setText(n > 0 ? "(select a preset)" : "(no presets saved yet)", juce::dontSendNotification);
}

void PresetBarComponent::loadSelected()
{
    presetManager.loadPreset(presetCombo.getSelectedItemIndex());
}

void PresetBarComponent::stepPreset(int delta)
{
    const int n = presetManager.getNumPresets();
    if (n == 0) return;
    const int cur  = presetManager.getCurrentIndex();
    const int next = (cur < 0) ? 0 : (cur + delta + n) % n;
    presetManager.loadPreset(next);
    refreshCombo();
}

void PresetBarComponent::promptAndSave()
{
    auto* aw = new juce::AlertWindow("Save Preset", "Enter a name for this preset:",
                                     juce::MessageBoxIconType::NoIcon);
    const int cur = presetManager.getCurrentIndex();
    aw->addTextEditor("name", cur >= 0 ? presetManager.getPresetName(cur) : juce::String());
    aw->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result)
    {
        std::unique_ptr<juce::AlertWindow> ownedAw(aw);
        if (result == 1)
        {
            presetManager.savePreset(aw->getTextEditorContents("name"));
            refreshCombo();
        }
    }));
}

void PresetBarComponent::resized()
{
    auto b = getLocalBounds().reduced(2);
    constexpr int arrowW = 22, saveW = 56, gap = 4;

    prevButton.setBounds(b.removeFromLeft(arrowW));
    b.removeFromLeft(gap);
    saveButton.setBounds(b.removeFromRight(saveW));
    b.removeFromRight(gap);
    nextButton.setBounds(b.removeFromRight(arrowW));
    b.removeFromRight(gap);
    presetCombo.setBounds(b);
}

void PresetBarComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff141414));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff2a2a2a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}
