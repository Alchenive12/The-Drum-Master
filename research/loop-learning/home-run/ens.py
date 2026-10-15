# Averages the saved test-set probabilities of several slow models into one .npz (an ensemble), for vote_eval.py.
import sys, numpy as np
zs = [np.load(p) for p in sys.argv[2:]]
keys = set.intersection(*[set(k for k in z.files if k.startswith("P_")) for z in zs])
np.savez(sys.argv[1], **{k: np.mean([z[k] for z in zs], axis=0) for k in keys})
print("ensemble of", len(zs), "->", sys.argv[1], sorted(keys))
