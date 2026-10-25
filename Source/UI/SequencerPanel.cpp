#include "SequencerPanel.h"
#include <map>

namespace ui
{
namespace
{
const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::String noteName (int midi) { return noteNames[midi % 12] + juce::String (midi / 12 - 2); }

struct SeqView
{
    int root, scale, octaves, steps, rows;
};

SeqView readView (ZeygorgProcessor& p)
{
    auto& s = p.apvts;
    SeqView v;
    v.root = juce::roundToInt (getParameter (s, ParamIDs::randRoot));
    v.scale = juce::roundToInt (getParameter (s, ParamIDs::randScale));
    v.octaves = juce::roundToInt (getParameter (s, ParamIDs::randOctaves));
    v.steps = juce::roundToInt (getParameter (s, ParamIDs::seqSteps));
    v.rows = rf::Scales::degreeCount (v.scale, v.octaves);
    return v;
}

constexpr int gutter = 38;
}

//==============================================================================
StepGrid::StepGrid (ZeygorgProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void StepGrid::timerCallback()
{
    auto v = readView (processor);
    auto version = processor.pattern.version.load();
    auto step = processor.sequencer.currentStep.load();
    if (version != lastVersion || step != lastStep || v.rows != lastRows || v.steps != lastSteps
        || v.root != lastRoot || v.scale != lastScale)
    {
        lastVersion = version; lastStep = step; lastRows = v.rows; lastSteps = v.steps;
        lastRoot = v.root; lastScale = v.scale;
        repaint();
    }
}

juce::Rectangle<float> StepGrid::gridArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft ((float) gutter).reduced (4.0f, 4.0f);
}

int StepGrid::rows() const
{
    return readView (const_cast<ZeygorgProcessor&> (processor)).rows;
}

std::pair<int, int> StepGrid::cellAt (juce::Point<float> pos) const
{
    auto r = gridArea();
    auto n = rows();
    auto step = juce::jlimit (0, rf::Pattern::maxSteps - 1, (int) std::floor ((pos.x - r.getX()) / r.getWidth() * rf::Pattern::maxSteps));
    auto row = juce::jlimit (0, n - 1, (int) std::floor ((pos.y - r.getY()) / r.getHeight() * (float) n));
    return { step, n - 1 - row };
}

void StepGrid::paint (juce::Graphics& g)
{
    auto v = readView (processor);
    paintWell (g, getLocalBounds().toFloat());
    auto r = gridArea();
    auto cellW = r.getWidth() / rf::Pattern::maxSteps;
    auto cellH = r.getHeight() / (float) v.rows;
    const auto& scale = rf::Scales::get (v.scale);
    auto seqOn = getParameter (processor.apvts, ParamIDs::randOn) > 0.5f;

    // Rows: roots stand out.
    for (int row = 0; row < v.rows; ++row)
    {
        auto degree = v.rows - 1 - row;
        auto y = r.getY() + (float) row * cellH;
        auto isRoot = degree % scale.size == 0;
        g.setColour (isRoot ? juce::Colour (0xff1b1b1f) : juce::Colour (0xff111113));
        g.fillRect (r.getX(), y, r.getWidth(), cellH);

        auto note = rf::Scales::noteForDegree (degree, v.root, v.scale);
        g.setFont (isRoot ? Fonts::label (9.5f) : Fonts::mono (9.0f));
        g.setColour (isRoot ? Palette::text : Palette::dimText.withAlpha (0.7f));
        if (cellH >= 9.0f || isRoot)
            g.drawText (noteName (note), juce::Rectangle<float> (4.0f, y, (float) gutter - 6.0f, cellH), juce::Justification::centredRight);
    }

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    for (int row = 1; row < v.rows; ++row)
        g.drawHorizontalLine ((int) (r.getY() + (float) row * cellH), r.getX(), r.getRight());

    for (int c = 1; c < rf::Pattern::maxSteps; ++c)
    {
        g.setColour (c % 4 == 0 ? Palette::faint.withAlpha (0.8f) : juce::Colours::black.withAlpha (0.7f));
        g.drawVerticalLine ((int) (r.getX() + (float) c * cellW), r.getY(), r.getBottom());
    }

    // Playhead.
    auto current = processor.sequencer.currentStep.load();
    if (seqOn && current >= 0)
    {
        g.setColour (Palette::accent.withAlpha (0.07f));
        g.fillRect (r.getX() + (float) current * cellW, r.getY(), cellW, r.getHeight());
    }

    // Notes.
    for (int st = 0; st < rf::Pattern::maxSteps; ++st)
    {
        auto step = processor.pattern.get (st);
        if (step.isRest())
            continue;
        auto degree = juce::jmin (step.degree, v.rows - 1);
        auto row = v.rows - 1 - degree;
        auto cell = juce::Rectangle<float> (r.getX() + (float) st * cellW, r.getY() + (float) row * cellH, cellW, cellH).reduced (2.0f, juce::jmin (2.0f, cellH * 0.15f));
        auto vel = (float) step.velocity / 127.0f;
        auto playing = seqOn && st == current;
        if (playing)
        {
            g.setColour (Palette::accent.withAlpha (0.25f));
            g.fillRoundedRectangle (cell.expanded (2.0f), 4.0f);
        }
        g.setColour (Palette::accent.withAlpha (playing ? 1.0f : 0.35f + 0.55f * vel));
        g.fillRoundedRectangle (cell, 3.0f);
        if (step.degree >= v.rows)
        {
            g.setColour (Palette::background);
            g.drawRoundedRectangle (cell.reduced (2.0f), 2.0f, 1.0f);
        }
    }

    // Steps past the loop length are dimmed.
    if (v.steps < rf::Pattern::maxSteps)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRect (r.withLeft (r.getX() + (float) v.steps * cellW));
    }
}

