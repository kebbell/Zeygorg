#pragma once

#include "RaveStack.h"
#include "Dirt.h"
#include "Modulation.h"
#include "Delay.h"
#include <atomic>

namespace rf
{

// The whole effect chain:
//   input -> rave stack -> filter (env + LFO) -> dirt -> LFO volume x gate
//         -> delay -> reverb -> dry/wet -> output
class Engine
{
public:
    struct Settings
    {
        RaveStack::Settings stack;

        int filterType = 0;          // 0 LP, 1 BP, 2 HP
        float cutoff = 20000.0f;
        float resonance = 0.1f;      // 0..1
        float envAmount = 0.0f;      // -1..1
        float envAttackMs = 10.0f, envReleaseMs = 150.0f;

        Dirt::Settings dirt;

        Lfo::Shape lfoShape = Lfo::Shape::sine;
        bool lfoSync = true;
        float lfoRateHz = 1.0f;
        double lfoBeats = 1.0;
        float lfoToCutoff = 0.0f, lfoToDetune = 0.0f, lfoToAmp = 0.0f;

        bool gateOn = false;
        int gatePattern = 0;
        double gateStepBeats = 0.25;
        float gateDepth = 1.0f, gateSmooth = 0.2f;

        double delayBeats = 0.75;
        float delayFeedback = 0.4f;
        bool delayPingPong = true;
        float delayTone = 5000.0f, delayMix = 0.0f;

        float revSize = 0.6f, revDamp = 0.4f, revWidth = 1.0f, revMix = 0.0f;

        float mix = 1.0f;
        float outputDb = 0.0f;
    };

    struct Transport
    {
        bool playing = false;
        double bpm = 0.0;     // 0 = unknown
        double ppq = 0.0;     // only meaningful when playing
    };

    void prepare (double sr, int maxBlockSize)
    {
        sampleRate = sr;

        stack.prepare (sr);
        dirt.prepare (sr);
        env.prepare (sr);
        lfo.prepare (sr);
        gate.prepare (sr);
        delay.prepare (sr);

        filter.prepare ({ sr, (juce::uint32) maxBlockSize, 2 });
        filter.reset();

        reverb.setSampleRate (sr);
        reverb.reset();

        dryBuffer.setSize (2, maxBlockSize);
        mix.reset (sr, 0.03);
        outGain.reset (sr, 0.03);
        freeBeat = 0.0;
    }

    void reset()
    {
        stack.reset();
        dirt.reset();
        env.reset();
        lfo.reset();
        gate.reset();
        delay.reset();
        filter.reset();
        reverb.reset();
    }

    void process (juce::AudioBuffer<float>& buffer, const Settings& s, const Transport& t)
    {
        auto numSamples = buffer.getNumSamples();
        auto numChannels = juce::jmin (2, buffer.getNumChannels());
        if (numSamples == 0 || numChannels == 0)
            return;

        if (dryBuffer.getNumSamples() < numSamples)
            dryBuffer.setSize (2, numSamples, false, false, true);

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        applySettings (s);

        auto bpm = t.bpm > 0.0 ? t.bpm : 128.0;
        auto beatsPerSample = bpm / 60.0 / sampleRate;
        delay.setSettings (s.delayBeats * 60.0 / bpm, s.delayFeedback, s.delayPingPong, s.delayTone, s.delayMix);
        auto beat0 = t.playing ? t.ppq : freeBeat;
        freeBeat = beat0 + beatsPerSample * numSamples;

        auto* left  = buffer.getWritePointer (0);
        auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        float lfoValue = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            auto beat = beat0 + beatsPerSample * i;

            lfoValue = s.lfoSync ? lfo.processSynced (beat, s.lfoBeats)
                                 : lfo.processFree (s.lfoRateHz);

            auto inL = left[i];
            auto inR = right != nullptr ? right[i] : inL;

            auto level = env.process (0.5f * (inL + inR));

            float l, r;
            stack.process (inL, inR, juce::jmax (0.0f, 1.0f + s.lfoToDetune * lfoValue), l, r);

            if ((i & 15) == 0)
            {
                // Envelope pushes up to ±5 octaves, the LFO up to ±4.
                auto octaves = s.envAmount * juce::jmin (1.0f, level * 4.0f) * 5.0f
                             + s.lfoToCutoff * lfoValue * 4.0f;
                auto fc = juce::jlimit (20.0f, (float) sampleRate * 0.45f, s.cutoff * std::exp2 (octaves));
                filter.setCutoffFrequency (fc);
            }

            l = filter.processSample (0, l);
            r = filter.processSample (1, r);

            dirt.process (l, r);

            // LFO > Volume dips the level on the LFO's low half (full level at +1).
            auto amp = (1.0f - s.lfoToAmp * (0.5f - 0.5f * lfoValue)) * gate.process (beat);
            l *= amp;
            r *= amp;

            delay.process (l, r);

            left[i] = l;
            if (right != nullptr)
                right[i] = r;
            else
                left[i] = 0.5f * (l + r);
        }

        if (right != nullptr)
            reverb.processStereo (left, right, numSamples);
        else
            reverb.processMono (left, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            auto m = mix.getNextValue();
            auto g = outGain.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                auto dry = dryBuffer.getSample (ch, i);
                d[i] = (dry + (d[i] - dry) * m) * g;
            }
        }

        currentGateStep.store (gate.stepAt (beat0 + beatsPerSample * (numSamples - 1)));
        currentLfo.store (lfoValue);
    }

    // Read by the editor for its displays.
    std::atomic<int> currentGateStep { 0 };
    std::atomic<float> currentLfo { 0.0f };

private:
    void applySettings (const Settings& s)
    {
        stack.setTargets (s.stack);
        dirt.setTargets (s.dirt);
        env.setTimes (s.envAttackMs, s.envReleaseMs);
        lfo.setShape (s.lfoShape);
        gate.setSettings (s.gateOn, s.gatePattern, s.gateStepBeats, s.gateDepth, s.gateSmooth);

        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (s.filterType == 1 ? T::bandpass : s.filterType == 2 ? T::highpass : T::lowpass);
        filter.setResonance (0.5f * std::pow (20.0f, s.resonance)); // Q 0.5..10

        juce::Reverb::Parameters rp;
        rp.roomSize = s.revSize;
        rp.damping = s.revDamp;
        rp.width = s.revWidth;
        // juce::Reverb scales dry by 2 and wet by 3 internally, so compensate.
        rp.wetLevel = s.revMix * 0.33f;
        rp.dryLevel = 0.5f * (1.0f - 0.4f * s.revMix);
        reverb.setParameters (rp);

        mix.setTargetValue (s.mix);
        outGain.setTargetValue (juce::Decibels::decibelsToGain (s.outputDb));
    }

    double sampleRate = 44100.0, freeBeat = 0.0;

    RaveStack stack;
    Dirt dirt;
    EnvelopeFollower env;
    Lfo lfo;
    TranceGate gate;
    StereoDelay delay;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Reverb reverb;

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> mix { 1.0f }, outGain { 1.0f };
};

} // namespace rf
