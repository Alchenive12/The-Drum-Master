// File: dsp/include/plugin_dsp/DualResEstimator.h
// Dual-resolution soft-mask estimator (docs/02-algorithm.md D1, research/engine-shootout "Cd1k"):
//   input -> LinearPhaseSplit at 1.5 kHz
//         -> low band:  STFT N = 1024 (2048 at >= 88.2 kHz), hop N/8   (frequency detail for kick/bass)
//         -> high band: STFT N = 256  (512  at >= 88.2 kHz), hop N/4   (time detail for clicks/hats)
//   For every active output (one per active drum type) each bin is multiplied by that type's share
//   (0..1, from the caller), resynthesised, and the two bands are summed and time-aligned.
// Shares that sum to 1 over all outputs give estimates that sum to the input delayed by latency().
// prepare() allocates; processSample() never does.
#pragma once

#include "LinearPhaseSplit.h"
#include "StreamingStft.h"

#include <cstddef>
#include <vector>

namespace plugin_dsp
{
class DualResEstimator
{
public:
    using Complex = MultiOutputStft::Complex;
    enum class Band { low, high };

    static constexpr double crossoverHz = 1500.0;

    void prepare (double sampleRate, std::size_t maxOutputs)
    {
        sr = sampleRate;
        const bool highRate = sampleRate >= 88000.0;
        const std::size_t nLo = highRate ? 2048 : 1024, nHi = highRate ? 512 : 256;
        split.prepare (sampleRate, crossoverHz);
        lo.prepare (nLo, nLo / 8, maxOutputs);
        hi.prepare (nHi, nHi / 4, maxOutputs);
        hiAlign = static_cast<std::size_t> (lo.latency() - hi.latency());
        hiDelay.assign ((hiAlign + 1) * maxOutputs, 0.0f);
        loOut.assign (maxOutputs, 0.0f);
        hiOut.assign (maxOutputs, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        split.reset();
        lo.reset();
        hi.reset();
        std::fill (hiDelay.begin(), hiDelay.end(), 0.0f);
        delayPos = 0;
    }

    /** Total delay of the estimates relative to the input, in samples (~24 ms at 48 kHz). */
    int latency() const noexcept { return split.latency() + lo.latency(); }
    std::size_t maxOutputs() const noexcept { return lo.maxOutputs(); }
    double binHz (Band b) const noexcept
    {
        return sr / static_cast<double> (b == Band::low ? lo.fftSize() : hi.fftSize());
    }

    /** share (Band, output, bin, frameIndex) -> float in 0..1. Called per active output, bin and frame.
        out[0 .. activeOutputs-1] receives one sample of each estimate. */
    template <typename ShareFn>
    void processSample (float x, float* out, std::size_t activeOutputs, ShareFn&& share) noexcept
    {
        float high = 0.0f;
        const float low = split.processSample (x, high);

        auto maskFor = [&share] (Band band) {
            return [&share, band] (std::size_t o, Complex* bins, std::size_t nb, std::size_t frame) {
                for (std::size_t k = 0; k < nb; ++k) bins[k] *= share (band, o, k, frame);
            };
        };
        auto noAnalysis = [] (const Complex*, std::size_t, std::size_t) {};
        lo.processSample (low, loOut.data(), activeOutputs, noAnalysis, maskFor (Band::low));
        hi.processSample (high, hiOut.data(), activeOutputs, noAnalysis, maskFor (Band::high));

        // Delay the (shorter-latency) high band so both bands line up.
        const std::size_t len = hiAlign + 1;
        for (std::size_t o = 0; o < activeOutputs; ++o)
        {
            float* d = hiDelay.data() + o * len;
            d[delayPos] = hiOut[o];
            out[o] = loOut[o] + d[(delayPos + 1) % len];
        }
        for (std::size_t o = activeOutputs; o < lo.maxOutputs(); ++o) hiDelay[o * len + delayPos] = 0.0f;
        delayPos = (delayPos + 1) % len;
    }

private:
    double sr = 48000.0;
    LinearPhaseSplit split;
    MultiOutputStft lo, hi;
    std::size_t hiAlign = 0, delayPos = 0;
    std::vector<float> hiDelay, loOut, hiOut;
};
} // namespace plugin_dsp
