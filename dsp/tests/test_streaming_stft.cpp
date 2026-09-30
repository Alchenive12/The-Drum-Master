// File: dsp/tests/test_streaming_stft.cpp
// Streaming STFT: perfect reconstruction with an identity mask (exact latency), linear masks,
// and the frame callback cadence.
#include "dsp_test.h"
#include <plugin_dsp/StreamingStft.h>

#include <random>

namespace
{
std::vector<float> noise (std::size_t n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    std::vector<float> x (n);
    for (auto& v : x) v = d (rng);
    return x;
}

double maxErrorVsDelayed (const std::vector<float>& y, const std::vector<float>& x, int delay, float gain)
{
    double e = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        const long src = static_cast<long> (i) - delay;
        const float expected = src >= 0 ? gain * x[static_cast<std::size_t> (src)] : 0.0f;
        e = std::fmax (e, std::fabs (static_cast<double> (y[i] - expected)));
    }
    return e;
}
} // namespace

int main()
{
    using plugin_dsp::StreamingStft;

    struct Cfg { std::size_t n, hop; };
    for (const Cfg c : { Cfg { 1024, 128 }, Cfg { 256, 64 }, Cfg { 2048, 256 }, Cfg { 64, 16 } })
    {
        StreamingStft stft;
        stft.prepare (c.n, c.hop);
        DSP_CHECK (stft.latency() == static_cast<int> (c.n) - 1);
        DSP_CHECK (stft.numBins() == c.n / 2 + 1);

        const auto x = noise (20000, static_cast<unsigned> (c.n));

        // Identity mask -> input delayed by exactly latency() samples, from the very first sample.
        std::size_t frames = 0;
        std::vector<float> y (x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = stft.processSample (x[i], [&] (StreamingStft::Complex*, std::size_t bins, std::size_t) {
                DSP_CHECK (bins == c.n / 2 + 1);
                ++frames;
            });
        DSP_CHECK (maxErrorVsDelayed (y, x, stft.latency(), 1.0f) < 2e-5);
        DSP_CHECK (frames == x.size() / c.hop);
        DSP_CHECK (dsptest::allFinite (y));

        // A constant gain mask scales the output exactly (linearity).
        stft.reset();
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = stft.processSample (x[i], [] (StreamingStft::Complex* b, std::size_t bins, std::size_t) {
                for (std::size_t k = 0; k < bins; ++k) b[k] *= 0.25f;
            });
        DSP_CHECK (maxErrorVsDelayed (y, x, stft.latency(), 0.25f) < 1e-5);

        // Zero mask -> silence.
        stft.reset();
        double peak = 0.0;
        for (std::size_t i = 0; i < x.size(); ++i)
            peak = std::fmax (peak, std::fabs (static_cast<double> (stft.processSample (x[i], [] (StreamingStft::Complex* b, std::size_t bins, std::size_t) {
                for (std::size_t k = 0; k < bins; ++k) b[k] = {};
            }))));
        DSP_CHECK (peak < 1e-7);
    }

    // Masking out a band removes it: 1 kHz survives, 8 kHz is gone (N = 1024 at 48 kHz, bin = 46.9 Hz).
    {
        StreamingStft stft;
        stft.prepare (1024, 128);
        const auto a = dsptest::sine (1000.0, 48000.0, 48000), b = dsptest::sine (8000.0, 48000.0, 48000);
        std::vector<float> y (a.size());
        for (std::size_t i = 0; i < a.size(); ++i)
            y[i] = stft.processSample (0.5f * (a[i] + b[i]), [] (StreamingStft::Complex* bins, std::size_t nb, std::size_t) {
                for (std::size_t k = 0; k < nb; ++k)
                    if (static_cast<double> (k) * 48000.0 / 1024.0 > 4000.0) bins[k] = {};
            });
        DSP_CHECK_NEAR (dsptest::magnitudeAt (y, 1000.0, 48000.0, 4800), 0.5, 0.01);
        DSP_CHECK (dsptest::magnitudeAt (y, 8000.0, 48000.0, 4800) < 0.005);
    }

    DSP_TEST_END;
}
