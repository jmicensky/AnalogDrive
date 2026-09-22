#pragma once
#include <JuceHeader.h>
#include "LNLModel.h"

class ArtifactFile
{
public:
    // Load a saturation artifact JSON into model.
    // Returns empty string on success, or an error message.
    static juce::String load(const juce::String& jsonText, LNLModel& model);
    static juce::String load(const juce::File& file, LNLModel& model);

private:
    static void parseGainModel(const juce::var& obj, GainModel& gm);
};
