#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleLibrary.h"

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

    // Keys (on-screen keyboard / MIDI)
    inline constexpr auto keysLevel  = "keysLevel";
    inline constexpr auto keysSource = "keysSource";
    inline constexpr auto sampleIndex= "sampleIndex";
    inline constexpr auto keysTranspose = "keysTranspose";
    inline constexpr auto keysRelease= "keysRelease";
}

namespace Choices
{
    inline const juce::StringArray filterTypes { "Low Pass", "Band Pass", "High Pass" };
    inline const juce::StringArray dirtShapes  { "Soft", "Hard", "Fold", "Tube" };
    inline const juce::StringArray keysSources { "Saw", "Samples" };
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
    auto ms = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })
                    .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue(); });
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
    add (new P ({ ParamIDs::keysLevel, 1 }, "Keys Level", NormalisableRange<float> (0.0f, 1.0f), 0.7f, percent));
    add (new C ({ ParamIDs::keysSource, 1 }, "Keys Source", Choices::keysSources, 1));
    add (new AudioParameterInt ({ ParamIDs::sampleIndex, 1 }, "Sample", 0, juce::jmax (0, SampleLibrary::get().size() - 1), 0,
                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                {
                                    const auto& e = SampleLibrary::get()[v];
                                    return e.category + " " + e.name;
                                })));
    add (new AudioParameterInt ({ ParamIDs::keysTranspose, 1 }, "Keys Transpose", -24, 24, 0,
                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                {
                                    return (v > 0 ? "+" : "") + String (v) + " st";
                                })));
    add (new P ({ ParamIDs::keysRelease, 1 }, "Keys Release", freqRange (5.0f, 2000.0f, 200.0f), 250.0f, ms));

    return { p.begin(), p.end() };
}
