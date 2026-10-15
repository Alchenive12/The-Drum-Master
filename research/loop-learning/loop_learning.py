# File: research/loop-learning/loop_learning.py
#
# F4 loop learning, research (docs/05-features.md). Reads extract_long.cpp output and scores, on the
# real evaluation sets, per drum type exactly like research/classifier-eval/eval.cpp
# (type said = weight >= 0.5; misc weight = max(p_misc, 1 - best other p)):
#   fast          the shipped fast model (40 ms picture), as in the plugin today
#   fast+group    hits grouped per loop by their fast picture (repeats of one sound), each group's
#                 probabilities averaged (the vote) and given to all its hits
#   slow          a bigger model on the full-length picture (to +1.1 s), trained on synthetic drums only
#   ctrl          the same network and data on the 40 ms picture (shows what full length adds)
#   fast+slow     the two models averaged 50/50, and with per-type weights; and + group
# Nothing is tuned on the set it is scored on: the grouping threshold is picked 2-fold by file inside
# each real set, the per-type vote weights on the other real set.
#
# Sets: synthetic train/val (synth_aug.py), electronic + acoustic (classifier-eval), kit (kit_loops.py:
# one sample per drum reused, as real beats are made - the case grouping repeats is for).
# Usage: python3 loop_learning.py <dir with train.csv val.csv electronic.csv acoustic.csv [kit.csv] (+ .missed.csv)>
import os, sys, time
import numpy as np
from scipy.cluster.hierarchy import linkage, fcluster
from sklearn.neural_network import MLPClassifier
from sklearn.preprocessing import StandardScaler

TYPES = ["kick", "snare", "closed hat", "open hat", "crash", "ride", "rimshot", "clap", "misc"]
NT = len(TYPES)

def load(path):
    import pandas as pd   # much faster than np.loadtxt on the large (GB) training sets
    df = pd.read_csv(path, dtype={"file": str})
    files = df["file"].to_numpy()
    cols = list(df.columns[1:])
    data = df[cols].to_numpy(dtype=np.float32)
    yi = [i for i, c in enumerate(cols) if c.startswith("y")]
    pi = [i for i, c in enumerate(cols) if c.startswith("p")]
    fi = [i for i, c in enumerate(cols) if c.startswith("f")]
    li = [i for i, c in enumerate(cols) if c.startswith("l")]
    missed = np.zeros(NT, dtype=int)
    with open(path + ".missed.csv") as f:
        f.readline()
        for line in f:
            bits = int(line.strip().split(",")[1])
            for t in range(NT): missed[t] += (bits >> t) & 1
    return dict(file=files, onset=data[:, 0], Y=data[:, yi].astype(int), P=data[:, pi], F=data[:, fi], L=data[:, li], missed=missed)

def weights(P):
    W = P.copy()
    W[:, 8] = np.maximum(P[:, 8], 1.0 - P[:, :8].max(axis=1))
    return W

def score(W, d):
    pred = W >= 0.5
    Y = d["Y"].astype(bool)
    tp = (pred & Y).sum(0); fp = (pred & ~Y).sum(0); fn = (~pred & Y).sum(0) + d["missed"]
    f1 = np.where(tp > 0, 2 * tp / np.maximum(2 * tp + fp + fn, 1), 0.0)
    prec = np.where(tp + fp > 0, tp / np.maximum(tp + fp, 1), 0.0)
    return f1, prec, tp + fn

def group_vote(P, d, threshold):
    """Average probabilities over repeats of one sound in each loop (grouped by the fast picture)."""
    out = P.copy()
    # After the onset only (the 25 ms before it holds whatever else was ringing), and without the absolute
    # level: the same sample played softer is the same sound. Measured on kit loops: the same sample is
    # ~8 dB RMS apart this way, different drums ~20 dB (with the 25 ms before: 12 vs 21 dB).
    G = d["F"][:, 32:-1]
    for name in np.unique(d["file"]):
        idx = np.where(d["file"] == name)[0]
        if len(idx) < 2: continue
        Z = linkage(G[idx], method="average", metric="euclidean")
        lab = fcluster(Z, t=threshold * np.sqrt(G.shape[1]), criterion="distance")   # threshold = RMS dB distance
        for c in np.unique(lab):
            members = idx[lab == c]
            out[members] = P[members].mean(axis=0)
    return out

def train(X, Y, hidden, seed=0):
    sc = StandardScaler().fit(X)
    m = MLPClassifier(hidden_layer_sizes=hidden, alpha=1e-3, batch_size=256, learning_rate_init=1e-3,
                      max_iter=200, early_stopping=True, validation_fraction=0.1, n_iter_no_change=10, random_state=seed)
    m.fit(sc.transform(X), Y)
    return lambda Z: m.predict_proba(sc.transform(Z))

