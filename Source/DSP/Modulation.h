#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace rf
{

// Envelope follower on the input level, used to push the filter cutoff.
class EnvelopeFollower
{
public:
    void prepare (double sr) { sampleRate = sr; env = 0.0f; }
    void reset() { env = 0.0f; }

    void setTimes (float attackMs, float releaseMs)
    {
        attack  = coeff (attackMs);
        release = coeff (releaseMs);
    }

    float process (float x)
    {
        auto level = std::abs (x);
        auto c = level > env ? attack : release;
        env = level + c * (env - level);
        return env;
    }

private:
    float coeff (float ms) const { return (float) std::exp (-1.0 / (sampleRate * ms * 0.001)); }

    double sampleRate = 44100.0;
    float env = 0.0f, attack = 0.0f, release = 0.0f;
};

// Bipolar LFO (-1..1). Synced mode derives its phase straight from the host's
// beat position so it stays locked to the bar; free mode runs on its own.
class Lfo
{
public:
    enum class Shape { sine, triangle, saw, square, sampleHold };

    void prepare (double sr) { sampleRate = sr; reset(); }
    void reset() { freePhase = 0.0; lastPhase = 0.0; heldValue = 0.0f; }

    void setShape (Shape s) { shape = s; }

    float processSynced (double beat, double beatsPerCycle)
    {
        auto p = beat / beatsPerCycle;
        return valueAt (p - std::floor (p));
    }

    float processFree (float hz)
    {
        freePhase += hz / sampleRate;
        freePhase -= std::floor (freePhase);
        return valueAt (freePhase);
    }

private:
    float valueAt (double phase)
    {
        auto wrapped = phase < lastPhase; // new cycle started
        lastPhase = phase;
        auto p = (float) phase;

        switch (shape)
        {
            case Shape::triangle:   return 1.0f - 4.0f * std::abs (p - 0.5f);
            case Shape::saw:        return 2.0f * p - 1.0f;
            case Shape::square:     return p < 0.5f ? 1.0f : -1.0f;
            case Shape::sampleHold:
                if (wrapped)
                    heldValue = random.nextFloat() * 2.0f - 1.0f;
                return heldValue;
            case Shape::sine:
            default:                return std::sin (juce::MathConstants<float>::twoPi * p);
        }
    }

    double sampleRate = 44100.0, freePhase = 0.0, lastPhase = 0.0;
    Shape shape = Shape::sine;
    float heldValue = 0.0f;
    juce::Random random;
};

// 16-step trance gate. Returns a gain (0..1) for each sample.
class TranceGate
{
public:
    static constexpr int numSteps = 16;
    using Pattern = std::array<std::uint8_t, numSteps>;

    static const Pattern& pattern (int index)
    {
        static const std::array<Pattern, 6> patterns {{
            { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 }, // Pulse
            { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0 }, // Offbeat
            { 1,0,1,1, 1,0,1,1, 1,0,1,1, 1,0,1,1 }, // Gallop
            { 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,0,1,0 }, // Stutter
            { 1,0,0,1, 0,0,1,0, 0,1,0,0, 1,0,1,0 }, // Syncopate
            { 1,1,1,1, 0,0,0,0, 1,1,0,0, 1,0,1,0 }, // Builder
        }};
        return patterns[(size_t) juce::jlimit (0, 5, index)];
    }

    void prepare (double sr) { sampleRate = sr; gain = 1.0f; }
    void reset() { gain = 1.0f; }

    void setSettings (bool enabled, int patternIndex, double stepBeats, float depth, float smooth)
    {
        on = enabled;
        current = &pattern (patternIndex);
        beatsPerStep = stepBeats;
        gateDepth = depth;
        // 0.5 ms (clicky) up to ~25 ms (soft pumping) per transition.
        smoothCoeff = (float) std::exp (-1.0 / (sampleRate * (0.0005 + 0.025 * smooth)));
    }

    int stepAt (double beat) const
    {
        auto s = (long long) std::floor (beat / beatsPerStep);
        return (int) (((s % numSteps) + numSteps) % numSteps);
    }

    float process (double beat)
    {
        float target = 1.0f;
        if (on && ! (*current)[(size_t) stepAt (beat)])
            target = 1.0f - gateDepth;

        gain = target + smoothCoeff * (gain - target);
        return gain;
    }

private:
    double sampleRate = 44100.0, beatsPerStep = 0.25;
    const Pattern* current = &pattern (0);
    bool on = false;
    float gateDepth = 1.0f, smoothCoeff = 0.0f, gain = 1.0f;
};

} // namespace rf
