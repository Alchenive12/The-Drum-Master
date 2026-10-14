// File: research/loop-learning/extract_long.cpp
// F4 loop learning, research (docs/05-features.md): for every hit the plugin detects in a labelled set
// (<name>.wav + <name>.txt, research/classifier-eval format), write
//   - the matched label (9 bits; all zero for a false onset), exactly as research/classifier-eval/eval.cpp matches,
//   - the shipped fast model's 9 probabilities (HitClassifier, what the plugin decides 40 ms after the hit),
//   - the fast 225-number picture (HitFeatures, -25..+40 ms), used for grouping repeats,
//   - a full-length picture for a slow model: 40 mel bands x 16 time segments from -30 ms to +1100 ms
//     (dB, minus the hit's loudest post-onset value), that loudest value, and the time to the previous
//     and next detected onset (where other hits start to overlap the picture).
// Labelled hits the detector missed go to <out>.missed.csv (file, label bits), so scores match eval.cpp.
// Not part of the plugin. Build: g++ -O2 -std=c++17 -I../../dsp/include -I../classifier-eval extract_long.cpp -o extract_long
// Run:   ./extract_long <set_dir> <out.csv>
#include <plugin_dsp/Fft.h>
#include <plugin_dsp/HitClassifier.h>
#include <plugin_dsp/HitFeatures.h>
#include <plugin_dsp/OnsetDetector.h>
#include "wav_labels.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

using namespace plugin_dsp;

namespace
{
constexpr std::size_t numMel = 40;
constexpr std::array<double, 17> segEdgesMs { -30, 0, 10, 20, 30, 45, 60, 80, 110, 150, 200, 270, 360, 480, 640, 850, 1100 };
constexpr std::size_t numSeg = segEdgesMs.size() - 1;
constexpr std::size_t numLong = numMel * numSeg + 3;

/** Offline log-mel spectrogram of a whole file: frames every `hop` samples, centred at frame * hop. */
struct Spectrogram
{
    std::size_t hop = 256;
    std::vector<std::array<float, numMel>> frames;

