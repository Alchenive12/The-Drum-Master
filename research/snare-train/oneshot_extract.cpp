// File: research/snare-train/oneshot_extract.cpp
// Turns labelled one-shots from sample-pack folders into training/test rows for the hit classifier,
// using the plugin's own OnsetDetector + HitFeatures (so the rows match what the plugin sees).
// Labels come from file/folder names exactly like dsp/tools/bench.cpp (dsp/tools/sample_packs.h).
// Runs on the user's PC; the audio never leaves it, only feature rows (numbers) are written.
//
//   oneshot_extract list <root> <list.tsv>                 append labelled one-shots under <root>
//   oneshot_extract extract <list.tsv> <start> <count> <out.csv>
//        each sound alone at 3 velocities (0.35, 0.65, 1.0) after 0.5 s of silence, like the bench
//   oneshot_extract beat <list.tsv> <seed> <count> <out.csv>
//        2-bar beats built from one pack (a kit), every detected onset labelled (multi-label)
//   oneshot_extract mix <list.tsv> <seed> <count> <out.csv>
//        layered hits: a snare/clap/rim on top of a kick or hat (both labelled), random gains/offset
//
// Build: g++ -O2 -std=c++17 -I../../dsp/include -I../../dsp/tools oneshot_extract.cpp -o oneshot_extract
#include <plugin_dsp/DrumTypes.h>
#include <plugin_dsp/HitFeatures.h>
#include <plugin_dsp/OnsetDetector.h>

#include "sample_packs.h"
#include "wav_io.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace plugin_dsp;
static constexpr double SR = 48000.0;

struct Item { std::string path, pack, group; int type = 8; };

static std::vector<Item> readList (const std::string& file)
{
    std::vector<Item> out;
    std::ifstream f (file);
    std::string line;
    while (std::getline (f, line))
    {
        std::stringstream ss (line);
        Item it;
        std::string t;
        std::getline (ss, it.path, '\t'); std::getline (ss, it.pack, '\t'); std::getline (ss, it.group, '\t'); std::getline (ss, t, '\t');
        it.type = std::atoi (t.c_str());
        out.push_back (it);
    }
    return out;
}

static bool load (const std::string& path, std::vector<float>& mono)
{
    auto a = wav::read (path);
    if (! a.ok) return false;
    mono = wav::resample (a.mono, a.sampleRate, SR);
    if (mono.size() > static_cast<std::size_t> (4.0 * SR)) return false;     // a loop hiding in a one-shot folder
    float peak = 0.0f;
    for (float v : mono) peak = std::max (peak, std::fabs (v));
    if (peak < 1e-4f) return false;
    for (float& v : mono) v /= peak;
    return true;
}

/** Features of the detection nearest to `at` (within 25 ms); false if nothing was detected there. */
static bool featuresAt (const std::vector<float>& audio, std::int64_t at, std::vector<float>& out)
{
    OnsetDetector det; det.prepare (SR);
    HitFeatures feat; feat.prepare (SR);
    std::deque<std::int64_t> pending;
    const auto tol = static_cast<std::int64_t> (0.025 * SR);
    std::int64_t bestErr = tol + 1;
    OnsetEvent e;
    auto take = [&] (std::int64_t onset) {
        const auto err = std::llabs (onset - at);
        if (err < bestErr) { bestErr = err; const auto v = feat.extract (onset); out.assign (v.begin(), v.end()); }
    };
    auto push = [&] (float v) {
        if (det.processSample (v, e)) pending.push_back (e.sample);
        feat.pushSample (v);
        while (! pending.empty() && feat.samplesSeen() > pending.front() + feat.samplesAfterOnset())
        {
            take (pending.front());
            pending.pop_front();
        }
    };
    for (float v : audio) push (v);
    for (int i = 0; i < static_cast<int> (0.3 * SR); ++i) push (0.0f);
    return bestErr <= tol;
}

