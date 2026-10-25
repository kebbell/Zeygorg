#include "LayerPanel.h"

namespace ui
{
namespace
{
constexpr int sampleWave = 6;
const juce::StringArray noteNames { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::String noteName (int midi)
{
    return noteNames[midi % 12] + juce::String (midi / 12 - 2); // 60 = C3, as in Ableton
}
}

//==============================================================================
WaveformView::WaveformView (ZeygorgProcessor& p, int l) : processor (p), layer (l)
{
    setTooltip ("Drag the edges to set start and end. Drop an audio file on the layer to load it.");
}

void WaveformView::update()
{
    auto& s = processor.apvts;
    auto get = [&] (LayerParam::Field f) { return s.getRawParameterValue (LayerParam::id (layer, f))->load(); };

    auto newSample = processor.synth.getSample (layer);
    auto changed = newSample != sample;
    sample = newSample;
    if (changed)
        rebuildPeaks();

    auto newStart = get (LayerParam::start), newEnd = get (LayerParam::end);
    auto newPlay = processor.synth.playhead[(size_t) layer].load();
    auto newWave = juce::roundToInt (get (LayerParam::wave));
    auto newUni = juce::roundToInt (get (LayerParam::unison));
    auto newDet = get (LayerParam::detune);
    auto newLoop = get (LayerParam::loop) > 0.5f, newRev = get (LayerParam::reverse) > 0.5f;
    auto newMissing = processor.isLayerFileMissing (layer);

    auto moved = [] (float a, float b) { return std::abs (a - b) > 0.0005f; };
    if (changed || moved (newStart, start) || moved (newEnd, end) || moved (newPlay, playhead) || newWave != wave
        || newUni != unison || moved (newDet, detune) || newLoop != loop || newRev != reverse || newMissing != missing)
    {
        start = newStart; end = newEnd; playhead = newPlay; wave = newWave;
        unison = newUni; detune = newDet; loop = newLoop; reverse = newRev; missing = newMissing;
        repaint();
    }
}

void WaveformView::rebuildPeaks()
{
    peaks.clear();
    if (sample == nullptr)
        return;

    const auto& audio = sample->audio;
    const int columns = 600;
    auto len = audio.getNumSamples();
    peaks.resize (columns);
    for (int c = 0; c < columns; ++c)
    {
        auto a = (int) ((juce::int64) c * len / columns);
        auto b = juce::jmax (a + 1, (int) ((juce::int64) (c + 1) * len / columns));
        float lo = 0.0f, hi = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (audio.getReadPointer (ch, a), juce::jmin (b, len) - a);
            lo = juce::jmin (lo, range.getStart());
            hi = juce::jmax (hi, range.getEnd());
        }
        peaks[(size_t) c] = { lo, hi };
    }

    // Normalise the drawing so quiet samples are still readable.
    float peak = 0.01f;
    for (auto& p : peaks)
        peak = juce::jmax (peak, std::abs (p.first), std::abs (p.second));
    for (auto& p : peaks)
        p = { p.first / peak, p.second / peak };
}

juce::Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (8.0f, 6.0f).withTrimmedTop (14.0f);
}

float WaveformView::xFor (float norm) const
{
    auto r = waveArea();
    return r.getX() + norm * r.getWidth();
}

float WaveformView::normFor (float x) const
{
    auto r = waveArea();
    return juce::jlimit (0.0f, 1.0f, (x - r.getX()) / r.getWidth());
}

int WaveformView::handleAt (juce::Point<float> p) const
{
    if (wave != sampleWave || sample == nullptr)
        return -1;
    auto ds = std::abs (p.x - xFor (start)), de = std::abs (p.x - xFor (end));
    if (juce::jmin (ds, de) > 8.0f)
        return -1;
    return ds <= de ? 0 : 1;
}

void WaveformView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    paintWell (g, bounds);

    if (wave == sampleWave)
        paintSample (g, waveArea());
    else
        paintOscillator (g, waveArea(), wave);

    if (dropHighlight)
    {
        g.setColour (Palette::accent.withAlpha (0.08f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Palette::accent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 1.5f);
        g.setFont (Fonts::title (12.0f));
        g.drawText ("DROP TO LOAD", bounds, juce::Justification::centred);
    }
}

