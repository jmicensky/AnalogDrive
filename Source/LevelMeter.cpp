#include "LevelMeter.h"

LevelMeter::LevelMeter()
{
    setOpaque(true);
}

float LevelMeter::dbToNorm(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

void LevelMeter::updateChannel(Channel& c, float peak, bool clip)
{
    const float db = juce::Decibels::gainToDecibels(peak, kMinDb);

    c.barDb = juce::jmax(db, c.barDb - kBarReleaseDb);

    if (db >= c.holdDb)                       { c.holdDb = db; c.holdTicks = kPeakHoldTicks; }
    else if (c.holdTicks > 0)                 --c.holdTicks;
    else                                      c.holdDb = juce::jmax(c.barDb, c.holdDb - kHoldFallDb);

    if (clip)                c.clipTicks = kClipHoldTicks;
    else if (c.clipTicks > 0) --c.clipTicks;
}

void LevelMeter::update(float peakL, float peakR, bool clipL, bool clipR)
{
    updateChannel(left,  peakL, clipL);
    updateChannel(right, peakR, clipR);

    // The number only moves when a louder peak arrives, or once per hold period
    // (then it shows the loudest peak of this frame), so it can actually be read.
    const float db = juce::Decibels::gainToDecibels(juce::jmax(peakL, peakR), kMinDb);
    if (db >= readoutDb || readoutTicks <= 0)
    {
        readoutDb    = db;
        readoutTicks = kPeakHoldTicks;
    }
    else
    {
        --readoutTicks;
    }

    repaint();
}

void LevelMeter::mouseDown(const juce::MouseEvent&)
{
    left = right = Channel{};
    readoutDb = kMinDb;
    readoutTicks = 0;
    repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a0a));

    // ── Digital peak readout — top strip, held ────────────────────────────────
    {
        const juce::Rectangle<float> readoutRect(0.0f, 0.0f, (float)getWidth(), (float)kReadoutH);
        g.setColour(juce::Colour(0xff000000));
        g.fillRect(readoutRect);
        g.setColour(juce::Colour(0xff1a2a1a));
        g.drawRect(readoutRect, 1.0f);

        const juce::String text = (readoutDb <= kMinDb + 0.05f)
            ? juce::String("-INF")
            : juce::String(readoutDb, 1);
        g.setColour(readoutDb >= 0.0f ? juce::Colour(0xffff4040) : juce::Colour(0xff33ff33));
        g.setFont(juce::Font(juce::FontOptions().withName("Courier New").withHeight(9.5f)));
        g.drawFittedText(text, readoutRect.reduced(2.0f).toNearestInt(),
                         juce::Justification::centred, 1);
    }

    const float w = (float)getWidth();
    const float gap = 2.0f;
    const float barW = (w - gap) * 0.5f;

    // ── Clip dots — dim when idle so it's clear they exist, bright when lit ─
    auto drawClipDot = [&](float x, const Channel& c)
    {
        const float d = 6.0f;
        const juce::Rectangle<float> dot(x + (barW - d) * 0.5f, kReadoutH + (kClipRowH - d) * 0.5f, d, d);
        g.setColour(c.clipTicks > 0 ? juce::Colour(0xffff2020) : juce::Colour(0xff3a1414));
        g.fillEllipse(dot);
    };
    drawClipDot(0.0f,       left);
    drawClipDot(barW + gap, right);

    // ── Bars + peak-hold lines ───────────────────────────────────────────────
    const float top = (float)(kReadoutH + kClipRowH);
    const float h   = (float)getHeight() - top;

    auto drawBar = [&](float x, const Channel& c)
    {
        const juce::Rectangle<float> full(x, top, barW, h);
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRect(full);

        const float barH = h * dbToNorm(c.barDb);
        const juce::Rectangle<float> lit(x, top + h - barH, barW, barH);

        juce::ColourGradient grad(juce::Colour(0xff33cc33), 0.0f, top + h,
                                  juce::Colour(0xffee3333), 0.0f, top, false);
        grad.addColour(0.75, juce::Colour(0xffdddd33));
        g.setGradientFill(grad);
        g.fillRect(lit);

        if (c.holdDb > kMinDb + 0.05f)
        {
            const float y = top + h - h * dbToNorm(c.holdDb);
            g.setColour(c.holdDb >= 0.0f ? juce::Colour(0xffff4040) : juce::Colour(0xffe0e0e0));
            g.fillRect(x + 1.0f, juce::jmax(top, y - 1.0f), barW - 2.0f, 2.0f);
        }

        g.setColour(juce::Colour(0xff333333));
        g.drawRect(full, 1.0f);
    };

    drawBar(0.0f,       left);
    drawBar(barW + gap, right);
}
