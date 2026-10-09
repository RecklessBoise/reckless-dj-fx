#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "params/Parameters.h"

namespace rdfx
{
/** Resolves the working tempo and the musical position for each audio block. */
class TempoSync
{
public:
    struct State
    {
        double bpm = 128.0;
        double beatPos = 0.0;        // quarter-note position at block start
        bool hostPlaying = false;    // true when beatPos comes from the host transport
        double beatsPerSample = 0.0;
        BpmMode source = BpmMode::Host; // where the tempo actually comes from (Host = the DAW)
    };

    void prepare (double sampleRate) noexcept;

    /** Call once per block. `playHead` may be null. */
    const State& update (juce::AudioPlayHead* playHead, BpmMode mode, double paramBpm, int numSamples) noexcept;

    const State& getState() const noexcept { return state; }

    static double beatsToSeconds (double beats, double bpm) noexcept { return beats * 60.0 / juce::jmax (1.0, bpm); }
    static double beatsToSamples (double beats, double bpm, double sr) noexcept { return beatsToSeconds (beats, bpm) * sr; }

    /** Samples to wait from block start until the next multiple of `gridBeats` (0 if on the grid). */
    static int samplesUntilGrid (double beatPos, double gridBeats, double beatsPerSample) noexcept;

    /** Tap-tempo helper: returns averaged BPM once >= 2 taps fall within 2 s of each other, else 0. */
    class Tapper
    {
    public:
        double tap (double nowMs) noexcept;
    private:
        double taps[8] {};
        int count = 0;
    };

private:
    double sampleRate = 44100.0;
    double internalBeatPos = 0.0;
    State state;
};
} // namespace rdfx
