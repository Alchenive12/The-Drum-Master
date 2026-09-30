<!-- File: prompts/answers/agent1.md -->
# Answer to prompt 01 — Agent 1

> Pasted by the user on 2026-09-30. The content is unchanged; only the plain-text tables were
> reformatted into Markdown tables.

## Kick
Existing decided controls not repeated. Table shows only changes.

| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| ADD: Sub Tail | Adjusts decay/length of the kick's sub band | -100% to +100% | 0% | Tightens boomy kicks or extends short kicks without gating the whole bus |
| ADD: Mid Knock | Adds/cuts 1–3 kHz beater/box punch | -6 to +6 dB | 0 dB | Helps kick cut through small speakers and dense loops |
| REMOVE: Clipping | Drop per-drum clipper; use saturation + global clip instead | N/A | N/A | Avoids stacked clippers and option bloat |

## Snare
Existing decided controls not repeated. Table shows only changes.

| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| ADD: Transient/Attack | Sharpens or softens the initial snare crack | -100% to +100% | 0% | Snare attack is the main "hit" cue in a mixed loop |
| ADD: Tail/Decay | Controls ring/tail length of the snare band | -100% to +100% | 0% | Lets you tighten room/ring without gating other drums |
| REMOVE: Generic EQ | Fold tone shaping into Body/Crack and the four macros | N/A | N/A | Keeps snare page focused; macros already move hidden EQ |

## Closed hi-hat
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Tone Tilt | Darkens or brightens the hat band | -6 to +6 dB tilt | 0 dB | Closed hats vary from dark to bright in mixed loops |
| Attack/Snap | Sharpens or softens initial stick click | -100% to +100% | 0% | Controls hat definition without touching snare |
| Body/Pedal | Adds/cuts low-mid "chick" around 300–1 kHz | -6 to +6 dB | 0 dB | Fixes thin or clunky closed hats |
| Tightness/Decay | Shortens or lengthens hat tail | -100% to +100% | 0% | Prevents hat overlap with snare or next hat |
| Sizzle/Air | Boosts/cuts 8–14 kHz noise | -6 to +6 dB | 0 dB | Adds modern air or tames harshness |
| Width | Mono/stereo spread of hat band | 0–200% | 100% | Hats are often too wide or too mono in loops |
| Low Cut (optional) | Removes rumble below hat band | 20–500 Hz HPF | 20 Hz off | Cleans kick bleed in the hat range |

## Open hi-hat
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Tone Tilt | Darkens or brightens the open hat | -6 to +6 dB tilt | 0 dB | Open hats often need darkening or extra shimmer |
| Attack/Stick | Sharpens or softens initial stick | -100% to +100% | 0% | Keeps stick definition when the tail is long |
| Decay/Tail | Shortens or lengthens open hat tail | -100% to +100% | 0% | Controls masking over snare and vocal space |
| Wash/Body | Adds/cuts 400 Hz–2 kHz noise body | -6 to +6 dB | 0 dB | Fixes thin or muddy open hats |
| Sizzle/Air | Boosts/cuts 8–14 kHz noise | -6 to +6 dB | 0 dB | Adds air or reduces brittle top |
| Width | Mono/stereo spread of open hat band | 0–200% | 100% | Open hats can dominate stereo image |
| Closed-Hat Choke (optional) | Reduces open hat when closed hat is detected | 0–100% | 0% | Musical choke without needing multitrack |
| Low Cut (optional) | Removes rumble below hat band | 20–500 Hz HPF | 20 Hz off | Cleans low bleed |

## Crash
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Tone Tilt | Darkens or brightens crash | -6 to +6 dB tilt | 0 dB | Crashes range from dark to bright in loops |
| Attack/Transient | Sharpens or softens crash onset | -100% to +100% | 0% | Controls "splash" vs smooth entry |
| Decay/Tail | Shortens or lengthens crash tail | -100% to +100% | 0% | Keeps crash from washing over the whole bar |
| Wash/Body | Adds/cuts 300 Hz–2 kHz wash | -6 to +6 dB | 0 dB | Fixes thin or muddy crash body |
| Sizzle/Air | Boosts/cuts 8–14 kHz air | -6 to +6 dB | 0 dB | Adds shimmer or tames harsh cymbals |
| Width | Mono/stereo spread of crash band | 0–200% | 100% | Crashes often need wider or narrower image |
| Low Cut | Removes rumble below crash band | 20–500 Hz HPF | 20 Hz off | Cleans kick/floor rumble under crash |
| Duck on Kick/Snare (optional) | Reduces crash when kick/snare hit | 0–100% | 0% | Prevents crash masking important hits |

