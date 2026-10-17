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
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "loop", 1 }, "Preview Loop", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "kbd", 1 }, "Keyboard Target",
        StringArray { "Auto (MIDI-Kanal)", "Drums", "Bass", "Keys", "Pad", "Lead" }, 3));

    for (int t = 0; t < numTracks; ++t)
    {
        const String n (trackName (t));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "vol" + String (t), 1 }, n + " Volume", NormalisableRange<float> (0, 1.5f), 1.0f));
        l.add (std::make_unique<AudioParameterBool>  (ParameterID { "mute" + String (t), 1 }, n + " Mute", false));
    }

    // synth editor parameters (defaults = Boom Bap sound)
    for (int t = 1; t < numTracks; ++t)
    {
        const auto patch = SynthEngine::stylePatch (0, t);
        for (int i = 0; i < numSynthParams; ++i)
        {
            const auto& d = synthParamDef (i);
            const ParameterID pid { synthParamId (t, i), 1 };
            const String name = String (trackName (t)) + " " + d.name;
            const float def = getPatchValue (patch, i);
            if (d.kind == 1)
            {
                StringArray waves;
                for (int w = 0; w < 5; ++w) waves.add (oscName (w));
                l.add (std::make_unique<AudioParameterChoice> (pid, name, waves, (int) def));
            }
            else if (d.kind == 2)
                l.add (std::make_unique<AudioParameterInt> (pid, name, (int) d.min, (int) d.max, (int) def));
            else
            {
                NormalisableRange<float> r (d.min, d.max);
                if (d.skewCentre > 0) r.setSkewForCentre (d.skewCentre);
                const String unit (d.unit);
                l.add (std::make_unique<AudioParameterFloat> (pid, name, r, def, AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([unit] (float v, int)
                    {
                        if (unit == "s")  return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
                        if (unit == "Hz") return v < 1000.0f ? String (v, v < 10.0f ? 1 : 0) + " Hz" : String (v / 1000.0f, 1) + " kHz";
                        if (unit.isEmpty()) return String (v, 2);
                        return String (v, 1) + " " + unit;
                    })));
            }
        }
    }

    for (int slot = 0; slot < numDrumSlots; ++slot)
    {
        for (int i = 0; i < numDrumParams; ++i)
        {
            const auto& d = drumParamDef (i);
            NormalisableRange<float> r (d.min, d.max);
            if (i == dpDecay) r.setSkewForCentre (1.0f);
            const String unit (d.unit);
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { drumParamId (slot, i), 1 },
                String (drumSlotName (slot)) + " " + d.name, r, drumDefault (i), AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([unit] (float v, int)
                    {
                        if (unit == "st") return (v > 0 ? "+" : "") + String (v, 1) + " st";
                        if (unit == "x")  return String (v, 2) + "x";
                        return String (v, 2);
                    })));
        }
    }

    // instrument sample slots
    for (int t = 1; t < numTracks; ++t)
    {
        const String n = String (trackName (t)) + " Sample ";
        auto id = [t] (const char* w) { return ParameterID { sampleParamId (t, w), 1 }; };
        l.add (std::make_unique<AudioParameterInt> (id ("root"), n + "Root", 0, 127, 60, AudioParameterIntAttributes()
                   .withStringFromValueFunction ([] (int v, int) { return SampleTools::noteName (v); })));
        auto secs = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s";
        });
        l.add (std::make_unique<AudioParameterFloat> (id ("fine"), n + "Fine", NormalisableRange<float> (-100, 100, 1), 0,
                   AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ct"; })));
        l.add (std::make_unique<AudioParameterFloat> (id ("att"), n + "Attack", NormalisableRange<float> (0.001f, 2.0f, 0, 0.35f), 0.002f, secs));
        l.add (std::make_unique<AudioParameterFloat> (id ("rel"), n + "Release", NormalisableRange<float> (0.01f, 4.0f, 0, 0.35f), t == 3 ? 0.8f : 0.15f, secs));
        l.add (std::make_unique<AudioParameterBool> (id ("loop"), n + "Loop", t == 3));
        auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
        l.add (std::make_unique<AudioParameterFloat> (id ("ls"), n + "Loop Start", NormalisableRange<float> (0, 0.98f), 0.3f, pct));
        l.add (std::make_unique<AudioParameterFloat> (id ("le"), n + "Loop End", NormalisableRange<float> (0.02f, 1.0f), 0.9f, pct));
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
    pLoop    = apvts.getRawParameterValue ("loop");
    pKbd     = apvts.getRawParameterValue ("kbd");
    for (int t = 0; t < numTracks; ++t)
    {
        pVol[(size_t) t]  = apvts.getRawParameterValue ("vol" + juce::String (t));
        pMute[(size_t) t] = apvts.getRawParameterValue ("mute" + juce::String (t));
        if (trackTakesSample (t))
        {
            auto& sp = pSmp[(size_t) t];
            sp.root = apvts.getRawParameterValue (sampleParamId (t, "root"));
            sp.fine = apvts.getRawParameterValue (sampleParamId (t, "fine"));
            sp.att  = apvts.getRawParameterValue (sampleParamId (t, "att"));
            sp.rel  = apvts.getRawParameterValue (sampleParamId (t, "rel"));
            sp.loop = apvts.getRawParameterValue (sampleParamId (t, "loop"));
            sp.ls   = apvts.getRawParameterValue (sampleParamId (t, "ls"));
            sp.le   = apvts.getRawParameterValue (sampleParamId (t, "le"));
        }
    }

    for (int t = 1; t < numTracks; ++t)
        for (int i = 0; i < numSynthParams; ++i)
            pSyn[(size_t) t][(size_t) i] = apvts.getRawParameterValue (synthParamId (t, i));
    for (int slot = 0; slot < numDrumSlots; ++slot)
        for (int i = 0; i < numDrumParams; ++i)
            pDrum[(size_t) slot][(size_t) i] = apvts.getRawParameterValue (drumParamId (slot, i));
    apvts.state.setProperty ("soundVersion", 1, nullptr);

    for (auto* id : genParams)
        apvts.addParameterListener (id, this);

    lastStyle = (int) pStyle->load();
    regenerateNow();
}

HipHopBeatMakerProcessor::~HipHopBeatMakerProcessor()
{
    renderPool.removeAllJobs (true, 10000);
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
    live.reserve (512);
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
    auto pat = std::make_shared<Pattern> (Generator::generate (currentSettings(), &edits));
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
        const int from = lastStyle;
        lastStyle = style;
        if (auto* p = apvts.getParameter ("bpm"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) defaultBpm (style)));

        const int soundOwner = pendingStyleChange.from >= 0 ? pendingStyleChange.from : from;
        if (! isSoundModified (soundOwner))
        {
            pendingStyleChange = {};
            loadStyleSound (style);
        }
        else if (editorOpen.load())
        {
            pendingStyleChange = { soundOwner, style }; // editor asks the user
        }
        else
        {
            // nobody to ask (automation): keep a backup, then load the style sound
            saveProfile (juce::String ("Auto-Backup ") + styleName (soundOwner) + " "
                         + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), soundOwner);
            pendingStyleChange = {};
            loadStyleSound (style);
        }
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

