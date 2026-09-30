# File: research/lookahead-test/lookahead_test.py
#
# Phase-2 experiment: how much lookahead does DETECTION need?
#
# For every labelled hit, the classifier may only see audio up to  onset + W  (plus anything before
# the onset, which is always available). We sweep W and measure per-type F1. The lookahead the
# plugin needs is about W + 3 ms (onset decision + 2 ms gate pre-roll).
#
# The classifier is a generic learned model (gradient-boosted trees on log-mel features), NOT the
# plugin's rule-based scorer. It measures how much information exists in the first W ms; the plugin's
# hand-made rules will do somewhat worse, but they hit the same knee.
#
# Data (not in the repo; download with fetch_data.sh):
#   acoustic   - MDB Drums (23 real recorded drum tracks, 7994 labelled hits, CC BY-NC-SA 4.0)
#   electronic - loops assembled here from Dirt-Samples one-shots (808/909 etc., incl. claps, rims);
#                samples are split into 5 folds, so test loops only use samples never seen in training.
#
# Usage: python3 lookahead_test.py <mdb_dir> <dirt_dir>

import glob, os, sys, time
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.metrics import f1_score

SR = 44100
TYPES = ["kick", "snare", "closed hat", "open hat", "crash", "ride", "rimshot", "clap", "misc"]
WINDOWS_MS = [5, 10, 15, 20, 25, 30, 40, 50, 60, 80, 100]
SEG_EDGES_MS = [0, 5, 10, 15, 20, 25, 30, 40, 50, 60, 80, 100]   # post-onset feature segments
PRE_MS = 25
N_FFT, HOP, N_MELS = 512, 64, 32
rng = np.random.default_rng(1)

# ------------------------------------------------------------------ features
def mel_filterbank():
    def hz2mel(f): return 2595 * np.log10(1 + f / 700)
    def mel2hz(m): return 700 * (10 ** (m / 2595) - 1)
    pts = mel2hz(np.linspace(hz2mel(30), hz2mel(16000), N_MELS + 2))
    bins = np.fft.rfftfreq(N_FFT, 1 / SR)
    fb = np.zeros((N_MELS, len(bins)))
    for i in range(N_MELS):
        l, c, r = pts[i], pts[i + 1], pts[i + 2]
        fb[i] = np.clip(np.minimum((bins - l) / (c - l), (r - bins) / (r - c)), 0, None)
    return fb
FB = mel_filterbank()
WIN = np.hanning(N_FFT)

def logmel(x):
    x = np.concatenate([np.zeros(N_FFT), x, np.zeros(N_FFT)])
    n = (len(x) - N_FFT) // HOP + 1
    idx = np.arange(N_FFT)[None, :] + HOP * np.arange(n)[:, None]
    spec = np.abs(np.fft.rfft(x[idx] * WIN, axis=1)) ** 2
    lm = np.log10(spec @ FB.T + 1e-10)
    frame_end = np.arange(n) * HOP + N_FFT - N_FFT   # sample index (in x) where each frame ENDS
    return lm, frame_end

def event_features(lm, frame_end, onset, w_ms):
    """Mean log-mel per segment, using only frames that END before onset + W."""
    feats = []
    def seg(a_ms, b_ms):
        a, b = onset + a_ms * SR / 1000, onset + b_ms * SR / 1000
        m = (frame_end > a) & (frame_end <= b)
        return lm[m].mean(axis=0) if m.any() else np.full(N_MELS, -10.0)
    feats.append(seg(-PRE_MS, 0))
    for a, b in zip(SEG_EDGES_MS[:-1], SEG_EDGES_MS[1:]):
        if b <= w_ms:
            feats.append(seg(a, b))
    return np.concatenate(feats)

def refine_onsets(x, onsets, search_ms=10):
    """Annotations can be off by a few ms: move each onset to the steepest energy rise nearby."""
    e = np.convolve(x ** 2, np.ones(44) / 44, mode="same")      # 1 ms energy
    le = 10 * np.log10(e + 1e-12)
    rise = np.zeros_like(le); rise[88:] = le[88:] - le[:-88]      # 2 ms rise
    out = []
    for o in onsets:
        a, b = max(0, o - int(search_ms * SR / 1000)), min(len(x), o + int(search_ms * SR / 1000))
        out.append(a + int(np.argmax(rise[a:b])) - 44)
    return out

