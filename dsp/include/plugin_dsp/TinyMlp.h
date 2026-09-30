// File: dsp/include/plugin_dsp/TinyMlp.h
// Minimal multi-layer perceptron for inference: standardise -> dense + ReLU -> dense + sigmoid.
// Weights live in a generated header (research/classifier-train/train.py). No allocation after
// construction, no dependencies. Multi-label: one independent probability per output.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace plugin_dsp
{
template <std::size_t In, std::size_t Hidden, std::size_t Out>
struct TinyMlpWeights
{
    std::array<float, In> mean, scale;             // x' = (x - mean) * scale
    std::array<float, In * Hidden> w1;             // row-major [in][hidden]
    std::array<float, Hidden> b1;
    std::array<float, Hidden * Out> w2;            // row-major [hidden][out]
    std::array<float, Out> b2;
};

template <std::size_t In, std::size_t Hidden, std::size_t Out>
class TinyMlp
{
public:
    using Weights = TinyMlpWeights<In, Hidden, Out>;

    explicit TinyMlp (const Weights& w) noexcept : weights (w) {}

    /** probabilities[o] in 0..1 for each output. */
    void run (const std::array<float, In>& x, std::array<float, Out>& probabilities) const noexcept
    {
        std::array<float, Hidden> h = weights.b1;
        for (std::size_t i = 0; i < In; ++i)
        {
            const float v = (x[i] - weights.mean[i]) * weights.scale[i];
            const float* row = weights.w1.data() + i * Hidden;
            for (std::size_t j = 0; j < Hidden; ++j) h[j] += v * row[j];
        }
        for (auto& v : h) v = std::max (0.0f, v);
        for (std::size_t o = 0; o < Out; ++o)
        {
            float z = weights.b2[o];
            for (std::size_t j = 0; j < Hidden; ++j) z += h[j] * weights.w2[j * Out + o];
            probabilities[o] = 1.0f / (1.0f + std::exp (-std::clamp (z, -30.0f, 30.0f)));
        }
    }

private:
    const Weights& weights;
};
} // namespace plugin_dsp
