// Loads built plugin bundles the way a DAW does: scan the file, instantiate,
// prepare, process audio with MIDI, and open the editor.
//
// Usage: ZeygorgLoadTest <path to .vst3 or .component> [...]

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

static bool testPlugin (juce::AudioPluginFormatManager& formats, const juce::String& path)
{
    std::printf ("== %s\n", path.toRawUTF8());

    for (auto* format : formats.getFormats())
    {
        if (! format->fileMightContainThisPluginType (path))
            continue;

        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile (found, path);
        if (found.isEmpty())
        {
            std::printf ("  %s: no plugin found in file\n", format->getName().toRawUTF8());
            return false;
        }

        const auto& desc = *found[0];
        std::printf ("  found: %s by %s (%s, %d in / %d out, midi in: %s)\n",
                     desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.pluginFormatName.toRawUTF8(),
                     desc.numInputChannels, desc.numOutputChannels, desc.isInstrument ? "instrument" : "effect");

        juce::String error;
        auto instance = formats.createPluginInstance (desc, 48000.0, 512, error);
        if (instance == nullptr)
        {
            std::printf ("  FAIL: couldn't create instance: %s\n", error.toRawUTF8());
            return false;
        }

        instance->enableAllBuses();
        instance->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (juce::jmax (2, instance->getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        bool finite = true;

        for (int block = 0; block < 200; ++block)
        {
            buffer.clear();
            midi.clear();
            if (block == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
            if (block == 150)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            instance->processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    auto v = buffer.getSample (ch, i);
                    finite = finite && std::isfinite (v);
                    peak = juce::jmax (peak, std::abs (v));
                }
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->hasEditor() ? instance->createEditor() : nullptr);
        auto editorOk = editor != nullptr && editor->getWidth() > 0;
        editor.reset();
        instance->releaseResources();

        auto ok = finite && peak > 0.01f && editorOk;
        std::printf ("  %s: MIDI note peak %.3f, editor %s\n", ok ? "ok" : "FAIL", peak, editorOk ? "opened" : "missing");
        return ok;
    }

    std::printf ("  FAIL: no format recognises this file\n");
    return false;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    int failures = 0;
    for (int i = 1; i < argc; ++i)
        if (! testPlugin (formats, juce::String (argv[i])))
            ++failures;

    std::printf ("\n%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