## Ride
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Ping/Stick | Adds/cuts 2–6 kHz stick ping | -6 to +6 dB | 0 dB | Ride can be too clicky or too washy |
| Tone Tilt | Darkens or brightens ride | -6 to +6 dB tilt | 0 dB | Ride tone varies widely in mixed loops |
| Wash/Body | Adds/cuts 300 Hz–2 kHz wash | -6 to +6 dB | 0 dB | Controls ride body vs clutter |
| Bell | Adds/cuts 1–5 kHz bell emphasis | -6 to +6 dB | 0 dB | Helps ride bell cut or blend |
| Decay/Tail | Shortens or lengthens ride tail | -100% to +100% | 0% | Controls sustain under other drums |
| Sizzle/Air | Boosts/cuts 8–14 kHz air | -6 to +6 dB | 0 dB | Adds shimmer or reduces harshness |
| Width | Mono/stereo spread of ride band | 0–200% | 100% | Ride can sit too wide or too mono |
| Low Cut (optional) | Removes rumble below ride band | 20–500 Hz HPF | 20 Hz off | Cleans low bleed |

## Rimshot
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Crack/Attack | Adds/cuts 2–6 kHz rim crack | -6 to +6 dB | 0 dB | Rimshot needs sharp midrange crack |
| Body | Adds/cuts 200–600 Hz body | -6 to +6 dB | 0 dB | Controls thin vs woody rim |
| Tone Tilt | Darkens or brightens rimshot | -6 to +6 dB tilt | 0 dB | Matches rim to snare or genre |
| Decay/Tail | Shortens or lengthens rim tail | -100% to +100% | 0% | Keeps rim tight and separated |
| Saturation | Adds harmonic grit to rim hit | 0–100% | 0% | Helps rim cut without pure EQ |
| Width (optional) | Mono/stereo spread of rim band | 0–200% | 100% | Rimshots often sound better narrow |
| Low Cut (optional) | Removes rumble below rim band | 20–300 Hz HPF | 20 Hz off | Cleans low bleed |

## Clap
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Attack/Transient | Sharpens or softens clap onset | -100% to +100% | 0% | Controls clap "snap" vs soft layer |
| Spread/Width | Mono/stereo spread of clap band | 0–200% | 100% | Claps often need width without touching snare |
| Body | Adds/cuts 200–600 Hz body | -6 to +6 dB | 0 dB | Fixes thin or boxy claps |
| Tone Tilt | Darkens or brightens clap | -6 to +6 dB tilt | 0 dB | Matches clap to snare/genre |
| Decay/Tail | Shortens or lengthens clap tail | -100% to +100% | 0% | Controls clap sustain and room feel |
| Saturation | Adds harmonic density to clap | 0–100% | 0% | Helps clap cut in dense mixes |
| Low Cut (optional) | Removes rumble below clap band | 20–500 Hz HPF | 20 Hz off | Cleans low bleed |

## Misc
| Control name | What it does | Suggested range + unit | Default | Why this drum needs it |
|---|---|---|---|---|
| Tone Tilt | Darkens or brightens uncertain hits | -6 to +6 dB tilt | 0 dB | Gentle correction if misc is slightly off |
| Attack/Transient | Sharpens or softens uncertain hits | -100% to +100% | 0% | Keeps misc from sounding wrong |
| Decay/Tail | Shortens or lengthens uncertain tails | -100% to +100% | 0% | Controls bleed from unknown hits |
| Width | Mono/stereo spread of misc band | 0–200% | 100% | Keeps uncertain hits from widening the bus |
| Saturation | Adds subtle harmonic density | 0–100% | 0% | Optional glue for uncertain hits |
| Low Cut (optional) | Removes rumble below misc band | 20–500 Hz HPF | 20 Hz off | Cleans low bleed |
| Output Trim (optional) | Adjusts misc hit level | -12 to +12 dB | 0 dB | Fixes misdetected loud/quiet hits |

## Snare macros
| Macro | Sound goal | Underlying settings it moves | Default |
|---|---|---|---|
| Dubstepify | Tight, loud, aggressive, mid-forward, clipped | Hard saturation/clip, fast compression, short decay, boost 1–4 kHz, cut 200–400 Hz, slightly narrower | 0% |
| Popify | Polished, bright, controlled, tight but not stiff | Moderate compression, boost 3–8 kHz air, cut 300–500 Hz mud, slight transient boost, medium decay, subtle saturation, wider | 0% |
| Rockify | Fat, roomy, punchy, natural | Boost 100–250 Hz body, boost 3–6 kHz crack, medium saturation, slower compression, longer decay, wider | 0% |
| Rimify | Thin, cracky, short, midrange | HPF, cut low body, boost 1–5 kHz, short decay, fast transient, narrower | 0% |

## Quick-mix groups
| Group | Character knobs besides volume |
|---|---|
| Kick | Punch, Boom |
| Snare | Crack, Fat |
| Hats | Tone, Tightness |

## Risks
- Misdetection between closed/open hats, ride/crash, rim/snare, and clap/snare can apply wrong EQ or decay.
- Frequency-band isolation is never perfectly clean; overlapping kick, snare, and hats can cause phase or artifact issues.
- Per-hit gain/EQ changes can pump if detection jitters or tails overlap.
- Too many deep controls will overwhelm users; macros must hide most complexity.
- Stereo width and choke/duck interactions can weaken mono compatibility or sound unnatural if timing is late.
