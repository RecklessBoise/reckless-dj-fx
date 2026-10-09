#pragma once

#include "DspUtils.h"
#include "TempoSync.h"
#include "params/Parameters.h"

namespace rdfx
{
/** Sound Color FX section: one bipolar COLOR knob (center = dry) plus a PARAMETER knob. */
class ColorFxEngine
{
public:
    struct Settings
    {
        bool on = false;
        ColorFxType type = ColorFxType::Filter;
        float amount = 0.0f; // -1..1
        float param = 0.5f;  // 0..1
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Settings& s, const TempoSync::State& tempo) noexcept;

private:
    double sampleRate = 44100.0;
    ColorFxType currentType = ColorFxType::Filter;
    float onGain = 0.0f, onStep = 0.0f, switchFade = 1.0f, switchStep = 0.0f;
    OnePole amountSmooth, paramSmooth;

    Svf filter, noiseFilter, wetFilter;
    juce::Reverb space;
    StereoDelayLine dub;
    OnePole dubDampL, dubDampR, gateSmooth;
    juce::Random rng;
    float crushHold[2] {}, crushCounter[2] {};
    unsigned counter = 0;
    float dubDelaySamples = 1000.0f;
};
} // namespace rdfx
