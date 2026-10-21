// Offline smoke test: renders a saw riff through RaveForge with the defaults,
// everything maxed, a "showcase" setting and 20 random settings, and fails if
// any output is NaN/inf or runs away. Also writes WAVs and a UI snapshot.
//
// Usage: RaveForgeTest [output-dir]

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr double seconds = 6.0;

juce::AudioBuffer<float> makeRiff()
{
    // Eight-note minor riff on a plain saw at 0.3 peak.
    const int notes[] { 45, 45, 57, 45, 52, 45, 55, 53 };
    auto total = (int) (seconds * sampleRate);
    auto noteLen = (int) (sampleRate * 0.25);
    juce::AudioBuffer<float> buf (2, total);
    double phase = 0.0;

    for (int i = 0; i < total; ++i)
    {
        auto note = notes[(i / noteLen) % 8];
        auto freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        phase += freq / sampleRate;
        phase -= std::floor (phase);

        auto posInNote = i % noteLen;
        auto envelope = posInNote < noteLen * 0.8 ? 1.0f : 0.0f;
        auto s = (float) (2.0 * phase - 1.0) * 0.3f * envelope;
        buf.setSample (0, i, s);
        buf.setSample (1, i, s);
    }
    return buf;
}

struct Result { bool ok; float peak; };

Result render (RaveForgeProcessor& proc, const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& out)
{
    proc.reset();
    out.makeCopyOf (input);
    juce::MidiBuffer midi;
    float peak = 0.0f;

    for (int start = 0; start < out.getNumSamples(); start += blockSize)
    {
        auto n = juce::jmin (blockSize, out.getNumSamples() - start);
        juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, start, n);
        proc.processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                auto v = block.getSample (ch, i);
                if (! std::isfinite (v))
                    return { false, INFINITY };
                peak = juce::jmax (peak, std::abs (v));
            }
    }
    return { peak < 8.0f, peak };
}

void setAll (RaveForgeProcessor& proc, std::function<float (juce::RangedAudioParameter&)> value)
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (value (*r));
}

void setParam (RaveForgeProcessor& proc, const juce::String& id, float plainValue)
{
    auto* p = proc.apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
    if (auto* writer = wav.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0))
    {
        stream.release();
        std::unique_ptr<juce::AudioFormatWriter> w (writer);
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    auto outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    RaveForgeProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    auto input = makeRiff();
    juce::AudioBuffer<float> out;
    int failures = 0;

    auto report = [&] (const juce::String& name, Result r)
    {
        std::printf ("%-12s %s  peak %.3f\n", name.toRawUTF8(), r.ok ? "ok  " : "FAIL", r.peak);
        if (! r.ok)
            ++failures;
    };

    setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
    report ("defaults", render (proc, input, out));
    writeWav (outDir.getChildFile ("defaults.wav"), out);

    setAll (proc, [] (auto&) { return 1.0f; });
    report ("maxed", render (proc, input, out));

    // A typical "big rave" patch to listen to.
    setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
    setParam (proc, ParamIDs::power, 1.0f);
    setParam (proc, ParamIDs::width, 1.0f);
    setParam (proc, ParamIDs::detune, 25.0f);
    setParam (proc, ParamIDs::subOct, 0.5f);
    setParam (proc, ParamIDs::cutoff, 2500.0f);
    setParam (proc, ParamIDs::resonance, 0.45f);
    setParam (proc, ParamIDs::lfoCutoff, 0.4f);
    setParam (proc, ParamIDs::drive, 9.0f);
    setParam (proc, ParamIDs::gateOn, 1.0f);
    setParam (proc, ParamIDs::dlyMix, 0.3f);
    setParam (proc, ParamIDs::revMix, 0.25f);
    report ("showcase", render (proc, input, out));
    writeWav (outDir.getChildFile ("showcase.wav"), out);

    // Keys only: silent input, a chord held on the on-screen keyboard state.
    {
        juce::AudioBuffer<float> silence (2, input.getNumSamples());
        silence.clear();
        for (auto note : { 45, 52, 57 })
            proc.keyboardState.noteOn (1, note, 0.9f);
        auto r = render (proc, silence, out);
        proc.keyboardState.allNotesOff (1);
        r.ok = r.ok && r.peak > 0.05f; // the synth must actually sound
        report ("keys", r);
        writeWav (outDir.getChildFile ("keys.wav"), out);
    }

    juce::Random rng (1234);
    for (int i = 0; i < 20; ++i)
    {
        setAll (proc, [&] (auto&) { return rng.nextFloat(); });
        report ("random " + juce::String (i + 1), render (proc, input, out));
    }

    // UI snapshot with the showcase-ish defaults restored.
    setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
    setParam (proc, ParamIDs::gateOn, 1.0f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile ("ui.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
