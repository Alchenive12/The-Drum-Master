// File: dsp/include/plugin_dsp/HitFeatures.h
// Streaming log-mel analysis that turns an onset into a fixed feature vector for the learned
// hit classifier (docs/03-decisions-for-review.md, D-C1).
//
//   mono input -> Hann frames (N = 512 at 44.1-48 kHz, 1024 at >= 88.2 kHz; hop N/8 ~ 1.3 ms)
//   -> 32 mel bands 30 Hz .. 16 kHz, dB, kept in a ring of recent frames
//   -> for an onset o: mean band level of the frames that END inside each segment
//        pre (-25, 0] ms, then (0,5] (5,10] (10,15] (15,20] (20,30] (30,40] ms
//      = 7 x 32 values, minus the loudest post-onset value (level-independent),
//      + that loudest value itself (absolute level, dB) = 225 features.
// The segments stop at 40 ms: the measured detection knee (research/lookahead-test), inside the
// 50 ms lookahead. prepare() allocates; pushSample() and extract() never do.
#pragma once

#include "Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace plugin_dsp
{
class HitFeatures
{
public:
    static constexpr std::size_t numMels = 32;
    static constexpr std::size_t numSegments = 7;
    static constexpr std::size_t numFeatures = numMels * numSegments + 1;
    static constexpr std::array<double, numSegments + 1> segmentEdgesMs { -25, 0, 5, 10, 15, 20, 30, 40 };

    using Vector = std::array<float, numFeatures>;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fftSize = sampleRate >= 88000.0 ? 1024 : 512;
        hop = fftSize / 8;
        fft.prepare (fftSize);
        frame.assign (fftSize, {});
        window.resize (fftSize);
        for (std::size_t i = 0; i < fftSize; ++i)
            window[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * static_cast<double> (i) / static_cast<double> (fftSize)));

        // Triangular mel filterbank, 30 Hz .. min(16 kHz, 0.45 sr).
        auto hzToMel = [] (double f) { return 2595.0 * std::log10 (1.0 + f / 700.0); };
        auto melToHz = [] (double m) { return 700.0 * (std::pow (10.0, m / 2595.0) - 1.0); };
        const double lo = hzToMel (30.0), hi = hzToMel (std::min (16000.0, 0.45 * sampleRate));
        const std::size_t bins = fftSize / 2 + 1;
        melWeights.assign (numMels * bins, 0.0f);
        for (std::size_t m = 0; m < numMels; ++m)
        {
            const double l = melToHz (lo + (hi - lo) * static_cast<double> (m) / (numMels + 1));
            const double c = melToHz (lo + (hi - lo) * static_cast<double> (m + 1) / (numMels + 1));
            const double r = melToHz (lo + (hi - lo) * static_cast<double> (m + 2) / (numMels + 1));
            for (std::size_t k = 0; k < bins; ++k)
            {
                const double f = static_cast<double> (k) * sampleRate / static_cast<double> (fftSize);
                const double w = std::max (0.0, std::min ((f - l) / (c - l), (r - f) / (r - c)));
                melWeights[m * bins + k] = static_cast<float> (w);
            }
        }
        double sumW = 0.0;
        for (float w : window) sumW += static_cast<double> (w);
        norm = static_cast<float> (4.0 / (sumW * sumW));

        std::size_t ring = 1;
        while (ring < fftSize * 2) ring <<= 1;
        raw.assign (ring, 0.0f);
        rawMask = ring - 1;
        // Enough frames for 25 ms before + 40 ms after an onset + the reporting delay, with margin.
        frameCapacity = static_cast<std::size_t> (std::ceil (0.25 * sr / static_cast<double> (hop))) + 8;
        frames.assign (frameCapacity * numMels, -120.0f);
        frameEnds.assign (frameCapacity, std::numeric_limits<std::int64_t>::min());
        reset();
    }

    void reset() noexcept
    {
        std::fill (raw.begin(), raw.end(), 0.0f);
        std::fill (frames.begin(), frames.end(), -120.0f);
        std::fill (frameEnds.begin(), frameEnds.end(), std::numeric_limits<std::int64_t>::min());
        sampleCount = 0;
        hopCounter = 0;
        framePos = 0;
    }

    /** Samples needed after an onset before extract() can run (40 ms). */
    std::int64_t samplesAfterOnset() const noexcept
    {
        return static_cast<std::int64_t> (std::ceil (segmentEdgesMs.back() * sr / 1000.0));
    }

    std::int64_t samplesSeen() const noexcept { return sampleCount; }

    void pushSample (float x) noexcept
    {
        raw[static_cast<std::size_t> (sampleCount) & rawMask] = x;
        if (++hopCounter == hop)
        {
            hopCounter = 0;
            analyse();
        }
        ++sampleCount;
    }

    /** Feature vector of the onset at input index `onset`. Needs samplesSeen() > onset + samplesAfterOnset(). */
    Vector extract (std::int64_t onset) const noexcept
    {
        std::array<std::array<float, numMels>, numSegments> sum {};
        std::array<int, numSegments> count {};
        for (std::size_t f = 0; f < frameCapacity; ++f)
        {
            const std::int64_t end = frameEnds[f];
            if (end == std::numeric_limits<std::int64_t>::min()) continue;
            const double ms = 1000.0 * static_cast<double> (end - onset) / sr;
            for (std::size_t s = 0; s < numSegments; ++s)
                if (ms > segmentEdgesMs[s] && ms <= segmentEdgesMs[s + 1])
                {
                    for (std::size_t m = 0; m < numMels; ++m) sum[s][m] += frames[f * numMels + m];
                    ++count[s];
                    break;
                }
        }
        Vector v {};
        float peak = -120.0f;
        for (std::size_t s = 0; s < numSegments; ++s)
            for (std::size_t m = 0; m < numMels; ++m)
            {
                const float value = count[s] > 0 ? sum[s][m] / static_cast<float> (count[s]) : -120.0f;
                v[s * numMels + m] = value;
                if (s > 0) peak = std::max (peak, value);
            }
        for (std::size_t i = 0; i + 1 < numFeatures; ++i) v[i] = std::max (v[i] - peak, -80.0f);   // relative, floored
        v[numFeatures - 1] = peak;
        return v;
    }

