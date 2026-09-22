#include "ModelManager.h"
#include "ArtifactFile.h"
#include "AnalysisEngine.h"
#include <BinaryData.h>
#include <cmath>

// Cepstral minimum-phase conversion + 1 kHz normalisation.
// Takes the linear-phase FIR from AnalysisEngine and returns a minimum-phase FIR
// with the same magnitude response. Uses JUCE FFT (available here via ModelManager.h).
static std::vector<float> toMinimumPhase(const std::vector<float>& h, float sampleRate)
{
    const int M = static_cast<int>(h.size());
    if (M < 2) return h;

    // FFT size: power-of-2 >= 4*M to minimise circular aliasing in the cepstral domain
    int fftOrder = 1;
    int fftSize  = 2;
    while (fftSize < 4 * M) { fftSize <<= 1; ++fftOrder; }

    juce::dsp::FFT fft(fftOrder);

    // buf is 2*fftSize floats — JUCE real FFT requires this size
    std::vector<float> buf(static_cast<size_t>(2 * fftSize), 0.f);

    // 1. Copy linear-phase FIR into real input (zero-padded)
    for (int i = 0; i < M; ++i)
        buf[static_cast<size_t>(i)] = h[static_cast<size_t>(i)];

    // 2. Forward FFT → H(k) stored as fftSize interleaved complex pairs
    fft.performRealOnlyForwardTransform(buf.data(), false);

    // 3. Replace each bin with (log|H(k)|, 0) — input for real cepstrum IFFT
    for (int k = 0; k < fftSize; ++k)
    {
        const float re  = buf[static_cast<size_t>(2 * k)];
        const float im  = buf[static_cast<size_t>(2 * k + 1)];
        buf[static_cast<size_t>(2 * k)]     = std::log(std::max(std::sqrt(re*re + im*im), 1e-10f));
        buf[static_cast<size_t>(2 * k + 1)] = 0.f;
    }

    // 4. Inverse FFT → real cepstrum c[n] in buf[0..fftSize-1]
    fft.performRealOnlyInverseTransform(buf.data());

    // 5. Causal windowing: keep c[0], double c[1..half-1], keep c[half], zero c[half+1..]
    const int half = fftSize / 2;
    for (int n = 1; n < half; ++n)
        buf[static_cast<size_t>(n)] *= 2.f;
    for (int n = half + 1; n < fftSize; ++n)
        buf[static_cast<size_t>(n)] = 0.f;
    for (int n = fftSize; n < 2 * fftSize; ++n)
        buf[static_cast<size_t>(n)] = 0.f;

    // 6. Forward FFT of windowed cepstrum → complex log-spectrum of min-phase filter
    fft.performRealOnlyForwardTransform(buf.data(), false);

    // 7. Exponentiate each bin: H_mp(k) = exp(logMag + j*phase)
    for (int k = 0; k < fftSize; ++k)
    {
        const float mag   = std::exp(buf[static_cast<size_t>(2 * k)]);
        const float phase = buf[static_cast<size_t>(2 * k + 1)];
        buf[static_cast<size_t>(2 * k)]     = mag * std::cos(phase);
        buf[static_cast<size_t>(2 * k + 1)] = mag * std::sin(phase);
    }

    // 8. Inverse FFT → minimum-phase impulse response in buf[0..fftSize-1]
    fft.performRealOnlyInverseTransform(buf.data());

    // 9. Extract first M taps
    std::vector<float> hmp(static_cast<size_t>(M));
    for (int n = 0; n < M; ++n)
        hmp[static_cast<size_t>(n)] = buf[static_cast<size_t>(n)];

    // 10. Normalise to unit gain at 1 kHz
    float re1k = 0.f, im1k = 0.f;
    for (int n = 0; n < M; ++n)
    {
        const float ang = -2.f * juce::MathConstants<float>::pi * 1000.f * static_cast<float>(n) / sampleRate;
        re1k += hmp[static_cast<size_t>(n)] * std::cos(ang);
        im1k += hmp[static_cast<size_t>(n)] * std::sin(ang);
    }
    const float gain1k = std::sqrt(re1k * re1k + im1k * im1k);
    if (gain1k > 1e-10f)
    {
        const float inv = 1.f / gain1k;
        for (auto& tap : hmp)
            tap *= inv;
    }

    return hmp;
}

