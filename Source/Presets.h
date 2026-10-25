#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Factory and user presets plus A/B compare, for any APVTS-based plugin. The
// plugin supplies how to capture and restore its full state (parameters plus
// anything kept as ValueTree properties), so presets carry everything.
class PresetManager
{
public:
    struct Factory
    {
        juce::String name, category;
        std::vector<std::pair<juce::String, float>> values;   // parameter ID -> plain value
        std::vector<std::pair<juce::String, juce::String>> properties; // extra state
    };

    struct Entry
    {
        juce::String name, category;
        int factoryIndex = -1;   // or a user file
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState& s, std::vector<Factory> factoryPresets,
                   juce::String pluginFolder, juce::String fileExtension,
                   std::function<juce::ValueTree()> capture, std::function<void (const juce::ValueTree&)> restore)
        : state (s), factory (std::move (factoryPresets)), folder (std::move (pluginFolder)),
          extension (std::move (fileExtension)), captureState (std::move (capture)), restoreState (std::move (restore))
    {
        rescan();
    }

    //==========================================================================
    juce::File userDirectory() const
    {
        auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
       #if JUCE_MAC
        base = base.getChildFile ("Application Support");
       #endif
        return base.getChildFile (folder).getChildFile ("Presets");
    }

    void rescan()
    {
        entries.clear();
        for (int i = 0; i < (int) factory.size(); ++i)
            entries.push_back ({ factory[(size_t) i].name, factory[(size_t) i].category, i, {} });

        auto files = userDirectory().findChildFiles (juce::File::findFiles, false, "*" + extension);
        files.sort();
        for (auto& f : files)
            entries.push_back ({ f.getFileNameWithoutExtension(), "User", -1, f });
    }

    const std::vector<Entry>& getEntries() const { return entries; }
    int numFactory() const { return (int) factory.size(); }
    const Factory& getFactory (int i) const { return factory[(size_t) i]; }

    juce::String currentName() const { return state.state.getProperty (nameProperty, "Init").toString(); }

    int currentIndex() const
    {
        auto name = currentName();
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].name == name)
                return i;
        return -1;
    }

    void load (int entryIndex)
    {
        if (entryIndex < 0 || entryIndex >= (int) entries.size())
            return;
        const auto& e = entries[(size_t) entryIndex];
        if (e.factoryIndex >= 0)
            loadFactory (e.factoryIndex);
        else
            loadFile (e.file);
    }

    void step (int delta)
    {
        if (entries.empty())
            return;
        auto n = (int) entries.size();
        auto i = currentIndex();
        load (i < 0 ? 0 : ((i + delta) % n + n) % n);
    }

    void loadFactory (int i)
    {
        const auto& p = factory[(size_t) juce::jlimit (0, (int) factory.size() - 1, i)];

        // Start from defaults so a preset only lists what it changes.
        for (auto* param : state.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (param))
                r->setValueNotifyingHost (r->getDefaultValue());

        for (auto& [id, value] : p.values)
            if (auto* r = state.getParameter (id))
                r->setValueNotifyingHost (r->convertTo0to1 (value));

        auto tree = captureState();
        for (auto& [key, value] : p.properties)
            tree.setProperty (key, value, nullptr);
        tree.setProperty (nameProperty, p.name, nullptr);
        restoreState (tree);
        notify();
    }

    bool loadFile (const juce::File& f)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
            return false;
        auto tree = juce::ValueTree::fromXml (*xml);
        tree.setProperty (nameProperty, f.getFileNameWithoutExtension(), nullptr);
        restoreState (tree);
        notify();
        return true;
    }

    bool save (const juce::String& name)
    {
        auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty())
            return false;

        auto dir = userDirectory();
        dir.createDirectory();
        state.state.setProperty (nameProperty, clean, nullptr);
        auto tree = captureState();
        auto ok = false;
        if (auto xml = tree.createXml())
            ok = xml->writeTo (dir.getChildFile (clean + extension));
        rescan();
        notify();
        return ok;
    }

    //==========================================================================
    // A/B: two full states to flip between. B starts as a copy of A.
    int activeSlot() const { return slot; }

    void selectSlot (int newSlot)
    {
        if (newSlot == slot)
            return;
        slots[(size_t) slot] = captureState();
        slot = newSlot;
        if (slots[(size_t) slot].isValid())
            restoreState (slots[(size_t) slot]);
        notify();
    }

    void copyToOther()
    {
        slots[(size_t) (1 - slot)] = captureState();
    }

    std::function<void()> onChange;

    static constexpr const char* nameProperty = "presetName";

private:
    void notify()
    {
        if (onChange != nullptr)
            onChange();
    }

    juce::AudioProcessorValueTreeState& state;
    std::vector<Factory> factory;
    juce::String folder, extension;
    std::function<juce::ValueTree()> captureState;
    std::function<void (const juce::ValueTree&)> restoreState;
    std::vector<Entry> entries;
    std::array<juce::ValueTree, 2> slots;
    int slot = 0;
};
