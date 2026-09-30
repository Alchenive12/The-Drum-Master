<!-- File: docs/02-algorithm.md -->
# The Drum Master — Phase 2b: Detection and processing algorithm

> Status: **proposal, waiting for approval** (2 decisions at the bottom).
> Sources: `prompts/answers/02-claude.md` (written blind, first) · `02-agent1.md` · `02-agent2.md`.
> Same rule as phase 2a: **an idea is in if 2 of 3 sources suggested it**. Single-source ideas are
> judged on merit and marked. **[guess]** values are tuned in phase 3 against the tests.

## Where all three agree (the backbone)
- **Detection:** multi-band spectral flux with an adaptive median threshold and peak picking. A
  separate detector per band is what makes simultaneous hits (kick + hat) detectable.
- **Features:** band energy ratios, decay slope, a tonality measure, and a burst count for claps.
- **The hard pairs are solved the same way:**
  - closed vs open hat by decay;
  - crash vs ride by tonality and spread;
  - clap by its bursts;
  - rimshot as short and tonal.
- **Confidence:** if it's below a threshold, the hit goes to Misc.
- **Hit end:** the tail drops a fixed amount below its peak, or reaches the noise floor, or the
  same type hits again.
- **Auto detect:** a type is present after several confident hits, and the plugin adapts to this
  loop. Manual mode removes the other types.
- **Transparency** at default is a hard test.

## Where Claude was outvoted
| Claude proposed | Outcome |
|---|---|
| IIR filterbank for analysis | **Replaced by an STFT** (A1 + A2). It also yields the spectral features in one place. |
| Clap needs ≥ 2 bursts | **≥ 3 bursts** (A1 + A2) |
| Grouping window ±3 ms | **±6 ms**, between A1's 15 ms and Claude's 3 ms, judged on the STFT hop [guess] |
| Floor + 3 dB for tail end | **Floor + 6 dB** (A1), a safer margin |
| Present after 3 hits | **4 hits** (A1) plus A1's "spread out" guard |
| Time-domain band processing | **Both agents chose STFT masks. See D1.** |

## The design

### 1. Pipeline
1. Delay the stereo input by the lookahead **D** (the audio path). Analysis runs on the undelayed
   mono sum, which is the "future" relative to the output.
2. **STFT analysis:** Hann window, N = 1024 with hop 128 at 44.1/48 kHz, and N = 2048 with hop 256
   at 88.2/96 kHz. [A1; A2 similar]
3. Band energies over **6 analysis bands** (A1), with a per-band onset detector on each.
4. Group the per-band onsets into **events**.
5. Split each event into drum types with **templates** (multi-label), then score each candidate
   type with **rule-based soft scores**. Low confidence goes to Misc.
6. Track each hit's tail until it ends.
7. Apply each active drum's settings to its frequency range, weighted by its **share** of that
   range (see D1 for how), then the global section.

### 2. Lookahead
- **D = 30 ms**, reported to the host as latency (Claude + A2; A1 says 20). At the moment a hit
  starts playing, about 28 ms of it has already been analysed.
- **The gate starts 2 ms before the transient**, so no attack is ever cut. [Claude]
- **Hat revision:** a hit is labelled provisionally, then re-scored once decay evidence is clear
  (up to 100 ms after the onset). If the label changes, the settings crossfade over 5 ms.
  [A1 + Claude]
- **See D2** for whether the lookahead should be adjustable.

### 3. Onset detection
| Band | Range | Home drums |
|---|---|---|
| B0 | 20–120 Hz | kick sub |
| B1 | 120–400 Hz | kick body, snare body |
| B2 | 400–1500 Hz | snare, clap body, rim wood |
| B3 | 1.5–5 kHz | snare crack, rim, clap, kick top, hat tick |
| B4 | 5–10 kHz | hats, cymbals |
| B5 | 10–20 kHz | air, sizzle |

- **Band energy:** E_b[n] = Σ_{k∈B_b} |X(k,n)|². **Flux:**
  O_b[n] = max(0, 10·log10(E_b[n] + ε) − 10·log10(E_b[n−1] + ε)). [A1, A2]
