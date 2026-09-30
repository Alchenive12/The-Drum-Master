<!-- File: prompts/answers/02-claude.md -->
# Answer to prompt 02 — Claude

> Written and committed **before** reading any other model's answer to prompt 02.
> **[est]** marks established practice, **[guess]** marks Claude's own assumption or value that
> needs tuning.

## 1. Overview
1. Split the input into two paths. The **analysis path** sees the input now, which is the "future"
   relative to the output. The **audio path** is delayed by the lookahead D.
2. Analysis runs on the mono sum m = (L + R) / 2 through an 8-band IIR filterbank. [est]
3. Per band, compute log energy per 1 ms frame and an onset (flux) function. [est]
4. Find onsets per band with an adaptive threshold, then group onsets from different bands that
   fall within ±3 ms into one **event**. [est]
5. Explain each event's spectrum as a **non-negative mix of per-type templates** (NNLS). This makes
   detection multi-label by construction: kick + hat gives two non-zero activations. [est: NMF/NNLS
   drum transcription]
6. For each type that's present, compute shape features (decay slope, burst count, tonality, body
   ratio) over the event window, then score and set a confidence. Low confidence goes to Misc.
7. Emit hit records: (type, onset sample, confidence, activation, band ownership weights). A
   per-type state machine then tracks the tail until the hit ends.
8. The audio path is split into 4 processing bands with **complementary linear-phase** filters. The
   bands sum exactly back to the delayed input.
9. For each band, add each active type's *processed-minus-dry* difference, scaled by
   (ownership weight × hit gate). With nothing active, the output is exactly the delayed input.
10. Sum the bands, then apply the global section (punch, clip, dry/wet, output).

## 2. Lookahead
- **D = 30 ms** (1323 samples at 44.1 kHz, 2880 at 96 kHz). This is reported to the host as
  latency. [guess, but reasoned]
- **Why 30 ms:** at the moment a hit's first sample reaches the output, the analysis has already
  seen D − 2 ms ≈ 28 ms of that hit. That's enough for:
  - **closed vs open hat:** the decay slope over about 25 ms tells them apart,
  - **clap:** its bursts come 5–15 ms apart, so two or three bursts are visible,
  - **ride vs crash:** tonality over about 20 ms.
  Kick vs snare would need only about 8 ms. [est for burst and decay timings, guess for the exact
  numbers]
- The lookahead also lets the hit gate **start ramping 2 ms before the transient**, so no attack is
  ever cut off.
- The linear-phase crossover delay (about 13 ms, section 7) fits **inside** D, so it adds no extra
  latency.

## 3. Onset detection
- **Filterbank edges (Hz):** 20–60, 60–150, 150–400, 400–1k, 1k–3k, 3k–6k, 6k–10k, 10k–20k. Each
  band is a 4th-order band-pass.
- **Frame energy** (hop H = 1 ms): E_b[n] = 10·log10( (1/H)·Σ y_b² + ε ).
- **Flux:** d_b[n] = max(0, E_b[n] − min(E_b[n−1], E_b[n−2], E_b[n−3])). This measures the rise over
  the last 1–3 ms. [est]
- **Adaptive threshold:** θ_b[n] = median(d_b over the last 250 ms) + δ, with δ = 9 dB − 6 dB·s,
  where s ∈ [0, 1] is the sensitivity knob.
- **Onset in band b** when d_b[n] > θ_b[n], d_b[n] is a local maximum (±1 frame), and more than
  T_ref = 25 ms have passed since that band's last onset.
- **Event grouping:** onsets in any bands within ±3 ms form one event at t_e = the earliest onset
  time. The onset position is refined to the sample using a 0.25 ms fast envelope. Simultaneous
  hits in different ranges (kick + hat) fire different bands in the same event, and section 4
  separates them.

## 4. Classification
**Event spectrum.** For each band, the power rise is
p_b = 10^(E_b,peak/10) − 10^(E_b,pre/10), clipped to ≥ 0. Here "peak" is the maximum within
[t_e, t_e + 10 ms] and "pre" is the mean over [t_e − 5 ms, t_e − 1 ms].

**Decomposition.** Solve p ≈ Σ_t A_t·S_t for A_t ≥ 0, where S_t is type t's template: an
8-value band spectrum that sums to 1. It's solved with about 20 multiplicative-update iterations
on a 9×8 problem, which is cheap. Types that aren't allowed (manual mode, or not yet found) are
left out of the dictionary. **Misc has a flat template**, so energy nothing else explains lands in
Misc instead of forcing a wrong type. [est: NNLS/NMF; guess: templates]

**A type is a candidate** when its share A_t / Σ_u A_u ≥ 0.15 and A_t is above −50 dBFS.

