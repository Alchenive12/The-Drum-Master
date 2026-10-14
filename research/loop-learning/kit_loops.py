# File: research/loop-learning/kit_loops.py
#
# "Kit loops" for F4 loop learning (docs/05-features.md): like research/lookahead-test's electronic
# loops (same held-out Dirt-Samples one-shots, same patterns, same 5 sample folds), except each loop
# picks ONE sample per drum type - a kit - and reuses it for every hit, as beats are really made.
# That is what grouping repeats needs; the original loops draw a new sample for every hit.
# Hits keep a small random pitch/speed change (+-2 %) and +-2 ms timing, so repeats are close, not identical.
# Output: research/classifier-eval format (<name>.wav + <name>.txt).
# Usage: python3 kit_loops.py <dirt_dir> <out_dir> [num_loops]
import os, sys
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "lookahead-test"))
import lookahead_test as lt   # noqa: E402

NAMES = {"kick": "kick", "snare": "snare", "closed hat": "closedHat", "open hat": "openHat", "crash": "crash",
         "ride": "ride", "rimshot": "rimshot", "clap": "clap", "misc": "misc"}

def main():
    dirt, out = sys.argv[1], sys.argv[2]
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    rng = np.random.default_rng(7)
    shots = lt.load_oneshots(dirt)
    os.makedirs(out, exist_ok=True)
    for k in range(count):
        fold = k % 5
        kit = {}
        for t, lst in shots.items():
            pool = lst[fold::5] if len(lst) >= 5 else lst
            if pool: kit[t] = pool[rng.integers(len(pool))]
        step = 60 / rng.uniform(85, 150) / 4
        n = int(8.0 * lt.SR)
        x = np.zeros(n + 2 * lt.SR)
        hits = []
        def place(t, s, gain_db):
            if t not in kit: return
            y = resample_poly(kit[t], 100, int(round(100 * rng.uniform(0.98, 1.02))))
            o = int(s * step * lt.SR + rng.normal(0, 0.002) * lt.SR)
            if o < 0: return
            x[o:o + len(y)] += y * 10 ** (gain_db / 20)
            hits.append((o, t))
        for s in range(int(8.0 / step)):
            b = s % 16
            if b in (0, 8) or rng.random() < 0.12: place("kick", s, rng.uniform(-4, 0))
            if b in (4, 12):
                r = rng.random()
                place("snare" if r < 0.55 else ("clap" if r < 0.8 else "rimshot"), s, rng.uniform(-5, 0))
                if rng.random() < 0.25: place("clap", s, rng.uniform(-8, -2))
            if s % 2 == 0 or rng.random() < 0.3:
                place("open hat" if rng.random() < 0.12 else "closed hat", s, rng.uniform(-14, -6))
            if b == 0 and rng.random() < 0.25: place("crash", s, rng.uniform(-10, -4))
            if s % 4 == 2 and rng.random() < 0.15: place("ride", s, rng.uniform(-14, -6))
            if rng.random() < 0.03: place("misc", s, rng.uniform(-12, -4))
        events = lt.group_events(sorted(hits))
        name = f"kit{k:03d}"
        sf.write(os.path.join(out, name + ".wav"), (x[:n] * 0.3).astype(np.float32), lt.SR, subtype="FLOAT")
        with open(os.path.join(out, name + ".txt"), "w") as f:
            for o, labs in events:
                f.write(f"{o} {','.join(sorted(NAMES[l] for l in labs))}\n")
    print("kit loops:", count)

if __name__ == "__main__":
    main()
