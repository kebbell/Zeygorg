#include "PluginProcessor.h"
#include "PluginEditor.h"

RaveForgeProcessor::RaveForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RaveForge", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            paramIDs.add (ranged->getParameterID());

    for (auto& id : paramIDs)
        params[std::string_view (id.toRawUTF8())] = apvts.getRawParameterValue (id);
}

bool RaveForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void RaveForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
}

rf::Engine::Settings RaveForgeProcessor::readSettings() const
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

void RaveForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    engine.process (buffer, readSettings(), transport);
}

juce::AudioProcessorEditor* RaveForgeProcessor::createEditor()
{
    return new RaveForgeEditor (*this);
}

void RaveForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RaveForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

#if ! RAVEFORGE_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RaveForgeProcessor();
}
#endif