**Shape features** are computed over [t_e, t_e + 28 ms] on the candidate's own bands:
- Decay slope σ (dB/ms): the linear-regression slope of E_b from the peak to peak + 20 ms.
- Burst count β: local maxima of the 0.25 ms envelope in 1–6 kHz that are ≥ 3 ms apart with dips
  of ≥ 3 dB, counted within 0–30 ms. [est: clap structure]
- Tonality: spectral flatness F = geometric mean / arithmetic mean of a 1024-point FFT power
  spectrum over the range of interest. Low F means tonal peaks, high F means noise. [est]
- Body ratio R_body = E(150–400 Hz) − E(1–3 kHz), in dB.
- Level L = the peak level relative to the running median of this type's previous peaks. This is
  used for volume and outlier rejection.

**Separating the confusable types** (score s ∈ [0, 1] via a logistic function around the
thresholds) [guess, to tune]:

| Pair | Rule |
|---|---|
| Closed vs open hat | σ ≤ −0.6 dB/ms → closed. σ ≥ −0.25 dB/ms → open. In between, the confidence drops. |
| Crash vs ride | Ride: F(3–10k) < 0.35 (tonal ping/bell), moderate level. Crash: F > 0.5, high level, energy extending down to 400 Hz, σ > −0.1 dB/ms. |
| Hat vs cymbal | Cymbals carry significant energy below 3 kHz and decay slowly. Hats sit at 6k and above. |
| Clap | β ≥ 2 **and** R_body < −6 dB |
| Rimshot | R_body < −6 dB, β ≤ 1, tonal 1–3 kHz peak (F < 0.3), σ ≤ −0.8 dB/ms |
| Snare | R_body ≥ −6 dB (body present) plus noise at 6 kHz and above (wires), moderate σ |

**Confidence** = (template share) × (rule score). If it's below c_min = 0.55 − 0.25·s, the hit
becomes **Misc**, which is neutral by default.

**Late refinement.** If a hat's slope is still ambiguous at 28 ms, the hit starts as "closed", the
more common case. It's re-scored at 40 ms, and if the label flips, the settings crossfade over
5 ms. [guess]

## 5. Hit duration
- The tail envelope env_t is the energy in type t's owned bands, weighted by its ownership.
- **The hit ends when the first of these happens:**
  - env_t falls below max(peak − R_t, floor + 3 dB);
  - a new onset of the **same type** arrives (the old hit releases over 5 ms while the new one
    starts);
  - the duration cap is reached.
- **The noise floor** is the running 10th percentile of E_b over 2 s.
- **R_t (dB):** kick 30, snare 30, rim 25, clap 30, closed hat 25, open hat 35, ride 35, crash 40,
  misc 25. [guess]
- **Duration caps:** crash 4 s, open hat and ride 2 s, all others 1 s.
- **Gate shape:** raised-cosine attack starting 2 ms before the onset and reaching 1 at the
  onset. Raised-cosine release over 20 ms after the end.

## 6. Auto detect and manual mode
- **Auto detect:**
  - While it runs, all 9 types are in the dictionary.
  - A type becomes **present** after N = 3 hits with confidence ≥ 0.7. Its string appears at that
    moment.
  - One false positive therefore never adds a string.
- **Adapting to the loop:**
  - After each confident, single-dominant hit (confidence ≥ 0.8 and share ≥ 0.7), the template is
    updated: S_t ← normalise((1 − η)·S_t + η·p̂), with η = 0.1.
  - The learned templates are saved with the session.
  - A "reset learning" action restores the generic templates. [est idea, guess values]
- **Stopping:** detection stops when the host reports that playback stopped. [VERIFY(host): how FL
  Studio reports transport to effects]
- **Manual mode:** only the selected types (plus Misc) are in the dictionary, so an unselected type
  can never be detected. Its energy is shared among the selected types or goes to Misc.

## 7. Per-drum processing
- **Bands:** 4 processing bands with crossovers at 150 Hz, 1.5 kHz and 6 kHz:
  - Low (kick);
  - Low-mid (snare, clap and rimshot body, cymbal body);
  - High-mid (snare crack, rim click, clap, kick "Top", hat tick);
  - High (hats, cymbals, snare wires, air).
- **Transparent split:**
  - Use linear-phase FIR low-passes of equal length and delay:
    B_low = h_150 * x, B_lm = h_1500 * x − B_low, B_hm = h_6000 * x − h_1500 * x, and
    B_high = x_delayed − h_6000 * x.
  - The sum is **exactly** the delayed input (complementary by construction).
  - The FIR length is about 1280 taps at 48 kHz, set by the 150 Hz crossover. It runs as
    partitioned FFT convolution, and its delay fits inside D. [est]
- **Ownership:** a type's settings act in band b in proportion to its share of that band's energy:
  - w_t,b = A_t·S_t,b / (Σ_u A_u·S_u,b + B_b), where B_b is the background energy (floor) in that
    band. This way a snare doesn't "own" the highs while a loud hat plays there, and a quiet hit
    doesn't own a band full of background.
  - The weights are smoothed with a 2 ms one-pole.
