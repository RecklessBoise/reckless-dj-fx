#include "FactoryPresets.h"
#include "params/Parameters.h"

namespace rdfx
{
namespace
{
using BT = BeatFxType;
using CT = ColorFxType;

/** Fluent helper to describe one preset. */
struct P
{
    FactoryPreset p;

    P (juce::String name, juce::String category) { p.name = std::move (name); p.category = std::move (category); }

    P& set (const char* id, float v) { p.values.emplace_back (id, v); return *this; }

    P& beat (BT t, int idx, float level = 0.5f)
    {
        return set (ParamID::beatOn, 1).set (ParamID::beatType, (float) t).set (ParamID::beatIdx, (float) idx)
              .set (ParamID::beatSync, 1).set (ParamID::level, level);
    }
    P& freeTime (float ms) { return set (ParamID::beatSync, 0).set (ParamID::timeMs, ms); }
    P& bands (bool lo, bool mid, bool hi) { return set (ParamID::fxLow, lo).set (ParamID::fxMid, mid).set (ParamID::fxHi, hi); }
    P& tape() { return set (ParamID::tape, 1); }
    P& color (CT t, float amount, float param = 0.5f)
    {
        return set (ParamID::colorOn, 1).set (ParamID::colorType, (float) t).set (ParamID::colorAmt, amount)
              .set (ParamID::colorParam, param);
    }
    P& out (float db) { return set (ParamID::outGain, db); }
};

juce::String fxName (BT t)
{
    // "PING PONG" -> "Ping Pong"
    auto words = juce::StringArray::fromTokens (beatFxNames()[(int) t], " ", "");
    for (auto& w : words) w = w.substring (0, 1) + w.substring (1).toLowerCase();
    return words.joinIntoString (" ");
}

juce::String colorName (CT t)
{
    auto words = juce::StringArray::fromTokens (colorFxNames()[(int) t], " ", "");
    for (auto& w : words) w = w.substring (0, 1) + w.substring (1).toLowerCase();
    return words.joinIntoString (" ");
}

juce::String categoryFor (BT t)
{
    switch (t)
    {
        case BT::Delay: case BT::Echo: case BT::PingPong:   return "Delay & Echo";
        case BT::Spiral: case BT::Helix: case BT::Reverb:   return "Space & Reverb";
        case BT::Flanger: case BT::Phaser: case BT::Mobius: return "Modulation";
        case BT::Filter: case BT::TripletFilter:            return "Filter";
        case BT::Trans:                                     return "Rhythmic";
        case BT::Roll: case BT::TripletRoll:                return "Roll";
    }
    return "Beat FX";
}

/** Beat indices that make musical sense per effect: 0=1/16 1=1/8 2=1/4 3=1/2 4=3/4 5=1 6=2 7=4 8=8 9=16 */
std::vector<int> beatChoices (BT t)
{
    switch (t)
    {
        case BT::Delay: case BT::Echo: case BT::PingPong: return { 1, 2, 3, 4, 5, 6 };
        case BT::Spiral: case BT::Helix:                  return { 2, 3, 4, 5, 6 };
        case BT::Reverb:                                  return { 1, 3, 5, 7, 9 };
        case BT::Flanger: case BT::Phaser:                return { 5, 6, 7, 8, 9 };
        case BT::Filter: case BT::TripletFilter:          return { 3, 5, 6, 7, 8 };
        case BT::Trans:                                   return { 0, 1, 2, 3, 5 };
        case BT::Roll: case BT::TripletRoll:              return { 0, 1, 2, 3, 4, 5 };
        case BT::Mobius:                                  return { 5, 6, 7, 8 };
    }
    return { kDefaultBeatIdx };
}

float defaultLevel (BT t)
{
    switch (t)
    {
        case BT::Delay: case BT::Echo: case BT::PingPong: return 0.55f;
        case BT::Spiral: case BT::Helix:                  return 0.6f;
        case BT::Reverb:                                  return 0.6f;
        case BT::Roll: case BT::TripletRoll:              return 1.0f;
        case BT::Trans:                                   return 0.9f;
        case BT::Flanger: case BT::Phaser: case BT::Filter:
        case BT::TripletFilter: case BT::Mobius:          return 0.75f;
    }
    return 0.75f;
}

std::vector<FactoryPreset> build()
{
    std::vector<P> list;
    auto add = [&] (P p) { list.push_back (std::move (p)); };

    // 0. Init
    add (P ("Init", "Init"));

    // 1. Every Beat FX at every musical beat value
    for (int i = 0; i < kNumBeatFx; ++i)
    {
        const auto t = (BT) i;
        for (int idx : beatChoices (t))
            add (P (fxName (t) + " " + beatDisplayText (t, idx), categoryFor (t)).beat (t, idx, defaultLevel (t)));
    }

    // 2. FX FREQUENCY variants (band-limited effects)
    struct Band { const char* label; bool lo, mid, hi; };
    const Band bandSets[] = { { "Hi Only", false, false, true }, { "No Low", false, true, true }, { "Low Only", true, false, false } };
    for (int i = 0; i < kNumBeatFx; ++i)
    {
        const auto t = (BT) i;
        const int idx = (t == BT::Reverb) ? 6 : (t == BT::Roll || t == BT::TripletRoll || t == BT::Trans) ? 2 : 3;
        for (auto& b : bandSets)
            add (P (fxName (t) + " " + beatDisplayText (t, idx) + " " + b.label, categoryFor (t))
                     .beat (t, idx, defaultLevel (t)).bands (b.lo, b.mid, b.hi));
    }

    // 3. Heavy (max depth) variants
    for (int i = 0; i < kNumBeatFx; ++i)
    {
        const auto t = (BT) i;
        add (P (fxName (t) + " Deep", categoryFor (t)).beat (t, beatChoices (t)[2], 0.95f).out (-1.5f));
        add (P (fxName (t) + " Subtle", categoryFor (t)).beat (t, beatChoices (t)[1], 0.25f));
    }

    // 4. X-Pad tape mode echoes
    for (auto t : { BT::Delay, BT::Echo, BT::PingPong })
        for (int idx : { 2, 3, 4, 5 })
            add (P (fxName (t) + " Tape " + beatDisplayText (t, idx), "Tape").beat (t, idx, 0.6f).tape());

    // 5. Free time (unsynced TIME knob)
    struct Free { const char* name; BT t; float ms; float lvl; };
    const Free frees[] = {
        { "Slapback 85ms", BT::Delay, 85, 0.5f },      { "Slapback 120ms", BT::Delay, 120, 0.5f },
        { "Doubler 30ms", BT::Delay, 30, 0.45f },      { "Rockabilly Echo", BT::Echo, 140, 0.55f },
        { "Canyon 900ms", BT::Echo, 900, 0.6f },       { "Space Bounce 333ms", BT::PingPong, 333, 0.6f },
        { "Jet Flanger 8s", BT::Flanger, 8000, 0.8f }, { "Slow Phase 6s", BT::Phaser, 6000, 0.8f },
        { "Wobble Filter 250ms", BT::Filter, 250, 0.85f }, { "Stutter Gate 60ms", BT::Trans, 60, 0.9f },
        { "Machine Gun 40ms", BT::Roll, 40, 1.0f },    { "Hall 3200", BT::Reverb, 3200, 0.6f },
        { "Room 800", BT::Reverb, 800, 0.5f },         { "Helix Drone 2s", BT::Helix, 2000, 0.65f },
        { "Spiral Time Warp", BT::Spiral, 700, 0.6f }, { "Mobius 3s Climb", BT::Mobius, 3000, 0.8f },
    };
    for (auto& f : frees)
        add (P (f.name, "Free Time").beat (f.t, kDefaultBeatIdx, f.lvl).freeTime (f.ms));

    // 6. Sound Color FX positions
    struct Pos { const char* label; float amt; };
    const Pos positions[] = { { "Left Light", -0.35f }, { "Left Deep", -0.85f }, { "Right Light", 0.35f }, { "Right Deep", 0.85f } };
    for (int c = 0; c < kNumColorFx; ++c)
    {
        const auto ct = (CT) c;
        for (auto& pos : positions)
        {
            add (P (colorName (ct) + " " + pos.label, "Color FX").color (ct, pos.amt, 0.3f));
            add (P (colorName (ct) + " " + pos.label + " +", "Color FX").color (ct, pos.amt, 0.85f));
        }
    }

    // 7. Combos: one Color FX feeding one Beat FX
    struct Combo { const char* name; CT c; float amt; float prm; BT b; int idx; float lvl; };
    const Combo combos[] = {
        { "Dub Station", CT::DubEcho, 0.45f, 0.7f, BT::Echo, 4, 0.5f },
        { "Underwater Echo", CT::Filter, -0.55f, 0.4f, BT::Echo, 3, 0.6f },
        { "Airy Ping Pong", CT::Filter, 0.4f, 0.3f, BT::PingPong, 2, 0.6f },
        { "Crushed Roll", CT::Crush, 0.6f, 0.6f, BT::Roll, 1, 1.0f },
        { "Lo-Fi Spiral", CT::Crush, -0.45f, 0.4f, BT::Spiral, 4, 0.6f },
        { "Cathedral Wash", CT::Space, 0.5f, 0.9f, BT::Reverb, 9, 0.7f },
        { "Noise Riser", CT::Noise, 0.7f, 0.8f, BT::Filter, 6, 0.8f },
        { "White Out", CT::Noise, 0.95f, 1.0f, BT::Reverb, 8, 0.8f },
        { "Sweep Phaser", CT::Sweep, 0.5f, 0.6f, BT::Phaser, 7, 0.8f },
        { "Gated Echo", CT::Sweep, -0.7f, 0.5f, BT::Echo, 3, 0.5f },
        { "Helix Tunnel", CT::Filter, -0.4f, 0.6f, BT::Helix, 3, 0.65f },
        { "Mobius Space", CT::Space, 0.4f, 0.7f, BT::Mobius, 7, 0.7f },
        { "Telephone Delay", CT::Filter, 0.6f, 0.7f, BT::Delay, 2, 0.6f },
        { "Dusty Trans", CT::Crush, -0.3f, 0.5f, BT::Trans, 1, 0.9f },
        { "Flange Crush", CT::Crush, 0.4f, 0.5f, BT::Flanger, 7, 0.8f },
        { "Dub Spiral", CT::DubEcho, -0.5f, 0.8f, BT::Spiral, 3, 0.6f },
        { "Space Ping", CT::Space, -0.5f, 0.6f, BT::PingPong, 3, 0.5f },
        { "Triplet Wah", CT::Sweep, 0.35f, 0.8f, BT::TripletFilter, 5, 0.85f },
        { "Bit Roll Triplets", CT::Crush, -0.6f, 0.7f, BT::TripletRoll, 2, 1.0f },
        { "Dark Hall", CT::Filter, -0.6f, 0.3f, BT::Reverb, 7, 0.7f },
        { "Bright Hall", CT::Filter, 0.5f, 0.3f, BT::Reverb, 7, 0.7f },
        { "Noisy Helix", CT::Noise, -0.5f, 0.5f, BT::Helix, 4, 0.6f },
        { "Dub Roll", CT::DubEcho, 0.6f, 0.6f, BT::Roll, 3, 1.0f },
        { "Space Trans", CT::Space, 0.6f, 0.5f, BT::Trans, 2, 0.85f },
        { "Filtered Mobius", CT::Filter, 0.45f, 0.8f, BT::Mobius, 6, 0.75f },
        { "Crunch Phaser", CT::Crush, 0.5f, 0.3f, BT::Phaser, 6, 0.8f },
        { "Sweep Spiral", CT::Sweep, 0.6f, 0.5f, BT::Spiral, 5, 0.6f },
        { "Gate Roll", CT::Sweep, -0.5f, 0.7f, BT::Roll, 2, 1.0f },
        { "Dub Delay Low Cut", CT::DubEcho, 0.7f, 0.5f, BT::Delay, 4, 0.5f },
        { "Hi-Pass Reverb Out", CT::Filter, 0.75f, 0.4f, BT::Reverb, 8, 0.75f },
    };
    for (auto& c : combos)
        add (P (c.name, "Combos").color (c.c, c.amt, c.prm).beat (c.b, c.idx, c.lvl));

    // 8. Performance presets (named for typical DJ moments)
    add (P ("Build-Up Riser", "Performance").color (CT::Filter, 0.55f, 0.75f).beat (BT::Echo, 3, 0.55f).bands (false, true, true));
    add (P ("Drop Washout", "Performance").beat (BT::Reverb, 9, 0.9f).color (CT::Filter, 0.6f, 0.4f));
    add (P ("Outro Echo Out", "Performance").beat (BT::Echo, 5, 0.8f));
    add (P ("Intro Low Cut", "Performance").color (CT::Filter, 0.45f, 0.4f));
    add (P ("Breakdown Dub", "Performance").color (CT::DubEcho, 0.55f, 0.75f).beat (BT::Spiral, 3, 0.45f));
    add (P ("Snare Roll 1/8", "Performance").beat (BT::Roll, 1, 1.0f).bands (false, true, true));
    add (P ("Snare Roll 1/16", "Performance").beat (BT::Roll, 0, 1.0f).bands (false, true, true));
    add (P ("Vocal Echo Throw", "Performance").beat (BT::PingPong, 4, 0.7f).bands (false, true, false));
    add (P ("Hat Flanger", "Performance").beat (BT::Flanger, 6, 0.8f).bands (false, false, true));
    add (P ("Kick Stutter", "Performance").beat (BT::Trans, 1, 0.9f).bands (true, false, false));
    add (P ("Bass Swap Prep", "Performance").color (CT::Filter, 0.3f, 0.3f).beat (BT::Filter, 7, 0.6f).bands (true, false, false));
    add (P ("Spiral Into Space", "Performance").beat (BT::Spiral, 5, 0.75f).color (CT::Space, 0.4f, 0.8f));
    add (P ("Helix Freeze", "Performance").beat (BT::Helix, 2, 0.8f));
    add (P ("Mobius Lift", "Performance").beat (BT::Mobius, 6, 0.85f).color (CT::Noise, 0.4f, 0.4f));
    add (P ("Triplet Shuffle", "Performance").beat (BT::TripletRoll, 2, 1.0f).bands (false, true, true));
    add (P ("Triplet Filter Groove", "Performance").beat (BT::TripletFilter, 6, 0.8f));
    add (P ("Echo Tape Spin", "Performance").beat (BT::Echo, 3, 0.65f).tape().color (CT::Crush, -0.25f, 0.4f));
    add (P ("Trans 1/16 Chop", "Performance").beat (BT::Trans, 0, 1.0f));
    add (P ("Ping Pong Hi Sparkle", "Performance").beat (BT::PingPong, 2, 0.6f).bands (false, false, true));
    add (P ("Reverb Tail Out", "Performance").beat (BT::Reverb, 9, 1.0f).out (-2.0f));
    add (P ("Noise Sweep Up", "Performance").color (CT::Noise, 0.8f, 0.7f));
    add (P ("Noise Sweep Down", "Performance").color (CT::Noise, -0.8f, 0.7f));
    add (P ("Lo-Fi Radio", "Performance").color (CT::Crush, 0.55f, 0.6f));
    add (P ("8-Bit Breakdown", "Performance").color (CT::Crush, -0.9f, 1.0f).beat (BT::Trans, 2, 0.6f));
    add (P ("Space Odyssey", "Performance").color (CT::Space, 0.85f, 1.0f).beat (BT::Mobius, 7, 0.6f));
    add (P ("Club Phaser", "Performance").beat (BT::Phaser, 7, 0.85f).bands (false, true, true));
    add (P ("Dub Siren Echo", "Performance").color (CT::DubEcho, -0.8f, 0.95f));
    add (P ("Gate Pump", "Performance").color (CT::Sweep, -0.6f, 0.6f));
    add (P ("Band Sweep", "Performance").color (CT::Sweep, 0.7f, 0.7f));
    add (P ("Echo Out Low Kill", "Performance").beat (BT::Echo, 5, 0.8f).bands (false, true, true).color (CT::Filter, 0.4f, 0.4f));

    std::vector<FactoryPreset> result;
    result.reserve (list.size());
    for (auto& p : list)
        result.push_back (std::move (p.p));
    return result;
}
} // namespace

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = build();
    return presets;
}
} // namespace rdfx
