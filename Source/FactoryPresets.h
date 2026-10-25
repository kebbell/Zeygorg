#pragma once

#include "Parameters.h"
#include "Presets.h"

// Zeygorg's built-in sounds. Each lists only what differs from the defaults.
// Sample numbers index the built-in library: Synths 0-200, Acid 201-300,
// HiString 301-310.
namespace FactoryPresets
{
inline juce::String layer (int l, LayerParam::Field f) { return LayerParam::id (l, f); }

inline std::vector<PresetManager::Factory> all (const juce::String& defaultPattern)
{
    using namespace ParamIDs;
    using LP = LayerParam::Field;

    // Wave indices from Choices::waves.
    constexpr float supersaw = 0, saw = 1, square = 2, triangle = 3, sine = 4, sampleWave = 6;

    std::vector<PresetManager::Factory> p {
        { "Init", "Basics", {}, {} },

        { "Rave Stab", "Stabs", {
            { power, 1.0f }, { width, 1.0f }, { detune, 22.0f }, { subOct, 0.35f },
            { layer (0, LP::sample), 12 }, { ampRelease, 400.0f },
            { dlyMix, 0.22f }, { dlyDiv, 5 }, { revMix, 0.22f }, { revSize, 0.7f } }, {} },

        { "Hoover", "Leads", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), supersaw },
            { layer (1, LP::unison), 8 }, { layer (1, LP::detune), 45.0f }, { layer (1, LP::spread), 1.0f },
            { layer (2, LP::on), 1 }, { layer (2, LP::wave), saw }, { layer (2, LP::octave), -1 }, { layer (2, LP::level), 0.5f },
            { voiceMode, 2 }, { glide, 140.0f }, { vCutoff, 3500.0f }, { vEnvAmt, 0.25f }, { fltDecay, 600.0f },
            { power, 0.8f }, { detune, 30.0f }, { lfoDetune, 0.45f }, { lfoDiv, 4 },
            { drive, 6.0f }, { revMix, 0.25f } }, {} },

        { "Supersaw Wall", "Pads", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::unison), 8 },
            { layer (1, LP::detune), 30.0f }, { layer (1, LP::spread), 1.0f },
            { layer (2, LP::on), 1 }, { layer (2, LP::wave), sine }, { layer (2, LP::level), 0.5f },
            { ampAttack, 8.0f }, { ampRelease, 700.0f },
            { power, 0.6f }, { width, 1.0f }, { revMix, 0.35f }, { revSize, 0.85f }, { dlyMix, 0.18f } }, {} },

        { "Pumping Chords", "Chords", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::unison), 6 }, { layer (1, LP::detune), 18.0f },
            { chordMode, 8 }, { ampDecay, 700.0f }, { ampSustain, 0.6f }, { ampRelease, 300.0f },
            { vCutoff, 5000.0f }, { pumpOn, 1 }, { pumpDepth, 0.75f }, { pumpRelease, 0.55f },
            { power, 0.4f }, { revMix, 0.3f } }, {} },

        { "Acid Line", "Sequences", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), saw }, { layer (1, LP::unison), 1 },
            { voiceMode, 2 }, { glide, 60.0f }, { vCutoff, 420.0f }, { vReso, 0.82f }, { vEnvAmt, 0.6f },
            { fltDecay, 180.0f }, { fltSustain, 0.0f }, { ampSustain, 1.0f },
            { randOn, 1 }, { randRoot, 9 }, { randLength, 0.8f },
            { drive, 12.0f }, { shape, 3 }, { power, 0.0f }, { dlyMix, 0.2f }, { dlyDiv, 5 } },
          { { "pattern", "0:110 0:80 7:100 0:90 3:120 0:80 5:100 4:90 0:110 - 7:120 3:90 0:100 5:80 3:110 2:90" } } },

        { "Trance Gate Pad", "Pads", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::unison), 7 }, { layer (1, LP::detune), 22.0f },
            { layer (2, LP::on), 1 }, { layer (2, LP::wave), triangle }, { layer (2, LP::octave), 1 }, { layer (2, LP::level), 0.4f },
            { ampAttack, 300.0f }, { ampRelease, 1200.0f },
            { gateOn, 1 }, { gatePattern, 0 }, { gateSmooth, 0.3f },
            { dlyMix, 0.3f }, { revMix, 0.4f }, { revSize, 0.8f } }, {} },

        { "Arp Rush", "Sequences", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), saw }, { layer (1, LP::unison), 3 },
            { layer (1, LP::detune), 12.0f },
            { arpOn, 1 }, { arpMode, 2 }, { arpOctaves, 2 }, { arpGate, 0.5f },
            { vCutoff, 2500.0f }, { vEnvAmt, 0.4f }, { fltDecay, 200.0f },
            { ampDecay, 250.0f }, { ampSustain, 0.4f }, { ampRelease, 150.0f },
            { dlyMix, 0.3f }, { dlyDiv, 5 }, { revMix, 0.25f }, { power, 0.5f } }, {} },

        { "Dub Chord", "Chords", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), saw }, { layer (1, LP::unison), 2 },
            { layer (1, LP::detune), 10.0f }, { chordMode, 8 },
            { vCutoff, 900.0f }, { vReso, 0.3f }, { vEnvAmt, 0.25f }, { fltDecay, 250.0f },
            { ampDecay, 250.0f }, { ampSustain, 0.0f }, { ampRelease, 200.0f },
            { dlyDiv, 8 }, { dlyFeedback, 0.7f }, { dlyMix, 0.4f }, { dlyTone, 1800.0f },
            { revMix, 0.45f }, { revSize, 0.9f }, { power, 0.0f } }, {} },

        { "Bitcrush Bass", "Bass", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), square }, { layer (1, LP::octave), -1 },
            { layer (1, LP::unison), 2 }, { layer (1, LP::detune), 8.0f },
            { layer (2, LP::on), 1 }, { layer (2, LP::level), 0.7f },
            { voiceMode, 1 }, { glide, 30.0f },
            { bits, 6.0f }, { downsample, 4.0f }, { drive, 10.0f }, { shape, 1 },
            { cutoff, 2200.0f }, { power, 0.0f } }, {} },

        { "Euphoric Lead", "Leads", {
            { layer (0, LP::sample), 303 }, { layer (0, LP::level), 0.6f },
            { layer (1, LP::on), 1 }, { layer (1, LP::unison), 7 }, { layer (1, LP::detune), 25.0f },
            { ampRelease, 500.0f }, { vCutoff, 7000.0f },
            { dlyMix, 0.3f }, { dlyDiv, 5 }, { revMix, 0.35f }, { revSize, 0.8f }, { power, 0.5f } }, {} },

        { "Stretched Strings", "Pads", {
            { layer (0, LP::sample), 305 }, { layer (0, LP::warp), 1 }, { layer (0, LP::speed), 0.5f },
            { layer (0, LP::loop), 1 }, { layer (0, LP::grain), 90.0f }, { layer (0, LP::end), 0.8f },
            { ampAttack, 400.0f }, { ampRelease, 1500.0f },
            { revMix, 0.5f }, { revSize, 0.9f }, { power, 0.4f }, { width, 1.0f } }, {} },

        { "Tempo Warp Acid", "Sequences", {
            { layer (0, LP::sample), 230 }, { layer (0, LP::warp), 2 }, { layer (0, LP::sync), 3 }, { layer (0, LP::loop), 1 },
            { layer (0, LP::grain), 50.0f },
            { gateOn, 1 }, { gatePattern, 3 }, { dlyMix, 0.2f }, { power, 0.3f } }, {} },

        { "Reverse Swell", "Pads", {
            { layer (0, LP::sample), 40 }, { layer (0, LP::reverse), 1 },
            { ampAttack, 200.0f }, { ampRelease, 800.0f },
            { pumpOn, 1 }, { pumpRate, 2 }, { pumpDepth, 0.5f },
            { revMix, 0.45f }, { revSize, 0.85f }, { power, 0.7f } }, {} },

        { "Sidechain Organ", "Chords", {
            { layer (0, LP::on), 0 }, { layer (1, LP::on), 1 }, { layer (1, LP::wave), square }, { layer (1, LP::unison), 1 },
            { layer (1, LP::level), 0.5f },
            { layer (2, LP::on), 1 }, { layer (2, LP::octave), 0 }, { layer (2, LP::level), 0.8f },
            { chordMode, 3 }, { vCutoff, 4000.0f },
            { pumpOn, 1 }, { pumpDepth, 0.7f }, { revMix, 0.2f }, { power, 0.3f } }, {} },

        { "Random Stab Seq", "Sequences", {
            { layer (0, LP::sample), 75 }, { randOn, 1 }, { randRate, 1 }, { randOctaves, 2 },
            { ampRelease, 200.0f }, { dlyMix, 0.3f }, { dlyDiv, 5 }, { revMix, 0.2f }, { power, 0.8f } },
          { { "pattern", "0:120 - 4:100 7:90 - 2:110 - 9:100 0:120 - 4:90 - 11:110 7:90 - 5:100" } } },

        { "Glue & Limit", "Basics", {
            { compOn, 1 }, { compThresh, -18.0f }, { compRatio, 3.0f }, { compMakeup, 6.0f } }, {} },
    };

    juce::ignoreUnused (sampleWave);

    // Every preset sets the pattern and drops any dragged-in files, so loading
    // one never keeps leftovers from the last.
    for (auto& preset : p)
    {
        auto has = [&preset] (const char* key)
        {
            for (auto& [k, v] : preset.properties)
                if (k == key) return true;
            return false;
        };
        if (! has ("pattern"))
            preset.properties.push_back ({ "pattern", defaultPattern });
        for (int l = 0; l < 3; ++l)
            preset.properties.push_back ({ "layerFile" + juce::String (l + 1), {} });
    }
    return p;
}
} // namespace FactoryPresets
