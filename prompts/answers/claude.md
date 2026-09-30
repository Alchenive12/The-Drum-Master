<!-- File: prompts/answers/claude.md -->
# Answer to prompt 01 — Claude

> Written and committed **before** reading any other model's answer, so the phase-2 comparison is
> fair. Same format as the prompt asks for.

## Kick
Changes to the decided list:
- **Add: Decay / tail length.** Shortens or lengthens the boom. This is the most common kick fix on a
  loop that it's already mixed into.
- **Add: Sub tune/focus (optional).** Picks the frequency the low-end boost centres on, so it hits
  the kick's actual fundamental.
- **Keep everything else.** "Top" and "bottom" replace the kick-in and kick-out mics of AD2.

| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Low-end boost | Adds weight under the kick | 0 – +12 dB | 0 dB | The main reason people reach for a kick plugin |
| Sub focus (optional) | Where the boost sits | 30 – 120 Hz | auto (detected fundamental) | Boosting the wrong frequency makes mud |
| Top | Beater click / attack level | -12 – +12 dB | 0 dB | Cut-through on small speakers |
| Bottom | Body / boom level | -12 – +12 dB | 0 dB | Weight versus tightness |
| Decay | Shorter/longer tail | -100 – +100 % | 0 % | Tightens boomy loop kicks |
| Compression | Evens out and thickens hits | 0 – 100 % | 0 % | Consistency across hits |
| Saturation | Adds harmonics so the kick is heard on phones | 0 – 100 % | 0 % | Translation |
| Clipping | Shaves the peak for loudness | 0 – 100 % | 0 % | Loud genres |
| EQ | Low / mid / high shelf-bell | ±12 dB each | 0 dB | General tone |

## Snare
Changes to the decided list:
- **Add: Body / Crack split.** Two level knobs, one for the low "thud" of the snare and one for the
  bright crack. This is the snare's version of the kick's top and bottom.
- **Add: Decay.** Shorten or lengthen the ring and the rattle of the snare wires.
- **Add: Room (optional).** A short ambience that follows each hit. Rockify uses it.

| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Body | Low "thud" level | -12 – +12 dB | 0 dB | Thin versus fat snare |
| Crack | Bright attack/crack level | -12 – +12 dB | 0 dB | Cut-through |
| Decay | Shorter/longer ring and wires | -100 – +100 % | 0 % | Tight pop versus open rock |
| Compression | Even and thicken | 0 – 100 % | 0 % | Consistency |
| Saturation | Grit and harmonics | 0 – 100 % | 0 % | Aggression |
| EQ | Low / mid / high | ±12 dB each | 0 dB | General tone |
| Room (optional) | Short ambience after each hit | 0 – 100 % | 0 % | Size and depth |
| Dubstepify / Popify / Rockify / Rimify | Genre macros (see below) | 0 – 100 % each | 0 % | Fast genre shaping |

## Closed hi-hat
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness of each hit | -24 – +12 dB | 0 dB | Hats are usually too loud or too quiet in a loop |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Harsh versus dull hats |
| Tick | Attack emphasis | -100 – +100 % | 0 % | Crisp versus soft groove |
| Decay | Shorter/longer hit | -100 – +100 % | 0 % | Tighter groove |
| De-harsh | Tames the 3–8 kHz bite only when it's too loud | 0 – 100 % | 0 % | Harshness is the #1 hat problem |
| Width (optional) | Stereo spread of the hat | 0 – 200 % | 100 % | Space in the mix |

## Open hi-hat
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Balance |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Sizzle control |
| Decay / choke | Shortens the sizzle | -100 – +100 % | 0 % | Open hats smear the groove when too long |
| De-harsh | Tames the bite | 0 – 100 % | 0 % | Harshness |
| Saturation (optional) | Gentle grit | 0 – 100 % | 0 % | Lo-fi and trap styles |
| Width (optional) | Stereo spread | 0 – 200 % | 100 % | Space |

## Crash
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Crashes often poke out |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Sizzle versus warmth |
| Decay | Shortens the wash | -100 – +100 % | 0 % | Stops the wash covering the next bars |
| Low cut | Removes low rumble from the crash only | 20 – 800 Hz | 20 Hz (off) | Mud from cymbals under the kick and snare |
| De-harsh | Tames the bite | 0 – 100 % | 0 % | Harshness |
| Width (optional) | Stereo spread | 0 – 200 % | 100 % | Size |

## Ride
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Balance |
| Ping | Stick / bell attack level | -12 – +12 dB | 0 dB | Articulation of the pattern |
| Wash | Shimmer / tail level | -12 – +12 dB | 0 dB | A ride turns to mush when the wash builds up |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | General tone |
| De-harsh (optional) | Tames the bite | 0 – 100 % | 0 % | Harshness |

## Rimshot
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Balance |
| Click | Sharpness of the attack | -100 – +100 % | 0 % | Rimshots can be piercing or buried |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Tone |
| Decay | Ring length | -100 – +100 % | 0 % | Tighten |
| Saturation (optional) | Grit | 0 – 100 % | 0 % | Character |

## Clap
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Balance |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Tone |
| Body | Low-mid weight | -12 – +12 dB | 0 dB | Thin claps are common |
| Decay | Tail length | -100 – +100 % | 0 % | Tight versus roomy |
| Compression | Evens and thickens | 0 – 100 % | 0 % | Consistency |
| Saturation (optional) | Grit | 0 – 100 % | 0 % | Character |

## Misc
| Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Level | Loudness | -24 – +12 dB | 0 dB | Balance |
| Tone | Dark ↔ bright tilt | -100 – +100 % | 0 % | Tone |
| Decay | Tail length | -100 – +100 % | 0 % | Tighten |

Misc is kept minimal on purpose: its hits are uncertain, so strong settings would hit the wrong
drums.

## Snare macros
| Macro | Sound | Settings it moves (in words) |
|---|---|---|
| Dubstepify | Huge, bright, slammed, aggressive | More crack, strong compression, heavy saturation, clipping, boosted upper-mids, longer decay |
| Popify | Clean, tight, polished, forward | Moderate crack, gentle compression, bright top, low-mid cut, shorter decay |
| Rockify | Fat, roomy, natural | More body, gentle saturation, room on, longer decay, less top |
| Rimify | Sharp, hard, rim-forward | More crack and click in the upper-mids, less body, shorter decay |

**Blending:** each macro contributes in proportion to its knob. When two macros move the same
setting, their contributions add up, with a limit so stacking all four can't go extreme.

## Quick-mix groups
| Group | Character knobs besides volume |
|---|---|
| Kick | **Punch** (attack emphasis) · **Weight** (low-end boost) |
| Snare | **Punch** · **Tone** (body ↔ crack) |
| Hats | **Tone** (dark ↔ bright) · **Tame** (de-harsh + shorter decay) |

## Risks / pitfalls
- **Band splitting must be transparent.** When every setting is at its default, the output must
  equal the input. This is the first test to write in phase 3.
- **Overlapping frequency ranges.** Snare, clap and rimshot share the mid range, and hats, ride and
  crash share the highs. The detector has to decide which one the energy belongs to, or the
  settings fight each other.
- **Decay controls on a mixed loop** can only shorten what's there cleanly. Lengthening a tail
  means adding sustain, which should be limited and gentle.
- **Too many controls.** 9 drums × about 6 controls plus macros and globals is a lot of parameters
  for FL Studio automation. They need clear, stable IDs and names from day one.
- **Wrong detections are audible.** Strong settings plus a missed or false detection gives a
  pumping, inconsistent sound. The sensitivity knob and a neutral Misc are the safety net.
