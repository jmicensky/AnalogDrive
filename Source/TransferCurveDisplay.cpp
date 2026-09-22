#include "TransferCurveDisplay.h"
#include "PluginParameters.h"
#include <cmath>

namespace
{
    constexpr uint32_t kGreenPrimary = 0xff33ff33;
    constexpr uint32_t kGreenDim     = 0x7a33ff33;
    constexpr uint32_t kGreenGrid    = 0x1233ff33;
    constexpr uint32_t kGreenZero    = 0x4933ff33;
    constexpr uint32_t kBgBase       = 0xff1f1f1f;
    constexpr uint32_t kBgPlot       = 0xff1a1a1a;
}

TransferCurveDisplay::TransferCurveDisplay(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    setOpaque(true);
    algoMode = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::algoMode.getParamID()));
    recompute();
    apvts.addParameterListener(ParamIDs::algoMode.getParamID(), this);
}

TransferCurveDisplay::~TransferCurveDisplay()
{
    apvts.removeParameterListener(ParamIDs::algoMode.getParamID(), this);
}

void TransferCurveDisplay::parameterChanged(const juce::String&, float newValue)
{
    juce::MessageManager::callAsync([this, newValue]
    {
        algoMode = static_cast<int>(newValue);
        recompute();
        repaint();
    });
}

void TransferCurveDisplay::recompute()
{
    ys = SaturationModeEngine::computeTransferCurve(static_cast<AlgoMode>(algoMode), 121, kDomain);

    // Auto-fit the Y range per mode, symmetric around 0 — some modes (Rectifier,
    // Chebyshev) swing far past ±1 at this domain, others (Cubic) never leave
    // ±0.7; a shared fixed range would make most modes look like a flat line.
    float peak = 0.01f;
    for (float y : ys) peak = std::max(peak, std::abs(y));
    yRange = peak * 1.15f;
}

void TransferCurveDisplay::paint(juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float left   = 6.0f;
    const float right  = bounds.getWidth()  - 6.0f;
    const float top    = 8.0f;
    const float bottom = bounds.getHeight() - 8.0f;
    const float plotW  = right - left;
    const float plotH  = bottom - top;

    g.fillAll(juce::Colour(kBgBase));

    const juce::Rectangle<float> bezel(left - 4.0f, top - 4.0f, plotW + 8.0f, plotH + 8.0f);
    g.setColour(juce::Colour(kBgPlot));
    g.fillRoundedRectangle(bezel, 8.0f);
    g.setColour(juce::Colour(0x2033ff33));
    g.drawRoundedRectangle(bezel.reduced(0.5f), 8.0f, 1.0f);

    g.setColour(juce::Colour(kBgPlot));
    g.fillRect(left, top, plotW, plotH);

    // Zero-crossing axes — the reference every one of these curves is judged against.
    g.setColour(juce::Colour(kGreenZero));
    g.drawHorizontalLine(juce::roundToInt(top + plotH * 0.5f), left, right);
    g.drawVerticalLine  (juce::roundToInt(left + plotW * 0.5f), top, bottom);

    // Light grid at the quarter marks
    g.setColour(juce::Colour(kGreenGrid));
    for (float f : { 0.25f, 0.75f })
    {
        g.drawHorizontalLine(juce::roundToInt(top + plotH * f), left, right);
        g.drawVerticalLine  (juce::roundToInt(left + plotW * f), top, bottom);
    }

    g.setColour(juce::Colour(0x3033ff33));
    g.drawRect(left, top, plotW, plotH, 1.0f);

    // ── Curve ── x implicit (evenly spaced across the sampled domain), y auto-fit
    juce::Path curve;
    bool started = false;
    const int n = (int)ys.size();
    for (int i = 0; i < n; ++i)
    {
        const float x = left + plotW * (float)i / (float)(n - 1);
        const float yNorm = juce::jlimit(0.0f, 1.0f, 0.5f - 0.5f * (ys[(size_t)i] / yRange));
        const float y = top + yNorm * plotH;
        if (!started) { curve.startNewSubPath(x, y); started = true; }
        else           curve.lineTo(x, y);
    }

    if (started)
    {
        g.setColour(juce::Colour(0x1a33ff33));
        g.strokePath(curve, juce::PathStrokeType(3.0f));
        g.setColour(juce::Colour(kGreenDim));
        g.strokePath(curve, juce::PathStrokeType(1.4f));
        g.setColour(juce::Colour(kGreenPrimary));
        g.strokePath(curve, juce::PathStrokeType(1.0f));
    }

    // Mode name tag, top-left inside the plot
    const auto names = SaturationModeEngine::getModeNames();
    const juce::String tag = (algoMode >= 0 && algoMode < names.size()) ? names[algoMode] : juce::String();
    g.setColour(juce::Colour(kGreenDim));
    g.setFont(juce::Font(juce::FontOptions().withName("Courier New").withHeight(9.0f)));
    g.drawText(tag, juce::roundToInt(left) + 4, juce::roundToInt(top) + 2, (int)plotW - 8, 12,
               juce::Justification::centredLeft);
}
