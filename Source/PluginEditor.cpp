#include "PluginEditor.h"

namespace ui
{

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& id, const juce::String& name, juce::Colour accent)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    // Set before the text box exists, since it copies these colours when created.
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
    slider.getProperties().set ("accent", (juce::int64) accent.getARGB());
    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    // The attachment set the text before the formatter was in place, so refresh it.
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
Choice::Choice (APVTS& state, const juce::String& id, const juce::String& name)
{
    // The attachment only selects items, so they have to exist first.
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
        box.addItemList (p->choices, 1);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);

    addAndMakeVisible (box);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 110), 24));
}

//==============================================================================
Toggle::Toggle (APVTS& state, const juce::String& id, const juce::String& name)
    : attachment (state, id, button)
{
    button.setButtonText (name);
    addAndMakeVisible (button);
}

void Toggle::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    button.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 90), 24));
}

//==============================================================================
Section::Section (const juce::String& t, juce::Colour a, int c, int r)
    : cols (c), rows (r), title (t), accent (a) {}

void paintPanel (juce::Graphics& g, juce::Component& c, const juce::String& title, juce::Colour accent)
{
    auto r = c.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (12.0f, 11.0f, 3.0f, 14.0f, 1.5f);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), 22, 8, c.getWidth() - 30, 20, juce::Justification::centredLeft);
}

void Section::paint (juce::Graphics& g)
{
    paintPanel (g, *this, title, accent);
}

void Section::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (26);

    auto itemRows = footer != nullptr ? rows - 1 : rows;
    auto cellH = area.getHeight() / rows;

    if (footer != nullptr)
        footer->setBounds (area.removeFromBottom (cellH).reduced (6, 10));

    auto cellW = area.getWidth() / cols;
    for (int i = 0; i < items.size(); ++i)
    {
        auto col = i % cols, row = i / cols;
        if (row >= itemRows)
            break;
        items[i]->setBounds (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH);
    }
}

//==============================================================================
KeysPanel::KeysPanel (APVTS& s)
    : state (s),
      sampleParam (*s.getParameter (ParamIDs::sampleIndex)),
      source (s, ParamIDs::keysSource, "Source"),
      transpose (s, ParamIDs::keysTranspose, "Transpose", Palette::accent),
      release (s, ParamIDs::keysRelease, "Release", Palette::accent),
      level (s, ParamIDs::keysLevel, "Level", Palette::accent)
{
    for (auto* c : std::initializer_list<juce::Component*> { &source, &prev, &next, &nameButton, &transpose, &release, &level })
        addAndMakeVisible (c);

    for (auto* b : { &prev, &next, &nameButton })
    {
        b->setColour (juce::TextButton::buttonColourId, Palette::track);
        b->setColour (juce::TextButton::textColourOffId, Palette::text);
        b->setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }

    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    nameButton.onClick = [this] { showMenu(); };
    refresh();
}

void KeysPanel::paint (juce::Graphics& g)
{
    paintPanel (g, *this, "Keys", Palette::accent2);

    g.setColour (Palette::dimText);
    g.setFont (juce::FontOptions (12.0f));
    auto label = "Sample   " + juce::String (shownIndex + 1) + " / " + juce::String (SampleLibrary::get().size());
    g.drawText (label, nameButton.getX(), nameButton.getY() - 20, nameButton.getWidth(), 16, juce::Justification::centred);
}

void KeysPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (22);

    auto knobs = area.removeFromRight (3 * 74);
    for (auto* k : { &transpose, &release, &level })
        k->setBounds (knobs.removeFromLeft (74));

    source.setBounds (area.removeFromLeft (110));
    area.removeFromLeft (4);

    auto row = area.withTrimmedTop (16).withSizeKeepingCentre (area.getWidth(), 28);
    prev.setBounds (row.removeFromLeft (32));
    next.setBounds (row.removeFromRight (32));
    nameButton.setBounds (row.reduced (6, 0));
}

