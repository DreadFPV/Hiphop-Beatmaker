// Offline test: generates every style/section, checks the data and renders audio.
// Usage: HHBMRenderTest <outputDir>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Generator.h"
#include "SynthEngine.h"
#include "MidiExport.h"
#include "SampleTools.h"

using namespace hhbm;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } } while (0)

struct Ev { double beat; int track, pitch; float vel; bool on; };

static juce::AudioBuffer<float> render (const Pattern& p, int style, double bpm, double sr)
{
    SynthEngine eng;
    eng.prepare (sr, 512);
    eng.setStyle (style);

    std::vector<Ev> ev;
    for (int t = 0; t < numTracks; ++t)
        for (auto& n : p.tracks[(size_t) t])
        {
            ev.push_back ({ n.start, t, n.pitch, n.velocity, true });
            ev.push_back ({ n.start + n.length, t, n.pitch, 0, false });
        }
    std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.beat != b.beat ? a.beat < b.beat : (! a.on && b.on); });

    const double spb = sr * 60.0 / bpm;
    const int total = (int) ((p.lengthBeats + 4.0) * spb);
    juce::AudioBuffer<float> out (2, total);
    out.clear();

    int pos = 0;
    for (auto& e : ev)
    {
        int at = juce::jmin (total, (int) (e.beat * spb));
        if (at > pos) { eng.render (out, pos, at - pos); pos = at; }
        if (e.on) eng.noteOn (e.track, e.pitch, e.vel);
        else      eng.noteOff (e.track, e.pitch);
    }
    if (pos < total) eng.render (out, pos, total - pos);
    return out;
}


// ------------------------------------------------------------ sample tests
static juce::AudioBuffer<float> tone (double sr, double seconds, std::function<float (double t, double& ph)> fn)
{
    juce::AudioBuffer<float> b (1, (int) (sr * seconds));
    double ph = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
        b.setSample (0, i, fn (i / sr, ph));
    return b;
}

static float midiOf (double hz) { return (float) (69.0 + 12.0 * std::log2 (hz / 440.0)); }

