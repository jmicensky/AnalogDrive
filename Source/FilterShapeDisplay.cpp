#include "FilterShapeDisplay.h"
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

    // The curve only needs to convey shape, not track the DAW's actual sample rate.
    constexpr float kRefSampleRate = 48000.0f;

    struct BiquadCoeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    // RBJ shelf coefficients — mirrors AudioEngine::makeLowShelf/makeHighShelf exactly
    // (duplicated here rather than shared, since AudioEngine keeps them private).
    BiquadCoeffs makeLowShelf(float freq, float gainDb, float sr, float S)
    {
        const float A    = std::pow(10.f, gainDb / 40.f);
        const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
        const float cosw = std::cos(w0), sinw = std::sin(w0), sqA = std::sqrt(A);
        const float alp  = sinw * 0.5f * std::sqrt((A + 1.f / A) * (1.f / S - 1.f) + 2.f);

        const float b0 =    A * ((A+1) - (A-1)*cosw + 2*sqA*alp);
        const float b1 =  2*A * ((A-1) - (A+1)*cosw);
        const float b2 =    A * ((A+1) - (A-1)*cosw - 2*sqA*alp);
        const float a0 =         (A+1) + (A-1)*cosw + 2*sqA*alp;
        const float a1 = -2.f * ((A-1) + (A+1)*cosw);
        const float a2 =         (A+1) + (A-1)*cosw - 2*sqA*alp;

        return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
    }

    BiquadCoeffs makeHighShelf(float freq, float gainDb, float sr, float S)
    {
        const float A    = std::pow(10.f, gainDb / 40.f);
        const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
        const float cosw = std::cos(w0), sinw = std::sin(w0), sqA = std::sqrt(A);
        const float alp  = sinw * 0.5f * std::sqrt((A + 1.f / A) * (1.f / S - 1.f) + 2.f);

        const float b0 =     A * ((A+1) + (A-1)*cosw + 2*sqA*alp);
        const float b1 = -2*A * ((A-1) + (A+1)*cosw);
        const float b2 =     A * ((A+1) + (A-1)*cosw - 2*sqA*alp);
        const float a0 =          (A+1) - (A-1)*cosw + 2*sqA*alp;
        const float a1 =  2.f * ((A-1) - (A+1)*cosw);
        const float a2 =          (A+1) - (A-1)*cosw - 2*sqA*alp;

        return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
    }

    float biquadMagnitudeDb(const BiquadCoeffs& c, float freq, float sr)
    {
        const float w  = juce::MathConstants<float>::twoPi * freq / sr;
        const float c1 = std::cos(w),     s1 = std::sin(w);
        const float c2 = std::cos(2.f*w), s2 = std::sin(2.f*w);

        const float numRe = c.b0 + c.b1*c1 + c.b2*c2;
        const float numIm = -(c.b1*s1 + c.b2*s2);
        const float denRe = 1.f + c.a1*c1 + c.a2*c2;
        const float denIm = -(c.a1*s1 + c.a2*s2);

        const float magNum = std::sqrt(numRe*numRe + numIm*numIm);
        const float magDen = std::sqrt(denRe*denRe + denIm*denIm);
        const float mag    = magNum / juce::jmax(magDen, 1e-9f);

        return 20.f * std::log10(juce::jmax(mag, 1e-6f));
    }

    // Mirrors AudioEngine's TILT gain-mapping (Pultec-style low/high shelf from the
    // cutoff knob) — including its softened shelf slope (kTiltShelfS in AudioEngine.cpp).
    float tiltMagnitudeDb(float cutoffHz, float freq, float sr)
    {
        constexpr float kTiltShelfS = 0.4f;
        const float logFc   = std::log(juce::jlimit(20.f, 15000.f, cutoffHz) / 1000.f);
        const float logMaxL = std::log(1000.f / 20.f);
        const float logMaxH = std::log(15000.f / 1000.f);
        const float maxGain = 7.f;

        if (logFc < -0.02f)
        {
            const float gDb = juce::jlimit(0.f, maxGain, (-logFc / logMaxL) * maxGain);
            return biquadMagnitudeDb(makeLowShelf(100.f, gDb, sr, kTiltShelfS), freq, sr);
        }
        if (logFc > 0.02f)
        {
            const float gDb = juce::jlimit(0.f, maxGain, (logFc / logMaxH) * maxGain);
            return biquadMagnitudeDb(makeHighShelf(7000.f, gDb, sr, kTiltShelfS), freq, sr);
        }
        return 0.f;
    }

    // Mirrors AudioEngine's Air band (12 kHz high shelf, unchanged Q).
    float airMagnitudeDb(float gainDb, float freq, float sr)
    {
        if (gainDb <= 0.01f) return 0.f;
        return biquadMagnitudeDb(makeHighShelf(12000.f, gainDb, sr, 0.7f), freq, sr);
    }

    // Standard 2-pole state-variable prototype at Q = 1/sqrt(2) — matches the default
    // resonance of JUCE's StateVariableTPTFilter used for HP/LP/BP. Analytic
    // approximation, close enough for a shape preview.
    float svfMagnitudeDb(int filterType, float cutoffHz, float freq)
    {
        constexpr float invQ = 1.0f / 0.70710678f;
        const float x      = freq / juce::jmax(1.0f, cutoffHz);
        const float x2     = x * x;
        const float denom  = juce::jmax((1.0f - x2)*(1.0f - x2) + (x*invQ)*(x*invQ), 1e-12f);

        float mag2;
        switch (filterType)
        {
            case 1:  mag2 = 1.0f / denom;                         break;  // LP
            case 2:  mag2 = (x*invQ)*(x*invQ) / denom;            break;  // BP
            default: mag2 = (x2 * x2) / denom;                    break;  // HP
        }
        return 10.0f * std::log10(juce::jmax(mag2, 1e-12f));
    }
}

