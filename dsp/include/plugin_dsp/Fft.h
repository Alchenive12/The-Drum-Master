// File: dsp/include/plugin_dsp/Fft.h
// In-place radix-2 complex FFT. Standard C++ only, no JUCE.
// prepare() allocates the tables; forward()/inverse() never allocate (safe in processBlock).
#pragma once

#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace plugin_dsp
{
class Fft
{
public:
    using Complex = std::complex<float>;

    /** size must be a power of two >= 2. Allocates. */
    void prepare (std::size_t size)
    {
        assert (size >= 2 && (size & (size - 1)) == 0);
        n = size;
        bitrev.assign (n, 0);
        std::size_t bits = 0;
        while ((std::size_t { 1 } << bits) < n) ++bits;
        for (std::size_t i = 0; i < n; ++i)
        {
            std::size_t r = 0;
            for (std::size_t b = 0; b < bits; ++b)
                if (i & (std::size_t { 1 } << b)) r |= std::size_t { 1 } << (bits - 1 - b);
            bitrev[i] = r;
        }
        twiddles.resize (n / 2);
        for (std::size_t k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * 3.14159265358979323846 * static_cast<double> (k) / static_cast<double> (n);
            twiddles[k] = Complex (static_cast<float> (std::cos (a)), static_cast<float> (std::sin (a)));
        }
    }

    std::size_t size() const noexcept { return n; }

    /** X[k] = sum x[i] e^{-2 pi j i k / N}. Unscaled. */
    void forward (Complex* data) const noexcept { transform (data, false); }

    /** Inverse including the 1/N scale, so inverse(forward(x)) == x. */
    void inverse (Complex* data) const noexcept
    {
        transform (data, true);
        const float s = 1.0f / static_cast<float> (n);
        for (std::size_t i = 0; i < n; ++i) data[i] *= s;
    }

private:
    void transform (Complex* a, bool inv) const noexcept
    {
        for (std::size_t i = 0; i < n; ++i)
            if (i < bitrev[i]) std::swap (a[i], a[bitrev[i]]);

        for (std::size_t len = 2; len <= n; len <<= 1)
        {
            const std::size_t half = len / 2, step = n / len;
            for (std::size_t i = 0; i < n; i += len)
                for (std::size_t j = 0; j < half; ++j)
                {
                    const Complex w = inv ? std::conj (twiddles[j * step]) : twiddles[j * step];
                    const Complex u = a[i + j], v = a[i + j + half] * w;
                    a[i + j] = u + v;
                    a[i + j + half] = u - v;
                }
        }
    }

    std::size_t n = 0;
    std::vector<std::size_t> bitrev;
    std::vector<Complex> twiddles;
};
} // namespace plugin_dsp