void KeysPanel::refresh()
{
    auto index = juce::roundToInt (sampleParam.convertFrom0to1 (sampleParam.getValue()));
    auto useSample = state.getRawParameterValue (ParamIDs::keysSource)->load() > 0.5f;
    if (index == shownIndex && useSample == shownUseSample)
        return;

    shownIndex = index;
    shownUseSample = useSample;
    const auto& lib = SampleLibrary::get();
    const auto& e = lib[index];
    nameButton.setButtonText (e.category + "  /  " + e.name);
    repaint();

    for (auto* c : std::initializer_list<juce::Component*> { &prev, &next, &nameButton })
        c->setEnabled (useSample);
}

void KeysPanel::select (int index)
{
    auto count = SampleLibrary::get().size();
    index = ((index % count) + count) % count;
    sampleParam.beginChangeGesture();
    sampleParam.setValueNotifyingHost (sampleParam.convertTo0to1 ((float) index));
    sampleParam.endChangeGesture();
    refresh();
}

void KeysPanel::step (int delta)
{
    select (shownIndex + delta);
}

void KeysPanel::showMenu()
{
    const auto& lib = SampleLibrary::get();
    juce::PopupMenu menu;

    for (auto& category : lib.categories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < lib.size(); ++i)
            if (lib[i].category == category)
                sub.addItem (i + 1, lib[i].name, true, i == shownIndex);
        menu.addSubMenu (category, sub, true, nullptr, lib[shownIndex].category == category);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton),
                        [safe = juce::Component::SafePointer<KeysPanel> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                                safe->select (result - 1);
                        });
}

//==============================================================================
RandomiserPanel::RandomiserPanel (APVTS& s)
    : seedParam (*s.getParameter (ParamIDs::randSeed)),
      on (s, ParamIDs::randOn, "On"),
      key (s, ParamIDs::randRoot, "Key"),
      scale (s, ParamIDs::randScale, "Scale"),
      rate (s, ParamIDs::randRate, "Rate"),
      octaves (s, ParamIDs::randOctaves, "Octaves", Palette::accent),
      chance (s, ParamIDs::randChance, "Chance", Palette::accent),
      length (s, ParamIDs::randLength, "Length", Palette::accent),
      snap (s, ParamIDs::snapKeys, "Snap Keys")
{
    for (auto* c : std::initializer_list<juce::Component*> { &on, &key, &scale, &rate, &octaves, &chance, &length, &newPattern, &snap })
        addAndMakeVisible (c);

    newPattern.setColour (juce::TextButton::buttonColourId, Palette::track);
    newPattern.setColour (juce::TextButton::textColourOffId, Palette::text);
    newPattern.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    newPattern.onClick = [this]
    {
        auto seed = juce::Random::getSystemRandom().nextInt ({ 1, 10000 });
        seedParam.beginChangeGesture();
        seedParam.setValueNotifyingHost (seedParam.convertTo0to1 ((float) seed));
        seedParam.endChangeGesture();
    };
}

void RandomiserPanel::paint (juce::Graphics& g)
{
    paintPanel (g, *this, "Randomiser", Palette::accent2);

    g.setColour (Palette::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Pattern", newPattern.getX() - 10, newPattern.getY() - 20, newPattern.getWidth() + 20, 16, juce::Justification::centred);
}

void RandomiserPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (22);

    // Fixed widths for the controls, then share out whatever is left.
    std::pair<juce::Component*, int> cols[] {
        { &on, 58 }, { &key, 64 }, { &scale, 128 }, { &rate, 70 },
        { &octaves, 66 }, { &chance, 66 }, { &length, 66 }, { &newPattern, 64 }, { &snap, 86 } };

    auto total = 0;
    for (auto& c : cols)
        total += c.second;
    auto extra = juce::jmax (0, area.getWidth() - total) / (int) std::size (cols);

    for (auto& [comp, width] : cols)
    {
        auto cell = area.removeFromLeft (width + extra);
        if (comp == &newPattern)
            comp->setBounds (cell.withTrimmedTop (16).withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 64), 24));
        else
            comp->setBounds (cell);
    }
}

//==============================================================================
void GateDisplay::setState (int patternIndex, int step, bool on)
{
    if (patternIndex != pattern || step != current || on != enabled)
    {
        pattern = patternIndex;
        current = step;
        enabled = on;
        repaint();
    }
}

