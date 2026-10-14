// File: dsp/include/plugin_dsp/OnsetDetector.h
// Multi-band onset detector (docs/02-algorithm.md §3).
//
//   mono input -> Hann-windowed FFT frames, N = 1024 / hop 128 at 44.1-48 kHz, 2048 / 256 at >= 88.2 kHz
//                 (Hann, not the resynthesis sqrt-Hann: far less leakage into empty bands)
//   -> per-bin level S[k] (dB) and 6 band levels L_b    B0 20-120 · B1 120-400 · B2 400-1.5k
//                                                       B3 1.5k-5k · B4 5k-10k · B5 10k-20k
//   -> SuperFlux per band (Boeck & Widmer 2013):
//        O_b = mean over the band's bins of max(0, S[k,n] - R[k]),
//        R[k] = max of S over frames n-mu .. n-mu-2 (mu ~ 10 ms) and bins k-w .. k+w.
//      The lag covers the ramp of the 21 ms window; the max over older frames ignores a band
//      recovering from a dip (two kick tones beating); the max over neighbour bins ignores energy
//      sliding sideways (a kick or 808 pitch drop moving from B1 into B0). B0/B1 use a longer
//      reference (>= 25 ms): a 21 ms window holds only ~1 cycle of a 48 Hz tone, so its level
//      wobbles a few dB from frame to frame with the phase.
//   -> the band and its neighbours together must also rise >= minLevelRiseDb over the same lag
//      (a kick or 808 pitch drop moves energy from B1 into B0 and concentrates it into fewer bins:
//      single bins and even B0 alone rise, B0 + B1 together don't; a real hit raises them all)
//   -> adaptive threshold T_b = alpha(s) * median(O_b, last 100 ms) + delta
//   -> peak pick (O > T, local max over +-1 frame), refractory 25 ms per band, level gates
//      (absolute floor, and within `relativeGateDb` of the loudest band, so leakage never triggers)
//   -> bands with onsets within +-6 ms form one event (multi-label: kick + hat = one event, two band groups);
//      a much weaker onset (< half the flux) within 15 ms after an event is part of that hit (a clap's
//      later bursts, a buzz), not a new event
//   -> the event time is refined to ~1 ms: steepest 1 ms energy rise inside the onset frame, on the
//      raw input for low-band events, on the 1st / 2nd difference (high emphasis) when the strongest
//      band is mid / high, so a quiet hat isn't timed by a loud kick tail.
//
// prepare() allocates; processSample() never does. An event is reported a bounded time after the
// real onset (maxReportDelay(), under 40 ms), inside the 50 ms lookahead.
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
struct OnsetEvent
{
    static constexpr std::size_t numBands = 6;
    std::int64_t sample = 0;                 // refined onset, as an index into the input stream
    std::uint32_t bandMask = 0;              // bit b set: band b had an onset in this event
    std::array<float, numBands> fluxDb {};   // flux of each participating band (0 otherwise)
    std::array<float, numBands> levelDb {};  // band levels at the first onset frame (all bands)
    std::uint32_t midiTypes = 0;             // MIDI mode: the drums the notes named (HitClassifier skips its network)
};

class OnsetDetector
{
public:
    static constexpr std::size_t numBands = OnsetEvent::numBands;

    struct Settings
    {
        float deltaDb = 2.0f;          // threshold floor above the scaled median (mean dB rise per bin)
        float levelFloorDb = -70.0f;   // ignore bands quieter than this (0 dB = full-scale sine)
        float relativeGateDb = 35.0f;  // ignore bands this far below the loudest band in the frame
        float minLevelRiseDb = 3.0f;   // band + neighbours must rise at least this much over the lag
        double lagSeconds = 0.010;     // flux = rise over this lag (mu frames)...
        std::size_t referenceSpan = 3; // ...against the loudest of this many frames from n - mu back
        double lowReferenceSeconds = 0.025;   // B0/B1: reference covers at least one 40 Hz cycle
        double medianSeconds = 0.100;
        double refractorySeconds = 0.025;
        double groupSeconds = 0.006;
        double tailMergeSeconds = 0.015;
    };