void HipHopBeatMakerProcessor::applyParams (SynthEngine& e) const
{
    e.setStyle ((int) pStyle->load());
    e.setTuning (pTuning->load() > 0.5f ? 432.0f : 440.0f);
    for (int t = 0; t < numTracks; ++t)
        e.setTrackGain (t, pMute[(size_t) t]->load() > 0.5f ? 0.0f : pVol[(size_t) t]->load());
    for (int t = 1; t < numTracks; ++t)
    {
        Patch patch;
        for (int i = 0; i < numSynthParams; ++i)
            setPatchValue (patch, i, pSyn[(size_t) t][(size_t) i]->load());
        e.setPatch (t, patch);
    }
    for (int slot = 0; slot < numDrumSlots; ++slot)
    {
        const auto& d = pDrum[(size_t) slot];
        e.setDrumTweak (slot, { d[dpTune]->load(), d[dpDecay]->load(), d[dpTone]->load(), d[dpLevel]->load() });
    }
    for (int t = 1; t < numTracks; ++t)
    {
        const auto& sp = pSmp[(size_t) t];
        SampleParams prm;
        prm.root = sp.root->load();
        prm.fineCents = sp.fine->load();
        prm.attack = sp.att->load();
        prm.release = sp.rel->load();
        prm.loop = sp.loop->load() > 0.5f;
        prm.loopStart = sp.ls->load();
        prm.loopEnd = juce::jmax (sp.le->load(), prm.loopStart + 0.01f);
        e.setSampleParams (t, prm);
    }
}

