# File: research/classifier-eval/export_kit_loops.py
#
# A third evaluation set, "electronic_kit": loops like research/lookahead-test's electronic ones (same
# Dirt-Samples one-shots, same pattern rules, ±2 ms humanising, per-hit gain variation), but each loop
# uses ONE sample per drum type with one fixed pitch, the way a produced beat repeats one kit.
# The original set picks a new random sample for every hit, which hides anything that learns this
# loop's sounds (docs/09: loop templates, loop memory).
#
# Usage: python3 export_kit_loops.py <dirt_dir> <out_dir> [n_loops=40]
# Writes <out_dir>/electronic_kit/loopNNN.{wav,txt} in eval.cpp's format.

import os, sys
import numpy as np
from scipy.signal import resample_poly

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "lookahead-test"))
import lookahead_test as lt   # noqa: E402
from export_sets import write  # noqa: E402

def make_kit_loops(shots, n_loops=40, seconds=8.0, seed=7):
    rng = np.random.default_rng(seed)
    loops = []
    for k in range(n_loops):
        kit = {}
        for t, lst in shots.items():
            if lst:
                y = lst[rng.integers(len(lst))]
                kit[t] = resample_poly(y, 100, int(round(100 * rng.uniform(0.95, 1.05))))
        bpm = rng.uniform(85, 150)
        step = 60 / bpm / 4
        n = int(seconds * lt.SR)
        x = np.zeros(n + lt.SR * 2)
        hits = []
        def place(t, s, gain_db):
            if t not in kit: return
            o = int(s * step * lt.SR + rng.normal(0, 0.002) * lt.SR)
            if o < 0: return
            y = kit[t]
            x[o:o + len(y)] += y * 10 ** (gain_db / 20)
            hits.append((o, t))
        for s in range(int(seconds / step)):   # same pattern rules as lookahead_test.make_loops
            beat16 = s % 16
            if beat16 in (0, 8) or rng.random() < 0.12: place("kick", s, rng.uniform(-4, 0))
            if beat16 in (4, 12):
                r = rng.random()
                place("snare" if r < 0.55 else ("clap" if r < 0.8 else "rimshot"), s, rng.uniform(-5, 0))
                if rng.random() < 0.25: place("clap", s, rng.uniform(-8, -2))
            if s % 2 == 0 or rng.random() < 0.3:
                place("open hat" if rng.random() < 0.12 else "closed hat", s, rng.uniform(-14, -6))
            if beat16 == 0 and rng.random() < 0.25: place("crash", s, rng.uniform(-10, -4))
            if s % 4 == 2 and rng.random() < 0.15: place("ride", s, rng.uniform(-14, -6))
            if rng.random() < 0.03: place("misc", s, rng.uniform(-12, -4))
        loops.append((f"loop{k:03d}", x[:n] * 0.3, lt.group_events(sorted(hits))))
    return loops

def main():
    dirt_dir, out = sys.argv[1], sys.argv[2]
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    for name, x, events in make_kit_loops(lt.load_oneshots(dirt_dir), n):
        write(os.path.join(out, "electronic_kit"), name, x, events)
    print("done")

if __name__ == "__main__":
    main()
