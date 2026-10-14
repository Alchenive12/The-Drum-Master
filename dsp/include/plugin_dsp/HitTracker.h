// File: dsp/include/plugin_dsp/HitTracker.h
// Turns classified hits into per-type gates on the output timeline (docs/02-algorithm.md §5, §7).
//
// Input side (pushSample, the undelayed signal): a 1 ms envelope per range (kick < 150 Hz,
// snare-type 150 Hz-5 kHz, metal > 5 kHz, misc broadband) and a slow noise floor. Each hit
// segment ends at the first of
//   - the envelope stays below max(peak - R_t, floor + 6 dB) for a 20 ms hangover
//     (peak = loudest point in the first 20 ms; a re-rise above peak - 20 dB restarts the wait,
//     so a flam doesn't chop the hit),
//   - a new hit of the same type (retrigger),
//   - the length cap (crash 4 s, open hat and ride 2 s, others 1 s).
// Output side (gate(t, tau), tau = the sample being output = input index - lookahead): raised-cosine
// attack from onset - 2 ms to the onset, full while the segment lasts, raised-cosine release over
// 20 ms after its end; scaled by the hit's type weight. timeSinceOnset() gives the newest hit's
// age at tau (for Crack / Decay curves).
// No allocation after prepare().
#pragma once

#include "Biquad.h"
#include "DrumTypes.h"
#include "HitClassifier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace plugin_dsp
{
class HitTracker
{
public:
    static constexpr std::int64_t open = std::numeric_limits<std::int64_t>::max();

    struct Settings
    {
        double preRollSeconds = 0.002, releaseSeconds = 0.020, hangoverSeconds = 0.020, peakWindowSeconds = 0.020;
        float floorMarginDb = 6.0f, flamRiseDb = 20.0f, minWeight = 0.3f;
        std::array<float, numDrumTypes> tailDropDb { 36, 36, 30, 40, 40, 40, 30, 36, 30 };   // kick, snare, CH, OH, crash, ride, rim, clap, misc
        std::array<double, numDrumTypes> maxSeconds { 1.0, 1.0, 1.0, 2.0, 4.0, 2.0, 1.0, 1.0, 1.0 };
    };

    void prepare (double sampleRate) { prepare (sampleRate, Settings {}); }

    void prepare (double sampleRate, const Settings& s)
    {
        sr = sampleRate;
        settings = s;
        preRoll = static_cast<std::int64_t> (std::lround (s.preRollSeconds * sr));
        release = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::lround (s.releaseSeconds * sr)));
        hangover = static_cast<std::int64_t> (std::lround (s.hangoverSeconds * sr));
        peakWindow = static_cast<std::int64_t> (std::lround (s.peakWindowSeconds * sr));
        envCoeff = static_cast<float> (std::exp (-1.0 / (0.001 * sr)));
        floorRise = static_cast<float> (3.0 / sr);   // the floor may rise 3 dB per second
        lowFilter.setCoeffs (BiquadCoeffs::lowPass (sr, 150.0, 0.707));
        midFilter[0].setCoeffs (BiquadCoeffs::highPass (sr, 150.0, 0.707));
        midFilter[1].setCoeffs (BiquadCoeffs::lowPass (sr, 5000.0, 0.707));
        highFilter.setCoeffs (BiquadCoeffs::highPass (sr, 5000.0, 0.707));
        std::size_t ring = 1;   // level history: hits arrive ~40-48 ms after their onset
        while (ring < static_cast<std::size_t> (0.100 * sr)) ring <<= 1;
        levelHistory.assign (ring, {});
        historyMask = ring - 1;
        reset();
    }

    void reset() noexcept
    {
        lowFilter.reset();
        highFilter.reset();
        for (auto& f : midFilter) f.reset();
        energy.fill (0.0f);
        floorDb.fill (-60.0f);
        std::fill (levelHistory.begin(), levelHistory.end(), std::array<float, numRanges> { -140.0f, -140.0f, -140.0f, -140.0f });
        for (auto& list : segments) list = SegmentList {};
        inputIndex = 0;
    }

    /** Push one input sample (undelayed): updates envelopes and ends segments whose tail is over. */
    void pushSample (float x) noexcept
    {
        const std::array<float, numRanges> v { lowFilter.process (x), midFilter[1].process (midFilter[0].process (x)),
                                               highFilter.process (x), x };
        for (std::size_t r = 0; r < numRanges; ++r)
        {
            energy[r] = envCoeff * energy[r] + (1.0f - envCoeff) * v[r] * v[r];
            const float db = 10.0f * std::log10 (energy[r] + 1e-14f);
            levelDb[r] = db;
            floorDb[r] = std::min (floorDb[r] + floorRise, std::max (-80.0f, db));   // digital silence must not pin it at -140
        }
        levelHistory[static_cast<std::size_t> (inputIndex) & historyMask] = levelDb;
        for (std::size_t t = 0; t < numDrumTypes; ++t)
            for (auto& seg : segments[t].items)
                if (seg.used && seg.end == open) updateTail (seg, t, inputIndex, levelDb[rangeOf (t)]);
        ++inputIndex;
    }

    /** Register a classified hit: every type with weight >= minWeight gets a segment (misc only when
        it is the hit's main type; otherwise its weight is just the unexplained rest). The hit arrives
        after its onset, so its peak and tail so far are replayed from the level history. */
    void addHit (const ClassifiedHit& h) noexcept
    {
        const std::int64_t oldest = std::max<std::int64_t> (0, inputIndex - static_cast<std::int64_t> (levelHistory.size()));
        for (std::size_t t = 0; t < numDrumTypes; ++t)
        {
            if (t == index (DrumType::misc) ? h.primary != DrumType::misc : h.weight[t] < settings.minWeight) continue;
            SegmentList& list = segments[t];
            for (auto& seg : list.items)   // retrigger: the previous hit of this type ends here
                if (seg.used && seg.onset < h.sample && seg.end > h.sample) seg.end = h.sample;
            Segment& s = list.items[list.next];
            list.next = (list.next + 1) % list.items.size();
            s = Segment {};
            s.used = true;
            s.onset = h.sample;
            s.weight = std::min (1.0f, h.weight[t]);
            s.cap = h.sample + static_cast<std::int64_t> (settings.maxSeconds[t] * sr);
            for (std::int64_t n = std::max (s.onset, oldest); n < inputIndex && s.end == open; ++n)
                updateTail (s, t, n, levelHistory[static_cast<std::size_t> (n) & historyMask][rangeOf (t)]);
        }
    }

    /** Gate of type t at output position tau (0..1). */
    float gate (DrumType type, std::int64_t tau) const noexcept
    {
        float g = 0.0f;
        for (const auto& seg : segments[index (type)].items)
        {
            if (! seg.used || tau < seg.onset - preRoll) continue;
            const std::int64_t end = std::min (seg.end, seg.cap);
            float shape = 1.0f;
            if (tau < seg.onset)
                shape = raisedCosine (static_cast<float> (tau - (seg.onset - preRoll)) / static_cast<float> (std::max<std::int64_t> (1, preRoll)));
            else if (tau >= end)
            {
                if (tau >= end + release) continue;
                shape = 1.0f - raisedCosine (static_cast<float> (tau - end) / static_cast<float> (release));
            }
            g = std::max (g, shape * seg.weight);
        }
        return g;
    }

    /** Samples since the newest onset of type t at or before tau; -1 inside a hit's pre-roll (also
        when it retriggers a sounding one) or when no segment covers tau. */
    std::int64_t timeSinceOnset (DrumType type, std::int64_t tau) const noexcept
    {
        std::int64_t best = open;
        for (const auto& seg : segments[index (type)].items)
            if (seg.used && tau >= seg.onset - preRoll && tau < std::min (seg.end, seg.cap) + release)
                best = std::min (best, tau - seg.onset);
        return best == open ? -1 : std::max<std::int64_t> (-1, best);
    }

    /** End of the newest segment of type t (open while the tail is still sounding). For tests. */
    std::int64_t lastEnd (DrumType type) const noexcept
    {
        const Segment* newest = nullptr;
        for (const auto& seg : segments[index (type)].items)
            if (seg.used && (! newest || seg.onset > newest->onset)) newest = &seg;
        return newest ? std::min (newest->end, newest->cap) : -1;
    }

