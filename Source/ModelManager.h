#pragma once
#include <JuceHeader.h>
#include "LNLModel.h"

struct ModelEntry
{
    juce::String name;
    LNLModel     model;
};

class ModelManager
{
public:
    // Load all bundled models from BinaryData. Call once at startup.
    void loadBuiltInModels(double sampleRate);

    // Re-build FIRs when the DAW sample rate changes.
    void rebuildFirsForSampleRate(double sampleRate);

    int  getNumModels() const { return (int)models.size(); }
    const LNLModel& getModel(int index) const;
    juce::StringArray getModelNames() const;

    // Number of curated bundled profiles (kBuiltInProfiles in ModelManager.cpp).
    // Static and BinaryData-only, so it can be called before any ModelManager
    // instance exists — needed to size the modelIndex parameter's range in the
    // AudioProcessor's constructor, alongside SaturationModeEngine's algorithmic
    // mode count (the modelIndex range spans both: profiles first, then modes).
    static int countBuiltInModels();

private:
    void buildFir(LNLModel& m, double sr);

    // Detects a per-gain-stage waveshaper/Volterra table that's either (a) a wide
    // dead zone of bit-exact zeros (a failed regression fit, e.g. a decorrelated or
    // latency-misaligned tone capture for that step) or (b) wildly larger in
    // magnitude than both immediate neighbors (an isolated blown-up capture, e.g.
    // clipped tone or wrong pad state) — real analog gain staging changes gradually
    // between adjacent steps, so either signature means the capture at that step is
    // unreliable. An isolated bad stage (good neighbors on both sides) is repaired
    // by averaging them; a leading/trailing RUN of bad stages is dropped entirely,
    // so the drive knob's full rotation stretches across only the stages that were
    // genuinely captured, rather than wasting part of its travel on a frozen
    // duplicate of the last good stage.
    static void repairOutlierGainStages(std::vector<GainModel>& gainModels);

    // Catches a different failure mode than repairOutlierGainStages: a stage whose
    // data isn't corrupted/dead-zoned, just implausibly LOUD in absolute terms —
    // e.g. a capture session where gain-staging drifted, producing a smooth but
    // wildly escalating waveshaper/H2 magnitude (10-20x+ what any other bundled
    // model shows). Rather than discard that stage (losing its actual captured
    // character), this rescales its waveshaper down to match the nearest stage that
    // IS plausible, and caps its H2 kernel to an absolute ceiling — same curve
    // shape, tamed intensity, brought in line with every other bundled model.
    static void capExcessiveGain(std::vector<GainModel>& gainModels);

    std::vector<ModelEntry> models;
};
