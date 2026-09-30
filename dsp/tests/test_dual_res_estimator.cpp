// File: dsp/tests/test_dual_res_estimator.cpp
// Dual-resolution estimator: reconstruction, complementary shares, frequency separation,
// latency budget (must fit inside the 50 ms lookahead, docs/02-algorithm.md D2).
#include "dsp_test.h"
#include <plugin_dsp/DualResEstimator.h>

#include <random>

namespace
{
using plugin_dsp::DualResEstimator;

std::vector<float> noise (std::size_t n, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    std::vector<float> x (n);
    for (auto& v : x) v = d (rng);
    return x;
}

double maxErr (const std::vector<float>& y, const std::vector<float>& x, int delay)
{
    double e = 0.0;
    for (std::size_t i = 0; i < y.size(); ++i)
    {
        const long src = static_cast<long> (i) - delay;
        const float expected = src >= 0 ? x[static_cast<std::size_t> (src)] : 0.0f;
        e = std::fmax (e, std::fabs (static_cast<double> (y[i] - expected)));
    }
    return e;
}
} // namespace

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        DualResEstimator est;
        est.prepare (sr, 3);
        DSP_CHECK (est.maxOutputs() == 3);
        const double latencyMs = 1000.0 * est.latency() / sr;
        DSP_CHECK (latencyMs > 20.0 && latencyMs < 30.0);   // fits the 50 ms lookahead with room to spare

        const auto x = noise (static_cast<std::size_t> (sr * 0.75), 3);

        // 1) One output with share 1 everywhere -> the input, delayed by latency().
        {
            std::vector<float> y (x.size());
            float o = 0.0f;
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                est.processSample (x[i], &o, 1, [] (DualResEstimator::Band, std::size_t, std::size_t, std::size_t) { return 1.0f; });
                y[i] = o;
            }
            DSP_CHECK (maxErr (y, x, est.latency()) < 1e-4);
            DSP_CHECK (dsptest::allFinite (y));
        }

        // 2) Three outputs with shares 0.5 / 0.3 / 0.2 -> they sum back to the delayed input.
        {
            est.reset();
            std::vector<float> sum (x.size());
            float o[3] = {};
            const float shares[3] = { 0.5f, 0.3f, 0.2f };
            for (std::size_t i = 0; i < x.size(); ++i)
            {
                est.processSample (x[i], o, 3, [&] (DualResEstimator::Band, std::size_t out, std::size_t, std::size_t) { return shares[out]; });
                sum[i] = o[0] + o[1] + o[2];
            }
            DSP_CHECK (maxErr (sum, x, est.latency()) < 1e-4);
        }

        // 3) Frequency shares: output 0 owns < 300 Hz, output 1 the rest.
        //    A 100 Hz tone lands in output 0, a 5 kHz tone in output 1.
        {
            const double loBin = est.binHz (DualResEstimator::Band::low);
            auto share = [&] (DualResEstimator::Band b, std::size_t out, std::size_t k, std::size_t) {
                const bool bass = b == DualResEstimator::Band::low && static_cast<double> (k) * loBin < 300.0;
                return (out == 0) == bass ? 1.0f : 0.0f;
            };
            const std::size_t n = static_cast<std::size_t> (sr * 0.5), skip = static_cast<std::size_t> (sr * 0.1);
            const auto a = dsptest::sine (100.0, sr, n), b = dsptest::sine (5000.0, sr, n);
            std::vector<float> o0 (n), o1 (n);
            float o[2] = {};
            est.reset();
            for (std::size_t i = 0; i < n; ++i)
            {
                est.processSample (0.5f * (a[i] + b[i]), o, 2, share);
                o0[i] = o[0];
                o1[i] = o[1];
            }
            DSP_CHECK_NEAR (dsptest::magnitudeAt (o0, 100.0, sr, skip), 0.5, 0.02);
            DSP_CHECK (dsptest::magnitudeAt (o0, 5000.0, sr, skip) < 0.005);
            DSP_CHECK_NEAR (dsptest::magnitudeAt (o1, 5000.0, sr, skip), 0.5, 0.02);
            DSP_CHECK (dsptest::magnitudeAt (o1, 100.0, sr, skip) < 0.005);
        }

        // 4) Zero active outputs is valid (no drum active) and doesn't disturb later estimates.
        {
            est.reset();
            float o = 0.0f;
            for (std::size_t i = 0; i < 1000; ++i)
                est.processSample (x[i], &o, 0, [] (DualResEstimator::Band, std::size_t, std::size_t, std::size_t) { return 1.0f; });
            DSP_CHECK_NEAR (o, 0.0f, 0.0);
        }
    }

    DSP_TEST_END;
}
