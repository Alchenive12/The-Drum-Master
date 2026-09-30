// File: dsp/tests/dsp_test.h
// Tiny dependency-free test helpers for the pure DSP core.
// Each test file has its own main() and ends with DSP_TEST_END.
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

namespace dsptest
{
inline int& failures() { static int f = 0; return f; }

constexpr double pi = 3.14159265358979323846;

/** Sine wave, amplitude 1. */
inline std::vector<float> sine (double freqHz, double sampleRate, std::size_t numSamples)
{
    std::vector<float> out (numSamples);
    for (std::size_t i = 0; i < numSamples; ++i)
        out[i] = static_cast<float> (std::sin (2.0 * pi * freqHz * static_cast<double> (i) / sampleRate));
    return out;
}

/** Unit impulse at sample 0. */
inline std::vector<float> impulse (std::size_t numSamples)
{
    std::vector<float> out (numSamples, 0.0f);
    if (numSamples > 0) out[0] = 1.0f;
    return out;
}

inline bool allFinite (const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite (v)) return false;
    return true;
}

inline double rms (const std::vector<float>& x, std::size_t skip = 0)
{
    if (x.size() <= skip) return 0.0;
    double sum = 0.0;
    for (std::size_t i = skip; i < x.size(); ++i)
        sum += static_cast<double> (x[i]) * static_cast<double> (x[i]);
    return std::sqrt (sum / static_cast<double> (x.size() - skip));
}

inline double toDb (double linear) { return 20.0 * std::log10 (linear > 1e-12 ? linear : 1e-12); }

/** Magnitude of one frequency in a signal (Goertzel), normalised so a unit sine reads ~1.0.
    Use on a steady-state section (skip the start) for filter/EQ response tests. */
inline double magnitudeAt (const std::vector<float>& x, double freqHz, double sampleRate, std::size_t skip = 0)
{
    if (x.size() <= skip) return 0.0;
    const double w = 2.0 * pi * freqHz / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (std::size_t i = skip; i < x.size(); ++i)
    {
        const double s0 = static_cast<double> (x[i]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    const double n = static_cast<double> (x.size() - skip);
    return 2.0 * std::sqrt (power > 0.0 ? power : 0.0) / n;
}
} // namespace dsptest

#define DSP_CHECK(cond)                                                                 \
    do {                                                                                \
        if (! (cond)) {                                                                 \
            std::fprintf (stderr, "%s:%d  CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++dsptest::failures();                                                      \
        }                                                                               \
    } while (0)

#define DSP_CHECK_NEAR(actual, expected, tol)                                           \
    do {                                                                                \
        const double a_ = static_cast<double> (actual), e_ = static_cast<double> (expected); \
        if (! (std::fabs (a_ - e_) <= static_cast<double> (tol))) {                     \
            std::fprintf (stderr, "%s:%d  NEAR failed: %s = %.9g, expected %.9g (tol %.3g)\n", \
                          __FILE__, __LINE__, #actual, a_, e_, static_cast<double> (tol)); \
            ++dsptest::failures();                                                      \
        }                                                                               \
    } while (0)

#define DSP_TEST_END                                                                    \
    do {                                                                                \
        if (dsptest::failures() == 0) { std::printf ("PASS %s\n", __FILE__); return 0; } \
        std::fprintf (stderr, "FAIL %s: %d check(s)\n", __FILE__, dsptest::failures()); \
        return 1;                                                                       \
    } while (0)