int HipHopBeatMakerProcessor::keyboardTrack (int midiChannel) const
{
    const int k = (int) pKbd->load();
    if (k > 0) return k - 1;
    switch (midiChannel)
    {
        case 10: return (int) Track::Drums;
        case 1:  return (int) Track::Bass;
        case 3:  return (int) Track::Pad;
        case 4:  return (int) Track::Lead;
        default: return (int) Track::Keys;
    }
}

void HipHopBeatMakerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // ---------------------------------------------------- live MIDI (keyboard)
    live.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (live.size() >= live.capacity()) break;
        if (m.isNoteOn())
            live.push_back ({ (double) meta.samplePosition, keyboardTrack (m.getChannel()), m.getNoteNumber(), m.getFloatVelocity(), true, true });
        else if (m.isNoteOff())
            live.push_back ({ (double) meta.samplePosition, keyboardTrack (m.getChannel()), m.getNoteNumber(), 0.0f, false, true });
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            engine.allNotesOff();
    }
    if (! live.empty()) lastLiveNote = (int) live.back().pitch;
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

    applyParams (engine);

    if (restartRequested.exchange (false))
    {
        internalBeat = 0;
        releaseHeld (midi, 0);
    }

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
    const double len = c != nullptr ? c->pattern->lengthBeats : 0.0;
    const bool loopPreview = pLoop->load() > 0.5f;

    bool running = false;
    double startBeat = 0;
    if (hostPlaying) { running = true; startBeat = ppq; }
    else if (previewPlaying.load())
    {
        if (! loopPreview && len > 0 && internalBeat >= len)
            previewPlaying = false; // played once to the end
        else
        {
            running = true;
            startBeat = internalBeat;
            internalBeat += blockBeats;
        }
    }

    pending.clear();

    if (! running || c == nullptr || len <= 0)
    {
        if (wasRunning) releaseHeld (midi, 0);
        wasRunning = false;
        expectedNextBeat = -1;
        uiBeat = -1.0;
        if (! previewPlaying.load()) internalBeat = 0;
    }
    else
    {
        if (wasRunning && std::abs (startBeat - expectedNextBeat) > 0.01)
            releaseHeld (midi, 0); // host jumped
        wasRunning = true;
        expectedNextBeat = startBeat + blockBeats;

        const bool once = ! hostPlaying && ! loopPreview;
        double cur = once ? startBeat : std::fmod (startBeat, len);
        if (cur < 0) cur += len;
        uiBeat = cur;

        double remaining = once ? std::min (blockBeats, len - cur) : blockBeats;
        double sampleBase = 0;
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
    }

    // merge live keyboard notes (pending stays sorted by offset)
    for (const auto& e : live)
    {
        const int off = juce::jlimit (0, n - 1, (int) e.beat);
        auto it = std::upper_bound (pending.begin(), pending.end(), off,
                                    [] (int o, const std::pair<int, const Event*>& p) { return o < p.first; });
        pending.insert (it, { off, &e });
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

        if (e.live)
        {
            if (e.on) engine.noteOn (e.track, e.pitch, e.vel);
            else      engine.noteOff (e.track, e.pitch);
            continue;
        }

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

    if (! soundOnParam()) buffer.clear();
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
    const auto p = Generator::generate (s, &edits);
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

// ============================================================ note editing
std::vector<Note> HipHopBeatMakerProcessor::sectionNotes (int section, int track) const
{
    if (edits.has (section, track))
        return *edits.notes[(size_t) section][(size_t) track];
    auto s = currentSettings();
    s.section = section;
    return Generator::generate (s).tracks[(size_t) track];
}

void HipHopBeatMakerProcessor::applyOverride (int section, int track, std::shared_ptr<const std::vector<Note>> notes)
{
    edits.notes[(size_t) section][(size_t) track] = std::move (notes);
    storeEdit (section, track);
    regenerateNow();
}

void HipHopBeatMakerProcessor::setEditedNotes (int section, int track, std::vector<Note> notes)
{
    auto before = edits.notes[(size_t) section][(size_t) track];
    const double len = Generator::sectionBars (section) * 4.0;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [len] (const Note& n)
                 { return n.start < 0 || n.start >= len || n.pitch < 0 || n.pitch > 127; }), notes.end());
    for (auto& n : notes)
    {
        n.length = juce::jlimit (0.01, len - n.start, n.length);
        n.velocity = juce::jlimit (0.01f, 1.0f, n.velocity);
    }
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });
    auto after = std::make_shared<const std::vector<Note>> (std::move (notes));
    undoStack.push_back ({ section, track, before, after });
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    redoStack.clear();
    applyOverride (section, track, after);
}

