<!-- File: prompts/01-per-drum-mix-settings.md -->
# Prompt 01: per-drum mix settings

**How to use:** copy everything inside the box below (GitHub's copy button is at its top right)
and paste it into another model or agent **as is**. Don't add hints, so every model gets the same
question. Paste each answer back to Claude. It will be saved to `prompts/answers/<model>.md` and
compared against `prompts/answers/claude.md`, which Claude wrote before reading any other answer.

Written only from `docs/01-concept.md` (phase 1, approved).

```text
You are an experienced drum mixing engineer and audio plugin designer.

I am designing an audio plugin called "The Drum Master". Please help me decide which mix
controls each drum type should get.

HOW THE PLUGIN WORKS
- It is an insert effect on a drum bus or on a drum loop. The input is an already-mixed stereo drum
  signal: kick, snare, hats etc. are all on one track. There are no separate multitracks, no MIDI
  and no samples.
- It listens slightly ahead (lookahead) and recognises every hit as one of these types:
  kick, snare, closed hi-hat, open hi-hat, crash, ride, rimshot, clap, misc.
  Several types can be detected at the same moment (for example kick + closed hat).
- When a hit is detected, that drum type's settings are applied only to the hit's own frequency
  range and only for the hit's duration (from its start through its tail, or until the next hit of
  the same type). Other drums playing at the same moment, in other frequency ranges, are left alone.
- "Misc" is for uncertain hits and is neutral by default, so a wrong guess never sounds bad.
- The mixing workflow is inspired by Addictive Drums 2 (per-drum channels with character
  controls), but the plugin works on audio that already exists. It shapes hits; it never adds or
  replaces sounds.

WHAT IS ALREADY DECIDED
- Kick deep-edit: low-end boost, "top" (beater click/attack), "bottom" (body/boom), compression,
  saturation, clipping, EQ.
- Snare deep-edit: saturation, EQ, compression, plus four blendable genre macro knobs:
  Dubstepify, Popify, Rockify, Rimify. Each macro moves a hidden set of parameters.
- Quick-mix groups on the main screen: Kick (kick) / Snare (snare, rimshot, clap) /
  Hats (closed hat, open hat, ride, crash). Each group gets volume + 1–2 character knobs.
  A Link switch keeps the group volumes' dB offsets moving together.
- Global: dry/wet, output gain, detection sensitivity, punch, clip, bypass. An advanced global page
  has bus distortion, saturation, filter, etc.

WHAT I NEED FROM YOU
1. For EACH of these drum types, suggest its deep-edit controls:
   closed hi-hat, open hi-hat, crash, ride, rimshot, clap, misc.
   Aim for 4–8 controls per drum. Each control must make sense when it only acts on that drum's
   frequency range during that drum's hits (so no controls that need a separate multitrack or
   re-triggering a sample).
2. For kick and snare: say what you would ADD to or REMOVE from the decided list, and why
   (maximum 3 changes each).
3. For the snare macros (Dubstepify, Popify, Rockify, Rimify): describe in words what each one
   should do to the snare sound, and which underlying settings it should move.
4. For each quick-mix group (Kick, Snare, Hats): which 1–2 character knobs besides volume?

ANSWER FORMAT (please keep exactly this, so answers from different sources can be compared)
- One section per drum type, in this order: kick, snare, closed hi-hat, open hi-hat, crash, ride,
  rimshot, clap, misc. Then one section for the snare macros and one for the quick-mix groups.
- Inside each drum section, a table with these columns:
  Control name | What it does (plain words) | Suggested range + unit | Default | Why this drum needs it
- Defaults must be neutral (the drum sounds unchanged at default), except where you explain why not.
- Mark any control you consider optional with "(optional)".
- No code. Keep the explanations short.
- End with max 5 bullet points: risks or pitfalls you see in this per-drum design.
```
