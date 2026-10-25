#include "Displays.h"

namespace ui
{
namespace
{
// Visible along its whole parent chain (unlike isShowing(), this doesn't need a
// window, so offscreen snapshots still animate). Saves work on hidden pages.
bool isOnVisiblePage (const juce::Component& c)
{
    for (auto* p = &c; p != nullptr; p = p->getParentComponent())
        if (! p->isVisible())
            return false;
    return true;
}
}

//==============================================================================
Visualiser::Visualiser (const float* r, int size, const std::atomic<int>& w, std::function<double()> sr)
    : ring (r), ringSize (size), writeIndex (w), getSampleRate (std::move (sr))
{
    spectrumDb.fill (-100.0f);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to switch between scope and spectrum");
    startTimerHz (30);
}

void Visualiser::timerCallback()
{
    if (! isOnVisiblePage (*this))
        return;

    // Newest fftSize samples, oldest first.
    auto w = writeIndex.load (std::memory_order_acquire);
    for (int i = 0; i < fftSize; ++i)
        snapshot[(size_t) i] = ring[(w - fftSize + i + ringSize) & (ringSize - 1)];

    if (mode != Mode::scope)
    {
        std::copy (snapshot.begin(), snapshot.end(), fftData.begin());
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        for (int i = 0; i < fftSize / 2; ++i)
        {
            auto db = juce::Decibels::gainToDecibels (fftData[(size_t) i] * 4.0f / (float) fftSize, -100.0f);
            auto& s = spectrumDb[(size_t) i];
            s = db > s ? db : s - 1.5f; // quick rise, slow fall
        }
    }
    repaint();
}

void Visualiser::mouseUp (const juce::MouseEvent& e)
{
    if (clickToSwitch && e.mouseWasClicked())
        setMode (mode == Mode::scope ? Mode::spectrum : Mode::scope);
}

void Visualiser::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    paintWell (g, r);
    r = r.reduced (4.0f, 3.0f);

    if (mode == Mode::both)
    {
        auto left = r.removeFromLeft (r.getWidth() * 0.4f);
        paintScope (g, left);
        g.setColour (Palette::panelEdge);
        g.drawVerticalLine ((int) r.getX() + 2, r.getY() + 2.0f, r.getBottom() - 2.0f);
        paintSpectrum (g, r.withTrimmedLeft (6.0f));
    }
    else if (mode == Mode::scope)
        paintScope (g, r);
    else
        paintSpectrum (g, r);
}

void Visualiser::paintScope (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Palette::panelEdge);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    // Trigger on a rising zero crossing so the trace sits still.
    const int span = 1024;
    auto startIndex = fftSize - span;
    for (int i = fftSize - span; i > span / 2; --i)
        if (snapshot[(size_t) i - 1] <= 0.0f && snapshot[(size_t) i] > 0.0f)
        {
            startIndex = i - span / 2;
            break;
        }
    startIndex = juce::jlimit (0, fftSize - span, startIndex);

    float peak = 0.05f;
    for (int i = 0; i < span; ++i)
        peak = juce::jmax (peak, std::abs (snapshot[(size_t) (startIndex + i)]));
    auto scale = juce::jmin (1.0f / peak, 4.0f) * 0.9f; // auto-gain, but don't blow up silence

    juce::Path p;
    auto w = (int) r.getWidth();
    for (int x = 0; x <= w; ++x)
    {
        auto idx = startIndex + juce::jmin (span - 1, x * span / juce::jmax (1, w));
        auto y = r.getCentreY() - snapshot[(size_t) idx] * scale * r.getHeight() * 0.5f;
        if (x == 0) p.startNewSubPath (r.getX(), y);
        else        p.lineTo (r.getX() + (float) x, y);
    }
    g.setColour (Palette::accent.withAlpha (0.15f));
    g.strokePath (p, juce::PathStrokeType (3.0f));
    g.setColour (Palette::accent.withAlpha (0.9f));
    g.strokePath (p, juce::PathStrokeType (1.2f));
}

