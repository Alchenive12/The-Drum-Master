// File: dsp/tests/test_puppet_show.cpp
// The Puppet Show: measurement math, nothing before 4 hits, the correction formula (dead zone,
// Depth, bipolar knobs, Mud sign), the limits, glide times, off -> 0, and in the engine: still
// bit-exact transparent while nothing is measured.
#include "dsp_test.h"
#include <plugin_dsp/DrumEngine.h>
#include <plugin_dsp/PuppetShow.h>

#include <algorithm>
#include <memory>

namespace
{
using namespace plugin_dsp;
using PS = PuppetShow;

/** A clean single-type hit whose rise spectrum is white (constant power per Hz) except a scale on
    the bands below 90 Hz (sets the kick sub ratio) and above 8 kHz. */
ClassifiedHit hitOf (DrumType t, float subScale = 1.0f, float airScale = 1.0f)
{
    ClassifiedHit h;
    h.primary = t;
    h.confidence = 0.95f;
    h.weight[index (t)] = 0.95f;
    for (std::size_t b = 0; b < ClassifiedHit::numBands; ++b) h.rise[b] = 1.0f;
    h.rise[0] = h.rise[1] = subScale;   // 30-100 Hz
    h.rise[12] = airScale;              // 10-16 kHz
    return h;
}

void settle (PS& ps, const DrumParams::PuppetShow& p, double seconds)
{
    for (double t = 0.0; t < seconds; t += 0.01) ps.advance (p, 0.01);
}
} // namespace