void StepGrid::apply (int step, int degree)
{
    auto current = processor.pattern.get (step);
    if (erasing)
    {
        if (! current.isRest())
            processor.pattern.set (step, {});
    }
    else if (current.degree != degree)
    {
        processor.pattern.set (step, { degree, current.isRest() ? 100 : current.velocity });
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    auto [step, degree] = cellAt (e.position);
    auto current = processor.pattern.get (step);
    erasing = ! current.isRest() && juce::jmin (current.degree, rows() - 1) == degree;
    apply (step, degree);
    lastPaintedStep = step;
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    auto [step, degree] = cellAt (e.position);
    if (erasing && step == lastPaintedStep)
        return;
    apply (step, degree);
    lastPaintedStep = step;
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    lastPaintedStep = -1;
}

//==============================================================================
VelocityLane::VelocityLane (ZeygorgProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    startTimerHz (20);
}

void VelocityLane::timerCallback()
{
    auto v = processor.pattern.version.load();
    if (v != lastVersion)
    {
        lastVersion = v;
        repaint();
    }
}

void VelocityLane::paint (juce::Graphics& g)
{
    paintWell (g, getLocalBounds().toFloat());
    auto r = getLocalBounds().toFloat().withTrimmedLeft ((float) leftGutter).reduced (4.0f, 5.0f);
    auto cellW = r.getWidth() / rf::Pattern::maxSteps;

    g.setFont (Fonts::label (9.0f));
    g.setColour (Palette::faint);
    g.drawText ("VEL", juce::Rectangle<float> (2.0f, r.getY(), (float) leftGutter - 6.0f, r.getHeight()), juce::Justification::centredRight);

    for (int st = 0; st < rf::Pattern::maxSteps; ++st)
    {
        auto step = processor.pattern.get (st);
        auto col = juce::Rectangle<float> (r.getX() + (float) st * cellW, r.getY(), cellW, r.getHeight()).reduced (cellW * 0.22f, 0.0f);
        g.setColour (Palette::track.withAlpha (0.5f));
        g.fillRoundedRectangle (col, 2.0f);
        if (step.isRest())
            continue;
        auto h = col.getHeight() * (float) step.velocity / 127.0f;
        g.setColour (Palette::accent.withAlpha (0.85f));
        g.fillRoundedRectangle (col.withTop (col.getBottom() - h), 2.0f);
    }
}

void VelocityLane::mouseDrag (const juce::MouseEvent& e)
{
    auto r = getLocalBounds().toFloat().withTrimmedLeft ((float) leftGutter).reduced (4.0f, 5.0f);
    auto st = juce::jlimit (0, rf::Pattern::maxSteps - 1, (int) ((e.position.x - r.getX()) / r.getWidth() * rf::Pattern::maxSteps));
    auto step = processor.pattern.get (st);
    if (step.isRest())
        return;
    step.velocity = juce::jlimit (1, 127, juce::roundToInt ((r.getBottom() - e.position.y) / r.getHeight() * 127.0f));
    processor.pattern.set (st, step);
    repaint();
}

//==============================================================================
DragMidiButton::DragMidiButton (ZeygorgProcessor& p)
    : Button ("Drag MIDI", "Drag onto a MIDI track to drop the pattern as a clip"), processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void DragMidiButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 5)
        return;
    dragStarted = true;
    auto file = processor.writePatternMidi();
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                [safe = juce::Component::SafePointer<DragMidiButton> (this)]
                                                                {
                                                                    if (safe != nullptr)
                                                                        safe->dragStarted = false;
                                                                });
}

