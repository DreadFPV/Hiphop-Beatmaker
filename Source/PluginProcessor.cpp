#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hhbm;

namespace
{
const char* genParams[] = { "style", "section", "key", "scale", "progression", "swing", "variation", "humanize", "seed" };

juce::StringArray namesOf (const char* (*fn) (int), int count)
{
    juce::StringArray a;
    for (int i = 0; i < count; ++i) a.add (fn (i));
    return a;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout HipHopBeatMakerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "style", 1 }, "Style", namesOf (styleName, numStyles), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "section", 1 }, "Section", namesOf (sectionName, numSections), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", namesOf (keyName, 12), 9));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", namesOf (scaleName, numScales), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "progression", 1 }, "Chords", namesOf (progressionName, numProgressions), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "tuning", 1 }, "Tuning", StringArray { "440 Hz", "432 Hz" }, 0));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (0, 1), 0.5f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "variation", 1 }, "Variation", NormalisableRange<float> (0, 1), 0.35f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize", NormalisableRange<float> (0, 1), 0.3f));
    l.add (std::make_unique<AudioParameterInt>   (ParameterID { "seed", 1 }, "Seed", 1, 9999, 1));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bpm", 1 }, "Preview BPM", NormalisableRange<float> (60, 180, 1), 90));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sound", 1 }, "Sounds On", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "midiout", 1 }, "MIDI Out", false));

    for (int t = 0; t < numTracks; ++t)
    {
        const String n (trackName (t));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "vol" + String (t), 1 }, n + " Volume", NormalisableRange<float> (0, 1.5f), 1.0f));
        l.add (std::make_unique<AudioParameterBool>  (ParameterID { "mute" + String (t), 1 }, n + " Mute", false));
    }
    return l;
}

HipHopBeatMakerProcessor::HipHopBeatMakerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HHBM", createLayout())
{
    formats.registerBasicFormats();

    pStyle   = apvts.getRawParameterValue ("style");
    pTuning  = apvts.getRawParameterValue ("tuning");
    pBpm     = apvts.getRawParameterValue ("bpm");
    pSound   = apvts.getRawParameterValue ("sound");
    pMidiOut = apvts.getRawParameterValue ("midiout");
    for (int t = 0; t < numTracks; ++t)
    {
        pVol[(size_t) t]  = apvts.getRawParameterValue ("vol" + juce::String (t));
        pMute[(size_t) t] = apvts.getRawParameterValue ("mute" + juce::String (t));
    }

    for (auto* id : genParams)
        apvts.addParameterListener (id, this);

    lastStyle = (int) pStyle->load();
    regenerateNow();
}

HipHopBeatMakerProcessor::~HipHopBeatMakerProcessor()
{
    cancelPendingUpdate();
    for (auto* id : genParams)
        apvts.removeParameterListener (id, this);
}

bool HipHopBeatMakerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void HipHopBeatMakerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    engine.setStyle ((int) pStyle->load());
    pending.reserve (8192);
    for (auto& h : held) h.reset();
    wasRunning = false;
    expectedNextBeat = -1;
}

// ============================================================ generation
Settings HipHopBeatMakerProcessor::currentSettings() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Settings s;
    s.style       = (int) v ("style");
    s.section     = (int) v ("section");
    s.key         = (int) v ("key");
    s.scale       = (int) v ("scale");
    s.progression = (int) v ("progression");
    s.swing       = v ("swing");
    s.variation   = v ("variation");
    s.humanize    = v ("humanize");
    s.seed        = (int) v ("seed");
    return s;
}

void HipHopBeatMakerProcessor::regenerateNow()
{
    auto c = std::make_shared<Compiled>();
    auto pat = std::make_shared<Pattern> (Generator::generate (currentSettings()));
    const double len = pat->lengthBeats;

    for (int t = 0; t < numTracks; ++t)
    {
        for (const auto& n : pat->tracks[(size_t) t])
        {
            c->events.push_back ({ n.start, t, n.pitch, n.velocity, true });
            c->events.push_back ({ std::min (n.start + n.length, len - 1.0e-6), t, n.pitch, 0.0f, false });
        }
    }
    std::sort (c->events.begin(), c->events.end(), [] (const Event& a, const Event& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return ! a.on && b.on; // note-offs first
    });
    c->pattern = std::move (pat);

    std::shared_ptr<const Compiled> old;
    {
        const juce::SpinLock::ScopedLockType sl (patternLock);
        old = compiled;
        compiled = c;
    }
    if (old) graveyard.push_back (old);
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(),
                                     [] (const auto& p) { return p.use_count() <= 1; }),
                     graveyard.end());
    ++patternVersion;
}

std::shared_ptr<const Pattern> HipHopBeatMakerProcessor::getPattern() const
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    return compiled ? compiled->pattern : nullptr;
}

