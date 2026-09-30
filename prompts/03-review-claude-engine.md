<!-- File: prompts/03-review-claude-engine.md -->
# Prompt 03: agents review Claude's processing engine

**How to use:** copy everything inside the box and paste it into the **same** agents that answered
prompt 02, ideally in the same chat so they remember their own answer. Paste each reply back to
Claude → `prompts/answers/03-<agent>.md`.

```text
Earlier you designed the detection + processing algorithm for my drum plugin "The Drum Master"
(an insert effect on an already-mixed drum loop that detects each hit — kick, snare, closed hat,
open hat, crash, ride, rimshot, clap, misc — with 30 ms lookahead and applies that drum's own mix
settings only to its frequency range and only for the hit's duration; transparent at default).

Three designs were proposed for HOW to apply the per-drum settings. You and another agent proposed
an STFT spectral gain mask. A third designer (Claude) proposed a time-domain band engine and then
tested both. I want your honest, critical review. Do not simply agree — look for flaws.

CLAUDE'S PROPOSAL (detection stays STFT-based, as you proposed)
- Audio path split into 6 bands with complementary linear-phase FIR filters (crossovers 120, 400,
  1500, 5000, 10000 Hz; equal-length low-passes, bands formed by differences, so the bands sum
  exactly to the delayed input). FIR delay (~21 ms) fits inside the 30 ms lookahead.
- For each active drum t, a share a_t,b(n) in each band b (its fraction of that band's energy,
  from detection templates x activation, smoothed ~2 ms).
- Estimate of the drum: x̂_t(n) = Σ_b a_t,b(n) · x_b(n).
- Apply the drum's REAL time-domain processors P_t (EQ, compressor, transient shaper, saturator,
  clipper) to that estimate, and add only the difference back: y = x + Σ_t ( P_t(x̂_t) − x̂_t ).
  At default P_t = identity → y = x exactly; with no hit a_t = 0 → y = x exactly.

THE TEST CLAUDE RAN (offline C++)
- Synthetic 4-bar loop at 48 kHz from separate stems (kick = pitch sweep + click; snare = 190 Hz
  tone + noise; closed hats every 8th; one open hat per bar; kick/snare land on top of hats).
- Because stems exist, the ideal result is exact: ideal = mix − target_stem + P(target_stem).
- All engines get perfect onset timing. Shares computed "realistically" as each stem's AVERAGE
  spectrum × its envelope (also tested with perfect per-moment shares — same ranking).
- Engines: A = STFT gain mask (N=1024, hop 128 and hop 256, Hann-sqrt, per-bin gain
  1 + Σ a_t(k,n)(g_t(n) − 1), g sampled at frame centre); B = the 6-band engine above;
  C = hybrid (STFT soft-mask estimate of the drum, then the same time-domain P_t as B).
- Score = energy(output − ideal) / energy(ideal − input) in dB (lower = better; −20 dB = 99 %
  right). "Pre" = the same error counted only 1–25 ms BEFORE each hit (smearing).

RESULTS (realistic shares)
| Test                          | A mask hop128        | A mask hop256        | B 6 bands           | C hybrid            |
| Neutral settings (null)       | −272 dBFS            | —                    | −317 dBFS           | = A                 |
| Kick level −6 dB              | −27.1                | −27.7                | −29.0               | −28.2               |
| Snare attack +6 dB (0–8 ms)   | −10.9 (pre −34)      | −10.7 (pre −34)      | −19.9 (no pre)      | −18.6 (no pre)      |
| Open-hat decay shorter        | −4.0                 | −4.0                 | −3.9                | −3.9                |
| Snare compression 4:1         | −19.0 (pre −40)      | −18.7 (pre −40)      | −24.8 (no pre)      | −23.9 (no pre)      |
| Kick saturation (tanh)        | impossible (0.0)     | impossible (0.0)     | −39.8               | −39.9               |

Claude's conclusion: time-domain processing wins for transients, dynamics and saturation; static
level is a tie; decay-shortening is weak for everyone (tails share bands with other drums); and
the STFT's finer frequency ownership (hybrid C) adds nothing over 6 bands here. Recommendation:
6 time-domain bands (B), hybrid C as fallback.

WHAT I NEED FROM YOU (use exactly these numbered sections, keep it short)
1. Verdict: do you now recommend A, B, C, or something else? One sentence why.
2. Flaws in the TEST: anything that unfairly favours or penalises one engine (settings, metric,
   signals, how A was implemented). Be specific; say how you would fix the test.
3. Flaws in the PROPOSAL (B): where will 6 bands fail on real loops that the synthetic test hides?
   (e.g. kick "Top" click and hats sharing 1.5–5 kHz, bass under the kick, 808 glides, CPU, the
   21 ms FIR delay at 96 kHz, stereo).
4. Could an improved STFT approach (e.g. smaller window for transients, dual-resolution, phase-
   aware processing) beat B on these tests? What exactly would you change, and what would it cost?
5. The decay-shortening problem (all engines ~60 % right): any idea that would actually improve it
   on a mixed loop?
6. One extra test you would add before we commit to a design.

RULES
- Formulas welcome, no code.
- Mark what is established practice vs your guess.
- Disagree where you disagree — a useful "no" is worth more than a polite "yes".
```
