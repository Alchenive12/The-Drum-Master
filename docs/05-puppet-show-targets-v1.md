<!-- File: docs/05-puppet-show-targets-v1.md -->
# The Puppet Show — v1 genre targets

> Status: **v1 values, for review**. The method was decided by the user on 2026-09-30: the average
> of the agents' answers. The kick-row anchoring is Claude's decision (D-C15). v2 replaces all of
> this with values measured from the user's genre-named sample packs.
> Sources:
> - `prompts/05-puppet-show-genre-targets.md`
> - answers: `prompts/answers/05-claude.md` (blind), `05-agent1.md`, `05-agent2.md`
> - averaging: `research/puppet-show/average_targets.py`
> - real-kit check: `research/puppet-show/measure_kits.py`

## Measurements (inside one drum's own sound, dB)
| ID | What | Definition |
|---|---|---|
| M1 | Kick sub | kick energy 30–90 Hz / kick energy 90 Hz–16 kHz (changed, see below) |
| M2 | Mud | 200–500 Hz / total, kick + snare + toms |
| M3 | Snare presence | snare / clap 1.5–5 kHz / total |
| M4 | Kick click | kick 2–5 kHz / kick 30 Hz–16 kHz |
| M5 | Cymbal air | hats + cymbals 8–16 kHz / 2–16 kHz |

## Targets (dB)
| Genre | M1 kick sub | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---|---|---|---|---|---|
| Rock | +5.5 | −9.3 | −5.3 | −31.5 | −5.2 |
| Pop | +6.7 | −11.5 | −4.5 | −31.2 | −3.3 |
| EDM | +11.1 | −12.7 | −4.3 | −31.7 | −3.0 |
| Metal | +3.6 | −12.5 | −4.3 | −26.8 | −4.3 |
| Hip-hop | +12.6 | −9.8 | −6.5 | −35.8 | −5.5 |

- **M2, M3, M5:** the plain average of the three answers (Claude blind, agent 1, agent 2).
- **M1, M4:** the answers' differences between genres are kept, and the row is anchored to real
  kits (below).

## Tolerance: no correction inside ±this (dB, average of the answers)
| Genre | M1 | M2 | M3 | M4 | M5 |
|---|---|---|---|---|---|
| Rock | 2.2 | 2.3 | 2.0 | 2.3 | 2.0 |
| Pop | 1.8 | 1.8 | 1.7 | 2.0 | 1.7 |
| EDM | 1.8 | 1.7 | 1.5 | 2.0 | 1.5 |
| Metal | 1.8 | 2.0 | 1.8 | 1.8 | 2.0 |
| Hip-hop | 2.7 | 2.3 | 2.3 | 2.8 | 2.3 |

## Knobs (dB, average of the answers)
| Knob | Measures | Target move at ±100% | Max boost | Max cut |
|---|---|---|---|---|
| Bass boost (kick) | M1 | 5.0 | 5.3 | 6.7 |
| Mud control (bodies) | M2 | 5.3 | 3.7 | 8.3 |
| Mid presence (snare, clap) | M3 | 5.0 | 6.0 | 6.0 |
| Mid presence (kick click) | M4 | 5.3 | 6.7 | 7.0 |
| High control (hats, cymbals) | M5 | 4.5 | 5.0 | 6.0 |

## Why the two kick rows were anchored (D-C15)
- **The check:** the three answers were checked against 2,000+ isolated real hits from 43 E-GMD
  kits, ranging from acoustic to 808/909, metal and lo-fi.

| | Answers (range over genres) | Real kits: median of kit medians (10–90%) | Verdict |
|---|---|---|---|
| Old M1 (sub / total) | −2.0 … −7.3 | −0.6 (range −0.1 … −4.4) | impossible for real kicks |
| M4 click | −5.5 … −14.5 | −31.4 (range −59 … −9) | ~20 dB off |
| M2 mud | −9.3 … −12.7 | −13.1 (−22.8 … −2.7) | fits |
| M3 snare presence | −4.3 … −6.5 | −11.0 (−17.4 … −6.7) | ~5 dB brighter than raw kits; plausible for finished mixes |
| M5 cymbal air | −3.0 … −5.5 | −3.2 (−4.6 … −2.4) | fits |

- **Why the kick answers were off:** a kick's *energy* sits almost entirely below 90 Hz. The click
  is loud to the ear but tiny in energy, and everyone, Claude included, answered by ear.
- **What was kept:** the answers still agree on the genre *order* (Hip-hop and EDM the most sub,
  Metal the most click).
- **M1 is now sub vs rest.** The old "sub / total" ratio can't go above 0 dB, and real kicks sit
  right at −0.6, so it barely moves. The new ratio moves roughly 1 dB per 1 dB of low shelf, which
  also matches how the answers thought about the knob spans.
- **How:** each answer is converted to the new M1 unit. Then, for M1 and M4, the genre row is
  shifted so its mean lands on the real-kit median: M1 +7.9, M4 −31.4.
- **Limits:** E-GMD kits are well-produced kit samples, not finished mixes. v2 (the user's packs,
  named by genre) measures the real genre medians directly.

## Targets in the plugin's scale (what `PuppetShow.h` uses)
- **Why a second scale:** the plugin doesn't take a 150 ms window per hit. It measures each hit's
  *rise spectrum* (what the hit added in its first ~40 ms, from the classifier), and on the same
  kits that reads differently. The kick sub, for example, has not fully built up yet.
- **How:**
  - The same 43 kits (5 performances each) were run through the plugin's own detector, classifier
    and measurement (`research/puppet-show/measure_rise.cpp`).
  - Each row above was shifted by (plugin median − 150 ms median).
  - The genre differences and knob limits are unchanged.

| | M1 | M2 | M3 | M4 | M5 |
|---|---|---|---|---|---|
| Real kits, 150 ms scale | +7.9 | −13.1 | −11.0 | −31.4 | −3.2 |
| Real kits, plugin scale | −1.6 | −3.8 | −13.6 | −28.8 | −2.3 |
| Shift | −9.5 | +9.3 | −2.6 | +2.6 | +0.9 |

| Genre (plugin scale) | M1 kick sub | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---|---|---|---|---|---|
| Rock | −4.0 | 0.0 | −7.9 | −28.9 | −4.3 |
| Pop | −2.8 | −2.2 | −7.1 | −28.6 | −2.4 |
| EDM | +1.6 | −3.4 | −6.9 | −29.1 | −2.1 |
| Metal | −5.9 | −3.2 | −6.9 | −24.2 | −3.4 |
| Hip-hop | +3.1 | −0.5 | −9.1 | −33.2 | −4.6 |

- **Mud (M2) is the least trustworthy row.** It mixes kick and snare hits, so the shift depends on
  how many of each a loop has. On the MDB rock tracks it reads −10 to −13, so The Puppet Show asks
  for its full +3.7 dB body boost. `VERIFY(dsp)` at home. This is the first row to re-measure in
  v2.