void WaveformView::paintSample (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto top = getLocalBounds().toFloat().reduced (10.0f, 4.0f).removeFromTop (14.0f);
    g.setFont (Fonts::label (10.5f));

    if (sample == nullptr || peaks.empty())
    {
        g.setColour (Palette::dimText);
        g.drawText ("No sample", r, juce::Justification::centred);
        return;
    }

    g.setColour (missing ? juce::Colour (0xffb0b0b0) : Palette::dimText);
    auto title = missing ? "File missing - using " + sample->name : sample->name;
    g.drawText (title, top.withTrimmedRight (70.0f), juce::Justification::centredLeft, true);
    auto seconds = sample->audio.getNumSamples() / juce::jmax (1.0, sample->sampleRate);
    g.setFont (Fonts::mono (9.5f));
    g.drawText (noteName (sample->rootNote) + "  " + juce::String (seconds, 2) + "s", top, juce::Justification::centredRight);

    auto a = juce::jmin (start, end), b = juce::jmax (start, end);
    auto mid = r.getCentreY();
    auto half = r.getHeight() * 0.5f;
    auto columns = (int) peaks.size();

    // Waveform: bright inside the region, dim outside.
    for (int x = 0; x < (int) r.getWidth(); ++x)
    {
        auto norm = (float) x / r.getWidth();
        const auto& p = peaks[(size_t) juce::jmin (columns - 1, (int) (norm * (float) columns))];
        auto inside = norm >= a && norm <= b;
        g.setColour (inside ? Palette::accent.withAlpha (0.78f) : Palette::faint);
        auto y0 = mid - p.second * half, y1 = mid - p.first * half;
        g.fillRect (r.getX() + (float) x, y0, 1.0f, juce::jmax (1.0f, y1 - y0));
    }

    g.setColour (Palette::panelEdge);
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());

    // Region shading and handles.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (r.withRight (xFor (a)));
    g.fillRect (r.withLeft (xFor (b)));

    for (int h = 0; h < 2; ++h)
    {
        auto x = xFor (h == 0 ? start : end);
        auto active = hover == h || dragging == h;
        g.setColour (active ? Palette::accent : Palette::accent2);
        g.fillRect (x - 0.75f, r.getY(), 1.5f, r.getHeight());
        juce::Path tab;
        auto dir = h == 0 ? 1.0f : -1.0f;
        tab.addTriangle (x, r.getY(), x + dir * 7.0f, r.getY(), x, r.getY() + 7.0f);
        g.fillPath (tab);
    }

    if (loop || reverse)
    {
        g.setFont (Fonts::label (9.0f));
        g.setColour (Palette::accent2);
        juce::String tags = juce::String (loop ? "LOOP " : "") + (reverse ? "REV" : "");
        g.drawText (tags.trim(), r.reduced (4.0f, 2.0f), juce::Justification::bottomRight);
    }

    if (playhead >= 0.0f)
    {
        auto x = xFor (a + playhead * (b - a));
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.fillRect (x - 2.0f, r.getY(), 4.0f, r.getHeight());
        g.setColour (Palette::accent);
        g.fillRect (x - 0.5f, r.getY(), 1.0f, r.getHeight());
    }
}

