#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Engine.h"
#include "DSP/KeysSynth.h"
#include "Parameters.h"
#include <map>
#include <string_view>

class RaveForgeProcessor : public juce::AudioProcessor
{
public:
    RaveForgeProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override { engine.reset(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RaveForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rf::Engine engine;

    // Shared with the editor's on-screen keyboard.
    juce::MidiKeyboardState keyboardState;

private:
    rf::Engine::Settings readSettings() const;
    float get (std::string_view id) const { return params.find (id)->second->load(); }
    int getChoice (std::string_view id) const { return juce::roundToInt (get (id)); }

    // Keyed by views into `paramIDs` so lookups on the audio thread don't allocate.
    juce::StringArray paramIDs;
    std::map<std::string_view, std::atomic<float>*, std::less<>> params;

    rf::KeysSynth keys;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RaveForgeProcessor)
};
