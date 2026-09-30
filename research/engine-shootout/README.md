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

---

# Shootout v2: after the agents' review (`shootout2.cpp`, `RESULTS2.txt`)

Both agents (`prompts/answers/03-*.md`) said v1 was too easy. It never put two sounds in the
**same frequency range at the same time**, which is the case that decides the design. v2 fixes
every critique that can be tested offline:

| Critique | Fix in v2 |
|---|---|
| No same-range overlaps (both agents) | Bass with an 808 glide under the kick; clap layered on the snare; open hat over a crash tail |
| Drums too simple (both) | Kick with a body mode, pitch drop and click; snare with 3 modes plus a 3.8 kHz ring |
| Try a dual-resolution STFT (both) | Engine **Ad**: an N = 2048 mask below 1.5 kHz and N = 256 above. Engine **Cd**: the same split used as the estimate, then time-domain processors. |
| L2 metric is phase-sensitive (both) | A second, phase-insensitive **spectral** error next to it |
| No onset jitter (A1) | A mode with ±2 ms timing error |
| "Saturation impossible" is unfair (A1) | Agreed: a mask plus a time-domain nonlinearity **is** C. The A columns show n/a for saturation. |
| New cases | T6 clap level (layered on the snare), T7 kick Top EQ (the click shares 1.5–5 kHz with hats) |
| Latency (Claude's own check) | A 2048 window alone is 43 ms, so it doesn't fit the 30 ms lookahead. **Cd1k** uses 1024 for the lows (21 ms). |

## Results v2 (template shares, the realistic case; L2 error in dB, lower is better)
| Test | A mask | Ad dual mask | B 6 bands | C hybrid | **Cd1k hybrid dual (fits 30 ms)** |
|---|---|---|---|---|---|
| T1 kick −6 dB, bass under it | −5.3 | −5.1 | −6.0 | −5.3 | −5.3 |
| T2 snare Crack, clap on it | −7.2 | −6.6 | −9.4 | −9.7 | **−10.0** |
| T3 open-hat Decay, crash under it | −2.1 | −2.2 | −2.0 | −2.0 | −2.2 |
| T4 snare compression, clap on it | −9.9 | −10.1 | −9.4 | −10.4 | **−10.4** |
| T5 kick saturation, bass under it | n/a | n/a | **−5.9** | −5.2 | −5.2 |
| T6 clap −6 dB, on the snare | −5.1 | −5.3 | −4.6 | −5.2 | **−5.3** |
| T7 kick Top +6 dB, hats in range | −10.5 | **−18.7** | −8.4 | −10.5 | **−18.7** |

## What v2 changes
1. **Claude's 6-band engine (B) loses.** On the kick Top test it's 10 dB worse than the
   dual-resolution hybrid, because a 1.5–5 kHz band can't tell the kick's 3 ms click from the
   hats. Both agents predicted exactly this.
2. **The winner is Cd1k: a dual-resolution STFT *estimate* plus *time-domain* processors.** It's best
   or tied-best on 6 of 7 tests, and only 0.7 dB behind B on saturation. That's Agent 2's verdict (C)
   with the dual-resolution idea from both agents, in a version that fits the 30 ms latency.
3. **Pure masks still lose the transient test** (−7 vs −10 dB) and can't saturate.
4. **The sobering part: same-range overlaps cost every method a lot.** v1 was 99% right; v2 is
   about 70–90% right for level and dynamics, and about 40% for Decay. A kick change will partly
   move a bass playing under it, and a snare change will partly move a clap layered on it. This is
   physics on a mixed loop, not an engine bug. Whether it's audible is a question for the listening
   test at home.
5. **±2 ms jitter** only affects the timed tests (T2, T3), and only slightly (about 2 dB on T2).

## Still not tested
Real multitrack stems (A1's main request), stereo, a perceptual metric (A2: PEAQ/ViSQOL), and
listening. The decay ideas are still untested: A1's predictive decay model and A2's spectral
floor. Real-stem, stereo and decay tests go into phase 3 as the first `dsp/` tests.
