#include "SynthEngine.h"

namespace hhbm
{
static constexpr float twoPi = juce::MathConstants<float>::twoPi;

// ============================================================ Voice
void Voice::start (int n, float v, const Patch& p, float f, bool legato,
                   std::shared_ptr<const SampleData> sample, const SampleParams& params)
{
    const bool fresh = ! legato || ! env.active();
    patch = p;
    note = n;
    vel = v;
    targetFreq = f;
    if (fresh)
    {
        freq = f;
        for (size_t i = 0; i < phase.size(); ++i)
            phase[i] = 0.25f * (float) i;
        modPhase = 0;
        filterL.reset();
        filterR.reset();
        time = 0;
        lfo = 0;
        spos = 0;
    }
    glideCoef = p.glide > 0 ? std::exp (-1.0f / (p.glide * sr * 0.35f)) : 0.0f;

    smp = std::move (sample);
    sp = params;
    if (smp != nullptr && smp->data.getNumSamples() > 2)
    {
        const int len = smp->data.getNumSamples();
        loopA = juce::jlimit (0, len - 2, (int) (sp.loopStart * (float) len));
        loopB = juce::jlimit (0, len - 1, (int) (sp.loopEnd * (float) len));
        if (loopB - loopA < 64) sp.loop = false;
        xfade = juce::jmin ((int) (smp->sampleRate * 0.012), (loopB - loopA) / 4);
        env.setTimes (sp.attack, 0.05f, 1.0f, sp.release, sr);
    }
    else
    {
        smp = nullptr;
        env.setTimes (p.att, p.dec, p.sus, p.rel, sr);
    }
    env.noteOn (true);
    fmEnv = 1.0f;
    pitchEnvVal = p.pitchEnv;
}

float Voice::readSample (int ch, double pos) const
{
    const auto& d = smp->data;
    const int idx = (int) pos;
    if (idx < 0 || idx + 1 >= d.getNumSamples()) return 0.0f;
    const float* x = d.getReadPointer (juce::jmin (ch, d.getNumChannels() - 1));
    const float frac = (float) (pos - idx);
    return x[idx] + frac * (x[idx + 1] - x[idx]);
}

void Voice::renderSample (float* outL, float* outR, int n)
{
    const float peCoef = std::exp (-1.0f / (juce::jmax (0.001f, patch.pitchEnvTime) * sr));
    const double rootHz = 440.0 * std::exp2 ((sp.root - 69.0) / 12.0) * std::exp2 (sp.fineCents / 1200.0);
    const double rateRatio = smp->sampleRate / sr;
    const double len = smp->data.getNumSamples();
    const double loopLen = loopB - loopA;

    for (int i = 0; i < n; ++i)
    {
        const float e = env.next();
        if (! env.active()) { note = -1; break; }

        if (glideCoef > 0) freq = targetFreq + (freq - targetFreq) * glideCoef;
        else               freq = targetFreq;
        pitchEnvVal *= peCoef;
        lfo += patch.vibratoRate / sr;
        if (lfo >= 1) lfo -= 1;
        time += 1.0f / sr;

        const float vibAmt = patch.vibratoDepth * juce::jmin (1.0f, time / 0.35f);
        const float f = freq * std::exp2 ((pitchEnvVal + vibAmt * std::sin (twoPi * lfo)) / 12.0f);
        const double inc = f / rootHz * rateRatio;

        float l = readSample (0, spos), r = readSample (1, spos);

        if (sp.loop && xfade > 0 && spos >= loopB - xfade)
        {
            // crossfade the loop end into the loop start
            const float w = (float) ((spos - (loopB - xfade)) / xfade);
            const double back = spos - loopLen;
            l = l * (1.0f - w) + readSample (0, back) * w;
            r = r * (1.0f - w) + readSample (1, back) * w;
        }

        spos += inc;
        if (sp.loop && spos >= loopB)
            spos -= loopLen;
        else if (! sp.loop && spos >= len - 1)
        {
            kill();
            break;
        }

        const float amp = e * vel * 0.8f;
        outL[i] += l * amp;
        outR[i] += r * amp;
    }
}

void Voice::glideTo (int n, float f)
{
    note = n;
    targetFreq = f;
    if (patch.glide <= 0) freq = f;
}

float Voice::poly (float t, float dt) const
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt){ t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

void Voice::render (float* outL, float* outR, int n)
{
    if (smp != nullptr) { renderSample (outL, outR, n); return; }

    const float fmCoef = std::exp (-1.0f / (juce::jmax (0.01f, patch.fmDecay) * sr));
    const float peCoef = std::exp (-1.0f / (juce::jmax (0.001f, patch.pitchEnvTime) * sr));
    const int uni = juce::jlimit (1, 4, patch.unison);

    for (int i = 0; i < n; ++i)
    {
        const float e = env.next();
        if (! env.active()) { note = -1; break; }

        if (glideCoef > 0) freq = targetFreq + (freq - targetFreq) * glideCoef;
        else               freq = targetFreq;

        pitchEnvVal *= peCoef;
        fmEnv *= fmCoef;
        lfo += patch.vibratoRate / sr;
        if (lfo >= 1) lfo -= 1;
        time += 1.0f / sr;

        const float vibAmt = patch.vibratoDepth * juce::jmin (1.0f, time / 0.35f);
        const float semis = pitchEnvVal + vibAmt * std::sin (twoPi * lfo);
        const float f = freq * std::exp2 (semis / 12.0f);

        float l = 0, r = 0;
        for (int u = 0; u < uni; ++u)
        {
            const float spread = uni > 1 ? ((float) u / (float) (uni - 1) * 2.0f - 1.0f) : 0.0f;
            const float fu = f * std::exp2 (patch.detune * spread / 1200.0f);
            const float dt = juce::jmin (0.49f, fu / sr);
            float& ph = phase[(size_t) u];
            float s = 0;

            switch (patch.osc)
            {
                case Patch::Sine:     s = std::sin (twoPi * ph); break;
                case Patch::Saw:      s = 2.0f * ph - 1.0f - poly (ph, dt); break;
                case Patch::Square:
                {
                    float t2 = ph + 0.5f; if (t2 >= 1) t2 -= 1;
                    s = (ph < 0.5f ? 1.0f : -1.0f) + poly (ph, dt) - poly (t2, dt);
                    break;
                }
                case Patch::Triangle: s = 4.0f * std::abs (ph - 0.5f) - 1.0f; break;
                case Patch::Fm:
                {
                    if (u == 0)
                    {
                        modPhase += fu * patch.fmRatio / sr;
                        modPhase -= std::floor (modPhase);
                    }
                    s = std::sin (twoPi * ph + patch.fmIndex * fmEnv * std::sin (twoPi * modPhase));
                    break;
                }
            }

            ph += dt;
            if (ph >= 1) ph -= 1;

            if (uni == 1) { l += s; r += s; }
            else          { l += s * (1.0f - 0.5f * spread); r += s * (1.0f + 0.5f * spread); }
        }
        if (uni > 1) { l /= (float) uni * 0.7f; r /= (float) uni * 0.7f; }

        if (patch.subLevel > 0)
        {
            const float sub = patch.subLevel * std::sin (twoPi * phase[3]);
            phase[3] += f * 0.5f / sr;
            if (phase[3] >= 1) phase[3] -= 1;
            l += sub; r += sub;
        }

        if ((i & 15) == 0)
        {
            float c = patch.cutoff * std::exp2 (patch.envToCutoff * e);
            if (patch.keyTrack > 0) c *= std::pow (f / 261.6f, patch.keyTrack);
            filterL.set (c, patch.q, sr);
            filterR.set (c, patch.q, sr);
        }
        l = filterL.lp (l);
        r = filterR.lp (r);

        if (patch.drive > 0)
        {
            const float d = 1.0f + patch.drive * 4.0f;
            l = std::tanh (l * d) / std::tanh (d) ;
            r = std::tanh (r * d) / std::tanh (d);
        }

        float amp = e * vel * patch.gain;
        if (patch.tremolo > 0)
            amp *= 1.0f - patch.tremolo * (0.5f + 0.5f * std::sin (twoPi * 4.5f * time));

        outL[i] += l * amp;
        outR[i] += r * amp;
    }
}

// ============================================================ DrumVoice
static const float metallicFreqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };

void DrumVoice::start (int s, float v, int st, float rate, std::shared_ptr<const DrumSample> smp, const DrumTweak& tw)
{
    slot = s; vel = v; style = st; sr = rate;
    tweak = tw;
    tweak.decay = juce::jlimit (0.1f, 8.0f, tweak.decay);
    ratio = std::exp2 (tweak.tune / 12.0f);
    sample = std::move (smp);
    t = 0; phase = 0; gain = 1; chokeGain = 1;
    for (auto& m : mphase) m = 0;
    f1.reset(); f2.reset();
    active = true;
    noise.s = 1234567u + (uint32_t) s * 7919u;

    if (sample != nullptr)
    {
        samplePos = 0;
        sampleInc = sample->sampleRate / sr * ratio;
        return;
    }

    const float tm = std::exp2 (tweak.tone * 1.5f) * std::sqrt (ratio); // filter colour
    switch (slot)
    {
        case 1: f1.set ((style == (int) Style::LoFi ? 2500.0f : 1500.0f) * tm, 0.7f, sr); break;  // snare HP
        case 2: f1.set ((1300) * tm, 1.6f, sr); break;    // clap BP
        case 3: case 4: f1.set ((7500) * tm, 0.8f, sr); f2.set ((11000) * tm, 0.7f, sr); break; // hats HP + LP
        case 5: f1.set ((2000) * tm, 2.0f, sr); break;    // rim
        case 6: f1.set ((6500) * tm, 1.4f, sr); break;    // shaker
        case 7: f1.set ((4500) * tm, 0.7f, sr); break;    // crash
        default: f1.set ((style == (int) Style::LoFi ? 1800.0f : 9000.0f) * tm, 0.7f, sr); break; // kick tone LP
    }
}