private:
    static constexpr std::size_t numRanges = 4;   // low, mid, high, broadband

    struct Segment
    {
        bool used = false;
        std::int64_t onset = 0, end = open, cap = open, belowSince = -1;
        float weight = 0.0f, peakDb = -140.0f;
    };

    struct SegmentList
    {
        std::array<Segment, 8> items {};
        std::size_t next = 0;
    };

    static std::size_t rangeOf (std::size_t type) noexcept
    {
        switch (static_cast<DrumType> (type))
        {
            case DrumType::kick: return 0;
            case DrumType::snare: case DrumType::rimshot: case DrumType::clap: return 1;
            case DrumType::closedHat: case DrumType::openHat: case DrumType::crash: case DrumType::ride: return 2;
            case DrumType::misc: return 3;
        }
        return 3;
    }

    static float raisedCosine (float x) noexcept
    {
        x = std::clamp (x, 0.0f, 1.0f);
        return 0.5f - 0.5f * std::cos (3.14159265f * x);
    }

    void updateTail (Segment& seg, std::size_t type, std::int64_t n, float level) noexcept
    {
        if (n < seg.onset) return;
        const std::size_t r = rangeOf (type);
        if (n <= seg.onset + peakWindow)
        {
            seg.peakDb = std::max (seg.peakDb, level);
            return;
        }
        if (n >= seg.cap) { seg.end = seg.cap; return; }
        const float threshold = std::max (seg.peakDb - settings.tailDropDb[type], floorDb[r] + settings.floorMarginDb);
        if (level < threshold)
        {
            if (seg.belowSince < 0) seg.belowSince = n;
            if (n - seg.belowSince >= hangover) seg.end = seg.belowSince;
        }
        else if (level > seg.peakDb - settings.flamRiseDb || level >= threshold)
            seg.belowSince = -1;   // still sounding (or rose again: flam guard)
    }

    double sr = 48000.0;
    Settings settings;
    std::int64_t preRoll = 96, release = 960, hangover = 960, peakWindow = 960, inputIndex = 0;
    float envCoeff = 0.98f, floorRise = 0.0f;
    Biquad lowFilter, highFilter;
    std::array<Biquad, 2> midFilter {};
    std::array<float, numRanges> energy {}, levelDb {}, floorDb {};
    std::vector<std::array<float, numRanges>> levelHistory;
    std::size_t historyMask = 0;
    std::array<SegmentList, numDrumTypes> segments {};
};
} // namespace plugin_dsp
