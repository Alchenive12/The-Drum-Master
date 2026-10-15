# Scores a bigger fast model (train_slow_torch.py --picture F .npz) alone and with the plugin's second network
# (V1, per-type max, main leads above 0.6), against the plugin today, on each test set.
import os, sys
sys.path.insert(0, r"C:\Users\didi\source\repos\The-Drum-Master\train-home\src\research\loop-learning")
import numpy as np, loop_learning as L, train_fast_torch as F
z = np.load(sys.argv[1]); V1 = F.load_header(sys.argv[2])
for p in sys.argv[3:]:
    name = os.path.splitext(os.path.basename(p))[0]; d = L.load(p)
    pb = z["P_" + name]; pv = F.mlp(V1, d["F"])
    rows = [("plugin today", d["P"]), ("big fast", pb), ("big fast + V1", F.combine(pb, pv)), ("big fast + V1 lead .5", F.combine(pb, pv, 0.5))]
    print("==", name)
    for lab, P in rows:
        f1, pr, n = L.score(L.weights(P), d)
        print(f"{lab:22s}" + "".join(f"{f1[t]:7.2f} ({pr[t]:.2f})" if n[t] else f"{'-':>14s}" for t in range(8)), flush=True)
