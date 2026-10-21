#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    // Monochrome: near-black panels, greys, and white for anything "on".
    inline const juce::Colour background { 0xff0a0a0a };
    inline const juce::Colour panel      { 0xff141414 };
    inline const juce::Colour panelEdge  { 0xff262626 };
    inline const juce::Colour track      { 0xff2a2a2a };
    inline const juce::Colour text       { 0xffe6e6e6 };
    inline const juce::Colour dimText    { 0xff8c8c8c };
    inline const juce::Colour accent     { 0xffe6e6e6 };
    inline const juce::Colour accent2    { 0xffa6a6a6 };
}

class RaveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RaveLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Palette::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Palette::dimText);
        setColour (juce::ComboBox::backgroundColourId, Palette::track);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, Palette::text);
        setColour (juce::ComboBox::arrowColourId, Palette::dimText);
        setColour (juce::PopupMenu::backgroundColourId, Palette::panel);
        setColour (juce::PopupMenu::textColourId, Palette::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::track.brighter (0.2f));
        setColour (juce::ToggleButton::textColourId, Palette::text);
        setColour (juce::ToggleButton::tickColourId, Palette::accent);
        setColour (juce::ToggleButton::tickDisabledColourId, Palette::dimText);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();
        auto lineW = juce::jmax (2.5f, radius * 0.14f);
        auto arcR = radius - lineW * 0.5f;
        auto angle = startAngle + pos * (endAngle - startAngle);

        auto accentProp = slider.getProperties()["accent"];
        auto accent = accentProp.isVoid() ? Palette::accent : juce::Colour ((juce::uint32) (juce::int64) accentProp);
        auto colour = slider.isEnabled() ? accent : Palette::dimText;

        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Palette::track);
        g.strokePath (bg, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        // Bipolar parameters fill from the middle.
        auto fromAngle = slider.getMinimum() < 0.0 ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, fromAngle, angle, true);
        g.setColour (colour.withAlpha (0.08f));
        g.strokePath (arc, { lineW * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        g.setColour (colour);
        g.strokePath (arc, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        auto knobR = arcR - lineW * 1.6f;
        g.setColour (Palette::panelEdge);
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -knobR + 3.0f, 3.0f, knobR * 0.5f, 1.5f);
        g.setColour (Palette::text);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat();
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 4.0f);

        juce::Path arrow;
        auto ax = (float) width - 12.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getLabelFont (juce::Label&) override { return juce::Font (juce::FontOptions (12.0f)); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto on = b.getToggleState();
        g.setColour (on ? Palette::accent : Palette::track.brighter (highlighted ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? Palette::background : Palette::dimText);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
    }
};
