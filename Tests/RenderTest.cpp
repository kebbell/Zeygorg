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
    setParam (proc, ParamIDs::limitOn, 0.0f);
    {
        auto r = render (proc, input, out);
        r.ok = r.ok && r.peak < 1.0f; // the defaults shouldn't need the limiter
        report ("defaults raw", r);
    }
    setParam (proc, ParamIDs::limitOn, 1.0f);
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

    juce::AudioBuffer<float> silence (2, input.getNumSamples());
    silence.clear();

    // Holds a chord on the on-screen keyboard while rendering silence.
    auto renderKeys = [&] (std::initializer_list<int> notes)
    {
        for (auto note : notes)
            proc.keyboardState.noteOn (1, note, 0.9f);
        auto r = render (proc, silence, out);
        proc.keyboardState.allNotesOff (1);
        return r;
    };

    auto layerParam = [] (int l, LayerParam::Field f) { return LayerParam::id (l, f); };

    // Each oscillator wave on layer 2 alone.
    for (int w = 0; w < Choices::waves.size() - 1; ++w)
    {
        setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
        setParam (proc, layerParam (0, LayerParam::on), 0.0f);
        setParam (proc, layerParam (1, LayerParam::on), 1.0f);
        setParam (proc, layerParam (1, LayerParam::wave), (float) w);
        auto r = renderKeys ({ 45, 52, 57 });
        r.ok = r.ok && r.peak > 0.05f;
        report ("wave " + Choices::waves[w], r);
        if (w == 0)
            writeWav (outDir.getChildFile ("supersaw.wav"), out);
    }

    // Samples through every warp mode, plus loop and reverse.
    for (int mode = 0; mode < 6; ++mode)
    {
        setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
        setParam (proc, layerParam (0, LayerParam::sample), (float) (mode * 50));
        setParam (proc, layerParam (0, LayerParam::warp), (float) juce::jmin (mode, 2));
        if (mode == 1) setParam (proc, layerParam (0, LayerParam::speed), 0.5f);
        if (mode == 3) setParam (proc, layerParam (0, LayerParam::loop), 1.0f);
        if (mode == 4) setParam (proc, layerParam (0, LayerParam::reverse), 1.0f);
        if (mode == 5) { setParam (proc, layerParam (0, LayerParam::unison), 5.0f); setParam (proc, layerParam (0, LayerParam::warp), 1.0f); }
        proc.syncSamples();
        auto r = renderKeys ({ 48, 55, 60 });
        r.ok = r.ok && r.peak > 0.05f;
        const char* names[] { "repitch", "stretch", "tempo sync", "loop", "reverse", "unison stretch" };
        report (juce::String ("sample ") + names[mode], r);
        if (mode == 1)
            writeWav (outDir.getChildFile ("stretch.wav"), out);
    }

    // A dropped-in file: write a WAV, point layer 1 at it, play it.
    {
        auto file = outDir.getChildFile ("Test Pad A2.wav");
        juce::AudioBuffer<float> tone (1, 22050);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            tone.setSample (0, i, 0.5f * std::sin ((float) i * 110.0f * juce::MathConstants<float>::twoPi / 44100.0f));
        {
            file.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> stream (file.createOutputStream().release());
            if (auto* writer = wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0))
            {
                stream.release();
                std::unique_ptr<juce::AudioFormatWriter> w (writer);
                w->writeFromAudioSampleBuffer (tone, 0, tone.getNumSamples());
            }
        }
        setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
        proc.setLayerFile (0, file);
        auto loaded = proc.synth.getSample (0);
        auto r = renderKeys ({ 57 });
        auto ok = loaded != nullptr && loaded->rootNote == 57 && loaded->audio.getNumSamples() == 22050 && r.ok && r.peak > 0.05f
               && SampleLibrary::rootNoteFromName ("Stab C#3") == 61 && SampleLibrary::rootNoteFromName ("bass_F2") == 53
               && SampleLibrary::rootNoteFromName ("no note here") == 60;
        report ("user file", { ok, r.peak });
        proc.clearLayerFile (0);
    }

    // Sequencer: notes stay in key, repeat every loop, pair on/off.
    {
        rf::Sequencer seq;
        rf::Pattern pattern;
        pattern.generate (42, 0.7f, rf::Scales::degreeCount (1, 2));
        rf::Sequencer::Settings ss;
        ss.on = true;
        ss.root = 9;      // A
        ss.scale = 1;     // minor
        ss.octaves = 2;
        ss.swing = 0.4f;
        const auto& minor = rf::Scales::all()[1];

        auto beatsPerSample = 128.0 / 60.0 / sampleRate;
        juce::Array<int> notes;
        int ons = 0, offs = 0, outOfKey = 0;
        double beat = 0.0;
        for (int b = 0; b < 2000; ++b)
        {
            juce::MidiBuffer midi;
            seq.process (midi, blockSize, beat, beatsPerSample, pattern, ss);
            beat += beatsPerSample * blockSize;
            for (const auto m : midi)
            {
                auto msg = m.getMessage();
                if (msg.isNoteOn())
                {
                    ++ons;
                    notes.add (msg.getNoteNumber());
                    auto rel = ((msg.getNoteNumber() - ss.root) % 12 + 12) % 12;
                    if (std::find (minor.steps.begin(), minor.steps.begin() + minor.size, rel) == minor.steps.begin() + minor.size)
                        ++outOfKey;
                }
                else if (msg.isNoteOff())
                    ++offs;
            }
        }

        auto perLoop = 0;
        for (int i = 0; i < rf::Pattern::maxSteps; ++i)
            perLoop += pattern.get (i).isRest() ? 0 : 1;
        auto repeats = perLoop > 0 && notes.size() > 2 * perLoop;
        for (int i = 0; repeats && i < perLoop; ++i)
            repeats = notes[i] == notes[i + perLoop];

        rf::Pattern copy;
        copy.fromString (pattern.toString());
        auto roundTrip = copy.toString() == pattern.toString();

        auto ok = ons > 100 && outOfKey == 0 && std::abs (ons - offs) <= 1 && repeats && roundTrip;
        std::printf ("sequencer      %s  %d notes, %d out of key, %d per loop, repeats %s, save %s\n",
                     ok ? "ok  " : "FAIL", ons, outOfKey, perLoop, repeats ? "yes" : "no", roundTrip ? "ok" : "bad");
        if (! ok)
            ++failures;

        auto snapOk = rf::Scales::snap (61, 0, 0) == 60 && rf::Scales::snap (66, 0, 0) == 65 && rf::Scales::snap (64, 9, 1) == 64
                   && rf::Scales::snap (63, 9, 1) == 62;
        std::printf ("snap           %s\n", snapOk ? "ok" : "FAIL");
        if (! snapOk)
            ++failures;
    }

    // Chords: offs match ons even if the chord changes while the key is held.
    {
        rf::NoteMapper mapper;
        juce::MidiBuffer midi, scratch;
        rf::NoteMapper::Settings ms;
        ms.chord = 8; // Min7
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        mapper.process (midi, scratch, ms);
        int ons = 0;
        for (const auto m : midi) ons += m.getMessage().isNoteOn() ? 1 : 0;

        ms.chord = 3; // switch to Major before the release
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        mapper.process (midi, scratch, ms);
        juce::Array<int> offs;
        for (const auto m : midi) if (m.getMessage().isNoteOff()) offs.add (m.getMessage().getNoteNumber());

        auto triad = rf::NoteMapper::notesFor (62, { false, 11, 0, 0 }); // D in C major: D F A
        auto ok = ons == 4 && offs.size() == 4 && offs.contains (70) && triad == std::vector<int> { 62, 65, 69 };
        std::printf ("chords         %s  %d ons, %d offs\n", ok ? "ok" : "FAIL", ons, offs.size());
        if (! ok)
            ++failures;
    }

    // Arp: plays held notes on the grid and lets go of everything after release.
    {
        rf::Arp arp;
        rf::Arp::Settings as;
        as.on = true;
        as.mode = 2;
        as.octaves = 2;
        auto beatsPerSample = 128.0 / 60.0 / sampleRate;
        double beat = 0.0;
        int ons = 0, offs = 0;
        juce::MidiBuffer scratch;
        juce::SortedSet<int> seen;
        for (int b = 0; b < 400; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (auto n : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, 0.8f), 0);
            if (b == 300)
                for (auto n : { 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            arp.process (midi, scratch, blockSize, beat, beatsPerSample, as);
            beat += beatsPerSample * blockSize;
            for (const auto m : midi)
            {
                if (m.getMessage().isNoteOn()) { ++ons; seen.add (m.getMessage().getNoteNumber()); }
                if (m.getMessage().isNoteOff()) ++offs;
            }
        }
        auto ok = ons >= 25 && ons == offs && seen.size() == 6; // ~27 sixteenths in 300 blocks
        std::printf ("arp            %s  %d notes, %d distinct\n", ok ? "ok" : "FAIL", ons, seen.size());
        if (! ok)
            ++failures;
    }

    // Every factory preset, played with a chord (and its own sequencer/arp).
    {
        for (int i = 0; i < proc.presets.numFactory(); ++i)
        {
            proc.presets.loadFactory (i);
            auto r = renderKeys ({ 57, 60, 64 });
            r.ok = r.ok && r.peak > 0.02f && r.peak < 2.5f;
            report ("preset " + proc.presets.getFactory (i).name, r);
            if (i == 5)
                writeWav (outDir.getChildFile ("preset-acid.wav"), out);
        }
        proc.presets.loadFactory (0);
    }

    // The pattern as a MIDI file for dragging into the DAW.
    {
        auto file = proc.writePatternMidi();
        juce::MidiFile midi;
        juce::FileInputStream in (file);
        auto ok = in.openedOk() && midi.readFrom (in) && midi.getNumTracks() == 1 && midi.getTrack (0)->getNumEvents() > 4;
        std::printf ("midi export    %s  %s\n", ok ? "ok" : "FAIL", file.getFileName().toRawUTF8());
        if (! ok)
            ++failures;
    }

    juce::Random rng (1234);
    for (int i = 0; i < 20; ++i)
    {
        setAll (proc, [&] (auto&) { return rng.nextFloat(); });
        proc.syncSamples();
        for (auto note : { 48, 60 })
            proc.keyboardState.noteOn (1, note, 0.9f);
        report ("random " + juce::String (i + 1), render (proc, input, out));
        proc.keyboardState.allNotesOff (1);
    }

    // UI snapshots of every page, with a sound playing so the displays move.
    setAll (proc, [] (auto& p) { return p.getDefaultValue(); });
    proc.presets.loadFactory (3);
    setParam (proc, ParamIDs::gateOn, 1.0f);
    setParam (proc, ParamIDs::gateDepth, 0.5f);
    setParam (proc, ParamIDs::pumpOn, 1.0f);
    setParam (proc, ParamIDs::pumpDepth, 0.4f);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto* zey = dynamic_cast<ZeygorgEditor*> (editor.get());
        editor->setVisible (true); // as a host would; the displays only animate when visible
        for (int ch = 1; ch <= 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (proc.keyboardState.isNoteOn (ch, n))
                {
                    std::printf ("  stuck key ch %d note %d\n", ch, n);
                    ++failures;
                }

        const char* pages[] { "ui-synth.png", "ui-sequence.png", "ui-fx.png" };
        for (int page = 0; page < 3; ++page)
        {
            zey->showPage (page);
            for (auto note : { 57, 64 })
                proc.keyboardState.noteOn (1, note, 0.9f);
            render (proc, silence, out); // fills the scope and playheads
            // Let the displays' timers pick up the new state.
            for (int t = 0; t < 6; ++t)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            proc.keyboardState.allNotesOff (1);

            auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
            auto file = outDir.getChildFile (pages[page]);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