void HipHopBeatMakerProcessor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void HipHopBeatMakerProcessor::handleAsyncUpdate()
{
    const int style = (int) pStyle->load();
    if (style != lastStyle)
    {
        lastStyle = style;
        if (auto* p = apvts.getParameter ("bpm"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) defaultBpm (style)));
    }
    regenerateNow();
}

void HipHopBeatMakerProcessor::reroll()
{
    if (auto* p = apvts.getParameter ("seed"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::Random::getSystemRandom().nextInt ({ 1, 10000 })));
}

// ============================================================ audio
void HipHopBeatMakerProcessor::releaseHeld (juce::MidiBuffer& midi, int offset)
{
    const bool midiOut = pMidiOut->load() > 0.5f;
    for (int t = 0; t < numTracks; ++t)
    {
        for (int p = 0; p < 128; ++p)
        {
            if (! held[(size_t) t][(size_t) p]) continue;
            engine.noteOff (t, p);
            if (midiOut) midi.addEvent (juce::MidiMessage::noteOff (trackChannel (t), p), offset);
        }
        held[(size_t) t].reset();
    }
    engine.allNotesOff();
}

void HipHopBeatMakerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();

    {
        const juce::SpinLock::ScopedTryLockType tl (patternLock);
        if (tl.isLocked()) audioCompiled = compiled;
    }
    const Compiled* c = audioCompiled.get();
    if (c != lastCompiled)
    {
        releaseHeld (midi, 0);
        lastCompiled = c;
    }

    engine.setStyle ((int) pStyle->load());
    engine.setTuning (pTuning->load() > 0.5f ? 432.0f : 440.0f);
    for (int t = 0; t < numTracks; ++t)
        engine.setTrackGain (t, pMute[(size_t) t]->load() > 0.5f ? 0.0f : pVol[(size_t) t]->load());

    // ---------------------------------------------------- transport
    double bpm = pBpm->load();
    bool hostPlaying = false;
    double ppq = 0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) { bpm = *b; hostBpm = *b; }
            if (auto p = pos->getPpqPosition()) { ppq = *p; hostPlaying = pos->getIsPlaying(); }
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);

    const double beatsPerSample = bpm / 60.0 / sr;
    const double blockBeats = n * beatsPerSample;

    bool running = false;
    double startBeat = 0;
    if (hostPlaying)              { running = true; startBeat = ppq; }
    else if (previewPlaying.load()) { running = true; startBeat = internalBeat; internalBeat += blockBeats; }

    const bool soundOn = pSound->load() > 0.5f;

    if (! running || c == nullptr || c->pattern->lengthBeats <= 0)
    {
        if (wasRunning) releaseHeld (midi, 0);
        wasRunning = false;
        expectedNextBeat = -1;
        uiBeat = -1.0;
        if (! previewPlaying.load()) internalBeat = 0;
        engine.render (buffer, 0, n); // let tails ring out
        if (! soundOn) buffer.clear();
        return;
    }

    if (wasRunning && std::abs (startBeat - expectedNextBeat) > 0.01)
        releaseHeld (midi, 0); // host jumped
    wasRunning = true;
    expectedNextBeat = startBeat + blockBeats;

    const double len = c->pattern->lengthBeats;
    double cur = std::fmod (startBeat, len);
    if (cur < 0) cur += len;
    uiBeat = cur;

    // ---------------------------------------------------- collect events in this block
    pending.clear();
    double remaining = blockBeats, sampleBase = 0;
    int guard = 0;
    while (remaining > 1.0e-12 && guard++ < 8)
    {
        const double segEnd = std::min (len, cur + remaining);
        auto it = std::lower_bound (c->events.begin(), c->events.end(), cur,
                                    [] (const Event& e, double b) { return e.beat < b; });
        for (; it != c->events.end() && it->beat < segEnd; ++it)
        {
            int off = (int) (sampleBase + (it->beat - cur) / beatsPerSample);
            pending.push_back ({ juce::jlimit (0, n - 1, off), &*it });
        }
        const double used = segEnd - cur;
        sampleBase += used / beatsPerSample;
        remaining -= used;
        cur = segEnd >= len - 1.0e-12 ? 0.0 : segEnd;
    }

    // ---------------------------------------------------- play
    const bool midiOut = pMidiOut->load() > 0.5f;
    int pos = 0;
    for (const auto& pe : pending)
    {
        if (pe.first > pos)
        {
            engine.render (buffer, pos, pe.first - pos);
            pos = pe.first;
        }
        const Event& e = *pe.second;
        auto& h = held[(size_t) e.track];
        const bool muted = pMute[(size_t) e.track]->load() > 0.5f;
        const int ch = trackChannel (e.track);

        if (e.on)
        {
            if (h[(size_t) e.pitch])
            {
                engine.noteOff (e.track, e.pitch);
                if (midiOut && ! muted) midi.addEvent (juce::MidiMessage::noteOff (ch, e.pitch), pe.first);
            }
            engine.noteOn (e.track, e.pitch, e.vel);
            h.set ((size_t) e.pitch);
            if (midiOut && ! muted)
                midi.addEvent (juce::MidiMessage::noteOn (ch, e.pitch, (juce::uint8) juce::jlimit (1, 127, (int) (e.vel * 127))), pe.first);
        }
        else if (h[(size_t) e.pitch])
        {
            engine.noteOff (e.track, e.pitch);
            h.reset ((size_t) e.pitch);
            if (midiOut && ! muted) midi.addEvent (juce::MidiMessage::noteOff (ch, e.pitch), pe.first);
        }
    }
    if (pos < n) engine.render (buffer, pos, n - pos);

    if (! soundOn) buffer.clear();
}