    void compute (const std::vector<float>& x, double sr)
    {
        const std::size_t n = 1024;
        Fft fft;
        fft.prepare (n);
        std::vector<Fft::Complex> buf (n);
        std::vector<float> win (n);
        for (std::size_t i = 0; i < n; ++i) win[i] = static_cast<float> (0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * static_cast<double> (i) / static_cast<double> (n)));
        // triangular mel filters, 30 Hz .. 16 kHz
        auto mel = [] (double f) { return 2595.0 * std::log10 (1.0 + f / 700.0); };
        auto hz = [] (double m) { return 700.0 * (std::pow (10.0, m / 2595.0) - 1.0); };
        std::vector<double> centres (numMel + 2);
        for (std::size_t i = 0; i < centres.size(); ++i)
            centres[i] = hz (mel (30.0) + (mel (16000.0) - mel (30.0)) * static_cast<double> (i) / static_cast<double> (numMel + 1));
        const double binHz = sr / static_cast<double> (n);
        std::vector<std::vector<std::pair<std::size_t, float>>> filt (numMel);
        for (std::size_t m = 0; m < numMel; ++m)
            for (std::size_t k = 1; k <= n / 2; ++k)
            {
                const double f = static_cast<double> (k) * binHz;
                double w = 0.0;
                if (f > centres[m] && f <= centres[m + 1]) w = (f - centres[m]) / (centres[m + 1] - centres[m]);
                else if (f > centres[m + 1] && f < centres[m + 2]) w = (centres[m + 2] - f) / (centres[m + 2] - centres[m + 1]);
                if (w > 0.0) filt[m].push_back ({ k, static_cast<float> (w) });
            }
        const std::size_t count = x.size() / hop + 1;
        frames.assign (count, {});
        for (std::size_t fi = 0; fi < count; ++fi)
        {
            const auto centre = static_cast<std::int64_t> (fi * hop);
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::int64_t q = centre - static_cast<std::int64_t> (n / 2) + static_cast<std::int64_t> (i);
                const float v = q >= 0 && q < static_cast<std::int64_t> (x.size()) ? x[static_cast<std::size_t> (q)] : 0.0f;
                buf[i] = Fft::Complex (v * win[i], 0.0f);
            }
            fft.forward (buf.data());
            for (std::size_t m = 0; m < numMel; ++m)
            {
                double e = 0.0;
                for (const auto& [k, w] : filt[m]) e += static_cast<double> (w) * static_cast<double> (std::norm (buf[k]));
                frames[fi][m] = static_cast<float> (10.0 * std::log10 (e + 1e-12));
            }
        }
    }

    std::array<float, numLong> picture (std::int64_t onset, double sr, std::int64_t prevOnset, std::int64_t nextOnset) const
    {
        std::array<float, numLong> out {};
        float peak = -200.0f;
        for (std::size_t s = 0; s < numSeg; ++s)
        {
            const double a = static_cast<double> (onset) + segEdgesMs[s] * 0.001 * sr, b = static_cast<double> (onset) + segEdgesMs[s + 1] * 0.001 * sr;
            auto f0 = static_cast<std::int64_t> (std::ceil (a / static_cast<double> (hop))), f1 = static_cast<std::int64_t> (std::floor (b / static_cast<double> (hop)));
            if (f1 < f0) f1 = f0;   // short segments: the nearest frame
            for (std::size_t m = 0; m < numMel; ++m)
            {
                double sum = 0.0;
                int c = 0;
                for (std::int64_t f = f0; f <= f1; ++f)
                {
                    const auto idx = static_cast<std::size_t> (std::clamp<std::int64_t> (f, 0, static_cast<std::int64_t> (frames.size()) - 1));
                    sum += static_cast<double> (frames[idx][m]);
                    ++c;
                }
                out[s * numMel + m] = static_cast<float> (sum / c);
                if (s > 0) peak = std::max (peak, out[s * numMel + m]);
            }
        }
        for (std::size_t i = 0; i < numMel * numSeg; ++i) out[i] = std::max (out[i] - peak, -80.0f);
        out[numMel * numSeg] = peak;
        out[numMel * numSeg + 1] = prevOnset < 0 ? 1100.0f : static_cast<float> (std::min (1100.0, 1000.0 * static_cast<double> (onset - prevOnset) / sr));
        out[numMel * numSeg + 2] = nextOnset < 0 ? 1100.0f : static_cast<float> (std::min (1100.0, 1000.0 * static_cast<double> (nextOnset - onset) / sr));
        return out;
    }
};
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::fprintf (stderr, "usage: extract_long <set_dir> <out.csv>\n"); return 1; }
    const std::string dir = argv[1], outPath = argv[2];
    std::FILE* out = std::fopen (outPath.c_str(), "w");
    std::FILE* missed = std::fopen ((outPath + ".missed.csv").c_str(), "w");
    if (! out || ! missed) return 1;
    std::fprintf (out, "file,onset");
    for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",y%zu", t);
    for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",p%zu", t);
    for (std::size_t f = 0; f < HitFeatures::numFeatures; ++f) std::fprintf (out, ",f%zu", f);
    for (std::size_t f = 0; f < numLong; ++f) std::fprintf (out, ",l%zu", f);
    std::fprintf (out, "\n");
    std::fprintf (missed, "file,types\n");

    long rows = 0;
    const auto names = evalio::listSet (dir);
    for (const auto& name : names)
    {
        std::vector<float> x;
        double sr = 44100.0;
        if (! evalio::readWav (dir + "/" + name + ".wav", x, sr)) continue;
        const auto labels = evalio::readLabels (dir + "/" + name + ".txt");

        // the plugin's own detection and fast classification (as eval.cpp), plus the fast picture
        OnsetDetector det;
        det.prepare (sr);
        HitClassifier cls;
        cls.prepare (sr);
        HitFeatures feat;
        feat.prepare (sr);
        struct Hit { std::int64_t onset; std::array<float, numDrumTypes> p; HitFeatures::Vector f; std::uint32_t y = 0; };
        std::vector<Hit> hits;
        std::deque<std::int64_t> pendingFeat;
        std::vector<HitFeatures::Vector> feats;
        std::vector<std::int64_t> featOnsets;
        OnsetEvent e;
        ClassifiedHit h;
        auto push = [&] (float v) {
            if (det.processSample (v, e)) { cls.addEvent (e); pendingFeat.push_back (e.sample); }
            cls.pushSample (v);
            feat.pushSample (v);
            while (! pendingFeat.empty() && feat.samplesSeen() > pendingFeat.front() + feat.samplesAfterOnset())
            {
                feats.push_back (feat.extract (pendingFeat.front()));
                featOnsets.push_back (pendingFeat.front());
                pendingFeat.pop_front();
            }
            while (cls.popClassified (h)) hits.push_back ({ h.sample, cls.lastMeasurements().probability, {} });
        };
        for (float v : x) push (v);
        for (int i = 0; i < static_cast<int> (0.3 * sr); ++i) push (0.0f);
        if (feats.size() != hits.size()) { std::fprintf (stderr, "%s: %zu hits vs %zu pictures\n", name.c_str(), hits.size(), feats.size()); return 1; }
        // the classifier may finish two close events in swapped order: pair pictures by onset, then sort by time
        for (auto& hit : hits)
        {
            const auto it = std::find (featOnsets.begin(), featOnsets.end(), hit.onset);
            if (it == featOnsets.end()) { std::fprintf (stderr, "%s: no picture for onset %lld\n", name.c_str(), static_cast<long long> (hit.onset)); return 1; }
            hit.f = feats[static_cast<std::size_t> (it - featOnsets.begin())];
        }

        // label matching in the classifier's order, exactly as eval.cpp; then sort by time for the neighbours
        const auto tol = static_cast<std::int64_t> (0.025 * sr);
        std::vector<bool> used (labels.size(), false);
        for (auto& hit : hits)
        {
            std::size_t best = labels.size();
            std::int64_t bestErr = tol + 1;
            for (std::size_t i = 0; i < labels.size(); ++i)
            {
                const std::int64_t err = std::llabs (labels[i].sample - hit.onset);
                if (! used[i] && err < bestErr) { bestErr = err; best = i; }
            }
            hit.y = best < labels.size() ? labels[best].types : 0u;
            if (best < labels.size()) used[best] = true;
        }
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.onset < b.onset; });

        Spectrogram spec;
        spec.compute (x, sr);
        for (std::size_t hi = 0; hi < hits.size(); ++hi)
        {
            const auto& hit = hits[hi];
            const std::uint32_t y = hit.y;
            const auto lp = spec.picture (hit.onset, sr, hi > 0 ? hits[hi - 1].onset : -1, hi + 1 < hits.size() ? hits[hi + 1].onset : -1);
            std::fprintf (out, "%s,%lld", name.c_str(), static_cast<long long> (hit.onset));
            for (std::size_t t = 0; t < numDrumTypes; ++t) std::fprintf (out, ",%u", (y >> t) & 1u);
            for (float v : hit.p) std::fprintf (out, ",%.4f", static_cast<double> (v));
            for (float v : hit.f) std::fprintf (out, ",%.2f", static_cast<double> (v));
            for (float v : lp) std::fprintf (out, ",%.2f", static_cast<double> (v));
            std::fprintf (out, "\n");
            ++rows;
        }
        for (std::size_t i = 0; i < labels.size(); ++i)
            if (! used[i]) std::fprintf (missed, "%s,%u\n", name.c_str(), labels[i].types);
    }
    std::fclose (out);
    std::fclose (missed);
    std::fprintf (stderr, "%zu files, %ld events\n", names.size(), rows);
    return 0;
}
