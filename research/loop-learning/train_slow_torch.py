# File: research/loop-learning/train_slow_torch.py
#
# F4 loop learning: trains the slow model (full-length picture -> 9 drum probabilities) with PyTorch,
# on the GPU when there is one (home: RTX 3060 Ti), else the CPU. Input: extract_long.cpp CSVs.
#
# Training sets (--train, any number): e.g. Groove (gmd.csv), E-GMD (egmd.csv), synthetic (synth.csv),
# the user's packs (home_train.csv). Held out exactly like research/classifier-train/train.py, so the
# fast and slow models never see the same validation data: 15 % of Groove performances by file
# (numpy seed 0) and six whole E-GMD kits (needs --egmd-subset <subset.csv> from fetch_egmd.py).
# Like train.py: rows with rare labels are oversampled, and every row gets a random static EQ and level
# (in the feature domain) so the net learns the drum, not the kit or the mix.
# Test sets (--test, any number; never trained on) are scored exactly like research/classifier-eval/eval.cpp.
# Saves the network, its input scaling and every test set's probabilities (for the vote) to an .npz.
#
# Usage: python3 train_slow_torch.py --train gmd.csv egmd.csv synth.csv --test acoustic.csv electronic.csv kit.csv
#                                    --out slow.npz [--egmd-subset subset.csv] [--hidden 512,256] [--epochs 80] [--aug 1]
import argparse, csv, os, time
import numpy as np
import torch
from torch import nn

import loop_learning as L

MELS, SEGS = 40, 16
HELD_KITS = {"Acoustic Kit", "808 Simple", "Heavy Metal", "Nu RNB", "Cassette (Lo-Fi Compress)", "Jazz"}

def augment(X, rng):
    """Random static EQ (tilt + two bumps, +-6 dB) and level, re-referenced to the post-onset peak
    exactly like extract_long.cpp's picture (train.py's augment, for the full-length layout)."""
    n = len(X)
    m = np.linspace(-1, 1, MELS)[None, :]
    curve = rng.uniform(-6, 6, (n, 1)) * m
    for _ in range(2):
        c = rng.uniform(-1, 1, (n, 1)); w = rng.uniform(0.1, 0.4, (n, 1)); g = rng.uniform(-6, 6, (n, 1))
        curve += g * np.exp(-0.5 * ((m - c) / w) ** 2)
    rel = X[:, :SEGS * MELS].reshape(n, SEGS, MELS) + curve[:, None, :]
    shift = rel[:, 1:, :].reshape(n, -1).max(1)
    out = X.copy()
    out[:, :SEGS * MELS] = np.maximum(rel - shift[:, None, None], -80.0).reshape(n, -1)
    out[:, SEGS * MELS] = X[:, SEGS * MELS] + shift + rng.uniform(-12, 6, n)
    return out.astype(np.float32)