// Curated set of bundled hardware profiles: {BinaryData resource name, display name}.
// Picked by hand from everything under ProfilerModels/ after A/B comparison — see
// [[project_neve_capi_data_quality]] for why these two specifically.
static const struct { const char* dataName; const char* displayName; } kBuiltInProfiles[] =
{
    { "SystemCalExperiment_Capi_model_json", "CAPI VP28"        },
    { "AmtekSS_Bias_Standard_model_json",    "Amptek Saturator" },
};

int ModelManager::countBuiltInModels()
{
    return (int)(sizeof(kBuiltInProfiles) / sizeof(kBuiltInProfiles[0]));
}

void ModelManager::buildFir(LNLModel& m, double sr)
{
    // Always rebuild from measured FR data (ref §6).
    // Never use the stored l1Fir from JSON — tap count and SR may differ.
    if (m.frFrequencies.empty()) return;

    // L1 FIR: design linear-phase prototype, then convert to minimum-phase
    m.l1Fir = toMinimumPhase(
        AnalysisEngine::rebuildFirAtSampleRate(m.frFrequencies, m.frMagnitudeDb, (float)sr),
        (float)sr);

    // Right-channel L1 FIR for stereo models — same pipeline
    if (!m.frMagnitudeDbR.empty())
        m.l1FirR = toMinimumPhase(
            AnalysisEngine::rebuildFirAtSampleRate(m.frFrequencies, m.frMagnitudeDbR, (float)sr),
            (float)sr);

    // L1 FIR tap count per paper eq.(16): nextOdd(max(floor(sr/43), 127))
    // L2 FIR: only rebuild if not already loaded from JSON (the profiler computed
    // it from residual L1∘N vs recording — more accurate than rebuilding from raw FR).
    // Here we rebuild as a fallback for SR-mismatched loads.
    int l2Taps = static_cast<int>(std::round((float)sr / 43.0f));
    l2Taps = std::max(l2Taps, 127);
    if (l2Taps % 2 == 0) ++l2Taps;
    if (m.l2Fir.empty())   // preserve JSON-loaded residual L2 if present
        m.l2Fir = AnalysisEngine::rebuildFirAtSampleRate(
            m.frFrequencies, m.frMagnitudeDb, (float)sr, l2Taps, 1.0f, 0.0f);

    m.sampleRate = static_cast<float>(sr);
}

void ModelManager::loadBuiltInModels(double sampleRate)
{
    models.clear();
    for (auto& entry : kBuiltInProfiles)
    {
        int dataSize = 0;
        const char* data = BinaryData::getNamedResource(entry.dataName, dataSize);
        if (!data) continue;

        ModelEntry me;
        me.name = entry.displayName;
        juce::String json(data, (size_t)dataSize);
        juce::String err = ArtifactFile::load(json, me.model);
        if (err.isNotEmpty())
        {
            DBG("ModelManager: failed to load " << entry.displayName << ": " << err);
            continue;
        }
        me.model.name = entry.displayName;
        repairOutlierGainStages(me.model.gainModels);
        repairOutlierGainStages(me.model.gainModelsR);
        capExcessiveGain(me.model.gainModels);
        capExcessiveGain(me.model.gainModelsR);
        buildFir(me.model, sampleRate);
        models.push_back(std::move(me));
    }
}

namespace
{
    float avgAbs(const std::vector<float>& v)
    {
        if (v.empty()) return 0.0f;
        float s = 0.f;
        for (float x : v) s += std::abs(x);
        return s / (float)v.size();
    }

    float peakAbs(const std::vector<float>& v)
    {
        float m = 0.f;
        for (float x : v) m = std::max(m, std::abs(x));
        return m;
    }