void GateDisplay::paint (juce::Graphics& g)
{
    const auto& steps = rf::TranceGate::pattern (pattern);
    auto gap = 4.0f;
    auto w = ((float) getWidth() - gap * 15.0f) / 16.0f;

    for (int i = 0; i < 16; ++i)
    {
        juce::Rectangle<float> cell ((float) i * (w + gap), 0.0f, w, (float) getHeight());
        auto active = steps[(size_t) i] != 0;
        auto colour = active ? Palette::accent2 : Palette::track;
        if (! enabled)
            colour = colour.withMultipliedSaturation (0.2f).withMultipliedAlpha (0.5f);

        g.setColour (colour.withAlpha (active ? 0.85f : 1.0f));
        g.fillRoundedRectangle (cell, 3.0f);

        if (enabled && i == current)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (cell.reduced (1.0f), 3.0f, 2.0f);
        }
    }
}
} // namespace ui

//==============================================================================
ZeygorgEditor::ZeygorgEditor (ZeygorgProcessor& p)
    : AudioProcessorEditor (&p), raveProcessor (p),
      rave ("Boost", Palette::accent2, 3, 2),
      filter ("Filter", Palette::accent2, 3, 2),
      dirt ("Dirt", Palette::accent2, 3, 2),
      lfo ("LFO", Palette::accent2, 4, 2),
      gate ("Trance Gate", Palette::accent2, 5, 2),
      delay ("Delay", Palette::accent2, 3, 2),
      reverb ("Reverb", Palette::accent2, 2, 2),
      master ("Master", Palette::accent2, 1, 2),
      keysPanel (p.apvts),
      randomiserPanel (p.apvts),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    using namespace ParamIDs;
    using ui::Knob;
    using ui::Choice;
    using ui::Toggle;

    setLookAndFeel (&lookAndFeel);
    auto& s = p.apvts;

    rave.add<Knob> (s, power, "Power", Palette::accent);
    rave.add<Knob> (s, width, "Width", Palette::accent);
    rave.add<Knob> (s, detune, "Detune", Palette::accent);
    rave.add<Knob> (s, subOct, "Sub Oct", Palette::accent);
    rave.add<Knob> (s, fifth, "Fifth", Palette::accent);
    rave.add<Knob> (s, octUp, "Oct Up", Palette::accent);

    filter.add<Choice> (s, fltType, "Type");
    filter.add<Knob> (s, cutoff, "Cutoff", Palette::accent);
    filter.add<Knob> (s, resonance, "Resonance", Palette::accent);
    filter.add<Knob> (s, envAmount, "Env Amt", Palette::accent);
    filter.add<Knob> (s, envAttack, "Attack", Palette::accent);
    filter.add<Knob> (s, envRelease, "Release", Palette::accent);

    dirt.add<Choice> (s, shape, "Shape");
    dirt.add<Knob> (s, drive, "Drive", Palette::accent);
    dirt.add<Knob> (s, dirtMix, "Mix", Palette::accent);
    dirt.add<Knob> (s, bits, "Bits", Palette::accent);
    dirt.add<Knob> (s, downsample, "Downsample", Palette::accent);

    lfo.add<Choice> (s, lfoShape, "Shape");
    lfo.add<Toggle> (s, lfoSync, "Sync");
    lfoDivChoice = &lfo.add<Choice> (s, lfoDiv, "Division");
    lfoRateKnob = &lfo.add<Knob> (s, lfoRate, "Rate", Palette::accent);
    lfo.add<Knob> (s, lfoCutoff, "> Cutoff", Palette::accent);
    lfo.add<Knob> (s, lfoDetune, "> Detune", Palette::accent);
    lfo.add<Knob> (s, lfoAmp, "> Volume", Palette::accent);

    gate.add<Toggle> (s, gateOn, "Gate On");
    gate.add<Choice> (s, gatePattern, "Pattern");
    gate.add<Choice> (s, gateRate, "Rate");
    gate.add<Knob> (s, gateDepth, "Depth", Palette::accent);
    gate.add<Knob> (s, gateSmooth, "Smooth", Palette::accent);
    gate.setFooter (&gateDisplay);

    delay.add<Choice> (s, dlyDiv, "Time");
    delay.add<Toggle> (s, dlyPingPong, "Ping Pong");
    delay.add<Knob> (s, dlyFeedback, "Feedback", Palette::accent);
    delay.add<Knob> (s, dlyTone, "Tone", Palette::accent);
    delay.add<Knob> (s, dlyMix, "Mix", Palette::accent);

    reverb.add<Knob> (s, revSize, "Size", Palette::accent);
    reverb.add<Knob> (s, revDamp, "Damping", Palette::accent);
    reverb.add<Knob> (s, revWidth, "Width", Palette::accent);
    reverb.add<Knob> (s, revMix, "Mix", Palette::accent);

    master.add<Knob> (s, ParamIDs::mix, "Dry/Wet", Palette::accent);
    master.add<Knob> (s, output, "Output", Palette::accent);

    for (auto* c : { &rave, &filter, &dirt, &lfo, &gate, &delay, &reverb, &master })
        addAndMakeVisible (c);

    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd0d0d0));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1a1a1a));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3a3a));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour (0xff6e6e6e));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour (0x22000000));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonBackgroundColourId, Palette::panel);
    keyboard.setColour (juce::MidiKeyboardComponent::upDownButtonArrowColourId, Palette::dimText);
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colour (0xff555555));
    keyboard.setAvailableRange (24, 96);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setWantsKeyboardFocus (false);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (keysPanel);
    addAndMakeVisible (randomiserPanel);

    setSize (1280, 830);
    timerCallback();
    startTimerHz (30);
}

