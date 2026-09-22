#pragma once
#include <vector>
#include <string>

struct GainModel
{
    float gainValue = 0.0f;
    std::vector<float> waveshaper;         // 1024 entries
    std::vector<float> lowBandWaveshaper;
    std::vector<float> highBandWaveshaper;
    // Volterra per-gain kernels (empty when not used)
    std::vector<float> volterraH1;
    std::vector<float> volterraH2;
};

struct LNLModel
{
    enum class ModelType { LNL, Volterra };

    bool isValid() const {
        if (modelType == ModelType::Volterra)
            return !volterraH1.empty()
                || (!gainModels.empty() && !gainModels[0].volterraH1.empty());
        return !waveshaper.empty() && !l1Fir.empty();
    }

    bool isStereoModel()   const { return !waveshaperR.empty(); }
    bool isMultiBandModel() const { return !lowBandWaveshaper.empty() && !highBandWaveshaper.empty(); }
    bool isMultiModel()    const { return gainModels.size() > 1; }

    ModelType modelType = ModelType::LNL;

    float sampleRate  = 44100.0f;
    float inputPadDb  = 0.0f;

    // L1 FIR (linear color)
    std::vector<float> l1Fir;
    std::vector<float> frFrequencies;
    std::vector<float> frMagnitudeDb;
    std::vector<float> frMagnitudeDbR;  // right-channel FR data for stereo models

    // Waveshaper (1024-entry table, input [-1,+1])
    std::vector<float> waveshaper;

    // L2 FIR — full-bandwidth post-saturation correction, applied by AudioEngine
    std::vector<float> l2Fir;

    // Stereo right-channel overrides
    std::vector<float> l1FirR;
    std::vector<float> waveshaperR;
    std::vector<float> l2FirR;
    std::vector<GainModel> gainModelsR;

    // Multiband tables (shared when isMultiModel() is false)
    std::vector<float> lowBandWaveshaper;
    std::vector<float> highBandWaveshaper;

    // Drive knob blending
    std::vector<GainModel> gainModels;

    // Volterra kernels (used when modelType == Volterra)
    std::vector<float> volterraH1;
    std::vector<float> volterraH2;
    int volterraM2 = 0;

    std::string name;
};
