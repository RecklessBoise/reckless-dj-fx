#include "BeatFx.h"

namespace rdfx
{
namespace
{
constexpr double kMaxDelaySeconds = 8.5;

double clampDelay (double samples, int capacity) noexcept
{
    // Halve musically until it fits (e.g. 16 beats at a slow tempo)
    while (samples > capacity && samples > 2.0)
        samples *= 0.5;
    return juce::jmax (1.0, samples);
}

//==============================================================================
/** DELAY, ECHO, PING PONG, SPIRAL and HELIX share one feedback delay design. */
class DelayFamily final : public BeatEffect
{
public:
    enum class Mode { Delay, Echo, PingPong, Spiral, Helix };
    explicit DelayFamily (Mode m) : mode (m) {}

    void prepare (double sr) override
    {
        sampleRate = sr;
        line.prepare (sr, kMaxDelaySeconds);
        const float damp = mode == Mode::Delay ? 9000.0f : mode == Mode::Echo ? 4500.0f
                         : mode == Mode::PingPong ? 6000.0f : mode == Mode::Spiral ? 5500.0f : 7000.0f;
        for (auto& f : dampL) f.setCutoff (damp, sr);
        for (auto& f : dampR) f.setCutoff (damp, sr);
        hpL.setCutoff (70.0f, sr);
        hpR.setCutoff (70.0f, sr);
        const int primes[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i)
        {
            diffL[(size_t) i].prepare ((int) (primes[i] * sr / 44100.0));
            diffR[(size_t) i].prepare ((int) ((primes[i] + 23) * sr / 44100.0));
        }
        reset();
    }

    void reset() override
    {
        line.reset();
        for (auto& f : dampL) f.reset();
        for (auto& f : dampR) f.reset();
        hpL.reset(); hpR.reset();
        for (auto& a : diffL) a.reset();
        for (auto& a : diffR) a.reset();
        delaySmoothed = -1.0f;
    }

    void beginBlock (const BeatBlockCtx& ctx) override
    {
        BeatEffect::beginBlock (ctx);
        float glide = 0.04f;
        if (mode == Mode::Spiral) glide = 0.25f;
        else if (mode == Mode::Helix) glide = 0.35f;
        else if (ctx.tape) glide = 0.45f; // X-Pad tape mode: pitch-bending time changes
        smoother.setTime (glide, ctx.sampleRate);
        target = (float) clampDelay (ctx.timeSamples, line.capacity());
        if (delaySmoothed < 0.0f)
        {
            delaySmoothed = target;
            smoother.reset (target);
        }
    }

    bool isAdditive() const noexcept override { return true; }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        delaySmoothed = smoother.process (target);
        float wl = line.readL (delaySmoothed);
        float wr = line.readR (delaySmoothed);

        switch (mode)
        {
            case Mode::Delay:
                push (inL + 0.28f * dampL[0].process (wl), inR + 0.28f * dampR[0].process (wr));
                break;
            case Mode::Echo:
                push (inL + 0.64f * dampL[0].process (wl), inR + 0.64f * dampR[0].process (wr));
                break;
            case Mode::PingPong:
            {
                const float mono = 0.5f * (inL + inR);
                // Left tap feeds the right line and vice versa -> bouncing repeats
                push (mono + 0.62f * dampR[0].process (wr), 0.62f * dampL[0].process (wl));
                break;
            }
            case Mode::Spiral:
            {
                float dl = dampL[0].process (wl), dr = dampR[0].process (wr);
                for (auto& a : diffL) dl = a.process (dl);
                for (auto& a : diffR) dr = a.process (dr);
                push (inL + 0.74f * dl, inR + 0.74f * dr);
                wl = 0.6f * wl + 0.4f * dl;
                wr = 0.6f * wr + 0.4f * dr;
                break;
            }
            case Mode::Helix:
            {
                const float dl = dampL[0].process (wl);
                const float dr = dampR[0].process (wr);
                push (0.75f * inL + 0.93f * (dl - hpL.process (dl)), 0.75f * inR + 0.93f * (dr - hpR.process (dr)));
                break;
            }
        }

        outL = wl;
        outR = wr;
        advance();
    }

private:
    void push (float l, float r) noexcept { line.push (sanitize (softClip (l)), sanitize (softClip (r))); }

