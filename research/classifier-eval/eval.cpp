// File: research/classifier-eval/eval.cpp
// Runs the plugin's own dsp/ OnsetDetector + HitClassifier on real drums (export_sets.py output)
// and reports onset F1 and per-type precision / recall. Not part of the plugin build.
//
// Build: g++ -O2 -std=c++17 -I../../dsp/include eval.cpp -o eval   (wav_labels.h is next to it)
// Run:   ./eval <set_dir> [--tolerance-ms 25]
//
// Matching: a detected event matches the nearest labelled event within the tolerance (labels
// within 10 ms are one event, a set of types). Per type: predicted = weight >= 0.5 in the matched
// event; true = the type is in the label set. Unmatched detections count as false positives
// for their predicted types; missed labelled events as false negatives for their types.
#include "wav_labels.h"
#include <plugin_dsp/HitClassifier.h>

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace plugin_dsp;

using evalio::Label;

int main (int argc, char** argv)
{
    if (argc < 2) { std::fprintf (stderr, "usage: eval <set_dir> [--tolerance-ms N]\n"); return 1; }
    const std::string dir = argv[1];
    double tolMs = 25.0;
    std::string dumpPath;
    for (int i = 2; i + 1 < argc; ++i)
    {
        if (std::string (argv[i]) == "--tolerance-ms") tolMs = std::atof (argv[i + 1]);
        if (std::string (argv[i]) == "--dump") dumpPath = argv[i + 1];
    }
    std::FILE* dump = dumpPath.empty() ? nullptr : std::fopen (dumpPath.c_str(), "w");
    if (dump)
    {
        std::fprintf (dump, "truth,primary,regLow,regMid,regMetal,residual,hiDrop,hiFrac,conc,lowExt,bursts,midFlat,body,cenEarly,cenLate,lowShare,highShare,bodyProm,logCen,hiRise,midRise,file");
        for (int b = 0; b < 13; ++b) std::fprintf (dump, ",m%d", b);
        std::fprintf (dump, "\n");
    }

    const auto names = evalio::listSet (dir);

    std::array<long, numDrumTypes> tp {}, fp {}, fn {};
    long onsetTp = 0, onsetFp = 0, onsetFn = 0;
    std::map<std::string, long> confusion;   // "true -> primary" for matched single-label events

    for (const auto& name : names)
    {
        std::vector<float> x;
        double sr = 44100.0;
        if (! evalio::readWav (dir + "/" + name + ".wav", x, sr)) continue;
        const auto labels = evalio::readLabels (dir + "/" + name + ".txt");

        OnsetDetector det;
        det.prepare (sr);
        HitClassifier cls;
        cls.prepare (sr);
        std::vector<ClassifiedHit> hits;
        std::vector<HitClassifier::Measurements> meas;
        OnsetEvent e;
        ClassifiedHit h;
        auto push = [&] (float v) {
            if (det.processSample (v, e)) cls.addEvent (e);
            cls.pushSample (v);
            while (cls.popClassified (h)) { hits.push_back (h); meas.push_back (cls.lastMeasurements()); }
        };
        for (float v : x) push (v);
        for (int i = 0; i < static_cast<int> (0.3 * sr); ++i) push (0.0f);

        const auto tol = static_cast<std::int64_t> (tolMs * sr / 1000.0);
        std::vector<bool> used (labels.size(), false);
        for (std::size_t hi = 0; hi < hits.size(); ++hi)
        {
            const auto& hit = hits[hi];
            std::uint32_t predicted = 0;
            for (std::size_t t = 0; t < numDrumTypes; ++t) if (hit.weight[t] >= 0.5f) predicted |= 1u << t;
            std::size_t best = labels.size();
            std::int64_t bestErr = tol + 1;
            for (std::size_t i = 0; i < labels.size(); ++i)
            {
                const std::int64_t err = std::llabs (labels[i].sample - hit.sample);
                if (! used[i] && err < bestErr) { bestErr = err; best = i; }
            }
            if (best == labels.size())
            {
                ++onsetFp;
                for (std::size_t t = 0; t < numDrumTypes; ++t) if (predicted & (1u << t)) ++fp[t];
                continue;
            }
            used[best] = true;
            ++onsetTp;
            const std::uint32_t truth = labels[best].types;
            for (std::size_t t = 0; t < numDrumTypes; ++t)
            {
                const bool p = (predicted >> t) & 1u, g = (truth >> t) & 1u;
                if (p && g) ++tp[t];
                else if (p) ++fp[t];
                else if (g) ++fn[t];
            }
            if (dump)
            {
                const auto& m = meas[hi];
                std::string ts;
                for (std::size_t t = 0; t < numDrumTypes; ++t) if ((truth >> t) & 1u) { if (! ts.empty()) ts += "+"; ts += drumTypeName (static_cast<DrumType> (t)); }
                std::fprintf (dump, "%s,%s,%.3f,%.3f,%.3f,%.3f,%.2f,%.3f,%.3f,%.3f,%d,%.3f,%.2f,%.1f,%.1f", ts.c_str(), drumTypeName (hit.primary),
                              static_cast<double> (m.regionShare[0]), static_cast<double> (m.regionShare[1]), static_cast<double> (m.regionShare[2]),
                              static_cast<double> (m.residualRatio), static_cast<double> (m.hiDropDb), static_cast<double> (m.hiDecayFraction),
                              static_cast<double> (m.flatness), static_cast<double> (m.lowExtension), m.bursts, static_cast<double> (m.midFlatness),
                              static_cast<double> (m.bodyRatioDb), static_cast<double> (m.centroidEarlyHz), static_cast<double> (m.centroidLateHz));
                std::fprintf (dump, ",%.4f,%.4f,%.3f,%.3f,%.2f,%.2f,%s", static_cast<double> (m.lowShare), static_cast<double> (m.highShare),
                              static_cast<double> (m.bodyProminence), static_cast<double> (m.logCentroid), static_cast<double> (m.hiRiseDb),
                              static_cast<double> (m.midRiseDb), name.c_str());
                float mx = 1e-20f;
                for (float v : m.magnitude) mx = std::max (mx, v);
                for (float v : m.magnitude) std::fprintf (dump, ",%.3f", static_cast<double> (v / mx));
                std::fprintf (dump, "\n");
            }
            if ((truth & (truth - 1)) == 0)   // single-label truth: record the confusion
            {
                std::size_t g = 0;
                while (! ((truth >> g) & 1u)) ++g;
                ++confusion[std::string (drumTypeName (static_cast<DrumType> (g))) + " -> " + drumTypeName (hit.primary)];
            }
        }
        for (std::size_t i = 0; i < labels.size(); ++i)
            if (! used[i])
            {
                ++onsetFn;
                for (std::size_t t = 0; t < numDrumTypes; ++t) if ((labels[i].types >> t) & 1u) ++fn[t];
            }
    }

    if (dump) std::fclose (dump);
    auto f1 = [] (long a, long b, long c) { return a ? 2.0 * a / (2.0 * a + b + c) : 0.0; };
    std::printf ("files %zu, tolerance %.0f ms\n", names.size(), tolMs);
    std::printf ("onsets: tp %ld fp %ld fn %ld  F1 %.3f\n", onsetTp, onsetFp, onsetFn, f1 (onsetTp, onsetFp, onsetFn));
    std::printf ("%-11s %6s %6s %6s  %9s %6s %6s\n", "type", "tp", "fp", "fn", "precision", "recall", "F1");
    for (std::size_t t = 0; t < numDrumTypes; ++t)
    {
        if (tp[t] + fn[t] == 0 && fp[t] == 0) continue;
        const double p = tp[t] + fp[t] ? static_cast<double> (tp[t]) / static_cast<double> (tp[t] + fp[t]) : 0.0;
        const double r = tp[t] + fn[t] ? static_cast<double> (tp[t]) / static_cast<double> (tp[t] + fn[t]) : 0.0;
        std::printf ("%-11s %6ld %6ld %6ld  %9.2f %6.2f %6.2f\n", drumTypeName (static_cast<DrumType> (t)), tp[t], fp[t], fn[t], p, r, f1 (tp[t], fp[t], fn[t]));
    }
    std::printf ("\nmost common confusions (single-label events):\n");
    std::vector<std::pair<long, std::string>> c;
    for (auto& kv : confusion) c.push_back ({ kv.second, kv.first });
    std::sort (c.rbegin(), c.rend());
    for (std::size_t i = 0; i < c.size() && i < 14; ++i) std::printf ("  %5ld  %s\n", c[i].first, c[i].second.c_str());
    return 0;
}