void HipHopBeatMakerProcessor::unlockTrack (int section, int track)
{
    auto before = edits.notes[(size_t) section][(size_t) track];
    if (before == nullptr) return;
    undoStack.push_back ({ section, track, before, nullptr });
    redoStack.clear();
    applyOverride (section, track, nullptr);
}

bool HipHopBeatMakerProcessor::undoEdit()
{
    if (undoStack.empty()) return false;
    auto a = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back (a);
    applyOverride (a.section, a.track, a.before);
    lastUndoTarget = { a.section, a.track };
    return true;
}

bool HipHopBeatMakerProcessor::redoEdit()
{
    if (redoStack.empty()) return false;
    auto a = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (a);
    applyOverride (a.section, a.track, a.after);
    lastUndoTarget = { a.section, a.track };
    return true;
}

bool HipHopBeatMakerProcessor::isLocked (int section, int track) const
{
    if (section == (int) Section::FullSong)
    {
        for (int s = 0; s < 5; ++s)
            if (edits.has (s, track)) return true;
        return false;
    }
    return edits.has (section, track);
}

void HipHopBeatMakerProcessor::storeEdit (int section, int track)
{
    auto node = apvts.state.getOrCreateChildWithName ("Edits", nullptr);
    const juce::String key = "s" + juce::String (section) + "t" + juce::String (track);
    const auto& n = edits.notes[(size_t) section][(size_t) track];
    if (n == nullptr) { node.removeProperty (key, nullptr); return; }
    juce::String txt;
    for (const auto& x : *n)
        txt << x.pitch << " " << juce::String (x.start, 5) << " " << juce::String (x.length, 5) << " " << juce::String (x.velocity, 3) << ";";
    node.setProperty (key, txt.isEmpty() ? juce::String ("-") : txt, nullptr);
}

