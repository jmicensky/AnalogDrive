#pragma once
#include <JuceHeader.h>

// Algorithmic saturation "colors" — hand-designed per-sample waveshapers built from
// tanh/polynomial math, imitating an analog device's character rather than being
// measured from one. Separate system from ModelManager's hardware profiles (see
// Saturation Handoffs/SATURATION_MODES.md for the design reference).
//
// IMPORTANT: this enum's order must exactly match getModeNames()'s order below, and
// PluginProcessor sizes the modelIndex parameter assuming this list comes right
// after ModelManager's profiles — see AudioEngine::process for the index split.
enum class AlgoMode
{
    Tape, Diode, Transformer, Triode,
    Silicon, Arctan, Cubic, Rectifier,
    Wavefolder, Foldback, Chebyshev, TubeSag
};

class SaturationModeEngine
{
public:
    void prepare(double sampleRate);
    void reset();

    void setMode(AlgoMode m) { mode = m; }

    // x: input sample. driveLinear: linear pre-shaper gain (0-24 dB mapped by the
    // caller from the drive knob). channel: 0=L, 1=R — only TubeSag uses this, for
    // its per-channel envelope follower.
    float processSample(float x, float driveLinear, int channel);

    static juce::StringArray getModeNames();
    static int getNumModes() { return getModeNames().size(); }

    // Samples the mode's transfer curve y=f(x) at `numPoints` evenly-spaced x
    // values across [-domain, domain] — for the UI's curve-shape preview.
    // Domain defaults wider than unity because fold/wrap modes (Wavefolder,
    // Foldback) only show their real character once driven past ±1. TubeSag's
    // curve is dynamic (bias drifts with a live envelope follower, no single
    // fixed shape) — this returns its quiescent (resting, no recent signal)
    // curve as a representative reference, not "the" curve.
    static std::vector<float> computeTransferCurve(AlgoMode m, int numPoints = 121, float domain = 4.0f);

private:
    float shape(float x, int channel);

    static float satTape(float x);
    static float satDiode(float x);
    static float satTransformer(float x);
    static float satTriode(float x);
    static float satSilicon(float x);
    static float satArctan(float x);
    static float satCubic(float x);
    static float satRectifier(float x);
    static float satWavefolder(float x);
    static float satFoldback(float x);
    static float satChebyshev(float x);
    float        satTubeSag(float x, int channel);

    AlgoMode mode = AlgoMode::Tape;

    // Envelope follower state for TubeSag (models power-supply sag under a hot
    // transient — the bias point drifts with recent signal level).
    double sampleRate    = 44100.0;
    float  attackCoeff   = 0.0f;
    float  releaseCoeff  = 0.0f;
    float  envL = 0.0f, envR = 0.0f;
};
