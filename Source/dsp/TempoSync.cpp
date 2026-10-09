#include "TempoSync.h"

namespace rdfx
{
void TempoSync::prepare (double sr) noexcept
{
    sampleRate = sr;
    internalBeatPos = 0.0;
}

const TempoSync::State& TempoSync::update (juce::AudioPlayHead* playHead, BpmMode mode, double paramBpm, int numSamples) noexcept
{
    double hostBpm = 0.0;
    std::optional<double> hostPpq;
    bool playing = false;

    if (playHead != nullptr)
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            playing = pos->getIsPlaying();
            if (playing)
                if (auto ppq = pos->getPpqPosition())
                    hostPpq = *ppq;
        }
    }

    // Inside a DAW the effects always follow the DAW tempo and transport. TAP / MANUAL only apply
    // when the host gives no tempo (standalone app).
    const bool hostTempo = hostBpm > 1.0;
    double bpm = hostTempo ? hostBpm : paramBpm;
    bpm = juce::jlimit (20.0, 400.0, bpm);
    state.source = hostTempo ? BpmMode::Host : (mode == BpmMode::Host ? BpmMode::Manual : mode);

    state.bpm = bpm;
    state.beatsPerSample = bpm / (60.0 * sampleRate);

    if (hostPpq.has_value())
    {
        state.beatPos = *hostPpq;
        state.hostPlaying = true;
        internalBeatPos = *hostPpq;
    }
    else
    {
        state.beatPos = internalBeatPos;
        state.hostPlaying = false;
    }

    internalBeatPos += state.beatsPerSample * numSamples;
    return state;
}

int TempoSync::samplesUntilGrid (double beatPos, double gridBeats, double beatsPerSample) noexcept
{
    if (gridBeats <= 0.0 || beatsPerSample <= 0.0)
        return 0;
    const double cycles = beatPos / gridBeats;
    const double frac = cycles - std::floor (cycles);
    if (frac < 1.0e-6 || frac > 1.0 - 1.0e-6)
        return 0;
    return (int) std::ceil ((1.0 - frac) * gridBeats / beatsPerSample);
}

double TempoSync::Tapper::tap (double nowMs) noexcept
{
    if (count > 0 && nowMs - taps[(count - 1) % 8] > 2000.0)
        count = 0;

    taps[count % 8] = nowMs;
    ++count;

    const int n = juce::jmin (count, 8);
    if (n < 2)
        return 0.0;

    const double first = taps[(count - n) % 8];
    const double last = taps[(count - 1) % 8];
    const double interval = (last - first) / (n - 1);
    return interval > 0.0 ? juce::jlimit (40.0, 250.0, 60000.0 / interval) : 0.0;
}
} // namespace rdfx