float DrumVoice::metallic()
{
    float s = 0;
    for (int i = 0; i < 6; ++i)
    {
        mphase[(size_t) i] += metallicFreqs[i] * 1.6f * ratio / sr;
        if (mphase[(size_t) i] >= 1) mphase[(size_t) i] -= 1;
        s += mphase[(size_t) i] < 0.5f ? 1.0f : -1.0f;
    }
    return s / 6.0f;
}

float DrumVoice::synth()
{
    const float realTime = t / sr;
    const float time = realTime / tweak.decay; // envelope time (Decay stretches it)
    float out = 0, end = 1.0f;
    const bool is808 = style == (int) Style::Trap || style == (int) Style::Drill;

    switch (slot)
    {
        case 0: // kick
        {
            const bool dnb = isDnbFamily (style);
            const float decay = is808 ? 0.22f : (dnb ? (style == (int) Style::Neuro ? 0.16f : 0.2f)
                                : (style == (int) Style::LoFi ? 0.28f : (style == (int) Style::GFunk ? 0.4f : 0.34f)));
            const float f = is808 ? (52.0f + 210.0f * std::exp (-time / 0.018f))
                          : dnb   ? (56.0f + 180.0f * std::exp (-time / 0.022f))
                                  : (48.0f + 130.0f * std::exp (-time / 0.035f));
            phase += f * ratio / sr;
            if (phase >= 1) phase -= 1;
            out = std::sin (twoPi * phase) * std::exp (-time / decay);
            out += noise.next() * std::exp (-time / 0.003f) * (style == (int) Style::LoFi ? 0.1f : 0.35f) * (1.0f + tweak.tone);
            out = std::tanh (out * (style == (int) Style::BoomBap ? 2.2f : 1.5f));
            out = f1.lp (out) * 0.95f;
            end = 1.3f;
            break;
        }
        case 1: // snare
        {
            phase += ((isDnbFamily (style) ? 215.0f : 185.0f) + 60.0f * std::exp (-time / 0.01f)) * ratio / sr;
            if (phase >= 1) phase -= 1;
            const float tone = std::sin (twoPi * phase) * std::exp (-time / 0.05f) * 0.6f;
            const float nz = f1.hp (noise.next()) * std::exp (-time / (style == (int) Style::LoFi ? 0.11f : (isDnbFamily (style) ? 0.13f : 0.16f)))
                             * (isDnbFamily (style) ? 1.3f : 1.0f);
            out = (tone + nz) * 0.75f;
            end = 0.7f;
            break;
        }
        case 2: // clap
        {
            float e;
            if (time < 0.03f) e = std::exp (-std::fmod (time, 0.01f) / 0.003f);
            else              e = std::exp (-(time - 0.03f) / 0.13f);
            out = f1.bp (noise.next()) * e * 1.6f;
            end = 0.7f;
            break;
        }
        case 3: case 4: // hats
        {
            const float s = metallic() * 0.6f + noise.next() * 0.5f;
            const float decay = slot == 3 ? 0.035f : 0.32f;
            out = f2.lp (f1.hp (s)) * std::exp (-time / decay) * 0.9f;
            end = slot == 3 ? 0.3f : 1.4f;
            break;
        }
        case 5: // rim
        {
            out = std::sin (twoPi * 1700.0f * ratio * realTime) * std::exp (-time / 0.012f)
                + std::sin (twoPi * 820.0f * ratio * realTime) * std::exp (-time / 0.02f) * 0.6f
                + f1.bp (noise.next()) * std::exp (-time / 0.008f) * 0.4f;
            end = 0.25f;
            break;
        }
        case 6: // shaker
        {
            const float e = (1.0f - std::exp (-time / 0.008f)) * std::exp (-time / 0.05f);
            out = f1.bp (noise.next()) * e * 1.2f;
            end = 0.35f;
            break;
        }
        case 7: // crash
        {
            const float s = metallic() * 0.4f + noise.next() * 0.6f;
            out = f1.hp (s) * std::exp (-time / 1.1f) * 0.8f;
            end = 4.0f;
            break;
        }
        default: end = 0; break;
    }

    if (time > end) active = false;
    return out;
}

