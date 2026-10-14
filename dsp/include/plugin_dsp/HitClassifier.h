// File: dsp/include/plugin_dsp/HitClassifier.h
// Labels each onset with the drum types it contains (docs/02-algorithm.md §4; decision D-C1 in
// docs/03-decisions-for-review.md).
//
//   onset o -> HitFeatures (32-band log-mel picture from o - 25 ms to o + 40 ms, 225 numbers)
//           -> TinyMlp (225 -> 48 ReLU -> 9 sigmoid, weights in HitClassifierWeights.h, trained on
//              the Groove MIDI + Expanded Groove MIDI datasets (CC BY 4.0) and our own synthetic loops)
//           -> one independent probability per type = its weight (multi-label: kick + hat is both)
//
// Then: types the user switched off (manual mode) get 0 and their probability moves to misc; the
// primary type is the most likely non-misc type, or misc when that is below the confidence cut
// (0.55 - 0.2 x sensitivity), so an uncertain hit stays neutral.
// Also kept per hit: the rise spectrum (13 bands, what the hit added over the frame before it) for
// the engine's per-drum shares and The Puppet Show, and the broadband peak of its first 10 ms.
// Auto detect: a type is present after >= 4 hits with mean confidence >= 0.65 spread over >= 1.5 s.
// A decision is made 40 ms after the onset (the measured knee, research/lookahead-test), inside the
// 50 ms lookahead. prepare() allocates; pushSample()/addEvent()/popClassified() never do.
#pragma once