// ============================================================ export
double HipHopBeatMakerProcessor::exportBpm() const
{
    const double h = hostBpm.load();
    return h > 0 ? h : (double) pBpm->load();
}

juce::File HipHopBeatMakerProcessor::exportSet (bool fullSong)
{
    auto s = currentSettings();
    if (fullSong) s.section = (int) Section::FullSong;
    const auto p = Generator::generate (s);
    const double bpm = exportBpm();
    const auto base = MidiExport::baseName (s, bpm);

    auto folder = MidiExport::exportRoot().getNonexistentChildFile (base, {}, false);
    folder.createDirectory();

    for (int t = 0; t < numTracks; ++t)
        if (! p.tracks[(size_t) t].empty())
            MidiExport::write (MidiExport::trackFile (p, t, bpm), folder.getChildFile (base + "_" + trackName (t) + ".mid"));

    MidiExport::write (MidiExport::allTracksFile (p, bpm), folder.getChildFile (base + "_AllTracks.mid"));
    return folder;
}

juce::File HipHopBeatMakerProcessor::writeDragFile (int track)
{
    auto pat = getPattern();
    if (pat == nullptr) return {};
    const auto s = currentSettings();
    const double bpm = exportBpm();
    auto dir = MidiExport::exportRoot().getChildFile ("_Drag");
    dir.createDirectory();

    const juce::String suffix = track < 0 ? juce::String ("AllTracks") : juce::String (trackName (track));
    auto file = dir.getChildFile (MidiExport::baseName (s, bpm) + "_" + suffix + ".mid");
    const auto mf = track < 0 ? MidiExport::allTracksFile (*pat, bpm) : MidiExport::trackFile (*pat, track, bpm);
    return MidiExport::write (mf, file) ? file : juce::File();
}

// ============================================================ samples
bool HipHopBeatMakerProcessor::loadSample (int slot, const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 20.0);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len <= 1) return false;

    auto ds = std::make_shared<DrumSample>();
    ds->data.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&ds->data, 0, len, 0, true, true);
    ds->sampleRate = reader->sampleRate;
    ds->name = file.getFileNameWithoutExtension();

    // normalise peaks above 0 dBFS
    const float peak = ds->data.getMagnitude (0, len);
    if (peak > 1.0f) ds->data.applyGain (1.0f / peak);

    engine.setSample (slot, ds);
    apvts.state.getOrCreateChildWithName ("Samples", nullptr)
        .setProperty ("slot" + juce::String (slot), file.getFullPathName(), nullptr);
    return true;
}

void HipHopBeatMakerProcessor::clearSample (int slot)
{
    engine.setSample (slot, nullptr);
    apvts.state.getOrCreateChildWithName ("Samples", nullptr).removeProperty ("slot" + juce::String (slot), nullptr);
}

juce::String HipHopBeatMakerProcessor::sampleName (int slot) const
{
    auto s = engine.getSample (slot);
    return s ? s->name : juce::String();
}

void HipHopBeatMakerProcessor::reloadSamplesFromState()
{
    auto node = apvts.state.getChildWithName ("Samples");
    for (int i = 0; i < numDrumSlots; ++i)
    {
        engine.setSample (i, nullptr);
        if (! node.isValid()) continue;
        const juce::File f (node.getProperty ("slot" + juce::String (i)).toString());
        if (f.existsAsFile()) loadSample (i, f);
    }
}

// ============================================================ state
void HipHopBeatMakerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HipHopBeatMakerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            lastStyle = (int) pStyle->load();
            reloadSamplesFromState();
            regenerateNow();
        }
    }
}

juce::AudioProcessorEditor* HipHopBeatMakerProcessor::createEditor()
{
    return new HipHopBeatMakerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HipHopBeatMakerProcessor();
}
