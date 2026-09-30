<!-- File: research/engine-shootout/README.md -->
# Engine shootout: STFT masks vs time-domain bands

A phase-2 experiment to settle D1 in `docs/02-algorithm.md` with measurements instead of opinions.
It's **not** part of the plugin build or CI: a single C++ file, compiled and run in Claude's
sandbox (verification level 2).

```
g++ -O2 -std=c++17 main.cpp -o shootout && ./shootout
```

## Method
- **A synthetic 4-bar drum loop at 48 kHz**, built from separate stems:
  - kick (pitch sweep plus click);
  - snare (190 Hz tone plus noise);
  - closed hats on every 8th note;
  - one open hat per bar.
  The kick and snare land on top of hats, just like a real loop.
- **The ideal result is known exactly**, because the stems exist: ideal = mix − target stem +
  process(target stem).
- **Every engine gets the same perfect hit timing**, so only the processing method differs.
- **Engines:**
  - **A: STFT gain mask** (Agents 1 + 2), at hop 128 (Agent 1) and hop 256 (Agent 2).
  - **B: 6 time-domain bands** (Claude). Complementary linear-phase FIR bands; the drum's share of
    each band gives an estimate of the drum, the real processor runs on that estimate, and the
    difference is added back.
  - **C: Hybrid.** An STFT soft mask gives the estimate, then the same time-domain processor as B.
- **Ownership shares, measured two ways:**
  - **oracle:** from the real stems at every moment. This is the upper bound.
  - **template:** each drum's average spectrum × its envelope. This is closer to what the real
    detector can know.
- **Score:** error = energy(output − ideal) / energy(ideal − input), in dB. Lower is better:
  −10 dB means the intended change is 90% right, −20 dB means 99%. **Pre-echo** counts only the
  error in the 1–25 ms **before** each hit, which is smearing ahead of the transient.

## Results (template shares, the realistic case)
| Test | A mask hop128 | A mask hop256 | **B 6 bands** | C hybrid |
|---|---|---|---|---|
| Neutral settings (transparency) | −272 dBFS ✔ | — | −317 dBFS ✔ | same as A ✔ |
| T1 kick level −6 dB | −27.1 | −27.7 | **−29.0** | −28.2 |
| T2 snare Crack +6 dB | −10.9, pre-echo −34 | −10.7, pre-echo −34 | **−19.9, no pre-echo** | −18.6, no pre-echo |
| T3 open-hat Decay shorter | −4.0 | −4.0 | −3.9 | −3.9 |
| T4 snare Compression 4:1 | −19.0, pre-echo −40 | −18.7, pre-echo −40 | **−24.8, no pre-echo** | −23.9, no pre-echo |
| T5 kick Saturation | **can't** (0.0) | **can't** (0.0) | **−39.8** | −39.9 |

The oracle shares give the same ranking; all the numbers are in `RESULTS.txt`.

## What it shows
1. **Both approaches are perfectly transparent** at neutral settings.
2. **Static level (T1): a tie.** The STFT's finer frequency ownership gives no measurable advantage
   over 6 bands on this loop.
3. **Transients and dynamics (T2, T4): time-domain wins clearly.** It gets about 99% right versus
   about 92–98%. The STFT mask also smears energy **before** the hit, the pre-echo Agent 2 warned
   about.
   *Correction to Claude's earlier claim:* a mask **can** do compression and attack shaping as
   time-varying gains. It just does them less accurately, and with pre-echo.
4. **Saturation and clipping (T5): only time-domain can do them at all.** A gain never creates
   harmonics.
5. **Decay shortening (T3) is weak for every method** (about 60% right), because the open hat's
   tail shares its band with snare noise and hats. **This is a limit of working on a mixed loop,
   not of either method.** The Decay controls need a gentle range and honest expectations.
6. **B ≈ C.** Once the processing runs in the time domain, it barely matters whether the drum
   estimate comes from 6 bands or from STFT bins.

## Limits of this test
- Synthetic sounds, 4 stems, mono, 48 kHz only, and perfect hit timing. Real loops are messier.
- Template shares still use the true envelope. The real detector's envelope will be noisier.
- **CPU isn't compared:** this is offline, unoptimised code.
- The next check is the same test on real multitrack drum stems, where the ideal is known too.