//==============================================================================
SequencerPanel::SequencerPanel (ZeygorgProcessor& p)
    : processor (p),
      powerAttachment (p.apvts, ParamIDs::randOn, power),
      key (p.apvts, ParamIDs::randRoot, "Key"),
      scale (p.apvts, ParamIDs::randScale, "Scale"),
      rate (p.apvts, ParamIDs::randRate, "Rate"),
      octaves (p.apvts, ParamIDs::randOctaves, "Octaves"),
      steps (p.apvts, ParamIDs::seqSteps, "Steps"),
      length (p.apvts, ParamIDs::randLength, "Length"),
      swing (p.apvts, ParamIDs::seqSwing, "Swing"),
      density (p.apvts, ParamIDs::randChance, "Density"),
      dragMidi (p),
      grid (p),
      velocity (p)
{
    power.setTooltip ("Sequencer on/off");
    power.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    for (auto* c : std::initializer_list<juce::Component*> { &power, &key, &scale, &rate, &octaves, &steps, &length, &swing, &density,
                                                             &newPattern, &varyPattern, &clear, &left, &right, &dragMidi, &grid, &velocity })
        addAndMakeVisible (c);

    newPattern.onClick = [this]
    {
        auto v = readView (processor);
        processor.pattern.generate (juce::Random::getSystemRandom().nextInt ({ 1, 100000 }),
                                    getParameter (processor.apvts, ParamIDs::randChance), v.rows);
    };
    varyPattern.onClick = [this] { vary(); };
    clear.onClick = [this] { processor.pattern.clear(); };
    left.onClick = [this] { shift (-1); };
    right.onClick = [this] { shift (1); };
}

void SequencerPanel::shift (int delta)
{
    auto n = readView (processor).steps;
    std::vector<rf::Pattern::Step> s;
    for (int i = 0; i < n; ++i)
        s.push_back (processor.pattern.get (i));
    for (int i = 0; i < n; ++i)
        processor.pattern.set (((i + delta) % n + n) % n, s[(size_t) i]);
}

void SequencerPanel::vary()
{
    auto v = readView (processor);
    auto& rng = juce::Random::getSystemRandom();
    for (int k = 0; k < 3; ++k)
    {
        auto i = rng.nextInt (v.steps);
        auto step = processor.pattern.get (i);
        if (step.isRest() || rng.nextFloat() < 0.75f)
            processor.pattern.set (i, { rng.nextInt (v.rows), step.isRest() ? 100 : step.velocity });
        else
            processor.pattern.set (i, {});
    }
}

void SequencerPanel::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds().toFloat());
    auto header = getLocalBounds().reduced (10, 0).removeFromTop (34);
    header.removeFromLeft (26);
    g.setFont (Fonts::title (11.0f));
    g.setColour (power.getToggleState() ? Palette::text : Palette::dimText);
    g.drawText ("SEQUENCER", header, juce::Justification::centredLeft);
    g.setFont (Fonts::body (11.5f));
    g.setColour (Palette::faint);
    g.drawText ("Click to place notes, drag to paint. Locked to the DAW's grid.", header, juce::Justification::centredRight);
}

void SequencerPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 0);
    auto header = area.removeFromTop (34);
    power.setBounds (header.removeFromLeft (20).withSizeKeepingCentre (20, 16));

    auto controls = area.removeFromTop (74);
    auto buttons = controls.removeFromRight (224);
    std::pair<juce::Component*, int> cells[] { { &key, 66 }, { &scale, 124 }, { &rate, 72 }, { &octaves, 60 }, { &steps, 60 },
                                               { &length, 60 }, { &swing, 60 }, { &density, 60 } };
    auto total = 0;
    for (auto& c : cells) total += c.second;
    auto extra = juce::jmax (0, controls.getWidth() - total) / (int) std::size (cells);
    for (auto& [comp, w] : cells)
        comp->setBounds (controls.removeFromLeft (w + extra));

    buttons.removeFromLeft (10);
    auto b1 = buttons.removeFromTop (buttons.getHeight() / 2).reduced (0, 5).withTrimmedTop (8);
    auto b2 = buttons.reduced (0, 5).withTrimmedBottom (8);
    auto bw = (b1.getWidth() - 12) / 3;
    newPattern.setBounds (b1.removeFromLeft (bw)); b1.removeFromLeft (6);
    varyPattern.setBounds (b1.removeFromLeft (bw)); b1.removeFromLeft (6);
    clear.setBounds (b1);
    left.setBounds (b2.removeFromLeft (bw / 2 - 3)); b2.removeFromLeft (6);
    right.setBounds (b2.removeFromLeft (bw / 2 - 3)); b2.removeFromLeft (6);
    dragMidi.setBounds (b2);

    area.removeFromTop (6);
    area.removeFromBottom (10);
    velocity.setBounds (area.removeFromBottom (54));
    area.removeFromBottom (6);
    grid.setBounds (area);
}

