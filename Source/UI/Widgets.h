#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

// Parameter-bound controls and panels shared by every page. Nothing here
// knows about Zeygorg's parameters.
namespace ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Draws a panel: soft gradient, hairline edge and an optional title.
void paintPanel (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title = {});

// Draws an inset "screen" for displays.
void paintWell (juce::Graphics& g, juce::Rectangle<float> bounds);

// A rotary knob with its name above and value below. Double-click resets
// it, click the value to type one.
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& id, const juce::String& name);
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

// An LED pill bound to a bool parameter. With a `caption` it lines up with
// knobs (caption above); without, it fills its bounds.
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& id, const juce::String& text, const juce::String& caption = {});
    void resized() override;

    juce::ToggleButton button;

private:
    juce::Label label;
    APVTS::ButtonAttachment attachment;
};

// A plain push button styled like the rest.
class Button : public juce::TextButton
{
public:
    explicit Button (const juce::String& text, const juce::String& tooltip = {})
        : juce::TextButton (text)
    {
        setTooltip (tooltip);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
};

// A titled panel that lays controls out on a grid. It can carry an on/off
// switch in its title bar (the body dims while it's off) and a footer that
// takes the bottom rows (for a display).
class Section : public juce::Component
{
public:
    Section (const juce::String& title, int cols, int rows);

    template <typename T, typename... Args>
    T& add (Args&&... args)
    {
        auto* c = new T (std::forward<Args> (args)...);
        items.add (c);
        addAndMakeVisible (c);
        return *c;
    }

    // Leaves a cell empty.
    void skip() { items.add (new juce::Component()); }

    void setFooter (juce::Component* c, int footerRows = 1);
    void setHeaderToggle (APVTS& state, const juce::String& id);

    void paint (juce::Graphics&) override;
    void resized() override;

    const int cols, rows;

private:
    void updateDim();

    juce::String title;
    juce::OwnedArray<juce::Component> items;
    juce::Component* footer = nullptr;
    int footerRows = 1;
    std::unique_ptr<juce::ToggleButton> headerToggle;
    std::unique_ptr<APVTS::ButtonAttachment> headerAttachment;
};

// Sets a parameter from UI code as a single undoable gesture.
inline void setParameter (APVTS& state, const juce::String& id, float plainValue)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

inline float getParameter (APVTS& state, const juce::String& id)
{
    return state.getRawParameterValue (id)->load();
}
} // namespace ui
