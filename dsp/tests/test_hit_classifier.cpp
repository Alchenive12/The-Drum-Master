// File: dsp/tests/test_hit_classifier.cpp
// OnsetDetector -> HitClassifier on the synthetic kit: every drum alone at three levels, kick + hat
// together, the dense loop, the manual allowed-type mask, and auto-detect presence.
// Prints a confusion summary so tuning is visible in the CI log.
// The classifier is a learned network (D-C1); its real accuracy is measured on real drums in
// research/classifier-eval. This synthetic kit is not realistic (its snare is mostly bright noise,
// its tom is a pitched kick), so here the checks are sanity checks, not the accuracy target.
#include "dsp_test.h"
#include "synth_drums.h"
#include <plugin_dsp/HitClassifier.h>

#include <cstdio>

namespace
{
using namespace plugin_dsp;

DrumType expected (synth::Drum d)
{
    switch (d)
    {
        case synth::Drum::kick:      return DrumType::kick;
        case synth::Drum::snare:     return DrumType::snare;
        case synth::Drum::closedHat: return DrumType::closedHat;
        case synth::Drum::openHat:   return DrumType::openHat;
        case synth::Drum::crash:     return DrumType::crash;
        case synth::Drum::ride:      return DrumType::ride;
        case synth::Drum::rimshot:   return DrumType::rimshot;
        case synth::Drum::clap:      return DrumType::clap;
        case synth::Drum::tom:       return DrumType::misc;
        case synth::Drum::count:     break;
    }
    return DrumType::misc;
}

std::vector<ClassifiedHit> run (const std::vector<float>& x, double sr, HitClassifier& c)
{
    OnsetDetector d;
    d.prepare (sr);
    std::vector<ClassifiedHit> out;
    OnsetEvent e;
    ClassifiedHit h;
    auto step = [&] (float v) {
        if (d.processSample (v, e)) c.addEvent (e);
        c.pushSample (v);
        while (c.popClassified (h)) out.push_back (h);
    };
    for (float v : x) step (v);
    for (std::size_t i = 0; i < static_cast<std::size_t> (0.3 * sr); ++i) step (0.0f);
    return out;
}

const ClassifiedHit* nearest (const std::vector<ClassifiedHit>& hits, std::size_t t, double sr)
{
    const ClassifiedHit* best = nullptr;
    for (const auto& h : hits)
        if (std::llabs (h.sample - static_cast<std::int64_t> (t)) <= static_cast<std::int64_t> (0.010 * sr)
            && (! best || std::llabs (h.sample - static_cast<std::int64_t> (t)) < std::llabs (best->sample - static_cast<std::int64_t> (t))))
            best = &h;
    return best;
}
} // namespace

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        // 1) Every drum alone, three levels: the primary label must be the drum (tom -> misc).
        //    Hits are spaced so each rendered tail has ended before the next onset (a sound that
        //    stops exactly at an onset is a known weak spot, docs/03-decisions-for-review.md).
        {
            std::vector<synth::Hit> hits;
            std::size_t pos = static_cast<std::size_t> (0.3 * sr);
            for (float g : { 1.0f, 0.25f, 0.063f })
                for (std::size_t d = 0; d < synth::numDrums; ++d)
                {
                    hits.push_back ({ static_cast<synth::Drum> (d), pos, g });
                    pos += static_cast<std::size_t> ((static_cast<synth::Drum> (d) == synth::Drum::crash ? 2.7 : 0.8) * sr);
                }
            HitClassifier c;
            c.prepare (sr);
            c.setLearning (false);
            const auto out = run (synth::render (hits, sr, pos + static_cast<std::size_t> (sr)), sr, c);
            int correct = 0, kicksWrong = 0;
            for (const auto& h : hits)
            {
                const auto* got = nearest (out, h.sample, sr);
                const bool ok = got && got->primary == expected (h.drum);
                correct += ok ? 1 : 0;
                kicksWrong += (! ok && h.drum == synth::Drum::kick) ? 1 : 0;
                if (! ok)
                    std::printf ("[%.0f Hz] isolated %-10s g=%.3f -> %s\n", sr, synth::name (h.drum), static_cast<double> (h.gain),
                                 got ? drumTypeName (got->primary) : "(none)");
            }
            std::printf ("[%.0f Hz] isolated: %d / %zu correct\n", sr, correct, hits.size());
            DSP_CHECK (correct >= static_cast<int> (0.6 * static_cast<double> (hits.size())));
            DSP_CHECK (kicksWrong == 0);   // the kick is right at every level
        }

        // 2) Kick and closed hat together: both types get a real share.
        {
            const std::size_t t0 = static_cast<std::size_t> (0.3 * sr);
            HitClassifier c;
            c.prepare (sr);
            const auto out = run (synth::render ({ { synth::Drum::kick, t0, 1.0f }, { synth::Drum::closedHat, t0, 0.35f } }, sr, t0 + static_cast<std::size_t> (sr)), sr, c);
            const auto* got = nearest (out, t0, sr);
            DSP_CHECK (got != nullptr);
            if (got)
            {
                std::printf ("[%.0f Hz] kick+hat: kick %.2f  closed hat %.2f  misc %.2f\n", sr,
                             static_cast<double> (got->weight[index (DrumType::kick)]),
                             static_cast<double> (got->weight[index (DrumType::closedHat)]),
                             static_cast<double> (got->weight[index (DrumType::misc)]));
                DSP_CHECK (got->weight[index (DrumType::kick)] >= 0.2f);
                DSP_CHECK (got->weight[index (DrumType::closedHat)] >= 0.15f);
            }
        }

        // 3) Dense loop: the primary label of each detected event is one of the true labels there.
        {
            const auto hits = synth::denseLoop (sr, 4, static_cast<std::size_t> (0.2 * sr));
            HitClassifier c;
            c.prepare (sr);
            const auto out = run (synth::render (hits, sr, static_cast<std::size_t> (9.0 * sr)), sr, c);
            int matched = 0, correct = 0;
            for (const auto& h : out)
            {
                bool any = false, ok = false;
                for (const auto& t : hits)
                    if (std::llabs (h.sample - static_cast<std::int64_t> (t.sample)) <= static_cast<std::int64_t> (0.010 * sr))
                    {
                        any = true;
                        ok = ok || expected (t.drum) == h.primary;
                    }
                if (! any) continue;
                ++matched;
                correct += ok ? 1 : 0;
            }
            const double acc = matched ? static_cast<double> (correct) / matched : 0.0;
            std::printf ("[%.0f Hz] dense loop: %d / %d events labelled with a true type (%.1f %%)\n", sr, correct, matched, 100.0 * acc);
            DSP_CHECK (acc >= 0.85);

            // Auto-detect presence after 4 bars: the steady parts are present, the 2 crashes aren't.
            const auto present = c.presentTypes();
            DSP_CHECK ((present & bit (DrumType::kick)) != 0);
            DSP_CHECK ((present & bit (DrumType::snare)) != 0 || (present & bit (DrumType::clap)) != 0);
            DSP_CHECK ((present & bit (DrumType::closedHat)) != 0);
            DSP_CHECK ((present & bit (DrumType::crash)) == 0);
            // The Puppet Show hook learns only from clean single-type hits; in this loop every kick
            // lands with a hat, but the hats on steps 2 and 8 are alone.
            DSP_CHECK (c.averageSpectrum (DrumType::closedHat)[11] > 0.0f);
        }

        // 4) Manual mode without open hat: an open hat is never labelled open hat.
        {
            const std::size_t t0 = static_cast<std::size_t> (0.3 * sr);
            HitClassifier c;
            c.prepare (sr);
            c.setAllowedTypes (allDrumTypes & ~bit (DrumType::openHat));
            const auto out = run (synth::render ({ { synth::Drum::openHat, t0, 0.5f } }, sr, t0 + static_cast<std::size_t> (sr)), sr, c);
            for (const auto& h : out) DSP_CHECK (h.weight[index (DrumType::openHat)] <= 0.0f);
        }
    }

    DSP_TEST_END;
}