    void prepare (double sampleRate) { prepare (sampleRate, Settings {}); }

    void prepare (double sampleRate, const Settings& s)
    {
        sr = sampleRate;
        settings = s;
        const bool highRate = sampleRate >= 88000.0;
        fftSize = highRate ? 2048 : 1024;
        hop = highRate ? 256 : 128;
        numBins = fftSize / 2 + 1;
        fft.prepare (fftSize);
        frame.assign (fftSize, {});
        window.resize (fftSize);

        // Level normalisation: a full-scale sine reads ~0 dB.
        double sumW = 0.0;
        for (std::size_t i = 0; i < fftSize; ++i)
        {
            window[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * static_cast<double> (i) / static_cast<double> (fftSize)));
            sumW += static_cast<double> (window[i]);
        }
        levelNorm = static_cast<float> (4.0 / (sumW * sumW));

        const double binHz = sampleRate / static_cast<double> (fftSize);
        const std::array<double, numBands + 1> edges { 20.0, 120.0, 400.0, 1500.0, 5000.0, 10000.0, 20000.0 };
        for (std::size_t b = 0; b < numBands; ++b)
        {
            binLo[b] = std::max<std::size_t> (1, static_cast<std::size_t> (std::ceil (edges[b] / binHz)));
            binHi[b] = std::min (fftSize / 2, static_cast<std::size_t> (std::floor (edges[b + 1] / binHz)));
            if (binHi[b] < binLo[b]) binHi[b] = binLo[b];
        }
        spreadLowBins = static_cast<std::size_t> (std::ceil (400.0 / binHz));   // wider spread for pitch drops

        lagFrames = std::clamp<std::size_t> (static_cast<std::size_t> (std::lround (s.lagSeconds * sr / static_cast<double> (hop))), 1, 12);
        referenceSpan = std::clamp<std::size_t> (s.referenceSpan, 1, spectraFrames - lagFrames);
        lowReferenceSpan = std::clamp<std::size_t> (static_cast<std::size_t> (std::ceil (s.lowReferenceSeconds * sr / static_cast<double> (hop))),
                                                    referenceSpan, spectraFrames - lagFrames);
        spectra.assign (numBins * spectraFrames, 0.0f);
        bandLevels.assign (numBands * spectraFrames, 0.0f);
        binDb.assign (numBins, 0.0f);
        reference.assign (numBins, 0.0f);

        medianFrames = std::max<std::size_t> (5, static_cast<std::size_t> (std::lround (s.medianSeconds * sr / static_cast<double> (hop))));
        history.assign (numBands * medianFrames, 0.0f);
        scratch.assign (medianFrames, 0.0f);
        refractoryFrames = static_cast<std::size_t> (std::ceil (s.refractorySeconds * sr / static_cast<double> (hop)));
        groupSamples = static_cast<std::int64_t> (std::lround (s.groupSeconds * sr));
        tailMergeSamples = static_cast<std::int64_t> (std::lround (s.tailMergeSeconds * sr));