FilterShapeDisplay::FilterShapeDisplay(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    setOpaque(true);

    filterType   = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::filterType.getParamID()));
    filterCutoff = *apvts.getRawParameterValue(ParamIDs::filterCutoff.getParamID());
    filterOn     = *apvts.getRawParameterValue(ParamIDs::filterEnabled.getParamID()) > 0.5f;
    airOn        = *apvts.getRawParameterValue(ParamIDs::airEnabled.getParamID()) > 0.5f;
    airGainDb    = *apvts.getRawParameterValue(ParamIDs::airGain.getParamID());
    recompute();

    apvts.addParameterListener(ParamIDs::filterType.getParamID(),    this);
    apvts.addParameterListener(ParamIDs::filterCutoff.getParamID(),  this);
    apvts.addParameterListener(ParamIDs::filterEnabled.getParamID(), this);
    apvts.addParameterListener(ParamIDs::airEnabled.getParamID(),    this);
    apvts.addParameterListener(ParamIDs::airGain.getParamID(),       this);
}

FilterShapeDisplay::~FilterShapeDisplay()
{
    apvts.removeParameterListener(ParamIDs::filterType.getParamID(),    this);
    apvts.removeParameterListener(ParamIDs::filterCutoff.getParamID(),  this);
    apvts.removeParameterListener(ParamIDs::filterEnabled.getParamID(), this);
    apvts.removeParameterListener(ParamIDs::airEnabled.getParamID(),    this);
    apvts.removeParameterListener(ParamIDs::airGain.getParamID(),       this);
}

void FilterShapeDisplay::parameterChanged(const juce::String&, float)
{
    juce::MessageManager::callAsync([this]
    {
        filterType   = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::filterType.getParamID()));
        filterCutoff = *apvts.getRawParameterValue(ParamIDs::filterCutoff.getParamID());
        filterOn     = *apvts.getRawParameterValue(ParamIDs::filterEnabled.getParamID()) > 0.5f;
        airOn        = *apvts.getRawParameterValue(ParamIDs::airEnabled.getParamID()) > 0.5f;
        airGainDb    = *apvts.getRawParameterValue(ParamIDs::airGain.getParamID());
        recompute();
        repaint();
    });
}

float FilterShapeDisplay::freqToNorm(float hz) noexcept
{
    if (hz <= 0.0f) hz = kMinFreq;
    return std::log(hz / kMinFreq) / std::log(kMaxFreq / kMinFreq);
}

void FilterShapeDisplay::recompute()
{
    constexpr int kNumPoints = 200;
    freqs.resize((size_t)kNumPoints);
    magsDb.resize((size_t)kNumPoints);

    // Air runs independently of the main filter section in the real signal chain
    // (a second biquad in series, gated only on its own enabled/gain), so its
    // contribution is added regardless of filterOn/filterType — dB-additive,
    // matching how cascaded biquad magnitudes combine.
    for (int i = 0; i < kNumPoints; ++i)
    {
        const float t  = (float) i / (float) (kNumPoints - 1);
        const float hz = kMinFreq * std::pow(kMaxFreq / kMinFreq, t);

        const float filterMag = !filterOn ? 0.f
            : (filterType == 3) ? tiltMagnitudeDb(filterCutoff, hz, kRefSampleRate)
                                 : svfMagnitudeDb(filterType, filterCutoff, hz);
        const float airMag = airOn ? airMagnitudeDb(airGainDb, hz, kRefSampleRate) : 0.f;

        freqs[(size_t) i]  = hz;
        magsDb[(size_t) i] = filterMag + airMag;
    }
}

