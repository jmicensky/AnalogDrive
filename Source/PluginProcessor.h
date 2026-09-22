#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"
#include "PluginParameters.h"
#include "PresetManager.h"

class ExpandedSaturationProcessor : public juce::AudioProcessor
{
public:
    ExpandedSaturationProcessor();
    ~ExpandedSaturationProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Expanded Saturation"; }

    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    AudioEngine& getAudioEngine() { return engine; }
    PresetManager& getPresetManager() { return presetManager; }

private:
    AudioEngine engine;
    bool engineInitialised = false;
    PresetManager presetManager { apvts };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpandedSaturationProcessor)
};
