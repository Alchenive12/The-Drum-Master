# File: research/snare-train/synth_snares.py
#
# Licence-free training snares: drum-machine style (808 / 909 / modern layered) and acoustic-style
# snares, every parameter drawn at random, plus other one-shots from ../classifier-train/synth_aug.py
# for balance. Our own generated audio, so the "clean" model can train on it with no licence question.
# The old test snare was mostly bright noise and got read as a hat; these have the tonal body
# (180-350 Hz), the pitch drop and the noise "snappy" balance real snares have.
#
# Output: <out>/<type>/<name>.wav (24-bit 48 kHz) - folder names are the labels for make_list.py.
# Usage: python3 synth_snares.py <out_dir> [num_snares] [num_each_other]
import os, sys
import numpy as np
import soundfile as sf
from scipy.signal import butter, lfilter

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "classifier-train"))
import synth_aug as sa   # noqa: E402

SR = 44100
rng = np.random.default_rng(808)

def bp(x, lo=None, hi=None, order=2):
    if lo: b, a = butter(order, lo / (SR / 2), "high"); x = lfilter(b, a, x)
    if hi: b, a = butter(order, min(hi, 0.45 * SR) / (SR / 2), "low"); x = lfilter(b, a, x)
    return x

def env(n, tau, attack=0.0):
    e = np.exp(-np.arange(n) / (tau * SR))
    if attack > 0: e *= np.minimum(1.0, np.arange(n) / (attack * SR))
    return e

def snare808():
    n = int(rng.uniform(0.25, 0.6) * SR); t = np.arange(n) / SR
    f1 = rng.uniform(160, 260); f2 = f1 * rng.uniform(1.5, 2.0)
    tone = (np.sin(2 * np.pi * f1 * t) * env(n, rng.uniform(0.03, 0.09)) + rng.uniform(0.3, 0.9) * np.sin(2 * np.pi * f2 * t) * env(n, rng.uniform(0.02, 0.06)))
    snappy = bp(rng.standard_normal(n), rng.uniform(1500, 5000), rng.uniform(9000, 16000)) * env(n, rng.uniform(0.05, 0.2))
    return rng.uniform(0.4, 1.0) * tone + rng.uniform(0.3, 1.0) * snappy

def snare909():
    n = int(rng.uniform(0.25, 0.6) * SR); t = np.arange(n) / SR
    f0 = rng.uniform(170, 280); drop = rng.uniform(1.1, 1.8); tau = rng.uniform(0.005, 0.02)
    ph = 2 * np.pi * (f0 * t + f0 * (drop - 1) * tau * (1 - np.exp(-t / tau)))
    body = (np.sin(ph) + rng.uniform(0.2, 0.6) * np.sin(1.62 * ph)) * env(n, rng.uniform(0.03, 0.08))
    noise = bp(rng.standard_normal(n), rng.uniform(600, 2500), rng.uniform(7000, 15000)) * env(n, rng.uniform(0.06, 0.25))
    return rng.uniform(0.5, 1.0) * body + rng.uniform(0.4, 1.0) * noise

def snareAcoustic():
    n = int(rng.uniform(0.3, 0.8) * SR); t = np.arange(n) / SR
    modes = rng.uniform(170, 260) * np.array([1.0, 1.59, 2.14, 2.30, 2.65])
    body = sum(rng.uniform(0.2, 1.0) / (k + 1) * np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) for k, f in enumerate(modes)) * env(n, rng.uniform(0.04, 0.12))
    wires = bp(rng.standard_normal(n), rng.uniform(1500, 4000), rng.uniform(8000, 14000)) * env(n, rng.uniform(0.08, 0.3), 0.002)
    stick = bp(rng.standard_normal(n), 2000, None) * env(n, 0.0015)
    room = bp(rng.standard_normal(n), 300, 6000) * env(n, rng.uniform(0.15, 0.5)) * rng.uniform(0.0, 0.15)
    return rng.uniform(0.5, 1.0) * body + rng.uniform(0.4, 1.0) * wires + rng.uniform(0.2, 0.6) * stick + room

def snareModern():
    """Layered trap / EDM snare: a drum-machine snare plus a clap-ish or noise layer and saturation."""
    base = [snare808, snare909, snareAcoustic][int(rng.integers(0, 3))]()
    layer = sa.clap() if rng.random() < 0.4 else bp(rng.standard_normal(len(base)), 3000, None) * env(len(base), rng.uniform(0.02, 0.08))
    m = min(len(base), len(layer)); x = base[:m] + rng.uniform(0.1, 0.5) * layer[:m]
    return np.tanh(rng.uniform(1.0, 3.0) * x / (np.abs(x).max() + 1e-9))

def finish(x):
    x = x * rng.uniform(0.85, 1.0) / (np.abs(x).max() + 1e-9)
    if rng.random() < 0.5:                       # random tilt EQ: dark or bright mixes
        x = bp(x, None, rng.uniform(5000, 12000)) if rng.random() < 0.5 else x + rng.uniform(0.1, 0.5) * bp(x, 4000, None)
    x[-min(len(x), 200):] *= np.linspace(1, 0, min(len(x), 200))
    return (x / (np.abs(x).max() + 1e-9) * 0.9).astype(np.float32)

def main():
    out = sys.argv[1]; ns = int(sys.argv[2]) if len(sys.argv) > 2 else 600; no = int(sys.argv[3]) if len(sys.argv) > 3 else 150
    makers = [("Snare", snare808), ("Snare", snare909), ("Snare", snareAcoustic), ("Snare", snareModern)]
    os.makedirs(os.path.join(out, "Snares"), exist_ok=True)
    for k in range(ns):
        name, f = makers[k % len(makers)]
        sf.write(os.path.join(out, "Snares", f"Synth {name} {f.__name__} {k:04d}.wav"), finish(f()), SR, subtype="PCM_24")
    others = [("Kicks", "Kick", sa.MAKERS["kick"]), ("Claps", "Clap", sa.clap), ("Hats", "Closed Hat", sa.MAKERS["closedHat"]),
              ("Open Hats", "Open Hat", sa.MAKERS["openHat"]), ("Rims", "Rim", sa.rim), ("Crashes", "Crash", sa.crash),
              ("Rides", "Ride", sa.ride), ("Percs", "Tom", sa.tom)]
    for folder, name, f in others:
        os.makedirs(os.path.join(out, folder), exist_ok=True)
        for k in range(no):
            x = finish(f())
            if len(x) > int(3.9 * SR): x = x[: int(3.9 * SR)]
            sf.write(os.path.join(out, folder, f"Synth {name} {k:04d}.wav"), x, SR, subtype="PCM_24")
    print("snares:", ns, "others:", no, "each")

if __name__ == "__main__":
    main()
