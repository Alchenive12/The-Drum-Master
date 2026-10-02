// File: dsp/include/plugin_dsp/PuppetShow.h
// The Puppet Show (docs/05-features.md F1): a genre-aware smart EQ that keeps listening.
// Targets: docs/05-puppet-show-targets-v1.md (average of 3 answers, kick rows anchored to 43 real
// kits, decision D-C15). Design: prompts/answers/04-claude.md (decision D-C16).
//
// Listening (always, even when off, so switching on acts at once): every clean single-type hit
// is measured from its rise spectrum (ClassifiedHit::rise, 13 bands) as a ratio inside that
// drum's own sound, in dB:
//   M1 kick sub       kick 30-90 Hz / kick 90 Hz-16 kHz
//   M2 mud            200-500 Hz / total, kick + snare + toms (misc)
//   M3 presence       snare / clap 1.5-5 kHz / total
//   M4 kick click     kick 2-5 kHz / kick total
//   M5 cymbal air     hats + cymbals 8-16 kHz / 2-16 kHz
// Each measurement is an exponential average over ~12 hits.
//
// Correcting (when on): aim = genre target + span x knob (Mud: minus, + = cut more mud);
// error = aim - measured with a soft dead zone of the genre tolerance; gain = Depth x 0.8 x error,
// clamped to the knob's max boost / cut; no gain before 4 hits, boosts at most half size before
// 12 hits. Gains glide: towards a stronger correction tau 1.5 s (cuts 1 s), back tau 3 s; off or
// Depth 0 glides to 0 in ~0.2 s. The engine puts the gains into dedicated EQ slots of each drum's
// own chain, so they act only while that drum plays (drum-aware), measured before the user's own
// settings, which then act on top (it never fights the user's EQ).
// No allocation; addHit() on the audio thread, gains readable by the UI (atomics) for the meters.
#pragma once

#include "DrumParams.h"
#include "DrumTypes.h"
#include "HitClassifier.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <limits>

namespace plugin_dsp
{
class PuppetShow
{
public:
    enum Measure : std::size_t { kickSub, mud, presence, kickClick, air, numMeasures };
    static constexpr std::size_t numGenres = 5;   // Rock, Pop, EDM, Metal, Hip-hop

    struct Knob { float span, maxBoost, maxCut; };

    // docs/05-puppet-show-targets-v1.md, "Targets in the plugin's scale": the averaged answers,
    // moved from 150 ms-window ratios to what measure() reads from a hit's rise spectrum.
    static constexpr std::array<std::array<float, numMeasures>, numGenres> targets { {
        { -4.0f, 0.0f, -7.9f, -28.9f, -4.3f },    // Rock
        { -2.8f, -2.2f, -7.1f, -28.6f, -2.4f },   // Pop
        { 1.6f, -3.4f, -6.9f, -29.1f, -2.1f },    // EDM
        { -5.9f, -3.2f, -6.9f, -24.2f, -3.4f },   // Metal
        { 3.1f, -0.5f, -9.1f, -33.2f, -4.6f },    // Hip-hop
    } };
    static constexpr std::array<std::array<float, numMeasures>, numGenres> tolerance { {
        { 2.2f, 2.3f, 2.0f, 2.3f, 2.0f },
        { 1.8f, 1.8f, 1.7f, 2.0f, 1.7f },
        { 1.8f, 1.7f, 1.5f, 2.0f, 1.5f },
        { 1.8f, 2.0f, 1.8f, 1.8f, 2.0f },
        { 2.7f, 2.3f, 2.3f, 2.8f, 2.3f },
    } };
    static constexpr std::array<Knob, numMeasures> knobs { {
        { 5.0f, 5.3f, 6.7f },   // Bass boost -> kick sub
        { 5.3f, 3.7f, 8.3f },   // Mud control -> bodies
        { 5.0f, 6.0f, 6.0f },   // Mid presence -> snare, clap
        { 5.3f, 6.7f, 7.0f },   // Mid presence -> kick click
        { 4.5f, 5.0f, 6.0f },   // High control -> hats, cymbals
    } };

    /** Where each measure's gain acts: one EQ band in one drum's own chain (the engine fills the
        drums' reserved slots from this table in order; the editor draws its curves from it). */
    enum class Shape : std::uint8_t { bell, lowShelf, highShelf };
    struct Band { DrumType drum; Measure measure; Shape shape; float freq, q; };
    static constexpr std::array<Band, 11> bands { {
        { DrumType::kick, kickSub, Shape::lowShelf, 70.0f, 0.707f },
        { DrumType::kick, mud, Shape::bell, 250.0f, 1.0f },
        { DrumType::kick, kickClick, Shape::bell, 3500.0f, 0.9f },
        { DrumType::snare, mud, Shape::bell, 400.0f, 1.0f },
        { DrumType::snare, presence, Shape::bell, 3000.0f, 0.9f },
        { DrumType::clap, presence, Shape::bell, 3000.0f, 0.9f },
        { DrumType::misc, mud, Shape::bell, 300.0f, 1.0f },   // toms are misc
        { DrumType::closedHat, air, Shape::highShelf, 8000.0f, 0.707f },
        { DrumType::openHat, air, Shape::highShelf, 8000.0f, 0.707f },
        { DrumType::crash, air, Shape::highShelf, 8000.0f, 0.707f },
        { DrumType::ride, air, Shape::highShelf, 8000.0f, 0.707f },
    } };

    void reset() noexcept
    {
        measured.fill (0.0f);
        count.fill (0);
        gain.fill (0.0f);
        for (auto& g : published) g.store (0.0f, std::memory_order_relaxed);
    }