//==============================================================================
ChordDisplay::ChordDisplay (APVTS& s) : state (s)
{
    startTimerHz (15);
}

void ChordDisplay::timerCallback()
{
    auto chord = juce::roundToInt (getParameter (state, ParamIDs::chordMode));
    auto root = juce::roundToInt (getParameter (state, ParamIDs::randRoot));
    auto scale = juce::roundToInt (getParameter (state, ParamIDs::randScale));
    auto snap = getParameter (state, ParamIDs::snapKeys) > 0.5f ? 1 : 0;
    if (chord != lastChord || root != lastRoot || scale != lastScale || snap != lastSnap)
    {
        lastChord = chord; lastRoot = root; lastScale = scale; lastSnap = snap;
        repaint();
    }
}

void ChordDisplay::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);
    auto r = bounds.reduced (10.0f, 8.0f);

    rf::NoteMapper::Settings s;
    s.chord = juce::jmax (0, lastChord);
    s.root = juce::jmax (0, lastRoot);
    s.scale = juce::jmax (0, lastScale);
    s.snap = lastSnap == 1;
    auto played = 60 + s.root;
    auto notes = rf::NoteMapper::notesFor (played, s);

    juce::StringArray names;
    for (auto n : notes)
        names.add (noteName (n));

    auto top = r.removeFromTop (16.0f);
    g.setFont (Fonts::label (10.5f));
    g.setColour (Palette::dimText);
    g.drawText (noteName (played) + " plays", top, juce::Justification::centredLeft);
    g.setFont (Fonts::mono (10.0f));
    g.setColour (Palette::text);
    g.drawText (names.joinIntoString ("  "), top, juce::Justification::centredRight);
    r.removeFromTop (6.0f);

    // Three octaves from C2, with the scale's notes marked along the bottom.
    const int lo = 48, hi = 84;
    int whiteCount = 0;
    for (int n = lo; n < hi; ++n)
        if (! juce::MidiMessage::isMidiNoteBlack (n)) ++whiteCount;
    auto keyW = r.getWidth() / (float) whiteCount;
    const auto& scale = rf::Scales::get (s.scale);
    auto inScale = [&] (int n)
    {
        auto rel = ((n - s.root) % 12 + 12) % 12;
        for (int i = 0; i < scale.size; ++i)
            if (scale.steps[(size_t) i] == rel) return true;
        return false;
    };
    auto isOn = [&] (int n) { return std::find (notes.begin(), notes.end(), n) != notes.end(); };

    std::map<int, juce::Rectangle<float>> whiteRects;
    int wi = 0;
    for (int n = lo; n < hi; ++n)
    {
        if (juce::MidiMessage::isMidiNoteBlack (n))
            continue;
        auto key = juce::Rectangle<float> (r.getX() + (float) wi * keyW, r.getY(), keyW, r.getHeight()).reduced (0.75f, 0.0f);
        whiteRects[n] = key;
        g.setColour (isOn (n) ? Palette::accent : juce::Colour (0xff2c2c31));
        g.fillRoundedRectangle (key, 2.0f);
        if (inScale (n) && ! isOn (n))
        {
            g.setColour (Palette::dimText);
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ key.getCentreX(), key.getBottom() - 6.0f }));
        }
        ++wi;
    }
    for (int n = lo; n < hi; ++n)
    {
        if (! juce::MidiMessage::isMidiNoteBlack (n))
            continue;
        auto& left = whiteRects[n - 1];
        auto key = juce::Rectangle<float> (left.getRight() - keyW * 0.3f, r.getY(), keyW * 0.6f, r.getHeight() * 0.6f);
        g.setColour (isOn (n) ? Palette::accent : juce::Colour (0xff111113));
        g.fillRoundedRectangle (key, 2.0f);
        if (inScale (n) && ! isOn (n))
        {
            g.setColour (Palette::dimText);
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ key.getCentreX(), key.getBottom() - 5.0f }));
        }
    }
}

} // namespace ui
