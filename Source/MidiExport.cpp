#include "MidiExport.h"

namespace hhbm
{
namespace
{
juce::MidiMessageSequence tempoTrack (double bpm, const Pattern& p, bool withMarkers)
{
    juce::MidiMessageSequence seq;
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm));
    tempo.setTimeStamp (0);
    seq.addEvent (tempo);
    auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
    ts.setTimeStamp (0);
    seq.addEvent (ts);

    if (withMarkers)
    {
        for (const auto& m : p.markers)
        {
            auto mk = juce::MidiMessage::textMetaEvent (6, sectionName (m.section)); // 6 = marker
            mk.setTimeStamp (m.start * MidiExport::ppq);
            seq.addEvent (mk);
        }
    }

    auto eot = juce::MidiMessage::endOfTrack();
    eot.setTimeStamp (p.lengthBeats * MidiExport::ppq);
    seq.addEvent (eot);
    return seq;
}

juce::MidiMessageSequence noteTrack (const Pattern& p, int track, bool withTempo, double bpm)
{
    juce::MidiMessageSequence seq;
    const int ch = trackChannel (track);

    auto name = juce::MidiMessage::textMetaEvent (3, trackName (track)); // 3 = track name
    name.setTimeStamp (0);
    seq.addEvent (name);

    if (withTempo)
    {
        auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm));
        tempo.setTimeStamp (0);
        seq.addEvent (tempo);
        auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4);
        ts.setTimeStamp (0);
        seq.addEvent (ts);
    }

    for (const auto& n : p.tracks[(size_t) track])
    {
        const double on  = std::round (n.start * MidiExport::ppq);
        const double off = std::max (on + 1.0, std::round ((n.start + n.length) * MidiExport::ppq));
        const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::round (n.velocity * 127.0f));
        seq.addEvent (juce::MidiMessage::noteOn (ch, n.pitch, vel), on);
        seq.addEvent (juce::MidiMessage::noteOff (ch, n.pitch), off);
    }
    seq.updateMatchedPairs();

    auto eot = juce::MidiMessage::endOfTrack();
    eot.setTimeStamp (std::max (p.lengthBeats * MidiExport::ppq, seq.getEndTime()));
    seq.addEvent (eot);
    seq.sort();
    return seq;
}
} // namespace

juce::MidiFile MidiExport::trackFile (const Pattern& p, int track, double bpm)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (noteTrack (p, track, true, bpm));
    return mf;
}

juce::MidiFile MidiExport::allTracksFile (const Pattern& p, double bpm)
{
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (ppq);
    mf.addTrack (tempoTrack (bpm, p, p.markers.size() > 1));
    for (int t = 0; t < numTracks; ++t)
        mf.addTrack (noteTrack (p, t, false, bpm));
    return mf;
}

juce::File MidiExport::exportRoot()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Hip-Hop BeatMaker")
                   .getChildFile ("Exports");
    dir.createDirectory();
    return dir;
}

bool MidiExport::write (const juce::MidiFile& mf, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream out (file);
    if (! out.openedOk()) return false;
    return mf.writeTo (out, 1);
}

juce::String MidiExport::baseName (const Settings& s, double bpm)
{
    juce::String style = juce::String (styleName (s.style)).removeCharacters (" -");
    juce::String section = juce::String (sectionName (s.section)).removeCharacters (" ");
    juce::String key = juce::String (keyName (s.key)).replace ("#", "s");
    const bool minorish = s.scale <= 3;
    return style + "_" + section + "_" + key + (minorish ? "m" : "") + "_" + juce::String ((int) std::round (bpm)) + "bpm";
}
} // namespace hhbm
