#pragma once

#include <array>
#include <vector>
#include <cstdint>

namespace hhbm
{
// ---------------------------------------------------------------- enums
enum class Style   { BoomBap = 0, Trap, Drill, LoFi, GFunk, count };
enum class Section { Intro = 0, Verse, Hook, Bridge, Outro, FullSong, count };
enum class Track   { Drums = 0, Bass, Keys, Pad, Lead, count };

constexpr int numStyles   = (int) Style::count;
constexpr int numSections = (int) Section::count;
constexpr int numTracks   = (int) Track::count;

inline const char* styleName (int i)
{
    static const char* n[] = { "Boom Bap", "Trap", "Drill", "Lo-Fi", "G-Funk" };
    return n[i];
}

inline const char* sectionName (int i)
{
    static const char* n[] = { "Intro", "Verse", "Hook", "Bridge", "Outro", "Full Song" };
    return n[i];
}

inline const char* trackName (int i)
{
    static const char* n[] = { "Drums", "Bass", "Keys", "Pad", "Lead" };
    return n[i];
}

// MIDI channel (1-based) per track in exports and MIDI out
inline int trackChannel (int t)
{
    static const int ch[] = { 10, 1, 2, 3, 4 };
    return ch[t];
}

inline const char* keyName (int i)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((i % 12) + 12) % 12];
}

// ---------------------------------------------------------------- scales
constexpr int numScales = 6;

inline const char* scaleName (int i)
{
    static const char* n[] = { "Minor", "Harmonic Minor", "Dorian", "Phrygian", "Major", "Mixolydian" };
    return n[i];
}

inline const std::array<int, 7>& scaleSteps (int i)
{
    static const std::array<int, 7> s[] = {
        { 0, 2, 3, 5, 7, 8, 10 },   // natural minor
        { 0, 2, 3, 5, 7, 8, 11 },   // harmonic minor
        { 0, 2, 3, 5, 7, 9, 10 },   // dorian
        { 0, 1, 3, 5, 7, 8, 10 },   // phrygian
        { 0, 2, 4, 5, 7, 9, 11 },   // major
        { 0, 2, 4, 5, 7, 9, 10 },   // mixolydian
    };
    return s[i];
}

// ---------------------------------------------------------------- chord progressions
// Scale degrees (0-based), one chord per bar, repeated over the section.
constexpr int numProgressions = 7; // index 0 = "Auto (Style)"

inline const char* progressionName (int i)
{
    static const char* n[] = { "Auto (Style)", "i - VI - III - VII", "i - iv - v - i",
                               "i - VI - iv - v", "ii - V - I - vi", "i - VII - VI - VII",
                               "i - i - VI - V" };
    return n[i];
}

inline const std::array<int, 4>& progressionDegrees (int i)
{
    static const std::array<int, 4> p[] = {
        { 0, 5, 2, 6 }, // placeholder for auto
        { 0, 5, 2, 6 },
        { 0, 3, 4, 0 },
        { 0, 5, 3, 4 },
        { 1, 4, 0, 5 },
        { 0, 6, 5, 6 },
        { 0, 0, 5, 4 },
    };
    return p[i];
}

// ---------------------------------------------------------------- drum map (General MIDI)
namespace Drum
{
    constexpr int Kick = 36, Rim = 37, Snare = 38, Clap = 39,
                  ClosedHat = 42, OpenHat = 46, Crash = 49, Shaker = 70;
}

constexpr int numDrumSlots = 8;

inline const char* drumSlotName (int i)
{
    static const char* n[] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Rim", "Shaker", "Crash" };
    return n[i];
}

inline int drumSlotNote (int i)
{
    static const int n[] = { Drum::Kick, Drum::Snare, Drum::Clap, Drum::ClosedHat,
                             Drum::OpenHat, Drum::Rim, Drum::Shaker, Drum::Crash };
    return n[i];
}

inline int drumNoteToSlot (int note)
{
    for (int i = 0; i < numDrumSlots; ++i)
        if (drumSlotNote (i) == note)
            return i;
    return -1;
}

// ---------------------------------------------------------------- pattern data
struct Note
{
    int pitch = 60;
    double start = 0.0;    // in quarter-note beats
    double length = 0.25;  // in beats
    float velocity = 0.8f; // 0..1
};

struct SectionMarker
{
    int section = 0;
    double start = 0.0;
    double length = 0.0;
};

struct Pattern
{
    double lengthBeats = 16.0;
    std::array<std::vector<Note>, numTracks> tracks;
    std::vector<SectionMarker> markers; // filled for Full Song
};

struct Settings
{
    int style = 0;
    int section = 1;
    int key = 9;          // A
    int scale = 0;
    int progression = 0;
    float swing = 0.5f;     // 0..1 (amount of the style's swing)
    float variation = 0.3f; // 0..1
    float humanize = 0.3f;  // 0..1
    int seed = 1;
};

inline double defaultBpm (int style)
{
    static const double b[] = { 90.0, 140.0, 142.0, 80.0, 94.0 };
    return b[style];
}
} // namespace hhbm
