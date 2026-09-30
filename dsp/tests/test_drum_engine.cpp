// File: dsp/tests/test_drum_engine.cpp
// The whole DSP: bit-exact transparency and fixed 50 ms latency at 44.1 / 48 / 96 kHz, a kick
// level change acts on kicks and leaves hats alone, output gain / dry-wet / bypass, Shadow Play
// events, and the real-time factor.
#include "dsp_test.h"
#include "synth_drums.h"
#include <plugin_dsp/DrumEngine.h>

#include <chrono>
#include <cstdio>
#include <memory>

namespace
{
using namespace plugin_dsp;

struct Stereo { std::vector<float> l, r; };

Stereo processAll (DrumEngine& e, const Stereo& in, std::size_t block = 512)
{
    Stereo out = in;
    for (std::size_t i = 0; i < out.l.size(); i += block)
    {
        const std::size_t n = std::min (block, out.l.size() - i);
        e.process (out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}

Stereo loopKickHats (double sr, std::size_t length, std::vector<std::size_t>& kicks, std::vector<std::size_t>& soloHats)
{
    std::vector<synth::Hit> hits;
    const auto beat = static_cast<std::size_t> (0.5 * sr);   // 120 bpm
    for (std::size_t t = static_cast<std::size_t> (0.2 * sr); t + beat < length; t += beat)
    {
        hits.push_back ({ synth::Drum::kick, t, 0.9f });
        kicks.push_back (t);
        hits.push_back ({ synth::Drum::closedHat, t + beat / 2, 0.35f });
        soloHats.push_back (t + beat / 2);
    }
    const auto x = synth::render (hits, sr, length);
    return { x, x };
}

/** Energy (dB) of a filtered signal in [from, from + len). */
double bandEnergyDb (const std::vector<float>& x, const BiquadCoeffs& c, const std::vector<std::size_t>& starts, std::size_t offset, std::size_t len)
{
    Biquad f1, f2;
    f1.setCoeffs (c);
    f2.setCoeffs (c);
    std::vector<float> y (x.size());
    for (std::size_t i = 0; i < x.size(); ++i) y[i] = f2.process (f1.process (x[i]));
    double e = 0.0;
    for (std::size_t s : starts)
        for (std::size_t i = s + offset; i < std::min (y.size(), s + offset + len); ++i) e += static_cast<double> (y[i]) * static_cast<double> (y[i]);
    return 10.0 * std::log10 (e + 1e-20);
}
} // namespace

int main()
{
    // 1) Transparent and exactly 50 ms late, at every rate, in odd block sizes.
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        auto e = std::make_unique<DrumEngine>();
        e->prepare (sr);
        const auto D = static_cast<std::size_t> (e->latencySamples());
        DSP_CHECK (D == static_cast<std::size_t> (std::lround (0.05 * sr)));
        const auto len = static_cast<std::size_t> (1.5 * sr);
        auto loop = synth::render (synth::denseLoop (sr, 1, static_cast<std::size_t> (0.1 * sr)), sr, len);
        Stereo in { loop, loop };
        synth::Rng rng (7);
        for (auto& v : in.r) v = 0.7f * v + 0.01f * static_cast<float> (rng.next());
        const auto out = processAll (*e, in, 333);
        std::size_t bad = 0;
        for (std::size_t i = 0; i < len; ++i)
        {
            const float el = i >= D ? in.l[i - D] : 0.0f, er = i >= D ? in.r[i - D] : 0.0f;
            if (std::fabs (out.l[i] - el) > 0.0f || std::fabs (out.r[i] - er) > 0.0f) ++bad;   // bit exact
        }
        if (bad) std::printf ("sr %.0f: %zu samples differ from the delayed input\n", sr, bad);
        DSP_CHECK (bad == 0);
    }

    // 2) Kick level -6 dB: kicks drop ~6 dB below 120 Hz, the hats in between don't move.
    {
        const double sr = 48000.0;
        const auto len = static_cast<std::size_t> (4.5 * sr);
        std::vector<std::size_t> kicks, hats;
        const auto in = loopKickHats (sr, len, kicks, hats);
        auto ref = std::make_unique<DrumEngine>();
        ref->prepare (sr);
        const auto dry = processAll (*ref, in);

        auto e = std::make_unique<DrumEngine>();
        e->prepare (sr);
        DrumParams p;
        p.kick.level = -6.0f;
        e->setParams (p);
        const auto out = processAll (*e, in);

        const auto D = static_cast<std::size_t> (e->latencySamples());
        std::vector<std::size_t> k2 (kicks.begin() + 2, kicks.end()), h2 (hats.begin() + 2, hats.end());   // skip the first ones (learning)
        const auto lp = BiquadCoeffs::lowPass (sr, 120.0, 0.707), hp = BiquadCoeffs::highPass (sr, 6000.0, 0.707);
        const double kickChange = bandEnergyDb (out.l, lp, k2, D, static_cast<std::size_t> (0.15 * sr)) - bandEnergyDb (dry.l, lp, k2, D, static_cast<std::size_t> (0.15 * sr));
        const double hatChange = bandEnergyDb (out.l, hp, h2, D, static_cast<std::size_t> (0.05 * sr)) - bandEnergyDb (dry.l, hp, h2, D, static_cast<std::size_t> (0.05 * sr));
        std::printf ("kick -6 dB: kick band %.2f dB, hats %.2f dB\n", kickChange, hatChange);
        DSP_CHECK (kickChange < -5.0 && kickChange > -7.0);
        DSP_CHECK (std::fabs (hatChange) < 0.5);

        // Shadow Play got the kicks, on time.
        ShadowEvent ev;
        std::size_t kickEvents = 0, onTime = 0;
        while (e->shadowEvents().pop (ev))
            if (ev.type == DrumType::kick)
            {
                ++kickEvents;
                for (std::size_t k : kicks)
                    if (std::llabs (ev.sample - static_cast<std::int64_t> (k)) < static_cast<std::int64_t> (0.002 * sr)) ++onTime;
            }
        std::printf ("shadow events: %zu kicks (%zu on time) of %zu\n", kickEvents, onTime, kicks.size());
        DSP_CHECK (kickEvents == kicks.size() && onTime == kicks.size());
    }

    // 3) Output gain, dry/wet and bypass.
    {
        const double sr = 48000.0;
        const auto len = static_cast<std::size_t> (1.0 * sr);
        const auto x = dsptest::sine (200.0, sr, len);
        auto e = std::make_unique<DrumEngine>();
        e->prepare (sr);
        const auto D = static_cast<std::size_t> (e->latencySamples());
        DrumParams p;
        p.global.outputDb = -6.0f;
        e->setParams (p);
        auto out = processAll (*e, { x, x });
        const float g = dbToGain (-6.0f);
        DSP_CHECK_NEAR (out.l[len - 1], g * x[len - 1 - D], 1e-5);   // settled after 5 ms smoothing

        p.global.outputDb = 0.0f;
        p.global.bypass = true;
        e->setParams (p);
        out = processAll (*e, { x, x });
        DSP_CHECK_NEAR (out.l[len - 1], x[len - 1 - D], 1e-6);

        p.global.bypass = false;
        p.global.mix = 0.0f;
        p.kick.level = 12.0f;   // would change things if wet
        e->setParams (p);
        out = processAll (*e, { x, x });
        DSP_CHECK_NEAR (out.l[len - 1], x[len - 1 - D], 1e-6);
    }

    // 4) Real-time factor (stereo, 48 kHz, every type processing). Printed; checked loosely.
    {
        const double sr = 48000.0;
        const auto len = static_cast<std::size_t> (4.0 * sr);
        auto loop = synth::render (synth::denseLoop (sr, 2, 0), sr, len);
        auto e = std::make_unique<DrumEngine>();
        e->prepare (sr);
        DrumParams p;
        p.kick.comp = 50; p.snare.popify = 60; p.closedHat.tone = 3; p.openHat.decay = -40; p.crash.width = 150;
        p.ride.ping = 30; p.rim.sat = 40; p.clap.comp = 50; p.misc.tone = 2; p.advanced.sat = 20;
        e->setParams (p);
        const auto t0 = std::chrono::steady_clock::now();
        const auto out = processAll (*e, { loop, loop });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("real-time factor: %.3f (%.1f %% of one core for stereo 48 kHz)\n", secs / 4.0, 100.0 * secs / 4.0);
        DSP_CHECK (dsptest::allFinite (out.l) && dsptest::allFinite (out.r));
    }
    DSP_TEST_END;
}
