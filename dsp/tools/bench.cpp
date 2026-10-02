// File: dsp/tools/bench.cpp
// Offline benchmark of the real DrumEngine (no JUCE, no DAW):
//   1) detection accuracy per genre (the Puppet Show's five), drums alone and inside a music bed;
//   2) accuracy per hit type (each drum alone) with a confusion summary;
//   3) processing cost with more and more effects switched on, at 128 and 512 sample blocks.
// Audio comes from dsp/tests/synth_drums.h, so every number repeats exactly. Synthetic drums are a
// controlled measurement, not proof on real recordings: confirm at home with real tracks.
//
// Build:  cmake -S dsp -B build-dsp -DPLUGIN_DSP_BENCH=ON -DCMAKE_BUILD_TYPE=Release && cmake --build build-dsp --target drum_bench
//    or:  g++ -std=c++17 -O2 -DNDEBUG -I dsp/include -I dsp/tests -I dsp/tools dsp/tools/bench.cpp -o drum_bench
//
// Modes (docs/07-home-test-plan.md):
//   drum_bench                                   synthetic baseline (the numbers in docs/06-testing.md)
//   drum_bench --oneshots <folder>               every labelled one-shot in a sample-pack folder, alone
//   drum_bench --kits <folder> --genre <g>       beats built from your one-shots (g: rock pop edm house metal hip-hop)
//   drum_bench --loops <folder>                  loops without labels: what was detected, for checking by ear
//   drum_bench --export-synth <folder>           write the synthetic one-shots as WAVs (pipeline check)
// Options: --csv <file> (per-file / per-event detail), --kitsets <n> (kits mode, default 5), --sens <0..100>

#include <plugin_dsp/DrumEngine.h>
#include "synth_drums.h"
#include "wav_io.h"
#include "sample_packs.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <map>
#include <vector>

using namespace plugin_dsp;

static constexpr double SR = 48000.0;
static constexpr double TOL = 0.025;   // a detection within 25 ms of the true onset counts

static DrumType toPlugin (synth::Drum d)
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
        default:                     return DrumType::misc;   // toms have no own string: "misc"
    }
}

static const char* typeName (DrumType t)
{
    static const char* n[] = { "kick", "snare", "closed hat", "open hat", "crash", "ride", "rimshot", "clap", "misc" };
    return n[static_cast<std::size_t> (t)];
}

// ------------------------------------------------------------------ genre patterns
struct Song { std::string name; std::vector<synth::Hit> hits; std::size_t length; double bpm; };