    // A wide run of bit-exact zeros in a waveshaper table is not something real analog
    // hardware produces — a genuine saturation curve always has some residual signal,
    // even a tiny one. This is the signature of a failed regression fit (e.g. a
    // decorrelated/latency-misaligned tone capture for that one gain step regressing
    // to zero everywhere it had no usable scatter data), not real hardware behaviour.
    bool hasDeadZone(const std::vector<float>& ws)
    {
        if (ws.size() < 64) return false;
        size_t longestRun = 0, run = 0;
        for (float v : ws)
        {
            if (std::abs(v) < 1e-5f) { if (++run > longestRun) longestRun = run; }
            else run = 0;
        }
        return longestRun > ws.size() / 10;  // >10% of the table exactly zero in a row
    }
}

void ModelManager::repairOutlierGainStages(std::vector<GainModel>& gm)
{
    const size_t N = gm.size();
    if (N < 2) return;

    // ── Step 1: detect corrupted stages ───────────────────────────────────────
    // Two independent signatures of a failed capture/regression at that gain step:
    //  (a) a wide dead zone in the waveshaper (see hasDeadZone above), and
    //  (b) a magnitude wildly larger than BOTH immediate neighbors — an isolated
    //      blow-up (e.g. a clipped or mis-padded tone capture for just that step).
    // A stage flagged by either check has ALL of its tables replaced together in
    // Step 2, since a bad capture at that step taints everything derived from it,
    // even fields whose numbers happen to still look plausible on their own.
    std::vector<bool> corrupted(N, false);
    constexpr float kOutlierRatio = 3.0f;  // flag only if >3x the neighbors' average

    for (size_t i = 0; i < N; ++i)
    {
        if (hasDeadZone(gm[i].waveshaper)) { corrupted[i] = true; continue; }
        if (i == 0 || i + 1 >= N) continue;  // outlier-vs-neighbor test needs both neighbors

        auto isOutlier = [&](std::vector<float> GainModel::*member)
        {
            const auto& cur = gm[i].*member;
            const auto& loV = gm[i - 1].*member;
            const auto& hiV = gm[i + 1].*member;
            if (cur.empty() || loV.empty() || hiV.empty()) return false;
            const float curMag   = avgAbs(cur);
            const float neighAvg = 0.5f * (avgAbs(loV) + avgAbs(hiV));
            return neighAvg > 1e-8f && curMag > kOutlierRatio * neighAvg;
        };

        if (isOutlier(&GainModel::waveshaper) || isOutlier(&GainModel::volterraH1)
            || isOutlier(&GainModel::volterraH2))
            corrupted[i] = true;
    }

    // ── Step 2: repair an isolated bad stage by averaging its two good neighbors;
    // DROP a leading/trailing run of bad stages entirely (no good neighbor on the
    // far side). Position-based blending (see blendVolterra/blendWaveshapers in
    // SaturationProcessor.cpp) always spans the full 0..1 drive range across
    // whatever stages remain in this array — so dropping the unusable stages
    // stretches the drive knob's ENTIRE physical rotation across only the
    // genuinely-measured range, instead of spending part of the knob's travel
    // frozen on a duplicated copy of the last good stage. This doesn't invent any
    // character beyond what was actually captured; it just stops wasting knob
    // travel on stages that were never real.
    std::vector<bool> toDrop(N, false);

    auto averageStages = [&](size_t i, size_t a, size_t b)
    {
        auto avgVec = [](const std::vector<float>& x, const std::vector<float>& y)
        {
            const size_t n = std::min(x.size(), y.size());
            std::vector<float> out(n);
            for (size_t k = 0; k < n; ++k) out[k] = 0.5f * (x[k] + y[k]);
            return out;
        };
        gm[i].waveshaper = avgVec(gm[a].waveshaper, gm[b].waveshaper);
        gm[i].volterraH1 = avgVec(gm[a].volterraH1, gm[b].volterraH1);
        gm[i].volterraH2 = avgVec(gm[a].volterraH2, gm[b].volterraH2);
    };

    for (size_t i = 0; i < N; ++i)
    {
        if (!corrupted[i]) continue;

        const bool loGood = (i > 0)     && !corrupted[i - 1];
        const bool hiGood = (i + 1 < N) && !corrupted[i + 1];

        DBG("ModelManager: repairing corrupted gain stage " << (int)i
            << (loGood && hiGood ? " (averaging neighbors)" : " (dropping — no good neighbor on both sides)"));

        if (loGood && hiGood)
            averageStages(i, i - 1, i + 1);
        else
            toDrop[i] = true;
    }

    if (std::any_of(toDrop.begin(), toDrop.end(), [](bool b) { return b; }))
    {
        std::vector<GainModel> kept;
        kept.reserve(N);
        for (size_t i = 0; i < N; ++i)
            if (!toDrop[i]) kept.push_back(std::move(gm[i]));
        gm = std::move(kept);
    }
}

