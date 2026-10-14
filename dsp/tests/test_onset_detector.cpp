// File: dsp/tests/test_onset_detector.cpp
// Onset detection on the deterministic synthetic kit (dsp/tests/synth_drums.h):
// isolated hits of all 9 drums, kick + hat together (one event, low and high bands),
// a dense overlapping loop (F1 >= 0.95 within +-10 ms), no events on silence / steady tones,
// at 44.1, 48 and 96 kHz. Prints timing stats so tuning is visible in the CI log.
#include "dsp_test.h"
#include "synth_drums.h"
#include <plugin_dsp/OnsetDetector.h>

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
using plugin_dsp::OnsetDetector;
using plugin_dsp::OnsetEvent;

/** Longest wait (samples) between an event's onset and the sample that reported it, minus the
    detector's promised maxReportDelay(); must stay <= 0. Updated by detect(). */
std::int64_t lateBy = std::numeric_limits<std::int64_t>::min();

std::vector<OnsetEvent> detect (const std::vector<float>& x, double sr)
{
    OnsetDetector d;
    d.prepare (sr);
    std::vector<OnsetEvent> ev;
    OnsetEvent e;
    std::int64_t n = 0;
    auto push = [&] (float v) {
        if (d.processSample (v, e))
        {
            ev.push_back (e);
            lateBy = std::max (lateBy, n - e.sample - d.maxReportDelay());
        }
        ++n;
    };
    for (float v : x) push (v);
    for (std::size_t i = 0; i < static_cast<std::size_t> (sr * 0.2); ++i) push (0.0f);   // flush the tail
    return ev;
}

/** True onsets closer than 6 ms are one event (layered hits). */
std::vector<std::int64_t> trueEvents (const std::vector<synth::Hit>& hits, double sr)
{
    std::vector<std::int64_t> t;
    for (const auto& h : hits) t.push_back (static_cast<std::int64_t> (h.sample));
    std::sort (t.begin(), t.end());
    std::vector<std::int64_t> out;
    for (auto v : t)
        if (out.empty() || v - out.back() > static_cast<std::int64_t> (0.006 * sr)) out.push_back (v);
    return out;
}

struct Match { std::size_t tp = 0, fp = 0, fn = 0; double meanErrMs = 0.0, maxErrMs = 0.0; };

Match match (const std::vector<std::int64_t>& truth, const std::vector<OnsetEvent>& ev, double sr, double tolMs)
{
    Match m;
    std::vector<bool> used (ev.size(), false);
    const auto tol = static_cast<std::int64_t> (tolMs * sr / 1000.0);
    double errSum = 0.0;
    for (auto t : truth)
    {
        std::size_t best = ev.size();
        std::int64_t bestErr = tol + 1;
        for (std::size_t i = 0; i < ev.size(); ++i)
        {
            const std::int64_t err = std::llabs (ev[i].sample - t);
            if (! used[i] && err < bestErr) { bestErr = err; best = i; }
        }
        if (best < ev.size())
        {
            used[best] = true;
            ++m.tp;
            const double ms = 1000.0 * static_cast<double> (bestErr) / sr;
            errSum += ms;
            m.maxErrMs = std::max (m.maxErrMs, ms);
        }
        else ++m.fn;
    }
    for (bool u : used) if (! u) ++m.fp;
    m.meanErrMs = m.tp ? errSum / static_cast<double> (m.tp) : 0.0;
    return m;
}

double f1 (const Match& m)
{
    const double p = m.tp + m.fp ? static_cast<double> (m.tp) / static_cast<double> (m.tp + m.fp) : 0.0;
    const double r = m.tp + m.fn ? static_cast<double> (m.tp) / static_cast<double> (m.tp + m.fn) : 0.0;
    return p + r > 0 ? 2 * p * r / (p + r) : 0.0;
}

bool hasBand (const OnsetEvent& e, std::size_t lo, std::size_t hi)
{
    for (std::size_t b = lo; b <= hi; ++b) if (e.bandMask & (1u << b)) return true;
    return false;
}
} // namespace

