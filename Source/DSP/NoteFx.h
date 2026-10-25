#pragma once

#include "Sequencer.h"
#include <array>

namespace rf
{

// Turns each incoming note into a chord, optionally snapping it into a key
// first. It remembers what every key produced, so note-offs always match
// their note-ons even if the chord or scale changes while a key is held.
class NoteMapper
{
public:
    static const juce::StringArray& chordNames()
    {
        static const juce::StringArray names { "Off", "Octave", "Fifth", "Major", "Minor", "Sus2", "Sus4",
                                               "Maj7", "Min7", "Min9", "Rave Stab", "Scale Triad", "Scale 7th" };
        return names;
    }

    struct Settings
    {
        bool snap = false;
        int chord = 0;
        int root = 0, scale = 1;
    };

    void reset()
    {
        for (auto& m : memory)
            m.count = 0;
    }

    void process (juce::MidiBuffer& midi, juce::MidiBuffer& scratch, const Settings& s)
    {
        if (! s.snap && s.chord == 0 && ! anyHeld)
            return;

        scratch.clear();
        anyHeld = false;

        for (const auto m : midi)
        {
            auto msg = m.getMessage();
            if (! msg.isNoteOnOrOff())
            {
                scratch.addEvent (msg, m.samplePosition);
                continue;
            }

            auto& slot = memory[(size_t) ((msg.getChannel() - 1) * 128 + msg.getNoteNumber())];
            for (int i = 0; i < slot.count; ++i)
                scratch.addEvent (juce::MidiMessage::noteOff (msg.getChannel(), slot.notes[(size_t) i]), m.samplePosition);
            slot.count = 0;

            if (msg.isNoteOn())
            {
                build (msg.getNoteNumber(), s, slot);
                for (int i = 0; i < slot.count; ++i)
                    scratch.addEvent (juce::MidiMessage::noteOn (msg.getChannel(), slot.notes[(size_t) i], msg.getVelocity()),
                                      m.samplePosition);
            }
        }

        for (auto& m : memory)
            anyHeld = anyHeld || m.count > 0;

        midi.swapWith (scratch);
    }

    // The notes one key produces, for displays.
    static std::vector<int> notesFor (int note, const Settings& s)
    {
        Slot slot;
        build (note, s, slot);
        return { slot.notes.begin(), slot.notes.begin() + slot.count };
    }

private:
    struct Slot { std::array<int, 6> notes {}; int count = 0; };

    static void build (int note, const Settings& s, Slot& slot)
    {
        auto base = s.snap ? Scales::snap (note, s.root, s.scale) : note;

        // Fixed shapes (no allocation: this runs on the audio thread). 99 ends a list.
        static constexpr int fixed[][6] {
            { 0, 99 }, { 0, 12, 99 }, { 0, 7, 12, 99 }, { 0, 4, 7, 99 }, { 0, 3, 7, 99 }, { 0, 2, 7, 99 }, { 0, 5, 7, 99 },
            { 0, 4, 7, 11, 99 }, { 0, 3, 7, 10, 99 }, { 0, 3, 7, 10, 14, 99 }, { -12, 0, 3, 7, 12, 99 } };

        std::array<int, 6> notes {};
        int count = 0;
        if (s.chord < (int) std::size (fixed))
        {
            for (auto i : fixed[juce::jmax (0, s.chord)])
            {
                if (i == 99) break;
                notes[(size_t) count++] = base + i;
            }
        }
        else
        {
            // Stacked thirds inside the key.
            auto d = Scales::degreeOf (base, s.root, s.scale);
            count = s.chord == 11 ? 3 : 4;
            for (int i = 0; i < count; ++i)
                notes[(size_t) i] = Scales::noteForDegree (d + 2 * i, s.root, s.scale);
        }

        for (int k = 0; k < count; ++k)
        {
            auto n = notes[(size_t) k];
            if (n < 0 || n > 127 || slot.count >= (int) slot.notes.size())
                continue;
            if (std::find (slot.notes.begin(), slot.notes.begin() + slot.count, n) == slot.notes.begin() + slot.count)
                slot.notes[(size_t) slot.count++] = n;
        }
    }

    std::array<Slot, 16 * 128> memory {};
    bool anyHeld = false;
};

// Arpeggiates held notes on the host's grid.
class Arp
{
public:
    static const juce::StringArray& modeNames()
    {
        static const juce::StringArray names { "Up", "Down", "Up/Down", "Random", "As Played", "Chord" };
        return names;
    }

    struct Settings
    {
        bool on = false;
        int mode = 0;
        double stepBeats = 0.25;
        int octaves = 1;
        float gate = 0.6f;
        bool latch = false;
    };

    void reset()
    {
        heldCount = 0;
        activeCount = 0;
        index = 0;
        down.fill (false);
    }

