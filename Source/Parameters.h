#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleLibrary.h"
#include "DSP/NoteFx.h"

// Parameter IDs, choice lists and the APVTS layout. Everything the engine and
// the editor need to agree on lives here.
namespace ParamIDs
{
    // Rave stack
    inline constexpr auto power      = "power";
    inline constexpr auto width      = "width";
    inline constexpr auto detune     = "detune";
    inline constexpr auto subOct     = "subOct";
    inline constexpr auto fifth      = "fifth";
    inline constexpr auto octUp      = "octUp";

    // Filter + envelope follower
    inline constexpr auto fltType    = "fltType";
    inline constexpr auto cutoff     = "cutoff";
    inline constexpr auto resonance  = "resonance";
    inline constexpr auto envAmount  = "envAmount";
    inline constexpr auto envAttack  = "envAttack";
    inline constexpr auto envRelease = "envRelease";

    // Dirt
    inline constexpr auto drive      = "drive";
    inline constexpr auto shape      = "shape";
    inline constexpr auto bits       = "bits";
    inline constexpr auto downsample = "downsample";
    inline constexpr auto dirtMix    = "dirtMix";

    // LFO
    inline constexpr auto lfoShape   = "lfoShape";
    inline constexpr auto lfoSync    = "lfoSync";
    inline constexpr auto lfoRate    = "lfoRate";
    inline constexpr auto lfoDiv     = "lfoDiv";
    inline constexpr auto lfoCutoff  = "lfoCutoff";
    inline constexpr auto lfoDetune  = "lfoDetune";
    inline constexpr auto lfoAmp     = "lfoAmp";

    // Trance gate
    inline constexpr auto gateOn     = "gateOn";
    inline constexpr auto gatePattern= "gatePattern";
    inline constexpr auto gateRate   = "gateRate";
    inline constexpr auto gateDepth  = "gateDepth";
    inline constexpr auto gateSmooth = "gateSmooth";

    // Delay
    inline constexpr auto dlyDiv     = "dlyDiv";
    inline constexpr auto dlyFeedback= "dlyFeedback";
    inline constexpr auto dlyPingPong= "dlyPingPong";
    inline constexpr auto dlyTone    = "dlyTone";
    inline constexpr auto dlyMix     = "dlyMix";

    // Reverb
    inline constexpr auto revSize    = "revSize";
    inline constexpr auto revDamp    = "revDamp";
    inline constexpr auto revWidth   = "revWidth";
    inline constexpr auto revMix     = "revMix";

    // Master
    inline constexpr auto mix        = "mix";
    inline constexpr auto output     = "output";

    // Synth (shared by the three layers)
    inline constexpr auto ampAttack  = "ampAttack";
    inline constexpr auto ampDecay   = "ampDecay";
    inline constexpr auto ampSustain = "ampSustain";
    inline constexpr auto ampRelease = "ampRelease";
    inline constexpr auto vFltType   = "vFltType";
    inline constexpr auto vCutoff    = "vCutoff";
    inline constexpr auto vReso      = "vReso";
    inline constexpr auto vEnvAmt    = "vEnvAmt";
    inline constexpr auto vKeyTrack  = "vKeyTrack";
    inline constexpr auto fltAttack  = "fltAttack";
    inline constexpr auto fltDecay   = "fltDecay";
    inline constexpr auto fltSustain = "fltSustain";
    inline constexpr auto fltRelease = "fltRelease";
    inline constexpr auto voiceMode  = "voiceMode";
    inline constexpr auto glide      = "glide";
    inline constexpr auto velSens    = "velSens";
    inline constexpr auto synthTranspose = "synthTranspose";
    inline constexpr auto synthLevel = "synthLevel";

    // Sequencer (pattern on the DAW grid) + key snapping
    inline constexpr auto randOn     = "randOn";
    inline constexpr auto randRoot   = "randRoot";
    inline constexpr auto randScale  = "randScale";
    inline constexpr auto randRate   = "randRate";
    inline constexpr auto randOctaves= "randOctaves";
    inline constexpr auto randChance = "randChance";
    inline constexpr auto randLength = "randLength";
    inline constexpr auto seqSteps   = "seqSteps";
    inline constexpr auto seqSwing   = "seqSwing";
    inline constexpr auto snapKeys   = "snapKeys";

    // Arp + chord
    inline constexpr auto arpOn      = "arpOn";
    inline constexpr auto arpMode    = "arpMode";
    inline constexpr auto arpRate    = "arpRate";
    inline constexpr auto arpOctaves = "arpOctaves";
    inline constexpr auto arpGate    = "arpGate";
    inline constexpr auto arpLatch   = "arpLatch";
    inline constexpr auto chordMode  = "chordMode";