void Visualiser::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto sr = getSampleRate != nullptr ? getSampleRate() : 44100.0;
    if (sr <= 0.0)
        sr = 44100.0;

    auto xFor = [&r] (double hz) { return r.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * r.getWidth(); };
    g.setColour (Palette::panelEdge);
    for (auto hz : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine ((int) xFor (hz), r.getY(), r.getBottom());

    auto yFor = [&r] (float db) { return r.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 84.0f) / 84.0f) * r.getHeight(); };

    juce::Path p;
    auto w = (int) r.getWidth();
    p.startNewSubPath (r.getX(), r.getBottom());
    for (int x = 0; x <= w; x += 2)
    {
        auto f0 = 20.0 * std::pow (1000.0, (double) x / w);
        auto f1 = 20.0 * std::pow (1000.0, (double) (x + 2) / w);
        auto exact = juce::jlimit (1.0, (double) fftSize / 2 - 2, f0 / sr * fftSize);
        auto b0 = (int) exact;
        auto b1 = juce::jlimit (b0, fftSize / 2 - 1, (int) (f1 / sr * fftSize));
        float db;
        if (b1 <= b0 + 1)
        {
            // Several pixels per bin down low: interpolate so it curves instead of stepping.
            auto frac = (float) (exact - b0);
            db = spectrumDb[(size_t) b0] + frac * (spectrumDb[(size_t) b0 + 1] - spectrumDb[(size_t) b0]);
        }
        else
        {
            db = -100.0f;
            for (int b = b0; b <= b1; ++b)
                db = juce::jmax (db, spectrumDb[(size_t) b]);
        }
        p.lineTo (r.getX() + (float) x, yFor (db));
    }
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();

    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.35f), r.getX(), r.getY(),
                                             Palette::accent.withAlpha (0.02f), r.getX(), r.getBottom(), false));
    g.fillPath (p);
    g.setColour (Palette::accent.withAlpha (0.85f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

//==============================================================================
LevelMeter::LevelMeter (const std::atomic<float>& l, const std::atomic<float>& r)
    : left (l), right (r)
{
    setTooltip ("Output level");
    startTimerHz (30);
}

void LevelMeter::timerCallback()
{
    const std::atomic<float>* src[] { &left, &right };
    for (int i = 0; i < 2; ++i)
    {
        auto db = juce::Decibels::gainToDecibels (src[i]->load(), -60.0f);
        levels[i] = db > levels[i] ? db : juce::jmax (db, levels[i] - 1.2f);
        if (db >= holds[i]) { holds[i] = db; holdTimers[i] = 40; }
        else if (--holdTimers[i] <= 0) holds[i] = juce::jmax (-60.0f, holds[i] - 0.8f);
    }
    if (isOnVisiblePage (*this))
        repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    paintWell (g, r);
    r = r.reduced (3.0f);
    auto barW = (r.getWidth() - 2.0f) * 0.5f;

    for (int i = 0; i < 2; ++i)
    {
        auto bar = juce::Rectangle<float> (r.getX() + (float) i * (barW + 2.0f), r.getY(), barW, r.getHeight());
        auto toY = [&bar] (float db) { return bar.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * bar.getHeight(); };
        g.setColour (Palette::track.withAlpha (0.6f));
        g.fillRoundedRectangle (bar, 1.5f);

        auto top = toY (levels[i]);
        g.setColour (levels[i] > -0.5f ? juce::Colours::white : Palette::accent2);
        g.fillRoundedRectangle (bar.withTop (top), 1.5f);
        g.setColour (Palette::accent);
        g.fillRect (bar.getX(), toY (holds[i]) - 1.0f, bar.getWidth(), 1.5f);
    }
}

//==============================================================================
ReductionMeter::ReductionMeter (std::function<float()> f) : getDb (std::move (f))
{
    setTooltip ("Compressor gain reduction");
    startTimerHz (30);
}

void ReductionMeter::timerCallback()
{
    auto db = getDb != nullptr ? getDb() : 0.0f;
    shown = db < shown ? db : shown + (db - shown) * 0.2f;
    if (isOnVisiblePage (*this))
        repaint();
}

void ReductionMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    r = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 26.0f));
    paintWell (g, r);
    r = r.reduced (6.0f, 6.0f);
    g.setFont (Fonts::label (9.5f));
    g.setColour (Palette::dimText);
    g.drawText ("GAIN REDUCTION", r.removeFromLeft (100.0f), juce::Justification::centredLeft);
    auto amount = juce::jlimit (0.0f, 1.0f, -shown / 24.0f);
    g.setColour (Palette::track);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (r.withWidth (r.getWidth() * amount), 2.0f);
    g.setFont (Fonts::mono (9.5f));
    g.setColour (amount > 0.8f ? Palette::background : Palette::text);
    g.drawText (juce::String (shown, 1) + " dB", r.reduced (4.0f, 0.0f), juce::Justification::centredRight);
}

