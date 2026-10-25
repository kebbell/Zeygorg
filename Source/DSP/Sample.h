#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace rf
{

// Decoded audio for one sample slot, shared between the message thread and
// the voices playing it.
struct Sample : juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Sample>;

    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    int rootNote = 60;
    juce::String name;
};

} // namespace rf
