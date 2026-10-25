#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace rf
{

// Tempo-synced "sidechain" ducking: dips on every beat (or bar, or 1/8) and
// swells back up, the way a kick-triggered compressor would.
class Pump
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        smooth = (float) std::exp (-1.0 / (sr * 0.0015));
        reset();
    }

    void reset() { gain = 1.0f; }

    // `release` is how much of the period the swell back up takes (0..1).
    float process (double beat, double periodBeats, float depth, float release)
    {
        auto p = beat / periodBeats;
        auto phase = (float) (p - std::floor (p));
        auto r = 0.08f + 0.9f * release;
        auto x = juce::jmin (1.0f, phase / r);
        auto curve = x * x * (3.0f - 2.0f * x); // smoothstep back to full level
        auto targetGain = 1.0f - depth * (1.0f - curve);
        gain = targetGain + smooth * (gain - targetGain); // ~1.5 ms so the dip doesn't click
        return gain;
    }

private:
    double sampleRate = 44100.0;
    float smooth = 0.0f, gain = 1.0f;
};

// Compressor with makeup gain, then an optional brickwall-ish limiter at the
// very end of the chain. Tracks gain reduction for the editor's meter.
class Dynamics
{
public:
    struct Settings
    {
        bool compOn = false;
        float threshold = -12.0f, ratio = 4.0f, attackMs = 10.0f, releaseMs = 120.0f, makeupDb = 0.0f;
        bool limitOn = true;
        float ceilingDb = -0.3f;
    };

    void prepare (double sr, int maxBlockSize)
    {
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 2 };
        comp.prepare (spec);
        makeup.reset (sr, 0.05);
        limiterRelease = (float) std::exp (-1.0 / (sr * 0.08));
        reset();
    }

    void reset()
    {
        comp.reset();
        limiterEnv = 0.0f;
        gainReductionDb.store (0.0f);
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        comp.setThreshold (s.threshold);
        comp.setRatio (juce::jmax (1.0f, s.ratio));
        comp.setAttack (s.attackMs);
        comp.setRelease (s.releaseMs);
        makeup.setTargetValue (juce::Decibels::decibelsToGain (s.makeupDb));
        ceiling = juce::Decibels::decibelsToGain (s.ceilingDb);
    }

    void processCompressor (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        if (! settings.compOn)
        {
            gainReductionDb.store (gainReductionDb.load() * 0.8f);
            return;
        }

        float inSq = 0.0f, outSq = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            auto g = makeup.getNextValue();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                auto x = d[i];
                auto y = comp.processSample (ch, x);
                inSq += x * x;
                outSq += y * y;
                d[i] = y * g;
            }
        }

        auto gr = inSq > 1.0e-9f ? 10.0f * std::log10 ((outSq + 1.0e-12f) / inSq) : 0.0f;
        gainReductionDb.store (juce::jmin (0.0f, gr));
    }

    // Stereo-linked peak limiter: instant attack, smooth release, never adds gain.
    void processLimiter (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        if (! settings.limitOn)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int i = 0; i < numSamples; ++i)
        {
            auto peak = juce::jmax (std::abs (l[i]), r != nullptr ? std::abs (r[i]) : 0.0f);
            limiterEnv = peak > limiterEnv ? peak : peak + limiterRelease * (limiterEnv - peak);
            if (limiterEnv > ceiling)
            {
                auto g = ceiling / limiterEnv;
                l[i] *= g;
                if (r != nullptr)
                    r[i] *= g;
            }
        }
    }

    std::atomic<float> gainReductionDb { 0.0f };

private:
    Settings settings;
    juce::dsp::Compressor<float> comp;
    float ceiling = 1.0f, limiterEnv = 0.0f, limiterRelease = 0.999f;
    juce::SmoothedValue<float> makeup { 1.0f };
};

} // namespace rf
