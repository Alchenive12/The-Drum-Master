# File: research/loop-learning/vote_eval.py
#
# F4: the fast model (shipped, 40 ms) and one or two trained slow models (train_slow_torch.py .npz) voting.
# Per drum type each slow model gets a weight (0, .25, ... with the weights summing to <= 1; the fast model
# gets the rest), picked on OTHER test sets than the one scored
# (acoustic <-> electronic; kit and any others from acoustic + electronic together), so no number here
# is tuned on its own test data. Scores exactly like research/classifier-eval/eval.cpp.
#
# Usage: python3 vote_eval.py <slow.npz>[,<slow2.npz>] <test.csv> [<test.csv> ...]   (names as saved: P_<csv stem>)
import os, sys
import numpy as np
import loop_learning as L

GRID = [0.0, 0.25, 0.5, 0.75, 1.0]

def combos(k):
    if k == 1: return [(a,) for a in GRID]
    return [(a, b) for a in GRID for b in GRID if a + b <= 1.0 + 1e-9]

def mix(d, Ps, w):
    """w: (types x models) slow weights; the fast model gets 1 - sum."""
    P = (1 - w.sum(1)) * d["P"]
    for m, Pm in enumerate(Ps): P = P + w[:, m] * Pm
    return P

def pick(sets, k):
    """Per-type slow weights maximising the summed F1 over the given (d, [Pslow...]) pairs."""
    w = np.zeros((L.NT, k))
    for t in range(L.NT):
        def total(c):
            s = 0.0
            for d, Ps in sets:
                P = d["P"].copy(); P[:, t] = (1 - sum(c)) * d["P"][:, t] + sum(c[m] * Ps[m][:, t] for m in range(k))
                s += L.score(L.weights(P), d)[0][t]
            return s
        w[t] = max(combos(k), key=total)
    return w

def main():
    zs = [np.load(p) for p in sys.argv[1].split(",")]
    k = len(zs)
    tests = {}
    for p in sys.argv[2:]:
        name = os.path.splitext(os.path.basename(p))[0]
        tests[name] = (L.load(p), [z["P_" + name] for z in zs])
    for name, (d, Ps) in tests.items():
        if name == "acoustic": others = ["electronic"]
        elif name == "electronic": others = ["acoustic"]
        else: others = [n for n in ("acoustic", "electronic") if n in tests and n != name]
        w = pick([tests[o] for o in others if o in tests], k)
        Pv = mix(d, Ps, w)
        print(f"== {name}  (slow weights from {'+'.join(others)}: " + " ".join(f"{L.TYPES[t]} " + "/".join(f"{v:.2f}" for v in w[t]) for t in range(8)) + ")")
        print(f"{'':22s}" + "".join(f"{t[:10]:>14s}" for t in L.TYPES[:8]))
        rows = [("fast (plugin today)", d["P"])] + [(f"slow {m + 1}", Pm) for m, Pm in enumerate(Ps)] + [("vote", Pv)]
        for label, P in rows:
            f1, prec, n = L.score(L.weights(P), d)
            print(f"{label:22s}" + "".join(f"{f1[t]:7.2f} ({prec[t]:.2f})" if n[t] else f"{'-':>14s}" for t in range(8)))
        print(f"{'(true hits)':22s}" + "".join(f"{n[t]:>14d}" for t in range(8)))
        print()

if __name__ == "__main__":
    main()
