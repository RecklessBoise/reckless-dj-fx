#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace rdfx
{
namespace ParamID
{
    // Beat FX
    inline constexpr const char* beatOn    = "beatOn";
    inline constexpr const char* beatType  = "beatType";
    inline constexpr const char* beatIdx   = "beatIdx";
    inline constexpr const char* beatSync  = "beatSync";
    inline constexpr const char* timeMs    = "timeMs";
    inline constexpr const char* level     = "level";
    inline constexpr const char* fxLow     = "fxLow";
    inline constexpr const char* fxMid     = "fxMid";
    inline constexpr const char* fxHi      = "fxHi";
    inline constexpr const char* quantize  = "quantize";
    inline constexpr const char* tape      = "tape";
    // Sound Color FX
    inline constexpr const char* colorOn    = "colorOn";
    inline constexpr const char* colorType  = "colorType";
    inline constexpr const char* colorAmt   = "colorAmt";
    inline constexpr const char* colorParam = "colorParam";
    inline constexpr const char* centerLock = "centerLock";
    // Tempo / global
    inline constexpr const char* bpmMode = "bpmMode";
    inline constexpr const char* bpm     = "bpm";
    inline constexpr const char* outGain = "outGain";
}

enum class BeatFxType
{
    Delay, Echo, PingPong, Spiral, Helix, Reverb, Flanger, Phaser,
    Filter, TripletFilter, Trans, Roll, TripletRoll, Mobius
};
inline constexpr int kNumBeatFx = 14;

enum class ColorFxType { Space, DubEcho, Sweep, Noise, Crush, Filter };
inline constexpr int kNumColorFx = 6;

enum class BpmMode { Host, Tap, Manual };

const juce::StringArray& beatFxNames();
const juce::StringArray& colorFxNames();

/** Beat values selectable with the BEAT ◄► buttons / X-Pad. */
inline constexpr int kNumBeats = 10;
inline constexpr double kBeatValues[kNumBeats] = { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 2, 3.0 / 4, 1.0, 2.0, 4.0, 8.0, 16.0 };
inline constexpr int kDefaultBeatIdx = 5;
const juce::StringArray& beatLabels();

bool isTripletFx (BeatFxType t) noexcept;
bool isReverbFx (BeatFxType t) noexcept;

/** Text shown in the beat display for a given effect + beat index (e.g. "1/2", "3/8T", "40%"). */
juce::String beatDisplayText (BeatFxType t, int beatIdx);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace rdfx