    Mode mode;
    double sampleRate = 44100.0;
    StereoDelayLine line;
    OnePole smoother, hpL, hpR;
    std::array<OnePole, 1> dampL, dampR;
    std::array<Allpass, 4> diffL, diffR;
    float target = 1.0f, delaySmoothed = -1.0f;
};

//==============================================================================
class ReverbFx final : public BeatEffect
{
public:
    void prepare (double sr) override { reverb.setSampleRate (sr); reset(); }
    void reset() override { reverb.reset(); }
    bool isAdditive() const noexcept override { return true; }

    void beginBlock (const BeatBlockCtx& ctx) override
    {
        BeatEffect::beginBlock (ctx);
        juce::Reverb::Parameters p;
        p.roomSize = 0.30f + 0.69f * juce::jlimit (0.0f, 1.0f, ctx.amount);
        p.damping = 0.35f;
        p.width = 1.0f;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        reverb.setParameters (p);
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        float l = inL, r = inR;
        reverb.processStereo (&l, &r, 1);
        outL = l * 0.8f;
        outR = r * 0.8f;
        advance();
    }

private:
    juce::Reverb reverb;
};

//==============================================================================
class FlangerFx final : public BeatEffect
{
public:
    void prepare (double sr) override { sampleRate = sr; line.prepare (sr, 0.03); reset(); }
    void reset() override { line.reset(); fbL = fbR = 0.0f; }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        const float pL = phase (blockCtx.cycleBeats);
        const float pR = wrap01 (pL + 0.25);
        const float msL = 0.4f + 7.0f * (0.5f - 0.5f * std::cos (kTwoPi * pL));
        const float msR = 0.4f + 7.0f * (0.5f - 0.5f * std::cos (kTwoPi * pR));
        const float dL = line.readL (msL * 0.001f * (float) sampleRate);
        const float dR = line.readR (msR * 0.001f * (float) sampleRate);
        line.push (sanitize (inL + 0.6f * fbL), sanitize (inR + 0.6f * fbR));
        fbL = softClip (dL);
        fbR = softClip (dR);
        outL = 0.65f * (inL + dL);
        outR = 0.65f * (inR + dR);
        advance();
    }

private:
    double sampleRate = 44100.0;
    StereoDelayLine line;
    float fbL = 0, fbR = 0;
};

//==============================================================================
class PhaserFx final : public BeatEffect
{
public:
    void prepare (double sr) override { sampleRate = sr; reset(); }
    void reset() override
    {
        for (auto& s : stL) s = 0;
        for (auto& s : stR) s = 0;
        lastL = lastR = 0;
        counter = 0;
    }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        if ((counter++ & 7) == 0)
        {
            coefL = coefFor (phase (blockCtx.cycleBeats));
            coefR = coefFor (phase (blockCtx.cycleBeats, 0.2));
        }
        const float yL = chain (stL, inL + 0.45f * lastL, coefL);
        const float yR = chain (stR, inR + 0.45f * lastR, coefR);
        lastL = softClip (yL);
        lastR = softClip (yR);
        outL = 0.7f * (inL + yL);
        outR = 0.7f * (inR + yR);
        advance();
    }

private:
    float coefFor (float p) const noexcept
    {
        const float lfo = 0.5f - 0.5f * std::cos (kTwoPi * p);
        const float f = 160.0f * std::pow (2.0f, lfo * 5.0f);
        const float t = std::tan (juce::MathConstants<float>::pi * f / (float) sampleRate);
        return (t - 1.0f) / (t + 1.0f);
    }

    static float chain (std::array<float, 6>& st, float x, float a) noexcept
    {
        for (auto& s : st)
        {
            const float y = a * x + s;
            s = x - a * y;
            x = y;
        }
        return x;
    }

    double sampleRate = 44100.0;
    std::array<float, 6> stL {}, stR {};
    float lastL = 0, lastR = 0, coefL = 0, coefR = 0;
    unsigned counter = 0;
};