    void process (juce::MidiBuffer& midi, juce::MidiBuffer& scratch, int numSamples, double beat0, double beatsPerSample,
                  const Settings& s)
    {
        auto beatEnd = beat0 + beatsPerSample * numSamples;
        auto toOffset = [&] (double beat)
        {
            return juce::jlimit (0, numSamples - 1, (int) std::floor ((beat - beat0) / beatsPerSample));
        };

        if (! s.on)
        {
            if (wasOn)
            {
                releaseActive (midi, 0);
                reset();
                wasOn = false;
            }
            return;
        }
        wasOn = true;

        // Take the played notes out of the stream and track what's held.
        scratch.clear();
        auto hadNotes = heldCount > 0;
        for (const auto m : midi)
        {
            auto msg = m.getMessage();
            if (msg.isNoteOn())
            {
                if (s.latch && ! anyDown())
                    heldCount = 0;
                down[(size_t) msg.getNoteNumber()] = true;
                addHeld (msg.getNoteNumber(), msg.getVelocity());
            }
            else if (msg.isNoteOff())
            {
                down[(size_t) msg.getNoteNumber()] = false;
                if (! s.latch)
                    removeHeld (msg.getNoteNumber());
            }
            else
            {
                scratch.addEvent (msg, m.samplePosition);
            }
        }
        midi.swapWith (scratch);

        if (! s.latch && latched)
            for (int i = heldCount; --i >= 0;)
                if (! down[(size_t) held[(size_t) i].note])
                    removeHeld (held[(size_t) i].note);
        latched = s.latch;

        if (! hadNotes && heldCount > 0)
            index = 0;

        // A jump in the host position: drop what's sounding.
        if (beat0 < lastBeatEnd - 1.0e-6 || beat0 > lastBeatEnd + 1.0)
            releaseActive (midi, 0);
        lastBeatEnd = beatEnd;

        if (activeCount > 0 && (offBeat < beatEnd || heldCount == 0))
            releaseActive (midi, heldCount == 0 ? 0 : toOffset (offBeat));

        if (heldCount == 0)
            return;

        auto firstStep = (long long) std::ceil (beat0 / s.stepBeats - 1.0e-9);
        for (auto step = firstStep; (double) step * s.stepBeats < beatEnd; ++step)
        {
            auto stepBeat = (double) step * s.stepBeats;
            auto offset = toOffset (stepBeat);
            releaseActive (midi, offset);

            buildOrder (s);
            if (orderCount == 0)
                break;

            if (s.mode == 5) // chord: everything at once
            {
                for (int i = 0; i < heldCount; ++i)
                    play (midi, held[(size_t) i].note, held[(size_t) i].velocity, offset);
            }
            else
            {
                int pick;
                if (s.mode == 3)
                    pick = random.nextInt (orderCount);
                else if (s.mode == 2 && orderCount > 1)
                {
                    auto cycle = 2 * (orderCount - 1);
                    auto p = index % cycle;
                    pick = p < orderCount ? p : cycle - p;
                }
                else
                    pick = index % orderCount;

                play (midi, order[(size_t) pick].note, order[(size_t) pick].velocity, offset);
                ++index;
            }

            offBeat = stepBeat + s.stepBeats * juce::jlimit (0.05, 1.0, (double) s.gate);
            if (offBeat < beatEnd)
                releaseActive (midi, juce::jmax (juce::jmin (offset + 1, numSamples - 1), toOffset (offBeat)));
        }
    }

private:
    struct Note { int note = 0; int velocity = 100; };

    bool anyDown() const
    {
        for (auto d : down)
            if (d) return true;
        return false;
    }

    void addHeld (int note, int velocity)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i].note == note)
                return;
        if (heldCount < (int) held.size())
            held[(size_t) heldCount++] = { note, velocity };
    }

    void removeHeld (int note)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i].note == note)
            {
                for (int j = i; j < heldCount - 1; ++j)
                    held[(size_t) j] = held[(size_t) j + 1];
                --heldCount;
                return;
            }
    }

    void buildOrder (const Settings& s)
    {
        std::array<Note, 32> base {};
        for (int i = 0; i < heldCount; ++i)
            base[(size_t) i] = held[(size_t) i];

        if (s.mode != 4)
            std::sort (base.begin(), base.begin() + heldCount, [] (auto a, auto b) { return a.note < b.note; });

        orderCount = 0;
        auto octaves = juce::jlimit (1, 4, s.octaves);
        for (int o = 0; o < octaves; ++o)
            for (int i = 0; i < heldCount && orderCount < (int) order.size(); ++i)
                if (base[(size_t) i].note + 12 * o <= 127)
                    order[(size_t) orderCount++] = { base[(size_t) i].note + 12 * o, base[(size_t) i].velocity };

        if (s.mode == 1)
            std::reverse (order.begin(), order.begin() + orderCount);
    }

    void play (juce::MidiBuffer& midi, int note, int velocity, int offset)
    {
        if (activeCount >= (int) active.size())
            return;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)), offset);
        active[(size_t) activeCount++] = note;
    }

    void releaseActive (juce::MidiBuffer& midi, int offset)
    {
        for (int i = 0; i < activeCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (1, active[(size_t) i]), offset);
        activeCount = 0;
    }

    std::array<Note, 32> held {};
    std::array<Note, 128> order {};
    std::array<int, 32> active {};
    std::array<bool, 128> down {};
    int heldCount = 0, orderCount = 0, activeCount = 0, index = 0;
    double offBeat = 0.0, lastBeatEnd = 0.0;
    bool wasOn = false, latched = false;
    juce::Random random;
};

} // namespace rf