private:
    void analyse() noexcept
    {
        const std::int64_t first = sampleCount - static_cast<std::int64_t> (fftSize) + 1;
        for (std::size_t i = 0; i < fftSize; ++i)
        {
            const std::int64_t p = first + static_cast<std::int64_t> (i);
            frame[i] = Fft::Complex (p >= 0 ? raw[static_cast<std::size_t> (p) & rawMask] * window[i] : 0.0f, 0.0f);
        }
        fft.forward (frame.data());
        const std::size_t bins = fftSize / 2 + 1;
        float* out = frames.data() + framePos * numMels;
        for (std::size_t m = 0; m < numMels; ++m)
        {
            float e = 0.0f;
            const float* w = melWeights.data() + m * bins;
            for (std::size_t k = 0; k < bins; ++k) e += w[k] * std::norm (frame[k]);
            out[m] = std::max (-120.0f, 10.0f * std::log10 (e * norm + 1e-12f));
        }
        frameEnds[framePos] = sampleCount;
        framePos = (framePos + 1) % frameCapacity;
    }

    double sr = 48000.0;
    std::size_t fftSize = 512, hop = 64, rawMask = 0, hopCounter = 0, framePos = 0, frameCapacity = 0;
    std::int64_t sampleCount = 0;
    float norm = 1.0f;
    Fft fft;
    std::vector<Fft::Complex> frame;
    std::vector<float> window, melWeights, raw, frames;
    std::vector<std::int64_t> frameEnds;
};
} // namespace plugin_dsp
