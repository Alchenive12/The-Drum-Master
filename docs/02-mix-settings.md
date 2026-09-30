<!-- File: docs/02-mix-settings.md -->
# The Drum Master — Phase 2a: Per-drum mix settings

> Status: **proposal, waiting for approval** (3 decisions for the user at the bottom).
> Sources: `prompts/answers/claude.md` (written blind, first) · `agent1.md` · `agent2.md`.
> Ranges and units are settled later, in the phase-2 parameter table. This file only decides
> **which controls exist**.

## How the comparison was done
- **In:** any control that at least 2 of the 3 sources suggested, under any name.
- **Single-source ideas:** judged on merit. Each one is marked with who suggested it and why it's
  in or out.
- **Claude gets no tie-breaker.** Where Claude was outvoted, the vote stands and it's listed below.
- **Level on every drum.** Only Claude listed a per-drum Level. It stays anyway, because the Snare
  and Hats quick-mix groups hold several drums, and you need to balance them against each other.
  The group volume is an offset on top of each drum's own level.

## Where the sources agreed (strong signals)
- **Every drum needs a decay/tail control.** All 3 sources suggested one for all 9 drums. This is
  the most important finding.
- **Every drum needs an attack/transient control.** All 3 had one for kick (Top), snare, closed
  hat, rimshot and clap.
- **Tone (dark ↔ bright)** on every hat and cymbal: all 3 sources.
- **Width** on hats, cymbals and clap: 2–3 sources each.
- **Quick-mix Kick = Punch + Weight/Boom:** all 3 sources.
- **Rimify, Popify, Rockify** are described almost the same way by all 3 sources.

## Where Claude was outvoted
| Claude suggested | Outcome | Why |
|---|---|---|
| Snare Room | **Dropped** | Only 1 of 3. Rockify gets its "room" from a longer decay instead. |
| Clap Compression | **Dropped** | Only 1 of 3. Attack plus Saturation cover it. |
| Misc kept to 3 controls | **Changed to 4** | Both agents gave Misc an Attack control. It's added. |
| Kick Clipping kept | **Your decision** (see D1) | Both agents said remove it, but you asked for it. |

## Decided controls per drum

### Kick (9)
| Control | Plain words | Sources |
|---|---|---|
| Level | Loudness of the kick | Claude |
| Low-end boost | Sub **below ~60 Hz** only | Claude + A2 ("Sub") |
| Top | Beater click / attack | decided in phase 1 |
| Bottom | Body / chest punch, **~60–150 Hz** | decided in phase 1 |
| Decay | Shorter/longer boom | **all 3** |
| Compression | Evens out and thickens | decided in phase 1 |
| Saturation | Harmonics so the kick is heard on small speakers | decided in phase 1 |
| Clipping | Shaves peaks for loudness | phase 1, **see D1** |
| EQ | Fine-tuning | decided in phase 1 |

- Agent 2 said Low-end boost and Bottom overlap. It's fixed by giving each its own range: boost =
  sub, Bottom = body.
- Out: Mid Knock 1–3 kHz (A1 only; Top plus EQ already cover it). Sub focus (Claude only; it
  becomes an automatic setting, not a knob).

### Snare (11)
| Control | Plain words | Sources |
|---|---|---|
| Level | Loudness (the snare group also holds rim and clap) | Claude |
| Crack | Attack / stick transient | **all 3** |
| Body | Low "thud" | Claude + A1 (in its macros) + A2 (in its macros) |
| Decay | Ring and wires length | **all 3** |
| Compression | decided in phase 1 | — |
| Saturation | decided in phase 1 | — |
| EQ | decided in phase 1 | — |
| Dubstepify · Popify · Rockify · Rimify | Blendable genre macros | decided in phase 1, **see D2** |

- Out: removing the snare EQ (A1 only; you asked for EQ).

### Closed hi-hat (6)
Level · **Tone** (all 3) · **Tick** (attack, all 3) · **Decay** (all 3) · **De-harsh** (Claude + A2)
· **Width** (Claude + A1).
- Out: Body/Pedal and Low cut (A1 only). Tone plus De-harsh cover them.

