#include "PluginParameters.h"
#include "SaturationModeEngine.h"
#include "ModelManager.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── Distortion ────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamIDs::algoMode, "Distortion Mode", 0, SaturationModeEngine::getNumModes() - 1, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::algoDrive, "Distortion Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.3f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::algoOutput, "Distortion Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::algoMix, "Distortion Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::algoBypass, "Distortion Bypass", true));  // bypassed by default

    // ── Filter ────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::filterEnabled, "Filter Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamIDs::filterType, "Filter Type", 0, 3, 3));  // 0=HP 1=LP 2=BP 3=TILT

    {
        auto freqRange = juce::NormalisableRange<float>(20.f, 15000.f, 0.f);
        freqRange.setSkewForCentre(1000.f);
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParamIDs::filterCutoff, "Filter Cutoff", freqRange, 1000.f));
    }

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::airEnabled, "Air Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::airGain, "Air Gain",
        juce::NormalisableRange<float>(0.0f, 9.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::filterBypass, "Filter Section Bypass", false));

    // ── Modeling ──────────────────────────────────────────────────────────────
    // Choice (not Int) so hosts show profile names in automation lanes. State
    // saves the index, so pre-existing sessions keep CAPI=0 / Amptek=1.
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::modelSwitch, "Model", ModelManager::getBuiltInModelNames(), 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::modelDrive, "Model Drive",
        juce::NormalisableRange<float>(0.0f, ParamIDs::kModelDriveMax, 0.001f),
        ParamIDs::kModelDriveMax * 0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::modelOutput, "Model Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::modelBypass, "Model Bypass", false));

    return { params.begin(), params.end() };
}
