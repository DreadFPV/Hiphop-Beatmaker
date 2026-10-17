#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "MusicTypes.h"

#include <array>
#include <memory>
#include <vector>

namespace hhbm
{
// ============================================================ small DSP helpers
struct Noise
{
    uint32_t s = 22222u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) ((int32_t) s) * (1.0f / 2147483648.0f); }
};

/** TPT state variable filter (Zavalishin), mono. */
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1.4f, a1 = 0, a2 = 0, a3 = 0;

    void set (float cutoff, float q, float sr)
    {
        cutoff = juce::jlimit (20.0f, sr * 0.45f, cutoff);
        g = std::tan (juce::MathConstants<float>::pi * cutoff / sr);
        k = 1.0f / juce::jmax (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { ic1 = ic2 = 0; }

    // returns lp, bp, hp via refs
    void process (float x, float& lp, float& bp, float& hp)
    {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    float lp (float x) { float l, b, h; process (x, l, b, h); return l; }
    float bp (float x) { float l, b, h; process (x, l, b, h); return b; }
    float hp (float x) { float l, b, h; process (x, l, b, h); return h; }
};

struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
    float value = 0, a = 0.01f, d = 0.2f, s = 0.7f, r = 0.2f, sr = 44100;

    void setTimes (float att, float dec, float sus, float rel, float rate)
    {
        a = att; d = dec; s = sus; r = rel; sr = rate;
    }
    void noteOn (bool retrigger) { if (retrigger) value = juce::jmin (value, 1.0f); stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    bool active() const { return stage != Idle; }

    float next()
    {
        switch (stage)
        {
            case Attack:
                value += 1.0f / juce::jmax (1.0f, a * sr);
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value = s + (value - s) * std::exp (-1.0f / juce::jmax (1.0f, d * sr * 0.3f));
                if (std::abs (value - s) < 1.0e-4f) { value = s; stage = s > 0.0001f ? Sustain : Idle; }
                break;
            case Sustain: value = s; break;
            case Release:
                value *= std::exp (-1.0f / juce::jmax (1.0f, r * sr * 0.3f));
                if (value < 1.0e-4f) { value = 0; stage = Idle; }
                break;
            case Idle: default: value = 0; break;
        }
        return value;
    }
};

// ============================================================ tonal patch
struct Patch
{
    enum Osc { Sine, Saw, Square, Triangle, Fm };
    Osc osc = Sine;
    float fmRatio = 1, fmIndex = 0, fmDecay = 0.5f;
    float att = 0.005f, dec = 0.3f, sus = 0.7f, rel = 0.2f;
    float cutoff = 18000, envToCutoff = 0, q = 0.7f, keyTrack = 0;
    float detune = 0;  int unison = 1;
    float drive = 0;
    float glide = 0;          // seconds
    float vibratoDepth = 0, vibratoRate = 5;
    float pitchEnv = 0, pitchEnvTime = 0.05f; // semitones
    float tremolo = 0;
    float subLevel = 0;
    float gain = 0.5f;
};

// ============================================================ samples
/** A loaded audio file (drum one-shot or instrument sample). */
struct SampleData
{
    juce::AudioBuffer<float> data;
    double sampleRate = 44100;
    juce::String name;
};
using DrumSample = SampleData;

/** Playback settings of an instrument sample slot. */
struct SampleParams
{
    float root = 60;        // MIDI note of the recorded pitch
    float fineCents = 0;    // -100..100
    float attack = 0.002f;  // seconds
    float release = 0.15f;  // seconds
    bool loop = false;
    float loopStart = 0.3f; // 0..1 of the sample
    float loopEnd = 0.9f;   // 0..1 of the sample
};

class Voice
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; }
    void start (int note, float vel, const Patch& p, float freq, bool legato,
                std::shared_ptr<const SampleData> sample = nullptr, const SampleParams& sp = {});
    void glideTo (int note, float freq);
    void release() { env.noteOff(); }
    void kill()    { env.stage = Adsr::Idle; env.value = 0; note = -1; }
    bool active() const { return env.active(); }
    void render (float* l, float* r, int n);

    int note = -1;
    uint64_t age = 0;

private:
    float poly (float t, float dt) const;
    void renderSample (float* l, float* r, int n);
    float readSample (int ch, double pos) const;
    float sr = 44100;
    std::shared_ptr<const SampleData> smp;
    SampleParams sp;
    double spos = 0;
    int loopA = 0, loopB = 0, xfade = 0;
    Patch patch;
    Adsr env;
    float vel = 1, freq = 440, targetFreq = 440, glideCoef = 0;
    std::array<float, 4> phase {};
    float modPhase = 0, fmEnv = 0, pitchEnvVal = 0, lfo = 0, time = 0;
    Svf filterL, filterR;
};

// ============================================================ drums

class DrumVoice
{
public:
    void start (int slot, float vel, int style, float sr, std::shared_ptr<const DrumSample> sample);
    void choke() { if (active) chokeGain = 0.995f; }
    void render (float* l, float* r, int n);

    bool active = false;
    int slot = -1;

private:
    float synth();
    float metallic();
    std::array<float, 6> mphase {};
    int style = 0;
    float sr = 44100, vel = 1, t = 0, phase = 0, gain = 1, chokeGain = 1;
    Noise noise;
    Svf f1, f2;
    std::shared_ptr<const DrumSample> sample;
    double samplePos = 0, sampleInc = 1;
};

// ============================================================ engine
class SynthEngine
{
public:
    void prepare (double sampleRate, int blockSize);
    void setStyle (int s);
    void setTuning (float a4) { a4Hz = a4; }
    void setTrackGain (int track, float g) { trackGain[(size_t) track] = g; }

    void noteOn (int track, int note, float velocity);
    void noteOff (int track, int note);
    void allNotesOff();

    /** Sample slots for the drum kit — the pointer is swapped atomically from the message thread. */
    void setSample (int slot, std::shared_ptr<const DrumSample> s);
    std::shared_ptr<const DrumSample> getSample (int slot) const;

    /** Instrument samples for Bass, Keys, Pad, Lead (nullptr = built-in synth). */
    void setInstrumentSample (int track, std::shared_ptr<const SampleData> s);
    std::shared_ptr<const SampleData> getInstrumentSample (int track) const;
    void setSampleParams (int track, const SampleParams& p) { instParams[(size_t) track] = p; }

    void render (juce::AudioBuffer<float>& buffer, int start, int num);

private:
    float freqFor (int note) const { return a4Hz * std::pow (2.0f, (note - 69) / 12.0f); }
    Voice* findPolyVoice (std::vector<Voice>& pool);
    void updatePatches();

    double sr = 44100;
    int style = 0;
    float a4Hz = 440;
    std::array<float, numTracks> trackGain { 1, 1, 1, 1, 1 };

    std::array<DrumVoice, 24> drumVoices;
    std::array<std::shared_ptr<const DrumSample>, numDrumSlots> samples;
    std::array<std::shared_ptr<const SampleData>, numTracks> instSamples;
    std::array<SampleParams, numTracks> instParams;
    mutable juce::SpinLock sampleLock;

    // mono synths keep a note stack for legato / 808 glides
    struct Mono
    {
        Voice voice;
        std::vector<int> stack;
        float vel = 1;
    };
    Mono bass, lead;
    std::vector<Voice> keys, pad;
    Patch bassPatch, leadPatch, keysPatch, padPatch;
    uint64_t counter = 0;

    juce::AudioBuffer<float> temp;
};
} // namespace hhbm
