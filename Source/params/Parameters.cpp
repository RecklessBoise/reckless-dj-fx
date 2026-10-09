#include "Parameters.h"

namespace rdfx
{
const juce::StringArray& beatFxNames()
{
    static const juce::StringArray names { "DELAY", "ECHO", "PING PONG", "SPIRAL", "HELIX", "REVERB", "FLANGER",
                                           "PHASER", "FILTER", "TRIPLET FILTER", "TRANS", "ROLL", "TRIPLET ROLL", "MOBIUS" };
    return names;
}

const juce::StringArray& colorFxNames()
{
    static const juce::StringArray names { "SPACE", "DUB ECHO", "SWEEP", "NOISE", "CRUSH", "FILTER" };
    return names;
}

const juce::StringArray& beatLabels()
{
    static const juce::StringArray labels { "1/16", "1/8", "1/4", "1/2", "3/4", "1", "2", "4", "8", "16" };
    return labels;
}

bool isTripletFx (BeatFxType t) noexcept
{
    return t == BeatFxType::TripletFilter || t == BeatFxType::TripletRoll;
}

bool isReverbFx (BeatFxType t) noexcept { return t == BeatFxType::Reverb; }

juce::String beatDisplayText (BeatFxType t, int beatIdx)
{
    beatIdx = juce::jlimit (0, kNumBeats - 1, beatIdx);
    if (isReverbFx (t))
        return juce::String ((beatIdx + 1) * 10) + "%";

    if (isTripletFx (t))
    {
        // Triplet values: 2/3 of the straight value, shown as e.g. "1/6", "1/3", "2/3"
        static const juce::StringArray triplet { "1/24", "1/12", "1/6", "1/3", "1/2", "2/3", "4/3", "8/3", "16/3", "32/3" };
        return triplet[beatIdx];
    }
    return beatLabels()[beatIdx];
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percent = AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })
                             .withValueFromStringFunction ([] (const String& t) { return t.retainCharacters ("0123456789.-").getFloatValue() / 100.0f; });
    const auto bipolar = AudioParameterFloatAttributes()
                             .withStringFromValueFunction ([] (float v, int)
                             {
                                 const int pct = roundToInt (std::abs (v) * 100.0f);
                                 return pct == 0 ? String ("CENTER") : (v < 0 ? "L " : "R ") + String (pct) + "%";
                             })
                             .withValueFromStringFunction ([] (const String& t)
                             {
                                 const float v = t.retainCharacters ("0123456789.").getFloatValue() / 100.0f;
                                 return t.trimStart().startsWithIgnoreCase ("L") || t.trimStart().startsWithChar ('-') ? -v : v;
                             });

    auto beatGroup = std::make_unique<AudioProcessorParameterGroup> ("beat", "Beat FX", "|");
    beatGroup->addChild (
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::beatOn, 1 }, "Beat FX On", false),
        std::make_unique<AudioParameterChoice> (ParameterID { ParamID::beatType, 1 }, "Beat FX Type", beatFxNames(), 0),
        std::make_unique<AudioParameterChoice> (ParameterID { ParamID::beatIdx, 1 }, "Beat", beatLabels(), kDefaultBeatIdx),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::beatSync, 1 }, "Beat Sync", true),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::timeMs, 1 }, "Time",
                                               NormalisableRange<float> (1.0f, 4000.0f, 0.1f, 0.35f), 500.0f,
                                               AudioParameterFloatAttributes()
                                                   .withLabel ("ms")
                                                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; })
                                                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::level, 1 }, "Level/Depth",
                                               NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::fxLow, 1 }, "FX Freq Low", true),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::fxMid, 1 }, "FX Freq Mid", true),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::fxHi, 1 }, "FX Freq Hi", true),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::quantize, 1 }, "Quantize", true),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::tape, 1 }, "X-Pad Tape Mode", false));

    auto colorGroup = std::make_unique<AudioProcessorParameterGroup> ("color", "Sound Color FX", "|");
    colorGroup->addChild (
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::colorOn, 1 }, "Color FX On", false),
        std::make_unique<AudioParameterChoice> (ParameterID { ParamID::colorType, 1 }, "Color FX Type", colorFxNames(), 5),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::colorAmt, 1 }, "Color",
                                               NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolar),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::colorParam, 1 }, "Color Parameter",
                                               NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent),
        std::make_unique<AudioParameterBool> (ParameterID { ParamID::centerLock, 1 }, "Center Lock", true));

    auto globalGroup = std::make_unique<AudioProcessorParameterGroup> ("global", "Global", "|");
    globalGroup->addChild (
        std::make_unique<AudioParameterChoice> (ParameterID { ParamID::bpmMode, 1 }, "BPM Source",
                                                StringArray { "HOST", "TAP", "MANUAL" }, 0),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::bpm, 1 }, "BPM",
                                               NormalisableRange<float> (40.0f, 250.0f, 0.1f), 128.0f,
                                               AudioParameterFloatAttributes()
                                                   .withStringFromValueFunction ([] (float v, int) { return String (v, 1); })),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamID::outGain, 1 }, "Output",
                                               NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                                               AudioParameterFloatAttributes()
                                                   .withLabel ("dB")
                                                   .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
                                                   .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); })));

    layout.add (std::move (beatGroup), std::move (colorGroup), std::move (globalGroup));
    return layout;
}
} // namespace rdfx
