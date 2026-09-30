// File: research/puppet-show/measure_rise.cpp
// Measures The Puppet Show's five ratios the way the PLUGIN measures them (onset detector ->
// classifier -> rise spectrum -> PuppetShow::measure) on labelled sets, per kit/file median, then
// the median over files. Used to move the v1 targets (measured on 150 ms windows) into the
// plugin's own measurement scale (docs/05-puppet-show-targets-v1.md, D-C15).
// Build: g++ -O2 -std=c++17 -I../../dsp/include -I../classifier-eval measure_rise.cpp -o measure_rise
// Run:   ./measure_rise <set_dir>
#include "wav_labels.h"
#include <plugin_dsp/HitClassifier.h>
#include <plugin_dsp/PuppetShow.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace plugin_dsp;
using PS = PuppetShow;

static float median (std::vector<float> v)
{
    if (v.empty()) return std::nanf ("");
    std::nth_element (v.begin(), v.begin() + static_cast<std::ptrdiff_t> (v.size() / 2), v.end());
    return v[v.size() / 2];
}

int main (int argc, char** argv)
{
    if (argc < 2) return 1;
    const char* names[] = { "M1 kick sub", "M2 mud", "M3 presence", "M4 kick click", "M5 air" };
    std::array<std::vector<float>, PS::numMeasures> fileMedians;
    for (const auto& name : evalio::listSet (argv[1]))
    {
        std::vector<float> x;
        double sr = 44100.0;
        if (! evalio::readWav (std::string (argv[1]) + "/" + name + ".wav", x, sr)) continue;
        const auto labels = evalio::readLabels (std::string (argv[1]) + "/" + name + ".txt");
        OnsetDetector det;
        det.prepare (sr);
        HitClassifier cls;
        cls.prepare (sr);
        std::array<std::vector<float>, PS::numMeasures> vals;
        OnsetEvent e;
        ClassifiedHit h;
        auto take = [&] (const ClassifiedHit& hit) {
            // clean, confident, and the ground truth agrees (a single label within 25 ms)
            const std::size_t t = index (hit.primary);
            if (hit.primary == DrumType::misc || hit.confidence < 0.7f || hit.weight[t] < 0.7f) return;
            for (std::size_t u = 0; u < numDrumTypes; ++u)
                if (u != t && u != index (DrumType::misc) && hit.weight[u] >= 0.3f) return;
            bool truth = false;
            for (const auto& l : labels)
                if (std::llabs (l.sample - hit.sample) <= static_cast<std::int64_t> (0.025 * sr)) truth = l.types == bit (hit.primary);
            if (! truth) return;
            const auto m = PS::measure (hit.rise);
            auto add = [&] (PS::Measure k) { if (std::isfinite (m[k])) vals[k].push_back (m[k]); };
            switch (hit.primary)
            {
                case DrumType::kick: add (PS::kickSub); add (PS::kickClick); add (PS::mud); break;
                case DrumType::snare: add (PS::presence); add (PS::mud); break;
                case DrumType::clap: add (PS::presence); break;
                case DrumType::closedHat: case DrumType::openHat: case DrumType::crash: case DrumType::ride: add (PS::air); break;
                default: break;
            }
        };
        auto push = [&] (float v) {
            if (det.processSample (v, e)) cls.addEvent (e);
            cls.pushSample (v);
            while (cls.popClassified (h)) take (h);
        };
        for (float v : x) push (v);
        for (int i = 0; i < static_cast<int> (0.3 * sr); ++i) push (0.0f);
        for (std::size_t k = 0; k < PS::numMeasures; ++k)
            if (vals[k].size() >= 5) fileMedians[k].push_back (median (vals[k]));
    }
    for (std::size_t k = 0; k < PS::numMeasures; ++k)
        std::printf ("%-14s median over files %6.1f dB  (%zu files)\n", names[k], static_cast<double> (median (fileMedians[k])), fileMedians[k].size());
}
