# File: research/classifier-train/synth_aug.py
#
# Randomised synthetic electronic drums for classifier training: covers what the Groove MIDI
# Dataset lacks (claps, 808-style kicks, drum-machine hats). Our own generated audio, so no
# licence question. Every parameter is drawn at random per hit, so the model learns the category,
# not one sample. Output: <out>/<name>.wav (float32 mono 44.1 kHz) + <name>.txt labels.
# Usage: python3 synth_aug.py <out_dir> [num_loops]
import os, sys
import numpy as np
import soundfile as sf
from scipy.signal import butter, lfilter

SR = 44100
rng = np.random.default_rng(2026)

def band(x, lo=None, hi=None, order=2):
    if lo: b, a = butter(order, lo / (SR / 2), "high"); x = lfilter(b, a, x)
    if hi: b, a = butter(order, min(hi, 0.45 * SR) / (SR / 2), "low"); x = lfilter(b, a, x)
    return x

def env(n, tau):
    return np.exp(-np.arange(n) / (tau * SR))

def kick808():
    n = int(rng.uniform(0.3, 1.2) * SR); t = np.arange(n) / SR
    f1, f0, tau = rng.uniform(38, 65), rng.uniform(80, 220), rng.uniform(0.01, 0.06)
    ph = 2 * np.pi * (f1 * t + (f0 - f1) * tau * (1 - np.exp(-t / tau)))
    x = np.sin(ph) * env(n, rng.uniform(0.15, 0.9))
    x += rng.uniform(0, 0.3) * band(rng.standard_normal(n), 1500, 8000) * env(n, 0.003)
    return np.tanh(rng.uniform(1, 4) * x)

def kickAcoustic():
    n = int(0.45 * SR); t = np.arange(n) / SR
    f1, f0, tau = rng.uniform(45, 70), rng.uniform(110, 180), rng.uniform(0.01, 0.03)
    x = np.sin(2 * np.pi * (f1 * t + (f0 - f1) * tau * (1 - np.exp(-t / tau)))) * env(n, rng.uniform(0.08, 0.3))
    return x + rng.uniform(0.1, 0.5) * band(rng.standard_normal(n), 1000, 6000) * env(n, rng.uniform(0.002, 0.008))

def snare():
    n = int(0.5 * SR); t = np.arange(n) / SR
    body = sum(rng.uniform(0.2, 0.6) * np.sin(2 * np.pi * f * t) * env(n, rng.uniform(0.02, 0.08)) for f in rng.uniform(150, 450, 2))
    noise = band(rng.standard_normal(n), rng.uniform(800, 3000), rng.uniform(8000, 16000)) * env(n, rng.uniform(0.06, 0.25))
    return rng.uniform(0.3, 1.0) * body + rng.uniform(0.4, 1.0) * noise

def clap():
    n = int(0.5 * SR); x = np.zeros(n)
    bursts, gap = int(rng.integers(2, 6)), rng.uniform(0.004, 0.012)
    noise = band(rng.standard_normal(n), rng.uniform(600, 1500), rng.uniform(2500, 7000))
    e = np.zeros(n)
    for k in range(bursts):
        s = int(k * gap * SR)
        e[s:] += rng.uniform(0.6, 1.0) * env(n - s, rng.uniform(0.002, 0.006))
    s = int((bursts - 1) * gap * SR)
    e[s:] += rng.uniform(0.2, 0.6) * env(n - s, rng.uniform(0.03, 0.2))
    return noise * e

def metal(decay):
    n = int(max(decay * 6, 0.1) * SR); t = np.arange(n) / SR
    osc = sum(np.sign(np.sin(2 * np.pi * f * t)) for f in rng.uniform(200, 900, 6))   # 808-style square cluster
    x = rng.uniform(0, 0.6) * osc + rng.uniform(0.5, 1.0) * rng.standard_normal(n)
    return band(x, rng.uniform(4000, 9000), None, 4) * env(n, decay)

def crash():
    n = int(rng.uniform(1.0, 2.5) * SR)
    return band(rng.standard_normal(n), rng.uniform(600, 2500), 16000) * env(n, rng.uniform(0.5, 1.5)) * np.minimum(1, np.arange(n) / (0.003 * SR))

