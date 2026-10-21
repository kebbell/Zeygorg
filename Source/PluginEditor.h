#pragma once

#include "PluginProcessor.h"
#include "RaveLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// A named rotary knob bound to one parameter.
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& id, const juce::String& name, juce::Colour accent);
    void resized() override;

    juce::Slider slider;

private:
    juce::Label label;
    APVTS::SliderAttachment attachment;
};

// A named dropdown bound to a choice parameter.
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& id, const juce::String& name);
    void resized() override;

    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// An on/off button bound to a bool parameter.
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& id, const juce::String& name);
    void resized() override;

private:
    juce::ToggleButton button;
    APVTS::ButtonAttachment attachment;
};

// A titled panel that lays its controls out on a grid. An optional `footer`
// takes the last row on its own (used for the gate's step display).
class Section : public juce::Component
{
public:
    Section (const juce::String& title, juce::Colour accent, int cols, int rows);

    template <typename T, typename... Args>
    T& add (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        items.add (c);
        addAndMakeVisible (c);
        return *c;
    }

    void setFooter (juce::Component* c) { footer = c; addAndMakeVisible (c); }

    void paint (juce::Graphics&) override;
    void resized() override;

    const int cols, rows;

private:
    juce::String title;
    juce::Colour accent;
    juce::OwnedArray<juce::Component> items;
    juce::Component* footer = nullptr;
};

// Shows the selected 16-step gate pattern with the playing step lit.
class GateDisplay : public juce::Component
{
public:
    void setState (int patternIndex, int step, bool on);
    void paint (juce::Graphics&) override;

private:
    int pattern = 0, current = 0;
    bool enabled = false;
};
} // namespace ui

class RaveForgeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RaveForgeEditor (RaveForgeProcessor&);
    ~RaveForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    RaveForgeProcessor& raveProcessor;
    RaveLookAndFeel lookAndFeel;

    ui::Section rave, filter, dirt, lfo, gate, delay, reverb, master;
    ui::GateDisplay gateDisplay;
    juce::MidiKeyboardComponent keyboard;

    ui::Knob* lfoRateKnob = nullptr;
    ui::Choice* lfoDivChoice = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RaveForgeEditor)
};
