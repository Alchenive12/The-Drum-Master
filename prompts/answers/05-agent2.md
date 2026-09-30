<!-- File: prompts/answers/05-agent2.md -->
# Prompt 05 — Agent 2's answer (pasted by the user 2026-09-30; values unchanged)

TABLE 1 - TARGETS (dB)
| Genre   | M1 kick bass | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---------|--------------|--------|-------------------|---------------|---------------|
| Rock    | -6.0 | -10.0 | -5.0 | -8.0  | -5.0 |
| Pop     | -5.0 | -12.5 | -4.0 | -6.5  | -2.5 |
| EDM     | -2.5 | -14.0 | -3.0 | -6.0  | -2.0 |
| Metal   | -5.5 | -15.0 | -4.5 | -3.5  | -4.0 |
| Hip-hop | -1.5 | -8.5  | -6.5 | -11.0 | -6.5 |

TABLE 2 - TOLERANCE (dB)
| Genre   | M1  | M2  | M3  | M4  | M5  |
|---------|-----|-----|-----|-----|-----|
| Rock    | 2.0 | 2.5 | 2.0 | 2.0 | 2.0 |
| Pop     | 1.5 | 1.5 | 1.5 | 1.5 | 1.5 |
| EDM     | 1.0 | 1.0 | 1.0 | 1.5 | 1.0 |
| Metal   | 1.5 | 1.5 | 1.5 | 1.0 | 2.0 |
| Hip-hop | 3.0 | 2.5 | 2.5 | 3.0 | 2.5 |

TABLE 3 - KNOBS
| Knob                         | Measures | SPAN at +100 % (dB) | MAX boost (dB) | MAX cut (dB) |
|------------------------------|----------|---------------------|----------------|--------------|
| Bass boost (kick)            | M1 | 4.0 | 6.0  | 8.0  |
| Mud control (bodies)         | M2 | 6.0 | 4.0  | 12.0 |
| Mid presence (snare, clap)   | M3 | 5.0 | 8.0  | 8.0  |
| Mid presence (kick click)    | M4 | 6.0 | 10.0 | 10.0 |
| High control (hats, cymbals) | M5 | 4.0 | 6.0  | 8.0  |

TABLE 4 - CONFIDENCE
| Genre   | M1 | M2 | M3 | M4 | M5 |
|---------|----|----|----|----|----|
| Rock    | 5 | 5 | 5 | 4 | 4 |
| Pop     | 5 | 5 | 5 | 5 | 5 |
| EDM     | 5 | 5 | 5 | 4 | 5 |
| Metal   | 5 | 5 | 4 | 5 | 4 |
| Hip-hop | 4 | 4 | 5 | 4 | 5 |

NOTES
- Hip-hop's M1 variance: the -1.5 dB target assumes a modern 808-driven sub-kick. A vintage sampled "boom-bap" kick reads lower (around -4.0 dB); the 3.0 dB tolerance accounts for this split.
- Metal's M4 dominance: Metal requires an aggressive "typewriter" kick (-3.5 dB) to cut through dense walls of distorted guitars.
- Pop/EDM M2 mud scooping: these genres need clean low-mids for synths and vocals; mud is scooped hard (-12.5 to -14.0 dB) compared to Rock or Hip-hop, which rely on 200-500 Hz for warmth and grit.
- EDM M3: the -3.0 dB snare presence is largely due to layered white-noise claps firing with the snare, concentrating energy in 1.5-5 kHz.
- Rock/Metal M5 restraint: cymbals in heavy guitar music sit lower in the extreme highs; pushing cymbal air too hard creates harsh masking with guitar amp fizz.
- Relative ratio mechanics: boosting one band lowers the measurement of the others (boosting a kick's M1 lowers its M4 even if the highs weren't touched).
