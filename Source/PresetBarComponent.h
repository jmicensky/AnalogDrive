#pragma once
#include <JuceHeader.h>
#include "PresetManager.h"

// Slim preset browser bar: [<] [preset name dropdown, Factory/User sections] [>] [SAVE].
// Loading a preset replaces the ENTIRE plugin state (apvts.replaceState), same as
// a DAW session restore — not a per-section operation.
class PresetBarComponent : public juce::Component
{
public:
    PresetBarComponent(PresetManager& presetManager);

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void refreshCombo();
    void loadSelected();
    void stepPreset(int delta);
    void promptAndSave();

    PresetManager& presetManager;

    juce::ComboBox   presetCombo;
    juce::TextButton prevButton { "<" };
    juce::TextButton nextButton { ">" };
    juce::TextButton saveButton { "SAVE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBarComponent)
};
