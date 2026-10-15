# Leak check: are any Dirt-Samples one-shots (the electronic / kit test sets) byte-identical, or identical
# as audio (same samples after decoding), to a sound in the training folders?
import hashlib, os, sys
import numpy as np, soundfile as sf
def audio_key(p):
    try:
        x, sr = sf.read(p, always_2d=True, dtype="float32")
    except Exception:
        return None
    x = x.mean(1); x = x[np.abs(x) > 1e-4]
    if len(x) < 100: return None
    x = x / (np.abs(x).max() + 1e-12)
    return hashlib.md5(np.round(x[:4000], 3).tobytes()).hexdigest()
def walk(roots):
    for r in roots:
        for d, _, fs in os.walk(r):
            for f in fs:
                if f.lower().endswith((".wav", ".aif", ".aiff", ".flac")): yield os.path.join(d, f)
dirt = {}
for p in walk([r"D:\dm-data\pub\dirt"]):
    k = audio_key(p)
    if k: dirt.setdefault(k, []).append(p)
print("dirt sounds:", len(dirt), flush=True)
roots = [r"D:\dm-data\web", r"D:\stuff to save", r"D:\MHA", r"D:\Sickrate & SIIK Essentials I (WAV Samples)",
         r"D:\Sickrate and SIIK Essentials II Full Pack", r"D:\Splice", r"D:\Splice2", r"D:\CymaticsLofiToolkit-V1-m2d",
         r"D:\Drum Stems", r"D:\Downloads old\DABRO Music - Future Bass Vol.2", r"C:\Users\didi\Documents\Splice - user-2587851674",
         r"C:\Program Files\Image-Line\FL Studio 2024\Data\Patches\Packs"]
n = hits = 0
for p in walk(roots):
    if "loop" in p.lower(): continue
    n += 1
    k = audio_key(p)
    if k in dirt:
        hits += 1; print("SAME AUDIO:", p, "==", dirt[k][0], flush=True)
print(f"checked {n} training sounds, {hits} identical to a Dirt test sound")
