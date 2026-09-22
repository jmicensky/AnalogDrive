#include "AnalysisEngine.h"
#include <cmath>
#include <algorithm>

static constexpr double kPi = 3.14159265358979323846;

static float hann(int n, int N)
{
    return 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(kPi) * static_cast<float>(n)
                                   / static_cast<float>(N - 1)));
}

std::vector<float> AnalysisEngine::rebuildFirAtSampleRate(
    const std::vector<float>& frFreq,
    const std::vector<float>& frMagDb,
    float targetSampleRate,
    int   numTaps,
    float hfTaper,
    float maxFreqHz)
{
    if (frFreq.empty() || frFreq.size() != frMagDb.size())
        return {};

    if (numTaps < 0)
        numTaps = static_cast<int>(std::round(targetSampleRate / 43.0f));
    if (numTaps % 2 == 0) ++numTaps;

    int N       = numTaps;
    int M       = N / 2;
    int halfBins = N / 2 + 1;

    std::vector<float> spectrum(static_cast<size_t>(halfBins), 0.0f);

    for (int k = 0; k < halfBins; ++k)
    {
        float freq  = static_cast<float>(k) * targetSampleRate / static_cast<float>(N);
        float magDb = 0.0f;

        if (freq <= frFreq.front())
        {
            magDb = frMagDb.front();
        }
        else if (freq >= frFreq.back())
        {
            magDb = frMagDb.back();
        }
        else
        {
            auto it = std::lower_bound(frFreq.begin(), frFreq.end(), freq);
            int hi  = static_cast<int>(it - frFreq.begin());
            int lo  = hi - 1;
            auto slo = static_cast<size_t>(lo);
            auto shi = static_cast<size_t>(hi);
            float t  = (freq - frFreq[slo]) / (frFreq[shi] - frFreq[slo]);
            magDb    = frMagDb[slo] + t * (frMagDb[shi] - frMagDb[slo]);
        }

        if (hfTaper < 1.0f)
        {
            float normFreq = freq / (targetSampleRate * 0.5f);
            float taper = 1.0f - (1.0f - hfTaper) * normFreq;
            taper = std::max(0.0f, taper);
            magDb *= taper;
        }

        spectrum[static_cast<size_t>(k)] = std::pow(10.0f, magDb / 20.0f);

        if (maxFreqHz > 0.0f && freq > maxFreqHz)
            spectrum[static_cast<size_t>(k)] = 0.0f;
    }

    std::vector<float> h(static_cast<size_t>(N), 0.0f);
    int lastBin = halfBins - 1;
    for (int n = 0; n < N; ++n)
    {
        double sum = static_cast<double>(spectrum[0]);
        for (int k = 1; k < lastBin; ++k)
            sum += 2.0 * static_cast<double>(spectrum[static_cast<size_t>(k)])
                       * std::cos(2.0 * kPi * k * (n - M) / N);
        if (lastBin > 0)
            sum += static_cast<double>(spectrum[static_cast<size_t>(lastBin)])
                   * std::cos(kPi * (n - M));
        h[static_cast<size_t>(n)] = static_cast<float>(sum / N) * hann(n, N);
    }

    return h;
}