        std::size_t ring = 1;
        while (ring < 4 * fftSize) ring <<= 1;
        raw.assign (ring, 0.0f);
        rawMask = ring - 1;
        msSamples = std::max<std::size_t> (8, static_cast<std::size_t> (std::lround (sr / 1000.0)));
        env.assign (2 * fftSize / std::max<std::size_t> (1, msSamples / 4) + 32, 0.0f);
        reset();
    }

    void reset() noexcept
    {
        std::fill (history.begin(), history.end(), 0.0f);
        std::fill (raw.begin(), raw.end(), 0.0f);
        std::fill (spectra.begin(), spectra.end(), settings.levelFloorDb - 10.0f);
        std::fill (bandLevels.begin(), bandLevels.end(), settings.levelFloorDb - 10.0f);
        historyPos = historyCount = spectraPos = hopCounter = 0;
        sampleCount = 0;
        for (auto& b : bands) b = BandState {};
        pending = Pending {};
        readyCount = readyHead = 0;
        lastEventFrameEnd = std::numeric_limits<std::int64_t>::min() / 2;
        lastEventMaxFlux = 0.0f;
    }

    /** 0 = only strong onsets, 1 = everything; 0.5 default (docs/02-parameters.md global_sensitivity). */
    void setSensitivity (float s01) noexcept
    {
        const float s = std::clamp (s01, 0.0f, 1.0f);
        alpha = 2.4f - 1.2f * s;
    }

    /** Upper bound (samples) between a real onset and the moment its event is reported. */
    std::int64_t maxReportDelay() const noexcept
    {
        return static_cast<std::int64_t> (fftSize + 3 * hop) + groupSamples;
    }

    /** Push one mono sample. Returns true and fills `out` when an event is complete. */
    bool processSample (float x, OnsetEvent& out) noexcept
    {
        raw[static_cast<std::size_t> (sampleCount) & rawMask] = x;
        if (++hopCounter == hop)
        {
            hopCounter = 0;
            const std::int64_t first = sampleCount - static_cast<std::int64_t> (fftSize) + 1;   // oldest sample of the frame
            for (std::size_t i = 0; i < fftSize; ++i)
            {
                const std::int64_t p = first + static_cast<std::int64_t> (i);
                const float v = p >= 0 ? raw[static_cast<std::size_t> (p) & rawMask] : 0.0f;
                frame[i] = Fft::Complex (v * window[i], 0.0f);
            }
            fft.forward (frame.data());
            analyseFrame();
        }
        ++sampleCount;

        if (readyCount == 0) return false;
        out = ready[readyHead];
        readyHead = (readyHead + 1) % ready.size();
        --readyCount;
        return true;
    }