//==============================================================================
EnvelopeEditor::EnvelopeEditor (APVTS& s, juce::String a, juce::String d, juce::String su, juce::String re)
    : state (s), ids { std::move (a), std::move (d), std::move (su), std::move (re) }
{
    setTooltip ("Drag the points to shape the envelope");
    startTimerHz (20);
}

void EnvelopeEditor::timerCallback()
{
    // Repaint only when a parameter moved (e.g. from a knob or automation).
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        auto v = norm (ids[i]);
        changed = changed || ! juce::approximatelyEqual (v, lastValues[i]);
        lastValues[i] = v;
    }
    if (changed)
        repaint();
}

float EnvelopeEditor::norm (const juce::String& id) const
{
    auto* p = state.getParameter (id);
    return p != nullptr ? p->getValue() : 0.0f;
}

void EnvelopeEditor::setNorm (const juce::String& id, float v)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

juce::Rectangle<float> EnvelopeEditor::plot() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 9.0f);
}

EnvelopeEditor::Points EnvelopeEditor::points() const
{
    auto r = plot();
    auto seg = r.getWidth() * 0.29f;
    Points p;
    p.start = { r.getX(), r.getBottom() };
    p.peak = { r.getX() + seg * norm (ids[0]), r.getY() };
    auto sustainY = r.getBottom() - norm (ids[2]) * r.getHeight();
    p.sustain = { p.peak.x + seg * norm (ids[1]), sustainY };
    p.sustainEnd = { p.sustain.x + r.getWidth() * 0.13f, sustainY };
    p.end = { p.sustainEnd.x + seg * norm (ids[3]), r.getBottom() };
    return p;
}

int EnvelopeEditor::handleAt (juce::Point<float> pos) const
{
    auto p = points();
    juce::Point<float> handles[] { p.peak, p.sustain, p.end };
    auto best = -1;
    auto bestDist = 14.0f;
    for (int i = 0; i < 3; ++i)
    {
        auto d = handles[i].getDistanceFrom (pos);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    auto r = plot();
    auto p = points();

    g.setColour (Palette::panelEdge.withAlpha (0.7f));
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());

    // Curved segments, like the real (exponential-ish) envelope.
    juce::Path curve;
    curve.startNewSubPath (p.start);
    curve.quadraticTo ({ p.start.x + (p.peak.x - p.start.x) * 0.35f, p.peak.y + (p.start.y - p.peak.y) * 0.2f }, p.peak);
    curve.quadraticTo ({ p.peak.x + (p.sustain.x - p.peak.x) * 0.25f, p.sustain.y }, p.sustain);
    curve.lineTo (p.sustainEnd);
    curve.quadraticTo ({ p.sustainEnd.x + (p.end.x - p.sustainEnd.x) * 0.25f, p.end.y }, p.end);

    auto fill = curve;
    fill.lineTo (p.end.x, r.getBottom());
    fill.lineTo (p.start);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.22f), r.getX(), r.getY(),
                                             Palette::accent.withAlpha (0.02f), r.getX(), r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Palette::accent);
    g.strokePath (curve, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Point<float> handles[] { p.peak, p.sustain, p.end };
    for (int i = 0; i < 3; ++i)
    {
        auto active = i == hover || i == dragging;
        auto d = active ? 10.0f : 7.0f;
        auto rr = juce::Rectangle<float> (d, d).withCentre (handles[i]);
        g.setColour (Palette::background);
        g.fillEllipse (rr.expanded (2.0f));
        g.setColour (active ? Palette::accent : Palette::accent2);
        g.fillEllipse (rr);
    }

    g.setFont (Fonts::label (9.5f));
    g.setColour (Palette::faint);
    g.drawText ("A", juce::Rectangle<float> (p.start.x, r.getBottom() - 12.0f, juce::jmax (12.0f, p.peak.x - p.start.x), 12.0f), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (p.sustainEnd.x, r.getBottom() - 12.0f, juce::jmax (12.0f, p.end.x - p.sustainEnd.x), 12.0f), juce::Justification::centred);
}

