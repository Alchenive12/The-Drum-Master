// File: dsp/include/plugin_dsp/GainMath.h
// Example of a pure DSP header: standard C++ only, no JUCE, no allocation.
// Replace or delete once the real DSP core exists.
#pragma once

#include <cmath>

namespace plugin_dsp
{
inline float dbToGain (float db) noexcept
{
    return db <= -120.0f ? 0.0f : std::pow (10.0f, db / 20.0f);
}

inline float gainToDb (float gain) noexcept
{
    return gain <= 1.0e-6f ? -120.0f : 20.0f * std::log10 (gain);
}
} // namespace plugin_dsp
