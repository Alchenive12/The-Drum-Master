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
- **v1 engine built:** `dsp/include/plugin_dsp/PuppetShow.h` (D-C16), with the targets moved into the plugin's measurement scale.
- **v1 targets done:** `docs/05-puppet-show-targets-v1.md` (both agents answered 2026-09-30; the kick rows are anchored to real kits, D-C15).
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

---

## F3 — MIDI mode (the notes say which drum hits)

> Asked by the user on 2026-10-14. Answers: MIDI **in** (not out); General MIDI default with
> several notes allowed per drum; presets saved with the project **and** as files.

### In one sentence
Send the drum pattern's MIDI into the plugin, and every note tells the master exactly which drum hit
and when, so it never has to guess from the audio.

### How the user will use it
1. Route the drum pattern's MIDI to The Drum Master. In FL Studio that's a MIDI Out channel playing
   the same pattern, sent to the plugin's MIDI input port (`VERIFY(host)`: the exact FL routing).
2. Open the MIDI page (the round MIDI button on the stage) and switch MIDI MODE on.
3. The default map is General MIDI. To change it, press LEARN on a drum and play a note, or CLEAR a drum.
4. Save the map as a preset to reuse it in other projects.

### Behaviour
- **Default map (General MIDI):**

  | Drum | Notes |
  |---|---|
  | Kick | 35, 36 |
  | Snare | 38, 40 |
  | Rimshot | 37 |
  | Clap | 39 |
  | Closed hat | 42, 44 (pedal hat) |
  | Open hat | 46 |
  | Crash | 49, 57, 52 (china), 55 (splash) |
  | Ride | 51, 59, 53 (bell) |
  | Misc | toms 41, 43, 45, 47, 48, 50 |

  All other notes are ignored. FL names note 36 "C3"; the page uses FL's names.
- **One drum per note, any number of notes per drum.** LEARN moves a note to the new drum.
- **The note decides the hit, the audio still shapes it.**
  - A note becomes a hit at the note's time. The classifier's network is skipped.
  - The hit still gets its rise spectrum, peak level and tail from the audio, so the per-drum
    shares, The Puppet Show and the tail tracking work as before.
  - Notes within 6 ms of each other are one layered hit (kick + hat).
- **Audio onsets without a note add nothing** while the notes keep coming.
- **Fallback:** if MIDI mode is on but no note has arrived for 4 s, audio detection takes over again,
  and it hands back at the next note.
- **The strings still drop in** (auto detect counts MIDI hits like confident detections). Manual-mode
  strings still apply: a note for a drum without a string goes to Misc.
- **Presets:**
  - "General MIDI" is built in.
  - Your own presets are XML files in `Documents/The Drum Master/MIDI maps/`.
  - The project always saves the current map, the mode, and the preset name. "Custom" means it was
    edited after loading.

### Hard parts, stated honestly
- **Timing:** the hit starts at the note's time, with the usual 2 ms pre-roll before it.
  - If the drum audio reaches the plugin later than the MIDI (sampler latency, uncompensated routing),
    the gate opens a little early. That's harmless.
  - If the audio is **earlier** than the note, the first part of the transient is missed.
  - `VERIFY(host)`: check alignment in FL. If needed, a MIDI offset knob is a small follow-up.
- **Accepting MIDI changes the plugin's buses**: `NEEDS_MIDI_INPUT` in CMakeLists.txt. VST3 gets an
  event input. On macOS the AU type would become a "music effect" (aumf); macOS isn't built yet.

### F3b — MIDI write (the hits become a .mid file)

> Asked by the user on 2026-10-14: a WRITE button, no quantization, Misc not written by default,
> and a small button next to WRITE that writes Misc on a note no default kit uses.

- **Use:**
  - Press WRITE on the MIDI page, then play the loop.
  - The take ends at the first loop wrap (FL looping the pattern), when playback stops, or when you
    press STOP.
  - The button then reads DRAG .MID. Drag it into FL's playlist or piano roll.
  - Every file is also saved in `Documents/The Drum Master/Written MIDI/`.