static Song makeGenre (const std::string& g)
{
    Song s { g, {}, 0, 120.0 };
    synth::Rng rng (g.size() * 977u + 13u);
    auto vel = [&] (double lo, double hi) { return static_cast<float> (lo + (hi - lo) * 0.5 * (rng.next() + 1.0)); };
    double swing = 0.5;
    if (g == "rock") s.bpm = 120;
    if (g == "pop") s.bpm = 100;
    if (g == "edm") s.bpm = 128;
    if (g == "house") s.bpm = 124;
    if (g == "metal") s.bpm = 180;
    if (g == "hip-hop") { s.bpm = 88; swing = 0.58; }
    const double step = 60.0 / s.bpm / 4.0;
    const std::size_t lead = static_cast<std::size_t> (0.5 * SR);
    auto at = [&] (int bar, int s16)
    {
        double t = (bar * 16 + s16) * step;
        if (s16 % 2 == 1) t += (swing - 0.5) * 2.0 * step;   // swung 16ths
        return lead + static_cast<std::size_t> (t * SR);
    };
    auto add = [&] (synth::Drum d, int bar, int st, float v) { s.hits.push_back ({ d, at (bar, st), v }); };
    const int bars = 16;
    for (int b = 0; b < bars; ++b)
    {
        if (g == "rock")
        {
            add (synth::Drum::kick, b, 0, vel (0.8, 1.0)); add (synth::Drum::kick, b, 8, vel (0.75, 0.95));
            if (b % 2) add (synth::Drum::kick, b, 10, vel (0.6, 0.8));
            add (synth::Drum::snare, b, 4, vel (0.75, 0.95)); add (synth::Drum::snare, b, 12, vel (0.75, 0.95));
            for (int st = 0; st < 16; st += 2) add (synth::Drum::closedHat, b, st, vel (0.25, 0.45));
            if (b % 4 == 0) add (synth::Drum::crash, b, 0, vel (0.45, 0.6));
            if (b % 4 == 3) add (synth::Drum::openHat, b, 14, vel (0.3, 0.45));
        }
        else if (g == "pop")
        {
            add (synth::Drum::kick, b, 0, vel (0.8, 1.0)); add (synth::Drum::kick, b, 7, vel (0.6, 0.8)); add (synth::Drum::kick, b, 8, vel (0.75, 0.9));
            add (synth::Drum::snare, b, 4, vel (0.7, 0.9)); add (synth::Drum::snare, b, 12, vel (0.7, 0.9));
            add (synth::Drum::clap, b, 4, vel (0.35, 0.5)); add (synth::Drum::clap, b, 12, vel (0.35, 0.5));
            for (int st = 0; st < 16; ++st) add (synth::Drum::closedHat, b, st, vel (0.12, 0.3));
            if (b % 8 == 0) add (synth::Drum::crash, b, 0, vel (0.4, 0.55));
        }
        else if (g == "edm" || g == "house")
        {
            for (int st : { 0, 4, 8, 12 }) add (synth::Drum::kick, b, st, vel (0.9, 1.0));
            add (synth::Drum::clap, b, 4, vel (0.6, 0.8)); add (synth::Drum::clap, b, 12, vel (0.6, 0.8));
            for (int st : { 2, 6, 10, 14 }) add (synth::Drum::openHat, b, st, vel (0.3, 0.45));
            for (int st = 1; st < 16; st += 2) add (synth::Drum::closedHat, b, st, vel (0.1, 0.22));
            if (b % 8 == 0) add (synth::Drum::crash, b, 0, vel (0.45, 0.6));
        }
        else if (g == "metal")
        {
            for (int st = 0; st < 16; ++st) add (synth::Drum::kick, b, st, vel (0.65, 0.9));
            add (synth::Drum::snare, b, 4, vel (0.85, 1.0)); add (synth::Drum::snare, b, 12, vel (0.85, 1.0));
            for (int st = 0; st < 16; st += 2) add (synth::Drum::ride, b, st, vel (0.3, 0.45));
            if (b % 2 == 0) add (synth::Drum::crash, b, 0, vel (0.5, 0.65));
            if (b % 4 == 3) add (synth::Drum::tom, b, 14, vel (0.6, 0.8));
        }
        else   // hip-hop
        {
            add (synth::Drum::kick, b, 0, vel (0.85, 1.0)); add (synth::Drum::kick, b, 3, vel (0.6, 0.8)); add (synth::Drum::kick, b, 10, vel (0.75, 0.9));
            add (synth::Drum::snare, b, 4, vel (0.75, 0.95)); add (synth::Drum::snare, b, 12, vel (0.75, 0.95));
            add (synth::Drum::clap, b, 12, vel (0.3, 0.45));
            for (int st = 0; st < 16; st += 2) add (synth::Drum::closedHat, b, st, vel (0.2, 0.4));
            if (b % 2) add (synth::Drum::rimshot, b, 7, vel (0.35, 0.5));
        }
    }
    s.length = at (bars, 0) + static_cast<std::size_t> (1.5 * SR);
    return s;
}

/** A simple music bed: a bass line and sustained chords, so detection has to cope with a full mix. */
static std::vector<float> musicBed (std::size_t len, double bpm, float gain)
{
    std::vector<float> out (len, 0.0f);
    const double beat = 60.0 / bpm;
    const double roots[] = { 55.0, 43.65, 49.0, 41.2 };          // A1 F1 G1 E1, one per bar
    double phB = 0.0, ph[3] = { 0, 0, 0 };
    for (std::size_t i = 0; i < len; ++i)
    {
        const double t = static_cast<double> (i) / SR;
        const int bar = static_cast<int> (t / (4.0 * beat));
        const double f = roots[bar % 4];
        const double inBeat = std::fmod (t, beat) / beat;
        const double bassEnv = std::exp (-3.0 * inBeat);           // plucked bass on every beat
        phB += 2.0 * 3.14159265358979 * f / SR;
        double v = 0.55 * bassEnv * (std::sin (phB) + 0.3 * std::sin (2.0 * phB));
        const double ratios[] = { 4.0, 5.0, 6.0 };                 // a soft major triad, 2 octaves up
        for (int k = 0; k < 3; ++k)
        {
            ph[k] += 2.0 * 3.14159265358979 * f * ratios[k] / SR;
            v += 0.12 * (std::sin (ph[k]) + 0.25 * std::sin (3.0 * ph[k]));
        }
        out[i] = static_cast<float> (gain * v);
    }
    return out;
}

