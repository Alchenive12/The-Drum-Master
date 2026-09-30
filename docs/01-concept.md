<!-- File: docs/01-concept.md -->
# The Drum Master — Phase 1: Concept

> Status: **approved (2026-09-30)**. No math and no code in this phase, only what the plugin does
> and how it should feel. The deep-edit settings for the drums other than kick and snare are
> decided in phase 2 (`prompts/01-per-drum-mix-settings.md`).

## One sentence
An effect that sits on a drum bus or on the insert of a drum loop. It recognises each drum hit as it
arrives and mixes that hit with the settings you chose for that kind of drum. The mixing workflow
is inspired by Addictive Drums 2, but it works on audio you already have.

## Where it lives
- It's an **insert effect** on a drum bus, a drum-loop track, or a stereo drum stem. It is not an
  instrument and it has no MIDI input.
- The input is a mixed drum signal in which the kick, snare, hats, etc. are all on one track.
- The output is the same loop, with each hit shaped by the settings for its drum type.
- **Home test setup:** FL Studio on Windows, i7-11700K, RTX 3060 Ti. The main format is
  **VST3**.

## What it hears: drum types
| Type | In plain words |
|---|---|
| Kick | Low thump, strong start, short-to-medium tail |
| Snare | Mid-range crack plus a noisy body |
| Closed hi-hat | Short, bright tick |
| Open hi-hat | Bright, longer sizzle |
| Crash | Loud, bright, long wash |
| Ride | Bright "ping" with a shimmering tail |
| Rimshot | Sharp, hard, mid-high click |
| Clap | A few quick bursts close together, mid-high and noisy |
| Misc | Anything it can't place confidently (percussion, toms, FX, uncertain hits) |

## How it hears: detection, in words
- It listens **slightly ahead** of what you hear (lookahead). This lets it identify a hit before the
  hit reaches the output, so the right settings are already in place on the very first sample of the
  hit.
- For each hit it looks at three things:
  - **frequency range**: where the energy sits (low, mid, high),
  - **transient shape**: how sharply it starts and how long it rings,
  - **volume**: how loud it is compared with the rest of the loop.
- **Several drums at once.** If a kick and a closed hat land together, both are detected. Each
  type's settings act **only on that drum's own frequency range**, so the kick settings shape the
  lows and the hat settings shape the highs. The two don't fight each other.
- **How long a hit lasts.** A hit's settings stay on through its **tail**. They end when the tail
  has faded, or earlier if a new hit of the same type is detected.
- The lookahead adds a small, fixed delay. The plugin reports that delay to FL Studio so FL can
  compensate, and the loop stays in time with the rest of the mix.
- The exact method will be worked out together in phase 2.

## Two ways to start
1. **Auto detect.** Press *Auto detect* and play the loop. While it plays, the plugin notes which
   drum types it hears and adds each one to the puppet display as it finds it. You don't set
   anything. It **stops when playback stops**.
2. **Manual.** Pick the expected drum types yourself: "kick", "snare", "closed hat", and so on. You
   pick types, never samples. Types that aren't on the list are never guessed, which should make
   detection more reliable.

Either way you end up with a list of the drum types the plugin should expect, and each one has its
own mix settings. You can still add or remove types by hand after auto detect.

## Mixing: three layers
### 1. Quick mix (main screen)
Fast controls for the three groups you touch most. These are for quick decisions, with no need to
go into any drum.

| Group | Covers |
|---|---|
| Kick | Kick |
| Snare | Snare, rimshot, clap |
| Hats | Closed hat, open hat, ride, crash |

Each group gets **volume** plus one or two **character knobs** (for example punch and tone).
A **Link** switch ties the group volumes together: with Link on, adding or removing dB on one group
does the same to the others, so the balance between them stays the same.

**Global controls** (also on the main screen): **dry/wet**, **output gain**, **detection
sensitivity**, **punch**, **clip**, **bypass**.

### 2. Deep edit per drum (click a string)
Clicking a string **zooms in on the drum hanging from it** and opens that drum's full settings.

**Kick**
- **Low-end boost**: adds weight and sub.
- **Top**: the beater click and attack at the front of the kick.
- **Bottom**: the body and boom behind the attack.
- **Compression**
- **Saturation**
- **Clipping**
- **EQ**

**Snare**
- **Saturation**
- **EQ**
- **Compression**
- **Character knobs**. Each one turns a whole set of hidden settings at once to push the snare
  towards a genre:
  - **Dubstepify**: huge, bright, compressed and aggressive.
  - **Popify**: clean, tight, polished and forward.
  - **Rockify**: fat, roomy and natural, with body.
  - **Rimify**: more of the rim, a sharper and harder crack.

  The knobs **blend**. For example, half Rockify plus a little Dubstepify mixes both characters.
  In phase 2 we'll decide exactly which hidden settings each knob moves.

**Other drums** (closed hat, open hat, crash, ride, rimshot, clap, misc): their settings are decided
in phase 2. Other models are asked with `prompts/01-per-drum-mix-settings.md`, and the answers are
compared fairly against Claude's own answer (`prompts/answers/claude.md`, written first).

### 3. Advanced global (click the puppet master)
Clicking the **puppet master himself** opens the advanced global page. It works on the whole bus:
**distortion**, **saturation**, **filter**, and more to be listed in phase 2.

## Look and feel: the Puppet Master
- A **puppet master** stands above the plugin. He's taller than the window, so you only see his
  hands and the control bar he holds. His face is always cut off by the top edge.
- **Strings** hang from the control bar. There's one string for each drum type on your list.
- At the end of each string hangs that **drum type**, for example a small kick drum, a snare, a
  hi-hat or a crash cymbal.
- When the plugin hears a hit, that drum's **string jerks up a little**, as if the master pulled
  it. If two drums hit together, both strings jerk.
- In auto-detect mode, new strings and drums **drop in** as new types are found.
- **Click a string** → zoom in on that drum's deep edit. **Click the puppet master** → advanced
  global page.
- The generic JUCE editor stands in until phase 4 (UI). The puppet master is the phase-4 goal, and
  everything above works without it.

## What it is NOT (for now)
- Not a drum replacer or sampler: it never adds sounds, it only shapes the hits that are already
  there.
- Not a full stem separator: it shapes each drum inside its own frequency range, but it doesn't
  output separate tracks.
- Not a MIDI extractor (this could come back as a phase-5 feature).
