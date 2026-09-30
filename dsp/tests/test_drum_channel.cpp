// File: dsp/tests/test_drum_channel.cpp
// One drum's processor chain: neutral = exactly nothing, gate scaling, EQ response, attack and tail
// shaping by hit age, compressor curve, saturation / clip ceilings, width.
#include "dsp_test.h"
#include <plugin_dsp/DrumChannel.h>

#include <cstdint>
#include <vector>

namespace
{
using namespace plugin_dsp;

struct Out { std::vector<float> l, r; };

/** Runs a mono (or stereo) signal through a channel; returns the processed signal = x + delta. */
Out run (DrumChannel& ch, const std::vector<float>& xl, const std::vector<float>& xr, float gate, std::int64_t onset = 0)
{
    Out o { xl, xr };
    for (std::size_t i = 0; i < xl.size(); ++i)
    {
        float dl = 0.0f, dr = 0.0f;
        const auto age = static_cast<std::int64_t> (i) - onset;
        ch.process (xl[i], xr[i], age, gate, dl, dr);
        o.l[i] += dl;
        o.r[i] += dr;
    }
    return o;
}
} // namespace

int main()
{
    const double sr = 48000.0;
    const std::size_t n = 24000;
    const auto s1k = dsptest::sine (1000.0, sr, n);

    // 1) Neutral chain adds exactly nothing.
    {
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (ChannelSettings {});
        const auto o = run (ch, s1k, s1k, 1.0f);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < n; ++i) maxDiff = std::max (maxDiff, static_cast<double> (std::fabs (o.l[i] - s1k[i])));
        DSP_CHECK (maxDiff <= 0.0);
    }

    // 2) EQ bell +6 dB at 1 kHz, gate 1 -> +6 dB; gate 0 -> nothing; gate 0.5 -> halfway (linear).
    {
        ChannelSettings s;
        s.eq[0] = { EqSlot::Kind::bell, 1000.0f, 1.0f, 6.0f };
        for (float gate : { 1.0f, 0.0f, 0.5f })
        {
            DrumChannel ch;
            ch.prepare (sr);
            ch.setSettings (s);
            const auto o = run (ch, s1k, s1k, gate);
            const double expect = 1.0 + static_cast<double> (gate) * (std::pow (10.0, 6.0 / 20.0) - 1.0);
            DSP_CHECK_NEAR (dsptest::magnitudeAt (o.l, 1000.0, sr, 4800), expect, 0.01);
        }
        // high-pass slot: 200 Hz removes 50 Hz
        ChannelSettings h;
        h.eq[3] = { EqSlot::Kind::highPass, 200.0f, 0.707f, 0.0f };
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (h);
        const auto s50 = dsptest::sine (50.0, sr, n);
        const auto o = run (ch, s50, s50, 1.0f);
        DSP_CHECK (dsptest::magnitudeAt (o.l, 50.0, sr, 4800) < 0.1);
    }

    // 3) Attack +100 %: +9 dB for attackMs, then back to 0 dB; -100 %: -15 dB.
    {
        ChannelSettings s;
        s.attack = 1.0f;
        s.attackMs = 10.0f;
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (s);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (0))), 9.0, 1e-3);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (479))), 9.0, 1e-3);
        DSP_CHECK_NEAR (ch.shapeGain (720), 0.5 * (std::pow (10.0, 9.0 / 20.0) + 1.0), 1e-3);   // half way through the fade (in gain)
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (960))), 0.0, 1e-3);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (48000))), 0.0, 1e-3);
        s.attack = -1.0f;
        ch.setSettings (s);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (100))), -15.0, 1e-3);
    }

    // 4) Decay: +100 % -> +6 dB tail; -100 % -> 0.3 dB/ms faster (-30 dB 100 ms after the window), floor -60 dB.
    {
        ChannelSettings s;
        s.attackMs = 10.0f;
        s.decay = 1.0f;
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (s);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (0))), 0.0, 1e-3);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (4800))), 6.0, 1e-3);
        s.decay = -1.0f;
        ch.setSettings (s);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (480 + 4800))), -30.0, 0.05);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (480 + 48000))), -60.0, 0.05);
        s.decay = 0.0f;
        s.tailDb = -6.0f;   // ride Wash
        ch.setSettings (s);
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (ch.shapeGain (4800))), -6.0, 1e-3);
    }

    // 5) Compressor: static curve and a real loud sine gets quieter, a quiet one untouched.
    {
        ChannelSettings s;
        s.comp = 1.0f;   // threshold -30 dBFS, ratio 6
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (s);
        DSP_CHECK_NEAR (ch.compGainDb (-60.0f), 0.0, 1e-6);
        DSP_CHECK_NEAR (ch.compGainDb (-10.0f), -20.0f * (1.0f - 1.0f / 6.0f), 1e-4);
        std::vector<float> loud (s1k), quiet (s1k);
        for (auto& v : quiet) v *= 0.001f;   // -60 dBFS
        const auto ol = run (ch, loud, loud, 1.0f);
        ch.reset();
        const auto oq = run (ch, quiet, quiet, 1.0f);
        const double makeup = std::pow (10.0, 0.5 * 24.0 * (1.0 - 1.0 / 6.0) / 20.0);
        DSP_CHECK (dsptest::rms (ol.l, 4800) < 0.5 * dsptest::rms (loud, 4800));
        DSP_CHECK_NEAR (dsptest::rms (oq.l, 4800) / dsptest::rms (quiet, 4800), makeup, 0.02 * makeup);
    }

    // 6) Saturation holds a -6 dBFS peak, lifts quiet parts (at most +8 dB), rounds loud peaks;
    //    clip holds its ceiling.
    {
        ChannelSettings s;
        s.sat = 1.0f;
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (s);
        std::vector<float> small (s1k), half (s1k);
        for (auto& v : small) v *= 0.01f;
        for (auto& v : half) v *= 0.5f;
        const double lift = dsptest::rms (run (ch, small, small, 1.0f).l) / dsptest::rms (small);
        DSP_CHECK (lift > 2.0 && lift < 2.6);   // 2.5 / tanh (2.5) = 2.51
        float peak = 0.0f;
        for (float v : run (ch, half, half, 1.0f).l) peak = std::max (peak, std::fabs (v));
        DSP_CHECK_NEAR (peak, 0.5, 1e-3);
        peak = 0.0f;
        for (float v : run (ch, s1k, s1k, 1.0f).l) peak = std::max (peak, std::fabs (v));
        DSP_CHECK (peak < 0.52f);
        ChannelSettings c;
        c.clip = 1.0f;   // ceiling -12 dBFS
        ch.setSettings (c);
        peak = 0.0f;
        for (float v : run (ch, s1k, s1k, 1.0f).l) peak = std::max (peak, std::fabs (v));
        DSP_CHECK_NEAR (dsptest::toDb (static_cast<double> (peak)), -12.0, 0.01);
        DSP_CHECK (dsptest::allFinite (run (ch, s1k, s1k, 1.0f).l));
    }

    // 7) Width 0 folds to mono, width 2 doubles the side; level scales both.
    {
        const auto s2k = dsptest::sine (2000.0, sr, n);
        ChannelSettings s;
        s.width = 0.0f;
        DrumChannel ch;
        ch.prepare (sr);
        ch.setSettings (s);
        auto o = run (ch, s1k, s2k, 1.0f);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < n; ++i) maxDiff = std::max (maxDiff, static_cast<double> (std::fabs (o.l[i] - o.r[i])));
        DSP_CHECK (maxDiff < 1e-6);
        s.width = 2.0f;
        s.levelDb = -6.0f;
        ch.setSettings (s);
        o = run (ch, s1k, s2k, 1.0f);
        const float g = dbToGain (-6.0f);
        for (std::size_t i = 0; i < n; i += 997)
        {
            const float side = 0.5f * (s1k[i] - s2k[i]) * 2.0f, mid = 0.5f * (s1k[i] + s2k[i]);
            DSP_CHECK_NEAR (o.l[i], g * (mid + side), 1e-5);
            DSP_CHECK_NEAR (o.r[i], g * (mid - side), 1e-5);
        }
    }

    DSP_TEST_END;
}
