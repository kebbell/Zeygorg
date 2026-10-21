#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace rf
{

// Polyphonic keys played from the on-screen keyboard or host MIDI: either a
// plain saw or the selected sample, pitched from its root note. It's mixed
// into the input before the boost stack, which does the fattening.
class KeysSynth
{
public:
    struct Sample : juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<Sample>;
        juce::AudioBuffer<float> audio;
        double sampleRate = 44100.0;
        int rootNote = 60;
    };

    struct Settings
    {
        bool useSample = true;
        int transpose = 0;        // semitones
        float releaseMs = 250.0f;
        float level = 0.7f;
    };

    KeysSynth()
    {
        for (int i = 0; i < 12; ++i)
            synth.addVoice (new Voice (*this));
        synth.addSound (new AnySound());
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
        buffer.setSize (2, maxBlockSize);
    }

    // Message thread: swap in a newly loaded sample. Voices still playing the
    // old one keep it alive; it's freed later by collectGarbage(), never on
    // the audio thread.
    void setSample (Sample::Ptr s)
    {
        {
            const juce::SpinLock::ScopedLockType lock (sampleLock);
            std::swap (current, s);
        }
        if (s != nullptr)
            retired.add (s);
        collectGarbage();
    }

    void collectGarbage()
    {
        for (int i = retired.size(); --i >= 0;)
            if (retired.getUnchecked (i)->getReferenceCount() == 1)
                retired.remove (i);
    }

    // Renders the keys and adds them into `out`.
    void renderAdding (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Settings& s)
    {
        settings = s;
        {
            const juce::SpinLock::ScopedTryLockType lock (sampleLock);
            if (lock.isLocked())
                active = current;
        }

        auto n = out.getNumSamples();
        if (buffer.getNumSamples() < n)
            buffer.setSize (2, n, false, false, true);

        buffer.clear (0, n);
        synth.renderNextBlock (buffer, midi, 0, n);

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.addFrom (ch, 0, buffer, juce::jmin (ch, 1), 0, n, s.level);
    }

private:
    struct AnySound : juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class Voice : public juce::SynthesiserVoice
    {
    public:
        explicit Voice (KeysSynth& o) : owner (o) {}

        bool canPlaySound (juce::SynthesiserSound*) override { return true; }

        void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
        {
            const auto& s = owner.settings;
            auto pitch = note + s.transpose;

            sample = s.useSample ? owner.active : nullptr;
            if (sample != nullptr)
            {
                position = 0.0;
                increment = std::exp2 ((pitch - sample->rootNote) / 12.0) * sample->sampleRate / getSampleRate();
                gain = 0.5f * velocity;
            }
            else
            {
                phase = 0.0;
                increment = juce::MidiMessage::getMidiNoteInHertz (pitch) / getSampleRate();
                gain = 0.25f * velocity;
            }

            adsr.setSampleRate (getSampleRate());
            adsr.setParameters ({ 0.002f, 0.0f, 1.0f, s.releaseMs * 0.001f });
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
                adsr.noteOff();
            else
                finish();
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            auto* outL = out.getWritePointer (0);
            auto* outR = out.getWritePointer (1);

            for (int i = start; i < start + num; ++i)
            {
                float l, r;
                if (sample != nullptr)
                {
                    if (! readSample (l, r))
                    {
                        finish();
                        return;
                    }
                }
                else
                {
                    l = r = (float) (2.0 * phase - 1.0 - polyBlep (phase));
                    phase += increment;
                    if (phase >= 1.0)
                        phase -= 1.0;
                }

                auto g = gain * adsr.getNextSample();
                outL[i] += l * g;
                outR[i] += r * g;

                if (! adsr.isActive())
                {
                    finish();
                    return;
                }
            }
        }

    private:
        bool readSample (float& l, float& r)
        {
            const auto& audio = sample->audio;
            auto len = audio.getNumSamples();
            auto i = (int) position;
            if (i + 1 >= len)
                return false;

            auto f = (float) (position - i);
            auto ch1 = juce::jmin (1, audio.getNumChannels() - 1);
            auto* a = audio.getReadPointer (0);
            auto* b = audio.getReadPointer (ch1);
            l = a[i] + f * (a[i + 1] - a[i]);
            r = b[i] + f * (b[i + 1] - b[i]);
            position += increment;
            return true;
        }

        // Smooths the saw's reset to cut aliasing.
        double polyBlep (double t) const
        {
            auto dt = increment;
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        void finish()
        {
            adsr.reset();
            clearCurrentNote();
            sample = nullptr; // just drops a reference; the owner still holds it
        }

        KeysSynth& owner;
        Sample::Ptr sample;
        double phase = 0.0, position = 0.0, increment = 0.0;
        float gain = 0.0f;
        juce::ADSR adsr;
    };

    juce::Synthesiser synth;
    juce::AudioBuffer<float> buffer;
    Settings settings;

    juce::SpinLock sampleLock;
    Sample::Ptr current;                 // set by the message thread
    Sample::Ptr active;                  // the audio thread's copy
    juce::ReferenceCountedArray<Sample> retired;
};

} // namespace rf