static void sampleTests()
{
    using namespace SampleTools;
    std::cout << "--- sample tests\n";

    // names (C3 = 60)
    CHECK (rootFromName ("808 Boom C1") == 36, "name C1 -> " << rootFromName ("808 Boom C1"));
    CHECK (rootFromName ("Bass_F#2") == 54, "name F#2 -> " << rootFromName ("Bass_F#2"));
    CHECK (rootFromName ("Keys_Eb3_soft") == 63, "name Eb3 -> " << rootFromName ("Keys_Eb3_soft"));
    CHECK (rootFromName ("Rhodes Chord Am") == -1, "name Am");
    CHECK (rootFromName ("Kick 01") == -1, "name Kick");
    CHECK (rootFromName ("Lead A-1") == 21, "name A-1 -> " << rootFromName ("Lead A-1"));

    const double sr = 44100;
    const double tp = juce::MathConstants<double>::twoPi;

    struct Case { const char* name; double hz; std::function<float (double, double&)> fn; };
    std::vector<Case> cases = {
        { "sine 110",   110.0,  [&] (double, double& ph) { ph += 110.0 / sr; return (float) std::sin (tp * ph); } },
        { "saw 65.4",   65.406, [&] (double, double& ph) { ph += 65.406 / sr; ph -= std::floor (ph); return (float) (2 * ph - 1); } },
        { "808 A1",     55.0,   [&] (double t, double& ph) { ph += (55.0 + 150.0 * std::exp (-t / 0.02)) / sr; return (float) std::tanh (2 * std::sin (tp * ph)) * (float) std::exp (-t / 0.8); } },
        { "rhodes 261", 261.63, [&] (double t, double& ph) { ph += 261.63 / sr; return (float) ((0.4 * std::sin (tp * ph) + 0.8 * std::sin (2 * tp * ph) + 0.5 * std::sin (3 * tp * ph)) * std::exp (-t)); } },
        { "string 440 +20ct", 440.0 * std::exp2 (20.0 / 1200.0), [&] (double, double& ph) { ph += 440.0 * std::exp2 (20.0 / 1200.0) / sr; ph -= std::floor (ph); return (float) (2 * ph - 1) * 0.5f; } },
    };
    for (auto& c : cases)
    {
        auto b = tone (sr, 1.0, c.fn);
        const float got = detectPitch (b, sr), want = midiOf (c.hz);
        std::cout << "   detect " << c.name << ": " << got << " (want " << want << ")\n";
        CHECK (std::abs (got - want) < 0.15f, "detect " << c.name);
    }

    // playback: A3 (220 Hz, root 57) sample played at A4 -> 440 Hz
    auto makeSample = [&] (double hz, double seconds)
    {
        auto sd = std::make_shared<SampleData>();
        sd->data = tone (sr, seconds, [&] (double, double& ph) { ph += hz / sr; return (float) std::sin (tp * ph) * 0.8f; });
        sd->sampleRate = sr;
        return sd;
    };

    auto play = [&] (float a4, const SampleParams& prm, std::shared_ptr<SampleData> sd, std::vector<std::pair<int, double>> notes, double seconds)
    {
        SynthEngine eng;
        eng.prepare (48000, 512); // different rate than the sample on purpose
        eng.setTuning (a4);
        eng.setInstrumentSample ((int) Track::Bass, sd);
        eng.setSampleParams ((int) Track::Bass, prm);
        juce::AudioBuffer<float> out (2, (int) (48000 * seconds));
        out.clear();
        int pos = 0;
        for (auto& [note, at] : notes)
        {
            int s = (int) (at * 48000);
            if (s > pos) { eng.render (out, pos, s - pos); pos = s; }
            eng.noteOn ((int) Track::Bass, note, 1.0f);
        }
        eng.render (out, pos, out.getNumSamples() - pos);
        return out;
    };

    SampleParams prm; prm.root = 57;
    {
        auto out = play (440, prm, makeSample (220, 2.0), { { 69, 0.0 } }, 1.0);
        const float got = detectPitch (out, 48000);
        std::cout << "   play A4 from A3 sample: " << got << "\n";
        CHECK (std::abs (got - 69.0f) < 0.1f, "transpose");
    }
    {
        auto out = play (432, prm, makeSample (220, 2.0), { { 69, 0.0 } }, 1.0);
        const float got = detectPitch (out, 48000);
        std::cout << "   432 Hz: " << got << " (want " << midiOf (432) << ")\n";
        CHECK (std::abs (got - midiOf (432)) < 0.1f, "432 tuning");
    }
    {
        SampleParams lp = prm; lp.loop = true; lp.loopStart = 0.2f; lp.loopEnd = 0.9f;
        auto out = play (440, lp, makeSample (220, 0.3), { { 57, 0.0 } }, 2.0);
        const float late = out.getRMSLevel (0, 48000 * 3 / 2, 4800);
        std::cout << "   loop: rms after 1.5s of 0.3s sample = " << late << "\n";
        CHECK (late > 0.2f, "loop sustain");
        auto noLoop = play (440, prm, makeSample (220, 0.3), { { 57, 0.0 } }, 2.0);
        CHECK (noLoop.getRMSLevel (0, 48000, 4800) < 1e-4f, "one-shot ends");
    }
    {
        // 808 glide: style Trap bass patch has glide; overlapping notes slide
        SynthEngine eng;
        eng.prepare (48000, 512);
        eng.setStyle ((int) Style::Trap);
        eng.setInstrumentSample ((int) Track::Bass, makeSample (55, 3.0));
        SampleParams p808; p808.root = 33; p808.loop = true;
        eng.setSampleParams ((int) Track::Bass, p808);
        juce::AudioBuffer<float> out (2, 48000);
        out.clear();
        eng.noteOn ((int) Track::Bass, 33, 1.0f);
        eng.render (out, 0, 24000);
        eng.noteOn ((int) Track::Bass, 40, 1.0f); // overlapping -> glide, no retrigger
        eng.render (out, 24000, 24000);
        juce::AudioBuffer<float> tail (1, 12000);
        tail.copyFrom (0, 0, out, 0, 36000, 12000);
        const float got = detectPitch (tail, 48000);
        std::cout << "   808 glide end pitch: " << got << "\n";
        CHECK (std::abs (got - 40.0f) < 0.15f, "glide target");
    }
}

