# File: research/loop-learning/train_slow_torch.py
#
# F4 loop learning: trains the slow model (full-length picture -> 9 drum probabilities) with PyTorch,
# on the GPU when there is one (home: RTX 3060 Ti), else the CPU. Same data, picture and scoring as
# loop_learning.py, so results compare directly. Saves the network and its input scaling to an .npz
# (export to a dsp/ header comes when a model is chosen for the plugin).
#
# Usage: python3 train_slow_torch.py <dir with train.csv and the test sets> <out.npz> [--hidden 512,256] [--epochs 60]
#        test sets scored if present: electronic, acoustic, kit
import os, sys, time
import numpy as np
import torch
from torch import nn

import loop_learning as L

def main():
    d_dir, out = sys.argv[1], sys.argv[2]
    hidden = [512, 256]; epochs = 60
    for i, a in enumerate(sys.argv):
        if a == "--hidden": hidden = [int(v) for v in sys.argv[i + 1].split(",")]
        if a == "--epochs": epochs = int(sys.argv[i + 1])
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    torch.manual_seed(0)

    tr = L.load(os.path.join(d_dir, "train.csv"))
    X, Y = tr["L"].astype(np.float32), tr["Y"].astype(np.float32)
    mean, std = X.mean(0), X.std(0) + 1e-6
    Xn = (X - mean) / std
    rng = np.random.default_rng(0)
    perm = rng.permutation(len(X)); nval = len(X) // 10
    va, trn = perm[:nval], perm[nval:]

    layers, width = [], X.shape[1]
    for h in hidden:
        layers += [nn.Linear(width, h), nn.ReLU(), nn.Dropout(0.2)]
        width = h
    layers.append(nn.Linear(width, Y.shape[1]))
    net = nn.Sequential(*layers).to(dev)
    opt = torch.optim.AdamW(net.parameters(), lr=1e-3, weight_decay=1e-3)
    lossf = nn.BCEWithLogitsLoss()
    Xt, Yt = torch.tensor(Xn, device=dev), torch.tensor(Y, device=dev)

    best, bestState, wait, t0 = 1e9, None, 0, time.time()
    for ep in range(epochs):
        net.train()
        order = torch.tensor(rng.permutation(trn), device=dev)
        for b in range(0, len(order), 256):
            idx = order[b:b + 256]
            opt.zero_grad()
            loss = lossf(net(Xt[idx]), Yt[idx])
            loss.backward()
            opt.step()
        net.eval()
        with torch.no_grad():
            v = lossf(net(Xt[va]), Yt[va]).item()
        if v < best - 1e-4: best, bestState, wait = v, {k: t.clone() for k, t in net.state_dict().items()}, 0
        else:
            wait += 1
            if wait >= 8: break
    net.load_state_dict(bestState)
    print(f"device {dev}, {ep + 1} epochs, {time.time() - t0:.0f} s, validation loss {best:.4f}")

    def predict(Z):
        with torch.no_grad():
            return torch.sigmoid(net(torch.tensor((Z - mean) / std, dtype=torch.float32, device=dev))).cpu().numpy()

    for name in ["electronic", "acoustic", "kit"]:
        p = os.path.join(d_dir, name + ".csv")
        if not os.path.exists(p): continue
        d = L.load(p)
        f1, prec, n = L.score(L.weights(predict(d["L"])), d)
        print(f"{name:11s} slow F1: " + "  ".join(f"{L.TYPES[t]} {f1[t]:.2f}" for t in range(8) if n[t]))

    np.savez(out, mean=mean, std=std, hidden=np.array(hidden), **{k: v.cpu().numpy() for k, v in net.state_dict().items()})
    print("saved", out)

if __name__ == "__main__":
    main()
