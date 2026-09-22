#pragma once
#include <vector>

class AnalysisEngine
{
public:
    // Rebuild a linear-phase FIR from frequency-response data at a new sample rate.
    // Pass numTaps = -1 to auto-size (scales so the LF floor is ~43 Hz).
    // hfTaper = 1.0f for no taper.
    // maxFreqHz > 0 zeros spectrum bins above that frequency.
    static std::vector<float> rebuildFirAtSampleRate(
        const std::vector<float>& frFrequencies,
        const std::vector<float>& frMagnitudeDb,
        float targetSampleRate,
        int   numTaps   = -1,
        float hfTaper   = 1.0f,
        float maxFreqHz = 0.0f);
};
