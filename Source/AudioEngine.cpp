#include "AudioEngine.h"
#include "PluginParameters.h"
#include <cmath>
#include <array>

static juce::dsp::StateVariableTPTFilterType mapFilterType(int t)
{
    switch (t)
    {
        case 1:  return juce::dsp::StateVariableTPTFilterType::lowpass;
        case 2:  return juce::dsp::StateVariableTPTFilterType::bandpass;
        default: return juce::dsp::StateVariableTPTFilterType::highpass;
    }
}

// ── RBJ biquad shelf helpers ──────────────────────────────────────────────────

AudioEngine::BiquadCoeffs AudioEngine::makeLowShelf(float freq, float gainDb, float sr, float S)
{
    const float A    = std::pow(10.f, gainDb / 40.f);
    const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
    const float cosw = std::cos(w0), sinw = std::sin(w0), sqA = std::sqrt(A);
    const float alp  = sinw * 0.5f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);

    const float b0 =    A * ((A+1) - (A-1)*cosw + 2*sqA*alp);
    const float b1 =  2*A * ((A-1) - (A+1)*cosw);
    const float b2 =    A * ((A+1) - (A-1)*cosw - 2*sqA*alp);
    const float a0 =         (A+1) + (A-1)*cosw + 2*sqA*alp;
    const float a1 = -2.f * ((A-1) + (A+1)*cosw);
    const float a2 =         (A+1) + (A-1)*cosw - 2*sqA*alp;

    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

AudioEngine::BiquadCoeffs AudioEngine::makeHighShelf(float freq, float gainDb, float sr, float S)
{
    const float A    = std::pow(10.f, gainDb / 40.f);
    const float w0   = juce::MathConstants<float>::twoPi * freq / sr;
    const float cosw = std::cos(w0), sinw = std::sin(w0), sqA = std::sqrt(A);
    const float alp  = sinw * 0.5f * std::sqrt((A + 1.f/A) * (1.f/S - 1.f) + 2.f);

    const float b0 =     A * ((A+1) + (A-1)*cosw + 2*sqA*alp);
    const float b1 = -2*A * ((A-1) + (A+1)*cosw);
    const float b2 =     A * ((A+1) + (A-1)*cosw - 2*sqA*alp);
    const float a0 =          (A+1) - (A-1)*cosw + 2*sqA*alp;
    const float a1 =  2.f * ((A-1) - (A+1)*cosw);
    const float a2 =          (A+1) - (A-1)*cosw - 2*sqA*alp;

    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

float AudioEngine::applyBiquad(float x, BiquadState& s, const BiquadCoeffs& c)
{
    const float y = c.b0*x + s.s1;
    s.s1 = c.b1*x - c.a1*y + s.s2;
    s.s2 = c.b2*x - c.a2*y;
    return y;
}

// ── Setup ────────────────────────────────────────────────────────────────────

void AudioEngine::setupOversampling(int numChannels, int maxBlockSize, bool qualityHF)
{
    using FT = juce::dsp::Oversampling<float>::FilterType;
    const auto filterType = qualityHF ? FT::filterHalfBandFIREquiripple
                                      : FT::filterHalfBandPolyphaseIIR;
    const bool maxQuality = qualityHF;
    const bool intLatency = !qualityHF;

    channelOversamplers.clear();
    channelFilters.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto os = std::make_unique<juce::dsp::Oversampling<float>>(
            1u, 1u, filterType, maxQuality, intLatency);
        os->initProcessing(static_cast<size_t>(maxBlockSize));
        os->reset();
        channelOversamplers.push_back(std::move(os));
        channelFilters.push_back(std::make_unique<juce::dsp::StateVariableTPTFilter<float>>());
    }

    lastQualityHF = qualityHF;
    rebuildAtOversampledRate();
}

void AudioEngine::rebuildAtOversampledRate()
{
    // Everything downstream of the upsample step — filter, modeling — runs at 2x,
    // so its rate-dependent coefficients/FIRs must be (re)designed for 2x, not the
    // host's base rate. See the class comment in AudioEngine.h for the one
    // accepted exception (the modeling section's H2 kernel).
    const double osRate = currentSampleRate * 2.0;
    modelManager.rebuildFirsForSampleRate(osRate);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = osRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(currentMaxBlock * 2);
    spec.numChannels      = 1;
    for (auto& f : channelFilters)
        if (f) { f->prepare(spec); f->reset(); }
}