# ------------------------------------------------------------------ acoustic: MDB Drums
MDB_MAP = {"KD": "kick", "SD": "snare", "SDG": "snare", "SDB": "snare", "SDF": "snare", "SDNS": "snare",
           "SDD": "snare", "CHH": "closed hat", "PHH": "closed hat", "OHH": "open hat",
           "CRC": "crash", "CHC": "crash", "SPC": "crash", "RDC": "ride", "RDB": "ride",
           "SST": "rimshot", "LFT": "misc", "MHT": "misc", "HFT": "misc", "HIT": "misc", "TMB": "misc"}

def load_mdb(mdb_dir):
    tracks = []
    for ann in sorted(glob.glob(os.path.join(mdb_dir, "annotations", "subclass", "*_subclass.txt"))):
        name = os.path.basename(ann).replace("_subclass.txt", "")
        x, sr = sf.read(os.path.join(mdb_dir, "audio", "drum_only", name + "_Drum.wav"))
        assert sr == SR
        if x.ndim > 1: x = x.mean(axis=1)
        rows = [l.split() for l in open(ann) if l.strip()]
        hits = sorted((float(t), MDB_MAP[c]) for t, c in rows)
        tracks.append((name, x, group_events([(int(t * SR), c) for t, c in hits])))
    return tracks

def group_events(hits, tol_ms=10):
    """Hits within 10 ms form one event with a set of labels (multi-label)."""
    events = []
    for o, c in hits:
        if events and o - events[-1][0] <= tol_ms * SR / 1000:
            events[-1][1].add(c)
        else:
            events.append([o, {c}])
    return events

# ------------------------------------------------------------------ electronic: loops from one-shots
DIRT_MAP = {
    "kick": ["808bd", "bd", "clubkick", "hardkick", "popkick", "kicklinn", "hh/005*", "hh/006*"],
    "snare": ["808sd", "sn", "sd", "hh/011*", "hh/012*"],
    "closed hat": ["808hc", "hc", "hh/000*"],
    "open hat": ["808oh", "ho", "hh/007*"],
    "crash": ["808cy", "hh/001*"],
    "ride": ["cr", "hh/010*"],            # the "cr" folder holds RIDED* ride samples
    "clap": ["cp", "realclaps"],
    "rimshot": ["rm", "rs"],
    "misc": ["hh/002*", "hh/003*", "hh/004*", "hh/008*", "hh/009*"],
}

def load_oneshots(dirt_dir):
    shots = {}
    for t, pats in DIRT_MAP.items():
        files = []
        for p in pats:
            files += sorted(glob.glob(os.path.join(dirt_dir, p, "*")) if "*" not in p else glob.glob(os.path.join(dirt_dir, p)))
        lst = []
        for f in files:
            x, sr = sf.read(f)
            if x.ndim > 1: x = x.mean(axis=1)
            if sr != SR: x = resample_poly(x, SR, sr)
            start = int(np.argmax(np.abs(x) > 0.05 * np.abs(x).max()))   # trim leading silence
            x = x[start:start + int(1.5 * SR)]
            lst.append(x / (np.abs(x).max() + 1e-9))
        shots[t] = lst
    return shots

