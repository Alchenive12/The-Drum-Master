<!-- File: prompts/answers/05-agent1.md -->
# Prompt 05 — Agent 1's answer (pasted by the user 2026-09-30; tables re-typed with pipes, values unchanged; max cuts were given as negative numbers, stored here as magnitudes)

TABLE 1 - TARGETS (dB)
| Genre   | M1 kick bass | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---------|--------------|--------|-------------------|---------------|---------------|
| Rock    | -5.5 | -8.8  | -6.0 | -8.5  | -5.5 |
| Pop     | -6.0 | -10.0 | -5.5 | -7.0  | -4.5 |
| EDM     | -2.5 | -11.0 | -5.0 | -7.0  | -4.0 |
| Metal   | -8.5 | -9.5  | -5.5 | -5.0  | -5.0 |
| Hip-hop | -3.0 | -10.0 | -7.0 | -10.5 | -6.0 |

TABLE 2 - TOLERANCE (dB)
| Genre   | M1  | M2  | M3  | M4  | M5  |
|---------|-----|-----|-----|-----|-----|
| Rock    | 3.0 | 3.0 | 2.5 | 3.0 | 2.5 |
| Pop     | 2.5 | 2.5 | 2.0 | 2.5 | 2.0 |
| EDM     | 3.0 | 2.5 | 2.0 | 2.5 | 2.0 |
| Metal   | 2.5 | 3.0 | 2.5 | 2.5 | 2.5 |
| Hip-hop | 3.5 | 3.0 | 3.0 | 3.5 | 3.0 |

TABLE 3 - KNOBS
| Knob                         | Measures | SPAN at +100 % (dB) | MAX boost (dB) | MAX cut (dB) |
|------------------------------|----------|---------------------|----------------|--------------|
| Bass boost (kick)            | M1 | 5.0 | 4.0 | 6.0 |
| Mud control (bodies)         | M2 | 4.0 | 4.0 | 5.0 |
| Mid presence (snare, clap)   | M3 | 4.0 | 4.0 | 4.0 |
| Mid presence (kick click)    | M4 | 4.0 | 4.0 | 5.0 |
| High control (hats, cymbals) | M5 | 3.5 | 4.0 | 4.0 |

TABLE 4 - CONFIDENCE (1 = guess, 5 = sure)
| Genre   | M1 | M2 | M3 | M4 | M5 |
|---------|----|----|----|----|----|
| Rock    | 4 | 5 | 4 | 4 | 4 |
| Pop     | 3 | 4 | 3 | 3 | 3 |
| EDM     | 4 | 4 | 4 | 4 | 4 |
| Metal   | 4 | 4 | 4 | 4 | 3 |
| Hip-hop | 3 | 3 | 3 | 3 | 3 |

NOTES
- Metal vs Rock on M1/M4: Metal kick is the outlier. Heavy scooping below 100 Hz plus a beater click at 3–5 kHz pushes M1 far below pink noise and M4 close to it. Rock keeps more 60–80 Hz body and a softer click. Confident on the direction (4), less on the exact values.
- EDM vs Hip-hop on M1: both want deep sub, but EDM kicks are usually sine-swept and short (sub concentrated 40–60 Hz), while Hip-hop is dominated by 808s whose sub sits lower and lasts longer. Low-mid confidence.
- M2 is the most reliable measurement. Every genre cuts mud 200–500 Hz to some degree; EDM/Pop most, Rock least (guitars occupy that region). High confidence except Hip-hop (sampled vs 808).
- Hip-hop is the widest-distribution genre (boom-bap, trap, pop-rap). Treat the row as "trap/modern"; offer a "Boom-bap" variant that shifts M4 up ~2 dB and M5 down ~1 dB.
- Tolerances are tight on Pop and EDM deliberately (most standardized). Rock/Metal/Hip-hop tolerate more variety.
- Knob spans are intentionally small: a smart EQ moving more than ~5 dB on a drum bus is heard as a tone shift, not a correction.
- Knobs are asymmetric on purpose (bigger max cut than boost); M1 and M4 should cut harder than they boost so the plugin never makes a thin kick worse.
- Tables cannot express 808 vs sampled kick, layered snare, programmed vs live hats. A future "profile" selector (808 kick / acoustic kick / layered snare / live kit) would beat any genre target; ship genre targets first, add profiles later.
