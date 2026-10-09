#include "PluginProcessor.h"

using namespace rdfx;

namespace
{
void fillNoise (juce::AudioBuffer<float>& b, juce::Random& rng, float gain = 0.5f)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * gain);
}

struct Stats { bool finite = true; float peak = 0.0f; double energy = 0.0; };

Stats stats (const juce::AudioBuffer<float>& b)
{
    Stats s;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float x = b.getSample (ch, i);
            if (! std::isfinite (x)) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (x));
            s.energy += (double) x * x;
        }
    return s;
}
} // namespace

class TempoTests final : public juce::UnitTest
{
public:
    TempoTests() : juce::UnitTest ("Tempo sync", "RecklessDJFX") {}

    void runTest() override
    {
        beginTest ("beat to time conversion");
        expectWithinAbsoluteError (TempoSync::beatsToSeconds (1.0, 120.0), 0.5, 1e-9);
        expectWithinAbsoluteError (TempoSync::beatsToSamples (0.5, 128.0, 48000.0), 11250.0, 1e-6);

        beginTest ("triplet values are 2/3 of straight values");
        expectWithinAbsoluteError (BeatFxEngine::effectiveBeats (BeatFxType::TripletRoll, 5), 2.0 / 3.0, 1e-9);
        expectWithinAbsoluteError (BeatFxEngine::effectiveBeats (BeatFxType::Roll, 5), 1.0, 1e-9);

        beginTest ("quantize grid");
        const double bps = 120.0 / (60.0 * 48000.0);
        expectEquals (TempoSync::samplesUntilGrid (4.0, 1.0, bps), 0);
        expectEquals (TempoSync::samplesUntilGrid (3.5, 1.0, bps), 12000);

        beginTest ("tap tempo");
        TempoSync::Tapper tapper;
        expectEquals (tapper.tap (0.0), 0.0);
        tapper.tap (500.0);
        tapper.tap (1000.0);
        expectWithinAbsoluteError (tapper.tap (1500.0), 120.0, 0.01);

        beginTest ("beat display text");
        expectEquals (beatDisplayText (BeatFxType::Reverb, 3), juce::String ("40%"));
        expectEquals (beatDisplayText (BeatFxType::Echo, 3), juce::String ("1/2"));
        expectEquals (beatDisplayText (BeatFxType::TripletFilter, 5), juce::String ("2/3"));
    }
};

class EffectTests final : public juce::UnitTest
{
public:
    EffectTests() : juce::UnitTest ("Beat FX + Color FX", "RecklessDJFX") {}

