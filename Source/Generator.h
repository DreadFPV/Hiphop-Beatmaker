#pragma once

#include "MusicTypes.h"

namespace hhbm
{
/** Builds the MIDI content for one section (or the full song) from the settings.
    Deterministic: the same settings + seed always give the same result on every OS. */
class Generator
{
public:
    static Pattern generate (const Settings& s);

    static int sectionBars (int section);

    /** Song order used for "Full Song". */
    static const std::vector<int>& songOrder();
};
} // namespace hhbm
