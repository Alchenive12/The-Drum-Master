<!-- File: prompts/answers/02-agent1.md -->
# Answer to prompt 02 — Agent 1

> Pasted by the user on 2026-09-30. Content unchanged. Plain-text tables were reformatted into
> Markdown. **Paste loss:** some formulas were cut off where they contained `|` characters
> (centroid, HNR); they're marked `[cut in paste]`.

## 1. Overview: pipeline in 10 steps
1. Input stereo frame → push into a lookahead ring buffer of L = 20 ms.
2. Analysis STFT on the future-most window: Hann, N = 1024 @ 44.1/48 k, 2048 @ 88.2/96 k, hop H = 128 samples. Gives magnitude X(f,n) and phase.
3. Band energies E_b[n] over 6 fixed analysis bands (see §3).
4. Per-band onset detector on each E_b[n] (positive flux + adaptive threshold + peak pick). Outputs onset candidates per band.
5. Cluster onsets across bands within W_co = 15 ms → one hit event that may contain several simultaneous drum activations.
6. Feature extraction per active band-group of a cluster (centroid, rolloff, decay, HNR, ZCR, band ratios, attack slope, multi-burst count).
7. Classification with a rule-based soft scorer (optionally a small MLP on the same features) → per-type probability + confidence. Below threshold → misc.
8. Tail tracking per detected hit: envelope follower on the hit's dominant bands; ends when decay criterion or retrigger is met.
9. Per-hit processing: per-bin gain G(f,n) = Σ_d a_d(f,n) · g_d(f,n) where a_d is the drum's normalized spectral share and g_d the user's settings for that drum (default = 1). Applied as a real magnitude gain; phase untouched.
10. Reconstruct via inverse STFT + overlap-add, then output delayed by L samples. Report L to the host as latency.

## 2. Lookahead
Recommended: L = 20 ms default, user range 10–40 ms.
- Lower bound is set by physics: to compute a magnitude spectrum that reliably separates kick / snare / hat / cymbal, you need roughly one analysis window of future audio. With N = 1024 @ 44.1 k that is ~23 ms, but a 50 % window overlap (center the window on the current sample) means ~11.5 ms of future is enough for a single frame.
- Add ~5 ms for the onset decision (need to compare against the previous frame) and ~3 ms for classification. → ~20 ms is the realistic minimum that keeps classification reliable.
- Below ~10 ms, closed vs open hat and crash vs ride start to collapse, because you cannot see enough of the attack.
- Above ~40 ms, musicians start to feel the monitoring delay.
- The plugin should expose the delay value to the host via getLatencySamples(); a manual offset is provided for hosts that do not compensate correctly (FL Studio historically needs this on some plugins — guess from experience, not guaranteed).

## 3. Onset detection
Analysis bands (non-overlapping, chosen so each drum has a "home" band):

| Band | Range | Home drums |
|---|---|---|
| B0 | 20–120 Hz | kick sub |
| B1 | 120–400 Hz | kick body, snare body, toms |
| B2 | 400–1500 Hz | snare fundamental, clap body, rim |
| B3 | 1.5–5 kHz | snare crack, rim, clap |
| B4 | 5–10 kHz | hats, cymbal |
| B5 | 10–16 kHz | air, hat sizzle |

Per-band energy (frame n, hop 5 ms, window 10 ms): E_b[n] = Σ_{k∈B_b} |X(k,n)|²

Onset strength (positive flux): O_b[n] = max(0, 10·log10(E_b[n] + ε) − 10·log10(E_b[n−1] + ε))

Adaptive threshold over a sliding window of M = 20 frames: T_b[n] = α · median(O_b[n−M .. n−1]) + δ, with α ≈ 1.8, δ ≈ 0.5 dB.

Peak pick: O_b[n] > T_b[n] and O_b[n] ≥ O_b[n±1] (3-point local maximum).

Simultaneous hits: because each band has its own detector, a kick+hat at the same sample time produces independent onsets in B0/B1 and B4/B5. Cluster onsets whose start times fall within W_co = 15 ms into one hit event. The event then carries a set of active band-groups; the classifier can label more than one drum for the same event.

