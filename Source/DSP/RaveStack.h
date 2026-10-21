#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

namespace rf
{

// Mono history buffer that all the pitch-shift voices read from.
class History
{
public:
    void prepare (double sampleRate)
    {
        // Enough for the longest grain window (60 ms) at 192 kHz, rounded up to a power of two.
        auto needed = (int) std::ceil (sampleRate * 0.07) + 8;
        size = juce::nextPowerOfTwo (juce::jmax (needed, 1024));
        mask = size - 1;
        data.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset() { std::fill (data.begin(), data.end(), 0.0f); }

    void push (float x)
    {
        data[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    // Cubic (Hermite) read, `delay` samples behind the most recent write.
    float read (float delay) const
    {
        auto pos = (float) writePos - 1.0f - delay;
        auto i = (int) std::floor (pos);
        auto f = pos - (float) i;

        auto xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        auto c1 = 0.5f * (x1 - xm1);
        auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    float at (int i) const { return data[(size_t) (i & mask)]; }

    std::vector<float> data;
    int size = 0, mask = 0, writePos = 0;
};

// Classic two-tap delay-line pitch shifter: each tap sweeps through a window
// while the other crossfades in, so the read rate (and so the pitch) differs
// from the write rate by `ratio`.
class PitchShifter
{
public:
    void prepare (double sr, float windowMs, float startPhase)
    {
        window = (float) (sr * windowMs / 1000.0);
        phase = startPhase;
    }

    void reset (float startPhase) { phase = startPhase; }

    float process (const History& h, float ratio)
    {
        phase += (1.0f - ratio) / window;
        phase -= std::floor (phase);

        auto p2 = phase + 0.5f;
        p2 -= std::floor (p2);

        // sin² gains for the two taps (half a cycle apart) always sum to 1.
        auto g1 = std::sin (juce::MathConstants<float>::pi * phase);
        auto g2 = std::sin (juce::MathConstants<float>::pi * p2);

        return h.read (2.0f + phase * window) * g1 * g1
             + h.read (2.0f + p2 * window) * g2 * g2;
    }

private:
    float window = 1000.0f, phase = 0.0f;
};

// The detuned "rave" stack: the input in the centre, plus up to six detuned
// copies that Power fades in (widest pair first) and Width pans apart, plus
// three pitched layers (sub octave, fifth, octave up).
class RaveStack
{
public:
    static constexpr int numVoices = 6;

    struct Settings
    {
        float power = 0.5f;      // 0..1
        float width = 0.7f;      // 0..1
        float detuneCents = 18.0f;
        float sub = 0.0f, fifth = 0.0f, octUp = 0.0f;
    };

    void prepare (double sampleRate)
    {
        history.prepare (sampleRate);

        for (int v = 0; v < numVoices; ++v)
            voices[(size_t) v].prepare (sampleRate, 30.0f + 4.0f * (float) v, (float) v / (float) numVoices);

        subShifter.prepare (sampleRate, 60.0f, 0.13f);
        fifthShifter.prepare (sampleRate, 45.0f, 0.37f);
        octShifter.prepare (sampleRate, 35.0f, 0.71f);

        for (auto* s : { &power, &width, &sub, &fifth, &octUp })
            s->reset (sampleRate, 0.03);
    }

    void reset()
    {
        history.reset();
        for (int v = 0; v < numVoices; ++v)
            voices[(size_t) v].reset ((float) v / (float) numVoices);
    }

    void setTargets (const Settings& s)
    {
        power.setTargetValue (s.power);
        width.setTargetValue (s.width);
        sub.setTargetValue (s.sub);
        fifth.setTargetValue (s.fifth);
        octUp.setTargetValue (s.octUp);
        detuneCents = s.detuneCents;
    }

    // `detuneScale` lets the LFO push the detune around (1 = as set).
    void process (float inL, float inR, float detuneScale, float& outL, float& outR)
    {
        auto mono = 0.5f * (inL + inR);
        history.push (mono);

        auto pw = power.getNextValue();
        auto wd = width.getNextValue();
        auto cents = detuneCents * detuneScale;

        // Voice order: widest pair first, so turning Power up opens the stack outwards.
        static constexpr float spread[numVoices] { 1.0f, -1.0f, 0.62f, -0.62f, 0.3f, -0.3f };

        float l = inL, r = inR, energy = 1.0f;

        for (int v = 0; v < numVoices; ++v)
        {
            auto g = juce::jlimit (0.0f, 1.0f, pw * (float) numVoices - (float) v);
            auto ratio = std::exp2 (spread[v] * cents / 1200.0f);
            auto s = voices[(size_t) v].process (history, ratio); // always run so phases stay continuous

            if (g <= 0.0f)
                continue;

            // Constant-power pan from -1..1.
            auto pan = spread[v] * wd;
            auto angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
            l += s * g * std::cos (angle) * juce::MathConstants<float>::sqrt2;
            r += s * g * std::sin (angle) * juce::MathConstants<float>::sqrt2;
            energy += g * g;
        }

        auto layer = subShifter.process (history, 0.5f) * sub.getNextValue()
                   + fifthShifter.process (history, 1.4983f) * fifth.getNextValue()
                   + octShifter.process (history, 2.0f) * octUp.getNextValue();
        layer *= 0.8f;

        // The copies are only partly correlated: somewhere between amplitude
        // (^1) and energy (^0.5) summing, so normalise in between.
        auto norm = std::pow (energy, -0.7f);
        outL = l * norm + layer;
        outR = r * norm + layer;
    }

private:
    History history;
    std::array<PitchShifter, numVoices> voices;
    PitchShifter subShifter, fifthShifter, octShifter;

    juce::SmoothedValue<float> power, width, sub, fifth, octUp;
    float detuneCents = 18.0f;
};

} // namespace rf