void WaveformView::paintOscillator (juce::Graphics& g, juce::Rectangle<float> r, int w)
{
    auto top = getLocalBounds().toFloat().reduced (10.0f, 4.0f).removeFromTop (14.0f);
    g.setFont (Fonts::label (10.5f));
    g.setColour (Palette::dimText);
    auto voices = w == 0 ? juce::jmax (7, unison) : w == 5 ? 1 : unison;
    g.drawText (Choices::waves[w] + (voices > 1 ? "  /  " + juce::String (voices) + " voices" : juce::String()),
                top, juce::Justification::centredLeft);

    g.setColour (Palette::panelEdge);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    auto shape = [w] (float ph, juce::Random& rng)
    {
        ph -= std::floor (ph);
        switch (w)
        {
            case 2:  return ph < 0.5f ? 0.8f : -0.8f;
            case 3:  return 4.0f * std::abs (ph - 0.5f) - 1.0f;
            case 4:  return std::sin (juce::MathConstants<float>::twoPi * ph);
            case 5:  return rng.nextFloat() * 2.0f - 1.0f;
            default: return 2.0f * ph - 1.0f;
        }
    };

    // The unison stack drawn as ghost copies, drifting apart with the detune.
    juce::Random rng (7);
    auto copies = juce::jmin (voices, 7);
    for (int c = copies; --c >= 0;)
    {
        auto t = copies > 1 ? 2.0f * (float) c / (float) (copies - 1) - 1.0f : 0.0f;
        auto offset = t * detune / 100.0f * 0.25f;
        juce::Path p;
        for (int x = 0; x <= (int) r.getWidth(); ++x)
        {
            auto ph = (float) x / r.getWidth() * 2.0f * (1.0f + offset * 0.2f) + offset;
            auto y = r.getCentreY() - shape (ph, rng) * r.getHeight() * 0.42f;
            if (x == 0) p.startNewSubPath (r.getX(), y);
            else        p.lineTo (r.getX() + (float) x, y);
        }
        auto centre = c == copies / 2;
        g.setColour (Palette::accent.withAlpha (centre ? 0.9f : 0.22f));
        g.strokePath (p, juce::PathStrokeType (centre ? 1.5f : 1.0f));
    }
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    auto h = handleAt (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragging = handleAt (e.position);
    if (dragging >= 0)
        if (auto* p = processor.apvts.getParameter (LayerParam::id (layer, dragging == 0 ? LayerParam::start : LayerParam::end)))
            p->beginChangeGesture();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    if (auto* p = processor.apvts.getParameter (LayerParam::id (layer, dragging == 0 ? LayerParam::start : LayerParam::end)))
        p->setValueNotifyingHost (normFor (e.position.x));
    update();
}

void WaveformView::mouseUp (const juce::MouseEvent& e)
{
    if (dragging >= 0)
    {
        if (auto* p = processor.apvts.getParameter (LayerParam::id (layer, dragging == 0 ? LayerParam::start : LayerParam::end)))
            p->endChangeGesture();
    }
    else if (e.mouseWasClicked() && onEmptyClick != nullptr)
    {
        onEmptyClick();
    }
    dragging = -1;
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    // Reset the region.
    setParameter (processor.apvts, LayerParam::id (layer, LayerParam::start), 0.0f);
    setParameter (processor.apvts, LayerParam::id (layer, LayerParam::end), 1.0f);
}

//==============================================================================
LayerPanel::LayerPanel (ZeygorgProcessor& p, int l)
    : processor (p), state (p.apvts), layer (l),
      powerAttachment (p.apvts, LayerParam::id (l, LayerParam::on), power),
      view (p, l),
      level (state, param (LayerParam::level), "Level"),
      pan (state, param (LayerParam::pan), "Pan"),
      octave (state, param (LayerParam::octave), "Octave"),
      semi (state, param (LayerParam::semi), "Semi"),
      fine (state, param (LayerParam::fine), "Fine"),
      unison (state, param (LayerParam::unison), "Unison"),
      detune (state, param (LayerParam::detune), "Detune"),
      spread (state, param (LayerParam::spread), "Spread"),
      speed (state, param (LayerParam::speed), "Speed"),
      grain (state, param (LayerParam::grain), "Grain"),
      warp (state, param (LayerParam::warp), "Warp"),
      sync (state, param (LayerParam::sync), "Length"),
      loop (state, param (LayerParam::loop), "Loop"),
      reverse (state, param (LayerParam::reverse), "Rev")
{
    power.setTooltip ("Layer " + juce::String (l + 1) + " on/off");
    power.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (power);

    waveBox.addItemList (Choices::waves, 1);
    waveAttachment = std::make_unique<APVTS::ComboBoxAttachment> (state, param (LayerParam::wave), waveBox);
    waveBox.setTooltip ("Oscillator or sample");
    addAndMakeVisible (waveBox);

    view.onEmptyClick = [this]
    {
        if (juce::roundToInt (getParameter (state, param (LayerParam::wave))) == sampleWave)
            showBrowser();
    };
    addAndMakeVisible (view);

    for (auto* b : { &prev, &next, &browse, &load })
        addAndMakeVisible (b);
    prev.onClick = [this] { stepSample (-1); };
    next.onClick = [this] { stepSample (1); };
    browse.onClick = [this] { showBrowser(); };
    load.onClick = [this] { chooseFile(); };

    for (auto* c : std::initializer_list<juce::Component*> { &level, &pan, &octave, &semi, &fine, &unison, &detune, &spread,
                                                             &warp, &speed, &sync, &grain, &loop, &reverse })
        addAndMakeVisible (c);

    timerCallback();
    startTimerHz (30);
}

void LayerPanel::paint (juce::Graphics& g)
{
    paintPanel (g, getLocalBounds().toFloat());

    auto header = getLocalBounds().reduced (10, 0).removeFromTop (34);
    header.removeFromLeft (26);
    g.setFont (Fonts::title (11.0f));
    g.setColour (power.getToggleState() ? Palette::text : Palette::dimText);
    g.drawText ("LAYER " + juce::String (layer + 1), header, juce::Justification::centredLeft);

    if (dragOver)
    {
        g.setColour (Palette::accent.withAlpha (0.7f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 9.0f, 1.5f);
    }
}

void LayerPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 0);
    auto header = area.removeFromTop (34);
    power.setBounds (header.removeFromLeft (20).withSizeKeepingCentre (20, 16));
    waveBox.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 22));

    view.setBounds (area.removeFromTop (100));
    area.removeFromTop (6);

    auto row = area.removeFromTop (24);
    load.setBounds (row.removeFromRight (52));
    row.removeFromRight (6);
    prev.setBounds (row.removeFromLeft (26));
    next.setBounds (row.removeFromRight (26));
    browse.setBounds (row.reduced (4, 0));
    area.removeFromTop (6);
    area.removeFromBottom (6);

    auto rowH = area.getHeight() / 2;
    auto cellW = area.getWidth() / 6;
    auto cell = [&] (int col, int r) { return juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + r * rowH, cellW, rowH); };

    juce::Component* top[] { &level, &pan, &octave, &semi, &fine, &unison };
    for (int i = 0; i < 6; ++i)
        top[i]->setBounds (cell (i, 0));

    detune.setBounds (cell (0, 1));
    spread.setBounds (cell (1, 1));
    // Warp gets a wider cell for its menu; speed (or length) and grain share the rest.
    auto sampleCells = cell (2, 1).withWidth (cellW * 3);
    warp.setBounds (sampleCells.removeFromLeft (sampleCells.getWidth() * 2 / 5));
    speed.setBounds (sampleCells.removeFromLeft (sampleCells.getWidth() / 2));
    sync.setBounds (speed.getBounds());
    grain.setBounds (sampleCells);

    auto toggles = cell (5, 1).reduced (4, 0);
    toggles.removeFromTop (14);
    auto th = juce::jmin (22, (toggles.getHeight() - 6) / 2);
    loop.setBounds (toggles.removeFromTop (th));
    toggles.removeFromTop (6);
    reverse.setBounds (toggles.removeFromTop (th));
}

