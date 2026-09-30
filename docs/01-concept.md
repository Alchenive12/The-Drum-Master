<!-- File: docs/01-concept.md -->
# The Drum Master — Phase 1: Concept

> Status: **draft, waiting for approval**. No math and no code in this phase, only what the plugin
> does and how it should feel. The open questions at the bottom must be answered before phase 2.

## One sentence
An effect that sits on a drum bus or on the insert of a drum loop. It recognises each drum hit as it
arrives and mixes that hit with the settings you chose for that kind of drum.

## Where it lives
- It's an **insert effect** on a drum bus, a drum-loop track, or a stereo drum stem. It is not an instrument and it has no MIDI input.
- The input is a mixed drum signal in which the kick, snare, hats, etc. are all on one track.
- The output is the same loop, with each hit shaped by the settings for its drum type.

## What it hears: drum types
The plugin sorts every hit into one of these types:

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
- The exact method will be worked out together in phase 2.
- The lookahead adds a small, fixed delay. The plugin reports that delay to the DAW so the DAW can
  compensate, and the loop stays in time with the rest of the mix.

## Two ways to start
1. **Auto detect.** Press *Auto detect* and play the loop. While it plays, the plugin notes which
   drum types it hears and adds each one to the display as it finds it. You don't set anything. When
   you stop listening (press again, or stop playback — open question), the list stays.
2. **Manual.** Pick the expected drum types yourself: "kick", "snare", "closed hat", and so on. You
   pick types, never samples. Types that aren't on the list are never guessed, which should make
   detection more reliable.

Either way you end up with a list of the drum types the plugin should expect, and each one has its
own mix settings.

## What "mixing a hit" means
- Every drum type has its own settings, which you choose.
- When a hit is recognised, that type's settings are applied to the hit.
- If a hit is uncertain it goes to **Misc**, which by default leaves the sound unchanged, so a
  wrong guess never sounds bad.
- Which settings each type gets (volume, EQ, punch, compression, saturation, stereo width, …) is an
  open question below.

## Look and feel: the Puppet Master
- A **puppet master** stands above the plugin. He's taller than the window, so you only see his
  hands and the control bar he holds. His face is always cut off by the top edge, which keeps him
  mysterious.
- **Strings** hang from the control bar. There's one string for each drum type on your list.
- At the end of each string hangs that **drum type**, for example a small kick drum, a snare, a
  hi-hat or a crash cymbal.
- When the plugin hears a hit, that drum's **string jerks up a little**, as if the master pulled
  it. This gives you a live view of what the plugin thinks it hears.
- In auto-detect mode, new strings and drums **drop in** as new types are found.
- The generic JUCE editor stands in until phase 4 (UI). The puppet master is the phase-4 goal, and
  everything above works without it.

## What it is NOT (for now)
- Not a drum replacer or sampler: it never adds sounds, it only shapes the hits that are already
  there.
- Not a full stem separator: it can't pull the kick out of the loop as its own track (see open
  question 1).
- Not a MIDI extractor (this could come back as a phase-5 feature).

## Open questions (answer these before phase 2)
1. **Overlapping hits.** In a loop, a kick and a closed hat often land at the same moment, and
   they're mixed together. When the plugin applies the "kick" settings, it's shaping the whole signal
   at that moment, hat included. Is that OK? Options:
   a. Yes: settings apply to the whole signal during that hit. This is the simplest option and the
      most predictable.
   b. Allow several types at once ("kick + hat"). Each type's settings then only act on its own
      frequency range (for example, kick settings only touch the lows).
   c. Something else you have in mind.
2. **Per-drum settings.** Which knobs does each drum type get? Pick your must-haves:
   volume · EQ (low/mid/high or a tilt) · transient punch/attack · sustain/tail · compression ·
   saturation/drive · stereo width/pan · reverb send. Should every type have the same set, or
   should each type have its own (for example, kick gets "sub", cymbals get "tame")?
3. **How long a hit lasts.** How long should a hit's settings stay on: until the next hit, for a
   fixed time per type, or until the hit's own tail fades out?
4. **Ending auto detect.** Should it stop when you press the button again, when playback stops, or
   after N bars? Can you edit the list afterwards (remove a false "crash", add a missing "ride")?
5. **Clicking a puppet.** Does clicking a drum puppet open its settings? Should there be a solo or
   mute for each type?
6. **Global controls.** Do you want a dry/wet mix, output gain, a detection "sensitivity" knob,
   bypass?
7. **Formats and home setup.** Which formats (VST3 / AU / Standalone) and which OS and DAW will you
   test on at home?
