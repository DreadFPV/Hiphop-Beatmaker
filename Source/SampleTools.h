#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace hhbm::SampleTools
{
/** Finds a note name like "C1", "F#2", "Bb-1" in a file name. Returns MIDI note or -1. */
int rootFromName (const juce::String& fileNameWithoutExtension);

/** Estimates the pitch of a tonal sample (McLeod pitch method).
    Returns a fractional MIDI note, or -1 when no stable pitch was found. */
float detectPitch (const juce::AudioBuffer<float>& buffer, double sampleRate);

juce::String noteName (int midiNote); // "C3" with C3 = 60 (Cubase convention)
} // namespace hhbm::SampleTools