    void runTest() override
    {
        const double sr = 48000.0;
        const int block = 256;
        TempoSync::State tempo;
        tempo.bpm = 128.0;
        tempo.beatsPerSample = tempo.bpm / (60.0 * sr);
        juce::Random rng (42);

        for (int t = 0; t < kNumBeatFx; ++t)
        {
            beginTest ("Beat FX " + beatFxNames()[t] + " is stable and audible");
            for (int beatIdx : { 0, 5, 9 })
                for (bool partial : { false, true })
                {
                    BeatFxEngine engine;
                    engine.prepare (sr, block);
                    BeatFxEngine::Settings s;
                    s.on = true;
                    s.type = (BeatFxType) t;
                    s.beatIdx = beatIdx;
                    s.level = 1.0f;
                    s.quantize = false;
                    s.low = ! partial;

                    juce::AudioBuffer<float> buf (2, block);
                    Stats total;
                    double diffEnergy = 0.0;
                    tempo.beatPos = 0.0;
                    for (int b = 0; b < 400; ++b) // ~2 s
                    {
                        fillNoise (buf, rng);
                        juce::AudioBuffer<float> dry (buf);
                        engine.process (buf, s, tempo);
                        tempo.beatPos += block * tempo.beatsPerSample;
                        const auto st = stats (buf);
                        total.finite &= st.finite;
                        total.peak = juce::jmax (total.peak, st.peak);
                        for (int ch = 0; ch < 2; ++ch)
                            for (int i = 0; i < block; ++i)
                                diffEnergy += std::pow (buf.getSample (ch, i) - dry.getSample (ch, i), 2.0);
                    }
                    expect (total.finite, "non-finite output");
                    expect (total.peak < 8.0f, "output peak too high: " + juce::String (total.peak));
                    if (beatIdx <= 5) // long values (16 beats) only start to loop/gate after the test window
                        expect (diffEnergy > 1.0, "effect has no audible effect");

                    // Switching off: trails must decay towards silence without blowing up
                    s.on = false;
                    float tailStart = 0.0f, tailEnd = 0.0f;
                    bool tailFinite = true;
                    for (int b = 0; b < 4000; ++b)
                    {
                        buf.clear();
                        engine.process (buf, s, tempo);
                        const auto st = stats (buf);
                        tailFinite &= st.finite;
                        if (b < 50) tailStart = juce::jmax (tailStart, st.peak);
                        if (b >= 3950) tailEnd = juce::jmax (tailEnd, st.peak);
                    }
                    expect (tailFinite);
                    if (beatIdx <= 5)
                        expect (tailEnd < juce::jmax (0.05f, tailStart * 0.5f),
                                "tail does not decay: " + juce::String (tailStart) + " -> " + juce::String (tailEnd));
                }
        }

        beginTest ("HELIX loops one beat and, at 100 % LEVEL/DEPTH, no longer lets the live input through");
        {
            BeatFxEngine::Settings s;
            s.on = true;
            s.type = BeatFxType::Helix;
            s.beatIdx = 3; // 1/2 beat = ~234 ms at 128 BPM
            s.level = 1.0f;
            s.quantize = false;
            BeatFxEngine a, b;
            a.prepare (sr, block);
            b.prepare (sr, block);
            juce::AudioBuffer<float> bufA (2, block), bufB (2, block);
            juce::Random capture (7);
            for (int i = 0; i < 60; ++i) // ~320 ms: the loop is captured from the same audio in both
            {
                fillNoise (bufA, capture);
                bufB.makeCopyOf (bufA);
                a.process (bufA, s, tempo);
                b.process (bufB, s, tempo);
            }
            float maxDiff = 0.0f, peak = 0.0f;
            for (int i = 0; i < 100; ++i) // then A gets silence and B gets new noise
            {
                bufA.clear();
                fillNoise (bufB, rng);
                a.process (bufA, s, tempo);
                b.process (bufB, s, tempo);
                for (int ch = 0; ch < 2; ++ch)
                    for (int n = 0; n < block; ++n)
                    {
                        maxDiff = juce::jmax (maxDiff, std::abs (bufA.getSample (ch, n) - bufB.getSample (ch, n)));
                        peak = juce::jmax (peak, std::abs (bufA.getSample (ch, n)));
                    }
            }
            expect (peak > 0.1f, "the loop is not playing");
            expectLessThan (maxDiff, 1.0e-4f);
        }

        beginTest ("SPIRAL repeats forever while on, then fades out when switched off");
        {
            BeatFxEngine engine;
            engine.prepare (sr, block);
            BeatFxEngine::Settings s;
            s.on = true;
            s.type = BeatFxType::Spiral;
            s.beatIdx = 3; // 1/2 beat
            s.level = 1.0f;
            s.quantize = false;
            juce::AudioBuffer<float> buf (2, block);
            auto runSilence = [&] (int blocks)
            {
                float peak = 0.0f;
                for (int b = 0; b < blocks; ++b)
                {
                    buf.clear();
                    engine.process (buf, s, tempo);
                    peak = juce::jmax (peak, stats (buf).peak);
                }
                return peak;
            };
            for (int b = 0; b < 20; ++b) // ~100 ms of noise into the loop
            {
                fillNoise (buf, rng);
                engine.process (buf, s, tempo);
            }
            const float early = runSilence (100);   // first ~0.5 s of repeats
            const float later = runSilence (1000);  // ~5 s later
            const float evenLater = runSilence (100);
            expect (evenLater > early * 0.5f, "spiral decays while on: " + juce::String (early) + " -> " + juce::String (evenLater));
            expect (later < 4.0f);
            s.on = false;
            runSilence (1000);
            expectLessThan (runSilence (50), 0.05f);
        }

        beginTest ("Beat FX off is transparent");
        {
            BeatFxEngine engine;
            engine.prepare (sr, block);
            BeatFxEngine::Settings s;
            juce::AudioBuffer<float> buf (2, block);
            fillNoise (buf, rng);
            juce::AudioBuffer<float> dry (buf);
            engine.process (buf, s, tempo);
            float maxDiff = 0.0f;
            for (int i = 0; i < block; ++i)
                maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - dry.getSample (0, i)));
            expectLessThan (maxDiff, 1.0e-6f);
        }

        for (int t = 0; t < kNumColorFx; ++t)
        {
            beginTest ("Color FX " + colorFxNames()[t] + " is stable and dry at center");
            for (float amount : { -1.0f, -0.4f, 0.4f, 1.0f })
            {
                ColorFxEngine engine;
                engine.prepare (sr, block);
                ColorFxEngine::Settings s { true, (ColorFxType) t, amount, 0.8f };
                juce::AudioBuffer<float> buf (2, block);
                float peak = 0.0f;
                bool finite = true;
                for (int b = 0; b < 300; ++b)
                {
                    fillNoise (buf, rng);
                    engine.process (buf, s, tempo);
                    const auto st = stats (buf);
                    finite &= st.finite;
                    peak = juce::jmax (peak, st.peak);
                }
                expect (finite);
                expect (peak < 8.0f, "peak " + juce::String (peak));
            }

            ColorFxEngine engine;
            engine.prepare (sr, block);
            ColorFxEngine::Settings s { true, (ColorFxType) t, 0.0f, 0.5f };
            juce::AudioBuffer<float> buf (2, block);
            float maxDiff = 0.0f;
            for (int b = 0; b < 50; ++b)
            {
                fillNoise (buf, rng);
                juce::AudioBuffer<float> dry (buf);
                engine.process (buf, s, tempo);
                if (b > 10)
                    for (int i = 0; i < block; ++i)
                        maxDiff = juce::jmax (maxDiff, std::abs (buf.getSample (0, i) - dry.getSample (0, i)));
            }
            expectLessThan (maxDiff, 1.0e-4f);
        }

        beginTest ("CRUSH keeps the volume constant across the COLOR / PARAMETER range");
        for (float amount : { -1.0f, -0.6f, -0.3f, 0.3f, 0.6f, 1.0f })
            for (float param : { 0.1f, 0.9f })
            {
                ColorFxEngine engine;
                engine.prepare (sr, block);
                ColorFxEngine::Settings s { true, ColorFxType::Crush, amount, param };
                juce::AudioBuffer<float> buf (2, block);
                double inEnergy = 0.0, outEnergy = 0.0;
                for (int b = 0; b < 400; ++b)
                {
                    fillNoise (buf, rng, 0.3f);
                    const double e = stats (buf).energy;
                    engine.process (buf, s, tempo);
                    if (b >= 100) { inEnergy += e; outEnergy += stats (buf).energy; }
                }
                const double db = 10.0 * std::log10 (outEnergy / inEnergy);
                expect (std::abs (db) < 2.0, "COLOR " + juce::String (amount) + " PARAMETER " + juce::String (param)
                                                 + ": level change " + juce::String (db, 1) + " dB");
            }

        beginTest ("Full processor renders every factory preset safely");
        {
            RecklessDJFXProcessor proc;
            proc.setPlayConfigDetails (2, 2, sr, block);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            for (int i = 0; i < proc.getNumPrograms(); ++i)
            {
                proc.setCurrentProgram (i);
                for (int b = 0; b < 20; ++b)
                {
                    fillNoise (buf, rng, 0.3f);
                    proc.processBlock (buf, midi);
                    const auto st = stats (buf);
                    if (! st.finite || st.peak > 10.0f)
                    {
                        expect (false, "preset " + proc.getProgramName (i) + " misbehaves");
                        break;
                    }
                }
            }
        }
    }
};

static TempoTests tempoTests;
static EffectTests effectTests;
