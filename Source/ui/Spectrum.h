// File: Source/ui/Spectrum.h
// The Puppet Show's live spectrum (UI thread only): the latest 4096 samples of the bus before and
// after the plugin, a Hann window, the dsp/ core's own FFT, then log-spaced points from 20 Hz to
// 20 kHz with a +4.5 dB/octave display tilt (so a drum bus doesn't read as "all bass") and a fast
// rise / slow fall. Allocates only in its constructor.
#pragma once

#include <plugin_dsp/Fft.h>

#include <array>
#include <cmath>
#include <vector>

namespace ui
{
class Spectrum
{
public:
    static constexpr std::size_t fftSize = 4096, halfSize = fftSize / 2, numPoints = 180;
    static constexpr float floorDb = -96.0f;

    Spectrum() : buffer (fftSize), window (fftSize)
    {
        fft.prepare (fftSize);
        float sum = 0.0f;
        for (std::size_t i = 0; i < fftSize; ++i)
        {
            window[i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * static_cast<float> (i) / static_cast<float> (fftSize - 1));
            sum += window[i];
        }
        norm = 2.0f / sum;
        before.fill (floorDb);
        after.fill (floorDb);
    }

    /** x of point i, 0..1 (log frequency 20 Hz .. 20 kHz). */
    static float position (std::size_t i) { return static_cast<float> (i) / static_cast<float> (numPoints - 1); }
    static float frequency (float pos01) { return 20.0f * std::pow (1000.0f, pos01); }

    /** Analyse new audio (fftSize samples each, oldest first). */
    void analyse (const float* in, const float* out, double sampleRate)
    {
        update (in, sampleRate, before);
        update (out, sampleRate, after);
    }

    /** Fill from a made-up spectrum (screenshot demo mode). */
    template <typename Fn>
    void synthesise (Fn&& dbAtFrequency, bool isAfter)
    {
        auto& dst = isAfter ? after : before;
        for (std::size_t i = 0; i < numPoints; ++i)
            dst[i] = dbAtFrequency (frequency (position (i)));
    }

    std::array<float, numPoints> before {}, after {};

private:
    void update (const float* x, double sr, std::array<float, numPoints>& dst)
    {
        for (std::size_t i = 0; i < fftSize; ++i)
            buffer[i] = plugin_dsp::Fft::Complex (x[i] * window[i], 0.0f);
        fft.forward (buffer.data());
        const float binHz = static_cast<float> (sr) / static_cast<float> (fftSize);
        auto binDb = [this] (std::size_t b) {
            const float m = std::abs (buffer[std::min (b, halfSize - 1)]) * norm;
            return 20.0f * std::log10 (std::max (m, 1.0e-6f));
        };
        for (std::size_t i = 0; i < numPoints; ++i)
        {
            const float f = frequency (position (i));
            const float lo = frequency (position (i) - 0.5f / static_cast<float> (numPoints - 1)) / binHz;
            const float hi = frequency (position (i) + 0.5f / static_cast<float> (numPoints - 1)) / binHz;
            float db;
            if (std::floor (hi) > std::ceil (lo))   // several bins: the loudest of them
            {
                db = floorDb;
                for (auto b = static_cast<std::size_t> (std::ceil (lo)); b <= static_cast<std::size_t> (std::floor (hi)) && b < halfSize; ++b)
                    db = std::max (db, binDb (b));
            }
            else   // between two bins (the low end): interpolate in dB
            {
                const float pos = f / binHz;
                const auto b0 = static_cast<std::size_t> (pos);
                const float t = pos - static_cast<float> (b0);
                db = binDb (b0) * (1.0f - t) + binDb (b0 + 1) * t;
            }
            db += 4.5f * std::log2 (f / 1000.0f);   // display tilt
            db = std::max (db, floorDb);
            float& v = dst[i];
            v += (db - v) * (db > v ? 0.7f : 0.16f);   // fast rise, slow fall (at 30 fps)
        }
    }

    plugin_dsp::Fft fft;
    std::vector<plugin_dsp::Fft::Complex> buffer;
    std::vector<float> window;
    float norm = 1.0f;
};
} // namespace ui
