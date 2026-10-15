# File: research/loop-learning/home_packs.py
#
# Home training data from the user's own sample folders (F4, docs/05-features.md).
#   1. Finds one-shots (WAV/AIFF/FLAC) under the given roots and labels each from its file name, then
#      its folders - the same keyword rules as dsp/tools/sample_packs.h (the benchmark's --oneshots mode).
#      Loops ("loop" in the path, or longer than 4 s) and anything without a drum keyword are skipped.
#   2. Splits by PACK (the first folder under a root, or Splice's pack folder), never by sample, so the
#      test packs are truly unheard: about 1 pack in 5 goes to test (stable: decided by the pack's name).
#   3. Writes kit loops like kit_loops.py: each loop picks one sample per drum from ONE pack (a kit),
#      plays a groove with it, labels every hit. Format: research/classifier-eval (<name>.wav + .txt).
#
# Licences: give packs whose licence forbids AI/ML training with --test-only; they only ever go to the
# test set. Check Splice's current terms before training on Splice sounds (see docs/05-features.md F4).
#
# Usage (Windows example):
#   python home_packs.py --out D:\dm-data\home --roots "%USERPROFILE%\Documents\Splice" "D:\Samples" [--test-only "D:\Samples\SomePack"] [--loops-per-pack 6]
import argparse, hashlib, os, re, sys
import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

SR = 44100
EXT = (".wav", ".aif", ".aiff", ".flac")

# ---------------------------------------------------------------- labels (sample_packs.h, same order)
def has_word(s, w):
    return re.search(r"(?<![a-z])" + re.escape(w) + r"(?![a-z])", s) is not None

RULES = [
    ("openHat",   ["open hat", "openhat", "open_hat", "open-hat", "open hh", "ohh"], ["oh", "ohat"]),
    ("closedHat", ["closed hat", "closedhat", "closed_hat", "hihat", "hi-hat", "hi hat", "hi_hat"], ["hat", "hats", "hh", "chh", "ch"]),
    ("crash",     ["crash", "cymbal"], ["cym"]),
    ("ride",      ["ride"], []),
    ("rimshot",   ["rimshot", "sidestick", "side stick", "side_stick"], ["rim", "rims", "stick"]),
    ("clap",      ["clap"], ["snap", "snaps"]),
    ("snare",     ["snare"], ["snr", "sd"]),
    ("kick",      ["808"], []),
    ("kick",      ["kick", "bassdrum", "bass drum"], ["kik", "bd"]),
    ("misc",      ["shaker", "tambourine", "cowbell", "conga", "bongo", "woodblock", "clave"], ["tom", "toms", "perc", "percs", "tamb", "block"]),
]

def label_from(text):
    s = text.lower()
    if "open" in s and (any(k in s for k in ["hihat", "hi-hat", "hi hat", "hi_hat"]) or any(has_word(s, w) for w in ["hat", "hats", "hh"])):
        return "openHat"
    for t, sub, word in RULES:
        if any(k in s for k in sub) or any(has_word(s, w) for w in word):
            return t
    return None

def scan(root, test_only, collections=()):
    shots = []
    root = os.path.abspath(os.path.expandvars(root))
    for d, _, files in os.walk(root):
        for f in files:
            if not f.lower().endswith(EXT): continue
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root)
            parts = rel.split(os.sep)
            if "loop" in rel.lower(): continue
            # pack: Splice keeps sounds under .../packs/<pack name>/..., others: first folder under the root
            low = [x.lower() for x in parts]
            if "packs" in low[:-1]: pack = parts[low.index("packs") + 1]
            elif parts[0] in collections and len(parts) > 2: pack = parts[0] + "/" + parts[1]   # folder of packs
            else: pack = parts[0] if len(parts) > 1 else os.path.basename(root)
            t = label_from(os.path.splitext(f)[0])
            for folder in reversed(parts[:-1]):
                if t: break
                t = label_from(folder)
            if not t: continue
            shots.append(dict(path=p, pack=pack, type=t, test_only=any(os.path.abspath(p).startswith(os.path.abspath(x)) for x in test_only)))
    return shots

def load(path):
    try:
        x, sr = sf.read(path, always_2d=True)
    except Exception:
        try:   # FL Studio's packs: RIFF/WAVE holding an Ogg Vorbis stream (format 0x674f)
            import io
            b = open(path, "rb").read(64 * 1024 * 1024)
            if not (b[:4] == b"RIFF" and b"OggS" in b[:4096]): return None
            x, sr = sf.read(io.BytesIO(b[b.index(b"OggS"):]), always_2d=True)
        except Exception:
            return None
    x = x.mean(axis=1)
    if len(x) == 0 or len(x) / sr > 4.0: return None   # empty, or a loop / long sound
    if sr != SR: x = resample_poly(x, SR, sr)
    start = int(np.argmax(np.abs(x) > 0.05 * (np.abs(x).max() + 1e-12)))
    x = x[start:start + int(1.5 * SR)]
    peak = np.abs(x).max()
    return x / peak if peak > 1e-6 else None

# ---------------------------------------------------------------- loops (kit_loops.py's grooves)
NAMES = {"kick", "snare", "closedHat", "openHat", "crash", "ride", "rimshot", "clap", "misc"}