    /** Listen to one classified hit (audio thread). */
    void addHit (const ClassifiedHit& h) noexcept
    {
        const std::size_t t = index (h.primary);
        if (h.primary == DrumType::misc || h.confidence < 0.7f || h.weight[t] < 0.7f) return;
        for (std::size_t u = 0; u < numDrumTypes; ++u)
            if (u != t && u != index (DrumType::misc) && h.weight[u] >= 0.3f) return;   // clean hits only
        const auto m = measure (h.rise);
        switch (h.primary)
        {
            case DrumType::kick:
                learn (kickSub, m[kickSub]);
                learn (kickClick, m[kickClick]);
                learn (mud, m[mud]);
                break;
            case DrumType::snare:
                learn (presence, m[presence]);
                learn (mud, m[mud]);
                break;
            case DrumType::clap: learn (presence, m[presence]); break;
            case DrumType::closedHat: case DrumType::openHat: case DrumType::crash: case DrumType::ride:
                learn (air, m[air]);
                break;
            case DrumType::rimshot: case DrumType::misc: break;
        }
    }

    /** Advance the gains by dt seconds (once per block). */
    void advance (const DrumParams::PuppetShow& p, double dtSeconds) noexcept
    {
        const auto g = static_cast<std::size_t> (p.genre);
        const float depth = p.enabled ? std::clamp (p.depth / 100.0f, 0.0f, 1.0f) : 0.0f;
        const std::array<float, numMeasures> knob { p.bass / 100.0f, -p.mud / 100.0f, p.presence / 100.0f, p.presence / 100.0f, p.high / 100.0f };
        for (std::size_t m = 0; m < numMeasures; ++m)
        {
            float target = 0.0f;
            if (depth > 0.0f && count[m] >= minHits)
            {
                const float aim = targets[g][m] + knobs[m].span * std::clamp (knob[m], -1.0f, 1.0f);
                float error = aim - measured[m];
                const float tol = tolerance[g][m];
                error = error > tol ? error - tol : (error < -tol ? error + tol : 0.0f);   // soft dead zone
                const float boostLimit = count[m] >= fullHits ? knobs[m].maxBoost : 0.5f * knobs[m].maxBoost;
                target = std::clamp (depth * 0.8f * error, -knobs[m].maxCut, boostLimit);
            }
            // Glide: stronger correction slowly, relaxing slower still; cuts a bit faster; off fast.
            double tau = 3.0;
            if (depth <= 0.0f) tau = 0.2;
            else if (std::fabs (target) > std::fabs (gain[m])) tau = target < 0.0f ? 1.0 : 1.5;
            const auto a = static_cast<float> (1.0 - std::exp (-dtSeconds / tau));
            gain[m] += a * (target - gain[m]);
            if (std::fabs (gain[m]) < 1e-4f && std::fabs (target) < 1e-4f) gain[m] = 0.0f;
            published[m].store (gain[m], std::memory_order_relaxed);
        }
    }

    /** Current correction of a measure's band in dB (audio thread). */
    float gainDb (Measure m) const noexcept { return gain[m]; }
    /** Same, safe to read from the UI thread (the live band meters). */
    float meterDb (Measure m) const noexcept { return published[m].load (std::memory_order_relaxed); }
    /** Averaged measurement (dB) and how many hits it holds (tests, meters). */
    float measuredDb (Measure m) const noexcept { return measured[m]; }
    int hits (Measure m) const noexcept { return count[m]; }

    /** The five ratios of one hit's rise spectrum (energy = mean power per bin x band width, split
        linearly where a range cuts through a band). */
    static std::array<float, numMeasures> measure (const std::array<float, ClassifiedHit::numBands>& rise) noexcept
    {
        auto energy = [&rise] (double lo, double hi) {
            const auto& e = HitClassifier::bandEdges;
            double sum = 0.0;
            for (std::size_t b = 0; b < ClassifiedHit::numBands; ++b)
            {
                const double overlap = std::max (0.0, std::min (hi, e[b + 1]) - std::max (lo, e[b]));
                sum += static_cast<double> (rise[b]) * overlap;
            }
            return sum;
        };
        // A band the hit added (almost) nothing to can't be measured: NaN, and learn() skips it.
        auto db = [] (double num, double den) {
            if (! (num > 1e-20) || ! (den > 1e-20)) return std::numeric_limits<float>::quiet_NaN();
            return static_cast<float> (10.0 * std::log10 (num / den));
        };
        const double total = energy (30.0, 16000.0);
        return { db (energy (30.0, 90.0), energy (90.0, 16000.0)), db (energy (200.0, 500.0), total),
                 db (energy (1500.0, 5000.0), total), db (energy (2000.0, 5000.0), total),
                 db (energy (8000.0, 16000.0), energy (2000.0, 16000.0)) };
    }

private:
    static constexpr int minHits = 4, fullHits = 12;

    void learn (Measure m, float valueDb) noexcept
    {
        if (! std::isfinite (valueDb) || valueDb < -80.0f || valueDb > 40.0f) return;
        if (count[m] >= minHits) valueDb = std::clamp (valueDb, measured[m] - 12.0f, measured[m] + 12.0f);   // one odd hit can't swing it
        const float a = count[m] < fullHits ? 1.0f / static_cast<float> (count[m] + 1) : 1.0f / static_cast<float> (fullHits);
        measured[m] += a * (valueDb - measured[m]);
        ++count[m];
    }

    std::array<float, numMeasures> measured {}, gain {};
    std::array<int, numMeasures> count {};
    std::array<std::atomic<float>, numMeasures> published {};
};
} // namespace plugin_dsp
