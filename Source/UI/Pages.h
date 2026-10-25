#pragma once

#include "Widgets.h"
#include "Displays.h"
#include "LayerPanel.h"
#include "SequencerPanel.h"

namespace ui
{

// Shows the 16-step trance gate pattern with the playing step lit.
class GateDisplay : public juce::Component, private juce::Timer
{
public:
    explicit GateDisplay (ZeygorgProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    ZeygorgProcessor& processor;
    int pattern = -1, current = -1;
    bool enabled = false;
};

// How notes travel through the MIDI section, lighting the stages in use.
class NoteFlow : public juce::Component, private juce::Timer
{
public:
    explicit NoteFlow (APVTS& state);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    APVTS& state;
    int lastMask = -1;
};

//==============================================================================
class SynthPage : public juce::Component
{
public:
    explicit SynthPage (ZeygorgProcessor& p);
    void resized() override;

private:
    juce::OwnedArray<LayerPanel> layers;
    Section amp, filter, filterEnv, voice;
    EnvelopeEditor ampEditor, filterEditor;
};

class SequencePage : public juce::Component
{
public:
    explicit SequencePage (ZeygorgProcessor& p);
    void resized() override;

private:
    SequencerPanel sequencer;
    Section arp, chords, flowSection;
    ChordDisplay chordDisplay;
    NoteFlow flow;
};

class FxPage : public juce::Component, private juce::Timer
{
public:
    explicit FxPage (ZeygorgProcessor& p);
    void resized() override;

private:
    void timerCallback() override;

    ZeygorgProcessor& processor;
    Section boost, filter, dirt, lfo, gate, delay, reverb, master, pump, comp, analyserSection;
    GateDisplay gateDisplay;
    PumpDisplay pumpDisplay;
    ReductionMeter reduction;
    Visualiser analyser;
    Knob* lfoRateKnob = nullptr;
    Choice* lfoDivChoice = nullptr;
};

} // namespace ui
