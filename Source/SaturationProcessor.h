#pragma once
#include <JuceHeader.h>
#include "LNLModel.h"
#include "OlsFir.h"

class SaturationProcessor
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Volterra path: inputPad → (L1 FIR ‖ H2) → sum → tanh. Waveshaper applied by AudioEngine.
    // LNL path: inputPad → waveshaper → L1 FIR (OLS) → L2 FIR → finalScale, all at base rate.
    void process(juce::AudioBuffer<float>& buffer,
                 const LNLModel& model,
                 float driveAmt);

    // Apply L2 FIR correction filter. Called by AudioEngine after the oversampled waveshaper
    // step in the Volterra path (for LNL this is already handled inside process()).
    void applyL2Fir(juce::AudioBuffer<float>& buffer, const LNLModel& model);

    // Public so AudioEngine can call them for the oversampled waveshaper step
    static float applyWaveshaper(float x, const std::vector<float>& table);
    static void  blendWaveshapers(const LNLModel& model, float driveAmt,
                                  std::vector<float>& outWs, bool rightChannel = false);

private:
    void blendVolterra(const LNLModel& model, float driveAmt,
                       std::vector<float>& outH1, std::vector<float>& outH2,
                       bool rightChannel = false);

    float processVolterraOneSample(float x,
                                   const std::vector<float>& h1,
                                   const std::vector<float>& h2,
                                   int M2,
                                   std::vector<float>& inputHistory,
                                   int& histWritePos,
                                   float inputPadLinear);

    void resizeFirState(std::vector<float>& hist, int& writePos, int firLen);

    // ── LNL state ─────────────────────────────────────────────────────────────
    // L1 FIR: overlap-save (OLS) with direct-form fallback
    OlsFir olsFirL1L, olsFirL1R;

    // L2 correction FIR: same OLS/direct-form-fallback approach as L1. At
    // 279-559 taps this was previously always run as naive direct-form
    // convolution (no OLS), making it the single biggest per-instance CPU cost
    // once L1 was already OLS-accelerated — this brings it in line.
    OlsFir olsFirL2L, olsFirL2R;

    // Blended waveshaper tables (LNL path)
    std::vector<float> blendedWsL, blendedWsR;

    // ── Volterra state ────────────────────────────────────────────────────────
    std::vector<float> voltHistL, voltHistR;   // input history for H2 quadratic kernel
    int voltWPL = 0, voltWPR = 0;

    std::vector<float> blendedH1L, blendedH1R; // populated but unused in playback path
    std::vector<float> blendedH2L, blendedH2R;

    std::vector<float> h2ScratchL, h2ScratchR; // per-block H2 correction values

    double sampleRate   = 44100.0;
    int    maxBlockSize = 512;
};
