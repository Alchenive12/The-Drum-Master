// File: dsp/tests/synth_drums.h
// Deterministic synthetic drum kit + loop builder for dsp/ tests (docs/02-algorithm.md §9).
// Uses its own RNG and filters so every platform (GCC, Clang, MSVC) renders identical audio;
// std:: distributions are implementation-defined and would differ between CI runners.
// Test support only; not part of the plugin.
#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace synth
{
enum class Drum { kick, snare, closedHat, openHat, crash, ride, rimshot, clap, tom, count };
constexpr std::size_t numDrums = static_cast<std::size_t> (Drum::count);

inline const char* name (Drum d)
{
    constexpr std::array<const char*, numDrums> n { "kick", "snare", "closed hat", "open hat", "crash",
                                                    "ride", "rimshot", "clap", "tom" };
    return n[static_cast<std::size_t> (d)];
}

/** xorshift32, uniform in [-1, 1). Same sequence everywhere. */
struct Rng
{
    std::uint32_t s;
    explicit Rng (std::uint32_t seed) : s (seed ? seed : 0x9E3779B9u) {}
    double next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return static_cast<double> (s) / 2147483648.0 - 1.0;
    }
};

/** One-pole high-pass / low-pass cascades for shaping noise. */
struct Shaper
{
    double hpA = 0.0, lpA = 0.0;
    int hpPoles = 0, lpPoles = 0;
    std::array<double, 4> hx {}, hy {}, ly {};

    Shaper (double sr, double hpHz, int hpOrder, double lpHz, int lpOrder)
        : hpA (hpHz > 0 ? std::exp (-2.0 * 3.14159265358979323846 * hpHz / sr) : 0.0),
          lpA (lpHz > 0 ? std::exp (-2.0 * 3.14159265358979323846 * lpHz / sr) : 0.0),
          hpPoles (hpHz > 0 ? hpOrder : 0), lpPoles (lpHz > 0 ? lpOrder : 0) {}

    double process (double x)
    {
        for (std::size_t p = 0; p < static_cast<std::size_t> (hpPoles); ++p)
        {
            const double y = hpA * (hy[p] + x - hx[p]);
            hx[p] = x; hy[p] = y; x = y;
        }
        for (std::size_t p = 0; p < static_cast<std::size_t> (lpPoles); ++p)
        {
            ly[p] = lpA * ly[p] + (1.0 - lpA) * x;
            x = ly[p];
        }
        return x;
    }
};

/** Render one hit into out at `start`. Its peak is normalised to `gain`, so levels in a loop are what
    the loop says (the noise shapers lose a lot of level, especially for hats). */