def make_loop(kit, rng, seconds=8.0):
    step = 60 / rng.uniform(80, 170) / 4
    n = int(seconds * SR)
    x = np.zeros(n + 2 * SR)
    hits = []
    def place(t, s, gain_db):
        if t not in kit: return
        y = resample_poly(kit[t], 100, int(round(100 * rng.uniform(0.98, 1.02))))
        o = int(s * step * SR + rng.normal(0, 0.002) * SR)
        if o < 0: return
        x[o:o + len(y)] += y * 10 ** (gain_db / 20)
        hits.append((o, t))
    swing = rng.uniform(0, 0.25) if rng.random() < 0.4 else 0.0
    for s in range(int(seconds / step)):
        b = s % 16
        ss = s + (swing if s % 2 else 0.0)
        if b in (0, 8) or (b in (6, 10, 14) and rng.random() < 0.3) or rng.random() < 0.06: place("kick", ss, rng.uniform(-4, 0))
        if b in (4, 12) or (b == 8 and rng.random() < 0.15):   # backbeat, sometimes half-time
            r = rng.random()
            place("snare" if r < 0.55 else ("clap" if r < 0.8 else "rimshot"), ss, rng.uniform(-5, 0))
            if rng.random() < 0.25: place("clap", ss, rng.uniform(-8, -2))
        if s % 2 == 0 or rng.random() < 0.35:
            place("openHat" if rng.random() < 0.12 else "closedHat", ss, rng.uniform(-14, -5))
        if b == 0 and rng.random() < 0.25: place("crash", ss, rng.uniform(-10, -3))
        if s % 4 == 2 and rng.random() < 0.15: place("ride", ss, rng.uniform(-14, -6))
        if rng.random() < 0.04: place("misc", ss, rng.uniform(-12, -3))
    hits.sort()
    events = []
    for o, t in hits:
        if events and o - events[-1][0] <= int(0.010 * SR): events[-1][1].add(t)
        else: events.append([o, {t}])
    return (x[:n] * 0.3).astype(np.float32), events

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--roots", nargs="+", required=True)
    ap.add_argument("--test-only", nargs="*", default=[])
    ap.add_argument("--loops-per-pack", type=int, default=6)
    ap.add_argument("--test-share", type=float, default=0.2)
    ap.add_argument("--samples-per-loop", type=float, default=0,
                    help="if > 0, a pack gets max(--loops-per-pack, samples / this) loops, capped by --max-loops")
    ap.add_argument("--max-loops", type=int, default=60)
    ap.add_argument("--collections", nargs="*", default=[], help="first-level folder names that hold several packs")
    ap.add_argument("--skip", nargs="*", default=[], help="folders to leave out entirely (path prefixes)")
    a = ap.parse_args()
    rng = np.random.default_rng(2026)
    shots = [s for r in a.roots for s in scan(r, a.test_only, set(a.collections))]
    skip = [os.path.abspath(x).lower() for x in a.skip]
    shots = [s for s in shots if not any(os.path.abspath(s["path"]).lower().startswith(x) for x in skip)]
    packs = {}
    for s in shots: packs.setdefault(s["pack"], []).append(s)
    print(f"{len(shots)} labelled one-shots in {len(packs)} packs")
    counts = {"train": 0, "test": 0}
    os.makedirs(a.out, exist_ok=True)
    log = open(os.path.join(a.out, "packs.csv"), "w")
    log.write("pack,split,samples,types\n")
    for pack, lst in sorted(packs.items()):
        h = int(hashlib.sha1(pack.encode("utf-8")).hexdigest(), 16) % 1000 / 1000.0
        split = "test" if any(s["test_only"] for s in lst) or h < a.test_share else "train"
        bytype = {}
        for s in lst: bytype.setdefault(s["type"], []).append(s["path"])
        log.write(f"\"{pack}\",{split},{len(lst)},{'+'.join(sorted(bytype))}\n")
        if len({"kick", "closedHat"} & set(bytype)) + bool({"snare", "clap"} & set(bytype)) < 2:
            continue   # a groove needs at least two of: kick, a backbeat (snare/clap), hats
        out = os.path.join(a.out, split)
        os.makedirs(out, exist_ok=True)
        cache = {}
        nloops = a.loops_per_pack
        if a.samples_per_loop > 0: nloops = min(a.max_loops, max(nloops, int(len(lst) / a.samples_per_loop)))
        for k in range(nloops):
            kit = {}
            for t, paths in bytype.items():
                for _ in range(4):   # a few tries: some files are unreadable or too long
                    p = paths[rng.integers(len(paths))]
                    if p not in cache: cache[p] = load(p)
                    if cache[p] is not None: kit[t] = cache[p]; break
            if not kit: continue
            x, events = make_loop(kit, rng)
            name = re.sub(r"[^A-Za-z0-9]+", "_", pack)[:40] + f"_{k:02d}"
            sf.write(os.path.join(out, name + ".wav"), x, SR, subtype="FLOAT")
            with open(os.path.join(out, name + ".txt"), "w") as f:
                for o, labs in events: f.write(f"{o} {','.join(sorted(labs))}\n")
            counts[split] += 1
    log.close()
    print(f"loops: train {counts['train']}, test {counts['test']} -> {a.out} (pack split in packs.csv)")

if __name__ == "__main__":
    main()
