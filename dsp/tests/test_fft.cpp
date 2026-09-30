// File: dsp/tests/test_fft.cpp
// FFT against a naive DFT, round trip, and a known sine bin.
#include "dsp_test.h"
#include <plugin_dsp/Fft.h>

#include <complex>
#include <random>

int main()
{
    using plugin_dsp::Fft;
    using C = Fft::Complex;

    for (std::size_t n : { std::size_t { 2 }, std::size_t { 8 }, std::size_t { 64 }, std::size_t { 1024 } })
    {
        Fft fft;
        fft.prepare (n);
        DSP_CHECK (fft.size() == n);

        std::mt19937 rng (static_cast<unsigned> (n));
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        std::vector<C> x (n);
        for (auto& v : x) v = C (d (rng), d (rng));

        // Forward vs naive DFT
        std::vector<C> X = x;
        fft.forward (X.data());
        double maxErr = 0.0;
        for (std::size_t k = 0; k < n; ++k)
        {
            std::complex<double> acc (0.0, 0.0);
            for (std::size_t i = 0; i < n; ++i)
            {
                const double a = -2.0 * dsptest::pi * static_cast<double> (i * k) / static_cast<double> (n);
                acc += std::complex<double> (x[i]) * std::complex<double> (std::cos (a), std::sin (a));
            }
            maxErr = std::fmax (maxErr, std::abs (acc - std::complex<double> (X[k])));
        }
        DSP_CHECK (maxErr < 1e-3 * static_cast<double> (n));

        // Round trip
        std::vector<C> y = X;
        fft.inverse (y.data());
        double rtErr = 0.0;
        for (std::size_t i = 0; i < n; ++i) rtErr = std::fmax (rtErr, static_cast<double> (std::abs (y[i] - x[i])));
        DSP_CHECK (rtErr < 1e-5);
    }

    // A cosine at bin 5 of a 64-point FFT puts N/2 into bins 5 and 59, nothing elsewhere.
    {
        Fft fft;
        fft.prepare (64);
        std::vector<C> x (64);
        for (std::size_t i = 0; i < 64; ++i)
            x[i] = C (static_cast<float> (std::cos (2.0 * dsptest::pi * 5.0 * static_cast<double> (i) / 64.0)), 0.0f);
        fft.forward (x.data());
        DSP_CHECK_NEAR (std::abs (x[5]), 32.0, 1e-3);
        DSP_CHECK_NEAR (std::abs (x[59]), 32.0, 1e-3);
        DSP_CHECK_NEAR (std::abs (x[6]), 0.0, 1e-3);
    }

    DSP_TEST_END;
}
