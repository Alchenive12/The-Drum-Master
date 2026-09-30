<!-- File: prompts/answers/04-claude.md -->
# Prompt 04 — Claude's answer (written blind, before any other answer)

## 1. What to measure
Every measurement is a **ratio inside one drum's own spectrum**. That makes it independent of the
loop's level and of how loud that drum is. It's taken from that drum's estimate during its gate
(the first 150 ms of each confident hit, or the whole tail if shorter), as energy in dB.

| Knob | Drums | Measurement (dB) |
|---|---|---|
| Bass boost | kick | E(30–90 Hz) / E(30 Hz–16 kHz) |
| Mud control | kick, snare, toms (misc) | E(200–500 Hz) / E(total), energy-weighted over the three |
| Mid presence | snare, clap / kick | snare, clap: E(1.5–5 kHz) / E(total) · kick: E(2–5 kHz) / E(total) ("click") |
| High control | closed + open hat, ride, crash | E(8–16 kHz) / E(2–16 kHz) ("air vs harsh"); separately, harsh peak = max 1/3-octave in 3–7 kHz minus the 2–10 kHz mean |

Rules for the measurements:
- Only confident hits count: weight ≥ 0.7, confidence ≥ 0.7, and the hit ≥ 20 dB above the noise
  floor.
- Hits where another drum covers the same band are skipped. For example, a snare measurement is
  skipped when a crash hit lands within 50 ms.

## 2. Genre targets (v1: from experience; confidence low–medium)
Targets are dB ratios as defined above. The tolerance is the dead zone where nothing happens.

| Genre | Bass (kick) | Mud (bodies) | Presence snare/clap | Presence kick click | High (air ratio) |
|---|---|---|---|---|---|
| Rock | −6 | −9 | −5 | −14 | −5 |
| Pop | −4 | −12 | −4 | −16 | −3 |
| EDM | −2.5 | −13 | −5 | −18 | −3 |
| Metal | −8 | −13 | −3 | −8 | −4 |
| Hip-hop | −1.5 | −11 | −6 | −22 | −4 |
| **Tolerance** | ±1.5 | ±1.5 | ±1.5 | ±2 | ±1.5 |

- **Where the numbers come from:** typical mix moves per genre. Examples: metal's clicky, tight
  kick; hip-hop's sub-heavy 808; pop's airy hats; rock keeping snare body. These are starting
  points, not measurements.
- **To measure properly** (recommended before release):
  - Take ≥ 30 commercially mixed stems or loops per genre.
  - Use material we're licensed to analyse: our own renders, royalty-free loop packs whose licence
    allows analysis, or CC0/CC BY multitracks.
  - Run the plugin's own detector and measurements over them.
  - Target = the median, tolerance = the interquartile range / 2.
  - Measuring only (never shipping audio) makes licensing easy.

## 3. Correction math
- **Knob and Depth:**
  - k = knob / 100, in −1..1.
  - Aim = target + 6 dB × k (for Mud: aim = target − 6 dB × k, so + means a stronger cut).
  - error = aim − measured. If |error| < tolerance, error = 0; otherwise error is reduced by the
    tolerance (a soft dead zone).
  - gain = Depth × 0.8 × error, clamped per band.
- **Bands:**

| Band | Filter (per drum, in that drum's chain) | Max boost / cut |
|---|---|---|
| Bass | kick low shelf 70 Hz, Q 0.7 | +6 / −6 dB |
| Mud | bell Q 1.0: kick 250 Hz, snare 400 Hz, toms 300 Hz | +3 / −8 dB |
| Presence | bell Q 0.9: snare / clap 3 kHz, kick 3.5 kHz | +6 / −6 dB |
| High (air) | high shelf 8 kHz on hats / cymbals | +5 / −6 dB |
| High (harsh) | bell Q 2 at the measured harsh peak, only cuts, only when the peak > 4 dB | 0 / −6 dB |

- Because the filters sit in the drum's own chain, they act only during that drum's hits
  (drum-aware, as decided).

## 4. Time behaviour
- **Measurement:** an exponential average over about the last 12 hits of each drum
  (α = 1/12 per hit).
- **Band gain smoothing:** it moves towards a stronger correction with τ = 1.5 s and back with
  τ = 3 s. Cuts are allowed to be faster than boosts (τ 1 s), because a cut is the safer move.
- **Startup:** no correction until 4 confident hits of that drum; then it fades in over 2 s.
- **Loop change:** if 3 hits in a row differ from the average by more than 6 dB, the average
  re-learns quickly (α = 0.5 for 4 hits).
- **No pumping:** the gains move on slow averages across many hits, never inside one hit. Smoothing
  is on the dB values; coefficients are re-designed from them once per block.

## 5. Safety
- **Where it measures:** on the raw input estimate, **before** the user's own per-drum settings.
  The Puppet Show corrects towards the genre, and the user's knobs then act on top of that as
  offsets, so it never fights the user's EQ. (Measuring after the user's EQ would undo their boost.)
- **No boosting noise:** don't boost a band whose energy is less than 10 dB above that band's noise
  floor.
- **A bad measurement can't cause a big boost:** measurements must have ≥ 8 hits of history before a
  full-size boost is allowed. Boosts are also clamped to half their maximum while fewer than 12
  hits have been seen.
- **Depth 0 or Off:** exactly nothing. Switching Off fades the gains to 0 over 200 ms.

## 6. Why it can beat a generic smart EQ
- **It corrects each drum only while that drum plays.** A generic dynamic EQ on the bus reacts to
  the sum, so taming harsh hats also dulls the snare's crack.
- **Its reference is per drum type,** not an average bus curve. A dark kick can be fixed without
  touching the bright hats.
- **Its meters say "kick: +3 dB bass, hats: −2 dB air"**, which people understand, instead of a
  cloud of moving bands.
