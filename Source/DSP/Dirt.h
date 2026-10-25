#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace rf
{

// Drive into a waveshaper (optionally oversampled to cut aliasing), then bit
// reduction and sample-rate reduction.
class Dirt
{
public:
    enum class Shape { soft, hard, fold, tube };

    struct Settings
    {
        float driveDb = 0.0f;
        Shape shape = Shape::soft;
        float bits = 16.0f;       // 2..16, 16 = off
        float downsample = 1.0f;  // 1..32, 1 = off
        float mix = 1.0f;
        int oversample = 0;       // 0 off, 1 = 2x, 2 = 4x
    };

    void prepare (double sampleRate, int maxBlockSize)
    {
        drive.reset (sampleRate, 0.03);
        mix.reset (sampleRate, 0.03);
        // ~10 Hz DC blocker for the asymmetric tube curve.
        dcCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 10.0 / sampleRate);

        for (int i = 0; i < 2; ++i)
        {
            oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
                2, (size_t) i + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            oversamplers[(size_t) i]->initProcessing ((size_t) maxBlockSize);
        }

        dry.setSize (2, maxBlockSize);
        gains.resize ((size_t) maxBlockSize);
        mixes.resize ((size_t) maxBlockSize);
        reset();
    }

    void reset()
    {
        holdL = holdR = 0.0f;
        holdCounter = 0.0f;
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
        for (auto& os : oversamplers)
            if (os != nullptr)
                os->reset();
    }

    void setTargets (const Settings& s)
    {
        drive.setTargetValue (juce::Decibels::decibelsToGain (s.driveDb));
        mix.setTargetValue (s.mix);
        shape = s.shape;
        bits = s.bits;
        downsample = s.downsample;
        oversample = juce::jlimit (0, 2, s.oversample);
    }

    // Skips all the work when the section is doing nothing.
    bool isBypassed() const
    {
        return ! drive.isSmoothing() && ! mix.isSmoothing()
            && drive.getTargetValue() <= 1.0001f && bits >= 15.95f && downsample <= 1.01f && shape != Shape::fold;
    }

    void process (juce::AudioBuffer<float>& buffer, int numChannels, int numSamples)
    {
        if (gains.size() < (size_t) numSamples)
            return; // larger than prepared for; leave it clean rather than allocate

        for (int ch = 0; ch < numChannels; ++ch)
            dry.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            gains[(size_t) i] = drive.getNextValue();
            mixes[(size_t) i] = mix.getNextValue();
        }

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);

        if (oversample > 0)
        {
            auto& os = *oversamplers[(size_t) oversample - 1];
            auto up = os.processSamplesUp (block);
            auto factor = (int) os.getOversamplingFactor();
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (int j = 0; j < (int) up.getNumSamples(); ++j)
                    d[j] = shapeSample (d[j] * gains[(size_t) (j / factor)]);
            }
            os.processSamplesDown (block);
        }
        else
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    d[i] = shapeSample (d[i] * gains[(size_t) i]);
            }
        }

        auto* l = buffer.getWritePointer (0);
        auto* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
        auto* dl = dry.getReadPointer (0);
        auto* dr = numChannels > 1 ? dry.getReadPointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            // Partial make-up so cranking drive doesn't just get louder.
            auto makeup = 1.0f / std::sqrt (gains[(size_t) i]);
            auto wetL = l[i] * makeup;
            auto wetR = r != nullptr ? r[i] * makeup : wetL;

            if (bits < 15.95f)
            {
                auto levels = std::exp2 (bits - 1.0f);
                wetL = std::round (wetL * levels) / levels;
                wetR = std::round (wetR * levels) / levels;
            }

            if (downsample > 1.01f)
            {
                holdCounter += 1.0f;
                if (holdCounter >= downsample)
                {
                    holdCounter -= downsample;
                    holdL = wetL;
                    holdR = wetR;
                }
                wetL = holdL;
                wetR = holdR;
            }

            wetL = dcBlock (wetL, dcInL, dcOutL);
            wetR = dcBlock (wetR, dcInR, dcOutR);

            auto m = mixes[(size_t) i];
            l[i] = dl[i] + (wetL - dl[i]) * m;
            if (r != nullptr)
                r[i] = dr[i] + (wetR - dr[i]) * m;
        }
    }

private:
    float shapeSample (float x) const
    {
        switch (shape)
        {
            case Shape::hard: return juce::jlimit (-1.0f, 1.0f, x);
            case Shape::fold:
            {
                // Triangle fold: bounces back off ±1 instead of clipping.
                auto t = 0.25f * x + 0.25f;
                return 4.0f * std::abs (t - std::round (t)) - 1.0f;
            }
            case Shape::tube:
                return x >= 0.0f ? std::tanh (x) : std::tanh (0.6f * x) / 0.6f * 0.8f;
            case Shape::soft:
            default:
                return std::tanh (x);
        }
    }

    float dcBlock (float x, float& prevIn, float& prevOut) const
    {
        auto y = x - prevIn + dcCoeff * prevOut;
        prevIn = x;
        prevOut = y;
        return y;
    }

    juce::SmoothedValue<float> drive { 1.0f }, mix { 1.0f };
    Shape shape = Shape::soft;
    float bits = 16.0f, downsample = 1.0f;
    int oversample = 0;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;
    juce::AudioBuffer<float> dry;
    std::vector<float> gains, mixes;

    float holdL = 0.0f, holdR = 0.0f, holdCounter = 0.0f;
    float dcCoeff = 0.999f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
};

} // namespace rf
