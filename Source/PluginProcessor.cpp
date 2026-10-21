#include "PluginProcessor.h"
#include "PluginEditor.h"

ZeygorgProcessor::ZeygorgProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Zeygorg", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            paramIDs.add (ranged->getParameterID());

    for (auto& id : paramIDs)
        params[std::string_view (id.toRawUTF8())] = apvts.getRawParameterValue (id);

    syncSample();
    startTimerHz (20);
}

void ZeygorgProcessor::syncSample()
{
    auto index = getChoice (ParamIDs::sampleIndex);
    if (index != loadedSample)
    {
        keys.setSample (SampleLibrary::get().load (index));
        loadedSample = index;
    }
    keys.collectGarbage();
}

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
    keys.prepare (sampleRate, samplesPerBlock);
    randomiser.reset();
    snappedMidi.ensureSize (4096);
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

    s.mix = get (ParamIDs::mix);
    s.outputDb = get (output);
    return s;
}

void ZeygorgProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

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
    auto root = getChoice (ParamIDs::randRoot);
    auto scale = getChoice (ParamIDs::randScale);

    // Merge on-screen key presses into the host MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    // Snap played notes into the randomiser's key and scale. It's a fixed
    // mapping, so note-offs land on the same notes as their note-ons.
    if (get (ParamIDs::snapKeys) > 0.5f)
    {
        snappedMidi.clear();
        for (const auto m : midi)
        {
            auto msg = m.getMessage();
            if (msg.isNoteOnOrOff())
                msg.setNoteNumber (rf::Scales::snap (msg.getNoteNumber(), root, scale));
            snappedMidi.addEvent (msg, m.samplePosition);
        }
        midi.swapWith (snappedMidi);
    }

    // Random notes on the DAW's grid (or free-running at its tempo when stopped).
    {
        auto bpm = transport.bpm > 0.0 ? transport.bpm : 128.0;
        auto beatsPerSample = bpm / 60.0 / getSampleRate();
        auto beat0 = transport.playing ? transport.ppq : freeBeat;
        freeBeat = beat0 + beatsPerSample * numSamples;

        rf::Randomiser::Settings rs;
        rs.on = get (ParamIDs::randOn) > 0.5f;
        rs.root = root;
        rs.scale = scale;
        rs.stepBeats = Choices::randRateBeats[juce::jlimit (0, Choices::randRates.size() - 1, getChoice (ParamIDs::randRate))];
        rs.octaves = getChoice (ParamIDs::randOctaves);
        rs.chance = get (ParamIDs::randChance);
        rs.length = get (ParamIDs::randLength);
        rs.seed = getChoice (ParamIDs::randSeed);
        randomiser.process (midi, numSamples, beat0, beatsPerSample, rs);
    }

    // Play the keys synth into the input.
    rf::KeysSynth::Settings keySettings;
    keySettings.useSample = getChoice (ParamIDs::keysSource) == 1;
    keySettings.transpose = getChoice (ParamIDs::keysTranspose);
    keySettings.releaseMs = get (ParamIDs::keysRelease);
    keySettings.level = get (ParamIDs::keysLevel);
    keys.renderAdding (buffer, midi, keySettings);

    engine.process (buffer, readSettings(), transport);
}

juce::AudioProcessorEditor* ZeygorgProcessor::createEditor()
{
    return new ZeygorgEditor (*this);
}

void ZeygorgProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ZeygorgProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

#if ! ZEYGORG_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZeygorgProcessor();
}
#endif