//==============================================================================
/** FILTER (sine sweep) and TRIPLET FILTER (decaying pulse on each triplet cycle). */
class FilterFx final : public BeatEffect
{
public:
    explicit FilterFx (bool tripletPulse) : triplet (tripletPulse) {}
    void prepare (double sr) override { svf.prepare (sr); reset(); }
    void reset() override { svf.reset(); counter = 0; }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        if ((counter++ & 15) == 0)
        {
            const float p = phase (blockCtx.cycleBeats);
            float cutoff, q;
            if (triplet)
            {
                const float env = (1.0f - p) * (1.0f - p);
                cutoff = 180.0f * std::pow (2.0f, env * 6.3f);
                q = 3.5f;
            }
            else
            {
                const float lfo = 0.5f - 0.5f * std::cos (kTwoPi * p);
                cutoff = 140.0f * std::pow (2.0f, lfo * 6.8f);
                q = 2.4f;
            }
            svf.setParams (cutoff, q);
        }
        outL = svf.process (0, inL).lp;
        outR = svf.process (1, inR).lp;
        advance();
    }

private:
    bool triplet;
    Svf svf;
    unsigned counter = 0;
};

//==============================================================================
class TransFx final : public BeatEffect
{
public:
    void prepare (double sr) override { smooth.setTime (0.0015f, sr); reset(); }
    void reset() override { smooth.reset (1.0f); }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        const float gate = smooth.process (phase (blockCtx.cycleBeats) < 0.5f ? 1.0f : 0.0f);
        outL = inL * gate;
        outR = inR * gate;
        advance();
    }

private:
    OnePole smooth;
};

//==============================================================================
/** ROLL / TRIPLET ROLL: captures audio at activation and loops the selected length. */
class RollFx final : public BeatEffect
{
public:
    void prepare (double sr) override { line.prepare (sr, kMaxDelaySeconds); reset(); }
    void reset() override { line.reset(); active = false; counter = 0; recorded = 0; }

    void onActivate() noexcept override
    {
        active = true;
        loopStart = line.getWritePos();
        counter = 0;
        recorded = 0;
        maxLen = 0;
    }

    void onDeactivate() noexcept override { active = false; }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        if (! active)
        {
            line.push (inL, inR);
            outL = inL;
            outR = inR;
            advance();
            return;
        }

        int len = (int) clampDelay (blockCtx.timeSamples, line.capacity() / 2);
        const int fade = juce::jlimit (8, 512, len / 4);

        // Keep recording until we have the longest loop plus the crossfade tail
        maxLen = juce::jmax (maxLen, len);
        if (recorded < maxLen + fade && recorded < line.capacity() - 1)
        {
            line.push (inL, inR);
            ++recorded;
        }
        else if (len > recorded - fade)
        {
            len = juce::jmax (1, recorded - fade);
        }

        const int pos = counter % len;
        const int idx = loopStart + pos;
        float l = line.atL (idx), r = line.atR (idx);

        if (counter >= len && pos < fade)
        {
            // Blend the loop start with the audio that followed the loop end
            const float w = (float) pos / (float) fade;
            l = l * w + line.atL (idx + len) * (1.0f - w);
            r = r * w + line.atR (idx + len) * (1.0f - w);
        }

        outL = l;
        outR = r;
        ++counter;
        advance();
    }

private:
    StereoDelayLine line;
    bool active = false;
    int loopStart = 0, counter = 0, recorded = 0, maxLen = 0;
};

//==============================================================================
/** MOBIUS: an endlessly rising (Shepard-style) resonant filter bank locked to the beat. */
class MobiusFx final : public BeatEffect
{
public:
    void prepare (double sr) override
    {
        for (auto& f : bank) f.prepare (sr);
        reset();
    }
    void reset() override { for (auto& f : bank) f.reset(); counter = 0; }

    void process (float inL, float inR, float& outL, float& outR) noexcept override
    {
        const float p = phase (blockCtx.cycleBeats);
        if ((counter++ & 15) == 0)
        {
            for (int k = 0; k < kBands; ++k)
            {
                const float oct = (float) k + p;
                bank[(size_t) k].setParams (70.0f * std::pow (2.0f, oct), 5.0f);
                weights[(size_t) k] = 0.5f - 0.5f * std::cos (kTwoPi * oct / (float) kBands);
            }
        }
        float l = 0, r = 0;
        for (int k = 0; k < kBands; ++k)
        {
            l += bank[(size_t) k].process (0, inL).bp * weights[(size_t) k];
            r += bank[(size_t) k].process (1, inR).bp * weights[(size_t) k];
        }
        outL = 1.3f * l;
        outR = 1.3f * r;
        advance();
    }

private:
    static constexpr int kBands = 7;
    std::array<Svf, kBands> bank;
    std::array<float, kBands> weights {};
    unsigned counter = 0;
};
} // namespace

