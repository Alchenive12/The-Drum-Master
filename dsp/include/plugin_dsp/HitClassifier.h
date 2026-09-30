// File: dsp/include/plugin_dsp/HitClassifier.h
// Classifies the events of OnsetDetector into the nine drum types (docs/02-algorithm.md §4).
//
// Two stages:
//   1. Regions by template fit. The event's rise spectrum (13 log bands, post-onset minus
//      pre-onset power, compressed to power^(1/4) so a snare's noise or a kick's click counts next
//      to its body) is explained as a non-negative mix of five templates (low drum, snare body,
//      mid crack, hat, cymbal), solved with multiplicative NNLS updates. The templates map to three
//      regions: LOW (low drum), MID (snare body + mid crack), METAL (hat + cymbal). Kick + hat splits
//      into LOW + METAL; a snare (body + noise) fits MID better than "kick + hat". Energy the
//      templates can't explain goes to misc.
//   2. Types inside a region by drum physics, measured on the first W ms:
//        LOW:   kick vs tom       -> pitch drop (low centroid late / early) or a low late centroid
//        MID:   clap              -> >= 3 bursts in the 1-5 kHz envelope within 32 ms
//               rimshot           -> tonal ring: low spectral flatness at 1-4 kHz, no bursts
//               snare             -> otherwise; bursts + strong body = snare + clap layered
//        METAL: closed hat        -> the 5 kHz+ envelope loses most of its own rise within W (or
//                                    drops > 12 dB), so a closed hat over a ringing crash still counts
//               ride              -> tonal 2-12 kHz: the loudest 5 % of bins hold much of the energy
//               crash / open hat  -> crash reaches lower: 630 Hz-2.5 kHz vs 4-16 kHz ratio
//        tom -> misc (toms are "misc" in the concept)
// A region is present when its templates explain most of the energy in its own frequency range
// (low 30-400 Hz, mid 400-4k, metal 4k-16k) and that range holds real energy; this finds a quiet
// hat under a loud kick. Type weight = region presence x how clear the physics decision is;
// below the threshold -> misc.
//
// Manual mode (allowed-type mask), auto-detect presence counting, and per-type long-term average
// spectra (the hook The Puppet Show compares against its genre targets, docs/05-features.md).
// A decision is made W = 40 ms after the onset (measured knee, research/lookahead-test), inside
// the 50 ms lookahead. prepare() allocates; pushSample()/addEvent()/popClassified() never do.
#pragma once