int main (int argc, char** argv)
{
    sampleTests();
    juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("render");
    outDir.createDirectory();

    for (int style = 0; style < numStyles; ++style)
    {
        for (int section = 0; section < numSections; ++section)
        {
            Settings s;
            s.style = style; s.section = section; s.seed = 42; s.key = 9; s.variation = 0.5f;
            const auto p = Generator::generate (s);
            const auto p2 = Generator::generate (s);

            const juce::String tag = juce::String (styleName (style)) + "/" + sectionName (section);
            CHECK (p.lengthBeats > 0, tag << " length");

            juce::String counts;
            for (int t = 0; t < numTracks; ++t)
            {
                const auto& tr = p.tracks[(size_t) t];
                CHECK (tr.size() == p2.tracks[(size_t) t].size(), tag << " not deterministic");
                for (auto& n : tr)
                {
                    CHECK (n.pitch >= 0 && n.pitch < 128, tag << " pitch " << n.pitch);
                    CHECK (n.start >= 0 && n.start + n.length <= p.lengthBeats + 1e-6, tag << " " << trackName (t) << " note outside " << n.start << "+" << n.length);
                    CHECK (n.velocity > 0 && n.velocity <= 1, tag << " vel");
                }
                counts << trackName (t) << "=" << (int) tr.size() << " ";
            }
            std::cout << tag << "  " << counts << "\n";

            if (section == (int) Section::FullSong || section == (int) Section::Hook)
            {
                const double bpm = defaultBpm (style);
                auto audio = render (p, style, bpm, 44100);
                float peak = audio.getMagnitude (0, audio.getNumSamples());
                float rms = audio.getRMSLevel (0, 0, audio.getNumSamples());
                bool finite = true;
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < audio.getNumSamples(); ++i)
                        if (! std::isfinite (audio.getSample (ch, i))) { finite = false; break; }

                std::cout << "   render peak=" << peak << " rms=" << rms << (finite ? "" : " NaN!") << "\n";
                CHECK (finite, tag << " NaN");
                CHECK (rms > 0.02f, tag << " too quiet");
                CHECK (peak <= 1.0f, tag << " clipping");

                // per-track levels
                if (section == (int) Section::Hook)
                {
                    juce::String lv;
                    for (int t = 0; t < numTracks; ++t)
                    {
                        Pattern one; one.lengthBeats = p.lengthBeats; one.tracks[(size_t) t] = p.tracks[(size_t) t];
                        auto a = render (one, style, bpm, 44100);
                        lv << trackName (t) << "=" << juce::String (a.getRMSLevel (0, 0, a.getNumSamples()), 3) << " ";
                    }
                    std::cout << "   track rms: " << lv << "\n";
                }

                const auto base = juce::String (styleName (style)).removeCharacters (" -") + "_" + juce::String (sectionName (section)).removeCharacters (" ");
                juce::WavAudioFormat wav;
                auto f = outDir.getChildFile (base + ".wav");
                f.deleteFile();
                if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream().release()))
                {
                    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100, 2, 16, {}, 0));
                    if (w) { os.release(); w->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples()); }
                }
                MidiExport::write (MidiExport::allTracksFile (p, bpm), outDir.getChildFile (base + ".mid"));
            }
        }
    }

    std::cout << (failures == 0 ? "ALL OK" : "FAILURES: " + std::to_string (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