// ------------------------------------------------------------------ run the engine, collect detections
struct Detection { double t; DrumType type; };

static int gSensitivity = 50;
static std::vector<Detection> detect (const std::vector<float>& mono, std::uint32_t allowed = 0, std::size_t block = 512, int sensitivity = -1)
{
    auto e = std::make_unique<DrumEngine>();
    e->prepare (SR);
    if (allowed != 0) e->setAllowedTypes (allowed);
    DrumParams p;
    p.global.sensitivity = static_cast<float> (sensitivity < 0 ? gSensitivity : sensitivity);
    e->setParams (p);
    std::vector<float> l (mono), r (mono);
    std::vector<Detection> out;
    for (std::size_t i = 0; i < l.size(); i += block)
    {
        const std::size_t n = std::min (block, l.size() - i);
        e->setParams (p);
        e->process (l.data() + i, r.data() + i, n);
        ShadowEvent ev;
        while (e->shadowEvents().pop (ev))
            out.push_back ({ static_cast<double> (ev.sample) / SR, ev.type });   // already on the input timeline
    }
    return out;
}

// ------------------------------------------------------------------ scoring
struct Score
{
    int onsets = 0, onsetHits = 0, detections = 0, falseOnsets = 0, onsetLabelOk = 0;
    std::array<int, numDrumTypes> truth {}, found {}, said {}, saidRight {};
    std::array<std::array<int, numDrumTypes + 1>, numDrumTypes> confusion {};   // [truth][said or "missed"]
};

static Score score (const std::vector<synth::Hit>& hits, const std::vector<Detection>& det)
{
    Score s;
    s.detections = static_cast<int> (det.size());
    // onsets: true hits closer than 10 ms count as one onset (layers)
    std::vector<double> onsets;
    {
        std::vector<double> ts;
        for (const auto& h : hits) ts.push_back (static_cast<double> (h.sample) / SR);
        std::sort (ts.begin(), ts.end());
        for (double t : ts) if (onsets.empty() || t - onsets.back() > 0.010) onsets.push_back (t);
    }
    s.onsets = static_cast<int> (onsets.size());
    std::vector<bool> used (det.size(), false);
    for (double t : onsets)
    {
        bool hit = false;
        for (std::size_t k = 0; k < det.size(); ++k)
            if (std::fabs (det[k].t - t) <= TOL) { hit = true; break; }
        if (hit) ++s.onsetHits;
    }
    for (double t : onsets)    // fair for one label per onset: is it one of the drums really there?
    {
        int nearest = -1; double nd = 1e9;
        for (std::size_t k = 0; k < det.size(); ++k)
            if (std::fabs (det[k].t - t) <= TOL && std::fabs (det[k].t - t) < nd) { nd = std::fabs (det[k].t - t); nearest = static_cast<int> (k); }
        if (nearest < 0) continue;
        for (const auto& h : hits)
            if (std::fabs (static_cast<double> (h.sample) / SR - t) <= 0.010 && toPlugin (h.drum) == det[static_cast<std::size_t> (nearest)].type)
            { ++s.onsetLabelOk; break; }
    }
    for (const auto& d : det)
    {
        bool near = false;
        for (double t : onsets) if (std::fabs (d.t - t) <= TOL) { near = true; break; }
        if (! near) ++s.falseOnsets;
    }
    // per type: a true hit is "found" if a detection of the same type is within TOL
    std::vector<bool> claimed (det.size(), false);
    for (const auto& h : hits)
    {
        const auto tt = toPlugin (h.drum);
        const auto ti = static_cast<std::size_t> (tt);
        const double t = static_cast<double> (h.sample) / SR;
        ++s.truth[ti];
        int best = -1; double bestDt = 1e9; int nearestAny = -1; double nearestDt = 1e9;
        for (std::size_t k = 0; k < det.size(); ++k)
        {
            const double dt = std::fabs (det[k].t - t);
            if (dt > TOL) continue;
            if (det[k].type == tt && ! claimed[k] && dt < bestDt) { best = static_cast<int> (k); bestDt = dt; }
            if (dt < nearestDt) { nearestAny = static_cast<int> (k); nearestDt = dt; }
        }
        if (best >= 0) { claimed[static_cast<std::size_t> (best)] = true; ++s.found[ti]; ++s.confusion[ti][ti]; }
        else if (nearestAny >= 0) ++s.confusion[ti][static_cast<std::size_t> (det[static_cast<std::size_t> (nearestAny)].type)];
        else ++s.confusion[ti][numDrumTypes];
    }
    for (std::size_t k = 0; k < det.size(); ++k)
    {
        const auto di = static_cast<std::size_t> (det[k].type);
        ++s.said[di];
        if (claimed[k]) ++s.saidRight[di];
    }
    return s;
}