void DrumVoice::render (float* l, float* r, int n)
{
    static const float slotGain[numDrumSlots] = { 0.95f, 0.65f, 0.55f, 0.28f, 0.26f, 0.38f, 0.22f, 0.2f };

    for (int i = 0; i < n && active; ++i)
    {
        float sL, sR;
        if (sample != nullptr)
        {
            const int len = sample->data.getNumSamples();
            const int idx = (int) samplePos;
            if (idx + 1 >= len) { active = false; break; }
            const float frac = (float) (samplePos - idx);
            auto read = [&] (int ch)
            {
                const float* d = sample->data.getReadPointer (juce::jmin (ch, sample->data.getNumChannels() - 1));
                return d[idx] + frac * (d[idx + 1] - d[idx]);
            };
            sL = read (0);
            sR = read (1);
            samplePos += sampleInc;
            float g = vel * 0.6f * tweak.level;
            if (tweak.decay < 1.0f)
                g *= std::exp (-(t / sr) / (0.4f * (float) (len / sample->sampleRate) * tweak.decay));
            sL *= g; sR *= g;
        }
        else
        {
            sL = sR = synth() * vel * slotGain[slot] * 0.55f * tweak.level;
        }

        if (chokeGain < 1.0f)
        {
            gain *= chokeGain;
            if (gain < 1.0e-3f) active = false;
        }
        l[i] += sL * gain;
        r[i] += sR * gain;
        t += 1.0f;
    }
}

// ============================================================ engine
void SynthEngine::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate;
    temp.setSize (2, juce::jmax (512, blockSize));
    keys.assign (12, {});
    pad.assign (12, {});
    for (auto& v : keys) v.prepare (sr);
    for (auto& v : pad)  v.prepare (sr);
    bass.voice.prepare (sr);
    lead.voice.prepare (sr);
    for (auto& d : drumVoices) d.active = false;
    for (int t = 1; t < numTracks; ++t)
        patches[(size_t) t] = stylePatch (style, t);
}

void SynthEngine::setStyle (int s)
{
    style = s; // drum synthesis flavour; tonal patches come from setPatch()
}