void HipHopBeatMakerProcessor::loadEditsFromState()
{
    edits = {};
    undoStack.clear();
    redoStack.clear();
    auto node = apvts.state.getChildWithName ("Edits");
    if (! node.isValid()) return;
    for (int s = 0; s < 5; ++s)
        for (int t = 0; t < numTracks; ++t)
        {
            const juce::String key = "s" + juce::String (s) + "t" + juce::String (t);
            if (! node.hasProperty (key)) continue;
            std::vector<Note> notes;
            for (auto& item : juce::StringArray::fromTokens (node.getProperty (key).toString(), ";", {}))
            {
                auto f = juce::StringArray::fromTokens (item.trim(), " ", {});
                if (f.size() < 4) continue;
                notes.push_back ({ f[0].getIntValue(), f[1].getDoubleValue(), f[2].getDoubleValue(), f[3].getFloatValue() });
            }
            edits.notes[(size_t) s][(size_t) t] = std::make_shared<const std::vector<Note>> (std::move (notes));
        }
}

// ============================================================ audio export
bool HipHopBeatMakerProcessor::startAudioRender (bool fullSong, bool mix, bool stems)
{
    if (renderBusy.exchange (true)) return false;
    renderProgress = 0.0f;
    const Overrides snapshot = edits; // copy of shared pointers – safe to read on the worker
    renderPool.addJob ([this, fullSong, mix, stems, snapshot]
    {
        const auto folder = renderAudioNow (fullSong, mix, stems, snapshot);
        {
            const juce::SpinLock::ScopedLockType l (renderLock);
            renderFolder = folder;
        }
        renderProgress = 1.0f;
        renderBusy = false;
        ++renderCounter;
    });
    return true;
}

juce::File HipHopBeatMakerProcessor::renderAudioNow (bool fullSong, bool mix, bool stems, const Overrides& ov)
{
    auto s = currentSettings();
    if (fullSong) s.section = (int) Section::FullSong;
    const auto pat = Generator::generate (s, &ov);
    const double bpm = exportBpm();
    const double rate = 48000.0;
    const auto base = MidiExport::baseName (s, bpm);

    auto folder = MidiExport::exportRoot().getNonexistentChildFile (base + "_Audio", {}, false);
    folder.createDirectory();

    std::vector<Event> ev;
    for (int t = 0; t < numTracks; ++t)
        for (const auto& n : pat.tracks[(size_t) t])
        {
            ev.push_back ({ n.start, t, n.pitch, n.velocity, true });
            ev.push_back ({ n.start + n.length, t, n.pitch, 0.0f, false });
        }
    std::sort (ev.begin(), ev.end(), [] (const Event& a, const Event& b)
    {
        if (a.beat != b.beat) return a.beat < b.beat;
        return ! a.on && b.on;
    });

    const double spb = rate * 60.0 / bpm;
    const int total = (int) (pat.lengthBeats * spb + rate * 2.5); // + tail

    std::vector<int> jobs; // -1 = mix, else solo track
    if (mix) jobs.push_back (-1);
    if (stems)
        for (int t = 0; t < numTracks; ++t)
            if (! pat.tracks[(size_t) t].empty() && pMute[(size_t) t]->load() < 0.5f)
                jobs.push_back (t);

    for (size_t j = 0; j < jobs.size(); ++j)
    {
        const int solo = jobs[j];
        SynthEngine e;
        e.prepare (rate, 1024);
        applyParams (e);
        for (int slot = 0; slot < numDrumSlots; ++slot) e.setSample (slot, engine.getSample (slot));
        for (int t = 1; t < numTracks; ++t) e.setInstrumentSample (t, engine.getInstrumentSample (t));
        if (solo >= 0)
            for (int t = 0; t < numTracks; ++t)
                e.setTrackGain (t, t == solo ? pVol[(size_t) t]->load() : 0.0f);

        juce::AudioBuffer<float> out (2, total);
        out.clear();
        int pos = 0;
        for (const auto& x : ev)
        {
            const int at = juce::jmin (total, (int) (x.beat * spb));
            if (at > pos) { e.render (out, pos, at - pos); pos = at; }
            if (x.on) e.noteOn (x.track, x.pitch, x.vel);
            else      e.noteOff (x.track, x.pitch);
        }
        if (pos < total) e.render (out, pos, total - pos);

        const juce::String name = base + "_" + (solo < 0 ? juce::String ("Mix") : juce::String (trackName (solo))) + ".wav";
        auto file = folder.getChildFile (name);
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = file.createOutputStream())
        {
            std::unique_ptr<juce::OutputStream> os (stream.release());
            if (auto* w = wav.createWriterFor (os.get(), rate, 2, 24, {}, 0))
            {
                os.release();
                std::unique_ptr<juce::AudioFormatWriter> writer (w);
                writer->writeFromAudioSampleBuffer (out, 0, total);
            }
        }
        renderProgress = (float) (j + 1) / (float) jobs.size();
    }
    return folder;
}

