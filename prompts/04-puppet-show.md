<!-- File: prompts/04-puppet-show.md -->
# Prompt 04: The Puppet Show — genre targets and band math

**How to use:** copy everything inside the box below and paste it into another model or agent
**as is**. Paste each answer back to Claude. It will be saved to
`prompts/answers/04-<model>.md` and compared against `prompts/answers/04-claude.md`, which
Claude wrote before reading any other answer.

Written from `docs/05-features.md` (F1, approved). On purpose, it contains requirements only and
none of Claude's design ideas.

```text
You are an expert mixing engineer for drums across genres AND an audio DSP engineer.

I am building "The Drum Master", a real-time insert plugin for an already-mixed drum loop or drum
bus. It already detects every hit (kick, snare, closed hat, open hat, crash, ride, rimshot, clap,
misc/toms) with a 50 ms lookahead, knows each hit's start and tail, and can apply processing to
one drum's own frequency range only while that drum is sounding. It keeps a long-term average
spectrum of each drum type in the loop.

I need a feature called "The Puppet Show": a genre-aware smart EQ that is OFF by default. The user
picks a genre and a Depth, presses play, and it keeps listening (always active, like a dynamic EQ
such as Guardian Angel / Soothe / Gullfoss) and pulls each drum towards what that drum typically
sounds like in that genre. It is its own stage: it never moves the user's own knobs; its live
changes are shown as band meters.

CONTROLS (fixed)
- Genre: Rock, Pop, EDM, Metal, Hip-hop
- Depth: 0-100 %, one general amount for everything
- Four bipolar knobs, -100..+100 %, where 0 = "aim exactly at the genre's target",
  + = more than the genre, - = less:
  - Bass boost    -> acts on the KICK (low-end weight)
  - Mud control   -> acts on the BODIES of kick, snare and toms (boxy 200-500 Hz build-up);
                     + = cut more mud, - = keep more body
  - Mid presence  -> acts on snare crack, clap and kick click (about 1-5 kHz)
  - High control  -> acts on hats and cymbals (too harsh -> tame, too dull -> add air)
  Each acts only during those drums' hits.

PLEASE ANSWER
1. What to MEASURE for each of the four knobs: exact frequency ranges, which drums, and how to
   make the measurement independent of the loop's overall level and of how loud the drum is.
2. GENRE TARGETS: for each genre and each of the four measurements, a target value (with units)
   and a tolerance. Say where each number comes from (experience, references, published
   analyses) and how confident you are. If you would rather measure targets from reference
   material, describe exactly how (which material, licences, how many loops, which statistic).
3. The CORRECTION MATH: from (measurement, target, knob, Depth) to a gain in dB per band, per drum;
   filter types, frequencies, Q; maximum boost and cut; how Depth and the bipolar knob combine.
4. TIME BEHAVIOUR: how fast it reacts (attack/release, asymmetric?), how many hits it averages,
   what it does in the first seconds and after the loop changes, how to avoid pumping.
5. SAFETY: what must never happen (e.g. boosting noise, fighting the user's own EQ, huge boosts
   on a bad measurement), and how to guarantee it.
6. Anything that would make this sound clearly better than a generic smart EQ on a drum bus.

Answer in structured sections with tables where it helps. Give concrete numbers, not only
principles.
```