private:
    struct BandState
    {
        float prevFlux = 0.0f, prevPrevFlux = 0.0f, prevThreshold = 1e9f, prevLevel = -120.0f, prevGate = 0.0f;
        std::size_t framesSinceOnset = 1u << 20;
    };

    struct Pending
    {
        bool active = false;
        std::int64_t frameEnd = 0;
        OnsetEvent event;
    };

    void analyseFrame() noexcept
    {
        const std::int64_t frameEnd = sampleCount;   // index of the newest sample in this frame
        const float floorBin = settings.levelFloorDb - 10.0f;
        for (std::size_t k = 0; k < numBins; ++k)
            binDb[k] = std::max (floorBin, 10.0f * std::log10 (std::norm (frame[k]) * levelNorm + 1e-12f));

        // Reference: loudest of frames n-mu .. n-mu-span+1, spread over neighbour bins.
        std::fill (reference.begin(), reference.end(), floorBin);
        const std::size_t lowBinsEnd = binLo[2];   // bins of B0 and B1
        for (std::size_t f = 0; f < lowReferenceSpan; ++f)
        {
            const std::size_t age = lagFrames + f;   // frames back from n (1 = previous frame)
            const float* old = spectra.data() + ((spectraPos + spectraFrames - age) % spectraFrames) * numBins;
            const std::size_t kEnd = f < referenceSpan ? numBins : lowBinsEnd;
            for (std::size_t k = 0; k < kEnd; ++k)
            {
                const std::size_t w = k < spreadLowBins ? 2 : 1;
                const std::size_t lo = k >= w ? k - w : 0, hi = std::min (numBins - 1, k + w);
                float m = old[lo];
                for (std::size_t j = lo + 1; j <= hi; ++j) m = std::max (m, old[j]);
                reference[k] = std::max (reference[k], m);
            }
        }
        std::array<float, numBands> level {}, flux {}, levelRise {};
        for (std::size_t b = 0; b < numBands; ++b)
        {
            float e = 0.0f, rise = 0.0f;
            for (std::size_t k = binLo[b]; k <= binHi[b]; ++k)
            {
                e += std::norm (frame[k]);
                rise += std::max (0.0f, binDb[k] - reference[k]);
            }
            level[b] = 10.0f * std::log10 (e * levelNorm + 1e-12f);
            flux[b] = rise / static_cast<float> (binHi[b] - binLo[b] + 1);
        }
        // Neighbourhood rise: band b-1 .. b+1 summed, now vs the loudest reference frame.
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const std::size_t lo = b > 0 ? b - 1 : 0, hi = std::min (numBands - 1, b + 1);
            auto energyDb = [&] (const float* levels) {
                float e = 0.0f;
                for (std::size_t j = lo; j <= hi; ++j) e += std::pow (10.0f, levels[j] / 10.0f);
                return 10.0f * std::log10 (e + 1e-12f);
            };
            const float now = energyDb (level.data());
            float ref = settings.levelFloorDb - 10.0f;
            const std::size_t span = b <= 1 ? lowReferenceSpan : referenceSpan;
            for (std::size_t f = 0; f < span; ++f)
                ref = std::max (ref, energyDb (bandLevels.data() + ((spectraPos + spectraFrames - lagFrames - f) % spectraFrames) * numBands));
            levelRise[b] = now - ref;
            if (levelRise[b] < settings.minLevelRiseDb) flux[b] = 0.0f;   // bins rose, the neighbourhood didn't
        }
        std::copy (binDb.begin(), binDb.end(), spectra.begin() + static_cast<std::ptrdiff_t> (spectraPos * numBins));
        std::copy (level.begin(), level.end(), bandLevels.begin() + static_cast<std::ptrdiff_t> (spectraPos * numBands));
        spectraPos = (spectraPos + 1) % spectraFrames;
        const float loudest = *std::max_element (level.begin(), level.end());
        const float gate = std::max (settings.levelFloorDb, loudest - settings.relativeGateDb);

        for (std::size_t b = 0; b < numBands; ++b)
        {
            BandState& st = bands[b];
            const float threshold = alpha * median (b) + settings.deltaDb;

            // Frame n-1 is an onset if it beat its threshold, is a local maximum and is loud enough.
            ++st.framesSinceOnset;
            if (st.prevFlux > st.prevThreshold && st.prevFlux >= st.prevPrevFlux && st.prevFlux >= flux[b]
                && st.prevLevel > st.prevGate && st.framesSinceOnset > refractoryFrames)
            {
                st.framesSinceOnset = 0;
                addBandOnset (b, frameEnd - static_cast<std::int64_t> (hop), st.prevFlux, level);
            }

            st.prevPrevFlux = st.prevFlux;
            st.prevFlux = flux[b];
            st.prevThreshold = threshold;
            st.prevLevel = level[b];
            st.prevGate = gate;
            history[b * medianFrames + historyPos] = flux[b];
        }
        historyPos = (historyPos + 1) % medianFrames;
        historyCount = std::min (historyCount + 1, medianFrames);

        // Close the pending event once no more bands can join it.
        if (pending.active && frameEnd - pending.frameEnd > groupSamples + static_cast<std::int64_t> (hop))
            finalisePending();
    }

    float median (std::size_t b) noexcept
    {
        if (historyCount == 0) return 0.0f;
        const float* h = history.data() + b * medianFrames;
        std::copy (h, h + historyCount, scratch.begin());
        auto mid = scratch.begin() + static_cast<std::ptrdiff_t> (historyCount / 2);
        std::nth_element (scratch.begin(), mid, scratch.begin() + static_cast<std::ptrdiff_t> (historyCount));
        return *mid;
    }

    void addBandOnset (std::size_t b, std::int64_t frameEnd, float flux, const std::array<float, numBands>& level) noexcept
    {
        if (pending.active && std::llabs (frameEnd - pending.frameEnd) > groupSamples)
            finalisePending();
        if (! pending.active && frameEnd - lastEventFrameEnd <= tailMergeSamples && flux < 0.5f * lastEventMaxFlux)
            return;   // a weaker echo of the hit just reported (clap burst, buzz)
        if (! pending.active)
        {
            pending = Pending {};
            pending.active = true;
            pending.frameEnd = frameEnd;
            pending.event.levelDb = level;
        }
        pending.event.bandMask |= 1u << b;
        pending.event.fluxDb[b] = std::max (pending.event.fluxDb[b], flux);
    }

    void finalisePending() noexcept
    {
        std::size_t strongest = 0;
        for (std::size_t b = 1; b < numBands; ++b)
            if (pending.event.fluxDb[b] > pending.event.fluxDb[strongest]) strongest = b;
        const int emphasis = strongest >= 3 ? 2 : (strongest == 2 ? 1 : 0);
        pending.event.sample = refine (pending.frameEnd, emphasis);
        lastEventFrameEnd = pending.frameEnd;
        lastEventMaxFlux = pending.event.fluxDb[strongest];
        if (readyCount < ready.size())
        {
            ready[(readyHead + readyCount) % ready.size()] = pending.event;
            ++readyCount;
        }
        pending.active = false;
    }

    /** Steepest rise of a 1 ms energy window (0.25 ms steps) inside the onset frame
        (frameEnd - N .. frameEnd + hop). emphasis 0 = raw, 1 = 1st difference, 2 = 2nd difference. */
    std::int64_t refine (std::int64_t frameEnd, int emphasis) noexcept
    {
        const std::int64_t n = static_cast<std::int64_t> (fftSize), ms = static_cast<std::int64_t> (msSamples);
        const std::int64_t oldest = std::max<std::int64_t> (0, sampleCount - static_cast<std::int64_t> (raw.size()) + 1);
        const std::int64_t fallback = std::max<std::int64_t> (0, frameEnd - n / 2);
        const std::int64_t from = std::max (oldest + ms + 2, frameEnd - n);
        const std::int64_t to = std::min (sampleCount - ms, frameEnd + static_cast<std::int64_t> (hop));
        if (to <= from) return fallback;

        const std::int64_t stride = std::max<std::int64_t> (1, ms / 4);
        std::size_t count = 0;
        for (std::int64_t p = from - ms; p <= to && count < env.size(); p += stride, ++count)
        {
            float e = 0.0f;
            for (std::int64_t i = 0; i < ms; ++i)
            {
                const std::int64_t q = p + i;
                const float x0 = raw[static_cast<std::size_t> (q) & rawMask];
                const float x1 = raw[static_cast<std::size_t> (q - 1) & rawMask];
                const float x2 = raw[static_cast<std::size_t> (q - 2) & rawMask];
                const float v = emphasis == 0 ? x0 : (emphasis == 1 ? x0 - x1 : x0 - 2.0f * x1 + x2);
                e += v * v;
            }
            env[count] = 10.0f * std::log10 (e / static_cast<float> (ms) + 1e-12f);
        }
        const std::size_t lag = 4;   // 4 strides = 1 ms: compare with the window just before
        std::int64_t best = fallback;
        float bestRise = -1e9f;
        for (std::size_t i = lag; i < count; ++i)
        {
            const float rise = env[i] - env[i - lag];
            if (rise > bestRise)
            {
                bestRise = rise;
                best = from - ms + static_cast<std::int64_t> (i) * stride;
            }
        }
        return best;
    }

    static constexpr std::size_t spectraFrames = 16;

    double sr = 48000.0;
    Settings settings;
    float alpha = 1.8f, levelNorm = 1.0f;
    std::size_t fftSize = 1024, hop = 128, numBins = 513, hopCounter = 0;
    std::size_t lagFrames = 4, referenceSpan = 3, lowReferenceSpan = 10, spreadLowBins = 9, spectraPos = 0;
    std::size_t medianFrames = 37, historyPos = 0, historyCount = 0, refractoryFrames = 10;
    std::size_t msSamples = 48, rawMask = 0;
    std::int64_t groupSamples = 288, tailMergeSamples = 720, sampleCount = 0, lastEventFrameEnd = 0;
    float lastEventMaxFlux = 0.0f;
    Fft fft;
    std::vector<Fft::Complex> frame;
    std::vector<float> window, spectra, bandLevels, binDb, reference, history, scratch, raw, env;
    std::array<std::size_t, numBands> binLo {}, binHi {};
    std::array<BandState, numBands> bands {};
    Pending pending;
    std::array<OnsetEvent, 8> ready {};
    std::size_t readyCount = 0, readyHead = 0;
};
} // namespace plugin_dsp