def held_out_mask(d, path, egmd_subset):
    """Rows train.py also keeps out of training: Groove validation files, E-GMD held-out kits."""
    name = os.path.basename(path).lower()
    files = d["file"]
    if "gmd" in name and "egmd" not in name:
        uf = np.unique(files); np.random.default_rng(0).shuffle(uf)
        val = set(uf[: len(uf) * 15 // 100])
        return np.array([f in val for f in files])
    if "egmd" in name and egmd_subset:
        kit_of = {os.path.splitext(r["audio_filename"])[0].replace("/", "_"): r["kit_name"] for r in csv.DictReader(open(egmd_subset))}
        return np.array([kit_of.get(f, "?") in HELD_KITS for f in files])
    return np.zeros(len(files), dtype=bool)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", nargs="+", required=True)
    ap.add_argument("--test", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--egmd-subset", default="")
    ap.add_argument("--hidden", default="512,256")
    ap.add_argument("--epochs", type=int, default=80)
    ap.add_argument("--aug", type=int, default=1)
    ap.add_argument("--max-rows", type=int, default=600000)
    a = ap.parse_args()
    hidden = [int(v) for v in a.hidden.split(",")]
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(0)
    rng = np.random.default_rng(0)

    Xs, Ys, heldsets = [], [], []
    for p in a.train:
        d = L.load(p)
        held = held_out_mask(d, p, a.egmd_subset)
        Xs.append(d["L"][~held]); Ys.append(d["Y"][~held])
        if held.any():
            heldsets.append((os.path.basename(p) + " held-out", {k: (v[held] if isinstance(v, np.ndarray) and v.shape[:1] == held.shape else v) for k, v in d.items()}))
            heldsets[-1][1]["missed"] = np.zeros(L.NT, dtype=int)   # held-out rows only: no missed-label counts
        print(f"train {os.path.basename(p)}: {(~held).sum()} hits ({held.sum()} held out)")
    X, Y = np.vstack(Xs).astype(np.float32), np.vstack(Ys).astype(np.float32)
    if len(X) > a.max_rows:
        keep = rng.choice(len(X), a.max_rows, replace=False); X, Y = X[keep], Y[keep]

    # oversample rare labels (as train.py), then EQ/level augmentation copies
    counts = Y.sum(0) + 1; target = np.median(counts)
    rep = np.ones(len(Y))
    for t in range(L.NT):
        if counts[t] < target: rep[Y[:, t] == 1] = np.maximum(rep[Y[:, t] == 1], min(8.0, target / counts[t]))
    idx = np.repeat(np.arange(len(Y)), np.round(rep).astype(int))
    X, Y = X[idx], Y[idx]
    X[:, SEGS * MELS] += rng.uniform(-24, 6, len(X)).astype(np.float32)   # the loop's level says little
    if a.aug > 0:
        X = np.vstack([X] + [augment(X, rng) for _ in range(a.aug)]); Y = np.vstack([Y] * (a.aug + 1))
    print(f"training rows {len(X)} on {dev}; label counts " + " ".join(f"{L.TYPES[t]}={int(Y[:, t].sum())}" for t in range(L.NT)))

    mean, std = X.mean(0), X.std(0) + 1e-3
    perm = rng.permutation(len(X)); nval = len(X) // 20
    va, trn = perm[:nval], perm[nval:]
    layers, width = [], X.shape[1]
    for h in hidden:
        layers += [nn.Linear(width, h), nn.ReLU(), nn.Dropout(0.2)]
        width = h
    layers.append(nn.Linear(width, L.NT))
    net = nn.Sequential(*layers).to(dev)
    opt = torch.optim.AdamW(net.parameters(), lr=1e-3, weight_decay=1e-3)
    lossf = nn.BCEWithLogitsLoss()
    Xt = torch.tensor((X - mean) / std, device=dev); Yt = torch.tensor(Y, device=dev)
    best, bestState, wait, t0 = 1e9, None, 0, time.time()
    for ep in range(a.epochs):
        net.train()
        order = torch.randperm(len(trn), device=dev)
        trn_t = torch.tensor(trn, device=dev)[order]
        for b in range(0, len(trn_t), 512):
            i = trn_t[b:b + 512]
            opt.zero_grad(); lossf(net(Xt[i]), Yt[i]).backward(); opt.step()
        net.eval()
        with torch.no_grad():
            v = lossf(net(Xt[va]), Yt[va]).item()
        if v < best - 1e-4: best, bestState, wait = v, {k: t.clone() for k, t in net.state_dict().items()}, 0
        else:
            wait += 1
            if wait >= 8: break
    net.load_state_dict(bestState)
    print(f"{ep + 1} epochs, {time.time() - t0:.0f} s, validation loss {best:.4f}")

    def predict(Z):
        out = []
        with torch.no_grad():
            for b in range(0, len(Z), 8192):
                out.append(torch.sigmoid(net(torch.tensor((Z[b:b + 8192] - mean) / std, dtype=torch.float32, device=dev))).cpu().numpy())
        return np.vstack(out) if out else np.zeros((0, L.NT))

    saved = {}
    for name, d in heldsets + [(os.path.splitext(os.path.basename(p))[0], L.load(p)) for p in a.test]:
        P = predict(d["L"])
        saved["P_" + name] = P
        f1, prec, n = L.score(L.weights(P), d)
        print(f"{name:28s} slow F1: " + "  ".join(f"{L.TYPES[t]} {f1[t]:.2f}" for t in range(8) if n[t]))
    np.savez(a.out, mean=mean, std=std, hidden=np.array(hidden), **{k: v.cpu().numpy() for k, v in net.state_dict().items()}, **saved)
    print("saved", a.out)

if __name__ == "__main__":
    main()
