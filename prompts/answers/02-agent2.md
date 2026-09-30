<!-- File: prompts/answers/02-agent2.md -->
# Answer to prompt 02 — Agent 2

> Pasted by the user on 2026-09-30, in the same message as Agent 1's answer. Content unchanged.
> **Paste loss:** the display formulas in §3, §5 and §7 came through empty; they're marked
> `[formula lost in paste]`.

Here is the algorithm and DSP design for "The Drum Master," tailored for real-time performance, zero-latency phase coherency (when flat), and high transcription accuracy.

## 1. Overview
The pipeline processes the audio in the frequency domain to allow surgical frequency targeting without crossing-filter phase issues.
- Input Buffering: Stereo audio enters a ring buffer for lookahead.
- STFT Analysis: Audio is windowed (e.g., Hann) and converted to the frequency domain via Short-Time Fourier Transform (STFT).
- Multi-band Onset Detection: Spectral flux is calculated across low, mid, and high frequency bands to detect simultaneous hits.
- Feature Extraction: When an onset triggers, spectral and temporal features are calculated over the lookahead window.
- Classification: Features feed a lightweight Random Forest (RF) classifier to output probabilities for the 9 drum types.
- Mask Generation: For each active drum, an ideal target EQ/Dynamic curve (a spectral mask) is generated based on user settings, constrained to that drum's predefined frequency range.
- Mask Aggregation: Masks from overlapping drums are mathematically combined per-frequency-bin.
- Spectral Processing: The combined mask multiplies the delayed STFT magnitudes.
- Inverse STFT: The processed frames are reconstructed into the time domain using Overlap-Add (OLA).
- Output: Audio is sent to the DAW, perfectly aligned with the reported latency.

## 2. Lookahead
Duration: 30 milliseconds (e.g., 1411 samples at 44.1 kHz).

Why:
- Established practice: Human hearing integrates transients over ~10-20ms. To accurately classify a drum (especially closed vs. open hat), we must observe not just the attack, but the initial decay trajectory.
- Action: The plugin uses this 30ms window to run the STFT, detect the onset, extract 20ms of temporal features, run the classifier, and fade in the DSP settings. The DAW delays other tracks by 30ms so everything plays in sync.

## 3. Onset detection
To catch simultaneous hits (e.g., kick + hi-hat), we use Multi-Band Spectral Flux (established practice).
- Divide STFT bins into B bands (Low: 20-250Hz, Mid: 250-3000Hz, High: 3000-20000Hz).
- Calculate the positive flux (energy increase) per band b at frame n: [formula lost in paste]
- Compute an adaptive threshold using a moving median over the past M frames: [formula lost in paste]
- An onset is detected in band b if Flux_b(n) > T_b(n) AND Flux_b(n) > Flux_b(n-1) AND Flux_b(n) > Flux_b(n+1) (peak picking).

Simultaneous hits simply register as concurrent onsets in different bands.

## 4. Classification
For low CPU footprint, we use a Random Forest (RF) classifier (tree depth ~10, ~50 trees). It is tiny (< 500 KB), fast, and requires no heavy tensor libraries.

Features computed over the lookahead window:
- Spectral Centroid: Center of mass of the spectrum.
- Spectral Spread: Variance around the centroid.
- Spectral Crest: Max bin magnitude divided by average magnitude (tonality measure).
- Band Energies: Energy in 12 Mel-spaced frequency bands.
- Decay Rate: The slope of energy E(n) over the 20ms window: Slope = dE/dn.

Mapping logic to the 9 types (heuristic/established mix):
- Closed vs Open Hat: Both have high Centroid. Closed hat has a steep, negative high-frequency Decay Rate; Open hat has a flat/slow Decay Rate.
- Crash vs Ride: Ride has a high Crest factor in the upper-mids (the "ping" or bell is highly tonal) and lower Spread. Crash has a very high Spread (broadband white noise) and lower Crest.
- Snare vs Rimshot vs Clap: Snare shows broadband energy (mid Centroid, high Spread). Rimshot shows a massive Crest factor in the 500-800Hz range (the shell resonance). Clap exhibits 3-4 distinct micro-transients within a 15ms window (measured by counting local peaks in the time-domain envelope).
- Confidence: The RF outputs a probability vector P_class. If max(P_class) < τ_conf, the hit is labeled Misc.

## 5. Hit duration
We track the energy envelope of the specific frequency band defining that drum (e.g., low-mids for snare). [formula lost in paste]

