#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "MusicTypes.h"

namespace hhbm
{
struct MidiExport
{
    static constexpr int ppq = 960;

    /** One track (Drums, Bass, ...) as its own MIDI file. */
    static juce::MidiFile trackFile (const Pattern& p, int track, double bpm);

    /** All tracks in one Type-1 file (tempo track + one track per instrument + section markers). */
    static juce::MidiFile allTracksFile (const Pattern& p, double bpm);

    /** ~/Documents/Hip-Hop BeatMaker/Exports */
    static juce::File exportRoot();

    static bool write (const juce::MidiFile& mf, const juce::File& file);

    /** e.g. "BoomBap_Hook_Am_92bpm" */
    static juce::String baseName (const Settings& s, double bpm);
};
} // namespace hhbm