def main():
    d_dir = sys.argv[1]
    names = ["train", "val", "electronic", "acoustic"] + (["kit"] if os.path.exists(os.path.join(d_dir, "kit.csv")) else [])
    sets = {s: load(os.path.join(d_dir, s + ".csv")) for s in names}
    tr, va = sets["train"], sets["val"]
    print(f"synthetic train {len(tr['Y'])} hits, val {len(va['Y'])}; electronic {len(sets['electronic']['Y'])}, acoustic {len(sets['acoustic']['Y'])}")

    t0 = time.time()
    slow = train(tr["L"], tr["Y"], (512, 256))
    ctrl = train(tr["F"], tr["Y"], (512, 256))
    print(f"trained in {time.time() - t0:.0f} s")

    real = [n for n in ["electronic", "acoustic", "kit"] if n in sets]
    weightsFrom = {"electronic": "acoustic", "acoustic": "electronic", "kit": "acoustic"}   # never the scored set
    slowP = {n: slow(sets[n]["L"]) for n in real}

    # Grouping threshold: 2-fold by file inside each real set (tuned on half the loops, scored on the
    # other half). The synthetic loops can't tune it: their generator draws every hit anew, nothing repeats.
    def grouped_cv(P, d):
        files = np.unique(d["file"])
        out = P.copy()
        for k in range(2):
            tune = np.isin(d["file"], files[k::2]); test = ~tune
            sub = lambda m: {key: (v[m] if isinstance(v, np.ndarray) and v.shape[:1] == m.shape else v) for key, v in d.items()}
            dt, ds = sub(tune), sub(test)
            dt["missed"] = ds["missed"] = np.zeros(NT, dtype=int)
            best = max([2, 4, 6, 8, 10, 12], key=lambda th: np.average(score(weights(group_vote(P[tune], dt, th)), dt)[0][:8],
                                                                            weights=np.maximum(score(weights(P[tune]), dt)[2][:8], 1)))
            out[test] = group_vote(P[test], ds, best)
            print(f"    {d.get('name', '')} fold {k}: threshold {best} dB")
        return out

    # Per-type vote weights for the slow model, picked on the OTHER real set (never on the one scored).
    grid = [0.0, 0.25, 0.5, 0.75, 1.0]
    def pick_weights(d, Ps):
        w = np.zeros(NT)
        for t in range(NT):
            def f1_of(a):
                P = d["P"].copy(); P[:, t] = (1 - a) * d["P"][:, t] + a * Ps[:, t]
                return score(weights(P), d)[0][t]
            w[t] = max(grid, key=f1_of)
        return w

    for name in real:
        d = sets[name]; d["name"] = name
        other = weightsFrom[name]
        w = pick_weights(sets[other], slowP[other])
        Pw = (1 - w) * d["P"] + w * slowP[name]
        sizes = []
        for f in np.unique(d["file"]):
            idx = np.where(d["file"] == f)[0]
            if len(idx) > 1:
                lab = fcluster(linkage(d["F"][idx, 32:-1], "average", "euclidean"), t=8 * np.sqrt(192), criterion="distance")
                sizes.append(len(idx) / len(np.unique(lab)))
        print(f"== {name}: hits per group at 8 dB: {np.mean(sizes):.1f}; slow-model weights from {other}: " + " ".join(f"{TYPES[t]}={w[t]:.2f}" for t in range(8)))
        rows = {
            "fast (plugin today)": d["P"],
            "fast + group vote (2-fold)": grouped_cv(d["P"], d),
            "ctrl: synth-only, 40 ms": ctrl(d["F"]),
            "slow: synth-only, full length": slowP[name],
            "fast + slow, 50/50": 0.5 * (d["P"] + slowP[name]),
            "fast + slow, per-type weights": Pw,
            "  ... + group vote (2-fold)": grouped_cv(Pw, d),
        }
        print(f"{'':32s}" + "".join(f"{t[:10]:>14s}" for t in TYPES[:8]))
        for label, P in rows.items():
            f1, prec, n = score(weights(P), d)
            print(f"{label:32s}" + "".join(f"{f1[t]:7.2f} ({prec[t]:.2f})" if n[t] else f"{'-':>14s}" for t in range(8)))
        print(f"{'(true hits)':32s}" + "".join(f"{n[t]:>14d}" for t in range(8)))
        print()

if __name__ == "__main__":
    main()