void FilterShapeDisplay::paint(juce::Graphics& g)
{
    const auto  bounds = getLocalBounds().toFloat();
    const float left   = 4.0f;
    const float right  = bounds.getWidth()  - 4.0f;
    const float top    = 8.0f;
    const float bottom = bounds.getHeight() - 16.0f;
    const float plotW  = right - left;
    const float plotH  = bottom - top;

    g.fillAll(juce::Colour(kBgBase));

    const juce::Rectangle<float> bezel(left - 4.0f, top - 4.0f, plotW + 8.0f, plotH + 8.0f);
    g.setColour(juce::Colour(kBgPlot));
    g.fillRoundedRectangle(bezel, 10.0f);
    g.setColour(juce::Colour(0x2033ff33));
    g.drawRoundedRectangle(bezel.reduced(0.5f), 10.0f, 1.2f);

    g.setColour(juce::Colour(kBgPlot));
    g.fillRect(left, top, plotW, plotH);

    // Simplified grid: three frequency guides, one 0 dB line.
    const float freqGrid[]          = { 100.0f, 1000.0f, 10000.0f };
    const juce::String freqLabels[] = { "100", "1k", "10k" };
    g.setFont(juce::Font(juce::FontOptions().withName("Courier New").withHeight(9.0f)));
    for (int i = 0; i < 3; ++i)
    {
        const float x = left + freqToNorm(freqGrid[i]) * plotW;
        g.setColour(juce::Colour(kGreenGrid));
        g.drawVerticalLine(juce::roundToInt(x), top, bottom);
        g.setColour(juce::Colour(kGreenDim));
        g.drawText(freqLabels[i], juce::roundToInt(x) - 14, juce::roundToInt(bottom) + 2, 28, 12,
                   juce::Justification::centred);
    }

    auto dbToY = [&](float db)
    {
        const float n = (kMaxDb - db) / (kMaxDb - kMinDb);
        return top + juce::jlimit(0.0f, 1.0f, n) * plotH;
    };

    g.setColour(juce::Colour(kGreenZero));
    g.drawHorizontalLine(juce::roundToInt(dbToY(0.0f)), left, right);

    g.setColour(juce::Colour(0x3033ff33));
    g.drawRect(left, top, plotW, plotH, 1.0f);

    // ── Curve ──
    juce::Path curve;
    bool started = false;
    for (size_t i = 0; i < freqs.size(); ++i)
    {
        const float x = left + freqToNorm(freqs[i]) * plotW;
        const float y = dbToY(magsDb[i]);
        if (!started) { curve.startNewSubPath(x, y); started = true; }
        else           curve.lineTo(x, y);
    }

    if (started)
    {
        g.setColour(juce::Colour(0x1a33ff33));
        g.strokePath(curve, juce::PathStrokeType(3.5f));
        g.setColour(juce::Colour(kGreenDim));
        g.strokePath(curve, juce::PathStrokeType(1.6f));
        g.setColour(juce::Colour(kGreenPrimary));
        g.strokePath(curve, juce::PathStrokeType(1.0f));
    }

    // Active filter-type tag, top-left inside the plot — shows which stage(s) are
    // actually contributing to the curve above (main filter, Air, both, or neither).
    static const char* kTypeNames[] = { "HP", "LP", "BP", "TILT" };
    juce::String tag = filterOn ? juce::String(kTypeNames[juce::jlimit(0, 3, filterType)]) : juce::String();
    if (airOn && airGainDb > 0.01f)
        tag = tag.isEmpty() ? juce::String("AIR") : tag + " +AIR";
    g.setColour(juce::Colour(kGreenDim));
    g.setFont(juce::Font(juce::FontOptions().withName("Courier New").withHeight(9.5f)));
    g.drawText(tag, juce::roundToInt(left) + 4, juce::roundToInt(top) + 2, 60, 12,
               juce::Justification::centredLeft);

    // Dim overlay only when NEITHER stage is contributing — Air alone should still
    // read clearly even with the main filter bypassed (same treatment as
    // FilterComponent, extended to account for Air being independent of filterOn).
    if (!filterOn && !(airOn && airGainDb > 0.01f))
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(left, top, plotW, plotH);
    }
}
