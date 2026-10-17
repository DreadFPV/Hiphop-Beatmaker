// Runs the real processor without a host: keyboard, loop, WAV export, projects.
#include "PluginProcessor.h"
namespace hhbm { inline bool operator== (const Note& a, const Note& b) { return a.pitch == b.pitch && a.start == b.start; } inline bool operator!= (const std::vector<Note>& a, const std::vector<Note>& b) { return a.size() != b.size(); } }

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

static float runBlocks (HipHopBeatMakerProcessor& p, int blocks, juce::MidiBuffer* firstMidi = nullptr)
{
    juce::AudioBuffer<float> buf (2, 512);
    float rms = 0;
    for (int i = 0; i < blocks; ++i)
    {
        juce::MidiBuffer m;
        if (i == 0 && firstMidi) m = *firstMidi;
        p.processBlock (buf, m);
        rms = juce::jmax (rms, buf.getRMSLevel (0, 0, 512));
    }
    return rms;
}

static void setP (HipHopBeatMakerProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    HipHopBeatMakerProcessor p;
    p.prepareToPlay (48000, 512);

    // 1. silence when stopped
    CHECK (runBlocks (p, 20) < 1e-4f, "stopped = silent");

    // 2. keyboard plays Keys (default target)
    juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 10);
    CHECK (runBlocks (p, 10, &on) > 0.01f, "MIDI keyboard plays while stopped");
    juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    runBlocks (p, 400, &off);
    CHECK (runBlocks (p, 5) < 1e-3f, "note off releases");

    // 3. auto mode: channel 10 -> drums (kick)
    setP (p, "kbd", 0);
    juce::MidiBuffer kick; kick.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
    CHECK (runBlocks (p, 10, &kick) > 0.05f, "auto: channel 10 plays drums");
    runBlocks (p, 300);

    // 4. preview transport + loop off -> stops after the section
    setP (p, "loop", 0);
    setP (p, "section", 0); // Intro, 4 bars at 90 bpm = 10.7 s
    p.regenerateNow();
    p.restartPreview();
    runBlocks (p, 20);
    CHECK (p.isPreviewPlaying(), "preview running");
    runBlocks (p, (int) (11.5 * 48000 / 512));
    CHECK (! p.isPreviewPlaying(), "loop off: stops at the end");

    setP (p, "loop", 1);
    p.restartPreview();
    runBlocks (p, (int) (12.0 * 48000 / 512));
    CHECK (p.isPreviewPlaying() && p.uiBeat.load() >= 0 && p.uiBeat.load() < 4.0, "loop on: wraps to the start (beat " << p.uiBeat.load() << ")");
    p.setPreviewPlaying (false);
    runBlocks (p, 5);

    // 5. WAV export (mix + stems)
    setP (p, "section", 2);
    p.regenerateNow();
    const int before = p.renderCounter.load();
    CHECK (p.startAudioRender (false, true, true), "render started");
    for (int i = 0; i < 600 && p.renderCounter.load() == before; ++i) juce::Thread::sleep (50);
    const auto folder = p.lastRenderFolder();
    auto wavs = folder.findChildFiles (juce::File::findFiles, false, "*.wav");
    CHECK (wavs.size() == 6, "mix + 5 stems written (" << wavs.size() << ") in " << folder.getFileName());
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    for (auto& w : wavs)
    {
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (w));
        juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, true);
        const float rms = b.getRMSLevel (0, 0, b.getNumSamples());
        std::cout << "      " << w.getFileName() << "  " << juce::String (b.getNumSamples() / 48000.0, 1) << " s  rms " << rms << "  " << r->bitsPerSample << " bit\n";
        CHECK (rms > 0.005f && r->bitsPerSample == 24 && r->sampleRate == 48000, "wav " << w.getFileName());
    }

    // 6. project round trip
    setP (p, "p1cut", 4321.0f);
    setP (p, "seed", 77);
    auto proj = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("test.hhbeat");
    CHECK (p.saveProject (proj), "project saved");
    setP (p, "p1cut", 500.0f);
    setP (p, "seed", 3);
    CHECK (p.loadProject (proj), "project loaded");
    const float cut = p.apvts.getRawParameterValue ("p1cut")->load();
    CHECK (std::abs (cut - 4321.0f) < 5.0f && (int) p.apvts.getRawParameterValue ("seed")->load() == 77, "values restored (cutoff " << cut << ")");

    folder.deleteRecursively();

    // 7. note editing
    {
        using namespace hhbm;
        setP (p, "section", (int) Section::Hook);
        setP (p, "seed", 5);
        p.regenerateNow();
        const int H = (int) Section::Hook, B = (int) Track::Bass;
        auto generated = p.sectionNotes (H, B);
        CHECK (! p.isLocked (H, B) && ! generated.empty(), "bass hook generated (" << generated.size() << " notes)");

        std::vector<Note> mine { { 40, 0.0, 1.0, 0.9f }, { 43, 2.0, 0.5, 0.7f }, { 47, 9.0, 3.0, 0.8f } };
        p.setEditedNotes (H, B, mine);
        CHECK (p.isLocked (H, B), "edited track is locked");
        auto pat = p.getPattern();
        CHECK (pat->tracks[(size_t) B].size() == 3 && pat->tracks[(size_t) B][1].pitch == 43, "pattern uses edited notes");

        setP (p, "seed", 6); p.regenerateNow();
        CHECK (p.getPattern()->tracks[(size_t) B].size() == 3, "seed change keeps locked track");
        CHECK (p.sectionNotes (H, (int) Track::Keys) != std::vector<Note>() , "other tracks still generate");

        setP (p, "section", (int) Section::FullSong); p.regenerateNow();
        int hookBass = 0;
        for (auto& n : p.getPattern()->tracks[(size_t) B]) if (n.pitch == 47 && std::abs (n.length - 3.0) < 1e-6) ++hookBass;
        CHECK (hookBass == 3, "every hook in full song uses the edit (" << hookBass << ")");

        // MIDI export contains the edit
        setP (p, "section", H); p.regenerateNow();
        auto dir = p.exportSet (false);
        auto bassFile = dir.findChildFiles (juce::File::findFiles, false, "*_Bass.mid");
        juce::MidiFile mf;
        juce::FileInputStream in (bassFile[0]);
        mf.readFrom (in);
        int ons = 0;
        for (auto* ev : *mf.getTrack (0)) if (ev->message.isNoteOn()) ++ons;
        CHECK (ons == 3, "exported bass MIDI has the 3 edited notes");
        dir.deleteRecursively();

        // undo / redo
        CHECK (p.undoEdit() && ! p.isLocked (H, B), "undo removes the edit");
        CHECK (p.redoEdit() && p.isLocked (H, B), "redo restores it");

        // project round trip
        auto proj2 = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("edit.hhbeat");
        p.saveProject (proj2);
        p.unlockTrack (H, B);
        CHECK (! p.isLocked (H, B), "unlock -> generated again");
        p.loadProject (proj2);
        auto back = p.sectionNotes (H, B);
        CHECK (p.isLocked (H, B) && back.size() == 3 && back[2].pitch == 47 && std::abs (back[2].length - 3.0) < 1e-4, "edits survive save/load");
        proj2.deleteFile();
    }
    std::cout << (failures == 0 ? "ALL OK" : "FAILURES: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