static double pct (int a, int b) { return b > 0 ? 100.0 * a / b : 0.0; }

static void printScore (const char* label, const Score& s)
{
    const double rec = pct (s.onsetHits, s.onsets);
    const double prec = s.detections > 0 ? 100.0 * (s.detections - s.falseOnsets) / s.detections : 0.0;
    const double f1 = rec + prec > 0 ? 2.0 * rec * prec / (rec + prec) : 0.0;
    int truthAll = 0, foundAll = 0;
    for (std::size_t i = 0; i < numDrumTypes; ++i) { truthAll += s.truth[i]; foundAll += s.found[i]; }
    std::printf ("  %-26s onsets found %5.1f%%  false %3d  F1 %5.1f%%  | label is a drum really there %5.1f%%  | every layered hit typed %5.1f%%\n",
                 label, rec, s.falseOnsets, f1, pct (s.onsetLabelOk, s.onsetHits), pct (foundAll, truthAll));
    std::printf ("  %-26s", "");
    for (std::size_t i = 0; i < numDrumTypes; ++i)
        if (s.truth[i] > 0) std::printf (" %s %.0f%%", typeName (static_cast<DrumType> (i)), pct (s.found[i], s.truth[i]));
    std::printf ("\n");
}

// ------------------------------------------------------------------ performance
static DrumParams effectsLevel (int level)
{
    DrumParams p;
    if (level >= 1)   // quick mix knobs moved
    {
        p.quick.kickVolume = 3; p.quick.kickPunch = 40; p.quick.kickWeight = 30;
        p.quick.snareVolume = 2; p.quick.snareSnap = 35; p.quick.snareFat = 25;
        p.quick.hatsVolume = -2; p.quick.hatsTone = 20; p.quick.hatsTame = 30;
        p.global.punch = 30;
    }
    if (level >= 2)   // every drum's deep edit in use
    {
        p.kick = { 2, 40, 30, 20, 25, 50, 30, 20, 3, -2, 800, 2 };
        p.snare.level = 1; p.snare.crack = 30; p.snare.body = 25; p.snare.decay = 20; p.snare.comp = 50; p.snare.sat = 30;
        p.snare.eqLow = 2; p.snare.eqMid = -2; p.snare.eqHigh = 3; p.snare.rockify = 40; p.snare.popify = 20;
        p.closedHat = { 0, 20, 30, 20, 40, 120 };
        p.openHat = { 0, 15, 20, 20, 120, 20, 30 };
        p.crash = { 0, 10, 20, 15, 120, 30, 200 };
        p.ride = { 0, 30, 20, 20, 10, 120 };
        p.rim = { 0, 30, 20, 10, 20, 20 };
        p.clap = { 0, 30, 20, 10, 20, 120, 40, 20 };
        p.misc = { 0, 10, 20, 10 };
        p.advanced = { 30, 30, 40, 16000 };
        p.global.clip = 30;
    }
    if (level >= 3)   // plus the Puppet Show
    {
        p.puppetShow.enabled = true;
        p.puppetShow.genre = DrumParams::PuppetShow::Genre::rock;
        p.puppetShow.depth = 100; p.puppetShow.bass = 30; p.puppetShow.mud = 30; p.puppetShow.presence = 30; p.puppetShow.high = 20;
    }
    return p;
}

