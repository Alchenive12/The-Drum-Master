<!-- File: docs/05-features.md -->
# The Drum Master — Features beyond the core

> Under the protocol, every approved feature goes back through a **short phase 1** (concept in
> words) before any math or code. Both features below were requested on 2026-09-30 and are
> **in scope for v1**. The user accepts that they delay v1.

---

## F1 — The Puppet Show (genre-aware smart EQ)

**Status:** approved for v1 · short phase 1 **draft** (open questions at the bottom)
**Inspiration:** Guardian Angel in HarmonyEngine (a smart dynamic EQ for vocals). We port the ideas,
not the code, because `dsp/` stays free of JUCE.

### In one sentence
Pick a genre, set the Depth, press play: the puppet master listens to each drum in your loop and
pulls it towards what that drum usually sounds like in that genre. Then you fine-tune each drum
yourself.

### How the user will use it
1. Open a drum loop and put The Drum Master on it.
2. **First thing:** open The Puppet Show. It's **off by default.** Choose a **genre** and set the
   **Depth**.
3. Play the loop. The Puppet Show listens and shapes the kick, snare, hats and the rest towards the
   genre.
4. Then open each drum (click its string) and tweak it to taste, as normal.

### Why it can beat a generic smart EQ
Generic smart EQs (Gullfoss, Soothe, Guardian Angel) have to guess what they're hearing. The Drum
Master **knows** that this hit is a kick and that one is a snare. So The Puppet Show compares
**this kick** with **a typical kick in the chosen genre**, and it only acts during kicks.

### Claude's proposed shape (to confirm, Q1 and Q2)
- **It works as a layer, not by overwriting your knobs.**
  - Each drum's setting = **your knob + Depth × the Puppet Show's suggestion**.
  - The suggestion shows as a faint "ghost" mark on each knob, so you can see what it did.
  - Moving Depth never erases your own tweaks, and your tweaks never fight the Puppet Show.
- **It listens, then settles.** While the loop plays, the suggestions adapt. Once they're stable,
  they hold still instead of moving all the time, so nothing pumps. A **"listen again"** action
  starts a fresh pass.
- **It uses controls that already exist** (per-drum EQ, Body, Crack, Tone, Decay), so it adds no
  new processing stage and no new artifacts.

### Hard parts, stated honestly
- **Genre targets** decide how good this feature is. Someone has to define what a trap kick, a rock
  snare or pop hats typically look like. That takes reference data and listening, not only code, and
  it's phase-2 research for this feature (prompts to other models plus measurements, like the
  lookahead test).
- **Mixed-loop limits still apply.** Changes are only about 70–90% accurate when drums overlap, so
  the suggestions must stay moderate.

### Hook in the core (phase 3, cheap)
The detector keeps each drum type's **long-term average spectrum**. Auto-detect needs that anyway,
and it's exactly the input The Puppet Show compares against its genre targets.

### Open questions (F1)
- **Q1:** Should it be a layer with ghost marks (Claude's proposal), or should it simply set the
  knobs?
- **Q2:** Listen-then-settle with "listen again" (proposal), or keep adapting all the time?
- **Q3:** Which genres for v1? Proposal: trap, drill, hip-hop (boom bap), pop, rock, dubstep, house
  or techno, lo-fi. Add or remove any.
- **Q4:** One Depth for everything, or Depth per drum group (kick, snare, hats)?

---

## F2 — The "what the puppet hears" visualizer

**Status:** approved for v1 · short phase 1 **draft** · UI (phase 4), but the data feed is built in
phase 3.
**Name idea:** **"Shadow Play"** (shadow puppetry, which fits the theme). The user decides.

### In one sentence
While the track plays, every detected hit is drawn as a shape: a **circle for the kick**, a
**triangle for the snare**, and so on. The shapes rebuild the drum loop as a living picture.

### How it could look (Claude's proposal, Q5 and Q6)
- **The loop as a ring.** One bar or loop is a circle, like a clock face synced to FL's playhead.
  - Each hit appears at its position in the bar. As the loop repeats, hits at the same spot land on
    top of each other, so **the pattern builds up** and you literally see the groove.
  - It fits the creepy stage, a bit like a ritual circle or a music box.
- **Size** = how loud the hit is. **Brightness** = how sure the detector is. **Misc** hits look faint
  and grey.
- **Older hits fade** like chalk or smoke, so the picture stays alive, not cluttered.
- **Side benefit:** it shows exactly what the detector hears. If a hat is drawn as a snare, you see
  it immediately. That's great for tuning detection, and for trusting the plugin.

### Proposed shapes (Q5)
| Drum | Shape |
|---|---|
| Kick | circle (solid) |
| Snare | triangle |
| Closed hat | small dot / tick |
| Open hat | hollow ring |
| Crash | star-burst |
| Ride | diamond |
| Rimshot | thin slash |
| Clap | cluster of 3 small triangles (its 3 bursts) |
| Misc | small faint square |

### Cost
- **Cheap on the audio side.** The detector already produces (type, time, level, confidence) for
  every hit. The audio thread only drops those into a lock-free queue, and the editor draws them.
- **All the work is drawing** (phase 4), and it's limited to the editor's frame rate.

### Open questions (F2)
- **Q5:** Are these shapes OK? Any drum you want drawn differently?
- **Q6:** Should the loop be drawn as a ring (proposal) or as a scrolling timeline, left to right?
- **Q7:** Where does it live: behind the puppet master on the main screen, or on its own "stage"
  page?
- **Q8:** What should it be called: "Shadow Play" or something else?
