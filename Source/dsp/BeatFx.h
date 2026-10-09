#pragma once

#include <array>
#include <memory>
#include "DspUtils.h"
#include "TempoSync.h"
#include "params/Parameters.h"

namespace rdfx
{
/** Per-block context handed to every Beat FX. */
struct BeatBlockCtx
{
    double sampleRate = 44100.0;
    double beatPos = 0.0;         // quarter notes at block start
    double beatsPerSample = 0.0;
    double cycleBeats = 1.0;      // selected beat value (triplet-adjusted)
    double timeSamples = 22050.0; // selected time in samples (clamped by each effect)
    float amount = 0.5f;          // 0..1 normalised beat/time (used by REVERB)
    float level = 0.5f;           // LEVEL/DEPTH (some effects use it as depth, e.g. SPIRAL feedback)
    bool tape = false;
};

/** Base class for one Beat FX algorithm. process() is called once per sample. */
class BeatEffect
{
public:
    virtual ~BeatEffect() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void beginBlock (const BeatBlockCtx& ctx) { blockCtx = ctx; sampleInBlock = 0; }
    virtual void process (float inL, float inR, float& outL, float& outR) noexcept = 0;
    /** Additive effects (delays/reverbs) add a wet signal on top of the dry and keep trails. */
    virtual bool isAdditive() const noexcept { return false; }
    /** Additive effects that replace the original sound as LEVEL/DEPTH goes up (HELIX at 100 % = effect only). */
    virtual bool ducksDry() const noexcept { return false; }
    virtual void onActivate() noexcept {}
    virtual void onDeactivate() noexcept {}

protected:
    /** Musical phase (0..1) of the current sample within a cycle of `cycleBeats`. */
    float phase (double cycleBeats, double offset = 0.0) const noexcept
    {
        return wrap01 ((blockCtx.beatPos + sampleInBlock * blockCtx.beatsPerSample) / juce::jmax (1.0e-4, cycleBeats) + offset);
    }
    void advance() noexcept { ++sampleInBlock; }

    BeatBlockCtx blockCtx;
    int sampleInBlock = 0;
};

std::unique_ptr<BeatEffect> createBeatEffect (BeatFxType type);

/** The complete Beat FX section: band split (FX FREQUENCY), effect, level/depth, on/off, quantize. */
class BeatFxEngine
{
public:
    struct Settings
    {
        bool on = false;
        BeatFxType type = BeatFxType::Delay;
        int beatIdx = kDefaultBeatIdx;
        bool sync = true;
        float timeMs = 500.0f;
        float level = 0.5f;
        bool low = true, mid = true, hi = true;
        bool quantize = true;
        bool tape = false;
    };

    BeatFxEngine();
    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& s, const TempoSync::State& tempo) noexcept;

    /** True while the effect is audible (on, or trails still ringing). Used by the UI LED. */
    bool isEngaged() const noexcept { return onGain > 0.001f; }

    static double effectiveBeats (BeatFxType type, int beatIdx) noexcept;

private:
    BeatFxType currentType = BeatFxType::Delay;
    std::array<std::unique_ptr<BeatEffect>, kNumBeatFx> effects;
    double sampleRate = 44100.0;

    juce::dsp::LinkwitzRileyFilter<float> splitLowMid, splitMidHi, lowAllpass;

    float onGain = 0.0f, onStep = 0.0f;
    float switchFade = 1.0f, switchStep = 0.0f;
    bool gateOpen = false;      // the effect is "on" after quantize
    bool wasRequested = false;
    int pendingStart = -1;      // samples until a quantized start, -1 = none
    OnePole levelSmooth;
};
} // namespace rdfx