- **Threshold:** T_b[n] = α·median(O_b over the last 100 ms) + δ, with **α = 2.4 − 1.2·s**. The
  sensitivity knob s ∈ [0, 1] scales it (A2's idea), and s = 0.5 gives A1's α = 1.8. δ = 0.5 dB.
  [A1 + A2, guess]
- **Peak pick:** O_b[n] > T_b[n] and O_b[n] ≥ O_b[n ± 1]. [all 3]
- **Refractory time per band:** 25 ms. [Claude]
- **Grouping:** onsets within ±6 ms form one event. The onset is then refined to the sample using
  a fast time-domain envelope. [Claude + A1, window judged]

### 4. Classification
- **Event spectrum:** p_b is the per-band power rise from before the onset to its peak.
- **Template split (multi-label), single-source, kept on merit:**
  - Solve p ≈ Σ_t A_t·S_t with A_t ≥ 0, where S_t is each type's 6-band template (A1 calls these
    "spectral prototypes M_d").
  - Types not allowed are left out, and **Misc has a flat template** so unexplained energy lands
    there.
  - Why keep it: one step gives both the multi-label split and the **shares** that processing
    needs (§7). A1's per-band-group classification does the first but not the second.
- **Features per candidate** (over the ~28 ms analysed):

| Feature | Definition | Sources |
|---|---|---|
| Band ratios | r_b = E_b / Σ E | all 3 |
| Centroid / spread | Σ f·\|X\|² / Σ \|X\|², and the variance around it | A1 + A2 |
| Tonality | Spectral flatness and crest per band | all 3 (flatness / HNR / crest) |
| Decay slope | Linear-regression slope of E_b after the peak, in dB/ms | all 3 |
| Attack slope | Rise rate of the fast envelope | A1 + Claude |
| Burst count | Envelope peaks ≥ 3 ms apart, dips ≥ 3 dB, within 30 ms | all 3 |
| Stereo correlation (optional) | ρ = Σ L·R / √(Σ L²·Σ R²) in B4 | A1 only; cheap, helps crash vs hat |

  Out: zero-crossing rate (A1 only; centroid covers it).
- **Scoring:** each type's score is the product of Gaussian memberships on its key features. A
  softmax then gives p_c, and confidence = the largest p_c. [A1]
