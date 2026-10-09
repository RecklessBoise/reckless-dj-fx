#include "ColorFx.h"

namespace rdfx
{
void ColorFxEngine::prepare (double sr, int)
{
    sampleRate = sr;
    filter.prepare (sr);
    noiseFilter.prepare (sr);
    wetFilter.prepare (sr);
    space.setSampleRate (sr);
    dub.prepare (sr, 4.0);
    amountSmooth.setTime (0.015f, sr);
    paramSmooth.setTime (0.03f, sr);
    gateSmooth.setTime (0.002f, sr);
    // Long enough windows that the make-up follows the loudness, not the waveform (no pumping)
    crushInPower.setTime (0.40f, sr);
    crushOutPower.setTime (0.40f, sr);
    crushGain.setTime (0.30f, sr);
    dubDampL.setCutoff (3000.0f, sr);
    dubDampR.setCutoff (3000.0f, sr);
    onStep = 1.0f / (float) (0.008 * sr);
    switchStep = 1.0f / (float) (0.010 * sr);
    reset();
}

void ColorFxEngine::reset()
{
    filter.reset();
    noiseFilter.reset();
    wetFilter.reset();
    space.reset();
    dub.reset();
    dubDampL.reset();
    dubDampR.reset();
    gateSmooth.reset (1.0f);
    amountSmooth.reset();
    paramSmooth.reset (0.5f);
    onGain = 0.0f;
    switchFade = 1.0f;
    crushHold[0] = crushHold[1] = 0.0f;
    crushInPower.reset();
    crushOutPower.reset();
    crushGain.reset (1.0f);
    crushCounter[0] = crushCounter[1] = 0.0f;
}

void ColorFxEngine::process (juce::AudioBuffer<float>& buffer, const Settings& s, const TempoSync::State& tempo) noexcept
{
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    const float dubTarget = (float) TempoSync::beatsToSamples (0.75, tempo.bpm, sampleRate); // dotted 1/8
    const float dubCap = (float) (dub.capacity() - 1);

    for (int i = 0; i < n; ++i)
    {
        // Type change: fade out, swap and clear state, fade in
        if (s.type != currentType && switchFade <= 0.0f)
        {
            currentType = s.type;
            filter.reset(); noiseFilter.reset(); wetFilter.reset();
            space.reset(); dub.reset();
        }
        switchFade = juce::jlimit (0.0f, 1.0f, switchFade + (s.type != currentType ? -switchStep : switchStep));
        onGain = juce::jlimit (0.0f, 1.0f, onGain + (s.on ? onStep : -onStep));

        const float k = amountSmooth.process (s.amount);
        const float p = paramSmooth.process (s.param);
        const float a = std::abs (k);
        const bool left = k < 0.0f;
        const float presence = juce::jmin (1.0f, a * 25.0f); // exactly dry at the center position

        const float xL = L[i];
        const float xR = R != nullptr ? R[i] : xL;
        float yL = xL, yR = xR;

        if (onGain > 0.0f)
        {
            const bool updateCoeffs = (counter++ & 15) == 0;

            switch (currentType)
            {
                case ColorFxType::Filter:
                {
                    if (updateCoeffs)
                        filter.setParams (left ? 20000.0f * std::pow (2.0f, -a * 9.5f) : 20.0f * std::pow (2.0f, a * 9.5f),
                                          0.7f + p * 5.0f);
                    const auto oL = filter.process (0, xL);
                    const auto oR = filter.process (1, xR);
                    yL = xL + ((left ? oL.lp : oL.hp) - xL) * presence;
                    yR = xR + ((left ? oR.lp : oR.hp) - xR) * presence;
                    break;
                }
                case ColorFxType::Space:
                {
                    if (updateCoeffs)
                    {
                        wetFilter.setParams (left ? 20000.0f * std::pow (2.0f, -a * 5.6f) : 20.0f * std::pow (2.0f, a * 7.6f), 0.8f);
                        juce::Reverb::Parameters rp;
                        rp.roomSize = 0.5f + p * 0.48f;
                        rp.damping = 0.3f;
                        rp.wetLevel = 1.0f;
                        rp.dryLevel = 0.0f;
                        rp.width = 1.0f;
                        space.setParameters (rp);
                    }
                    const auto fL = wetFilter.process (0, xL * a);
                    const auto fR = wetFilter.process (1, xR * a);
                    float wl = left ? fL.lp : fL.hp;
                    float wr = left ? fR.lp : fR.hp;
                    space.processStereo (&wl, &wr, 1);
                    yL = xL + 0.9f * wl;
                    yR = xR + 0.9f * wr;
                    break;
                }
                case ColorFxType::DubEcho:
                {
                    if (updateCoeffs)
                        wetFilter.setParams (left ? 12000.0f * std::pow (2.0f, -a * 4.5f) : 40.0f * std::pow (2.0f, a * 6.0f), 0.9f);
                    dubDelaySamples += 0.0005f * (juce::jmin (dubTarget, dubCap) - dubDelaySamples);
                    const float wl = dub.readL (dubDelaySamples);
                    const float wr = dub.readR (dubDelaySamples);
                    const float fb = 0.45f + p * 0.5f;
                    const auto fL = wetFilter.process (0, wl);
                    const auto fR = wetFilter.process (1, wr);
                    const float filtL = left ? fL.lp : fL.hp;
                    const float filtR = left ? fR.lp : fR.hp;
                    dub.push (sanitize (softClip (xL * a + fb * dubDampL.process (filtL))),
                              sanitize (softClip (xR * a + fb * dubDampR.process (filtR))));
                    yL = xL + wl;
                    yR = xR + wr;
                    break;
                }
                case ColorFxType::Sweep:
                {
                    if (left)
                    {
                        // Gate chopping on a 1/16 grid; PARAMETER tightens the gate
                        const double pos = (tempo.beatPos + i * tempo.beatsPerSample) / 0.25;
                        const float ph = wrap01 (pos);
                        const float open = ph < (0.6f - p * 0.4f) ? 1.0f : 1.0f - a;
                        const float g = gateSmooth.process (open);
                        yL = xL * g;
                        yR = xR * g;
                    }
                    else
                    {
                        if (updateCoeffs)
                            filter.setParams (120.0f * std::pow (2.0f, a * 6.8f), 1.0f + p * 7.0f);
                        const float bl = filter.process (0, xL).bp * 1.6f;
                        const float br = filter.process (1, xR).bp * 1.6f;
                        const float mix = juce::jmin (1.0f, a * 3.0f);
                        yL = xL + (bl - xL) * mix;
                        yR = xR + (br - xR) * mix;
                    }
                    break;
                }
                case ColorFxType::Noise:
                {
                    if (updateCoeffs)
                        noiseFilter.setParams (left ? 18000.0f * std::pow (2.0f, -a * 6.5f) : 30.0f * std::pow (2.0f, a * 8.0f), 1.2f);
                    const float level = a * (0.05f + 0.35f * p);
                    const auto nL = noiseFilter.process (0, rng.nextFloat() * 2.0f - 1.0f);
                    const auto nR = noiseFilter.process (1, rng.nextFloat() * 2.0f - 1.0f);
                    yL = xL + (left ? nL.lp : nL.hp) * level;
                    yR = xR + (left ? nR.lp : nR.hp) * level;
                    break;
                }
                case ColorFxType::Crush:
                {
                    // Mixer-style crush: drive into a saturator, then a companded (mu-law) quantiser so the
                    // quiet tails break up into grit, a light sample-rate reduction and a resonant LPF (left) /
                    // HPF (right). PARAMETER sets how hard it crushes.
                    if (updateCoeffs)
                        filter.setParams (left ? 18000.0f * std::pow (2.0f, -a * 4.6f) : 25.0f * std::pow (2.0f, a * 6.2f),
                                          1.1f + 1.4f * a);
                    const float drive = 1.0f + a * (2.5f + 9.0f * p);
                    const float norm = 1.0f / std::tanh (drive);
                    const float bits = juce::jmax (3.0f, 16.0f - a * (10.0f + 2.5f * p));
                    const float steps = std::pow (2.0f, bits - 1.0f);
                    const float hold = 1.0f + std::pow (a, 1.5f) * (2.0f + 12.0f * p);
                    constexpr float mu = 48.0f;
                    const float muNorm = 1.0f / std::log1p (mu);
                    float in[2] = { xL, xR }, out[2];
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        if ((crushCounter[ch] += 1.0f) >= hold)
                        {
                            crushCounter[ch] -= hold;
                            const float sat = std::tanh (in[ch] * drive) * norm;
                            const float c = std::copysign (std::log1p (mu * std::abs (sat)) * muNorm, sat);
                            const float q = std::round (c * steps) / steps;
                            crushHold[ch] = std::copysign ((std::pow (1.0f + mu, std::abs (q)) - 1.0f) / mu, q);
                        }
                        const auto o = filter.process (ch, crushHold[ch]);
                        out[ch] = left ? o.lp : o.hp;
                    }

                    // Loudness match: the crushed signal is scaled so its RMS follows the input's,
                    // so turning COLOR / PARAMETER changes the sound, not the volume (the saturator alone
                    // can be ~20 dB hotter than the input, hence the wide range)
                    const float inPow = crushInPower.process (0.5f * (xL * xL + xR * xR));
                    const float outPow = crushOutPower.process (0.5f * (out[0] * out[0] + out[1] * out[1]));
                    const float makeup = crushGain.process (juce::jlimit (0.03f, 4.0f, std::sqrt ((inPow + 1.0e-9f) / (outPow + 1.0e-9f))));
                    for (int ch = 0; ch < 2; ++ch)
                        out[ch] = in[ch] + (out[ch] * makeup - in[ch]) * presence;
                    yL = out[0];
                    yR = out[1];
                    break;
                }
            }
        }

        const float g = onGain * switchFade;
        L[i] = sanitize (xL + (yL - xL) * g);
        if (R != nullptr) R[i] = sanitize (xR + (yR - xR) * g);
    }
}
} // namespace rdfx
