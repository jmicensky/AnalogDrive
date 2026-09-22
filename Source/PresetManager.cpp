#include "PresetManager.h"

namespace
{
    const juce::String kFactorySuffix = "_preset_xml";
}

juce::File PresetManager::getUserPresetsFolder()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Saturation Sounds")
                   .getChildFile("Analog Drive")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    refresh();
}

void PresetManager::refresh()
{
    std::vector<Preset> factory, user;

    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String rn(BinaryData::namedResourceList[i]);
        if (!rn.endsWith(kFactorySuffix)) continue;

        Preset p;
        p.name = rn.dropLastCharacters(kFactorySuffix.length());
        p.isFactory = true;
        p.binaryDataName = BinaryData::namedResourceList[i];
        factory.push_back(p);
    }
    std::sort(factory.begin(), factory.end(),
              [](const Preset& a, const Preset& b) { return a.name < b.name; });

    auto userFiles = getUserPresetsFolder().findChildFiles(juce::File::findFiles, false, "*.xml");
    userFiles.sort();
    for (auto& f : userFiles)
    {
        Preset p;
        p.name = f.getFileNameWithoutExtension();
        p.isFactory = false;
        p.file = f;
        user.push_back(p);
    }

    const juce::String currentName = (currentIndex >= 0 && currentIndex < (int)presets.size())
                                          ? presets[(size_t)currentIndex].name : juce::String();

    presets.clear();
    presets.insert(presets.end(), factory.begin(), factory.end());
    presets.insert(presets.end(), user.begin(),    user.end());

    // Try to keep pointing at the same preset by name across a refresh (its
    // index may have shifted if a new file sorted in before it).
    currentIndex = -1;
    if (currentName.isNotEmpty())
        for (int i = 0; i < (int)presets.size(); ++i)
            if (presets[(size_t)i].name == currentName) { currentIndex = i; break; }
}

juce::String PresetManager::getPresetName(int index) const
{
    if (index < 0 || index >= (int)presets.size()) return {};
    return presets[(size_t)index].name;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (auto& p : presets) names.add(p.name);
    return names;
}

bool PresetManager::isFactoryPreset(int index) const
{
    if (index < 0 || index >= (int)presets.size()) return false;
    return presets[(size_t)index].isFactory;
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= (int)presets.size()) return;
    const auto& p = presets[(size_t)index];

    std::unique_ptr<juce::XmlElement> xml;
    if (p.isFactory)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource(p.binaryDataName, size);
        if (data == nullptr) return;
        xml = juce::XmlDocument::parse(juce::String::fromUTF8(data, size));
    }
    else
    {
        xml = juce::XmlDocument::parse(p.file);
    }

    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        currentIndex = index;
    }
}

int PresetManager::savePreset(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty()) return -1;

    const auto file = getUserPresetsFolder().getChildFile(
        juce::File::createLegalFileName(trimmed) + ".xml");

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml == nullptr || !xml->writeTo(file))
        return -1;

    refresh();
    for (int i = 0; i < (int)presets.size(); ++i)
        if (!presets[(size_t)i].isFactory && presets[(size_t)i].name == trimmed)
        {
            currentIndex = i;
            return i;
        }
    return -1;
}