void ModelManager::capExcessiveGain(std::vector<GainModel>& gm)
{
    const size_t N = gm.size();
    if (N == 0) return;

    // Every bundled model we've measured so far — including deliberately "hot"
    // overdrive-style captures that turned out clean — keeps its waveshaper peak
    // under ~1.3 and its H2 magnitude under ~0.7. These ceilings sit comfortably
    // above that with real margin, while sitting an order of magnitude below the
    // kind of blow-up a drifted capture session produces (BluesDriverStandard hit
    // waveshaper peaks of 14-16 and H2 magnitudes of 13-14 even at its lowest gain
    // stage).
    constexpr float kMaxWaveshaperPeak = 1.5f;
    constexpr float kMaxH2Magnitude    = 1.0f;

    // ── Waveshaper: rescale to match the nearest stage that's already plausible ──
    // Computed from the ORIGINAL data up front, so "nearest good neighbor" always
    // refers to something genuinely captured, not another stage's rescaled result.
    std::vector<float> peaks(N);
    for (size_t i = 0; i < N; ++i) peaks[i] = peakAbs(gm[i].waveshaper);

    for (size_t i = 0; i < N; ++i)
    {
        if (peaks[i] <= kMaxWaveshaperPeak) continue;

        float refPeak = -1.0f;
        for (size_t d = 1; d < N && refPeak < 0.0f; ++d)
        {
            if (i >= d && peaks[i - d] <= kMaxWaveshaperPeak) refPeak = peaks[i - d];
            else if (i + d < N && peaks[i + d] <= kMaxWaveshaperPeak) refPeak = peaks[i + d];
        }
        if (refPeak < 0.0f) refPeak = kMaxWaveshaperPeak;  // every stage is hot — cap absolutely instead

        const float scale = refPeak / peaks[i];
        for (float& x : gm[i].waveshaper) x *= scale;

        DBG("ModelManager: capped gain stage " << (int)i << " waveshaper peak "
            << peaks[i] << " -> " << refPeak);
    }

    // ── H2: independent absolute ceiling ────────────────────────────────────────
    // Not gated on the waveshaper check above — a stage can have a perfectly
    // plausible waveshaper while its H2 kernel is separately blown up (true for
    // BluesDriverStandard's lowest two gain stages).
    for (auto& g : gm)
    {
        const float h2Peak = avgAbs(g.volterraH2);
        if (h2Peak <= kMaxH2Magnitude) continue;
        const float scale = kMaxH2Magnitude / h2Peak;
        for (float& x : g.volterraH2) x *= scale;
    }
}

void ModelManager::rebuildFirsForSampleRate(double sampleRate)
{
    for (auto& e : models)
        buildFir(e.model, sampleRate);
}

const LNLModel& ModelManager::getModel(int index) const
{
    static LNLModel empty;
    if (index < 0 || index >= static_cast<int>(models.size())) return empty;
    return models[static_cast<size_t>(index)].model;
}

juce::StringArray ModelManager::getModelNames() const
{
    juce::StringArray names;
    for (auto& e : models)
        names.add(e.name);
    return names;
}