//==============================================================================
std::unique_ptr<BeatEffect> createBeatEffect (BeatFxType type)
{
    using M = DelayFamily::Mode;
    switch (type)
    {
        case BeatFxType::Delay:         return std::make_unique<DelayFamily> (M::Delay);
        case BeatFxType::Echo:          return std::make_unique<DelayFamily> (M::Echo);
        case BeatFxType::PingPong:      return std::make_unique<DelayFamily> (M::PingPong);
        case BeatFxType::Spiral:        return std::make_unique<DelayFamily> (M::Spiral);
        case BeatFxType::Helix:         return std::make_unique<DelayFamily> (M::Helix);
        case BeatFxType::Reverb:        return std::make_unique<ReverbFx>();
        case BeatFxType::Flanger:       return std::make_unique<FlangerFx>();
        case BeatFxType::Phaser:        return std::make_unique<PhaserFx>();
        case BeatFxType::Filter:        return std::make_unique<FilterFx> (false);
        case BeatFxType::TripletFilter: return std::make_unique<FilterFx> (true);
        case BeatFxType::Trans:         return std::make_unique<TransFx>();
        case BeatFxType::Roll:          return std::make_unique<RollFx>();
        case BeatFxType::TripletRoll:   return std::make_unique<RollFx>();
        case BeatFxType::Mobius:        return std::make_unique<MobiusFx>();
    }
    return nullptr;
}

//==============================================================================
BeatFxEngine::BeatFxEngine()
{
    for (int i = 0; i < kNumBeatFx; ++i)
        effects[(size_t) i] = createBeatEffect ((BeatFxType) i);
}

double BeatFxEngine::effectiveBeats (BeatFxType type, int beatIdx) noexcept
{
    const double b = kBeatValues[juce::jlimit (0, kNumBeats - 1, beatIdx)];
    return isTripletFx (type) ? b * 2.0 / 3.0 : b;
}

void BeatFxEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    for (auto& e : effects) e->prepare (sr);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
    splitLowMid.prepare (spec);
    splitMidHi.prepare (spec);
    lowAllpass.prepare (spec);
    splitLowMid.setCutoffFrequency (300.0f);
    splitMidHi.setCutoffFrequency (3500.0f);
    lowAllpass.setCutoffFrequency (3500.0f);
    lowAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);

    onStep = 1.0f / (float) (0.006 * sr);
    switchStep = 1.0f / (float) (0.010 * sr);
    levelSmooth.setTime (0.02f, sr);
    reset();
}

void BeatFxEngine::reset()
{
    for (auto& e : effects) e->reset();
    splitLowMid.reset();
    splitMidHi.reset();
    lowAllpass.reset();
    onGain = 0.0f;
    switchFade = 1.0f;
    gateOpen = false;
    wasRequested = false;
    pendingStart = -1;
}

