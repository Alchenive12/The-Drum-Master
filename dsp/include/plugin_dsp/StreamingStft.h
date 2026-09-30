// File: dsp/include/plugin_dsp/StreamingStft.h
// Sample-by-sample STFT -> spectral mask -> overlap-add resynthesis (sqrt-Hann in and out).
// With a mask that leaves the spectrum untouched, the output is the input delayed by exactly
// latency() = fftSize - 1 samples. prepare() allocates; processSample() never does.
// Used by the dual-resolution drum estimator (docs/02-algorithm.md, D1).
#pragma once

#include "Fft.h"

#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace plugin_dsp
{
class StreamingStft
{
public:
    using Complex = Fft::Complex;

    /** fftSize: power of two. hop must divide fftSize / 2 (constant overlap-add). Allocates. */
    void prepare (std::size_t fftSize, std::size_t hopSize)
    {
        assert (hopSize > 0 && (fftSize / 2) % hopSize == 0);
        n = fftSize;
        hop = hopSize;
        fft.prepare (n);
        window.resize (n);
        for (std::size_t i = 0; i < n; ++i)
            window[i] = static_cast<float> (std::sqrt (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846
                                                                                * static_cast<double> (i) / static_cast<double> (n))));
        // sum over frames of sqrtHann^2 = sum of periodic Hann = N / (2 hop)
        olaScale = static_cast<float> (2.0 * static_cast<double> (hop) / static_cast<double> (n));
        in.assign (n, 0.0f);
        acc.assign (n, 0.0f);
        spectrum.assign (n, Complex {});
        reset();
    }

    void reset() noexcept
    {
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (acc.begin(), acc.end(), 0.0f);
        pos = 0;
        hopCounter = 0;
        frameIndex = 0;
    }

    std::size_t fftSize() const noexcept { return n; }
    std::size_t hopSize() const noexcept { return hop; }
    std::size_t numBins() const noexcept { return n / 2 + 1; }
    int latency() const noexcept { return static_cast<int> (n) - 1; }

    /** Push one input sample, get one output sample (delayed by latency()).
        Every hop samples, mask (Complex* bins, std::size_t numBins, std::size_t frameIndex) is called
        with bins 0..N/2 of the newest frame and may change them in place. The frame covers the last
        N input samples, i.e. it is centred N/2 samples before the current input sample. */
    template <typename MaskFn>
    float processSample (float x, MaskFn&& mask) noexcept
    {
        in[pos] = x;

        if (++hopCounter == hop)
        {
            hopCounter = 0;
            runFrame (mask);
        }

        // The oldest slot (time n - N + 1) is now complete: read it and clear it for reuse.
        const std::size_t outPos = (pos + 1) % n;
        const float y = acc[outPos] * olaScale;
        acc[outPos] = 0.0f;
        pos = outPos;
        return y;
    }

private:
    template <typename MaskFn>
    void runFrame (MaskFn&& mask) noexcept
    {
        // Frame = last N samples, oldest first. The oldest is at pos + 1.
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::size_t p = (pos + 1 + i) % n;
            spectrum[i] = Complex (in[p] * window[i], 0.0f);
        }
        fft.forward (spectrum.data());
        mask (spectrum.data(), numBins(), frameIndex++);
        for (std::size_t k = 1; k < n / 2; ++k)          // keep the spectrum Hermitian
            spectrum[n - k] = std::conj (spectrum[k]);
        spectrum[0] = Complex (spectrum[0].real(), 0.0f);
        spectrum[n / 2] = Complex (spectrum[n / 2].real(), 0.0f);
        fft.inverse (spectrum.data());
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::size_t p = (pos + 1 + i) % n;
            acc[p] += spectrum[i].real() * window[i];
        }
    }

    Fft fft;
    std::size_t n = 0, hop = 0, pos = 0, hopCounter = 0, frameIndex = 0;
    float olaScale = 1.0f;
    std::vector<float> window, in, acc;
    std::vector<Complex> spectrum;
};
} // namespace plugin_dsp
