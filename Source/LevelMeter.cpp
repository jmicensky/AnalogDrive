#include "LevelMeter.h"

LevelMeter::LevelMeter()
{
    setOpaque(true);
    startTimerHz(30);
}

LevelMeter::~LevelMeter()
{
    stopTimer();
}

void LevelMeter::pushLevels(float peakL, float peakR)
{
    // "Hold the loudest peak seen since the last UI poll" — the timer callback
    // reads-and-resets, so this only ever needs to grow between polls.
    float prevL = peakLAtomic.load(std::memory_order_relaxed);
    while (peakL > prevL && !peakLAtomic.compare_exchange_weak(prevL, peakL, std::memory_order_relaxed)) {}
    float prevR = peakRAtomic.load(std::memory_order_relaxed);
    while (peakR > prevR && !peakRAtomic.compare_exchange_weak(prevR, peakR, std::memory_order_relaxed)) {}
}

float LevelMeter::dbToNorm(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

void LevelMeter::timerCallback()
{
    const float peakL = peakLAtomic.exchange(0.0f, std::memory_order_relaxed);
    const float peakR = peakRAtomic.exchange(0.0f, std::memory_order_relaxed);

    const float dbL = juce::Decibels::gainToDecibels(peakL, kMinDb);
    const float dbR = juce::Decibels::gainToDecibels(peakR, kMinDb);

    // Instant attack, gentle release, so transients are visible but the meter
    // doesn't flicker to the floor between blocks.
    displayL = juce::jmax(dbL, displayL - kDecayDbPerTick);
    displayR = juce::jmax(dbR, displayR - kDecayDbPerTick);

    // Digital readout: raw instantaneous peak, no hold/decay — updates every
    // tick with whatever the true peak was since the last poll.
    instDb = juce::jmax(dbL, dbR);

    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));

    // ── Digital peak readout — top strip, instantaneous (no hold/decay) ───────
    {
        const juce::Rectangle<float> readoutRect(0.0f, 0.0f, (float)getWidth(), (float)kReadoutH);
        g.setColour(juce::Colour(0xff000000));
        g.fillRect(readoutRect);
        g.setColour(juce::Colour(0xff1a2a1a));
        g.drawRect(readoutRect, 1.0f);

        const juce::String text = (instDb <= kMinDb + 0.05f)
            ? juce::String("-INF")
            : juce::String(instDb, 1);
        g.setColour(juce::Colour(0xff33ff33));
        g.setFont(juce::Font(juce::FontOptions().withName("Courier New").withHeight(9.5f)));
        g.drawFittedText(text, readoutRect.reduced(2.0f).toNearestInt(),
                         juce::Justification::centred, 1);
    }

    const float w = (float)getWidth(), h = (float)getHeight() - (float)kReadoutH;
    const float top = (float)kReadoutH;
    const float gap = 2.0f;
    const float barW = (w - gap) * 0.5f;

    auto drawBar = [&](float x, float norm)
    {
        const juce::Rectangle<float> full(x, top, barW, h);
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(full);

        const float barH = h * norm;
        const juce::Rectangle<float> lit(x, top + h - barH, barW, barH);

        juce::ColourGradient grad(juce::Colour(0xff33cc33), 0.0f, top + h,
                                  juce::Colour(0xffee3333), 0.0f, top, false);
        grad.addColour(0.75, juce::Colour(0xffdddd33));
        g.setGradientFill(grad);
        g.fillRect(lit);

        g.setColour(juce::Colour(0xff333333));
        g.drawRect(full, 1.0f);
    };

    drawBar(0.0f,       dbToNorm(displayL));
    drawBar(barW + gap, dbToNorm(displayR));
}
