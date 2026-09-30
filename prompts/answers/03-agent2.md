<!-- File: prompts/answers/03-agent2.md -->
# Answer to prompt 03 — Agent 2 (review of Claude's engine)

> Pasted by the user on 2026-09-30. Content unchanged; headings formatted.

## 1. Verdict
I recommend Hybrid (C), because high-resolution STFT masking is mandatory for isolating overlapping drums in a complex mix, while time-domain rendering is mandatory for applying true non-linear saturation and artifact-free transient shaping.

## 2. Flaws in the TEST
- Rigged Signals: Synthetic drums (e.g., snare as "190 Hz tone + noise") lack the complex, modulating, inharmonic partials of real drums. This artificially benefits the 6-band (B) engine, which relies on simple, static average spectra (a_{t,b}).
- Rigged against STFT (A): STFT magnitude gain modulation (g_t(n)) simply cannot perform time-domain transient shaping or fast compression without pre-echo and smearing (established practice). Asking it to perform time-domain tasks guarantees its failure. Furthermore, testing "saturation" on STFT magnitudes is mathematically impossible in a linear transform—it requires time-domain waveshaping.
- Blind Metric: The energy error metric (energy(output − ideal)) is a basic L2 norm. It severely penalizes slight phase shifts that humans cannot hear, while under-reporting intermodulation distortion and pre-echo, which humans detect instantly.

## 3. Flaws in the PROPOSAL (B - 6 bands)
- Severe Cross-Talk: 6 bands is far too broad. The 1.5–5 kHz band contains the kick "click", snare "crack", and hi-hats. If you compress or transient-shape the snare in this band, you will audibly pump/duck the kick and hats playing simultaneously (established practice).
- Sample Rate Scaling: A 21 ms FIR crossover at 44.1 kHz is ~926 samples. To maintain the low 120 Hz crossover at 96 kHz, the FIR must double in length to ~1852 samples. This eats up roughly 20 ms of your 30 ms lookahead, leaving almost zero time for onset detection, RF classification, and envelope smoothing.
- Band-Limited Saturation: Applying non-linear saturation to a restricted band (e.g., 400–1500 Hz) sounds thin, fizzy, and "synthy" compared to full-bandwidth saturation, because the crucial intermodulation distortion between the drum's sub-frequencies and high-frequencies is lost.
- The 808 Problem: A kick and an 808 glide both live entirely in the 0–120 Hz band. The 6-band engine has zero ability to separate them if they overlap.

## 4. Could an improved STFT approach beat B?
Yes. An optimized STFT approach could beat B on transients while crushing it on isolation.
- Dual-Resolution STFT (Multi-Resolution STFT): (Established practice). Use short windows (e.g., 128 samples / 2.9 ms) for frequencies above ~1.5 kHz to tightly track transients without pre-echo, and long windows (e.g., 2048 samples) for low frequencies to maintain isolation for the kick and snare body.
- Phase-Aware Masking: Instead of only modulating magnitudes, update the STFT phase using a phase-derivative (Phase Vocoder) or complex-domain smoothing to prevent time-smearing when altering the envelope.
- Cost: This requires running two or more parallel FFTs at different window sizes, increasing CPU load by ~2-3x, though it remains well within the capacity of a modern i7 core.

## 5. The decay-shortening problem
The problem: All engines scored poorly (~60%) because an open hat's tail shares frequencies with continuous room tone and snare resonance. If you turn down those frequencies, you audibly "duck" the room tone, causing the mix to suck in and out (established practice).
- Solution (My guess/extrapolation): Sparse Spectral Floor. Instead of applying a flat attenuation to the STFT bins or FIR bands, implement a "spectral floor." Calculate a rolling minimum magnitude across the high frequencies (representing the continuous room noise). When shortening the hat decay, attenuate the peaks down to this noise floor, but no further. This removes the hat's tonal energy while preserving the illusion of continuous background audio.

## 6. One extra test
The "Bonham" Acoustic Bleed Test: Process a heavily compressed, acoustic drum loop recorded with heavy room mics (massive phase correlation and bleed between hits), using a perceptual metric like ViSQOL or PEAQ instead of a raw energy delta. This will instantly reveal the pumping artifacts of the 6-band engine and the phase-smearing of the STFT engine.
