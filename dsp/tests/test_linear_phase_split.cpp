// File: dsp/tests/test_linear_phase_split.cpp
// Complementary split: low + high == delayed input; bands separate around the crossover;
// taps and latency scale with the sample rate.
#include "dsp_test.h"
#include <plugin_dsp/LinearPhaseSplit.h>

#include <random>

namespace
{
struct Bands { std::vector<float> low, high; };

Bands run (plugin_dsp::LinearPhaseSplit& s, const std::vector<float>& x)
{
    Bands b { std::vector<float> (x.size()), std::vector<float> (x.size()) };
    for (std::size_t i = 0; i < x.size(); ++i) b.low[i] = s.processSample (x[i], b.high[i]);
    return b;
}
} // namespace

int main()
{
    using plugin_dsp::LinearPhaseSplit;

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        LinearPhaseSplit s;
        s.prepare (sr, 1500.0);
        DSP_CHECK (s.numTaps() % 2 == 1);
        DSP_CHECK (s.latency() == static_cast<int> ((s.numTaps() - 1) / 2));
        DSP_CHECK_NEAR (1000.0 * s.latency() / sr, 2.64, 0.05);   // ~2.6 ms at every rate

        // Perfect reconstruction on noise.
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        std::vector<float> x (static_cast<std::size_t> (sr / 2));
        for (auto& v : x) v = d (rng);
        const auto b = run (s, x);
        double err = 0.0;
        for (std::size_t i = 0; i < x.size(); ++i)
        {
            const long src = static_cast<long> (i) - s.latency();
            const float expected = src >= 0 ? x[static_cast<std::size_t> (src)] : 0.0f;
            err = std::fmax (err, std::fabs (static_cast<double> (b.low[i] + b.high[i] - expected)));
        }
        DSP_CHECK (err < 1e-5);
        DSP_CHECK (dsptest::allFinite (b.low) && dsptest::allFinite (b.high));

        // Separation: 200 Hz stays low, 6 kHz goes high.
        const std::size_t n = static_cast<std::size_t> (sr / 2), skip = static_cast<std::size_t> (sr / 20);
        s.reset();
        const auto lo = run (s, dsptest::sine (200.0, sr, n));
        DSP_CHECK_NEAR (dsptest::magnitudeAt (lo.low, 200.0, sr, skip), 1.0, 0.01);
        DSP_CHECK (dsptest::magnitudeAt (lo.high, 200.0, sr, skip) < 0.01);
        s.reset();
        const auto hi = run (s, dsptest::sine (6000.0, sr, n));
        DSP_CHECK_NEAR (dsptest::magnitudeAt (hi.high, 6000.0, sr, skip), 1.0, 0.01);
        DSP_CHECK (dsptest::magnitudeAt (hi.low, 6000.0, sr, skip) < 0.01);
    }

    DSP_TEST_END;
}