- **Rules for the hard pairs** [guess thresholds, to tune]:
  - Closed hat: t60 < 80 ms. Open hat: t60 > 150 ms. In between, confidence drops and the
    provisional-then-revise path is used. [A1 + Claude]
  - Crash: low tonality, broadband, slow decay. Ride: tonal ping in B2–B3. **If the two are close,
    the hit goes to Misc** rather than risk mangling a crash. [A1, fits "a wrong guess never sounds
    bad"]
  - Clap: ≥ 3 bursts in 30 ms. Rimshot: decay < 40 ms and tonal in 1–3 kHz. Snare: body energy in
    B1. [all 3]
- **Misc** if the confidence is below θ_conf = 0.55 − 0.2·s. [Claude, A1 and A2 range 0.45–0.65]
- **Machine learning:** not in version 1. Rule-based scoring comes first (Claude + A1).
  - A small RF (A2) or MLP (A1) can replace the scorer later, but only if the rules miss the
    targets on real loops.
  - It would need labelled data for all 9 types, and most public datasets only label kick, snare
    and hat.

### 5. Hit duration
- **The hit ends when the first of these happens:**
  - the envelope stays below max(peak − R_t, floor + 6 dB) for a **20 ms hangover** [A1];
  - the same type retriggers, with a **5 ms crossfade** [Claude + A1];
  - a duration cap is reached (crash 4 s, open hat and ride 2 s, others 1 s) [Claude].
- **R_t per type** (dB): kick 36, snare 36, rim 30, clap 36, closed hat 30, open hat 40, ride 40,
  crash 40, misc 30. These sit between Claude's values, A1's 40 and A2's 36. [guess]
- **Flam guard:** if the envelope rises back above peak − 20 dB inside the tail without a new onset,
  the hit is extended. [A1, merit]

### 6. Auto detect and manual mode
- **Present:** ≥ 4 hits with mean confidence ≥ 0.65, spread over at least a third of the playback
  so far. The string drops in **as soon as** this is met, live, as the concept requires. [A1 +
  Claude]
- **Adapting to the loop:**
  - Each type's templates and feature centres move toward this loop's confident hits.
  - The move is capped at 50% (A1), the learned values are saved with the session, and "reset
    learning" restores the generic ones. [all 3]
- **Stopping:** when host playback stops. `VERIFY(host)`: how FL Studio reports transport.
- **Manual mode:** types that aren't selected are removed from the templates and scoring. Their
  energy goes to the selected types or to Misc. [all 3]
- **Parked for phase 4/5:** a "show suppressed detections" indicator (A1).

### 7. Per-drum processing (depends on D1, recommendation: hybrid dual-resolution)
Fixed in both D1 options:
- **Transparency:** every setting at default, or no hit active → output = input delayed by exactly
  D. [all 3]
- **Shares, not winner-takes-all:** each active type's settings act in a band or bin in proportion
  to its share of it:
  - share = A_t·S_t / (Σ_u A_u·S_u + background), smoothed over 2 ms [Claude + A1];
  - two drums in one range (snare + clap) blend their settings by share, and they never multiply
    [Claude + A1];
  - rejected: taking the max of the two (A2 only).
- **Fades:** 2 ms pre-roll before the transient, 20 ms release, 5 ms crossfade on retrigger.
  [Claude + A1]

### 8. Internal constants (merged)
| Name | Meaning | Value | Sensitivity |
|---|---|---|---|
| D | Lookahead / latency | 30 ms | high |
| N / hop | STFT size / hop | 1024 / 128 (2048 / 256 at ≥ 88.2 kHz) | medium |
| α | Onset threshold multiplier | 2.4 − 1.2·s | high |
| δ | Threshold floor | 0.5 dB | low |
| M | Median window | 100 ms | medium |
| T_ref | Per-band refractory time | 25 ms | medium |
| T_grp | Band grouping window | ±6 ms | medium |
| ρ_min | Min template share for a candidate | 0.15 | medium |
| θ_conf | Min confidence, else Misc | 0.55 − 0.2·s | high |
| τ_sm | Softmax temperature | 0.1 | high |
| t60 closed / open | Hat decay limits | 80 / 150 ms | high |
| bursts_clap | Min clap bursts | 3 in 30 ms | medium |
| R_t | Tail end below peak | 30–40 dB per type | medium |
| floor margin | Tail end above floor | 6 dB | low |
| hangover | Tail-end confirmation | 20 ms | low |
| K_present / c̄ | Auto-detect count / mean confidence | 4 / 0.65 | low |
| adapt cap | Max move toward this loop | 50 % | medium |
| pre-roll / release / retrigger | Gate shape | 2 / 20 / 5 ms | low |

The **high** ones are candidates for an advanced page later. [A1's idea]

### 9. Testing
- **Offline harness** (A1): a command-line tool that reads a WAV and writes the processed WAV plus
  a detection JSON. It runs in CI and in Claude's sandbox.
- **Synthetic drum generator** with known ground truth (Claude): all 9 types, random patterns,
  levels, overlaps and flams, fixed seeds. This is how phase 3 is tested away from home.
- **Pass thresholds:**

| Test | Pass |
|---|---|
| Synthetic, per type | F1 ≥ 0.95 within ±10 ms |
| Real loops: kick / snare / hats | F1 ≥ 0.95 / 0.92 / 0.90 [A1; A2 ≥ 0.92] |
| Real loops: cymbals / rim / clap | F1 ≥ 0.85 / 0.80 / 0.80 [A1] |
| Misc rate on real loops | < 5 % [A1] |
| False hits | < 2 per minute [A1] |
| Transparency (defaults, or no hits) | residual ≤ −100 dBFS [Claude; A1's −60 is too loose, A2's −144 is below float precision] |
| Latency | reported = measured, exactly [A1 said ±2 samples] |
| CPU | < 5 % of one i7-11700K core at 44.1 kHz, < 10 % at 96 kHz, buffers 32–2048 [A1] |

- **Stress set:** kick + snare + hat + crash together, flams, swung hats, half-time, EDM with
  sustained sub. [A1]
- **Real labelled loops:** licensing needs checking first. At home, the ear test in FL Studio is
  level 4.

### 10. Risks
- **Closed vs open hat** is the hardest decision (all 3). Fallback: provisional label, then revise
  with a crossfade.
- **Heavily processed loops** (compressed, reverb, distortion) smear onsets and features. Fallback:
  loop adaptation, the sensitivity knob, manual mode.
- **A broadband crash hides quieter hits** in the same moment. Per-band detectors help, but it's not
  solved.
- **Non-drum content** (bass in the kick band, vocals in the snare band) gets processed. Fallback:
  require a sharp attack and keep the kick band tight. [A1]
- **Loop adaptation drifting.** Guarded by the 50% cap, high confidence only, and reset.
- **Host latency reporting.** `VERIFY(host)` in FL Studio at home. A manual offset slider (A1)
  is added only if FL turns out to need it.
- **CPU spikes** with 4+ simultaneous hits. Fallback: cap the features computed per event. [A2]
- **Decay controls are inherently approximate on a mixed loop.** The shootout scored about 60% for
  every method, because tails share bands with other drums. Fallback: a gentle range, and
  documenting it honestly.

## Decisions for you
- **D1: How the processing is done.** Measured twice in `research/engine-shootout/`, on synthetic
  loops with known stems, so the ideal result is exact.
  - **v1** (no same-range overlaps): the time-domain bands won. Both agents' review
    (`prompts/answers/03-*.md`) said the test was too easy, and **they were right**.
  - **v2** adds kick + bass, snare + clap and open hat + crash, richer drums, dual-resolution STFT
    engines, a phase-insensitive metric and timing jitter. Realistic shares, L2 error in dB (lower
    is better):

    | Test | STFT mask (A1 + A2) | 6 bands (Claude) | **Hybrid dual-res, fits 30 ms** |
    |---|---|---|---|
    | Kick −6 dB, bass under it | −5.3 | −6.0 | −5.3 |
    | Snare Crack, clap on it | −7.2 | −9.4 | **−10.0** |
    | Snare compression, clap on it | −9.9 | −9.4 | **−10.4** |
    | Kick saturation, bass under it | impossible | **−5.9** | −5.2 |
    | Clap −6 dB, on the snare | −5.1 | −4.6 | **−5.3** |
    | Kick Top +6 dB, hats in range | −10.5 | −8.4 | **−18.7** |
    | Open-hat Decay, crash under it | −2.1 | −2.0 | −2.2 |

  - **New recommendation (it replaces Claude's earlier one): the hybrid dual-resolution engine.**
    - **Estimate:** an STFT soft-mask estimate of each active drum, with N = 1024 / hop 128 below
      1.5 kHz and N = 256 / hop 64 above.
    - **Processing:** that drum's real time-domain processors (EQ, compressor, transient shaper,
      saturator, clipper) run on the estimate, and y = x + Σ (P(x̂) − x̂).
    - **Where it came from:** this is Agent 2's verdict plus both agents' dual-resolution idea. It
      beats the 6 bands clearly on transients and on the kick click versus hats, and it can do
      everything the mask can't.
    - **Latency:** about 21 ms for the STFT plus a short split filter, inside the 30 ms lookahead.
  - **Honest expectation:** where two sounds share a range at the same moment, **every** method is
    only about 70–90% right for level and dynamics, and about 40% for Decay. A kick change partly
    moves a bass under it. The ear test at home decides whether that's audible.
  - Still untested: real multitrack stems, stereo, and the two decay ideas (A1's predictive decay,
    A2's spectral floor). They become the first `dsp/` tests in phase 3.
- **D2: Lookahead.** Fixed at 30 ms, or adjustable 10–40 ms (A1)?
  *Claude's recommendation:* **fixed 30 ms for version 1.** The detection needs it, and the hybrid
  engine's ~21 ms latency fits inside it. FL compensates the delay. A low-latency mode can come in
  phase 5.