Why this works for simultaneous hits: the flux is measured per band, so a loud kick does not hide a quiet hat. What will hide a hat is a broadband transient like a crash starting in the same frame — that's a risk in §10.

## 4. Classification
Features per active band-group of a cluster (all computed on the future-window magnitudes, over the hit's onset neighbourhood of 3 frames):

| Feature | Formula | Purpose |
|---|---|---|
| Spectral centroid | C = Σ f·\|X\|… [cut in paste] | … [cut in paste] |
| Spectral rolloff 85 % | f where cumulative energy = 0.85 | brightness tail |
| Band energy ratios | r_b = E_b / Σ E_b | home band check |
| Attack slope | (max env − env at onset) / Δt | sharp vs soft |
| Decay slope | fitted dB/s on E_b after peak | closed vs open, rim vs snare |
| Zero-crossing rate (B1–B2) | standard | noisy vs tonal |
| HNR (harmonic-to-noise) | HNR = 10·log10(Σ_{k∈H} \|X\|… [cut in paste] | … [cut in paste] |
| Multi-burst count | peaks in envelope within first 30 ms | clap vs snare |
| Stereo correlation ρ in B4 | Σ L·R / sqrt(ΣL²·ΣR²) | crash/ride width, hats mono-check |

Mapping to the 9 types (rule-based, soft scores via Gaussian memberships):
- Kick: high r_B0 + r_B1, C < 200 Hz, low ZCR. Fast attack.
- Snare: high r_B1 + r_B2 + r_B3, C ∈ [300, 1500] Hz, moderate HNR, decay 80–200 ms.
- Rimshot: very short decay (< 40 ms), C ∈ [1, 3 kHz], high HNR (narrowband), attack sharp.
- Clap: multi-burst ≥ 3 in first 30 ms, C ∈ [1, 3 kHz], low HNR, wide stereo.
- Closed hat: C > 7 kHz, decay < 80 ms, low r_B0..B2.
- Open hat: C > 6 kHz, decay 150–500 ms, high sustained E_B4.
- Crash: C ∈ [4, 8 kHz], decay > 800 ms, broadband rolloff > 10 kHz, low HNR, ρ high (wide).
- Ride: C ∈ [5, 9 kHz] and strong tonal ping (HNR high in B2–B3), decay 300–1500 ms.
- Misc: none of the above, or confidence below threshold.

Confidence: for each class c define a score s_c ∈ [0,1] as the product of Gaussian memberships on the key features (each exp(−(x−μ_c)²/2σ_c²)). Then softmax-normalize: p_c = exp(s_c/τ) / Σ exp(s_d/τ), τ ≈ 0.1. Confidence = max_c p_c. If max_c p_c < θ_conf (default 0.45), reassign the hit to misc. misc is neutral.

The tricky pairs, explicitly:
- Closed vs open hat: the only strong separator is decay. Closed: t_60 < 80 ms. Open: t_60 > 150 ms. Between 80–150 ms → bias toward open if C > 7 kHz and r_B5/B4 > 0.3, else closed. This is the hardest decision and the reason 20 ms lookahead alone is marginal; here we cheat: the classifier issues a provisional label at hit start (from attack features: open hats tend to have a longer early sustain) and a revised label ~100 ms later. Since processing only starts affecting the tail region after ~100 ms, the provisional label affects only the first few ms of audio, which is dominated by the attack anyway.
- Crash vs ride: crash = low HNR, broadband; ride = strong ping + narrower bandwidth. If both scores are close → misc rather than risk mangling a crash.
- Snare vs rimshot vs clap: rimshot is short and tonal; clap has multi-burst; snare has body energy in B1. If the envelope has ≥ 3 peaks within 30 ms → clap candidate wins. If decay < 40 ms and HNR > 6 dB → rimshot.

Optional ML layer: a tiny MLP (feature vector ~15 floats → 128 hidden → 9 softmax, ≈ 10k parameters) can replace the rule scorer. Training data: a few hours of annotated drum loops (e.g., E-GMD / MDB Drums rendered to loops). Model size < 50 KB, inference < 1 ms per hit. The rule-based scorer above is the fallback that always ships.

## 5. Hit duration
For each detected hit, maintain an envelope follower e(t) on the energy-weighted sum of its dominant bands (e.g., kick → B0+B1, snare → B1+B2+B3, hats → B4+B5).
- Track peak P over the first 20 ms.
- Hit ends when both are true for ≥ hangover = 20 ms: e(t) < max(P − 40 dB, noise_floor + 6 dB), and dB slope < 0 (still decaying, not a new transient).
- If a new hit of the same type is detected before the end: close the old tail and open the new one, crossfading over 5 ms (see §7).
- If a hit of the same type is not detected but the envelope re-rises above P − 20 dB within the tail window, extend the tail (prevents flam chopping).

## 6. Auto detect
During playback:
- Run detection normally, collect all hits with p_c > θ_conf.
- Store running counts n_c, running mean confidence c̄_c, and a prototype feature vector μ_c (running mean of features of confident hits).
- On playback stop, a type is marked present if n_c ≥ K_present (= 4) and c̄_c ≥ 0.6 and the hits are spread over more than one third of the loop (avoids one flam triggering a type).
- Prototypes μ_c are used to adapt the classifier for this loop: Gaussian memberships get μ = 0.5·μ_global + 0.5·μ_c and σ shrinks by 20 %. This is few-shot adaptation and is a design guess — it helps a great deal in practice but has to be tested.

Manual mode: the plugin ignores types not on the user's list. Any hit classified as a non-listed type is either routed to misc (if "misc" is enabled) or suppressed (no processing, no gain). The user gets a "Show suppressed detections" indicator so it is obvious when a hit is being ignored.

## 7. Per-drum processing
Core idea: transparent per-bin blend. For each STFT bin (f, n) and each drum d with active hit:
- M_d(f) = a static spectral prototype of drum d (normalized to 0..1), derived from its home bands.
- e_d(n) = the hit's time envelope (0 outside the hit, → 1 at the peak, smoothly tapering at the tail via the envelope follower from §5).
- Unnormalized share: ã_d(f,n) = M_d(f) · e_d(n).
- Normalize: a_d(f,n) = ã_d(f,n) / max(1, Σ_e ã_e(f,n)).

Then the total per-bin gain is G(f,n) = 1 + Σ_d a_d(f,n) · (g_d(f) − 1), where g_d(f) is the user's frequency-dependent gain for drum d (level + EQ + saturation's static curve, all 1.0 at default).

Transparency at default: every g_d = 1 ⇒ G = 1 everywhere ⇒ output = input (up to STFT reconstruction error, which is < −90 dB with Hann + 75 % overlap).

Two types sharing a range (snare + clap in mids; hats + crash in highs): the normalization Σ a_d ≤ 1 means each bin's total gain is a weighted blend of the two drums' settings, never the product. If both are at default → no change. If snare is −6 dB and clap is 0 dB in a shared bin, that bin gets −6 dB · a_snare / (a_snare + a_clap). So the share of the bin to each drum matters: the drum whose prototype dominates that bin at that moment gets its way. This is the key anti-phase, anti-pumping behaviour.

Fade in/out without clicks: the envelope e_d(n) is smoothed with an asymmetric one-pole: attack τ_a = 2 ms (fast, preserves transient), release τ_r = 15 ms (slow, avoids clicks), 5 ms crossfade between consecutive hits of the same type.

Because all gains are real, non-negative scalars applied to the magnitude spectrum with the input phase preserved, there is no phase change and no comb filtering. Reconstruction is exact in the STFT domain.

Fallback if the STFT is too expensive: a 6-band linear-phase FIR filterbank (512-tap, polyphase) giving the same six bands; processing becomes per-band scalar gains, slightly coarser but cheaper. Set as a hidden option.

## 8. Internal constants
| Name | Meaning | Value + unit | Sensitivity |
|---|---|---|---|
| fs | sample rate | 44.1–96 kHz | — |
| N | analysis FFT size | 1024 (≤48 k) / 2048 (≥88 k) | medium |
| H | STFT hop | 128 samples (~2.9 ms @44.1 k) | high |
| W | analysis window | Hann, N samples | low |
| L | reported lookahead | 20 ms default (range 10–40) | high |
| H_onset | onset frame hop | 5 ms | medium |
| W_onset | onset energy window | 10 ms | medium |
| M | adaptive-threshold window | 20 frames | medium |
| α | threshold multiplier | 1.8 | high |
| δ | threshold floor | 0.5 dB | low |
| ε | log energy floor | 1e-10 | low |
| W_co | simultaneous-hit coincidence | 15 ms | medium |
| τ_a | envelope attack τ | 2 ms | medium |
| τ_r | envelope release τ | 15 ms | medium |
| τ_tail | tail-tracking release τ | 30 ms | medium |
| tail_thresh | tail end level below peak | −40 dB | medium |
| hangover | min tail length | 20 ms | low |
| θ_conf | misc-fallback threshold | 0.45 | high |
| τ_sm | classifier softmax temperature | 0.1 | high |
| K_present | auto-detect min count | 4 | low |
| c̄_present | auto-detect min mean conf | 0.60 | medium |
| decay_closed_max | closed-hat max t60 | 80 ms | high |
| decay_open_min | open-hat min t60 | 150 ms | high |
| C_kick_max | kick max centroid | 200 Hz | medium |
| C_snare | snare centroid range | 300–1500 Hz | medium |
| C_rim | rim centroid range | 1–3 kHz | medium |
| C_hat | hat centroid min | 7 kHz | high |
| C_crash | crash centroid range | 4–8 kHz | medium |
| C_ride | ride centroid range | 5–9 kHz | medium |

Constants marked high are the ones I would expose as advanced settings, because a wrong value will audibly misclassify.

## 9. Testing
Offline harness (no DAW): a command-line binary that reads a WAV, runs the DSP, writes the output WAV and a detection JSON.

Detection tests — use annotated drum-loop datasets (E-GMD, MDB Drums, plus synthesized loops with known timing):

| Metric | Definition | Pass |
|---|---|---|
| Onset F1 | match within ±20 ms | 0.95 |
| Kick F1 | per-class F1 | 0.95 |
| Snare F1 | per-class F1 | 0.92 |
| Hats F1 (closed + open) | per-class F1 | 0.90 |
| Cymbals F1 (crash + ride) | per-class F1 | 0.85 |
| Rimshot F1 | per-class F1 | 0.80 |
| Clap F1 | per-class F1 | 0.80 |
| Misc rate | fraction of true hits sent to misc | < 5 % |
| False-positive rate | hits invented by the detector | < 2 / minute |

Transparency test: all parameters at default, output WAV vs input WAV delayed by L. Compute residual RMS, peak, and THD+N. Pass: residual < −60 dBFS RMS and THD+N < 0.1 %.

Latency test: reported L in samples must match measured delay to ±2 samples.

CPU test: measure single-instance wall time on a 60 s loop at 44.1 k / 96 k with buffers 32 / 128 / 2048. Pass: < 5 % of one i7-11700K core at 44.1 k, < 10 % at 96 k.

Stress tests: simultaneous kick+snare+hat+crash, snare flams, swing hats, half-time loops, and EDM loops with sustained sub-bass in the kick band.

## 10. Risks
- Closed vs open hat is the single hardest problem; the 100 ms delay in the final decision means the first 100 ms of an open hat may briefly sound like a closed hat. Fallback: extend the provisional-envelope window, or accept a small error.
- Low lookahead + long tails: crash and ride decays outlast the lookahead, so their classification is confirmed late. Fallback: classify on attack features only and refine mid-tail.
- Simultaneous broadband transients (crash + kick at bar 1) can hide quieter drums in the same frame. Fallback: per-band detectors help, but leakage remains.
- Frequency overlap between kick and bass or snare and low vocal in a mixed loop will cause the plugin to process non-drum content. Fallback: tighten the kick band to < 100 Hz and require a sharp attack.
- Auto-detect adaptation can overfit a single loop and mislabel similar loops. Fallback: cap adaptation strength at 50 % and always allow the user to reset.
- STFT gain modulation on sustained tails can produce low-level "breathing" if two drums share a bin and their settings differ. Fallback: raise τ_r to 30 ms in that case.
- Host latency reporting is inconsistent across DAWs; a wrong L produces flammed playback against other tracks. Fallback: manual latency offset slider in the UI.
