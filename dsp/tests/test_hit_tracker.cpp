// File: dsp/tests/test_hit_tracker.cpp
// Gates from the hit tracker: exact attack ramp (2 ms pre-roll), tail end per drum, retrigger,
// release shape, weights, length cap, timeSinceOnset.
#include "dsp_test.h"
#include "synth_drums.h"
#include <plugin_dsp/HitTracker.h>

#include <cstdio>

namespace
{
using namespace plugin_dsp;

ClassifiedHit hitOf (DrumType t, std::size_t sample, float weight = 1.0f)
{
    ClassifiedHit h;
    h.sample = static_cast<std::int64_t> (sample);
    h.weight[index (t)] = weight;
    h.primary = t;
    return h;
}

/** Feed audio; hits are registered 45 ms after their onset (about when the classifier reports). */
void run (HitTracker& tr, const std::vector<float>& x, const std::vector<ClassifiedHit>& hits, double sr)
{
    const auto delay = static_cast<std::size_t> (0.045 * sr);
    std::size_t next = 0;
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        while (next < hits.size() && static_cast<std::size_t> (hits[next].sample) + delay <= i) tr.addHit (hits[next++]);
        tr.pushSample (x[i]);
    }
}
} // namespace

int main()
{
    const double sr = 48000.0;
    const std::size_t t0 = static_cast<std::size_t> (0.3 * sr);

    // 1) A kick: gate ramps from onset-2 ms to 1 at the onset, holds, ends when the tail is gone.
    {
        HitTracker tr;
        tr.prepare (sr);
        const auto x = synth::render ({ { synth::Drum::kick, t0, 1.0f } }, sr, t0 + static_cast<std::size_t> (1.5 * sr));
        run (tr, x, { hitOf (DrumType::kick, t0) }, sr);
        const auto o = static_cast<std::int64_t> (t0);
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, o - 97), 0.0, 1e-6);
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, o - 48), 0.5, 0.02);
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, o), 1.0, 1e-6);
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, o + 4800), 1.0, 1e-6);
        const double endMs = 1000.0 * static_cast<double> (tr.lastEnd (DrumType::kick) - o) / sr;
        std::printf ("kick tail end at %.0f ms\n", endMs);
        DSP_CHECK (endMs > 300.0 && endMs <= 540.0);   // synth kick is 0.5 s long (+ envelope and 150 Hz filter ring-out)
        const auto end = tr.lastEnd (DrumType::kick);
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, end + 480), 0.5, 0.02);   // half way through the 20 ms release
        DSP_CHECK_NEAR (tr.gate (DrumType::kick, end + 960), 0.0, 1e-6);
        DSP_CHECK (tr.gate (DrumType::snare, o + 100) <= 0.0f);           // other types untouched
        DSP_CHECK (tr.timeSinceOnset (DrumType::kick, o + 480) == 480);
    }

    // 2) A closed hat ends far sooner than an open hat.
    {
        HitTracker a, b;
        a.prepare (sr);
        b.prepare (sr);
        run (a, synth::render ({ { synth::Drum::closedHat, t0, 0.5f } }, sr, t0 + static_cast<std::size_t> (sr)), { hitOf (DrumType::closedHat, t0) }, sr);
        run (b, synth::render ({ { synth::Drum::openHat, t0, 0.5f } }, sr, t0 + static_cast<std::size_t> (sr)), { hitOf (DrumType::openHat, t0) }, sr);
        const double ch = 1000.0 * static_cast<double> (a.lastEnd (DrumType::closedHat) - static_cast<std::int64_t> (t0)) / sr;
        const double oh = 1000.0 * static_cast<double> (b.lastEnd (DrumType::openHat) - static_cast<std::int64_t> (t0)) / sr;
        std::printf ("closed hat ends at %.0f ms, open hat at %.0f ms\n", ch, oh);
        DSP_CHECK (ch < 160.0);
        DSP_CHECK (oh > 400.0);
    }

    // 3) Retrigger: two kicks 200 ms apart -> the gate never drops, time since onset restarts.
    {
        HitTracker tr;
        tr.prepare (sr);
        const std::size_t t1 = t0 + static_cast<std::size_t> (0.2 * sr);
        const auto x = synth::render ({ { synth::Drum::kick, t0, 1.0f }, { synth::Drum::kick, t1, 1.0f } }, sr, t1 + static_cast<std::size_t> (sr));
        run (tr, x, { hitOf (DrumType::kick, t0), hitOf (DrumType::kick, t1) }, sr);
        float minGate = 1.0f;
        for (std::size_t i = t0; i < t1 + 480; ++i) minGate = std::min (minGate, tr.gate (DrumType::kick, static_cast<std::int64_t> (i)));
        DSP_CHECK (minGate > 0.99f);
        DSP_CHECK (tr.timeSinceOnset (DrumType::kick, static_cast<std::int64_t> (t1 + 10)) == 10);
    }

    // 4) Weights scale the gate; weights below minWeight add nothing.
    {
        HitTracker tr;
        tr.prepare (sr);
        const auto x = synth::render ({ { synth::Drum::snare, t0, 1.0f } }, sr, t0 + static_cast<std::size_t> (sr));
        ClassifiedHit h = hitOf (DrumType::snare, t0, 0.6f);
        h.weight[index (DrumType::clap)] = 0.2f;
        run (tr, x, { h }, sr);
        DSP_CHECK_NEAR (tr.gate (DrumType::snare, static_cast<std::int64_t> (t0 + 100)), 0.6, 1e-6);
        DSP_CHECK (tr.gate (DrumType::clap, static_cast<std::int64_t> (t0 + 100)) <= 0.0f);
    }

    // 5) Length cap: a constant noise that never decays still ends at the cap (closed hat: 1 s).
    {
        HitTracker tr;
        tr.prepare (sr);
        std::vector<float> x (t0 + static_cast<std::size_t> (2.0 * sr), 0.0f);
        synth::Rng rng (5);
        for (std::size_t i = t0; i < x.size(); ++i) x[i] = static_cast<float> (0.3 * rng.next());
        run (tr, x, { hitOf (DrumType::closedHat, t0) }, sr);
        DSP_CHECK_NEAR (static_cast<double> (tr.lastEnd (DrumType::closedHat) - static_cast<std::int64_t> (t0)) / sr, 1.0, 0.001);
    }

    DSP_TEST_END;
}
