// File: dsp/include/plugin_dsp/StreamingStft.h
// Sample-by-sample STFT -> spectral mask(s) -> overlap-add resynthesis (sqrt-Hann in and out).
//
// MultiOutputStft: one analysis, up to maxOutputs independently masked resyntheses (one per active
// drum type in the dual-resolution estimator, docs/02-algorithm.md D1).
// StreamingStft:   the single-output case.
//
// With a mask that leaves the spectrum untouched, an output is the input delayed by exactly
// latency() = fftSize - 1 samples. prepare() allocates; processSample() never does.
#pragma once

#include "Fft.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace plugin_dsp
{
class MultiOutputStft
{
public:
    using Complex = Fft::Complex;

    /** fftSize: power of two. hop must divide fftSize / 2 (constant overlap-add). Allocates. */
    void prepare (std::size_t fftSize, std::size_t hopSize, std::size_t maxOutputs)
    {
        assert (hopSize > 0 && (fftSize / 2) % hopSize == 0 && maxOutputs > 0);
        n = fftSize;
        hop = hopSize;
        outputs = maxOutputs;
        fft.prepare (n);
        window.resize (n);
        for (std::size_t i = 0; i < n; ++i)
            window[i] = static_cast<float> (std::sqrt (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846
                                                                                * static_cast<double> (i) / static_cast<double> (n))));
        olaScale = static_cast<float> (2.0 * static_cast<double> (hop) / static_cast<double> (n));   // 1 / sum(Hann)
        in.assign (n, 0.0f);
        acc.assign (n * outputs, 0.0f);
        analysis.assign (n, Complex {});
        work.assign (n, Complex {});
        reset();
    }

    void reset() noexcept
    {
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (acc.begin(), acc.end(), 0.0f);
        pos = hopCounter = frameIndex = 0;
    }

    std::size_t fftSize() const noexcept { return n; }
    std::size_t hopSize() const noexcept { return hop; }
    std::size_t numBins() const noexcept { return n / 2 + 1; }
    std::size_t maxOutputs() const noexcept { return outputs; }
    int latency() const noexcept { return static_cast<int> (n) - 1; }

    /** Push one input sample; writes one sample per output to out[0 .. activeOutputs-1].
        Every hop samples: analyse(const Complex* bins, numBins, frameIndex) is called once with the
        unmasked spectrum, then mask(output, Complex* bins, numBins, frameIndex) once per active
        output on its own copy. The frame covers the last N inputs (centred N/2 samples ago). */
    template <typename AnalyseFn, typename MaskFn>
    void processSample (float x, float* out, std::size_t activeOutputs, AnalyseFn&& analyse, MaskFn&& mask) noexcept
    {
        assert (activeOutputs <= outputs);
        in[pos] = x;

        if (++hopCounter == hop)
        {
            hopCounter = 0;
            runFrame (activeOutputs, analyse, mask);
        }

        const std::size_t outPos = (pos + 1) % n;   // slot of time n - N + 1, now complete
        for (std::size_t o = 0; o < activeOutputs; ++o)
        {
            float& slot = acc[o * n + outPos];
            out[o] = slot * olaScale;
            slot = 0.0f;
        }
        for (std::size_t o = activeOutputs; o < outputs; ++o) acc[o * n + outPos] = 0.0f;
        pos = outPos;
    }

private:
    template <typename AnalyseFn, typename MaskFn>
    void runFrame (std::size_t activeOutputs, AnalyseFn& analyse, MaskFn& mask) noexcept
    {
        for (std::size_t i = 0; i < n; ++i)
            analysis[i] = Complex (in[(pos + 1 + i) % n] * window[i], 0.0f);
        fft.forward (analysis.data());
        analyse (static_cast<const Complex*> (analysis.data()), numBins(), frameIndex);

        for (std::size_t o = 0; o < activeOutputs; ++o)
        {
            std::copy (analysis.begin(), analysis.end(), work.begin());
            mask (o, work.data(), numBins(), frameIndex);
            for (std::size_t k = 1; k < n / 2; ++k) work[n - k] = std::conj (work[k]);   // Hermitian
            work[0] = Complex (work[0].real(), 0.0f);
            work[n / 2] = Complex (work[n / 2].real(), 0.0f);
            fft.inverse (work.data());
            float* a = acc.data() + o * n;
            for (std::size_t i = 0; i < n; ++i) a[(pos + 1 + i) % n] += work[i].real() * window[i];
        }
        ++frameIndex;
    }

    Fft fft;
    std::size_t n = 0, hop = 0, outputs = 0, pos = 0, hopCounter = 0, frameIndex = 0;
    float olaScale = 1.0f;
    std::vector<float> window, in, acc;
    std::vector<Complex> analysis, work;
};

class StreamingStft
{
public:
    using Complex = MultiOutputStft::Complex;

    void prepare (std::size_t fftSize, std::size_t hopSize) { impl.prepare (fftSize, hopSize, 1); }
    void reset() noexcept { impl.reset(); }
    std::size_t fftSize() const noexcept { return impl.fftSize(); }
    std::size_t hopSize() const noexcept { return impl.hopSize(); }
    std::size_t numBins() const noexcept { return impl.numBins(); }
    int latency() const noexcept { return impl.latency(); }

    /** mask (Complex* bins, std::size_t numBins, std::size_t frameIndex) may change bins 0..N/2. */
    template <typename MaskFn>
    float processSample (float x, MaskFn&& mask) noexcept
    {
        float y = 0.0f;
        impl.processSample (x, &y, 1, [] (const Complex*, std::size_t, std::size_t) {},
                            [&mask] (std::size_t, Complex* b, std::size_t nb, std::size_t f) { mask (b, nb, f); });
        return y;
    }

private:
    MultiOutputStft impl;
};
} // namespace plugin_dsp
