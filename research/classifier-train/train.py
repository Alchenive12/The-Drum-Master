# File: research/classifier-train/train.py
#
# Trains the hit classifier (225 features -> 48 ReLU -> 9 sigmoid, multi-label) on permissively
# licensed / own data only, evaluates on data it never saw, and exports the weights as a C++ header.
#
#   train:    Groove MIDI Dataset (CC BY 4.0) + our synthetic electronic loops (synth_aug.py)
#   evaluate: held-out Groove performances, MDB Drums (acoustic, CC BY-NC-SA: evaluation only),
#             Dirt-Samples loops (electronic, licence unclear: evaluation only)
#
# Usage: python3 train.py <scratch_dir> <header_out> [--hidden 48] [--aug 1] [--max-egmd 400000]
#   (reads X_gmd.csv, X_syn.csv, X_ac.csv, X_el.csv and every X_egmd*.csv in <scratch_dir>)
import os, sys, time
import numpy as np
from sklearn.neural_network import MLPClassifier
from sklearn.metrics import f1_score

TYPES = ["kick", "snare", "closedHat", "openHat", "crash", "ride", "rimshot", "clap", "misc"]

def load(path):
    cache = path + ".npz"
    if os.path.exists(cache) and os.path.getmtime(cache) > os.path.getmtime(path):
        d = np.load(cache)
        return d["files"], d["X"], d["y"]
    with open(path) as f:
        ncol = len(f.readline().split(","))
        files = np.array([line.split(",", 1)[0] for line in f])
    data = np.loadtxt(path, delimiter=",", skiprows=1, usecols=range(2, ncol), dtype=np.float32)
    X, y = data[:, 9:], data[:, :9].astype(np.int8)
    np.savez(cache, files=files, X=X, y=y)
    return files, X, y

MELS, SEGS = 32, 7

def augment(X, rng):
    """Random static EQ (tilt + two bumps, +-9 dB) and level (-12..+6 dB) applied in the feature domain:
    the same dB curve is added to every time segment, then the features are re-referenced to the
    new post-onset peak exactly like HitFeatures::extract does. Teaches the net that a kick is a kick
    on any kit, desk or mix."""
    n = len(X)
    m = np.linspace(-1, 1, MELS)[None, :]
    curve = rng.uniform(-6, 6, (n, 1)) * m
    for _ in range(2):
        c = rng.uniform(-1, 1, (n, 1)); w = rng.uniform(0.1, 0.4, (n, 1)); g = rng.uniform(-6, 6, (n, 1))
        curve += g * np.exp(-0.5 * ((m - c) / w) ** 2)
    rel = X[:, :-1].reshape(n, SEGS, MELS) + curve[:, None, :]
    shift = rel[:, 1:, :].reshape(n, -1).max(1)
    out = np.empty_like(X)
    out[:, :-1] = np.maximum(rel - shift[:, None, None], -80.0).reshape(n, -1)
    out[:, -1] = X[:, -1] + shift + rng.uniform(-12, 6, n)
    return out

def report(name, y, p, thr=0.5):
    pred = (p >= thr).astype(int)
    f1s = [f1_score(y[:, t], pred[:, t], zero_division=0) if y[:, t].sum() > 0 else float("nan") for t in range(len(TYPES))]
    line = " ".join(f"{TYPES[t][:6]:>6}={f1s[t]:.2f}" if not np.isnan(f1s[t]) else f"{TYPES[t][:6]:>6}=  - " for t in range(len(TYPES)))
    valid = [v for v in f1s if not np.isnan(v)]
    print(f"{name:<34} macroF1 {np.mean(valid):.3f} | {line}", flush=True)
    return f1s

