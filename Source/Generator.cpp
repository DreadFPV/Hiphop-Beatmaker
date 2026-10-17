#include "Generator.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace hhbm
{
namespace
{
// ============================================================ deterministic RNG
struct Rng
{
    uint32_t state;

    explicit Rng (uint32_t seed) : state (seed ? seed : 0x9e3779b9u) {}

    uint32_t next()
    {
        // xorshift32 — identical on every platform/compiler
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uni()                  { return (float) ((next() >> 8) * (1.0 / 16777216.0)); }
    bool chance (float p)        { return uni() < p; }
    int range (int lo, int hi)   { return lo + (int) (next() % (uint32_t) (hi - lo + 1)); } // inclusive
    float bipolar()              { return uni() * 2.0f - 1.0f; }
};

uint32_t mix (uint32_t a, uint32_t b, uint32_t c = 0, uint32_t d = 0)
{
    uint32_t h = 2166136261u;
    for (auto v : { a, b, c, d })
    {
        h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
        h *= 16777619u;
    }
    return h;
}

constexpr double stepLen = 0.25; // one 16th in beats
constexpr double barLen  = 4.0;

// ============================================================ music helpers
struct Harmony
{
    int key, scale;

    int degToMidi (int degree, int baseMidi) const
    {
        const auto& st = scaleSteps (scale);
        int oct = (int) std::floor (degree / 7.0);
        int d = degree - oct * 7;
        return baseMidi + key + 12 * oct + st[(size_t) d];
    }

    /** chord tones as pitch classes (relative to C) for a chord on 'degree' with n notes */
    std::vector<int> chordPcs (int degree, int n) const
    {
        std::vector<int> pcs;
        for (int i = 0; i < n; ++i)
            pcs.push_back (degToMidi (degree + 2 * i, 0) % 12);
        return pcs;
    }

    /** voice the chord around 'center' (close position) */
    std::vector<int> voice (int degree, int n, int center) const
    {
        std::vector<int> out;
        for (int pc : chordPcs (degree, n))
        {
            int p = center - 6 + ((pc - (center - 6)) % 12 + 12) % 12;
            out.push_back (p);
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    int rootPitch (int degree, int lowLimit) const
    {
        int p = degToMidi (degree, 0) % 12;
        while (p < lowLimit) p += 12;
        return p;
    }
};

int autoProgression (int style, int seed)
{
    // style-typical progressions; the seed picks one of them
    static const std::vector<int> pools[] = {
        { 3, 1, 6, 2 },   // Boom Bap
        { 1, 5, 2, 6 },   // Trap
        { 6, 3, 1, 5 },   // Drill
        { 4, 1, 3 },      // Lo-Fi
        { 5, 2, 1 },      // G-Funk
        { 1, 3, 5 },      // Drum & Bass
        { 6, 2, 1 },      // Neurofunk
        { 4, 1, 3 },      // Liquid
        { 5, 2, 6 },      // Jungle
    };
    const auto& pool = pools[style];
    return pool[mix ((uint32_t) seed, 909u, (uint32_t) style) % pool.size()];
}

// ============================================================ drum patterns
// chars: x = hit, o = ghost, ? = optional (depends on Variation), . = rest
struct DrumLine { int note; const char* a; const char* b; float vel; };

std::vector<DrumLine> drumLines (Style st)
{
    using namespace Drum;
    switch (st)
    {
        case Style::BoomBap: return {
            { Kick,      "x.....?.x.x.....", "x.?...x...x..x..", 1.00f },
            { Snare,     "....x..o....x..?", "....x..o....x.ox", 0.95f },
            { ClosedHat, "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x?x.", 0.75f },
            { OpenHat,   "..............?.", "..............x.", 0.55f },
            { Rim,       "..?.......?.....", "..?.....?.......", 0.45f } };

        case Style::Trap: return {
            { Kick,      "x......x..x.....", "x..x......x..?..", 1.00f },
            { Snare,     "........x.......", "........x.......", 0.90f },
            { Clap,      "........x.......", "........x.....?.", 0.95f },
            { ClosedHat, "xxxxxxxxxxxxxxxx", "xxxxxxxxxxxxxxxx", 0.70f },
            { OpenHat,   "......?.......?.", "..............?.", 0.50f } };

        case Style::Drill: return {
            { Kick,      "x.........x.....", "x......x.......?", 1.00f },
            { Snare,     "........x.......", "........x..x....", 0.95f },
            { ClosedHat, "x..x..x.x..x..x.", "x..x..x.x..x.x.x", 0.70f },
            { Rim,       "..?...?......?..", "...?..?...?.....", 0.55f },
            { OpenHat,   "..............?.", "......?.........", 0.45f } };

        case Style::LoFi: return {
            { Kick,      "x......x..x.....", "x.?....x.....?..", 0.85f },
            { Snare,     "....x.......x...", "....x.......x..o", 0.70f },
            { ClosedHat, "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.x.x?", 0.50f },
            { Shaker,    "?.?.?.?.?.?.?.?.", ".?.?.?.?.?.?.?.?", 0.35f },
            { Rim,       "..........?.....", "......?.........", 0.40f } };

        case Style::GFunk: return {
            { Kick,      "x.....x...x.....", "x.....x.?.x..x..", 1.00f },
            { Snare,     "....x.......x...", "....x.......x..o", 0.90f },
            { Clap,      "....x.......x...", "....x.......x...", 0.70f },
            { ClosedHat, "xoxoxoxoxoxoxoxo", "xoxoxoxoxoxoxo?o", 0.65f },
            { OpenHat,   "..............?.", "..............x.", 0.50f },
            { Shaker,    ".?.?.?.?.?.?.?.?", ".?.?.?.?.?.?.?.?", 0.30f } };

        case Style::DnB: return {
            { Kick,      "x.........x.....", "x.........x...x.", 1.00f },
            { Snare,     "....x..o....x..o", "....x..o....x.xx", 0.95f },
            { ClosedHat, "x.x.x.x.x.x.x.x.", "x.x.x.x.x.x.xxxx", 0.60f },
            { OpenHat,   "..?.......?.....", "..x.......?.....", 0.45f },
            { Shaker,    ".?.?.?.?.?.?.?.?", ".x.x.x.x.x.x.x.x", 0.30f } };

        case Style::Neuro: return {
            { Kick,      "x.........x.....", "x.........x..x..", 1.00f },
            { Snare,     "....x.......x...", "....x.......x.xx", 1.00f },
            { Clap,      "....x.......x...", "....x.......x...", 0.60f },
            { ClosedHat, "x.x.x.x.x.x.x.x.", "x.xxx.x.x.xxx.xx", 0.55f },
            { Rim,       "......x.....?...", "...?..x.....x...", 0.50f },
            { OpenHat,   "..?...........?.", "..x...........x.", 0.40f } };

        case Style::Liquid: return {
            { Kick,      "x.........x.....", "x......x..x.....", 0.90f },
            { Snare,     "....x.......x...", "....x.......x..o", 0.80f },
            { ClosedHat, "x?x?x?x?x?x?x?x?", "x?x?x?x?x?x?x?xx", 0.45f },
            { Shaker,    "?.?.?.?.?.?.?.?.", ".?.?.?.?.?.?.?.?", 0.30f },
            { OpenHat,   "..............?.", "......?.......x.", 0.35f } };

        case Style::Jungle: return {
            { Kick,      "x.x.......xx....", "x.x.......x.....", 1.00f },
            { Snare,     "....x..x.x..x..x", "....x..x.x..xx.x", 0.90f },
            { ClosedHat, "x.x.x.x.x.x.x.x.", "x.x.x.x.xxx.x.x.", 0.55f },
            { OpenHat,   "......?.......?.", "......x.......?.", 0.45f },
            { Rim,       "?..?..?..?......", "?..?..?..?..?...", 0.40f } };

        default: break;
    }
    return {};
}

// ============================================================ the builder
class Builder
{
public:
    Builder (const Settings& s, const Overrides* o) : set (s), st ((Style) s.style), harm { s.key, s.scale }, edits (o) {}

    void buildSection (Pattern& out, int section, double offset, int occurrence)
    {
        const int bars = Generator::sectionBars (section);
        const double len = bars * barLen;

        auto prog = progressionDegrees (set.progression == 0 ? autoProgression (set.style, set.seed) : set.progression);
        std::vector<int> chordPerBar;
        for (int b = 0; b < bars; ++b)
        {
            int idx = (section == (int) Section::Bridge) ? (b + 2) % 4 : b % 4;
            chordPerBar.push_back (prog[(size_t) idx]);
        }

        std::array<std::vector<Note>, numTracks> t;
        drums  (t[(int) Track::Drums], section, bars, occurrence);
        bass   (t[(int) Track::Bass],  section, bars, chordPerBar, t[(int) Track::Drums]);
        keys   (t[(int) Track::Keys],  section, bars, chordPerBar);
        pad    (t[(int) Track::Pad],   section, bars, chordPerBar);
        lead   (t[(int) Track::Lead],  section, bars, chordPerBar);

        Rng hr (mix ((uint32_t) set.seed, 777u, (uint32_t) section, (uint32_t) occurrence));

        for (int tr = 0; tr < numTracks; ++tr)
        {
            if (edits != nullptr && edits->has (section, tr))
            {
                // user-edited (locked) track: exact notes, no groove / humanize
                for (auto n : *edits->notes[(size_t) section][(size_t) tr])
                {
                    if (n.start >= len || n.start < 0) continue;
                    n.length = std::max (0.01, std::min (n.length, len - n.start));
                    n.start += offset;
                    out.tracks[(size_t) tr].push_back (n);
                }
                continue;
            }
            for (auto n : t[(size_t) tr])
            {
                groove (n, tr, hr);
                n.start = std::clamp (n.start, 0.0, len - 0.01);
                n.length = std::max (0.03, std::min (n.length, len - n.start));
                n.start += offset;
                out.tracks[(size_t) tr].push_back (n);
            }
        }

        out.markers.push_back ({ section, offset, len });
    }

private:
    const Settings& set;
    Style st;
    Harmony harm;
    const Overrides* edits = nullptr;

    // ---------------------------------------------------------------- groove
    double swingDelay() const
    {
        static const float styleAmount[] = { 0.8f, 0.25f, 0.3f, 1.0f, 0.5f, 0.1f, 0.05f, 0.3f, 0.2f };
        // MPC style: off-16th moves from 50% up to 75% of the 8th
        double f = 0.5 + 0.25 * set.swing * styleAmount[set.style];
        return (f - 0.5) * 0.5;
    }

    void groove (Note& n, int track, Rng& r) const
    {
        const double posInEighth = std::fmod (n.start, 0.5);
        const bool offSixteenth = std::abs (posInEighth - 0.25) < 1.0e-6;
        const bool onGrid = std::abs (std::fmod (n.start, stepLen)) < 1.0e-6;

        if (offSixteenth && onGrid)
        {
            const double d = swingDelay();
            n.start += d;
            if (track != (int) Track::Pad)
                n.length = std::max (0.05, n.length - d * 0.5);
        }

        if (set.humanize > 0.0f && track != (int) Track::Pad)
        {
            const double timing = track == (int) Track::Drums ? 0.012 : 0.02;
            if (n.start > 0.001)
                n.start += r.bipolar() * timing * set.humanize;
            n.velocity *= 1.0f + r.bipolar() * 0.15f * set.humanize;
        }
        n.velocity = std::clamp (n.velocity, 0.08f, 1.0f);
    }

    // ---------------------------------------------------------------- drums
    void drums (std::vector<Note>& out, int section, int bars, int occurrence)
    {
        Rng r (mix ((uint32_t) set.seed, 101u + (uint32_t) set.style, (uint32_t) section, (uint32_t) occurrence));
        const auto lines = drumLines (st);
        const bool hook = section == (int) Section::Hook;
        const float var = std::clamp (set.variation + (hook ? 0.2f : 0.0f), 0.0f, 1.0f);
        const float optProb = 0.12f + 0.65f * var;

        for (int bar = 0; bar < bars; ++bar)
        {
            const bool lastBar = bar == bars - 1;
            const bool fillBar = lastBar || (bar % 4 == 3) || (var > 0.5f && bar % 2 == 1 && r.chance (var - 0.4f));

            for (const auto& l : lines)
            {
                if (! drumAllowed (section, l.note, bar, bars))
                    continue;

                const char* p = fillBar ? l.b : l.a;

                for (int step = 0; step < 16; ++step)
                {
                    const char c = p[step];
                    if (c == '.') continue;
                    if (c == '?' && ! r.chance (optProb)) continue;

                    double start = bar * barLen + step * stepLen;
                    if (section == (int) Section::Intro && start >= (bars * barLen - 2.0) && l.note != Drum::ClosedHat && l.note != Drum::Shaker)
                        continue; // little drop before the verse

                    float vel = l.vel;
                    if (c == 'o') vel *= 0.45f;
                    if (c == '?') vel *= 0.8f;
                    if (l.note == Drum::ClosedHat || l.note == Drum::Shaker)
                        vel *= (step % 4 == 0) ? 1.0f : (step % 2 == 0 ? 0.8f : 0.6f);

                    double len = (l.note == Drum::OpenHat) ? 0.5 : 0.2;

                    // trap / drill hat rolls
                    if ((st == Style::Trap || st == Style::Drill) && l.note == Drum::ClosedHat)
                    {
                        if (hatRoll (out, r, bar, step, vel, var))
                        {
                            if (st == Style::Trap) ++step; // roll replaces the next 16th too
                            continue;
                        }
                    }

                    out.push_back ({ l.note, start, len, vel });
                }
            }
        }

        // DnB family: snare roll into the next section
        if (isDnbFamily (set.style) && section != (int) Section::Outro && bars >= 2)
        {
            const double b = (bars - 1) * barLen;
            const int hits = set.variation > 0.5f ? 8 : 4;
            for (int i = 0; i < hits; ++i)
            {
                const double pos = b + 4.0 - (hits - i) * 0.25;
                out.push_back ({ Drum::Snare, pos, 0.12, 0.45f + 0.5f * (float) i / (float) hits });
            }
        }

        // crash on section start
        if (section == (int) Section::Hook || section == (int) Section::Outro
            || (section == (int) Section::Verse && occurrence > 0))
            out.push_back ({ Drum::Crash, 0.0, 2.0, 0.8f });

        // outro: final hit
        if (section == (int) Section::Outro)
        {
            const double endBar = (bars - 1) * barLen;
            out.push_back ({ Drum::Kick, endBar, 0.25, 1.0f });
            out.push_back ({ Drum::Crash, endBar, 3.5, 0.9f });
        }
    }

    bool drumAllowed (int section, int note, int bar, int bars) const
    {
        const bool firstHalf = bar < bars / 2;
        switch ((Section) section)
        {
            case Section::Intro:
                if (firstHalf)
                    return note == Drum::ClosedHat || note == Drum::Shaker || note == Drum::Rim;
                return true;
            case Section::Bridge:
                if (note == Drum::Kick) return bar == bars - 1;
                if (note == Drum::Snare || note == Drum::Clap) return bar % 2 == 1;
                return true;
            case Section::Outro:
                if (bar == bars - 1) return false; // handled separately
                if (! firstHalf) return note == Drum::ClosedHat || note == Drum::Shaker || note == Drum::Rim;
                return true;
            default:
                return true;
        }
    }

    bool hatRoll (std::vector<Note>& out, Rng& r, int bar, int step, float vel, float var)
    {
        // rolls mostly in the second half of the bar
        if (step < 6 || step % 2 != 0) return false;
        if (! r.chance (0.06f + 0.25f * var)) return false;

        const double start = bar * barLen + step * stepLen;
        const bool triplet = r.chance (0.5f);
        const int count = triplet ? 3 : (r.chance (0.5f) ? 4 : 6);
        const double span = triplet ? 0.5 : (count == 4 ? 0.5 : 0.5);
        const double d = span / count;
        const bool rampUp = r.chance (0.6f);

        for (int i = 0; i < count; ++i)
        {
            float f = (float) i / (float) (count - 1);
            float v = vel * (rampUp ? (0.55f + 0.45f * f) : (1.0f - 0.45f * f));
            out.push_back ({ Drum::ClosedHat, start + i * d, d * 0.8, v });
        }
        return true;
    }

    // ---------------------------------------------------------------- bass
    void bass (std::vector<Note>& out, int section, int bars, const std::vector<int>& chords,
               const std::vector<Note>& drumNotes)
    {
        if (section == (int) Section::Intro && st != Style::LoFi)
        {
            // intro: bass only enters in the last bar (or not at all)
            if (bars < 2) return;
        }

        Rng r (mix ((uint32_t) set.seed, 202u + (uint32_t) set.style, (uint32_t) section));
        int low = 30;

        auto rootOf = [&] (int bar) { return harm.rootPitch (chords[(size_t) bar], low); };

        auto activeBar = [&] (int bar)
        {
            if (section == (int) Section::Intro) return bar == bars - 1 || (st == Style::LoFi && bar >= bars / 2);
            if (section == (int) Section::Outro) return bar < bars / 2;
            return true;
        };

        if (st == Style::Neuro)
        {
            static const char* grooves[] = { "x..x..x.x.x..x.x", "x.x..x...x.x..x.", "xx..x..x..x.x..x" };
            const char* g = grooves[r.range (0, 2)];
            for (int bar = 0; bar < bars; ++bar)
            {
                if (! activeBar (bar)) continue;
                const int root = rootOf (bar);
                for (int s16 = 0; s16 < 16; ++s16)
                {
                    if (g[s16] != 'x') continue;
                    int p = root;
                    const float c = r.uni();
                    if (s16 > 0)
                    {
                        if (c < 0.25f * (0.5f + set.variation)) p = root + 12;
                        else if (c < 0.35f * (0.5f + set.variation)) p = root + 1;   // dark b2
                        else if (c < 0.45f * (0.5f + set.variation)) p = root + 7;
                    }
                    const bool longNote = s16 == 0 || r.chance (0.15f);
                    out.push_back ({ p, bar * barLen + s16 * stepLen, longNote ? 0.7 : 0.2, s16 % 4 == 0 ? 1.0f : 0.8f });
                }
            }
        }
        else if (st == Style::Trap || st == Style::Drill || st == Style::BoomBap
                 || st == Style::DnB || st == Style::Liquid || st == Style::Jungle)
        {
            // bass follows the kick
            std::vector<double> kicks;
            for (auto& n : drumNotes)
                if (n.pitch == Drum::Kick) kicks.push_back (n.start);
            std::sort (kicks.begin(), kicks.end());
            kicks.erase (std::unique (kicks.begin(), kicks.end(), [] (double a, double b) { return std::abs (a - b) < 0.01; }), kicks.end());

            // in sections without kick on the downbeat, still play the root on bar starts
            for (int bar = 0; bar < bars; ++bar)
            {
                double bs = bar * barLen;
                bool has = std::any_of (kicks.begin(), kicks.end(), [&] (double k) { return std::abs (k - bs) < 0.01; });
                if (! has) kicks.push_back (bs);
            }
            std::sort (kicks.begin(), kicks.end());

            const bool is808 = st != Style::BoomBap;
            for (size_t i = 0; i < kicks.size(); ++i)
            {
                const double start = kicks[i];
                const int bar = (int) (start / barLen);
                if (bar >= bars || ! activeBar (bar)) continue;

                const double next = (i + 1 < kicks.size()) ? kicks[i + 1] : bars * barLen;
                double len = next - start;
                if (! is808) len = std::min (len, 1.5) * 0.9;

                int pitch = rootOf (bar);
                const bool offbeat = std::fmod (start, barLen) > 0.01;
                if (offbeat && r.chance (0.15f + 0.4f * set.variation))
                {
                    const int choice = r.range (0, 2);
                    pitch += (choice == 0 ? 12 : (choice == 1 ? 7 : -2));
                }

                // 808 glide: overlap into the next note
                bool glide = is808 && (i + 1 < kicks.size()) && r.chance (st == Style::Drill ? 0.45f : (st == Style::Liquid ? 0.3f : 0.2f)) && set.variation > 0.1f;
                if (glide) len += 0.12;

                out.push_back ({ pitch, start, std::max (0.1, len), 0.95f });

                if (st == Style::Drill && r.chance (0.25f * set.variation) && len > 1.5)
                {
                    // drill slide: short note into a new pitch inside a long 808
                    double ss = start + len * 0.6;
                    out.push_back ({ pitch + (r.chance (0.5f) ? 12 : 1), ss, len * 0.4, 0.9f });
                    out.back().start = std::round (ss / stepLen) * stepLen;
                }
            }
        }
        else if (st == Style::LoFi)
        {
            for (int bar = 0; bar < bars; ++bar)
            {
                if (! activeBar (bar)) continue;
                const int root = rootOf (bar);
                const int nextRoot = rootOf ((bar + 1) % bars);
                const double b = bar * barLen;
                out.push_back ({ root, b, 1.8, 0.8f });
                out.push_back ({ r.chance (0.5f) ? root + 7 : root, b + 2.5, 1.0, 0.7f });
                if (r.chance (0.2f + 0.5f * set.variation))
                {
                    int approach = nextRoot + (r.chance (0.5f) ? -1 : 2);
                    out.push_back ({ approach, b + 3.5, 0.45, 0.65f });
                }
            }
        }
        else // GFunk
        {
            static const char* grooves[] = { "x..x..x.x.x..x..", "x.....xx..x.x...", "x..x...xx..x..x." };
            const char* g = grooves[r.range (0, 2)];
            for (int bar = 0; bar < bars; ++bar)
            {
                if (! activeBar (bar)) continue;
                const int root = rootOf (bar);
                const int seventh = harm.degToMidi (chords[(size_t) bar] + 6, 0) % 12;
                for (int s = 0; s < 16; ++s)
                {
                    if (g[s] != 'x') continue;
                    int p = root;
                    if (s > 0)
                    {
                        float c = r.uni();
                        if (c < 0.25f * (0.5f + set.variation)) p = root + 12;
                        else if (c < 0.4f * (0.5f + set.variation)) p = root + 7;
                        else if (c < 0.5f * (0.5f + set.variation)) p = root + ((seventh - root) % 12 + 12) % 12;
                    }
                    out.push_back ({ p, bar * barLen + s * stepLen, 0.22, s == 0 ? 0.95f : 0.8f });
                }
            }
        }
    }

    // ---------------------------------------------------------------- keys
    int chordSize() const
    {
        switch (st)
        {
            case Style::LoFi:    return set.variation > 0.5f ? 5 : 4;
            case Style::GFunk:   return 4;
            case Style::Liquid:  return set.variation > 0.5f ? 5 : 4;
            case Style::Neuro:   return 2;
            case Style::BoomBap: return set.variation > 0.6f ? 4 : 3;
            default:             return 3;
        }
    }

    void keys (std::vector<Note>& out, int section, int bars, const std::vector<int>& chords)
    {
        Rng r (mix ((uint32_t) set.seed, 303u + (uint32_t) set.style, (uint32_t) section));
        const int n = chordSize();

        auto active = [&] (int bar)
        {
            if (section == (int) Section::Bridge) return st == Style::LoFi || st == Style::GFunk || st == Style::Liquid || bar % 2 == 0;
            return true;
        };

        for (int bar = 0; bar < bars; ++bar)
        {
            if (! active (bar)) continue;
            const auto v = harm.voice (chords[(size_t) bar], n, 64);
            const double b = bar * barLen;

            auto chordAt = [&] (double start, double len, float vel, double strum)
            {
                for (size_t i = 0; i < v.size(); ++i)
                    out.push_back ({ v[i], start + strum * (double) i, len - strum * (double) i, vel });
            };

            switch (st)
            {
                case Style::BoomBap:
                {
                    // chopped piano stabs
                    static const char* rh[] = { "x.....x...x.....", "x..x......x.....", "x.......x.....x." };
                    const char* p = rh[(bar + r.range (0, 2)) % 3];
                    for (int s = 0; s < 16; ++s)
                        if (p[s] == 'x')
                            chordAt (b + s * stepLen, s == 0 ? 1.4 : 0.6, s == 0 ? 0.8f : 0.65f, 0.0);
                    break;
                }
                case Style::Trap:
                {
                    // dark broken-chord piano in 8ths
                    for (int e = 0; e < 8; ++e)
                    {
                        int idx = (e % 2 == 0) ? 0 : (int) (1 + (e / 2) % (v.size() - 1));
                        if (e >= 4 && r.chance (0.3f * set.variation)) continue;
                        out.push_back ({ v[(size_t) idx] + (e == 0 ? -12 : 0), b + e * 0.5, 0.45, e == 0 ? 0.75f : 0.6f });
                    }
                    break;
                }
                case Style::Drill:
                {
                    static const char* p = "x..x..x...x..x..";
                    for (int s = 0; s < 16; ++s)
                    {
                        if (p[s] != 'x') continue;
                        int note = v[(size_t) r.range (0, (int) v.size() - 1)];
                        out.push_back ({ note + 12, b + s * stepLen, 0.6, 0.7f });
                    }
                    break;
                }
                case Style::LoFi:
                    chordAt (b, 2.4, 0.6f, 0.03);
                    if (r.chance (0.3f + 0.5f * set.variation))
                        chordAt (b + 2.5, 1.4, 0.5f, 0.025);
                    break;
                case Style::DnB:
                {
                    static const char* rh[] = { "x.....x...x.....", "x.........x.....", "......x.....x..." };
                    const char* p = rh[(bar + r.range (0, 2)) % 3];
                    for (int s16 = 0; s16 < 16; ++s16)
                        if (p[s16] == 'x') chordAt (b + s16 * stepLen, 0.45, 0.6f, 0.0);
                    break;
                }
                case Style::Neuro:
                {
                    static const char* p = "..x....x..x.....";
                    for (int s16 = 0; s16 < 16; ++s16)
                        if (p[s16] == 'x' && r.chance (0.5f + 0.4f * set.variation))
                            chordAt (b + s16 * stepLen, 0.2, 0.7f, 0.0);
                    break;
                }
                case Style::Liquid:
                    chordAt (b, 2.6, 0.55f, 0.02);
                    if (r.chance (0.4f + 0.4f * set.variation))
                        chordAt (b + 2.75, 1.2, 0.45f, 0.02);
                    break;
                case Style::Jungle:
                case Style::GFunk:
                {
                    static const char* p = "..x...x...x...x.";
                    for (int s = 0; s < 16; ++s)
                        if (p[s] == 'x' && (s != 14 || r.chance (0.5f + 0.5f * set.variation)))
                            chordAt (b + s * stepLen, 0.35, 0.6f, 0.0);
                    break;
                }
                default: break;
            }
        }
    }

    // ---------------------------------------------------------------- pad
    void pad (std::vector<Note>& out, int section, int bars, const std::vector<int>& chords)
    {
        const bool always = st == Style::Trap || st == Style::Drill || st == Style::LoFi || isDnbFamily (set.style);
        const bool hookish = section == (int) Section::Hook || section == (int) Section::Bridge || section == (int) Section::Intro;
        if (! always && ! hookish) return;

        // sound of the pad part depends on the seed (same for every section of a song)
        Rng r (mix ((uint32_t) set.seed, 606u + (uint32_t) set.style));
        const int size   = r.chance (0.35f + 0.4f * set.variation) ? 4 : 3;   // add the 7th
        const int center = 54 + r.range (0, 8);                                // register
        const bool open  = r.chance (0.4f);                                   // spread voicing
        const bool bassNote = r.chance (0.3f);                                // root an octave below
        const int rhythm = r.range (0, 4);
        // 0 whole bar · 1 two halves · 2 long + push into next bar · 3 anticipated by an 8th · 4 dotted swells

        const float vel = (st == Style::LoFi || st == Style::Liquid) ? 0.45f : 0.6f;
        auto chord = [&] (int degree)
        {
            auto v = harm.voice (degree, size, center);
            if (open && v.size() >= 3) v[1] += 12;          // drop-2 style spread
            if (bassNote) v.push_back (v.front() - 12);
            std::sort (v.begin(), v.end());
            return v;
        };
        auto add = [&] (int degree, double start, double len, float vv)
        {
            for (auto p : chord (degree))
                out.push_back ({ p, start, len, vv });
        };

        for (int bar = 0; bar < bars; ++bar)
        {
            const int deg = chords[(size_t) bar];
            const int next = chords[(size_t) ((bar + 1) % bars)];
            const double b = bar * barLen;
            switch (rhythm)
            {
                case 0: add (deg, b, barLen - 0.02, vel); break;
                case 1: add (deg, b, 1.95, vel); add (deg, b + 2.0, 1.95, vel * 0.85f); break;
                case 2:
                    add (deg, b, 3.45, vel);
                    if (bar < bars - 1) add (next, b + 3.5, 0.45, vel * 0.8f);
                    break;
                case 3:
                    // chord change arrives an 8th early (except the very first bar)
                    add (deg, bar == 0 ? b : b - 0.5, bar == 0 ? 3.48 : 3.98, vel);
                    break;
                default:
                    add (deg, b, 1.45, vel); add (deg, b + 1.5, 1.45, vel * 0.8f); add (deg, b + 3.0, 0.95, vel * 0.7f);
                    break;
            }
        }
    }

    // ---------------------------------------------------------------- lead / melody
    void lead (std::vector<Note>& out, int section, int bars, const std::vector<int>& chords)
    {
        // which bars carry the melody
        auto active = [&] (int bar)
        {
            switch ((Section) section)
            {
                case Section::Intro:  return (st == Style::Trap || st == Style::Drill) ? true : bar >= bars / 2;
                case Section::Verse:  return st == Style::Trap || st == Style::Drill || st == Style::Liquid
                                             || ((st == Style::GFunk || st == Style::DnB || st == Style::Neuro || st == Style::Jungle) && bar % 4 == 3);
                case Section::Hook:   return true;
                case Section::Bridge: return bar % 2 == 0;
                case Section::Outro:  return bar < bars / 2;
                default:              return true;
            }
        };

        // motif depends only on seed + style, so every hook sounds like the same song
        Rng r (mix ((uint32_t) set.seed, 404u + (uint32_t) set.style));

        static const char* rhythms[numStyles][3] = {
            { "x.....x.....x...", "x.......x..x....", "x..x......x....." },   // BoomBap: sparse horn/flute
            { "x.x.x..x.x.x.x..", "x..x..x.x.x..x..", "x.xx..x.x..x.x.x" },   // Trap: bells
            { "x..x..x...x.x...", "x..x..x.x..x....", "x.....x.x..x..x." },   // Drill
            { "x.......x.......", "x.....x.......x.", "x...........x..." },   // LoFi
            { "x...........x...", "x.......x.......", "x.....x.....x..." },   // GFunk: long whines
            { "x.x.x.x.x.x.x.x.", "x..x..x.x..x..x.", "x...x.x...x.x..." },   // DnB: plucks / arps
            { "x......x..x.....", "x.....x.....x...", "x..x......x....." },   // Neuro: sci-fi stabs
            { "x.......x...x...", "x...........x...", "x.....x.......x." },   // Liquid: soft melody
            { "x...x.......x...", "x.x.....x.......", "x.....x.x......." },   // Jungle: horn stabs
        };

        struct MotifNote { int step; int degOffset; };
        std::array<std::vector<MotifNote>, 2> motif;

        for (int mb = 0; mb < 2; ++mb)
        {
            const char* rh = rhythms[set.style][r.range (0, 2)];
            int deg = r.range (0, 2) * 2; // start on a chord tone
            for (int s = 0; s < 16; ++s)
            {
                if (rh[s] != 'x') continue;
                if (s % 4 == 0)
                    deg = 2 * r.range (0, 2); // strong beat: chord tone (root, 3rd, 5th)
                else
                    deg += r.chance (0.5f) ? (r.chance (0.7f) ? 1 : 2) : (r.chance (0.7f) ? -1 : -2);
                deg = std::clamp (deg, -3, 9);
                motif[(size_t) mb].push_back ({ s, deg });
            }
        }

        const int base = (st == Style::BoomBap || st == Style::LoFi || st == Style::Jungle) ? 60 : 72;

        for (int bar = 0; bar < bars; ++bar)
        {
            if (! active (bar)) continue;
            auto m = motif[(size_t) (bar % 2)];

            // phrase answer on every 4th bar
            if (bar % 4 == 3)
            {
                Rng vr (mix ((uint32_t) set.seed, 505u + (uint32_t) set.style, (uint32_t) bar));
                for (auto& mn : m)
                    if (vr.chance (0.25f + 0.5f * set.variation))
                        mn.degOffset += vr.range (-2, 2);
            }

            const int chordDeg = chords[(size_t) bar];
            for (size_t i = 0; i < m.size(); ++i)
            {
                const double start = bar * barLen + m[i].step * stepLen;
                const double end = (i + 1 < m.size()) ? bar * barLen + m[i + 1].step * stepLen : (bar + 1) * barLen;
                double len = end - start;
                if (st == Style::BoomBap) len = std::min (len, 1.0);
                if (st == Style::Trap)    len = std::min (len, 0.75);
                if (st == Style::DnB || st == Style::Neuro || st == Style::Jungle) len = std::min (len, 0.5);

                int pitch = harm.degToMidi (chordDeg + m[i].degOffset, base);
                while (pitch > base + 19) pitch -= 12;
                while (pitch < base - 5) pitch += 12;

                const bool legato = st == Style::GFunk || st == Style::Drill || st == Style::Liquid;
                out.push_back ({ pitch, start, legato ? len + 0.05 : len * 0.92, i == 0 ? 0.85f : 0.72f });
            }
        }
    }
};
} // namespace

// ============================================================ public
int Generator::sectionBars (int section)
{
    static const int b[] = { 4, 8, 8, 4, 4 };
    return section >= 0 && section < 5 ? b[section] : 8;
}

const std::vector<int>& Generator::songOrder()
{
    static const std::vector<int> o {
        (int) Section::Intro, (int) Section::Verse, (int) Section::Hook,
        (int) Section::Verse, (int) Section::Hook, (int) Section::Bridge,
        (int) Section::Hook, (int) Section::Outro
    };
    return o;
}

Pattern Generator::generate (const Settings& s, const Overrides* edits)
{
    Settings safe = s;
    safe.style = std::clamp (s.style, 0, numStyles - 1);
    safe.scale = std::clamp (s.scale, 0, numScales - 1);
    safe.progression = std::clamp (s.progression, 0, numProgressions - 1);
    safe.key = ((s.key % 12) + 12) % 12;

    Pattern p;
    Builder b (safe, edits);

    if (safe.section == (int) Section::FullSong)
    {
        double offset = 0.0;
        std::array<int, 6> occ {};
        for (int sec : songOrder())
        {
            b.buildSection (p, sec, offset, occ[(size_t) sec]++);
            offset += sectionBars (sec) * barLen;
        }
        p.lengthBeats = offset;
    }
    else
    {
        b.buildSection (p, safe.section, 0.0, 0);
        p.lengthBeats = sectionBars (safe.section) * barLen;
    }

    for (auto& t : p.tracks)
        std::sort (t.begin(), t.end(), [] (const Note& a, const Note& c) { return a.start < c.start; });

    return p;
}
} // namespace hhbm
