#include "Pages.h"

namespace ui
{
namespace
{
constexpr int gap = 10;

// Lays sections out left to right, sharing the width by their column counts.
void layoutRow (juce::Rectangle<int> row, std::initializer_list<std::pair<juce::Component*, int>> items)
{
    int total = 0;
    for (auto& [c, units] : items)
        total += units;

    auto unit = (float) (row.getWidth() - gap * ((int) items.size() - 1)) / (float) total;
    auto i = 0;
    for (auto& [c, units] : items)
    {
        auto w = ++i == (int) items.size() ? row.getWidth() : juce::roundToInt (unit * (float) units);
        c->setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gap);
    }
}
}

//==============================================================================
GateDisplay::GateDisplay (ZeygorgProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void GateDisplay::timerCallback()
{
    auto& s = processor.apvts;
    auto newPattern = juce::roundToInt (getParameter (s, ParamIDs::gatePattern));
    auto newStep = processor.engine.currentGateStep.load();
    auto on = getParameter (s, ParamIDs::gateOn) > 0.5f;
    if (newPattern != pattern || newStep != current || on != enabled)
    {
        pattern = newPattern;
        current = newStep;
        enabled = on;
        repaint();
    }
}

void GateDisplay::paint (juce::Graphics& g)
{
    const auto& steps = rf::TranceGate::pattern (juce::jmax (0, pattern));
    auto r = getLocalBounds().toFloat();
    auto stepGap = 3.0f;
    auto w = (r.getWidth() - stepGap * 15.0f) / 16.0f;

    for (int i = 0; i < 16; ++i)
    {
        juce::Rectangle<float> cell (r.getX() + (float) i * (w + stepGap), r.getY(), w, r.getHeight());
        auto active = steps[(size_t) i] != 0;
        auto playing = enabled && i == current;
        g.setColour (active ? Palette::accent.withAlpha (playing ? 1.0f : 0.7f) : Palette::track);
        g.fillRoundedRectangle (cell, 2.5f);
        if (playing && ! active)
        {
            g.setColour (Palette::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (cell.reduced (0.5f), 2.5f, 1.0f);
        }
        if (i % 4 == 0)
        {
            g.setColour (Palette::faint);
            g.fillRect (cell.getX(), cell.getBottom() + 3.0f, w, 1.5f);
        }
    }
}

//==============================================================================
NoteFlow::NoteFlow (APVTS& s) : state (s)
{
    startTimerHz (10);
}

void NoteFlow::timerCallback()
{
    auto mask = (getParameter (state, ParamIDs::randOn) > 0.5f ? 1 : 0)
              | (getParameter (state, ParamIDs::snapKeys) > 0.5f ? 2 : 0)
              | (juce::roundToInt (getParameter (state, ParamIDs::chordMode)) << 2)
              | (getParameter (state, ParamIDs::arpOn) > 0.5f ? 1 << 8 : 0)
              | (juce::roundToInt (getParameter (state, ParamIDs::arpMode)) << 9);
    if (mask != lastMask)
    {
        lastMask = mask;
        repaint();
    }
}

void NoteFlow::paint (juce::Graphics& g)
{
    auto chord = juce::roundToInt (getParameter (state, ParamIDs::chordMode));
    auto arpMode = juce::roundToInt (getParameter (state, ParamIDs::arpMode));

    struct Stage { juce::String name, detail; bool on; };
    Stage stages[] {
        { "Keys", "& MIDI in", true },
        { "Sequencer", "pattern", getParameter (state, ParamIDs::randOn) > 0.5f },
        { "Snap", "to key", getParameter (state, ParamIDs::snapKeys) > 0.5f },
        { "Chord", chord > 0 ? rf::NoteMapper::chordNames()[chord] : "off", chord > 0 },
        { "Arp", rf::Arp::modeNames()[arpMode], getParameter (state, ParamIDs::arpOn) > 0.5f },
        { "Synth", "3 layers", true },
    };

    auto r = getLocalBounds().toFloat();
    auto n = (int) std::size (stages);
    auto arrowW = 12.0f;
    auto boxW = (r.getWidth() - arrowW * (float) (n - 1)) / (float) n;

    for (int i = 0; i < n; ++i)
    {
        auto box = juce::Rectangle<float> (r.getX() + (float) i * (boxW + arrowW), r.getY(), boxW, r.getHeight()).reduced (0.0f, 4.0f);
        const auto& st = stages[i];
        g.setColour (st.on ? Palette::accent : Palette::control);
        g.fillRoundedRectangle (box, 6.0f);
        if (! st.on)
        {
            g.setColour (Palette::controlEdge);
            g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
        }
        auto text = box.reduced (4.0f, 6.0f);
        g.setFont (Fonts::title (10.0f));
        g.setColour (st.on ? Palette::background : Palette::dimText);
        g.drawFittedText (st.name.toUpperCase(), text.removeFromTop (text.getHeight() * 0.55f).toNearestInt(), juce::Justification::centredBottom, 1);
        g.setFont (Fonts::body (10.5f));
        g.setColour (st.on ? Palette::background.withAlpha (0.7f) : Palette::faint);
        g.drawFittedText (st.detail, text.toNearestInt(), juce::Justification::centredTop, 1);

        if (i < n - 1)
        {
            juce::Path arrow;
            auto ax = box.getRight() + arrowW * 0.5f, ay = box.getCentreY();
            arrow.addTriangle (ax - 3.0f, ay - 4.0f, ax - 3.0f, ay + 4.0f, ax + 3.0f, ay);
            g.setColour (Palette::faint);
            g.fillPath (arrow);
        }
    }
}

//==============================================================================
SynthPage::SynthPage (ZeygorgProcessor& p)
    : amp ("Amp Envelope", 5, 3), filter ("Filter", 3, 2), filterEnv ("Filter Envelope", 4, 3), voice ("Voice", 2, 2),
      ampEditor (p.apvts, ParamIDs::ampAttack, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease),
      filterEditor (p.apvts, ParamIDs::fltAttack, ParamIDs::fltDecay, ParamIDs::fltSustain, ParamIDs::fltRelease)
{
    using namespace ParamIDs;
    auto& s = p.apvts;

    for (int l = 0; l < rf::Synth::numLayers; ++l)
        addAndMakeVisible (layers.add (new LayerPanel (p, l)));

    amp.add<Knob> (s, ampAttack, "Attack");
    amp.add<Knob> (s, ampDecay, "Decay");
    amp.add<Knob> (s, ampSustain, "Sustain");
    amp.add<Knob> (s, ampRelease, "Release");
    amp.add<Knob> (s, velSens, "Velocity");
    amp.setFooter (&ampEditor, 2);

    filter.add<Choice> (s, vFltType, "Type");
    filter.add<Knob> (s, vCutoff, "Cutoff");
    filter.add<Knob> (s, vReso, "Resonance");
    filter.add<Knob> (s, vEnvAmt, "Env Amt");
    filter.add<Knob> (s, vKeyTrack, "Key Track");

    filterEnv.add<Knob> (s, fltAttack, "Attack");
    filterEnv.add<Knob> (s, fltDecay, "Decay");
    filterEnv.add<Knob> (s, fltSustain, "Sustain");
    filterEnv.add<Knob> (s, fltRelease, "Release");
    filterEnv.setFooter (&filterEditor, 2);

    voice.add<Choice> (s, voiceMode, "Mode");
    voice.add<Knob> (s, glide, "Glide");
    voice.add<Knob> (s, synthTranspose, "Transpose");
    voice.add<Knob> (s, synthLevel, "Level");

    for (auto* c : { &amp, &filter, &filterEnv, &voice })
        addAndMakeVisible (c);
}

void SynthPage::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (area.getHeight() * 54 / 100);
    area.removeFromTop (gap);

    layoutRow (top, { { layers[0], 1 }, { layers[1], 1 }, { layers[2], 1 } });
    layoutRow (area, { { &amp, 5 }, { &filter, 3 }, { &filterEnv, 4 }, { &voice, 2 } });
}

//==============================================================================
SequencePage::SequencePage (ZeygorgProcessor& p)
    : sequencer (p), arp ("Arpeggiator", 3, 2), chords ("Chords & Key Lock", 2, 3), flowSection ("Note Flow", 1, 1),
      chordDisplay (p.apvts), flow (p.apvts)
{
    using namespace ParamIDs;
    auto& s = p.apvts;

    arp.setHeaderToggle (s, arpOn);
    arp.add<Choice> (s, arpMode, "Mode");
    arp.add<Choice> (s, arpRate, "Rate");
    arp.add<Toggle> (s, arpLatch, "Latch", "Hold");
    arp.add<Knob> (s, arpOctaves, "Octaves");
    arp.add<Knob> (s, arpGate, "Gate");

    chords.add<Choice> (s, chordMode, "Chord");
    chords.add<Toggle> (s, snapKeys, "Snap", "Snap Keys");
    chords.setFooter (&chordDisplay, 2);

    flowSection.setFooter (&flow, 1);

    for (auto* c : std::initializer_list<juce::Component*> { &sequencer, &arp, &chords, &flowSection })
        addAndMakeVisible (c);
}

void SequencePage::resized()
{
    auto area = getLocalBounds();
    auto right = area.removeFromRight (384);
    area.removeFromRight (gap);
    sequencer.setBounds (area);

    arp.setBounds (right.removeFromTop (220));
    right.removeFromTop (gap);
    flowSection.setBounds (right.removeFromBottom (110));
    right.removeFromBottom (gap);
    chords.setBounds (right);
}

//==============================================================================
FxPage::FxPage (ZeygorgProcessor& p)
    : processor (p),
      boost ("Boost", 3, 2), filter ("Filter", 3, 2), dirt ("Dirt", 3, 2), lfo ("LFO", 4, 2),
      gate ("Trance Gate", 4, 2), delay ("Delay", 3, 2), reverb ("Reverb", 2, 2), master ("Master", 2, 2),
      pump ("Pump", 3, 2), comp ("Compressor", 5, 2), analyserSection ("Analyser", 1, 1),
      gateDisplay (p),
      pumpDisplay (p.apvts, ParamIDs::pumpDepth, ParamIDs::pumpRelease, [&p] { return p.engine.currentPump.load(); }),
      reduction ([&p] { return p.engine.gainReductionDb(); }),
      analyser (p.engine.scope.data(), rf::Engine::scopeSize, p.engine.scopeWrite, [&p] { return p.getSampleRate(); })
{
    using namespace ParamIDs;
    auto& s = p.apvts;

    boost.add<Knob> (s, power, "Power");
    boost.add<Knob> (s, width, "Width");
    boost.add<Knob> (s, detune, "Detune");
    boost.add<Knob> (s, subOct, "Sub Oct");
    boost.add<Knob> (s, fifth, "Fifth");
    boost.add<Knob> (s, octUp, "Oct Up");

    filter.add<Choice> (s, fltType, "Type");
    filter.add<Knob> (s, cutoff, "Cutoff");
    filter.add<Knob> (s, resonance, "Resonance");
    filter.add<Knob> (s, envAmount, "Env Amt");
    filter.add<Knob> (s, envAttack, "Attack");
    filter.add<Knob> (s, envRelease, "Release");

    dirt.add<Choice> (s, shape, "Shape");
    dirt.add<Knob> (s, drive, "Drive");
    dirt.add<Knob> (s, dirtMix, "Mix");
    dirt.add<Knob> (s, bits, "Bits");
    dirt.add<Knob> (s, downsample, "Downsample");
    dirt.add<Choice> (s, dirtOS, "Oversample");

    lfo.add<Choice> (s, lfoShape, "Shape");
    lfo.add<Toggle> (s, lfoSync, "Sync", "Tempo");
    lfoDivChoice = &lfo.add<Choice> (s, lfoDiv, "Division");
    lfoRateKnob = &lfo.add<Knob> (s, lfoRate, "Rate");
    lfo.add<Knob> (s, lfoCutoff, "> Cutoff");
    lfo.add<Knob> (s, lfoDetune, "> Detune");
    lfo.add<Knob> (s, lfoAmp, "> Volume");

    gate.setHeaderToggle (s, gateOn);
    gate.add<Choice> (s, gatePattern, "Pattern");
    gate.add<Choice> (s, gateRate, "Rate");
    gate.add<Knob> (s, gateDepth, "Depth");
    gate.add<Knob> (s, gateSmooth, "Smooth");
    gate.setFooter (&gateDisplay);

    delay.add<Choice> (s, dlyDiv, "Time");
    delay.add<Toggle> (s, dlyPingPong, "Ping Pong", "Stereo");
    delay.add<Knob> (s, dlyFeedback, "Feedback");
    delay.add<Knob> (s, dlyTone, "Tone");
    delay.add<Knob> (s, dlyMix, "Mix");

    reverb.add<Knob> (s, revSize, "Size");
    reverb.add<Knob> (s, revDamp, "Damping");
    reverb.add<Knob> (s, revWidth, "Width");
    reverb.add<Knob> (s, revMix, "Mix");

    master.add<Knob> (s, ParamIDs::mix, "Dry/Wet");
    master.add<Knob> (s, output, "Output");
    master.add<Toggle> (s, limitOn, "Limit", "Limiter");

    pump.setHeaderToggle (s, pumpOn);
    pump.add<Choice> (s, pumpRate, "Rate");
    pump.add<Knob> (s, pumpDepth, "Depth");
    pump.add<Knob> (s, pumpRelease, "Release");
    pump.setFooter (&pumpDisplay);

    comp.setHeaderToggle (s, compOn);
    comp.add<Knob> (s, compThresh, "Threshold");
    comp.add<Knob> (s, compRatio, "Ratio");
    comp.add<Knob> (s, compAttack, "Attack");
    comp.add<Knob> (s, compRelease, "Release");
    comp.add<Knob> (s, compMakeup, "Makeup");
    comp.setFooter (&reduction);

    analyser.setMode (Visualiser::Mode::both);
    analyser.setClickToSwitch (false);
    analyser.setTooltip ("Output scope and spectrum");
    analyserSection.setFooter (&analyser, 1);

    for (auto* c : { &boost, &filter, &dirt, &lfo, &gate, &delay, &reverb, &master, &pump, &comp, &analyserSection })
        addAndMakeVisible (c);

    timerCallback();
    startTimerHz (10);
}

void FxPage::timerCallback()
{
    auto synced = getParameter (processor.apvts, ParamIDs::lfoSync) > 0.5f;
    lfoRateKnob->setEnabled (! synced);
    lfoDivChoice->setEnabled (synced);
}

void FxPage::resized()
{
    auto area = getLocalBounds();
    auto rowH = (area.getHeight() - 2 * gap) / 3;
    auto row1 = area.removeFromTop (rowH);
    area.removeFromTop (gap);
    auto row2 = area.removeFromTop (rowH);
    area.removeFromTop (gap);

    layoutRow (row1, { { &boost, 3 }, { &filter, 3 }, { &dirt, 3 }, { &lfo, 4 } });
    layoutRow (row2, { { &gate, 4 }, { &delay, 3 }, { &reverb, 2 }, { &master, 2 } });
    layoutRow (area, { { &pump, 3 }, { &comp, 5 }, { &analyserSection, 5 } });
}

} // namespace ui
