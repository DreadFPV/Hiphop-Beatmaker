// Offline test: generates every style/section, checks the data and renders audio.
// Usage: HHBMRenderTest <outputDir>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Generator.h"
#include "SynthEngine.h"
#include "MidiExport.h"

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

int main (int argc, char** argv)
{
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