def ride():
    n = int(1.5 * SR); t = np.arange(n) / SR
    p = sum(np.sin(2 * np.pi * f * t) for f in rng.uniform(2500, 7000, 5)) * env(n, rng.uniform(0.3, 1.0))
    return rng.uniform(0.1, 0.3) * p + band(rng.standard_normal(n), 5000, None) * (env(n, 0.03) + rng.uniform(0.1, 0.3) * env(n, 0.8))

def rim():
    n = int(0.15 * SR); t = np.arange(n) / SR
    f = rng.uniform(900, 3000)
    return np.sin(2 * np.pi * f * t) * env(n, rng.uniform(0.01, 0.04)) + rng.uniform(0.2, 0.6) * rng.standard_normal(n) * env(n, 0.002)

def tom():
    n = int(0.6 * SR); t = np.arange(n) / SR
    f1 = rng.uniform(70, 250)
    return np.sin(2 * np.pi * (f1 * t + 0.3 * f1 * 0.05 * (1 - np.exp(-t / 0.05)))) * env(n, rng.uniform(0.15, 0.5))

MAKERS = {"kick": lambda: kick808() if rng.random() < 0.6 else kickAcoustic(), "snare": snare, "clap": clap,
          "closedHat": lambda: metal(rng.uniform(0.012, 0.06)), "openHat": lambda: metal(rng.uniform(0.15, 0.6)),
          "crash": crash, "ride": ride, "rimshot": rim, "misc": tom}

def place(x, s, y, gain_db):
    y = y / (np.abs(y).max() + 1e-9) * 10 ** (gain_db / 20)
    e = min(len(x), s + len(y))
    if s < len(x):
        f = min(len(y), e - s); y = y[:f].copy(); y[-min(len(y), 220):] *= np.linspace(1, 0, min(len(y), 220))
        x[s:e] += y

def loop(seconds=8.0):
    n = int(seconds * SR); x = np.zeros(n + 3 * SR); hits = []
    step = 60 / rng.uniform(80, 170) / 4
    def hit(t, s, g):
        o = int(s * step * SR + rng.normal(0, 0.002) * SR)
        if o >= 0: place(x, o, MAKERS[t](), g); hits.append((o, t))
    for s in range(int(seconds / step)):
        b = s % 16
        if b in (0, 10) or rng.random() < 0.1: hit("kick", s, rng.uniform(-4, 0))
        if b in (4, 12):
            r = rng.random()
            hit("snare" if r < 0.4 else ("clap" if r < 0.8 else "rimshot"), s, rng.uniform(-6, 0))
            if rng.random() < 0.3: hit("clap", s, rng.uniform(-10, -3))
        if s % 2 == 0 or rng.random() < 0.25: hit("openHat" if rng.random() < 0.12 else "closedHat", s, rng.uniform(-16, -6))
        if b == 0 and rng.random() < 0.2: hit("crash", s, rng.uniform(-12, -4))
        if s % 4 == 2 and rng.random() < 0.15: hit("ride", s, rng.uniform(-14, -6))
        if rng.random() < 0.03: hit("misc", s, rng.uniform(-10, -3))
        if rng.random() < 0.02: hit("clap", s, rng.uniform(-8, -2))
    hits.sort()
    events = []
    for o, t in hits:
        if events and o - events[-1][0] <= int(0.010 * SR): events[-1][1].add(t)
        else: events.append([o, {t}])
    return (x[:n] * 0.25).astype(np.float32), events

def main():
    out = sys.argv[1]; count = int(sys.argv[2]) if len(sys.argv) > 2 else 150
    os.makedirs(out, exist_ok=True)
    for k in range(count):
        x, events = loop()
        sf.write(os.path.join(out, f"synth{k:03d}.wav"), x, SR, subtype="FLOAT")
        with open(os.path.join(out, f"synth{k:03d}.txt"), "w") as f:
            for o, types in events: f.write(f"{o} {','.join(sorted(types))}\n")
    print("loops:", count)

if __name__ == "__main__":
    main()
