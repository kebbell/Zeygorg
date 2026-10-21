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

void Section::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (12.0f, 11.0f, 3.0f, 14.0f, 1.5f);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), 22, 8, getWidth() - 30, 20, juce::Justification::centredLeft);
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
RaveForgeEditor::RaveForgeEditor (RaveForgeProcessor& p)
    : AudioProcessorEditor (&p), raveProcessor (p),
      rave ("Rave", Palette::accent, 3, 2),
      filter ("Filter", Palette::accent2, 3, 2),
      dirt ("Dirt", juce::Colour (0xffffa53d), 3, 2),
      lfo ("LFO", juce::Colour (0xff9d6bff), 4, 2),
      gate ("Trance Gate", Palette::accent2, 5, 2),
      delay ("Delay", juce::Colour (0xff4dffa0), 3, 2),
      reverb ("Reverb", juce::Colour (0xff6b9dff), 2, 2),
      master ("Master", Palette::accent, 1, 2)
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
    filter.add<Knob> (s, cutoff, "Cutoff", Palette::accent2);
    filter.add<Knob> (s, resonance, "Resonance", Palette::accent2);
    filter.add<Knob> (s, envAmount, "Env Amt", Palette::accent2);
    filter.add<Knob> (s, envAttack, "Attack", Palette::accent2);
    filter.add<Knob> (s, envRelease, "Release", Palette::accent2);

    auto orange = juce::Colour (0xffffa53d);
    dirt.add<Choice> (s, shape, "Shape");
    dirt.add<Knob> (s, drive, "Drive", orange);
    dirt.add<Knob> (s, dirtMix, "Mix", orange);
    dirt.add<Knob> (s, bits, "Bits", orange);
    dirt.add<Knob> (s, downsample, "Downsample", orange);

    auto purple = juce::Colour (0xff9d6bff);
    lfo.add<Choice> (s, lfoShape, "Shape");
    lfo.add<Toggle> (s, lfoSync, "Sync");
    lfoDivChoice = &lfo.add<Choice> (s, lfoDiv, "Division");
    lfoRateKnob = &lfo.add<Knob> (s, lfoRate, "Rate", purple);
    lfo.add<Knob> (s, lfoCutoff, "> Cutoff", purple);
    lfo.add<Knob> (s, lfoDetune, "> Detune", purple);
    lfo.add<Knob> (s, lfoAmp, "> Volume", purple);

    gate.add<Toggle> (s, gateOn, "Gate On");
    gate.add<Choice> (s, gatePattern, "Pattern");
    gate.add<Choice> (s, gateRate, "Rate");
    gate.add<Knob> (s, gateDepth, "Depth", Palette::accent2);
    gate.add<Knob> (s, gateSmooth, "Smooth", Palette::accent2);
    gate.setFooter (&gateDisplay);

    auto green = juce::Colour (0xff4dffa0);
    delay.add<Choice> (s, dlyDiv, "Time");
    delay.add<Toggle> (s, dlyPingPong, "Ping Pong");
    delay.add<Knob> (s, dlyFeedback, "Feedback", green);
    delay.add<Knob> (s, dlyTone, "Tone", green);
    delay.add<Knob> (s, dlyMix, "Mix", green);

    auto blue = juce::Colour (0xff6b9dff);
    reverb.add<Knob> (s, revSize, "Size", blue);
    reverb.add<Knob> (s, revDamp, "Damping", blue);
    reverb.add<Knob> (s, revWidth, "Width", blue);
    reverb.add<Knob> (s, revMix, "Mix", blue);

    master.add<Knob> (s, ParamIDs::mix, "Dry/Wet", Palette::accent);
    master.add<Knob> (s, output, "Output", Palette::accent);

    for (auto* c : { &rave, &filter, &dirt, &lfo, &gate, &delay, &reverb, &master })
        addAndMakeVisible (c);

    setSize (1120, 600);
    timerCallback();
    startTimerHz (30);
}

RaveForgeEditor::~RaveForgeEditor()
{
    setLookAndFeel (nullptr);
}

void RaveForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().removeFromTop (48).reduced (18, 0);
    juce::ColourGradient grad (Palette::accent, (float) header.getX(), 0.0f,
                               Palette::accent2, (float) header.getX() + 200.0f, 0.0f, false);
    g.setGradientFill (grad);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("RAVEFORGE", header, juce::Justification::centredLeft);

    g.setColour (Palette::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("detuned stack  /  filter  /  dirt  /  lfo  /  gate  /  delay  /  reverb",
                header, juce::Justification::centredRight);
}

void RaveForgeEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (40);

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

void RaveForgeEditor::timerCallback()
{
    auto& s = raveProcessor.apvts;
    auto synced = s.getRawParameterValue (ParamIDs::lfoSync)->load() > 0.5f;
    lfoRateKnob->setEnabled (! synced);
    lfoDivChoice->setEnabled (synced);

    gateDisplay.setState (juce::roundToInt (s.getRawParameterValue (ParamIDs::gatePattern)->load()),
                          raveProcessor.engine.currentGateStep.load(),
                          s.getRawParameterValue (ParamIDs::gateOn)->load() > 0.5f);
}