### Open hi-hat (7)
Level · **Tone** (all 3) · **Body** (A1 + A2) · **Decay** (all 3) · **Width** (Claude + A1) ·
**Saturation** (optional, Claude + A2) · **De-harsh** (optional; Claude only, though A1's Sizzle
cut does a similar job, and it matches the closed hat, so the hat pages stay alike).
- Parked for phase 5: **Closed-hat choke** (A1). It's a good idea, but it depends on the detector
  working first.

### Crash (7)
Level · **Tone** (all 3) · **Body** (A1 + A2) · **Decay** (all 3) · **Low cut** (Claude + A1) ·
**Width** (all 3) · **De-harsh** (optional, Claude).
- Parked for phase 5: **Duck on kick/snare** (A1).

### Ride (6)
Level · **Ping** (all 3) · **Bell** (A1 + A2) · **Wash** (all 3) · **Tone** (all 3) · **Width**
(optional, A1).

### Rimshot (6)
Level · **Click** (attack, all 3) · **Body/Wood** (A1 + A2) · **Tone** (Claude + A1) · **Decay** (all 3)
· **Saturation** (all 3, optional in 2 of them).

### Clap (7)
Level · **Attack** (A1 + A2) · **Body** (all 3) · **Tone** (all 3) · **Decay** (all 3) · **Width**
(A1 + A2) · **Saturation** (optional, Claude + A1).

### Misc (4, kept minimal on purpose)
Level · **Tone** (all 3) · **Attack** (A1 + A2) · **Decay** (all 3).
Misc hits are uncertain, so strong controls there would affect the wrong drums.

## Snare macros
| Macro | Agreed sound | Moves (in words) | Agreement |
|---|---|---|---|
| Popify | Clean, tight, bright, out of the vocal's way | Cut low-mid mud, brighter top, snappy compression, shorter decay, slight crack boost | all 3 |
| Rockify | Fat, natural, roomy | More body, warm saturation, gentler/parallel compression, longer decay | all 3 |
| Rimify | Sharp, woody, rim-forward | Cut body, boost upper-mids, more crack, short decay | all 3 |
| Dubstepify | Huge, aggressive, clipped, loud | Heavy saturation and clipping, hard compression, more crack; **tail and body direction: see D2** | split |

**Blending:** each macro contributes in proportion to its knob. Where macros move the same
setting, their contributions add up, with a limit so stacking all four never goes extreme.

## Quick-mix groups
| Group | Character knobs | Sources |
|---|---|---|
| Kick | **Punch** · **Weight** | all 3 |
| Snare | **Snap** · **Fat** | A1 + A2 (Claude had Punch + Tone) |
| Hats | **Tone** · **Tame** (de-harsh + shorter decay) | Tone: all 3 · Tame: Claude + A2 |

Plus Link, as decided in phase 1.

## Risks the agents raised, and what they mean for phase 2
- **Band splitting must be transparent.** All 3 sources raised this. When every setting is at its
  default, the output must equal the input. This becomes the first DSP test.
- **Processing one drum affects everything else in its band.** Agent 2 gave the clearest examples:
  shortening an open hat's decay also ducks cymbal wash and room sound in the same band, and
  compressing the snare also pumps a hat that lands in that band at the same moment. This is the
  real cost of answer 1b. Phase 2 has to decide how gently the changes fade in and out.
- **Misdetection is audible:** closed vs open hat, ride vs crash, rim vs snare, clap vs snare.
  Detection sensitivity, a neutral Misc, and smooth transitions are the safety net.
- **Too many controls.** There are about 63 per-drum controls, plus quick mix, globals and advanced.
  The quick mix and macros must cover 90% of use. Every parameter gets a stable ID from day one.

## Decisions for you
- **D1: Kick Clipping.** Both agents say drop it: stacked clippers, and distortion of other sounds
  in the low band. Global Clip already exists. Keep it on the kick page, or drop it?
  *Claude's recommendation:* keep it, since you asked for it. Default is off, and it's a soft clip
  that only acts on the kick band.
- **D2: Dubstepify tail.** Short and tight (Agent 1), or long, with the noise tail compressed up
  (Claude + Agent 2)? Also: should it add body around 200 Hz (Agent 2) or cut it (Agent 1)?
  *Claude's recommendation:* long tail plus added body, the "huge" dubstep snare. Pick whatever
  matches the snares in the tracks you make.
- **D3: Approve the lists above**, or name any control to add or remove.
