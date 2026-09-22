#include "FilterComponent.h"
#include "PluginParameters.h"

FilterComponent::FilterComponent(juce::AudioProcessorValueTreeState& a)
    : apvts(a),
      cutoffKnob(a, ParamIDs::filterCutoff.getParamID(), "CUTOFF"),
      airKnob   (a, ParamIDs::airGain.getParamID(),      "AIR")
{
    cutoffKnob.getSlider().textFromValueFunction = [](double v) -> juce::String {
        return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " Hz"
                          : juce::String(v / 1000.0, 2) + " kHz";
    };
    cutoffKnob.getSlider().setRotaryParameters(
        -140.f * juce::MathConstants<float>::pi / 180.f,
         140.f * juce::MathConstants<float>::pi / 180.f, true);
    addAndMakeVisible(cutoffKnob);

    airKnob.getSlider().textFromValueFunction = [](double v) -> juce::String {
        return juce::String(v, 1) + " dB";
    };
    airKnob.getSlider().setRotaryParameters(
        -140.f * juce::MathConstants<float>::pi / 180.f,
         140.f * juce::MathConstants<float>::pi / 180.f, true);
    addAndMakeVisible(airKnob);

    for (auto* btn : { &tiltButton, &hpButton, &lpButton, &bpButton })
    {
        btn->setRadioGroupId(42);
        btn->setClickingTogglesState(true);
        addAndMakeVisible(btn);
    }
    tiltButton.onClick = [this] { setFilterType(3); };
    hpButton  .onClick = [this] { setFilterType(0); };
    lpButton  .onClick = [this] { setFilterType(1); };
    bpButton  .onClick = [this] { setFilterType(2); };

    syncTypeButtons(static_cast<int>(*apvts.getRawParameterValue(
        ParamIDs::filterType.getParamID())));

    apvts.addParameterListener(ParamIDs::filterType.getParamID(),    this);
    apvts.addParameterListener(ParamIDs::filterEnabled.getParamID(), this);
}

FilterComponent::~FilterComponent()
{
    apvts.removeParameterListener(ParamIDs::filterType.getParamID(),    this);
    apvts.removeParameterListener(ParamIDs::filterEnabled.getParamID(), this);
}

void FilterComponent::parameterChanged(const juce::String& paramID, float newValue)
{
    juce::MessageManager::callAsync([this, id = paramID, v = newValue]
    {
        if (id == ParamIDs::filterType.getParamID())
            syncTypeButtons(static_cast<int>(v));
        else
            repaint();
    });
}

void FilterComponent::setFilterType(int type)
{
    if (auto* param = apvts.getParameter(ParamIDs::filterType.getParamID()))
        param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(type)));
    syncTypeButtons(type);
}

void FilterComponent::syncTypeButtons(int type)
{
    tiltButton.setToggleState(type == 3, juce::dontSendNotification);
    hpButton  .setToggleState(type == 0, juce::dontSendNotification);
    lpButton  .setToggleState(type == 1, juce::dontSendNotification);
    bpButton  .setToggleState(type == 2, juce::dontSendNotification);
}

void FilterComponent::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff5a7878));
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
    g.drawText("FILTER", 0, 14, getWidth(), 18, juce::Justification::centred, false);

    // No dim overlay on the type buttons when the filter is off — with the
    // buttons enlarged to fill the section, a flat black rect here read as a
    // stray box that didn't blend with the faceplate texture. The button
    // LookAndFeel's own unlit styling, plus filterEnableLed/filterSectionBypassLed
    // elsewhere in the chassis, already make the on/off state clear.
}

void FilterComponent::resized()
{
    const int W        = getWidth();
    const int knobRowY = 32;
    const int knobH    = 70;

    cutoffKnob.setBounds(18,  knobRowY, 76, knobH);
    airKnob   .setBounds(112, knobRowY, 80, knobH);

    const int btnW = 28, btnGap = 3;
    const int btnsX = (W - (4 * btnW + 3 * btnGap)) / 2;

    btnRowY = knobRowY + knobH + 8;
    btnRowH = getHeight() - btnRowY - 12;   // fill the empty space below the buttons
    const int btnH = btnRowH;

    tiltButton.setBounds(btnsX,                       btnRowY, btnW, btnH);
    hpButton  .setBounds(btnsX +     (btnW + btnGap), btnRowY, btnW, btnH);
    lpButton  .setBounds(btnsX + 2 * (btnW + btnGap), btnRowY, btnW, btnH);
    bpButton  .setBounds(btnsX + 3 * (btnW + btnGap), btnRowY, btnW, btnH);
}