Patch SynthEngine::stylePatch (int style, int track)
{
    Patch b, l, k, p;

    switch ((Style) style)
    {
        case Style::BoomBap:
            b.osc = Patch::Sine; b.drive = 0.35f; b.att = 0.003f; b.dec = 0.6f; b.sus = 0.35f; b.rel = 0.12f; b.cutoff = 900; b.envToCutoff = 1; b.gain = 0.7f;
            l.osc = Patch::Triangle; l.vibratoDepth = 0.15f; l.att = 0.04f; l.dec = 0.3f; l.sus = 0.8f; l.rel = 0.15f; l.cutoff = 3500; l.gain = 0.25f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.8f; k.fmDecay = 0.25f; k.att = 0.002f; k.dec = 1.2f; k.sus = 0; k.rel = 0.3f; k.cutoff = 5000; k.drive = 0.2f; k.gain = 0.18f;
            p.cutoff = 1600;
            break;
        case Style::Trap:
            b.osc = Patch::Sine; b.drive = 0.6f; b.att = 0.002f; b.dec = 1.6f; b.sus = 0.55f; b.rel = 0.15f; b.glide = 0.06f; b.pitchEnv = 2.0f; b.pitchEnvTime = 0.03f; b.cutoff = 6000; b.gain = 0.75f;
            l.osc = Patch::Fm; l.fmRatio = 3.5f; l.fmIndex = 2.5f; l.fmDecay = 0.6f; l.att = 0.001f; l.dec = 1.0f; l.sus = 0; l.rel = 0.3f; l.gain = 0.26f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.2f; k.fmDecay = 0.3f; k.att = 0.002f; k.dec = 1.0f; k.sus = 0; k.rel = 0.4f; k.cutoff = 3000; k.gain = 0.2f;
            p.cutoff = 1800;
            break;
        case Style::Drill:
            b.osc = Patch::Sine; b.drive = 0.8f; b.att = 0.002f; b.dec = 1.8f; b.sus = 0.6f; b.rel = 0.15f; b.glide = 0.09f; b.pitchEnv = 2.0f; b.pitchEnvTime = 0.03f; b.cutoff = 6000; b.gain = 0.72f;
            l.osc = Patch::Square; l.cutoff = 1800; l.q = 0.9f; l.att = 0.01f; l.dec = 0.4f; l.sus = 0.6f; l.rel = 0.2f; l.glide = 0.05f; l.vibratoDepth = 0.1f; l.gain = 0.18f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.2f; k.fmDecay = 0.3f; k.att = 0.002f; k.dec = 1.0f; k.sus = 0; k.rel = 0.4f; k.cutoff = 2500; k.gain = 0.2f;
            p.cutoff = 1400; p.vibratoDepth = 0.05f;
            break;
        case Style::LoFi:
            b.osc = Patch::Triangle; b.cutoff = 700; b.att = 0.01f; b.dec = 0.8f; b.sus = 0.4f; b.rel = 0.2f; b.gain = 0.9f;
            l.osc = Patch::Fm; l.fmRatio = 1; l.fmIndex = 0.6f; l.fmDecay = 1.0f; l.att = 0.01f; l.dec = 0.8f; l.sus = 0.3f; l.rel = 0.3f; l.cutoff = 2500; l.gain = 0.25f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.0f; k.fmDecay = 0.6f; k.tremolo = 0.25f; k.att = 0.003f; k.dec = 2.0f; k.sus = 0.2f; k.rel = 0.4f; k.cutoff = 2200; k.gain = 0.16f;
            p.cutoff = 900;
            break;
        case Style::DnB:
            b.osc = Patch::Sine; b.drive = 0.35f; b.att = 0.003f; b.dec = 0.9f; b.sus = 0.75f; b.rel = 0.1f; b.glide = 0.03f; b.cutoff = 3000; b.gain = 0.8f;
            l.osc = Patch::Fm; l.fmRatio = 2.0f; l.fmIndex = 1.8f; l.fmDecay = 0.25f; l.att = 0.002f; l.dec = 0.4f; l.sus = 0.0f; l.rel = 0.2f; l.cutoff = 6000; l.gain = 0.26f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.4f; k.fmDecay = 0.3f; k.att = 0.002f; k.dec = 0.6f; k.sus = 0.1f; k.rel = 0.25f; k.cutoff = 3500; k.gain = 0.18f;
            p.cutoff = 2200;
            break;
        case Style::Neuro:
            // reese / growl bass
            b.osc = Patch::Saw; b.unison = 3; b.detune = 18; b.cutoff = 380; b.envToCutoff = 3.0f; b.q = 2.5f; b.drive = 0.8f;
            b.att = 0.002f; b.dec = 0.25f; b.sus = 0.4f; b.rel = 0.06f; b.subLevel = 0.6f; b.gain = 0.6f;
            l.osc = Patch::Saw; l.unison = 2; l.detune = 25; l.cutoff = 2000; l.q = 1.2f; l.glide = 0.03f; l.vibratoDepth = 0.1f;
            l.att = 0.005f; l.dec = 0.3f; l.sus = 0.5f; l.rel = 0.15f; l.gain = 0.26f;
            k.osc = Patch::Square; k.cutoff = 1500; k.envToCutoff = 2.0f; k.q = 1.5f; k.drive = 0.4f; k.att = 0.001f; k.dec = 0.2f; k.sus = 0.0f; k.rel = 0.08f; k.gain = 0.22f;
            p.cutoff = 900;
            break;
        case Style::Liquid:
            b.osc = Patch::Triangle; b.cutoff = 600; b.att = 0.01f; b.dec = 1.0f; b.sus = 0.7f; b.rel = 0.2f; b.glide = 0.04f; b.gain = 0.85f;
            l.osc = Patch::Triangle; l.vibratoDepth = 0.15f; l.vibratoRate = 5.0f; l.att = 0.03f; l.dec = 0.5f; l.sus = 0.7f; l.rel = 0.4f; l.cutoff = 3500; l.glide = 0.05f; l.gain = 0.25f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.0f; k.fmDecay = 0.6f; k.tremolo = 0.2f; k.att = 0.003f; k.dec = 2.0f; k.sus = 0.25f; k.rel = 0.5f; k.cutoff = 2600; k.gain = 0.16f;
            p.cutoff = 1600;
            break;
        case Style::Jungle:
            b.osc = Patch::Sine; b.drive = 0.25f; b.att = 0.004f; b.dec = 1.2f; b.sus = 0.6f; b.rel = 0.15f; b.cutoff = 2000; b.gain = 0.8f;
            l.osc = Patch::Square; l.cutoff = 2500; l.q = 0.9f; l.att = 0.02f; l.dec = 0.3f; l.sus = 0.6f; l.rel = 0.15f; l.gain = 0.16f;
            k.osc = Patch::Fm; k.fmRatio = 2.0f; k.fmIndex = 0.8f; k.fmDecay = 0.15f; k.att = 0.001f; k.dec = 0.15f; k.sus = 0.0f; k.rel = 0.06f; k.cutoff = 5000; k.gain = 0.18f;
            p.cutoff = 1400;
            break;
        case Style::GFunk:
        default:
            b.osc = Patch::Saw; b.cutoff = 500; b.envToCutoff = 2.5f; b.q = 1.2f; b.att = 0.002f; b.dec = 0.18f; b.sus = 0.35f; b.rel = 0.08f; b.drive = 0.2f; b.glide = 0.02f; b.subLevel = 0.5f; b.gain = 0.95f;
            l.osc = Patch::Triangle; l.glide = 0.12f; l.vibratoDepth = 0.3f; l.vibratoRate = 5.5f; l.att = 0.03f; l.dec = 0.3f; l.sus = 0.9f; l.rel = 0.2f; l.cutoff = 4000; l.gain = 0.28f;
            k.osc = Patch::Fm; k.fmRatio = 1; k.fmIndex = 1.0f; k.fmDecay = 0.5f; k.tremolo = 0.15f; k.att = 0.003f; k.dec = 0.5f; k.sus = 0.3f; k.rel = 0.3f; k.cutoff = 4000; k.gain = 0.16f;
            p.cutoff = 2000;
            break;
    }

    // pad: shared base, cutoff per style
    p.osc = Patch::Saw; p.unison = 3; p.detune = 12; p.att = 0.4f; p.dec = 1.0f; p.sus = 0.8f; p.rel = 0.8f; p.q = 0.6f; p.gain = 0.09f;

    // mix balance between the instruments
    b.gain *= 0.6f; k.gain *= 1.9f; l.gain *= 1.4f; p.gain *= 1.4f;

    switch ((Track) track)
    {
        case Track::Bass: return b;
        case Track::Keys: return k;
        case Track::Pad:  return p;
        case Track::Lead: return l;
        default:          return {};
    }
}