inline void renderHit (std::vector<float>& out, std::size_t start, Drum d, double sr, float gain, std::uint32_t seed)
{
    constexpr double pi = 3.14159265358979323846;
    Rng rng (seed);
    std::vector<double> hit;
    // Every hit ends with a 5 ms fade, so cutting the render never creates a click (a real onset).
    auto add = [&] (double seconds, auto&& f) {
        const std::size_t n = static_cast<std::size_t> (seconds * sr), fade = static_cast<std::size_t> (0.005 * sr);
        hit.assign (n, 0.0);
        for (std::size_t i = 0; i < n; ++i)
        {
            const double tail = i + fade > n ? static_cast<double> (n - i) / static_cast<double> (fade) : 1.0;
            hit[i] = f (static_cast<double> (i) / sr) * tail;
        }
    };

    switch (d)
    {
        case Drum::kick:
            add (0.5, [&] (double t) {
                const double ph = 2 * pi * (48.0 * t + 112.0 * 0.035 * (1 - std::exp (-t / 0.035)));   // 160 -> 48 Hz
                return 0.75 * std::sin (ph) * std::exp (-t / 0.2) + 0.2 * std::sin (2 * pi * 118 * t) * std::exp (-t / 0.05)
                     + 0.25 * std::sin (2 * pi * 3400 * t) * std::exp (-t / 0.003);
            });
            break;
        case Drum::snare:
        {
            Shaper sh (sr, 1000, 2, 0, 0);
            add (0.45, [&] (double t) {
                return 0.30 * std::sin (2 * pi * 185 * t) * std::exp (-t / 0.06) + 0.18 * std::sin (2 * pi * 330 * t) * std::exp (-t / 0.04)
                     + 0.12 * std::sin (2 * pi * 450 * t) * std::exp (-t / 0.03) + 0.5 * sh.process (rng.next()) * std::exp (-t / 0.12);
            });
            break;
        }
        case Drum::closedHat:
        {
            Shaper sh (sr, 7000, 4, 0, 0);
            add (0.12, [&] (double t) { return 0.9 * sh.process (rng.next()) * std::exp (-t / 0.02); });
            break;
        }
        case Drum::openHat:
        {
            Shaper sh (sr, 7000, 4, 0, 0);
            add (0.7, [&] (double t) { return 0.8 * sh.process (rng.next()) * std::exp (-t / 0.25); });
            break;
        }
        case Drum::crash:
        {
            Shaper sh (sr, 2500, 2, 0, 0);
            add (2.5, [&] (double t) { return 0.7 * sh.process (rng.next()) * std::exp (-t / 0.9) * std::fmin (1.0, t / 0.002); });
            break;
        }
        case Drum::ride:
        {
            Shaper sh (sr, 6000, 3, 0, 0);
            add (1.5, [&] (double t) {
                double p = 0.0;
                for (double f : { 2830.0, 3910.0, 5170.0, 6340.0 }) p += std::sin (2 * pi * f * t);
                return 0.12 * p * std::exp (-t / 0.6) + 0.5 * sh.process (rng.next()) * (std::exp (-t / 0.03) + 0.2 * std::exp (-t / 0.8));
            });
            break;
        }
        case Drum::rimshot:
            add (0.15, [&] (double t) {
                return 0.5 * std::sin (2 * pi * 1900 * t) * std::exp (-t / 0.025) + 0.3 * std::sin (2 * pi * 2700 * t) * std::exp (-t / 0.018)
                     + 0.4 * rng.next() * std::exp (-t / 0.002);
            });
            break;
        case Drum::clap:
        {
            Shaper sh (sr, 800, 2, 4000, 2);
            add (0.3, [&] (double t) {
                double e = 0.5 * std::exp (-t / 0.07);
                for (int k = 0; k < 3; ++k)
                {
                    const double tk = t - 0.007 * k;
                    if (tk >= 0) e += std::exp (-tk / 0.004);
                }
                return 2.0 * sh.process (rng.next()) * e;
            });
            break;
        }
        case Drum::tom:
            add (0.5, [&] (double t) {
                const double ph = 2 * pi * (100.0 * t + 40.0 * 0.05 * (1 - std::exp (-t / 0.05)));
                return 0.7 * std::sin (ph) * std::exp (-t / 0.3);
            });
            break;
        case Drum::count: break;
    }

    double peak = 1e-12;
    for (double v : hit) peak = std::fmax (peak, std::fabs (v));
    const double scale = static_cast<double> (gain) / peak;
    for (std::size_t i = 0; i < hit.size() && start + i < out.size(); ++i)
        out[start + i] += static_cast<float> (hit[i] * scale);
}

struct Hit
{
    Drum drum;
    std::size_t sample;   // true onset
    float gain;
};

/** Mix a list of hits into a mono buffer. Seeds depend on the hit index, so renders repeat exactly. */
inline std::vector<float> render (const std::vector<Hit>& hits, double sr, std::size_t length)
{
    std::vector<float> out (length, 0.0f);
    for (std::size_t i = 0; i < hits.size(); ++i)
        renderHit (out, hits[i].sample, hits[i].drum, sr, hits[i].gain, static_cast<std::uint32_t> (1000 + 7919 * i));
    return out;
}

/** A 120 bpm, 16th-note grid loop with overlaps: kick + hats together, clap layered on the snare,
    crash on bar starts, an open hat and a rimshot per bar. Gains vary deterministically. */
inline std::vector<Hit> denseLoop (double sr, int bars, std::size_t offset)
{
    std::vector<Hit> h;
    Rng rng (42);
    const double step = 60.0 / 120.0 / 4.0;
    auto at = [&] (int bar, int s16) { return offset + static_cast<std::size_t> ((bar * 16 + s16) * step * sr); };
    auto g = [&] (double lo, double hi) { return static_cast<float> (lo + (hi - lo) * 0.5 * (rng.next() + 1.0)); };
    for (int bar = 0; bar < bars; ++bar)
    {
        for (int s : { 0, 6, 10 }) h.push_back ({ Drum::kick, at (bar, s), g (0.7, 1.0) });
        for (int s : { 4, 12 }) h.push_back ({ Drum::snare, at (bar, s), g (0.6, 0.9) });
        h.push_back ({ Drum::clap, at (bar, 12), g (0.3, 0.5) });
        for (int s = 0; s < 16; s += 2)
            if (s != 14) h.push_back ({ Drum::closedHat, at (bar, s), g (0.2, 0.4) });
        h.push_back ({ Drum::openHat, at (bar, 14), g (0.25, 0.4) });
        if (bar % 2 == 0) h.push_back ({ Drum::crash, at (bar, 0), g (0.3, 0.5) });
        h.push_back ({ Drum::rimshot, at (bar, 7), g (0.3, 0.5) });
    }
    return h;
}
} // namespace synth
