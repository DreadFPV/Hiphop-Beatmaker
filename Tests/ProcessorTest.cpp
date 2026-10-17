// Runs the real processor without a host: keyboard, loop, WAV export, projects.
#include "PluginProcessor.h"

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
    std::cout << (failures == 0 ? "ALL OK" : "FAILURES: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