static void perf (const std::vector<float>& mono)
{
    const char* names[] = { "detection + routing only", "+ quick mix", "+ every drum's effects", "+ Puppet Show" };
    for (std::size_t block : { std::size_t (128), std::size_t (512) })
    {
        std::printf ("\n  block %zu samples (%.2f ms budget per block at 48 kHz, stereo)\n", block, 1000.0 * static_cast<double> (block) / SR);
        for (int lvl = 0; lvl < 4; ++lvl)
        {
            auto e = std::make_unique<DrumEngine>();
            e->prepare (SR);
            const auto p = effectsLevel (lvl);
            std::vector<float> l (mono), r (mono);
            double worst = 0.0;
            std::vector<double> times;
            const auto t0 = std::chrono::steady_clock::now();
            for (std::size_t i = 0; i < l.size(); i += block)
            {
                const std::size_t n = std::min (block, l.size() - i);
                const auto b0 = std::chrono::steady_clock::now();
                e->setParams (p);
                e->process (l.data() + i, r.data() + i, n);
                ShadowEvent ev;
                while (e->shadowEvents().pop (ev)) {}
                const double bt = std::chrono::duration<double> (std::chrono::steady_clock::now() - b0).count();
                worst = std::max (worst, bt);
                times.push_back (bt);
            }
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const double audio = static_cast<double> (l.size()) / SR;
            std::sort (times.begin(), times.end());
            const double p99 = times[static_cast<std::size_t> (0.99 * static_cast<double> (times.size() - 1))];
            const double budget = static_cast<double> (block) / SR;
            std::printf ("    %-26s %5.2f%% of one core  (%.0fx real time)  99%% of blocks under %.0f%% of budget, worst %.0f%%\n",
                         names[lvl], 100.0 * secs / audio, audio / secs, 100.0 * p99 / budget, 100.0 * worst / budget);
        }
    }
}


// ------------------------------------------------------------------ real sample packs
static std::FILE* gCsv = nullptr;

struct Loaded { packs::Sample info; std::vector<float> audio; };

static std::vector<Loaded> loadOneShots (const std::string& root, bool verbose)
{
    std::vector<Loaded> out;
    int unlabelled = 0, loops = 0, failed = 0, tooLong = 0;
    for (const auto& smp : packs::scan (root))
    {
        if (smp.loop) { ++loops; continue; }
        if (! smp.labelled) { ++unlabelled; continue; }
        auto a = wav::read (smp.path);
        if (! a.ok) { ++failed; if (verbose) std::printf ("  skip (%s): %s\n", a.error.c_str(), smp.path.c_str()); continue; }
        auto mono = wav::resample (a.mono, a.sampleRate, SR);
        if (mono.size() > static_cast<std::size_t> (4.0 * SR)) { ++tooLong; continue; }   // a loop hiding in a one-shot folder
        float peak = 0.0f;
        for (float v : mono) peak = std::max (peak, std::fabs (v));
        if (peak < 1e-4f) { ++failed; continue; }
        for (float& v : mono) v /= peak;
        out.push_back ({ smp, std::move (mono) });
    }
    std::printf ("  found %zu labelled one-shots (skipped: %d unlabelled, %d in loop folders, %d longer than 4 s, %d unreadable)\n",
                 out.size(), unlabelled, loops, tooLong, failed);
    return out;
}