    // Extra FX
    inline constexpr auto dirtOS     = "dirtOS";
    inline constexpr auto pumpOn     = "pumpOn";
    inline constexpr auto pumpRate   = "pumpRate";
    inline constexpr auto pumpDepth  = "pumpDepth";
    inline constexpr auto pumpRelease= "pumpRelease";
    inline constexpr auto compOn     = "compOn";
    inline constexpr auto compThresh = "compThresh";
    inline constexpr auto compRatio  = "compRatio";
    inline constexpr auto compAttack = "compAttack";
    inline constexpr auto compRelease= "compRelease";
    inline constexpr auto compMakeup = "compMakeup";
    inline constexpr auto limitOn    = "limitOn";
}

// Per-layer parameters: IDs are "l1wave", "l2level" and so on.
namespace LayerParam
{
    enum Field { on, wave, sample, level, pan, octave, semi, fine, unison, detune, spread,
                 warp, speed, sync, grain, start, end, loop, reverse, numFields };

    inline const juce::String& id (int layer, Field f)
    {
        static const auto table = []
        {
            static const char* suffixes[] { "on", "wave", "sample", "level", "pan", "octave", "semi", "fine", "unison",
                                            "detune", "spread", "warp", "speed", "sync", "grain", "start", "end",
                                            "loop", "reverse" };
            std::array<std::array<juce::String, numFields>, 3> t;
            for (int l = 0; l < 3; ++l)
                for (int field = 0; field < numFields; ++field)
                    t[(size_t) l][(size_t) field] = "l" + juce::String (l + 1) + suffixes[field];
            return t;
        }();
        return table[(size_t) layer][(size_t) f];
    }
}

