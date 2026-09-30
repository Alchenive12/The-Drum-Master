<!-- File: prompts/02-detection-algorithm.md -->
# Prompt 02: hit detection and per-drum processing algorithm

**How to use:** copy everything inside the box below and paste it into another model or agent
**as is**. Paste each answer back to Claude. It will be saved to
`prompts/answers/02-<model>.md` and compared against `prompts/answers/02-claude.md`, which
Claude wrote before reading any other answer.

Written from `docs/01-concept.md` and `docs/02-mix-settings.md` (both approved). On purpose, it
contains requirements only and none of Claude's design ideas.

```text
You are an expert in real-time audio DSP, music information retrieval (drum transcription) and
audio plugin development.

I am designing an audio plugin called "The Drum Master" and need the detection and processing
algorithm. Please design it.

WHAT THE PLUGIN DOES
- It is an insert effect on a drum bus or drum loop. Input: an already-mixed stereo drum signal
  (kick, snare, hats etc. all on one track). No multitracks, no MIDI, no samples.
- It must recognise every hit, in real time, as one or more of these types:
  kick, snare, closed hi-hat, open hi-hat, crash, ride, rimshot, clap, misc.
- It uses a small lookahead: the audio output is delayed so that each hit is identified BEFORE it
  reaches the output, and that drum's settings are already in place on the first sample of the hit.
  The delay is reported to the DAW for compensation.
- Detection should use (at least) the hit's frequency range, transient shape and volume.
- Several types can hit at the same moment (e.g. kick + closed hat). All of them must be detected.
- When a hit is detected, that drum type's mix settings (level, EQ, attack, decay, compression,
  saturation, width etc.) are applied ONLY to that drum's own frequency range, and ONLY for the
  hit's duration: from its start through its tail, until the tail has faded or a new hit of the
  same type is detected. Other drums sounding at the same time in other frequency ranges must be
  left alone.
- "Misc" catches uncertain hits and is neutral by default, so a wrong guess never sounds bad.
- With every setting at its default, the plugin must be sonically transparent.

TWO MODES
1. Auto detect: the user presses a button and plays the loop. While playback runs, the plugin
   discovers which drum types are present in this loop and adds them to the UI. It stops when DAW
   playback stops. The user can then add or remove types by hand.
2. Manual: the user picks which drum types to expect. Types not on the list must never be
   detected.

CONSTRAINTS
- Real time inside a DAW (target: FL Studio on Windows; i7-11700K). CPU-only, no GPU. It should use
  a small share of one core per instance at 44.1–96 kHz and any buffer size (32–2048 samples).
- The lookahead should be as small as possible while still classifying reliably. Tell me what
  you think it must be, and why.
- The DSP must be testable offline, without a DAW (unit tests on generated or recorded audio).
- Prefer methods that can be implemented without huge dependencies. If you propose machine
  learning, say what data it needs, how big the model is, and what the non-ML fallback is.

WHAT I NEED FROM YOU (please use exactly these numbered sections)
1. Overview: the whole pipeline in 5–10 steps, from input sample to output sample.
2. Lookahead: how long, and what the plugin does with it.
3. Onset detection: how a new hit is found, including simultaneous hits in different frequency
   ranges. Give the formulas.
4. Classification: which features you compute per hit (with formulas), and how they map to the 9
   types. Explain especially: closed vs open hat, crash vs ride, snare vs rimshot vs clap. Say how
   confidence is computed and when a hit falls to "misc".
5. Hit duration: how the end of a hit's tail is detected.
6. Auto detect: how the plugin decides a type is "present" in the loop, and whether/how it adapts
   to this specific loop. How manual mode restricts detection.
7. Per-drum processing: how a drum's settings are applied only to its own frequency range while
   staying transparent at default. What happens when two detected types share a frequency range
   (e.g. snare + clap in the mids, hats + crash in the highs). How settings fade in and out without
   clicks, pumping or phase problems.
8. Internal constants: a table of every threshold/time constant you use.
   Columns: Name | Meaning | Value + unit | How sensitive the result is to it (low/medium/high).
9. Testing: how to test detection accuracy and transparency offline, with which metrics and what
   pass thresholds.
10. Risks: max 7 bullet points on where this design will fail, and what the fallback is.

RULES
- Formulas are welcome; no code.
- Say clearly when something is your guess vs. established practice.
- Keep it as short as possible while complete.
```