- **Timing is exact:** each note sits where the hit was detected, with no quantization (detection is
  accurate to under 1 ms). The file starts at the bar where the take began, so it keeps its place in
  the bar, and it carries the tempo and time signature.
- **Notes:** each drum's usual General MIDI note if the map has it (kick 36, snare 38, rim 37, clap 39,
  closed hat 42, open hat 46, crash 49, ride 51), otherwise that drum's lowest mapped note. A drum
  with no notes isn't written. Channel 10, 1/16 note long.
- **Velocity:** from the hit's peak level. The loudest hit of each drum in the take is 127; 30 dB
  quieter is 1.
- **Misc:** not written by default. The MISC button writes misc hits on note 34, just below the
  General MIDI drums, which no default kit uses. If your map uses 34, the next free note down is
  used. The choice is saved with the project.
- **One pass, not a stack:** looping the pattern would put each pass on top of the last, so the take
  ends at the first jump of the song position. Hits from just before the jump, which are decided
  about 40 ms later, still land in the right place.
- **Accuracy:** the file is only as good as the detection (see `docs/03-decisions-for-review.md`,
  Classifier results). Kick, snare and closed hat are mostly right. Open/closed hat, crash/ride and
  claps will need fixing in the piano roll.
- With MIDI mode on, the notes come from your MIDI, so a take just copies them back.

---

## F4 — Loop learning: two models vote, the winners become string fingerprints (plan, 2026-10-14)

> The user's idea (2026-10-14). Not built yet. Phase 1 (concept), with the plan for the home training session.

### In one sentence
Give the plugin a loop (a file, or a few bars of playback). It studies it with no time limit, using
two models and the loop's repetitions. It then hangs one string per sound it found, each with that
sound's fingerprint, exactly as if the user had uploaded the samples. From then on, live hits are
matched against those fingerprints.

