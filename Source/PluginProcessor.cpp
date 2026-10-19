#include "PluginProcessor.h"
#include "PluginEditor.h"

AnalogDriveProcessor::AnalogDriveProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void AnalogDriveProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // processBlock's buffer has max(in, out) channels — e.g. 2 for Logic's
    // mono-in/stereo-out tracks — so size the engine for that, not just inputs.
    int numChannels = std::max(getTotalNumInputChannels(), getTotalNumOutputChannels());
    if (!engineInitialised)
    {
        engine.initialise(sampleRate, samplesPerBlock, numChannels);
        engineInitialised = true;
    }
    else
    {
        engine.prepare(sampleRate, samplesPerBlock, numChannels);
    }
}

void AnalogDriveProcessor::releaseResources()
{
    engine.reset();
}

void AnalogDriveProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    AudioEngine::Params p;
    p.algoMode     = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::algoMode.getParamID()));
    p.algoDrive    = *apvts.getRawParameterValue(ParamIDs::algoDrive.getParamID());
    p.algoOutputDb = *apvts.getRawParameterValue(ParamIDs::algoOutput.getParamID());
    p.algoMix      = *apvts.getRawParameterValue(ParamIDs::algoMix.getParamID());
    p.algoBypass   = *apvts.getRawParameterValue(ParamIDs::algoBypass.getParamID()) > 0.5f;

    p.filterEnabled = *apvts.getRawParameterValue(ParamIDs::filterEnabled.getParamID()) > 0.5f;
    p.filterType    = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::filterType.getParamID()));
    p.filterCutoff  = *apvts.getRawParameterValue(ParamIDs::filterCutoff.getParamID());
    p.airEnabled    = *apvts.getRawParameterValue(ParamIDs::airEnabled.getParamID()) > 0.5f;
    p.airGainDb     = *apvts.getRawParameterValue(ParamIDs::airGain.getParamID());
    p.filterBypass  = *apvts.getRawParameterValue(ParamIDs::filterBypass.getParamID()) > 0.5f;

    p.modelSwitch  = static_cast<int>(*apvts.getRawParameterValue(ParamIDs::modelSwitch.getParamID()));
    p.modelDrive   = *apvts.getRawParameterValue(ParamIDs::modelDrive.getParamID());
    p.modelOutputDb = *apvts.getRawParameterValue(ParamIDs::modelOutput.getParamID());
    p.modelBypass  = *apvts.getRawParameterValue(ParamIDs::modelBypass.getParamID()) > 0.5f;

    setLatencySamples(engine.getLatencySamples());

    engine.process(buffer, p);
}

juce::AudioProcessorEditor* AnalogDriveProcessor::createEditor()
{
    return new AnalogDriveEditor(*this);
}

void AnalogDriveProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void AnalogDriveProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AnalogDriveProcessor();
}
