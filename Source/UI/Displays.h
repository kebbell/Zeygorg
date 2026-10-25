#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Widgets.h"

// Animated displays: scope/spectrum, meters, envelope editor and curves.
namespace ui
{

// Oscilloscope and spectrum analyser over a ring buffer the audio thread
// fills. Click to switch between the two (or show both side by side).
class Visualiser : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Mode { scope, spectrum, both };

    Visualiser (const float* ring, int ringSize, const std::atomic<int>& writeIndex, std::function<double()> sampleRate);

    void setMode (Mode m) { mode = m; repaint(); }
    void setClickToSwitch (bool b) { clickToSwitch = b; }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void paintScope (juce::Graphics&, juce::Rectangle<float>);
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float>);

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;

    const float* ring;
    int ringSize;
    const std::atomic<int>& writeIndex;
    std::function<double()> getSampleRate;

    Mode mode = Mode::scope;
    bool clickToSwitch = true;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrumDb {};
    std::array<float, fftSize> snapshot {};
};

// Stereo peak meter with a short hold.
class LevelMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    LevelMeter (const std::atomic<float>& left, const std::atomic<float>& right);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    const std::atomic<float>& left;
    const std::atomic<float>& right;
    float levels[2] {}, holds[2] {};
    int holdTimers[2] {};
};

// A horizontal gain-reduction bar (0 to -24 dB).
class ReductionMeter : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit ReductionMeter (std::function<float()> getReductionDb);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::function<float()> getDb;
    float shown = 0.0f;
};

// ADSR drawn as a curve with draggable points: attack, decay/sustain, release.
class EnvelopeEditor : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    EnvelopeEditor (APVTS& state, juce::String attackId, juce::String decayId, juce::String sustainId, juce::String releaseId);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    struct Points { juce::Point<float> start, peak, sustain, sustainEnd, end; };
    Points points() const;
    juce::Rectangle<float> plot() const;
    int handleAt (juce::Point<float>) const;
    float norm (const juce::String& id) const;
    void setNorm (const juce::String& id, float v);
    void timerCallback() override;

    APVTS& state;
    juce::String ids[4];
    int hover = -1, dragging = -1;
    float lastValues[4] {};
};

// The pump's ducking curve over one period.
class PumpDisplay : public juce::Component, private juce::Timer
{
public:
    PumpDisplay (APVTS& state, juce::String depthId, juce::String releaseId, std::function<float()> currentGain);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    APVTS& state;
    juce::String depthId, releaseId;
    std::function<float()> currentGain;
    float shownGain = 1.0f;
};

} // namespace ui
