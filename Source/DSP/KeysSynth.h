#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace rf
{

// A plain polyphonic saw played from the on-screen keyboard or host MIDI.
// It's mixed into the input before the rave stack, which does the fattening.
class KeysSynth
{
public:
    KeysSynth()
    {
        for (int i = 0; i < 8; ++i)
            synth.addVoice (new SawVoice());
        synth.addSound (new AnySound());
    }

    void prepare (double sampleRate, int maxBlockSize)
    {
        synth.setCurrentPlaybackSampleRate (sampleRate);
        buffer.setSize (2, maxBlockSize);
    }

    // Renders the synth and adds it into `out` at `level`.
    void renderAdding (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, float level)
    {
        auto n = out.getNumSamples();
        if (buffer.getNumSamples() < n)
            buffer.setSize (2, n, false, false, true);

        buffer.clear (0, n);
        synth.renderNextBlock (buffer, midi, 0, n);

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
            out.addFrom (ch, 0, buffer, juce::jmin (ch, 1), 0, n, level);
    }

private:
    struct AnySound : juce::SynthesiserSound
    {
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class SawVoice : public juce::SynthesiserVoice
    {
    public:
        bool canPlaySound (juce::SynthesiserSound*) override { return true; }

        void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
        {
            increment = juce::MidiMessage::getMidiNoteInHertz (note) / getSampleRate();
            gain = 0.25f * velocity;
            adsr.setSampleRate (getSampleRate());
            adsr.setParameters ({ 0.005f, 0.2f, 0.8f, 0.3f });
            adsr.noteOn();
        }

        void stopNote (float, bool allowTailOff) override
        {
            if (allowTailOff)
            {
                adsr.noteOff();
            }
            else
            {
                adsr.reset();
                clearCurrentNote();
            }
        }

        void pitchWheelMoved (int) override {}
        void controllerMoved (int, int) override {}

        void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
        {
            if (! isVoiceActive())
                return;

            for (int i = start; i < start + num; ++i)
            {
                auto s = (float) (2.0 * phase - 1.0 - polyBlep (phase)) * gain * adsr.getNextSample();
                phase += increment;
                if (phase >= 1.0)
                    phase -= 1.0;

                for (int ch = 0; ch < out.getNumChannels(); ++ch)
                    out.addSample (ch, i, s);

                if (! adsr.isActive())
                {
                    clearCurrentNote();
                    break;
                }
            }
        }

    private:
        // Smooths the saw's reset to cut aliasing.
        double polyBlep (double t) const
        {
            auto dt = increment;
            if (t < dt)       { t /= dt; return t + t - t * t - 1.0; }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
            return 0.0;
        }

        double phase = 0.0, increment = 0.0;
        float gain = 0.0f;
        juce::ADSR adsr;
    };

    juce::Synthesiser synth;
    juce::AudioBuffer<float> buffer;
};

} // namespace rf
