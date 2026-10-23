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

    // Loudest absolute sample per channel since the previous take*() call, before
    // any processing (input) and after (output), plus whether it clipped. The
    // audio thread only ever raises these; the UI timer reads-and-resets, so a
    // transient landing between two UI frames is never lost.
    //   Input clip:  a sample at or above 0 dBFS arrived.
    //   Output clip: the signal reaching the final safety soft-clipper was over
    //                0 dBFS (the soft-clipper itself keeps the output just below).
    struct MeterReading { float peakL, peakR; bool clipL, clipR; };
    MeterReading takeInputMeter()  { return take(inputMeter); }
    MeterReading takeOutputMeter() { return take(outputMeter); }

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

    struct MeterState
    {
        std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
        std::atomic<bool>  clipL { false }, clipR { false };
    };
    MeterState inputMeter, outputMeter;

    // Audio thread: raise the held peak (never lower it) and latch any clip.
    static void report(MeterState& m, float peakL, float peakR, bool clipL, bool clipR)
    {
        auto raise = [](std::atomic<float>& a, float v)
        {
            float prev = a.load(std::memory_order_relaxed);
            while (v > prev && !a.compare_exchange_weak(prev, v, std::memory_order_relaxed)) {}
        };
        raise(m.peakL, peakL);
        raise(m.peakR, peakR);
        if (clipL) m.clipL.store(true, std::memory_order_relaxed);
        if (clipR) m.clipR.store(true, std::memory_order_relaxed);
    }

    static MeterReading take(MeterState& m)
    {
        return { m.peakL.exchange(0.0f,  std::memory_order_relaxed),
                 m.peakR.exchange(0.0f,  std::memory_order_relaxed),
                 m.clipL.exchange(false, std::memory_order_relaxed),
                 m.clipR.exchange(false, std::memory_order_relaxed) };
    }

    double currentSampleRate = 44100.0;
    int    lastNumChannels   = 2;
    int    currentMaxBlock   = 512;
};
