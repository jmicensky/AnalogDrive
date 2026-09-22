#pragma once
#include <JuceHeader.h>

// Manages named preset snapshots of the plugin's full APVTS state.
//
// Factory presets are embedded as BinaryData resources (read-only, ship with
// every install) — any file dropped into Presets/Factory/*.xml before a build
// gets picked up automatically (see CMakeLists.txt's GLOB for that folder) and
// shows up here as a resource named "<Name>_preset_xml". None exist yet as of
// 2026-09-21; this is the pipeline for the ~10 the user is still auditioning.
//
// User presets are plain XML files in the OS's per-user app-data folder
// (~/Library/Application Support/Saturation Sounds/Analog Drive/Presets/ on
// macOS) — editable/deletable by the user like any other file, found fresh
// each time refresh() runs (not cached at compile time).
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        bool isFactory = false;
        juce::File file;                       // empty for factory presets
        const char* binaryDataName = nullptr;   // null for user presets
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvtsToUse);

    // Re-scans the user presets folder and the embedded factory list. Factory
    // presets always sort first (alphabetically within each group).
    void refresh();

    int getNumPresets() const { return (int)presets.size(); }
    juce::String getPresetName(int index) const;
    juce::StringArray getPresetNames() const;
    bool isFactoryPreset(int index) const;

    // Replaces the ENTIRE apvts state with the preset's saved snapshot.
    void loadPreset(int index);

    // Saves the CURRENT apvts state as a new user preset with this name (or
    // overwrites an existing user preset of the same name — factory presets
    // can't be overwritten this way, since they aren't files). Returns the new
    // preset's index, or -1 on failure (e.g. empty name, or a write error).
    int savePreset(const juce::String& name);

    int getCurrentIndex() const { return currentIndex; }
    void setCurrentIndex(int index) { currentIndex = index; }

    static juce::File getUserPresetsFolder();

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> presets;
    int currentIndex = -1;
};
