#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "RackLookAndFeel.h"
#include "ModelSelectorComponent.h"
#include "KnobComponent.h"
#include "FilterComponent.h"
#include "SpectrumDisplay.h"
#include "FilterShapeDisplay.h"
#include "TransferCurveDisplay.h"
#include "LevelMeter.h"
#include "BypassOverlay.h"
#include "PresetBarComponent.h"

// Small LED toggle button — warm red tube-glow when ON, dark maroon when OFF.
// Ported unchanged from SaturationModel1; used here for the three section-bypass
// switches (lit = section ACTIVE, same convention as the original filter/air LEDs).
class EnableLed : public juce::Button
{
public:
    EnableLed() : juce::Button({}) { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool isHighlighted, bool /*isDown*/) override
    {
        auto b   = getLocalBounds().toFloat().reduced(0.8f);
        float cx = b.getCentreX(), cy = b.getCentreY(), r = b.getWidth() * 0.5f;
        bool  on = getToggleState();

        if (on)
        {
            juce::ColourGradient gr(
                juce::Colour(0xffffcc44), cx - r * 0.25f, cy - r * 0.30f,
                juce::Colour(0xff880800), cx + r * 0.75f, cy + r * 0.75f, true);
            gr.addColour(0.38, juce::Colour(0xffee3300));
            gr.addColour(0.70, juce::Colour(0xff991100));
            g.setGradientFill(gr);
            g.fillEllipse(b);
            g.setColour(juce::Colour(0xff660800));
            g.drawEllipse(b, 0.9f);
            float gr2 = r * 0.38f;
            g.setColour(juce::Colour(0x88ffffaa));
            g.fillEllipse(cx - r * 0.25f - gr2 * 0.5f, cy - r * 0.30f - gr2 * 0.5f, gr2, gr2);
        }
        else
        {
            g.setColour(isHighlighted ? juce::Colour(0xff2e0b0b) : juce::Colour(0xff1e0808));
            g.fillEllipse(b);
            g.setColour(juce::Colour(0xff5a1818));
            g.drawEllipse(b, 0.9f);
            g.setColour(juce::Colour(0x22882200));
            g.fillEllipse(b.reduced(2.0f));
        }
    }
};

class AnalogDriveEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    explicit AnalogDriveEditor(AnalogDriveProcessor&);
    ~AnalogDriveEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { refreshMeters(); }

    static void drawScrew(juce::Graphics& g, float cx, float cy);
    static void drawMountingHole(juce::Graphics& g, float cx, float cy);
    static void drawSectionLabel(juce::Graphics& g, const juce::String& text, int x, int y, int w);

    // Brand nameplate (silver plate, screws, engraved 2-line text) — fills the
    // distortion column's top slot, the one column without its own shape display.
    static void drawNameplate(juce::Graphics& g);

    void updateFrequencyResponseDisplay();
    void refreshMeters();

    AnalogDriveProcessor& pluginProcessor;

    RackLookAndFeel rackLookAndFeel;

    // ── Preset bar (spans the full apvts state, not any one section) ─────────
    PresetBarComponent presetBar;

    // ── Top row: shape displays ─────────────────────────────────────────────
    FilterShapeDisplay filterShapeDisplay;         // over the FILTER column
    SpectrumDisplay     frequencyResponseDisplay;  // over the MODELING column
    TransferCurveDisplay transferCurveDisplay;     // over the DISTORTION column, beside the nameplate

    // ── Distortion (algorithmic) section ────────────────────────────────────
    ModelSelectorComponent distortionSelector;
    KnobComponent distortionDriveKnob;
    KnobComponent distortionOutputKnob;
    KnobComponent distortionMixKnob;
    EnableLed     distortionBypassLed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> distortionBypassAttach;

    // ── Filter section ───────────────────────────────────────────────────────
    FilterComponent filterComponent;
    EnableLed filterEnableLed;
    EnableLed airEnableLed;
    EnableLed filterSectionBypassLed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> airEnableAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterSectionBypassAttach;

    // ── Modeling (hardware profile) section ─────────────────────────────────
    ModelSelectorComponent modelSelector;
    KnobComponent modelDriveKnob;
    KnobComponent modelOutputKnob;
    EnableLed     modelBypassLed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modelBypassAttach;


    // ── Bypass darken/blur overlays — added AFTER each section's own components
    // so they paint on top; see BypassOverlay.h. ───────────────────────────────
    BypassOverlay distortionOverlay;
    BypassOverlay filterOverlay;
    BypassOverlay modelOverlay;

    // ── Meters ────────────────────────────────────────────────────────────────
    LevelMeter inputMeter, outputMeter;

    juce::Image textureImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalogDriveEditor)
};
