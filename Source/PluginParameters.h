#pragma once
#include <JuceHeader.h>

// Three-stage serial chain: Distortion (algorithmic) -> Filter -> Modeling (hardware
// profile), each independently bypassable. See AudioEngine::process for the order.
namespace ParamIDs
{
    // ── Distortion section (first in the chain) ─────────────────────────────────
    inline const juce::ParameterID algoMode   { "algoMode",   1 };  // which of the 12 algorithmic modes
    inline const juce::ParameterID algoDrive  { "algoDrive",  1 };  // 0-1 -> 0-24 dB pre-shaper gain
    inline const juce::ParameterID algoOutput { "algoOutput", 1 };  // dB trim
    inline const juce::ParameterID algoMix    { "algoMix",    1 };  // wet/dry, THIS section only
    inline const juce::ParameterID algoBypass { "algoBypass", 1 };

    // ── Filter section (second in the chain) ────────────────────────────────────
    inline const juce::ParameterID filterEnabled { "filterEnabled", 1 };
    inline const juce::ParameterID filterType    { "filterType",    1 };  // 0=HP 1=LP 2=BP 3=TILT
    inline const juce::ParameterID filterCutoff  { "filterCutoff",  1 };
    inline const juce::ParameterID airEnabled    { "airEnabled",    1 };
    inline const juce::ParameterID airGain       { "airGain",       1 };  // 0-9 dB
    inline const juce::ParameterID filterBypass  { "filterBypass",  1 };  // master bypass for the whole section

    // ── Modeling section (last in the chain, always 100% wet) ───────────────────
    inline const juce::ParameterID modelSwitch { "modelSwitch", 1 };  // 0=CAPI 1=Amptek
    inline const juce::ParameterID modelDrive  { "modelDrive",  1 };  // capped, see kModelDriveMax
    inline const juce::ParameterID modelOutput { "modelOutput", 1 };
    inline const juce::ParameterID modelBypass { "modelBypass", 1 };

    // Oversampling filter type for the one shared oversampled pass wrapping the
    // whole chain (IIR polyphase = low latency, FIR equiripple = phase-accurate).
    inline const juce::ParameterID qualityMode { "qualityMode", 1 };

    // Modeling drive is capped below the old plugin's full range: with the
    // algorithmic section now cascaded in front of it, two gain/drive stages
    // compound, so the modeling stage's own ceiling is pulled back to leave
    // headroom rather than letting both stages fight for the same 0-100%.
    constexpr float kModelDriveMax = 0.9f;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
