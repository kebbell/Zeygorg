#include "PresetBar.h"

namespace ui
{

PresetBar::PresetBar (PresetManager& p) : presets (p)
{
    for (auto* b : { &prev, &next, &name, &save, &slotA, &slotB })
        addAndMakeVisible (b);

    prev.onClick = [this] { presets.step (-1); refresh(); };
    next.onClick = [this] { presets.step (1); refresh(); };
    name.onClick = [this] { showMenu(); };
    save.onClick = [this] { askToSave(); };
    slotA.onClick = [this] { presets.selectSlot (0); refresh(); };
    slotB.onClick = [this] { presets.selectSlot (1); refresh(); };
    slotA.setClickingTogglesState (false);
    slotB.setClickingTogglesState (false);

    refresh();
    startTimerHz (8);
}

void PresetBar::timerCallback() { refresh(); }

void PresetBar::refresh()
{
    auto n = presets.currentName();
    if (n != shownName)
    {
        shownName = n;
        name.setButtonText (n + "  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")));
    }
    if (presets.activeSlot() != shownSlot)
    {
        shownSlot = presets.activeSlot();
        slotA.setToggleState (shownSlot == 0, juce::dontSendNotification);
        slotB.setToggleState (shownSlot == 1, juce::dontSendNotification);
    }
}

void PresetBar::paint (juce::Graphics&) {}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    slotB.setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    slotA.setBounds (r.removeFromRight (28));
    r.removeFromRight (12);
    save.setBounds (r.removeFromRight (54));
    r.removeFromRight (8);
    prev.setBounds (r.removeFromLeft (28));
    next.setBounds (r.removeFromRight (28));
    name.setBounds (r.reduced (4, 0));
}

void PresetBar::showMenu()
{
    presets.rescan();
    const auto& entries = presets.getEntries();
    auto current = presets.currentIndex();

    juce::PopupMenu menu;
    juce::StringArray categories;
    for (auto& e : entries)
        categories.addIfNotAlreadyThere (e.category);

    for (auto& cat : categories)
    {
        menu.addSectionHeader (cat);
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].category == cat)
                menu.addItem (i + 1, entries[(size_t) i].name, true, i == current);
    }

    menu.addSeparator();
    menu.addItem (-1, "Save as...");
    menu.addItem (-2, "Copy " + juce::String (presets.activeSlot() == 0 ? "A to B" : "B to A"));
    menu.addItem (-3, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&name).withMinimumWidth (name.getWidth()),
                        [safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == -1) safe->askToSave();
                            else if (result == -2) safe->presets.copyToOther();
                            else if (result == -3)
                            {
                                auto dir = safe->presets.userDirectory();
                                dir.createDirectory();
                                dir.revealToUser();
                            }
                            else safe->presets.load (result - 1);
                            safe->refresh();
                        });
}

void PresetBar::askToSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name this sound:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", presets.currentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create (
                                  [safe = juce::Component::SafePointer<PresetBar> (this), w] (int result)
                                  {
                                      if (safe != nullptr && result == 1)
                                      {
                                          safe->presets.save (w->getTextEditorContents ("name"));
                                          safe->refresh();
                                      }
                                  }),
                        true);
}

} // namespace ui