void BeatFxEngine::process (juce::AudioBuffer<float>& buffer, const Settings& s, const TempoSync::State& tempo) noexcept
{
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // ---- on/off with optional quantized start
    if (s.on && ! wasRequested)
    {
        const double grid = juce::jmin (1.0, effectiveBeats (s.type, s.beatIdx));
        pendingStart = (s.quantize && tempo.hostPlaying)
                           ? TempoSync::samplesUntilGrid (tempo.beatPos, grid, tempo.beatsPerSample)
                           : 0;
    }
    else if (! s.on)
    {
        pendingStart = -1;
        gateOpen = false;
    }
    wasRequested = s.on;

    const bool wantSwitch = s.type != currentType;

    // ---- block context for the active effect
    BeatBlockCtx ctx;
    ctx.sampleRate = sampleRate;
    ctx.beatPos = tempo.beatPos;
    ctx.beatsPerSample = tempo.beatsPerSample;
    ctx.tape = s.tape;
    if (s.sync)
    {
        ctx.cycleBeats = effectiveBeats (currentType, s.beatIdx);
        ctx.timeSamples = TempoSync::beatsToSamples (ctx.cycleBeats, tempo.bpm, sampleRate);
        ctx.amount = (float) (s.beatIdx + 1) / (float) kNumBeats;
    }
    else
    {
        ctx.timeSamples = s.timeMs * 0.001 * sampleRate;
        ctx.cycleBeats = (s.timeMs * 0.001) / TempoSync::beatsToSeconds (1.0, tempo.bpm);
        ctx.amount = juce::jlimit (0.0f, 1.0f, s.timeMs / 4000.0f);
    }

    auto* fx = effects[(size_t) currentType].get();
    fx->beginBlock (ctx);

    const bool allBands = s.low && s.mid && s.hi;

    for (int i = 0; i < n; ++i)
    {
        if (pendingStart >= 0 && i >= pendingStart)
        {
            pendingStart = -1;
            gateOpen = true;
            fx->onActivate();
        }

        // Effect type change: fade out, swap, fade in
        if (wantSwitch && switchFade <= 0.0f && currentType != s.type)
        {
            fx->onDeactivate();
            currentType = s.type;
            fx = effects[(size_t) currentType].get();
            fx->reset();
            ctx.cycleBeats = s.sync ? effectiveBeats (currentType, s.beatIdx) : ctx.cycleBeats;
            if (s.sync) ctx.timeSamples = TempoSync::beatsToSamples (ctx.cycleBeats, tempo.bpm, sampleRate);
            ctx.beatPos = tempo.beatPos + i * tempo.beatsPerSample;
            fx->beginBlock (ctx);
            if (gateOpen) fx->onActivate();
        }
        switchFade = juce::jlimit (0.0f, 1.0f, switchFade + (currentType != s.type ? -switchStep : switchStep));

        const float prevOn = onGain;
        onGain = juce::jlimit (0.0f, 1.0f, onGain + (gateOpen ? onStep : -onStep));
        if (prevOn > 0.0f && onGain <= 0.0f)
            fx->onDeactivate();

        const float level = levelSmooth.process (s.level);
        const float xL = L[i];
        const float xR = R != nullptr ? R[i] : xL;

        float selL = xL, selR = xR, restL = 0.0f, restR = 0.0f;
        if (! allBands)
        {
            float lowL, highL, lowR, highR, midL, hiL, midR, hiR;
            splitLowMid.processSample (0, xL, lowL, highL);
            splitLowMid.processSample (1, xR, lowR, highR);
            splitMidHi.processSample (0, highL, midL, hiL);
            splitMidHi.processSample (1, highR, midR, hiR);
            lowL = lowAllpass.processSample (0, lowL);
            lowR = lowAllpass.processSample (1, lowR);
            selL = (s.low ? lowL : 0.0f) + (s.mid ? midL : 0.0f) + (s.hi ? hiL : 0.0f);
            selR = (s.low ? lowR : 0.0f) + (s.mid ? midR : 0.0f) + (s.hi ? hiR : 0.0f);
            restL = (s.low ? 0.0f : lowL) + (s.mid ? 0.0f : midL) + (s.hi ? 0.0f : hiL);
            restR = (s.low ? 0.0f : lowR) + (s.mid ? 0.0f : midR) + (s.hi ? 0.0f : hiR);
        }

        float outL, outR, wL, wR;
        if (fx->isAdditive())
        {
            // Send is gated, return keeps ringing -> natural trails when the FX is switched off
            const float send = onGain * switchFade;
            fx->process (selL * send, selR * send, wL, wR);
            outL = xL + wL * level * switchFade;
            outR = xR + wR * level * switchFade;
        }
        else
        {
            fx->process (selL, selR, wL, wR);
            const float fxL = restL + selL * (1.0f - level) + wL * level;
            const float fxR = restR + selR * (1.0f - level) + wR * level;
            const float g = onGain * switchFade;
            outL = (1.0f - g) * xL + g * fxL;
            outR = (1.0f - g) * xR + g * fxR;
        }

        L[i] = sanitize (outL);
        if (R != nullptr) R[i] = sanitize (outR);
    }

    if (pendingStart > 0)
        pendingStart = juce::jmax (0, pendingStart - n);
}
} // namespace rdfx