#include "Biquad.h"
#include "DrumTypes.h"
#include "Fft.h"
#include "OnsetDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace plugin_dsp
{
struct ClassifiedHit
{
    static constexpr std::size_t numBands = 13;
    std::int64_t sample = 0;                           // onset (input stream index)
    std::array<float, numDrumTypes> weight {};         // how sure each type is present in this hit (0..1;
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
    enum Template : std::size_t { tplLow, tplSnareBody, tplMidCrack, tplHat, tplCymbal, numTemplates };
    enum Region : std::size_t { regionLow, regionMid, regionMetal, numRegions };
    static constexpr std::array<Region, numTemplates> regionOf { regionLow, regionMid, regionMid, regionMetal, regionMetal };

    struct Settings
    {
        double windowSeconds = 0.040;       // audio after the onset used for the decision
        float closedHatDropDb = 12.0f;      // 5 kHz+ drop within the window: more = closed hat
        float rideConcentration = 0.5f;     // energy share of the loudest 5 % of 2-12 kHz bins: more = ride
        int clapBursts = 3;                 // bursts in 30 ms for a clap
        float burstProminenceDb = 5.0f;     // a burst stands this far above the dip before it
        float clapBodyRatioDb = -3.0f;      // body (160-400) vs crack (1-4k): more + bursts = snare + clap
        float rimFlatness = 0.15f;          // 1-4 kHz flatness: less = tonal ring = rimshot
        float crashLowExtension = 0.25f;    // mean magnitude 630-2.5k / 4-16k: more = crash
        float kickGlideRatio = 0.85f;       // low centroid late / early below this = kick (pitch drop)
        float kickCentroidHz = 110.0f;      // or a late low centroid below this = kick
        float minFamilyShare = 0.15f;
        float residualToMisc = 0.75f;       // unexplained energy above this -> misc
        int presentMinHits = 4;
        float presentMinConfidence = 0.65f;
        double presentMinSpanSeconds = 1.5;
        float learnRate = 0.1f;             // per confident hit
        float maxAdaptation = 0.5f;         // learned templates move at most half way from generic
    };

    void prepare (double sampleRate) { prepare (sampleRate, Settings {}); }

    void prepare (double sampleRate, const Settings& s)
    {
        sr = sampleRate;
        settings = s;
        fftSize = sampleRate >= 88000.0 ? 2048 : 1024;
        fft.prepare (fftSize);
        frame.assign (fftSize, {});
        power.assign (fftSize / 2 + 1, 0.0f);
        prePower.assign (fftSize / 2 + 1, 0.0f);
        window.resize (fftSize);
        for (std::size_t i = 0; i < fftSize; ++i)
            window[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * pi * static_cast<double> (i) / static_cast<double> (fftSize)));
        const double binHz = sampleRate / static_cast<double> (fftSize);
        for (std::size_t b = 0; b < numBands; ++b)
        {
            binLo[b] = std::max<std::size_t> (1, static_cast<std::size_t> (std::lround (bandEdges[b] / binHz)));
            binHi[b] = std::max (binLo[b], std::min (fftSize / 2, static_cast<std::size_t> (std::lround (bandEdges[b + 1] / binHz)) - 1));
        }
        flatLo = static_cast<std::size_t> (2000.0 / binHz);
        midFlatLo = static_cast<std::size_t> (1000.0 / binHz);
        midFlatHi = static_cast<std::size_t> (4000.0 / binHz);
        lowCentroidHi = std::max<std::size_t> (2, static_cast<std::size_t> (500.0 / binHz));
        binWidthHz = binHz;
        flatHi = std::min (fftSize / 2, static_cast<std::size_t> (12000.0 / binHz));
        sorted.assign (flatHi - flatLo + 1, 0.0f);

        windowSamples = static_cast<std::int64_t> (std::lround (s.windowSeconds * sr));
        msSamples = std::max<std::int64_t> (8, static_cast<std::int64_t> (std::lround (sr / 1000.0)));

        std::size_t ring = 1;
        while (ring < static_cast<std::size_t> (0.3 * sr) + 4 * fftSize) ring <<= 1;
        raw.assign (ring, 0.0f);
        hiRing.assign (ring, 0.0f);
        midRing.assign (ring, 0.0f);
        mask = ring - 1;
        envelope.assign (static_cast<std::size_t> (0.05 * sr / (static_cast<double> (msSamples) / 2.0)) + 16, 0.0f);

        hiFilter[0].setCoeffs (BiquadCoeffs::highPass (sr, 5000.0, 0.707));
        hiFilter[1].setCoeffs (BiquadCoeffs::highPass (sr, 5000.0, 0.707));
        midFilter[0].setCoeffs (BiquadCoeffs::highPass (sr, 1000.0, 0.707));
        midFilter[1].setCoeffs (BiquadCoeffs::lowPass (sr, 5000.0, 0.707));

        genericTemplates();
        reset();
    }

    void reset() noexcept
    {
        std::fill (raw.begin(), raw.end(), 0.0f);
        std::fill (hiRing.begin(), hiRing.end(), 0.0f);
        std::fill (midRing.begin(), midRing.end(), 0.0f);
        for (auto& f : hiFilter) f.reset();
        for (auto& f : midFilter) f.reset();
        sampleCount = 0;
        pendingCount = 0;
        readyCount = readyHead = 0;
    }

    /** Forget everything learned (presence, average spectra, adapted templates). */
    void resetLearning() noexcept
    {
        genericTemplates();
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

    /** Long-term average rise spectrum of a type (normalised magnitudes, 13 bands); zeros if unseen. */
    const std::array<float, numBands>& averageSpectrum (DrumType t) const noexcept { return longTerm[index (t)]; }

    /** What the last classification measured (tuning, tests, and later the Shadow Play display). */
    struct Measurements
    {
        std::array<float, numRegions> regionShare {};
        float midFlatness = 0.0f, lowExtension = 0.0f;
        std::array<float, numBands> magnitude {};
        float residualRatio = 0.0f, hiDropDb = 0.0f, midDropDb = 0.0f, flatness = 0.0f /* 2-12k concentration */, bodyRatioDb = 0.0f;
        int bursts = 0;
        float centroidEarlyHz = 0.0f, centroidLateHz = 0.0f, hiDecayFraction = 0.0f;
        float lowShare = 0.0f;      // rise power below 160 Hz / total
        float highShare = 0.0f;     // rise power above 4 kHz / total
        float bodyProminence = 0.0f;// compressed magnitude 160-400 Hz / mean 630 Hz-16 kHz
        float logCentroid = 0.0f;   // magnitude-weighted mean band index (0 = 30 Hz, 12 = 10-16 kHz)
        float hiRiseDb = 0.0f;      // 5 kHz+ envelope: peak minus pre-onset level
        float midRiseDb = 0.0f;     // 1-5 kHz envelope: peak minus pre-onset level
    };
    const Measurements& lastMeasurements() const noexcept { return measured; }

    /** Samples between an onset and its classification (the decision window). */
    std::int64_t decisionDelay() const noexcept { return windowSamples; }

    void pushSample (float x) noexcept
    {
        const std::size_t p = static_cast<std::size_t> (sampleCount) & mask;
        raw[p] = x;
        hiRing[p] = hiFilter[1].process (hiFilter[0].process (x));
        midRing[p] = midFilter[1].process (midFilter[0].process (x));
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

    // ---------------------------------------------------------------- templates
    void genericTemplates() noexcept
    {
        // Magnitudes per band (mean power per bin, square-rooted), 30 Hz ... 16 kHz. Starting points
        // from drum physics; they adapt towards the loop while learning. Decision D-C3 in
        // docs/03-decisions-for-review.md.
        // Compressed domain (power^(1/4)); bands 30,60,100,160,250,400,630,1k,1.6k,2.5k,4k,6.3k,10k-16k.
        generic[tplLow]       = { 0.70f, 0.90f, 1.00f, 0.80f, 0.45f, 0.25f, 0.18f, 0.16f, 0.16f, 0.14f, 0.03f, 0.01f, 0.005f };
        generic[tplSnareBody] = { 0.10f, 0.15f, 0.35f, 0.90f, 1.00f, 0.80f, 0.65f, 0.60f, 0.60f, 0.60f, 0.58f, 0.52f, 0.45f };
        generic[tplMidCrack]  = { 0.10f, 0.10f, 0.15f, 0.25f, 0.40f, 0.60f, 0.80f, 0.95f, 1.00f, 0.80f, 0.60f, 0.45f, 0.30f };
        generic[tplHat]       = { 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.10f, 0.15f, 0.25f, 0.40f, 0.70f, 1.00f, 0.90f };
        generic[tplCymbal]    = { 0.00f, 0.00f, 0.05f, 0.10f, 0.15f, 0.25f, 0.40f, 0.60f, 0.80f, 0.95f, 1.00f, 0.95f, 0.85f };
        for (auto& t : generic) normalise (t);
        templates = generic;
    }

    static void normalise (std::array<float, numBands>& t) noexcept
    {
        float n = 0.0f;
        for (float v : t) n += v * v;
        n = std::sqrt (n) + 1e-12f;
        for (float& v : t) v /= n;
    }

    // ---------------------------------------------------------------- analysis helpers
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

    /** Centroid (Hz) of what the hit adds below ~500 Hz (power minus the pre-onset frame). */
    float lowCentroid() const noexcept
    {
        float num = 0.0f, den = 1e-20f;
        for (std::size_t k = 1; k <= lowCentroidHi; ++k)
        {
            const float added = std::max (0.0f, power[k] - prePower[k]);
            num += static_cast<float> (static_cast<double> (k) * binWidthHz) * added;
            den += added;
        }
        return num / den;
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

    /** 1 ms RMS envelope in dB of a filtered ring, 0.5 ms steps, from `from` for `steps` steps. */
    std::size_t envelopeDb (const std::vector<float>& ring, std::int64_t from, std::size_t steps, std::int64_t length) noexcept
    {
        const std::int64_t stride = std::max<std::int64_t> (1, msSamples / 2);
        steps = std::min (steps, envelope.size());
        for (std::size_t st = 0; st < steps; ++st)
        {
            const std::int64_t p = from + static_cast<std::int64_t> (st) * stride;
            float e = 0.0f;
            for (std::int64_t i = 0; i < length; ++i)
            {
                const float v = ring[static_cast<std::size_t> (p + i) & mask];
                e += v * v;
            }
            envelope[st] = 10.0f * std::log10 (e / static_cast<float> (length) + 1e-14f);
        }
        return steps;
    }

    // ---------------------------------------------------------------- classification
    void classify (const OnsetEvent& ev) noexcept
    {
        const std::int64_t o = ev.sample, n = static_cast<std::int64_t> (fftSize);
        ClassifiedHit hit;
        hit.sample = o;

        // Rise spectrum: max of two post-onset frames minus the frame just before the onset.
        std::array<float, numBands> pre {}, post1 {}, post2 {};
        spectrumAt (o - msSamples - n);
        bandPowers (pre);
        std::copy (power.begin(), power.end(), prePower.begin());
        spectrumAt (o - n / 8);   // starts just before the onset
        bandPowers (post1);
        // Tonality 2-12 kHz: share of energy in the loudest 5 % of bins (noise ~0.2-0.3, partials much more).
        std::copy (power.begin() + static_cast<std::ptrdiff_t> (flatLo), power.begin() + static_cast<std::ptrdiff_t> (flatHi + 1), sorted.begin());
        const std::size_t topN = std::max<std::size_t> (1, sorted.size() / 20);
        std::nth_element (sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t> (topN), sorted.end(), std::greater<float>());
        float top = 0.0f, all = 1e-20f;
        for (std::size_t i = 0; i < sorted.size(); ++i) { all += sorted[i]; if (i < topN) top += sorted[i]; }
        const float concentration = top / all;
        float mNum = 0.0f, mDen = 0.0f;
        for (std::size_t k = midFlatLo; k <= midFlatHi; ++k)
        {
            mNum += std::log (power[k] + 1e-20f);
            mDen += power[k];
        }
        const float midBins = static_cast<float> (midFlatHi - midFlatLo + 1);
        const float midFlatness = std::exp (mNum / midBins) / (mDen / midBins + 1e-20f);
        const float centroidEarly = lowCentroid();
        spectrumAt (std::min (o + windowSamples - n, o + n / 2));
        bandPowers (post2);
        const float centroidLate = lowCentroid();

        std::array<float, numBands> mag {};
        float magNorm = 0.0f;
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const float post = std::max (post1[b], post2[b]);
            hit.rise[b] = post > 2.0f * pre[b] ? post - pre[b] : 0.0f;   // at least +3 dB, else no rise (tail wobble)
            mag[b] = std::sqrt (std::sqrt (hit.rise[b]));   // power^(1/4)
            magNorm += mag[b] * mag[b];
        }

        // Stage 1: regions by NNLS (multiplicative updates) over five templates; residual -> misc.
        std::array<float, numTemplates> act {};
        const bool regionAllowed[numRegions] = {
            true,   // low drum: kick, or tom -> misc (always allowed)
            (allowed & (bit (DrumType::snare) | bit (DrumType::clap) | bit (DrumType::rimshot))) != 0,
            (allowed & (bit (DrumType::closedHat) | bit (DrumType::openHat) | bit (DrumType::crash) | bit (DrumType::ride))) != 0,
        };
        for (std::size_t t = 0; t < numTemplates; ++t) act[t] = regionAllowed[regionOf[t]] ? 1.0f : 0.0f;
        for (int it = 0; it < 150; ++it)
            for (std::size_t t = 0; t < numTemplates; ++t)
            {
                if (act[t] <= 0.0f) continue;
                float num = 0.0f, den = 1e-20f;
                for (std::size_t b = 0; b < numBands; ++b)
                {
                    float model = 0.0f;
                    for (std::size_t g = 0; g < numTemplates; ++g) model += act[g] * templates[g][b];
                    num += templates[t][b] * mag[b];
                    den += templates[t][b] * model;
                }
                act[t] *= num / den;
            }
        float residual = 0.0f, maxMag = 1e-20f;
        std::array<float, numBands> model {};
        std::array<std::array<float, numBands>, numRegions> part {};
        for (std::size_t b = 0; b < numBands; ++b)
        {
            for (std::size_t t = 0; t < numTemplates; ++t)
            {
                const float v = act[t] * templates[t][b];
                model[b] += v;
                part[regionOf[t]][b] += v;
            }
            residual += (mag[b] - model[b]) * (mag[b] - model[b]);
            maxMag = std::max (maxMag, mag[b]);
        }
        const float residualRatio = magNorm > 0.0f ? std::min (1.0f, residual / magNorm) : 1.0f;
        float actSum = 1e-20f;
        for (float a : act) actSum += a;

        // Region presence: share of the modelled energy in the region's own bands that the region
        // explains, times how much real energy those bands hold (compared with the loudest band).
        constexpr std::size_t coreLo[numRegions] = { 0, 5, 10 }, coreHi[numRegions] = { 4, 9, 12 };
        std::array<float, numRegions> regionShare {};
        for (std::size_t r = 0; r < numRegions; ++r)
        {
            float own = 0.0f, total = 1e-20f, coreMax = 0.0f;
            for (std::size_t b = coreLo[r]; b <= coreHi[r]; ++b)
            {
                own += part[r][b];
                total += model[b];
                coreMax = std::max (coreMax, mag[b]);
            }
            const float explainedHere = own / total;
            const float energyHere = std::clamp ((coreMax / maxMag - 0.03f) / 0.05f, 0.0f, 1.0f);   // 0 below 3 %, 1 above 8 % (compressed)
            regionShare[r] = std::clamp ((explainedHere - 0.2f) / 0.3f, 0.0f, 1.0f) * energyHere;   // 0 below 20 %, 1 above 50 %
        }

        // Envelopes for the physics stage.
        const std::int64_t stride = std::max<std::int64_t> (1, msSamples / 2);
        const std::size_t steps = static_cast<std::size_t> ((windowSamples - msSamples) / stride);
        envelopeDb (hiRing, o - 6 * msSamples, 8, msSamples);   // 6 ms before the onset
        float hiPre = envelope[0];
        for (std::size_t i = 1; i < 8; ++i) hiPre = std::max (hiPre, envelope[i]);
        const std::size_t hiSteps = envelopeDb (hiRing, o, steps, msSamples);
        const float hiPeak = *std::max_element (envelope.begin(), envelope.begin() + static_cast<std::ptrdiff_t> (std::min<std::size_t> (hiSteps, 20)));
        const float hiEnd = envelope[hiSteps - 1];
        const float hiDrop = hiPeak - hiEnd;
        const float hiDecayFraction = hiDrop / std::max (hiPeak - hiPre, 3.0f);   // share of its own rise lost

        envelopeDb (midRing, o - 6 * msSamples, 8, msSamples);   // 6 ms before the onset
        float midPre = envelope[0];
        for (std::size_t i = 1; i < 8; ++i) midPre = std::max (midPre, envelope[i]);
        const std::size_t midSteps = envelopeDb (midRing, o - msSamples, steps, msSamples + msSamples / 2);
        const float midPeak = *std::max_element (envelope.begin(), envelope.begin() + static_cast<std::ptrdiff_t> (std::min<std::size_t> (midSteps, 24)));
        const float midDrop = midPeak - envelope[midSteps - 1];
        const std::size_t burstSteps = std::min<std::size_t> (midSteps, static_cast<std::size_t> (0.032 * sr / static_cast<double> (stride)));
        const int bursts = countBursts (burstSteps, midPeak, static_cast<std::size_t> (0.004 * sr / static_cast<double> (stride)));

        const float lowExtension = (mag[6] + mag[7] + mag[8]) / (mag[10] + mag[11] + mag[12] + 1e-12f);
        const float bodyDb = 10.0f * std::log10 (hit.rise[3] + hit.rise[4] + 1e-14f);
        const float crackDb = 10.0f * std::log10 (hit.rise[7] + hit.rise[8] + hit.rise[9] + 1e-14f);
        const float bodyRatio = bodyDb - crackDb;

        measured.regionShare = regionShare;
        measured.midFlatness = midFlatness;
        measured.lowExtension = lowExtension;
        measured.magnitude = mag;
        measured.residualRatio = residualRatio;
        measured.hiDropDb = hiDrop;
        measured.midDropDb = midDrop;
        measured.flatness = concentration;
        measured.bodyRatioDb = bodyRatio;
        measured.bursts = bursts;
        measured.centroidEarlyHz = centroidEarly;
        measured.centroidLateHz = centroidLate;
        measured.hiDecayFraction = hiDecayFraction;
        {
            float lo = 0.0f, hi = 0.0f, total = 1e-20f, num = 0.0f, den = 1e-20f;
            for (std::size_t b = 0; b < numBands; ++b)
            {
                const float p = hit.rise[b] * static_cast<float> (binHi[b] - binLo[b] + 1);   // total power in the band
                total += p;
                if (b <= 2) lo += p;
                if (b >= 10) hi += p;
                num += static_cast<float> (b) * mag[b];
                den += mag[b];
            }
            measured.lowShare = lo / total;
            measured.highShare = hi / total;
            measured.logCentroid = num / den;
            float highMean = 0.0f;
            for (std::size_t b = 6; b < numBands; ++b) highMean += mag[b];
            highMean /= static_cast<float> (numBands - 6);
            measured.bodyProminence = 0.5f * (mag[3] + mag[4]) / (highMean + 1e-12f);
            measured.hiRiseDb = hiPeak - hiPre;
            measured.midRiseDb = midPeak - midPre;
        }

        // Stage 2: types inside each family.
        std::array<float, numDrumTypes> w {};
        std::array<float, numDrumTypes> conf {};
        auto sigmoid = [] (float x) { return 1.0f / (1.0f + std::exp (-x)); };
        auto assign = [&] (DrumType t, float share, float p) {   // p = probability of this decision
            const float certainty = std::fabs (2.0f * p - 1.0f);
            w[index (t)] += share;
            conf[index (t)] = std::max (conf[index (t)], share * (0.6f + 0.4f * certainty));
        };
        const float explainedShare = 1.0f - (residualRatio > settings.residualToMisc ? residualRatio : 0.0f);
        for (std::size_t r = 0; r < numRegions; ++r)
        {
            const float share = regionShare[r] * explainedShare;
            if (share < settings.minFamilyShare) continue;
            switch (r)
            {
                case regionLow:
                {
                    const float glide = centroidLate / std::max (centroidEarly, 1.0f);
                    const float pKick = std::max (sigmoid ((settings.kickGlideRatio - glide) / 0.04f),
                                                  sigmoid ((settings.kickCentroidHz - centroidLate) / 10.0f));
                    if (pKick >= 0.5f && (allowed & bit (DrumType::kick))) assign (DrumType::kick, share, pKick);
                    else w[index (DrumType::misc)] += share;   // tom
                    break;
                }
                case regionMid:
                {
                    const float pClap = sigmoid ((static_cast<float> (bursts) - (static_cast<float> (settings.clapBursts) - 0.5f)) / 0.3f);
                    const float pBody = sigmoid ((bodyRatio - settings.clapBodyRatioDb) / 2.0f);
                    const float pRim = (1.0f - pClap) * sigmoid ((settings.rimFlatness - midFlatness) / 0.04f);
                    if (pClap > 0.5f && pBody > 0.5f)   // clap layered on a snare: both
                    {
                        assign (pick (DrumType::snare), share, pBody);
                        assign (pick (DrumType::clap), share, pClap);
                    }
                    else if (pClap > 0.5f) assign (pick (DrumType::clap), share, pClap);
                    else if (pRim > 0.5f)  assign (pick (DrumType::rimshot), share, pRim);
                    else                   assign (pick (DrumType::snare), share, 1.0f - std::max (pClap, pRim));
                    break;
                }
                case regionMetal:
                {
                    const float pClosed = std::max (sigmoid ((hiDrop - settings.closedHatDropDb) / 3.0f),
                                                    sigmoid ((hiDecayFraction - 0.55f) / 0.08f));
                    const float pRide = sigmoid ((concentration - settings.rideConcentration) / 0.05f);
                    const float pCrash = sigmoid ((lowExtension - settings.crashLowExtension) / 0.06f);
                    DrumType t = DrumType::openHat;
                    float p = 1.0f - pClosed;
                    if (pClosed > 0.5f)     { t = DrumType::closedHat; p = pClosed; }
                    else if (pRide > 0.5f)  { t = DrumType::ride;      p = std::min (p, pRide); }
                    else if (pCrash > 0.5f) { t = DrumType::crash;     p = std::min (p, pCrash); }
                    else                    { p = std::min (p, 1.0f - pCrash); }
                    assign (pickMetal (t), share, p);
                    break;
                }
                default: break;
            }
        }
        // Low confidence -> not that type; misc takes whatever no type is sure about (neutral).
        float strongest = 0.0f;
        for (std::size_t t = 0; t + 1 < numDrumTypes; ++t)
        {
            if (w[t] > 0.0f && conf[t] < minConfidence) w[t] = 0.0f;
            w[t] = std::min (w[t], 1.0f);
            strongest = std::max (strongest, w[t]);
        }
        w[index (DrumType::misc)] = std::max (w[index (DrumType::misc)], 1.0f - strongest);

        // Primary label = the strongest real type; misc only when no type is confident.
        std::size_t best = index (DrumType::misc);
        for (std::size_t t = 0; t + 1 < numDrumTypes; ++t)
            if (w[t] > 0.0f && (best == index (DrumType::misc) || w[t] > w[best])) best = t;
        hit.weight = w;
        hit.primary = static_cast<DrumType> (best);
        hit.confidence = best == index (DrumType::misc) ? w[best] : conf[best];
        hit.peakDb = std::max (hiPeak, midPeak);

        learn (hit, mag, act, actSum);
        if (readyCount < ready.size())
        {
            ready[(readyHead + readyCount) % ready.size()] = hit;
            ++readyCount;
        }
    }

    /** Metal type with the allowed mask applied: the closest allowed relative, else misc. */
    DrumType pickMetal (DrumType wanted) const noexcept
    {
        if (allowed & bit (wanted)) return wanted;
        const DrumType order[4][3] = {
            { DrumType::openHat, DrumType::ride, DrumType::crash },     // closed hat
            { DrumType::closedHat, DrumType::crash, DrumType::ride },   // open hat
            { DrumType::openHat, DrumType::ride, DrumType::closedHat }, // crash
            { DrumType::crash, DrumType::openHat, DrumType::closedHat } // ride
        };
        const std::size_t row = wanted == DrumType::closedHat ? 0 : wanted == DrumType::openHat ? 1 : wanted == DrumType::crash ? 2 : 3;
        for (DrumType t : order[row]) if (allowed & bit (t)) return t;
        return DrumType::misc;
    }

    /** Mid type with the allowed mask applied (snare -> clap -> rimshot -> misc). */
    DrumType pick (DrumType wanted) const noexcept
    {
        if (allowed & bit (wanted)) return wanted;
        for (DrumType t : { DrumType::snare, DrumType::clap, DrumType::rimshot })
            if (allowed & bit (t)) return t;
        return DrumType::misc;
    }

    /** Local maxima of `envelope` at least 3 dB above the dip before them, `minGap` steps apart. */
    int countBursts (std::size_t steps, float peak, std::size_t minGap) const noexcept
    {
        int count = 0;
        std::size_t last = 0;
        float dip = 1e9f;
        bool first = true;
        for (std::size_t i = 1; i + 1 < steps; ++i)
        {
            dip = std::min (dip, envelope[i]);
            const bool isMax = envelope[i] >= envelope[i - 1] && envelope[i] > envelope[i + 1];
            if (isMax && envelope[i] > peak - 12.0f && (first || (envelope[i] - dip >= settings.burstProminenceDb && i - last >= minGap)))
            {
                ++count;
                last = i;
                dip = envelope[i];
                first = false;
            }
        }
        return count;
    }

    void learn (const ClassifiedHit& hit, const std::array<float, numBands>& mag,
                const std::array<float, numTemplates>& act, float actSum) noexcept
    {
        const std::size_t t = index (hit.primary);
        if (hit.primary == DrumType::misc) return;
        Presence& p = presence[t];
        if (p.count == 0) p.firstSample = hit.sample;
        p.lastSample = hit.sample;
        ++p.count;
        p.confidenceSum += hit.confidence;

        if (! learning || hit.confidence < 0.8f || hit.weight[t] < 0.7f) return;

        // Long-term average spectrum of this type (the Puppet Show hook).
        std::array<float, numBands> m = mag;
        normalise (m);
        const float a = longTermCount[t] == 0 ? 1.0f : settings.learnRate;
        for (std::size_t b = 0; b < numBands; ++b) longTerm[t][b] += a * (m[b] - longTerm[t][b]);
        ++longTermCount[t];

        // Adapt the dominant family's template towards this loop, capped at maxAdaptation.
        std::size_t fam = 0;
        for (std::size_t f = 1; f < numTemplates; ++f) if (act[f] > act[fam]) fam = f;
        if (act[fam] / actSum < 0.7f) return;
        std::array<float, numBands> learned = templates[fam];
        for (std::size_t b = 0; b < numBands; ++b) learned[b] += settings.learnRate * (m[b] - learned[b]);
        for (std::size_t b = 0; b < numBands; ++b)   // clamp the distance from the generic template
        {
            const float lo = generic[fam][b] * (1.0f - settings.maxAdaptation), hi = generic[fam][b] * (1.0f + settings.maxAdaptation) + 0.05f * settings.maxAdaptation;
            learned[b] = std::clamp (learned[b], lo, hi);
        }
        normalise (learned);
        templates[fam] = learned;
    }

    double sr = 48000.0;
    Settings settings;
    std::size_t fftSize = 1024, mask = 0, flatLo = 0, flatHi = 0, midFlatLo = 0, midFlatHi = 0, lowCentroidHi = 10;
    double binWidthHz = 46.875;
    std::int64_t windowSamples = 1920, msSamples = 48, sampleCount = 0;
    Fft fft;
    std::vector<Fft::Complex> frame;
    std::vector<float> power, prePower, window, raw, hiRing, midRing, envelope, sorted;
    std::array<std::size_t, numBands> binLo {}, binHi {};
    std::array<Biquad, 2> hiFilter {}, midFilter {};
    std::array<std::array<float, numBands>, numTemplates> generic {}, templates {};
    std::array<Presence, numDrumTypes> presence {};
    std::array<std::array<float, numBands>, numDrumTypes> longTerm {};
    std::array<int, numDrumTypes> longTermCount {};
    std::uint32_t allowed = allDrumTypes;
    bool learning = true;
    float minConfidence = 0.45f;
    std::array<OnsetEvent, 16> pending {};
    std::size_t pendingCount = 0;
    std::array<ClassifiedHit, 16> ready {};
    Measurements measured;
    std::size_t readyCount = 0, readyHead = 0;
};
} // namespace plugin_dsp
