<!-- File: prompts/05-puppet-show-genre-targets.md -->
# Prompt 05: The Puppet Show — generic genre targets for v1

**Why this prompt:**
- v1 of The Puppet Show uses **generic genre targets**.
- Every model answers the same fixed table, and the v1 values are the **average of all answers**
  per cell. Claude's blind answer is `prompts/answers/05-claude.md`, written before any other
  answer.
- In v2 the targets will be **measured** from the user's own genre-named sample packs, replacing
  these.
- This supersedes the target part of prompt 04. Prompt 04 is now optional (a design review).

**How to use:**
1. Copy everything inside the box and paste it into each agent as is.
2. Paste each answer back to Claude. It will be saved to `prompts/answers/05-<model>.md` and
   averaged.

```text
You are an experienced mixing engineer for drums in many genres.

I am building a drum-bus plugin with a genre-aware smart EQ called "The Puppet Show". While the
loop plays, it measures each drum's tonal balance and gently pulls it towards a TARGET for the
chosen genre. I need those targets.

HOW THE PLUGIN MEASURES (important - please answer in exactly these units)
All five measurements are ratios INSIDE one drum's own sound, in dB, so they do not depend on
how loud the loop or the drum is. "Energy" = total energy of that drum's hits in that frequency
range. Always negative (a part is smaller than the whole). Closer to 0 = more of the sound is in
that range.

  M1 KICK BASS      = kick energy 30-90 Hz        / kick energy 30 Hz-16 kHz
  M2 MUD            = energy 200-500 Hz            / total energy, for kick + snare + toms together
  M3 SNARE PRESENCE = snare (and clap) 1.5-5 kHz  / their total energy
  M4 KICK CLICK     = kick energy 2-5 kHz          / kick energy 30 Hz-16 kHz
  M5 CYMBAL AIR     = hats + cymbals 8-16 kHz      / their energy 2-16 kHz

Calibration: pink noise (equal energy per octave) would read
  M1 = -7.6 dB, M2 = -8.4 dB, M3 = -7.2 dB, M4 = -8.4 dB, M5 = -4.8 dB.
So e.g. a deep 808 kick reads well above -7.6 on M1 (maybe -2), a thin clicky metal kick below it.

WHAT I NEED
Targets = how these drums typically measure in a FINISHED, well-mixed commercial track of the
genre (the drums as heard in the final mix, not raw recordings).

Genres: Rock, Pop, EDM, Metal, Hip-hop.

Also, for each measurement, the plugin has a bipolar knob (-100 % .. +100 %, 0 = the genre
target). Tell me:
  - SPAN: how many dB the target should move at +100 % (and the opposite at -100 %)
  - MAX: the largest boost and the largest cut the smart EQ may ever apply for that knob, in dB

PLEASE ANSWER IN EXACTLY THIS FORMAT (numbers only in the tables, one decimal), then add short
notes below the tables. If unsure, still give your best number and a low confidence.

TABLE 1 - TARGETS (dB)
| Genre   | M1 kick bass | M2 mud | M3 snare presence | M4 kick click | M5 cymbal air |
|---------|--------------|--------|-------------------|---------------|---------------|
| Rock    |              |        |                   |               |               |
| Pop     |              |        |                   |               |               |
| EDM     |              |        |                   |               |               |
| Metal   |              |        |                   |               |               |
| Hip-hop |              |        |                   |               |               |

TABLE 2 - TOLERANCE (dB): how far a drum may be from the target and still sound right for the
genre (the smart EQ does nothing inside this range)
| Genre   | M1 | M2 | M3 | M4 | M5 |
|---------|----|----|----|----|----|
| Rock    |    |    |    |    |    |
| Pop     |    |    |    |    |    |
| EDM     |    |    |    |    |    |
| Metal   |    |    |    |    |    |
| Hip-hop |    |    |    |    |    |

TABLE 3 - KNOBS
| Knob                         | Measures | SPAN at +100 % (dB) | MAX boost (dB) | MAX cut (dB) |
|------------------------------|----------|---------------------|----------------|--------------|
| Bass boost (kick)            | M1       |                     |                |              |
| Mud control (bodies)         | M2       |                     |                |              |
| Mid presence (snare, clap)   | M3       |                     |                |              |
| Mid presence (kick click)    | M4       |                     |                |              |
| High control (hats, cymbals) | M5       |                     |                |              |

TABLE 4 - CONFIDENCE (1 = guess, 5 = sure)
| Genre   | M1 | M2 | M3 | M4 | M5 |
|---------|----|----|----|----|----|
| Rock    |    |    |    |    |    |
| Pop     |    |    |    |    |    |
| EDM     |    |    |    |    |    |
| Metal   |    |    |    |    |    |
| Hip-hop |    |    |    |    |    |

NOTES: 3-8 short bullets. What typically defines each genre's drum sound in these terms, and
anything the tables cannot express (e.g. "Hip-hop kick bass depends on 808 vs sampled kick").
```
