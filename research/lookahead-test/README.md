<!-- File: research/lookahead-test/README.md -->
# Lookahead test: how many ms does detection need?

A phase-2 experiment to settle D2 in `docs/02-algorithm.md` with real drums. It's not part of the
plugin build. The audio stays outside the repo: `fetch_data.sh` downloads it, and
`lookahead_test.py` runs the test. The full output is in `RESULTS.txt`.

## Method
- **Question:** if the plugin may only see the first **W ms** of each hit (plus anything before
  it), how well can the hit's type be recognised? The plugin's lookahead ≈ W + about 3 ms (for the
  onset decision and the 2 ms gate pre-roll).
- **Acoustic set:** MDB Drums, 23 real recorded drum tracks (rock, jazz, funk, metal, …), 6,513 hit
  events.
  - Simultaneous hits count as one multi-label event.
  - Annotation times are refined to the nearest energy rise (±10 ms).
  - Tested on tracks the model never trained on (5-fold, grouped by track).
- **Electronic set:** 100 loops (85–150 bpm) assembled from 808/909-style one-shots (Dirt-Samples),
  4,319 events.
  - Includes claps (some layered on the snare), rimshots, ride and crash, with ±2 ms humanising,
    gain and pitch variation.
  - Each test loop uses only samples that were never in training, except rimshot, where there are
    only 3 samples.
- **Classifier:** a generic learned model (Extra-Trees on 32-band log-mel energies in time segments),
  **not** the plugin's rule-based scorer.
  - It measures how much information exists in the first W ms. The plugin's hand-made rules will
    score lower, but the point where more lookahead stops helping (the knee) is what we need.
- **Score:** average precision (AP, 0–1) per type. It's threshold-free and fair to rare types.
  Best-threshold F1 is also in `RESULTS.txt`.

## Results (average precision, 1.0 = perfect)
### Electronic loops
| W ms | kick | snare | closed hat | open hat | crash | ride | rim* | clap | misc | overall |
|---|---|---|---|---|---|---|---|---|---|---|
| 10 | 0.90 | 0.76 | 0.99 | 0.80 | 0.31 | 0.68 | 0.78 | 0.35 | 0.13 | 0.63 |
| 15 | 0.96 | 0.90 | 0.99 | 0.89 | 0.63 | 0.77 | 0.91 | 0.57 | 0.19 | 0.76 |
| 20 | 0.98 | 0.93 | 1.00 | 0.92 | 0.76 | 0.81 | 0.94 | **0.82** | 0.23 | 0.82 |
| 30 | 0.99 | 0.93 | 1.00 | 0.93 | 0.78 | 0.83 | 0.96 | 0.83 | 0.35 | 0.85 |
| **40** | 0.99 | 0.94 | 1.00 | 0.94 | 0.79 | 0.85 | 0.98 | 0.85 | 0.47 | **0.87** |
| **50** | 0.99 | 0.94 | 1.00 | 0.94 | 0.82 | 0.85 | 0.98 | 0.86 | 0.49 | **0.87** |
| 60 | 0.99 | 0.95 | 1.00 | 0.94 | 0.82 | 0.87 | 0.98 | 0.87 | 0.45 | 0.87 |
| 100 | 0.99 | 0.95 | 1.00 | 0.92 | 0.86 | 0.89 | 0.97 | 0.89 | 0.38 | 0.87 |

\* Only 3 rimshot samples, so they couldn't be held out, and that column is optimistic.

### Acoustic recordings
| W ms | kick | snare | closed hat | open hat | crash | ride | misc | overall** |
|---|---|---|---|---|---|---|---|---|
| 10 | 0.81 | 0.81 | 0.68 | 0.13 | 0.07 | 0.54 | 0.04 | 0.39 |
| 20 | 0.87 | 0.88 | 0.79 | 0.28 | 0.09 | 0.57 | 0.13 | 0.45 |
| 30 | 0.88 | 0.90 | 0.83 | 0.34 | 0.13 | 0.53 | 0.28 | 0.49 |
| **40** | 0.89 | 0.91 | 0.84 | 0.37 | 0.21 | 0.54 | 0.38 | 0.52 |
| **50** | 0.89 | 0.91 | 0.85 | 0.40 | 0.29 | 0.55 | 0.41 | 0.54 |
| 60 | 0.89 | 0.91 | 0.86 | 0.43 | 0.32 | 0.54 | 0.43 | 0.55 |
| 100 | 0.89 | 0.91 | 0.87 | 0.51 | 0.43 | 0.60 | 0.44 | 0.59 |

\*\* The overall acoustic score also includes rimshot, but the dataset has only 38 side-stick hits
and they weren't learnable (≈ 0.01–0.04).

## What it shows
1. **Below 20 ms is not enough.** Snare, clap and crash are still climbing fast. The clap jumps
   from 0.57 to 0.82 between 15 and 20 ms, which is when its second and third bursts arrive.
2. **Kick, snare and closed hat are settled by 20–30 ms** in both sets.
3. **The electronic set is fully settled by 40–50 ms.** 50 ms gives the same score as 100 ms.
4. **Acoustic open hat and crash keep improving up to 100 ms and beyond.** Their identity is in the
   tail, and that's too long to wait for. The "label now, correct later" crossfade from
   `docs/02-algorithm.md` is the right tool for them, not more lookahead.
5. Ride on acoustic (jazz) recordings is hard at any length (≈ 0.55). It gets confused with hats
   played at the same time. That's a feature problem, not a lookahead problem.

## Recommendation
- **Minimum:** 20 ms. **Knee:** 40 ms. **Recommended: 50 ms**, because latency is cheap for this
  plugin: one instance per project on a drum bus, compensated by FL Studio.
- **Beyond 50 ms:** no gain on electronic drums. For acoustic cymbals, the late-correction mechanism
  (re-label within 100 ms, 5 ms crossfade) does the rest.
- **The engine fits:** the hybrid processing engine needs about 21 ms, inside the 50 ms.

## Limits
- Generic learned classifier. The plugin's rule-based scorer will be less accurate, and its knee
  should be re-measured with the same script once it exists in `dsp/` (phase 3).
- The electronic set comes from a small sample pool (clap 6, ride 7, rimshot 3 samples). The real
  loops you use in FL Studio should be tested at home.
- Mono, 44.1 kHz only.
