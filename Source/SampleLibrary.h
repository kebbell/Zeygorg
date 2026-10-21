#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include "DSP/KeysSynth.h"

// The samples compiled in from Samples/, sorted into categories. Each file's
// root note comes from its name ("VEC2 Synths 101 C#.wav"; H is German for B).
class SampleLibrary
{
public:
    struct Entry
    {
        juce::String category;   // "Synths", "Acid" or "HiString"
        juce::String name;       // e.g. "101 C#"
        const char* resource = nullptr;
        int rootNote = 60;
    };

    static const SampleLibrary& get()
    {
        static const SampleLibrary library;
        return library;
    }

    int size() const { return entries.size(); }
    const Entry& operator[] (int i) const { return entries.getReference (juce::jlimit (0, entries.size() - 1, i)); }
    juce::StringArray categories() const { return { "Synths", "Acid", "HiString" }; }

    // Decodes one sample. Call off the audio thread.
    rf::KeysSynth::Sample::Ptr load (int index) const
    {
        if (entries.isEmpty())
            return nullptr;

        const auto& e = (*this)[index];
        int bytes = 0;
        auto* data = BinaryData::getNamedResource (e.resource, bytes);
        if (data == nullptr)
            return nullptr;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (
            wav.createReaderFor (new juce::MemoryInputStream (data, (size_t) bytes, false), true));
        if (reader == nullptr)
            return nullptr;

        auto sample = new rf::KeysSynth::Sample();
        sample->audio.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&sample->audio, 0, (int) reader->lengthInSamples, 0, true, true);
        sample->sampleRate = reader->sampleRate;
        sample->rootNote = e.rootNote;
        return sample;
    }

private:
    SampleLibrary()
    {
        static const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H" };

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            juce::String file (BinaryData::originalFilenames[i]);
            if (! file.endsWithIgnoreCase (".wav"))
                continue;

            auto base = file.dropLastCharacters (4);
            Entry e;
            e.resource = BinaryData::namedResourceList[i];
            e.category = base.contains ("Acid") ? "Acid" : base.contains ("HiString") ? "HiString" : "Synths";

            // Strip the pack prefix ("VEC2 Synths HiString ", "VEC2 Acid Sounds ", ...).
            auto words = juce::StringArray::fromTokens (base, " ", "");
            e.name = words[words.size() - 2] + " " + words[words.size() - 1];

            auto note = noteNames.indexOf (words[words.size() - 1].replace ("B", "H"));
            e.rootNote = 60 + juce::jmax (0, note); // middle C plays C samples at their own pitch

            entries.add (e);
        }

        auto order = categories();
        std::sort (entries.begin(), entries.end(), [&order] (const Entry& a, const Entry& b)
        {
            auto ca = order.indexOf (a.category), cb = order.indexOf (b.category);
            return ca != cb ? ca < cb : a.name.compareNatural (b.name) < 0;
        });
    }

    juce::Array<Entry> entries;
};
