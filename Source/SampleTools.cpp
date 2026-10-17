#include "SampleTools.h"

#include <cmath>
#include <vector>

namespace hhbm::SampleTools
{
juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (n < 0 || n > 127) return "-";
    return juce::String (names[n % 12]) + juce::String (n / 12 - 2); // C3 = 60
}

int rootFromName (const juce::String& name)
{
    const auto s = name.toUpperCase();
    const int len = s.length();

    // scan from the right: the note is usually at the end ("808 Boom F#1")
    for (int i = len - 1; i >= 0; --i)
    {
        const juce::juce_wchar c = s[i];
        if (c < 'A' || c > 'G') continue;

        // must not be part of a word
        if (i > 0 && juce::CharacterFunctions::isLetter (s[i - 1])) continue;

        static const int pcs[] = { 9, 11, 0, 2, 4, 5, 7 }; // A..G
        int pc = pcs[c - 'A'];
        int j = i + 1;

        if (j < len && (s[j] == '#' || (s[j] == 'S' && j + 1 < len && (juce::CharacterFunctions::isDigit (s[j + 1]) || s[j + 1] == '-'))))
        { ++pc; ++j; }
        else if (j < len && s[j] == 'B' && j + 1 < len && (juce::CharacterFunctions::isDigit (s[j + 1]) || s[j + 1] == '-'))
        { --pc; ++j; }

        bool neg = false;
        if (j < len && s[j] == '-') { neg = true; ++j; }
        if (j >= len || ! juce::CharacterFunctions::isDigit (s[j])) continue;

        int oct = s[j] - '0';
        ++j;
        if (j < len && juce::CharacterFunctions::isLetterOrDigit (s[j])) continue; // e.g. "C12" or "D4x"
        if (neg) oct = -oct;

        const int note = (oct + 2) * 12 + ((pc % 12) + 12) % 12 + (pc < 0 ? -12 : (pc > 11 ? 12 : 0));
        if (note >= 0 && note <= 127) return note;
    }
    return -1;
}

float detectPitch (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int total = buffer.getNumSamples();
    if (total < 2048 || sampleRate <= 0) return -1.0f;

    // skip the attack (808 pitch drop, transients)
    int start = (int) (sampleRate * 0.08);
    if (total > (int) (sampleRate * 0.5)) start = (int) (sampleRate * 0.15); // 808 pitch drop has settled
    else if (total < (int) (sampleRate * 0.3)) start = total / 10;

    const int window = juce::jmin (4096, total - start);
    const int maxLag = juce::jmin ((int) (sampleRate / 27.0), window / 2);
    const int minLag = juce::jmax (2, (int) (sampleRate / 2000.0));
    if (window < 1024 || maxLag <= minLag) return -1.0f;

    std::vector<float> x ((size_t) window);
    for (int i = 0; i < window; ++i)
    {
        float v = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            v += buffer.getSample (ch, start + i);
        x[(size_t) i] = v;
    }

    double energy = 0;
    for (auto v : x) energy += v * v;
    if (energy / window < 1.0e-8) return -1.0f;

    // normalised square difference function
    std::vector<float> nsdf ((size_t) maxLag + 1, 0.0f);
    for (int tau = 0; tau <= maxLag; ++tau)
    {
        double r = 0, m = 0;
        for (int j = 0; j < window - tau; ++j)
        {
            const double a = x[(size_t) j], b = x[(size_t) (j + tau)];
            r += a * b;
            m += a * a + b * b;
        }
        nsdf[(size_t) tau] = m > 0 ? (float) (2.0 * r / m) : 0.0f;
    }

    // key maxima between positive zero crossings
    std::vector<int> peaks;
    int tau = 1;
    while (tau < maxLag && nsdf[(size_t) tau] > 0) ++tau; // skip the lag-0 lobe
    while (tau < maxLag)
    {
        while (tau < maxLag && nsdf[(size_t) tau] <= 0) ++tau;
        int best = -1;
        while (tau < maxLag && nsdf[(size_t) tau] > 0)
        {
            if (best < 0 || nsdf[(size_t) tau] > nsdf[(size_t) best]) best = tau;
            ++tau;
        }
        if (best >= minLag) peaks.push_back (best);
    }
    if (peaks.empty()) return -1.0f;

    float globalMax = 0;
    for (int p : peaks) globalMax = juce::jmax (globalMax, nsdf[(size_t) p]);
    if (globalMax < 0.6f) return -1.0f;

    int chosen = peaks.front();
    for (int p : peaks)
        if (nsdf[(size_t) p] >= 0.88f * globalMax) { chosen = p; break; }

    // parabolic interpolation
    double lag = chosen;
    if (chosen > 0 && chosen < maxLag)
    {
        const double a = nsdf[(size_t) chosen - 1], b = nsdf[(size_t) chosen], c = nsdf[(size_t) chosen + 1];
        const double d = a - 2 * b + c;
        if (std::abs (d) > 1e-12) lag += 0.5 * (a - c) / d;
    }

    const double f = sampleRate / lag;
    return (float) (69.0 + 12.0 * std::log2 (f / 440.0));
}
} // namespace hhbm::SampleTools
