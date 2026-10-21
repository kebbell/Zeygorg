// Offline smoke test: renders a saw riff through Zeygorg with the defaults,
// everything maxed, a "showcase" setting and 20 random settings, and fails if
// any output is NaN/inf or runs away. Also writes WAVs and a UI snapshot.
//
// Usage: ZeygorgTest [output-dir]

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

Result render (ZeygorgProcessor& proc, const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& out)
{
    proc.reset();
    out.makeCopyOf (input);
    juce::MidiBuffer midi;
    float peak = 0.0f;

    for (int start = 0; start < out.getNumSamples(); start += blockSize)
    {
        auto n = juce::jmin (blockSize, out.getNumSamples() - start);
        juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, start, n);
        midi.clear(); // hosts hand over a fresh buffer every block
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

void setAll (ZeygorgProcessor& proc, std::function<float (juce::RangedAudioParameter&)> value)
{
    for (auto* p : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            r->setValueNotifyingHost (value (*r));
}

void setParam (ZeygorgProcessor& proc, const juce::String& id, float plainValue)
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

    ZeygorgProcessor proc;
    proc.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    auto input = makeRiff();
    juce::AudioBuffer<float> out;
    int failures = 0;

    auto report = [&] (const juce::String& name, Result r)
    {
        std::printf ("%-14s %s  peak %.3f\n", name.toRawUTF8(), r.ok ? "ok  " : "FAIL", r.peak);
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

    // Every built-in sample must decode and have a sensible root note.
    {
        const auto& lib = SampleLibrary::get();
        int bad = 0;
        for (int i = 0; i < lib.size(); ++i)
        {
            auto sample = lib.load (i);
            if (sample == nullptr || sample->audio.getNumSamples() < 100 || lib[i].rootNote < 60 || lib[i].rootNote > 71)
            {
                std::printf ("  bad sample %d: %s %s\n", i, lib[i].category.toRawUTF8(), lib[i].name.toRawUTF8());
                ++bad;
            }
        }
        report ("samples x" + juce::String (lib.size()), { bad == 0 && lib.size() == 311, (float) bad });
    }

    // Keys only (saw): silent input, a chord held on the on-screen keyboard state.
    setParam (proc, ParamIDs::keysSource, 0.0f);
    {
        juce::AudioBuffer<float> silence (2, input.getNumSamples());
        silence.clear();
        for (auto note : { 45, 52, 57 })
            proc.keyboardState.noteOn (1, note, 0.9f);
        auto r = render (proc, silence, out);
        proc.keyboardState.allNotesOff (1);
        r.ok = r.ok && r.peak > 0.05f; // the synth must actually sound
        report ("keys saw", r);
        writeWav (outDir.getChildFile ("keys.wav"), out);
    }

    // Keys from a few samples across the categories.
    setParam (proc, ParamIDs::keysSource, 1.0f);
    for (auto index : { 0, 100, 250, 305 })
    {
        setParam (proc, ParamIDs::sampleIndex, (float) index);
        proc.syncSample();
        juce::AudioBuffer<float> silence (2, input.getNumSamples());
        silence.clear();
        for (auto note : { 48, 55, 60 })
            proc.keyboardState.noteOn (1, note, 0.9f);
        auto r = render (proc, silence, out);
        proc.keyboardState.allNotesOff (1);
        r.ok = r.ok && r.peak > 0.05f && proc.getLoadedSampleIndex() == index;
        report ("sample " + juce::String (index), r);
        if (index == 0)
            writeWav (outDir.getChildFile ("sample.wav"), out);
    }

    // Randomiser on its own: notes must stay in key, repeat every loop, and
    // pair every note-on with a note-off.
    {
        rf::Randomiser rnd;
        rf::Randomiser::Settings rs;
        rs.on = true;
        rs.root = 9;      // A
        rs.scale = 1;     // minor
        rs.octaves = 2;
        rs.seed = 42;
        const auto& minor = rf::Scales::all()[1];

        auto beatsPerSample = 128.0 / 60.0 / sampleRate;
        juce::Array<int> notes;
        int ons = 0, offs = 0, outOfKey = 0;
        double beat = 0.0;
        for (int b = 0; b < 2000; ++b)
        {
            juce::MidiBuffer midi;
            rnd.process (midi, blockSize, beat, beatsPerSample, rs);
            beat += beatsPerSample * blockSize;
            for (const auto m : midi)
            {
                auto msg = m.getMessage();
                if (msg.isNoteOn())
                {
                    ++ons;
                    notes.add (msg.getNoteNumber());
                    auto rel = ((msg.getNoteNumber() - rs.root) % 12 + 12) % 12;
                    if (std::find (minor.steps.begin(), minor.steps.begin() + minor.size, rel) == minor.steps.begin() + minor.size)
                        ++outOfKey;
                }
                else if (msg.isNoteOff())
                    ++offs;
            }
        }

        // Steps are 1/16ths, so the first 16 notes of loop 1 and loop 2 must match.
        auto perLoop = 0;
        { juce::MidiBuffer m; rf::Randomiser r2; r2.process (m, (int) (4.0 / beatsPerSample), 0.0, beatsPerSample, rs);
          for (const auto e : m) perLoop += e.getMessage().isNoteOn() ? 1 : 0; }
        auto repeats = perLoop > 0 && notes.size() > 2 * perLoop;
        for (int i = 0; repeats && i < perLoop; ++i)
            repeats = notes[i] == notes[i + perLoop];

        auto ok = ons > 100 && outOfKey == 0 && std::abs (ons - offs) <= 1 && repeats;
        std::printf ("randomiser     %s  %d notes, %d out of key, %d per bar, repeats %s\n",
                     ok ? "ok  " : "FAIL", ons, outOfKey, perLoop, repeats ? "yes" : "no");
        if (! ok)
            ++failures;

        auto snapOk = rf::Scales::snap (61, 0, 0) == 60 && rf::Scales::snap (66, 0, 0) == 65 && rf::Scales::snap (64, 9, 1) == 64
                   && rf::Scales::snap (63, 9, 1) == 62;
        std::printf ("snap           %s\n", snapOk ? "ok" : "FAIL");
        if (! snapOk)
            ++failures;
    }

    // Randomiser through the whole plugin (transport stopped, so it free-runs).
    {
        setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
        setParam (proc, ParamIDs::randOn, 1.0f);
        setParam (proc, ParamIDs::sampleIndex, 210.0f);
        proc.syncSample();
        juce::AudioBuffer<float> silence (2, input.getNumSamples());
        silence.clear();
        auto r = render (proc, silence, out);
        r.ok = r.ok && r.peak > 0.05f;
        report ("random keys", r);
        writeWav (outDir.getChildFile ("randomiser.wav"), out);
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
        for (int ch = 1; ch <= 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (proc.keyboardState.isNoteOn (ch, n))
                {
                    std::printf ("  stuck key ch %d note %d\n", ch, n);
                    ++failures;
                }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile ("ui.png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
