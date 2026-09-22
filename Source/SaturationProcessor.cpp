#include "SaturationProcessor.h"
#include <algorithm>
#include <cmath>

void SaturationProcessor::prepare(double sr, int maxBlock, int /*numChannels*/)
{
    sampleRate   = sr;
    maxBlockSize = maxBlock;
    reset();
}

void SaturationProcessor::reset()
{
    olsFirL1L.reset();  olsFirL1R.reset();
    olsFirL2L.reset();  olsFirL2R.reset();

    auto clear = [](std::vector<float>& v, int& wp)
        { std::fill(v.begin(), v.end(), 0.f); wp = 0; };
    clear(voltHistL, voltWPL);  clear(voltHistR, voltWPR);
}

void SaturationProcessor::resizeFirState(std::vector<float>& hist, int& wp, int len)
{
    if (hist.size() != static_cast<size_t>(len))
    {
        hist.assign(static_cast<size_t>(len), 0.f);
        wp = 0;
    }
}

// ── LNL helpers ───────────────────────────────────────────────────────────────

/*static*/ float SaturationProcessor::applyWaveshaper(float x, const std::vector<float>& table)
{
    if (table.size() < 2) return x;
    float clamped = juce::jlimit(-1.f, 1.f, x);
    // Captured tables are built so index (table.size()/2) represents x==0 exactly
    // (verified: every model's center sample is precisely 0.0, symmetric ramp
    // either side). Scaling by (N-1)/2 instead of N/2 put x==0 halfway between
    // the two samples straddling that center point instead of on it, which for
    // an antisymmetric-but-not-perfectly-linear curve interpolated to a small
    // nonzero value — a constant DC offset out of the waveshaper for pure
    // silence in. fi is explicitly clamped to the table's valid range so ±1
    // inputs resolve to the table's real endpoints rather than extrapolating.
    const float half = static_cast<float>(table.size()) * 0.5f;
    float fi  = juce::jlimit(0.0f, static_cast<float>(table.size() - 1), (clamped + 1.f) * half);
    int   idx = juce::jlimit(0, static_cast<int>(table.size()) - 2,
                              static_cast<int>(fi));
    float frac = fi - static_cast<float>(idx);
    auto  i    = static_cast<size_t>(idx);
    return table[i] + frac * (table[i + 1] - table[i]);
}

/*static*/ void SaturationProcessor::blendWaveshapers(const LNLModel& model, float driveAmt,
                                           std::vector<float>& outWs, bool rightChannel)
{
    const auto& gm = rightChannel ? model.gainModelsR : model.gainModels;

    if (gm.size() < 2)
    {
        outWs = rightChannel ? model.waveshaperR : model.waveshaper;
        return;
    }

    // Position-based blending: driveAmt 0→1 maps uniformly across N gain models
    const int   N      = static_cast<int>(gm.size());
    const float pos    = juce::jlimit(0.0f, (float)(N - 1), driveAmt * (float)(N - 1));
    const int   lo     = juce::jlimit(0, N - 2, (int)pos);
    const int   hi     = lo + 1;
    const float lambda = pos - (float)lo;

    auto slo = static_cast<size_t>(lo);
    auto shi = static_cast<size_t>(hi);

    const auto& wsLo = gm[slo].waveshaper;
    const auto& wsHi = gm[shi].waveshaper;
    int sz = static_cast<int>(std::min(wsLo.size(), wsHi.size()));
    outWs.resize(static_cast<size_t>(sz));
    for (int i = 0; i < sz; ++i)
    {
        auto si   = static_cast<size_t>(i);
        outWs[si] = (1.f - lambda) * wsLo[si] + lambda * wsHi[si];
    }
}

// ── Volterra helpers ──────────────────────────────────────────────────────────

