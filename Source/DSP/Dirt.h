#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace rf
{

// Drive into a waveshaper, then bit reduction and sample-rate reduction.
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
    };

    void prepare (double sampleRate)
    {
        drive.reset (sampleRate, 0.03);
        mix.reset (sampleRate, 0.03);
        // ~10 Hz DC blocker for the asymmetric tube curve.
        dcCoeff = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 10.0 / sampleRate);
        reset();
    }

    void reset()
    {
        holdL = holdR = 0.0f;
        holdCounter = 0.0f;
        dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    }

    void setTargets (const Settings& s)
    {
        drive.setTargetValue (juce::Decibels::decibelsToGain (s.driveDb));
        mix.setTargetValue (s.mix);
        shape = s.shape;
        bits = s.bits;
        downsample = s.downsample;
    }

    void process (float& l, float& r)
    {
        auto g = drive.getNextValue();
        auto m = mix.getNextValue();

        // Partial make-up so cranking drive doesn't just get louder.
        auto makeup = 1.0f / std::sqrt (g);

        auto wetL = shapeSample (l * g) * makeup;
        auto wetR = shapeSample (r * g) * makeup;

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

        l += (wetL - l) * m;
        r += (wetR - r) * m;
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

    float holdL = 0.0f, holdR = 0.0f, holdCounter = 0.0f;
    float dcCoeff = 0.999f, dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
};

} // namespace rf
