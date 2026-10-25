#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

namespace
{
juce::String defaultPattern()
{
    rf::Pattern p;
    p.generate (1, 0.7f, rf::Scales::degreeCount (1, 1));
    return p.toString();
}
}

ZeygorgProcessor::ZeygorgProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Zeygorg", createParameterLayout()),
      presets (apvts, FactoryPresets::all (defaultPattern()), "Kebbell/Zeygorg", ".zgpreset",
               [this] { return captureState(); },
               [this] (const juce::ValueTree& t) { restoreState (t); })
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            paramIDs.add (ranged->getParameterID());

    for (auto& id : paramIDs)
        params[std::string_view (id.toRawUTF8())] = apvts.getRawParameterValue (id);

    pattern.fromString (defaultPattern());
    syncSamples();
    startTimerHz (20);
}

//==============================================================================
void ZeygorgProcessor::setLayerFile (int layer, const juce::File& file)
{
    apvts.state.setProperty (layerFileKey (layer), file.getFullPathName(), nullptr);
    syncSamples();
}

void ZeygorgProcessor::clearLayerFile (int layer)
{
    apvts.state.setProperty (layerFileKey (layer), juce::String(), nullptr);
    syncSamples();
}

juce::String ZeygorgProcessor::getLayerFile (int layer) const
{
    return apvts.state.getProperty (layerFileKey (layer)).toString();
}

void ZeygorgProcessor::syncSamples()
{
    for (int l = 0; l < rf::Synth::numLayers; ++l)
    {
        auto file = getLayerFile (l);
        auto index = getChoice (LayerParam::id (l, LayerParam::sample));
        auto key = file.isNotEmpty() ? "file:" + file : "lib:" + juce::String (index);
        if (key == loadedKey[(size_t) l])
            continue;

        rf::Sample::Ptr sample;
        fileMissing[(size_t) l] = false;
        if (file.isNotEmpty())
        {
            sample = SampleLibrary::loadFile (juce::File (file));
            fileMissing[(size_t) l] = sample == nullptr;
        }
        if (sample == nullptr)
            sample = SampleLibrary::get().load (index);

        synth.setSample (l, sample);
        loadedKey[(size_t) l] = key;
    }
    synth.collectGarbage();
}

//==============================================================================
juce::ValueTree ZeygorgProcessor::captureState()
{
    apvts.state.setProperty ("pattern", pattern.toString(), nullptr);
    return apvts.copyState();
}

void ZeygorgProcessor::restoreState (const juce::ValueTree& tree)
{
    if (! tree.hasType (apvts.state.getType()))
        return;
    apvts.replaceState (tree.createCopy());

    auto text = apvts.state.getProperty ("pattern").toString();
    pattern.fromString (text.isNotEmpty() ? text : defaultPattern());
    syncSamples();
}

juce::File ZeygorgProcessor::writePatternMidi() const
{
    using namespace ParamIDs;
    auto rateIndex = juce::jlimit (0, Choices::randRates.size() - 1, getChoice (randRate));
    auto stepBeats = Choices::randRateBeats[rateIndex];
    auto steps = juce::jlimit (1, rf::Pattern::maxSteps, getChoice (seqSteps));
    auto root = getChoice (randRoot), scale = getChoice (randScale);
    auto degrees = rf::Scales::degreeCount (scale, getChoice (randOctaves));
    auto length = (double) get (randLength);
    auto swing = stepBeats * 0.5 * (double) get (seqSwing);

    constexpr int ticksPerBeat = 960;
    juce::MidiMessageSequence seq;
    for (int i = 0; i < steps; ++i)
    {
        auto st = pattern.get (i);
        if (st.isRest())
            continue;
        auto note = rf::Scales::noteForDegree (juce::jmin (st.degree, degrees - 1), root, scale);
        auto on = (i * stepBeats + ((i & 1) != 0 ? swing : 0.0)) * ticksPerBeat;
        auto off = on + stepBeats * juce::jlimit (0.05, 1.0, length) * ticksPerBeat;
        seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) st.velocity), on);
        seq.addEvent (juce::MidiMessage::noteOff (1, note), off);
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), steps * stepBeats * ticksPerBeat);
    seq.updateMatchedPairs();

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (ticksPerBeat);
    midi.addTrack (seq);

    auto name = "Zeygorg " + Choices::roots[root] + " " + rf::Scales::get (scale).name + " Pattern.mid";
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name);
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (out.openedOk())
        midi.writeTo (out);
    return file;
}

