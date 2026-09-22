#pragma once
#include <JuceHeader.h>

// Two-way hardware-style selector switch for the modeling section (ParamIDs::modelSwitch):
// CAPI (0) vs Amptek (1). Same manual radio-button-pair wiring FilterComponent uses
// for its HP/LP/BP/TILT buttons — ButtonAttachment only covers bool params, and this
// is a 2-value AudioParameterInt.
class ModelSwitchComponent : public juce::Component,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ModelSwitchComponent(juce::AudioProcessorValueTreeState& apvts);
    ~ModelSwitchComponent() override;

    void parameterChanged(const juce::String& paramID, float newValue) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Fired whenever the choice changes — by click OR by host automation/state
    // restore (parameterChanged also triggers it), so the editor's frequency-
    // response display can always be kept in sync.
    std::function<void(int)> onSelectionChanged;

private:
    void setChoice(int choice);
    void syncButtons(int choice);

    juce::AudioProcessorValueTreeState& apvts;

    juce::TextButton capiButton   { "CAPI" };
    juce::TextButton amptekButton { "AMPTEK" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelSwitchComponent)
};
