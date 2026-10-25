#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Engine.h"
#include "DSP/Synth.h"
#include "DSP/NoteFx.h"
#include "Parameters.h"
#include "Presets.h"
#include <map>
#include <string_view>

class ZeygorgProcessor : public juce::AudioProcessor,
                         private juce::Timer
{
public:
    ZeygorgProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zeygorg"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return presets.numFactory(); }
    int getCurrentProgram() override { return juce::jmax (0, presets.currentIndex()); }
    void setCurrentProgram (int index) override { presets.loadFactory (index); }
    const juce::String getProgramName (int index) override { return presets.getFactory (index).name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    rf::Engine engine;
    rf::Synth synth;
    rf::Pattern pattern;
    rf::Sequencer sequencer;
    PresetManager presets;

    // Shared with the editor's on-screen keyboard.
    juce::MidiKeyboardState keyboardState;

    // Layer samples: either from the built-in library (the layer's Sample
    // parameter) or a file the user dropped in, which wins while it's set.
    void setLayerFile (int layer, const juce::File& file);
    void clearLayerFile (int layer);
    juce::String getLayerFile (int layer) const;
    bool isLayerFileMissing (int layer) const { return fileMissing[(size_t) layer]; }

    // Loads whatever the layers ask for if it changed. Runs on a timer;
    // public so the offline test can call it directly.
    void syncSamples();

    // Full state including the pattern and dropped files, for presets and A/B.
    juce::ValueTree captureState();
    void restoreState (const juce::ValueTree&);

    // Writes the pattern as a one-loop MIDI file for dragging into the DAW.
    juce::File writePatternMidi() const;

    static juce::String layerFileKey (int layer) { return "layerFile" + juce::String (layer + 1); }

private:
    rf::Engine::Settings readSettings() const;
    rf::Synth::Settings readSynthSettings (double bpm) const;
    float get (const char* id) const { return params.find (std::string_view (id))->second->load(); }
    float get (const juce::String& id) const { return get (id.toRawUTF8()); }
    int getChoice (const char* id) const { return juce::roundToInt (get (id)); }
    int getChoice (const juce::String& id) const { return juce::roundToInt (get (id)); }

    // Keyed by views into `paramIDs` so lookups on the audio thread don't allocate.
    juce::StringArray paramIDs;
    std::map<std::string_view, std::atomic<float>*, std::less<>> params;

    void timerCallback() override { syncSamples(); }

    rf::NoteMapper mapper;
    rf::Arp arp;
    juce::MidiBuffer scratchMidi;
    double freeBeat = 0.0;
    std::array<juce::String, rf::Synth::numLayers> loadedKey;
    std::array<bool, rf::Synth::numLayers> fileMissing {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZeygorgProcessor)
};
