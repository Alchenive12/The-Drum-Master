# plugin today (old extract_long CSVs) vs the rebuilt plugin (new CSVs), per type F1 (precision), eval.cpp scoring.
import os, sys
sys.path.insert(0, r"C:\Users\didi\source\repos\The-Drum-Master\train-home\src\research\loop-learning")
import loop_learning as L
old, new = sys.argv[1], sys.argv[2]
for name in sys.argv[3:]:
    print("==", name, "  ", "".join(f"{t[:10]:>14s}" for t in L.TYPES[:8]))
    for lab, root in (("plugin today", old), ("new plugin", new)):
        d = L.load(os.path.join(root, name + ".csv"))
        f1, pr, n = L.score(L.weights(d["P"]), d)
        print(f"{lab:14s}" + "".join(f"{f1[t]:7.2f} ({pr[t]:.2f})" if n[t] else f"{'-':>14s}" for t in range(8)) + f"   onsets {len(d['P'])}", flush=True)
