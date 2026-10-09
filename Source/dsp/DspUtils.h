#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace rdfx
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

inline float wrap01 (double x) noexcept { return (float) (x - std::floor (x)); }

/** Circular stereo buffer with fractional (cubic) read. */
class StereoDelayLine
{
public:
    void prepare (double sampleRate, double maxSeconds)
    {
        size = juce::jmax (16, (int) std::ceil (sampleRate * maxSeconds) + 8);
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        writePos = 0;
    }

    int capacity() const noexcept { return size - 4; }
    int getWritePos() const noexcept { return writePos; }

    void push (float l, float r) noexcept
    {
        bufL[(size_t) writePos] = l;
        bufR[(size_t) writePos] = r;
        if (++writePos >= size) writePos = 0;
    }

    /** Reads `delay` samples behind the most recently pushed sample (delay >= 1). */
    float readL (float delay) const noexcept { return read (bufL, delay); }
    float readR (float delay) const noexcept { return read (bufR, delay); }

    /** Absolute index read (used by the roll looper). */
    float atL (int index) const noexcept { return bufL[(size_t) wrapIndex (index)]; }
    float atR (int index) const noexcept { return bufR[(size_t) wrapIndex (index)]; }

    int wrapIndex (int i) const noexcept
    {
        i %= size;
        return i < 0 ? i + size : i;
    }

private:
    float read (const std::vector<float>& b, float delay) const noexcept
    {
        delay = juce::jlimit (1.0f, (float) capacity(), delay);
        const float pos = (float) writePos - delay;
        const int i1 = (int) std::floor (pos);
        const float t = pos - (float) i1;
        const float y0 = b[(size_t) wrapIndex (i1 - 1)];
        const float y1 = b[(size_t) wrapIndex (i1)];
        const float y2 = b[(size_t) wrapIndex (i1 + 1)];
        const float y3 = b[(size_t) wrapIndex (i1 + 2)];
        // 4-point, 3rd-order Hermite
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    std::vector<float> bufL, bufR;
    int size = 0, writePos = 0;
};

/** One-pole lowpass, also usable as a parameter smoother. */
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, double sr) noexcept { a = std::exp (-kTwoPi * hz / (float) sr); }
    void setTime (float seconds, double sr) noexcept { a = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (seconds * (float) sr)); }
    float process (float x) noexcept { z = x + a * (z - x); return z; }
    void reset (float v = 0.0f) noexcept { z = v; }
};

/** Simple Schroeder allpass used for diffusion. */
struct Allpass
{
    std::vector<float> buf;
    int idx = 0;
    float g = 0.6f;
    void prepare (int len) { buf.assign ((size_t) juce::jmax (1, len), 0.0f); idx = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); idx = 0; }
    float process (float x) noexcept
    {
        const float d = buf[(size_t) idx];
        const float y = -g * x + d;
        buf[(size_t) idx] = x + g * y;
        if (++idx >= (int) buf.size()) idx = 0;
        return y;
    }
};

/** Stereo TPT state-variable filter with cheap coefficient updates. */
class Svf
{
public:
    void prepare (double sr) { sampleRate = sr; reset(); setParams (1000.0f, 0.707f); }
    void reset() { for (auto& s : ic1) s = 0; for (auto& s : ic2) s = 0; }

    void setParams (float cutoffHz, float q) noexcept
    {
        cutoffHz = juce::jlimit (10.0f, (float) (sampleRate * 0.45), cutoffHz);
        g = std::tan (juce::MathConstants<float>::pi * cutoffHz / (float) sampleRate);
        k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { float lp, bp, hp; };

    Out process (int ch, float x) noexcept
    {
        const float v3 = x - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.0f * v1 - ic1[ch];
        ic2[ch] = 2.0f * v2 - ic2[ch];
        return { v2, v1, x - k * v1 - v2 };
    }

private:
    double sampleRate = 44100.0;
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1[2] {}, ic2[2] {};
};

/** Soft limiter to keep feedback paths and outputs musical and bounded. */
inline float softClip (float x) noexcept
{
    if (x > 1.5f) return 1.0f;
    if (x < -1.5f) return -1.0f;
    return x - (4.0f / 27.0f) * x * x * x;
}

inline float sanitize (float x) noexcept
{
    return std::isfinite (x) ? x : 0.0f;
}
} // namespace rdfx
