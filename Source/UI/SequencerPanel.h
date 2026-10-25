#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace ui
{

// The pattern as a piano-roll grid: one row per scale note, 16 steps.
// Click to place or remove a note, drag to paint.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    explicit StepGrid (ZeygorgProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<float> gridArea() const;
    int rows() const;
    std::pair<int, int> cellAt (juce::Point<float>) const;   // {step, degree}
    void apply (int step, int degree);

    ZeygorgProcessor& processor;
    int lastVersion = -1, lastStep = -2, lastRows = 0, lastSteps = 0, lastRoot = -1, lastScale = -1;
    bool erasing = false;
    int lastPaintedStep = -1;
};

// Per-step velocity bars under the grid.
class VelocityLane : public juce::Component, private juce::Timer
{
public:
    explicit VelocityLane (ZeygorgProcessor& p);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent&) override;

    int leftGutter = 38;

private:
    void timerCallback() override;
    ZeygorgProcessor& processor;
    int lastVersion = -1;
};

// Drag this into the DAW to drop the pattern as a MIDI clip.
class DragMidiButton : public Button
{
public:
    explicit DragMidiButton (ZeygorgProcessor& p);
    void mouseDrag (const juce::MouseEvent&) override;

private:
    ZeygorgProcessor& processor;
    bool dragStarted = false;
};

class SequencerPanel : public juce::Component
{
public:
    explicit SequencerPanel (ZeygorgProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void shift (int delta);
    void vary();

    ZeygorgProcessor& processor;
    juce::ToggleButton power;
    APVTS::ButtonAttachment powerAttachment;
    Choice key, scale, rate;
    Knob octaves, steps, length, swing, density;
    Button newPattern { "New", "Roll a new random pattern (uses Density)" };
    Button varyPattern { "Vary", "Change a few steps" };
    Button clear { "Clear", "Remove every note" };
    Button left { juce::CharPointer_UTF8 ("\xe2\x80\xb9"), "Shift the pattern one step earlier" };
    Button right { juce::CharPointer_UTF8 ("\xe2\x80\xba"), "Shift the pattern one step later" };
    DragMidiButton dragMidi;
    StepGrid grid;
    VelocityLane velocity;
};

// Shows the chord a C (or the key's root) would play.
class ChordDisplay : public juce::Component, private juce::Timer
{
public:
    explicit ChordDisplay (APVTS& state);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    APVTS& state;
    int lastChord = -1, lastRoot = -1, lastScale = -1, lastSnap = -1;
};

} // namespace ui