void SaturationProcessor::blendVolterra(const LNLModel& model, float driveAmt,
                                        std::vector<float>& outH1, std::vector<float>& outH2,
                                        bool rightChannel)
{
    const auto& gm = rightChannel ? model.gainModelsR : model.gainModels;

    // Fallback to root-level kernels if gainModels are absent or unpopulated
    if (gm.empty() || gm[0].volterraH1.empty())
    {
        outH1 = model.volterraH1;
        outH2 = model.volterraH2;
        return;
    }

    if (gm.size() == 1)
    {
        outH1 = gm[0].volterraH1;
        outH2 = gm[0].volterraH2;
        return;
    }

    // Position-based blending: driveAmt 0→1 maps uniformly across N gain models
    const int   N      = static_cast<int>(gm.size());
    const float pos    = juce::jlimit(0.0f, (float)(N - 1), driveAmt * (float)(N - 1));
    const int   lo     = juce::jlimit(0, N - 2, (int)pos);
    const int   hi     = lo + 1;
    const float lambda = pos - (float)lo;

    auto slo = static_cast<size_t>(lo);
    auto shi = static_cast<size_t>(hi);

    // Blend H1 (linear kernels)
    const auto& h1Lo = gm[slo].volterraH1;
    const auto& h1Hi = gm[shi].volterraH1;
    int h1Sz = static_cast<int>(std::min(h1Lo.size(), h1Hi.size()));
    outH1.resize(static_cast<size_t>(h1Sz));
    for (int i = 0; i < h1Sz; ++i)
    {
        auto si   = static_cast<size_t>(i);
        outH1[si] = (1.f - lambda) * h1Lo[si] + lambda * h1Hi[si];
    }

    // Blend H2 (quadratic kernels, upper-triangular packed)
    const auto& h2Lo = gm[slo].volterraH2;
    const auto& h2Hi = gm[shi].volterraH2;
    int h2Sz = static_cast<int>(std::min(h2Lo.size(), h2Hi.size()));
    outH2.resize(static_cast<size_t>(h2Sz));
    for (int i = 0; i < h2Sz; ++i)
    {
        auto si   = static_cast<size_t>(i);
        outH2[si] = (1.f - lambda) * h2Lo[si] + lambda * h2Hi[si];
    }
}

float SaturationProcessor::processVolterraOneSample(float x,
                                                    const std::vector<float>& h1,
                                                    const std::vector<float>& h2,
                                                    int M2,
                                                    std::vector<float>& inputHistory,
                                                    int& histWritePos,
                                                    float inputPadLinear)
{
    x *= inputPadLinear;
    const int histLen = static_cast<int>(inputHistory.size());

    inputHistory[static_cast<size_t>(histWritePos)] = x;

    // ── H1: linear kernel — carries the hardware's full frequency character ──
    float y = 0.f;
    if (!h1.empty())
    {
        const int h1Len = static_cast<int>(h1.size());
        for (int k = 0; k < h1Len; ++k)
        {
            int pk = histWritePos - k;
            if (pk < 0) pk += histLen;
            y += h1[static_cast<size_t>(k)] * inputHistory[static_cast<size_t>(pk)];
        }
    }

    // ── H2: quadratic kernel — harmonic content; grows as input² with drive ──
    for (int i = 0; i < M2; ++i)
    {
        int pi = histWritePos - i;
        if (pi < 0) pi += histLen;
        float xi = inputHistory[static_cast<size_t>(pi)];

        for (int j = i; j < M2; ++j)
        {
            int pj = histWritePos - j;
            if (pj < 0) pj += histLen;
            float xj = inputHistory[static_cast<size_t>(pj)];

            int flatIdx = i * M2 - i * (i - 1) / 2 + (j - i);
            y += h2[static_cast<size_t>(flatIdx)] * xi * xj;
        }
    }

    if (++histWritePos >= histLen) histWritePos = 0;

    // Soft-limit the combined H1+H2 output before the waveshaper stage.
    // tanh keeps the signal well-behaved and adds higher-order harmonics at high drive.
    return std::tanh(y);
}

// ── process ───────────────────────────────────────────────────────────────────

