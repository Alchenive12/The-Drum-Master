<!-- File: docs/05-features.md -->
# The Drum Master — Features beyond the core

> Under the protocol, every approved feature goes back through a **short phase 1** (concept in
> words) before any math or code. Both features below were requested on 2026-09-30 and are
> **in scope for v1**. The user accepts that they delay v1.

---

## F1 — The Puppet Show (genre-aware smart EQ)

**Status:** approved for v1 · short phase 1 **approved 2026-09-30** · phase 2 started. **Decided (user, 2026-09-30):**
- **v1 uses generic genre targets:** the average of the agents' answers to
  `prompts/05-puppet-show-genre-targets.md` (a fixed table: 5 measurements × 5 genres, plus knob
  span and max boost/cut).
- **v2 measures the targets** from the user's own genre-named sample packs.
- Prompt 04 (full design review) is now optional.
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

### Decided (2026-09-30)
- **It always listens actively, like Guardian Angel.** It's a dynamic EQ stage that keeps adapting
  while the track plays. This replaces Claude's "listen then settle" proposal.
- Because it's always moving, it's **its own stage**. It doesn't turn your knobs.
  - Your per-drum knobs stay exactly where you put them.
  - The Puppet Show's live changes show as **moving band meters** (like Guardian Angel's), so you
    always see what it's doing.
- **Knobs:**

| Knob | What it does (plain words) | Guardian Angel counterpart |
|---|---|---|
| **Genre** | Which sound to aim for | — |
| **Depth** | Master amount for everything below | Depth |
| **Bass boost** | Adds low-end weight when the kick or low end is thinner than the genre wants | Biggify |
| **High control** | Keeps the top end on target: tames harshness when it's too bright, adds air when it's too dull | Harshness + Sibilance |
| **Mud control** | Cuts the boxy 200–500 Hz build-up when there's more than the genre wants | Mud |
| **Mid presence** | Brings the snare crack, kick click and clap forward (about 1–5 kHz) when they're buried | — (new) |

- **Keeping it from pumping** (lessons from Guardian Angel): slow, asymmetric attack and release
  smoothing, smoothing done on the band settings (never the raw filter coefficients), a maximum dB
  per band, and Depth as a global limit.

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

### Answers (F1, 2026-09-30)
- **Always listens actively**, as its own dynamic stage with live band meters.
- **Q9, drum-aware: yes.**
  - Bass boost → kick.
  - Mud control → kick, snare and tom bodies.
  - Mid presence → snare crack, clap and kick click.
  - High control → hats and cymbals.
  - Each acts only during those drums' hits.
- **Q10, bipolar: yes.** Each of the four knobs goes from −100 to +100%.
  - **0 = aim exactly at the genre's target.**
  - **+ = more than the genre** (more bass, more highs, more presence, and for Mud control a
    stronger mud cut).
  - **− = less** (tighter low end, darker top, less presence, and for Mud control keeping more of
    the body).
  - Depth sets how hard the bands pull towards that aim.
- **Q3, genres for v1:** Rock · Pop · EDM · Metal · Hip-hop.
- **Q4:** **one general Depth** for everything.

---

## F2 — Shadow Play (the "what the puppet hears" visualizer)

**Status:** approved for v1 · short phase 1 **approved 2026-09-30** · UI (phase 4), but the data feed
is built in phase 3.
**Name:** **Shadow Play** (chosen by the user, 2026-09-30).

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

### Decided (F2, 2026-09-30; the user left Q5–Q7 to Claude)
- **Q5, shapes:** as in the table above.
- **Q6, layout: the ring.** One bar is a clock face synced to FL's playhead. Hits at the same spot
  stack up as the loop repeats, so the groove "draws itself", and older hits fade like smoke.
- **Q7, where:** on the **main screen, as the stage floor below the puppet master**: the strings hang
  down into the ring, so each shape appears under the puppet that caught it. A click enlarges Shadow
  Play to fill the window, and a second click shrinks it back.
- **Q8, name:** Shadow Play.