def main():
    S = sys.argv[1]; header_out = sys.argv[2]
    hidden = int(sys.argv[sys.argv.index("--hidden") + 1]) if "--hidden" in sys.argv else 48
    fg, Xg, yg = load(f"{S}/X_gmd.csv")
    fs, Xs, ys = load(f"{S}/X_syn.csv")
    fa, Xa, ya = load(f"{S}/X_ac.csv")
    fe, Xe, ye = load(f"{S}/X_el.csv")
    import glob
    egmd_parts = sorted(glob.glob(f"{S}/X_egmd*.csv"))
    have_egmd = len(egmd_parts) > 0
    if have_egmd:
        loaded = [load(p) for p in egmd_parts]
        fk = np.concatenate([a[0] for a in loaded]); Xk = np.vstack([a[1] for a in loaded]); yk = np.vstack([a[2] for a in loaded])
        cap = int(sys.argv[sys.argv.index("--max-egmd") + 1]) if "--max-egmd" in sys.argv else 400000
        if len(yk) > cap:   # keep memory in check: a random subset of events (every kit stays represented)
            keep = np.sort(np.random.default_rng(1).choice(len(yk), cap, replace=False))
            fk, Xk, yk = fk[keep], Xk[keep], yk[keep]
    print("events: gmd", len(yg), "e-gmd", len(yk) if have_egmd else 0, "synth", len(ys), "acoustic", len(ya), "electronic", len(ye))

    # Hold out ~15 % of Groove performances (by file) for validation.
    rng = np.random.default_rng(0)
    uf = np.unique(fg); rng.shuffle(uf)
    val_files = set(uf[: len(uf) * 15 // 100])
    vmask = np.array([f in val_files for f in fg])
    parts_X, parts_y = [Xg[~vmask], Xs], [yg[~vmask], ys]
    if have_egmd:
        # Hold out whole kits (never heard in training) to measure what matters: unseen kits.
        import csv as _csv
        kit_of = {os.path.splitext(r["audio_filename"])[0].replace("/", "_"): r["kit_name"]
                  for r in _csv.DictReader(open(f"{S}/egmd/subset.csv"))}
        kits = np.array([kit_of.get(f, "?") for f in fk])
        held_kits = {"Acoustic Kit", "808 Simple", "Heavy Metal", "Nu RNB", "Cassette (Lo-Fi Compress)", "Jazz"}
        kmask = np.array([k in held_kits for k in kits])
        parts_X.append(Xk[~kmask]); parts_y.append(yk[~kmask])
    Xtr = np.vstack(parts_X); ytr = np.vstack(parts_y)
    print("train label counts:", dict(zip(TYPES, ytr.sum(0))))

    # Oversample rows with rare labels so every type gets comparable weight.
    counts = ytr.sum(0) + 1
    target = np.median(counts)
    rep = np.ones(len(ytr))
    for t in range(len(TYPES)):
        if counts[t] < target: rep[ytr[:, t] == 1] = np.maximum(rep[ytr[:, t] == 1], min(8.0, target / counts[t]))
    idx = np.repeat(np.arange(len(ytr)), np.round(rep).astype(int))
    Xtr, ytr = Xtr[idx], ytr[idx]
    # The loop's absolute level says little (users' loops sit anywhere from -30 to 0 dBFS), so every
    # training row gets a random level shift; the net must rely on the spectral shape over time.
    Xtr[:, -1] += rng.uniform(-24, 6, len(Xtr)).astype(np.float32)
    copies = int(sys.argv[sys.argv.index("--aug") + 1]) if "--aug" in sys.argv else 1
    if copies > 0:
        Xtr = np.vstack([Xtr] + [augment(Xtr, rng) for _ in range(copies)]); ytr = np.vstack([ytr] * (copies + 1))
    print("training rows:", len(ytr))

    mean = Xtr.mean(0); std = Xtr.std(0) + 1e-3
    norm = lambda X: (X - mean) / std
    t0 = time.time()
    clf = MLPClassifier(hidden_layer_sizes=(hidden,), activation="relu", alpha=1e-3, batch_size=512,
                        learning_rate_init=2e-3, max_iter=60, early_stopping=True, n_iter_no_change=6, random_state=0)
    clf.fit(norm(Xtr), ytr)
    print(f"trained in {time.time() - t0:.0f} s, {clf.n_iter_} epochs")

    evals = [("groove held-out (train domain)", Xg[vmask], yg[vmask])]
    if have_egmd:
        evals.append(("e-gmd held-out kits (never seen)", Xk[kmask], yk[kmask]))
    evals += [("MDB acoustic (never seen)", Xa, ya), ("electronic loops (never seen)", Xe, ye)]
    summary_parts = []
    for name, X, y in evals:
        f1s = report(name, y, clf.predict_proba(norm(X)))
        valid = [v for v in f1s if not np.isnan(v)]
        summary_parts.append(f"{name.split(' (')[0]} macro {np.mean(valid):.2f}")
    summary = "; ".join(summary_parts)

    import pickle
    pickle.dump((clf, mean, std), open(header_out + ".pkl", "wb"))
    w1, w2 = clf.coefs_; b1, b2 = clf.intercepts_
    fl = lambda a: ", ".join(f"{float(v):.6g}f" for v in np.ravel(a))
    with open(header_out, "w") as f:
        f.write("// File: dsp/include/plugin_dsp/HitClassifierWeights.h\n")
        f.write("// GENERATED by research/classifier-train/train.py - do not edit by hand.\n")
        f.write("// Weights of the hit classifier network (HitClassifier.h, decision D-C1).\n")
        f.write("// Trained on the Groove MIDI Dataset (Gillick, Roberts, Engel, Tsao, Bamman 2019) and the\n")
        f.write("// Expanded Groove MIDI Dataset (Callender, Hawthorne, Engel 2020), Magenta, both CC BY 4.0\n")
        f.write("// (https://magenta.tensorflow.org/datasets/groove, /datasets/e-gmd), and our own synthetic loops.\n")
        f.write(f"// Held-out results (per-type F1 at 0.5): {summary}\n")
        f.write("#pragma once\n\n#include \"TinyMlp.h\"\n\n#include <cstddef>\n\nnamespace plugin_dsp\n{\n")
        f.write(f"constexpr std::size_t hitClassifierHidden = {hidden};\n\n")
        f.write(f"inline const TinyMlpWeights<{w1.shape[0]}, {hidden}, {w2.shape[1]}>& hitClassifierWeights()\n{{\n")
        f.write(f"    static const TinyMlpWeights<{w1.shape[0]}, {hidden}, {w2.shape[1]}> w {{\n")
        rows = [mean, 1.0 / std, w1, b1, w2, b2]
        for i, a in enumerate(rows):
            f.write("        {{ " + fl(a) + " }}" + ("," if i + 1 < len(rows) else "") + "\n")
        f.write("    };\n    return w;\n}\n} // namespace plugin_dsp\n")
    print("wrote", header_out)

if __name__ == "__main__":
    main()