### The parts
1. **The fast model (today's, live).** It decides 40 ms after each hit, with the 225-number log-mel
   picture and a small network (225 → 96 → 9).
2. **A new slow model (loop learning only).** It sees each hit's whole length, up to about 1 s, so
   decays and rings count. Speed doesn't matter, so it can be ~100× bigger. It's trained on full-length
   samples.
3. **Repetition.** A loop repeats the same samples, so the plugin groups hits that sound alike, using
   their fingerprints.
4. **The vote.**
   - Each group is labelled from all its hits, by both models.
   - The two models get **weights per drum type**, learned on test data rather than set equal. The
     slow model should count more for open hat, crash and ride, where more time helps (measured up to
     100 ms and beyond). The fast one is enough for kick and snare.
5. **Fingerprints.**
   - Each group's cleanest hit becomes that string's fingerprint (same as F5, uploaded samples).
   - A wrong group is fixed with one click, which fixes every hit of that sound.
6. **Bonus (distillation).** The slow model can label lots of unlabelled audio to train the fast
   model, so live detection improves too, even without loop learning.

### Hard parts, stated honestly
- **Inference must run inside the plugin, in C++, with no Python.**
  - A bigger MLP reuses `TinyMlp.h` as is.
  - A convolutional network needs a small conv layer in `dsp/` (about 200 lines). The alternative,
    ONNX Runtime, is a large dependency.
  - Start with the bigger MLP on a longer picture. Move to a CNN only if it measurably wins.
- **Voting can't fix a sound that both models consistently mishear**, such as the synthetic snare
  heard as a hat. That's what the one-click fix is for.
- **Training on your own samples:**
  - Split by **pack**, not by sample, so the test kits are truly unseen.
  - Keep MDB Drums and the electronic loops as the untouched yardstick, so before/after stays
    comparable with `docs/testing/results/2026-10-14-start-of-week-vs-now.txt`.
  - Check each pack's licence (EULA) before shipping weights trained on it. Packs that forbid it are
    test-only.

### Home training session (RTX 3060 Ti): step by step
This needs a Claude session that runs **on your PC** (the Claude Desktop app, or `claude remote-control` in
a terminal in the repo folder). A cloud session can't reach your samples, GPU, FL or Visual Studio.

**Licences first.**
- **Testing on any sample is fine. Training shipped weights needs the pack's licence to allow it.**
- **Splice:** as far as we know, Splice's licence forbids using its sounds to train AI/ML models. Check the
  current terms. Until then, give your Splice folder as `--test-only`.
- **Free packs from the web:** train only on CC0 or explicitly ML-permitted packs. Others are `--test-only`.

**Steps (PowerShell in the repo folder):**
1. **Tools:**
   - `pip install numpy scipy scikit-learn soundfile pandas mido`
   - `pip install torch --index-url https://download.pytorch.org/whl/cu124` (the CUDA build, for the GPU)
   - `cmake -S research/loop-learning -B build-research`
   - `cmake --build build-research --config Release` (Visual Studio). This builds `extract_long`,
     `extract` and `eval`.
2. **Your packs → kit loops, split by pack:**

   `python research/loop-learning/home_packs.py --out D:\dm-data\home --roots D:\Samples --test-only "$env:USERPROFILE\Documents\Splice"`

   `packs.csv` lists every pack and its split.
3. **Pictures:**
   - `build-research\Release\extract_long.exe D:\dm-data\home\train D:\dm-data\ll\home_train.csv`, and the
     same for `test` → `home_test.csv`.
   - For the fast model, `build-research\Release\extract.exe D:\dm-data\home\train <S>\X_home.csv`
     (`test` → `X_hometest.csv`).
4. **The public sets, as yardsticks and as training data:**
   - `research/lookahead-test/fetch_data.sh` (MDB Drums, Dirt-Samples) plus
     `research/classifier-eval/export_sets.py` and `research/loop-learning/kit_loops.py`: the test sets.
   - Groove and E-GMD: `research/classifier-train/README.md` (download, `prep_gmd.py`), then `extract_long`
     on each.
5. **Train the slow model (GPU):**

   `python research/loop-learning/train_slow_torch.py --train gmd.csv egmd.csv synth.csv home_train.csv --test acoustic.csv electronic.csv kit.csv home_test.csv --egmd-subset <egmd>\subset.csv --out slow.npz`
6. **Measure the vote:** `python research/loop-learning/vote_eval.py slow.npz acoustic.csv electronic.csv kit.csv home_test.csv`
7. **Fast model (optional, with your packs):** `python research/classifier-train/train.py <S> dsp/include/plugin_dsp/HitClassifierWeights.h --hidden 96 --aug 1`
   (it picks up `X_home*.csv`). Then rebuild the plugin, run the DSP tests, and compare `eval` before and
   after on every set.

### First measurements (2026-10-14, away from home)
Raw numbers: `docs/testing/results/2026-10-14-loop-learning.txt`. Tools: `research/loop-learning/`.
Nothing was tuned on the set it was scored on.

- **Full length helps hats, even trained on synthetic drums only.**
  - With per-type vote weights (picked on another set), closed hat goes 0.51 → 0.68 acoustic,
    0.58 → 0.67 electronic and 0.59 → 0.70 on kit loops. Open hat goes 0.06 → 0.25 acoustic.
  - The PyTorch slow model, with dropout, reaches 0.48 for open hat.
  - Kick stays about the same. Snare dips (0.65 → 0.60 acoustic): synthetic snares sound nothing like
    real ones, so the slow model's snare weight is near 0.
- **Grouping repeats and voting changes almost nothing** (±0.02, a few precision gains).
  - Same sample vs different drums is measurable (about 8 vs 20 dB RMS on the after-the-hit picture).
    But the models' mistakes are **consistent per sound**: every repeat of a misheard sample is
    misheard the same way, so voting can't fix it.
  - Grouping is still the right tool for fingerprints and the one-click fix (a fix applies to every
    repeat). It just isn't an accuracy gain by itself.
- **So the gains have to come from better models:** real training data (your packs), full length, and
  independent evidence. Rhythm (below) is independent evidence.

### Slow model on real drums (2026-10-15, cloud CPU)
Raw numbers: `docs/testing/results/2026-10-15-slow-model-real-data.txt`. Training data: Groove + E-GMD
(10 performances x 43 kits) + synthetic, with the same held-out Groove files and E-GMD kits as the fast
model. Vote weights are always picked on other test sets.
- **The real-drum slow model alone, on acoustic drums (MDB):** snare 0.65 → 0.76, closed hat 0.51 → 0.72,
  ride 0.35 → 0.55, open hat 0.06 → 0.23, kick unchanged (0.83). On electronic drums it's weaker than the
  fast model for hats (0.44) and claps: Groove and E-GMD have no claps and few drum-machine hats.
- **Three-way vote** (fast + real slow + synthetic slow):

  | F1 | acoustic | electronic | kit loops |
  |---|---|---|---|
  | kick | 0.83 → 0.87 | 0.78 → 0.81 | 0.78 → 0.80 |
  | snare | 0.65 → 0.68 | 0.53 → 0.52 | 0.47 → 0.60 |
  | closed hat | 0.51 → 0.61 | 0.58 → 0.59 | 0.59 → 0.76 |
  | open hat | 0.06 → 0.33 | 0.23 → 0.29 | 0.33 → 0.36 |
  | ride | 0.35 → 0.55 | 0.08 → 0.16 | 0.14 → 0.22 |
  | clap | – | 0.33 → 0.33 | 0.31 → 0.40 |

  Better than the plugin today on all three sets, but still short of the v1 bar (0.85).
- **One balanced model (real + synthetic ×4) doesn't close the electronic gap.** The specialists' vote
  stays best.
- **What's missing is real electronic drums to train on: your packs** (licence permitting) are the next
  step, on the home GPU with all the data.
- **Not yet in the plugin:** these models run offline in Python. Shipping means exporting the slow model
  to C++ (a bigger `TinyMlp` with two hidden layers) and building F4's loop learning around it.

### Rhythm as evidence (user's idea, 2026-10-14)
- **The idea:** a hit's place in the bar says something about what it is. Kick on 1 (and often 3),
  snare or clap on 2 and 4, hats on the 8ths or 16ths.
- **The plugin knows the bar position.** The host's song position (ppq) is already used by Shadow
  Play and MIDI write, so this is cheap to add as an input to the slow model or the group vote.
- **Why it can help where voting didn't:** rhythm is independent of the sound, so it can break a
  consistent mishearing. Example: a "hat" group that only ever lands on 2 and 4 is probably a bright
  snare, which is exactly the synthetic-snare confusion.
- **Risks:** genres move things (trap snares on 3, half-time, fills, off-beat kicks). So it should be
  evidence with a learned weight, never a rule. The model learns per-position statistics from real
  grooves.
- **How to measure it fairly:** not on our generated loops, which were built from these very patterns
  and would flatter it. It needs real performances with a known bar grid: the Groove MIDI Dataset
  (already our training source) has tempo and bar positions. That's a download, so it fits the home
  session, or away from home with your OK.

### What can be prepared away from home
- ~~Measure grouping and voting~~ done (above).
- ~~Full-length features and PyTorch~~ done: `research/loop-learning/extract_long.cpp` and
  `train_slow_torch.py`. It runs on CUDA when present and was tested on CPU here.
- **Next:** the rhythm feature (bar position) in `extract_long.cpp` plus a measurement on Groove MIDI;
  and full-length synthetic training extended with your packs at home.