#include "DrumTypes.h"
#include "Fft.h"
#include "HitClassifierWeights.h"
#include "HitFeatures.h"
#include "OnsetDetector.h"
#include "TinyMlp.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace plugin_dsp
{
struct ClassifiedHit
{
    static constexpr std::size_t numBands = 13;
    std::int64_t sample = 0;                           // onset (input stream index)
    std::array<float, numDrumTypes> weight {};         // probability that each type is in this hit (0..1;
                                                       // layered hits have several; misc = the unexplained rest)
    std::array<float, numBands> rise {};               // rise spectrum, mean power per bin (linear)
    DrumType primary = DrumType::misc;
    float confidence = 0.0f;                           // of the primary type
    float peakDb = -120.0f;                            // broadband peak level in the first 10 ms
};

class HitClassifier
{
public:
    static constexpr std::size_t numBands = ClassifiedHit::numBands;
    static constexpr std::array<double, numBands + 1> bandEdges { 30, 60, 100, 160, 250, 400, 630, 1000,
                                                                  1600, 2500, 4000, 6300, 10000, 16000 };
    using Network = TinyMlp<HitFeatures::numFeatures, hitClassifierHidden, numDrumTypes>;

    struct Settings
    {
        int presentMinHits = 4;
        float presentMinConfidence = 0.65f;
        double presentMinSpanSeconds = 1.5;
        float learnRate = 0.1f;   // long-term spectra, per confident hit
    };

    HitClassifier() : network (hitClassifierWeights()) {}

    void prepare (double sampleRate) { prepare (sampleRate, Settings {}); }

    void prepare (double sampleRate, const Settings& s)
    {
        sr = sampleRate;
        settings = s;
        features.prepare (sampleRate);
        windowSamples = features.samplesAfterOnset();
        msSamples = std::max<std::int64_t> (8, static_cast<std::int64_t> (std::lround (sr / 1000.0)));

        fftSize = sampleRate >= 88000.0 ? 2048 : 1024;
        fft.prepare (fftSize);
        frame.assign (fftSize, {});
        power.assign (fftSize / 2 + 1, 0.0f);
        window.resize (fftSize);
        for (std::size_t i = 0; i < fftSize; ++i)
            window[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (i) / static_cast<double> (fftSize)));
        const double binHz = sampleRate / static_cast<double> (fftSize);
        for (std::size_t b = 0; b < numBands; ++b)
        {
            binLo[b] = std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (bandEdges[b] / binHz)));
            binHi[b] = std::max (binLo[b], std::min (fftSize / 2, static_cast<std::size_t> (std::lround (bandEdges[b + 1] / binHz)) - 1));
        }

        std::size_t ring = 1;
        while (ring < static_cast<std::size_t> (0.3 * sr) + 4 * fftSize) ring <<= 1;
        raw.assign (ring, 0.0f);
        mask = ring - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill (raw.begin(), raw.end(), 0.0f);
        features.reset();
        sampleCount = 0;
        pendingCount = 0;
        readyCount = readyHead = 0;
    }

    /** Forget everything learned about this loop (presence, average spectra). */
    void resetLearning() noexcept
    {
        for (auto& p : presence) p = Presence {};
        for (auto& sp : longTerm) sp.fill (0.0f);
        longTermCount.fill (0);
    }

    void setAllowedTypes (std::uint32_t typeMask) noexcept { allowed = typeMask | bit (DrumType::misc); }
    std::uint32_t allowedTypes() const noexcept { return allowed; }
    void setLearning (bool shouldLearn) noexcept { learning = shouldLearn; }
    void setSensitivity (float s01) noexcept { minConfidence = 0.55f - 0.2f * std::clamp (s01, 0.0f, 1.0f); }

    /** Types seen often and confidently enough to count as present in this loop (auto detect). */
    std::uint32_t presentTypes() const noexcept
    {
        std::uint32_t m = 0;
        for (std::size_t t = 0; t < numDrumTypes; ++t)
        {
            const Presence& p = presence[t];
            if (p.count >= settings.presentMinHits
                && p.confidenceSum / static_cast<float> (p.count) >= settings.presentMinConfidence
                && static_cast<double> (p.lastSample - p.firstSample) >= settings.presentMinSpanSeconds * sr)
                m |= 1u << t;
        }
        return m;
    }

    /** Long-term average rise spectrum of a type (power^1/4 magnitudes, unit length, 13 bands); zeros if unseen. */
    const std::array<float, numBands>& averageSpectrum (DrumType t) const noexcept { return longTerm[index (t)]; }

    /** What the last classification saw (tuning, tests, research/classifier-eval). */
    struct Measurements
    {
        std::array<float, numDrumTypes> probability {};   // raw network output, before the allowed mask
        std::array<float, numBands> magnitude {};         // rise spectrum, power^1/4
    };
    const Measurements& lastMeasurements() const noexcept { return measured; }

    /** Samples between an onset and its classification (the decision window). */
    std::int64_t decisionDelay() const noexcept { return windowSamples; }

    void pushSample (float x) noexcept
    {
        raw[static_cast<std::size_t> (sampleCount) & mask] = x;
        features.pushSample (x);
        ++sampleCount;

        // Classify every pending event whose window is complete.
        std::size_t i = 0;
        while (i < pendingCount)
        {
            if (sampleCount >= pending[i].sample + windowSamples + 1)
            {
                classify (pending[i]);
                pending[i] = pending[pendingCount - 1];
                --pendingCount;
            }
            else
                ++i;
        }
    }

    void addEvent (const OnsetEvent& e) noexcept
    {
        if (pendingCount < pending.size()) pending[pendingCount++] = e;
    }

    /** MIDI mode: a note named drum t at input sample `sample`. Notes within 6 ms are one hit (kick + hat). */
    void addMidiHit (std::int64_t sample, DrumType t) noexcept
    {
        for (std::size_t i = 0; i < pendingCount; ++i)
            if (pending[i].midiTypes != 0 && std::llabs (pending[i].sample - sample) <= 6 * msSamples)
            {
                pending[i].midiTypes |= bit (t);
                return;
            }
        OnsetEvent e;
        e.sample = sample;
        e.midiTypes = bit (t);
        addEvent (e);
    }

    bool popClassified (ClassifiedHit& out) noexcept
    {
        if (readyCount == 0) return false;
        out = ready[readyHead];
        readyHead = (readyHead + 1) % ready.size();
        --readyCount;
        return true;
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    struct Presence
    {
        int count = 0;
        float confidenceSum = 0.0f;
        std::int64_t firstSample = 0, lastSample = 0;
    };

    /** Hann FFT of the fftSize samples starting at `start`; fills `power` (per bin). */
    void spectrumAt (std::int64_t start) noexcept
    {
        for (std::size_t i = 0; i < fftSize; ++i)
        {
            const std::int64_t q = start + static_cast<std::int64_t> (i);
            const float v = q >= 0 ? raw[static_cast<std::size_t> (q) & mask] : 0.0f;
            frame[i] = Fft::Complex (v * window[i], 0.0f);
        }
        fft.forward (frame.data());
        for (std::size_t k = 0; k < power.size(); ++k) power[k] = std::norm (frame[k]);
    }

    void bandPowers (std::array<float, numBands>& out) const noexcept
    {
        for (std::size_t b = 0; b < numBands; ++b)
        {
            float s = 0.0f;
            for (std::size_t k = binLo[b]; k <= binHi[b]; ++k) s += power[k];
            out[b] = s / static_cast<float> (binHi[b] - binLo[b] + 1);
        }
    }

    void classify (const OnsetEvent& ev) noexcept
    {
        const std::int64_t o = ev.sample, n = static_cast<std::int64_t> (fftSize);
        ClassifiedHit hit;
        hit.sample = o;

        // Rise spectrum: max of two post-onset frames minus the frame just before the onset.
        std::array<float, numBands> pre {}, post1 {}, post2 {};
        spectrumAt (o - msSamples - n);
        bandPowers (pre);
        spectrumAt (o - n / 8);
        bandPowers (post1);
        spectrumAt (std::min (o + windowSamples - n, o + n / 2));
        bandPowers (post2);
        std::array<float, numBands> mag {};
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const float post = std::max (post1[b], post2[b]);
            hit.rise[b] = post > 2.0f * pre[b] ? post - pre[b] : 0.0f;   // at least +3 dB, else no rise (tail wobble)
            mag[b] = std::sqrt (std::sqrt (hit.rise[b]));
        }

        float peak = 0.0f;
        for (std::int64_t i = o; i < o + 10 * msSamples; ++i) peak = std::max (peak, std::fabs (raw[static_cast<std::size_t> (i) & mask]));
        hit.peakDb = peak > 1e-6f ? 20.0f * std::log10 (peak) : -120.0f;

        // The network: one probability per type. In MIDI mode the notes already said which drums.
        std::array<float, numDrumTypes> p {};
        if (ev.midiTypes != 0)
            for (std::size_t t = 0; t < numDrumTypes; ++t) p[t] = (ev.midiTypes >> t) & 1u ? 1.0f : 0.0f;
        else
            network.run (features.extract (o), p);
        measured.probability = p;
        measured.magnitude = mag;

        // Manual mode: switched-off types get nothing; what they would have claimed goes to misc.
        std::array<float, numDrumTypes> w {};
        float lost = 0.0f;
        for (std::size_t t = 0; t < numDrumTypes; ++t)
        {
            if ((allowed >> t) & 1u) w[t] = p[t];
            else lost = std::max (lost, p[t]);
        }
        const std::size_t miscIndex = index (DrumType::misc);
        std::size_t best = miscIndex;
        for (std::size_t t = 0; t < numDrumTypes; ++t)
            if (t != miscIndex && (best == miscIndex || w[t] > w[best])) best = t;
        const float bestP = best == miscIndex ? 0.0f : w[best];
        w[miscIndex] = std::max ({ w[miscIndex], lost, 1.0f - bestP });   // the unexplained rest

        hit.weight = w;
        if (best != miscIndex && bestP >= minConfidence)
        {
            hit.primary = static_cast<DrumType> (best);
            hit.confidence = bestP;
        }
        else
        {
            hit.primary = DrumType::misc;
            hit.confidence = w[miscIndex];
        }

        learn (hit, mag);
        if (readyCount < ready.size())
        {
            ready[(readyHead + readyCount) % ready.size()] = hit;
            ++readyCount;
        }
    }

    void learn (const ClassifiedHit& hit, std::array<float, numBands> mag) noexcept
    {
        // Presence (auto detect): every type this hit clearly contains, not only the primary one
        // (a kick under a louder hat is still a kick in the loop).
        for (std::size_t t = 0; t < numDrumTypes; ++t)
        {
            if (t == index (DrumType::misc) || hit.weight[t] < 0.5f) continue;
            Presence& pr = presence[t];
            if (pr.count == 0) pr.firstSample = hit.sample;
            pr.lastSample = hit.sample;
            ++pr.count;
            pr.confidenceSum += hit.weight[t];
        }

        // Long-term spectrum: only clean single-type hits, so a type's spectrum is its own.
        if (! learning || hit.primary == DrumType::misc || hit.confidence < 0.8f) return;
        const std::size_t t = index (hit.primary);
        for (std::size_t u = 0; u < numDrumTypes; ++u)
            if (u != t && u != index (DrumType::misc) && hit.weight[u] >= 0.3f) return;
        float norm = 0.0f;
        for (float v : mag) norm += v * v;
        if (norm <= 1e-20f) return;
        norm = std::sqrt (norm);
        for (float& v : mag) v /= norm;
        const float a = longTermCount[t] == 0 ? 1.0f : settings.learnRate;
        for (std::size_t b = 0; b < numBands; ++b) longTerm[t][b] += a * (mag[b] - longTerm[t][b]);
        ++longTermCount[t];
    }

    double sr = 48000.0;
    Settings settings;
    HitFeatures features;
    Network network;
    std::size_t fftSize = 1024, mask = 0;
    std::int64_t windowSamples = 1920, msSamples = 48, sampleCount = 0;
    Fft fft;
    std::vector<Fft::Complex> frame;
    std::vector<float> power, window, raw;
    std::array<std::size_t, numBands> binLo {}, binHi {};

    std::uint32_t allowed = allDrumTypes;
    bool learning = true;
    float minConfidence = 0.45f;

    std::array<OnsetEvent, 32> pending {};
    std::size_t pendingCount = 0;
    std::array<ClassifiedHit, 64> ready {};
    std::size_t readyHead = 0, readyCount = 0;

    std::array<Presence, numDrumTypes> presence {};
    std::array<std::array<float, numBands>, numDrumTypes> longTerm {};
    std::array<int, numDrumTypes> longTermCount {};
    Measurements measured;
};
} // namespace plugin_dsp
