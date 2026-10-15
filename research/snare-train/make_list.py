# File: research/snare-train/make_list.py
# Builds the one-shot list (path, pack, group, type) for oneshot_extract.cpp from a list of audio
# paths (one per line, relative to <base>), using the same name rules as dsp/tools/sample_packs.h.
# Pack = the top folder, or top/second folder for collection folders like "stuff to save".
# Usage: python3 make_list.py <base> <paths.txt> <list.tsv> [collection_folder ...]
import os, re, sys

TYPES = ["kick", "snare", "closedHat", "openHat", "crash", "ride", "rimshot", "clap", "misc"]

def has_word(s, w):
    for m in re.finditer(re.escape(w), s):
        b, e = m.start(), m.end()
        if (b == 0 or not s[b - 1].isalpha()) and (e >= len(s) or not s[e].isalpha()):
            return True
    return False

def has_any(s, words, whole):
    return any(has_word(s, w) if whole else w in s for w in words)

RULES = [
    ("openHat", "open hat", ["open hat", "openhat", "open_hat", "open-hat", "open hh", "ohh"], ["oh", "ohat"]),
    ("closedHat", "closed hat", ["closed hat", "closedhat", "closed_hat", "hihat", "hi-hat", "hi hat", "hi_hat"], ["hat", "hats", "hh", "chh", "ch"]),
    ("crash", "crash", ["crash", "cymbal"], ["cym"]),
    ("ride", "ride", ["ride"], []),
    ("rimshot", "rimshot", ["rimshot", "sidestick", "side stick", "side_stick"], ["rim", "rims", "stick"]),
    ("clap", "clap", ["clap"], ["snap", "snaps"]),
    ("snare", "snare", ["snare"], ["snr", "sd"]),
    ("kick", "808", ["808"], []),
    ("kick", "kick", ["kick", "bassdrum", "bass drum"], ["kik", "bd"]),
    ("misc", "perc", ["shaker", "tambourine", "cowbell", "conga", "bongo", "woodblock", "clave"], ["tom", "toms", "perc", "percs", "tamb", "block"]),
]

def label_from(raw):
    s = raw.lower()
    if "open" in s and (has_any(s, ["hihat", "hi-hat", "hi hat", "hi_hat"], False) or has_any(s, ["hat", "hats", "hh"], True)):
        return "openHat", "open hat"
    for t, g, sub, word in RULES:
        if has_any(s, sub, False) or has_any(s, word, True):
            return t, g
    return None

def main():
    base, paths, out = sys.argv[1], sys.argv[2], sys.argv[3]
    collections = set(sys.argv[4:])
    n = skipped = 0
    with open(out, "w") as f:
        for rel in open(paths, encoding="utf-8", errors="surrogateescape"):
            rel = rel.rstrip("\n")
            if rel.startswith("./"): rel = rel[2:]
            if not rel.lower().endswith(".wav"): skipped += 1; continue
            parts = rel.split("/")
            if "loop" in rel.lower(): skipped += 1; continue
            lab = label_from(os.path.splitext(parts[-1])[0])
            for folder in reversed(parts[1:-1]):
                if lab: break
                lab = label_from(folder)
            if not lab: skipped += 1; continue
            pack = "/".join(parts[:2]) if parts[0] in collections and len(parts) > 2 else parts[0]
            f.write(f"{os.path.join(base, rel)}\t{pack}\t{lab[1]}\t{TYPES.index(lab[0])}\n")
            n += 1
    print(f"{n} labelled one-shots, {skipped} skipped (loops, unlabelled, not WAV)")

if __name__ == "__main__":
    main()
