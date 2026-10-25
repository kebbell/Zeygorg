#include "Widgets.h"

namespace ui
{

void paintPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    r = r.reduced (0.5f);
    g.setGradientFill (juce::ColourGradient (Palette::panelTop, r.getX(), r.getY(),
                                             Palette::panel, r.getX(), r.getY() + juce::jmin (160.0f, r.getHeight()), false));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (Palette::panelEdge);
    g.drawRoundedRectangle (r, 9.0f, 1.0f);

    // A faint highlight along the top edge.
    g.setColour (juce::Colours::white.withAlpha (0.035f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 9.0f, r.getRight() - 9.0f);

    if (title.isNotEmpty())
    {
        g.setColour (Palette::text);
        g.setFont (Fonts::title (11.0f));
        g.drawText (title.toUpperCase(), (int) r.getX() + 14, (int) r.getY() + 9, (int) r.getWidth() - 28, 16,
                    juce::Justification::centredLeft);
    }
}

void paintWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Palette::well);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 6.0f, r.getRight() - 6.0f);
}

//==============================================================================
Knob::Knob (APVTS& state, const juce::String& id, const juce::String& name)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    // Set before the text box exists, since it copies these colours when created.
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 15);
    slider.setTextBoxIsEditable (true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.setScrollWheelEnabled (true);

    if (auto* p = state.getParameter (id))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64));
    }
    addAndMakeVisible (slider);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setFont (Fonts::label (10.0f));
    label.setColour (juce::Label::textColourId, Palette::dimText);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    // The attachment set the text before the formatter was in place, so refresh it.
    slider.updateText();
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    // Keep the knob round-ish however wide the cell is.
    auto knobH = juce::jmin (r.getHeight(), r.getWidth() * 3 / 4 + 15);
    slider.setBounds (r.withSizeKeepingCentre (r.getWidth(), knobH));
}

//==============================================================================
Choice::Choice (APVTS& state, const juce::String& id, const juce::String& name)
{
    // The attachment only selects items, so they have to exist first.
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
    {
        box.addItemList (p->choices, 1);
        box.setTooltip (p->getName (64));
    }
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
    box.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (box);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setFont (Fonts::label (10.0f));
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    box.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 8, 128), 24).withY (r.getY() + juce::jmax (4, (r.getHeight() - 24) / 2 - 6)));
}

//==============================================================================
Toggle::Toggle (APVTS& state, const juce::String& id, const juce::String& text, const juce::String& caption)
    : attachment (state, id, button)
{
    button.setButtonText (text);
    button.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (auto* p = state.getParameter (id))
        button.setTooltip (p->getName (64));
    addAndMakeVisible (button);

    if (caption.isNotEmpty())
    {
        label.setText (caption.toUpperCase(), juce::dontSendNotification);
        label.setFont (Fonts::label (10.0f));
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    }
}

void Toggle::resized()
{
    auto r = getLocalBounds();
    if (label.isVisible())
    {
        label.setBounds (r.removeFromTop (14));
        auto w = juce::jmin (r.getWidth() - 8, juce::jmax (64, button.getButtonText().length() * 7 + 34));
        button.setBounds (r.withSizeKeepingCentre (w, 24).withY (r.getY() + juce::jmax (4, (r.getHeight() - 24) / 2 - 6)));
    }
    else
    {
        button.setBounds (r);
    }
}

//==============================================================================
Section::Section (const juce::String& t, int c, int r)
    : cols (c), rows (r), title (t) {}

void Section::setFooter (juce::Component* c, int numRows)
{
    footer = c;
    footerRows = numRows;
    addAndMakeVisible (c);
}

void Section::setHeaderToggle (APVTS& state, const juce::String& id)
{
    headerToggle = std::make_unique<juce::ToggleButton>();
    headerToggle->setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (auto* p = state.getParameter (id))
        headerToggle->setTooltip (p->getName (64));
    headerAttachment = std::make_unique<APVTS::ButtonAttachment> (state, id, *headerToggle);
    headerToggle->onStateChange = [this] { updateDim(); };
    addAndMakeVisible (*headerToggle);
    updateDim();
}

void Section::updateDim()
{
    auto on = headerToggle == nullptr || headerToggle->getToggleState();
    for (auto* c : items)
        c->setAlpha (on ? 1.0f : 0.45f);
    if (footer != nullptr)
        footer->setAlpha (on ? 1.0f : 0.45f);
}

void Section::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds().toFloat(), title);
}

void Section::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    auto header = area.removeFromTop (26);
    if (headerToggle != nullptr)
        headerToggle->setBounds (header.removeFromRight (34).withSizeKeepingCentre (30, 16).translated (-4, 2));

    auto itemRows = footer != nullptr ? rows - footerRows : rows;
    auto cellH = area.getHeight() / rows;

    if (footer != nullptr)
        footer->setBounds (area.removeFromBottom (cellH * footerRows).reduced (6, 8));

    auto cellW = area.getWidth() / cols;
    for (int i = 0; i < items.size(); ++i)
    {
        auto col = i % cols, row = i / cols;
        if (row >= itemRows)
            break;
        // A little air between rows so a value never touches the next label.
        items[i]->setBounds (juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH)
                                 .withTrimmedBottom (row < itemRows - 1 ? 6 : 0));
    }
}

} // namespace ui
