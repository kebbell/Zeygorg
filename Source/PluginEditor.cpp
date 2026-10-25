#include "PluginEditor.h"

namespace ui
{

//==============================================================================
TabBar::TabBar (juce::StringArray n) : names (std::move (n))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<int> TabBar::tabBounds (int i) const
{
    const int w = 112;
    return { i * w, 0, w, getHeight() };
}

void TabBar::paint (juce::Graphics& g)
{
    g.setColour (Palette::panelEdge);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    for (int i = 0; i < names.size(); ++i)
    {
        auto r = tabBounds (i);
        auto selected = i == current;
        g.setFont (Fonts::title (12.0f));
        g.setColour (selected ? Palette::text : i == hover ? Palette::accent2 : Palette::dimText);
        g.drawText (names[i].toUpperCase(), r.withTrimmedBottom (3), juce::Justification::centred);
        if (selected)
        {
            g.setColour (Palette::accent);
            g.fillRoundedRectangle (r.reduced (22, 0).removeFromBottom (2).toFloat(), 1.0f);
        }
    }
}

void TabBar::setCurrent (int index)
{
    index = juce::jlimit (0, names.size() - 1, index);
    if (index == current)
        return;
    current = index;
    repaint();
    if (onChange != nullptr)
        onChange (current);
}

void TabBar::mouseUp (const juce::MouseEvent& e)
{
    for (int i = 0; i < names.size(); ++i)
        if (tabBounds (i).contains (e.getPosition()))
            setCurrent (i);
}

void TabBar::mouseMove (const juce::MouseEvent& e)
{
    auto h = -1;
    for (int i = 0; i < names.size(); ++i)
        if (tabBounds (i).contains (e.getPosition()))
            h = i;
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void TabBar::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

//==============================================================================
Keyboard::Keyboard (juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard) {}

void Keyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                              juce::Colour, juce::Colour)
{
    auto r = area.reduced (0.75f, 0.0f).withTrimmedBottom (1.0f);
    g.setGradientFill (juce::ColourGradient (isDown ? juce::Colour (0xfff2f2f3) : juce::Colour (0xffcfcfd2), r.getX(), r.getY(),
                                             isDown ? juce::Colour (0xffd8d8da) : juce::Colour (0xffb0b0b4), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    if (isOver && ! isDown)
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRoundedRectangle (r, 3.0f);
    }
    if (note % 12 == 0)
    {
        g.setFont (Fonts::label (9.0f));
        g.setColour (juce::Colour (0xff5a5a60));
        g.drawText ("C" + juce::String (note / 12 - 2), r.removeFromBottom (14.0f), juce::Justification::centred);
    }
}

void Keyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    auto r = area.reduced (1.0f, 0.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 2.5f);
    g.setGradientFill (juce::ColourGradient (isDown ? juce::Colour (0xffe8e8ea) : juce::Colour (0xff2c2c30), r.getX(), r.getY(),
                                             isDown ? juce::Colour (0xffbdbdc2) : juce::Colour (0xff111113), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 2.5f);
    if (isOver && ! isDown)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (r, 2.5f);
    }
}
} // namespace ui

//==============================================================================
ZeygorgEditor::ZeygorgEditor (ZeygorgProcessor& p)
    : AudioProcessorEditor (&p), zeyProcessor (p),
      presetBar (p.presets),
      visualiser (p.engine.scope.data(), rf::Engine::scopeSize, p.engine.scopeWrite, [&p] { return p.getSampleRate(); }),
      meter (p.engine.peakLeft, p.engine.peakRight),
      synthPage (p), sequencePage (p), fxPage (p),
      keyboard (p.keyboardState)
{
    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &visualiser, &meter, &tabs, &synthPage, &sequencePage, &fxPage, &keyboard })
        content.addAndMakeVisible (c);

    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::transparentBlack);
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);
    keyboard.setAvailableRange (24, 96);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setWantsKeyboardFocus (false);
    keyboard.setMouseCursor (juce::MouseCursor::PointingHandCursor);

    tabs.onChange = [this] (int i) { showPage (i); };
    showPage (juce::jlimit (0, 2, (int) p.apvts.state.getProperty ("uiPage", 0)));

    // Resizable with a fixed aspect ratio; everything scales together.
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 2, baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) baseWidth / baseHeight);
    auto scale = juce::jlimit (0.6, 2.0, (double) p.apvts.state.getProperty ("uiScale", 1.0));
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));

    startTimerHz (4);
}

ZeygorgEditor::~ZeygorgEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void ZeygorgEditor::showPage (int index)
{
    synthPage.setVisible (index == 0);
    sequencePage.setVisible (index == 1);
    fxPage.setVisible (index == 2);
    if (tabs.getCurrent() != index)
        tabs.setCurrent (index);
    zeyProcessor.apvts.state.setProperty ("uiPage", index, nullptr);
}

void ZeygorgEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);
}

void ZeygorgEditor::resized()
{
    auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, baseWidth, baseHeight);
    zeyProcessor.apvts.state.setProperty ("uiScale", scale, nullptr);
}

void ZeygorgEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Header.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff131315), 0.0f, 0.0f, Palette::background, 0.0f, 64.0f, false));
    g.fillRect (0, 0, baseWidth, 64);

    g.setColour (Palette::text);
    g.setFont (Fonts::logo (25.0f));
    g.drawText ("ZEYGORG", 22, 11, 220, 30, juce::Justification::centredLeft);
    g.setFont (Fonts::title (8.5f));
    g.setColour (Palette::dimText);
    g.drawText ("RAVE SYNTH  +  BOOST ENGINE", 24, 39, 240, 12, juce::Justification::centredLeft);

    // Status on the tab row.
    g.setFont (Fonts::body (11.5f));
    g.setColour (Palette::faint);
    auto voices = juce::jmax (0, shownVoices);
    g.drawText (juce::String (voices) + (voices == 1 ? " voice" : " voices") + "     Drop audio files onto a layer to play them",
                tabs.getBounds().withTrimmedRight (6).withTrimmedBottom (2), juce::Justification::centredRight);
}

void ZeygorgEditor::layoutContent()
{
    presetBar.setBounds (310, 18, 480, 28);
    meter.setBounds (baseWidth - 22 - 16, 11, 16, 42);
    visualiser.setBounds (meter.getX() - 8 - 330, 11, 330, 42);

    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (12, 0);
    area.removeFromTop (66);
    tabs.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);

    area.removeFromBottom (12);
    keyboard.setBounds (area.removeFromBottom (82));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f); // C1..C7 = 43 white keys
    area.removeFromBottom (10);

    for (auto* page : std::initializer_list<juce::Component*> { &synthPage, &sequencePage, &fxPage })
        page->setBounds (area);
}

void ZeygorgEditor::timerCallback()
{
    auto v = zeyProcessor.synth.activeVoices.load();
    if (v != shownVoices)
    {
        shownVoices = v;
        content.repaint (tabs.getBounds());
    }
}