int AudioEngine::getLatencySamples() const
{
    if (!channelOversamplers.empty() && channelOversamplers[0])
        return static_cast<int>(channelOversamplers[0]->getLatencyInSamples());
    return 0;
}

void AudioEngine::initialise(double sampleRate, int maxBlockSize, int numChannels)
{
    currentSampleRate = sampleRate;
    lastNumChannels   = numChannels;
    currentMaxBlock   = maxBlockSize;

    modelManager.loadBuiltInModels(sampleRate);
    setupOversampling(numChannels, maxBlockSize, lastQualityHF);  // also rebuilds FIRs at 2x

    const double osRate = sampleRate * 2.0;
    satProc.prepare(osRate, maxBlockSize * 2, numChannels);
    modeEngine.prepare(osRate);
}

void AudioEngine::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    const bool srChanged  = std::abs(sampleRate - currentSampleRate) > 0.5;
    const bool chChanged  = numChannels != lastNumChannels;
    const bool blkChanged = maxBlockSize != currentMaxBlock;

    currentSampleRate = sampleRate;
    lastNumChannels   = numChannels;
    currentMaxBlock   = maxBlockSize;

    if (chChanged || blkChanged || srChanged || channelOversamplers.empty())
        setupOversampling(numChannels, maxBlockSize, lastQualityHF);
    else if (srChanged)
        rebuildAtOversampledRate();

    const double osRate = sampleRate * 2.0;
    satProc.prepare(osRate, maxBlockSize * 2, numChannels);
    modeEngine.prepare(osRate);
}

void AudioEngine::reset()
{
    satProc.reset();
    modeEngine.reset();
    for (auto& f : channelFilters)  if (f) f->reset();
    for (auto& os : channelOversamplers) if (os) os->reset();
    tiltStateL = {}; tiltStateR = {};
    airStateL  = {}; airStateR  = {};
    modelLowShelfStateL = {}; modelLowShelfStateR = {};
}

// ── Process ──────────────────────────────────────────────────────────────────

