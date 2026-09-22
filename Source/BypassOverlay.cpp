#include "BypassOverlay.h"

BypassOverlay::BypassOverlay(juce::AudioProcessorValueTreeState& a, const juce::String& pid)
    : apvts(a), paramIDStr(pid)
{
    setInterceptsMouseClicks(false, false);
    apvts.addParameterListener(paramIDStr, this);
}

BypassOverlay::~BypassOverlay()
{
    apvts.removeParameterListener(paramIDStr, this);
}

void BypassOverlay::parameterChanged(const juce::String&, float newValue)
{
    const bool nowBypassed = newValue > 0.5f;
    juce::MessageManager::callAsync([this, nowBypassed] { setBypassed(nowBypassed); });
}

void BypassOverlay::refreshFromParameter()
{
    setBypassed(*apvts.getRawParameterValue(paramIDStr) > 0.5f);
}

void BypassOverlay::setBypassed(bool shouldBeBypassed)
{
    bypassed = shouldBeBypassed;
    setInterceptsMouseClicks(bypassed, false);
    if (bypassed) captureAndBlur();
    else          blurredImage = juce::Image();
    repaint();
}

void BypassOverlay::resized()
{
    // Bounds changed (e.g. editor resize) — the cached snapshot, if any, is now
    // the wrong size, so recapture immediately if we're currently showing one.
    if (bypassed) captureAndBlur();
}

void BypassOverlay::captureAndBlur()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || getWidth() <= 0 || getHeight() <= 0)
        return;

    // Hide self while snapshotting the parent so we don't capture our own
    // (possibly stale) overlay content instead of the live controls beneath it.
    setVisible(false);
    juce::Image snap = parent->createComponentSnapshot(getBounds(), false, 1.0f);
    setVisible(true);

    if (snap.isValid())
    {
        // Needs a real alpha channel for the edge vignette below — component
        // snapshots normally come back as opaque RGB.
        if (snap.getFormat() != juce::Image::ARGB)
            snap = snap.convertedToFormat(juce::Image::ARGB);

        juce::ImageConvolutionKernel kernel(9);
        kernel.createGaussianBlur(4.0f);
        kernel.applyToImage(snap, snap, snap.getBounds());

        // Barely-there grey wash baked into the blurred image itself, instead
        // of a flat black rect on top — reads as "switched off" without going
        // dark/high-contrast/"digital".
        juce::Graphics g(snap);
        g.setColour(juce::Colour(0xff8c8c8c).withAlpha(0.16f));
        g.fillRect(snap.getBounds());

        // Fade the whole blurred+tinted image out near its own edges so it
        // blends into the sharp, unblurred UI around it instead of ending in
        // a hard rectangular seam.
        applyEdgeVignette(snap);
    }

    blurredImage = snap;
}

void BypassOverlay::applyEdgeVignette(juce::Image& img)
{
    const int w = img.getWidth();
    const int h = img.getHeight();
    const float feather = juce::jmin(20.0f, w * 0.3f, h * 0.3f);
    if (feather <= 0.0f)
        return;

    juce::Image::BitmapData data(img, juce::Image::BitmapData::readWrite);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float dx = (float) juce::jmin(x, w - 1 - x);
            const float dy = (float) juce::jmin(y, h - 1 - y);
            const float t  = juce::jlimit(0.0f, 1.0f, juce::jmin(dx, dy) / feather);
            const float factor = t * t * (3.0f - 2.0f * t); // smoothstep

            auto& pixel = *reinterpret_cast<juce::PixelARGB*>(data.getPixelPointer(x, y));
            pixel.multiplyAlpha(factor);
        }
    }
}

void BypassOverlay::paint(juce::Graphics& g)
{
    if (!bypassed) return;

    if (blurredImage.isValid())
        g.drawImageAt(blurredImage, 0, 0);
}