namespace Choices
{
    inline const juce::StringArray filterTypes { "Low Pass", "Band Pass", "High Pass" };
    inline const juce::StringArray dirtShapes  { "Soft", "Hard", "Fold", "Tube" };
    inline const juce::StringArray waves       { "Supersaw", "Saw", "Square", "Triangle", "Sine", "Noise", "Sample" };
    inline const juce::StringArray warps       { "Repitch", "Stretch", "Tempo Sync" };
    inline const juce::StringArray syncLengths { "1/2 Beat", "1 Beat", "2 Beats", "1 Bar", "2 Bars", "4 Bars", "8 Bars" };
    inline constexpr double syncLengthBeats[]  { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    inline const juce::StringArray voiceModes  { "Poly", "Mono", "Legato" };
    inline const juce::StringArray pumpRates   { "1/8", "1/4", "1/2", "1 Bar" };
    inline constexpr double pumpRateBeats[]    { 0.5, 1.0, 2.0, 4.0 };
    inline const juce::StringArray oversampling { "Off", "2x", "4x" };
    inline const juce::StringArray roots       { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray randRates   { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    inline constexpr double randRateBeats[]    { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    inline const juce::StringArray lfoShapes   { "Sine", "Triangle", "Saw", "Square", "S&H" };

    inline const juce::StringArray lfoDivs     { "4 Bars", "2 Bars", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/8D" };
    inline constexpr double lfoDivBeats[]      { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 0.75 };

    inline const juce::StringArray gatePatterns { "Pulse", "Offbeat", "Gallop", "Stutter", "Syncopate", "Builder" };
    inline const juce::StringArray gateRates    { "1/8", "1/16", "1/32" };
    inline constexpr double gateRateBeats[]     { 0.5, 0.25, 0.125 };

    inline const juce::StringArray delayDivs   { "1/32", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1/1" };
    inline constexpr double delayDivBeats[]    { 0.125, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0 };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using C = AudioParameterChoice;
    using B = AudioParameterBool;
    using Attr = AudioParameterFloatAttributes;

    auto percent = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })
                         .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto bipolar = Attr().withStringFromValueFunction ([] (float v, int) { auto p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; })
                         .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });
    auto hz = Attr().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; })
                    .withValueFromStringFunction ([] (const String& s) { return s.containsIgnoreCase ("k") ? s.getFloatValue() * 1000.0f : s.getFloatValue(); });
    auto ms = Attr().withStringFromValueFunction ([] (float v, int)
                    {
                        return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms";
                    })
                    .withValueFromStringFunction ([] (const String& s) { return s.trim().endsWith ("s") && ! s.contains ("ms") ? s.getFloatValue() * 1000.0f : s.getFloatValue(); });
    auto db = Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
                    .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });

    auto freqRange = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto add = [&p] (auto* param) { p.emplace_back (param); };

    add (new P ({ ParamIDs::power, 1 }, "Power", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent));
    add (new P ({ ParamIDs::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    add (new P ({ ParamIDs::detune, 1 }, "Detune", NormalisableRange<float> (0.0f, 50.0f), 18.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ct"; })));
    add (new P ({ ParamIDs::subOct, 1 }, "Sub Oct", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new P ({ ParamIDs::fifth, 1 }, "Fifth", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new P ({ ParamIDs::octUp, 1 }, "Oct Up", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    add (new C ({ ParamIDs::fltType, 1 }, "Filter Type", Choices::filterTypes, 0));
    add (new P ({ ParamIDs::cutoff, 1 }, "Cutoff", freqRange (20.0f, 20000.0f, 1000.0f), 20000.0f, hz));
    add (new P ({ ParamIDs::resonance, 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent));
    add (new P ({ ParamIDs::envAmount, 1 }, "Env Amount", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar));
    add (new P ({ ParamIDs::envAttack, 1 }, "Env Attack", freqRange (1.0f, 200.0f, 20.0f), 10.0f, ms));
    add (new P ({ ParamIDs::envRelease, 1 }, "Env Release", freqRange (10.0f, 1000.0f, 150.0f), 150.0f, ms));

    add (new P ({ ParamIDs::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 36.0f), 0.0f, db));
    add (new C ({ ParamIDs::shape, 1 }, "Shape", Choices::dirtShapes, 0));
    add (new P ({ ParamIDs::bits, 1 }, "Bits", NormalisableRange<float> (2.0f, 16.0f), 16.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " bit"; })));
    add (new P ({ ParamIDs::downsample, 1 }, "Downsample", NormalisableRange<float> (1.0f, 32.0f, 0.0f, 0.5f), 1.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return "x" + String (v, 1); })));
    add (new P ({ ParamIDs::dirtMix, 1 }, "Dirt Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));

    add (new C ({ ParamIDs::lfoShape, 1 }, "LFO Shape", Choices::lfoShapes, 0));
    add (new B ({ ParamIDs::lfoSync, 1 }, "LFO Sync", true));
    add (new P ({ ParamIDs::lfoRate, 1 }, "LFO Rate", freqRange (0.05f, 20.0f, 2.0f), 1.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
    add (new C ({ ParamIDs::lfoDiv, 1 }, "LFO Division", Choices::lfoDivs, 4));
    add (new P ({ ParamIDs::lfoCutoff, 1 }, "LFO > Cutoff", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar));
    add (new P ({ ParamIDs::lfoDetune, 1 }, "LFO > Detune", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new P ({ ParamIDs::lfoAmp, 1 }, "LFO > Volume", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    add (new B ({ ParamIDs::gateOn, 1 }, "Gate On", false));
    add (new C ({ ParamIDs::gatePattern, 1 }, "Gate Pattern", Choices::gatePatterns, 0));
    add (new C ({ ParamIDs::gateRate, 1 }, "Gate Rate", Choices::gateRates, 1));
    add (new P ({ ParamIDs::gateDepth, 1 }, "Gate Depth", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    add (new P ({ ParamIDs::gateSmooth, 1 }, "Gate Smooth", NormalisableRange<float> (0.0f, 1.0f), 0.2f, percent));

    add (new C ({ ParamIDs::dlyDiv, 1 }, "Delay Time", Choices::delayDivs, 5));
    add (new P ({ ParamIDs::dlyFeedback, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.4f, percent));
    add (new B ({ ParamIDs::dlyPingPong, 1 }, "Ping Pong", true));
    add (new P ({ ParamIDs::dlyTone, 1 }, "Delay Tone", freqRange (200.0f, 20000.0f, 3000.0f), 5000.0f, hz));
    add (new P ({ ParamIDs::dlyMix, 1 }, "Delay Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    add (new P ({ ParamIDs::revSize, 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
    add (new P ({ ParamIDs::revDamp, 1 }, "Reverb Damping", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent));
    add (new P ({ ParamIDs::revWidth, 1 }, "Reverb Width", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    add (new P ({ ParamIDs::revMix, 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));

    add (new P ({ ParamIDs::mix, 1 }, "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    add (new P ({ ParamIDs::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db));
    add (new C ({ ParamIDs::dirtOS, 1 }, "Dirt Oversampling", Choices::oversampling, 1));

    // Layers
    auto cents = Attr().withStringFromValueFunction ([] (float v, int) { auto c = roundToInt (v); return (c > 0 ? "+" : "") + String (c) + " ct"; });
    auto panAttr = Attr().withStringFromValueFunction ([] (float v, int)
    {
        auto amount = roundToInt (v * 50.0f);
        return amount == 0 ? String ("C") : String (std::abs (amount)) + (amount < 0 ? "L" : "R");
    });
    auto speedAttr = Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; });
    auto semis = [] (const char* unit)
    {
        return AudioParameterIntAttributes().withStringFromValueFunction ([unit] (int v, int) { return (v > 0 ? "+" : "") + String (v) + unit; });
    };
    auto lastSample = juce::jmax (0, SampleLibrary::get().size() - 1);

    for (int l = 0; l < 3; ++l)
    {
        using LP = LayerParam::Field;
        auto name = [l] (const char* n) { return "Layer " + String (l + 1) + " " + n; };
        auto pid = [l] (LP f) { return ParameterID { LayerParam::id (l, f), 1 }; };

        add (new B (pid (LP::on), name ("On"), l == 0));
        add (new C (pid (LP::wave), name ("Wave"), Choices::waves, l == 0 ? 6 : l == 1 ? 0 : 4));
        add (new AudioParameterInt (pid (LP::sample), name ("Sample"), 0, lastSample, 0,
                                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                    {
                                        const auto& e = SampleLibrary::get()[v];
                                        return e.category + " " + e.name;
                                    })));
        add (new P (pid (LP::level), name ("Level"), NormalisableRange<float> (0.0f, 1.0f), 0.8f, percent));
        add (new P (pid (LP::pan), name ("Pan"), NormalisableRange<float> (-1.0f, 1.0f), 0.0f, panAttr));
        add (new AudioParameterInt (pid (LP::octave), name ("Octave"), -3, 3, l == 2 ? -1 : 0, semis (" oct")));
        add (new AudioParameterInt (pid (LP::semi), name ("Semitone"), -12, 12, 0, semis (" st")));
        add (new P (pid (LP::fine), name ("Fine"), NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents));
        add (new AudioParameterInt (pid (LP::unison), name ("Unison"), 1, 8, l == 1 ? 7 : 1,
                                    AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + (v == 1 ? " voice" : " voices"); })));
        add (new P (pid (LP::detune), name ("Detune"), NormalisableRange<float> (0.0f, 100.0f), 20.0f,
                    Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })));
        add (new P (pid (LP::spread), name ("Spread"), NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
        add (new C (pid (LP::warp), name ("Warp"), Choices::warps, 0));
        add (new P (pid (LP::speed), name ("Speed"), freqRange (0.25f, 4.0f, 1.0f), 1.0f, speedAttr));
        add (new C (pid (LP::sync), name ("Sync Length"), Choices::syncLengths, 3));
        add (new P (pid (LP::grain), name ("Grain"), freqRange (10.0f, 250.0f, 60.0f), 60.0f, ms));
        add (new P (pid (LP::start), name ("Start"), NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
        add (new P (pid (LP::end), name ("End"), NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
        add (new B (pid (LP::loop), name ("Loop"), false));
        add (new B (pid (LP::reverse), name ("Reverse"), false));
    }

    // Shared voice
    auto longMs = [&] (float lo, float hi, float centre) { return freqRange (lo, hi, centre); };
    add (new P ({ ParamIDs::ampAttack, 1 }, "Amp Attack", longMs (0.5f, 5000.0f, 200.0f), 2.0f, ms));
    add (new P ({ ParamIDs::ampDecay, 1 }, "Amp Decay", longMs (5.0f, 5000.0f, 400.0f), 400.0f, ms));
    add (new P ({ ParamIDs::ampSustain, 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent));
    add (new P ({ ParamIDs::ampRelease, 1 }, "Amp Release", longMs (5.0f, 8000.0f, 400.0f), 250.0f, ms));
    add (new C ({ ParamIDs::vFltType, 1 }, "Synth Filter Type", Choices::filterTypes, 0));
    add (new P ({ ParamIDs::vCutoff, 1 }, "Synth Cutoff", freqRange (20.0f, 20000.0f, 1000.0f), 20000.0f, hz));
    add (new P ({ ParamIDs::vReso, 1 }, "Synth Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.1f, percent));
    add (new P ({ ParamIDs::vEnvAmt, 1 }, "Synth Filter Env", NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar));
    add (new P ({ ParamIDs::vKeyTrack, 1 }, "Synth Key Track", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new P ({ ParamIDs::fltAttack, 1 }, "Filter Attack", longMs (0.5f, 5000.0f, 200.0f), 2.0f, ms));
    add (new P ({ ParamIDs::fltDecay, 1 }, "Filter Decay", longMs (5.0f, 5000.0f, 400.0f), 400.0f, ms));
    add (new P ({ ParamIDs::fltSustain, 1 }, "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new P ({ ParamIDs::fltRelease, 1 }, "Filter Release", longMs (5.0f, 8000.0f, 400.0f), 300.0f, ms));
    add (new C ({ ParamIDs::voiceMode, 1 }, "Voice Mode", Choices::voiceModes, 0));
    add (new P ({ ParamIDs::glide, 1 }, "Glide", longMs (0.0f, 2000.0f, 150.0f), 0.0f, ms));
    add (new P ({ ParamIDs::velSens, 1 }, "Velocity", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    add (new AudioParameterInt ({ ParamIDs::synthTranspose, 1 }, "Transpose", -24, 24, 0, semis (" st")));
    add (new P ({ ParamIDs::synthLevel, 1 }, "Synth Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));

    // Sequencer
    add (new B ({ ParamIDs::randOn, 1 }, "Sequencer On", false));
    add (new C ({ ParamIDs::randRoot, 1 }, "Key", Choices::roots, 0));
    add (new C ({ ParamIDs::randScale, 1 }, "Scale", rf::Scales::names(), 1));
    add (new C ({ ParamIDs::randRate, 1 }, "Sequencer Rate", Choices::randRates, 3));
    add (new AudioParameterInt ({ ParamIDs::randOctaves, 1 }, "Sequencer Octaves", 1, 3, 1,
                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " oct"; })));
    add (new P ({ ParamIDs::randChance, 1 }, "Random Density", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    add (new P ({ ParamIDs::randLength, 1 }, "Note Length", NormalisableRange<float> (0.05f, 1.0f), 0.5f, percent));
    add (new AudioParameterInt ({ ParamIDs::seqSteps, 1 }, "Steps", 1, 16, 16));
    add (new P ({ ParamIDs::seqSwing, 1 }, "Swing", NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent));
    add (new B ({ ParamIDs::snapKeys, 1 }, "Snap Keys To Scale", false));

    // Arp + chord
    add (new B ({ ParamIDs::arpOn, 1 }, "Arp On", false));
    add (new C ({ ParamIDs::arpMode, 1 }, "Arp Mode", rf::Arp::modeNames(), 0));
    add (new C ({ ParamIDs::arpRate, 1 }, "Arp Rate", Choices::randRates, 3));
    add (new AudioParameterInt ({ ParamIDs::arpOctaves, 1 }, "Arp Octaves", 1, 4, 1,
                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + " oct"; })));
    add (new P ({ ParamIDs::arpGate, 1 }, "Arp Gate", NormalisableRange<float> (0.05f, 1.0f), 0.6f, percent));
    add (new B ({ ParamIDs::arpLatch, 1 }, "Arp Latch", false));
    add (new C ({ ParamIDs::chordMode, 1 }, "Chord", rf::NoteMapper::chordNames(), 0));

    // Pump + dynamics
    add (new B ({ ParamIDs::pumpOn, 1 }, "Pump On", false));
    add (new C ({ ParamIDs::pumpRate, 1 }, "Pump Rate", Choices::pumpRates, 1));
    add (new P ({ ParamIDs::pumpDepth, 1 }, "Pump Depth", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent));
    add (new P ({ ParamIDs::pumpRelease, 1 }, "Pump Release", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent));
    add (new B ({ ParamIDs::compOn, 1 }, "Compressor On", false));
    add (new P ({ ParamIDs::compThresh, 1 }, "Comp Threshold", NormalisableRange<float> (-48.0f, 0.0f), -12.0f, db));
    add (new P ({ ParamIDs::compRatio, 1 }, "Comp Ratio", freqRange (1.0f, 20.0f, 4.0f), 4.0f,
                Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })));
    add (new P ({ ParamIDs::compAttack, 1 }, "Comp Attack", freqRange (0.1f, 100.0f, 10.0f), 10.0f, ms));
    add (new P ({ ParamIDs::compRelease, 1 }, "Comp Release", freqRange (10.0f, 1000.0f, 120.0f), 120.0f, ms));
    add (new P ({ ParamIDs::compMakeup, 1 }, "Comp Makeup", NormalisableRange<float> (0.0f, 24.0f), 0.0f, db));
    add (new B ({ ParamIDs::limitOn, 1 }, "Limiter On", true));

    return { p.begin(), p.end() };
}