static void header (std::FILE* out)
{
    std::fprintf (out, "idx,idx2,gain,det");
    for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",y%zu", t);
    for (std::size_t f = 0; f < HitFeatures::numFeatures; ++f) std::fprintf (out, ",f%zu", f);
    std::fprintf (out, "\n");
}

static void row (std::FILE* out, long idx, long idx2, float gain, bool ok, std::uint32_t labels, const std::vector<float>& v)
{
    std::fprintf (out, "%ld,%ld,%.2f,%d", idx, idx2, static_cast<double> (gain), ok ? 1 : 0);
    for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",%u", (labels >> t) & 1u);
    for (std::size_t f = 0; f < HitFeatures::numFeatures; ++f) std::fprintf (out, ",%.2f", ok ? static_cast<double> (v[f]) : 0.0);
    std::fprintf (out, "\n");
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "usage: see the file header\n"); return 1; }
    const std::string mode = argv[1];
    if (mode == "list")
    {
        std::FILE* out = std::fopen (argv[3], "a");
        int n = 0, skipped = 0;
        for (const auto& s : packs::scan (argv[2]))
        {
            if (s.loop || ! s.labelled) { ++skipped; continue; }
            std::fprintf (out, "%s\t%s\t%s\t%d\n", s.path.c_str(), s.pack.c_str(), s.group.c_str(), static_cast<int> (s.type));
            ++n;
        }
        std::fclose (out);
        std::fprintf (stderr, "%s: %d labelled one-shots (%d loops/unlabelled skipped)\n", argv[2], n, skipped);
        return 0;
    }
    if (argc < 6) return 1;
    const auto items = readList (argv[2]);
    std::FILE* out = std::fopen (argv[5], "w");
    header (out);
    std::vector<float> a, b, v;
    if (mode == "extract")
    {
        const long start = std::atol (argv[3]), count = std::atol (argv[4]);
        for (long i = start; i < std::min<long> (start + count, static_cast<long> (items.size())); ++i)
        {
            if (! load (items[static_cast<std::size_t> (i)].path, a)) { row (out, i, -1, 0.0f, false, 0, v); continue; }
            for (float gain : { 0.35f, 0.65f, 1.0f })
            {
                std::vector<float> audio (static_cast<std::size_t> (0.5 * SR), 0.0f);
                for (float s : a) audio.push_back (s * gain);
                audio.resize (audio.size() + static_cast<std::size_t> (0.5 * SR), 0.0f);
                const bool ok = featuresAt (audio, static_cast<std::int64_t> (0.5 * SR), v);
                row (out, i, -1, gain, ok, 1u << static_cast<unsigned> (items[static_cast<std::size_t> (i)].type), v);
            }
        }
    }
    else if (mode == "mix")
    {
        std::mt19937 rng (static_cast<std::uint32_t> (std::atol (argv[3])));
        const long count = std::atol (argv[4]);
        std::vector<long> tops, bottoms;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto t = static_cast<DrumType> (items[i].type);
            if (t == DrumType::snare || t == DrumType::clap || t == DrumType::rimshot) tops.push_back (static_cast<long> (i));
            if (t == DrumType::kick || t == DrumType::closedHat || t == DrumType::openHat) bottoms.push_back (static_cast<long> (i));
        }
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        for (long k = 0; k < count && ! tops.empty() && ! bottoms.empty(); ++k)
        {
            const long i = tops[rng() % tops.size()], j = bottoms[rng() % bottoms.size()];
            if (! load (items[static_cast<std::size_t> (i)].path, a) || ! load (items[static_cast<std::size_t> (j)].path, b)) continue;
            const float ga = 0.4f + 0.6f * u (rng), gb = 0.25f + 0.75f * u (rng);
            const auto off = static_cast<std::size_t> (u (rng) * 0.004 * SR);     // up to 4 ms apart (layered / flam)
            std::vector<float> audio (static_cast<std::size_t> (0.5 * SR) + std::max (a.size(), b.size() + off) + static_cast<std::size_t> (0.5 * SR), 0.0f);
            const auto o = static_cast<std::size_t> (0.5 * SR);
            for (std::size_t n = 0; n < a.size(); ++n) audio[o + n] += a[n] * ga;
            for (std::size_t n = 0; n < b.size(); ++n) audio[o + off + n] += b[n] * gb;
            const bool ok = featuresAt (audio, static_cast<std::int64_t> (o), v);
            const std::uint32_t labels = (1u << static_cast<unsigned> (items[static_cast<std::size_t> (i)].type))
                                       | (1u << static_cast<unsigned> (items[static_cast<std::size_t> (j)].type));
            row (out, i, j, ga, ok, labels, v);
        }
    }
    else if (mode == "beat")
    {
        // 2-bar beats from one pack's sounds (a kit), labelled per onset: hats between and under
        // snares, snare tails over hats, kicks under claps - the context isolated one-shots lack.
        std::mt19937 rng (static_cast<std::uint32_t> (std::atol (argv[3])));
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        const long count = std::atol (argv[4]);
        std::map<std::string, std::array<std::vector<long>, numDrumTypes>> kits;
        for (std::size_t i = 0; i < items.size(); ++i)
            kits[items[i].pack][static_cast<std::size_t> (items[i].type)].push_back (static_cast<long> (i));
        std::vector<std::string> usable;
        auto has = [&] (const std::string& p, DrumType t) { return ! kits[p][static_cast<std::size_t> (t)].empty(); };
        for (auto& [p, k] : kits)
            if (has (p, DrumType::kick) && has (p, DrumType::closedHat) && (has (p, DrumType::snare) || has (p, DrumType::clap))) usable.push_back (p);
        std::map<long, std::vector<float>> cache;
        auto get = [&] (long i) -> const std::vector<float>* {
            auto it = cache.find (i);
            if (it == cache.end()) { std::vector<float> s; if (! load (items[static_cast<std::size_t> (i)].path, s)) s.clear(); it = cache.emplace (i, std::move (s)).first; }
            return it->second.empty() ? nullptr : &it->second;
        };
        auto pick = [&] (const std::string& p, DrumType t) -> long {
            const auto& v2 = kits[p][static_cast<std::size_t> (t)];
            return v2.empty() ? -1 : v2[rng() % v2.size()];
        };
        for (long k = 0; k < count && ! usable.empty(); ++k)
        {
            if (cache.size() > 400) cache.clear();
            const std::string& p = usable[rng() % usable.size()];
            const long kick = pick (p, DrumType::kick), hat = pick (p, DrumType::closedHat);
            long back = has (p, DrumType::snare) && (! has (p, DrumType::clap) || u (rng) < 0.65f) ? pick (p, DrumType::snare) : pick (p, DrumType::clap);
            const long layer = u (rng) < 0.25f ? pick (p, DrumType::clap) : -1;          // snare + clap stacked
            const long open = u (rng) < 0.5f ? pick (p, DrumType::openHat) : -1;
            const long crash = u (rng) < 0.4f ? pick (p, DrumType::crash) : -1;
            const long ride = u (rng) < 0.2f ? pick (p, DrumType::ride) : -1;
            const long perc = u (rng) < 0.3f ? pick (p, DrumType::misc) : -1;
            const long rim = u (rng) < 0.2f ? pick (p, DrumType::rimshot) : -1;
            const double step = 60.0 / (80.0 + 80.0 * u (rng)) / 4.0;
            const bool sixteenths = u (rng) < 0.4f;
            struct Ev { std::int64_t at; long idx; float g; };
            std::vector<Ev> evs;
            const auto lead = static_cast<std::int64_t> (0.5 * SR);
            auto add = [&] (int s, long idx, float g) {
                if (idx < 0) return;
                const auto jitter = static_cast<std::int64_t> ((u (rng) - 0.5f) * 0.004 * SR);
                evs.push_back ({ lead + static_cast<std::int64_t> (s * step * SR) + jitter, idx, g });
            };
            for (int s = 0; s < 32; ++s)
            {
                const int b = s % 16;
                if (b == 0 || b == 10 || (u (rng) < 0.12f)) add (s, kick, 0.7f + 0.3f * u (rng));
                if (b == 4 || b == 12) { add (s, back, 0.7f + 0.3f * u (rng)); if (layer >= 0) add (s, layer, 0.4f + 0.4f * u (rng)); }
                else if (u (rng) < 0.06f) add (s, back, 0.25f + 0.3f * u (rng));                // ghost note
                const bool hatStep = sixteenths || s % 2 == 0;
                if (ride >= 0 && s % 2 == 0) add (s, ride, 0.3f + 0.4f * u (rng));
                else if (hatStep) add (s, (open >= 0 && (b == 14 || (b == 6 && u (rng) < 0.3f))) ? open : hat, 0.25f + 0.45f * u (rng));
                if (s == 0 && crash >= 0) add (s, crash, 0.6f + 0.4f * u (rng));
                if (perc >= 0 && u (rng) < 0.1f) add (s, perc, 0.3f + 0.5f * u (rng));
                if (rim >= 0 && u (rng) < 0.08f) add (s, rim, 0.3f + 0.5f * u (rng));
            }
            std::vector<float> audio (static_cast<std::size_t> (lead + static_cast<std::int64_t> (32 * step * SR) + static_cast<std::int64_t> (1.5 * SR)), 0.0f);
            bool okKit = true;
            std::vector<std::pair<std::int64_t, std::uint32_t>> labels;
            for (const auto& e : evs)
            {
                const auto* s = get (e.idx);
                if (! s) { okKit = false; break; }
                for (std::size_t n = 0; n < s->size() && static_cast<std::size_t> (e.at) + n < audio.size(); ++n) audio[static_cast<std::size_t> (e.at) + n] += (*s)[n] * e.g * 0.5f;
                labels.push_back ({ e.at, 1u << static_cast<unsigned> (items[static_cast<std::size_t> (e.idx)].type) });
            }
            if (! okKit) continue;
            std::sort (labels.begin(), labels.end());
            std::vector<std::pair<std::int64_t, std::uint32_t>> merged;                       // hits within 10 ms = one layered hit
            for (const auto& l : labels)
                if (! merged.empty() && l.first - merged.back().first <= static_cast<std::int64_t> (0.010 * SR)) merged.back().second |= l.second;
                else merged.push_back (l);
            OnsetDetector det; det.prepare (SR);
            HitFeatures feat; feat.prepare (SR);
            std::deque<std::int64_t> pending;
            std::vector<bool> used (merged.size(), false);
            const auto tol = static_cast<std::int64_t> (0.025 * SR);
            OnsetEvent e;
            auto emit = [&] (std::int64_t onset) {
                std::size_t best = merged.size(); std::int64_t bestErr = tol + 1;
                for (std::size_t i = 0; i < merged.size(); ++i)
                {
                    const auto err = std::llabs (merged[i].first - onset);
                    if (! used[i] && err < bestErr) { bestErr = err; best = i; }
                }
                const std::uint32_t y = best < merged.size() ? merged[best].second : 0u;
                if (best < merged.size()) used[best] = true;
                const auto fv = feat.extract (onset);
                v.assign (fv.begin(), fv.end());
                row (out, kick, back, 1.0f, true, y, v);
            };
            auto push = [&] (float x) {
                if (det.processSample (x, e)) pending.push_back (e.sample);
                feat.pushSample (x);
                while (! pending.empty() && feat.samplesSeen() > pending.front() + feat.samplesAfterOnset()) { emit (pending.front()); pending.pop_front(); }
            };
            for (float x : audio) push (x);
            for (int i = 0; i < static_cast<int> (0.3 * SR); ++i) push (0.0f);
        }
    }
    std::fclose (out);
    return 0;
}
