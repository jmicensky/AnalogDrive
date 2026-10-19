#pragma once
#include <JuceHeader.h>
#include "ModelManager.h"
#include "SaturationProcessor.h"
#include "SaturationModeEngine.h"

// Serial chain: Distortion (algorithmic) -> Filter (+Air) -> Modeling (hardware
// profile), all inside ONE shared 2x-oversampled pass per channel. Each stage is
// independently bypassable.
//
// IMPORTANT DEVIATION FROM THE ORIGINAL SaturationModel1 ENGINE: there, the
// Volterra model's H1 FIR + H2 quadratic kernel ran at the HOST's base sample
// rate deliberately (H1 is redesigned per-rate via rebuildFirAtSampleRate, but H2
// is a fixed discrete-time kernel with no per-rate redesign path), and only the
// waveshaper lookup table ran oversampled. Wrapping the WHOLE chain in one shared
// oversampled pass means L1 gets redesigned for the oversampled rate, and the
// fixed per-sample kernels (H2, L2) are re-timed from each profile's capture
// rate to the oversampled rate by ModelManager::buildForSampleRate.
class AudioEngine
{
public:
    void initialise(double sampleRate, int maxBlockSize, int numChannels);
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    struct Params
    {
        // Distortion
        int   algoMode;
        float algoDrive;      // 0-1 -> 0-24 dB pre-shaper gain
        float algoOutputDb;
        float algoMix;        // wet/dry, this section only
        bool  algoBypass;

        // Filter
        bool  filterEnabled;
        int   filterType;
        float filterCutoff;
        bool  airEnabled;
        float airGainDb;
        bool  filterBypass;   // master bypass for the whole section

        // Modeling — always 100% wet; "amount" is drive/output only
        int   modelSwitch;    // index into ModelManager's built-in profiles
        float modelDrive;
        float modelOutputDb;
        bool  modelBypass;
    };

    void process(juce::AudioBuffer<float>& buffer, const Params& p);

    // Peak absolute sample value seen in the most recent process() call, per
    // channel, before any processing (input) and after (output) — for the UI
    // meters. Read from the message thread; written once per block on the audio
    // thread, so a torn read is at worst one block stale.
    float getInputPeakL()  const { return inputPeakL.load(std::memory_order_relaxed); }
    float getInputPeakR()  const { return inputPeakR.load(std::memory_order_relaxed); }
    float getOutputPeakL() const { return outputPeakL.load(std::memory_order_relaxed); }
    float getOutputPeakR() const { return outputPeakR.load(std::memory_order_relaxed); }

    int getLatencySamples() const;

    ModelManager& getModelManager() { return modelManager; }

private:
    void setupOversampling(int numChannels, int maxBlockSize);
    void rebuildAtOversampledRate();

    // ── RBJ biquad helpers (TILT + Air shelf) — computed for the OVERSAMPLED rate,
    // since they now run inside the shared oversampled block. ─────────────────
    struct BiquadCoeffs { float b0=1,b1=0,b2=0, a1=0,a2=0; };
    struct BiquadState  { float s1=0,s2=0; };

    static BiquadCoeffs makeLowShelf (float freq, float gainDb, float sr, float S);
    static BiquadCoeffs makeHighShelf(float freq, float gainDb, float sr, float S);
    static float        applyBiquad  (float x, BiquadState& s, const BiquadCoeffs& c);

    BiquadCoeffs tiltCoeffs;
    BiquadState  tiltStateL, tiltStateR;

    BiquadCoeffs airCoeffs;
    BiquadState  airStateL, airStateR;

    // Fixed (not user-facing) low shelf ahead of the modeling stage: strong
    // sub-40Hz content was driving the Volterra H2 term into audible low-end
    // "flub" (intermodulation mush), so this gently pulls back just the deep
    // sub-bass BEFORE it reaches the nonlinearity, rather than trying to clean
    // up artifacts after they're already generated. See kModelLowShelf* in
    // AudioEngine.cpp for the exact corner/gain/slope.
    BiquadCoeffs modelLowShelfCoeffs;
    BiquadState  modelLowShelfStateL, modelLowShelfStateR;

    int lastFilterType = -1;

    ModelManager         modelManager;
    SaturationProcessor  satProc;
    SaturationModeEngine modeEngine;

    // One mono filter/oversampler per channel — required now that each channel is
    // upsampled independently (channelOversamplers), so the HP/LP/BP filter can't
    // process both channels in one multi-channel call like the original engine did;
    // giving it its own instance per channel keeps L/R state properly separate.
    std::vector<std::unique_ptr<juce::dsp::StateVariableTPTFilter<float>>> channelFilters;
    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> channelOversamplers;

    std::vector<float> wsBlendL, wsBlendR;  // blended modeling waveshaper tables

    // No dry-delay-line/base-rate dry buffer is needed here, unlike the original
    // engine: the distortion section's Mix blends against a snapshot of the
    // UPSAMPLED signal taken immediately after upsampling (before any processing),
    // so wet and dry are already sample-aligned with no extra latency to compensate.

    std::atomic<float> inputPeakL  { 0.0f }, inputPeakR  { 0.0f };
    std::atomic<float> outputPeakL { 0.0f }, outputPeakR { 0.0f };

    double currentSampleRate = 44100.0;
    int    lastNumChannels   = 2;
    int    currentMaxBlock   = 512;
};
