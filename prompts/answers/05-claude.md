<!-- File: prompts/answers/05-claude.md -->
# Prompt 05 — Claude's answer (written blind, before any other answer)

These are the same starting points as Claude's prompt-04 answer, which was also written blind, now
in the prompt-05 format. They come from typical genre mix moves, not measurements.

TABLE 1 - TARGETS (dB)
| Genre   | M1 kick bass | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---------|--------------|--------|-------------------|---------------|---------------|
| Rock    | -6.0         | -9.0   | -5.0              | -14.0         | -5.0          |
| Pop     | -4.0         | -12.0  | -4.0              | -16.0         | -3.0          |
| EDM     | -2.5         | -13.0  | -5.0              | -18.0         | -3.0          |
| Metal   | -8.0         | -13.0  | -3.0              | -8.0          | -4.0          |
| Hip-hop | -1.5         | -11.0  | -6.0              | -22.0         | -4.0          |

TABLE 2 - TOLERANCE (dB)
| Genre   | M1  | M2  | M3  | M4  | M5  |
|---------|-----|-----|-----|-----|-----|
| Rock    | 1.5 | 1.5 | 1.5 | 2.0 | 1.5 |
| Pop     | 1.5 | 1.5 | 1.5 | 2.0 | 1.5 |
| EDM     | 1.5 | 1.5 | 1.5 | 2.0 | 1.5 |
| Metal   | 1.5 | 1.5 | 1.5 | 2.0 | 1.5 |
| Hip-hop | 1.5 | 1.5 | 1.5 | 2.0 | 1.5 |

TABLE 3 - KNOBS
| Knob                         | Measures | SPAN at +100 % (dB) | MAX boost (dB) | MAX cut (dB) |
|------------------------------|----------|---------------------|----------------|--------------|
| Bass boost (kick)            | M1       | 6.0                 | 6.0            | 6.0          |
| Mud control (bodies)         | M2       | 6.0                 | 3.0            | 8.0          |
| Mid presence (snare, clap)   | M3       | 6.0                 | 6.0            | 6.0          |
| Mid presence (kick click)    | M4       | 6.0                 | 6.0            | 6.0          |
| High control (hats, cymbals) | M5       | 6.0                 | 5.0            | 6.0          |

TABLE 4 - CONFIDENCE (1 = guess, 5 = sure)
| Genre   | M1 | M2 | M3 | M4 | M5 |
|---------|----|----|----|----|----|
| Rock    | 3  | 2  | 3  | 2  | 3  |
| Pop     | 3  | 2  | 3  | 2  | 3  |
| EDM     | 3  | 2  | 2  | 2  | 3  |
| Metal   | 3  | 2  | 3  | 3  | 2  |
| Hip-hop | 4  | 2  | 2  | 3  | 2  |

NOTES
- Hip-hop and EDM kicks are sub-heavy (808s, layered subs), so M1 sits closest to 0. Metal kicks
  are tight and clicky: lowest M1, highest M4.
- Rock keeps more snare and tom body, so its M2 target is the least negative. Pop and EDM scoop
  the low-mids hardest.
- Metal snares are the most cutting (M3 highest). Hip-hop snares and claps are often darker or
  lo-fi.
- Pop and EDM hats are the airiest (M5).
- Hip-hop kick bass depends heavily on 808 vs sampled kick. Its tolerance could be wider than the
  table says.
- Mud control's max cut (8 dB) is larger than its max boost (3 dB): adding boxiness back is rarely
  wanted.
