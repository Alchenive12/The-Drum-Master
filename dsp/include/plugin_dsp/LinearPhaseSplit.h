// File: dsp/include/plugin_dsp/LinearPhaseSplit.h
// Complementary two-band split: low = h * x (linear-phase FIR low-pass), high = x(n - D) - low.
// low + high == input delayed by latency() = (taps - 1) / 2, exactly (up to float rounding).
// Splits the audio for the dual-resolution STFT estimator (docs/02-algorithm.md, D1).
// prepare() allocates; processSample() never does.
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace plugin_dsp
{
class LinearPhaseSplit
{
public:
    /** Taps are scaled with the sample rate (255 at 48 kHz, always odd), Blackman-windowed sinc. */
    void prepare (double sampleRate, double crossoverHz)
    {
        std::size_t taps = static_cast<std::size_t> (std::lround (255.0 * sampleRate / 48000.0));
        if (taps % 2 == 0) ++taps;
        if (taps < 31) taps = 31;
        h.assign (taps, 0.0f);

        const double M = static_cast<double> (taps - 1) / 2.0, wc = 2.0 * crossoverHz / sampleRate;
        double sum = 0.0;
        std::vector<double> hd (taps);
        for (std::size_t i = 0; i < taps; ++i)
        {
            const double k = static_cast<double> (i) - M;
            const double sinc = std::fabs (k) < 1e-12 ? wc : std::sin (pi * wc * k) / (pi * k);
            const double t = static_cast<double> (i) / static_cast<double> (taps - 1);
            const double blackman = 0.42 - 0.5 * std::cos (2.0 * pi * t) + 0.08 * std::cos (4.0 * pi * t);
            hd[i] = sinc * blackman;
            sum += hd[i];
        }
        for (std::size_t i = 0; i < taps; ++i) h[i] = static_cast<float> (hd[i] / sum);   // DC gain exactly 1

        delay = (taps - 1) / 2;
        buf.assign (2 * taps, 0.0f);   // doubled so every read window is contiguous
        reset();
    }

    void reset() noexcept
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        pos = 0;
    }

    int latency() const noexcept { return static_cast<int> (delay); }
    std::size_t numTaps() const noexcept { return h.size(); }

    /** Returns the low band; the high band is written to highOut. */
    float processSample (float x, float& highOut) noexcept
    {
        const std::size_t taps = h.size();
        buf[pos] = x;
        buf[pos + taps] = x;
        // buf[pos + 1 .. pos + taps] holds the last `taps` samples, oldest first.
        const float* w = buf.data() + pos + 1;
        float low = 0.0f;
        for (std::size_t i = 0; i < taps; ++i) low += h[i] * w[i];
        const float delayed = w[taps - 1 - delay];
        highOut = delayed - low;
        pos = (pos + 1) % taps;
        return low;
    }

private:
    static constexpr double pi = 3.14159265358979323846;
    std::vector<float> h, buf;
    std::size_t delay = 0, pos = 0;
};
} // namespace plugin_dsp
