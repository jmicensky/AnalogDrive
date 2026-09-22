#pragma once
#include <JuceHeader.h>

// Darkens and blurs whatever's visually beneath it whenever the given bool APVTS
// parameter is true — used to show a bypassed section's knobs/controls as
// "switched off". A live child component can't be blurred by drawing over it
// (siblings paint on top of any overlay drawn first), so this instead snapshots
// its parent's content in its own bounds, blurs that image, and shows the
// blurred image in place of the live controls — only re-snapshotting when the
// bypass state changes or this overlay is resized, not every frame.
//
// Must be added AFTER the section's own components (so it paints on top), sized
// to exactly cover that section's control area, and NOT intercept mouse clicks
// unless currently bypassed (so a bypassed section's controls can't be fiddled
// with through the blur, matching how a real hardware bypass works).
class BypassOverlay : public juce::Component,
                      public juce::AudioProcessorValueTreeState::Listener
{
public:
    BypassOverlay(juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID);
    ~BypassOverlay() override;

    void parameterChanged(const juce::String& paramID, float newValue) override;
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Call once after the editor's initial layout (setSize) has run, so bounds
    // and sibling components are valid — picks up the parameter's current value,
    // since addParameterListener doesn't fire an initial callback on its own.
    void refreshFromParameter();

private:
    void setBypassed(bool shouldBeBypassed);
    void captureAndBlur();
    static void applyEdgeVignette(juce::Image& img);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramIDStr;

    bool bypassed = false;
    juce::Image blurredImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BypassOverlay)
};