int main()
{
    // 1) Measurement: white rise spectrum -> ratios of band widths.
    {
        const auto m = PS::measure (hitOf (DrumType::kick).rise);
        DSP_CHECK_NEAR (m[PS::kickSub], 10.0 * std::log10 (60.0 / 15910.0), 0.01);
        DSP_CHECK_NEAR (m[PS::mud], 10.0 * std::log10 (300.0 / 15970.0), 0.01);
        DSP_CHECK_NEAR (m[PS::presence], 10.0 * std::log10 (3500.0 / 15970.0), 0.01);
        DSP_CHECK_NEAR (m[PS::kickClick], 10.0 * std::log10 (3000.0 / 15970.0), 0.01);
        DSP_CHECK_NEAR (m[PS::air], 10.0 * std::log10 (8000.0 / 14000.0), 0.01);
    }

    // 2) Kick with too little sub for Rock: nothing before 4 hits, then a boost that settles at
    //    Depth x 0.8 x (error - tolerance), half-limited before 12 hits.
    {
        DrumParams::PuppetShow p;
        p.enabled = true;
        p.depth = 100.0f;
        p.genre = DrumParams::PuppetShow::Genre::rock;
        // A kick with roughly 6 dB less sub than the Rock target.
        const double rockSub = static_cast<double> (PS::targets[0][PS::kickSub]);
        const auto sub = static_cast<float> (std::pow (10.0, (rockSub - 6.0) / 10.0) * 15910.0 / 60.0);
        const auto wantDb = static_cast<double> (PS::measure (hitOf (DrumType::kick, sub).rise)[PS::kickSub]);
        PS ps;
        ps.reset();
        for (int i = 0; i < 3; ++i) ps.addHit (hitOf (DrumType::kick, sub));
        settle (ps, p, 10.0);
        DSP_CHECK (std::fabs (ps.gainDb (PS::kickSub)) <= 0.0f);   // 3 hits: not yet
        for (int i = 0; i < 20; ++i) ps.addHit (hitOf (DrumType::kick, sub));
        DSP_CHECK_NEAR (ps.measuredDb (PS::kickSub), wantDb, 0.01);
        settle (ps, p, 20.0);
        const double expect = 0.8 * ((rockSub - wantDb) - 2.2);   // Rock tolerance 2.2
        DSP_CHECK (expect > 2.0 && expect < 5.3);
        DSP_CHECK_NEAR (ps.gainDb (PS::kickSub), expect, 0.02);
        DSP_CHECK_NEAR (ps.meterDb (PS::kickSub), expect, 0.02);   // what the UI meter shows

        // Bass boost +100 %: aim +5 dB higher -> bigger boost, but never above the 5.3 dB limit.
        p.bass = 100.0f;
        settle (ps, p, 20.0);
        DSP_CHECK_NEAR (ps.gainDb (PS::kickSub), 5.3, 0.02);
        // Depth 50 % halves it.
        p.bass = 0.0f;
        p.depth = 50.0f;
        settle (ps, p, 30.0);
        DSP_CHECK_NEAR (ps.gainDb (PS::kickSub), 0.5 * expect, 0.02);
        // Off: glides to exactly 0 fast.
        p.enabled = false;
        settle (ps, p, 2.0);
        DSP_CHECK (std::fabs (ps.gainDb (PS::kickSub)) <= 0.0f);
    }

    // 3) Mud: too little body -> boost, capped at 3.7 dB; Mud control +100 % aims 5.3 dB lower
    //    (+ = cut more), which here lands inside the tolerance -> nothing.
    {
        DrumParams::PuppetShow p;
        p.enabled = true;
        p.depth = 100.0f;
        PS ps;
        ps.reset();
        const double rockMud = static_cast<double> (PS::targets[0][PS::mud]);   // 0.0
        // a white rise spectrum: mud reads -17.3 dB, far below the Rock target
        auto h = hitOf (DrumType::snare);
        for (int i = 0; i < 20; ++i) ps.addHit (h);
        const double measured = static_cast<double> (ps.measuredDb (PS::mud));
        settle (ps, p, 20.0);
        const double want = std::clamp (0.8 * ((rockMud - measured) - 2.3), -8.3, 3.7);
        DSP_CHECK_NEAR (ps.gainDb (PS::mud), want, 0.02);
        p.mud = 100.0f;
        settle (ps, p, 30.0);
        double err = (rockMud - 5.3) - measured;
        err = err > 2.3 ? err - 2.3 : (err < -2.3 ? err + 2.3 : 0.0);
        DSP_CHECK_NEAR (ps.gainDb (PS::mud), std::clamp (0.8 * err, -8.3, 3.7), 0.02);
        DSP_CHECK (static_cast<double> (ps.gainDb (PS::mud)) < want);   // + = less mud
    }

    // 4) Hits that are not clean (a kick with a hat on it) or unsure are not measured.
    {
        PS ps;
        ps.reset();
        auto layered = hitOf (DrumType::kick);
        layered.weight[index (DrumType::closedHat)] = 0.8f;
        auto unsure = hitOf (DrumType::kick);
        unsure.confidence = 0.5f;
        for (int i = 0; i < 10; ++i) { ps.addHit (layered); ps.addHit (unsure); }
        DSP_CHECK (ps.hits (PS::kickSub) == 0);
    }

    // 5) Engine: Puppet Show on at full depth but nothing measured yet -> still bit-exact transparent.
    {
        const double sr = 48000.0;
        auto e = std::make_unique<DrumEngine>();
        e->prepare (sr);
        DrumParams p;
        p.puppetShow.enabled = true;
        p.puppetShow.depth = 100.0f;
        p.puppetShow.bass = 100.0f;
        e->setParams (p);
        auto x = dsptest::sine (220.0, sr, 24000);   // a steady tone: no drum hits
        std::vector<float> l (x), r (x);
        for (std::size_t i = 0; i < l.size(); i += 512)
            e->process (l.data() + i, r.data() + i, std::min<std::size_t> (512, l.size() - i));
        const auto D = static_cast<std::size_t> (e->latencySamples());
        std::size_t bad = 0;
        for (std::size_t i = D; i < l.size(); ++i)
            if (std::fabs (l[i] - x[i - D]) > 0.0f) ++bad;
        DSP_CHECK (bad == 0);
    }
    DSP_TEST_END;
}