int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        // 1) Every drum alone, 400 ms apart, at three levels (0, -12, -24 dB).
        {
            std::vector<synth::Hit> hits;
            std::size_t pos = static_cast<std::size_t> (0.3 * sr);
            for (float g : { 1.0f, 0.25f, 0.063f })
                for (std::size_t d = 0; d < synth::numDrums; ++d)
                {
                    if (static_cast<synth::Drum> (d) == synth::Drum::crash) continue;   // long tail; tested in the loop
                    hits.push_back ({ static_cast<synth::Drum> (d), pos, g });
                    pos += static_cast<std::size_t> (0.4 * sr);
                }
            const auto x = synth::render (hits, sr, pos + static_cast<std::size_t> (sr));
            const auto ev = detect (x, sr);
            const Match m = match (trueEvents (hits, sr), ev, sr, 10.0);
            std::printf ("[%.0f Hz] isolated: tp %zu fp %zu fn %zu, timing mean %.2f ms max %.2f ms\n",
                         sr, m.tp, m.fp, m.fn, m.meanErrMs, m.maxErrMs);
            DSP_CHECK (m.fn == 0);
            DSP_CHECK (m.fp == 0);
            DSP_CHECK (m.maxErrMs <= 3.0);
        }

        // 2) Kick and closed hat on the same sample -> one event with a low band and a high band.
        {
            const std::size_t t0 = static_cast<std::size_t> (0.3 * sr);
            const std::vector<synth::Hit> hits { { synth::Drum::kick, t0, 1.0f }, { synth::Drum::closedHat, t0, 0.3f } };
            const auto ev = detect (synth::render (hits, sr, t0 + static_cast<std::size_t> (sr)), sr);
            DSP_CHECK (ev.size() == 1);
            if (ev.size() == 1)
            {
                DSP_CHECK (hasBand (ev[0], 0, 1));   // kick: 20-400 Hz
                DSP_CHECK (hasBand (ev[0], 4, 5));   // hat: 5-20 kHz
                DSP_CHECK (std::llabs (ev[0].sample - static_cast<std::int64_t> (t0)) <= static_cast<std::int64_t> (0.003 * sr));
            }
        }

        // 2b) A hat 5 ms after a kick joins the kick's event; the event is still reported in time.
        {
            const std::size_t t0 = static_cast<std::size_t> (0.3 * sr);
            const std::size_t t1 = t0 + static_cast<std::size_t> (0.005 * sr);
            const std::vector<synth::Hit> hits { { synth::Drum::kick, t0, 1.0f }, { synth::Drum::closedHat, t1, 0.3f } };
            lateBy = std::numeric_limits<std::int64_t>::min();
            const auto ev = detect (synth::render (hits, sr, t0 + static_cast<std::size_t> (sr)), sr);
            std::printf ("[%.0f Hz] kick + late hat: %zu event(s), reported %.1f ms vs promised delay\n",
                         sr, ev.size(), 1000.0 * static_cast<double> (lateBy) / sr);
            DSP_CHECK (lateBy <= 0);
        }

        // 3) Dense 4-bar loop with overlaps (kick + hats, clap on snare, crash, open hat, rimshot).
        {
            const auto hits = synth::denseLoop (sr, 4, static_cast<std::size_t> (0.2 * sr));
            const auto x = synth::render (hits, sr, static_cast<std::size_t> (9.0 * sr));
            const auto ev = detect (x, sr);
            const Match m = match (trueEvents (hits, sr), ev, sr, 10.0);
            std::printf ("[%.0f Hz] dense loop: tp %zu fp %zu fn %zu, F1 %.3f, timing mean %.2f ms max %.2f ms\n",
                         sr, m.tp, m.fp, m.fn, f1 (m), m.meanErrMs, m.maxErrMs);
            DSP_CHECK (f1 (m) >= 0.95);
        }

        // 4) Silence and steady tones produce no events.
        {
            const std::size_t n = static_cast<std::size_t> (2.0 * sr);
            DSP_CHECK (detect (std::vector<float> (n, 0.0f), sr).empty());
            auto s = dsptest::sine (100.0, sr, n);
            const auto h = dsptest::sine (3000.0, sr, n);
            for (std::size_t i = 0; i < n; ++i) s[i] = 0.5f * s[i] + 0.2f * h[i];
            const auto ev = detect (s, sr);
            // The very start of the tone is a legitimate onset; nothing after it.
            DSP_CHECK (ev.size() <= 1);
            for (const auto& e : ev) DSP_CHECK (e.sample < static_cast<std::int64_t> (0.02 * sr));
        }

        // 5) Report delay stays inside the 50 ms lookahead, and every event above kept to it.
        {
            DSP_CHECK (lateBy <= 0);
            OnsetDetector d;
            d.prepare (sr);
            DSP_CHECK (1000.0 * static_cast<double> (d.maxReportDelay()) / sr < 40.0);
        }
    }

    DSP_TEST_END;
}
