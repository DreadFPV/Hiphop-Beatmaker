#pragma once

#include "SynthEngine.h"

namespace hhbm
{
/** All editable synth parameters of a tonal track (Bass, Keys, Pad, Lead). Parameter ID = "p" + track + key. */
struct SynthParamDef
{
    const char* key;
    const char* name;
    const char* group;
    float min, max, skewCentre; // skewCentre <= 0 -> linear
    int kind;                   // 0 = float, 1 = choice (osc), 2 = int
    const char* unit;
};

enum SynthParam
{
    spOsc, spUnison, spDetune, spSub, spFmRatio, spFmIndex, spFmDecay,
    spCutoff, spRes, spFilterEnv, spKeyTrack, spDrive,
    spAttack, spDecay, spSustain, spRelease, spLevel,
    spGlide, spVibDepth, spVibRate, spPitchEnv, spPitchTime, spTremolo,
    numSynthParams
};

inline const SynthParamDef& synthParamDef (int i)
{
    static const SynthParamDef defs[numSynthParams] = {
        { "osc", "Wellenform", "OSZILLATOR", 0, 4, 0, 1, "" },
        { "uni", "Unison",     "OSZILLATOR", 1, 4, 0, 2, "" },
        { "det", "Detune",     "OSZILLATOR", 0, 50, 0, 0, "ct" },
        { "sub", "Sub",        "OSZILLATOR", 0, 1, 0, 0, "" },
        { "fmr", "FM Ratio",   "OSZILLATOR", 0.25f, 8, 1.5f, 0, "" },
        { "fmi", "FM Index",   "OSZILLATOR", 0, 10, 2, 0, "" },
        { "fmd", "FM Decay",   "OSZILLATOR", 0.01f, 3, 0.5f, 0, "s" },

        { "cut", "Cutoff",     "FILTER", 20, 18000, 1200, 0, "Hz" },
        { "res", "Resonanz",   "FILTER", 0.3f, 8, 1.2f, 0, "" },
        { "env", "Filter Env", "FILTER", 0, 6, 0, 0, "Okt" },
        { "key", "Keytrack",   "FILTER", 0, 1, 0, 0, "" },
        { "drv", "Drive",      "FILTER", 0, 1, 0, 0, "" },

        { "att", "Attack",     "HUELLKURVE", 0.001f, 4, 0.2f, 0, "s" },
        { "dec", "Decay",      "HUELLKURVE", 0.01f, 6, 0.6f, 0, "s" },
        { "sus", "Sustain",    "HUELLKURVE", 0, 1, 0, 0, "" },
        { "rel", "Release",    "HUELLKURVE", 0.01f, 6, 0.5f, 0, "s" },
        { "lvl", "Level",      "HUELLKURVE", 0, 1.5f, 0.4f, 0, "" },

        { "gld", "Glide",      "MODULATION", 0, 0.5f, 0.08f, 0, "s" },
        { "vbd", "Vibrato",    "MODULATION", 0, 1, 0, 0, "st" },
        { "vbr", "Vib Rate",   "MODULATION", 0.5f, 10, 0, 0, "Hz" },
        { "pen", "Pitch Env",  "MODULATION", -24, 24, 0, 0, "st" },
        { "pet", "Pitch Zeit", "MODULATION", 0.001f, 0.5f, 0.05f, 0, "s" },
        { "trm", "Tremolo",    "MODULATION", 0, 1, 0, 0, "" },
    };
    return defs[i];
}

inline const char* oscName (int i)
{
    static const char* n[] = { "Sine", "Saw", "Square", "Triangle", "FM" };
    return n[i];
}

inline float getPatchValue (const Patch& p, int i)
{
    switch (i)
    {
        case spOsc:       return (float) p.osc;
        case spUnison:    return (float) p.unison;
        case spDetune:    return p.detune;
        case spSub:       return p.subLevel;
        case spFmRatio:   return p.fmRatio;
        case spFmIndex:   return p.fmIndex;
        case spFmDecay:   return p.fmDecay;
        case spCutoff:    return p.cutoff;
        case spRes:       return p.q;
        case spFilterEnv: return p.envToCutoff;
        case spKeyTrack:  return p.keyTrack;
        case spDrive:     return p.drive;
        case spAttack:    return p.att;
        case spDecay:     return p.dec;
        case spSustain:   return p.sus;
        case spRelease:   return p.rel;
        case spLevel:     return p.gain;
        case spGlide:     return p.glide;
        case spVibDepth:  return p.vibratoDepth;
        case spVibRate:   return p.vibratoRate;
        case spPitchEnv:  return p.pitchEnv;
        case spPitchTime: return p.pitchEnvTime;
        case spTremolo:   return p.tremolo;
        default:          return 0;
    }
}

inline void setPatchValue (Patch& p, int i, float v)
{
    switch (i)
    {
        case spOsc:       p.osc = (Patch::Osc) juce::jlimit (0, 4, (int) std::round (v)); break;
        case spUnison:    p.unison = juce::jlimit (1, 4, (int) std::round (v)); break;
        case spDetune:    p.detune = v; break;
        case spSub:       p.subLevel = v; break;
        case spFmRatio:   p.fmRatio = v; break;
        case spFmIndex:   p.fmIndex = v; break;
        case spFmDecay:   p.fmDecay = v; break;
        case spCutoff:    p.cutoff = v; break;
        case spRes:       p.q = v; break;
        case spFilterEnv: p.envToCutoff = v; break;
        case spKeyTrack:  p.keyTrack = v; break;
        case spDrive:     p.drive = v; break;
        case spAttack:    p.att = v; break;
        case spDecay:     p.dec = v; break;
        case spSustain:   p.sus = v; break;
        case spRelease:   p.rel = v; break;
        case spLevel:     p.gain = v; break;
        case spGlide:     p.glide = v; break;
        case spVibDepth:  p.vibratoDepth = v; break;
        case spVibRate:   p.vibratoRate = v; break;
        case spPitchEnv:  p.pitchEnv = v; break;
        case spPitchTime: p.pitchEnvTime = v; break;
        case spTremolo:   p.tremolo = v; break;
        default: break;
    }
}

// ---------------------------------------------------------------- drums: "d" + slot + key
enum DrumParam { dpTune, dpDecay, dpTone, dpLevel, numDrumParams };

inline const SynthParamDef& drumParamDef (int i)
{
    static const SynthParamDef defs[numDrumParams] = {
        { "tune", "Tune",  "DRUM", -24, 24, 0, 0, "st" },
        { "dec",  "Decay", "DRUM", 0.25f, 4, 1, 0, "x" },
        { "tone", "Tone",  "DRUM", -1, 1, 0, 0, "" },
        { "lvl",  "Level", "DRUM", 0, 1.5f, 0, 0, "" },
    };
    return defs[i];
}

inline float drumDefault (int i) { return i == dpDecay || i == dpLevel ? 1.0f : 0.0f; }

inline juce::String synthParamId (int track, int param) { return "p" + juce::String (track) + synthParamDef (param).key; }
inline juce::String drumParamId (int slot, int param)   { return "d" + juce::String (slot) + drumParamDef (param).key; }
} // namespace hhbm
