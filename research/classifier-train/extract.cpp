// File: research/classifier-train/extract.cpp
// Runs the plugin's own OnsetDetector + HitFeatures over a labelled set (<name>.wav + <name>.txt)
// and writes one CSV row per detected event: file, onset, 9 label bits, 225 features.
// A detection with no label within the tolerance gets all-zero labels (a false onset: the model
// should call it nothing / misc). Build: g++ -O2 -std=c++17 -I../../dsp/include extract.cpp -o extract
#include <plugin_dsp/DrumTypes.h>
#include <plugin_dsp/HitFeatures.h>
#include <plugin_dsp/OnsetDetector.h>

#include "../classifier-eval/wav_labels.h"

#include <cstdio>
#include <deque>

using namespace plugin_dsp;

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage: extract <set_dir> <out.csv> [--limit N]\n"); return 1; }
    std::size_t limit = 1u << 30;
    for (int i = 3; i + 1 < argc; ++i) if (std::string (argv[i]) == "--limit") limit = static_cast<std::size_t> (std::atol (argv[i + 1]));
    std::FILE* out = std::fopen (argv[2], "w");
    std::fprintf (out, "file,onset");
    for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",y%zu", t);
    for (std::size_t f = 0; f < HitFeatures::numFeatures; ++f) std::fprintf (out, ",f%zu", f);
    std::fprintf (out, "\n");

    auto names = evalio::listSet (argv[1]);
    if (names.size() > limit) names.resize (limit);
    long rows = 0;
    for (const auto& name : names)
    {
        std::vector<float> x;
        double sr = 44100.0;
        if (! evalio::readWav (std::string (argv[1]) + "/" + name + ".wav", x, sr)) continue;
        const auto labels = evalio::readLabels (std::string (argv[1]) + "/" + name + ".txt");
        OnsetDetector det;
        det.prepare (sr);
        HitFeatures feat;
        feat.prepare (sr);
        std::deque<std::int64_t> pending;
        const auto tol = static_cast<std::int64_t> (0.025 * sr);
        std::vector<bool> used (labels.size(), false);
        OnsetEvent e;
        auto emit = [&] (std::int64_t onset) {
            std::size_t best = labels.size();
            std::int64_t bestErr = tol + 1;
            for (std::size_t i = 0; i < labels.size(); ++i)
            {
                const std::int64_t err = std::llabs (labels[i].sample - onset);
                if (! used[i] && err < bestErr) { bestErr = err; best = i; }
            }
            const std::uint32_t y = best < labels.size() ? labels[best].types : 0u;
            if (best < labels.size()) used[best] = true;
            const auto v = feat.extract (onset);
            std::fprintf (out, "%s,%lld", name.c_str(), static_cast<long long> (onset));
            for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",%u", (y >> t) & 1u);
            for (float f : v) std::fprintf (out, ",%.2f", static_cast<double> (f));
            std::fprintf (out, "\n");
            ++rows;
        };
        auto push = [&] (float v) {
            if (det.processSample (v, e)) pending.push_back (e.sample);
            feat.pushSample (v);
            while (! pending.empty() && feat.samplesSeen() > pending.front() + feat.samplesAfterOnset())
            {
                emit (pending.front());
                pending.pop_front();
            }
        };
        for (float v : x) push (v);
        for (int i = 0; i < static_cast<int> (0.3 * sr); ++i) push (0.0f);
    }
    std::fclose (out);
    std::fprintf (stderr, "%zu files, %ld events\n", names.size(), rows);
    return 0;
}