void LayerPanel::timerCallback()
{
    view.update();

    auto isSample = juce::roundToInt (getParameter (state, param (LayerParam::wave))) == sampleWave;
    auto warpMode = juce::roundToInt (getParameter (state, param (LayerParam::warp)));
    auto wave = juce::roundToInt (getParameter (state, param (LayerParam::wave)));
    auto stacked = juce::roundToInt (getParameter (state, param (LayerParam::unison))) > 1 || wave == 0;
    auto on = power.getToggleState();

    for (auto* c : std::initializer_list<juce::Component*> { &prev, &next, &browse, &load, &warp, &loop, &reverse })
        c->setEnabled (isSample);
    speed.setVisible (warpMode != 2);
    sync.setVisible (warpMode == 2);
    speed.setEnabled (isSample && warpMode == 1);
    sync.setEnabled (isSample);
    grain.setEnabled (isSample && warpMode != 0);
    detune.setEnabled (stacked);
    spread.setEnabled (stacked);

    for (auto* c : getChildren())
        if (c != &power)
            c->setAlpha (on ? 1.0f : 0.5f);

    // Sample name on the browse button.
    juce::String name;
    if (isSample)
    {
        auto file = processor.getLayerFile (layer);
        if (file.isNotEmpty())
            name = juce::File (file).getFileNameWithoutExtension();
        else
        {
            const auto& e = SampleLibrary::get()[juce::roundToInt (getParameter (state, param (LayerParam::sample)))];
            name = e.category + "  /  " + e.name;
        }
    }
    else
    {
        name = "Oscillator";
    }
    if (name != shownName)
    {
        shownName = name;
        browse.setButtonText (name + "  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbe")));
    }
}