//==============================================================================
bool ZeygorgProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ZeygorgProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    synth.prepare (sampleRate, samplesPerBlock);
    sequencer.reset();
    mapper.reset();
    arp.reset();
    scratchMidi.ensureSize (8192);
}

void ZeygorgProcessor::reset()
{
    engine.reset();
    synth.allOff (true);
    sequencer.reset();
    mapper.reset();
    arp.reset();
}

rf::Engine::Settings ZeygorgProcessor::readSettings() const
{
    using namespace ParamIDs;
    rf::Engine::Settings s;

    s.stack.power = get (power);
    s.stack.width = get (width);
    s.stack.detuneCents = get (detune);
    s.stack.sub = get (subOct);
    s.stack.fifth = get (fifth);
    s.stack.octUp = get (octUp);

    s.filterType = getChoice (fltType);
    s.cutoff = get (cutoff);
    s.resonance = get (resonance);
    s.envAmount = get (envAmount);
    s.envAttackMs = get (envAttack);
    s.envReleaseMs = get (envRelease);

    s.dirt.driveDb = get (drive);
    s.dirt.shape = (rf::Dirt::Shape) getChoice (shape);
    s.dirt.bits = get (bits);
    s.dirt.downsample = get (downsample);
    s.dirt.mix = get (dirtMix);
    s.dirt.oversample = getChoice (dirtOS);

    s.lfoShape = (rf::Lfo::Shape) getChoice (lfoShape);
    s.lfoSync = get (lfoSync) > 0.5f;
    s.lfoRateHz = get (lfoRate);
    s.lfoBeats = Choices::lfoDivBeats[juce::jlimit (0, Choices::lfoDivs.size() - 1, getChoice (lfoDiv))];
    s.lfoToCutoff = get (lfoCutoff);
    s.lfoToDetune = get (lfoDetune);
    s.lfoToAmp = get (lfoAmp);

    s.gateOn = get (gateOn) > 0.5f;
    s.gatePattern = getChoice (gatePattern);
    s.gateStepBeats = Choices::gateRateBeats[juce::jlimit (0, Choices::gateRates.size() - 1, getChoice (gateRate))];
    s.gateDepth = get (gateDepth);
    s.gateSmooth = get (gateSmooth);

    s.delayBeats = Choices::delayDivBeats[juce::jlimit (0, Choices::delayDivs.size() - 1, getChoice (dlyDiv))];
    s.delayFeedback = get (dlyFeedback);
    s.delayPingPong = get (dlyPingPong) > 0.5f;
    s.delayTone = get (dlyTone);
    s.delayMix = get (dlyMix);

    s.revSize = get (revSize);
    s.revDamp = get (revDamp);
    s.revWidth = get (revWidth);
    s.revMix = get (revMix);

    s.pumpOn = get (pumpOn) > 0.5f;
    s.pumpBeats = Choices::pumpRateBeats[juce::jlimit (0, Choices::pumpRates.size() - 1, getChoice (pumpRate))];
    s.pumpDepth = get (pumpDepth);
    s.pumpRelease = get (pumpRelease);

    s.dynamics.compOn = get (compOn) > 0.5f;
    s.dynamics.threshold = get (compThresh);
    s.dynamics.ratio = get (compRatio);
    s.dynamics.attackMs = get (compAttack);
    s.dynamics.releaseMs = get (compRelease);
    s.dynamics.makeupDb = get (compMakeup);
    s.dynamics.limitOn = get (limitOn) > 0.5f;

    s.mix = get (ParamIDs::mix);
    s.outputDb = get (output);
    return s;
}

