#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
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

    inline const Scale& get (int index)
    {
        return all()[(size_t) juce::jlimit (0, (int) all().size() - 1, index)];
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
        const auto& scale = get (scaleIndex);
        auto rel = ((note - root) % 12 + 12) % 12;
        for (int d = 0; d < 12; ++d)
            for (int i = 0; i < scale.size; ++i)
                if (scale.steps[(size_t) i] == (rel - d + 12) % 12)
                    return note - d;
        return note;
    }

    // Number of pattern rows: every scale note over `octaves`, plus the top root.
    inline int degreeCount (int scaleIndex, int octaves)
    {
        return get (scaleIndex).size * juce::jlimit (1, 3, octaves) + 1;
    }

    // MIDI note for a scale degree counted up from the root in octave 3.
    inline int noteForDegree (int degree, int root, int scaleIndex, int baseNote = 48)
    {
        const auto& scale = get (scaleIndex);
        auto octave = degree >= 0 ? degree / scale.size : (degree - scale.size + 1) / scale.size;
        auto index = degree - octave * scale.size;
        return baseNote + root + 12 * octave + scale.steps[(size_t) index];
    }

    // The scale degree of `note` (snapped into the scale first), relative to `baseNote + root`.
    inline int degreeOf (int note, int root, int scaleIndex, int baseNote = 48)
    {
        const auto& scale = get (scaleIndex);
        auto snapped = snap (note, root, scaleIndex) - baseNote - root;
        auto octave = (int) std::floor (snapped / 12.0);
        auto rel = snapped - octave * 12;
        for (int i = 0; i < scale.size; ++i)
            if (scale.steps[(size_t) i] == rel)
                return octave * scale.size + i;
        return octave * scale.size;
    }
}

// A 16-step note pattern the audio thread can read while the editor edits it.
// Each step is a rest or a scale degree with a velocity.
class Pattern
{
public:
    static constexpr int maxSteps = 16;

    struct Step
    {
        int degree = -1;          // -1 = rest
        int velocity = 100;
        bool isRest() const { return degree < 0; }
    };

    Step get (int i) const
    {
        auto v = cells[(size_t) i].load (std::memory_order_relaxed);
        if (v == 0)
            return {};
        return { (v & 0xff) - 1, (v >> 8) & 0x7f };
    }

    void set (int i, Step s)
    {
        cells[(size_t) i].store (s.isRest() ? 0 : ((s.degree + 1) & 0xff) | (juce::jlimit (1, 127, s.velocity) << 8));
        ++version;
    }

    void clear()
    {
        for (int i = 0; i < maxSteps; ++i)
            set (i, {});
    }

    // A fresh pattern from `seed`. The first step always plays the root.
    void generate (int seed, float chance, int degrees)
    {
        for (int i = 0; i < maxSteps; ++i)
        {
            juce::Random rng ((juce::int64) seed * 7919 + i * 104729);
            auto play = i == 0 || rng.nextFloat() < chance;
            auto pitchRoll = rng.nextFloat();
            auto velocity = juce::roundToInt (127.0f * (0.65f + 0.35f * rng.nextFloat()));
            if (i == 0)
                pitchRoll = 0.0f;
            set (i, play ? Step { juce::jmin (degrees - 1, (int) (pitchRoll * (float) degrees)), velocity } : Step {});
        }
    }

    // "3:100 - 5:90 ..." for saving in the plugin state.
    juce::String toString() const
    {
        juce::StringArray parts;
        for (int i = 0; i < maxSteps; ++i)
        {
            auto s = get (i);
            parts.add (s.isRest() ? "-" : juce::String (s.degree) + ":" + juce::String (s.velocity));
        }
        return parts.joinIntoString (" ");
    }

    void fromString (const juce::String& text)
    {
        auto parts = juce::StringArray::fromTokens (text, " ", "");
        for (int i = 0; i < maxSteps; ++i)
        {
            auto p = parts[i];
            if (p.isEmpty() || p == "-")
                set (i, {});
            else
                set (i, { juce::jlimit (0, 254, p.upToFirstOccurrenceOf (":", false, false).getIntValue()),
                          p.contains (":") ? p.fromFirstOccurrenceOf (":", false, false).getIntValue() : 100 });
        }
    }

    std::atomic<int> version { 0 }; // bumped on every edit, for the editor

private:
    std::array<std::atomic<int>, maxSteps> cells {};
};

// Plays a Pattern on a tempo-synced grid. The step comes straight from the
// host's beat position, so the pattern lands in the same place every time
// the DAW plays it.
class Sequencer
{
public:
    struct Settings
    {
        bool on = false;
        int root = 0;             // 0 = C
        int scale = 1;
        double stepBeats = 0.25;  // 1/16
        int octaves = 1;
        int steps = 16;           // loop length
        float length = 0.5f;      // note length as a fraction of a step
        float swing = 0.0f;       // 0..1, delays every second step by up to half a step
    };

    void reset() { activeNote = -1; currentStep.store (-1); }

    // Adds note on/offs for this block into `midi`. `beat0` is the block's
    // start position in beats (from the DAW while it plays).
    void process (juce::MidiBuffer& midi, int numSamples, double beat0, double beatsPerSample,
                  const Pattern& pattern, const Settings& s)
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
        {
            currentStep.store (-1);
            return;
        }

        if (activeNote >= 0 && offBeat < beatEnd)
            noteOff (midi, toOffset (offBeat));

        auto steps = juce::jlimit (1, Pattern::maxSteps, s.steps);
        auto degrees = Scales::degreeCount (s.scale, s.octaves);
        auto swingBeats = s.stepBeats * 0.5 * juce::jlimit (0.0f, 1.0f, s.swing);

        // Start one step back so a swung step from the previous slot isn't missed.
        auto firstStep = (long long) std::floor (beat0 / s.stepBeats) - 1;
        for (auto step = firstStep; (double) step * s.stepBeats < beatEnd; ++step)
        {
            auto stepBeat = (double) step * s.stepBeats + ((step & 1) != 0 ? swingBeats : 0.0);
            if (stepBeat < beat0 || stepBeat >= beatEnd)
                continue;

            auto offset = toOffset (stepBeat);
            auto pos = (int) (((step % steps) + steps) % steps);
            currentStep.store (pos);

            if (activeNote >= 0)
                noteOff (midi, offset);

            auto st = pattern.get (pos);
            if (st.isRest())
                continue;

            activeNote = juce::jlimit (0, 127, Scales::noteForDegree (juce::jmin (st.degree, degrees - 1), s.root, s.scale));
            midi.addEvent (juce::MidiMessage::noteOn (channel, activeNote, (juce::uint8) st.velocity), offset);
            // Never past the next step's onset, which swing can pull earlier.
            auto nextBeat = (double) (step + 1) * s.stepBeats + (((step + 1) & 1) != 0 ? swingBeats : 0.0);
            offBeat = juce::jmin (nextBeat - 1.0e-6, stepBeat + s.stepBeats * juce::jlimit (0.05, 1.0, (double) s.length));

            if (offBeat < beatEnd)
                noteOff (midi, juce::jmax (juce::jmin (offset + 1, numSamples - 1), toOffset (offBeat)));
        }
    }

    std::atomic<int> currentStep { -1 }; // for the editor's playhead

    static constexpr int channel = 16; // keeps generated notes apart from played ones

private:
    void noteOff (juce::MidiBuffer& midi, int offset)
    {
        midi.addEvent (juce::MidiMessage::noteOff (channel, activeNote), offset);
        activeNote = -1;
    }

    int activeNote = -1;
    double offBeat = 0.0, lastBeatEnd = 0.0;
};

} // namespace rf