void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
{
    auto h = handleAt (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EnvelopeEditor::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position);
    auto gesture = [this] (int idx, bool begin)
    {
        if (auto* p = state.getParameter (ids[idx]))
            begin ? p->beginChangeGesture() : p->endChangeGesture();
    };
    if (dragging == 0) gesture (0, true);
    if (dragging == 1) { gesture (1, true); gesture (2, true); }
    if (dragging == 2) gesture (3, true);
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    auto r = plot();
    auto seg = r.getWidth() * 0.29f;
    auto p = points();

    if (dragging == 0)
        setNorm (ids[0], (e.position.x - r.getX()) / seg);
    else if (dragging == 1)
    {
        setNorm (ids[1], (e.position.x - p.peak.x) / seg);
        setNorm (ids[2], (r.getBottom() - e.position.y) / r.getHeight());
    }
    else
        setNorm (ids[3], (e.position.x - p.sustainEnd.x) / seg);
    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    auto end = [this] (int idx)
    {
        if (auto* p = state.getParameter (ids[idx]))
            p->endChangeGesture();
    };
    if (dragging == 0) end (0);
    if (dragging == 1) { end (1); end (2); }
    if (dragging == 2) end (3);
    dragging = -1;
}

//==============================================================================
PumpDisplay::PumpDisplay (APVTS& s, juce::String d, juce::String r, std::function<float()> g)
    : state (s), depthId (std::move (d)), releaseId (std::move (r)), currentGain (std::move (g))
{
    startTimerHz (30);
}

void PumpDisplay::timerCallback()
{
    shownGain = currentGain != nullptr ? currentGain() : 1.0f;
    if (isOnVisiblePage (*this))
        repaint();
}

void PumpDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    auto r = bounds.reduced (8.0f, 7.0f);
    auto meter = r.removeFromRight (6.0f);
    r.removeFromRight (6.0f);

    auto depth = state.getRawParameterValue (depthId)->load();
    auto release = state.getRawParameterValue (releaseId)->load();
    auto rr = 0.08f + 0.9f * release;

    // Two periods of the same curve the DSP uses.
    juce::Path p;
    auto w = (int) r.getWidth();
    for (int x = 0; x <= w; ++x)
    {
        auto phase = std::fmod ((float) x / (float) w * 2.0f, 1.0f);
        auto t = juce::jmin (1.0f, phase / rr);
        auto gain = 1.0f - depth * (1.0f - t * t * (3.0f - 2.0f * t));
        auto y = r.getBottom() - gain * r.getHeight();
        if (x == 0) p.startNewSubPath (r.getX(), y);
        else        p.lineTo (r.getX() + (float) x, y);
    }
    auto fill = p;
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.2f), r.getX(), r.getY(),
                                             Palette::accent.withAlpha (0.02f), r.getX(), r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (Palette::accent);
    g.strokePath (p, juce::PathStrokeType (1.5f));

    g.setColour (Palette::track);
    g.fillRoundedRectangle (meter, 2.0f);
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * shownGain), 2.0f);
}

} // namespace ui
