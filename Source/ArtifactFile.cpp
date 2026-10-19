#include "ArtifactFile.h"

static std::vector<float> parseFloatArray(const juce::var& v)
{
    std::vector<float> out;
    if (auto* arr = v.getArray())
    {
        out.reserve(static_cast<size_t>(arr->size()));
        for (auto& e : *arr)
            out.push_back(static_cast<float>(static_cast<double>(e)));
    }
    return out;
}

void ArtifactFile::parseGainModel(const juce::var& obj, GainModel& gm)
{
    gm.gainValue          = static_cast<float>(static_cast<double>(obj["gainValue"]));
    gm.waveshaper         = parseFloatArray(obj["waveshaper"]);
    gm.lowBandWaveshaper  = parseFloatArray(obj["lowBandWaveshaper"]);
    gm.highBandWaveshaper = parseFloatArray(obj["highBandWaveshaper"]);
    gm.volterraH1         = parseFloatArray(obj["volterraH1"]);
    gm.volterraH2         = parseFloatArray(obj["volterraH2"]);
}

juce::String ArtifactFile::load(const juce::String& jsonText, LNLModel& model)
{
    auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject())
        return "Failed to parse JSON";

    auto& obj = *parsed.getDynamicObject();

    model.sampleRate    = static_cast<float>(static_cast<double>(obj.getProperty("sampleRate")));
    model.captureSampleRate = model.sampleRate;
    model.inputPadDb    = static_cast<float>(static_cast<double>(obj.getProperty("inputPadDb")));
    model.l1Fir          = parseFloatArray(obj.getProperty("l1Fir"));
    model.frFrequencies  = parseFloatArray(obj.getProperty("frFrequencies"));
    model.frMagnitudeDb  = parseFloatArray(obj.getProperty("frMagnitudeDb"));
    model.frMagnitudeDbR = parseFloatArray(obj.getProperty("frMagnitudeDbR"));
    model.waveshaper    = parseFloatArray(obj.getProperty("waveshaper"));
    model.l2Fir         = parseFloatArray(obj.getProperty("l2Fir"));

    juce::String typeStr = obj.getProperty("modelType").toString();
    model.modelType = (typeStr == "Volterra") ? LNLModel::ModelType::Volterra
                                              : LNLModel::ModelType::LNL;
    model.volterraM2 = static_cast<int>(obj.getProperty("volterraM2"));
    model.volterraH1 = parseFloatArray(obj.getProperty("volterraH1"));
    model.volterraH2 = parseFloatArray(obj.getProperty("volterraH2"));

    model.l1FirR       = parseFloatArray(obj.getProperty("l1FirR"));
    model.waveshaperR  = parseFloatArray(obj.getProperty("waveshaperR"));
    model.l2FirR       = parseFloatArray(obj.getProperty("l2FirR"));

    model.lowBandWaveshaper  = parseFloatArray(obj.getProperty("lowBandWaveshaper"));
    model.highBandWaveshaper = parseFloatArray(obj.getProperty("highBandWaveshaper"));

    if (auto* gmArr = obj.getProperty("gainModels").getArray())
    {
        int n = gmArr->size();
        model.gainModels.resize(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            parseGainModel((*gmArr)[i], model.gainModels[static_cast<size_t>(i)]);
    }

    if (auto* gmArr = obj.getProperty("gainModelsR").getArray())
    {
        int n = gmArr->size();
        model.gainModelsR.resize(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
            parseGainModel((*gmArr)[i], model.gainModelsR[static_cast<size_t>(i)]);
    }

    if (!model.isValid())
        return "Artifact missing required fields (l1Fir or waveshaper)";

    return {};
}

juce::String ArtifactFile::load(const juce::File& file, LNLModel& model)
{
    if (!file.existsAsFile())
        return "File not found: " + file.getFullPathName();
    return load(file.loadFileAsString(), model);
}
