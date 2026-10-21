#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace rf
{

// Tempo-synced stereo delay with optional ping-pong and a low-pass "tone"
// control inside the feedback loop, so repeats get darker as they fade.
class StereoDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        // Longest division is a whole note; allow for 40 BPM.
        size = juce::nextPowerOfTwo ((int) (sr * 6.5));
        mask = size - 1;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        delaySamples.reset (sr, 0.15);
        mix.reset (sr, 0.03);
        writePos = 0;
        toneL = toneR = 0.0f;
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        toneL = toneR = 0.0f;
    }

    void setSettings (double seconds, float fb, bool pp, float toneHz, float mixAmount)
    {
        auto samples = (float) juce::jlimit (1.0, (double) size - 4.0, seconds * sampleRate);
        if (delaySamples.getTargetValue() <= 0.0f)
            delaySamples.setCurrentAndTargetValue (samples);
        else
            delaySamples.setTargetValue (samples);

        feedback = fb;
        pingPong = pp;
        toneCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * toneHz / sampleRate);
        mix.setTargetValue (mixAmount);
    }

    void process (float& l, float& r)
    {
        auto d = delaySamples.getNextValue();
        auto m = mix.getNextValue();

        auto wetL = read (bufL, d);
        auto wetR = read (bufR, d);

        toneL = wetL + toneCoeff * (toneL - wetL);
        toneR = wetR + toneCoeff * (toneR - wetR);

        float inL, inR;
        if (pingPong)
        {
            // Mono input enters on the left, and each repeat crosses sides.
            inL = 0.5f * (l + r) + toneR * feedback;
            inR = toneL * feedback;
        }
        else
        {
            inL = l + toneL * feedback;
            inR = r + toneR * feedback;
        }

        // Soft-limit what goes back in so high feedback can't run away.
        bufL[(size_t) writePos] = std::tanh (inL);
        bufR[(size_t) writePos] = std::tanh (inR);
        writePos = (writePos + 1) & mask;

        l = l * (1.0f - 0.5f * m) + toneL * m;
        r = r * (1.0f - 0.5f * m) + toneR * m;
    }

private:
    float read (const std::vector<float>& buf, float delay) const
    {
        auto pos = (float) writePos - delay;
        auto i = (int) std::floor (pos);
        auto f = pos - (float) i;
        auto a = buf[(size_t) (i & mask)];
        auto b = buf[(size_t) ((i + 1) & mask)];
        return a + f * (b - a);
    }

    double sampleRate = 44100.0;
    std::vector<float> bufL, bufR;
    int size = 0, mask = 0, writePos = 0;

    juce::SmoothedValue<float> delaySamples { 0.0f }, mix { 0.0f };
    float feedback = 0.4f, toneCoeff = 0.0f, toneL = 0.0f, toneR = 0.0f;
    bool pingPong = true;
};

} // namespace rf
