#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace rf
{

namespace Scales
{
    struct Scale { const char* name; std::array<int, 12> steps; int size; };

    inline const std::array<Scale, 10>& all()
    {
        static const std::array<Scale, 10> scales {{
            { "Major",          { 0, 2, 4, 5, 7, 9, 11 }, 7 },
            { "Minor",          { 0, 2, 3, 5, 7, 8, 10 }, 7 },
            { "Harmonic Minor", { 0, 2, 3, 5, 7, 8, 11 }, 7 },
            { "Dorian",         { 0, 2, 3, 5, 7, 9, 10 }, 7 },
            { "Phrygian",       { 0, 1, 3, 5, 7, 8, 10 }, 7 },
            { "Mixolydian",     { 0, 2, 4, 5, 7, 9, 10 }, 7 },
            { "Minor Pent",     { 0, 3, 5, 7, 10 }, 5 },
            { "Major Pent",     { 0, 2, 4, 7, 9 }, 5 },
            { "Blues",          { 0, 3, 5, 6, 7, 10 }, 6 },
            { "Chromatic",      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },
        }};
        return scales;
    }

    inline juce::StringArray names()
    {
        juce::StringArray n;
        for (auto& s : all())
            n.add (s.name);
        return n;
    }

    // Moves `note` down to the nearest note in the scale.
    inline int snap (int note, int root, int scaleIndex)
    {
        const auto& scale = all()[(size_t) juce::jlimit (0, (int) all().size() - 1, scaleIndex)];
        auto rel = ((note - root) % 12 + 12) % 12;
        for (int d = 0; d < 12; ++d)
            for (int i = 0; i < scale.size; ++i)
                if (scale.steps[(size_t) i] == (rel - d + 12) % 12)
                    return note - d;
        return note;
    }
}

// Generates random notes from a scale on a tempo-synced grid. The pattern is
// a 16-step loop derived from `seed` and the host's beat position, so it
// repeats every loop and lands in the same place every time the DAW plays it.
class Randomiser
{
public:
    static constexpr int loopSteps = 16;

    struct Settings
    {
        bool on = false;
        int root = 0;             // 0 = C
        int scale = 1;
        double stepBeats = 0.25;  // 1/16
        int octaves = 1;
        float chance = 0.7f;      // how many steps play
        float length = 0.5f;      // note length as a fraction of a step
        int seed = 1;
    };

    void reset() { activeNote = -1; }

    // Adds note on/offs for this block into `midi`. `beat0` is the block's
    // start position in beats (from the DAW while it plays).
    void process (juce::MidiBuffer& midi, int numSamples, double beat0, double beatsPerSample, const Settings& s)
    {
        auto beatEnd = beat0 + beatsPerSample * numSamples;
        auto toOffset = [&] (double beat)
        {
            return juce::jlimit (0, numSamples - 1, (int) std::floor ((beat - beat0) / beatsPerSample));
        };

        // A jump (loop, seek, stop) or switching off: release what's sounding.
        if (activeNote >= 0 && (! s.on || beat0 < lastBeatEnd - 1.0e-6 || beat0 > lastBeatEnd + 1.0))
            noteOff (midi, 0);

        lastBeatEnd = beatEnd;
        if (! s.on)
            return;

        if (activeNote >= 0 && offBeat < beatEnd)
            noteOff (midi, toOffset (offBeat));

        auto firstStep = (long long) std::ceil (beat0 / s.stepBeats - 1.0e-9);
        for (auto step = firstStep; (double) step * s.stepBeats < beatEnd; ++step)
        {
            auto stepBeat = (double) step * s.stepBeats;
            auto offset = toOffset (stepBeat);

            if (activeNote >= 0)
                noteOff (midi, offset);

            auto pos = (int) (((step % loopSteps) + loopSteps) % loopSteps);
            juce::Random rng ((juce::int64) s.seed * 7919 + pos * 104729);
            auto play = rng.nextFloat() < s.chance;
            auto pitchRoll = rng.nextFloat();
            auto velocity = 0.65f + 0.35f * rng.nextFloat();
            if (! play)
                continue;

            const auto& scale = Scales::all()[(size_t) juce::jlimit (0, (int) Scales::all().size() - 1, s.scale)];
            auto count = scale.size * juce::jmax (1, s.octaves) + 1; // includes the top root
            auto degree = juce::jmin (count - 1, (int) (pitchRoll * (float) count));
            activeNote = 48 + s.root + 12 * (degree / scale.size) + scale.steps[(size_t) (degree % scale.size)];

            midi.addEvent (juce::MidiMessage::noteOn (channel, activeNote, velocity), offset);
            offBeat = stepBeat + s.stepBeats * juce::jlimit (0.05, 1.0, (double) s.length);

            if (offBeat < beatEnd)
                noteOff (midi, juce::jmax (juce::jmin (offset + 1, numSamples - 1), toOffset (offBeat)));
        }
    }

private:
    void noteOff (juce::MidiBuffer& midi, int offset)
    {
        midi.addEvent (juce::MidiMessage::noteOff (channel, activeNote), offset);
        activeNote = -1;
    }

    static constexpr int channel = 16; // keeps generated notes apart from played ones
    int activeNote = -1;
    double offBeat = 0.0, lastBeatEnd = 0.0;
};

} // namespace rf
