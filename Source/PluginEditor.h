#pragma once

#include "PluginProcessor.h"
#include "UI/Theme.h"
#include "UI/Pages.h"
#include "UI/PresetBar.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace ui
{
// Page tabs along the top: an underline marks the current one.
class TabBar : public juce::Component
{
public:
    explicit TabBar (juce::StringArray names);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void setCurrent (int index);
    int getCurrent() const { return current; }
    std::function<void (int)> onChange;

private:
    juce::Rectangle<int> tabBounds (int i) const;
    juce::StringArray names;
    int current = 0, hover = -1;
};

// The on-screen keyboard, drawn to match: graphite keys, white when held.
class Keyboard : public juce::MidiKeyboardComponent
{
public:
    explicit Keyboard (juce::MidiKeyboardState& state);
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour noteFillColour) override;
};
} // namespace ui

class ZeygorgEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ZeygorgEditor (ZeygorgProcessor&);
    ~ZeygorgEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1280, baseHeight = 880;

    // Switches pages; also used by the offline test for snapshots.
    void showPage (int index);

private:
    // Everything is laid out at the base size inside this, then scaled.
    class Content : public juce::Component
    {
    public:
        explicit Content (ZeygorgEditor& e) : editor (e) {}
        void paint (juce::Graphics& g) override { editor.paintContent (g); }
        void resized() override { editor.layoutContent(); }

    private:
        ZeygorgEditor& editor;
    };

    void paintContent (juce::Graphics&);
    void layoutContent();
    void timerCallback() override;

    ZeygorgProcessor& zeyProcessor;
    ZeyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    Content content { *this };
    ui::PresetBar presetBar;
    ui::Visualiser visualiser;
    ui::LevelMeter meter;
    ui::TabBar tabs { { "Synth", "Sequence", "FX" } };
    ui::SynthPage synthPage;
    ui::SequencePage sequencePage;
    ui::FxPage fxPage;
    ui::Keyboard keyboard;
    int shownVoices = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZeygorgEditor)
};