// ============================================================ projects
juce::File HipHopBeatMakerProcessor::projectFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Hip-Hop BeatMaker").getChildFile ("Projekte");
    d.createDirectory();
    return d;
}

bool HipHopBeatMakerProcessor::saveProject (const juce::File& f)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return false;
    xml->setAttribute ("app", "Hip-Hop BeatMaker");
    if (! xml->writeTo (f)) return false;
    currentProject = f;
    return true;
}

bool HipHopBeatMakerProcessor::loadProject (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    juce::MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    setPreviewPlaying (false);
    setStateInformation (mb.getData(), (int) mb.getSize());
    currentProject = f;
    return true;
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

bool HipHopBeatMakerProcessor::loadInstrumentSample (int track, const juce::File& file, bool detectRoot)
{
    if (! trackTakesSample (track)) return false;
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) return false;

    const auto maxLen = (juce::int64) (reader->sampleRate * 30.0);
    const int len = (int) juce::jmin (reader->lengthInSamples, maxLen);
    if (len <= 64) return false;

    auto sd = std::make_shared<SampleData>();
    sd->data.setSize ((int) juce::jlimit (1u, 2u, reader->numChannels), len);
    reader->read (&sd->data, 0, len, 0, true, true);
    sd->sampleRate = reader->sampleRate;
    sd->name = file.getFileNameWithoutExtension();
    const float peak = sd->data.getMagnitude (0, len);
    if (peak > 0.0001f) sd->data.applyGain (0.9f / peak); // normalise

    if (detectRoot)
    {
        const int nameRoot = SampleTools::rootFromName (sd->name);
        const float pitch = SampleTools::detectPitch (sd->data, sd->sampleRate);
        int root = 60;
        float fine = 0;
        if (nameRoot >= 0 && pitch >= 0)
        {
            // trust the note name, take the octave from the analysis
            root = nameRoot + 12 * (int) std::round ((pitch - (float) nameRoot) / 12.0f);
            if (std::abs (pitch - (float) root) < 0.5f) fine = (pitch - (float) root) * 100.0f;
        }
        else if (pitch >= 0) { root = (int) std::round (pitch); fine = (pitch - (float) root) * 100.0f; }
        else if (nameRoot >= 0) root = nameRoot;

        auto set = [this, track] (const char* what, float v)
        {
            if (auto* p = apvts.getParameter (sampleParamId (track, what)))
                p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
        set ("root", (float) juce::jlimit (0, 127, root));
        set ("fine", juce::jlimit (-100.0f, 100.0f, std::round (fine)));
        lastDetection = nameRoot >= 0 ? juce::String ("Dateiname") : (pitch >= 0 ? juce::String ("Analyse") : juce::String ("nicht erkannt"));
    }

    engine.setInstrumentSample (track, sd);
    apvts.state.getOrCreateChildWithName ("InstSamples", nullptr)
        .setProperty ("track" + juce::String (track), file.getFullPathName(), nullptr);
    return true;
}

void HipHopBeatMakerProcessor::clearInstrumentSample (int track)
{
    engine.setInstrumentSample (track, nullptr);
    apvts.state.getOrCreateChildWithName ("InstSamples", nullptr).removeProperty ("track" + juce::String (track), nullptr);
}

void HipHopBeatMakerProcessor::reloadSamplesFromState()
{
    auto inst = apvts.state.getChildWithName ("InstSamples");
    for (int t = 1; t < numTracks; ++t)
    {
        engine.setInstrumentSample (t, nullptr);
        if (! inst.isValid()) continue;
        const juce::File f (inst.getProperty ("track" + juce::String (t)).toString());
        if (f.existsAsFile()) loadInstrumentSample (t, f, false);
    }

    auto node = apvts.state.getChildWithName ("Samples");
    for (int i = 0; i < numDrumSlots; ++i)
    {
        engine.setSample (i, nullptr);
        if (! node.isValid()) continue;
        const juce::File f (node.getProperty ("slot" + juce::String (i)).toString());
        if (f.existsAsFile()) loadSample (i, f);
    }
}

// ============================================================ sound design
void HipHopBeatMakerProcessor::setParam (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void HipHopBeatMakerProcessor::loadStyleSound (int style)
{
    for (int t = 1; t < numTracks; ++t)
    {
        const auto patch = SynthEngine::stylePatch (style, t);
        for (int i = 0; i < numSynthParams; ++i)
            setParam (synthParamId (t, i), getPatchValue (patch, i));
    }
    for (int slot = 0; slot < numDrumSlots; ++slot)
        for (int i = 0; i < numDrumParams; ++i)
            setParam (drumParamId (slot, i), drumDefault (i));
}

void HipHopBeatMakerProcessor::loadInstrumentStyleDefault (int track)
{
    const int style = (int) pStyle->load();
    if (track == 0)
    {
        for (int slot = 0; slot < numDrumSlots; ++slot)
            for (int i = 0; i < numDrumParams; ++i)
                setParam (drumParamId (slot, i), drumDefault (i));
        return;
    }
    const auto patch = SynthEngine::stylePatch (style, track);
    for (int i = 0; i < numSynthParams; ++i)
        setParam (synthParamId (track, i), getPatchValue (patch, i));
}

bool HipHopBeatMakerProcessor::isSoundModified (int style) const
{
    auto differs = [this] (const juce::String& id, float want)
    {
        auto* p = apvts.getParameter (id);
        return p != nullptr && std::abs (p->getValue() - p->convertTo0to1 (want)) > 0.002f;
    };
    for (int t = 1; t < numTracks; ++t)
    {
        const auto patch = SynthEngine::stylePatch (style, t);
        for (int i = 0; i < numSynthParams; ++i)
            if (differs (synthParamId (t, i), getPatchValue (patch, i))) return true;
    }
    for (int slot = 0; slot < numDrumSlots; ++slot)
        for (int i = 0; i < numDrumParams; ++i)
            if (differs (drumParamId (slot, i), drumDefault (i))) return true;
    return false;
}

juce::File HipHopBeatMakerProcessor::presetsRoot()
{
    auto d = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile ("Hip-Hop BeatMaker").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::File HipHopBeatMakerProcessor::profileFolder()
{
    auto d = presetsRoot().getChildFile ("Sound-Profile");
    d.createDirectory();
    return d;
}

juce::File HipHopBeatMakerProcessor::instrumentFolder (int track)
{
    auto d = presetsRoot().getChildFile (trackName (track));
    d.createDirectory();
    return d;
}

std::unique_ptr<juce::XmlElement> HipHopBeatMakerProcessor::soundToXml (int scope, int sourceStyle) const
{
    auto xml = std::make_unique<juce::XmlElement> ("HHSound");
    xml->setAttribute ("version", 1);
    xml->setAttribute ("kind", scope < 0 ? "profile" : "instrument");
    xml->setAttribute ("style", styleName (sourceStyle >= 0 ? sourceStyle : (int) pStyle->load()));
    if (scope >= 0) xml->setAttribute ("track", trackName (scope));

    auto add = [&] (const juce::String& storedId, const juce::String& paramId)
    {
        if (auto* p = apvts.getParameter (paramId))
        {
            auto* e = xml->createNewChildElement ("P");
            e->setAttribute ("id", storedId);
            e->setAttribute ("v", (double) dynamic_cast<juce::RangedAudioParameter*> (p)->convertFrom0to1 (p->getValue()));
        }
    };

    for (int t = 1; t < numTracks; ++t)
        if (scope < 0 || scope == t)
            for (int i = 0; i < numSynthParams; ++i)
                add (scope < 0 ? synthParamId (t, i) : juce::String (synthParamDef (i).key), synthParamId (t, i));

    if (scope <= 0)
        for (int slot = 0; slot < numDrumSlots; ++slot)
            for (int i = 0; i < numDrumParams; ++i)
                add (drumParamId (slot, i), drumParamId (slot, i));

    return xml;
}

bool HipHopBeatMakerProcessor::soundFromXml (const juce::XmlElement& xml, int scope)
{
    if (! xml.hasTagName ("HHSound")) return false;
    int applied = 0;
    for (auto* e : xml.getChildWithTagNameIterator ("P"))
    {
        const auto id = e->getStringAttribute ("id");
        const float v = (float) e->getDoubleAttribute ("v");
        juce::String target;
        if (scope > 0 && ! id.startsWithChar ('p') && ! id.startsWithChar ('d'))
            target = "p" + juce::String (scope) + id;             // instrument preset -> this track
        else if (scope < 0 || (scope == 0 && id.startsWithChar ('d')))
            target = id;                                          // profile / drum kit
        if (target.isNotEmpty() && apvts.getParameter (target) != nullptr)
        {
            setParam (target, v);
            ++applied;
        }
    }
    return applied > 0;
}

static juce::String safeFileName (const juce::String& name)
{
    auto n = juce::File::createLegalFileName (name.trim());
    return n.isEmpty() ? juce::String ("Preset") : n;
}

juce::File HipHopBeatMakerProcessor::saveProfile (const juce::String& name, int sourceStyle)
{
    auto f = profileFolder().getChildFile (safeFileName (name) + ".hhsound");
    if (auto xml = soundToXml (-1, sourceStyle)) xml->writeTo (f);
    return f;
}

bool HipHopBeatMakerProcessor::loadProfile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    return xml != nullptr && soundFromXml (*xml, -1);
}

juce::File HipHopBeatMakerProcessor::saveInstrumentPreset (int track, const juce::String& name)
{
    auto f = instrumentFolder (track).getChildFile (safeFileName (name) + ".hhpreset");
    if (auto xml = soundToXml (track)) xml->writeTo (f);
    return f;
}

bool HipHopBeatMakerProcessor::loadInstrumentPreset (int track, const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    return xml != nullptr && soundFromXml (*xml, track);
}

void HipHopBeatMakerProcessor::resolveStyleChange (StyleChoice choice, const juce::String& profileName)
{
    const auto pending = pendingStyleChange;
    pendingStyleChange = {};
    if (pending.to < 0) return;

    if (choice == StyleChoice::SaveAndLoad)
        saveProfile (profileName.isNotEmpty() ? profileName : juce::String (styleName (pending.from)) + " eigener Sound", pending.from);

    if (choice != StyleChoice::KeepSound)
        loadStyleSound ((int) pStyle->load());
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
            auto tree = juce::ValueTree::fromXml (*xml);
            const bool hasSound = tree.hasProperty ("soundVersion");
            apvts.replaceState (tree);
            apvts.state.setProperty ("soundVersion", 1, nullptr);
            lastStyle = (int) pStyle->load();
            pendingStyleChange = {};
            if (! hasSound) loadStyleSound (lastStyle); // project from an older version
            loadEditsFromState();
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