The hit ends when E_drum(n) falls below −36 dB relative to its peak at onset, OR falls below an absolute noise floor threshold, OR if a new onset of the exact same class is detected (which instantly cuts off the old tail and starts a new one).

## 6. Auto detect
- Discovery: When Auto Detect is engaged, the RF runs on all detected onsets. We maintain a running histogram of detected classes. If a class accumulates > N hits with confidence > 85%, it is flagged as "present" and added to the UI.
- Adaptation (My proposal): The plugin stores the median feature vector for each detected class in this specific loop. It subtly shifts the RF decision boundaries toward these local centroids, drastically reducing false positives for the rest of the session.
- Manual Mode: The user selects allowed types. The RF output probabilities for unselected types are forced to 0.0, and the remaining probabilities are re-normalized.

## 7. Per-drum processing
To apply EQ/compression only to a specific frequency range without phase crossover artifacts, we use Spectral Masking (established practice for audio restoration, novel for drum mixing).
- Every processing control (EQ, Saturation drive, Compression makeup) is translated into a target gain per frequency bin: G_target(k).
- A drum's settings are restricted by a predefined spectral envelope W_drum(k) (e.g., Kick is 1.0 at 60Hz, fading to 0.0 at 1kHz).
- Transparency: If controls are default, G_target(k) = 1.0. The inverse STFT (Overlap-Add) guarantees perfect mathematical reconstruction (null test transparency).
- Overlapping Frequencies: If a Snare and Clap hit simultaneously, we calculate their masks independently. For bins where they overlap, we take the maximum absolute modification: G_total(k) = max_dB(G_snare(k), G_clap(k)).
- Fading (Anti-Click): To prevent clicks when a hit starts/stops, the processing mask is smoothed over time using a one-pole lowpass filter before multiplying the audio STFT: [formula lost in paste]

## 8. Internal constants
| Name | Meaning | Value + unit | Sensitivity |
|---|---|---|---|
| STFT Size | FFT window length | 1024 samples | High (trades time vs freq resolution) |
| Hop Size | STFT step forward | 256 samples | Medium (CPU vs envelope smoothness) |
| Lookahead | Time before output | ~30 ms | High (too low = bad classification) |
| M | Flux median window | 100 ms | Medium (adapts to groove density) |
| λ | Flux threshold multiplier | 1.5 | High (controls false positive onsets) |
| τ_conf | RF probability threshold | 0.65 (65%) | High (determines how much goes to "Misc") |
| γ | Duration envelope decay | 0.95 | Low (tail tracking smoothness) |
| β | Mask smoothing filter | 0.8 | High (too low = clicks; too high = smeared attacks) |

## 9. Testing
- Detection accuracy: Use standard MIR datasets (ENST-Drums, IDMT-SMT-Drums). Run offline.
  - Metric: F1-score (harmonic mean of precision and recall) per drum class.
  - Threshold: > 92% F1-score for Kick/Snare/Hats; > 85% for auxiliary percussion.
- Transparency: Process a dense drum loop with all controls at default. Invert the phase of the output and sum it with the dry input.
  - Metric/Threshold: Peak amplitude of the sum must be ≤ −144 dBFS (perfect digital null).
- CPU: Run 10 instances in a headless VST host. CPU usage must remain < 5% of one core on target hardware.

## 10. Risks
- Pre-echo (Transient Smearing): Modifying magnitudes in the STFT domain without phase adjustments can soften sharp transients (kick clicks). Fallback: Use transient-preserving STFT techniques (phase-locking) or run a parallel time-domain transient shaper just for the attack phase.
- Ghost Note Masking: Very quiet ghost notes might fall below the adaptive spectral flux threshold T_b(n). Fallback: Allow user to tweak an overarching "Detection Sensitivity" knob (scales λ).
- High CPU on Dense Overlaps: Calculating complex ML features for 4 simultaneous hits could cause a CPU spike, leading to DAW dropouts. Fallback: Cap simultaneous feature extractions; prioritize low/mid frequency onsets.
- Classifier Drift on heavily processed loops: If the input audio already has heavy distortion/reverb, the RF (trained on dry hits) will fail. Fallback: The "Auto Detect" adaptation phase must be heavily relied upon, or require the user to use Manual mode.
- Tail Cutoffs: If a fast drum fill occurs, a new onset cuts the previous tail, potentially causing an unnatural gating effect on the EQ mask. Fallback: Use a crossfaded release rather than a hard cutoff for the mask smoothing filter.
