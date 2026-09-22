#pragma once
#include <JuceHeader.h>

// Simple vertical stereo peak meter. The audio thread calls pushLevels() once per
// block (lock-free); a 30Hz Timer polls and repaints on the message thread.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter();
    ~LevelMeter() override;

    // Call from the audio thread with the peak absolute sample value seen in the
    // current block, per channel (mono callers can pass the same value twice).
    void pushLevels(float peakL, float peakR);

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;
    float dbToNorm(float db) const noexcept;

    std::atomic<float> peakLAtomic { 0.0f };
    std::atomic<float> peakRAtomic { 0.0f };

    float displayL = 0.0f, displayR = 0.0f;  // smoothed dB (drives the bar), message-thread only
    float instDb   = kMinDb;                 // raw peak dB this tick (drives the digital readout) — no hold/decay

    static constexpr float kMinDb        = -60.0f;
    static constexpr float kMaxDb        =   3.0f;
    static constexpr float kDecayDbPerTick = 2.5f;  // at 30Hz, ~75 dB/s release
    static constexpr int   kReadoutH     = 26;      // top strip reserved for the numeric dB readout

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