/** Each one-shot alone, at three velocities, through the engine (all types allowed). */
static int oneShotsMode (const std::string& root)
{
    std::printf ("ONE-SHOTS: %s\n", root.c_str());
    const auto shots = loadOneShots (root, false);
    if (shots.empty()) return 1;
    std::map<std::string, std::array<int, numDrumTypes + 1>> byGroup;          // group -> counts per detected type (+missed)
    std::map<std::string, std::pair<int, int>> byPack;                          // pack -> right, total
    std::vector<std::pair<double, std::string>> worst;
    if (gCsv) std::fprintf (gCsv, "file,pack,group,truth,gain,detected,right\n");
    for (const auto& s : shots)
    {
        int right = 0;
        for (float gain : { 0.35f, 0.65f, 1.0f })
        {
            std::vector<float> audio (static_cast<std::size_t> (0.5 * SR), 0.0f);
            for (float v : s.audio) audio.push_back (v * gain);
            audio.resize (audio.size() + static_cast<std::size_t> (1.0 * SR), 0.0f);
            const auto det = detect (audio);
            int nearest = -1; double nd = 1e9;
            for (std::size_t k = 0; k < det.size(); ++k)
                if (std::fabs (det[k].t - 0.5) <= TOL && std::fabs (det[k].t - 0.5) < nd) { nd = std::fabs (det[k].t - 0.5); nearest = static_cast<int> (k); }
            const std::size_t said = nearest < 0 ? numDrumTypes : static_cast<std::size_t> (det[static_cast<std::size_t> (nearest)].type);
            ++byGroup[s.info.group][said];
            const bool ok = said == static_cast<std::size_t> (s.info.type);
            right += ok ? 1 : 0;
            auto& bp = byPack[s.info.pack];
            bp.first += ok ? 1 : 0;
            ++bp.second;
            if (gCsv) std::fprintf (gCsv, "\"%s\",\"%s\",%s,%s,%.2f,%s,%d\n", s.info.path.c_str(), s.info.pack.c_str(), s.info.group.c_str(),
                                    typeName (s.info.type), gain, said == numDrumTypes ? "missed" : typeName (static_cast<DrumType> (said)), ok ? 1 : 0);
        }
        if (right < 3) worst.push_back ({ right / 3.0, s.info.group + "  " + s.info.path });
    }
    std::printf ("\n  per sound group (3 velocities each)     right    mistaken for\n");
    for (const auto& [group, counts] : byGroup)
    {
        int total = 0;
        for (int c : counts) total += c;
        DrumType truth = DrumType::misc; std::string g2;
        packs::labelFrom (group, truth, g2);
        const auto ti = static_cast<std::size_t> (truth);
        std::printf ("  %-12s %5d sounds              %5.1f%%  ", group.c_str(), total / 3, pct (counts[ti], total));
        for (std::size_t j = 0; j <= numDrumTypes; ++j)
            if (j != ti && counts[j] > 0) std::printf (" %s %.0f%%", j == numDrumTypes ? "missed" : typeName (static_cast<DrumType> (j)), pct (counts[j], total));
        std::printf ("\n");
    }
    std::printf ("\n  per pack                               right\n");
    for (const auto& [pack, rt] : byPack) std::printf ("  %-38s %5.1f%%  (%d tests)\n", pack.c_str(), pct (rt.first, rt.second), rt.second);
    std::sort (worst.begin(), worst.end());
    std::printf ("\n  hardest sounds (right at 0 of 3 velocities first; up to 25):\n");
    for (std::size_t i = 0; i < worst.size() && i < 25; ++i) std::printf ("    %d/3  %s\n", static_cast<int> (worst[i].first * 3.0 + 0.5), worst[i].second.c_str());
    return 0;
}

/** Genre beats built from real one-shots: same patterns and scoring as the synthetic test. */
static int kitsMode (const std::string& root, const std::string& genre, int kitsets)
{
    std::printf ("KITS: %s  genre %s  (%d different kits)\n", root.c_str(), genre.c_str(), kitsets);
    const auto shots = loadOneShots (root, false);
    std::map<DrumType, std::vector<const Loaded*>> pool;
    for (const auto& s : shots)
        if (s.info.group != "808" || genre == "hip-hop") pool[s.info.type].push_back (&s);
    const auto song = makeGenre (genre);
    std::uint32_t kit = 0;
    for (const auto& h : song.hits) if (! pool[toPlugin (h.drum)].empty()) kit |= 1u << static_cast<unsigned> (toPlugin (h.drum));
    for (const auto& h : song.hits)
        if (pool[toPlugin (h.drum)].empty()) { std::printf ("  note: no %s samples found - those hits are left out\n", typeName (toPlugin (h.drum))); break; }
    if (gCsv) std::fprintf (gCsv, "kitset,mix,onsets_found,false_onsets,label_ok,layered_typed\n");
    for (int k = 0; k < kitsets; ++k)
    {
        synth::Rng rng (static_cast<std::uint32_t> (1234 + 77 * k));
        std::map<DrumType, const Loaded*> chosen;                // one sound per drum type per kit, like a real kit
        for (auto& [t, v] : pool) if (! v.empty()) chosen[t] = v[static_cast<std::size_t> ((rng.next() + 1.0) * 0.5 * static_cast<double> (v.size())) % v.size()];
        std::vector<synth::Hit> hits;
        std::vector<float> drums (song.length, 0.0f);
        for (const auto& h : song.hits)
        {
            const auto it = chosen.find (toPlugin (h.drum));
            if (it == chosen.end()) continue;
            hits.push_back (h);
            const auto& a = it->second->audio;
            for (std::size_t i = 0; i < a.size() && h.sample + i < drums.size(); ++i) drums[h.sample + i] += h.gain * a[i];
        }
        std::printf ("\n kit %d:", k + 1);
        for (const auto& [t, l] : chosen) std::printf ("  %s=%s", typeName (t), std::filesystem::path (l->info.path).stem().string().c_str());
        std::printf ("\n");
        const auto sA = score (hits, detect (drums));
        printScore ("drums alone", sA);
        auto bed = musicBed (song.length, song.bpm, 0.25f);
        std::vector<float> mix (drums);
        for (std::size_t i = 0; i < mix.size(); ++i) mix[i] = 0.8f * mix[i] + bed[i];
        const auto sB = score (hits, detect (mix, kit));
        printScore ("full mix, only its kit", sB);
        for (const auto* sc : { &sA, &sB })
            if (gCsv)
            {
                int ta = 0, fa = 0;
                for (std::size_t i = 0; i < numDrumTypes; ++i) { ta += sc->truth[i]; fa += sc->found[i]; }
                std::fprintf (gCsv, "%d,%s,%.1f,%d,%.1f,%.1f\n", k + 1, sc == &sA ? "alone" : "mix", pct (sc->onsetHits, sc->onsets), sc->falseOnsets,
                              pct (sc->onsetLabelOk, sc->onsetHits), pct (fa, ta));
            }
    }
    return 0;
}