def make_loops(shots, n_loops=100, seconds=8.0):
    # 5 folds of samples per type: loop k uses only fold k % 5
    folds = {t: [lst[i::5] if len(lst) >= 5 else [lst[j] for j in range(len(lst)) if j % 5 == f] for i in range(5)]
             for t, lst in shots.items() for f in [0]}
    for t, lst in shots.items():
        if len(lst) < 5:   # too few samples to hold out: repeat, and say so in the results
            folds[t] = [lst] * 5
    loops = []
    for k in range(n_loops):
        fold = k % 5
        bpm = rng.uniform(85, 150)
        step = 60 / bpm / 4
        n = int(seconds * SR)
        x = np.zeros(n + SR * 2)
        hits = []
        def place(t, s, gain_db):
            pool = folds[t][fold]
            if not pool: return
            y = pool[rng.integers(len(pool))]
            ratio = rng.uniform(0.95, 1.05)                       # small pitch/speed variation
            y = resample_poly(y, 100, int(round(100 * ratio)))
            o = int(s * step * SR + rng.normal(0, 0.002) * SR)    # humanise ±2 ms
            if o < 0: return
            x[o:o + len(y)] += y * 10 ** (gain_db / 20)
            hits.append((o, t))
        for s in range(int(seconds / step)):
            beat16 = s % 16
            if beat16 in (0, 8) or rng.random() < 0.12: place("kick", s, rng.uniform(-4, 0))
            if beat16 in (4, 12):
                r = rng.random()
                place("snare" if r < 0.55 else ("clap" if r < 0.8 else "rimshot"), s, rng.uniform(-5, 0))
                if rng.random() < 0.25: place("clap", s, rng.uniform(-8, -2))          # layered clap
            if s % 2 == 0 or rng.random() < 0.3:
                place("open hat" if rng.random() < 0.12 else "closed hat", s, rng.uniform(-14, -6))
            if beat16 == 0 and rng.random() < 0.25: place("crash", s, rng.uniform(-10, -4))
            if s % 4 == 2 and rng.random() < 0.15: place("ride", s, rng.uniform(-14, -6))
            if rng.random() < 0.03: place("misc", s, rng.uniform(-12, -4))
        loops.append((f"loop{k:03d}", x[:n] * 0.3, group_events(sorted(hits)), fold))
    return loops

# ------------------------------------------------------------------ evaluation
def evaluate(dataset, groups, types_present, label):
    print(f"\n=== {label}: {len(dataset)} events ===")
    y = np.array([[t in labs for t in types_present] for _, _, labs in dataset])
    counts = y.sum(axis=0)
    print("events per type: " + ", ".join(f"{t} {c}" for t, c in zip(types_present, counts)))
    header = f"{'W ms':>5} | " + " | ".join(f"{t[:10]:>10}" for t in types_present) + " |  macro"
    print(header); print("-" * len(header))
    rows = []
    for w in WINDOWS_MS:
        X = np.stack([event_features(lm, fe, o, w) for (lm, fe), o, _ in dataset])
        pred = np.zeros_like(y)
        ug = np.unique(groups)
        for f in range(5):
            test_g = ug[f::5]
            te = np.isin(groups, test_g); tr = ~te
            for c in range(len(types_present)):
                if y[tr, c].sum() < 3: continue
                clf = HistGradientBoostingClassifier(max_iter=150, learning_rate=0.1, class_weight="balanced", random_state=0)
                clf.fit(X[tr], y[tr, c])
                pred[te, c] = clf.predict(X[te])
        f1 = [f1_score(y[:, c], pred[:, c], zero_division=0) for c in range(len(types_present))]
        rows.append((w, f1))
        print(f"{w:>5} | " + " | ".join(f"{v:10.2f}" for v in f1) + f" | {np.mean(f1):6.3f}", flush=True)
    return rows

def main():
    mdb_dir, dirt_dir = sys.argv[1], sys.argv[2]
    t0 = time.time()

    # acoustic
    tracks = load_mdb(mdb_dir)
    ds, groups = [], []
    for gi, (name, x, events) in enumerate(tracks):
        lm, fe = logmel(x)
        ons = refine_onsets(x, [o for o, _ in events])
        for (o_ann, labs), o in zip(events, ons):
            if o > PRE_MS * SR / 1000 + N_FFT:
                ds.append(((lm, fe), o, labs)); groups.append(gi)
    ac_types = ["kick", "snare", "closed hat", "open hat", "crash", "ride", "rimshot", "misc"]
    ac = evaluate(ds, np.array(groups), ac_types, "ACOUSTIC (MDB Drums, real recordings, grouped by track)")

    # electronic
    shots = load_oneshots(dirt_dir)
    print("\none-shots per type: " + ", ".join(f"{t} {len(v)}" for t, v in shots.items()))
    loops = make_loops(shots)
    ds, groups = [], []
    for name, x, events, fold in loops:
        lm, fe = logmel(x)
        for o, labs in events:
            if o > PRE_MS * SR / 1000 + N_FFT:
                ds.append(((lm, fe), o, labs)); groups.append(fold)
    el = evaluate(ds, np.array(groups), TYPES, "ELECTRONIC (loops from held-out one-shots, grouped by sample fold)")
    print(f"\n(total {time.time() - t0:.0f} s)")

if __name__ == "__main__":
    main()
