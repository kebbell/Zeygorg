#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The shared look: near-black panels, greys, and white for anything "on".
// Kept plugin-agnostic so it can be reused across the Kebbell plugins.
namespace Palette
{
    inline const juce::Colour background { 0xff0a0a0b };
    inline const juce::Colour panel      { 0xff141416 };
    inline const juce::Colour panelTop   { 0xff19191c };
    inline const juce::Colour panelEdge  { 0xff26262b };
    inline const juce::Colour well       { 0xff0c0c0e };   // inset displays
    inline const juce::Colour control    { 0xff1e1e22 };   // buttons, combo boxes
    inline const juce::Colour controlEdge{ 0xff2e2e34 };
    inline const juce::Colour track      { 0xff2a2a2f };
    inline const juce::Colour text       { 0xffe8e8ea };
    inline const juce::Colour dimText    { 0xff8a8a91 };
    inline const juce::Colour faint      { 0xff4a4a50 };
    inline const juce::Colour accent     { 0xfff4f4f5 };   // white: values, "on"
    inline const juce::Colour accent2    { 0xffa9a9b0 };   // mid grey: secondary marks
}

namespace Fonts
{
    // Avenir Next for type, Menlo for numbers. Both ship with macOS; other
    // systems fall back to their default sans / mono.
    inline juce::Font title (float size = 11.0f)
    {
        return juce::Font (juce::FontOptions ("Avenir Next", "Demi Bold", size)).withExtraKerningFactor (0.12f);
    }

    inline juce::Font label (float size = 11.0f)
    {
        return juce::Font (juce::FontOptions ("Avenir Next", "Medium", size)).withExtraKerningFactor (0.06f);
    }

    inline juce::Font body (float size = 12.5f)
    {
        return juce::Font (juce::FontOptions ("Avenir Next", "Regular", size));
    }

    inline juce::Font mono (float size = 11.0f)
    {
        return juce::Font (juce::FontOptions ("Menlo", "Regular", size));
    }

    inline juce::Font logo (float size = 26.0f)
    {
        return juce::Font (juce::FontOptions ("Avenir Next", "Heavy", size)).withExtraKerningFactor (0.18f);
    }
}

class ZeyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZeyLookAndFeel()
    {
        using namespace Palette;
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, track.brighter (0.3f));
        setColour (juce::Label::textColourId, dimText);
        setColour (juce::Label::textWhenEditingColourId, text);
        setColour (juce::Label::backgroundWhenEditingColourId, control);
        setColour (juce::Label::outlineWhenEditingColourId, controlEdge);
        setColour (juce::TextEditor::backgroundColourId, control);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, track.brighter (0.3f));
        setColour (juce::TextEditor::outlineColourId, controlEdge);
        setColour (juce::TextEditor::focusedOutlineColourId, accent2);
        setColour (juce::CaretComponent::caretColourId, accent);
        setColour (juce::ComboBox::backgroundColourId, control);
        setColour (juce::ComboBox::outlineColourId, controlEdge);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dimText);
        setColour (juce::PopupMenu::backgroundColourId, panelTop);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::headerTextColourId, dimText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, track);
        setColour (juce::PopupMenu::highlightedTextColourId, accent);
        setColour (juce::TextButton::buttonColourId, control);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, background);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::TooltipWindow::backgroundColourId, panelTop);
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, controlEdge);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, text);
        setColour (juce::AlertWindow::outlineColourId, controlEdge);
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();
        auto lineW = juce::jlimit (2.0f, 3.5f, radius * 0.12f);
        auto arcR = radius - lineW * 0.5f;
        auto angle = startAngle + pos * (endAngle - startAngle);
        auto enabled = slider.isEnabled();
        auto hot = enabled && (slider.isMouseOverOrDragging());
        auto colour = enabled ? Palette::accent : Palette::faint;

        // Track.
        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Palette::track);
        g.strokePath (bg, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        // Value arc; bipolar parameters fill from the middle.
        auto fromAngle = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0 ? (startAngle + endAngle) * 0.5f : startAngle;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
        if (hot)
        {
            g.setColour (colour.withAlpha (0.10f));
            g.strokePath (arc, { lineW * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        }
        g.setColour (colour.withAlpha (enabled ? 0.95f : 0.6f));
        g.strokePath (arc, { lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

        // Cap.
        auto capR = arcR - lineW * 2.2f;
        auto cap = juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (cap.translated (0.0f, 1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (hot ? 0xff34343a : 0xff2c2c31), cap.getX(), cap.getY(),
                                                 juce::Colour (0xff18181b), cap.getX(), cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colour (0xff3a3a40));
        g.drawEllipse (cap.reduced (0.5f), 1.0f);

        // Pointer.
        auto p1 = centre.getPointOnCircumference (capR * 0.25f, angle);
        auto p2 = centre.getPointOnCircumference (capR * 0.86f, angle);
        g.setColour (enabled ? Palette::text : Palette::dimText);
        g.drawLine ({ p1, p2 }, juce::jmax (1.8f, capR * 0.12f));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (slider);
        l->setFont (Fonts::mono (10.5f));
        l->setJustificationType (juce::Justification::centred);
        return l;
    }

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& slider) override
    {
        auto layout = LookAndFeel_V4::getSliderLayout (slider);
        // Pull the readout up under the knob.
        if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
            layout.textBoxBounds = layout.textBoxBounds.translated (0, -2);
        return layout;
    }

    //==========================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
        auto hot = box.isMouseOver (true) && box.isEnabled();
        g.setColour (hot ? Palette::control.brighter (0.08f) : Palette::control);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (hot ? Palette::controlEdge.brighter (0.25f) : Palette::controlEdge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        juce::Path arrow;
        auto ax = (float) width - 11.0f, ay = (float) height * 0.5f;
        arrow.startNewSubPath (ax - 3.5f, ay - 1.5f);
        arrow.lineTo (ax, ay + 2.0f);
        arrow.lineTo (ax + 3.5f, ay - 1.5f);
        g.setColour (Palette::dimText.withAlpha (box.isEnabled() ? 1.0f : 0.35f));
        g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::body (12.5f); }
    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
    juce::Font getPopupMenuFont() override { return Fonts::body (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::label (12.0f); }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (Palette::panelTop);
        g.setColour (Palette::controlEdge);
        g.drawRect (0, 0, width, height);
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        g.setFont (Fonts::title (10.5f));
        g.setColour (Palette::dimText);
        g.drawFittedText (name.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft, 1);
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto on = b.getToggleState();
        auto base = on ? Palette::accent : Palette::control;
        if (b.isEnabled() && (highlighted || down))
            base = on ? base.darker (0.08f) : base.brighter (down ? 0.14f : 0.07f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 5.0f);
        if (! on)
        {
            g.setColour (Palette::controlEdge.brighter (highlighted ? 0.2f : 0.0f));
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        auto colour = b.getToggleState() ? Palette::background : Palette::text;
        g.setColour (colour.withAlpha (b.isEnabled() ? 1.0f : 0.35f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
    }

    // Toggles are pills with an LED.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto on = b.getToggleState();
        auto enabled = b.isEnabled();

        g.setColour (on ? Palette::control.brighter (0.1f) : Palette::control.brighter (highlighted ? 0.06f : 0.0f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Palette::accent2.withAlpha (0.5f) : Palette::controlEdge);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

        auto hasText = b.getButtonText().isNotEmpty();
        auto ledD = juce::jmin (7.0f, r.getHeight() * 0.36f);
        auto led = juce::Rectangle<float> (ledD, ledD).withCentre ({ hasText ? r.getX() + r.getHeight() * 0.5f + 2.0f : r.getCentreX(), r.getCentreY() });
        if (on && enabled)
        {
            g.setColour (Palette::accent.withAlpha (0.18f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setColour (on && enabled ? Palette::accent : Palette::faint);
        g.fillEllipse (led);

        if (hasText)
        {
            g.setFont (Fonts::label (11.5f));
            g.setColour ((on ? Palette::text : Palette::dimText).withAlpha (enabled ? 1.0f : 0.4f));
            g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (led.getRight() - r.getX() + 4.0f).withTrimmedRight (6.0f).toNearestInt(),
                              juce::Justification::centred, 1);
        }
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<int> (width, height).toFloat();
        g.setColour (Palette::panelTop);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Palette::controlEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (Palette::text);
        g.setFont (Fonts::body (12.5f));
        g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 3);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        auto w = juce::jmin (320, juce::GlyphArrangement::getStringWidthInt (Fonts::body (12.5f), text) + 18);
        auto h = 26;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                     pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
            .constrainedWithin (parent);
    }
};