/** Loops have no labels: list what was detected (per second and by type) for listening checks. */
static int loopsMode (const std::string& root)
{
    std::printf ("LOOPS: %s  (no ground truth: compare with your ears; per-event times in --csv)\n\n", root.c_str());
    if (gCsv) std::fprintf (gCsv, "file,time_s,type\n");
    int files = 0;
    for (const auto& smp : packs::scan (root))
    {
        auto a = wav::read (smp.path);
        if (! a.ok) continue;
        auto mono = wav::resample (a.mono, a.sampleRate, SR);
        mono.insert (mono.begin(), static_cast<std::size_t> (0.5 * SR), 0.0f);   // the detector needs audio before the first hit
        auto det = detect (mono);
        for (auto& d : det) d.t -= 0.5;
        mono.erase (mono.begin(), mono.begin() + static_cast<std::ptrdiff_t> (0.5 * SR));
        std::array<int, numDrumTypes> n {};
        for (const auto& d : det) ++n[static_cast<std::size_t> (d.type)];
        const double secs = static_cast<double> (mono.size()) / SR;
        std::printf ("  %-60s %5.1f s  %5.1f hits/s ", std::filesystem::path (smp.path).filename().string().substr (0, 60).c_str(), secs, static_cast<double> (det.size()) / std::max (secs, 0.1));
        for (std::size_t i = 0; i < numDrumTypes; ++i) if (n[i] > 0) std::printf (" %s %d", typeName (static_cast<DrumType> (i)), n[i]);
        std::printf ("\n");
        if (gCsv) for (const auto& d : det) std::fprintf (gCsv, "\"%s\",%.4f,%s\n", smp.path.c_str(), d.t, typeName (d.type));
        ++files;
    }
    std::printf ("\n  %d loops\n", files);
    return files > 0 ? 0 : 1;
}

/** The synthetic one-shots as WAVs, named like a sample pack (checks the WAV path end to end). */
static int exportSynth (const std::string& dir)
{
    std::filesystem::create_directories (dir);
    const char* names[] = { "Kick", "Snare", "Closed Hat", "Open Hat", "Crash", "Ride", "Rimshot", "Clap", "Tom" };
    for (std::size_t d = 0; d < synth::numDrums; ++d)
        for (int k = 0; k < 5; ++k)
        {
            std::vector<float> a (static_cast<std::size_t> (1.5 * SR), 0.0f);
            synth::renderHit (a, 0, static_cast<synth::Drum> (d), SR, 0.9f, static_cast<std::uint32_t> (500 + 31 * d + 7 * static_cast<std::size_t> (k)));
            wav::write16 (dir + "/Synth " + names[d] + " " + std::to_string (k + 1) + ".wav", a, SR);
        }
    std::printf ("wrote %zu one-shots to %s\n", synth::numDrums * 5, dir.c_str());
    return 0;
}