void LayerPanel::selectLibrary (int index)
{
    auto count = SampleLibrary::get().size();
    index = ((index % count) + count) % count;
    processor.clearLayerFile (layer);
    setParameter (state, param (LayerParam::sample), (float) index);
    processor.syncSamples();
}

void LayerPanel::stepSample (int delta)
{
    selectLibrary (juce::roundToInt (getParameter (state, param (LayerParam::sample))) + delta);
}

void LayerPanel::showBrowser()
{
    const auto& lib = SampleLibrary::get();
    auto current = juce::roundToInt (getParameter (state, param (LayerParam::sample)));
    auto usingFile = processor.getLayerFile (layer).isNotEmpty();

    juce::PopupMenu menu;
    menu.addSectionHeader ("Built-in");
    for (auto& category : lib.categories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < lib.size(); ++i)
            if (lib[i].category == category)
                sub.addItem (i + 1, lib[i].name, true, ! usingFile && i == current);
        menu.addSubMenu (category, sub, true, nullptr, ! usingFile && lib[current].category == category);
    }
    menu.addSeparator();
    menu.addItem (-1, "Load audio file...");
    if (usingFile)
        menu.addItem (-2, "Clear " + juce::File (processor.getLayerFile (layer)).getFileName());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&browse).withMinimumWidth (browse.getWidth()),
                        [safe = juce::Component::SafePointer<LayerPanel> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == -1) safe->chooseFile();
                            else if (result == -2) { safe->processor.clearLayerFile (safe->layer); }
                            else safe->selectLibrary (result - 1);
                        });
}

void LayerPanel::chooseFile()
{
    auto startDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    auto current = processor.getLayerFile (layer);
    if (current.isNotEmpty())
        startDir = juce::File (current).getParentDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Load a sample into layer " + juce::String (layer + 1), startDir,
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<LayerPanel> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe != nullptr && fc.getResult().existsAsFile())
                                  safe->loadFile (fc.getResult());
                          });
}

void LayerPanel::loadFile (const juce::File& file)
{
    processor.setLayerFile (layer, file);
    // Dropping a file means "play this": switch the layer on and to Sample.
    setParameter (state, param (LayerParam::on), 1.0f);
    setParameter (state, param (LayerParam::wave), (float) sampleWave);
}

bool LayerPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (SampleLibrary::canLoad (f))
            return true;
    return false;
}

void LayerPanel::fileDragEnter (const juce::StringArray&, int, int)
{
    dragOver = true;
    view.setDropHighlight (true);
    repaint();
}

void LayerPanel::fileDragExit (const juce::StringArray&)
{
    dragOver = false;
    view.setDropHighlight (false);
    repaint();
}

void LayerPanel::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragExit (files);
    for (auto& f : files)
        if (SampleLibrary::canLoad (f))
        {
            loadFile (juce::File (f));
            break;
        }
}

} // namespace ui