void SynthEngine::setSample (int slot, std::shared_ptr<const DrumSample> s)
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    samples[(size_t) slot] = std::move (s);
}

std::shared_ptr<const DrumSample> SynthEngine::getSample (int slot) const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return samples[(size_t) slot];
}

void SynthEngine::setInstrumentSample (int track, std::shared_ptr<const SampleData> s)
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    instSamples[(size_t) track] = std::move (s);
}

std::shared_ptr<const SampleData> SynthEngine::getInstrumentSample (int track) const
{
    const juce::SpinLock::ScopedLockType sl (sampleLock);
    return instSamples[(size_t) track];
}

Voice* SynthEngine::findPolyVoice (std::vector<Voice>& pool)
{
    Voice* oldest = nullptr;
    for (auto& v : pool)
    {
        if (! v.active()) return &v;
        if (oldest == nullptr || v.age < oldest->age) oldest = &v;
    }
    if (oldest) oldest->kill();
    return oldest;
}

void SynthEngine::noteOn (int track, int note, float velocity)
{
    ++counter;
    switch ((Track) track)
    {
        case Track::Drums:
        {
            const int slot = drumNoteToSlot (note);
            if (slot < 0) return;
            if (slot == 3) // closed hat chokes open hat
                for (auto& d : drumVoices)
                    if (d.active && d.slot == 4) d.choke();

            DrumVoice* target = nullptr;
            for (auto& d : drumVoices)
                if (! d.active) { target = &d; break; }
            if (target == nullptr) target = &drumVoices[counter % drumVoices.size()];
            target->start (slot, velocity, style, (float) sr, getSample (slot), drumTweaks[(size_t) slot]);
            break;
        }
        case Track::Bass:
        case Track::Lead:
        {
            auto& m = track == (int) Track::Bass ? bass : lead;
            const auto& p = patches[(size_t) track];
            m.stack.erase (std::remove (m.stack.begin(), m.stack.end(), note), m.stack.end());
            m.stack.push_back (note);
            const bool legato = m.voice.active() && p.glide > 0 && m.stack.size() > 1;
            if (legato) m.voice.glideTo (note, freqFor (note));
            else        m.voice.start (note, velocity, p, freqFor (note), false,
                                       getInstrumentSample (track), instParams[(size_t) track]);
            break;
        }
        case Track::Keys:
        case Track::Pad:
        {
            auto& pool = track == (int) Track::Keys ? keys : pad;
            if (auto* v = findPolyVoice (pool))
            {
                v->start (note, velocity, patches[(size_t) track], freqFor (note), false,
                          getInstrumentSample (track), instParams[(size_t) track]);
                v->age = counter;
            }
            break;
        }
        default: break;
    }
}

