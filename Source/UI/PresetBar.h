#pragma once

#include "Widgets.h"
#include "../Presets.h"

namespace ui
{

// Preset browser for the header: previous/next, a name that opens the full
// list, save, and A/B compare.
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& presets);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void askToSave();
    void refresh();

    PresetManager& presets;
    Button prev { juce::CharPointer_UTF8 ("\xe2\x80\xb9"), "Previous preset" };
    Button next { juce::CharPointer_UTF8 ("\xe2\x80\xba"), "Next preset" };
    Button name { "", "Browse presets" };
    Button save { "Save", "Save the current sound as a user preset" };
    Button slotA { "A", "Compare: sound A" };
    Button slotB { "B", "Compare: sound B (starts as a copy of A)" };
    juce::String shownName;
    int shownSlot = -1;
};

} // namespace ui
