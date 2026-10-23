#pragma once
#include <JuceHeader.h>

// Vertical stereo peak meter, fed by the editor's 30 Hz timer with the loudest
// peak per channel since the previous frame (see AudioEngine::takeInputMeter).
// Built to be readable rather than twitchy:
//   - bars rise instantly but fall slowly, with a peak-hold line per channel
//   - the dB readout holds the highest peak for a second and a half instead of
//     changing every frame
//   - a red clip dot above each bar stays lit for 2 s after that channel clips
// Clicking the meter clears the held peaks and clip dots.
class LevelMeter : public juce::Component
{
public:
    LevelMeter();

    // Message thread, once per editor timer tick (kTickHz).
    void update(float peakL, float peakR, bool clipL, bool clipR);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    struct Channel
    {
        float barDb  = kMinDb;   // drives the bar: instant attack, slow release
        float holdDb = kMinDb;   // peak-hold line
        int   holdTicks = 0;     // ticks left before holdDb starts falling
        int   clipTicks = 0;     // ticks left for the clip dot
    };

    void updateChannel(Channel& c, float peak, bool clip);
    float dbToNorm(float db) const noexcept;

    Channel left, right;
    float readoutDb = kMinDb;    // highest recent peak, shown as the number
    int   readoutTicks = 0;      // ticks left before the readout may drop

    static constexpr int   kTickHz            = 30;
    static constexpr float kMinDb             = -60.0f;
    static constexpr float kMaxDb             =   3.0f;
    static constexpr float kBarReleaseDb      = 20.0f / kTickHz;  // 20 dB/s fall, like a slow PPM
    static constexpr float kHoldFallDb        = 30.0f / kTickHz;  // hold line falls 30 dB/s once released
    static constexpr int   kPeakHoldTicks     = kTickHz * 3 / 2;  // 1.5 s
    static constexpr int   kClipHoldTicks     = kTickHz * 2;      // 2 s
    static constexpr int   kReadoutH          = 26;               // top strip: numeric dB readout
    static constexpr int   kClipRowH          = 10;               // clip dots, between readout and bars

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
