# File: research/loop-learning/vote_eval.py
#
# F4: the fast model (shipped, 40 ms) and a trained slow model (train_slow_torch.py .npz) voting.
# Per drum type the slow model gets a weight 0..1, picked on OTHER test sets than the one scored
# (acoustic <-> electronic; kit and any others from acoustic + electronic together), so no number here
# is tuned on its own test data. Scores exactly like research/classifier-eval/eval.cpp.
#
# Usage: python3 vote_eval.py <slow.npz> <test.csv> [<test.csv> ...]   (names as saved: P_<csv stem>)
import os, sys
import numpy as np
import loop_learning as L

GRID = [0.0, 0.25, 0.5, 0.75, 1.0]

def pick(sets):
    """Per-type slow weight maximising the summed F1 over the given (d, Pslow) pairs."""
    w = np.zeros(L.NT)
    for t in range(L.NT):
        def total(a):
            s = 0.0
            for d, Ps in sets:
                P = d["P"].copy(); P[:, t] = (1 - a) * d["P"][:, t] + a * Ps[:, t]
                s += L.score(L.weights(P), d)[0][t]
            return s
        w[t] = max(GRID, key=total)
    return w

def main():
    z = np.load(sys.argv[1])
    tests = {}
    for p in sys.argv[2:]:
        name = os.path.splitext(os.path.basename(p))[0]
        tests[name] = (L.load(p), z["P_" + name])
    for name, (d, Ps) in tests.items():
        if name == "acoustic": others = ["electronic"]
        elif name == "electronic": others = ["acoustic"]
        else: others = [n for n in ("acoustic", "electronic") if n in tests and n != name]
        w = pick([tests[o] for o in others if o in tests])
        Pv = (1 - w) * d["P"] + w * Ps
        print(f"== {name}  (slow weights from {'+'.join(others)}: " + " ".join(f"{L.TYPES[t]} {w[t]:.2f}" for t in range(8)) + ")")
        print(f"{'':22s}" + "".join(f"{t[:10]:>14s}" for t in L.TYPES[:8]))
        for label, P in [("fast (plugin today)", d["P"]), ("slow", Ps), ("vote", Pv)]:
            f1, prec, n = L.score(L.weights(P), d)
            print(f"{label:22s}" + "".join(f"{f1[t]:7.2f} ({prec[t]:.2f})" if n[t] else f"{'-':>14s}" for t in range(8)))
        print(f"{'(true hits)':22s}" + "".join(f"{n[t]:>14d}" for t in range(8)))
        print()

if __name__ == "__main__":
    main()
