// File: research/puppet-show/run_on_loops.cpp
// Runs the whole DrumEngine with The Puppet Show on (Rock, Depth 100) over WAV loops and prints
// what it measured, the targets and the corrections it settled on.
// Build: g++ -O2 -std=c++17 -I../../dsp/include -I../classifier-eval run_on_loops.cpp -o run_on_loops
#include "wav_labels.h"
#include <plugin_dsp/DrumEngine.h>
#include <cstdio>
#include <memory>
using namespace plugin_dsp;
int main (int argc, char** argv) {
  const char* names[] = { "kick sub", "mud", "presence", "kick click", "air" };
  for (int a = 1; a < argc; ++a) {
    std::vector<float> x; double sr = 44100; if (! evalio::readWav (argv[a], x, sr)) continue;
    auto e = std::make_unique<DrumEngine>(); e->prepare (sr);
    DrumParams p; p.puppetShow.enabled = true; p.puppetShow.depth = 100; p.puppetShow.genre = DrumParams::PuppetShow::Genre::rock; e->setParams (p);
    std::vector<float> r (x);
    for (std::size_t i = 0; i < x.size(); i += 512) e->process (x.data() + i, r.data() + i, std::min<std::size_t> (512, x.size() - i));
    std::printf ("%s\n", argv[a]);
    for (std::size_t m = 0; m < PuppetShow::numMeasures; ++m) {
      auto mm = static_cast<PuppetShow::Measure> (m);
      std::printf ("  %-10s measured %6.1f dB (%3d hits)  rock target %6.1f  gain %+5.1f dB\n", names[m], static_cast<double> (e->puppet().measuredDb (mm)), e->puppet().hits (mm), static_cast<double> (PuppetShow::targets[0][m]), static_cast<double> (e->puppet().gainDb (mm)));
    }
  }
}