ZeygorgEditor::~ZeygorgEditor()
{
    setLookAndFeel (nullptr);
}

void ZeygorgEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().removeFromTop (48).reduced (18, 0);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ZEYGORG", header, juce::Justification::centredLeft);

    g.setColour (Palette::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("boost  /  keys  /  randomiser  /  filter  /  dirt  /  lfo  /  gate  /  delay  /  reverb",
                header, juce::Justification::centredRight);
}

void ZeygorgEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (40);

    // C1..C7 is 43 white keys; size them so the whole range fills the width.
    keyboard.setBounds (area.removeFromBottom (88));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f);
    area.removeFromBottom (10);

    // Keys and the randomiser sit on top, where the sample is picked.
    auto keysRow = area.removeFromTop (136);
    keysPanel.setBounds (keysRow.removeFromLeft ((keysRow.getWidth() - 10) * 46 / 100));
    keysRow.removeFromLeft (10);
    randomiserPanel.setBounds (keysRow);
    area.removeFromTop (10);

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<ui::Section*> sections)
    {
        int totalCols = 0;
        for (auto* s : sections)
            totalCols += s->cols;

        auto gap = 10;
        auto unit = (float) (row.getWidth() - gap * ((int) sections.size() - 1)) / (float) totalCols;
        for (auto* s : sections)
        {
            s->setBounds (row.removeFromLeft (juce::roundToInt (unit * (float) s->cols)));
            row.removeFromLeft (gap);
        }
    };

    auto top = area.removeFromTop ((area.getHeight() - 10) / 2);
    area.removeFromTop (10);

    layoutRow (top, { &rave, &filter, &dirt, &lfo });
    layoutRow (area, { &gate, &delay, &reverb, &master });
}

void ZeygorgEditor::timerCallback()
{
    keysPanel.refresh();

    auto& s = raveProcessor.apvts;
    auto synced = s.getRawParameterValue (ParamIDs::lfoSync)->load() > 0.5f;
    lfoRateKnob->setEnabled (! synced);
    lfoDivChoice->setEnabled (synced);

    gateDisplay.setState (juce::roundToInt (s.getRawParameterValue (ParamIDs::gatePattern)->load()),
                          raveProcessor.engine.currentGateStep.load(),
                          s.getRawParameterValue (ParamIDs::gateOn)->load() > 0.5f);
}