void AudioEngine::process(juce::AudioBuffer<float>& buffer, const Params& p)
{
    const int numSamples      = buffer.getNumSamples();
    const int numChannels     = buffer.getNumChannels();
    const int numProcChannels = std::min(numChannels, 2);

    if (p.qualityModeHF != lastQualityHF)
        setupOversampling(numChannels, currentMaxBlock, p.qualityModeHF);

    // ── Input metering (raw, pre-processing) ──────────────────────────────────
    {
        float pkL = 0.f, pkR = 0.f;
        if (numChannels > 0)
        {
            const float* d = buffer.getReadPointer(0);
            for (int n = 0; n < numSamples; ++n) pkL = std::max(pkL, std::abs(d[n]));
        }
        pkR = pkL;
        if (numChannels > 1)
        {
            const float* d = buffer.getReadPointer(1);
            pkR = 0.f;
            for (int n = 0; n < numSamples; ++n) pkR = std::max(pkR, std::abs(d[n]));
        }
        inputPeakL.store(pkL, std::memory_order_relaxed);
        inputPeakR.store(pkR, std::memory_order_relaxed);
    }

    const LNLModel& model = modelManager.getModel(p.modelSwitch);
    const double    osRate = currentSampleRate * 2.0;

    modeEngine.setMode(static_cast<AlgoMode>(p.algoMode));
    const float algoDriveLin = juce::Decibels::decibelsToGain(p.algoDrive * 24.0f);
    const float algoOutLin   = juce::Decibels::decibelsToGain(p.algoOutputDb);

    // ── Filter/Air coefficient prep (once per block, at the oversampled rate) ──
    if (!p.filterBypass && p.filterEnabled)
    {
        if (p.filterType != lastFilterType)
        {
            tiltStateL = {}; tiltStateR = {};
            for (auto& f : channelFilters) if (f) f->reset();
            lastFilterType = p.filterType;
        }

        if (p.filterType == 3)  // TILT: Pultec-shaped low/high shelf, softened Q
        {
            constexpr float kTiltShelfS = 0.4f;
            const float logFc   = std::log(juce::jlimit(20.f, 15000.f, p.filterCutoff) / 1000.f);
            const float logMaxL = std::log(1000.f / 20.f);
            const float logMaxH = std::log(15000.f / 1000.f);
            const float maxGain = 7.f;

            if (logFc < -0.02f)
            {
                const float gDb = juce::jlimit(0.f, maxGain, (-logFc / logMaxL) * maxGain);
                tiltCoeffs = makeLowShelf(100.f, gDb, (float)osRate, kTiltShelfS);
            }
            else if (logFc > 0.02f)
            {
                const float gDb = juce::jlimit(0.f, maxGain, (logFc / logMaxH) * maxGain);
                tiltCoeffs = makeHighShelf(7000.f, gDb, (float)osRate, kTiltShelfS);
            }
            else
            {
                tiltCoeffs = BiquadCoeffs{};
            }
        }
    }

    if (!p.filterBypass && p.airEnabled && p.airGainDb > 0.01f)
        airCoeffs = makeHighShelf(12000.f, p.airGainDb, (float)osRate, 0.7f);  // unchanged Q

    // Fixed low shelf ahead of the modeling stage — see AudioEngine.h. Corner
    // pulled down to ~27 Hz and a fairly steep (S=1) slope so the transition
    // stays clear of 40 Hz and above, per the "don't touch 40Hz+" requirement.
    constexpr float kModelLowShelfFreqHz = 27.0f;
    constexpr float kModelLowShelfGainDb = -3.0f;
    constexpr float kModelLowShelfS      = 1.0f;  // medium/standard RBJ shelf slope
    if (!p.modelBypass)
        modelLowShelfCoeffs = makeLowShelf(kModelLowShelfFreqHz, kModelLowShelfGainDb,
                                           (float)osRate, kModelLowShelfS);

    // Model Drive's own parameter range tops out at kModelDriveMax (< 1.0, see
    // PluginParameters.h) so two cascaded drive stages don't compound too far.
    // But feeding that raw value straight into the gain-model blend below meant
    // turning the knob to its physical maximum never actually reached the
    // hottest captured hardware gain stage (it topped out partway there) — that
    // was why max Drive sounded subtler than the real unit clipping. Normalizing
    // here so the knob's full travel spans the FULL captured range fixes that
    // without inventing any new nonlinearity — same measured curves, just fully
    // reachable now.
    const float modelDriveNorm = juce::jlimit(0.0f, 1.0f, p.modelDrive / ParamIDs::kModelDriveMax);

    // ── Modeling waveshaper blend (once per block; position-based across the
    // model's captured gain stages, same math as SaturationModel1) ────────────
    SaturationProcessor::blendWaveshapers(model, modelDriveNorm, wsBlendL, false);
    if (model.isStereoModel())
        SaturationProcessor::blendWaveshapers(model, modelDriveNorm, wsBlendR, true);

    const float modelInputPadLin    = juce::Decibels::decibelsToGain(model.inputPadDb);
    const float modelInputPadMakeup = (model.inputPadDb < -0.1f) ? (1.0f / modelInputPadLin) : 1.0f;
    // Output is intentionally allowed to rise with Drive rather than being
    // auto-normalized back down — matches how the real hardware behaves (ride
    // Output afterward): -6dB at minimum Drive up to +9dB at maximum.
    const float modelDrivePostMul   = std::pow(2.0f, modelDriveNorm * 2.5f - 1.0f);
    const float modelFinalScale     = modelInputPadMakeup * modelDrivePostMul
                                     * juce::Decibels::decibelsToGain(p.modelOutputDb);

    // ── Phase 1: upsample, then Distortion, then Filter — per channel ─────────
    std::array<float*, 2> upPtrs { nullptr, nullptr };
    int upN = 0;

    for (int ch = 0; ch < numProcChannels; ++ch)
    {
        const bool isRight = (ch == 1);
        float* data = buffer.getWritePointer(ch);

        auto& os = *channelOversamplers[static_cast<size_t>(ch)];
        juce::dsp::AudioBlock<float> inBlock(&data, 1, static_cast<size_t>(numSamples));
        auto upBlock  = os.processSamplesUp(inBlock);
        float* up     = upBlock.getChannelPointer(0);
        upN           = static_cast<int>(upBlock.getNumSamples());
        upPtrs[static_cast<size_t>(ch)] = up;

        // Stage 1: Distortion — Mix blends against the just-upsampled signal, so
        // wet and dry are already sample-aligned (no delay line needed).
        if (!p.algoBypass)
        {
            std::vector<float> algoDryUp(up, up + upN);
            for (int i = 0; i < upN; ++i)
            {
                float y = modeEngine.processSample(up[i], algoDriveLin, ch) * algoOutLin;
                up[i] = p.algoMix * y + (1.0f - p.algoMix) * algoDryUp[i];
            }
        }

        // Stage 2: Filter + Air
        if (!p.filterBypass)
        {
            if (p.filterEnabled)
            {
                if (p.filterType == 3)
                {
                    auto& st = isRight ? tiltStateR : tiltStateL;
                    for (int i = 0; i < upN; ++i)
                        up[i] = applyBiquad(up[i], st, tiltCoeffs);
                }
                else if (channelFilters[static_cast<size_t>(ch)])
                {
                    auto& cf = *channelFilters[static_cast<size_t>(ch)];
                    cf.setType(mapFilterType(p.filterType));
                    cf.setCutoffFrequency(juce::jlimit(20.f, 15000.f, p.filterCutoff));
                    juce::dsp::AudioBlock<float> fb(&up, 1, static_cast<size_t>(upN));
                    cf.process(juce::dsp::ProcessContextReplacing<float>(fb));
                }
            }

            if (p.airEnabled && p.airGainDb > 0.01f)
            {
                auto& st = isRight ? airStateR : airStateL;
                for (int i = 0; i < upN; ++i)
                    up[i] = applyBiquad(up[i], st, airCoeffs);
            }
        }
    }

    // ── Phase 2: Modeling — ONE call spanning both channels. SaturationProcessor
    // derives "which physical channel" from the buffer's own channel count, so
    // it must see both channels together (not one at a time) to route its
    // internal L/R history state correctly. ───────────────────────────────────
    if (!p.modelBypass)
    {
        // Low shelf FIRST — ahead of H1/H2/waveshaper — so the sub-40Hz energy
        // that provokes the "flub" is pulled back before it ever reaches the
        // nonlinearity, rather than filtering the mess out after the fact.
        for (int ch = 0; ch < numProcChannels; ++ch)
        {
            auto& st  = (ch == 1) ? modelLowShelfStateR : modelLowShelfStateL;
            float* up = upPtrs[static_cast<size_t>(ch)];
            for (int i = 0; i < upN; ++i)
                up[i] = applyBiquad(up[i], st, modelLowShelfCoeffs);
        }

        juce::AudioBuffer<float> modelBuf(upPtrs.data(), numProcChannels, upN);
        satProc.process(modelBuf, model, modelDriveNorm);

        if (model.modelType == LNLModel::ModelType::Volterra)
        {
            for (int ch = 0; ch < numProcChannels; ++ch)
            {
                const auto& ws = (ch == 1 && model.isStereoModel()) ? wsBlendR : wsBlendL;
                float* up = upPtrs[static_cast<size_t>(ch)];
                if (ws.empty())
                    for (int i = 0; i < upN; ++i) up[i] *= modelFinalScale;
                else
                    for (int i = 0; i < upN; ++i)
                        up[i] = SaturationProcessor::applyWaveshaper(up[i], ws) * modelFinalScale;
            }
            satProc.applyL2Fir(modelBuf, model);
        }
    }

    // ── Phase 3: downsample ────────────────────────────────────────────────────
    for (int ch = 0; ch < numProcChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        auto& os = *channelOversamplers[static_cast<size_t>(ch)];
        juce::dsp::AudioBlock<float> outBlock(&data, 1, static_cast<size_t>(numSamples));
        os.processSamplesDown(outBlock);
    }

    // ── Output soft-limiter (tanh knee at ±0.95 FS) + output metering ─────────
    float outPkL = 0.f, outPkR = 0.f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        float  pk   = 0.f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float ax = std::abs(data[n]);
            if (ax > 0.95f)
            {
                constexpr float knee = 0.05f;
                data[n] = std::copysign(0.95f + knee * std::tanh((ax - 0.95f) / knee), data[n]);
            }
            pk = std::max(pk, std::abs(data[n]));
        }
        if (ch == 0) outPkL = pk; else if (ch == 1) outPkR = pk;
    }
    if (numChannels < 2) outPkR = outPkL;
    outputPeakL.store(outPkL, std::memory_order_relaxed);
    outputPeakR.store(outPkR, std::memory_order_relaxed);
}
