<!-- File: prompts/answers/03-agent1.md -->
# Answer to prompt 03 — Agent 1 (review of Claude's engine)

> Pasted by the user on 2026-09-30. Content unchanged; headings formatted.

## 1. Verdict
B as the shipping base, C as the reference-quality path, A only for static level/EQ — because time-domain nonlinearities (compression, saturation, clipping) genuinely cannot be expressed as a magnitude-only STFT mask, which is the one real finding in Claude's test. But B alone has a fatal problem on real loops (below), so I would not commit to B without fixing it.

## 2. Flaws in the test
- "Saturation impossible for A" is a test artifact, not a mathematical fact. You can OLA to time domain, apply tanh, and re-analyze. That makes A ≈ C. The test should have included A′ = A + time-domain nonlinearity, and probably a B′ = B with STFT soft-mask estimate (that is C — it did not test what it claims).
- Signals too clean. Kick = pitch sweep + click and snare = 190 Hz tone + noise hide modal complexity. Real kicks have a moving centroid over 30 ms; real snares have 200/400 Hz modes and 3–5 kHz ring. Fix: rerun on real stems (Cambridge MT / E-GMD with ground-truth multitracks).
- Shares from average spectrum ignore time-varying spectrum. A kick's centroid moves hundreds of Hz during its first 30 ms, where the transient shaper lives. Both engines get bad shares, but B's band aggregation hides it more — that is not the same as B being correct. Fix: per-frame share from a short-term spectral envelope.
- Metric penalises small time misalignment of small changes. A −6 dB kick level is a subtle ideal; a 2 ms gain error causes disproportionate error. Fix: integrate error over 20 ms around the hit, or use log-spectral distance.
- No overlapping-in-same-band test. Kick + bass, snare + clap, hat + crash are exactly where B's estimate x̂_t = Σ a_t,b · x_b contains other drums. This is the test that decides the design, and it is missing.
- No stereo test, no onset jitter, no realistic loop variation. Detection and processing errors interact; perfect timing hides that.
- Pre-error metric is asymmetric. B's FIR delay lives inside the lookahead, so B has "no pre" by construction. A's gain smoothing must also be placed causally inside the lookahead for a fair comparison.

## 3. Flaws in proposal B on real loops
- Kick "Top" (1.5–5 kHz) shares B3 with hats and snare crack. The kick's estimate contains hat energy, so the kick's transient shaper and saturator react to hats. On a busy loop, "Top" is really "hat + kick Top".
- Bass / sub under the kick sits in B0–B1. Kick compression and "Bottom" processing act on the bass. Standard in rock, funk and any 808 loop.
- 808 glides move pitch across B0→B1 during the note; a static a_t,B0 is wrong for the whole sustain.
- Snare + clap in B1–B3. Their shares compete. Under 4:1 compression, whichever is louder dominates the other's gain. Claude's test had no clap.
- Hat + crash both in B4/B5. This is why every engine fails the decay-shortening test — the two drums are indistinguishable in one 5 kHz band.
- 21 ms FIR at 96 kHz + a 120 Hz crossover needs ~1000 taps for linear-phase at that frequency. Either raise the lowest crossover, accept minimum-phase, or accept phase error at low frequencies. This is guess on the exact tap count but the direction is right.
- Stereo. Independent L/R compression and saturation shifts the image; detectors and gains must be stereo-linked (established practice, not stated in the proposal).
- Complementary FIR sum is exact only if the band filters match sample-for-sample. Difference structures accumulate rounding error and stage delay mismatches.

## 4. Could an improved STFT beat B?
Yes, likely on real loops. Specific changes:
- A′ (post-OLA time-domain nonlinearity) = C. The proposal already reports C ties B. The real difference between A and B is not frequency ownership; it is that A's pure mask cannot do nonlinearities. Fix that and A immediately recovers most of the gap.
- Dual resolution. N=256, hop 64 for the first 15 ms (transient shaping, saturation); N=2048, hop 512 for tails (EQ, decay shortening). Combine via a frequency-weighted mask. Cost ≈ 2× A, roughly comparable to B, and far better ownership in the 1.5–5 kHz region where kick click, snare crack and hats collide.
- Per-frame share, not static template. Recompute a_t(k,n) from the short-time spectral envelope. This is a bigger win than extra resolution. My guess is that this alone closes most of B's advantage on real loops.
- Phase. Leave phase untouched for noisy drums (established: gain masking of noise-like content is benign). Only pitched kick and ride ping would benefit from phase-locked processing; cost is real and benefit small — optional.

## 5. The decay-shortening problem
The reason everyone fails at ~60 % is that nobody is separating the drum's tail from other drums' tails — they are shortening a shared band. Ideas that could actually move the number:
- Per-hit spectral template × decay model. At the hit, capture S_t(f) from STFT. Predicted residual at time n: S_t(f) · d_t(n) · e_measured(n), where d_t is the type's empirical normalized decay. Apply g(f,n) = min(1, d_target(n) / d_measured(n)). Uses per-type knowledge; needs measured decay curves per type per genre.
- Narrow sub-bands (≈500 Hz) in 5–12 kHz. Open hat and crash have different spectral envelopes; 5 kHz-wide bands cannot exploit that. Cheap addition to B.
- Predictive masking instead of reactive detection. Commit to the drum's expected decay at onset and apply the difference envelope; do not try to identify the tail in the mix. This is my guess, but it is the only approach that does not require the impossible (perfect band separation).
- Do not chase 100 %. Below ~5 % of tail energy the change is inaudible.

## 6. One extra test
Real multitrack loop with four or more simultaneous types and known stems, including kick + bass overlap, snare + clap, hat + crash + ride, on 20+ different loops across genres. Score energy(output − ideal) / energy(ideal − input) with ideal computed from real stems. Pass: B (and C) within 3 dB of ideal on static/dynamics and within 6 dB on decay shortening; plus a blind A/B listening test with three mixing engineers, because a numerical win that no one hears is not a shippable win.
