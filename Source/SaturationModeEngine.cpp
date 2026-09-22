#include "SaturationModeEngine.h"
#include <cmath>

// ── Reference modes (Saturation Handoffs/SATURATION_MODES.md) ──────────────────
// All built from three moves: asymmetry (different constant per sign of x),
// biasing (shift x before the tanh, subtract the bias's own tanh back out to keep
// the origin fixed), and blending two tanh calls at different drive amounts.

float SaturationModeEngine::satTape(float x)
{
    return 0.95f * std::tanh(0.85f * x) + 0.05f * std::tanh(2.0f * x);
}

float SaturationModeEngine::satDiode(float x)
{
    return x >= 0.0f ? std::tanh(1.6f * x) : std::tanh(0.9f * x);
}

float SaturationModeEngine::satTransformer(float x)
{
    const float c = x - 0.08f * x * x * x;
    return 0.85f * std::tanh(1.3f * x) + 0.15f * std::tanh(c);
}

float SaturationModeEngine::satTriode(float x)
{
    constexpr float bias = 0.25f;
    return std::tanh(1.1f * (x + bias)) - std::tanh(1.1f * bias);
}

// ── New modes ────────────────────────────────────────────────────────────────

// Silicon: steep symmetric tanh, no blend — brighter/harder than Diode's
// asymmetric germanium-style curve.
float SaturationModeEngine::satSilicon(float x)
{
    return std::tanh(4.0f * x);
}

// Arctan: same soft-clip family as tanh but a fatter shoulder and different
// high-order harmonic falloff — the classic op-amp-clip alternative to tanh.
float SaturationModeEngine::satArctan(float x)
{
    return (2.0f / juce::MathConstants<float>::pi) * std::atan(2.5f * x);
}

// Cubic soft-clip: the textbook smoothed hard clip (x - x^3/3, flat beyond ±1) —
// flatter/more clinical than any tanh-based curve.
float SaturationModeEngine::satCubic(float x)
{
    if (std::abs(x) >= 1.0f)
        return std::copysign(2.0f / 3.0f, x);
    return x - (x * x * x) / 3.0f;
}

// Rectifier / octave fuzz: blends a tanh base with a rectified (squared, sign-
// preserved) term — the squaring is what generates the octave-up harmonic that
// none of the pure-tanh modes touch.
float SaturationModeEngine::satRectifier(float x)
{
    return 0.7f * std::tanh(1.4f * x) + 0.3f * std::copysign(x * x, x);
}

// Wavefolder: folds the signal back on itself past a threshold instead of
// clamping it — denser, more inharmonic than any clip-based mode.
float SaturationModeEngine::satWavefolder(float x)
{
    return std::sin(x);
}

// Foldback: triangle-wave reflection off both rails, repeated — a more extreme
// wavefolder variant with a metallic, near-ring-mod character at high drive.
float SaturationModeEngine::satFoldback(float x)
{
    constexpr float T = 1.0f, period = 4.0f * T;
    float xm = std::fmod(x + T, period);
    if (xm < 0.0f) xm += period;
    return std::abs(xm - 2.0f * T) - T;
}

// Chebyshev: T3(x) = 4x^3 - 3x injects a pure 3rd harmonic by design, rather than
// an emergent mix of harmonics from a clipping shape.
float SaturationModeEngine::satChebyshev(float x)
{
    const float c = juce::jlimit(-1.0f, 1.0f, x);
    return 4.0f * c * c * c - 3.0f * c;
}

// Tube sag: Triode's biased tanh, but the bias point drifts upward under a hot
// transient (envelope follower on |x|) and relaxes back down — imitates
// power-supply sag, so the curve "breathes" rather than being fixed.
float SaturationModeEngine::satTubeSag(float x, int channel)
{
    float& env = (channel == 0) ? envL : envR;
    const float rect  = std::abs(x);
    const float coeff = (rect > env) ? attackCoeff : releaseCoeff;
    env += coeff * (rect - env);

    const float bias = 0.25f + 0.15f * env;
    return std::tanh(1.1f * (x + bias)) - std::tanh(1.1f * bias);
}

// ── Engine ───────────────────────────────────────────────────────────────────

void SaturationModeEngine::prepare(double sr)
{
    sampleRate = sr;
    constexpr float attackMs = 5.0f, releaseMs = 100.0f;
    attackCoeff  = 1.0f - std::exp(-1.0f / (0.001f * attackMs  * (float)sampleRate));
    releaseCoeff = 1.0f - std::exp(-1.0f / (0.001f * releaseMs * (float)sampleRate));
    reset();
}

void SaturationModeEngine::reset()
{
    envL = envR = 0.0f;
}

float SaturationModeEngine::shape(float x, int channel)
{
    switch (mode)
    {
        case AlgoMode::Tape:        return satTape(x);
        case AlgoMode::Diode:       return satDiode(x);
        case AlgoMode::Transformer: return satTransformer(x);
        case AlgoMode::Triode:      return satTriode(x);
        case AlgoMode::Silicon:     return satSilicon(x);
        case AlgoMode::Arctan:      return satArctan(x);
        case AlgoMode::Cubic:       return satCubic(x);
        case AlgoMode::Rectifier:   return satRectifier(x);
        case AlgoMode::Wavefolder:  return satWavefolder(x);
        case AlgoMode::Foldback:    return satFoldback(x);
        case AlgoMode::Chebyshev:   return satChebyshev(x);
        case AlgoMode::TubeSag:     return satTubeSag(x, channel);
    }
    return x;
}

float SaturationModeEngine::processSample(float x, float driveLinear, int channel)
{
    x = juce::jlimit(-10.0f, 10.0f, x * driveLinear);
    return shape(x, channel);
}

juce::StringArray SaturationModeEngine::getModeNames()
{
    // Order must match AlgoMode's declaration order exactly.
    return { "Tape", "Diode", "Transformer", "Triode",
             "Silicon", "Arctan", "Cubic", "Rectifier",
             "Wavefolder", "Foldback", "Chebyshev", "Tube Sag" };
}

std::vector<float> SaturationModeEngine::computeTransferCurve(AlgoMode m, int numPoints, float domain)
{
    std::vector<float> ys(static_cast<size_t>(numPoints));
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = -domain + 2.0f * domain * (float)i / (float)(numPoints - 1);
        float y;
        switch (m)
        {
            case AlgoMode::Tape:        y = satTape(x);        break;
            case AlgoMode::Diode:       y = satDiode(x);       break;
            case AlgoMode::Transformer: y = satTransformer(x); break;
            case AlgoMode::Triode:      y = satTriode(x);      break;
            case AlgoMode::Silicon:     y = satSilicon(x);     break;
            case AlgoMode::Arctan:      y = satArctan(x);      break;
            case AlgoMode::Cubic:       y = satCubic(x);       break;
            case AlgoMode::Rectifier:   y = satRectifier(x);   break;
            case AlgoMode::Wavefolder:  y = satWavefolder(x);  break;
            case AlgoMode::Foldback:    y = satFoldback(x);    break;
            case AlgoMode::Chebyshev:   y = satChebyshev(x);   break;
            case AlgoMode::TubeSag:
            {
                // Quiescent (env=0) reference curve — see header comment.
                constexpr float bias = 0.25f;
                y = std::tanh(1.1f * (x + bias)) - std::tanh(1.1f * bias);
                break;
            }
            default: y = x; break;
        }
        ys[static_cast<size_t>(i)] = y;
    }
    return ys;
}