- **Applying settings (difference structure):**
  - y_b = B_b + Σ_t a_t,b(n) · ( P_t,b(B_b) − B_b ), where a_t,b(n) = w_t,b · g_t(n) and g_t is the
    hit gate.
  - With every setting at its default, P = identity, so y = x exactly. With no hit, a = 0, so
    y = x.
  - Every processor P runs continuously, so it's never cold-started at a hit.
- **EQ inside P** uses the form H = 1 + (G − 1)·BP. Blending with weight a then gives exactly an EQ
  of gain 1 + a·(G − 1): no comb filtering from partial blends. [est]
- **Dynamics** (compression, attack/decay shaping) act as gain curves on the band signal and are
  driven by the band's own envelope. Their gain change is rate-limited to ≤ 1 dB/ms outside the
  transient, to avoid pumping.
- **Saturation and clipping** use 2× oversampling inside P, blended the same way.
- **Width** is mid/side per band: S × (1 + a·(width − 1)).
- **Two types sharing a band** (snare + clap, hat + crash) each act according to their own w. The
  sum of the w values is ≤ 1 thanks to the background term B_b.

## 8. Internal constants
| Name | Meaning | Value + unit | Sensitivity |
|---|---|---|---|
| D | Lookahead / reported latency | 30 ms | high |
| H | Analysis hop | 1 ms | medium |
| δ | Onset threshold above median | 9 − 6·s dB | high |
| T_med | Median window for threshold | 250 ms | low |
| T_ref | Per-band refractory time | 25 ms | medium |
| T_grp | Cross-band grouping window | ±3 ms | medium |
| ρ | Min template share for a candidate | 0.15 | medium |
| c_min | Min confidence, else Misc | 0.55 − 0.25·s | high |
| σ_closed / σ_open | Hat decay thresholds | −0.6 / −0.25 dB/ms | high |
| F_ride / F_crash | Flatness thresholds | 0.35 / 0.5 | medium |
| β_clap | Min bursts for clap | 2 | medium |
| R_body | Body-ratio threshold | −6 dB | medium |
| N_present | Hits before a type is "present" | 3 | low |
| η | Template learning rate | 0.1 | medium |
| R_t | Tail-end drop per type | 25–40 dB | medium |
| T_pre / T_rel | Gate pre-roll / release | 2 ms / 20 ms | low |
| T_w | Ownership weight smoothing | 2 ms | low |
| Crossovers | Processing bands | 150 / 1500 / 6000 Hz | medium |

## 9. Testing (offline, in `dsp/` tests)
- **Synthetic drum generator with known ground truth:**
  - kick: a sine sweeping 150→50 Hz with an exponential decay;
  - snare: a 200 Hz tone plus band-passed noise;
  - closed hat: high-passed noise with a 30 ms decay; open hat: the same with a 400 ms decay;
  - crash: broadband noise lasting 2 s; ride: inharmonic partials plus noise;
  - rim: a 2 kHz tonal burst plus a click; clap: 3 noise bursts 8 ms apart plus a tail.
  Random patterns, levels and overlaps, fixed seeds.
- **Metrics:**
  - onset F1 within ±10 ms, per type;
  - confusion matrix;
  - Misc rate;
  - null test;
  - CPU time.
- **Pass thresholds:**
  - Synthetic: F1 ≥ 0.95 per type.
  - Transparency: every setting at default → output − delayed input ≤ −100 dBFS.
  - Any settings with no hits → ≤ −100 dBFS.
  - CPU: 60 s of 48 kHz stereo processed in < 1.5 s on one CI core.
  - Real labelled loops: F1 ≥ 0.85 for kick, snare and hats. Public drum-transcription datasets
    exist (for example ENST-Drums, IDMT-SMT-Drums, MDB Drums), but their licences must be checked,
    and most only label kick, snare and hi-hat. [est]
- **At home (level 4):** the final ear test in FL Studio.

## 10. Risks
- **Heavily compressed, reverb-heavy or sidechained loops** smear the onsets. Fallback: the
  sensitivity knob, Misc, and manual mode to cut down on confusion.
- **Half-open hats** sit between closed and open. Fallback: the late re-score with a crossfade;
  the worst case is the closed-hat settings at reduced weight.
- **Crash-rides and layered samples** (clap + snare) are detected as two types. The ownership
  weights split the band between them, which is acceptable.
- **Kick "Top"** acts in the high-mid band, where hats also live. Ownership weights limit the
  spill, but it isn't zero.
- **30 ms latency** is fine for mixing and not for live monitoring. A 10 ms low-latency mode with
  lower accuracy could be a phase-5 feature.
- **Template learning can drift** after repeated misdetections. Guards: high confidence only, a
  low η, and a reset button.
- **All thresholds are [guess]** until they're tuned on real loops. The synthetic tests prove the
  pipeline works, not the tuning.