void SynthEngine::noteOff (int track, int note)
{
    switch ((Track) track)
    {
        case Track::Bass:
        case Track::Lead:
        {
            auto& m = track == (int) Track::Bass ? bass : lead;
            m.stack.erase (std::remove (m.stack.begin(), m.stack.end(), note), m.stack.end());
            if (m.voice.note == note)
            {
                if (! m.stack.empty()) m.voice.glideTo (m.stack.back(), freqFor (m.stack.back()));
                else                   m.voice.release();
            }
            break;
        }
        case Track::Keys:
        case Track::Pad:
            for (auto& v : (track == (int) Track::Keys ? keys : pad))
                if (v.note == note && v.active()) v.release();
            break;
        default: break; // drums are one-shots
    }
}

void SynthEngine::allNotesOff()
{
    bass.stack.clear(); lead.stack.clear();
    bass.voice.release(); lead.voice.release();
    for (auto& v : keys) v.release();
    for (auto& v : pad)  v.release();
}

void SynthEngine::render (juce::AudioBuffer<float>& buffer, int start, int num)
{
    const int chans = buffer.getNumChannels();
    if (chans == 0) return;

    while (num > 0)
    {
        const int n = juce::jmin (num, temp.getNumSamples());

        for (int tr = 0; tr < numTracks; ++tr)
        {
            temp.clear (0, n);
            float* l = temp.getWritePointer (0);
            float* r = temp.getWritePointer (1);

            switch ((Track) tr)
            {
                case Track::Drums: for (auto& d : drumVoices) if (d.active) d.render (l, r, n); break;
                case Track::Bass:  if (bass.voice.active()) bass.voice.render (l, r, n); break;
                case Track::Lead:  if (lead.voice.active()) lead.voice.render (l, r, n); break;
                case Track::Keys:  for (auto& v : keys) if (v.active()) v.render (l, r, n); break;
                case Track::Pad:   for (auto& v : pad)  if (v.active()) v.render (l, r, n); break;
                default: break;
            }

            const float g = trackGain[(size_t) tr];
            buffer.addFrom (0, start, temp, 0, 0, n, g);
            if (chans > 1) buffer.addFrom (1, start, temp, 1, 0, n, g);
        }

        // gentle master soft clip
        for (int ch = 0; ch < juce::jmin (2, chans); ++ch)
        {
            auto* d = buffer.getWritePointer (ch, start);
            for (int i = 0; i < n; ++i)
                d[i] = std::tanh (d[i]);
        }

        start += n;
        num -= n;
    }
}
} // namespace hhbm