void SaturationProcessor::process(juce::AudioBuffer<float>& buffer,
                                  const LNLModel& model,
                                  float driveAmt)
{
    if (!model.isValid()) return;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float inputPadLin = juce::Decibels::decibelsToGain(model.inputPadDb);

    // ── Volterra path ─────────────────────────────────────────────────────────
    // Original architecture (ref §04_PLUGIN_SIGNAL_CHAIN):
    //   inputPad → H2 quadratic correction (from input history)
    //            → L1 FIR (full hardware FR, overlap-save)
    //   sum(L1_out, H2_corr) → tanh → [AudioEngine applies waveshaper + finalScale]
    if (model.modelType == LNLModel::ModelType::Volterra)
    {
        // blendVolterra fills blendedH2 (H1 is unused in this path)
        blendVolterra(model, driveAmt, blendedH1L, blendedH2L, false);
        if (model.isStereoModel())
            blendVolterra(model, driveAmt, blendedH1R, blendedH2R, true);

        const int M2      = model.volterraM2;
        const int histLen = std::max(1, M2);

        resizeFirState(voltHistL, voltWPL, histLen);
        if (numChannels > 1)
            resizeFirState(voltHistR, voltWPR, histLen);

        h2ScratchL.resize(static_cast<size_t>(numSamples));
        h2ScratchR.resize(static_cast<size_t>(numSamples));

        for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
        {
            bool isRight = (ch == 1);
            const auto& h2    = (isRight && model.isStereoModel()) ? blendedH2R : blendedH2L;
            const auto& l1fir = (isRight && !model.l1FirR.empty()) ? model.l1FirR : model.l1Fir;
            auto& voltHist    = isRight ? voltHistR : voltHistL;
            int&  voltWP      = isRight ? voltWPR   : voltWPL;
            auto& olsL1       = isRight ? olsFirL1R : olsFirL1L;
            auto& h2Scratch   = isRight ? h2ScratchR : h2ScratchL;

            float* data = buffer.getWritePointer(ch);

            // Step 1: Apply inputPad; compute H2 quadratic correction per sample
            for (int n = 0; n < numSamples; ++n)
            {
                data[n] *= inputPadLin;

                voltHist[static_cast<size_t>(voltWP)] = data[n];

                float h2val = 0.f;
                for (int i = 0; i < M2; ++i)
                {
                    int pi = voltWP - i;
                    if (pi < 0) pi += histLen;
                    const float xi = voltHist[static_cast<size_t>(pi)];

                    for (int j = i; j < M2; ++j)
                    {
                        int pj = voltWP - j;
                        if (pj < 0) pj += histLen;
                        const int flatIdx = i * M2 - i * (i - 1) / 2 + (j - i);
                        h2val += h2[static_cast<size_t>(flatIdx)] * xi * voltHist[static_cast<size_t>(pj)];
                    }
                }
                h2Scratch[static_cast<size_t>(n)] = h2val;

                if (++voltWP >= histLen) voltWP = 0;
            }

            // Step 2: Apply full L1 FIR (hardware FR) to the padded input block
            if (!l1fir.empty())
                olsL1.prepare(l1fir, numSamples);
            olsL1.process(data, numSamples);

            // Step 3: Sum L1 FIR output + H2 correction, soft-limit via tanh
            for (int n = 0; n < numSamples; ++n)
                data[n] = std::tanh(data[n] + h2Scratch[static_cast<size_t>(n)]);
        }
        return;
    }

    // ── LNL path — waveshaper+FIRs at base rate, finalScale applied here ────────
    const float inputPadMakeup = (model.inputPadDb < -0.1f) ? (1.0f / inputPadLin) : 1.0f;
    const float drivePostMul   = std::pow(2.0f, driveAmt * 2.0f - 1.0f);
    const float finalScale     = inputPadMakeup * drivePostMul;
    // Single models use drive as waveshaper pre-multiply (symmetric pre+post architecture).
    // Multi-models encode gain in the blended table — preMul stays 1.0.
    const float wsPremul = model.isMultiModel() ? 1.0f : drivePostMul;

    blendWaveshapers(model, driveAmt, blendedWsL, false);
    if (model.isStereoModel())
        blendWaveshapers(model, driveAmt, blendedWsR, true);

    for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
    {
        bool isRight = (ch == 1);
        const auto& ws    = (isRight && model.isStereoModel()) ? blendedWsR : blendedWsL;
        const auto& l1fir = (isRight && model.isStereoModel()) ? model.l1FirR : model.l1Fir;
        const auto& l2fir = (isRight && model.isStereoModel()) ? model.l2FirR : model.l2Fir;
        auto& olsL1       = isRight ? olsFirL1R : olsFirL1L;
        auto& olsL2       = isRight ? olsFirL2R : olsFirL2L;

        // Prepare OlsFir (no-op if taps pointer and block size unchanged)
        if (!l1fir.empty())
            olsL1.prepare(l1fir, numSamples);

        float* data = buffer.getWritePointer(ch);

        // Step 1: inputPad (+ drive pre-multiply for single models) → waveshaper
        for (int n = 0; n < numSamples; ++n)
            data[n] = applyWaveshaper(data[n] * (inputPadLin * wsPremul), ws);

        // Step 2: L1 FIR — OLS at buffer ≥ 256, direct-form below
        olsL1.process(data, numSamples);

        // Step 3: L2 correction FIR (same OLS/direct-form fallback as L1) + finalScale
        if (!l2fir.empty())
        {
            olsL2.prepare(l2fir, numSamples);
            olsL2.process(data, numSamples);
        }
        for (int n = 0; n < numSamples; ++n)
            data[n] *= finalScale;
    }
}

void SaturationProcessor::applyL2Fir(juce::AudioBuffer<float>& buffer, const LNLModel& model)
{
    if (!model.isValid()) return;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
    {
        bool isRight = (ch == 1);
        const auto& l2fir = (isRight && model.isStereoModel()) ? model.l2FirR : model.l2Fir;
        if (l2fir.empty()) continue;

        auto& olsL2 = isRight ? olsFirL2R : olsFirL2L;
        olsL2.prepare(l2fir, numSamples);
        olsL2.process(buffer.getWritePointer(ch), numSamples);
    }
}