static int synthBaseline();

int main (int argc, char** argv)
{
    std::string mode, folder, genre = "hip-hop", csv;
    int kitsets = 5;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string (argv[++i]) : std::string(); };
        if (a == "--oneshots" || a == "--kits" || a == "--loops" || a == "--export-synth") { mode = a; folder = next(); }
        else if (a == "--genre") genre = next();
        else if (a == "--csv") csv = next();
        else if (a == "--kitsets") kitsets = std::max (1, std::atoi (next().c_str()));
        else if (a == "--sens") gSensitivity = std::clamp (std::atoi (next().c_str()), 0, 100);
        else { std::printf ("unknown option %s (see the top of dsp/tools/bench.cpp)\n", a.c_str()); return 2; }
    }
    if (! csv.empty()) gCsv = std::fopen (csv.c_str(), "w");
    int rc = 0;
    if (mode.empty()) rc = synthBaseline();
    else if (mode == "--oneshots") rc = oneShotsMode (folder);
    else if (mode == "--kits") rc = kitsMode (folder, genre, kitsets);
    else if (mode == "--loops") rc = loopsMode (folder);
    else rc = exportSynth (folder);
    if (gCsv) std::fclose (gCsv);
    return rc;
}

static int synthBaseline()

{
    std::printf ("THE DRUM MASTER - offline benchmark (synthetic drums, 48 kHz, sensitivity 50, all types allowed)\n");

    std::printf ("\n1) DETECTION BY GENRE (16 bars each; tolerance +/-25 ms)\n");
    Song keep;
    std::vector<float> keepMix;
    for (const char* g : { "rock", "pop", "edm", "metal", "hip-hop" })
    {
        const auto song = makeGenre (g);
        auto drums = synth::render (song.hits, SR, song.length);
        std::printf ("\n %s (%.0f bpm, %zu hits)\n", g, song.bpm, song.hits.size());
        printScore ("drums alone", score (song.hits, detect (drums)));
        auto bed = musicBed (song.length, song.bpm, 0.25f);   // about -12 dB under the drums
        std::vector<float> mix (drums);
        for (std::size_t i = 0; i < mix.size(); ++i) mix[i] = 0.8f * mix[i] + bed[i];
        printScore ("in a full mix", score (song.hits, detect (mix)));
        std::uint32_t kit = 0;
        for (const auto& h : song.hits) kit |= 1u << static_cast<unsigned> (toPlugin (h.drum));
        printScore ("full mix, only its kit", score (song.hits, detect (mix, kit)));
        if (std::string (g) == "rock") { keep = song; keepMix = mix; }
    }

    std::printf ("\n2) EACH HIT TYPE ALONE (40 hits, velocities 0.3..1.0, 0.45 s apart)\n");
    std::printf ("   type        right    mistaken for (share of all its hits)\n");
    for (std::size_t d = 0; d < synth::numDrums; ++d)
    {
        std::vector<synth::Hit> hits;
        synth::Rng rng (static_cast<std::uint32_t> (31 + d));
        for (int k = 0; k < 40; ++k)
            hits.push_back ({ static_cast<synth::Drum> (d), static_cast<std::size_t> ((0.5 + 0.45 * k) * SR),
                              static_cast<float> (0.3 + 0.35 * (rng.next() + 1.0)) });
        const auto audio = synth::render (hits, SR, static_cast<std::size_t> ((0.5 + 0.45 * 40 + 1.5) * SR));
        const auto s = score (hits, detect (audio));
        const auto ti = static_cast<std::size_t> (toPlugin (static_cast<synth::Drum> (d)));
        std::printf ("   %-11s %5.1f%%  ", synth::name (static_cast<synth::Drum> (d)), pct (s.found[ti], s.truth[ti]));
        for (std::size_t j = 0; j <= numDrumTypes; ++j)
        {
            if (j == ti) continue;
            const int c = s.confusion[ti][j];
            if (c > 0) std::printf (" %s %.0f%%", j == numDrumTypes ? "missed" : typeName (static_cast<DrumType> (j)), pct (c, s.truth[ti]));
        }
        std::printf ("\n");
    }

    std::printf ("\n3) PERFORMANCE (rock in a full mix, %.0f s of audio)\n", static_cast<double> (keepMix.size()) / SR);
    perf (keepMix);
    return 0;
}