rf::Synth::Settings ZeygorgProcessor::readSynthSettings (double bpm) const
{
    using namespace ParamIDs;
    using LP = LayerParam::Field;
    rf::Synth::Settings s;

    for (int l = 0; l < rf::Synth::numLayers; ++l)
    {
        auto id = [l] (LP f) -> const juce::String& { return LayerParam::id (l, f); };
        auto& ls = s.layers[(size_t) l];
        ls.on = get (id (LP::on)) > 0.5f;
        ls.wave = (rf::Synth::Wave) juce::jlimit (0, Choices::waves.size() - 1, getChoice (id (LP::wave)));
        ls.level = get (id (LP::level));
        ls.pan = get (id (LP::pan));
        ls.octave = getChoice (id (LP::octave));
        ls.semi = getChoice (id (LP::semi));
        ls.fine = get (id (LP::fine));
        ls.unison = getChoice (id (LP::unison));
        ls.detune = get (id (LP::detune));
        ls.spread = get (id (LP::spread));
        ls.warp = (rf::Synth::Warp) juce::jlimit (0, 2, getChoice (id (LP::warp)));
        ls.speed = get (id (LP::speed));
        ls.syncBeats = Choices::syncLengthBeats[juce::jlimit (0, Choices::syncLengths.size() - 1, getChoice (id (LP::sync)))];
        ls.grainMs = get (id (LP::grain));
        ls.start = get (id (LP::start));
        ls.end = get (id (LP::end));
        ls.loop = get (id (LP::loop)) > 0.5f;
        ls.reverse = get (id (LP::reverse)) > 0.5f;
    }

    s.ampA = get (ampAttack);
    s.ampD = get (ampDecay);
    s.ampS = get (ampSustain);
    s.ampR = get (ampRelease);
    s.filterType = getChoice (vFltType);
    s.cutoff = get (vCutoff);
    s.reso = get (vReso);
    s.envAmt = get (vEnvAmt);
    s.keyTrack = get (vKeyTrack);
    s.fltA = get (fltAttack);
    s.fltD = get (fltDecay);
    s.fltS = get (fltSustain);
    s.fltR = get (fltRelease);
    s.mode = (rf::Synth::VoiceMode) juce::jlimit (0, 2, getChoice (voiceMode));
    s.glideMs = get (glide);
    s.velSens = get (velSens);
    s.transpose = getChoice (synthTranspose);
    s.level = get (synthLevel);
    s.bpm = bpm;
    return s;
}

void ZeygorgProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    using namespace ParamIDs;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    rf::Engine::Transport transport;
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            if (auto bpm = pos->getBpm())
                transport.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition())
            {
                transport.ppq = *ppq;
                transport.playing = pos->getIsPlaying();
            }
        }
    }

    auto numSamples = buffer.getNumSamples();
    auto root = getChoice (randRoot);
    auto scale = getChoice (randScale);
    auto bpm = transport.bpm > 0.0 ? transport.bpm : 128.0;
    auto beatsPerSample = bpm / 60.0 / getSampleRate();
    auto beat0 = transport.playing ? transport.ppq : freeBeat;
    freeBeat = beat0 + beatsPerSample * numSamples;

    // Notes flow: played keys + host MIDI -> sequencer adds its notes ->
    // snap/chord -> arp -> synth.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    {
        rf::Sequencer::Settings ss;
        ss.on = get (randOn) > 0.5f;
        ss.root = root;
        ss.scale = scale;
        ss.stepBeats = Choices::randRateBeats[juce::jlimit (0, Choices::randRates.size() - 1, getChoice (randRate))];
        ss.octaves = getChoice (randOctaves);
        ss.steps = getChoice (seqSteps);
        ss.length = get (randLength);
        ss.swing = get (seqSwing);
        sequencer.process (midi, numSamples, beat0, beatsPerSample, pattern, ss);
    }

    {
        rf::NoteMapper::Settings ms;
        ms.snap = get (snapKeys) > 0.5f;
        ms.chord = getChoice (chordMode);
        ms.root = root;
        ms.scale = scale;
        mapper.process (midi, scratchMidi, ms);
    }

    {
        rf::Arp::Settings as;
        as.on = get (arpOn) > 0.5f;
        as.mode = getChoice (arpMode);
        as.stepBeats = Choices::randRateBeats[juce::jlimit (0, Choices::randRates.size() - 1, getChoice (arpRate))];
        as.octaves = getChoice (arpOctaves);
        as.gate = get (arpGate);
        as.latch = get (arpLatch) > 0.5f;
        arp.process (midi, scratchMidi, numSamples, beat0, beatsPerSample, as);
    }

    synth.renderAdding (buffer, midi, readSynthSettings (bpm));
    engine.process (buffer, readSettings(), transport);
}

juce::AudioProcessorEditor* ZeygorgProcessor::createEditor()
{
    return new ZeygorgEditor (*this);
}

void ZeygorgProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = captureState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ZeygorgProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreState (juce::ValueTree::fromXml (*xml));
}

#if ! ZEYGORG_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZeygorgProcessor();
}
#endif
